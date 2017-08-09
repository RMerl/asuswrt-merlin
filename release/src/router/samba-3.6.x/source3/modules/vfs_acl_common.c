/*
 * Store Windows ACLs in data store - common functions.
 * #included into modules/vfs_acl_xattr.c and modules/vfs_acl_tdb.c
 *
 * Copyright (C) Volker Lendecke, 2008
 * Copyright (C) Jeremy Allison, 2009
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

#include "smbd/smbd.h"
#include "system/filesys.h"
#include "../libcli/security/security.h"
#include "../librpc/gen_ndr/ndr_security.h"
#include "../lib/util/bitmap.h"

static NTSTATUS create_acl_blob(const struct security_descriptor *psd,
			DATA_BLOB *pblob,
			uint16_t hash_type,
			uint8_t hash[XATTR_SD_HASH_SIZE]);

static NTSTATUS get_acl_blob(TALLOC_CTX *ctx,
			vfs_handle_struct *handle,
			files_struct *fsp,
			const char *name,
			DATA_BLOB *pblob);

static NTSTATUS store_acl_blob_fsp(vfs_handle_struct *handle,
			files_struct *fsp,
			DATA_BLOB *pblob);

#define HASH_SECURITY_INFO (SECINFO_OWNER | \
				SECINFO_GROUP | \
				SECINFO_DACL | \
				SECINFO_SACL)

/*******************************************************************
 Hash a security descriptor.
*******************************************************************/

static NTSTATUS hash_sd_sha256(struct security_descriptor *psd,
			uint8_t *hash)
{
	DATA_BLOB blob;
	SHA256_CTX tctx;
	NTSTATUS status;

	memset(hash, '\0', XATTR_SD_HASH_SIZE);
	status = create_acl_blob(psd, &blob, XATTR_SD_HASH_TYPE_SHA256, hash);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	SHA256_Init(&tctx);
	SHA256_Update(&tctx, blob.data, blob.length);
	SHA256_Final(hash, &tctx);

	return NT_STATUS_OK;
}

/*******************************************************************
 Parse out a struct security_descriptor from a DATA_BLOB.
*******************************************************************/

static NTSTATUS parse_acl_blob(const DATA_BLOB *pblob,
				struct security_descriptor **ppdesc,
				uint16_t *p_hash_type,
				uint8_t hash[XATTR_SD_HASH_SIZE])
{
	TALLOC_CTX *ctx = talloc_tos();
	struct xattr_NTACL xacl;
	enum ndr_err_code ndr_err;
	size_t sd_size;

	ndr_err = ndr_pull_struct_blob(pblob, ctx, &xacl,
			(ndr_pull_flags_fn_t)ndr_pull_xattr_NTACL);

	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DEBUG(5, ("parse_acl_blob: ndr_pull_xattr_NTACL failed: %s\n",
			ndr_errstr(ndr_err)));
		return ndr_map_error2ntstatus(ndr_err);
	}

	switch (xacl.version) {
		case 2:
			*ppdesc = make_sec_desc(ctx, SD_REVISION,
					xacl.info.sd_hs2->sd->type | SEC_DESC_SELF_RELATIVE,
					xacl.info.sd_hs2->sd->owner_sid,
					xacl.info.sd_hs2->sd->group_sid,
					xacl.info.sd_hs2->sd->sacl,
					xacl.info.sd_hs2->sd->dacl,
					&sd_size);
			/* No hash - null out. */
			*p_hash_type = XATTR_SD_HASH_TYPE_NONE;
			memset(hash, '\0', XATTR_SD_HASH_SIZE);
			break;
		case 3:
			*ppdesc = make_sec_desc(ctx, SD_REVISION,
					xacl.info.sd_hs3->sd->type | SEC_DESC_SELF_RELATIVE,
					xacl.info.sd_hs3->sd->owner_sid,
					xacl.info.sd_hs3->sd->group_sid,
					xacl.info.sd_hs3->sd->sacl,
					xacl.info.sd_hs3->sd->dacl,
					&sd_size);
			*p_hash_type = xacl.info.sd_hs3->hash_type;
			/* Current version 3. */
			memcpy(hash, xacl.info.sd_hs3->hash, XATTR_SD_HASH_SIZE);
			break;
		default:
			return NT_STATUS_REVISION_MISMATCH;
	}

	TALLOC_FREE(xacl.info.sd);

	return (*ppdesc != NULL) ? NT_STATUS_OK : NT_STATUS_NO_MEMORY;
}

/*******************************************************************
 Create a DATA_BLOB from a security descriptor.
*******************************************************************/

