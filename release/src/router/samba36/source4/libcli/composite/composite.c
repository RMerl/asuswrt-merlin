/* 
   Unix SMB/CIFS implementation.

   Copyright (C) Volker Lendecke 2005
   Copyright (C) Andrew Tridgell 2005
   
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
/*
  composite API helper functions
*/

#include "includes.h"
#include "lib/events/events.h"
#include "libcli/raw/libcliraw.h"
#include "libcli/smb2/smb2.h"
#include "libcli/composite/composite.h"
#include "../libcli/nbt/libnbt.h"

/*
 create a new composite_context structure
 and initialize it
*/
_PUBLIC_ struct composite_context *composite_create(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev)
{
	struct composite_context *c;

	c = talloc_zero(mem_ctx, struct composite_context);
	if (!c) return NULL;
	c->state = COMPOSITE_STATE_IN_PROGRESS;
	c->event_ctx = ev;

	return c;
}

/*
  block until a composite function has completed, then return the status
*/
_PUBLIC_ NTSTATUS composite_wait(struct composite_context *c)
{
	if (c == NULL) return NT_STATUS_NO_MEMORY;

	c->used_wait = true;

	while (c->state < COMPOSITE_STATE_DONE) {
		if (event_loop_once(c->event_ctx) != 0) {
			return NT_STATUS_UNSUCCESSFUL;
		}
	}

	return c->status;
}

/*
  block until a composite function has completed, then return the status. 
  Free the composite context before returning
*/
_PUBLIC_ NTSTATUS composite_wait_free(struct composite_context *c)
{
	NTSTATUS status = composite_wait(c);
	talloc_free(c);
	return status;
}

/* 
   callback from composite_done() and composite_error()

   this is used to allow for a composite function to complete without
   going through any state transitions. When that happens the caller
   has had no opportunity to fill in the async callback fields
   (ctx->async.fn and ctx->async.private_data) which means the usual way of
   dealing with composite functions doesn't work. To cope with this,
   we trigger a timer event that will happen then the event loop is
   re-entered. This gives the caller a chance to setup the callback,
   and allows the caller to ignore the fact that the composite
   function completed early
*/
static void composite_trigger(struct tevent_context *ev, struct tevent_timer *te,
			      struct timeval t, void *ptr)
{
	struct composite_context *c = talloc_get_type(ptr, struct composite_context);
	if (c->async.fn) {
		c->async.fn(c);
	}
}


_PUBLIC_ void composite_error(struct composite_context *ctx, NTSTATUS status)
{
	/* you are allowed to pass NT_STATUS_OK to composite_error(), in which
	   case it is equivalent to composite_done() */
	if (NT_STATUS_IS_OK(status)) {
		composite_done(ctx);
		return;
	}
	if (!ctx->used_wait && !ctx->async.fn) {
		event_add_timed(ctx->event_ctx, ctx, timeval_zero(), composite_trigger, ctx);
	}
	ctx->status = status;
	ctx->state = COMPOSITE_STATE_ERROR;
	if (ctx->async.fn != NULL) {
		ctx->async.fn(ctx);
	}
}

_PUBLIC_ bool composite_nomem(const void *p, struct composite_context *ctx)
{
	if (p != NULL) {
		return false;
	}
	composite_error(ctx, NT_STATUS_NO_MEMORY);
	return true;
}

_PUBLIC_ bool composite_is_ok(struct composite_context *ctx)
{
	if (NT_STATUS_IS_OK(ctx->status)) {
		return true;
	}
	composite_error(ctx, ctx->status);
	return false;
}

_PUBLIC_ void composite_done(struct composite_context *ctx)
{
	if (!ctx->used_wait && !ctx->async.fn) {
		event_add_timed(ctx->event_ctx, ctx, timeval_zero(), composite_trigger, ctx);
	}
	ctx->state = COMPOSITE_STATE_DONE;
	if (ctx->async.fn != NULL) {
		ctx->async.fn(ctx);
	}
}

_PUBLIC_ void composite_continue(struct composite_context *ctx,
				 struct composite_context *new_ctx,
				 void (*continuation)(struct composite_context *),
				 void *private_data)
{
	if (composite_nomem(new_ctx, ctx)) return;
	new_ctx->async.fn = continuation;
	new_ctx->async.private_data = private_data;

	/* if we are setting up a continuation, and the context has
	   already finished, then we should run the callback with an
	   immediate event, otherwise we can be stuck forever */
	if (new_ctx->state >= COMPOSITE_STATE_DONE && continuation) {
		event_add_timed(new_ctx->event_ctx, new_ctx, timeval_zero(), composite_trigger, new_ctx);
	}
}

_PUBLIC_ void composite_continue_smb(struct composite_context *ctx,
				     struct smbcli_request *new_req,
				     void (*continuation)(struct smbcli_request *),
				     void *private_data)
{
	if (composite_nomem(new_req, ctx)) return;
	new_req->async.fn = continuation;
	new_req->async.private_data = private_data;
}

_PUBLIC_ void composite_continue_smb2(struct composite_context *ctx,
				      struct smb2_request *new_req,
				      void (*continuation)(struct smb2_request *),
				      void *private_data)
{
	if (composite_nomem(new_req, ctx)) return;
	new_req->async.fn = continuation;
	new_req->async.private_data = private_data;
}

_PUBLIC_ void composite_continue_nbt(struct composite_context *ctx,
				     struct nbt_name_request *new_req,
				     void (*continuation)(struct nbt_name_request *),
				     void *private_data)
{
	if (composite_nomem(new_req, ctx)) return;
	new_req->async.fn = continuation;
	new_req->async.private_data = private_data;
}
