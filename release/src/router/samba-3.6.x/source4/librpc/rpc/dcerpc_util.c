/* 
   Unix SMB/CIFS implementation.

   dcerpc utility functions

   Copyright (C) Andrew Tridgell 2003
   Copyright (C) Jelmer Vernooij 2004
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2005
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
#include "lib/events/events.h"
#include "libcli/composite/composite.h"
#include "librpc/gen_ndr/ndr_epmapper_c.h"
#include "librpc/gen_ndr/ndr_dcerpc.h"
#include "librpc/gen_ndr/ndr_misc.h"
#include "librpc/rpc/dcerpc_proto.h"
#include "auth/credentials/credentials.h"
#include "param/param.h"
#include "librpc/rpc/rpc_common.h"

/*
  find a dcerpc call on an interface by name
*/
const struct ndr_interface_call *dcerpc_iface_find_call(const struct ndr_interface_table *iface,
							const char *name)
{
	int i;
	for (i=0;i<iface->num_calls;i++) {
		if (strcmp(iface->calls[i].name, name) == 0) {
			return &iface->calls[i];
		}
	}
	return NULL;
}

/* 
   push a ncacn_packet into a blob, potentially with auth info
*/
NTSTATUS ncacn_push_auth(DATA_BLOB *blob, TALLOC_CTX *mem_ctx, 
			 struct ncacn_packet *pkt,
			 struct dcerpc_auth *auth_info)
{
	struct ndr_push *ndr;
	enum ndr_err_code ndr_err;

	ndr = ndr_push_init_ctx(mem_ctx);
	if (!ndr) {
		return NT_STATUS_NO_MEMORY;
	}

	if (!(pkt->drep[0] & DCERPC_DREP_LE)) {
		ndr->flags |= LIBNDR_FLAG_BIGENDIAN;
	}

	if (pkt->pfc_flags & DCERPC_PFC_FLAG_OBJECT_UUID) {
		ndr->flags |= LIBNDR_FLAG_OBJECT_PRESENT;
	}

	if (auth_info) {
		pkt->auth_length = auth_info->credentials.length;
	} else {
		pkt->auth_length = 0;
	}

	ndr_err = ndr_push_ncacn_packet(ndr, NDR_SCALARS|NDR_BUFFERS, pkt);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return ndr_map_error2ntstatus(ndr_err);
	}

	if (auth_info) {
#if 0
		/* the s3 rpc server doesn't handle auth padding in
		   bind requests. Use zero auth padding to keep us
		   working with old servers */
		uint32_t offset = ndr->offset;
		ndr_err = ndr_push_align(ndr, 16);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			return ndr_map_error2ntstatus(ndr_err);
		}
		auth_info->auth_pad_length = ndr->offset - offset;
#else
		auth_info->auth_pad_length = 0;
#endif
		ndr_err = ndr_push_dcerpc_auth(ndr, NDR_SCALARS|NDR_BUFFERS, auth_info);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			return ndr_map_error2ntstatus(ndr_err);
		}
	}

	*blob = ndr_push_blob(ndr);

	/* fill in the frag length */
	dcerpc_set_frag_length(blob, blob->length);

	return NT_STATUS_OK;
}


struct epm_map_binding_state {
	struct dcerpc_binding *binding;
	const struct ndr_interface_table *table;
	struct dcerpc_pipe *pipe;
	struct policy_handle handle;
	struct GUID guid;
	struct epm_twr_t twr;
	struct epm_twr_t *twr_r;
	uint32_t num_towers;
	struct epm_Map r;
};


static void continue_epm_recv_binding(struct composite_context *ctx);
static void continue_epm_map(struct tevent_req *subreq);


