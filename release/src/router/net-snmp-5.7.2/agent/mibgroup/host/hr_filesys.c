/*
 *  Host Resources MIB - File System device group implementation - hr_filesys.c
 *
 */
/* Portions of this file are subject to the following copyright(s).  See
 * the Net-SNMP's COPYING file for more details and other copyrights
 * that may apply:
 */
/*
 * Portions of this file are copyrighted by:
 * Copyright (C) 2007 Apple, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/hardware/memory.h>
#include "host_res.h"
#include "hr_filesys.h"
#include "hr_storage.h"
#include "hr_disk.h"
#include <net-snmp/utilities.h>

#if HAVE_MNTENT_H
#include <mntent.h>
#endif
#if HAVE_SYS_MNTENT_H
#include <sys/mntent.h>
#endif
#if HAVE_SYS_MNTTAB_H
#include <sys/mnttab.h>
#endif
#if HAVE_SYS_STATVFS_H
#include <sys/statvfs.h>
#endif
#if HAVE_SYS_VFS_H
#include <sys/vfs.h>
#endif
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifdef HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif

#include <ctype.h>
#if HAVE_STRING_H
#include <string.h>
#endif
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif

#if HAVE_NBUTIL_H
#include <nbutil.h>
#endif

#if defined(aix4) || defined(aix5) || defined(aix6) || defined(aix7)
#include <sys/mntctl.h>
#include <sys/vmount.h>
#include <sys/statfs.h>
#endif

netsnmp_feature_require(se_find_free_value_in_slist)
netsnmp_feature_require(date_n_time)
netsnmp_feature_require(ctime_to_timet)

#if defined(bsdi4) || defined(freebsd3) || defined(freebsd4) || defined(freebsd5) || defined(darwin)
#if HAVE_GETFSSTAT && defined(MFSNAMELEN)
#define MOUNT_NFS	"nfs"
#define MNTTYPE_UFS	"ufs"
#define BerkelyFS
#define MNTTYPE_FFS	"ffs"
#define MNTTYPE_NFS	"nfs"
#define MNTTYPE_NFS3	"nfs"
#define MNTTYPE_HFS	"hfs"
#define MNTTYPE_MFS	"mfs"
#define MNTTYPE_MSDOS	"msdos"
#define MNTTYPE_LFS	"lfs"
#define MNTTYPE_FDESC	"fdesc"
#define MNTTYPE_PORTAL	"portal"
#define MNTTYPE_NULL	"null"
#define MNTTYPE_UMAP	"umap"
#define MNTTYPE_KERNFS	"kernfs"
#define MNTTYPE_PROCFS	"procfs"
#define MNTTYPE_AFS	"afs"
#define MNTTYPE_CD9660	"cd9660"
#define MNTTYPE_UNION	"union"
#define MNTTYPE_ADOSFS	"adosfs"
#define MNTTYPE_EXT2FS	"ext2fs"
#define MNTTYPE_CFS	"coda"
#define MNTTYPE_NTFS	"ntfs"
#endif
#endif                          /* freebsd3 */

#define HRFS_MONOTONICALLY_INCREASING

        /*********************
	 *
	 *  Kernel & interface information,
	 *   and internal forward declarations
	 *
	 *********************/

#ifdef solaris2

struct mnttab   HRFS_entry_struct;
struct mnttab  *HRFS_entry = &HRFS_entry_struct;
#define	HRFS_name	mnt_special
#define	HRFS_mount	mnt_mountp
#define	HRFS_type	mnt_fstype
#define	HRFS_statfs	statvfs

#elif defined(HAVE_STATVFS) && defined(__NetBSD__)

#if !defined(MFSNAMELEN) && defined(_VFS_NAMELEN)
#define MFSNAMELEN _VFS_NAMELEN
#endif

#define getfsstat getvfsstat

static struct statvfs	*fsstats = NULL;
struct statvfs		*HRFS_entry;
static int		fscount;
#define HRFS_mount	f_mntonname
#define	HRFS_name	f_mntfromname
#define HRFS_statfs	statvfs
#define	HRFS_type	f_fstypename
#elif defined(HAVE_GETFSSTAT) && !defined(HAVE_STATFS) && defined(HAVE_STATVFS)

static struct statfs	*fsstats = NULL;
struct statfs		*HRFS_entry;
static int		fscount;
#define HRFS_mount	f_mntonname
#define HRFS_name	f_mntfromname
#define HRFS_statfs	statvfs
#define HRFS_type	f_fstypename

