/*
 * Unix SMB/CIFS implementation.
 *
 * Support for OneFS native NTFS ACLs
 *
 * Copyright (C) Steven Danneman, 2008
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"
#include "smbd/smbd.h"
#include "onefs.h"
#include "onefs_config.h"

#include <isi_acl/isi_acl_util.h>
#include <ifs/ifs_syscalls.h>
#include <sys/isi_acl.h>

const struct enum_list enum_onefs_acl_wire_format[] = {
	{ACL_FORMAT_RAW,  "No Format"},
	{ACL_FORMAT_WINDOWS_SD, "Format Windows SD"},
	{ACL_FORMAT_ALWAYS, "Always Format SD"},
	{-1, NULL}
};

/**
 * Turn SID into UID/GID and setup a struct ifs_identity
 */
static bool
onefs_sid_to_identity(const struct dom_sid *sid, struct ifs_identity *id,
    bool is_group)
{
	enum ifs_identity_type type = IFS_ID_TYPE_LAST+1;
	uid_t uid = 0;
	gid_t gid = 0;

	if (!sid || sid_equal(sid, &global_sid_NULL))
		type = IFS_ID_TYPE_NULL;
	else if (sid_equal(sid, &global_sid_World))
		type = IFS_ID_TYPE_EVERYONE;
	else if (sid_equal(sid, &global_sid_Creator_Owner))
		type = IFS_ID_TYPE_CREATOR_OWNER;
	else if (sid_equal(sid, &global_sid_Creator_Group))
		type = IFS_ID_TYPE_CREATOR_GROUP;
	else if (is_group) {
		if (!sid_to_gid(sid, &gid))
			return false;
		type = IFS_ID_TYPE_GID;
	} else {
		if (sid_to_uid(sid, &uid))
			type = IFS_ID_TYPE_UID;
		else if (sid_to_gid(sid, &gid))
			type = IFS_ID_TYPE_GID;
		else
			return false;
	}

	if (aclu_initialize_identity(id, type, uid, gid, is_group)) {
		DEBUG(3, ("Call to aclu_initialize_identity failed! id=%x, "
		    "type=%d, uid=%u, gid=%u, is_group=%d\n",
		    (unsigned int)id, type, uid, gid, is_group));
		return false;
	}

	return true;
}

/**
 * Turn struct ifs_identity into SID
 */
static bool
onefs_identity_to_sid(struct ifs_identity *id, struct dom_sid *sid)
{
	if (!id || !sid)
		return false;

	if (id->type >= IFS_ID_TYPE_LAST)
		return false;

	switch (id->type) {
	    case IFS_ID_TYPE_UID:
	        uid_to_sid(sid, id->id.uid);
		break;
	    case IFS_ID_TYPE_GID:
		gid_to_sid(sid, id->id.gid);
		break;
	    case IFS_ID_TYPE_EVERYONE:
		sid_copy(sid, &global_sid_World);
		break;
	    case IFS_ID_TYPE_NULL:
		sid_copy(sid, &global_sid_NULL);
		break;
	    case IFS_ID_TYPE_CREATOR_OWNER:
		sid_copy(sid, &global_sid_Creator_Owner);
		break;
	    case IFS_ID_TYPE_CREATOR_GROUP:
		sid_copy(sid, &global_sid_Creator_Group);
		break;
	    default:
		DEBUG(0, ("Unknown identity type: %d\n", id->type));
		return false;
	}

	return true;
}

static bool
onefs_og_to_identity(struct dom_sid *sid, struct ifs_identity * ident,
    bool is_group, int snum)
{
	const struct dom_sid *b_admin_sid = &global_sid_Builtin_Administrators;

	if (!onefs_sid_to_identity(sid, ident, is_group)) {
		if (!lp_parm_bool(snum, PARM_ONEFS_TYPE,
		     PARM_UNMAPPABLE_SIDS_IGNORE,
		     PARM_UNMAPPABLE_SIDS_IGNORE_DEFAULT)) {
			DEBUG(3, ("Unresolvable SID (%s) found.\n",
				sid_string_dbg(sid)));
			return false;
		}
		if (!onefs_sid_to_identity(b_admin_sid, ident, is_group)) {
			return false;
		}
		DEBUG(3, ("Mapping unresolvable owner SID (%s) to Builtin "
			"Administrators group.\n",
			sid_string_dbg(sid)));
	}
	return true;
}

