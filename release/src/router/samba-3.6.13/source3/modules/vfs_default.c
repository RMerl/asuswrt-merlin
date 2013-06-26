/*
   Unix SMB/CIFS implementation.
   Wrap disk only vfs functions to sidestep dodgy compilers.
   Copyright (C) Tim Potter 1998
   Copyright (C) Jeremy Allison 2007

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
#include "system/time.h"
#include "system/filesys.h"
#include "smbd/smbd.h"
#include "ntioctl.h"
#include "smbprofile.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_VFS

/* Check for NULL pointer parameters in vfswrap_* functions */

/* We don't want to have NULL function pointers lying around.  Someone
   is sure to try and execute them.  These stubs are used to prevent
   this possibility. */

static int vfswrap_connect(vfs_handle_struct *handle,  const char *service, const char *user)
{
    return 0;    /* Return >= 0 for success */
}

static void vfswrap_disconnect(vfs_handle_struct *handle)
{
}

/* Disk operations */

static uint64_t vfswrap_disk_free(vfs_handle_struct *handle,  const char *path, bool small_query, uint64_t *bsize,
			       uint64_t *dfree, uint64_t *dsize)
{
	uint64_t result;

	result = sys_disk_free(handle->conn, path, small_query, bsize, dfree, dsize);
	return result;
}

static int vfswrap_get_quota(struct vfs_handle_struct *handle,  enum SMB_QUOTA_TYPE qtype, unid_t id, SMB_DISK_QUOTA *qt)
{
#ifdef HAVE_SYS_QUOTAS
	int result;

	START_PROFILE(syscall_get_quota);
	result = sys_get_quota(handle->conn->connectpath, qtype, id, qt);
	END_PROFILE(syscall_get_quota);
	return result;
#else
	errno = ENOSYS;
	return -1;
#endif
}

static int vfswrap_set_quota(struct vfs_handle_struct *handle,  enum SMB_QUOTA_TYPE qtype, unid_t id, SMB_DISK_QUOTA *qt)
{
#ifdef HAVE_SYS_QUOTAS
	int result;

	START_PROFILE(syscall_set_quota);
	result = sys_set_quota(handle->conn->connectpath, qtype, id, qt);
	END_PROFILE(syscall_set_quota);
	return result;
#else
	errno = ENOSYS;
	return -1;
#endif
}

static int vfswrap_get_shadow_copy_data(struct vfs_handle_struct *handle,
					struct files_struct *fsp,
					struct shadow_copy_data *shadow_copy_data,
					bool labels)
{
	errno = ENOSYS;
	return -1;  /* Not implemented. */
}

static int vfswrap_statvfs(struct vfs_handle_struct *handle,  const char *path, vfs_statvfs_struct *statbuf)
{
	return sys_statvfs(path, statbuf);
}

static uint32_t vfswrap_fs_capabilities(struct vfs_handle_struct *handle,
		enum timestamp_set_resolution *p_ts_res)
{
	connection_struct *conn = handle->conn;
	uint32_t caps = FILE_CASE_SENSITIVE_SEARCH | FILE_CASE_PRESERVED_NAMES;
	struct smb_filename *smb_fname_cpath = NULL;
	NTSTATUS status;
	int ret = -1;

#if defined(DARWINOS)
	struct vfs_statvfs_struct statbuf;
	ZERO_STRUCT(statbuf);
	sys_statvfs(conn->connectpath, &statbuf);
	caps = statbuf.FsCapabilities;
#endif

	*p_ts_res = TIMESTAMP_SET_SECONDS;

	/* Work out what timestamp resolution we can
	 * use when setting a timestamp. */

	status = create_synthetic_smb_fname(talloc_tos(),
				conn->connectpath,
				NULL,
				NULL,
				&smb_fname_cpath);
	if (!NT_STATUS_IS_OK(status)) {
		return caps;
	}

	ret = SMB_VFS_STAT(conn, smb_fname_cpath);
	if (ret == -1) {
		TALLOC_FREE(smb_fname_cpath);
		return caps;
	}

	if (smb_fname_cpath->st.st_ex_mtime.tv_nsec ||
			smb_fname_cpath->st.st_ex_atime.tv_nsec ||
			smb_fname_cpath->st.st_ex_ctime.tv_nsec) {
		/* If any of the normal UNIX directory timestamps
		 * have a non-zero tv_nsec component assume
		 * we might be able to set sub-second timestamps.
		 * See what filetime set primitives we have.
		 */
#if defined(HAVE_UTIMENSAT)
		*p_ts_res = TIMESTAMP_SET_NT_OR_BETTER;
#elif defined(HAVE_UTIMES)
		/* utimes allows msec timestamps to be set. */
		*p_ts_res = TIMESTAMP_SET_MSEC;
#elif defined(HAVE_UTIME)
		/* utime only allows sec timestamps to be set. */
		*p_ts_res = TIMESTAMP_SET_SECONDS;
#endif

		DEBUG(10,("vfswrap_fs_capabilities: timestamp "
			"resolution of %s "
			"available on share %s, directory %s\n",
			*p_ts_res == TIMESTAMP_SET_MSEC ? "msec" : "sec",
			lp_servicename(conn->params->service),
			conn->connectpath ));
	}
	TALLOC_FREE(smb_fname_cpath);
	return caps;
}

/* Directory operations */

static SMB_STRUCT_DIR *vfswrap_opendir(vfs_handle_struct *handle,  const char *fname, const char *mask, uint32 attr)
{
	SMB_STRUCT_DIR *result;

	START_PROFILE(syscall_opendir);
	result = sys_opendir(fname);
	END_PROFILE(syscall_opendir);
	return result;
}

static SMB_STRUCT_DIR *vfswrap_fdopendir(vfs_handle_struct *handle,
			files_struct *fsp,
			const char *mask,
			uint32 attr)
{
	SMB_STRUCT_DIR *result;

	START_PROFILE(syscall_fdopendir);
	result = sys_fdopendir(fsp->fh->fd);
	END_PROFILE(syscall_fdopendir);
	return result;
}


static SMB_STRUCT_DIRENT *vfswrap_readdir(vfs_handle_struct *handle,
				          SMB_STRUCT_DIR *dirp,
					  SMB_STRUCT_STAT *sbuf)
{
	SMB_STRUCT_DIRENT *result;

	START_PROFILE(syscall_readdir);
	result = sys_readdir(dirp);
	/* Default Posix readdir() does not give us stat info.
	 * Set to invalid to indicate we didn't return this info. */
	if (sbuf)
		SET_STAT_INVALID(*sbuf);
	END_PROFILE(syscall_readdir);
	return result;
}

static void vfswrap_seekdir(vfs_handle_struct *handle,  SMB_STRUCT_DIR *dirp, long offset)
{
	START_PROFILE(syscall_seekdir);
	sys_seekdir(dirp, offset);
	END_PROFILE(syscall_seekdir);
}

static long vfswrap_telldir(vfs_handle_struct *handle,  SMB_STRUCT_DIR *dirp)
{
	long result;
	START_PROFILE(syscall_telldir);
	result = sys_telldir(dirp);
	END_PROFILE(syscall_telldir);
	return result;
}

static void vfswrap_rewinddir(vfs_handle_struct *handle,  SMB_STRUCT_DIR *dirp)
{
	START_PROFILE(syscall_rewinddir);
	sys_rewinddir(dirp);
	END_PROFILE(syscall_rewinddir);
}

