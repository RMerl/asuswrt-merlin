/* 
   Unix SMB/CIFS implementation.

   general name resolution interface

   Copyright (C) Andrew Tridgell 2005
   Copyright (C) Jelmer Vernooij 2007
   
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
#include "libcli/resolve/resolve.h"
#include "librpc/gen_ndr/ndr_nbt.h"
#include "system/network.h"
#include "lib/socket/socket.h"
#include "../lib/util/dlinklist.h"
#include "lib/tsocket/tsocket.h"
#include "lib/util/util_net.h"

struct resolve_state {
	struct resolve_context *ctx;
	struct resolve_method *method;
	uint32_t flags;
	uint16_t port;
	struct nbt_name name;
	struct composite_context *creq;
	struct socket_address **addrs;
	char **names;
};

static struct composite_context *setup_next_method(struct composite_context *c);


struct resolve_context {
	struct resolve_method {
		resolve_name_send_fn send_fn;
		resolve_name_recv_fn recv_fn;
		void *privdata;
		struct resolve_method *prev, *next;
	} *methods;
};

/**
 * Initialize a resolve context
 */
struct resolve_context *resolve_context_init(TALLOC_CTX *mem_ctx)
{
	return talloc_zero(mem_ctx, struct resolve_context);
}

/**
 * Add a resolve method
 */
bool resolve_context_add_method(struct resolve_context *ctx, resolve_name_send_fn send_fn, 
				resolve_name_recv_fn recv_fn, void *userdata)
{
	struct resolve_method *method = talloc_zero(ctx, struct resolve_method);

	if (method == NULL)
		return false;

	method->send_fn = send_fn;
	method->recv_fn = recv_fn;
	method->privdata = userdata;
	DLIST_ADD_END(ctx->methods, method, struct resolve_method *);
	return true;
}

/**
  handle completion of one name resolve method
*/
static void resolve_handler(struct composite_context *creq)
{
	struct composite_context *c = (struct composite_context *)creq->async.private_data;
	struct resolve_state *state = talloc_get_type(c->private_data, struct resolve_state);
	const struct resolve_method *method = state->method;

	c->status = method->recv_fn(creq, state, &state->addrs, &state->names);
	
	if (!NT_STATUS_IS_OK(c->status)) {
		state->method = state->method->next;
		state->creq = setup_next_method(c);
		if (state->creq != NULL) {
			return;
		}
	}

	if (!NT_STATUS_IS_OK(c->status)) {
		c->state = COMPOSITE_STATE_ERROR;
	} else {
		c->state = COMPOSITE_STATE_DONE;
	}
	if (c->async.fn) {
		c->async.fn(c);
	}
}


static struct composite_context *setup_next_method(struct composite_context *c)
{
	struct resolve_state *state = talloc_get_type(c->private_data, struct resolve_state);
	struct composite_context *creq = NULL;

	do {
		if (state->method) {
			creq = state->method->send_fn(c, c->event_ctx,
						      state->method->privdata,
						      state->flags,
						      state->port,
						      &state->name);
		}
		if (creq == NULL && state->method) state->method = state->method->next;

	} while (!creq && state->method);

	if (creq) {
		creq->async.fn = resolve_handler;
		creq->async.private_data = c;
	}

	return creq;
}

/*
  general name resolution - async send
 */
struct composite_context *resolve_name_all_send(struct resolve_context *ctx,
						TALLOC_CTX *mem_ctx,
						uint32_t flags, /* RESOLVE_NAME_FLAG_* */
						uint16_t port,
						struct nbt_name *name,
						struct tevent_context *event_ctx)
{
	struct composite_context *c;
	struct resolve_state *state;

	if (event_ctx == NULL) {
		return NULL;
	}

	c = composite_create(mem_ctx, event_ctx);
	if (c == NULL) return NULL;

	if (composite_nomem(c->event_ctx, c)) return c;

	state = talloc(c, struct resolve_state);
	if (composite_nomem(state, c)) return c;
	c->private_data = state;

	state->flags = flags;
	state->port = port;

	c->status = nbt_name_dup(state, name, &state->name);
	if (!composite_is_ok(c)) return c;
	
	state->ctx = talloc_reference(state, ctx);
	if (composite_nomem(state->ctx, c)) return c;

	if (is_ipaddress(state->name.name) || 
	    strcasecmp(state->name.name, "localhost") == 0) {
		struct in_addr ip = interpret_addr2(state->name.name);

		state->addrs = talloc_array(state, struct socket_address *, 2);
		if (composite_nomem(state->addrs, c)) return c;
		state->addrs[0] = socket_address_from_strings(state->addrs, "ipv4",
							      inet_ntoa(ip), 0);
		if (composite_nomem(state->addrs[0], c)) return c;
		state->addrs[1] = NULL;
		state->names = talloc_array(state, char *, 2);
		if (composite_nomem(state->names, c)) return c;
		state->names[0] = talloc_strdup(state->names, state->name.name);
		if (composite_nomem(state->names[0], c)) return c;
		state->names[1] = NULL;
		composite_done(c);
		return c;
	}

