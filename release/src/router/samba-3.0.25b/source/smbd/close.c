/* 
   Unix SMB/CIFS implementation.
   file closing
   Copyright (C) Andrew Tridgell 1992-1998
   Copyright (C) Jeremy Allison 1992-2007.
   Copyright (C) Volker Lendecke 2005
   
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

extern struct current_user current_user;

/****************************************************************************
 Run a file if it is a magic script.
****************************************************************************/

static void check_magic(files_struct *fsp,connection_struct *conn)
{
	if (!*lp_magicscript(SNUM(conn)))
		return;

	DEBUG(5,("checking magic for %s\n",fsp->fsp_name));

	{
		char *p;
		if (!(p = strrchr_m(fsp->fsp_name,'/')))
			p = fsp->fsp_name;
		else
			p++;

		if (!strequal(lp_magicscript(SNUM(conn)),p))
			return;
	}

	{
		int ret;
		pstring magic_output;
		pstring fname;
		SMB_STRUCT_STAT st;
		int tmp_fd, outfd;

		pstrcpy(fname,fsp->fsp_name);
		if (*lp_magicoutput(SNUM(conn)))
			pstrcpy(magic_output,lp_magicoutput(SNUM(conn)));
		else
			slprintf(magic_output,sizeof(fname)-1, "%s.out",fname);

		chmod(fname,0755);
		ret = smbrun(fname,&tmp_fd);
		DEBUG(3,("Invoking magic command %s gave %d\n",fname,ret));
		unlink(fname);
		if (ret != 0 || tmp_fd == -1) {
			if (tmp_fd != -1)
				close(tmp_fd);
			return;
		}
		outfd = open(magic_output, O_CREAT|O_EXCL|O_RDWR, 0600);
		if (outfd == -1) {
			close(tmp_fd);
			return;
		}

		if (sys_fstat(tmp_fd,&st) == -1) {
			close(tmp_fd);
			close(outfd);
			return;
		}

		transfer_file(tmp_fd,outfd,(SMB_OFF_T)st.st_size);
		close(tmp_fd);
		close(outfd);
	}
}

/****************************************************************************
  Common code to close a file or a directory.
****************************************************************************/

static NTSTATUS close_filestruct(files_struct *fsp)
{   
	NTSTATUS status = NT_STATUS_OK;
	connection_struct *conn = fsp->conn;
    
	if (fsp->fh->fd != -1) {
		if(flush_write_cache(fsp, CLOSE_FLUSH) == -1) {
			status = map_nt_error_from_unix(errno);
		}
		delete_write_cache(fsp);
	}

	conn->num_files_open--;
	SAFE_FREE(fsp->wbmpx_ptr);
	return status;
}    

/****************************************************************************
 If any deferred opens are waiting on this close, notify them.
****************************************************************************/

static void notify_deferred_opens(struct share_mode_lock *lck)
{
 	int i;
 
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
 			schedule_deferred_open_smb_message(e->op_mid);
 		} else {
			char msg[MSG_SMB_SHARE_MODE_ENTRY_SIZE];

			share_mode_entry_to_message(msg, e);

 			message_send_pid(e->pid, MSG_SMB_OPEN_RETRY,
 					 msg, MSG_SMB_SHARE_MODE_ENTRY_SIZE, True);
 		}
 	}
}

/****************************************************************************
 Deal with removing a share mode on last close.
****************************************************************************/

static NTSTATUS close_remove_share_mode(files_struct *fsp,
					enum file_close_type close_type)
{
	connection_struct *conn = fsp->conn;
	BOOL delete_file = False;
	struct share_mode_lock *lck;
	SMB_STRUCT_STAT sbuf;
	NTSTATUS status = NT_STATUS_OK;

	/*
	 * Lock the share entries, and determine if we should delete
	 * on close. If so delete whilst the lock is still in effect.
	 * This prevents race conditions with the file being created. JRA.
	 */

	lck = get_share_mode_lock(NULL, fsp->dev, fsp->inode, NULL, NULL);

