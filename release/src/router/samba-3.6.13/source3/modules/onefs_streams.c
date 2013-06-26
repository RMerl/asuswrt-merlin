/*
 * Unix SMB/CIFS implementation.
 *
 * Support for OneFS Alternate Data Streams
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

#include <sys/isi_enc.h>

NTSTATUS onefs_stream_prep_smb_fname(TALLOC_CTX *ctx,
				     const struct smb_filename *smb_fname_in,
				     struct smb_filename **smb_fname_out)
{
	char *stream_name = NULL;
	NTSTATUS status;

	/*
	 * Only attempt to strip off the trailing :$DATA if there is an actual
	 * stream there.  If it is the default stream, the smb_fname_out will
	 * just have a NULL stream so the base file is opened.
	 */
	if (smb_fname_in->stream_name &&
	    !is_ntfs_default_stream_smb_fname(smb_fname_in)) {
		char *str_tmp = smb_fname_in->stream_name;

		/* First strip off the leading ':' */
		if (str_tmp[0] == ':') {
			str_tmp++;
		}

		/* Create a new copy of the stream_name. */
		stream_name = talloc_strdup(ctx, str_tmp);
		if (stream_name == NULL) {
			return NT_STATUS_NO_MEMORY;
		}

		/* Strip off the :$DATA if one exists. */
		str_tmp = strrchr_m(stream_name, ':');
		if (str_tmp) {
			if (StrCaseCmp(str_tmp, ":$DATA") != 0) {
				return NT_STATUS_INVALID_PARAMETER;
			}
			str_tmp[0] = '\0';
		}
	}

	/*
	 * If there was a stream that wasn't the default stream the leading
	 * colon and trailing :$DATA has now been stripped off.  Create a new
	 * smb_filename to pass back.
	 */
	status = create_synthetic_smb_fname(ctx, smb_fname_in->base_name,
					    stream_name, &smb_fname_in->st,
					    smb_fname_out);
	TALLOC_FREE(stream_name);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(5, ("Failed to prep stream name for %s: %s\n",
			  *smb_fname_out ?
			  smb_fname_str_dbg(*smb_fname_out) : "NULL",
			  nt_errstr(status)));
	}
	return status;
}

int onefs_close(vfs_handle_struct *handle, struct files_struct *fsp)
{
	int ret2, ret = 0;

	if (fsp->base_fsp) {
		ret = SMB_VFS_NEXT_CLOSE(handle, fsp->base_fsp);
	}
	ret2 = SMB_VFS_NEXT_CLOSE(handle, fsp);

	return ret ? ret : ret2;
}

/*
 * Get the ADS directory fd for a file.
 */
static int get_stream_dir_fd(connection_struct *conn, const char *base,
			     int *base_fdp)
{
	int base_fd;
	int dir_fd;
	int saved_errno;

	DEBUG(10, ("Getting stream directory fd: %s (%d)\n", base,
		   base_fdp ? *base_fdp : -1));

	/* If a valid base_fdp was given, use it. */
	if (base_fdp && *base_fdp >= 0) {
		base_fd = *base_fdp;
	} else {
		base_fd = onefs_sys_create_file(conn,
						-1,
						base,
						0,
						0,
						0,
						0,
						0,
						0,
						INTERNAL_OPEN_ONLY,
						0,
						NULL,
						0,
						NULL);
		if (base_fd < 0) {
			DEBUG(5, ("Failed getting base fd: %s\n",
				  strerror(errno)));
			return -1;
		}
	}

	/* Open the ADS directory. */
	dir_fd = onefs_sys_create_file(conn,
					base_fd,
					".",
					0,
					FILE_READ_DATA,
					0,
					0,
					0,
					0,
					INTERNAL_OPEN_ONLY,
					0,
					NULL,
					0,
					NULL);

	/* Close base_fd if it's not need or on error. */
	if (!base_fdp || dir_fd < 0) {
		saved_errno = errno;
		close(base_fd);
		errno = saved_errno;
	}

	/* Set the out base_fdp if successful and it was requested. */
	if (base_fdp && dir_fd >= 0) {
		*base_fdp = base_fd;
	}

	if (dir_fd < 0) {
		DEBUG(5, ("Failed getting stream directory fd: %s\n",
			  strerror(errno)));
	}

	return dir_fd;
}

