/*
   Unix SMB/CIFS implementation.
   file closing
   Copyright (C) Andrew Tridgell 1992-1998
   Copyright (C) Jeremy Allison 1992-2007.
   Copyright (C) Volker Lendecke 2005

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
#include "system/filesys.h"
#include "printing.h"
#include "smbd/smbd.h"
#include "smbd/globals.h"
#include "fake_file.h"
#include "transfer_file.h"
#include "auth.h"
#include "messages.h"

/****************************************************************************
 Run a file if it is a magic script.
****************************************************************************/

static NTSTATUS check_magic(struct files_struct *fsp)
{
	int ret;
	const char *magic_output = NULL;
	SMB_STRUCT_STAT st;
	int tmp_fd, outfd;
	TALLOC_CTX *ctx = NULL;
	const char *p;
	struct connection_struct *conn = fsp->conn;
	char *fname = NULL;
	NTSTATUS status;

	if (!*lp_magicscript(SNUM(conn))) {
		return NT_STATUS_OK;
	}

	DEBUG(5,("checking magic for %s\n", fsp_str_dbg(fsp)));

	ctx = talloc_stackframe();

	fname = fsp->fsp_name->base_name;

	if (!(p = strrchr_m(fname,'/'))) {
		p = fname;
	} else {
		p++;
	}

	if (!strequal(lp_magicscript(SNUM(conn)),p)) {
		status = NT_STATUS_OK;
		goto out;
	}

	if (*lp_magicoutput(SNUM(conn))) {
		magic_output = lp_magicoutput(SNUM(conn));
	} else {
		magic_output = talloc_asprintf(ctx,
				"%s.out",
				fname);
	}
	if (!magic_output) {
		status = NT_STATUS_NO_MEMORY;
		goto out;
	}

	/* Ensure we don't depend on user's PATH. */
	p = talloc_asprintf(ctx, "./%s", fname);
	if (!p) {
		status = NT_STATUS_NO_MEMORY;
		goto out;
	}

	if (chmod(fname, 0755) == -1) {
		status = map_nt_error_from_unix(errno);
		goto out;
	}
	ret = smbrun(p,&tmp_fd);
	DEBUG(3,("Invoking magic command %s gave %d\n",
		p,ret));

	unlink(fname);
	if (ret != 0 || tmp_fd == -1) {
		if (tmp_fd != -1) {
			close(tmp_fd);
		}
		status = NT_STATUS_UNSUCCESSFUL;
		goto out;
	}
	outfd = open(magic_output, O_CREAT|O_EXCL|O_RDWR, 0600);
	if (outfd == -1) {
		int err = errno;
		close(tmp_fd);
		status = map_nt_error_from_unix(err);
		goto out;
	}

	if (sys_fstat(tmp_fd, &st, false) == -1) {
		int err = errno;
		close(tmp_fd);
		close(outfd);
		status = map_nt_error_from_unix(err);
		goto out;
	}

	if (transfer_file(tmp_fd,outfd,(SMB_OFF_T)st.st_ex_size) == (SMB_OFF_T)-1) {
		int err = errno;
		close(tmp_fd);
		close(outfd);
		status = map_nt_error_from_unix(err);
		goto out;
	}
	close(tmp_fd);
	if (close(outfd) == -1) {
		status = map_nt_error_from_unix(errno);
		goto out;
	}

	status = NT_STATUS_OK;

 out:
	TALLOC_FREE(ctx);
	return status;
}

/****************************************************************************
  Common code to close a file or a directory.
****************************************************************************/

static NTSTATUS close_filestruct(files_struct *fsp)
{
	NTSTATUS status = NT_STATUS_OK;

	if (fsp->fh->fd != -1) {
		if(flush_write_cache(fsp, CLOSE_FLUSH) == -1) {
			status = map_nt_error_from_unix(errno);
		}
		delete_write_cache(fsp);
	}

	return status;
}

/****************************************************************************
 If any deferred opens are waiting on this close, notify them.
****************************************************************************/

static void notify_deferred_opens(struct messaging_context *msg_ctx,
				  struct share_mode_lock *lck)
{
 	int i;

	if (!should_notify_deferred_opens()) {
		return;
	}
 
