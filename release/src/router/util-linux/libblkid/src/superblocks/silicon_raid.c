/*
 * Copyright (C) 2008 Karel Zak <kzak@redhat.com>
 * Copyright (C) 2005 Kay Sievers <kay.sievers@vrfy.org>
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
#include <errno.h>
#include <ctype.h>
#include <stdint.h>
#include <stddef.h>

#include "superblocks.h"

struct silicon_metadata {
	uint8_t		unknown0[0x2E];
	uint8_t		ascii_version[0x36 - 0x2E];
	uint8_t		diskname[0x56 - 0x36];
	uint8_t		unknown1[0x60 - 0x56];
	uint32_t	magic;
	uint32_t	unknown1a[0x6C - 0x64];
	uint32_t	array_sectors_low;
	uint32_t	array_sectors_high;
	uint8_t		unknown2[0x78 - 0x74];
	uint32_t	thisdisk_sectors;
	uint8_t		unknown3[0x100 - 0x7C];
	uint8_t		unknown4[0x104 - 0x100];
	uint16_t	product_id;
	uint16_t	vendor_id;
	uint16_t	minor_ver;
	uint16_t	major_ver;
} __attribute__((packed));

#define SILICON_MAGIC		0x2F000000


static int probe_silraid(blkid_probe pr,
		const struct blkid_idmag *mag __attribute__((__unused__)))
{
	uint64_t off;
	struct silicon_metadata *sil;

	if (pr->size < 0x10000)
		return -1;
	if (!S_ISREG(pr->mode) && !blkid_probe_is_wholedisk(pr))
		return -1;

	off = ((pr->size / 0x200) - 1) * 0x200;

	sil = (struct silicon_metadata *)
			blkid_probe_get_buffer(pr, off,
				sizeof(struct silicon_metadata));
	if (!sil)
		return -1;

	if (le32_to_cpu(sil->magic) != SILICON_MAGIC)
		return -1;

	if (blkid_probe_sprintf_version(pr, "%u.%u",
				le16_to_cpu(sil->major_ver),
				le16_to_cpu(sil->minor_ver)) != 0)
		return -1;
	if (blkid_probe_set_magic(pr,
			off + offsetof(struct silicon_metadata, magic),
			sizeof(sil->magic),
			(unsigned char *) &sil->magic))
		return -1;
	return 0;
}

const struct blkid_idinfo silraid_idinfo = {
	.name		= "silicon_medley_raid_member",
	.usage		= BLKID_USAGE_RAID,
	.probefunc	= probe_silraid,
	.magics		= BLKID_NONE_MAGIC
};


