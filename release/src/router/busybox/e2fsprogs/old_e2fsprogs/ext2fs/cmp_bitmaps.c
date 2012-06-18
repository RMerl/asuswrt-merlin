/* vi: set sw=4 ts=4: */
/*
 * cmp_bitmaps.c --- routines to compare inode and block bitmaps.
 *
 * Copyright (C) 1995 Theodore Ts'o.
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
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#include "ext2_fs.h"
#include "ext2fs.h"

errcode_t ext2fs_compare_block_bitmap(ext2fs_block_bitmap bm1,
				      ext2fs_block_bitmap bm2)
{
	blk_t	i;

	EXT2_CHECK_MAGIC(bm1, EXT2_ET_MAGIC_BLOCK_BITMAP);
	EXT2_CHECK_MAGIC(bm2, EXT2_ET_MAGIC_BLOCK_BITMAP);

	if ((bm1->start != bm2->start) ||
	    (bm1->end != bm2->end) ||
	    (memcmp(bm1->bitmap, bm2->bitmap,
		    (size_t) (bm1->end - bm1->start)/8)))
		return EXT2_ET_NEQ_BLOCK_BITMAP;

	for (i = bm1->end - ((bm1->end - bm1->start) % 8); i <= bm1->end; i++)
		if (ext2fs_fast_test_block_bitmap(bm1, i) !=
		    ext2fs_fast_test_block_bitmap(bm2, i))
			return EXT2_ET_NEQ_BLOCK_BITMAP;

	return 0;
}

errcode_t ext2fs_compare_inode_bitmap(ext2fs_inode_bitmap bm1,
				      ext2fs_inode_bitmap bm2)
{
	ext2_ino_t	i;

	EXT2_CHECK_MAGIC(bm1, EXT2_ET_MAGIC_INODE_BITMAP);
	EXT2_CHECK_MAGIC(bm2, EXT2_ET_MAGIC_INODE_BITMAP);

	if ((bm1->start != bm2->start) ||
	    (bm1->end != bm2->end) ||
	    (memcmp(bm1->bitmap, bm2->bitmap,
		    (size_t) (bm1->end - bm1->start)/8)))
		return EXT2_ET_NEQ_INODE_BITMAP;

	for (i = bm1->end - ((bm1->end - bm1->start) % 8); i <= bm1->end; i++)
		if (ext2fs_fast_test_inode_bitmap(bm1, i) !=
		    ext2fs_fast_test_inode_bitmap(bm2, i))
			return EXT2_ET_NEQ_INODE_BITMAP;

	return 0;
}