	if (lck == NULL) {
		DEBUG(0, ("close_remove_share_mode: Could not get share mode "
			  "lock for file %s\n", fsp->fsp_name));
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (!del_share_mode(lck, fsp)) {
		DEBUG(0, ("close_remove_share_mode: Could not delete share "
			  "entry for file %s\n", fsp->fsp_name));
	}

	if (fsp->initial_delete_on_close && (lck->delete_token == NULL)) {
		BOOL became_user = False;

		/* Initial delete on close was set and no one else
		 * wrote a real delete on close. */

		if (current_user.vuid != fsp->vuid) {
			become_user(conn, fsp->vuid);
			became_user = True;
		}
		set_delete_on_close_lck(lck, True, &current_user.ut);
		if (became_user) {
			unbecome_user();
		}
	}

	delete_file = lck->delete_on_close;

	if (delete_file) {
		int i;
		/* See if others still have the file open. If this is the
		 * case, then don't delete. If all opens are POSIX delete now. */
		for (i=0; i<lck->num_share_modes; i++) {
			struct share_mode_entry *e = &lck->share_modes[i];
			if (is_valid_share_mode_entry(e)) {
				if (fsp->posix_open && (e->flags & SHARE_MODE_FLAG_POSIX_OPEN)) {
					continue;
				}
				delete_file = False;
				break;
			}
		}
	}

	/* Notify any deferred opens waiting on this close. */
	notify_deferred_opens(lck);
	reply_to_oplock_break_requests(fsp);

	/*
	 * NT can set delete_on_close of the last open
	 * reference to a file.
	 */

	if (!(close_type == NORMAL_CLOSE || close_type == SHUTDOWN_CLOSE)
	    || !delete_file
	    || (lck->delete_token == NULL)) {
		TALLOC_FREE(lck);
		return NT_STATUS_OK;
	}

	/*
	 * Ok, we have to delete the file
	 */

	DEBUG(5,("close_remove_share_mode: file %s. Delete on close was set "
		 "- deleting file.\n", fsp->fsp_name));

	/* Become the user who requested the delete. */

	if (!push_sec_ctx()) {
		smb_panic("close_remove_share_mode: file %s. failed to push "
			  "sec_ctx.\n");
	}

	set_sec_ctx(lck->delete_token->uid,
		    lck->delete_token->gid,
		    lck->delete_token->ngroups,
		    lck->delete_token->groups,
		    NULL);

	/* We can only delete the file if the name we have is still valid and
	   hasn't been renamed. */
	
	if(SMB_VFS_STAT(conn,fsp->fsp_name,&sbuf) != 0) {
		DEBUG(5,("close_remove_share_mode: file %s. Delete on close "
			 "was set and stat failed with error %s\n",
			 fsp->fsp_name, strerror(errno) ));
		/*
		 * Don't save the errno here, we ignore this error
		 */
		goto done;
	}

	if(sbuf.st_dev != fsp->dev || sbuf.st_ino != fsp->inode) {
		DEBUG(5,("close_remove_share_mode: file %s. Delete on close "
			 "was set and dev and/or inode does not match\n",
			 fsp->fsp_name ));
		DEBUG(5,("close_remove_share_mode: file %s. stored dev = %x, "
			 "inode = %.0f stat dev = %x, inode = %.0f\n",
			 fsp->fsp_name,
			 (unsigned int)fsp->dev, (double)fsp->inode,
			 (unsigned int)sbuf.st_dev, (double)sbuf.st_ino ));
		/*
		 * Don't save the errno here, we ignore this error
		 */
		goto done;
	}

	if (SMB_VFS_UNLINK(conn,fsp->fsp_name) != 0) {
		/*
		 * This call can potentially fail as another smbd may
		 * have had the file open with delete on close set and
		 * deleted it when its last reference to this file
		 * went away. Hence we log this but not at debug level
		 * zero.
		 */

		DEBUG(5,("close_remove_share_mode: file %s. Delete on close "
			 "was set and unlink failed with error %s\n",
			 fsp->fsp_name, strerror(errno) ));

		status = map_nt_error_from_unix(errno);
	}

	notify_fname(conn, NOTIFY_ACTION_REMOVED,
		     FILE_NOTIFY_CHANGE_FILE_NAME,
		     fsp->fsp_name);

	/* As we now have POSIX opens which can unlink
 	 * with other open files we may have taken
 	 * this code path with more than one share mode
 	 * entry - ensure we only delete once by resetting
 	 * the delete on close flag. JRA.
 	 */

	set_delete_on_close_lck(lck, False, NULL);

 done:

	/* unbecome user. */
	pop_sec_ctx();
	
	TALLOC_FREE(lck);
	return status;
}

/****************************************************************************
 Close a file.

 close_type can be NORMAL_CLOSE=0,SHUTDOWN_CLOSE,ERROR_CLOSE.
 printing and magic scripts are only run on normal close.
 delete on close is done on normal and shutdown close.
****************************************************************************/

static NTSTATUS close_normal_file(files_struct *fsp, enum file_close_type close_type)
{
	NTSTATUS status = NT_STATUS_OK;
	NTSTATUS saved_status1 = NT_STATUS_OK;
	NTSTATUS saved_status2 = NT_STATUS_OK;
	NTSTATUS saved_status3 = NT_STATUS_OK;
	connection_struct *conn = fsp->conn;

	if (fsp->aio_write_behind) {
		/*
	 	 * If we're finishing write behind on a close we can get a write
		 * error here, we must remember this.
		 */
		int ret = wait_for_aio_completion(fsp);
		if (ret) {
			saved_status1 = map_nt_error_from_unix(ret);
		}
	} else {
		cancel_aio_by_fsp(fsp);
	}
 
	/*
	 * If we're flushing on a close we can get a write
	 * error here, we must remember this.
	 */

	saved_status2 = close_filestruct(fsp);

	if (fsp->print_file) {
		print_fsp_end(fsp, close_type);
		file_free(fsp);
		return NT_STATUS_OK;
	}

	/* If this is an old DOS or FCB open and we have multiple opens on
	   the same handle we only have one share mode. Ensure we only remove
	   the share mode on the last close. */

	if (fsp->fh->ref_count == 1) {
		/* Should we return on error here... ? */
		saved_status3 = close_remove_share_mode(fsp, close_type);
	}

	if(fsp->oplock_type) {
		release_file_oplock(fsp);
	}

	locking_close_file(fsp);

	status = fd_close(conn, fsp);

	/* check for magic scripts */
	if (close_type == NORMAL_CLOSE) {
		check_magic(fsp,conn);
	}

	/*
	 * Ensure pending modtime is set after close.
	 */

	if (fsp->pending_modtime_owner && !null_timespec(fsp->pending_modtime)) {
		set_filetime(conn, fsp->fsp_name, fsp->pending_modtime);
	} else if (!null_timespec(fsp->last_write_time)) {
		set_filetime(conn, fsp->fsp_name, fsp->last_write_time);
	}

	if (NT_STATUS_IS_OK(status)) {
		if (!NT_STATUS_IS_OK(saved_status1)) {
			status = saved_status1;
		} else if (!NT_STATUS_IS_OK(saved_status2)) {
			status = saved_status2;
		} else if (!NT_STATUS_IS_OK(saved_status3)) {
			status = saved_status3;
		}
	}

	DEBUG(2,("%s closed file %s (numopen=%d) %s\n",
		conn->user,fsp->fsp_name,
		conn->num_files_open,
		nt_errstr(status) ));

	file_free(fsp);
	return status;
}

/****************************************************************************
 Close a directory opened by an NT SMB call. 
****************************************************************************/
  
static NTSTATUS close_directory(files_struct *fsp, enum file_close_type close_type)
{
	struct share_mode_lock *lck = 0;
	BOOL delete_dir = False;
	NTSTATUS status = NT_STATUS_OK;

	/*
	 * NT can set delete_on_close of the last open
	 * reference to a directory also.
	 */

	lck = get_share_mode_lock(NULL, fsp->dev, fsp->inode, NULL, NULL);

	if (lck == NULL) {
		DEBUG(0, ("close_directory: Could not get share mode lock for %s\n", fsp->fsp_name));
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (!del_share_mode(lck, fsp)) {
		DEBUG(0, ("close_directory: Could not delete share entry for %s\n", fsp->fsp_name));
	}

	if (fsp->initial_delete_on_close) {
		BOOL became_user = False;

		/* Initial delete on close was set - for
		 * directories we don't care if anyone else
		 * wrote a real delete on close. */

		if (current_user.vuid != fsp->vuid) {
			become_user(fsp->conn, fsp->vuid);
			became_user = True;
		}
		send_stat_cache_delete_message(fsp->fsp_name);
		set_delete_on_close_lck(lck, True, &current_user.ut);
		if (became_user) {
			unbecome_user();
		}
	}

	delete_dir = lck->delete_on_close;

	if (delete_dir) {
		int i;
		/* See if others still have the dir open. If this is the
		 * case, then don't delete. If all opens are POSIX delete now. */
		for (i=0; i<lck->num_share_modes; i++) {
			struct share_mode_entry *e = &lck->share_modes[i];
			if (is_valid_share_mode_entry(e)) {
				if (fsp->posix_open && (e->flags & SHARE_MODE_FLAG_POSIX_OPEN)) {
					continue;
				}
				delete_dir = False;
				break;
			}
		}
	}

	if ((close_type == NORMAL_CLOSE || close_type == SHUTDOWN_CLOSE) &&
				delete_dir &&
				lck->delete_token) {
	
		/* Become the user who requested the delete. */

		if (!push_sec_ctx()) {
			smb_panic("close_directory: failed to push sec_ctx.\n");
		}

		set_sec_ctx(lck->delete_token->uid,
				lck->delete_token->gid,
				lck->delete_token->ngroups,
				lck->delete_token->groups,
				NULL);

		TALLOC_FREE(lck);

		status = rmdir_internals(fsp->conn, fsp->fsp_name);

		DEBUG(5,("close_directory: %s. Delete on close was set - "
			 "deleting directory returned %s.\n",
			 fsp->fsp_name, nt_errstr(status)));

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

	/*
	 * Do the code common to files and directories.
	 */
	close_filestruct(fsp);
	file_free(fsp);
	return status;
}

/****************************************************************************
 Close a 'stat file' opened internally.
****************************************************************************/
  
NTSTATUS close_stat(files_struct *fsp)
{
	/*
	 * Do the code common to files and directories.
	 */
	close_filestruct(fsp);
	file_free(fsp);
	return NT_STATUS_OK;
}

/****************************************************************************
 Close a files_struct.
****************************************************************************/
  
NTSTATUS close_file(files_struct *fsp, enum file_close_type close_type)
{
	if(fsp->is_directory) {
		return close_directory(fsp, close_type);
	} else if (fsp->is_stat) {
		return close_stat(fsp);
	} else if (fsp->fake_file_handle != NULL) {
		return close_fake_file(fsp);
	}
	return close_normal_file(fsp, close_type);
}