int onefs_rename(vfs_handle_struct *handle,
		 const struct smb_filename *smb_fname_src,
		 const struct smb_filename *smb_fname_dst)
{
	struct smb_filename *smb_fname_src_onefs = NULL;
	struct smb_filename *smb_fname_dst_onefs = NULL;
	NTSTATUS status;
	int saved_errno;
	int dir_fd = -1;
	int ret = -1;

	START_PROFILE(syscall_rename_at);

	if (!is_ntfs_stream_smb_fname(smb_fname_src) &&
	    !is_ntfs_stream_smb_fname(smb_fname_dst)) {
		ret = SMB_VFS_NEXT_RENAME(handle, smb_fname_src,
					  smb_fname_dst);
		goto done;
	}

	/* For now don't allow renames from or to the default stream. */
	if (is_ntfs_default_stream_smb_fname(smb_fname_src) ||
	    is_ntfs_default_stream_smb_fname(smb_fname_dst)) {
		DEBUG(3, ("Unable to rename to/from a default stream: %s -> "
			  "%s\n", smb_fname_str_dbg(smb_fname_src),
			  smb_fname_str_dbg(smb_fname_dst)));
		errno = ENOSYS;
		goto done;
	}

	/* prep stream smb_filename structs. */
	status = onefs_stream_prep_smb_fname(talloc_tos(), smb_fname_src,
					     &smb_fname_src_onefs);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		goto done;
	}
	status = onefs_stream_prep_smb_fname(talloc_tos(), smb_fname_dst,
					     &smb_fname_dst_onefs);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		goto done;
	}

	dir_fd = get_stream_dir_fd(handle->conn, smb_fname_src->base_name,
				   NULL);
	if (dir_fd < -1) {
		goto done;
	}

	DEBUG(8, ("onefs_rename called for %s => %s\n",
		  smb_fname_str_dbg(smb_fname_src_onefs),
		  smb_fname_str_dbg(smb_fname_dst_onefs)));

	/* Handle rename of stream to default stream specially. */
	if (smb_fname_dst_onefs->stream_name == NULL) {
		ret = enc_renameat(dir_fd, smb_fname_src_onefs->stream_name,
				   ENC_DEFAULT, AT_FDCWD,
				   smb_fname_dst_onefs->base_name,
				   ENC_DEFAULT);
	} else {
		ret = enc_renameat(dir_fd, smb_fname_src_onefs->stream_name,
				   ENC_DEFAULT, dir_fd,
				   smb_fname_dst_onefs->stream_name,
				   ENC_DEFAULT);
	}

 done:
	END_PROFILE(syscall_rename_at);
	TALLOC_FREE(smb_fname_src_onefs);
	TALLOC_FREE(smb_fname_dst_onefs);

	saved_errno = errno;
	if (dir_fd >= 0) {
		close(dir_fd);
	}
	errno = saved_errno;
	return ret;
}

/*
 * Merge a base file's sbuf into the a streams's sbuf.
 */
static void merge_stat(SMB_STRUCT_STAT *stream_sbuf,
		       const SMB_STRUCT_STAT *base_sbuf)
{
	int dos_flags = (UF_DOS_NOINDEX | UF_DOS_ARCHIVE |
	    UF_DOS_HIDDEN | UF_DOS_RO | UF_DOS_SYSTEM);
	stream_sbuf->st_ex_mtime = base_sbuf->st_ex_mtime;
	stream_sbuf->st_ex_ctime = base_sbuf->st_ex_ctime;
	stream_sbuf->st_ex_atime = base_sbuf->st_ex_atime;
	stream_sbuf->st_ex_flags &= ~dos_flags;
	stream_sbuf->st_ex_flags |= base_sbuf->st_ex_flags & dos_flags;
}