static NTSTATUS create_acl_blob(const struct security_descriptor *psd,
			DATA_BLOB *pblob,
			uint16_t hash_type,
			uint8_t hash[XATTR_SD_HASH_SIZE])
{
	struct xattr_NTACL xacl;
	struct security_descriptor_hash_v3 sd_hs3;
	enum ndr_err_code ndr_err;
	TALLOC_CTX *ctx = talloc_tos();

	ZERO_STRUCT(xacl);
	ZERO_STRUCT(sd_hs3);

	xacl.version = 3;
	xacl.info.sd_hs3 = &sd_hs3;
	xacl.info.sd_hs3->sd = CONST_DISCARD(struct security_descriptor *, psd);
	xacl.info.sd_hs3->hash_type = hash_type;
	memcpy(&xacl.info.sd_hs3->hash[0], hash, XATTR_SD_HASH_SIZE);

	ndr_err = ndr_push_struct_blob(
			pblob, ctx, &xacl,
			(ndr_push_flags_fn_t)ndr_push_xattr_NTACL);

	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DEBUG(5, ("create_acl_blob: ndr_push_xattr_NTACL failed: %s\n",
			ndr_errstr(ndr_err)));
		return ndr_map_error2ntstatus(ndr_err);
	}

	return NT_STATUS_OK;
}

/*******************************************************************
 Add in 3 inheritable components for a non-inheritable directory ACL.
 CREATOR_OWNER/CREATOR_GROUP/WORLD.
*******************************************************************/

static NTSTATUS add_directory_inheritable_components(vfs_handle_struct *handle,
                                const char *name,
				SMB_STRUCT_STAT *psbuf,
				struct security_descriptor *psd)
{
	struct connection_struct *conn = handle->conn;
	int num_aces = (psd->dacl ? psd->dacl->num_aces : 0);
	struct smb_filename smb_fname;
	enum security_ace_type acltype;
	uint32_t access_mask;
	mode_t dir_mode;
	mode_t file_mode;
	mode_t mode;
	struct security_ace *new_ace_list = TALLOC_ZERO_ARRAY(talloc_tos(),
						struct security_ace,
						num_aces + 3);

	if (new_ace_list == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	/* Fake a quick smb_filename. */
	ZERO_STRUCT(smb_fname);
	smb_fname.st = *psbuf;
	smb_fname.base_name = CONST_DISCARD(char *, name);

	dir_mode = unix_mode(conn,
			FILE_ATTRIBUTE_DIRECTORY, &smb_fname, NULL);
	file_mode = unix_mode(conn,
			FILE_ATTRIBUTE_ARCHIVE, &smb_fname, NULL);

	mode = dir_mode | file_mode;

	DEBUG(10, ("add_directory_inheritable_components: directory %s, "
		"mode = 0%o\n",
		name,
		(unsigned int)mode ));

	if (num_aces) {
		memcpy(new_ace_list, psd->dacl->aces,
			num_aces * sizeof(struct security_ace));
	}
	access_mask = map_canon_ace_perms(SNUM(conn), &acltype,
				mode & 0700, false);

	init_sec_ace(&new_ace_list[num_aces],
			&global_sid_Creator_Owner,
			acltype,
			access_mask,
			SEC_ACE_FLAG_CONTAINER_INHERIT|
				SEC_ACE_FLAG_OBJECT_INHERIT|
				SEC_ACE_FLAG_INHERIT_ONLY);
	access_mask = map_canon_ace_perms(SNUM(conn), &acltype,
				(mode << 3) & 0700, false);
	init_sec_ace(&new_ace_list[num_aces+1],
			&global_sid_Creator_Group,
			acltype,
			access_mask,
			SEC_ACE_FLAG_CONTAINER_INHERIT|
				SEC_ACE_FLAG_OBJECT_INHERIT|
				SEC_ACE_FLAG_INHERIT_ONLY);
	access_mask = map_canon_ace_perms(SNUM(conn), &acltype,
				(mode << 6) & 0700, false);
	init_sec_ace(&new_ace_list[num_aces+2],
			&global_sid_World,
			acltype,
			access_mask,
			SEC_ACE_FLAG_CONTAINER_INHERIT|
				SEC_ACE_FLAG_OBJECT_INHERIT|
				SEC_ACE_FLAG_INHERIT_ONLY);
	if (psd->dacl) {
		psd->dacl->aces = new_ace_list;
		psd->dacl->num_aces += 3;
	} else {
		psd->dacl = make_sec_acl(talloc_tos(),
				NT4_ACL_REVISION,
				3,
				new_ace_list);
		if (psd->dacl == NULL) {
			return NT_STATUS_NO_MEMORY;
		}
	}
	return NT_STATUS_OK;
}

/*******************************************************************
 Pull a DATA_BLOB from an xattr given a pathname.
 If the hash doesn't match, or doesn't exist - return the underlying
 filesystem sd.
*******************************************************************/

static NTSTATUS get_nt_acl_internal(vfs_handle_struct *handle,
				files_struct *fsp,
				const char *name,
			        uint32_t security_info,
				struct security_descriptor **ppdesc)
{
	DATA_BLOB blob = data_blob_null;
	NTSTATUS status;
	uint16_t hash_type = XATTR_SD_HASH_TYPE_NONE;
	uint8_t hash[XATTR_SD_HASH_SIZE];
	uint8_t hash_tmp[XATTR_SD_HASH_SIZE];
	struct security_descriptor *psd = NULL;
	struct security_descriptor *pdesc_next = NULL;
	bool ignore_file_system_acl = lp_parm_bool(SNUM(handle->conn),
						ACL_MODULE_NAME,
						"ignore system acls",
						false);

