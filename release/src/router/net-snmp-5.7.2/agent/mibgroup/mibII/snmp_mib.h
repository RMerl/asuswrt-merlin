#ifndef _MIBGROUP_SNMP_H
#define _MIBGROUP_SNMP_H

config_require(mibII/updates)
config_exclude(mibII/snmp_mib_5_5)

void            init_snmp_mib(void);

#endif                          /* _MIBGROUP_SNMP_H */
