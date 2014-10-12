/*
   Unix SMB/CIFS implementation.

   a composite API for finding a DC and its name via CLDAP

   Copyright (C) Andrew Tridgell 2010
   Copyright (C) Andrew Bartlett 2010

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
#include "libcli/resolve/resolve.h"
#include "libcli/cldap/cldap.h"
#include "libcli/finddc.h"
#include "libcli/security/security.h"
#include "lib/util/tevent_ntstatus.h"
#include "libcli/composite/composite.h"

struct finddcs_cldap_state {
	struct tevent_context *ev;
	struct tevent_req *req;
	const char *domain_name;
	struct dom_sid *domain_sid;
	const char *srv_name;
	const char **srv_addresses;
	uint32_t minimum_dc_flags;
	uint32_t srv_address_index;
	struct cldap_socket *cldap;
	struct cldap_netlogon *netlogon;
};

static void finddcs_cldap_srv_resolved(struct composite_context *ctx);
static void finddcs_cldap_netlogon_replied(struct tevent_req *req);
static bool finddcs_cldap_srv_lookup(struct finddcs_cldap_state *state,
				     struct finddcs *io,
				     struct resolve_context *resolve_ctx,
				     struct tevent_context *event_ctx);
static bool finddcs_cldap_nbt_lookup(struct finddcs_cldap_state *state,
				     struct finddcs *io,
				     struct resolve_context *resolve_ctx,
				     struct tevent_context *event_ctx);
static void finddcs_cldap_name_resolved(struct composite_context *ctx);
static void finddcs_cldap_next_server(struct finddcs_cldap_state *state);
static bool finddcs_cldap_ipaddress(struct finddcs_cldap_state *state, struct finddcs *io);


/*
 * find a list of DCs via DNS/CLDAP
 *
 */
struct tevent_req *finddcs_cldap_send(TALLOC_CTX *mem_ctx,
				      struct finddcs *io,
				      struct resolve_context *resolve_ctx,
				      struct tevent_context *event_ctx)
{
	struct finddcs_cldap_state *state;
	struct tevent_req *req;

	req = tevent_req_create(mem_ctx, &state, struct finddcs_cldap_state);
	if (req == NULL) {
		return NULL;
	}

	state->req = req;
	state->ev = event_ctx;
	state->minimum_dc_flags = io->in.minimum_dc_flags;
	state->domain_name = talloc_strdup(state, io->in.domain_name);
	if (tevent_req_nomem(state->domain_name, req)) {
		return tevent_req_post(req, event_ctx);
	}

	if (io->in.domain_sid) {
		state->domain_sid = dom_sid_dup(state, io->in.domain_sid);
		if (tevent_req_nomem(state->domain_sid, req)) {
			return tevent_req_post(req, event_ctx);
		}
	} else {
		state->domain_sid = NULL;
	}

	if (io->in.server_address) {
		DEBUG(4,("finddcs: searching for a DC by IP %s\n", io->in.server_address));
		if (!finddcs_cldap_ipaddress(state, io)) {
			return tevent_req_post(req, event_ctx);
		}
	} else if (strchr(state->domain_name, '.')) {
		/* looks like a DNS name */
		DEBUG(4,("finddcs: searching for a DC by DNS domain %s\n", state->domain_name));
		if (!finddcs_cldap_srv_lookup(state, io, resolve_ctx, event_ctx)) {
			return tevent_req_post(req, event_ctx);
		}
	} else {
		DEBUG(4,("finddcs: searching for a DC by NBT lookup %s\n", state->domain_name));
		if (!finddcs_cldap_nbt_lookup(state, io, resolve_ctx, event_ctx)) {
			return tevent_req_post(req, event_ctx);
		}
	}

	return req;
}


/*
  we've been told the IP of the server, bypass name
  resolution and go straight to CLDAP
*/
static bool finddcs_cldap_ipaddress(struct finddcs_cldap_state *state, struct finddcs *io)
{
	NTSTATUS status;

	state->srv_addresses = talloc_array(state, const char *, 2);
	if (tevent_req_nomem(state->srv_addresses, state->req)) {
		return false;
	}
	state->srv_addresses[0] = talloc_strdup(state->srv_addresses, io->in.server_address);
	if (tevent_req_nomem(state->srv_addresses[0], state->req)) {
		return false;
	}
	state->srv_addresses[1] = NULL;
	state->srv_address_index = 0;
	status = cldap_socket_init(state, state->ev, NULL, NULL, &state->cldap);
	if (tevent_req_nterror(state->req, status)) {
		return false;
	}

	finddcs_cldap_next_server(state);
	return tevent_req_is_nterror(state->req, &status);
}

