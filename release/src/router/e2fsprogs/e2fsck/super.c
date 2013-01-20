/*
 * e2fsck.c - superblock checks
 *
 * Copyright (C) 1993, 1994, 1995, 1996, 1997 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#ifndef EXT2_SKIP_UUID
#include "uuid/uuid.h"
#endif
#include "e2fsck.h"
#include "problem.h"

#define MIN_CHECK 1
#define MAX_CHECK 2

static void check_super_value(e2fsck_t ctx, const char *descr,
			      unsigned long value, int flags,
			      unsigned long min_val, unsigned long max_val)
{
	struct		problem_context pctx;

	if (((flags & MIN_CHECK) && (value < min_val)) ||
	    ((flags & MAX_CHECK) && (value > max_val))) {
		clear_problem_context(&pctx);
		pctx.num = value;
		pctx.str = descr;
		fix_problem(ctx, PR_0_MISC_CORRUPT_SUPER, &pctx);
		ctx->flags |= E2F_FLAG_ABORT; /* never get here! */
	}
}

/*
 * helper function to release an inode
 */
struct process_block_struct {
	e2fsck_t 	ctx;
	char 		*buf;
	struct problem_context *pctx;
	int		truncating;
	int		truncate_offset;
	e2_blkcnt_t	truncate_block;
	int		truncated_blocks;
	int		abort;
	errcode_t	errcode;
};

static int release_inode_block(ext2_filsys fs,
			       blk_t	*block_nr,
			       e2_blkcnt_t blockcnt,
			       blk_t	ref_blk EXT2FS_ATTR((unused)),
			       int	ref_offset EXT2FS_ATTR((unused)),
			       void *priv_data)
{
	struct process_block_struct *pb;
	e2fsck_t 		ctx;
	struct problem_context	*pctx;
	blk_t			blk = *block_nr;
	int			retval = 0;

	pb = (struct process_block_struct *) priv_data;
	ctx = pb->ctx;
	pctx = pb->pctx;

	pctx->blk = blk;
	pctx->blkcount = blockcnt;

	if (HOLE_BLKADDR(blk))
		return 0;

	if ((blk < fs->super->s_first_data_block) ||
	    (blk >= fs->super->s_blocks_count)) {
		fix_problem(ctx, PR_0_ORPHAN_ILLEGAL_BLOCK_NUM, pctx);
	return_abort:
		pb->abort = 1;
		return BLOCK_ABORT;
	}

	if (!ext2fs_test_block_bitmap(fs->block_map, blk)) {
		fix_problem(ctx, PR_0_ORPHAN_ALREADY_CLEARED_BLOCK, pctx);
		goto return_abort;
	}

	/*
	 * If we are deleting an orphan, then we leave the fields alone.
	 * If we are truncating an orphan, then update the inode fields
	 * and clean up any partial block data.
	 */
	if (pb->truncating) {
		/*
		 * We only remove indirect blocks if they are
		 * completely empty.
		 */
		if (blockcnt < 0) {
			int	i, limit;
			blk_t	*bp;

			pb->errcode = io_channel_read_blk(fs->io, blk, 1,
							pb->buf);
			if (pb->errcode)
				goto return_abort;

			limit = fs->blocksize >> 2;
			for (i = 0, bp = (blk_t *) pb->buf;
			     i < limit;	 i++, bp++)
				if (*bp)
					return 0;
		}
		/*
		 * We don't remove direct blocks until we've reached
		 * the truncation block.
		 */
		if (blockcnt >= 0 && blockcnt < pb->truncate_block)
			return 0;
		/*
		 * If part of the last block needs truncating, we do
		 * it here.
		 */
		if ((blockcnt == pb->truncate_block) && pb->truncate_offset) {
			pb->errcode = io_channel_read_blk(fs->io, blk, 1,
							pb->buf);
			if (pb->errcode)
				goto return_abort;
			memset(pb->buf + pb->truncate_offset, 0,
			       fs->blocksize - pb->truncate_offset);
			pb->errcode = io_channel_write_blk(fs->io, blk, 1,
							 pb->buf);
			if (pb->errcode)
				goto return_abort;
		}
		pb->truncated_blocks++;
		*block_nr = 0;
		retval |= BLOCK_CHANGED;
	}

	ext2fs_block_alloc_stats(fs, blk, -1);
	return retval;
}

/*
 * This function releases an inode.  Returns 1 if an inconsistency was
 * found.  If the inode has a link count, then it is being truncated and
 * not deleted.
 */
static int release_inode_blocks(e2fsck_t ctx, ext2_ino_t ino,
				struct ext2_inode *inode, char *block_buf,
				struct problem_context *pctx)
{
	struct process_block_struct 	pb;
	ext2_filsys			fs = ctx->fs;
	errcode_t			retval;
	__u32				count;

	if (!ext2fs_inode_has_valid_blocks(inode))
		return 0;

