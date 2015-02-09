/*
 *   Copyright (C) International Business Machines  Corp., 2002
 *
 *   This program is free software;  you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY;  without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program;  if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#ifndef _H_JFS_ACL
#define _H_JFS_ACL

#ifdef CONFIG_JFS_POSIX_ACL

int jfs_check_acl(struct inode *, int);
int jfs_init_acl(tid_t, struct inode *, struct inode *);
int jfs_acl_chmod(struct inode *inode);

#else

static inline int jfs_init_acl(tid_t tid, struct inode *inode,
			       struct inode *dir)
{
	return 0;
}

static inline int jfs_acl_chmod(struct inode *inode)
{
	return 0;
}

#endif
#endif		/* _H_JFS_ACL */
