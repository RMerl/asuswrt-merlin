/*++


Module Name:

    vfsdebug.h

Abstract:

    This module defines debug services for VFS

Author:

    Ahdrey Shedel

Revision History:

    27/12/2002 - Andrey Shedel - Created

--*/



#ifdef UFSD_DEBUG

#define DEBUG_TRACE_ALWAYS         0x00000000
#define DEBUG_TRACE_ERROR          0x00000001
#define DEBUG_TRACE_DEBUG_HOOKS    0x00000002
#define DEBUG_TRACE_UFSD           0x00000010
#define DEBUG_TRACE_UFSDAPI        0x00000080
#define DEBUG_TRACE_VFS            0x00002000
#define DEBUG_TRACE_VFS_WBWE       0x00004000
// highest one contains VERY expensive log.
#define DEBUG_TRACE_PAGE_BH        0x10000000
#define DEBUG_TRACE_IO             0x20000000
#define DEBUG_TRACE_SEMA           0x40000000
#define DEBUG_TRACE_MEMMNGR        0x80000000

#define DEBUG_TRACE_DEFAULT        0x0000000f


#define ASSERT(cond) {                                      \
  if (!(cond)) {                                            \
    UFSD_DebugPrintf("***** assertion failed: %s at %s: %u\n", #cond, __FILE__, (unsigned)__LINE__);  \
  }                                                         \
}

#define VERIFY(cond) ASSERT(cond)
#define DEBUG_ONLY(e) e


extern long UFSD_DebugTraceLevel;

EXTERN_C UFSDAPI_CALL void UFSD_DebugInc( int Indent );
EXTERN_C UFSDAPI_CALLv void UFSD_DebugPrintf(const char* fmt, ...)
  __attribute__ ((format (printf, 1, 2)));

#define DebugTrace(INDENT,LEVEL,X) {                              \
  if (((LEVEL) == 0) || (UFSD_DebugTraceLevel & (LEVEL))) {       \
    if ((INDENT) < 0)                                             \
      UFSD_DebugInc( (INDENT) );                                  \
    UFSD_DebugPrintf X;                                           \
    if ((INDENT) > 0)                                             \
        UFSD_DebugInc( (INDENT) );                                \
  }                                                               \
}


EXTERN_C UFSDAPI_CALL void InitTrace( void );
EXTERN_C UFSDAPI_CALL void CloseTrace( void );
EXTERN_C UFSDAPI_CALL void SetTrace( const char* TraceFile );
EXTERN_C UFSDAPI_CALL void SetCycle( int CycleBytes );

#else

#define ASSERT(cond) NOTHING;
#define VERIFY(cond) {(void)(cond);}
#define DEBUG_ONLY(e)
#define UFSD_DumpAep(aep, Mask) NOTHING;
#define DebugTrace(i, l, x) NOTHING;

#define InitTrace()   NOTHING
#define CloseTrace()  NOTHING
#define SetTrace(x)   NOTHING
#define SetCycle(x)   NOTHING

#endif //UFSD_DEBUG

#define C_ASSERT(e) typedef char _C_ASSERT_[(e)?1:-1]


#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(P)         {(void)(P);}
#endif

