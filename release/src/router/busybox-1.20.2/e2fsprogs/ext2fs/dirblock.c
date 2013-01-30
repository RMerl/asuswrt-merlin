/* vi: set sw=4 ts=4: */
/*
 * dirblock.c --- directory block routines.
 *
 * Copyright (C) 1995, 1996 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include <stdio.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <string.h>
#include <time.h>

#include "ext2_fs.h"
#include "ext2fs.h"

errcode_t ext2fs_read_dir_block2(ext2_filsys fs, blk_t block,
				 void *buf, int flags EXT2FS_ATTR((unused)))
{
	errcode_t	retval;
	char		*p, *end;
	struct ext2_dir_entry *dirent;
	unsigned int	name_len, rec_len;
#if BB_BIG_ENDIAN
	unsigned int do_swap;
#endif

	retval = io_channel_read_blk(fs->io, block, 1, buf);
	if (retval)
		return retval;
#if BB_BIG_ENDIAN
	do_swap = (fs->flags & (EXT2_FLAG_SWAP_BYTES|
				EXT2_FLAG_SWAP_BYTES_READ)) != 0;
#endif
	p = (char *) buf;
	end = (char *) buf + fs->blocksize;
	while (p < end-8) {
		dirent = (struct ext2_dir_entry *) p;
#if BB_BIG_ENDIAN
		if (do_swap) {
			dirent->inode = ext2fs_swab32(dirent->inode);
			dirent->rec_len = ext2fs_swab16(dirent->rec_len);
			dirent->name_len = ext2fs_swab16(dirent->name_len);
		}
#endif
		name_len = dirent->name_len;
#ifdef WORDS_BIGENDIAN
		if (flags & EXT2_DIRBLOCK_V2_STRUCT)
			dirent->name_len = ext2fs_swab16(dirent->name_len);
#endif
		rec_len = dirent->rec_len;
		if ((rec_len < 8) || (rec_len % 4)) {
			rec_len = 8;
			retval = EXT2_ET_DIR_CORRUPTED;
		}
		if (((name_len & 0xFF) + 8) > dirent->rec_len)
			retval = EXT2_ET_DIR_CORRUPTED;
		p += rec_len;
	}
	return retval;
}

errcode_t ext2fs_read_dir_block(ext2_filsys fs, blk_t block,
				 void *buf)
{
	return ext2fs_read_dir_block2(fs, block, buf, 0);
}


errcode_t ext2fs_write_dir_block2(ext2_filsys fs, blk_t block,
				  void *inbuf, int flags EXT2FS_ATTR((unused)))
{
#if BB_BIG_ENDIAN
	int		do_swap = 0;
	errcode_t	retval;
	char		*p, *end;
	char		*buf = NULL;
	struct ext2_dir_entry *dirent;

	if ((fs->flags & EXT2_FLAG_SWAP_BYTES) ||
	    (fs->flags & EXT2_FLAG_SWAP_BYTES_WRITE))
		do_swap = 1;

#ifndef WORDS_BIGENDIAN
	if (!do_swap)
		return io_channel_write_blk(fs->io, block, 1, (char *) inbuf);
#endif

	retval = ext2fs_get_mem(fs->blocksize, &buf);
	if (retval)
		return retval;
	memcpy(buf, inbuf, fs->blocksize);
	p = buf;
	end = buf + fs->blocksize;
	while (p < end) {
		dirent = (struct ext2_dir_entry *) p;
		if ((dirent->rec_len < 8) ||
		    (dirent->rec_len % 4)) {
			ext2fs_free_mem(&buf);
			return EXT2_ET_DIR_CORRUPTED;
		}
		p += dirent->rec_len;
		if (do_swap) {
			dirent->inode = ext2fs_swab32(dirent->inode);
			dirent->rec_len = ext2fs_swab16(dirent->rec_len);
			dirent->name_len = ext2fs_swab16(dirent->name_len);
		}
#ifdef WORDS_BIGENDIAN
		if (flags & EXT2_DIRBLOCK_V2_STRUCT)
			dirent->name_len = ext2fs_swab16(dirent->name_len);
#endif
	}
	retval = io_channel_write_blk(fs->io, block, 1, buf);
	ext2fs_free_mem(&buf);
	return retval;
#else
	return io_channel_write_blk(fs->io, block, 1, (char *) inbuf);
#endif
}


errcode_t ext2fs_write_dir_block(ext2_filsys fs, blk_t block,
				 void *inbuf)
{
	return ext2fs_write_dir_block2(fs, block, inbuf, 0);
}
