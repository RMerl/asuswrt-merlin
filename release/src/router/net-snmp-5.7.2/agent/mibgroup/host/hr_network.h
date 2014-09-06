/*
 *  Host Resources MIB - network device group interface - hr_network.h
 *
 */
#ifndef _MIBGROUP_HRNET_H
#define _MIBGROUP_HRNET_H

config_require(host/hr_device)

extern void     init_hr_network(void);
extern FindVarMethod var_hrnet;

config_require(host/hr_device mibII/ifTable)
#endif                          /* _MIBGROUP_HRNET_H */
