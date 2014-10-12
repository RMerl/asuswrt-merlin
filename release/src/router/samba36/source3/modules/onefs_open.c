/*
 * Unix SMB/CIFS implementation.
 *
 * This file began with some code from source3/smbd/open.c and has been
 * modified it work with ifs_createfile.
 *
 * ifs_createfile is a CIFS-specific syscall for opening/files and
 * directories.  It adds support for:
 *    - Full in-kernel access checks using a windows access_mask
 *    - Cluster-coherent share mode locks
 *    - Cluster-coherent oplocks
 *    - Streams
 *    - Setting security descriptors at create time
 *    - Setting dos_attributes at create time
 *
 * Copyright (C) Andrew Tridgell 1992-1998
 * Copyright (C) Jeremy Allison 2001-2004
 * Copyright (C) Volker Lendecke 2005
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
#include "oplock_onefs.h"
#include "smbd/globals.h"

extern const struct generic_mapping file_generic_mapping;

struct onefs_fsp_data {
	uint64_t oplock_callback_id;
};

static NTSTATUS onefs_create_file_unixpath(connection_struct *conn,
			      struct smb_request *req,
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
			      int *pinfo,
			      struct onefs_fsp_data *fsp_data);

/****************************************************************************
 Open a file.
****************************************************************************/

static NTSTATUS onefs_open_file(files_struct *fsp,
				connection_struct *conn,
				struct smb_request *req,
				const char *parent_dir,
				struct smb_filename *smb_fname,
				int flags,
				mode_t unx_mode,
				uint32 access_mask,
				uint32 open_access_mask,
				int oplock_request,
				uint64 id,
				uint32 share_access,
				uint32 create_options,
				uint32_t new_dos_attributes,
				struct security_descriptor *sd,
				int *granted_oplock)
{
	struct smb_filename *smb_fname_onefs = NULL;
	NTSTATUS status = NT_STATUS_OK;
	int accmode = (flags & O_ACCMODE);
	int local_flags = flags;
	bool file_existed = VALID_STAT(smb_fname->st);
	const char *wild;
	int base_fd = -1;

	fsp->fh->fd = -1;
	errno = EPERM;

	/* Check permissions */

	/*
	 * This code was changed after seeing a client open request
	 * containing the open mode of (DENY_WRITE/read-only) with
	 * the 'create if not exist' bit set. The previous code
	 * would fail to open the file read only on a read-only share
	 * as it was checking the flags parameter  directly against O_RDONLY,
	 * this was failing as the flags parameter was set to O_RDONLY|O_CREAT.
	 * JRA.
	 */

	if (!CAN_WRITE(conn)) {
		/* It's a read-only share - fail if we wanted to write. */
		if(accmode != O_RDONLY) {
			DEBUG(3, ("Permission denied opening %s\n",
				  smb_fname_str_dbg(smb_fname)));
			return NT_STATUS_ACCESS_DENIED;
		} else if(flags & O_CREAT) {
			/* We don't want to write - but we must make sure that
			   O_CREAT doesn't create the file if we have write
			   access into the directory.
			*/
			flags &= ~O_CREAT;
			local_flags &= ~O_CREAT;
		}
	}

	/*
	 * This little piece of insanity is inspired by the
	 * fact that an NT client can open a file for O_RDONLY,
	 * but set the create disposition to FILE_EXISTS_TRUNCATE.
	 * If the client *can* write to the file, then it expects to
	 * truncate the file, even though it is opening for readonly.
	 * Quicken uses this stupid trick in backup file creation...
	 * Thanks *greatly* to "David W. Chapman Jr." <dwcjr@inethouston.net>
	 * for helping track this one down. It didn't bite us in 2.0.x
	 * as we always opened files read-write in that release. JRA.
	 */

	if ((accmode == O_RDONLY) && ((flags & O_TRUNC) == O_TRUNC)) {
		DEBUG(10,("onefs_open_file: truncate requested on read-only "
			  "open for file %s\n", smb_fname_str_dbg(smb_fname)));
		local_flags = (flags & ~O_ACCMODE)|O_RDWR;
	}

#if defined(O_NONBLOCK) && defined(S_ISFIFO)
	/*
	 * We would block on opening a FIFO with no one else on the
	 * other end. Do what we used to do and add O_NONBLOCK to the
	 * open flags. JRA.
	 */

	if (file_existed && S_ISFIFO(smb_fname->st.st_ex_mode)) {
		local_flags |= O_NONBLOCK;
	}
#endif

	/* Don't create files with Microsoft wildcard characters. */
	if (fsp->base_fsp) {
		/*
		 * wildcard characters are allowed in stream names
		 * only test the basefilename
		 */
		wild = fsp->base_fsp->fsp_name->base_name;
	} else {
		wild = smb_fname->base_name;
	}
	if ((local_flags & O_CREAT) && !file_existed &&
	    ms_has_wild(wild))  {
		/*
		 * XXX: may need to remvoe this return...
		 *
		 * We dont think this check needs to exist. All it does is
		 * block creating files with Microsoft wildcards, which is
		 * fine if the creation originated from NFS or locally and
		 * then was copied via Samba.
		 */
		DEBUG(1, ("onefs_open_file: creating file with wildcard: %s\n",
			  smb_fname_str_dbg(smb_fname)));
		return NT_STATUS_OBJECT_NAME_INVALID;
	}

	/* Actually do the open */

#ifdef O_NOFOLLOW
	/*
	 * Never follow symlinks on a POSIX client. The
	 * client should be doing this.
	 */

	if (fsp->posix_open || !lp_symlinks(SNUM(conn))) {
		flags |= O_NOFOLLOW;
	}
#endif
	/* Setup a onefs-style smb_fname struct. */
	status = onefs_stream_prep_smb_fname(talloc_tos(), smb_fname,
					     &smb_fname_onefs);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	/* If it's a stream pass in the base_fd */
	if ((conn->fs_capabilities & FILE_NAMED_STREAMS) &&
	    is_ntfs_stream_smb_fname(smb_fname_onefs)) {
		SMB_ASSERT(fsp->base_fsp);

		DEBUG(10, ("Opening a stream: base=%s(%d), stream=%s\n",
			   smb_fname_onefs->base_name, fsp->base_fsp->fh->fd,
			   smb_fname_onefs->stream_name));

		base_fd = fsp->base_fsp->fh->fd;
	}

	fsp->fh->fd = onefs_sys_create_file(conn,
					    base_fd,
					    smb_fname_onefs->stream_name != NULL ?
					    smb_fname_onefs->stream_name :
					    smb_fname_onefs->base_name,
					    access_mask,
					    open_access_mask,
					    share_access,
					    create_options,
					    flags,
					    unx_mode,
					    oplock_request,
					    id,
					    sd,
					    new_dos_attributes,
					    granted_oplock);
	TALLOC_FREE(smb_fname_onefs);

	if (fsp->fh->fd == -1) {
		if (errno == EMFILE) {
			static time_t last_warned = 0L;

			if (time((time_t *) NULL) > last_warned) {
				DEBUG(0, ("Too many open files, unable "
					  "to open more!  smbd's max "
					  "open files = %d, also check "
					  "sysctl kern.maxfiles and "
					  "sysctl kern.maxfilesperproc\n",
					  lp_max_open_files()));
				last_warned = time((time_t *) NULL);
			}
		}

		status = map_nt_error_from_unix(errno);
		DEBUG(3, ("Error opening file %s (%s) (local_flags=%d) "
			  "(flags=%d)\n", smb_fname_str_dbg(smb_fname),
			  strerror(errno), local_flags, flags));
		return status;
	}

	if ((local_flags & O_CREAT) && !file_existed) {

		/* Inherit the ACL if required */
		if (lp_inherit_perms(SNUM(conn))) {
			inherit_access_posix_acl(conn, parent_dir,
			    smb_fname->base_name, unx_mode);
		}

		/* Change the owner if required. */
		if (lp_inherit_owner(SNUM(conn))) {
			change_file_owner_to_parent(conn, parent_dir,
			    fsp);
		}

		notify_fname(conn, NOTIFY_ACTION_ADDED,
		    FILE_NOTIFY_CHANGE_FILE_NAME, smb_fname->base_name);
	}

	if (!file_existed) {
		int ret;

		if (fsp->fh->fd == -1) {
			ret = SMB_VFS_STAT(conn, smb_fname);
		} else {
			ret = SMB_VFS_FSTAT(fsp, &smb_fname->st);
			/* If we have an fd, this stat should succeed. */
			if (ret == -1) {
				DEBUG(0, ("Error doing fstat on open file %s "
					  "(%s)\n",
					  smb_fname_str_dbg(smb_fname),
					  strerror(errno) ));
			}
		}

		/* For a non-io open, this stat failing means file not found. JRA */
		if (ret == -1) {
			status = map_nt_error_from_unix(errno);
			fd_close(fsp);
			return status;
		}
	}

	/*
	 * POSIX allows read-only opens of directories. We don't
	 * want to do this (we use a different code path for this)
	 * so catch a directory open and return an EISDIR. JRA.
	 */

	if(S_ISDIR(smb_fname->st.st_ex_mode)) {
		fd_close(fsp);
		errno = EISDIR;
		return NT_STATUS_FILE_IS_A_DIRECTORY;
	}

	fsp->mode = smb_fname->st.st_ex_mode;
	fsp->file_id = vfs_file_id_from_sbuf(conn, &smb_fname->st);
	fsp->vuid = req ? req->vuid : UID_FIELD_INVALID;
	fsp->file_pid = req ? req->smbpid : 0;
	fsp->can_lock = True;
	fsp->can_read = (access_mask & (FILE_READ_DATA)) ? True : False;
	if (!CAN_WRITE(conn)) {
		fsp->can_write = False;
	} else {
		fsp->can_write = (access_mask & (FILE_WRITE_DATA | FILE_APPEND_DATA)) ?
			True : False;
	}
	fsp->print_file = NULL;
	fsp->modified = False;
	fsp->sent_oplock_break = NO_BREAK_SENT;
	fsp->is_directory = False;
	if (conn->aio_write_behind_list &&
	    is_in_path(smb_fname->base_name, conn->aio_write_behind_list,
		       conn->case_sensitive)) {
		fsp->aio_write_behind = True;
	}

	fsp->wcp = NULL; /* Write cache pointer. */

	DEBUG(2,("%s opened file %s read=%s write=%s (numopen=%d)\n",
		 conn->session_info->unix_name,
		 smb_fname_str_dbg(smb_fname),
		 BOOLSTR(fsp->can_read), BOOLSTR(fsp->can_write),
		 conn->num_files_open));

	errno = 0;
	return NT_STATUS_OK;
}

