# Microsoft Developer Studio Project File - Name="libilbccodec" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=libilbccodec - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "libilbccodec.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "libilbccodec.mak" CFG="libilbccodec - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "libilbccodec - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "libilbccodec - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "libilbccodec - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "output\ilbc-i386-win32-vc6-release"
# PROP BASE Intermediate_Dir "output\ilbc-i386-win32-vc6-release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "output\ilbc-i386-win32-vc6-release"
# PROP Intermediate_Dir "output\ilbc-i386-win32-vc6-release"
# PROP Target_Dir ""
F90=df.exe
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\lib\libilbccodec-i386-win32-vc6-release.lib"

!ELSEIF  "$(CFG)" == "libilbccodec - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "output\ilbc-i386-win32-vc6-debug"
# PROP BASE Intermediate_Dir "output\ilbc-i386-win32-vc6-debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "output\ilbc-i386-win32-vc6-debug"
# PROP Intermediate_Dir "output\ilbc-i386-win32-vc6-debug"
# PROP Target_Dir ""
F90=df.exe
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\lib\libilbccodec-i386-win32-vc6-debug.lib"

!ENDIF 

# Begin Target

# Name "libilbccodec - Win32 Release"
# Name "libilbccodec - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\ilbc\anaFilter.c
# End Source File
# Begin Source File

SOURCE=..\..\ilbc\constants.c
# End Source File
# Begin Source File

SOURCE=..\..\ilbc\createCB.c
# End Source File
# Begin Source File

SOURCE=..\..\ilbc\doCPLC.c
# End Source File
# Begin Source File

SOURCE=..\..\ilbc\enhancer.c
# End Source File
# Begin Source File

SOURCE=..\..\ilbc\filter.c
# End Source File
# Begin Source File

SOURCE=..\..\ilbc\FrameClassify.c
# End Source File
# Begin Source File

SOURCE=..\..\ilbc\gainquant.c
# End Source File
# Begin Source File

SOURCE=..\..\ilbc\getCBvec.c
# End Source File
# Begin Source File

SOURCE=..\..\ilbc\helpfun.c
# End Source File
# Begin Source File

SOURCE=..\..\ilbc\hpInput.c
# End Source File
# Begin Source File

SOURCE=..\..\ilbc\hpOutput.c
# End Source File
# Begin Source File

SOURCE=..\..\ilbc\iCBConstruct.c
# End Source File
# Begin Source File

SOURCE=..\..\ilbc\iCBSearch.c
# End Source File
# Begin Source File

SOURCE=..\..\ilbc\iLBC_decode.c
# End Source File
# Begin Source File

SOURCE=..\..\ilbc\iLBC_encode.c
# End Source File
# Begin Source File

SOURCE=..\..\ilbc\LPCdecode.c
# End Source File
# Begin Source File

SOURCE=..\..\ilbc\LPCencode.c
# End Source File
# Begin Source File

SOURCE=..\..\ilbc\lsf.c
# End Source File
# Begin Source File

SOURCE=..\..\ilbc\packing.c
# End Source File
# Begin Source File

SOURCE=..\..\ilbc\StateConstructW.c
# End Source File
# Begin Source File

SOURCE=..\..\ilbc\StateSearchW.c
# End Source File
# Begin Source File

SOURCE=..\..\ilbc\syntFilter.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\ilbc\anaFilter.h
# End Source File
# Begin Source File

SOURCE=..\..\ilbc\constants.h
# End Source File
# Begin Source File

SOURCE=..\..\ilbc\createCB.h
# End Source File
# Begin Source File

SOURCE=..\..\ilbc\doCPLC.h
# End Source File
# Begin Source File

SOURCE=..\..\ilbc\enhancer.h
# End Source File
# Begin Source File

SOURCE=..\..\ilbc\filter.h
# End Source File
# Begin Source File

SOURCE=..\..\ilbc\FrameClassify.h
# End Source File
# Begin Source File

SOURCE=..\..\ilbc\gainquant.h
# End Source File
# Begin Source File

SOURCE=..\..\ilbc\getCBvec.h
# End Source File
# Begin Source File

SOURCE=..\..\ilbc\helpfun.h
# End Source File
# Begin Source File

SOURCE=..\..\ilbc\hpInput.h
# End Source File
# Begin Source File

SOURCE=..\..\ilbc\hpOutput.h
# End Source File
# Begin Source File

SOURCE=..\..\ilbc\iCBConstruct.h
# End Source File
# Begin Source File

SOURCE=..\..\ilbc\iCBSearch.h
# End Source File
# Begin Source File

SOURCE=..\..\ilbc\iLBC_decode.h
# End Source File
# Begin Source File

SOURCE=..\..\ilbc\iLBC_define.h
# End Source File
# Begin Source File

SOURCE=..\..\ilbc\iLBC_encode.h
# End Source File
# Begin Source File

SOURCE=..\..\ilbc\LPCdecode.h
# End Source File
# Begin Source File

SOURCE=..\..\ilbc\LPCencode.h
# End Source File
# Begin Source File

SOURCE=..\..\ilbc\lsf.h
# End Source File
# Begin Source File

SOURCE=..\..\ilbc\packing.h
# End Source File
# Begin Source File

SOURCE=..\..\ilbc\StateConstructW.h
# End Source File
# Begin Source File

SOURCE=..\..\ilbc\StateSearchW.h
# End Source File
# Begin Source File

SOURCE=..\..\ilbc\syntFilter.h
# End Source File
# End Group
# End Target
# End Project
