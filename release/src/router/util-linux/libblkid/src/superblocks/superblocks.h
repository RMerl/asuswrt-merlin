/*
 * Copyright (C) 2008-2009 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */
#ifndef _BLKID_SUPERBLOCKS_H
#define _BLKID_SUPERBLOCKS_H

#include "blkidP.h"

extern const struct blkid_idinfo cramfs_idinfo;
extern const struct blkid_idinfo swap_idinfo;
extern const struct blkid_idinfo swsuspend_idinfo;
extern const struct blkid_idinfo adraid_idinfo;
extern const struct blkid_idinfo ddfraid_idinfo;
extern const struct blkid_idinfo iswraid_idinfo;
extern const struct blkid_idinfo jmraid_idinfo;
extern const struct blkid_idinfo lsiraid_idinfo;
extern const struct blkid_idinfo nvraid_idinfo;
extern const struct blkid_idinfo pdcraid_idinfo;
extern const struct blkid_idinfo silraid_idinfo;
extern const struct blkid_idinfo viaraid_idinfo;
extern const struct blkid_idinfo linuxraid_idinfo;
extern const struct blkid_idinfo ext4dev_idinfo;
extern const struct blkid_idinfo ext4_idinfo;
extern const struct blkid_idinfo ext3_idinfo;
extern const struct blkid_idinfo ext2_idinfo;
extern const struct blkid_idinfo jbd_idinfo;
extern const struct blkid_idinfo jfs_idinfo;
extern const struct blkid_idinfo xfs_idinfo;
extern const struct blkid_idinfo gfs_idinfo;
extern const struct blkid_idinfo gfs2_idinfo;
extern const struct blkid_idinfo romfs_idinfo;
extern const struct blkid_idinfo ocfs_idinfo;
extern const struct blkid_idinfo ocfs2_idinfo;
extern const struct blkid_idinfo oracleasm_idinfo;
extern const struct blkid_idinfo reiser_idinfo;
extern const struct blkid_idinfo reiser4_idinfo;
extern const struct blkid_idinfo hfs_idinfo;
extern const struct blkid_idinfo hfsplus_idinfo;
extern const struct blkid_idinfo ntfs_idinfo;
extern const struct blkid_idinfo iso9660_idinfo;
extern const struct blkid_idinfo udf_idinfo;
extern const struct blkid_idinfo vxfs_idinfo;
extern const struct blkid_idinfo minix_idinfo;
extern const struct blkid_idinfo vfat_idinfo;
extern const struct blkid_idinfo ufs_idinfo;
extern const struct blkid_idinfo hpfs_idinfo;
extern const struct blkid_idinfo lvm2_idinfo;
extern const struct blkid_idinfo lvm1_idinfo;
extern const struct blkid_idinfo snapcow_idinfo;
extern const struct blkid_idinfo luks_idinfo;
extern const struct blkid_idinfo highpoint37x_idinfo;
extern const struct blkid_idinfo highpoint45x_idinfo;
extern const struct blkid_idinfo squashfs_idinfo;
extern const struct blkid_idinfo netware_idinfo;
extern const struct blkid_idinfo sysv_idinfo;
extern const struct blkid_idinfo xenix_idinfo;
extern const struct blkid_idinfo btrfs_idinfo;
extern const struct blkid_idinfo ubifs_idinfo;
extern const struct blkid_idinfo zfs_idinfo;
extern const struct blkid_idinfo bfs_idinfo;
extern const struct blkid_idinfo vmfs_volume_idinfo;
extern const struct blkid_idinfo vmfs_fs_idinfo;
extern const struct blkid_idinfo drbd_idinfo;
extern const struct blkid_idinfo drbdproxy_datalog_idinfo;
extern const struct blkid_idinfo befs_idinfo;
extern const struct blkid_idinfo nilfs2_idinfo;
extern const struct blkid_idinfo exfat_idinfo;

/*
 * superblock functions
 */
extern int blkid_probe_set_version(blkid_probe pr, const char *version);
extern int blkid_probe_sprintf_version(blkid_probe pr, const char *fmt, ...)
		__attribute__ ((format (printf, 2, 3)));

extern int blkid_probe_set_label(blkid_probe pr, unsigned char *label, size_t len);
extern int blkid_probe_set_utf8label(blkid_probe pr, unsigned char *label,
                size_t len, int enc);
extern int blkid_probe_sprintf_uuid(blkid_probe pr, unsigned char *uuid,
                size_t len, const char *fmt, ...)
		__attribute__ ((format (printf, 4, 5)));
extern int blkid_probe_strncpy_uuid(blkid_probe pr, unsigned char *str, size_t len);

extern int blkid_probe_set_uuid(blkid_probe pr, unsigned char *uuid);
extern int blkid_probe_set_uuid_as(blkid_probe pr, unsigned char *uuid, const char *name);


#endif /* _BLKID_SUPERBLOCKS_H */
