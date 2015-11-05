/*
 *	Mother Operating System - An x86 based Operating System
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
#include <Directory.h>
#include <StringUtil.h>
#include <MemUtil.h>
#include <DeviceDrive.h>
#include <Display.h>
#include <DMM.h>
#include <ProcessEnv.h>
#include <SystemUtil.h>
#include <FileOperations.h>

#define MAX_SECTORS_PER_RW 8

/************************************* Static Functions ***********************************/
static byte Directory_BufferedWrite(DriveInfo* pDriveInfo, unsigned uiSectorID, byte* bSectorBuffer, byte* bBuffer, 
									unsigned* uiStartSectorID, unsigned* uiPrevSectorID, unsigned* iCount, byte bFlush)
{
	byte bNewBuffering = false ;
	byte bStatus ;

	if(bFlush)
	{
		if(*iCount > 0)
		{
			RETURN_IF_NOT(bStatus, Directory_RawWrite(pDriveInfo, *uiStartSectorID, *uiStartSectorID + *iCount, bBuffer), Directory_SUCCESS) ;
		}

		*iCount = 0 ;
		return Directory_SUCCESS ;
	}

	if((*iCount) == 0)
	{
		*uiStartSectorID = *uiPrevSectorID = uiSectorID ;

		MemUtil_CopyMemory(MemUtil_GetDS(), (unsigned)bSectorBuffer, MemUtil_GetDS(), (unsigned)bBuffer, 512) ;

		*iCount = 1 ;
	}
	else if((*uiPrevSectorID) + 1 == uiSectorID)
	{
		*uiPrevSectorID = uiSectorID ;

		MemUtil_CopyMemory(MemUtil_GetDS(), (unsigned)bSectorBuffer, MemUtil_GetDS(), (unsigned)(bBuffer + (*iCount) * 512), 512) ;

		(*iCount)++ ;
	}
	else
	{
		bNewBuffering = true ;
	}

	if((*iCount) == MAX_SECTORS_PER_RW || bNewBuffering)
	{
		RETURN_IF_NOT(bStatus, Directory_RawWrite(pDriveInfo, *uiStartSectorID, *uiStartSectorID + *iCount, bBuffer), Directory_SUCCESS) ;

		*iCount = 0 ;
		
		if(bNewBuffering)
		{	
			*uiStartSectorID = *uiPrevSectorID = uiSectorID ;

			MemUtil_CopyMemory(MemUtil_GetDS(), (unsigned)bSectorBuffer, MemUtil_GetDS(), (unsigned)bBuffer, 512) ;
		
			*iCount = 1 ;
		}
	}

	return Directory_SUCCESS ;
}

void Directory_GetLastReadSectorDetails(const ProcFileDescriptor* pFDEntry, int* iSectorIndex, unsigned* uiSectorID)
{
	*iSectorIndex = pFDEntry->iLastReadSectorIndex ;
	*uiSectorID = pFDEntry->uiLastReadSectorNumber ;
}

void Directory_SetLastReadSectorDetails(ProcFileDescriptor* pFDEntry, int iSectorIndex, unsigned uiSectorID)
{
	pFDEntry->iLastReadSectorIndex = iSectorIndex ;
	pFDEntry->uiLastReadSectorNumber = uiSectorID ;
}

/**********************************************************************************************/

byte Directory_Create(ProcessAddressSpace* processAddressSpace, int iDriveID, byte* bParentDirectoryBuffer, const FileSystem_CWD* pCWD, 
		char* szDirName, unsigned short usDirAttribute)
{
	byte bStatus ;
	byte bIsPresent ;
	byte bSectorBuffer[512] ;
	unsigned uiSectorNo ;
	byte bSectorPos ;
	unsigned uiFreeSectorID ;

	GET_DRIVE_FOR_FS_OPS(iDriveID, Directory_FAILURE) ;

	FileSystem_PresentWorkingDirectory* pPWD = &(processAddressSpace->processPWD) ;

	if(pCWD->pDirEntry->uiStartSectorID == EOC)
	{
		RETURN_IF_NOT(bStatus, FileSystem_AllocateSector(pDriveInfo, &uiFreeSectorID), FileSystem_SUCCESS) ;

		uiSectorNo = uiFreeSectorID ;
		bSectorPos = 0 ;

		pCWD->pDirEntry->uiStartSectorID = uiFreeSectorID ;
	}
	else
	{
		RETURN_IF_NOT(bStatus, Directory_FindDirectory(pDriveInfo, pCWD, szDirName, &uiSectorNo, &bSectorPos, &bIsPresent, bSectorBuffer), Directory_SUCCESS) ;

		if(bIsPresent == true)
			return Directory_ERR_EXISTS ;

		if(bSectorPos == EOC_B)
		{
			RETURN_IF_NOT(bStatus, FileSystem_AllocateSector(pDriveInfo, &uiFreeSectorID), FileSystem_SUCCESS) ;

			RETURN_IF_NOT(bStatus, FileSystem_SetSectorEntryValue(pDriveInfo, uiSectorNo, uiFreeSectorID), FileSystem_SUCCESS) ;

			uiSectorNo = uiFreeSectorID ;
			bSectorPos = 0 ;
		}
	}

	Directory_PopulateDirEntry(((FileSystem_DIR_Entry*)bSectorBuffer) + bSectorPos, szDirName, usDirAttribute, 
		processAddressSpace->iUserID, pCWD->uiSectorNo, pCWD->bSectorEntryPosition) ;

	RETURN_IF_NOT(bStatus, Directory_RawWrite(pDriveInfo, uiSectorNo, uiSectorNo + 1, bSectorBuffer), Directory_SUCCESS) ;

	pCWD->pDirEntry->uiSize++ ;

	RETURN_IF_NOT(bStatus, Directory_RawWrite(pDriveInfo, pCWD->uiSectorNo, pCWD->uiSectorNo + 1, bParentDirectoryBuffer), Directory_SUCCESS) ;

	if(pDriveInfo->drive.iID == processAddressSpace->iDriveID
			&& pCWD->uiSectorNo == pPWD->uiSectorNo
			&& pCWD->bSectorEntryPosition == pPWD->bSectorEntryPosition)
	{
		MemUtil_CopyMemory(MemUtil_GetDS(), (unsigned)pCWD->pDirEntry, MemUtil_GetDS(), (unsigned)&pPWD->DirEntry, sizeof(FileSystem_DIR_Entry)) ;
	}

	//TODO: Required Only If "/" Dir Entry is Created
	if(String_Compare((const char*)pCWD->pDirEntry->Name, FS_ROOT_DIR) == 0)
	{
		MemUtil_CopyMemory(MemUtil_GetDS(), (unsigned)pCWD->pDirEntry, MemUtil_GetDS(), 
							(unsigned)&(pDriveInfo->FSMountInfo.FSpwd.DirEntry),
							sizeof(FileSystem_DIR_Entry)) ;
	}

	return Directory_SUCCESS ;
}

