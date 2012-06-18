/*
 * NFS4 ACL handling
 *
 * Copyright (C) Jim McDonough, 2006
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

#include "includes.h"
#include "nfs4_acls.h"

#define SMBACL4_PARAM_TYPE_NAME "nfs4"

#define SMB_ACE4_INT_MAGIC 0x76F8A967
typedef struct _SMB_ACE4_INT_T
{
	uint32	magic;
	SMB_ACE4PROP_T	prop;
	void	*next;
} SMB_ACE4_INT_T;

#define SMB_ACL4_INT_MAGIC 0x29A3E792
typedef struct _SMB_ACL4_INT_T
{
	uint32	magic;
	uint32	naces;
	SMB_ACE4_INT_T	*first;
	SMB_ACE4_INT_T	*last;
} SMB_ACL4_INT_T;

extern struct current_user current_user;
extern int try_chown(connection_struct *conn, const char *fname, uid_t uid, gid_t gid);
extern BOOL unpack_nt_owners(int snum, uid_t *puser, gid_t *pgrp,
	uint32 security_info_sent, SEC_DESC *psd);

static SMB_ACL4_INT_T *get_validated_aclint(SMB4ACL_T *acl)
{
	SMB_ACL4_INT_T *aclint = (SMB_ACL4_INT_T *)acl;
	if (acl==NULL)
	{
		DEBUG(2, ("acl is NULL\n"));
		errno = EINVAL;
		return NULL;
	}
	if (aclint->magic!=SMB_ACL4_INT_MAGIC)
	{
		DEBUG(2, ("aclint bad magic 0x%x\n", aclint->magic));
		errno = EINVAL;
		return NULL;
	}
	return aclint;
}

static SMB_ACE4_INT_T *get_validated_aceint(SMB4ACE_T *ace)
{
	SMB_ACE4_INT_T *aceint = (SMB_ACE4_INT_T *)ace;
	if (ace==NULL)
	{
		DEBUG(2, ("ace is NULL\n"));
		errno = EINVAL;
		return NULL;
	}
	if (aceint->magic!=SMB_ACE4_INT_MAGIC)
	{
		DEBUG(2, ("aceint bad magic 0x%x\n", aceint->magic));
		errno = EINVAL;
		return NULL;
	}
	return aceint;
}

SMB4ACL_T *smb_create_smb4acl(void)
{
	TALLOC_CTX *mem_ctx = main_loop_talloc_get();
	SMB_ACL4_INT_T	*acl = (SMB_ACL4_INT_T *)TALLOC_SIZE(mem_ctx, sizeof(SMB_ACL4_INT_T));
	if (acl==NULL)
	{
		DEBUG(0, ("TALLOC_SIZE failed\n"));
		errno = ENOMEM;
		return NULL;
	}
	memset(acl, 0, sizeof(SMB_ACL4_INT_T));
	acl->magic = SMB_ACL4_INT_MAGIC;
	/* acl->first, last = NULL not needed */
	return (SMB4ACL_T *)acl;
}

SMB4ACE_T *smb_add_ace4(SMB4ACL_T *acl, SMB_ACE4PROP_T *prop)
{
	SMB_ACL4_INT_T *aclint = get_validated_aclint(acl);
	TALLOC_CTX *mem_ctx = main_loop_talloc_get();
	SMB_ACE4_INT_T *ace;

	ace = (SMB_ACE4_INT_T *)TALLOC_SIZE(mem_ctx, sizeof(SMB_ACE4_INT_T));
	if (ace==NULL)
	{
		DEBUG(0, ("TALLOC_SIZE failed\n"));
		errno = ENOMEM;
		return NULL;
	}
	memset(ace, 0, sizeof(SMB_ACE4_INT_T));
	ace->magic = SMB_ACE4_INT_MAGIC;
	/* ace->next = NULL not needed */
	memcpy(&ace->prop, prop, sizeof(SMB_ACE4PROP_T));

	if (aclint->first==NULL)
	{
		aclint->first = ace;
		aclint->last = ace;
	} else {
		aclint->last->next = (void *)ace;
		aclint->last = ace;
	}
	aclint->naces++;

	return (SMB4ACE_T *)ace;
}

