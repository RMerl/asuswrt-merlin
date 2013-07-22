/*
 * sgi partition parsing code
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

#define SGI_MAXPARTITIONS	16

/* partition type */
#define SGI_TYPE_VOLHDR		0x00
#define SGI_TYPE_VOLULME	0x06	/* entire disk */

struct sgi_device_parameter {
	unsigned char skew;
	unsigned char gap1;
	unsigned char gap2;
	unsigned char sparecyl;

	uint16_t pcylcount;
	uint16_t head_vol0;
	uint16_t ntrks;		/* tracks in cyl 0 or vol 0 */

	unsigned char cmd_tag_queue_depth;
	unsigned char unused0;

	uint16_t unused1;
	uint16_t nsect;		/* sectors/tracks in cyl 0 or vol 0 */
	uint16_t bytes;
	uint16_t ilfact;
	uint32_t flags;		/* controller flags */
	uint32_t datarate;
	uint32_t retries_on_error;
	uint32_t ms_per_word;
	uint16_t xylogics_gap1;
	uint16_t xylogics_syncdelay;
	uint16_t xylogics_readdelay;
	uint16_t xylogics_gap2;
	uint16_t xylogics_readgate;
	uint16_t xylogics_writecont;
} __attribute__((packed));

struct sgi_disklabel {
	uint32_t magic;			/* magic number */
	uint16_t root_part_num;		/* # root partition */
	uint16_t swap_part_num;		/* # swap partition */
	unsigned char boot_file[16];	/* name of boot file */

	struct sgi_device_parameter	devparam;	/* not used now */

	struct sgi_volume {
		unsigned char name[8];	/* name of volume */
		uint32_t block_num;	/* logical block number */
		uint32_t num_bytes;	/* how big, in bytes */
	} __attribute__((packed)) volume[15];

	struct sgi_partition {
		uint32_t num_blocks;	/* size in logical blocks */
		uint32_t first_block;	/* first logical block */
		uint32_t type;		/* type of this partition */
	} __attribute__((packed)) partitions[SGI_MAXPARTITIONS];

	/* checksum is the 32bit 2's complement sum of the disklabel */
	uint32_t csum;			/* disk label checksum */
	uint32_t padding;		/* padding */
} __attribute__((packed));

static uint32_t count_checksum(struct sgi_disklabel *label)
{
	int i;
	uint32_t *ptr = (uint32_t *) label;
	uint32_t sum = 0;

	i = sizeof(*label) / sizeof(*ptr);

	while (i--)
		sum += be32_to_cpu(ptr[i]);

	return sum;
}


static int probe_sgi_pt(blkid_probe pr,
		const struct blkid_idmag *mag __attribute__((__unused__)))
{
	struct sgi_disklabel *l;
	struct sgi_partition *p;
	blkid_parttable tab = NULL;
	blkid_partlist ls;
	int i;

	l = (struct sgi_disklabel *) blkid_probe_get_sector(pr, 0);
	if (!l)
		goto nothing;

	if (count_checksum(l)) {
		DBG(DEBUG_LOWPROBE, printf(
			"detected corrupted sgi disk label -- ignore\n"));
		goto nothing;
	}

	if (blkid_partitions_need_typeonly(pr))
		/* caller does not ask for details about partitions */
		return 0;

	ls = blkid_probe_get_partlist(pr);
	if (!ls)
		goto err;

	tab = blkid_partlist_new_parttable(ls, "sgi", 0);
	if (!tab)
		goto err;

	for(i = 0, p = &l->partitions[0]; i < SGI_MAXPARTITIONS; i++, p++) {
		uint32_t size = be32_to_cpu(p->num_blocks);
		uint32_t start = be32_to_cpu(p->first_block);
		uint32_t type = be32_to_cpu(p->type);
		blkid_partition par;

		if (size == 0 || type == SGI_TYPE_VOLULME ||
			         type == SGI_TYPE_VOLHDR) {
			blkid_partlist_increment_partno(ls);
			continue;
		}
		par = blkid_partlist_add_partition(ls, tab, start, size);
		if (!par)
			goto err;

		blkid_partition_set_type(par, type);
	}

	return 0;

nothing:
	return 1;
err:
	return -1;
}

const struct blkid_idinfo sgi_pt_idinfo =
{
	.name		= "sgi",
	.probefunc	= probe_sgi_pt,
	.magics		=
	{
		{ .magic = "\x0B\xE5\xA9\x41", .len = 4	},
		{ NULL }
	}
};

