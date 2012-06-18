/*
   Unix SMB/CIFS implementation.

   Command backend for setpwent

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

struct cmd_setpwent_state {
	struct composite_context *ctx;
	struct wbsrv_service *service;
	struct libnet_context *libnet_ctx;

	struct wbsrv_pwent *result;
};

static void cmd_setpwent_recv_domain(struct composite_context *ctx);
static void cmd_setpwent_recv_user_list(struct composite_context *ctx);

struct composite_context *wb_cmd_setpwent_send(TALLOC_CTX *mem_ctx,
		struct wbsrv_service *service)
{
	struct composite_context *ctx, *result;
	struct cmd_setpwent_state *state;

	DEBUG(5, ("wb_cmd_setpwent_send called\n"));

	result = composite_create(mem_ctx, service->task->event_ctx);
	if (!result) return NULL;

	state = talloc(mem_ctx, struct cmd_setpwent_state);
	if (composite_nomem(state, result)) return result;

	state->ctx = result;
	result->private_data = state;
	state->service = service;

	state->result = talloc(state, struct wbsrv_pwent);
	if (composite_nomem(state->result, state->ctx)) return result;

	ctx = wb_sid2domain_send(state, service, service->primary_sid);
	if (composite_nomem(ctx, state->ctx)) return result;

	composite_continue(state->ctx, ctx, cmd_setpwent_recv_domain, state);
	return result;
}

static void cmd_setpwent_recv_domain(struct composite_context *ctx)
{
	struct cmd_setpwent_state *state = talloc_get_type(
			ctx->async.private_data, struct cmd_setpwent_state);
	struct wbsrv_domain *domain;
	struct libnet_UserList *user_list;

	DEBUG(5, ("cmd_setpwent_recv_domain called\n"));

	state->ctx->status = wb_sid2domain_recv(ctx, &domain);
	if (!composite_is_ok(state->ctx)) return;

	state->libnet_ctx = domain->libnet_ctx;

	user_list = talloc(state->result, struct libnet_UserList);
	if (composite_nomem(user_list, state->ctx)) return;

	user_list->in.domain_name = talloc_strdup(state,
			domain->libnet_ctx->samr.name);
	if (composite_nomem(user_list->in.domain_name, state->ctx)) return;

	/* Page size recommended by Rafal */
	user_list->in.page_size = 128;

	/* Always get the start of the list */
	user_list->in.resume_index = 0;

	ctx = libnet_UserList_send(domain->libnet_ctx, state->result, user_list,
			NULL);

	composite_continue(state->ctx, ctx, cmd_setpwent_recv_user_list, state);
}

static void cmd_setpwent_recv_user_list(struct composite_context *ctx)
{
	struct cmd_setpwent_state *state = talloc_get_type(
			ctx->async.private_data, struct cmd_setpwent_state);
	struct libnet_UserList *user_list;

	DEBUG(5, ("cmd_setpwent_recv_user_list called\n"));

	user_list = talloc(state->result, struct libnet_UserList);
	if (composite_nomem(user_list, state->ctx)) return;

	state->ctx->status = libnet_UserList_recv(ctx, state->result,
			user_list);
	if (!composite_is_ok(state->ctx)) return;

	state->result->user_list = user_list;
	state->result->page_index = 0;
	state->result->libnet_ctx = state->libnet_ctx;

	composite_done(state->ctx);
}

NTSTATUS wb_cmd_setpwent_recv(struct composite_context *ctx,
		TALLOC_CTX *mem_ctx, struct wbsrv_pwent **pwent)
{
	NTSTATUS status = composite_wait(ctx);

	DEBUG(5, ("wb_cmd_setpwent_recv called\n"));

	if (NT_STATUS_IS_OK(status)) {
		struct cmd_setpwent_state *state =
			talloc_get_type(ctx->private_data,
				struct cmd_setpwent_state);

		*pwent = talloc_steal(mem_ctx, state->result);
	}

	talloc_free(ctx);
	return status;
}

