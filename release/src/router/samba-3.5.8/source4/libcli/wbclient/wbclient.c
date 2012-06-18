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
#include "libcli/wbclient/wbclient.h"

/**
 * Get the server_id of the winbind task.
 *
 * \param[in] msg_ctx message context to use
 * \param[in] mem_ctx talloc context to use
 * \param[out] ids array of server_id structs containing the winbind id
 * \return NT_STATUS_OK on success, NT_STATUS_INTERNAL_ERROR on failure
 */
static NTSTATUS get_server_id(struct messaging_context *msg_ctx,
			      TALLOC_CTX *mem_ctx, struct server_id **ids)
{
	*ids = irpc_servers_byname(msg_ctx, mem_ctx, "winbind_server");
	if (*ids == NULL || (*ids)[0].id == 0) {
		DEBUG(0, ("Geting the winbind server ID failed.\n"));
		return NT_STATUS_INTERNAL_ERROR;
	}
	return NT_STATUS_OK;
}

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
	NTSTATUS status;

	ctx = talloc(mem_ctx, struct wbc_context);
	if (ctx == NULL) return NULL;

	status = get_server_id(msg_ctx, mem_ctx, &ctx->ids);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(ctx);
		return NULL;
	}

	ctx->msg_ctx = msg_ctx;
	ctx->event_ctx = event_ctx;

	return ctx;
}

struct wbc_idmap_state {
	struct composite_context *ctx;
	struct winbind_get_idmap *req;
	struct irpc_request *irpc_req;
	struct id_mapping *ids;
};

static void sids_to_xids_recv_ids(struct irpc_request *req);

struct composite_context *wbc_sids_to_xids_send(struct wbc_context *wbc_ctx,
						TALLOC_CTX *mem_ctx,
						uint32_t count,
						struct id_mapping *ids)
{
	struct composite_context *ctx;
	struct wbc_idmap_state *state;

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

	state->irpc_req = IRPC_CALL_SEND(wbc_ctx->msg_ctx, wbc_ctx->ids[0],
					 winbind, WINBIND_GET_IDMAP, state->req,
					 state);
	if (composite_nomem(state->irpc_req, ctx)) return ctx;

	composite_continue_irpc(ctx, state->irpc_req, sids_to_xids_recv_ids,
				state);
	return ctx;
}

static void sids_to_xids_recv_ids(struct irpc_request *req)
{
	struct wbc_idmap_state *state = talloc_get_type_abort(
							req->async.private_data,
							struct wbc_idmap_state);

	state->ctx->status = irpc_call_recv(state->irpc_req);
	if (!composite_is_ok(state->ctx)) return;

	state->ids = state->req->out.ids;
	composite_done(state->ctx);
}

NTSTATUS wbc_sids_to_xids_recv(struct composite_context *ctx,
			       struct id_mapping **ids)
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

static void xids_to_sids_recv_ids(struct irpc_request *req);

struct composite_context *wbc_xids_to_sids_send(struct wbc_context *wbc_ctx,
						TALLOC_CTX *mem_ctx,
						uint32_t count,
						struct id_mapping *ids)
{
	struct composite_context *ctx;
	struct wbc_idmap_state *state;

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

	state->irpc_req = IRPC_CALL_SEND(wbc_ctx->msg_ctx, wbc_ctx->ids[0],
					 winbind, WINBIND_GET_IDMAP, state->req,
					 state);
	if (composite_nomem(state->irpc_req, ctx)) return ctx;

	composite_continue_irpc(ctx, state->irpc_req, xids_to_sids_recv_ids,
			state);

	return ctx;
}

static void xids_to_sids_recv_ids(struct irpc_request *req)
{
	struct wbc_idmap_state *state = talloc_get_type_abort(
							req->async.private_data,
							struct wbc_idmap_state);

	state->ctx->status = irpc_call_recv(state->irpc_req);
	if (!composite_is_ok(state->ctx)) return;

	state->ids = state->req->out.ids;
	composite_done(state->ctx);
}

NTSTATUS wbc_xids_to_sids_recv(struct composite_context *ctx,
			       struct id_mapping **ids)
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

