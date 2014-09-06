/* this is a Net-SNMP distributed file that sets all default mib
   modules to be built into the Net-SNMP agent */


/* these go into both the mini agent and the full agent */
config_require(snmpv3mibs)

/* very few default mibs */
config_add_mib(SNMPv2-MIB)
config_add_mib(IF-MIB)
config_add_mib(IP-MIB)
config_add_mib(TCP-MIB)
config_add_mib(UDP-MIB)

#ifdef NETSNMP_MINI_AGENT

/* limit the mibII modules to the bare minimum */
config_require(mibII/snmp_mib)
config_require(mibII/system_mib)
config_require(mibII/sysORTable)
config_require(mibII/vacm_vars)
config_require(mibII/vacm_conf)

#else /* !NETSNMP_MINI_AGENT == the full shabang */

config_require(mibII)
config_require(ucd_snmp)
config_require(notification)
config_require(notification-log-mib)
config_require(target)
config_require(agent_mibs)
config_require(agentx)
config_require(disman/event)
#ifndef NETSNMP_NO_WRITE_SUPPORT
config_require(disman/schedule)
#endif /* !NETSNMP_NO_WRITE_SUPPORT */
config_require(utilities)

/* default MIBs to auto-include for parsing */
/* NOTE: we consider these MIBs users will likely want to load by
   default, even if they're not supporting it in the agent (ie, the
   command line tools need to load them anyway) */
config_add_mib(HOST-RESOURCES-MIB)
config_add_mib(NOTIFICATION-LOG-MIB)
config_add_mib(DISMAN-EVENT-MIB)
#ifndef NETSNMP_NO_WRITE_SUPPORT
config_add_mib(DISMAN-SCHEDULE-MIB)
#endif /* !NETSNMP_NO_WRITE_SUPPORT */

/* architecture specific extra modules */
/* these symbols are set in the host specific net-snmp/system/<os>.h files */
#ifdef NETSNMP_INCLUDE_HOST_RESOURCES
    config_require(host)
#endif

#endif
