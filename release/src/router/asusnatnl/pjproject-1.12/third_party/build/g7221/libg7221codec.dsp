# Microsoft Developer Studio Project File - Name="libg7221codec" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=libg7221codec - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "libg7221codec.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "libg7221codec.mak" CFG="libg7221codec - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "libg7221codec - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "libg7221codec - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "libg7221codec - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "output\libg7221codec-i386-win32-vc6-release"
# PROP Intermediate_Dir "output\libg7221codec-i386-win32-vc6-release"
# PROP Target_Dir ""
F90=df.exe
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "../.." /I "../../g7221/common" /I "../../g7221/common/stl-files" /I "../../../pjlib/include" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /FR /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\lib\libg7221codec-i386-win32-vc6-release.lib"

!ELSEIF  "$(CFG)" == "libg7221codec - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "output\libg7221codec-i386-win32-vc6-debug"
# PROP Intermediate_Dir "output\libg7221codec-i386-win32-vc6-debug"
# PROP Target_Dir ""
F90=df.exe
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "../.." /I "../../g7221/common" /I "../../g7221/common/stl-files" /I "../../../pjlib/include" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /FR /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\lib\libg7221codec-i386-win32-vc6-debug.lib"

!ENDIF 

# Begin Target

# Name "libg7221codec - Win32 Release"
# Name "libg7221codec - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Group "common"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\g7221\common\basic_op.c
# End Source File
# Begin Source File

SOURCE=..\..\g7221\common\basic_op.h
# End Source File
# Begin Source File

SOURCE=..\..\g7221\common\basic_op_i.h
# End Source File
# Begin Source File

SOURCE=..\..\g7221\common\common.c
# End Source File
# Begin Source File

SOURCE=..\..\g7221\common\count.h
# End Source File
# Begin Source File

SOURCE=..\..\g7221\common\defs.h
# End Source File
# Begin Source File

SOURCE=..\..\g7221\common\huff_def.h
# End Source File
# Begin Source File

SOURCE=..\..\g7221\common\huff_tab.c
# End Source File
# Begin Source File

SOURCE=..\..\g7221\common\huff_tab.h
# End Source File
# Begin Source File

SOURCE=..\..\g7221\common\tables.c
# End Source File
# Begin Source File

SOURCE=..\..\g7221\common\tables.h
# End Source File
# Begin Source File

SOURCE=..\..\g7221\common\typedef.h
# End Source File
# End Group
# Begin Group "decode"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\g7221\decode\coef2sam.c
# End Source File
# Begin Source File

SOURCE=..\..\g7221\decode\dct4_s.c
# End Source File
# Begin Source File

SOURCE=..\..\g7221\decode\dct4_s.h
# End Source File
# Begin Source File

SOURCE=..\..\g7221\decode\decoder.c
# End Source File
# End Group
# Begin Group "encode"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\g7221\encode\dct4_a.c
# End Source File
# Begin Source File

SOURCE=..\..\g7221\encode\dct4_a.h
# End Source File
# Begin Source File

SOURCE=..\..\g7221\encode\encoder.c
# End Source File
# Begin Source File

SOURCE=..\..\g7221\encode\sam2coef.c
# End Source File
# End Group
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# End Group
# End Target
# End Project
