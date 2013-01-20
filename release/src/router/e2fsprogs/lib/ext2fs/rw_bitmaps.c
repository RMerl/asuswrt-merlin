/*
 * rw_bitmaps.c --- routines to read and write the  inode and block bitmaps.
 *
 * Copyright (C) 1993, 1994, 1994, 1996 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Library
 * General Public License, version 2.
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

static errcode_t write_bitmaps(ext2_filsys fs, int do_inode, int do_block)
{
	dgrp_t 		i;
	unsigned int	j;
	int		block_nbytes, inode_nbytes;
	unsigned int	nbits;
	errcode_t	retval;
	char 		*block_buf, *inode_buf;
	int		csum_flag = 0;
	blk_t		blk;
	blk_t		blk_itr = fs->super->s_first_data_block;
	ext2_ino_t	ino_itr = 1;

	EXT2_CHECK_MAGIC(fs, EXT2_ET_MAGIC_EXT2FS_FILSYS);

	if (!(fs->flags & EXT2_FLAG_RW))
		return EXT2_ET_RO_FILSYS;

	if (EXT2_HAS_RO_COMPAT_FEATURE(fs->super,
				       EXT4_FEATURE_RO_COMPAT_GDT_CSUM))
		csum_flag = 1;

	inode_nbytes = block_nbytes = 0;
	if (do_block) {
		block_nbytes = EXT2_BLOCKS_PER_GROUP(fs->super) / 8;
		retval = ext2fs_get_memalign(fs->blocksize, fs->blocksize,
					     &block_buf);
		if (retval)
			return retval;
		memset(block_buf, 0xff, fs->blocksize);
	}
	if (do_inode) {
		inode_nbytes = (size_t)
			((EXT2_INODES_PER_GROUP(fs->super)+7) / 8);
		retval = ext2fs_get_memalign(fs->blocksize, fs->blocksize,
					     &inode_buf);
		if (retval)
			return retval;
		memset(inode_buf, 0xff, fs->blocksize);
	}

	for (i = 0; i < fs->group_desc_count; i++) {
		if (!do_block)
			goto skip_block_bitmap;

		if (csum_flag && fs->group_desc[i].bg_flags &
		    EXT2_BG_BLOCK_UNINIT)
			goto skip_this_block_bitmap;

		retval = ext2fs_get_block_bitmap_range(fs->block_map,
				blk_itr, block_nbytes << 3, block_buf);
		if (retval)
			return retval;

		if (i == fs->group_desc_count - 1) {
			/* Force bitmap padding for the last group */
			nbits = ((fs->super->s_blocks_count
				  - fs->super->s_first_data_block)
				 % EXT2_BLOCKS_PER_GROUP(fs->super));
			if (nbits)
				for (j = nbits; j < fs->blocksize * 8; j++)
					ext2fs_set_bit(j, block_buf);
		}
		blk = fs->group_desc[i].bg_block_bitmap;
		if (blk) {
			retval = io_channel_write_blk(fs->io, blk, 1,
						      block_buf);
			if (retval)
				return EXT2_ET_BLOCK_BITMAP_WRITE;
		}
	skip_this_block_bitmap:
		blk_itr += block_nbytes << 3;
	skip_block_bitmap:

		if (!do_inode)
			continue;

		if (csum_flag && fs->group_desc[i].bg_flags &
		    EXT2_BG_INODE_UNINIT)
			goto skip_this_inode_bitmap;

		retval = ext2fs_get_inode_bitmap_range(fs->inode_map,
				ino_itr, inode_nbytes << 3, inode_buf);
		if (retval)
			return retval;

		blk = fs->group_desc[i].bg_inode_bitmap;
		if (blk) {
			retval = io_channel_write_blk(fs->io, blk, 1,
						      inode_buf);
			if (retval)
				return EXT2_ET_INODE_BITMAP_WRITE;
		}
	skip_this_inode_bitmap:
		ino_itr += inode_nbytes << 3;

	}
	if (do_block) {
		fs->flags &= ~EXT2_FLAG_BB_DIRTY;
		ext2fs_free_mem(&block_buf);
	}
	if (do_inode) {
		fs->flags &= ~EXT2_FLAG_IB_DIRTY;
		ext2fs_free_mem(&inode_buf);
	}
	return 0;
}