static int vfswrap_mkdir(vfs_handle_struct *handle,  const char *path, mode_t mode)
{
	int result;
	bool has_dacl = False;
	char *parent = NULL;

	START_PROFILE(syscall_mkdir);

	if (lp_inherit_acls(SNUM(handle->conn))
	    && parent_dirname(talloc_tos(), path, &parent, NULL)
	    && (has_dacl = directory_has_default_acl(handle->conn, parent)))
		mode = (0777 & lp_dir_mask(SNUM(handle->conn)));

	TALLOC_FREE(parent);

	result = mkdir(path, mode);

	if (result == 0 && !has_dacl) {
		/*
		 * We need to do this as the default behavior of POSIX ACLs
		 * is to set the mask to be the requested group permission
		 * bits, not the group permission bits to be the requested
		 * group permission bits. This is not what we want, as it will
		 * mess up any inherited ACL bits that were set. JRA.
		 */
		int saved_errno = errno; /* We may get ENOSYS */
		if ((SMB_VFS_CHMOD_ACL(handle->conn, path, mode) == -1) && (errno == ENOSYS))
			errno = saved_errno;
	}

	END_PROFILE(syscall_mkdir);
	return result;
}

static int vfswrap_rmdir(vfs_handle_struct *handle,  const char *path)
{
	int result;

	START_PROFILE(syscall_rmdir);
	result = rmdir(path);
	END_PROFILE(syscall_rmdir);
	return result;
}

static int vfswrap_closedir(vfs_handle_struct *handle,  SMB_STRUCT_DIR *dirp)
{
	int result;

	START_PROFILE(syscall_closedir);
	result = sys_closedir(dirp);
	END_PROFILE(syscall_closedir);
	return result;
}

static void vfswrap_init_search_op(vfs_handle_struct *handle,
				   SMB_STRUCT_DIR *dirp)
{
	/* Default behavior is a NOOP */
}

/* File operations */

static int vfswrap_open(vfs_handle_struct *handle,
			struct smb_filename *smb_fname,
			files_struct *fsp, int flags, mode_t mode)
{
	int result = -1;

	START_PROFILE(syscall_open);

	if (smb_fname->stream_name) {
		errno = ENOENT;
		goto out;
	}

	result = sys_open(smb_fname->base_name, flags, mode);
 out:
	END_PROFILE(syscall_open);
	return result;
}

static NTSTATUS vfswrap_create_file(vfs_handle_struct *handle,
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
	return create_file_default(handle->conn, req, root_dir_fid, smb_fname,
				   access_mask, share_access,
				   create_disposition, create_options,
				   file_attributes, oplock_request,
				   allocation_size, private_flags,
				   sd, ea_list, result,
				   pinfo);
}

static int vfswrap_close(vfs_handle_struct *handle, files_struct *fsp)
{
	int result;

	START_PROFILE(syscall_close);
	result = fd_close_posix(fsp);
	END_PROFILE(syscall_close);
	return result;
}

static ssize_t vfswrap_read(vfs_handle_struct *handle, files_struct *fsp, void *data, size_t n)
{
	ssize_t result;

	START_PROFILE_BYTES(syscall_read, n);
	result = sys_read(fsp->fh->fd, data, n);
	END_PROFILE(syscall_read);
	return result;
}

static ssize_t vfswrap_pread(vfs_handle_struct *handle, files_struct *fsp, void *data,
			size_t n, SMB_OFF_T offset)
{
	ssize_t result;

#if defined(HAVE_PREAD) || defined(HAVE_PREAD64)
	START_PROFILE_BYTES(syscall_pread, n);
	result = sys_pread(fsp->fh->fd, data, n, offset);
	END_PROFILE(syscall_pread);

	if (result == -1 && errno == ESPIPE) {
		/* Maintain the fiction that pipes can be seeked (sought?) on. */
		result = SMB_VFS_READ(fsp, data, n);
		fsp->fh->pos = 0;
	}

#else /* HAVE_PREAD */
	SMB_OFF_T   curr;
	int lerrno;

	curr = SMB_VFS_LSEEK(fsp, 0, SEEK_CUR);
	if (curr == -1 && errno == ESPIPE) {
		/* Maintain the fiction that pipes can be seeked (sought?) on. */
		result = SMB_VFS_READ(fsp, data, n);
		fsp->fh->pos = 0;
		return result;
	}

	if (SMB_VFS_LSEEK(fsp, offset, SEEK_SET) == -1) {
		return -1;
	}

	errno = 0;
	result = SMB_VFS_READ(fsp, data, n);
	lerrno = errno;

	SMB_VFS_LSEEK(fsp, curr, SEEK_SET);
	errno = lerrno;

#endif /* HAVE_PREAD */

	return result;
}

static ssize_t vfswrap_write(vfs_handle_struct *handle, files_struct *fsp, const void *data, size_t n)
{
	ssize_t result;

	START_PROFILE_BYTES(syscall_write, n);
	result = sys_write(fsp->fh->fd, data, n);
	END_PROFILE(syscall_write);
	return result;
}

static ssize_t vfswrap_pwrite(vfs_handle_struct *handle, files_struct *fsp, const void *data,
			size_t n, SMB_OFF_T offset)
{
	ssize_t result;

#if defined(HAVE_PWRITE) || defined(HAVE_PRWITE64)
	START_PROFILE_BYTES(syscall_pwrite, n);
	result = sys_pwrite(fsp->fh->fd, data, n, offset);
	END_PROFILE(syscall_pwrite);

	if (result == -1 && errno == ESPIPE) {
		/* Maintain the fiction that pipes can be sought on. */
		result = SMB_VFS_WRITE(fsp, data, n);
	}

#else /* HAVE_PWRITE */
	SMB_OFF_T   curr;
	int         lerrno;

	curr = SMB_VFS_LSEEK(fsp, 0, SEEK_CUR);
	if (curr == -1) {
		return -1;
	}

	if (SMB_VFS_LSEEK(fsp, offset, SEEK_SET) == -1) {
		return -1;
	}

	result = SMB_VFS_WRITE(fsp, data, n);
	lerrno = errno;

	SMB_VFS_LSEEK(fsp, curr, SEEK_SET);
	errno = lerrno;

#endif /* HAVE_PWRITE */

	return result;
}

static SMB_OFF_T vfswrap_lseek(vfs_handle_struct *handle, files_struct *fsp, SMB_OFF_T offset, int whence)
{
	SMB_OFF_T result = 0;

	START_PROFILE(syscall_lseek);

	/* Cope with 'stat' file opens. */
	if (fsp->fh->fd != -1)
		result = sys_lseek(fsp->fh->fd, offset, whence);

	/*
	 * We want to maintain the fiction that we can seek
	 * on a fifo for file system purposes. This allows
	 * people to set up UNIX fifo's that feed data to Windows
	 * applications. JRA.
	 */

	if((result == -1) && (errno == ESPIPE)) {
		result = 0;
		errno = 0;
	}

	END_PROFILE(syscall_lseek);
	return result;
}

