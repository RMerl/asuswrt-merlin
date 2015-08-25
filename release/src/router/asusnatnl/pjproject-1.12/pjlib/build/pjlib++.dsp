# Microsoft Developer Studio Project File - Name="pjlib++" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=pjlib++ - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "pjlib++.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "pjlib++.mak" CFG="pjlib++ - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "pjlib++ - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "pjlib++ - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "pjlib++ - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ".\output\pjlib++-i386-win32-vc6-release"
# PROP BASE Intermediate_Dir ".\output\pjlib++-i386-win32-vc6-release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir ".\output\pjlib++-i386-win32-vc6-release"
# PROP Intermediate_Dir ".\output\pjlib++-i386-win32-vc6-release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "../include" /I "../../pjlib-util/include" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D PJ_WIN32=1 /D PJ_M_I386=1 /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"../lib/pjlib++-i386-win32-vc6-release.lib"

!ELSEIF  "$(CFG)" == "pjlib++ - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir ".\output\pjlib++-i386-win32-vc6-debug"
# PROP BASE Intermediate_Dir ".\output\pjlib++-i386-win32-vc6-debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir ".\output\pjlib++-i386-win32-vc6-debug"
# PROP Intermediate_Dir ".\output\pjlib++-i386-win32-vc6-debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "../include" /I "../../pjlib-util/include" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D PJ_WIN32=1 /D PJ_M_I386=1 /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"../lib/pjlib++-i386-win32-vc6-debug.lib"

!ENDIF 

# Begin Target

# Name "pjlib++ - Win32 Release"
# Name "pjlib++ - Win32 Debug"
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE="..\include\pj++\file.hpp"
# End Source File
# Begin Source File

SOURCE="..\include\pj++\hash.hpp"
# End Source File
# Begin Source File

SOURCE="..\include\pj++\list.hpp"
# End Source File
# Begin Source File

SOURCE="..\include\pj++\lock.hpp"
# End Source File
# Begin Source File

SOURCE="..\include\pj++\os.hpp"
# End Source File
# Begin Source File

SOURCE="..\include\pjlib++.hpp"
# End Source File
# Begin Source File

SOURCE="..\include\pj++\pool.hpp"
# End Source File
# Begin Source File

SOURCE="..\include\pj++\proactor.hpp"
# End Source File
# Begin Source File

SOURCE="..\include\pj++\scanner.hpp"
# End Source File
# Begin Source File

SOURCE="..\include\pj++\sock.hpp"
# End Source File
# Begin Source File

SOURCE="..\include\pj++\string.hpp"
# End Source File
# Begin Source File

SOURCE="..\include\pj++\timer.hpp"
# End Source File
# Begin Source File

SOURCE="..\include\pj++\tree.hpp"
# End Source File
# Begin Source File

SOURCE="..\include\pj++\types.hpp"
# End Source File
# End Group
# End Target
# End Project