/****************************************************************************
 Handle the 1 second delay in returning a SHARING_VIOLATION error.
****************************************************************************/

static void defer_open(struct share_mode_lock *lck,
		       struct timeval request_time,
		       struct timeval timeout,
		       struct smb_request *req,
		       struct deferred_open_record *state)
{
	int i;

	/* Paranoia check */

	for (i=0; i<lck->num_share_modes; i++) {
		struct share_mode_entry *e = &lck->share_modes[i];

		if (!is_deferred_open_entry(e)) {
			continue;
		}

		if (procid_is_me(&e->pid) && (e->op_mid == req->mid)) {
			DEBUG(0, ("Trying to defer an already deferred "
				"request: mid=%llu, exiting\n",
				(unsigned long long)req->mid));
			exit_server("attempt to defer a deferred request");
		}
	}

	/* End paranoia check */

	DEBUG(10,("defer_open_sharing_error: time [%u.%06u] adding deferred "
		  "open entry for mid %llu\n",
		  (unsigned int)request_time.tv_sec,
		  (unsigned int)request_time.tv_usec,
		  (unsigned long long)req->mid));

	if (!push_deferred_open_message_smb(req, request_time, timeout,
				       state->id, (char *)state, sizeof(*state))) {
		exit_server("push_deferred_open_message_smb failed");
	}
	add_deferred_open(lck, req->mid, request_time, state->id);
}

static void schedule_defer_open(struct share_mode_lock *lck,
				struct timeval request_time,
				struct smb_request *req)
{
	struct deferred_open_record state;

	/* This is a relative time, added to the absolute
	   request_time value to get the absolute timeout time.
	   Note that if this is the second or greater time we enter
	   this codepath for this particular request mid then
	   request_time is left as the absolute time of the *first*
	   time this request mid was processed. This is what allows
	   the request to eventually time out. */

	struct timeval timeout;

	/* Normally the smbd we asked should respond within
	 * OPLOCK_BREAK_TIMEOUT seconds regardless of whether
	 * the client did, give twice the timeout as a safety
	 * measure here in case the other smbd is stuck
	 * somewhere else. */

	/*
	 * On OneFS, the kernel will always send an oplock_revoked message
	 * before this timeout is hit.
	 */
	timeout = timeval_set(OPLOCK_BREAK_TIMEOUT*10, 0);

	/* Nothing actually uses state.delayed_for_oplocks
	   but it's handy to differentiate in debug messages
	   between a 30 second delay due to oplock break, and
	   a 1 second delay for share mode conflicts. */

	state.delayed_for_oplocks = True;
	state.failed = false;
	state.id = lck->id;

	if (!request_timed_out(request_time, timeout)) {
		defer_open(lck, request_time, timeout, req, &state);
	} else {
		/* A delayed-for-oplocks deferred open timing out should only
		 * happen if there is a bug or extreme load, since we set the
		 * timeout to 300 seconds. */
		DEBUG(0, ("Deferred open timeout! request_time=%d.%d, "
		    "mid=%d\n", request_time.tv_sec, request_time.tv_usec,
		    req->mid));
	}
}

/****************************************************************************
 Open a file with a share mode.  Passed in an already created files_struct.
****************************************************************************/
NTSTATUS onefs_open_file_ntcreate(connection_struct *conn,
				  struct smb_request *req,
				  struct smb_filename *smb_fname,
				  uint32 access_mask,
				  uint32 share_access,
				  uint32 create_disposition,
				  uint32 create_options,
				  uint32 new_dos_attributes,
				  int oplock_request,
				  uint32_t private_flags,
				  struct security_descriptor *sd,
				  files_struct *fsp,
				  int *pinfo,
				  struct onefs_fsp_data *fsp_data)
{
	int flags=0;
	int flags2=0;
	bool file_existed = VALID_STAT(smb_fname->st);
	bool def_acl = False;
	bool posix_open = False;
	bool new_file_created = False;
	bool clear_ads = False;
	struct file_id id;
	mode_t new_unx_mode = (mode_t)0;
	mode_t unx_mode = (mode_t)0;
	int info;
	uint32 existing_dos_attributes = 0;
	struct timeval request_time = timeval_zero();
	struct share_mode_lock *lck = NULL;
	uint32 open_access_mask = access_mask;
	NTSTATUS status;
	int ret_flock;
	char *parent_dir;
	int granted_oplock;
	uint64_t oplock_callback_id = 0;
	uint32 createfile_attributes = 0;

	ZERO_STRUCT(id);

	if (conn->printer) {
		/*
		 * Printers are handled completely differently.
		 * Most of the passed parameters are ignored.
		 */

		if (pinfo) {
			*pinfo = FILE_WAS_CREATED;
		}

		DEBUG(10, ("onefs_open_file_ntcreate: printer open fname=%s\n",
			   smb_fname_str_dbg(smb_fname)));

		return print_spool_open(fsp, smb_fname->base_name,
					req->vuid);
	}

	if (!parent_dirname(talloc_tos(), smb_fname->base_name, &parent_dir,
			    NULL)) {
		return NT_STATUS_NO_MEMORY;
	}

	if (new_dos_attributes & FILE_FLAG_POSIX_SEMANTICS) {
		posix_open = True;
		unx_mode = (mode_t)(new_dos_attributes & ~FILE_FLAG_POSIX_SEMANTICS);
		new_dos_attributes = 0;
	} else {
		/* We add FILE_ATTRIBUTE_ARCHIVE to this as this mode is only used if the file is
		 * created new. */
		unx_mode = unix_mode(conn, new_dos_attributes | FILE_ATTRIBUTE_ARCHIVE,
				     smb_fname, parent_dir);
	}

	DEBUG(10,("onefs_open_file_ntcreate: fname=%s, dos_attrs=0x%x "
		  "access_mask=0x%x share_access=0x%x "
		  "create_disposition = 0x%x create_options=0x%x "
		  "unix mode=0%o oplock_request=0x%x\n",
		  smb_fname_str_dbg(smb_fname), new_dos_attributes,
		  access_mask, share_access, create_disposition,
		  create_options, unx_mode, oplock_request));

	/*
	 * Any non-stat-only open has the potential to contend oplocks, which
	 * means to avoid blocking in the kernel (which is unacceptable), the
	 * open must be deferred.  In order to defer opens, req must not be
	 * NULL.  The known cases of calling with a NULL req:
	 *
	 *   1. Open the base file of a stream: Always done stat-only
	 *
	 *   2. open_file_fchmod(), which is called from 3 places:
	 *      A. try_chown: Posix acls only. Never called on onefs.
	 *      B. set_ea_dos_attributes: Can't be called from onefs, because
	 *         SMB_VFS_SETXATTR return ENOSYS.
	 *      C. file_set_dos_mode: This would only happen if the "dos
	 *         filemode" smb.conf parameter is set to yes.  We ship with
	 *	   it off, but if a customer were to turn it on it would be
	 *         bad.
	 */
	if (req == NULL && !is_stat_open(access_mask) &&
	    !is_ntfs_stream_smb_fname(smb_fname)) {
		smb_panic("NULL req on a non-stat-open!");
	}

	if ((req == NULL) && ((oplock_request & INTERNAL_OPEN_ONLY) == 0)) {
		DEBUG(0, ("No smb request but not an internal only open!\n"));
		return NT_STATUS_INTERNAL_ERROR;
	}

	/*
	 * Only non-internal opens can be deferred at all
	 */

