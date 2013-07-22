/*
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
#include <inttypes.h>

#include "superblocks.h"

struct ntfs_super_block {
	uint8_t		jump[3];
	uint8_t		oem_id[8];
	uint8_t		bios_parameter_block[25];
	uint16_t	unused[2];
	uint64_t	number_of_sectors;
	uint64_t	mft_cluster_location;
	uint64_t	mft_mirror_cluster_location;
	int8_t		cluster_per_mft_record;
	uint8_t		reserved1[3];
	int8_t		cluster_per_index_record;
	uint8_t		reserved2[3];
	uint64_t	volume_serial;
	uint16_t	checksum;
} __attribute__((packed));

struct master_file_table_record {
	uint32_t	magic;
	uint16_t	usa_ofs;
	uint16_t	usa_count;
	uint64_t	lsn;
	uint16_t	sequence_number;
	uint16_t	link_count;
	uint16_t	attrs_offset;
	uint16_t	flags;
	uint32_t	bytes_in_use;
	uint32_t	bytes_allocated;
} __attribute__((__packed__));

struct file_attribute {
	uint32_t	type;
	uint32_t	len;
	uint8_t		non_resident;
	uint8_t		name_len;
	uint16_t	name_offset;
	uint16_t	flags;
	uint16_t	instance;
	uint32_t	value_len;
	uint16_t	value_offset;
} __attribute__((__packed__));

#define MFT_RECORD_VOLUME			3
#define MFT_RECORD_ATTR_VOLUME_NAME		0x60
#define MFT_RECORD_ATTR_VOLUME_INFO		0x70
#define MFT_RECORD_ATTR_OBJECT_ID		0x40
#define MFT_RECORD_ATTR_END			0xffffffffu

static int probe_ntfs(blkid_probe pr, const struct blkid_idmag *mag)
{
	struct ntfs_super_block *ns;
	struct master_file_table_record *mft;
	struct file_attribute *attr;
	int		bytes_per_sector, sectors_per_cluster;
	int		mft_record_size, attr_off, attr_len;
	unsigned int	attr_type, val_len;
	int		val_off;
	uint64_t		nr_clusters;
	blkid_loff_t off;
	unsigned char *buf_mft, *val;

	ns = blkid_probe_get_sb(pr, mag, struct ntfs_super_block);
	if (!ns)
		return -1;

	bytes_per_sector = ns->bios_parameter_block[0] +
		(ns->bios_parameter_block[1]  << 8);
	sectors_per_cluster = ns->bios_parameter_block[2];

	if ((bytes_per_sector < 512) || (sectors_per_cluster == 0))
		return 1;

	if (ns->cluster_per_mft_record < 0)
		mft_record_size = 1 << (0 - ns->cluster_per_mft_record);
	else
		mft_record_size = ns->cluster_per_mft_record *
			sectors_per_cluster * bytes_per_sector;
	nr_clusters = le64_to_cpu(ns->number_of_sectors) / sectors_per_cluster;

	if ((le64_to_cpu(ns->mft_cluster_location) > nr_clusters) ||
	    (le64_to_cpu(ns->mft_mirror_cluster_location) > nr_clusters))
		return 1;

	off = le64_to_cpu(ns->mft_mirror_cluster_location) *
		bytes_per_sector * sectors_per_cluster;

	buf_mft = blkid_probe_get_buffer(pr, off, mft_record_size);
	if (!buf_mft)
		return 1;

	if (memcmp(buf_mft, "FILE", 4))
		return 1;

	off = le64_to_cpu(ns->mft_cluster_location) * bytes_per_sector *
		sectors_per_cluster;

	buf_mft = blkid_probe_get_buffer(pr, off, mft_record_size);
	if (!buf_mft)
		return 1;

	if (memcmp(buf_mft, "FILE", 4))
		return 1;

	off += MFT_RECORD_VOLUME * mft_record_size;

	buf_mft = blkid_probe_get_buffer(pr, off, mft_record_size);
	if (!buf_mft)
		return 1;

	if (memcmp(buf_mft, "FILE", 4))
		return 1;

	mft = (struct master_file_table_record *) buf_mft;

	attr_off = le16_to_cpu(mft->attrs_offset);

	while (1) {
		attr = (struct file_attribute *) (buf_mft + attr_off);
		attr_len = le32_to_cpu(attr->len);
		attr_type = le32_to_cpu(attr->type);
		val_off = le16_to_cpu(attr->value_offset);
		val_len = le32_to_cpu(attr->value_len);

		attr_off += attr_len;

		if ((attr_off > mft_record_size) ||
		    (attr_len == 0))
			break;

		if (attr_type == MFT_RECORD_ATTR_END)
			break;

		if (attr_type == MFT_RECORD_ATTR_VOLUME_NAME) {
			val = ((uint8_t *) attr) + val_off;
			blkid_probe_set_utf8label(pr, val, val_len, BLKID_ENC_UTF16LE);
		}
	}

	blkid_probe_sprintf_uuid(pr,
			(unsigned char *) &ns->volume_serial,
			sizeof(ns->volume_serial),
			"%016" PRIX64, le64_to_cpu(ns->volume_serial));
	return 0;
}


const struct blkid_idinfo ntfs_idinfo =
{
	.name		= "ntfs",
	.usage		= BLKID_USAGE_FILESYSTEM,
	.probefunc	= probe_ntfs,
	.magics		=
	{
		{ .magic = "NTFS    ", .len = 8, .sboff = 3 },
		{ NULL }
	}
};

