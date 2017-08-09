/*
 * Store Windows ACLs in a tdb.
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
#include "system/filesys.h"
#include "librpc/gen_ndr/xattr.h"
#include "librpc/gen_ndr/ndr_xattr.h"
#include "../lib/crypto/crypto.h"
#include "dbwrap.h"
#include "auth.h"
#include "util_tdb.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_VFS

#define ACL_MODULE_NAME "acl_tdb"
#include "modules/vfs_acl_common.c"

static unsigned int ref_count;
static struct db_context *acl_db;

/*******************************************************************
 Open acl_db if not already open, increment ref count.
*******************************************************************/

static bool acl_tdb_init(void)
{
	char *dbname;

	if (acl_db) {
		ref_count++;
		return true;
	}

	dbname = state_path("file_ntacls.tdb");

	if (dbname == NULL) {
		errno = ENOSYS;
		return false;
	}

	become_root();
	acl_db = db_open(NULL, dbname, 0, TDB_DEFAULT, O_RDWR|O_CREAT, 0600);
	unbecome_root();

	if (acl_db == NULL) {
#if defined(ENOTSUP)
		errno = ENOTSUP;
#else
		errno = ENOSYS;
#endif
		TALLOC_FREE(dbname);
		return false;
	}

	ref_count++;
	TALLOC_FREE(dbname);
	return true;
}

/*******************************************************************
 Lower ref count and close acl_db if zero.
*******************************************************************/

static void disconnect_acl_tdb(struct vfs_handle_struct *handle)
{
	SMB_VFS_NEXT_DISCONNECT(handle);
	ref_count--;
	if (ref_count == 0) {
		TALLOC_FREE(acl_db);
	}
}

/*******************************************************************
 Fetch_lock the tdb acl record for a file
*******************************************************************/

static struct db_record *acl_tdb_lock(TALLOC_CTX *mem_ctx,
					struct db_context *db,
					const struct file_id *id)
{
	uint8 id_buf[16];

	/* For backwards compatibility only store the dev/inode. */
	push_file_id_16((char *)id_buf, id);
	return db->fetch_locked(db,
				mem_ctx,
				make_tdb_data(id_buf,
					sizeof(id_buf)));
}

/*******************************************************************
 Delete the tdb acl record for a file
*******************************************************************/

static NTSTATUS acl_tdb_delete(vfs_handle_struct *handle,
				struct db_context *db,
				SMB_STRUCT_STAT *psbuf)
{
	NTSTATUS status;
	struct file_id id = vfs_file_id_from_sbuf(handle->conn, psbuf);
	struct db_record *rec = acl_tdb_lock(talloc_tos(), db, &id);

	/*
	 * If rec == NULL there's not much we can do about it
	 */

	if (rec == NULL) {
		DEBUG(10,("acl_tdb_delete: rec == NULL\n"));
		TALLOC_FREE(rec);
		return NT_STATUS_OK;
	}

	status = rec->delete_rec(rec);
	TALLOC_FREE(rec);
	return status;
}

/*******************************************************************
 Pull a security descriptor into a DATA_BLOB from a tdb store.
*******************************************************************/

static NTSTATUS get_acl_blob(TALLOC_CTX *ctx,
			vfs_handle_struct *handle,
			files_struct *fsp,
			const char *name,
			DATA_BLOB *pblob)
{
	uint8 id_buf[16];
	TDB_DATA data;
	struct file_id id;
	struct db_context *db = acl_db;
	NTSTATUS status = NT_STATUS_OK;
	SMB_STRUCT_STAT sbuf;

	ZERO_STRUCT(sbuf);

	if (fsp) {
		status = vfs_stat_fsp(fsp);
		sbuf = fsp->fsp_name->st;
	} else {
		int ret = vfs_stat_smb_fname(handle->conn, name, &sbuf);
		if (ret == -1) {
			status = map_nt_error_from_unix(errno);
		}
	}

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	id = vfs_file_id_from_sbuf(handle->conn, &sbuf);

	/* For backwards compatibility only store the dev/inode. */
	push_file_id_16((char *)id_buf, &id);

	if (db->fetch(db,
			ctx,
			make_tdb_data(id_buf, sizeof(id_buf)),
			&data) == -1) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	pblob->data = data.dptr;
	pblob->length = data.dsize;

	DEBUG(10,("get_acl_blob: returned %u bytes from file %s\n",
		(unsigned int)data.dsize, name ));

	if (pblob->length == 0 || pblob->data == NULL) {
		return NT_STATUS_NOT_FOUND;
	}
	return NT_STATUS_OK;
}

/*******************************************************************
 Store a DATA_BLOB into a tdb record given an fsp pointer.
*******************************************************************/

static NTSTATUS store_acl_blob_fsp(vfs_handle_struct *handle,
				files_struct *fsp,
				DATA_BLOB *pblob)
{
	uint8 id_buf[16];
	struct file_id id;
	TDB_DATA data;
	struct db_context *db = acl_db;
	struct db_record *rec;
	NTSTATUS status;

	DEBUG(10,("store_acl_blob_fsp: storing blob length %u on file %s\n",
		  (unsigned int)pblob->length, fsp_str_dbg(fsp)));

	status = vfs_stat_fsp(fsp);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	id = vfs_file_id_from_sbuf(handle->conn, &fsp->fsp_name->st);

	/* For backwards compatibility only store the dev/inode. */
	push_file_id_16((char *)id_buf, &id);
	rec = db->fetch_locked(db, talloc_tos(),
				make_tdb_data(id_buf,
					sizeof(id_buf)));
	if (rec == NULL) {
		DEBUG(0, ("store_acl_blob_fsp_tdb: fetch_lock failed\n"));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}
	data.dptr = pblob->data;
	data.dsize = pblob->length;
	return rec->store(rec, data, 0);
}