byte Directory_Delete(ProcessAddressSpace* processAddressSpace, int iDriveID, byte* bParentDirectoryBuffer, const FileSystem_CWD* pCWD, 
				const char* szDirName)
{
	byte bStatus ;
	byte bIsPresent ;
	byte bSectorBuffer[512] ;
	unsigned uiSectorNo ;
	byte bSectorPos ;

	GET_DRIVE_FOR_FS_OPS(iDriveID, Directory_FAILURE) ;
	FileSystem_PresentWorkingDirectory* pPWD = &(processAddressSpace->processPWD) ;

	if(pCWD->pDirEntry->uiStartSectorID == EOC)
	{
		return Directory_ERR_NOT_EXISTS ;
	}
	else
	{
		RETURN_IF_NOT(bStatus, Directory_FindDirectory(pDriveInfo, pCWD, szDirName, &uiSectorNo, &bSectorPos, &bIsPresent, bSectorBuffer), Directory_SUCCESS) ;

		if(bIsPresent == false)
			return Directory_ERR_NOT_EXISTS ;
	}

	FileSystem_DIR_Entry* delDir = ((FileSystem_DIR_Entry*)bSectorBuffer) + bSectorPos ;

	if((delDir->usAttribute & ATTR_TYPE_DIRECTORY) == ATTR_TYPE_DIRECTORY)
	{
		if(delDir->uiSize != 0)
			return Directory_ERR_NOT_EMPTY ;
	}

	unsigned uiCurrentSectorID = delDir->uiStartSectorID ;
	unsigned uiNextSectorID ;

	while(uiCurrentSectorID != EOC)
	{
		RETURN_IF_NOT(bStatus, FileSystem_DeAllocateSector(pDriveInfo, uiCurrentSectorID, &uiNextSectorID), FileSystem_SUCCESS) ;

		uiCurrentSectorID = uiNextSectorID ;
	}

	delDir->usAttribute |= ATTR_DELETED_DIR ;

	RETURN_IF_NOT(bStatus, Directory_RawWrite(pDriveInfo, uiSectorNo, uiSectorNo + 1, bSectorBuffer), Directory_SUCCESS) ;

	pCWD->pDirEntry->uiSize-- ;

	RETURN_IF_NOT(bStatus, Directory_RawWrite(pDriveInfo, pCWD->uiSectorNo, pCWD->uiSectorNo + 1, bParentDirectoryBuffer), Directory_SUCCESS) ;
	
	if(pDriveInfo->drive.iID == processAddressSpace->iDriveID
			&& pCWD->uiSectorNo == pPWD->uiSectorNo
			&& pCWD->bSectorEntryPosition == pPWD->bSectorEntryPosition)
	{
		MemUtil_CopyMemory(MemUtil_GetDS(), (unsigned)pCWD->pDirEntry, MemUtil_GetDS(), (unsigned)&pPWD->DirEntry, sizeof(FileSystem_DIR_Entry)) ;
	}

	//TODO: Required Only If "/" Dir Entry is Created
	if(String_Compare((const char*)pCWD->pDirEntry->Name, FS_ROOT_DIR) == 0)
	{
		MemUtil_CopyMemory(MemUtil_GetDS(), (unsigned)pCWD->pDirEntry, MemUtil_GetDS(), 
							(unsigned)&(pDriveInfo->FSMountInfo.FSpwd.DirEntry),
							sizeof(FileSystem_DIR_Entry)) ;
	}

	return Directory_SUCCESS ;
}

byte Directory_GetDirEntryForCreateDelete(const ProcessAddressSpace* processAddressSpace, int iDriveID, const char* szDirPath, char* szDirName, 
			unsigned* uiSectorNo, byte* bSectorPos, byte* bDirectoryBuffer)
{
	byte bStatus ;
	byte bIsPresent ;
	FileSystem_CWD CWD ;

	GET_DRIVE_FOR_FS_OPS(iDriveID, Directory_FAILURE) ;
	FileSystem_MountInfo* pFSMountInfo = &pDriveInfo->FSMountInfo ;

	if(String_Length(szDirPath) == 0 ||	String_Compare(FS_ROOT_DIR, szDirPath) == 0)
		return Directory_ERR_EXISTS ;

	if(szDirPath[0] == '/' || processAddressSpace->iDriveID != iDriveID)
	{
		CWD.pDirEntry = &(pFSMountInfo->FSpwd.DirEntry) ;
		CWD.uiSectorNo = *uiSectorNo = pFSMountInfo->FSpwd.uiSectorNo ;
		CWD.bSectorEntryPosition = *bSectorPos = pFSMountInfo->FSpwd.bSectorEntryPosition ;
	}
	else
	{
		CWD.pDirEntry = const_cast<FileSystem_DIR_Entry*>(&(processAddressSpace->processPWD.DirEntry)) ;
		CWD.uiSectorNo = *uiSectorNo = processAddressSpace->processPWD.uiSectorNo ;
		CWD.bSectorEntryPosition = *bSectorPos = processAddressSpace->processPWD.bSectorEntryPosition ;
	}

	RETURN_IF_NOT(bStatus, Directory_RawRead(pDriveInfo, *uiSectorNo, *uiSectorNo + 1, bDirectoryBuffer), Directory_SUCCESS) ;

	int iListSize, i ;

	StringDefTokenizer tokenizer ;

	String_Tokenize(szDirPath, '/', &iListSize, tokenizer) ;

	for(i = 0; i < iListSize - 1; i++)
	{
		RETURN_IF_NOT(bStatus, Directory_FindDirectory(pDriveInfo, &CWD, tokenizer.szToken[i], uiSectorNo, bSectorPos, &bIsPresent, bDirectoryBuffer), Directory_SUCCESS) ;

		if(bIsPresent == false)
			return Directory_ERR_NOT_EXISTS ;

		CWD.pDirEntry = ((FileSystem_DIR_Entry*)bDirectoryBuffer) + *bSectorPos ;
		CWD.uiSectorNo = *uiSectorNo ;
		CWD.bSectorEntryPosition = *bSectorPos ;
	}

	String_Copy(szDirName, tokenizer.szToken[i]) ;
	
	if(String_Compare(DIR_SPECIAL_CURRENT, szDirName) == 0 ||
		String_Compare(DIR_SPECIAL_PARENT, szDirName) == 0)
		return Directory_ERR_SPECIAL_ENTRIES ;

	return Directory_SUCCESS ;
}

