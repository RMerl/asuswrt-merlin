/* 
   Unix SMB/CIFS implementation.

   POSIX NTVFS backend - unlink

   Copyright (C) Andrew Tridgell 2004

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
#include "vfs_posix.h"
#include "system/dir.h"

/*
  retry an open after a sharing violation
*/
static void pvfs_retry_unlink(struct pvfs_odb_retry *r,
			      struct ntvfs_module_context *ntvfs,
			      struct ntvfs_request *req,
			      void *_io,
			      void *private_data,
			      enum pvfs_wait_notice reason)
{
	union smb_unlink *io = talloc_get_type(_io, union smb_unlink);
	NTSTATUS status = NT_STATUS_INTERNAL_ERROR;

	talloc_free(r);

	switch (reason) {
	case PVFS_WAIT_CANCEL:
/*TODO*/
		status = NT_STATUS_CANCELLED;
		break;
	case PVFS_WAIT_TIMEOUT:
		/* if it timed out, then give the failure
		   immediately */
/*TODO*/
		status = NT_STATUS_SHARING_VIOLATION;
		break;
	case PVFS_WAIT_EVENT:

		/* try the open again, which could trigger another retry setup
		   if it wants to, so we have to unmark the async flag so we
		   will know if it does a second async reply */
		req->async_states->state &= ~NTVFS_ASYNC_STATE_ASYNC;

		status = pvfs_unlink(ntvfs, req, io);
		if (req->async_states->state & NTVFS_ASYNC_STATE_ASYNC) {
			/* the 2nd try also replied async, so we don't send
			   the reply yet */
			return;
		}

		/* re-mark it async, just in case someone up the chain does
		   paranoid checking */
		req->async_states->state |= NTVFS_ASYNC_STATE_ASYNC;
		break;
	}

	/* send the reply up the chain */
	req->async_states->status = status;
	req->async_states->send_fn(req);
}

/*
  setup for a unlink retry after a sharing violation
  or a non granted oplock
*/
static NTSTATUS pvfs_unlink_setup_retry(struct ntvfs_module_context *ntvfs,
					struct ntvfs_request *req,
					union smb_unlink *io,
					struct odb_lock *lck,
					NTSTATUS status)
{
	struct pvfs_state *pvfs = talloc_get_type(ntvfs->private_data,
				  struct pvfs_state);
	struct timeval end_time;

	if (NT_STATUS_EQUAL(status, NT_STATUS_SHARING_VIOLATION)) {
		end_time = timeval_add(&req->statistics.request_time,
				       0, pvfs->sharing_violation_delay);
	} else if (NT_STATUS_EQUAL(status, NT_STATUS_OPLOCK_NOT_GRANTED)) {
		end_time = timeval_add(&req->statistics.request_time,
				       pvfs->oplock_break_timeout, 0);
	} else {
		return NT_STATUS_INTERNAL_ERROR;
	}

	return pvfs_odb_retry_setup(ntvfs, req, lck, end_time, io, NULL,
				    pvfs_retry_unlink);
}


/*
  unlink a file
*/
static NTSTATUS pvfs_unlink_file(struct pvfs_state *pvfs,
				 struct pvfs_filename *name)
{
	NTSTATUS status;

	if (name->dos.attrib & FILE_ATTRIBUTE_DIRECTORY) {
		return NT_STATUS_FILE_IS_A_DIRECTORY;
	}

	if (name->st.st_nlink == 1) {
		status = pvfs_xattr_unlink_hook(pvfs, name->full_name);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
	}

	/* finally try the actual unlink */
	if (unlink(name->full_name) == -1) {
		status = pvfs_map_errno(pvfs, errno);
	}

	if (NT_STATUS_IS_OK(status)) {
		notify_trigger(pvfs->notify_context, 
			       NOTIFY_ACTION_REMOVED, 
			       FILE_NOTIFY_CHANGE_FILE_NAME,
			       name->full_name);
	}

	return status;
}

