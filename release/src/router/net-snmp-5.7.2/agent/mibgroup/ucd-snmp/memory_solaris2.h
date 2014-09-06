/*
 *  memory quantity mib groups
 *
 */
#ifndef _MIBGROUP_MEMORY_SOLARIS2_H
#define _MIBGROUP_MEMORY_SOLARIS2_H

config_require(util_funcs/header_generic)

#include "mibdefs.h"

void            init_memory_solaris2(void);

#endif                          /* _MIBGROUP_MEMORY_SOLARIS2_H */
