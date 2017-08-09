/* 
   Unix SMB/CIFS implementation.

   Command backend for wbinfo --user-domgroups

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
#include "libcli/security/security.h"
#include "winbind/wb_server.h"
#include "winbind/wb_helper.h"
#include "smbd/service_task.h"

struct cmd_userdomgroups_state {
	struct composite_context *ctx;
	struct dom_sid *dom_sid;
	uint32_t user_rid;
	uint32_t num_rids;
	uint32_t *rids;
};

static void userdomgroups_recv_domain(struct composite_context *ctx);
static void userdomgroups_recv_rids(struct composite_context *ctx);

struct composite_context *wb_cmd_userdomgroups_send(TALLOC_CTX *mem_ctx,
						    struct wbsrv_service *service,
						    const struct dom_sid *sid)
{
	struct composite_context *result, *ctx;
	struct cmd_userdomgroups_state *state;

	result = composite_create(mem_ctx, service->task->event_ctx);
	if (result == NULL) goto failed;

	state = talloc(result, struct cmd_userdomgroups_state);
	if (state == NULL) goto failed;
	state->ctx = result;
	result->private_data = state;

	state->dom_sid = dom_sid_dup(state, sid);
	if (state->dom_sid == NULL) goto failed;
	state->dom_sid->num_auths -= 1;

	state->user_rid = sid->sub_auths[sid->num_auths-1];

	ctx = wb_sid2domain_send(state, service, sid);

	composite_continue(state->ctx, ctx, userdomgroups_recv_domain, state);

	if (ctx) {
		return result;
	}

 failed:
	talloc_free(result);
	return NULL;
}

static void userdomgroups_recv_domain(struct composite_context *ctx)
{
	struct cmd_userdomgroups_state *state =
		talloc_get_type(ctx->async.private_data,
				struct cmd_userdomgroups_state);
	struct wbsrv_domain *domain;

	state->ctx->status = wb_sid2domain_recv(ctx, &domain);
	if (!composite_is_ok(state->ctx)) return;

	ctx = wb_samr_userdomgroups_send(state, domain->libnet_ctx->samr.pipe,
					 &domain->libnet_ctx->samr.handle,
					 state->user_rid);
	composite_continue(state->ctx, ctx, userdomgroups_recv_rids, state);
	
}

static void userdomgroups_recv_rids(struct composite_context *ctx)
{
	struct cmd_userdomgroups_state *state =
		talloc_get_type(ctx->async.private_data,
				struct cmd_userdomgroups_state);

	state->ctx->status = wb_samr_userdomgroups_recv(ctx, state,
							&state->num_rids,
							&state->rids);
	if (!composite_is_ok(state->ctx)) return;
	
	composite_done(state->ctx);
}

NTSTATUS wb_cmd_userdomgroups_recv(struct composite_context *c,
				   TALLOC_CTX *mem_ctx,
				   uint32_t *num_sids, struct dom_sid ***sids)
{
	struct cmd_userdomgroups_state *state =
		talloc_get_type(c->private_data,
				struct cmd_userdomgroups_state);
	uint32_t i;
	NTSTATUS status;

	status = composite_wait(c);
	if (!NT_STATUS_IS_OK(status)) goto done;

	*num_sids = state->num_rids;
	*sids = talloc_array(mem_ctx, struct dom_sid *, state->num_rids);
	if (*sids == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}

	for (i=0; i<state->num_rids; i++) {
		(*sids)[i] = dom_sid_add_rid((*sids), state->dom_sid,
					     state->rids[i]);
		if ((*sids)[i] == NULL) {
			status = NT_STATUS_NO_MEMORY;
			goto done;
		}
	}

done:
	talloc_free(c);
	return status;
}

NTSTATUS wb_cmd_userdomgroups(TALLOC_CTX *mem_ctx,
			      struct wbsrv_service *service,
			      const struct dom_sid *sid,
			      uint32_t *num_sids, struct dom_sid ***sids)
{
	struct composite_context *c =
		wb_cmd_userdomgroups_send(mem_ctx, service, sid);
	return wb_cmd_userdomgroups_recv(c, mem_ctx, num_sids, sids);
}
