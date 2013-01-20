/*
 * resize2fs.c --- ext2 main routine
 *
 * Copyright (C) 1997, 1998 by Theodore Ts'o and
 * 	PowerQuest, Inc.
 *
 * Copyright (C) 1999, 2000 by Theosore Ts'o
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

/*
 * Resizing a filesystem consists of the following phases:
 *
 *	1.  Adjust superblock and write out new parts of the inode
 * 		table
 * 	2.  Determine blocks which need to be relocated, and copy the
 * 		contents of blocks from their old locations to the new ones.
 * 	3.  Scan the inode table, doing the following:
 * 		a.  If blocks have been moved, update the block
 * 			pointers in the inodes and indirect blocks to
 * 			point at the new block locations.
 * 		b.  If parts of the inode table need to be evacuated,
 * 			copy inodes from their old locations to their
 * 			new ones.
 * 		c.  If (b) needs to be done, note which blocks contain
 * 			directory information, since we will need to
 * 			update the directory information.
 * 	4.  Update the directory blocks with the new inode locations.
 * 	5.  Move the inode tables, if necessary.
 */

#include "resize2fs.h"
#include <time.h>

#ifdef __linux__			/* Kludge for debugging */
#define RESIZE2FS_DEBUG
#endif

static void fix_uninit_block_bitmaps(ext2_filsys fs);
static errcode_t adjust_superblock(ext2_resize_t rfs, blk_t new_size);
static errcode_t blocks_to_move(ext2_resize_t rfs);
static errcode_t block_mover(ext2_resize_t rfs);
static errcode_t inode_scan_and_fix(ext2_resize_t rfs);
static errcode_t inode_ref_fix(ext2_resize_t rfs);
static errcode_t move_itables(ext2_resize_t rfs);
static errcode_t fix_resize_inode(ext2_filsys fs);
static errcode_t ext2fs_calculate_summary_stats(ext2_filsys fs);
static errcode_t fix_sb_journal_backup(ext2_filsys fs);

/*
 * Some helper CPP macros
 */
#define FS_BLOCK_BM(fs, i) ((fs)->group_desc[(i)].bg_block_bitmap)
#define FS_INODE_BM(fs, i) ((fs)->group_desc[(i)].bg_inode_bitmap)
#define FS_INODE_TB(fs, i) ((fs)->group_desc[(i)].bg_inode_table)

#define IS_BLOCK_BM(fs, i, blk) ((blk) == FS_BLOCK_BM((fs),(i)))
#define IS_INODE_BM(fs, i, blk) ((blk) == FS_INODE_BM((fs),(i)))

#define IS_INODE_TB(fs, i, blk) (((blk) >= FS_INODE_TB((fs), (i))) && \
				 ((blk) < (FS_INODE_TB((fs), (i)) + \
					   (fs)->inode_blocks_per_group)))

#define META_OVERHEAD(fs) (2 + (fs)->inode_blocks_per_group)
#define SUPER_OVERHEAD(fs) (1 + (fs)->desc_blocks +\
			    (fs)->super->s_reserved_gdt_blocks)

/*
 * This is the top-level routine which does the dirty deed....
 */
errcode_t resize_fs(ext2_filsys fs, blk_t *new_size, int flags,
		    errcode_t (*progress)(ext2_resize_t rfs, int pass,
					  unsigned long cur,
					  unsigned long max_val))
{
	ext2_resize_t	rfs;
	errcode_t	retval;

	retval = ext2fs_read_bitmaps(fs);
	if (retval)
		return retval;

	fs->super->s_state |= EXT2_ERROR_FS;
	ext2fs_mark_super_dirty(fs);
	ext2fs_flush(fs);

	/*
	 * Create the data structure
	 */
	retval = ext2fs_get_mem(sizeof(struct ext2_resize_struct), &rfs);
	if (retval)
		return retval;
	memset(rfs, 0, sizeof(struct ext2_resize_struct));

	fix_uninit_block_bitmaps(fs);
	fs->priv_data = rfs;
	rfs->old_fs = fs;
	rfs->flags = flags;
	rfs->itable_buf	 = 0;
	rfs->progress = progress;
	retval = ext2fs_dup_handle(fs, &rfs->new_fs);
	if (retval)
		goto errout;

	retval = adjust_superblock(rfs, *new_size);
	if (retval)
		goto errout;

	fix_uninit_block_bitmaps(rfs->new_fs);
	/* Clear the block bitmap uninit flag for the last block group */
	rfs->new_fs->group_desc[rfs->new_fs->group_desc_count-1].bg_flags &=
		~EXT2_BG_BLOCK_UNINIT;

	*new_size = rfs->new_fs->super->s_blocks_count;

	retval = blocks_to_move(rfs);
	if (retval)
		goto errout;

#ifdef RESIZE2FS_DEBUG
	if (rfs->flags & RESIZE_DEBUG_BMOVE)
		printf("Number of free blocks: %u/%u, Needed: %d\n",
		       rfs->old_fs->super->s_free_blocks_count,
		       rfs->new_fs->super->s_free_blocks_count,
		       rfs->needed_blocks);
#endif

	retval = block_mover(rfs);
	if (retval)
		goto errout;

	retval = inode_scan_and_fix(rfs);
	if (retval)
		goto errout;

	retval = inode_ref_fix(rfs);
	if (retval)
		goto errout;

	retval = move_itables(rfs);
	if (retval)
		goto errout;

	retval = ext2fs_calculate_summary_stats(rfs->new_fs);
	if (retval)
		goto errout;

	retval = fix_resize_inode(rfs->new_fs);
	if (retval)
		goto errout;

	retval = fix_sb_journal_backup(rfs->new_fs);
	if (retval)
		goto errout;

	rfs->new_fs->super->s_state &= ~EXT2_ERROR_FS;
	rfs->new_fs->flags &= ~EXT2_FLAG_MASTER_SB_ONLY;
	retval = ext2fs_close(rfs->new_fs);
	if (retval)
		goto errout;

	rfs->flags = flags;

	ext2fs_free(rfs->old_fs);
	if (rfs->itable_buf)
		ext2fs_free_mem(&rfs->itable_buf);
	if (rfs->reserve_blocks)
		ext2fs_free_block_bitmap(rfs->reserve_blocks);
	if (rfs->move_blocks)
		ext2fs_free_block_bitmap(rfs->move_blocks);
	ext2fs_free_mem(&rfs);

	return 0;

errout:
	if (rfs->new_fs)
		ext2fs_free(rfs->new_fs);
	if (rfs->itable_buf)
		ext2fs_free_mem(&rfs->itable_buf);
	ext2fs_free_mem(&rfs);
	return retval;
}

/*
 * Clean up the bitmaps for unitialized bitmaps
 */
static void fix_uninit_block_bitmaps(ext2_filsys fs)
{
	blk_t		i, blk, super_blk, old_desc_blk, new_desc_blk;
	int		old_desc_blocks;
	dgrp_t		g;

	if (!(EXT2_HAS_RO_COMPAT_FEATURE(fs->super,
					 EXT4_FEATURE_RO_COMPAT_GDT_CSUM)))
		return;

	for (g=0; g < fs->group_desc_count; g++) {
		if (!(fs->group_desc[g].bg_flags & EXT2_BG_BLOCK_UNINIT))
			continue;

		blk = (g * fs->super->s_blocks_per_group) +
			fs->super->s_first_data_block;

		ext2fs_super_and_bgd_loc(fs, g, &super_blk,
					 &old_desc_blk, &new_desc_blk, 0);

		if (fs->super->s_feature_incompat & EXT2_FEATURE_INCOMPAT_META_BG)
			old_desc_blocks = fs->super->s_first_meta_bg;
		else
			old_desc_blocks = fs->desc_blocks +
				fs->super->s_reserved_gdt_blocks;

		for (i=0; i < fs->super->s_blocks_per_group; i++, blk++) {
			if (blk >= fs->super->s_blocks_count)
				break;
			if ((blk == super_blk) ||
			    (old_desc_blk && old_desc_blocks &&
			     (blk >= old_desc_blk) &&
			     (blk < old_desc_blk + old_desc_blocks)) ||
			    (new_desc_blk && (blk == new_desc_blk)) ||
			    (blk == fs->group_desc[g].bg_block_bitmap) ||
			    (blk == fs->group_desc[g].bg_inode_bitmap) ||
			    (blk >= fs->group_desc[g].bg_inode_table &&
			     (blk < fs->group_desc[g].bg_inode_table
			      + fs->inode_blocks_per_group)))
				ext2fs_fast_mark_block_bitmap(fs->block_map, blk);
			else
				ext2fs_fast_unmark_block_bitmap(fs->block_map, blk);
		}
	}
}

/* --------------------------------------------------------------------
 *
 * Resize processing, phase 1.
 *
 * In this phase we adjust the in-memory superblock information, and
 * initialize any new parts of the inode table.  The new parts of the
 * inode table are created in virgin disk space, so we can abort here
 * without any side effects.
 * --------------------------------------------------------------------
 */

