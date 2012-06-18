/* 
   Unix SMB/CIFS implementation.

   a composite API for finding a DC and its name

   Copyright (C) Volker Lendecke 2005
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2006
   
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

#include "include/includes.h"
#include "lib/messaging/irpc.h"
#include "librpc/gen_ndr/ndr_irpc.h"
#include "librpc/gen_ndr/samr.h"
#include "libcli/composite/composite.h"
#include "libcli/libcli.h"
#include "libcli/resolve/resolve.h"
#include "libcli/finddcs.h"

struct finddcs_state {
	struct composite_context *ctx;
	struct messaging_context *msg_ctx;

	const char *my_netbios_name;
	const char *domain_name;
	struct dom_sid *domain_sid;

	struct nbtd_getdcname r;
	struct nbt_name_status node_status;

	struct smb_iconv_convenience *iconv_convenience;

	int num_dcs;
	struct nbt_dc_name *dcs;
	uint16_t nbt_port;
};

static void finddcs_name_resolved(struct composite_context *ctx);
static void finddcs_getdc_replied(struct irpc_request *ireq);
static void fallback_node_status(struct finddcs_state *state);
static void fallback_node_status_replied(struct nbt_name_request *name_req);

/* 
 * Setup and send off the a normal name resolution for the target name.
 *
 * The domain_sid parameter is optional, and is used in the subsequent getdc request.
 *
 * This will try a GetDC request, but this may not work.  It will try
 * a node status as a fallback, then return no name (but still include
 * the IP)
 */

struct composite_context *finddcs_send(TALLOC_CTX *mem_ctx,
				       const char *my_netbios_name,
				       uint16_t nbt_port,
				       const char *domain_name,
				       int name_type,
				       struct dom_sid *domain_sid,
				       struct smb_iconv_convenience *iconv_convenience,
				       struct resolve_context *resolve_ctx,
				       struct tevent_context *event_ctx,
				       struct messaging_context *msg_ctx)
{
	struct composite_context *c, *creq;
	struct finddcs_state *state;
	struct nbt_name name;

	c = composite_create(mem_ctx, event_ctx);
	if (c == NULL) return NULL;

	state = talloc(c, struct finddcs_state);
	if (composite_nomem(state, c)) return c;
	c->private_data = state;

	state->ctx = c;

	state->nbt_port = nbt_port;
	state->my_netbios_name = talloc_strdup(state, my_netbios_name);
	state->domain_name = talloc_strdup(state, domain_name);
	state->iconv_convenience = iconv_convenience;
	if (composite_nomem(state->domain_name, c)) return c;

	if (domain_sid) {
		state->domain_sid = talloc_reference(state, domain_sid);
		if (composite_nomem(state->domain_sid, c)) return c;
	} else {
		state->domain_sid = NULL;
	}

	state->msg_ctx = msg_ctx;

	make_nbt_name(&name, state->domain_name, name_type);
	creq = resolve_name_send(resolve_ctx, state, &name, event_ctx);
	composite_continue(c, creq, finddcs_name_resolved, state);
	return c;
}

/* Having got an name query answer, fire off a GetDC request, so we
 * can find the target's all-important name.  (Kerberos and some
 * netlogon operations are quite picky about names) 
 *
 * The name is a courtesy, if we don't find it, don't completely fail.
 *
 * However, if the nbt server is down, fall back to a node status
 * request
 */
static void finddcs_name_resolved(struct composite_context *ctx)
{
	struct finddcs_state *state =
		talloc_get_type(ctx->async.private_data, struct finddcs_state);
	struct irpc_request *ireq;
	struct server_id *nbt_servers;
	const char *address;

	state->ctx->status = resolve_name_recv(ctx, state, &address);
	if (!composite_is_ok(state->ctx)) return;

	/* TODO: This should try and find all the DCs, and give the
	 * caller them in the order they responded */

	state->num_dcs = 1;
	state->dcs = talloc_array(state, struct nbt_dc_name, state->num_dcs);
	if (composite_nomem(state->dcs, state->ctx)) return;

	state->dcs[0].address = talloc_steal(state->dcs, address);

	/* Try and find the nbt server.  Fallback to a node status
	 * request if we can't make this happen The nbt server just
	 * might not be running, or we may not have a messaging
	 * context (not root etc) */
	if (!state->msg_ctx) {
		fallback_node_status(state);
		return;
	}

	nbt_servers = irpc_servers_byname(state->msg_ctx, state, "nbt_server");
	if ((nbt_servers == NULL) || (nbt_servers[0].id == 0)) {
		fallback_node_status(state);
		return;
	}

	state->r.in.domainname = state->domain_name;
	state->r.in.ip_address = state->dcs[0].address;
	state->r.in.my_computername = state->my_netbios_name;
	state->r.in.my_accountname = talloc_asprintf(state, "%s$", state->my_netbios_name);
	if (composite_nomem(state->r.in.my_accountname, state->ctx)) return;
	state->r.in.account_control = ACB_WSTRUST;
	state->r.in.domain_sid = state->domain_sid;

	ireq = irpc_call_send(state->msg_ctx, nbt_servers[0],
			      &ndr_table_irpc, NDR_NBTD_GETDCNAME,
			      &state->r, state);
	if (!ireq) {
		fallback_node_status(state);
		return;
	}

	composite_continue_irpc(state->ctx, ireq, finddcs_getdc_replied, state);
}

