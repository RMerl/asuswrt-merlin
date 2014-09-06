# Microsoft Developer Studio Project File - Name="snmptrapd" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=snmptrapd - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "snmptrapd.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "snmptrapd.mak" CFG="snmptrapd - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "snmptrapd - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "snmptrapd - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "snmptrapd - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_CRT_SECURE_NO_WARNINGS" /D "_CRT_NONSTDC_NO_WARNINGS" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "." /I ".." /I "..\..\snmplib" /I "..\.." /I "..\..\include" /I "..\..\agent\mibgroup" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_CRT_SECURE_NO_WARNINGS" /D "_CRT_NONSTDC_NO_WARNINGS" /D "_MBCS" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib advapi32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 advapi32.lib ws2_32.lib kernel32.lib user32.lib /nologo /subsystem:console /pdb:none /machine:I386 /out:"../bin/release/snmptrapd.exe" /libpath:"../lib/release"

!ELSEIF  "$(CFG)" == "snmptrapd - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_CRT_SECURE_NO_WARNINGS" /D "_CRT_NONSTDC_NO_WARNINGS" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /Zi /Od /I "." /I ".." /I "..\..\snmplib" /I "..\.." /I "..\..\include" /I "..\..\agent\mibgroup" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_CRT_SECURE_NO_WARNINGS" /D "_CRT_NONSTDC_NO_WARNINGS" /D "_MBCS" /FR /YX /FD /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib advapi32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 advapi32.lib ws2_32.lib kernel32.lib user32.lib /nologo /subsystem:console /incremental:no /debug /machine:I386 /out:"../bin/debug/snmptrapd.exe" /pdbtype:sept /libpath:"../lib/debug"

!ENDIF 

# Begin Target

# Name "snmptrapd - Win32 Release"
# Name "snmptrapd - Win32 Debug"
# Begin Source File

SOURCE=..\..\apps\snmptrapd.c
# End Source File
# Begin Source File

SOURCE=..\..\apps\snmptrapd_handlers.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\winservice.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\winservice.rc
# End Source File
# End Target
# End Project
