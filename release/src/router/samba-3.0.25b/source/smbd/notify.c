/*
   Unix SMB/CIFS implementation.
   change notify handling
   Copyright (C) Andrew Tridgell 2000
   Copyright (C) Jeremy Allison 1994-1998
   Copyright (C) Volker Lendecke 2007

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

struct notify_change_request {
	struct notify_change_request *prev, *next;
	struct files_struct *fsp;	/* backpointer for cancel by mid */
	char request_buf[smb_size];
	uint32 filter;
	uint32 current_bufsize;
	struct notify_mid_map *mid_map;
	void *backend_data;
};

static void notify_fsp(files_struct *fsp, uint32 action, const char *name);

static struct notify_mid_map *notify_changes_by_mid;

/*
 * For NTCancel, we need to find the notify_change_request indexed by
 * mid. Separate list here.
 */

struct notify_mid_map {
	struct notify_mid_map *prev, *next;
	struct notify_change_request *req;
	uint16 mid;
};

static BOOL notify_marshall_changes(int num_changes,
				    struct notify_change *changes,
				    prs_struct *ps)
{
	int i;
	UNISTR uni_name;

	for (i=0; i<num_changes; i++) {
		struct notify_change *c = &changes[i];
		size_t namelen;
		uint32 u32_tmp;	/* Temp arg to prs_uint32 to avoid
				 * signed/unsigned issues */

		namelen = convert_string_allocate(
			NULL, CH_UNIX, CH_UTF16LE, c->name, strlen(c->name)+1,
			&uni_name.buffer, True);
		if ((namelen == -1) || (uni_name.buffer == NULL)) {
			goto fail;
		}

		namelen -= 2;	/* Dump NULL termination */

		/*
		 * Offset to next entry, only if there is one
		 */

		u32_tmp = (i == num_changes-1) ? 0 : namelen + 12;
		if (!prs_uint32("offset", ps, 1, &u32_tmp)) goto fail;

		u32_tmp = c->action;
		if (!prs_uint32("action", ps, 1, &u32_tmp)) goto fail;

		u32_tmp = namelen;
		if (!prs_uint32("namelen", ps, 1, &u32_tmp)) goto fail;

		if (!prs_unistr("name", ps, 1, &uni_name)) goto fail;

		/*
		 * Not NULL terminated, decrease by the 2 UCS2 \0 chars
		 */
		prs_set_offset(ps, prs_offset(ps)-2);

		SAFE_FREE(uni_name.buffer);
	}

	return True;

 fail:
	SAFE_FREE(uni_name.buffer);
	return False;
}

/****************************************************************************
 Setup the common parts of the return packet and send it.
*****************************************************************************/

static void change_notify_reply_packet(const char *request_buf,
				       NTSTATUS error_code)
{
	char outbuf[smb_size+38];

	memset(outbuf, '\0', sizeof(outbuf));
	construct_reply_common(request_buf, outbuf);

	ERROR_NT(error_code);

	/*
	 * Seems NT needs a transact command with an error code
	 * in it. This is a longer packet than a simple error.
	 */
	set_message(outbuf,18,0,False);

	show_msg(outbuf);
	if (!send_smb(smbd_server_fd(),outbuf))
		exit_server_cleanly("change_notify_reply_packet: send_smb "
				    "failed.");
}

void change_notify_reply(const char *request_buf,
			 struct notify_change_buf *notify_buf)
{
	char *outbuf = NULL;
	prs_struct ps;
	size_t buflen;

	if (notify_buf->num_changes == -1) {
		change_notify_reply_packet(request_buf, NT_STATUS_OK);
		return;
	}

	if (!prs_init(&ps, 0, NULL, False)
	    || !notify_marshall_changes(notify_buf->num_changes,
					notify_buf->changes, &ps)) {
		change_notify_reply_packet(request_buf, NT_STATUS_NO_MEMORY);
		goto done;
	}

	buflen = smb_size+38+prs_offset(&ps) + 4 /* padding */;

	if (!(outbuf = SMB_MALLOC_ARRAY(char, buflen))) {
		change_notify_reply_packet(request_buf, NT_STATUS_NO_MEMORY);
		goto done;
	}

	construct_reply_common(request_buf, outbuf);

	if (send_nt_replies(outbuf, buflen, NT_STATUS_OK, prs_data_p(&ps),
			    prs_offset(&ps), NULL, 0) == -1) {
		exit_server("change_notify_reply_packet: send_smb failed.");
	}

 done:
	SAFE_FREE(outbuf);
	prs_mem_free(&ps);

	TALLOC_FREE(notify_buf->changes);
	notify_buf->num_changes = 0;
}

static void notify_callback(void *private_data, const struct notify_event *e)
{
	files_struct *fsp = (files_struct *)private_data;
	DEBUG(10, ("notify_callback called for %s\n", fsp->fsp_name));
	notify_fsp(fsp, e->action, e->path);
}

