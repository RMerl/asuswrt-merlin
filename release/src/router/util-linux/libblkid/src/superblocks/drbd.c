/*
 * Copyright (C) 2009 by Bastian Friedrich <bastian.friedrich@collax.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 *
 * defines, structs taken from drbd source; file names represent drbd source
 * files.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <inttypes.h>
#include <stddef.h>

#include "superblocks.h"

/*
 * drbd/linux/drbd.h
 */
#define DRBD_MAGIC 0x83740267

/*
 * user/drbdmeta.c
 * We only support v08 for now
 */
#define DRBD_MD_MAGIC_08         (DRBD_MAGIC+4)
#define DRBD_MD_MAGIC_84_UNCLEAN (DRBD_MAGIC+5)

/*
 * drbd/linux/drbd.h
 */
enum drbd_uuid_index {
	UI_CURRENT,
	UI_BITMAP,
	UI_HISTORY_START,
	UI_HISTORY_END,
	UI_SIZE,		/* nl-packet: number of dirty bits */
	UI_FLAGS,		/* nl-packet: flags */
	UI_EXTENDED_SIZE	/* Everything. */
};

/*
 * user/drbdmeta.c
 * Minor modifications wrt. types
 */
struct md_on_disk_08 {
	uint64_t la_sect;         /* last agreed size. */
	uint64_t uuid[UI_SIZE];   /* UUIDs */
	uint64_t device_uuid;
	uint64_t reserved_u64_1;
	uint32_t flags;
	uint32_t magic;
	uint32_t md_size_sect;
	int32_t  al_offset;       /* signed sector offset to this block */
	uint32_t al_nr_extents;   /* important for restoring the AL */
	int32_t  bm_offset;       /* signed sector offset to the bitmap, from here */
	uint32_t bm_bytes_per_bit;
	uint32_t reserved_u32[4];

	char reserved[8 * 512 - (8*(UI_SIZE+3)+4*11)];
};


static int probe_drbd(blkid_probe pr,
		const struct blkid_idmag *mag __attribute__((__unused__)))
{
	struct md_on_disk_08 *md;
	off_t off;

	off = pr->size - sizeof(*md);

	/* Small devices cannot be drbd (?) */
	if (pr->size < 0x10000)
		return -1;

	md = (struct md_on_disk_08 *)
			blkid_probe_get_buffer(pr,
					off,
					sizeof(struct md_on_disk_08));
	if (!md)
		return -1;

	if (be32_to_cpu(md->magic) != DRBD_MD_MAGIC_08 &&
			be32_to_cpu(md->magic) != DRBD_MD_MAGIC_84_UNCLEAN)
		return -1;

	/*
	 * DRBD does not have "real" uuids; the following resembles DRBD's
	 * notion of uuids (64 bit, see struct above)
	 */
	blkid_probe_sprintf_uuid(pr,
		(unsigned char *) &md->device_uuid, sizeof(md->device_uuid),
		"%" PRIx64, be64_to_cpu(md->device_uuid));

	blkid_probe_set_version(pr, "v08");

	if (blkid_probe_set_magic(pr,
				off + offsetof(struct md_on_disk_08, magic),
				sizeof(md->magic),
				(unsigned char *) &md->magic))
		return -1;

	return 0;
}

const struct blkid_idinfo drbd_idinfo =
{
	.name		= "drbd",
	.usage		= BLKID_USAGE_RAID,
	.probefunc	= probe_drbd,
	.magics		= BLKID_NONE_MAGIC
};