/*
  unlink one file
*/
static NTSTATUS pvfs_unlink_one(struct pvfs_state *pvfs,
				struct ntvfs_request *req,
				union smb_unlink *unl,
				struct pvfs_filename *name)
{
	NTSTATUS status;
	struct odb_lock *lck = NULL;

	/* make sure its matches the given attributes */
	status = pvfs_match_attrib(pvfs, name,
				   unl->unlink.in.attrib, 0);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = pvfs_can_delete(pvfs, req, name, &lck);

	/*
	 * on a sharing violation we need to retry when the file is closed by
	 * the other user, or after 1 second
	 * on a non granted oplock we need to retry when the file is closed by
	 * the other user, or after 30 seconds
	 */
	if ((NT_STATUS_EQUAL(status, NT_STATUS_SHARING_VIOLATION) ||
	     NT_STATUS_EQUAL(status, NT_STATUS_OPLOCK_NOT_GRANTED)) &&
	    (req->async_states->state & NTVFS_ASYNC_STATE_MAY_ASYNC)) {
		return pvfs_unlink_setup_retry(pvfs->ntvfs, req, unl, lck, status);
	}

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (name->stream_name) {
		if (!name->stream_exists) {
			return NT_STATUS_OBJECT_NAME_NOT_FOUND;
		}

		return pvfs_stream_delete(pvfs, name, -1);
	}

	return pvfs_unlink_file(pvfs, name);
}

/*
  delete a file - the dirtype specifies the file types to include in the search. 
  The name can contain CIFS wildcards, but rarely does (except with OS/2 clients)
*/
NTSTATUS pvfs_unlink(struct ntvfs_module_context *ntvfs,
		     struct ntvfs_request *req,
		     union smb_unlink *unl)
{
	struct pvfs_state *pvfs = talloc_get_type(ntvfs->private_data,
				  struct pvfs_state);
	struct pvfs_dir *dir;
	NTSTATUS status;
	uint32_t total_deleted=0;
	struct pvfs_filename *name;
	const char *fname;
	off_t ofs;

	/* resolve the cifs name to a posix name */
	status = pvfs_resolve_name(pvfs, req, unl->unlink.in.pattern, 
				   PVFS_RESOLVE_WILDCARD |
				   PVFS_RESOLVE_STREAMS |
				   PVFS_RESOLVE_NO_OPENDB,
				   &name);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (!name->exists && !name->has_wildcard) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	if (name->exists && 
	    (name->dos.attrib & FILE_ATTRIBUTE_DIRECTORY)) {
		return NT_STATUS_FILE_IS_A_DIRECTORY;
	}

	if (!name->has_wildcard) {
		return pvfs_unlink_one(pvfs, req, unl, name);
	}

	/*
	 * disable async requests in the wildcard case
	 * untill we have proper tests for this
	 */
	req->async_states->state &= ~NTVFS_ASYNC_STATE_MAY_ASYNC;

	/* get list of matching files */
	status = pvfs_list_start(pvfs, name, req, &dir);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = NT_STATUS_NO_SUCH_FILE;
	talloc_free(name);

	ofs = 0;

	while ((fname = pvfs_list_next(dir, &ofs))) {
		/* this seems to be a special case */
		if ((unl->unlink.in.attrib & FILE_ATTRIBUTE_DIRECTORY) &&
		    (ISDOT(fname) || ISDOTDOT(fname))) {
			return NT_STATUS_OBJECT_NAME_INVALID;
		}

		/* get a pvfs_filename object */
		status = pvfs_resolve_partial(pvfs, req,
					      pvfs_list_unix_path(dir),
					      fname,
					      PVFS_RESOLVE_NO_OPENDB,
					      &name);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}

		status = pvfs_unlink_one(pvfs, req, unl, name);
		if (NT_STATUS_IS_OK(status)) {
			total_deleted++;
		}

		talloc_free(name);
	}

	if (total_deleted > 0) {
		status = NT_STATUS_OK;
	}

	return status;
}


