/*
 * Unix SMB/Netbios implementation.
 * VFS module to get and set HP-UX ACLs - prototype header
 * Copyright (C) Michael Adam 2008
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/*
 * This module supports JFS (POSIX) ACLs on VxFS (Veritas * Filesystem).
 * These are available on HP-UX 11.00 if JFS 3.3 is installed.
 * On HP-UX 11i (11.11 and above) these ACLs are supported out of
 * the box.
 *
 * There is another form of ACLs on HFS. These ACLs have a
 * completely different API and their own set of userland tools.
 * Since HFS seems to be considered deprecated, HFS acls
 * are not supported. (They could be supported through a separate
 * vfs-module if there is demand.)
 */

#ifndef __VFS_HPUXACL_H__
#define __VFS_HPUXACL_H__

SMB_ACL_T hpuxacl_sys_acl_get_file(vfs_handle_struct *handle,
				   const char *path_p,
				   SMB_ACL_TYPE_T type);

SMB_ACL_T hpuxacl_sys_acl_get_fd(vfs_handle_struct *handle,
				 files_struct *fsp);

int hpuxacl_sys_acl_set_file(vfs_handle_struct *handle,
			     const char *name,
			     SMB_ACL_TYPE_T type,
			     SMB_ACL_T theacl);

int hpuxacl_sys_acl_set_fd(vfs_handle_struct *handle,
			   files_struct *fsp,
			   SMB_ACL_T theacl);

int hpuxacl_sys_acl_delete_def_file(vfs_handle_struct *handle,
				    const char *path);

NTSTATUS vfs_hpuxacl_init(void);

#endif

