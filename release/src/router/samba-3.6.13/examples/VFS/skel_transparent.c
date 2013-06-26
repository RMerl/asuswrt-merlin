/* 
 * Skeleton VFS module.  Implements passthrough operation of all VFS
 * calls to disk functions.
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
 * please make sure that you remove all skel_XXX() functions you don't
 * want to implement!! The passthrough operations are not
 * neccessary in a real module.
 *
 * --metze
 */

static int skel_connect(vfs_handle_struct *handle,  const char *service, const char *user)    
{
	return SMB_VFS_NEXT_CONNECT(handle, service, user);
}

static void skel_disconnect(vfs_handle_struct *handle)
{
	SMB_VFS_NEXT_DISCONNECT(handle);
}

static uint64_t skel_disk_free(vfs_handle_struct *handle,  const char *path,
	bool small_query, uint64_t *bsize,
	uint64_t *dfree, uint64_t *dsize)
{
	return SMB_VFS_NEXT_DISK_FREE(handle, path, small_query, bsize, 
					 dfree, dsize);
}

static int skel_get_quota(vfs_handle_struct *handle,  enum SMB_QUOTA_TYPE qtype, unid_t id, SMB_DISK_QUOTA *dq)
{
	return SMB_VFS_NEXT_GET_QUOTA(handle, qtype, id, dq);
}

static int skel_set_quota(vfs_handle_struct *handle,  enum SMB_QUOTA_TYPE qtype, unid_t id, SMB_DISK_QUOTA *dq)
{
	return SMB_VFS_NEXT_SET_QUOTA(handle, qtype, id, dq);
}

static int skel_get_shadow_copy_data(vfs_handle_struct *handle, files_struct *fsp, struct shadow_copy_data *shadow_copy_data, bool labels)
{
	return SMB_VFS_NEXT_GET_SHADOW_COPY_DATA(handle, fsp, shadow_copy_data, labels);
}

static int skel_statvfs(struct vfs_handle_struct *handle, const char *path, struct vfs_statvfs_struct *statbuf)
{
	return SMB_VFS_NEXT_STATVFS(handle, path, statbuf);
}

static uint32_t skel_fs_capabilities(struct vfs_handle_struct *handle, enum timestamp_set_resolution *p_ts_res)
{
	return SMB_VFS_NEXT_FS_CAPABILITIES(handle, p_ts_res);
}

static SMB_STRUCT_DIR *skel_opendir(vfs_handle_struct *handle,  const char *fname, const char *mask, uint32 attr)
{
	return SMB_VFS_NEXT_OPENDIR(handle, fname, mask, attr);
}

static SMB_STRUCT_DIR *skel_fdopendir(vfs_handle_struct *handle, files_struct *fsp, const char *mask, uint32 attr)
{
	return SMB_VFS_NEXT_FDOPENDIR(handle, fsp, mask, attr);
}

static SMB_STRUCT_DIRENT *skel_readdir(vfs_handle_struct *handle,
				       SMB_STRUCT_DIR *dirp,
				       SMB_STRUCT_STAT *sbuf)
{
	return SMB_VFS_NEXT_READDIR(handle, dirp, sbuf);
}

static void skel_seekdir(vfs_handle_struct *handle,  SMB_STRUCT_DIR *dirp, long offset)
{
	SMB_VFS_NEXT_SEEKDIR(handle, dirp, offset);
}

static long skel_telldir(vfs_handle_struct *handle,  SMB_STRUCT_DIR *dirp)
{
	return SMB_VFS_NEXT_TELLDIR(handle, dirp);
}

static void skel_rewind_dir(vfs_handle_struct *handle, SMB_STRUCT_DIR *dirp)
{
	SMB_VFS_NEXT_REWINDDIR(handle, dirp);
}

static int skel_mkdir(vfs_handle_struct *handle,  const char *path, mode_t mode)
{
	return SMB_VFS_NEXT_MKDIR(handle, path, mode);
}

static int skel_rmdir(vfs_handle_struct *handle,  const char *path)
{
	return SMB_VFS_NEXT_RMDIR(handle, path);
}

