/*
 *	Upanix - An x86 based Operating System
 *  Copyright (C) 2011 'Prajwala Prabhakar' 'srinivasa_prajwal@yahoo.co.in'
 *                                                                          
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *                                                                          
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *                                                                          
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/
 */
#include <ProcessManager.h>
#include <Display.h>
#include <DisplayManager.h>
#include <MemUtil.h>
#include <MemManager.h>
#include <FileSystem.h>
#include <Directory.h>
#include <PIC.h>
#include <PIT.h>
#include <AsmUtil.h>
#include <ProcessLoader.h>
#include <ProcessAllocator.h>
#include <DynamicLinkLoader.h>
#include <DMM.h>
#include <DSUtil.h>
#include <DLLLoader.h>
#include <ProcessEnv.h>
#include <KernelService.h>
#include <ProcFileManager.h>
#include <UserManager.h>
#include <ProcessGroupManager.h>
#include <MountManager.h>
#include <StringUtil.h>
#include <MOSMain.h>
#include <ProcessConstants.h>
#include <KernelUtil.h>
#include <Exerr.h>

int ProcessManager::_iCurrentProcessID = NO_PROCESS_ID;
static const char* DEF_KERNEL_PROC_NAME = "Kernel Proc" ;

static void ProcessManager_Yield()
{
	PIT_SetContextSwitch(true) ;
	ProcessManager_EXIT() ;
	ProcessManager_RESTORE() ;	
}

ProcessManager::ProcessManager() : 
  _uiProcessCount(0),
  _processAddressSpace((ProcessAddressSpace*)MEM_PAS_START)
{
	if(((sizeof(ProcessAddressSpace) * MAX_NO_PROCESS)) > (MEM_PAS_END - MEM_PAS_START))
    throw Exerr(XLOC, "Process Address Space InSufficient");

  for(int i = 0; i < MAX_NO_PROCESS; i++)
  {
    _processAddressSpace[i].bFree = true;
    _processAddressSpace[i].status = NEW;
  }
      
  for(int i = 0; i < PIC::MAX_INTERRUPT; i++)
    _interruptOccured[i] = false ;

  for(int i = 0; i < MAX_RESOURCE; i++)
    _resourceList[i] = false ;

  PIT_SetContextSwitch(false) ;

	TaskState* sysTSS = (TaskState*)(MEM_SYS_TSS_START - GLOBAL_DATA_SEGMENT_BASE) ;
	sysTSS->CR3_PDBR = MEM_PDBR ;
	sysTSS->DEBUG_T_BIT = 0 ;
	sysTSS->IO_MAP_BASE = 103 ;

  ProcessLoader::Instance();
	ProcessGroupManager::Instance();

	for(int i = 0; i < PIC::MAX_INTERRUPT; i++)
		DSUtil_InitializeQueue(&_interruptQueue[i], (unsigned)&(_interruptQueueBuffer[i]), MAX_PROC_ON_INT_QUEUE) ;

	DLLLoader_Initialize() ;	

	DSUtil_InitializeSLL(&_processList) ;

	KC::MDisplay().LoadMessage("Process Manager Initialization", Success);
}

ProcessAddressSpace& ProcessManager::GetAddressSpace(int pid)
{
  return _processAddressSpace[pid];
}

void ProcessManager::BuildCallGate(unsigned short usGateSelector, unsigned uiOffset, unsigned short usSelector, byte bParameterCount)
{
	GateDescriptor* gateEntry = (GateDescriptor*)(MEM_GDT_START + usGateSelector) ;
	
	__asm__ __volatile__("push %ds") ;
	MemUtil_SetDS(SYS_LINEAR_SELECTOR_DEFINED) ;

	gateEntry->lowerOffset = uiOffset & 0x0000FFFF ;
	gateEntry->selector = usSelector ;
	gateEntry->parameterCount = bParameterCount & 0x1F ;
	gateEntry->options = 0xEC ; // 11101100 --> 1 = Present. 11 = DPL. 01100 = CallGateType 
	gateEntry->upperOffset = (uiOffset & 0xFFFF0000) >> 16 ;

	__asm__ __volatile__("pop %ds") ;
}

void ProcessManager::BuildIntGate(unsigned short usGateSelector, unsigned uiOffset, unsigned short usSelector, byte bParameterCount)
{
	GateDescriptor* gateEntry = (GateDescriptor*)(MEM_GDT_START + usGateSelector) ;
	
	__asm__ __volatile__("push %ds") ;
	MemUtil_SetDS(SYS_LINEAR_SELECTOR_DEFINED) ;

	gateEntry->lowerOffset = uiOffset & 0x0000FFFF ;
	gateEntry->selector = usSelector ;
	gateEntry->parameterCount = bParameterCount & 0x1F ;
	gateEntry->options = 0xEE ; // 11101110 --> 1 = Present. 11 = DPL. 01110 = IntGateType 
	
	gateEntry->upperOffset = (uiOffset & 0xFFFF0000) >> 16 ;

	__asm__ __volatile__("pop %ds") ;
}

int ProcessManager::FindFreePAS()
{
	static Mutex m;
  MutexGuard g(m);

	if(_uiProcessCount + 1 > MAX_NO_PROCESS)
    throw Exerr(XLOC, "Max process limit reached!");

	for(int i = 0; i < MAX_NO_PROCESS; i++)
	{
		if(_processAddressSpace[i].bFree == true)
		{
			_processAddressSpace[i].uiAUTAddress = NULL ;
			_processAddressSpace[i].uiDMMFlag = false ;
			_processAddressSpace[i].bFree = false ;
			_processAddressSpace[i].status = NEW ;
			return i;
		}
	}

  throw Exerr(XLOC, "no free process address space available");
}