#elif defined(HAVE_GETFSSTAT)
static struct statfs *fsstats = 0;
static int      fscount;
struct statfs  *HRFS_entry;
#define HRFS_statfs	statfs
#ifdef MFSNAMELEN
#define HRFS_type	f_fstypename
#else
#define HRFS_type	f_type
#endif
#define HRFS_mount	f_mntonname
#define HRFS_name	f_mntfromname

#elif defined(dynix)
struct mntent  *HRFS_entry;
#define	HRFS_name	mnt_fsname
#define	HRFS_mount	mnt_dir
#define	HRFS_type	mnt_type
#define	HRFS_statfs	statvfs

#elif defined(WIN32)
struct win_statfs	*HRFS_entry;
static int		fscount;
#define HRFS_mount	f_driveletter
#define	HRFS_name	f_fsid
#define HRFS_statfs	win_statfs
#define	HRFS_type	f_type

#elif defined(aix4) || defined(aix5) || defined(aix6) || defined(aix7)

struct vmount *aixmnt, *aixcurr;
struct HRFS_entry {
	char *HRFS_name;
	char *HRFS_mount;
	int HRFS_type;
	int HRFS_flags;
} *HRFS_entry;
#define HRFS_statfs statfs

#else
struct mntent  *HRFS_entry;
#define	HRFS_name	mnt_fsname
#define	HRFS_mount	mnt_dir
#define	HRFS_type	mnt_type
#define	HRFS_statfs	statfs

#ifdef linux
#define MNTTYPE_CD9660	"iso9660"
#define MNTTYPE_EXT2FS	"ext2"
#define MNTTYPE_EXT3FS	"ext3"
#define MNTTYPE_SMBFS	"smbfs"
#define MNTTYPE_MSDOS	"msdos"
#define MNTTYPE_FAT32	"vfat"
#define MNTTYPE_NTFS	"ntfs"
#define MNTTYPE_NFS4	"nfs4"
#endif	/* linux */

#endif

#define	FULL_DUMP	0
#define	PART_DUMP	1

extern void     Init_HR_FileSys(void);
extern int      Get_Next_HR_FileSys(void);
char           *cook_device(char *);
static u_char  *when_dumped(char *filesys, int level, size_t * length);
int             header_hrfilesys(struct variable *, oid *, size_t *, int,
                                 size_t *, WriteMethod **);

        /*********************
	 *
	 *  Initialisation & common implementation functions
	 *
	 *********************/

#define HRFSYS_INDEX		1
#define HRFSYS_MOUNT		2
#define HRFSYS_RMOUNT		3
#define HRFSYS_TYPE		4
#define HRFSYS_ACCESS		5
#define HRFSYS_BOOT		6
#define HRFSYS_STOREIDX		7
#define HRFSYS_FULLDUMP		8
#define HRFSYS_PARTDUMP		9

struct variable4 hrfsys_variables[] = {
    {HRFSYS_INDEX, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_hrfilesys, 2, {1, 1}},
    {HRFSYS_MOUNT, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_hrfilesys, 2, {1, 2}},
    {HRFSYS_RMOUNT, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_hrfilesys, 2, {1, 3}},
    {HRFSYS_TYPE, ASN_OBJECT_ID, NETSNMP_OLDAPI_RONLY,
     var_hrfilesys, 2, {1, 4}},
    {HRFSYS_ACCESS, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_hrfilesys, 2, {1, 5}},
    {HRFSYS_BOOT, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_hrfilesys, 2, {1, 6}},
    {HRFSYS_STOREIDX, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_hrfilesys, 2, {1, 7}},
    {HRFSYS_FULLDUMP, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_hrfilesys, 2, {1, 8}},
    {HRFSYS_PARTDUMP, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_hrfilesys, 2, {1, 9}},
};
oid             hrfsys_variables_oid[] = { 1, 3, 6, 1, 2, 1, 25, 3, 8 };

void
init_hr_filesys(void)
{
    REGISTER_MIB("host/hr_filesys", hrfsys_variables, variable4,
                 hrfsys_variables_oid);
#if defined(aix4) || defined(aix5) || defined(aix6) || defined(aix7)
    /* something leaks, make it idiot-safe */
    aixmnt = NULL;
    aixcurr = NULL;
#endif
}

