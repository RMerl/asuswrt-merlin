/*
 * ntfscluster - Part of the Linux-NTFS project.
 *
 * Copyright (c) 2002-2003 Richard Russon
 *
 * This utility will locate the owner of any given sector or cluster.
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

#ifndef _NTFSCLUSTER_H_
#define _NTFSCLUSTER_H_

#include "types.h"
#include "layout.h"

enum action {
	act_none,
	act_info,
	act_cluster,
	act_sector,
	act_inode,
	act_file,
	act_last,
	act_error,
};

struct options {
	char		*device;	/* Device/File to work with */
	enum action	 action;	/* What to do */
	int		 quiet;		/* Less output */
	int		 verbose;	/* Extra output */
	int		 force;		/* Override common sense */
	char		*filename;	/* File to examine */
	u64		 inode;		/* Inode to examine */
	s64		 range_begin;	/* Look for objects in this range */
	s64		 range_end;
};

struct match {
	u64		 inum;		/* Inode number */
	LCN		 lcn;		/* Last cluster in use */
	ATTR_TYPES	 type;		/* Attribute type */
	ntfschar	*name;		/* Attribute name */
	int		 name_len;	/* Length of attribute name */
};

#endif /* _NTFSCLUSTER_H_ */


