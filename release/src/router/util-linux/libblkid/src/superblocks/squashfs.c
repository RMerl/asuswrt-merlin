/*
 * Copyright (C) 2008 Karel Zak <kzak@redhat.com>
 *
 * Inspired by libvolume_id by
 *     Kay Sievers <kay.sievers@vrfy.org>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#include "bitops.h"	/* swab16() */
#include "superblocks.h"

struct sqsh_super_block {
	uint32_t	s_magic;
	uint32_t	inodes;
	uint32_t	bytes_used_2;
	uint32_t	uid_start_2;
	uint32_t	guid_start_2;
	uint32_t	inode_table_start_2;
	uint32_t	directory_table_start_2;
	uint16_t	s_major;
	uint16_t	s_minor;
} __attribute__((packed));

static int probe_squashfs(blkid_probe pr, const struct blkid_idmag *mag)
{
	struct sqsh_super_block *sq;

	sq = blkid_probe_get_sb(pr, mag, struct sqsh_super_block);
	if (!sq)
		return -1;

	if (strcmp(mag->magic, "sqsh") == 0 ||
	    strcmp(mag->magic, "qshs") == 0)
		blkid_probe_sprintf_version(pr, "%u.%u",
				sq->s_major,
				sq->s_minor);
	else
		blkid_probe_sprintf_version(pr, "%u.%u",
				swab16(sq->s_major),
				swab16(sq->s_minor));
	return 0;
}

const struct blkid_idinfo squashfs_idinfo =
{
	.name		= "squashfs",
	.usage		= BLKID_USAGE_FILESYSTEM,
	.probefunc	= probe_squashfs,
	.magics		=
	{
		{ .magic = "sqsh", .len = 4 },
		{ .magic = "hsqs", .len = 4 }, /* swap */

		/* LZMA version */
		{ .magic = "qshs", .len = 4 },
		{ .magic = "shsq", .len = 4 }, /* swap */
		{ NULL }
	}
};


