/*
 *  Host Resources MIB - File System device group implementation (HAL rewrite) - hrh_filesys.c
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
#include <net-snmp/agent/hardware/fsys.h>
#include "host_res.h"
#include "hrh_filesys.h"
#include "hrh_storage.h"
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

#if defined(aix4) || defined(aix5) || defined(aix6) || defined(aix7)
#include <sys/mntctl.h>
#include <sys/vmount.h>
#include <sys/statfs.h>
#endif

netsnmp_feature_require(date_n_time)
netsnmp_feature_require(ctime_to_timet)

#define HRFS_MONOTONICALLY_INCREASING

        /*********************
	 *
	 *  Kernel & interface information,
	 *   and internal forward declarations
	 *
	 *********************/
netsnmp_fsys_info *HRFS_entry;

#define	FULL_DUMP	0
#define	PART_DUMP	1

extern void     Init_HR_FileSys(void);
extern int      Get_Next_HR_FileSys(void);
char           *cook_device(char *);
static u_char  *when_dumped(char *filesys, int level, size_t * length);
int             header_hrhfilesys(struct variable *, oid *, size_t *, int,
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
    {HRFSYS_INDEX,    ASN_INTEGER,   NETSNMP_OLDAPI_RONLY,
     var_hrhfilesys, 2, {1, 1}},
    {HRFSYS_MOUNT,    ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_hrhfilesys, 2, {1, 2}},
    {HRFSYS_RMOUNT,   ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_hrhfilesys, 2, {1, 3}},
    {HRFSYS_TYPE,     ASN_OBJECT_ID, NETSNMP_OLDAPI_RONLY,
     var_hrhfilesys, 2, {1, 4}},
    {HRFSYS_ACCESS,   ASN_INTEGER,   NETSNMP_OLDAPI_RONLY,
     var_hrhfilesys, 2, {1, 5}},
    {HRFSYS_BOOT,     ASN_INTEGER,   NETSNMP_OLDAPI_RONLY,
     var_hrhfilesys, 2, {1, 6}},
    {HRFSYS_STOREIDX, ASN_INTEGER,   NETSNMP_OLDAPI_RONLY,
     var_hrhfilesys, 2, {1, 7}},
    {HRFSYS_FULLDUMP, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_hrhfilesys, 2, {1, 8}},
    {HRFSYS_PARTDUMP, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_hrhfilesys, 2, {1, 9}},
};
oid             hrfsys_variables_oid[] = { 1, 3, 6, 1, 2, 1, 25, 3, 8 };

void
init_hrh_filesys(void)
{
    REGISTER_MIB("host/hr_filesys", hrfsys_variables, variable4,
                 hrfsys_variables_oid);
}

