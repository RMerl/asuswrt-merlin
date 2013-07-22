/*
 * Copyright (C) 1999, 2001 by Andries Brouwer
 * Copyright (C) 1999, 2000, 2003 by Theodore Ts'o
 * Copyright (C) 2008 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <stdint.h>

#include "superblocks.h"

struct romfs_super_block {
	unsigned char	ros_magic[8];
	uint32_t	ros_dummy1[2];
	unsigned char	ros_volume[16];
} __attribute__((packed));

static int probe_romfs(blkid_probe pr, const struct blkid_idmag *mag)
{
	struct romfs_super_block *ros;

	ros = blkid_probe_get_sb(pr, mag, struct romfs_super_block);
	if (!ros)
		return -1;

	if (strlen((char *) ros->ros_volume))
		blkid_probe_set_label(pr, ros->ros_volume,
				sizeof(ros->ros_volume));
	return 0;
}

const struct blkid_idinfo romfs_idinfo =
{
	.name		= "romfs",
	.usage		= BLKID_USAGE_FILESYSTEM,
	.probefunc	= probe_romfs,
	.magics		=
	{
		{ .magic = "-rom1fs-", .len = 8 },
		{ NULL }
	}
};

