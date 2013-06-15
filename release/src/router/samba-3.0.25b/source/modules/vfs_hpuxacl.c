/*
 * Unix SMB/Netbios implementation.
 * VFS module to get and set HP-UX ACLs
 * Copyright (C) Michael Adam 2006
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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

/* =================================================================
 * NOTE:
 *
 * The original hpux-acl code in lib/sysacls.c was based upon the
 * solaris acl code in the same file. Now for the new modularized
 * acl implementation, I have taken the code from vfs_solarisacls.c
 * and did similar adaptations as were done before, essentially
 * reusing the original internal aclsort functions.
 * The check for the presence of the acl() call has been adopted, and
 * a check for the presence of the aclsort() call has been added. 
 * 
 * Michael Adam <obnox@samba.org>
 *
 * ================================================================= */


#include "includes.h"

/* 
 * including standard header <sys/aclv.h> 
 *
 * included here as a quick hack for the special HP-UX-situation:
 *
 * The problem is that, on HP-UX, jfs/posix acls are
 * defined in <sys/aclv.h>, while the deprecated hfs acls 
 * are defined inside <sys/acl.h>.
 *
 */
/* GROUP is defined somewhere else so undef it here... */
#undef GROUP
#include <sys/aclv.h>
/* dl.h: needed to check for acl call via shl_findsym */
#include <dl.h>

typedef struct acl HPUX_ACE_T;
typedef struct acl *HPUX_ACL_T;
typedef int HPUX_ACL_TAG_T;   /* the type of an ACL entry */
typedef ushort HPUX_PERM_T;

/* Structure to capture the count for each type of ACE. 
 * (for hpux_internal_aclsort */
struct hpux_acl_types {
	int n_user;
	int n_def_user;
	int n_user_obj;
	int n_def_user_obj;

	int n_group;
	int n_def_group;
	int n_group_obj;
	int n_def_group_obj;

	int n_other;
	int n_other_obj;
	int n_def_other_obj;

	int n_class_obj;
	int n_def_class_obj;

	int n_illegal_obj;
};

/* for convenience: check if hpux acl entry is a default entry?  */
#define _IS_DEFAULT(ace) ((ace).a_type & ACL_DEFAULT)
#define _IS_OF_TYPE(ace, type) ( \
	(((type) == SMB_ACL_TYPE_ACCESS) && !_IS_DEFAULT(ace)) \
	|| \
	(((type) == SMB_ACL_TYPE_DEFAULT) && _IS_DEFAULT(ace)) \
)


/* prototypes for private functions */

static HPUX_ACL_T hpux_acl_init(int count);
static BOOL smb_acl_to_hpux_acl(SMB_ACL_T smb_acl, 
		HPUX_ACL_T *solariacl, int *count, 
		SMB_ACL_TYPE_T type);
static SMB_ACL_T hpux_acl_to_smb_acl(HPUX_ACL_T hpuxacl, int count,
		SMB_ACL_TYPE_T type);
static HPUX_ACL_TAG_T smb_tag_to_hpux_tag(SMB_ACL_TAG_T smb_tag);
static SMB_ACL_TAG_T hpux_tag_to_smb_tag(HPUX_ACL_TAG_T hpux_tag);
static BOOL hpux_add_to_acl(HPUX_ACL_T *hpux_acl, int *count,
		HPUX_ACL_T add_acl, int add_count, SMB_ACL_TYPE_T type);
static BOOL hpux_acl_get_file(const char *name, HPUX_ACL_T *hpuxacl, 
		int *count);
static SMB_ACL_PERM_T hpux_perm_to_smb_perm(const HPUX_PERM_T perm);
static HPUX_PERM_T smb_perm_to_hpux_perm(const SMB_ACL_PERM_T perm);
#if 0
static BOOL hpux_acl_check(HPUX_ACL_T hpux_acl, int count);
#endif
/* aclsort (internal) and helpers: */
static BOOL hpux_acl_sort(HPUX_ACL_T acl, int count);
static int hpux_internal_aclsort(int acl_count, int calclass, HPUX_ACL_T aclp);
static void hpux_count_obj(int acl_count, HPUX_ACL_T aclp, 
		struct hpux_acl_types *acl_type_count);
static void hpux_swap_acl_entries(HPUX_ACE_T *aclp0, HPUX_ACE_T *aclp1);
static BOOL hpux_prohibited_duplicate_type(int acl_type);

static BOOL hpux_acl_call_present(void);
static BOOL hpux_aclsort_call_present(void);


/* public functions - the api */