static bool
sid_in_ignore_list(struct dom_sid * sid, int snum)
{
	const char ** sid_list = NULL;
	struct dom_sid match;

	sid_list = lp_parm_string_list(snum, PARM_ONEFS_TYPE,
	    PARM_UNMAPPABLE_SIDS_IGNORE_LIST,
	    PARM_UNMAPPABLE_SIDS_IGNORE_LIST_DEFAULT);

	/* Fast path a NULL list */
	if (!sid_list || *sid_list == NULL)
		return false;

	while (*sid_list) {
		if (string_to_sid(&match, *sid_list))
			if (sid_equal(sid, &match))
				return true;
		sid_list++;
	}

	return false;
}

/**
 * Convert a trustee to a struct identity
 */
static bool
onefs_samba_ace_to_ace(struct security_ace * samba_ace, struct ifs_ace * ace,
    bool *mapped, int snum)
{
	struct ifs_identity ident = {.type=IFS_ID_TYPE_LAST, .id.uid=0};

	SMB_ASSERT(ace);
	SMB_ASSERT(mapped);
	SMB_ASSERT(samba_ace);

	if (onefs_sid_to_identity(&samba_ace->trustee, &ident, false)) {
		*mapped = true;
	} else {

		SMB_ASSERT(ident.id.uid >= 0);

		/* Ignore the sid if it's in the list */
		if (sid_in_ignore_list(&samba_ace->trustee, snum)) {
			DEBUG(3, ("Silently failing to set ACE for SID (%s) "
				"because it is in the ignore sids list\n",
				sid_string_dbg(&samba_ace->trustee)));
			*mapped = false;
		} else if ((samba_ace->type == SEC_ACE_TYPE_ACCESS_DENIED) &&
		    lp_parm_bool(snum, PARM_ONEFS_TYPE,
		    PARM_UNMAPPABLE_SIDS_DENY_EVERYONE,
		    PARM_UNMAPPABLE_SIDS_DENY_EVERYONE_DEFAULT)) {
			/* If the ace is deny translated to Everyone */
			DEBUG(3, ("Mapping unresolvable deny ACE SID (%s) "
				"to Everyone.\n",
				sid_string_dbg(&samba_ace->trustee)));
			if (aclu_initialize_identity(&ident,
				IFS_ID_TYPE_EVERYONE, 0, 0, False) != 0) {
				DEBUG(2, ("aclu_initialize_identity() "
					"failed making Everyone\n"));
				return false;
			}
			*mapped = true;
		} else if (lp_parm_bool(snum, PARM_ONEFS_TYPE,
			   PARM_UNMAPPABLE_SIDS_IGNORE,
			   PARM_UNMAPPABLE_SIDS_IGNORE_DEFAULT)) {
			DEBUG(3, ("Silently failing to set ACE for SID (%s) "
				"because it is unresolvable\n",
				sid_string_dbg(&samba_ace->trustee)));
			*mapped = false;
		} else {
			/* Fail for lack of a better option */
			return false;
		}
	}

	if (*mapped) {
		if (aclu_initialize_ace(ace, samba_ace->type,
			samba_ace->access_mask, samba_ace->flags, 0,
			&ident))
			return false;

		if ((ace->trustee.type == IFS_ID_TYPE_CREATOR_OWNER ||
			ace->trustee.type == IFS_ID_TYPE_CREATOR_GROUP) &&
		    nt4_compatible_acls())
			ace->flags |= SEC_ACE_FLAG_INHERIT_ONLY;
	}

	return true;
}

/**
 * Convert a struct security_acl to a struct ifs_security_acl
 */
