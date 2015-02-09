/* NOMMU mmap support for RomFS on MTD devices
 *
 * Copyright © 2007 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/mm.h>
#include <linux/mtd/super.h>
#include "internal.h"

/*
 * try to determine where a shared mapping can be made
 * - only supported for NOMMU at the moment (MMU can't doesn't copy private
 *   mappings)
 * - attempts to map through to the underlying MTD device
 */
static unsigned long romfs_get_unmapped_area(struct file *file,
					     unsigned long addr,
					     unsigned long len,
					     unsigned long pgoff,
					     unsigned long flags)
{
	struct inode *inode = file->f_mapping->host;
	struct mtd_info *mtd = inode->i_sb->s_mtd;
	unsigned long isize, offset;

	if (!mtd)
		goto cant_map_directly;

	isize = i_size_read(inode);
	offset = pgoff << PAGE_SHIFT;
	if (offset > isize || len > isize || offset > isize - len)
		return (unsigned long) -EINVAL;

	/* we need to call down to the MTD layer to do the actual mapping */
	if (mtd->get_unmapped_area) {
		if (addr != 0)
			return (unsigned long) -EINVAL;

		if (len > mtd->size || pgoff >= (mtd->size >> PAGE_SHIFT))
			return (unsigned long) -EINVAL;

		offset += ROMFS_I(inode)->i_dataoffset;
		if (offset > mtd->size - len)
			return (unsigned long) -EINVAL;

		return mtd->get_unmapped_area(mtd, len, offset, flags);
	}

cant_map_directly:
	return (unsigned long) -ENOSYS;
}

/*
 * permit a R/O mapping to be made directly through onto an MTD device if
 * possible
 */
static int romfs_mmap(struct file *file, struct vm_area_struct *vma)
{
	return vma->vm_flags & (VM_SHARED | VM_MAYSHARE) ? 0 : -ENOSYS;
}

const struct file_operations romfs_ro_fops = {
	.llseek			= generic_file_llseek,
	.read			= do_sync_read,
	.aio_read		= generic_file_aio_read,
	.splice_read		= generic_file_splice_read,
	.mmap			= romfs_mmap,
	.get_unmapped_area	= romfs_get_unmapped_area,
};