/*
 * If the group descriptor's bitmap and inode table blocks are valid,
 * release them in the new filesystem data structure, and mark them as
 * reserved so the old inode table blocks don't get overwritten.
 */
static void free_gdp_blocks(ext2_filsys fs,
			    ext2fs_block_bitmap reserve_blocks,
			    struct ext2_group_desc *gdp)
{
	blk_t	blk;
	int	j;

	if (gdp->bg_block_bitmap &&
	    (gdp->bg_block_bitmap < fs->super->s_blocks_count)) {
		ext2fs_block_alloc_stats(fs, gdp->bg_block_bitmap, -1);
		ext2fs_mark_block_bitmap(reserve_blocks,
					 gdp->bg_block_bitmap);
	}

	if (gdp->bg_inode_bitmap &&
	    (gdp->bg_inode_bitmap < fs->super->s_blocks_count)) {
		ext2fs_block_alloc_stats(fs, gdp->bg_inode_bitmap, -1);
		ext2fs_mark_block_bitmap(reserve_blocks,
					 gdp->bg_inode_bitmap);
	}

	if (gdp->bg_inode_table == 0 ||
	    (gdp->bg_inode_table >= fs->super->s_blocks_count))
		return;

	for (blk = gdp->bg_inode_table, j = 0;
	     j < fs->inode_blocks_per_group; j++, blk++) {
		if (blk >= fs->super->s_blocks_count)
			break;
		ext2fs_block_alloc_stats(fs, blk, -1);
		ext2fs_mark_block_bitmap(reserve_blocks, blk);
	}
}

/*
 * This routine is shared by the online and offline resize routines.
 * All of the information which is adjusted in memory is done here.
 *
 * The reserve_blocks parameter is only needed when shrinking the
 * filesystem.
 */
errcode_t adjust_fs_info(ext2_filsys fs, ext2_filsys old_fs,
			 ext2fs_block_bitmap reserve_blocks, blk_t new_size)
{
	errcode_t	retval;
	int		overhead = 0;
	int		rem;
	blk_t		blk, group_block;
	ext2_ino_t	real_end;
	int		adj, old_numblocks, numblocks, adjblocks;
	unsigned long	i, j, old_desc_blocks, max_group;
	unsigned int	meta_bg, meta_bg_size;
	int		has_super, csum_flag;
	unsigned long long new_inodes;	/* u64 to check for overflow */
	double		percent;

	fs->super->s_blocks_count = new_size;

retry:
	fs->group_desc_count = ext2fs_div_ceil(fs->super->s_blocks_count -
				       fs->super->s_first_data_block,
				       EXT2_BLOCKS_PER_GROUP(fs->super));
	if (fs->group_desc_count == 0)
		return EXT2_ET_TOOSMALL;
	fs->desc_blocks = ext2fs_div_ceil(fs->group_desc_count,
					  EXT2_DESC_PER_BLOCK(fs->super));

	/*
	 * Overhead is the number of bookkeeping blocks per group.  It
	 * includes the superblock backup, the group descriptor
	 * backups, the inode bitmap, the block bitmap, and the inode
	 * table.
	 */
	overhead = (int) (2 + fs->inode_blocks_per_group);

	if (ext2fs_bg_has_super(fs, fs->group_desc_count - 1))
		overhead += 1 + fs->desc_blocks +
			fs->super->s_reserved_gdt_blocks;

	/*
	 * See if the last group is big enough to support the
	 * necessary data structures.  If not, we need to get rid of
	 * it.
	 */
	rem = (fs->super->s_blocks_count - fs->super->s_first_data_block) %
		fs->super->s_blocks_per_group;
	if ((fs->group_desc_count == 1) && rem && (rem < overhead))
		return EXT2_ET_TOOSMALL;
	if (rem && (rem < overhead+50)) {
		fs->super->s_blocks_count -= rem;
		goto retry;
	}
	/*
	 * Adjust the number of inodes
	 */
	new_inodes =(unsigned long long) fs->super->s_inodes_per_group * fs->group_desc_count;
	if (new_inodes > ~0U) {
		fprintf(stderr, _("inodes (%llu) must be less than %u"),
				   new_inodes, ~0U);
		return EXT2_ET_TOO_MANY_INODES;
	}
	fs->super->s_inodes_count = fs->super->s_inodes_per_group *
		fs->group_desc_count;

	/*
	 * Adjust the number of free blocks
	 */
	blk = old_fs->super->s_blocks_count;
	if (blk > fs->super->s_blocks_count)
		fs->super->s_free_blocks_count -=
			(blk - fs->super->s_blocks_count);
	else
		fs->super->s_free_blocks_count +=
			(fs->super->s_blocks_count - blk);

	/*
	 * Adjust the number of reserved blocks
	 */
	percent = (old_fs->super->s_r_blocks_count * 100.0) /
		old_fs->super->s_blocks_count;
	fs->super->s_r_blocks_count = (unsigned int) (percent *
					fs->super->s_blocks_count / 100.0);

	/*
	 * Adjust the bitmaps for size
	 */
	retval = ext2fs_resize_inode_bitmap(fs->super->s_inodes_count,
					    fs->super->s_inodes_count,
					    fs->inode_map);
	if (retval) goto errout;

	real_end = ((EXT2_BLOCKS_PER_GROUP(fs->super)
		     * fs->group_desc_count)) - 1 +
			     fs->super->s_first_data_block;
	retval = ext2fs_resize_block_bitmap(fs->super->s_blocks_count-1,
					    real_end, fs->block_map);

	if (retval) goto errout;

	/*
	 * Reallocate the group descriptors as necessary.
	 */
	if (old_fs->desc_blocks != fs->desc_blocks) {
		retval = ext2fs_resize_mem(old_fs->desc_blocks *
					   fs->blocksize,
					   fs->desc_blocks * fs->blocksize,
					   &fs->group_desc);
		if (retval)
			goto errout;
		if (fs->desc_blocks > old_fs->desc_blocks)
			memset((char *) fs->group_desc +
			       (old_fs->desc_blocks * fs->blocksize), 0,
			       (fs->desc_blocks - old_fs->desc_blocks) *
			       fs->blocksize);
	}

	/*
	 * If the resize_inode feature is set, and we are changing the
	 * number of descriptor blocks, then adjust
	 * s_reserved_gdt_blocks if possible to avoid needing to move
	 * the inode table either now or in the future.
	 */
	if ((fs->super->s_feature_compat &
	     EXT2_FEATURE_COMPAT_RESIZE_INODE) &&
	    (old_fs->desc_blocks != fs->desc_blocks)) {
		int new;

		new = ((int) fs->super->s_reserved_gdt_blocks) +
			(old_fs->desc_blocks - fs->desc_blocks);
		if (new < 0)
			new = 0;
		if (new > (int) fs->blocksize/4)
			new = fs->blocksize/4;
		fs->super->s_reserved_gdt_blocks = new;
	}

	/*
	 * If we are shrinking the number of block groups, we're done
	 * and can exit now.
	 */
	if (old_fs->group_desc_count > fs->group_desc_count) {
		/*
		 * Check the block groups that we are chopping off
		 * and free any blocks associated with their metadata
		 */
		for (i = fs->group_desc_count;
		     i < old_fs->group_desc_count; i++) {
			free_gdp_blocks(fs, reserve_blocks,
					&old_fs->group_desc[i]);
		}
		retval = 0;
		goto errout;
	}

	/*
	 * Fix the count of the last (old) block group
	 */
	old_numblocks = (old_fs->super->s_blocks_count -
			 old_fs->super->s_first_data_block) %
				 old_fs->super->s_blocks_per_group;
	if (!old_numblocks)
		old_numblocks = old_fs->super->s_blocks_per_group;
	if (old_fs->group_desc_count == fs->group_desc_count) {
		numblocks = (fs->super->s_blocks_count -
			     fs->super->s_first_data_block) %
			fs->super->s_blocks_per_group;
		if (!numblocks)
			numblocks = fs->super->s_blocks_per_group;
	} else
		numblocks = fs->super->s_blocks_per_group;
	i = old_fs->group_desc_count - 1;
	fs->group_desc[i].bg_free_blocks_count += (numblocks-old_numblocks);
	ext2fs_group_desc_csum_set(fs, i);

	/*
	 * If the number of block groups is staying the same, we're
	 * done and can exit now.  (If the number block groups is
	 * shrinking, we had exited earlier.)
	 */
	if (old_fs->group_desc_count >= fs->group_desc_count) {
		retval = 0;
		goto errout;
	}

	/*
	 * Initialize the new block group descriptors
	 */
	group_block = fs->super->s_first_data_block +
		old_fs->group_desc_count * fs->super->s_blocks_per_group;

	csum_flag = EXT2_HAS_RO_COMPAT_FEATURE(fs->super,
					       EXT4_FEATURE_RO_COMPAT_GDT_CSUM);
	adj = old_fs->group_desc_count;
	max_group = fs->group_desc_count - adj;
	if (fs->super->s_feature_incompat & EXT2_FEATURE_INCOMPAT_META_BG)
		old_desc_blocks = fs->super->s_first_meta_bg;
	else
		old_desc_blocks = fs->desc_blocks +
			fs->super->s_reserved_gdt_blocks;
	for (i = old_fs->group_desc_count;
	     i < fs->group_desc_count; i++) {
		memset(&fs->group_desc[i], 0,
		       sizeof(struct ext2_group_desc));
		adjblocks = 0;

		fs->group_desc[i].bg_flags = 0;
		if (csum_flag)
			fs->group_desc[i].bg_flags |= EXT2_BG_INODE_UNINIT |
				EXT2_BG_INODE_ZEROED;
		if (i == fs->group_desc_count-1) {
			numblocks = (fs->super->s_blocks_count -
				     fs->super->s_first_data_block) %
					     fs->super->s_blocks_per_group;
			if (!numblocks)
				numblocks = fs->super->s_blocks_per_group;
		} else {
			numblocks = fs->super->s_blocks_per_group;
			if (csum_flag)
				fs->group_desc[i].bg_flags |=
					EXT2_BG_BLOCK_UNINIT;
		}

		has_super = ext2fs_bg_has_super(fs, i);
		if (has_super) {
			ext2fs_block_alloc_stats(fs, group_block, +1);
			adjblocks++;
		}
		meta_bg_size = EXT2_DESC_PER_BLOCK(fs->super);
		meta_bg = i / meta_bg_size;
		if (!(fs->super->s_feature_incompat &
		      EXT2_FEATURE_INCOMPAT_META_BG) ||
		    (meta_bg < fs->super->s_first_meta_bg)) {
			if (has_super) {
				for (j=0; j < old_desc_blocks; j++)
					ext2fs_block_alloc_stats(fs,
						 group_block + 1 + j, +1);
				adjblocks += old_desc_blocks;
			}
		} else {
			if (has_super)
				has_super = 1;
			if (((i % meta_bg_size) == 0) ||
			    ((i % meta_bg_size) == 1) ||
			    ((i % meta_bg_size) == (meta_bg_size-1)))
				ext2fs_block_alloc_stats(fs,
						 group_block + has_super, +1);
		}

		adjblocks += 2 + fs->inode_blocks_per_group;

		numblocks -= adjblocks;
		fs->super->s_free_blocks_count -= adjblocks;
		fs->super->s_free_inodes_count +=
			fs->super->s_inodes_per_group;
		fs->group_desc[i].bg_free_blocks_count = numblocks;
		fs->group_desc[i].bg_free_inodes_count =
			fs->super->s_inodes_per_group;
		fs->group_desc[i].bg_used_dirs_count = 0;
		ext2fs_group_desc_csum_set(fs, i);

		retval = ext2fs_allocate_group_table(fs, i, 0);
		if (retval) goto errout;

		group_block += fs->super->s_blocks_per_group;
	}
	retval = 0;

errout:
	return (retval);
}

