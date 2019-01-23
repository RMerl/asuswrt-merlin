/* 
   Unix SMB/CIFS implementation.

   Command backend for wbinfo -m

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
#include "librpc/gen_ndr/ndr_lsa_c.h"

/* List trusted domains. To avoid the trouble with having to wait for other
 * conflicting requests waiting for the lsa pipe we're opening our own lsa
 * pipe here. */

struct cmd_list_trustdom_state {
	struct composite_context *ctx;
	struct dcerpc_pipe *lsa_pipe;
	struct policy_handle *lsa_policy;
	uint32_t num_domains;
	struct wb_dom_info **domains;

	uint32_t resume_handle;
	struct lsa_DomainList domainlist;
	struct lsa_EnumTrustDom r;
};

static void cmd_list_trustdoms_recv_domain(struct composite_context *ctx);
static void cmd_list_trustdoms_recv_lsa(struct composite_context *ctx);
static void cmd_list_trustdoms_recv_doms(struct tevent_req *subreq);

struct composite_context *wb_cmd_list_trustdoms_send(TALLOC_CTX *mem_ctx,
						     struct wbsrv_service *service)
{
	struct composite_context *result, *ctx;
	struct cmd_list_trustdom_state *state;

	result = composite_create(mem_ctx, service->task->event_ctx);
	if (result == NULL) goto failed;

	state = talloc(result, struct cmd_list_trustdom_state);
	if (state == NULL) goto failed;
	state->ctx = result;
	result->private_data = state;

	ctx = wb_sid2domain_send(state, service, service->primary_sid);
	if (ctx == NULL) goto failed;
	ctx->async.fn = cmd_list_trustdoms_recv_domain;
	ctx->async.private_data = state;
	return result;

 failed:
	talloc_free(result);
	return NULL;
}

static void cmd_list_trustdoms_recv_domain(struct composite_context *ctx)
{
	struct cmd_list_trustdom_state *state =
		talloc_get_type(ctx->async.private_data,
				struct cmd_list_trustdom_state);
	struct wbsrv_domain *domain;
	struct smbcli_tree *tree;

	state->ctx->status = wb_sid2domain_recv(ctx, &domain);
	if (!composite_is_ok(state->ctx)) return;

	tree = dcerpc_smb_tree(domain->libnet_ctx->lsa.pipe->conn);
	if (composite_nomem(tree, state->ctx)) return;

	ctx = wb_init_lsa_send(state, domain);
	composite_continue(state->ctx, ctx, cmd_list_trustdoms_recv_lsa,
			   state);
}

static void cmd_list_trustdoms_recv_lsa(struct composite_context *ctx)
{
	struct cmd_list_trustdom_state *state =
		talloc_get_type(ctx->async.private_data,
				struct cmd_list_trustdom_state);
	struct tevent_req *subreq;

	state->ctx->status = wb_init_lsa_recv(ctx, state,
					      &state->lsa_pipe,
					      &state->lsa_policy);
	if (!composite_is_ok(state->ctx)) return;

	state->num_domains = 0;
	state->domains = NULL;

	state->domainlist.count = 0;
	state->domainlist.domains = NULL;

	state->resume_handle = 0;
	state->r.in.handle = state->lsa_policy;
	state->r.in.resume_handle = &state->resume_handle;
	state->r.in.max_size = 1000;
	state->r.out.resume_handle = &state->resume_handle;
	state->r.out.domains = &state->domainlist;

	subreq = dcerpc_lsa_EnumTrustDom_r_send(state,
						state->ctx->event_ctx,
						state->lsa_pipe->binding_handle,
						&state->r);
	if (composite_nomem(subreq, state->ctx)) return;
	tevent_req_set_callback(subreq, cmd_list_trustdoms_recv_doms, state);
}

static void cmd_list_trustdoms_recv_doms(struct tevent_req *subreq)
{
	struct cmd_list_trustdom_state *state =
		tevent_req_callback_data(subreq,
		struct cmd_list_trustdom_state);
	uint32_t i, old_num_domains;

	state->ctx->status = dcerpc_lsa_EnumTrustDom_r_recv(subreq, state);
	TALLOC_FREE(subreq);
	if (!composite_is_ok(state->ctx)) return;
	state->ctx->status = state->r.out.result;

	if (!NT_STATUS_IS_OK(state->ctx->status) &&
	    !NT_STATUS_EQUAL(state->ctx->status, NT_STATUS_NO_MORE_ENTRIES) &&
	    !NT_STATUS_EQUAL(state->ctx->status, STATUS_MORE_ENTRIES)) {
		composite_error(state->ctx, state->ctx->status);
		return;
	}

	old_num_domains = state->num_domains;

	state->num_domains += state->r.out.domains->count;
	state->domains = talloc_realloc(state, state->domains,
					struct wb_dom_info *,
					state->num_domains);
	if (state->num_domains && 
	    composite_nomem(state->domains, state->ctx)) return;

	for (i=0; i<state->r.out.domains->count; i++) {
		uint32_t j = i+old_num_domains;
		state->domains[j] = talloc(state->domains,
					   struct wb_dom_info);
		if (composite_nomem(state->domains[i], state->ctx)) return;
		state->domains[j]->name = talloc_steal(
			state->domains[j],
			state->r.out.domains->domains[i].name.string);
		state->domains[j]->sid = talloc_steal(
			state->domains[j],
			state->r.out.domains->domains[i].sid);
	}

	if (NT_STATUS_IS_OK(state->ctx->status) || NT_STATUS_EQUAL(state->ctx->status, NT_STATUS_NO_MORE_ENTRIES)) {
		state->ctx->status = NT_STATUS_OK;
		composite_done(state->ctx);
		return;
	}

	state->domainlist.count = 0;
	state->domainlist.domains = NULL;
	state->r.in.handle = state->lsa_policy;
	state->r.in.resume_handle = &state->resume_handle;
	state->r.in.max_size = 1000;
	state->r.out.resume_handle = &state->resume_handle;
	state->r.out.domains = &state->domainlist;
	
	subreq = dcerpc_lsa_EnumTrustDom_r_send(state,
						state->ctx->event_ctx,
						state->lsa_pipe->binding_handle,
						&state->r);
	if (composite_nomem(subreq, state->ctx)) return;
	tevent_req_set_callback(subreq, cmd_list_trustdoms_recv_doms, state);
}

NTSTATUS wb_cmd_list_trustdoms_recv(struct composite_context *ctx,
				    TALLOC_CTX *mem_ctx,
				    uint32_t *num_domains,
				    struct wb_dom_info ***domains)
{
	NTSTATUS status = composite_wait(ctx);
	if (NT_STATUS_IS_OK(status)) {
		struct cmd_list_trustdom_state *state =
			talloc_get_type(ctx->private_data,
					struct cmd_list_trustdom_state);
		*num_domains = state->num_domains;
		*domains = talloc_steal(mem_ctx, state->domains);
	}
	talloc_free(ctx);
	return status;
}
