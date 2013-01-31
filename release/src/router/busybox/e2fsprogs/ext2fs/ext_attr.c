/* vi: set sw=4 ts=4: */
/*
 * ext_attr.c --- extended attribute blocks
 *
 * Copyright (C) 2001 Andreas Gruenbacher, <a.gruenbacher@computer.org>
 *
 * Copyright (C) 2002 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include "ext2_fs.h"
#include "ext2_ext_attr.h"
#include "ext2fs.h"

errcode_t ext2fs_read_ext_attr(ext2_filsys fs, blk_t block, void *buf)
{
	errcode_t	retval;

	retval = io_channel_read_blk(fs->io, block, 1, buf);
	if (retval)
		return retval;
#if BB_BIG_ENDIAN
	if ((fs->flags & (EXT2_FLAG_SWAP_BYTES|
			  EXT2_FLAG_SWAP_BYTES_READ)) != 0)
		ext2fs_swap_ext_attr(buf, buf, fs->blocksize, 1);
#endif
	return 0;
}

errcode_t ext2fs_write_ext_attr(ext2_filsys fs, blk_t block, void *inbuf)
{
	errcode_t	retval;
	char		*write_buf;
	char		*buf = NULL;

	if (BB_BIG_ENDIAN && ((fs->flags & EXT2_FLAG_SWAP_BYTES) ||
	    (fs->flags & EXT2_FLAG_SWAP_BYTES_WRITE))) {
		retval = ext2fs_get_mem(fs->blocksize, &buf);
		if (retval)
			return retval;
		write_buf = buf;
		ext2fs_swap_ext_attr(buf, inbuf, fs->blocksize, 1);
	} else
		write_buf = (char *) inbuf;
	retval = io_channel_write_blk(fs->io, block, 1, write_buf);
	if (buf)
		ext2fs_free_mem(&buf);
	if (!retval)
		ext2fs_mark_changed(fs);
	return retval;
}

/*
 * This function adjusts the reference count of the EA block.
 */
errcode_t ext2fs_adjust_ea_refcount(ext2_filsys fs, blk_t blk,
				    char *block_buf, int adjust,
				    __u32 *newcount)
{
	errcode_t	retval;
	struct ext2_ext_attr_header *header;
	char	*buf = NULL;

	if ((blk >= fs->super->s_blocks_count) ||
	    (blk < fs->super->s_first_data_block))
		return EXT2_ET_BAD_EA_BLOCK_NUM;

	if (!block_buf) {
		retval = ext2fs_get_mem(fs->blocksize, &buf);
		if (retval)
			return retval;
		block_buf = buf;
	}

	retval = ext2fs_read_ext_attr(fs, blk, block_buf);
	if (retval)
		goto errout;

	header = (struct ext2_ext_attr_header *) block_buf;
	header->h_refcount += adjust;
	if (newcount)
		*newcount = header->h_refcount;

	retval = ext2fs_write_ext_attr(fs, blk, block_buf);
	if (retval)
		goto errout;

errout:
	if (buf)
		ext2fs_free_mem(&buf);
	return retval;
}