/*
 * This routine adjusts the superblock and other data structures, both
 * in disk as well as in memory...
 */
static errcode_t adjust_superblock(ext2_resize_t rfs, blk_t new_size)
{
	ext2_filsys fs;
	int		adj = 0;
	errcode_t	retval;
	blk_t		group_block;
	unsigned long	i;
	unsigned long	max_group;

	fs = rfs->new_fs;
	ext2fs_mark_super_dirty(fs);
	ext2fs_mark_bb_dirty(fs);
	ext2fs_mark_ib_dirty(fs);

	retval = ext2fs_allocate_block_bitmap(fs, _("reserved blocks"),
					      &rfs->reserve_blocks);
	if (retval)
		return retval;

	retval = adjust_fs_info(fs, rfs->old_fs, rfs->reserve_blocks, new_size);
	if (retval)
		goto errout;

	/*
	 * Check to make sure there are enough inodes
	 */
	if ((rfs->old_fs->super->s_inodes_count -
	     rfs->old_fs->super->s_free_inodes_count) >
	    rfs->new_fs->super->s_inodes_count) {
		retval = ENOSPC;
		goto errout;
	}

	/*
	 * If we are shrinking the number block groups, we're done and
	 * can exit now.
	 */
	if (rfs->old_fs->group_desc_count > fs->group_desc_count) {
		retval = 0;
		goto errout;
	}

	/*
	 * If the number of block groups is staying the same, we're
	 * done and can exit now.  (If the number block groups is
	 * shrinking, we had exited earlier.)
	 */
	if (rfs->old_fs->group_desc_count >= fs->group_desc_count) {
		retval = 0;
		goto errout;
	}

	/*
	 * Initialize the new block group descriptors
	 */
	retval = ext2fs_get_array(fs->blocksize, fs->inode_blocks_per_group,
				&rfs->itable_buf);
	if (retval)
		goto errout;

	memset(rfs->itable_buf, 0, fs->blocksize * fs->inode_blocks_per_group);
	group_block = fs->super->s_first_data_block +
		rfs->old_fs->group_desc_count * fs->super->s_blocks_per_group;

	adj = rfs->old_fs->group_desc_count;
	max_group = fs->group_desc_count - adj;
	if (rfs->progress) {
		retval = rfs->progress(rfs, E2_RSZ_EXTEND_ITABLE_PASS,
				       0, max_group);
		if (retval)
			goto errout;
	}
	for (i = rfs->old_fs->group_desc_count;
	     i < fs->group_desc_count; i++) {
		/*
		 * Write out the new inode table
		 */
		retval = io_channel_write_blk(fs->io,
					      fs->group_desc[i].bg_inode_table,
					      fs->inode_blocks_per_group,
					      rfs->itable_buf);
		if (retval) goto errout;

		io_channel_flush(fs->io);
		if (rfs->progress) {
			retval = rfs->progress(rfs, E2_RSZ_EXTEND_ITABLE_PASS,
					       i - adj + 1, max_group);
			if (retval)
				goto errout;
		}
		group_block += fs->super->s_blocks_per_group;
	}
	io_channel_flush(fs->io);
	retval = 0;

errout:
	return retval;
}

/* --------------------------------------------------------------------
 *
 * Resize processing, phase 2.
 *
 * In this phase we adjust determine which blocks need to be moved, in
 * blocks_to_move().  We then copy the blocks to their ultimate new
 * destinations using block_mover().  Since we are copying blocks to
 * their new locations, again during this pass we can abort without
 * any problems.
 * --------------------------------------------------------------------
 */

/*
 * This helper function creates a block bitmap with all of the
 * filesystem meta-data blocks.
 */
static errcode_t mark_table_blocks(ext2_filsys fs,
				   ext2fs_block_bitmap bmap)
{
	blk_t			b;
	unsigned int		j;
	dgrp_t			i;
	unsigned long		meta_bg_size;
	unsigned int		old_desc_blocks;

	meta_bg_size = EXT2_DESC_PER_BLOCK(fs->super);
	if (fs->super->s_feature_incompat & EXT2_FEATURE_INCOMPAT_META_BG)
		old_desc_blocks = fs->super->s_first_meta_bg;
	else
		old_desc_blocks = fs->desc_blocks +
			fs->super->s_reserved_gdt_blocks;
	for (i = 0; i < fs->group_desc_count; i++) {
		ext2fs_reserve_super_and_bgd(fs, i, bmap);

		/*
		 * Mark the blocks used for the inode table
		 */
		for (j = 0, b = fs->group_desc[i].bg_inode_table;
		     j < (unsigned int) fs->inode_blocks_per_group;
		     j++, b++)
			ext2fs_mark_block_bitmap(bmap, b);

		/*
		 * Mark block used for the block bitmap
		 */
		ext2fs_mark_block_bitmap(bmap,
					 fs->group_desc[i].bg_block_bitmap);

		/*
		 * Mark block used for the inode bitmap
		 */
		ext2fs_mark_block_bitmap(bmap,
					 fs->group_desc[i].bg_inode_bitmap);
	}
	return 0;
}

/*
 * This function checks to see if a particular block (either a
 * superblock or a block group descriptor) overlaps with an inode or
 * block bitmap block, or with the inode table.
 */
static void mark_fs_metablock(ext2_resize_t rfs,
			      ext2fs_block_bitmap meta_bmap,
			      int group, blk_t blk)
{
	ext2_filsys 	fs = rfs->new_fs;

	ext2fs_mark_block_bitmap(rfs->reserve_blocks, blk);
	ext2fs_block_alloc_stats(fs, blk, +1);

	/*
	 * Check to see if we overlap with the inode or block bitmap,
	 * or the inode tables.  If not, and the block is in use, then
	 * mark it as a block to be moved.
	 */
	if (IS_BLOCK_BM(fs, group, blk)) {
		FS_BLOCK_BM(fs, group) = 0;
		rfs->needed_blocks++;
	} else if (IS_INODE_BM(fs, group, blk)) {
		FS_INODE_BM(fs, group) = 0;
		rfs->needed_blocks++;
	} else if (IS_INODE_TB(fs, group, blk)) {
		FS_INODE_TB(fs, group) = 0;
		rfs->needed_blocks++;
	} else if (EXT2_HAS_RO_COMPAT_FEATURE(fs->super,
					      EXT4_FEATURE_RO_COMPAT_GDT_CSUM) &&
		   (fs->group_desc[group].bg_flags & EXT2_BG_BLOCK_UNINIT)) {
		/*
		 * If the block bitmap is uninitialized, which means
		 * nothing other than standard metadata in use.
		 */
		return;
	} else if (ext2fs_test_block_bitmap(rfs->old_fs->block_map, blk) &&
		   !ext2fs_test_block_bitmap(meta_bmap, blk)) {
		ext2fs_mark_block_bitmap(rfs->move_blocks, blk);
		rfs->needed_blocks++;
	}
}


