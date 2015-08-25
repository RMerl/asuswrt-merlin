# Microsoft Developer Studio Project File - Name="pjmedia_audiodev" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=pjmedia_audiodev - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "pjmedia_audiodev.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "pjmedia_audiodev.mak" CFG="pjmedia_audiodev - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "pjmedia_audiodev - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "pjmedia_audiodev - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "pjmedia_audiodev - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ".\output\pjmedia-audiodev-i386-win32-vc6-release"
# PROP BASE Intermediate_Dir ".\output\pjmedia-audiodev-i386-win32-vc6-release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir ".\output\pjmedia-audiodev-i386-win32-vc6-release"
# PROP Intermediate_Dir ".\output\pjmedia-audiodev-i386-win32-vc6-release"
# PROP Target_Dir ""
F90=df.exe
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MD /W4 /GX /Zi /O2 /I "../include" /I "../../pjlib/include" /I "../../pjlib-util/include" /I "../../pjnath/include" /I "../../third_party/portaudio/include" /I "../../third_party/speex/include" /I "../../third_party/build/srtp" /I "../../third_party/srtp/crypto/include" /I "../../third_party/srtp/include" /I "../.." /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D PJ_WIN32=1 /D PJ_M_I386=1 /FR /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"../lib/pjmedia-audiodev-i386-win32-vc6-release.lib"

!ELSEIF  "$(CFG)" == "pjmedia_audiodev - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir ".\output\pjmedia_audiodev-i386-win32-vc6-debug"
# PROP BASE Intermediate_Dir ".\output\pjmedia_audiodev-i386-win32-vc6-debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir ".\output\pjmedia-audiodev-i386-win32-vc6-debug"
# PROP Intermediate_Dir ".\output\pjmedia-audiodev-i386-win32-vc6-debug"
# PROP Target_Dir ""
F90=df.exe
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W4 /Gm /GX /ZI /Od /I "../include" /I "../../pjlib/include" /I "../../pjlib-util/include" /I "../../pjnath/include" /I "../../third_party/portaudio/include" /I "../../third_party/speex/include" /I "../../third_party/build/srtp" /I "../../third_party/srtp/crypto/include" /I "../../third_party/srtp/include" /I "../.." /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D PJ_WIN32=1 /D PJ_M_I386=1 /FR /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"../lib/pjmedia-audiodev-i386-win32-vc6-debug.lib"

!ENDIF 

# Begin Target

# Name "pjmedia_audiodev - Win32 Release"
# Name "pjmedia_audiodev - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE="..\src\pjmedia-audiodev\audiodev.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-audiodev\audiotest.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-audiodev\errno.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-audiodev\legacy_dev.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-audiodev\null_dev.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-audiodev\pa_dev.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-audiodev\symb_aps_dev.cpp"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-audiodev\symb_mda_dev.cpp"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-audiodev\wmme_dev.c"
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE="..\include\pjmedia-audiodev\audiodev.h"
# End Source File
# Begin Source File

SOURCE="..\include\pjmedia-audiodev\audiodev_imp.h"
# End Source File
# Begin Source File

SOURCE="..\include\pjmedia-audiodev\audiotest.h"
# End Source File
# Begin Source File

SOURCE="..\include\pjmedia-audiodev\config.h"
# End Source File
# Begin Source File

SOURCE="..\include\pjmedia-audiodev\errno.h"
# End Source File
# End Group
# End Target
# End Project
