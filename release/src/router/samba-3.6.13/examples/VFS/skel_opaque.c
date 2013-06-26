/* 
 * Skeleton VFS module.  Implements dummy versions of all VFS
 * functions.
 *
 * Copyright (C) Tim Potter, 1999-2000
 * Copyright (C) Alexander Bokovoy, 2002
 * Copyright (C) Stefan (metze) Metzmacher, 2003
 * Copyright (C) Jeremy Allison 2009
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
#include "smbd/proto.h"

/* PLEASE,PLEASE READ THE VFS MODULES CHAPTER OF THE 
   SAMBA DEVELOPERS GUIDE!!!!!!
 */

/* If you take this file as template for your module
 * you must re-implement every function.
 */

static int skel_connect(vfs_handle_struct *handle,  const char *service, const char *user)    
{
	errno = ENOSYS;
	return -1;
}

static void skel_disconnect(vfs_handle_struct *handle)
{
	;
}

static uint64_t skel_disk_free(vfs_handle_struct *handle,  const char *path,
	bool small_query, uint64_t *bsize,
	uint64_t *dfree, uint64_t *dsize)
{
	*bsize = 0;
	*dfree = 0;
	*dsize = 0;
	return 0;
}

static int skel_get_quota(vfs_handle_struct *handle,  enum SMB_QUOTA_TYPE qtype, unid_t id, SMB_DISK_QUOTA *dq)
{
	errno = ENOSYS;
	return -1;
}

static int skel_set_quota(vfs_handle_struct *handle,  enum SMB_QUOTA_TYPE qtype, unid_t id, SMB_DISK_QUOTA *dq)
{
	errno = ENOSYS;
	return -1;
}

static int skel_get_shadow_copy_data(vfs_handle_struct *handle, files_struct *fsp, struct shadow_copy_data *shadow_copy_data, bool labels)
{
	errno = ENOSYS;
	return -1;
}

static int skel_statvfs(struct vfs_handle_struct *handle, const char *path, struct vfs_statvfs_struct *statbuf)
{
	errno = ENOSYS;
	return -1;
}

static uint32_t skel_fs_capabilities(struct vfs_handle_struct *handle, enum timestamp_set_resolution *p_ts_res)
{
	return 0;
}

static SMB_STRUCT_DIR *skel_opendir(vfs_handle_struct *handle,  const char *fname, const char *mask, uint32 attr)
{
	return NULL;
}

static SMB_STRUCT_DIR *skel_fdopendir(vfs_handle_struct *handle, files_struct *fsp, const char *mask, uint32 attr)
{
	return NULL;
}

static SMB_STRUCT_DIRENT *skel_readdir(vfs_handle_struct *handle,
				       SMB_STRUCT_DIR *dirp,
				       SMB_STRUCT_STAT *sbuf)
{
	return NULL;
}

static void skel_seekdir(vfs_handle_struct *handle,  SMB_STRUCT_DIR *dirp, long offset)
{
	;
}

static long skel_telldir(vfs_handle_struct *handle,  SMB_STRUCT_DIR *dirp)
{
	return (long)-1;
}

static void skel_rewind_dir(vfs_handle_struct *handle, SMB_STRUCT_DIR *dirp)
{
	;
}

static int skel_mkdir(vfs_handle_struct *handle,  const char *path, mode_t mode)
{
	errno = ENOSYS;
	return -1;
}

static int skel_rmdir(vfs_handle_struct *handle,  const char *path)
{
	errno = ENOSYS;
	return -1;
}

static int skel_closedir(vfs_handle_struct *handle,  SMB_STRUCT_DIR *dir)
{
	errno = ENOSYS;
	return -1;
}

static void skel_init_search_op(struct vfs_handle_struct *handle, SMB_STRUCT_DIR *dirp)
{
	;
}

static int skel_open(vfs_handle_struct *handle, struct smb_filename *smb_fname,
		     files_struct *fsp, int flags, mode_t mode)
{
	errno = ENOSYS;
	return -1;
}