	if (req) {
		void *ptr;
		if (get_deferred_open_message_state(req,
				&request_time,
				&ptr)) {
			struct deferred_open_record *state = (struct deferred_open_record *)ptr;

			/* Remember the absolute time of the original
			   request with this mid. We'll use it later to
			   see if this has timed out. */

			/* Remove the deferred open entry under lock. */
			remove_deferred_open_entry(state->id, req->mid);

			/* Ensure we don't reprocess this message. */
			remove_deferred_open_message_smb(req->mid);

			/*
			 * When receiving a semlock_async_failure message, the
			 * deferred open will be marked as "failed". Returning
			 * INTERNAL_ERROR.
			 */
			if (state->failed) {
				DEBUG(0, ("onefs_open_file_ntcreate: "
					  "semlock_async_failure detected!\n"));
				return NT_STATUS_INTERNAL_ERROR;
			}
		}
	}

	status = check_name(conn, smb_fname->base_name);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (!posix_open) {
		new_dos_attributes &= SAMBA_ATTRIBUTES_MASK;
		if (file_existed) {
			existing_dos_attributes = dos_mode(conn, smb_fname);
		}
	}

	/* Setup dos_attributes to be set by ifs_createfile */
	if (lp_store_dos_attributes(SNUM(conn))) {
		createfile_attributes = (new_dos_attributes | FILE_ATTRIBUTE_ARCHIVE) &
		    ~(FILE_ATTRIBUTE_NONINDEXED | FILE_ATTRIBUTE_COMPRESSED);
	}

	/* Ignore oplock requests if oplocks are disabled. */
	if (!lp_oplocks(SNUM(conn)) ||
	    IS_VETO_OPLOCK_PATH(conn, smb_fname->base_name)) {
		/* Mask off everything except the private Samba bits. */
		oplock_request &= SAMBA_PRIVATE_OPLOCK_MASK;
	}

	/* this is for OS/2 long file names - say we don't support them */
	if (!lp_posix_pathnames() && strstr(smb_fname->base_name,".+,;=[].")) {
		/* OS/2 Workplace shell fix may be main code stream in a later
		 * release. */
		DEBUG(5,("onefs_open_file_ntcreate: OS/2 long filenames are "
			  "not supported.\n"));
		if (use_nt_status()) {
			return NT_STATUS_OBJECT_NAME_NOT_FOUND;
		}
		return NT_STATUS_DOS(ERRDOS, ERRcannotopen);
	}

	switch( create_disposition ) {
		/*
		 * Currently we're using FILE_SUPERSEDE as the same as
		 * FILE_OVERWRITE_IF but they really are
		 * different. FILE_SUPERSEDE deletes an existing file
		 * (requiring delete access) then recreates it.
		 */
		case FILE_SUPERSEDE:
			/**
			 * @todo: Clear all file attributes?
			 * http://www.osronline.com/article.cfm?article=302
			 * create if not exist, trunc if exist
			 *
			 * If file exists replace/overwrite. If file doesn't
			 * exist create.
			 */
			flags2 |= (O_CREAT | O_TRUNC);
			clear_ads = true;
			break;

		case FILE_OVERWRITE_IF:
			/* If file exists replace/overwrite. If file doesn't
			 * exist create. */
			flags2 |= (O_CREAT | O_TRUNC);
			clear_ads = true;
			break;

		case FILE_OPEN:
			/* If file exists open. If file doesn't exist error. */
			if (!file_existed) {
				DEBUG(5,("onefs_open_file_ntcreate: FILE_OPEN "
					 "requested for file %s and file "
					 "doesn't exist.\n",
					 smb_fname_str_dbg(smb_fname)));
				errno = ENOENT;
				return NT_STATUS_OBJECT_NAME_NOT_FOUND;
			}
			break;

		case FILE_OVERWRITE:
			/* If file exists overwrite. If file doesn't exist
			 * error. */
			if (!file_existed) {
				DEBUG(5, ("onefs_open_file_ntcreate: "
					  "FILE_OVERWRITE requested for file "
					  "%s and file doesn't exist.\n",
					  smb_fname_str_dbg(smb_fname)));
				errno = ENOENT;
				return NT_STATUS_OBJECT_NAME_NOT_FOUND;
			}
			flags2 |= O_TRUNC;
			clear_ads = true;
			break;

		case FILE_CREATE:
			/* If file exists error. If file doesn't exist
			 * create. */
			if (file_existed) {
				DEBUG(5, ("onefs_open_file_ntcreate: "
					  "FILE_CREATE requested for file %s "
					  "and file already exists.\n",
					  smb_fname_str_dbg(smb_fname)));
				if (S_ISDIR(smb_fname->st.st_ex_mode)) {
					errno = EISDIR;
				} else {
					errno = EEXIST;
				}
				return map_nt_error_from_unix(errno);
			}
			flags2 |= (O_CREAT|O_EXCL);
			break;

		case FILE_OPEN_IF:
			/* If file exists open. If file doesn't exist
			 * create. */
			flags2 |= O_CREAT;
			break;

		default:
			return NT_STATUS_INVALID_PARAMETER;
	}

	/* Match attributes on file exists and overwrite. */
	if (!posix_open && file_existed &&
	    ((create_disposition == FILE_OVERWRITE) ||
		(create_disposition == FILE_OVERWRITE_IF))) {
		if (!open_match_attributes(conn, existing_dos_attributes,
					   new_dos_attributes,
					   smb_fname->st.st_ex_mode,
					   unx_mode, &new_unx_mode)) {
			DEBUG(5, ("onefs_open_file_ntcreate: attributes "
				  "missmatch for file %s (%x %x) (0%o, 0%o)\n",
				  smb_fname_str_dbg(smb_fname),
				  existing_dos_attributes,
				  new_dos_attributes,
				  (unsigned int)smb_fname->st.st_ex_mode,
				  (unsigned int)unx_mode ));
			errno = EACCES;
			return NT_STATUS_ACCESS_DENIED;
		}
	}

	/*
	 * OneFS understands MAXIMUM_ALLOWED_ACCESS, so only hack the
	 * access_mask, but leave the MAA for the actual open in
	 * open_access_mask.
	 */
	open_access_mask = access_mask;
	if (open_access_mask & MAXIMUM_ALLOWED_ACCESS) {
		access_mask |= FILE_GENERIC_ALL;
	}

	/* Convert GENERIC bits to specific bits. */
	se_map_generic(&access_mask, &file_generic_mapping);
	se_map_generic(&open_access_mask, &file_generic_mapping);

	if ((flags2 & O_TRUNC) || (oplock_request & FORCE_OPLOCK_BREAK_TO_NONE)) {
		/* This will cause oplock breaks. */
		open_access_mask |= FILE_WRITE_DATA;
	}

	DEBUG(10, ("onefs_open_file_ntcreate: fname=%s, after mapping "
		   "open_access_mask=%#x, access_mask=0x%x\n",
		   smb_fname_str_dbg(smb_fname), open_access_mask,
		   access_mask));

	/*
	 * Note that we ignore the append flag as append does not
	 * mean the same thing under DOS and Unix.
	 */

	if ((access_mask & (FILE_WRITE_DATA | FILE_APPEND_DATA)) ||
	    (oplock_request & FORCE_OPLOCK_BREAK_TO_NONE)) {

		/*
		 * DENY_DOS opens are always underlying read-write on the
		 * file handle, no matter what the requested access mask
		 * says. Stock samba just sets the flags, but since
		 * ifs_createfile uses the access_mask, it must be updated as
		 * well.  This allows BASE-DENY* to pass.
		 */
		if (create_options & NTCREATEX_OPTIONS_PRIVATE_DENY_DOS) {

			DEBUG(10,("onefs_open_file_ntcreate: deny_dos: "
				  "Adding O_RDWR to flags "
				  "(0x%x) and some READ bits to "
				  "open_access_mask (0x%x)\n",
				  flags, open_access_mask));

			flags = O_RDWR;
			open_access_mask |= (FILE_READ_ATTRIBUTES |
			    FILE_READ_DATA | FILE_READ_EA | FILE_EXECUTE);

		} else if (access_mask & (FILE_READ_ATTRIBUTES |
			       FILE_READ_DATA |
			       FILE_READ_EA |
			       FILE_EXECUTE)) {
			flags = O_RDWR;
		} else {
			flags = O_WRONLY;
		}
	} else {
		flags = O_RDONLY;
	}

	/* Currently we only look at FILE_WRITE_THROUGH for create options. */
#if defined(O_SYNC)
	if ((create_options & FILE_WRITE_THROUGH) &&
	    lp_strict_sync(SNUM(conn))) {
		flags2 |= O_SYNC;
	}
#endif /* O_SYNC */

	if (posix_open && (access_mask & FILE_APPEND_DATA)) {
		flags2 |= O_APPEND;
	}

