/* 
   Unix SMB/CIFS implementation.

   dcerpc schannel operations

   Copyright (C) Andrew Tridgell 2004
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2004-2005
   Copyright (C) Rafal Szczesniak 2006

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
#include <tevent.h>
#include "auth/auth.h"
#include "libcli/composite/composite.h"
#include "libcli/auth/libcli_auth.h"
#include "librpc/gen_ndr/ndr_netlogon.h"
#include "librpc/gen_ndr/ndr_netlogon_c.h"
#include "auth/credentials/credentials.h"
#include "librpc/rpc/dcerpc_proto.h"
#include "param/param.h"

struct schannel_key_state {
	struct dcerpc_pipe *pipe;
	struct dcerpc_pipe *pipe2;
	struct dcerpc_binding *binding;
	struct cli_credentials *credentials;
	struct netlogon_creds_CredentialState *creds;
	uint32_t negotiate_flags;
	struct netr_Credential credentials1;
	struct netr_Credential credentials2;
	struct netr_Credential credentials3;
	struct netr_ServerReqChallenge r;
	struct netr_ServerAuthenticate2 a;
	const struct samr_Password *mach_pwd;
};


static void continue_secondary_connection(struct composite_context *ctx);
static void continue_bind_auth_none(struct composite_context *ctx);
static void continue_srv_challenge(struct tevent_req *subreq);
static void continue_srv_auth2(struct tevent_req *subreq);


/*
  Stage 2 of schannel_key: Receive endpoint mapping and request secondary
  rpc connection
*/
static void continue_epm_map_binding(struct composite_context *ctx)
{
	struct composite_context *c;
	struct schannel_key_state *s;
	struct composite_context *sec_conn_req;

	c = talloc_get_type(ctx->async.private_data, struct composite_context);
	s = talloc_get_type(c->private_data, struct schannel_key_state);

	/* receive endpoint mapping */
	c->status = dcerpc_epm_map_binding_recv(ctx);
	if (!NT_STATUS_IS_OK(c->status)) {
		DEBUG(0,("Failed to map DCERPC/TCP NCACN_NP pipe for '%s' - %s\n",
			 NDR_NETLOGON_UUID, nt_errstr(c->status)));
		composite_error(c, c->status);
		return;
	}

	/* send a request for secondary rpc connection */
	sec_conn_req = dcerpc_secondary_connection_send(s->pipe,
							s->binding);
	if (composite_nomem(sec_conn_req, c)) return;

	composite_continue(c, sec_conn_req, continue_secondary_connection, c);
}


/*
  Stage 3 of schannel_key: Receive secondary rpc connection and perform
  non-authenticated bind request
*/
static void continue_secondary_connection(struct composite_context *ctx)
{
	struct composite_context *c;
	struct schannel_key_state *s;
	struct composite_context *auth_none_req;

	c = talloc_get_type(ctx->async.private_data, struct composite_context);
	s = talloc_get_type(c->private_data, struct schannel_key_state);

	/* receive secondary rpc connection */
	c->status = dcerpc_secondary_connection_recv(ctx, &s->pipe2);
	if (!composite_is_ok(c)) return;

	talloc_steal(s, s->pipe2);

	/* initiate a non-authenticated bind */
	auth_none_req = dcerpc_bind_auth_none_send(c, s->pipe2, &ndr_table_netlogon);
	if (composite_nomem(auth_none_req, c)) return;

	composite_continue(c, auth_none_req, continue_bind_auth_none, c);
}


/*
  Stage 4 of schannel_key: Receive non-authenticated bind and get
  a netlogon challenge
*/
static void continue_bind_auth_none(struct composite_context *ctx)
{
	struct composite_context *c;
	struct schannel_key_state *s;
	struct tevent_req *subreq;

	c = talloc_get_type(ctx->async.private_data, struct composite_context);
	s = talloc_get_type(c->private_data, struct schannel_key_state);

	/* receive result of non-authenticated bind request */
	c->status = dcerpc_bind_auth_none_recv(ctx);
	if (!composite_is_ok(c)) return;
	
	/* prepare a challenge request */
	s->r.in.server_name   = talloc_asprintf(c, "\\\\%s", dcerpc_server_name(s->pipe));
	if (composite_nomem(s->r.in.server_name, c)) return;
	s->r.in.computer_name = cli_credentials_get_workstation(s->credentials);
	s->r.in.credentials   = &s->credentials1;
	s->r.out.return_credentials  = &s->credentials2;
	
	generate_random_buffer(s->credentials1.data, sizeof(s->credentials1.data));

	/*
	  request a netlogon challenge - a rpc request over opened secondary pipe
	*/
	subreq = dcerpc_netr_ServerReqChallenge_r_send(s, c->event_ctx,
						       s->pipe2->binding_handle,
						       &s->r);
	if (composite_nomem(subreq, c)) return;

	tevent_req_set_callback(subreq, continue_srv_challenge, c);
}


