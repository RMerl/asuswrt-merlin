/*
 * Store posix-level xattrs in a tdb
 *
 * Copyright (C) Volker Lendecke, 2007
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
#include "system/filesys.h"
#include "smbd/smbd.h"
#include "librpc/gen_ndr/xattr.h"
#include "librpc/gen_ndr/ndr_xattr.h"
#include "../librpc/gen_ndr/ndr_netlogon.h"
#include "dbwrap.h"
#include "util_tdb.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_VFS

/*
 * unmarshall tdb_xattrs
 */

static NTSTATUS xattr_tdb_pull_attrs(TALLOC_CTX *mem_ctx,
				     const TDB_DATA *data,
				     struct tdb_xattrs **presult)
{
	DATA_BLOB blob;
	enum ndr_err_code ndr_err;
	struct tdb_xattrs *result;

	if (!(result = TALLOC_ZERO_P(mem_ctx, struct tdb_xattrs))) {
		return NT_STATUS_NO_MEMORY;
	}

	if (data->dsize == 0) {
		*presult = result;
		return NT_STATUS_OK;
	}

	blob = data_blob_const(data->dptr, data->dsize);

	ndr_err = ndr_pull_struct_blob(&blob, result, result,
		(ndr_pull_flags_fn_t)ndr_pull_tdb_xattrs);

	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DEBUG(0, ("ndr_pull_tdb_xattrs failed: %s\n",
			  ndr_errstr(ndr_err)));
		TALLOC_FREE(result);
		return ndr_map_error2ntstatus(ndr_err);
	}

	*presult = result;
	return NT_STATUS_OK;
}

/*
 * marshall tdb_xattrs
 */

static NTSTATUS xattr_tdb_push_attrs(TALLOC_CTX *mem_ctx,
				     const struct tdb_xattrs *attribs,
				     TDB_DATA *data)
{
	DATA_BLOB blob;
	enum ndr_err_code ndr_err;

	ndr_err = ndr_push_struct_blob(&blob, mem_ctx, attribs,
		(ndr_push_flags_fn_t)ndr_push_tdb_xattrs);

	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DEBUG(0, ("ndr_push_tdb_xattrs failed: %s\n",
			  ndr_errstr(ndr_err)));
		return ndr_map_error2ntstatus(ndr_err);
	}

	*data = make_tdb_data(blob.data, blob.length);
	return NT_STATUS_OK;
}

/*
 * Load tdb_xattrs for a file from the tdb
 */

static NTSTATUS xattr_tdb_load_attrs(TALLOC_CTX *mem_ctx,
				     struct db_context *db_ctx,
				     const struct file_id *id,
				     struct tdb_xattrs **presult)
{
	uint8 id_buf[16];
	NTSTATUS status;
	TDB_DATA data;

	/* For backwards compatibility only store the dev/inode. */
	push_file_id_16((char *)id_buf, id);

	if (db_ctx->fetch(db_ctx, mem_ctx,
			  make_tdb_data(id_buf, sizeof(id_buf)),
			  &data) == -1) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	status = xattr_tdb_pull_attrs(mem_ctx, &data, presult);
	TALLOC_FREE(data.dptr);
	return status;
}

/*
 * fetch_lock the tdb_ea record for a file
 */

static struct db_record *xattr_tdb_lock_attrs(TALLOC_CTX *mem_ctx,
					      struct db_context *db_ctx,
					      const struct file_id *id)
{
	uint8 id_buf[16];

	/* For backwards compatibility only store the dev/inode. */
	push_file_id_16((char *)id_buf, id);
	return db_ctx->fetch_locked(db_ctx, mem_ctx,
				    make_tdb_data(id_buf, sizeof(id_buf)));
}

/*
 * Save tdb_xattrs to a previously fetch_locked record
 */

static NTSTATUS xattr_tdb_save_attrs(struct db_record *rec,
				     const struct tdb_xattrs *attribs)
{
	TDB_DATA data = tdb_null;
	NTSTATUS status;

	status = xattr_tdb_push_attrs(talloc_tos(), attribs, &data);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("xattr_tdb_push_attrs failed: %s\n",
			  nt_errstr(status)));
		return status;
	}

	status = rec->store(rec, data, 0);

	TALLOC_FREE(data.dptr);

	return status;
}

/*
 * Worker routine for getxattr and fgetxattr
 */

