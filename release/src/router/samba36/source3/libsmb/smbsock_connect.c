/*
   Unix SMB/CIFS implementation.
   Connect to 445 and 139/nbsesssetup
   Copyright (C) Volker Lendecke 2010

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
#include "../lib/util/tevent_ntstatus.h"
#include "client.h"
#include "async_smb.h"
#include "libsmb/nmblib.h"

struct nb_connect_state {
	struct tevent_context *ev;
	const struct sockaddr_storage *addr;
	const char *called_name;
	int sock;

	struct nmb_name called;
	struct nmb_name calling;
};

static int nb_connect_state_destructor(struct nb_connect_state *state);
static void nb_connect_connected(struct tevent_req *subreq);
static void nb_connect_done(struct tevent_req *subreq);

static struct tevent_req *nb_connect_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  const struct sockaddr_storage *addr,
					  const char *called_name,
					  int called_type,
					  const char *calling_name,
					  int calling_type)
{
	struct tevent_req *req, *subreq;
	struct nb_connect_state *state;

	req = tevent_req_create(mem_ctx, &state, struct nb_connect_state);
	if (req == NULL) {
		return NULL;
	}
	state->ev = ev;
	state->called_name = called_name;
	state->addr = addr;

	state->sock = -1;
	make_nmb_name(&state->called, called_name, called_type);
	make_nmb_name(&state->calling, calling_name, calling_type);

	talloc_set_destructor(state, nb_connect_state_destructor);

	subreq = open_socket_out_send(state, ev, addr, 139, 5000);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, nb_connect_connected, req);
	return req;
}

static int nb_connect_state_destructor(struct nb_connect_state *state)
{
	if (state->sock != -1) {
		close(state->sock);
	}
	return 0;
}

static void nb_connect_connected(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct nb_connect_state *state = tevent_req_data(
		req, struct nb_connect_state);
	NTSTATUS status;

	status = open_socket_out_recv(subreq, &state->sock);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}
	subreq = cli_session_request_send(state, state->ev, state->sock,
					  &state->called, &state->calling);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, nb_connect_done, req);
}

static void nb_connect_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct nb_connect_state *state = tevent_req_data(
		req, struct nb_connect_state);
	bool ret;
	int err;
	uint8_t resp;

	ret = cli_session_request_recv(subreq, &err, &resp);
	TALLOC_FREE(subreq);
	if (!ret) {
		tevent_req_nterror(req, map_nt_error_from_unix(err));
		return;
	}

	/*
	 * RFC1002: 0x82 - POSITIVE SESSION RESPONSE
	 */

	if (resp != 0x82) {
		/*
		 * The server did not like our session request
		 */
		close(state->sock);
		state->sock = -1;

		if (strequal(state->called_name, "*SMBSERVER")) {
			/*
			 * Here we could try a name status request and
			 * use the first 0x20 type name.
			 */
			tevent_req_nterror(
				req, NT_STATUS_RESOURCE_NAME_NOT_FOUND);
			return;
		}

		/*
		 * We could be subtle and distinguish between
		 * different failure modes, but what we do here
		 * instead is just retry with *SMBSERVER type 0x20.
		 */
		state->called_name = "*SMBSERVER";
		make_nmb_name(&state->called, state->called_name, 0x20);

		subreq = open_socket_out_send(state, state->ev, state->addr,
					      139, 5000);
		if (tevent_req_nomem(subreq, req)) {
			return;
		}
		tevent_req_set_callback(subreq, nb_connect_connected, req);
		return;
	}

	tevent_req_done(req);
	return;

}

static NTSTATUS nb_connect_recv(struct tevent_req *req, int *sock)
{
	struct nb_connect_state *state = tevent_req_data(
		req, struct nb_connect_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}
	*sock = state->sock;
	state->sock = -1;
	return NT_STATUS_OK;
}

struct smbsock_connect_state {
	struct tevent_context *ev;
	const struct sockaddr_storage *addr;
	const char *called_name;
	uint8_t called_type;
	const char *calling_name;
	uint8_t calling_type;
	struct tevent_req *req_139;
	struct tevent_req *req_445;
	int sock;
	uint16_t port;
};

static int smbsock_connect_state_destructor(
	struct smbsock_connect_state *state);