static int skel_closedir(vfs_handle_struct *handle,  SMB_STRUCT_DIR *dir)
{
	return SMB_VFS_NEXT_CLOSEDIR(handle, dir);
}

static void skel_init_search_op(struct vfs_handle_struct *handle, SMB_STRUCT_DIR *dirp)
{
	SMB_VFS_NEXT_INIT_SEARCH_OP(handle, dirp);
}

static int skel_open(vfs_handle_struct *handle, struct smb_filename *smb_fname,
		     files_struct *fsp, int flags, mode_t mode)
{
	return SMB_VFS_NEXT_OPEN(handle, smb_fname, fsp, flags, mode);
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
	return SMB_VFS_NEXT_CREATE_FILE(handle,
				req,
				root_dir_fid,
				smb_fname,
				access_mask,
				share_access,
				create_disposition,
				create_options,
				file_attributes,
				oplock_request,
				allocation_size,
				private_flags,
				sd,
				ea_list,
				result,
				pinfo);
}

static int skel_close_fn(vfs_handle_struct *handle, files_struct *fsp)
{
	return SMB_VFS_NEXT_CLOSE(handle, fsp);
}

static ssize_t skel_vfs_read(vfs_handle_struct *handle, files_struct *fsp, void *data, size_t n)
{
	return SMB_VFS_NEXT_READ(handle, fsp, data, n);
}

static ssize_t skel_pread(vfs_handle_struct *handle, files_struct *fsp, void *data, size_t n, SMB_OFF_T offset)
{
	return SMB_VFS_NEXT_PREAD(handle, fsp, data, n, offset);
}

static ssize_t skel_write(vfs_handle_struct *handle, files_struct *fsp, const void *data, size_t n)
{
	return SMB_VFS_NEXT_WRITE(handle, fsp, data, n);
}

static ssize_t skel_pwrite(vfs_handle_struct *handle, files_struct *fsp, const void *data, size_t n, SMB_OFF_T offset)
{
	return SMB_VFS_NEXT_PWRITE(handle, fsp, data, n, offset);
}

static SMB_OFF_T skel_lseek(vfs_handle_struct *handle, files_struct *fsp, SMB_OFF_T offset, int whence)
{
	return SMB_VFS_NEXT_LSEEK(handle, fsp, offset, whence);
}

static ssize_t skel_sendfile(vfs_handle_struct *handle, int tofd, files_struct *fromfsp, const DATA_BLOB *hdr, SMB_OFF_T offset, size_t n)
{
	return SMB_VFS_NEXT_SENDFILE(handle, tofd, fromfsp, hdr, offset, n);
}

static ssize_t skel_recvfile(vfs_handle_struct *handle, int fromfd, files_struct *tofsp, SMB_OFF_T offset, size_t n)
{
	return SMB_VFS_NEXT_RECVFILE(handle, fromfd, tofsp, offset, n);
}

static int skel_rename(vfs_handle_struct *handle,
		       const struct smb_filename *smb_fname_src,
		       const struct smb_filename *smb_fname_dst)
{
	return SMB_VFS_NEXT_RENAME(handle, smb_fname_src, smb_fname_dst);
}

static int skel_fsync(vfs_handle_struct *handle, files_struct *fsp)
{
	return SMB_VFS_NEXT_FSYNC(handle, fsp);
}

static int skel_stat(vfs_handle_struct *handle, struct smb_filename *smb_fname)
{
	return SMB_VFS_NEXT_STAT(handle, smb_fname);
}

static int skel_fstat(vfs_handle_struct *handle, files_struct *fsp, SMB_STRUCT_STAT *sbuf)
{
	return SMB_VFS_NEXT_FSTAT(handle, fsp, sbuf);
}

static int skel_lstat(vfs_handle_struct *handle, struct smb_filename *smb_fname)
{
	return SMB_VFS_NEXT_LSTAT(handle, smb_fname);
}

