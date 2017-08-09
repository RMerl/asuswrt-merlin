/*
   Unix SMB/Netbios implementation.
   VFS module to get and set irix acls
   Copyright (C) Michael Adam 2006,2008

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

#include "includes.h"
#include "system/filesys.h"
#include "smbd/smbd.h"
#include "modules/vfs_irixacl.h"


/* prototypes for private functions first - for clarity */

/* public functions - the api */

SMB_ACL_T irixacl_sys_acl_get_file(vfs_handle_struct *handle,
				    const char *path_p,
				    SMB_ACL_TYPE_T type)
{
	errno = ENOTSUP;
	return NULL;
}

SMB_ACL_T irixacl_sys_acl_get_fd(vfs_handle_struct *handle,
				 files_struct *fsp)
{
	errno = ENOTSUP;
	return NULL;
}

int irixacl_sys_acl_set_file(vfs_handle_struct *handle,
			      const char *name,
			      SMB_ACL_TYPE_T type,
			      SMB_ACL_T theacl)
{
	errno = ENOTSUP;
	return -1;
}

int irixacl_sys_acl_set_fd(vfs_handle_struct *handle,
			    files_struct *fsp,
			    SMB_ACL_T theacl)
{
	errno = ENOTSUP;
	return -1;
}

int irixacl_sys_acl_delete_def_file(vfs_handle_struct *handle,
				     const char *path)
{
	errno = ENOTSUP;
	return -1;
}

/* private functions */

/* VFS operations structure */

static struct vfs_fn_pointers irixacl_fns = {
	.sys_acl_get_file = irixacl_sys_acl_get_file,
	.sys_acl_get_fd = irixacl_sys_acl_get_fd,
	.sys_acl_set_file = irixacl_sys_acl_set_file,
	.sys_acl_set_fd = irixacl_sys_acl_set_fd,
	.sys_acl_delete_def_file = irixacl_sys_acl_delete_def_file,
};

NTSTATUS vfs_irixacl_init(void);
NTSTATUS vfs_irixacl_init(void)
{
	return smb_register_vfs(SMB_VFS_INTERFACE_VERSION, "irixacl",
				&irixacl_fns);
}

/* ENTE */
