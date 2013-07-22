/*
 * Copyright (C) 2008 Karel Zak <kzak@redhat.com>
 *
 * This is written from sratch according to Linux kernel fs/sysv/super.c file.
 * It seems that sysv probing code in libvolume_id and also in the original
 * blkid is useless.
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <stddef.h>

#include "superblocks.h"

#define XENIX_NICINOD				100
#define XENIX_NICFREE				100

struct xenix_super_block {
	uint16_t	s_isize;
	uint32_t	s_fsize;
	uint16_t	s_nfree;
	uint32_t	s_free[XENIX_NICFREE];
	uint16_t	s_ninode;
	uint16_t	s_inode[XENIX_NICINOD];
	uint8_t		s_flock;
	uint8_t		s_ilock;
	uint8_t		s_fmod;
	uint8_t		s_ronly;
	uint32_t	s_time;
	uint32_t	s_tfree;
	uint16_t	s_tinode;
	uint16_t	s_dinfo[4];
	uint8_t		s_fname[6];
	uint8_t		s_fpack[6];
	uint8_t		s_clean;
	uint8_t		s_fill[371];
	uint32_t	s_magic;
	uint32_t	s_type;
} __attribute__((packed));


#define SYSV_NICINOD			100
#define SYSV_NICFREE			50

struct sysv_super_block
{
	uint16_t	s_isize;
	uint16_t	s_pad0;
	uint32_t	s_fsize;
	uint16_t	s_nfree;
	uint16_t	s_pad1;
	uint32_t	s_free[SYSV_NICFREE];
	uint16_t	s_ninode;
	uint16_t	s_pad2;
	uint16_t	s_inode[SYSV_NICINOD];
	uint8_t		s_flock;
	uint8_t		s_ilock;
	uint8_t		s_fmod;
	uint8_t		s_ronly;
	uint32_t	s_time;
	uint16_t	s_dinfo[4];
	uint32_t	s_tfree;
	uint16_t	s_tinode;
	uint16_t	s_pad3;
	uint8_t		s_fname[6];
	uint8_t		s_fpack[6];
	uint32_t	s_fill[12];
	uint32_t	s_state;
	uint32_t	s_magic;
	uint32_t	s_type;
};

static int probe_xenix(blkid_probe pr, const struct blkid_idmag *mag)
{
	struct xenix_super_block *sb;

	sb = blkid_probe_get_sb(pr, mag, struct xenix_super_block);
	if (!sb)
		return -1;
	blkid_probe_set_label(pr, sb->s_fname, sizeof(sb->s_fname));
	return 0;
}

#define SYSV_BLOCK_SIZE	1024

/* Note that we don't probe for Coherent FS, this FS does not have
 * magic string. (It requires to probe fname/fpack field..)
 */
static int probe_sysv(blkid_probe pr,
		const struct blkid_idmag *mag __attribute__((__unused__)))
{
	struct sysv_super_block *sb;
	int blocks[] = {0, 9, 15, 18};
	size_t i;

	for (i = 0; i < ARRAY_SIZE(blocks); i++) {
		int off = blocks[i] * SYSV_BLOCK_SIZE + SYSV_BLOCK_SIZE/2;

		sb = (struct sysv_super_block *)
			blkid_probe_get_buffer(pr,
					off,
					sizeof(struct sysv_super_block));
		if (!sb)
			return -1;

		if (sb->s_magic == cpu_to_le32(0xfd187e20) ||
		    sb->s_magic == cpu_to_be32(0xfd187e20)) {

			if (blkid_probe_set_label(pr, sb->s_fname,
						sizeof(sb->s_fname)))
				return -1;

			if (blkid_probe_set_magic(pr,
					off + offsetof(struct sysv_super_block,
								s_magic),
					sizeof(sb->s_magic),
					(unsigned char *) &sb->s_magic))
				return -1;

			return 0;
		}
	}
	return 1;
}

const struct blkid_idinfo xenix_idinfo =
{
	.name		= "xenix",
	.usage		= BLKID_USAGE_FILESYSTEM,
	.probefunc	= probe_xenix,
	.magics		=
	{
		{ .magic = "\x2b\x55\x44", .len = 3, .kboff = 1, .sboff = 0x400 },
		{ .magic = "\x44\x55\x2b", .len = 3, .kboff = 1, .sboff = 0x400 },
		{ NULL }
	}
};

const struct blkid_idinfo sysv_idinfo =
{
	.name		= "sysv",
	.usage		= BLKID_USAGE_FILESYSTEM,
	.probefunc	= probe_sysv,

	/* SYSV is BE and LE and superblock could be on four positions. It's
	 * simpler to probe for the magic string by .probefunc().
	 */
	.magics		= BLKID_NONE_MAGIC
};

