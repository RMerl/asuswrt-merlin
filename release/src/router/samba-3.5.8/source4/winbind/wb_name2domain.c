/*
   Unix SMB/CIFS implementation.

   Find and init a domain struct for a name

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
#include "smbd/service_task.h"
#include "winbind/wb_helper.h"
#include "param/param.h"

struct name2domain_state {
	struct composite_context *ctx;
	struct wbsrv_service *service;

	struct wbsrv_domain *domain;
};

static void name2domain_recv_sid(struct composite_context *ctx);
static void name2domain_recv_domain(struct composite_context *ctx);

struct composite_context *wb_name2domain_send(TALLOC_CTX *mem_ctx,
		struct wbsrv_service *service, const char* name)
{
	struct composite_context *result, *ctx;
	struct name2domain_state *state;
	char *user_dom, *user_name;
	bool ok;

	DEBUG(5, ("wb_name2domain_send called\n"));

	result = composite_create(mem_ctx, service->task->event_ctx);
	if (!result) return NULL;

	state = talloc(result, struct name2domain_state);
	if (composite_nomem(state, result)) return result;
	state->ctx = result;
	result->private_data = state;
	state->service = service;

	ok = wb_samba3_split_username(state, service->task->lp_ctx, name, &user_dom, &user_name);
	if(!ok) {
		composite_error(state->ctx, NT_STATUS_OBJECT_NAME_INVALID);
		return result;
	}

	ctx = wb_cmd_lookupname_send(state, service, user_dom, user_name);
	if (composite_nomem(ctx, state->ctx)) return result;

	composite_continue(result, ctx, name2domain_recv_sid, state);
	return result;
}

static void name2domain_recv_sid(struct composite_context *ctx)
{
	struct name2domain_state *state =
		talloc_get_type(ctx->async.private_data,
				struct name2domain_state);
	struct wb_sid_object *sid;

	DEBUG(5, ("name2domain_recv_sid called\n"));

	state->ctx->status = wb_cmd_lookupname_recv(ctx, state, &sid);
	if(!composite_is_ok(state->ctx)) return;

	ctx = wb_sid2domain_send(state, state->service, sid->sid);

	composite_continue(state->ctx, ctx, name2domain_recv_domain, state);
}

static void name2domain_recv_domain(struct composite_context *ctx)
{
	struct name2domain_state *state =
		talloc_get_type(ctx->async.private_data,
				struct name2domain_state);
	struct wbsrv_domain *domain;

	DEBUG(5, ("name2domain_recv_domain called\n"));

	state->ctx->status = wb_sid2domain_recv(ctx, &domain);
	if(!composite_is_ok(state->ctx)) return;

	state->domain = domain;

	composite_done(state->ctx);
}

NTSTATUS wb_name2domain_recv(struct composite_context *ctx,
		struct wbsrv_domain **result)
{
	NTSTATUS status = composite_wait(ctx);

	DEBUG(5, ("wb_name2domain_recv called\n"));

	if (NT_STATUS_IS_OK(status)) {
		struct name2domain_state *state =
			talloc_get_type(ctx->private_data,
					struct name2domain_state);
		*result = state->domain;
	}
	talloc_free(ctx);
	return status;
}