static uint64_t skel_get_alloc_size(struct vfs_handle_struct *handle, struct files_struct *fsp, const SMB_STRUCT_STAT *sbuf)
{
	return SMB_VFS_NEXT_GET_ALLOC_SIZE(handle, fsp, sbuf);
}

static int skel_unlink(vfs_handle_struct *handle,
		       const struct smb_filename *smb_fname)
{
	return SMB_VFS_NEXT_UNLINK(handle, smb_fname);
}

static int skel_chmod(vfs_handle_struct *handle,  const char *path, mode_t mode)
{
	return SMB_VFS_NEXT_CHMOD(handle, path, mode);
}

static int skel_fchmod(vfs_handle_struct *handle, files_struct *fsp, mode_t mode)
{
	return SMB_VFS_NEXT_FCHMOD(handle, fsp, mode);
}

static int skel_chown(vfs_handle_struct *handle,  const char *path, uid_t uid, gid_t gid)
{
	return SMB_VFS_NEXT_CHOWN(handle, path, uid, gid);
}

static int skel_fchown(vfs_handle_struct *handle, files_struct *fsp, uid_t uid, gid_t gid)
{
	return SMB_VFS_NEXT_FCHOWN(handle, fsp, uid, gid);
}

static int skel_lchown(vfs_handle_struct *handle,  const char *path, uid_t uid, gid_t gid)
{
	return SMB_VFS_NEXT_LCHOWN(handle, path, uid, gid);
}

static int skel_chdir(vfs_handle_struct *handle,  const char *path)
{
	return SMB_VFS_NEXT_CHDIR(handle, path);
}

static char *skel_getwd(vfs_handle_struct *handle,  char *buf)
{
	return SMB_VFS_NEXT_GETWD(handle, buf);
}

static int skel_ntimes(vfs_handle_struct *handle,
		       const struct smb_filename *smb_fname,
		       struct smb_file_time *ft)
{
	return SMB_VFS_NEXT_NTIMES(handle, smb_fname, ft);
}

static int skel_ftruncate(vfs_handle_struct *handle, files_struct *fsp, SMB_OFF_T offset)
{
	return SMB_VFS_NEXT_FTRUNCATE(handle, fsp, offset);
}

static int skel_fallocate(vfs_handle_struct *handle, files_struct *fsp,
			enum vfs_fallocate_mode mode,
			SMB_OFF_T offset,
			SMB_OFF_T len)
{
	return SMB_VFS_NEXT_FALLOCATE(handle, fsp, mode, offset, len);
}

static bool skel_lock(vfs_handle_struct *handle, files_struct *fsp, int op, SMB_OFF_T offset, SMB_OFF_T count, int type)
{
	return SMB_VFS_NEXT_LOCK(handle, fsp, op, offset, count, type);
}

static int skel_kernel_flock(struct vfs_handle_struct *handle, struct files_struct *fsp, uint32 share_mode, uint32 access_mask)
{
	return SMB_VFS_NEXT_KERNEL_FLOCK(handle, fsp, share_mode, access_mask);
}

static int skel_linux_setlease(struct vfs_handle_struct *handle, struct files_struct *fsp, int leasetype)
{
	return SMB_VFS_NEXT_LINUX_SETLEASE(handle, fsp, leasetype);
}

static bool skel_getlock(vfs_handle_struct *handle, files_struct *fsp, SMB_OFF_T *poffset, SMB_OFF_T *pcount, int *ptype, pid_t *ppid)
{
	return SMB_VFS_NEXT_GETLOCK(handle, fsp, poffset, pcount, ptype, ppid);
}

static int skel_symlink(vfs_handle_struct *handle,  const char *oldpath, const char *newpath)
{
	return SMB_VFS_NEXT_SYMLINK(handle, oldpath, newpath);
}

static int skel_vfs_readlink(vfs_handle_struct *handle, const char *path, char *buf, size_t bufsiz)
{
	return SMB_VFS_NEXT_READLINK(handle, path, buf, bufsiz);
}

static int skel_link(vfs_handle_struct *handle,  const char *oldpath, const char *newpath)
{
	return SMB_VFS_NEXT_LINK(handle, oldpath, newpath);
}