static NTSTATUS skel_create_file(struct vfs_handle_struct *handle,
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
				uint32_t private_flags,
                                struct security_descriptor *sd,
                                struct ea_list *ea_list,
                                files_struct **result,
                                int *pinfo)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

static int skel_close_fn(vfs_handle_struct *handle, files_struct *fsp)
{
	errno = ENOSYS;
	return -1;
}

static ssize_t skel_vfs_read(vfs_handle_struct *handle, files_struct *fsp, void *data, size_t n)
{
	errno = ENOSYS;
	return -1;
}

static ssize_t skel_pread(vfs_handle_struct *handle, files_struct *fsp, void *data, size_t n, SMB_OFF_T offset)
{
	errno = ENOSYS;
	return -1;
}

static ssize_t skel_write(vfs_handle_struct *handle, files_struct *fsp, const void *data, size_t n)
{
	errno = ENOSYS;
	return -1;
}

static ssize_t skel_pwrite(vfs_handle_struct *handle, files_struct *fsp, const void *data, size_t n, SMB_OFF_T offset)
{
	errno = ENOSYS;
	return -1;
}

static SMB_OFF_T skel_lseek(vfs_handle_struct *handle, files_struct *fsp, SMB_OFF_T offset, int whence)
{
	errno = ENOSYS;
	return (SMB_OFF_T)-1;
}

static ssize_t skel_sendfile(vfs_handle_struct *handle, int tofd, files_struct *fromfsp, const DATA_BLOB *hdr, SMB_OFF_T offset, size_t n)
{
	errno = ENOSYS;
	return -1;
}

static ssize_t skel_recvfile(vfs_handle_struct *handle, int fromfd, files_struct *tofsp, SMB_OFF_T offset, size_t n)
{
	errno = ENOSYS;
	return -1;
}

static int skel_rename(vfs_handle_struct *handle,
		       const struct smb_filename *smb_fname_src,
		       const struct smb_filename *smb_fname_dst)
{
	errno = ENOSYS;
	return -1;
}

static int skel_fsync(vfs_handle_struct *handle, files_struct *fsp)
{
	errno = ENOSYS;
	return -1;
}

static int skel_stat(vfs_handle_struct *handle, struct smb_filename *smb_fname)
{
	errno = ENOSYS;
	return -1;
}

static int skel_fstat(vfs_handle_struct *handle, files_struct *fsp, SMB_STRUCT_STAT *sbuf)
{
	errno = ENOSYS;
	return -1;
}

static int skel_lstat(vfs_handle_struct *handle, struct smb_filename *smb_fname)
{
	errno = ENOSYS;
	return -1;
}

static uint64_t skel_get_alloc_size(struct vfs_handle_struct *handle, struct files_struct *fsp, const SMB_STRUCT_STAT *sbuf)
{
	errno = ENOSYS;
	return -1;
}

static int skel_unlink(vfs_handle_struct *handle,
		       const struct smb_filename *smb_fname)
{
	errno = ENOSYS;
	return -1;
}

static int skel_chmod(vfs_handle_struct *handle,  const char *path, mode_t mode)
{
	errno = ENOSYS;
	return -1;
}

static int skel_fchmod(vfs_handle_struct *handle, files_struct *fsp, mode_t mode)
{
	errno = ENOSYS;
	return -1;
}

static int skel_chown(vfs_handle_struct *handle,  const char *path, uid_t uid, gid_t gid)
{
	errno = ENOSYS;
	return -1;
}

static int skel_fchown(vfs_handle_struct *handle, files_struct *fsp, uid_t uid, gid_t gid)
{
	errno = ENOSYS;
	return -1;
}

static int skel_lchown(vfs_handle_struct *handle,  const char *path, uid_t uid, gid_t gid)
{
	errno = ENOSYS;
	return -1;
}

static int skel_chdir(vfs_handle_struct *handle,  const char *path)
{
	errno = ENOSYS;
	return -1;
}

static char *skel_getwd(vfs_handle_struct *handle,  char *buf)
{
	errno = ENOSYS;
	return NULL;
}

