# Microsoft Developer Studio Project File - Name="pjmedia_codec" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=pjmedia_codec - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "pjmedia_codec.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "pjmedia_codec.mak" CFG="pjmedia_codec - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "pjmedia_codec - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "pjmedia_codec - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "pjmedia_codec - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ".\output\pjmedia-codec-i386-win32-vc6-debug"
# PROP BASE Intermediate_Dir ".\output\pjmedia-codec-i386-win32-vc6-release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir ".\output\pjmedia-codec-i386-win32-vc6-release"
# PROP Intermediate_Dir ".\output\pjmedia-codec-i386-win32-vc6-release"
# PROP Target_Dir ""
F90=df.exe
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MD /W4 /Zi /O2 /I "../include" /I "../../pjlib/include" /I "../src/pjmedia-codec" /I "../../third_party" /I "../../third_party/speex/include" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D PJ_WIN32=1 /D PJ_M_I386=1 /D "HAVE_CONFIG_H" /FR /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\lib\pjmedia-codec-i386-win32-vc6-release.lib"

!ELSEIF  "$(CFG)" == "pjmedia_codec - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir ".\output\pjmedia-codec-i386-win32-vc6-debug"
# PROP BASE Intermediate_Dir ".\output\pjmedia-codec-i386-win32-vc6-debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir ".\output\pjmedia-codec-i386-win32-vc6-debug"
# PROP Intermediate_Dir ".\output\pjmedia-codec-i386-win32-vc6-debug"
# PROP Target_Dir ""
F90=df.exe
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W4 /Gm /ZI /Od /I "../include" /I "../../pjlib/include" /I "../src/pjmedia-codec" /I "../../third_party" /I "../../third_party/speex/include" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D PJ_WIN32=1 /D PJ_M_I386=1 /D "HAVE_CONFIG_H" /FR /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\lib\pjmedia-codec-i386-win32-vc6-debug.lib"

!ENDIF 

# Begin Target

# Name "pjmedia_codec - Win32 Release"
# Name "pjmedia_codec - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Group "g722 Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE="..\src\pjmedia-codec\g722\g722_dec.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\g722\g722_dec.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\g722\g722_enc.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\g722\g722_enc.h"
# End Source File
# End Group
# Begin Source File

SOURCE="..\src\pjmedia-codec\g722.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\g7221.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\gsm.c"

!IF  "$(CFG)" == "pjmedia_codec - Win32 Release"

!ELSEIF  "$(CFG)" == "pjmedia_codec - Win32 Debug"

# ADD CPP /W4

!ENDIF 

# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\ilbc.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\ipp_codecs.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\l16.c"

!IF  "$(CFG)" == "pjmedia_codec - Win32 Release"

!ELSEIF  "$(CFG)" == "pjmedia_codec - Win32 Debug"

# ADD CPP /W4

!ENDIF 

# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\passthrough.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex_codec.c"

!IF  "$(CFG)" == "pjmedia_codec - Win32 Release"

!ELSEIF  "$(CFG)" == "pjmedia_codec - Win32 Debug"

# ADD CPP /W4

!ENDIF 

# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE="..\include\pjmedia-codec\amr_helper.h"
# End Source File
# Begin Source File

SOURCE="..\include\pjmedia-codec\config.h"
# End Source File
# Begin Source File

SOURCE="..\include\pjmedia-codec\g722.h"
# End Source File
# Begin Source File

SOURCE="..\include\pjmedia-codec\g7221.h"
# End Source File
# Begin Source File

SOURCE="..\include\pjmedia-codec\gsm.h"
# End Source File
# Begin Source File

SOURCE="..\include\pjmedia-codec\ilbc.h"
# End Source File
# Begin Source File

SOURCE="..\include\pjmedia-codec\ipp_codecs.h"
# End Source File
# Begin Source File

SOURCE="..\include\pjmedia-codec\l16.h"
# End Source File
# Begin Source File

SOURCE="..\include\pjmedia-codec\passthrough.h"
# End Source File
# Begin Source File

SOURCE="..\include\pjmedia-codec.h"
# End Source File
# Begin Source File

SOURCE="..\include\pjmedia-codec\speex.h"
# End Source File
# Begin Source File

SOURCE="..\include\pjmedia-codec\types.h"
# End Source File
# End Group
# End Target
# End Project