static int skel_mknod(vfs_handle_struct *handle,  const char *path, mode_t mode, SMB_DEV_T dev)
{
	return SMB_VFS_NEXT_MKNOD(handle, path, mode, dev);
}

static char *skel_realpath(vfs_handle_struct *handle,  const char *path)
{
	return SMB_VFS_NEXT_REALPATH(handle, path);
}

static NTSTATUS skel_notify_watch(struct vfs_handle_struct *handle,
	    struct sys_notify_context *ctx, struct notify_entry *e,
	    void (*callback)(struct sys_notify_context *ctx, void *private_data, struct notify_event *ev),
	    void *private_data, void *handle_p)
{
	return SMB_VFS_NEXT_NOTIFY_WATCH(handle, ctx, e, callback,
		private_data, handle_p);
}

static int skel_chflags(vfs_handle_struct *handle,  const char *path, uint flags)
{
	return SMB_VFS_NEXT_CHFLAGS(handle, path, flags);
}

static struct file_id skel_file_id_create(vfs_handle_struct *handle,
					  const SMB_STRUCT_STAT *sbuf)
{
	return SMB_VFS_NEXT_FILE_ID_CREATE(handle, sbuf);
}

static NTSTATUS skel_streaminfo(struct vfs_handle_struct *handle,
				struct files_struct *fsp,
				const char *fname,
				TALLOC_CTX *mem_ctx,
				unsigned int *num_streams,
				struct stream_struct **streams)
{
	return SMB_VFS_NEXT_STREAMINFO(handle,
					fsp,
					fname,
					mem_ctx,
					num_streams,
					streams);
}

static int skel_get_real_filename(struct vfs_handle_struct *handle,
				const char *path,
				const char *name,
				TALLOC_CTX *mem_ctx,
				char **found_name)
{
	return SMB_VFS_NEXT_GET_REAL_FILENAME(handle,
					path,
					name,
					mem_ctx,
					found_name);
}

static const char *skel_connectpath(struct vfs_handle_struct *handle,
				const char *filename)
{
	return SMB_VFS_NEXT_CONNECTPATH(handle, filename);
}

static NTSTATUS skel_brl_lock_windows(struct vfs_handle_struct *handle,
				struct byte_range_lock *br_lck,
				struct lock_struct *plock,
				bool blocking_lock,
				struct blocking_lock_record *blr)
{
	return SMB_VFS_NEXT_BRL_LOCK_WINDOWS(handle,
					br_lck,
					plock,
					blocking_lock,
					blr);
}

static bool skel_brl_unlock_windows(struct vfs_handle_struct *handle,
				struct messaging_context *msg_ctx,
				struct byte_range_lock *br_lck,
				const struct lock_struct *plock)
{
	return SMB_VFS_NEXT_BRL_UNLOCK_WINDOWS(handle,
					msg_ctx,
					br_lck,
					plock);
}

static bool skel_brl_cancel_windows(struct vfs_handle_struct *handle,
				struct byte_range_lock *br_lck,
				struct lock_struct *plock,
				struct blocking_lock_record *blr)
{
	return SMB_VFS_NEXT_BRL_CANCEL_WINDOWS(handle,
				br_lck,
				plock,
				blr);
}

static bool skel_strict_lock(struct vfs_handle_struct *handle,
				struct files_struct *fsp,
				struct lock_struct *plock)
{
	return SMB_VFS_NEXT_STRICT_LOCK(handle,
					fsp,
					plock);
}

static void skel_strict_unlock(struct vfs_handle_struct *handle,
				struct files_struct *fsp,
				struct lock_struct *plock)
{
	SMB_VFS_NEXT_STRICT_UNLOCK(handle,
					fsp,
					plock);
}

static NTSTATUS skel_translate_name(struct vfs_handle_struct *handle,
				const char *mapped_name,
				enum vfs_translate_direction direction,
				TALLOC_CTX *mem_ctx,
				char **pmapped_name)
{
	return SMB_VFS_NEXT_TRANSLATE_NAME(handle, mapped_name, direction,
					   mem_ctx, pmapped_name);
}

