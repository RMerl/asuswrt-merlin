/*
 * alloc_tables.c --- Allocate tables for a newly initialized
 * filesystem.  Used by mke2fs when initializing a filesystem
 *
 * Copyright (C) 1996 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Library
 * General Public License, version 2.
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
 * This routine searches for free blocks that can allocate a full
 * group of bitmaps or inode tables for a flexbg group.  Returns the
 * block number with a correct offset were the bitmaps and inode
 * tables can be allocated continously and in order.
 */
static blk_t flexbg_offset(ext2_filsys fs, dgrp_t group, blk_t start_blk,
			   ext2fs_block_bitmap bmap, int offset, int size,
			   int elem_size)
{
	int		flexbg, flexbg_size;
	blk_t		last_blk, first_free = 0;
	dgrp_t	       	last_grp;

	flexbg_size = 1 << fs->super->s_log_groups_per_flex;
	flexbg = group / flexbg_size;

	if (size > (int) (fs->super->s_blocks_per_group / 8))
		size = (int) fs->super->s_blocks_per_group / 8;

	if (offset)
		offset -= 1;

	/*
	 * Don't do a long search if the previous block
	 * search is still valid.
	 */
	if (start_blk && group % flexbg_size) {
		if (ext2fs_test_block_bitmap_range(bmap, start_blk + elem_size,
						   size))
			return start_blk + elem_size;
	}

	start_blk = ext2fs_group_first_block(fs, flexbg_size * flexbg);
	last_grp = group | (flexbg_size - 1);
	if (last_grp > fs->group_desc_count)
		last_grp = fs->group_desc_count;
	last_blk = ext2fs_group_last_block(fs, last_grp);

	/* Find the first available block */
	if (ext2fs_get_free_blocks(fs, start_blk, last_blk, 1, bmap,
				   &first_free))
		return first_free;

	if (ext2fs_get_free_blocks(fs, first_free + offset, last_blk, size,
				   bmap, &first_free))
		return first_free;

	return first_free;
}

