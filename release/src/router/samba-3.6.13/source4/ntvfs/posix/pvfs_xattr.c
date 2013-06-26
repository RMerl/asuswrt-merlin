/* 
   Unix SMB/CIFS implementation.

   POSIX NTVFS backend - xattr support

   Copyright (C) Andrew Tridgell 2004

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
#include "param/param.h"

/*
  pull a xattr as a blob
*/
static NTSTATUS pull_xattr_blob(struct pvfs_state *pvfs,
				TALLOC_CTX *mem_ctx,
				const char *attr_name, 
				const char *fname, 
				int fd, 
				size_t estimated_size,
				DATA_BLOB *blob)
{
	NTSTATUS status;

	if (pvfs->ea_db) {
		return pull_xattr_blob_tdb(pvfs, mem_ctx, attr_name, fname, 
					   fd, estimated_size, blob);
	}

	status = pull_xattr_blob_system(pvfs, mem_ctx, attr_name, fname, 
					fd, estimated_size, blob);

	/* if the filesystem doesn't support them, then tell pvfs not to try again */
	if (NT_STATUS_EQUAL(status, NT_STATUS_NOT_SUPPORTED)||
	    NT_STATUS_EQUAL(status, NT_STATUS_NOT_IMPLEMENTED)||
	    NT_STATUS_EQUAL(status, NT_STATUS_INVALID_SYSTEM_SERVICE)) {
		DEBUG(2,("pvfs_xattr: xattr not supported in filesystem: %s\n", nt_errstr(status)));
		pvfs->flags &= ~PVFS_FLAG_XATTR_ENABLE;
		status = NT_STATUS_NOT_FOUND;
	}

	return status;
}

/*
  push a xattr as a blob
*/
static NTSTATUS push_xattr_blob(struct pvfs_state *pvfs,
				const char *attr_name, 
				const char *fname, 
				int fd, 
				const DATA_BLOB *blob)
{
	if (pvfs->ea_db) {
		return push_xattr_blob_tdb(pvfs, attr_name, fname, fd, blob);
	}
	return push_xattr_blob_system(pvfs, attr_name, fname, fd, blob);
}


/*
  delete a xattr
*/
static NTSTATUS delete_xattr(struct pvfs_state *pvfs, const char *attr_name, 
			     const char *fname, int fd)
{
	if (pvfs->ea_db) {
		return delete_xattr_tdb(pvfs, attr_name, fname, fd);
	}
	return delete_xattr_system(pvfs, attr_name, fname, fd);
}

/*
  a hook called on unlink - allows the tdb xattr backend to cleanup
*/
NTSTATUS pvfs_xattr_unlink_hook(struct pvfs_state *pvfs, const char *fname)
{
	if (pvfs->ea_db) {
		return unlink_xattr_tdb(pvfs, fname);
	}
	return unlink_xattr_system(pvfs, fname);
}


/*
  load a NDR structure from a xattr
*/
NTSTATUS pvfs_xattr_ndr_load(struct pvfs_state *pvfs,
			     TALLOC_CTX *mem_ctx,
			     const char *fname, int fd, const char *attr_name,
			     void *p, void *pull_fn)
{
	NTSTATUS status;
	DATA_BLOB blob;
	enum ndr_err_code ndr_err;

	status = pull_xattr_blob(pvfs, mem_ctx, attr_name, fname, 
				 fd, XATTR_DOSATTRIB_ESTIMATED_SIZE, &blob);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	/* pull the blob */
	ndr_err = ndr_pull_struct_blob(&blob, mem_ctx, p,
								   (ndr_pull_flags_fn_t)pull_fn);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return ndr_map_error2ntstatus(ndr_err);
	}

	data_blob_free(&blob);

	return NT_STATUS_OK;
}

