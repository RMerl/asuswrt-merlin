/* vi: set sw=4 ts=4: */
/*
 * expand.c --- expand an ext2fs directory
 *
 * Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999  Theodore Ts'o.
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

struct expand_dir_struct {
	int		done;
	int		newblocks;
	errcode_t	err;
};

static int expand_dir_proc(ext2_filsys	fs,
			   blk_t	*blocknr,
			   e2_blkcnt_t	blockcnt,
			   blk_t	ref_block EXT2FS_ATTR((unused)),
			   int		ref_offset EXT2FS_ATTR((unused)),
			   void		*priv_data)
{
	struct expand_dir_struct *es = (struct expand_dir_struct *) priv_data;
	blk_t	new_blk;
	static blk_t	last_blk = 0;
	char		*block;
	errcode_t	retval;

	if (*blocknr) {
		last_blk = *blocknr;
		return 0;
	}
	retval = ext2fs_new_block(fs, last_blk, 0, &new_blk);
	if (retval) {
		es->err = retval;
		return BLOCK_ABORT;
	}
	if (blockcnt > 0) {
		retval = ext2fs_new_dir_block(fs, 0, 0, &block);
		if (retval) {
			es->err = retval;
			return BLOCK_ABORT;
		}
		es->done = 1;
		retval = ext2fs_write_dir_block(fs, new_blk, block);
	} else {
		retval = ext2fs_get_mem(fs->blocksize, &block);
		if (retval) {
			es->err = retval;
			return BLOCK_ABORT;
		}
		memset(block, 0, fs->blocksize);
		retval = io_channel_write_blk(fs->io, new_blk, 1, block);
	}
	if (retval) {
		es->err = retval;
		return BLOCK_ABORT;
	}
	ext2fs_free_mem(&block);
	*blocknr = new_blk;
	ext2fs_block_alloc_stats(fs, new_blk, +1);
	es->newblocks++;

	if (es->done)
		return (BLOCK_CHANGED | BLOCK_ABORT);
	else
		return BLOCK_CHANGED;
}

errcode_t ext2fs_expand_dir(ext2_filsys fs, ext2_ino_t dir)
{
	errcode_t	retval;
	struct expand_dir_struct es;
	struct ext2_inode	inode;

	EXT2_CHECK_MAGIC(fs, EXT2_ET_MAGIC_EXT2FS_FILSYS);

	if (!(fs->flags & EXT2_FLAG_RW))
		return EXT2_ET_RO_FILSYS;

	if (!fs->block_map)
		return EXT2_ET_NO_BLOCK_BITMAP;

	retval = ext2fs_check_directory(fs, dir);
	if (retval)
		return retval;

	es.done = 0;
	es.err = 0;
	es.newblocks = 0;

	retval = ext2fs_block_iterate2(fs, dir, BLOCK_FLAG_APPEND,
				       0, expand_dir_proc, &es);

	if (es.err)
		return es.err;
	if (!es.done)
		return EXT2_ET_EXPAND_DIR_ERR;

	/*
	 * Update the size and block count fields in the inode.
	 */
	retval = ext2fs_read_inode(fs, dir, &inode);
	if (retval)
		return retval;

	inode.i_size += fs->blocksize;
	inode.i_blocks += (fs->blocksize / 512) * es.newblocks;

	retval = ext2fs_write_inode(fs, dir, &inode);
	if (retval)
		return retval;

	return 0;
}
