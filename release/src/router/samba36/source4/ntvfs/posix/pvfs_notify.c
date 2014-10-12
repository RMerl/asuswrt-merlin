/* 
   Unix SMB/CIFS implementation.

   POSIX NTVFS backend - notify

   Copyright (C) Andrew Tridgell 2006

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
#include "lib/messaging/irpc.h"
#include "messaging/messaging.h"
#include "../lib/util/dlinklist.h"
#include "lib/events/events.h"

/* pending notifies buffer, hung off struct pvfs_file for open directories
   that have used change notify */
struct pvfs_notify_buffer {
	struct pvfs_file *f;
	uint32_t num_changes;
	struct notify_changes *changes;
	uint32_t max_buffer_size;
	uint32_t current_buffer_size;
	bool overflowed;

	/* a list of requests waiting for events on this handle */
	struct notify_pending {
		struct notify_pending *next, *prev;
		struct ntvfs_request *req;
		union smb_notify *info;
	} *pending;
};

/*
  send a notify on the next event run. 
*/
static void pvfs_notify_send_next(struct tevent_context *ev, struct tevent_timer *te, 
				  struct timeval t, void *ptr)
{
	struct ntvfs_request *req = talloc_get_type(ptr, struct ntvfs_request);
	req->async_states->send_fn(req);
}


/*
  send a reply to a pending notify request
*/
static void pvfs_notify_send(struct pvfs_notify_buffer *notify_buffer, 
			     NTSTATUS status, bool immediate)
{
	struct notify_pending *pending = notify_buffer->pending;
	struct ntvfs_request *req;
	union smb_notify *info;

	if (notify_buffer->current_buffer_size > notify_buffer->max_buffer_size && 
	    notify_buffer->num_changes != 0) {
		/* on buffer overflow return no changes and destroys the notify buffer */
		notify_buffer->num_changes = 0;
		while (notify_buffer->pending) {
			pvfs_notify_send(notify_buffer, NT_STATUS_OK, immediate);
		}
		notify_buffer->overflowed = true;
		return;
	}

	/* see if there is anyone waiting */
	if (notify_buffer->pending == NULL) {
		return;
	}

	DLIST_REMOVE(notify_buffer->pending, pending);

	req = pending->req;
	info = pending->info;

	info->nttrans.out.num_changes = notify_buffer->num_changes;
	info->nttrans.out.changes = talloc_steal(req, notify_buffer->changes);
	notify_buffer->num_changes = 0;
	notify_buffer->overflowed = false;
	notify_buffer->changes = NULL;
	notify_buffer->current_buffer_size = 0;

	talloc_free(pending);

	if (info->nttrans.out.num_changes != 0) {
		status = NT_STATUS_OK;
	}

	req->async_states->status = status;

	if (immediate) {
		req->async_states->send_fn(req);
		return;
	} 

	/* we can't call pvfs_notify_send() directly here, as that
	   would free the request, and the ntvfs modules above us
	   could use it, so call it on the next event */
	event_add_timed(req->ctx->event_ctx, 
			req, timeval_zero(), pvfs_notify_send_next, req);
}

/*
  destroy a notify buffer. Called when the handle is closed
 */
static int pvfs_notify_destructor(struct pvfs_notify_buffer *n)
{
	notify_remove(n->f->pvfs->notify_context, n);
	n->f->notify_buffer = NULL;
	pvfs_notify_send(n, NT_STATUS_OK, true);
	return 0;
}


/*
  called when a async notify event comes in
*/
static void pvfs_notify_callback(void *private_data, const struct notify_event *ev)
{
	struct pvfs_notify_buffer *n = talloc_get_type(private_data, struct pvfs_notify_buffer);
	size_t len;
	struct notify_changes *n2;
	char *new_path;

	if (n->overflowed) {
		return;
	}

	n2 = talloc_realloc(n, n->changes, struct notify_changes, n->num_changes+1);
	if (n2 == NULL) {
		/* nothing much we can do for this */
		return;
	}
	n->changes = n2;

	new_path = talloc_strdup(n->changes, ev->path);
	if (new_path == NULL) {
		return;
	}
	string_replace(new_path, '/', '\\');

	n->changes[n->num_changes].action = ev->action;
	n->changes[n->num_changes].name.s = new_path;
	n->num_changes++;

	/*
	  work out how much room this will take in the buffer
	*/
	len = 12 + strlen_m(ev->path)*2;
	if (len & 3) {
		len += 4 - (len & 3);
	}
	n->current_buffer_size += len;

	/* send what we have, unless its the first part of a rename */
	if (ev->action != NOTIFY_ACTION_OLD_NAME) {
		pvfs_notify_send(n, NT_STATUS_OK, true);
	}
}

