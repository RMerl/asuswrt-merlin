/*
 * collate.h - Defines for NTFS collation handling.  Originated from the Linux-NTFS
 *             project.
 *
 * Copyright (c) 2004 Anton Altaparmakov
 * Copyright (c) 2005 Yura Pakhuchiy
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

#ifndef _NTFS_COLLATE_H
#define _NTFS_COLLATE_H

#include "types.h"
#include "volume.h"

#define NTFS_COLLATION_ERROR -2

extern COLLATE ntfs_get_collate_function(COLLATION_RULES);

#endif /* _NTFS_COLLATE_H */
