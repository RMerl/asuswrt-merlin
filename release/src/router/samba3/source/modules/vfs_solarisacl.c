/*
   Unix SMB/Netbios implementation.
   VFS module to get and set Solaris ACLs
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


/* typedef struct acl SOLARIS_ACE_T; */
typedef aclent_t SOLARIS_ACE_T;
typedef aclent_t *SOLARIS_ACL_T;
typedef int SOLARIS_ACL_TAG_T;   /* the type of an ACL entry */
typedef o_mode_t SOLARIS_PERM_T;

/* for convenience: check if solaris acl entry is a default entry?  */
#define _IS_DEFAULT(ace) ((ace).a_type & ACL_DEFAULT)
#define _IS_OF_TYPE(ace, type) ( \
	(((type) == SMB_ACL_TYPE_ACCESS) && !_IS_DEFAULT(ace)) \
	|| \
	(((type) == SMB_ACL_TYPE_DEFAULT) && _IS_DEFAULT(ace)) \
)


/* prototypes for private functions */

static SOLARIS_ACL_T solaris_acl_init(int count);
static BOOL smb_acl_to_solaris_acl(SMB_ACL_T smb_acl, 
		SOLARIS_ACL_T *solariacl, int *count, 
		SMB_ACL_TYPE_T type);
static SMB_ACL_T solaris_acl_to_smb_acl(SOLARIS_ACL_T solarisacl, int count,
		SMB_ACL_TYPE_T type);
static SOLARIS_ACL_TAG_T smb_tag_to_solaris_tag(SMB_ACL_TAG_T smb_tag);
static SMB_ACL_TAG_T solaris_tag_to_smb_tag(SOLARIS_ACL_TAG_T solaris_tag);
static BOOL solaris_add_to_acl(SOLARIS_ACL_T *solaris_acl, int *count,
		SOLARIS_ACL_T add_acl, int add_count, SMB_ACL_TYPE_T type);
static BOOL solaris_acl_get_file(const char *name, SOLARIS_ACL_T *solarisacl, 
		int *count);
static BOOL solaris_acl_get_fd(int fd, SOLARIS_ACL_T *solarisacl, int *count);
static BOOL solaris_acl_sort(SOLARIS_ACL_T acl, int count);
static SMB_ACL_PERM_T solaris_perm_to_smb_perm(const SOLARIS_PERM_T perm);
static SOLARIS_PERM_T smb_perm_to_solaris_perm(const SMB_ACL_PERM_T perm);
static BOOL solaris_acl_check(SOLARIS_ACL_T solaris_acl, int count);


/* public functions - the api */

SMB_ACL_T solarisacl_sys_acl_get_file(vfs_handle_struct *handle,
				      const char *path_p,
				      SMB_ACL_TYPE_T type)
{
	SMB_ACL_T result = NULL;
	int count;
	SOLARIS_ACL_T solaris_acl = NULL;
	
	DEBUG(10, ("solarisacl_sys_acl_get_file called for file '%s'.\n", 
		   path_p));

	if (type != SMB_ACL_TYPE_ACCESS && type != SMB_ACL_TYPE_DEFAULT) {
		DEBUG(10, ("invalid SMB_ACL_TYPE given (%d)\n", type));
		errno = EINVAL;
		goto done;
	}

	DEBUGADD(10, ("getting %s acl\n", 
		      ((type == SMB_ACL_TYPE_ACCESS) ? "access" : "default")));

	if (!solaris_acl_get_file(path_p, &solaris_acl, &count)) {
		goto done;
	}
	result = solaris_acl_to_smb_acl(solaris_acl, count, type);
	if (result == NULL) {
		DEBUG(10, ("conversion solaris_acl -> smb_acl failed (%s).\n",
			   strerror(errno)));
	}
	
 done:
	DEBUG(10, ("solarisacl_sys_acl_get_file %s.\n",
		   ((result == NULL) ? "failed" : "succeeded" )));
	SAFE_FREE(solaris_acl);
	return result;
}


/*
 * get the access ACL of a file referred to by a fd
 */