/*
  Stage 2 of epm_map_binding: Receive connected rpc pipe and send endpoint
  mapping rpc request
*/
static void continue_epm_recv_binding(struct composite_context *ctx)
{
	struct composite_context *c = talloc_get_type(ctx->async.private_data,
						      struct composite_context);
	struct epm_map_binding_state *s = talloc_get_type(c->private_data,
							  struct epm_map_binding_state);
	struct tevent_req *subreq;

	/* receive result of rpc pipe connect request */
	c->status = dcerpc_pipe_connect_b_recv(ctx, c, &s->pipe);
	if (!composite_is_ok(c)) return;

	/* prepare requested binding parameters */
	s->binding->object         = s->table->syntax_id;

	c->status = dcerpc_binding_build_tower(s->pipe, s->binding, &s->twr.tower);
	if (!composite_is_ok(c)) return;
	
	/* with some nice pretty paper around it of course */
	s->r.in.object        = &s->guid;
	s->r.in.map_tower     = &s->twr;
	s->r.in.entry_handle  = &s->handle;
	s->r.in.max_towers    = 1;
	s->r.out.entry_handle = &s->handle;
	s->r.out.num_towers   = &s->num_towers;

	/* send request for an endpoint mapping - a rpc request on connected pipe */
	subreq = dcerpc_epm_Map_r_send(s, c->event_ctx,
				       s->pipe->binding_handle,
				       &s->r);
	if (composite_nomem(subreq, c)) return;
	
	tevent_req_set_callback(subreq, continue_epm_map, c);
}


/*
  Stage 3 of epm_map_binding: Receive endpoint mapping and provide binding details
*/
static void continue_epm_map(struct tevent_req *subreq)
{
	struct composite_context *c = tevent_req_callback_data(subreq,
				      struct composite_context);
	struct epm_map_binding_state *s = talloc_get_type(c->private_data,
							  struct epm_map_binding_state);

	/* receive result of a rpc request */
	c->status = dcerpc_epm_Map_r_recv(subreq, s);
	TALLOC_FREE(subreq);
	if (!composite_is_ok(c)) return;

	/* check the details */
	if (s->r.out.result != 0 || *s->r.out.num_towers != 1) {
		composite_error(c, NT_STATUS_PORT_UNREACHABLE);
		return;
	}
	
	s->twr_r = s->r.out.towers[0].twr;
	if (s->twr_r == NULL) {
		composite_error(c, NT_STATUS_PORT_UNREACHABLE);
		return;
	}

	if (s->twr_r->tower.num_floors != s->twr.tower.num_floors ||
	    s->twr_r->tower.floors[3].lhs.protocol != s->twr.tower.floors[3].lhs.protocol) {
		composite_error(c, NT_STATUS_PORT_UNREACHABLE);
		return;
	}

	/* get received endpoint */
	s->binding->endpoint = talloc_reference(s->binding,
						dcerpc_floor_get_rhs_data(c, &s->twr_r->tower.floors[3]));
	if (composite_nomem(s->binding->endpoint, c)) return;

	composite_done(c);
}


/*
  Request for endpoint mapping of dcerpc binding - try to request for endpoint
  unless there is default one.
*/
struct composite_context *dcerpc_epm_map_binding_send(TALLOC_CTX *mem_ctx,
						      struct dcerpc_binding *binding,
						      const struct ndr_interface_table *table,
						      struct tevent_context *ev,
						      struct loadparm_context *lp_ctx)
{
	struct composite_context *c;
	struct epm_map_binding_state *s;
	struct composite_context *pipe_connect_req;
	struct cli_credentials *anon_creds;

	NTSTATUS status;
	struct dcerpc_binding *epmapper_binding;
	int i;

	if (ev == NULL) {
		return NULL;
	}

	/* composite context allocation and setup */
	c = composite_create(mem_ctx, ev);
	if (c == NULL) {
		return NULL;
	}

	s = talloc_zero(c, struct epm_map_binding_state);
	if (composite_nomem(s, c)) return c;
	c->private_data = s;

	s->binding = binding;
	s->table   = table;

	/* anonymous credentials for rpc connection used to get endpoint mapping */
	anon_creds = cli_credentials_init(mem_ctx);
	cli_credentials_set_anonymous(anon_creds);

	/*
	  First, check if there is a default endpoint specified in the IDL
	*/
	if (table != NULL) {
		struct dcerpc_binding *default_binding;

		/* Find one of the default pipes for this interface */
		for (i = 0; i < table->endpoints->count; i++) {
			status = dcerpc_parse_binding(mem_ctx, table->endpoints->names[i], &default_binding);

			if (NT_STATUS_IS_OK(status)) {
				if (binding->transport == NCA_UNKNOWN) 
					binding->transport = default_binding->transport;
				if (default_binding->transport == binding->transport && 
					default_binding->endpoint) {
					binding->endpoint = talloc_reference(binding, default_binding->endpoint);
					talloc_free(default_binding);

					composite_done(c);
					return c;

				} else {
					talloc_free(default_binding);
				}
			}
		}
	}

