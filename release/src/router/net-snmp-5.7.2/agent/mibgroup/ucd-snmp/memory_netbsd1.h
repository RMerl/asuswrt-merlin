/*
 *  memory quantity mib groups
 *
 */
#ifndef _MIBGROUP_MEMORY_NETBSD1_H
#define _MIBGROUP_MEMORY_NETBSD1_H

config_require(util_funcs/header_generic)

#include "mibdefs.h"

extern void     init_memory_netbsd1(void);

#endif                          /* _MIBGROUP_MEMORY_NETBSD1_H */