/* fake timestamps */
static void onefs_adjust_stat_time(struct connection_struct *conn,
				   const char *fname, SMB_STRUCT_STAT *sbuf)
{
	struct onefs_vfs_share_config cfg;
	struct timeval tv_now = {0, 0};
	bool static_mtime = False;
	bool static_atime = False;

	if (!onefs_get_config(SNUM(conn),
			      ONEFS_VFS_CONFIG_FAKETIMESTAMPS, &cfg)) {
		return;
	}

	if (IS_MTIME_STATIC_PATH(conn, &cfg, fname)) {
		sbuf->st_ex_mtime = sbuf->st_ex_btime;
		static_mtime = True;
	}
	if (IS_ATIME_STATIC_PATH(conn, &cfg, fname)) {
		sbuf->st_ex_atime = sbuf->st_ex_btime;
		static_atime = True;
	}

	if (IS_CTIME_NOW_PATH(conn, &cfg, fname)) {
		if (cfg.ctime_slop < 0) {
			sbuf->st_ex_btime.tv_sec = INT_MAX - 1;
		} else {
			GetTimeOfDay(&tv_now);
			sbuf->st_ex_btime.tv_sec = tv_now.tv_sec +
			    cfg.ctime_slop;
		}
	}

	if (!static_mtime && IS_MTIME_NOW_PATH(conn,&cfg,fname)) {
		if (cfg.mtime_slop < 0) {
			sbuf->st_ex_mtime.tv_sec = INT_MAX - 1;
		} else {
			if (tv_now.tv_sec == 0)
				GetTimeOfDay(&tv_now);
			sbuf->st_ex_mtime.tv_sec = tv_now.tv_sec +
			    cfg.mtime_slop;
		}
	}
	if (!static_atime && IS_ATIME_NOW_PATH(conn,&cfg,fname)) {
		if (cfg.atime_slop < 0) {
			sbuf->st_ex_atime.tv_sec = INT_MAX - 1;
		} else {
			if (tv_now.tv_sec == 0)
				GetTimeOfDay(&tv_now);
			sbuf->st_ex_atime.tv_sec = tv_now.tv_sec +
			    cfg.atime_slop;
		}
	}
}

static int stat_stream(struct connection_struct *conn, const char *base,
		       const char *stream, SMB_STRUCT_STAT *sbuf, int flags)
{
	SMB_STRUCT_STAT base_sbuf;
	int base_fd = -1, dir_fd, ret, saved_errno;

	dir_fd = get_stream_dir_fd(conn, base, &base_fd);
	if (dir_fd < 0) {
		return -1;
	}

	/* Stat the stream. */
	ret = onefs_sys_fstat_at(dir_fd, stream, sbuf, flags);
	if (ret != -1) {
		DEBUG(10, ("stat of stream '%s' failed: %s\n", stream,
			   strerror(errno)));
	} else {
		/* Now stat the base file and merge the results. */
		ret = onefs_sys_fstat(base_fd, &base_sbuf);
		if (ret != -1) {
			merge_stat(sbuf, &base_sbuf);
		}
	}

	saved_errno = errno;
	close(dir_fd);
	close(base_fd);
	errno = saved_errno;
	return ret;
}

int onefs_stat(vfs_handle_struct *handle, struct smb_filename *smb_fname)
{
	struct smb_filename *smb_fname_onefs = NULL;
	NTSTATUS status;
	int ret;

	status = onefs_stream_prep_smb_fname(talloc_tos(), smb_fname,
					     &smb_fname_onefs);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		return -1;
	}

	/*
	 * If the smb_fname has no stream or is :$DATA, then just stat the
	 * base stream. Otherwise stat the stream.
	 */
	if (!is_ntfs_stream_smb_fname(smb_fname_onefs)) {
		ret = onefs_sys_stat(smb_fname_onefs->base_name,
				     &smb_fname->st);
	} else {
		ret = stat_stream(handle->conn, smb_fname_onefs->base_name,
				  smb_fname_onefs->stream_name, &smb_fname->st,
				  0);
	}

	onefs_adjust_stat_time(handle->conn, smb_fname->base_name,
			       &smb_fname->st);

	TALLOC_FREE(smb_fname_onefs);

	return ret;
}

