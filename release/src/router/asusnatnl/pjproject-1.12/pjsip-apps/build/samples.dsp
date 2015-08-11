# Microsoft Developer Studio Project File - Name="samples" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) External Target" 0x0106

CFG=samples - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "samples.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "samples.mak" CFG="samples - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "samples - Win32 Release" (based on "Win32 (x86) External Target")
!MESSAGE "samples - Win32 Debug" (based on "Win32 (x86) External Target")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""

!IF  "$(CFG)" == "samples - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "./output/samples-i386-win32-vc6-release"
# PROP BASE Intermediate_Dir "./output/samples-i386-win32-vc6-release"
# PROP BASE Cmd_Line "NMAKE /f samples.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "samples.exe"
# PROP BASE Bsc_Name "samples.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "./output/samples-i386-win32-vc6-release"
# PROP Intermediate_Dir "./output/samples-i386-win32-vc6-release"
# PROP Cmd_Line "nmake /NOLOGO /S /f Samples-vc.mak BUILD_MODE=release VC_VER=6"
# PROP Rebuild_Opt "/a"
# PROP Target_File "All samples"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "samples - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "./output/samples-i386-win32-vc6-debug"
# PROP BASE Intermediate_Dir "./output/samples-i386-win32-vc6-debug"
# PROP BASE Cmd_Line "NMAKE /f samples.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "samples.exe"
# PROP BASE Bsc_Name "samples.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "./output/samples-i386-win32-vc6-debug"
# PROP Intermediate_Dir "./output/samples-i386-win32-vc6-debug"
# PROP Cmd_Line "nmake /NOLOGO /S /f Samples-vc.mak BUILD_MODE=debug VC_VER=6"
# PROP Rebuild_Opt "/a"
# PROP Target_File "All samples"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ENDIF 

# Begin Target

# Name "samples - Win32 Release"
# Name "samples - Win32 Debug"

!IF  "$(CFG)" == "samples - Win32 Release"

!ELSEIF  "$(CFG)" == "samples - Win32 Debug"

!ENDIF 

# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\src\samples\aectest.c
# End Source File
# Begin Source File

SOURCE=..\src\samples\auddemo.c
# End Source File
# Begin Source File

SOURCE=..\src\samples\confbench.c
# End Source File
# Begin Source File

SOURCE=..\src\samples\confsample.c
# End Source File
# Begin Source File

SOURCE=..\src\samples\encdec.c
# End Source File
# Begin Source File

SOURCE=..\src\samples\footprint.c
# End Source File
# Begin Source File

SOURCE=..\src\samples\httpdemo.c
# End Source File
# Begin Source File

SOURCE=..\src\samples\icedemo.c
# End Source File
# Begin Source File

SOURCE=..\src\samples\invtester.c
# End Source File
# Begin Source File

SOURCE=..\src\samples\jbsim.c
# End Source File
# Begin Source File

SOURCE=..\src\samples\latency.c
# End Source File
# Begin Source File

SOURCE=..\src\samples\level.c
# End Source File
# Begin Source File

SOURCE=..\src\samples\mix.c
# End Source File
# Begin Source File

SOURCE=..\src\samples\pcaputil.c
# End Source File
# Begin Source File

SOURCE="..\src\samples\pjsip-perf.c"
# End Source File
# Begin Source File

SOURCE=..\src\samples\playfile.c
# End Source File
# Begin Source File

SOURCE=..\src\samples\playsine.c
# End Source File
# Begin Source File

SOURCE=..\src\samples\recfile.c
# End Source File
# Begin Source File

SOURCE=..\src\samples\resampleplay.c
# End Source File
# Begin Source File

SOURCE=..\src\samples\simple_pjsua.c
# End Source File
# Begin Source File

SOURCE=..\src\samples\simpleua.c
# End Source File
# Begin Source File

SOURCE=..\src\samples\siprtp.c
# End Source File
# Begin Source File

SOURCE=..\src\samples\siprtp_report.c
# End Source File
# Begin Source File

SOURCE=..\src\samples\sipstateless.c
# End Source File
# Begin Source File

SOURCE=..\src\samples\sndinfo.c
# End Source File
# Begin Source File

SOURCE=..\src\samples\sndtest.c
# End Source File
# Begin Source File

SOURCE=..\src\samples\stateful_proxy.c
# End Source File
# Begin Source File

SOURCE=..\src\samples\stateless_proxy.c
# End Source File
# Begin Source File

SOURCE=..\src\samples\stereotest.c
# End Source File
# Begin Source File

SOURCE=..\src\samples\streamutil.c
# End Source File
# Begin Source File

SOURCE=..\src\samples\strerror.c
# End Source File
# Begin Source File

SOURCE=..\src\samples\tonegen.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\src\samples\proxy.h
# End Source File
# Begin Source File

SOURCE=..\src\samples\util.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# Begin Source File

SOURCE=".\Samples-vc.mak"
# End Source File
# Begin Source File

SOURCE=.\Samples.mak
# End Source File
# End Target
# End Project
