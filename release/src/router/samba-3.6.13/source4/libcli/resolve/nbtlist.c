/* 
   Unix SMB/CIFS implementation.

   nbt list of addresses name resolution module

   Copyright (C) Andrew Tridgell 2005
   
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
  TODO: we should lower the timeout, and add retries for each name
*/

#include "includes.h"
#include "libcli/composite/composite.h"
#include "system/network.h"
#include "lib/socket/socket.h"
#include "lib/socket/netif.h"
#include "librpc/gen_ndr/ndr_nbt.h"
#include "../libcli/nbt/libnbt.h"
#include "param/param.h"
#include "libcli/resolve/resolve.h"

struct nbtlist_state {
	uint16_t flags;
	uint16_t port;
	struct nbt_name name;
	struct nbt_name_socket *nbtsock;
	int num_queries;
	struct nbt_name_request **queries;
	struct nbt_name_query *io_queries;
	struct socket_address **addrs;
	char **names;
	struct interface *ifaces;
};

/*
  handle events during nbtlist name resolution
*/
static void nbtlist_handler(struct nbt_name_request *req)
{
	struct composite_context *c = talloc_get_type(req->async.private_data,
						      struct composite_context);
	struct nbtlist_state *state = talloc_get_type(c->private_data, struct nbtlist_state);
	struct nbt_name_query *q;
	int i;

	for (i=0;i<state->num_queries;i++) {
		if (req == state->queries[i]) break;
	}

	if (i == state->num_queries) {
		/* not for us?! */
		composite_error(c, NT_STATUS_INTERNAL_ERROR);
		return;
	}

	q = &state->io_queries[i];

	c->status = nbt_name_query_recv(req, state, q);

	/* free the network resource directly */
	talloc_free(state->nbtsock);
	if (!composite_is_ok(c)) return;

	if (q->out.num_addrs < 1) {
		composite_error(c, NT_STATUS_UNEXPECTED_NETWORK_ERROR);
		return;
	}

	state->addrs = talloc_array(state, struct socket_address *,
				    q->out.num_addrs + 1);
	if (composite_nomem(state->addrs, c)) return;

	state->names = talloc_array(state, char *, q->out.num_addrs + 1);
	if (composite_nomem(state->names, c)) return;

	for (i=0;i<q->out.num_addrs;i++) {
		state->addrs[i] = socket_address_from_strings(state->addrs,
							      "ipv4",
							      q->out.reply_addrs[i],
							      state->port);
		if (composite_nomem(state->addrs[i], c)) return;

		state->names[i] = talloc_strdup(state->names, state->name.name);
		if (composite_nomem(state->names[i], c)) return;
	}
	state->addrs[i] = NULL;
	state->names[i] = NULL;

	composite_done(c);
}

/*
  nbtlist name resolution method - async send
 */
struct composite_context *resolve_name_nbtlist_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *event_ctx,
						    uint32_t flags,
						    uint16_t port,
						    struct nbt_name *name, 
						    const char **address_list,
						    struct interface *ifaces,
						    uint16_t nbt_port,
						    int nbt_timeout,
						    bool broadcast,
						    bool wins_lookup)
{
	struct composite_context *c;
	struct nbtlist_state *state;
	int i;

	c = composite_create(mem_ctx, event_ctx);
	if (c == NULL) return NULL;

	if (flags & RESOLVE_NAME_FLAG_FORCE_DNS) {
		composite_error(c, NT_STATUS_OBJECT_NAME_NOT_FOUND);
		return c;
	}

	if (strlen(name->name) > 15) {
		composite_error(c, NT_STATUS_OBJECT_NAME_NOT_FOUND);
		return c;
	}

	state = talloc(c, struct nbtlist_state);
	if (composite_nomem(state, c)) return c;
	c->private_data = state;

	state->flags = flags;
	state->port = port;

	c->status = nbt_name_dup(state, name, &state->name);
	if (!composite_is_ok(c)) return c;

	state->name.name = strupper_talloc(state, state->name.name);
	if (composite_nomem(state->name.name, c)) return c;
	if (state->name.scope) {
		state->name.scope = strupper_talloc(state, state->name.scope);
		if (composite_nomem(state->name.scope, c)) return c;
	}

	state->ifaces = talloc_reference(state, ifaces);

	/*
	 * we can't push long names on the wire,
	 * so bail out here to give a useful error message
	 */
	if (strlen(state->name.name) > 15) {
		composite_error(c, NT_STATUS_OBJECT_NAME_NOT_FOUND);
		return c;
	}

	state->nbtsock = nbt_name_socket_init(state, event_ctx);
	if (composite_nomem(state->nbtsock, c)) return c;

	/* count the address_list size */
	for (i=0;address_list[i];i++) /* noop */ ;

	state->num_queries = i;
	state->io_queries = talloc_array(state, struct nbt_name_query, state->num_queries);
	if (composite_nomem(state->io_queries, c)) return c;

	state->queries = talloc_array(state, struct nbt_name_request *, state->num_queries);
	if (composite_nomem(state->queries, c)) return c;

	for (i=0;i<state->num_queries;i++) {
		state->io_queries[i].in.name        = state->name;
		state->io_queries[i].in.dest_addr   = talloc_strdup(state->io_queries, address_list[i]);
		state->io_queries[i].in.dest_port   = nbt_port;
		if (composite_nomem(state->io_queries[i].in.dest_addr, c)) return c;

		state->io_queries[i].in.broadcast   = broadcast;
		state->io_queries[i].in.wins_lookup = wins_lookup;
		state->io_queries[i].in.timeout     = nbt_timeout;
		state->io_queries[i].in.retries     = 2;

		state->queries[i] = nbt_name_query_send(state->nbtsock, &state->io_queries[i]);
		if (composite_nomem(state->queries[i], c)) return c;

		state->queries[i]->async.fn      = nbtlist_handler;
		state->queries[i]->async.private_data = c;
	}

	return c;
}

/*
  nbt list of addresses name resolution method - recv side
 */
NTSTATUS resolve_name_nbtlist_recv(struct composite_context *c, 
				   TALLOC_CTX *mem_ctx,
				   struct socket_address ***addrs,
				   char ***names)
{
	NTSTATUS status;

	status = composite_wait(c);

	if (NT_STATUS_IS_OK(status)) {
		struct nbtlist_state *state = talloc_get_type(c->private_data, struct nbtlist_state);
		*addrs = talloc_steal(mem_ctx, state->addrs);
		if (names) {
			*names = talloc_steal(mem_ctx, state->names);
		}
	}

	talloc_free(c);
	return status;
}

