/*
   Unix SMB/CIFS implementation.

   Command backend for getpwent

   Copyright (C) Kai Blin 2007

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
#include "winbind/wb_async_helpers.h"
#include "winbind/wb_helper.h"
#include "smbd/service_task.h"
#include "libnet/libnet_proto.h"

struct cmd_getpwent_state {
	struct composite_context *ctx;
	struct wbsrv_service *service;

	struct wbsrv_pwent *pwent;
	uint32_t max_users;

	uint32_t num_users;
	struct winbindd_pw *result;
};

static void cmd_getpwent_recv_pwnam(struct composite_context *ctx);
#if 0 /*FIXME: implement this*/
static void cmd_getpwent_recv_user_list(struct composite_context *ctx);
#endif

struct composite_context *wb_cmd_getpwent_send(TALLOC_CTX *mem_ctx,
		struct wbsrv_service *service, struct wbsrv_pwent *pwent,
		uint32_t max_users)
{
	struct composite_context *ctx, *result;
	struct cmd_getpwent_state *state;

	DEBUG(5, ("wb_cmd_getpwent_send called\n"));

	result = composite_create(mem_ctx, service->task->event_ctx);
	if (!result) return NULL;

	state = talloc(mem_ctx, struct cmd_getpwent_state);
	if (composite_nomem(state, result)) return result;

	state->ctx = result;
	result->private_data = state;
	state->service = service;
	state->pwent = pwent;
	state->max_users = max_users;
	state->num_users = 0;

	/* If there are users left in the libnet_UserList and we're below the
	 * maximum number of users to get per winbind getpwent call, use
	 * getpwnam to get the winbindd_pw struct */
	if (pwent->page_index < pwent->user_list->out.count) {
		int idx = pwent->page_index;
		char *username = talloc_strdup(state,
			pwent->user_list->out.users[idx].username);

		pwent->page_index++;
		ctx = wb_cmd_getpwnam_send(state, service, username);
		if (composite_nomem(ctx, state->ctx)) return result;

		composite_continue(state->ctx, ctx, cmd_getpwent_recv_pwnam,
			state);
	} else {
	/* If there is no valid user left, call libnet_UserList to get a new
	 * list of users. */
		composite_error(state->ctx, NT_STATUS_NO_MORE_ENTRIES);
	}
	return result;
}

static void cmd_getpwent_recv_pwnam(struct composite_context *ctx)
{
	struct cmd_getpwent_state *state =
		talloc_get_type(ctx->async.private_data,
				struct cmd_getpwent_state);
	struct winbindd_pw *pw;

	DEBUG(5, ("cmd_getpwent_recv_pwnam called\n"));

	state->ctx->status = wb_cmd_getpwnam_recv(ctx, state, &pw);
	if (!composite_is_ok(state->ctx)) return;

	/*FIXME: Cheat for now and only get one user per call */
	state->result = pw;

	composite_done(state->ctx);
}

NTSTATUS wb_cmd_getpwent_recv(struct composite_context *ctx,
		TALLOC_CTX *mem_ctx, struct winbindd_pw **pw,
		uint32_t *num_users)
{
	NTSTATUS status = composite_wait(ctx);

	DEBUG(5, ("wb_cmd_getpwent_recv called\n"));

	if (NT_STATUS_IS_OK(status)) {
		struct cmd_getpwent_state *state =
			talloc_get_type(ctx->private_data,
					struct cmd_getpwent_state);
		*pw = talloc_steal(mem_ctx, state->result);
		/*FIXME: Cheat and only get oner user */
		*num_users = 1;
	}

	talloc_free(ctx);
	return status;
}

