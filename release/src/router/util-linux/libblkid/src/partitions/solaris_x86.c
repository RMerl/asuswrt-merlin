/*
 * Solaris x86 partition parsing code
 *
 * Copyright (C) 2009 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "partitions.h"

/*
 * Solaris-x86 is always within primary dos partition (nested PT table).  The
 * solaris-x86 vtoc allows to split the entire partition to "slices". The
 * offset (start) of the slice is always relatively to the primary dos
 * partition.
 *
 * Note that Solaris-SPARC uses entire disk with a different partitionning
 * scheme.
 */

/* some other implementation than Linux kernel assume 8 partitions only */
#define SOLARIS_MAXPARTITIONS	16

/* disklabel (vtoc) location  */
#define SOLARIS_SECTOR		1			/* in 512-sectors */
#define SOLARIS_OFFSET		(SOLARIS_SECTOR << 9)	/* in bytes */
#define SOLARIS_MAGICOFFSET	(SOLARIS_OFFSET + 12)	/* v_sanity offset in bytes */

/* slice tags */
#define SOLARIS_TAG_WHOLEDISK	5

struct solaris_slice {
	uint16_t s_tag;      /* ID tag of partition */
	uint16_t s_flag;     /* permission flags */
	uint32_t s_start;    /* start sector no of partition */
	uint32_t s_size;     /* # of blocks in partition */
} __attribute__((packed));

struct solaris_vtoc {
	unsigned int v_bootinfo[3];     /* info needed by mboot (unsupported) */

	uint32_t     v_sanity;          /* to verify vtoc sanity */
	uint32_t     v_version;         /* layout version */
	char         v_volume[8];       /* volume name */
	uint16_t     v_sectorsz;        /* sector size in bytes */
	uint16_t     v_nparts;          /* number of partitions */
	unsigned int v_reserved[10];    /* free space */

	struct solaris_slice v_slice[SOLARIS_MAXPARTITIONS]; /* slices */

	unsigned int timestamp[SOLARIS_MAXPARTITIONS]; /* timestamp (unsupported) */
	char         v_asciilabel[128];	/* for compatibility */
} __attribute__((packed));

static int probe_solaris_pt(blkid_probe pr,
		const struct blkid_idmag *mag __attribute__((__unused__)))
{
	struct solaris_vtoc *l;	/* disk label */
	struct solaris_slice *p;	/* partitsion */
	blkid_parttable tab = NULL;
	blkid_partition parent;
	blkid_partlist ls;
	int i;
	uint16_t nparts;

	l = (struct solaris_vtoc *) blkid_probe_get_sector(pr, SOLARIS_SECTOR);
	if (!l)
		goto nothing;

	if (le32_to_cpu(l->v_version) != 1) {
		DBG(DEBUG_LOWPROBE, printf(
			"WARNING: unsupported solaris x86 version %d, ignore\n",
			le32_to_cpu(l->v_version)));
		goto nothing;
	}

	if (blkid_partitions_need_typeonly(pr))
		/* caller does not ask for details about partitions */
		return 0;

	ls = blkid_probe_get_partlist(pr);
	if (!ls)
		goto err;

	parent = blkid_partlist_get_parent(ls);

	tab = blkid_partlist_new_parttable(ls, "solaris", SOLARIS_OFFSET);
	if (!tab)
		goto err;

	nparts = le16_to_cpu(l->v_nparts);
	if (nparts > SOLARIS_MAXPARTITIONS)
		nparts = SOLARIS_MAXPARTITIONS;

	for (i = 1, p = &l->v_slice[0];	i < nparts; i++, p++) {

		uint32_t start = le32_to_cpu(p->s_start);
		uint32_t size = le32_to_cpu(p->s_size);
		blkid_partition par;

		if (size == 0 || le16_to_cpu(p->s_tag) == SOLARIS_TAG_WHOLEDISK)
			continue;

		if (parent)
			/* Solaris slices are relative to the parent (primary
			 * DOS partition) */
			start += blkid_partition_get_start(parent);

		if (parent && !blkid_is_nested_dimension(parent, start, size)) {
			DBG(DEBUG_LOWPROBE, printf(
				"WARNING: solaris partition (%d) overflow "
				"detected, ignore\n", i));
			continue;
		}

		par = blkid_partlist_add_partition(ls, tab, start, size);
		if (!par)
			goto err;

		blkid_partition_set_type(par, le16_to_cpu(p->s_tag));
		blkid_partition_set_flags(par, le16_to_cpu(p->s_flag));
	}

	return 0;

nothing:
	return 1;
err:
	return -1;
}

const struct blkid_idinfo solaris_x86_pt_idinfo =
{
	.name		= "solaris",
	.probefunc	= probe_solaris_pt,
	.magics		=
	{
		{
		  .magic = "\xEE\xDE\x0D\x60",	/* little-endian magic string */
		  .len = 4,			/* v_sanity size in bytes */
		  .sboff = SOLARIS_MAGICOFFSET	/* offset of v_sanity */
		},
		{ NULL }
	}
};

