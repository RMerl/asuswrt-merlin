/*
   Unix SMB/CIFS implementation.

   Command backend for wbinfo -U

   Copyright (C) 2007-2008 Kai Blin

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
#include "libcli/composite/composite.h"
#include "winbind/wb_server.h"
#include "smbd/service_task.h"

struct uid2sid_state {
	struct composite_context *ctx;
	struct wbsrv_service *service;
	struct dom_sid *sid;
};

static void uid2sid_recv_sid(struct composite_context *ctx);

struct composite_context *wb_uid2sid_send(TALLOC_CTX *mem_ctx,
		struct wbsrv_service *service, uid_t uid)
{
	struct composite_context *result, *ctx;
	struct uid2sid_state *state;
	struct id_map *ids;

	DEBUG(5, ("wb_uid2sid_send called\n"));

	result = composite_create(mem_ctx, service->task->event_ctx);
	if (!result) return NULL;

	state = talloc(result, struct uid2sid_state);
	if (composite_nomem(state, result)) return result;

	state->ctx = result;
	result->private_data = state;
	state->service = service;

	ids = talloc(result, struct id_map);
	if (composite_nomem(ids, result)) return result;
	ids->sid = NULL;
	ids->xid.id = uid;
	ids->xid.type = ID_TYPE_UID;

	ctx = wb_xids2sids_send(result, service, 1, ids);
	if (composite_nomem(ctx, result)) return result;

	composite_continue(result, ctx, uid2sid_recv_sid, state);
	return result;
}

static void uid2sid_recv_sid(struct composite_context *ctx)
{
	struct uid2sid_state *state = talloc_get_type(ctx->async.private_data,
						      struct uid2sid_state);
	struct id_map *ids = NULL;

	state->ctx->status = wb_xids2sids_recv(ctx, &ids);
	if (!composite_is_ok(state->ctx)) return;

	if (ids->status != ID_MAPPED) {
		composite_error(state->ctx, NT_STATUS_UNSUCCESSFUL);
		return;
	}

	state->sid = ids->sid;

	composite_done(state->ctx);
}

NTSTATUS wb_uid2sid_recv(struct composite_context *ctx, TALLOC_CTX *mem_ctx,
		struct dom_sid **sid)
{
	NTSTATUS status = composite_wait(ctx);

	DEBUG(5, ("wb_uid2sid_recv called.\n"));

	if (NT_STATUS_IS_OK(status)) {
		struct uid2sid_state *state =
			talloc_get_type(ctx->private_data,
				struct uid2sid_state);
		*sid = talloc_steal(mem_ctx, state->sid);
	}
	talloc_free(ctx);
	return status;
}