SMB_ACE4PROP_T *smb_get_ace4(SMB4ACE_T *ace)
{
	SMB_ACE4_INT_T *aceint = get_validated_aceint(ace);
	if (aceint==NULL)
		return NULL;

	return &aceint->prop;
}

SMB4ACE_T *smb_next_ace4(SMB4ACE_T *ace)
{
	SMB_ACE4_INT_T *aceint = get_validated_aceint(ace);
	if (aceint==NULL)
		return NULL;

	return (SMB4ACE_T *)aceint->next;
}

SMB4ACE_T *smb_first_ace4(SMB4ACL_T *acl)
{
	SMB_ACL4_INT_T *aclint = get_validated_aclint(acl);
	if (aclint==NULL)
		return NULL;

	return (SMB4ACE_T *)aclint->first;
}

uint32 smb_get_naces(SMB4ACL_T *acl)
{
	SMB_ACL4_INT_T *aclint = get_validated_aclint(acl);
	if (aclint==NULL)
		return 0;

	return aclint->naces;
}

static int smbacl4_GetFileOwner(files_struct *fsp, SMB_STRUCT_STAT *psbuf)
{
	memset(psbuf, 0, sizeof(SMB_STRUCT_STAT));
	if (fsp->is_directory || fsp->fh->fd == -1) {
		/* Get the stat struct for the owner info. */
		if (SMB_VFS_STAT(fsp->conn,fsp->fsp_name, psbuf) != 0)
		{
			DEBUG(8, ("SMB_VFS_STAT failed with error %s\n",
				strerror(errno)));
			return -1;
		}
	} else {
		if (SMB_VFS_FSTAT(fsp,fsp->fh->fd, psbuf) != 0)
		{
			DEBUG(8, ("SMB_VFS_FSTAT failed with error %s\n",
				strerror(errno)));
			return -1;
		}
	}

	return 0;
}

static BOOL smbacl4_nfs42win(SMB4ACL_T *acl, /* in */
	DOM_SID *psid_owner, /* in */
	DOM_SID *psid_group, /* in */
	SEC_ACE **ppnt_ace_list, /* out */
	int *pgood_aces /* out */
)
{
	SMB_ACL4_INT_T *aclint = (SMB_ACL4_INT_T *)acl;
	SMB_ACE4_INT_T *aceint;
	SEC_ACE *nt_ace_list = NULL;
	int good_aces = 0;
	TALLOC_CTX *mem_ctx = main_loop_talloc_get();

	DEBUG(10, ("smbacl_nfs42win entered"));

	aclint = get_validated_aclint(acl);
	if (aclint==NULL)
		return False;

	if (aclint->naces) {
		nt_ace_list = (SEC_ACE *)TALLOC_SIZE(mem_ctx, aclint->naces * sizeof(SEC_ACE));
		if (nt_ace_list==NULL)
		{
			DEBUG(10, ("talloc error"));
			errno = ENOMEM;
			return False;
		}
		memset(nt_ace_list, 0, aclint->naces * sizeof(SEC_ACE));
	} else {
		nt_ace_list = NULL;
	}

	for (aceint=aclint->first; aceint!=NULL; aceint=(SMB_ACE4_INT_T *)aceint->next) {
		SEC_ACCESS mask;
		DOM_SID sid;
		SMB_ACE4PROP_T	*ace = &aceint->prop;

		DEBUG(10, ("magic: 0x%x, type: %d, iflags: %x, flags: %x, mask: %x, "
			"who: %d\n", aceint->magic, ace->aceType, ace->flags,
			ace->aceFlags, ace->aceMask, ace->who.id));

		SMB_ASSERT(aceint->magic==SMB_ACE4_INT_MAGIC);

		if (ace->flags & SMB_ACE4_ID_SPECIAL) {
			switch (ace->who.special_id) {
			case SMB_ACE4_WHO_OWNER:
				sid_copy(&sid, psid_owner);
				break;
			case SMB_ACE4_WHO_GROUP:
				sid_copy(&sid, psid_group);
				break;
			case SMB_ACE4_WHO_EVERYONE:
				sid_copy(&sid, &global_sid_World);
				break;
			default:
				DEBUG(8, ("invalid special who id %d "
					"ignored\n", ace->who.special_id));
			}
		} else {
			if (ace->aceFlags & SMB_ACE4_IDENTIFIER_GROUP) {
				gid_to_sid(&sid, ace->who.gid);
			} else {
				uid_to_sid(&sid, ace->who.uid);
			}
		}
		DEBUG(10, ("mapped %d to %s\n", ace->who.id,
			sid_string_static(&sid)));

		init_sec_access(&mask, ace->aceMask);
		init_sec_ace(&nt_ace_list[good_aces++], &sid,
			ace->aceType, mask,
			ace->aceFlags & 0xf);
	}

	*ppnt_ace_list = nt_ace_list;
	*pgood_aces = good_aces;

	return True;
}

