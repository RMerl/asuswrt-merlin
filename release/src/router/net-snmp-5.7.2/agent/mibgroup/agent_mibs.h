config_require(agent/nsTransactionTable)
config_require(agent/nsModuleTable)
#ifndef NETSNMP_NO_DEBUGGING
config_require(agent/nsDebug)
#endif
config_require(agent/nsCache)
config_require(agent/nsLogging)
config_require(agent/nsVacmAccessTable)
config_add_mib(NET-SNMP-AGENT-MIB)
