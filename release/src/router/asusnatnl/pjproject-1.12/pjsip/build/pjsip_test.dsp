# Microsoft Developer Studio Project File - Name="pjsip_test" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=pjsip_test - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "pjsip_test.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "pjsip_test.mak" CFG="pjsip_test - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "pjsip_test - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "pjsip_test - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "pjsip_test - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ".\output\test-pjsip-i386-win32-vc6-release"
# PROP BASE Intermediate_Dir ".\output\test-pjsip-i386-win32-vc6-release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir ".\output\test-pjsip-i386-win32-vc6-release"
# PROP Intermediate_Dir ".\output\test-pjsip-i386-win32-vc6-release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /Zi /O2 /Ob2 /I "../include" /I "../../pjlib/include" /I "../../pjlib-util/include" /I "../../pjnath/include" /I "../../pjmedia/include" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /D "PJ_WIN32" /D "PJ_M_I386" /FR /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 dsound.lib dxguid.lib netapi32.lib mswsock.lib ws2_32.lib kernel32.lib netapi32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /profile /map /debug /machine:I386 /out:"..\bin\test-pjsip-i386-win32-vc6-release.exe" /fixed:no

!ELSEIF  "$(CFG)" == "pjsip_test - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir ".\output\test-pjsip-i386-win32-vc6-debug"
# PROP BASE Intermediate_Dir ".\output\test-pjsip-i386-win32-vc6-debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir ".\output\test-pjsip-i386-win32-vc6-debug"
# PROP Intermediate_Dir ".\output\test-pjsip-i386-win32-vc6-debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "../include" /I "../../pjlib/include" /I "../../pjlib-util/include" /I "../../pjnath/include" /I "../../pjmedia/include" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /D "PJ_WIN32" /D "PJ_M_I386" /FR /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 dsound.lib dxguid.lib netapi32.lib mswsock.lib ws2_32.lib kernel32.lib netapi32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /out:"..\bin\test-pjsip-i386-win32-vc6-debug.exe" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "pjsip_test - Win32 Release"
# Name "pjsip_test - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE="..\src\test\dlg_core_test.c"
# End Source File
# Begin Source File

SOURCE="..\src\test\dns_test.c"
# End Source File
# Begin Source File

SOURCE="..\src\test\inv_offer_answer_test.c"
# End Source File
# Begin Source File

SOURCE="..\src\test\main.c"
# End Source File
# Begin Source File

SOURCE="..\src\test\msg_err_test.c"
# End Source File
# Begin Source File

SOURCE="..\src\test\msg_logger.c"
# End Source File
# Begin Source File

SOURCE="..\src\test\msg_test.c"
# End Source File
# Begin Source File

SOURCE=..\src\test\multipart_test.c
# End Source File
# Begin Source File

SOURCE="..\src\test\regc_test.c"
# End Source File
# Begin Source File

SOURCE="..\src\test\test.c"
# End Source File
# Begin Source File

SOURCE="..\src\test\transport_loop_test.c"
# End Source File
# Begin Source File

SOURCE="..\src\test\transport_tcp_test.c"
# End Source File
# Begin Source File

SOURCE="..\src\test\transport_test.c"
# End Source File
# Begin Source File

SOURCE="..\src\test\transport_udp_test.c"
# End Source File
# Begin Source File

SOURCE="..\src\test\tsx_basic_test.c"
# End Source File
# Begin Source File

SOURCE="..\src\test\tsx_bench.c"
# End Source File
# Begin Source File

SOURCE="..\src\test\tsx_uac_test.c"
# End Source File
# Begin Source File

SOURCE="..\src\test\tsx_uas_test.c"
# End Source File
# Begin Source File

SOURCE="..\src\test\txdata_test.c"
# End Source File
# Begin Source File

SOURCE="..\src\test\uri_test.c"
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE="..\src\test\test.h"
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project
