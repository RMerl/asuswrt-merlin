/*
 *  Host Resources MIB - partition device group interface - hr_partition.h
 *
 */
#ifndef _MIBGROUP_HRPART_H
#define _MIBGROUP_HRPART_H

config_require(host/hr_disk)

extern void     init_hr_partition(void);
extern FindVarMethod var_hrpartition;


#endif                          /* _MIBGROUP_HRPART_H */
