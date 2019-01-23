/* 
   Unix SMB/CIFS implementation.

   POSIX NTVFS backend - xattr support using a tdb

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
#include "lib/util/tdb_wrap.h"
#include <tdb.h>
#include "vfs_posix.h"

#define XATTR_LIST_ATTR ".xattr_list"

/*
  we need to maintain a list of attributes on each file, so that unlink
  can automatically clean them up
*/
static NTSTATUS xattr_tdb_add_list(struct tdb_wrap *ea_tdb, TALLOC_CTX *ctx, const char *attr_name,
				   const char *fname, int fd)
{
	DATA_BLOB blob;
	TALLOC_CTX *mem_ctx;
	const char *s;
	NTSTATUS status;
	size_t len;

	if (strcmp(attr_name, XATTR_LIST_ATTR) == 0) {
		return NT_STATUS_OK;
	}

	mem_ctx = talloc_new(ctx);

	status = pull_xattr_blob_tdb_raw(ea_tdb, mem_ctx, XATTR_LIST_ATTR,
				     fname, fd, 100, &blob);
	if (!NT_STATUS_IS_OK(status)) {
		blob = data_blob(NULL, 0);
	}

	for (s=(const char *)blob.data; s < (const char *)(blob.data+blob.length); s += strlen(s) + 1) {
		if (strcmp(attr_name, s) == 0) {
			talloc_free(mem_ctx);
			return NT_STATUS_OK;
		}
	}

	len = strlen(attr_name) + 1;

	blob.data = talloc_realloc(mem_ctx, blob.data, uint8_t, blob.length + len);
	if (blob.data == NULL) {
		talloc_free(mem_ctx);
		return NT_STATUS_NO_MEMORY;		
	}
	memcpy(blob.data + blob.length, attr_name, len);
	blob.length += len;

	status = push_xattr_blob_tdb_raw(ea_tdb,ctx, XATTR_LIST_ATTR, fname, fd, &blob);
	talloc_free(mem_ctx);

	return status;
}

/*
  form a key for using in the ea_tdb
*/
static NTSTATUS get_ea_tdb_key(TALLOC_CTX *mem_ctx,
			      const char *attr_name,
			      const char *fname, int fd, 
			      TDB_DATA *key)
{
	struct stat st;
	size_t len = strlen(attr_name);

	if (fd == -1) {
		if (stat(fname, &st) == -1) {
			return NT_STATUS_NOT_FOUND;
		}
	} else {
		if (fstat(fd, &st) == -1) {
			return NT_STATUS_NOT_FOUND;
		}
	}

	key->dptr = talloc_array(mem_ctx, uint8_t, 16 + len);
	if (key->dptr == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	key->dsize = 16 + len;

	SBVAL(key->dptr, 0, st.st_dev);
	SBVAL(key->dptr, 8, st.st_ino);
	memcpy(key->dptr+16, attr_name, len);
	
	return NT_STATUS_OK;
}



/*
  pull a xattr as a blob, using the ea_tdb_context tdb
*/
NTSTATUS pull_xattr_blob_tdb_raw(struct tdb_wrap *ea_tdb,
			     TALLOC_CTX *mem_ctx,
			     const char *attr_name,
			     const char *fname,
			     int fd,
			     size_t estimated_size,
			     DATA_BLOB *blob)
{
	TDB_DATA tkey, tdata;
	NTSTATUS status;

	status = get_ea_tdb_key(mem_ctx, attr_name, fname, fd, &tkey);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	tdata = tdb_fetch(ea_tdb->tdb, tkey);
	if (tdata.dptr == NULL) {
		return NT_STATUS_NOT_FOUND;
	}

	*blob = data_blob_talloc(mem_ctx, tdata.dptr, tdata.dsize);
	free(tdata.dptr);
	if (blob->data == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	return NT_STATUS_OK;	
}

NTSTATUS pull_xattr_blob_tdb(struct pvfs_state *pvfs,
			     TALLOC_CTX *mem_ctx,
			     const char *attr_name,
			     const char *fname,
			     int fd,
			     size_t estimated_size,
			     DATA_BLOB *blob)
{
	return pull_xattr_blob_tdb_raw(pvfs->ea_db,mem_ctx,attr_name,fname,fd,estimated_size,blob);
}

/*
  push a xattr as a blob, using ea_tdb
*/
NTSTATUS push_xattr_blob_tdb_raw(struct tdb_wrap *ea_tdb,
			     TALLOC_CTX *mem_ctx,
			     const char *attr_name,
			     const char *fname,
			     int fd,
			     const DATA_BLOB *blob)
{
	TDB_DATA tkey, tdata;
	NTSTATUS status;

	status = get_ea_tdb_key(mem_ctx, attr_name, fname, fd, &tkey);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	
	tdata.dptr = blob->data;
	tdata.dsize = blob->length;

	if (tdb_chainlock(ea_tdb->tdb, tkey) != 0) {
		talloc_free(tkey.dptr);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	status = xattr_tdb_add_list(ea_tdb,mem_ctx, attr_name, fname, fd);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	if (tdb_store(ea_tdb->tdb, tkey, tdata, TDB_REPLACE) == -1) {
		status = NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

done:
	tdb_chainunlock(ea_tdb->tdb, tkey);
	talloc_free(tkey.dptr);
	return status;	
}
NTSTATUS push_xattr_blob_tdb(struct pvfs_state *pvfs,
			     const char *attr_name,
			     const char *fname,
			     int fd,
			     const DATA_BLOB *blob)
{
	return push_xattr_blob_tdb_raw(pvfs->ea_db,pvfs,attr_name,fname,fd,blob);
}


/*
  delete a xattr
*/
NTSTATUS delete_xattr_tdb(struct pvfs_state *pvfs, const char *attr_name, 
			  const char *fname, int fd)
{
	TDB_DATA tkey;
	NTSTATUS status;

	status = get_ea_tdb_key(NULL, attr_name, fname, fd, &tkey);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	
	if (tdb_delete(pvfs->ea_db->tdb, tkey) == -1) {
		talloc_free(tkey.dptr);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	talloc_free(tkey.dptr);
	return NT_STATUS_OK;	
}



/*
  delete all xattrs for a file
*/
NTSTATUS unlink_xattr_tdb(struct pvfs_state *pvfs, const char *fname)
{
	TALLOC_CTX *mem_ctx = talloc_new(pvfs);
	DATA_BLOB blob;
	const char *s;
	NTSTATUS status;

	status = pull_xattr_blob_tdb(pvfs, mem_ctx, XATTR_LIST_ATTR,
				     fname, -1, 100, &blob);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(mem_ctx);
		return NT_STATUS_OK;
	}

	for (s=(const char *)blob.data; s < (const char *)(blob.data+blob.length); s += strlen(s) + 1) {
		delete_xattr_tdb(pvfs, s, fname, -1);
	}

	status = delete_xattr_tdb(pvfs, XATTR_LIST_ATTR, fname, -1);
	talloc_free(mem_ctx);
	return status;
}
