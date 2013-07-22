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

struct promise_metadata {
	uint8_t	sig[24];
};

#define PDC_CONFIG_OFF		0x1200
#define PDC_SIGNATURE		"Promise Technology, Inc."

static int probe_pdcraid(blkid_probe pr,
		const struct blkid_idmag *mag __attribute__((__unused__)))
{
	unsigned int i;
	static unsigned int sectors[] = {
		63, 255, 256, 16, 399, 0
	};

	if (pr->size < 0x40000)
		return -1;
	if (!S_ISREG(pr->mode) && !blkid_probe_is_wholedisk(pr))
		return -1;

	for (i = 0; sectors[i] != 0; i++) {
		uint64_t off;
		struct promise_metadata *pdc;

		off = ((pr->size / 0x200) - sectors[i]) * 0x200;
		pdc = (struct promise_metadata *)
				blkid_probe_get_buffer(pr,
					off,
					sizeof(struct promise_metadata));
		if (!pdc)
			return -1;

		if (memcmp(pdc->sig, PDC_SIGNATURE,
				sizeof(PDC_SIGNATURE) - 1) == 0) {

			if (blkid_probe_set_magic(pr, off, sizeof(pdc->sig),
						(unsigned char *) pdc->sig))
				return -1;
			return 0;
		}
	}
	return -1;
}

const struct blkid_idinfo pdcraid_idinfo = {
	.name		= "promise_fasttrack_raid_member",
	.usage		= BLKID_USAGE_RAID,
	.probefunc	= probe_pdcraid,
	.magics		= BLKID_NONE_MAGIC
};


