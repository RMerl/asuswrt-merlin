/*
 * pass5.c --- check block and inode bitmaps against on-disk bitmaps
 *
 * Copyright (C) 1993, 1994, 1995, 1996, 1997 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 *
 */

#include "config.h"
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>

#include "e2fsck.h"
#include "problem.h"

#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))

static void check_block_bitmaps(e2fsck_t ctx);
static void check_inode_bitmaps(e2fsck_t ctx);
static void check_inode_end(e2fsck_t ctx);
static void check_block_end(e2fsck_t ctx);

void e2fsck_pass5(e2fsck_t ctx)
{
#ifdef RESOURCE_TRACK
	struct resource_track	rtrack;
#endif
	struct problem_context	pctx;

#ifdef MTRACE
	mtrace_print("Pass 5");
#endif

	init_resource_track(&rtrack, ctx->fs->io);
	clear_problem_context(&pctx);

	if (!(ctx->options & E2F_OPT_PREEN))
		fix_problem(ctx, PR_5_PASS_HEADER, &pctx);

	if (ctx->progress)
		if ((ctx->progress)(ctx, 5, 0, ctx->fs->group_desc_count*2))
			return;

	e2fsck_read_bitmaps(ctx);

	check_block_bitmaps(ctx);
	if (ctx->flags & E2F_FLAG_SIGNAL_MASK)
		return;
	check_inode_bitmaps(ctx);
	if (ctx->flags & E2F_FLAG_SIGNAL_MASK)
		return;
	check_inode_end(ctx);
	if (ctx->flags & E2F_FLAG_SIGNAL_MASK)
		return;
	check_block_end(ctx);
	if (ctx->flags & E2F_FLAG_SIGNAL_MASK)
		return;

	ext2fs_free_inode_bitmap(ctx->inode_used_map);
	ctx->inode_used_map = 0;
	ext2fs_free_inode_bitmap(ctx->inode_dir_map);
	ctx->inode_dir_map = 0;
	ext2fs_free_block_bitmap(ctx->block_found_map);
	ctx->block_found_map = 0;

	print_resource_track(ctx, _("Pass 5"), &rtrack, ctx->fs->io);
}

static void e2fsck_discard_blocks(e2fsck_t ctx, blk64_t start,
				  blk64_t count)
{
	ext2_filsys fs = ctx->fs;

	/*
	 * If the filesystem has changed it means that there was an corruption
	 * which should be repaired, but in some cases just one e2fsck run is
	 * not enough to fix the problem, hence it is not safe to run discard
	 * in this case.
	 */
	if (ext2fs_test_changed(fs))
		ctx->options &= ~E2F_OPT_DISCARD;

	if ((ctx->options & E2F_OPT_DISCARD) &&
	    (io_channel_discard(fs->io, start, count)))
		ctx->options &= ~E2F_OPT_DISCARD;
}

/*
 * This will try to discard number 'count' inodes starting at
 * inode number 'start' within the 'group'. Note that 'start'
 * is 1-based, it means that we need to adjust it by -1 in this
 * function to compute right offset in the particular inode table.
 */
static void e2fsck_discard_inodes(e2fsck_t ctx, dgrp_t group,
				  ext2_ino_t start, int count)
{
	ext2_filsys fs = ctx->fs;
	blk64_t blk, num;

	/*
	 * Sanity check for 'start'
	 */
	if ((start < 1) || (start > EXT2_INODES_PER_GROUP(fs->super))) {
		printf("PROGRAMMING ERROR: Got start %d outside of group %d!"
		       " Disabling discard\n",
			start, group);
		ctx->options &= ~E2F_OPT_DISCARD;
	}

	/*
	 * Do not attempt to discard if E2F_OPT_DISCARD is not set. And also
	 * skip the discard on this group if discard does not zero data.
	 * The reason is that if the inode table is not zeroed discard would
	 * no help us since we need to zero it anyway, or if the inode table
	 * is zeroed then the read after discard would not be deterministic
	 * anyway and we would not be able to assume that this inode table
	 * was zeroed anymore so we would have to zero it again, which does
	 * not really make sense.
	 */
	if (!(ctx->options & E2F_OPT_DISCARD) ||
	    !io_channel_discard_zeroes_data(fs->io))
		return;

	/*
	 * Start is inode number within the group which starts
	 * counting from 1, so we need to adjust it.
	 */
	start -= 1;

	/*
	 * We can discard only blocks containing only unused
	 * inodes in the table.
	 */
	blk = DIV_ROUND_UP(start,
		EXT2_INODES_PER_BLOCK(fs->super));
	count -= (blk * EXT2_INODES_PER_BLOCK(fs->super) - start);
	blk += ext2fs_inode_table_loc(fs, group);
	num = count / EXT2_INODES_PER_BLOCK(fs->super);

	if (num > 0)
		e2fsck_discard_blocks(ctx, blk, num);
}