static int skel_ntimes(vfs_handle_struct *handle,
		       const struct smb_filename *smb_fname,
		       struct smb_file_time *ft)
{
	errno = ENOSYS;
	return -1;
}

static int skel_ftruncate(vfs_handle_struct *handle, files_struct *fsp, SMB_OFF_T offset)
{
	errno = ENOSYS;
	return -1;
}

static int skel_fallocate(vfs_handle_struct *handle, files_struct *fsp,
			enum vfs_fallocate_mode mode,
			SMB_OFF_T offset, SMB_OFF_T len)
{
	errno = ENOSYS;
	return -1;
}

static bool skel_lock(vfs_handle_struct *handle, files_struct *fsp, int op, SMB_OFF_T offset, SMB_OFF_T count, int type)
{
	errno = ENOSYS;
	return false;
}

static int skel_kernel_flock(struct vfs_handle_struct *handle, struct files_struct *fsp, uint32 share_mode, uint32 access_mask)
{
	errno = ENOSYS;
	return -1;
}

static int skel_linux_setlease(struct vfs_handle_struct *handle, struct files_struct *fsp, int leasetype)
{
	errno = ENOSYS;
	return -1;
}

static bool skel_getlock(vfs_handle_struct *handle, files_struct *fsp, SMB_OFF_T *poffset, SMB_OFF_T *pcount, int *ptype, pid_t *ppid)
{
	errno = ENOSYS;
	return false;
}

static int skel_symlink(vfs_handle_struct *handle,  const char *oldpath, const char *newpath)
{
	errno = ENOSYS;
	return -1;
}

static int skel_vfs_readlink(vfs_handle_struct *handle, const char *path, char *buf, size_t bufsiz)
{
	errno = ENOSYS;
	return -1;
}

static int skel_link(vfs_handle_struct *handle,  const char *oldpath, const char *newpath)
{
	errno = ENOSYS;
	return -1;
}

static int skel_mknod(vfs_handle_struct *handle,  const char *path, mode_t mode, SMB_DEV_T dev)
{
	errno = ENOSYS;
	return -1;
}

static char *skel_realpath(vfs_handle_struct *handle,  const char *path)
{
	errno = ENOSYS;
	return NULL;
}

static NTSTATUS skel_notify_watch(struct vfs_handle_struct *handle,
	    struct sys_notify_context *ctx, struct notify_entry *e,
	    void (*callback)(struct sys_notify_context *ctx, void *private_data, struct notify_event *ev),
	    void *private_data, void *handle_p)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

static int skel_chflags(vfs_handle_struct *handle,  const char *path, uint flags)
{
	errno = ENOSYS;
	return -1;
}

static struct file_id skel_file_id_create(vfs_handle_struct *handle,
					  const SMB_STRUCT_STAT *sbuf)
{
	struct file_id id;
	ZERO_STRUCT(id);
	errno = ENOSYS;
	return id;
}

static NTSTATUS skel_streaminfo(struct vfs_handle_struct *handle,
				struct files_struct *fsp,
				const char *fname,
				TALLOC_CTX *mem_ctx,
				unsigned int *num_streams,
				struct stream_struct **streams)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

static int skel_get_real_filename(struct vfs_handle_struct *handle,
				const char *path,
				const char *name,
				TALLOC_CTX *mem_ctx,
				char **found_name)
{
	errno = ENOSYS;
	return -1;
}

static const char *skel_connectpath(struct vfs_handle_struct *handle,
				const char *filename)
{
	errno = ENOSYS;
	return NULL;
}

static NTSTATUS skel_brl_lock_windows(struct vfs_handle_struct *handle,
				struct byte_range_lock *br_lck,
				struct lock_struct *plock,
				bool blocking_lock,
				struct blocking_lock_record *blr)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

static bool skel_brl_unlock_windows(struct vfs_handle_struct *handle,
				struct messaging_context *msg_ctx,
				struct byte_range_lock *br_lck,
				const struct lock_struct *plock)
{
	errno = ENOSYS;
	return false;
}

