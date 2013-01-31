/* vi: set sw=4 ts=4: */
/*
 * gen_bitmap.c --- Generic bitmap routines that used to be inlined.
 *
 * Copyright (C) 2001 Theodore Ts'o.
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

int ext2fs_mark_generic_bitmap(ext2fs_generic_bitmap bitmap,
					 __u32 bitno)
{
	if ((bitno < bitmap->start) || (bitno > bitmap->end)) {
		ext2fs_warn_bitmap2(bitmap, EXT2FS_MARK_ERROR, bitno);
		return 0;
	}
	return ext2fs_set_bit(bitno - bitmap->start, bitmap->bitmap);
}

int ext2fs_unmark_generic_bitmap(ext2fs_generic_bitmap bitmap,
					   blk_t bitno)
{
	if ((bitno < bitmap->start) || (bitno > bitmap->end)) {
		ext2fs_warn_bitmap2(bitmap, EXT2FS_UNMARK_ERROR, bitno);
		return 0;
	}
	return ext2fs_clear_bit(bitno - bitmap->start, bitmap->bitmap);
}
