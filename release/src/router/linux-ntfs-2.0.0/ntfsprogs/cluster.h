/*
 * cluster - Part of the Linux-NTFS project.
 *
 * Copyright (c) 2003 Richard Russon
 *
 * This function will locate the owner of any given sector or cluster range.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the main directory of the Linux-NTFS
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _CLUSTER_H_
#define _CLUSTER_H_

#include "types.h"
#include "volume.h"

typedef struct {
	int x;
} ntfs_cluster;

typedef int (cluster_cb)(ntfs_inode *ino, ATTR_RECORD *attr, runlist_element *run, void *data);

int cluster_find(ntfs_volume *vol, LCN c_begin, LCN c_end, cluster_cb *cb, void *data);

#endif /* _CLUSTER_H_ */

