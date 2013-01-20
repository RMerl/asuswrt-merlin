/* vi: set sw=4 ts=4: */
/*
 * rs_bitmap.c --- routine for changing the size of a bitmap
 *
 * Copyright (C) 1996, 1997 Theodore Ts'o.
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

errcode_t ext2fs_resize_generic_bitmap(__u32 new_end, __u32 new_real_end,
				       ext2fs_generic_bitmap bmap)
{
	errcode_t	retval;
	size_t		size, new_size;
	__u32		bitno;

	if (!bmap)
		return EXT2_ET_INVALID_ARGUMENT;

	EXT2_CHECK_MAGIC(bmap, EXT2_ET_MAGIC_GENERIC_BITMAP);

	/*
	 * If we're expanding the bitmap, make sure all of the new
	 * parts of the bitmap are zero.
	 */
	if (new_end > bmap->end) {
		bitno = bmap->real_end;
		if (bitno > new_end)
			bitno = new_end;
		for (; bitno > bmap->end; bitno--)
			ext2fs_clear_bit(bitno - bmap->start, bmap->bitmap);
	}
	if (new_real_end == bmap->real_end) {
		bmap->end = new_end;
		return 0;
	}

	size = ((bmap->real_end - bmap->start) / 8) + 1;
	new_size = ((new_real_end - bmap->start) / 8) + 1;

	if (size != new_size) {
		retval = ext2fs_resize_mem(size, new_size, &bmap->bitmap);
		if (retval)
			return retval;
	}
	if (new_size > size)
		memset(bmap->bitmap + size, 0, new_size - size);

	bmap->end = new_end;
	bmap->real_end = new_real_end;
	return 0;
}

errcode_t ext2fs_resize_inode_bitmap(__u32 new_end, __u32 new_real_end,
				     ext2fs_inode_bitmap bmap)
{
	errcode_t	retval;

	if (!bmap)
		return EXT2_ET_INVALID_ARGUMENT;

	EXT2_CHECK_MAGIC(bmap, EXT2_ET_MAGIC_INODE_BITMAP);

	bmap->magic = EXT2_ET_MAGIC_GENERIC_BITMAP;
	retval = ext2fs_resize_generic_bitmap(new_end, new_real_end,
					      bmap);
	bmap->magic = EXT2_ET_MAGIC_INODE_BITMAP;
	return retval;
}

errcode_t ext2fs_resize_block_bitmap(__u32 new_end, __u32 new_real_end,
				     ext2fs_block_bitmap bmap)
{
	errcode_t	retval;

	if (!bmap)
		return EXT2_ET_INVALID_ARGUMENT;

	EXT2_CHECK_MAGIC(bmap, EXT2_ET_MAGIC_BLOCK_BITMAP);

	bmap->magic = EXT2_ET_MAGIC_GENERIC_BITMAP;
	retval = ext2fs_resize_generic_bitmap(new_end, new_real_end,
					      bmap);
	bmap->magic = EXT2_ET_MAGIC_BLOCK_BITMAP;
	return retval;
}
