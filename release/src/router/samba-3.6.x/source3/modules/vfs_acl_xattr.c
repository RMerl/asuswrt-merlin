/*
 * Store Windows ACLs in xattrs.
 *
 * Copyright (C) Volker Lendecke, 2008
 * Copyright (C) Jeremy Allison, 2008
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
#include "librpc/gen_ndr/xattr.h"
#include "librpc/gen_ndr/ndr_xattr.h"
#include "../lib/crypto/crypto.h"
#include "auth.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_VFS

/* Pull in the common functions. */
#define ACL_MODULE_NAME "acl_xattr"

#include "modules/vfs_acl_common.c"

/*******************************************************************
 Pull a security descriptor into a DATA_BLOB from a xattr.
*******************************************************************/

static NTSTATUS get_acl_blob(TALLOC_CTX *ctx,
			vfs_handle_struct *handle,
			files_struct *fsp,
			const char *name,
			DATA_BLOB *pblob)
{
	size_t size = 1024;
	uint8_t *val = NULL;
	uint8_t *tmp;
	ssize_t sizeret;
	int saved_errno = 0;

	ZERO_STRUCTP(pblob);

  again:

	tmp = TALLOC_REALLOC_ARRAY(ctx, val, uint8_t, size);
	if (tmp == NULL) {
		TALLOC_FREE(val);
		return NT_STATUS_NO_MEMORY;
	}
	val = tmp;

	become_root();
	if (fsp && fsp->fh->fd != -1) {
		sizeret = SMB_VFS_FGETXATTR(fsp, XATTR_NTACL_NAME, val, size);
	} else {
		sizeret = SMB_VFS_GETXATTR(handle->conn, name,
					XATTR_NTACL_NAME, val, size);
	}
	if (sizeret == -1) {
		saved_errno = errno;
	}
	unbecome_root();

	/* Max ACL size is 65536 bytes. */
	if (sizeret == -1) {
		errno = saved_errno;
		if ((errno == ERANGE) && (size != 65536)) {
			/* Too small, try again. */
			size = 65536;
			goto again;
		}

		/* Real error - exit here. */
		TALLOC_FREE(val);
		return map_nt_error_from_unix(errno);
	}

	pblob->data = val;
	pblob->length = sizeret;
	return NT_STATUS_OK;
}

/*******************************************************************
 Store a DATA_BLOB into an xattr given an fsp pointer.
*******************************************************************/

static NTSTATUS store_acl_blob_fsp(vfs_handle_struct *handle,
				files_struct *fsp,
				DATA_BLOB *pblob)
{
	int ret;
	int saved_errno = 0;

	DEBUG(10,("store_acl_blob_fsp: storing blob length %u on file %s\n",
		  (unsigned int)pblob->length, fsp_str_dbg(fsp)));

	become_root();
	if (fsp->fh->fd != -1) {
		ret = SMB_VFS_FSETXATTR(fsp, XATTR_NTACL_NAME,
			pblob->data, pblob->length, 0);
	} else {
		ret = SMB_VFS_SETXATTR(fsp->conn, fsp->fsp_name->base_name,
				XATTR_NTACL_NAME,
				pblob->data, pblob->length, 0);
	}
	if (ret) {
		saved_errno = errno;
	}
	unbecome_root();
	if (ret) {
		DEBUG(5, ("store_acl_blob_fsp: setting attr failed for file %s"
			"with error %s\n",
			fsp_str_dbg(fsp),
			strerror(saved_errno) ));
		errno = saved_errno;
		return map_nt_error_from_unix(saved_errno);
	}
	return NT_STATUS_OK;
}

/*********************************************************************
 Remove a Windows ACL - we're setting the underlying POSIX ACL.
*********************************************************************/

static int sys_acl_set_file_xattr(vfs_handle_struct *handle,
                              const char *name,
                              SMB_ACL_TYPE_T type,
                              SMB_ACL_T theacl)
{
	int ret = SMB_VFS_NEXT_SYS_ACL_SET_FILE(handle,
						name,
						type,
						theacl);
	if (ret == -1) {
		return -1;
	}

	become_root();
	SMB_VFS_REMOVEXATTR(handle->conn, name, XATTR_NTACL_NAME);
	unbecome_root();

	return ret;
}

/*********************************************************************
 Remove a Windows ACL - we're setting the underlying POSIX ACL.
*********************************************************************/

static int sys_acl_set_fd_xattr(vfs_handle_struct *handle,
                            files_struct *fsp,
                            SMB_ACL_T theacl)
{
	int ret = SMB_VFS_NEXT_SYS_ACL_SET_FD(handle,
						fsp,
						theacl);
	if (ret == -1) {
		return -1;
	}

	become_root();
	SMB_VFS_FREMOVEXATTR(fsp, XATTR_NTACL_NAME);
	unbecome_root();

	return ret;
}

static int connect_acl_xattr(struct vfs_handle_struct *handle,
				const char *service,
				const char *user)
{
	int ret = SMB_VFS_NEXT_CONNECT(handle, service, user);

	if (ret < 0) {
		return ret;
	}

	/* Ensure we have the parameters correct if we're
	 * using this module. */
	DEBUG(2,("connect_acl_xattr: setting 'inherit acls = true' "
		"'dos filemode = true' and "
		"'force unknown acl user = true' for service %s\n",
		service ));

        lp_do_parameter(SNUM(handle->conn), "inherit acls", "true");
        lp_do_parameter(SNUM(handle->conn), "dos filemode", "true");
        lp_do_parameter(SNUM(handle->conn), "force unknown acl user", "true");

	return 0;
}

static struct vfs_fn_pointers vfs_acl_xattr_fns = {
	.connect_fn = connect_acl_xattr,
	.opendir = opendir_acl_common,
	.mkdir = mkdir_acl_common,
	.rmdir = rmdir_acl_common,
	.open_fn = open_acl_common,
	.create_file = create_file_acl_common,
	.unlink = unlink_acl_common,
	.chmod = chmod_acl_module_common,
	.fchmod = fchmod_acl_module_common,
	.fget_nt_acl = fget_nt_acl_common,
	.get_nt_acl = get_nt_acl_common,
	.fset_nt_acl = fset_nt_acl_common,
	.chmod_acl = chmod_acl_acl_module_common,
	.fchmod_acl = fchmod_acl_acl_module_common,
	.sys_acl_set_file = sys_acl_set_file_xattr,
	.sys_acl_set_fd = sys_acl_set_fd_xattr
};

NTSTATUS vfs_acl_xattr_init(void)
{
	return smb_register_vfs(SMB_VFS_INTERFACE_VERSION, "acl_xattr",
				&vfs_acl_xattr_fns);
}