static bool skel_brl_cancel_windows(struct vfs_handle_struct *handle,
				struct byte_range_lock *br_lck,
				struct lock_struct *plock,
				struct blocking_lock_record *blr)
{
	errno = ENOSYS;
	return false;
}

static bool skel_strict_lock(struct vfs_handle_struct *handle,
				struct files_struct *fsp,
				struct lock_struct *plock)
{
	errno = ENOSYS;
	return false;
}

static void skel_strict_unlock(struct vfs_handle_struct *handle,
				struct files_struct *fsp,
				struct lock_struct *plock)
{
	;
}

static NTSTATUS skel_translate_name(struct vfs_handle_struct *handle,
				const char *mapped_name,
				enum vfs_translate_direction direction,
				TALLOC_CTX *mem_ctx,
				char **pmapped_name)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS skel_fget_nt_acl(vfs_handle_struct *handle, files_struct *fsp,
	uint32 security_info, struct security_descriptor **ppdesc)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS skel_get_nt_acl(vfs_handle_struct *handle,
	const char *name, uint32 security_info, struct security_descriptor **ppdesc)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS skel_fset_nt_acl(vfs_handle_struct *handle, files_struct *fsp,
	uint32 security_info_sent, const struct security_descriptor *psd)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

static int skel_chmod_acl(vfs_handle_struct *handle,  const char *name, mode_t mode)
{
	errno = ENOSYS;
	return -1;
}

static int skel_fchmod_acl(vfs_handle_struct *handle, files_struct *fsp, mode_t mode)
{
	errno = ENOSYS;
	return -1;
}

static int skel_sys_acl_get_entry(vfs_handle_struct *handle,  SMB_ACL_T theacl, int entry_id, SMB_ACL_ENTRY_T *entry_p)
{
	errno = ENOSYS;
	return -1;
}

static int skel_sys_acl_get_tag_type(vfs_handle_struct *handle,  SMB_ACL_ENTRY_T entry_d, SMB_ACL_TAG_T *tag_type_p)
{
	errno = ENOSYS;
	return -1;
}

static int skel_sys_acl_get_permset(vfs_handle_struct *handle,  SMB_ACL_ENTRY_T entry_d, SMB_ACL_PERMSET_T *permset_p)
{
	errno = ENOSYS;
	return -1;
}

static void *skel_sys_acl_get_qualifier(vfs_handle_struct *handle,  SMB_ACL_ENTRY_T entry_d)
{
	errno = ENOSYS;
	return NULL;
}

static SMB_ACL_T skel_sys_acl_get_file(vfs_handle_struct *handle,  const char *path_p, SMB_ACL_TYPE_T type)
{
	errno = ENOSYS;
	return (SMB_ACL_T)NULL;
}

static SMB_ACL_T skel_sys_acl_get_fd(vfs_handle_struct *handle, files_struct *fsp)
{
	errno = ENOSYS;
	return (SMB_ACL_T)NULL;
}

static int skel_sys_acl_clear_perms(vfs_handle_struct *handle,  SMB_ACL_PERMSET_T permset)
{
	errno = ENOSYS;
	return -1;
}

static int skel_sys_acl_add_perm(vfs_handle_struct *handle,  SMB_ACL_PERMSET_T permset, SMB_ACL_PERM_T perm)
{
	errno = ENOSYS;
	return -1;
}

static char *skel_sys_acl_to_text(vfs_handle_struct *handle,  SMB_ACL_T theacl, ssize_t *plen)
{
	errno = ENOSYS;
	return NULL;
}

static SMB_ACL_T skel_sys_acl_init(vfs_handle_struct *handle,  int count)
{
	errno = ENOSYS;
	return (SMB_ACL_T)NULL;
}

static int skel_sys_acl_create_entry(vfs_handle_struct *handle,  SMB_ACL_T *pacl, SMB_ACL_ENTRY_T *pentry)
{
	errno = ENOSYS;
	return -1;
}

static int skel_sys_acl_set_tag_type(vfs_handle_struct *handle,  SMB_ACL_ENTRY_T entry, SMB_ACL_TAG_T tagtype)
{
	errno = ENOSYS;
	return -1;
}

