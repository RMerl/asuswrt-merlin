/*
   Unix SMB/CIFS implementation.

   Echo example async client library

   Copyright (C) 2010 Kai Blin  <kai@samba.org>

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

#include "replace.h"
#include "system/network.h"
#include <tevent.h>
#include "lib/tsocket/tsocket.h"
#include "libcli/util/ntstatus.h"
#include "libcli/echo/libecho.h"
#include "lib/util/tevent_ntstatus.h"
#include "libcli/util/error.h"

/*
 * Following the Samba convention for async functions, set up a state struct
 * for this set of calls. The state is always called function_name_state for
 * the set of async functions related to function_name_send().
 */
struct echo_request_state {
	struct tevent_context *ev;
	ssize_t orig_len;
	struct tdgram_context *dgram;
	char *message;
};

/* Declare callback functions used below. */
static void echo_request_get_reply(struct tevent_req *subreq);
static void echo_request_done(struct tevent_req *subreq);

struct tevent_req *echo_request_send(TALLOC_CTX *mem_ctx,
				     struct tevent_context *ev,
				     const char *server_addr_string,
				     const char *message)
{
	struct tevent_req *req, *subreq;
	struct echo_request_state *state;
	struct tsocket_address *local_addr, *server_addr;
	struct tdgram_context *dgram;
	int ret;

	/*
	 * Creating the initial tevent_req is the only place where returning
	 * NULL is allowed. Everything after that should return a more
	 * meaningful error using tevent_req_post().
	 */
	req = tevent_req_create(mem_ctx, &state, struct echo_request_state);
	if (req == NULL) {
		return NULL;
	}

	/*
	 * We need to dispatch new async functions in the callbacks, hold
	 * on to the event context.
	 */
	state->ev = ev;

	/* libecho uses connected UDP sockets, take care of this here */
	ret = tsocket_address_inet_from_strings(state, "ip", NULL, 0,
						&local_addr);
	if (ret != 0) {
		tevent_req_nterror(req, map_nt_error_from_unix(ret));
		return tevent_req_post(req, ev);
	}

	ret = tsocket_address_inet_from_strings(state, "ip", server_addr_string,
						ECHO_PORT, &server_addr);
	if (ret != 0) {
		tevent_req_nterror(req, map_nt_error_from_unix(ret));
		return tevent_req_post(req, ev);
	}

	ret = tdgram_inet_udp_socket(local_addr, server_addr, state, &dgram);
	if (ret != 0) {
		tevent_req_nterror(req, map_nt_error_from_unix(ret));
		return tevent_req_post(req, ev);
	}

	state->dgram = dgram;
	state->orig_len = strlen(message) + 1;

	/* Start of a subrequest for the actual data sending */
	subreq = tdgram_sendto_send(state, ev, dgram,
				    (const uint8_t *) message,
				    state->orig_len, NULL);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}

	/*
	 * And tell tevent what to call when the subreq is done. Note that the
	 * original req structure is passed into the callback as callback data.
	 * This is used to get to the state struct in callbacks.
	 */
	tevent_req_set_callback(subreq, echo_request_get_reply, req);
	return req;
}

/*
 * The following two callbacks both demonstrate the way of getting back the
 * state struct in a callback function.
 */

static void echo_request_get_reply(struct tevent_req *subreq)
{
	/* Get the parent request struct from the callback data */
	struct tevent_req *req = tevent_req_callback_data(subreq,
						struct tevent_req);
	/* And get the state struct from the parent request struct */
	struct echo_request_state *state = tevent_req_data(req,
						struct echo_request_state);
	ssize_t len;
	int err = 0;

	len = tdgram_sendto_recv(subreq, &err);
	TALLOC_FREE(subreq);

	if (len == -1 && err != 0) {
		tevent_req_nterror(req, map_nt_error_from_unix(err));
		return;
	}

	if (len != state->orig_len) {
		tevent_req_nterror(req, NT_STATUS_UNEXPECTED_NETWORK_ERROR);
		return;
	}

	/* Send off the second subreq here, this time to receive the reply */
	subreq = tdgram_recvfrom_send(state, state->ev, state->dgram);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}

	/* And set the new callback */
	tevent_req_set_callback(subreq, echo_request_done, req);
	return;
}

static void echo_request_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(subreq,
						struct tevent_req);
	struct echo_request_state *state = tevent_req_data(req,
						struct echo_request_state);

	ssize_t len;
	int err = 0;

	len = tdgram_recvfrom_recv(subreq, &err, state,
				   (uint8_t **)&state->message,
				   NULL);
	TALLOC_FREE(subreq);

	if (len == -1 && err != 0) {
		tevent_req_nterror(req, map_nt_error_from_unix(err));
		return;
	}

	state->message[len-1] = '\0';
	/* Once the async function has completed, set tevent_req_done() */
	tevent_req_done(req);
}

/*
 * In the recv function, we usually need to move the data from the state struct
 * to the memory area owned by the caller. Also, the function
 * tevent_req_received() is called to take care of freeing the memory still
 * associated with the request.
 */

NTSTATUS echo_request_recv(struct tevent_req *req,
			   TALLOC_CTX *mem_ctx,
			   char **message)
{
	struct echo_request_state *state = tevent_req_data(req,
			struct echo_request_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		tevent_req_received(req);
		return status;
	}

	*message = talloc_move(mem_ctx, &state->message);
	tevent_req_received(req);

	return NT_STATUS_OK;
}