	pb.buf = block_buf + 3 * ctx->fs->blocksize;
	pb.ctx = ctx;
	pb.abort = 0;
	pb.errcode = 0;
	pb.pctx = pctx;
	if (inode->i_links_count) {
		pb.truncating = 1;
		pb.truncate_block = (e2_blkcnt_t)
			((((long long)inode->i_size_high << 32) +
			  inode->i_size + fs->blocksize - 1) /
			 fs->blocksize);
		pb.truncate_offset = inode->i_size % fs->blocksize;
	} else {
		pb.truncating = 0;
		pb.truncate_block = 0;
		pb.truncate_offset = 0;
	}
	pb.truncated_blocks = 0;
	retval = ext2fs_block_iterate2(fs, ino, BLOCK_FLAG_DEPTH_TRAVERSE,
				      block_buf, release_inode_block, &pb);
	if (retval) {
		com_err("release_inode_blocks", retval,
			_("while calling ext2fs_block_iterate for inode %d"),
			ino);
		return 1;
	}
	if (pb.abort)
		return 1;

	/* Refresh the inode since ext2fs_block_iterate may have changed it */
	e2fsck_read_inode(ctx, ino, inode, "release_inode_blocks");

	if (pb.truncated_blocks)
		ext2fs_iblk_sub_blocks(fs, inode, pb.truncated_blocks);

	if (inode->i_file_acl) {
		retval = ext2fs_adjust_ea_refcount(fs, inode->i_file_acl,
						   block_buf, -1, &count);
		if (retval == EXT2_ET_BAD_EA_BLOCK_NUM) {
			retval = 0;
			count = 1;
		}
		if (retval) {
			com_err("release_inode_blocks", retval,
		_("while calling ext2fs_adjust_ea_refcount for inode %d"),
				ino);
			return 1;
		}
		if (count == 0)
			ext2fs_block_alloc_stats(fs, inode->i_file_acl, -1);
		inode->i_file_acl = 0;
	}
	return 0;
}

/*
 * This function releases all of the orphan inodes.  It returns 1 if
 * it hit some error, and 0 on success.
 */
static int release_orphan_inodes(e2fsck_t ctx)
{
	ext2_filsys fs = ctx->fs;
	ext2_ino_t	ino, next_ino;
	struct ext2_inode inode;
	struct problem_context pctx;
	char *block_buf;

	if ((ino = fs->super->s_last_orphan) == 0)
		return 0;

	/*
	 * Win or lose, we won't be using the head of the orphan inode
	 * list again.
	 */
	fs->super->s_last_orphan = 0;
	ext2fs_mark_super_dirty(fs);

	/*
	 * If the filesystem contains errors, don't run the orphan
	 * list, since the orphan list can't be trusted; and we're
	 * going to be running a full e2fsck run anyway...
	 */
	if (fs->super->s_state & EXT2_ERROR_FS)
		return 0;

	if ((ino < EXT2_FIRST_INODE(fs->super)) ||
	    (ino > fs->super->s_inodes_count)) {
		clear_problem_context(&pctx);
		pctx.ino = ino;
		fix_problem(ctx, PR_0_ORPHAN_ILLEGAL_HEAD_INODE, &pctx);
		return 1;
	}

	block_buf = (char *) e2fsck_allocate_memory(ctx, fs->blocksize * 4,
						    "block iterate buffer");
	e2fsck_read_bitmaps(ctx);

	while (ino) {
		e2fsck_read_inode(ctx, ino, &inode, "release_orphan_inodes");
		clear_problem_context(&pctx);
		pctx.ino = ino;
		pctx.inode = &inode;
		pctx.str = inode.i_links_count ? _("Truncating") :
			_("Clearing");

		fix_problem(ctx, PR_0_ORPHAN_CLEAR_INODE, &pctx);

		next_ino = inode.i_dtime;
		if (next_ino &&
		    ((next_ino < EXT2_FIRST_INODE(fs->super)) ||
		     (next_ino > fs->super->s_inodes_count))) {
			pctx.ino = next_ino;
			fix_problem(ctx, PR_0_ORPHAN_ILLEGAL_INODE, &pctx);
			goto return_abort;
		}

		if (release_inode_blocks(ctx, ino, &inode, block_buf, &pctx))
			goto return_abort;

		if (!inode.i_links_count) {
			ext2fs_inode_alloc_stats2(fs, ino, -1,
						  LINUX_S_ISDIR(inode.i_mode));
			inode.i_dtime = ctx->now;
		} else {
			inode.i_dtime = 0;
		}
		e2fsck_write_inode(ctx, ino, &inode, "delete_file");
		ino = next_ino;
	}
	ext2fs_free_mem(&block_buf);
	return 0;
return_abort:
	ext2fs_free_mem(&block_buf);
	return 1;
}

/*
 * Check the resize inode to make sure it is sane.  We check both for
 * the case where on-line resizing is not enabled (in which case the
 * resize inode should be cleared) as well as the case where on-line
 * resizing is enabled.
 */