 	for (i=0; i<lck->num_share_modes; i++) {
 		struct share_mode_entry *e = &lck->share_modes[i];
 
 		if (!is_deferred_open_entry(e)) {
 			continue;
 		}
 
 		if (procid_is_me(&e->pid)) {
 			/*
 			 * We need to notify ourself to retry the open.  Do
 			 * this by finding the queued SMB record, moving it to
 			 * the head of the queue and changing the wait time to
 			 * zero.
 			 */
 			schedule_deferred_open_message_smb(e->op_mid);
 		} else {
			char msg[MSG_SMB_SHARE_MODE_ENTRY_SIZE];

			share_mode_entry_to_message(msg, e);

			messaging_send_buf(msg_ctx, e->pid, MSG_SMB_OPEN_RETRY,
					   (uint8 *)msg,
					   MSG_SMB_SHARE_MODE_ENTRY_SIZE);
 		}
 	}
}

/****************************************************************************
 Delete all streams
****************************************************************************/

NTSTATUS delete_all_streams(connection_struct *conn, const char *fname)
{
	struct stream_struct *stream_info = NULL;
	int i;
	unsigned int num_streams = 0;
	TALLOC_CTX *frame = talloc_stackframe();
	NTSTATUS status;

	status = vfs_streaminfo(conn, NULL, fname, talloc_tos(),
				&num_streams, &stream_info);

	if (NT_STATUS_EQUAL(status, NT_STATUS_NOT_IMPLEMENTED)) {
		DEBUG(10, ("no streams around\n"));
		TALLOC_FREE(frame);
		return NT_STATUS_OK;
	}

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("vfs_streaminfo failed: %s\n",
			   nt_errstr(status)));
		goto fail;
	}

	DEBUG(10, ("delete_all_streams found %d streams\n",
		   num_streams));

	if (num_streams == 0) {
		TALLOC_FREE(frame);
		return NT_STATUS_OK;
	}

	for (i=0; i<num_streams; i++) {
		int res;
		struct smb_filename *smb_fname_stream = NULL;

		if (strequal(stream_info[i].name, "::$DATA")) {
			continue;
		}

		status = create_synthetic_smb_fname(talloc_tos(), fname,
						    stream_info[i].name, NULL,
						    &smb_fname_stream);

		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0, ("talloc_aprintf failed\n"));
			goto fail;
		}

		res = SMB_VFS_UNLINK(conn, smb_fname_stream);

		if (res == -1) {
			status = map_nt_error_from_unix(errno);
			DEBUG(10, ("Could not delete stream %s: %s\n",
				   smb_fname_str_dbg(smb_fname_stream),
				   strerror(errno)));
			TALLOC_FREE(smb_fname_stream);
			break;
		}
		TALLOC_FREE(smb_fname_stream);
	}

 fail:
	TALLOC_FREE(frame);
	return status;
}

/****************************************************************************
 Deal with removing a share mode on last close.
****************************************************************************/

static NTSTATUS close_remove_share_mode(files_struct *fsp,
					enum file_close_type close_type)
{
	connection_struct *conn = fsp->conn;
	bool delete_file = false;
	bool changed_user = false;
	struct share_mode_lock *lck = NULL;
	NTSTATUS status = NT_STATUS_OK;
	NTSTATUS tmp_status;
	struct file_id id;
	const struct security_unix_token *del_token = NULL;
	const struct security_token *del_nt_token = NULL;
	bool got_tokens = false;

	/* Ensure any pending write time updates are done. */
	if (fsp->update_write_time_event) {
		update_write_time_handler(smbd_event_context(),
					fsp->update_write_time_event,
					timeval_current(),
					(void *)fsp);
	}

	/*
	 * Lock the share entries, and determine if we should delete
	 * on close. If so delete whilst the lock is still in effect.
	 * This prevents race conditions with the file being created. JRA.
	 */

	lck = get_share_mode_lock(talloc_tos(), fsp->file_id, NULL, NULL,
				  NULL);

	if (lck == NULL) {
		DEBUG(0, ("close_remove_share_mode: Could not get share mode "
			  "lock for file %s\n", fsp_str_dbg(fsp)));
		status = NT_STATUS_INVALID_PARAMETER;
		goto done;
	}

	if (fsp->write_time_forced) {
		DEBUG(10,("close_remove_share_mode: write time forced "
			"for file %s\n",
			fsp_str_dbg(fsp)));
		set_close_write_time(fsp, lck->changed_write_time);
	} else if (fsp->update_write_time_on_close) {
		/* Someone had a pending write. */
		if (null_timespec(fsp->close_write_time)) {
			DEBUG(10,("close_remove_share_mode: update to current time "
				"for file %s\n",
				fsp_str_dbg(fsp)));
			/* Update to current time due to "normal" write. */
			set_close_write_time(fsp, timespec_current());
		} else {
			DEBUG(10,("close_remove_share_mode: write time pending "
				"for file %s\n",
				fsp_str_dbg(fsp)));
			/* Update to time set on close call. */
			set_close_write_time(fsp, fsp->close_write_time);
		}
	}

