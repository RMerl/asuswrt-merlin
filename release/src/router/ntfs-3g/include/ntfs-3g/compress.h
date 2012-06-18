/*
 * compress.h - Exports for compressed attribute handling. 
 * 		Originated from the Linux-NTFS project.
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
 * along with this program (in the main directory of the NTFS-3G
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _NTFS_COMPRESS_H
#define _NTFS_COMPRESS_H

#include "types.h"
#include "attrib.h"

extern s64 ntfs_compressed_attr_pread(ntfs_attr *na, s64 pos, s64 count,
		void *b);

extern s64 ntfs_compressed_pwrite(ntfs_attr *na, runlist_element *brl, s64 wpos,
				s64 offs, s64 to_write, s64 rounded,
				const void *b, int compressed_part,
				VCN *update_from);

extern int ntfs_compressed_close(ntfs_attr *na, runlist_element *brl,
				s64 offs, VCN *update_from);

#endif /* defined _NTFS_COMPRESS_H */

