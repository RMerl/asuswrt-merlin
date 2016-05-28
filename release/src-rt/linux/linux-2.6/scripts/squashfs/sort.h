#ifndef SORT_H 
#define SORT_H

/*
 * Squashfs
 *
 * Copyright (c) 2002, 2003, 2004, 2005, 2006, 2007
 * Phillip Lougher <phillip@lougher.org.uk>
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
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * sort.h
 */

struct dir_info {
	char			*pathname;
	unsigned int		count;
	unsigned int		directory_count;
	unsigned int		current_count;
	unsigned int		byte_count;
	char			dir_is_ldir;
	struct dir_ent		*dir_ent;
	struct dir_ent		**list;
	DIR			*linuxdir;
};

struct dir_ent {
	char			*name;
	char			*pathname;
	struct inode_info	*inode;
	struct dir_info		*dir;
	struct dir_info		*our_dir;
	struct old_root_entry_info *data;
};

struct inode_info {
	unsigned int		nlink;
	struct stat		buf;
	squashfs_inode		inode;
	unsigned int		type;
	unsigned int		inode_number;
	char			read;
	struct inode_info	*next;
};

struct priority_entry {
	struct dir_ent *dir;
	struct priority_entry *next;
};
#endif
