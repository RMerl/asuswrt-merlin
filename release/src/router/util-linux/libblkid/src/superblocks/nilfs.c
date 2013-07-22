/*
 * Copyright (C) 2010 by Jiro SEKIBA <jir@unicus.jp>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License
 */
#include <stddef.h>
#include <string.h>

#include "superblocks.h"
#include "crc32.h"

struct nilfs_super_block {
	uint32_t	s_rev_level;
	uint16_t	s_minor_rev_level;
	uint16_t	s_magic;

	uint16_t	s_bytes;

	uint16_t	s_flags;
	uint32_t	s_crc_seed;
	uint32_t	s_sum;

	uint32_t	s_log_block_size;

	uint64_t	s_nsegments;
	uint64_t	s_dev_size;
	uint64_t	s_first_data_block;
	uint32_t	s_blocks_per_segment;
	uint32_t	s_r_segments_percentage;

	uint64_t	s_last_cno;
	uint64_t	s_last_pseg;
	uint64_t	s_last_seq;
	uint64_t	s_free_blocks_count;

	uint64_t	s_ctime;

	uint64_t	s_mtime;
	uint64_t	s_wtime;
	uint16_t	s_mnt_count;
	uint16_t	s_max_mnt_count;
	uint16_t	s_state;
	uint16_t	s_errors;
	uint64_t	s_lastcheck;

	uint32_t	s_checkinterval;
	uint32_t	s_creator_os;
	uint16_t	s_def_resuid;
	uint16_t	s_def_resgid;
	uint32_t	s_first_ino;

	uint16_t	s_inode_size;
	uint16_t	s_dat_entry_size;
	uint16_t	s_checkpoint_size;
	uint16_t	s_segment_usage_size;

	uint8_t		s_uuid[16];
	char		s_volume_name[80];

	uint32_t	s_c_interval;
	uint32_t	s_c_block_max;
	uint32_t	s_reserved[192];
};

/* nilfs2 magic string */
#define NILFS_SB_MAGIC		"\x34\x34"
/* nilfs2 super block offset */
#define NILFS_SB_OFF		0x400
/* nilfs2 super block offset in kB */
#define NILFS_SB_KBOFF		(NILFS_SB_OFF >> 10)
/* nilfs2 magic string offset within super block */
#define NILFS_MAG_OFF		6

static int probe_nilfs2(blkid_probe pr, const struct blkid_idmag *mag)
{
	struct nilfs_super_block *sb;
	static unsigned char sum[4];
	const int sumoff = offsetof(struct nilfs_super_block, s_sum);
	size_t bytes;
	uint32_t crc;

	sb = blkid_probe_get_sb(pr, mag, struct nilfs_super_block);
	if (!sb)
		return -1;

	bytes = le16_to_cpu(sb->s_bytes);
	crc = crc32(le32_to_cpu(sb->s_crc_seed), (unsigned char *)sb, sumoff);
	crc = crc32(crc, sum, 4);
	crc = crc32(crc, (unsigned char *)sb + sumoff + 4, bytes - sumoff - 4);

	if (crc != le32_to_cpu(sb->s_sum))
		return -1;

	if (strlen(sb->s_volume_name))
		blkid_probe_set_label(pr, (unsigned char *) sb->s_volume_name,
				      sizeof(sb->s_volume_name));

	blkid_probe_set_uuid(pr, sb->s_uuid);
	blkid_probe_sprintf_version(pr, "%u", le32_to_cpu(sb->s_rev_level));

	return 0;
}

const struct blkid_idinfo nilfs2_idinfo =
{
	.name		= "nilfs2",
	.usage		= BLKID_USAGE_FILESYSTEM,
	.probefunc	= probe_nilfs2,
	.magics		=
	{
		{
			.magic = NILFS_SB_MAGIC,
			.len = 2,
			.kboff = NILFS_SB_KBOFF,
			.sboff = NILFS_MAG_OFF
		},
		{ NULL }
	}
};
