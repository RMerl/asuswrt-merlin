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

struct isw_metadata {
	uint8_t		sig[32];
	uint32_t	check_sum;
	uint32_t	mpb_size;
	uint32_t	family_num;
	uint32_t	generation_num;
};

#define ISW_SIGNATURE		"Intel Raid ISM Cfg Sig. "


static int probe_iswraid(blkid_probe pr,
		const struct blkid_idmag *mag __attribute__((__unused__)))
{
	uint64_t off;
	struct isw_metadata *isw;

	if (pr->size < 0x10000)
		return -1;
	if (!S_ISREG(pr->mode) && !blkid_probe_is_wholedisk(pr))
		return -1;

	off = ((pr->size / 0x200) - 2) * 0x200;
	isw = (struct isw_metadata *)
			blkid_probe_get_buffer(pr,
					off,
					sizeof(struct isw_metadata));
	if (!isw)
		return -1;
	if (memcmp(isw->sig, ISW_SIGNATURE, sizeof(ISW_SIGNATURE)-1) != 0)
		return -1;
	if (blkid_probe_sprintf_version(pr, "%6s",
			&isw->sig[sizeof(ISW_SIGNATURE)-1]) != 0)
		return -1;
	if (blkid_probe_set_magic(pr, off, sizeof(isw->sig),
				(unsigned char *) isw->sig))
		return -1;
	return 0;
}

const struct blkid_idinfo iswraid_idinfo = {
	.name		= "isw_raid_member",
	.usage		= BLKID_USAGE_RAID,
	.probefunc	= probe_iswraid,
	.magics		= BLKID_NONE_MAGIC
};