	if (!posix_open && !CAN_WRITE(conn)) {
		/*
		 * We should really return a permission denied error if either
		 * O_CREAT or O_TRUNC are set, but for compatibility with
		 * older versions of Samba we just AND them out.
		 */
		flags2 &= ~(O_CREAT|O_TRUNC);

		/* Deny DELETE_ACCESS explicitly if the share is read only. */
		if (access_mask & DELETE_ACCESS) {
			return map_nt_error_from_unix(EACCES);
		}
	}

	/* Ensure we can't write on a read-only share or file. */
	if (flags != O_RDONLY && file_existed &&
	    (!CAN_WRITE(conn) || IS_DOS_READONLY(existing_dos_attributes))) {
		DEBUG(5, ("onefs_open_file_ntcreate: write access requested "
			  "for file %s on read only %s\n",
			  smb_fname_str_dbg(smb_fname),
			  !CAN_WRITE(conn) ? "share" : "file" ));
		errno = EACCES;
		return NT_STATUS_ACCESS_DENIED;
	}

	DEBUG(10, ("fsp = %p\n", fsp));

	fsp->share_access = share_access;
	fsp->fh->private_options = private_flags;
	fsp->access_mask = open_access_mask; /* We change this to the
					      * requested access_mask after
					      * the open is done. */
	fsp->posix_open = posix_open;

	/* Ensure no SAMBA_PRIVATE bits can be set. */
	fsp->oplock_type = (oplock_request & ~SAMBA_PRIVATE_OPLOCK_MASK);

	if (timeval_is_zero(&request_time)) {
		request_time = fsp->open_time;
	}

	if (file_existed) {
		struct timespec old_write_time = smb_fname->st.st_ex_mtime;
		id = vfs_file_id_from_sbuf(conn, &smb_fname->st);

		lck = get_share_mode_lock(talloc_tos(), id,
					  conn->connectpath,
					  smb_fname, &old_write_time);

		if (lck == NULL) {
			DEBUG(0, ("Could not get share mode lock\n"));
			return NT_STATUS_SHARING_VIOLATION;
		}

		if (lck->delete_on_close) {
			/* DELETE_PENDING is not deferred for a second */
			TALLOC_FREE(lck);
			return NT_STATUS_DELETE_PENDING;
		}
	}

	SMB_ASSERT(!file_existed || (lck != NULL));

	/*
	 * Ensure we pay attention to default ACLs on directories.  May be
	 * neccessary depending on ACL policies.
	 */
        if ((flags2 & O_CREAT) && lp_inherit_acls(SNUM(conn)) &&
	    (def_acl = directory_has_default_acl(conn, parent_dir))) {
		unx_mode = 0777;
	}

	DEBUG(4,("calling onefs_open_file with flags=0x%X flags2=0x%X "
		 "mode=0%o, access_mask = 0x%x, open_access_mask = 0x%x\n",
		 (unsigned int)flags, (unsigned int)flags2,
		 (unsigned int)unx_mode, (unsigned int)access_mask,
		 (unsigned int)open_access_mask));

	/*
	 * Since the open is guaranteed to be stat only if req == NULL, a
	 * callback record is only needed if req != NULL.
	 */
	if (req) {
		SMB_ASSERT(fsp_data);
		oplock_callback_id = onefs_oplock_wait_record(req->mid);
		if (oplock_callback_id == 0) {
			return NT_STATUS_NO_MEMORY;
		}
	} else {
		/*
		 * It is also already asserted it's either a stream or a
		 * stat-only open at this point.
		 */
		SMB_ASSERT(fsp->oplock_type == NO_OPLOCK);

		/* The kernel and Samba's version of stat-only differs
		 * slightly: The kernel doesn't think its stat-only if we're
		 * truncating.  We'd better have a req in order to defer the
		 * open. */
		SMB_ASSERT(!((flags|flags2) & O_TRUNC));
	}

	/* Do the open. */
	status = onefs_open_file(fsp,
				 conn,
				 req,
				 parent_dir,
				 smb_fname,
				 flags|flags2,
				 unx_mode,
				 access_mask,
				 open_access_mask,
				 fsp->oplock_type,
				 oplock_callback_id,
				 share_access,
				 create_options,
				 createfile_attributes,
				 sd,
				 &granted_oplock);

	if (!NT_STATUS_IS_OK(status)) {

		/* OneFS Oplock Handling */
		if (errno == EINPROGRESS) {

			/* If we get EINPROGRESS, the kernel will send us an
			 * asynchronous semlock event back. Ensure we can defer
			 * the open, by asserting req. */
			SMB_ASSERT(req);

			if (lck == NULL) {
				/*
				 * We hit the race that when we did the stat
				 * on the file it did not exist, and someone
				 * has created it in between the stat and the
				 * open_file() call. Defer our open waiting,
				 * to break the oplock of the first opener.
				 */

				struct timespec old_write_time;

				DEBUG(3, ("Someone created file %s with an "
					  "oplock after we looked: Retrying\n",
					  smb_fname_str_dbg(smb_fname)));
				/*
				 * We hit the race that when we did the stat
				 * on the file it did not exist, and someone
				 * has created it in between the stat and the
				 * open_file() call. Just retry immediately.
				 */
				id = vfs_file_id_from_sbuf(conn,
							   &smb_fname->st);
				if (!(lck = get_share_mode_lock(talloc_tos(),
					  id, conn->connectpath, smb_fname,
					  &old_write_time))) {
					/*
					 * Emergency exit
					 */
					DEBUG(0, ("onefs_open_file_ntcreate: "
						  "Could not get share mode "
						  "lock for %s\n",
						smb_fname_str_dbg(smb_fname)));
					status = NT_STATUS_SHARING_VIOLATION;

					/* XXXZLK: This will cause us to get a
					 * semlock event when we aren't
					 * expecting one. */
					goto cleanup_destroy;
				}

				schedule_defer_open(lck, request_time, req);
				goto cleanup;
			}
			/* Waiting for an oplock */
			DEBUG(5,("Async createfile because a client has an "
				 "oplock on %s\n",
				 smb_fname_str_dbg(smb_fname)));

			SMB_ASSERT(req);
			schedule_defer_open(lck, request_time, req);
			goto cleanup;
		}

		/* Check for a sharing violation */
		if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
			uint32 can_access_mask;
			bool can_access = True;

			/* If we raced on open we may not have a valid file_id
			 * or stat buf.  Get them again. */
			if (SMB_VFS_STAT(conn, fname, psbuf) == -1) {
				DEBUG(0,("Error doing stat on file %s "
					"(%s)\n", fname, strerror(errno)));
				status = NT_STATUS_SHARING_VIOLATION;
				goto cleanup_destroy;
			}
			id = vfs_file_id_from_sbuf(conn, psbuf);

			/* Check if this can be done with the deny_dos and fcb
			 * calls. */

			/* Try to find dup fsp if possible. */
			if (private_flags &
			    (NTCREATEX_OPTIONS_PRIVATE_DENY_DOS|
			     NTCREATEX_OPTIONS_PRIVATE_DENY_FCB)) {

				if (req == NULL) {
					DEBUG(0, ("DOS open without an SMB "
						  "request!\n"));
					status = NT_STATUS_INTERNAL_ERROR;
					goto cleanup_destroy;
				}

				/* Use the client requested access mask here,
				 * not the one we open with. */
				status = fcb_or_dos_open(req,
							conn,
							fsp,
							smb_fname,
							id,
							req->smbpid,
							req->vuid,
							access_mask,
							share_access,
							create_options);

				if (NT_STATUS_IS_OK(status)) {
					if (pinfo) {
						*pinfo = FILE_WAS_OPENED;
					}
					status =  NT_STATUS_OK;
					goto cleanup;
				}
			}

			/*
			 * This next line is a subtlety we need for
			 * MS-Access. If a file open will fail due to share
			 * permissions and also for security (access) reasons,
			 * we need to return the access failed error, not the
			 * share error. We can't open the file due to kernel
			 * oplock deadlock (it's possible we failed above on
			 * the open_mode_check()) so use a userspace check.
			 */

			if (flags & O_RDWR) {
				can_access_mask = FILE_READ_DATA|FILE_WRITE_DATA;
			} else if (flags & O_WRONLY) {
				can_access_mask = FILE_WRITE_DATA;
			} else {
				can_access_mask = FILE_READ_DATA;
			}

			if (((can_access_mask & FILE_WRITE_DATA) &&
				!CAN_WRITE(conn)) ||
			    !can_access_file_data(conn, smb_fname,
						  can_access_mask)) {
				can_access = False;
			}

			/*
			 * If we're returning a share violation, ensure we
			 * cope with the braindead 1 second delay.
			 */
			if (!(oplock_request & INTERNAL_OPEN_ONLY) &&
			    lp_defer_sharing_violations()) {
				struct timeval timeout;
				struct deferred_open_record state;
				int timeout_usecs;

				/* this is a hack to speed up torture tests
				   in 'make test' */
				timeout_usecs = lp_parm_int(SNUM(conn),
				    "smbd","sharedelay",
				    SHARING_VIOLATION_USEC_WAIT);

				/* This is a relative time, added to the
				   absolute request_time value to get the
				   absolute timeout time.  Note that if this
				   is the second or greater time we enter this
				   codepath for this particular request mid
				   then request_time is left as the absolute
				   time of the *first* time this request mid
				   was processed. This is what allows the
				   request to eventually time out. */

				timeout = timeval_set(0, timeout_usecs);

				/* Nothing actually uses
				   state.delayed_for_oplocks but it's handy to
				   differentiate in debug messages between a
				   30 second delay due to oplock break, and a
				   1 second delay for share mode conflicts. */

				state.delayed_for_oplocks = False;
				state.id = id;
				state.failed = false;

				/*
				 * We hit the race that when we did the stat
				 * on the file it did not exist, and someone
				 * has created it in between the stat and the
				 * open_file() call.  Retrieve the share_mode
				 * lock on the newly opened file so we can
				 * defer our request.
				 */
				if (lck == NULL) {
					struct timespec old_write_time;
					old_write_time = get_mtimespec(psbuf);

					lck = get_share_mode_lock(talloc_tos(),
					    id, conn->connectpath, fname,
					    &old_write_time);
					if (lck == NULL) {
						DEBUG(0,
						    ("onefs_open_file_ntcreate:"
						     " Could not get share "
						     "mode lock for %s\n",
						     fname));
						/* This will cause us to return
						 * immediately skipping the
						 * the 1 second delay, which
						 * isn't a big deal */
						status = NT_STATUS_SHARING_VIOLATION;
						goto cleanup_destroy;
					}
				}

				if ((req != NULL) &&
				    !request_timed_out(request_time, timeout))
				{
					defer_open(lck, request_time, timeout,
						   req, &state);
				}
			}

			if (can_access) {
				/*
				 * We have detected a sharing violation here
				 * so return the correct error code
				 */
				status = NT_STATUS_SHARING_VIOLATION;
			} else {
				status = NT_STATUS_ACCESS_DENIED;
			}

			goto cleanup_destroy;
		}

