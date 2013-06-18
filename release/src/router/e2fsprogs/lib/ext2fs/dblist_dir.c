/*
 * dblist_dir.c --- iterate by directory entry
 *
 * Copyright 1997 by Theodore Ts'o
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
#include <string.h>
#include <time.h>

#include "ext2_fs.h"
#include "ext2fsP.h"

static int db_dir_proc(ext2_filsys fs, struct ext2_db_entry2 *db_info,
		       void *priv_data);

errcode_t ext2fs_dblist_dir_iterate(ext2_dblist dblist,
				    int	flags,
				    char	*block_buf,
				    int (*func)(ext2_ino_t dir,
						int	entry,
						struct ext2_dir_entry *dirent,
						int	offset,
						int	blocksize,
						char	*buf,
						void	*priv_data),
				    void *priv_data)
{
	errcode_t		retval;
	struct dir_context	ctx;

	EXT2_CHECK_MAGIC(dblist, EXT2_ET_MAGIC_DBLIST);

	ctx.dir = 0;
	ctx.flags = flags;
	if (block_buf)
		ctx.buf = block_buf;
	else {
		retval = ext2fs_get_mem(dblist->fs->blocksize, &ctx.buf);
		if (retval)
			return retval;
	}
	ctx.func = func;
	ctx.priv_data = priv_data;
	ctx.errcode = 0;

	retval = ext2fs_dblist_iterate2(dblist, db_dir_proc, &ctx);

	if (!block_buf)
		ext2fs_free_mem(&ctx.buf);
	if (retval)
		return retval;
	return ctx.errcode;
}

static int db_dir_proc(ext2_filsys fs, struct ext2_db_entry2 *db_info,
		       void *priv_data)
{
	struct dir_context	*ctx;
	int			ret;

	ctx = (struct dir_context *) priv_data;
	ctx->dir = db_info->ino;
	ctx->errcode = 0;

	ret = ext2fs_process_dir_block(fs, &db_info->blk,
				       db_info->blockcnt, 0, 0, priv_data);
	if ((ret & BLOCK_ABORT) && !ctx->errcode)
		return DBLIST_ABORT;
	return 0;
}