byte Directory_GetDirectoryContent(const char* szFileName, ProcessAddressSpace* processAddressSpace, int iDriveID, FileSystem_DIR_Entry** pDirList, int* iListSize)
{
	byte bStatus ;
	byte bDirectoryBuffer[512] ;

	GET_DRIVE_FOR_FS_OPS(iDriveID, Directory_FAILURE) ;
	FileSystem_CWD CWD ;
	if(processAddressSpace->iDriveID == iDriveID)
	{
		CWD.pDirEntry = &(processAddressSpace->processPWD.DirEntry) ;
		CWD.uiSectorNo = processAddressSpace->processPWD.uiSectorNo ;
		CWD.bSectorEntryPosition = processAddressSpace->processPWD.bSectorEntryPosition ;
	}
	else
	{
		CWD.pDirEntry = &(pDriveInfo->FSMountInfo.FSpwd.DirEntry) ;
		CWD.uiSectorNo = pDriveInfo->FSMountInfo.FSpwd.uiSectorNo ;
		CWD.bSectorEntryPosition = pDriveInfo->FSMountInfo.FSpwd.bSectorEntryPosition ;
	}

	FileSystem_DIR_Entry* dirFile ;
	FileSystem_DIR_Entry* pAddress ;

	if(String_Length(szFileName) == 0)
	{
		dirFile = CWD.pDirEntry ;
	}
	else
	{
		unsigned uiSectorNo ;
		byte bSectorPos ;

		RETURN_IF_NOT(bStatus, Directory_GetDirEntryInfo(pDriveInfo, &CWD, szFileName, &uiSectorNo, &bSectorPos, bDirectoryBuffer), Directory_SUCCESS) ;

		dirFile = ((FileSystem_DIR_Entry*)bDirectoryBuffer) + bSectorPos ;

		if(dirFile->usAttribute & ATTR_DELETED_DIR)
			return Directory_ERR_NOT_EXISTS ;

		if((dirFile->usAttribute & ATTR_TYPE_DIRECTORY) != ATTR_TYPE_DIRECTORY)
		{
			*iListSize = 1 ;

			if(processAddressSpace->bIsKernelProcess == true)
			{
				*pDirList = (FileSystem_DIR_Entry*)DMM_AllocateForKernel(sizeof(FileSystem_DIR_Entry)) ;
				pAddress = *pDirList ;
			}
			else
			{
				*pDirList = (FileSystem_DIR_Entry*)DMM_Allocate(processAddressSpace, sizeof(FileSystem_DIR_Entry)) ;
				pAddress = (FileSystem_DIR_Entry*)(((unsigned)*pDirList + PROCESS_BASE) - GLOBAL_DATA_SEGMENT_BASE) ;
			}

			MemUtil_CopyMemory(MemUtil_GetDS(), (unsigned)dirFile, MemUtil_GetDS(), (unsigned)pAddress, sizeof(FileSystem_DIR_Entry)) ;

			return Directory_SUCCESS ;
		}
	}

	unsigned uiCurrentSectorID ;
	int iScanDirCount = 0;
	byte bSectorPosIndex ;

	FileSystem_DIR_Entry* curDir ;

	iScanDirCount = 0 ;
	uiCurrentSectorID = dirFile->uiStartSectorID ;
	*iListSize = dirFile->uiSize ;

	if(processAddressSpace->bIsKernelProcess == true)
	{
		*pDirList = (FileSystem_DIR_Entry*)DMM_AllocateForKernel(sizeof(FileSystem_DIR_Entry) * (*iListSize)) ;
		pAddress = *pDirList ;
	}
	else
	{
		*pDirList = (FileSystem_DIR_Entry*)DMM_Allocate(processAddressSpace, sizeof(FileSystem_DIR_Entry) * (*iListSize)) ;
		pAddress = (FileSystem_DIR_Entry*)(((unsigned)*pDirList + PROCESS_BASE) - GLOBAL_DATA_SEGMENT_BASE) ;
	}

	while(uiCurrentSectorID != EOC)
	{
		RETURN_IF_NOT(bStatus, Directory_RawRead(pDriveInfo, uiCurrentSectorID, uiCurrentSectorID + 1, bDirectoryBuffer), Directory_SUCCESS) ;

		for(bSectorPosIndex = 0; bSectorPosIndex < DIR_ENTRIES_PER_SECTOR; bSectorPosIndex++)
		{
			curDir = ((FileSystem_DIR_Entry*)bDirectoryBuffer) + bSectorPosIndex ;

			if(curDir->usAttribute & ATTR_DELETED_DIR)
			{
				continue ;
			}
			else
			{
				MemUtil_CopyMemory(MemUtil_GetDS(), (unsigned)curDir, MemUtil_GetDS(), 
								(unsigned)&(pAddress[iScanDirCount]), sizeof(FileSystem_DIR_Entry)) ;
				iScanDirCount++ ;
				if(iScanDirCount >= *iListSize)
					return Directory_SUCCESS ;
			}
		}

		RETURN_IF_NOT(bStatus, FileSystem_GetSectorEntryValue(pDriveInfo, uiCurrentSectorID, &uiCurrentSectorID), FileSystem_SUCCESS) ;
	}

	return Directory_SUCCESS ;
}

void Directory_PopulateDirEntry(FileSystem_DIR_Entry* dirEntry, char* szDirName, unsigned short usDirAttribute, int iUserID, unsigned uiParentSecNo, byte bParentSecPos)
{
	String_Copy((char*)dirEntry->Name, szDirName) ;
	
	dirEntry->usAttribute = usDirAttribute ;
	
	SystemUtil_GetTimeOfDay(&dirEntry->CreatedTime) ;
	SystemUtil_GetTimeOfDay(&dirEntry->AccessedTime) ;
	SystemUtil_GetTimeOfDay(&dirEntry->ModifiedTime) ;

	dirEntry->uiStartSectorID = EOC ;
	dirEntry->uiSize = 0 ;

	dirEntry->uiParentSecID = uiParentSecNo ;
	dirEntry->bParentSectorPos = bParentSecPos ;

	dirEntry->iUserID = iUserID ;
}