#define NO_BLK ((blk64_t) -1)

static void print_bitmap_problem(e2fsck_t ctx, int problem,
			    struct problem_context *pctx)
{
	switch (problem) {
	case PR_5_BLOCK_UNUSED:
		if (pctx->blk == pctx->blk2)
			pctx->blk2 = 0;
		else
			problem = PR_5_BLOCK_RANGE_UNUSED;
		break;
	case PR_5_BLOCK_USED:
		if (pctx->blk == pctx->blk2)
			pctx->blk2 = 0;
		else
			problem = PR_5_BLOCK_RANGE_USED;
		break;
	case PR_5_INODE_UNUSED:
		if (pctx->ino == pctx->ino2)
			pctx->ino2 = 0;
		else
			problem = PR_5_INODE_RANGE_UNUSED;
		break;
	case PR_5_INODE_USED:
		if (pctx->ino == pctx->ino2)
			pctx->ino2 = 0;
		else
			problem = PR_5_INODE_RANGE_USED;
		break;
	}
	fix_problem(ctx, problem, pctx);
	pctx->blk = pctx->blk2 = NO_BLK;
	pctx->ino = pctx->ino2 = 0;
}

/* Just to be more succint */
#define B2C(x)	EXT2FS_B2C(fs, (x))
#define EQ_CLSTR(x, y) (B2C(x) == B2C(y))
#define LE_CLSTR(x, y) (B2C(x) <= B2C(y))
#define GE_CLSTR(x, y) (B2C(x) >= B2C(y))

