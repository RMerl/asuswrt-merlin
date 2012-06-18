/* 
   Unix SMB/CIFS implementation.

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
/*
  a composite API for finding a DC and its name
*/

#include "includes.h"
#include "libcli/composite/composite.h"
#include "winbind/wb_async_helpers.h"

#include "lib/messaging/irpc.h"
#include "librpc/gen_ndr/irpc.h"
#include "auth/credentials/credentials.h"
#include "libcli/security/security.h"
#include "libcli/auth/libcli_auth.h"
#include "librpc/gen_ndr/ndr_netlogon_c.h"
#include "librpc/gen_ndr/ndr_lsa_c.h"
#include "librpc/gen_ndr/ndr_samr_c.h"

#include "winbind/wb_helper.h"

struct lsa_lookupsids_state {
	struct composite_context *ctx;
	int num_sids;
	struct lsa_LookupSids r;
	struct lsa_SidArray sids;
	struct lsa_TransNameArray names;
	struct lsa_RefDomainList *domains;
	uint32_t count;
	struct wb_sid_object **result;
};

static void lsa_lookupsids_recv_names(struct rpc_request *req);

struct composite_context *wb_lsa_lookupsids_send(TALLOC_CTX *mem_ctx,
						 struct dcerpc_pipe *lsa_pipe,
						 struct policy_handle *handle,
						 int num_sids,
						 const struct dom_sid **sids)
{
	struct composite_context *result;
	struct rpc_request *req;
	struct lsa_lookupsids_state *state;
	int i;

	result = composite_create(mem_ctx, lsa_pipe->conn->event_ctx);
	if (result == NULL) goto failed;

	state = talloc(result, struct lsa_lookupsids_state);
	if (state == NULL) goto failed;
	result->private_data = state;
	state->ctx = result;

	state->sids.num_sids = num_sids;
	state->sids.sids = talloc_array(state, struct lsa_SidPtr, num_sids);
	if (state->sids.sids == NULL) goto failed;

	for (i=0; i<num_sids; i++) {
		state->sids.sids[i].sid = dom_sid_dup(state->sids.sids,
						      sids[i]);
		if (state->sids.sids[i].sid == NULL) goto failed;
	}

	state->domains = talloc(state, struct lsa_RefDomainList);
	if (state->domains == NULL) goto failed;

	state->count = 0;
	state->num_sids = num_sids;
	state->names.count = 0;
	state->names.names = NULL;

	state->r.in.handle = handle;
	state->r.in.sids = &state->sids;
	state->r.in.names = &state->names;
	state->r.in.level = 1;
	state->r.in.count = &state->count;
	state->r.out.names = &state->names;
	state->r.out.count = &state->count;
	state->r.out.domains = &state->domains;

	req = dcerpc_lsa_LookupSids_send(lsa_pipe, state, &state->r);
	if (req == NULL) goto failed;

	req->async.callback = lsa_lookupsids_recv_names;
	req->async.private_data = state;
	return result;

 failed:
	talloc_free(result);
	return NULL;
}

static void lsa_lookupsids_recv_names(struct rpc_request *req)
{
	struct lsa_lookupsids_state *state =
		talloc_get_type(req->async.private_data,
				struct lsa_lookupsids_state);
	int i;

	state->ctx->status = dcerpc_ndr_request_recv(req);
	if (!composite_is_ok(state->ctx)) return;
	state->ctx->status = state->r.out.result;
	if (!NT_STATUS_IS_OK(state->ctx->status) &&
	    !NT_STATUS_EQUAL(state->ctx->status, STATUS_SOME_UNMAPPED)) {
		composite_error(state->ctx, state->ctx->status);
		return;
	}

	state->result = talloc_array(state, struct wb_sid_object *,
				     state->num_sids);
	if (composite_nomem(state->result, state->ctx)) return;

	for (i=0; i<state->num_sids; i++) {
		struct lsa_TranslatedName *name =
			&state->r.out.names->names[i];
		struct lsa_DomainInfo *dom;
		struct lsa_RefDomainList *domains =
			state->domains;

		state->result[i] = talloc_zero(state->result,
					       struct wb_sid_object);
		if (composite_nomem(state->result[i], state->ctx)) return;

		state->result[i]->type = name->sid_type;
		if (state->result[i]->type == SID_NAME_UNKNOWN) {
			continue;
		}

		if (name->sid_index >= domains->count) {
			composite_error(state->ctx,
					NT_STATUS_INVALID_PARAMETER);
			return;
		}

		dom = &domains->domains[name->sid_index];
		state->result[i]->domain = talloc_reference(state->result[i],
							    dom->name.string);
		if ((name->sid_type == SID_NAME_DOMAIN) ||
		    (name->name.string == NULL)) {
			state->result[i]->name =
				talloc_strdup(state->result[i], "");
		} else {
			state->result[i]->name =
				talloc_steal(state->result[i],
					     name->name.string);
		}

		if (composite_nomem(state->result[i]->name, state->ctx)) {
			return;
		}
	}

	composite_done(state->ctx);
}