	if (fsp && name == NULL) {
		name = fsp->fsp_name->base_name;
	}

	DEBUG(10, ("get_nt_acl_internal: name=%s\n", name));

	/* Get the full underlying sd for the hash
	   or to return as backup. */
	if (fsp) {
		status = SMB_VFS_NEXT_FGET_NT_ACL(handle,
				fsp,
				HASH_SECURITY_INFO,
				&pdesc_next);
	} else {
		status = SMB_VFS_NEXT_GET_NT_ACL(handle,
				name,
				HASH_SECURITY_INFO,
				&pdesc_next);
	}

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("get_nt_acl_internal: get_next_acl for file %s "
			"returned %s\n",
			name,
			nt_errstr(status)));
		return status;
	}

	status = get_acl_blob(talloc_tos(), handle, fsp, name, &blob);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("get_nt_acl_internal: get_acl_blob returned %s\n",
			nt_errstr(status)));
		psd = pdesc_next;
		goto out;
	}

	status = parse_acl_blob(&blob, &psd,
				&hash_type, &hash[0]);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("parse_acl_blob returned %s\n",
				nt_errstr(status)));
		psd = pdesc_next;
		goto out;
	}

	/* Ensure the hash type is one we know. */
	switch (hash_type) {
		case XATTR_SD_HASH_TYPE_NONE:
			/* No hash, just return blob sd. */
			goto out;
		case XATTR_SD_HASH_TYPE_SHA256:
			break;
		default:
			DEBUG(10, ("get_nt_acl_internal: ACL blob revision "
				"mismatch (%u) for file %s\n",
				(unsigned int)hash_type,
				name));
			TALLOC_FREE(psd);
			psd = pdesc_next;
			goto out;
	}

	if (ignore_file_system_acl) {
		goto out;
	}

	status = hash_sd_sha256(pdesc_next, hash_tmp);
	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(psd);
		psd = pdesc_next;
		goto out;
	}

	if (memcmp(&hash[0], &hash_tmp[0], XATTR_SD_HASH_SIZE) == 0) {
		/* Hash matches, return blob sd. */
		DEBUG(10, ("get_nt_acl_internal: blob hash "
			"matches for file %s\n",
			name ));
		goto out;
	}

	/* Hash doesn't match, return underlying sd. */
	TALLOC_FREE(psd);
	psd = pdesc_next;

  out:

	if (psd != pdesc_next) {
		/* We're returning the blob, throw
 		 * away the filesystem SD. */
		TALLOC_FREE(pdesc_next);
	} else {
		SMB_STRUCT_STAT sbuf;
		SMB_STRUCT_STAT *psbuf = &sbuf;
		bool is_directory = false;
		/*
		 * We're returning the underlying ACL from the
		 * filesystem. If it's a directory, and has no
		 * inheritable ACE entries we have to fake them.
		 */
		if (fsp) {
			status = vfs_stat_fsp(fsp);
			if (!NT_STATUS_IS_OK(status)) {
				return status;
			}
			psbuf = &fsp->fsp_name->st;
		} else {
			int ret = vfs_stat_smb_fname(handle->conn,
						name,
						&sbuf);
			if (ret == -1) {
				return map_nt_error_from_unix(errno);
			}
		}
		is_directory = S_ISDIR(psbuf->st_ex_mode);

		if (ignore_file_system_acl) {
			TALLOC_FREE(pdesc_next);
			status = make_default_filesystem_acl(talloc_tos(),
						name,
						psbuf,
						&psd);
			if (!NT_STATUS_IS_OK(status)) {
				return status;
			}
		} else {
			if (is_directory &&
				!sd_has_inheritable_components(psd,
							true)) {
				status = add_directory_inheritable_components(
							handle,
							name,
							psbuf,
							psd);
				if (!NT_STATUS_IS_OK(status)) {
					return status;
				}
			}
			/* The underlying POSIX module always sets
			   the ~SEC_DESC_DACL_PROTECTED bit, as ACLs
			   can't be inherited in this way under POSIX.
			   Remove it for Windows-style ACLs. */
			psd->type &= ~SEC_DESC_DACL_PROTECTED;
		}
	}

	if (!(security_info & SECINFO_OWNER)) {
		psd->owner_sid = NULL;
	}
	if (!(security_info & SECINFO_GROUP)) {
		psd->group_sid = NULL;
	}
	if (!(security_info & SECINFO_DACL)) {
		psd->type &= ~SEC_DESC_DACL_PRESENT;
		psd->dacl = NULL;
	}
	if (!(security_info & SECINFO_SACL)) {
		psd->type &= ~SEC_DESC_SACL_PRESENT;
		psd->sacl = NULL;
	}

	TALLOC_FREE(blob.data);
	*ppdesc = psd;

	if (DEBUGLEVEL >= 10) {
		DEBUG(10,("get_nt_acl_internal: returning acl for %s is:\n",
			name ));
		NDR_PRINT_DEBUG(security_descriptor, psd);
	}

	return NT_STATUS_OK;
}