void ProcessManager::BuildDescriptor(Descriptor* descriptor, unsigned uiLimit, unsigned uiBase, byte bType, byte bFlag)
{
	descriptor->usLimit15_0 = (0xFFFF & uiLimit) ;
	descriptor->usBase15_0 = (0xFFFF & uiBase) ;
	descriptor->bBase23_16 = (uiBase >> 16) & 0xFF ;
	descriptor->bType = bType ;
	descriptor->bLimit19_16_Flags = (bFlag << 4) | ((uiLimit >> 16) & 0xF) ;
	descriptor->bBase31_24 = (uiBase >> 24) & 0xFF ;
}

void ProcessManager::BuildLDT(ProcessLDT* processLDT)
{
	unsigned uiSegmentBase = PROCESS_BASE ;
	unsigned uiProcessLimit = 0xFFFFF ;

	BuildDescriptor(&processLDT->NullDesc, 0, 0, 0, 0) ;
	BuildDescriptor(&processLDT->LinearDesc, uiProcessLimit, uiSegmentBase, 0xF2, 0xC) ;
	BuildDescriptor(&processLDT->CodeDesc, uiProcessLimit, uiSegmentBase, 0xFA, 0xC) ;
	BuildDescriptor(&processLDT->DataDesc, uiProcessLimit, uiSegmentBase, 0xF2, 0xC) ;
	BuildDescriptor(&processLDT->StackDesc, uiProcessLimit, uiSegmentBase, 0xF2, 0xC) ;
	
	BuildDescriptor(&processLDT->CallGateStackDesc, uiProcessLimit, GLOBAL_DATA_SEGMENT_BASE, 0x92, 0xC);
}

void ProcessManager::BuildKernelTaskLDT(int iProcessID)
{
	ProcessLDT& processLDT = GetAddressSpace(iProcessID).processLDT;

	BuildDescriptor(&processLDT.NullDesc, 0, 0, 0, 0) ;
//BuildDescriptor(&processLDT->LinearDesc, 0xFFFFF, 0x00, 0x92, 0xC) ;
	BuildDescriptor(&processLDT.LinearDesc, 0xFFFFF, GLOBAL_DATA_SEGMENT_BASE, 0x92, 0xC) ;
	BuildDescriptor(&processLDT.CodeDesc, 0xFFFFF, GLOBAL_DATA_SEGMENT_BASE, 0x9A, 0xC) ;
	BuildDescriptor(&processLDT.DataDesc, 0xFFFFF, GLOBAL_DATA_SEGMENT_BASE, 0x92, 0xC) ;
	BuildDescriptor(&processLDT.StackDesc, 0xFFFFF, GLOBAL_DATA_SEGMENT_BASE, 0x92, 0xC) ;
}

void ProcessManager::BuildIntTaskState(const unsigned uiTaskAddress, const unsigned uiTSSAddress, const int stack)
{
	TaskState* taskState = (TaskState*)(uiTSSAddress - GLOBAL_DATA_SEGMENT_BASE) ;
	memset(taskState, 0, sizeof(TaskState)) ;

	taskState->EIP = uiTaskAddress ;
	taskState->ESP = stack ;

	taskState->ES = 0x8 ;

	taskState->CS = 0x10 ;
	taskState->DS = 0x18 ;
	taskState->SS = 0x18 ;
	taskState->FS = 0x18 ;
	taskState->GS = 0x18 ;
	taskState->LDT = 0x0 ;

	taskState->CR3_PDBR = MEM_PDBR ;
	taskState->EFLAGS = 0x202 ;
	taskState->DEBUG_T_BIT = 0x00 ;
	taskState->IO_MAP_BASE = 103 ; // > TSS Limit => No I/O Permission Bit Map present
}

void ProcessManager::BuildKernelTaskState(const unsigned uiTaskAddress, const int iProcessID, const unsigned uiStackTop, unsigned uiParam1, unsigned uiParam2)
{
	TaskState& taskState = GetAddressSpace(iProcessID).taskState;
	memset(&taskState, 0, sizeof(TaskState));

	taskState.EIP = uiTaskAddress ;

	((unsigned*)(uiStackTop - 8))[0] = uiParam1 ;
	((unsigned*)(uiStackTop - 8))[1] = uiParam2 ;
	taskState.ESP = uiStackTop - 12 ;

	taskState.ES = 0x8 | 0x4 ;

	taskState.CS = 0x10 | 0x4 ;

	taskState.DS = 0x18 | 0x4 ;
	taskState.FS = 0x18 | 0x4 ;
	taskState.GS = 0x18 | 0x4 ;

	taskState.SS = 0x20 | 0x4 ;

	taskState.LDT = 0x50 ;

	taskState.CR3_PDBR = MEM_PDBR ;
	taskState.EFLAGS = 0x202 ;
	taskState.DEBUG_T_BIT = 0x00 ;
	taskState.IO_MAP_BASE = 103 ; // > TSS Limit => No I/O Permission Bit Map present
}

