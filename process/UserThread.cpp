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

#include <UserThread.h>
#include <ProcessManager.h>

//thread must have a parent
UserThread::UserThread(UserProcess& parent, uint32_t entryAddress, void* arg)
    : Process("", parent.processID(), false), _parent(parent) {
  _name = _parent.name() + "_T" + upan::string::to_string(_processID);
  _mainThreadID = parent.processID();
  _processBase = _parent.getProcessBase();

  _stackPTEAddress = Process::Common::AllocatePTEForStack();
  Process::Common::AllocateStackSpace(_stackPTEAddress);
  const auto stackArgSize = PushProgramInitStackData(arg);
  const uint32_t stackTopAddress = PROCESS_STACK_TOP_ADDRESS - PROCESS_BASE;
  _taskState.BuildForUser(stackTopAddress, _parent.taskState().CR3_PDBR, entryAddress, stackArgSize);
  _processLDT.BuildForUser();
  _userID = _parent.userID();

  _parent.addThreadID(_processID);
}

void UserThread::DeAllocateResources() {
  Process::Common::DeAllocateStackSpace(_stackPTEAddress);
  MemManager::Instance().DeAllocatePhysicalPage(_stackPTEAddress / PAGE_SIZE);
}

uint32_t UserThread::PushProgramInitStackData(void* arg) {
  const uint32_t uiPTEIndex = PAGE_TABLE_ENTRIES - PROCESS_CG_STACK_PAGES - NO_OF_PAGES_FOR_STARTUP_ARGS;
  uint32_t uiPageAddress = (((unsigned*)(_stackPTEAddress - GLOBAL_DATA_SEGMENT_BASE))[uiPTEIndex]) & 0xFFFFF000;

  const uint32_t uiProcessEntryStackSize = sizeof(arg) + 4;
  uiPageAddress = uiPageAddress + PAGE_SIZE - GLOBAL_DATA_SEGMENT_BASE - uiProcessEntryStackSize;
  //call return address, unused - the thread function is a typical c function and expects the return address to be the first entry on top of call stack
  //but a thread function - unlike a typical c function, should exit() instead of return
  ((unsigned*)(uiPageAddress))[0] = 0;
  //parameter
  ((unsigned*)(uiPageAddress))[1] = (uint32_t)arg;

  return uiProcessEntryStackSize;
}

void UserThread::onLoad() {
  Process::Common::UpdatePDEWithStackPTE(_taskState.CR3_PDBR, _stackPTEAddress);
}