/*
 * This routine marks and unmarks reserved blocks in the new block
 * bitmap.  It also determines which blocks need to be moved and
 * places this information into the move_blocks bitmap.
 */
static errcode_t blocks_to_move(ext2_resize_t rfs)
{
	int		j, has_super;
	dgrp_t		i, max_groups, g;
	blk_t		blk, group_blk;
	unsigned long	old_blocks, new_blocks;
	unsigned int	meta_bg, meta_bg_size;
	errcode_t	retval;
	ext2_filsys 	fs, old_fs;
	ext2fs_block_bitmap	meta_bmap;
	__u32		save_incompat_flag;

	fs = rfs->new_fs;
	old_fs = rfs->old_fs;
	if (old_fs->super->s_blocks_count > fs->super->s_blocks_count)
		fs = rfs->old_fs;

	retval = ext2fs_allocate_block_bitmap(fs, _("blocks to be moved"),
					      &rfs->move_blocks);
	if (retval)
		return retval;

	retval = ext2fs_allocate_block_bitmap(fs, _("meta-data blocks"),
					      &meta_bmap);
	if (retval)
		return retval;

	retval = mark_table_blocks(old_fs, meta_bmap);
	if (retval)
		return retval;

	fs = rfs->new_fs;

	/*
	 * If we're shrinking the filesystem, we need to move all of
	 * the blocks that don't fit any more
	 */
	for (blk = fs->super->s_blocks_count;
	     blk < old_fs->super->s_blocks_count; blk++) {
		g = ext2fs_group_of_blk(fs, blk);
		if (EXT2_HAS_RO_COMPAT_FEATURE(fs->super,
					       EXT4_FEATURE_RO_COMPAT_GDT_CSUM) &&
		    (old_fs->group_desc[g].bg_flags & EXT2_BG_BLOCK_UNINIT)) {
			/*
			 * The block bitmap is uninitialized, so skip
			 * to the next block group.
			 */
			blk = ((g+1) * fs->super->s_blocks_per_group) +
				fs->super->s_first_data_block - 1;
			continue;
		}
		if (ext2fs_test_block_bitmap(old_fs->block_map, blk) &&
		    !ext2fs_test_block_bitmap(meta_bmap, blk)) {
			ext2fs_mark_block_bitmap(rfs->move_blocks, blk);
			rfs->needed_blocks++;
		}
		ext2fs_mark_block_bitmap(rfs->reserve_blocks, blk);
	}

	if (fs->super->s_feature_incompat & EXT2_FEATURE_INCOMPAT_META_BG) {
		old_blocks = old_fs->super->s_first_meta_bg;
		new_blocks = fs->super->s_first_meta_bg;
	} else {
		old_blocks = old_fs->desc_blocks + old_fs->super->s_reserved_gdt_blocks;
		new_blocks = fs->desc_blocks + fs->super->s_reserved_gdt_blocks;
	}

	if (old_blocks == new_blocks) {
		retval = 0;
		goto errout;
	}

	max_groups = fs->group_desc_count;
	if (max_groups > old_fs->group_desc_count)
		max_groups = old_fs->group_desc_count;
	group_blk = old_fs->super->s_first_data_block;
	/*
	 * If we're reducing the number of descriptor blocks, this
	 * makes life easy.  :-)   We just have to mark some extra
	 * blocks as free.
	 */
	if (old_blocks > new_blocks) {
		for (i = 0; i < max_groups; i++) {
			if (!ext2fs_bg_has_super(fs, i)) {
				group_blk += fs->super->s_blocks_per_group;
				continue;
			}
			for (blk = group_blk+1+new_blocks;
			     blk < group_blk+1+old_blocks; blk++) {
				ext2fs_block_alloc_stats(fs, blk, -1);
				rfs->needed_blocks--;
			}
			group_blk += fs->super->s_blocks_per_group;
		}
		retval = 0;
		goto errout;
	}
	/*
	 * If we're increasing the number of descriptor blocks, life
	 * gets interesting....
	 */
	meta_bg_size = EXT2_DESC_PER_BLOCK(fs->super);
	for (i = 0; i < max_groups; i++) {
		has_super = ext2fs_bg_has_super(fs, i);
		if (has_super)
			mark_fs_metablock(rfs, meta_bmap, i, group_blk);

		meta_bg = i / meta_bg_size;
		if (!(fs->super->s_feature_incompat &
		      EXT2_FEATURE_INCOMPAT_META_BG) ||
		    (meta_bg < fs->super->s_first_meta_bg)) {
			if (has_super) {
				for (blk = group_blk+1;
				     blk < group_blk + 1 + new_blocks; blk++)
					mark_fs_metablock(rfs, meta_bmap,
							  i, blk);
			}
		} else {
			if (has_super)
				has_super = 1;
			if (((i % meta_bg_size) == 0) ||
			    ((i % meta_bg_size) == 1) ||
			    ((i % meta_bg_size) == (meta_bg_size-1)))
				mark_fs_metablock(rfs, meta_bmap, i,
						  group_blk + has_super);
		}

		if (fs->group_desc[i].bg_inode_table &&
		    fs->group_desc[i].bg_inode_bitmap &&
		    fs->group_desc[i].bg_block_bitmap)
			goto next_group;

		/*
		 * Reserve the existing meta blocks that we know
		 * aren't to be moved.
		 */
		if (fs->group_desc[i].bg_block_bitmap)
			ext2fs_mark_block_bitmap(rfs->reserve_blocks,
				 fs->group_desc[i].bg_block_bitmap);
		if (fs->group_desc[i].bg_inode_bitmap)
			ext2fs_mark_block_bitmap(rfs->reserve_blocks,
				 fs->group_desc[i].bg_inode_bitmap);
		if (fs->group_desc[i].bg_inode_table)
			for (blk = fs->group_desc[i].bg_inode_table, j=0;
			     j < fs->inode_blocks_per_group ; j++, blk++)
				ext2fs_mark_block_bitmap(rfs->reserve_blocks,
							 blk);

		/*
		 * Allocate the missing data structures
		 *
		 * XXX We have a problem with FLEX_BG and off-line
		 * resizing where we are growing the size of the
		 * filesystem.  ext2fs_allocate_group_table() will try
		 * to reserve the inode table in the desired flex_bg
		 * location.  However, passing rfs->reserve_blocks
		 * doesn't work since it only has reserved the blocks
		 * that will be used in the new block group -- and
		 * with flex_bg, we can and will allocate the tables
		 * outside of the block group.  And we can't pass in
		 * the fs->block_map because it doesn't handle
		 * overlapping inode table movements right.  So for
		 * now, we temporarily disable flex_bg to force
		 * ext2fs_allocate_group_tables() to allocate the bg
		 * metadata in side the block group, and the restore
		 * it afterwards.  Ugly, until we can fix this up
		 * right later.
		 */
		save_incompat_flag = fs->super->s_feature_incompat;
		fs->super->s_feature_incompat &= ~EXT4_FEATURE_INCOMPAT_FLEX_BG;
		retval = ext2fs_allocate_group_table(fs, i,
						     rfs->reserve_blocks);
		fs->super->s_feature_incompat = save_incompat_flag;
		if (retval)
			goto errout;

		/*
		 * For those structures that have changed, we need to
		 * do bookkeepping.
		 */
		if (FS_BLOCK_BM(old_fs, i) !=
		    (blk = FS_BLOCK_BM(fs, i))) {
			ext2fs_block_alloc_stats(fs, blk, +1);
			if (ext2fs_test_block_bitmap(old_fs->block_map, blk) &&
			    !ext2fs_test_block_bitmap(meta_bmap, blk))
				ext2fs_mark_block_bitmap(rfs->move_blocks,
							 blk);
		}
		if (FS_INODE_BM(old_fs, i) !=
		    (blk = FS_INODE_BM(fs, i))) {
			ext2fs_block_alloc_stats(fs, blk, +1);
			if (ext2fs_test_block_bitmap(old_fs->block_map, blk) &&
			    !ext2fs_test_block_bitmap(meta_bmap, blk))
				ext2fs_mark_block_bitmap(rfs->move_blocks,
							 blk);
		}

		/*
		 * The inode table, if we need to relocate it, is
		 * handled specially.  We have to reserve the blocks
		 * for both the old and the new inode table, since we
		 * can't have the inode table be destroyed during the
		 * block relocation phase.
		 */
		if (FS_INODE_TB(fs, i) == FS_INODE_TB(old_fs, i))
			goto next_group; /* inode table not moved */

		rfs->needed_blocks += fs->inode_blocks_per_group;

		/*
		 * Mark the new inode table as in use in the new block
		 * allocation bitmap, and move any blocks that might
		 * be necessary.
		 */
		for (blk = fs->group_desc[i].bg_inode_table, j=0;
		     j < fs->inode_blocks_per_group ; j++, blk++) {
			ext2fs_block_alloc_stats(fs, blk, +1);
			if (ext2fs_test_block_bitmap(old_fs->block_map, blk) &&
			    !ext2fs_test_block_bitmap(meta_bmap, blk))
				ext2fs_mark_block_bitmap(rfs->move_blocks,
							 blk);
		}

		/*
		 * Make sure the old inode table is reserved in the
		 * block reservation bitmap.
		 */
		for (blk = rfs->old_fs->group_desc[i].bg_inode_table, j=0;
		     j < fs->inode_blocks_per_group ; j++, blk++)
			ext2fs_mark_block_bitmap(rfs->reserve_blocks, blk);

	next_group:
		group_blk += rfs->new_fs->super->s_blocks_per_group;
	}
	retval = 0;

errout:
	if (meta_bmap)
		ext2fs_free_block_bitmap(meta_bmap);

	return retval;
}

