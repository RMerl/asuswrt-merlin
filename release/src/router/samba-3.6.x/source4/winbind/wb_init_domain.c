/* 
   Unix SMB/CIFS implementation.

   A composite API for initializing a domain

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
#include "winbind/wb_server.h"
#include "smbd/service_task.h"
#include "librpc/gen_ndr/ndr_netlogon.h"
#include "librpc/gen_ndr/ndr_lsa_c.h"
#include "librpc/gen_ndr/ndr_samr_c.h"
#include "libcli/libcli.h"

#include "libcli/security/security.h"


#include "auth/credentials/credentials.h"
#include "param/param.h"

/*
 * Initialize a domain:
 *
 * - With schannel credentials, try to open the SMB connection and
 *   NETLOGON pipe with the machine creds. This works against W2k3SP1
 *   with an NTLMSSP session setup. Fall back to anonymous (for the CIFS level).
 *
 * - If we have schannel creds, do the auth2 and open the schannel'ed netlogon
 *   pipe.
 *
 * - Open LSA. If we have machine creds, try to open with SPNEGO or NTLMSSP. Fall back
 *   to schannel.
 *
 * - With queryinfopolicy, verify that we're talking to the right domain
 *
 * A bit complex, but with all the combinations I think it's the best we can
 * get. NT4, W2k3 and W2k all have different combinations, but in the end we
 * have a signed&sealed lsa connection on all of them.
 *
 * Not sure if it is overkill, but it seems to work.
 */

struct init_domain_state {
	struct composite_context *ctx;
	struct wbsrv_domain *domain;
	struct wbsrv_service *service;

	struct lsa_ObjectAttribute objectattr;
	struct lsa_OpenPolicy2 lsa_openpolicy;
	struct lsa_QueryInfoPolicy queryinfo;
	union lsa_PolicyInformation *info;
};

static void init_domain_recv_netlogonpipe(struct composite_context *ctx);
static void init_domain_recv_lsa_pipe(struct composite_context *ctx);
static void init_domain_recv_lsa_policy(struct tevent_req *subreq);
static void init_domain_recv_queryinfo(struct tevent_req *subreq);
static void init_domain_recv_samr(struct composite_context *ctx);

static struct dcerpc_binding *init_domain_binding(struct init_domain_state *state, 
						  const struct ndr_interface_table *table) 
{
	struct dcerpc_binding *binding;
	NTSTATUS status;

	/* Make a binding string */
	{
		char *s = talloc_asprintf(state, "ncacn_np:%s", state->domain->dc_name);
		if (s == NULL) return NULL;
		status = dcerpc_parse_binding(state, s, &binding);
		talloc_free(s);
		if (!NT_STATUS_IS_OK(status)) {
			return NULL;
		}
	}

	/* Alter binding to contain hostname, but also address (so we don't look it up twice) */
	binding->target_hostname = state->domain->dc_name;
	binding->host = state->domain->dc_address;

	/* This shouldn't make a network call, as the mappings for named pipes are well known */
	status = dcerpc_epm_map_binding(binding, binding, table, state->service->task->event_ctx,
					state->service->task->lp_ctx);
	if (!NT_STATUS_IS_OK(status)) {
		return NULL;
	}

	return binding;
}

struct composite_context *wb_init_domain_send(TALLOC_CTX *mem_ctx,
					      struct wbsrv_service *service,
					      struct wb_dom_info *dom_info)
{
	struct composite_context *result, *ctx;
	struct init_domain_state *state;

	result = composite_create(mem_ctx, service->task->event_ctx);
	if (result == NULL) goto failed;

	state = talloc_zero(result, struct init_domain_state);
	if (state == NULL) goto failed;
	state->ctx = result;
	result->private_data = state;

	state->service = service;

	state->domain = talloc(state, struct wbsrv_domain);
	if (state->domain == NULL) goto failed;

	state->domain->info = talloc_reference(state->domain, dom_info);
	if (state->domain->info == NULL) goto failed;

	state->domain->dc_name = dom_info->dc->name;
	state->domain->dc_address = dom_info->dc->address;

	state->domain->libnet_ctx = libnet_context_init(service->task->event_ctx, 
							service->task->lp_ctx);