/*
 * header_hrfilesys(...
 * Arguments:
 * vp     IN      - pointer to variable entry that points here
 * name    IN/OUT  - IN/name requested, OUT/name found
 * length  IN/OUT  - length of IN/OUT oid's 
 * exact   IN      - TRUE if an exact match was requested
 * var_len OUT     - length of variable or 0 if function returned
 * write_method
 * 
 */

int
header_hrfilesys(struct variable *vp,
                 oid * name,
                 size_t * length,
                 int exact, size_t * var_len, WriteMethod ** write_method)
{
#define HRFSYS_ENTRY_NAME_LENGTH	11
    oid             newname[MAX_OID_LEN];
    int             fsys_idx, LowIndex = -1;
    int             result;

    DEBUGMSGTL(("host/hr_filesys", "var_hrfilesys: "));
    DEBUGMSGOID(("host/hr_filesys", name, *length));
    DEBUGMSG(("host/hr_filesys", " %d\n", exact));

    memcpy((char *) newname, (char *) vp->name, vp->namelen * sizeof(oid));
    /*
     * Find "next" file system entry 
     */

    Init_HR_FileSys();
    for (;;) {
        fsys_idx = Get_Next_HR_FileSys();
        if (fsys_idx == -1)
            break;
        newname[HRFSYS_ENTRY_NAME_LENGTH] = fsys_idx;
        result = snmp_oid_compare(name, *length, newname, vp->namelen + 1);
        if (exact && (result == 0)) {
            LowIndex = fsys_idx;
            break;
        }
        if ((!exact && (result < 0)) &&
            (LowIndex == -1 || fsys_idx < LowIndex)) {
            LowIndex = fsys_idx;
#ifdef HRFS_MONOTONICALLY_INCREASING
            break;
#endif
        }
    }

    if (LowIndex == -1) {
        DEBUGMSGTL(("host/hr_filesys", "... index out of range\n"));
        return (MATCH_FAILED);
    }

    memcpy((char *) name, (char *) newname,
           (vp->namelen + 1) * sizeof(oid));
    *length = vp->namelen + 1;
    *write_method = (WriteMethod*)0;
    *var_len = sizeof(long);    /* default to 'long' results */

    DEBUGMSGTL(("host/hr_filesys", "... get filesys stats "));
    DEBUGMSGOID(("host/hr_filesys", name, *length));
    DEBUGMSG(("host/hr_filesys", "\n"));

    return LowIndex;
}


oid             fsys_type_id[] = { 1, 3, 6, 1, 2, 1, 25, 3, 9, 1 };     /* hrFSOther */
int             fsys_type_len =
    sizeof(fsys_type_id) / sizeof(fsys_type_id[0]);

        /*********************
	 *
	 *  System specific implementation functions
	 *
	 *********************/


