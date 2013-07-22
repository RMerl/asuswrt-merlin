/*
 * uktrix partition parsing code
 *
 * Copyright (C) 2010 Karel Zak <kzak@redhat.com>
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

#define ULTRIX_MAXPARTITIONS	8
#define ULTRIX_MAGIC		0x032957

/* sector with partition table */
#define ULTRIX_SECTOR		((16384 - sizeof(struct ultrix_disklabel)) >> 9)
/* position of partition table within ULTRIX_SECTOR */
#define ULTRIX_OFFSET		(512 - sizeof(struct ultrix_disklabel))

struct ultrix_disklabel {
	int32_t	pt_magic;	/* magic no. indicating part. info exits */
	int32_t	pt_valid;	/* set by driver if pt is current */
	struct  pt_info {
		int32_t		pi_nblocks; /* no. of sectors */
		uint32_t	pi_blkoff;  /* block offset for start */
	} pt_part[ULTRIX_MAXPARTITIONS];
} __attribute__((packed));


static int probe_ultrix_pt(blkid_probe pr,
		const struct blkid_idmag *mag __attribute__((__unused__)))
{
	unsigned char *data;
	struct ultrix_disklabel *l;
	blkid_parttable tab = NULL;
	blkid_partlist ls;
	int i;

	data = blkid_probe_get_sector(pr, ULTRIX_SECTOR);
	if (!data)
		goto nothing;

	l = (struct ultrix_disklabel *) (data + ULTRIX_OFFSET);

	if (l->pt_magic != ULTRIX_MAGIC || l->pt_valid != 1)
		goto nothing;

	if (blkid_partitions_need_typeonly(pr))
		/* caller does not ask for details about partitions */
		return 0;

	ls = blkid_probe_get_partlist(pr);
	if (!ls)
		goto err;

	tab = blkid_partlist_new_parttable(ls, "ultrix", 0);
	if (!tab)
		goto err;

	for (i = 0; i < ULTRIX_MAXPARTITIONS; i++) {
		if (!l->pt_part[i].pi_nblocks)
			 blkid_partlist_increment_partno(ls);
		else {
			if (!blkid_partlist_add_partition(ls, tab,
						l->pt_part[i].pi_blkoff,
						l->pt_part[i].pi_nblocks))
				goto err;
		}
	}

	return 0;
nothing:
	return 1;
err:
	return -1;
}

const struct blkid_idinfo ultrix_pt_idinfo =
{
	.name		= "ultrix",
	.probefunc	= probe_ultrix_pt,
	.magics		= BLKID_NONE_MAGIC
};