static int skel_sys_acl_set_qualifier(vfs_handle_struct *handle,  SMB_ACL_ENTRY_T entry, void *qual)
{
	errno = ENOSYS;
	return -1;
}

static int skel_sys_acl_set_permset(vfs_handle_struct *handle,  SMB_ACL_ENTRY_T entry, SMB_ACL_PERMSET_T permset)
{
	errno = ENOSYS;
	return -1;
}

static int skel_sys_acl_valid(vfs_handle_struct *handle,  SMB_ACL_T theacl )
{
	errno = ENOSYS;
	return -1;
}

static int skel_sys_acl_set_file(vfs_handle_struct *handle,  const char *name, SMB_ACL_TYPE_T acltype, SMB_ACL_T theacl)
{
	errno = ENOSYS;
	return -1;
}

static int skel_sys_acl_set_fd(vfs_handle_struct *handle, files_struct *fsp, SMB_ACL_T theacl)
{
	errno = ENOSYS;
	return -1;
}

static int skel_sys_acl_delete_def_file(vfs_handle_struct *handle,  const char *path)
{
	errno = ENOSYS;
	return -1;
}

static int skel_sys_acl_get_perm(vfs_handle_struct *handle,  SMB_ACL_PERMSET_T permset, SMB_ACL_PERM_T perm)
{
	errno = ENOSYS;
	return -1;
}

static int skel_sys_acl_free_text(vfs_handle_struct *handle,  char *text)
{
	errno = ENOSYS;
	return -1;
}

static int skel_sys_acl_free_acl(vfs_handle_struct *handle,  SMB_ACL_T posix_acl)
{
	errno = ENOSYS;
	return -1;
}

static int skel_sys_acl_free_qualifier(vfs_handle_struct *handle,  void *qualifier, SMB_ACL_TAG_T tagtype)
{
	errno = ENOSYS;
	return -1;
}

static ssize_t skel_getxattr(vfs_handle_struct *handle, const char *path, const char *name, void *value, size_t size)
{
	errno = ENOSYS;
	return -1;
}

static ssize_t skel_lgetxattr(vfs_handle_struct *handle, const char *path, const char *name, void *value, size_t
size)
{
	errno = ENOSYS;
	return -1;
}

static ssize_t skel_fgetxattr(vfs_handle_struct *handle, struct files_struct *fsp, const char *name, void *value, size_t size)
{
	errno = ENOSYS;
	return -1;
}

static ssize_t skel_listxattr(vfs_handle_struct *handle, const char *path, char *list, size_t size)
{
	errno = ENOSYS;
	return -1;
}

static ssize_t skel_llistxattr(vfs_handle_struct *handle, const char *path, char *list, size_t size)
{
	errno = ENOSYS;
	return -1;
}

static ssize_t skel_flistxattr(vfs_handle_struct *handle, struct files_struct *fsp, char *list, size_t size)
{
	errno = ENOSYS;
	return -1;
}

static int skel_removexattr(vfs_handle_struct *handle, const char *path, const char *name)
{
	errno = ENOSYS;
	return -1;
}

static int skel_lremovexattr(vfs_handle_struct *handle, const char *path, const char *name)
{
	errno = ENOSYS;
	return -1;
}

static int skel_fremovexattr(vfs_handle_struct *handle, struct files_struct *fsp, const char *name)
{
	errno = ENOSYS;
	return -1;
        return SMB_VFS_NEXT_FREMOVEXATTR(handle, fsp, name);
}

static int skel_setxattr(vfs_handle_struct *handle, const char *path, const char *name, const void *value, size_t size, int flags)
{
	errno = ENOSYS;
	return -1;
}

static int skel_lsetxattr(vfs_handle_struct *handle, const char *path, const char *name, const void *value, size_t size, int flags)
{
	errno = ENOSYS;
	return -1;
}

static int skel_fsetxattr(vfs_handle_struct *handle, struct files_struct *fsp, const char *name, const void *value, size_t size, int flags)
{
	errno = ENOSYS;
	return -1;
}

