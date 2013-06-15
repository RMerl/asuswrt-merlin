/*
   Unix SMB/Netbios implementation.
   VFS module to get and set posix acls
   Copyright (C) Volker Lendecke 2006

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


/* prototypes for static functions first - for clarity */

static BOOL smb_ace_to_internal(acl_entry_t posix_ace,
				struct smb_acl_entry *ace);
static struct smb_acl_t *smb_acl_to_internal(acl_t acl);
static int smb_acl_set_mode(acl_entry_t entry, SMB_ACL_PERM_T perm);
static acl_t smb_acl_to_posix(const struct smb_acl_t *acl);


/* public functions - the api */

SMB_ACL_T posixacl_sys_acl_get_file(vfs_handle_struct *handle,
				    const char *path_p,
				    SMB_ACL_TYPE_T type)
{
	struct smb_acl_t *result;
	acl_type_t acl_type;
	acl_t acl;

	switch(type) {
	case SMB_ACL_TYPE_ACCESS:
		acl_type = ACL_TYPE_ACCESS;
		break;
	case SMB_ACL_TYPE_DEFAULT:
		acl_type = ACL_TYPE_DEFAULT;
		break;
	default:
		errno = EINVAL;
		return NULL;
	}

	acl = acl_get_file(path_p, acl_type);

	if (acl == NULL) {
		return NULL;
	}

	result = smb_acl_to_internal(acl);
	acl_free(acl);
	return result;
}

SMB_ACL_T posixacl_sys_acl_get_fd(vfs_handle_struct *handle,
				  files_struct *fsp,
				  int fd)
{
	struct smb_acl_t *result;
	acl_t acl = acl_get_fd(fd);

	if (acl == NULL) {
		return NULL;
	}

	result = smb_acl_to_internal(acl);
	acl_free(acl);
	return result;
}

int posixacl_sys_acl_set_file(vfs_handle_struct *handle,
			      const char *name,
			      SMB_ACL_TYPE_T type,
			      SMB_ACL_T theacl)
{
	int res;
	acl_type_t acl_type;
	acl_t acl;

	DEBUG(10, ("Calling acl_set_file: %s, %d\n", name, type));

	switch(type) {
	case SMB_ACL_TYPE_ACCESS:
		acl_type = ACL_TYPE_ACCESS;
		break;
	case SMB_ACL_TYPE_DEFAULT:
		acl_type = ACL_TYPE_DEFAULT;
		break;
	default:
		errno = EINVAL;
		return -1;
	}

	if ((acl = smb_acl_to_posix(theacl)) == NULL) {
		return -1;
	}
	res = acl_set_file(name, acl_type, acl);
	if (res != 0) {
		DEBUG(10, ("acl_set_file failed: %s\n", strerror(errno)));
	}
	acl_free(acl);
	return res;
}

int posixacl_sys_acl_set_fd(vfs_handle_struct *handle,
			    files_struct *fsp,
			    int fd, SMB_ACL_T theacl)
{
	int res;
	acl_t acl = smb_acl_to_posix(theacl);
	if (acl == NULL) {
		return -1;
	}
	res =  acl_set_fd(fd, acl);
	acl_free(acl);
	return res;
}

int posixacl_sys_acl_delete_def_file(vfs_handle_struct *handle,
				     const char *path)
{
	return acl_delete_def_file(path);
}


/* private functions */

static BOOL smb_ace_to_internal(acl_entry_t posix_ace,
				struct smb_acl_entry *ace)
{
	acl_tag_t tag;
	acl_permset_t permset;

	if (acl_get_tag_type(posix_ace, &tag) != 0) {
		DEBUG(0, ("smb_acl_get_tag_type failed\n"));
		return False;
	}