SMB_ACL_T hpuxacl_sys_acl_get_file(vfs_handle_struct *handle,
				      const char *path_p,
				      SMB_ACL_TYPE_T type)
{
	SMB_ACL_T result = NULL;
	int count;
	HPUX_ACL_T hpux_acl;
	
	DEBUG(10, ("hpuxacl_sys_acl_get_file called for file '%s'.\n", 
		   path_p));

	if(hpux_acl_call_present() == False) {
		/* Looks like we don't have the acl() system call on HPUX. 
		 * May be the system doesn't have the latest version of JFS.
		 */
		goto done;
	}

	if (type != SMB_ACL_TYPE_ACCESS && type != SMB_ACL_TYPE_DEFAULT) {
		DEBUG(10, ("invalid SMB_ACL_TYPE given (%d)\n", type));
		errno = EINVAL;
		goto done;
	}

	DEBUGADD(10, ("getting %s acl\n", 
		      ((type == SMB_ACL_TYPE_ACCESS) ? "access" : "default")));

	if (!hpux_acl_get_file(path_p, &hpux_acl, &count)) {
		goto done;
	}
	result = hpux_acl_to_smb_acl(hpux_acl, count, type);
	if (result == NULL) {
		DEBUG(10, ("conversion hpux_acl -> smb_acl failed (%s).\n",
			   strerror(errno)));
	}
	
 done:
	DEBUG(10, ("hpuxacl_sys_acl_get_file %s.\n",
		   ((result == NULL) ? "failed" : "succeeded" )));
	SAFE_FREE(hpux_acl);
	return result;
}


/*
 * get the access ACL of a file referred to by a fd
 */
SMB_ACL_T hpuxacl_sys_acl_get_fd(vfs_handle_struct *handle,
				 files_struct *fsp,
				 int fd)
{
        /* 
	 * HPUX doesn't have the facl call. Fake it using the path.... JRA. 
	 */
	/* For all I see, the info should already be in the fsp
	 * parameter, but get it again to be safe --- necessary? */
        files_struct *file_struct_p = file_find_fd(fd);
        if (file_struct_p == NULL) {
                errno = EBADF;
                return NULL;
        }
        /*
         * We know we're in the same conn context. So we
         * can use the relative path.
         */
	DEBUG(10, ("redirecting call of hpuxacl_sys_acl_get_fd to "
		"hpuxacl_sys_acl_get_file (no facl syscall on HPUX).\n"));

        return hpuxacl_sys_acl_get_file(handle, file_struct_p->fsp_name, 
			SMB_ACL_TYPE_ACCESS);
}


int hpuxacl_sys_acl_set_file(vfs_handle_struct *handle,
			     const char *name,
			     SMB_ACL_TYPE_T type,
			     SMB_ACL_T theacl)
{
	int ret = -1;
	SMB_STRUCT_STAT s;
	HPUX_ACL_T hpux_acl;
	int count;
	
	DEBUG(10, ("hpuxacl_sys_acl_set_file called for file '%s'\n",
		   name));


	if(hpux_acl_call_present() == False) {
		/* Looks like we don't have the acl() system call on HPUX. 
		 * May be the system doesn't have the latest version of JFS.
		 */
		goto done;
	}

	if ((type != SMB_ACL_TYPE_ACCESS) && (type != SMB_ACL_TYPE_DEFAULT)) {
		errno = EINVAL;
		DEBUG(10, ("invalid smb acl type given (%d).\n", type));
		goto done;
	}
	DEBUGADD(10, ("setting %s acl\n", 
		      ((type == SMB_ACL_TYPE_ACCESS) ? "access" : "default")));

	if(!smb_acl_to_hpux_acl(theacl, &hpux_acl, &count, type)) {
		DEBUG(10, ("conversion smb_acl -> hpux_acl failed (%s).\n",
			   strerror(errno)));
                goto done;
	}

	/*
	 * if the file is a directory, there is extra work to do:
	 * since the hpux acl call stores both the access acl and 
	 * the default acl as provided, we have to get the acl part 
	 * that has _not_ been specified in "type" from the file first 
	 * and concatenate it with the acl provided.
	 */
	if (SMB_VFS_STAT(handle->conn, name, &s) != 0) {
		DEBUG(10, ("Error in stat call: %s\n", strerror(errno)));
		goto done;
	}
	if (S_ISDIR(s.st_mode)) {
		HPUX_ACL_T other_acl; 
		int other_count;
		SMB_ACL_TYPE_T other_type;

		other_type = (type == SMB_ACL_TYPE_ACCESS) 
			? SMB_ACL_TYPE_DEFAULT
			: SMB_ACL_TYPE_ACCESS;
		DEBUGADD(10, ("getting acl from filesystem\n"));
		if (!hpux_acl_get_file(name, &other_acl, &other_count)) {
			DEBUG(10, ("error getting acl from directory\n"));
			goto done;
		}
		DEBUG(10, ("adding %s part of fs acl to given acl\n",
			   ((other_type == SMB_ACL_TYPE_ACCESS) 
			    ? "access"
			    : "default")));
		if (!hpux_add_to_acl(&hpux_acl, &count, other_acl,
					other_count, other_type)) 
		{
			DEBUG(10, ("error adding other acl.\n"));
			SAFE_FREE(other_acl);
			goto done;
		}
		SAFE_FREE(other_acl);
	}
	else if (type != SMB_ACL_TYPE_ACCESS) {
		errno = EINVAL;
		goto done;
	}

	if (!hpux_acl_sort(hpux_acl, count)) {
		DEBUG(10, ("resulting acl is not valid!\n"));
		goto done;
	}
	DEBUG(10, ("resulting acl is valid.\n"));

	ret = acl(CONST_DISCARD(char *, name), ACL_SET, count, hpux_acl);
	if (ret != 0) {
		DEBUG(0, ("ERROR calling acl: %s\n", strerror(errno)));
	}
	
 done:
	DEBUG(10, ("hpuxacl_sys_acl_set_file %s.\n",
		   ((ret != 0) ? "failed" : "succeeded")));
	SAFE_FREE(hpux_acl);
	return ret;
}