u_char         *
var_hrfilesys(struct variable *vp,
              oid * name,
              size_t * length,
              int exact, size_t * var_len, WriteMethod ** write_method)
{
    int             fsys_idx;
    static char     string[1024];
    char           *mnt_type;

    fsys_idx =
        header_hrfilesys(vp, name, length, exact, var_len, write_method);
    if (fsys_idx == MATCH_FAILED)
        return NULL;


    switch (vp->magic) {
    case HRFSYS_INDEX:
        long_return = fsys_idx;
        return (u_char *) & long_return;
    case HRFSYS_MOUNT:
        strlcpy(string, HRFS_entry->HRFS_mount, sizeof(string));
        *var_len = strlen(string);
        return (u_char *) string;
    case HRFSYS_RMOUNT:
        if (Check_HR_FileSys_NFS())
            strlcpy(string, HRFS_entry->HRFS_name, sizeof(string));
        else
            string[0] = '\0';
        *var_len = strlen(string);
        return (u_char *) string;

    case HRFSYS_TYPE:
        if (Check_HR_FileSys_NFS())
            fsys_type_id[fsys_type_len - 1] = 14;
        else {
        /*
         * Not sufficient to identity the file
         *   type precisely, but it's a start.
         */
#if HAVE_GETFSSTAT && !defined(MFSNAMELEN)
        switch (HRFS_entry->HRFS_type) {
        case MOUNT_UFS:
            fsys_type_id[fsys_type_len - 1] = 3;
            break;
        case MOUNT_NFS:
            fsys_type_id[fsys_type_len - 1] = 14;
            break;
        case MOUNT_HFS:
            fsys_type_id[fsys_type_len - 1] = 7;
            break;
        case MOUNT_MFS:
            fsys_type_id[fsys_type_len - 1] = 8;
            break;
        case MOUNT_MSDOS:
            fsys_type_id[fsys_type_len - 1] = 5;
            break;
        case MOUNT_LFS:
            fsys_type_id[fsys_type_len - 1] = 1;
            break;
        case MOUNT_LOFS:
            fsys_type_id[fsys_type_len - 1] = 1;
            break;
        case MOUNT_FDESC:
            fsys_type_id[fsys_type_len - 1] = 1;
            break;
        case MOUNT_PORTAL:
            fsys_type_id[fsys_type_len - 1] = 1;
            break;
        case MOUNT_NULL:
            fsys_type_id[fsys_type_len - 1] = 1;
            break;
        case MOUNT_UMAP:
            fsys_type_id[fsys_type_len - 1] = 1;
            break;
        case MOUNT_KERNFS:
            fsys_type_id[fsys_type_len - 1] = 1;
            break;
        case MOUNT_PROCFS:
            fsys_type_id[fsys_type_len - 1] = 1;
            break;
        case MOUNT_AFS:
            fsys_type_id[fsys_type_len - 1] = 16;
            break;
        case MOUNT_CD9660:
            fsys_type_id[fsys_type_len - 1] = 12;
            break;
        case MOUNT_UNION:
            fsys_type_id[fsys_type_len - 1] = 1;
            break;
        case MOUNT_DEVFS:
            fsys_type_id[fsys_type_len - 1] = 1;
            break;
#ifdef MOUNT_EXT2FS
        case MOUNT_EXT2FS:
            fsys_type_id[fsys_type_len - 1] = 23;
            break;
#endif
#ifdef MOUNT_TFS
        case MOUNT_TFS:
            fsys_type_id[fsys_type_len - 1] = 15;
            break;
#endif
        }
#elif defined(aix4) || defined(aix5) || defined(aix6) || defined(aix7)
        switch (HRFS_entry->HRFS_type) {
        case MNT_AIX:
        case MNT_JFS:
            fsys_type_id[fsys_type_len - 1] = 3;
            break;
        case MNT_CDROM:
            fsys_type_id[fsys_type_len - 1] = 12;
            break;
#ifdef MNT_NAMEFS
        case MNT_NAMEFS:
#endif
#ifdef MNT_PROCFS
        case MNT_PROCFS:
#endif
        case MNT_SFS:
        case MNT_CACHEFS:
            fsys_type_id[fsys_type_len - 1] = 1;
            break;
        case MNT_NFS:
        case MNT_NFS3:
        case MNT_AUTOFS:
            fsys_type_id[fsys_type_len - 1] = 14;
            break;
        }
#else
        mnt_type = HRFS_entry->HRFS_type;
        if (mnt_type == NULL)
            fsys_type_id[fsys_type_len - 1] = 2;        /* unknown */
#ifdef MNTTYPE_HFS
        else if (!strcmp(mnt_type, MNTTYPE_HFS))
            fsys_type_id[fsys_type_len - 1] = 7;
#endif
#ifdef MNTTYPE_UFS
        else if (!strcmp(mnt_type, MNTTYPE_UFS))
#if (defined(BerkelyFS) && !defined(MNTTYPE_HFS)) || defined(solaris2)
            fsys_type_id[fsys_type_len - 1] = 3;
#else                           /* SysV */
            fsys_type_id[fsys_type_len - 1] = 4;        /* or 3? XXX */
#endif
#endif
#ifdef MNTTYPE_SYSV
        else if (!strcmp(mnt_type, MNTTYPE_SYSV))
            fsys_type_id[fsys_type_len - 1] = 4;
#endif
#ifdef MNTTYPE_PC
        else if (!strcmp(mnt_type, MNTTYPE_PC))
            fsys_type_id[fsys_type_len - 1] = 5;
#endif
#ifdef MNTTYPE_MSDOS
        else if (!strcmp(mnt_type, MNTTYPE_MSDOS))
            fsys_type_id[fsys_type_len - 1] = 5;
#endif
#ifdef MNTTYPE_FAT32
        else if (!strcmp(mnt_type, MNTTYPE_FAT32))
            fsys_type_id[fsys_type_len - 1] = 22;
#endif
#ifdef MNTTYPE_CDFS
        else if (!strcmp(mnt_type, MNTTYPE_CDFS))
#ifdef RockRidge
            fsys_type_id[fsys_type_len - 1] = 13;
#else                           /* ISO 9660 */
            fsys_type_id[fsys_type_len - 1] = 12;
#endif
#endif
#ifdef MNTTYPE_HSFS
        else if (!strcmp(mnt_type, MNTTYPE_HSFS))
            fsys_type_id[fsys_type_len - 1] = 13;
#endif
#ifdef MNTTYPE_ISO9660
        else if (!strcmp(mnt_type, MNTTYPE_ISO9660))
            fsys_type_id[fsys_type_len - 1] = 12;
#endif
#ifdef MNTTYPE_CD9660
        else if (!strcmp(mnt_type, MNTTYPE_CD9660))
            fsys_type_id[fsys_type_len - 1] = 12;
#endif
#ifdef MNTTYPE_SMBFS
        else if (!strcmp(mnt_type, MNTTYPE_SMBFS))
            fsys_type_id[fsys_type_len - 1] = 14;
#endif
#ifdef MNTTYPE_NFS
        else if (!strcmp(mnt_type, MNTTYPE_NFS))
            fsys_type_id[fsys_type_len - 1] = 14;
#endif
#ifdef MNTTYPE_NFS3
        else if (!strcmp(mnt_type, MNTTYPE_NFS3))
            fsys_type_id[fsys_type_len - 1] = 14;
#endif
#ifdef MNTTYPE_NFS4
        else if (!strcmp(mnt_type, MNTTYPE_NFS4))
            fsys_type_id[fsys_type_len - 1] = 14;
#endif
#ifdef MNTTYPE_MFS
        else if (!strcmp(mnt_type, MNTTYPE_MFS))
            fsys_type_id[fsys_type_len - 1] = 8;
#endif
#ifdef MNTTYPE_EXT2FS
        else if (!strcmp(mnt_type, MNTTYPE_EXT2FS))
            fsys_type_id[fsys_type_len - 1] = 23;
#endif
#ifdef MNTTYPE_EXT3FS
        else if (!strcmp(mnt_type, MNTTYPE_EXT3FS))
            fsys_type_id[fsys_type_len - 1] = 23;
#endif
#ifdef MNTTYPE_NTFS
        else if (!strcmp(mnt_type, MNTTYPE_NTFS))
            fsys_type_id[fsys_type_len - 1] = 9;
#endif
        else
            fsys_type_id[fsys_type_len - 1] = 1;        /* Other */
#endif                          /* HAVE_GETFSSTAT */
        }

        *var_len = sizeof(fsys_type_id);
        return (u_char *) fsys_type_id;

    case HRFSYS_ACCESS:
#if defined(HAVE_STATVFS) && defined(__NetBSD__)
	long_return = HRFS_entry->f_flag & MNT_RDONLY ? 2 : 1;
#elif defined(HAVE_GETFSSTAT)
#if HAVE_STRUCT_STATFS_F_FLAGS
        long_return = HRFS_entry->f_flags & MNT_RDONLY ? 2 : 1;
#else
        long_return = HRFS_entry->f_flag & MNT_RDONLY ? 2 : 1;
#endif
#elif defined(cygwin)
        long_return = 1;
#elif defined(aix4) || defined(aix5) || defined(aix6) || defined(aix7)
        long_return = (HRFS_entry->HRFS_flags & MNT_READONLY) == 0 ? 1 : 2;
#else
#if HAVE_HASMNTOPT
        if (hasmntopt(HRFS_entry, "ro") != NULL)
            long_return = 2;    /* Read Only */
        else
#endif
            long_return = 1;    /* Read-Write */
#endif
        return (u_char *) & long_return;
    case HRFSYS_BOOT:
        if (HRFS_entry->HRFS_mount[0] == '/' &&
            HRFS_entry->HRFS_mount[1] == 0)
            long_return = 1;    /* root is probably bootable! */
        else
            long_return = 2;    /* others probably aren't */
        return (u_char *) & long_return;
    case HRFSYS_STOREIDX:
        long_return = fsys_idx + NETSNMP_MEM_TYPE_MAX;
        return (u_char *) & long_return;
    case HRFSYS_FULLDUMP:
        return when_dumped(HRFS_entry->HRFS_name, FULL_DUMP, var_len);
    case HRFSYS_PARTDUMP:
        return when_dumped(HRFS_entry->HRFS_name, PART_DUMP, var_len);
    default:
        DEBUGMSGTL(("snmpd", "unknown sub-id %d in var_hrfilesys\n",
                    vp->magic));
    }
    return NULL;
}


        /*********************
	 *
	 *  Internal implementation functions
	 *
	 *********************/

