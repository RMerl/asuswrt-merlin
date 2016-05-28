/*
   Unix SMB/Netbios implementation.
   VFS module to get and set posix acls
   Copyright (C) Jim McDonough <jmcd@us.ibm.com> 2006

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

extern SMB_ACL_T aixacl_to_smbacl( struct acl *file_acl);
extern struct acl *aixacl_smb_to_aixacl(SMB_ACL_TYPE_T acltype, SMB_ACL_T theacl);

SMB_ACL_T aixacl_sys_acl_get_file(vfs_handle_struct *handle,
				    const char *path_p,
				    SMB_ACL_TYPE_T type)
{
	struct acl *file_acl = (struct acl *)NULL;
	struct smb_acl_t *result = (struct smb_acl_t *)NULL;
	
	int rc = 0;
	uid_t user_id;

	/* AIX has no DEFAULT */
	if  ( type == SMB_ACL_TYPE_DEFAULT )
		return NULL;

	/* Get the acl using statacl */
 
	DEBUG(10,("Entering AIX sys_acl_get_file\n"));
	DEBUG(10,("path_p is %s\n",path_p));

	file_acl = (struct acl *)SMB_MALLOC(BUFSIZ);
 
	if(file_acl == NULL) {
		errno=ENOMEM;
		DEBUG(0,("Error in AIX sys_acl_get_file: %d\n",errno));
		return(NULL);
	}

	memset(file_acl,0,BUFSIZ);

	rc = statacl((char *)path_p,0,file_acl,BUFSIZ);
	if( (rc == -1) && (errno == ENOSPC)) {
		struct acl *new_acl = SMB_MALLOC(file_acl->acl_len + sizeof(struct acl));
		if( new_acl == NULL) {
			SAFE_FREE(file_acl);
			errno = ENOMEM;
			return NULL;
		}
		file_acl = new_acl;
		rc = statacl((char *)path_p,0,file_acl,file_acl->acl_len+sizeof(struct acl));
		if( rc == -1) {
			DEBUG(0,("statacl returned %d with errno %d\n",rc,errno));
			SAFE_FREE(file_acl);
			return(NULL);
		}
	}

	DEBUG(10,("Got facl and returned it\n"));

	
	result = aixacl_to_smbacl(file_acl);
	SAFE_FREE(file_acl);
	return result;
	
	/*errno = ENOTSUP;
	return NULL;*/
}

SMB_ACL_T aixacl_sys_acl_get_fd(vfs_handle_struct *handle,
				  files_struct *fsp,
				  int fd)
{

	struct acl *file_acl = (struct acl *)NULL;
	struct smb_acl_t *result = (struct smb_acl_t *)NULL;
	
	int rc = 0;
	uid_t user_id;

	/* Get the acl using fstatacl */
   
	DEBUG(10,("Entering AIX sys_acl_get_fd\n"));
	DEBUG(10,("fd is %d\n",fd));
	file_acl = (struct acl *)SMB_MALLOC(BUFSIZ);

	if(file_acl == NULL) {
		errno=ENOMEM;
		DEBUG(0,("Error in AIX sys_acl_get_fd is %d\n",errno));
		return(NULL);
	}

	memset(file_acl,0,BUFSIZ);

	rc = fstatacl(fd,0,file_acl,BUFSIZ);
	if( (rc == -1) && (errno == ENOSPC)) {
		struct acl *new_acl = SMB_MALLOC(file_acl->acl_len + sizeof(struct acl));
		if( new_acl == NULL) {
			SAFE_FREE(file_acl);
			errno = ENOMEM;
			return NULL;
		}
		file_acl = new_acl;
		rc = fstatacl(fd,0,file_acl,file_acl->acl_len + sizeof(struct acl));
		if( rc == -1) {
			DEBUG(0,("fstatacl returned %d with errno %d\n",rc,errno));
			SAFE_FREE(file_acl);
			return(NULL);
		}
	}

	DEBUG(10,("Got facl and returned it\n"));

	result = aixacl_to_smbacl(file_acl);
	SAFE_FREE(file_acl);
	return result;
	
	/*errno = ENOTSUP;
	return NULL;*/
}

int aixacl_sys_acl_set_file(vfs_handle_struct *handle,
			      const char *name,
			      SMB_ACL_TYPE_T type,
			      SMB_ACL_T theacl)
{
	struct acl *file_acl = NULL;
	uint rc;
	
	file_acl = aixacl_smb_to_aixacl(type, theacl);
	if (!file_acl)
		return -1;

	rc = chacl((char *)name,file_acl,file_acl->acl_len);
	DEBUG(10,("errno is %d\n",errno));
	DEBUG(10,("return code is %d\n",rc));
	SAFE_FREE(file_acl);
	DEBUG(10,("Exiting the aixacl_sys_acl_set_file\n"));

	return rc;
}

int aixacl_sys_acl_set_fd(vfs_handle_struct *handle,
			    files_struct *fsp,
			    int fd, SMB_ACL_T theacl)
{
	struct acl *file_acl = NULL;
	uint rc;

	file_acl = aixacl_smb_to_aixacl(SMB_ACL_TYPE_ACCESS, theacl);
	if (!file_acl)
		return -1;

	rc = fchacl(fd,file_acl,file_acl->acl_len);
	DEBUG(10,("errno is %d\n",errno));
	DEBUG(10,("return code is %d\n",rc));
	SAFE_FREE(file_acl);
	DEBUG(10,("Exiting aixacl_sys_acl_set_fd\n"));

	return rc;
}

int aixacl_sys_acl_delete_def_file(vfs_handle_struct *handle,
				     const char *path)
{
	return 0; /* otherwise you can't set acl at upper level */
}

/* VFS operations structure */

static vfs_op_tuple aixacl_op_tuples[] = {
	/* Disk operations */
  {SMB_VFS_OP(aixacl_sys_acl_get_file),
   SMB_VFS_OP_SYS_ACL_GET_FILE,
   SMB_VFS_LAYER_TRANSPARENT},

  {SMB_VFS_OP(aixacl_sys_acl_get_fd),
   SMB_VFS_OP_SYS_ACL_GET_FD,
   SMB_VFS_LAYER_TRANSPARENT},

  {SMB_VFS_OP(aixacl_sys_acl_set_file),
   SMB_VFS_OP_SYS_ACL_SET_FILE,
   SMB_VFS_LAYER_TRANSPARENT},

  {SMB_VFS_OP(aixacl_sys_acl_set_fd),
   SMB_VFS_OP_SYS_ACL_SET_FD,
   SMB_VFS_LAYER_TRANSPARENT},

  {SMB_VFS_OP(aixacl_sys_acl_delete_def_file),
   SMB_VFS_OP_SYS_ACL_DELETE_DEF_FILE,
   SMB_VFS_LAYER_TRANSPARENT},

  {SMB_VFS_OP(NULL),
   SMB_VFS_OP_NOOP,
   SMB_VFS_LAYER_NOOP}
};

NTSTATUS vfs_aixacl_init(void);
NTSTATUS vfs_aixacl_init(void)
{
	return smb_register_vfs(SMB_VFS_INTERFACE_VERSION, "aixacl",
				aixacl_op_tuples);
}