void check_resize_inode(e2fsck_t ctx)
{
	ext2_filsys fs = ctx->fs;
	struct ext2_inode inode;
	struct problem_context	pctx;
	int		i, gdt_off, ind_off;
	dgrp_t		j;
	blk_t		blk, pblk, expect;
	__u32 		*dind_buf = 0, *ind_buf;
	errcode_t	retval;

	clear_problem_context(&pctx);

	/*
	 * If the resize inode feature isn't set, then
	 * s_reserved_gdt_blocks must be zero.
	 */
	if (!(fs->super->s_feature_compat &
	      EXT2_FEATURE_COMPAT_RESIZE_INODE)) {
		if (fs->super->s_reserved_gdt_blocks) {
			pctx.num = fs->super->s_reserved_gdt_blocks;
			if (fix_problem(ctx, PR_0_NONZERO_RESERVED_GDT_BLOCKS,
					&pctx)) {
				fs->super->s_reserved_gdt_blocks = 0;
				ext2fs_mark_super_dirty(fs);
			}
		}
	}

	/* Read the resize inode */
	pctx.ino = EXT2_RESIZE_INO;
	retval = ext2fs_read_inode(fs, EXT2_RESIZE_INO, &inode);
	if (retval) {
		if (fs->super->s_feature_compat &
		    EXT2_FEATURE_COMPAT_RESIZE_INODE)
			ctx->flags |= E2F_FLAG_RESIZE_INODE;
		return;
	}

	/*
	 * If the resize inode feature isn't set, check to make sure
	 * the resize inode is cleared; then we're done.
	 */
	if (!(fs->super->s_feature_compat &
	      EXT2_FEATURE_COMPAT_RESIZE_INODE)) {
		for (i=0; i < EXT2_N_BLOCKS; i++) {
			if (inode.i_block[i])
				break;
		}
		if ((i < EXT2_N_BLOCKS) &&
		    fix_problem(ctx, PR_0_CLEAR_RESIZE_INODE, &pctx)) {
			memset(&inode, 0, sizeof(inode));
			e2fsck_write_inode(ctx, EXT2_RESIZE_INO, &inode,
					   "clear_resize");
		}
		return;
	}

	/*
	 * The resize inode feature is enabled; check to make sure the
	 * only block in use is the double indirect block
	 */
	blk = inode.i_block[EXT2_DIND_BLOCK];
	for (i=0; i < EXT2_N_BLOCKS; i++) {
		if (i != EXT2_DIND_BLOCK && inode.i_block[i])
			break;
	}
	if ((i < EXT2_N_BLOCKS) || !blk || !inode.i_links_count ||
	    !(inode.i_mode & LINUX_S_IFREG) ||
	    (blk < fs->super->s_first_data_block ||
	     blk >= fs->super->s_blocks_count)) {
	resize_inode_invalid:
		if (fix_problem(ctx, PR_0_RESIZE_INODE_INVALID, &pctx)) {
			memset(&inode, 0, sizeof(inode));
			e2fsck_write_inode(ctx, EXT2_RESIZE_INO, &inode,
					   "clear_resize");
			ctx->flags |= E2F_FLAG_RESIZE_INODE;
		}
		if (!(ctx->options & E2F_OPT_READONLY)) {
			fs->super->s_state &= ~EXT2_VALID_FS;
			ext2fs_mark_super_dirty(fs);
		}
		goto cleanup;
	}
	dind_buf = (__u32 *) e2fsck_allocate_memory(ctx, fs->blocksize * 2,
						    "resize dind buffer");
	ind_buf = (__u32 *) ((char *) dind_buf + fs->blocksize);

	retval = ext2fs_read_ind_block(fs, blk, dind_buf);
	if (retval)
		goto resize_inode_invalid;

	gdt_off = fs->desc_blocks;
	pblk = fs->super->s_first_data_block + 1 + fs->desc_blocks;
	for (i = 0; i < fs->super->s_reserved_gdt_blocks / 4;
	     i++, gdt_off++, pblk++) {
		gdt_off %= fs->blocksize/4;
		if (dind_buf[gdt_off] != pblk)
			goto resize_inode_invalid;
		retval = ext2fs_read_ind_block(fs, pblk, ind_buf);
		if (retval)
			goto resize_inode_invalid;
		ind_off = 0;
		for (j = 1; j < fs->group_desc_count; j++) {
			if (!ext2fs_bg_has_super(fs, j))
				continue;
			expect = pblk + (j * fs->super->s_blocks_per_group);
			if (ind_buf[ind_off] != expect)
				goto resize_inode_invalid;
			ind_off++;
		}
	}

cleanup:
	if (dind_buf)
		ext2fs_free_mem(&dind_buf);

 }

/*
 * This function checks the dirhash signed/unsigned hint if necessary.
 */
