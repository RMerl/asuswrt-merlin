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
#include <tevent.h>
#include "lib/messaging/irpc.h"
#include "librpc/gen_ndr/ndr_irpc_c.h"
#include "libcli/composite/composite.h"
#include "libcli/libcli.h"
#include "libcli/resolve/resolve.h"
#include "lib/util/tevent_ntstatus.h"

struct finddcs_nbt_state {
	struct tevent_context *ev;
	struct tevent_req *req;
	struct messaging_context *msg_ctx;

	const char *my_netbios_name;
	const char *domain_name;
	struct dom_sid *domain_sid;

	struct nbtd_getdcname r;
	struct nbt_name_status node_status;

	int num_dcs;
	struct nbt_dc_name *dcs;
	uint16_t nbt_port;
};

static void finddcs_nbt_name_resolved(struct composite_context *ctx);
static void finddcs_nbt_getdc_replied(struct tevent_req *subreq);
static void fallback_node_status(struct finddcs_nbt_state *state);
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

struct tevent_req *finddcs_nbt_send(TALLOC_CTX *mem_ctx,
				const char *my_netbios_name,
				uint16_t nbt_port,
				const char *domain_name,
				int name_type,
				struct dom_sid *domain_sid,
				struct resolve_context *resolve_ctx,
				struct tevent_context *event_ctx,
				struct messaging_context *msg_ctx)
{
	struct finddcs_nbt_state *state;
	struct nbt_name name;
	struct tevent_req *req;
	struct composite_context *creq;

	req = tevent_req_create(mem_ctx, &state, struct finddcs_nbt_state);
	if (req == NULL) {
		return NULL;
	}

	state->req = req;
	state->ev = event_ctx;
	state->nbt_port = nbt_port;
	state->my_netbios_name = talloc_strdup(state, my_netbios_name);
	if (tevent_req_nomem(state->my_netbios_name, req)) {
		return tevent_req_post(req, event_ctx);
	}
	state->domain_name = talloc_strdup(state, domain_name);
	if (tevent_req_nomem(state->domain_name, req)) {
		return tevent_req_post(req, event_ctx);
	}

	if (domain_sid) {
		state->domain_sid = talloc_reference(state, domain_sid);
		if (tevent_req_nomem(state->domain_sid, req)) {
			return tevent_req_post(req, event_ctx);
		}
	} else {
		state->domain_sid = NULL;
	}

	state->msg_ctx = msg_ctx;

	make_nbt_name(&name, state->domain_name, name_type);
	creq = resolve_name_send(resolve_ctx, state, &name, event_ctx);
	if (tevent_req_nomem(creq, req)) {
		return tevent_req_post(req, event_ctx);
	}
	creq->async.fn = finddcs_nbt_name_resolved;
	creq->async.private_data = state;

	return req;
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
static void finddcs_nbt_name_resolved(struct composite_context *ctx)
{
	struct finddcs_nbt_state *state =
		talloc_get_type(ctx->async.private_data, struct finddcs_nbt_state);
	struct tevent_req *subreq;
	struct dcerpc_binding_handle *irpc_handle;
	const char *address;
	NTSTATUS status;

	status = resolve_name_recv(ctx, state, &address);
	if (tevent_req_nterror(state->req, status)) {
		return;
	}

	/* TODO: This should try and find all the DCs, and give the
	 * caller them in the order they responded */

	state->num_dcs = 1;
	state->dcs = talloc_array(state, struct nbt_dc_name, state->num_dcs);
	if (tevent_req_nomem(state->dcs, state->req)) {
		return;
	}

	state->dcs[0].address = talloc_steal(state->dcs, address);

	/* Try and find the nbt server.  Fallback to a node status
	 * request if we can't make this happen The nbt server just
	 * might not be running, or we may not have a messaging
	 * context (not root etc) */
	if (!state->msg_ctx) {
		fallback_node_status(state);
		return;
	}

	irpc_handle = irpc_binding_handle_by_name(state, state->msg_ctx,
						  "nbt_server", &ndr_table_irpc);
	if (irpc_handle == NULL) {
		fallback_node_status(state);
		return;
	}

	state->r.in.domainname = state->domain_name;
	state->r.in.ip_address = state->dcs[0].address;
	state->r.in.my_computername = state->my_netbios_name;
	state->r.in.my_accountname = talloc_asprintf(state, "%s$", state->my_netbios_name);
	if (tevent_req_nomem(state->r.in.my_accountname, state->req)) {
		return;
	}
	state->r.in.account_control = ACB_WSTRUST;
	state->r.in.domain_sid = state->domain_sid;
	if (state->r.in.domain_sid == NULL) {
		state->r.in.domain_sid = talloc_zero(state, struct dom_sid);
	}

	subreq = dcerpc_nbtd_getdcname_r_send(state, state->ev,
					      irpc_handle, &state->r);
	if (tevent_req_nomem(subreq, state->req)) {
		return;
	}
	tevent_req_set_callback(subreq, finddcs_nbt_getdc_replied, state);
}

/* Called when the GetDC request returns */
static void finddcs_nbt_getdc_replied(struct tevent_req *subreq)
{
	struct finddcs_nbt_state *state =
		tevent_req_callback_data(subreq,
		struct finddcs_nbt_state);
	NTSTATUS status;

	status = dcerpc_nbtd_getdcname_r_recv(subreq, state);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		fallback_node_status(state);
		return;
	}

