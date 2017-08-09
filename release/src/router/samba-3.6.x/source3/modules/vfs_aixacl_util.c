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

SMB_ACL_T aixacl_to_smbacl(struct acl *file_acl)
{
	struct acl_entry *acl_entry;
	struct ace_id *idp;
	
	struct smb_acl_t *result = SMB_MALLOC_P(struct smb_acl_t);
	struct smb_acl_entry *ace;
	int i;
	
	if (result == NULL) {
		return NULL;
	}
	ZERO_STRUCTP(result);
	
	/* Point to the first acl entry in the acl */
	acl_entry =  file_acl->acl_ext;


	
	DEBUG(10,("acl_entry is %p\n",(void *)acl_entry));
	DEBUG(10,("acl_last(file_acl) id %p\n",(void *)acl_last(file_acl)));

	/* Check if the extended acl bit is on.   *
	 * If it isn't, do not show the           *
	 * contents of the acl since AIX intends *
	 * the extended info to remain unused     */

	if(file_acl->acl_mode & S_IXACL){
		/* while we are not pointing to the very end */
		while(acl_entry < acl_last(file_acl)) {
			/* before we malloc anything, make sure this is  */
			/* a valid acl entry and one that we want to map */
			idp = id_nxt(acl_entry->ace_id);
			if((acl_entry->ace_type == ACC_SPECIFY ||
				(acl_entry->ace_type == ACC_PERMIT)) && (idp != id_last(acl_entry))) {
					acl_entry = acl_nxt(acl_entry);
					continue;
			}

			idp = acl_entry->ace_id;
			DEBUG(10,("idp->id_data is %d\n",idp->id_data[0]));
			
			result = SMB_REALLOC(result, sizeof(struct smb_acl_t) +
				     (sizeof(struct smb_acl_entry) *
				      (result->count+1)));
			if (result == NULL) {
				DEBUG(0, ("SMB_REALLOC failed\n"));
				errno = ENOMEM;
				return NULL;
			}
			

			DEBUG(10,("idp->id_type is %d\n",idp->id_type));
			ace = &result->acl[result->count];
			
			ace->a_type = idp->id_type;
							
			switch(ace->a_type) {
			case ACEID_USER: {
			ace->uid = idp->id_data[0];
			DEBUG(10,("case ACEID_USER ace->uid is %d\n",ace->uid));
			ace->a_type = SMB_ACL_USER;
			break;
			}
		
			case ACEID_GROUP: {
			ace->gid = idp->id_data[0];
			DEBUG(10,("case ACEID_GROUP ace->gid is %d\n",ace->gid));
			ace->a_type = SMB_ACL_GROUP;
			break;
			}
			default:
				break;
			}
			/* The access in the acl entries must be left shifted by *
			 * three bites, because they will ultimately be compared *
			 * to S_IRUSR, S_IWUSR, and S_IXUSR.                  */

			switch(acl_entry->ace_type){
			case ACC_PERMIT:
			case ACC_SPECIFY:
				ace->a_perm = acl_entry->ace_access;
				ace->a_perm <<= 6;
				DEBUG(10,("ace->a_perm is %d\n",ace->a_perm));
				break;
			case ACC_DENY:
				/* Since there is no way to return a DENY acl entry *
				 * change to PERMIT and then shift.                 */
				DEBUG(10,("acl_entry->ace_access is %d\n",acl_entry->ace_access));
				ace->a_perm = ~acl_entry->ace_access & 7;
				DEBUG(10,("ace->a_perm is %d\n",ace->a_perm));
				ace->a_perm <<= 6;
				break;
			default:
				DEBUG(0, ("unknown ace->type\n"));
			 	SAFE_FREE(result);
				return(0);
			}
		
			result->count++;
			ace->a_perm |= (ace->a_perm & S_IRUSR) ? SMB_ACL_READ : 0;
			ace->a_perm |= (ace->a_perm & S_IWUSR) ? SMB_ACL_WRITE : 0;
			ace->a_perm |= (ace->a_perm & S_IXUSR) ? SMB_ACL_EXECUTE : 0;
			DEBUG(10,("ace->a_perm is %d\n",ace->a_perm));
			
			DEBUG(10,("acl_entry = %p\n",(void *)acl_entry));
			DEBUG(10,("The ace_type is %d\n",acl_entry->ace_type));
 
			acl_entry = acl_nxt(acl_entry);
		}
	} /* end of if enabled */

	/* Since owner, group, other acl entries are not *
	 * part of the acl entries in an acl, they must  *
	 * be dummied up to become part of the list.     */

	for( i = 1; i < 4; i++) {
		DEBUG(10,("i is %d\n",i));

			result = SMB_REALLOC(result, sizeof(struct smb_acl_t) +
				     (sizeof(struct smb_acl_entry) *
				      (result->count+1)));
			if (result == NULL) {
				DEBUG(0, ("SMB_REALLOC failed\n"));
				errno = ENOMEM;
				DEBUG(0,("Error in AIX sys_acl_get_file is %d\n",errno));
				return NULL;
			}
			
		ace = &result->acl[result->count];
		
		ace->uid = 0;
		ace->gid = 0;
		DEBUG(10,("ace->uid = %d\n",ace->uid));
		
		switch(i) {
		case 2:
			ace->a_perm = file_acl->g_access << 6;
			ace->a_type = SMB_ACL_GROUP_OBJ;
			break;

		case 3:
			ace->a_perm = file_acl->o_access << 6;
			ace->a_type = SMB_ACL_OTHER;
			break;
 
		case 1:
			ace->a_perm = file_acl->u_access << 6;
			ace->a_type = SMB_ACL_USER_OBJ;
			break;
 
		default:
			return(NULL);

		}
		ace->a_perm |= ((ace->a_perm & S_IRUSR) ? SMB_ACL_READ : 0);
		ace->a_perm |= ((ace->a_perm & S_IWUSR) ? SMB_ACL_WRITE : 0);
		ace->a_perm |= ((ace->a_perm & S_IXUSR) ? SMB_ACL_EXECUTE : 0);
		
		memcpy(&result->acl[result->count],ace,sizeof(struct smb_acl_entry));
		result->count++;
		DEBUG(10,("ace->a_perm = %d\n",ace->a_perm));
		DEBUG(10,("ace->a_type = %d\n",ace->a_type));
	}


	return result;


}

