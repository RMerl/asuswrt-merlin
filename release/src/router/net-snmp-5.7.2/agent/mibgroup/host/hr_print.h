/*
 *  Host Resources MIB - printer device group interface - hr_print.h
 *
 */
#ifndef _MIBGROUP_HRPRINT_H
#define _MIBGROUP_HRPRINT_H

config_require(host/hr_device)
config_require(util_funcs)

extern void     init_hr_print(void);
extern FindVarMethod var_hrprint;


#endif                          /* _MIBGROUP_HRPRINT_H */
