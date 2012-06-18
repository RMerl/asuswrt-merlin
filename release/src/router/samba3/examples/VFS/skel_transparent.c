/* 
 * Skeleton VFS module.  Implements passthrough operation of all VFS
 * calls to disk functions.
 *
 * Copyright (C) Tim Potter, 1999-2000
 * Copyright (C) Alexander Bokovoy, 2002
 * Copyright (C) Stefan (metze) Metzmacher, 2003
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *  
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *  
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#include "includes.h"

/* PLEASE,PLEASE READ THE VFS MODULES CHAPTER OF THE 
   SAMBA DEVELOPERS GUIDE!!!!!!
 */

/* If you take this file as template for your module
 * please make sure that you remove all functions you didn't
 * want to implement!!
 *
 * This passthrough operations are useless in reall vfs modules!
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

static SMB_BIG_UINT skel_disk_free(vfs_handle_struct *handle,  const char *path,
	BOOL small_query, SMB_BIG_UINT *bsize,
	SMB_BIG_UINT *dfree, SMB_BIG_UINT *dsize)
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

static int skel_get_shadow_copy_data(vfs_handle_struct *handle, files_struct *fsp, SHADOW_COPY_DATA *shadow_copy_data, BOOL labels)
{
	return SMB_VFS_NEXT_GET_SHADOW_COPY_DATA(handle, fsp, shadow_copy_data, labels);
}

static int skel_statvfs(struct vfs_handle_struct *handle, const char *path, struct vfs_statvfs_struct *statbuf)
{
	return SMB_VFS_NEXT_STATVFS(handle, path, statbuf);
}

static SMB_STRUCT_DIR *skel_opendir(vfs_handle_struct *handle,  const char *fname, const char *mask, uint32 attr)
{
	return SMB_VFS_NEXT_OPENDIR(handle, fname, mask, attr);
}

static SMB_STRUCT_DIRENT *skel_readdir(vfs_handle_struct *handle,  SMB_STRUCT_DIR *dirp)
{
	return SMB_VFS_NEXT_READDIR(handle, dirp);
}

static void skel_seekdir(vfs_handle_struct *handle,  SMB_STRUCT_DIR *dirp, long offset)
{
	return SMB_VFS_NEXT_SEEKDIR(handle, dirp, offset);
}

static long skel_telldir(vfs_handle_struct *handle,  SMB_STRUCT_DIR *dirp)
{
	return SMB_VFS_NEXT_TELLDIR(handle, dirp);
}

static void skel_rewinddir(vfs_handle_struct *handle,  SMB_STRUCT_DIR *dirp)
{
	return SMB_VFS_NEXT_REWINDDIR(handle, dirp);
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

static int skel_open(vfs_handle_struct *handle,  const char *fname, files_struct *fsp, int flags, mode_t mode)
{
	return SMB_VFS_NEXT_OPEN(handle, fname, fsp, flags, mode);
}

static int skel_close(vfs_handle_struct *handle, files_struct *fsp, int fd)
{
	return SMB_VFS_NEXT_CLOSE(handle, fsp, fd);
}

static ssize_t skel_read(vfs_handle_struct *handle, files_struct *fsp, int fd, void *data, size_t n)
{
	return SMB_VFS_NEXT_READ(handle, fsp, fd, data, n);
}

static ssize_t skel_pread(vfs_handle_struct *handle, files_struct *fsp, int fd, void *data, size_t n, SMB_OFF_T offset)
{
	return SMB_VFS_NEXT_PREAD(handle, fsp, fd, data, n, offset);
}

static ssize_t skel_write(vfs_handle_struct *handle, files_struct *fsp, int fd, const void *data, size_t n)
{
	return SMB_VFS_NEXT_WRITE(handle, fsp, fd, data, n);
}

static ssize_t skel_pwrite(vfs_handle_struct *handle, files_struct *fsp, int fd, const void *data, size_t n, SMB_OFF_T offset)
{
	return SMB_VFS_NEXT_PWRITE(handle, fsp, fd, data, n, offset);
}

static SMB_OFF_T skel_lseek(vfs_handle_struct *handle, files_struct *fsp, int filedes, SMB_OFF_T offset, int whence)
{
	return SMB_VFS_NEXT_LSEEK(handle, fsp, filedes, offset, whence);
}

static int skel_rename(vfs_handle_struct *handle,  const char *oldname, const char *newname)
{
	return SMB_VFS_NEXT_RENAME(handle, oldname, newname);
}

static int skel_fsync(vfs_handle_struct *handle, files_struct *fsp, int fd)
{
	return SMB_VFS_NEXT_FSYNC(handle, fsp, fd);
}

static int skel_stat(vfs_handle_struct *handle,  const char *fname, SMB_STRUCT_STAT *sbuf)
{
	return SMB_VFS_NEXT_STAT(handle, fname, sbuf);
}

static int skel_fstat(vfs_handle_struct *handle, files_struct *fsp, int fd, SMB_STRUCT_STAT *sbuf)
{
	return SMB_VFS_NEXT_FSTAT(handle, fsp, fd, sbuf);
}

static int skel_lstat(vfs_handle_struct *handle,  const char *path, SMB_STRUCT_STAT *sbuf)
{
	return SMB_VFS_NEXT_LSTAT(handle, path, sbuf);
}

static int skel_unlink(vfs_handle_struct *handle,  const char *path)
{
	return SMB_VFS_NEXT_UNLINK(handle, path);
}

static int skel_chmod(vfs_handle_struct *handle,  const char *path, mode_t mode)
{
	return SMB_VFS_NEXT_CHMOD(handle, path, mode);
}

static int skel_fchmod(vfs_handle_struct *handle, files_struct *fsp, int fd, mode_t mode)
{
	return SMB_VFS_NEXT_FCHMOD(handle, fsp, fd, mode);
}

static int skel_chown(vfs_handle_struct *handle,  const char *path, uid_t uid, gid_t gid)
{
	return SMB_VFS_NEXT_CHOWN(handle, path, uid, gid);
}

static int skel_fchown(vfs_handle_struct *handle, files_struct *fsp, int fd, uid_t uid, gid_t gid)
{
	return SMB_VFS_NEXT_FCHOWN(handle, fsp, fd, uid, gid);
}

static int skel_chdir(vfs_handle_struct *handle,  const char *path)
{
	return SMB_VFS_NEXT_CHDIR(handle, path);
}

static char *skel_getwd(vfs_handle_struct *handle,  char *buf)
{
	return SMB_VFS_NEXT_GETWD(handle, buf);
}

static int skel_ntimes(vfs_handle_struct *handle,  const char *path, const struct timespec ts[2])
{
	return SMB_VFS_NEXT_NTIMES(handle, path, ts);
}

static int skel_ftruncate(vfs_handle_struct *handle, files_struct *fsp, int fd, SMB_OFF_T offset)
{
	return SMB_VFS_NEXT_FTRUNCATE(handle, fsp, fd, offset);
}

static BOOL skel_lock(vfs_handle_struct *handle, files_struct *fsp, int fd, int op, SMB_OFF_T offset, SMB_OFF_T count, int type)
{
	return SMB_VFS_NEXT_LOCK(handle, fsp, fd, op, offset, count, type);
}

static BOOL skel_getlock(vfs_handle_struct *handle, files_struct *fsp, int fd, SMB_OFF_T *poffset, SMB_OFF_T *pcount, int *ptype, pid_t *ppid)
{
	return SMB_VFS_NEXT_GETLOCK(handle, fsp, fd, poffset, pcount, ptype, ppid);
}

static int skel_symlink(vfs_handle_struct *handle,  const char *oldpath, const char *newpath)
{
	return SMB_VFS_NEXT_SYMLINK(handle, oldpath, newpath);
}

static int skel_readlink(vfs_handle_struct *handle,  const char *path, char *buf, size_t bufsiz)
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

static char *skel_realpath(vfs_handle_struct *handle,  const char *path, char *resolved_path)
{
	return SMB_VFS_NEXT_REALPATH(handle, path, resolved_path);
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

static size_t skel_fget_nt_acl(vfs_handle_struct *handle, files_struct *fsp,
	int fd, uint32 security_info, SEC_DESC **ppdesc)
{
	return SMB_VFS_NEXT_FGET_NT_ACL(handle, fsp, fd, security_info, ppdesc);
}

static size_t skel_get_nt_acl(vfs_handle_struct *handle, files_struct *fsp,
	const char *name, uint32 security_info, SEC_DESC **ppdesc)
{
	return SMB_VFS_NEXT_GET_NT_ACL(handle, fsp, name, security_info, ppdesc);
}

static BOOL skel_fset_nt_acl(vfs_handle_struct *handle, files_struct *fsp,
	int fd, uint32 security_info_sent, SEC_DESC *psd)
{
	return SMB_VFS_NEXT_FSET_NT_ACL(handle, fsp, fd, security_info_sent, psd);
}

static BOOL skel_set_nt_acl(vfs_handle_struct *handle, files_struct *fsp,
	const char *name, uint32 security_info_sent, SEC_DESC *psd)
{
	return SMB_VFS_NEXT_SET_NT_ACL(handle, fsp, name, security_info_sent, psd);
}

static int skel_chmod_acl(vfs_handle_struct *handle,  const char *name, mode_t mode)
{
	/* If the underlying VFS doesn't have ACL support... */
	if (!handle->vfs_next.ops.chmod_acl) {
		errno = ENOSYS;
		return -1;
	}
	return SMB_VFS_NEXT_CHMOD_ACL(handle, name, mode);
}

