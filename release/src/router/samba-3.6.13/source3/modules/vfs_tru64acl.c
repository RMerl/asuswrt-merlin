/*
   Unix SMB/Netbios implementation.
   VFS module to get and set Tru64 acls
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
#include "modules/vfs_tru64acl.h"

/* prototypes for private functions first - for clarity */

static struct smb_acl_t *tru64_acl_to_smb_acl(const struct acl *tru64_acl);
static bool tru64_ace_to_smb_ace(acl_entry_t tru64_ace, 
				struct smb_acl_entry *smb_ace);
static acl_t smb_acl_to_tru64_acl(const SMB_ACL_T smb_acl);
static acl_tag_t smb_tag_to_tru64(SMB_ACL_TAG_T smb_tag);
static SMB_ACL_TAG_T tru64_tag_to_smb(acl_tag_t tru64_tag);
static acl_perm_t smb_permset_to_tru64(SMB_ACL_PERM_T smb_permset);
static SMB_ACL_PERM_T tru64_permset_to_smb(const acl_perm_t tru64_permset);


/* public functions - the api */

SMB_ACL_T tru64acl_sys_acl_get_file(vfs_handle_struct *handle,
				    const char *path_p,
				    SMB_ACL_TYPE_T type)
{
        struct smb_acl_t *result;
        acl_type_t the_acl_type;
        acl_t tru64_acl;

	DEBUG(10, ("Hi! This is tru64acl_sys_acl_get_file.\n"));

        switch(type) {
        case SMB_ACL_TYPE_ACCESS:
                the_acl_type = ACL_TYPE_ACCESS;
                break;
        case SMB_ACL_TYPE_DEFAULT:
                the_acl_type = ACL_TYPE_DEFAULT;
                break;
        default:
                errno = EINVAL;
                return NULL;
        }

        tru64_acl = acl_get_file((char *)path_p, the_acl_type);

        if (tru64_acl == NULL) {
                return NULL;
        }

        result = tru64_acl_to_smb_acl(tru64_acl);
        acl_free(tru64_acl);
        return result;
}

SMB_ACL_T tru64acl_sys_acl_get_fd(vfs_handle_struct *handle,
				  files_struct *fsp)
{
	struct smb_acl_t *result;
	acl_t tru64_acl = acl_get_fd(fsp->fh->fd, ACL_TYPE_ACCESS);

	if (tru64_acl == NULL) {
		return NULL;
	}
	
	result = tru64_acl_to_smb_acl(tru64_acl);
	acl_free(tru64_acl);
	return result;
}

int tru64acl_sys_acl_set_file(vfs_handle_struct *handle,
			      const char *name,
			      SMB_ACL_TYPE_T type,
			      SMB_ACL_T theacl)
{
        int res;
        acl_type_t the_acl_type;
        acl_t tru64_acl;

        DEBUG(10, ("tru64acl_sys_acl_set_file called with name %s, type %d\n", 
			name, type));

        switch(type) {
        case SMB_ACL_TYPE_ACCESS:
		DEBUGADD(10, ("got acl type ACL_TYPE_ACCESS\n"));
                the_acl_type = ACL_TYPE_ACCESS;
                break;
        case SMB_ACL_TYPE_DEFAULT:
		DEBUGADD(10, ("got acl type ACL_TYPE_DEFAULT\n"));
                the_acl_type = ACL_TYPE_DEFAULT;
                break;
        default:
		DEBUGADD(10, ("invalid acl type\n"));
                errno = EINVAL;
                goto fail;
        }

	tru64_acl = smb_acl_to_tru64_acl(theacl);
        if (tru64_acl == NULL) {
		DEBUG(10, ("smb_acl_to_tru64_acl failed!\n"));
                goto fail;
        }
	DEBUG(10, ("got tru64 acl...\n"));
        res = acl_set_file((char *)name, the_acl_type, tru64_acl);
        acl_free(tru64_acl);
        if (res != 0) {
                DEBUG(10, ("acl_set_file failed: %s\n", strerror(errno)));
		goto fail;
        }
        return res;
fail:
	DEBUG(1, ("tru64acl_sys_acl_set_file failed!\n"));
	return -1;
}

int tru64acl_sys_acl_set_fd(vfs_handle_struct *handle,
			    files_struct *fsp,
			    SMB_ACL_T theacl)
{
        int res;
        acl_t tru64_acl = smb_acl_to_tru64_acl(theacl);
        if (tru64_acl == NULL) {
                return -1;
        }
        res =  acl_set_fd(fsp->fh->fd, ACL_TYPE_ACCESS, tru64_acl);
        acl_free(tru64_acl);
        return res;

}