void ProcessManager::BuildTaskState(ProcessAddressSpace* pProcessAddressSpace, unsigned uiPDEAddress, unsigned uiEntryAdddress, unsigned uiProcessEntryStackSize)
{
	int pr = 100 ;
	TaskState* taskState = &(pProcessAddressSpace->taskState) ;
	memset(taskState, 0, sizeof(TaskState)) ;

	taskState->CR3_PDBR = uiPDEAddress ;
	taskState->EIP = uiEntryAdddress ;
	taskState->ESP = pProcessAddressSpace->uiProcessBase + (pProcessAddressSpace->uiNoOfPagesForProcess - PROCESS_CG_STACK_PAGES) * PAGE_SIZE - uiProcessEntryStackSize ;

	if(pr == 1) //GDT - Low Priority
	{
		taskState->ES = 0x50 | 0x3 ;
	
		taskState->CS = 0x40 | 0x3 ; 
	
		taskState->DS = 0x48 | 0x3 ;
		taskState->SS = 0x48 | 0x3 ;
		taskState->FS = 0x48 | 0x3 ;
		taskState->GS = 0x48 | 0x3 ;
		taskState->LDT = 0x0 ;
	}
	else if(pr == 2) //GDT - High Priority
	{
		taskState->ES = 0x8 ;
	
		taskState->CS = 0x10 ;
	
		taskState->DS = 0x18 ;
		taskState->SS = 0x18 ;
		taskState->FS = 0x18 ;
		taskState->GS = 0x18 ;
		taskState->LDT = 0x0 ;
	}
	else //LDT
	{
		taskState->ES = 0x8 | 0x7 ;
	
		taskState->CS = 0x10 | 0x7 ;
	
		taskState->DS = 0x18 | 0x7 ;
		taskState->FS = 0x18 | 0x7 ;
		taskState->GS = 0x18 | 0x7 ;

		taskState->SS = 0x20 | 0x7 ;

		taskState->SS0 = 0x28 | 0x4 ;
		taskState->ESP0 = PROCESS_BASE + pProcessAddressSpace->uiProcessBase + (pProcessAddressSpace->uiNoOfPagesForProcess) * PAGE_SIZE - GLOBAL_DATA_SEGMENT_BASE ;
		taskState->LDT = 0x50 ;
	}
	taskState->EFLAGS = 0x202 ;
	taskState->DEBUG_T_BIT = 0x00 ;
	taskState->IO_MAP_BASE = 103 ; // > TSS Limit => No I/O Permission Bit Map present
}

void ProcessManager::AddToSchedulerList(int iNewProcessID)
{
  ProcessSwitchLock switchLock;

	if(_iCurrentProcessID == NO_PROCESS_ID)
	{
		_iCurrentProcessID = iNewProcessID;
    _processAddressSpace[_iCurrentProcessID].iNextProcessID = _iCurrentProcessID;
    _processAddressSpace[_iCurrentProcessID].iPrevProcessID = _iCurrentProcessID;
	}
	else
	{
		int iPrevProcessID = _processAddressSpace[_iCurrentProcessID].iPrevProcessID;
		
		_processAddressSpace[iNewProcessID].iNextProcessID = _iCurrentProcessID;
		_processAddressSpace[iNewProcessID].iPrevProcessID = iPrevProcessID;

		_processAddressSpace[_iCurrentProcessID].iPrevProcessID = iNewProcessID;
		_processAddressSpace[iPrevProcessID].iNextProcessID = iNewProcessID;
	}

	InsertIntoProcessList(iNewProcessID);
}

void ProcessManager::DeleteFromSchedulerList(int iDeleteProcessID)
{
	if(_processAddressSpace[iDeleteProcessID].iNextProcessID == iDeleteProcessID)
	{
		_iCurrentProcessID = NO_PROCESS_ID ;
	}
	else
	{
		int iPrevProcessID = _processAddressSpace[iDeleteProcessID].iPrevProcessID ;
		int iNextProcessID = _processAddressSpace[iDeleteProcessID].iNextProcessID ;

		_processAddressSpace[iPrevProcessID].iNextProcessID = iNextProcessID ;
		_processAddressSpace[iNextProcessID].iPrevProcessID = iPrevProcessID ;
	}

  DeleteFromProcessList(iDeleteProcessID) ;

	_processAddressSpace[iDeleteProcessID].bFree = true ;
}

void ProcessManager::Destroy(int iDeleteProcessID)
{
	// Release process
	ProcessAddressSpace& pas = GetAddressSpace(iDeleteProcessID);

	Atomic::Swap((__volatile__ int&)(pas.status), static_cast<int>(TERMINATED)) ;

	//MemManager::Instance().DisplayNoOfFreePages();
	//MemManager::Instance().DisplayNoOfAllocPages(0,0);
	
	// Deallocate Resources
	DeAllocateResources(iDeleteProcessID) ;

  // Release From Process Group
	ProcessGroupManager::Instance().RemoveFromFGProcessList(iDeleteProcessID) ;
	ProcessGroupManager::Instance().RemoveProcessFromGroup(iDeleteProcessID) ;

	int iProcessGroupID = pas.iProcessGroupID ;

	if(ProcessGroupManager::Instance().GetProcessCount(iProcessGroupID) == 0)
		ProcessGroupManager::Instance().DestroyProcessGroup(iProcessGroupID) ;

	// Child processes of this process (if any) will be redirected to "kernel" process
	int iNextProcessID = pas.iNextProcessID ;
	while(true)
	{
		if(iNextProcessID == NO_PROCESS_ID || iNextProcessID == iDeleteProcessID)
			break ;

		ProcessAddressSpace* pc = &GetAddressSpace(iNextProcessID);

		if(pc->iParentProcessID == iDeleteProcessID)
		{
			if(pc->status == TERMINATED)
			{
				Release(iNextProcessID) ;
			}
			else
			{
				pc->iParentProcessID = MOSMain_KernelProcessID() ;
			}
		}

		iNextProcessID = pc->iNextProcessID ;
	}	

	int iParentProcessID = pas.iParentProcessID ;
	
	if(iParentProcessID == NO_PROCESS_ID)
		Release(iDeleteProcessID) ;
	
	//MemManager::Instance().DisplayNoOfFreePages();
	//MemManager::Instance().DisplayNoOfAllocPages(0,0);
}