	if (!del_share_mode(lck, fsp)) {
		DEBUG(0, ("close_remove_share_mode: Could not delete share "
			  "entry for file %s\n",
			  fsp_str_dbg(fsp)));
	}

	if (fsp->initial_delete_on_close &&
			!is_delete_on_close_set(lck, fsp->name_hash)) {
		bool became_user = False;

		/* Initial delete on close was set and no one else
		 * wrote a real delete on close. */

		if (get_current_vuid(conn) != fsp->vuid) {
			become_user(conn, fsp->vuid);
			became_user = True;
		}
		fsp->delete_on_close = true;
		set_delete_on_close_lck(fsp, lck, True,
				get_current_nttok(conn),
				get_current_utok(conn));
		if (became_user) {
			unbecome_user();
		}
	}

	delete_file = is_delete_on_close_set(lck, fsp->name_hash);

	if (delete_file) {
		int i;
		/* See if others still have the file open via this pathname.
		   If this is the case, then don't delete. If all opens are
		   POSIX delete now. */
		for (i=0; i<lck->num_share_modes; i++) {
			struct share_mode_entry *e = &lck->share_modes[i];
			if (is_valid_share_mode_entry(e) &&
					e->name_hash == fsp->name_hash) {
				if (fsp->posix_open && (e->flags & SHARE_MODE_FLAG_POSIX_OPEN)) {
					continue;
				}
				delete_file = False;
				break;
			}
		}
	}

	/* Notify any deferred opens waiting on this close. */
	notify_deferred_opens(conn->sconn->msg_ctx, lck);
	reply_to_oplock_break_requests(fsp);

	/*
	 * NT can set delete_on_close of the last open
	 * reference to a file.
	 */

	if (!(close_type == NORMAL_CLOSE || close_type == SHUTDOWN_CLOSE) ||
			!delete_file) {
		TALLOC_FREE(lck);
		return NT_STATUS_OK;
	}

	/*
	 * Ok, we have to delete the file
	 */

	DEBUG(5,("close_remove_share_mode: file %s. Delete on close was set "
		 "- deleting file.\n", fsp_str_dbg(fsp)));

	/*
	 * Don't try to update the write time when we delete the file
	 */
	fsp->update_write_time_on_close = false;

	got_tokens = get_delete_on_close_token(lck, fsp->name_hash,
					&del_nt_token, &del_token);
	SMB_ASSERT(got_tokens);

	if (!unix_token_equal(del_token, get_current_utok(conn))) {
		/* Become the user who requested the delete. */

		DEBUG(5,("close_remove_share_mode: file %s. "
			"Change user to uid %u\n",
			fsp_str_dbg(fsp),
			(unsigned int)del_token->uid));

		if (!push_sec_ctx()) {
			smb_panic("close_remove_share_mode: file %s. failed to push "
				  "sec_ctx.\n");
		}

		set_sec_ctx(del_token->uid,
			    del_token->gid,
			    del_token->ngroups,
			    del_token->groups,
			    del_nt_token);

		changed_user = true;
	}

	/* We can only delete the file if the name we have is still valid and
	   hasn't been renamed. */

	tmp_status = vfs_stat_fsp(fsp);
	if (!NT_STATUS_IS_OK(tmp_status)) {
		DEBUG(5,("close_remove_share_mode: file %s. Delete on close "
			 "was set and stat failed with error %s\n",
			 fsp_str_dbg(fsp), nt_errstr(tmp_status)));
		/*
		 * Don't save the errno here, we ignore this error
		 */
		goto done;
	}

	id = vfs_file_id_from_sbuf(conn, &fsp->fsp_name->st);

	if (!file_id_equal(&fsp->file_id, &id)) {
		DEBUG(5,("close_remove_share_mode: file %s. Delete on close "
			 "was set and dev and/or inode does not match\n",
			 fsp_str_dbg(fsp)));
		DEBUG(5,("close_remove_share_mode: file %s. stored file_id %s, "
			 "stat file_id %s\n",
			 fsp_str_dbg(fsp),
			 file_id_string_tos(&fsp->file_id),
			 file_id_string_tos(&id)));
		/*
		 * Don't save the errno here, we ignore this error
		 */
		goto done;
	}

