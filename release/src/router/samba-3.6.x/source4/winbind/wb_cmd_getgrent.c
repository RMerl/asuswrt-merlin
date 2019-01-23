/*
   Unix SMB/CIFS implementation.

   Command backend for getgrent

   Copyright (C) Matthieu Patou 2010

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

struct cmd_getgrent_state {
	struct composite_context *ctx;
	struct wbsrv_service *service;

	struct wbsrv_grent *grent;
	uint32_t max_groups;

	uint32_t num_groups;
	struct winbindd_gr *result;
};

static void cmd_getgrent_recv_grnam(struct composite_context *ctx);
#if 0 /*FIXME: implement this*/
static void cmd_getgrent_recv_user_list(struct composite_context *ctx);
#endif

struct composite_context *wb_cmd_getgrent_send(TALLOC_CTX *mem_ctx,
		struct wbsrv_service *service, struct wbsrv_grent *grent,
		uint32_t max_groups)
{
	struct composite_context *ctx, *result;
	struct cmd_getgrent_state *state;

	DEBUG(5, ("wb_cmd_getgrent_send called\n"));

	result = composite_create(mem_ctx, service->task->event_ctx);
	if (!result) return NULL;

	state = talloc(mem_ctx, struct cmd_getgrent_state);
	if (composite_nomem(state, result)) return result;

	state->ctx = result;
	result->private_data = state;
	state->service = service;
	state->grent = grent;
	state->max_groups = max_groups;
	state->num_groups = 0;

	/* If there are groups left in the libnet_GroupList and we're below the
	 * maximum number of groups to get per winbind getgrent call, use
	 * getgrnam to get the winbindd_gr struct */
	if (grent->page_index < grent->group_list->out.count) {
		int idx = grent->page_index;
		char *groupname = talloc_strdup(state,
			grent->group_list->out.groups[idx].groupname);

		grent->page_index++;
		ctx = wb_cmd_getgrnam_send(state, service, groupname);
		if (composite_nomem(ctx, state->ctx)) return result;

		composite_continue(state->ctx, ctx, cmd_getgrent_recv_grnam,
			state);
	} else {
	/* If there is no valid group left, call libnet_GroupList to get a new
	 * list of group. */
		composite_error(state->ctx, NT_STATUS_NO_MORE_ENTRIES);
	}
	return result;
}

static void cmd_getgrent_recv_grnam(struct composite_context *ctx)
{
	struct cmd_getgrent_state *state =
		talloc_get_type(ctx->async.private_data,
				struct cmd_getgrent_state);
	struct winbindd_gr *gr;

	DEBUG(5, ("cmd_getgrent_recv_grnam called\n"));

	state->ctx->status = wb_cmd_getgrnam_recv(ctx, state, &gr);
	if (!composite_is_ok(state->ctx)) return;

	/*FIXME: Cheat for now and only get one group per call */
	state->result = gr;

	composite_done(state->ctx);
}

NTSTATUS wb_cmd_getgrent_recv(struct composite_context *ctx,
		TALLOC_CTX *mem_ctx, struct winbindd_gr **gr,
		uint32_t *num_groups)
{
	NTSTATUS status = composite_wait(ctx);

	DEBUG(5, ("wb_cmd_getgrent_recv called\n"));

	if (NT_STATUS_IS_OK(status)) {
		struct cmd_getgrent_state *state =
			talloc_get_type(ctx->private_data,
					struct cmd_getgrent_state);
		*gr = talloc_steal(mem_ctx, state->result);
		/*FIXME: Cheat and only get one group */
		*num_groups = 1;
	}

	talloc_free(ctx);
	return status;
}