int onefs_fstat(vfs_handle_struct *handle, struct files_struct *fsp,
		SMB_STRUCT_STAT *sbuf)
{
	SMB_STRUCT_STAT base_sbuf;
	int ret;

	/* Stat the stream, by calling next_fstat on the stream's fd. */
	ret = onefs_sys_fstat(fsp->fh->fd, sbuf);
	if (ret == -1) {
		return ret;
	}

	/* Stat the base file and merge the results. */
	if (fsp != NULL && fsp->base_fsp != NULL) {
		ret = onefs_sys_fstat(fsp->base_fsp->fh->fd, &base_sbuf);
		if (ret != -1) {
			merge_stat(sbuf, &base_sbuf);
		}
	}

	onefs_adjust_stat_time(handle->conn, fsp->fsp_name->base_name, sbuf);
	return ret;
}

int onefs_lstat(vfs_handle_struct *handle, struct smb_filename *smb_fname)
{
	struct smb_filename *smb_fname_onefs = NULL;
	NTSTATUS status;
	int ret;

	status = onefs_stream_prep_smb_fname(talloc_tos(), smb_fname,
					     &smb_fname_onefs);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		return -1;
	}

	/*
	 * If the smb_fname has no stream or is :$DATA, then just stat the
	 * base stream. Otherwise stat the stream.
	 */
	if (!is_ntfs_stream_smb_fname(smb_fname_onefs)) {
		ret = onefs_sys_lstat(smb_fname_onefs->base_name,
				      &smb_fname->st);
	} else {
		ret = stat_stream(handle->conn, smb_fname_onefs->base_name,
				  smb_fname_onefs->stream_name, &smb_fname->st,
				  AT_SYMLINK_NOFOLLOW);
	}

	onefs_adjust_stat_time(handle->conn, smb_fname->base_name,
			       &smb_fname->st);

	TALLOC_FREE(smb_fname_onefs);

	return ret;
}

int onefs_unlink(vfs_handle_struct *handle,
		 const struct smb_filename *smb_fname)
{
	struct smb_filename *smb_fname_onefs = NULL;
	int ret;
	int dir_fd, saved_errno;
	NTSTATUS status;

	/* Not a stream. */
	if (!is_ntfs_stream_smb_fname(smb_fname)) {
		return SMB_VFS_NEXT_UNLINK(handle, smb_fname);
	}

	status = onefs_stream_prep_smb_fname(talloc_tos(), smb_fname,
					     &smb_fname_onefs);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		return -1;
	}

	/* Default stream (the ::$DATA was just stripped off). */
	if (!is_ntfs_stream_smb_fname(smb_fname_onefs)) {
		ret = SMB_VFS_NEXT_UNLINK(handle, smb_fname_onefs);
		goto out;
	}

	dir_fd = get_stream_dir_fd(handle->conn, smb_fname_onefs->base_name,
				   NULL);
	if (dir_fd < 0) {
		ret = -1;
		goto out;
	}

	ret = enc_unlinkat(dir_fd, smb_fname_onefs->stream_name, ENC_DEFAULT,
			   0);

	saved_errno = errno;
	close(dir_fd);
	errno = saved_errno;
 out:
	TALLOC_FREE(smb_fname_onefs);
	return ret;
}

int onefs_vtimes_streams(vfs_handle_struct *handle,
			 const struct smb_filename *smb_fname,
			 int flags, struct timespec times[3])
{
	struct smb_filename *smb_fname_onefs = NULL;
	int ret;
	int dirfd;
	int saved_errno;
	NTSTATUS status;

	START_PROFILE(syscall_ntimes);

	if (!is_ntfs_stream_smb_fname(smb_fname)) {
		ret = vtimes(smb_fname->base_name, times, flags);
		return ret;
	}

	status = onefs_stream_prep_smb_fname(talloc_tos(), smb_fname,
					     &smb_fname_onefs);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		return -1;
	}

	/* Default stream (the ::$DATA was just stripped off). */
	if (!is_ntfs_stream_smb_fname(smb_fname_onefs)) {
		ret = vtimes(smb_fname_onefs->base_name, times, flags);
		goto out;
	}

	dirfd = get_stream_dir_fd(handle->conn, smb_fname->base_name, NULL);
	if (dirfd < -1) {
		ret = -1;
		goto out;
	}

	ret = enc_vtimesat(dirfd, smb_fname_onefs->stream_name, ENC_DEFAULT,
			   times, flags);

	saved_errno = errno;
	close(dirfd);
	errno = saved_errno;

 out:
	END_PROFILE(syscall_ntimes);
	TALLOC_FREE(smb_fname_onefs);
	return ret;
}