/* Called when the GetDC request returns */
static void finddcs_getdc_replied(struct irpc_request *ireq)
{
	struct finddcs_state *state =
		talloc_get_type(ireq->async.private_data, struct finddcs_state);

	state->ctx->status = irpc_call_recv(ireq);
	if (!composite_is_ok(state->ctx)) return;

	state->dcs[0].name = talloc_steal(state->dcs, state->r.out.dcname);
	composite_done(state->ctx);
}

/* The GetDC request might not be available (such as occours when the
 * NBT server is down).  Fallback to a node status.  It is the best
 * hope we have... */
static void fallback_node_status(struct finddcs_state *state) 
{
	struct nbt_name_socket *nbtsock;
	struct nbt_name_request *name_req;

	state->node_status.in.name.name = "*";
	state->node_status.in.name.type = NBT_NAME_CLIENT;
	state->node_status.in.name.scope = NULL;
	state->node_status.in.dest_addr = state->dcs[0].address;
	state->node_status.in.dest_port = state->nbt_port;
	state->node_status.in.timeout = 1;
	state->node_status.in.retries = 2;

	nbtsock = nbt_name_socket_init(state, state->ctx->event_ctx, 
				       state->iconv_convenience);
	if (composite_nomem(nbtsock, state->ctx)) return;
	
	name_req = nbt_name_status_send(nbtsock, &state->node_status);
	if (composite_nomem(name_req, state->ctx)) return;

	composite_continue_nbt(state->ctx, 
			       name_req, 
			       fallback_node_status_replied,
			       state);
}

/* We have a node status reply (or perhaps a timeout) */
static void fallback_node_status_replied(struct nbt_name_request *name_req) 
{
	int i;
	struct finddcs_state *state = talloc_get_type(name_req->async.private_data, struct finddcs_state);
	state->ctx->status = nbt_name_status_recv(name_req, state, &state->node_status);
	if (!composite_is_ok(state->ctx)) return;

	for (i=0; i < state->node_status.out.status.num_names; i++) {
		int j;
		if (state->node_status.out.status.names[i].type == NBT_NAME_SERVER) {
			char *name = talloc_strndup(state->dcs, state->node_status.out.status.names[0].name, 15);
			/* Strip space padding */
			if (name) {
				j = MIN(strlen(name), 15);
				for (; j > 0 && name[j - 1] == ' '; j--) {
					name[j - 1] = '\0';
				}
			}
			state->dcs[0].name = name;
			composite_done(state->ctx);
			return;
		}
	}
	composite_error(state->ctx, NT_STATUS_NO_LOGON_SERVERS);
}

NTSTATUS finddcs_recv(struct composite_context *c, TALLOC_CTX *mem_ctx,
		      int *num_dcs, struct nbt_dc_name **dcs)
{
	NTSTATUS status = composite_wait(c);
	if (NT_STATUS_IS_OK(status)) {
		struct finddcs_state *state =
			talloc_get_type(c->private_data, struct finddcs_state);
		*num_dcs = state->num_dcs;
		*dcs = talloc_steal(mem_ctx, state->dcs);
	}
	talloc_free(c);
	return status;
}

NTSTATUS finddcs(TALLOC_CTX *mem_ctx,
		 const char *my_netbios_name,
		 uint16_t nbt_port,
		 const char *domain_name, int name_type, 
		 struct dom_sid *domain_sid,
		 struct smb_iconv_convenience *iconv_convenience,
		 struct resolve_context *resolve_ctx,
		 struct tevent_context *event_ctx,
		 struct messaging_context *msg_ctx,
		 int *num_dcs, struct nbt_dc_name **dcs)
{
	struct composite_context *c = finddcs_send(mem_ctx,
						   my_netbios_name,
						   nbt_port,
						   domain_name, name_type,
						   domain_sid, 
						   iconv_convenience,
						   resolve_ctx,
						   event_ctx, msg_ctx);
	return finddcs_recv(c, mem_ctx, num_dcs, dcs);
}
