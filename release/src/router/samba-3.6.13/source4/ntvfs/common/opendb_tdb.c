/* 
   Unix SMB/CIFS implementation.

   Copyright (C) Andrew Tridgell 2004
   Copyright (C) Stefan Metzmacher 2008

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

/*
  this is the open files database, tdb backend. It implements shared
  storage of what files are open between server instances, and
  implements the rules of shared access to files.

  The caller needs to provide a file_key, which specifies what file
  they are talking about. This needs to be a unique key across all
  filesystems, and is usually implemented in terms of a device/inode
  pair.

  Before any operations can be performed the caller needs to establish
  a lock on the record associated with file_key. That is done by
  calling odb_lock(). The caller releases this lock by calling
  talloc_free() on the returned handle.

  All other operations on a record are done by passing the odb_lock()
  handle back to this module. The handle contains internal
  information about what file_key is being operated on.
*/

#include "includes.h"
#include "system/filesys.h"
#include <tdb.h>
#include "messaging/messaging.h"
#include "lib/util/tdb_wrap.h"
#include "lib/messaging/irpc.h"
#include "librpc/gen_ndr/ndr_opendb.h"
#include "ntvfs/ntvfs.h"
#include "ntvfs/common/ntvfs_common.h"
#include "cluster/cluster.h"
#include "param/param.h"
#include "ntvfs/sysdep/sys_lease.h"

struct odb_context {
	struct tdb_wrap *w;
	struct ntvfs_context *ntvfs_ctx;
	bool oplocks;
	struct sys_lease_context *lease_ctx;
};

/*
  an odb lock handle. You must obtain one of these using odb_lock() before doing
  any other operations. 
*/
struct odb_lock {
	struct odb_context *odb;
	TDB_DATA key;

	struct opendb_file file;

	struct {
		struct opendb_entry *e;
		bool attrs_only;
	} can_open;
};

static NTSTATUS odb_oplock_break_send(struct messaging_context *msg_ctx,
				      struct opendb_entry *e,
				      uint8_t level);

/*
  Open up the openfiles.tdb database. Close it down using
  talloc_free(). We need the messaging_ctx to allow for pending open
  notifications.
*/
static struct odb_context *odb_tdb_init(TALLOC_CTX *mem_ctx, 
					struct ntvfs_context *ntvfs_ctx)
{
	struct odb_context *odb;

	odb = talloc(mem_ctx, struct odb_context);
	if (odb == NULL) {
		return NULL;
	}

	odb->w = cluster_tdb_tmp_open(odb, ntvfs_ctx->lp_ctx, "openfiles.tdb", TDB_DEFAULT);
	if (odb->w == NULL) {
		talloc_free(odb);
		return NULL;
	}

	odb->ntvfs_ctx = ntvfs_ctx;

	odb->oplocks = share_bool_option(ntvfs_ctx->config, SHARE_OPLOCKS, SHARE_OPLOCKS_DEFAULT);

	odb->lease_ctx = sys_lease_context_create(ntvfs_ctx->config, odb,
						  ntvfs_ctx->event_ctx,
						  ntvfs_ctx->msg_ctx,
						  odb_oplock_break_send);

	return odb;
}

/*
  destroy a lock on the database
*/
static int odb_lock_destructor(struct odb_lock *lck)
{
	tdb_chainunlock(lck->odb->w->tdb, lck->key);
	return 0;
}

static NTSTATUS odb_pull_record(struct odb_lock *lck, struct opendb_file *file);

/*
  get a lock on a entry in the odb. This call returns a lock handle,
  which the caller should unlock using talloc_free().
*/
static struct odb_lock *odb_tdb_lock(TALLOC_CTX *mem_ctx,
				     struct odb_context *odb, DATA_BLOB *file_key)
{
	struct odb_lock *lck;
	NTSTATUS status;

	lck = talloc(mem_ctx, struct odb_lock);
	if (lck == NULL) {
		return NULL;
	}

	lck->odb = talloc_reference(lck, odb);
	lck->key.dptr = talloc_memdup(lck, file_key->data, file_key->length);
	lck->key.dsize = file_key->length;
	if (lck->key.dptr == NULL) {
		talloc_free(lck);
		return NULL;
	}

