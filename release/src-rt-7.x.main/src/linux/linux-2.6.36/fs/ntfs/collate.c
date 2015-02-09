/*
 * collate.c - NTFS kernel collation handling.  Part of the Linux-NTFS project.
 *
 * Copyright (c) 2004 Anton Altaparmakov
 *
 * This program/include file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program/include file is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the main directory of the Linux-NTFS
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "collate.h"
#include "debug.h"
#include "ntfs.h"

static int ntfs_collate_binary(ntfs_volume *vol,
		const void *data1, const int data1_len,
		const void *data2, const int data2_len)
{
	int rc;

	ntfs_debug("Entering.");
	rc = memcmp(data1, data2, min(data1_len, data2_len));
	if (!rc && (data1_len != data2_len)) {
		if (data1_len < data2_len)
			rc = -1;
		else
			rc = 1;
	}
	ntfs_debug("Done, returning %i", rc);
	return rc;
}

static int ntfs_collate_ntofs_ulong(ntfs_volume *vol,
		const void *data1, const int data1_len,
		const void *data2, const int data2_len)
{
	int rc;
	u32 d1, d2;

	ntfs_debug("Entering.");
	BUG_ON(data1_len != data2_len);
	BUG_ON(data1_len != 4);
	d1 = le32_to_cpup(data1);
	d2 = le32_to_cpup(data2);
	if (d1 < d2)
		rc = -1;
	else {
		if (d1 == d2)
			rc = 0;
		else
			rc = 1;
	}
	ntfs_debug("Done, returning %i", rc);
	return rc;
}

typedef int (*ntfs_collate_func_t)(ntfs_volume *, const void *, const int,
		const void *, const int);

static ntfs_collate_func_t ntfs_do_collate0x0[3] = {
	ntfs_collate_binary,
	NULL/*ntfs_collate_file_name*/,
	NULL/*ntfs_collate_unicode_string*/,
};

static ntfs_collate_func_t ntfs_do_collate0x1[4] = {
	ntfs_collate_ntofs_ulong,
	NULL/*ntfs_collate_ntofs_sid*/,
	NULL/*ntfs_collate_ntofs_security_hash*/,
	NULL/*ntfs_collate_ntofs_ulongs*/,
};

/**
 * ntfs_collate - collate two data items using a specified collation rule
 * @vol:	ntfs volume to which the data items belong
 * @cr:		collation rule to use when comparing the items
 * @data1:	first data item to collate
 * @data1_len:	length in bytes of @data1
 * @data2:	second data item to collate
 * @data2_len:	length in bytes of @data2
 *
 * Collate the two data items @data1 and @data2 using the collation rule @cr
 * and return -1, 0, ir 1 if @data1 is found, respectively, to collate before,
 * to match, or to collate after @data2.
 *
 * For speed we use the collation rule @cr as an index into two tables of
 * function pointers to call the appropriate collation function.
 */
int ntfs_collate(ntfs_volume *vol, COLLATION_RULE cr,
		const void *data1, const int data1_len,
		const void *data2, const int data2_len) {
	int i;

	ntfs_debug("Entering.");
	BUG_ON(cr != COLLATION_BINARY && cr != COLLATION_NTOFS_ULONG);
	i = le32_to_cpu(cr);
	BUG_ON(i < 0);
	if (i <= 0x02)
		return ntfs_do_collate0x0[i](vol, data1, data1_len,
				data2, data2_len);
	BUG_ON(i < 0x10);
	i -= 0x10;
	if (likely(i <= 3))
		return ntfs_do_collate0x1[i](vol, data1, data1_len,
				data2, data2_len);
	BUG();
	return 0;
}