	if ((conn->fs_capabilities & FILE_NAMED_STREAMS)
	    && !is_ntfs_stream_smb_fname(fsp->fsp_name)) {

		status = delete_all_streams(conn, fsp->fsp_name->base_name);

		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(5, ("delete_all_streams failed: %s\n",
				  nt_errstr(status)));
			goto done;
		}
	}


	if (SMB_VFS_UNLINK(conn, fsp->fsp_name) != 0) {
		/*
		 * This call can potentially fail as another smbd may
		 * have had the file open with delete on close set and
		 * deleted it when its last reference to this file
		 * went away. Hence we log this but not at debug level
		 * zero.
		 */

		DEBUG(5,("close_remove_share_mode: file %s. Delete on close "
			 "was set and unlink failed with error %s\n",
			 fsp_str_dbg(fsp), strerror(errno)));

		status = map_nt_error_from_unix(errno);
	}

	/* As we now have POSIX opens which can unlink
 	 * with other open files we may have taken
 	 * this code path with more than one share mode
 	 * entry - ensure we only delete once by resetting
 	 * the delete on close flag. JRA.
 	 */

	fsp->delete_on_close = false;
	set_delete_on_close_lck(fsp, lck, false, NULL, NULL);

 done:

	if (changed_user) {
		/* unbecome user. */
		pop_sec_ctx();
	}

	TALLOC_FREE(lck);

	if (delete_file) {
		/*
		 * Do the notification after we released the share
		 * mode lock. Inside notify_fname we take out another
		 * tdb lock. With ctdb also accessing our databases,
		 * this can lead to deadlocks. Putting this notify
		 * after the TALLOC_FREE(lck) above we avoid locking
		 * two records simultaneously. Notifies are async and
		 * informational only, so calling the notify_fname
		 * without holding the share mode lock should not do
		 * any harm.
		 */
		notify_fname(conn, NOTIFY_ACTION_REMOVED,
			     FILE_NOTIFY_CHANGE_FILE_NAME,
			     fsp->fsp_name->base_name);
	}

	return status;
}

void set_close_write_time(struct files_struct *fsp, struct timespec ts)
{
	DEBUG(6,("close_write_time: %s" , time_to_asc(convert_timespec_to_time_t(ts))));

	if (null_timespec(ts)) {
		return;
	}
	fsp->write_time_forced = false;
	fsp->update_write_time_on_close = true;
	fsp->close_write_time = ts;
}

static NTSTATUS update_write_time_on_close(struct files_struct *fsp)
{
	struct smb_file_time ft;
	NTSTATUS status;
	struct share_mode_lock *lck = NULL;

	ZERO_STRUCT(ft);

	if (!fsp->update_write_time_on_close) {
		return NT_STATUS_OK;
	}

	if (null_timespec(fsp->close_write_time)) {
		fsp->close_write_time = timespec_current();
	}

	/* Ensure we have a valid stat struct for the source. */
	status = vfs_stat_fsp(fsp);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (!VALID_STAT(fsp->fsp_name->st)) {
		/* if it doesn't seem to be a real file */
		return NT_STATUS_OK;
	}

	/* On close if we're changing the real file time we
	 * must update it in the open file db too. */
	(void)set_write_time(fsp->file_id, fsp->close_write_time);

	lck = get_share_mode_lock(talloc_tos(), fsp->file_id, NULL, NULL, NULL);
	if (lck) {
		/* Close write times overwrite sticky write times
		   so we must replace any sticky write time here. */
		if (!null_timespec(lck->changed_write_time)) {
			(void)set_sticky_write_time(fsp->file_id, fsp->close_write_time);
		}
		TALLOC_FREE(lck);
	}

	ft.mtime = fsp->close_write_time;
	/* We must use NULL for the fsp handle here, as smb_set_file_time()
	   checks the fsp access_mask, which may not include FILE_WRITE_ATTRIBUTES.
	   As this is a close based update, we are not directly changing the
	   file attributes from a client call, but indirectly from a write. */
	status = smb_set_file_time(fsp->conn, NULL, fsp->fsp_name, &ft, false);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10,("update_write_time_on_close: smb_set_file_time "
			"on file %s returned %s\n",
			fsp_str_dbg(fsp),
			nt_errstr(status)));
		return status;
	}

	return status;
}

static NTSTATUS ntstatus_keeperror(NTSTATUS s1, NTSTATUS s2)
{
	if (!NT_STATUS_IS_OK(s1)) {
		return s1;
	}
	return s2;
}

/****************************************************************************
 Close a file.

 close_type can be NORMAL_CLOSE=0,SHUTDOWN_CLOSE,ERROR_CLOSE.
 printing and magic scripts are only run on normal close.
 delete on close is done on normal and shutdown close.
****************************************************************************/

