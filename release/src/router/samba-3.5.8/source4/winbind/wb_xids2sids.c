/*
   Unix SMB/CIFS implementation.

   Convet an unixid struct to a SID

   Copyright (C) 2008 Kai Blin

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
#include "libcli/security/proto.h"
#include "winbind/idmap.h"

struct xids2sids_state {
	struct composite_context *ctx;
	struct wbsrv_service *service;
	struct id_mapping *ids;
	int count;
};

struct composite_context *wb_xids2sids_send(TALLOC_CTX *mem_ctx,
					    struct wbsrv_service *service,
					    int count, struct id_mapping *ids)
{
	struct composite_context *result;
	struct xids2sids_state *state;

	DEBUG(5, ("wb_xids2sids_send called\n"));

	result = composite_create(mem_ctx, service->task->event_ctx);
	if (!result) return NULL;

	state = talloc(mem_ctx, struct xids2sids_state);
	if (composite_nomem(state, result)) return result;

	state->ctx = result;
	result->private_data = state;
	state->service = service;
	state->count = count;
	state->ids = ids;

	state->ctx->status = idmap_xids_to_sids(service->idmap_ctx, mem_ctx,
					        count, state->ids);
	if (!composite_is_ok(state->ctx)) return result;

	composite_done(state->ctx);
	return result;
}

NTSTATUS wb_xids2sids_recv(struct composite_context *ctx,
			   struct id_mapping **ids)
{
	NTSTATUS status = composite_wait(ctx);
	struct xids2sids_state *state =	talloc_get_type(ctx->private_data,
							struct xids2sids_state);

	DEBUG(5, ("wb_xids2sids_recv called.\n"));

	*ids = state->ids;

	talloc_free(ctx);
	return status;
}

