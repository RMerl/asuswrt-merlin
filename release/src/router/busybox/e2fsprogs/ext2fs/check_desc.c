/* vi: set sw=4 ts=4: */
/*
 * check_desc.c --- Check the group descriptors of an ext2 filesystem
 *
 * Copyright (C) 1993, 1994, 1995, 1996 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include <stdio.h>
#include <string.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <fcntl.h>
#include <time.h>
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#include "ext2_fs.h"
#include "ext2fs.h"

/*
 * This routine sanity checks the group descriptors
 */
errcode_t ext2fs_check_desc(ext2_filsys fs)
{
	dgrp_t i;
	blk_t block = fs->super->s_first_data_block;
	blk_t next;

	EXT2_CHECK_MAGIC(fs, EXT2_ET_MAGIC_EXT2FS_FILSYS);

	for (i = 0; i < fs->group_desc_count; i++) {
		next = block + fs->super->s_blocks_per_group;
		/*
		 * Check to make sure block bitmap for group is
		 * located within the group.
		 */
		if (fs->group_desc[i].bg_block_bitmap < block ||
		    fs->group_desc[i].bg_block_bitmap >= next)
			return EXT2_ET_GDESC_BAD_BLOCK_MAP;
		/*
		 * Check to make sure inode bitmap for group is
		 * located within the group
		 */
		if (fs->group_desc[i].bg_inode_bitmap < block ||
		    fs->group_desc[i].bg_inode_bitmap >= next)
			return EXT2_ET_GDESC_BAD_INODE_MAP;
		/*
		 * Check to make sure inode table for group is located
		 * within the group
		 */
		if (fs->group_desc[i].bg_inode_table < block ||
		    ((fs->group_desc[i].bg_inode_table +
		      fs->inode_blocks_per_group) >= next))
			return EXT2_ET_GDESC_BAD_INODE_TABLE;

		block = next;
	}
	return 0;
}
