/*
 * Copyright (C) 2009 Mike Hommey <mh@glandium.org>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */
#include "superblocks.h"

struct vmfs_fs_info {
	uint32_t magic;
	uint32_t volume_version;
	uint8_t version;
	uint8_t uuid[16];
	uint32_t mode;
	char label[128];
} __attribute__ ((__packed__));

struct vmfs_volume_info {
	uint32_t magic;
	uint32_t ver;
	uint8_t irrelevant[122];
	uint8_t uuid[16];
} __attribute__ ((__packed__));

static int probe_vmfs_fs(blkid_probe pr, const struct blkid_idmag *mag)
{
	struct vmfs_fs_info *header;

	header = blkid_probe_get_sb(pr, mag, struct vmfs_fs_info);
	if (header == NULL)
		return -1;

	blkid_probe_sprintf_uuid(pr, (unsigned char *) header->uuid, 16,
		"%02x%02x%02x%02x-%02x%02x%02x%02x-"
		"%02x%02x-%02x%02x%02x%02x%02x%02x",
		header->uuid[3], header->uuid[2], header->uuid[1],
		header->uuid[0], header->uuid[7], header->uuid[6],
		header->uuid[5], header->uuid[4], header->uuid[9],
		header->uuid[8], header->uuid[10], header->uuid[11],
		header->uuid[12], header->uuid[13], header->uuid[14],
		header->uuid[15]);

	blkid_probe_set_label(pr, (unsigned char *) header->label,
					sizeof(header->label));
	blkid_probe_sprintf_version(pr, "%u", header->version);
	return 0;
}

static int probe_vmfs_volume(blkid_probe pr, const struct blkid_idmag *mag)
{
	struct vmfs_volume_info *header;
	unsigned char *lvm_uuid;

	header = blkid_probe_get_sb(pr, mag, struct vmfs_volume_info);
	if (header == NULL)
		return -1;

	blkid_probe_sprintf_value(pr, "UUID_SUB",
		"%02x%02x%02x%02x-%02x%02x%02x%02x-"
		"%02x%02x-%02x%02x%02x%02x%02x%02x",
		header->uuid[3], header->uuid[2], header->uuid[1],
		header->uuid[0], header->uuid[7], header->uuid[6],
		header->uuid[5], header->uuid[4], header->uuid[9],
		header->uuid[8], header->uuid[10], header->uuid[11],
		header->uuid[12], header->uuid[13], header->uuid[14],
		header->uuid[15]);
	blkid_probe_sprintf_version(pr, "%u", le32_to_cpu(header->ver));

	lvm_uuid = blkid_probe_get_buffer(pr,
				1024 * 1024 /* Start of the volume info */
				+ 512 /* Offset to lvm info */
				+ 20 /* Offset in lvm info */, 35);
	if (lvm_uuid)
		blkid_probe_strncpy_uuid(pr, lvm_uuid, 35);

	return 0;
}

const struct blkid_idinfo vmfs_fs_idinfo =
{
	.name		= "VMFS",
	.usage		= BLKID_USAGE_FILESYSTEM,
	.probefunc	= probe_vmfs_fs,
	.magics		=
	{
		{ .magic = "\x5e\xf1\xab\x2f", .len = 4, .kboff = 2048 },
		{ NULL }
	}
};

const struct blkid_idinfo vmfs_volume_idinfo =
{
	.name		= "VMFS_volume_member",
	.usage		= BLKID_USAGE_RAID,
	.probefunc	= probe_vmfs_volume,
	.magics		=
	{
		{ .magic = "\x0d\xd0\x01\xc0", .len = 4, .kboff = 1024 },
		{ NULL }
	}
};
