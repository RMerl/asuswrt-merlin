/* 
   Unix SMB/CIFS implementation.

   Command backend for wbinfo -n

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
#include "winbind/wb_async_helpers.h"
#include "winbind/wb_helper.h"
#include "smbd/service_task.h"

struct cmd_lookupname_state {
	struct composite_context *ctx;
	const char *name;
	struct wb_sid_object *result;
};

static void lookupname_recv_domain(struct composite_context *ctx);
static void lookupname_recv_sids(struct composite_context *ctx);

struct composite_context *wb_cmd_lookupname_send(TALLOC_CTX *mem_ctx,
						 struct wbsrv_service *service,
						 const char *dom_name,
						 const char *name)
{
	struct composite_context *result, *ctx;
	struct cmd_lookupname_state *state;

	result = composite_create(mem_ctx, service->task->event_ctx);
	if (result == NULL) goto failed;

	state = talloc(result, struct cmd_lookupname_state);
	if (state == NULL) goto failed;
	state->ctx = result;
	result->private_data = state;

	state->name = talloc_asprintf(state, "%s\\%s", dom_name, name);
	if (state->name == NULL) goto failed;

	ctx = wb_sid2domain_send(state, service, service->primary_sid);
	if (ctx == NULL) goto failed;

	ctx->async.fn = lookupname_recv_domain;
	ctx->async.private_data = state;
	return result;

 failed:
	talloc_free(result);
	return NULL;
}

static void lookupname_recv_domain(struct composite_context *ctx)
{
	struct cmd_lookupname_state *state =
		talloc_get_type(ctx->async.private_data,
				struct cmd_lookupname_state);
	struct wbsrv_domain *domain;

	state->ctx->status = wb_sid2domain_recv(ctx, &domain);
	if (!composite_is_ok(state->ctx)) return;

	ctx = wb_lsa_lookupnames_send(state, domain->libnet_ctx->lsa.pipe,
				      &domain->libnet_ctx->lsa.handle, 1, &state->name);
	composite_continue(state->ctx, ctx, lookupname_recv_sids, state);
}

static void lookupname_recv_sids(struct composite_context *ctx)
{
	struct cmd_lookupname_state *state =
		talloc_get_type(ctx->async.private_data,
				struct cmd_lookupname_state);
	struct wb_sid_object **sids;

	state->ctx->status = wb_lsa_lookupnames_recv(ctx, state, &sids);
	if (!composite_is_ok(state->ctx)) return;

	state->result = sids[0];
	composite_done(state->ctx);
}

NTSTATUS wb_cmd_lookupname_recv(struct composite_context *c,
				TALLOC_CTX *mem_ctx,
				struct wb_sid_object **sid)
{
	struct cmd_lookupname_state *state =
		talloc_get_type(c->private_data, struct cmd_lookupname_state);
	NTSTATUS status = composite_wait(c);
	if (NT_STATUS_IS_OK(status)) {
		*sid = talloc_steal(mem_ctx, state->result);
	}
	talloc_free(state);
	return status;
}

NTSTATUS wb_cmd_lookupname(TALLOC_CTX *mem_ctx,
			   struct wbsrv_service *service,
			   const char *dom_name,
			   const char *name,
			   struct wb_sid_object **sid)
{
	struct composite_context *c =
		wb_cmd_lookupname_send(mem_ctx, service, dom_name, name);
	return wb_cmd_lookupname_recv(c, mem_ctx, sid);
}
