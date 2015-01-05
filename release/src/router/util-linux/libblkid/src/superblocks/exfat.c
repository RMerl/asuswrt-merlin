/*
 * Copyright (C) 2010 Andrew Nayenko <resver@gmail.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */
#include "superblocks.h"

struct exfat_super_block {
	uint8_t jump[3];
	uint8_t oem_name[8];
	uint8_t	__unused1[53];
	uint64_t block_start;
	uint64_t block_count;
	uint32_t fat_block_start;
	uint32_t fat_block_count;
	uint32_t cluster_block_start;
	uint32_t cluster_count;
	uint32_t rootdir_cluster;
	uint8_t volume_serial[4];
	struct {
		uint8_t minor;
		uint8_t major;
	} version;
	uint16_t volume_state;
	uint8_t block_bits;
	uint8_t bpc_bits;
	uint8_t fat_count;
	uint8_t drive_no;
	uint8_t allocated_percent;
} __attribute__((__packed__));

struct exfat_entry_label {
	uint8_t type;
	uint8_t length;
	uint8_t name[30];
} __attribute__((__packed__));

#define BLOCK_SIZE(sb) (1 << (sb)->block_bits)
#define CLUSTER_SIZE(sb) (BLOCK_SIZE(sb) << (sb)->bpc_bits)
#define EXFAT_FIRST_DATA_CLUSTER 2
#define EXFAT_LAST_DATA_CLUSTER 0xffffff6
#define EXFAT_ENTRY_SIZE 32

#define EXFAT_ENTRY_EOD		0x00
#define EXFAT_ENTRY_LABEL	0x83

static blkid_loff_t block_to_offset(const struct exfat_super_block *sb,
		blkid_loff_t block)
{
	return (blkid_loff_t) block << sb->block_bits;
}

static blkid_loff_t cluster_to_block(const struct exfat_super_block *sb,
		uint32_t cluster)
{
	return le32_to_cpu(sb->cluster_block_start) +
			((blkid_loff_t) (cluster - EXFAT_FIRST_DATA_CLUSTER)
					<< sb->bpc_bits);
}

static blkid_loff_t cluster_to_offset(const struct exfat_super_block *sb,
		uint32_t cluster)
{
	return block_to_offset(sb, cluster_to_block(sb, cluster));
}

static uint32_t next_cluster(blkid_probe pr,
		const struct exfat_super_block *sb, uint32_t cluster)
{
	uint32_t *next;
	blkid_loff_t fat_offset;

	fat_offset = block_to_offset(sb, le32_to_cpu(sb->fat_block_start))
		+ (blkid_loff_t) cluster * sizeof(cluster);
	next = (uint32_t *) blkid_probe_get_buffer(pr, fat_offset,
			sizeof(uint32_t));
	if (!next)
		return 0;
	return le32_to_cpu(*next);
}

static struct exfat_entry_label *find_label(blkid_probe pr,
		const struct exfat_super_block *sb)
{
	uint32_t cluster = le32_to_cpu(sb->rootdir_cluster);
	blkid_loff_t offset = cluster_to_offset(sb, cluster);
	uint8_t *entry;

	for (;;) {
		entry = (uint8_t *) blkid_probe_get_buffer(pr, offset,
				EXFAT_ENTRY_SIZE);
		if (!entry)
			return NULL;
		if (entry[0] == EXFAT_ENTRY_EOD)
			return NULL;
		if (entry[0] == EXFAT_ENTRY_LABEL)
			return (struct exfat_entry_label *) entry;
		offset += EXFAT_ENTRY_SIZE;
		if (offset % CLUSTER_SIZE(sb) == 0) {
			cluster = next_cluster(pr, sb, cluster);
			if (cluster < EXFAT_FIRST_DATA_CLUSTER)
				return NULL;
			if (cluster > EXFAT_LAST_DATA_CLUSTER)
				return NULL;
			offset = cluster_to_offset(sb, cluster);
		}
	}
}

static int probe_exfat(blkid_probe pr, const struct blkid_idmag *mag)
{
	struct exfat_super_block *sb;
	struct exfat_entry_label *label;

	sb = blkid_probe_get_sb(pr, mag, struct exfat_super_block);
	if (!sb)
		return -1;

	label = find_label(pr, sb);
	if (label)
		blkid_probe_set_utf8label(pr, label->name,
				min(label->length * 2, 30), BLKID_ENC_UTF16LE);

	blkid_probe_sprintf_uuid(pr, sb->volume_serial, 4,
			"%02hhX%02hhX-%02hhX%02hhX",
			sb->volume_serial[3], sb->volume_serial[2],
			sb->volume_serial[1], sb->volume_serial[0]);

	blkid_probe_sprintf_version(pr, "%u.%u",
			sb->version.major, sb->version.minor);

	return 0;
}

const struct blkid_idinfo exfat_idinfo =
{
	.name		= "exfat",
	.usage		= BLKID_USAGE_FILESYSTEM,
	.probefunc	= probe_exfat,
	.magics		=
	{
		{ .magic = "EXFAT   ", .len = 8, .sboff = 3 },
		{ NULL }
	}
};
