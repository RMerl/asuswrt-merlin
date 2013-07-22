/*
 * Copyright (C) 1999 by Andries Brouwer
 * Copyright (C) 1999, 2000, 2003 by Theodore Ts'o
 * Copyright (C) 2001 by Andreas Dilger
 * Copyright (C) 2004 Kay Sievers <kay.sievers@vrfy.org>
 * Copyright (C) 2008 Karel Zak <kzak@redhat.com>
 *
 * Inspired also by libvolume_id by
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

/* PVD - Primary volume descriptor */
struct iso_volume_descriptor {
	unsigned char	vd_type;
	unsigned char	vd_id[5];
	unsigned char	vd_version;
	unsigned char	flags;
	unsigned char	system_id[32];
	unsigned char	volume_id[32];
	unsigned char	unused[8];
	unsigned char	space_size[8];
	unsigned char	escape_sequences[8];
} __attribute__((packed));

#define ISO_SUPERBLOCK_OFFSET		0x8000
#define ISO_SECTOR_SIZE			0x800
#define ISO_VD_OFFSET			(ISO_SUPERBLOCK_OFFSET + ISO_SECTOR_SIZE)
#define ISO_VD_SUPPLEMENTARY		0x2
#define ISO_VD_END			0xff
#define ISO_VD_MAX			16

struct high_sierra_volume_descriptor {
	unsigned char	foo[8];
	unsigned char	type;
	unsigned char	id[5];
	unsigned char	version;
	unsigned char	unused1;
	unsigned char	system_id[32];
	unsigned char   volume_id[32];
} __attribute__((packed));

/* returns 1 if the begin of @ascii is equal to @utf16 string.
 */
static int ascii_eq_utf16be(unsigned char *ascii,
			unsigned char *utf16, size_t len)
{
	size_t a, u;

	for (a = 0, u = 0; u < len; a++, u += 2) {
		if (utf16[u] != 0x0 || ascii[a] != utf16[u + 1])
			return 0;
	}
	return 1;
}

/* old High Sierra format */
static int probe_iso9660_hsfs(blkid_probe pr, const struct blkid_idmag *mag)
{
	struct high_sierra_volume_descriptor *iso;

	iso = blkid_probe_get_sb(pr, mag, struct high_sierra_volume_descriptor);
	if (!iso)
		return -1;

	blkid_probe_set_version(pr, "High Sierra");
	blkid_probe_set_label(pr, iso->volume_id, sizeof(iso->volume_id));
	return 0;
}

/* iso9660 [+ Microsoft Joliet Extension] */
static int probe_iso9660(blkid_probe pr, const struct blkid_idmag *mag)
{
	struct iso_volume_descriptor *iso;
	unsigned char label[32];
	int i;
	int off;

	if (strcmp(mag->magic, "CDROM") == 0)
		return probe_iso9660_hsfs(pr, mag);

	iso = blkid_probe_get_sb(pr, mag, struct iso_volume_descriptor);
	if (!iso)
		return -1;

	memcpy(label, iso->volume_id, sizeof(label));

	/* Joliet Extension */
	off = ISO_VD_OFFSET;
	for (i = 0; i < ISO_VD_MAX; i++) {
		iso = (struct iso_volume_descriptor *)
			blkid_probe_get_buffer(pr,
					off,
					sizeof(struct iso_volume_descriptor));

		if (iso == NULL || iso->vd_type == ISO_VD_END)
			break;
		if (iso->vd_type != ISO_VD_SUPPLEMENTARY)
			continue;

		if (memcmp(iso->escape_sequences, "%/@", 3) == 0 ||
		    memcmp(iso->escape_sequences, "%/C", 3) == 0 ||
		    memcmp(iso->escape_sequences, "%/E", 3) == 0) {

			blkid_probe_set_version(pr, "Joliet Extension");

			/* Is the Joliet (UTF16BE) label equal to the label in
			 * the PVD? If yes, use PVD label.  The Jolied version
			 * of the label could be trimed (because UTF16..).
			 */
			if (ascii_eq_utf16be(label, iso->volume_id, 32))
				break;

			blkid_probe_set_utf8label(pr,
					iso->volume_id,
					sizeof(iso->volume_id),
					BLKID_ENC_UTF16BE);
			goto has_label;
		}
		off += ISO_SECTOR_SIZE;
	}

	/* Joliet not found, let use standard iso label */
	blkid_probe_set_label(pr, label, sizeof(label));

has_label:
	return 0;
}


const struct blkid_idinfo iso9660_idinfo =
{
	.name		= "iso9660",
	.usage		= BLKID_USAGE_FILESYSTEM,
	.probefunc	= probe_iso9660,
	.flags		= BLKID_IDINFO_TOLERANT,
	.magics		=
	{
		{ .magic = "CD001", .len = 5, .kboff = 32, .sboff = 1 },
		{ .magic = "CDROM", .len = 5, .kboff = 32, .sboff = 9 },
		{ NULL }
	}
};