static void smbsock_connect_connected(struct tevent_req *subreq);
static void smbsock_connect_do_139(struct tevent_req *subreq);

struct tevent_req *smbsock_connect_send(TALLOC_CTX *mem_ctx,
					struct tevent_context *ev,
					const struct sockaddr_storage *addr,
					uint16_t port,
					const char *called_name,
					int called_type,
					const char *calling_name,
					int calling_type)
{
	struct tevent_req *req, *subreq;
	struct smbsock_connect_state *state;

	req = tevent_req_create(mem_ctx, &state, struct smbsock_connect_state);
	if (req == NULL) {
		return NULL;
	}
	state->ev = ev;
	state->addr = addr;
	state->sock = -1;
	state->called_name =
		(called_name != NULL) ? called_name : "*SMBSERVER";
	state->called_type =
		(called_type != -1) ? called_type : 0x20;
	state->calling_name =
		(calling_name != NULL) ? calling_name : global_myname();
	state->calling_type =
		(calling_type != -1) ? calling_type : 0x00;

	talloc_set_destructor(state, smbsock_connect_state_destructor);

	if (port == 139) {
		subreq = tevent_wakeup_send(state, ev, timeval_set(0, 0));
		if (tevent_req_nomem(subreq, req)) {
			return tevent_req_post(req, ev);
		}
		tevent_req_set_callback(subreq, smbsock_connect_do_139, req);
		return req;
	}
	if (port != 0) {
		state->req_445 = open_socket_out_send(state, ev, addr, port,
						      5000);
		if (tevent_req_nomem(state->req_445, req)) {
			return tevent_req_post(req, ev);
		}
		tevent_req_set_callback(
			state->req_445, smbsock_connect_connected, req);
		return req;
	}

	/*
	 * port==0, try both
	 */

	state->req_445 = open_socket_out_send(state, ev, addr, 445, 5000);
	if (tevent_req_nomem(state->req_445, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(state->req_445, smbsock_connect_connected,
				req);

	/*
	 * After 5 msecs, fire the 139 request
	 */
	state->req_139 = tevent_wakeup_send(
		state, ev, timeval_current_ofs(0, 5000));
	if (tevent_req_nomem(state->req_139, req)) {
		TALLOC_FREE(state->req_445);
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(state->req_139, smbsock_connect_do_139,
				req);
	return req;
}

static int smbsock_connect_state_destructor(
	struct smbsock_connect_state *state)
{
	if (state->sock != -1) {
		close(state->sock);
		state->sock = -1;
	}
	return 0;
}

static void smbsock_connect_do_139(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct smbsock_connect_state *state = tevent_req_data(
		req, struct smbsock_connect_state);
	bool ret;

	ret = tevent_wakeup_recv(subreq);
	TALLOC_FREE(subreq);
	if (!ret) {
		tevent_req_nterror(req, NT_STATUS_INTERNAL_ERROR);
		return;
	}
	state->req_139 = nb_connect_send(state, state->ev, state->addr,
					 state->called_name,
					 state->called_type,
					 state->calling_name,
					 state->calling_type);
	if (tevent_req_nomem(state->req_139, req)) {
		return;
	}
	tevent_req_set_callback(state->req_139, smbsock_connect_connected,
				req);
}

static void smbsock_connect_connected(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct smbsock_connect_state *state = tevent_req_data(
		req, struct smbsock_connect_state);
	struct tevent_req *unfinished_req;
	NTSTATUS status;

	if (subreq == state->req_445) {

		status = open_socket_out_recv(subreq, &state->sock);
		TALLOC_FREE(state->req_445);
		unfinished_req = state->req_139;
		state->port = 445;

	} else if (subreq == state->req_139) {

		status = nb_connect_recv(subreq, &state->sock);
		TALLOC_FREE(state->req_139);
		unfinished_req = state->req_445;
		state->port = 139;

	} else {
		tevent_req_nterror(req, NT_STATUS_INTERNAL_ERROR);
		return;
	}

	if (NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(unfinished_req);
		state->req_139 = NULL;
		state->req_445 = NULL;
		tevent_req_done(req);
		return;
	}
	if (unfinished_req == NULL) {
		/*
		 * Both requests failed
		 */
		tevent_req_nterror(req, status);
		return;
	}
	/*
	 * Do nothing, wait for the second request to come here.
	 */
}

NTSTATUS smbsock_connect_recv(struct tevent_req *req, int *sock,
			      uint16_t *ret_port)
{
	struct smbsock_connect_state *state = tevent_req_data(
		req, struct smbsock_connect_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}
	*sock = state->sock;
	state->sock = -1;
	if (ret_port != NULL) {
		*ret_port = state->port;
	}
	return NT_STATUS_OK;
}

NTSTATUS smbsock_connect(const struct sockaddr_storage *addr, uint16_t port,
			 const char *called_name, int called_type,
			 const char *calling_name, int calling_type,
			 int *pfd, uint16_t *ret_port, int sec_timeout)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev;
	struct tevent_req *req;
	NTSTATUS status = NT_STATUS_NO_MEMORY;

	ev = event_context_init(frame);
	if (ev == NULL) {
		goto fail;
	}
	req = smbsock_connect_send(frame, ev, addr, port,
				   called_name, called_type,
				   calling_name, calling_type);
	if (req == NULL) {
		goto fail;
	}
	if ((sec_timeout != 0) &&
	    !tevent_req_set_endtime(
		    req, ev, timeval_current_ofs(sec_timeout, 0))) {
		goto fail;
	}
	if (!tevent_req_poll_ntstatus(req, ev, &status)) {
		goto fail;
	}
	status = smbsock_connect_recv(req, pfd, ret_port);
 fail:
	TALLOC_FREE(frame);
	return status;
}

struct smbsock_any_connect_state {
	struct tevent_context *ev;
	const struct sockaddr_storage *addrs;
	const char **called_names;
	int *called_types;
	const char **calling_names;
	int *calling_types;
	size_t num_addrs;
	uint16_t port;

	struct tevent_req **requests;
	size_t num_sent;
	size_t num_received;

	int fd;
	uint16_t chosen_port;
	size_t chosen_index;
};

static bool smbsock_any_connect_send_next(
	struct tevent_req *req,	struct smbsock_any_connect_state *state);
static void smbsock_any_connect_trynext(struct tevent_req *subreq);
static void smbsock_any_connect_connected(struct tevent_req *subreq);

struct tevent_req *smbsock_any_connect_send(TALLOC_CTX *mem_ctx,
					    struct tevent_context *ev,
					    const struct sockaddr_storage *addrs,
					    const char **called_names,
					    int *called_types,
					    const char **calling_names,
					    int *calling_types,
					    size_t num_addrs, uint16_t port)
{
	struct tevent_req *req, *subreq;
	struct smbsock_any_connect_state *state;

	req = tevent_req_create(mem_ctx, &state,
				struct smbsock_any_connect_state);
	if (req == NULL) {
		return NULL;
	}
	state->ev = ev;
	state->addrs = addrs;
	state->num_addrs = num_addrs;
	state->called_names = called_names;
	state->called_types = called_types;
	state->calling_names = calling_names;
	state->calling_types = calling_types;
	state->port = port;

	if (num_addrs == 0) {
		tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return tevent_req_post(req, ev);
	}

	state->requests = talloc_zero_array(state, struct tevent_req *,
					    num_addrs);
	if (tevent_req_nomem(state->requests, req)) {
		return tevent_req_post(req, ev);
	}
	if (!smbsock_any_connect_send_next(req, state)) {
		return tevent_req_post(req, ev);
	}
	if (state->num_sent >= state->num_addrs) {
		return req;
	}
	subreq = tevent_wakeup_send(state, ev, timeval_current_ofs(0, 10000));
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, smbsock_any_connect_trynext, req);
	return req;
}

static void smbsock_any_connect_trynext(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct smbsock_any_connect_state *state = tevent_req_data(
		req, struct smbsock_any_connect_state);
	bool ret;

	ret = tevent_wakeup_recv(subreq);
	TALLOC_FREE(subreq);
	if (!ret) {
		tevent_req_nterror(req, NT_STATUS_INTERNAL_ERROR);
		return;
	}
	if (!smbsock_any_connect_send_next(req, state)) {
		return;
	}
	if (state->num_sent >= state->num_addrs) {
		return;
	}
	subreq = tevent_wakeup_send(state, state->ev,
				    tevent_timeval_set(0, 10000));
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, smbsock_any_connect_trynext, req);
}

