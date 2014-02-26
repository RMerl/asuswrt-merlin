/*++

Module Name:

    ufsdapi.h

Abstract:

    This module is a common include file for
    linux vfs modules.

Author:

    Ahdrey Shedel

Revision History:

    27/12/2002 - Andrey Shedel - Created

--*/
#ifndef __UFSDAPI_INC__
#define __UFSDAPI_INC__



/*
 * Missing but useful declarations.
 */

#define ARGUMENT_PRESENT(p) (NULL != (p))

#ifndef _TYPEDEF_UINT64_
typedef unsigned long long  UINT64;
#define _TYPEDEF_UINT64_
#endif

#define MINUS_ONE_ULL 0xffffffffffffffffull

#ifndef SetFlag
  #define SetFlag(flags, single_flag) (flags |= (single_flag))
#endif

#ifndef ClearFlag
  #define ClearFlag(flags, single_flag) (flags &= ~(single_flag))
#endif

#ifndef FlagOn
  #define FlagOn(flags, single_flag) ( 0 != ((flags) & (single_flag)))
#endif

#ifndef IN
  #define IN
#endif

#ifndef OUT
  #define OUT
#endif

#ifndef OPTIONAL
  #define OPTIONAL
#endif

#ifndef NOTHING
  #define NOTHING {;}
#endif

typedef int
(*SEQ_PRINTF)(
    IN struct seq_file* m,
    IN const char * fmt,
    IN ...
    )
#if __GNUC__ >= 3
    __attribute__ ((format (printf, 2, 3)))
#endif
    ;



#ifndef min
#define min(x,y) ({ \
  const typeof(x) _x = (x); \
  const typeof(y) _y = (y); \
  (void) (&_x == &_y);    \
  _x < _y ? _x : _y; })

#define max(x,y) ({ \
  const typeof(x) _x = (x); \
  const typeof(y) _y = (y); \
  (void) (&_x == &_y);    \
  _x > _y ? _x : _y; })

#define min_t(type,x,y) \
  ({ type __x = (x); type __y = (y); __x < __y ? __x: __y; })
#define max_t(type,x,y) \
  ({ type __x = (x); type __y = (y); __x > __y ? __x: __y; })

#endif


#define _QUOTE1(name) #name
#define _QUOTE2(name) _QUOTE1(name)
#define QUOTED_UFSD_DEVICE  _QUOTE2(UFSD_DEVICE)



struct super_block;
struct buffer_head;
struct nls_table;
typedef struct super_block SB;

typedef struct mount_options {
    struct nls_table* nls[8];     // if NULL == nls[i] then this is a builtin utf8
    int               nls_count;  // always >= 1
    int               bias;       // bias = UTC - local time. Eastern time zone: +300, Paris,Berlin: -60, Moscow: -180
    unsigned int      clumpKb;    // clump size in Kb
    unsigned int      raKb;       // readahead size in Kb
    unsigned int      fs_uid;
    unsigned int      fs_gid;
    unsigned short    fs_fmask;
    unsigned short    fs_dmask;
    char fmask;           // fmask was set
    char dmask;           // dmask was set
    char uid;             // uid was set
    char gid;             // gid was set
    char showmeta;        // set = show meta files
    char showexec;        // set = only set x bit for com/exe/bat
    char sys_immutable;   // set = system files are immutable
    char nocase;          // Does this need case conversion? 0=need case conversion
    char noatime;         // do not update atime.
    char bestcompr;       // Use best compression instead of standard
    char sparse;          // Always create sparse files
    char force;           // Force to mount dirty volume
    char nohidden;        // Skip hidden files while enumerating
    char user_xattr;      // Extended user attributes
    char acl;             // POSIX Access Control Lists
    char chkcnv;          // Check string convertation UNICODE->MBCS->UNCIODE
    char usrquota;
    char grpquota;
    char sync;
    char nolazy;          // Lazy open
    char delalloc;        // Delay allocate
    char nojnl;           // Turn off journal
    char nobarrier;       // Turn off barrier
    char nobuf;           // Do not buffer BdRead/BdWrite
    char wb;              // write-backup mode

    // These members are used to return useful information from mount/remount
    char delim;           // Stream delimiter
    char journal;         // Journal status: 0 - absent, 1 - ok, 2 - replayed, 3 - need replay
    char ugm;             // Volume supports UGM
    char posixtime;       // filesystem uses posix time (HFS+)
    char utf8link;        // filesystem uses utf8 links (HFS+)
    char needencode;      // filesystem requires encode (exFAT)
    char ntfs;            // filesystem is ntfs
    char hfs;             // filesystem is hfs
    char exfat;           // filesystem is exfat
    char refs;            // filesystem is refs
} mount_options;