static int skel_aio_read(struct vfs_handle_struct *handle, struct files_struct *fsp, SMB_STRUCT_AIOCB *aiocb)
{
	errno = ENOSYS;
	return -1;
}

static int skel_aio_write(struct vfs_handle_struct *handle, struct files_struct *fsp, SMB_STRUCT_AIOCB *aiocb)
{
	errno = ENOSYS;
	return -1;
}

static ssize_t skel_aio_return_fn(struct vfs_handle_struct *handle, struct files_struct *fsp, SMB_STRUCT_AIOCB *aiocb)
{
	errno = ENOSYS;
	return -1;
}

static int skel_aio_cancel(struct vfs_handle_struct *handle, struct files_struct *fsp, SMB_STRUCT_AIOCB *aiocb)
{
	errno = ENOSYS;
	return -1;
}

static int skel_aio_error_fn(struct vfs_handle_struct *handle, struct files_struct *fsp, SMB_STRUCT_AIOCB *aiocb)
{
	errno = ENOSYS;
	return -1;
}

static int skel_aio_fsync(struct vfs_handle_struct *handle, struct files_struct *fsp, int op, SMB_STRUCT_AIOCB *aiocb)
{
	errno = ENOSYS;
	return -1;
}

static int skel_aio_suspend(struct vfs_handle_struct *handle, struct files_struct *fsp, const SMB_STRUCT_AIOCB * const aiocb[], int n, const struct timespec *ts)
{
	errno = ENOSYS;
	return -1;
}

static bool skel_aio_force(struct vfs_handle_struct *handle, struct files_struct *fsp)
{
	errno = ENOSYS;
	return false;
}

static bool skel_is_offline(struct vfs_handle_struct *handle, const struct smb_filename *fname, SMB_STRUCT_STAT *sbuf)
{
	errno = ENOSYS;
	return false;
}

static int skel_set_offline(struct vfs_handle_struct *handle, const struct smb_filename *fname)
{
	errno = ENOSYS;
	return -1;
}

/* VFS operations structure */

struct vfs_fn_pointers skel_transparent_fns = {
	/* Disk operations */

	.connect_fn = skel_connect,
	.disconnect = skel_disconnect,
	.disk_free = skel_disk_free,
	.get_quota = skel_get_quota,
	.set_quota = skel_set_quota,
	.get_shadow_copy_data = skel_get_shadow_copy_data,
	.statvfs = skel_statvfs,
	.fs_capabilities = skel_fs_capabilities,

	/* Directory operations */

	.opendir = skel_opendir,
	.fdopendir = skel_fdopendir,
	.readdir = skel_readdir,
	.seekdir = skel_seekdir,
	.telldir = skel_telldir,
	.rewind_dir = skel_rewind_dir,
	.mkdir = skel_mkdir,
	.rmdir = skel_rmdir,
	.closedir = skel_closedir,
	.init_search_op = skel_init_search_op,

	/* File operations */

	.open_fn = skel_open,
	.create_file = skel_create_file,
	.close_fn = skel_close_fn,
	.vfs_read = skel_vfs_read,
	.pread = skel_pread,
	.write = skel_write,
	.pwrite = skel_pwrite,
	.lseek = skel_lseek,
	.sendfile = skel_sendfile,
	.recvfile = skel_recvfile,
	.rename = skel_rename,
	.fsync = skel_fsync,
	.stat = skel_stat,
	.fstat = skel_fstat,
	.lstat = skel_lstat,
	.get_alloc_size = skel_get_alloc_size,
	.unlink = skel_unlink,
	.chmod = skel_chmod,
	.fchmod = skel_fchmod,
	.chown = skel_chown,
	.fchown = skel_fchown,
	.lchown = skel_lchown,
	.chdir = skel_chdir,
	.getwd = skel_getwd,
	.ntimes = skel_ntimes,
	.ftruncate = skel_ftruncate,
	.fallocate = skel_fallocate,
	.lock = skel_lock,
	.kernel_flock = skel_kernel_flock,
	.linux_setlease = skel_linux_setlease,
	.getlock = skel_getlock,
	.symlink = skel_symlink,
	.vfs_readlink = skel_vfs_readlink,
	.link = skel_link,
	.mknod = skel_mknod,
	.realpath = skel_realpath,
	.notify_watch = skel_notify_watch,
	.chflags = skel_chflags,
	.file_id_create = skel_file_id_create,