static bool
onefs_samba_acl_to_acl(struct security_acl *samba_acl, struct ifs_security_acl **acl,
    bool * ignore_aces, int snum)
{
	int num_aces = 0;
	struct ifs_ace *aces = NULL;
	struct security_ace *samba_aces;
	bool mapped;
	int i, j;

	SMB_ASSERT(ignore_aces);

	if ((!acl) || (!samba_acl))
		return false;

	samba_aces = samba_acl->aces;

	if (samba_acl->num_aces > 0 && samba_aces) {
		/* Setup ACES */
		num_aces = samba_acl->num_aces;
		aces = SMB_MALLOC_ARRAY(struct ifs_ace, num_aces);

		for (i = 0, j = 0; j < num_aces; i++, j++) {
			if (!onefs_samba_ace_to_ace(&samba_aces[j],
				&aces[i], &mapped, snum))
				goto err_free;

			if (!mapped)
				i--;
		}
		num_aces = i;
	}

	/* If aces are given but we cannot apply them due to the reasons
	 * above we do not change the SD.  However, if we are told to
	 * explicitly set an SD with 0 aces we honor this operation */
	*ignore_aces = samba_acl->num_aces > 0 && num_aces < 1;

	if (*ignore_aces == false)
		if (aclu_initialize_acl(acl, aces, num_aces))
			goto err_free;

	/* Currently aclu_initialize_acl should copy the aces over, allowing
	 * us to immediately free */
	free(aces);
	return true;

err_free:
	free(aces);
	return false;
}

/**
 * Convert a struct ifs_security_acl to a struct security_acl
 */
static bool
onefs_acl_to_samba_acl(struct ifs_security_acl *acl, struct security_acl **samba_acl)
{
	struct security_ace *samba_aces = NULL;
	struct security_acl *tmp_samba_acl = NULL;
	int i, num_aces = 0;

	if (!samba_acl)
		return false;

	/* NULL ACL */
	if (!acl) {
		*samba_acl = NULL;
		return true;
	}

	/* Determine number of aces in ACL */
	if (!acl->aces)
		num_aces = 0;
	else
		num_aces = acl->num_aces;

	/* Allocate the ace list. */
	if (num_aces > 0) {
		if ((samba_aces = SMB_MALLOC_ARRAY(struct security_ace, num_aces)) == NULL)
		{
			DEBUG(0, ("Unable to malloc space for %d aces.\n",
			    num_aces));
			return false;
		}
		memset(samba_aces, '\0', (num_aces) * sizeof(struct security_ace));
	}

	for (i = 0; i < num_aces; i++) {
		struct dom_sid sid;

		if (!onefs_identity_to_sid(&acl->aces[i].trustee, &sid))
			goto err_free;

		init_sec_ace(&samba_aces[i], &sid, acl->aces[i].type,
		    acl->aces[i].access_mask, acl->aces[i].flags);
	}

	if ((tmp_samba_acl = make_sec_acl(talloc_tos(), acl->revision, num_aces,
	    samba_aces)) == NULL) {
	       DEBUG(0, ("Unable to malloc space for acl.\n"));
	       goto err_free;
	}

	*samba_acl = tmp_samba_acl;
	SAFE_FREE(samba_aces);
	return true;
err_free:
	SAFE_FREE(samba_aces);
	return false;
}

/**
 * @brief Reorder ACLs into the "correct" order for Windows Explorer.
 *
 * Windows Explorer expects ACLs to be in a standard order (inherited first,
 * then deny, then permit.)  When ACLs are composed from POSIX file permissions
 * bits, they may not match these expectations, generating an annoying warning
 * dialog for the user.  This function will, if configured appropriately,
 * reorder the ACLs for these "synthetic" (POSIX-derived) descriptors to prevent
 * this.  The list is changed within the security descriptor passed in.
 *
 * @param fsp files_struct with service configs; must not be NULL
 * @param sd security descriptor being normalized;
 *           sd->dacl->aces is rewritten in-place, so must not be NULL
 * @return true on success, errno will be set on error
 *
 * @bug Although Windows Explorer likes the reordering, they seem to cause
 *  problems with Excel and Word sending back the reordered ACLs to us and
 *  changing policy; see Isilon bug 30165.
 */