	switch(tag) {
	case ACL_USER:
		ace->a_type = SMB_ACL_USER;
		break;
	case ACL_USER_OBJ:
		ace->a_type = SMB_ACL_USER_OBJ;
		break;
	case ACL_GROUP:
		ace->a_type = SMB_ACL_GROUP;
		break;
	case ACL_GROUP_OBJ:
		ace->a_type = SMB_ACL_GROUP_OBJ;
		break;
	case ACL_OTHER:
		ace->a_type = SMB_ACL_OTHER;
		break;
	case ACL_MASK:
		ace->a_type = SMB_ACL_MASK;
		break;
	default:
		DEBUG(0, ("unknown tag type %d\n", (unsigned int)tag));
		return False;
	}
	switch(ace->a_type) {
	case SMB_ACL_USER: {
		uid_t *puid = (uid_t *)acl_get_qualifier(posix_ace);
		if (puid == NULL) {
			DEBUG(0, ("smb_acl_get_qualifier failed\n"));
			return False;
		}
		ace->uid = *puid;
		acl_free(puid);
		break;
	}
		
	case SMB_ACL_GROUP: {
		gid_t *pgid = (uid_t *)acl_get_qualifier(posix_ace);
		if (pgid == NULL) {
			DEBUG(0, ("smb_acl_get_qualifier failed\n"));
			return False;
		}
		ace->gid = *pgid;
		acl_free(pgid);
		break;
	}
	default:
		break;
	}
	if (acl_get_permset(posix_ace, &permset) != 0) {
		DEBUG(0, ("smb_acl_get_mode failed\n"));
		return False;
	}
	ace->a_perm = 0;
#ifdef HAVE_ACL_GET_PERM_NP
	ace->a_perm |= (acl_get_perm_np(permset, ACL_READ) ? SMB_ACL_READ : 0);
	ace->a_perm |= (acl_get_perm_np(permset, ACL_WRITE) ? SMB_ACL_WRITE : 0);
	ace->a_perm |= (acl_get_perm_np(permset, ACL_EXECUTE) ? SMB_ACL_EXECUTE : 0);
#else
	ace->a_perm |= (acl_get_perm(permset, ACL_READ) ? SMB_ACL_READ : 0);
	ace->a_perm |= (acl_get_perm(permset, ACL_WRITE) ? SMB_ACL_WRITE : 0);
	ace->a_perm |= (acl_get_perm(permset, ACL_EXECUTE) ? SMB_ACL_EXECUTE : 0);
#endif
	return True;
}

static struct smb_acl_t *smb_acl_to_internal(acl_t acl)
{
	struct smb_acl_t *result = SMB_MALLOC_P(struct smb_acl_t);
	int entry_id = ACL_FIRST_ENTRY;
	acl_entry_t e;
	if (result == NULL) {
		return NULL;
	}
	ZERO_STRUCTP(result);
	while (acl_get_entry(acl, entry_id, &e) == 1) {

		entry_id = ACL_NEXT_ENTRY;

		result = (struct smb_acl_t *)SMB_REALLOC(
			result, sizeof(struct smb_acl_t) +
			(sizeof(struct smb_acl_entry) * (result->count+1)));
		if (result == NULL) {
			DEBUG(0, ("SMB_REALLOC failed\n"));
			errno = ENOMEM;
			return NULL;
		}

		if (!smb_ace_to_internal(e, &result->acl[result->count])) {
			SAFE_FREE(result);
			return NULL;
		}

		result->count += 1;
	}
	return result;
}

static int smb_acl_set_mode(acl_entry_t entry, SMB_ACL_PERM_T perm)
{
        int ret;
        acl_permset_t permset;

	if ((ret = acl_get_permset(entry, &permset)) != 0) {
		return ret;
	}
        if ((ret = acl_clear_perms(permset)) != 0) {
                return ret;
	}
        if ((perm & SMB_ACL_READ) &&
	    ((ret = acl_add_perm(permset, ACL_READ)) != 0)) {
		return ret;
	}
        if ((perm & SMB_ACL_WRITE) &&
	    ((ret = acl_add_perm(permset, ACL_WRITE)) != 0)) {
		return ret;
	}
        if ((perm & SMB_ACL_EXECUTE) &&
	    ((ret = acl_add_perm(permset, ACL_EXECUTE)) != 0)) {
		return ret;
	}
        return acl_set_permset(entry, permset);
}