/*
 * Streaminfo enumeration functionality
 */
struct streaminfo_state {
	TALLOC_CTX *mem_ctx;
	vfs_handle_struct *handle;
	unsigned int num_streams;
	struct stream_struct *streams;
	NTSTATUS status;
};

static bool add_one_stream(TALLOC_CTX *mem_ctx, unsigned int *num_streams,
			   struct stream_struct **streams,
			   const char *name, SMB_OFF_T size,
			   SMB_OFF_T alloc_size)
{
	struct stream_struct *tmp;

	tmp = TALLOC_REALLOC_ARRAY(mem_ctx, *streams, struct stream_struct,
				   (*num_streams)+1);
	if (tmp == NULL) {
		return false;
	}

	tmp[*num_streams].name = talloc_asprintf(mem_ctx, ":%s:%s", name,
						 "$DATA");
	if (tmp[*num_streams].name == NULL) {
		return false;
	}

	tmp[*num_streams].size = size;
	tmp[*num_streams].alloc_size = alloc_size;

	*streams = tmp;
	*num_streams += 1;
	return true;
}

static NTSTATUS walk_onefs_streams(connection_struct *conn, files_struct *fsp,
				   const char *fname,
				   struct streaminfo_state *state,
				   SMB_STRUCT_STAT *base_sbuf)
{
	NTSTATUS status = NT_STATUS_OK;
	bool opened_base_fd = false;
	int base_fd = -1;
	int dir_fd = -1;
	int stream_fd = -1;
	int ret;
	SMB_STRUCT_DIR *dirp = NULL;
	SMB_STRUCT_DIRENT *dp = NULL;
	files_struct fake_fs;
	struct fd_handle fake_fh;
	SMB_STRUCT_STAT stream_sbuf;

	ZERO_STRUCT(fake_fh);
	ZERO_STRUCT(fake_fs);

	/* If the base file is already open, use its fd. */
	if ((fsp != NULL) && (fsp->fh->fd != -1)) {
		base_fd = fsp->fh->fd;
	} else {
		opened_base_fd = true;
	}

	dir_fd = get_stream_dir_fd(conn, fname, &base_fd);
	if (dir_fd < 0) {
		return map_nt_error_from_unix(errno);
	}

	/* Open the ADS directory. */
	if ((dirp = fdopendir(dir_fd)) == NULL) {
		DEBUG(0, ("Error on opendir %s. errno=%d (%s)\n",
			  fname, errno, strerror(errno)));
		status = map_nt_error_from_unix(errno);
		goto out;
	}

	/* Initialize the dir state struct and add it to the list.
	 * This is a layer violation, and really should be handled by a
	 * VFS_FDOPENDIR() call which would properly setup the dir state.
	 * But since this is all within the onefs.so module, we cheat for
	 * now and call directly into the readdirplus code.
	 * NOTE: This state MUST be freed by a proper VFS_CLOSEDIR() call. */
	ret = onefs_rdp_add_dir_state(conn, dirp);
	if (ret) {
		DEBUG(0, ("Error adding dir_state to the list\n"));
		status = map_nt_error_from_unix(errno);
		goto out;
	}

	fake_fs.conn = conn;
	fake_fs.fh = &fake_fh;
	status = create_synthetic_smb_fname(talloc_tos(), fname, NULL, NULL,
					    &fake_fs.fsp_name);
	if (!NT_STATUS_IS_OK(status)) {
		goto out;
	}

	/* Iterate over the streams in the ADS directory. */
	while ((dp = SMB_VFS_READDIR(conn, dirp, NULL)) != NULL) {
		/* Skip the "." and ".." entries */
		if ((strcmp(dp->d_name, ".") == 0) ||
		    (strcmp(dp->d_name, "..") == 0))
			continue;

		/* Open actual stream */
		if ((stream_fd = onefs_sys_create_file(conn,
							 base_fd,
							 dp->d_name,
							 0,
							 0,
							 0,
							 0,
							 0,
							 0,
							 INTERNAL_OPEN_ONLY,
							 0,
							 NULL,
							 0,
							 NULL)) == -1) {
			DEBUG(0, ("Error opening stream %s:%s. "
				  "errno=%d (%s)\n", fname, dp->d_name, errno,
				  strerror(errno)));
			continue;
		}

		/* Figure out the stat info. */
		fake_fh.fd = stream_fd;
		ret = SMB_VFS_FSTAT(&fake_fs, &stream_sbuf);
		close(stream_fd);

		if (ret) {
			DEBUG(0, ("Error fstating stream %s:%s. "
				  "errno=%d (%s)\n", fname, dp->d_name, errno,
				  strerror(errno)));
			continue;
		}

		merge_stat(&stream_sbuf, base_sbuf);

		if (!add_one_stream(state->mem_ctx,
				    &state->num_streams, &state->streams,
				    dp->d_name, stream_sbuf.st_ex_size,
				    SMB_VFS_GET_ALLOC_SIZE(conn, NULL,
							   &stream_sbuf))) {
			state->status = NT_STATUS_NO_MEMORY;
			break;
		}
	}

out:
	/* Cleanup everything that was opened. */
	if (dirp != NULL) {
		SMB_VFS_CLOSEDIR(conn, dirp);
	}
	if (dir_fd >= 0) {
		close(dir_fd);
	}
	if (opened_base_fd) {
		SMB_ASSERT(base_fd >= 0);
		close(base_fd);
	}

	TALLOC_FREE(fake_fs.fsp_name);
	return status;
}