static ssize_t vfswrap_sendfile(vfs_handle_struct *handle, int tofd, files_struct *fromfsp, const DATA_BLOB *hdr,
			SMB_OFF_T offset, size_t n)
{
	ssize_t result;

	START_PROFILE_BYTES(syscall_sendfile, n);
	result = sys_sendfile(tofd, fromfsp->fh->fd, hdr, offset, n);
	END_PROFILE(syscall_sendfile);
	return result;
}

static ssize_t vfswrap_recvfile(vfs_handle_struct *handle,
			int fromfd,
			files_struct *tofsp,
			SMB_OFF_T offset,
			size_t n)
{
	ssize_t result;

	START_PROFILE_BYTES(syscall_recvfile, n);
	result = sys_recvfile(fromfd, tofsp->fh->fd, offset, n);
	END_PROFILE(syscall_recvfile);
	return result;
}

static int vfswrap_rename(vfs_handle_struct *handle,
			  const struct smb_filename *smb_fname_src,
			  const struct smb_filename *smb_fname_dst)
{
	int result = -1;

	START_PROFILE(syscall_rename);

	if (smb_fname_src->stream_name || smb_fname_dst->stream_name) {
		errno = ENOENT;
		goto out;
	}

	result = rename(smb_fname_src->base_name, smb_fname_dst->base_name);

 out:
	END_PROFILE(syscall_rename);
	return result;
}

static int vfswrap_fsync(vfs_handle_struct *handle, files_struct *fsp)
{
#ifdef HAVE_FSYNC
	int result;

	START_PROFILE(syscall_fsync);
	result = fsync(fsp->fh->fd);
	END_PROFILE(syscall_fsync);
	return result;
#else
	return 0;
#endif
}

static int vfswrap_stat(vfs_handle_struct *handle,
			struct smb_filename *smb_fname)
{
	int result = -1;

	START_PROFILE(syscall_stat);

	if (smb_fname->stream_name) {
		errno = ENOENT;
		goto out;
	}

	result = sys_stat(smb_fname->base_name, &smb_fname->st,
			  lp_fake_dir_create_times(SNUM(handle->conn)));
 out:
	END_PROFILE(syscall_stat);
	return result;
}

static int vfswrap_fstat(vfs_handle_struct *handle, files_struct *fsp, SMB_STRUCT_STAT *sbuf)
{
	int result;

	START_PROFILE(syscall_fstat);
	result = sys_fstat(fsp->fh->fd,
			   sbuf, lp_fake_dir_create_times(SNUM(handle->conn)));
	END_PROFILE(syscall_fstat);
	return result;
}

static int vfswrap_lstat(vfs_handle_struct *handle,
			 struct smb_filename *smb_fname)
{
	int result = -1;

	START_PROFILE(syscall_lstat);

	if (smb_fname->stream_name) {
		errno = ENOENT;
		goto out;
	}

	result = sys_lstat(smb_fname->base_name, &smb_fname->st,
			   lp_fake_dir_create_times(SNUM(handle->conn)));
 out:
	END_PROFILE(syscall_lstat);
	return result;
}

static NTSTATUS vfswrap_translate_name(struct vfs_handle_struct *handle,
				       const char *name,
				       enum vfs_translate_direction direction,
				       TALLOC_CTX *mem_ctx,
				       char **mapped_name)
{
	return NT_STATUS_NONE_MAPPED;
}

/********************************************************************
 Given a stat buffer return the allocated size on disk, taking into
 account sparse files.
********************************************************************/
static uint64_t vfswrap_get_alloc_size(vfs_handle_struct *handle,
				       struct files_struct *fsp,
				       const SMB_STRUCT_STAT *sbuf)
{
	uint64_t result;

	START_PROFILE(syscall_get_alloc_size);

	if(S_ISDIR(sbuf->st_ex_mode)) {
		result = 0;
		goto out;
	}

#if defined(HAVE_STAT_ST_BLOCKS) && defined(STAT_ST_BLOCKSIZE)
	result = (uint64_t)STAT_ST_BLOCKSIZE * (uint64_t)sbuf->st_ex_blocks;
#else
	result = get_file_size_stat(sbuf);
#endif

	if (fsp && fsp->initial_allocation_size)
		result = MAX(result,fsp->initial_allocation_size);

	result = smb_roundup(handle->conn, result);

 out:
	END_PROFILE(syscall_get_alloc_size);
	return result;
}

static int vfswrap_unlink(vfs_handle_struct *handle,
			  const struct smb_filename *smb_fname)
{
	int result = -1;

	START_PROFILE(syscall_unlink);

	if (smb_fname->stream_name) {
		errno = ENOENT;
		goto out;
	}
	result = unlink(smb_fname->base_name);

 out:
	END_PROFILE(syscall_unlink);
	return result;
}

static int vfswrap_chmod(vfs_handle_struct *handle,  const char *path, mode_t mode)
{
	int result;

	START_PROFILE(syscall_chmod);

	/*
	 * We need to do this due to the fact that the default POSIX ACL
	 * chmod modifies the ACL *mask* for the group owner, not the
	 * group owner bits directly. JRA.
	 */


	{
		int saved_errno = errno; /* We might get ENOSYS */
		if ((result = SMB_VFS_CHMOD_ACL(handle->conn, path, mode)) == 0) {
			END_PROFILE(syscall_chmod);
			return result;
		}
		/* Error - return the old errno. */
		errno = saved_errno;
	}

	result = chmod(path, mode);
	END_PROFILE(syscall_chmod);
	return result;
}

static int vfswrap_fchmod(vfs_handle_struct *handle, files_struct *fsp, mode_t mode)
{
	int result;

	START_PROFILE(syscall_fchmod);

	/*
	 * We need to do this due to the fact that the default POSIX ACL
	 * chmod modifies the ACL *mask* for the group owner, not the
	 * group owner bits directly. JRA.
	 */

	{
		int saved_errno = errno; /* We might get ENOSYS */
		if ((result = SMB_VFS_FCHMOD_ACL(fsp, mode)) == 0) {
			END_PROFILE(syscall_fchmod);
			return result;
		}
		/* Error - return the old errno. */
		errno = saved_errno;
	}

#if defined(HAVE_FCHMOD)
	result = fchmod(fsp->fh->fd, mode);
#else
	result = -1;
	errno = ENOSYS;
#endif

	END_PROFILE(syscall_fchmod);
	return result;
}

static int vfswrap_chown(vfs_handle_struct *handle, const char *path, uid_t uid, gid_t gid)
{
	int result;

	START_PROFILE(syscall_chown);
	result = chown(path, uid, gid);
	END_PROFILE(syscall_chown);
	return result;
}

static int vfswrap_fchown(vfs_handle_struct *handle, files_struct *fsp, uid_t uid, gid_t gid)
{
#ifdef HAVE_FCHOWN
	int result;

	START_PROFILE(syscall_fchown);
	result = fchown(fsp->fh->fd, uid, gid);
	END_PROFILE(syscall_fchown);
	return result;
#else
	errno = ENOSYS;
	return -1;
#endif
}

static int vfswrap_lchown(vfs_handle_struct *handle, const char *path, uid_t uid, gid_t gid)
{
	int result;

	START_PROFILE(syscall_lchown);
	result = lchown(path, uid, gid);
	END_PROFILE(syscall_lchown);
	return result;
}

static int vfswrap_chdir(vfs_handle_struct *handle,  const char *path)
{
	int result;

	START_PROFILE(syscall_chdir);
	result = chdir(path);
	END_PROFILE(syscall_chdir);
	return result;
}