/*********************************************************************
 Create a default ACL by inheriting from the parent. If no inheritance
 from the parent available, don't set anything. This will leave the actual
 permissions the new file or directory already got from the filesystem
 as the NT ACL when read.
*********************************************************************/

static NTSTATUS inherit_new_acl(vfs_handle_struct *handle,
					files_struct *fsp,
					struct security_descriptor *parent_desc,
					bool is_directory)
{
	TALLOC_CTX *ctx = talloc_tos();
	NTSTATUS status = NT_STATUS_OK;
	struct security_descriptor *psd = NULL;
	struct dom_sid *owner_sid = NULL;
	struct dom_sid *group_sid = NULL;
	uint32_t security_info_sent = (SECINFO_OWNER | SECINFO_GROUP | SECINFO_DACL);
	bool inherit_owner = lp_inherit_owner(SNUM(handle->conn));
	bool inheritable_components = sd_has_inheritable_components(parent_desc,
					is_directory);
	size_t size;

	if (!inheritable_components && !inherit_owner) {
		/* Nothing to inherit and not setting owner. */
		return NT_STATUS_OK;
	}

	/* Create an inherited descriptor from the parent. */

	if (DEBUGLEVEL >= 10) {
		DEBUG(10,("inherit_new_acl: parent acl for %s is:\n",
			fsp_str_dbg(fsp) ));
		NDR_PRINT_DEBUG(security_descriptor, parent_desc);
	}

	/* Inherit from parent descriptor if "inherit owner" set. */
	if (inherit_owner) {
		owner_sid = parent_desc->owner_sid;
		group_sid = parent_desc->group_sid;
	}

	if (owner_sid == NULL) {
		owner_sid = &handle->conn->session_info->security_token->sids[PRIMARY_USER_SID_INDEX];
	}
	if (group_sid == NULL) {
		group_sid = &handle->conn->session_info->security_token->sids[PRIMARY_GROUP_SID_INDEX];
	}

	status = se_create_child_secdesc(ctx,
			&psd,
			&size,
			parent_desc,
			owner_sid,
			group_sid,
			is_directory);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	/* If inheritable_components == false,
	   se_create_child_secdesc()
	   creates a security desriptor with a NULL dacl
	   entry, but with SEC_DESC_DACL_PRESENT. We need
	   to remove that flag. */

	if (!inheritable_components) {
		security_info_sent &= ~SECINFO_DACL;
		psd->type &= ~SEC_DESC_DACL_PRESENT;
	}

	if (DEBUGLEVEL >= 10) {
		DEBUG(10,("inherit_new_acl: child acl for %s is:\n",
			fsp_str_dbg(fsp) ));
		NDR_PRINT_DEBUG(security_descriptor, psd);
	}

	if (inherit_owner) {
		/* We need to be root to force this. */
		become_root();
	}
	status = SMB_VFS_FSET_NT_ACL(fsp,
				security_info_sent,
				psd);
	if (inherit_owner) {
		unbecome_root();
	}
	return status;
}

static NTSTATUS get_parent_acl_common(vfs_handle_struct *handle,
				const char *path,
				struct security_descriptor **pp_parent_desc)
{
	char *parent_name = NULL;
	NTSTATUS status;

	if (!parent_dirname(talloc_tos(), path, &parent_name, NULL)) {
		return NT_STATUS_NO_MEMORY;
	}