static ssize_t xattr_tdb_getattr(struct db_context *db_ctx,
				 const struct file_id *id,
				 const char *name, void *value, size_t size)
{
	struct tdb_xattrs *attribs;
	uint32_t i;
	ssize_t result = -1;
	NTSTATUS status;

	DEBUG(10, ("xattr_tdb_getattr called for file %s, name %s\n",
		   file_id_string_tos(id), name));

	status = xattr_tdb_load_attrs(talloc_tos(), db_ctx, id, &attribs);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("xattr_tdb_fetch_attrs failed: %s\n",
			   nt_errstr(status)));
		errno = EINVAL;
		return -1;
	}

	for (i=0; i<attribs->num_eas; i++) {
		if (strcmp(attribs->eas[i].name, name) == 0) {
			break;
		}
	}

	if (i == attribs->num_eas) {
		errno = ENOATTR;
		goto fail;
	}

	if (attribs->eas[i].value.length > size) {
		errno = ERANGE;
		goto fail;
	}

	memcpy(value, attribs->eas[i].value.data,
	       attribs->eas[i].value.length);
	result = attribs->eas[i].value.length;

 fail:
	TALLOC_FREE(attribs);
	return result;
}

static ssize_t xattr_tdb_getxattr(struct vfs_handle_struct *handle,
				  const char *path, const char *name,
				  void *value, size_t size)
{
	SMB_STRUCT_STAT sbuf;
	struct file_id id;
	struct db_context *db;

	SMB_VFS_HANDLE_GET_DATA(handle, db, struct db_context, return -1);

	if (vfs_stat_smb_fname(handle->conn, path, &sbuf) == -1) {
		return -1;
	}

	id = SMB_VFS_FILE_ID_CREATE(handle->conn, &sbuf);

	return xattr_tdb_getattr(db, &id, name, value, size);
}

static ssize_t xattr_tdb_fgetxattr(struct vfs_handle_struct *handle,
				   struct files_struct *fsp,
				   const char *name, void *value, size_t size)
{
	SMB_STRUCT_STAT sbuf;
	struct file_id id;
	struct db_context *db;

	SMB_VFS_HANDLE_GET_DATA(handle, db, struct db_context, return -1);

	if (SMB_VFS_FSTAT(fsp, &sbuf) == -1) {
		return -1;
	}

	id = SMB_VFS_FILE_ID_CREATE(handle->conn, &sbuf);

	return xattr_tdb_getattr(db, &id, name, value, size);
}

/*
 * Worker routine for setxattr and fsetxattr
 */

static int xattr_tdb_setattr(struct db_context *db_ctx,
			     const struct file_id *id, const char *name,
			     const void *value, size_t size, int flags)
{
	NTSTATUS status;
	struct db_record *rec;
	struct tdb_xattrs *attribs;
	uint32_t i;

	DEBUG(10, ("xattr_tdb_setattr called for file %s, name %s\n",
		   file_id_string_tos(id), name));

	rec = xattr_tdb_lock_attrs(talloc_tos(), db_ctx, id);

	if (rec == NULL) {
		DEBUG(0, ("xattr_tdb_lock_attrs failed\n"));
		errno = EINVAL;
		return -1;
	}

	status = xattr_tdb_pull_attrs(rec, &rec->value, &attribs);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("xattr_tdb_fetch_attrs failed: %s\n",
			   nt_errstr(status)));
		TALLOC_FREE(rec);
		return -1;
	}

	for (i=0; i<attribs->num_eas; i++) {
		if (strcmp(attribs->eas[i].name, name) == 0) {
			if (flags & XATTR_CREATE) {
				TALLOC_FREE(rec);
				errno = EEXIST;
				return -1;
			}
			break;
		}
	}

	if (i == attribs->num_eas) {
		struct xattr_EA *tmp;

		if (flags & XATTR_REPLACE) {
			TALLOC_FREE(rec);
			errno = ENOATTR;
			return -1;
		}

		tmp = TALLOC_REALLOC_ARRAY(
			attribs, attribs->eas, struct xattr_EA,
			attribs->num_eas+ 1);

		if (tmp == NULL) {
			DEBUG(0, ("TALLOC_REALLOC_ARRAY failed\n"));
			TALLOC_FREE(rec);
			errno = ENOMEM;
			return -1;
		}

		attribs->eas = tmp;
		attribs->num_eas += 1;
	}

	attribs->eas[i].name = name;
	attribs->eas[i].value.data = CONST_DISCARD(uint8 *, value);
	attribs->eas[i].value.length = size;

	status = xattr_tdb_save_attrs(rec, attribs);

	TALLOC_FREE(rec);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("save failed: %s\n", nt_errstr(status)));
		return -1;
	}

	return 0;
}