static NTSTATUS close_normal_file(struct smb_request *req, files_struct *fsp,
				  enum file_close_type close_type)
{
	NTSTATUS status = NT_STATUS_OK;
	NTSTATUS tmp;
	connection_struct *conn = fsp->conn;

	if (close_type == ERROR_CLOSE) {
		cancel_aio_by_fsp(fsp);
	} else {
		/*
	 	 * If we're finishing async io on a close we can get a write
		 * error here, we must remember this.
		 */
		int ret = wait_for_aio_completion(fsp);
		if (ret) {
			status = ntstatus_keeperror(
				status, map_nt_error_from_unix(ret));
		}
	}

	/*
	 * If we're flushing on a close we can get a write
	 * error here, we must remember this.
	 */

	tmp = close_filestruct(fsp);
	status = ntstatus_keeperror(status, tmp);

	if (fsp->print_file) {
#ifndef PRINTER_SUPPORT
		return NT_STATUS_OK;
#endif
		/* FIXME: return spool errors */
		print_spool_end(fsp, close_type);
		file_free(req, fsp);
		return NT_STATUS_OK;
	}

	/* Remove the oplock before potentially deleting the file. */
	if(fsp->oplock_type) {
		release_file_oplock(fsp);
	}

	/* If this is an old DOS or FCB open and we have multiple opens on
	   the same handle we only have one share mode. Ensure we only remove
	   the share mode on the last close. */

	if (fsp->fh->ref_count == 1) {
		/* Should we return on error here... ? */
		tmp = close_remove_share_mode(fsp, close_type);
		status = ntstatus_keeperror(status, tmp);
	}

	locking_close_file(conn->sconn->msg_ctx, fsp, close_type);

	tmp = fd_close(fsp);
	status = ntstatus_keeperror(status, tmp);

	/* check for magic scripts */
	if (close_type == NORMAL_CLOSE) {
		tmp = check_magic(fsp);
		status = ntstatus_keeperror(status, tmp);
	}

	/*
	 * Ensure pending modtime is set after close.
	 */

	tmp = update_write_time_on_close(fsp);
	if (NT_STATUS_EQUAL(tmp, NT_STATUS_OBJECT_NAME_NOT_FOUND)) {
		/* Someone renamed the file or a parent directory containing
		 * this file. We can't do anything about this, we don't have
		 * an "update timestamp by fd" call in POSIX. Eat the error. */

		tmp = NT_STATUS_OK;
	}

	status = ntstatus_keeperror(status, tmp);

	DEBUG(2,("%s closed file %s (numopen=%d) %s\n",
		conn->session_info->unix_name, fsp_str_dbg(fsp),
		conn->num_files_open - 1,
		nt_errstr(status) ));

	file_free(req, fsp);
	return status;
}
/****************************************************************************
 Static function used by reply_rmdir to delete an entire directory
 tree recursively. Return True on ok, False on fail.
****************************************************************************/

static bool recursive_rmdir(TALLOC_CTX *ctx,
			connection_struct *conn,
			struct smb_filename *smb_dname)
{
	const char *dname = NULL;
	char *talloced = NULL;
	bool ret = True;
	long offset = 0;
	SMB_STRUCT_STAT st;
	struct smb_Dir *dir_hnd;

	SMB_ASSERT(!is_ntfs_stream_smb_fname(smb_dname));

	dir_hnd = OpenDir(talloc_tos(), conn, smb_dname->base_name, NULL, 0);
	if(dir_hnd == NULL)
		return False;

	while((dname = ReadDirName(dir_hnd, &offset, &st, &talloced))) {
		struct smb_filename *smb_dname_full = NULL;
		char *fullname = NULL;
		bool do_break = true;
		NTSTATUS status;

		if (ISDOT(dname) || ISDOTDOT(dname)) {
			TALLOC_FREE(talloced);
			continue;
		}

		if (!is_visible_file(conn, smb_dname->base_name, dname, &st,
				     false)) {
			TALLOC_FREE(talloced);
			continue;
		}

		/* Construct the full name. */
		fullname = talloc_asprintf(ctx,
				"%s/%s",
				smb_dname->base_name,
				dname);
		if (!fullname) {
			errno = ENOMEM;
			goto err_break;
		}

		status = create_synthetic_smb_fname(talloc_tos(), fullname,
						    NULL, NULL,
						    &smb_dname_full);
		if (!NT_STATUS_IS_OK(status)) {
			goto err_break;
		}

		if(SMB_VFS_LSTAT(conn, smb_dname_full) != 0) {
			goto err_break;
		}

		if(smb_dname_full->st.st_ex_mode & S_IFDIR) {
			if(!recursive_rmdir(ctx, conn, smb_dname_full)) {
				goto err_break;
			}
			if(SMB_VFS_RMDIR(conn,
					 smb_dname_full->base_name) != 0) {
				goto err_break;
			}
		} else if(SMB_VFS_UNLINK(conn, smb_dname_full) != 0) {
			goto err_break;
		}

		/* Successful iteration. */
		do_break = false;

	 err_break:
		TALLOC_FREE(smb_dname_full);
		TALLOC_FREE(fullname);
		TALLOC_FREE(talloced);
		if (do_break) {
			ret = false;
			break;
		}
	}
	TALLOC_FREE(dir_hnd);
	return ret;
}

