/*
 * check_desc.c --- Check the group descriptors of an ext2 filesystem
 *
 * Copyright (C) 1993, 1994, 1995, 1996 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Library
 * General Public License, version 2.
 * %End-Header%
 */

#include "config.h"
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
	ext2fs_block_bitmap bmap;
	errcode_t retval;
	dgrp_t i;
	blk64_t first_block = fs->super->s_first_data_block;
	blk64_t last_block = ext2fs_blocks_count(fs->super)-1;
	blk64_t blk, b;
	unsigned int j;

	EXT2_CHECK_MAGIC(fs, EXT2_ET_MAGIC_EXT2FS_FILSYS);

	retval = ext2fs_allocate_subcluster_bitmap(fs, "check_desc map", &bmap);
	if (retval)
		return retval;

	for (i = 0; i < fs->group_desc_count; i++)
		ext2fs_reserve_super_and_bgd(fs, i, bmap);

	for (i = 0; i < fs->group_desc_count; i++) {
		if (!EXT2_HAS_INCOMPAT_FEATURE(fs->super,
					       EXT4_FEATURE_INCOMPAT_FLEX_BG)) {
			first_block = ext2fs_group_first_block2(fs, i);
			last_block = ext2fs_group_last_block2(fs, i);
		}

		/*
		 * Check to make sure the block bitmap for group is sane
		 */
		blk = ext2fs_block_bitmap_loc(fs, i);
		if (blk < first_block || blk > last_block ||
		    ext2fs_test_block_bitmap2(bmap, blk)) {
			retval = EXT2_ET_GDESC_BAD_BLOCK_MAP;
			goto errout;
		}
		ext2fs_mark_block_bitmap2(bmap, blk);

		/*
		 * Check to make sure the inode bitmap for group is sane
		 */
		blk = ext2fs_inode_bitmap_loc(fs, i);
		if (blk < first_block || blk > last_block ||
		    ext2fs_test_block_bitmap2(bmap, blk)) {
			retval = EXT2_ET_GDESC_BAD_INODE_MAP;
			goto errout;
		}
		ext2fs_mark_block_bitmap2(bmap, blk);

		/*
		 * Check to make sure the inode table for group is sane
		 */
		blk = ext2fs_inode_table_loc(fs, i);
		if (blk < first_block ||
		    ((blk + fs->inode_blocks_per_group - 1) > last_block)) {
			retval = EXT2_ET_GDESC_BAD_INODE_TABLE;
			goto errout;
		}
		for (j = 0, b = blk; j < fs->inode_blocks_per_group;
		     j++, b++) {
			if (ext2fs_test_block_bitmap2(bmap, b)) {
				retval = EXT2_ET_GDESC_BAD_INODE_TABLE;
				goto errout;
			}
			ext2fs_mark_block_bitmap2(bmap, b);
		}
	}
errout:
	ext2fs_free_block_bitmap(bmap);
	return retval;
}
