/* vi: set sw=4 ts=4: */
/*
 * block.c --- iterate over all blocks in an inode
 *
 * Copyright (C) 1993, 1994, 1995, 1996 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include <stdio.h>
#include <string.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "ext2_fs.h"
#include "ext2fs.h"

struct block_context {
	ext2_filsys	fs;
	int (*func)(ext2_filsys	fs,
		    blk_t	*blocknr,
		    e2_blkcnt_t	bcount,
		    blk_t	ref_blk,
		    int		ref_offset,
		    void	*priv_data);
	e2_blkcnt_t	bcount;
	int		bsize;
	int		flags;
	errcode_t	errcode;
	char	*ind_buf;
	char	*dind_buf;
	char	*tind_buf;
	void	*priv_data;
};

static int block_iterate_ind(blk_t *ind_block, blk_t ref_block,
			     int ref_offset, struct block_context *ctx)
{
	int	ret = 0, changed = 0;
	int	i, flags, limit, offset;
	blk_t	*block_nr;

	limit = ctx->fs->blocksize >> 2;
	if (!(ctx->flags & BLOCK_FLAG_DEPTH_TRAVERSE) &&
	    !(ctx->flags & BLOCK_FLAG_DATA_ONLY))
		ret = (*ctx->func)(ctx->fs, ind_block,
				   BLOCK_COUNT_IND, ref_block,
				   ref_offset, ctx->priv_data);
	if (!*ind_block || (ret & BLOCK_ABORT)) {
		ctx->bcount += limit;
		return ret;
	}
	if (*ind_block >= ctx->fs->super->s_blocks_count ||
	    *ind_block < ctx->fs->super->s_first_data_block) {
		ctx->errcode = EXT2_ET_BAD_IND_BLOCK;
		ret |= BLOCK_ERROR;
		return ret;
	}
	ctx->errcode = ext2fs_read_ind_block(ctx->fs, *ind_block,
					     ctx->ind_buf);
	if (ctx->errcode) {
		ret |= BLOCK_ERROR;
		return ret;
	}

	block_nr = (blk_t *) ctx->ind_buf;
	offset = 0;
	if (ctx->flags & BLOCK_FLAG_APPEND) {
		for (i = 0; i < limit; i++, ctx->bcount++, block_nr++) {
			flags = (*ctx->func)(ctx->fs, block_nr, ctx->bcount,
					     *ind_block, offset,
					     ctx->priv_data);
			changed	|= flags;
			if (flags & BLOCK_ABORT) {
				ret |= BLOCK_ABORT;
				break;
			}
			offset += sizeof(blk_t);
		}
	} else {
		for (i = 0; i < limit; i++, ctx->bcount++, block_nr++) {
			if (*block_nr == 0)
				continue;
			flags = (*ctx->func)(ctx->fs, block_nr, ctx->bcount,
					     *ind_block, offset,
					     ctx->priv_data);
			changed	|= flags;
			if (flags & BLOCK_ABORT) {
				ret |= BLOCK_ABORT;
				break;
			}
			offset += sizeof(blk_t);
		}
	}
	if (changed & BLOCK_CHANGED) {
		ctx->errcode = ext2fs_write_ind_block(ctx->fs, *ind_block,
						      ctx->ind_buf);
		if (ctx->errcode)
			ret |= BLOCK_ERROR | BLOCK_ABORT;
	}
	if ((ctx->flags & BLOCK_FLAG_DEPTH_TRAVERSE) &&
	    !(ctx->flags & BLOCK_FLAG_DATA_ONLY) &&
	    !(ret & BLOCK_ABORT))
		ret |= (*ctx->func)(ctx->fs, ind_block,
				    BLOCK_COUNT_IND, ref_block,
				    ref_offset, ctx->priv_data);
	return ret;
}

static int block_iterate_dind(blk_t *dind_block, blk_t ref_block,
			      int ref_offset, struct block_context *ctx)
{
	int	ret = 0, changed = 0;
	int	i, flags, limit, offset;
	blk_t	*block_nr;

	limit = ctx->fs->blocksize >> 2;
	if (!(ctx->flags & (BLOCK_FLAG_DEPTH_TRAVERSE |
			    BLOCK_FLAG_DATA_ONLY)))
		ret = (*ctx->func)(ctx->fs, dind_block,
				   BLOCK_COUNT_DIND, ref_block,
				   ref_offset, ctx->priv_data);
	if (!*dind_block || (ret & BLOCK_ABORT)) {
		ctx->bcount += limit*limit;
		return ret;
	}
	if (*dind_block >= ctx->fs->super->s_blocks_count ||
	    *dind_block < ctx->fs->super->s_first_data_block) {
		ctx->errcode = EXT2_ET_BAD_DIND_BLOCK;
		ret |= BLOCK_ERROR;
		return ret;
	}
	ctx->errcode = ext2fs_read_ind_block(ctx->fs, *dind_block,
					     ctx->dind_buf);
	if (ctx->errcode) {
		ret |= BLOCK_ERROR;
		return ret;
	}

	block_nr = (blk_t *) ctx->dind_buf;
	offset = 0;
	if (ctx->flags & BLOCK_FLAG_APPEND) {
		for (i = 0; i < limit; i++, block_nr++) {
			flags = block_iterate_ind(block_nr,
						  *dind_block, offset,
						  ctx);
			changed |= flags;
			if (flags & (BLOCK_ABORT | BLOCK_ERROR)) {
				ret |= flags & (BLOCK_ABORT | BLOCK_ERROR);
				break;
			}
			offset += sizeof(blk_t);
		}
	} else {
		for (i = 0; i < limit; i++, block_nr++) {
			if (*block_nr == 0) {
				ctx->bcount += limit;
				continue;
			}
			flags = block_iterate_ind(block_nr,
						  *dind_block, offset,
						  ctx);
			changed |= flags;
			if (flags & (BLOCK_ABORT | BLOCK_ERROR)) {
				ret |= flags & (BLOCK_ABORT | BLOCK_ERROR);
				break;
			}
			offset += sizeof(blk_t);
		}
	}
	if (changed & BLOCK_CHANGED) {
		ctx->errcode = ext2fs_write_ind_block(ctx->fs, *dind_block,
						      ctx->dind_buf);
		if (ctx->errcode)
			ret |= BLOCK_ERROR | BLOCK_ABORT;
	}
	if ((ctx->flags & BLOCK_FLAG_DEPTH_TRAVERSE) &&
	    !(ctx->flags & BLOCK_FLAG_DATA_ONLY) &&
	    !(ret & BLOCK_ABORT))
		ret |= (*ctx->func)(ctx->fs, dind_block,
				    BLOCK_COUNT_DIND, ref_block,
				    ref_offset, ctx->priv_data);
	return ret;
}

static int block_iterate_tind(blk_t *tind_block, blk_t ref_block,
			      int ref_offset, struct block_context *ctx)
{
	int	ret = 0, changed = 0;
	int	i, flags, limit, offset;
	blk_t	*block_nr;

	limit = ctx->fs->blocksize >> 2;
	if (!(ctx->flags & (BLOCK_FLAG_DEPTH_TRAVERSE |
			    BLOCK_FLAG_DATA_ONLY)))
		ret = (*ctx->func)(ctx->fs, tind_block,
				   BLOCK_COUNT_TIND, ref_block,
				   ref_offset, ctx->priv_data);
	if (!*tind_block || (ret & BLOCK_ABORT)) {
		ctx->bcount += limit*limit*limit;
		return ret;
	}
	if (*tind_block >= ctx->fs->super->s_blocks_count ||
	    *tind_block < ctx->fs->super->s_first_data_block) {
		ctx->errcode = EXT2_ET_BAD_TIND_BLOCK;
		ret |= BLOCK_ERROR;
		return ret;
	}
	ctx->errcode = ext2fs_read_ind_block(ctx->fs, *tind_block,
					     ctx->tind_buf);
	if (ctx->errcode) {
		ret |= BLOCK_ERROR;
		return ret;
	}

	block_nr = (blk_t *) ctx->tind_buf;
	offset = 0;
	if (ctx->flags & BLOCK_FLAG_APPEND) {
		for (i = 0; i < limit; i++, block_nr++) {
			flags = block_iterate_dind(block_nr,
						   *tind_block,
						   offset, ctx);
			changed |= flags;
			if (flags & (BLOCK_ABORT | BLOCK_ERROR)) {
				ret |= flags & (BLOCK_ABORT | BLOCK_ERROR);
				break;
			}
			offset += sizeof(blk_t);
		}
	} else {
		for (i = 0; i < limit; i++, block_nr++) {
			if (*block_nr == 0) {
				ctx->bcount += limit*limit;
				continue;
			}
			flags = block_iterate_dind(block_nr,
						   *tind_block,
						   offset, ctx);
			changed |= flags;
			if (flags & (BLOCK_ABORT | BLOCK_ERROR)) {
				ret |= flags & (BLOCK_ABORT | BLOCK_ERROR);
				break;
			}
			offset += sizeof(blk_t);
		}
	}
	if (changed & BLOCK_CHANGED) {
		ctx->errcode = ext2fs_write_ind_block(ctx->fs, *tind_block,
						      ctx->tind_buf);
		if (ctx->errcode)
			ret |= BLOCK_ERROR | BLOCK_ABORT;
	}
	if ((ctx->flags & BLOCK_FLAG_DEPTH_TRAVERSE) &&
	    !(ctx->flags & BLOCK_FLAG_DATA_ONLY) &&
	    !(ret & BLOCK_ABORT))
		ret |= (*ctx->func)(ctx->fs, tind_block,
				    BLOCK_COUNT_TIND, ref_block,
				    ref_offset, ctx->priv_data);

	return ret;
}

errcode_t ext2fs_block_iterate2(ext2_filsys fs,
				ext2_ino_t ino,
				int	flags,
				char *block_buf,
				int (*func)(ext2_filsys fs,
					    blk_t	*blocknr,
					    e2_blkcnt_t	blockcnt,
					    blk_t	ref_blk,
					    int		ref_offset,
					    void	*priv_data),
				void *priv_data)
{
	int	i;
	int	got_inode = 0;
	int	ret = 0;
	blk_t	blocks[EXT2_N_BLOCKS];	/* directory data blocks */
	struct ext2_inode inode;
	errcode_t	retval;
	struct block_context ctx;
	int	limit;

	EXT2_CHECK_MAGIC(fs, EXT2_ET_MAGIC_EXT2FS_FILSYS);

	/*
	 * Check to see if we need to limit large files
	 */
	if (flags & BLOCK_FLAG_NO_LARGE) {
		ctx.errcode = ext2fs_read_inode(fs, ino, &inode);
		if (ctx.errcode)
			return ctx.errcode;
		got_inode = 1;
		if (!LINUX_S_ISDIR(inode.i_mode) &&
		    (inode.i_size_high != 0))
			return EXT2_ET_FILE_TOO_BIG;
	}

	retval = ext2fs_get_blocks(fs, ino, blocks);
	if (retval)
		return retval;

	limit = fs->blocksize >> 2;

	ctx.fs = fs;
	ctx.func = func;
	ctx.priv_data = priv_data;
	ctx.flags = flags;
	ctx.bcount = 0;
	if (block_buf) {
		ctx.ind_buf = block_buf;
	} else {
		retval = ext2fs_get_mem(fs->blocksize * 3, &ctx.ind_buf);
		if (retval)
			return retval;
	}
	ctx.dind_buf = ctx.ind_buf + fs->blocksize;
	ctx.tind_buf = ctx.dind_buf + fs->blocksize;

	/*
	 * Iterate over the HURD translator block (if present)
	 */
	if ((fs->super->s_creator_os == EXT2_OS_HURD) &&
	    !(flags & BLOCK_FLAG_DATA_ONLY)) {
		ctx.errcode = ext2fs_read_inode(fs, ino, &inode);
		if (ctx.errcode)
			goto abort_exit;
		got_inode = 1;
		if (inode.osd1.hurd1.h_i_translator) {
			ret |= (*ctx.func)(fs,
					   &inode.osd1.hurd1.h_i_translator,
					   BLOCK_COUNT_TRANSLATOR,
					   0, 0, priv_data);
			if (ret & BLOCK_ABORT)
				goto abort_exit;
		}
	}

	/*
	 * Iterate over normal data blocks
	 */
	for (i = 0; i < EXT2_NDIR_BLOCKS; i++, ctx.bcount++) {
		if (blocks[i] || (flags & BLOCK_FLAG_APPEND)) {
			ret |= (*ctx.func)(fs, &blocks[i],
					    ctx.bcount, 0, i, priv_data);
			if (ret & BLOCK_ABORT)
				goto abort_exit;
		}
	}
	if (*(blocks + EXT2_IND_BLOCK) || (flags & BLOCK_FLAG_APPEND)) {
		ret |= block_iterate_ind(blocks + EXT2_IND_BLOCK,
					 0, EXT2_IND_BLOCK, &ctx);
		if (ret & BLOCK_ABORT)
			goto abort_exit;
	} else
		ctx.bcount += limit;
	if (*(blocks + EXT2_DIND_BLOCK) || (flags & BLOCK_FLAG_APPEND)) {
		ret |= block_iterate_dind(blocks + EXT2_DIND_BLOCK,
					  0, EXT2_DIND_BLOCK, &ctx);
		if (ret & BLOCK_ABORT)
			goto abort_exit;
	} else
		ctx.bcount += limit * limit;
	if (*(blocks + EXT2_TIND_BLOCK) || (flags & BLOCK_FLAG_APPEND)) {
		ret |= block_iterate_tind(blocks + EXT2_TIND_BLOCK,
					  0, EXT2_TIND_BLOCK, &ctx);
		if (ret & BLOCK_ABORT)
			goto abort_exit;
	}

