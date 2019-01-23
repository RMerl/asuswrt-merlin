/*
 * Time auditing VFS module for samba.  Log time taken for VFS call to syslog
 * facility.
 *
 * Copyright (C) Abhidnya Chirmule <achirmul@in.ibm.com> 2009
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

/*
 * This module implements logging for time taken for all Samba VFS operations.
 *
 * vfs objects = time_audit
 */


#include "includes.h"
#include "smbd/smbd.h"
#include "ntioctl.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_VFS

static double audit_timeout;

static void smb_time_audit_log(const char *syscallname, double elapsed)
{
	DEBUG(0, ("WARNING: System call \"%s\" took unexpectedly long "
		  "(%.2f seconds) -- Validate that file and storage "
		  "subsystems are operating normally\n", syscallname,
		  elapsed));
}

static int smb_time_audit_connect(vfs_handle_struct *handle,
				  const char *svc, const char *user)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	if (!handle) {
		return -1;
	}

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_CONNECT(handle, svc, user);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;
	if (timediff > audit_timeout) {
		smb_time_audit_log("connect", timediff);
	}
	return result;
}

static void smb_time_audit_disconnect(vfs_handle_struct *handle)
{
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	SMB_VFS_NEXT_DISCONNECT(handle);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("disconnect", timediff);
	}

	return;
}

static uint64_t smb_time_audit_disk_free(vfs_handle_struct *handle,
					 const char *path,
					 bool small_query, uint64_t *bsize,
					 uint64_t *dfree, uint64_t *dsize)
{
	uint64_t result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_DISK_FREE(handle, path, small_query, bsize,
					dfree, dsize);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	/* Don't have a reasonable notion of failure here */
	if (timediff > audit_timeout) {
		smb_time_audit_log("disk_free", timediff);
	}

	return result;
}

static int smb_time_audit_get_quota(struct vfs_handle_struct *handle,
				    enum SMB_QUOTA_TYPE qtype, unid_t id,
				    SMB_DISK_QUOTA *qt)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_GET_QUOTA(handle, qtype, id, qt);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("get_quota", timediff);
	}
	return result;
}

static int smb_time_audit_set_quota(struct vfs_handle_struct *handle,
				    enum SMB_QUOTA_TYPE qtype, unid_t id,
				    SMB_DISK_QUOTA *qt)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_SET_QUOTA(handle, qtype, id, qt);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("set_quota", timediff);
	}

	return result;
}

static int smb_time_audit_get_shadow_copy_data(struct vfs_handle_struct *handle,
					       struct files_struct *fsp,
					       struct shadow_copy_data *shadow_copy_data,
					       bool labels)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_GET_SHADOW_COPY_DATA(handle, fsp,
						   shadow_copy_data, labels);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("get_shadow_copy_data", timediff);
	}

	return result;
}

static int smb_time_audit_statvfs(struct vfs_handle_struct *handle,
				  const char *path,
				  struct vfs_statvfs_struct *statbuf)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_STATVFS(handle, path, statbuf);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("statvfs", timediff);
	}

	return result;
}

static uint32_t smb_time_audit_fs_capabilities(struct vfs_handle_struct *handle,
					       enum timestamp_set_resolution *p_ts_res)
{
	uint32_t result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_FS_CAPABILITIES(handle, p_ts_res);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("fs_capabilities", timediff);
	}

	return result;
}

static SMB_STRUCT_DIR *smb_time_audit_opendir(vfs_handle_struct *handle,
					      const char *fname,
					      const char *mask, uint32 attr)
{
	SMB_STRUCT_DIR *result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_OPENDIR(handle, fname, mask, attr);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("opendir", timediff);
	}

	return result;
}

static SMB_STRUCT_DIR *smb_time_audit_fdopendir(vfs_handle_struct *handle,
					      files_struct *fsp,
					      const char *mask, uint32 attr)
{
	SMB_STRUCT_DIR *result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_FDOPENDIR(handle, fsp, mask, attr);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("fdopendir", timediff);
	}

	return result;
}

static SMB_STRUCT_DIRENT *smb_time_audit_readdir(vfs_handle_struct *handle,
						 SMB_STRUCT_DIR *dirp,
						 SMB_STRUCT_STAT *sbuf)
{
	SMB_STRUCT_DIRENT *result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_READDIR(handle, dirp, sbuf);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("readdir", timediff);
	}

	return result;
}

static void smb_time_audit_seekdir(vfs_handle_struct *handle,
				   SMB_STRUCT_DIR *dirp, long offset)
{
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	SMB_VFS_NEXT_SEEKDIR(handle, dirp, offset);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("seekdir", timediff);
	}

	return;
}

static long smb_time_audit_telldir(vfs_handle_struct *handle,
				   SMB_STRUCT_DIR *dirp)
{
	long result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_TELLDIR(handle, dirp);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("telldir", timediff);
	}

	return result;
}

static void smb_time_audit_rewinddir(vfs_handle_struct *handle,
				     SMB_STRUCT_DIR *dirp)
{
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	SMB_VFS_NEXT_REWINDDIR(handle, dirp);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("rewinddir", timediff);
	}

	return;
}