byte Directory_FindDirectory(DriveInfo* pDriveInfo, const FileSystem_CWD* pCWD, const char* szDirName, unsigned* uiSectorNo, byte* bSectorPos, byte* bIsPresent, byte* bDestSectorBuffer)
{
	if((pCWD->pDirEntry->usAttribute & ATTR_TYPE_DIRECTORY) == 0)
		return Directory_ERR_NOT_DIR ;

	byte bStatus ;
	byte bSectorBuffer[512] ;

	if(String_Compare(szDirName, DIR_SPECIAL_CURRENT) == 0)
	{
		*uiSectorNo = pCWD->uiSectorNo ;
		*bSectorPos = pCWD->bSectorEntryPosition ;
		*bIsPresent = true ;

		RETURN_IF_NOT(bStatus, Directory_RawRead(pDriveInfo, *uiSectorNo, *uiSectorNo + 1, bDestSectorBuffer), Directory_SUCCESS) ;
			
		return Directory_SUCCESS ;
	}

	FileSystem_DIR_Entry* pDirEntry = pCWD->pDirEntry ;

	if(String_Compare(szDirName, DIR_SPECIAL_PARENT) == 0)
	{
		if(String_Compare((const char*)pDirEntry->Name, FS_ROOT_DIR) == 0)
		{
			*uiSectorNo = pCWD->uiSectorNo ;
			*bSectorPos = pCWD->bSectorEntryPosition ;
		}
		else
		{
			*uiSectorNo = pDirEntry->uiParentSecID ;
			*bSectorPos = pDirEntry->bParentSectorPos ;
		}
		*bIsPresent = true ;
	
		RETURN_IF_NOT(bStatus, Directory_RawRead(pDriveInfo, *uiSectorNo, *uiSectorNo + 1, bDestSectorBuffer), Directory_SUCCESS) ;
			
		return Directory_SUCCESS ;
	}

	byte bDeletedEntryFound ;
	unsigned uiCurrentSectorID ;
	unsigned uiNextSectorID ;
	unsigned uiScanDirCount ;
	byte bSectorPosIndex ;

	FileSystem_DIR_Entry* curDir ;

	*uiSectorNo = EOC ;
	*bSectorPos = EOC_B ;
	*bIsPresent = false ;
	bDeletedEntryFound = false ;
	uiScanDirCount = 0 ;

	uiCurrentSectorID = pDirEntry->uiStartSectorID ;

	while(uiCurrentSectorID != EOC)
	{
		RETURN_IF_NOT(bStatus, Directory_RawRead(pDriveInfo, uiCurrentSectorID, uiCurrentSectorID + 1, bSectorBuffer), Directory_SUCCESS) ;

		for(bSectorPosIndex = 0; bSectorPosIndex < DIR_ENTRIES_PER_SECTOR; bSectorPosIndex++)
		{
			curDir = ((FileSystem_DIR_Entry*)bSectorBuffer) + bSectorPosIndex ;

			if(String_Compare(szDirName, (const char*)curDir->Name) == 0 && !(curDir->usAttribute & ATTR_DELETED_DIR))
			{
				MemUtil_CopyMemory(MemUtil_GetDS(), (unsigned)&bSectorBuffer, MemUtil_GetDS(), (unsigned)bDestSectorBuffer, 512) ;
				*uiSectorNo = uiCurrentSectorID ;
				*bSectorPos = bSectorPosIndex ;
				*bIsPresent = true ;
				
				return Directory_SUCCESS ;
			}

			if((curDir->usAttribute & ATTR_DELETED_DIR)) 
			{
				if(bDeletedEntryFound == false)
				{
					MemUtil_CopyMemory(MemUtil_GetDS(), (unsigned)&bSectorBuffer, MemUtil_GetDS(), (unsigned)bDestSectorBuffer, 512) ;
					*uiSectorNo = uiCurrentSectorID ;
					*bSectorPos = bSectorPosIndex ;
			
					bDeletedEntryFound = true ;
				}
			}
			else
			{
				uiScanDirCount++ ;
				if(uiScanDirCount >= pDirEntry->uiSize)
					break ;
			}
		}

		RETURN_IF_NOT(bStatus, FileSystem_GetSectorEntryValue(pDriveInfo, uiCurrentSectorID, &uiNextSectorID), FileSystem_SUCCESS) ;

		if(uiScanDirCount >= pDirEntry->uiSize)
		{
			if(bDeletedEntryFound == true)
				return Directory_SUCCESS ;

			if(bSectorPosIndex < DIR_ENTRIES_PER_SECTOR - 1)
			{
				MemUtil_CopyMemory(MemUtil_GetDS(), (unsigned)&bSectorBuffer, MemUtil_GetDS(), (unsigned)bDestSectorBuffer, 512) ;
				*uiSectorNo = uiCurrentSectorID ;
				*bSectorPos = bSectorPosIndex + 1 ;

				return Directory_SUCCESS ;
			}

			if(bSectorPosIndex == DIR_ENTRIES_PER_SECTOR - 1)
			{
				if(uiNextSectorID != EOC)
				{
					*uiSectorNo = uiNextSectorID ;
					*bSectorPos = 0 ;
	
					return Directory_SUCCESS ;
				}
				
				*uiSectorNo = uiCurrentSectorID ;
				*bSectorPos = EOC_B ;

				return Directory_SUCCESS ;
			}
		}
		uiCurrentSectorID = uiNextSectorID ;
	}

	return Directory_ERR_FS_TABLE_CORRUPTED ;
}

byte Directory_RawRead(DriveInfo* pDriveInfo, unsigned uiStartSectorID, unsigned uiEndSectorID, byte* bSectorBuffer)
{
	byte bStatus ;

	uiStartSectorID = FileSystem_GetRealSectorNumber(uiStartSectorID, pDriveInfo) ;
	uiEndSectorID = FileSystem_GetRealSectorNumber(uiEndSectorID, pDriveInfo);

	RETURN_IF_NOT(bStatus, DeviceDrive_Read(pDriveInfo, uiStartSectorID, (uiEndSectorID - uiStartSectorID), bSectorBuffer), DeviceDrive_SUCCESS) ;

	return Directory_SUCCESS ;
}

byte Directory_RawWrite(DriveInfo* pDriveInfo, unsigned uiStartSectorID, unsigned uiEndSectorID, byte* bSectorBuffer)
{
	byte bStatus ;

	uiStartSectorID = FileSystem_GetRealSectorNumber(uiStartSectorID, pDriveInfo) ;
	uiEndSectorID = FileSystem_GetRealSectorNumber(uiEndSectorID, pDriveInfo) ;

	RETURN_IF_NOT(bStatus, DeviceDrive_Write(pDriveInfo, uiStartSectorID, (uiEndSectorID - uiStartSectorID), bSectorBuffer), DeviceDrive_SUCCESS) ;

	return Directory_SUCCESS ;
}

byte Directory_FileWrite(DriveInfo* pDriveInfo, FileSystem_CWD* pCWD, ProcFileDescriptor* pFDEntry, byte* bDataBuffer, unsigned uiDataSize)
{
	if(uiDataSize == 0)
		return Directory_ERR_ZERO_WRITE_SIZE ;

	byte bStatus ;
	unsigned uiSectorNo ;
	byte bSectorPos ;
	byte bDirectoryBuffer[512] ;
	const char* szFileName = pFDEntry->szFileName ;
	unsigned uiOffset = pFDEntry->uiOffset ;

	RETURN_IF_NOT(bStatus, Directory_GetDirEntryInfo(pDriveInfo, pCWD, szFileName, &uiSectorNo, &bSectorPos, bDirectoryBuffer), Directory_SUCCESS) ;

	FileSystem_DIR_Entry* dirFile = ((FileSystem_DIR_Entry*)bDirectoryBuffer) + bSectorPos ;

	if((dirFile->usAttribute & ATTR_TYPE_DIRECTORY) != 0)
		return Directory_ERR_IS_DIRECTORY ;

	RETURN_IF_NOT(bStatus, Directory_ActualFileWrite(pDriveInfo, bDataBuffer, pFDEntry, uiDataSize, dirFile), Directory_SUCCESS) ;

	if(dirFile->uiSize < (uiOffset + uiDataSize))
	{
		dirFile->uiSize = uiOffset + uiDataSize ;

		RETURN_IF_NOT(bStatus, Directory_RawWrite(pDriveInfo, uiSectorNo, uiSectorNo + 1, bDirectoryBuffer), Directory_SUCCESS) ;
	}

	return Directory_SUCCESS ;
}

