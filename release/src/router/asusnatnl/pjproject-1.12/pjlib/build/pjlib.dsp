# Microsoft Developer Studio Project File - Name="pjlib" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=pjlib - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "pjlib.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "pjlib.mak" CFG="pjlib - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "pjlib - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "pjlib - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "pjlib - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ".\output\pjlib-i386-win32-vc6-release"
# PROP BASE Intermediate_Dir ".\output\pjlib-i386-win32-vc6-release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir ".\output\pjlib-i386-win32-vc6-release"
# PROP Intermediate_Dir ".\output\pjlib-i386-win32-vc6-release"
# PROP Target_Dir ""
F90=df.exe
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MD /W4 /Zi /O2 /Ob2 /I "../include" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "PJ_WIN32" /D "PJ_M_I386" /FR /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"../lib/pjlib-i386-win32-vc6-release.lib"

!ELSEIF  "$(CFG)" == "pjlib - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir ".\output\pjlib-i386-win32-vc6-debug"
# PROP BASE Intermediate_Dir ".\output\pjlib-i386-win32-vc6-debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir ".\output\pjlib-i386-win32-vc6-debug"
# PROP Intermediate_Dir ".\output\pjlib-i386-win32-vc6-debug"
# PROP Target_Dir ""
F90=df.exe
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W4 /Gm /GX /ZI /Od /I "../include" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "PJ_WIN32" /D "PJ_M_I386" /FR /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"../lib/pjlib-i386-win32-vc6-debug.lib"

!ENDIF 

# Begin Target

# Name "pjlib - Win32 Release"
# Name "pjlib - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Group "Other Targets"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\src\pj\addr_resolv_linux_kernel.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=..\src\pj\guid_simple.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=..\src\pj\ioqueue_dummy.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=..\src\pj\ioqueue_epoll.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=..\src\pj\ip_helper_generic.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=..\src\pj\log_writer_printk.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=..\src\pj\os_core_linux_kernel.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=..\src\pj\os_core_unix.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=..\src\pj\os_error_linux_kernel.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=..\src\pj\os_error_unix.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=..\src\pj\os_time_linux_kernel.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=..\src\pj\os_timestamp_linux.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=..\src\pj\os_timestamp_linux_kernel.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=..\src\pj\pool_policy_kmalloc.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=..\src\pj\sock_linux_kernel.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=..\src\pj\symbols.c
# PROP Exclude_From_Build 1
# End Source File
# End Group
# Begin Source File

SOURCE=..\src\pj\activesock.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\addr_resolv_sock.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\array.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\config.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\ctype.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\errno.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\except.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\fifobuf.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\file_access_win32.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\file_io_ansi.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=..\src\pj\file_io_win32.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\guid.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\guid_win32.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\hash.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\ioqueue_common_abs.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=..\src\pj\ioqueue_common_abs.h
# End Source File
# Begin Source File

SOURCE=..\src\pj\ioqueue_select.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\ioqueue_winnt.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=..\src\pj\ip_helper_win32.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\list.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\lock.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\log.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\log_writer_stdout.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\os_core_win32.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\os_error_win32.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\os_time_win32.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\os_timestamp_common.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\os_timestamp_win32.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\pool.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\pool_buf.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\pool_caching.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\pool_dbg.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\pool_policy_malloc.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\rand.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\rbtree.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\sock_bsd.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\sock_common.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\sock_qos_bsd.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\sock_qos_common.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\sock_select.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\ssl_sock_common.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\ssl_sock_dump.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\ssl_sock_ossl.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\string.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\timer.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\types.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\unicode_win32.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Group "compat"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\include\pj\compat\assert.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\compat\cc_gcc.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\compat\cc_msvc.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\compat\ctype.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\compat\errno.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\compat\high_precision.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\compat\m_alpha.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\compat\m_i386.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\compat\m_m68k.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\compat\m_sparc.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\compat\malloc.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\compat\os_linux.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\compat\os_linux_kernel.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\compat\os_palmos.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\compat\os_sunos.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\compat\os_win32.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\compat\rand.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\compat\setjmp.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\compat\size_t.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\compat\socket.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\compat\stdarg.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\compat\stdfileio.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\compat\string.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\compat\time.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\compat\vsprintf.h
# End Source File
# End Group
# Begin Source File

SOURCE=..\include\pj\activesock.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\addr_resolv.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\array.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\assert.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\config.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\config_site.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\config_site_sample.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\ctype.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\doxygen.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\errno.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\except.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\fifobuf.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\file_access.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\file_io.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\guid.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\hash.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\ioqueue.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\ip_helper.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\list.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\lock.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\log.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\math.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\os.h
# End Source File
# Begin Source File

SOURCE=..\include\pjlib.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\pool.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\pool_alt.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\pool_buf.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\rand.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\rbtree.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\sock.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\sock_qos.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\sock_select.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\ssl_sock.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\string.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\timer.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\types.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\unicode.h
# End Source File
# End Group
# Begin Group "Inline Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\include\pj\list_i.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\pool_i.h
# End Source File
# Begin Source File

SOURCE=..\include\pj\string_i.h
# End Source File
# End Group
# End Target
# End Project