static bool
onefs_canon_acl(files_struct *fsp, struct ifs_security_descriptor *sd)
{
	int error = 0;
	int cur;
	struct ifs_ace *new_aces = NULL;
	int new_aces_count = 0;
	SMB_STRUCT_STAT sbuf;

	if (sd == NULL || sd->dacl == NULL || sd->dacl->num_aces == 0)
		return true;

	/*
	 * Find out if this is a windows bit, and if the smb policy wants us to
	 * lie about the sd.
	 */
	SMB_ASSERT(fsp != NULL);
	switch (lp_parm_enum(SNUM(fsp->conn), PARM_ONEFS_TYPE,
		PARM_ACL_WIRE_FORMAT, enum_onefs_acl_wire_format,
		PARM_ACL_WIRE_FORMAT_DEFAULT))  {
	case ACL_FORMAT_RAW:
		return true;

	case ACL_FORMAT_WINDOWS_SD:
		error = SMB_VFS_FSTAT(fsp, &sbuf);
		if (error)
			return false;

		if ((sbuf.st_ex_flags & SF_HASNTFSACL) != 0) {
			DEBUG(10, ("Did not canonicalize ACLs because a "
				   "Windows ACL set was found for file %s\n",
				   fsp_str_dbg(fsp)));
			return true;
		}
		break;

	case ACL_FORMAT_ALWAYS:
		break;

	default:
		SMB_ASSERT(false);
		return false;
	}

	new_aces = SMB_MALLOC_ARRAY(struct ifs_ace, sd->dacl->num_aces);
	if (new_aces == NULL)
		return false;

	/*
	 * By walking down the list 3 separate times, we can avoid the need
	 * to create multiple temp buffers and extra copies.
	 */

	/* Explict deny aces first */
	for (cur = 0; cur < sd->dacl->num_aces; cur++)  {
		if (!(sd->dacl->aces[cur].flags & IFS_ACE_FLAG_INHERITED_ACE) &&
		    (sd->dacl->aces[cur].type == IFS_ACE_TYPE_ACCESS_DENIED))
			new_aces[new_aces_count++] = sd->dacl->aces[cur];
	}

	/* Explict allow aces second */
	for (cur = 0; cur < sd->dacl->num_aces; cur++)  {
		if (!(sd->dacl->aces[cur].flags & IFS_ACE_FLAG_INHERITED_ACE) &&
		    !(sd->dacl->aces[cur].type == IFS_ACE_TYPE_ACCESS_DENIED))
			new_aces[new_aces_count++] = sd->dacl->aces[cur];
	}

	/* Inherited deny/allow aces third */
	for (cur = 0; cur < sd->dacl->num_aces; cur++)  {
		if ((sd->dacl->aces[cur].flags & IFS_ACE_FLAG_INHERITED_ACE))
			new_aces[new_aces_count++] = sd->dacl->aces[cur];
	}

	SMB_ASSERT(new_aces_count == sd->dacl->num_aces);
	DEBUG(10, ("Performed canonicalization of ACLs for file %s\n",
		   fsp_str_dbg(fsp)));

	/*
	 * At this point you would think we could just do this:
	 *   SAFE_FREE(sd->dacl->aces);
	 *   sd->dacl->aces = new_aces;
	 * However, in some cases the existing aces pointer does not point
	 * to the beginning of an allocated block.  So we have to do a more
	 * expensive memcpy()
	 */
	memcpy(sd->dacl->aces, new_aces,
	    sizeof(struct ifs_ace) * new_aces_count);

	SAFE_FREE(new_aces);
	return true;
}


/**
 * This enum is a helper for onefs_fget_nt_acl() to communicate with
 * onefs_init_ace().
 */
enum mode_ident { USR, GRP, OTH };

/**
 * Initializes an ACE for addition to a synthetic ACL.
 */
static struct ifs_ace onefs_init_ace(struct connection_struct *conn,
				     mode_t mode,
				     bool isdir,
				     enum mode_ident ident)
{
	struct ifs_ace result;
	enum ifs_ace_rights r,w,x;

	r = isdir ? UNIX_DIRECTORY_ACCESS_R : UNIX_ACCESS_R;
	w = isdir ? UNIX_DIRECTORY_ACCESS_W : UNIX_ACCESS_W;
	x = isdir ? UNIX_DIRECTORY_ACCESS_X : UNIX_ACCESS_X;

	result.type = IFS_ACE_TYPE_ACCESS_ALLOWED;
	result.ifs_flags = 0;
	result.flags = isdir ? IFS_ACE_FLAG_CONTAINER_INHERIT :
	    IFS_ACE_FLAG_OBJECT_INHERIT;
	result.flags |= IFS_ACE_FLAG_INHERIT_ONLY;

