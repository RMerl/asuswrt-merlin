/*
 * alloc.c --- allocate new inodes, blocks for ext2fs
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
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <time.h>
#include <string.h>
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#include "ext2_fs.h"
#include "ext2fs.h"

/*
 * Check for uninit block bitmaps and deal with them appropriately
 */
static void check_block_uninit(ext2_filsys fs, ext2fs_block_bitmap map,
			       dgrp_t group)
{
	blk_t		i;
	blk64_t		blk, super_blk, old_desc_blk, new_desc_blk;
	int		old_desc_blocks;

	if (!(EXT2_HAS_RO_COMPAT_FEATURE(fs->super,
					 EXT4_FEATURE_RO_COMPAT_GDT_CSUM)) ||
	    !(ext2fs_bg_flags_test(fs, group, EXT2_BG_BLOCK_UNINIT)))
		return;

	blk = ext2fs_group_first_block2(fs, group);

	ext2fs_super_and_bgd_loc2(fs, group, &super_blk,
				  &old_desc_blk, &new_desc_blk, 0);

	if (fs->super->s_feature_incompat &
	    EXT2_FEATURE_INCOMPAT_META_BG)
		old_desc_blocks = fs->super->s_first_meta_bg;
	else
		old_desc_blocks = fs->desc_blocks + fs->super->s_reserved_gdt_blocks;

	for (i=0; i < fs->super->s_blocks_per_group; i++, blk++)
		ext2fs_fast_unmark_block_bitmap2(map, blk);

	blk = ext2fs_group_first_block2(fs, group);
	for (i=0; i < fs->super->s_blocks_per_group; i++, blk++) {
		if ((blk == super_blk) ||
		    (old_desc_blk && old_desc_blocks &&
		     (blk >= old_desc_blk) &&
		     (blk < old_desc_blk + old_desc_blocks)) ||
		    (new_desc_blk && (blk == new_desc_blk)) ||
		    (blk == ext2fs_block_bitmap_loc(fs, group)) ||
		    (blk == ext2fs_inode_bitmap_loc(fs, group)) ||
		    (blk >= ext2fs_inode_table_loc(fs, group) &&
		     (blk < ext2fs_inode_table_loc(fs, group)
		      + fs->inode_blocks_per_group)))
			ext2fs_fast_mark_block_bitmap2(map, blk);
	}
	ext2fs_bg_flags_clear(fs, group, EXT2_BG_BLOCK_UNINIT);
	ext2fs_group_desc_csum_set(fs, group);
	ext2fs_mark_super_dirty(fs);
	ext2fs_mark_bb_dirty(fs);
}

/*
 * Check for uninit inode bitmaps and deal with them appropriately
 */
static void check_inode_uninit(ext2_filsys fs, ext2fs_inode_bitmap map,
			  dgrp_t group)
{
	ext2_ino_t	i, ino;

	if (!(EXT2_HAS_RO_COMPAT_FEATURE(fs->super,
					 EXT4_FEATURE_RO_COMPAT_GDT_CSUM)) ||
	    !(ext2fs_bg_flags_test(fs, group, EXT2_BG_INODE_UNINIT)))
		return;

	ino = (group * fs->super->s_inodes_per_group) + 1;
	for (i=0; i < fs->super->s_inodes_per_group; i++, ino++)
		ext2fs_fast_unmark_inode_bitmap2(map, ino);

	ext2fs_bg_flags_clear(fs, group, EXT2_BG_INODE_UNINIT);
	ext2fs_group_desc_csum_set(fs, group);
	ext2fs_mark_ib_dirty(fs);
	ext2fs_mark_super_dirty(fs);
	check_block_uninit(fs, fs->block_map, group);
}

/*
 * Right now, just search forward from the parent directory's block
 * group to find the next free inode.
 *
 * Should have a special policy for directories.
 */