static errcode_t read_bitmaps(ext2_filsys fs, int do_inode, int do_block)
{
	dgrp_t i;
	char *block_bitmap = 0, *inode_bitmap = 0;
	char *buf;
	errcode_t retval;
	int block_nbytes = EXT2_BLOCKS_PER_GROUP(fs->super) / 8;
	int inode_nbytes = EXT2_INODES_PER_GROUP(fs->super) / 8;
	int csum_flag = 0;
	int do_image = fs->flags & EXT2_FLAG_IMAGE_FILE;
	unsigned int	cnt;
	blk_t	blk;
	blk_t	blk_itr = fs->super->s_first_data_block;
	blk_t   blk_cnt;
	ext2_ino_t ino_itr = 1;
	ext2_ino_t ino_cnt;

	EXT2_CHECK_MAGIC(fs, EXT2_ET_MAGIC_EXT2FS_FILSYS);

	fs->write_bitmaps = ext2fs_write_bitmaps;

	if (EXT2_HAS_RO_COMPAT_FEATURE(fs->super,
				       EXT4_FEATURE_RO_COMPAT_GDT_CSUM))
		csum_flag = 1;

	retval = ext2fs_get_mem(strlen(fs->device_name) + 80, &buf);
	if (retval)
		return retval;
	if (do_block) {
		if (fs->block_map)
			ext2fs_free_block_bitmap(fs->block_map);
		strcpy(buf, "block bitmap for ");
		strcat(buf, fs->device_name);
		retval = ext2fs_allocate_block_bitmap(fs, buf, &fs->block_map);
		if (retval)
			goto cleanup;
		if (do_image)
			retval = ext2fs_get_mem(fs->blocksize, &block_bitmap);
		else
			retval = ext2fs_get_memalign((unsigned) block_nbytes,
						     fs->blocksize,
						     &block_bitmap);
			
		if (retval)
			goto cleanup;
	} else
		block_nbytes = 0;
	if (do_inode) {
		if (fs->inode_map)
			ext2fs_free_inode_bitmap(fs->inode_map);
		strcpy(buf, "inode bitmap for ");
		strcat(buf, fs->device_name);
		retval = ext2fs_allocate_inode_bitmap(fs, buf, &fs->inode_map);
		if (retval)
			goto cleanup;
		retval = ext2fs_get_mem(do_image ? fs->blocksize :
					(unsigned) inode_nbytes, &inode_bitmap);
		if (retval)
			goto cleanup;
	} else
		inode_nbytes = 0;
	ext2fs_free_mem(&buf);

	if (fs->flags & EXT2_FLAG_IMAGE_FILE) {
		blk = (fs->image_header->offset_inodemap / fs->blocksize);
		ino_cnt = fs->super->s_inodes_count;
		while (inode_nbytes > 0) {
			retval = io_channel_read_blk(fs->image_io, blk++,
						     1, inode_bitmap);
			if (retval)
				goto cleanup;
			cnt = fs->blocksize << 3;
			if (cnt > ino_cnt)
				cnt = ino_cnt;
			retval = ext2fs_set_inode_bitmap_range(fs->inode_map,
					       ino_itr, cnt, inode_bitmap);
			if (retval)
				goto cleanup;
			ino_itr += fs->blocksize << 3;
			ino_cnt -= fs->blocksize << 3;
			inode_nbytes -= fs->blocksize;
		}
		blk = (fs->image_header->offset_blockmap /
		       fs->blocksize);
		blk_cnt = EXT2_BLOCKS_PER_GROUP(fs->super) *
			fs->group_desc_count;
		while (block_nbytes > 0) {
			retval = io_channel_read_blk(fs->image_io, blk++,
						     1, block_bitmap);
			if (retval)
				goto cleanup;
			cnt = fs->blocksize << 3;
			if (cnt > blk_cnt)
				cnt = blk_cnt;
			retval = ext2fs_set_block_bitmap_range(fs->block_map,
				       blk_itr, cnt, block_bitmap);
			if (retval)
				goto cleanup;
			blk_itr += fs->blocksize << 3;
			blk_cnt -= fs->blocksize << 3;
			block_nbytes -= fs->blocksize;
		}
		goto success_cleanup;
	}

	for (i = 0; i < fs->group_desc_count; i++) {
		if (block_bitmap) {
			blk = fs->group_desc[i].bg_block_bitmap;
			if (csum_flag && fs->group_desc[i].bg_flags &
			    EXT2_BG_BLOCK_UNINIT &&
			    ext2fs_group_desc_csum_verify(fs, i))
				blk = 0;
			if (blk) {
				retval = io_channel_read_blk(fs->io, blk,
					     -block_nbytes, block_bitmap);
				if (retval) {
					retval = EXT2_ET_BLOCK_BITMAP_READ;
					goto cleanup;
				}
			} else
				memset(block_bitmap, 0, block_nbytes);
			cnt = block_nbytes << 3;
			retval = ext2fs_set_block_bitmap_range(fs->block_map,
					       blk_itr, cnt, block_bitmap);
			if (retval)
				goto cleanup;
			blk_itr += block_nbytes << 3;
		}
		if (inode_bitmap) {
			blk = fs->group_desc[i].bg_inode_bitmap;
			if (csum_flag && fs->group_desc[i].bg_flags &
			    EXT2_BG_INODE_UNINIT &&
			    ext2fs_group_desc_csum_verify(fs, i))
				blk = 0;
			if (blk) {
				retval = io_channel_read_blk(fs->io, blk,
					     -inode_nbytes, inode_bitmap);
				if (retval) {
					retval = EXT2_ET_INODE_BITMAP_READ;
					goto cleanup;
				}
			} else
				memset(inode_bitmap, 0, inode_nbytes);
			cnt = inode_nbytes << 3;
			retval = ext2fs_set_inode_bitmap_range(fs->inode_map,
					       ino_itr, cnt, inode_bitmap);
			if (retval)
				goto cleanup;
			ino_itr += inode_nbytes << 3;
		}
	}
success_cleanup:
	if (inode_bitmap)
		ext2fs_free_mem(&inode_bitmap);
	if (block_bitmap)
		ext2fs_free_mem(&block_bitmap);
	return 0;

cleanup:
	if (do_block) {
		ext2fs_free_mem(&fs->block_map);
		fs->block_map = 0;
	}
	if (do_inode) {
		ext2fs_free_mem(&fs->inode_map);
		fs->inode_map = 0;
	}
	if (inode_bitmap)
		ext2fs_free_mem(&inode_bitmap);
	if (block_bitmap)
		ext2fs_free_mem(&block_bitmap);
	if (buf)
		ext2fs_free_mem(&buf);
	return retval;
}

errcode_t ext2fs_read_inode_bitmap(ext2_filsys fs)
{
	return read_bitmaps(fs, 1, 0);
}

errcode_t ext2fs_read_block_bitmap(ext2_filsys fs)
{
	return read_bitmaps(fs, 0, 1);
}

errcode_t ext2fs_write_inode_bitmap(ext2_filsys fs)
{
	return write_bitmaps(fs, 1, 0);
}

errcode_t ext2fs_write_block_bitmap (ext2_filsys fs)
{
	return write_bitmaps(fs, 0, 1);
}

errcode_t ext2fs_read_bitmaps(ext2_filsys fs)
{
	if (fs->inode_map && fs->block_map)
		return 0;

	return read_bitmaps(fs, !fs->inode_map, !fs->block_map);
}

errcode_t ext2fs_write_bitmaps(ext2_filsys fs)
{
	int do_inode = fs->inode_map && ext2fs_test_ib_dirty(fs);
	int do_block = fs->block_map && ext2fs_test_bb_dirty(fs);

	if (!do_inode && !do_block)
		return 0;

	return write_bitmaps(fs, do_inode, do_block);
}
