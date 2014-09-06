# Microsoft Developer Studio Project File - Name="libsnmp" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=libsnmp - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "libsnmp.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "libsnmp.mak" CFG="libsnmp - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "libsnmp - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "libsnmp - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "libsnmp - Win32 Release"

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
# ADD CPP /nologo /MD /W3 /GX /O2 /I "." /I ".." /I "..\..\snmplib" /I "..\.." /I "..\..\include" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_CRT_SECURE_NO_WARNINGS" /D "_CRT_NONSTDC_NO_WARNINGS" /D "_MBCS" /YX /FD /c
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"../lib/release/netsnmp.lib"

!ELSEIF  "$(CFG)" == "libsnmp - Win32 Debug"

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
# ADD CPP /nologo /MDd /W3 /Gm /GX /Zi /Od /I "." /I ".." /I "..\..\snmplib" /I "..\.." /I "..\..\include" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_CRT_SECURE_NO_WARNINGS" /D "_CRT_NONSTDC_NO_WARNINGS" /D "_MBCS" /YX /FD /c
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"../lib/debug/netsnmp.lib"

!ENDIF 

# Begin Target

# Name "libsnmp - Win32 Release"
# Name "libsnmp - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\snmplib\asn1.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\callback.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\check_varbind.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\closedir.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\container.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\container_binary_array.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\container_iterator.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\container_list_ssll.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\container_null.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\data_list.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\default_store.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\fd_event_manager.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\getopt.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\gettimeofday.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\inet_ntop.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\inet_pton.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\int64.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\keytools.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\large_fd_set.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\lcd_time.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\md5.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\mib.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\mt_support.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\oid_stash.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\opendir.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\parse.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\read_config.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\readdir.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\scapi.c
# End Source File
# Begin Source File

SOURCE="..\..\snmplib\snmp-tc.c"
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\snmp.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\snmp_alarm.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\snmp_api.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\snmp_auth.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\snmp_client.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\snmp_debug.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\snmp_enum.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\snmp_logging.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\snmp_parse_args.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\snmp_secmod.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\snmp_service.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\snmp_transport.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\snmp_version.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\transports\snmpAliasDomain.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\transports\snmpCallbackDomain.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\transports\snmpIPv4BaseDomain.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\transports\snmpIPv6BaseDomain.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\transports\snmpSocketBaseDomain.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\transports\snmpTCPBaseDomain.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\transports\snmpTCPDomain.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\transports\snmpTCPIPv6Domain.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\transports\snmpUDPBaseDomain.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\transports\snmpUDPDomain.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\transports\snmpUDPIPv4BaseDomain.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\transports\snmpUDPIPv6Domain.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\snmpusm.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\snmpv3.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\strlcat.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\strlcpy.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\strtok_r.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\strtoull.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\system.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\tools.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\ucd_compat.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\vacm.c
# End Source File
# Begin Source File

SOURCE=..\..\snmplib\winpipe.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE="..\..\include\net-snmp\library\asn1.h"
# End Source File
# Begin Source File

SOURCE="..\..\include\net-snmp\library\callback.h"
# End Source File
# Begin Source File

SOURCE="..\..\include\net-snmp\library\check_varbind.h"
# End Source File
# Begin Source File

SOURCE="..\..\include\net-snmp\library\container.h"
# End Source File
# Begin Source File

SOURCE="..\..\include\net-snmp\library\data_list.h"
# End Source File
# Begin Source File

SOURCE="..\..\include\net-snmp\library\default_store.h"
# End Source File
# Begin Source File

SOURCE="..\..\include\net-snmp\library\fd_event_manager.h"
# End Source File
# Begin Source File

SOURCE="..\..\include\net-snmp\library\getopt.h"
# End Source File
# Begin Source File

SOURCE="..\..\include\net-snmp\library\int64.h"
# End Source File
# Begin Source File

SOURCE="..\..\include\net-snmp\library\keytools.h"
# End Source File
# Begin Source File

SOURCE="..\..\include\net-snmp\library\large_fd_set.h"
# End Source File
# Begin Source File

SOURCE="..\..\include\net-snmp\library\lcd_time.h"
# End Source File
# Begin Source File

SOURCE="..\..\include\net-snmp\library\md5.h"
# End Source File
# Begin Source File

SOURCE="..\..\include\net-snmp\library\mib.h"
# End Source File
# Begin Source File

SOURCE="..\..\include\net-snmp\library\mt_support.h"
# End Source File
# Begin Source File

SOURCE="..\..\include\net-snmp\library\oid_stash.h"
# End Source File
# Begin Source File

SOURCE="..\..\include\net-snmp\library\parse.h"
# End Source File
# Begin Source File

SOURCE="..\..\include\net-snmp\library\read_config.h"
# End Source File
# Begin Source File

SOURCE="..\..\include\net-snmp\library\scapi.h"
# End Source File
# Begin Source File

SOURCE="..\..\include\net-snmp\library\snmp-tc.h"
# End Source File
# Begin Source File

SOURCE="..\..\include\net-snmp\library\snmp_alarm.h"
# End Source File
# Begin Source File

SOURCE="..\..\include\net-snmp\library\snmp_api.h"
# End Source File
# Begin Source File

SOURCE="..\..\include\net-snmp\library\snmp_client.h"
# End Source File
# Begin Source File

SOURCE="..\..\include\net-snmp\library\snmp_debug.h"
# End Source File
# Begin Source File

SOURCE="..\..\include\net-snmp\library\snmp_enum.h"
# End Source File
# Begin Source File

SOURCE="..\..\include\net-snmp\library\snmp_logging.h"
# End Source File
# Begin Source File

SOURCE="..\..\include\net-snmp\library\snmp_parse_args.h"
# End Source File
# Begin Source File

SOURCE="..\..\include\net-snmp\library\snmp_secmod.h"
# End Source File
# Begin Source File

SOURCE="..\..\include\net-snmp\library\snmp_transport.h"
# End Source File
# Begin Source File

SOURCE="..\..\include\net-snmp\library\snmpCallbackDomain.h"
# End Source File
# Begin Source File

SOURCE="..\..\include\net-snmp\library\snmpTCPDomain.h"
# End Source File
# Begin Source File

SOURCE="..\..\include\net-snmp\library\snmpUDPDomain.h"
# End Source File
# Begin Source File

SOURCE="..\..\include\net-snmp\library\snmpusm.h"
# End Source File
# Begin Source File

SOURCE="..\..\include\net-snmp\library\snmpv3.h"
# End Source File
# Begin Source File

SOURCE="..\..\include\net-snmp\library\strtok_r.h"
# End Source File
# Begin Source File

SOURCE="..\..\include\net-snmp\library\system.h"
# End Source File
# Begin Source File

SOURCE="..\..\include\net-snmp\library\tools.h"
# End Source File
# Begin Source File

SOURCE="..\..\include\net-snmp\library\ucd_compat.h"
# End Source File
# Begin Source File

SOURCE="..\..\include\net-snmp\library\vacm.h"
# End Source File
# End Group
# End Target
# End Project