abort_exit:
	if (ret & BLOCK_CHANGED) {
		if (!got_inode) {
			retval = ext2fs_read_inode(fs, ino, &inode);
			if (retval)
				return retval;
		}
		for (i=0; i < EXT2_N_BLOCKS; i++)
			inode.i_block[i] = blocks[i];
		retval = ext2fs_write_inode(fs, ino, &inode);
		if (retval)
			return retval;
	}

	if (!block_buf)
		ext2fs_free_mem(&ctx.ind_buf);

	return (ret & BLOCK_ERROR) ? ctx.errcode : 0;
}

/*
 * Emulate the old ext2fs_block_iterate function!
 */

struct xlate {
	int (*func)(ext2_filsys	fs,
		    blk_t	*blocknr,
		    int		bcount,
		    void	*priv_data);
	void *real_private;
};

#ifdef __TURBOC__
# pragma argsused
#endif
static int xlate_func(ext2_filsys fs, blk_t *blocknr, e2_blkcnt_t blockcnt,
		      blk_t ref_block EXT2FS_ATTR((unused)),
		      int ref_offset EXT2FS_ATTR((unused)),
		      void *priv_data)
{
	struct xlate *xl = (struct xlate *) priv_data;

	return (*xl->func)(fs, blocknr, (int) blockcnt, xl->real_private);
}

errcode_t ext2fs_block_iterate(ext2_filsys fs,
			       ext2_ino_t ino,
			       int	flags,
			       char *block_buf,
			       int (*func)(ext2_filsys fs,
					   blk_t	*blocknr,
					   int	blockcnt,
					   void	*priv_data),
			       void *priv_data)
{
	struct xlate xl;

	xl.real_private = priv_data;
	xl.func = func;

	return ext2fs_block_iterate2(fs, ino, BLOCK_FLAG_NO_LARGE | flags,
				     block_buf, xlate_func, &xl);
}