size_t smb_get_nt_acl_nfs4(files_struct *fsp,
	uint32 security_info,
	SEC_DESC **ppdesc, SMB4ACL_T *acl)
{
	int	good_aces = 0;
	SMB_STRUCT_STAT sbuf;
	DOM_SID sid_owner, sid_group;
	size_t sd_size = 0;
	SEC_ACE *nt_ace_list = NULL;
	SEC_ACL *psa = NULL;
	TALLOC_CTX *mem_ctx = main_loop_talloc_get();

	DEBUG(10, ("smb_get_nt_acl_nfs4 invoked for %s\n", fsp->fsp_name));

	if (acl==NULL || smb_get_naces(acl)==0)
		return 0; /* special because we shouldn't alloc 0 for win */

	if (smbacl4_GetFileOwner(fsp, &sbuf))
		return 0;

	uid_to_sid(&sid_owner, sbuf.st_uid);
	gid_to_sid(&sid_group, sbuf.st_gid);

	if (smbacl4_nfs42win(acl,
		&sid_owner,
		&sid_group,
		&nt_ace_list,
		&good_aces
		)==False) {
		DEBUG(8,("smbacl4_nfs42win failed\n"));
		return 0;
	}

	psa = make_sec_acl(mem_ctx, NT4_ACL_REVISION,
		good_aces, nt_ace_list);
	if (psa == NULL) {
		DEBUG(2,("make_sec_acl failed\n"));
		return 0;
	}

	DEBUG(10,("after make sec_acl\n"));
	*ppdesc = make_sec_desc(mem_ctx, SEC_DESC_REVISION,
		SEC_DESC_SELF_RELATIVE,
		(security_info & OWNER_SECURITY_INFORMATION)
		? &sid_owner : NULL,
		(security_info & GROUP_SECURITY_INFORMATION)
		? &sid_group : NULL,
		NULL, psa, &sd_size);
	if (*ppdesc==NULL) {
		DEBUG(2,("make_sec_desc failed\n"));
		return 0;
	}

	DEBUG(10, ("smb_get_nt_acl_nfs4 successfully exited with sd_size %d\n", sd_size));
	return sd_size;
}

enum smbacl4_mode_enum {e_simple=0, e_special=1};
enum smbacl4_acedup_enum {e_dontcare=0, e_reject=1, e_ignore=2, e_merge=3};

typedef struct _smbacl4_vfs_params {
	enum smbacl4_mode_enum mode;
	BOOL do_chown;
	enum smbacl4_acedup_enum acedup;
} smbacl4_vfs_params;

/*
 * Gather special parameters for NFS4 ACL handling
 */