void ProcessManager::Load(int iProcessID)
{
	MemUtil_CopyMemory(MemUtil_GetDS(), (unsigned)&GetAddressSpace(iProcessID).processLDT,
					SYS_LINEAR_SELECTOR_DEFINED, MEM_LDT_START, sizeof(ProcessLDT)) ;

	MemUtil_CopyMemory(MemUtil_GetDS(), (unsigned)&GetAddressSpace(iProcessID).taskState,
					SYS_LINEAR_SELECTOR_DEFINED, MEM_USER_TSS_START, sizeof(TaskState)) ;
}

void ProcessManager::Store(int iProcessID)
{
	MemUtil_CopyMemory(SYS_LINEAR_SELECTOR_DEFINED, MEM_LDT_START, MemUtil_GetDS(), 
					(unsigned)&GetAddressSpace(iProcessID).processLDT, sizeof(ProcessLDT)) ;

	MemUtil_CopyMemory(SYS_LINEAR_SELECTOR_DEFINED, MEM_USER_TSS_START,	MemUtil_GetDS(), 
					(unsigned)&GetAddressSpace(iProcessID).taskState, sizeof(TaskState)) ;
}

void ProcessManager::DoContextSwitch(int iProcessID)
{
	__volatile__ ProcessAddressSpace* pPAS = &GetAddressSpace(iProcessID) ;
	__volatile__ ProcessStateInfo* pStat = pPAS->pProcessStateInfo ;

	if(pPAS->status == TERMINATED)
	{
		// Parent Responsible for freeing address space of terminated child process
		return ;
	}

	switch(pPAS->status)
	{
	case WAIT_SLEEP:
		{
			if(PIT_GetClockCount() >= pStat->uiSleepTime)
			{
				pStat->uiSleepTime = 0 ;
				pPAS->status = RUN ;
			}
			else
			{
				if(debug_point) 
				{
					printf("\n Sleep Time: %u", pStat->uiSleepTime) ;
					printf("\n PIT Tick Count: %u", PIT_GetClockCount()) ;
					printf("\n") ;
				}
				return ;
			}
		}
		break ;

	case WAIT_INT:
		{
			if(!WakeupProcessOnInterrupt(iProcessID))
				return ;
		}
		break ;
	
	case WAIT_CHILD:
		{
			if(pStat->iWaitChildProcId < 0)
			{
				pStat->iWaitChildProcId = NO_PROCESS_ID ;
				pPAS->status = RUN ;
			}
			else
			{
				ProcessAddressSpace* p = &GetAddressSpace(pStat->iWaitChildProcId) ;

				if(p->bFree == true || p->iParentProcessID != iProcessID)
				{
					pStat->iWaitChildProcId = NO_PROCESS_ID ;
					pPAS->status = RUN ;
				}
				else if(p->status == TERMINATED && p->iParentProcessID == iProcessID)
				{
					Release(pStat->iWaitChildProcId) ;
					pStat->iWaitChildProcId = NO_PROCESS_ID ;
					pPAS->status = RUN ;
				}
				else
					return ;
			}
		}
		break ;

	case WAIT_RESOURCE:
		{
			if(pStat->uiWaitResourceId == RESOURCE_NIL)
			{
				pPAS->status = RUN ;
			}
			else
			{
				if(_resourceList[pStat->uiWaitResourceId] == false)
				{ 
					pStat->uiWaitResourceId = RESOURCE_NIL ;
					pPAS->status = RUN ;
				}
				else
					return ;
			}
		}
		break ;

	case WAIT_KERNEL_SERVICE:
		{
			ProcessAddressSpace* p = &GetAddressSpace(iProcessID) ;
			if(p->pProcessStateInfo->bKernelServiceComplete)
			{
				p->pProcessStateInfo->bKernelServiceComplete = false ;
				p->status = RUN ;
			}
			else
				return ;
		}
		break ;

	case RUN:
		break ;

	default:
		return ;
	}

	Load(iProcessID) ;

	PIT_SetContextSwitch(false) ;

	KERNEL_MODE = false ;
	/* Switch Insturction */
	__asm__ __volatile__("lcall $0x28, $0x00") ;
	KERNEL_MODE = true ;

	Store(iProcessID) ;

	if(PIT_IsContextSwitch() == false || pPAS->status == TERMINATED)
		Destroy(iProcessID) ;

	if(debug_point) TRACE_LINE ;
	//PIC::Instance().EnableInterrupt(TIMER_IRQ) ;
	PIT_SetTaskSwitch(true) ;
	if(debug_point) TRACE_LINE ;
}

void ProcessManager::StartScheduler()
{
	while(_uiProcessCount > 0)
	{
		__volatile__ ProcessAddressSpace* pPAS = &GetCurrentPAS();

		if(pPAS->status == TERMINATED && pPAS->iParentProcessID == MOSMain_KernelProcessID())
		{
			int pid = pPAS->iNextProcessID ;
			Release(_iCurrentProcessID) ;
			_iCurrentProcessID = pid ;
			continue ;
		}

		DoContextSwitch(_iCurrentProcessID) ;

		if(_iCurrentProcessID != NO_PROCESS_ID && PIT_IsTaskSwitch())
		{
			_iCurrentProcessID = _processAddressSpace[_iCurrentProcessID].iNextProcessID;
		}
	}
}

void ProcessManager::EnableTaskSwitch()
{
	PIT_SetTaskSwitch(true) ;
}

void ProcessManager::DisableTaskSwitch()
{
	PIT_SetTaskSwitch(false) ;
}

