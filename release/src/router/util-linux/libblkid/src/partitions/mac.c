/*
 * mac partitions parsing code
 *
 * Copyright (C) 2009 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 *
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "partitions.h"

#define MAC_PARTITION_MAGIC		0x504d
#define MAC_PARTITION_MAGIC_OLD		0x5453

/*
 * Mac partition entry
 * http://developer.apple.com/legacy/mac/library/documentation/mac/Devices/Devices-126.html
 */
struct mac_partition {
	uint16_t	signature;	/* expected to be MAC_PARTITION_MAGIC */
	uint16_t	reserved;	/* reserved */
	uint32_t	map_count;	/* # blocks in partition map */
	uint32_t	start_block;	/* absolute starting block # of partition */
	uint32_t	block_count;	/* number of blocks in partition */
	char		name[32];	/* partition name */
	char		type[32];	/* string type description */
	uint32_t	data_start;	/* rel block # of first data block */
	uint32_t	data_count;	/* number of data blocks */
	uint32_t	status;		/* partition status bits */
	uint32_t	boot_start;	/* first logical block of boot code */
	uint32_t	boot_size;	/* size of boot code, in bytes */
	uint32_t	boot_load;	/* boot code load address */
	uint32_t	boot_load2;	/* reserved */
	uint32_t	boot_entry;	/* boot code entry point */
	uint32_t	boot_entry2;	/* reserved */
	uint32_t	boot_cksum;	/* boot code checksum */
	char		processor[16];	/* identifies ISA of boot */

	/* there is more stuff after this that we don't need */
} __attribute__((packed));

/*
 * Driver descriptor structure, in block 0
 * http://developer.apple.com/legacy/mac/library/documentation/mac/Devices/Devices-121.html
 */
struct mac_driver_desc {
	uint16_t	signature;	/* expected to be MAC_DRIVER_MAGIC */
	uint16_t	block_size;	/* block size of the device */
	uint32_t	block_count;	/* number of blocks on the device */

	/* there is more stuff after this that we don't need */
} __attribute__((packed));

static inline unsigned char *get_mac_block(
					blkid_probe pr,
					uint16_t block_size,
					uint32_t num)
{
	return blkid_probe_get_buffer(pr,
			(blkid_loff_t) num * block_size, block_size);
}

static inline int has_part_signature(struct mac_partition *p)
{
	return	be16_to_cpu(p->signature) == MAC_PARTITION_MAGIC ||
		be16_to_cpu(p->signature) == MAC_PARTITION_MAGIC_OLD;
}

static int probe_mac_pt(blkid_probe pr,
		const struct blkid_idmag *mag __attribute__((__unused__)))
{
	struct mac_driver_desc *md;
	struct mac_partition *p;
	blkid_parttable tab = NULL;
	blkid_partlist ls;
	uint16_t block_size;
	uint16_t ssf;	/* sector size fragment */
	uint32_t nblks, i;


	/* The driver descriptor record is always located at physical block 0,
	 * the first block on the disk.
	 */
	md = (struct mac_driver_desc *) blkid_probe_get_sector(pr, 0);
	if (!md)
		goto nothing;

	block_size = be16_to_cpu(md->block_size);

	/* The partition map always begins at physical block 1,
	 * the second block on the disk.
	 */
	p = (struct mac_partition *) get_mac_block(pr, block_size, 1);
	if (!p)
		goto nothing;

	/* check the first partition signature */
	if (!has_part_signature(p))
		goto nothing;

	if (blkid_partitions_need_typeonly(pr))
		/* caller does not ask for details about partitions */
		return 0;

	ls = blkid_probe_get_partlist(pr);
	if (!ls)
		goto err;

	tab = blkid_partlist_new_parttable(ls, "mac", 0);
	if (!tab)
		goto err;

	ssf = block_size / 512;
	nblks = be32_to_cpu(p->map_count);

	for (i = 1; i <= nblks; ++i) {
		blkid_partition par;
		uint32_t start;
		uint32_t size;

		p = (struct mac_partition *) get_mac_block(pr, block_size, i);
		if (!p)
			goto nothing;
		if (!has_part_signature(p))
			goto nothing;

		if (be32_to_cpu(p->map_count) != nblks) {
			DBG(DEBUG_LOWPROBE, printf(
				"mac: inconsisten map_count in partition map, "
			        "entry[0]: %d, entry[%d]: %d\n",
				nblks, i - 1,
				be32_to_cpu(p->map_count)));
		}

		/*
		 * note that libparted ignores some mac partitions according to
		 * the partition name (e.g. "Apple_Free" or "Apple_Void"). We
		 * follows Linux kernel and all partitions are visible
		 */

		start = be32_to_cpu(p->start_block) * ssf;
		size = be32_to_cpu(p->block_count) * ssf;

		par = blkid_partlist_add_partition(ls, tab, start, size);
		if (!par)
			goto err;

		blkid_partition_set_name(par, (unsigned char *) p->name,
						sizeof(p->name));

		blkid_partition_set_type_string(par, (unsigned char *) p->type,
						sizeof(p->type));
	}

	return 0;

nothing:
	return 1;
err:
	return -1;
}

/*
 * Mac disk always begin with "Driver Descriptor Record"
 * (struct mac_driver_desc) and magic 0x4552.
 */
const struct blkid_idinfo mac_pt_idinfo =
{
	.name		= "mac",
	.probefunc	= probe_mac_pt,
	.magics		=
	{
		/* big-endian magic string */
		{ .magic = "\x45\x52", .len = 2 },
		{ NULL }
	}
};

