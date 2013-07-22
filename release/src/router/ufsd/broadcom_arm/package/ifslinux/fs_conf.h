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

#if !defined UFSD_DEBUG && !defined NDEBUG
  #define UFSD_DEBUG 1
#endif

#if defined UFSD_DEBUG && !defined UFSD_TRACE
#define UFSD_TRACE
#endif

#if defined UFSD_DEBUG && defined UFSD_TRACE
  #define UFSD_TRACE_NTFS_RW      // trace read/write operation (Used by UFSD_SDK).
  #define UFSD_TRACE_NTFS_DIR     // trace directory enumeration (Used by UFSD_SDK).
  #define UFSD_NTFS_EXTRA_LOG
  #define UFSD_TRACE_NTFS_JOURNAL
  #define UFSD_TRACE_HFS_RW
  #define UFSD_TRACE_HFS_DIR
  #define UFSD_TRACE_HFS_JOURNAL
  #define UFSD_TRACE_FAT_DIR
  #define UFSD_TRACE_EXFAT_RW
//  #define UFSD_TRACE_EXFAT_DIR
  #define UFSD_EXFAT_EXTRA_LOG
  #define UFSD_PROFILE          // Profiler puts results into trace
#endif

#if defined UFSD_TRACE
  #define UFSD_NTFS_LOG
  #define UFSD_FAT_LOG
  #define UFSD_EXFAT_LOG
  #define UFSD_HFS_LOG
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
// _UFSDTrace is used to trace messages from UFSD
// UFSDError is called when error occurs
//
EXTERN_C UFSDAPI_CALL void UFSDError( int Err, const char* FileName, int Line );
EXTERN_C UFSDAPI_CALLv void _UFSDTrace( const char* fmt, ... ) __attribute__ ((format (printf, 1, 2)));

#if defined UFSD_TRACE
  #define UFSDTrace(a) _UFSDTrace a
#else
  #define UFSDTrace(a) do{}while((void)0,0)
#endif

#define UFSD_ERROR_DEFINED // UFSDError is already declared
#define _VERSIONS_H_       // Exclude configuration part of file versions.h


// Trace messages even in release version
EXTERN_C UFSDAPI_CALLv int UFSD_printk( const char* fmt, ... ) __attribute__ ((format (printf, 1, 2)));

#define UFSDTracek(a) UFSD_printk a


#ifdef UFSD_DEBUG
// Usefull to debug some cases
EXTERN_C UFSDAPI_CALL void UFSD_DumpStack( void );
#else
#define UFSD_DumpStack()
#endif


#ifndef NDEBUG

EXTERN_C UFSDAPI_CALLv void UFSD_DebugPrintf( const char* fmt, ...)  __attribute__ ((format (printf, 1, 2)));

#define assert(cond) {                                           \
  if (!(cond)) {                                                 \
    UFSD_DebugPrintf( "***** assertion failed at "               \
            __FILE__", %u: '%s'\n", __LINE__ , #cond);           \
  }                                                              \
}
#endif


#define PIN_BUFFER          ((const void*)(size_t)0)
#define UNPIN_BUFFER        ((const void*)(size_t)1)
#define MINUS_ONE_BUFFER    ((const void*)(size_t)2)

#ifndef Add2Ptr
  #define Add2Ptr(P,I)   ((unsigned char*)(P) + (I))
  #define PtrOffset(B,O) ((size_t)((size_t)(O) - (size_t)(B)))
#endif
