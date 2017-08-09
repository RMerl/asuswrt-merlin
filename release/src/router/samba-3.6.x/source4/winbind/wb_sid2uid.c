/*
   Unix SMB/CIFS implementation.

   Map a SID to a uid

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
#include "libcli/security/security.h"

struct sid2uid_state {
	struct composite_context *ctx;
	struct wbsrv_service *service;
	uid_t uid;
};

static void sid2uid_recv_uid(struct composite_context *ctx);

struct composite_context *wb_sid2uid_send(TALLOC_CTX *mem_ctx,
		struct wbsrv_service *service, const struct dom_sid *sid)
{
	struct composite_context *result, *ctx;
	struct sid2uid_state *state;
	struct id_map *ids;

	DEBUG(5, ("wb_sid2uid_send called\n"));

	result = composite_create(mem_ctx, service->task->event_ctx);
	if (!result) return NULL;

	state = talloc(result, struct sid2uid_state);
	if (composite_nomem(state, result)) return result;

	state->ctx = result;
	result->private_data = state;
	state->service = service;

	ids = talloc(result, struct id_map);
	if (composite_nomem(ids, result)) return result;

	ids->sid = dom_sid_dup(result, sid);
	if (composite_nomem(ids->sid, result)) return result;

	ctx = wb_sids2xids_send(result, service, 1, ids);
	if (composite_nomem(ctx, result)) return result;

	composite_continue(result, ctx, sid2uid_recv_uid, state);
	return result;
}

static void sid2uid_recv_uid(struct composite_context *ctx)
{
	struct sid2uid_state *state = talloc_get_type(ctx->async.private_data,
						      struct sid2uid_state);

	struct id_map *ids = NULL;

	state->ctx->status = wb_sids2xids_recv(ctx, &ids);
	if (!composite_is_ok(state->ctx)) return;

	if (ids->status != ID_MAPPED) {
		composite_error(state->ctx, NT_STATUS_UNSUCCESSFUL);
		return;
	}

	if (ids->xid.type == ID_TYPE_BOTH ||
	    ids->xid.type == ID_TYPE_UID) {
		state->uid = ids->xid.id;
		composite_done(state->ctx);
	} else {
		composite_error(state->ctx, NT_STATUS_INVALID_SID);
	}
}

NTSTATUS wb_sid2uid_recv(struct composite_context *ctx, uid_t *uid)
{
	NTSTATUS status = composite_wait(ctx);

	DEBUG(5, ("wb_sid2uid_recv called\n"));

	if (NT_STATUS_IS_OK(status)) {
		struct sid2uid_state *state =
			talloc_get_type(ctx->private_data,
				struct sid2uid_state);
		*uid = state->uid;
	}
	talloc_free(ctx);
	return status;
}