static int smbacl4_get_vfs_params(
	const char *type_name,
	files_struct *fsp,
	smbacl4_vfs_params *params
)
{
	static const struct enum_list enum_smbacl4_modes[] = {
		{ e_simple, "simple" },
		{ e_special, "special" }
	};
	static const struct enum_list enum_smbacl4_acedups[] = {
		{ e_dontcare, "dontcare" },
		{ e_reject, "reject" },
		{ e_ignore, "ignore" },
		{ e_merge, "merge" },
	};

	memset(params, 0, sizeof(smbacl4_vfs_params));
	params->mode = (enum smbacl4_mode_enum)lp_parm_enum(
		SNUM(fsp->conn), type_name,
		"mode", enum_smbacl4_modes, e_simple);
	params->do_chown = lp_parm_bool(SNUM(fsp->conn), type_name,
		"chown", True);
	params->acedup = (enum smbacl4_acedup_enum)lp_parm_enum(
		SNUM(fsp->conn), type_name,
		"acedup", enum_smbacl4_acedups, e_dontcare);

	DEBUG(10, ("mode:%s, do_chown:%s, acedup: %s\n",
		enum_smbacl4_modes[params->mode].name,
		params->do_chown ? "true" : "false",
		enum_smbacl4_acedups[params->acedup].name));

	return 0;
}

static void smbacl4_dump_nfs4acl(int level, SMB4ACL_T *acl)
{
	SMB_ACL4_INT_T *aclint = get_validated_aclint(acl);
	SMB_ACE4_INT_T *aceint;

	DEBUG(level, ("NFS4ACL: size=%d\n", aclint->naces));

	for(aceint = aclint->first; aceint!=NULL; aceint=(SMB_ACE4_INT_T *)aceint->next) {
		SMB_ACE4PROP_T *ace = &aceint->prop;

		DEBUG(level, ("\tACE: type=%d, flags=0x%x, fflags=0x%x, mask=0x%x, id=%d\n",
			ace->aceType,
			ace->aceFlags, ace->flags,
			ace->aceMask,
			ace->who.id));
	}
}

/* 
 * Find 2 NFS4 who-special ACE property (non-copy!!!)
 * match nonzero if "special" and who is equal
 * return ace if found matching; otherwise NULL
 */
static SMB_ACE4PROP_T *smbacl4_find_equal_special(
	SMB4ACL_T *acl,
	SMB_ACE4PROP_T *aceNew)
{
	SMB_ACL4_INT_T *aclint = get_validated_aclint(acl);
	SMB_ACE4_INT_T *aceint;

	for(aceint = aclint->first; aceint!=NULL; aceint=(SMB_ACE4_INT_T *)aceint->next) {
		SMB_ACE4PROP_T *ace = &aceint->prop;

		if (ace->flags == aceNew->flags &&
			ace->aceType==aceNew->aceType &&
			(ace->aceFlags&SMB_ACE4_IDENTIFIER_GROUP)==
			(aceNew->aceFlags&SMB_ACE4_IDENTIFIER_GROUP)
		) {
			/* keep type safety; e.g. gid is an u.short */
			if (ace->flags & SMB_ACE4_ID_SPECIAL)
			{
				if (ace->who.special_id==aceNew->who.special_id)
					return ace;
			} else {
				if (ace->aceFlags & SMB_ACE4_IDENTIFIER_GROUP)
				{
					if (ace->who.gid==aceNew->who.gid)
						return ace;
				} else {
					if (ace->who.uid==aceNew->who.uid)
						return ace;
				}
			}
		}
	}

	return NULL;
}