/****************************************************************************
 The internals of the rmdir code - called elsewhere.
****************************************************************************/

static NTSTATUS rmdir_internals(TALLOC_CTX *ctx, files_struct *fsp)
{
	connection_struct *conn = fsp->conn;
	struct smb_filename *smb_dname = fsp->fsp_name;
	int ret;

	SMB_ASSERT(!is_ntfs_stream_smb_fname(smb_dname));

	/* Might be a symlink. */
	if(SMB_VFS_LSTAT(conn, smb_dname) != 0) {
		return map_nt_error_from_unix(errno);
	}

	if (S_ISLNK(smb_dname->st.st_ex_mode)) {
		/* Is what it points to a directory ? */
		if(SMB_VFS_STAT(conn, smb_dname) != 0) {
			return map_nt_error_from_unix(errno);
		}
		if (!(S_ISDIR(smb_dname->st.st_ex_mode))) {
			return NT_STATUS_NOT_A_DIRECTORY;
		}
		ret = SMB_VFS_UNLINK(conn, smb_dname);
	} else {
		ret = SMB_VFS_RMDIR(conn, smb_dname->base_name);
	}
	if (ret == 0) {
		notify_fname(conn, NOTIFY_ACTION_REMOVED,
			     FILE_NOTIFY_CHANGE_DIR_NAME,
			     smb_dname->base_name);
		return NT_STATUS_OK;
	}

	if(((errno == ENOTEMPTY)||(errno == EEXIST)) && *lp_veto_files(SNUM(conn))) {
		/*
		 * Check to see if the only thing in this directory are
		 * vetoed files/directories. If so then delete them and
		 * retry. If we fail to delete any of them (and we *don't*
		 * do a recursive delete) then fail the rmdir.
		 */
		SMB_STRUCT_STAT st;
		const char *dname = NULL;
		char *talloced = NULL;
		long dirpos = 0;
		struct smb_Dir *dir_hnd = OpenDir(talloc_tos(), conn,
						  smb_dname->base_name, NULL,
						  0);

		if(dir_hnd == NULL) {
			errno = ENOTEMPTY;
			goto err;
		}

		while ((dname = ReadDirName(dir_hnd, &dirpos, &st,
					    &talloced)) != NULL) {
			if((strcmp(dname, ".") == 0) || (strcmp(dname, "..")==0)) {
				TALLOC_FREE(talloced);
				continue;
			}
			if (!is_visible_file(conn, smb_dname->base_name, dname,
					     &st, false)) {
				TALLOC_FREE(talloced);
				continue;
			}
			if(!IS_VETO_PATH(conn, dname)) {
				TALLOC_FREE(dir_hnd);
				TALLOC_FREE(talloced);
				errno = ENOTEMPTY;
				goto err;
			}
			TALLOC_FREE(talloced);
		}

		/* We only have veto files/directories.
		 * Are we allowed to delete them ? */

		if(!lp_recursive_veto_delete(SNUM(conn))) {
			TALLOC_FREE(dir_hnd);
			errno = ENOTEMPTY;
			goto err;
		}

		/* Do a recursive delete. */
		RewindDir(dir_hnd,&dirpos);
		while ((dname = ReadDirName(dir_hnd, &dirpos, &st,
					    &talloced)) != NULL) {
			struct smb_filename *smb_dname_full = NULL;
			char *fullname = NULL;
			bool do_break = true;
			NTSTATUS status;

			if (ISDOT(dname) || ISDOTDOT(dname)) {
				TALLOC_FREE(talloced);
				continue;
			}
			if (!is_visible_file(conn, smb_dname->base_name, dname,
					     &st, false)) {
				TALLOC_FREE(talloced);
				continue;
			}

			fullname = talloc_asprintf(ctx,
					"%s/%s",
					smb_dname->base_name,
					dname);

			if(!fullname) {
				errno = ENOMEM;
				goto err_break;
			}

			status = create_synthetic_smb_fname(talloc_tos(),
							    fullname, NULL,
							    NULL,
							    &smb_dname_full);
			if (!NT_STATUS_IS_OK(status)) {
				errno = map_errno_from_nt_status(status);
				goto err_break;
			}

			if(SMB_VFS_LSTAT(conn, smb_dname_full) != 0) {
				goto err_break;
			}
			if(smb_dname_full->st.st_ex_mode & S_IFDIR) {
				if(!recursive_rmdir(ctx, conn,
						    smb_dname_full)) {
					goto err_break;
				}
				if(SMB_VFS_RMDIR(conn,
					smb_dname_full->base_name) != 0) {
					goto err_break;
				}
			} else if(SMB_VFS_UNLINK(conn, smb_dname_full) != 0) {
				goto err_break;
			}

			/* Successful iteration. */
			do_break = false;

		 err_break:
			TALLOC_FREE(fullname);
			TALLOC_FREE(smb_dname_full);
			TALLOC_FREE(talloced);
			if (do_break)
				break;
		}
		TALLOC_FREE(dir_hnd);
		/* Retry the rmdir */
		ret = SMB_VFS_RMDIR(conn, smb_dname->base_name);
	}

  err:

	if (ret != 0) {
		DEBUG(3,("rmdir_internals: couldn't remove directory %s : "
			 "%s\n", smb_fname_str_dbg(smb_dname),
			 strerror(errno)));
		return map_nt_error_from_unix(errno);
	}

	notify_fname(conn, NOTIFY_ACTION_REMOVED,
		     FILE_NOTIFY_CHANGE_DIR_NAME,
		     smb_dname->base_name);

	return NT_STATUS_OK;
}

