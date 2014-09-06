#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/hardware/fsys.h>

#include <stdio.h>
#if HAVE_SYS_MNTCTL_H
#include <sys/mntctl.h>
#endif
#if HAVE_SYS_VMOUNT_H
#include <sys/vmount.h>
#endif
#if HAVE_SYS_STATFS_H
#include <sys/statfs.h>
#endif
#if HAVE_SYS_STATVFS_H
#include <sys/statvfs.h>
#endif


int
_fsys_remote( char *device, int type, char *host )
{
    if (( type == NETSNMP_FS_TYPE_NFS) ||
        ( type == NETSNMP_FS_TYPE_AFS))
        return 1;
    else
        return 0;
}

int
_fsys_type( int type)
{
    DEBUGMSGTL(("fsys:type", "Classifying %d\n", type));

    switch ( type ) {
        case  MNT_AIX:
        case  MNT_JFS:
            return NETSNMP_FS_TYPE_BERKELEY;

        case  MNT_CDROM:
            return NETSNMP_FS_TYPE_ISO9660;

        case  MNT_NFS:
        case  MNT_NFS3:
        case  MNT_AUTOFS:
            return NETSNMP_FS_TYPE_NFS;

    /*
     *  The following code covers selected filesystems
     *    which are not covered by the HR-TYPES enumerations,
     *    but should still be monitored.
     *  These are all mapped into type "other"
     *
     */    
#ifdef MNT_NAMEFS
        case MNT_NAMEFS:
#endif
#ifdef MNT_PROCFS
        case MNT_PROCFS:
#endif
        case MNT_SFS:
        case MNT_CACHEFS:
            return NETSNMP_FS_TYPE_OTHER;

    /*    
     *  All other types are silently skipped
     */
        default:
            return NETSNMP_FS_TYPE_IGNORE;
    }
    return NETSNMP_FS_TYPE_IGNORE;  /* Not reached */
}

void
netsnmp_fsys_arch_init( void )
{
    return;
}

void
netsnmp_fsys_arch_load( void )
{
    int  ret  = 0, i = 0;
    uint size = 0;

    struct vmount *aixmnt, *aixcurr;
    char          *path;
    struct statfs  stat_buf;
    netsnmp_fsys_info *entry;
    char               tmpbuf[1024];

    /*
     * Retrieve information about the currently mounted filesystems...
     */
    ret = mntctl(MCTL_QUERY, sizeof(uint), &size);
    if ( ret != 0 || size<=0 ) {
        snmp_log_perror( "initial mntctl failed" );
        return;
    }

    aixmnt = (struct vmount *)malloc( size );
    if ( aixmnt == NULL ) {
        snmp_log_perror( "cannot allocate memory for mntctl data" );
        return;
    }

    ret = mntctl(MCTL_QUERY, size, aixmnt );
    if ( ret <= 0 ) {
        free(aixmnt);
        snmp_log_perror( "main mntctl failed" );
        return;
    }
    aixcurr = aixmnt;


    /*
     * ... and insert this into the filesystem container.
     */

    for (i = 0;
         i < ret;
         i++, aixcurr = (struct vmount *) ((char*)aixcurr + aixcurr->vmt_length) ) {

        path = vmt2dataptr( aixcurr, VMT_STUB );
        entry = netsnmp_fsys_by_path( path, NETSNMP_FS_FIND_CREATE );
        if (!entry) {
            continue;
        }

        strlcpy(entry->path, path, sizeof(entry->path));
        strlcpy(entry->device, vmt2dataptr(aixcurr, VMT_OBJECT),
                sizeof(entry->device));
        entry->type   = _fsys_type( aixcurr->vmt_gfstype );

        if (!(entry->type & _NETSNMP_FS_TYPE_SKIP_BIT))
            entry->flags |= NETSNMP_FS_FLAG_ACTIVE;

        if ( _fsys_remote( entry->device, entry->type, vmt2dataptr( aixcurr, VMT_HOST) ))
            entry->flags |= NETSNMP_FS_FLAG_REMOTE;
        if ( aixcurr->vmt_flags & MNT_READONLY )
            entry->flags |= NETSNMP_FS_FLAG_RONLY;
        /*
         *  The root device is presumably bootable.
         *  Other partitions probably aren't!
         */
        if ((entry->path[0] == '/') && (entry->path[1] == '\0'))
            entry->flags |= NETSNMP_FS_FLAG_BOOTABLE;

        /*
         *  XXX - identify removeable disks
         */

        /*
         *  Optionally skip retrieving statistics for remote mounts
         */
        if ( (entry->flags & NETSNMP_FS_FLAG_REMOTE) &&
            netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID,
                                   NETSNMP_DS_AGENT_SKIPNFSINHOSTRESOURCES))
            continue;

        if ( statfs( entry->path, &stat_buf ) < 0 ) {
            snprintf( tmpbuf, sizeof(tmpbuf), "Cannot statfs %s\n", entry->path );
            snmp_log_perror( tmpbuf );
            continue;
        }
        entry->units =  stat_buf.f_bsize;
        entry->size  =  stat_buf.f_blocks;
        entry->used  = (stat_buf.f_blocks - stat_buf.f_bfree);
        entry->avail =  stat_buf.f_bavail;
        entry->inums_total = stat_buf.f_files;
        entry->inums_avail = stat_buf.f_ffree;
        netsnmp_fsys_calculate32(entry);
    }
    free(aixmnt);
    aixmnt  = NULL;
    aixcurr = NULL;
}