static int      HRFS_index;
#ifndef HAVE_GETFSSTAT
static FILE    *fp;
#endif

void
Init_HR_FileSys(void)
{
#if HAVE_GETFSSTAT
#if defined(HAVE_STATVFS) && defined(__NetBSD__)
    fscount = getvfsstat(NULL, 0, ST_NOWAIT);
#else
    fscount = getfsstat(NULL, 0, MNT_NOWAIT);
#endif
    if (fsstats)
        free((char *) fsstats);
    fsstats = NULL;
    fsstats = malloc(fscount * sizeof(*fsstats));
#if defined(HAVE_STATVFS) && defined(__NetBSD__)
    getvfsstat(fsstats, fscount * sizeof(*fsstats), ST_NOWAIT);
#else
    getfsstat(fsstats, fscount * sizeof(*fsstats), MNT_NOWAIT);
#endif
    HRFS_index = 0;
#elif defined(aix4) || defined(aix5) || defined(aix6) || defined(aix7)
    int ret;
    uint size;
    ret = 0;
    size = 0;
    if(aixmnt != NULL) free(aixmnt); /* something leaks, make it idiot-safe */
    aixmnt = NULL;
    aixcurr = NULL;
    HRFS_index = 0;
    ret = mntctl(MCTL_QUERY, sizeof(uint), &size);
    if(ret == 0 && size > 0) {
        aixmnt = malloc(size + sizeof(struct HRFS_entry));
        if(aixmnt != NULL) {
            HRFS_entry = (char *) aixmnt + size;
            ret = mntctl(MCTL_QUERY, size, aixmnt);
            HRFS_index = 1;
            if(ret <= 0) {
                free(aixmnt);
                aixmnt = NULL;
                HRFS_entry = NULL;
            } else aixcurr = aixmnt;
        }
    }
#else
    HRFS_index = 1;
    if (fp != NULL)
        fclose(fp);
    fp = fopen(ETC_MNTTAB, "r");
    if (!fp) {
      netsnmp_config_error("Can't open mnttab %s\n", ETC_MNTTAB);
    }
#endif
}