static NTSTATUS skel_fget_nt_acl(vfs_handle_struct *handle, files_struct *fsp,
	uint32 security_info, struct security_descriptor **ppdesc)
{
	return SMB_VFS_NEXT_FGET_NT_ACL(handle, fsp, security_info, ppdesc);
}

static NTSTATUS skel_get_nt_acl(vfs_handle_struct *handle,
	const char *name, uint32 security_info, struct security_descriptor **ppdesc)
{
	return SMB_VFS_NEXT_GET_NT_ACL(handle, name, security_info, ppdesc);
}

static NTSTATUS skel_fset_nt_acl(vfs_handle_struct *handle, files_struct *fsp,
	uint32 security_info_sent, const struct security_descriptor *psd)
{
	return SMB_VFS_NEXT_FSET_NT_ACL(handle, fsp, security_info_sent, psd);
}

static int skel_chmod_acl(vfs_handle_struct *handle,  const char *name, mode_t mode)
{
	return SMB_VFS_NEXT_CHMOD_ACL(handle, name, mode);
}

static int skel_fchmod_acl(vfs_handle_struct *handle, files_struct *fsp, mode_t mode)
{
	return SMB_VFS_NEXT_FCHMOD_ACL(handle, fsp, mode);
}

static int skel_sys_acl_get_entry(vfs_handle_struct *handle,  SMB_ACL_T theacl, int entry_id, SMB_ACL_ENTRY_T *entry_p)
{
	return SMB_VFS_NEXT_SYS_ACL_GET_ENTRY(handle, theacl, entry_id, entry_p);
}

static int skel_sys_acl_get_tag_type(vfs_handle_struct *handle,  SMB_ACL_ENTRY_T entry_d, SMB_ACL_TAG_T *tag_type_p)
{
	return SMB_VFS_NEXT_SYS_ACL_GET_TAG_TYPE(handle, entry_d, tag_type_p);
}

static int skel_sys_acl_get_permset(vfs_handle_struct *handle,  SMB_ACL_ENTRY_T entry_d, SMB_ACL_PERMSET_T *permset_p)
{
	return SMB_VFS_NEXT_SYS_ACL_GET_PERMSET(handle, entry_d, permset_p);
}

static void *skel_sys_acl_get_qualifier(vfs_handle_struct *handle,  SMB_ACL_ENTRY_T entry_d)
{
	return SMB_VFS_NEXT_SYS_ACL_GET_QUALIFIER(handle, entry_d);
}

static SMB_ACL_T skel_sys_acl_get_file(vfs_handle_struct *handle,  const char *path_p, SMB_ACL_TYPE_T type)
{
	return SMB_VFS_NEXT_SYS_ACL_GET_FILE(handle, path_p, type);
}

static SMB_ACL_T skel_sys_acl_get_fd(vfs_handle_struct *handle, files_struct *fsp)
{
	return SMB_VFS_NEXT_SYS_ACL_GET_FD(handle, fsp);
}

static int skel_sys_acl_clear_perms(vfs_handle_struct *handle,  SMB_ACL_PERMSET_T permset)
{
	return SMB_VFS_NEXT_SYS_ACL_CLEAR_PERMS(handle, permset);
}

static int skel_sys_acl_add_perm(vfs_handle_struct *handle,  SMB_ACL_PERMSET_T permset, SMB_ACL_PERM_T perm)
{
	return SMB_VFS_NEXT_SYS_ACL_ADD_PERM(handle, permset, perm);
}

static char *skel_sys_acl_to_text(vfs_handle_struct *handle,  SMB_ACL_T theacl, ssize_t *plen)
{
	return SMB_VFS_NEXT_SYS_ACL_TO_TEXT(handle, theacl, plen);
}

static SMB_ACL_T skel_sys_acl_init(vfs_handle_struct *handle,  int count)
{
	return SMB_VFS_NEXT_SYS_ACL_INIT(handle, count);
}

