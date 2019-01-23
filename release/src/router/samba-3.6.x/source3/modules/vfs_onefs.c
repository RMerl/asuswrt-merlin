/*
 * Unix SMB/CIFS implementation.
 * Support for OneFS
 *
 * Copyright (C) Tim Prouty, 2008
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
#include "onefs.h"
#include "onefs_config.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_VFS

static int onefs_connect(struct vfs_handle_struct *handle, const char *service,
			 const char *user)
{
	int ret = SMB_VFS_NEXT_CONNECT(handle, service, user);

	if (ret < 0) {
		return ret;
	}

	ret = onefs_load_config(handle->conn);
	if (ret) {
		SMB_VFS_NEXT_DISCONNECT(handle);
		DEBUG(3, ("Load config failed: %s\n", strerror(errno)));
		return ret;
	}

	return 0;
}

static int onefs_mkdir(vfs_handle_struct *handle, const char *path,
		       mode_t mode)
{
	/* SMB_VFS_MKDIR should never be called in vfs_onefs */
	SMB_ASSERT(false);
	return SMB_VFS_NEXT_MKDIR(handle, path, mode);
}

static int onefs_open(vfs_handle_struct *handle,
		      struct smb_filename *smb_fname,
		      files_struct *fsp, int flags, mode_t mode)
{
	/* SMB_VFS_OPEN should never be called in vfs_onefs */
	SMB_ASSERT(false);
	return SMB_VFS_NEXT_OPEN(handle, smb_fname, fsp, flags, mode);
}

static ssize_t onefs_sendfile(vfs_handle_struct *handle, int tofd,
			      files_struct *fromfsp, const DATA_BLOB *header,
			      SMB_OFF_T offset, size_t count)
{
	ssize_t result;

	START_PROFILE_BYTES(syscall_sendfile, count);
	result = onefs_sys_sendfile(handle->conn, tofd, fromfsp->fh->fd,
				    header, offset, count);
	END_PROFILE(syscall_sendfile);
	return result;
}

static ssize_t onefs_recvfile(vfs_handle_struct *handle, int fromfd,
			      files_struct *tofsp, SMB_OFF_T offset,
			      size_t count)
{
	ssize_t result;

	START_PROFILE_BYTES(syscall_recvfile, count);
	result = onefs_sys_recvfile(fromfd, tofsp->fh->fd, offset, count);
	END_PROFILE(syscall_recvfile);
	return result;
}

static uint64_t onefs_get_alloc_size(struct vfs_handle_struct *handle,
				     files_struct *fsp,
				     const SMB_STRUCT_STAT *sbuf)
{
	uint64_t result;

	START_PROFILE(syscall_get_alloc_size);

	if(S_ISDIR(sbuf->st_ex_mode)) {
		result = 0;
		goto out;
	}

	/* Just use the file size since st_blocks is unreliable on OneFS. */
	result = get_file_size_stat(sbuf);

	if (fsp && fsp->initial_allocation_size)
		result = MAX(result,fsp->initial_allocation_size);

	result = smb_roundup(handle->conn, result);

 out:
	END_PROFILE(syscall_get_alloc_size);
	return result;
}

static struct file_id onefs_file_id_create(struct vfs_handle_struct *handle,
					   const SMB_STRUCT_STAT *sbuf)
{
	struct file_id key;

	/* the ZERO_STRUCT ensures padding doesn't break using the key as a
	 * blob */
	ZERO_STRUCT(key);

	key.devid = sbuf->st_ex_dev;
	key.inode = sbuf->st_ex_ino;
	key.extid = sbuf->vfs_private;

	return key;
}

static int onefs_statvfs(vfs_handle_struct *handle, const char *path,
			 vfs_statvfs_struct *statbuf)
{
	struct statvfs statvfs_buf;
	int result;

	DEBUG(5, ("Calling SMB_STAT_VFS \n"));
	result = statvfs(path, &statvfs_buf);
	ZERO_STRUCTP(statbuf);

	if (!result) {
		statbuf->OptimalTransferSize = statvfs_buf.f_iosize;
		statbuf->BlockSize = statvfs_buf.f_bsize;
		statbuf->TotalBlocks = statvfs_buf.f_blocks;
		statbuf->BlocksAvail = statvfs_buf.f_bfree;
		statbuf->UserBlocksAvail = statvfs_buf.f_bavail;
		statbuf->TotalFileNodes = statvfs_buf.f_files;
		statbuf->FreeFileNodes = statvfs_buf.f_ffree;
		statbuf->FsIdentifier =
		    (((uint64_t)statvfs_buf.f_fsid.val[0]<<32) &
			0xffffffff00000000LL) |
		    (uint64_t)statvfs_buf.f_fsid.val[1];
	}
        return result;
}

