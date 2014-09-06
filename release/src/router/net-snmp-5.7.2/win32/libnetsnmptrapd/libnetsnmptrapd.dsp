# Microsoft Developer Studio Project File - Name="libnetsnmptrapd" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=libnetsnmptrapd - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "libnetsnmptrapd.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "libnetsnmptrapd.mak" CFG="libnetsnmptrapd - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "libnetsnmptrapd - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "libnetsnmptrapd - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "libnetsnmptrapd - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "../lib/release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_CRT_SECURE_NO_WARNINGS" /D "_CRT_NONSTDC_NO_WARNINGS" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "." /I ".." /I "..\..\snmplib" /I "..\.." /I "..\..\include" /I "..\..\agent" /I "..\..\agent\mibgroup" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_CRT_SECURE_NO_WARNINGS" /D "_CRT_NONSTDC_NO_WARNINGS" /D "_MBCS" /YX /FD /c
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"../lib/release/netsnmptrapd.lib"

!ELSEIF  "$(CFG)" == "libnetsnmptrapd - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "../lib/debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_CRT_SECURE_NO_WARNINGS" /D "_CRT_NONSTDC_NO_WARNINGS" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /Zi /Od /I "." /I ".." /I "..\..\snmplib" /I "..\.." /I "..\..\include" /I "..\..\agent" /I "..\..\agent\mibgroup" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_CRT_SECURE_NO_WARNINGS" /D "_CRT_NONSTDC_NO_WARNINGS" /D "_MBCS" /YX /FD /c
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"../lib/debug/netsnmptrapd.lib"

!ENDIF 

# Begin Target

# Name "libnetsnmptrapd - Win32 Release"
# Name "libnetsnmptrapd - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\apps\snmptrapd.c
# End Source File
# Begin Source File

SOURCE=..\..\apps\snmptrapd_auth.c
# End Source File
# Begin Source File

SOURCE=..\..\apps\snmptrapd_handlers.c
# End Source File
# Begin Source File

SOURCE=..\..\apps\snmptrapd_log.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE="..\..\agent\mibgroup\notification-log-mib\notification_log.h"
# End Source File
# Begin Source File

SOURCE="..\..\apps\snmptrapd_auth.h"
# End Source File
# Begin Source File

SOURCE="..\..\apps\snmptrapd_handlers.h"
# End Source File
# Begin Source File

SOURCE="..\..\apps\snmptrapd_log.h"
# End Source File
# End Group
# End Target
# End Project