static int smb_time_audit_mkdir(vfs_handle_struct *handle,
				const char *path, mode_t mode)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_MKDIR(handle, path, mode);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("mkdir", timediff);
	}

	return result;
}

static int smb_time_audit_rmdir(vfs_handle_struct *handle,
				const char *path)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_RMDIR(handle, path);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("rmdir", timediff);
	}

	return result;
}

static int smb_time_audit_closedir(vfs_handle_struct *handle,
				   SMB_STRUCT_DIR *dirp)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_CLOSEDIR(handle, dirp);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("closedir", timediff);
	}

	return result;
}

static void smb_time_audit_init_search_op(vfs_handle_struct *handle,
					  SMB_STRUCT_DIR *dirp)
{
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	SMB_VFS_NEXT_INIT_SEARCH_OP(handle, dirp);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("init_search_op", timediff);
	}
	return;
}

static int smb_time_audit_open(vfs_handle_struct *handle,
			       struct smb_filename *fname,
			       files_struct *fsp,
			       int flags, mode_t mode)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_OPEN(handle, fname, fsp, flags, mode);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("open", timediff);
	}

	return result;
}

static NTSTATUS smb_time_audit_create_file(vfs_handle_struct *handle,
					   struct smb_request *req,
					   uint16_t root_dir_fid,
					   struct smb_filename *fname,
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
					   files_struct **result_fsp,
					   int *pinfo)
{
	NTSTATUS result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_CREATE_FILE(
		handle,					/* handle */
		req,					/* req */
		root_dir_fid,				/* root_dir_fid */
		fname,					/* fname */
		access_mask,				/* access_mask */
		share_access,				/* share_access */
		create_disposition,			/* create_disposition*/
		create_options,				/* create_options */
		file_attributes,			/* file_attributes */
		oplock_request,				/* oplock_request */
		allocation_size,			/* allocation_size */
		private_flags,
		sd,					/* sd */
		ea_list,				/* ea_list */
		result_fsp,				/* result */
		pinfo);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("create_file", timediff);
	}

	return result;
}

static int smb_time_audit_close(vfs_handle_struct *handle, files_struct *fsp)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_CLOSE(handle, fsp);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("close", timediff);
	}

	return result;
}

static ssize_t smb_time_audit_read(vfs_handle_struct *handle,
				   files_struct *fsp, void *data, size_t n)
{
	ssize_t result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_READ(handle, fsp, data, n);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("read", timediff);
	}

	return result;
}

static ssize_t smb_time_audit_pread(vfs_handle_struct *handle,
				    files_struct *fsp,
				    void *data, size_t n, SMB_OFF_T offset)
{
	ssize_t result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_PREAD(handle, fsp, data, n, offset);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("pread", timediff);
	}

	return result;
}

static ssize_t smb_time_audit_write(vfs_handle_struct *handle,
				    files_struct *fsp,
				    const void *data, size_t n)
{
	ssize_t result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_WRITE(handle, fsp, data, n);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("write", timediff);
	}

	return result;
}

static ssize_t smb_time_audit_pwrite(vfs_handle_struct *handle,
				     files_struct *fsp,
				     const void *data, size_t n,
				     SMB_OFF_T offset)
{
	ssize_t result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_PWRITE(handle, fsp, data, n, offset);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("pwrite", timediff);
	}

	return result;
}

static SMB_OFF_T smb_time_audit_lseek(vfs_handle_struct *handle,
				      files_struct *fsp,
				      SMB_OFF_T offset, int whence)
{
	SMB_OFF_T result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_LSEEK(handle, fsp, offset, whence);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("lseek", timediff);
	}

	return result;
}

static ssize_t smb_time_audit_sendfile(vfs_handle_struct *handle, int tofd,
				       files_struct *fromfsp,
				       const DATA_BLOB *hdr, SMB_OFF_T offset,
				       size_t n)
{
	ssize_t result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_SENDFILE(handle, tofd, fromfsp, hdr, offset, n);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("sendfile", timediff);
	}

	return result;
}

static ssize_t smb_time_audit_recvfile(vfs_handle_struct *handle, int fromfd,
				       files_struct *tofsp,
				       SMB_OFF_T offset,
				       size_t n)
{
	ssize_t result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_RECVFILE(handle, fromfd, tofsp, offset, n);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("recvfile", timediff);
	}

	return result;
}

static int smb_time_audit_rename(vfs_handle_struct *handle,
				 const struct smb_filename *oldname,
				 const struct smb_filename *newname)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_RENAME(handle, oldname, newname);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("rename", timediff);
	}

	return result;
}

static int smb_time_audit_fsync(vfs_handle_struct *handle, files_struct *fsp)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_FSYNC(handle, fsp);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("fsync", timediff);
	}

	return result;
}

static int smb_time_audit_stat(vfs_handle_struct *handle,
			       struct smb_filename *fname)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_STAT(handle, fname);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("stat", timediff);
	}

	return result;
}

static int smb_time_audit_fstat(vfs_handle_struct *handle, files_struct *fsp,
				SMB_STRUCT_STAT *sbuf)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_FSTAT(handle, fsp, sbuf);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("fstat", timediff);
	}

	return result;
}

static int smb_time_audit_lstat(vfs_handle_struct *handle,
				struct smb_filename *path)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_LSTAT(handle, path);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("lstat", timediff);
	}

	return result;
}