static int xattr_tdb_setxattr(struct vfs_handle_struct *handle,
			      const char *path, const char *name,
			      const void *value, size_t size, int flags)
{
	SMB_STRUCT_STAT sbuf;
	struct file_id id;
	struct db_context *db;

	SMB_VFS_HANDLE_GET_DATA(handle, db, struct db_context, return -1);

	if (vfs_stat_smb_fname(handle->conn, path, &sbuf) == -1) {
		return -1;
	}

	id = SMB_VFS_FILE_ID_CREATE(handle->conn, &sbuf);

	return xattr_tdb_setattr(db, &id, name, value, size, flags);
}

static int xattr_tdb_fsetxattr(struct vfs_handle_struct *handle,
			       struct files_struct *fsp,
			       const char *name, const void *value,
			       size_t size, int flags)
{
	SMB_STRUCT_STAT sbuf;
	struct file_id id;
	struct db_context *db;

	SMB_VFS_HANDLE_GET_DATA(handle, db, struct db_context, return -1);

	if (SMB_VFS_FSTAT(fsp, &sbuf) == -1) {
		return -1;
	}

	id = SMB_VFS_FILE_ID_CREATE(handle->conn, &sbuf);

	return xattr_tdb_setattr(db, &id, name, value, size, flags);
}

/*
 * Worker routine for listxattr and flistxattr
 */

static ssize_t xattr_tdb_listattr(struct db_context *db_ctx,
				  const struct file_id *id, char *list,
				  size_t size)
{
	NTSTATUS status;
	struct tdb_xattrs *attribs;
	uint32_t i;
	size_t len = 0;

	status = xattr_tdb_load_attrs(talloc_tos(), db_ctx, id, &attribs);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("xattr_tdb_fetch_attrs failed: %s\n",
			   nt_errstr(status)));
		errno = EINVAL;
		return -1;
	}

	DEBUG(10, ("xattr_tdb_listattr: Found %d xattrs\n",
		   attribs->num_eas));

	for (i=0; i<attribs->num_eas; i++) {
		size_t tmp;

		DEBUG(10, ("xattr_tdb_listattr: xattrs[i].name: %s\n",
			   attribs->eas[i].name));

		tmp = strlen(attribs->eas[i].name);

		/*
		 * Try to protect against overflow
		 */

		if (len + (tmp+1) < len) {
			TALLOC_FREE(attribs);
			errno = EINVAL;
			return -1;
		}

		/*
		 * Take care of the terminating NULL
		 */
		len += (tmp + 1);
	}

	if (len > size) {
		TALLOC_FREE(attribs);
		errno = ERANGE;
		return -1;
	}

	len = 0;

	for (i=0; i<attribs->num_eas; i++) {
		strlcpy(list+len, attribs->eas[i].name,
			size-len);
		len += (strlen(attribs->eas[i].name) + 1);
	}

	TALLOC_FREE(attribs);
	return len;
}

static ssize_t xattr_tdb_listxattr(struct vfs_handle_struct *handle,
				   const char *path, char *list, size_t size)
{
	SMB_STRUCT_STAT sbuf;
	struct file_id id;
	struct db_context *db;

	SMB_VFS_HANDLE_GET_DATA(handle, db, struct db_context, return -1);

	if (vfs_stat_smb_fname(handle->conn, path, &sbuf) == -1) {
		return -1;
	}

	id = SMB_VFS_FILE_ID_CREATE(handle->conn, &sbuf);

	return xattr_tdb_listattr(db, &id, list, size);
}

static ssize_t xattr_tdb_flistxattr(struct vfs_handle_struct *handle,
				    struct files_struct *fsp, char *list,
				    size_t size)
{
	SMB_STRUCT_STAT sbuf;
	struct file_id id;
	struct db_context *db;

	SMB_VFS_HANDLE_GET_DATA(handle, db, struct db_context, return -1);

	if (SMB_VFS_FSTAT(fsp, &sbuf) == -1) {
		return -1;
	}

	id = SMB_VFS_FILE_ID_CREATE(handle->conn, &sbuf);

	return xattr_tdb_listattr(db, &id, list, size);
}