/*
 * This helper function tries to allocate a new block.  We try to
 * avoid hitting the original group descriptor blocks at least at
 * first, since we want to make it possible to recover from a badly
 * aborted resize operation as much as possible.
 *
 * In the future, I may further modify this routine to balance out
 * where we get the new blocks across the various block groups.
 * Ideally we would allocate blocks that corresponded with the block
 * group of the containing inode, and keep contiguous blocks
 * together.  However, this very difficult to do efficiently, since we
 * don't have the necessary information up front.
 */

#define AVOID_OLD	1
#define DESPERATION	2

static void init_block_alloc(ext2_resize_t rfs)
{
	rfs->alloc_state = AVOID_OLD;
	rfs->new_blk = rfs->new_fs->super->s_first_data_block;
#if 0
	/* HACK for testing */
	if (rfs->new_fs->super->s_blocks_count >
	    rfs->old_fs->super->s_blocks_count)
		rfs->new_blk = rfs->old_fs->super->s_blocks_count;
#endif
}

static blk_t get_new_block(ext2_resize_t rfs)
{
	ext2_filsys	fs = rfs->new_fs;

	while (1) {
		if (rfs->new_blk >= fs->super->s_blocks_count) {
			if (rfs->alloc_state == DESPERATION)
				return 0;

#ifdef RESIZE2FS_DEBUG
			if (rfs->flags & RESIZE_DEBUG_BMOVE)
				printf("Going into desperation mode "
				       "for block allocations\n");
#endif
			rfs->alloc_state = DESPERATION;
			rfs->new_blk = fs->super->s_first_data_block;
			continue;
		}
		if (ext2fs_test_block_bitmap(fs->block_map, rfs->new_blk) ||
		    ext2fs_test_block_bitmap(rfs->reserve_blocks,
					     rfs->new_blk) ||
		    ((rfs->alloc_state == AVOID_OLD) &&
		     (rfs->new_blk < rfs->old_fs->super->s_blocks_count) &&
		     ext2fs_test_block_bitmap(rfs->old_fs->block_map,
					      rfs->new_blk))) {
			rfs->new_blk++;
			continue;
		}
		return rfs->new_blk;
	}
}

static errcode_t resize2fs_get_alloc_block(ext2_filsys fs, blk64_t goal,
					   blk64_t *ret)
{
	ext2_resize_t rfs = (ext2_resize_t) fs->priv_data;
	blk_t blk;

	blk = get_new_block(rfs);
	if (!blk)
		return ENOSPC;

#ifdef RESIZE2FS_DEBUG
	if (rfs->flags & 0xF)
		printf("get_alloc_block allocating %u\n", blk);
#endif

	ext2fs_mark_block_bitmap(rfs->old_fs->block_map, blk);
	ext2fs_mark_block_bitmap(rfs->new_fs->block_map, blk);
	*ret = (blk64_t) blk;
	return 0;
}

static errcode_t block_mover(ext2_resize_t rfs)
{
	blk_t			blk, old_blk, new_blk;
	ext2_filsys		fs = rfs->new_fs;
	ext2_filsys		old_fs = rfs->old_fs;
	errcode_t		retval;
	int			size, c;
	int			to_move, moved;
	ext2_badblocks_list	badblock_list = 0;
	int			bb_modified = 0;

	fs->get_alloc_block = resize2fs_get_alloc_block;
	old_fs->get_alloc_block = resize2fs_get_alloc_block;

	retval = ext2fs_read_bb_inode(old_fs, &badblock_list);
	if (retval)
		return retval;

	new_blk = fs->super->s_first_data_block;
	if (!rfs->itable_buf) {
		retval = ext2fs_get_array(fs->blocksize,
					fs->inode_blocks_per_group,
					&rfs->itable_buf);
		if (retval)
			return retval;
	}
	retval = ext2fs_create_extent_table(&rfs->bmap, 0);
	if (retval)
		return retval;

	/*
	 * The first step is to figure out where all of the blocks
	 * will go.
	 */
	to_move = moved = 0;
	init_block_alloc(rfs);
	for (blk = old_fs->super->s_first_data_block;
	     blk < old_fs->super->s_blocks_count; blk++) {
		if (!ext2fs_test_block_bitmap(old_fs->block_map, blk))
			continue;
		if (!ext2fs_test_block_bitmap(rfs->move_blocks, blk))
			continue;
		if (ext2fs_badblocks_list_test(badblock_list, blk)) {
			ext2fs_badblocks_list_del(badblock_list, blk);
			bb_modified++;
			continue;
		}

		new_blk = get_new_block(rfs);
		if (!new_blk) {
			retval = ENOSPC;
			goto errout;
		}
		ext2fs_block_alloc_stats(fs, new_blk, +1);
		ext2fs_add_extent_entry(rfs->bmap, blk, new_blk);
		to_move++;
	}

	if (to_move == 0) {
		if (rfs->bmap) {
			ext2fs_free_extent_table(rfs->bmap);
			rfs->bmap = 0;
		}
		retval = 0;
		goto errout;
	}

	/*
	 * Step two is to actually move the blocks
	 */
	retval =  ext2fs_iterate_extent(rfs->bmap, 0, 0, 0);
	if (retval) goto errout;

	if (rfs->progress) {
		retval = (rfs->progress)(rfs, E2_RSZ_BLOCK_RELOC_PASS,
					 0, to_move);
		if (retval)
			goto errout;
	}
	while (1) {
		retval = ext2fs_iterate_extent(rfs->bmap, &old_blk, &new_blk, &size);
		if (retval) goto errout;
		if (!size)
			break;
#ifdef RESIZE2FS_DEBUG
		if (rfs->flags & RESIZE_DEBUG_BMOVE)
			printf("Moving %d blocks %u->%u\n",
			       size, old_blk, new_blk);
#endif
		do {
			c = size;
			if (c > fs->inode_blocks_per_group)
				c = fs->inode_blocks_per_group;
			retval = io_channel_read_blk(fs->io, old_blk, c,
						     rfs->itable_buf);
			if (retval) goto errout;
			retval = io_channel_write_blk(fs->io, new_blk, c,
						      rfs->itable_buf);
			if (retval) goto errout;
			size -= c;
			new_blk += c;
			old_blk += c;
			moved += c;
			if (rfs->progress) {
				io_channel_flush(fs->io);
				retval = (rfs->progress)(rfs,
						E2_RSZ_BLOCK_RELOC_PASS,
						moved, to_move);
				if (retval)
					goto errout;
			}
		} while (size > 0);
		io_channel_flush(fs->io);
	}

errout:
	if (badblock_list) {
		if (!retval && bb_modified)
			retval = ext2fs_update_bb_inode(old_fs,
							badblock_list);
		ext2fs_badblocks_list_free(badblock_list);
	}
	return retval;
}


/* --------------------------------------------------------------------
 *
 * Resize processing, phase 3
 *
 * --------------------------------------------------------------------
 */


struct process_block_struct {
	ext2_resize_t 		rfs;
	ext2_ino_t		ino;
	struct ext2_inode *	inode;
	errcode_t		error;
	int			is_dir;
	int			changed;
};

static int process_block(ext2_filsys fs, blk_t	*block_nr,
			 e2_blkcnt_t blockcnt,
			 blk_t ref_block EXT2FS_ATTR((unused)),
			 int ref_offset EXT2FS_ATTR((unused)), void *priv_data)
{
	struct process_block_struct *pb;
	errcode_t	retval;
	blk_t		block, new_block;
	int		ret = 0;

	pb = (struct process_block_struct *) priv_data;
	block = *block_nr;
	if (pb->rfs->bmap) {
		new_block = ext2fs_extent_translate(pb->rfs->bmap, block);
		if (new_block) {
			*block_nr = new_block;
			ret |= BLOCK_CHANGED;
			pb->changed = 1;
#ifdef RESIZE2FS_DEBUG
			if (pb->rfs->flags & RESIZE_DEBUG_BMOVE)
				printf("ino=%u, blockcnt=%lld, %u->%u\n",
				       pb->ino, blockcnt, block, new_block);
#endif
			block = new_block;
		}
	}
	if (pb->is_dir) {
		retval = ext2fs_add_dir_block(fs->dblist, pb->ino,
					      block, (int) blockcnt);
		if (retval) {
			pb->error = retval;
			ret |= BLOCK_ABORT;
		}
	}
	return ret;
}