/*
 * set the access ACL on the file referred to by a fd 
 */
int hpuxacl_sys_acl_set_fd(vfs_handle_struct *handle,
			      files_struct *fsp,
			      int fd, SMB_ACL_T theacl)
{
        /*
         * HPUX doesn't have the facl call. Fake it using the path.... JRA.
         */
	/* For all I see, the info should already be in the fsp
	 * parameter, but get it again to be safe --- necessary? */
        files_struct *file_struct_p = file_find_fd(fd);
        if (file_struct_p == NULL) {
                errno = EBADF;
                return -1;
        }
        /*
         * We know we're in the same conn context. So we
         * can use the relative path.
         */
	DEBUG(10, ("redirecting call of hpuxacl_sys_acl_set_fd to "
		"hpuxacl_sys_acl_set_file (no facl syscall on HPUX)\n"));

        return hpuxacl_sys_acl_set_file(handle, file_struct_p->fsp_name,
			SMB_ACL_TYPE_ACCESS, theacl);
}


/*
 * delete the default ACL of a directory
 *
 * This is achieved by fetching the access ACL and rewriting it 
 * directly, via the hpux system call: the ACL_SET call on 
 * directories writes both the access and the default ACL as provided.
 *
 * XXX: posix acl_delete_def_file returns an error if
 * the file referred to by path is not a directory.
 * this function does not complain but the actions 
 * have no effect on a file other than a directory.
 * But sys_acl_delete_default_file is only called in
 * smbd/posixacls.c after having checked that the file
 * is a directory, anyways. So implementing the extra
 * check is considered unnecessary. --- Agreed? XXX
 */
int hpuxacl_sys_acl_delete_def_file(vfs_handle_struct *handle,
				    const char *path)
{
	SMB_ACL_T smb_acl;
	int ret = -1;
	HPUX_ACL_T hpux_acl;
	int count;

	DEBUG(10, ("entering hpuxacl_sys_acl_delete_def_file.\n"));
	
	smb_acl = hpuxacl_sys_acl_get_file(handle, path, 
					   SMB_ACL_TYPE_ACCESS);
	if (smb_acl == NULL) {
		DEBUG(10, ("getting file acl failed!\n"));
		goto done;
	}
	if (!smb_acl_to_hpux_acl(smb_acl, &hpux_acl, &count, 
				 SMB_ACL_TYPE_ACCESS))
	{
		DEBUG(10, ("conversion smb_acl -> hpux_acl failed.\n"));
		goto done;
	}
	if (!hpux_acl_sort(hpux_acl, count)) {
		DEBUG(10, ("resulting acl is not valid!\n"));
		goto done;
	}
	ret = acl(CONST_DISCARD(char *, path), ACL_SET, count, hpux_acl);
	if (ret != 0) {
		DEBUG(10, ("settinge file acl failed!\n"));
	}
	
 done:
	DEBUG(10, ("hpuxacl_sys_acl_delete_def_file %s.\n",
		   ((ret != 0) ? "failed" : "succeeded" )));
	SAFE_FREE(smb_acl);
	return ret;
}


/* 
 * private functions 
 */

static HPUX_ACL_T hpux_acl_init(int count)
{
	HPUX_ACL_T hpux_acl = 
		(HPUX_ACL_T)SMB_MALLOC(sizeof(HPUX_ACE_T) * count);
	if (hpux_acl == NULL) {
		errno = ENOMEM;
	}
	return hpux_acl;
}

/*
 * Convert the SMB acl to the ACCESS or DEFAULT part of a 
 * hpux ACL, as desired.
 */