	/* Create a credentials structure */
	state->domain->libnet_ctx->cred = cli_credentials_init(state->domain);
	if (state->domain->libnet_ctx->cred == NULL) goto failed;

	cli_credentials_set_conf(state->domain->libnet_ctx->cred, service->task->lp_ctx);

	/* Connect the machine account to the credentials */
	state->ctx->status =
		cli_credentials_set_machine_account(state->domain->libnet_ctx->cred, state->domain->libnet_ctx->lp_ctx);
	if (!NT_STATUS_IS_OK(state->ctx->status)) goto failed;

	state->domain->netlogon_binding = init_domain_binding(state, &ndr_table_netlogon);

	state->domain->netlogon_pipe = NULL;

	if ((!cli_credentials_is_anonymous(state->domain->libnet_ctx->cred)) &&
	    ((lpcfg_server_role(service->task->lp_ctx) == ROLE_DOMAIN_MEMBER) ||
	     (lpcfg_server_role(service->task->lp_ctx) == ROLE_DOMAIN_CONTROLLER)) &&
	    (dom_sid_equal(state->domain->info->sid,
			   state->service->primary_sid))) {
		state->domain->netlogon_binding->flags |= DCERPC_SCHANNEL;

		/* For debugging, it can be a real pain if all the traffic is encrypted */
		if (lpcfg_winbind_sealed_pipes(service->task->lp_ctx)) {
			state->domain->netlogon_binding->flags |= (DCERPC_SIGN | DCERPC_SEAL );
		} else {
			state->domain->netlogon_binding->flags |= (DCERPC_SIGN);
		}
	}

	/* No encryption on anonymous pipes */

	ctx = dcerpc_pipe_connect_b_send(state, state->domain->netlogon_binding, 
					 &ndr_table_netlogon,
					 state->domain->libnet_ctx->cred,
					 service->task->event_ctx,
					 service->task->lp_ctx);
	
	if (composite_nomem(ctx, state->ctx)) {
		goto failed;
	}
	
	composite_continue(state->ctx, ctx, init_domain_recv_netlogonpipe,
			   state);
	return result;
 failed:
	talloc_free(result);
	return NULL;
}

/* Having make a netlogon connection (possibly secured with schannel),
 * make an LSA connection to the same DC, on the same IPC$ share */
static void init_domain_recv_netlogonpipe(struct composite_context *ctx)
{
	struct init_domain_state *state =
		talloc_get_type(ctx->async.private_data,
				struct init_domain_state);

	state->ctx->status = dcerpc_pipe_connect_b_recv(ctx, state->domain, 
						   &state->domain->netlogon_pipe);
	
	if (!composite_is_ok(state->ctx)) {
		return;
	}
	talloc_reparent(state, state->domain->netlogon_pipe, state->domain->netlogon_binding);

	state->domain->lsa_binding = init_domain_binding(state, &ndr_table_lsarpc);

	/* For debugging, it can be a real pain if all the traffic is encrypted */
	if (lpcfg_winbind_sealed_pipes(state->service->task->lp_ctx)) {
		state->domain->lsa_binding->flags |= (DCERPC_SIGN | DCERPC_SEAL );
	} else {
		state->domain->lsa_binding->flags |= (DCERPC_SIGN);
	}

	state->domain->libnet_ctx->lsa.pipe = NULL;

	/* this will make the secondary connection on the same IPC$ share, 
	   secured with SPNEGO or NTLMSSP */
	ctx = dcerpc_secondary_auth_connection_send(state->domain->netlogon_pipe,
						    state->domain->lsa_binding,
						    &ndr_table_lsarpc,
						    state->domain->libnet_ctx->cred,
						    state->domain->libnet_ctx->lp_ctx
		);
	composite_continue(state->ctx, ctx, init_domain_recv_lsa_pipe, state);
}

