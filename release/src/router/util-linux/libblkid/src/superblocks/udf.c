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

struct volume_descriptor {
	struct descriptor_tag {
		uint16_t	id;
		uint16_t	version;
		uint8_t		checksum;
		uint8_t		reserved;
		uint16_t	serial;
		uint16_t	crc;
		uint16_t	crc_len;
		uint32_t	location;
	} __attribute__((packed)) tag;

	union {
		struct anchor_descriptor {
			uint32_t	length;
			uint32_t	location;
		} __attribute__((packed)) anchor;

		struct primary_descriptor {
			uint32_t	seq_num;
			uint32_t	desc_num;
			struct dstring {
				uint8_t	clen;
				uint8_t	c[31];
			} __attribute__((packed)) ident;
		} __attribute__((packed)) primary;

	} __attribute__((packed)) type;

} __attribute__((packed));

struct volume_structure_descriptor {
	uint8_t		type;
	uint8_t		id[5];
	uint8_t		version;
} __attribute__((packed));

#define UDF_VSD_OFFSET			0x8000LL

static int probe_udf(blkid_probe pr,
		const struct blkid_idmag *mag __attribute__((__unused__)))
{
	struct volume_descriptor *vd;
	struct volume_structure_descriptor *vsd;
	unsigned int bs;
	unsigned int b;
	unsigned int type;
	unsigned int count;
	unsigned int loc;

	/* search Volume Sequence Descriptor (VSD) to get the logical
	 * block size of the volume */
	for (bs = 0x800; bs < 0x8000; bs += 0x800) {
		vsd = (struct volume_structure_descriptor *)
			blkid_probe_get_buffer(pr,
					UDF_VSD_OFFSET + bs,
					sizeof(*vsd));
		if (!vsd)
			return 1;
		if (vsd->id[0] != '\0')
			goto nsr;
	}
	return -1;

nsr:
	/* search the list of VSDs for a NSR descriptor */
	for (b = 0; b < 64; b++) {
		vsd = (struct volume_structure_descriptor *)
			blkid_probe_get_buffer(pr,
					UDF_VSD_OFFSET + ((blkid_loff_t) b * bs),
					sizeof(*vsd));
		if (!vsd)
			return -1;
		if (vsd->id[0] == '\0')
			return -1;
		if (memcmp(vsd->id, "NSR02", 5) == 0)
			goto anchor;
		if (memcmp(vsd->id, "NSR03", 5) == 0)
			goto anchor;
	}
	return -1;

anchor:
	/* read Anchor Volume Descriptor (AVDP) */
	vd = (struct volume_descriptor *)
		blkid_probe_get_buffer(pr, 256 * bs, sizeof(*vd));
	if (!vd)
		return -1;

	type = le16_to_cpu(vd->tag.id);
	if (type != 2) /* TAG_ID_AVDP */
		return 0;

	/* get desriptor list address and block count */
	count = le32_to_cpu(vd->type.anchor.length) / bs;
	loc = le32_to_cpu(vd->type.anchor.location);

	/* pick the primary descriptor from the list */
	for (b = 0; b < count; b++) {
		vd = (struct volume_descriptor *)
			blkid_probe_get_buffer(pr,
					(blkid_loff_t) (loc + b) * bs,
					sizeof(*vd));
		if (!vd)
			return -1;

		type = le16_to_cpu(vd->tag.id);
		if (type == 0)
			break;
		if (le32_to_cpu(vd->tag.location) != loc + b)
			break;
		if (type == 1) { /* TAG_ID_PVD */
			uint8_t clen = vd->type.primary.ident.clen;

			if (clen == 8)
				blkid_probe_set_label(pr,
						vd->type.primary.ident.c, 31);
			else if (clen == 16)
				blkid_probe_set_utf8label(pr,
						vd->type.primary.ident.c,
						31, BLKID_ENC_UTF16BE);

			if (clen == 8 || clen == 16)
				break;
		}
	}

	return 0;
}


const struct blkid_idinfo udf_idinfo =
{
	.name		= "udf",
	.usage		= BLKID_USAGE_FILESYSTEM,
	.probefunc	= probe_udf,
	.flags		= BLKID_IDINFO_TOLERANT,
	.magics		=
	{
		{ .magic = "BEA01", .len = 5, .kboff = 32, .sboff = 1 },
		{ .magic = "BOOT2", .len = 5, .kboff = 32, .sboff = 1 },
		{ .magic = "CD001", .len = 5, .kboff = 32, .sboff = 1 },
		{ .magic = "CDW02", .len = 5, .kboff = 32, .sboff = 1 },
		{ .magic = "NSR02", .len = 5, .kboff = 32, .sboff = 1 },
		{ .magic = "NSR03", .len = 5, .kboff = 32, .sboff = 1 },
		{ .magic = "TEA01", .len = 5, .kboff = 32, .sboff = 1 },
		{ NULL }
	}
};