static int skel_sys_acl_create_entry(vfs_handle_struct *handle,  SMB_ACL_T *pacl, SMB_ACL_ENTRY_T *pentry)
{
	return SMB_VFS_NEXT_SYS_ACL_CREATE_ENTRY(handle, pacl, pentry);
}

static int skel_sys_acl_set_tag_type(vfs_handle_struct *handle,  SMB_ACL_ENTRY_T entry, SMB_ACL_TAG_T tagtype)
{
	return SMB_VFS_NEXT_SYS_ACL_SET_TAG_TYPE(handle, entry, tagtype);
}

static int skel_sys_acl_set_qualifier(vfs_handle_struct *handle,  SMB_ACL_ENTRY_T entry, void *qual)
{
	return SMB_VFS_NEXT_SYS_ACL_SET_QUALIFIER(handle, entry, qual);
}

static int skel_sys_acl_set_permset(vfs_handle_struct *handle,  SMB_ACL_ENTRY_T entry, SMB_ACL_PERMSET_T permset)
{
	return SMB_VFS_NEXT_SYS_ACL_SET_PERMSET(handle, entry, permset);
}

static int skel_sys_acl_valid(vfs_handle_struct *handle,  SMB_ACL_T theacl )
{
	return SMB_VFS_NEXT_SYS_ACL_VALID(handle, theacl);
}

static int skel_sys_acl_set_file(vfs_handle_struct *handle,  const char *name, SMB_ACL_TYPE_T acltype, SMB_ACL_T theacl)
{
	return SMB_VFS_NEXT_SYS_ACL_SET_FILE(handle, name, acltype, theacl);
}

static int skel_sys_acl_set_fd(vfs_handle_struct *handle, files_struct *fsp, SMB_ACL_T theacl)
{
	return SMB_VFS_NEXT_SYS_ACL_SET_FD(handle, fsp, theacl);
}

static int skel_sys_acl_delete_def_file(vfs_handle_struct *handle,  const char *path)
{
	return SMB_VFS_NEXT_SYS_ACL_DELETE_DEF_FILE(handle, path);
}

static int skel_sys_acl_get_perm(vfs_handle_struct *handle,  SMB_ACL_PERMSET_T permset, SMB_ACL_PERM_T perm)
{
	return SMB_VFS_NEXT_SYS_ACL_GET_PERM(handle, permset, perm);
}

static int skel_sys_acl_free_text(vfs_handle_struct *handle,  char *text)
{
	return SMB_VFS_NEXT_SYS_ACL_FREE_TEXT(handle, text);
}

static int skel_sys_acl_free_acl(vfs_handle_struct *handle,  SMB_ACL_T posix_acl)
{
	return SMB_VFS_NEXT_SYS_ACL_FREE_ACL(handle, posix_acl);
}

static int skel_sys_acl_free_qualifier(vfs_handle_struct *handle,  void *qualifier, SMB_ACL_TAG_T tagtype)
{
	return SMB_VFS_NEXT_SYS_ACL_FREE_QUALIFIER(handle, qualifier, tagtype);
}

static ssize_t skel_getxattr(vfs_handle_struct *handle, const char *path, const char *name, void *value, size_t size)
{
        return SMB_VFS_NEXT_GETXATTR(handle, path, name, value, size);
}

static ssize_t skel_lgetxattr(vfs_handle_struct *handle, const char *path, const char *name, void *value, size_t
size)
{
        return SMB_VFS_NEXT_LGETXATTR(handle, path, name, value, size);
}

static ssize_t skel_fgetxattr(vfs_handle_struct *handle, struct files_struct *fsp, const char *name, void *value, size_t size)
{
        return SMB_VFS_NEXT_FGETXATTR(handle, fsp, name, value, size);
}

static ssize_t skel_listxattr(vfs_handle_struct *handle, const char *path, char *list, size_t size)
{
        return SMB_VFS_NEXT_LISTXATTR(handle, path, list, size);
}

static ssize_t skel_llistxattr(vfs_handle_struct *handle, const char *path, char *list, size_t size)
{
        return SMB_VFS_NEXT_LLISTXATTR(handle, path, list, size);
}