static void check_block_bitmaps(e2fsck_t ctx)
{
	ext2_filsys fs = ctx->fs;
	blk64_t	i;
	unsigned int	*free_array;
	int	group = 0;
	unsigned int	blocks = 0;
	blk64_t	free_blocks = 0;
	blk64_t first_free = ext2fs_blocks_count(fs->super);
	unsigned int	group_free = 0;
	int	actual, bitmap;
	struct problem_context	pctx;
	int	problem, save_problem, fixit, had_problem;
	errcode_t	retval;
	int		csum_flag;
	int		skip_group = 0;
	int	old_desc_blocks = 0;
	int	count = 0;
	int	cmp_block = 0;
	int	redo_flag = 0;
	blk64_t	super_blk, old_desc_blk, new_desc_blk;
	char *actual_buf, *bitmap_buf;

	actual_buf = (char *) e2fsck_allocate_memory(ctx, fs->blocksize,
						     "actual bitmap buffer");
	bitmap_buf = (char *) e2fsck_allocate_memory(ctx, fs->blocksize,
						     "bitmap block buffer");

	clear_problem_context(&pctx);
	free_array = (unsigned int *) e2fsck_allocate_memory(ctx,
	    fs->group_desc_count * sizeof(unsigned int), "free block count array");

	if ((B2C(fs->super->s_first_data_block) <
	     ext2fs_get_block_bitmap_start2(ctx->block_found_map)) ||
	    (B2C(ext2fs_blocks_count(fs->super)-1) >
	     ext2fs_get_block_bitmap_end2(ctx->block_found_map))) {
		pctx.num = 1;
		pctx.blk = B2C(fs->super->s_first_data_block);
		pctx.blk2 = B2C(ext2fs_blocks_count(fs->super) - 1);
		pctx.ino = ext2fs_get_block_bitmap_start2(ctx->block_found_map);
		pctx.ino2 = ext2fs_get_block_bitmap_end2(ctx->block_found_map);
		fix_problem(ctx, PR_5_BMAP_ENDPOINTS, &pctx);

		ctx->flags |= E2F_FLAG_ABORT; /* fatal */
		goto errout;
	}

	if ((B2C(fs->super->s_first_data_block) <
	     ext2fs_get_block_bitmap_start2(fs->block_map)) ||
	    (B2C(ext2fs_blocks_count(fs->super)-1) >
	     ext2fs_get_block_bitmap_end2(fs->block_map))) {
		pctx.num = 2;
		pctx.blk = B2C(fs->super->s_first_data_block);
		pctx.blk2 = B2C(ext2fs_blocks_count(fs->super) - 1);
		pctx.ino = ext2fs_get_block_bitmap_start2(fs->block_map);
		pctx.ino2 = ext2fs_get_block_bitmap_end2(fs->block_map);
		fix_problem(ctx, PR_5_BMAP_ENDPOINTS, &pctx);

		ctx->flags |= E2F_FLAG_ABORT; /* fatal */
		goto errout;
	}

	csum_flag = EXT2_HAS_RO_COMPAT_FEATURE(fs->super,
					       EXT4_FEATURE_RO_COMPAT_GDT_CSUM);
redo_counts:
	had_problem = 0;
	save_problem = 0;
	pctx.blk = pctx.blk2 = NO_BLK;
	if (csum_flag &&
	    (ext2fs_bg_flags_test(fs, group, EXT2_BG_BLOCK_UNINIT)))
		skip_group++;
	for (i = B2C(fs->super->s_first_data_block);
	     i < ext2fs_blocks_count(fs->super);
	     i += EXT2FS_CLUSTER_RATIO(fs)) {
		int first_block_in_bg = (B2C(i) -
					 B2C(fs->super->s_first_data_block)) %
			fs->super->s_clusters_per_group == 0;
		int n, nbytes = fs->super->s_clusters_per_group / 8;

		actual = ext2fs_fast_test_block_bitmap2(ctx->block_found_map, i);

		/*
		 * Try to optimize pass5 by extracting a bitmap block
		 * as expected from what we have on disk, and then
		 * comparing the two.  If they are identical, then
		 * update the free block counts and go on to the next
		 * block group.  This is much faster than doing the
		 * individual bit-by-bit comparison.  The one downside
		 * is that this doesn't work if we are asking e2fsck
		 * to do a discard operation.
		 */
		if (!first_block_in_bg ||
		    (group == (int)fs->group_desc_count - 1) ||
		    (ctx->options & E2F_OPT_DISCARD))
			goto no_optimize;

		retval = ext2fs_get_block_bitmap_range2(ctx->block_found_map,
				B2C(i), fs->super->s_clusters_per_group,
				actual_buf);
		if (retval)
			goto no_optimize;
		if (ext2fs_bg_flags_test(fs, group, EXT2_BG_BLOCK_UNINIT))
			memset(bitmap_buf, 0, nbytes);
		else {
			retval = ext2fs_get_block_bitmap_range2(fs->block_map,
					B2C(i), fs->super->s_clusters_per_group,
					bitmap_buf);
			if (retval)
				goto no_optimize;
		}
		if (memcmp(actual_buf, bitmap_buf, nbytes) != 0)
			goto no_optimize;
		n = ext2fs_bitcount(actual_buf, nbytes);
		group_free = fs->super->s_clusters_per_group - n;
		free_blocks += group_free;
		i += EXT2FS_C2B(fs, fs->super->s_clusters_per_group - 1);
		goto next_group;
	no_optimize:

		if (skip_group) {
			if (first_block_in_bg) {
				super_blk = 0;
				old_desc_blk = 0;
				new_desc_blk = 0;
				ext2fs_super_and_bgd_loc2(fs, group, &super_blk,
					 &old_desc_blk, &new_desc_blk, 0);

				if (fs->super->s_feature_incompat &
						EXT2_FEATURE_INCOMPAT_META_BG)
					old_desc_blocks =
						fs->super->s_first_meta_bg;
				else
					old_desc_blocks = fs->desc_blocks +
					fs->super->s_reserved_gdt_blocks;

				count = 0;
				cmp_block = fs->super->s_clusters_per_group;
				if (group == (int)fs->group_desc_count - 1)
					cmp_block = EXT2FS_NUM_B2C(fs,
						    ext2fs_group_blocks_count(fs, group));
			}

			bitmap = 0;
			if (EQ_CLSTR(i, super_blk) ||
			    (old_desc_blk && old_desc_blocks &&
			     GE_CLSTR(i, old_desc_blk) &&
			     LE_CLSTR(i, old_desc_blk + old_desc_blocks-1)) ||
			    (new_desc_blk && EQ_CLSTR(i, new_desc_blk)) ||
			    EQ_CLSTR(i, ext2fs_block_bitmap_loc(fs, group)) ||
			    EQ_CLSTR(i, ext2fs_inode_bitmap_loc(fs, group)) ||
			    (GE_CLSTR(i, ext2fs_inode_table_loc(fs, group)) &&
			     LE_CLSTR(i, (ext2fs_inode_table_loc(fs, group) +
					  fs->inode_blocks_per_group - 1)))) {
				bitmap = 1;
				actual = (actual != 0);
				count++;
				cmp_block--;
			} else if ((EXT2FS_B2C(fs, i) - count -
				    EXT2FS_B2C(fs, fs->super->s_first_data_block)) %
				   fs->super->s_clusters_per_group == 0) {
				/*
				 * When the compare data blocks in block bitmap
				 * are 0, count the free block,
				 * skip the current block group.
				 */
				if (ext2fs_test_block_bitmap_range2(
					    ctx->block_found_map,
					    EXT2FS_B2C(fs, i),
					    cmp_block)) {
					/*
					 * -1 means to skip the current block
					 * group.
					 */
					blocks = fs->super->s_clusters_per_group - 1;
					group_free = cmp_block;
					free_blocks += cmp_block;
					/*
					 * The current block group's last block
					 * is set to i.
					 */
					i += EXT2FS_C2B(fs, cmp_block - 1);
					bitmap = 1;
					goto do_counts;
				}
			}
		} else if (redo_flag)
			bitmap = actual;
		else
			bitmap = ext2fs_fast_test_block_bitmap2(fs->block_map, i);

		if (!actual == !bitmap)
			goto do_counts;

		if (!actual && bitmap) {
			/*
			 * Block not used, but marked in use in the bitmap.
			 */
			problem = PR_5_BLOCK_UNUSED;
		} else {
			/*
			 * Block used, but not marked in use in the bitmap.
			 */
			problem = PR_5_BLOCK_USED;

			if (skip_group) {
				struct problem_context pctx2;
				pctx2.blk = i;
				pctx2.group = group;
				if (fix_problem(ctx, PR_5_BLOCK_UNINIT,&pctx2)){
					ext2fs_bg_flags_clear(fs, group, EXT2_BG_BLOCK_UNINIT);
					skip_group = 0;
				}
			}
		}
		if (pctx.blk == NO_BLK) {
			pctx.blk = pctx.blk2 = i;
			save_problem = problem;
		} else {
			if ((problem == save_problem) &&
			    (pctx.blk2 == i-1))
				pctx.blk2++;
			else {
				print_bitmap_problem(ctx, save_problem, &pctx);
				pctx.blk = pctx.blk2 = i;
				save_problem = problem;
			}
		}
		ctx->flags |= E2F_FLAG_PROG_SUPPRESS;
		had_problem++;

		/*
		 * If there a problem we should turn off the discard so we
		 * do not compromise the filesystem.
		 */
		ctx->options &= ~E2F_OPT_DISCARD;

	do_counts:
		if (!bitmap) {
			group_free++;
			free_blocks++;
			if (first_free > i)
				first_free = i;
		} else if (i > first_free) {
			e2fsck_discard_blocks(ctx, first_free,
					      (i - first_free));
			first_free = ext2fs_blocks_count(fs->super);
		}
		blocks ++;
		if ((blocks == fs->super->s_clusters_per_group) ||
		    (EXT2FS_B2C(fs, i) ==
		     EXT2FS_B2C(fs, ext2fs_blocks_count(fs->super)-1))) {
			/*
			 * If the last block of this group is free, then we can
			 * discard it as well.
			 */
			if (!bitmap && i >= first_free)
				e2fsck_discard_blocks(ctx, first_free,
						      (i - first_free) + 1);
		next_group:
			first_free = ext2fs_blocks_count(fs->super);

			free_array[group] = group_free;
			group ++;
			blocks = 0;
			group_free = 0;
			skip_group = 0;
			if (ctx->progress)
				if ((ctx->progress)(ctx, 5, group,
						    fs->group_desc_count*2))
					goto errout;
			if (csum_flag &&
			    (i != ext2fs_blocks_count(fs->super)-1) &&
			    ext2fs_bg_flags_test(fs, group, 
						EXT2_BG_BLOCK_UNINIT))
				skip_group++;
		}
	}
	if (pctx.blk != NO_BLK)
		print_bitmap_problem(ctx, save_problem, &pctx);
	if (had_problem)
		fixit = end_problem_latch(ctx, PR_LATCH_BBITMAP);
	else
		fixit = -1;
	ctx->flags &= ~E2F_FLAG_PROG_SUPPRESS;

	if (fixit == 1) {
		ext2fs_free_block_bitmap(fs->block_map);
		retval = ext2fs_copy_bitmap(ctx->block_found_map,
						  &fs->block_map);
		if (retval) {
			clear_problem_context(&pctx);
			fix_problem(ctx, PR_5_COPY_BBITMAP_ERROR, &pctx);
			ctx->flags |= E2F_FLAG_ABORT;
			goto errout;
		}
		ext2fs_set_bitmap_padding(fs->block_map);
		ext2fs_mark_bb_dirty(fs);

		/* Redo the counts */
		blocks = 0; free_blocks = 0; group_free = 0; group = 0;
		memset(free_array, 0, fs->group_desc_count * sizeof(int));
		redo_flag++;
		goto redo_counts;
	} else if (fixit == 0)
		ext2fs_unmark_valid(fs);

	for (i = 0; i < fs->group_desc_count; i++) {
		if (free_array[i] != ext2fs_bg_free_blocks_count(fs, i)) {
			pctx.group = i;
			pctx.blk = ext2fs_bg_free_blocks_count(fs, i);
			pctx.blk2 = free_array[i];

			if (fix_problem(ctx, PR_5_FREE_BLOCK_COUNT_GROUP,
					&pctx)) {
				ext2fs_bg_free_blocks_count_set(fs, i, free_array[i]);
				ext2fs_mark_super_dirty(fs);
			} else
				ext2fs_unmark_valid(fs);
		}
	}
	free_blocks = EXT2FS_C2B(fs, free_blocks);
	if (free_blocks != ext2fs_free_blocks_count(fs->super)) {
		pctx.group = 0;
		pctx.blk = ext2fs_free_blocks_count(fs->super);
		pctx.blk2 = free_blocks;

		if (fix_problem(ctx, PR_5_FREE_BLOCK_COUNT, &pctx)) {
			ext2fs_free_blocks_count_set(fs->super, free_blocks);
			ext2fs_mark_super_dirty(fs);
		}
	}
errout:
	ext2fs_free_mem(&free_array);
	ext2fs_free_mem(&actual_buf);
	ext2fs_free_mem(&bitmap_buf);
}

