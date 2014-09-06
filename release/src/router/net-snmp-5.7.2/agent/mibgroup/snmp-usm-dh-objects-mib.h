#ifndef SNMP_USM_DH_OBJECTS_MIB_H
#define SNMP_USM_DH_OBJECTS_MIB_H

config_add_mib(SNMP-USM-DH-OBJECTS-MIB);
config_require(snmp-usm-dh-objects-mib/usmDHUserKeyTable)
config_require(snmp-usm-dh-objects-mib/usmDHParameters)

#endif /* SNMP_USM_DH_OBJECTS_MIB_H */
