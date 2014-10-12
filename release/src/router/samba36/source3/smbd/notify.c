/*
   Unix SMB/CIFS implementation.
   change notify handling
   Copyright (C) Andrew Tridgell 2000
   Copyright (C) Jeremy Allison 1994-1998
   Copyright (C) Volker Lendecke 2007

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
#include "smbd/smbd.h"
#include "smbd/globals.h"
#include "../librpc/gen_ndr/ndr_notify.h"

struct notify_change_request {
	struct notify_change_request *prev, *next;
	struct files_struct *fsp;	/* backpointer for cancel by mid */
	struct smb_request *req;
	uint32 filter;
	uint32 max_param;
	void (*reply_fn)(struct smb_request *req,
			 NTSTATUS error_code,
			 uint8_t *buf, size_t len);
	struct notify_mid_map *mid_map;
	void *backend_data;
};

static void notify_fsp(files_struct *fsp, uint32 action, const char *name);

/*
 * For NTCancel, we need to find the notify_change_request indexed by
 * mid. Separate list here.
 */

struct notify_mid_map {
	struct notify_mid_map *prev, *next;
	struct notify_change_request *req;
	uint64_t mid;
};

static bool notify_change_record_identical(struct notify_change *c1,
					struct notify_change *c2)
{
	/* Note this is deliberately case sensitive. */
	if (c1->action == c2->action &&
			strcmp(c1->name, c2->name) == 0) {
		return True;
	}
	return False;
}

static bool notify_marshall_changes(int num_changes,
				uint32 max_offset,
				struct notify_change *changes,
				DATA_BLOB *final_blob)
{
	int i;

	if (num_changes == -1) {
		return false;
	}

	for (i=0; i<num_changes; i++) {
		enum ndr_err_code ndr_err;
		struct notify_change *c;
		struct FILE_NOTIFY_INFORMATION m;
		DATA_BLOB blob;

		/* Coalesce any identical records. */
		while (i+1 < num_changes &&
			notify_change_record_identical(&changes[i],
						&changes[i+1])) {
			i++;
		}

		c = &changes[i];

		m.FileName1 = c->name;
		m.FileNameLength = strlen_m(c->name)*2;
		m.Action = c->action;
		m.NextEntryOffset = (i == num_changes-1) ? 0 : ndr_size_FILE_NOTIFY_INFORMATION(&m, 0);

		/*
		 * Offset to next entry, only if there is one
		 */

		ndr_err = ndr_push_struct_blob(&blob, talloc_tos(), &m,
			(ndr_push_flags_fn_t)ndr_push_FILE_NOTIFY_INFORMATION);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			return false;
		}

		if (DEBUGLEVEL >= 10) {
			NDR_PRINT_DEBUG(FILE_NOTIFY_INFORMATION, &m);
		}

		if (!data_blob_append(talloc_tos(), final_blob,
				      blob.data, blob.length)) {
			data_blob_free(&blob);
			return false;
		}

		data_blob_free(&blob);

		if (final_blob->length > max_offset) {
			/* Too much data for client. */
			DEBUG(10, ("Client only wanted %d bytes, trying to "
				   "marshall %d bytes\n", (int)max_offset,
				   (int)final_blob->length));
			return False;
		}
	}

	return True;
}

/****************************************************************************
 Setup the common parts of the return packet and send it.
*****************************************************************************/

void change_notify_reply(struct smb_request *req,
			 NTSTATUS error_code,
			 uint32_t max_param,
			 struct notify_change_buf *notify_buf,
			 void (*reply_fn)(struct smb_request *req,
					  NTSTATUS error_code,
					  uint8_t *buf, size_t len))
{
	DATA_BLOB blob = data_blob_null;

	if (!NT_STATUS_IS_OK(error_code)) {
		reply_fn(req, error_code, NULL, 0);
		return;
	}

	if (max_param == 0 || notify_buf == NULL) {
		reply_fn(req, NT_STATUS_OK, NULL, 0);
		return;
	}

	if (!notify_marshall_changes(notify_buf->num_changes, max_param,
					notify_buf->changes, &blob)) {
		/*
		 * We exceed what the client is willing to accept. Send
		 * nothing.
		 */
		data_blob_free(&blob);
	}

	reply_fn(req, NT_STATUS_OK, blob.data, blob.length);

	data_blob_free(&blob);

	TALLOC_FREE(notify_buf->changes);
	notify_buf->num_changes = 0;
}

