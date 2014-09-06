#ifndef _MIBGROUP_SYSTEM_MIB_H
#define _MIBGROUP_SYSTEM_MIB_H

#ifdef __cplusplus
extern "C" {
#endif

config_require(util_funcs mibII/updates)

void init_system_mib(void);

#ifdef __cplusplus
}
#endif

#endif                          /* _MIBGROUP_SYSTEM_MIB_H */