static ssize_t skel_flistxattr(vfs_handle_struct *handle, struct files_struct *fsp, char *list, size_t size)
{
        return SMB_VFS_NEXT_FLISTXATTR(handle, fsp, list, size);
}

static int skel_removexattr(vfs_handle_struct *handle, const char *path, const char *name)
{
        return SMB_VFS_NEXT_REMOVEXATTR(handle, path, name);
}

static int skel_lremovexattr(vfs_handle_struct *handle, const char *path, const char *name)
{
        return SMB_VFS_NEXT_LREMOVEXATTR(handle, path, name);
}

static int skel_fremovexattr(vfs_handle_struct *handle, struct files_struct *fsp, const char *name)
{
        return SMB_VFS_NEXT_FREMOVEXATTR(handle, fsp, name);
}

static int skel_setxattr(vfs_handle_struct *handle, const char *path, const char *name, const void *value, size_t size, int flags)
{
        return SMB_VFS_NEXT_SETXATTR(handle, path, name, value, size, flags);
}

static int skel_lsetxattr(vfs_handle_struct *handle, const char *path, const char *name, const void *value, size_t size, int flags)
{
        return SMB_VFS_NEXT_LSETXATTR(handle, path, name, value, size, flags);
}

static int skel_fsetxattr(vfs_handle_struct *handle, struct files_struct *fsp, const char *name, const void *value, size_t size, int flags)
{
        return SMB_VFS_NEXT_FSETXATTR(handle, fsp, name, value, size, flags);
}

static int skel_aio_read(struct vfs_handle_struct *handle, struct files_struct *fsp, SMB_STRUCT_AIOCB *aiocb)
{
	return SMB_VFS_NEXT_AIO_READ(handle, fsp, aiocb);
}

static int skel_aio_write(struct vfs_handle_struct *handle, struct files_struct *fsp, SMB_STRUCT_AIOCB *aiocb)
{
	return SMB_VFS_NEXT_AIO_WRITE(handle, fsp, aiocb);
}

static ssize_t skel_aio_return_fn(struct vfs_handle_struct *handle, struct files_struct *fsp, SMB_STRUCT_AIOCB *aiocb)
{
	return SMB_VFS_NEXT_AIO_RETURN(handle, fsp, aiocb);
}

static int skel_aio_cancel(struct vfs_handle_struct *handle, struct files_struct *fsp, SMB_STRUCT_AIOCB *aiocb)
{
	return SMB_VFS_NEXT_AIO_CANCEL(handle, fsp, aiocb);
}

static int skel_aio_error_fn(struct vfs_handle_struct *handle, struct files_struct *fsp, SMB_STRUCT_AIOCB *aiocb)
{
	return SMB_VFS_NEXT_AIO_ERROR(handle, fsp, aiocb);
}

static int skel_aio_fsync(struct vfs_handle_struct *handle, struct files_struct *fsp, int op, SMB_STRUCT_AIOCB *aiocb)
{
	return SMB_VFS_NEXT_AIO_FSYNC(handle, fsp, op, aiocb);
}

static int skel_aio_suspend(struct vfs_handle_struct *handle, struct files_struct *fsp, const SMB_STRUCT_AIOCB * const aiocb[], int n, const struct timespec *ts)
{
	return SMB_VFS_NEXT_AIO_SUSPEND(handle, fsp, aiocb, n, ts);
}

static bool skel_aio_force(struct vfs_handle_struct *handle, struct files_struct *fsp)
{
        return SMB_VFS_NEXT_AIO_FORCE(handle, fsp);
}

static bool skel_is_offline(struct vfs_handle_struct *handle, const struct smb_filename *fname, SMB_STRUCT_STAT *sbuf)
{
	return SMB_VFS_NEXT_IS_OFFLINE(handle, fname, sbuf);
}

static int skel_set_offline(struct vfs_handle_struct *handle, const struct smb_filename *fname)
{
	return SMB_VFS_NEXT_SET_OFFLINE(handle, fname);
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