static void notify_callback(void *private_data, const struct notify_event *e)
{
	files_struct *fsp = (files_struct *)private_data;
	DEBUG(10, ("notify_callback called for %s\n", fsp_str_dbg(fsp)));
	notify_fsp(fsp, e->action, e->path);
}

NTSTATUS change_notify_create(struct files_struct *fsp, uint32 filter,
			      bool recursive)
{
	char *fullpath;
	struct notify_entry e;
	NTSTATUS status;

	SMB_ASSERT(fsp->notify == NULL);

	if (!(fsp->notify = TALLOC_ZERO_P(NULL, struct notify_change_buf))) {
		DEBUG(0, ("talloc failed\n"));
		return NT_STATUS_NO_MEMORY;
	}

	/* Do notify operations on the base_name. */
	if (asprintf(&fullpath, "%s/%s", fsp->conn->connectpath,
		     fsp->fsp_name->base_name) == -1) {
		DEBUG(0, ("asprintf failed\n"));
		TALLOC_FREE(fsp->notify);
		return NT_STATUS_NO_MEMORY;
	}

	ZERO_STRUCT(e);
	e.path = fullpath;
	e.dir_fd = fsp->fh->fd;
	e.dir_id = fsp->file_id;
	e.filter = filter;
	e.subdir_filter = 0;
	if (recursive) {
		e.subdir_filter = filter;
	}

	status = notify_add(fsp->conn->notify_ctx, &e, notify_callback, fsp);
	SAFE_FREE(fullpath);

	return status;
}

NTSTATUS change_notify_add_request(struct smb_request *req,
				uint32 max_param,
				uint32 filter, bool recursive,
				struct files_struct *fsp,
				void (*reply_fn)(struct smb_request *req,
					NTSTATUS error_code,
					uint8_t *buf, size_t len))
{
	struct notify_change_request *request = NULL;
	struct notify_mid_map *map = NULL;
	struct smbd_server_connection *sconn = req->sconn;

	DEBUG(10, ("change_notify_add_request: Adding request for %s: "
		   "max_param = %d\n", fsp_str_dbg(fsp), (int)max_param));

	if (!(request = talloc(NULL, struct notify_change_request))
	    || !(map = talloc(request, struct notify_mid_map))) {
		TALLOC_FREE(request);
		return NT_STATUS_NO_MEMORY;
	}

	request->mid_map = map;
	map->req = request;

	request->req = talloc_move(request, &req);
	request->max_param = max_param;
	request->filter = filter;
	request->fsp = fsp;
	request->reply_fn = reply_fn;
	request->backend_data = NULL;

	DLIST_ADD_END(fsp->notify->requests, request,
		      struct notify_change_request *);

	map->mid = request->req->mid;
	DLIST_ADD(sconn->smb1.notify_mid_maps, map);

	return NT_STATUS_OK;
}

static void change_notify_remove_request(struct smbd_server_connection *sconn,
					 struct notify_change_request *remove_req)
{
	files_struct *fsp;
	struct notify_change_request *req;

	/*
	 * Paranoia checks, the fsp referenced must must have the request in
	 * its list of pending requests
	 */

	fsp = remove_req->fsp;
	SMB_ASSERT(fsp->notify != NULL);

	for (req = fsp->notify->requests; req; req = req->next) {
		if (req == remove_req) {
			break;
		}
	}

	if (req == NULL) {
		smb_panic("notify_req not found in fsp's requests");
	}

	DLIST_REMOVE(fsp->notify->requests, req);
	DLIST_REMOVE(sconn->smb1.notify_mid_maps, req->mid_map);
	TALLOC_FREE(req);
}

/****************************************************************************
 Delete entries by mid from the change notify pending queue. Always send reply.
*****************************************************************************/

void remove_pending_change_notify_requests_by_mid(
	struct smbd_server_connection *sconn, uint64_t mid)
{
	struct notify_mid_map *map;

	for (map = sconn->smb1.notify_mid_maps; map; map = map->next) {
		if (map->mid == mid) {
			break;
		}
	}

	if (map == NULL) {
		return;
	}

	change_notify_reply(map->req->req,
			    NT_STATUS_CANCELLED, 0, NULL, map->req->reply_fn);
	change_notify_remove_request(sconn, map->req);
}

