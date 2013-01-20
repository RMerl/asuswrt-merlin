/* vi: set sw=4 ts=4: */
/*
 * rw_bitmaps.c --- routines to read and write the  inode and block bitmaps.
 *
 * Copyright (C) 1993, 1994, 1994, 1996 Theodore Ts'o.
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
#include <fcntl.h>
#include <time.h>
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#include "ext2_fs.h"
#include "ext2fs.h"
#include "e2image.h"

#if defined(__powerpc__) && BB_BIG_ENDIAN
/*
 * On the PowerPC, the big-endian variant of the ext2 filesystem
 * has its bitmaps stored as 32-bit words with bit 0 as the LSB
 * of each word.  Thus a bitmap with only bit 0 set would be, as
 * a string of bytes, 00 00 00 01 00 ...
 * To cope with this, we byte-reverse each word of a bitmap if
 * we have a big-endian filesystem, that is, if we are *not*
 * byte-swapping other word-sized numbers.
 */
#define EXT2_BIG_ENDIAN_BITMAPS
#endif

#ifdef EXT2_BIG_ENDIAN_BITMAPS
static void ext2fs_swap_bitmap(ext2_filsys fs, char *bitmap, int nbytes)
{
	__u32 *p = (__u32 *) bitmap;
	int n;

	for (n = nbytes / sizeof(__u32); n > 0; --n, ++p)
		*p = ext2fs_swab32(*p);
}
#endif

errcode_t ext2fs_write_inode_bitmap(ext2_filsys fs)
{
	dgrp_t		i;
	size_t		nbytes;
	errcode_t	retval;
	char * inode_bitmap = fs->inode_map->bitmap;
	char * bitmap_block = NULL;
	blk_t		blk;

	EXT2_CHECK_MAGIC(fs, EXT2_ET_MAGIC_EXT2FS_FILSYS);

	if (!(fs->flags & EXT2_FLAG_RW))
		return EXT2_ET_RO_FILSYS;
	if (!inode_bitmap)
		return 0;
	nbytes = (size_t) ((EXT2_INODES_PER_GROUP(fs->super)+7) / 8);

	retval = ext2fs_get_mem(fs->blocksize, &bitmap_block);
	if (retval)
		return retval;
	memset(bitmap_block, 0xff, fs->blocksize);
	for (i = 0; i < fs->group_desc_count; i++) {
		memcpy(bitmap_block, inode_bitmap, nbytes);
		blk = fs->group_desc[i].bg_inode_bitmap;
		if (blk) {
#ifdef EXT2_BIG_ENDIAN_BITMAPS
			if (!((fs->flags & EXT2_FLAG_SWAP_BYTES) ||
			      (fs->flags & EXT2_FLAG_SWAP_BYTES_WRITE)))
				ext2fs_swap_bitmap(fs, bitmap_block, nbytes);
#endif
			retval = io_channel_write_blk(fs->io, blk, 1,
						      bitmap_block);
			if (retval)
				return EXT2_ET_INODE_BITMAP_WRITE;
		}
		inode_bitmap += nbytes;
	}
	fs->flags &= ~EXT2_FLAG_IB_DIRTY;
	ext2fs_free_mem(&bitmap_block);
	return 0;
}

errcode_t ext2fs_write_block_bitmap (ext2_filsys fs)
{
	dgrp_t		i;
	unsigned int	j;
	int		nbytes;
	unsigned int	nbits;
	errcode_t	retval;
	char * block_bitmap = fs->block_map->bitmap;
	char * bitmap_block = NULL;
	blk_t		blk;

	EXT2_CHECK_MAGIC(fs, EXT2_ET_MAGIC_EXT2FS_FILSYS);

	if (!(fs->flags & EXT2_FLAG_RW))
		return EXT2_ET_RO_FILSYS;
	if (!block_bitmap)
		return 0;
	nbytes = EXT2_BLOCKS_PER_GROUP(fs->super) / 8;
	retval = ext2fs_get_mem(fs->blocksize, &bitmap_block);
	if (retval)
		return retval;
	memset(bitmap_block, 0xff, fs->blocksize);
	for (i = 0; i < fs->group_desc_count; i++) {
		memcpy(bitmap_block, block_bitmap, nbytes);
		if (i == fs->group_desc_count - 1) {
			/* Force bitmap padding for the last group */
			nbits = ((fs->super->s_blocks_count
				  - fs->super->s_first_data_block)
				 % EXT2_BLOCKS_PER_GROUP(fs->super));
			if (nbits)
				for (j = nbits; j < fs->blocksize * 8; j++)
					ext2fs_set_bit(j, bitmap_block);
		}
		blk = fs->group_desc[i].bg_block_bitmap;
		if (blk) {
#ifdef EXT2_BIG_ENDIAN_BITMAPS
			if (!((fs->flags & EXT2_FLAG_SWAP_BYTES) ||
			      (fs->flags & EXT2_FLAG_SWAP_BYTES_WRITE)))
				ext2fs_swap_bitmap(fs, bitmap_block, nbytes);
#endif
			retval = io_channel_write_blk(fs->io, blk, 1,
						      bitmap_block);
			if (retval)
				return EXT2_ET_BLOCK_BITMAP_WRITE;
		}
		block_bitmap += nbytes;
	}
	fs->flags &= ~EXT2_FLAG_BB_DIRTY;
	ext2fs_free_mem(&bitmap_block);
	return 0;
}

