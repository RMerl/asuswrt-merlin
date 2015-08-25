# Microsoft Developer Studio Project File - Name="pjlib_util" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=pjlib_util - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "pjlib_util.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "pjlib_util.mak" CFG="pjlib_util - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "pjlib_util - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "pjlib_util - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "pjlib_util - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "./output/pjlib-util-i386-win32-vc6-release"
# PROP BASE Intermediate_Dir "./output/pjlib-util-i386-win32-vc6-release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "./output/pjlib-util-i386-win32-vc6-release"
# PROP Intermediate_Dir "./output/pjlib-util-i386-win32-vc6-release"
# PROP Target_Dir ""
F90=df.exe
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MD /W4 /GX /Zi /O2 /Ob2 /I "../include" /I "../../pjlib/include" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D PJ_WIN32=1 /D PJ_M_I386=1 /FR /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"../lib/pjlib-util-i386-win32-vc6-release.lib"

!ELSEIF  "$(CFG)" == "pjlib_util - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "./output/pjlib-util-i386-win32-vc6-debug"
# PROP BASE Intermediate_Dir "./output/pjlib-util-i386-win32-vc6-debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "./output/pjlib-util-i386-win32-vc6-debug"
# PROP Intermediate_Dir "./output/pjlib-util-i386-win32-vc6-debug"
# PROP Target_Dir ""
F90=df.exe
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W4 /Gm /GX /ZI /Od /I "../include" /I "../../pjlib/include" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D PJ_WIN32=1 /D PJ_M_I386=1 /FR /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"../lib/pjlib-util-i386-win32-vc6-debug.lib"

!ENDIF 

# Begin Target

# Name "pjlib_util - Win32 Release"
# Name "pjlib_util - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE="..\src\pjlib-util\base64.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjlib-util\crc32.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjlib-util\dns.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjlib-util\dns_dump.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjlib-util\dns_server.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjlib-util\errno.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjlib-util\getopt.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjlib-util\hmac_md5.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjlib-util\hmac_sha1.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjlib-util\http_client.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjlib-util\md5.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjlib-util\pcap.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjlib-util\resolver.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjlib-util\scanner.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjlib-util\scanner_cis_bitwise.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE="..\src\pjlib-util\scanner_cis_uint.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE="..\src\pjlib-util\sha1.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjlib-util\srv_resolver.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjlib-util\string.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjlib-util\stun_simple.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjlib-util\stun_simple_client.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjlib-util\symbols.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjlib-util\xml.c"
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE="..\include\pjlib-util\base64.h"
# End Source File
# Begin Source File

SOURCE="..\include\pjlib-util\config.h"
# End Source File
# Begin Source File

SOURCE="..\include\pjlib-util\crc32.h"
# End Source File
# Begin Source File

SOURCE="..\include\pjlib-util\dns.h"
# End Source File
# Begin Source File

SOURCE="..\include\pjlib-util\dns_server.h"
# End Source File
# Begin Source File

SOURCE="..\include\pjlib-util\errno.h"
# End Source File
# Begin Source File

SOURCE="..\include\pjlib-util\getopt.h"
# End Source File
# Begin Source File

SOURCE="..\include\pjlib-util\hmac_md5.h"
# End Source File
# Begin Source File

SOURCE="..\include\pjlib-util\hmac_sha1.h"
# End Source File
# Begin Source File

SOURCE="..\include\pjlib-util\http_client.h"
# End Source File
# Begin Source File

SOURCE="..\include\pjlib-util\md5.h"
# End Source File
# Begin Source File

SOURCE="..\include\pjlib-util\pcap.h"
# End Source File
# Begin Source File

SOURCE="..\include\pjlib-util.h"
# End Source File
# Begin Source File

SOURCE="..\include\pjlib-util\resolver.h"
# End Source File
# Begin Source File

SOURCE="..\include\pjlib-util\scanner.h"
# End Source File
# Begin Source File

SOURCE="..\include\pjlib-util\scanner_cis_bitwise.h"
# End Source File
# Begin Source File

SOURCE="..\include\pjlib-util\scanner_cis_uint.h"
# End Source File
# Begin Source File

SOURCE="..\include\pjlib-util\sha1.h"
# End Source File
# Begin Source File

SOURCE="..\include\pjlib-util\srv_resolver.h"
# End Source File
# Begin Source File

SOURCE="..\include\pjlib-util\string.h"
# End Source File
# Begin Source File

SOURCE="..\include\pjlib-util\stun_simple.h"
# End Source File
# Begin Source File

SOURCE="..\include\pjlib-util\types.h"
# End Source File
# Begin Source File

SOURCE="..\include\pjlib-util\xml.h"
# End Source File
# End Group
# End Target
# End Project