void smbd_notify_cancel_by_smbreq(const struct smb_request *smbreq)
{
	struct smbd_server_connection *sconn = smbreq->sconn;
	struct notify_mid_map *map;

	for (map = sconn->smb1.notify_mid_maps; map; map = map->next) {
		if (map->req->req == smbreq) {
			break;
		}
	}

	if (map == NULL) {
		return;
	}

	change_notify_reply(map->req->req,
			    NT_STATUS_CANCELLED, 0, NULL, map->req->reply_fn);
	change_notify_remove_request(sconn, map->req);
}

/****************************************************************************
 Delete entries by fnum from the change notify pending queue.
*****************************************************************************/

void remove_pending_change_notify_requests_by_fid(files_struct *fsp,
						  NTSTATUS status)
{
	if (fsp->notify == NULL) {
		return;
	}

	while (fsp->notify->requests != NULL) {
		change_notify_reply(fsp->notify->requests->req,
				    status, 0, NULL,
				    fsp->notify->requests->reply_fn);
		change_notify_remove_request(fsp->conn->sconn,
					     fsp->notify->requests);
	}
}

static void notify_parent_dir(connection_struct *conn,
			      uint32 action, uint32 filter,
			      const char *path)
{
	struct smb_filename smb_fname_parent;
	char *parent;
	const char *name;
	char *oldwd;

	if (!parent_dirname(talloc_tos(), path, &parent, &name)) {
		return;
	}

	ZERO_STRUCT(smb_fname_parent);
	smb_fname_parent.base_name = parent;

	oldwd = vfs_GetWd(parent, conn);
	if (oldwd == NULL) {
		goto done;
	}
	if (vfs_ChDir(conn, conn->connectpath) == -1) {
		goto done;
	}

	if (SMB_VFS_STAT(conn, &smb_fname_parent) == -1) {
		goto chdir_done;
	}

	notify_onelevel(conn->notify_ctx, action, filter,
			SMB_VFS_FILE_ID_CREATE(conn, &smb_fname_parent.st),
			name);
chdir_done:
	vfs_ChDir(conn, oldwd);
done:
	TALLOC_FREE(parent);
}

void notify_fname(connection_struct *conn, uint32 action, uint32 filter,
		  const char *path)
{
	char *fullpath;

	if (path[0] == '.' && path[1] == '/') {
		path += 2;
	}
	notify_parent_dir(conn, action, filter, path);

	fullpath = talloc_asprintf(talloc_tos(), "%s/%s", conn->connectpath,
				   path);
	if (fullpath == NULL) {
		DEBUG(0, ("asprintf failed\n"));
		return;
	}
	notify_trigger(conn->notify_ctx, action, filter, fullpath);
	TALLOC_FREE(fullpath);
}

static void notify_fsp(files_struct *fsp, uint32 action, const char *name)
{
	struct notify_change *change, *changes;
	char *tmp;

	if (fsp->notify == NULL) {
		/*
		 * Nobody is waiting, don't queue
		 */
		return;
	}

	/*
	 * Someone has triggered a notify previously, queue the change for
	 * later.
	 */

	if ((fsp->notify->num_changes > 1000) || (name == NULL)) {
		/*
		 * The real number depends on the client buf, just provide a
		 * guard against a DoS here.  If name == NULL the CN backend is
		 * alerting us to a problem.  Possibly dropped events.  Clear
		 * queued changes and send the catch-all response to the client
		 * if a request is pending.
		 */
		TALLOC_FREE(fsp->notify->changes);
		fsp->notify->num_changes = -1;
		if (fsp->notify->requests != NULL) {
			change_notify_reply(fsp->notify->requests->req,
					    NT_STATUS_OK,
					    fsp->notify->requests->max_param,
					    fsp->notify,
					    fsp->notify->requests->reply_fn);
			change_notify_remove_request(fsp->conn->sconn,
						     fsp->notify->requests);
		}
		return;
	}

	/* If we've exceeded the server side queue or received a NULL name
	 * from the underlying CN implementation, don't queue up any more
	 * requests until we can send a catch-all response to the client */
	if (fsp->notify->num_changes == -1) {
		return;
	}

	if (!(changes = TALLOC_REALLOC_ARRAY(
		      fsp->notify, fsp->notify->changes,
		      struct notify_change, fsp->notify->num_changes+1))) {
		DEBUG(0, ("talloc_realloc failed\n"));
		return;
	}

	fsp->notify->changes = changes;

	change = &(fsp->notify->changes[fsp->notify->num_changes]);

	if (!(tmp = talloc_strdup(changes, name))) {
		DEBUG(0, ("talloc_strdup failed\n"));
		return;
	}

	string_replace(tmp, '/', '\\');
	change->name = tmp;	

	change->action = action;
	fsp->notify->num_changes += 1;

	if (fsp->notify->requests == NULL) {
		/*
		 * Nobody is waiting, so don't send anything. The ot
		 */
		return;
	}

	if (action == NOTIFY_ACTION_OLD_NAME) {
		/*
		 * We have to send the two rename events in one reply. So hold
		 * the first part back.
		 */
		return;
	}

	/*
	 * Someone is waiting for the change, trigger the reply immediately.
	 *
	 * TODO: do we have to walk the lists of requests pending?
	 */

	change_notify_reply(fsp->notify->requests->req,
			    NT_STATUS_OK,
			    fsp->notify->requests->max_param,
			    fsp->notify,
			    fsp->notify->requests->reply_fn);

	change_notify_remove_request(fsp->conn->sconn, fsp->notify->requests);
}