static void check_inode_bitmaps(e2fsck_t ctx)
{
	ext2_filsys fs = ctx->fs;
	ext2_ino_t	i;
	unsigned int	free_inodes = 0;
	int		group_free = 0;
	int		dirs_count = 0;
	int		group = 0;
	unsigned int	inodes = 0;
	ext2_ino_t	*free_array;
	ext2_ino_t	*dir_array;
	int		actual, bitmap;
	errcode_t	retval;
	struct problem_context	pctx;
	int		problem, save_problem, fixit, had_problem;
	int		csum_flag;
	int		skip_group = 0;
	int		redo_flag = 0;
	ext2_ino_t		first_free = fs->super->s_inodes_per_group + 1;

	clear_problem_context(&pctx);
	free_array = (ext2_ino_t *) e2fsck_allocate_memory(ctx,
	    fs->group_desc_count * sizeof(ext2_ino_t), "free inode count array");

	dir_array = (ext2_ino_t *) e2fsck_allocate_memory(ctx,
	   fs->group_desc_count * sizeof(ext2_ino_t), "directory count array");

	if ((1 < ext2fs_get_inode_bitmap_start2(ctx->inode_used_map)) ||
	    (fs->super->s_inodes_count >
	     ext2fs_get_inode_bitmap_end2(ctx->inode_used_map))) {
		pctx.num = 3;
		pctx.blk = 1;
		pctx.blk2 = fs->super->s_inodes_count;
		pctx.ino = ext2fs_get_inode_bitmap_start2(ctx->inode_used_map);
		pctx.ino2 = ext2fs_get_inode_bitmap_end2(ctx->inode_used_map);
		fix_problem(ctx, PR_5_BMAP_ENDPOINTS, &pctx);

		ctx->flags |= E2F_FLAG_ABORT; /* fatal */
		goto errout;
	}
	if ((1 < ext2fs_get_inode_bitmap_start2(fs->inode_map)) ||
	    (fs->super->s_inodes_count >
	     ext2fs_get_inode_bitmap_end2(fs->inode_map))) {
		pctx.num = 4;
		pctx.blk = 1;
		pctx.blk2 = fs->super->s_inodes_count;
		pctx.ino = ext2fs_get_inode_bitmap_start2(fs->inode_map);
		pctx.ino2 = ext2fs_get_inode_bitmap_end2(fs->inode_map);
		fix_problem(ctx, PR_5_BMAP_ENDPOINTS, &pctx);

		ctx->flags |= E2F_FLAG_ABORT; /* fatal */
		goto errout;
	}

	csum_flag = EXT2_HAS_RO_COMPAT_FEATURE(fs->super,
					       EXT4_FEATURE_RO_COMPAT_GDT_CSUM);
redo_counts:
	had_problem = 0;
	save_problem = 0;
	pctx.ino = pctx.ino2 = 0;
	if (csum_flag &&
	    (ext2fs_bg_flags_test(fs, group, EXT2_BG_INODE_UNINIT)))
		skip_group++;

	/* Protect loop from wrap-around if inodes_count is maxed */
	for (i = 1; i <= fs->super->s_inodes_count && i > 0; i++) {
		bitmap = 0;
		if (skip_group &&
		    i % fs->super->s_inodes_per_group == 1) {
			/*
			 * Current inode is the first inode
			 * in the current block group.
			 */
			if (ext2fs_test_inode_bitmap_range(
				    ctx->inode_used_map, i,
				    fs->super->s_inodes_per_group)) {
				/*
				 * When the compared inodes in inodes bitmap
				 * are 0, count the free inode,
				 * skip the current block group.
				 */
				first_free = 1;
				inodes = fs->super->s_inodes_per_group - 1;
				group_free = inodes;
				free_inodes += inodes;
				i += inodes;
				skip_group = 0;
				goto do_counts;
			}
		}

		actual = ext2fs_fast_test_inode_bitmap2(ctx->inode_used_map, i);
		if (redo_flag)
			bitmap = actual;
		else if (!skip_group)
			bitmap = ext2fs_fast_test_inode_bitmap2(fs->inode_map, i);
		if (!actual == !bitmap)
			goto do_counts;

		if (!actual && bitmap) {
			/*
			 * Inode wasn't used, but marked in bitmap
			 */
			problem = PR_5_INODE_UNUSED;
		} else /* if (actual && !bitmap) */ {
			/*
			 * Inode used, but not in bitmap
			 */
			problem = PR_5_INODE_USED;

			/* We should never hit this, because it means that
			 * inodes were marked in use that weren't noticed
			 * in pass1 or pass 2. It is easier to fix the problem
			 * than to kill e2fsck and leave the user stuck. */
			if (skip_group) {
				struct problem_context pctx2;
				pctx2.blk = i;
				pctx2.group = group;
				if (fix_problem(ctx, PR_5_INODE_UNINIT,&pctx2)){
					ext2fs_bg_flags_clear(fs, group, EXT2_BG_INODE_UNINIT);
					skip_group = 0;
				}
			}
		}
		if (pctx.ino == 0) {
			pctx.ino = pctx.ino2 = i;
			save_problem = problem;
		} else {
			if ((problem == save_problem) &&
			    (pctx.ino2 == i-1))
				pctx.ino2++;
			else {
				print_bitmap_problem(ctx, save_problem, &pctx);
				pctx.ino = pctx.ino2 = i;
				save_problem = problem;
			}
		}
		ctx->flags |= E2F_FLAG_PROG_SUPPRESS;
		had_problem++;
		/*
		 * If there a problem we should turn off the discard so we
		 * do not compromise the filesystem.
		 */
		ctx->options &= ~E2F_OPT_DISCARD;

do_counts:
		inodes++;
		if (bitmap) {
			if (ext2fs_test_inode_bitmap2(ctx->inode_dir_map, i))
				dirs_count++;
			if (inodes > first_free) {
				e2fsck_discard_inodes(ctx, group, first_free,
						      inodes - first_free);
				first_free = fs->super->s_inodes_per_group + 1;
			}
		} else {
			group_free++;
			free_inodes++;
			if (first_free > inodes)
				first_free = inodes;
		}

		if ((inodes == fs->super->s_inodes_per_group) ||
		    (i == fs->super->s_inodes_count)) {
			/*
			 * If the last inode is free, we can discard it as well.
			 */
			if (!bitmap && inodes >= first_free)
				e2fsck_discard_inodes(ctx, group, first_free,
						      inodes - first_free + 1);
			/*
			 * If discard zeroes data and the group inode table
			 * was not zeroed yet, set itable as zeroed
			 */
			if ((ctx->options & E2F_OPT_DISCARD) &&
			    io_channel_discard_zeroes_data(fs->io) &&
			    !(ext2fs_bg_flags_test(fs, group,
						   EXT2_BG_INODE_ZEROED))) {
				ext2fs_bg_flags_set(fs, group,
						    EXT2_BG_INODE_ZEROED);
				ext2fs_group_desc_csum_set(fs, group);
			}

			first_free = fs->super->s_inodes_per_group + 1;
			free_array[group] = group_free;
			dir_array[group] = dirs_count;
			group ++;
			inodes = 0;
			skip_group = 0;
			group_free = 0;
			dirs_count = 0;
			if (ctx->progress)
				if ((ctx->progress)(ctx, 5,
					    group + fs->group_desc_count,
					    fs->group_desc_count*2))
					goto errout;
			if (csum_flag &&
			    (i != fs->super->s_inodes_count) &&
			    (ext2fs_bg_flags_test(fs, group, EXT2_BG_INODE_UNINIT)
			     ))
				skip_group++;
		}
	}
	if (pctx.ino)
		print_bitmap_problem(ctx, save_problem, &pctx);

	if (had_problem)
		fixit = end_problem_latch(ctx, PR_LATCH_IBITMAP);
	else
		fixit = -1;
	ctx->flags &= ~E2F_FLAG_PROG_SUPPRESS;

	if (fixit == 1) {
		ext2fs_free_inode_bitmap(fs->inode_map);
		retval = ext2fs_copy_bitmap(ctx->inode_used_map,
						  &fs->inode_map);
		if (retval) {
			clear_problem_context(&pctx);
			fix_problem(ctx, PR_5_COPY_IBITMAP_ERROR, &pctx);
			ctx->flags |= E2F_FLAG_ABORT;
			goto errout;
		}
		ext2fs_set_bitmap_padding(fs->inode_map);
		ext2fs_mark_ib_dirty(fs);

		/* redo counts */
		inodes = 0; free_inodes = 0; group_free = 0;
		dirs_count = 0; group = 0;
		memset(free_array, 0, fs->group_desc_count * sizeof(int));
		memset(dir_array, 0, fs->group_desc_count * sizeof(int));
		redo_flag++;
		goto redo_counts;
	} else if (fixit == 0)
		ext2fs_unmark_valid(fs);

	for (i = 0; i < fs->group_desc_count; i++) {
		if (free_array[i] != ext2fs_bg_free_inodes_count(fs, i)) {
			pctx.group = i;
			pctx.ino = ext2fs_bg_free_inodes_count(fs, i);
			pctx.ino2 = free_array[i];
			if (fix_problem(ctx, PR_5_FREE_INODE_COUNT_GROUP,
					&pctx)) {
				ext2fs_bg_free_inodes_count_set(fs, i, free_array[i]);
				ext2fs_mark_super_dirty(fs);
			} else
				ext2fs_unmark_valid(fs);
		}
		if (dir_array[i] != ext2fs_bg_used_dirs_count(fs, i)) {
			pctx.group = i;
			pctx.ino = ext2fs_bg_used_dirs_count(fs, i);
			pctx.ino2 = dir_array[i];

			if (fix_problem(ctx, PR_5_FREE_DIR_COUNT_GROUP,
					&pctx)) {
				ext2fs_bg_used_dirs_count_set(fs, i, dir_array[i]);
				ext2fs_mark_super_dirty(fs);
			} else
				ext2fs_unmark_valid(fs);
		}
	}
	if (free_inodes != fs->super->s_free_inodes_count) {
		pctx.group = -1;
		pctx.ino = fs->super->s_free_inodes_count;
		pctx.ino2 = free_inodes;

		if (fix_problem(ctx, PR_5_FREE_INODE_COUNT, &pctx)) {
			fs->super->s_free_inodes_count = free_inodes;
			ext2fs_mark_super_dirty(fs);
		}
	}
errout:
	ext2fs_free_mem(&free_array);
	ext2fs_free_mem(&dir_array);
}