static char *vfswrap_getwd(vfs_handle_struct *handle,  char *path)
{
	char *result;

	START_PROFILE(syscall_getwd);
	result = sys_getwd(path);
	END_PROFILE(syscall_getwd);
	return result;
}

/*********************************************************************
 nsec timestamp resolution call. Convert down to whatever the underlying
 system will support.
**********************************************************************/

static int vfswrap_ntimes(vfs_handle_struct *handle,
			  const struct smb_filename *smb_fname,
			  struct smb_file_time *ft)
{
	int result = -1;

	START_PROFILE(syscall_ntimes);

	if (smb_fname->stream_name) {
		errno = ENOENT;
		goto out;
	}

	if (ft != NULL) {
		if (null_timespec(ft->atime)) {
			ft->atime= smb_fname->st.st_ex_atime;
		}

		if (null_timespec(ft->mtime)) {
			ft->mtime = smb_fname->st.st_ex_mtime;
		}

		if (!null_timespec(ft->create_time)) {
			set_create_timespec_ea(handle->conn,
					       smb_fname,
					       ft->create_time);
		}

		if ((timespec_compare(&ft->atime,
				      &smb_fname->st.st_ex_atime) == 0) &&
		    (timespec_compare(&ft->mtime,
				      &smb_fname->st.st_ex_mtime) == 0)) {
			return 0;
		}
	}

#if defined(HAVE_UTIMENSAT)
	if (ft != NULL) {
		struct timespec ts[2];
		ts[0] = ft->atime;
		ts[1] = ft->mtime;
		result = utimensat(AT_FDCWD, smb_fname->base_name, ts, 0);
	} else {
		result = utimensat(AT_FDCWD, smb_fname->base_name, NULL, 0);
	}
	if (!((result == -1) && (errno == ENOSYS))) {
		goto out;
	}
#endif
#if defined(HAVE_UTIMES)
	if (ft != NULL) {
		struct timeval tv[2];
		tv[0] = convert_timespec_to_timeval(ft->atime);
		tv[1] = convert_timespec_to_timeval(ft->mtime);
		result = utimes(smb_fname->base_name, tv);
	} else {
		result = utimes(smb_fname->base_name, NULL);
	}
	if (!((result == -1) && (errno == ENOSYS))) {
		goto out;
	}
#endif
#if defined(HAVE_UTIME)
	if (ft != NULL) {
		struct utimbuf times;
		times.actime = convert_timespec_to_time_t(ft->atime);
		times.modtime = convert_timespec_to_time_t(ft->mtime);
		result = utime(smb_fname->base_name, &times);
	} else {
		result = utime(smb_fname->base_name, NULL);
	}
	if (!((result == -1) && (errno == ENOSYS))) {
		goto out;
	}
#endif
	errno = ENOSYS;
	result = -1;

 out:
	END_PROFILE(syscall_ntimes);
	return result;
}

/*********************************************************************
 A version of ftruncate that will write the space on disk if strict
 allocate is set.
**********************************************************************/

static int strict_allocate_ftruncate(vfs_handle_struct *handle, files_struct *fsp, SMB_OFF_T len)
{
	SMB_OFF_T space_to_write;
	uint64_t space_avail;
	uint64_t bsize,dfree,dsize;
	int ret;
	NTSTATUS status;
	SMB_STRUCT_STAT *pst;

	status = vfs_stat_fsp(fsp);
	if (!NT_STATUS_IS_OK(status)) {
		return -1;
	}
	pst = &fsp->fsp_name->st;

#ifdef S_ISFIFO
	if (S_ISFIFO(pst->st_ex_mode))
		return 0;
#endif

	if (pst->st_ex_size == len)
		return 0;

	/* Shrink - just ftruncate. */
	if (pst->st_ex_size > len)
		return sys_ftruncate(fsp->fh->fd, len);

	space_to_write = len - pst->st_ex_size;

	/* for allocation try fallocate first. This can fail on some
	   platforms e.g. when the filesystem doesn't support it and no
	   emulation is being done by the libc (like on AIX with JFS1). In that
	   case we do our own emulation. fallocate implementations can
	   return ENOTSUP or EINVAL in cases like that. */
	ret = SMB_VFS_FALLOCATE(fsp, VFS_FALLOCATE_EXTEND_SIZE,
				pst->st_ex_size, space_to_write);
	if (ret == ENOSPC) {
		errno = ENOSPC;
		return -1;
	}
	if (ret == 0) {
		return 0;
	}
	DEBUG(10,("strict_allocate_ftruncate: SMB_VFS_FALLOCATE failed with "
		"error %d. Falling back to slow manual allocation\n", ret));

	/* available disk space is enough or not? */
	space_avail = get_dfree_info(fsp->conn,
				     fsp->fsp_name->base_name, false,
				     &bsize,&dfree,&dsize);
	/* space_avail is 1k blocks */
	if (space_avail == (uint64_t)-1 ||
			((uint64_t)space_to_write/1024 > space_avail) ) {
		errno = ENOSPC;
		return -1;
	}

	/* Write out the real space on disk. */
	ret = vfs_slow_fallocate(fsp, pst->st_ex_size, space_to_write);
	if (ret != 0) {
		errno = ret;
		ret = -1;
	}

	return 0;
}

static int vfswrap_ftruncate(vfs_handle_struct *handle, files_struct *fsp, SMB_OFF_T len)
{
	int result = -1;
	SMB_STRUCT_STAT *pst;
	NTSTATUS status;
	char c = 0;

	START_PROFILE(syscall_ftruncate);

	if (lp_strict_allocate(SNUM(fsp->conn)) && !fsp->is_sparse) {
		result = strict_allocate_ftruncate(handle, fsp, len);
		END_PROFILE(syscall_ftruncate);
		return result;
	}

	/* we used to just check HAVE_FTRUNCATE_EXTEND and only use
	   sys_ftruncate if the system supports it. Then I discovered that
	   you can have some filesystems that support ftruncate
	   expansion and some that don't! On Linux fat can't do
	   ftruncate extend but ext2 can. */

	result = sys_ftruncate(fsp->fh->fd, len);
	if (result == 0)
		goto done;

	/* According to W. R. Stevens advanced UNIX prog. Pure 4.3 BSD cannot
	   extend a file with ftruncate. Provide alternate implementation
	   for this */

	/* Do an fstat to see if the file is longer than the requested
	   size in which case the ftruncate above should have
	   succeeded or shorter, in which case seek to len - 1 and
	   write 1 byte of zero */
	status = vfs_stat_fsp(fsp);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	pst = &fsp->fsp_name->st;

#ifdef S_ISFIFO
	if (S_ISFIFO(pst->st_ex_mode)) {
		result = 0;
		goto done;
	}
#endif

	if (pst->st_ex_size == len) {
		result = 0;
		goto done;
	}

	if (pst->st_ex_size > len) {
		/* the sys_ftruncate should have worked */
		goto done;
	}

	if (SMB_VFS_PWRITE(fsp, &c, 1, len-1)!=1) {
		goto done;
	}

	result = 0;

  done:

	END_PROFILE(syscall_ftruncate);
	return result;
}