static uint64_t smb_time_audit_get_alloc_size(vfs_handle_struct *handle,
					      files_struct *fsp,
					      const SMB_STRUCT_STAT *sbuf)
{
	uint64_t result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_GET_ALLOC_SIZE(handle, fsp, sbuf);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("get_alloc_size", timediff);
	}

	return result;
}

static int smb_time_audit_unlink(vfs_handle_struct *handle,
				 const struct smb_filename *path)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_UNLINK(handle, path);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("unlink", timediff);
	}

	return result;
}

static int smb_time_audit_chmod(vfs_handle_struct *handle,
				const char *path, mode_t mode)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_CHMOD(handle, path, mode);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("chmod", timediff);
	}

	return result;
}

static int smb_time_audit_fchmod(vfs_handle_struct *handle, files_struct *fsp,
				 mode_t mode)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_FCHMOD(handle, fsp, mode);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("fchmod", timediff);
	}

	return result;
}

static int smb_time_audit_chown(vfs_handle_struct *handle,
				const char *path, uid_t uid, gid_t gid)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_CHOWN(handle, path, uid, gid);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("chown", timediff);
	}

	return result;
}

static int smb_time_audit_fchown(vfs_handle_struct *handle, files_struct *fsp,
				 uid_t uid, gid_t gid)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_FCHOWN(handle, fsp, uid, gid);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("fchown", timediff);
	}

	return result;
}

static int smb_time_audit_lchown(vfs_handle_struct *handle,
				 const char *path, uid_t uid, gid_t gid)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_LCHOWN(handle, path, uid, gid);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("lchown", timediff);
	}

	return result;
}

static int smb_time_audit_chdir(vfs_handle_struct *handle, const char *path)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_CHDIR(handle, path);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("chdir", timediff);
	}

	return result;
}

static char *smb_time_audit_getwd(vfs_handle_struct *handle, char *path)
{
	char *result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_GETWD(handle, path);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("getwd", timediff);
	}

	return result;
}

static int smb_time_audit_ntimes(vfs_handle_struct *handle,
				 const struct smb_filename *path,
				 struct smb_file_time *ft)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_NTIMES(handle, path, ft);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("ntimes", timediff);
	}

	return result;
}

static int smb_time_audit_ftruncate(vfs_handle_struct *handle,
				    files_struct *fsp,
				    SMB_OFF_T len)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_FTRUNCATE(handle, fsp, len);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("ftruncate", timediff);
	}

	return result;
}

static int smb_time_audit_fallocate(vfs_handle_struct *handle,
				    files_struct *fsp,
				    enum vfs_fallocate_mode mode,
				    SMB_OFF_T offset,
				    SMB_OFF_T len)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_FALLOCATE(handle, fsp, mode, offset, len);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("fallocate", timediff);
	}

	return result;
}

static bool smb_time_audit_lock(vfs_handle_struct *handle, files_struct *fsp,
				int op, SMB_OFF_T offset, SMB_OFF_T count,
				int type)
{
	bool result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_LOCK(handle, fsp, op, offset, count, type);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("lock", timediff);
	}

	return result;
}

static int smb_time_audit_kernel_flock(struct vfs_handle_struct *handle,
				       struct files_struct *fsp,
				       uint32 share_mode, uint32 access_mask)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_KERNEL_FLOCK(handle, fsp, share_mode,
					   access_mask);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("kernel_flock", timediff);
	}

	return result;
}

static int smb_time_audit_linux_setlease(vfs_handle_struct *handle,
					 files_struct *fsp,
					 int leasetype)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_LINUX_SETLEASE(handle, fsp, leasetype);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("linux_setlease", timediff);
	}

	return result;
}

static bool smb_time_audit_getlock(vfs_handle_struct *handle,
				   files_struct *fsp,
				   SMB_OFF_T *poffset, SMB_OFF_T *pcount,
				   int *ptype, pid_t *ppid)
{
	bool result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_GETLOCK(handle, fsp, poffset, pcount, ptype,
				      ppid);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("getlock", timediff);
	}

	return result;
}

static int smb_time_audit_symlink(vfs_handle_struct *handle,
				  const char *oldpath, const char *newpath)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_SYMLINK(handle, oldpath, newpath);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("symlink", timediff);
	}

	return result;
}

static int smb_time_audit_readlink(vfs_handle_struct *handle,
			  const char *path, char *buf, size_t bufsiz)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_READLINK(handle, path, buf, bufsiz);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("readlink", timediff);
	}

	return result;
}

static int smb_time_audit_link(vfs_handle_struct *handle,
			       const char *oldpath, const char *newpath)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_LINK(handle, oldpath, newpath);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("link", timediff);
	}

	return result;
}

static int smb_time_audit_mknod(vfs_handle_struct *handle,
				const char *pathname, mode_t mode,
				SMB_DEV_T dev)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_MKNOD(handle, pathname, mode, dev);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("mknod", timediff);
	}

	return result;
}

static char *smb_time_audit_realpath(vfs_handle_struct *handle,
				     const char *path)
{
	char *result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_REALPATH(handle, path);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("realpath", timediff);
	}

	return result;
}

