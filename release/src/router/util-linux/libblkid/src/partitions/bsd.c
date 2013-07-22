/*
 * BSD/OSF partition parsing code
 *
 * Copyright (C) 2009 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 *
 * Inspired by fdisk, partx, Linux kernel, libparted and openbsd header files.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "partitions.h"

#define BSD_MAXPARTITIONS	16
#define BSD_FS_UNUSED		0

struct bsd_disklabel {
	uint32_t	d_magic;		/* the magic number */
	int16_t		d_type;			/* drive type */
	int16_t		d_subtype;		/* controller/d_type specific */
	char		d_typename[16];		/* type name, e.g. "eagle" */
	char		d_packname[16];		/* pack identifier */

			/* disk geometry: */
	uint32_t	d_secsize;		/* # of bytes per sector */
	uint32_t	d_nsectors;		/* # of data sectors per track */
	uint32_t	d_ntracks;		/* # of tracks per cylinder */
	uint32_t	d_ncylinders;		/* # of data cylinders per unit */
	uint32_t	d_secpercyl;		/* # of data sectors per cylinder */
	uint32_t	d_secperunit;		/* # of data sectors per unit */

	/*
	 * Spares (bad sector replacements) below
	 * are not counted in d_nsectors or d_secpercyl.
	 * Spare sectors are assumed to be physical sectors
	 * which occupy space at the end of each track and/or cylinder.
	 */
	uint16_t	d_sparespertrack;	/* # of spare sectors per track */
	uint16_t	d_sparespercyl;		/* # of spare sectors per cylinder */

	/*
	 * Alternate cylinders include maintenance, replacement,
	 * configuration description areas, etc.
	 */
	uint32_t	d_acylinders;		/* # of alt. cylinders per unit */

			/* hardware characteristics: */
	/*
	 * d_interleave, d_trackskew and d_cylskew describe perturbations
	 * in the media format used to compensate for a slow controller.
	 * Interleave is physical sector interleave, set up by the formatter
	 * or controller when formatting.  When interleaving is in use,
	 * logically adjacent sectors are not physically contiguous,
	 * but instead are separated by some number of sectors.
	 * It is specified as the ratio of physical sectors traversed
	 * per logical sector.  Thus an interleave of 1:1 implies contiguous
	 * layout, while 2:1 implies that logical sector 0 is separated
	 * by one sector from logical sector 1.
	 * d_trackskew is the offset of sector 0 on track N
	 * relative to sector 0 on track N-1 on the same cylinder.
	 * Finally, d_cylskew is the offset of sector 0 on cylinder N
	 * relative to sector 0 on cylinder N-1.
	 */
	uint16_t	d_rpm;			/* rotational speed */
	uint16_t	d_interleave;		/* hardware sector interleave */
	uint16_t	d_trackskew;		/* sector 0 skew, per track */
	uint16_t	d_cylskew;		/* sector 0 skew, per cylinder */
	uint32_t	d_headswitch;		/* head switch time, usec */
	uint32_t	d_trkseek;		/* track-to-track seek, usec */
	uint32_t	d_flags;		/* generic flags */
	uint32_t	d_drivedata[5];		/* drive-type specific information */
	uint32_t	d_spare[5];		/* reserved for future use */
	uint32_t	d_magic2;		/* the magic number (again) */
	uint16_t	d_checksum;		/* xor of data incl. partitions */

			/* filesystem and partition information: */
	uint16_t	d_npartitions;	        /* number of partitions in following */
	uint32_t	d_bbsize;	        /* size of boot area at sn0, bytes */
	uint32_t	d_sbsize;	        /* max size of fs superblock, bytes */

	struct bsd_partition	 {		/* the partition table */
		uint32_t	p_size;	        /* number of sectors in partition */
		uint32_t	p_offset;       /* starting sector */
		uint32_t	p_fsize;        /* filesystem basic fragment size */
		uint8_t		p_fstype;	/* filesystem type, see below */
		uint8_t		p_frag;		/* filesystem fragments per block */
		uint16_t	p_cpg;	        /* filesystem cylinders per group */
	} __attribute__((packed)) d_partitions[BSD_MAXPARTITIONS];	/* actually may be more */
} __attribute__((packed));


/* Returns 'blkid_idmag' in 512-sectors */
#define BLKID_MAG_SECTOR(_mag)  (((_mag)->kboff * 2)  + ((_mag)->sboff >> 9))

/* Returns 'blkid_idmag' in bytes */
#define BLKID_MAG_OFFSET(_mag)  ((_mag)->kboff >> 10) + ((_mag)->sboff)

/* Returns 'blkid_idmag' offset in bytes within the last sector */
#define BLKID_MAG_LASTOFFSET(_mag) \
		 (BLKID_MAG_OFFSET(_mag) - (BLKID_MAG_SECTOR(_mag) << 9))