static int vfswrap_fallocate(vfs_handle_struct *handle,
			files_struct *fsp,
			enum vfs_fallocate_mode mode,
			SMB_OFF_T offset,
			SMB_OFF_T len)
{
	int result;

	START_PROFILE(syscall_fallocate);
	if (mode == VFS_FALLOCATE_EXTEND_SIZE) {
		result = sys_posix_fallocate(fsp->fh->fd, offset, len);
	} else if (mode == VFS_FALLOCATE_KEEP_SIZE) {
		result = sys_fallocate(fsp->fh->fd, mode, offset, len);
	} else {
		errno = EINVAL;
		result = -1;
	}
	END_PROFILE(syscall_fallocate);
	return result;
}

static bool vfswrap_lock(vfs_handle_struct *handle, files_struct *fsp, int op, SMB_OFF_T offset, SMB_OFF_T count, int type)
{
	bool result;

	START_PROFILE(syscall_fcntl_lock);
	result =  fcntl_lock(fsp->fh->fd, op, offset, count, type);
	END_PROFILE(syscall_fcntl_lock);
	return result;
}

static int vfswrap_kernel_flock(vfs_handle_struct *handle, files_struct *fsp,
				uint32 share_mode, uint32 access_mask)
{
	START_PROFILE(syscall_kernel_flock);
	kernel_flock(fsp->fh->fd, share_mode, access_mask);
	END_PROFILE(syscall_kernel_flock);
	return 0;
}

static bool vfswrap_getlock(vfs_handle_struct *handle, files_struct *fsp, SMB_OFF_T *poffset, SMB_OFF_T *pcount, int *ptype, pid_t *ppid)
{
	bool result;

	START_PROFILE(syscall_fcntl_getlock);
	result =  fcntl_getlock(fsp->fh->fd, poffset, pcount, ptype, ppid);
	END_PROFILE(syscall_fcntl_getlock);
	return result;
}

static int vfswrap_linux_setlease(vfs_handle_struct *handle, files_struct *fsp,
				int leasetype)
{
	int result = -1;

	START_PROFILE(syscall_linux_setlease);

#ifdef HAVE_KERNEL_OPLOCKS_LINUX
	result = linux_setlease(fsp->fh->fd, leasetype);
#else
	errno = ENOSYS;
#endif
	END_PROFILE(syscall_linux_setlease);
	return result;
}

static int vfswrap_symlink(vfs_handle_struct *handle,  const char *oldpath, const char *newpath)
{
	int result;

	START_PROFILE(syscall_symlink);
	result = symlink(oldpath, newpath);
	END_PROFILE(syscall_symlink);
	return result;
}

static int vfswrap_readlink(vfs_handle_struct *handle,  const char *path, char *buf, size_t bufsiz)
{
	int result;

	START_PROFILE(syscall_readlink);
	result = readlink(path, buf, bufsiz);
	END_PROFILE(syscall_readlink);
	return result;
}

static int vfswrap_link(vfs_handle_struct *handle,  const char *oldpath, const char *newpath)
{
	int result;

	START_PROFILE(syscall_link);
	result = link(oldpath, newpath);
	END_PROFILE(syscall_link);
	return result;
}

static int vfswrap_mknod(vfs_handle_struct *handle,  const char *pathname, mode_t mode, SMB_DEV_T dev)
{
	int result;

	START_PROFILE(syscall_mknod);
	result = sys_mknod(pathname, mode, dev);
	END_PROFILE(syscall_mknod);
	return result;
}

static char *vfswrap_realpath(vfs_handle_struct *handle,  const char *path)
{
	char *result;

	START_PROFILE(syscall_realpath);
#ifdef REALPATH_TAKES_NULL
	result = realpath(path, NULL);
#else
	result = SMB_MALLOC_ARRAY(char, PATH_MAX+1);
	if (result) {
		char *resolved_path = realpath(path, result);
		if (!resolved_path) {
			SAFE_FREE(result);
		} else {
			/* SMB_ASSERT(result == resolved_path) ? */
			result = resolved_path;
		}
	}
#endif
	END_PROFILE(syscall_realpath);
	return result;
}

static NTSTATUS vfswrap_notify_watch(vfs_handle_struct *vfs_handle,
				     struct sys_notify_context *ctx,
				     struct notify_entry *e,
				     void (*callback)(struct sys_notify_context *ctx, 
						      void *private_data,
						      struct notify_event *ev),
				     void *private_data, void *handle)
{
	/*
	 * So far inotify is the only supported default notify mechanism. If
	 * another platform like the the BSD's or a proprietary Unix comes
	 * along and wants another default, we can play the same trick we
	 * played with Posix ACLs.
	 *
	 * Until that is the case, hard-code inotify here.
	 */
#ifdef HAVE_INOTIFY
	if (lp_kernel_change_notify(ctx->conn->params)) {
		return inotify_watch(ctx, e, callback, private_data, handle);
	}
#endif
	/*
	 * Do nothing, leave everything to notify_internal.c
	 */
	return NT_STATUS_OK;
}

static int vfswrap_chflags(vfs_handle_struct *handle, const char *path,
			   unsigned int flags)
{
#ifdef HAVE_CHFLAGS
	return chflags(path, flags);
#else
	errno = ENOSYS;
	return -1;
#endif
}

static struct file_id vfswrap_file_id_create(struct vfs_handle_struct *handle,
					     const SMB_STRUCT_STAT *sbuf)
{
	struct file_id key;

	/* the ZERO_STRUCT ensures padding doesn't break using the key as a
	 * blob */
	ZERO_STRUCT(key);

	key.devid = sbuf->st_ex_dev;
	key.inode = sbuf->st_ex_ino;
	/* key.extid is unused by default. */

	return key;
}

static NTSTATUS vfswrap_streaminfo(vfs_handle_struct *handle,
				   struct files_struct *fsp,
				   const char *fname,
				   TALLOC_CTX *mem_ctx,
				   unsigned int *pnum_streams,
				   struct stream_struct **pstreams)
{
	SMB_STRUCT_STAT sbuf;
	struct stream_struct *tmp_streams = NULL;
	int ret;

	if ((fsp != NULL) && (fsp->is_directory)) {
		/*
		 * No default streams on directories
		 */
		goto done;
	}

	if ((fsp != NULL) && (fsp->fh->fd != -1)) {
		ret = SMB_VFS_FSTAT(fsp, &sbuf);
	}
	else {
		struct smb_filename smb_fname;

		ZERO_STRUCT(smb_fname);
		smb_fname.base_name = discard_const_p(char, fname);

		if (lp_posix_pathnames()) {
			ret = SMB_VFS_LSTAT(handle->conn, &smb_fname);
		} else {
			ret = SMB_VFS_STAT(handle->conn, &smb_fname);
		}
		sbuf = smb_fname.st;
	}

	if (ret == -1) {
		return map_nt_error_from_unix(errno);
	}

	if (S_ISDIR(sbuf.st_ex_mode)) {
		goto done;
	}