const char     *HRFS_ignores[] = {
#ifdef MNTTYPE_IGNORE
    MNTTYPE_IGNORE,
#endif
#ifdef MNTTYPE_SWAP
    MNTTYPE_SWAP,
#endif
#ifdef MNTTYPE_PROC
    MNTTYPE_PROC,
#endif
#ifdef MNTTYPE_PROCFS
    MNTTYPE_PROCFS,
#endif
#ifdef MNTTYPE_AUTOFS
    MNTTYPE_AUTOFS,
#else
    "autofs",
#endif
#ifdef linux
    "autofs",
    "bdev",
    "binfmt_misc",
    "cpuset",
    "debugfs",
    "devfs",
    "devpts",
    "eventpollfs",
    "futexfs",
    "hugetlbfs",
    "inotifyfs",
    "mqueue",
    "nfsd",
    "pipefs",
    "proc",
    "ramfs",
    "rootfs",
    "rpc_pipefs",
    "securityfs",
    "shm",
    "sockfs",
    "sysfs",
    "tmpfs",
    "usbdevfs",
    "usbfs",
#endif
#ifdef solaris2
    "mntfs",
    "proc",
    "fd",
#endif
    NULL
};

int
Get_Next_HR_FileSys(void)
{
#if HAVE_GETFSSTAT
    if (HRFS_index >= fscount)
        return -1;
    HRFS_entry = fsstats + HRFS_index;
    return ++HRFS_index;
#elif defined(aix4) || defined(aix5) || defined(aix6) || defined(aix7)
    if(aixcurr == NULL) {
        if(aixmnt != NULL) free(aixmnt);
        aixmnt = NULL;
        HRFS_entry = NULL;
        return -1;
    }
    HRFS_entry->HRFS_name = vmt2dataptr(aixcurr, VMT_OBJECT);
    HRFS_entry->HRFS_mount = vmt2dataptr(aixcurr, VMT_STUB);
    HRFS_entry->HRFS_type = aixcurr->vmt_gfstype;
    HRFS_entry->HRFS_flags = aixcurr->vmt_flags;
    aixcurr = (char *) aixcurr + aixcurr->vmt_length;
    if((char *) aixcurr >= (char *) HRFS_entry) aixcurr = NULL;
    switch(HRFS_entry->HRFS_type) {
#ifdef MNT_NAMEFS
        case MNT_NAMEFS:
#endif
#ifdef MNT_PROCFS
        case MNT_PROCFS:
#endif
        case MNT_SFS:
            return Get_Next_HR_FileSys();
            break;
    }
    return HRFS_index++;
#else
    const char    **cpp;

    if (fp == NULL)
        return -1;

#ifdef solaris2
    if (getmntent(fp, HRFS_entry) != 0)
        return -1;
#else
    HRFS_entry = getmntent(fp);
    if (HRFS_entry == NULL)
        return -1;
#endif                          /* solaris2 */

    for (cpp = HRFS_ignores; *cpp != NULL; ++cpp)
        if (!strcmp(HRFS_entry->HRFS_type, *cpp))
            return Get_Next_HR_FileSys();

    /*
     * Try and ensure that index values are persistent
     *   at least within a single run of the agent
     */
    HRFS_index = se_find_value_in_slist("filesys", HRFS_entry->HRFS_name );
    if (HRFS_index == SE_DNE) {
        HRFS_index = se_find_free_value_in_slist("filesys");
        if (HRFS_index == SE_DNE) { HRFS_index = 1; }
        se_add_pair_to_slist( "filesys",
                              strdup( HRFS_entry->HRFS_name ), HRFS_index);
    }

    return HRFS_index++;
#endif                          /* HAVE_GETFSSTAT */
}