SMB_ACL_T solarisacl_sys_acl_get_fd(vfs_handle_struct *handle,
				    files_struct *fsp,
				    int fd)
{
	SMB_ACL_T result = NULL;
	int count;
	SOLARIS_ACL_T solaris_acl = NULL;

	DEBUG(10, ("entering solarisacl_sys_acl_get_fd.\n"));

	if (!solaris_acl_get_fd(fd, &solaris_acl, &count)) {
		goto done;
	}
	/* 
	 * The facl call returns both ACCESS and DEFAULT acls (as present). 
	 * The posix acl_get_fd function returns only the
	 * access acl. So we need to filter this out here.  
	 */
	result = solaris_acl_to_smb_acl(solaris_acl, count,
					SMB_ACL_TYPE_ACCESS);
	if (result == NULL) {
		DEBUG(10, ("conversion solaris_acl -> smb_acl failed (%s).\n",
			   strerror(errno)));
	}
	
 done:
	DEBUG(10, ("solarisacl_sys_acl_get_fd %s.\n", 
		   ((result == NULL) ? "failed" : "succeeded")));
	SAFE_FREE(solaris_acl);
	return result;
}

int solarisacl_sys_acl_set_file(vfs_handle_struct *handle,
				const char *name,
				SMB_ACL_TYPE_T type,
				SMB_ACL_T theacl)
{
	int ret = -1;
	struct stat s;
	SOLARIS_ACL_T solaris_acl = NULL;
	int count;
	
	DEBUG(10, ("solarisacl_sys_acl_set_file called for file '%s'\n",
		   name));

	if ((type != SMB_ACL_TYPE_ACCESS) && (type != SMB_ACL_TYPE_DEFAULT)) {
		errno = EINVAL;
		DEBUG(10, ("invalid smb acl type given (%d).\n", type));
		goto done;
	}
	DEBUGADD(10, ("setting %s acl\n", 
		      ((type == SMB_ACL_TYPE_ACCESS) ? "access" : "default")));

	if(!smb_acl_to_solaris_acl(theacl, &solaris_acl, &count, type)) {
		DEBUG(10, ("conversion smb_acl -> solaris_acl failed (%s).\n",
			   strerror(errno)));
                goto done;
	}

	/*
	 * if the file is a directory, there is extra work to do:
	 * since the solaris acl call stores both the access acl and 
	 * the default acl as provided, we have to get the acl part 
	 * that has not been specified in "type" from the file first 
	 * and concatenate it with the acl provided.
	 */
	if (SMB_VFS_STAT(handle->conn, name, &s) != 0) {
		DEBUG(10, ("Error in stat call: %s\n", strerror(errno)));
		goto done;
	}
	if (S_ISDIR(s.st_mode)) {
		SOLARIS_ACL_T other_acl; 
		int other_count;
		SMB_ACL_TYPE_T other_type;

		other_type = (type == SMB_ACL_TYPE_ACCESS) 
			? SMB_ACL_TYPE_DEFAULT
			: SMB_ACL_TYPE_ACCESS;
		DEBUGADD(10, ("getting acl from filesystem\n"));
		if (!solaris_acl_get_file(name, &other_acl, &other_count)) {
			DEBUG(10, ("error getting acl from directory\n"));
			goto done;
		}
		DEBUG(10, ("adding %s part of fs acl to given acl\n",
			   ((other_type == SMB_ACL_TYPE_ACCESS) 
			    ? "access"
			    : "default")));
		if (!solaris_add_to_acl(&solaris_acl, &count, other_acl,
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

	if (!solaris_acl_sort(solaris_acl, count)) {
		DEBUG(10, ("resulting acl is not valid!\n"));
		goto done;
	}

	ret = acl(name, SETACL, count, solaris_acl);
	
 done:
	DEBUG(10, ("solarisacl_sys_acl_set_file %s.\n",
		   ((ret != 0) ? "failed" : "succeeded")));
	SAFE_FREE(solaris_acl);
	return ret;
}

/*
 * set the access ACL on the file referred to by a fd 
 */
int solarisacl_sys_acl_set_fd(vfs_handle_struct *handle,
			      files_struct *fsp,
			      int fd, SMB_ACL_T theacl)
{
	SOLARIS_ACL_T solaris_acl = NULL;
	SOLARIS_ACL_T default_acl = NULL;
	int count, default_count;
	int ret = -1;

	DEBUG(10, ("entering solarisacl_sys_acl_set_fd\n"));

	/* 
	 * the posix acl_set_fd call sets the access acl of the
	 * file referred to by fd. the solaris facl-SETACL call
	 * sets the access and default acl as provided, so we
	 * have to retrieve the default acl of the file and 
	 * concatenate it with the access acl provided.
	 */
	if (!smb_acl_to_solaris_acl(theacl, &solaris_acl, &count, 
				    SMB_ACL_TYPE_ACCESS))
	{
		DEBUG(10, ("conversion smb_acl -> solaris_acl failed (%s).\n",
			   strerror(errno)));
		goto done;
	}
	if (!solaris_acl_get_fd(fd, &default_acl, &default_count)) {
		DEBUG(10, ("error getting (default) acl from fd\n"));
		goto done;
	}
	if (!solaris_add_to_acl(&solaris_acl, &count,
				default_acl, default_count,
				SMB_ACL_TYPE_DEFAULT))
	{
		DEBUG(10, ("error adding default acl to solaris acl\n"));
		goto done;
	}
	if (!solaris_acl_sort(solaris_acl, count)) {
		DEBUG(10, ("resulting acl is not valid!\n"));
		goto done;
	}

	ret = facl(fd, SETACL, count, solaris_acl);
	if (ret != 0) {
		DEBUG(10, ("call of facl failed (%s).\n", strerror(errno)));
	}

 done:
	DEBUG(10, ("solarisacl_sys_acl_st_fd %s.\n",
		   ((ret == 0) ? "succeded" : "failed" )));
	SAFE_FREE(solaris_acl);
	SAFE_FREE(default_acl);
	return ret;
}

/*
 * delete the default ACL of a directory
 *
 * This is achieved by fetching the access ACL and rewriting it 
 * directly, via the solaris system call: the SETACL call on 
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
int solarisacl_sys_acl_delete_def_file(vfs_handle_struct *handle,
				       const char *path)
{
	SMB_ACL_T smb_acl;
	int ret = -1;
	SOLARIS_ACL_T solaris_acl = NULL;
	int count;

	DEBUG(10, ("entering solarisacl_sys_acl_delete_def_file.\n"));
	
	smb_acl = solarisacl_sys_acl_get_file(handle, path, 
					      SMB_ACL_TYPE_ACCESS);
	if (smb_acl == NULL) {
		DEBUG(10, ("getting file acl failed!\n"));
		goto done;
	}
	if (!smb_acl_to_solaris_acl(smb_acl, &solaris_acl, &count, 
				    SMB_ACL_TYPE_ACCESS))
	{
		DEBUG(10, ("conversion smb_acl -> solaris_acl failed.\n"));
		goto done;
	}
	if (!solaris_acl_sort(solaris_acl, count)) {
		DEBUG(10, ("resulting acl is not valid!\n"));
		goto done;
	}
	ret = acl(path, SETACL, count, solaris_acl);
	if (ret != 0) {
		DEBUG(10, ("settinge file acl failed!\n"));
	}
	
 done:
	DEBUG(10, ("solarisacl_sys_acl_delete_def_file %s.\n",
		   ((ret != 0) ? "failed" : "succeeded" )));
	SAFE_FREE(smb_acl);
	return ret;
}


/* private functions */

static SOLARIS_ACL_T solaris_acl_init(int count)
{
	SOLARIS_ACL_T solaris_acl = 
		(SOLARIS_ACL_T)SMB_MALLOC(sizeof(aclent_t) * count);
	if (solaris_acl == NULL) {
		errno = ENOMEM;
	}
	return solaris_acl;
}

/*
 * Convert the SMB acl to the ACCESS or DEFAULT part of a 
 * solaris ACL, as desired.
 */
static BOOL smb_acl_to_solaris_acl(SMB_ACL_T smb_acl, 
				   SOLARIS_ACL_T *solaris_acl, int *count, 
				   SMB_ACL_TYPE_T type)
{
	BOOL ret = False;
	int i;
	int check_which, check_rc;

	DEBUG(10, ("entering smb_acl_to_solaris_acl\n"));
	
	*solaris_acl = NULL;
	*count = 0;

	for (i = 0; i < smb_acl->count; i++) {
		const struct smb_acl_entry *smb_entry = &(smb_acl->acl[i]);
		SOLARIS_ACE_T solaris_entry;

		ZERO_STRUCT(solaris_entry);

		solaris_entry.a_type = smb_tag_to_solaris_tag(smb_entry->a_type);
		if (solaris_entry.a_type == 0) {
			DEBUG(10, ("smb_tag to solaris_tag failed\n"));
			goto fail;
		}
		switch(solaris_entry.a_type) {
		case USER:
			DEBUG(10, ("got tag type USER with uid %d\n", 
				   smb_entry->uid));
			solaris_entry.a_id = (uid_t)smb_entry->uid;
			break;
		case GROUP:
			DEBUG(10, ("got tag type GROUP with gid %d\n", 
				   smb_entry->gid));
			solaris_entry.a_id = (uid_t)smb_entry->gid;
			break;
		default:
			break;
		}
		if (type == SMB_ACL_TYPE_DEFAULT) {
			DEBUG(10, ("adding default bit to solaris ace\n"));
			solaris_entry.a_type |= ACL_DEFAULT;
		}
		
		solaris_entry.a_perm = 
			smb_perm_to_solaris_perm(smb_entry->a_perm);
		DEBUG(10, ("assembled the following solaris ace:\n"));
		DEBUGADD(10, (" - type: 0x%04x\n", solaris_entry.a_type));
		DEBUGADD(10, (" - id: %d\n", solaris_entry.a_id));
		DEBUGADD(10, (" - perm: o%o\n", solaris_entry.a_perm));
		if (!solaris_add_to_acl(solaris_acl, count, &solaris_entry, 
					1, type))
		{
			DEBUG(10, ("error adding acl entry\n"));
			goto fail;
		}
		DEBUG(10, ("count after adding: %d (i: %d)\n", *count, i));
		DEBUG(10, ("test, if entry has been copied into acl:\n"));
		DEBUGADD(10, (" - type: 0x%04x\n",
			      (*solaris_acl)[(*count)-1].a_type));
		DEBUGADD(10, (" - id: %d\n",
			      (*solaris_acl)[(*count)-1].a_id));
		DEBUGADD(10, (" - perm: o%o\n",
			      (*solaris_acl)[(*count)-1].a_perm));
	}

	ret = True;
	goto done;
	
 fail:
	SAFE_FREE(*solaris_acl);
 done:
	DEBUG(10, ("smb_acl_to_solaris_acl %s\n",
		   ((ret == True) ? "succeeded" : "failed")));
	return ret;
}

/* 
 * convert either the access or the default part of a 
 * soaris acl to the SMB_ACL format.
 */
static SMB_ACL_T solaris_acl_to_smb_acl(SOLARIS_ACL_T solaris_acl, int count, 
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
		
		if (!_IS_OF_TYPE(solaris_acl[i], type)) {
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
					 solaris_tag_to_smb_tag(solaris_acl[i].a_type)) != 0)
		{
			DEBUG(10, ("invalid tag type given: 0x%04x\n",
				   solaris_acl[i].a_type));
			goto fail;
		}
		/* intentionally not checking return code here: */
		sys_acl_set_qualifier(smb_entry, (void *)&solaris_acl[i].a_id);
		smb_perm = solaris_perm_to_smb_perm(solaris_acl[i].a_perm);
		if (sys_acl_set_permset(smb_entry, &smb_perm) != 0) {
			DEBUG(10, ("invalid permset given: %d\n", 
				   solaris_acl[i].a_perm));
			goto fail;
		}
		result->count += 1;
	}
	goto done;
	
 fail:
	SAFE_FREE(result);
 done:
	DEBUG(10, ("solaris_acl_to_smb_acl %s\n",
		   ((result == NULL) ? "failed" : "succeeded")));
	return result;
}



static SOLARIS_ACL_TAG_T smb_tag_to_solaris_tag(SMB_ACL_TAG_T smb_tag)
{
	SOLARIS_ACL_TAG_T solaris_tag = 0;

	DEBUG(10, ("smb_tag_to_solaris_tag\n"));
	DEBUGADD(10, (" --> got smb tag 0x%04x\n", smb_tag));
	
	switch (smb_tag) {
	case SMB_ACL_USER:
		solaris_tag = USER;
		break;
	case SMB_ACL_USER_OBJ:
		solaris_tag = USER_OBJ;
		break;
	case SMB_ACL_GROUP:
		solaris_tag = GROUP;
		break;
	case SMB_ACL_GROUP_OBJ:
		solaris_tag = GROUP_OBJ;
		break;
	case SMB_ACL_OTHER:
		solaris_tag = OTHER_OBJ;
		break;
	case SMB_ACL_MASK:
		solaris_tag = CLASS_OBJ;
		break;
	default:
		DEBUGADD(10, (" !!! unknown smb tag type 0x%04x\n", smb_tag));
		break;
	}
	
	DEBUGADD(10, (" --> determined solaris tag 0x%04x\n", solaris_tag));

	return solaris_tag;
}

static SMB_ACL_TAG_T solaris_tag_to_smb_tag(SOLARIS_ACL_TAG_T solaris_tag)
{
	SMB_ACL_TAG_T smb_tag = 0;

	DEBUG(10, ("solaris_tag_to_smb_tag:\n"));
	DEBUGADD(10, (" --> got solaris tag 0x%04x\n", solaris_tag)); 
	
	solaris_tag &= ~ACL_DEFAULT; 

	switch (solaris_tag) {
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
		DEBUGADD(10, (" !!! unknown solaris tag type: 0x%04x\n", 
					solaris_tag));
		break;
	}

	DEBUGADD(10, (" --> determined smb tag 0x%04x\n", smb_tag));
	
	return smb_tag;
}


static SMB_ACL_PERM_T solaris_perm_to_smb_perm(const SOLARIS_PERM_T perm)
{
	SMB_ACL_PERM_T smb_perm = 0;
	smb_perm |= ((perm & SMB_ACL_READ) ? SMB_ACL_READ : 0);
	smb_perm |= ((perm & SMB_ACL_WRITE) ? SMB_ACL_WRITE : 0);
	smb_perm |= ((perm & SMB_ACL_EXECUTE) ? SMB_ACL_EXECUTE : 0);
	return smb_perm;
}


static SOLARIS_PERM_T smb_perm_to_solaris_perm(const SMB_ACL_PERM_T perm)
{
	SOLARIS_PERM_T solaris_perm = 0;
	solaris_perm |= ((perm & SMB_ACL_READ) ? SMB_ACL_READ : 0);
	solaris_perm |= ((perm & SMB_ACL_WRITE) ? SMB_ACL_WRITE : 0);
	solaris_perm |= ((perm & SMB_ACL_EXECUTE) ? SMB_ACL_EXECUTE : 0);
	return solaris_perm;
}


static BOOL solaris_acl_get_file(const char *name, SOLARIS_ACL_T *solaris_acl, 
				 int *count)
{
	BOOL result = False;

	DEBUG(10, ("solaris_acl_get_file called for file '%s'\n", name));
	
	/* 
	 * The original code tries some INITIAL_ACL_SIZE
	 * and only did the GETACLCNT call upon failure
	 * (for performance reasons).
	 * For the sake of simplicity, I skip this for now. 
	 */
	*count = acl(name, GETACLCNT, 0, NULL);
	if (*count < 0) {
		DEBUG(10, ("acl GETACLCNT failed: %s\n", strerror(errno)));
		goto done;
	}
	*solaris_acl = solaris_acl_init(*count);
	if (*solaris_acl == NULL) {
		DEBUG(10, ("error allocating memory for solaris acl...\n"));
		goto done;
	}
	*count = acl(name, GETACL, *count, *solaris_acl);
	if (*count < 0) {
		DEBUG(10, ("acl GETACL failed: %s\n", strerror(errno)));
		goto done;
	}
	result = True;

 done:
        DEBUG(10, ("solaris_acl_get_file %s.\n",
		   ((result == True) ? "succeeded" : "failed" )));
	return result;
}


static BOOL solaris_acl_get_fd(int fd, SOLARIS_ACL_T *solaris_acl, int *count)
{
	BOOL ret = False;

	DEBUG(10, ("entering solaris_acl_get_fd\n"));

	/* 
	 * see solaris_acl_get_file for comment about omission 
	 * of INITIAL_ACL_SIZE... 
	 */
	*count = facl(fd, GETACLCNT, 0, NULL);
	if (*count < 0) {
		DEBUG(10, ("facl GETACLCNT failed: %s\n", strerror(errno)));
		goto done;
	}
	*solaris_acl = solaris_acl_init(*count);
	if (*solaris_acl == NULL) {
		DEBUG(10, ("error allocating memory for solaris acl...\n"));
		goto done;
	}
	*count = facl(fd, GETACL, *count, *solaris_acl);
	if (*count < 0) {
		DEBUG(10, ("facl GETACL failed: %s\n", strerror(errno)));
		goto done;
	}
	ret = True;

 done:
	DEBUG(10, ("solaris_acl_get_fd %s\n",
		   ((ret == True) ? "succeeded" : "failed")));
	return ret;
}



/*
 * Add entries to a solaris ACL.
 *
 * Entries are directly added to the solarisacl parameter.
 * if memory allocation fails, this may result in solarisacl 
 * being NULL. if the resulting acl is to be checked and is 
 * not valid, it is kept in solarisacl but False is returned.
 *
 * The type of ACEs (access/default) to be added to the ACL can 
 * be selected via the type parameter. 
 * I use the SMB_ACL_TYPE_T type here. Since SMB_ACL_TYPE_ACCESS
 * is defined as "0", this means that one can only add either
 * access or default ACEs, not both at the same time. If it 
 * should become necessary to add all of an ACL, one would have
 * to replace this parameter by another type.
 */
static BOOL solaris_add_to_acl(SOLARIS_ACL_T *solaris_acl, int *count,
			       SOLARIS_ACL_T add_acl, int add_count, 
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
		ADD_TO_ARRAY(NULL, SOLARIS_ACE_T, add_acl[i], 
			     solaris_acl, count);
		if (solaris_acl == NULL) {
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
static BOOL solaris_acl_sort(SOLARIS_ACL_T solaris_acl, int count)
{
	int fixmask = (count <= 4);

	if (aclsort(count, fixmask, solaris_acl) != 0) {
		errno = EINVAL;
		return False;
	}
	return True;
}

/*
 * acl check function:
 *   unused at the moment but could be used to get more
 *   concrete error messages for debugging...
 *   (acl sort just says that the acl is invalid...)
 */
static BOOL solaris_acl_check(SOLARIS_ACL_T solaris_acl, int count)
{
	int check_rc;
	int check_which;
	
	check_rc = aclcheck(solaris_acl, count, &check_which);
	if (check_rc != 0) {
		DEBUG(10, ("acl is not valid:\n"));
		DEBUGADD(10, (" - return code: %d\n", check_rc));
		DEBUGADD(10, (" - which: %d\n", check_which));
		if (check_which != -1) {
			DEBUGADD(10, (" - invalid entry:\n"));
			DEBUGADD(10, ("   * type: %d:\n", 
				      solaris_acl[check_which].a_type));
			DEBUGADD(10, ("   * id: %d\n",
				      solaris_acl[check_which].a_id));
			DEBUGADD(10, ("   * perm: 0o%o\n",
				      solaris_acl[check_which].a_perm));
		}
		return False;
	}
	return True;
}


/* VFS operations structure */

static vfs_op_tuple solarisacl_op_tuples[] = {
	/* Disk operations */
	{SMB_VFS_OP(solarisacl_sys_acl_get_file),
	 SMB_VFS_OP_SYS_ACL_GET_FILE,
	 SMB_VFS_LAYER_TRANSPARENT},

	{SMB_VFS_OP(solarisacl_sys_acl_get_fd),
	 SMB_VFS_OP_SYS_ACL_GET_FD,
	 SMB_VFS_LAYER_TRANSPARENT},

	{SMB_VFS_OP(solarisacl_sys_acl_set_file),
	 SMB_VFS_OP_SYS_ACL_SET_FILE,
	 SMB_VFS_LAYER_TRANSPARENT},

	{SMB_VFS_OP(solarisacl_sys_acl_set_fd),
	 SMB_VFS_OP_SYS_ACL_SET_FD,
	 SMB_VFS_LAYER_TRANSPARENT},

	{SMB_VFS_OP(solarisacl_sys_acl_delete_def_file),
	 SMB_VFS_OP_SYS_ACL_DELETE_DEF_FILE,
	 SMB_VFS_LAYER_TRANSPARENT},

	{SMB_VFS_OP(NULL),
	 SMB_VFS_OP_NOOP,
	 SMB_VFS_LAYER_NOOP}
};

NTSTATUS vfs_solarisacl_init(void);
NTSTATUS vfs_solarisacl_init(void)
{
	return smb_register_vfs(SMB_VFS_INTERFACE_VERSION, "solarisacl",
				solarisacl_op_tuples);
}

/* ENTE */