/****************************************************************************
 Close a directory opened by an NT SMB call. 
****************************************************************************/
  
static NTSTATUS close_directory(struct smb_request *req, files_struct *fsp,
				enum file_close_type close_type)
{
	struct share_mode_lock *lck = NULL;
	bool delete_dir = False;
	NTSTATUS status = NT_STATUS_OK;
	NTSTATUS status1 = NT_STATUS_OK;
	const struct security_token *del_nt_token = NULL;
	const struct security_unix_token *del_token = NULL;

	/*
	 * NT can set delete_on_close of the last open
	 * reference to a directory also.
	 */

	lck = get_share_mode_lock(talloc_tos(), fsp->file_id, NULL, NULL,
				  NULL);

	if (lck == NULL) {
		DEBUG(0, ("close_directory: Could not get share mode lock for "
			  "%s\n", fsp_str_dbg(fsp)));
		status = NT_STATUS_INVALID_PARAMETER;
		goto out;
	}

	if (!del_share_mode(lck, fsp)) {
		DEBUG(0, ("close_directory: Could not delete share entry for "
			  "%s\n", fsp_str_dbg(fsp)));
	}

	if (fsp->initial_delete_on_close) {
		bool became_user = False;

		/* Initial delete on close was set - for
		 * directories we don't care if anyone else
		 * wrote a real delete on close. */

		if (get_current_vuid(fsp->conn) != fsp->vuid) {
			become_user(fsp->conn, fsp->vuid);
			became_user = True;
		}
		send_stat_cache_delete_message(fsp->conn->sconn->msg_ctx,
					       fsp->fsp_name->base_name);
		set_delete_on_close_lck(fsp, lck, true,
				get_current_nttok(fsp->conn),
				get_current_utok(fsp->conn));
		fsp->delete_on_close = true;
		if (became_user) {
			unbecome_user();
		}
	}

	delete_dir = get_delete_on_close_token(lck, fsp->name_hash,
					&del_nt_token, &del_token);

	if (delete_dir) {
		int i;
		/* See if others still have the dir open. If this is the
		 * case, then don't delete. If all opens are POSIX delete now. */
		for (i=0; i<lck->num_share_modes; i++) {
			struct share_mode_entry *e = &lck->share_modes[i];
			if (is_valid_share_mode_entry(e) &&
					e->name_hash == fsp->name_hash) {
				if (fsp->posix_open && (e->flags & SHARE_MODE_FLAG_POSIX_OPEN)) {
					continue;
				}
				delete_dir = False;
				break;
			}
		}
	}