/*
 * Progress callback
 */
static errcode_t progress_callback(ext2_filsys fs,
				   ext2_inode_scan scan EXT2FS_ATTR((unused)),
				   dgrp_t group, void * priv_data)
{
	ext2_resize_t rfs = (ext2_resize_t) priv_data;
	errcode_t		retval;

	/*
	 * This check is to protect against old ext2 libraries.  It
	 * shouldn't be needed against new libraries.
	 */
	if ((group+1) == 0)
		return 0;

	if (rfs->progress) {
		io_channel_flush(fs->io);
		retval = (rfs->progress)(rfs, E2_RSZ_INODE_SCAN_PASS,
					 group+1, fs->group_desc_count);
		if (retval)
			return retval;
	}

	return 0;
}

static errcode_t inode_scan_and_fix(ext2_resize_t rfs)
{
	struct process_block_struct	pb;
	ext2_ino_t		ino, new_inode;
	struct ext2_inode 	*inode = NULL;
	ext2_inode_scan 	scan = NULL;
	errcode_t		retval;
	char			*block_buf = 0;
	ext2_ino_t		start_to_move;
	blk_t			orig_size, new_block;
	int			inode_size;

	if ((rfs->old_fs->group_desc_count <=
	     rfs->new_fs->group_desc_count) &&
	    !rfs->bmap)
		return 0;

	/*
	 * Save the original size of the old filesystem, and
	 * temporarily set the size to be the new size if the new size
	 * is larger.  We need to do this to avoid catching an error
	 * by the block iterator routines
	 */
	orig_size = rfs->old_fs->super->s_blocks_count;
	if (orig_size < rfs->new_fs->super->s_blocks_count)
		rfs->old_fs->super->s_blocks_count =
			rfs->new_fs->super->s_blocks_count;

	retval = ext2fs_open_inode_scan(rfs->old_fs, 0, &scan);
	if (retval) goto errout;

	retval = ext2fs_init_dblist(rfs->old_fs, 0);
	if (retval) goto errout;
	retval = ext2fs_get_array(rfs->old_fs->blocksize, 3, &block_buf);
	if (retval) goto errout;

	start_to_move = (rfs->new_fs->group_desc_count *
			 rfs->new_fs->super->s_inodes_per_group);

	if (rfs->progress) {
		retval = (rfs->progress)(rfs, E2_RSZ_INODE_SCAN_PASS,
					 0, rfs->old_fs->group_desc_count);
		if (retval)
			goto errout;
	}
	ext2fs_set_inode_callback(scan, progress_callback, (void *) rfs);
	pb.rfs = rfs;
	pb.inode = inode;
	pb.error = 0;
	new_inode = EXT2_FIRST_INODE(rfs->new_fs->super);
	inode_size = EXT2_INODE_SIZE(rfs->new_fs->super);
	inode = malloc(inode_size);
	if (!inode) {
		retval = ENOMEM;
		goto errout;
	}
	/*
	 * First, copy all of the inodes that need to be moved
	 * elsewhere in the inode table
	 */
	while (1) {
		retval = ext2fs_get_next_inode_full(scan, &ino, inode, inode_size);
		if (retval) goto errout;
		if (!ino)
			break;

		if (inode->i_links_count == 0 && ino != EXT2_RESIZE_INO)
			continue; /* inode not in use */

		pb.is_dir = LINUX_S_ISDIR(inode->i_mode);
		pb.changed = 0;

		if (inode->i_file_acl && rfs->bmap) {
			new_block = ext2fs_extent_translate(rfs->bmap,
							    inode->i_file_acl);
			if (new_block) {
				inode->i_file_acl = new_block;
				retval = ext2fs_write_inode_full(rfs->old_fs,
							    ino, inode, inode_size);
				if (retval) goto errout;
			}
		}

		if (ext2fs_inode_has_valid_blocks(inode) &&
		    (rfs->bmap || pb.is_dir)) {
			pb.ino = ino;
			retval = ext2fs_block_iterate2(rfs->old_fs,
						       ino, 0, block_buf,
						       process_block, &pb);
			if (retval)
				goto errout;
			if (pb.error) {
				retval = pb.error;
				goto errout;
			}
		}

		if (ino <= start_to_move)
			continue; /* Don't need to move it. */

		/*
		 * Find a new inode
		 */
		retval = ext2fs_new_inode(rfs->new_fs, 0, 0, 0, &new_inode);
		if (retval)
			goto errout;

		ext2fs_inode_alloc_stats2(rfs->new_fs, new_inode, +1,
					  pb.is_dir);
		if (pb.changed) {
			/* Get the new version of the inode */
			retval = ext2fs_read_inode_full(rfs->old_fs, ino,
						inode, inode_size);
			if (retval) goto errout;
		}
		inode->i_ctime = time(0);
		retval = ext2fs_write_inode_full(rfs->old_fs, new_inode,
						inode, inode_size);
		if (retval) goto errout;

#ifdef RESIZE2FS_DEBUG
		if (rfs->flags & RESIZE_DEBUG_INODEMAP)
			printf("Inode moved %u->%u\n", ino, new_inode);
#endif
		if (!rfs->imap) {
			retval = ext2fs_create_extent_table(&rfs->imap, 0);
			if (retval)
				goto errout;
		}
		ext2fs_add_extent_entry(rfs->imap, ino, new_inode);
	}
	io_channel_flush(rfs->old_fs->io);

errout:
	rfs->old_fs->super->s_blocks_count = orig_size;
	if (rfs->bmap) {
		ext2fs_free_extent_table(rfs->bmap);
		rfs->bmap = 0;
	}
	if (scan)
		ext2fs_close_inode_scan(scan);
	if (block_buf)
		ext2fs_free_mem(&block_buf);
	free(inode);
	return retval;
}

/* --------------------------------------------------------------------
 *
 * Resize processing, phase 4.
 *
 * --------------------------------------------------------------------
 */

struct istruct {
	ext2_resize_t rfs;
	errcode_t	err;
	unsigned int	max_dirs;
	unsigned int	num;
};

static int check_and_change_inodes(ext2_ino_t dir,
				   int entry EXT2FS_ATTR((unused)),
				   struct ext2_dir_entry *dirent, int offset,
				   int	blocksize EXT2FS_ATTR((unused)),
				   char *buf EXT2FS_ATTR((unused)),
				   void *priv_data)
{
	struct istruct *is = (struct istruct *) priv_data;
	struct ext2_inode 	inode;
	ext2_ino_t		new_inode;
	errcode_t		retval;

	if (is->rfs->progress && offset == 0) {
		io_channel_flush(is->rfs->old_fs->io);
		is->err = (is->rfs->progress)(is->rfs,
					      E2_RSZ_INODE_REF_UPD_PASS,
					      ++is->num, is->max_dirs);
		if (is->err)
			return DIRENT_ABORT;
	}

	if (!dirent->inode)
		return 0;

	new_inode = ext2fs_extent_translate(is->rfs->imap, dirent->inode);

	if (!new_inode)
		return 0;
#ifdef RESIZE2FS_DEBUG
	if (is->rfs->flags & RESIZE_DEBUG_INODEMAP)
		printf("Inode translate (dir=%u, name=%.*s, %u->%u)\n",
		       dir, dirent->name_len&0xFF, dirent->name,
		       dirent->inode, new_inode);
#endif

	dirent->inode = new_inode;

	/* Update the directory mtime and ctime */
	retval = ext2fs_read_inode(is->rfs->old_fs, dir, &inode);
	if (retval == 0) {
		inode.i_mtime = inode.i_ctime = time(0);
		is->err = ext2fs_write_inode(is->rfs->old_fs, dir, &inode);
		if (is->err)
			return DIRENT_ABORT;
	}

	return DIRENT_CHANGED;
}

static errcode_t inode_ref_fix(ext2_resize_t rfs)
{
	errcode_t		retval;
	struct istruct 		is;

	if (!rfs->imap)
		return 0;

	/*
	 * Now, we iterate over all of the directories to update the
	 * inode references
	 */
	is.num = 0;
	is.max_dirs = ext2fs_dblist_count(rfs->old_fs->dblist);
	is.rfs = rfs;
	is.err = 0;

	if (rfs->progress) {
		retval = (rfs->progress)(rfs, E2_RSZ_INODE_REF_UPD_PASS,
					 0, is.max_dirs);
		if (retval)
			goto errout;
	}

	retval = ext2fs_dblist_dir_iterate(rfs->old_fs->dblist,
					   DIRENT_FLAG_INCLUDE_EMPTY, 0,
					   check_and_change_inodes, &is);
	if (retval)
		goto errout;
	if (is.err) {
		retval = is.err;
		goto errout;
	}

	if (rfs->progress && (is.num < is.max_dirs))
		(rfs->progress)(rfs, E2_RSZ_INODE_REF_UPD_PASS,
				is.max_dirs, is.max_dirs);

errout:
	ext2fs_free_extent_table(rfs->imap);
	rfs->imap = 0;
	return retval;
}