/*
  setup a notify buffer on a directory handle
*/
static NTSTATUS pvfs_notify_setup(struct pvfs_state *pvfs, struct pvfs_file *f, 
				  uint32_t buffer_size, uint32_t filter, bool recursive)
{
	NTSTATUS status;
	struct notify_entry e;
	
	/* We may not fill in all the elements in this entry -
	 * structure may in future be shared with Samba3 */
	ZERO_STRUCT(e);

	/* We may not fill in all the elements in this entry -
	 * structure may in future be shared with Samba3 */
	ZERO_STRUCT(e);

	f->notify_buffer = talloc_zero(f, struct pvfs_notify_buffer);
	NT_STATUS_HAVE_NO_MEMORY(f->notify_buffer);

	f->notify_buffer->max_buffer_size = buffer_size;
	f->notify_buffer->f = f;

	e.filter    = filter;
	e.path      = f->handle->name->full_name;
	if (recursive) {
		e.subdir_filter = filter;
	} else {
		e.subdir_filter = 0;
	}

	status = notify_add(pvfs->notify_context, &e, 
			    pvfs_notify_callback, f->notify_buffer);
	NT_STATUS_NOT_OK_RETURN(status);

	talloc_set_destructor(f->notify_buffer, pvfs_notify_destructor);

	return NT_STATUS_OK;
}

/*
  called from the pvfs_wait code when either an event has come in, or
  the notify request has been cancelled
*/
static void pvfs_notify_end(void *private_data, enum pvfs_wait_notice reason)
{
	struct pvfs_notify_buffer *notify_buffer = talloc_get_type(private_data,
								   struct pvfs_notify_buffer);
	if (reason == PVFS_WAIT_CANCEL) {
		pvfs_notify_send(notify_buffer, NT_STATUS_CANCELLED, false);
	} else {
		pvfs_notify_send(notify_buffer, NT_STATUS_OK, true);
	}
}

/* change notify request - always async. This request blocks until the
   event buffer is non-empty */
NTSTATUS pvfs_notify(struct ntvfs_module_context *ntvfs, 
		     struct ntvfs_request *req,
		     union smb_notify *info)
{
	struct pvfs_state *pvfs = talloc_get_type(ntvfs->private_data, 
						  struct pvfs_state);
	struct pvfs_file *f;
	NTSTATUS status;
	struct notify_pending *pending;

	if (info->nttrans.level != RAW_NOTIFY_NTTRANS) {
		return ntvfs_map_notify(ntvfs, req, info);
	}

	f = pvfs_find_fd(pvfs, req, info->nttrans.in.file.ntvfs);
	if (!f) {
		return NT_STATUS_INVALID_HANDLE;
	}

	/* this request doesn't make sense unless its async */
	if (!(req->async_states->state & NTVFS_ASYNC_STATE_MAY_ASYNC)) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* its only valid for directories */
	if (f->handle->fd != -1) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* if the handle doesn't currently have a notify buffer then
	   create one */
	if (f->notify_buffer == NULL) {
		status = pvfs_notify_setup(pvfs, f, 
					   info->nttrans.in.buffer_size, 
					   info->nttrans.in.completion_filter,
					   info->nttrans.in.recursive);
		NT_STATUS_NOT_OK_RETURN(status);
	}

	/* we update the max_buffer_size on each call, but we do not
	   update the recursive flag or filter */
	f->notify_buffer->max_buffer_size = info->nttrans.in.buffer_size;

	pending = talloc(f->notify_buffer, struct notify_pending);
	NT_STATUS_HAVE_NO_MEMORY(pending);

	pending->req = talloc_reference(pending, req);
	NT_STATUS_HAVE_NO_MEMORY(pending->req);	
	pending->info = info;

	DLIST_ADD_END(f->notify_buffer->pending, pending, struct notify_pending *);

	/* if the buffer is empty then start waiting */
	if (f->notify_buffer->num_changes == 0 && 
	    !f->notify_buffer->overflowed) {
		struct pvfs_wait *wait_handle;
		wait_handle = pvfs_wait_message(pvfs, req, -1,
						timeval_zero(),
						pvfs_notify_end,
						f->notify_buffer);
		NT_STATUS_HAVE_NO_MEMORY(wait_handle);
		talloc_steal(req, wait_handle);
		return NT_STATUS_OK;
	}

	req->async_states->state |= NTVFS_ASYNC_STATE_ASYNC;
	pvfs_notify_send(f->notify_buffer, NT_STATUS_OK, false);

	return NT_STATUS_OK;
}
