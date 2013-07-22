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

#include <string.h>
#include "superblocks.h"
#include "minix.h"

static int probe_minix(blkid_probe pr, const struct blkid_idmag *mag)
{
	unsigned char *ext;
	int version;

	/* for more details see magic strings below */
	switch(mag->magic[1]) {
	case '\023':
		version = 1;
		break;
	case '\044':
		version = 2;
		break;
	case '\115':
		version = 3;
		break;
	default:
		return -1;
		break;
	}

	if (version <= 2) {
		struct minix_super_block *sb;
		uint32_t zones;

		sb = blkid_probe_get_sb(pr, mag, struct minix_super_block);
		if (!sb || sb->s_imap_blocks == 0 || sb->s_zmap_blocks == 0)
			return -1;

		zones = version == 2 ? sb->s_zones : sb->s_nzones;

		/* sanity checks to be sure that the FS is really minix */
		if (sb->s_imap_blocks * MINIX_BLOCK_SIZE * 8 < sb->s_ninodes + 1)
			return -1;
		if (sb->s_zmap_blocks * MINIX_BLOCK_SIZE * 8 < zones - sb->s_firstdatazone + 1)
			return -1;

	} else if (version == 3) {
		struct minix3_super_block *sb;

		sb = blkid_probe_get_sb(pr, mag, struct minix3_super_block);
		if (!sb || sb->s_imap_blocks == 0 || sb->s_zmap_blocks == 0)
			return -1;

	}

	/* unfortunately, some parts of ext3 is sometimes possible to
	 * interpreted as minix superblock. So check for extN magic
	 * string. (For extN magic string and offsets see ext.c.)
	 */
	ext = blkid_probe_get_buffer(pr, 0x400 + 0x38, 2);
	if (ext && memcmp(ext, "\123\357", 2) == 0)
		return -1;

	blkid_probe_sprintf_version(pr, "%d", version);
	return 0;
}

const struct blkid_idinfo minix_idinfo =
{
	.name		= "minix",
	.usage		= BLKID_USAGE_FILESYSTEM,
	.probefunc	= probe_minix,
	.magics		=
	{
		/* version 1 */
		{ .magic = "\177\023", .len = 2, .kboff = 1, .sboff = 0x10 },
		{ .magic = "\217\023", .len = 2, .kboff = 1, .sboff = 0x10 },

		/* version 2 */
		{ .magic = "\150\044", .len = 2, .kboff = 1, .sboff = 0x10 },
		{ .magic = "\170\044", .len = 2, .kboff = 1, .sboff = 0x10 },

		/* version 3 */
		{ .magic = "\132\115", .len = 2, .kboff = 1, .sboff = 0x18 },
		{ NULL }
	}
};