byte Directory_ActualFileWrite(DriveInfo* pDriveInfo, byte* bDataBuffer, ProcFileDescriptor* pFDEntry,
	   							unsigned uiDataSize, FileSystem_DIR_Entry* dirFile)
{
	unsigned uiCurrentSectorID, uiNextSectorID, uiPrevSectorID = EOC ;
	int iStartWriteSectorNo, iStartWriteSectorPos ;
	int iSectorIndex ;
	unsigned uiWriteRemainingCount, uiWrittenCount ;
	unsigned uiCurrentFileSize ;
	unsigned uiOffset = pFDEntry->uiOffset ;

	byte bStatus ; 
	byte bStartAllocation ;
	byte bSectorBuffer[512] ;

	iStartWriteSectorNo = uiOffset / 512 ;
	iStartWriteSectorPos = uiOffset % 512 ;

	uiCurrentFileSize = dirFile->uiSize ;

	Directory_GetLastReadSectorDetails(pFDEntry, &iSectorIndex, &uiCurrentSectorID) ;

	if(iSectorIndex < 0 || iSectorIndex > iStartWriteSectorNo)
	{
		iSectorIndex = 0 ;
		uiCurrentSectorID = dirFile->uiStartSectorID ;
	}
	
	while(iSectorIndex < iStartWriteSectorNo && uiCurrentSectorID != EOC)
	{
		RETURN_IF_NOT(bStatus, FileSystem_GetSectorEntryValue(pDriveInfo, uiCurrentSectorID, &uiNextSectorID), FileSystem_SUCCESS) ;

		iSectorIndex++ ;
		uiPrevSectorID = uiCurrentSectorID ;
		uiCurrentSectorID = uiNextSectorID ;
	}

	if(uiCurrentSectorID == EOC)
	{
		memset((char*)bSectorBuffer, 0, 512) ;

		do
		{
			RETURN_IF_NOT(bStatus, FileSystem_AllocateSector(pDriveInfo, &uiCurrentSectorID), FileSystem_SUCCESS) ;

			if(dirFile->uiStartSectorID == EOC)
			{
				dirFile->uiStartSectorID = uiCurrentSectorID ;
			}
			else
			{
				RETURN_IF_NOT(bStatus, FileSystem_SetSectorEntryValue(pDriveInfo, uiPrevSectorID, uiCurrentSectorID), FileSystem_SUCCESS) ;
			}
			
			uiPrevSectorID = uiCurrentSectorID ;

			RETURN_IF_NOT(bStatus, Directory_RawWrite(pDriveInfo, uiCurrentSectorID, uiCurrentSectorID + 1, bSectorBuffer), Directory_SUCCESS) ;
			
			Directory_SetLastReadSectorDetails(pFDEntry, iSectorIndex, uiCurrentSectorID) ;

			iSectorIndex++ ;

		} while(iSectorIndex <= iStartWriteSectorNo) ;
	}
	else
	{
		Directory_SetLastReadSectorDetails(pFDEntry, iSectorIndex, uiCurrentSectorID) ;
	}

	uiWrittenCount = 0 ;
	uiWriteRemainingCount = uiDataSize ;

	if(iStartWriteSectorPos != 0)
	{
		RETURN_IF_NOT(bStatus, Directory_RawRead(pDriveInfo, uiCurrentSectorID, uiCurrentSectorID + 1, bSectorBuffer), Directory_SUCCESS) ;

		uiWrittenCount = 512 - iStartWriteSectorPos ;
		if(uiDataSize <= uiWrittenCount)
			uiWrittenCount = uiDataSize ;

		MemUtil_CopyMemory(MemUtil_GetDS(), (unsigned)bDataBuffer, MemUtil_GetDS(), 
						(unsigned)(bSectorBuffer + iStartWriteSectorPos), uiWrittenCount) ;

		RETURN_IF_NOT(bStatus,
			Directory_RawWrite(pDriveInfo, uiCurrentSectorID, uiCurrentSectorID + 1, bSectorBuffer), Directory_SUCCESS) ;
			
		if(uiWrittenCount == uiDataSize)
			return Directory_SUCCESS ;

       RETURN_IF_NOT(bStatus, FileSystem_GetSectorEntryValue(pDriveInfo, uiCurrentSectorID, &uiNextSectorID), FileSystem_SUCCESS) ;

        uiPrevSectorID = uiCurrentSectorID ;
        uiCurrentSectorID = uiNextSectorID ;

        uiWriteRemainingCount -= uiWrittenCount ;
	}

	bStartAllocation = false ;
	
	unsigned uiBufStartSectorID, uiBufPrecSectorID ;
	byte bWriteBuffer[MAX_SECTORS_PER_RW * 512] ;
	int iBufCount = 0 ;

	while(true)
	{
		if(uiCurrentSectorID == EOC || bStartAllocation == true)
		{
			bStartAllocation = true ;
			RETURN_IF_NOT(bStatus, FileSystem_AllocateSector(pDriveInfo, &uiCurrentSectorID), FileSystem_SUCCESS) ;
			RETURN_IF_NOT(bStatus, FileSystem_SetSectorEntryValue(pDriveInfo, uiPrevSectorID, uiCurrentSectorID), FileSystem_SUCCESS) ;
		}
		
		if(uiWriteRemainingCount < 512)
		{
			if(bStartAllocation == false && (uiOffset + uiDataSize) < uiCurrentFileSize)
			{
				RETURN_IF_NOT(bStatus, 
							Directory_RawRead(pDriveInfo, uiCurrentSectorID, uiCurrentSectorID + 1, bSectorBuffer),
							FileSystem_SUCCESS) ;
			}

			MemUtil_CopyMemory(MemUtil_GetDS(), (unsigned)(bDataBuffer + uiWrittenCount), MemUtil_GetDS(),
							(unsigned)bSectorBuffer, uiWriteRemainingCount) ;
							
			RETURN_IF_NOT(bStatus, 
						Directory_BufferedWrite(pDriveInfo, uiCurrentSectorID, bSectorBuffer,
						bWriteBuffer, &uiBufStartSectorID, &uiBufPrecSectorID, (unsigned*)&iBufCount, 
						false),
						Directory_SUCCESS) ;

			RETURN_IF_NOT(bStatus, 
						Directory_BufferedWrite(pDriveInfo, EOC, NULL,
						bWriteBuffer, &uiBufStartSectorID, &uiBufPrecSectorID, (unsigned*)&iBufCount, 
						true),
						Directory_SUCCESS) ;
		
			return Directory_SUCCESS ;
		}

		RETURN_IF_NOT(bStatus,
				Directory_BufferedWrite(pDriveInfo, uiCurrentSectorID, 
				bDataBuffer + uiWrittenCount,
				bWriteBuffer, &uiBufStartSectorID, &uiBufPrecSectorID, (unsigned*)&iBufCount, false),
				Directory_SUCCESS) ;
		
		uiWrittenCount += 512 ;
		uiWriteRemainingCount -= 512 ;

		if(uiWriteRemainingCount == 0)
		{
			RETURN_IF_NOT(bStatus, 
						Directory_BufferedWrite(pDriveInfo, EOC, NULL,
						bWriteBuffer, &uiBufStartSectorID, &uiBufPrecSectorID, (unsigned*)&iBufCount, 
						true),
						Directory_SUCCESS) ;

			return Directory_SUCCESS ;
		}

		RETURN_IF_NOT(bStatus, FileSystem_GetSectorEntryValue(pDriveInfo, uiCurrentSectorID, &uiNextSectorID), FileSystem_SUCCESS) ;

		uiPrevSectorID = uiCurrentSectorID ;
		uiCurrentSectorID = uiNextSectorID ;
	}

	return Directory_ERR_FS_TABLE_CORRUPTED ;
}

