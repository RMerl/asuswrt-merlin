/*
   Unix SMB/CIFS implementation.
   Check access to files based on security descriptors.
   Copyright (C) Jeremy Allison 2005-2006.
   Copyright (C) Michael Adam 2007.

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
#include "../libcli/security/security.h"
#include "../librpc/gen_ndr/ndr_security.h"
#include "smbd/smbd.h"

#undef  DBGC_CLASS
#define DBGC_CLASS DBGC_ACLS

/**
 * Security descriptor / NT Token level access check function.
 */
bool can_access_file_acl(struct connection_struct *conn,
			 const struct smb_filename *smb_fname,
			 uint32_t access_mask)
{
	NTSTATUS status;
	uint32_t access_granted;
	struct security_descriptor *secdesc = NULL;
	bool ret;

	if (get_current_uid(conn) == (uid_t)0) {
		/* I'm sorry sir, I didn't know you were root... */
		return true;
	}

	if (access_mask == DELETE_ACCESS &&
			VALID_STAT(smb_fname->st) &&
			S_ISLNK(smb_fname->st.st_ex_mode)) {
		/* We can always delete a symlink. */
		return true;
	}

	status = SMB_VFS_GET_NT_ACL(conn, smb_fname->base_name,
				    (SECINFO_OWNER |
				     SECINFO_GROUP |
				     SECINFO_DACL),
				    &secdesc);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(5, ("Could not get acl: %s\n", nt_errstr(status)));
		ret = false;
		goto out;
	}

	status = se_access_check(secdesc, get_current_nttok(conn),
				 access_mask, &access_granted);
	ret = NT_STATUS_IS_OK(status);

	if (DEBUGLEVEL >= 10) {
		DEBUG(10,("can_access_file_acl for file %s "
			"access_mask 0x%x, access_granted 0x%x "
			"access %s\n",
			smb_fname_str_dbg(smb_fname),
			(unsigned int)access_mask,
			(unsigned int)access_granted,
			ret ? "ALLOWED" : "DENIED" ));
		NDR_PRINT_DEBUG(security_descriptor, secdesc);
	}
 out:
	TALLOC_FREE(secdesc);
	return ret;
}

/****************************************************************************
 Actually emulate the in-kernel access checking for delete access. We need
 this to successfully return ACCESS_DENIED on a file open for delete access.
****************************************************************************/

bool can_delete_file_in_directory(connection_struct *conn,
				  const struct smb_filename *smb_fname)
{
	TALLOC_CTX *ctx = talloc_tos();
	char *dname = NULL;
	struct smb_filename *smb_fname_parent = NULL;
	NTSTATUS status;
	bool ret;

	if (!CAN_WRITE(conn)) {
		return False;
	}

	if (!lp_acl_check_permissions(SNUM(conn))) {
		/* This option means don't check. */
		return true;
	}

	/* Get the parent directory permission mask and owners. */
	if (!parent_dirname(ctx, smb_fname->base_name, &dname, NULL)) {
		return False;
	}

	status = create_synthetic_smb_fname(ctx, dname, NULL, NULL,
					    &smb_fname_parent);
	if (!NT_STATUS_IS_OK(status)) {
		ret = false;
		goto out;
	}

	if(SMB_VFS_STAT(conn, smb_fname_parent) != 0) {
		ret = false;
		goto out;
	}

	/* fast paths first */

	if (!S_ISDIR(smb_fname_parent->st.st_ex_mode)) {
		ret = false;
		goto out;
	}
	if (get_current_uid(conn) == (uid_t)0) {
		/* I'm sorry sir, I didn't know you were root... */
		ret = true;
		goto out;
	}

#ifdef S_ISVTX
	/* sticky bit means delete only by owner of file or by root or
	 * by owner of directory. */
	if (smb_fname_parent->st.st_ex_mode & S_ISVTX) {
		if (!VALID_STAT(smb_fname->st)) {
			/* If the file doesn't already exist then
			 * yes we'll be able to delete it. */
			ret = true;
			goto out;
		}

		/*
		 * Patch from SATOH Fumiyasu <fumiyas@miraclelinux.com>
		 * for bug #3348. Don't assume owning sticky bit
		 * directory means write access allowed.
		 * Fail to delete if we're not the owner of the file,
		 * or the owner of the directory as we have no possible
		 * chance of deleting. Otherwise, go on and check the ACL.
		 */
		if ((get_current_uid(conn) !=
			smb_fname_parent->st.st_ex_uid) &&
		    (get_current_uid(conn) != smb_fname->st.st_ex_uid)) {
			DEBUG(10,("can_delete_file_in_directory: not "
				  "owner of file %s or directory %s",
				  smb_fname_str_dbg(smb_fname),
				  smb_fname_str_dbg(smb_fname_parent)));
			ret = false;
			goto out;
		}
	}
#endif

	/* now for ACL checks */

	/*
	 * There's two ways to get the permission to delete a file: First by
	 * having the DELETE bit on the file itself and second if that does
	 * not help, by the DELETE_CHILD bit on the containing directory.
	 *
	 * Here we only check the directory permissions, we will
	 * check the file DELETE permission separately.
	 */

	ret = can_access_file_acl(conn, smb_fname_parent, FILE_DELETE_CHILD);
 out:
	TALLOC_FREE(dname);
	TALLOC_FREE(smb_fname_parent);
	return ret;
}