/*
  Stage 5 of schannel_key: Receive a challenge and perform authentication
  on the netlogon pipe
*/
static void continue_srv_challenge(struct tevent_req *subreq)
{
	struct composite_context *c;
	struct schannel_key_state *s;

	c = tevent_req_callback_data(subreq, struct composite_context);
	s = talloc_get_type(c->private_data, struct schannel_key_state);

	/* receive rpc request result - netlogon challenge */
	c->status = dcerpc_netr_ServerReqChallenge_r_recv(subreq, s);
	TALLOC_FREE(subreq);
	if (!composite_is_ok(c)) return;

	/* prepare credentials for auth2 request */
	s->mach_pwd = cli_credentials_get_nt_hash(s->credentials, c);

	/* auth2 request arguments */
	s->a.in.server_name      = s->r.in.server_name;
	s->a.in.account_name     = cli_credentials_get_username(s->credentials);
	s->a.in.secure_channel_type =
		cli_credentials_get_secure_channel_type(s->credentials);
	s->a.in.computer_name    = cli_credentials_get_workstation(s->credentials);
	s->a.in.negotiate_flags  = &s->negotiate_flags;
	s->a.in.credentials      = &s->credentials3;
	s->a.out.negotiate_flags = &s->negotiate_flags;
	s->a.out.return_credentials     = &s->credentials3;

	s->creds = netlogon_creds_client_init(s, 
					      s->a.in.account_name, 
					      s->a.in.computer_name,
					      &s->credentials1, &s->credentials2,
					      s->mach_pwd, &s->credentials3, s->negotiate_flags);
	if (composite_nomem(s->creds, c)) {
		return;
	}
	/*
	  authenticate on the netlogon pipe - a rpc request over secondary pipe
	*/
	subreq = dcerpc_netr_ServerAuthenticate2_r_send(s, c->event_ctx,
							s->pipe2->binding_handle,
							&s->a);
	if (composite_nomem(subreq, c)) return;

	tevent_req_set_callback(subreq, continue_srv_auth2, c);
}


/*
  Stage 6 of schannel_key: Receive authentication request result and verify
  received credentials
*/
static void continue_srv_auth2(struct tevent_req *subreq)
{
	struct composite_context *c;
	struct schannel_key_state *s;

	c = tevent_req_callback_data(subreq, struct composite_context);
	s = talloc_get_type(c->private_data, struct schannel_key_state);

	/* receive rpc request result - auth2 credentials */ 
	c->status = dcerpc_netr_ServerAuthenticate2_r_recv(subreq, s);
	TALLOC_FREE(subreq);
	if (!composite_is_ok(c)) return;

	/* verify credentials */
	if (!netlogon_creds_client_check(s->creds, s->a.out.return_credentials)) {
		composite_error(c, NT_STATUS_UNSUCCESSFUL);
		return;
	}

	/* setup current netlogon credentials */
	cli_credentials_set_netlogon_creds(s->credentials, s->creds);

	composite_done(c);
}


/*
  Initiate establishing a schannel key using netlogon challenge
  on a secondary pipe
*/
struct composite_context *dcerpc_schannel_key_send(TALLOC_CTX *mem_ctx,
						   struct dcerpc_pipe *p,
						   struct cli_credentials *credentials,
						   struct loadparm_context *lp_ctx)
{
	struct composite_context *c;
	struct schannel_key_state *s;
	struct composite_context *epm_map_req;
	enum netr_SchannelType schannel_type = cli_credentials_get_secure_channel_type(credentials);
	
	/* composite context allocation and setup */
	c = composite_create(mem_ctx, p->conn->event_ctx);
	if (c == NULL) return NULL;

	s = talloc_zero(c, struct schannel_key_state);
	if (composite_nomem(s, c)) return c;
	c->private_data = s;

	/* store parameters in the state structure */
	s->pipe        = p;
	s->credentials = credentials;

	/* allocate credentials */
	/* type of authentication depends on schannel type */
	if (schannel_type == SEC_CHAN_RODC) {
		s->negotiate_flags = NETLOGON_NEG_AUTH2_RODC_FLAGS;
	} else if (s->pipe->conn->flags & DCERPC_SCHANNEL_128) {
		s->negotiate_flags = NETLOGON_NEG_AUTH2_ADS_FLAGS;
	} else {
		s->negotiate_flags = NETLOGON_NEG_AUTH2_FLAGS;
	}

