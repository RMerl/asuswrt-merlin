/* 
   Unix SMB/CIFS implementation.

   POSIX NTVFS backend - NT ACLs in xattrs

   Copyright (C) Andrew Tridgell 2006

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
#include "vfs_posix.h"
#include "../lib/util/unix_privs.h"
#include "librpc/gen_ndr/ndr_xattr.h"

/*
  load the current ACL from extended attributes
*/
static NTSTATUS pvfs_acl_load_xattr(struct pvfs_state *pvfs, struct pvfs_filename *name, int fd,
				    TALLOC_CTX *mem_ctx,
				    struct security_descriptor **sd)
{
	NTSTATUS status;
	struct xattr_NTACL *acl;

	if (!(pvfs->flags & PVFS_FLAG_XATTR_ENABLE)) {
		return NT_STATUS_NOT_FOUND;
	}

	acl = talloc_zero(mem_ctx, struct xattr_NTACL);
	NT_STATUS_HAVE_NO_MEMORY(acl);

	status = pvfs_xattr_ndr_load(pvfs, mem_ctx, name->full_name, fd, 
				     XATTR_NTACL_NAME,
				     acl, (void *) ndr_pull_xattr_NTACL);

	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(acl);
		return status;
	}

	if (acl->version != 1) {
		talloc_free(acl);
		return NT_STATUS_INVALID_ACL;
	}
	
	*sd = talloc_steal(mem_ctx, acl->info.sd);

	return NT_STATUS_OK;
}

/*
  save the acl for a file into filesystem xattr
*/
static NTSTATUS pvfs_acl_save_xattr(struct pvfs_state *pvfs, struct pvfs_filename *name, int fd,
				    struct security_descriptor *sd)
{
	NTSTATUS status;
	void *privs;
	struct xattr_NTACL acl;

	if (!(pvfs->flags & PVFS_FLAG_XATTR_ENABLE)) {
		return NT_STATUS_OK;
	}

	acl.version = 1;
	acl.info.sd = sd;

	/* this xattr is in the "system" namespace, so we need
	   admin privileges to set it */
	privs = root_privileges();
	status = pvfs_xattr_ndr_save(pvfs, name->full_name, fd, 
				     XATTR_NTACL_NAME, 
				     &acl, (void *) ndr_push_xattr_NTACL);
	talloc_free(privs);
	return status;
}


/*
  initialise pvfs acl xattr backend
*/
NTSTATUS pvfs_acl_xattr_init(void)
{
	struct pvfs_acl_ops ops = {
		.name = "xattr",
		.acl_load = pvfs_acl_load_xattr,
		.acl_save = pvfs_acl_save_xattr
	};
	return pvfs_acl_register(&ops);
}
