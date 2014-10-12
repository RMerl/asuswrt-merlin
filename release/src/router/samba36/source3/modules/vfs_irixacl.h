/*
   Unix SMB/Netbios implementation.
   VFS module to get and set irix acls - prototype header
   Copyright (C) Michael Adam 2008

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __VFS_IRIXACL_H__
#define __VFS_IRIXACL_H__

SMB_ACL_T irixacl_sys_acl_get_file(vfs_handle_struct *handle,
				   const char *path_p,
				   SMB_ACL_TYPE_T type);

SMB_ACL_T irixacl_sys_acl_get_fd(vfs_handle_struct *handle,
				 files_struct *fsp);

int irixacl_sys_acl_set_file(vfs_handle_struct *handle,
			     const char *name,
			     SMB_ACL_TYPE_T type,
			     SMB_ACL_T theacl);

int irixacl_sys_acl_set_fd(vfs_handle_struct *handle,
			   files_struct *fsp,
			   SMB_ACL_T theacl);

int irixacl_sys_acl_delete_def_file(vfs_handle_struct *handle,
				    const char *path);

NTSTATUS vfs_irixacl_init(void);

#endif