static void check_inode_end(e2fsck_t ctx)
{
	ext2_filsys fs = ctx->fs;
	ext2_ino_t	end, save_inodes_count, i;
	struct problem_context	pctx;

	clear_problem_context(&pctx);

	end = EXT2_INODES_PER_GROUP(fs->super) * fs->group_desc_count;
	pctx.errcode = ext2fs_fudge_inode_bitmap_end(fs->inode_map, end,
						     &save_inodes_count);
	if (pctx.errcode) {
		pctx.num = 1;
		fix_problem(ctx, PR_5_FUDGE_BITMAP_ERROR, &pctx);
		ctx->flags |= E2F_FLAG_ABORT; /* fatal */
		return;
	}
	if (save_inodes_count == end)
		return;

	/* protect loop from wrap-around if end is maxed */
	for (i = save_inodes_count + 1; i <= end && i > save_inodes_count; i++) {
		if (!ext2fs_test_inode_bitmap(fs->inode_map, i)) {
			if (fix_problem(ctx, PR_5_INODE_BMAP_PADDING, &pctx)) {
				for (; i <= end; i++)
					ext2fs_mark_inode_bitmap(fs->inode_map,
								 i);
				ext2fs_mark_ib_dirty(fs);
			} else
				ext2fs_unmark_valid(fs);
			break;
		}
	}

	pctx.errcode = ext2fs_fudge_inode_bitmap_end(fs->inode_map,
						     save_inodes_count, 0);
	if (pctx.errcode) {
		pctx.num = 2;
		fix_problem(ctx, PR_5_FUDGE_BITMAP_ERROR, &pctx);
		ctx->flags |= E2F_FLAG_ABORT; /* fatal */
		return;
	}
}

