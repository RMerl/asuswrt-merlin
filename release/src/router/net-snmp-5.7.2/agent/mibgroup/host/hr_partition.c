/*
 *  Host Resources MIB - partition device group implementation - hr_partition.c
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
#include <fcntl.h>
#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <errno.h>

#include "host_res.h"
#include "hr_partition.h"
#include "hr_filesys.h"
#include "hr_disk.h"

#include <sys/stat.h>

#define HRP_MONOTONICALLY_INCREASING

        /*********************
	 *
	 *  Kernel & interface information,
	 *   and internal forward declarations
	 *
	 *********************/

static int      HRP_savedDiskIndex;
static int      HRP_savedPartIndex;
static char     HRP_savedName[1024];
#ifdef NETSNMP_CAN_GET_DISK_LABEL
static char     HRP_savedLabel[1024];
#endif

static int      HRP_DiskIndex;

static void     Save_HR_Partition(int, int);


        /*********************
	 *
	 *  Initialisation & common implementation functions
	 *
	 *********************/

static void     Init_HR_Partition(void);
static int      Get_Next_HR_Partition(void);
int             header_hrpartition(struct variable *, oid *, size_t *, int,
                                   size_t *, WriteMethod **);


#define	HRPART_INDEX		1
#define	HRPART_LABEL		2
#define	HRPART_ID		3
#define	HRPART_SIZE		4
#define	HRPART_FSIDX		5

struct variable4 hrpartition_variables[] = {
    {HRPART_INDEX, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_hrpartition, 2, {1, 1}},
    {HRPART_LABEL, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_hrpartition, 2, {1, 2}},
    {HRPART_ID, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_hrpartition, 2, {1, 3}},
    {HRPART_SIZE, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_hrpartition, 2, {1, 4}},
    {HRPART_FSIDX, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_hrpartition, 2, {1, 5}}
};
oid             hrpartition_variables_oid[] =
    { 1, 3, 6, 1, 2, 1, 25, 3, 7 };


void
init_hr_partition(void)
{
    REGISTER_MIB("host/hr_partition", hrpartition_variables, variable4,
                 hrpartition_variables_oid);
}


/*
 * header_hrpartition(...
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
header_hrpartition(struct variable *vp,
                   oid * name,
                   size_t * length,
                   int exact,
                   size_t * var_len, WriteMethod ** write_method)
{
#define HRPART_DISK_NAME_LENGTH		11
#define HRPART_ENTRY_NAME_LENGTH	12
    oid             newname[MAX_OID_LEN];
    int             part_idx, LowDiskIndex = -1, LowPartIndex = -1;
    int             result;

    DEBUGMSGTL(("host/hr_partition", "var_hrpartition: "));
    DEBUGMSGOID(("host/hr_partition", name, *length));
    DEBUGMSG(("host/hr_partition", " %d\n", exact));

    memcpy((char *) newname, (char *) vp->name,
           (int) vp->namelen * sizeof(oid));
    /*
     * Find "next" partition entry 
     */

    Init_HR_Disk();
    Init_HR_Partition();

    /*
     *  Find the "next" disk and partition entries.
     *  If we're in the middle of the table, then there's
     *     no point in examining earlier disks, so set the
     *     starting disk to that of the variable being queried.
     *
     *  If we've moved from one column of the table to another,
     *     then we need to start at the beginning again.
     *     (i.e. the 'compare' fails to match)
     *  Similarly if we're at the start of the table
     *     (i.e. *length is too short to be a full instance)
     */

    if ((snmp_oid_compare(vp->name, vp->namelen, name, vp->namelen) == 0)
        && (*length > HRPART_DISK_NAME_LENGTH)) {
        LowDiskIndex =
            (name[HRPART_DISK_NAME_LENGTH] &
             ((1 << HRDEV_TYPE_SHIFT) - 1));

        DEBUGMSGTL(("host/hr_partition", "... low index %d\n", LowDiskIndex));
        while (HRP_DiskIndex < LowDiskIndex) {
            Init_HR_Partition();        /* moves to next disk */
            if (HRP_DiskIndex == -1)
                return (MATCH_FAILED);
        }
    }

    for (;;) {
        part_idx = Get_Next_HR_Partition();
        DEBUGMSGTL(("host/hr_partition", "... part index %d\n", part_idx));
        if (part_idx == 0)
            break;
        newname[HRPART_DISK_NAME_LENGTH] =
            (HRDEV_DISK << HRDEV_TYPE_SHIFT) + HRP_DiskIndex;
        newname[HRPART_ENTRY_NAME_LENGTH] = part_idx;
        result = snmp_oid_compare(name, *length, newname, vp->namelen + 2);
        if (exact && (result == 0)) {
            Save_HR_Partition(HRP_DiskIndex, part_idx);
            LowDiskIndex = HRP_DiskIndex;
            LowPartIndex = part_idx;
            break;
        }
        if (!exact && (result < 0)) {
            if (LowPartIndex == -1) {
                Save_HR_Partition(HRP_DiskIndex, part_idx);
                LowDiskIndex = HRP_DiskIndex;
                LowPartIndex = part_idx;
            } else if (LowDiskIndex < HRP_DiskIndex)
                break;
            else if (part_idx < LowPartIndex) {
                Save_HR_Partition(HRP_DiskIndex, part_idx);
                LowDiskIndex = HRP_DiskIndex;
                LowPartIndex = part_idx;
            }
#ifdef HRP_MONOTONICALLY_INCREASING
            break;
#endif
        }
    }

    if (LowPartIndex == -1) {
        DEBUGMSGTL(("host/hr_partition", "... index out of range\n"));
        return (MATCH_FAILED);
    }

    newname[HRPART_DISK_NAME_LENGTH] =
        (HRDEV_DISK << HRDEV_TYPE_SHIFT) + LowDiskIndex;
    newname[HRPART_ENTRY_NAME_LENGTH] = LowPartIndex;
    memcpy((char *) name, (char *) newname,
           ((int) vp->namelen + 2) * sizeof(oid));
    *length = vp->namelen + 2;
    *write_method = (WriteMethod*)0;
    *var_len = sizeof(long);    /* default to 'long' results */

    DEBUGMSGTL(("host/hr_partition", "... get partition stats "));
    DEBUGMSGOID(("host/hr_partition", name, *length));
    DEBUGMSG(("host/hr_partition", "\n"));
    return LowPartIndex;
}


        /*********************
	 *
	 *  System specific implementation functions
	 *
	 *********************/


