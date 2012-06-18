/* 
   Unix SMB/CIFS implementation.

   Connect to the LSA pipe, given an smbcli_tree and possibly some
   credentials. Try ntlmssp, schannel and anon in that order.

   Copyright (C) Volker Lendecke 2005
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2007
 
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

#include "libcli/raw/libcliraw.h"
#include "librpc/gen_ndr/ndr_lsa_c.h"
#include "winbind/wb_server.h"

/* Helper to initialize LSA with a specific auth methods. Verify by opening
 * the LSA policy. */

struct init_lsa_state {
	struct composite_context *ctx;
	struct dcerpc_pipe *lsa_pipe;

	uint8_t auth_type;
	struct cli_credentials *creds;

	struct lsa_ObjectAttribute objectattr;
	struct lsa_OpenPolicy2 openpolicy;
	struct policy_handle *handle;
};

static void init_lsa_recv_pipe(struct composite_context *ctx);
static void init_lsa_recv_openpol(struct rpc_request *req);

struct composite_context *wb_init_lsa_send(TALLOC_CTX *mem_ctx,
					   struct wbsrv_domain *domain)
{
	struct composite_context *result, *ctx;
	struct init_lsa_state *state;

	result = composite_create(mem_ctx, domain->netlogon_pipe->conn->event_ctx);
	if (result == NULL) goto failed;

	state = talloc(result, struct init_lsa_state);
	if (state == NULL) goto failed;
	state->ctx = result;
	result->private_data = state;

	/* this will make the secondary connection on the same IPC$ share, 
	   secured with SPNEGO or NTLMSSP */
	ctx = dcerpc_secondary_auth_connection_send(domain->netlogon_pipe,
						    domain->lsa_binding,
						    &ndr_table_lsarpc,
						    domain->libnet_ctx->cred,
						    domain->libnet_ctx->lp_ctx);
	composite_continue(state->ctx, ctx, init_lsa_recv_pipe, state);
	return result;
	
 failed:
	talloc_free(result);
	return NULL;
}

static void init_lsa_recv_pipe(struct composite_context *ctx)
{
	struct rpc_request *req;
	struct init_lsa_state *state =
		talloc_get_type(ctx->async.private_data,
				struct init_lsa_state);

	state->ctx->status = dcerpc_secondary_auth_connection_recv(ctx, state,
								   &state->lsa_pipe);
	if (!composite_is_ok(state->ctx)) return;
			
	state->handle = talloc(state, struct policy_handle);
	if (composite_nomem(state->handle, state->ctx)) return;

	state->openpolicy.in.system_name =
		talloc_asprintf(state, "\\\\%s",
				dcerpc_server_name(state->lsa_pipe));
	ZERO_STRUCT(state->objectattr);
	state->openpolicy.in.attr = &state->objectattr;
	state->openpolicy.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	state->openpolicy.out.handle = state->handle;

	req = dcerpc_lsa_OpenPolicy2_send(state->lsa_pipe, state,
					  &state->openpolicy);
	composite_continue_rpc(state->ctx, req, init_lsa_recv_openpol, state);
}

static void init_lsa_recv_openpol(struct rpc_request *req)
{
	struct init_lsa_state *state =
		talloc_get_type(req->async.private_data,
				struct init_lsa_state);

	state->ctx->status = dcerpc_ndr_request_recv(req);
	if (!composite_is_ok(state->ctx)) return;
	state->ctx->status = state->openpolicy.out.result;
	if (!composite_is_ok(state->ctx)) return;

	composite_done(state->ctx);
}

NTSTATUS wb_init_lsa_recv(struct composite_context *c,
			  TALLOC_CTX *mem_ctx,
			  struct dcerpc_pipe **lsa_pipe,
			  struct policy_handle **lsa_policy)
{
	NTSTATUS status = composite_wait(c);
	if (NT_STATUS_IS_OK(status)) {
		struct init_lsa_state *state =
			talloc_get_type(c->private_data,
					struct init_lsa_state);
		*lsa_pipe = talloc_steal(mem_ctx, state->lsa_pipe);
		*lsa_policy = talloc_steal(mem_ctx, state->handle);
	}
	talloc_free(c);
	return status;
}

