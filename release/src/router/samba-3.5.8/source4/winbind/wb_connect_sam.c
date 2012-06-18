/* 
   Unix SMB/CIFS implementation.

   Connect to the SAMR pipe, and return connection and domain handles.

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
#include "libcli/security/security.h"
#include "librpc/gen_ndr/ndr_samr_c.h"
#include "winbind/wb_server.h"


/* Helper to initialize SAMR with a specific auth methods. Verify by opening
 * the SAM handle */

struct connect_samr_state {
	struct composite_context *ctx;
	struct dom_sid *sid;

	struct dcerpc_pipe *samr_pipe;
	struct policy_handle *connect_handle;
	struct policy_handle *domain_handle;

	struct samr_Connect2 c;
	struct samr_OpenDomain o;
};

static void connect_samr_recv_pipe(struct composite_context *ctx);
static void connect_samr_recv_conn(struct rpc_request *req);
static void connect_samr_recv_open(struct rpc_request *req);

struct composite_context *wb_connect_samr_send(TALLOC_CTX *mem_ctx,
					   struct wbsrv_domain *domain)
{
	struct composite_context *result, *ctx;
	struct connect_samr_state *state;

	result = composite_create(mem_ctx, domain->netlogon_pipe->conn->event_ctx);
	if (result == NULL) goto failed;

	state = talloc(result, struct connect_samr_state);
	if (state == NULL) goto failed;
	state->ctx = result;
	result->private_data = state;

	state->sid = dom_sid_dup(state, domain->info->sid);
	if (state->sid == NULL) goto failed;

	/* this will make the secondary connection on the same IPC$ share, 
	   secured with SPNEGO, NTLMSSP or SCHANNEL */
	ctx = dcerpc_secondary_auth_connection_send(domain->netlogon_pipe,
						    domain->samr_binding,
						    &ndr_table_samr,
						    domain->libnet_ctx->cred,
						    domain->libnet_ctx->lp_ctx);
	composite_continue(state->ctx, ctx, connect_samr_recv_pipe, state);
	return result;
	
 failed:
	talloc_free(result);
	return NULL;
}

static void connect_samr_recv_pipe(struct composite_context *ctx)
{
	struct rpc_request *req;
	struct connect_samr_state *state =
		talloc_get_type(ctx->async.private_data,
				struct connect_samr_state);

	state->ctx->status = dcerpc_secondary_auth_connection_recv(ctx, state, 
								   &state->samr_pipe);
	if (!composite_is_ok(state->ctx)) return;
			
	state->connect_handle = talloc(state, struct policy_handle);
	if (composite_nomem(state->connect_handle, state->ctx)) return;

	state->c.in.system_name =
		talloc_asprintf(state, "\\\\%s",
				dcerpc_server_name(state->samr_pipe));
	state->c.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	state->c.out.connect_handle = state->connect_handle;

	req = dcerpc_samr_Connect2_send(state->samr_pipe, state, &state->c);
	composite_continue_rpc(state->ctx, req, connect_samr_recv_conn, state);
	return;
}

static void connect_samr_recv_conn(struct rpc_request *req)
{
	struct connect_samr_state *state =
		talloc_get_type(req->async.private_data,
				struct connect_samr_state);

	state->ctx->status = dcerpc_ndr_request_recv(req);
	if (!composite_is_ok(state->ctx)) return;
	state->ctx->status = state->c.out.result;
	if (!composite_is_ok(state->ctx)) return;

	state->domain_handle = talloc(state, struct policy_handle);
	if (composite_nomem(state->domain_handle, state->ctx)) return;

	state->o.in.connect_handle = state->connect_handle;
	state->o.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	state->o.in.sid = state->sid;
	state->o.out.domain_handle = state->domain_handle;

	req = dcerpc_samr_OpenDomain_send(state->samr_pipe, state, &state->o);
	composite_continue_rpc(state->ctx, req,
			       connect_samr_recv_open, state);
}

static void connect_samr_recv_open(struct rpc_request *req)
{
	struct connect_samr_state *state =
		talloc_get_type(req->async.private_data,
				struct connect_samr_state);

	state->ctx->status = dcerpc_ndr_request_recv(req);
	if (!composite_is_ok(state->ctx)) return;
	state->ctx->status = state->o.out.result;
	if (!composite_is_ok(state->ctx)) return;

	composite_done(state->ctx);
}

NTSTATUS wb_connect_samr_recv(struct composite_context *c,
			     TALLOC_CTX *mem_ctx,
			     struct dcerpc_pipe **samr_pipe,
			     struct policy_handle *connect_handle,
			     struct policy_handle *domain_handle)
{
	NTSTATUS status = composite_wait(c);
	if (NT_STATUS_IS_OK(status)) {
		struct connect_samr_state *state =
			talloc_get_type(c->private_data,
					struct connect_samr_state);
		*samr_pipe = talloc_steal(mem_ctx, state->samr_pipe);
		*connect_handle = *state->connect_handle;
		*domain_handle = *state->domain_handle;
	}
	talloc_free(c);
	return status;
}