	tmp_streams = talloc_realloc(mem_ctx, *pstreams, struct stream_struct,
					(*pnum_streams) + 1);
	if (tmp_streams == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	tmp_streams[*pnum_streams].name = talloc_strdup(tmp_streams, "::$DATA");
	if (tmp_streams[*pnum_streams].name == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	tmp_streams[*pnum_streams].size = sbuf.st_ex_size;
	tmp_streams[*pnum_streams].alloc_size = SMB_VFS_GET_ALLOC_SIZE(handle->conn, fsp, &sbuf);

	*pnum_streams += 1;
	*pstreams = tmp_streams;
 done:
	return NT_STATUS_OK;
}

static int vfswrap_get_real_filename(struct vfs_handle_struct *handle,
				     const char *path,
				     const char *name,
				     TALLOC_CTX *mem_ctx,
				     char **found_name)
{
	/*
	 * Don't fall back to get_real_filename so callers can differentiate
	 * between a full directory scan and an actual case-insensitive stat.
	 */
	errno = EOPNOTSUPP;
	return -1;
}

static const char *vfswrap_connectpath(struct vfs_handle_struct *handle,
				       const char *fname)
{
	return handle->conn->connectpath;
}

static NTSTATUS vfswrap_brl_lock_windows(struct vfs_handle_struct *handle,
					 struct byte_range_lock *br_lck,
					 struct lock_struct *plock,
					 bool blocking_lock,
					 struct blocking_lock_record *blr)
{
	SMB_ASSERT(plock->lock_flav == WINDOWS_LOCK);

	/* Note: blr is not used in the default implementation. */
	return brl_lock_windows_default(br_lck, plock, blocking_lock);
}

static bool vfswrap_brl_unlock_windows(struct vfs_handle_struct *handle,
				       struct messaging_context *msg_ctx,
				       struct byte_range_lock *br_lck,
			               const struct lock_struct *plock)
{
	SMB_ASSERT(plock->lock_flav == WINDOWS_LOCK);

	return brl_unlock_windows_default(msg_ctx, br_lck, plock);
}

static bool vfswrap_brl_cancel_windows(struct vfs_handle_struct *handle,
				       struct byte_range_lock *br_lck,
				       struct lock_struct *plock,
				       struct blocking_lock_record *blr)
{
	SMB_ASSERT(plock->lock_flav == WINDOWS_LOCK);

	/* Note: blr is not used in the default implementation. */
	return brl_lock_cancel_default(br_lck, plock);
}

static bool vfswrap_strict_lock(struct vfs_handle_struct *handle,
				files_struct *fsp,
				struct lock_struct *plock)
{
	SMB_ASSERT(plock->lock_type == READ_LOCK ||
	    plock->lock_type == WRITE_LOCK);

	return strict_lock_default(fsp, plock);
}

static void vfswrap_strict_unlock(struct vfs_handle_struct *handle,
				files_struct *fsp,
				struct lock_struct *plock)
{
	SMB_ASSERT(plock->lock_type == READ_LOCK ||
	    plock->lock_type == WRITE_LOCK);

	strict_unlock_default(fsp, plock);
}

/* NT ACL operations. */

static NTSTATUS vfswrap_fget_nt_acl(vfs_handle_struct *handle,
				    files_struct *fsp,
				    uint32 security_info,
				    struct security_descriptor **ppdesc)
{
	NTSTATUS result;

	START_PROFILE(fget_nt_acl);
	result = posix_fget_nt_acl(fsp, security_info, ppdesc);
	END_PROFILE(fget_nt_acl);
	return result;
}

static NTSTATUS vfswrap_get_nt_acl(vfs_handle_struct *handle,
				   const char *name,
				   uint32 security_info,
				   struct security_descriptor **ppdesc)
{
	NTSTATUS result;

	START_PROFILE(get_nt_acl);
	result = posix_get_nt_acl(handle->conn, name, security_info, ppdesc);
	END_PROFILE(get_nt_acl);
	return result;
}

static NTSTATUS vfswrap_fset_nt_acl(vfs_handle_struct *handle, files_struct *fsp, uint32 security_info_sent, const struct security_descriptor *psd)
{
	NTSTATUS result;

	START_PROFILE(fset_nt_acl);
	result = set_nt_acl(fsp, security_info_sent, psd);
	END_PROFILE(fset_nt_acl);
	return result;
}

static int vfswrap_chmod_acl(vfs_handle_struct *handle,  const char *name, mode_t mode)
{
#ifdef HAVE_NO_ACL
	errno = ENOSYS;
	return -1;
#else
	int result;

	START_PROFILE(chmod_acl);
	result = chmod_acl(handle->conn, name, mode);
	END_PROFILE(chmod_acl);
	return result;
#endif
}

static int vfswrap_fchmod_acl(vfs_handle_struct *handle, files_struct *fsp, mode_t mode)
{
#ifdef HAVE_NO_ACL
	errno = ENOSYS;
	return -1;
#else
	int result;

	START_PROFILE(fchmod_acl);
	result = fchmod_acl(fsp, mode);
	END_PROFILE(fchmod_acl);
	return result;
#endif
}

static int vfswrap_sys_acl_get_entry(vfs_handle_struct *handle,  SMB_ACL_T theacl, int entry_id, SMB_ACL_ENTRY_T *entry_p)
{
	return sys_acl_get_entry(theacl, entry_id, entry_p);
}

static int vfswrap_sys_acl_get_tag_type(vfs_handle_struct *handle,  SMB_ACL_ENTRY_T entry_d, SMB_ACL_TAG_T *tag_type_p)
{
	return sys_acl_get_tag_type(entry_d, tag_type_p);
}

static int vfswrap_sys_acl_get_permset(vfs_handle_struct *handle,  SMB_ACL_ENTRY_T entry_d, SMB_ACL_PERMSET_T *permset_p)
{
	return sys_acl_get_permset(entry_d, permset_p);
}

static void * vfswrap_sys_acl_get_qualifier(vfs_handle_struct *handle,  SMB_ACL_ENTRY_T entry_d)
{
	return sys_acl_get_qualifier(entry_d);
}

static SMB_ACL_T vfswrap_sys_acl_get_file(vfs_handle_struct *handle,  const char *path_p, SMB_ACL_TYPE_T type)
{
	return sys_acl_get_file(handle, path_p, type);
}

static SMB_ACL_T vfswrap_sys_acl_get_fd(vfs_handle_struct *handle, files_struct *fsp)
{
	return sys_acl_get_fd(handle, fsp);
}

static int vfswrap_sys_acl_clear_perms(vfs_handle_struct *handle,  SMB_ACL_PERMSET_T permset)
{
	return sys_acl_clear_perms(permset);
}

static int vfswrap_sys_acl_add_perm(vfs_handle_struct *handle,  SMB_ACL_PERMSET_T permset, SMB_ACL_PERM_T perm)
{
	return sys_acl_add_perm(permset, perm);
}

static char * vfswrap_sys_acl_to_text(vfs_handle_struct *handle,  SMB_ACL_T theacl, ssize_t *plen)
{
	return sys_acl_to_text(theacl, plen);
}

static SMB_ACL_T vfswrap_sys_acl_init(vfs_handle_struct *handle,  int count)
{
	return sys_acl_init(count);
}

static int vfswrap_sys_acl_create_entry(vfs_handle_struct *handle,  SMB_ACL_T *pacl, SMB_ACL_ENTRY_T *pentry)
{
	return sys_acl_create_entry(pacl, pentry);
}

static int vfswrap_sys_acl_set_tag_type(vfs_handle_struct *handle,  SMB_ACL_ENTRY_T entry, SMB_ACL_TAG_T tagtype)
{
	return sys_acl_set_tag_type(entry, tagtype);
}

static int vfswrap_sys_acl_set_qualifier(vfs_handle_struct *handle,  SMB_ACL_ENTRY_T entry, void *qual)
{
	return sys_acl_set_qualifier(entry, qual);
}

static int vfswrap_sys_acl_set_permset(vfs_handle_struct *handle,  SMB_ACL_ENTRY_T entry, SMB_ACL_PERMSET_T permset)
{
	return sys_acl_set_permset(entry, permset);
}

static int vfswrap_sys_acl_valid(vfs_handle_struct *handle,  SMB_ACL_T theacl )
{
	return sys_acl_valid(theacl );
}

static int vfswrap_sys_acl_set_file(vfs_handle_struct *handle,  const char *name, SMB_ACL_TYPE_T acltype, SMB_ACL_T theacl)
{
	return sys_acl_set_file(handle, name, acltype, theacl);
}

static int vfswrap_sys_acl_set_fd(vfs_handle_struct *handle, files_struct *fsp, SMB_ACL_T theacl)
{
	return sys_acl_set_fd(handle, fsp, theacl);
}

static int vfswrap_sys_acl_delete_def_file(vfs_handle_struct *handle,  const char *path)
{
	return sys_acl_delete_def_file(handle, path);
}

static int vfswrap_sys_acl_get_perm(vfs_handle_struct *handle,  SMB_ACL_PERMSET_T permset, SMB_ACL_PERM_T perm)
{
	return sys_acl_get_perm(permset, perm);
}

static int vfswrap_sys_acl_free_text(vfs_handle_struct *handle,  char *text)
{
	return sys_acl_free_text(text);
}

static int vfswrap_sys_acl_free_acl(vfs_handle_struct *handle,  SMB_ACL_T posix_acl)
{
	return sys_acl_free_acl(posix_acl);
}

static int vfswrap_sys_acl_free_qualifier(vfs_handle_struct *handle,  void *qualifier, SMB_ACL_TAG_T tagtype)
{
	return sys_acl_free_qualifier(qualifier, tagtype);
}

/****************************************************************
 Extended attribute operations.
*****************************************************************/

static ssize_t vfswrap_getxattr(struct vfs_handle_struct *handle,const char *path, const char *name, void *value, size_t size)
{
	return sys_getxattr(path, name, value, size);
}

static ssize_t vfswrap_lgetxattr(struct vfs_handle_struct *handle,const char *path, const char *name, void *value, size_t size)
{
	return sys_lgetxattr(path, name, value, size);
}

static ssize_t vfswrap_fgetxattr(struct vfs_handle_struct *handle, struct files_struct *fsp, const char *name, void *value, size_t size)
{
	return sys_fgetxattr(fsp->fh->fd, name, value, size);
}

static ssize_t vfswrap_listxattr(struct vfs_handle_struct *handle, const char *path, char *list, size_t size)
{
	return sys_listxattr(path, list, size);
}

static ssize_t vfswrap_llistxattr(struct vfs_handle_struct *handle, const char *path, char *list, size_t size)
{
	return sys_llistxattr(path, list, size);
}

static ssize_t vfswrap_flistxattr(struct vfs_handle_struct *handle, struct files_struct *fsp, char *list, size_t size)
{
	return sys_flistxattr(fsp->fh->fd, list, size);
}

static int vfswrap_removexattr(struct vfs_handle_struct *handle, const char *path, const char *name)
{
	return sys_removexattr(path, name);
}

static int vfswrap_lremovexattr(struct vfs_handle_struct *handle, const char *path, const char *name)
{
	return sys_lremovexattr(path, name);
}

static int vfswrap_fremovexattr(struct vfs_handle_struct *handle, struct files_struct *fsp, const char *name)
{
	return sys_fremovexattr(fsp->fh->fd, name);
}

static int vfswrap_setxattr(struct vfs_handle_struct *handle, const char *path, const char *name, const void *value, size_t size, int flags)
{
	return sys_setxattr(path, name, value, size, flags);
}

static int vfswrap_lsetxattr(struct vfs_handle_struct *handle, const char *path, const char *name, const void *value, size_t size, int flags)
{
	return sys_lsetxattr(path, name, value, size, flags);
}

static int vfswrap_fsetxattr(struct vfs_handle_struct *handle, struct files_struct *fsp, const char *name, const void *value, size_t size, int flags)
{
	return sys_fsetxattr(fsp->fh->fd, name, value, size, flags);
}

static int vfswrap_aio_read(struct vfs_handle_struct *handle, struct files_struct *fsp, SMB_STRUCT_AIOCB *aiocb)
{
	int ret;
	/*
	 * aio_read must be done as root, because in the glibc aio
	 * implementation the helper thread needs to be able to send a signal
	 * to the main thread, even when it has done a seteuid() to a
	 * different user.
	 */
	become_root();
	ret = sys_aio_read(aiocb);
	unbecome_root();
	return ret;
}

static int vfswrap_aio_write(struct vfs_handle_struct *handle, struct files_struct *fsp, SMB_STRUCT_AIOCB *aiocb)
{
	int ret;
	/*
	 * aio_write must be done as root, because in the glibc aio
	 * implementation the helper thread needs to be able to send a signal
	 * to the main thread, even when it has done a seteuid() to a
	 * different user.
	 */
	become_root();
	ret = sys_aio_write(aiocb);
	unbecome_root();
	return ret;
}

static ssize_t vfswrap_aio_return(struct vfs_handle_struct *handle, struct files_struct *fsp, SMB_STRUCT_AIOCB *aiocb)
{
	return sys_aio_return(aiocb);
}

static int vfswrap_aio_cancel(struct vfs_handle_struct *handle, struct files_struct *fsp, SMB_STRUCT_AIOCB *aiocb)
{
	return sys_aio_cancel(fsp->fh->fd, aiocb);
}

static int vfswrap_aio_error(struct vfs_handle_struct *handle, struct files_struct *fsp, SMB_STRUCT_AIOCB *aiocb)
{
	return sys_aio_error(aiocb);
}

static int vfswrap_aio_fsync(struct vfs_handle_struct *handle, struct files_struct *fsp, int op, SMB_STRUCT_AIOCB *aiocb)
{
	return sys_aio_fsync(op, aiocb);
}

static int vfswrap_aio_suspend(struct vfs_handle_struct *handle, struct files_struct *fsp, const SMB_STRUCT_AIOCB * const aiocb[], int n, const struct timespec *timeout)
{
	return sys_aio_suspend(aiocb, n, timeout);
}

static bool vfswrap_aio_force(struct vfs_handle_struct *handle, struct files_struct *fsp)
{
	return false;
}

static bool vfswrap_is_offline(struct vfs_handle_struct *handle,
			       const struct smb_filename *fname,
			       SMB_STRUCT_STAT *sbuf)
{
	NTSTATUS status;
	char *path;

        if (ISDOT(fname->base_name) || ISDOTDOT(fname->base_name)) {
		return false;
	}

	if (!lp_dmapi_support(SNUM(handle->conn)) || !dmapi_have_session()) {
#if defined(ENOTSUP)
		errno = ENOTSUP;
#endif
		return false;
	}

        status = get_full_smb_filename(talloc_tos(), fname, &path);
        if (!NT_STATUS_IS_OK(status)) {
                errno = map_errno_from_nt_status(status);
                return false;
        }

	return (dmapi_file_flags(path) & FILE_ATTRIBUTE_OFFLINE) != 0;
}

static int vfswrap_set_offline(struct vfs_handle_struct *handle,
			       const struct smb_filename *fname)
{
	/* We don't know how to set offline bit by default, needs to be overriden in the vfs modules */
#if defined(ENOTSUP)
	errno = ENOTSUP;
#endif
	return -1;
}

static struct vfs_fn_pointers vfs_default_fns = {
	/* Disk operations */

	.connect_fn = vfswrap_connect,
	.disconnect = vfswrap_disconnect,
	.disk_free = vfswrap_disk_free,
	.get_quota = vfswrap_get_quota,
	.set_quota = vfswrap_set_quota,
	.get_shadow_copy_data = vfswrap_get_shadow_copy_data,
	.statvfs = vfswrap_statvfs,
	.fs_capabilities = vfswrap_fs_capabilities,

	/* Directory operations */

	.opendir = vfswrap_opendir,
	.fdopendir = vfswrap_fdopendir,
	.readdir = vfswrap_readdir,
	.seekdir = vfswrap_seekdir,
	.telldir = vfswrap_telldir,
	.rewind_dir = vfswrap_rewinddir,
	.mkdir = vfswrap_mkdir,
	.rmdir = vfswrap_rmdir,
	.closedir = vfswrap_closedir,
	.init_search_op = vfswrap_init_search_op,

	/* File operations */

	.open_fn = vfswrap_open,
	.create_file = vfswrap_create_file,
	.close_fn = vfswrap_close,
	.vfs_read = vfswrap_read,
	.pread = vfswrap_pread,
	.write = vfswrap_write,
	.pwrite = vfswrap_pwrite,
	.lseek = vfswrap_lseek,
	.sendfile = vfswrap_sendfile,
	.recvfile = vfswrap_recvfile,
	.rename = vfswrap_rename,
	.fsync = vfswrap_fsync,
	.stat = vfswrap_stat,
	.fstat = vfswrap_fstat,
	.lstat = vfswrap_lstat,
	.get_alloc_size = vfswrap_get_alloc_size,
	.unlink = vfswrap_unlink,
	.chmod = vfswrap_chmod,
	.fchmod = vfswrap_fchmod,
	.chown = vfswrap_chown,
	.fchown = vfswrap_fchown,
	.lchown = vfswrap_lchown,
	.chdir = vfswrap_chdir,
	.getwd = vfswrap_getwd,
	.ntimes = vfswrap_ntimes,
	.ftruncate = vfswrap_ftruncate,
	.fallocate = vfswrap_fallocate,
	.lock = vfswrap_lock,
	.kernel_flock = vfswrap_kernel_flock,
	.linux_setlease = vfswrap_linux_setlease,
	.getlock = vfswrap_getlock,
	.symlink = vfswrap_symlink,
	.vfs_readlink = vfswrap_readlink,
	.link = vfswrap_link,
	.mknod = vfswrap_mknod,
	.realpath = vfswrap_realpath,
	.notify_watch = vfswrap_notify_watch,
	.chflags = vfswrap_chflags,
	.file_id_create = vfswrap_file_id_create,
	.streaminfo = vfswrap_streaminfo,
	.get_real_filename = vfswrap_get_real_filename,
	.connectpath = vfswrap_connectpath,
	.brl_lock_windows = vfswrap_brl_lock_windows,
	.brl_unlock_windows = vfswrap_brl_unlock_windows,
	.brl_cancel_windows = vfswrap_brl_cancel_windows,
	.strict_lock = vfswrap_strict_lock,
	.strict_unlock = vfswrap_strict_unlock,
	.translate_name = vfswrap_translate_name,

	/* NT ACL operations. */

	.fget_nt_acl = vfswrap_fget_nt_acl,
	.get_nt_acl = vfswrap_get_nt_acl,
	.fset_nt_acl = vfswrap_fset_nt_acl,

	/* POSIX ACL operations. */

	.chmod_acl = vfswrap_chmod_acl,
	.fchmod_acl = vfswrap_fchmod_acl,

	.sys_acl_get_entry = vfswrap_sys_acl_get_entry,
	.sys_acl_get_tag_type = vfswrap_sys_acl_get_tag_type,
	.sys_acl_get_permset = vfswrap_sys_acl_get_permset,
	.sys_acl_get_qualifier = vfswrap_sys_acl_get_qualifier,
	.sys_acl_get_file = vfswrap_sys_acl_get_file,
	.sys_acl_get_fd = vfswrap_sys_acl_get_fd,
	.sys_acl_clear_perms = vfswrap_sys_acl_clear_perms,
	.sys_acl_add_perm = vfswrap_sys_acl_add_perm,
	.sys_acl_to_text = vfswrap_sys_acl_to_text,
	.sys_acl_init = vfswrap_sys_acl_init,
	.sys_acl_create_entry = vfswrap_sys_acl_create_entry,
	.sys_acl_set_tag_type = vfswrap_sys_acl_set_tag_type,
	.sys_acl_set_qualifier = vfswrap_sys_acl_set_qualifier,
	.sys_acl_set_permset = vfswrap_sys_acl_set_permset,
	.sys_acl_valid = vfswrap_sys_acl_valid,
	.sys_acl_set_file = vfswrap_sys_acl_set_file,
	.sys_acl_set_fd = vfswrap_sys_acl_set_fd,
	.sys_acl_delete_def_file = vfswrap_sys_acl_delete_def_file,
	.sys_acl_get_perm = vfswrap_sys_acl_get_perm,
	.sys_acl_free_text = vfswrap_sys_acl_free_text,
	.sys_acl_free_acl = vfswrap_sys_acl_free_acl,
	.sys_acl_free_qualifier = vfswrap_sys_acl_free_qualifier,

	/* EA operations. */
	.getxattr = vfswrap_getxattr,
	.lgetxattr = vfswrap_lgetxattr,
	.fgetxattr = vfswrap_fgetxattr,
	.listxattr = vfswrap_listxattr,
	.llistxattr = vfswrap_llistxattr,
	.flistxattr = vfswrap_flistxattr,
	.removexattr = vfswrap_removexattr,
	.lremovexattr = vfswrap_lremovexattr,
	.fremovexattr = vfswrap_fremovexattr,
	.setxattr = vfswrap_setxattr,
	.lsetxattr = vfswrap_lsetxattr,
	.fsetxattr = vfswrap_fsetxattr,

	/* aio operations */
	.aio_read = vfswrap_aio_read,
	.aio_write = vfswrap_aio_write,
	.aio_return_fn = vfswrap_aio_return,
	.aio_cancel = vfswrap_aio_cancel,
	.aio_error_fn = vfswrap_aio_error,
	.aio_fsync = vfswrap_aio_fsync,
	.aio_suspend = vfswrap_aio_suspend,
	.aio_force = vfswrap_aio_force,

	/* offline operations */
	.is_offline = vfswrap_is_offline,
	.set_offline = vfswrap_set_offline
};

NTSTATUS vfs_default_init(void);
NTSTATUS vfs_default_init(void)
{
	return smb_register_vfs(SMB_VFS_INTERFACE_VERSION,
				DEFAULT_VFS_MODULE_NAME, &vfs_default_fns);
}


