/*
   Unix SMB/CIFS implementation.
   Wrap disk only vfs functions to sidestep dodgy compilers.
   Copyright (C) Tim Potter 1998
   Copyright (C) Jeremy Allison 2007

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "includes.h"

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

static SMB_BIG_UINT vfswrap_disk_free(vfs_handle_struct *handle,  const char *path, BOOL small_query, SMB_BIG_UINT *bsize,
			       SMB_BIG_UINT *dfree, SMB_BIG_UINT *dsize)
{
	SMB_BIG_UINT result;

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

static int vfswrap_get_shadow_copy_data(struct vfs_handle_struct *handle, struct files_struct *fsp, SHADOW_COPY_DATA *shadow_copy_data, BOOL labels)
{
	errno = ENOSYS;
	return -1;  /* Not implemented. */
}

static int vfswrap_statvfs(struct vfs_handle_struct *handle,  const char *path, vfs_statvfs_struct *statbuf)
{
	return sys_statvfs(path, statbuf);
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

static SMB_STRUCT_DIRENT *vfswrap_readdir(vfs_handle_struct *handle,  SMB_STRUCT_DIR *dirp)
{
	SMB_STRUCT_DIRENT *result;

	START_PROFILE(syscall_readdir);
	result = sys_readdir(dirp);
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
	BOOL has_dacl = False;

	START_PROFILE(syscall_mkdir);

	if (lp_inherit_acls(SNUM(handle->conn)) && (has_dacl = directory_has_default_acl(handle->conn, parent_dirname(path))))
		mode = 0777;

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

/* File operations */

static int vfswrap_open(vfs_handle_struct *handle,  const char *fname,
	files_struct *fsp, int flags, mode_t mode)
{
	int result;

	START_PROFILE(syscall_open);
	result = sys_open(fname, flags, mode);
	END_PROFILE(syscall_open);
	return result;
}

static int vfswrap_close(vfs_handle_struct *handle, files_struct *fsp, int fd)
{
	int result;

	START_PROFILE(syscall_close);

	result = close(fd);
	END_PROFILE(syscall_close);
	return result;
}

static ssize_t vfswrap_read(vfs_handle_struct *handle, files_struct *fsp, int fd, void *data, size_t n)
{
	ssize_t result;

	START_PROFILE_BYTES(syscall_read, n);
	result = sys_read(fd, data, n);
	END_PROFILE(syscall_read);
	return result;
}

static ssize_t vfswrap_pread(vfs_handle_struct *handle, files_struct *fsp, int fd, void *data,
			size_t n, SMB_OFF_T offset)
{
	ssize_t result;

#if defined(HAVE_PREAD) || defined(HAVE_PREAD64)
	START_PROFILE_BYTES(syscall_pread, n);
	result = sys_pread(fd, data, n, offset);
	END_PROFILE(syscall_pread);

	if (result == -1 && errno == ESPIPE) {
		/* Maintain the fiction that pipes can be seeked (sought?) on. */
		result = SMB_VFS_READ(fsp, fd, data, n);
		fsp->fh->pos = 0;
	}

#else /* HAVE_PREAD */
	SMB_OFF_T   curr;
	int lerrno;

	curr = SMB_VFS_LSEEK(fsp, fd, 0, SEEK_CUR);
	if (curr == -1 && errno == ESPIPE) {
		/* Maintain the fiction that pipes can be seeked (sought?) on. */
		result = SMB_VFS_READ(fsp, fd, data, n);
		fsp->fh->pos = 0;
		return result;
	}

	if (SMB_VFS_LSEEK(fsp, fd, offset, SEEK_SET) == -1) {
		return -1;
	}

	errno = 0;
	result = SMB_VFS_READ(fsp, fd, data, n);
	lerrno = errno;

	SMB_VFS_LSEEK(fsp, fd, curr, SEEK_SET);
	errno = lerrno;

#endif /* HAVE_PREAD */

	return result;
}

static ssize_t vfswrap_write(vfs_handle_struct *handle, files_struct *fsp, int fd, const void *data, size_t n)
{
	ssize_t result;

	START_PROFILE_BYTES(syscall_write, n);
	result = sys_write(fd, data, n);
	END_PROFILE(syscall_write);
	return result;
}

static ssize_t vfswrap_pwrite(vfs_handle_struct *handle, files_struct *fsp, int fd, const void *data,
			size_t n, SMB_OFF_T offset)
{
	ssize_t result;

#if defined(HAVE_PWRITE) || defined(HAVE_PRWITE64)
	START_PROFILE_BYTES(syscall_pwrite, n);
	result = sys_pwrite(fd, data, n, offset);
	END_PROFILE(syscall_pwrite);

	if (result == -1 && errno == ESPIPE) {
		/* Maintain the fiction that pipes can be sought on. */
		result = SMB_VFS_WRITE(fsp, fd, data, n);
	}

#else /* HAVE_PWRITE */
	SMB_OFF_T   curr;
	int         lerrno;

	curr = SMB_VFS_LSEEK(fsp, fd, 0, SEEK_CUR);
	if (curr == -1) {
		return -1;
	}

	if (SMB_VFS_LSEEK(fsp, fd, offset, SEEK_SET) == -1) {
		return -1;
	}

	result = SMB_VFS_WRITE(fsp, fd, data, n);
	lerrno = errno;

	SMB_VFS_LSEEK(fsp, fd, curr, SEEK_SET);
	errno = lerrno;

#endif /* HAVE_PWRITE */

	return result;
}

static SMB_OFF_T vfswrap_lseek(vfs_handle_struct *handle, files_struct *fsp, int filedes, SMB_OFF_T offset, int whence)
{
	SMB_OFF_T result = 0;

	START_PROFILE(syscall_lseek);

	/* Cope with 'stat' file opens. */
	if (filedes != -1)
		result = sys_lseek(filedes, offset, whence);

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

static ssize_t vfswrap_sendfile(vfs_handle_struct *handle, int tofd, files_struct *fsp, int fromfd, const DATA_BLOB *hdr,
			SMB_OFF_T offset, size_t n)
{
	ssize_t result;

	START_PROFILE_BYTES(syscall_sendfile, n);
	result = sys_sendfile(tofd, fromfd, hdr, offset, n);
	END_PROFILE(syscall_sendfile);
	return result;
}

/*********************************************************
 For rename across filesystems Patch from Warren Birnbaum
 <warrenb@hpcvscdp.cv.hp.com>
**********************************************************/

static int copy_reg(const char *source, const char *dest)
{
	SMB_STRUCT_STAT source_stats;
	int saved_errno;
	int ifd = -1;
	int ofd = -1;

	if (sys_lstat (source, &source_stats) == -1)
		return -1;

	if (!S_ISREG (source_stats.st_mode))
		return -1;

	if((ifd = sys_open (source, O_RDONLY, 0)) < 0)
		return -1;

	if (unlink (dest) && errno != ENOENT)
		return -1;

#ifdef O_NOFOLLOW
	if((ofd = sys_open (dest, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW, 0600)) < 0 )
#else
	if((ofd = sys_open (dest, O_WRONLY | O_CREAT | O_TRUNC , 0600)) < 0 )
#endif
		goto err;

	if (transfer_file(ifd, ofd, (size_t)-1) == -1)
		goto err;

	/*
	 * Try to preserve ownership.  For non-root it might fail, but that's ok.
	 * But root probably wants to know, e.g. if NFS disallows it.
	 */

#ifdef HAVE_FCHOWN
	if ((fchown(ofd, source_stats.st_uid, source_stats.st_gid) == -1) && (errno != EPERM))
#else
	if ((chown(dest, source_stats.st_uid, source_stats.st_gid) == -1) && (errno != EPERM))
#endif
		goto err;

	/*
	 * fchown turns off set[ug]id bits for non-root,
	 * so do the chmod last.
	 */

#if defined(HAVE_FCHMOD)
	if (fchmod (ofd, source_stats.st_mode & 07777))
#else
	if (chmod (dest, source_stats.st_mode & 07777))
#endif
		goto err;

	if (close (ifd) == -1)
		goto err;

	if (close (ofd) == -1)
		return -1;

	/* Try to copy the old file's modtime and access time.  */
	{
		struct utimbuf tv;

		tv.actime = source_stats.st_atime;
		tv.modtime = source_stats.st_mtime;
		utime(dest, &tv);
	}

	if (unlink (source) == -1)
		return -1;

	return 0;

  err:

	saved_errno = errno;
	if (ifd != -1)
		close(ifd);
	if (ofd != -1)
		close(ofd);
	errno = saved_errno;
	return -1;
}

static int vfswrap_rename(vfs_handle_struct *handle,  const char *oldname, const char *newname)
{
	int result;

	START_PROFILE(syscall_rename);
	result = rename(oldname, newname);
	if ((result == -1) && (errno == EXDEV)) {
		/* Rename across filesystems needed. */
		result = copy_reg(oldname, newname);
	}

	END_PROFILE(syscall_rename);
	return result;
}

static int vfswrap_fsync(vfs_handle_struct *handle, files_struct *fsp, int fd)
{
#ifdef HAVE_FSYNC
	int result;

	START_PROFILE(syscall_fsync);
	result = fsync(fd);
	END_PROFILE(syscall_fsync);
	return result;
#else
	return 0;
#endif
}

static int vfswrap_stat(vfs_handle_struct *handle,  const char *fname, SMB_STRUCT_STAT *sbuf)
{
	int result;

	START_PROFILE(syscall_stat);
	result = sys_stat(fname, sbuf);
	END_PROFILE(syscall_stat);
	return result;
}

static int vfswrap_fstat(vfs_handle_struct *handle, files_struct *fsp, int fd, SMB_STRUCT_STAT *sbuf)
{
	int result;

	START_PROFILE(syscall_fstat);
	result = sys_fstat(fd, sbuf);
	END_PROFILE(syscall_fstat);
	return result;
}

int vfswrap_lstat(vfs_handle_struct *handle,  const char *path, SMB_STRUCT_STAT *sbuf)
{
	int result;

	START_PROFILE(syscall_lstat);
	result = sys_lstat(path, sbuf);
	END_PROFILE(syscall_lstat);
	return result;
}

static int vfswrap_unlink(vfs_handle_struct *handle,  const char *path)
{
	int result;

	START_PROFILE(syscall_unlink);
	result = unlink(path);
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

static int vfswrap_fchmod(vfs_handle_struct *handle, files_struct *fsp, int fd, mode_t mode)
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
		if ((result = SMB_VFS_FCHMOD_ACL(fsp, fd, mode)) == 0) {
			END_PROFILE(syscall_fchmod);
			return result;
		}
		/* Error - return the old errno. */
		errno = saved_errno;
	}

#if defined(HAVE_FCHMOD)
	result = fchmod(fd, mode);
#else
	result = -1;
	errno = ENOSYS;
#endif

	END_PROFILE(syscall_fchmod);
	return result;
}

static int vfswrap_chown(vfs_handle_struct *handle,  const char *path, uid_t uid, gid_t gid)
{
	int result;

	START_PROFILE(syscall_chown);
	result = sys_chown(path, uid, gid);
	END_PROFILE(syscall_chown);
	return result;
}

static int vfswrap_fchown(vfs_handle_struct *handle, files_struct *fsp, int fd, uid_t uid, gid_t gid)
{
#ifdef HAVE_FCHOWN
	int result;

	START_PROFILE(syscall_fchown);
	result = fchown(fd, uid, gid);
	END_PROFILE(syscall_fchown);
	return result;
#else
	errno = ENOSYS;
	return -1;
#endif
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

static int vfswrap_ntimes(vfs_handle_struct *handle, const char *path, const struct timespec ts[2])
{
	int result;

	START_PROFILE(syscall_ntimes);
#if defined(HAVE_UTIMES)
	{
		struct timeval tv[2];
		tv[0] = convert_timespec_to_timeval(ts[0]);
		tv[1] = convert_timespec_to_timeval(ts[1]);
		result = utimes(path, tv);
	}
#elif defined(HAVE_UTIME)
	{
		struct utimbuf times;
		times.actime = convert_timespec_to_time_t(ts[0]);
		times.modtime = convert_timespec_to_time_t(ts[1]);
		result = utime(path, &times);
	}
#else
	errno = ENOSYS;
	result = -1;
#endif
	END_PROFILE(syscall_ntimes);
	return result;
}

/*********************************************************************
 A version of ftruncate that will write the space on disk if strict
 allocate is set.
**********************************************************************/

static int strict_allocate_ftruncate(vfs_handle_struct *handle, files_struct *fsp, int fd, SMB_OFF_T len)
{
	SMB_STRUCT_STAT st;
	SMB_OFF_T currpos = SMB_VFS_LSEEK(fsp, fd, 0, SEEK_CUR);
	unsigned char zero_space[4096];
	SMB_OFF_T space_to_write;

	if (currpos == -1)
		return -1;

	if (SMB_VFS_FSTAT(fsp, fd, &st) == -1)
		return -1;

	space_to_write = len - st.st_size;

#ifdef S_ISFIFO
	if (S_ISFIFO(st.st_mode))
		return 0;
#endif

	if (st.st_size == len)
		return 0;

	/* Shrink - just ftruncate. */
	if (st.st_size > len)
		return sys_ftruncate(fd, len);

	/* Write out the real space on disk. */
	if (SMB_VFS_LSEEK(fsp, fd, st.st_size, SEEK_SET) != st.st_size)
		return -1;

	space_to_write = len - st.st_size;

	memset(zero_space, '\0', sizeof(zero_space));
	while ( space_to_write > 0) {
		SMB_OFF_T retlen;
		SMB_OFF_T current_len_to_write = MIN(sizeof(zero_space),space_to_write);

		retlen = SMB_VFS_WRITE(fsp,fsp->fh->fd,(char *)zero_space,current_len_to_write);
		if (retlen <= 0)
			return -1;

		space_to_write -= retlen;
	}

	/* Seek to where we were */
	if (SMB_VFS_LSEEK(fsp, fd, currpos, SEEK_SET) != currpos)
		return -1;

	return 0;
}

static int vfswrap_ftruncate(vfs_handle_struct *handle, files_struct *fsp, int fd, SMB_OFF_T len)
{
	int result = -1;
	SMB_STRUCT_STAT st;
	char c = 0;
	SMB_OFF_T currpos;

	START_PROFILE(syscall_ftruncate);

	if (lp_strict_allocate(SNUM(fsp->conn))) {
		result = strict_allocate_ftruncate(handle, fsp, fd, len);
		END_PROFILE(syscall_ftruncate);
		return result;
	}

#if 1 // AVM patch - don't growth the file  (too much time and RAM for copy of large files to USB1.1 FAT filesystem)
	{ 
	SMB_BIG_UINT big_len = len;

	result = SMB_VFS_FSTAT(fsp,fsp->fh->fd,&st);
	if (result == -1) {
		goto done;
	}

	if (big_len == (SMB_BIG_UINT)st.st_size) {
		result = 0;
		goto done;
	}

	if (big_len > (SMB_BIG_UINT)st.st_size) {
		SMB_BIG_UINT space_avail;
		SMB_BIG_UINT bsize,dfree,dsize;
		big_len -= st.st_size;
		big_len /= 1024; /* Len is now number of 1k blocks needed. */
		space_avail = SMB_VFS_DISK_FREE(fsp->conn ,fsp->fsp_name,False,&bsize,&dfree,&dsize);
		if (space_avail == (SMB_BIG_UINT)-1) {
			result = -1;
			goto done;
		}

		if (big_len > space_avail) {
			errno = ENOSPC;
			result = -1;
			goto done;
		}

		// do nothing to be fast!
		result = 0;
		goto done;
	}
	} // block
#endif // AVM Patch

	/* we used to just check HAVE_FTRUNCATE_EXTEND and only use
	   sys_ftruncate if the system supports it. Then I discovered that
	   you can have some filesystems that support ftruncate
	   expansion and some that don't! On Linux fat can't do
	   ftruncate extend but ext2 can. */

	result = sys_ftruncate(fd, len);
	if (result == 0)
		goto done;

	/* According to W. R. Stevens advanced UNIX prog. Pure 4.3 BSD cannot
	   extend a file with ftruncate. Provide alternate implementation
	   for this */
	currpos = SMB_VFS_LSEEK(fsp, fd, 0, SEEK_CUR);
	if (currpos == -1) {
		goto done;
	}

	/* Do an fstat to see if the file is longer than the requested
	   size in which case the ftruncate above should have
	   succeeded or shorter, in which case seek to len - 1 and
	   write 1 byte of zero */
	if (SMB_VFS_FSTAT(fsp, fd, &st) == -1) {
		goto done;
	}

#ifdef S_ISFIFO
	if (S_ISFIFO(st.st_mode)) {
		result = 0;
		goto done;
	}
#endif

	if (st.st_size == len) {
		result = 0;
		goto done;
	}

	if (st.st_size > len) {
		/* the sys_ftruncate should have worked */
		goto done;
	}

	if (SMB_VFS_LSEEK(fsp, fd, len-1, SEEK_SET) != len -1)
		goto done;

	if (SMB_VFS_WRITE(fsp, fd, &c, 1)!=1)
		goto done;

	/* Seek to where we were */
	if (SMB_VFS_LSEEK(fsp, fd, currpos, SEEK_SET) != currpos)
		goto done;
	result = 0;

  done:

	END_PROFILE(syscall_ftruncate);
	return result;
}

static BOOL vfswrap_lock(vfs_handle_struct *handle, files_struct *fsp, int fd, int op, SMB_OFF_T offset, SMB_OFF_T count, int type)
{
	BOOL result;

	START_PROFILE(syscall_fcntl_lock);
	result =  fcntl_lock(fd, op, offset, count, type);
	END_PROFILE(syscall_fcntl_lock);
	return result;
}

static int vfswrap_kernel_flock(vfs_handle_struct *handle, files_struct *fsp, int fd,
				uint32 share_mode)
{
	START_PROFILE(syscall_kernel_flock);
	kernel_flock(fd, share_mode);
	END_PROFILE(syscall_kernel_flock);
	return 0;
}

static BOOL vfswrap_getlock(vfs_handle_struct *handle, files_struct *fsp, int fd, SMB_OFF_T *poffset, SMB_OFF_T *pcount, int *ptype, pid_t *ppid)
{
	BOOL result;

	START_PROFILE(syscall_fcntl_getlock);
	result =  fcntl_getlock(fd, poffset, pcount, ptype, ppid);
	END_PROFILE(syscall_fcntl_getlock);
	return result;
}

static int vfswrap_linux_setlease(vfs_handle_struct *handle, files_struct *fsp, int fd,
				int leasetype)
{
	int result = -1;

	START_PROFILE(syscall_linux_setlease);

#ifdef HAVE_KERNEL_OPLOCKS_LINUX
	/* first set the signal handler */
	if(linux_set_lease_sighandler(fd) == -1)
		return -1;

	result = linux_setlease(fd, leasetype);
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
	result = sys_symlink(oldpath, newpath);
	END_PROFILE(syscall_symlink);
	return result;
}

static int vfswrap_readlink(vfs_handle_struct *handle,  const char *path, char *buf, size_t bufsiz)
{
	int result;

	START_PROFILE(syscall_readlink);
	result = sys_readlink(path, buf, bufsiz);
	END_PROFILE(syscall_readlink);
	return result;
}

static int vfswrap_link(vfs_handle_struct *handle,  const char *oldpath, const char *newpath)
{
	int result;

	START_PROFILE(syscall_link);
	result = sys_link(oldpath, newpath);
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

static char *vfswrap_realpath(vfs_handle_struct *handle,  const char *path, char *resolved_path)
{
	char *result;

	START_PROFILE(syscall_realpath);
	result = sys_realpath(path, resolved_path);
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

static int vfswrap_chflags(vfs_handle_struct *handle, const char *path, int flags)
{
#ifdef HAVE_CHFLAGS
	return chflags(path, flags);
#else
	errno = ENOSYS;
	return -1;
#endif
}

static size_t vfswrap_fget_nt_acl(vfs_handle_struct *handle, files_struct *fsp, int fd, uint32 security_info, SEC_DESC **ppdesc)
{
	size_t result;

	START_PROFILE(fget_nt_acl);
	result = get_nt_acl(fsp, security_info, ppdesc);
	END_PROFILE(fget_nt_acl);
	return result;
}

static size_t vfswrap_get_nt_acl(vfs_handle_struct *handle, files_struct *fsp, const char *name, uint32 security_info, SEC_DESC **ppdesc)
{
	size_t result;

	START_PROFILE(get_nt_acl);
	result = get_nt_acl(fsp, security_info, ppdesc);
	END_PROFILE(get_nt_acl);
	return result;
}

static BOOL vfswrap_fset_nt_acl(vfs_handle_struct *handle, files_struct *fsp, int fd, uint32 security_info_sent, SEC_DESC *psd)
{
	BOOL result;

	START_PROFILE(fset_nt_acl);
	result = set_nt_acl(fsp, security_info_sent, psd);
	END_PROFILE(fset_nt_acl);
	return result;
}

static BOOL vfswrap_set_nt_acl(vfs_handle_struct *handle, files_struct *fsp, const char *name, uint32 security_info_sent, SEC_DESC *psd)
{
	BOOL result;

	START_PROFILE(set_nt_acl);
	result = set_nt_acl(fsp, security_info_sent, psd);
	END_PROFILE(set_nt_acl);
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

static int vfswrap_fchmod_acl(vfs_handle_struct *handle, files_struct *fsp, int fd, mode_t mode)
{
#ifdef HAVE_NO_ACL
	errno = ENOSYS;
	return -1;
#else
	int result;

	START_PROFILE(fchmod_acl);
	result = fchmod_acl(fsp, fd, mode);
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

static SMB_ACL_T vfswrap_sys_acl_get_fd(vfs_handle_struct *handle, files_struct *fsp, int fd)
{
	return sys_acl_get_fd(handle, fsp, fd);
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

static int vfswrap_sys_acl_set_fd(vfs_handle_struct *handle, files_struct *fsp, int fd, SMB_ACL_T theacl)
{
	return sys_acl_set_fd(handle, fsp, fd, theacl);
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

static ssize_t vfswrap_fgetxattr(struct vfs_handle_struct *handle, struct files_struct *fsp,int fd, const char *name, void *value, size_t size)
{
	return sys_fgetxattr(fd, name, value, size);
}

static ssize_t vfswrap_listxattr(struct vfs_handle_struct *handle, const char *path, char *list, size_t size)
{
	return sys_listxattr(path, list, size);
}

ssize_t vfswrap_llistxattr(struct vfs_handle_struct *handle, const char *path, char *list, size_t size)
{
	return sys_llistxattr(path, list, size);
}

ssize_t vfswrap_flistxattr(struct vfs_handle_struct *handle, struct files_struct *fsp,int fd, char *list, size_t size)
{
	return sys_flistxattr(fd, list, size);
}

static int vfswrap_removexattr(struct vfs_handle_struct *handle, const char *path, const char *name)
{
	return sys_removexattr(path, name);
}

static int vfswrap_lremovexattr(struct vfs_handle_struct *handle, const char *path, const char *name)
{
	return sys_lremovexattr(path, name);
}

static int vfswrap_fremovexattr(struct vfs_handle_struct *handle, struct files_struct *fsp,int fd, const char *name)
{
	return sys_fremovexattr(fd, name);
}

static int vfswrap_setxattr(struct vfs_handle_struct *handle, const char *path, const char *name, const void *value, size_t size, int flags)
{
	return sys_setxattr(path, name, value, size, flags);
}

static int vfswrap_lsetxattr(struct vfs_handle_struct *handle, const char *path, const char *name, const void *value, size_t size, int flags)
{
	return sys_lsetxattr(path, name, value, size, flags);
}

static int vfswrap_fsetxattr(struct vfs_handle_struct *handle, struct files_struct *fsp,int fd, const char *name, const void *value, size_t size, int flags)
{
	return sys_fsetxattr(fd, name, value, size, flags);
}

static int vfswrap_aio_read(struct vfs_handle_struct *handle, struct files_struct *fsp, SMB_STRUCT_AIOCB *aiocb)
{
	return sys_aio_read(aiocb);
}

static int vfswrap_aio_write(struct vfs_handle_struct *handle, struct files_struct *fsp, SMB_STRUCT_AIOCB *aiocb)
{
	return sys_aio_write(aiocb);
}

static ssize_t vfswrap_aio_return(struct vfs_handle_struct *handle, struct files_struct *fsp, SMB_STRUCT_AIOCB *aiocb)
{
	return sys_aio_return(aiocb);
}

static int vfswrap_aio_cancel(struct vfs_handle_struct *handle, struct files_struct *fsp, int fd, SMB_STRUCT_AIOCB *aiocb)
{
	return sys_aio_cancel(fd, aiocb);
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

static vfs_op_tuple vfs_default_ops[] = {

	/* Disk operations */

	{SMB_VFS_OP(vfswrap_connect),	SMB_VFS_OP_CONNECT,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_disconnect),	SMB_VFS_OP_DISCONNECT,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_disk_free),	SMB_VFS_OP_DISK_FREE,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_get_quota),	SMB_VFS_OP_GET_QUOTA,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_set_quota),	SMB_VFS_OP_SET_QUOTA,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_get_shadow_copy_data), SMB_VFS_OP_GET_SHADOW_COPY_DATA,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_statvfs),	SMB_VFS_OP_STATVFS,
	 SMB_VFS_LAYER_OPAQUE},

	/* Directory operations */

	{SMB_VFS_OP(vfswrap_opendir),	SMB_VFS_OP_OPENDIR,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_readdir),	SMB_VFS_OP_READDIR,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_seekdir),	SMB_VFS_OP_SEEKDIR,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_telldir),	SMB_VFS_OP_TELLDIR,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_rewinddir),	SMB_VFS_OP_REWINDDIR,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_mkdir),	SMB_VFS_OP_MKDIR,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_rmdir),	SMB_VFS_OP_RMDIR,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_closedir),	SMB_VFS_OP_CLOSEDIR,
	 SMB_VFS_LAYER_OPAQUE},

	/* File operations */

	{SMB_VFS_OP(vfswrap_open),	SMB_VFS_OP_OPEN,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_close),	SMB_VFS_OP_CLOSE,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_read),	SMB_VFS_OP_READ,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_pread),	SMB_VFS_OP_PREAD,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_write),	SMB_VFS_OP_WRITE,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_pwrite),	SMB_VFS_OP_PWRITE,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_lseek),	SMB_VFS_OP_LSEEK,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_sendfile),	SMB_VFS_OP_SENDFILE,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_rename),	SMB_VFS_OP_RENAME,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_fsync),	SMB_VFS_OP_FSYNC,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_stat),	SMB_VFS_OP_STAT,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_fstat),	SMB_VFS_OP_FSTAT,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_lstat),	SMB_VFS_OP_LSTAT,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_unlink),	SMB_VFS_OP_UNLINK,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_chmod),	SMB_VFS_OP_CHMOD,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_fchmod),	SMB_VFS_OP_FCHMOD,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_chown),	SMB_VFS_OP_CHOWN,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_fchown),	SMB_VFS_OP_FCHOWN,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_chdir),	SMB_VFS_OP_CHDIR,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_getwd),	SMB_VFS_OP_GETWD,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_ntimes),	SMB_VFS_OP_NTIMES,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_ftruncate),	SMB_VFS_OP_FTRUNCATE,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_lock),	SMB_VFS_OP_LOCK,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_kernel_flock),	SMB_VFS_OP_KERNEL_FLOCK,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_linux_setlease),	SMB_VFS_OP_LINUX_SETLEASE,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_getlock),	SMB_VFS_OP_GETLOCK,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_symlink),	SMB_VFS_OP_SYMLINK,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_readlink),	SMB_VFS_OP_READLINK,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_link),	SMB_VFS_OP_LINK,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_mknod),	SMB_VFS_OP_MKNOD,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_realpath),	SMB_VFS_OP_REALPATH,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_notify_watch),	SMB_VFS_OP_NOTIFY_WATCH,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_chflags),	SMB_VFS_OP_CHFLAGS,
	 SMB_VFS_LAYER_OPAQUE},

	/* NT ACL operations. */

	{SMB_VFS_OP(vfswrap_fget_nt_acl),	SMB_VFS_OP_FGET_NT_ACL,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_get_nt_acl),	SMB_VFS_OP_GET_NT_ACL,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_fset_nt_acl),	SMB_VFS_OP_FSET_NT_ACL,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_set_nt_acl),	SMB_VFS_OP_SET_NT_ACL,
	 SMB_VFS_LAYER_OPAQUE},

	/* POSIX ACL operations. */

	{SMB_VFS_OP(vfswrap_chmod_acl),	SMB_VFS_OP_CHMOD_ACL,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_fchmod_acl),	SMB_VFS_OP_FCHMOD_ACL,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_sys_acl_get_entry),	SMB_VFS_OP_SYS_ACL_GET_ENTRY,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_sys_acl_get_tag_type),	SMB_VFS_OP_SYS_ACL_GET_TAG_TYPE,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_sys_acl_get_permset),	SMB_VFS_OP_SYS_ACL_GET_PERMSET,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_sys_acl_get_qualifier),	SMB_VFS_OP_SYS_ACL_GET_QUALIFIER,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_sys_acl_get_file),	SMB_VFS_OP_SYS_ACL_GET_FILE,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_sys_acl_get_fd),	SMB_VFS_OP_SYS_ACL_GET_FD,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_sys_acl_clear_perms),	SMB_VFS_OP_SYS_ACL_CLEAR_PERMS,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_sys_acl_add_perm),	SMB_VFS_OP_SYS_ACL_ADD_PERM,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_sys_acl_to_text),	SMB_VFS_OP_SYS_ACL_TO_TEXT,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_sys_acl_init),	SMB_VFS_OP_SYS_ACL_INIT,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_sys_acl_create_entry),	SMB_VFS_OP_SYS_ACL_CREATE_ENTRY,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_sys_acl_set_tag_type),	SMB_VFS_OP_SYS_ACL_SET_TAG_TYPE,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_sys_acl_set_qualifier),	SMB_VFS_OP_SYS_ACL_SET_QUALIFIER,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_sys_acl_set_permset),	SMB_VFS_OP_SYS_ACL_SET_PERMSET,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_sys_acl_valid),	SMB_VFS_OP_SYS_ACL_VALID,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_sys_acl_set_file),	SMB_VFS_OP_SYS_ACL_SET_FILE,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_sys_acl_set_fd),	SMB_VFS_OP_SYS_ACL_SET_FD,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_sys_acl_delete_def_file),	SMB_VFS_OP_SYS_ACL_DELETE_DEF_FILE,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_sys_acl_get_perm),	SMB_VFS_OP_SYS_ACL_GET_PERM,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_sys_acl_free_text),	SMB_VFS_OP_SYS_ACL_FREE_TEXT,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_sys_acl_free_acl),	SMB_VFS_OP_SYS_ACL_FREE_ACL,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_sys_acl_free_qualifier),	SMB_VFS_OP_SYS_ACL_FREE_QUALIFIER,
	 SMB_VFS_LAYER_OPAQUE},

	/* EA operations. */

	{SMB_VFS_OP(vfswrap_getxattr),	SMB_VFS_OP_GETXATTR,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_lgetxattr),	SMB_VFS_OP_LGETXATTR,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_fgetxattr),	SMB_VFS_OP_FGETXATTR,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_listxattr),	SMB_VFS_OP_LISTXATTR,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_llistxattr),	SMB_VFS_OP_LLISTXATTR,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_flistxattr),	SMB_VFS_OP_FLISTXATTR,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_removexattr),	SMB_VFS_OP_REMOVEXATTR,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_lremovexattr),	SMB_VFS_OP_LREMOVEXATTR,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_fremovexattr),	SMB_VFS_OP_FREMOVEXATTR,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_setxattr),	SMB_VFS_OP_SETXATTR,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_lsetxattr),	SMB_VFS_OP_LSETXATTR,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_fsetxattr),	SMB_VFS_OP_FSETXATTR,
	 SMB_VFS_LAYER_OPAQUE},

	{SMB_VFS_OP(vfswrap_aio_read),	SMB_VFS_OP_AIO_READ,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_aio_write),	SMB_VFS_OP_AIO_WRITE,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_aio_return),	SMB_VFS_OP_AIO_RETURN,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_aio_cancel), SMB_VFS_OP_AIO_CANCEL,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_aio_error),	SMB_VFS_OP_AIO_ERROR,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_aio_fsync),	SMB_VFS_OP_AIO_FSYNC,
	 SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(vfswrap_aio_suspend),SMB_VFS_OP_AIO_SUSPEND,
	 SMB_VFS_LAYER_OPAQUE},

	/* Finish VFS operations definition */

	{SMB_VFS_OP(NULL),		SMB_VFS_OP_NOOP,
	 SMB_VFS_LAYER_NOOP}
};

NTSTATUS vfs_default_init(void);
NTSTATUS vfs_default_init(void)
{
	unsigned int needed = SMB_VFS_OP_LAST + 1; /* convert from index to count */

	if (ARRAY_SIZE(vfs_default_ops) != needed) {
		DEBUG(0, ("%s: %u ops registered, but %u ops are required\n",
			DEFAULT_VFS_MODULE_NAME, (unsigned int)ARRAY_SIZE(vfs_default_ops), needed));
		smb_panic("operation(s) missing from default VFS module");
	}

	return smb_register_vfs(SMB_VFS_INTERFACE_VERSION,
				DEFAULT_VFS_MODULE_NAME, vfs_default_ops);
}
