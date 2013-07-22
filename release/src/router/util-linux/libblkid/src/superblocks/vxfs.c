/*
 * Copyright (C) 2008 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */

#include <stdio.h>

#include "superblocks.h"

struct vxfs_super_block {
	uint32_t		vs_magic;
	int32_t			vs_version;
};

static int probe_vxfs(blkid_probe pr, const struct blkid_idmag *mag)
{
	struct vxfs_super_block *vxs;

	vxs = blkid_probe_get_sb(pr, mag, struct vxfs_super_block);
	if (!vxs)
		return -1;

	blkid_probe_sprintf_version(pr, "%u", (unsigned int) vxs->vs_version);
	return 0;
}


const struct blkid_idinfo vxfs_idinfo =
{
	.name		= "vxfs",
	.usage		= BLKID_USAGE_FILESYSTEM,
	.probefunc	= probe_vxfs,
	.magics		=
	{
		{ .magic = "\365\374\001\245", .len = 4, .kboff = 1 },
		{ NULL }
	}
};