	if (tdb_chainlock(odb->w->tdb, lck->key) != 0) {
		talloc_free(lck);
		return NULL;
	}

	ZERO_STRUCT(lck->can_open);

	talloc_set_destructor(lck, odb_lock_destructor);

	status = odb_pull_record(lck, &lck->file);
	if (NT_STATUS_EQUAL(status, NT_STATUS_OBJECT_NAME_NOT_FOUND)) {
		/* initialise a blank structure */
		ZERO_STRUCT(lck->file);
	} else if (!NT_STATUS_IS_OK(status)) {
		talloc_free(lck);
		return NULL;
	}
	
	return lck;
}

static DATA_BLOB odb_tdb_get_key(TALLOC_CTX *mem_ctx, struct odb_lock *lck)
{
	return data_blob_talloc(mem_ctx, lck->key.dptr, lck->key.dsize);
}


/*
  determine if two odb_entry structures conflict

  return NT_STATUS_OK on no conflict
*/
static NTSTATUS share_conflict(struct opendb_entry *e1,
			       uint32_t stream_id,
			       uint32_t share_access,
			       uint32_t access_mask)
{
	/* if either open involves no read.write or delete access then
	   it can't conflict */
	if (!(e1->access_mask & (SEC_FILE_WRITE_DATA |
				 SEC_FILE_APPEND_DATA |
				 SEC_FILE_READ_DATA |
				 SEC_FILE_EXECUTE |
				 SEC_STD_DELETE))) {
		return NT_STATUS_OK;
	}
	if (!(access_mask & (SEC_FILE_WRITE_DATA |
			     SEC_FILE_APPEND_DATA |
			     SEC_FILE_READ_DATA |
			     SEC_FILE_EXECUTE |
			     SEC_STD_DELETE))) {
		return NT_STATUS_OK;
	}

	/* data IO access masks. This is skipped if the two open handles
	   are on different streams (as in that case the masks don't
	   interact) */
	if (e1->stream_id != stream_id) {
		return NT_STATUS_OK;
	}

#define CHECK_MASK(am, right, sa, share) \
	if (((am) & (right)) && !((sa) & (share))) return NT_STATUS_SHARING_VIOLATION

	CHECK_MASK(e1->access_mask, SEC_FILE_WRITE_DATA | SEC_FILE_APPEND_DATA,
		   share_access, NTCREATEX_SHARE_ACCESS_WRITE);
	CHECK_MASK(access_mask, SEC_FILE_WRITE_DATA | SEC_FILE_APPEND_DATA,
		   e1->share_access, NTCREATEX_SHARE_ACCESS_WRITE);
	
	CHECK_MASK(e1->access_mask, SEC_FILE_READ_DATA | SEC_FILE_EXECUTE,
		   share_access, NTCREATEX_SHARE_ACCESS_READ);
	CHECK_MASK(access_mask, SEC_FILE_READ_DATA | SEC_FILE_EXECUTE,
		   e1->share_access, NTCREATEX_SHARE_ACCESS_READ);

	CHECK_MASK(e1->access_mask, SEC_STD_DELETE,
		   share_access, NTCREATEX_SHARE_ACCESS_DELETE);
	CHECK_MASK(access_mask, SEC_STD_DELETE,
		   e1->share_access, NTCREATEX_SHARE_ACCESS_DELETE);
#undef CHECK_MASK
	return NT_STATUS_OK;
}

/*
  pull a record, translating from the db format to the opendb_file structure defined
  in opendb.idl
*/
static NTSTATUS odb_pull_record(struct odb_lock *lck, struct opendb_file *file)
{
	struct odb_context *odb = lck->odb;
	TDB_DATA dbuf;
	DATA_BLOB blob;
	enum ndr_err_code ndr_err;

	dbuf = tdb_fetch(odb->w->tdb, lck->key);
	if (dbuf.dptr == NULL) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	blob.data = dbuf.dptr;
	blob.length = dbuf.dsize;

	ndr_err = ndr_pull_struct_blob(&blob, lck, file, (ndr_pull_flags_fn_t)ndr_pull_opendb_file);
	free(dbuf.dptr);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return ndr_map_error2ntstatus(ndr_err);
	}

	return NT_STATUS_OK;
}

