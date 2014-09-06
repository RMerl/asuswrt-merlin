#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/hardware/fsys.h>
#include "hardware/fsys/hw_fsys.h"

#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#if HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif
#if HAVE_SYS_STATVFS_H
#include <sys/statvfs.h>
#endif


    /*
     * Handle minor naming differences between statfs/statvfs
     */
#if defined(_VFS_NAMELEN)
#define NSFS_NAMELEN  _VFS_NAMELEN
#else
#define NSFS_NAMELEN  MNAMELEN
#endif

#if defined(HAVE_GETVFSSTAT) && defined(__NetBSD__)
#define NSFS_GETFSSTAT  getvfsstat
#define NSFS_STATFS     statvfs
#define NSFS_FLAGS      f_flag
#else
#define NSFS_GETFSSTAT  getfsstat
#define NSFS_STATFS     statfs
#define NSFS_FLAGS      f_flags
#endif

/*
#if defined(HAVE_STATVFS)
#define NSFS_STATFS     statvfs
#define NSFS_FLAGS      f_flag
#else
#define NSFS_STATFS     statfs
#define NSFS_FLAGS      f_flags
#endif
*/

/*
#if defined(HAVE_STATVFS) && defined(__NetBSD__)
#define NSFS_NAMELEN    _VFS_NAMELEN
#define NSFS_GETFSSTAT  getvfsstat
#define NSFS_STATFS     statvfs
#else
#define NSFS_FLAGS      f_flags
#define NSFS_NAMELEN    _VFS_NAMELEN
#define NSFS_GETFSSTAT  getvfsstat
#define NSFS_STATFS     statvfs
#endif
*/

int
_fs_type( char *typename )
{
    DEBUGMSGTL(("fsys:type", "Classifying %s\n", typename));

    if ( !typename || *typename=='\0' )
       return NETSNMP_FS_TYPE_UNKNOWN;

#include "mounts.h"

       /*
        * List of mount types from <sys/mount.h>
        */
    else if ( !strcmp(typename, MOUNT_FFS) ||
              !strcmp(typename, MOUNT_UFS) )
       return NETSNMP_FS_TYPE_BERKELEY;
    else if ( !strcmp(typename, MOUNT_NFS) )
       return NETSNMP_FS_TYPE_NFS;
    else if ( !strcmp(typename, MOUNT_MFS) )
       return NETSNMP_FS_TYPE_MFS;
    else if ( !strcmp(typename, MOUNT_MSDOS) ||
              !strcmp(typename, MOUNT_MSDOSFS) )
       return NETSNMP_FS_TYPE_FAT;
    else if ( !strcmp(typename, MOUNT_AFS) )
       return NETSNMP_FS_TYPE_AFS;
    else if ( !strcmp(typename, MOUNT_CD9660) )
       return NETSNMP_FS_TYPE_ISO9660;
    else if ( !strcmp(typename, MOUNT_EXT2FS) )
       return NETSNMP_FS_TYPE_EXT2;
    else if ( !strcmp(typename, MOUNT_NTFS) )
       return NETSNMP_FS_TYPE_NTFS;

       /*
        * NetBSD also recognises the following filesystem types:
        *     MOUNT_NULL
        *     MOUNT_OVERLAY
        *     MOUNT_UMAP
        *     MOUNT_UNION
        *     MOUNT_CFS/CODA
        *     MOUNT_FILECORE
        *     MOUNT_SMBFS
        *     MOUNT_PTYFS
        * OpenBSD also recognises the following filesystem types:
        *     MOUNT_LOFS
        *     MOUNT_NCPFS
        *     MOUNT_XFS
        *     MOUNT_UDF
        * Both of them recognise the following filesystem types:
        *     MOUNT_LFS
        *     MOUNT_FDESC
        *     MOUNT_PORTAL
        *     MOUNT_KERNFS
        *     MOUNT_PROCFS
        *     MOUNT_ADOSFS
        *
        * All of these filesystems are mapped to NETSNMP_FS_TYPE_OTHER
        *   so will be picked up by the following default branch.
        */
    else
       return NETSNMP_FS_TYPE_OTHER;
}

void
netsnmp_fsys_arch_init( void )
{
    return;
}

void
netsnmp_fsys_arch_load( void )
{
    int n, i;
    struct NSFS_STATFS *stats;
    netsnmp_fsys_info *entry;

    /*
     * Retrieve information about the currently mounted filesystems...
     */
    n = NSFS_GETFSSTAT( NULL, 0, 0 );
    if ( n==0 )
        return;
    stats = (struct NSFS_STATFS *)malloc( n * sizeof( struct NSFS_STATFS ));
    n = NSFS_GETFSSTAT( stats, n * sizeof( struct NSFS_STATFS ), MNT_NOWAIT );

    /*
     * ... and insert this into the filesystem container.
     */
    for ( i=0; i<n; i++ ) {
        entry = netsnmp_fsys_by_path( stats[i].f_mntonname,
                                      NETSNMP_FS_FIND_CREATE );
        if (!entry)
            continue;

        strlcpy( entry->path,   stats[i].f_mntonname,   sizeof(entry->path));
        entry->path[sizeof(entry->path)-1] = '\0';
        strlcpy( entry->device, stats[i].f_mntfromname, sizeof(entry->device));
        entry->device[sizeof(entry->device)-1] = '\0';
        entry->units = stats[i].f_bsize;    /* or f_frsize */
        entry->size  = stats[i].f_blocks;
        entry->used  = (stats[i].f_blocks - stats[i].f_bfree);
        /* entry->avail is currently unsigned, so protect against negative
         * values!
         * This should be changed to a signed field.
         */
        if (stats[i].f_bavail < 0)
            entry->avail = 0;
        else
            entry->avail = stats[i].f_bavail;
        entry->inums_total = stats[i].f_files;
        entry->inums_avail = stats[i].f_ffree;

        entry->type = _fs_type( stats[i].f_fstypename );
        entry->flags |= NETSNMP_FS_FLAG_ACTIVE;

        if (! (stats[i].NSFS_FLAGS & MNT_LOCAL )) {
            entry->flags |= NETSNMP_FS_FLAG_REMOTE;
        }
        if (  stats[i].NSFS_FLAGS & MNT_RDONLY ) {
            entry->flags |= NETSNMP_FS_FLAG_RONLY;
        }
        if (  stats[i].NSFS_FLAGS & MNT_ROOTFS ) {
            entry->flags |= NETSNMP_FS_FLAG_BOOTABLE;
        }
        netsnmp_fsys_calculate32(entry);
    }

    free(stats);
}