	state->dcs[0].name = talloc_steal(state->dcs, state->r.out.dcname);
	tevent_req_done(state->req);
}

/* The GetDC request might not be available (such as occours when the
 * NBT server is down).  Fallback to a node status.  It is the best
 * hope we have... */
static void fallback_node_status(struct finddcs_nbt_state *state)
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

	nbtsock = nbt_name_socket_init(state, state->ev);
	if (tevent_req_nomem(nbtsock, state->req)) {
		return;
	}

	name_req = nbt_name_status_send(nbtsock, &state->node_status);
	if (tevent_req_nomem(name_req, state->req)) {
		return;
	}

	name_req->async.fn = fallback_node_status_replied;
	name_req->async.private_data = state;
}

/* We have a node status reply (or perhaps a timeout) */
static void fallback_node_status_replied(struct nbt_name_request *name_req)
{
	int i;
	struct finddcs_nbt_state *state = talloc_get_type(name_req->async.private_data, struct finddcs_nbt_state);
	NTSTATUS status;

	status = nbt_name_status_recv(name_req, state, &state->node_status);
	if (tevent_req_nterror(state->req, status)) {
		return;
	}

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
			tevent_req_done(state->req);
			return;
		}
	}
	tevent_req_nterror(state->req, NT_STATUS_NO_LOGON_SERVERS);
}

NTSTATUS finddcs_nbt_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
		      int *num_dcs, struct nbt_dc_name **dcs)
{
	struct finddcs_nbt_state *state = tevent_req_data(req, struct finddcs_nbt_state);
	bool ok;
	NTSTATUS status;

	ok = tevent_req_poll(req, state->ev);
	if (!ok) {
		talloc_free(req);
		return NT_STATUS_INTERNAL_ERROR;
	}
	status = tevent_req_simple_recv_ntstatus(req);
	if (NT_STATUS_IS_OK(status)) {
		*num_dcs = state->num_dcs;
		*dcs = talloc_steal(mem_ctx, state->dcs);
	}
	talloc_free(req);
	return status;
}

NTSTATUS finddcs_nbt(TALLOC_CTX *mem_ctx,
		 const char *my_netbios_name,
		 uint16_t nbt_port,
		 const char *domain_name, int name_type,
		 struct dom_sid *domain_sid,
		 struct resolve_context *resolve_ctx,
		 struct tevent_context *event_ctx,
		 struct messaging_context *msg_ctx,
		 int *num_dcs, struct nbt_dc_name **dcs)
{
	struct tevent_req *req = finddcs_nbt_send(mem_ctx,
					      my_netbios_name,
					      nbt_port,
					      domain_name, name_type,
					      domain_sid,
					      resolve_ctx,
					      event_ctx, msg_ctx);
	return finddcs_nbt_recv(req, mem_ctx, num_dcs, dcs);
}