byte Directory_FileRead(DriveInfo* pDriveInfo, FileSystem_CWD* pCWD, ProcFileDescriptor* pFDEntry, byte* bDataBuffer, 
						unsigned uiDataSize, unsigned* uiReadFileSize)

{
	*uiReadFileSize = 0 ;

	const char* szFileName = pFDEntry->szFileName ;
	unsigned uiOffset = pFDEntry->uiOffset ;

	byte bStatus ;
	FileSystem_DIR_Entry* pDirFile ;
	byte bDirectoryBuffer[512] ;
	unsigned uiSectorNo ;
	byte bSectorPos ;


	RETURN_IF_NOT(bStatus, Directory_GetDirEntryInfo(pDriveInfo, pCWD, szFileName, &uiSectorNo, &bSectorPos, bDirectoryBuffer), Directory_SUCCESS) ;
		
	pDirFile = ((FileSystem_DIR_Entry*)bDirectoryBuffer) + bSectorPos ;
	if((pDirFile->usAttribute & ATTR_TYPE_DIRECTORY) != 0)
		return Directory_ERR_IS_DIRECTORY ;

	if(uiOffset >= pDirFile->uiSize)
		return Directory_ERR_EOF ;
		
	if(pDirFile->uiSize == 0)
	{
		bDataBuffer[0] = '\0' ;
		return Directory_SUCCESS ;
	}

	unsigned uiCurrentSectorID, uiNextSectorID, uiStartSectorID ;
	int iStartReadSectorNo, iStartReadSectorPos ;
	int iSectorIndex ;
	unsigned uiCurrentFileSize ;
	int iReadRemainingCount, iReadCount, iCurrentReadSize, iSectorCount ;

	byte bSectorBuffer[MAX_SECTORS_PER_RW * 512] ;

	iStartReadSectorNo = uiOffset / 512 ;
	iStartReadSectorPos = uiOffset % 512 ;

	uiCurrentFileSize = pDirFile->uiSize ;

	Directory_GetLastReadSectorDetails(pFDEntry, &iSectorIndex, &uiCurrentSectorID) ;

	if(iSectorIndex < 0 || iSectorIndex > iStartReadSectorNo)
	{
		iSectorIndex = 0 ;
		uiCurrentSectorID = pDirFile->uiStartSectorID ;
	}
	
	while(iSectorIndex != iStartReadSectorNo)
	{
		RETURN_IF_NOT(bStatus, FileSystem_GetSectorEntryValue(pDriveInfo, uiCurrentSectorID, &uiNextSectorID), FileSystem_SUCCESS) ;

		iSectorIndex++ ;
		uiCurrentSectorID = uiNextSectorID ;
	}

	Directory_SetLastReadSectorDetails(pFDEntry, iSectorIndex, uiCurrentSectorID) ;
	unsigned uiLastReadSectorNumber = uiCurrentSectorID ;
	int iLastReadSectorIndex = iSectorIndex ;

	iReadCount = 0 ;
	iReadRemainingCount = (uiDataSize < (uiCurrentFileSize - uiOffset) && uiDataSize > 0) ? uiDataSize : (uiCurrentFileSize  - uiOffset) ;

	iSectorCount = 0 ;

	while(true)
	{
		if(uiCurrentSectorID == EOC)
		{
			*uiReadFileSize = iReadCount ;
			Directory_SetLastReadSectorDetails(pFDEntry, iLastReadSectorIndex, uiLastReadSectorNumber) ;
			return Directory_SUCCESS ;
		}
		
		uiStartSectorID = uiCurrentSectorID ;

		iLastReadSectorIndex += iSectorCount ;
		uiLastReadSectorNumber = uiCurrentSectorID ;

		iSectorCount = 1 ;

		for(;;)
		{
			RETURN_IF_NOT(bStatus, FileSystem_GetSectorEntryValue(pDriveInfo, uiCurrentSectorID, &uiNextSectorID), FileSystem_SUCCESS) ;

			if(uiCurrentSectorID + 1 == uiNextSectorID)
			{
				uiCurrentSectorID = uiNextSectorID ;

				iSectorCount++ ;
				iCurrentReadSize = iSectorCount * 512 - iStartReadSectorPos ;

				if(iReadRemainingCount <= iCurrentReadSize)
				{
					if((iStartReadSectorPos + iReadRemainingCount) <= 512)
						iSectorCount-- ;

					iCurrentReadSize = iReadRemainingCount ;
					break ;
				}

				if(iSectorCount == MAX_SECTORS_PER_RW)
				{
					RETURN_IF_NOT(bStatus, FileSystem_GetSectorEntryValue(pDriveInfo, uiCurrentSectorID, &uiNextSectorID), FileSystem_SUCCESS) ;

					uiCurrentSectorID = uiNextSectorID ;

					break ;	
				}
			}
			else
			{
				iCurrentReadSize = iSectorCount * 512 - iStartReadSectorPos ;

				if(iReadRemainingCount <= iCurrentReadSize)
					iCurrentReadSize = iReadRemainingCount ;

				uiCurrentSectorID = uiNextSectorID ;
				break ;
			}
		}

		RETURN_IF_NOT(bStatus, Directory_RawRead(pDriveInfo, uiStartSectorID, uiStartSectorID + iSectorCount, bSectorBuffer), FileSystem_SUCCESS) ;

		MemUtil_CopyMemory(MemUtil_GetDS(), (unsigned)bSectorBuffer + iStartReadSectorPos, MemUtil_GetDS(), (unsigned)bDataBuffer + iReadCount, iCurrentReadSize) ;

		iReadCount += iCurrentReadSize ;
		iReadRemainingCount -= iCurrentReadSize ;

		iStartReadSectorPos = 0 ;

		if(iReadRemainingCount <= 0)
		{
			Directory_SetLastReadSectorDetails(pFDEntry, iLastReadSectorIndex, uiLastReadSectorNumber) ;
			*uiReadFileSize = iReadCount ;
			return Directory_SUCCESS ;
		}
	}

	return Directory_ERR_FS_TABLE_CORRUPTED ;
}