	switch (ident) {
	case USR:
		result.access_mask =
		    ((mode & S_IRUSR) ? r : 0 ) |
		    ((mode & S_IWUSR) ? w : 0 ) |
		    ((mode & S_IXUSR) ? x : 0 );
		if (lp_parm_bool(SNUM(conn), PARM_ONEFS_TYPE,
		    PARM_CREATOR_OWNER_GETS_FULL_CONTROL,
		    PARM_CREATOR_OWNER_GETS_FULL_CONTROL_DEFAULT))
			result.access_mask |= GENERIC_ALL_ACCESS;
		result.trustee.type = IFS_ID_TYPE_CREATOR_OWNER;
		break;
	case GRP:
		result.access_mask =
		    ((mode & S_IRGRP) ? r : 0 ) |
		    ((mode & S_IWGRP) ? w : 0 ) |
		    ((mode & S_IXGRP) ? x : 0 );
		result.trustee.type = IFS_ID_TYPE_CREATOR_GROUP;
		break;
	case OTH:
		result.access_mask =
		    ((mode & S_IROTH) ? r : 0 ) |
		    ((mode & S_IWOTH) ? w : 0 ) |
		    ((mode & S_IXOTH) ? x : 0 );
		result.trustee.type = IFS_ID_TYPE_EVERYONE;
		break;
	}

	return result;
}

/**
 * This adds inheritable ACEs to the end of the DACL, with the ACEs
 * being derived from the mode bits.  This is useful for clients that have the
 * MoveSecurityAttributes regkey set to 0 or are in Simple File Sharing Mode.
 *
 * On these clients, when copying files from one folder to another inside the
 * same volume/share, the DACL is explicitely cleared.  Without inheritable
 * aces on the target folder the mode bits of the copied file are set to 000.
 *
 * See Isilon Bug 27990
 *
 * Note: This function allocates additional memory onto sd->dacl->aces, that
 * must be freed by the caller.
 */
static bool add_sfs_aces(files_struct *fsp, struct ifs_security_descriptor *sd)
{
	int error;
	SMB_STRUCT_STAT sbuf;

	error = SMB_VFS_FSTAT(fsp, &sbuf);
	if (error) {
		DEBUG(0, ("Failed to stat %s in simple files sharing "
			  "compatibility mode. errno=%d\n",
			  fsp_str_dbg(fsp), errno));
		return false;
	}

	/* Only continue if this is a synthetic ACL and a directory. */
	if (S_ISDIR(sbuf.st_ex_mode) &&
	    (sbuf.st_ex_flags & SF_HASNTFSACL) == 0) {
		struct ifs_ace new_aces[6];
		struct ifs_ace *old_aces;
		int i, num_aces_to_add = 0;
		mode_t file_mode = 0, dir_mode = 0;

		/* Use existing samba logic to derive the mode bits. */
		file_mode = unix_mode(fsp->conn, 0, fsp->fsp_name, NULL);
		dir_mode = unix_mode(fsp->conn, FILE_ATTRIBUTE_DIRECTORY, fsp->fsp_name, NULL);

		/* Initialize ACEs. */
		new_aces[0] = onefs_init_ace(fsp->conn, file_mode, false, USR);
		new_aces[1] = onefs_init_ace(fsp->conn, file_mode, false, GRP);
		new_aces[2] = onefs_init_ace(fsp->conn, file_mode, false, OTH);
		new_aces[3] = onefs_init_ace(fsp->conn, dir_mode, true, USR);
		new_aces[4] = onefs_init_ace(fsp->conn, dir_mode, true, GRP);
		new_aces[5] = onefs_init_ace(fsp->conn, dir_mode, true, OTH);

		for (i = 0; i < 6; i++)
			if (new_aces[i].access_mask != 0)
				num_aces_to_add++;

		/* Expand the ACEs array */
		if (num_aces_to_add != 0) {
			old_aces = sd->dacl->aces;

			sd->dacl->aces = SMB_MALLOC_ARRAY(struct ifs_ace,
			    sd->dacl->num_aces + num_aces_to_add);
			if (!sd->dacl->aces) {
				DEBUG(0, ("Unable to malloc space for "
				    "new_aces: %d.\n",
				     sd->dacl->num_aces + num_aces_to_add));
				return false;
			}
			memcpy(sd->dacl->aces, old_aces,
			    sizeof(struct ifs_ace) * sd->dacl->num_aces);

			/* Add the new ACEs to the DACL. */
			for (i = 0; i < 6; i++) {
				if (new_aces[i].access_mask != 0) {
					sd->dacl->aces[sd->dacl->num_aces] =
					    new_aces[i];
					sd->dacl->num_aces++;
				}
			}
		}
	}
	return true;
}

