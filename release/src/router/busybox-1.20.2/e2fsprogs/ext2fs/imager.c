/* vi: set sw=4 ts=4: */
/*
 * image.c --- writes out the critical parts of the filesystem as a
 *	flat file.
 *
 * Copyright (C) 2000 Theodore Ts'o.
 *
 * Note: this uses the POSIX IO interfaces, unlike most of the other
 * functions in this library.  So sue me.
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
#if HAVE_ERRNO_H
#include <errno.h>
#endif
#include <fcntl.h>
#include <time.h>
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#include "ext2_fs.h"
#include "ext2fs.h"

#ifndef HAVE_TYPE_SSIZE_T
typedef int ssize_t;
#endif

/*
 * This function returns 1 if the specified block is all zeros
 */
static int check_zero_block(char *buf, int blocksize)
{
	char	*cp = buf;
	int	left = blocksize;

	while (left > 0) {
		if (*cp++)
			return 0;
		left--;
	}
	return 1;
}

/*
 * Write the inode table out as a single block.
 */
#define BUF_BLOCKS	32

errcode_t ext2fs_image_inode_write(ext2_filsys fs, int fd, int flags)
{
	unsigned int	group, left, c, d;
	char		*buf, *cp;
	blk_t		blk;
	ssize_t		actual;
	errcode_t	retval;

	buf = xmalloc(fs->blocksize * BUF_BLOCKS);

	for (group = 0; group < fs->group_desc_count; group++) {
		blk = fs->group_desc[(unsigned)group].bg_inode_table;
		if (!blk)
			return EXT2_ET_MISSING_INODE_TABLE;
		left = fs->inode_blocks_per_group;
		while (left) {
			c = BUF_BLOCKS;
			if (c > left)
				c = left;
			retval = io_channel_read_blk(fs->io, blk, c, buf);
			if (retval)
				goto errout;
			cp = buf;
			while (c) {
				if (!(flags & IMAGER_FLAG_SPARSEWRITE)) {
					d = c;
					goto skip_sparse;
				}
				/* Skip zero blocks */
				if (check_zero_block(cp, fs->blocksize)) {
					c--;
					blk++;
					left--;
					cp += fs->blocksize;
					lseek(fd, fs->blocksize, SEEK_CUR);
					continue;
				}
				/* Find non-zero blocks */
				for (d=1; d < c; d++) {
					if (check_zero_block(cp + d*fs->blocksize, fs->blocksize))
						break;
				}
			skip_sparse:
				actual = write(fd, cp, fs->blocksize * d);
				if (actual == -1) {
					retval = errno;
					goto errout;
				}
				if (actual != (ssize_t) (fs->blocksize * d)) {
					retval = EXT2_ET_SHORT_WRITE;
					goto errout;
				}
				blk += d;
				left -= d;
				cp += fs->blocksize * d;
				c -= d;
			}
		}
	}
	retval = 0;

errout:
	free(buf);
	return retval;
}

/*
 * Read in the inode table and stuff it into place
 */
errcode_t ext2fs_image_inode_read(ext2_filsys fs, int fd,
				  int flags EXT2FS_ATTR((unused)))
{
	unsigned int	group, c, left;
	char		*buf;
	blk_t		blk;
	ssize_t		actual;
	errcode_t	retval;

	buf = xmalloc(fs->blocksize * BUF_BLOCKS);

	for (group = 0; group < fs->group_desc_count; group++) {
		blk = fs->group_desc[(unsigned)group].bg_inode_table;
		if (!blk) {
			retval = EXT2_ET_MISSING_INODE_TABLE;
			goto errout;
		}
		left = fs->inode_blocks_per_group;
		while (left) {
			c = BUF_BLOCKS;
			if (c > left)
				c = left;
			actual = read(fd, buf, fs->blocksize * c);
			if (actual == -1) {
				retval = errno;
				goto errout;
			}
			if (actual != (ssize_t) (fs->blocksize * c)) {
				retval = EXT2_ET_SHORT_READ;
				goto errout;
			}
			retval = io_channel_write_blk(fs->io, blk, c, buf);
			if (retval)
				goto errout;

			blk += c;
			left -= c;
		}
	}
	retval = ext2fs_flush_icache(fs);

errout:
	free(buf);
	return retval;
}

/*
 * Write out superblock and group descriptors
 */
