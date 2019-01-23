/* 
   Unix SMB/CIFS implementation.

   open database code - common include

   Copyright (C) Andrew Tridgell 2007
   
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

struct opendb_ops {
	struct odb_context *(*odb_init)(TALLOC_CTX *mem_ctx, 
					struct ntvfs_context *ntvfs_ctx);
	struct odb_lock *(*odb_lock)(TALLOC_CTX *mem_ctx,
				     struct odb_context *odb, DATA_BLOB *file_key);
	DATA_BLOB (*odb_get_key)(TALLOC_CTX *mem_ctx, struct odb_lock *lck);
	NTSTATUS (*odb_open_file)(struct odb_lock *lck,
				  void *file_handle, const char *path,
				  int *fd, NTTIME open_write_time,
				  bool allow_level_II_oplock,
				  uint32_t oplock_level, uint32_t *oplock_granted);
	NTSTATUS (*odb_open_file_pending)(struct odb_lock *lck, void *private_data);
	NTSTATUS (*odb_close_file)(struct odb_lock *lck, void *file_handle,
				   const char **delete_path);
	NTSTATUS (*odb_remove_pending)(struct odb_lock *lck, void *private_data);
	NTSTATUS (*odb_rename)(struct odb_lock *lck, const char *path);
	NTSTATUS (*odb_get_path)(struct odb_lock *lck, const char **path);
	NTSTATUS (*odb_set_delete_on_close)(struct odb_lock *lck, bool del_on_close);
	NTSTATUS (*odb_set_write_time)(struct odb_lock *lck,
				       NTTIME write_time, bool force);
	NTSTATUS (*odb_get_file_infos)(struct odb_context *odb, DATA_BLOB *key,
				       bool *del_on_close, NTTIME *write_time);
	NTSTATUS (*odb_can_open)(struct odb_lock *lck,
				 uint32_t stream_id, uint32_t share_access,
				 uint32_t access_mask, bool delete_on_close,
				 uint32_t open_disposition, bool break_to_none);
	NTSTATUS (*odb_update_oplock)(struct odb_lock *lck, void *file_handle,
				      uint32_t oplock_level);
	NTSTATUS (*odb_break_oplocks)(struct odb_lock *lck);
};

struct opendb_oplock_break {
	void *file_handle;
	uint8_t level;
};

void odb_set_ops(const struct opendb_ops *new_ops);
void odb_tdb_init_ops(void);
void odb_ctdb_init_ops(void);