/*
 * this function checks whether the current file system (info can be found
 * in HRFS_entry) is a NFS file system
 * HRFS_entry must be valid prior to calling this function
 * returns 1 if NFS file system, 0 otherwise
 */
int
Check_HR_FileSys_NFS (void)
{
#if HAVE_GETFSSTAT && !defined(MFSNAMELEN)
    if ((HRFS_entry->HRFS_type == MOUNT_NFS) ||
        (HRFS_entry->HRFS_type == MOUNT_AFS))
#elif defined(aix4) || defined(aix5) || defined(aix6) || defined(aix7)
    if(HRFS_entry->HRFS_type == MNT_NFS || HRFS_entry->HRFS_type == MNT_NFS3)
#else /* HAVE_GETFSSTAT */
    if ( HRFS_entry->HRFS_type != NULL && (
#if defined(MNTTYPE_NFS)
	!strcmp( HRFS_entry->HRFS_type, MNTTYPE_NFS) ||
#else
	!strcmp( HRFS_entry->HRFS_type, "nfs") ||
#endif
#if defined(MNTTYPE_NFS3)
	!strcmp( HRFS_entry->HRFS_type, MNTTYPE_NFS3) ||
#endif
#if defined(MNTTYPE_NFS4)
	!strcmp( HRFS_entry->HRFS_type, MNTTYPE_NFS4) ||
#endif
#if defined(MNTTYPE_SMBFS)
	!strcmp( HRFS_entry->HRFS_type, MNTTYPE_SMBFS) ||
#endif
#if defined(MNTTYPE_LOFS)
	!strcmp( HRFS_entry->HRFS_type, MNTTYPE_LOFS) ||
#endif
#if defined(MNTTYPE_AFP)
	!strcmp( HRFS_entry->HRFS_type, MNTTYPE_AFP) ||
#endif
	!strcmp( HRFS_entry->HRFS_type, "cifs") ||
	    /*
	     * MVFS is Rational ClearCase's view file system
	     * it is similiar to NFS file systems in that it is mounted
	     * locally or remotely from the ClearCase server
	     */
	!strcmp( HRFS_entry->HRFS_type, "mvfs")))
#endif /* HAVE_GETFSSTAT */
	return 1;	/* NFS file system */

    return 0;		/* no NFS file system */
}

void
End_HR_FileSys(void)
{
#ifdef HAVE_GETFSSTAT
    if (fsstats)
        free((char *) fsstats);
    fsstats = NULL;
#elif defined(aix4) || defined(aix5) || defined(aix6) || defined(aix7)
    if(aixmnt != NULL) {
        free(aixmnt);
        aixmnt = NULL;
        aixcurr = NULL;
        HRFS_entry = NULL;
    }
#else
    if (fp != NULL)
        fclose(fp);
    fp = NULL;
#endif
}