errcode_t ext2fs_allocate_group_table(ext2_filsys fs, dgrp_t group,
				      ext2fs_block_bitmap bmap)
{
	errcode_t	retval;
	blk_t		group_blk, start_blk, last_blk, new_blk, blk;
	dgrp_t		last_grp = 0;
	int		j, rem_grps = 0, flexbg_size = 0;

	group_blk = ext2fs_group_first_block(fs, group);
	last_blk = ext2fs_group_last_block(fs, group);

	if (!bmap)
		bmap = fs->block_map;

	if (EXT2_HAS_INCOMPAT_FEATURE(fs->super,
				      EXT4_FEATURE_INCOMPAT_FLEX_BG) &&
	    fs->super->s_log_groups_per_flex) {
		flexbg_size = 1 << fs->super->s_log_groups_per_flex;
		last_grp = group | (flexbg_size - 1);
		rem_grps = last_grp - group;
		if (last_grp > fs->group_desc_count)
			last_grp = fs->group_desc_count;
	}

	/*
	 * Allocate the block and inode bitmaps, if necessary
	 */
	if (fs->stride) {
		retval = ext2fs_get_free_blocks(fs, group_blk, last_blk,
						1, bmap, &start_blk);
		if (retval)
			return retval;
		start_blk += fs->inode_blocks_per_group;
		start_blk += ((fs->stride * group) %
			      (last_blk - start_blk + 1));
		if (start_blk >= last_blk)
			start_blk = group_blk;
	} else
		start_blk = group_blk;

	if (flexbg_size) {
		blk_t prev_block = 0;
		if (group && fs->group_desc[group-1].bg_block_bitmap)
			prev_block = fs->group_desc[group-1].bg_block_bitmap;
		start_blk = flexbg_offset(fs, group, prev_block, bmap,
						 0, rem_grps, 1);
		last_blk = ext2fs_group_last_block(fs, last_grp);
	}

	if (!fs->group_desc[group].bg_block_bitmap) {
		retval = ext2fs_get_free_blocks(fs, start_blk, last_blk,
						1, bmap, &new_blk);
		if (retval == EXT2_ET_BLOCK_ALLOC_FAIL)
			retval = ext2fs_get_free_blocks(fs, group_blk,
					last_blk, 1, bmap, &new_blk);
		if (retval)
			return retval;
		ext2fs_mark_block_bitmap(bmap, new_blk);
		fs->group_desc[group].bg_block_bitmap = new_blk;
		if (flexbg_size) {
			dgrp_t gr = ext2fs_group_of_blk(fs, new_blk);
			fs->group_desc[gr].bg_free_blocks_count--;
			fs->super->s_free_blocks_count--;
			fs->group_desc[gr].bg_flags &= ~EXT2_BG_BLOCK_UNINIT;
			ext2fs_group_desc_csum_set(fs, gr);
		}
	}

	if (flexbg_size) {
		blk_t prev_block = 0;
		if (group && fs->group_desc[group-1].bg_inode_bitmap)
			prev_block = fs->group_desc[group-1].bg_inode_bitmap;
		start_blk = flexbg_offset(fs, group, prev_block, bmap,
						 flexbg_size, rem_grps, 1);
		last_blk = ext2fs_group_last_block(fs, last_grp);
	}

	if (!fs->group_desc[group].bg_inode_bitmap) {
		retval = ext2fs_get_free_blocks(fs, start_blk, last_blk,
						1, bmap, &new_blk);
		if (retval == EXT2_ET_BLOCK_ALLOC_FAIL)
			retval = ext2fs_get_free_blocks(fs, group_blk,
					last_blk, 1, bmap, &new_blk);
		if (retval)
			return retval;
		ext2fs_mark_block_bitmap(bmap, new_blk);
		fs->group_desc[group].bg_inode_bitmap = new_blk;
		if (flexbg_size) {
			dgrp_t gr = ext2fs_group_of_blk(fs, new_blk);
			fs->group_desc[gr].bg_free_blocks_count--;
			fs->super->s_free_blocks_count--;
			fs->group_desc[gr].bg_flags &= ~EXT2_BG_BLOCK_UNINIT;
			ext2fs_group_desc_csum_set(fs, gr);
		}
	}

	/*
	 * Allocate the inode table
	 */
	if (flexbg_size) {
		blk_t prev_block = 0;
		if (group && fs->group_desc[group-1].bg_inode_table)
			prev_block = fs->group_desc[group-1].bg_inode_table;
		if (last_grp == fs->group_desc_count)
			rem_grps = last_grp - group;
		group_blk = flexbg_offset(fs, group, prev_block, bmap,
						 flexbg_size * 2,
						 fs->inode_blocks_per_group *
						 rem_grps,
						 fs->inode_blocks_per_group);
		last_blk = ext2fs_group_last_block(fs, last_grp);
	}

	if (!fs->group_desc[group].bg_inode_table) {
		retval = ext2fs_get_free_blocks(fs, group_blk, last_blk,
						fs->inode_blocks_per_group,
						bmap, &new_blk);
		if (retval)
			return retval;
		for (j=0, blk = new_blk;
		     j < fs->inode_blocks_per_group;
		     j++, blk++) {
			ext2fs_mark_block_bitmap(bmap, blk);
			if (flexbg_size) {
				dgrp_t gr = ext2fs_group_of_blk(fs, blk);
				fs->group_desc[gr].bg_free_blocks_count--;
				fs->super->s_free_blocks_count--;
				fs->group_desc[gr].bg_flags &= ~EXT2_BG_BLOCK_UNINIT;
				ext2fs_group_desc_csum_set(fs, gr);
			}
		}
		fs->group_desc[group].bg_inode_table = new_blk;
	}
	ext2fs_group_desc_csum_set(fs, group);
	return 0;
}

errcode_t ext2fs_allocate_tables(ext2_filsys fs)
{
	errcode_t	retval;
	dgrp_t		i;

	for (i = 0; i < fs->group_desc_count; i++) {
		retval = ext2fs_allocate_group_table(fs, i, fs->block_map);
		if (retval)
			return retval;
	}
	return 0;
}