/*
  push a record, translating from the opendb_file structure defined in opendb.idl
*/
static NTSTATUS odb_push_record(struct odb_lock *lck, struct opendb_file *file)
{
	struct odb_context *odb = lck->odb;
	TDB_DATA dbuf;
	DATA_BLOB blob;
	enum ndr_err_code ndr_err;
	int ret;

	if (file->num_entries == 0) {
		ret = tdb_delete(odb->w->tdb, lck->key);
		if (ret != 0) {
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}
		return NT_STATUS_OK;
	}

	ndr_err = ndr_push_struct_blob(&blob, lck, file, (ndr_push_flags_fn_t)ndr_push_opendb_file);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return ndr_map_error2ntstatus(ndr_err);
	}

	dbuf.dptr = blob.data;
	dbuf.dsize = blob.length;
		
	ret = tdb_store(odb->w->tdb, lck->key, dbuf, TDB_REPLACE);
	data_blob_free(&blob);
	if (ret != 0) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	return NT_STATUS_OK;
}

/*
  send an oplock break to a client
*/
static NTSTATUS odb_oplock_break_send(struct messaging_context *msg_ctx,
				      struct opendb_entry *e,
				      uint8_t level)
{
	NTSTATUS status;
	struct opendb_oplock_break op_break;
	DATA_BLOB blob;

	ZERO_STRUCT(op_break);

	/* tell the server handling this open file about the need to send the client
	   a break */
	op_break.file_handle	= e->file_handle;
	op_break.level		= level;

	blob = data_blob_const(&op_break, sizeof(op_break));

	status = messaging_send(msg_ctx, e->server,
				MSG_NTVFS_OPLOCK_BREAK, &blob);
	NT_STATUS_NOT_OK_RETURN(status);

	return NT_STATUS_OK;
}

static bool access_attributes_only(uint32_t access_mask,
				   uint32_t open_disposition,
				   bool break_to_none)
{
	switch (open_disposition) {
	case NTCREATEX_DISP_SUPERSEDE:
	case NTCREATEX_DISP_OVERWRITE_IF:
	case NTCREATEX_DISP_OVERWRITE:
		return false;
	default:
		break;
	}

	if (break_to_none) {
		return false;
	}

#define CHECK_MASK(m,g) ((m) && (((m) & ~(g))==0) && (((m) & (g)) != 0))
	return CHECK_MASK(access_mask,
			  SEC_STD_SYNCHRONIZE |
			  SEC_FILE_READ_ATTRIBUTE |
			  SEC_FILE_WRITE_ATTRIBUTE);
#undef CHECK_MASK
}

