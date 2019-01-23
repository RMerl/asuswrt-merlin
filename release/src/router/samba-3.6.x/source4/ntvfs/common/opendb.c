/* 
   Unix SMB/CIFS implementation.

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

/*
  this is the open files database. It implements shared storage of
  what files are open between server instances, and implements the rules
  of shared access to files.

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
#include "ntvfs/ntvfs.h"
#include "ntvfs/common/ntvfs_common.h"
#include "cluster/cluster.h"
#include "param/param.h"

static const struct opendb_ops *ops;

/*
  set the odb backend ops
*/
void odb_set_ops(const struct opendb_ops *new_ops)
{
	ops = new_ops;
}

/*
  Open up the openfiles.tdb database. Close it down using
  talloc_free(). We need the messaging_ctx to allow for pending open
  notifications.
*/
struct odb_context *odb_init(TALLOC_CTX *mem_ctx, 
				      struct ntvfs_context *ntvfs_ctx)
{
	if (ops == NULL) {
		odb_tdb_init_ops();
	}
	return ops->odb_init(mem_ctx, ntvfs_ctx);
}

/*
  get a lock on a entry in the odb. This call returns a lock handle,
  which the caller should unlock using talloc_free().
*/
struct odb_lock *odb_lock(TALLOC_CTX *mem_ctx,
				   struct odb_context *odb, DATA_BLOB *file_key)
{
	return ops->odb_lock(mem_ctx, odb, file_key);
}

DATA_BLOB odb_get_key(TALLOC_CTX *mem_ctx, struct odb_lock *lck)
{
	return ops->odb_get_key(mem_ctx, lck);
}

/*
  register an open file in the open files database.
  The share_access rules are implemented by odb_can_open()
  and it's needed to call odb_can_open() before
  odb_open_file() otherwise NT_STATUS_INTERNAL_ERROR is returned

  Note that the path is only used by the delete on close logic, not
  for comparing with other filenames
*/
NTSTATUS odb_open_file(struct odb_lock *lck,
				void *file_handle, const char *path,
				int *fd, NTTIME open_write_time,
				bool allow_level_II_oplock,
				uint32_t oplock_level, uint32_t *oplock_granted)
{
	return ops->odb_open_file(lck, file_handle, path,
				  fd, open_write_time,
				  allow_level_II_oplock,
				  oplock_level, oplock_granted);
}


/*
  register a pending open file in the open files database
*/
NTSTATUS odb_open_file_pending(struct odb_lock *lck, void *private_data)
{
	return ops->odb_open_file_pending(lck, private_data);
}


/*
  remove a opendb entry
*/
NTSTATUS odb_close_file(struct odb_lock *lck, void *file_handle,
				 const char **delete_path)
{
	return ops->odb_close_file(lck, file_handle, delete_path);
}


/*
  remove a pending opendb entry
*/
NTSTATUS odb_remove_pending(struct odb_lock *lck, void *private_data)
{
	return ops->odb_remove_pending(lck, private_data);
}


/*
  rename the path in a open file
*/
NTSTATUS odb_rename(struct odb_lock *lck, const char *path)
{
	return ops->odb_rename(lck, path);
}

/*
  get back the path of an open file
*/
NTSTATUS odb_get_path(struct odb_lock *lck, const char **path)
{
	return ops->odb_get_path(lck, path);
}

/*
  update delete on close flag on an open file
*/
NTSTATUS odb_set_delete_on_close(struct odb_lock *lck, bool del_on_close)
{
	return ops->odb_set_delete_on_close(lck, del_on_close);
}

/*
  update the write time on an open file
*/
NTSTATUS odb_set_write_time(struct odb_lock *lck,
			    NTTIME write_time, bool force)
{
	return ops->odb_set_write_time(lck, write_time, force);
}

/*
  return the current value of the delete_on_close bit,
  and the current write time.
*/
NTSTATUS odb_get_file_infos(struct odb_context *odb, DATA_BLOB *key,
			    bool *del_on_close, NTTIME *write_time)
{
	return ops->odb_get_file_infos(odb, key, del_on_close, write_time);
}

/*
  determine if a file can be opened with the given share_access,
  create_options and access_mask
*/
NTSTATUS odb_can_open(struct odb_lock *lck,
			       uint32_t stream_id, uint32_t share_access,
			       uint32_t access_mask, bool delete_on_close,
			       uint32_t open_disposition, bool break_to_none)
{
	return ops->odb_can_open(lck, stream_id, share_access, access_mask,
				 delete_on_close, open_disposition, break_to_none);
}

NTSTATUS odb_update_oplock(struct odb_lock *lck, void *file_handle,
				    uint32_t oplock_level)
{
	return ops->odb_update_oplock(lck, file_handle, oplock_level);
}

NTSTATUS odb_break_oplocks(struct odb_lock *lck)
{
	return ops->odb_break_oplocks(lck);
}
