/* 
   Unix SMB/CIFS implementation.

   Command backend for wbinfo --user-sids

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
#include "smbd/service_task.h"
#include "librpc/gen_ndr/ndr_samr_c.h"
#include "libcli/security/security.h"

/* Calculate the token in two steps: Go the user's originating domain, ask for
 * the user's domain groups. Then with the resulting list of sids go to our
 * own domain to expand the aliases aka domain local groups. */

struct cmd_usersids_state {
	struct composite_context *ctx;
	struct wbsrv_service *service;
	struct dom_sid *user_sid;
	uint32_t num_domgroups;
	struct dom_sid **domgroups;

	struct lsa_SidArray lsa_sids;
	struct samr_Ids rids;
	struct samr_GetAliasMembership r;

	uint32_t num_sids;
	struct dom_sid **sids;
};

static void usersids_recv_domgroups(struct composite_context *ctx);
static void usersids_recv_domain(struct composite_context *ctx);
static void usersids_recv_aliases(struct tevent_req *subreq);

struct composite_context *wb_cmd_usersids_send(TALLOC_CTX *mem_ctx,
					       struct wbsrv_service *service,
					       const struct dom_sid *sid)
{
	struct composite_context *result, *ctx;
	struct cmd_usersids_state *state;


	result = composite_create(mem_ctx, service->task->event_ctx);
	if (result == NULL) goto failed;

	state = talloc(result, struct cmd_usersids_state);
	if (state == NULL) goto failed;
	state->ctx = result;
	result->private_data = state;

	state->service = service;
	state->user_sid = dom_sid_dup(state, sid);
	if (state->user_sid == NULL) goto failed;

	ctx = wb_cmd_userdomgroups_send(state, service, sid);
	if (ctx == NULL) goto failed;

	ctx->async.fn = usersids_recv_domgroups;
	ctx->async.private_data = state;
	return result;

 failed:
	talloc_free(result);
	return NULL;
}

static void usersids_recv_domgroups(struct composite_context *ctx)
{
	struct cmd_usersids_state *state =
		talloc_get_type(ctx->async.private_data,
				struct cmd_usersids_state);

	state->ctx->status = wb_cmd_userdomgroups_recv(ctx, state,
						       &state->num_domgroups,
						       &state->domgroups);
	if (!composite_is_ok(state->ctx)) return;

	ctx = wb_sid2domain_send(state, state->service,
				 state->service->primary_sid);
	composite_continue(state->ctx, ctx, usersids_recv_domain, state);
}

static void usersids_recv_domain(struct composite_context *ctx)
{
        struct cmd_usersids_state *state =
                talloc_get_type(ctx->async.private_data,
                                struct cmd_usersids_state);
	struct tevent_req *subreq;
	struct wbsrv_domain *domain;
	uint32_t i;

	state->ctx->status = wb_sid2domain_recv(ctx, &domain);
	if (!composite_is_ok(state->ctx)) return;

	state->lsa_sids.num_sids = state->num_domgroups+1;
	state->lsa_sids.sids = talloc_array(state, struct lsa_SidPtr,
					    state->lsa_sids.num_sids);
	if (composite_nomem(state->lsa_sids.sids, state->ctx)) return;

	state->lsa_sids.sids[0].sid = state->user_sid;
	for (i=0; i<state->num_domgroups; i++) {
		state->lsa_sids.sids[i+1].sid = state->domgroups[i];
	}

	state->rids.count = 0;
	state->rids.ids = NULL;

	state->r.in.domain_handle = &domain->libnet_ctx->samr.handle;
	state->r.in.sids = &state->lsa_sids;
	state->r.out.rids = &state->rids;

	subreq = dcerpc_samr_GetAliasMembership_r_send(state,
						       state->ctx->event_ctx,
						       domain->libnet_ctx->samr.pipe->binding_handle,
						       &state->r);
	if (composite_nomem(subreq, state->ctx)) return;
	tevent_req_set_callback(subreq, usersids_recv_aliases, state);
}

static void usersids_recv_aliases(struct tevent_req *subreq)
{
	struct cmd_usersids_state *state =
		tevent_req_callback_data(subreq,
		struct cmd_usersids_state);
	uint32_t i;

	state->ctx->status = dcerpc_samr_GetAliasMembership_r_recv(subreq, state);
	TALLOC_FREE(subreq);
	if (!composite_is_ok(state->ctx)) return;
	state->ctx->status = state->r.out.result;
	if (!composite_is_ok(state->ctx)) return;

	state->num_sids = 1 + state->num_domgroups + state->r.out.rids->count;
	state->sids = talloc_array(state, struct dom_sid *, state->num_sids);
	if (composite_nomem(state->sids, state->ctx)) return;

	state->sids[0] = talloc_steal(state->sids, state->user_sid);

	for (i=0; i<state->num_domgroups; i++) {
		state->sids[1+i] =
			talloc_steal(state->sids, state->domgroups[i]);
	}

	for (i=0; i<state->r.out.rids->count; i++) {
		state->sids[1+state->num_domgroups+i] =	dom_sid_add_rid(
			state->sids, state->service->primary_sid,
			state->r.out.rids->ids[i]);

		if (composite_nomem(state->sids[1+state->num_domgroups+i],
				    state->ctx)) return;
	}

	composite_done(state->ctx);
}

NTSTATUS wb_cmd_usersids_recv(struct composite_context *ctx,
			      TALLOC_CTX *mem_ctx,
			      uint32_t *num_sids, struct dom_sid ***sids)
{
	NTSTATUS status = composite_wait(ctx);
	if (NT_STATUS_IS_OK(status)) {
		struct cmd_usersids_state *state =
			talloc_get_type(ctx->private_data,
					struct cmd_usersids_state);
		*num_sids = state->num_sids;
		*sids = talloc_steal(mem_ctx, state->sids);
	}
	talloc_free(ctx);
	return status;
}

NTSTATUS wb_cmd_usersids(TALLOC_CTX *mem_ctx, struct wbsrv_service *service,
			 const struct dom_sid *sid,
			 uint32_t *num_sids, struct dom_sid ***sids)
{
	struct composite_context *c =
		wb_cmd_usersids_send(mem_ctx, service, sid);
	return wb_cmd_usersids_recv(c, mem_ctx, num_sids, sids);
}