static bool retry_with_schannel(struct init_domain_state *state, 
				struct dcerpc_binding *binding,
				const struct ndr_interface_table *table,
				void (*continuation)(struct composite_context *))
{
	struct composite_context *ctx;
	state->ctx->status = NT_STATUS_OK;
	if (state->domain->netlogon_binding->flags & DCERPC_SCHANNEL 
	    && !(binding->flags & DCERPC_SCHANNEL)) {
		/* Opening a policy handle failed, perhaps it was
		 * because we don't get a 'wrong password' error on
		 * NTLMSSP binds */

		/* Try again with schannel */
		binding->flags |= DCERPC_SCHANNEL;

		/* Try again, likewise on the same IPC$ share, 
		   secured with SCHANNEL */
		ctx = dcerpc_secondary_auth_connection_send(state->domain->netlogon_pipe,
							    binding,
							    table, 
							    state->domain->libnet_ctx->cred,
							    state->domain->libnet_ctx->lp_ctx);
		composite_continue(state->ctx, ctx, continuation, state);		
		return true;
	} else {
		return false;
	}
}
/* We should now have either an authenticated LSA pipe, or an error.  
 * On success, open a policy handle
 */	
static void init_domain_recv_lsa_pipe(struct composite_context *ctx)
{
	struct init_domain_state *state =
		talloc_get_type(ctx->async.private_data,
				struct init_domain_state);
	struct tevent_req *subreq;

	state->ctx->status = dcerpc_secondary_auth_connection_recv(ctx, state->domain,
								   &state->domain->libnet_ctx->lsa.pipe);
	if (NT_STATUS_EQUAL(state->ctx->status, NT_STATUS_LOGON_FAILURE)) {
		if (retry_with_schannel(state, state->domain->lsa_binding, 
					&ndr_table_lsarpc,
					init_domain_recv_lsa_pipe)) {
			return;
		}
	}
	if (!composite_is_ok(state->ctx)) return;

	talloc_steal(state->domain->libnet_ctx, state->domain->libnet_ctx->lsa.pipe);
	talloc_reparent(state, state->domain->libnet_ctx->lsa.pipe, state->domain->lsa_binding);
	state->domain->libnet_ctx->lsa.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	state->domain->libnet_ctx->lsa.name = state->domain->info->name;

	ZERO_STRUCT(state->domain->libnet_ctx->lsa.handle);
	state->lsa_openpolicy.in.system_name =
		talloc_asprintf(state, "\\\\%s",
				dcerpc_server_name(state->domain->libnet_ctx->lsa.pipe));
	ZERO_STRUCT(state->objectattr);
	state->lsa_openpolicy.in.attr = &state->objectattr;
	state->lsa_openpolicy.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	state->lsa_openpolicy.out.handle = &state->domain->libnet_ctx->lsa.handle;

	subreq = dcerpc_lsa_OpenPolicy2_r_send(state,
					       state->ctx->event_ctx,
					       state->domain->libnet_ctx->lsa.pipe->binding_handle,
					       &state->lsa_openpolicy);
	if (composite_nomem(subreq, state->ctx)) return;
	tevent_req_set_callback(subreq, init_domain_recv_lsa_policy, state);
}

/* Receive a policy handle (or not, and retry the authentication) and
 * obtain some basic information about the domain */

static void init_domain_recv_lsa_policy(struct tevent_req *subreq)
{
	struct init_domain_state *state =
		tevent_req_callback_data(subreq,
		struct init_domain_state);

	state->ctx->status = dcerpc_lsa_OpenPolicy2_r_recv(subreq, state);
	TALLOC_FREE(subreq);
	if ((!NT_STATUS_IS_OK(state->ctx->status)
	      || !NT_STATUS_IS_OK(state->lsa_openpolicy.out.result))) {
		if (retry_with_schannel(state, state->domain->lsa_binding, 
					&ndr_table_lsarpc,
					init_domain_recv_lsa_pipe)) {
			return;
		}
	}
	if (!composite_is_ok(state->ctx)) return;
	state->ctx->status = state->lsa_openpolicy.out.result;
	if (!composite_is_ok(state->ctx)) return;

	state->info = talloc_zero(state->ctx, union lsa_PolicyInformation);
	if (composite_nomem(state->info, state->ctx)) return;

	state->queryinfo.in.handle = &state->domain->libnet_ctx->lsa.handle;
	state->queryinfo.in.level = LSA_POLICY_INFO_ACCOUNT_DOMAIN;
	state->queryinfo.out.info = &state->info;

	subreq = dcerpc_lsa_QueryInfoPolicy_r_send(state,
						   state->ctx->event_ctx,
						   state->domain->libnet_ctx->lsa.pipe->binding_handle,
						   &state->queryinfo);
	if (composite_nomem(subreq, state->ctx)) return;
	tevent_req_set_callback(subreq, init_domain_recv_queryinfo, state);
}

