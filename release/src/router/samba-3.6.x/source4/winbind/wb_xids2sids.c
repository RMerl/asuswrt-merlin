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

struct xids2sids_state {
	struct composite_context *ctx;
	struct wbsrv_service *service;
	struct id_map *ids;
	int count;
};

struct composite_context *wb_xids2sids_send(TALLOC_CTX *mem_ctx,
					    struct wbsrv_service *service,
					    unsigned int count, struct id_map *ids)
{
	struct composite_context *result;
	struct xids2sids_state *state;
	struct id_map **pointer_array;
	unsigned int i;

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

	/* We need to convert between calling conventions here - the
	 * values are filled in by reference, so we just need to
	 * provide pointers to them */
	pointer_array = talloc_array(state, struct id_map *, count+1);
	if (composite_nomem(pointer_array, result)) return result;

	for (i=0; i < count; i++) {
		pointer_array[i] = &ids[i];
	}
	pointer_array[i] = NULL;

	state->ctx->status = idmap_xids_to_sids(service->idmap_ctx, mem_ctx,
					        pointer_array);
	if (!composite_is_ok(state->ctx)) return result;

	composite_done(state->ctx);
	return result;
}

NTSTATUS wb_xids2sids_recv(struct composite_context *ctx,
			   struct id_map **ids)
{
	NTSTATUS status = composite_wait(ctx);
	struct xids2sids_state *state =	talloc_get_type(ctx->private_data,
							struct xids2sids_state);

	DEBUG(5, ("wb_xids2sids_recv called.\n"));

	/* We don't have to mess with pointer_array on the way out, as
	 * the results are filled into the pointers the caller
	 * supplied */
	*ids = state->ids;

	talloc_free(ctx);
	return status;
}