/*
  start a SRV DNS lookup
 */
static bool finddcs_cldap_srv_lookup(struct finddcs_cldap_state *state,
				     struct finddcs *io,
				     struct resolve_context *resolve_ctx,
				     struct tevent_context *event_ctx)
{
	struct composite_context *creq;
	struct nbt_name name;

	if (io->in.site_name) {
		state->srv_name = talloc_asprintf(state, "_ldap._tcp.%s._sites.%s",
					   io->in.site_name, io->in.domain_name);
	} else {
		state->srv_name = talloc_asprintf(state, "_ldap._tcp.%s", io->in.domain_name);
	}

	DEBUG(4,("finddcs: looking for SRV records for %s\n", state->srv_name));

	make_nbt_name(&name, state->srv_name, 0);

	creq = resolve_name_ex_send(resolve_ctx, state,
				    RESOLVE_NAME_FLAG_FORCE_DNS | RESOLVE_NAME_FLAG_DNS_SRV,
				    0, &name, event_ctx);
	if (tevent_req_nomem(creq, state->req)) {
		return false;
	}
	creq->async.fn = finddcs_cldap_srv_resolved;
	creq->async.private_data = state;

	return true;
}

/*
  start a NBT name lookup for domain<1C>
 */
static bool finddcs_cldap_nbt_lookup(struct finddcs_cldap_state *state,
				     struct finddcs *io,
				     struct resolve_context *resolve_ctx,
				     struct tevent_context *event_ctx)
{
	struct composite_context *creq;
	struct nbt_name name;

	make_nbt_name(&name, state->domain_name, NBT_NAME_LOGON);
	creq = resolve_name_send(resolve_ctx, state, &name, event_ctx);
	if (tevent_req_nomem(creq, state->req)) {
		return false;
	}
	creq->async.fn = finddcs_cldap_name_resolved;
	creq->async.private_data = state;
	return true;
}

/*
  fire off a CLDAP query to the next server
 */
static void finddcs_cldap_next_server(struct finddcs_cldap_state *state)
{
	struct tevent_req *subreq;

	if (state->srv_addresses[state->srv_address_index] == NULL) {
		tevent_req_nterror(state->req, NT_STATUS_OBJECT_NAME_NOT_FOUND);
		DEBUG(2,("finddcs: No matching CLDAP server found\n"));
		return;
	}

	state->netlogon = talloc_zero(state, struct cldap_netlogon);
	if (tevent_req_nomem(state->netlogon, state->req)) {
		return;
	}

	state->netlogon->in.dest_address = state->srv_addresses[state->srv_address_index];
	/* we should get the port from the SRV response */
	state->netlogon->in.dest_port = 389;
	if (strchr(state->domain_name, '.')) {
		state->netlogon->in.realm = state->domain_name;
	}
	if (state->domain_sid) {
		state->netlogon->in.domain_sid = dom_sid_string(state, state->domain_sid);
		if (tevent_req_nomem(state->netlogon->in.domain_sid, state->req)) {
			return;
		}
	}
	state->netlogon->in.acct_control = -1;
	state->netlogon->in.version =
		NETLOGON_NT_VERSION_5 |
		NETLOGON_NT_VERSION_5EX |
		NETLOGON_NT_VERSION_IP;
	state->netlogon->in.map_response = true;

	DEBUG(4,("finddcs: performing CLDAP query on %s\n", state->netlogon->in.dest_address));

	subreq = cldap_netlogon_send(state, state->cldap, state->netlogon);
	if (tevent_req_nomem(subreq, state->req)) {
		return;
	}

	tevent_req_set_callback(subreq, finddcs_cldap_netlogon_replied, state);
}


/*
  we have a response from a CLDAP server for a netlogon request
 */
