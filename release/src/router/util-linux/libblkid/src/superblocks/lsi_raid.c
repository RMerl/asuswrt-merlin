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

struct lsi_metadata {
	uint8_t		sig[6];
};


#define LSI_SIGNATURE		"$XIDE$"

static int probe_lsiraid(blkid_probe pr,
		const struct blkid_idmag *mag __attribute__((__unused__)))
{
	uint64_t off;
	struct lsi_metadata *lsi;

	if (pr->size < 0x10000)
		return -1;
	if (!S_ISREG(pr->mode) && !blkid_probe_is_wholedisk(pr))
		return -1;

	off = ((pr->size / 0x200) - 1) * 0x200;
	lsi = (struct lsi_metadata *)
		blkid_probe_get_buffer(pr,
				off,
				sizeof(struct lsi_metadata));
	if (!lsi)
		return -1;

	if (memcmp(lsi->sig, LSI_SIGNATURE, sizeof(LSI_SIGNATURE)-1) != 0)
		return -1;
	if (blkid_probe_set_magic(pr, off, sizeof(lsi->sig),
				(unsigned char *) lsi->sig))
		return -1;
	return 0;
}

const struct blkid_idinfo lsiraid_idinfo = {
	.name		= "lsi_mega_raid_member",
	.usage		= BLKID_USAGE_RAID,
	.probefunc	= probe_lsiraid,
	.magics		= BLKID_NONE_MAGIC
};