void ProcessManager::Sleep(__volatile__ unsigned uiSleepTime) // in Mili Seconds
{
	if(DoPoleWait())
	{
		KernelUtil::Wait(uiSleepTime) ;
		return ;
	}

	ProcessManager::DisableTaskSwitch() ;

	uiSleepTime = PIT_RoundSleepTime(uiSleepTime) ;
	GetCurrentPAS().pProcessStateInfo->uiSleepTime = PIT_GetClockCount() + uiSleepTime ;
	GetCurrentPAS().status = WAIT_SLEEP ;

	ProcessManager_Yield() ;
}

void ProcessManager::WaitOnInterrupt(const IRQ& irq)
{
	if(DoPoleWait())
	{
		KernelUtil::WaitOnInterrupt(irq);
		return;
	}

	ProcessManager::DisableTaskSwitch();
	
	GetCurrentPAS().pProcessStateInfo->pIRQ = &irq;
	GetCurrentPAS().status = WAIT_INT;

	ProcessManager_Yield();
}

void ProcessManager::WaitOnChild(int iChildProcessID)
{
	if(GetCurProcId() < 0)
		return ;

	if(iChildProcessID < 0 || iChildProcessID >= MAX_NO_PROCESS)
		return ;

	ProcessManager::DisableTaskSwitch() ;
	
	GetCurrentPAS().pProcessStateInfo->iWaitChildProcId = iChildProcessID ;
	GetCurrentPAS().status = WAIT_CHILD ;

	ProcessManager_Yield() ;
}

void ProcessManager::WaitOnResource(unsigned uiResourceType)
{
	if(GetCurProcId() < 0)
		return ;

	//ProcessManager_EnableTaskSwitch() ;
	ProcessManager::DisableTaskSwitch() ;
	
	GetCurrentPAS().pProcessStateInfo->uiWaitResourceId = uiResourceType ;
	GetCurrentPAS().status = WAIT_RESOURCE ;

	ProcessManager_Yield() ;
}

bool ProcessManager::IsChildAlive(int iChildProcessID)
{
  ProcessSwitchLock pLock;

	if(iChildProcessID < 0 || iChildProcessID >= MAX_NO_PROCESS)
    return false;
  ProcessAddressSpace* p = &GetAddressSpace(iChildProcessID) ;

  if(p->bFree == true || p->iParentProcessID != ProcessManager::GetCurrentProcessID())
    return false;
	return true;
}

byte ProcessManager::CreateKernelImage(const unsigned uiTaskAddress, int iParentProcessID, byte bIsFGProcess, unsigned uiParam1, unsigned uiParam2, 
		int* iProcessID, const char* szKernelProcName)
{
	int iNewProcessID = FindFreePAS();
  ProcessAddressSpace& newPAS = GetAddressSpace(iNewProcessID);

	if(iParentProcessID != NO_PROCESS_ID)
	{
    ProcessAddressSpace& parentPAS = GetAddressSpace(iParentProcessID);
		newPAS.iDriveID = parentPAS.iDriveID ;

		MemUtil_CopyMemory(MemUtil_GetDS(), (unsigned)&(parentPAS.processPWD),
		MemUtil_GetDS(), (unsigned)&(newPAS.processPWD), 
		sizeof(FileSystem_PresentWorkingDirectory)) ;
	}
	else
	{
		int iDriveID = ROOT_DRIVE_ID ;
		newPAS.iDriveID = iDriveID ;

		if(iDriveID != CURRENT_DRIVE)
		{
			DriveInfo* pDriveInfo = DeviceDrive_GetByID(iDriveID, false) ;
			if(pDriveInfo == NULL)
				return ProcessManager_FAILURE ;

			if(pDriveInfo->drive.bMounted)
			{
				MemUtil_CopyMemory(MemUtil_GetDS(), (unsigned)&(pDriveInfo->FSMountInfo.FSpwd), MemUtil_GetDS(), 
						(unsigned)&newPAS.processPWD, sizeof(FileSystem_PresentWorkingDirectory)) ;
			}
		}
	}

	unsigned uiStackAddress ;
	RETURN_X_IF_NOT(ProcessAllocator_AllocateAddressSpaceForKernel(&GetAddressSpace(iNewProcessID), 
		&uiStackAddress), ProcessAllocator_SUCCESS, ProcessManager_FAILURE) ;

	GetAddressSpace(iNewProcessID).bIsKernelProcess = true ;

	ProcessEnv_InitializeForKernelProcess() ;

//	DriveInfo* pDriveInfo = DeviceDrive_GetByID(GetAddressSpace(iNewProcessID).iDriveID) ;
//
//	MemUtil_CopyMemory(MemUtil_GetDS(), (unsigned)&(pDriveInfo->FSMountInfo.FSpwd), 
//	MemUtil_GetDS(), (unsigned)&GetAddressSpace(iNewProcessID).processPWD, sizeof(FileSystem_PresentWorkingDirectory)) ;

	unsigned uiStackTop = uiStackAddress - GLOBAL_DATA_SEGMENT_BASE + (PROCESS_KERNEL_STACK_PAGES * PAGE_SIZE) - 1 ;

	BuildKernelTaskState(uiTaskAddress, iNewProcessID, uiStackTop, uiParam1, uiParam2) ;
	BuildKernelTaskLDT(iNewProcessID) ;
	
	newPAS.iUserID = ROOT_USER_ID ;
	newPAS.iParentProcessID = iParentProcessID ;
	int iProcessGroupID ;

	if(iParentProcessID == NO_PROCESS_ID)
	{
    iProcessGroupID = ProcessGroupManager::Instance().CreateProcessGroup(bIsFGProcess);
	}
	else
	{
		iProcessGroupID = GetAddressSpace(iParentProcessID).iProcessGroupID ;
	}

	newPAS.iProcessGroupID = iProcessGroupID ;
	ProcessGroupManager::Instance().AddProcessToGroup(iNewProcessID) ;

	*iProcessID = iNewProcessID ;

	if(bIsFGProcess)
		ProcessGroupManager::Instance().PutOnFGProcessList(iNewProcessID) ;

	if(szKernelProcName == NULL)
		szKernelProcName = DEF_KERNEL_PROC_NAME ;

	newPAS.pname = (char*)DMM_AllocateForKernel(String_Length(szKernelProcName) + 1);
	String_Copy(newPAS.pname, szKernelProcName) ;

	ProcessStateInfo* pStateInfo = (ProcessStateInfo*)DMM_AllocateForKernel(sizeof(ProcessStateInfo)) ;
	
	pStateInfo->uiSleepTime = 0 ;
	pStateInfo->pIRQ = &PIC::Instance().NO_IRQ ;
	pStateInfo->iWaitChildProcId = NO_PROCESS_ID ;
	pStateInfo->uiWaitResourceId = RESOURCE_NIL ;

	newPAS.pProcessStateInfo = pStateInfo ;
	newPAS.status = RUN ;

	AddToSchedulerList(iNewProcessID) ;

	return ProcessManager_SUCCESS ;
}