static int smbacl4_fill_ace4(
	TALLOC_CTX *mem_ctx,
	smbacl4_vfs_params *params,
	uid_t ownerUID,
	gid_t ownerGID,
	SEC_ACE *ace_nt, /* input */
	SMB_ACE4PROP_T *ace_v4 /* output */
)
{
	const char *dom, *name;
	enum lsa_SidType type;
	uid_t uid;
	gid_t gid;

	DEBUG(10, ("got ace for %s\n",
		sid_string_static(&ace_nt->trustee)));

	memset(ace_v4, 0, sizeof(SMB_ACE4PROP_T));
	ace_v4->aceType = ace_nt->type; /* only ACCES|DENY supported right now */
	ace_v4->aceFlags = ace_nt->flags & SEC_ACE_FLAG_VALID_INHERIT;
	ace_v4->aceMask = ace_nt->access_mask &
		(STD_RIGHT_ALL_ACCESS | SA_RIGHT_FILE_ALL_ACCESS);

	if (ace_v4->aceFlags!=ace_nt->flags)
		DEBUG(9, ("ace_v4->aceFlags(0x%x)!=ace_nt->flags(0x%x)\n",
			ace_v4->aceFlags, ace_nt->flags));

	if (ace_v4->aceMask!=ace_nt->access_mask)
		DEBUG(9, ("ace_v4->aceMask(0x%x)!=ace_nt->access_mask(0x%x)\n",
			ace_v4->aceMask, ace_nt->access_mask));

	if (sid_equal(&ace_nt->trustee, &global_sid_World)) {
		ace_v4->who.special_id = SMB_ACE4_WHO_EVERYONE;
		ace_v4->flags |= SMB_ACE4_ID_SPECIAL;
	} else {
		if (!lookup_sid(mem_ctx, &ace_nt->trustee, &dom, &name, &type)) {
			DEBUG(8, ("Could not find %s' type\n",
				sid_string_static(&ace_nt->trustee)));
			errno = EINVAL;
			return -1;
		}

		if (type == SID_NAME_USER) {
			if (!sid_to_uid(&ace_nt->trustee, &uid)) {
				DEBUG(2, ("Could not convert %s to uid\n",
					sid_string_static(&ace_nt->trustee)));
				return -1;
			}

			if (params->mode==e_special && uid==ownerUID) {
				ace_v4->flags |= SMB_ACE4_ID_SPECIAL;
				ace_v4->who.special_id = SMB_ACE4_WHO_OWNER;
			} else {
				ace_v4->who.uid = uid;
			}
		} else { /* else group? - TODO check it... */
			if (!sid_to_gid(&ace_nt->trustee, &gid)) {
				DEBUG(2, ("Could not convert %s to gid\n",
					sid_string_static(&ace_nt->trustee)));
				return -1;
			}
			ace_v4->aceFlags |= SMB_ACE4_IDENTIFIER_GROUP;

			if (params->mode==e_special && gid==ownerGID) {
				ace_v4->flags |= SMB_ACE4_ID_SPECIAL;
				ace_v4->who.special_id = SMB_ACE4_WHO_GROUP;
			} else {
				ace_v4->who.gid = gid;
			}
		}
	}

	return 0; /* OK */
}

static int smbacl4_MergeIgnoreReject(
	enum smbacl4_acedup_enum acedup,
	SMB4ACL_T *acl, /* may modify it */
	SMB_ACE4PROP_T *ace, /* the "new" ACE */
	BOOL	*paddNewACE,
	int	i
)
{
	int	result = 0;
	SMB_ACE4PROP_T *ace4found = smbacl4_find_equal_special(acl, ace);
	if (ace4found)
	{
		switch(acedup)
		{
		case e_merge: /* "merge" flags */
			*paddNewACE = False;
			ace4found->aceFlags |= ace->aceFlags;
			ace4found->aceMask |= ace->aceMask;
			break;
		case e_ignore: /* leave out this record */
			*paddNewACE = False;
			break;
		case e_reject: /* do an error */
			DEBUG(8, ("ACL rejected by duplicate nt ace#%d\n", i));
			errno = EINVAL; /* SHOULD be set on any _real_ error */
			result = -1;
			break;
		default:
			break;
		}
	}
	return result;
}

