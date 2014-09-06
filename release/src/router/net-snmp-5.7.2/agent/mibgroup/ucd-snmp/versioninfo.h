/*
 *  Template MIB group interface - versioninfo.h
 *
 */
#ifndef _MIBGROUP_VERSIONINFO_H
#define _MIBGROUP_VERSIONINFO_H

void            init_versioninfo(void);

extern FindVarMethod var_extensible_version;
extern WriteMethod update_hook;
extern WriteMethod debugging_hook;
extern WriteMethod save_persistent;

#include "mibdefs.h"

/*
 * Version info mib 
 */
#define VERTAG 2
#define VERDATE 3
#define VERCDATE 4
#define VERIDENT 5
#define VERCONFIG 6
#define VERCLEARCACHE 10
#define VERUPDATECONFIG 11
#define VERRESTARTAGENT 12
#define VERSAVEPERSISTENT 13
#define VERDEBUGGING 20

config_require(util_funcs/header_generic)
config_require(util_funcs/restart)
config_require(util_funcs)

#endif                          /* _MIBGROUP_VERSIONINFO_H */
