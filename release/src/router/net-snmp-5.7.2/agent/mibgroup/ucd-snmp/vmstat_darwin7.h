/*
 *  vmstat mib groups
 *
 */
#ifndef _MIBGROUP_VMSTAT_DARWIN7_H
#define _MIBGROUP_VMSTAT_DARWIN7_H

config_require(util_funcs/header_generic)

#include "mibdefs.h"

void            init_vmstat_darwin7(void);

#endif                          /* _MIBGROUP_VMSTAT_DARWIN7_H */