#define JOURNAL_STATUS_ABSENT       0
#define JOURNAL_STATUS_OK           1
#define JOURNAL_STATUS_REPLAYED     2
#define JOURNAL_STATUS_NEED_REPLAY  3
#define JOURNAL_STATUS_NEED_REPLAY_NATIVE 4

#ifdef __cplusplus
  extern "C"{
#endif

void*
UFSDAPI_CALL
UFSD_CacheCreate(
    IN const char*  Name,
    IN size_t       size
    );

void
UFSDAPI_CALL
UFSD_CacheDestroy(
    IN void*  Cache
    );

void*
UFSDAPI_CALL
UFSD_CacheAlloc(
    IN void*  Cache,
    IN int    bZero
    );

void
UFSDAPI_CALL
UFSD_CacheFree(
    IN void*  Cache,
    IN void*  p
    );

void*
UFSDAPI_CALL
UFSD_HeapAlloc(
    IN size_t Size
    );

void
UFSDAPI_CALL
UFSD_HeapFree(
    IN void* Pointer
    );

UINT64
UFSDAPI_CALL
UFSD_CurrentTime(
    IN int PosixTime
    );

const char*
UFSDAPI_CALL
UFSDAPI_LibraryVersion(
    OUT int* EndianError
    );

unsigned
UFSDAPI_CALL
UFSD_BdRead(
    IN  SB*     sb,
    IN  UINT64  Offset,
    IN  size_t  Bytes,
    OUT void*   Buffer
    );

unsigned
UFSDAPI_CALL
UFSD_BdWrite(
    IN SB*      sb,
    IN UINT64   Offset,
    IN size_t   Bytes,
    IN const void*   Buffer,
    IN size_t   Wait
    );

unsigned
UFSDAPI_CALL
UFSD_BdMap(
    IN  SB*     sb,
    IN  UINT64  Offset,
    IN  size_t  Bytes,
    IN  size_t  Flags,
    OUT struct buffer_head**  Bcb,
    OUT void**  Mem
    );

void
UFSDAPI_CALL
UFSD_BdUnMap(
#ifdef UFSD_DEBUG
    IN SB* sb,
#endif
    IN struct buffer_head* bh
    );

unsigned
UFSDAPI_CALL
UFSD_BdSetDirty(
    IN SB*      sb,
    IN struct buffer_head* bh,
    IN size_t   Wait
    );

void
UFSDAPI_CALL
UFSD_BdLockBuffer(
    IN struct buffer_head* bh
    );

void
UFSDAPI_CALL
UFSD_BdUnLockBuffer(
    IN struct buffer_head* bh
    );

int
UFSDAPI_CALL
UFSD_BdDiscard(
    IN SB*      sb,
    IN UINT64   Offset,
    IN size_t   Bytes
    );

int
UFSDAPI_CALL
UFSD_BdZero(
    IN struct super_block* sb,
    IN UINT64 Offset,
    IN size_t Bytes
    );

void
UFSDAPI_CALL
UFSD_BdSetBlockSize(
    IN SB*  sb,
    IN unsigned int BytesPerBlock
    );

int
UFSDAPI_CALL
UFSD_BdIsReadonly(
    IN SB* sb
    );

const char*
UFSDAPI_CALL
UFSD_BdGetName(
    IN SB* sb
    );

unsigned
UFSDAPI_CALL
UFSD_BdFlush(
    IN SB* sb,
    IN size_t Wait
    );



//
// Some forwards and helper API declarations.
//

typedef struct UFSD_VOLUME  UFSD_VOLUME;
typedef struct UFSD_FILE    UFSD_FILE;

typedef struct UfsdFileInfo{

  UINT64        asize;
  UINT64        size;
  UINT64        vsize;
  UINT64        atime;
  UINT64        ctime;
  UINT64        mtime;
  size_t        Id;   // used as inode number
  unsigned int  link_count;
  unsigned int  generation;

  unsigned int    udev; // if mode == S_IFBLK | mode == S_IFCHR
  unsigned int    uid;
  unsigned int    gid;
  unsigned short  mode;

  unsigned char is_xattr;
  unsigned char is_sparse;
  unsigned char is_compr;
  unsigned char is_encrypt;
  unsigned char is_dir;
  unsigned char is_system;
  unsigned char is_readonly;
  unsigned char is_link;
  unsigned char is_ugm; // true if uid/gid/mode is valid

} UfsdFileInfo;


typedef struct UfsdCreate{
  UFSD_FILE*      lnk;
  const void*     data;
  size_t          len;
  unsigned int    uid;
  unsigned int    gid;
  unsigned short  mode;
} UfsdCreate;


//
// Layout of file fragment
//
typedef struct mapinfo {
  UINT64    Vbo;        // Virtual byte offset
  UINT64    Lbo;        // Logical byte offset
  UINT64    Len;        // Length of map in bytes
  // NOTE: 'Alloc' really should mean how many bytes were allocated if (Flags & UFSD_MAP_LBO_NEW)
  // But for us it is suitable to return absolute number of bytes allocated for file
  UINT64    Alloc;      // Total allocated bytes for file
  size_t    Flags;      // Properties of fragment
} mapinfo;


///////////////////////////////////////////////////////////
// UFSD_OnSetDirty
//
// Callback function. Called when volume becomes dirty
///////////////////////////////////////////////////////////
void
UFSDAPI_CALL
UFSD_OnSetDirty(
    IN void* Arg
    );

///////////////////////////////////////////////////////////
// UFSD_TimeT2UfsdPosix
//
// Used to convert time_t to posix time
///////////////////////////////////////////////////////////
UINT64
UFSDAPI_CALL
UFSD_TimeT2UfsdPosix(
    IN const void* time
    );

///////////////////////////////////////////////////////////
// UFSD_TimeT2UfsdNt
//
// Used to convert time_t to Nt time
///////////////////////////////////////////////////////////
UINT64
UFSDAPI_CALL
UFSD_TimeT2UfsdNt(
    IN const void* time
    );

int
UFSDAPI_CALL
UFSDAPI_NamesEqual(
    IN UFSD_VOLUME* Volume,
    IN const char*  Name1,
    IN unsigned     Name1Len,
    IN const char*  Name2,
    IN unsigned     Name2Len
    );

unsigned int
UFSDAPI_CALL
UFSDAPI_NameHash(
    IN UFSD_VOLUME* Volume,
    IN const char*  Name,
    IN unsigned     NameLen
    );

// This function is called once on module initialization/deinitialization
int
UFSDAPI_CALL
UFSDAPI_main(
    IN size_t Flags
    );

int
UFSDAPI_CALL
UFSDAPI_VolumeMount(
    IN SB*                  sb,
    IN unsigned int         BytesPerSector,
    IN const UINT64*        BytesPerSb,
    IN OUT mount_options*   opts,
    OUT UFSD_VOLUME**       Volume,
    IN unsigned long        TotalRam,
    IN unsigned long        BytesPerPage
    );

int
UFSDAPI_CALL
UFSDAPI_VolumeReMount(
    IN UFSD_VOLUME*       Volume,
    IN int                ReadOnly,
    IN OUT mount_options* opts
    );

void
UFSDAPI_CALL
UFSDAPI_VolumeFree(
    IN UFSD_VOLUME* Volume
    );

int
UFSDAPI_CALL
UFSDAPI_IsVolumeDirty(
    IN UFSD_VOLUME* Volume
    );

int
UFSDAPI_CALL
UFSDAPI_VolumeFlush(
    IN UFSD_VOLUME* Volume,
    IN int          Wait
    );

void
UFSDAPI_CALL
UFSDAPI_SetFreeSpaceCallBack(
    IN  UFSD_VOLUME*  Volume,
    IN  void (*FreeSpaceCallBack)(
        IN size_t Lcn,
        IN size_t Len,
        IN int    AsUsed,
        IN void*  Arg
        ),
    IN void* Arg
    );


typedef struct UfsdVolumeInfo{

  UINT64        FileSizeMax;
  UINT64        Eod;

  unsigned int  FsSignature;
  unsigned int  TotalBlocks;
  unsigned int  BytesPerBlock;
  unsigned int  BytesPerSector;
  unsigned int  NameLength;
  unsigned int  Dirty;

  char          ReadOnly;
  char          NeedEncode;   // 1 if filesystem requires encode (exFAT only)
} UfsdVolumeInfo;


unsigned
UFSDAPI_CALL
UFSDAPI_QueryVolumeId(
    IN  UFSD_VOLUME*    Volume
    );

int
UFSDAPI_CALL
UFSDAPI_QueryVolumeInfo(
    IN  UFSD_VOLUME*    Volume,
    OUT UfsdVolumeInfo* Info OPTIONAL,
    OUT char*           Label OPTIONAL,
    IN  size_t          BytesPerLabel OPTIONAL,
    OUT UINT64*         FreeBlocks OPTIONAL
    );

int
UFSDAPI_CALL
UFSDAPI_TraceVolumeInfo(
    IN  UFSD_VOLUME*     Volume,
    OUT struct seq_file* m,
    IN  SEQ_PRINTF       SeqPrintf
    );

typedef struct UfsdVolumeTune{
  unsigned int DirAge;
  unsigned int JnlRam;
} UfsdVolumeTune;

int
UFSDAPI_CALL
UFSDAPI_QueryVolumeTune(
    IN  UFSD_VOLUME*    Volume,
    OUT UfsdVolumeTune* Tune
    );

int
UFSDAPI_CALL
UFSDAPI_SetVolumeTune(
    IN  UFSD_VOLUME*  Volume,
    IN  const UfsdVolumeTune* Tune
    );

int
UFSDAPI_CALL
UFSDAPI_IoControl(
    IN UFSD_VOLUME*   Volume,
    IN UFSD_FILE*     FileHandle,
    IN size_t         IoControlCode,
    IN const void*    InputBuffer OPTIONAL,
    IN size_t         InputBufferSize OPTIONAL,
    OUT void*         OutputBuffer OPTIONAL,
    IN size_t         OutputBufferSize OPTIONAL,
    OUT size_t*       BytesReturned OPTIONAL,
    OUT UfsdFileInfo* Info
    );

int
UFSDAPI_CALL
UFSDAPI_ReadLink(
    IN  UFSD_VOLUME*  Volume,
    IN  UFSD_FILE*    FileHandle,
    OUT char*         LinkBuffer,
    IN  size_t        MaxCharsInLink
    );

UINT64
UFSDAPI_CALL
UFSDAPI_GetDirSize(
    IN UFSD_FILE*   Dir
    );

int
UFSDAPI_CALL
UFSDAPI_FileOpen(
    IN UFSD_VOLUME*           Volume,
    IN UFSD_FILE*             ParentDir,
    IN const unsigned char*   Name,
    IN size_t                 NameLen,
    IN const UfsdCreate*      Create,
    OUT int*                  subdir_count,
    OUT UFSD_FILE**           File,
    OUT struct UfsdFileInfo*  Info
    );

int
UFSDAPI_CALL
UFSDAPI_FileClose(
    IN UFSD_VOLUME* Volume,
    IN UFSD_FILE*   File
    );

int
UFSDAPI_CALL
UFSDAPI_FileMap(
    IN  UFSD_FILE*  File,
    IN  UINT64      Vbo,
    IN  size_t      Bytes,
    IN  size_t      Flags,
    OUT mapinfo*    Map
    );

int
UFSDAPI_CALL
UFSDAPI_FileRead(
    IN UFSD_VOLUME* Volume,
    IN UFSD_FILE*   FileHandle,
    IN const char*  StreamName,
    IN int          StreamNameLen,
    IN UINT64       Offset,
    IN size_t       ByteCount,
    OUT void*       Buffer,
    OUT size_t*     BytesDone
    );

int
UFSDAPI_CALL
UFSDAPI_FileWrite(
    IN UFSD_VOLUME* Volume,
    IN UFSD_FILE*   FileHandle,
    IN const char*  StreamName,
    IN int          StreamNameLen,
    IN UINT64       Offset,
    IN size_t       ByteCount,
    IN const void*  Buffer,
    OUT size_t*     BytesDone
    );

int
UFSDAPI_CALL
UFSDAPI_FileAllocate(
    IN UFSD_FILE*   FileHandle,
    IN UINT64       From,
    IN UINT64       Len,
    IN int          KeepSize,
    OUT mapinfo*    Map
    );

int
UFSDAPI_CALL
UFSDAPI_FileSetSize(
    IN UFSD_FILE*     FileHandle,
    IN UINT64         size,
    OUT UINT64*       asize OPTIONAL
    );

int
UFSDAPI_CALL
UFSDAPI_FileMove(
    IN UFSD_VOLUME* Volume,
    IN UFSD_FILE*   OldDirHandle,
    IN UFSD_FILE*   NewDirHandle,
    IN UFSD_FILE*   OldFileHandle,
    IN const unsigned char* OldName,
    IN size_t       OldNameLen,
    IN const unsigned char* NewName,
    IN size_t       NewNameLen
    );

int
UFSDAPI_CALL
UFSDAPI_FileFlush(
    IN UFSD_VOLUME*           Volume,
    IN UFSD_FILE*             FileHandle,
    IN UINT64                 size,
    IN UINT64                 vsize,
    IN const void*            atime,
    IN const void*            mtime,
    IN const void*            ctime,
    IN const unsigned*        gid,
    IN const unsigned*        uid,
    IN const unsigned short*  mode
    );

int
UFSDAPI_CALL
UFSDAPI_IsDirEmpty(
    IN UFSD_VOLUME* Volume,
    IN UFSD_FILE*   Dir
    );

int
UFSDAPI_CALL
UFSDAPI_Unlink(
    IN UFSD_VOLUME* Volume,
    IN UFSD_FILE*   Dir,
    IN const unsigned char* Name,
    IN size_t NameLen,
    IN const unsigned char* sName,
    IN size_t sNameLen,
    IN UFSD_FILE*   Obj
    );

int
UFSDAPI_CALL
UFSDAPI_SetVolumeInfo(
    IN UFSD_VOLUME*   Volume,
    IN const char*    Label,
    IN size_t         LabelLen
    );

int
UFSDAPI_CALL
UFSDAPI_SetXAttr(
    IN UFSD_VOLUME*   Volume,
    IN UFSD_FILE*     File,
    IN const char*    Name,
    IN size_t         NameLen,
    IN const void*    Buffer,
    IN size_t         BytesPerBuffer,
    IN int            Create,
    IN int            Replace
    );

int
UFSDAPI_CALL
UFSDAPI_ListXAttr(
    IN  UFSD_VOLUME*  Volume,
    IN  UFSD_FILE*    File,
    OUT void*         Buffer,
    IN  size_t        BytesPerBuffer,
    OUT size_t*       Bytes
    );

int
UFSDAPI_CALL
UFSDAPI_GetXAttr(
    IN  UFSD_VOLUME*  Volume,
    IN  UFSD_FILE*    FileHandle,
    IN  const char*   Name,
    IN  size_t        NameLen,
    OUT void*         Buffer,
    IN  size_t        BytesPerBuffer,
    OUT size_t*       Len
    );

int
UFSDAPI_CALL
UFSDAPI_FileOpenById(
    IN  UFSD_VOLUME*  Volume,
    IN  size_t        Id,
    OUT UFSD_FILE**   File,
    OUT UfsdFileInfo* Info
    );

int
UFSDAPI_CALL
UFSDAPI_EncodeFH(
    IN UFSD_VOLUME*   Volume,
    IN  UFSD_FILE*    Fso,
    OUT void*         fh,
    IN OUT int*       MaxLen
    );

unsigned
UFSDAPI_CALL
UFSDAPI_DecodeFH(
    IN UFSD_VOLUME*   Volume,
    IN const void*    fh,
    IN int            fh_len,
    IN const int*     fh_type,
    IN int            parent,
    OUT UFSD_FILE**   File,
    OUT struct UfsdFileInfo* Info
    );

unsigned
UFSDAPI_CALL
UFSDAPI_FileGetName(
    IN  UFSD_VOLUME*    Volume,
    IN  UFSD_FILE*      File,
    IN  size_t          ParNo,
    OUT char*           Name,
    IN  size_t          MaxLen
    );

unsigned
UFSDAPI_CALL
UFSDAPI_FileGetParent(
    IN  UFSD_VOLUME*          Volume,
    IN  UFSD_FILE*            File,
    OUT UFSD_FILE**           Parent,
    OUT struct UfsdFileInfo*  Info
    );



typedef struct UFSD_SEARCH UFSD_SEARCH;

int
UFSDAPI_CALL
UFSDAPI_FindOpen(
    IN UFSD_VOLUME*   Volume,
    IN UFSD_FILE*     Dir,
    IN UINT64         Pos,
    OUT UFSD_SEARCH** Scan
    );

int
UFSDAPI_CALL
UFSDAPI_FindGet(
    IN UFSD_SEARCH*   Scan,
    IN OUT UINT64*    Pos,
    OUT char**        Name,
    OUT size_t*       NameLen,
    OUT int*          is_dir,
    OUT size_t*       ino
    );

void
UFSDAPI_CALL
UFSDAPI_FindClose(
    IN UFSD_SEARCH* Scan
    );


int
UFSDAPI_CALL
UFSDAPI_FileInfo(
    IN UFSD_VOLUME*   Volume,
    IN UFSD_FILE*     File,
    OUT struct UfsdFileInfo* Info
    );


//
//  API to Linux' NLS package.
//

///////////////////////////////////////////////////////////
// UFSD_BCSToUni
//
// Converts multibyte string to UNICODE string
// Returns the length of destination string in symbols
///////////////////////////////////////////////////////////
int
UFSDAPI_CALL
UFSD_BCSToUni(
    OUT unsigned short*       ws,
    IN  int                   max_out,
    IN  const unsigned char*  s,
    IN  int                   len,
    IN  struct nls_table*     nls
    );

///////////////////////////////////////////////////////////
// UFSD_UniToBCS
//
// Converts UNICODE string to multibyte
// Returns the length of destination string in chars
///////////////////////////////////////////////////////////
int
UFSDAPI_CALL
UFSD_UniToBCS(
    OUT unsigned char*        s,
    IN  int                   max_out,
    IN  const unsigned short* ws,
    IN  int                   len,
    IN  struct nls_table *    nls
    );

void
UFSDAPI_CALL
UFSDAPI_DumpMemory(
    IN const void*  Mem,
    IN size_t       nBytes
    );

#ifdef __cplusplus
  }  //extern "C"{