static BOOL smb_acl_to_hpux_acl(SMB_ACL_T smb_acl, 
				   HPUX_ACL_T *hpux_acl, int *count, 
				   SMB_ACL_TYPE_T type)
{
	BOOL ret = False;
	int i;
	int check_which, check_rc;

	DEBUG(10, ("entering smb_acl_to_hpux_acl\n"));
	
	*hpux_acl = NULL;
	*count = 0;

	for (i = 0; i < smb_acl->count; i++) {
		const struct smb_acl_entry *smb_entry = &(smb_acl->acl[i]);
		HPUX_ACE_T hpux_entry;

		ZERO_STRUCT(hpux_entry);

		hpux_entry.a_type = smb_tag_to_hpux_tag(smb_entry->a_type);
		if (hpux_entry.a_type == 0) {
			DEBUG(10, ("smb_tag to hpux_tag failed\n"));
			goto fail;
		}
		switch(hpux_entry.a_type) {
		case USER:
			DEBUG(10, ("got tag type USER with uid %d\n", 
				   smb_entry->uid));
			hpux_entry.a_id = (uid_t)smb_entry->uid;
			break;
		case GROUP:
			DEBUG(10, ("got tag type GROUP with gid %d\n", 
				   smb_entry->gid));
			hpux_entry.a_id = (uid_t)smb_entry->gid;
			break;
		default:
			break;
		}
		if (type == SMB_ACL_TYPE_DEFAULT) {
			DEBUG(10, ("adding default bit to hpux ace\n"));
			hpux_entry.a_type |= ACL_DEFAULT;
		}
		
		hpux_entry.a_perm = 
			smb_perm_to_hpux_perm(smb_entry->a_perm);
		DEBUG(10, ("assembled the following hpux ace:\n"));
		DEBUGADD(10, (" - type: 0x%04x\n", hpux_entry.a_type));
		DEBUGADD(10, (" - id: %d\n", hpux_entry.a_id));
		DEBUGADD(10, (" - perm: o%o\n", hpux_entry.a_perm));
		if (!hpux_add_to_acl(hpux_acl, count, &hpux_entry, 
					1, type))
		{
			DEBUG(10, ("error adding acl entry\n"));
			goto fail;
		}
		DEBUG(10, ("count after adding: %d (i: %d)\n", *count, i));
		DEBUG(10, ("test, if entry has been copied into acl:\n"));
		DEBUGADD(10, (" - type: 0x%04x\n",
			      (*hpux_acl)[(*count)-1].a_type));
		DEBUGADD(10, (" - id: %d\n",
			      (*hpux_acl)[(*count)-1].a_id));
		DEBUGADD(10, (" - perm: o%o\n",
			      (*hpux_acl)[(*count)-1].a_perm));
	}

	ret = True;
	goto done;
	
 fail:
	SAFE_FREE(*hpux_acl);
 done:
	DEBUG(10, ("smb_acl_to_hpux_acl %s\n",
		   ((ret == True) ? "succeeded" : "failed")));
	return ret;
}

/* 
 * convert either the access or the default part of a 
 * soaris acl to the SMB_ACL format.
 */
static SMB_ACL_T hpux_acl_to_smb_acl(HPUX_ACL_T hpux_acl, int count, 
					SMB_ACL_TYPE_T type)
{
	SMB_ACL_T result;
	int i;

	if ((result = sys_acl_init(0)) == NULL) {
		DEBUG(10, ("error allocating memory for SMB_ACL\n"));
		goto fail;
	}
	for (i = 0; i < count; i++) {
		SMB_ACL_ENTRY_T smb_entry;
		SMB_ACL_PERM_T smb_perm;
		
		if (!_IS_OF_TYPE(hpux_acl[i], type)) {
			continue;
		}
		result = SMB_REALLOC(result, 
				     sizeof(struct smb_acl_t) +
				     (sizeof(struct smb_acl_entry) *
				      (result->count + 1)));
		if (result == NULL) {
			DEBUG(10, ("error reallocating memory for SMB_ACL\n"));
			goto fail;
		}
		smb_entry = &result->acl[result->count];
		if (sys_acl_set_tag_type(smb_entry,
					 hpux_tag_to_smb_tag(hpux_acl[i].a_type)) != 0)
		{
			DEBUG(10, ("invalid tag type given: 0x%04x\n",
				   hpux_acl[i].a_type));
			goto fail;
		}
		/* intentionally not checking return code here: */
		sys_acl_set_qualifier(smb_entry, (void *)&hpux_acl[i].a_id);
		smb_perm = hpux_perm_to_smb_perm(hpux_acl[i].a_perm);
		if (sys_acl_set_permset(smb_entry, &smb_perm) != 0) {
			DEBUG(10, ("invalid permset given: %d\n", 
				   hpux_acl[i].a_perm));
			goto fail;
		}
		result->count += 1;
	}
	goto done;
	
 fail:
	SAFE_FREE(result);
 done:
	DEBUG(10, ("hpux_acl_to_smb_acl %s\n",
		   ((result == NULL) ? "failed" : "succeeded")));
	return result;
}



static HPUX_ACL_TAG_T smb_tag_to_hpux_tag(SMB_ACL_TAG_T smb_tag)
{
	HPUX_ACL_TAG_T hpux_tag = 0;

	DEBUG(10, ("smb_tag_to_hpux_tag\n"));
	DEBUGADD(10, (" --> got smb tag 0x%04x\n", smb_tag));
	
	switch (smb_tag) {
	case SMB_ACL_USER:
		hpux_tag = USER;
		break;
	case SMB_ACL_USER_OBJ:
		hpux_tag = USER_OBJ;
		break;
	case SMB_ACL_GROUP:
		hpux_tag = GROUP;
		break;
	case SMB_ACL_GROUP_OBJ:
		hpux_tag = GROUP_OBJ;
		break;
	case SMB_ACL_OTHER:
		hpux_tag = OTHER_OBJ;
		break;
	case SMB_ACL_MASK:
		hpux_tag = CLASS_OBJ;
		break;
	default:
		DEBUGADD(10, (" !!! unknown smb tag type 0x%04x\n", smb_tag));
		break;
	}
	
	DEBUGADD(10, (" --> determined hpux tag 0x%04x\n", hpux_tag));

	return hpux_tag;
}