	epmapper_binding = talloc_zero(c, struct dcerpc_binding);
	if (composite_nomem(epmapper_binding, c)) return c;

	/* basic endpoint mapping data */
	epmapper_binding->transport		= binding->transport;
	epmapper_binding->host			= talloc_reference(epmapper_binding, binding->host);
	epmapper_binding->target_hostname       = epmapper_binding->host;
	epmapper_binding->options		= NULL;
	epmapper_binding->localaddress          = talloc_reference(epmapper_binding, binding->localaddress);
	epmapper_binding->flags			= 0;
	epmapper_binding->assoc_group_id	= 0;
	epmapper_binding->endpoint		= NULL;

	/* initiate rpc pipe connection */
	pipe_connect_req = dcerpc_pipe_connect_b_send(c, epmapper_binding, 
						      &ndr_table_epmapper,
						      anon_creds, c->event_ctx,
						      lp_ctx);
	if (composite_nomem(pipe_connect_req, c)) return c;
	
	composite_continue(c, pipe_connect_req, continue_epm_recv_binding, c);
	return c;
}


/*
  Receive result of endpoint mapping request
 */
NTSTATUS dcerpc_epm_map_binding_recv(struct composite_context *c)
{
	NTSTATUS status = composite_wait(c);
	
	talloc_free(c);
	return status;
}


/*
  Get endpoint mapping for rpc connection
*/
_PUBLIC_ NTSTATUS dcerpc_epm_map_binding(TALLOC_CTX *mem_ctx, struct dcerpc_binding *binding,
				const struct ndr_interface_table *table, struct tevent_context *ev,
				struct loadparm_context *lp_ctx)
{
	struct composite_context *c;

	c = dcerpc_epm_map_binding_send(mem_ctx, binding, table, ev, lp_ctx);
	return dcerpc_epm_map_binding_recv(c);
}


struct pipe_auth_state {
	struct dcerpc_pipe *pipe;
	struct dcerpc_binding *binding;
	const struct ndr_interface_table *table;
	struct loadparm_context *lp_ctx;
	struct cli_credentials *credentials;
};


static void continue_auth_schannel(struct composite_context *ctx);
static void continue_auth(struct composite_context *ctx);
static void continue_auth_none(struct composite_context *ctx);
static void continue_ntlmssp_connection(struct composite_context *ctx);
static void continue_spnego_after_wrong_pass(struct composite_context *ctx);


/*
  Stage 2 of pipe_auth: Receive result of schannel bind request
*/
static void continue_auth_schannel(struct composite_context *ctx)
{
	struct composite_context *c = talloc_get_type(ctx->async.private_data,
						      struct composite_context);

	c->status = dcerpc_bind_auth_schannel_recv(ctx);
	if (!composite_is_ok(c)) return;

	composite_done(c);
}


/*
  Stage 2 of pipe_auth: Receive result of authenticated bind request
*/
static void continue_auth(struct composite_context *ctx)
{
	struct composite_context *c = talloc_get_type(ctx->async.private_data,
						      struct composite_context);

	c->status = dcerpc_bind_auth_recv(ctx);
	if (!composite_is_ok(c)) return;
	
	composite_done(c);
}
/*
  Stage 2 of pipe_auth: Receive result of authenticated bind request, but handle fallbacks:
  SPNEGO -> NTLMSSP
*/
static void continue_auth_auto(struct composite_context *ctx)
{
	struct composite_context *c = talloc_get_type(ctx->async.private_data,
						      struct composite_context);
	struct pipe_auth_state *s = talloc_get_type(c->private_data, struct pipe_auth_state);
	struct composite_context *sec_conn_req;

	c->status = dcerpc_bind_auth_recv(ctx);
	if (NT_STATUS_EQUAL(c->status, NT_STATUS_INVALID_PARAMETER)) {
		/*
		 * Retry with NTLMSSP auth as fallback
		 * send a request for secondary rpc connection
		 */
		sec_conn_req = dcerpc_secondary_connection_send(s->pipe,
								s->binding);
		composite_continue(c, sec_conn_req, continue_ntlmssp_connection, c);
		return;
	} else if (NT_STATUS_EQUAL(c->status, NT_STATUS_LOGON_FAILURE)) {
		if (cli_credentials_wrong_password(s->credentials)) {
			/*
			 * Retry SPNEGO with a better password
			 * send a request for secondary rpc connection
			 */
			sec_conn_req = dcerpc_secondary_connection_send(s->pipe,
									s->binding);
			composite_continue(c, sec_conn_req, continue_spnego_after_wrong_pass, c);
			return;
		}
	}

	if (!composite_is_ok(c)) return;

	composite_done(c);
}