/**
 * Isilon-specific function for getting an NTFS ACL from an open file.
 *
 * @param[out] ppdesc SecDesc to allocate and fill in
 *
 * @return NTSTATUS based off errno on error
 */
NTSTATUS
onefs_fget_nt_acl(vfs_handle_struct *handle, files_struct *fsp,
		  uint32 security_info, struct security_descriptor **ppdesc)
{
	int error;
	uint32_t sd_size = 0;
	size_t size = 0;
	struct ifs_security_descriptor *sd = NULL;
	struct dom_sid owner_sid, group_sid;
	struct dom_sid *ownerp, *groupp;
	struct security_acl *dacl, *sacl;
	struct security_descriptor *pdesc;
	bool alloced = false;
	bool new_aces_alloced = false;
	bool fopened = false;
	NTSTATUS status = NT_STATUS_OK;

	START_PROFILE(syscall_get_sd);

	*ppdesc = NULL;

	DEBUG(5, ("Getting sd for file %s. security_info=%u\n",
		  fsp_str_dbg(fsp), security_info));

	if (lp_parm_bool(SNUM(fsp->conn), PARM_ONEFS_TYPE,
		PARM_IGNORE_SACLS, PARM_IGNORE_SACLS_DEFAULT)) {
		DEBUG(5, ("Ignoring SACL on %s.\n", fsp_str_dbg(fsp)));
		security_info &= ~SECINFO_SACL;
	}

	if (fsp->fh->fd == -1) {
		if ((fsp->fh->fd = onefs_sys_create_file(handle->conn,
							 -1,
							 fsp->fsp_name->base_name,
							 0,
							 0,
							 0,
							 0,
							 0,
							 0,
							 INTERNAL_OPEN_ONLY,
							 0,
							 NULL,
							 0,
							 NULL)) == -1) {
			DEBUG(0, ("Error opening file %s. errno=%d (%s)\n",
				  fsp_str_dbg(fsp), errno, strerror(errno)));
			status = map_nt_error_from_unix(errno);
			goto out;
		}
		fopened = true;
	}

        /* Get security descriptor */
        sd_size = 0;
        do {
                /* Allocate memory for get_security_descriptor */
		if (sd_size > 0) {
	                sd = SMB_REALLOC(sd, sd_size);
			if (!sd) {
				DEBUG(0, ("Unable to malloc %u bytes of space "
				    "for security descriptor.\n", sd_size));
				status = map_nt_error_from_unix(errno);
				goto out;
			}

			alloced = true;
		}

                error = ifs_get_security_descriptor(fsp->fh->fd, security_info,
		    sd_size, &sd_size, sd);
                if (error && (errno != EMSGSIZE)) {
			DEBUG(0, ("Failed getting size of security descriptor! "
			    "errno=%d\n", errno));
			status = map_nt_error_from_unix(errno);
			goto out;
                }
        } while (error);

	DEBUG(5, ("Got sd, size=%u:\n", sd_size));

	if (lp_parm_bool(SNUM(fsp->conn),
	    PARM_ONEFS_TYPE,
	    PARM_SIMPLE_FILE_SHARING_COMPATIBILITY_MODE,
	    PARM_SIMPLE_FILE_SHARING_COMPATIBILITY_MODE_DEFAULT) &&
	    sd->dacl) {
		if(!(new_aces_alloced = add_sfs_aces(fsp, sd)))
			goto out;
	}

	if (!(onefs_canon_acl(fsp, sd))) {
		status = map_nt_error_from_unix(errno);
		goto out;
	}

	DEBUG(5, ("Finished canonicalizing ACL\n"));

	ownerp = NULL;
	groupp = NULL;
	dacl = NULL;
	sacl = NULL;

	/* Copy owner into ppdesc */
	if (security_info & SECINFO_OWNER) {
		if (!onefs_identity_to_sid(sd->owner, &owner_sid)) {
			status = NT_STATUS_INVALID_PARAMETER;
			goto out;
		}

		ownerp = &owner_sid;
	}

	/* Copy group into ppdesc */
	if (security_info & SECINFO_GROUP) {
		if (!onefs_identity_to_sid(sd->group, &group_sid)) {
			status = NT_STATUS_INVALID_PARAMETER;
			goto out;
		}

		groupp = &group_sid;
	}

	/* Copy DACL into ppdesc */
	if (security_info & SECINFO_DACL) {
		if (!onefs_acl_to_samba_acl(sd->dacl, &dacl)) {
			status = NT_STATUS_INVALID_PARAMETER;
			goto out;
		}
	}

	/* Copy SACL into ppdesc */
	if (security_info & SECINFO_SACL) {
		if (!onefs_acl_to_samba_acl(sd->sacl, &sacl)) {
			status = NT_STATUS_INVALID_PARAMETER;
			goto out;
		}
	}

	/* AUTO_INHERIT_REQ bits are not returned over the wire so strip them
	 * off.  Eventually we should stop storing these in the kernel
	 * all together. See Isilon bug 40364 */
	sd->control &= ~(IFS_SD_CTRL_DACL_AUTO_INHERIT_REQ |
			 IFS_SD_CTRL_SACL_AUTO_INHERIT_REQ);

	pdesc = make_sec_desc(talloc_tos(), sd->revision, sd->control,
	    ownerp, groupp, sacl, dacl, &size);

	if (!pdesc) {
		DEBUG(0, ("Problem with make_sec_desc. Memory?\n"));
		status = map_nt_error_from_unix(errno);
		goto out;
	}

	*ppdesc = pdesc;

	DEBUG(5, ("Finished retrieving/canonicalizing SD!\n"));
	/* FALLTHROUGH */
out:

	END_PROFILE(syscall_get_sd);

	if (alloced && sd) {
		if (new_aces_alloced && sd->dacl->aces)
			SAFE_FREE(sd->dacl->aces);

		SAFE_FREE(sd);
	}

	if (fopened) {
		close(fsp->fh->fd);
		fsp->fh->fd = -1;
	}

	return status;
}

