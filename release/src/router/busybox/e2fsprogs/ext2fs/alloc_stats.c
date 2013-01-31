/* vi: set sw=4 ts=4: */
/*
 * alloc_stats.c --- Update allocation statistics for ext2fs
 *
 * Copyright (C) 2001 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 *
 */

#include <stdio.h>

#include "ext2_fs.h"
#include "ext2fs.h"

void ext2fs_inode_alloc_stats2(ext2_filsys fs, ext2_ino_t ino,
			       int inuse, int isdir)
{
	int	group = ext2fs_group_of_ino(fs, ino);

	if (inuse > 0)
		ext2fs_mark_inode_bitmap(fs->inode_map, ino);
	else
		ext2fs_unmark_inode_bitmap(fs->inode_map, ino);
	fs->group_desc[group].bg_free_inodes_count -= inuse;
	if (isdir)
		fs->group_desc[group].bg_used_dirs_count += inuse;
	fs->super->s_free_inodes_count -= inuse;
	ext2fs_mark_super_dirty(fs);
	ext2fs_mark_ib_dirty(fs);
}

void ext2fs_inode_alloc_stats(ext2_filsys fs, ext2_ino_t ino, int inuse)
{
	ext2fs_inode_alloc_stats2(fs, ino, inuse, 0);
}

void ext2fs_block_alloc_stats(ext2_filsys fs, blk_t blk, int inuse)
{
	int	group = ext2fs_group_of_blk(fs, blk);

	if (inuse > 0)
		ext2fs_mark_block_bitmap(fs->block_map, blk);
	else
		ext2fs_unmark_block_bitmap(fs->block_map, blk);
	fs->group_desc[group].bg_free_blocks_count -= inuse;
	fs->super->s_free_blocks_count -= inuse;
	ext2fs_mark_super_dirty(fs);
	ext2fs_mark_bb_dirty(fs);
}
