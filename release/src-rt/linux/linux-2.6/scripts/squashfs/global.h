#ifndef GLOBAL_H 
#define GLOBAL_H

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
 * global.h
 */

typedef struct squashfs_super_block squashfs_super_block;
typedef struct squashfs_dir_index squashfs_dir_index;
typedef struct squashfs_base_inode_header squashfs_base_inode_header;
typedef struct squashfs_ipc_inode_header squashfs_ipc_inode_header;
typedef struct squashfs_dev_inode_header squashfs_dev_inode_header;
typedef struct squashfs_symlink_inode_header squashfs_symlink_inode_header;
typedef struct squashfs_reg_inode_header squashfs_reg_inode_header;
typedef struct squashfs_lreg_inode_header squashfs_lreg_inode_header;
typedef struct squashfs_dir_inode_header squashfs_dir_inode_header;
typedef struct squashfs_ldir_inode_header squashfs_ldir_inode_header;
typedef struct squashfs_dir_entry squashfs_dir_entry;
typedef struct squashfs_dir_header squashfs_dir_header;
typedef struct squashfs_fragment_entry squashfs_fragment_entry;
typedef union squashfs_inode_header squashfs_inode_header;

#ifdef CONFIG_SQUASHFS_2_0_COMPATIBILITY
typedef struct squashfs_dir_index_2 squashfs_dir_index_2;
typedef struct squashfs_base_inode_header_2 squashfs_base_inode_header_2;
typedef struct squashfs_ipc_inode_header_2 squashfs_ipc_inode_header_2;
typedef struct squashfs_dev_inode_header_2 squashfs_dev_inode_header_2;
typedef struct squashfs_symlink_inode_header_2 squashfs_symlink_inode_header_2;
typedef struct squashfs_reg_inode_header_2 squashfs_reg_inode_header_2;
typedef struct squashfs_lreg_inode_header_2 squashfs_lreg_inode_header_2;
typedef struct squashfs_dir_inode_header_2 squashfs_dir_inode_header_2;
typedef struct squashfs_ldir_inode_header_2 squashfs_ldir_inode_header_2;
typedef struct squashfs_dir_entry_2 squashfs_dir_entry_2;
typedef struct squashfs_dir_header_2 squashfs_dir_header_2;
typedef struct squashfs_fragment_entry_2 squashfs_fragment_entry_2;
typedef union squashfs_inode_header_2 squashfs_inode_header_2;
#endif

typedef unsigned int squashfs_uid;
typedef long long squashfs_fragment_index;
typedef squashfs_inode_t squashfs_inode;
typedef squashfs_block_t squashfs_block;

#endif
