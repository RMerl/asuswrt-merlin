#ifndef _MIBGROUP_USMSTATS_H
#define _MIBGROUP_USMSTATS_H

config_exclude(snmpv3/usmStats)
config_add_mib(SNMP-USER-BASED-SM-MIB)

void init_usmStats_5_5(void);
void shutdown_usmStats_5_5(void);

#endif /* _MIBGROUP_USMSTATS_H */