static NTSTATUS smb_time_audit_notify_watch(struct vfs_handle_struct *handle,
			struct sys_notify_context *ctx,
			struct notify_entry *e,
			void (*callback)(struct sys_notify_context *ctx,
					void *private_data,
					struct notify_event *ev),
			void *private_data, void *handle_p)
{
	NTSTATUS result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_NOTIFY_WATCH(handle, ctx, e, callback,
					   private_data, handle_p);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("notify_watch", timediff);
	}

	return result;
}

static int smb_time_audit_chflags(vfs_handle_struct *handle,
				  const char *path, unsigned int flags)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_CHFLAGS(handle, path, flags);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("chflags", timediff);
	}

	return result;
}

static struct file_id smb_time_audit_file_id_create(struct vfs_handle_struct *handle,
						    const SMB_STRUCT_STAT *sbuf)
{
	struct file_id id_zero;
	struct file_id result;
	struct timespec ts1,ts2;
	double timediff;

	ZERO_STRUCT(id_zero);

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_FILE_ID_CREATE(handle, sbuf);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("file_id_create", timediff);
	}

	return result;
}

static NTSTATUS smb_time_audit_streaminfo(vfs_handle_struct *handle,
					  struct files_struct *fsp,
					  const char *fname,
					  TALLOC_CTX *mem_ctx,
					  unsigned int *pnum_streams,
					  struct stream_struct **pstreams)
{
	NTSTATUS result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_STREAMINFO(handle, fsp, fname, mem_ctx,
					 pnum_streams, pstreams);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("streaminfo", timediff);
	}

	return result;
}

static int smb_time_audit_get_real_filename(struct vfs_handle_struct *handle,
					    const char *path,
					    const char *name,
					    TALLOC_CTX *mem_ctx,
					    char **found_name)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_GET_REAL_FILENAME(handle, path, name, mem_ctx,
						found_name);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("get_real_filename", timediff);
	}

	return result;
}

static const char *smb_time_audit_connectpath(vfs_handle_struct *handle,
					      const char *fname)
{
	const char *result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_CONNECTPATH(handle, fname);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("connectpath", timediff);
	}

	return result;
}

static NTSTATUS smb_time_audit_brl_lock_windows(struct vfs_handle_struct *handle,
						struct byte_range_lock *br_lck,
						struct lock_struct *plock,
						bool blocking_lock,
						struct blocking_lock_record *blr)
{
	NTSTATUS result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_BRL_LOCK_WINDOWS(handle, br_lck, plock,
					       blocking_lock, blr);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("brl_lock_windows", timediff);
	}

	return result;
}

static bool smb_time_audit_brl_unlock_windows(struct vfs_handle_struct *handle,
					      struct messaging_context *msg_ctx,
					      struct byte_range_lock *br_lck,
					      const struct lock_struct *plock)
{
	bool result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_BRL_UNLOCK_WINDOWS(handle, msg_ctx, br_lck,
						 plock);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("brl_unlock_windows", timediff);
	}

	return result;
}

static bool smb_time_audit_brl_cancel_windows(struct vfs_handle_struct *handle,
					      struct byte_range_lock *br_lck,
					      struct lock_struct *plock,
					      struct blocking_lock_record *blr)
{
	bool result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_BRL_CANCEL_WINDOWS(handle, br_lck, plock, blr);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("brl_cancel_windows", timediff);
	}

	return result;
}

static bool smb_time_audit_strict_lock(struct vfs_handle_struct *handle,
				       struct files_struct *fsp,
				       struct lock_struct *plock)
{
	bool result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_STRICT_LOCK(handle, fsp, plock);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("strict_lock", timediff);
	}

	return result;
}

static void smb_time_audit_strict_unlock(struct vfs_handle_struct *handle,
					 struct files_struct *fsp,
					 struct lock_struct *plock)
{
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	SMB_VFS_NEXT_STRICT_UNLOCK(handle, fsp, plock);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("strict_unlock", timediff);
	}

	return;
}

static NTSTATUS smb_time_audit_translate_name(struct vfs_handle_struct *handle,
					      const char *name,
					      enum vfs_translate_direction direction,
					      TALLOC_CTX *mem_ctx,
					      char **mapped_name)
{
	NTSTATUS result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_TRANSLATE_NAME(handle, name, direction, mem_ctx,
					     mapped_name);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("translate_name", timediff);
	}

	return result;
}

static NTSTATUS smb_time_audit_fget_nt_acl(vfs_handle_struct *handle,
					   files_struct *fsp,
					   uint32 security_info,
					   struct security_descriptor **ppdesc)
{
	NTSTATUS result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_FGET_NT_ACL(handle, fsp, security_info, ppdesc);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("fget_nt_acl", timediff);
	}

	return result;
}

static NTSTATUS smb_time_audit_get_nt_acl(vfs_handle_struct *handle,
					  const char *name,
					  uint32 security_info,
					  struct security_descriptor **ppdesc)
{
	NTSTATUS result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_GET_NT_ACL(handle, name, security_info, ppdesc);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("get_nt_acl", timediff);
	}

	return result;
}

