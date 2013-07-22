/*
 * Copyright (C) 2008 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#include "superblocks.h"

/* Common gfs/gfs2 constants: */
#define GFS_LOCKNAME_LEN        64

/* gfs1 constants: */
#define GFS_FORMAT_FS           1309
#define GFS_FORMAT_MULTI        1401
/* gfs2 constants: */
#define GFS2_FORMAT_FS          1801
#define GFS2_FORMAT_MULTI       1900

struct gfs2_meta_header {
	uint32_t mh_magic;
	uint32_t mh_type;
	uint64_t __pad0;          /* Was generation number in gfs1 */
	uint32_t mh_format;
	uint32_t __pad1;          /* Was incarnation number in gfs1 */
};

struct gfs2_inum {
	uint64_t no_formal_ino;
	uint64_t no_addr;
};

struct gfs2_sb {
	struct gfs2_meta_header sb_header;

	uint32_t sb_fs_format;
	uint32_t sb_multihost_format;
	uint32_t  __pad0;  /* Was superblock flags in gfs1 */

	uint32_t sb_bsize;
	uint32_t sb_bsize_shift;
	uint32_t __pad1;   /* Was journal segment size in gfs1 */

	struct gfs2_inum sb_master_dir; /* Was jindex dinode in gfs1 */
	struct gfs2_inum __pad2; /* Was rindex dinode in gfs1 */
	struct gfs2_inum sb_root_dir;

	char sb_lockproto[GFS_LOCKNAME_LEN];
	char sb_locktable[GFS_LOCKNAME_LEN];

	struct gfs2_inum __pad3; /* Was quota inode in gfs1 */
	struct gfs2_inum __pad4; /* Was licence inode in gfs1 */
	uint8_t sb_uuid[16]; /* The UUID maybe 0 for backwards compat */
} __attribute__((packed));

static int probe_gfs(blkid_probe pr, const struct blkid_idmag *mag)
{
	struct gfs2_sb *sbd;

	sbd = blkid_probe_get_sb(pr, mag, struct gfs2_sb);
	if (!sbd)
		return -1;

	if (be32_to_cpu(sbd->sb_fs_format) == GFS_FORMAT_FS &&
	    be32_to_cpu(sbd->sb_multihost_format) == GFS_FORMAT_MULTI)
	{
		if (*sbd->sb_locktable)
			blkid_probe_set_label(pr,
				(unsigned char *) sbd->sb_locktable,
				sizeof(sbd->sb_locktable));

		blkid_probe_set_uuid(pr, sbd->sb_uuid);
		return 0;
	}

	return -1;
}

static int probe_gfs2(blkid_probe pr, const struct blkid_idmag *mag)
{
	struct gfs2_sb *sbd;

	sbd = blkid_probe_get_sb(pr, mag, struct gfs2_sb);
	if (!sbd)
		return -1;

	if (be32_to_cpu(sbd->sb_fs_format) == GFS2_FORMAT_FS &&
	    be32_to_cpu(sbd->sb_multihost_format) == GFS2_FORMAT_MULTI)
	{
		if (*sbd->sb_locktable)
			blkid_probe_set_label(pr,
				(unsigned char *) sbd->sb_locktable,
				sizeof(sbd->sb_locktable));
		blkid_probe_set_uuid(pr, sbd->sb_uuid);
		blkid_probe_set_version(pr, "1");
		return 0;
	}
	return -1;
}

const struct blkid_idinfo gfs_idinfo =
{
	.name		= "gfs",
	.usage		= BLKID_USAGE_FILESYSTEM,
	.probefunc	= probe_gfs,
	.minsz		= 32 * 1024 * 1024,	/* minimal size of GFS journal */
	.magics		=
	{
		{ .magic = "\x01\x16\x19\x70", .len = 4, .kboff = 64 },
		{ NULL }
	}
};

const struct blkid_idinfo gfs2_idinfo =
{
	.name		= "gfs2",
	.usage		= BLKID_USAGE_FILESYSTEM,
	.probefunc	= probe_gfs2,
	.minsz		= 32 * 1024 * 1024,	/* minimal size of GFS journal */
	.magics		=
	{
		{ .magic = "\x01\x16\x19\x70", .len = 4, .kboff = 64 },
		{ NULL }
	}
};

