# Microsoft Developer Studio Project File - Name="netsnmpmibs" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=netsnmpmibs - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "netsnmpmibs.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "netsnmpmibs.mak" CFG="netsnmpmibs - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "netsnmpmibs - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "netsnmpmibs - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "netsnmpmibs - Win32 Release"

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
# ADD LIB32 /nologo /out:"../lib/release/netsnmpmibs.lib"

!ELSEIF  "$(CFG)" == "netsnmpmibs - Win32 Debug"

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
# ADD LIB32 /nologo /out:"../lib/debug/netsnmpmibs.lib"

!ENDIF 

# Begin Target

# Name "netsnmpmibs - Win32 Release"
# Name "netsnmpmibs - Win32 Debug"
# Begin Group "mibII"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\agent\mibgroup\mibII\at.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\mibII\icmp.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\mibII\interfaces.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\mibII\ip.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\mibII\ipAddr.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\mibII\route_write.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\mibII\setSerialNo.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\mibII\snmp_mib.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\mibII\sysORTable.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\mibII\system_mib.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\mibII\tcp.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\mibII\tcpTable.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\mibII\udp.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\mibII\udpTable.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\mibII\updates.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\mibII\vacm_conf.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\mibII\vacm_context.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\mibII\vacm_vars.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\mibII\var_route.c
# End Source File
# End Group
# Begin Group "disman"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\agent\mibgroup\disman\event\mteEvent.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\disman\event\mteEventConf.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\disman\event\mteEventNotificationTable.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\disman\event\mteEventSetTable.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\disman\event\mteEventTable.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\disman\event\mteObjects.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\disman\event\mteObjectsConf.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\disman\event\mteObjectsTable.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\disman\event\mteScalars.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\disman\event\mteTriggerBooleanTable.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\disman\event\mteTrigger.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\disman\event\mteTriggerConf.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\disman\event\mteTriggerDeltaTable.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\disman\event\mteTriggerExistenceTable.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\disman\event\mteTriggerTable.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\disman\event\mteTriggerThresholdTable.c
# End Source File
# End Group
# Begin Group "examples"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\agent\mibgroup\examples\example.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\examples\ucdDemoPublic.c
# End Source File
# End Group
# Begin Group "ucd-snmp"

# PROP Default_Filter ""
# Begin Source File

SOURCE="..\..\agent\mibgroup\ucd-snmp\errormib.c"
# End Source File
# Begin Source File

SOURCE="..\..\agent\mibgroup\ucd-snmp\extensible.c"
# End Source File
# Begin Source File

SOURCE="..\..\agent\mibgroup\ucd-snmp\file.c"
# End Source File
# Begin Source File

SOURCE="..\..\agent\mibgroup\ucd-snmp\loadave.c"
# End Source File
# Begin Source File

SOURCE="..\..\agent\mibgroup\ucd-snmp\pass.c"
# End Source File
# Begin Source File

SOURCE="..\..\agent\mibgroup\ucd-snmp\pass_common.c"
# End Source File
# Begin Source File

SOURCE="..\..\agent\mibgroup\ucd-snmp\pass_persist.c"
# End Source File
# Begin Source File

SOURCE="..\..\agent\mibgroup\ucd-snmp\proc.c"
# End Source File
# Begin Source File

SOURCE="..\..\agent\mibgroup\ucd-snmp\proxy.c"
# End Source File
# Begin Source File

SOURCE="..\..\agent\mibgroup\ucd-snmp\versioninfo.c"
# End Source File
# End Group
# Begin Group "snmpv3"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\agent\mibgroup\snmpv3\snmpEngine.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\snmpv3\snmpMPDStats.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\snmpv3\usmConf.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\snmpv3\usmStats.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\snmpv3\usmUser.c
# End Source File
# End Group
# Begin Group "notification"

# PROP Default_Filter ""
# Begin Source File

SOURCE="..\..\agent\mibgroup\notification-log-mib\notification_log.c"
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\notification\snmpNotifyFilterProfileTable.c
# End Source File
# Begin Source File

SOURCE="..\..\agent\mibgroup\snmp-notification-mib\snmpNotifyFilterTable\snmpNotifyFilterTable.c"
# End Source File
# Begin Source File

SOURCE="..\..\agent\mibgroup\snmp-notification-mib\snmpNotifyFilterTable\snmpNotifyFilterTable_data_access.c"
# End Source File
# Begin Source File

SOURCE="..\..\agent\mibgroup\snmp-notification-mib\snmpNotifyFilterTable\snmpNotifyFilterTable_interface.c"
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\notification\snmpNotifyTable.c
# End Source File
# End Group
# Begin Group "target"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\agent\mibgroup\target\snmpTargetAddrEntry.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\target\snmpTargetParamsEntry.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\target\target.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\target\target_counters.c
# End Source File
# End Group
# Begin Group "agentx"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\agent\mibgroup\agentx\agentx_config.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\agentx\client.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\agentx\master.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\agentx\master_admin.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\agentx\protocol.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\agentx\subagent.c
# End Source File
# End Group
# Begin Group "agent"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\agent\mibgroup\agent\extend.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\agent\nsCache.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\agent\nsDebug.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\agent\nsLogging.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\agent\nsModuleTable.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\agent\nsTransactionTable.c
# End Source File
# End Group
# Begin Group "utilities"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\agent\mibgroup\utilities\execute.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\util_funcs\Exit.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\header_complex.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\util_funcs\header_generic.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\util_funcs\header_simple_table.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\utilities\iquery.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mib_modules.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\utilities\override.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\util_funcs\restart.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\util_funcs.c
# End Source File
# End Group
# Begin Group "winExtDLL"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\agent\mibgroup\winExtDLL.c
# End Source File
# Begin Source File

SOURCE=..\..\agent\mibgroup\winExtDLL.h
# End Source File
# End Group
# Begin Group "smux"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\agent\mibgroup\smux\smux.c
# End Source File
# End Group
# End Target
# End Project