		/*
		 * Normal error, for example EACCES
		 */
	 cleanup_destroy:
		if (oplock_callback_id != 0) {
			destroy_onefs_callback_record(oplock_callback_id);
		}
	 cleanup:
		TALLOC_FREE(lck);
		return status;
	}

	fsp->oplock_type = granted_oplock;

	if (oplock_callback_id != 0) {
		onefs_set_oplock_callback(oplock_callback_id, fsp);
		fsp_data->oplock_callback_id = oplock_callback_id;
	} else {
		SMB_ASSERT(fsp->oplock_type == NO_OPLOCK);
	}

	if (!file_existed) {
		struct timespec old_write_time = smb_fname->st.st_ex_mtime;
		/*
		 * Deal with the race condition where two smbd's detect the
		 * file doesn't exist and do the create at the same time. One
		 * of them will win and set a share mode, the other (ie. this
		 * one) should check if the requested share mode for this
		 * create is allowed.
		 */

		/*
		 * Now the file exists and fsp is successfully opened,
		 * fsp->file_id is valid and should replace the
		 * dev=0, inode=0 from a non existent file. Spotted by
		 * Nadav Danieli <nadavd@exanet.com>. JRA.
		 */

		id = fsp->file_id;

		lck = get_share_mode_lock(talloc_tos(), id,
					  conn->connectpath,
					  smb_fname, &old_write_time);

		if (lck == NULL) {
			DEBUG(0, ("onefs_open_file_ntcreate: Could not get "
				  "share mode lock for %s\n",
				  smb_fname_str_dbg(smb_fname)));
			fd_close(fsp);
			return NT_STATUS_SHARING_VIOLATION;
		}

		if (lck->delete_on_close) {
			status = NT_STATUS_DELETE_PENDING;
		}

		if (!NT_STATUS_IS_OK(status)) {
			struct deferred_open_record state;

			fd_close(fsp);

			state.delayed_for_oplocks = False;
			state.id = id;

			/* Do it all over again immediately. In the second
			 * round we will find that the file existed and handle
			 * the DELETE_PENDING and FCB cases correctly. No need
			 * to duplicate the code here. Essentially this is a
			 * "goto top of this function", but don't tell
			 * anybody... */

			if (req != NULL) {
				defer_open(lck, request_time, timeval_zero(),
					   req, &state);
			}
			TALLOC_FREE(lck);
			return status;
		}

		/*
		 * We exit this block with the share entry *locked*.....
		 */

	}

	SMB_ASSERT(lck != NULL);

	/* Delete streams if create_disposition requires it */
	if (file_existed && clear_ads &&
	    !is_ntfs_stream_smb_fname(smb_fname)) {
		status = delete_all_streams(conn, smb_fname->base_name);
		if (!NT_STATUS_IS_OK(status)) {
			TALLOC_FREE(lck);
			fd_close(fsp);
			return status;
		}
	}

	/* note that we ignore failure for the following. It is
           basically a hack for NFS, and NFS will never set one of
           these only read them. Nobody but Samba can ever set a deny
           mode and we have already checked our more authoritative
           locking database for permission to set this deny mode. If
           the kernel refuses the operations then the kernel is wrong.
	   note that GPFS supports it as well - jmcd */

	if (fsp->fh->fd != -1) {
		ret_flock = SMB_VFS_KERNEL_FLOCK(fsp, share_access, access_mask);
		if(ret_flock == -1 ){

			TALLOC_FREE(lck);
			fd_close(fsp);
			return NT_STATUS_SHARING_VIOLATION;
		}
	}

	/*
	 * At this point onwards, we can guarentee that the share entry
	 * is locked, whether we created the file or not, and that the
	 * deny mode is compatible with all current opens.
	 */

	/*
	 * According to Samba4, SEC_FILE_READ_ATTRIBUTE is always granted,
	 */
	fsp->access_mask = access_mask | FILE_READ_ATTRIBUTES;

	if (file_existed) {
		/* stat opens on existing files don't get oplocks. */
		if (is_stat_open(open_access_mask)) {
			fsp->oplock_type = NO_OPLOCK;
		}

		if (!(flags2 & O_TRUNC)) {
			info = FILE_WAS_OPENED;
		} else {
			info = FILE_WAS_OVERWRITTEN;
		}
	} else {
		info = FILE_WAS_CREATED;
	}

	if (pinfo) {
		*pinfo = info;
	}

	/*
	 * Setup the oplock info in both the shared memory and
	 * file structs.
	 */

	if ((fsp->oplock_type != NO_OPLOCK) &&
	    (fsp->oplock_type != FAKE_LEVEL_II_OPLOCK)) {
		if (!set_file_oplock(fsp, fsp->oplock_type)) {
			/* Could not get the kernel oplock */
			fsp->oplock_type = NO_OPLOCK;
		}
	}

	if (fsp->oplock_type == LEVEL_II_OPLOCK &&
	    (!lp_level2_oplocks(SNUM(conn)) ||
		!(global_client_caps & CAP_LEVEL_II_OPLOCKS))) {

		DEBUG(5, ("Downgrading level2 oplock on open "
			  "because level2 oplocks = off\n"));

		release_file_oplock(fsp);
	}

	if (info == FILE_WAS_OVERWRITTEN || info == FILE_WAS_CREATED ||
	    info == FILE_WAS_SUPERSEDED) {
		new_file_created = True;
	}

	set_share_mode(lck, fsp, get_current_uid(conn),
			req ? req->mid : 0,
		       fsp->oplock_type);

	/* Handle strange delete on close create semantics. */
	if (create_options & FILE_DELETE_ON_CLOSE) {
		status = can_set_delete_on_close(fsp, new_dos_attributes);

		if (!NT_STATUS_IS_OK(status)) {
			/* Remember to delete the mode we just added. */
			del_share_mode(lck, fsp);
			TALLOC_FREE(lck);
			fd_close(fsp);
			return status;
		}
		/* Note that here we set the *inital* delete on close flag,
		   not the regular one. The magic gets handled in close. */
		fsp->initial_delete_on_close = True;
	}

	/*
	 * Take care of inherited ACLs on created files - if default ACL not
	 * selected.
	 * May be necessary depending on acl policies.
	 */
	if (!posix_open && !file_existed && !def_acl &&
	    !(VALID_STAT(smb_fname->st) &&
		(smb_fname->st.st_ex_flags & SF_HASNTFSACL))) {

		int saved_errno = errno; /* We might get ENOSYS in the next
					  * call.. */

		if (SMB_VFS_FCHMOD_ACL(fsp, unx_mode) == -1 &&
		    errno == ENOSYS) {
			errno = saved_errno; /* Ignore ENOSYS */
		}

	} else if (new_unx_mode) {

		int ret = -1;

		/* Attributes need changing. File already existed. */

		{
			int saved_errno = errno; /* We might get ENOSYS in the
						  * next call.. */
			ret = SMB_VFS_FCHMOD_ACL(fsp, new_unx_mode);

			if (ret == -1 && errno == ENOSYS) {
				errno = saved_errno; /* Ignore ENOSYS */
			} else {
				DEBUG(5, ("onefs_open_file_ntcreate: reset "
					  "attributes of file %s to 0%o\n",
					  smb_fname_str_dbg(smb_fname),
					  (unsigned int)new_unx_mode));
				ret = 0; /* Don't do the fchmod below. */
			}
		}

		if ((ret == -1) &&
		    (SMB_VFS_FCHMOD(fsp, new_unx_mode) == -1))
			DEBUG(5, ("onefs_open_file_ntcreate: failed to reset "
				  "attributes of file %s to 0%o\n",
				  smb_fname_str_dbg(smb_fname),
				  (unsigned int)new_unx_mode));
	}

	/* If this is a successful open, we must remove any deferred open
	 * records. */
	if (req != NULL) {
		del_deferred_open_entry(lck, req->mid);
	}
	TALLOC_FREE(lck);

	return NT_STATUS_OK;
}