byte ProcessManager::Create(const char* szProcessName, int iParentProcessID, byte bIsFGProcess, int* iProcessID, int iUserID, int iNumberOfParameters, char** szArgumentList)
{
	//MemManager::Instance().DisplayNoOfFreePages();
	int iNewProcessID = FindFreePAS();
  ProcessAddressSpace& newPAS = GetAddressSpace(iNewProcessID);

	unsigned uiEntryAdddress;
	unsigned uiProcessEntryStackSize;
	unsigned uiPDEAddress;
	
	if(ProcessLoader_Load(szProcessName, &newPAS, &uiPDEAddress,
		 &uiEntryAdddress, &uiProcessEntryStackSize, iNumberOfParameters, szArgumentList) != ProcessLoader_SUCCESS)
	{
		return ProcessManager_FAILURE ;
	}

	if(iParentProcessID != NO_PROCESS_ID)
	{
    ProcessAddressSpace& parentPAS = GetAddressSpace(iParentProcessID);
		newPAS.iDriveID = parentPAS.iDriveID ;

		MemUtil_CopyMemory(MemUtil_GetDS(), (unsigned)&(parentPAS.processPWD),
		MemUtil_GetDS(), (unsigned)&(newPAS.processPWD), 
		sizeof(FileSystem_PresentWorkingDirectory)) ;
	}
	else
	{
		int iDriveID = ROOT_DRIVE_ID ;
		newPAS.iDriveID = iDriveID ;

		if(iDriveID != CURRENT_DRIVE)
		{
			DriveInfo* pDriveInfo = DeviceDrive_GetByID(iDriveID, false) ;
			if(pDriveInfo == NULL)
				return ProcessManager_FAILURE ;

			if(pDriveInfo->drive.bMounted)
			{
				MemUtil_CopyMemory(MemUtil_GetDS(), (unsigned)&(pDriveInfo->FSMountInfo.FSpwd), MemUtil_GetDS(), 
						(unsigned)&newPAS.processPWD, sizeof(FileSystem_PresentWorkingDirectory)) ;
			}
		}
	}

	RETURN_X_IF_NOT(ProcessEnv_Initialize(uiPDEAddress, iParentProcessID), ProcessEnv_SUCCESS, ProcessManager_FAILURE) ;

	RETURN_X_IF_NOT(ProcFileManager_Initialize(uiPDEAddress, iParentProcessID), ProcessEnv_SUCCESS, ProcessManager_FAILURE) ;

	newPAS.bIsKernelProcess = false ;
	newPAS.uiNoOfPagesForDLLPTE = 0 ;

	BuildLDT(&GetAddressSpace(iNewProcessID).processLDT) ;
	BuildTaskState(&GetAddressSpace(iNewProcessID), uiPDEAddress, uiEntryAdddress, uiProcessEntryStackSize) ;

	if(iUserID == DERIVE_FROM_PARENT)
	{
		newPAS.iUserID = GetCurrentPAS().iUserID ;
	}
	else
	{ 
		newPAS.iUserID = iUserID;
	}

	*iProcessID = iNewProcessID ;

	newPAS.iParentProcessID = iParentProcessID ;
	int iProcessGroupID ;

	if(iParentProcessID == NO_PROCESS_ID)
	{
    iProcessGroupID = ProcessGroupManager::Instance().CreateProcessGroup(bIsFGProcess);
	}
	else
	{
    ProcessAddressSpace& parentPAS = GetAddressSpace(iParentProcessID);
		newPAS.iDriveID = parentPAS.iDriveID ;
		iProcessGroupID = parentPAS.iProcessGroupID ;
	}

	newPAS.iProcessGroupID = iProcessGroupID ;
	ProcessGroupManager::Instance().AddProcessToGroup(iNewProcessID) ;

	*iProcessID = iNewProcessID ;

	if(bIsFGProcess)
		ProcessGroupManager::Instance().PutOnFGProcessList(iNewProcessID) ;

	DisplayManager::SetupPageTableForDisplayBuffer(iProcessGroupID, uiPDEAddress) ;

	newPAS.pname = (char*)DMM_AllocateForKernel(String_Length(szProcessName) + 1) ;
	String_Copy(newPAS.pname, szProcessName) ;

	// Init Process State Info
	ProcessStateInfo* pStateInfo = (ProcessStateInfo*)DMM_AllocateForKernel(sizeof(ProcessStateInfo)) ;
	
	pStateInfo->uiSleepTime = 0 ;
	pStateInfo->pIRQ = &PIC::Instance().NO_IRQ ;
	pStateInfo->iWaitChildProcId = NO_PROCESS_ID ;
	pStateInfo->uiWaitResourceId = RESOURCE_NIL ;

	newPAS.pProcessStateInfo = pStateInfo ;
	newPAS.status = RUN ;

	AddToSchedulerList(iNewProcessID) ;

	//MemManager::Instance().DisplayNoOfFreePages() ;
	return ProcessManager_SUCCESS ;
}