	/* allocate binding structure */
	s->binding = talloc_zero(c, struct dcerpc_binding);
	if (composite_nomem(s->binding, c)) return c;

	*s->binding = *s->pipe->binding;

	/* request the netlogon endpoint mapping */
	epm_map_req = dcerpc_epm_map_binding_send(c, s->binding,
						  &ndr_table_netlogon,
						  s->pipe->conn->event_ctx,
						  lp_ctx);
	if (composite_nomem(epm_map_req, c)) return c;

	composite_continue(c, epm_map_req, continue_epm_map_binding, c);
	return c;
}


/*
  Receive result of schannel key request
 */
NTSTATUS dcerpc_schannel_key_recv(struct composite_context *c)
{
	NTSTATUS status = composite_wait(c);
	
	talloc_free(c);
	return status;
}


struct auth_schannel_state {
	struct dcerpc_pipe *pipe;
	struct cli_credentials *credentials;
	const struct ndr_interface_table *table;
	struct loadparm_context *lp_ctx;
	uint8_t auth_level;
};


static void continue_bind_auth(struct composite_context *ctx);


/*
  Stage 2 of auth_schannel: Receive schannel key and intitiate an
  authenticated bind using received credentials
 */
static void continue_schannel_key(struct composite_context *ctx)
{
	struct composite_context *auth_req;
	struct composite_context *c = talloc_get_type(ctx->async.private_data,
						      struct composite_context);
	struct auth_schannel_state *s = talloc_get_type(c->private_data,
							struct auth_schannel_state);
	NTSTATUS status;

	/* receive schannel key */
	status = c->status = dcerpc_schannel_key_recv(ctx);
	if (!composite_is_ok(c)) {
		DEBUG(1, ("Failed to setup credentials: %s\n", nt_errstr(status)));
		return;
	}

	/* send bind auth request with received creds */
	auth_req = dcerpc_bind_auth_send(c, s->pipe, s->table, s->credentials, 
					 lpcfg_gensec_settings(c, s->lp_ctx),
					 DCERPC_AUTH_TYPE_SCHANNEL, s->auth_level,
					 NULL);
	if (composite_nomem(auth_req, c)) return;
	
	composite_continue(c, auth_req, continue_bind_auth, c);
}


/*
  Stage 3 of auth_schannel: Receivce result of authenticated bind
  and say if we're done ok.
*/
static void continue_bind_auth(struct composite_context *ctx)
{
	struct composite_context *c = talloc_get_type(ctx->async.private_data,
						      struct composite_context);

	c->status = dcerpc_bind_auth_recv(ctx);
	if (!composite_is_ok(c)) return;

	composite_done(c);
}


/*
  Initiate schannel authentication request
*/
struct composite_context *dcerpc_bind_auth_schannel_send(TALLOC_CTX *tmp_ctx, 
							 struct dcerpc_pipe *p,
							 const struct ndr_interface_table *table,
							 struct cli_credentials *credentials,
							 struct loadparm_context *lp_ctx,
							 uint8_t auth_level)
{
	struct composite_context *c;
	struct auth_schannel_state *s;
	struct composite_context *schan_key_req;

	/* composite context allocation and setup */
	c = composite_create(tmp_ctx, p->conn->event_ctx);
	if (c == NULL) return NULL;
	
	s = talloc_zero(c, struct auth_schannel_state);
	if (composite_nomem(s, c)) return c;
	c->private_data = s;

	/* store parameters in the state structure */
	s->pipe        = p;
	s->credentials = credentials;
	s->table       = table;
	s->auth_level  = auth_level;
	s->lp_ctx      = lp_ctx;

	/* start getting schannel key first */
	schan_key_req = dcerpc_schannel_key_send(c, p, credentials, lp_ctx);
	if (composite_nomem(schan_key_req, c)) return c;

	composite_continue(c, schan_key_req, continue_schannel_key, c);
	return c;
}


/*
  Receive result of schannel authentication request
*/
NTSTATUS dcerpc_bind_auth_schannel_recv(struct composite_context *c)
{
	NTSTATUS status = composite_wait(c);
	
	talloc_free(c);
	return status;
}


/*
  Perform schannel authenticated bind - sync version
 */
_PUBLIC_ NTSTATUS dcerpc_bind_auth_schannel(TALLOC_CTX *tmp_ctx, 
				   struct dcerpc_pipe *p,
				   const struct ndr_interface_table *table,
				   struct cli_credentials *credentials,
				   struct loadparm_context *lp_ctx,
				   uint8_t auth_level)
{
	struct composite_context *c;

	c = dcerpc_bind_auth_schannel_send(tmp_ctx, p, table, credentials, lp_ctx,
					   auth_level);
	return dcerpc_bind_auth_schannel_recv(c);
}
