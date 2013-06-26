/* 
   Unix SMB/CIFS implementation.

   POSIX NTVFS backend - async request wait routines

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
#include "lib/events/events.h"
#include "../lib/util/dlinklist.h"
#include "vfs_posix.h"
#include "smbd/service_stream.h"
#include "lib/messaging/irpc.h"

/* the context for a single wait instance */
struct pvfs_wait {
	struct pvfs_wait *next, *prev;
	struct pvfs_state *pvfs;
	void (*handler)(void *, enum pvfs_wait_notice);
	void *private_data;
	int msg_type;
	struct messaging_context *msg_ctx;
	struct tevent_context *ev;
	struct ntvfs_request *req;
	enum pvfs_wait_notice reason;
};

/*
  called from the ntvfs layer when we have requested setup of an async
  call.  this ensures that async calls runs with the right state of
  previous ntvfs handlers in the chain (such as security context)
*/
NTSTATUS pvfs_async_setup(struct ntvfs_module_context *ntvfs,
			  struct ntvfs_request *req, void *private_data)
{
	struct pvfs_wait *pwait = talloc_get_type(private_data,
						  struct pvfs_wait);
	pwait->handler(pwait->private_data, pwait->reason);
	return NT_STATUS_OK;
}

/*
  receive a completion message for a wait
*/
static void pvfs_wait_dispatch(struct messaging_context *msg,
			       void *private_data, uint32_t msg_type,
			       struct server_id src, DATA_BLOB *data)
{
	struct pvfs_wait *pwait = talloc_get_type(private_data,
						  struct pvfs_wait);
	struct ntvfs_request *req;
	void *p = NULL;

	/* we need to check that this one is for us. See
	   messaging_send_ptr() for the other side of this.
	 */
	if (data->length == sizeof(void *)) {
		void **pp;
		pp = (void **)data->data;
		p = *pp;
	}
	if (p == NULL || p != pwait->private_data) {
		return;
	}

	pwait->reason = PVFS_WAIT_EVENT;

	/* the extra reference here is to ensure that the req
	   structure is not destroyed when the async request reply is
	   sent, which would cause problems with the other ntvfs
	   modules above us */
	req = talloc_reference(msg, pwait->req);
	ntvfs_async_setup(pwait->req, pwait);
	talloc_unlink(msg, req);
}


/*
  receive a timeout on a message wait
*/
static void pvfs_wait_timeout(struct tevent_context *ev, 
			      struct tevent_timer *te, struct timeval t,
			      void *private_data)
{
	struct pvfs_wait *pwait = talloc_get_type(private_data,
						  struct pvfs_wait);
	struct ntvfs_request *req = pwait->req;

	pwait->reason = PVFS_WAIT_TIMEOUT;

	req = talloc_reference(ev, req);
	if (req != NULL) {
		ntvfs_async_setup(req, pwait);
		talloc_unlink(ev, req);
	}
}


/*
  destroy a pending wait
 */
static int pvfs_wait_destructor(struct pvfs_wait *pwait)
{
	if (pwait->msg_type != -1) {
		messaging_deregister(pwait->msg_ctx, pwait->msg_type, pwait);
	}
	DLIST_REMOVE(pwait->pvfs->wait_list, pwait);
	return 0;
}

/*
  setup a request to wait on a message of type msg_type, with a
  timeout (given as an expiry time)

  the return value is a handle. To stop waiting talloc_free this
  handle.

  if msg_type == -1 then no message is registered, and it is assumed
  that the caller handles any messaging setup needed
*/
struct pvfs_wait *pvfs_wait_message(struct pvfs_state *pvfs,
				    struct ntvfs_request *req,
				    int msg_type,
				    struct timeval end_time,
				    void (*fn)(void *, enum pvfs_wait_notice),
				    void *private_data)
{
	struct pvfs_wait *pwait;

	pwait = talloc(pvfs, struct pvfs_wait);
	if (pwait == NULL) {
		return NULL;
	}

	pwait->private_data = private_data;
	pwait->handler = fn;
	pwait->msg_ctx = pvfs->ntvfs->ctx->msg_ctx;
	pwait->ev = pvfs->ntvfs->ctx->event_ctx;
	pwait->msg_type = msg_type;
	pwait->req = talloc_reference(pwait, req);
	pwait->pvfs = pvfs;

	if (!timeval_is_zero(&end_time)) {
		/* setup a timer */
		event_add_timed(pwait->ev, pwait, end_time, pvfs_wait_timeout, pwait);
	}

	/* register with the messaging subsystem for this message
	   type */
	if (msg_type != -1) {
		messaging_register(pwait->msg_ctx,
				   pwait,
				   msg_type,
				   pvfs_wait_dispatch);
	}

	/* tell the main smb server layer that we will be replying 
	   asynchronously */
	req->async_states->state |= NTVFS_ASYNC_STATE_ASYNC;

	DLIST_ADD(pvfs->wait_list, pwait);

	/* make sure we cleanup the timer and message handler */
	talloc_set_destructor(pwait, pvfs_wait_destructor);

	return pwait;
}


/*
  cancel an outstanding async request
*/
NTSTATUS pvfs_cancel(struct ntvfs_module_context *ntvfs, struct ntvfs_request *req)
{
	struct pvfs_state *pvfs = talloc_get_type(ntvfs->private_data,
				  struct pvfs_state);
	struct pvfs_wait *pwait;

	for (pwait=pvfs->wait_list;pwait;pwait=pwait->next) {
		if (pwait->req == req) {
			/* trigger a cancel on the request */
			pwait->reason = PVFS_WAIT_CANCEL;
			ntvfs_async_setup(pwait->req, pwait);
			return NT_STATUS_OK;
		}
	}

	return NT_STATUS_DOS(ERRDOS, ERRcancelviolation);
}