NTSTATUS wb_lsa_lookupsids_recv(struct composite_context *c,
				TALLOC_CTX *mem_ctx,
				struct wb_sid_object ***names)
{
	NTSTATUS status = composite_wait(c);
	if (NT_STATUS_IS_OK(status)) {
		struct lsa_lookupsids_state *state =
			talloc_get_type(c->private_data,
					struct lsa_lookupsids_state);
		*names = talloc_steal(mem_ctx, state->result);
	}
	talloc_free(c);
	return status;
}


struct lsa_lookupnames_state {
	struct composite_context *ctx;
	uint32_t num_names;
	struct lsa_LookupNames r;
	struct lsa_TransSidArray sids;
	struct lsa_RefDomainList *domains;
	uint32_t count;
	struct wb_sid_object **result;
};

static void lsa_lookupnames_recv_sids(struct rpc_request *req);

struct composite_context *wb_lsa_lookupnames_send(TALLOC_CTX *mem_ctx,
						  struct dcerpc_pipe *lsa_pipe,
						  struct policy_handle *handle,
						  int num_names,
						  const char **names)
{
	struct composite_context *result;
	struct rpc_request *req;
	struct lsa_lookupnames_state *state;

	struct lsa_String *lsa_names;
	int i;

	result = composite_create(mem_ctx, lsa_pipe->conn->event_ctx);
	if (result == NULL) goto failed;

	state = talloc(result, struct lsa_lookupnames_state);
	if (state == NULL) goto failed;
	result->private_data = state;
	state->ctx = result;

	state->sids.count = 0;
	state->sids.sids = NULL;
	state->num_names = num_names;
	state->count = 0;

	lsa_names = talloc_array(state, struct lsa_String, num_names);
	if (lsa_names == NULL) goto failed;

	for (i=0; i<num_names; i++) {
		lsa_names[i].string = names[i];
	}

	state->domains = talloc(state, struct lsa_RefDomainList);
	if (state->domains == NULL) goto failed;

	state->r.in.handle = handle;
	state->r.in.num_names = num_names;
	state->r.in.names = lsa_names;
	state->r.in.sids = &state->sids;
	state->r.in.level = 1;
	state->r.in.count = &state->count;
	state->r.out.count = &state->count;
	state->r.out.sids = &state->sids;
	state->r.out.domains = &state->domains;

	req = dcerpc_lsa_LookupNames_send(lsa_pipe, state, &state->r);
	if (req == NULL) goto failed;

	req->async.callback = lsa_lookupnames_recv_sids;
	req->async.private_data = state;
	return result;

 failed:
	talloc_free(result);
	return NULL;
}

static void lsa_lookupnames_recv_sids(struct rpc_request *req)
{
	struct lsa_lookupnames_state *state =
		talloc_get_type(req->async.private_data,
				struct lsa_lookupnames_state);
	int i;

	state->ctx->status = dcerpc_ndr_request_recv(req);
	if (!composite_is_ok(state->ctx)) return;
	state->ctx->status = state->r.out.result;
	if (!NT_STATUS_IS_OK(state->ctx->status) &&
	    !NT_STATUS_EQUAL(state->ctx->status, STATUS_SOME_UNMAPPED)) {
		composite_error(state->ctx, state->ctx->status);
		return;
	}

	state->result = talloc_array(state, struct wb_sid_object *,
				     state->num_names);
	if (composite_nomem(state->result, state->ctx)) return;

	for (i=0; i<state->num_names; i++) {
		struct lsa_TranslatedSid *sid = &state->r.out.sids->sids[i];
		struct lsa_RefDomainList *domains = state->domains;
		struct lsa_DomainInfo *dom;

		state->result[i] = talloc_zero(state->result,
					       struct wb_sid_object);
		if (composite_nomem(state->result[i], state->ctx)) return;

		state->result[i]->type = sid->sid_type;
		if (state->result[i]->type == SID_NAME_UNKNOWN) {
			continue;
		}

		if (sid->sid_index >= domains->count) {
			composite_error(state->ctx,
					NT_STATUS_INVALID_PARAMETER);
			return;
		}

		dom = &domains->domains[sid->sid_index];

		state->result[i]->sid = dom_sid_add_rid(state->result[i],
							dom->sid, sid->rid);
	}

	composite_done(state->ctx);
}