static SMB_ACL_TAG_T hpux_tag_to_smb_tag(HPUX_ACL_TAG_T hpux_tag)
{
	SMB_ACL_TAG_T smb_tag = 0;

	DEBUG(10, ("hpux_tag_to_smb_tag:\n"));
	DEBUGADD(10, (" --> got hpux tag 0x%04x\n", hpux_tag)); 
	
	hpux_tag &= ~ACL_DEFAULT; 

	switch (hpux_tag) {
	case USER:
		smb_tag = SMB_ACL_USER;
		break;
	case USER_OBJ:
		smb_tag = SMB_ACL_USER_OBJ;
		break;
	case GROUP:
		smb_tag = SMB_ACL_GROUP;
		break;
	case GROUP_OBJ:
		smb_tag = SMB_ACL_GROUP_OBJ;
		break;
	case OTHER_OBJ:
		smb_tag = SMB_ACL_OTHER;
		break;
	case CLASS_OBJ:
		smb_tag = SMB_ACL_MASK;
		break;
	default:
		DEBUGADD(10, (" !!! unknown hpux tag type: 0x%04x\n", 
					hpux_tag));
		break;
	}

	DEBUGADD(10, (" --> determined smb tag 0x%04x\n", smb_tag));
	
	return smb_tag;
}


/* 
 * The permission bits used in the following two permission conversion 
 * functions are same, but the functions make us independent of the concrete 
 * permission data types.
 */
static SMB_ACL_PERM_T hpux_perm_to_smb_perm(const HPUX_PERM_T perm)
{
	SMB_ACL_PERM_T smb_perm = 0;
	smb_perm |= ((perm & SMB_ACL_READ) ? SMB_ACL_READ : 0);
	smb_perm |= ((perm & SMB_ACL_WRITE) ? SMB_ACL_WRITE : 0);
	smb_perm |= ((perm & SMB_ACL_EXECUTE) ? SMB_ACL_EXECUTE : 0);
	return smb_perm;
}


static HPUX_PERM_T smb_perm_to_hpux_perm(const SMB_ACL_PERM_T perm)
{
	HPUX_PERM_T hpux_perm = 0;
	hpux_perm |= ((perm & SMB_ACL_READ) ? SMB_ACL_READ : 0);
	hpux_perm |= ((perm & SMB_ACL_WRITE) ? SMB_ACL_WRITE : 0);
	hpux_perm |= ((perm & SMB_ACL_EXECUTE) ? SMB_ACL_EXECUTE : 0);
	return hpux_perm;
}


static BOOL hpux_acl_get_file(const char *name, HPUX_ACL_T *hpux_acl, 
				 int *count)
{
	BOOL result = False;
	static HPUX_ACE_T dummy_ace;

	DEBUG(10, ("hpux_acl_get_file called for file '%s'\n", name));
	
	/* 
	 * The original code tries some INITIAL_ACL_SIZE
	 * and only did the ACL_CNT call upon failure
	 * (for performance reasons).
	 * For the sake of simplicity, I skip this for now. 
	 *
	 * NOTE: There is a catch here on HP-UX: acl with cmd parameter
	 * ACL_CNT fails with errno EINVAL when called with a NULL
	 * pointer as last argument. So we need to use a dummy acl
	 * struct here (we make it static so it does not need to be
	 * instantiated or malloced each time this function is
	 * called). Btw: the count parameter does not seem to matter...
	 */
	*count = acl(CONST_DISCARD(char *, name), ACL_CNT, 0, &dummy_ace);
	if (*count < 0) {
		DEBUG(10, ("acl ACL_CNT failed: %s\n", strerror(errno)));
		goto done;
	}
	*hpux_acl = hpux_acl_init(*count);
	if (*hpux_acl == NULL) {
		DEBUG(10, ("error allocating memory for hpux acl...\n"));
		goto done;
	}
	*count = acl(CONST_DISCARD(char *, name), ACL_GET, *count, *hpux_acl);
	if (*count < 0) {
		DEBUG(10, ("acl ACL_GET failed: %s\n", strerror(errno)));
		goto done;
	}
	result = True;

 done:
        DEBUG(10, ("hpux_acl_get_file %s.\n",
		   ((result == True) ? "succeeded" : "failed" )));
	return result;
}




/*
 * Add entries to a hpux ACL.
 *
 * Entries are directly added to the hpuxacl parameter.
 * if memory allocation fails, this may result in hpuxacl 
 * being NULL. if the resulting acl is to be checked and is 
 * not valid, it is kept in hpuxacl but False is returned.
 *
 * The type of ACEs (access/default) to be added to the ACL can 
 * be selected via the type parameter. 
 * I use the SMB_ACL_TYPE_T type here. Since SMB_ACL_TYPE_ACCESS
 * is defined as "0", this means that one can only add either
 * access or default ACEs from the given ACL, not both at the same 
 * time. If it should become necessary to add all of an ACL, one 
 * would have to replace this parameter by another type.
 */
