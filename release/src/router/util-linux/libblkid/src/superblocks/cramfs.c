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
#include <stdint.h>

#include "superblocks.h"

struct cramfs_super
{
	uint8_t		magic[4];
	uint32_t	size;
	uint32_t	flags;
	uint32_t	future;
	uint8_t		signature[16];
	struct cramfs_info
	{
		uint32_t	crc;
		uint32_t	edition;
		uint32_t	blocks;
		uint32_t	files;
	} __attribute__((packed)) info;
	uint8_t		name[16];
} __attribute__((packed));

static int probe_cramfs(blkid_probe pr, const struct blkid_idmag *mag)
{
	struct cramfs_super *cs;

	cs = blkid_probe_get_sb(pr, mag, struct cramfs_super);
	if (!cs)
		return -1;

	blkid_probe_set_label(pr, cs->name, sizeof(cs->name));
	return 0;
}

const struct blkid_idinfo cramfs_idinfo =
{
	.name		= "cramfs",
	.usage		= BLKID_USAGE_FILESYSTEM,
	.probefunc	= probe_cramfs,
	.magics		=
	{
		{ "\x45\x3d\xcd\x28", 4, 0, 0 },
		{ "\x28\xcd\x3d\x45", 4, 0, 0 },
		{ NULL }
	}
};


