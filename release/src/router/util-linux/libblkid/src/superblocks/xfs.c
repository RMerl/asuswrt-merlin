/*
 * Copyright (C) 1999 by Andries Brouwer
 * Copyright (C) 1999, 2000, 2003 by Theodore Ts'o
 * Copyright (C) 2001 by Andreas Dilger
 * Copyright (C) 2004 Kay Sievers <kay.sievers@vrfy.org>
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

struct xfs_super_block {
	unsigned char	xs_magic[4];
	uint32_t	xs_blocksize;
	uint64_t	xs_dblocks;
	uint64_t	xs_rblocks;
	uint32_t	xs_dummy1[2];
	unsigned char	xs_uuid[16];
	uint32_t	xs_dummy2[15];
	char		xs_fname[12];
	uint32_t	xs_dummy3[2];
	uint64_t	xs_icount;
	uint64_t	xs_ifree;
	uint64_t	xs_fdblocks;
} __attribute__((packed));

static int probe_xfs(blkid_probe pr, const struct blkid_idmag *mag)
{
	struct xfs_super_block *xs;

	xs = blkid_probe_get_sb(pr, mag, struct xfs_super_block);
	if (!xs)
		return -1;

	if (strlen(xs->xs_fname))
		blkid_probe_set_label(pr, (unsigned char *) xs->xs_fname,
				sizeof(xs->xs_fname));
	blkid_probe_set_uuid(pr, xs->xs_uuid);
	return 0;
}

const struct blkid_idinfo xfs_idinfo =
{
	.name		= "xfs",
	.usage		= BLKID_USAGE_FILESYSTEM,
	.probefunc	= probe_xfs,
	.magics		=
	{
		{ .magic = "XFSB", .len = 4 },
		{ NULL }
	}
};

