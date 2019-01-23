/*
   Unix SMB/CIFS implementation.

   Winbind client library.

   Copyright (C) 2008 Kai Blin  <kai@samba.org>

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
#include <tevent.h>
#include "libcli/wbclient/wbclient.h"

/**
 * Initialize the wbclient context, talloc_free() when done.
 *
 * \param mem_ctx talloc context to allocate memory from
 * \param msg_ctx message context to use
 * \param
 */
struct wbc_context *wbc_init(TALLOC_CTX *mem_ctx,
			     struct messaging_context *msg_ctx,
			     struct tevent_context *event_ctx)
{
	struct wbc_context *ctx;

	ctx = talloc(mem_ctx, struct wbc_context);
	if (ctx == NULL) return NULL;

	ctx->event_ctx = event_ctx;

	ctx->irpc_handle = irpc_binding_handle_by_name(ctx, msg_ctx,
						       "winbind_server",
						       &ndr_table_winbind);
	if (ctx->irpc_handle == NULL) {
		talloc_free(ctx);
		return NULL;
	}

	return ctx;
}

struct wbc_idmap_state {
	struct composite_context *ctx;
	struct winbind_get_idmap *req;
	struct id_map *ids;
};

static void sids_to_xids_recv_ids(struct tevent_req *subreq);

struct composite_context *wbc_sids_to_xids_send(struct wbc_context *wbc_ctx,
						TALLOC_CTX *mem_ctx,
						uint32_t count,
						struct id_map *ids)
{
	struct composite_context *ctx;
	struct wbc_idmap_state *state;
	struct tevent_req *subreq;

	DEBUG(5, ("wbc_sids_to_xids called\n"));

	ctx = composite_create(mem_ctx, wbc_ctx->event_ctx);
	if (ctx == NULL) return NULL;

	state = talloc(ctx, struct wbc_idmap_state);
	if (composite_nomem(state, ctx)) return ctx;
	ctx->private_data = state;

	state->req = talloc(state, struct winbind_get_idmap);
	if (composite_nomem(state->req, ctx)) return ctx;

	state->req->in.count = count;
	state->req->in.level = WINBIND_IDMAP_LEVEL_SIDS_TO_XIDS;
	state->req->in.ids = ids;
	state->ctx = ctx;

	subreq = dcerpc_winbind_get_idmap_r_send(state,
						 wbc_ctx->event_ctx,
						 wbc_ctx->irpc_handle,
						 state->req);
	if (composite_nomem(subreq, ctx)) return ctx;

	tevent_req_set_callback(subreq, sids_to_xids_recv_ids, state);

	return ctx;
}

static void sids_to_xids_recv_ids(struct tevent_req *subreq)
{
	struct wbc_idmap_state *state =
		tevent_req_callback_data(subreq,
		struct wbc_idmap_state);

	state->ctx->status = dcerpc_winbind_get_idmap_r_recv(subreq, state);
	TALLOC_FREE(subreq);
	if (!composite_is_ok(state->ctx)) return;

	state->ids = state->req->out.ids;
	composite_done(state->ctx);
}

NTSTATUS wbc_sids_to_xids_recv(struct composite_context *ctx,
			       struct id_map **ids)
{
	NTSTATUS status = composite_wait(ctx);
		DEBUG(5, ("wbc_sids_to_xids_recv called\n"));
	if (NT_STATUS_IS_OK(status)) {
		struct wbc_idmap_state *state =	talloc_get_type_abort(
							ctx->private_data,
							struct wbc_idmap_state);
		*ids = state->ids;
	}

	return status;
}

static void xids_to_sids_recv_ids(struct tevent_req *subreq);

struct composite_context *wbc_xids_to_sids_send(struct wbc_context *wbc_ctx,
						TALLOC_CTX *mem_ctx,
						uint32_t count,
						struct id_map *ids)
{
	struct composite_context *ctx;
	struct wbc_idmap_state *state;
	struct tevent_req *subreq;

	DEBUG(5, ("wbc_xids_to_sids called\n"));

	ctx = composite_create(mem_ctx, wbc_ctx->event_ctx);
	if (ctx == NULL) return NULL;

	state = talloc(ctx, struct wbc_idmap_state);
	if (composite_nomem(state, ctx)) return ctx;
	ctx->private_data = state;

	state->req = talloc(state, struct winbind_get_idmap);
	if (composite_nomem(state->req, ctx)) return ctx;

	state->req->in.count = count;
	state->req->in.level = WINBIND_IDMAP_LEVEL_XIDS_TO_SIDS;
	state->req->in.ids = ids;
	state->ctx = ctx;

	subreq = dcerpc_winbind_get_idmap_r_send(state,
						 wbc_ctx->event_ctx,
						 wbc_ctx->irpc_handle,
						 state->req);
	if (composite_nomem(subreq, ctx)) return ctx;

	tevent_req_set_callback(subreq, xids_to_sids_recv_ids, state);

	return ctx;
}

static void xids_to_sids_recv_ids(struct tevent_req *subreq)
{
	struct wbc_idmap_state *state =
		tevent_req_callback_data(subreq,
		struct wbc_idmap_state);

	state->ctx->status = dcerpc_winbind_get_idmap_r_recv(subreq, state);
	TALLOC_FREE(subreq);
	if (!composite_is_ok(state->ctx)) return;

	state->ids = state->req->out.ids;
	composite_done(state->ctx);
}

NTSTATUS wbc_xids_to_sids_recv(struct composite_context *ctx,
			       struct id_map **ids)
{
	NTSTATUS status = composite_wait(ctx);
		DEBUG(5, ("wbc_xids_to_sids_recv called\n"));
	if (NT_STATUS_IS_OK(status)) {
		struct wbc_idmap_state *state =	talloc_get_type_abort(
							ctx->private_data,
							struct wbc_idmap_state);
		*ids = state->ids;
	}

	return status;
}

