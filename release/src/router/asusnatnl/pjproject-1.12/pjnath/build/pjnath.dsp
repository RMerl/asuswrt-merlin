# Microsoft Developer Studio Project File - Name="pjnath" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=pjnath - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "pjnath.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "pjnath.mak" CFG="pjnath - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "pjnath - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "pjnath - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "pjnath - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "./output/pjnath-i386-win32-vc6-release"
# PROP BASE Intermediate_Dir "./output/pjnath-i386-win32-vc6-release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "./output/pjnath-i386-win32-vc6-release"
# PROP Intermediate_Dir "./output/pjnath-i386-win32-vc6-release"
# PROP Target_Dir ""
F90=df.exe
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MD /W4 /GX /O1 /Ob2 /I "../include" /I "../../pjlib/include" /I "../../pjlib-util/include" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D PJ_WIN32=1 /D PJ_M_I386=1 /FR /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"../lib/pjnath-i386-win32-vc6-release.lib"

!ELSEIF  "$(CFG)" == "pjnath - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "./output/pjnath-i386-win32-vc6-debug"
# PROP BASE Intermediate_Dir "./output/pjnath-i386-win32-vc6-debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "./output/pjnath-i386-win32-vc6-debug"
# PROP Intermediate_Dir "./output/pjnath-i386-win32-vc6-debug"
# PROP Target_Dir ""
F90=df.exe
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W4 /Gm /GX /ZI /Od /I "../include" /I "../../pjlib/include" /I "../../pjlib-util/include" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D PJ_WIN32=1 /D PJ_M_I386=1 /FR /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"../lib/pjnath-i386-win32-vc6-debug.lib"

!ENDIF 

# Begin Target

# Name "pjnath - Win32 Release"
# Name "pjnath - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\src\pjnath\errno.c
# End Source File
# Begin Source File

SOURCE=..\src\pjnath\ice_session.c
# End Source File
# Begin Source File

SOURCE=..\src\pjnath\ice_strans.c
# End Source File
# Begin Source File

SOURCE=..\src\pjnath\nat_detect.c
# End Source File
# Begin Source File

SOURCE=..\src\pjnath\stun_auth.c
# End Source File
# Begin Source File

SOURCE=..\src\pjnath\stun_msg.c
# End Source File
# Begin Source File

SOURCE=..\src\pjnath\stun_msg_dump.c
# End Source File
# Begin Source File

SOURCE=..\src\pjnath\stun_session.c
# End Source File
# Begin Source File

SOURCE=..\src\pjnath\stun_sock.c
# End Source File
# Begin Source File

SOURCE=..\src\pjnath\stun_transaction.c
# End Source File
# Begin Source File

SOURCE=..\src\pjnath\turn_session.c
# End Source File
# Begin Source File

SOURCE=..\src\pjnath\turn_sock.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\include\pjnath\config.h
# End Source File
# Begin Source File

SOURCE=..\include\pjnath\errno.h
# End Source File
# Begin Source File

SOURCE=..\include\pjnath\ice_session.h
# End Source File
# Begin Source File

SOURCE=..\include\pjnath\ice_strans.h
# End Source File
# Begin Source File

SOURCE=..\include\pjnath\nat_detect.h
# End Source File
# Begin Source File

SOURCE=..\include\pjnath.h
# End Source File
# Begin Source File

SOURCE=..\include\pjnath\stun_auth.h
# End Source File
# Begin Source File

SOURCE=..\include\pjnath\stun_config.h
# End Source File
# Begin Source File

SOURCE=..\include\pjnath\stun_msg.h
# End Source File
# Begin Source File

SOURCE=..\include\pjnath\stun_session.h
# End Source File
# Begin Source File

SOURCE=..\include\pjnath\stun_sock.h
# End Source File
# Begin Source File

SOURCE=..\include\pjnath\stun_transaction.h
# End Source File
# Begin Source File

SOURCE=..\include\pjnath\turn_session.h
# End Source File
# Begin Source File

SOURCE=..\include\pjnath\turn_sock.h
# End Source File
# Begin Source File

SOURCE=..\include\pjnath\types.h
# End Source File
# End Group
# Begin Group "Doxygen Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\docs\doc_ice.h
# End Source File
# Begin Source File

SOURCE=..\docs\doc_mainpage.h
# End Source File
# Begin Source File

SOURCE=..\docs\doc_nat.h
# End Source File
# Begin Source File

SOURCE=..\docs\doc_samples.h
# End Source File
# Begin Source File

SOURCE=..\docs\doc_stun.h
# End Source File
# Begin Source File

SOURCE=..\docs\doc_turn.h
# End Source File
# End Group
# End Target
# End Project
