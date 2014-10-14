/*
 * Copyright (c) 2000-2003,2005 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write the Free Software Foundation,
 * Inc.,  51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_types.h"
#include "xfs_log.h"
#include "xfs_inum.h"
#include "xfs_trans.h"
#include "xfs_sb.h"
#include "xfs_ag.h"
#include "xfs_dir2.h"
#include "xfs_dmapi.h"
#include "xfs_mount.h"

xfs_dmops_t	xfs_dmcore_stub = {
	.xfs_send_data		= (xfs_send_data_t)fs_nosys,
	.xfs_send_mmap		= (xfs_send_mmap_t)fs_noerr,
	.xfs_send_destroy	= (xfs_send_destroy_t)fs_nosys,
	.xfs_send_namesp	= (xfs_send_namesp_t)fs_nosys,
	.xfs_send_unmount	= (xfs_send_unmount_t)fs_noval,
};