errcode_t ext2fs_new_inode(ext2_filsys fs, ext2_ino_t dir,
			   int mode EXT2FS_ATTR((unused)),
			   ext2fs_inode_bitmap map, ext2_ino_t *ret)
{
	ext2_ino_t	start_inode = 0;
	ext2_ino_t	i, ino_in_group, upto, first_zero;
	errcode_t	retval;
	dgrp_t		group;

	EXT2_CHECK_MAGIC(fs, EXT2_ET_MAGIC_EXT2FS_FILSYS);

	if (!map)
		map = fs->inode_map;
	if (!map)
		return EXT2_ET_NO_INODE_BITMAP;

	if (dir > 0) {
		group = (dir - 1) / EXT2_INODES_PER_GROUP(fs->super);
		start_inode = (group * EXT2_INODES_PER_GROUP(fs->super)) + 1;
	}
	if (start_inode < EXT2_FIRST_INODE(fs->super))
		start_inode = EXT2_FIRST_INODE(fs->super);
	if (start_inode > fs->super->s_inodes_count)
		return EXT2_ET_INODE_ALLOC_FAIL;
	i = start_inode;
	do {
		ino_in_group = (i - 1) % EXT2_INODES_PER_GROUP(fs->super);
		group = (i - 1) / EXT2_INODES_PER_GROUP(fs->super);

		check_inode_uninit(fs, map, group);
		upto = i + (EXT2_INODES_PER_GROUP(fs->super) - ino_in_group);
		if (i < start_inode && upto >= start_inode)
			upto = start_inode - 1;
		if (upto > fs->super->s_inodes_count)
			upto = fs->super->s_inodes_count;

		retval = ext2fs_find_first_zero_inode_bitmap2(map, i, upto,
							      &first_zero);
		if (retval == 0) {
			i = first_zero;
			break;
		}
		if (retval != ENOENT)
			return EXT2_ET_INODE_ALLOC_FAIL;
		i = upto + 1;
		if (i > fs->super->s_inodes_count)
			i = EXT2_FIRST_INODE(fs->super);
	} while (i != start_inode);

	if (ext2fs_test_inode_bitmap2(map, i))
		return EXT2_ET_INODE_ALLOC_FAIL;
	*ret = i;
	return 0;
}

/*
 * Stupid algorithm --- we now just search forward starting from the
 * goal.  Should put in a smarter one someday....
 */
errcode_t ext2fs_new_block2(ext2_filsys fs, blk64_t goal,
			   ext2fs_block_bitmap map, blk64_t *ret)
{
	blk64_t	i;
	int	c_ratio;

	EXT2_CHECK_MAGIC(fs, EXT2_ET_MAGIC_EXT2FS_FILSYS);

	if (!map)
		map = fs->block_map;
	if (!map)
		return EXT2_ET_NO_BLOCK_BITMAP;
	if (!goal || (goal >= ext2fs_blocks_count(fs->super)))
		goal = fs->super->s_first_data_block;
	i = goal;
	c_ratio = 1 << ext2fs_get_bitmap_granularity(map);
	if (c_ratio > 1)
		goal &= ~EXT2FS_CLUSTER_MASK(fs);
	check_block_uninit(fs, map,
			   (i - fs->super->s_first_data_block) /
			   EXT2_BLOCKS_PER_GROUP(fs->super));
	do {
		if (((i - fs->super->s_first_data_block) %
		     EXT2_BLOCKS_PER_GROUP(fs->super)) == 0)
			check_block_uninit(fs, map,
					   (i - fs->super->s_first_data_block) /
					   EXT2_BLOCKS_PER_GROUP(fs->super));

		if (!ext2fs_fast_test_block_bitmap2(map, i)) {
			*ret = i;
			return 0;
		}
		i = (i + c_ratio) & ~(c_ratio - 1);
		if (i >= ext2fs_blocks_count(fs->super))
			i = fs->super->s_first_data_block;
	} while (i != goal);
	return EXT2_ET_BLOCK_ALLOC_FAIL;
}