	status = get_nt_acl_internal(handle,
					NULL,
					parent_name,
					(SECINFO_OWNER |
					 SECINFO_GROUP |
					 SECINFO_DACL  |
					 SECINFO_SACL),
					pp_parent_desc);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10,("get_parent_acl_common: get_nt_acl_internal "
			"on directory %s for "
			"path %s returned %s\n",
			parent_name,
			path,
			nt_errstr(status) ));
	}
	return status;
}

static NTSTATUS check_parent_acl_common(vfs_handle_struct *handle,
				const char *path,
				uint32_t access_mask,
				struct security_descriptor **pp_parent_desc)
{
	struct security_descriptor *parent_desc = NULL;
	uint32_t access_granted = 0;
	NTSTATUS status;

	status = get_parent_acl_common(handle, path, &parent_desc);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if (pp_parent_desc) {
		*pp_parent_desc = parent_desc;
	}
	status = smb1_file_se_access_check(handle->conn,
					parent_desc,
					get_current_nttok(handle->conn),
					access_mask,
					&access_granted);
	if(!NT_STATUS_IS_OK(status)) {
		DEBUG(10,("check_parent_acl_common: access check "
			"on parent directory of "
			"path %s for mask 0x%x returned %s\n",
			path,
			access_mask,
			nt_errstr(status) ));
		return status;
	}
	return NT_STATUS_OK;
}

/*********************************************************************
 Check ACL on open. For new files inherit from parent directory.
*********************************************************************/

static int open_acl_common(vfs_handle_struct *handle,
			struct smb_filename *smb_fname,
			files_struct *fsp,
			int flags,
			mode_t mode)
{
	uint32_t access_granted = 0;
	struct security_descriptor *pdesc = NULL;
	bool file_existed = true;
	char *fname = NULL;
	NTSTATUS status;

	if (fsp->base_fsp) {
		/* Stream open. Base filename open already did the ACL check. */
		DEBUG(10,("open_acl_common: stream open on %s\n",
			fsp_str_dbg(fsp) ));
		return SMB_VFS_NEXT_OPEN(handle, smb_fname, fsp, flags, mode);
	}

	status = get_full_smb_filename(talloc_tos(), smb_fname,
				       &fname);
	if (!NT_STATUS_IS_OK(status)) {
		goto err;
	}

	status = get_nt_acl_internal(handle,
				NULL,
				fname,
				(SECINFO_OWNER |
				 SECINFO_GROUP |
				 SECINFO_DACL  |
				 SECINFO_SACL),
				&pdesc);
        if (NT_STATUS_IS_OK(status)) {
		/* See if we can access it. */
		status = smb1_file_se_access_check(handle->conn,
					pdesc,
					get_current_nttok(handle->conn),
					fsp->access_mask,
					&access_granted);
		/*
		 * Check if we need to override ACCESS_DENIED for DELETE_ACCESS.
		 * Do this if we only failed open on DELETE_ACCESS, and
		 * we have permission to delete from the parent directory.
		 */
		if (NT_STATUS_EQUAL(status, NT_STATUS_ACCESS_DENIED) &&
			(fsp->access_mask & DELETE_ACCESS) &&
			(access_granted == DELETE_ACCESS) &&
			can_delete_file_in_directory(handle->conn, smb_fname)) {
				DEBUG(10,("open_acl_xattr: "
					"overrode "
					"DELETE_ACCESS on "
					"file %s\n",
					smb_fname_str_dbg(smb_fname)));
				status = NT_STATUS_OK;
		} else if (!NT_STATUS_IS_OK(status)) {
			DEBUG(10,("open_acl_xattr: %s open "
				"for access 0x%x (0x%x) "
				"refused with error %s\n",
				fsp_str_dbg(fsp),
				(unsigned int)fsp->access_mask,
				(unsigned int)access_granted,
				nt_errstr(status) ));
			goto err;
		}
        } else if (NT_STATUS_EQUAL(status,NT_STATUS_OBJECT_NAME_NOT_FOUND)) {
		file_existed = false;
		/*
		 * If O_CREAT is true then we're trying to create a file.
		 * Check the parent directory ACL will allow this.
		 */
		if (flags & O_CREAT) {
			struct security_descriptor *parent_desc = NULL;
			struct security_descriptor **pp_psd = NULL;

			status = check_parent_acl_common(handle, fname,
					SEC_DIR_ADD_FILE, &parent_desc);
			if (!NT_STATUS_IS_OK(status)) {
				goto err;
			}

			/* Cache the parent security descriptor for
			 * later use. */

			pp_psd = VFS_ADD_FSP_EXTENSION(handle,
					fsp,
					struct security_descriptor *,
					NULL);
			if (!pp_psd) {
				status = NT_STATUS_NO_MEMORY;
				goto err;
			}

			*pp_psd = parent_desc;
			status = NT_STATUS_OK;
		}
	}

	DEBUG(10,("open_acl_xattr: get_nt_acl_attr_internal for "
		"%s returned %s\n",
		fsp_str_dbg(fsp),
		nt_errstr(status) ));

	fsp->fh->fd = SMB_VFS_NEXT_OPEN(handle, smb_fname, fsp, flags, mode);
	return fsp->fh->fd;

  err:

	errno = map_errno_from_nt_status(status);
	return -1;
}

