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
#include <errno.h>
#include <ctype.h>
#include <stdint.h>

#include "superblocks.h"

struct via_metadata {
	uint16_t	signature;
	uint8_t		version_number;
	struct via_array {
		uint16_t	disk_bit_mask;
		uint8_t		disk_array_ex;
		uint32_t	capacity_low;
		uint32_t	capacity_high;
		uint32_t	serial_checksum;
	} __attribute__((packed)) array;
	uint32_t	serial_checksum[8];
	uint8_t		checksum;
} __attribute__((packed));

#define VIA_SIGNATURE		0xAA55

/* 8 bit checksum on first 50 bytes of metadata. */
static uint8_t via_checksum(struct via_metadata *v)
{
	uint8_t i = 50, cs = 0;

	while (i--)
		cs += ((uint8_t*) v)[i];

	return cs == v->checksum;
}

static int probe_viaraid(blkid_probe pr,
		const struct blkid_idmag *mag __attribute__((__unused__)))
{
	uint64_t off;
	struct via_metadata *v;

	if (pr->size < 0x10000)
		return -1;
	if (!S_ISREG(pr->mode) && !blkid_probe_is_wholedisk(pr))
		return -1;

	off = ((pr->size / 0x200)-1) * 0x200;

	v = (struct via_metadata *)
			blkid_probe_get_buffer(pr,
				off,
				sizeof(struct via_metadata));
	if (!v)
		return -1;
	if (le16_to_cpu(v->signature) != VIA_SIGNATURE)
		return -1;
	if (v->version_number > 2)
		return -1;
	if (!via_checksum(v))
		return -1;
	if (blkid_probe_sprintf_version(pr, "%u", v->version_number) != 0)
		return -1;
	if (blkid_probe_set_magic(pr, off,
				sizeof(v->signature),
				(unsigned char *) &v->signature))
		return -1;
	return 0;
}

const struct blkid_idinfo viaraid_idinfo = {
	.name		= "via_raid_member",
	.usage		= BLKID_USAGE_RAID,
	.probefunc	= probe_viaraid,
	.magics		= BLKID_NONE_MAGIC
};