static int probe_bsd_pt(blkid_probe pr, const struct blkid_idmag *mag)
{
	struct bsd_disklabel *l;
	struct bsd_partition *p;
	const char *name = "bsd" ;
	blkid_parttable tab = NULL;
	blkid_partition parent;
	blkid_partlist ls;
	int i, nparts = BSD_MAXPARTITIONS;
	unsigned char *data;

	if (blkid_partitions_need_typeonly(pr))
		/* caller does not ask for details about partitions */
		return 0;

	data = blkid_probe_get_sector(pr, BLKID_MAG_SECTOR(mag));
	if (!data)
		goto nothing;

	l = (struct bsd_disklabel *) data + BLKID_MAG_LASTOFFSET(mag);

	ls = blkid_probe_get_partlist(pr);
	if (!ls)
		goto err;

	/* try to determine the real type of BSD system according to
	 * (parental) primary partition */
	parent = blkid_partlist_get_parent(ls);
	if (parent) {
		switch(blkid_partition_get_type(parent)) {
		case BLKID_FREEBSD_PARTITION:
			name = "freebsd";
			break;
		case BLKID_NETBSD_PARTITION:
			name = "netbsd";
			break;
		case BLKID_OPENBSD_PARTITION:
			name = "openbsd";
			break;
		default:
			DBG(DEBUG_LOWPROBE, printf(
				"WARNING: BSD label detected on unknown (0x%x) "
				"primary partition\n",
				blkid_partition_get_type(parent)));
			break;
		}
	}

	tab = blkid_partlist_new_parttable(ls, name, BLKID_MAG_OFFSET(mag));
	if (!tab)
		goto err;

	if (le16_to_cpu(l->d_npartitions) < BSD_MAXPARTITIONS)
		nparts = le16_to_cpu(l->d_npartitions);

	else if (le16_to_cpu(l->d_npartitions) > BSD_MAXPARTITIONS)
		DBG(DEBUG_LOWPROBE, printf(
			"WARNING: ignore %d more BSD partitions\n",
			le16_to_cpu(l->d_npartitions) - BSD_MAXPARTITIONS));

	for (i = 0, p = l->d_partitions; i < nparts; i++, p++) {
		blkid_partition par;
		uint32_t start, size;

		/* TODO: in fdisk-mode returns all non-zero (p_size) partitions */
		if (p->p_fstype == BSD_FS_UNUSED)
			continue;

		start = le32_to_cpu(p->p_offset);
		size = le32_to_cpu(p->p_size);

		if (parent && !blkid_is_nested_dimension(parent, start, size)) {
			DBG(DEBUG_LOWPROBE, printf(
				"WARNING: BSD partition (%d) overflow "
				"detected, ignore\n", i));
			continue;
		}

		par = blkid_partlist_add_partition(ls, tab, start, size);
		if (!par)
			goto err;

		blkid_partition_set_type(par, p->p_fstype);
	}

	return 0;

nothing:
	return 1;
err:
	return -1;
}


/*
 * All BSD variants use the same magic string (little-endian),
 * and the same disklabel.
 *
 * The difference between {Free,Open,...}BSD is in the (parental)
 * primary partition type.
 *
 * See also: http://en.wikipedia.org/wiki/BSD_disklabel
 *
 * The location of BSD disk label is architecture specific and in defined by
 * LABELSECTOR and LABELOFFSET macros in the disklabel.h file. The location
 * also depends on BSD variant, FreeBSD uses only one location, NetBSD and
 * OpenBSD are more creative...
 *
 * The basic overview:
 *
 * arch                    | LABELSECTOR | LABELOFFSET
 * ------------------------+-------------+------------
 * amd64 arm hppa hppa64   |             |
 * i386, macppc, mvmeppc   |      1      |      0
 * sgi, aviion, sh, socppc |             |
 * ------------------------+-------------+------------
 * alpha luna88k mac68k    |      0      |     64
 * sparc(OpenBSD) vax      |             |
 * ------------------------+-------------+------------
 * spark64 sparc(NetBSD)   |      0      |    128
 * ------------------------+-------------+------------
 *
 * ...and more (see http://fxr.watson.org/fxr/ident?v=NETBSD;i=LABELSECTOR)
 *
 */
const struct blkid_idinfo bsd_pt_idinfo =
{
	.name		= "bsd",
	.probefunc	= probe_bsd_pt,
	.magics		=
	{
		{ .magic = "\x57\x45\x56\x82", .len = 4, .sboff = 512 },
		{ .magic = "\x57\x45\x56\x82", .len = 4, .sboff = 64 },
		{ .magic = "\x57\x45\x56\x82", .len = 4, .sboff = 128 },
		{ NULL }
	}
};

