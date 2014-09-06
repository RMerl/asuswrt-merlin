/*
 * module to include the modules
 */
config_require(tlstm-mib/snmpTlstmSession)
config_require(tlstm-mib/snmpTlstmAddrTable)
config_require(tlstm-mib/snmpTlstmCertToTSNTable)
config_require(tlstm-mib/snmpTlstmParamsTable)
config_add_mib(SNMP-TLS-TM-MIB)
config_add_mib(SNMP-TSM-MIB)

#define SNMP_TLS_TM_BASE     1, 3, 6, 1, 2, 1, 198
