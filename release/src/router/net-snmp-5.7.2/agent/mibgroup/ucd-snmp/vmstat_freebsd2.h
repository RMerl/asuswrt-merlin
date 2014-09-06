/*
 *  vmstat mib groups
 *
 */
#ifndef _MIBGROUP_VMSTAT_FREEBSD2_H
#define _MIBGROUP_VMSTAT_FREEBSD2_H

config_require(util_funcs/header_generic)

#include "mibdefs.h"

void            init_vmstat_freebsd2(void);

#endif                          /* _MIBGROUP_VMSTAT_FREEBSD2_H */
