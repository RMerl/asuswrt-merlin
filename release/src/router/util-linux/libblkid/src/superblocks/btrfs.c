/*
 * Copyright (C) 2009 Karel Zak <kzak@redhat.com>
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

struct btrfs_super_block {
	uint8_t csum[32];
	uint8_t fsid[16];
	uint64_t bytenr;
	uint64_t flags;
	uint8_t magic[8];
	uint64_t generation;
	uint64_t root;
	uint64_t chunk_root;
	uint64_t log_root;
	uint64_t log_root_transid;
	uint64_t total_bytes;
	uint64_t bytes_used;
	uint64_t root_dir_objectid;
	uint64_t num_devices;
	uint32_t sectorsize;
	uint32_t nodesize;
	uint32_t leafsize;
	uint32_t stripesize;
	uint32_t sys_chunk_array_size;
	uint64_t chunk_root_generation;
	uint64_t compat_flags;
	uint64_t compat_ro_flags;
	uint64_t incompat_flags;
	uint16_t csum_type;
	uint8_t root_level;
	uint8_t chunk_root_level;
	uint8_t log_root_level;
	struct btrfs_dev_item {
		uint64_t devid;
		uint64_t total_bytes;
		uint64_t bytes_used;
		uint32_t io_align;
		uint32_t io_width;
		uint32_t sector_size;
		uint64_t type;
		uint64_t generation;
		uint64_t start_offset;
		uint32_t dev_group;
		uint8_t seek_speed;
		uint8_t bandwidth;
		uint8_t uuid[16];
		uint8_t fsid[16];
	} __attribute__ ((__packed__)) dev_item;
	uint8_t label[256];
} __attribute__ ((__packed__));

static int probe_btrfs(blkid_probe pr, const struct blkid_idmag *mag)
{
	struct btrfs_super_block *bfs;

	bfs = blkid_probe_get_sb(pr, mag, struct btrfs_super_block);
	if (!bfs)
		return -1;

	if (*bfs->label)
		blkid_probe_set_label(pr,
				(unsigned char *) bfs->label,
				sizeof(bfs->label));

	blkid_probe_set_uuid(pr, bfs->fsid);
	blkid_probe_set_uuid_as(pr, bfs->dev_item.uuid, "UUID_SUB");

	return 0;
}

const struct blkid_idinfo btrfs_idinfo =
{
	.name		= "btrfs",
	.usage		= BLKID_USAGE_FILESYSTEM,
	.probefunc	= probe_btrfs,
	.minsz		= 1024 * 1024,
	.magics		=
	{
		{ .magic = "_BHRfS_M", .len = 8, .kboff = 64, .sboff = 0x40 },
		{ NULL }
	}
};

