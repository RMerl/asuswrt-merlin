/*
 *  Host Resources MIB - disk device group interface - hr_disk.h
 *
 */
#ifndef _MIBGROUP_HRDISK_H
#define _MIBGROUP_HRDISK_H

config_require(host/hr_device)

extern void     init_hr_disk(void);
extern void     shutdown_hr_disk(void);

extern void     Init_HR_Disk(void);
extern int      Get_Next_HR_Disk(void);
extern int      Get_Next_HR_Disk_Partition(char *, size_t, int);
extern FindVarMethod var_hrdisk;


#endif                          /* _MIBGROUP_HRDISK_H */
