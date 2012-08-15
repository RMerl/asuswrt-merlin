/*
 * bootsect.h - Exports for bootsector record handling. Part of the Linux-NTFS
 *		project.
 *
 * Copyright (c) 2000-2002 Anton Altaparmakov
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

#ifndef _NTFS_BOOTSECT_H
#define _NTFS_BOOTSECT_H

#include "types.h"
#include "volume.h"
#include "layout.h"

/**
 * is_boot_sector_ntfs - check a boot sector for describing an ntfs volume
 * @b:		buffer containing the boot sector
 * @silent:	if 1 don't display progress information
 *
 * This function checks the boot sector in @b for describing a valid ntfs
 * volume. Return TRUE if @b is a valid NTFS boot sector or FALSE otherwise.
 * If silent is FALSE, progress output will be output to stdout. If silent is
 * TRUE no output to stdout will occur. Errors/warnings to stderr will occur
 * disregarding the value of silent (but only if configure was run with
 * --enable-debug).
 */
extern BOOL ntfs_boot_sector_is_ntfs(NTFS_BOOT_SECTOR *b, BOOL silent);
extern int ntfs_boot_sector_parse(ntfs_volume *vol,
		const NTFS_BOOT_SECTOR *bs);

#endif /* defined _NTFS_BOOTSECT_H */