int tru64acl_sys_acl_delete_def_file(vfs_handle_struct *handle,
				     const char *path)
{
	return acl_delete_def_file((char *)path);
}


/* private functions */

static struct smb_acl_t *tru64_acl_to_smb_acl(const struct acl *tru64_acl) 
{
	struct smb_acl_t *result;
	acl_entry_t entry;

	DEBUG(10, ("Hi! This is tru64_acl_to_smb_acl.\n"));
	
	if ((result = SMB_MALLOC_P(struct smb_acl_t)) == NULL) {
		DEBUG(0, ("SMB_MALLOC_P failed in tru64_acl_to_smb_acl\n"));
		errno = ENOMEM;
		goto fail;
	}
	ZERO_STRUCTP(result);
	if (acl_first_entry((struct acl *)tru64_acl) != 0) {
		DEBUG(10, ("acl_first_entry failed: %s\n", strerror(errno)));
		goto fail;
	}
	while ((entry = acl_get_entry((struct acl *)tru64_acl)) != NULL) {
		result = SMB_REALLOC(result, sizeof(struct smb_acl_t) +
					(sizeof(struct smb_acl_entry) * 
					 (result->count + 1)));
		if (result == NULL) {
			DEBUG(0, ("SMB_REALLOC failed in tru64_acl_to_smb_acl\n"));
			errno = ENOMEM;
			goto fail;
		}
		/* XYZ */
		if (!tru64_ace_to_smb_ace(entry, &result->acl[result->count])) {
			SAFE_FREE(result);
			goto fail;
		}
		result->count += 1;
	}
	return result;

fail:
	if (result != NULL) {
		SAFE_FREE(result);
	}
	DEBUG(1, ("tru64_acl_to_smb_acl failed!\n"));
	return NULL;
}

static bool tru64_ace_to_smb_ace(acl_entry_t tru64_ace, 
				struct smb_acl_entry *smb_ace) 
{
	acl_tag_t tru64_tag;
	acl_permset_t permset;
	SMB_ACL_TAG_T smb_tag_type;
	SMB_ACL_PERM_T smb_permset;
	void *qualifier;

	if (acl_get_tag_type(tru64_ace, &tru64_tag) != 0) {
		DEBUG(0, ("acl_get_tag_type failed: %s\n", strerror(errno)));
		return False;
	}
	
	/* On could set the tag type directly to save a function call, 
	 * but I like this better... */
	smb_tag_type = tru64_tag_to_smb(tru64_tag);
	if (smb_tag_type == 0) {
		DEBUG(3, ("invalid tag type given: %d\n", tru64_tag));
		return False;
	}
	if (sys_acl_set_tag_type(smb_ace, smb_tag_type) != 0) {
		DEBUG(3, ("sys_acl_set_tag_type failed: %s\n", 
				strerror(errno)));
		return False;
	}
	qualifier = acl_get_qualifier(tru64_ace);
	if (qualifier != NULL) {
		if (sys_acl_set_qualifier(smb_ace, qualifier) != 0) {
			DEBUG(3, ("sys_acl_set_qualifier failed\n"));
			return False;
		}
	}
	if (acl_get_permset(tru64_ace, &permset) != 0) {
		DEBUG(3, ("acl_get_permset failed: %s\n", strerror(errno)));
		return False;
	}
	smb_permset = tru64_permset_to_smb(*permset);
	if (sys_acl_set_permset(smb_ace, &smb_permset) != 0) {
		DEBUG(3, ("sys_acl_set_permset failed: %s\n", strerror(errno)));
		return False;
	}
	return True;
}