static NTSTATUS smb_time_audit_fset_nt_acl(vfs_handle_struct *handle,
					   files_struct *fsp,
					   uint32 security_info_sent,
					   const struct security_descriptor *psd)
{
	NTSTATUS result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_FSET_NT_ACL(handle, fsp, security_info_sent,
					  psd);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("fset_nt_acl", timediff);
	}

	return result;
}

static int smb_time_audit_chmod_acl(vfs_handle_struct *handle,
				    const char *path, mode_t mode)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_CHMOD_ACL(handle, path, mode);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("chmod_acl", timediff);
	}

	return result;
}

static int smb_time_audit_fchmod_acl(vfs_handle_struct *handle,
				     files_struct *fsp, mode_t mode)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_FCHMOD_ACL(handle, fsp, mode);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("fchmod_acl", timediff);
	}

	return result;
}

static int smb_time_audit_sys_acl_get_entry(vfs_handle_struct *handle,
					    SMB_ACL_T theacl, int entry_id,
					    SMB_ACL_ENTRY_T *entry_p)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_SYS_ACL_GET_ENTRY(handle, theacl, entry_id,
						entry_p);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("sys_acl_get_entry", timediff);
	}

	return result;
}

static int smb_time_audit_sys_acl_get_tag_type(vfs_handle_struct *handle,
					       SMB_ACL_ENTRY_T entry_d,
					       SMB_ACL_TAG_T *tag_type_p)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_SYS_ACL_GET_TAG_TYPE(handle, entry_d,
						   tag_type_p);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("sys_acl_get_tag_type", timediff);
	}

	return result;
}

static int smb_time_audit_sys_acl_get_permset(vfs_handle_struct *handle,
					      SMB_ACL_ENTRY_T entry_d,
					      SMB_ACL_PERMSET_T *permset_p)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_SYS_ACL_GET_PERMSET(handle, entry_d,
						  permset_p);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("sys_acl_get_permset", timediff);
	}

	return result;
}

static void * smb_time_audit_sys_acl_get_qualifier(vfs_handle_struct *handle,
						   SMB_ACL_ENTRY_T entry_d)
{
	void *result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_SYS_ACL_GET_QUALIFIER(handle, entry_d);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("sys_acl_get_qualifier", timediff);
	}

	return result;
}

static SMB_ACL_T smb_time_audit_sys_acl_get_file(vfs_handle_struct *handle,
						 const char *path_p,
						 SMB_ACL_TYPE_T type)
{
	SMB_ACL_T result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_SYS_ACL_GET_FILE(handle, path_p, type);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("sys_acl_get_file", timediff);
	}

	return result;
}

static SMB_ACL_T smb_time_audit_sys_acl_get_fd(vfs_handle_struct *handle,
					       files_struct *fsp)
{
	SMB_ACL_T result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_SYS_ACL_GET_FD(handle, fsp);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("sys_acl_get_fd", timediff);
	}

	return result;
}

static int smb_time_audit_sys_acl_clear_perms(vfs_handle_struct *handle,
					      SMB_ACL_PERMSET_T permset)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_SYS_ACL_CLEAR_PERMS(handle, permset);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("sys_acl_clear_perms", timediff);
	}

	return result;
}

static int smb_time_audit_sys_acl_add_perm(vfs_handle_struct *handle,
					   SMB_ACL_PERMSET_T permset,
					   SMB_ACL_PERM_T perm)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_SYS_ACL_ADD_PERM(handle, permset, perm);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("sys_acl_add_perm", timediff);
	}

	return result;
}

static char * smb_time_audit_sys_acl_to_text(vfs_handle_struct *handle,
					     SMB_ACL_T theacl,
					     ssize_t *plen)
{
	char * result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_SYS_ACL_TO_TEXT(handle, theacl, plen);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("sys_acl_to_text", timediff);
	}

	return result;
}

static SMB_ACL_T smb_time_audit_sys_acl_init(vfs_handle_struct *handle,
					     int count)
{
	SMB_ACL_T result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_SYS_ACL_INIT(handle, count);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("sys_acl_init", timediff);
	}

	return result;
}

static int smb_time_audit_sys_acl_create_entry(vfs_handle_struct *handle,
					       SMB_ACL_T *pacl,
					       SMB_ACL_ENTRY_T *pentry)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_SYS_ACL_CREATE_ENTRY(handle, pacl, pentry);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("sys_acl_create_entry", timediff);
	}

	return result;
}

static int smb_time_audit_sys_acl_set_tag_type(vfs_handle_struct *handle,
					       SMB_ACL_ENTRY_T entry,
					       SMB_ACL_TAG_T tagtype)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_SYS_ACL_SET_TAG_TYPE(handle, entry,
						   tagtype);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("sys_acl_set_tag_type", timediff);
	}

	return result;
}

static int smb_time_audit_sys_acl_set_qualifier(vfs_handle_struct *handle,
						SMB_ACL_ENTRY_T entry,
						void *qual)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_SYS_ACL_SET_QUALIFIER(handle, entry, qual);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("sys_acl_set_qualifier", timediff);
	}

	return result;
}

static int smb_time_audit_sys_acl_set_permset(vfs_handle_struct *handle,
					      SMB_ACL_ENTRY_T entry,
					      SMB_ACL_PERMSET_T permset)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_SYS_ACL_SET_PERMSET(handle, entry, permset);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("sys_acl_set_permset", timediff);
	}

	return result;
}

