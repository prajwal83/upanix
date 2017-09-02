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
#ifndef _FileSystem_H_
#define _FileSystem_H_

#include <Global.h>
#include <queue.h>
#include <vector.h>

#define MEDIA_REMOVABLE	0xF0
#define MEDIA_FIXED		0xF8

#define EOC		0x0FFFFFFF
#define EOC_B	0xFF
#define DIR_ENTRIES_PER_SECTOR 7

#define FS_ROOT_DIR "/"

#define ENTRIES_PER_TABLE_SECTOR	(128)

class DiskDrive;

class SectorBlockEntry
{
public:
  uint32_t* SectorBlock() { return _sectorBlock; }
  const uint32_t* SectorBlock() const { return _sectorBlock; }
  const uint32_t BlockId() const { return _blockId; }
  const uint32_t ReadCount() const { return _readCount; }
  const uint32_t WriteCount() const { return _writeCount; }

  void Load(DiskDrive& diskDrive, uint32_t sectortId);
  uint32_t Read(uint32_t sectorId);
  void Write(uint32_t sectorId, uint32_t value);

private:
  uint32_t _sectorBlock[ENTRIES_PER_TABLE_SECTOR];
  uint32_t _blockId;
  uint32_t _readCount;
  uint32_t _writeCount;
} PACKED;

class FileSystem
{
  public:
    FileSystem(DiskDrive& diskDrive) : _diskDrive(diskDrive), _freePoolQueue(nullptr)
    {
    }
    ~FileSystem()
    {
      delete _freePoolQueue;
    }

    uint64_t TotalSize() const { return _fsBootBlock.BPB_FSTableSize * ENTRIES_PER_TABLE_SECTOR * 512; }
    uint64_t UsedSize() const { return _fsBootBlock.uiUsedSectors * 512; }

    void Format();
    void AllocateFreePoolQueue(uint32_t size);
    void UnallocateFreePoolQueue();
    void ReadFSBootBlock();
    void WriteFSBootBlock();
    void LoadFreeSectors();
    void FlushTableCache(int iFlushSize);
    void AddToFreePoolCache(uint32_t sectorId) { _freePoolQueue->push_back(sectorId); }
    void AddToTableCache(unsigned uiSectorEntry);
    SectorBlockEntry* GetSectorEntryFromCache(unsigned uiSectorEntry);
    uint32_t AllocateSector();

    uint32_t GetTableSectorId(uint32_t uiSectorID) const;
    uint32_t GetRealSectorNumber(uint32_t uiSectorID) const;
    uint32_t GetSectorEntryValue(const unsigned uiSectorID);
    void SetSectorEntryValue(const unsigned uiSectorID, unsigned uiSectorEntryValue);

    void DisplayCache();

public:
    class Node
    {
    public:
      void Init(char* szDirName, unsigned short usDirAttribute, int iUserID, unsigned uiParentSecNo, byte bParentSecPos);
      void InitAsRoot(uint32_t parentSectorId);
      upan::string FullPath(DiskDrive& diskDrive);

      byte            Name[33] ;
      struct timeval  CreatedTime ;
      struct timeval  AccessedTime ;
      struct timeval  ModifiedTime ;
      byte            bParentSectorPos ;
      unsigned short  usAttribute ;
      unsigned        uiSize ;
      unsigned        uiStartSectorID ;
      unsigned        uiParentSecID ;
      int             iUserID ;
    } PACKED;


    typedef struct
    {
      Node DirEntry ;
      unsigned uiSectorNo ;
      byte bSectorEntryPosition ;
    } PACKED PresentWorkingDirectory ;

    typedef struct
    {
      Node* pDirEntry ;
      unsigned uiSectorNo ;
      byte bSectorEntryPosition ;
    } PACKED CWD ;

    //Ouput
    PresentWorkingDirectory FSpwd;

private:
    struct BootBlock
    {
      byte			BPB_jmpBoot[3] ;

      byte			BPB_Media ;
      unsigned short	BPB_SecPerTrk ;
      unsigned short	BPB_NumHeads ;

      unsigned short	BPB_BytesPerSec ;
      unsigned		BPB_TotSec32 ;
      unsigned		BPB_HiddSec ;

      unsigned short	BPB_RsvdSecCnt ;
      unsigned		BPB_FSTableSize ;

      unsigned short	BPB_ExtFlags ;
      unsigned short	BPB_FSVer ;
      unsigned short	BPB_FSInfo ;

      byte			BPB_BootSig ;
      unsigned		BPB_VolID ;
      byte			BPB_VolLab[11 + 1] ;

      unsigned		uiUsedSectors ;
    } PACKED;

private:
    void InitBootBlock(BootBlock&);
    void UpdateUsedSectors(unsigned uiSectorEntryValue);

    DiskDrive& _diskDrive;
    BootBlock _fsBootBlock;
    upan::queue<unsigned>* _freePoolQueue;
    upan::vector<SectorBlockEntry> _fsTableCache;
};

typedef struct
{
  int 	    st_dev;     /* ID of device containing file */
  int     	st_ino;     /* inode number */
  unsigned short    	st_mode;    /* protection */
  int   		st_nlink;   /* number of hard links */
  int     	st_uid;     /* user ID of owner */
  int     	st_gid;     /* group ID of owner */
  int     	st_rdev;    /* device ID (if special file) */
  unsigned    st_size;    /* total size, in bytes */
  unsigned	st_blksize; /* blocksize for filesystem I/O */
  unsigned  	st_blocks;  /* number of blocks allocated */

  struct timeval    	st_atime;   /* time of last access */
  struct timeval    	st_mtime;   /* time of last modification */
  struct timeval   	st_ctime;   /* time of last status change */
} FileSystem_FileStat ;

uint32_t FileSystem_DeAllocateSector(DiskDrive* pDiskDrive, unsigned uiCurrentSectorID) ;
unsigned FileSystem_GetSizeForTableCache(unsigned uiNoOfSectorsInTableCache) ;

#endif