/* --------------------------------------------------------------------
 *
 * Resize processing, phase 5.
 *
 * In this phase we actually move the inode table around, and then
 * update the summary statistics.  This is scary, since aborting here
 * will potentially scramble the filesystem.  (We are moving the
 * inode tables around in place, and so the potential for lost data,
 * or at the very least scrambling the mapping between filenames and
 * inode numbers is very high in case of a power failure here.)
 * --------------------------------------------------------------------
 */


/*
 * A very scary routine --- this one moves the inode table around!!!
 *
 * After this you have to use the rfs->new_fs file handle to read and
 * write inodes.
 */
static errcode_t move_itables(ext2_resize_t rfs)
{
	int		n, num, size, diff;
	dgrp_t		i, max_groups;
	ext2_filsys	fs = rfs->new_fs;
	char		*cp;
	blk_t		old_blk, new_blk, blk;
	errcode_t	retval;
	int		j, to_move, moved;

	max_groups = fs->group_desc_count;
	if (max_groups > rfs->old_fs->group_desc_count)
		max_groups = rfs->old_fs->group_desc_count;

	size = fs->blocksize * fs->inode_blocks_per_group;
	if (!rfs->itable_buf) {
		retval = ext2fs_get_mem(size, &rfs->itable_buf);
		if (retval)
			return retval;
	}

	/*
	 * Figure out how many inode tables we need to move
	 */
	to_move = moved = 0;
	for (i=0; i < max_groups; i++)
		if (rfs->old_fs->group_desc[i].bg_inode_table !=
		    fs->group_desc[i].bg_inode_table)
			to_move++;

	if (to_move == 0)
		return 0;

	if (rfs->progress) {
		retval = rfs->progress(rfs, E2_RSZ_MOVE_ITABLE_PASS,
				       0, to_move);
		if (retval)
			goto errout;
	}

	rfs->old_fs->flags |= EXT2_FLAG_MASTER_SB_ONLY;

	for (i=0; i < max_groups; i++) {
		old_blk = rfs->old_fs->group_desc[i].bg_inode_table;
		new_blk = fs->group_desc[i].bg_inode_table;
		diff = new_blk - old_blk;

#ifdef RESIZE2FS_DEBUG
		if (rfs->flags & RESIZE_DEBUG_ITABLEMOVE)
			printf("Itable move group %d block %u->%u (diff %d)\n",
			       i, old_blk, new_blk, diff);
#endif

		if (!diff)
			continue;

		retval = io_channel_read_blk(fs->io, old_blk,
					     fs->inode_blocks_per_group,
					     rfs->itable_buf);
		if (retval)
			goto errout;
		/*
		 * The end of the inode table segment often contains
		 * all zeros, and we're often only moving the inode
		 * table down a block or two.  If so, we can optimize
		 * things by not rewriting blocks that we know to be zero
		 * already.
		 */
		for (cp = rfs->itable_buf+size-1, n=0; n < size; n++, cp--)
			if (*cp)
				break;
		n = n >> EXT2_BLOCK_SIZE_BITS(fs->super);
#ifdef RESIZE2FS_DEBUG
		if (rfs->flags & RESIZE_DEBUG_ITABLEMOVE)
			printf("%d blocks of zeros...\n", n);
#endif
		num = fs->inode_blocks_per_group;
		if (n > diff)
			num -= n;

		retval = io_channel_write_blk(fs->io, new_blk,
					      num, rfs->itable_buf);
		if (retval) {
			io_channel_write_blk(fs->io, old_blk,
					     num, rfs->itable_buf);
			goto errout;
		}
		if (n > diff) {
			retval = io_channel_write_blk(fs->io,
			      old_blk + fs->inode_blocks_per_group,
			      diff, (rfs->itable_buf +
				     (fs->inode_blocks_per_group - diff) *
				     fs->blocksize));
			if (retval)
				goto errout;
		}

		for (blk = rfs->old_fs->group_desc[i].bg_inode_table, j=0;
		     j < fs->inode_blocks_per_group ; j++, blk++)
			ext2fs_block_alloc_stats(fs, blk, -1);

		rfs->old_fs->group_desc[i].bg_inode_table = new_blk;
		ext2fs_group_desc_csum_set(rfs->old_fs, i);
		ext2fs_mark_super_dirty(rfs->old_fs);
		ext2fs_flush(rfs->old_fs);

		if (rfs->progress) {
			retval = rfs->progress(rfs, E2_RSZ_MOVE_ITABLE_PASS,
					       ++moved, to_move);
			if (retval)
				goto errout;
		}
	}
	mark_table_blocks(fs, fs->block_map);
	ext2fs_flush(fs);
#ifdef RESIZE2FS_DEBUG
	if (rfs->flags & RESIZE_DEBUG_ITABLEMOVE)
		printf("Inode table move finished.\n");
#endif
	return 0;

errout:
	return retval;
}

/*
 * Fix the resize inode
 */
static errcode_t fix_resize_inode(ext2_filsys fs)
{
	struct ext2_inode	inode;
	errcode_t		retval;
	char *			block_buf;

	if (!(fs->super->s_feature_compat &
	      EXT2_FEATURE_COMPAT_RESIZE_INODE))
		return 0;

	retval = ext2fs_get_mem(fs->blocksize, &block_buf);
	if (retval) goto errout;

	retval = ext2fs_read_inode(fs, EXT2_RESIZE_INO, &inode);
	if (retval) goto errout;

	ext2fs_iblk_set(fs, &inode, 1);

	retval = ext2fs_write_inode(fs, EXT2_RESIZE_INO, &inode);
	if (retval) goto errout;

	if (!inode.i_block[EXT2_DIND_BLOCK]) {
		/*
		 * Avoid zeroing out block #0; that's rude.  This
		 * should never happen anyway since the filesystem
		 * should be fsck'ed and we assume it is consistent.
		 */
		fprintf(stderr,
			_("Should never happen: resize inode corrupt!\n"));
		exit(1);
	}

	memset(block_buf, 0, fs->blocksize);

	retval = io_channel_write_blk(fs->io, inode.i_block[EXT2_DIND_BLOCK],
				      1, block_buf);
	if (retval) goto errout;

	retval = ext2fs_create_resize_inode(fs);
	if (retval)
		goto errout;

errout:
	if (block_buf)
		ext2fs_free_mem(&block_buf);
	return retval;
}

/*
 * Finally, recalculate the summary information
 */
static errcode_t ext2fs_calculate_summary_stats(ext2_filsys fs)
{
	blk_t		blk;
	ext2_ino_t	ino;
	unsigned int	group = 0;
	unsigned int	count = 0;
	int		total_free = 0;
	int		group_free = 0;
	int		uninit = 0;
	blk_t		super_blk, old_desc_blk, new_desc_blk;
	int		old_desc_blocks;

	/*
	 * First calculate the block statistics
	 */
	uninit = fs->group_desc[group].bg_flags & EXT2_BG_BLOCK_UNINIT;
	ext2fs_super_and_bgd_loc(fs, group, &super_blk, &old_desc_blk,
				 &new_desc_blk, 0);
	if (fs->super->s_feature_incompat & EXT2_FEATURE_INCOMPAT_META_BG)
		old_desc_blocks = fs->super->s_first_meta_bg;
	else
		old_desc_blocks = fs->desc_blocks +
			fs->super->s_reserved_gdt_blocks;
	for (blk = fs->super->s_first_data_block;
	     blk < fs->super->s_blocks_count; blk++) {
		if ((uninit &&
		     !((blk == super_blk) ||
		       ((old_desc_blk && old_desc_blocks &&
			 (blk >= old_desc_blk) &&
			 (blk < old_desc_blk + old_desc_blocks))) ||
		       ((new_desc_blk && (blk == new_desc_blk))) ||
		       (blk == fs->group_desc[group].bg_block_bitmap) ||
		       (blk == fs->group_desc[group].bg_inode_bitmap) ||
		       ((blk >= fs->group_desc[group].bg_inode_table &&
			 (blk < fs->group_desc[group].bg_inode_table
			  + fs->inode_blocks_per_group))))) ||
		    (!ext2fs_fast_test_block_bitmap(fs->block_map, blk))) {
			group_free++;
			total_free++;
		}
		count++;
		if ((count == fs->super->s_blocks_per_group) ||
		    (blk == fs->super->s_blocks_count-1)) {
			fs->group_desc[group].bg_free_blocks_count =
				group_free;
			ext2fs_group_desc_csum_set(fs, group);
			group++;
			if (group >= fs->group_desc_count)
				break;
			count = 0;
			group_free = 0;
			uninit = (fs->group_desc[group].bg_flags &
				  EXT2_BG_BLOCK_UNINIT);
			ext2fs_super_and_bgd_loc(fs, group, &super_blk,
						 &old_desc_blk,
						 &new_desc_blk, 0);
			if (fs->super->s_feature_incompat &
			    EXT2_FEATURE_INCOMPAT_META_BG)
				old_desc_blocks = fs->super->s_first_meta_bg;
			else
				old_desc_blocks = fs->desc_blocks +
					fs->super->s_reserved_gdt_blocks;
		}
	}
	fs->super->s_free_blocks_count = total_free;

	/*
	 * Next, calculate the inode statistics
	 */
	group_free = 0;
	total_free = 0;
	count = 0;
	group = 0;

	/* Protect loop from wrap-around if s_inodes_count maxed */
	uninit = fs->group_desc[group].bg_flags & EXT2_BG_INODE_UNINIT;
	for (ino = 1; ino <= fs->super->s_inodes_count && ino > 0; ino++) {
		if (uninit ||
		    !ext2fs_fast_test_inode_bitmap(fs->inode_map, ino)) {
			group_free++;
			total_free++;
		}
		count++;
		if ((count == fs->super->s_inodes_per_group) ||
		    (ino == fs->super->s_inodes_count)) {
			fs->group_desc[group].bg_free_inodes_count =
				group_free;
			ext2fs_group_desc_csum_set(fs, group);
			group++;
			if (group >= fs->group_desc_count)
				break;
			count = 0;
			group_free = 0;
			uninit = (fs->group_desc[group].bg_flags &
				  EXT2_BG_INODE_UNINIT);
		}
	}
	fs->super->s_free_inodes_count = total_free;
	ext2fs_mark_super_dirty(fs);
	return 0;
}