static void e2fsck_fix_dirhash_hint(e2fsck_t ctx)
{
	struct ext2_super_block *sb = ctx->fs->super;
	struct problem_context pctx;
	char	c;

	if ((ctx->options & E2F_OPT_READONLY) ||
	    !(sb->s_feature_compat & EXT2_FEATURE_COMPAT_DIR_INDEX) ||
	    (sb->s_flags & (EXT2_FLAGS_SIGNED_HASH|EXT2_FLAGS_UNSIGNED_HASH)))
		return;

	c = (char) 255;

	clear_problem_context(&pctx);
	if (fix_problem(ctx, PR_0_DIRHASH_HINT, &pctx)) {
		if (((int) c) == -1) {
			sb->s_flags |= EXT2_FLAGS_SIGNED_HASH;
		} else {
			sb->s_flags |= EXT2_FLAGS_UNSIGNED_HASH;
		}
		ext2fs_mark_super_dirty(ctx->fs);
	}
}


void check_super_block(e2fsck_t ctx)
{
	ext2_filsys fs = ctx->fs;
	blk_t	first_block, last_block;
	struct ext2_super_block *sb = fs->super;
	struct ext2_group_desc *gd;
	problem_t	problem;
	blk_t	blocks_per_group = fs->super->s_blocks_per_group;
	blk_t	bpg_max;
	int	inodes_per_block;
	int	ipg_max;
	int	inode_size;
	int	accept_time_fudge;
	int	broken_system_clock;
	dgrp_t	i;
	blk_t	should_be;
	struct problem_context	pctx;
	blk_t	free_blocks = 0;
	ino_t	free_inodes = 0;
	int     csum_flag, clear_test_fs_flag;

	inodes_per_block = EXT2_INODES_PER_BLOCK(fs->super);
	ipg_max = inodes_per_block * (blocks_per_group - 4);
	if (ipg_max > EXT2_MAX_INODES_PER_GROUP(sb))
		ipg_max = EXT2_MAX_INODES_PER_GROUP(sb);
	bpg_max = 8 * EXT2_BLOCK_SIZE(sb);
	if (bpg_max > EXT2_MAX_BLOCKS_PER_GROUP(sb))
		bpg_max = EXT2_MAX_BLOCKS_PER_GROUP(sb);

	ctx->invalid_inode_bitmap_flag = (int *) e2fsck_allocate_memory(ctx,
		 sizeof(int) * fs->group_desc_count, "invalid_inode_bitmap");
	ctx->invalid_block_bitmap_flag = (int *) e2fsck_allocate_memory(ctx,
		 sizeof(int) * fs->group_desc_count, "invalid_block_bitmap");
	ctx->invalid_inode_table_flag = (int *) e2fsck_allocate_memory(ctx,
		sizeof(int) * fs->group_desc_count, "invalid_inode_table");

	clear_problem_context(&pctx);

	/*
	 * Verify the super block constants...
	 */
	check_super_value(ctx, "inodes_count", sb->s_inodes_count,
			  MIN_CHECK, 1, 0);
	check_super_value(ctx, "blocks_count", sb->s_blocks_count,
			  MIN_CHECK, 1, 0);
	check_super_value(ctx, "first_data_block", sb->s_first_data_block,
			  MAX_CHECK, 0, sb->s_blocks_count);
	check_super_value(ctx, "log_block_size", sb->s_log_block_size,
			  MIN_CHECK | MAX_CHECK, 0,
			  EXT2_MAX_BLOCK_LOG_SIZE - EXT2_MIN_BLOCK_LOG_SIZE);
	check_super_value(ctx, "log_frag_size", sb->s_log_frag_size,
			  MIN_CHECK | MAX_CHECK, 0, sb->s_log_block_size);
	check_super_value(ctx, "frags_per_group", sb->s_frags_per_group,
			  MIN_CHECK | MAX_CHECK, sb->s_blocks_per_group,
			  bpg_max);
	check_super_value(ctx, "blocks_per_group", sb->s_blocks_per_group,
			  MIN_CHECK | MAX_CHECK, 8, bpg_max);
	check_super_value(ctx, "inodes_per_group", sb->s_inodes_per_group,
			  MIN_CHECK | MAX_CHECK, inodes_per_block, ipg_max);
	check_super_value(ctx, "r_blocks_count", sb->s_r_blocks_count,
			  MAX_CHECK, 0, sb->s_blocks_count / 2);
	check_super_value(ctx, "reserved_gdt_blocks",
			  sb->s_reserved_gdt_blocks, MAX_CHECK, 0,
			  fs->blocksize/4);
	if (sb->s_rev_level > EXT2_GOOD_OLD_REV)
		check_super_value(ctx, "first_ino", sb->s_first_ino,
				  MIN_CHECK | MAX_CHECK,
				  EXT2_GOOD_OLD_FIRST_INO, sb->s_inodes_count);
	inode_size = EXT2_INODE_SIZE(sb);
	check_super_value(ctx, "inode_size",
			  inode_size, MIN_CHECK | MAX_CHECK,
			  EXT2_GOOD_OLD_INODE_SIZE, fs->blocksize);
	if (inode_size & (inode_size - 1)) {
		pctx.num = inode_size;
		pctx.str = "inode_size";
		fix_problem(ctx, PR_0_MISC_CORRUPT_SUPER, &pctx);
		ctx->flags |= E2F_FLAG_ABORT; /* never get here! */
		return;
	}

	if ((ctx->flags & E2F_FLAG_GOT_DEVSIZE) &&
	    (ctx->num_blocks < sb->s_blocks_count)) {
		pctx.blk = sb->s_blocks_count;
		pctx.blk2 = ctx->num_blocks;
		if (fix_problem(ctx, PR_0_FS_SIZE_WRONG, &pctx)) {
			ctx->flags |= E2F_FLAG_ABORT;
			return;
		}
	}

	if (sb->s_log_block_size != (__u32) sb->s_log_frag_size) {
		pctx.blk = EXT2_BLOCK_SIZE(sb);
		pctx.blk2 = EXT2_FRAG_SIZE(sb);
		fix_problem(ctx, PR_0_NO_FRAGMENTS, &pctx);
		ctx->flags |= E2F_FLAG_ABORT;
		return;
	}

	should_be = sb->s_frags_per_group >>
		(sb->s_log_block_size - sb->s_log_frag_size);
	if (sb->s_blocks_per_group != should_be) {
		pctx.blk = sb->s_blocks_per_group;
		pctx.blk2 = should_be;
		fix_problem(ctx, PR_0_BLOCKS_PER_GROUP, &pctx);
		ctx->flags |= E2F_FLAG_ABORT;
		return;
	}

	should_be = (sb->s_log_block_size == 0) ? 1 : 0;
	if (sb->s_first_data_block != should_be) {
		pctx.blk = sb->s_first_data_block;
		pctx.blk2 = should_be;
		fix_problem(ctx, PR_0_FIRST_DATA_BLOCK, &pctx);
		ctx->flags |= E2F_FLAG_ABORT;
		return;
	}

	should_be = sb->s_inodes_per_group * fs->group_desc_count;
	if (sb->s_inodes_count != should_be) {
		pctx.ino = sb->s_inodes_count;
		pctx.ino2 = should_be;
		if (fix_problem(ctx, PR_0_INODE_COUNT_WRONG, &pctx)) {
			sb->s_inodes_count = should_be;
			ext2fs_mark_super_dirty(fs);
		}
	}

	/*
	 * Verify the group descriptors....
	 */
	first_block = sb->s_first_data_block;
	last_block = sb->s_blocks_count-1;

	csum_flag = EXT2_HAS_RO_COMPAT_FEATURE(fs->super,
					       EXT4_FEATURE_RO_COMPAT_GDT_CSUM);
	for (i = 0, gd=fs->group_desc; i < fs->group_desc_count; i++, gd++) {
		pctx.group = i;

		if (!EXT2_HAS_INCOMPAT_FEATURE(fs->super,
					       EXT4_FEATURE_INCOMPAT_FLEX_BG)) {
			first_block = ext2fs_group_first_block(fs, i);
			last_block = ext2fs_group_last_block(fs, i);
		}

		if ((gd->bg_block_bitmap < first_block) ||
		    (gd->bg_block_bitmap > last_block)) {
			pctx.blk = gd->bg_block_bitmap;
			if (fix_problem(ctx, PR_0_BB_NOT_GROUP, &pctx))
				gd->bg_block_bitmap = 0;
		}
		if (gd->bg_block_bitmap == 0) {
			ctx->invalid_block_bitmap_flag[i]++;
			ctx->invalid_bitmaps++;
		}
		if ((gd->bg_inode_bitmap < first_block) ||
		    (gd->bg_inode_bitmap > last_block)) {
			pctx.blk = gd->bg_inode_bitmap;
			if (fix_problem(ctx, PR_0_IB_NOT_GROUP, &pctx))
				gd->bg_inode_bitmap = 0;
		}
		if (gd->bg_inode_bitmap == 0) {
			ctx->invalid_inode_bitmap_flag[i]++;
			ctx->invalid_bitmaps++;
		}
		if ((gd->bg_inode_table < first_block) ||
		    ((gd->bg_inode_table +
		      fs->inode_blocks_per_group - 1) > last_block)) {
			pctx.blk = gd->bg_inode_table;
			if (fix_problem(ctx, PR_0_ITABLE_NOT_GROUP, &pctx))
				gd->bg_inode_table = 0;
		}
		if (gd->bg_inode_table == 0) {
			ctx->invalid_inode_table_flag[i]++;
			ctx->invalid_bitmaps++;
		}
		free_blocks += gd->bg_free_blocks_count;
		free_inodes += gd->bg_free_inodes_count;

		if ((gd->bg_free_blocks_count > sb->s_blocks_per_group) ||
		    (gd->bg_free_inodes_count > sb->s_inodes_per_group) ||
		    (gd->bg_used_dirs_count > sb->s_inodes_per_group))
			ext2fs_unmark_valid(fs);

		should_be = 0;
		if (!ext2fs_group_desc_csum_verify(fs, i)) {
			if (fix_problem(ctx, PR_0_GDT_CSUM, &pctx)) {
				gd->bg_flags &=	~(EXT2_BG_BLOCK_UNINIT |
				                  EXT2_BG_INODE_UNINIT);
				gd->bg_itable_unused = 0;
				should_be = 1;
			}
			ext2fs_unmark_valid(fs);
		}

		if (!csum_flag &&
		    (gd->bg_flags &(EXT2_BG_BLOCK_UNINIT|EXT2_BG_INODE_UNINIT)||
		     gd->bg_itable_unused != 0)){
			if (fix_problem(ctx, PR_0_GDT_UNINIT, &pctx)) {
				gd->bg_flags &= ~(EXT2_BG_BLOCK_UNINIT |
						  EXT2_BG_INODE_UNINIT);
				gd->bg_itable_unused = 0;
				should_be = 1;
			}
			ext2fs_unmark_valid(fs);
		}

		if (i == fs->group_desc_count - 1 &&
		    gd->bg_flags & EXT2_BG_BLOCK_UNINIT) {
			if (fix_problem(ctx, PR_0_BB_UNINIT_LAST, &pctx)) {
				gd->bg_flags &= ~EXT2_BG_BLOCK_UNINIT;
				should_be = 1;
			}
			ext2fs_unmark_valid(fs);
		}

		if (gd->bg_flags & EXT2_BG_BLOCK_UNINIT &&
		    !(gd->bg_flags & EXT2_BG_INODE_UNINIT)) {
			if (fix_problem(ctx, PR_0_BB_UNINIT_IB_INIT, &pctx)) {
				gd->bg_flags &= ~EXT2_BG_BLOCK_UNINIT;
				should_be = 1;
			}
			ext2fs_unmark_valid(fs);
		}

		if (csum_flag &&
		    (gd->bg_itable_unused > gd->bg_free_inodes_count ||
		     gd->bg_itable_unused > sb->s_inodes_per_group)) {
			pctx.blk = gd->bg_itable_unused;
			if (fix_problem(ctx, PR_0_GDT_ITABLE_UNUSED, &pctx)) {
				gd->bg_itable_unused = 0;
				should_be = 1;
			}
			ext2fs_unmark_valid(fs);
		}

		if (should_be)
			ext2fs_group_desc_csum_set(fs, i);
	}

	/*
	 * Update the global counts from the block group counts.  This
	 * is needed for an experimental patch which eliminates
	 * locking the entire filesystem when allocating blocks or
	 * inodes; if the filesystem is not unmounted cleanly, the
	 * global counts may not be accurate.
	 */
	if ((free_blocks != sb->s_free_blocks_count) ||
	    (free_inodes != sb->s_free_inodes_count)) {
		if (ctx->options & E2F_OPT_READONLY)
			ext2fs_unmark_valid(fs);
		else {
			sb->s_free_blocks_count = free_blocks;
			sb->s_free_inodes_count = free_inodes;
			ext2fs_mark_super_dirty(fs);
		}
	}

	if ((sb->s_free_blocks_count > sb->s_blocks_count) ||
	    (sb->s_free_inodes_count > sb->s_inodes_count))
		ext2fs_unmark_valid(fs);


	/*
	 * If we have invalid bitmaps, set the error state of the
	 * filesystem.
	 */
	if (ctx->invalid_bitmaps && !(ctx->options & E2F_OPT_READONLY)) {
		sb->s_state &= ~EXT2_VALID_FS;
		ext2fs_mark_super_dirty(fs);
	}

	clear_problem_context(&pctx);

#ifndef EXT2_SKIP_UUID
	/*
	 * If the UUID field isn't assigned, assign it.
	 */
	if (!(ctx->options & E2F_OPT_READONLY) && uuid_is_null(sb->s_uuid)) {
		if (fix_problem(ctx, PR_0_ADD_UUID, &pctx)) {
			uuid_generate(sb->s_uuid);
			ext2fs_mark_super_dirty(fs);
			fs->flags &= ~EXT2_FLAG_MASTER_SB_ONLY;
		}
	}
#endif

	/*
	 * Check to see if we should disable the test_fs flag
	 */
	profile_get_boolean(ctx->profile, "options",
			    "clear_test_fs_flag", 0, 1,
			    &clear_test_fs_flag);
	if (!(ctx->options & E2F_OPT_READONLY) &&
	    clear_test_fs_flag &&
	    (fs->super->s_flags & EXT2_FLAGS_TEST_FILESYS) &&
	    (fs_proc_check("ext4") || check_for_modules("ext4"))) {
		if (fix_problem(ctx, PR_0_CLEAR_TESTFS_FLAG, &pctx)) {
			fs->super->s_flags &= ~EXT2_FLAGS_TEST_FILESYS;
			ext2fs_mark_super_dirty(fs);
			fs->flags &= ~EXT2_FLAG_MASTER_SB_ONLY;
		}
	}
			
	/*
	 * For the Hurd, check to see if the filetype option is set,
	 * since it doesn't support it.
	 */
	if (!(ctx->options & E2F_OPT_READONLY) &&
	    fs->super->s_creator_os == EXT2_OS_HURD &&
	    (fs->super->s_feature_incompat &
	     EXT2_FEATURE_INCOMPAT_FILETYPE)) {
		if (fix_problem(ctx, PR_0_HURD_CLEAR_FILETYPE, &pctx)) {
			fs->super->s_feature_incompat &=
				~EXT2_FEATURE_INCOMPAT_FILETYPE;
			ext2fs_mark_super_dirty(fs);
			fs->flags &= ~EXT2_FLAG_MASTER_SB_ONLY;
		}
	}

	/*
	 * If we have any of the compatibility flags set, we need to have a
	 * revision 1 filesystem.  Most kernels will not check the flags on
	 * a rev 0 filesystem and we may have corruption issues because of
	 * the incompatible changes to the filesystem.
	 */
	if (!(ctx->options & E2F_OPT_READONLY) &&
	    fs->super->s_rev_level == EXT2_GOOD_OLD_REV &&
	    (fs->super->s_feature_compat ||
	     fs->super->s_feature_ro_compat ||
	     fs->super->s_feature_incompat) &&
	    fix_problem(ctx, PR_0_FS_REV_LEVEL, &pctx)) {
		ext2fs_update_dynamic_rev(fs);
		ext2fs_mark_super_dirty(fs);
		fs->flags &= ~EXT2_FLAG_MASTER_SB_ONLY;
	}

	/*
	 * Clean up any orphan inodes, if present.
	 */
	if (!(ctx->options & E2F_OPT_READONLY) && release_orphan_inodes(ctx)) {
		fs->super->s_state &= ~EXT2_VALID_FS;
		ext2fs_mark_super_dirty(fs);
	}

	/*
	 * Unfortunately, due to Windows' unfortunate design decision
	 * to configure the hardware clock to tick localtime, instead
	 * of the more proper and less error-prone UTC time, many
	 * users end up in the situation where the system clock is
	 * incorrectly set at the time when e2fsck is run.
	 *
	 * Historically this was usually due to some distributions
	 * having buggy init scripts and/or installers that didn't
	 * correctly detect this case and take appropriate
	 * countermeasures.  However, it's still possible, despite the
	 * best efforts of init script and installer authors to not be
	 * able to detect this misconfiguration, usually due to a
	 * buggy or misconfigured virtualization manager or the
	 * installer not having access to a network time server during
	 * the installation process.  So by default, we allow the
	 * superblock times to be fudged by up to 24 hours.  This can
	 * be disabled by setting options.accept_time_fudge to the
	 * boolean value of false in e2fsck.conf.  We also support
	 * options.buggy_init_scripts for backwards compatibility.
	 */
	profile_get_boolean(ctx->profile, "options", "accept_time_fudge",
			    0, 1, &accept_time_fudge);
	profile_get_boolean(ctx->profile, "options", "buggy_init_scripts",
			    0, accept_time_fudge, &accept_time_fudge);
	ctx->time_fudge = accept_time_fudge ? 86400 : 0;

	profile_get_boolean(ctx->profile, "options", "broken_system_clock",
			    0, 0, &broken_system_clock);

	/*
	 * Check to see if the superblock last mount time or last
	 * write time is in the future.
	 */
	if (!broken_system_clock &&
	    !(ctx->flags & E2F_FLAG_TIME_INSANE) &&
	    fs->super->s_mtime > (__u32) ctx->now) {
		pctx.num = fs->super->s_mtime;
		problem = PR_0_FUTURE_SB_LAST_MOUNT;
		if (fs->super->s_mtime <= (__u32) ctx->now + ctx->time_fudge)
			problem = PR_0_FUTURE_SB_LAST_MOUNT_FUDGED;
		if (fix_problem(ctx, problem, &pctx)) {
			fs->super->s_mtime = ctx->now;
			ext2fs_mark_super_dirty(fs);
		}
	}
	if (!broken_system_clock &&
	    !(ctx->flags & E2F_FLAG_TIME_INSANE) &&
	    fs->super->s_wtime > (__u32) ctx->now) {
		pctx.num = fs->super->s_wtime;
		problem = PR_0_FUTURE_SB_LAST_WRITE;
		if (fs->super->s_wtime <= (__u32) ctx->now + ctx->time_fudge)
			problem = PR_0_FUTURE_SB_LAST_WRITE_FUDGED;
		if (fix_problem(ctx, problem, &pctx)) {
			fs->super->s_wtime = ctx->now;
			ext2fs_mark_super_dirty(fs);
		}
	}

	/*
	 * Move the ext3 journal file, if necessary.
	 */
	e2fsck_move_ext3_journal(ctx);

	/*
	 * Fix journal hint, if necessary
	 */
	e2fsck_fix_ext3_journal_hint(ctx);

	/*
	 * Add dirhash hint if necessary
	 */
	e2fsck_fix_dirhash_hint(ctx);

	return;
}