static BOOL hpux_add_to_acl(HPUX_ACL_T *hpux_acl, int *count,
			       HPUX_ACL_T add_acl, int add_count, 
			       SMB_ACL_TYPE_T type)
{
	int i;
	
	if ((type != SMB_ACL_TYPE_ACCESS) && (type != SMB_ACL_TYPE_DEFAULT)) 
	{
		DEBUG(10, ("invalid acl type given: %d\n", type));
		errno = EINVAL;
		return False;
	}
	for (i = 0; i < add_count; i++) {
		if (!_IS_OF_TYPE(add_acl[i], type)) {
			continue;
		}
		ADD_TO_ARRAY(NULL, HPUX_ACE_T, add_acl[i], 
			     hpux_acl, count);
		if (hpux_acl == NULL) {
			DEBUG(10, ("error enlarging acl.\n"));
			errno = ENOMEM;
			return False;
		}
	}
	return True;
}


/* 
 * sort the ACL and check it for validity
 *
 * [original comment from lib/sysacls.c:]
 * 
 * if it's a minimal ACL with only 4 entries then we
 * need to recalculate the mask permissions to make
 * sure that they are the same as the GROUP_OBJ
 * permissions as required by the UnixWare acl() system call.
 *
 * (note: since POSIX allows minimal ACLs which only contain
 * 3 entries - ie there is no mask entry - we should, in theory,
 * check for this and add a mask entry if necessary - however
 * we "know" that the caller of this interface always specifies
 * a mask, so in practice "this never happens" (tm) - if it *does*
 * happen aclsort() will fail and return an error and someone will
 * have to fix it...)
 */
static BOOL hpux_acl_sort(HPUX_ACL_T hpux_acl, int count)
{
	int fixmask = (count <= 4);

	if (hpux_internal_aclsort(count, fixmask, hpux_acl) != 0) {
		errno = EINVAL;
		return False;
	}
	return True;
}


/*
 * Helpers for hpux_internal_aclsort:
 *   - hpux_count_obj
 *   - hpux_swap_acl_entries
 *   - hpux_prohibited_duplicate_type
 *   - hpux_get_needed_class_perm
 */

/* hpux_count_obj:
 * Counts the different number of objects in a given array of ACL
 * structures.
 * Inputs:
 *
 * acl_count      - Count of ACLs in the array of ACL strucutres.
 * aclp           - Array of ACL structures.
 * acl_type_count - Pointer to acl_types structure. Should already be
 *                  allocated.
 * Output: 
 *
 * acl_type_count - This structure is filled up with counts of various 
 *                  acl types.
 */

static void hpux_count_obj(int acl_count, HPUX_ACL_T aclp, struct hpux_acl_types *acl_type_count)
{
	int i;

	memset(acl_type_count, 0, sizeof(struct hpux_acl_types));

	for(i=0;i<acl_count;i++) {
		switch(aclp[i].a_type) {
		case USER: 
			acl_type_count->n_user++;
			break;
		case USER_OBJ: 
			acl_type_count->n_user_obj++;
			break;
		case DEF_USER_OBJ: 
			acl_type_count->n_def_user_obj++;
			break;
		case GROUP: 
			acl_type_count->n_group++;
			break;
		case GROUP_OBJ: 
			acl_type_count->n_group_obj++;
			break;
		case DEF_GROUP_OBJ: 
			acl_type_count->n_def_group_obj++;
			break;
		case OTHER_OBJ: 
			acl_type_count->n_other_obj++;
			break;
		case DEF_OTHER_OBJ: 
			acl_type_count->n_def_other_obj++;
			break;
		case CLASS_OBJ:
			acl_type_count->n_class_obj++;
			break;
		case DEF_CLASS_OBJ:
			acl_type_count->n_def_class_obj++;
			break;
		case DEF_USER:
			acl_type_count->n_def_user++;
			break;
		case DEF_GROUP:
			acl_type_count->n_def_group++;
			break;
		default: 
			acl_type_count->n_illegal_obj++;
			break;
		}
	}
}

/* hpux_swap_acl_entries:  Swaps two ACL entries. 
 *
 * Inputs: aclp0, aclp1 - ACL entries to be swapped.
 */

static void hpux_swap_acl_entries(HPUX_ACE_T *aclp0, HPUX_ACE_T *aclp1)
{
	HPUX_ACE_T temp_acl;

	temp_acl.a_type = aclp0->a_type;
	temp_acl.a_id = aclp0->a_id;
	temp_acl.a_perm = aclp0->a_perm;

	aclp0->a_type = aclp1->a_type;
	aclp0->a_id = aclp1->a_id;
	aclp0->a_perm = aclp1->a_perm;

	aclp1->a_type = temp_acl.a_type;
	aclp1->a_id = temp_acl.a_id;
	aclp1->a_perm = temp_acl.a_perm;
}

/* hpux_prohibited_duplicate_type
 * Identifies if given ACL type can have duplicate entries or 
 * not.
 *
 * Inputs: acl_type - ACL Type.
 *
 * Outputs: 
 *
 * Return.. 
 *
 * True - If the ACL type matches any of the prohibited types.
 * False - If the ACL type doesn't match any of the prohibited types.
 */ 

static BOOL hpux_prohibited_duplicate_type(int acl_type)
{
	switch(acl_type) {
		case USER:
		case GROUP:
		case DEF_USER: 
		case DEF_GROUP:
			return True;
		default:
			return False;
	}
}