bool ProcessManager::GetFromInterruptQueue(const IRQ& irq)
{
  PICGuard g(irq);
	unsigned val ;
	ConsumeInterrupt(irq) ;
	return DSUtil_ReadFromQueue(&(_interruptQueue[irq.GetIRQNo()]), &val) != DSUtil_ERR_QUEUE_EMPTY;
}

bool ProcessManager::QueueInterrupt(const IRQ& irq)
{
  return DSUtil_WriteToQueue(&(_interruptQueue[irq.GetIRQNo()]), 1) != DSUtil_ERR_QUEUE_FULL;
}

void ProcessManager::SignalInterruptOccured(const IRQ& irq)
{
	SetInterruptOccured(irq) ;
	QueueInterrupt(irq);
}

PS* ProcessManager::GetProcListASync()
{
  PS* pProcList;
	PS* pPS;
	ProcessAddressSpace& pAddrSpc = GetCurrentPAS() ;

	if(pAddrSpc.bIsKernelProcess == true)
	{
		pPS = pProcList = (PS*)DMM_AllocateForKernel(sizeof(PS) * _uiProcessCount) ;
	}
	else
	{
		pProcList = (PS*)DMM_Allocate(&pAddrSpc, sizeof(PS) * _uiProcessCount) ;
		pPS = (PS*)(((unsigned)pProcList + PROCESS_BASE) - GLOBAL_DATA_SEGMENT_BASE) ;
	}

	DSUtil_SLLNode* pCur = _processList.pFirst ;
	for(int i = 0; pCur != NULL; pCur = pCur->pNext, i++)
	{
		ProcessAddressSpace& p = GetAddressSpace(pCur->val) ;
		
		pPS[i].pid = pCur->val ;
		pPS[i].status = p.status ;
		pPS[i].iParentProcessID = p.iParentProcessID ;
		pPS[i].iProcessGroupID = p.iProcessGroupID ;
		pPS[i].iUserID = p.iUserID ;

		char* pname ;
		if(pAddrSpc.bIsKernelProcess == true)
		{
			pname = pPS[i].pname = (char*)DMM_AllocateForKernel(String_Length(p.pname) + 1) ;
		}
		else
		{
			pPS[i].pname = (char*)DMM_Allocate(&pAddrSpc, String_Length(p.pname) + 1) ;
			pname = (char*)((unsigned)pPS[i].pname + PROCESS_BASE - GLOBAL_DATA_SEGMENT_BASE) ;
		}
		
		String_Copy(pname, p.pname) ;
	}
  return pProcList;
}

PS* ProcessManager::GetProcList(unsigned& uiListSize)
{
  ProcessSwitchLock lock;
  uiListSize = _uiProcessCount;
  return GetProcListASync();
}

void ProcessManager::FreeProcListMem(PS* pProcList, unsigned uiListSize)
{
	ProcessAddressSpace& pAddrSpc = GetCurrentPAS();

	for(unsigned i = 0; i < uiListSize; i++)
	{
		if(pAddrSpc.bIsKernelProcess == true)
			DMM_DeAllocateForKernel((unsigned)pProcList[i].pname) ;
		else
			DMM_DeAllocate(&pAddrSpc, (unsigned)pProcList[i].pname) ;
	}
	
	if(pAddrSpc.bIsKernelProcess == true)
		DMM_DeAllocateForKernel((unsigned)pProcList) ;
	else
		DMM_DeAllocate(&pAddrSpc, (unsigned)pProcList) ;
}

void ProcessManager::SetDMMFlag(int iProcessID, bool flag)
{
	GetAddressSpace(iProcessID).uiDMMFlag = flag ;
}

bool ProcessManager::IsDMMOn(int iProcessID)
{
	return GetAddressSpace(iProcessID).uiDMMFlag ;
}

bool ProcessManager::IsKernelProcess(int iProcessID)
{
	return GetAddressSpace( iProcessID ).bIsKernelProcess ;
}

void ProcessManager_Exit()
{
	PIT_SetContextSwitch(false) ;
	ProcessManager_EXIT() ;
}

bool ProcessManager::ConsumeInterrupt(const IRQ& irq)
{
	return Atomic::Swap((int&)_interruptOccured[ irq.GetIRQNo() ], false) ;
}

void ProcessManager::SetInterruptOccured(const IRQ& irq)
{
	Atomic::Swap((int&)_interruptOccured[ irq.GetIRQNo() ], true) ;
}

bool ProcessManager::IsResourceBusy(__volatile__ unsigned uiType)
{
	return _resourceList[ uiType ] ;
}

void ProcessManager::SetResourceBusy(unsigned uiType, bool bVal)
{
	_resourceList[ uiType ] = bVal ;
}

int ProcessManager::GetCurProcId()
{
	return KERNEL_MODE ? -1 : ProcessManager::GetCurrentProcessID();
}

void ProcessManager::Kill(int iProcessID)
{
	ProcessAddressSpace* pPAS = &GetAddressSpace(iProcessID) ;
	Atomic::Swap((__volatile__ int&)(pPAS->status), static_cast<int>(TERMINATED)) ;
}

void ProcessManager::WakeUpFromKSWait(int iProcessID)
{
	ProcessAddressSpace* pPAS = &GetAddressSpace(iProcessID) ;
	pPAS->pProcessStateInfo->bKernelServiceComplete = true ;
}

