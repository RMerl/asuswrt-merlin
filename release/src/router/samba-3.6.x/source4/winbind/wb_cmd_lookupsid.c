/* 
   Unix SMB/CIFS implementation.

   Command backend for wbinfo -s

   Copyright (C) Volker Lendecke 2005
   
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
#include "winbind/wb_helper.h"
#include "smbd/service_task.h"
#include "libcli/security/security.h"

struct cmd_lookupsid_state {
	struct composite_context *ctx;
	const struct dom_sid *sid;
	struct wb_sid_object *result;
};

static void lookupsid_recv_domain(struct composite_context *ctx);
static void lookupsid_recv_names(struct composite_context *ctx);

struct composite_context *wb_cmd_lookupsid_send(TALLOC_CTX *mem_ctx,
						struct wbsrv_service *service,
						const struct dom_sid *sid)
{
	struct composite_context *result, *ctx;
	struct cmd_lookupsid_state *state;

	DEBUG(5, ("wb_cmd_lookupsid_send called\n"));
	result = composite_create(mem_ctx, service->task->event_ctx);
	if (result == NULL) goto failed;

	state = talloc(result, struct cmd_lookupsid_state);
	if (state == NULL) goto failed;
	state->ctx = result;
	result->private_data = state;

	state->sid = dom_sid_dup(state, sid);
	if (state->sid == NULL) goto failed;

	ctx = wb_sid2domain_send(state, service, service->primary_sid);
	if (ctx == NULL) goto failed;

	ctx->async.fn = lookupsid_recv_domain;
	ctx->async.private_data = state;
	return result;

 failed:
	talloc_free(result);
	return NULL;
}

static void lookupsid_recv_domain(struct composite_context *ctx)
{
	struct cmd_lookupsid_state *state =
		talloc_get_type(ctx->async.private_data,
				struct cmd_lookupsid_state);
	struct wbsrv_domain *domain;

	state->ctx->status = wb_sid2domain_recv(ctx, &domain);
	if (!composite_is_ok(state->ctx)) return;

	ctx = wb_lsa_lookupsids_send(state, domain->libnet_ctx->lsa.pipe,
				     &domain->libnet_ctx->lsa.handle, 1, &state->sid);
	composite_continue(state->ctx, ctx, lookupsid_recv_names, state);
}

static void lookupsid_recv_names(struct composite_context *ctx)
{
	struct cmd_lookupsid_state *state =
		talloc_get_type(ctx->async.private_data,
				struct cmd_lookupsid_state);
	struct wb_sid_object **names;

	state->ctx->status = wb_lsa_lookupsids_recv(ctx, state, &names);
	if (!composite_is_ok(state->ctx)) return;

	state->result = names[0];
	composite_done(state->ctx);
}

NTSTATUS wb_cmd_lookupsid_recv(struct composite_context *c,
			       TALLOC_CTX *mem_ctx,
			       struct wb_sid_object **sid)
{
	struct cmd_lookupsid_state *state =
		talloc_get_type(c->private_data, struct cmd_lookupsid_state);
	NTSTATUS status = composite_wait(c);
	if (NT_STATUS_IS_OK(status)) {
		*sid = talloc_steal(mem_ctx, state->result);
	}
	talloc_free(state);
	return status;
}

NTSTATUS wb_cmd_lookupsid(TALLOC_CTX *mem_ctx, struct wbsrv_service *service,
			  const struct dom_sid *sid,
			  struct wb_sid_object **name)
{
	struct composite_context *c =
		wb_cmd_lookupsid_send(mem_ctx, service, sid);
	return wb_cmd_lookupsid_recv(c, mem_ctx, name);
}