/* hpux_get_needed_class_perm
 * Returns the permissions of a ACL structure only if the ACL
 * type matches one of the pre-determined types for computing 
 * CLASS_OBJ permissions.
 *
 * Inputs: aclp - Pointer to ACL structure.
 */

static int hpux_get_needed_class_perm(struct acl *aclp)
{
	switch(aclp->a_type) {
		case USER: 
		case GROUP_OBJ: 
		case GROUP: 
		case DEF_USER_OBJ: 
		case DEF_USER:
		case DEF_GROUP_OBJ: 
		case DEF_GROUP:
		case DEF_CLASS_OBJ:
		case DEF_OTHER_OBJ: 
			return aclp->a_perm;
		default: 
			return 0;
	}
}

/* hpux_internal_aclsort: aclsort for HPUX.
 *
 * -> The aclsort() system call is availabe on the latest HPUX General
 * -> Patch Bundles. So for HPUX, we developed our version of aclsort 
 * -> function. Because, we don't want to update to a new 
 * -> HPUX GR bundle just for aclsort() call.
 *
 * aclsort sorts the array of ACL structures as per the description in
 * aclsort man page. Refer to aclsort man page for more details
 *
 * Inputs:
 *
 * acl_count - Count of ACLs in the array of ACL structures.
 * calclass  - If this is not zero, then we compute the CLASS_OBJ
 *             permissions.
 * aclp      - Array of ACL structures.
 *
 * Outputs:
 *
 * aclp     - Sorted array of ACL structures.
 *
 * Outputs:
 *
 * Returns 0 for success -1 for failure. Prints a message to the Samba
 * debug log in case of failure.
 */

static int hpux_internal_aclsort(int acl_count, int calclass, HPUX_ACL_T aclp)
{
	struct hpux_acl_types acl_obj_count;
	int n_class_obj_perm = 0;
	int i, j;
 
	DEBUG(10,("Entering hpux_internal_aclsort. (calclass = %d)\n", calclass));

	if (hpux_aclsort_call_present()) {
		DEBUG(10, ("calling hpux aclsort\n"));
		return aclsort(acl_count, calclass, aclp);
	}

	DEBUG(10, ("using internal aclsort\n"));

	if(!acl_count) {
		DEBUG(10,("Zero acl count passed. Returning Success\n"));
		return 0;
	}

	if(aclp == NULL) {
		DEBUG(0,("Null ACL pointer in hpux_acl_sort. Returning Failure. \n"));
		return -1;
	}

	/* Count different types of ACLs in the ACLs array */

	hpux_count_obj(acl_count, aclp, &acl_obj_count);

	/* There should be only one entry each of type USER_OBJ, GROUP_OBJ, 
	 * CLASS_OBJ and OTHER_OBJ 
	 */

	if ( (acl_obj_count.n_user_obj  != 1) || 
		(acl_obj_count.n_group_obj != 1) || 
		(acl_obj_count.n_class_obj != 1) ||
		(acl_obj_count.n_other_obj != 1) ) 
	{
		DEBUG(0,("hpux_internal_aclsort: More than one entry or no entries for \
USER OBJ or GROUP_OBJ or OTHER_OBJ or CLASS_OBJ\n"));
		return -1;
	}

	/* If any of the default objects are present, there should be only
	 * one of them each.
	 */

	if ( (acl_obj_count.n_def_user_obj  > 1) || 
		(acl_obj_count.n_def_group_obj > 1) || 
		(acl_obj_count.n_def_other_obj > 1) || 
		(acl_obj_count.n_def_class_obj > 1) ) 
	{
		DEBUG(0,("hpux_internal_aclsort: More than one entry for DEF_CLASS_OBJ \
or DEF_USER_OBJ or DEF_GROUP_OBJ or DEF_OTHER_OBJ\n"));
		return -1;
	}

	/* We now have proper number of OBJ and DEF_OBJ entries. Now sort the acl 
	 * structures.  
	 *
	 * Sorting crieteria - First sort by ACL type. If there are multiple entries of
	 * same ACL type, sort by ACL id.
	 *
	 * I am using the trival kind of sorting method here because, performance isn't 
	 * really effected by the ACLs feature. More over there aren't going to be more
	 * than 17 entries on HPUX. 
	 */

	for(i=0; i<acl_count;i++) {
		for (j=i+1; j<acl_count; j++) {
			if( aclp[i].a_type > aclp[j].a_type ) {
				/* ACL entries out of order, swap them */
				hpux_swap_acl_entries((aclp+i), (aclp+j));
			} else if ( aclp[i].a_type == aclp[j].a_type ) {
				/* ACL entries of same type, sort by id */
				if(aclp[i].a_id > aclp[j].a_id) {
					hpux_swap_acl_entries((aclp+i), (aclp+j));
				} else if (aclp[i].a_id == aclp[j].a_id) {
					/* We have a duplicate entry. */
					if(hpux_prohibited_duplicate_type(aclp[i].a_type)) {
						DEBUG(0, ("hpux_internal_aclsort: Duplicate entry: Type(hex): %x Id: %d\n",
							aclp[i].a_type, aclp[i].a_id));
						return -1;
					}
				}
			}
		}
	}

	/* set the class obj permissions to the computed one. */
	if(calclass) {
		int n_class_obj_index = -1;

		for(i=0;i<acl_count;i++) {
			n_class_obj_perm |= hpux_get_needed_class_perm((aclp+i));

			if(aclp[i].a_type == CLASS_OBJ)
				n_class_obj_index = i;
		}
		aclp[n_class_obj_index].a_perm = n_class_obj_perm;
	}

	return 0;
}


