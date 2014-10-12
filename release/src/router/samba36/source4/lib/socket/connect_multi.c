/* 
   Unix SMB/CIFS implementation.

   Fire connect requests to a host and a number of ports, with a timeout
   between the connect request. Return if the first connect comes back
   successfully or return the last error.

   Copyright (C) Volker Lendecke 2005
   
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
#include "lib/socket/socket.h"
#include "lib/events/events.h"
#include "libcli/composite/composite.h"
#include "libcli/resolve/resolve.h"

#define MULTI_PORT_DELAY 2000 /* microseconds */

/*
  overall state
*/
struct connect_multi_state {
	struct socket_address *server_address;
	int num_ports;
	uint16_t *ports;

	struct socket_context *sock;
	uint16_t result_port;

	int num_connects_sent, num_connects_recv;
};

/*
  state of an individual socket_connect_send() call
*/
struct connect_one_state {
	struct composite_context *result;
	struct socket_context *sock;
	struct socket_address *addr;
};

static void continue_resolve_name(struct composite_context *creq);
static void connect_multi_timer(struct tevent_context *ev,
				    struct tevent_timer *te,
				    struct timeval tv, void *p);
static void connect_multi_next_socket(struct composite_context *result);
static void continue_one(struct composite_context *creq);

/*
  setup an async socket_connect, with multiple ports
*/
_PUBLIC_ struct composite_context *socket_connect_multi_send(
						    TALLOC_CTX *mem_ctx,
						    const char *server_name,
						    int num_server_ports,
						    uint16_t *server_ports,
						    struct resolve_context *resolve_ctx,
						    struct tevent_context *event_ctx)
{
	struct composite_context *result;
	struct connect_multi_state *multi;
	int i;

	struct nbt_name name;
	struct composite_context *creq;
		
	result = talloc_zero(mem_ctx, struct composite_context);
	if (result == NULL) return NULL;
	result->state = COMPOSITE_STATE_IN_PROGRESS;
	result->event_ctx = event_ctx;

	multi = talloc_zero(result, struct connect_multi_state);
	if (composite_nomem(multi, result)) goto failed;
	result->private_data = multi;

	multi->num_ports = num_server_ports;
	multi->ports = talloc_array(multi, uint16_t, multi->num_ports);
	if (composite_nomem(multi->ports, result)) goto failed;

	for (i=0; i<multi->num_ports; i++) {
		multi->ports[i] = server_ports[i];
	}

	/*  
	    we don't want to do the name resolution separately
		    for each port, so start it now, then only start on
		    the real sockets once we have an IP
	*/
	make_nbt_name_server(&name, server_name);

	creq = resolve_name_all_send(resolve_ctx, multi, 0, multi->ports[0], &name, result->event_ctx);
	if (composite_nomem(creq, result)) goto failed;

	composite_continue(result, creq, continue_resolve_name, result);

	return result;


 failed:
	composite_error(result, result->status);
	return result;
}

/*
  start connecting to the next socket/port in the list
*/
static void connect_multi_next_socket(struct composite_context *result)
{
	struct connect_multi_state *multi = talloc_get_type(result->private_data, 
							    struct connect_multi_state);
	struct connect_one_state *state;
	struct composite_context *creq;
	int next = multi->num_connects_sent;

	if (next == multi->num_ports) {
		/* don't do anything, just wait for the existing ones to finish */
		return;
	}

	multi->num_connects_sent += 1;

	state = talloc(multi, struct connect_one_state);
	if (composite_nomem(state, result)) return;

	state->result = result;
	result->status = socket_create("ipv4", SOCKET_TYPE_STREAM, &state->sock, 0);
	if (!composite_is_ok(result)) return;

	state->addr = socket_address_copy(state, multi->server_address);
	if (composite_nomem(state->addr, result)) return;

	socket_address_set_port(state->addr, multi->ports[next]);

	talloc_steal(state, state->sock);

	creq = socket_connect_send(state->sock, NULL, 
				   state->addr, 0,
				   result->event_ctx);
	if (composite_nomem(creq, result)) return;
	talloc_steal(state, creq);

	composite_continue(result, creq, continue_one, state);

	/* if there are more ports to go then setup a timer to fire when we have waited
	   for a couple of milli-seconds, when that goes off we try the next port regardless
	   of whether this port has completed */
	if (multi->num_ports > multi->num_connects_sent) {
		/* note that this timer is a child of the single
		   connect attempt state, so it will go away when this
		   request completes */
		event_add_timed(result->event_ctx, state,
				timeval_current_ofs(0, MULTI_PORT_DELAY),
				connect_multi_timer, result);
	}
}

/*
  a timer has gone off telling us that we should try the next port
*/
static void connect_multi_timer(struct tevent_context *ev,
				struct tevent_timer *te,
				struct timeval tv, void *p)
{
	struct composite_context *result = talloc_get_type(p, struct composite_context);
	connect_multi_next_socket(result);
}


/*
  recv name resolution reply then send the next connect
*/
static void continue_resolve_name(struct composite_context *creq)
{
	struct composite_context *result = talloc_get_type(creq->async.private_data, 
							   struct composite_context);
	struct connect_multi_state *multi = talloc_get_type(result->private_data, 
							    struct connect_multi_state);
	struct socket_address **addr;

	result->status = resolve_name_all_recv(creq, multi, &addr, NULL);
	if (!composite_is_ok(result)) return;

	/* Let's just go for the first for now */
	multi->server_address = addr[0];

	connect_multi_next_socket(result);
}

/*
  one of our socket_connect_send() calls hash finished. If it got a
  connection or there are none left then we are done
*/
static void continue_one(struct composite_context *creq)
{
	struct connect_one_state *state = talloc_get_type(creq->async.private_data, 
							  struct connect_one_state);
	struct composite_context *result = state->result;
	struct connect_multi_state *multi = talloc_get_type(result->private_data, 
							    struct connect_multi_state);
	NTSTATUS status;
	multi->num_connects_recv++;

	status = socket_connect_recv(creq);

	if (NT_STATUS_IS_OK(status)) {
		multi->sock = talloc_steal(multi, state->sock);
		multi->result_port = state->addr->port;
	}

	talloc_free(state);

	if (NT_STATUS_IS_OK(status) || 
	    multi->num_connects_recv == multi->num_ports) {
		result->status = status;
		composite_done(result);
		return;
	}

	/* try the next port */
	connect_multi_next_socket(result);
}

/*
  async recv routine for socket_connect_multi()
 */
_PUBLIC_ NTSTATUS socket_connect_multi_recv(struct composite_context *ctx,
				   TALLOC_CTX *mem_ctx,
				   struct socket_context **sock,
				   uint16_t *port)
{
	NTSTATUS status = composite_wait(ctx);
	if (NT_STATUS_IS_OK(status)) {
		struct connect_multi_state *multi =
			talloc_get_type(ctx->private_data,
					struct connect_multi_state);
		*sock = talloc_steal(mem_ctx, multi->sock);
		*port = multi->result_port;
	}
	talloc_free(ctx);
	return status;
}

NTSTATUS socket_connect_multi(TALLOC_CTX *mem_ctx,
			      const char *server_address,
			      int num_server_ports, uint16_t *server_ports,
			      struct resolve_context *resolve_ctx,
			      struct tevent_context *event_ctx,
			      struct socket_context **result,
			      uint16_t *result_port)
{
	struct composite_context *ctx =
		socket_connect_multi_send(mem_ctx, server_address,
					  num_server_ports, server_ports,
					  resolve_ctx,
					  event_ctx);
	return socket_connect_multi_recv(ctx, mem_ctx, result, result_port);
}