static int onefs_get_real_filename(vfs_handle_struct *handle, const char *path,
				   const char *name, TALLOC_CTX *mem_ctx,
				   char **found_name)
{
	struct stat sb;
	struct connection_struct *conn = handle->conn;
	struct stat_extra se;
	int result;
	char *unmangled_name = NULL;
	char *full_name = NULL;

	/* First demangle the name if necessary. */
	if (!conn->case_sensitive && mangle_is_mangled(name, conn->params) &&
	    mangle_lookup_name_from_8_3(mem_ctx, name, &unmangled_name,
					conn->params)) {
		/* Name is now unmangled. */
		name = unmangled_name;
	}

	/* Do the case insensitive stat. */
	ZERO_STRUCT(se);
	se.se_version = ESTAT_CURRENT_VERSION;
	se.se_flags = ESTAT_CASE_INSENSITIVE | ESTAT_SYMLINK_NOFOLLOW;

	if (*path != '\0') {
		if (!(full_name = talloc_asprintf(mem_ctx, "%s/%s", path, name))) {
			errno = ENOMEM;
			DEBUG(2, ("talloc_asprintf failed\n"));
			result = -1;
			goto done;
		}
	}

	if ((result = estat(full_name ? full_name : name, &sb, &se)) != 0) {
		DEBUG(2, ("error calling estat: %s\n", strerror(errno)));
		goto done;
	}

	*found_name = talloc_strdup(mem_ctx, se.se_realname);
	if (*found_name == NULL) {
		errno = ENOMEM;
		result = -1;
		goto done;
	}

done:
	TALLOC_FREE(full_name);
	TALLOC_FREE(unmangled_name);
	return result;
}

static int onefs_ntimes(vfs_handle_struct *handle,
			const struct smb_filename *smb_fname,
			struct smb_file_time *ft)
{
	int flags = 0;
	struct timespec times[3];

	if (!null_timespec(ft->atime)) {
		flags |= VT_ATIME;
		times[0] = ft->atime;
		DEBUG(6,("**** onefs_ntimes: actime: %s.%d\n",
			time_to_asc(convert_timespec_to_time_t(ft->atime)),
			ft->atime.tv_nsec));
	}

	if (!null_timespec(ft->mtime)) {
		flags |= VT_MTIME;
		times[1] = ft->mtime;
		DEBUG(6,("**** onefs_ntimes: modtime: %s.%d\n",
			time_to_asc(convert_timespec_to_time_t(ft->mtime)),
			ft->mtime.tv_nsec));
	}

	if (!null_timespec(ft->create_time)) {
		flags |= VT_BTIME;
		times[2] = ft->create_time;
		DEBUG(6,("**** onefs_ntimes: createtime: %s.%d\n",
		   time_to_asc(convert_timespec_to_time_t(ft->create_time)),
		   ft->create_time.tv_nsec));
	}

	return onefs_vtimes_streams(handle, smb_fname, flags, times);
}

static uint32_t onefs_fs_capabilities(struct vfs_handle_struct *handle, enum timestamp_set_resolution *p_ts_res)
{
	uint32_t result = 0;

	if (!lp_parm_bool(SNUM(handle->conn), PARM_ONEFS_TYPE,
		PARM_IGNORE_STREAMS, PARM_IGNORE_STREAMS_DEFAULT)) {
		result |= FILE_NAMED_STREAMS;
	}

	result |= SMB_VFS_NEXT_FS_CAPABILITIES(handle, p_ts_res);
	*p_ts_res = TIMESTAMP_SET_MSEC;
	return result;
}

static struct vfs_fn_pointers onefs_fns = {
	.connect_fn = onefs_connect,
	.fs_capabilities = onefs_fs_capabilities,
	.opendir = onefs_opendir,
	.readdir = onefs_readdir,
	.seekdir = onefs_seekdir,
	.telldir = onefs_telldir,
	.rewind_dir = onefs_rewinddir,
	.mkdir = onefs_mkdir,
	.closedir = onefs_closedir,
	.init_search_op = onefs_init_search_op,
	.open_fn = onefs_open,
	.create_file = onefs_create_file,
	.close_fn = onefs_close,
	.sendfile = onefs_sendfile,
	.recvfile = onefs_recvfile,
	.rename = onefs_rename,
	.stat = onefs_stat,
	.fstat = onefs_fstat,
	.lstat = onefs_lstat,
	.get_alloc_size = onefs_get_alloc_size,
	.unlink = onefs_unlink,
	.ntimes = onefs_ntimes,
	.file_id_create = onefs_file_id_create,
	.streaminfo = onefs_streaminfo,
	.brl_lock_windows = onefs_brl_lock_windows,
	.brl_unlock_windows = onefs_brl_unlock_windows,
	.brl_cancel_windows = onefs_brl_cancel_windows,
	.strict_lock = onefs_strict_lock,
	.strict_unlock = onefs_strict_unlock,
	.notify_watch = onefs_notify_watch,
	.fget_nt_acl = onefs_fget_nt_acl,
	.get_nt_acl = onefs_get_nt_acl,
	.fset_nt_acl = onefs_fset_nt_acl,
	.statvfs = onefs_statvfs,
	.get_real_filename = onefs_get_real_filename,
};

NTSTATUS vfs_onefs_init(void)
{
	return smb_register_vfs(SMB_VFS_INTERFACE_VERSION, "onefs",
				&onefs_fns);
}