static bool smbsock_any_connect_send_next(
	struct tevent_req *req, struct smbsock_any_connect_state *state)
{
	struct tevent_req *subreq;

	if (state->num_sent >= state->num_addrs) {
		tevent_req_nterror(req, NT_STATUS_INTERNAL_ERROR);
		return false;
	}
	subreq = smbsock_connect_send(
		state->requests, state->ev, &state->addrs[state->num_sent],
		state->port,
		(state->called_names != NULL)
		? state->called_names[state->num_sent] : NULL,
		(state->called_types != NULL)
		? state->called_types[state->num_sent] : -1,
		(state->calling_names != NULL)
		? state->calling_names[state->num_sent] : NULL,
		(state->calling_types != NULL)
		? state->calling_types[state->num_sent] : -1);
	if (tevent_req_nomem(subreq, req)) {
		return false;
	}
	tevent_req_set_callback(subreq, smbsock_any_connect_connected, req);

	state->requests[state->num_sent] = subreq;
	state->num_sent += 1;

	return true;
}

static void smbsock_any_connect_connected(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct smbsock_any_connect_state *state = tevent_req_data(
		req, struct smbsock_any_connect_state);
	NTSTATUS status;
	int fd;
	uint16_t chosen_port;
	size_t i;
	size_t chosen_index = 0;

	for (i=0; i<state->num_sent; i++) {
		if (state->requests[i] == subreq) {
			chosen_index = i;
			break;
		}
	}
	if (i == state->num_sent) {
		tevent_req_nterror(req, NT_STATUS_INTERNAL_ERROR);
		return;
	}

	status = smbsock_connect_recv(subreq, &fd, &chosen_port);

	TALLOC_FREE(subreq);
	state->requests[chosen_index] = NULL;

	if (NT_STATUS_IS_OK(status)) {
		/*
		 * This will kill all the other requests
		 */
		TALLOC_FREE(state->requests);
		state->fd = fd;
		state->chosen_port = chosen_port;
		state->chosen_index = chosen_index;
		tevent_req_done(req);
		return;
	}

	state->num_received += 1;
	if (state->num_received <= state->num_addrs) {
		/*
		 * More addrs pending, wait for the others
		 */
		return;
	}

	/*
	 * This is the last response, none succeeded.
	 */
	tevent_req_nterror(req, status);
	return;
}

