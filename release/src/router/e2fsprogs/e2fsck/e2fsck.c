/*
 * e2fsck.c - a consistency checker for the new extended file system.
 *
 * Copyright (C) 1993, 1994, 1995, 1996, 1997 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include "config.h"
#include <errno.h>

#include "e2fsck.h"
#include "problem.h"

/*
 * This function allocates an e2fsck context
 */
errcode_t e2fsck_allocate_context(e2fsck_t *ret)
{
	e2fsck_t	context;
	errcode_t	retval;
	char		*time_env;

	retval = ext2fs_get_mem(sizeof(struct e2fsck_struct), &context);
	if (retval)
		return retval;

	memset(context, 0, sizeof(struct e2fsck_struct));

	context->process_inode_size = 256;
	context->ext_attr_ver = 2;
	context->blocks_per_page = 1;
	context->htree_slack_percentage = 255;

	time_env = getenv("E2FSCK_TIME");
	if (time_env)
		context->now = strtoul(time_env, NULL, 0);
	else {
		context->now = time(0);
		if (context->now < 1262322000) /* January 1 2010 */
			context->flags |= E2F_FLAG_TIME_INSANE;
	}

	*ret = context;
	return 0;
}

/*
 * This function resets an e2fsck context; it is called when e2fsck
 * needs to be restarted.
 */
errcode_t e2fsck_reset_context(e2fsck_t ctx)
{
	int	i;

	ctx->flags &= E2F_RESET_FLAGS;
	ctx->lost_and_found = 0;
	ctx->bad_lost_and_found = 0;
	if (ctx->inode_used_map) {
		ext2fs_free_inode_bitmap(ctx->inode_used_map);
		ctx->inode_used_map = 0;
	}
	if (ctx->inode_dir_map) {
		ext2fs_free_inode_bitmap(ctx->inode_dir_map);
		ctx->inode_dir_map = 0;
	}
	if (ctx->inode_reg_map) {
		ext2fs_free_inode_bitmap(ctx->inode_reg_map);
		ctx->inode_reg_map = 0;
	}
	if (ctx->block_found_map) {
		ext2fs_free_block_bitmap(ctx->block_found_map);
		ctx->block_found_map = 0;
	}
	if (ctx->inode_link_info) {
		ext2fs_free_icount(ctx->inode_link_info);
		ctx->inode_link_info = 0;
	}
	if (ctx->journal_io) {
		if (ctx->fs && ctx->fs->io != ctx->journal_io)
			io_channel_close(ctx->journal_io);
		ctx->journal_io = 0;
	}
	if (ctx->fs && ctx->fs->dblist) {
		ext2fs_free_dblist(ctx->fs->dblist);
		ctx->fs->dblist = 0;
	}
	e2fsck_free_dir_info(ctx);
#ifdef ENABLE_HTREE
	e2fsck_free_dx_dir_info(ctx);
#endif
	if (ctx->refcount) {
		ea_refcount_free(ctx->refcount);
		ctx->refcount = 0;
	}
	if (ctx->refcount_extra) {
		ea_refcount_free(ctx->refcount_extra);
		ctx->refcount_extra = 0;
	}
	if (ctx->block_dup_map) {
		ext2fs_free_block_bitmap(ctx->block_dup_map);
		ctx->block_dup_map = 0;
	}
	if (ctx->block_ea_map) {
		ext2fs_free_block_bitmap(ctx->block_ea_map);
		ctx->block_ea_map = 0;
	}
	if (ctx->inode_bb_map) {
		ext2fs_free_inode_bitmap(ctx->inode_bb_map);
		ctx->inode_bb_map = 0;
	}
	if (ctx->inode_bad_map) {
		ext2fs_free_inode_bitmap(ctx->inode_bad_map);
		ctx->inode_bad_map = 0;
	}
	if (ctx->inode_imagic_map) {
		ext2fs_free_inode_bitmap(ctx->inode_imagic_map);
		ctx->inode_imagic_map = 0;
	}
	if (ctx->dirs_to_hash) {
		ext2fs_u32_list_free(ctx->dirs_to_hash);
		ctx->dirs_to_hash = 0;
	}

	/*
	 * Clear the array of invalid meta-data flags
	 */
	if (ctx->invalid_inode_bitmap_flag) {
		ext2fs_free_mem(&ctx->invalid_inode_bitmap_flag);
		ctx->invalid_inode_bitmap_flag = 0;
	}
	if (ctx->invalid_block_bitmap_flag) {
		ext2fs_free_mem(&ctx->invalid_block_bitmap_flag);
		ctx->invalid_block_bitmap_flag = 0;
	}
	if (ctx->invalid_inode_table_flag) {
		ext2fs_free_mem(&ctx->invalid_inode_table_flag);
		ctx->invalid_inode_table_flag = 0;
	}

	/* Clear statistic counters */
	ctx->fs_directory_count = 0;
	ctx->fs_regular_count = 0;
	ctx->fs_blockdev_count = 0;
	ctx->fs_chardev_count = 0;
	ctx->fs_links_count = 0;
	ctx->fs_symlinks_count = 0;
	ctx->fs_fast_symlinks_count = 0;
	ctx->fs_fifo_count = 0;
	ctx->fs_total_count = 0;
	ctx->fs_badblocks_count = 0;
	ctx->fs_sockets_count = 0;
	ctx->fs_ind_count = 0;
	ctx->fs_dind_count = 0;
	ctx->fs_tind_count = 0;
	ctx->fs_fragmented = 0;
	ctx->fs_fragmented_dir = 0;
	ctx->large_files = 0;

	for (i=0; i < MAX_EXTENT_DEPTH_COUNT; i++)
		ctx->extent_depth_count[i] = 0;

	/* Reset the superblock to the user's requested value */
	ctx->superblock = ctx->use_superblock;

	return 0;
}

