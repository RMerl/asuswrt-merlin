/* 
   Unix SMB/CIFS implementation.

   POSIX NTVFS backend - rename

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
#include "librpc/gen_ndr/security.h"
#include "param/param.h"


/*
  do a file rename, and send any notify triggers
*/
NTSTATUS pvfs_do_rename(struct pvfs_state *pvfs,
			struct odb_lock *lck,
			const struct pvfs_filename *name1,
			const char *name2)
{
	const char *r1, *r2;
	uint32_t mask;
	NTSTATUS status;

	if (pvfs_sys_rename(pvfs, name1->full_name, name2) == -1) {
		return pvfs_map_errno(pvfs, errno);
	}

	status = odb_rename(lck, name2);
	NT_STATUS_NOT_OK_RETURN(status);

	if (name1->dos.attrib & FILE_ATTRIBUTE_DIRECTORY) {
		mask = FILE_NOTIFY_CHANGE_DIR_NAME;
	} else {
		mask = FILE_NOTIFY_CHANGE_FILE_NAME;
	}
	/* 
	   renames to the same directory cause a OLD_NAME->NEW_NAME notify.
	   renames to a different directory are considered a remove/add 
	*/
	r1 = strrchr_m(name1->full_name, '/');
	r2 = strrchr_m(name2, '/');

	if ((r1-name1->full_name) != (r2-name2) ||
	    strncmp(name1->full_name, name2, r1-name1->full_name) != 0) {
		notify_trigger(pvfs->notify_context, 
			       NOTIFY_ACTION_REMOVED, 
			       mask,
			       name1->full_name);
		notify_trigger(pvfs->notify_context, 
			       NOTIFY_ACTION_ADDED, 
			       mask,
			       name2);
	} else {
		notify_trigger(pvfs->notify_context, 
			       NOTIFY_ACTION_OLD_NAME, 
			       mask,
			       name1->full_name);
		notify_trigger(pvfs->notify_context, 
			       NOTIFY_ACTION_NEW_NAME, 
			       mask,
			       name2);
	}

	/* this is a strange one. w2k3 gives an additional event for CHANGE_ATTRIBUTES
	   and CHANGE_CREATION on the new file when renaming files, but not 
	   directories */
	if ((name1->dos.attrib & FILE_ATTRIBUTE_DIRECTORY) == 0) {
		notify_trigger(pvfs->notify_context, 
			       NOTIFY_ACTION_MODIFIED, 
			       FILE_NOTIFY_CHANGE_ATTRIBUTES|FILE_NOTIFY_CHANGE_CREATION,
			       name2);
	}
	
	return NT_STATUS_OK;
}


/*
  resolve a wildcard rename pattern. This works on one component of the name
*/
static const char *pvfs_resolve_wildcard_component(TALLOC_CTX *mem_ctx, 
						   const char *fname, 
						   const char *pattern)
{
	const char *p1, *p2;
	char *dest, *d;

	/* the length is bounded by the length of the two strings combined */
	dest = talloc_array(mem_ctx, char, strlen(fname) + strlen(pattern) + 1);
	if (dest == NULL) {
		return NULL;
	}

	p1 = fname;
	p2 = pattern;
	d = dest;

	while (*p2) {
		codepoint_t c1, c2;
		size_t c_size1, c_size2;
		c1 = next_codepoint(p1, &c_size1);
		c2 = next_codepoint(p2, &c_size2);
		if (c2 == '?') {
			d += push_codepoint(d, c1);
		} else if (c2 == '*') {
			memcpy(d, p1, strlen(p1));
			d += strlen(p1);
			break;
		} else {
			d += push_codepoint(d, c2);
		}

		p1 += c_size1;
		p2 += c_size2;
	}

	*d = 0;

	talloc_set_name_const(dest, dest);

	return dest;
}

/*
  resolve a wildcard rename pattern.
*/
static const char *pvfs_resolve_wildcard(TALLOC_CTX *mem_ctx, 
					 const char *fname, 
					 const char *pattern)
{
	const char *base1, *base2;
	const char *ext1, *ext2;
	char *p;

	/* break into base part plus extension */
	p = strrchr_m(fname, '.');
	if (p == NULL) {
		ext1 = "";
		base1 = fname;
	} else {
		ext1 = talloc_strdup(mem_ctx, p+1);
		base1 = talloc_strndup(mem_ctx, fname, p-fname);
	}
	if (ext1 == NULL || base1 == NULL) {
		return NULL;
	}

	p = strrchr_m(pattern, '.');
	if (p == NULL) {
		ext2 = "";
		base2 = fname;
	} else {
		ext2 = talloc_strdup(mem_ctx, p+1);
		base2 = talloc_strndup(mem_ctx, pattern, p-pattern);
	}
	if (ext2 == NULL || base2 == NULL) {
		return NULL;
	}

	base1 = pvfs_resolve_wildcard_component(mem_ctx, base1, base2);
	ext1 = pvfs_resolve_wildcard_component(mem_ctx, ext1, ext2);
	if (base1 == NULL || ext1 == NULL) {
		return NULL;
	}

	if (*ext1 == 0) {
		return base1;
	}

	return talloc_asprintf(mem_ctx, "%s.%s", base1, ext1);
}

