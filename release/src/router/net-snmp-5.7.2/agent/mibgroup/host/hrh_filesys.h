/*
 *  Host Resources MIB - file system device group interface (HAL rewrite) - hrh_filesys.h
 *
 */
#ifndef _MIBGROUP_HRFSYS_H
#define _MIBGROUP_HRFSYS_H

extern void     init_hrh_filesys(void);
extern void     Init_HR_FileSys(void);
extern FindVarMethod var_hrhfilesys;
extern int      Get_Next_HR_FileSys(void);
extern int      Check_HR_FileSys_NFS(void);

extern int      Get_FSIndex(char *);
extern long     Get_FSSize(char *);     /* Temporary */

config_exclude( host/hr_filesys )

#endif                          /* _MIBGROUP_HRFSYS_H */