static void finddcs_cldap_netlogon_replied(struct tevent_req *subreq)
{
	struct finddcs_cldap_state *state;
	NTSTATUS status;

	state = tevent_req_callback_data(subreq, struct finddcs_cldap_state);

	status = cldap_netlogon_recv(subreq, state->netlogon, state->netlogon);
	talloc_free(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		state->srv_address_index++;
		finddcs_cldap_next_server(state);
		return;
	}
	if (state->minimum_dc_flags !=
	    (state->minimum_dc_flags & state->netlogon->out.netlogon.data.nt5_ex.server_type)) {
		/* the server didn't match the minimum requirements */
		DEBUG(4,("finddcs: Skipping DC %s with server_type=0x%08x - required 0x%08x\n",
			 state->srv_addresses[state->srv_address_index],
			 state->netlogon->out.netlogon.data.nt5_ex.server_type,
			 state->minimum_dc_flags));
		state->srv_address_index++;
		finddcs_cldap_next_server(state);
		return;
	}

	DEBUG(4,("finddcs: Found matching DC %s with server_type=0x%08x\n",
		 state->srv_addresses[state->srv_address_index],
		 state->netlogon->out.netlogon.data.nt5_ex.server_type));

	tevent_req_done(state->req);
}

/*
   handle NBT name lookup reply
 */
static void finddcs_cldap_name_resolved(struct composite_context *ctx)
{
	struct finddcs_cldap_state *state =
		talloc_get_type(ctx->async.private_data, struct finddcs_cldap_state);
	const char *address;
	NTSTATUS status;

	status = resolve_name_recv(ctx, state, &address);
	if (tevent_req_nterror(state->req, status)) {
		DEBUG(2,("finddcs: No matching NBT <1c> server found\n"));
		return;
	}

	DEBUG(4,("finddcs: Found NBT <1c> server at %s\n", address));

	state->srv_addresses = talloc_array(state, const char *, 2);
	if (tevent_req_nomem(state->srv_addresses, state->req)) {
		return;
	}
	state->srv_addresses[0] = address;
	state->srv_addresses[1] = NULL;

	state->srv_address_index = 0;

	status = cldap_socket_init(state, state->ev, NULL, NULL, &state->cldap);
	if (tevent_req_nterror(state->req, status)) {
		return;
	}

	finddcs_cldap_next_server(state);
}


/*
 * Having got a DNS SRV answer, fire off the first CLDAP request
 */
static void finddcs_cldap_srv_resolved(struct composite_context *ctx)
{
	struct finddcs_cldap_state *state =
		talloc_get_type(ctx->async.private_data, struct finddcs_cldap_state);
	NTSTATUS status;
	unsigned i;

	status = resolve_name_multiple_recv(ctx, state, &state->srv_addresses);
	if (tevent_req_nterror(state->req, status)) {
		DEBUG(2,("finddcs: Failed to find SRV record for %s\n", state->srv_name));
		return;
	}

	for (i=0; state->srv_addresses[i]; i++) {
		DEBUG(4,("finddcs: DNS SRV response %u at '%s'\n", i, state->srv_addresses[i]));
	}

	state->srv_address_index = 0;

	status = cldap_socket_init(state, state->ev, NULL, NULL, &state->cldap);
	if (tevent_req_nterror(state->req, status)) {
		return;
	}

	finddcs_cldap_next_server(state);
}


NTSTATUS finddcs_cldap_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx, struct finddcs *io)
{
	struct finddcs_cldap_state *state = tevent_req_data(req, struct finddcs_cldap_state);
	bool ok;
	NTSTATUS status;

	ok = tevent_req_poll(req, state->ev);
	if (!ok) {
		talloc_free(req);
		return NT_STATUS_INTERNAL_ERROR;
	}
	status = tevent_req_simple_recv_ntstatus(req);
	if (NT_STATUS_IS_OK(status)) {
		talloc_steal(mem_ctx, state->netlogon);
		io->out.netlogon = state->netlogon->out.netlogon;
		io->out.address = talloc_steal(mem_ctx, state->srv_addresses[state->srv_address_index]);
	}
	tevent_req_received(req);
	return status;
}

NTSTATUS finddcs_cldap(TALLOC_CTX *mem_ctx,
		       struct finddcs *io,
		       struct resolve_context *resolve_ctx,
		       struct tevent_context *event_ctx)
{
	NTSTATUS status;
	struct tevent_req *req = finddcs_cldap_send(mem_ctx,
						    io,
						    resolve_ctx,
						    event_ctx);
	status = finddcs_cldap_recv(req, mem_ctx, io);
	talloc_free(req);
	return status;
}
