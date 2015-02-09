/* Modified by Broadcom Corp. Portions Copyright (c) Broadcom Corp, 2012. */
/*
 * Squashfs - a compressed read only filesystem for Linux
 *
 * Copyright (c) 2010
 * Phillip Lougher <phillip@lougher.demon.co.uk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * xattr_id.c
 */

/*
 * This file implements code to map the 32-bit xattr id stored in the inode
 * into the on disk location of the xattr data.
 */

#include <linux/fs.h>
#include <linux/vfs.h>
#include <linux/slab.h>

#include "squashfs_fs.h"
#include "squashfs_fs_sb.h"
#include "squashfs.h"
#include "xattr.h"

/*
 * Map xattr id using the xattr id look up table
 */
int squashfs_xattr_lookup(struct super_block *sb, unsigned int index,
		int *count, unsigned int *size, unsigned long long *xattr)
{
	struct squashfs_sb_info *msblk = sb->s_fs_info;
	int block = SQUASHFS_XATTR_BLOCK(index);
	int offset = SQUASHFS_XATTR_BLOCK_OFFSET(index);
	u64 start_block = le64_to_cpu(msblk->xattr_id_table[block]);
	struct squashfs_xattr_id id;
	int err;

	err = squashfs_read_metadata(sb, &id, &start_block, &offset,
							sizeof(id));
	if (err < 0)
		return err;

	*xattr = le64_to_cpu(id.xattr);
	*size = le32_to_cpu(id.size);
	*count = le32_to_cpu(id.count);
	return 0;
}


/*
 * Read uncompressed xattr id lookup table indexes from disk into memory
 */
__le64 *squashfs_read_xattr_id_table(struct super_block *sb, u64 start,
		u64 *xattr_table_start, int *xattr_ids)
{
	unsigned int len;
	__le64 *xid_table;
	struct squashfs_xattr_id_table id_table;
	int err;

	err = squashfs_read_table(sb, &id_table, start, sizeof(id_table));
	if (err < 0) {
		ERROR("unable to read xattr id table\n");
		return ERR_PTR(err);
	}
	*xattr_table_start = le64_to_cpu(id_table.xattr_table_start);
	*xattr_ids = le32_to_cpu(id_table.xattr_ids);
	len = SQUASHFS_XATTR_BLOCK_BYTES(*xattr_ids);

	TRACE("In read_xattr_index_table, length %d\n", len);

	/* Allocate xattr id lookup table indexes */
	xid_table = kmalloc(len, GFP_KERNEL);
	if (xid_table == NULL) {
		ERROR("Failed to allocate xattr id index table\n");
		return ERR_PTR(-ENOMEM);
	}

	err = squashfs_read_table(sb, xid_table, start + sizeof(id_table), len);
	if (err < 0) {
		ERROR("unable to read xattr id index table\n");
		kfree(xid_table);
		return ERR_PTR(err);
	}

	return xid_table;
}