static acl_t smb_acl_to_tru64_acl(const SMB_ACL_T smb_acl) 
{
	acl_t result;
	acl_entry_t tru64_entry;
	int i;
	char *acl_text;
	ssize_t acl_text_len;
	
	/* The tru64 acl_init function takes a size_t value
	 * instead of a count of entries (as with posix). 
	 * the size parameter "Specifies the size of the working
	 * storage in bytes" (according to the man page). 
	 * But it is unclear to me, how this size is to be 
	 * calculated. 
	 *
	 * It should not matter, since acl_create_entry enlarges 
	 * the working storage at need. ... */

	DEBUG(10, ("Hi! This is smb_acl_to_tru64_acl.\n"));

	result = acl_init(1);

	if (result == NULL) {
		DEBUG(3, ("acl_init failed!\n"));
		goto fail;
	}
	
	DEBUGADD(10, ("parsing acl entries...\n"));
	for (i = 0; i < smb_acl->count; i++) {
		/* XYZ - maybe eliminate this direct access? */
		const struct smb_acl_entry *smb_entry = &smb_acl->acl[i];
		acl_tag_t tru64_tag;
		acl_perm_t tru64_permset;

		tru64_tag = smb_tag_to_tru64(smb_entry->a_type);
		if (tru64_tag == -1) {
			DEBUG(3, ("smb_tag_to_tru64 failed!\n"));
			goto fail;
		}

		if (tru64_tag == ACL_MASK) {
			DEBUGADD(10, (" - acl type ACL_MASK: not implemented on Tru64 ==> skipping\n"));
			continue;
		}
		
		tru64_entry = acl_create_entry(&result);
		if (tru64_entry == NULL) {
			DEBUG(3, ("acl_create_entry failed: %s\n", 
					strerror(errno)));
			goto fail;
		}

		if (acl_set_tag_type(tru64_entry, tru64_tag) != 0) {
			DEBUG(3, ("acl_set_tag_type(%d) failed: %s\n",
					strerror(errno)));
			goto fail;
		}
		
		switch (smb_entry->a_type) {
		case SMB_ACL_USER:
			if (acl_set_qualifier(tru64_entry, 
						(int *)&smb_entry->uid) != 0) 
			{
				DEBUG(3, ("acl_set_qualifier failed: %s\n",
					strerror(errno)));
				goto fail;
			}
			DEBUGADD(10, (" - setting uid to %d\n", smb_entry->uid));
			break;
		case SMB_ACL_GROUP:
			if (acl_set_qualifier(tru64_entry, 
						(int *)&smb_entry->gid) != 0)
			{
				DEBUG(3, ("acl_set_qualifier failed: %s\n",
					strerror(errno)));
				goto fail;
			}
			DEBUGADD(10, (" - setting gid to %d\n", smb_entry->gid));
			break;
		default:
			break;
		}

		tru64_permset = smb_permset_to_tru64(smb_entry->a_perm);
		if (tru64_permset == -1) {
			DEBUG(3, ("smb_permset_to_tru64 failed!\n"));
			goto fail;
		}
		DEBUGADD(10, (" - setting perms to %0d\n", tru64_permset));
		if (acl_set_permset(tru64_entry, &tru64_permset) != 0)
		{
			DEBUG(3, ("acl_set_permset failed: %s\n", strerror(errno)));
			goto fail;
		}
	} /* for */
	DEBUGADD(10, ("done parsing acl entries\n"));

	tru64_entry = NULL;
	if (acl_valid(result, &tru64_entry) != 0) {
		DEBUG(1, ("smb_acl_to_tru64_acl: ACL is invalid (%s)\n",
				strerror(errno)));
		if (tru64_entry != NULL) {
			DEBUGADD(1, ("the acl contains duplicate entries\n"));
		}
		goto fail;
	}
	DEBUGADD(10, ("acl is valid\n"));

	acl_text = acl_to_text(result, &acl_text_len);
	if (acl_text == NULL) {
		DEBUG(3, ("acl_to_text failed: %s\n", strerror(errno)));
		goto fail;
	}
	DEBUG(1, ("acl_text: %s\n", acl_text));
	free(acl_text);

	return result;

fail:
	if (result != NULL) {
		acl_free(result);
	}
	DEBUG(1, ("smb_acl_to_tru64_acl failed!\n"));
	return NULL;
}

static acl_tag_t smb_tag_to_tru64(SMB_ACL_TAG_T smb_tag)
{
	acl_tag_t result;
	switch (smb_tag) {
	case SMB_ACL_USER:
		result = ACL_USER;
		DEBUGADD(10, ("got acl type ACL_USER\n"));
		break;
	case SMB_ACL_USER_OBJ:
		result = ACL_USER_OBJ;
		DEBUGADD(10, ("got acl type ACL_USER_OBJ\n"));
		break;
	case SMB_ACL_GROUP:
		result = ACL_GROUP;
		DEBUGADD(10, ("got acl type ACL_GROUP\n"));
		break;
	case SMB_ACL_GROUP_OBJ:
		result = ACL_GROUP_OBJ;
		DEBUGADD(10, ("got acl type ACL_GROUP_OBJ\n"));
		break;
	case SMB_ACL_OTHER:
		result = ACL_OTHER;
		DEBUGADD(10, ("got acl type ACL_OTHER\n"));
		break;
	case SMB_ACL_MASK:
		result = ACL_MASK;
		DEBUGADD(10, ("got acl type ACL_MASK\n"));
		break;
	default:
		DEBUG(1, ("Unknown tag type %d\n", smb_tag));
		result = -1;
	}
	return result;
}