/*
  Stage 3 of pipe_auth (fallback to NTLMSSP case): Receive secondary
  rpc connection (the first one can't be used any more, due to the
  bind nak) and perform authenticated bind request
*/
static void continue_ntlmssp_connection(struct composite_context *ctx)
{
	struct composite_context *c;
	struct pipe_auth_state *s;
	struct composite_context *auth_req;
	struct dcerpc_pipe *p2;
	void *pp;

	c = talloc_get_type(ctx->async.private_data, struct composite_context);
	s = talloc_get_type(c->private_data, struct pipe_auth_state);

	/* receive secondary rpc connection */
	c->status = dcerpc_secondary_connection_recv(ctx, &p2);
	if (!composite_is_ok(c)) return;


	/* this is a rather strange situation. When
	   we come into the routine, s is a child of s->pipe, and
	   when we created p2 above, it also became a child of
	   s->pipe.

	   Now we want p2 to be a parent of s->pipe, and we want s to
	   be a parent of both of them! If we don't do this very
	   carefully we end up creating a talloc loop
	*/

	/* we need the new contexts to hang off the same context
	   that s->pipe is on, but the only way to get that is
	   via talloc_parent() */
	pp = talloc_parent(s->pipe);

	/* promote s to be at the top */
	talloc_steal(pp, s);

	/* and put p2 under s */
	talloc_steal(s, p2);

	/* now put s->pipe under p2 */
	talloc_steal(p2, s->pipe);

	s->pipe = p2;

	/* initiate a authenticated bind */
	auth_req = dcerpc_bind_auth_send(c, s->pipe, s->table,
					 s->credentials, 
					 lpcfg_gensec_settings(c, s->lp_ctx),
					 DCERPC_AUTH_TYPE_NTLMSSP,
					 dcerpc_auth_level(s->pipe->conn),
					 s->table->authservices->names[0]);
	composite_continue(c, auth_req, continue_auth, c);
}

/*
  Stage 3 of pipe_auth (retry on wrong password): Receive secondary
  rpc connection (the first one can't be used any more, due to the
  bind nak) and perform authenticated bind request
*/
static void continue_spnego_after_wrong_pass(struct composite_context *ctx)
{
	struct composite_context *c;
	struct pipe_auth_state *s;
	struct composite_context *auth_req;
	struct dcerpc_pipe *p2;

	c = talloc_get_type(ctx->async.private_data, struct composite_context);
	s = talloc_get_type(c->private_data, struct pipe_auth_state);

	/* receive secondary rpc connection */
	c->status = dcerpc_secondary_connection_recv(ctx, &p2);
	if (!composite_is_ok(c)) return;

	talloc_steal(s, p2);
	talloc_steal(p2, s->pipe);
	s->pipe = p2;

	/* initiate a authenticated bind */
	auth_req = dcerpc_bind_auth_send(c, s->pipe, s->table,
					 s->credentials, 
					 lpcfg_gensec_settings(c, s->lp_ctx),
					 DCERPC_AUTH_TYPE_SPNEGO,
					 dcerpc_auth_level(s->pipe->conn),
					 s->table->authservices->names[0]);
	composite_continue(c, auth_req, continue_auth, c);
}


/*
  Stage 2 of pipe_auth: Receive result of non-authenticated bind request
*/
static void continue_auth_none(struct composite_context *ctx)
{
	struct composite_context *c = talloc_get_type(ctx->async.private_data,
						      struct composite_context);

	c->status = dcerpc_bind_auth_none_recv(ctx);
	if (!composite_is_ok(c)) return;
	
	composite_done(c);
}


/*
  Request to perform an authenticated bind if required. Authentication
  is determined using credentials passed and binding flags.
*/
struct composite_context *dcerpc_pipe_auth_send(struct dcerpc_pipe *p, 
						struct dcerpc_binding *binding,
						const struct ndr_interface_table *table,
						struct cli_credentials *credentials,
						struct loadparm_context *lp_ctx)
{
	struct composite_context *c;
	struct pipe_auth_state *s;
	struct composite_context *auth_schannel_req;
	struct composite_context *auth_req;
	struct composite_context *auth_none_req;
	struct dcecli_connection *conn;
	uint8_t auth_type;

