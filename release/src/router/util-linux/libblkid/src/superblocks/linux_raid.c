/*
 * Copyright (C) 2008 Karel Zak <kzak@redhat.com>
 *
 * Inspired by libvolume_id by
 *     Kay Sievers <kay.sievers@vrfy.org>
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

struct mdp0_super_block {
	uint32_t	md_magic;
	uint32_t	major_version;
	uint32_t	minor_version;
	uint32_t	patch_version;
	uint32_t	gvalid_words;
	uint32_t	set_uuid0;
	uint32_t	ctime;
	uint32_t	level;
	uint32_t	size;
	uint32_t	nr_disks;
	uint32_t	raid_disks;
	uint32_t	md_minor;
	uint32_t	not_persistent;
	uint32_t	set_uuid1;
	uint32_t	set_uuid2;
	uint32_t	set_uuid3;
};

/*
 * Version-1, little-endian.
 */
struct mdp1_super_block {
	/* constant array information - 128 bytes */
	uint32_t	magic;		/* MD_SB_MAGIC: 0xa92b4efc - little endian */
	uint32_t	major_version;	/* 1 */
	uint32_t	feature_map;	/* 0 for now */
	uint32_t	pad0;		/* always set to 0 when writing */

	uint8_t		set_uuid[16];	/* user-space generated. */
	unsigned char	set_name[32];	/* set and interpreted by user-space */

	uint64_t	ctime;		/* lo 40 bits are seconds, top 24 are microseconds or 0*/
	uint32_t	level;		/* -4 (multipath), -1 (linear), 0,1,4,5 */
	uint32_t	layout;		/* only for raid5 currently */
	uint64_t	size;		/* used size of component devices, in 512byte sectors */

	uint32_t	chunksize;	/* in 512byte sectors */
	uint32_t	raid_disks;
	uint32_t	bitmap_offset;	/* sectors after start of superblock that bitmap starts
					 * NOTE: signed, so bitmap can be before superblock
					 * only meaningful of feature_map[0] is set.
					 */

	/* These are only valid with feature bit '4' */
	uint32_t	new_level;	/* new level we are reshaping to		*/
	uint64_t	reshape_position;	/* next address in array-space for reshape */
	uint32_t	delta_disks;	/* change in number of raid_disks		*/
	uint32_t	new_layout;	/* new layout					*/
	uint32_t	new_chunk;	/* new chunk size (bytes)			*/
	uint8_t		pad1[128-124];	/* set to 0 when written */

	/* constant this-device information - 64 bytes */
	uint64_t	data_offset;	/* sector start of data, often 0 */
	uint64_t	data_size;	/* sectors in this device that can be used for data */
	uint64_t	super_offset;	/* sector start of this superblock */
	uint64_t	recovery_offset;/* sectors before this offset (from data_offset) have been recovered */
	uint32_t	dev_number;	/* permanent identifier of this  device - not role in raid */
	uint32_t	cnt_corrected_read; /* number of read errors that were corrected by re-writing */
	uint8_t		device_uuid[16]; /* user-space setable, ignored by kernel */
        uint8_t		devflags;        /* per-device flags.  Only one defined...*/
	uint8_t		pad2[64-57];	/* set to 0 when writing */

	/* array state information - 64 bytes */
	uint64_t	utime;		/* 40 bits second, 24 btes microseconds */
	uint64_t	events;		/* incremented when superblock updated */
	uint64_t	resync_offset;	/* data before this offset (from data_offset) known to be in sync */
	uint32_t	sb_csum;	/* checksum upto dev_roles[max_dev] */
	uint32_t	max_dev;	/* size of dev_roles[] array to consider */
	uint8_t		pad3[64-32];	/* set to 0 when writing */

	/* device state information. Indexed by dev_number.
	 * 2 bytes per device
	 * Note there are no per-device state flags. State information is rolled
	 * into the 'roles' value.  If a device is spare or faulty, then it doesn't
	 * have a meaningful role.
	 */
	uint16_t	dev_roles[0];	/* role in array, or 0xffff for a spare, or 0xfffe for faulty */
};


#define MD_RESERVED_BYTES		0x10000
#define MD_SB_MAGIC			0xa92b4efc