byte Directory_GetDirEntryInfo(DriveInfo* pDriveInfo, FileSystem_CWD* pCWD, const char* szFileName, unsigned* uiSectorNo, byte* bSectorPos, byte* bDirectoryBuffer)
{
	byte bStatus ;
	byte bIsPresent ;
	FileSystem_CWD CWD ;
	FileSystem_MountInfo* pFSMountInfo = &(pDriveInfo->FSMountInfo) ;

	if(String_Length(szFileName) == 0)
		return Directory_ERR_INVALID_NAME ;

	if(szFileName[0] == '/')
	{
		if(String_Compare(FS_ROOT_DIR, szFileName) == 0)
		{
			RETURN_IF_NOT(bStatus, Directory_RawRead(pDriveInfo, pFSMountInfo->FSpwd.uiSectorNo, pFSMountInfo->FSpwd.uiSectorNo + 1, bDirectoryBuffer), Directory_SUCCESS) ;
			*uiSectorNo = pFSMountInfo->FSpwd.uiSectorNo ;
			*bSectorPos = pFSMountInfo->FSpwd.bSectorEntryPosition ;
			return Directory_SUCCESS ;
		}

		CWD.pDirEntry = (FileSystem_DIR_Entry*)&(pFSMountInfo->FSpwd.DirEntry) ;
		CWD.uiSectorNo = pFSMountInfo->FSpwd.uiSectorNo ;
		CWD.bSectorEntryPosition = pFSMountInfo->FSpwd.bSectorEntryPosition ;
	}
	else
	{
		CWD.pDirEntry = pCWD->pDirEntry ;
		CWD.uiSectorNo = pCWD->uiSectorNo ;
		CWD.bSectorEntryPosition = pCWD->bSectorEntryPosition ;
	}

	int iListSize = 0, i ;

	StringDefTokenizer tokenizer ;

	String_Tokenize(szFileName, '/', &iListSize, tokenizer) ;

	for(i = 0; i < iListSize; i++)
	{
		RETURN_IF_NOT(bStatus, Directory_FindDirectory(pDriveInfo, &CWD, tokenizer.szToken[i], uiSectorNo, bSectorPos, &bIsPresent, bDirectoryBuffer), Directory_SUCCESS) ;

		if(bIsPresent == false)
		{
			return Directory_ERR_NOT_EXISTS ;
		}
		CWD.pDirEntry = ((FileSystem_DIR_Entry*)bDirectoryBuffer) + *bSectorPos ;
		CWD.uiSectorNo = *uiSectorNo ;
		CWD.bSectorEntryPosition = *bSectorPos ;
	}

	return Directory_SUCCESS ;
}

byte Directory_Change(const char* szFileName, int iDriveID, ProcessAddressSpace* processAddressSpace)
{
	byte bStatus ;
	unsigned uiSectorNo ;
	byte bSectorPos ;
	byte bDirectoryBuffer[512] ;

	GET_DRIVE_FOR_FS_OPS(iDriveID, Directory_FAILURE) ;
	if(pDriveInfo == NULL)
		return Directory_ERR_INVALID_DRIVE ;

	FileSystem_CWD CWD ;
	FileSystem_PresentWorkingDirectory* pPWD = &(processAddressSpace->processPWD) ;
	if(iDriveID == processAddressSpace->iDriveID)
	{
		CWD.pDirEntry = &pPWD->DirEntry ;
		CWD.uiSectorNo = pPWD->uiSectorNo ;
		CWD.bSectorEntryPosition = pPWD->bSectorEntryPosition ;
	}
	else
	{
		CWD.pDirEntry = &(pDriveInfo->FSMountInfo.FSpwd.DirEntry) ;
		CWD.uiSectorNo = pDriveInfo->FSMountInfo.FSpwd.uiSectorNo ;
		CWD.bSectorEntryPosition = pDriveInfo->FSMountInfo.FSpwd.bSectorEntryPosition ;
	}

	RETURN_IF_NOT(bStatus, Directory_GetDirEntryInfo(pDriveInfo, &CWD, szFileName, &uiSectorNo, &bSectorPos, bDirectoryBuffer), Directory_SUCCESS) ;

	FileSystem_DIR_Entry* dirFile = ((FileSystem_DIR_Entry*)bDirectoryBuffer) + bSectorPos ;

	if((dirFile->usAttribute & ATTR_TYPE_DIRECTORY) == 0)
		return Directory_ERR_IS_NOT_DIRECTORY ;

	processAddressSpace->iDriveID = iDriveID ;

	MemUtil_CopyMemory(MemUtil_GetDS(), (unsigned)(((FileSystem_DIR_Entry*)bDirectoryBuffer) + bSectorPos), MemUtil_GetDS(), (unsigned)&pPWD->DirEntry, sizeof(FileSystem_DIR_Entry)) ;
	pPWD->uiSectorNo = uiSectorNo ;
	pPWD->bSectorEntryPosition = bSectorPos ;
	
	unsigned uiSecNo ;
	byte bSecPos ;
	char szPWD[256] ;
	char szTempPwd[256] = "" ;

	if(String_Compare((const char*)dirFile->Name, FS_ROOT_DIR) == 0)
		String_Copy(szPWD, FS_ROOT_DIR) ;
	else
	{
		while(true)
		{	
			String_Copy(szPWD, FS_ROOT_DIR) ;

			String_CanCat(szPWD, (const char*)dirFile->Name) ;
			String_CanCat(szPWD, szTempPwd) ;

			uiSecNo = dirFile->uiParentSecID ;
			bSecPos = dirFile->bParentSectorPos ;

			RETURN_IF_NOT(bStatus, Directory_RawRead(pDriveInfo, uiSecNo, uiSecNo + 1, bDirectoryBuffer), Directory_SUCCESS) ;

			dirFile = ((FileSystem_DIR_Entry*)bDirectoryBuffer) + bSecPos ;

			if(String_Compare((const char*)dirFile->Name, FS_ROOT_DIR) == 0)
				break ;

			String_Copy(szTempPwd, szPWD) ;
		}
	}

	String_Copy(szTempPwd, szPWD) ;
	String_Copy(szPWD, pDriveInfo->drive.driveName) ;
	String_CanCat(szPWD, "@") ;
	String_CanCat(szPWD, szTempPwd) ;

	ProcessEnv_Set("PWD", szPWD) ;

	return Directory_SUCCESS ;
}

