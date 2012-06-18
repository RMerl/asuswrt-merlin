/*
   Unix SMB/CIFS implementation.

   Map a SID to a gid

   Copyright (C) 2007-2008  Kai Blin

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
#include "winbind/wb_helper.h"
#include "libcli/security/security.h"
#include "winbind/idmap.h"

struct sid2gid_state {
	struct composite_context *ctx;
	struct wbsrv_service *service;
	gid_t gid;
};

static void sid2gid_recv_gid(struct composite_context *ctx);

struct composite_context *wb_sid2gid_send(TALLOC_CTX *mem_ctx,
		struct wbsrv_service *service, const struct dom_sid *sid)
{
	struct composite_context *result, *ctx;
	struct sid2gid_state *state;
	struct id_mapping *ids;

	DEBUG(5, ("wb_sid2gid_send called\n"));

	result = composite_create(mem_ctx, service->task->event_ctx);
	if (!result) return NULL;

	state = talloc(result, struct sid2gid_state);
	if(composite_nomem(state, result)) return result;

	state->ctx = result;
	result->private_data = state;
	state->service = service;

	ids = talloc(result, struct id_mapping);
	if (composite_nomem(ids, result)) return result;

	ids->sid = dom_sid_dup(result, sid);
	if (composite_nomem(ids->sid, result)) return result;

	ctx = wb_sids2xids_send(result, service, 1, ids);
	if (composite_nomem(ctx, result)) return result;

	composite_continue(result, ctx, sid2gid_recv_gid, state);
	return result;
}

static void sid2gid_recv_gid(struct composite_context *ctx)
{
	struct sid2gid_state *state = talloc_get_type(ctx->async.private_data,
						      struct sid2gid_state);

	struct id_mapping *ids = NULL;

	state->ctx->status = wb_sids2xids_recv(ctx, &ids);
	if (!composite_is_ok(state->ctx)) return;

	if (!NT_STATUS_IS_OK(ids->status)) {
		composite_error(state->ctx, ids->status);
		return;
	}

	if (ids->unixid->type == ID_TYPE_BOTH ||
	    ids->unixid->type == ID_TYPE_GID) {
		state->gid = ids->unixid->id;
		composite_done(state->ctx);
	} else {
		composite_error(state->ctx, NT_STATUS_INVALID_SID);
	}
}

NTSTATUS wb_sid2gid_recv(struct composite_context *ctx, gid_t *gid)
{
	NTSTATUS status = composite_wait(ctx);

	DEBUG(5, ("wb_sid2gid_recv called\n"));

	if (NT_STATUS_IS_OK(status)) {
		struct sid2gid_state *state =
			talloc_get_type(ctx->private_data,
				struct sid2gid_state);
		*gid = state->gid;
	}
	talloc_free(ctx);
	return status;
}