	/* composite context allocation and setup */
	c = composite_create(p, p->conn->event_ctx);
	if (c == NULL) return NULL;

	s = talloc_zero(c, struct pipe_auth_state);
	if (composite_nomem(s, c)) return c;
	c->private_data = s;

	/* store parameters in state structure */
	s->binding      = binding;
	s->table        = table;
	s->credentials  = credentials;
	s->pipe         = p;
	s->lp_ctx       = lp_ctx;

	conn = s->pipe->conn;
	conn->flags = binding->flags;

	if (DEBUGLVL(100)) {
		conn->flags |= DCERPC_DEBUG_PRINT_BOTH;
	}
	
	/* remember the binding string for possible secondary connections */
	conn->binding_string = dcerpc_binding_string(p, binding);

	if (cli_credentials_is_anonymous(s->credentials)) {
		auth_none_req = dcerpc_bind_auth_none_send(c, s->pipe, s->table);
		composite_continue(c, auth_none_req, continue_auth_none, c);
		return c;
	}

	if ((binding->flags & DCERPC_SCHANNEL) &&
	    !cli_credentials_get_netlogon_creds(s->credentials)) {
		/* If we don't already have netlogon credentials for
		 * the schannel bind, then we have to get these
		 * first */
		auth_schannel_req = dcerpc_bind_auth_schannel_send(c, s->pipe, s->table,
								   s->credentials, s->lp_ctx,
								   dcerpc_auth_level(conn));
		composite_continue(c, auth_schannel_req, continue_auth_schannel, c);
		return c;
	}

	/*
	 * we rely on the already authenticated CIFS connection
	 * if not doing sign or seal
	 */
	if (conn->transport.transport == NCACN_NP &&
	    !(s->binding->flags & (DCERPC_SIGN|DCERPC_SEAL))) {
		auth_none_req = dcerpc_bind_auth_none_send(c, s->pipe, s->table);
		composite_continue(c, auth_none_req, continue_auth_none, c);
		return c;
	}


	/* Perform an authenticated DCE-RPC bind
	 */
	if (!(conn->flags & (DCERPC_CONNECT|DCERPC_SEAL))) {
		/*
		  we are doing an authenticated connection,
		  which needs to use [connect], [sign] or [seal].
		  If nothing is specified, we default to [sign] now.
		  This give roughly the same protection as
		  ncacn_np with smb signing.
		*/
		conn->flags |= DCERPC_SIGN;
	}

	if (s->binding->flags & DCERPC_AUTH_SPNEGO) {
		auth_type = DCERPC_AUTH_TYPE_SPNEGO;

	} else if (s->binding->flags & DCERPC_AUTH_KRB5) {
		auth_type = DCERPC_AUTH_TYPE_KRB5;

	} else if (s->binding->flags & DCERPC_SCHANNEL) {
		auth_type = DCERPC_AUTH_TYPE_SCHANNEL;

	} else if (s->binding->flags & DCERPC_AUTH_NTLM) {
		auth_type = DCERPC_AUTH_TYPE_NTLMSSP;

	} else {
		/* try SPNEGO with fallback to NTLMSSP */
		auth_req = dcerpc_bind_auth_send(c, s->pipe, s->table,
						 s->credentials, 
						 lpcfg_gensec_settings(c, s->lp_ctx),
						 DCERPC_AUTH_TYPE_SPNEGO,
						 dcerpc_auth_level(conn),
						 s->table->authservices->names[0]);
		composite_continue(c, auth_req, continue_auth_auto, c);
		return c;
	}

	auth_req = dcerpc_bind_auth_send(c, s->pipe, s->table,
					 s->credentials, 
					 lpcfg_gensec_settings(c, s->lp_ctx),
					 auth_type,
					 dcerpc_auth_level(conn),
					 s->table->authservices->names[0]);
	composite_continue(c, auth_req, continue_auth, c);
	return c;
}


/*
  Receive result of authenticated bind request on dcerpc pipe

  This returns *p, which may be different to the one originally
  supllied, as it rebinds to a new pipe due to authentication fallback

*/
NTSTATUS dcerpc_pipe_auth_recv(struct composite_context *c, TALLOC_CTX *mem_ctx, 
			       struct dcerpc_pipe **p)
{
	NTSTATUS status;