/*
 *  Journal may have been relocated; update the backup journal blocks
 *  in the superblock.
 */
static errcode_t fix_sb_journal_backup(ext2_filsys fs)
{
	errcode_t	  retval;
	struct ext2_inode inode;

	if (!(fs->super->s_feature_compat & EXT3_FEATURE_COMPAT_HAS_JOURNAL))
		return 0;

	/* External journal? Nothing to do. */
	if (fs->super->s_journal_dev && !fs->super->s_journal_inum)
		return 0;

	retval = ext2fs_read_inode(fs, fs->super->s_journal_inum, &inode);
	if (retval)
		return retval;
	memcpy(fs->super->s_jnl_blocks, inode.i_block, EXT2_N_BLOCKS*4);
	fs->super->s_jnl_blocks[16] = inode.i_size;
	fs->super->s_jnl_backup_type = EXT3_JNL_BACKUP_BLOCKS;
	ext2fs_mark_super_dirty(fs);
	return 0;
}

/*
 * calcluate the minimum number of blocks the given fs can be resized to
 */
blk_t calculate_minimum_resize_size(ext2_filsys fs)
{
	blk_t inode_count, blks_needed, groups, data_blocks;
	blk_t grp, data_needed, last_start;
	int overhead = 0, num_of_superblocks = 0;
	int extra_groups = 0;
	int flexbg_size = 1 << fs->super->s_log_groups_per_flex;

	/*
	 * first figure out how many group descriptors we need to
	 * handle the number of inodes we have
	 */
	inode_count = fs->super->s_inodes_count -
		fs->super->s_free_inodes_count;
	blks_needed = ext2fs_div_ceil(inode_count,
				      fs->super->s_inodes_per_group) *
		EXT2_BLOCKS_PER_GROUP(fs->super);
	groups = ext2fs_div_ceil(blks_needed,
				 EXT2_BLOCKS_PER_GROUP(fs->super));

	/*
	 * we need to figure out how many backup superblocks we have so we can
	 * account for that in the metadata
	 */
	for (grp = 0; grp < fs->group_desc_count; grp++) {
		if (ext2fs_bg_has_super(fs, grp))
			num_of_superblocks++;
	}

	/* calculate how many blocks are needed for data */
	data_needed = fs->super->s_blocks_count -
		fs->super->s_free_blocks_count;
	data_needed -= SUPER_OVERHEAD(fs) * num_of_superblocks;
	data_needed -= META_OVERHEAD(fs) * fs->group_desc_count;

	if (fs->super->s_feature_incompat & EXT4_FEATURE_INCOMPAT_FLEX_BG) {
		/*
		 * For ext4 we need to allow for up to a flex_bg worth
		 * of inode tables of slack space so the resize
		 * operation can be guaranteed to finish.
		 */
		extra_groups = flexbg_size - (groups & (flexbg_size - 1));
		data_needed += META_OVERHEAD(fs) * extra_groups;
		extra_groups = groups % flexbg_size;
	}

	/*
	 * figure out how many data blocks we have given the number of groups
	 * we need for our inodes
	 */
	data_blocks = groups * EXT2_BLOCKS_PER_GROUP(fs->super);
	last_start = 0;
	for (grp = 0; grp < groups; grp++) {
		overhead = META_OVERHEAD(fs);

		if (ext2fs_bg_has_super(fs, grp))
			overhead += SUPER_OVERHEAD(fs);

		/*
		 * we want to keep track of how much data we can store in
		 * the groups leading up to the last group so we can determine
		 * how big the last group needs to be
		 */
		if (grp != (groups - 1))
			last_start += EXT2_BLOCKS_PER_GROUP(fs->super) -
				overhead;

		data_blocks -= overhead;
	}

	/*
	 * if we need more group descriptors in order to accomodate our data
	 * then we need to add them here
	 */
	while (data_needed > data_blocks) {
		blk_t remainder = data_needed - data_blocks;
		blk_t extra_grps;

		/* figure out how many more groups we need for the data */
		extra_grps = ext2fs_div_ceil(remainder,
					     EXT2_BLOCKS_PER_GROUP(fs->super));

		data_blocks += extra_grps * EXT2_BLOCKS_PER_GROUP(fs->super);

		/* ok we have to account for the last group */
		overhead = META_OVERHEAD(fs);
		if (ext2fs_bg_has_super(fs, groups-1))
			overhead += SUPER_OVERHEAD(fs);
		last_start += EXT2_BLOCKS_PER_GROUP(fs->super) - overhead;

		for (grp = groups; grp < groups+extra_grps; grp++) {
			overhead = META_OVERHEAD(fs);
			if (ext2fs_bg_has_super(fs, grp))
				overhead += SUPER_OVERHEAD(fs);

			/*
			 * again, we need to see how much data we cram into
			 * all of the groups leading up to the last group
			 */
			if (grp != (groups + extra_grps - 1))
				last_start += EXT2_BLOCKS_PER_GROUP(fs->super)
					- overhead;

			data_blocks -= overhead;
		}

		groups += extra_grps;
		extra_groups += extra_grps;
		if (fs->super->s_feature_incompat
			& EXT4_FEATURE_INCOMPAT_FLEX_BG
		    && extra_groups > flexbg_size) {
			/*
			 * For ext4 we need to allow for up to a flex_bg worth
			 * of inode tables of slack space so the resize
			 * operation can be guaranteed to finish.
			 */
			extra_groups = flexbg_size -
						(groups & (flexbg_size - 1));
			data_needed += META_OVERHEAD(fs) * extra_groups;
			extra_groups = groups % flexbg_size;
		}
	}

	/* now for the fun voodoo */
	overhead = META_OVERHEAD(fs);

	/*
	 * if this is the case then the last group is going to have data in it
	 * so we need to adjust the size of the last group accordingly
	 */
	if (last_start < data_needed) {
		blk_t remainder = data_needed - last_start;

		/*
		 * 50 is a magic number that mkfs/resize uses to see if its
		 * even worth making/resizing the fs.  basically you need to
		 * have at least 50 blocks in addition to the blocks needed
		 * for the metadata in the last group
		 */
		if (remainder > 50)
			overhead += remainder;
		else
			overhead += 50;
	} else
		overhead += 50;

	if (ext2fs_bg_has_super(fs, groups-1))
		overhead += SUPER_OVERHEAD(fs);

	/*
	 * since our last group doesn't have to be BLOCKS_PER_GROUP large, we
	 * only do groups-1, and then add the number of blocks needed to
	 * handle the group descriptor metadata+data that we need
	 */
	blks_needed = (groups-1) * EXT2_BLOCKS_PER_GROUP(fs->super);
	blks_needed += overhead;

	/*
	 * If at this point we've already added up more "needed" than
	 * the current size, just return current size as minimum.
	 */
	if (blks_needed >= fs->super->s_blocks_count)
		return fs->super->s_blocks_count;
	/*
	 * We need to reserve a few extra blocks if extents are
	 * enabled, in case we need to grow the extent tree.  The more
	 * we shrink the file system, the more space we need.
	 */
	if (fs->super->s_feature_incompat & EXT3_FEATURE_INCOMPAT_EXTENTS)
		blks_needed += (fs->super->s_blocks_count - blks_needed)/500;

	return blks_needed;
}
