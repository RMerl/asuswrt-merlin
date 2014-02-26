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