	struct pipe_auth_state *s = talloc_get_type(c->private_data,
						    struct pipe_auth_state);
	status = composite_wait(c);
	if (!NT_STATUS_IS_OK(status)) {
		char *uuid_str = GUID_string(s->pipe, &s->table->syntax_id.uuid);
		DEBUG(0, ("Failed to bind to uuid %s - %s\n", uuid_str, nt_errstr(status)));
		talloc_free(uuid_str);
	} else {
		talloc_steal(mem_ctx, s->pipe);
		*p = s->pipe;
	}

	talloc_free(c);
	return status;
}


/* 
   Perform an authenticated bind if needed - sync version

   This may change *p, as it rebinds to a new pipe due to authentication fallback
*/
_PUBLIC_ NTSTATUS dcerpc_pipe_auth(TALLOC_CTX *mem_ctx,
			  struct dcerpc_pipe **p, 
			  struct dcerpc_binding *binding,
			  const struct ndr_interface_table *table,
			  struct cli_credentials *credentials,
			  struct loadparm_context *lp_ctx)
{
	struct composite_context *c;

	c = dcerpc_pipe_auth_send(*p, binding, table, credentials, lp_ctx);
	return dcerpc_pipe_auth_recv(c, mem_ctx, p);
}


NTSTATUS dcerpc_generic_session_key(struct dcecli_connection *c,
				    DATA_BLOB *session_key)
{
	/* this took quite a few CPU cycles to find ... */
	session_key->data = discard_const_p(unsigned char, "SystemLibraryDTC");
	session_key->length = 16;
	return NT_STATUS_OK;
}

/*
  fetch the user session key - may be default (above) or the SMB session key

  The key is always truncated to 16 bytes 
*/
_PUBLIC_ NTSTATUS dcerpc_fetch_session_key(struct dcerpc_pipe *p,
					   DATA_BLOB *session_key)
{
	NTSTATUS status;
	status = p->conn->security_state.session_key(p->conn, session_key);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	session_key->length = MIN(session_key->length, 16);

	return NT_STATUS_OK;
}


/*
  log a rpc packet in a format suitable for ndrdump. This is especially useful
  for sealed packets, where ethereal cannot easily see the contents

  this triggers on a debug level of >= 10
*/
_PUBLIC_ void dcerpc_log_packet(const char *lockdir,
				const struct ndr_interface_table *ndr,
				uint32_t opnum, uint32_t flags,
				const DATA_BLOB *pkt)
{
	const int num_examples = 20;
	int i;

	if (lockdir == NULL) return;

	for (i=0;i<num_examples;i++) {
		char *name=NULL;
		asprintf(&name, "%s/rpclog/%s-%u.%d.%s", 
			 lockdir, ndr->name, opnum, i,
			 (flags&NDR_IN)?"in":"out");
		if (name == NULL) {
			return;
		}
		if (!file_exist(name)) {
			if (file_save(name, pkt->data, pkt->length)) {
				DEBUG(10,("Logged rpc packet to %s\n", name));
			}
			free(name);
			break;
		}
		free(name);
	}
}



/*
  create a secondary context from a primary connection

  this uses dcerpc_alter_context() to create a new dcerpc context_id
*/
_PUBLIC_ NTSTATUS dcerpc_secondary_context(struct dcerpc_pipe *p, 
				  struct dcerpc_pipe **pp2,
				  const struct ndr_interface_table *table)
{
	NTSTATUS status;
	struct dcerpc_pipe *p2;
	
	p2 = talloc_zero(p, struct dcerpc_pipe);
	if (p2 == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	p2->conn = talloc_reference(p2, p->conn);
	p2->request_timeout = p->request_timeout;

	p2->context_id = ++p->conn->next_context_id;

	p2->syntax = table->syntax_id;

	p2->transfer_syntax = p->transfer_syntax;

	p2->binding = talloc_reference(p2, p->binding);

	p2->binding_handle = dcerpc_pipe_binding_handle(p2);
	if (p2->binding_handle == NULL) {
		talloc_free(p2);
		return NT_STATUS_NO_MEMORY;
	}

	status = dcerpc_alter_context(p2, p2, &p2->syntax, &p2->transfer_syntax);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(p2);
		return status;
	}

	*pp2 = p2;

	return NT_STATUS_OK;
}