static errcode_t read_bitmaps(ext2_filsys fs, int do_inode, int do_block)
{
	dgrp_t i;
	char *block_bitmap = NULL, *inode_bitmap = NULL;
	char *buf;
	errcode_t retval;
	int block_nbytes = (int) EXT2_BLOCKS_PER_GROUP(fs->super) / 8;
	int inode_nbytes = (int) EXT2_INODES_PER_GROUP(fs->super) / 8;
	blk_t	blk;

	EXT2_CHECK_MAGIC(fs, EXT2_ET_MAGIC_EXT2FS_FILSYS);

	fs->write_bitmaps = ext2fs_write_bitmaps;

	retval = ext2fs_get_mem(strlen(fs->device_name) + 80, &buf);
	if (retval)
		return retval;
	if (do_block) {
		ext2fs_free_block_bitmap(fs->block_map);
		sprintf(buf, "block bitmap for %s", fs->device_name);
		retval = ext2fs_allocate_block_bitmap(fs, buf, &fs->block_map);
		if (retval)
			goto cleanup;
		block_bitmap = fs->block_map->bitmap;
	}
	if (do_inode) {
		ext2fs_free_inode_bitmap(fs->inode_map);
		sprintf(buf, "inode bitmap for %s", fs->device_name);
		retval = ext2fs_allocate_inode_bitmap(fs, buf, &fs->inode_map);
		if (retval)
			goto cleanup;
		inode_bitmap = fs->inode_map->bitmap;
	}
	ext2fs_free_mem(&buf);

	if (fs->flags & EXT2_FLAG_IMAGE_FILE) {
		if (inode_bitmap) {
			blk = (fs->image_header->offset_inodemap /
			       fs->blocksize);
			retval = io_channel_read_blk(fs->image_io, blk,
			     -(inode_nbytes * fs->group_desc_count),
			     inode_bitmap);
			if (retval)
				goto cleanup;
		}
		if (block_bitmap) {
			blk = (fs->image_header->offset_blockmap /
			       fs->blocksize);
			retval = io_channel_read_blk(fs->image_io, blk,
			     -(block_nbytes * fs->group_desc_count),
			     block_bitmap);
			if (retval)
				goto cleanup;
		}
		return 0;
	}

	for (i = 0; i < fs->group_desc_count; i++) {
		if (block_bitmap) {
			blk = fs->group_desc[i].bg_block_bitmap;
			if (blk) {
				retval = io_channel_read_blk(fs->io, blk,
					     -block_nbytes, block_bitmap);
				if (retval) {
					retval = EXT2_ET_BLOCK_BITMAP_READ;
					goto cleanup;
				}
#ifdef EXT2_BIG_ENDIAN_BITMAPS
				if (!((fs->flags & EXT2_FLAG_SWAP_BYTES) ||
				      (fs->flags & EXT2_FLAG_SWAP_BYTES_READ)))
					ext2fs_swap_bitmap(fs, block_bitmap, block_nbytes);
#endif
			} else
				memset(block_bitmap, 0, block_nbytes);
			block_bitmap += block_nbytes;
		}
		if (inode_bitmap) {
			blk = fs->group_desc[i].bg_inode_bitmap;
			if (blk) {
				retval = io_channel_read_blk(fs->io, blk,
					     -inode_nbytes, inode_bitmap);
				if (retval) {
					retval = EXT2_ET_INODE_BITMAP_READ;
					goto cleanup;
				}
#ifdef EXT2_BIG_ENDIAN_BITMAPS
				if (!((fs->flags & EXT2_FLAG_SWAP_BYTES) ||
				      (fs->flags & EXT2_FLAG_SWAP_BYTES_READ)))
					ext2fs_swap_bitmap(fs, inode_bitmap, inode_nbytes);
#endif
			} else
				memset(inode_bitmap, 0, inode_nbytes);
			inode_bitmap += inode_nbytes;
		}
	}
	return 0;

cleanup:
	if (do_block) {
		ext2fs_free_mem(&fs->block_map);
	}
	if (do_inode) {
		ext2fs_free_mem(&fs->inode_map);
	}
	ext2fs_free_mem(&buf);
	return retval;
}

errcode_t ext2fs_read_inode_bitmap (ext2_filsys fs)
{
	return read_bitmaps(fs, 1, 0);
}

errcode_t ext2fs_read_block_bitmap(ext2_filsys fs)
{
	return read_bitmaps(fs, 0, 1);
}

errcode_t ext2fs_read_bitmaps(ext2_filsys fs)
{
	EXT2_CHECK_MAGIC(fs, EXT2_ET_MAGIC_EXT2FS_FILSYS);

	if (fs->inode_map && fs->block_map)
		return 0;

	return read_bitmaps(fs, !fs->inode_map, !fs->block_map);
}

errcode_t ext2fs_write_bitmaps(ext2_filsys fs)
{
	errcode_t	retval;

	EXT2_CHECK_MAGIC(fs, EXT2_ET_MAGIC_EXT2FS_FILSYS);

	if (fs->block_map && ext2fs_test_bb_dirty(fs)) {
		retval = ext2fs_write_block_bitmap(fs);
		if (retval)
			return retval;
	}
	if (fs->inode_map && ext2fs_test_ib_dirty(fs)) {
		retval = ext2fs_write_inode_bitmap(fs);
		if (retval)
			return retval;
	}
	return 0;
}