/*
 * Check to see if we should backup the master sb to the backup super
 * blocks.  Returns non-zero if the sb should be backed up.
 */

/*
 * A few flags are set on the fly by the kernel, but only in the
 * primary superblock.  This is actually a bad thing, and we should
 * try to discourage it in the future.  In particular, for the newer
 * ext4 files, especially EXT4_FEATURE_RO_COMPAT_DIR_NLINK and
 * EXT3_FEATURE_INCOMPAT_EXTENTS.  So some of these may go away in the
 * future.  EXT3_FEATURE_INCOMPAT_RECOVER may also get set when
 * copying the primary superblock during online resize.
 *
 * The kernel will set EXT2_FEATURE_COMPAT_EXT_ATTR, but
 * unfortunately, we shouldn't ignore it since if it's not set in the
 * backup, the extended attributes in the filesystem will be stripped
 * away.
 */
#define FEATURE_RO_COMPAT_IGNORE	(EXT2_FEATURE_RO_COMPAT_LARGE_FILE| \
					 EXT4_FEATURE_RO_COMPAT_DIR_NLINK)
#define FEATURE_INCOMPAT_IGNORE		(EXT3_FEATURE_INCOMPAT_EXTENTS| \
					 EXT3_FEATURE_INCOMPAT_RECOVER)

