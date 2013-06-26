/* 
   Unix SMB/CIFS implementation.

   implements a non-blocking connect operation that is aware of the samba4
   events system

   Copyright (C) Andrew Tridgell 2005
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


struct connect_state {
	struct socket_context *sock;
	const struct socket_address *my_address;
	const struct socket_address *server_address;
	uint32_t flags;
};

static void socket_connect_handler(struct tevent_context *ev,
				   struct tevent_fd *fde, 
				   uint16_t flags, void *private_data);

/*
  call the real socket_connect() call, and setup event handler
*/
static void socket_send_connect(struct composite_context *result)
{
	struct tevent_fd *fde;
	struct connect_state *state = talloc_get_type(result->private_data, 
						      struct connect_state);

	result->status = socket_connect(state->sock,
					state->my_address,
					state->server_address,
					state->flags);
	if (NT_STATUS_IS_ERR(result->status) && 
	    !NT_STATUS_EQUAL(result->status,
			     NT_STATUS_MORE_PROCESSING_REQUIRED)) {
		composite_error(result, result->status);
		return;
	}

	fde = event_add_fd(result->event_ctx, result,
			   socket_get_fd(state->sock),
			   EVENT_FD_READ|EVENT_FD_WRITE, 
			   socket_connect_handler, result);
	composite_nomem(fde, result);
}


/*
  send a socket connect, potentially doing some name resolution first
*/
struct composite_context *socket_connect_send(struct socket_context *sock,
					      struct socket_address *my_address,
					      struct socket_address *server_address, 
					      uint32_t flags,
					      struct tevent_context *event_ctx)
{
	struct composite_context *result;
	struct connect_state *state;

	result = composite_create(sock, event_ctx);
	if (result == NULL) return NULL;

	state = talloc_zero(result, struct connect_state);
	if (composite_nomem(state, result)) return result;
	result->private_data = state;

	state->sock = talloc_reference(state, sock);
	if (composite_nomem(state->sock, result)) return result;

	if (my_address) {
		void *ref = talloc_reference(state, my_address);
		if (composite_nomem(ref, result)) {
			return result;
		}
		state->my_address = my_address;
	}

	{
		void *ref = talloc_reference(state, server_address);
		if (composite_nomem(ref, result)) {
			return result;
		}
		state->server_address = server_address;
	}

	state->flags = flags;

	set_blocking(socket_get_fd(sock), false);

	socket_send_connect(result);

	return result;
}

/*
  handle write events on connect completion
*/
static void socket_connect_handler(struct tevent_context *ev,
				   struct tevent_fd *fde, 
				   uint16_t flags, void *private_data)
{
	struct composite_context *result =
		talloc_get_type(private_data, struct composite_context);
	struct connect_state *state = talloc_get_type(result->private_data,
						      struct connect_state);

	result->status = socket_connect_complete(state->sock, state->flags);
	if (!composite_is_ok(result)) return;

	composite_done(result);
}

/*
  wait for a socket_connect_send() to finish
*/
NTSTATUS socket_connect_recv(struct composite_context *result)
{
	NTSTATUS status = composite_wait(result);
	talloc_free(result);
	return status;
}


/*
  like socket_connect() but takes an event context, doing a semi-async connect
*/
NTSTATUS socket_connect_ev(struct socket_context *sock,
			   struct socket_address *my_address,
			   struct socket_address *server_address, 
			   uint32_t flags,
			   struct tevent_context *ev)
{
	struct composite_context *ctx;
	ctx = socket_connect_send(sock, my_address, 
				  server_address, flags, ev);
	return socket_connect_recv(ctx);
}