static NTSTATUS odb_tdb_open_can_internal(struct odb_context *odb,
					  const struct opendb_file *file,
					  uint32_t stream_id, uint32_t share_access,
					  uint32_t access_mask, bool delete_on_close,
					  uint32_t open_disposition, bool break_to_none,
					  bool *_attrs_only)
{
	NTSTATUS status;
	uint32_t i;
	bool attrs_only = false;

	/* see if anyone has an oplock, which we need to break */
	for (i=0;i<file->num_entries;i++) {
		if (file->entries[i].oplock_level == OPLOCK_BATCH) {
			bool oplock_return = OPLOCK_BREAK_TO_LEVEL_II;
			/* if this is an attribute only access
			 * it doesn't conflict with a BACTCH oplock
			 * but we'll not grant the oplock below
			 */
			attrs_only = access_attributes_only(access_mask,
							    open_disposition,
							    break_to_none);
			if (attrs_only) {
				break;
			}
			/* a batch oplock caches close calls, which
			   means the client application might have
			   already closed the file. We have to allow
			   this close to propogate by sending a oplock
			   break request and suspending this call
			   until the break is acknowledged or the file
			   is closed */
			if (break_to_none ||
			    !file->entries[i].allow_level_II_oplock) {
				oplock_return = OPLOCK_BREAK_TO_NONE;
			}
			odb_oplock_break_send(odb->ntvfs_ctx->msg_ctx,
					      &file->entries[i],
					      oplock_return);
			return NT_STATUS_OPLOCK_NOT_GRANTED;
		}
	}

	if (file->delete_on_close) {
		/* while delete on close is set, no new opens are allowed */
		return NT_STATUS_DELETE_PENDING;
	}

	if (file->num_entries != 0 && delete_on_close) {
		return NT_STATUS_SHARING_VIOLATION;
	}

	/* check for sharing violations */
	for (i=0;i<file->num_entries;i++) {
		status = share_conflict(&file->entries[i], stream_id,
					share_access, access_mask);
		NT_STATUS_NOT_OK_RETURN(status);
	}

	/* we now know the open could succeed, but we need to check
	   for any exclusive oplocks. We can't grant a second open
	   till these are broken. Note that we check for batch oplocks
	   before checking for sharing violations, and check for
	   exclusive oplocks afterwards. */
	for (i=0;i<file->num_entries;i++) {
		if (file->entries[i].oplock_level == OPLOCK_EXCLUSIVE) {
			bool oplock_return = OPLOCK_BREAK_TO_LEVEL_II;
			/* if this is an attribute only access
			 * it doesn't conflict with an EXCLUSIVE oplock
			 * but we'll not grant the oplock below
			 */
			attrs_only = access_attributes_only(access_mask,
							    open_disposition,
							    break_to_none);
			if (attrs_only) {
				break;
			}
			/*
			 * send an oplock break to the holder of the
			 * oplock and tell caller to retry later
			 */
			if (break_to_none ||
			    !file->entries[i].allow_level_II_oplock) {
				oplock_return = OPLOCK_BREAK_TO_NONE;
			}
			odb_oplock_break_send(odb->ntvfs_ctx->msg_ctx,
					      &file->entries[i],
					      oplock_return);
			return NT_STATUS_OPLOCK_NOT_GRANTED;
		}
	}

	if (_attrs_only) {
		*_attrs_only = attrs_only;
	}
	return NT_STATUS_OK;
}

/*
  register an open file in the open files database.
  The share_access rules are implemented by odb_can_open()
  and it's needed to call odb_can_open() before
  odb_open_file() otherwise NT_STATUS_INTERNAL_ERROR is returned

  Note that the path is only used by the delete on close logic, not
  for comparing with other filenames
*/
static NTSTATUS odb_tdb_open_file(struct odb_lock *lck,
				  void *file_handle, const char *path,
				  int *fd, NTTIME open_write_time,
				  bool allow_level_II_oplock,
				  uint32_t oplock_level, uint32_t *oplock_granted)
{
	struct odb_context *odb = lck->odb;

	if (!lck->can_open.e) {
		return NT_STATUS_INTERNAL_ERROR;
	}

	if (odb->oplocks == false) {
		oplock_level = OPLOCK_NONE;
	}

	if (!oplock_granted) {
		oplock_level = OPLOCK_NONE;
	}

	if (lck->file.path == NULL) {
		lck->file.path = talloc_strdup(lck, path);
		NT_STATUS_HAVE_NO_MEMORY(lck->file.path);
	}

	if (lck->file.open_write_time == 0) {
		lck->file.open_write_time = open_write_time;
	}

	/*
	  possibly grant an exclusive, batch or level2 oplock
	*/
	if (lck->can_open.attrs_only) {
		oplock_level	= OPLOCK_NONE;
	} else if (oplock_level == OPLOCK_EXCLUSIVE) {
		if (lck->file.num_entries == 0) {
			oplock_level	= OPLOCK_EXCLUSIVE;
		} else if (allow_level_II_oplock) {
			oplock_level	= OPLOCK_LEVEL_II;
		} else {
			oplock_level	= OPLOCK_NONE;
		}
	} else if (oplock_level == OPLOCK_BATCH) {
		if (lck->file.num_entries == 0) {
			oplock_level	= OPLOCK_BATCH;
		} else if (allow_level_II_oplock) {
			oplock_level	= OPLOCK_LEVEL_II;
		} else {
			oplock_level	= OPLOCK_NONE;
		}
	} else if (oplock_level == OPLOCK_LEVEL_II) {
		oplock_level	= OPLOCK_LEVEL_II;
	} else {
		oplock_level	= OPLOCK_NONE;
	}

	lck->can_open.e->file_handle		= file_handle;
	lck->can_open.e->fd			= fd;
	lck->can_open.e->allow_level_II_oplock	= allow_level_II_oplock;
	lck->can_open.e->oplock_level		= oplock_level;

	if (odb->lease_ctx && fd) {
		NTSTATUS status;
		status = sys_lease_setup(odb->lease_ctx, lck->can_open.e);
		NT_STATUS_NOT_OK_RETURN(status);
	}

	if (oplock_granted) {
		if (lck->can_open.e->oplock_level == OPLOCK_EXCLUSIVE) {
			*oplock_granted	= EXCLUSIVE_OPLOCK_RETURN;
		} else if (lck->can_open.e->oplock_level == OPLOCK_BATCH) {
			*oplock_granted	= BATCH_OPLOCK_RETURN;
		} else if (lck->can_open.e->oplock_level == OPLOCK_LEVEL_II) {
			*oplock_granted	= LEVEL_II_OPLOCK_RETURN;
		} else {
			*oplock_granted	= NO_OPLOCK_RETURN;
		}
	}

	/* it doesn't conflict, so add it to the end */
	lck->file.entries = talloc_realloc(lck, lck->file.entries,
					   struct opendb_entry,
					   lck->file.num_entries+1);
	NT_STATUS_HAVE_NO_MEMORY(lck->file.entries);

	lck->file.entries[lck->file.num_entries] = *lck->can_open.e;
	lck->file.num_entries++;

	talloc_free(lck->can_open.e);
	lck->can_open.e = NULL;

	return odb_push_record(lck, &lck->file);
}


