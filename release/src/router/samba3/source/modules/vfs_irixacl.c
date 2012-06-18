/*
   Unix SMB/Netbios implementation.
   VFS module to get and set irix acls
   Copyright (C) Michael Adam 2006

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "includes.h"

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
				  files_struct *fsp,
				  int fd)
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
			    int fd, SMB_ACL_T theacl)
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

static vfs_op_tuple irixacl_op_tuples[] = {
	/* Disk operations */
  {SMB_VFS_OP(irixacl_sys_acl_get_file),
   SMB_VFS_OP_SYS_ACL_GET_FILE,
   SMB_VFS_LAYER_TRANSPARENT},

  {SMB_VFS_OP(irixacl_sys_acl_get_fd),
   SMB_VFS_OP_SYS_ACL_GET_FD,
   SMB_VFS_LAYER_TRANSPARENT},

  {SMB_VFS_OP(irixacl_sys_acl_set_file),
   SMB_VFS_OP_SYS_ACL_SET_FILE,
   SMB_VFS_LAYER_TRANSPARENT},

  {SMB_VFS_OP(irixacl_sys_acl_set_fd),
   SMB_VFS_OP_SYS_ACL_SET_FD,
   SMB_VFS_LAYER_TRANSPARENT},

  {SMB_VFS_OP(irixacl_sys_acl_delete_def_file),
   SMB_VFS_OP_SYS_ACL_DELETE_DEF_FILE,
   SMB_VFS_LAYER_TRANSPARENT},

  {SMB_VFS_OP(NULL),
   SMB_VFS_OP_NOOP,
   SMB_VFS_LAYER_NOOP}
};

NTSTATUS vfs_irixacl_init(void);
NTSTATUS vfs_irixacl_init(void)
{
	return smb_register_vfs(SMB_VFS_INTERFACE_VERSION, "irixacl",
				irixacl_op_tuples);
}

/* ENTE */
