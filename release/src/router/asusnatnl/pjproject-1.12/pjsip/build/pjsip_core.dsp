# Microsoft Developer Studio Project File - Name="pjsip_core" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=pjsip_core - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "pjsip_core.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "pjsip_core.mak" CFG="pjsip_core - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "pjsip_core - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "pjsip_core - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "pjsip_core - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ".\output\pjsip-core-i386-win32-vc6-release"
# PROP BASE Intermediate_Dir ".\output\pjsip-core-i386-win32-vc6-release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir ".\output\pjsip-core-i386-win32-vc6-release"
# PROP Intermediate_Dir ".\output\pjsip-core-i386-win32-vc6-release"
# PROP Target_Dir ""
F90=df.exe
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MD /W4 /Zi /O2 /I "../include" /I "../../pjlib/include" /I "../../pjlib-util/include" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D PJ_WIN32=1 /D PJ_M_I386=1 /FR /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"../lib/pjsip-core-i386-win32-vc6-release.lib"

!ELSEIF  "$(CFG)" == "pjsip_core - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir ".\output\pjsip-core-i386-win32-vc6-debug"
# PROP BASE Intermediate_Dir ".\output\pjsip-core-i386-win32-vc6-debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir ".\output\pjsip-core-i386-win32-vc6-debug"
# PROP Intermediate_Dir ".\output\pjsip-core-i386-win32-vc6-debug"
# PROP Target_Dir ""
F90=df.exe
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W4 /Gm /GX /ZI /Od /I "../include" /I "../../pjlib/include" /I "../../pjlib-util/include" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D PJ_WIN32=1 /D PJ_M_I386=1 /FR /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"../lib/pjsip-core-i386-win32-vc6-debug.lib"

!ENDIF 

# Begin Target

# Name "pjsip_core - Win32 Release"
# Name "pjsip_core - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Group "Base (.c)"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\src\pjsip\sip_config.c
# End Source File
# Begin Source File

SOURCE=..\src\pjsip\sip_errno.c
# End Source File
# End Group
# Begin Group "Messaging and Parsing (.c)"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\src\pjsip\sip_msg.c
# End Source File
# Begin Source File

SOURCE=..\src\pjsip\sip_multipart.c
# End Source File
# Begin Source File

SOURCE=..\src\pjsip\sip_parser.c
# End Source File
# Begin Source File

SOURCE=..\src\pjsip\sip_tel_uri.c
# End Source File
# Begin Source File

SOURCE=..\src\pjsip\sip_uri.c
# End Source File
# End Group
# Begin Group "Core (.c)"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\src\pjsip\sip_endpoint.c
# End Source File
# Begin Source File

SOURCE=..\src\pjsip\sip_util.c
# End Source File
# Begin Source File

SOURCE=..\src\pjsip\sip_util_proxy.c
# End Source File
# End Group
# Begin Group "Transport Layer (.c)"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\src\pjsip\sip_resolve.c
# End Source File
# Begin Source File

SOURCE=..\src\pjsip\sip_transport.c
# End Source File
# Begin Source File

SOURCE=..\src\pjsip\sip_transport_loop.c
# End Source File
# Begin Source File

SOURCE=..\src\pjsip\sip_transport_tcp.c
# End Source File
# Begin Source File

SOURCE=..\src\pjsip\sip_transport_tls.c
# End Source File
# Begin Source File

SOURCE=..\src\pjsip\sip_transport_tls_ossl.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=..\src\pjsip\sip_transport_udp.c
# End Source File
# End Group
# Begin Group "Authentication (.c)"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\src\pjsip\sip_auth_aka.c
# End Source File
# Begin Source File

SOURCE=..\src\pjsip\sip_auth_client.c
# End Source File
# Begin Source File

SOURCE=..\src\pjsip\sip_auth_msg.c
# End Source File
# Begin Source File

SOURCE=..\src\pjsip\sip_auth_parser.c
# End Source File
# Begin Source File

SOURCE=..\src\pjsip\sip_auth_server.c
# End Source File
# End Group
# Begin Group "Transaction Layer (.c)"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\src\pjsip\sip_transaction.c
# End Source File
# Begin Source File

SOURCE=..\src\pjsip\sip_util_statefull.c
# End Source File
# End Group
# Begin Group "UA Layer (.c)"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\src\pjsip\sip_dialog.c
# End Source File
# Begin Source File

SOURCE=..\src\pjsip\sip_ua_layer.c
# End Source File
# End Group
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Group "Base Types (.h)"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\include\pjsip\sip_config.h
# End Source File
# Begin Source File

SOURCE=..\include\pjsip\sip_errno.h
# End Source File
# Begin Source File

SOURCE=..\include\pjsip\sip_private.h
# End Source File
# Begin Source File

SOURCE=..\include\pjsip\sip_types.h
# End Source File
# End Group
# Begin Group "Messaging and Parsing (.h)"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\include\pjsip\print_util.h
# End Source File
# Begin Source File

SOURCE=..\include\pjsip\sip_msg.h
# End Source File
# Begin Source File

SOURCE=..\include\pjsip\sip_multipart.h
# End Source File
# Begin Source File

SOURCE=..\include\pjsip\sip_parser.h
# End Source File
# Begin Source File

SOURCE=..\include\pjsip\sip_tel_uri.h
# End Source File
# Begin Source File

SOURCE=..\include\pjsip\sip_uri.h
# End Source File
# End Group
# Begin Group "Core (.h)"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\include\pjsip\sip_endpoint.h
# End Source File
# Begin Source File

SOURCE=..\include\pjsip\sip_event.h
# End Source File
# Begin Source File

SOURCE=..\include\pjsip\sip_module.h
# End Source File
# Begin Source File

SOURCE=..\include\pjsip\sip_util.h
# End Source File
# End Group
# Begin Group "Transport Layer (.h)"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\include\pjsip\sip_resolve.h
# End Source File
# Begin Source File

SOURCE=..\include\pjsip\sip_transport.h
# End Source File
# Begin Source File

SOURCE=..\include\pjsip\sip_transport_loop.h
# End Source File
# Begin Source File

SOURCE=..\include\pjsip\sip_transport_tcp.h
# End Source File
# Begin Source File

SOURCE=..\include\pjsip\sip_transport_tls.h
# End Source File
# Begin Source File

SOURCE=..\include\pjsip\sip_transport_udp.h
# End Source File
# End Group
# Begin Group "Authentication (.h)"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\include\pjsip\sip_auth.h
# End Source File
# Begin Source File

SOURCE=..\include\pjsip\sip_auth_aka.h
# End Source File
# Begin Source File

SOURCE=..\include\pjsip\sip_auth_msg.h
# End Source File
# Begin Source File

SOURCE=..\include\pjsip\sip_auth_parser.h
# End Source File
# End Group
# Begin Group "Transaction Layer (.h)"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\include\pjsip\sip_transaction.h
# End Source File
# End Group
# Begin Group "UA Layer (.h)"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\include\pjsip\sip_dialog.h
# End Source File
# Begin Source File

SOURCE=..\include\pjsip\sip_ua_layer.h
# End Source File
# End Group
# Begin Source File

SOURCE=..\docs\doxygen.h
# End Source File
# Begin Source File

SOURCE=..\include\pjsip.h
# End Source File
# End Group
# Begin Group "Inline Files"

# PROP Default_Filter ""
# End Group
# Begin Source File

SOURCE=..\..\INSTALL.txt
# End Source File
# Begin Source File

SOURCE=..\..\RELNOTES.txt
# End Source File
# End Target
# End Project