	.streaminfo = skel_streaminfo,
	.get_real_filename = skel_get_real_filename,
	.connectpath = skel_connectpath,
	.brl_lock_windows = skel_brl_lock_windows,
	.brl_unlock_windows = skel_brl_unlock_windows,
	.brl_cancel_windows = skel_brl_cancel_windows,
	.strict_lock = skel_strict_lock,
	.strict_unlock = skel_strict_unlock,
	.translate_name = skel_translate_name,

	/* NT ACL operations. */

	.fget_nt_acl = skel_fget_nt_acl,
	.get_nt_acl = skel_get_nt_acl,
	.fset_nt_acl = skel_fset_nt_acl,

	/* POSIX ACL operations. */

	.chmod_acl = skel_chmod_acl,
	.fchmod_acl = skel_fchmod_acl,

	.sys_acl_get_entry = skel_sys_acl_get_entry,
	.sys_acl_get_tag_type = skel_sys_acl_get_tag_type,
	.sys_acl_get_permset = skel_sys_acl_get_permset,
	.sys_acl_get_qualifier = skel_sys_acl_get_qualifier,
	.sys_acl_get_file = skel_sys_acl_get_file,
	.sys_acl_get_fd = skel_sys_acl_get_fd,
	.sys_acl_clear_perms = skel_sys_acl_clear_perms,
	.sys_acl_add_perm = skel_sys_acl_add_perm,
	.sys_acl_to_text = skel_sys_acl_to_text,
	.sys_acl_init = skel_sys_acl_init,
	.sys_acl_create_entry = skel_sys_acl_create_entry,
	.sys_acl_set_tag_type = skel_sys_acl_set_tag_type,
	.sys_acl_set_qualifier = skel_sys_acl_set_qualifier,
	.sys_acl_set_permset = skel_sys_acl_set_permset,
	.sys_acl_valid = skel_sys_acl_valid,
	.sys_acl_set_file = skel_sys_acl_set_file,
	.sys_acl_set_fd = skel_sys_acl_set_fd,
	.sys_acl_delete_def_file = skel_sys_acl_delete_def_file,
	.sys_acl_get_perm = skel_sys_acl_get_perm,
	.sys_acl_free_text = skel_sys_acl_free_text,
	.sys_acl_free_acl = skel_sys_acl_free_acl,
	.sys_acl_free_qualifier = skel_sys_acl_free_qualifier,

	/* EA operations. */
	.getxattr = skel_getxattr,
	.lgetxattr = skel_lgetxattr,
	.fgetxattr = skel_fgetxattr,
	.listxattr = skel_listxattr,
	.llistxattr = skel_llistxattr,
	.flistxattr = skel_flistxattr,
	.removexattr = skel_removexattr,
	.lremovexattr = skel_lremovexattr,
	.fremovexattr = skel_fremovexattr,
	.setxattr = skel_setxattr,
	.lsetxattr = skel_lsetxattr,
	.fsetxattr = skel_fsetxattr,

	/* aio operations */
	.aio_read = skel_aio_read,
	.aio_write = skel_aio_write,
	.aio_return_fn = skel_aio_return_fn,
	.aio_cancel = skel_aio_cancel,
	.aio_error_fn = skel_aio_error_fn,
	.aio_fsync = skel_aio_fsync,
	.aio_suspend = skel_aio_suspend,
	.aio_force = skel_aio_force,

	/* offline operations */
	.is_offline = skel_is_offline,
	.set_offline = skel_set_offline
};

NTSTATUS vfs_skel_transparent_init(void)
{
	return smb_register_vfs(SMB_VFS_INTERFACE_VERSION, "skel_transparent", &skel_transparent_fns);
}
