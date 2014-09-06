#ifndef SNMPV3MIBS_H
#define SNMPV3MIBS_H

/*
 * snmpv3mibs.h: mib module to include the modules relavent to the
 * snmpv3 mib(s) 
 */

config_require(snmpv3/snmpEngine)
config_version_require((snmpv3/snmpMPDStats, 5.5, snmpv3/snmpMPDStats_5_5))
#ifdef NETSNMP_SECMOD_USM
config_version_require((snmpv3/usmStats, 5.5, snmpv3/usmStats_5_5))
config_require(snmpv3/usmConf)
config_require(snmpv3/usmUser)
#endif /* NETSNMP_SECMOD_USM */
#endif                          /* SNMPV3MIBS_H */