/*
  save a NDR structure into a xattr
*/
NTSTATUS pvfs_xattr_ndr_save(struct pvfs_state *pvfs,
			     const char *fname, int fd, const char *attr_name, 
			     void *p, void *push_fn)
{
	TALLOC_CTX *mem_ctx = talloc_new(NULL);
	DATA_BLOB blob;
	NTSTATUS status;
	enum ndr_err_code ndr_err;

	ndr_err = ndr_push_struct_blob(&blob, mem_ctx, p, (ndr_push_flags_fn_t)push_fn);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(mem_ctx);
		return ndr_map_error2ntstatus(ndr_err);
	}

	status = push_xattr_blob(pvfs, attr_name, fname, fd, &blob);
	talloc_free(mem_ctx);

	return status;
}


/*
  fill in file attributes from extended attributes
*/
NTSTATUS pvfs_dosattrib_load(struct pvfs_state *pvfs, struct pvfs_filename *name, int fd)
{
	NTSTATUS status;
	struct xattr_DosAttrib attrib;
	TALLOC_CTX *mem_ctx = talloc_new(name);
	struct xattr_DosInfo1 *info1;
	struct xattr_DosInfo2Old *info2;

	if (name->stream_name != NULL) {
		name->stream_exists = false;
	} else {
		name->stream_exists = true;
	}

	if (!(pvfs->flags & PVFS_FLAG_XATTR_ENABLE)) {
		return NT_STATUS_OK;
	}

	status = pvfs_xattr_ndr_load(pvfs, mem_ctx, name->full_name, 
				     fd, XATTR_DOSATTRIB_NAME,
				     &attrib,
				     (void *) ndr_pull_xattr_DosAttrib);

	/* not having a DosAttrib is not an error */
	if (NT_STATUS_EQUAL(status, NT_STATUS_NOT_FOUND)) {
		talloc_free(mem_ctx);
		return pvfs_stream_info(pvfs, name, fd);
	}

	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(mem_ctx);
		return status;
	}

	switch (attrib.version) {
	case 1:
		info1 = &attrib.info.info1;
		name->dos.attrib = pvfs_attrib_normalise(info1->attrib, 
							 name->st.st_mode);
		name->dos.ea_size = info1->ea_size;
		if (name->st.st_size == info1->size) {
			name->dos.alloc_size = 
				pvfs_round_alloc_size(pvfs, info1->alloc_size);
		}
		if (!null_nttime(info1->create_time)) {
			name->dos.create_time = info1->create_time;
		}
		if (!null_nttime(info1->change_time)) {
			name->dos.change_time = info1->change_time;
		}
		name->dos.flags = 0;
		break;

	case 2:
		/*
		 * Note: This is only used to parse existing values from disk
		 *       We use xattr_DosInfo1 again for storing new values
		 */
		info2 = &attrib.info.oldinfo2;
		name->dos.attrib = pvfs_attrib_normalise(info2->attrib, 
							 name->st.st_mode);
		name->dos.ea_size = info2->ea_size;
		if (name->st.st_size == info2->size) {
			name->dos.alloc_size = 
				pvfs_round_alloc_size(pvfs, info2->alloc_size);
		}
		if (!null_nttime(info2->create_time)) {
			name->dos.create_time = info2->create_time;
		}
		if (!null_nttime(info2->change_time)) {
			name->dos.change_time = info2->change_time;
		}
		name->dos.flags = info2->flags;
		break;

	default:
		DEBUG(0,("ERROR: Unsupported xattr DosAttrib version %d on '%s'\n",
			 attrib.version, name->full_name));
		talloc_free(mem_ctx);
		return NT_STATUS_INVALID_LEVEL;
	}
	talloc_free(mem_ctx);
	
	status = pvfs_stream_info(pvfs, name, fd);

	return status;
}