static int probe_raid0(blkid_probe pr, blkid_loff_t off)
{
	struct mdp0_super_block *mdp0;
	union {
		uint32_t ints[4];
		uint8_t bytes[16];
	} uuid;
	uint32_t ma, mi, pa;
	uint64_t size;

	if (pr->size < MD_RESERVED_BYTES)
		return -1;
	mdp0 = (struct mdp0_super_block *)
			blkid_probe_get_buffer(pr,
				off,
				sizeof(struct mdp0_super_block));
	if (!mdp0)
		return -1;

	memset(uuid.ints, 0, sizeof(uuid.ints));

	if (le32_to_cpu(mdp0->md_magic) == MD_SB_MAGIC) {
		uuid.ints[0] = swab32(mdp0->set_uuid0);
		if (le32_to_cpu(mdp0->minor_version) >= 90) {
			uuid.ints[1] = swab32(mdp0->set_uuid1);
			uuid.ints[2] = swab32(mdp0->set_uuid2);
			uuid.ints[3] = swab32(mdp0->set_uuid3);
		}
		ma = le32_to_cpu(mdp0->major_version);
		mi = le32_to_cpu(mdp0->minor_version);
		pa = le32_to_cpu(mdp0->patch_version);
		size = le32_to_cpu(mdp0->size);

	} else if (be32_to_cpu(mdp0->md_magic) == MD_SB_MAGIC) {
		uuid.ints[0] = mdp0->set_uuid0;
		if (be32_to_cpu(mdp0->minor_version) >= 90) {
			uuid.ints[1] = mdp0->set_uuid1;
			uuid.ints[2] = mdp0->set_uuid2;
			uuid.ints[3] = mdp0->set_uuid3;
		}
		ma = be32_to_cpu(mdp0->major_version);
		mi = be32_to_cpu(mdp0->minor_version);
		pa = be32_to_cpu(mdp0->patch_version);
		size = be32_to_cpu(mdp0->size);
	} else
		return 1;

	size <<= 10;	/* convert KiB to bytes */

	if (pr->size < 0 || (uint64_t) pr->size < size + MD_RESERVED_BYTES)
		/* device is too small */
		return 1;

	if (off < 0 || (uint64_t) off < size)
		/* no space before superblock */
		return 1;

	/*
	 * Check for collisions between RAID and partition table
	 *
	 * For example the superblock is at the end of the last partition, it's
	 * the same possition as at the end of the disk...
	 */
	if ((S_ISREG(pr->mode) || blkid_probe_is_wholedisk(pr)) &&
	    blkid_probe_is_covered_by_pt(pr,
			off - size,				/* min. start  */
			size + MD_RESERVED_BYTES)) {		/* min. length */

		/* ignore this superblock, it's within any partition and
		 * we are working with whole-disk now */
		return 1;
	}

	if (blkid_probe_sprintf_version(pr, "%u.%u.%u", ma, mi, pa) != 0)
		return -1;
	if (blkid_probe_set_uuid(pr, (unsigned char *) uuid.bytes) != 0)
		return -1;
	if (blkid_probe_set_magic(pr, off, sizeof(mdp0->md_magic),
				(unsigned char *) &mdp0->md_magic))
		return -1;
	return 0;
}

static int probe_raid1(blkid_probe pr, off_t off)
{
	struct mdp1_super_block *mdp1;

	mdp1 = (struct mdp1_super_block *)
			blkid_probe_get_buffer(pr,
				off,
				sizeof(struct mdp1_super_block));
	if (!mdp1)
		return -1;
	if (le32_to_cpu(mdp1->magic) != MD_SB_MAGIC)
		return -1;
	if (le32_to_cpu(mdp1->major_version) != 1U)
		return -1;
	if (le64_to_cpu(mdp1->super_offset) != (uint64_t) off >> 9)
		return -1;
	if (blkid_probe_set_uuid(pr, (unsigned char *) mdp1->set_uuid) != 0)
		return -1;
	if (blkid_probe_set_uuid_as(pr,
			(unsigned char *) mdp1->device_uuid, "UUID_SUB") != 0)
		return -1;
	if (blkid_probe_set_label(pr, mdp1->set_name,
				sizeof(mdp1->set_name)) != 0)
		return -1;
	if (blkid_probe_set_magic(pr, off, sizeof(mdp1->magic),
				(unsigned char *) &mdp1->magic))
		return -1;
	return 0;
}

int probe_raid(blkid_probe pr,
		const struct blkid_idmag *mag __attribute__((__unused__)))
{
	const char *ver = NULL;

	if (pr->size > MD_RESERVED_BYTES) {
		/* version 0 at the end of the device */
		uint64_t sboff = (pr->size & ~(MD_RESERVED_BYTES - 1))
			         - MD_RESERVED_BYTES;
		if (probe_raid0(pr, sboff) == 0)
			return 0;

		/* version 1.0 at the end of the device */
		sboff = (pr->size & ~(0x1000 - 1)) - 0x2000;
		if (probe_raid1(pr, sboff) == 0)
			ver = "1.0";
	}

	if (!ver) {
		/* version 1.1 at the start of the device */
		if (probe_raid1(pr, 0) == 0)
			ver = "1.1";

		/* version 1.2 at 4k offset from the start */
		else if (probe_raid1(pr, 0x1000) == 0)
			ver = "1.2";
	}

	if (ver) {
		blkid_probe_set_version(pr, ver);
		return 0;
	}
	return -1;
}


const struct blkid_idinfo linuxraid_idinfo = {
	.name		= "linux_raid_member",
	.usage		= BLKID_USAGE_RAID,
	.probefunc	= probe_raid,
	.magics		= BLKID_NONE_MAGIC
};


