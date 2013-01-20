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
	blk_t		blk, super_blk, old_desc_blk, new_desc_blk;
	int		old_desc_blocks;

	if (!(EXT2_HAS_RO_COMPAT_FEATURE(fs->super,
					 EXT4_FEATURE_RO_COMPAT_GDT_CSUM)) ||
	    !(fs->group_desc[group].bg_flags & EXT2_BG_BLOCK_UNINIT))
		return;

	blk = (group * fs->super->s_blocks_per_group) +
		fs->super->s_first_data_block;

	ext2fs_super_and_bgd_loc(fs, group, &super_blk,
				 &old_desc_blk, &new_desc_blk, 0);

	if (fs->super->s_feature_incompat &
	    EXT2_FEATURE_INCOMPAT_META_BG)
		old_desc_blocks = fs->super->s_first_meta_bg;
	else
		old_desc_blocks = fs->desc_blocks + fs->super->s_reserved_gdt_blocks;

	for (i=0; i < fs->super->s_blocks_per_group; i++, blk++) {
		if ((blk == super_blk) ||
		    (old_desc_blk && old_desc_blocks &&
		     (blk >= old_desc_blk) &&
		     (blk < old_desc_blk + old_desc_blocks)) ||
		    (new_desc_blk && (blk == new_desc_blk)) ||
		    (blk == fs->group_desc[group].bg_block_bitmap) ||
		    (blk == fs->group_desc[group].bg_inode_bitmap) ||
		    (blk >= fs->group_desc[group].bg_inode_table &&
		     (blk < fs->group_desc[group].bg_inode_table
		      + fs->inode_blocks_per_group)))
			ext2fs_fast_mark_block_bitmap(map, blk);
		else
			ext2fs_fast_unmark_block_bitmap(map, blk);
	}
	fs->group_desc[group].bg_flags &= ~EXT2_BG_BLOCK_UNINIT;
	ext2fs_group_desc_csum_set(fs, group);
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
	    !(fs->group_desc[group].bg_flags & EXT2_BG_INODE_UNINIT))
		return;

	ino = (group * fs->super->s_inodes_per_group) + 1;
	for (i=0; i < fs->super->s_inodes_per_group; i++, ino++)
		ext2fs_fast_unmark_inode_bitmap(map, ino);

	fs->group_desc[group].bg_flags &= ~EXT2_BG_INODE_UNINIT;
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
	ext2_ino_t	dir_group = 0;
	ext2_ino_t	i;
	ext2_ino_t	start_inode;

	EXT2_CHECK_MAGIC(fs, EXT2_ET_MAGIC_EXT2FS_FILSYS);

	if (!map)
		map = fs->inode_map;
	if (!map)
		return EXT2_ET_NO_INODE_BITMAP;

	if (dir > 0)
		dir_group = (dir - 1) / EXT2_INODES_PER_GROUP(fs->super);

	start_inode = (dir_group * EXT2_INODES_PER_GROUP(fs->super)) + 1;
	if (start_inode < EXT2_FIRST_INODE(fs->super))
		start_inode = EXT2_FIRST_INODE(fs->super);
	if (start_inode > fs->super->s_inodes_count)
		return EXT2_ET_INODE_ALLOC_FAIL;
	i = start_inode;

	do {
		if (((i - 1) % EXT2_INODES_PER_GROUP(fs->super)) == 0)
			check_inode_uninit(fs, map, (i - 1) /
					   EXT2_INODES_PER_GROUP(fs->super));

		if (!ext2fs_fast_test_inode_bitmap(map, i))
			break;
		i++;
		if (i > fs->super->s_inodes_count)
			i = EXT2_FIRST_INODE(fs->super);
	} while (i != start_inode);

	if (ext2fs_test_inode_bitmap(map, i))
		return EXT2_ET_INODE_ALLOC_FAIL;
	*ret = i;
	return 0;
}

/*
 * Stupid algorithm --- we now just search forward starting from the
 * goal.  Should put in a smarter one someday....
 */
errcode_t ext2fs_new_block(ext2_filsys fs, blk_t goal,
			   ext2fs_block_bitmap map, blk_t *ret)
{
	blk_t	i;

	EXT2_CHECK_MAGIC(fs, EXT2_ET_MAGIC_EXT2FS_FILSYS);

	if (!map)
		map = fs->block_map;
	if (!map)
		return EXT2_ET_NO_BLOCK_BITMAP;
	if (!goal || (goal >= fs->super->s_blocks_count))
		goal = fs->super->s_first_data_block;
	i = goal;
	check_block_uninit(fs, map,
			   (i - fs->super->s_first_data_block) /
			   EXT2_BLOCKS_PER_GROUP(fs->super));
	do {
		if (((i - fs->super->s_first_data_block) %
		     EXT2_BLOCKS_PER_GROUP(fs->super)) == 0)
			check_block_uninit(fs, map,
					   (i - fs->super->s_first_data_block) /
					   EXT2_BLOCKS_PER_GROUP(fs->super));

		if (!ext2fs_fast_test_block_bitmap(map, i)) {
			*ret = i;
			return 0;
		}
		i++;
		if (i >= fs->super->s_blocks_count)
			i = fs->super->s_first_data_block;
	} while (i != goal);
	return EXT2_ET_BLOCK_ALLOC_FAIL;
}

/*
 * This function zeros out the allocated block, and updates all of the
 * appropriate filesystem records.
 */
errcode_t ext2fs_alloc_block(ext2_filsys fs, blk_t goal,
			     char *block_buf, blk_t *ret)
{
	errcode_t	retval;
	blk_t		block;
	char		*buf = 0;

	if (!block_buf) {
		retval = ext2fs_get_mem(fs->blocksize, &buf);
		if (retval)
			return retval;
		block_buf = buf;
	}
	memset(block_buf, 0, fs->blocksize);

	if (fs->get_alloc_block) {
		blk64_t	new;

		retval = (fs->get_alloc_block)(fs, (blk64_t) goal, &new);
		if (retval)
			goto fail;
		block = (blk_t) new;
	} else {
		if (!fs->block_map) {
			retval = ext2fs_read_block_bitmap(fs);
			if (retval)
				goto fail;
		}

		retval = ext2fs_new_block(fs, goal, 0, &block);
		if (retval)
			goto fail;
	}

	retval = io_channel_write_blk(fs->io, block, 1, block_buf);
	if (retval)
		goto fail;

	ext2fs_block_alloc_stats(fs, block, +1);
	*ret = block;

fail:
	if (buf)
		ext2fs_free_mem(&buf);
	return retval;
}

errcode_t ext2fs_get_free_blocks(ext2_filsys fs, blk_t start, blk_t finish,
				 int num, ext2fs_block_bitmap map, blk_t *ret)
{
	blk_t	b = start;

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
	do {
		if (b+num-1 > fs->super->s_blocks_count)
			b = fs->super->s_first_data_block;
		if (ext2fs_fast_test_block_bitmap_range(map, b, num)) {
			*ret = b;
			return 0;
		}
		b++;
	} while (b != finish);
	return EXT2_ET_BLOCK_ALLOC_FAIL;
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
