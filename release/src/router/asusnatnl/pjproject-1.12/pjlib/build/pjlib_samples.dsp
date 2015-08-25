# Microsoft Developer Studio Project File - Name="pjlib_samples" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) External Target" 0x0106

CFG=pjlib_samples - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "pjlib_samples.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "pjlib_samples.mak" CFG="pjlib_samples - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "pjlib_samples - Win32 Release" (based on "Win32 (x86) External Target")
!MESSAGE "pjlib_samples - Win32 Debug" (based on "Win32 (x86) External Target")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""

!IF  "$(CFG)" == "pjlib_samples - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "./output/pjlib-samples-i386-win32-vc6-release"
# PROP BASE Intermediate_Dir "./output/pjlib-samples-i386-win32-vc6-release"
# PROP BASE Cmd_Line "NMAKE /f pjlib_samples.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "pjlib_samples.exe"
# PROP BASE Bsc_Name "pjlib_samples.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "./output/pjlib-samples-i386-win32-vc6-release"
# PROP Intermediate_Dir "./output/pjlib-samples-i386-win32-vc6-release"
# PROP Cmd_Line "nmake /f "pjlib_samples.mak" MODE=release"
# PROP Rebuild_Opt "/a"
# PROP Target_File "pjlib samples"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "pjlib_samples - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "./output/pjlib-samples-i386-win32-vc6-debug"
# PROP BASE Intermediate_Dir "./output/pjlib-samples-i386-win32-vc6-debug"
# PROP BASE Cmd_Line "NMAKE /f pjlib_samples.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "pjlib_samples.exe"
# PROP BASE Bsc_Name "pjlib_samples.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "./output/pjlib-samples-i386-win32-vc6-debug"
# PROP Intermediate_Dir "./output/pjlib-samples-i386-win32-vc6-debug"
# PROP Cmd_Line "nmake /nologo /f "pjlib_samples.mak" MODE=debug"
# PROP Rebuild_Opt "/a"
# PROP Target_File "pjlib samples"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ENDIF 

# Begin Target

# Name "pjlib_samples - Win32 Release"
# Name "pjlib_samples - Win32 Debug"

!IF  "$(CFG)" == "pjlib_samples - Win32 Release"

!ELSEIF  "$(CFG)" == "pjlib_samples - Win32 Debug"

!ENDIF 

# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE="..\src\pjlib-samples\except.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjlib-samples\list.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjlib-samples\log.c"
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# Begin Source File

SOURCE=.\pjlib_samples.mak
# End Source File
# End Target
# End Project