static int mkdir_acl_common(vfs_handle_struct *handle, const char *path, mode_t mode)
{
	int ret;
	NTSTATUS status;
	SMB_STRUCT_STAT sbuf;

	ret = vfs_stat_smb_fname(handle->conn, path, &sbuf);
	if (ret == -1 && errno == ENOENT) {
		/* We're creating a new directory. */
		status = check_parent_acl_common(handle, path,
				SEC_DIR_ADD_SUBDIR, NULL);
		if (!NT_STATUS_IS_OK(status)) {
			errno = map_errno_from_nt_status(status);
			return -1;
		}
	}

	return SMB_VFS_NEXT_MKDIR(handle, path, mode);
}

/*********************************************************************
 Fetch a security descriptor given an fsp.
*********************************************************************/

static NTSTATUS fget_nt_acl_common(vfs_handle_struct *handle, files_struct *fsp,
        uint32_t security_info, struct security_descriptor **ppdesc)
{
	return get_nt_acl_internal(handle, fsp,
				NULL, security_info, ppdesc);
}

/*********************************************************************
 Fetch a security descriptor given a pathname.
*********************************************************************/

static NTSTATUS get_nt_acl_common(vfs_handle_struct *handle,
        const char *name, uint32_t security_info, struct security_descriptor **ppdesc)
{
	return get_nt_acl_internal(handle, NULL,
				name, security_info, ppdesc);
}

/*********************************************************************
 Store a security descriptor given an fsp.
*********************************************************************/

static NTSTATUS fset_nt_acl_common(vfs_handle_struct *handle, files_struct *fsp,
        uint32_t security_info_sent, const struct security_descriptor *orig_psd)
{
	NTSTATUS status;
	DATA_BLOB blob;
	struct security_descriptor *pdesc_next = NULL;
	struct security_descriptor *psd = NULL;
	uint8_t hash[XATTR_SD_HASH_SIZE];

	if (DEBUGLEVEL >= 10) {
		DEBUG(10,("fset_nt_acl_xattr: incoming sd for file %s\n",
			  fsp_str_dbg(fsp)));
		NDR_PRINT_DEBUG(security_descriptor,
			CONST_DISCARD(struct security_descriptor *,orig_psd));
	}

	status = get_nt_acl_internal(handle, fsp,
			NULL,
			SECINFO_OWNER|SECINFO_GROUP|SECINFO_DACL|SECINFO_SACL,
			&psd);

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	psd->revision = orig_psd->revision;
	/* All our SD's are self relative. */
	psd->type = orig_psd->type | SEC_DESC_SELF_RELATIVE;

	if ((security_info_sent & SECINFO_OWNER) && (orig_psd->owner_sid != NULL)) {
		psd->owner_sid = orig_psd->owner_sid;
	}
	if ((security_info_sent & SECINFO_GROUP) && (orig_psd->group_sid != NULL)) {
		psd->group_sid = orig_psd->group_sid;
	}
	if (security_info_sent & SECINFO_DACL) {
		psd->dacl = orig_psd->dacl;
		psd->type |= SEC_DESC_DACL_PRESENT;
	}
	if (security_info_sent & SECINFO_SACL) {
		psd->sacl = orig_psd->sacl;
		psd->type |= SEC_DESC_SACL_PRESENT;
	}

	status = SMB_VFS_NEXT_FSET_NT_ACL(handle, fsp, security_info_sent, psd);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	/* Get the full underlying sd, then hash. */
	status = SMB_VFS_NEXT_FGET_NT_ACL(handle,
				fsp,
				HASH_SECURITY_INFO,
				&pdesc_next);

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = hash_sd_sha256(pdesc_next, hash);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (DEBUGLEVEL >= 10) {
		DEBUG(10,("fset_nt_acl_xattr: storing xattr sd for file %s\n",
			  fsp_str_dbg(fsp)));
		NDR_PRINT_DEBUG(security_descriptor,
			CONST_DISCARD(struct security_descriptor *,psd));
	}
	status = create_acl_blob(psd, &blob, XATTR_SD_HASH_TYPE_SHA256, hash);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("fset_nt_acl_xattr: create_acl_blob failed\n"));
		return status;
	}

	status = store_acl_blob_fsp(handle, fsp, &blob);

	return status;
}