static u_char  *
when_dumped(char *filesys, int level, size_t * length)
{
    time_t          dumpdate = 0, tmp;
    FILE           *dump_fp;
    char            line[1024];
    char           *cp1, *cp2, *cp3;

    /*
     * Look for the relevent entries in /etc/dumpdates
     *
     * This is complicated by the fact that disks are
     *   mounted using block devices, but dumps are
     *   done via the raw character devices.
     * Thus the device names in /etc/dumpdates and
     *   /etc/mnttab don't match.
     *   These comparisons are therefore made using the
     *   final portion of the device name only.
     */

    if (*filesys == '\0')       /* No filesystem name? */
        return date_n_time(NULL, length);
    cp1 = strrchr(filesys, '/');        /* Find the last element of the current FS */

    if (cp1 == NULL)
        cp1 = filesys;

    if ((dump_fp = fopen("/etc/dumpdates", "r")) == NULL)
        return date_n_time(NULL, length);

    while (fgets(line, sizeof(line), dump_fp) != NULL) {
        cp2 = strchr(line, ' ');        /* Start by looking at the device name only */
        if (cp2 != NULL) {
            *cp2 = '\0';
            cp3 = strrchr(line, '/');   /* and find the last element */
            if (cp3 == NULL)
                cp3 = line;

            if (strcmp(cp1, cp3) != 0)  /* Wrong FS */
                continue;

            ++cp2;
            while (isspace(*cp2 & 0xFF))
                ++cp2;          /* Now find the dump level */

            if (level == FULL_DUMP) {
                if (*(cp2++) != '0')
                    continue;   /* Not interested in partial dumps */
                while (isspace(*cp2 & 0xFF))
                    ++cp2;

                dumpdate = ctime_to_timet(cp2);
                fclose(dump_fp);
                return date_n_time(&dumpdate, length);
            } else {            /* Partial Dump */
                if (*(cp2++) == '0')
                    continue;   /* Not interested in full dumps */
                while (isspace(*cp2 & 0xFF))
                    ++cp2;

                tmp = ctime_to_timet(cp2);
                if (tmp > dumpdate)
                    dumpdate = tmp;     /* Remember the 'latest' partial dump */
            }
        }
    }

    fclose(dump_fp);

    return date_n_time(&dumpdate, length);
}


#define RAW_DEVICE_PREFIX	"/dev/rdsk"
#define COOKED_DEVICE_PREFIX	"/dev/dsk"

char           *
cook_device(char *dev)
{
    static char     cooked_dev[SNMP_MAXPATH+1];

    if (!strncmp(dev, RAW_DEVICE_PREFIX, strlen(RAW_DEVICE_PREFIX))) {
        strlcpy(cooked_dev, COOKED_DEVICE_PREFIX, sizeof(cooked_dev));
        strlcat(cooked_dev, dev + strlen(RAW_DEVICE_PREFIX),
                sizeof(cooked_dev));
    } else {
        strlcpy(cooked_dev, dev, sizeof(cooked_dev));
    }

    return cooked_dev;
}


int
Get_FSIndex(char *dev)
{
    int             iindex;

    Init_HR_FileSys();

    while ((iindex = Get_Next_HR_FileSys()) != -1)
        if (!strcmp(HRFS_entry->HRFS_name, cook_device(dev))) {
            End_HR_FileSys();
            return iindex;
        }

    End_HR_FileSys();
    return 0;
}

long
Get_FSSize(char *dev)
{
    struct HRFS_statfs statfs_buf;

    Init_HR_FileSys();

    while (Get_Next_HR_FileSys() != -1)
        if (!strcmp(HRFS_entry->HRFS_name, cook_device(dev))) {
            End_HR_FileSys();

            if (HRFS_statfs(HRFS_entry->HRFS_mount, &statfs_buf) != -1)
  		/*
  		 * with large file systems the following calculation produces
  		 * an overflow:
  		 * (statfs_buf.f_blocks*statfs_buf.f_bsize)/1024
  		 *
  		 * assumption: f_bsize is either 512 or a multiple of 1024
  		 * in case of 512 (f_blocks/2) is returned
  		 * otherwise (f_blocks*(f_bsize/1024)) is returned
  		 */
#if defined(solaris2) && defined(HAVE_STRUCT_STATVFS_F_FRSIZE)
                return (statfs_buf.f_blocks*(statfs_buf.f_frsize/1024));
#else
  		if (statfs_buf.f_bsize == 512)
  		    return (statfs_buf.f_blocks/2);
                else
  		    return (statfs_buf.f_blocks*(statfs_buf.f_bsize/1024));
#endif
            else
                return -1;
        }

    End_HR_FileSys();
    return 0;
}