static SMB4ACL_T *smbacl4_win2nfs4(
	SEC_ACL *dacl,
	smbacl4_vfs_params *pparams,
	uid_t ownerUID,
	gid_t ownerGID
)
{
	SMB4ACL_T *acl;
	uint32	i;
	TALLOC_CTX *mem_ctx = main_loop_talloc_get();

	DEBUG(10, ("smbacl4_win2nfs4 invoked\n"));

	acl = smb_create_smb4acl();
	if (acl==NULL)
		return NULL;

	for(i=0; i<dacl->num_aces; i++) {
		SMB_ACE4PROP_T	ace_v4;
		BOOL	addNewACE = True;

		if (smbacl4_fill_ace4(mem_ctx, pparams, ownerUID, ownerGID,
			dacl->aces + i, &ace_v4))
			return NULL;

		if (pparams->acedup!=e_dontcare) {
			if (smbacl4_MergeIgnoreReject(pparams->acedup, acl,
				&ace_v4, &addNewACE, i))
				return NULL;
		}

		if (addNewACE)
			smb_add_ace4(acl, &ace_v4);
	}

	return acl;
}

BOOL smb_set_nt_acl_nfs4(files_struct *fsp,
	uint32 security_info_sent,
	SEC_DESC *psd,
	set_nfs4acl_native_fn_t set_nfs4_native)
{
	smbacl4_vfs_params params;
	SMB4ACL_T *acl = NULL;
	BOOL	result;

	SMB_STRUCT_STAT sbuf;
	BOOL set_acl_as_root = False;
	uid_t newUID = (uid_t)-1;
	gid_t newGID = (gid_t)-1;
	int saved_errno;

	DEBUG(10, ("smb_set_nt_acl_nfs4 invoked for %s\n", fsp->fsp_name));

	if ((security_info_sent & (DACL_SECURITY_INFORMATION |
		GROUP_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION)) == 0)
	{
		DEBUG(9, ("security_info_sent (0x%x) ignored\n",
			security_info_sent));
		return True; /* won't show error - later to be refined... */
	}

	/* Special behaviours */
	if (smbacl4_get_vfs_params(SMBACL4_PARAM_TYPE_NAME, fsp, &params))
		return False;

	if (smbacl4_GetFileOwner(fsp, &sbuf))
		return False;

	if (params.do_chown) {
		/* chown logic is a copy/paste from posix_acl.c:set_nt_acl */
		if (!unpack_nt_owners(SNUM(fsp->conn), &newUID, &newGID, security_info_sent, psd))
		{
			DEBUG(8, ("unpack_nt_owners failed"));
			return False;
		}
		if (((newUID != (uid_t)-1) && (sbuf.st_uid != newUID)) ||
			((newGID != (gid_t)-1) && (sbuf.st_gid != newGID))) {
			if(try_chown(fsp->conn, fsp->fsp_name, newUID, newGID)) {
				DEBUG(3,("chown %s, %u, %u failed. Error = %s.\n",
					fsp->fsp_name, (unsigned int)newUID, (unsigned int)newGID, strerror(errno) ));
				return False;
			}
			DEBUG(10,("chown %s, %u, %u succeeded.\n",
				fsp->fsp_name, (unsigned int)newUID, (unsigned int)newGID));
			if (smbacl4_GetFileOwner(fsp, &sbuf))
				return False;
			/* If we successfully chowned, we know we must
			 * be able to set the acl, so do it as root.
			 */
			set_acl_as_root = True;
		}
	}

	if (!(security_info_sent & DACL_SECURITY_INFORMATION) || psd->dacl ==NULL) {
		DEBUG(10, ("no dacl found; security_info_sent = 0x%x\n", security_info_sent));
		return True;
	}
	acl = smbacl4_win2nfs4(psd->dacl, &params, sbuf.st_uid, sbuf.st_gid);
	if (!acl)
		return False;

	smbacl4_dump_nfs4acl(10, acl);

	if (set_acl_as_root) {
		become_root();
	}
	result = set_nfs4_native(fsp, acl);
	saved_errno = errno;
	if (set_acl_as_root) {
		unbecome_root();
	}
	if (result!=True) {
		errno = saved_errno;
		DEBUG(10, ("set_nfs4_native failed with %s\n", strerror(errno)));
		return False;
	}

	DEBUG(10, ("smb_set_nt_acl_nfs4 succeeded\n"));
	return True;
}
