/*
 * Unix SMB/CIFS implementation.
 * Support for OneFS
 *
 * Copyright (C) Steven Danneman, 2008
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

#ifndef _ONEFS_H
#define _ONEFS_H

/*
 * vfs interface handlers
 */
SMB_STRUCT_DIR *onefs_opendir(struct vfs_handle_struct *handle,
			      const char *fname, const char *mask,
			      uint32 attributes);

SMB_STRUCT_DIRENT *onefs_readdir(struct vfs_handle_struct *handle,
				 SMB_STRUCT_DIR *dirp, SMB_STRUCT_STAT *sbuf);

void onefs_seekdir(struct vfs_handle_struct *handle, SMB_STRUCT_DIR *dirp,
		   long offset);

long onefs_telldir(struct vfs_handle_struct *handle, SMB_STRUCT_DIR *dirp);

void onefs_rewinddir(struct vfs_handle_struct *handle, SMB_STRUCT_DIR *dirp);

int onefs_closedir(struct vfs_handle_struct *handle, SMB_STRUCT_DIR *dir);

void onefs_init_search_op(struct vfs_handle_struct *handle,
			  SMB_STRUCT_DIR *dirp);

NTSTATUS onefs_create_file(vfs_handle_struct *handle,
			   struct smb_request *req,
			   uint16_t root_dir_fid,
			   struct smb_filename *smb_fname,
			   uint32_t access_mask,
			   uint32_t share_access,
			   uint32_t create_disposition,
			   uint32_t create_options,
			   uint32_t file_attributes,
			   uint32_t oplock_request,
			   uint64_t allocation_size,
			   struct security_descriptor *sd,
			   struct ea_list *ea_list,
			   files_struct **result,
			   int *pinfo);

int onefs_close(vfs_handle_struct *handle, struct files_struct *fsp);

int onefs_rename(vfs_handle_struct *handle,
		 const struct smb_filename *smb_fname_src,
		 const struct smb_filename *smb_fname_dst);

int onefs_stat(vfs_handle_struct *handle, struct smb_filename *smb_fname);

int onefs_fstat(vfs_handle_struct *handle, struct files_struct *fsp,
		SMB_STRUCT_STAT *sbuf);

int onefs_lstat(vfs_handle_struct *handle, struct smb_filename *smb_fname);

int onefs_unlink(vfs_handle_struct *handle,
		 const struct smb_filename *smb_fname);

NTSTATUS onefs_streaminfo(vfs_handle_struct *handle,
			  struct files_struct *fsp,
			  const char *fname,
			  TALLOC_CTX *mem_ctx,
			  unsigned int *num_streams,
			  struct stream_struct **streams);

int onefs_vtimes_streams(vfs_handle_struct *handle,
			 const struct smb_filename *smb_fname,
			 int flags, struct timespec times[3]);

NTSTATUS onefs_brl_lock_windows(vfs_handle_struct *handle,
				struct byte_range_lock *br_lck,
				struct lock_struct *plock,
				bool blocking_lock,
				struct blocking_lock_record *blr);

bool onefs_brl_unlock_windows(vfs_handle_struct *handle,
			      struct messaging_context *msg_ctx,
			      struct byte_range_lock *br_lck,
			      const struct lock_struct *plock);

bool onefs_brl_cancel_windows(vfs_handle_struct *handle,
			      struct byte_range_lock *br_lck,
			      struct lock_struct *plock,
			      struct blocking_lock_record *blr);

bool onefs_strict_lock(vfs_handle_struct *handle,
			files_struct *fsp,
			struct lock_struct *plock);

void onefs_strict_unlock(vfs_handle_struct *handle,
			files_struct *fsp,
			struct lock_struct *plock);

NTSTATUS onefs_notify_watch(vfs_handle_struct *vfs_handle,
			    struct sys_notify_context *ctx,
			    struct notify_entry *e,
			    void (*callback)(struct sys_notify_context *ctx,
					void *private_data,
					struct notify_event *ev),
			    void *private_data,
			    void *handle_p);

NTSTATUS onefs_fget_nt_acl(vfs_handle_struct *handle, files_struct *fsp,
			   uint32 security_info, struct security_descriptor **ppdesc);

NTSTATUS onefs_get_nt_acl(vfs_handle_struct *handle, const char* name,
			  uint32 security_info, struct security_descriptor **ppdesc);

NTSTATUS onefs_fset_nt_acl(vfs_handle_struct *handle, files_struct *fsp,
			   uint32 security_info_sent,
			   const struct security_descriptor *psd);

/*
 * Utility functions
 */
struct ifs_security_descriptor;
NTSTATUS onefs_samba_sd_to_sd(uint32_t security_info_sent,
			      const struct security_descriptor *psd,
			      struct ifs_security_descriptor *sd, int snum,
			      uint32_t *security_info_effective);

NTSTATUS onefs_stream_prep_smb_fname(TALLOC_CTX *ctx,
				     const struct smb_filename *smb_fname_in,
				     struct smb_filename **smb_fname_out);

int onefs_rdp_add_dir_state(connection_struct *conn, SMB_STRUCT_DIR *dirp);

/*
 * System Interfaces
 */
int onefs_sys_create_file(connection_struct *conn,
			  int base_fd,
			  const char *path,
		          uint32_t access_mask,
		          uint32_t open_access_mask,
			  uint32_t share_access,
			  uint32_t create_options,
			  int flags,
			  mode_t mode,
			  int oplock_request,
			  uint64_t id,
			  struct security_descriptor *sd,
			  uint32_t ntfs_flags,
			  int *granted_oplock);

ssize_t onefs_sys_sendfile(connection_struct *conn, int tofd, int fromfd,
			   const DATA_BLOB *header, SMB_OFF_T offset,
			   size_t count);

ssize_t onefs_sys_recvfile(int fromfd, int tofd, SMB_OFF_T offset,
			   size_t count);

void init_stat_ex_from_onefs_stat(struct stat_ex *dst, const struct stat *src);

int onefs_sys_stat(const char *fname, SMB_STRUCT_STAT *sbuf);

int onefs_sys_fstat(int fd, SMB_STRUCT_STAT *sbuf);

int onefs_sys_fstat_at(int base_fd, const char *fname, SMB_STRUCT_STAT *sbuf,
		       int flags);

int onefs_sys_lstat(const char *fname, SMB_STRUCT_STAT *sbuf);



#endif /* _ONEFS_H */