static void init_domain_recv_queryinfo(struct tevent_req *subreq)
{
	struct init_domain_state *state =
		tevent_req_callback_data(subreq,
		struct init_domain_state);
	struct lsa_DomainInfo *dominfo;
	struct composite_context *ctx;

	state->ctx->status = dcerpc_lsa_QueryInfoPolicy_r_recv(subreq, state);
	TALLOC_FREE(subreq);
	if (!composite_is_ok(state->ctx)) return;
	state->ctx->status = state->queryinfo.out.result;
	if (!composite_is_ok(state->ctx)) return;

	dominfo = &(*state->queryinfo.out.info)->account_domain;

	if (strcasecmp(state->domain->info->name, dominfo->name.string) != 0) {
		DEBUG(2, ("Expected domain name %s, DC %s said %s\n",
			  state->domain->info->name,
			  dcerpc_server_name(state->domain->libnet_ctx->lsa.pipe),
			  dominfo->name.string));
		composite_error(state->ctx, NT_STATUS_INVALID_DOMAIN_STATE);
		return;
	}

	if (!dom_sid_equal(state->domain->info->sid, dominfo->sid)) {
		DEBUG(2, ("Expected domain sid %s, DC %s said %s\n",
			  dom_sid_string(state, state->domain->info->sid),
			  dcerpc_server_name(state->domain->libnet_ctx->lsa.pipe),
			  dom_sid_string(state, dominfo->sid)));
		composite_error(state->ctx, NT_STATUS_INVALID_DOMAIN_STATE);
		return;
	}

	state->domain->samr_binding = init_domain_binding(state, &ndr_table_samr);

	/* We want to use the same flags as the LSA pipe did (so, if
	 * it needed schannel, then we need that here too) */
	state->domain->samr_binding->flags = state->domain->lsa_binding->flags;

	state->domain->libnet_ctx->samr.pipe = NULL;

	ctx = wb_connect_samr_send(state, state->domain);
	composite_continue(state->ctx, ctx, init_domain_recv_samr, state);
}

/* Recv the SAMR details (SamrConnect and SamrOpenDomain handle) and
 * open an LDAP connection */
static void init_domain_recv_samr(struct composite_context *ctx)
{
	struct init_domain_state *state =
		talloc_get_type(ctx->async.private_data,
				struct init_domain_state);

	state->ctx->status = wb_connect_samr_recv(
		ctx, state->domain,
		&state->domain->libnet_ctx->samr.pipe,
		&state->domain->libnet_ctx->samr.connect_handle,
		&state->domain->libnet_ctx->samr.handle);
	if (!composite_is_ok(state->ctx)) return;

	talloc_reparent(state, state->domain->libnet_ctx->samr.pipe, state->domain->samr_binding);
	state->domain->libnet_ctx->samr.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	state->domain->libnet_ctx->samr.name = state->domain->info->name;
	state->domain->libnet_ctx->samr.sid = dom_sid_dup(
						state->domain->libnet_ctx,
						state->domain->info->sid);

	composite_done(state->ctx);
}

NTSTATUS wb_init_domain_recv(struct composite_context *c,
			     TALLOC_CTX *mem_ctx,
			     struct wbsrv_domain **result)
{
	NTSTATUS status = composite_wait(c);
	if (NT_STATUS_IS_OK(status)) {
		struct init_domain_state *state =
			talloc_get_type(c->private_data,
					struct init_domain_state);
		*result = talloc_steal(mem_ctx, state->domain);
	}
	talloc_free(c);
	return status;
}

NTSTATUS wb_init_domain(TALLOC_CTX *mem_ctx, struct wbsrv_service *service,
			struct wb_dom_info *dom_info,
			struct wbsrv_domain **result)
{
	struct composite_context *c =
		wb_init_domain_send(mem_ctx, service, dom_info);
	return wb_init_domain_recv(c, mem_ctx, result);
}
