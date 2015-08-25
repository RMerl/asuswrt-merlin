# Microsoft Developer Studio Project File - Name="libgsmcodec" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=libgsmcodec - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "libgsmcodec.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "libgsmcodec.mak" CFG="libgsmcodec - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "libgsmcodec - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "libgsmcodec - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "libgsmcodec - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "output\gsm-i386-win32-vc6-release"
# PROP BASE Intermediate_Dir "output\gsm-i386-win32-vc6-release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "output\gsm-i386-win32-vc6-release"
# PROP Intermediate_Dir "output\gsm-i386-win32-vc6-release"
# PROP Target_Dir ""
F90=df.exe
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MD /GX /O2 /I "." /I "../../gsm/inc" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\lib\libgsmcodec-i386-win32-vc6-release.lib"

!ELSEIF  "$(CFG)" == "libgsmcodec - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "output\gsm-i386-win32-vc6-debug"
# PROP BASE Intermediate_Dir "output\gsm-i386-win32-vc6-debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "output\gsm-i386-win32-vc6-debug"
# PROP Intermediate_Dir "output\gsm-i386-win32-vc6-debug"
# PROP Target_Dir ""
F90=df.exe
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W2 /Gm /GX /ZI /Od /I "." /I "../../gsm/inc" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\lib\libgsmcodec-i386-win32-vc6-debug.lib"

!ENDIF 

# Begin Target

# Name "libgsmcodec - Win32 Release"
# Name "libgsmcodec - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\gsm\src\add.c
# End Source File
# Begin Source File

SOURCE=..\..\gsm\src\code.c
# End Source File
# Begin Source File

SOURCE=..\..\gsm\src\debug.c
# End Source File
# Begin Source File

SOURCE=..\..\gsm\src\decode.c
# End Source File
# Begin Source File

SOURCE=..\..\gsm\src\gsm_create.c
# End Source File
# Begin Source File

SOURCE=..\..\gsm\src\gsm_decode.c
# End Source File
# Begin Source File

SOURCE=..\..\gsm\src\gsm_destroy.c
# End Source File
# Begin Source File

SOURCE=..\..\gsm\src\gsm_encode.c
# End Source File
# Begin Source File

SOURCE=..\..\gsm\src\gsm_explode.c
# End Source File
# Begin Source File

SOURCE=..\..\gsm\src\gsm_implode.c
# End Source File
# Begin Source File

SOURCE=..\..\gsm\src\gsm_option.c
# End Source File
# Begin Source File

SOURCE=..\..\gsm\src\gsm_print.c
# End Source File
# Begin Source File

SOURCE=..\..\gsm\src\long_term.c
# End Source File
# Begin Source File

SOURCE=..\..\gsm\src\lpc.c
# End Source File
# Begin Source File

SOURCE=..\..\gsm\src\preprocess.c
# End Source File
# Begin Source File

SOURCE=..\..\gsm\src\rpe.c
# End Source File
# Begin Source File

SOURCE=..\..\gsm\src\short_term.c
# End Source File
# Begin Source File

SOURCE=..\..\gsm\src\table.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\config.h
# End Source File
# Begin Source File

SOURCE=..\..\gsm\inc\config.h
# End Source File
# Begin Source File

SOURCE=..\..\gsm\inc\gsm.h
# End Source File
# Begin Source File

SOURCE=..\..\gsm\inc\private.h
# End Source File
# Begin Source File

SOURCE=..\..\gsm\inc\proto.h
# End Source File
# Begin Source File

SOURCE=..\..\gsm\inc\toast.h
# End Source File
# Begin Source File

SOURCE=..\..\gsm\inc\unproto.h
# End Source File
# End Group
# End Target
# End Project
