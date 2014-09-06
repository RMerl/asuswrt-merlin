/*
 *  Host Resources MIB - storage group interface - hr_storage.h
 *
 */
#ifndef _MIBGROUP_HRSTORAGE_H
#define _MIBGROUP_HRSTORAGE_H

config_require(hardware/memory)
/* config_require(host/hr_filesys) */

extern void     init_hr_storage(void);
extern FindVarMethod var_hrstore;


#define	HRS_TYPE_MBUF		1
#define	HRS_TYPE_MEM		2
#define	HRS_TYPE_SWAP		3
#define	HRS_TYPE_FIXED_MAX	3     /* the largest fixed type */

#ifdef WIN32
/* linux-compatible values for fs type */
#define MSDOS_SUPER_MAGIC     0x4d44
#define NTFS_SUPER_MAGIC      0x5346544E

/* Define the statfs structure for Windows. */
struct win_statfs {
   long    f_type;     /* type of filesystem */
   long    f_bsize;    /* optimal transfer block size */
   long    f_blocks;   /* total data blocks in file system */
   long    f_bfree;    /* free blocks in fs */
   long    f_bavail;   /* free blocks avail to non-superuser */
   long    f_files;    /* total file nodes in file system */
   long    f_ffree;    /* free file nodes in fs */
   long    f_fsid;     /* file system id */
   long    f_namelen;  /* maximum length of filenames */
   long    f_spare[6]; /* spare for later */
   char	   f_driveletter[6];
};

static int win_statfs (const char *path, struct win_statfs *buf);
#endif /* WIN32*/

#endif                          /* _MIBGROUP_HRSTORAGE_H */
