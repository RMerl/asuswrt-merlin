/*
 *  vmstat mib groups
 *
 */
#ifndef _MIBGROUP_VMSTAT_NETBSD1_H
#define _MIBGROUP_VMSTAT_NETBSD1_H

config_require(util_funcs/header_generic)

#include "mibdefs.h"

void            init_vmstat_netbsd1(void);

#endif                          /* _MIBGROUP_VMSTAT_NETBSD1_H */
