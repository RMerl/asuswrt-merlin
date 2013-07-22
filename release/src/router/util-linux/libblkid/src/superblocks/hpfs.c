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

struct hpfs_boot_block
{
	uint8_t		jmp[3];
	uint8_t		oem_id[8];
	uint8_t		bytes_per_sector[2];
	uint8_t		sectors_per_cluster;
	uint8_t		n_reserved_sectors[2];
	uint8_t		n_fats;
	uint8_t		n_rootdir_entries[2];
	uint8_t		n_sectors_s[2];
	uint8_t		media_byte;
	uint16_t	sectors_per_fat;
	uint16_t	sectors_per_track;
	uint16_t	heads_per_cyl;
	uint32_t	n_hidden_sectors;
	uint32_t	n_sectors_l;
	uint8_t		drive_number;
	uint8_t		mbz;
	uint8_t		sig_28h;
	uint8_t		vol_serno[4];
	uint8_t		vol_label[11];
	uint8_t		sig_hpfs[8];
	uint8_t		pad[448];
	uint8_t		magic[2];
} __attribute__((packed));

struct hpfs_super_block
{
	uint8_t		magic[4];
	uint8_t		magic1[4];
	uint8_t		version;
} __attribute__((packed));

struct hpfs_spare_super
{
	uint8_t		magic[4];
	uint8_t		magic1[4];
} __attribute__((packed));


#define HPFS_SB_OFFSET			0x2000
#define HPFS_SBSPARE_OFFSET		0x2200

static int probe_hpfs(blkid_probe pr, const struct blkid_idmag *mag)
{
	struct hpfs_super_block *hs;
	struct hpfs_spare_super *hss;
	struct hpfs_boot_block *hbb;
	uint8_t version;

	/* super block */
	hs = blkid_probe_get_sb(pr, mag, struct hpfs_super_block);
	if (!hs)
		return -1;
	version = hs->version;

	/* spare super block */
	hss = (struct hpfs_spare_super *)
			blkid_probe_get_buffer(pr,
				HPFS_SBSPARE_OFFSET,
				sizeof(struct hpfs_spare_super));
	if (!hss)
		return -1;
	if (memcmp(hss->magic, "\x49\x18\x91\xf9", 4) != 0)
		return -1;

	/* boot block (with UUID and LABEL) */
	hbb = (struct hpfs_boot_block *)
			blkid_probe_get_buffer(pr,
				0,
				sizeof(struct hpfs_boot_block));
	if (!hbb)
		return -1;
	if (memcmp(hbb->magic, "\x55\xaa", 2) == 0 &&
	    memcmp(hbb->sig_hpfs, "HPFS", 4) == 0 &&
	    hbb->sig_28h == 0x28) {
		blkid_probe_set_label(pr, hbb->vol_label, sizeof(hbb->vol_label));
		blkid_probe_sprintf_uuid(pr,
				hbb->vol_serno, sizeof(hbb->vol_serno),
				"%02X%02X-%02X%02X",
				hbb->vol_serno[3], hbb->vol_serno[2],
				hbb->vol_serno[1], hbb->vol_serno[0]);
	}
	blkid_probe_sprintf_version(pr, "%u", version);

	return 0;
}

const struct blkid_idinfo hpfs_idinfo =
{
	.name		= "hpfs",
	.usage		= BLKID_USAGE_FILESYSTEM,
	.probefunc	= probe_hpfs,
	.magics		=
	{
		{
		  .magic = "\x49\xe8\x95\xf9",
		  .len = 4,
		  .kboff = (HPFS_SB_OFFSET >> 10)
		},
		{ NULL }
	}
};


