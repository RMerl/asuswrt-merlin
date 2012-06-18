/* 
   Unix SMB/CIFS implementation.

   POSIX NTVFS backend - NT ACLs mapped to NFS4 ACLs, as per
   http://www.suse.de/~agruen/nfs4acl/

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
#include "librpc/gen_ndr/ndr_nfs4acl.h"
#include "libcli/security/security.h"

#define ACE4_IDENTIFIER_GROUP 0x40

/*
  load the current ACL from system.nfs4acl
*/
static NTSTATUS pvfs_acl_load_nfs4(struct pvfs_state *pvfs, struct pvfs_filename *name, int fd,
				   TALLOC_CTX *mem_ctx,
				   struct security_descriptor **psd)
{
	NTSTATUS status;
	struct nfs4acl *acl;
	struct security_descriptor *sd;
	int i, num_ids;
	struct id_mapping *ids;
	struct composite_context *ctx;

	acl = talloc_zero(mem_ctx, struct nfs4acl);
	NT_STATUS_HAVE_NO_MEMORY(acl);

	status = pvfs_xattr_ndr_load(pvfs, mem_ctx, name->full_name, fd, 
				     NFS4ACL_XATTR_NAME,
				     acl, ndr_pull_nfs4acl);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(acl);
		return status;
	}

	*psd = security_descriptor_initialise(mem_ctx);
	NT_STATUS_HAVE_NO_MEMORY(*psd);

	sd = *psd;

	sd->type |= acl->a_flags;

	/* the number of ids to map is the acl count plus uid and gid */
	num_ids = acl->a_count +2;
	ids = talloc_array(sd, struct id_mapping, num_ids);
	NT_STATUS_HAVE_NO_MEMORY(ids);

	ids[0].unixid = talloc(ids, struct unixid);
	NT_STATUS_HAVE_NO_MEMORY(ids[0].unixid);
	ids[0].unixid->id = name->st.st_uid;
	ids[0].unixid->type = ID_TYPE_UID;
	ids[0].sid = NULL;
	ids[0].status = NT_STATUS_NONE_MAPPED;

	ids[1].unixid = talloc(ids, struct unixid);
	NT_STATUS_HAVE_NO_MEMORY(ids[1].unixid);
	ids[1].unixid->id = name->st.st_gid;
	ids[1].unixid->type = ID_TYPE_GID;
	ids[1].sid = NULL;
	ids[1].status = NT_STATUS_NONE_MAPPED;

	for (i=0;i<acl->a_count;i++) {
		struct nfs4ace *a = &acl->ace[i];
		ids[i+2].unixid = talloc(ids, struct unixid);
		NT_STATUS_HAVE_NO_MEMORY(ids[i+2].unixid);
		ids[i+2].unixid->id = a->e_id;
		if (a->e_flags & ACE4_IDENTIFIER_GROUP) {
			ids[i+2].unixid->type = ID_TYPE_GID;
		} else {
			ids[i+2].unixid->type = ID_TYPE_UID;
		}
		ids[i+2].sid = NULL;
		ids[i+2].status = NT_STATUS_NONE_MAPPED;
	}

	/* Allocate memory for the sids from the security descriptor to be on
	 * the safe side. */
	ctx = wbc_xids_to_sids_send(pvfs->wbc_ctx, sd, num_ids, ids);
	NT_STATUS_HAVE_NO_MEMORY(ctx);
	status = wbc_xids_to_sids_recv(ctx, &ids);
	NT_STATUS_NOT_OK_RETURN(status);

	sd->owner_sid = talloc_steal(sd, ids[0].sid);
	sd->group_sid = talloc_steal(sd, ids[1].sid);

	for (i=0;i<acl->a_count;i++) {
		struct nfs4ace *a = &acl->ace[i];
		struct security_ace ace;
		ace.type = a->e_type;
		ace.flags = a->e_flags;
		ace.access_mask = a->e_mask;
		ace.trustee = *ids[i+2].sid;
		security_descriptor_dacl_add(sd, &ace);
	}

	return NT_STATUS_OK;
}

/*
  save the acl for a file into system.nfs4acl
*/
static NTSTATUS pvfs_acl_save_nfs4(struct pvfs_state *pvfs, struct pvfs_filename *name, int fd,
				   struct security_descriptor *sd)
{
	NTSTATUS status;
	void *privs;
	struct nfs4acl acl;
	int i;
	TALLOC_CTX *tmp_ctx;
	struct id_mapping *ids;
	struct composite_context *ctx;

	tmp_ctx = talloc_new(pvfs);
	NT_STATUS_HAVE_NO_MEMORY(tmp_ctx);

	acl.a_version = 0;
	acl.a_flags   = sd->type;
	acl.a_count   = sd->dacl?sd->dacl->num_aces:0;
	acl.a_owner_mask = 0;
	acl.a_group_mask = 0;
	acl.a_other_mask = 0;

	acl.ace = talloc_array(tmp_ctx, struct nfs4ace, acl.a_count);
	if (!acl.ace) {
		talloc_free(tmp_ctx);
		return NT_STATUS_NO_MEMORY;
	}

	ids = talloc_array(tmp_ctx, struct id_mapping, acl.a_count);
	if (ids == NULL) {
		talloc_free(tmp_ctx);
		return NT_STATUS_NO_MEMORY;
	}

	for (i=0;i<acl.a_count;i++) {
		struct security_ace *ace = &sd->dacl->aces[i];
		ids[i].unixid = NULL;
		ids[i].sid = dom_sid_dup(ids, &ace->trustee);
		if (ids[i].sid == NULL) {
			talloc_free(tmp_ctx);
			return NT_STATUS_NO_MEMORY;
		}
		ids[i].status = NT_STATUS_NONE_MAPPED;
	}

	ctx = wbc_sids_to_xids_send(pvfs->wbc_ctx,ids, acl.a_count, ids);
	if (ctx == NULL) {
		talloc_free(tmp_ctx);
		return NT_STATUS_NO_MEMORY;
	}
	status = wbc_sids_to_xids_recv(ctx, &ids);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(tmp_ctx);
		return status;
	}

	for (i=0;i<acl.a_count;i++) {
		struct nfs4ace *a = &acl.ace[i];
		struct security_ace *ace = &sd->dacl->aces[i];
		a->e_type  = ace->type;
		a->e_flags = ace->flags;
		a->e_mask  = ace->access_mask;
		if (ids[i].unixid->type != ID_TYPE_UID) {
			a->e_flags |= ACE4_IDENTIFIER_GROUP;
		}
		a->e_id = ids[i].unixid->id;
		a->e_who   = "";
	}

	privs = root_privileges();
	status = pvfs_xattr_ndr_save(pvfs, name->full_name, fd, 
				     NFS4ACL_XATTR_NAME, 
				     &acl, ndr_push_nfs4acl);
	talloc_free(privs);

	talloc_free(tmp_ctx);
	return status;
}


/*
  initialise pvfs acl NFS4 backend
*/
NTSTATUS pvfs_acl_nfs4_init(void)
{
	struct pvfs_acl_ops ops = {
		.name = "nfs4acl",
		.acl_load = pvfs_acl_load_nfs4,
		.acl_save = pvfs_acl_save_nfs4
	};
	return pvfs_acl_register(&ops);
}
