#ifndef _MIBGROUP_SNMPMPDSTATS_H
#define _MIBGROUP_SNMPMPDSTATS_H

config_exclude(snmpv3/snmpMPDStats)
config_add_mib(SNMP-MPD-MIB)

void init_snmpMPDStats_5_5(void);
void shutdown_snmpMPDStats_5_5(void);

#endif /* _MIBGROUP_SNMPMPDSTATS_H */