#endif


#if defined __STDC_VERSION__
#if __STDC_VERSION__ < 199901L
 # if __GNUC__ >= 2
 #  define __func__ __FUNCTION__
 # else
 #  define __func__ "<unknown>"
 # endif
#endif
#endif

//
// Duplicate defines from UFSD_SDK
// If someone changes it in UFSD_SDK
// we get warning here
//

// Possible flags for CFile::GetMap
#define UFSD_MAP_VBO_CREATE     0x01

// Possible flags for MapInfo::Flags
#define UFSD_MAP_LBO_NEW        0x01

// Possible options for SetWait
#define UFSD_RW_WAIT_SYNC       0x01
#define UFSD_RW_WAIT_BARRIER    0x02


// Possible options for Map
#define UFSD_RW_MAP_NO_READ      0x01

#ifndef __cplusplus

//===============================================
// Some of the UFSD errors
//===============================================


// No such file or directory
#define ERR_NOFILEEXISTS          0xa000010E

// Read I/O
#define ERR_READFILE              0xa000010C

// Write I/O
#define ERR_WRITEFILE             0xa000010D

// File exists
#define ERR_FILEEXISTS            0xa000010F

// Read-only
#define ERR_WPROTECT              0xa0000120

// Not enough free space
#define ERR_NOSPC                 0xa0000123

