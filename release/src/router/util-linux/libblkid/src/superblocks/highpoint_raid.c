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

#include "superblocks.h"

struct hpt45x_metadata {
	uint32_t	magic;
};

#define HPT45X_MAGIC_OK			0x5a7816f3
#define HPT45X_MAGIC_BAD		0x5a7816fd

static int probe_highpoint45x(blkid_probe pr,
		const struct blkid_idmag *mag __attribute__((__unused__)))
{
	struct hpt45x_metadata *hpt;
	uint64_t off;
	uint32_t magic;

	if (pr->size < 0x10000)
		return -1;
	if (!S_ISREG(pr->mode) && !blkid_probe_is_wholedisk(pr))
		return -1;

	off = ((pr->size / 0x200) - 11) * 0x200;
	hpt = (struct hpt45x_metadata *)
			blkid_probe_get_buffer(pr,
					off,
					sizeof(struct hpt45x_metadata));
	if (!hpt)
		return -1;
	magic = le32_to_cpu(hpt->magic);
	if (magic != HPT45X_MAGIC_OK && magic != HPT45X_MAGIC_BAD)
		return -1;
	if (blkid_probe_set_magic(pr, off, sizeof(hpt->magic),
				(unsigned char *) &hpt->magic))
		return -1;
	return 0;
}

static int probe_highpoint37x(blkid_probe pr,
		const struct blkid_idmag *mag __attribute__((__unused__)))
{
	if (!S_ISREG(pr->mode) && !blkid_probe_is_wholedisk(pr))
		return -1;
	return 0;
}


const struct blkid_idinfo highpoint45x_idinfo = {
	.name		= "hpt45x_raid_member",
	.usage		= BLKID_USAGE_RAID,
	.probefunc	= probe_highpoint45x,
	.magics		= BLKID_NONE_MAGIC
};

const struct blkid_idinfo highpoint37x_idinfo = {
	.name		= "hpt37x_raid_member",
	.usage		= BLKID_USAGE_RAID,
	.probefunc	= probe_highpoint37x,
	.magics		= {
		/*
		 * Superblok offset:                      4608 bytes  (9 sectors)
		 * Magic string offset within superblock:   32 bytes
		 *
		 * kboff = (4608 + 32) / 1024
		 * sboff = (4608 + 32) % kboff
		 */
		{ .magic = "\xf0\x16\x78\x5a", .len = 4, .kboff = 4, .sboff = 544 },
		{ .magic = "\xfd\x16\x78\x5a", .len = 4, .kboff = 4, .sboff = 544 },
		{ NULL }
	}
};