/*
  register a pending open file in the open files database
*/
static NTSTATUS odb_tdb_open_file_pending(struct odb_lock *lck, void *private_data)
{
	struct odb_context *odb = lck->odb;

	if (lck->file.path == NULL) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	lck->file.pending = talloc_realloc(lck, lck->file.pending,
					   struct opendb_pending,
					   lck->file.num_pending+1);
	NT_STATUS_HAVE_NO_MEMORY(lck->file.pending);

	lck->file.pending[lck->file.num_pending].server = odb->ntvfs_ctx->server_id;
	lck->file.pending[lck->file.num_pending].notify_ptr = private_data;

	lck->file.num_pending++;

	return odb_push_record(lck, &lck->file);
}


/*
  remove a opendb entry
*/
static NTSTATUS odb_tdb_close_file(struct odb_lock *lck, void *file_handle,
				   const char **_delete_path)
{
	struct odb_context *odb = lck->odb;
	const char *delete_path = NULL;
	int i;

	if (lck->file.path == NULL) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	/* find the entry, and delete it */
	for (i=0;i<lck->file.num_entries;i++) {
		if (file_handle == lck->file.entries[i].file_handle &&
		    cluster_id_equal(&odb->ntvfs_ctx->server_id, &lck->file.entries[i].server)) {
			if (lck->file.entries[i].delete_on_close) {
				lck->file.delete_on_close = true;
			}
			if (odb->lease_ctx && lck->file.entries[i].fd) {
				NTSTATUS status;
				status = sys_lease_remove(odb->lease_ctx, &lck->file.entries[i]);
				NT_STATUS_NOT_OK_RETURN(status);
			}
			if (i < lck->file.num_entries-1) {
				memmove(lck->file.entries+i, lck->file.entries+i+1,
					(lck->file.num_entries - (i+1)) *
					sizeof(struct opendb_entry));
			}
			break;
		}
	}

	if (i == lck->file.num_entries) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	/* send any pending notifications, removing them once sent */
	for (i=0;i<lck->file.num_pending;i++) {
		messaging_send_ptr(odb->ntvfs_ctx->msg_ctx,
				   lck->file.pending[i].server,
				   MSG_PVFS_RETRY_OPEN,
				   lck->file.pending[i].notify_ptr);
	}
	lck->file.num_pending = 0;

	lck->file.num_entries--;

	if (lck->file.num_entries == 0 && lck->file.delete_on_close) {
		delete_path = lck->file.path;
	}

	if (_delete_path) {
		*_delete_path = delete_path;
	}

	return odb_push_record(lck, &lck->file);
}

