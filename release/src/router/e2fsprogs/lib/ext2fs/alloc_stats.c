/*
 * alloc_stats.c --- Update allocation statistics for ext2fs
 *
 * Copyright (C) 2001 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Library
 * General Public License, version 2.
 * %End-Header%
 */

#include <stdio.h>

#include "ext2_fs.h"
#include "ext2fs.h"

void ext2fs_inode_alloc_stats2(ext2_filsys fs, ext2_ino_t ino,
			       int inuse, int isdir)
{
	int	group = ext2fs_group_of_ino(fs, ino);

#ifndef OMIT_COM_ERR
	if (ino > fs->super->s_inodes_count) {
		com_err("ext2fs_inode_alloc_stats2", 0,
			"Illegal inode number: %lu", (unsigned long) ino);
		return;
	}
#endif
	if (inuse > 0)
		ext2fs_mark_inode_bitmap(fs->inode_map, ino);
	else
		ext2fs_unmark_inode_bitmap(fs->inode_map, ino);
	fs->group_desc[group].bg_free_inodes_count -= inuse;
	if (isdir)
		fs->group_desc[group].bg_used_dirs_count += inuse;

	/* We don't strictly need to be clearing the uninit flag if inuse < 0
	 * (i.e. freeing inodes) but it also means something is bad. */
	fs->group_desc[group].bg_flags &= ~EXT2_BG_INODE_UNINIT;
	if (EXT2_HAS_RO_COMPAT_FEATURE(fs->super,
				       EXT4_FEATURE_RO_COMPAT_GDT_CSUM)) {
		ext2_ino_t first_unused_inode =	fs->super->s_inodes_per_group -
			fs->group_desc[group].bg_itable_unused +
			group * fs->super->s_inodes_per_group + 1;

		if (ino >= first_unused_inode)
			fs->group_desc[group].bg_itable_unused =
				group * fs->super->s_inodes_per_group +
				fs->super->s_inodes_per_group - ino;
		ext2fs_group_desc_csum_set(fs, group);
	}

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

#ifndef OMIT_COM_ERR
	if (blk >= fs->super->s_blocks_count) {
		com_err("ext2fs_block_alloc_stats", 0,
			"Illegal block number: %lu", (unsigned long) blk);
		return;
	}
#endif
	if (inuse > 0)
		ext2fs_mark_block_bitmap(fs->block_map, blk);
	else
		ext2fs_unmark_block_bitmap(fs->block_map, blk);
	fs->group_desc[group].bg_free_blocks_count -= inuse;
	fs->group_desc[group].bg_flags &= ~EXT2_BG_BLOCK_UNINIT;
	ext2fs_group_desc_csum_set(fs, group);

	fs->super->s_free_blocks_count -= inuse;
	ext2fs_mark_super_dirty(fs);
	ext2fs_mark_bb_dirty(fs);
	if (fs->block_alloc_stats)
		(fs->block_alloc_stats)(fs, (blk64_t) blk, inuse);
}

void ext2fs_set_block_alloc_stats_callback(ext2_filsys fs,
					   void (*func)(ext2_filsys fs,
							blk64_t blk,
							int inuse),
					   void (**old)(ext2_filsys fs,
							blk64_t blk,
							int inuse))
{
	if (!fs || fs->magic != EXT2_ET_MAGIC_EXT2FS_FILSYS)
		return;
	if (old)
		*old = fs->block_alloc_stats;

	fs->block_alloc_stats = func;
}