// This function is not implemented
#define ERR_NOTIMPLEMENTED        0xa0000124

// If the output buffer is too small to return any data
#define ERR_INSUFFICIENT_BUFFER   0xa0000125

// If the output buffer is too small to hold all of the data but can hold some entries
#define ERR_MORE_DATA             0xa0000126

#define ERR_DIRNOTEMPTY           0xa0000121

// Can't mount 'cause journal is not empty
// obsolete (will be removed)
#define ERR_NEED_REPLAY           0xa000012b

#define ERR_BADNAME_LEN           206

#ifndef Add2Ptr
  #define Add2Ptr(P,I)   ((unsigned char*)(P) + (I))
  #define PtrOffset(B,O) ((size_t)((size_t)(O) - (size_t)(B)))
#endif

#if defined CONFIG_LBD | defined CONFIG_LBDAF
  // sector_t - 64 bit value
  #define PSCT      "ll"
#else
  // sector_t - 32 bit value
  #define PSCT      "l"
#endif

// Add missing defines
#ifndef offsetof
  #define offsetof(type, member) ((long) &((type *) 0)->member)
#endif
#ifndef container_of
  #define container_of(ptr, type, member) ({                    \
          const typeof( ((type *)0)->member ) *__mptr = (ptr);  \
          (type *)( (char *)__mptr - offsetof(type,member) );})
#endif

  #define UFSD_IOC_SETVALID         _IOW('f', 91, unsigned long long)
  #define UFSD_IOC_SETCLUMP         _IOW('f', 101, unsigned int)
  #define UFSD_IOC_SETTIMES         _IOC(_IOC_WRITE,'f', 75, 4*sizeof(long long))
  #define UFSD_IOC_GETTIMES         _IOC(_IOC_READ, 'f', 73, 4*sizeof(long long))
  #define UFSD_IOC_SETATTR          _IOW('f', 99, unsigned int)
  #define UFSD_IOC_GETATTR          _IOR('f', 95, unsigned int)
  #define UFSD_IOC_GETMEMUSE        _IOC(_IOC_READ,'v', 20, 4*sizeof(size_t) )
  #define UFSD_IOC_GETVOLINFO       _IOC(_IOC_READ,'v', 21, 196 )

  #define UFSD_IOC32_GETMEMUSE      _IOC(_IOC_READ,'v', 20, 4*sizeof(int) )

#endif // #ifndef __cplusplus

#endif // #ifndef __UFSDAPI_INC__
