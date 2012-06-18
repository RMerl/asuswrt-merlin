/*
 *
 * Copyright (c) 2009 Martin Bene
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
 * along with this program (in the main directory of the NTFS-3G
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef EFS_H
#define EFS_H

int ntfs_get_efs_info(ntfs_inode *ni, char *value, size_t size);

int ntfs_set_efs_info(ntfs_inode *ni,
			const char *value, size_t size,	int flags);
int ntfs_efs_fixup_attribute(ntfs_attr_search_ctx *ctx, ntfs_attr *na);

#endif /* EFS_H */