static int smb_time_audit_sys_acl_valid(vfs_handle_struct *handle,
					SMB_ACL_T theacl)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_SYS_ACL_VALID(handle, theacl);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("sys_acl_valid", timediff);
	}

	return result;
}

static int smb_time_audit_sys_acl_set_file(vfs_handle_struct *handle,
					   const char *name,
					   SMB_ACL_TYPE_T acltype,
					   SMB_ACL_T theacl)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_SYS_ACL_SET_FILE(handle, name, acltype,
					       theacl);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("sys_acl_set_file", timediff);
	}

	return result;
}

static int smb_time_audit_sys_acl_set_fd(vfs_handle_struct *handle,
					 files_struct *fsp,
					 SMB_ACL_T theacl)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_SYS_ACL_SET_FD(handle, fsp, theacl);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("sys_acl_set_fd", timediff);
	}

	return result;
}

static int smb_time_audit_sys_acl_delete_def_file(vfs_handle_struct *handle,
						  const char *path)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_SYS_ACL_DELETE_DEF_FILE(handle, path);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("sys_acl_delete_def_file", timediff);
	}

	return result;
}

static int smb_time_audit_sys_acl_get_perm(vfs_handle_struct *handle,
					   SMB_ACL_PERMSET_T permset,
					   SMB_ACL_PERM_T perm)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_SYS_ACL_GET_PERM(handle, permset, perm);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("sys_acl_get_perm", timediff);
	}

	return result;
}

static int smb_time_audit_sys_acl_free_text(vfs_handle_struct *handle,
					    char *text)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_SYS_ACL_FREE_TEXT(handle, text);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("sys_acl_free_text", timediff);
	}

	return result;
}

static int smb_time_audit_sys_acl_free_acl(vfs_handle_struct *handle,
					   SMB_ACL_T posix_acl)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_SYS_ACL_FREE_ACL(handle, posix_acl);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("sys_acl_free_acl", timediff);
	}

	return result;
}

static int smb_time_audit_sys_acl_free_qualifier(vfs_handle_struct *handle,
						 void *qualifier,
						 SMB_ACL_TAG_T tagtype)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_SYS_ACL_FREE_QUALIFIER(handle, qualifier,
						     tagtype);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("sys_acl_free_qualifier", timediff);
	}

	return result;
}

static ssize_t smb_time_audit_getxattr(struct vfs_handle_struct *handle,
				       const char *path, const char *name,
				       void *value, size_t size)
{
	ssize_t result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_GETXATTR(handle, path, name, value, size);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("getxattr", timediff);
	}

	return result;
}

static ssize_t smb_time_audit_lgetxattr(struct vfs_handle_struct *handle,
					const char *path, const char *name,
					void *value, size_t size)
{
	ssize_t result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_LGETXATTR(handle, path, name, value, size);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("lgetxattr", timediff);
	}

	return result;
}

static ssize_t smb_time_audit_fgetxattr(struct vfs_handle_struct *handle,
					struct files_struct *fsp,
					const char *name, void *value,
					size_t size)
{
	ssize_t result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_FGETXATTR(handle, fsp, name, value, size);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("fgetxattr", timediff);
	}

	return result;
}

static ssize_t smb_time_audit_listxattr(struct vfs_handle_struct *handle,
					const char *path, char *list,
					size_t size)
{
	ssize_t result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_LISTXATTR(handle, path, list, size);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("listxattr", timediff);
	}

	return result;
}

static ssize_t smb_time_audit_llistxattr(struct vfs_handle_struct *handle,
					 const char *path, char *list,
					 size_t size)
{
	ssize_t result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_LLISTXATTR(handle, path, list, size);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("llistxattr", timediff);
	}

	return result;
}

static ssize_t smb_time_audit_flistxattr(struct vfs_handle_struct *handle,
					 struct files_struct *fsp, char *list,
					 size_t size)
{
	ssize_t result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_FLISTXATTR(handle, fsp, list, size);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("flistxattr", timediff);
	}

	return result;
}

static int smb_time_audit_removexattr(struct vfs_handle_struct *handle,
				      const char *path, const char *name)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_REMOVEXATTR(handle, path, name);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("removexattr", timediff);
	}

	return result;
}

static int smb_time_audit_lremovexattr(struct vfs_handle_struct *handle,
				       const char *path, const char *name)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_LREMOVEXATTR(handle, path, name);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("lremovexattr", timediff);
	}

	return result;
}

static int smb_time_audit_fremovexattr(struct vfs_handle_struct *handle,
				       struct files_struct *fsp,
				       const char *name)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_FREMOVEXATTR(handle, fsp, name);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("fremovexattr", timediff);
	}

	return result;
}

static int smb_time_audit_setxattr(struct vfs_handle_struct *handle,
				   const char *path, const char *name,
				   const void *value, size_t size,
				   int flags)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_SETXATTR(handle, path, name, value, size,
				       flags);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("setxattr", timediff);
	}

	return result;
}

static int smb_time_audit_lsetxattr(struct vfs_handle_struct *handle,
				    const char *path, const char *name,
				    const void *value, size_t size,
				    int flags)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_LSETXATTR(handle, path, name, value, size,
					flags);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("lsetxattr", timediff);
	}

	return result;
}