/****************************************************************************
 Actually emulate the in-kernel access checking for read/write access. We need
 this to successfully check for ability to write for dos filetimes.
 Note this doesn't take into account share write permissions.
****************************************************************************/

bool can_access_file_data(connection_struct *conn,
			  const struct smb_filename *smb_fname,
			  uint32 access_mask)
{
	if (!(access_mask & (FILE_READ_DATA|FILE_WRITE_DATA))) {
		return False;
	}
	access_mask &= (FILE_READ_DATA|FILE_WRITE_DATA);

	/* some fast paths first */

	DEBUG(10,("can_access_file_data: requesting 0x%x on file %s\n",
		  (unsigned int)access_mask, smb_fname_str_dbg(smb_fname)));

	if (get_current_uid(conn) == (uid_t)0) {
		/* I'm sorry sir, I didn't know you were root... */
		return True;
	}

	SMB_ASSERT(VALID_STAT(smb_fname->st));

	/* Check primary owner access. */
	if (get_current_uid(conn) == smb_fname->st.st_ex_uid) {
		switch (access_mask) {
			case FILE_READ_DATA:
				return (smb_fname->st.st_ex_mode & S_IRUSR) ?
				    True : False;

			case FILE_WRITE_DATA:
				return (smb_fname->st.st_ex_mode & S_IWUSR) ?
				    True : False;

			default: /* FILE_READ_DATA|FILE_WRITE_DATA */

				if ((smb_fname->st.st_ex_mode &
					(S_IWUSR|S_IRUSR)) ==
				    (S_IWUSR|S_IRUSR)) {
					return True;
				} else {
					return False;
				}
		}
	}

	/* now for ACL checks */

	return can_access_file_acl(conn, smb_fname, access_mask);
}

/****************************************************************************
 Userspace check for write access.
 Note this doesn't take into account share write permissions.
****************************************************************************/

bool can_write_to_file(connection_struct *conn,
		       const struct smb_filename *smb_fname)
{
	return can_access_file_data(conn, smb_fname, FILE_WRITE_DATA);
}

/****************************************************************************
 Check for an existing default Windows ACL on a directory.
****************************************************************************/

bool directory_has_default_acl(connection_struct *conn, const char *fname)
{
	/* returns talloced off tos. */
	struct security_descriptor *secdesc = NULL;
	unsigned int i;
	NTSTATUS status = SMB_VFS_GET_NT_ACL(conn, fname,
				SECINFO_DACL, &secdesc);

	if (!NT_STATUS_IS_OK(status) ||
			secdesc == NULL ||
			secdesc->dacl == NULL) {
		TALLOC_FREE(secdesc);
		return false;
	}

	for (i = 0; i < secdesc->dacl->num_aces; i++) {
		struct security_ace *psa = &secdesc->dacl->aces[i];
		if (psa->flags & (SEC_ACE_FLAG_OBJECT_INHERIT|
				SEC_ACE_FLAG_CONTAINER_INHERIT)) {
			TALLOC_FREE(secdesc);
			return true;
		}
	}
	TALLOC_FREE(secdesc);
	return false;
}