static acl_t smb_acl_to_posix(const struct smb_acl_t *acl)
{
	acl_t result;
	int i;

	result = acl_init(acl->count);
	if (result == NULL) {
		DEBUG(10, ("acl_init failed\n"));
		return NULL;
	}

	for (i=0; i<acl->count; i++) {
		const struct smb_acl_entry *entry = &acl->acl[i];
		acl_entry_t e;
		acl_tag_t tag;

		if (acl_create_entry(&result, &e) != 0) {
			DEBUG(1, ("acl_create_entry failed: %s\n",
				  strerror(errno)));
			goto fail;
		}

		switch (entry->a_type) {
		case SMB_ACL_USER:
			tag = ACL_USER;
			break;
		case SMB_ACL_USER_OBJ:
			tag = ACL_USER_OBJ;
			break;
		case SMB_ACL_GROUP:
			tag = ACL_GROUP;
			break;
		case SMB_ACL_GROUP_OBJ:
			tag = ACL_GROUP_OBJ;
			break;
		case SMB_ACL_OTHER:
			tag = ACL_OTHER;
			break;
		case SMB_ACL_MASK:
			tag = ACL_MASK;
			break;
		default:
			DEBUG(1, ("Unknown tag value %d\n", entry->a_type));
			goto fail;
		}

		if (acl_set_tag_type(e, tag) != 0) {
			DEBUG(10, ("acl_set_tag_type(%d) failed: %s\n",
				   tag, strerror(errno)));
			goto fail;
		}

		switch (entry->a_type) {
		case SMB_ACL_USER:
			if (acl_set_qualifier(e, &entry->uid) != 0) {
				DEBUG(1, ("acl_set_qualifiier failed: %s\n",
					  strerror(errno)));
				goto fail;
			}
			break;
		case SMB_ACL_GROUP:
			if (acl_set_qualifier(e, &entry->gid) != 0) {
				DEBUG(1, ("acl_set_qualifiier failed: %s\n",
					  strerror(errno)));
				goto fail;
			}
			break;
		default: 	/* Shut up, compiler! :-) */
			break;
		}

		if (smb_acl_set_mode(e, entry->a_perm) != 0) {
			goto fail;
		}
	}

	if (acl_valid(result) != 0) {
		DEBUG(0, ("smb_acl_to_posix: ACL is invalid for set (%s)\n",
			  strerror(errno)));
		goto fail;
	}

	return result;

 fail:
	if (result != NULL) {
		acl_free(result);
	}
	return NULL;
}

/* VFS operations structure */

static vfs_op_tuple posixacl_op_tuples[] = {
	/* Disk operations */
  {SMB_VFS_OP(posixacl_sys_acl_get_file),
   SMB_VFS_OP_SYS_ACL_GET_FILE,
   SMB_VFS_LAYER_TRANSPARENT},

  {SMB_VFS_OP(posixacl_sys_acl_get_fd),
   SMB_VFS_OP_SYS_ACL_GET_FD,
   SMB_VFS_LAYER_TRANSPARENT},

  {SMB_VFS_OP(posixacl_sys_acl_set_file),
   SMB_VFS_OP_SYS_ACL_SET_FILE,
   SMB_VFS_LAYER_TRANSPARENT},

  {SMB_VFS_OP(posixacl_sys_acl_set_fd),
   SMB_VFS_OP_SYS_ACL_SET_FD,
   SMB_VFS_LAYER_TRANSPARENT},

  {SMB_VFS_OP(posixacl_sys_acl_delete_def_file),
   SMB_VFS_OP_SYS_ACL_DELETE_DEF_FILE,
   SMB_VFS_LAYER_TRANSPARENT},

  {SMB_VFS_OP(NULL),
   SMB_VFS_OP_NOOP,
   SMB_VFS_LAYER_NOOP}
};

NTSTATUS vfs_posixacl_init(void);
NTSTATUS vfs_posixacl_init(void)
{
	return smb_register_vfs(SMB_VFS_INTERFACE_VERSION, "posixacl",
				posixacl_op_tuples);
}