u_char         *
var_hrpartition(struct variable * vp,
                oid * name,
                size_t * length,
                int exact, size_t * var_len, WriteMethod ** write_method)
{
    int             part_idx;
    static char     string[1024];
    struct stat     stat_buf;

    part_idx =
        header_hrpartition(vp, name, length, exact, var_len, write_method);
    if (part_idx == MATCH_FAILED)
        return NULL;

    if (stat(HRP_savedName, &stat_buf) == -1)
        return NULL;

    switch (vp->magic) {
    case HRPART_INDEX:
        long_return = part_idx;
        return (u_char *) & long_return;
    case HRPART_LABEL:
#ifdef NETSNMP_CAN_GET_DISK_LABEL
        *var_len = strlen(HRP_savedLabel);
        return (u_char *) HRP_savedLabel;
#else
        *var_len = strlen(HRP_savedName);
        return (u_char *) HRP_savedName;
#endif
    case HRPART_ID:            /* Use the device number */
        sprintf(string, "0x%x", (int) stat_buf.st_rdev);
        *var_len = strlen(string);
        return (u_char *) string;
    case HRPART_SIZE:
        /*
         * XXX - based on single partition assumption 
         */
        long_return = Get_FSSize(HRP_savedName);
        return (u_char *) & long_return;
    case HRPART_FSIDX:
        long_return = Get_FSIndex(HRP_savedName);
        return (u_char *) & long_return;
    default:
        DEBUGMSGTL(("snmpd", "unknown sub-id %d in var_hrpartition\n",
                    vp->magic));
    }
    return NULL;
}


        /*********************
	 *
	 *  Internal implementation functions
	 *
	 *********************/

static int      HRP_index;

static void
Init_HR_Partition(void)
{
    DEBUGMSGTL(("host/hr_partition", "Init_HR_Partition\n"));
    HRP_DiskIndex = Get_Next_HR_Disk();
    if (HRP_DiskIndex != -1)
        HRP_DiskIndex &= ((1 << HRDEV_TYPE_SHIFT) - 1);
    DEBUGMSGTL(("host/hr_partition", "...  %d\n",HRP_DiskIndex));

    HRP_index = -1;
}

static int
Get_Next_HR_Partition(void)
{
    char            string[1024];
    int             fd;

    DEBUGMSGTL(("host/hr_partition", "Get_Next_HR_Partition %d\n",HRP_DiskIndex));
    if (HRP_DiskIndex == -1) {
        return 0;
    }

    HRP_index++;
    while (Get_Next_HR_Disk_Partition(string, sizeof(string), HRP_index) != -1) {
        DEBUGMSGTL(("host/hr_partition",
                    "Get_Next_HR_Partition: %s (:%d)\n",
                    string, HRP_index));

#ifdef O_NDELAY
        fd = open(string, O_RDONLY|O_NDELAY);
#else
        fd = open(string, O_RDONLY);
#endif
        if (fd != -1) {
            close(fd);
            return HRP_index + 1;
        } else if (errno == EBUSY) {
            return HRP_index + 1;
        }
        HRP_index++;
    }

    /*
     * Finished with this disk, try the next
     */
    Init_HR_Partition();
    return (Get_Next_HR_Partition());
}

static void
Save_HR_Partition(int disk_idx, int part_idx)
{
    HRP_savedDiskIndex = disk_idx;
    HRP_savedPartIndex = part_idx;
    (void) Get_Next_HR_Disk_Partition(HRP_savedName, sizeof(HRP_savedName), HRP_index);
#ifdef NETSNMP_CAN_GET_DISK_LABEL
    (void) Get_HR_Disk_Label(HRP_savedLabel, sizeof(HRP_savedLabel), HRP_savedName);
#endif
}