NTSTATUS wb_lsa_lookupnames_recv(struct composite_context *c,
				 TALLOC_CTX *mem_ctx,
				 struct wb_sid_object ***sids)
{
	NTSTATUS status = composite_wait(c);
	if (NT_STATUS_IS_OK(status)) {
		struct lsa_lookupnames_state *state =
			talloc_get_type(c->private_data,
					struct lsa_lookupnames_state);
		*sids = talloc_steal(mem_ctx, state->result);
	}
	talloc_free(c);
	return status;
}
struct samr_getuserdomgroups_state {
	struct composite_context *ctx;
	struct dcerpc_pipe *samr_pipe;

	int num_rids;
	uint32_t *rids;

	struct samr_RidWithAttributeArray *rid_array;

	struct policy_handle *user_handle;
	struct samr_OpenUser o;
	struct samr_GetGroupsForUser g;
	struct samr_Close c;
};

static void samr_usergroups_recv_open(struct rpc_request *req);
static void samr_usergroups_recv_groups(struct rpc_request *req);
static void samr_usergroups_recv_close(struct rpc_request *req);

struct composite_context *wb_samr_userdomgroups_send(TALLOC_CTX *mem_ctx,
						     struct dcerpc_pipe *samr_pipe,
						     struct policy_handle *domain_handle,
						     uint32_t rid)
{
	struct composite_context *result;
	struct rpc_request *req;
	struct samr_getuserdomgroups_state *state;

	result = composite_create(mem_ctx, samr_pipe->conn->event_ctx);
	if (result == NULL) goto failed;

	state = talloc(result, struct samr_getuserdomgroups_state);
	if (state == NULL) goto failed;
	result->private_data = state;
	state->ctx = result;

	state->samr_pipe = samr_pipe;

	state->user_handle = talloc(state, struct policy_handle);
	if (state->user_handle == NULL) goto failed;

	state->o.in.domain_handle = domain_handle;
	state->o.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	state->o.in.rid = rid;
	state->o.out.user_handle = state->user_handle;

	req = dcerpc_samr_OpenUser_send(state->samr_pipe, state, &state->o);
	if (req == NULL) goto failed;

	req->async.callback = samr_usergroups_recv_open;
	req->async.private_data = state;
	return result;

 failed:
	talloc_free(result);
	return NULL;
}
					      
static void samr_usergroups_recv_open(struct rpc_request *req)
{
	struct samr_getuserdomgroups_state *state =
		talloc_get_type(req->async.private_data,
				struct samr_getuserdomgroups_state);

	state->ctx->status = dcerpc_ndr_request_recv(req);
	if (!composite_is_ok(state->ctx)) return;
	state->ctx->status = state->o.out.result;
	if (!composite_is_ok(state->ctx)) return;

	state->g.in.user_handle = state->user_handle;
	state->g.out.rids = &state->rid_array;

	req = dcerpc_samr_GetGroupsForUser_send(state->samr_pipe, state,
						&state->g);
	composite_continue_rpc(state->ctx, req, samr_usergroups_recv_groups,
			       state);
}

static void samr_usergroups_recv_groups(struct rpc_request *req)
{
	struct samr_getuserdomgroups_state *state =
		talloc_get_type(req->async.private_data,
				struct samr_getuserdomgroups_state);

	state->ctx->status = dcerpc_ndr_request_recv(req);
	if (!composite_is_ok(state->ctx)) return;
	state->ctx->status = state->g.out.result;
	if (!composite_is_ok(state->ctx)) return;

	state->c.in.handle = state->user_handle;
	state->c.out.handle = state->user_handle;

	req = dcerpc_samr_Close_send(state->samr_pipe, state, &state->c);
	composite_continue_rpc(state->ctx, req, samr_usergroups_recv_close,
			       state);
}

static void samr_usergroups_recv_close(struct rpc_request *req)
{
        struct samr_getuserdomgroups_state *state =
                talloc_get_type(req->async.private_data,
                                struct samr_getuserdomgroups_state);

        state->ctx->status = dcerpc_ndr_request_recv(req);
        if (!composite_is_ok(state->ctx)) return;
        state->ctx->status = state->c.out.result;
        if (!composite_is_ok(state->ctx)) return;

	composite_done(state->ctx);
}

NTSTATUS wb_samr_userdomgroups_recv(struct composite_context *ctx,
				    TALLOC_CTX *mem_ctx,
				    int *num_rids, uint32_t **rids)
{
        struct samr_getuserdomgroups_state *state =
                talloc_get_type(ctx->private_data,
                                struct samr_getuserdomgroups_state);

	int i;
	NTSTATUS status = composite_wait(ctx);
	if (!NT_STATUS_IS_OK(status)) goto done;

	*num_rids = state->rid_array->count;
	*rids = talloc_array(mem_ctx, uint32_t, *num_rids);
	if (*rids == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}

	for (i=0; i<*num_rids; i++) {
		(*rids)[i] = state->rid_array->rids[i].rid;
	}

 done:
	talloc_free(ctx);
	return status;
}
