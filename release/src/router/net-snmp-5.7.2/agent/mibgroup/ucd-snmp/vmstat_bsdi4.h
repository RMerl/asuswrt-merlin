/*
 *  vmstat mib groups
 *
 */
#ifndef _MIBGROUP_VMSTAT_BSDI4_H
#define _MIBGROUP_VMSTAT_BSDI4_H

config_require(util_funcs/header_generic)

#include "mibdefs.h"

void            init_vmstat_bsdi4(void);

#endif                          /* _MIBGROUP_VMSTAT_BSDI4_H */