/*
  update the oplock level of the client
*/
static NTSTATUS odb_tdb_update_oplock(struct odb_lock *lck, void *file_handle,
				      uint32_t oplock_level)
{
	struct odb_context *odb = lck->odb;
	int i;

	if (lck->file.path == NULL) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	/* find the entry, and update it */
	for (i=0;i<lck->file.num_entries;i++) {
		if (file_handle == lck->file.entries[i].file_handle &&
		    cluster_id_equal(&odb->ntvfs_ctx->server_id, &lck->file.entries[i].server)) {
			lck->file.entries[i].oplock_level = oplock_level;

			if (odb->lease_ctx && lck->file.entries[i].fd) {
				NTSTATUS status;
				status = sys_lease_update(odb->lease_ctx, &lck->file.entries[i]);
				NT_STATUS_NOT_OK_RETURN(status);
			}

			break;
		}
	}

	if (i == lck->file.num_entries) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	/* send any pending notifications, removing them once sent */
	for (i=0;i<lck->file.num_pending;i++) {
		messaging_send_ptr(odb->ntvfs_ctx->msg_ctx,
				   lck->file.pending[i].server,
				   MSG_PVFS_RETRY_OPEN,
				   lck->file.pending[i].notify_ptr);
	}
	lck->file.num_pending = 0;

	return odb_push_record(lck, &lck->file);
}

/*
  send oplocks breaks to none to all level2 holders
*/
static NTSTATUS odb_tdb_break_oplocks(struct odb_lock *lck)
{
	struct odb_context *odb = lck->odb;
	int i;
	bool modified = false;

	/* see if anyone has an oplock, which we need to break */
	for (i=0;i<lck->file.num_entries;i++) {
		if (lck->file.entries[i].oplock_level == OPLOCK_LEVEL_II) {
			/*
			 * there could be multiple level2 oplocks
			 * and we just send a break to none to all of them
			 * without waiting for a release
			 */
			odb_oplock_break_send(odb->ntvfs_ctx->msg_ctx,
					      &lck->file.entries[i],
					      OPLOCK_BREAK_TO_NONE);
			lck->file.entries[i].oplock_level = OPLOCK_NONE;
			modified = true;
		}
	}

	if (modified) {
		return odb_push_record(lck, &lck->file);
	}
	return NT_STATUS_OK;
}

/*
  remove a pending opendb entry
*/
static NTSTATUS odb_tdb_remove_pending(struct odb_lock *lck, void *private_data)
{
	struct odb_context *odb = lck->odb;
	int i;

	if (lck->file.path == NULL) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	/* find the entry, and delete it */
	for (i=0;i<lck->file.num_pending;i++) {
		if (private_data == lck->file.pending[i].notify_ptr &&
		    cluster_id_equal(&odb->ntvfs_ctx->server_id, &lck->file.pending[i].server)) {
			if (i < lck->file.num_pending-1) {
				memmove(lck->file.pending+i, lck->file.pending+i+1,
					(lck->file.num_pending - (i+1)) *
					sizeof(struct opendb_pending));
			}
			break;
		}
	}

	if (i == lck->file.num_pending) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	lck->file.num_pending--;
	
	return odb_push_record(lck, &lck->file);
}


/*
  rename the path in a open file
*/
static NTSTATUS odb_tdb_rename(struct odb_lock *lck, const char *path)
{
	if (lck->file.path == NULL) {
		/* not having the record at all is OK */
		return NT_STATUS_OK;
	}

	lck->file.path = talloc_strdup(lck, path);
	NT_STATUS_HAVE_NO_MEMORY(lck->file.path);

	return odb_push_record(lck, &lck->file);
}

/*
  get the path of an open file
*/
static NTSTATUS odb_tdb_get_path(struct odb_lock *lck, const char **path)
{
	*path = NULL;

	/* we don't ignore NT_STATUS_OBJECT_NAME_NOT_FOUND here */
	if (lck->file.path == NULL) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	*path = lck->file.path;

	return NT_STATUS_OK;
}