/**
 * Isilon-specific function for getting an NTFS ACL from a file path.
 *
 * Since onefs_fget_nt_acl() needs to open a filepath if the fd is invalid,
 * we just mock up a files_struct with the path and bad fd and call into it.
 *
 * @param[out] ppdesc SecDesc to allocate and fill in
 *
 * @return NTSTATUS based off errno on error
 */
NTSTATUS
onefs_get_nt_acl(vfs_handle_struct *handle, const char* name,
		 uint32 security_info, struct security_descriptor **ppdesc)
{
	files_struct finfo;
	struct fd_handle fh;
	NTSTATUS status;

	ZERO_STRUCT(finfo);
	ZERO_STRUCT(fh);

	finfo.fnum = -1;
	finfo.conn = handle->conn;
	finfo.fh = &fh;
	finfo.fh->fd = -1;
	status = create_synthetic_smb_fname(talloc_tos(), name, NULL, NULL,
					    &finfo.fsp_name);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = onefs_fget_nt_acl(handle, &finfo, security_info, ppdesc);

	TALLOC_FREE(finfo.fsp_name);
	return status;
}

/**
 * Isilon-specific function for setting up an ifs_security_descriptor, given a
 * samba struct security_descriptor
 *
 * @param[out] sd ifs_security_descriptor to fill in
 *
 * @return NTSTATUS_OK if successful
 */