errcode_t ext2fs_new_block(ext2_filsys fs, blk_t goal,
			   ext2fs_block_bitmap map, blk_t *ret)
{
	errcode_t retval;
	blk64_t val;
	retval = ext2fs_new_block2(fs, goal, map, &val);
	if (!retval)
		*ret = (blk_t) val;
	return retval;
}

/*
 * This function zeros out the allocated block, and updates all of the
 * appropriate filesystem records.
 */
errcode_t ext2fs_alloc_block2(ext2_filsys fs, blk64_t goal,
			     char *block_buf, blk64_t *ret)
{
	errcode_t	retval;
	blk64_t		block;
	char		*buf = 0;

	if (!block_buf) {
		retval = ext2fs_get_mem(fs->blocksize, &buf);
		if (retval)
			return retval;
		block_buf = buf;
	}
	memset(block_buf, 0, fs->blocksize);

	if (fs->get_alloc_block) {
		retval = (fs->get_alloc_block)(fs, goal, &block);
		if (retval)
			goto fail;
	} else {
		if (!fs->block_map) {
			retval = ext2fs_read_block_bitmap(fs);
			if (retval)
				goto fail;
		}

		retval = ext2fs_new_block2(fs, goal, 0, &block);
		if (retval)
			goto fail;
	}

	retval = io_channel_write_blk64(fs->io, block, 1, block_buf);
	if (retval)
		goto fail;

	ext2fs_block_alloc_stats2(fs, block, +1);
	*ret = block;

fail:
	if (buf)
		ext2fs_free_mem(&buf);
	return retval;
}

errcode_t ext2fs_alloc_block(ext2_filsys fs, blk_t goal,
			     char *block_buf, blk_t *ret)
{
	errcode_t retval;
	blk64_t	val;
	retval = ext2fs_alloc_block2(fs, goal, block_buf, &val);
	if (!retval)
		*ret = (blk_t) val;
	return retval;
}

errcode_t ext2fs_get_free_blocks2(ext2_filsys fs, blk64_t start, blk64_t finish,
				 int num, ext2fs_block_bitmap map, blk64_t *ret)
{
	blk64_t	b = start;
	int	c_ratio;

	EXT2_CHECK_MAGIC(fs, EXT2_ET_MAGIC_EXT2FS_FILSYS);

	if (!map)
		map = fs->block_map;
	if (!map)
		return EXT2_ET_NO_BLOCK_BITMAP;
	if (!b)
		b = fs->super->s_first_data_block;
	if (!finish)
		finish = start;
	if (!num)
		num = 1;
	c_ratio = 1 << ext2fs_get_bitmap_granularity(map);
	b &= ~(c_ratio - 1);
	finish &= ~(c_ratio -1);
	do {
		if (b+num-1 > ext2fs_blocks_count(fs->super))
			b = fs->super->s_first_data_block;
		if (ext2fs_fast_test_block_bitmap_range2(map, b, num)) {
			*ret = b;
			return 0;
		}
		b += c_ratio;
	} while (b != finish);
	return EXT2_ET_BLOCK_ALLOC_FAIL;
}

errcode_t ext2fs_get_free_blocks(ext2_filsys fs, blk_t start, blk_t finish,
				 int num, ext2fs_block_bitmap map, blk_t *ret)
{
	errcode_t retval;
	blk64_t val;
	retval = ext2fs_get_free_blocks2(fs, start, finish, num, map, &val);
	if(!retval)
		*ret = (blk_t) val;
	return retval;
}

void ext2fs_set_alloc_block_callback(ext2_filsys fs,
				     errcode_t (*func)(ext2_filsys fs,
						       blk64_t goal,
						       blk64_t *ret),
				     errcode_t (**old)(ext2_filsys fs,
						       blk64_t goal,
						       blk64_t *ret))
{
	if (!fs || fs->magic != EXT2_ET_MAGIC_EXT2FS_FILSYS)
		return;

	if (old)
		*old = fs->get_alloc_block;

	fs->get_alloc_block = func;
}
