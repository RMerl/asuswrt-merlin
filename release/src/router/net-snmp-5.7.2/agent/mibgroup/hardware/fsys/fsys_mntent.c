#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/hardware/fsys.h>

#include <stdio.h>
#if HAVE_MNTENT_H
#include <mntent.h>
#endif
#if HAVE_SYS_MNTTAB_H
#include <sys/mnttab.h>
#endif
#if HAVE_SYS_VFS_H
#include <sys/vfs.h>
#endif
#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#if HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif
#if HAVE_SYS_STATFS_H
#include <sys/statfs.h>
#endif
#if HAVE_SYS_STATVFS_H
#include <sys/statvfs.h>
#endif

#ifdef solaris2
#define _NETSNMP_GETMNTENT_TWO_ARGS 1
#else
#undef  _NETSNMP_GETMNTENT_TWO_ARGS 
#endif

    /*
     * Handle naming differences between getmntent() APIs
     */
#ifdef _NETSNMP_GETMNTENT_TWO_ARGS
    /* Two-argument form (Solaris) */
#define NSFS_MNTENT   struct mnttab
#define NSFS_PATH     mnt_mountp
#define NSFS_DEV      mnt_special
#define NSFS_TYPE     mnt_fstype

#define NSFS_STATFS   statvfs
#define NSFS_SIZE     f_frsize

#else
    /* One-argument form (everything else?) */
#define NSFS_MNTENT   struct mntent
#define NSFS_PATH     mnt_dir
#define NSFS_DEV      mnt_fsname
#define NSFS_TYPE     mnt_type

#define NSFS_STATFS   statfs
#define NSFS_SIZE     f_bsize

#endif

int
_fsys_remote( char *device, int type )
{
    if (( type == NETSNMP_FS_TYPE_NFS) ||
        ( type == NETSNMP_FS_TYPE_AFS))
        return 1;
    else
        return 0;
}

int
_fsys_type( char *typename )
{
    DEBUGMSGTL(("fsys:type", "Classifying %s\n", typename));

    if ( !typename || *typename=='\0' )
       return NETSNMP_FS_TYPE_UNKNOWN;

#include "mnttypes.h"

    else if ( !strcmp(typename, MNTTYPE_FFS) )
       return NETSNMP_FS_TYPE_BERKELEY;
    else if ( !strcmp(typename, MNTTYPE_UFS) )
       return _NETSNMP_FS_TYPE_UFS;   /* either N_FS_TYPE_BERKELEY or N_FS_TYPE_SYSV */
    else if ( !strcmp(typename, MNTTYPE_SYSV) )
       return NETSNMP_FS_TYPE_SYSV;
    else if ( !strcmp(typename, MNTTYPE_PC) ||
              !strcmp(typename, MNTTYPE_MSDOS) )
       return NETSNMP_FS_TYPE_FAT;
    else if ( !strcmp(typename, MNTTYPE_HFS) )
       return NETSNMP_FS_TYPE_HFS;
    else if ( !strcmp(typename, MNTTYPE_MFS) )
       return NETSNMP_FS_TYPE_MFS;
    else if ( !strcmp(typename, MNTTYPE_NTFS) )
       return NETSNMP_FS_TYPE_NTFS;
    else if ( !strcmp(typename, MNTTYPE_ISO9660) ||
              !strcmp(typename, MNTTYPE_CD9660) )
       return NETSNMP_FS_TYPE_ISO9660;
    else if ( !strcmp(typename, MNTTYPE_CDFS) )
       return _NETSNMP_FS_TYPE_CDFS;   /* either N_FS_TYPE_ISO9660 or N_FS_TYPE_ROCKRIDGE */
    else if ( !strcmp(typename, MNTTYPE_HSFS) )
       return NETSNMP_FS_TYPE_ROCKRIDGE;
    else if ( !strcmp(typename, MNTTYPE_NFS)  ||
              !strcmp(typename, MNTTYPE_NFS3) ||
              !strcmp(typename, MNTTYPE_NFS4) ||
              !strcmp(typename, MNTTYPE_CIFS) ||  /* i.e. SMB - ?? */
              !strcmp(typename, MNTTYPE_SMBFS)    /* ?? */ )
       return NETSNMP_FS_TYPE_NFS;
    else if ( !strcmp(typename, MNTTYPE_NCPFS) )
       return NETSNMP_FS_TYPE_NETWARE;
    else if ( !strcmp(typename, MNTTYPE_AFS) )
       return NETSNMP_FS_TYPE_AFS;
    else if ( !strcmp(typename, MNTTYPE_EXT2) ||
              !strcmp(typename, MNTTYPE_EXT3) ||
              !strcmp(typename, MNTTYPE_EXT4) ||
              !strcmp(typename, MNTTYPE_EXT2FS) ||
              !strcmp(typename, MNTTYPE_EXT3FS) ||
              !strcmp(typename, MNTTYPE_EXT4FS) )
       return NETSNMP_FS_TYPE_EXT2;
    else if ( !strcmp(typename, MNTTYPE_FAT32) ||
              !strcmp(typename, MNTTYPE_VFAT) )
       return NETSNMP_FS_TYPE_FAT32;

    /*
     *  The following code covers selected filesystems
     *    which are not covered by the HR-TYPES enumerations,
     *    but should still be monitored.
     *  These are all mapped into type "other"
     *
     *    (The systems listed are not fixed in stone,
     *     but are simply here to illustrate the principle!)
     */    
    else if ( !strcmp(typename, MNTTYPE_MVFS) ||
              !strcmp(typename, MNTTYPE_TMPFS) ||
              !strcmp(typename, MNTTYPE_GFS) ||
              !strcmp(typename, MNTTYPE_GFS2) ||
              !strcmp(typename, MNTTYPE_XFS) ||
              !strcmp(typename, MNTTYPE_JFS) ||
              !strcmp(typename, MNTTYPE_VXFS) ||
              !strcmp(typename, MNTTYPE_REISERFS) ||
              !strcmp(typename, MNTTYPE_OCFS2) ||
              !strcmp(typename, MNTTYPE_CVFS) ||
              !strcmp(typename, MNTTYPE_LOFS))
       return NETSNMP_FS_TYPE_OTHER;

    /*    
     *  All other types are silently skipped
     */
    else
       return NETSNMP_FS_TYPE_IGNORE;
}