/****************************************************************************
 Open a directory from an NT SMB call.
****************************************************************************/
static NTSTATUS onefs_open_directory(connection_struct *conn,
				     struct smb_request *req,
				     struct smb_filename *smb_dname,
				     uint32 access_mask,
				     uint32 share_access,
				     uint32 create_disposition,
				     uint32 create_options,
				     uint32 file_attributes,
				     struct security_descriptor *sd,
				     files_struct **result,
				     int *pinfo)
{
	files_struct *fsp = NULL;
	struct share_mode_lock *lck = NULL;
	NTSTATUS status;
	struct timespec mtimespec;
	int info = 0;
	char *parent_dir;
	bool posix_open = false;
	uint32 create_flags = 0;
	uint32 mode = lp_dir_mask(SNUM(conn));

	DEBUG(5, ("onefs_open_directory: opening directory %s, "
		  "access_mask = 0x%x, "
		  "share_access = 0x%x create_options = 0x%x, "
		  "create_disposition = 0x%x, file_attributes = 0x%x\n",
		  smb_fname_str_dbg(smb_dname), (unsigned int)access_mask,
		  (unsigned int)share_access, (unsigned int)create_options,
		  (unsigned int)create_disposition,
		  (unsigned int)file_attributes));

	if (!(file_attributes & FILE_FLAG_POSIX_SEMANTICS) &&
	    (conn->fs_capabilities & FILE_NAMED_STREAMS) &&
	    is_ntfs_stream_smb_fname(smb_dname)) {
		DEBUG(2, ("onefs_open_directory: %s is a stream name!\n",
			  smb_fname_str_dbg(smb_dname)));
		return NT_STATUS_NOT_A_DIRECTORY;
	}

	switch (create_disposition) {
		case FILE_OPEN:
			/* If directory exists open. If directory doesn't
			 * exist error. */
			create_flags = 0;
			info = FILE_WAS_OPENED;
			break;
		case FILE_CREATE:
			/* If directory exists error. If directory doesn't
			 * exist create. */
			create_flags = O_CREAT | O_EXCL;
			info = FILE_WAS_CREATED;
			break;
		case FILE_OPEN_IF:
			/* If directory exists open. If directory doesn't
			 * exist create. */

			/* Note: in order to return whether the directory was
			 * opened or created, we first try to open and then try
			 * to create. */
			create_flags = 0;
			info = FILE_WAS_OPENED;
			break;
		case FILE_SUPERSEDE:
		case FILE_OVERWRITE:
		case FILE_OVERWRITE_IF:
		default:
			DEBUG(5, ("onefs_open_directory: invalid "
				  "create_disposition 0x%x for directory %s\n",
				  (unsigned int)create_disposition,
				  smb_fname_str_dbg(smb_dname)));
			return NT_STATUS_INVALID_PARAMETER;
	}

	/*
	 * Check for write access to the share. Done in mkdir_internal() in
	 * mainline samba.
	 */
	if (!CAN_WRITE(conn) && (create_flags & O_CREAT)) {
		return NT_STATUS_ACCESS_DENIED;
	}

	/* Get parent dirname */
	if (!parent_dirname(talloc_tos(), smb_dname->base_name, &parent_dir,
			    NULL)) {
		return NT_STATUS_NO_MEMORY;
	}

	if (file_attributes & FILE_FLAG_POSIX_SEMANTICS) {
		posix_open = true;
		mode = (mode_t)(file_attributes & ~FILE_FLAG_POSIX_SEMANTICS);
		file_attributes = 0;
	} else {
		mode = unix_mode(conn, FILE_ATTRIBUTE_DIRECTORY, smb_dname, parent_dir);
	}

	/*
	 * The NONINDEXED and COMPRESSED bits seem to always be cleared on
	 * directories, no matter if you specify that they should be set.
	 */
	file_attributes &=
	    ~(FILE_ATTRIBUTE_NONINDEXED | FILE_ATTRIBUTE_COMPRESSED);

	status = file_new(req, conn, &fsp);
	if(!NT_STATUS_IS_OK(status)) {
		return status;
	}

	/*
	 * Actual open with retry magic to handle FILE_OPEN_IF which is
	 * unique because the kernel won't tell us if the file was opened or
	 * created.
	 */
 retry_open:
	fsp->fh->fd = onefs_sys_create_file(conn,
					    -1,
					    smb_dname->base_name,
					    access_mask,
					    access_mask,
					    share_access,
					    create_options,
					    create_flags | O_DIRECTORY,
					    mode,
					    0,
					    0,
					    sd,
					    file_attributes,
					    NULL);

	if (fsp->fh->fd == -1) {
		DEBUG(3, ("Error opening %s. Errno=%d (%s).\n",
			  smb_fname_str_dbg(smb_dname), errno,
			  strerror(errno)));
		SMB_ASSERT(errno != EINPROGRESS);

		if (create_disposition == FILE_OPEN_IF) {
			if (errno == ENOENT) {
				/* Try again, creating it this time. */
				create_flags = O_CREAT | O_EXCL;
				info = FILE_WAS_CREATED;
				goto retry_open;
			} else if (errno == EEXIST) {
				/* Uggh. Try again again. */
				create_flags = 0;
				info = FILE_WAS_OPENED;
				goto retry_open;
			}
		}

		/* Error cases below: */
		file_free(req, fsp);

		if ((errno == ENOENT) && (create_disposition == FILE_OPEN)) {
			DEBUG(5, ("onefs_open_directory: FILE_OPEN requested "
				  "for directory %s and it doesn't "
				  "exist.\n", smb_fname_str_dbg(smb_dname)));
			return NT_STATUS_OBJECT_NAME_NOT_FOUND;
		} else if ((errno == EEXIST) &&
		    (create_disposition == FILE_CREATE)) {
			DEBUG(5, ("onefs_open_directory: FILE_CREATE "
				  "requested for directory %s and it "
				  "already exists.\n",
				  smb_fname_str_dbg(smb_dname)));
			return NT_STATUS_OBJECT_NAME_COLLISION;
		} else if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
			/* Catch sharing violations. */
			return NT_STATUS_SHARING_VIOLATION;
		}

		return map_nt_error_from_unix(errno);
	}

	if (info == FILE_WAS_CREATED) {

		/* Pulled from mkdir_internal() */
		if (SMB_VFS_LSTAT(conn, smb_dname) == -1) {
			DEBUG(2, ("Could not stat directory '%s' just "
				  "created: %s\n",
				  smb_fname_str_dbg(smb_dname),
				  strerror(errno)));
			return map_nt_error_from_unix(errno);
		}

		if (!S_ISDIR(smb_dname->st.st_ex_mode)) {
			DEBUG(0, ("Directory just '%s' created is not a "
				  "directory\n",
				  smb_fname_str_dbg(smb_dname)));
			return NT_STATUS_ACCESS_DENIED;
		}

		if (!posix_open) {
			/*
			 * Check if high bits should have been set, then (if
			 * bits are missing): add them.  Consider bits
			 * automagically set by UNIX, i.e. SGID bit from
			 * parent dir.
			 */
			if (mode & ~(S_IRWXU|S_IRWXG|S_IRWXO) &&
			    (mode & ~smb_dname->st.st_ex_mode)) {
				SMB_VFS_CHMOD(conn, smb_dname->base_name,
				    (smb_dname->st.st_ex_mode |
					(mode & ~smb_dname->st.st_ex_mode)));
			}
		}

		/* Change the owner if required. */
		if (lp_inherit_owner(SNUM(conn))) {
			change_dir_owner_to_parent(conn, parent_dir,
						   smb_dname->base_name,
						   &smb_dname->st);
		}

		notify_fname(conn, NOTIFY_ACTION_ADDED,
			     FILE_NOTIFY_CHANGE_DIR_NAME,
			     smb_dname->base_name);
	}

	/* Stat the fd for Samba bookkeeping. */
	if(SMB_VFS_FSTAT(fsp, &smb_dname->st) != 0) {
		fd_close(fsp);
		file_free(req, fsp);
		return map_nt_error_from_unix(errno);
	}

	/* Setup the files_struct for it. */
	fsp->mode = smb_dname->st.st_ex_mode;
	fsp->file_id = vfs_file_id_from_sbuf(conn, &smb_dname->st);
	fsp->vuid = req ? req->vuid : UID_FIELD_INVALID;
	fsp->file_pid = req ? req->smbpid : 0;
	fsp->can_lock = False;
	fsp->can_read = False;
	fsp->can_write = False;

	fsp->share_access = share_access;
	fsp->fh->private_options = 0;
	/*
	 * According to Samba4, SEC_FILE_READ_ATTRIBUTE is always granted,
	 */
	fsp->access_mask = access_mask | FILE_READ_ATTRIBUTES;
	fsp->print_file = NULL;
	fsp->modified = False;
	fsp->oplock_type = NO_OPLOCK;
	fsp->sent_oplock_break = NO_BREAK_SENT;
	fsp->is_directory = True;
	fsp->posix_open = posix_open;

	status = fsp_set_smb_fname(fsp, smb_dname);
	if (!NT_STATUS_IS_OK(status)) {
		fd_close(fsp);
		file_free(req, fsp);
		return status;
	}

	mtimespec = smb_dname->st.st_ex_mtime;

	/*
	 * Still set the samba share mode lock for correct delete-on-close
	 * semantics and to make smbstatus more useful.
	 */
	lck = get_share_mode_lock(talloc_tos(), fsp->file_id,
				  conn->connectpath, smb_dname, &mtimespec);

	if (lck == NULL) {
		DEBUG(0, ("onefs_open_directory: Could not get share mode "
			  "lock for %s\n", smb_fname_str_dbg(smb_dname)));
		fd_close(fsp);
		file_free(req, fsp);
		return NT_STATUS_SHARING_VIOLATION;
	}

	if (lck->delete_on_close) {
		TALLOC_FREE(lck);
		fd_close(fsp);
		file_free(req, fsp);
		return NT_STATUS_DELETE_PENDING;
	}

	set_share_mode(lck, fsp, get_current_uid(conn),
		req ? req->mid : 0, NO_OPLOCK);

	/*
	 * For directories the delete on close bit at open time seems
	 * always to be honored on close... See test 19 in Samba4 BASE-DELETE.
	 */
	if (create_options & FILE_DELETE_ON_CLOSE) {
		status = can_set_delete_on_close(fsp, 0);
		if (!NT_STATUS_IS_OK(status) &&
		    !NT_STATUS_EQUAL(status, NT_STATUS_DIRECTORY_NOT_EMPTY)) {
			TALLOC_FREE(lck);
			fd_close(fsp);
			file_free(req, fsp);
			return status;
		}

		if (NT_STATUS_IS_OK(status)) {
			/* Note that here we set the *inital* delete on close flag,
			   not the regular one. The magic gets handled in close. */
			fsp->initial_delete_on_close = True;
		}
	}

	TALLOC_FREE(lck);

	if (pinfo) {
		*pinfo = info;
	}

	*result = fsp;
	return NT_STATUS_OK;
}