	if ((close_type == NORMAL_CLOSE || close_type == SHUTDOWN_CLOSE) &&
				delete_dir) {
	
		/* Become the user who requested the delete. */

		if (!push_sec_ctx()) {
			smb_panic("close_directory: failed to push sec_ctx.\n");
		}

		set_sec_ctx(del_token->uid,
				del_token->gid,
				del_token->ngroups,
				del_token->groups,
				del_nt_token);

		TALLOC_FREE(lck);

		if ((fsp->conn->fs_capabilities & FILE_NAMED_STREAMS)
		    && !is_ntfs_stream_smb_fname(fsp->fsp_name)) {

			status = delete_all_streams(fsp->conn, fsp->fsp_name->base_name);
			if (!NT_STATUS_IS_OK(status)) {
				DEBUG(5, ("delete_all_streams failed: %s\n",
					  nt_errstr(status)));
				goto out;
			}
		}

		status = rmdir_internals(talloc_tos(), fsp);

		DEBUG(5,("close_directory: %s. Delete on close was set - "
			 "deleting directory returned %s.\n",
			 fsp_str_dbg(fsp), nt_errstr(status)));

		/* unbecome user. */
		pop_sec_ctx();

		/*
		 * Ensure we remove any change notify requests that would
		 * now fail as the directory has been deleted.
		 */

		if(NT_STATUS_IS_OK(status)) {
			remove_pending_change_notify_requests_by_fid(fsp, NT_STATUS_DELETE_PENDING);
		}
	} else {
		TALLOC_FREE(lck);
		remove_pending_change_notify_requests_by_fid(
			fsp, NT_STATUS_OK);
	}

	status1 = fd_close(fsp);

	if (!NT_STATUS_IS_OK(status1)) {
		DEBUG(0, ("Could not close dir! fname=%s, fd=%d, err=%d=%s\n",
			  fsp_str_dbg(fsp), fsp->fh->fd, errno,
			  strerror(errno)));
	}

	/*
	 * Do the code common to files and directories.
	 */
	close_filestruct(fsp);
	file_free(req, fsp);

 out:
	TALLOC_FREE(lck);
	if (NT_STATUS_IS_OK(status) && !NT_STATUS_IS_OK(status1)) {
		status = status1;
	}
	return status;
}

/****************************************************************************
 Close a files_struct.
****************************************************************************/
  
NTSTATUS close_file(struct smb_request *req, files_struct *fsp,
		    enum file_close_type close_type)
{
	NTSTATUS status;
	struct files_struct *base_fsp = fsp->base_fsp;

	if(fsp->is_directory) {
		status = close_directory(req, fsp, close_type);
	} else if (fsp->fake_file_handle != NULL) {
		status = close_fake_file(req, fsp);
	} else {
		status = close_normal_file(req, fsp, close_type);
	}

	if ((base_fsp != NULL) && (close_type != SHUTDOWN_CLOSE)) {

		/*
		 * fsp was a stream, the base fsp can't be a stream as well
		 *
		 * For SHUTDOWN_CLOSE this is not possible here, because
		 * SHUTDOWN_CLOSE only happens from files.c which walks the
		 * complete list of files. If we mess with more than one fsp
		 * those loops will become confused.
		 */

		SMB_ASSERT(base_fsp->base_fsp == NULL);
		close_file(req, base_fsp, close_type);
	}

	return status;
}

/****************************************************************************
 Deal with an (authorized) message to close a file given the share mode
 entry.
****************************************************************************/

void msg_close_file(struct messaging_context *msg_ctx,
			void *private_data,
			uint32_t msg_type,
			struct server_id server_id,
			DATA_BLOB *data)
{
	struct smbd_server_connection *sconn;
	files_struct *fsp = NULL;
	struct share_mode_entry e;

	sconn = msg_ctx_to_sconn(msg_ctx);
	if (sconn == NULL) {
		DEBUG(1, ("could not find sconn\n"));
		return;
	}

	message_to_share_mode_entry(&e, (char *)data->data);

	if(DEBUGLVL(10)) {
		char *sm_str = share_mode_str(NULL, 0, &e);
		if (!sm_str) {
			smb_panic("talloc failed");
		}
		DEBUG(10,("msg_close_file: got request to close share mode "
			"entry %s\n", sm_str));
		TALLOC_FREE(sm_str);
	}

	fsp = file_find_dif(sconn, e.id, e.share_file_id);
	if (!fsp) {
		DEBUG(10,("msg_close_file: failed to find file.\n"));
		return;
	}
	close_file(NULL, fsp, NORMAL_CLOSE);
}