/*
  save the file attribute into the xattr
*/
NTSTATUS pvfs_dosattrib_save(struct pvfs_state *pvfs, struct pvfs_filename *name, int fd)
{
	struct xattr_DosAttrib attrib;
	struct xattr_DosInfo1 *info1;

	if (!(pvfs->flags & PVFS_FLAG_XATTR_ENABLE)) {
		return NT_STATUS_OK;
	}

	attrib.version = 1;
	info1 = &attrib.info.info1;

	name->dos.attrib = pvfs_attrib_normalise(name->dos.attrib, name->st.st_mode);

	info1->attrib      = name->dos.attrib;
	info1->ea_size     = name->dos.ea_size;
	info1->size        = name->st.st_size;
	info1->alloc_size  = name->dos.alloc_size;
	info1->create_time = name->dos.create_time;
	info1->change_time = name->dos.change_time;

	return pvfs_xattr_ndr_save(pvfs, name->full_name, fd, 
				   XATTR_DOSATTRIB_NAME, &attrib, 
				   (void *) ndr_push_xattr_DosAttrib);
}


/*
  load the set of DOS EAs
*/
NTSTATUS pvfs_doseas_load(struct pvfs_state *pvfs, struct pvfs_filename *name, int fd,
			  struct xattr_DosEAs *eas)
{
	NTSTATUS status;
	ZERO_STRUCTP(eas);
	if (!(pvfs->flags & PVFS_FLAG_XATTR_ENABLE)) {
		return NT_STATUS_OK;
	}
	status = pvfs_xattr_ndr_load(pvfs, eas, name->full_name, fd, XATTR_DOSEAS_NAME,
				     eas, (void *) ndr_pull_xattr_DosEAs);
	if (NT_STATUS_EQUAL(status, NT_STATUS_NOT_FOUND)) {
		return NT_STATUS_OK;
	}
	return status;
}

/*
  save the set of DOS EAs
*/
NTSTATUS pvfs_doseas_save(struct pvfs_state *pvfs, struct pvfs_filename *name, int fd,
			  struct xattr_DosEAs *eas)
{
	if (!(pvfs->flags & PVFS_FLAG_XATTR_ENABLE)) {
		return NT_STATUS_OK;
	}
	return pvfs_xattr_ndr_save(pvfs, name->full_name, fd, XATTR_DOSEAS_NAME, eas, 
				   (void *) ndr_push_xattr_DosEAs);
}


/*
  load the set of streams from extended attributes
*/
NTSTATUS pvfs_streams_load(struct pvfs_state *pvfs, struct pvfs_filename *name, int fd,
			   struct xattr_DosStreams *streams)
{
	NTSTATUS status;
	ZERO_STRUCTP(streams);
	if (!(pvfs->flags & PVFS_FLAG_XATTR_ENABLE)) {
		return NT_STATUS_OK;
	}
	status = pvfs_xattr_ndr_load(pvfs, streams, name->full_name, fd, 
				     XATTR_DOSSTREAMS_NAME,
				     streams, 
				     (void *) ndr_pull_xattr_DosStreams);
	if (NT_STATUS_EQUAL(status, NT_STATUS_NOT_FOUND)) {
		return NT_STATUS_OK;
	}
	return status;
}

/*
  save the set of streams into filesystem xattr
*/
NTSTATUS pvfs_streams_save(struct pvfs_state *pvfs, struct pvfs_filename *name, int fd,
			   struct xattr_DosStreams *streams)
{
	if (!(pvfs->flags & PVFS_FLAG_XATTR_ENABLE)) {
		return NT_STATUS_OK;
	}
	return pvfs_xattr_ndr_save(pvfs, name->full_name, fd, 
				   XATTR_DOSSTREAMS_NAME, 
				   streams, 
				   (void *) ndr_push_xattr_DosStreams);
}


/*
  load the current ACL from extended attributes
*/
NTSTATUS pvfs_acl_load(struct pvfs_state *pvfs, struct pvfs_filename *name, int fd,
		       struct xattr_NTACL *acl)
{
	NTSTATUS status;
	ZERO_STRUCTP(acl);
	if (!(pvfs->flags & PVFS_FLAG_XATTR_ENABLE)) {
		return NT_STATUS_NOT_FOUND;
	}
	status = pvfs_xattr_ndr_load(pvfs, acl, name->full_name, fd, 
				     XATTR_NTACL_NAME,
				     acl, 
				     (void *) ndr_pull_xattr_NTACL);
	return status;
}