static int smb_time_audit_fsetxattr(struct vfs_handle_struct *handle,
				    struct files_struct *fsp, const char *name,
				    const void *value, size_t size, int flags)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_FSETXATTR(handle, fsp, name, value, size, flags);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("fsetxattr", timediff);
	}

	return result;
}

static int smb_time_audit_aio_read(struct vfs_handle_struct *handle,
				   struct files_struct *fsp,
				   SMB_STRUCT_AIOCB *aiocb)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_AIO_READ(handle, fsp, aiocb);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("aio_read", timediff);
	}

	return result;
}

static int smb_time_audit_aio_write(struct vfs_handle_struct *handle,
				    struct files_struct *fsp,
				    SMB_STRUCT_AIOCB *aiocb)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_AIO_WRITE(handle, fsp, aiocb);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("aio_write", timediff);
	}

	return result;
}

static ssize_t smb_time_audit_aio_return(struct vfs_handle_struct *handle,
					 struct files_struct *fsp,
					 SMB_STRUCT_AIOCB *aiocb)
{
	ssize_t result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_AIO_RETURN(handle, fsp, aiocb);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("aio_return", timediff);
	}

	return result;
}

static int smb_time_audit_aio_cancel(struct vfs_handle_struct *handle,
				     struct files_struct *fsp,
				     SMB_STRUCT_AIOCB *aiocb)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_AIO_CANCEL(handle, fsp, aiocb);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("aio_cancel", timediff);
	}

	return result;
}

static int smb_time_audit_aio_error(struct vfs_handle_struct *handle,
				    struct files_struct *fsp,
				    SMB_STRUCT_AIOCB *aiocb)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_AIO_ERROR(handle, fsp, aiocb);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("aio_error", timediff);
	}

	return result;
}

static int smb_time_audit_aio_fsync(struct vfs_handle_struct *handle,
				    struct files_struct *fsp, int op,
				    SMB_STRUCT_AIOCB *aiocb)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_AIO_FSYNC(handle, fsp, op, aiocb);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("aio_fsync", timediff);
	}

	return result;
}

static int smb_time_audit_aio_suspend(struct vfs_handle_struct *handle,
				      struct files_struct *fsp,
				      const SMB_STRUCT_AIOCB * const aiocb[],
				      int n, const struct timespec *ts)
{
	int result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_AIO_SUSPEND(handle, fsp, aiocb, n, ts);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("aio_suspend", timediff);
	}

	return result;
}

static bool smb_time_audit_aio_force(struct vfs_handle_struct *handle,
				     struct files_struct *fsp)
{
	bool result;
	struct timespec ts1,ts2;
	double timediff;

	clock_gettime_mono(&ts1);
	result = SMB_VFS_NEXT_AIO_FORCE(handle, fsp);
	clock_gettime_mono(&ts2);
	timediff = nsec_time_diff(&ts2,&ts1)*1.0e-9;

	if (timediff > audit_timeout) {
		smb_time_audit_log("aio_force", timediff);
	}

	return result;
}



/* VFS operations */