int check_backup_super_block(e2fsck_t ctx)
{
	ext2_filsys	fs = ctx->fs;
	errcode_t	retval;
	dgrp_t		g;
	blk_t		sb;
	int		ret = 0;
	char		buf[SUPERBLOCK_SIZE];
	struct ext2_super_block	*backup_sb;

	/*
	 * If we are already writing out the backup blocks, then we
	 * don't need to test.  Also, if the filesystem is invalid, or
	 * the check was aborted or cancelled, we also don't want to
	 * do the backup.  If the filesystem was opened read-only then
	 * we can't do the backup.
	 */
	if (((fs->flags & EXT2_FLAG_MASTER_SB_ONLY) == 0) ||
	    !ext2fs_test_valid(fs) ||
	    (fs->super->s_state & EXT2_ERROR_FS) ||
	    (ctx->flags & (E2F_FLAG_ABORT | E2F_FLAG_CANCEL)) ||
	    (ctx->options & E2F_OPT_READONLY))
		return 0;

	for (g = 1; g < fs->group_desc_count; g++) {
		if (!ext2fs_bg_has_super(fs, g))
			continue;

		sb = fs->super->s_first_data_block +
			(g * fs->super->s_blocks_per_group);

		retval = io_channel_read_blk(fs->io, sb, -SUPERBLOCK_SIZE,
					     buf);
		if (retval)
			continue;
		backup_sb = (struct ext2_super_block *) buf;
#ifdef WORDS_BIGENDIAN
		ext2fs_swap_super(backup_sb);
#endif
		if ((backup_sb->s_magic != EXT2_SUPER_MAGIC) ||
		    (backup_sb->s_rev_level > EXT2_LIB_CURRENT_REV) ||
		    ((backup_sb->s_log_block_size + EXT2_MIN_BLOCK_LOG_SIZE) >
		     EXT2_MAX_BLOCK_LOG_SIZE) ||
		    (EXT2_INODE_SIZE(backup_sb) < EXT2_GOOD_OLD_INODE_SIZE))
			continue;

#define SUPER_INCOMPAT_DIFFERENT(x)	\
	((fs->super->x & ~FEATURE_INCOMPAT_IGNORE) !=	\
	 (backup_sb->x & ~FEATURE_INCOMPAT_IGNORE))
#define SUPER_RO_COMPAT_DIFFERENT(x)	\
	((fs->super->x & ~FEATURE_RO_COMPAT_IGNORE) !=	\
	 (backup_sb->x & ~FEATURE_RO_COMPAT_IGNORE))
#define SUPER_DIFFERENT(x)		\
	(fs->super->x != backup_sb->x)

		if (SUPER_DIFFERENT(s_feature_compat) ||
		    SUPER_INCOMPAT_DIFFERENT(s_feature_incompat) ||
		    SUPER_RO_COMPAT_DIFFERENT(s_feature_ro_compat) ||
		    SUPER_DIFFERENT(s_blocks_count) ||
		    SUPER_DIFFERENT(s_inodes_count) ||
		    memcmp(fs->super->s_uuid, backup_sb->s_uuid,
			   sizeof(fs->super->s_uuid)))
			ret = 1;
		break;
	}
	return ret;
}