NTSTATUS change_notify_create(struct files_struct *fsp, uint32 filter,
			      BOOL recursive)
{
	char *fullpath;
	struct notify_entry e;
	NTSTATUS status;

	SMB_ASSERT(fsp->notify == NULL);

	if (!(fsp->notify = TALLOC_ZERO_P(NULL, struct notify_change_buf))) {
		DEBUG(0, ("talloc failed\n"));
		return NT_STATUS_NO_MEMORY;
	}

	if (asprintf(&fullpath, "%s/%s", fsp->conn->connectpath,
		     fsp->fsp_name) == -1) {
		DEBUG(0, ("asprintf failed\n"));
		return NT_STATUS_NO_MEMORY;
	}

	e.path = fullpath;
	e.filter = filter;
	e.subdir_filter = 0;
	if (recursive) {
		e.subdir_filter = filter;
	}

	status = notify_add(fsp->conn->notify_ctx, &e, notify_callback, fsp);
	SAFE_FREE(fullpath);

	return status;
}

NTSTATUS change_notify_add_request(const char *inbuf, 
				   uint32 filter, BOOL recursive,
				   struct files_struct *fsp)
{
	struct notify_change_request *request = NULL;
	struct notify_mid_map *map = NULL;

	if (!(request = SMB_MALLOC_P(struct notify_change_request))
	    || !(map = SMB_MALLOC_P(struct notify_mid_map))) {
		SAFE_FREE(request);
		return NT_STATUS_NO_MEMORY;
	}

	request->mid_map = map;
	map->req = request;

	memcpy(request->request_buf, inbuf, sizeof(request->request_buf));
	request->current_bufsize = 0;
	request->filter = filter;
	request->fsp = fsp;
	request->backend_data = NULL;
	
	DLIST_ADD_END(fsp->notify->requests, request,
		      struct notify_change_request *);

	map->mid = SVAL(inbuf, smb_mid);
	DLIST_ADD(notify_changes_by_mid, map);

	/* Push the MID of this packet on the signing queue. */
	srv_defer_sign_response(SVAL(inbuf,smb_mid));

	return NT_STATUS_OK;
}

static void change_notify_remove_request(struct notify_change_request *remove_req)
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
		smb_panic("notify_req not found in fsp's requests\n");
	}

	DLIST_REMOVE(fsp->notify->requests, req);
	DLIST_REMOVE(notify_changes_by_mid, req->mid_map);
	SAFE_FREE(req->mid_map);
	TALLOC_FREE(req->backend_data);
	SAFE_FREE(req);
}

/****************************************************************************
 Delete entries by mid from the change notify pending queue. Always send reply.
*****************************************************************************/

void remove_pending_change_notify_requests_by_mid(uint16 mid)
{
	struct notify_mid_map *map;

	for (map = notify_changes_by_mid; map; map = map->next) {
		if (map->mid == mid) {
			break;
		}
	}

	if (map == NULL) {
		return;
	}

	change_notify_reply_packet(map->req->request_buf, NT_STATUS_CANCELLED);
	change_notify_remove_request(map->req);
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
		change_notify_reply_packet(
			fsp->notify->requests->request_buf, status);
		change_notify_remove_request(fsp->notify->requests);
	}
}

void notify_fname(connection_struct *conn, uint32 action, uint32 filter,
		  const char *path)
{
	char *fullpath;

	if (asprintf(&fullpath, "%s/%s", conn->connectpath, path) == -1) {
		DEBUG(0, ("asprintf failed\n"));
		return;
	}

	notify_trigger(conn->notify_ctx, action, filter, fullpath);
	SAFE_FREE(fullpath);
}

static void notify_fsp(files_struct *fsp, uint32 action, const char *name)
{
	struct notify_change *change, *changes;
	pstring name2;

	if (fsp->notify == NULL) {
		/*
		 * Nobody is waiting, don't queue
		 */
		return;
	}

	pstrcpy(name2, name);
	string_replace(name2, '/', '\\');

	/*
	 * Someone has triggered a notify previously, queue the change for
	 * later.
	 */

	if ((fsp->notify->num_changes > 1000) || (name == NULL)) {
		/*
		 * The real number depends on the client buf, just provide a
		 * guard against a DoS here.
		 */
		TALLOC_FREE(fsp->notify->changes);
		fsp->notify->num_changes = -1;
		return;
	}

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

	if (!(change->name = talloc_strdup(changes, name2))) {
		DEBUG(0, ("talloc_strdup failed\n"));
		return;
	}

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

	change_notify_reply(fsp->notify->requests->request_buf,
			    fsp->notify);

	change_notify_remove_request(fsp->notify->requests);
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

