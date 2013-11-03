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


#ifdef UFSD_TRACE
  void CloseTrace( void );
  int IsZero( const char*  data, size_t bytes );
#else
  #define CloseTrace()
#endif

#ifdef UFSD_DEBUG
 void CloseTrace( void );
 void SetTrace( const char* TraceFile );
#else
  #ifndef UFSD_TRACE
    #define CloseTrace()  NOTHING
    #define SetTrace(x)   NOTHING
  #endif
#endif

#ifdef UFSD_DEBUG
  #ifndef IsZero
    int IsZero( const char*  data, size_t bytes );
  #endif
#endif
