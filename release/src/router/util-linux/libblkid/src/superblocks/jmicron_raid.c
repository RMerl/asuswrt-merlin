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

struct jm_metadata {
	int8_t		signature[2];
	uint8_t		minor_version;
	uint8_t		major_version;
	uint16_t	checksum;
};

#define JM_SIGNATURE		"JM"

static int probe_jmraid(blkid_probe pr,
		const struct blkid_idmag *mag __attribute__((__unused__)))
{
	uint64_t off;
	struct jm_metadata *jm;

	if (pr->size < 0x10000)
		return -1;
	if (!S_ISREG(pr->mode) && !blkid_probe_is_wholedisk(pr))
		return -1;

	off = ((pr->size / 0x200) - 1) * 0x200;
	jm = (struct jm_metadata *)
		blkid_probe_get_buffer(pr,
				off,
				sizeof(struct jm_metadata));
	if (!jm)
		return -1;
	if (memcmp(jm->signature, JM_SIGNATURE, sizeof(JM_SIGNATURE) - 1) != 0)
		return -1;
	if (blkid_probe_sprintf_version(pr, "%u.%u",
				jm->major_version, jm->minor_version) != 0)
		return -1;
	if (blkid_probe_set_magic(pr, off, sizeof(jm->signature),
				(unsigned char *) jm->signature))
		return -1;
	return 0;
}

const struct blkid_idinfo jmraid_idinfo = {
	.name		= "jmicron_raid_member",
	.usage		= BLKID_USAGE_RAID,
	.probefunc	= probe_jmraid,
	.magics		= BLKID_NONE_MAGIC
};