static void check_block_end(e2fsck_t ctx)
{
	ext2_filsys fs = ctx->fs;
	blk64_t	end, save_blocks_count, i;
	struct problem_context	pctx;

	clear_problem_context(&pctx);

	end = ext2fs_get_block_bitmap_start2(fs->block_map) +
		((blk64_t)EXT2_CLUSTERS_PER_GROUP(fs->super) * fs->group_desc_count) - 1;
	pctx.errcode = ext2fs_fudge_block_bitmap_end2(fs->block_map, end,
						     &save_blocks_count);
	if (pctx.errcode) {
		pctx.num = 3;
		fix_problem(ctx, PR_5_FUDGE_BITMAP_ERROR, &pctx);
		ctx->flags |= E2F_FLAG_ABORT; /* fatal */
		return;
	}
	if (save_blocks_count == end)
		return;

	/* Protect loop from wrap-around if end is maxed */
	for (i = save_blocks_count + 1; i <= end && i > save_blocks_count; i++) {
		if (!ext2fs_test_block_bitmap2(fs->block_map,
					       EXT2FS_C2B(fs, i))) {
			if (fix_problem(ctx, PR_5_BLOCK_BMAP_PADDING, &pctx)) {
				for (; i <= end; i++)
					ext2fs_mark_block_bitmap2(fs->block_map,
							EXT2FS_C2B(fs, i));
				ext2fs_mark_bb_dirty(fs);
			} else
				ext2fs_unmark_valid(fs);
			break;
		}
	}

	pctx.errcode = ext2fs_fudge_block_bitmap_end2(fs->block_map,
						     save_blocks_count, 0);
	if (pctx.errcode) {
		pctx.num = 4;
		fix_problem(ctx, PR_5_FUDGE_BITMAP_ERROR, &pctx);
		ctx->flags |= E2F_FLAG_ABORT; /* fatal */
		return;
	}
}



