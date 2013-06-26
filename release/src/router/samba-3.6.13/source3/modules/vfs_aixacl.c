/*
   Unix SMB/Netbios implementation.
   VFS module to get and set posix acls
   Copyright (C) Jim McDonough <jmcd@us.ibm.com> 2006

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
				files_struct *fsp)
{

	struct acl *file_acl = (struct acl *)NULL;
	struct smb_acl_t *result = (struct smb_acl_t *)NULL;
	
	int rc = 0;
	uid_t user_id;

	/* Get the acl using fstatacl */
   
	DEBUG(10,("Entering AIX sys_acl_get_fd\n"));
	DEBUG(10,("fd is %d\n",fsp->fh->fd));
	file_acl = (struct acl *)SMB_MALLOC(BUFSIZ);

	if(file_acl == NULL) {
		errno=ENOMEM;
		DEBUG(0,("Error in AIX sys_acl_get_fd is %d\n",errno));
		return(NULL);
	}

	memset(file_acl,0,BUFSIZ);

	rc = fstatacl(fsp->fh->fd,0,file_acl,BUFSIZ);
	if( (rc == -1) && (errno == ENOSPC)) {
		struct acl *new_acl = SMB_MALLOC(file_acl->acl_len + sizeof(struct acl));
		if( new_acl == NULL) {
			SAFE_FREE(file_acl);
			errno = ENOMEM;
			return NULL;
		}
		file_acl = new_acl;
		rc = fstatacl(fsp->fh->fd,0,file_acl,file_acl->acl_len + sizeof(struct acl));
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
	unsigned int rc;
	
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
			    SMB_ACL_T theacl)
{
	struct acl *file_acl = NULL;
	unsigned int rc;

	file_acl = aixacl_smb_to_aixacl(SMB_ACL_TYPE_ACCESS, theacl);
	if (!file_acl)
		return -1;

	rc = fchacl(fsp->fh->fd,file_acl,file_acl->acl_len);
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

static struct vfs_fn_pointers vfs_aixacl_fns = {
	.sys_acl_get_file = aixacl_sys_acl_get_file,
	.sys_acl_get_fd = aixacl_sys_acl_get_fd,
	.sys_acl_set_file = aixacl_sys_acl_set_file,
	.sys_acl_set_fd = aixacl_sys_acl_set_fd,
	.sys_acl_delete_def_file = aixacl_sys_acl_delete_def_file,
};

NTSTATUS vfs_aixacl_init(void);
NTSTATUS vfs_aixacl_init(void)
{
	return smb_register_vfs(SMB_VFS_INTERFACE_VERSION, "aixacl",
				&vfs_aixacl_fns);
}