/*
 * Worker routine for removexattr and fremovexattr
 */

static int xattr_tdb_removeattr(struct db_context *db_ctx,
				const struct file_id *id, const char *name)
{
	NTSTATUS status;
	struct db_record *rec;
	struct tdb_xattrs *attribs;
	uint32_t i;

	rec = xattr_tdb_lock_attrs(talloc_tos(), db_ctx, id);

	if (rec == NULL) {
		DEBUG(0, ("xattr_tdb_lock_attrs failed\n"));
		errno = EINVAL;
		return -1;
	}

	status = xattr_tdb_pull_attrs(rec, &rec->value, &attribs);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("xattr_tdb_fetch_attrs failed: %s\n",
			   nt_errstr(status)));
		TALLOC_FREE(rec);
		return -1;
	}

	for (i=0; i<attribs->num_eas; i++) {
		if (strcmp(attribs->eas[i].name, name) == 0) {
			break;
		}
	}

	if (i == attribs->num_eas) {
		TALLOC_FREE(rec);
		errno = ENOATTR;
		return -1;
	}

	attribs->eas[i] =
		attribs->eas[attribs->num_eas-1];
	attribs->num_eas -= 1;

	if (attribs->num_eas == 0) {
		rec->delete_rec(rec);
		TALLOC_FREE(rec);
		return 0;
	}

	status = xattr_tdb_save_attrs(rec, attribs);

	TALLOC_FREE(rec);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("save failed: %s\n", nt_errstr(status)));
		return -1;
	}

	return 0;
}

static int xattr_tdb_removexattr(struct vfs_handle_struct *handle,
				 const char *path, const char *name)
{
	SMB_STRUCT_STAT sbuf;
	struct file_id id;
	struct db_context *db;

	SMB_VFS_HANDLE_GET_DATA(handle, db, struct db_context, return -1);

	if (vfs_stat_smb_fname(handle->conn, path, &sbuf) == -1) {
		return -1;
	}

	id = SMB_VFS_FILE_ID_CREATE(handle->conn, &sbuf);

	return xattr_tdb_removeattr(db, &id, name);
}

static int xattr_tdb_fremovexattr(struct vfs_handle_struct *handle,
				  struct files_struct *fsp, const char *name)
{
	SMB_STRUCT_STAT sbuf;
	struct file_id id;
	struct db_context *db;

	SMB_VFS_HANDLE_GET_DATA(handle, db, struct db_context, return -1);

	if (SMB_VFS_FSTAT(fsp, &sbuf) == -1) {
		return -1;
	}

	id = SMB_VFS_FILE_ID_CREATE(handle->conn, &sbuf);

	return xattr_tdb_removeattr(db, &id, name);
}

/*
 * Open the tdb file upon VFS_CONNECT
 */

static bool xattr_tdb_init(int snum, struct db_context **p_db)
{
	struct db_context *db;
	const char *dbname;
	char *def_dbname;

	def_dbname = state_path("xattr.tdb");
	if (def_dbname == NULL) {
		errno = ENOSYS;
		return false;
	}

	dbname = lp_parm_const_string(snum, "xattr_tdb", "file", def_dbname);

	/* now we know dbname is not NULL */

	become_root();
	db = db_open(NULL, dbname, 0, TDB_DEFAULT, O_RDWR|O_CREAT, 0600);
	unbecome_root();

	if (db == NULL) {
#if defined(ENOTSUP)
		errno = ENOTSUP;
#else
		errno = ENOSYS;
#endif
		TALLOC_FREE(def_dbname);
		return false;
	}

	*p_db = db;
	TALLOC_FREE(def_dbname);
	return true;
}

/*
 * On unlink we need to delete the tdb record
 */