	state->method = ctx->methods;
	if (state->method == NULL) {
		composite_error(c, NT_STATUS_BAD_NETWORK_NAME);
		return c;
	}
	state->creq = setup_next_method(c);
	if (composite_nomem(state->creq, c)) return c;
	
	return c;
}

/*
  general name resolution method - recv side
 */
NTSTATUS resolve_name_all_recv(struct composite_context *c,
			       TALLOC_CTX *mem_ctx,
			       struct socket_address ***addrs,
			       char ***names)
{
	NTSTATUS status;

	status = composite_wait(c);

	if (NT_STATUS_IS_OK(status)) {
		struct resolve_state *state = talloc_get_type(c->private_data, struct resolve_state);
		*addrs = talloc_steal(mem_ctx, state->addrs);
		if (names) {
			*names = talloc_steal(mem_ctx, state->names);
		}
	}

	talloc_free(c);
	return status;
}

struct composite_context *resolve_name_ex_send(struct resolve_context *ctx,
					       TALLOC_CTX *mem_ctx,
					       uint32_t flags, /* RESOLVE_NAME_FLAG_* */
					       uint16_t port,
					       struct nbt_name *name,
					       struct tevent_context *event_ctx)
{
	return resolve_name_all_send(ctx, mem_ctx, flags, port, name, event_ctx);
}

struct composite_context *resolve_name_send(struct resolve_context *ctx,
					    TALLOC_CTX *mem_ctx,
					    struct nbt_name *name,
					    struct tevent_context *event_ctx)
{
	return resolve_name_ex_send(ctx, mem_ctx, 0, 0, name, event_ctx);
}

NTSTATUS resolve_name_recv(struct composite_context *c,
			   TALLOC_CTX *mem_ctx,
			   const char **reply_addr)
{
	NTSTATUS status;
	struct socket_address **addrs = NULL;

	status = resolve_name_all_recv(c, mem_ctx, &addrs, NULL);

	if (NT_STATUS_IS_OK(status)) {
		struct tsocket_address *t_addr = socket_address_to_tsocket_address(addrs, addrs[0]);
		if (!t_addr) {
			return NT_STATUS_NO_MEMORY;
		}

		*reply_addr = tsocket_address_inet_addr_string(t_addr, mem_ctx);
		talloc_free(addrs);
		if (!*reply_addr) {
			return NT_STATUS_NO_MEMORY;
		}
	}

	return status;
}

/*
  receive multiple responses from resolve_name_send()
 */
NTSTATUS resolve_name_multiple_recv(struct composite_context *c,
				    TALLOC_CTX *mem_ctx,
				    const char ***reply_addrs)
{
	NTSTATUS status;
	struct socket_address **addrs = NULL;
	int i;

	status = resolve_name_all_recv(c, mem_ctx, &addrs, NULL);
	NT_STATUS_NOT_OK_RETURN(status);

	/* count the addresses */
	for (i=0; addrs[i]; i++) ;

	*reply_addrs = talloc_array(mem_ctx, const char *, i+1);
	NT_STATUS_HAVE_NO_MEMORY(*reply_addrs);

	for (i=0; addrs[i]; i++) {
		struct tsocket_address *t_addr = socket_address_to_tsocket_address(addrs, addrs[i]);
		NT_STATUS_HAVE_NO_MEMORY(t_addr);

		(*reply_addrs)[i] = tsocket_address_inet_addr_string(t_addr, *reply_addrs);
		NT_STATUS_HAVE_NO_MEMORY((*reply_addrs)[i]);
	}
	(*reply_addrs)[i] = NULL;

	talloc_free(addrs);

	return status;
}

/*
  general name resolution - sync call
 */
NTSTATUS resolve_name_ex(struct resolve_context *ctx,
			 uint32_t flags, /* RESOLVE_NAME_FLAG_* */
			 uint16_t port,
			 struct nbt_name *name,
			 TALLOC_CTX *mem_ctx,
			 const char **reply_addr,
			 struct tevent_context *ev)
{
	struct composite_context *c = resolve_name_ex_send(ctx, mem_ctx, flags, port, name, ev);
	return resolve_name_recv(c, mem_ctx, reply_addr);
}


/*
  general name resolution - sync call
 */
NTSTATUS resolve_name(struct resolve_context *ctx,
		      struct nbt_name *name,
		      TALLOC_CTX *mem_ctx,
		      const char **reply_addr,
		      struct tevent_context *ev)
{
	return resolve_name_ex(ctx, 0, 0, name, mem_ctx, reply_addr, ev);
}

/* Initialise a struct nbt_name with a NULL scope */

void make_nbt_name(struct nbt_name *nbt, const char *name, int type)
{
	nbt->name = name;
	nbt->scope = NULL;
	nbt->type = type;
}

/* Initialise a struct nbt_name with a NBT_NAME_CLIENT (0x00) name */

void make_nbt_name_client(struct nbt_name *nbt, const char *name)
{
	make_nbt_name(nbt, name, NBT_NAME_CLIENT);
}

/* Initialise a struct nbt_name with a NBT_NAME_SERVER (0x20) name */

void make_nbt_name_server(struct nbt_name *nbt, const char *name)
{
	make_nbt_name(nbt, name, NBT_NAME_SERVER);
}