static SMB_STRUCT_DIR *opendir_acl_common(vfs_handle_struct *handle,
			const char *fname, const char *mask, uint32 attr)
{
	NTSTATUS status;
	uint32_t access_granted = 0;
	struct security_descriptor *sd = NULL;

	status = get_nt_acl_internal(handle,
				NULL,
				fname,
				(SECINFO_OWNER |
				 SECINFO_GROUP |
				 SECINFO_DACL  |
				 SECINFO_SACL),
				&sd);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10,("opendir_acl_common: "
			"get_nt_acl_internal for dir %s "
			"failed with error %s\n",
			fname,
			nt_errstr(status) ));
		errno = map_errno_from_nt_status(status);
		return NULL;
	}

	/* See if we can access it. */
	status = smb1_file_se_access_check(handle->conn,
				sd,
				get_current_nttok(handle->conn),
				SEC_DIR_LIST,
				&access_granted);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10,("opendir_acl_common: %s open "
			"for access SEC_DIR_LIST "
			"refused with error %s\n",
			fname,
			nt_errstr(status) ));
		errno = map_errno_from_nt_status(status);
		return NULL;
	}

	return SMB_VFS_NEXT_OPENDIR(handle, fname, mask, attr);
}

static int acl_common_remove_object(vfs_handle_struct *handle,
					const char *path,
					bool is_directory)
{
	connection_struct *conn = handle->conn;
	struct file_id id;
	files_struct *fsp = NULL;
	int ret = 0;
	char *parent_dir = NULL;
	const char *final_component = NULL;
	struct smb_filename local_fname;
	int saved_errno = 0;
	char *saved_dir = NULL;

	saved_dir = vfs_GetWd(talloc_tos(),conn);
	if (!saved_dir) {
		saved_errno = errno;
		goto out;
	}

	if (!parent_dirname(talloc_tos(), path,
			&parent_dir, &final_component)) {
		saved_errno = ENOMEM;
		goto out;
	}

	DEBUG(10,("acl_common_remove_object: removing %s %s/%s\n",
		is_directory ? "directory" : "file",
		parent_dir, final_component ));

 	/* cd into the parent dir to pin it. */
	ret = vfs_ChDir(conn, parent_dir);
	if (ret == -1) {
		saved_errno = errno;
		goto out;
	}

	ZERO_STRUCT(local_fname);
	local_fname.base_name = CONST_DISCARD(char *,final_component);

	/* Must use lstat here. */
	ret = SMB_VFS_LSTAT(conn, &local_fname);
	if (ret == -1) {
		saved_errno = errno;
		goto out;
	}

	/* Ensure we have this file open with DELETE access. */
	id = vfs_file_id_from_sbuf(conn, &local_fname.st);
	for (fsp = file_find_di_first(conn->sconn, id); fsp;
		     fsp = file_find_di_next(fsp)) {
		if (fsp->access_mask & DELETE_ACCESS &&
				fsp->delete_on_close) {
			/* We did open this for delete,
			 * allow the delete as root.
			 */
			break;
		}
	}

	if (!fsp) {
		DEBUG(10,("acl_common_remove_object: %s %s/%s "
			"not an open file\n",
			is_directory ? "directory" : "file",
			parent_dir, final_component ));
		saved_errno = EACCES;
		goto out;
	}

	become_root();
	if (is_directory) {
		ret = SMB_VFS_NEXT_RMDIR(handle, final_component);
	} else {
		ret = SMB_VFS_NEXT_UNLINK(handle, &local_fname);
	}
	unbecome_root();

	if (ret == -1) {
		saved_errno = errno;
	}

  out:

	TALLOC_FREE(parent_dir);

	if (saved_dir) {
		vfs_ChDir(conn, saved_dir);
	}
	if (saved_errno) {
		errno = saved_errno;
	}
	return ret;
}

static int rmdir_acl_common(struct vfs_handle_struct *handle,
				const char *path)
{
	int ret;

	/* Try the normal rmdir first. */
	ret = SMB_VFS_NEXT_RMDIR(handle, path);
	if (ret == 0) {
		return 0;
	}
	if (errno == EACCES || errno == EPERM) {
		/* Failed due to access denied,
		   see if we need to root override. */
		return acl_common_remove_object(handle,
						path,
						true);
	}

	DEBUG(10,("rmdir_acl_common: unlink of %s failed %s\n",
		path,
		strerror(errno) ));
	return -1;
}