static ushort aixacl_smb_to_aixperm(SMB_ACL_PERM_T a_perm)
{
	ushort ret = (ushort)0;
	if (a_perm & SMB_ACL_READ)
		ret |= R_ACC;
	if (a_perm & SMB_ACL_WRITE)
		ret |= W_ACC;
	if (a_perm & SMB_ACL_EXECUTE)
		ret |= X_ACC;
	return ret;
}

struct acl *aixacl_smb_to_aixacl(SMB_ACL_TYPE_T acltype, SMB_ACL_T theacl)
{
	struct smb_acl_entry *smb_entry = NULL;
	struct acl *file_acl = NULL;
	struct acl *file_acl_temp = NULL;
	struct acl_entry *acl_entry = NULL;
	struct ace_id *ace_id = NULL;
	unsigned int id_type;
	unsigned int acl_length;
	int	i;
 
	DEBUG(10,("Entering aixacl_smb_to_aixacl\n"));
	/* AIX has no default ACL */
	if(acltype == SMB_ACL_TYPE_DEFAULT)
		return NULL;

	acl_length = BUFSIZ;
	file_acl = (struct acl *)SMB_MALLOC(BUFSIZ);
	if(file_acl == NULL) {
		errno = ENOMEM;
		DEBUG(0,("Error in aixacl_smb_to_aixacl is %d\n",errno));
		return NULL;
	}

	memset(file_acl,0,BUFSIZ);
 
	file_acl->acl_len = ACL_SIZ;
	file_acl->acl_mode = S_IXACL;

	for(i=0; i<theacl->count; i++ ) {
		smb_entry = &(theacl->acl[i]);
		id_type = smb_entry->a_type;
		DEBUG(10,("The id_type is %d\n",id_type));

		switch(id_type) {
			case SMB_ACL_USER_OBJ:
				file_acl->u_access = aixacl_smb_to_aixperm(smb_entry->a_perm);
				continue;
			case SMB_ACL_GROUP_OBJ:
				file_acl->g_access = aixacl_smb_to_aixperm(smb_entry->a_perm);
				continue;
			case SMB_ACL_OTHER:
				file_acl->o_access = aixacl_smb_to_aixperm(smb_entry->a_perm);
				continue;
			case SMB_ACL_MASK:
				continue;
			case SMB_ACL_GROUP:
				break; /* process this */
			case SMB_ACL_USER:
				break; /* process this */
			default: /* abnormal case */
				DEBUG(10,("The id_type is unknown !\n"));
				continue;
		}

		if((file_acl->acl_len + sizeof(struct acl_entry)) > acl_length) {
			acl_length += sizeof(struct acl_entry);
			file_acl_temp = (struct acl *)SMB_MALLOC(acl_length);
			if(file_acl_temp == NULL) {
				SAFE_FREE(file_acl);
				errno = ENOMEM;
				DEBUG(0,("Error in aixacl_smb_to_aixacl is %d\n",errno));
				return NULL;
			}

			memcpy(file_acl_temp,file_acl,file_acl->acl_len);
			SAFE_FREE(file_acl);
			file_acl = file_acl_temp;
		}

		acl_entry = (struct acl_entry *)((char *)file_acl + file_acl->acl_len);
		file_acl->acl_len += sizeof(struct acl_entry);
		acl_entry->ace_len = sizeof(struct acl_entry); /* contains 1 ace_id */
		acl_entry->ace_access = aixacl_smb_to_aixperm(smb_entry->a_perm);

		/* In order to use this, we'll need to wait until we can get denies */
		/* if(!acl_entry->ace_access && acl_entry->ace_type == ACC_PERMIT)
		acl_entry->ace_type = ACC_SPECIFY; */

		acl_entry->ace_type = ACC_SPECIFY;

		ace_id = acl_entry->ace_id;

		ace_id->id_type = (smb_entry->a_type==SMB_ACL_GROUP) ? ACEID_GROUP : ACEID_USER;
		DEBUG(10,("The id type is %d\n",ace_id->id_type));
		ace_id->id_len = sizeof(struct ace_id); /* contains 1 id_data */
		ace_id->id_data[0] = (smb_entry->a_type==SMB_ACL_GROUP) ? smb_entry->gid : smb_entry->uid;
	}

	return file_acl;
}
