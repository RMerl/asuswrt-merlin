#ifndef _NETSNMP_FSYS_MNTTYPES_H
#define _NETSNMP_FSYS_MNTTYPES_H
/*
 *  Some mounts can map to one of two hrFS types
 *    (depending on other characteristics of the system)
 *  Note which should be used *before* defining any
 *    type tokens which may be missing.
 */

#if (defined(BerkelyFS) && !defined(MNTTYPE_HFS)) || defined(solaris2)
#define  _NETSNMP_FS_TYPE_UFS  NETSNMP_FS_TYPE_BERKELEY
#else
#define  _NETSNMP_FS_TYPE_UFS  NETSNMP_FS_TYPE_SYSV
#endif

#ifdef RockRidge
#define  _NETSNMP_FS_TYPE_CDFS  NETSNMP_FS_TYPE_ROCKRIDGE
#else
#define  _NETSNMP_FS_TYPE_CDFS  NETSNMP_FS_TYPE_ISO9660
#endif


/*
 *  Ensure all recognised filesystem mount type tokens are
 *    available (even on systems where they're not used)
 */
#ifndef MNTTYPE_AFS
#define MNTTYPE_AFS      "afs"
#endif
#ifndef MNTTYPE_CDFS
#define MNTTYPE_CDFS     "cdfs"
#endif
#ifndef MNTTYPE_CD9660
#define MNTTYPE_CD9660   "cd9660"
#endif
#ifndef MNTTYPE_EXT2
#define MNTTYPE_EXT2     "ext2"
#endif
#ifndef MNTTYPE_EXT3
#define MNTTYPE_EXT3     "ext3"
#endif
#ifndef MNTTYPE_EXT4
#define MNTTYPE_EXT4     "ext4"
#endif
#ifndef MNTTYPE_EXT2FS
#define MNTTYPE_EXT2FS   "ext2fs"
#endif
#ifndef MNTTYPE_EXT3FS
#define MNTTYPE_EXT3FS   "ext3fs"
#endif
#ifndef MNTTYPE_EXT4FS
#define MNTTYPE_EXT4FS   "ext4fs"
#endif
#ifndef MNTTYPE_FAT32
#define MNTTYPE_FAT32    "fat32"
#endif
#ifndef MNTTYPE_FFS
#define MNTTYPE_FFS      "ffs"
#endif
#ifndef MNTTYPE_HFS
#define MNTTYPE_HFS      "hfs"
#endif
#ifndef MNTTYPE_HSFS
#define MNTTYPE_HSFS     "hsfs"
#endif
#ifndef MNTTYPE_ISO9660
#define MNTTYPE_ISO9660  "iso9660"
#endif
#ifndef MNTTYPE_MFS
#define MNTTYPE_MFS      "mfs"
#endif
#ifndef MNTTYPE_MSDOS
#define MNTTYPE_MSDOS    "msdos"
#endif
#ifndef MNTTYPE_NCPFS
#define MNTTYPE_NCPFS    "ncpfs"
#endif
#ifndef MNTTYPE_NFS
#define MNTTYPE_NFS      "nfs"
#endif
#ifndef MNTTYPE_NFS3
#define MNTTYPE_NFS3     "nfs3"
#endif
#ifndef MNTTYPE_NFS4
#define MNTTYPE_NFS4     "nfs4"
#endif
#ifndef MNTTYPE_NTFS
#define MNTTYPE_NTFS     "ntfs"
#endif
#ifndef MNTTYPE_PC
#define MNTTYPE_PC       "pc"
#endif
#ifndef MNTTYPE_SMBFS
#define MNTTYPE_SMBFS    "smbfs"
#endif
#ifndef MNTTYPE_CIFS
#define MNTTYPE_CIFS     "cifs"
#endif
#ifndef MNTTYPE_SYSV
#define MNTTYPE_SYSV     "sysv"
#endif
#ifndef MNTTYPE_UFS
#define MNTTYPE_UFS      "ufs"
#endif
#ifndef MNTTYPE_VFAT
#define MNTTYPE_VFAT     "vfat"
#endif

/*
 *  File systems to monitor, but not covered by HR-TYPES enumerations
 */
#ifndef MNTTYPE_MVFS
#define MNTTYPE_MVFS     "mvfs"
#endif
#ifndef MNTTYPE_TMPFS
#define MNTTYPE_TMPFS    "tmpfs"
#endif
#ifndef MNTTYPE_GFS
#define MNTTYPE_GFS      "gfs"
#endif
#ifndef MNTTYPE_GFS2
#define MNTTYPE_GFS2     "gfs2"
#endif
#ifndef MNTTYPE_XFS
#define MNTTYPE_XFS      "xfs"
#endif
#ifndef MNTTYPE_JFS
#define MNTTYPE_JFS      "jfs"
#endif
#ifndef MNTTYPE_VXFS
#define MNTTYPE_VXFS      "vxfs"
#endif
#ifndef MNTTYPE_REISERFS
#define MNTTYPE_REISERFS "reiserfs"
#endif
#ifndef MNTTYPE_LOFS
#define MNTTYPE_LOFS     "lofs"
#endif
#ifndef MNTTYPE_OCFS2
#define MNTTYPE_OCFS2    "ocfs2"
#endif
#ifndef MNTTYPE_CVFS
#define MNTTYPE_CVFS     "cvfs"
#endif

/*
 *  File systems to skip
 *    (Probably not strictly needed)
 */
#ifndef MNTTYPE_APP
#define MNTTYPE_APP      "app"
#endif
#ifndef MNTTYPE_DEVPTS
#define MNTTYPE_DEVPTS   "devpts"
#endif
#ifndef MNTTYPE_IGNORE
#define MNTTYPE_IGNORE   "ignore"
#endif
#ifndef MNTTYPE_PROC
#define MNTTYPE_PROC     "proc"
#endif
#ifndef MNTTYPE_SYSFS
#define MNTTYPE_SYSFS    "sysfs"
#endif
#ifndef MNTTYPE_USBFS
#define MNTTYPE_USBFS    "usbfs"
#endif
#ifndef MNTTYPE_BINFMT
#define MNTTYPE_BINFMT   "binfmt_misc"
#endif
#ifndef MNTTYPE_RPCPIPE
#define MNTTYPE_RPCPIPE  "rpc_pipefs"
#endif

#endif /* _NETSNMP_FSYS_MNTTYPES_H */