void e2fsck_free_context(e2fsck_t ctx)
{
	if (!ctx)
		return;

	e2fsck_reset_context(ctx);
	if (ctx->blkid)
		blkid_put_cache(ctx->blkid);

	if (ctx->profile)
		profile_release(ctx->profile);

	if (ctx->filesystem_name)
		ext2fs_free_mem(&ctx->filesystem_name);

	if (ctx->device_name)
		ext2fs_free_mem(&ctx->device_name);

	if (ctx->log_fn)
		free(ctx->log_fn);

	ext2fs_free_mem(&ctx);
}

/*
 * This function runs through the e2fsck passes and calls them all,
 * returning restart, abort, or cancel as necessary...
 */
typedef void (*pass_t)(e2fsck_t ctx);

pass_t e2fsck_passes[] = {
	e2fsck_pass1, e2fsck_pass2, e2fsck_pass3, e2fsck_pass4,
	e2fsck_pass5, 0 };

#define E2F_FLAG_RUN_RETURN	(E2F_FLAG_SIGNAL_MASK|E2F_FLAG_RESTART)

int e2fsck_run(e2fsck_t ctx)
{
	int	i;
	pass_t	e2fsck_pass;

#ifdef HAVE_SETJMP_H
	if (setjmp(ctx->abort_loc)) {
		ctx->flags &= ~E2F_FLAG_SETJMP_OK;
		return (ctx->flags & E2F_FLAG_RUN_RETURN);
	}
	ctx->flags |= E2F_FLAG_SETJMP_OK;
#endif

	for (i=0; (e2fsck_pass = e2fsck_passes[i]); i++) {
		if (ctx->flags & E2F_FLAG_RUN_RETURN)
			break;
		if (e2fsck_mmp_update(ctx->fs))
			fatal_error(ctx, 0);
		e2fsck_pass(ctx);
		if (ctx->progress)
			(void) (ctx->progress)(ctx, 0, 0, 0);
	}
	ctx->flags &= ~E2F_FLAG_SETJMP_OK;

	if (ctx->flags & E2F_FLAG_RUN_RETURN)
		return (ctx->flags & E2F_FLAG_RUN_RETURN);
	return 0;
}
