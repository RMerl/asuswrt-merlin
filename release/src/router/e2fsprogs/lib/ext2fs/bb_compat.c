/*
 * bb_compat.c --- compatibility badblocks routines
 *
 * Copyright (C) 1997 Theodore Ts'o.
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
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#include "ext2_fs.h"
#include "ext2fsP.h"

errcode_t badblocks_list_create(badblocks_list *ret, int size)
{
	return ext2fs_badblocks_list_create(ret, size);
}

void badblocks_list_free(badblocks_list bb)
{
	ext2fs_badblocks_list_free(bb);
}

errcode_t badblocks_list_add(badblocks_list bb, blk_t blk)
{
	return ext2fs_badblocks_list_add(bb, blk);
}

int badblocks_list_test(badblocks_list bb, blk_t blk)
{
	return ext2fs_badblocks_list_test(bb, blk);
}

errcode_t badblocks_list_iterate_begin(badblocks_list bb,
				       badblocks_iterate *ret)
{
	return ext2fs_badblocks_list_iterate_begin(bb, ret);
}

int badblocks_list_iterate(badblocks_iterate iter, blk_t *blk)
{
	return ext2fs_badblocks_list_iterate(iter, blk);
}

void badblocks_list_iterate_end(badblocks_iterate iter)
{
	ext2fs_badblocks_list_iterate_end(iter);
}