/*
  retry an rename after a sharing violation
*/
static void pvfs_retry_rename(struct pvfs_odb_retry *r,
			      struct ntvfs_module_context *ntvfs,
			      struct ntvfs_request *req,
			      void *_io,
			      void *private_data,
			      enum pvfs_wait_notice reason)
{
	union smb_rename *io = talloc_get_type(_io, union smb_rename);
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

		status = pvfs_rename(ntvfs, req, io);
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
  setup for a rename retry after a sharing violation
  or a non granted oplock
*/
static NTSTATUS pvfs_rename_setup_retry(struct ntvfs_module_context *ntvfs,
					struct ntvfs_request *req,
					union smb_rename *io,
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
				    pvfs_retry_rename);
}

/*
  rename one file from a wildcard set
*/
static NTSTATUS pvfs_rename_one(struct pvfs_state *pvfs, 
				struct ntvfs_request *req, 
				const char *dir_path,
				const char *fname1,
				const char *fname2,
				uint16_t attrib)
{
	struct pvfs_filename *name1, *name2;
	TALLOC_CTX *mem_ctx = talloc_new(req);
	struct odb_lock *lck = NULL;
	NTSTATUS status;

	/* resolve the wildcard pattern for this name */
	fname2 = pvfs_resolve_wildcard(mem_ctx, fname1, fname2);
	if (fname2 == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	/* get a pvfs_filename source object */
	status = pvfs_resolve_partial(pvfs, mem_ctx, 
				      dir_path, fname1,
				      PVFS_RESOLVE_NO_OPENDB,
				      &name1);
	if (!NT_STATUS_IS_OK(status)) {
		goto failed;
	}

	/* make sure its matches the given attributes */
	status = pvfs_match_attrib(pvfs, name1, attrib, 0);
	if (!NT_STATUS_IS_OK(status)) {
		goto failed;
	}

	status = pvfs_can_rename(pvfs, req, name1, &lck);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(lck);
		goto failed;
	}

	/* get a pvfs_filename dest object */
	status = pvfs_resolve_partial(pvfs, mem_ctx, 
				      dir_path, fname2,
				      PVFS_RESOLVE_NO_OPENDB,
				      &name2);
	if (NT_STATUS_IS_OK(status)) {
		status = pvfs_can_delete(pvfs, req, name2, NULL);
		if (!NT_STATUS_IS_OK(status)) {
			goto failed;
		}
	}

	status = NT_STATUS_OK;

	fname2 = talloc_asprintf(mem_ctx, "%s/%s", dir_path, fname2);
	if (fname2 == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	status = pvfs_do_rename(pvfs, lck, name1, fname2);

failed:
	talloc_free(mem_ctx);
	return status;
}


/*
  rename a set of files with wildcards
*/
static NTSTATUS pvfs_rename_wildcard(struct pvfs_state *pvfs, 
				     struct ntvfs_request *req, 
				     union smb_rename *ren, 
				     struct pvfs_filename *name1, 
				     struct pvfs_filename *name2)
{
	struct pvfs_dir *dir;
	NTSTATUS status;
	off_t ofs = 0;
	const char *fname, *fname2, *dir_path;
	uint16_t attrib = ren->rename.in.attrib;
	int total_renamed = 0;

	/* get list of matching files */
	status = pvfs_list_start(pvfs, name1, req, &dir);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = NT_STATUS_NO_SUCH_FILE;

	dir_path = pvfs_list_unix_path(dir);

	/* only allow wildcard renames within a directory */
	if (strncmp(dir_path, name2->full_name, strlen(dir_path)) != 0 ||
	    name2->full_name[strlen(dir_path)] != '/' ||
	    strchr(name2->full_name + strlen(dir_path) + 1, '/')) {
		DEBUG(3,(__location__ ": Invalid rename for %s -> %s\n",
			 name1->original_name, name2->original_name));
		return NT_STATUS_INVALID_PARAMETER;
	}

	fname2 = talloc_strdup(name2, name2->full_name + strlen(dir_path) + 1);
	if (fname2 == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	while ((fname = pvfs_list_next(dir, &ofs))) {
		status = pvfs_rename_one(pvfs, req, 
					 dir_path,
					 fname, fname2, attrib);
		if (NT_STATUS_IS_OK(status)) {
			total_renamed++;
		}
	}

	if (total_renamed == 0) {
		return status;
	}

	return NT_STATUS_OK;
}

/*
  rename a set of files - SMBmv interface
*/
static NTSTATUS pvfs_rename_mv(struct ntvfs_module_context *ntvfs,
			       struct ntvfs_request *req, union smb_rename *ren)
{
	struct pvfs_state *pvfs = talloc_get_type(ntvfs->private_data,
				  struct pvfs_state);
	NTSTATUS status;
	struct pvfs_filename *name1, *name2;
	struct odb_lock *lck = NULL;

	/* resolve the cifs name to a posix name */
	status = pvfs_resolve_name(pvfs, req, ren->rename.in.pattern1, 
				   PVFS_RESOLVE_WILDCARD, &name1);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = pvfs_resolve_name(pvfs, req, ren->rename.in.pattern2, 
				   PVFS_RESOLVE_WILDCARD, &name2);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (name1->has_wildcard || name2->has_wildcard) {
		return pvfs_rename_wildcard(pvfs, req, ren, name1, name2);
	}

	if (!name1->exists) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	if (strcmp(name1->full_name, name2->full_name) == 0) {
		return NT_STATUS_OK;
	}

	if (name2->exists) {
		return NT_STATUS_OBJECT_NAME_COLLISION;
	}

	status = pvfs_match_attrib(pvfs, name1, ren->rename.in.attrib, 0);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = pvfs_access_check_parent(pvfs, req, name2, SEC_DIR_ADD_FILE);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = pvfs_can_rename(pvfs, req, name1, &lck);
	/*
	 * on a sharing violation we need to retry when the file is closed by
	 * the other user, or after 1 second
	 * on a non granted oplock we need to retry when the file is closed by
	 * the other user, or after 30 seconds
	 */
	if ((NT_STATUS_EQUAL(status, NT_STATUS_SHARING_VIOLATION) ||
	     NT_STATUS_EQUAL(status, NT_STATUS_OPLOCK_NOT_GRANTED)) &&
	    (req->async_states->state & NTVFS_ASYNC_STATE_MAY_ASYNC)) {
		return pvfs_rename_setup_retry(pvfs->ntvfs, req, ren, lck, status);
	}

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = pvfs_do_rename(pvfs, lck, name1, name2->full_name);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	
	return NT_STATUS_OK;
}


/*
  rename a stream
*/
static NTSTATUS pvfs_rename_stream(struct ntvfs_module_context *ntvfs,
				   struct ntvfs_request *req, union smb_rename *ren,
				   struct pvfs_filename *name1)
{
	struct pvfs_state *pvfs = talloc_get_type(ntvfs->private_data,
				  struct pvfs_state);
	NTSTATUS status;
	struct odb_lock *lck = NULL;

	if (name1->has_wildcard) {
		DEBUG(3,(__location__ ": Invalid wildcard rename for %s\n",
			 name1->original_name));
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (ren->ntrename.in.new_name[0] != ':') {
		DEBUG(3,(__location__ ": Invalid rename for %s\n",
			 ren->ntrename.in.new_name));
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (!name1->exists) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	if (ren->ntrename.in.flags != RENAME_FLAG_RENAME) {
		DEBUG(3,(__location__ ": Invalid rename flags 0x%x for %s\n",
			 ren->ntrename.in.flags, ren->ntrename.in.new_name));
		return NT_STATUS_INVALID_PARAMETER;
	}

	status = pvfs_can_rename(pvfs, req, name1, &lck);
	/*
	 * on a sharing violation we need to retry when the file is closed by
	 * the other user, or after 1 second
	 * on a non granted oplock we need to retry when the file is closed by
	 * the other user, or after 30 seconds
	 */
	if ((NT_STATUS_EQUAL(status, NT_STATUS_SHARING_VIOLATION) ||
	     NT_STATUS_EQUAL(status, NT_STATUS_OPLOCK_NOT_GRANTED)) &&
	    (req->async_states->state & NTVFS_ASYNC_STATE_MAY_ASYNC)) {
		return pvfs_rename_setup_retry(pvfs->ntvfs, req, ren, lck, status);
	}
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = pvfs_access_check_simple(pvfs, req, name1, SEC_FILE_WRITE_ATTRIBUTE);
	NT_STATUS_NOT_OK_RETURN(status);

	status = pvfs_stream_rename(pvfs, name1, -1, 
				    ren->ntrename.in.new_name+1, 
				    true);
	NT_STATUS_NOT_OK_RETURN(status);
	
	return NT_STATUS_OK;
}

/*
  rename a set of files - ntrename interface
*/
static NTSTATUS pvfs_rename_nt(struct ntvfs_module_context *ntvfs,
			       struct ntvfs_request *req, union smb_rename *ren)
{
	struct pvfs_state *pvfs = talloc_get_type(ntvfs->private_data,
				  struct pvfs_state);
	NTSTATUS status;
	struct pvfs_filename *name1, *name2;
	struct odb_lock *lck = NULL;

	switch (ren->ntrename.in.flags) {
	case RENAME_FLAG_RENAME:
	case RENAME_FLAG_HARD_LINK:
	case RENAME_FLAG_COPY:
	case RENAME_FLAG_MOVE_CLUSTER_INFORMATION:
		break;
	default:
		return NT_STATUS_ACCESS_DENIED;
	}

	/* resolve the cifs name to a posix name */
	status = pvfs_resolve_name(pvfs, req, ren->ntrename.in.old_name, 
				   PVFS_RESOLVE_WILDCARD | PVFS_RESOLVE_STREAMS, &name1);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (name1->stream_name) {
		/* stream renames need to be handled separately */
		return pvfs_rename_stream(ntvfs, req, ren, name1);
	}

	status = pvfs_resolve_name(pvfs, req, ren->ntrename.in.new_name, 
				   PVFS_RESOLVE_WILDCARD, &name2);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (name1->has_wildcard || name2->has_wildcard) {
		return NT_STATUS_OBJECT_PATH_SYNTAX_BAD;
	}

	if (!name1->exists) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	if (strcmp(name1->full_name, name2->full_name) == 0) {
		return NT_STATUS_OK;
	}

	if (name2->exists) {
		return NT_STATUS_OBJECT_NAME_COLLISION;
	}

	status = pvfs_match_attrib(pvfs, name1, ren->ntrename.in.attrib, 0);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = pvfs_can_rename(pvfs, req, name1, &lck);
	/*
	 * on a sharing violation we need to retry when the file is closed by
	 * the other user, or after 1 second
	 * on a non granted oplock we need to retry when the file is closed by
	 * the other user, or after 30 seconds
	 */
	if ((NT_STATUS_EQUAL(status, NT_STATUS_SHARING_VIOLATION) ||
	     NT_STATUS_EQUAL(status, NT_STATUS_OPLOCK_NOT_GRANTED)) &&
	    (req->async_states->state & NTVFS_ASYNC_STATE_MAY_ASYNC)) {
		return pvfs_rename_setup_retry(pvfs->ntvfs, req, ren, lck, status);
	}
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	switch (ren->ntrename.in.flags) {
	case RENAME_FLAG_RENAME:
		status = pvfs_access_check_parent(pvfs, req, name2, SEC_DIR_ADD_FILE);
		NT_STATUS_NOT_OK_RETURN(status);
		status = pvfs_do_rename(pvfs, lck, name1, name2->full_name);
		NT_STATUS_NOT_OK_RETURN(status);
		break;

	case RENAME_FLAG_HARD_LINK:
		status = pvfs_access_check_parent(pvfs, req, name2, SEC_DIR_ADD_FILE);
		NT_STATUS_NOT_OK_RETURN(status);
		if (link(name1->full_name, name2->full_name) == -1) {
			return pvfs_map_errno(pvfs, errno);
		}
		break;

	case RENAME_FLAG_COPY:
		status = pvfs_access_check_parent(pvfs, req, name2, SEC_DIR_ADD_FILE);
		NT_STATUS_NOT_OK_RETURN(status);
		return pvfs_copy_file(pvfs, name1, name2);

	case RENAME_FLAG_MOVE_CLUSTER_INFORMATION:
		DEBUG(3,(__location__ ": Invalid rename cluster for %s\n",
			 name1->original_name));
		return NT_STATUS_INVALID_PARAMETER;

	default:
		return NT_STATUS_ACCESS_DENIED;
	}

	
	return NT_STATUS_OK;
}

/*
  rename a set of files - ntrename interface
*/
NTSTATUS pvfs_rename(struct ntvfs_module_context *ntvfs,
		     struct ntvfs_request *req, union smb_rename *ren)
{
	struct pvfs_state *pvfs = talloc_get_type(ntvfs->private_data,
				  struct pvfs_state);
	struct pvfs_file *f;

	switch (ren->generic.level) {
	case RAW_RENAME_RENAME:
		return pvfs_rename_mv(ntvfs, req, ren);

	case RAW_RENAME_NTRENAME:
		return pvfs_rename_nt(ntvfs, req, ren);

	case RAW_RENAME_NTTRANS:
		f = pvfs_find_fd(pvfs, req, ren->nttrans.in.file.ntvfs);
		if (!f) {
			return NT_STATUS_INVALID_HANDLE;
		}

		/* wk23 ignores the request */
		return NT_STATUS_OK;

	default:
		break;
	}

	return NT_STATUS_INVALID_LEVEL;
}