errcode_t ext2fs_image_super_write(ext2_filsys fs, int fd,
				   int flags EXT2FS_ATTR((unused)))
{
	char		*buf, *cp;
	ssize_t		actual;
	errcode_t	retval;

	buf = xmalloc(fs->blocksize);

	/*
	 * Write out the superblock
	 */
	memset(buf, 0, fs->blocksize);
	memcpy(buf, fs->super, SUPERBLOCK_SIZE);
	actual = write(fd, buf, fs->blocksize);
	if (actual == -1) {
		retval = errno;
		goto errout;
	}
	if (actual != (ssize_t) fs->blocksize) {
		retval = EXT2_ET_SHORT_WRITE;
		goto errout;
	}

	/*
	 * Now write out the block group descriptors
	 */
	cp = (char *) fs->group_desc;
	actual = write(fd, cp, fs->blocksize * fs->desc_blocks);
	if (actual == -1) {
		retval = errno;
		goto errout;
	}
	if (actual != (ssize_t) (fs->blocksize * fs->desc_blocks)) {
		retval = EXT2_ET_SHORT_WRITE;
		goto errout;
	}

	retval = 0;

errout:
	free(buf);
	return retval;
}

/*
 * Read the superblock and group descriptors and overwrite them.
 */
errcode_t ext2fs_image_super_read(ext2_filsys fs, int fd,
				  int flags EXT2FS_ATTR((unused)))
{
	char		*buf;
	ssize_t		actual, size;
	errcode_t	retval;

	size = fs->blocksize * (fs->group_desc_count + 1);
	buf = xmalloc(size);

	/*
	 * Read it all in.
	 */
	actual = read(fd, buf, size);
	if (actual == -1) {
		retval = errno;
		goto errout;
	}
	if (actual != size) {
		retval = EXT2_ET_SHORT_READ;
		goto errout;
	}

	/*
	 * Now copy in the superblock and group descriptors
	 */
	memcpy(fs->super, buf, SUPERBLOCK_SIZE);

	memcpy(fs->group_desc, buf + fs->blocksize,
	       fs->blocksize * fs->group_desc_count);

	retval = 0;

errout:
	free(buf);
	return retval;
}

/*
 * Write the block/inode bitmaps.
 */
errcode_t ext2fs_image_bitmap_write(ext2_filsys fs, int fd, int flags)
{
	char		*ptr;
	int		c, size;
	char		zero_buf[1024];
	ssize_t		actual;
	errcode_t	retval;

	if (flags & IMAGER_FLAG_INODEMAP) {
		if (!fs->inode_map) {
			retval = ext2fs_read_inode_bitmap(fs);
			if (retval)
				return retval;
		}
		ptr = fs->inode_map->bitmap;
		size = (EXT2_INODES_PER_GROUP(fs->super) / 8);
	} else {
		if (!fs->block_map) {
			retval = ext2fs_read_block_bitmap(fs);
			if (retval)
				return retval;
		}
		ptr = fs->block_map->bitmap;
		size = EXT2_BLOCKS_PER_GROUP(fs->super) / 8;
	}
	size = size * fs->group_desc_count;

	actual = write(fd, ptr, size);
	if (actual == -1) {
		retval = errno;
		goto errout;
	}
	if (actual != size) {
		retval = EXT2_ET_SHORT_WRITE;
		goto errout;
	}
	size = size % fs->blocksize;
	memset(zero_buf, 0, sizeof(zero_buf));
	if (size) {
		size = fs->blocksize - size;
		while (size) {
			c = size;
			if (c > (int) sizeof(zero_buf))
				c = sizeof(zero_buf);
			actual = write(fd, zero_buf, c);
			if (actual == -1) {
				retval = errno;
				goto errout;
			}
			if (actual != c) {
				retval = EXT2_ET_SHORT_WRITE;
				goto errout;
			}
			size -= c;
		}
	}
	retval = 0;
errout:
	return retval;
}


/*
 * Read the block/inode bitmaps.
 */
errcode_t ext2fs_image_bitmap_read(ext2_filsys fs, int fd, int flags)
{
	char		*ptr, *buf = NULL;
	int		size;
	ssize_t		actual;
	errcode_t	retval;

	if (flags & IMAGER_FLAG_INODEMAP) {
		if (!fs->inode_map) {
			retval = ext2fs_read_inode_bitmap(fs);
			if (retval)
				return retval;
		}
		ptr = fs->inode_map->bitmap;
		size = (EXT2_INODES_PER_GROUP(fs->super) / 8);
	} else {
		if (!fs->block_map) {
			retval = ext2fs_read_block_bitmap(fs);
			if (retval)
				return retval;
		}
		ptr = fs->block_map->bitmap;
		size = EXT2_BLOCKS_PER_GROUP(fs->super) / 8;
	}
	size = size * fs->group_desc_count;

	buf = xmalloc(size);

	actual = read(fd, buf, size);
	if (actual == -1) {
		retval = errno;
		goto errout;
	}
	if (actual != size) {
		retval = EXT2_ET_SHORT_WRITE;
		goto errout;
	}
	memcpy(ptr, buf, size);

	retval = 0;
errout:
	free(buf);
	return retval;
}