/*
 * Wrapper around onefs_open_file_ntcreate and onefs_open_directory.
 */
static NTSTATUS onefs_create_file_unixpath(connection_struct *conn,
					   struct smb_request *req,
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
					   int *pinfo,
					   struct onefs_fsp_data *fsp_data)
{
	int info = FILE_WAS_OPENED;
	files_struct *base_fsp = NULL;
	files_struct *fsp = NULL;
	NTSTATUS status;

	DEBUG(10,("onefs_create_file_unixpath: access_mask = 0x%x "
		  "file_attributes = 0x%x, share_access = 0x%x, "
		  "create_disposition = 0x%x create_options = 0x%x "
		  "oplock_request = 0x%x private_flags = 0x%x "
		  "ea_list = 0x%p, sd = 0x%p, "
		  "fname = %s\n",
		  (unsigned int)access_mask,
		  (unsigned int)file_attributes,
		  (unsigned int)share_access,
		  (unsigned int)create_disposition,
		  (unsigned int)create_options,
		  (unsigned int)oplock_request,
		  (unsigned int)private_flags,
		  ea_list, sd, smb_fname_str_dbg(smb_fname)));

	if (create_options & FILE_OPEN_BY_FILE_ID) {
		status = NT_STATUS_NOT_SUPPORTED;
		goto fail;
	}

	if (create_options & NTCREATEX_OPTIONS_INVALID_PARAM_MASK) {
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}

	if (req == NULL) {
		SMB_ASSERT((oplock_request & ~SAMBA_PRIVATE_OPLOCK_MASK) ==
			    NO_OPLOCK);
		oplock_request |= INTERNAL_OPEN_ONLY;
	}

	if (lp_parm_bool(SNUM(conn), PARM_ONEFS_TYPE,
		PARM_IGNORE_SACLS, PARM_IGNORE_SACLS_DEFAULT)) {
		access_mask &= ~SYSTEM_SECURITY_ACCESS;
	}

	if ((conn->fs_capabilities & FILE_NAMED_STREAMS)
	    && (access_mask & DELETE_ACCESS)
	    && !is_ntfs_stream_smb_fname(smb_fname)) {
		/*
		 * We can't open a file with DELETE access if any of the
		 * streams is open without FILE_SHARE_DELETE
		 */
		status = open_streams_for_delete(conn, smb_fname->base_name);

		if (!NT_STATUS_IS_OK(status)) {
			goto fail;
		}
	}

	if ((conn->fs_capabilities & FILE_NAMED_STREAMS)
	    && is_ntfs_stream_smb_fname(smb_fname)) {
		uint32 base_create_disposition;
		struct smb_filename *smb_fname_base = NULL;

		if (create_options & FILE_DIRECTORY_FILE) {
			status = NT_STATUS_NOT_A_DIRECTORY;
			goto fail;
		}

		switch (create_disposition) {
		case FILE_OPEN:
			base_create_disposition = FILE_OPEN;
			break;
		default:
			base_create_disposition = FILE_OPEN_IF;
			break;
		}

		/* Create an smb_filename with stream_name == NULL. */
		status = create_synthetic_smb_fname(talloc_tos(),
						    smb_fname->base_name,
						    NULL, NULL,
						    &smb_fname_base);
		if (!NT_STATUS_IS_OK(status)) {
			goto fail;
		}

		if (SMB_VFS_STAT(conn, smb_fname_base) == -1) {
			DEBUG(10, ("Unable to stat stream: %s\n",
				   smb_fname_str_dbg(smb_fname_base)));
		}

		status = onefs_create_file_unixpath(
			conn,				/* conn */
			NULL,				/* req */
			smb_fname_base,			/* fname */
			SYNCHRONIZE_ACCESS,		/* access_mask */
			(FILE_SHARE_READ |
			    FILE_SHARE_WRITE |
			    FILE_SHARE_DELETE),		/* share_access */
			base_create_disposition,	/* create_disposition*/
			0,				/* create_options */
			file_attributes,		/* file_attributes */
			NO_OPLOCK,			/* oplock_request */
			0,				/* allocation_size */
			0,				/* private_flags */
			NULL,				/* sd */
			NULL,				/* ea_list */
			&base_fsp,			/* result */
			NULL,				/* pinfo */
			NULL);				/* fsp_data */

		TALLOC_FREE(smb_fname_base);

		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(10, ("onefs_create_file_unixpath for base %s "
				   "failed: %s\n", smb_fname->base_name,
				   nt_errstr(status)));
			goto fail;
		}