NTSTATUS onefs_samba_sd_to_sd(uint32_t security_info_sent,
			      const struct security_descriptor *psd,
			      struct ifs_security_descriptor *sd, int snum,
			      uint32_t *security_info_effective)
{
	struct ifs_security_acl *daclp, *saclp;
	struct ifs_identity owner, group, *ownerp, *groupp;
	bool ignore_aces;

	ownerp = NULL;
	groupp = NULL;
	daclp = NULL;
	saclp = NULL;

	*security_info_effective = security_info_sent;

	/* Setup owner */
	if (security_info_sent & SECINFO_OWNER) {
		if (!onefs_og_to_identity(psd->owner_sid, &owner, false, snum))
			return NT_STATUS_ACCESS_DENIED;

		SMB_ASSERT(owner.id.uid >= 0);

		ownerp = &owner;
	}

	/* Setup group */
	if (security_info_sent & SECINFO_GROUP) {
		if (!onefs_og_to_identity(psd->group_sid, &group, true, snum))
			return NT_STATUS_ACCESS_DENIED;

		SMB_ASSERT(group.id.gid >= 0);

		groupp = &group;
	}

	/* Setup DACL */
	if ((security_info_sent & SECINFO_DACL) && (psd->dacl)) {
		if (!onefs_samba_acl_to_acl(psd->dacl, &daclp, &ignore_aces,
			snum))
			return NT_STATUS_ACCESS_DENIED;

		if (ignore_aces == true)
			*security_info_effective &= ~SECINFO_DACL;
	}

	/* Setup SACL */
	if (security_info_sent & SECINFO_SACL) {

		if (lp_parm_bool(snum, PARM_ONEFS_TYPE,
			    PARM_IGNORE_SACLS, PARM_IGNORE_SACLS_DEFAULT)) {
			DEBUG(5, ("Ignoring SACL.\n"));
			*security_info_effective &= ~SECINFO_SACL;
		} else {
			if (psd->sacl) {
				if (!onefs_samba_acl_to_acl(psd->sacl,
					&saclp, &ignore_aces, snum))
					return NT_STATUS_ACCESS_DENIED;

				if (ignore_aces == true) {
					*security_info_effective &=
					    ~SECINFO_SACL;
				}
			}
		}
	}

	/* Setup ifs_security_descriptor */
	DEBUG(5,("Setting up SD\n"));
	if (aclu_initialize_sd(sd, psd->type, ownerp, groupp,
		(daclp ? &daclp : NULL), (saclp ? &saclp : NULL), false))
		return NT_STATUS_ACCESS_DENIED;

	DEBUG(10, ("sec_info_sent: 0x%x, sec_info_effective: 0x%x.\n",
		   security_info_sent, *security_info_effective));

	return NT_STATUS_OK;
}

/**
 * Isilon-specific function for setting an NTFS ACL on an open file.
 *
 * @return NT_STATUS_UNSUCCESSFUL for userspace errors, NTSTATUS based off
 * errno on syscall errors
 */
NTSTATUS
onefs_fset_nt_acl(vfs_handle_struct *handle, files_struct *fsp,
		  uint32_t sec_info_sent, const struct security_descriptor *psd)
{
	struct ifs_security_descriptor sd = {};
	int fd = -1;
	bool fopened = false;
	NTSTATUS status;
	uint32_t sec_info_effective = 0;

	START_PROFILE(syscall_set_sd);

	DEBUG(5,("Setting SD on file %s.\n", fsp_str_dbg(fsp)));

	status = onefs_samba_sd_to_sd(sec_info_sent, psd, &sd,
				      SNUM(handle->conn), &sec_info_effective);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(3, ("SD initialization failure: %s\n", nt_errstr(status)));
		goto out;
	}

	fd = fsp->fh->fd;
	if (fd == -1) {
		DEBUG(10,("Reopening file %s.\n", fsp_str_dbg(fsp)));
		if ((fd = onefs_sys_create_file(handle->conn,
						-1,
						fsp->fsp_name->base_name,
						0,
						0,
						0,
						0,
						0,
						0,
						INTERNAL_OPEN_ONLY,
						0,
						NULL,
						0,
						NULL)) == -1) {
			DEBUG(0, ("Error opening file %s. errno=%d (%s)\n",
				  fsp_str_dbg(fsp), errno, strerror(errno)));
			status = map_nt_error_from_unix(errno);
			goto out;
		}
		fopened = true;
	}

        errno = 0;
	if (ifs_set_security_descriptor(fd, sec_info_effective, &sd)) {
		DEBUG(0, ("Error setting security descriptor = %s\n",
			  strerror(errno)));
		status = map_nt_error_from_unix(errno);
		goto out;
	}

	DEBUG(5, ("Security descriptor set correctly!\n"));
	status = NT_STATUS_OK;

	/* FALLTHROUGH */
out:
	END_PROFILE(syscall_set_sd);

	if (fopened)
		close(fd);

	aclu_free_sd(&sd, false);
	return status;
}