static NTSTATUS create_file_acl_common(struct vfs_handle_struct *handle,
				struct smb_request *req,
				uint16_t root_dir_fid,
				struct smb_filename *smb_fname,
				uint32_t access_mask,
				uint32_t share_access,
				uint32_t create_disposition,
				uint32_t create_options,
				uint32_t file_attributes,
				uint32_t oplock_request,
				uint64_t allocation_size,
				uint32_t private_flags,
				struct security_descriptor *sd,
				struct ea_list *ea_list,
				files_struct **result,
				int *pinfo)
{
	NTSTATUS status, status1;
	files_struct *fsp = NULL;
	int info;
	struct security_descriptor *parent_sd = NULL;
	struct security_descriptor **pp_parent_sd = NULL;

	status = SMB_VFS_NEXT_CREATE_FILE(handle,
					req,
					root_dir_fid,
					smb_fname,
					access_mask,
					share_access,
					create_disposition,
					create_options,
					file_attributes,
					oplock_request,
					allocation_size,
					private_flags,
					sd,
					ea_list,
					result,
					&info);

	if (!NT_STATUS_IS_OK(status)) {
		goto out;
	}

	if (info != FILE_WAS_CREATED) {
		/* File/directory was opened, not created. */
		goto out;
	}

	fsp = *result;

	if (fsp == NULL) {
		/* Only handle success. */
		goto out;
	}

	if (sd) {
		/* Security descriptor already set. */
		goto out;
	}

	if (fsp->base_fsp) {
		/* Stream open. */
		goto out;
	}

	/* See if we have a cached parent sd, if so, use it. */
	pp_parent_sd = (struct security_descriptor **)VFS_FETCH_FSP_EXTENSION(handle, fsp);
	if (!pp_parent_sd) {
		/* Must be a directory, fetch again (sigh). */
		status = get_parent_acl_common(handle,
				fsp->fsp_name->base_name,
				&parent_sd);
		if (!NT_STATUS_IS_OK(status)) {
			goto out;
		}
	} else {
		parent_sd = *pp_parent_sd;
	}

	if (!parent_sd) {
		goto err;
	}

	/* New directory - inherit from parent. */
	status1 = inherit_new_acl(handle, fsp, parent_sd, fsp->is_directory);

	if (!NT_STATUS_IS_OK(status1)) {
		DEBUG(1,("create_file_acl_common: error setting "
			"sd for %s (%s)\n",
			fsp_str_dbg(fsp),
			nt_errstr(status1) ));
	}

  out:

	if (fsp) {
		VFS_REMOVE_FSP_EXTENSION(handle, fsp);
	}

	if (NT_STATUS_IS_OK(status) && pinfo) {
		*pinfo = info;
	}
	return status;

  err:

	smb_panic("create_file_acl_common: logic error.\n");
	/* NOTREACHED */
	return status;
}

static int unlink_acl_common(struct vfs_handle_struct *handle,
			const struct smb_filename *smb_fname)
{
	int ret;

	/* Try the normal unlink first. */
	ret = SMB_VFS_NEXT_UNLINK(handle, smb_fname);
	if (ret == 0) {
		return 0;
	}
	if (errno == EACCES || errno == EPERM) {
		/* Failed due to access denied,
		   see if we need to root override. */

		/* Don't do anything fancy for streams. */
		if (smb_fname->stream_name) {
			return -1;
		}
		return acl_common_remove_object(handle,
					smb_fname->base_name,
					false);
	}

	DEBUG(10,("unlink_acl_common: unlink of %s failed %s\n",
		smb_fname->base_name,
		strerror(errno) ));
	return -1;
}

static int chmod_acl_module_common(struct vfs_handle_struct *handle,
			const char *path, mode_t mode)
{
	if (lp_posix_pathnames()) {
		/* Only allow this on POSIX pathnames. */
		return SMB_VFS_NEXT_CHMOD(handle, path, mode);
	}
	return 0;
}

static int fchmod_acl_module_common(struct vfs_handle_struct *handle,
			struct files_struct *fsp, mode_t mode)
{
	if (fsp->posix_open) {
		/* Only allow this on POSIX opens. */
		return SMB_VFS_NEXT_FCHMOD(handle, fsp, mode);
	}
	return 0;
}

static int chmod_acl_acl_module_common(struct vfs_handle_struct *handle,
			const char *name, mode_t mode)
{
	if (lp_posix_pathnames()) {
		/* Only allow this on POSIX pathnames. */
		return SMB_VFS_NEXT_CHMOD_ACL(handle, name, mode);
	}
	return 0;
}

static int fchmod_acl_acl_module_common(struct vfs_handle_struct *handle,
			struct files_struct *fsp, mode_t mode)
{
	if (fsp->posix_open) {
		/* Only allow this on POSIX opens. */
		return SMB_VFS_NEXT_FCHMOD_ACL(handle, fsp, mode);
	}
	return 0;
}
