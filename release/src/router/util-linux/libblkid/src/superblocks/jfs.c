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

struct jfs_super_block {
	unsigned char	js_magic[4];
	uint32_t	js_version;
	uint64_t	js_size;
	uint32_t	js_bsize;	/* 4: aggregate block size in bytes */
	uint16_t	js_l2bsize;	/* 2: log2 of s_bsize */
	uint16_t	js_l2bfactor;	/* 2: log2(s_bsize/hardware block size) */
	uint32_t	js_pbsize;	/* 4: hardware/LVM block size in bytes */
	uint16_t	js_l2pbsize;	/* 2: log2 of s_pbsize */
	uint16_t	js_pad;		/* 2: padding necessary for alignment */
	uint32_t	js_dummy2[26];
	unsigned char	js_uuid[16];
	unsigned char	js_label[16];
	unsigned char	js_loguuid[16];
};

static int probe_jfs(blkid_probe pr, const struct blkid_idmag *mag)
{
	struct jfs_super_block *js;

	js = blkid_probe_get_sb(pr, mag, struct jfs_super_block);
	if (!js)
		return -1;
	if (le32_to_cpu(js->js_bsize) != (1U << le16_to_cpu(js->js_l2bsize)))
		return 1;
	if (le32_to_cpu(js->js_pbsize) != (1U << le16_to_cpu(js->js_l2pbsize)))
		return 1;
	if ((le16_to_cpu(js->js_l2bsize) - le16_to_cpu(js->js_l2pbsize)) !=
	    le16_to_cpu(js->js_l2bfactor))
		return 1;

	if (strlen((char *) js->js_label))
		blkid_probe_set_label(pr, js->js_label, sizeof(js->js_label));
	blkid_probe_set_uuid(pr, js->js_uuid);
	return 0;
}


const struct blkid_idinfo jfs_idinfo =
{
	.name		= "jfs",
	.usage		= BLKID_USAGE_FILESYSTEM,
	.probefunc	= probe_jfs,
	.minsz		= 16 * 1024 * 1024,
	.magics		=
	{
		{ .magic = "JFS1", .len = 4, .kboff = 32 },
		{ NULL }
	}
};