/*********************************************************************
 On unlink we need to delete the tdb record (if using tdb).
*********************************************************************/

static int unlink_acl_tdb(vfs_handle_struct *handle,
			  const struct smb_filename *smb_fname)
{
	struct smb_filename *smb_fname_tmp = NULL;
	struct db_context *db = acl_db;
	NTSTATUS status;
	int ret = -1;

	status = copy_smb_filename(talloc_tos(), smb_fname, &smb_fname_tmp);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		goto out;
	}

	if (lp_posix_pathnames()) {
		ret = SMB_VFS_LSTAT(handle->conn, smb_fname_tmp);
	} else {
		ret = SMB_VFS_STAT(handle->conn, smb_fname_tmp);
	}

	if (ret == -1) {
		goto out;
	}

	ret = unlink_acl_common(handle, smb_fname_tmp);

	if (ret == -1) {
		goto out;
	}

	acl_tdb_delete(handle, db, &smb_fname_tmp->st);
 out:
	return ret;
}

/*********************************************************************
 On rmdir we need to delete the tdb record (if using tdb).
*********************************************************************/

static int rmdir_acl_tdb(vfs_handle_struct *handle, const char *path)
{

	SMB_STRUCT_STAT sbuf;
	struct db_context *db = acl_db;
	int ret = -1;

	if (lp_posix_pathnames()) {
		ret = vfs_lstat_smb_fname(handle->conn, path, &sbuf);
	} else {
		ret = vfs_stat_smb_fname(handle->conn, path, &sbuf);
	}

	if (ret == -1) {
		return -1;
	}

	ret = rmdir_acl_common(handle, path);
	if (ret == -1) {
		return -1;
	}

	acl_tdb_delete(handle, db, &sbuf);
	return 0;
}

/*******************************************************************
 Handle opening the storage tdb if so configured.
*******************************************************************/

static int connect_acl_tdb(struct vfs_handle_struct *handle,
				const char *service,
				const char *user)
{
	int ret = SMB_VFS_NEXT_CONNECT(handle, service, user);

	if (ret < 0) {
		return ret;
	}

	if (!acl_tdb_init()) {
		SMB_VFS_NEXT_DISCONNECT(handle);
		return -1;
	}

	/* Ensure we have the parameters correct if we're
	 * using this module. */
	DEBUG(2,("connect_acl_tdb: setting 'inherit acls = true' "
		"'dos filemode = true' and "
		"'force unknown acl user = true' for service %s\n",
		service ));

	lp_do_parameter(SNUM(handle->conn), "inherit acls", "true");
	lp_do_parameter(SNUM(handle->conn), "dos filemode", "true");
	lp_do_parameter(SNUM(handle->conn), "force unknown acl user", "true");

	return 0;
}

/*********************************************************************
 Remove a Windows ACL - we're setting the underlying POSIX ACL.
*********************************************************************/

static int sys_acl_set_file_tdb(vfs_handle_struct *handle,
                              const char *path,
                              SMB_ACL_TYPE_T type,
                              SMB_ACL_T theacl)
{
	SMB_STRUCT_STAT sbuf;
	struct db_context *db = acl_db;
	int ret = -1;

	if (lp_posix_pathnames()) {
		ret = vfs_lstat_smb_fname(handle->conn, path, &sbuf);
	} else {
		ret = vfs_stat_smb_fname(handle->conn, path, &sbuf);
	}

	if (ret == -1) {
		return -1;
	}

	ret = SMB_VFS_NEXT_SYS_ACL_SET_FILE(handle,
						path,
						type,
						theacl);
	if (ret == -1) {
		return -1;
	}

	acl_tdb_delete(handle, db, &sbuf);
	return 0;
}

/*********************************************************************
 Remove a Windows ACL - we're setting the underlying POSIX ACL.
*********************************************************************/

static int sys_acl_set_fd_tdb(vfs_handle_struct *handle,
                            files_struct *fsp,
                            SMB_ACL_T theacl)
{
	struct db_context *db = acl_db;
	NTSTATUS status;
	int ret;

	status = vfs_stat_fsp(fsp);
	if (!NT_STATUS_IS_OK(status)) {
		return -1;
	}

	ret = SMB_VFS_NEXT_SYS_ACL_SET_FD(handle,
						fsp,
						theacl);
	if (ret == -1) {
		return -1;
	}

	acl_tdb_delete(handle, db, &fsp->fsp_name->st);
	return 0;
}

static struct vfs_fn_pointers vfs_acl_tdb_fns = {
	.connect_fn = connect_acl_tdb,
	.disconnect = disconnect_acl_tdb,
	.opendir = opendir_acl_common,
	.mkdir = mkdir_acl_common,
	.rmdir = rmdir_acl_tdb,
	.open_fn = open_acl_common,
	.create_file = create_file_acl_common,
	.unlink = unlink_acl_tdb,
	.chmod = chmod_acl_module_common,
	.fchmod = fchmod_acl_module_common,
	.fget_nt_acl = fget_nt_acl_common,
	.get_nt_acl = get_nt_acl_common,
	.fset_nt_acl = fset_nt_acl_common,
	.chmod_acl = chmod_acl_acl_module_common,
	.fchmod_acl = fchmod_acl_acl_module_common,
	.sys_acl_set_file = sys_acl_set_file_tdb,
	.sys_acl_set_fd = sys_acl_set_fd_tdb
};

NTSTATUS vfs_acl_tdb_init(void)
{
	return smb_register_vfs(SMB_VFS_INTERFACE_VERSION, "acl_tdb",
				&vfs_acl_tdb_fns);
}
