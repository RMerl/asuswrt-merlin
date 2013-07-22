/*
 * unixware partition parsing code
 *
 * Copyright (C) 2009 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 *
 *
 * The intersting information about unixware PT:
 *   - Linux kernel / partx
 *   - vtoc(7) SCO UNIX command man page
 *   - evms source code (http://evms.sourceforge.net/)
 *   - vxtools source code (http://martin.hinner.info/fs/vxfs/)
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "partitions.h"

/* disklabel location */
#define UNIXWARE_SECTOR		29
#define UNIXWARE_OFFSET		(UNIXWARE_SECTOR << 9)	/* offset in bytes */
#define UNIXWARE_KBOFFSET	(UNIXWARE_OFFSET >> 10)	/* offset in 1024-blocks */

/* disklabel->d_magic offset within the last 1024 block */
#define UNIXWARE_MAGICOFFSET	(UNIXWARE_OFFSET - UNIXWARE_KBOFFSET + 4)

#define UNIXWARE_VTOCMAGIC	0x600DDEEEUL
#define UNIXWARE_MAXPARTITIONS	16

/* unixware_partition->s_label flags */
#define UNIXWARE_TAG_UNUSED       0x0000  /* unused partition */
#define UNIXWARE_TAG_BOOT         0x0001  /* boot fs */
#define UNIXWARE_TAG_ROOT         0x0002  /* root fs */
#define UNIXWARE_TAG_SWAP         0x0003  /* swap fs */
#define UNIXWARE_TAG_USER         0x0004  /* user fs */
#define UNIXWARE_TAG_ENTIRE_DISK  0x0005  /* whole disk */
#define UNIXWARE_TAG_ALT_S        0x0006  /* alternate sector space */
#define UNIXWARE_TAG_OTHER        0x0007  /* non unix */
#define UNIXWARE_TAG_ALT_T        0x0008  /* alternate track space */
#define UNIXWARE_TAG_STAND        0x0009  /* stand partition */
#define UNIXWARE_TAG_VAR          0x000a  /* var partition */
#define UNIXWARE_TAG_HOME         0x000b  /* home partition */
#define UNIXWARE_TAG_DUMP         0x000c  /* dump partition */
#define UNIXWARE_TAG_ALT_ST       0x000d  /* alternate sector track */
#define UNIXWARE_TAG_VM_PUBLIC    0x000e  /* volume mgt public partition */
#define UNIXWARE_TAG_VM_PRIVATE   0x000f  /* volume mgt private partition */


/* unixware_partition->s_flags flags */
#define UNIXWARE_FLAG_VALID	0x0200

struct unixware_partition {
	uint16_t	s_label;	/* partition label (tag) */
	uint16_t	s_flags;	/* permission flags */
	uint32_t	start_sect;	/* starting sector */
	uint32_t	nr_sects;	/* number of sectors */
} __attribute__((packed));

struct unixware_disklabel {
	uint32_t	d_type;		/* drive type */
	uint32_t	d_magic;	/* the magic number */
	uint32_t	d_version;	/* version number */
	char		d_serial[12];	/* serial number of the device */
	uint32_t	d_ncylinders;	/* # of data cylinders per device */
	uint32_t	d_ntracks;	/* # of tracks per cylinder */
	uint32_t	d_nsectors;	/* # of data sectors per track */
	uint32_t	d_secsize;	/* # of bytes per sector */
	uint32_t	d_part_start;	/* # of first sector of this partition */
	uint32_t	d_unknown1[12];	/* ? */
	uint32_t	d_alt_tbl;	/* byte offset of alternate table */
	uint32_t	d_alt_len;	/* byte length of alternate table */
	uint32_t	d_phys_cyl;	/* # of physical cylinders per device */
	uint32_t	d_phys_trk;	/* # of physical tracks per cylinder */
	uint32_t	d_phys_sec;	/* # of physical sectors per track */
	uint32_t	d_phys_bytes;	/* # of physical bytes per sector */
	uint32_t	d_unknown2;	/* ? */
	uint32_t	d_unknown3;	/* ? */
	uint32_t	d_pad[8];	/* pad */

	struct unixware_vtoc {
		uint32_t	v_magic;	/* the magic number */
		uint32_t	v_version;	/* version number */
		char		v_name[8];	/* volume name */
		uint16_t	v_nslices;	/* # of partitions */
		uint16_t	v_unknown1;	/* ? */
		uint32_t	v_reserved[10];	/* reserved */

		struct unixware_partition
			v_slice[UNIXWARE_MAXPARTITIONS]; /* partition */
	} __attribute__((packed)) vtoc;
};

static int probe_unixware_pt(blkid_probe pr,
		const struct blkid_idmag *mag __attribute__((__unused__)))
{
	struct unixware_disklabel *l;
	struct unixware_partition *p;
	blkid_parttable tab = NULL;
	blkid_partition parent;
	blkid_partlist ls;
	int i;

	l = (struct unixware_disklabel *)
			blkid_probe_get_sector(pr, UNIXWARE_SECTOR);
	if (!l)
		goto nothing;

	if (le32_to_cpu(l->vtoc.v_magic) != UNIXWARE_VTOCMAGIC)
		goto nothing;

	if (blkid_partitions_need_typeonly(pr))
		/* caller does not ask for details about partitions */
		return 0;

	ls = blkid_probe_get_partlist(pr);
	if (!ls)
		goto err;

	parent = blkid_partlist_get_parent(ls);

	tab = blkid_partlist_new_parttable(ls, "unixware", UNIXWARE_OFFSET);
	if (!tab)
		goto err;

	/* Skip the first partition that describe whole disk
	 */
	for (i = 1, p = &l->vtoc.v_slice[1];
			i < UNIXWARE_MAXPARTITIONS; i++, p++) {

		uint32_t start, size;
		uint16_t tag, flg;
		blkid_partition par;

		tag = le16_to_cpu(p->s_label);
		flg = le16_to_cpu(p->s_flags);

		if (tag == UNIXWARE_TAG_UNUSED ||
		    tag == UNIXWARE_TAG_ENTIRE_DISK ||
		    flg != UNIXWARE_FLAG_VALID)
			continue;

		start = le32_to_cpu(p->start_sect);
		size = le32_to_cpu(p->nr_sects);

		if (parent && !blkid_is_nested_dimension(parent, start, size)) {
			DBG(DEBUG_LOWPROBE, printf(
				"WARNING: unixware partition (%d) overflow "
				"detected, ignore\n", i));
			continue;
		}

		par = blkid_partlist_add_partition(ls, tab, start, size);
		if (!par)
			goto err;

		blkid_partition_set_type(par, tag);
		blkid_partition_set_flags(par, flg);
	}

	return 0;

nothing:
	return 1;
err:
	return -1;
}


/*
 * The unixware partition table is within primary DOS partition.  The PT is
 * located on 29 sector, PT magic string is d_magic member of 'struct
 * unixware_disklabel'.
 */
const struct blkid_idinfo unixware_pt_idinfo =
{
	.name		= "unixware",
	.probefunc	= probe_unixware_pt,
	.minsz		= 1024 * 1440 + 1,		/* ignore floppies */
	.magics		=
	{
		{
		  .magic = "\x0D\x60\xE5\xCA",	/* little-endian magic string */
		  .len = 4,			/* d_magic size in bytes */
		  .kboff = UNIXWARE_KBOFFSET,
		  .sboff = UNIXWARE_MAGICOFFSET
		},
		{ NULL }
	}
};