/*
 * header_hrhfilesys(...
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
header_hrhfilesys(struct variable *vp,
                 oid * name,
                 size_t * length,
                 int exact, size_t * var_len, WriteMethod ** write_method)
{
#define HRFSYS_ENTRY_NAME_LENGTH	11
    oid             newname[MAX_OID_LEN];
    int             fsys_idx, LowIndex = -1;
    int             result;

    DEBUGMSGTL(("host/hr_filesys", "var_hrhfilesys: "));
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
    *write_method = 0;
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
var_hrhfilesys(struct variable *vp,
              oid * name,
              size_t * length,
              int exact, size_t * var_len, WriteMethod ** write_method)
{
    int             fsys_idx;
    static char     string[1024];

    fsys_idx =
        header_hrhfilesys(vp, name, length, exact, var_len, write_method);
    if (fsys_idx == MATCH_FAILED)
        return NULL;

    switch (vp->magic) {
    case HRFSYS_INDEX:
        long_return = fsys_idx;
        return (u_char *) & long_return;
    case HRFSYS_MOUNT:
        snprintf(string, sizeof(string), "%s", HRFS_entry->path);
        string[ sizeof(string)-1 ] = 0;
        *var_len = strlen(string);
        return (u_char *) string;
    case HRFSYS_RMOUNT:
        if (HRFS_entry->flags & NETSNMP_FS_FLAG_REMOTE) {
            snprintf(string, sizeof(string), "%s", HRFS_entry->device);
            string[ sizeof(string)-1 ] = 0;
        } else
            string[0] = '\0';
        *var_len = strlen(string);
        return (u_char *) string;

    case HRFSYS_TYPE:
        fsys_type_id[fsys_type_len - 1] = 
            (HRFS_entry->type > _NETSNMP_FS_TYPE_LOCAL ?
                                 NETSNMP_FS_TYPE_OTHER : HRFS_entry->type);
        *var_len = sizeof(fsys_type_id);
        return (u_char *) fsys_type_id;

    case HRFSYS_ACCESS:
	long_return = HRFS_entry->flags & NETSNMP_FS_FLAG_RONLY ? 2 : 1;
        return (u_char *) & long_return;
    case HRFSYS_BOOT:
	long_return = HRFS_entry->flags & NETSNMP_FS_FLAG_BOOTABLE ? 1 : 2;
        return (u_char *) & long_return;
    case HRFSYS_STOREIDX:
        long_return = fsys_idx + NETSNMP_MEM_TYPE_MAX;
        return (u_char *) & long_return;
    case HRFSYS_FULLDUMP:
        return when_dumped(HRFS_entry->path, FULL_DUMP, var_len);
    case HRFSYS_PARTDUMP:
        return when_dumped(HRFS_entry->path, PART_DUMP, var_len);
    default:
        DEBUGMSGTL(("snmpd", "unknown sub-id %d in var_hrhfilesys\n",
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

void
Init_HR_FileSys(void)
{
    netsnmp_cache *c = netsnmp_fsys_get_cache();
    netsnmp_cache_check_and_reload( c );

    HRFS_entry = NULL;
    HRFS_index = 0;
}

int
Get_Next_HR_FileSys(void)
{
    if ( HRFS_entry ) {
        HRFS_entry = netsnmp_fsys_get_next( HRFS_entry );
    } else {     
        HRFS_entry = netsnmp_fsys_get_first();
    }
    /* Skip "inactive" entries */
    while ( HRFS_entry && !(HRFS_entry->flags & NETSNMP_FS_FLAG_ACTIVE))
        HRFS_entry = netsnmp_fsys_get_next( HRFS_entry );

    HRFS_index = (HRFS_entry ? HRFS_entry->idx.oids[0] : -1 );
    return HRFS_index;
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
            while (isspace(0xFF & *cp2))
                ++cp2;          /* Now find the dump level */

            if (level == FULL_DUMP) {
                if (*(cp2++) != '0')
                    continue;   /* Not interested in partial dumps */
                while (isspace(0xFF & *cp2))
                    ++cp2;

                dumpdate = ctime_to_timet(cp2);
                fclose(dump_fp);
                return date_n_time(&dumpdate, length);
            } else {            /* Partial Dump */
                if (*(cp2++) == '0')
                    continue;   /* Not interested in full dumps */
                while (isspace(0xFF & *cp2))
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
    netsnmp_fsys_info *fsys;

    fsys = netsnmp_fsys_by_device( dev, NETSNMP_FS_FIND_EXIST );
    return (fsys ? fsys->idx.oids[0] : -1 );
}

long
Get_FSSize(char *dev)
{
    netsnmp_fsys_info *fsys;

    fsys = netsnmp_fsys_by_device( dev, NETSNMP_FS_FIND_EXIST );
    if ( fsys )       
        return netsnmp_fsys_size( fsys );
    else
        return -1;
}

int
Check_HR_FileSys_NFS (void)
{
    return (HRFS_entry->flags & NETSNMP_FS_FLAG_REMOTE) ? 1 : 0;
}