/* 
 * hpux_acl_call_present:
 *
 * This checks if the POSIX ACL system call is defined
 * which basically corresponds to whether JFS 3.3 or
 * higher is installed. If acl() was called when it
 * isn't defined, it causes the process to core dump
 * so it is important to check this and avoid acl()
 * calls if it isn't there.                            
 */

static BOOL hpux_acl_call_present(void)
{

	shl_t handle = NULL;
	void *value;
	int ret_val=0;
	static BOOL already_checked = False;

	if(already_checked)
		return True;

	errno = 0;

	ret_val = shl_findsym(&handle, "acl", TYPE_PROCEDURE, &value);

	if(ret_val != 0) {
		DEBUG(5, ("hpux_acl_call_present: shl_findsym() returned %d, errno = %d, error %s\n",
			ret_val, errno, strerror(errno)));
		DEBUG(5,("hpux_acl_call_present: acl() system call is not present. Check if you have JFS 3.3 and above?\n"));
		errno = ENOSYS;
		return False;
	}

	DEBUG(10,("hpux_acl_call_present: acl() system call is present. We have JFS 3.3 or above \n"));

	already_checked = True;
	return True;
}

/* 
 * runtime check for presence of aclsort library call. 
 * same code as for acl call. if there are more of these,
 * a dispatcher function could be handy...
 */

static BOOL hpux_aclsort_call_present(void) 
{
	shl_t handle = NULL;
	void *value;
	int ret_val = 0;
	static BOOL already_checked = False;

	if (already_checked) {
		return True;
	}

	errno = 0;
	ret_val = shl_findsym(&handle, "aclsort", TYPE_PROCEDURE, &value);
	if (ret_val != 0) {
		DEBUG(5, ("hpux_aclsort_call_present: shl_findsym "
			"returned %d, errno = %d, error %s", 
			ret_val, errno, strerror(errno)));
		DEBUG(5, ("hpux_aclsort_call_present: "
			"aclsort() function not available.\n"));
		return False;
	}
	DEBUG(10,("hpux_aclsort_call_present: aclsort() function present.\n"));
	already_checked = True;
	return True;
}

#if 0
/*
 * acl check function:
 *   unused at the moment but could be used to get more
 *   concrete error messages for debugging...
 *   (acl sort just says that the acl is invalid...)
 */
static BOOL hpux_acl_check(HPUX_ACL_T hpux_acl, int count)
{
	int check_rc;
	int check_which;
	
	check_rc = aclcheck(hpux_acl, count, &check_which);
	if (check_rc != 0) {
		DEBUG(10, ("acl is not valid:\n"));
		DEBUGADD(10, (" - return code: %d\n", check_rc));
		DEBUGADD(10, (" - which: %d\n", check_which));
		if (check_which != -1) {
			DEBUGADD(10, (" - invalid entry:\n"));
			DEBUGADD(10, ("   * type: %d:\n", 
				      hpux_acl[check_which].a_type));
			DEBUGADD(10, ("   * id: %d\n",
				      hpux_acl[check_which].a_id));
			DEBUGADD(10, ("   * perm: 0o%o\n",
				      hpux_acl[check_which].a_perm));
		}
		return False;
	}
	return True;
}
#endif

/* VFS operations structure */

static vfs_op_tuple hpuxacl_op_tuples[] = {
	/* Disk operations */
	{SMB_VFS_OP(hpuxacl_sys_acl_get_file),
	 SMB_VFS_OP_SYS_ACL_GET_FILE,
	 SMB_VFS_LAYER_TRANSPARENT},

	{SMB_VFS_OP(hpuxacl_sys_acl_get_fd),
	 SMB_VFS_OP_SYS_ACL_GET_FD,
	 SMB_VFS_LAYER_TRANSPARENT},

	{SMB_VFS_OP(hpuxacl_sys_acl_set_file),
	 SMB_VFS_OP_SYS_ACL_SET_FILE,
	 SMB_VFS_LAYER_TRANSPARENT},

	{SMB_VFS_OP(hpuxacl_sys_acl_set_fd),
	 SMB_VFS_OP_SYS_ACL_SET_FD,
	 SMB_VFS_LAYER_TRANSPARENT},

	{SMB_VFS_OP(hpuxacl_sys_acl_delete_def_file),
	 SMB_VFS_OP_SYS_ACL_DELETE_DEF_FILE,
	 SMB_VFS_LAYER_TRANSPARENT},

	{SMB_VFS_OP(NULL),
	 SMB_VFS_OP_NOOP,
	 SMB_VFS_LAYER_NOOP}
};

NTSTATUS vfs_hpuxacl_init(void)
{
	return smb_register_vfs(SMB_VFS_INTERFACE_VERSION, "hpuxacl",
				hpuxacl_op_tuples);
}

/* ENTE */