void
netsnmp_fsys_arch_init( void )
{
    return;
}

void
netsnmp_fsys_arch_load( void )
{
    FILE              *fp=NULL;
#ifdef _NETSNMP_GETMNTENT_TWO_ARGS
    struct mnttab      mtmp;
    struct mnttab     *m = &mtmp;
#else
    struct mntent     *m;
#endif
    struct NSFS_STATFS stat_buf;
    netsnmp_fsys_info *entry;
    char               tmpbuf[1024];

    /*
     * Retrieve information about the currently mounted filesystems...
     */
    fp = fopen( ETC_MNTTAB, "r" );   /* OR setmntent()?? */
    if ( !fp ) {
        snprintf( tmpbuf, sizeof(tmpbuf), "Cannot open %s\n", ETC_MNTTAB );
        snmp_log_perror( tmpbuf );
        return;
    }

    /*
     * ... and insert this into the filesystem container.
     */
    while 
#ifdef _NETSNMP_GETMNTENT_TWO_ARGS
          ((getmntent(fp, m)) == 0 )
#else
          ((m = getmntent(fp)) != NULL )
#endif
    {
        entry = netsnmp_fsys_by_path( m->NSFS_PATH, NETSNMP_FS_FIND_CREATE );
        if (!entry) {
            continue;
        }

        strlcpy(entry->path, m->NSFS_PATH, sizeof(entry->path));
        strlcpy(entry->device, m->NSFS_DEV, sizeof(entry->device));
        entry->type = _fsys_type(m->NSFS_TYPE);
        if (!(entry->type & _NETSNMP_FS_TYPE_SKIP_BIT))
            entry->flags |= NETSNMP_FS_FLAG_ACTIVE;

        if ( _fsys_remote( entry->device, entry->type ))
            entry->flags |= NETSNMP_FS_FLAG_REMOTE;
#if HAVE_HASMNTOPT
        if (hasmntopt( m, "ro" ))
            entry->flags |= NETSNMP_FS_FLAG_RONLY;
#endif
        /*
         *  The root device is presumably bootable.
         *  Other partitions probably aren't!
         *
         *  XXX - what about /boot ??
         */
        if ((entry->path[0] == '/') &&
            (entry->path[1] == '\0'))
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

#ifdef irix6
        if ( NSFS_STATFS( entry->path, &stat_buf, sizeof(struct statfs), 0) < 0 )
#else
        if ( NSFS_STATFS( entry->path, &stat_buf ) < 0 )
#endif
        {
            snprintf( tmpbuf, sizeof(tmpbuf), "Cannot statfs %s\n", entry->path );
            snmp_log_perror( tmpbuf );
            continue;
        }
        entry->units =  stat_buf.NSFS_SIZE;
        entry->size  =  stat_buf.f_blocks;
        entry->used  = (stat_buf.f_blocks - stat_buf.f_bfree);
        /* entry->avail is currently unsigned, so protect against negative
         * values!
         * This should be changed to a signed field.
         */
        if (stat_buf.f_bavail < 0)
            entry->avail = 0;
        else
            entry->avail =  stat_buf.f_bavail;
        entry->inums_total = stat_buf.f_files;
        entry->inums_avail = stat_buf.f_ffree;
        netsnmp_fsys_calculate32(entry);
    }
    fclose( fp );
}