/*
  save the acl for a file into filesystem xattr
*/
NTSTATUS pvfs_acl_save(struct pvfs_state *pvfs, struct pvfs_filename *name, int fd,
		       struct xattr_NTACL *acl)
{
	NTSTATUS status;
	void *privs;

	if (!(pvfs->flags & PVFS_FLAG_XATTR_ENABLE)) {
		return NT_STATUS_OK;
	}

	/* this xattr is in the "system" namespace, so we need
	   admin privileges to set it */
	privs = root_privileges();
	status = pvfs_xattr_ndr_save(pvfs, name->full_name, fd, 
				     XATTR_NTACL_NAME, 
				     acl, 
				     (void *) ndr_push_xattr_NTACL);
	talloc_free(privs);
	return status;
}

/*
  create a zero length xattr with the given name
*/
NTSTATUS pvfs_xattr_create(struct pvfs_state *pvfs, 
			   const char *fname, int fd,
			   const char *attr_prefix,
			   const char *attr_name)
{
	NTSTATUS status;
	DATA_BLOB blob = data_blob(NULL, 0);
	char *aname = talloc_asprintf(NULL, "%s%s", attr_prefix, attr_name);
	if (aname == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	status = push_xattr_blob(pvfs, aname, fname, fd, &blob);
	talloc_free(aname);
	return status;
}


/*
  delete a xattr with the given name
*/
NTSTATUS pvfs_xattr_delete(struct pvfs_state *pvfs, 
			   const char *fname, int fd,
			   const char *attr_prefix,
			   const char *attr_name)
{
	NTSTATUS status;
	char *aname = talloc_asprintf(NULL, "%s%s", attr_prefix, attr_name);
	if (aname == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	status = delete_xattr(pvfs, aname, fname, fd);
	talloc_free(aname);
	return status;
}

/*
  load a xattr with the given name
*/
NTSTATUS pvfs_xattr_load(struct pvfs_state *pvfs, 
			 TALLOC_CTX *mem_ctx,
			 const char *fname, int fd,
			 const char *attr_prefix,
			 const char *attr_name,
			 size_t estimated_size,
			 DATA_BLOB *blob)
{
	NTSTATUS status;
	char *aname = talloc_asprintf(mem_ctx, "%s%s", attr_prefix, attr_name);
	if (aname == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	status = pull_xattr_blob(pvfs, mem_ctx, aname, fname, fd, estimated_size, blob);
	talloc_free(aname);
	return status;
}

/*
  save a xattr with the given name
*/
NTSTATUS pvfs_xattr_save(struct pvfs_state *pvfs, 
			 const char *fname, int fd,
			 const char *attr_prefix,
			 const char *attr_name,
			 const DATA_BLOB *blob)
{
	NTSTATUS status;
	char *aname = talloc_asprintf(NULL, "%s%s", attr_prefix, attr_name);
	if (aname == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	status = push_xattr_blob(pvfs, aname, fname, fd, blob);
	talloc_free(aname);
	return status;
}


/*
  probe for system support for xattrs
*/
void pvfs_xattr_probe(struct pvfs_state *pvfs)
{
	TALLOC_CTX *tmp_ctx = talloc_new(pvfs);
	DATA_BLOB blob;
	pull_xattr_blob(pvfs, tmp_ctx, "user.XattrProbe", pvfs->base_directory, 
			-1, 1, &blob);
	pull_xattr_blob(pvfs, tmp_ctx, "security.XattrProbe", pvfs->base_directory, 
			-1, 1, &blob);
	talloc_free(tmp_ctx);
}