void ProcessManager::WaitOnKernelService()
{
	if(GetCurProcId() < 0)
		return ; 

	ProcessManager::DisableTaskSwitch() ;

	GetCurrentPAS().pProcessStateInfo->bKernelServiceComplete = false ;
	GetCurrentPAS().status = WAIT_KERNEL_SERVICE ;

	ProcessManager_Yield() ;
}

bool ProcessManager::CopyDriveInfo(int iProcessID, int& iOldDriveId, FileSystem_PresentWorkingDirectory& mOldPWD)
{
	if(GetCurProcId() < 0)
		return false ;

	ProcessAddressSpace* pSrcPAS = &GetAddressSpace( iProcessID ) ;
	ProcessAddressSpace* pDestPAS = &GetCurrentPAS();

	iOldDriveId = pDestPAS->iDriveID ;
	MemUtil_CopyMemory(MemUtil_GetDS(), (unsigned)&(pDestPAS->processPWD), MemUtil_GetDS(), (unsigned)&(mOldPWD), sizeof(FileSystem_PresentWorkingDirectory)) ;

	pDestPAS->iDriveID = pSrcPAS->iDriveID ;
	MemUtil_CopyMemory(MemUtil_GetDS(), (unsigned)&(pSrcPAS->processPWD), MemUtil_GetDS(), (unsigned)&(pDestPAS->processPWD), 
		sizeof(FileSystem_PresentWorkingDirectory)) ;

	return true ;
}

bool ProcessManager::WakeupProcessOnInterrupt(__volatile__ int iProcessID)
{
	__volatile__ ProcessAddressSpace* p = &GetAddressSpace(iProcessID) ;
	const IRQ& irq = *p->pProcessStateInfo->pIRQ ;

	if(irq == PIC::Instance().NO_IRQ)
		return true ;

	if(irq == PIC::Instance().KEYBOARD_IRQ)
	{
		if(!(ProcessGroupManager::Instance().IsFGProcess(p->iProcessGroupID, iProcessID) && ProcessGroupManager::Instance().IsFGProcessGroup(p->iProcessGroupID)))
			return false ;
	}
	
	if(GetFromInterruptQueue(irq))
	{
		p->status = RUN;
		return true;
	}

	return false;
}

void ProcessManager::InsertIntoProcessList(int iProcessID)
{
  _uiProcessCount++;
	if(DSUtil_InsertSLL(&_processList, iProcessID) != DSUtil_SUCCESS)
		KC::MDisplay().Message("\n Failed to Insert Into Process List Data Structure\n", ' ') ;
}

void ProcessManager::DeleteFromProcessList(int iProcessID)
{
  _uiProcessCount--;
	if(DSUtil_DeleteSLL(&_processList, iProcessID) != DSUtil_SUCCESS)
		KC::MDisplay().Message("\n Failed to Delete From Process List Data Structure\n", ' ') ;
}

void ProcessManager::DeAllocateProcessInitDockMem(ProcessAddressSpace& pas)
{
	unsigned uiPDEAddress = pas.taskState.CR3_PDBR ;
	unsigned uiPDEIndex = ((PROCESS_INIT_DOCK_ADDRESS >> 22) & 0x3FF) ;
	unsigned uiPTEIndex = ((PROCESS_INIT_DOCK_ADDRESS >> 12) & 0x3FF) ;

	// I am assuming that this PTE and page are allocated solely for ProcInit docking
	// Meaning, we are not expecting any page faults to happen in this region of Process Strict Base of 16 MB
	// and actual process start address - which again is expected to fall beyond 20 MB page
	// In a way this is also controlled in MemManager::Instance().AllocatePage() by monitoring faulty address !
	
	unsigned uiPTEAddress = ((unsigned*)(uiPDEAddress - GLOBAL_DATA_SEGMENT_BASE))[uiPDEIndex] & 0xFFFFF000 ;
	
	unsigned uiPageNumber = (((unsigned*)(uiPTEAddress - GLOBAL_DATA_SEGMENT_BASE))[uiPTEIndex] & 0xFFFFF000) / PAGE_SIZE ;
	
	MemManager::Instance().DeAllocatePhysicalPage(uiPageNumber) ;
	MemManager::Instance().DeAllocatePhysicalPage(uiPTEAddress/PAGE_SIZE) ;
}

void ProcessManager::DeAllocateResources(int iProcessID)
{
	ProcessAddressSpace& pas = GetAddressSpace(iProcessID) ;

	if(pas.bIsKernelProcess == false)
	{
		DLLLoader_DeAllocateProcessDLLPTEPages(&pas) ;
		DynamicLinkLoader_UnInitialize(&pas) ;

		DMM_DeAllocatePhysicalPages(&pas) ;

		ProcessEnv_UnInitialize(pas) ;
		ProcFileManager_UnInitialize(&pas);

		DeAllocateProcessInitDockMem(pas) ;

		ProcessAllocator_DeAllocateAddressSpace(&pas) ;
	}
	else
	{
		ProcessAllocator_DeAllocateAddressSpaceForKernel(&pas) ;
	}
}

void ProcessManager::Release(int iProcessID)
{
  ProcessSwitchLock pLock;
	ProcessAddressSpace& pas = GetAddressSpace(iProcessID) ;
	DeleteFromSchedulerList(iProcessID) ;
	DMM_DeAllocateForKernel((unsigned)(pas.pname)) ;
	if(pas.pProcessStateInfo)
		DMM_DeAllocateForKernel((unsigned)(pas.pProcessStateInfo)) ;
}

bool ProcessManager::DoPoleWait()
{
	return (KERNEL_MODE || !PIT_IsTaskSwitch()) ;
}