NTSTATUS smbsock_any_connect_recv(struct tevent_req *req, int *pfd,
				  size_t *chosen_index,
				  uint16_t *chosen_port)
{
	struct smbsock_any_connect_state *state = tevent_req_data(
		req, struct smbsock_any_connect_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}
	*pfd = state->fd;
	if (chosen_index != NULL) {
		*chosen_index = state->chosen_index;
	}
	if (chosen_port != NULL) {
		*chosen_port = state->chosen_port;
	}
	return NT_STATUS_OK;
}

NTSTATUS smbsock_any_connect(const struct sockaddr_storage *addrs,
			     const char **called_names,
			     int *called_types,
			     const char **calling_names,
			     int *calling_types,
			     size_t num_addrs,
			     uint16_t port,
			     int sec_timeout,
			     int *pfd, size_t *chosen_index,
			     uint16_t *chosen_port)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev;
	struct tevent_req *req;
	NTSTATUS status = NT_STATUS_NO_MEMORY;

	ev = event_context_init(frame);
	if (ev == NULL) {
		goto fail;
	}
	req = smbsock_any_connect_send(frame, ev, addrs,
				       called_names, called_types,
				       calling_names, calling_types,
				       num_addrs, port);
	if (req == NULL) {
		goto fail;
	}
	if ((sec_timeout != 0) &&
	    !tevent_req_set_endtime(
		    req, ev, timeval_current_ofs(sec_timeout, 0))) {
		goto fail;
	}
	if (!tevent_req_poll_ntstatus(req, ev, &status)) {
		goto fail;
	}
	status = smbsock_any_connect_recv(req, pfd, chosen_index, chosen_port);
 fail:
	TALLOC_FREE(frame);
	return status;
}