static int skel_fchmod_acl(vfs_handle_struct *handle, files_struct *fsp, int fd, mode_t mode)
{
	/* If the underlying VFS doesn't have ACL support... */
	if (!handle->vfs_next.ops.fchmod_acl) {
		errno = ENOSYS;
		return -1;
	}
	return SMB_VFS_NEXT_FCHMOD_ACL(handle, fsp, fd, mode);
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

static SMB_ACL_T skel_sys_acl_get_fd(vfs_handle_struct *handle, files_struct *fsp, int fd)
{
	return SMB_VFS_NEXT_SYS_ACL_GET_FD(handle, fsp, fd);
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

static int skel_sys_acl_set_fd(vfs_handle_struct *handle, files_struct *fsp, int fd, SMB_ACL_T theacl)
{
	return SMB_VFS_NEXT_SYS_ACL_SET_FD(handle, fsp, fd, theacl);
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

static ssize_t skel_fgetxattr(vfs_handle_struct *handle, struct files_struct *fsp,int fd, const char *name, void *value, size_t size)
{
        return SMB_VFS_NEXT_FGETXATTR(handle, fsp, fd, name, value, size);
}

static ssize_t skel_listxattr(vfs_handle_struct *handle, const char *path, char *list, size_t size)
{
        return SMB_VFS_NEXT_LISTXATTR(handle, path, list, size);
}

static ssize_t skel_llistxattr(vfs_handle_struct *handle, const char *path, char *list, size_t size)
{
        return SMB_VFS_NEXT_LLISTXATTR(handle, path, list, size);
}

static ssize_t skel_flistxattr(vfs_handle_struct *handle, struct files_struct *fsp,int fd, char *list, size_t size)
{
        return SMB_VFS_NEXT_FLISTXATTR(handle, fsp, fd, list, size);
}

static int skel_removexattr(vfs_handle_struct *handle, const char *path, const char *name)
{
        return SMB_VFS_NEXT_REMOVEXATTR(handle, path, name);
}

static int skel_lremovexattr(vfs_handle_struct *handle, const char *path, const char *name)
{
        return SMB_VFS_NEXT_LREMOVEXATTR(handle, path, name);
}

static int skel_fremovexattr(vfs_handle_struct *handle, struct files_struct *fsp,int fd, const char *name)
{
        return SMB_VFS_NEXT_FREMOVEXATTR(handle, fsp, fd, name);
}

static int skel_setxattr(vfs_handle_struct *handle, const char *path, const char *name, const void *value, size_t size, int flags)
{
        return SMB_VFS_NEXT_SETXATTR(handle, path, name, value, size, flags);
}

static int skel_lsetxattr(vfs_handle_struct *handle, const char *path, const char *name, const void *value, size_t size, int flags)
{
        return SMB_VFS_NEXT_LSETXATTR(handle, path, name, value, size, flags);
}

static int skel_fsetxattr(vfs_handle_struct *handle, struct files_struct *fsp,int fd, const char *name, const void *value, size_t size, int flags)
{
        return SMB_VFS_NEXT_FSETXATTR(handle, fsp, fd, name, value, size, flags);
}

static int skel_aio_read(struct vfs_handle_struct *handle, struct files_struct *fsp, SMB_STRUCT_AIOCB *aiocb)
{
	return SMB_VFS_NEXT_AIO_READ(handle, fsp, aiocb);
}

static int skel_aio_write(struct vfs_handle_struct *handle, struct files_struct *fsp, SMB_STRUCT_AIOCB *aiocb)
{
	return SMB_VFS_NEXT_AIO_WRITE(handle, fsp, aiocb);
}

static ssize_t skel_aio_return(struct vfs_handle_struct *handle, struct files_struct *fsp, SMB_STRUCT_AIOCB *aiocb)
{
	return SMB_VFS_NEXT_AIO_RETURN(handle, fsp, aiocb);
}

static int skel_aio_cancel(struct vfs_handle_struct *handle, struct files_struct *fsp, int fd, SMB_STRUCT_AIOCB *aiocb)
{
	return SMB_VFS_NEXT_AIO_CANCEL(handle, fsp, fd, aiocb);
}

static int skel_aio_error(struct vfs_handle_struct *handle, struct files_struct *fsp, SMB_STRUCT_AIOCB *aiocb)
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

/* VFS operations structure */

static vfs_op_tuple skel_op_tuples[] = {

	/* Disk operations */

	{SMB_VFS_OP(skel_connect),			SMB_VFS_OP_CONNECT, 		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_disconnect),			SMB_VFS_OP_DISCONNECT,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_disk_free),			SMB_VFS_OP_DISK_FREE,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_get_quota),			SMB_VFS_OP_GET_QUOTA,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_set_quota),			SMB_VFS_OP_SET_QUOTA,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_get_shadow_copy_data),		SMB_VFS_OP_GET_SHADOW_COPY_DATA,SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_statvfs),			SMB_VFS_OP_STATVFS,		SMB_VFS_LAYER_TRANSPARENT},

	/* Directory operations */

	{SMB_VFS_OP(skel_opendir),			SMB_VFS_OP_OPENDIR,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_readdir),			SMB_VFS_OP_READDIR,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_seekdir),			SMB_VFS_OP_SEEKDIR,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_telldir),			SMB_VFS_OP_TELLDIR,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_rewinddir),			SMB_VFS_OP_REWINDDIR,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_mkdir),			SMB_VFS_OP_MKDIR,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_rmdir),			SMB_VFS_OP_RMDIR,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_closedir),			SMB_VFS_OP_CLOSEDIR,		SMB_VFS_LAYER_TRANSPARENT},

	/* File operations */

	{SMB_VFS_OP(skel_open),				SMB_VFS_OP_OPEN,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_close),			SMB_VFS_OP_CLOSE,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_read),				SMB_VFS_OP_READ,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_write),			SMB_VFS_OP_WRITE,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_lseek),			SMB_VFS_OP_LSEEK,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_rename),			SMB_VFS_OP_RENAME,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_fsync),			SMB_VFS_OP_FSYNC,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_stat),				SMB_VFS_OP_STAT,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_fstat),			SMB_VFS_OP_FSTAT,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_lstat),			SMB_VFS_OP_LSTAT,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_unlink),			SMB_VFS_OP_UNLINK,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_chmod),			SMB_VFS_OP_CHMOD,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_fchmod),			SMB_VFS_OP_FCHMOD,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_chown),			SMB_VFS_OP_CHOWN,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_fchown),			SMB_VFS_OP_FCHOWN,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_chdir),			SMB_VFS_OP_CHDIR,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_getwd),			SMB_VFS_OP_GETWD,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_ntimes),			SMB_VFS_OP_NTIMES,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_ftruncate),			SMB_VFS_OP_FTRUNCATE,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_lock),				SMB_VFS_OP_LOCK,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_getlock),			SMB_VFS_OP_GETLOCK,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_symlink),			SMB_VFS_OP_SYMLINK,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_readlink),			SMB_VFS_OP_READLINK,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_link),				SMB_VFS_OP_LINK,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_mknod),			SMB_VFS_OP_MKNOD,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_realpath),			SMB_VFS_OP_REALPATH,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_notify_watch),			SMB_VFS_OP_NOTIFY_WATCH,	SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_chflags),			SMB_VFS_OP_CHFLAGS,		SMB_VFS_LAYER_TRANSPARENT},

	/* NT File ACL operations */

	{SMB_VFS_OP(skel_fget_nt_acl),			SMB_VFS_OP_FGET_NT_ACL,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_get_nt_acl),			SMB_VFS_OP_GET_NT_ACL,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_fset_nt_acl),			SMB_VFS_OP_FSET_NT_ACL,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_set_nt_acl),			SMB_VFS_OP_SET_NT_ACL,		SMB_VFS_LAYER_TRANSPARENT},

	/* POSIX ACL operations */

	{SMB_VFS_OP(skel_chmod_acl),			SMB_VFS_OP_CHMOD_ACL,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_fchmod_acl),			SMB_VFS_OP_FCHMOD_ACL,		SMB_VFS_LAYER_TRANSPARENT},

	{SMB_VFS_OP(skel_sys_acl_get_entry),		SMB_VFS_OP_SYS_ACL_GET_ENTRY,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_sys_acl_get_tag_type),		SMB_VFS_OP_SYS_ACL_GET_TAG_TYPE,	SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_sys_acl_get_permset),		SMB_VFS_OP_SYS_ACL_GET_PERMSET,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_sys_acl_get_qualifier),	SMB_VFS_OP_SYS_ACL_GET_QUALIFIER,	SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_sys_acl_get_file),		SMB_VFS_OP_SYS_ACL_GET_FILE,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_sys_acl_get_fd),		SMB_VFS_OP_SYS_ACL_GET_FD,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_sys_acl_clear_perms),		SMB_VFS_OP_SYS_ACL_CLEAR_PERMS,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_sys_acl_add_perm),		SMB_VFS_OP_SYS_ACL_ADD_PERM,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_sys_acl_to_text),		SMB_VFS_OP_SYS_ACL_TO_TEXT,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_sys_acl_init),			SMB_VFS_OP_SYS_ACL_INIT,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_sys_acl_create_entry),		SMB_VFS_OP_SYS_ACL_CREATE_ENTRY,	SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_sys_acl_set_tag_type),		SMB_VFS_OP_SYS_ACL_SET_TAG_TYPE,	SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_sys_acl_set_qualifier),	SMB_VFS_OP_SYS_ACL_SET_QUALIFIER,	SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_sys_acl_set_permset),		SMB_VFS_OP_SYS_ACL_SET_PERMSET,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_sys_acl_valid),		SMB_VFS_OP_SYS_ACL_VALID,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_sys_acl_set_file),		SMB_VFS_OP_SYS_ACL_SET_FILE,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_sys_acl_set_fd),		SMB_VFS_OP_SYS_ACL_SET_FD,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_sys_acl_delete_def_file),	SMB_VFS_OP_SYS_ACL_DELETE_DEF_FILE,	SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_sys_acl_get_perm),		SMB_VFS_OP_SYS_ACL_GET_PERM,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_sys_acl_free_text),		SMB_VFS_OP_SYS_ACL_FREE_TEXT,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_sys_acl_free_acl),		SMB_VFS_OP_SYS_ACL_FREE_ACL,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_sys_acl_free_qualifier),	SMB_VFS_OP_SYS_ACL_FREE_QUALIFIER,	SMB_VFS_LAYER_TRANSPARENT},
	
	/* EA operations. */
	{SMB_VFS_OP(skel_getxattr),			SMB_VFS_OP_GETXATTR,			SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_lgetxattr),			SMB_VFS_OP_LGETXATTR,			SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_fgetxattr),			SMB_VFS_OP_FGETXATTR,			SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_listxattr),			SMB_VFS_OP_LISTXATTR,			SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_llistxattr),			SMB_VFS_OP_LLISTXATTR,			SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_flistxattr),			SMB_VFS_OP_FLISTXATTR,			SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_removexattr),			SMB_VFS_OP_REMOVEXATTR,			SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_lremovexattr),			SMB_VFS_OP_LREMOVEXATTR,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_fremovexattr),			SMB_VFS_OP_FREMOVEXATTR,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_setxattr),			SMB_VFS_OP_SETXATTR,			SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_lsetxattr),			SMB_VFS_OP_LSETXATTR,			SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_fsetxattr),			SMB_VFS_OP_FSETXATTR,			SMB_VFS_LAYER_TRANSPARENT},

	/* AIO operations. */
	{SMB_VFS_OP(skel_aio_read),			SMB_VFS_OP_AIO_READ,			SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_aio_write),			SMB_VFS_OP_AIO_WRITE,			SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_aio_return),			SMB_VFS_OP_AIO_RETURN,			SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_aio_cancel),			SMB_VFS_OP_AIO_CANCEL,			SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_aio_error),			SMB_VFS_OP_AIO_ERROR,			SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_aio_fsync),			SMB_VFS_OP_AIO_FSYNC,			SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_aio_suspend),			SMB_VFS_OP_AIO_SUSPEND,			SMB_VFS_LAYER_TRANSPARENT},

	{NULL,						SMB_VFS_OP_NOOP,			SMB_VFS_LAYER_NOOP}
};

NTSTATUS init_module(void)
{
	return smb_register_vfs(SMB_VFS_INTERFACE_VERSION, "skel_transparent", skel_op_tuples);
}