/*
  update delete on close flag on an open file
*/
static NTSTATUS odb_tdb_set_delete_on_close(struct odb_lock *lck, bool del_on_close)
{
	if (lck->file.path == NULL) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	lck->file.delete_on_close = del_on_close;

	return odb_push_record(lck, &lck->file);
}

/*
  update the write time on an open file
*/
static NTSTATUS odb_tdb_set_write_time(struct odb_lock *lck,
				       NTTIME write_time, bool force)
{
	if (lck->file.path == NULL) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	if (lck->file.changed_write_time != 0 && !force) {
		return NT_STATUS_OK;
	}

	lck->file.changed_write_time = write_time;

	return odb_push_record(lck, &lck->file);
}

/*
  return the current value of the delete_on_close bit, and how many
  people still have the file open
*/
static NTSTATUS odb_tdb_get_file_infos(struct odb_context *odb, DATA_BLOB *key,
				       bool *del_on_close, NTTIME *write_time)
{
	struct odb_lock *lck;

	if (del_on_close) {
		*del_on_close = false;
	}
	if (write_time) {
		*write_time = 0;
	}

	lck = odb_lock(odb, odb, key);
	NT_STATUS_HAVE_NO_MEMORY(lck);

	if (del_on_close) {
		*del_on_close = lck->file.delete_on_close;
	}
	if (write_time) {
		if (lck->file.changed_write_time == 0) {
			*write_time = lck->file.open_write_time;
		} else {
			*write_time = lck->file.changed_write_time;
		}
	}

	talloc_free(lck);

	return NT_STATUS_OK;
}


/*
  determine if a file can be opened with the given share_access,
  create_options and access_mask
*/
static NTSTATUS odb_tdb_can_open(struct odb_lock *lck,
				 uint32_t stream_id, uint32_t share_access,
				 uint32_t access_mask, bool delete_on_close,
				 uint32_t open_disposition, bool break_to_none)
{
	struct odb_context *odb = lck->odb;
	NTSTATUS status;

	status = odb_tdb_open_can_internal(odb, &lck->file, stream_id,
					   share_access, access_mask,
					   delete_on_close, open_disposition,
					   break_to_none, &lck->can_open.attrs_only);
	NT_STATUS_NOT_OK_RETURN(status);

	lck->can_open.e	= talloc(lck, struct opendb_entry);
	NT_STATUS_HAVE_NO_MEMORY(lck->can_open.e);

	lck->can_open.e->server			= odb->ntvfs_ctx->server_id;
	lck->can_open.e->file_handle		= NULL;
	lck->can_open.e->fd			= NULL;
	lck->can_open.e->stream_id		= stream_id;
	lck->can_open.e->share_access		= share_access;
	lck->can_open.e->access_mask		= access_mask;
	lck->can_open.e->delete_on_close	= delete_on_close;
	lck->can_open.e->allow_level_II_oplock	= false;
	lck->can_open.e->oplock_level		= OPLOCK_NONE;

	return NT_STATUS_OK;
}


static const struct opendb_ops opendb_tdb_ops = {
	.odb_init                = odb_tdb_init,
	.odb_lock                = odb_tdb_lock,
	.odb_get_key             = odb_tdb_get_key,
	.odb_open_file           = odb_tdb_open_file,
	.odb_open_file_pending   = odb_tdb_open_file_pending,
	.odb_close_file          = odb_tdb_close_file,
	.odb_remove_pending      = odb_tdb_remove_pending,
	.odb_rename              = odb_tdb_rename,
	.odb_get_path            = odb_tdb_get_path,
	.odb_set_delete_on_close = odb_tdb_set_delete_on_close,
	.odb_set_write_time      = odb_tdb_set_write_time,
	.odb_get_file_infos      = odb_tdb_get_file_infos,
	.odb_can_open            = odb_tdb_can_open,
	.odb_update_oplock       = odb_tdb_update_oplock,
	.odb_break_oplocks       = odb_tdb_break_oplocks
};


void odb_tdb_init_ops(void)
{
	sys_lease_init();
	odb_set_ops(&opendb_tdb_ops);
}