byte Directory_PresentWorkingDirectory(ProcessAddressSpace* processAddressSpace, char** uiReturnDirPathAddress)
{
	char* szPWD ;
	char* pAddress ;

	szPWD = ProcessEnv_Get("PWD") ;

	if(szPWD == NULL)
		return Directory_FAILURE ;

	if(processAddressSpace->bIsKernelProcess == true)
	{
		*uiReturnDirPathAddress = (char*)DMM_AllocateForKernel(String_Length(szPWD) + 1) ;
		pAddress = *uiReturnDirPathAddress ;
	}
	else
	{
		*uiReturnDirPathAddress = (char*)DMM_Allocate(processAddressSpace, String_Length(szPWD) + 1) ;
		pAddress = (char*)(((unsigned)*uiReturnDirPathAddress + PROCESS_BASE) - GLOBAL_DATA_SEGMENT_BASE) ;
	}

	if(*uiReturnDirPathAddress == NULL)
		return Directory_FAILURE ;

	String_Copy(pAddress, szPWD) ;
	return Directory_SUCCESS ;
}

byte Directory_GetDirEntry(const char* szFileName, ProcessAddressSpace* processAddressSpace, int iDriveID, FileSystem_DIR_Entry* pDirEntry)
{
	byte bStatus ;
	byte bDirectoryBuffer[512] ;

	GET_DRIVE_FOR_FS_OPS(iDriveID, Directory_FAILURE) ;
	FileSystem_CWD CWD ;
	if(processAddressSpace->iDriveID == iDriveID)
	{
		CWD.pDirEntry = &(processAddressSpace->processPWD.DirEntry) ;
		CWD.uiSectorNo = processAddressSpace->processPWD.uiSectorNo ;
		CWD.bSectorEntryPosition = processAddressSpace->processPWD.bSectorEntryPosition ;
	}
	else
	{
		CWD.pDirEntry = &(pDriveInfo->FSMountInfo.FSpwd.DirEntry) ;
		CWD.uiSectorNo = pDriveInfo->FSMountInfo.FSpwd.uiSectorNo ;
		CWD.bSectorEntryPosition = pDriveInfo->FSMountInfo.FSpwd.bSectorEntryPosition ;
	}

	FileSystem_DIR_Entry* dirFile ;

	unsigned uiSectorNo ;
	byte bSectorPos ;

	RETURN_IF_NOT(bStatus, Directory_GetDirEntryInfo(pDriveInfo, &CWD, szFileName, &uiSectorNo, &bSectorPos, bDirectoryBuffer), Directory_SUCCESS) ;

	dirFile = ((FileSystem_DIR_Entry*)bDirectoryBuffer) + bSectorPos ;

	if(dirFile->usAttribute & ATTR_DELETED_DIR)
		return Directory_ERR_NOT_EXISTS ;

	MemUtil_CopyMemory(MemUtil_GetDS(), (unsigned)dirFile, MemUtil_GetDS(), (unsigned)pDirEntry, sizeof(FileSystem_DIR_Entry)) ;

	return Directory_SUCCESS ;
}

byte Directory_FindFullDirPath(DriveInfo* pDriveInfo, const FileSystem_DIR_Entry* pDirEntry, char* szFullDirPath)
{
	byte bStatus ;
	byte bSectorBuffer[512] ;
	char temp[256] ;

	const FileSystem_DIR_Entry* pParseDirEntry = pDirEntry ;

	String_Copy(szFullDirPath, "") ;

	byte bFirst = true ;

	while(true)
	{
		if(String_Compare((const char*)pParseDirEntry->Name, FS_ROOT_DIR) == 0)
		{
			String_Copy(temp, szFullDirPath) ;
			String_Copy(szFullDirPath, FS_ROOT_DIR) ;
			String_CanCat(szFullDirPath, temp) ;
			
			return Directory_SUCCESS ;
		}
		else
		{
			String_Copy(temp, szFullDirPath) ;
			String_Copy(szFullDirPath, (const char*)pParseDirEntry->Name) ;
			
			if(!bFirst)
			{
				String_CanCat(szFullDirPath, FS_ROOT_DIR) ;
				String_CanCat(szFullDirPath, temp) ;
			}
			else
				bFirst = false ;
		}

		unsigned uiParSectorNo = pParseDirEntry->uiParentSecID ;
		byte bParSectorPos = pParseDirEntry->bParentSectorPos ;

		RETURN_IF_NOT(bStatus, Directory_RawRead(pDriveInfo, uiParSectorNo, uiParSectorNo + 1, bSectorBuffer), Directory_SUCCESS) ;
		
		pParseDirEntry = &((const FileSystem_DIR_Entry*)bSectorBuffer)[bParSectorPos] ;
	}

	return Directory_FAILURE ;
}

byte Directory_SyncPWD(ProcessAddressSpace* processAddressSpace)
{
	GET_DRIVE_FOR_FS_OPS(processAddressSpace->iDriveID, Directory_FAILURE) ;

	FileSystem_DIR_Entry* pDirEntry = &(processAddressSpace->processPWD.DirEntry) ;
	unsigned uiSectorNo = processAddressSpace->processPWD.uiSectorNo ;
	byte bSectorEntryPos = processAddressSpace->processPWD.bSectorEntryPosition ;

	byte bStatus ;
	byte bSectorBuffer[512] ;

	RETURN_IF_NOT(bStatus, Directory_RawRead(pDriveInfo, uiSectorNo, uiSectorNo + 1, bSectorBuffer), Directory_SUCCESS) ;

	FileSystem_DIR_Entry* pSrcDirEntry = &(((FileSystem_DIR_Entry*)bSectorBuffer)[bSectorEntryPos]) ;
	
	MemUtil_CopyMemory(MemUtil_GetDS(), (unsigned)pSrcDirEntry, MemUtil_GetDS(), (unsigned)pDirEntry, sizeof(FileSystem_DIR_Entry)) ;

	return Directory_SUCCESS ;
}

byte Directory_RawDirEntryRead(DriveInfo* pDriveInfo, unsigned uiSectorID, byte bSecPos, FileSystem_DIR_Entry* pDestDirEntry)
{
	byte bStatus ;
	byte bSectorBuffer[512] ;
	
	uiSectorID = FileSystem_GetRealSectorNumber(uiSectorID, pDriveInfo) ;

	RETURN_IF_NOT(bStatus, DeviceDrive_Read(pDriveInfo, uiSectorID, 1, bSectorBuffer), DeviceDrive_SUCCESS) ;

	FileSystem_DIR_Entry* pDirEntry = &((FileSystem_DIR_Entry*)bSectorBuffer)[bSecPos] ; 

	MemUtil_CopyMemory(MemUtil_GetDS(), (unsigned)pDirEntry, MemUtil_GetDS(), (unsigned)pDestDirEntry, sizeof(FileSystem_DIR_Entry)) ;

	return Directory_SUCCESS ;
}

void Directory_CopyDirEntry(FileSystem_DIR_Entry* pDest, FileSystem_DIR_Entry* pSrc)
{
	MemUtil_CopyMemory(MemUtil_GetDS(), (unsigned)pSrc, MemUtil_GetDS(), (unsigned)pDest, sizeof(FileSystem_DIR_Entry)) ;
}

