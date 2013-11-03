/*++

Module Name:

    fs_conf.h

Abstract:

    This module is a forced include file
    for entire UFSD-based project.

Author:

    Ahdrey Shedel

Revision History:

    27/12/2002 - Andrey Shedel - Created

    Since 29/07/2005 - Alexander Mamaev

--*/



#ifndef __linux__
#define __linux__
#endif

//
// Tune the UFSD library.
//

//
// This define allows to exclude part of library not used in driver
//
#define UFSD_DRIVER_LINUX

//
// Exclude zero tail
//
//#define UFSD_SKIP_ZERO_TAIL

//
// Do not include extended I/O code
// Some of the utilities (e.g. fsutil) uses
// this I/O codes to get usefull information
//
//#define UFSD_NO_USE_IOCTL

#ifndef UFSD_NO_USE_IOCTL
//
// /proc/fs/ufsd/<volume> contains NTFS read/write statistics
//
//#define UFSD_NTFS_STAT
#endif

//
// Library never allocates a lot of memory
// This defines turns off checking of malloc
//
#define UFSD_MALLOC_CANT_FAIL

//
// Do not allow CRT calls to be used directly from library
// When UFSD_NO_CRT_USE is defined then UFSD uses CMemoryManager's
// functions Memset/Memcpy instead of intrinsic
//
//#define UFSD_NO_CRT_USE


//
// Exclude code that supports User/Group/Mode emulation in NTFS
//
//#define UFSD_DISABLE_UGM  "Turn off $UGM"

#if defined UFSD_EXFAT
  #ifndef UFSD_RW_MAP_EXFAT
    #warning "UFSD_EXFAT (U86) requires UFSD_RW_MAP_EXFAT to operate with 32M clusters"
    #define UFSD_RW_MAP_EXFAT
  #endif
#endif

//
// Turn on experimental feature
//
//#define UFSD_RW_MAP_EXFAT   "Turn on mapping"
//#define UFSD_RW_MAP_HFS     "Turn on mapping"
//#if defined UFSD_RW_MAP_EXFAT | defined UFSD_RW_MAP_HFS
  #define UFSD_RW_MAP
//#endif

//
// Activate this define to implement super_operations::write_supe
//
//#define UFSD_WRITE_SUPER

//
// Activate this define to check media every write operation
//
//#define UFSD_CHECK_BDI

//
// NTFS uses 64 bit cluster (Win7.x64 still uses 32bit clusters)
//
//#define UFSD_NTFS_64BIT_CLUSTER

//
// Bitmap uses a saved fragments to speed up look for free space
//
// #define USE_ADVANCED_WND_BITMAP

//
// This driver ignores short names. Speed up directory enumeration
//
#define UFSD_IGNORE_SHORTNAMES

//
// Exclude annoying messages while tests
//
//#define UFSD_TRACE_SILENT

//
// Do not load all bitmap at mount
//
//#define UFSD_DELAY_BITMAP_LOAD

//
// Do not dirty volume on mount (rw)
//
//#define UFSD_DIRTY_OFF

//
// Do smart dirty
//
//#define UFSD_SMART_DIRTY

#if defined UFSD_SMART_DIRTY
  #ifndef UFSD_DIRTY_OFF
    #error "UFSD_SMART_DIRTY requires UFSD_DIRTY_OFF"
  #endif

  #ifndef UFSD_WRITE_SUPER
    #define UFSD_WRITE_SUPER
  #endif
#endif

//
// Force to activate trace
//
#define UFSD_TRACE

//#define UFSD_SMART_TRACE

#if !defined UFSD_DEBUG && !defined NDEBUG
  #define UFSD_DEBUG 1
#endif

#if defined UFSD_DEBUG && !defined UFSD_TRACE
  #define UFSD_TRACE
#endif

//#define UFSD_NTFS_JNL                 // Include NTFS journal
//#define UFSD_NTFS_LOGFILE             // Include NTFS native journal replaying


#ifdef __cplusplus
  #define EXTERN_C extern "C"
#else
  #define EXTERN_C extern
#endif

#if defined(i386)
  #define UFSDAPI_CALL __attribute__((stdcall)) __attribute__((regparm(0)))
  #define UFSDAPI_CALLv __attribute__((cdecl)) __attribute__((regparm(0)))
#else
  #define UFSDAPI_CALL
  #define UFSDAPI_CALLv
#endif


//
// Trace message in system log
//
EXTERN_C UFSDAPI_CALLv int UFSD_printk( const char* fmt, ... ) __attribute__ ((format (printf, 1, 2)));

#define UFSDTracek(a) UFSD_printk a