NTSTATUS onefs_streaminfo(vfs_handle_struct *handle,
			  struct files_struct *fsp,
			  const char *fname,
			  TALLOC_CTX *mem_ctx,
			  unsigned int *num_streams,
			  struct stream_struct **streams)
{
	SMB_STRUCT_STAT sbuf;
	int ret;
	NTSTATUS status;
	struct streaminfo_state state;

	/* Get a valid stat. */
	if ((fsp != NULL) && (fsp->fh->fd != -1)) {
		ret = SMB_VFS_FSTAT(fsp, &sbuf);
	} else {
		struct smb_filename *smb_fname = NULL;

		status = create_synthetic_smb_fname(talloc_tos(), fname, NULL,
						    NULL, &smb_fname);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
		ret = SMB_VFS_STAT(handle->conn, smb_fname);

		sbuf = smb_fname->st;

		TALLOC_FREE(smb_fname);
	}

	if (ret == -1) {
		return map_nt_error_from_unix(errno);
	}

	state.streams = *pstreams;
	state.num_streams = *pnum_streams;

	if (lp_parm_bool(SNUM(handle->conn), PARM_ONEFS_TYPE,
		PARM_IGNORE_STREAMS, PARM_IGNORE_STREAMS_DEFAULT)) {
		goto out;
	}

	state.mem_ctx = mem_ctx;
	state.handle = handle;
	state.status = NT_STATUS_OK;

	/* If there are more streams, add them too. */
	if (sbuf.st_ex_flags & UF_HASADS) {

		status = walk_onefs_streams(handle->conn, fsp, fname,
		    &state, &sbuf);

		if (!NT_STATUS_IS_OK(status)) {
			TALLOC_FREE(state.streams);
			return status;
		}

		if (!NT_STATUS_IS_OK(state.status)) {
			TALLOC_FREE(state.streams);
			return state.status;
		}
	}
 out:
	*num_streams = state.num_streams;
	*streams = state.streams;
	return SMB_VFS_NEXT_STREAMINFO(handle, fsp, fname, mem_ctx, pnum_streams, pstreams);
}