static int xattr_tdb_unlink(vfs_handle_struct *handle,
			    const struct smb_filename *smb_fname)
{
	struct smb_filename *smb_fname_tmp = NULL;
	struct file_id id;
	struct db_context *db;
	struct db_record *rec;
	NTSTATUS status;
	int ret = -1;
	bool remove_record = false;

	SMB_VFS_HANDLE_GET_DATA(handle, db, struct db_context, return -1);

	status = copy_smb_filename(talloc_tos(), smb_fname, &smb_fname_tmp);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		return -1;
	}

	if (lp_posix_pathnames()) {
		ret = SMB_VFS_LSTAT(handle->conn, smb_fname_tmp);
	} else {
		ret = SMB_VFS_STAT(handle->conn, smb_fname_tmp);
	}
	if (ret == -1) {
		goto out;
	}

	if (smb_fname_tmp->st.st_ex_nlink == 1) {
		/* Only remove record on last link to file. */
		remove_record = true;
	}

	ret = SMB_VFS_NEXT_UNLINK(handle, smb_fname_tmp);

	if (ret == -1) {
		goto out;
	}

	if (!remove_record) {
		goto out;
	}

	id = SMB_VFS_FILE_ID_CREATE(handle->conn, &smb_fname_tmp->st);

	rec = xattr_tdb_lock_attrs(talloc_tos(), db, &id);

	/*
	 * If rec == NULL there's not much we can do about it
	 */

	if (rec != NULL) {
		rec->delete_rec(rec);
		TALLOC_FREE(rec);
	}

 out:
	TALLOC_FREE(smb_fname_tmp);
	return ret;
}

/*
 * On rmdir we need to delete the tdb record
 */
static int xattr_tdb_rmdir(vfs_handle_struct *handle, const char *path)
{
	SMB_STRUCT_STAT sbuf;
	struct file_id id;
	struct db_context *db;
	struct db_record *rec;
	int ret;

	SMB_VFS_HANDLE_GET_DATA(handle, db, struct db_context, return -1);

	if (vfs_stat_smb_fname(handle->conn, path, &sbuf) == -1) {
		return -1;
	}

	ret = SMB_VFS_NEXT_RMDIR(handle, path);

	if (ret == -1) {
		return -1;
	}

	id = SMB_VFS_FILE_ID_CREATE(handle->conn, &sbuf);

	rec = xattr_tdb_lock_attrs(talloc_tos(), db, &id);

	/*
	 * If rec == NULL there's not much we can do about it
	 */

	if (rec != NULL) {
		rec->delete_rec(rec);
		TALLOC_FREE(rec);
	}

	return 0;
}

/*
 * Destructor for the VFS private data
 */

static void close_xattr_db(void **data)
{
	struct db_context **p_db = (struct db_context **)data;
	TALLOC_FREE(*p_db);
}

static int xattr_tdb_connect(vfs_handle_struct *handle, const char *service,
			  const char *user)
{
	char *sname = NULL;
	int res, snum;
	struct db_context *db;

	res = SMB_VFS_NEXT_CONNECT(handle, service, user);
	if (res < 0) {
		return res;
	}

	snum = find_service(talloc_tos(), service, &sname);
	if (snum == -1 || sname == NULL) {
		/*
		 * Should not happen, but we should not fail just *here*.
		 */
		return 0;
	}

	if (!xattr_tdb_init(snum, &db)) {
		DEBUG(5, ("Could not init xattr tdb\n"));
		lp_do_parameter(snum, "ea support", "False");
		return 0;
	}

	lp_do_parameter(snum, "ea support", "True");

	SMB_VFS_HANDLE_SET_DATA(handle, db, close_xattr_db,
				struct db_context, return -1);

	return 0;
}

static struct vfs_fn_pointers vfs_xattr_tdb_fns = {
	.getxattr = xattr_tdb_getxattr,
	.fgetxattr = xattr_tdb_fgetxattr,
	.setxattr = xattr_tdb_setxattr,
	.fsetxattr = xattr_tdb_fsetxattr,
	.listxattr = xattr_tdb_listxattr,
	.flistxattr = xattr_tdb_flistxattr,
	.removexattr = xattr_tdb_removexattr,
	.fremovexattr = xattr_tdb_fremovexattr,
	.unlink = xattr_tdb_unlink,
	.rmdir = xattr_tdb_rmdir,
	.connect_fn = xattr_tdb_connect,
};

NTSTATUS vfs_xattr_tdb_init(void);
NTSTATUS vfs_xattr_tdb_init(void)
{
	return smb_register_vfs(SMB_VFS_INTERFACE_VERSION, "xattr_tdb",
				&vfs_xattr_tdb_fns);
}