#ifdef UFSD_TRACE

  //
  // _UFSDTrace is used to trace messages from UFSD
  // UFSDError is called when error occurs
  //

  EXTERN_C UFSDAPI_CALL void UFSDError( int Err, const char* FileName, int Line );
  EXTERN_C UFSDAPI_CALLv void _UFSDTrace( const char* fmt, ... ) __attribute__ ((format (printf, 1, 2)));

  extern char ufsd_trace_file[128];
  extern char ufsd_trace_level[16];
  extern unsigned long UFSD_TraceLevel;
  extern unsigned long UFSD_CycleMB;

  #define UFSD_TRACE_NTFS_RW      // trace read/write operation (Used by UFSD_SDK).
  #define UFSD_TRACE_NTFS_DIR     // trace directory enumeration (Used by UFSD_SDK).
  #define UFSD_NTFS_EXTRA_LOG
  #define UFSD_TRACE_NTFS_JOURNAL
  #define UFSD_TRACE_HFS_RW
  #define UFSD_TRACE_HFS_DIR
  #define UFSD_TRACE_HFS_JOURNAL
  #define UFSD_TRACE_FAT_DIR
  #define UFSD_TRACE_EXFAT_RW
  #define UFSD_TRACE_EXFAT_DIR
  #define UFSD_EXFAT_EXTRA_LOG
  #define UFSD_TRACE_REFS_DIR
  #define UFSD_TRACE_REFS_RW

  #ifdef UFSD_DEBUG
    #define UFSD_NTFS_LOG
    #define UFSD_FAT_LOG
    #define UFSD_EXFAT_LOG
    #define UFSD_HFS_LOG
    #define UFSD_PROFILE          // Profiler puts results into trace
  #endif

  #define UFSD_LEVEL_ALWAYS         0x00000000
  #define UFSD_LEVEL_ERROR          0x00000001
  #define UFSD_LEVEL_DEBUG_HOOKS    0x00000002
  #define UFSD_LEVEL_UFSD           0x00000010
  #define UFSD_LEVEL_UFSDAPI        0x00000080
  #define UFSD_LEVEL_VFS            0x00002000
  #define UFSD_LEVEL_VFS_WBWE       0x00004000
  #define UFSD_LEVEL_VFS_GETBLK     0x00008000
  // highest one contains VERY expensive log.
  #define UFSD_LEVEL_PAGE_BH        0x10000000
  #define UFSD_LEVEL_IO             0x20000000
  #define UFSD_LEVEL_SEMA           0x40000000
  #define UFSD_LEVEL_MEMMNGR        0x80000000

  // "all"
  #define UFSD_LEVEL_STR_ALL        ~(UFSD_LEVEL_VFS_WBWE|UFSD_LEVEL_MEMMNGR|UFSD_LEVEL_IO|UFSD_LEVEL_UFSDAPI)
  // "vfs"
  #define UFSD_LEVEL_STR_VFS        (UFSD_LEVEL_SEMA|UFSD_LEVEL_PAGE_BH|UFSD_LEVEL_VFS|UFSD_LEVEL_VFS_GETBLK|UFSD_LEVEL_ERROR)
  // "lib"
  #define UFSD_LEVEL_STR_LIB        (UFSD_LEVEL_UFSD|UFSD_LEVEL_ERROR)
  // "mid"
  #define UFSD_LEVEL_STR_MID        (UFSD_LEVEL_VFS|UFSD_LEVEL_UFSD|UFSD_LEVEL_ERROR)

  #define UFSD_LEVEL_DEFAULT        0x0000000f

  EXTERN_C UFSDAPI_CALL void UFSD_TraceInc( int Indent );

  #define UFSDTrace(a)                          \
    do {                                        \
      if ( UFSD_TraceLevel & UFSD_LEVEL_UFSD )  \
        _UFSDTrace a;                           \
    } while((void)0,0)

  #define DebugTrace(INDENT,LEVEL,X)                        \
    do {                                                    \
      if ( 0 != (LEVEL) && !( UFSD_TraceLevel & (LEVEL) ) ) \
        break;                                              \
      if ( (INDENT) < 0 )                                   \
        UFSD_TraceInc( (INDENT) );                          \
      _UFSDTrace X;                                         \
      if ( (INDENT) > 0 )                                   \
        UFSD_TraceInc( (INDENT) );                          \
    } while((void)0,0)

  #define TRACE_ONLY(e) e

#else

  #define UFSDTrace(a) do{}while((void)0,0)
  #define DebugTrace(i, l, x) do{}while((void)0,0)
  #define TRACE_ONLY(e)

#endif /* UFSD_TRACE */


#define UFSD_ERROR_DEFINED // UFSDError is already declared
#define _VERSIONS_H_       // Exclude configuration part of file versions.h

#ifdef UFSD_DEBUG
  // Usefull to debug some cases
  EXTERN_C UFSDAPI_CALL void UFSD_DumpStack( void );
#else
  #define UFSD_DumpStack()
#endif


#ifdef UFSD_DEBUG

  #define ASSERT(cond) {                                           \
    if (!(cond)) {                                                 \
      _UFSDTrace( "***** assertion failed at "               \
              __FILE__", %u: '%s'\n", __LINE__ , #cond);           \
    }                                                              \
  }

  #define assert(cond) {                                           \
    if (!(cond)) {                                                 \
      _UFSDTrace( "***** assertion failed at "               \
              __FILE__", %u: '%s'\n", __LINE__ , #cond);           \
    }                                                              \
  }

  #define DEBUG_ONLY(e) e
  #define VERIFY(cond) ASSERT(cond)

#else

  #define ASSERT(cond)
  #define DEBUG_ONLY(e)
  #define VERIFY(cond) {(void)(cond);}

#endif /* UFSD_DEBUG */


#define PIN_BUFFER          ((const void*)(size_t)0)
#define UNPIN_BUFFER        ((const void*)(size_t)1)
#define MINUS_ONE_BUFFER    ((const void*)(size_t)2)

#ifndef Add2Ptr
  #define Add2Ptr(P,I)   ((unsigned char*)(P) + (I))
  #define PtrOffset(B,O) ((size_t)((size_t)(O) - (size_t)(B)))
#endif

#define C_ASSERT(e) typedef char _C_ASSERT_[(e)?1:-1]

#ifndef UNREFERENCED_PARAMETER
  #define UNREFERENCED_PARAMETER(P)         {(void)(P);}
#endif

