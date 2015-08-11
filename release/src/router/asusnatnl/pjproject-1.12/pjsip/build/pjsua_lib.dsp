# Microsoft Developer Studio Project File - Name="pjsua_lib" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=pjsua_lib - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "pjsua_lib.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "pjsua_lib.mak" CFG="pjsua_lib - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "pjsua_lib - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "pjsua_lib - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "pjsua_lib - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ".\output\pjsua-lib-i386-win32-vc6-release"
# PROP BASE Intermediate_Dir ".\output\pjsua-lib-i386-win32-vc6-release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir ".\output\pjsua-lib-i386-win32-vc6-release"
# PROP Intermediate_Dir ".\output\pjsua-lib-i386-win32-vc6-release"
# PROP Target_Dir ""
F90=df.exe
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MD /W4 /GX /Zi /O2 /I "../include" /I "../../pjmedia/include" /I "../../pjlib-util/include" /I "../../pjlib/include" /I "../../pjnath/include" /D "NDEBUG" /D PJ_WIN32=1 /D PJ_M_I386=1 /D "WIN32" /D "_MBCS" /D "_LIB" /FR /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\lib\pjsua-lib-i386-win32-vc6-release.lib"

!ELSEIF  "$(CFG)" == "pjsua_lib - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir ".\output\pjsua-lib-i386-win32-vc6-debug"
# PROP BASE Intermediate_Dir ".\output\pjsua-lib-i386-win32-vc6-debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir ".\output\pjsua-lib-i386-win32-vc6-debug"
# PROP Intermediate_Dir ".\output\pjsua-lib-i386-win32-vc6-debug"
# PROP Target_Dir ""
F90=df.exe
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W4 /Gm /GX /ZI /Od /I "../include" /I "../../pjmedia/include" /I "../../pjlib-util/include" /I "../../pjlib/include" /I "../../pjnath/include" /D "_DEBUG" /D PJ_WIN32=1 /D PJ_M_I386=1 /D "WIN32" /D "_MBCS" /D "_LIB" /FR /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\lib\pjsua-lib-i386-win32-vc6-debug.lib"

!ENDIF 

# Begin Target

# Name "pjsua_lib - Win32 Release"
# Name "pjsua_lib - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE="..\src\pjsua-lib\pjsua_acc.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjsua-lib\pjsua_call.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjsua-lib\pjsua_core.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjsua-lib\pjsua_im.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjsua-lib\pjsua_media.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjsua-lib\pjsua_pres.c"
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE="..\include\pjsua-lib\pjsua.h"
# End Source File
# Begin Source File

SOURCE="..\include\pjsua-lib\pjsua_internal.h"
# End Source File
# End Group
# End Target
# End Project