static SMB_ACL_TAG_T tru64_tag_to_smb(acl_tag_t tru64_tag)
{
	SMB_ACL_TAG_T smb_tag_type;
	switch(tru64_tag) {
	case ACL_USER:
		smb_tag_type = SMB_ACL_USER;
		DEBUGADD(10, ("got smb acl tag type SMB_ACL_USER\n"));
		break;
	case ACL_USER_OBJ:
		smb_tag_type = SMB_ACL_USER_OBJ;
		DEBUGADD(10, ("got smb acl tag type SMB_ACL_USER_OBJ\n"));
		break;
	case ACL_GROUP:
		smb_tag_type = SMB_ACL_GROUP;
		DEBUGADD(10, ("got smb acl tag type SMB_ACL_GROUP\n"));
		break;
	case ACL_GROUP_OBJ:
		smb_tag_type = SMB_ACL_GROUP_OBJ;
		DEBUGADD(10, ("got smb acl tag type SMB_ACL_GROUP_OBJ\n"));
		break;
	case ACL_OTHER:
		smb_tag_type = SMB_ACL_OTHER;
		DEBUGADD(10, ("got smb acl tag type SMB_ACL_OTHER\n"));
		break;
	case ACL_MASK:
		smb_tag_type = SMB_ACL_MASK;
		DEBUGADD(10, ("got smb acl tag type SMB_ACL_MASK\n"));
		break;
	default:
		DEBUG(0, ("Unknown tag type %d\n", (unsigned int)tru64_tag));
		smb_tag_type = 0;
	}
	return smb_tag_type;
}

static acl_perm_t smb_permset_to_tru64(SMB_ACL_PERM_T smb_permset)
{
	/* originally, I thought that acl_clear_perm was the
	 * proper way to reset the permset to 0. but without 
	 * initializing it to 0, acl_clear_perm fails.
	 * so probably, acl_clear_perm is not necessary here... ?! */
	acl_perm_t tru64_permset = 0;
	if (acl_clear_perm(&tru64_permset) != 0) {
		DEBUG(5, ("acl_clear_perm failed: %s\n", strerror(errno)));
		return -1;
	}
	/* according to original lib/sysacls.c, acl_add_perm is
	 * broken on tru64 ... */
	tru64_permset |= ((smb_permset & SMB_ACL_READ) ? ACL_READ : 0);
	tru64_permset |= ((smb_permset & SMB_ACL_WRITE) ? ACL_WRITE : 0);
	tru64_permset |= ((smb_permset & SMB_ACL_EXECUTE) ? ACL_EXECUTE : 0);
	return tru64_permset;
}

static SMB_ACL_PERM_T tru64_permset_to_smb(const acl_perm_t tru64_permset)
{
	SMB_ACL_PERM_T smb_permset  = 0;
	smb_permset |= ((tru64_permset & ACL_READ) ? SMB_ACL_READ : 0);
	smb_permset |= ((tru64_permset & ACL_WRITE) ? SMB_ACL_WRITE : 0);
	smb_permset |= ((tru64_permset & ACL_EXECUTE) ? SMB_ACL_EXECUTE : 0);
	return smb_permset;
}


/* VFS operations structure */

static struct vfs_fn_pointers tru64acl_fns = {
	.sys_acl_get_file = tru64acl_sys_acl_get_file,
	.sys_acl_get_fd = tru64acl_sys_acl_get_fd,
	.sys_acl_set_file = tru64acl_sys_acl_set_file,
	.sys_acl_set_fd = tru64acl_sys_acl_set_fd,
	.sys_acl_delete_def_file = tru64acl_sys_acl_delete_def_file,
};

NTSTATUS vfs_tru64acl_init(void);
NTSTATUS vfs_tru64acl_init(void)
{
	return smb_register_vfs(SMB_VFS_INTERFACE_VERSION, "tru64acl",
				&tru64acl_fns);
}

/* ENTE */
