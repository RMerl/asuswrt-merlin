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

/* linux-2.6/include/linux/swap.h */
struct swap_header_v1_2 {
     /*	char		bootbits[1024];	*/ /* Space for disklabel etc. */
	uint32_t	version;
	uint32_t	lastpage;
	uint32_t	nr_badpages;
	unsigned char	uuid[16];
	unsigned char	volume[16];
	uint32_t	padding[117];
	uint32_t	badpages[1];
} __attribute__((packed));

#define PAGESIZE_MIN	0xff6	/* 4086 (arm, i386, ...) */
#define PAGESIZE_MAX	0xfff6	/* 65526 (ia64) */

#define TOI_MAGIC_STRING	"\xed\xc3\x02\xe9\x98\x56\xe5\x0c"
#define TOI_MAGIC_STRLEN	(sizeof(TOI_MAGIC_STRING) - 1)

static int swap_set_info(blkid_probe pr, const char *version)
{
	struct swap_header_v1_2 *hdr;

	/* Swap header always located at offset of 1024 bytes */
	hdr = (struct swap_header_v1_2 *) blkid_probe_get_buffer(pr, 1024,
				sizeof(struct swap_header_v1_2));
	if (!hdr)
		return -1;

	/* SWAPSPACE2 - check for wrong version or zeroed pagecount */
	if (strcmp(version, "2") == 0 &&
	    (hdr->version != 1 || hdr->lastpage == 0))
		return -1;

	/* arbitrary sanity check.. is there any garbage down there? */
	if (hdr->padding[32] == 0 && hdr->padding[33] == 0) {
		if (hdr->volume[0] && blkid_probe_set_label(pr, hdr->volume,
				sizeof(hdr->volume)) < 0)
			return -1;
		if (blkid_probe_set_uuid(pr, hdr->uuid) < 0)
			return -1;
	}

	blkid_probe_set_version(pr, version);
	return 0;
}

static int probe_swap(blkid_probe pr, const struct blkid_idmag *mag)
{
	unsigned char *buf;

	if (!mag)
		return -1;

	/* TuxOnIce keeps valid swap header at the end of the 1st page */
	buf = blkid_probe_get_buffer(pr, 0, TOI_MAGIC_STRLEN);
	if (!buf)
		return -1;

	if (memcmp(buf, TOI_MAGIC_STRING, TOI_MAGIC_STRLEN) == 0)
		return 1;	/* Ignore swap signature, it's TuxOnIce */

	if (!memcmp(mag->magic, "SWAP-SPACE", mag->len)) {
		/* swap v0 doesn't support LABEL or UUID */
		blkid_probe_set_version(pr, "1");
		return 0;

	} else if (!memcmp(mag->magic, "SWAPSPACE2", mag->len))
		return swap_set_info(pr, "2");

	return -1;
}

static int probe_swsuspend(blkid_probe pr, const struct blkid_idmag *mag)
{
	if (!mag)
		return -1;
	if (!memcmp(mag->magic, "S1SUSPEND", mag->len))
		return swap_set_info(pr, "s1suspend");
	if (!memcmp(mag->magic, "S2SUSPEND", mag->len))
		return swap_set_info(pr, "s2suspend");
	if (!memcmp(mag->magic, "ULSUSPEND", mag->len))
		return swap_set_info(pr, "ulsuspend");
	if (!memcmp(mag->magic, TOI_MAGIC_STRING, mag->len))
		return swap_set_info(pr, "tuxonice");
	if (!memcmp(mag->magic, "LINHIB0001", mag->len))
		return swap_set_info(pr, "linhib0001");

	return -1;	/* no signature detected */
}

const struct blkid_idinfo swap_idinfo =
{
	.name		= "swap",
	.usage		= BLKID_USAGE_OTHER,
	.probefunc	= probe_swap,
	.minsz		= 10 * 4096,	/* 10 pages */
	.magics		=
	{
		{ "SWAP-SPACE", 10, 0,  0xff6 },
		{ "SWAPSPACE2", 10, 0,  0xff6 },
		{ "SWAP-SPACE", 10, 0, 0x1ff6 },
		{ "SWAPSPACE2", 10, 0, 0x1ff6 },
		{ "SWAP-SPACE", 10, 0, 0x3ff6 },
		{ "SWAPSPACE2", 10, 0, 0x3ff6 },
		{ "SWAP-SPACE", 10, 0, 0x7ff6 },
		{ "SWAPSPACE2", 10, 0, 0x7ff6 },
		{ "SWAP-SPACE", 10, 0, 0xfff6 },
		{ "SWAPSPACE2", 10, 0, 0xfff6 },
		{ NULL }
	}
};


const struct blkid_idinfo swsuspend_idinfo =
{
	.name		= "swsuspend",
	.usage		= BLKID_USAGE_OTHER,
	.probefunc	= probe_swsuspend,
	.minsz		= 10 * 4096,	/* 10 pages */
	.magics		=
	{
		{ TOI_MAGIC_STRING, TOI_MAGIC_STRLEN, 0, 0 },
		{ "S1SUSPEND", 9, 0, 0xff6 },
		{ "S2SUSPEND", 9, 0, 0xff6 },
		{ "ULSUSPEND", 9, 0, 0xff6 },
		{ "LINHIB0001",10,0, 0xff6 },

		{ "S1SUSPEND", 9, 0, 0x1ff6 },
		{ "S2SUSPEND", 9, 0, 0x1ff6 },
		{ "ULSUSPEND", 9, 0, 0x1ff6 },
		{ "LINHIB0001",10,0, 0x1ff6 },

		{ "S1SUSPEND", 9, 0, 0x3ff6 },
		{ "S2SUSPEND", 9, 0, 0x3ff6 },
		{ "ULSUSPEND", 9, 0, 0x3ff6 },
		{ "LINHIB0001",10,0, 0x3ff6 },

		{ "S1SUSPEND", 9, 0, 0x7ff6 },
		{ "S2SUSPEND", 9, 0, 0x7ff6 },
		{ "ULSUSPEND", 9, 0, 0x7ff6 },
		{ "LINHIB0001",10,0, 0x7ff6 },

		{ "S1SUSPEND", 9, 0, 0xfff6 },
		{ "S2SUSPEND", 9, 0, 0xfff6 },
		{ "ULSUSPEND", 9, 0, 0xfff6 },
		{ "LINHIB0001",10,0, 0xfff6 },

		{ NULL }
	}
};

