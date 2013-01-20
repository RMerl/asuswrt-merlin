/* vi: set sw=4 ts=4: */
/*
 * sparse.c --- find the groups in an ext2 filesystem with metadata backups
 *
 * Copyright (C) 1993, 1994, 1995, 1996 Theodore Ts'o.
 * Copyright (C) 2002 Andreas Dilger.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include <stdio.h>

#include "ext2_fs.h"
#include "ext2fsP.h"

static int test_root(int a, int b)
{
	if (a == 0)
		return 1;
	while (1) {
		if (a == 1)
			return 1;
		if (a % b)
			return 0;
		a = a / b;
	}
}

int ext2fs_bg_has_super(ext2_filsys fs, int group_block)
{
	if (!(fs->super->s_feature_ro_compat &
	      EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER))
		return 1;

	if (test_root(group_block, 3) || (test_root(group_block, 5)) ||
	    test_root(group_block, 7))
		return 1;

	return 0;
}

/*
 * Iterate through the groups which hold BACKUP superblock/GDT copies in an
 * ext3 filesystem.  The counters should be initialized to 1, 5, and 7 before
 * calling this for the first time.  In a sparse filesystem it will be the
 * sequence of powers of 3, 5, and 7: 1, 3, 5, 7, 9, 25, 27, 49, 81, ...
 * For a non-sparse filesystem it will be every group: 1, 2, 3, 4, ...
 */
unsigned int ext2fs_list_backups(ext2_filsys fs, unsigned int *three,
				 unsigned int *five, unsigned int *seven)
{
	unsigned int *min = three;
	int mult = 3;
	unsigned int ret;

	if (!(fs->super->s_feature_ro_compat &
	      EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER)) {
		ret = *min;
		*min += 1;
		return ret;
	}

	if (*five < *min) {
		min = five;
		mult = 5;
	}
	if (*seven < *min) {
		min = seven;
		mult = 7;
	}

	ret = *min;
	*min *= mult;

	return ret;
}