		/*
		 * Testing against windows xp/2003/vista shows that oplocks
		 * can actually be requested and granted on streams (see the
		 * RAW-OPLOCK-STREAM1 smbtorture test).
		 */
		if ((oplock_request & ~SAMBA_PRIVATE_OPLOCK_MASK) !=
		     NO_OPLOCK) {
			DEBUG(5, ("Oplock(%d) being requested on a stream! "
				  "Ignoring oplock request: fname=%s\n",
				  oplock_request & ~SAMBA_PRIVATE_OPLOCK_MASK,
				smb_fname_str_dbg(smb_fname)));
			/* Request NO_OPLOCK instead. */
			oplock_request &= SAMBA_PRIVATE_OPLOCK_MASK;
		}
	}

	/* Covert generic bits in the security descriptor. */
	if (sd != NULL) {
		security_acl_map_generic(sd->dacl, &file_generic_mapping);
		security_acl_map_generic(sd->sacl, &file_generic_mapping);
	}

	/*
	 * If it's a request for a directory open, deal with it separately.
	 */

	if (create_options & FILE_DIRECTORY_FILE) {

		if (create_options & FILE_NON_DIRECTORY_FILE) {
			status = NT_STATUS_INVALID_PARAMETER;
			goto fail;
		}

		/* Can't open a temp directory. IFS kit test. */
		if (!(file_attributes & FILE_FLAG_POSIX_SEMANTICS) &&
		     (file_attributes & FILE_ATTRIBUTE_TEMPORARY)) {
			status = NT_STATUS_INVALID_PARAMETER;
			goto fail;
		}

		/*
		 * We will get a create directory here if the Win32
		 * app specified a security descriptor in the
		 * CreateDirectory() call.
		 */

		status = onefs_open_directory(
			conn,				/* conn */
			req,				/* req */
			smb_fname,			/* fname */
			access_mask,			/* access_mask */
			share_access,			/* share_access */
			create_disposition,		/* create_disposition*/
			create_options,			/* create_options */
			file_attributes,		/* file_attributes */
			sd,				/* sd */
			&fsp,				/* result */
			&info);				/* pinfo */
	} else {

		/*
		 * Ordinary file case.
		 */

		status = file_new(req, conn, &fsp);
		if(!NT_STATUS_IS_OK(status)) {
			goto fail;
		}

		/*
		 * We're opening the stream element of a base_fsp
		 * we already opened. Set up the base_fsp pointer.
		 */
		if (base_fsp) {
			fsp->base_fsp = base_fsp;
		}

		status = onefs_open_file_ntcreate(
			conn,				/* conn */
			req,				/* req */
			smb_fname,			/* fname */
			access_mask,			/* access_mask */
			share_access,			/* share_access */
			create_disposition,		/* create_disposition*/
			create_options,			/* create_options */
			file_attributes,		/* file_attributes */
			oplock_request,			/* oplock_request */
			sd,				/* sd */
			fsp,				/* result */
			&info,				/* pinfo */
			fsp_data);			/* fsp_data */

		if(!NT_STATUS_IS_OK(status)) {
			file_free(req, fsp);
			fsp = NULL;
		}

		if (NT_STATUS_EQUAL(status, NT_STATUS_FILE_IS_A_DIRECTORY)) {

			/* A stream open never opens a directory */

			if (base_fsp) {
				status = NT_STATUS_FILE_IS_A_DIRECTORY;
				goto fail;
			}

			/*
			 * Fail the open if it was explicitly a non-directory
			 * file.
			 */

			if (create_options & FILE_NON_DIRECTORY_FILE) {
				status = NT_STATUS_FILE_IS_A_DIRECTORY;
				goto fail;
			}

			create_options |= FILE_DIRECTORY_FILE;

			status = onefs_open_directory(
				conn,			/* conn */
				req,			/* req */
				smb_fname,		/* fname */
				access_mask,		/* access_mask */
				share_access,		/* share_access */
				create_disposition,	/* create_disposition*/
				create_options,		/* create_options */
				file_attributes,	/* file_attributes */
				sd,			/* sd */
				&fsp,			/* result */
				&info);			/* pinfo */
		}
	}

	if (!NT_STATUS_IS_OK(status)) {
		goto fail;
	}

	fsp->base_fsp = base_fsp;

	SMB_ASSERT(fsp);

	if ((ea_list != NULL) && (info == FILE_WAS_CREATED)) {
		status = set_ea(conn, fsp, smb_fname, ea_list);
		if (!NT_STATUS_IS_OK(status)) {
			goto fail;
		}
	}

	if (!fsp->is_directory && S_ISDIR(smb_fname->st.st_ex_mode)) {
		status = NT_STATUS_ACCESS_DENIED;
		goto fail;
	}

	/* Save the requested allocation size. */
	if ((info == FILE_WAS_CREATED) || (info == FILE_WAS_OVERWRITTEN)) {
		if (allocation_size
		    && (allocation_size > smb_fname->st.st_ex_size)) {
			fsp->initial_allocation_size = smb_roundup(
				fsp->conn, allocation_size);
			if (fsp->is_directory) {
				/* Can't set allocation size on a directory. */
				status = NT_STATUS_ACCESS_DENIED;
				goto fail;
			}
			if (vfs_allocate_file_space(
				    fsp, fsp->initial_allocation_size) == -1) {
				status = NT_STATUS_DISK_FULL;
				goto fail;
			}
		} else {
			fsp->initial_allocation_size = smb_roundup(
				fsp->conn, (uint64_t)smb_fname->st.st_ex_size);
		}
	}

	DEBUG(10, ("onefs_create_file_unixpath: info=%d\n", info));

	*result = fsp;
	if (pinfo != NULL) {
		*pinfo = info;
	}
	if ((fsp->fh != NULL) && (fsp->fh->fd != -1)) {
		SMB_VFS_FSTAT(fsp, &smb_fname->st);
	}
	return NT_STATUS_OK;

 fail:
	DEBUG(10, ("onefs_create_file_unixpath: %s\n", nt_errstr(status)));

	if (fsp != NULL) {
		if (base_fsp && fsp->base_fsp == base_fsp) {
			/*
			 * The close_file below will close
			 * fsp->base_fsp.
			 */
			base_fsp = NULL;
		}
		close_file(req, fsp, ERROR_CLOSE);
		fsp = NULL;
	}
	if (base_fsp != NULL) {
		close_file(req, base_fsp, ERROR_CLOSE);
		base_fsp = NULL;
	}
	return status;
}

static void destroy_onefs_fsp_data(void *p_data)
{
	struct onefs_fsp_data *fsp_data = (struct onefs_fsp_data *)p_data;

	destroy_onefs_callback_record(fsp_data->oplock_callback_id);
}

/**
 * SMB_VFS_CREATE_FILE interface to onefs.
 */
NTSTATUS onefs_create_file(vfs_handle_struct *handle,
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
	connection_struct *conn = handle->conn;
	struct onefs_fsp_data fsp_data = {};
	int info = FILE_WAS_OPENED;
	files_struct *fsp = NULL;
	NTSTATUS status;

	DEBUG(10,("onefs_create_file: access_mask = 0x%x "
		  "file_attributes = 0x%x, share_access = 0x%x, "
		  "create_disposition = 0x%x create_options = 0x%x "
		  "oplock_request = 0x%x private_flags = 0x%x"
		  "root_dir_fid = 0x%x, ea_list = 0x%p, sd = 0x%p, "
		  "fname = %s\n",
		  (unsigned int)access_mask,
		  (unsigned int)file_attributes,
		  (unsigned int)share_access,
		  (unsigned int)create_disposition,
		  (unsigned int)create_options,
		  (unsigned int)oplock_request,
		  (unsigned int)private_flags,
		  (unsigned int)root_dir_fid,
		  ea_list, sd, smb_fname_str_dbg(smb_fname)));

	/* Get the file name if root_dir_fid was specified. */
	if (root_dir_fid != 0) {
		struct smb_filename *smb_fname_out = NULL;
		status = get_relative_fid_filename(conn, req, root_dir_fid,
						   smb_fname, &smb_fname_out);
		if (!NT_STATUS_IS_OK(status)) {
			goto fail;
		}
		smb_fname = smb_fname_out;
	}

	/* All file access must go through check_name() */
	status = check_name(conn, smb_fname->base_name);
	if (!NT_STATUS_IS_OK(status)) {
		goto fail;
	}

	if (is_ntfs_stream_smb_fname(smb_fname)) {
		if (!(conn->fs_capabilities & FILE_NAMED_STREAMS)) {
			status = NT_STATUS_OBJECT_NAME_NOT_FOUND;
			goto fail;
		}

		if (is_ntfs_default_stream_smb_fname(smb_fname)) {
			int ret;
			smb_fname->stream_name = NULL;
			/* We have to handle this error here. */
			if (create_options & FILE_DIRECTORY_FILE) {
				status = NT_STATUS_NOT_A_DIRECTORY;
				goto fail;
			}
			if (lp_posix_pathnames()) {
				ret = SMB_VFS_LSTAT(conn, smb_fname);
			} else {
				ret = SMB_VFS_STAT(conn, smb_fname);
			}

			if (ret == 0 && VALID_STAT_OF_DIR(smb_fname->st)) {
				status = NT_STATUS_FILE_IS_A_DIRECTORY;
				goto fail;
			}
		}
	}

	status = onefs_create_file_unixpath(
		conn,					/* conn */
		req,					/* req */
		smb_fname,				/* fname */
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
		&fsp,					/* result */
		&info,					/* pinfo */
		&fsp_data);				/* psbuf */

	if (!NT_STATUS_IS_OK(status)) {
		goto fail;
	}

	DEBUG(10, ("onefs_create_file: info=%d\n", info));

	/*
	 * Setup private onefs_fsp_data.  Currently the private data struct is
	 * only used to store the oplock_callback_id so that when the file is
	 * closed, the onefs_callback_record can be properly cleaned up in the
	 * oplock_onefs sub-system.
	 */
	if (fsp) {
		struct onefs_fsp_data *fsp_data_tmp = NULL;
		fsp_data_tmp = (struct onefs_fsp_data *)
		    VFS_ADD_FSP_EXTENSION(handle, fsp, struct onefs_fsp_data,
			&destroy_onefs_fsp_data);

		if (fsp_data_tmp == NULL) {
			status = NT_STATUS_NO_MEMORY;
			goto fail;
		}

		*fsp_data_tmp = fsp_data;
	}

	*result = fsp;
	if (pinfo != NULL) {
		*pinfo = info;
	}
	return NT_STATUS_OK;

 fail:
	DEBUG(10, ("onefs_create_file: %s\n", nt_errstr(status)));

	if (fsp != NULL) {
		close_file(req, fsp, ERROR_CLOSE);
		fsp = NULL;
	}
	return status;
}