static struct vfs_fn_pointers vfs_time_audit_fns = {
	.connect_fn = smb_time_audit_connect,
	.disconnect = smb_time_audit_disconnect,
	.disk_free = smb_time_audit_disk_free,
	.get_quota = smb_time_audit_get_quota,
	.set_quota = smb_time_audit_set_quota,
	.get_shadow_copy_data = smb_time_audit_get_shadow_copy_data,
	.statvfs = smb_time_audit_statvfs,
	.fs_capabilities = smb_time_audit_fs_capabilities,
	.opendir = smb_time_audit_opendir,
	.fdopendir = smb_time_audit_fdopendir,
	.readdir = smb_time_audit_readdir,
	.seekdir = smb_time_audit_seekdir,
	.telldir = smb_time_audit_telldir,
	.rewind_dir = smb_time_audit_rewinddir,
	.mkdir = smb_time_audit_mkdir,
	.rmdir = smb_time_audit_rmdir,
	.closedir = smb_time_audit_closedir,
	.init_search_op = smb_time_audit_init_search_op,
	.open_fn = smb_time_audit_open,
	.create_file = smb_time_audit_create_file,
	.close_fn = smb_time_audit_close,
	.vfs_read = smb_time_audit_read,
	.pread = smb_time_audit_pread,
	.write = smb_time_audit_write,
	.pwrite = smb_time_audit_pwrite,
	.lseek = smb_time_audit_lseek,
	.sendfile = smb_time_audit_sendfile,
	.recvfile = smb_time_audit_recvfile,
	.rename = smb_time_audit_rename,
	.fsync = smb_time_audit_fsync,
	.stat = smb_time_audit_stat,
	.fstat = smb_time_audit_fstat,
	.lstat = smb_time_audit_lstat,
	.get_alloc_size = smb_time_audit_get_alloc_size,
	.unlink = smb_time_audit_unlink,
	.chmod = smb_time_audit_chmod,
	.fchmod = smb_time_audit_fchmod,
	.chown = smb_time_audit_chown,
	.fchown = smb_time_audit_fchown,
	.lchown = smb_time_audit_lchown,
	.chdir = smb_time_audit_chdir,
	.getwd = smb_time_audit_getwd,
	.ntimes = smb_time_audit_ntimes,
	.ftruncate = smb_time_audit_ftruncate,
	.fallocate = smb_time_audit_fallocate,
	.lock = smb_time_audit_lock,
	.kernel_flock = smb_time_audit_kernel_flock,
	.linux_setlease = smb_time_audit_linux_setlease,
	.getlock = smb_time_audit_getlock,
	.symlink = smb_time_audit_symlink,
	.vfs_readlink = smb_time_audit_readlink,
	.link = smb_time_audit_link,
	.mknod = smb_time_audit_mknod,
	.realpath = smb_time_audit_realpath,
	.notify_watch = smb_time_audit_notify_watch,
	.chflags = smb_time_audit_chflags,
	.file_id_create = smb_time_audit_file_id_create,
	.streaminfo = smb_time_audit_streaminfo,
	.get_real_filename = smb_time_audit_get_real_filename,
	.connectpath = smb_time_audit_connectpath,
	.brl_lock_windows = smb_time_audit_brl_lock_windows,
	.brl_unlock_windows = smb_time_audit_brl_unlock_windows,
	.brl_cancel_windows = smb_time_audit_brl_cancel_windows,
	.strict_lock = smb_time_audit_strict_lock,
	.strict_unlock = smb_time_audit_strict_unlock,
	.translate_name = smb_time_audit_translate_name,
	.fget_nt_acl = smb_time_audit_fget_nt_acl,
	.get_nt_acl = smb_time_audit_get_nt_acl,
	.fset_nt_acl = smb_time_audit_fset_nt_acl,
	.chmod_acl = smb_time_audit_chmod_acl,
	.fchmod_acl = smb_time_audit_fchmod_acl,
	.sys_acl_get_entry = smb_time_audit_sys_acl_get_entry,
	.sys_acl_get_tag_type = smb_time_audit_sys_acl_get_tag_type,
	.sys_acl_get_permset = smb_time_audit_sys_acl_get_permset,
	.sys_acl_get_qualifier = smb_time_audit_sys_acl_get_qualifier,
	.sys_acl_get_file = smb_time_audit_sys_acl_get_file,
	.sys_acl_get_fd = smb_time_audit_sys_acl_get_fd,
	.sys_acl_clear_perms = smb_time_audit_sys_acl_clear_perms,
	.sys_acl_add_perm = smb_time_audit_sys_acl_add_perm,
	.sys_acl_to_text = smb_time_audit_sys_acl_to_text,
	.sys_acl_init = smb_time_audit_sys_acl_init,
	.sys_acl_create_entry = smb_time_audit_sys_acl_create_entry,
	.sys_acl_set_tag_type = smb_time_audit_sys_acl_set_tag_type,
	.sys_acl_set_qualifier = smb_time_audit_sys_acl_set_qualifier,
	.sys_acl_set_permset = smb_time_audit_sys_acl_set_permset,
	.sys_acl_valid = smb_time_audit_sys_acl_valid,
	.sys_acl_set_file = smb_time_audit_sys_acl_set_file,
	.sys_acl_set_fd = smb_time_audit_sys_acl_set_fd,
	.sys_acl_delete_def_file = smb_time_audit_sys_acl_delete_def_file,
	.sys_acl_get_perm = smb_time_audit_sys_acl_get_perm,
	.sys_acl_free_text = smb_time_audit_sys_acl_free_text,
	.sys_acl_free_acl = smb_time_audit_sys_acl_free_acl,
	.sys_acl_free_qualifier = smb_time_audit_sys_acl_free_qualifier,
	.getxattr = smb_time_audit_getxattr,
	.lgetxattr = smb_time_audit_lgetxattr,
	.fgetxattr = smb_time_audit_fgetxattr,
	.listxattr = smb_time_audit_listxattr,
	.llistxattr = smb_time_audit_llistxattr,
	.flistxattr = smb_time_audit_flistxattr,
	.removexattr = smb_time_audit_removexattr,
	.lremovexattr = smb_time_audit_lremovexattr,
	.fremovexattr = smb_time_audit_fremovexattr,
	.setxattr = smb_time_audit_setxattr,
	.lsetxattr = smb_time_audit_lsetxattr,
	.fsetxattr = smb_time_audit_fsetxattr,
	.aio_read = smb_time_audit_aio_read,
	.aio_write = smb_time_audit_aio_write,
	.aio_return_fn = smb_time_audit_aio_return,
	.aio_cancel = smb_time_audit_aio_cancel,
	.aio_error_fn = smb_time_audit_aio_error,
	.aio_fsync = smb_time_audit_aio_fsync,
	.aio_suspend = smb_time_audit_aio_suspend,
	.aio_force = smb_time_audit_aio_force,
};


NTSTATUS vfs_time_audit_init(void);
NTSTATUS vfs_time_audit_init(void)
{
	audit_timeout = (double)lp_parm_int(-1, "time_audit", "timeout",
					    10000) / 1000.0;
	return smb_register_vfs(SMB_VFS_INTERFACE_VERSION, "time_audit",
				&vfs_time_audit_fns);
}
