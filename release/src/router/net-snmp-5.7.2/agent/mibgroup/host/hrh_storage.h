/*
 *  Host Resources MIB - storage group interface (HAL rewrite) - hrh_storage.h
 *
 */
#ifndef _MIBGROUP_HRSTORAGE_H
#define _MIBGROUP_HRSTORAGE_H

config_require(hardware/memory)
config_require(hardware/fsys)
config_require(host/hrh_filesys)

config_exclude( host/hr_storage )

extern void     init_hrh_storage(void);
extern FindVarMethod var_hrstore;


#define	HRS_TYPE_MBUF		1
#define	HRS_TYPE_MEM		2
#define	HRS_TYPE_SWAP		3
#define	HRS_TYPE_FIXED_MAX	3     /* the largest fixed type */

#endif                          /* _MIBGROUP_HRSTORAGE_H */