char *notify_filter_string(TALLOC_CTX *mem_ctx, uint32 filter)
{
	char *result = NULL;

	result = talloc_strdup(mem_ctx, "");

	if (filter & FILE_NOTIFY_CHANGE_FILE_NAME)
		result = talloc_asprintf_append(result, "FILE_NAME|");
	if (filter & FILE_NOTIFY_CHANGE_DIR_NAME)
		result = talloc_asprintf_append(result, "DIR_NAME|");
	if (filter & FILE_NOTIFY_CHANGE_ATTRIBUTES)
		result = talloc_asprintf_append(result, "ATTRIBUTES|");
	if (filter & FILE_NOTIFY_CHANGE_SIZE)
		result = talloc_asprintf_append(result, "SIZE|");
	if (filter & FILE_NOTIFY_CHANGE_LAST_WRITE)
		result = talloc_asprintf_append(result, "LAST_WRITE|");
	if (filter & FILE_NOTIFY_CHANGE_LAST_ACCESS)
		result = talloc_asprintf_append(result, "LAST_ACCESS|");
	if (filter & FILE_NOTIFY_CHANGE_CREATION)
		result = talloc_asprintf_append(result, "CREATION|");
	if (filter & FILE_NOTIFY_CHANGE_EA)
		result = talloc_asprintf_append(result, "EA|");
	if (filter & FILE_NOTIFY_CHANGE_SECURITY)
		result = talloc_asprintf_append(result, "SECURITY|");
	if (filter & FILE_NOTIFY_CHANGE_STREAM_NAME)
		result = talloc_asprintf_append(result, "STREAM_NAME|");
	if (filter & FILE_NOTIFY_CHANGE_STREAM_SIZE)
		result = talloc_asprintf_append(result, "STREAM_SIZE|");
	if (filter & FILE_NOTIFY_CHANGE_STREAM_WRITE)
		result = talloc_asprintf_append(result, "STREAM_WRITE|");

	if (result == NULL) return NULL;
	if (*result == '\0') return result;

	result[strlen(result)-1] = '\0';
	return result;
}

struct sys_notify_context *sys_notify_context_create(connection_struct *conn,
						     TALLOC_CTX *mem_ctx, 
						     struct event_context *ev)
{
	struct sys_notify_context *ctx;

	if (!(ctx = TALLOC_P(mem_ctx, struct sys_notify_context))) {
		DEBUG(0, ("talloc failed\n"));
		return NULL;
	}

	ctx->ev = ev;
	ctx->conn = conn;
	ctx->private_data = NULL;
	return ctx;
}

NTSTATUS sys_notify_watch(struct sys_notify_context *ctx,
			  struct notify_entry *e,
			  void (*callback)(struct sys_notify_context *ctx, 
					   void *private_data,
					   struct notify_event *ev),
			  void *private_data, void *handle)
{
	return SMB_VFS_NOTIFY_WATCH(ctx->conn, ctx, e, callback, private_data,
				    handle);
}

