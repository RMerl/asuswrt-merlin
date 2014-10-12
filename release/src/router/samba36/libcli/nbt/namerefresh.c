/*
   Unix SMB/CIFS implementation.

   send out a name refresh request

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

#include "includes.h"
#include <tevent.h>
#include "../libcli/nbt/libnbt.h"
#include "../libcli/nbt/nbt_proto.h"
#include "lib/socket/socket.h"
#include "lib/util/tevent_ntstatus.h"

/*
  send a nbt name refresh request
*/
struct nbt_name_request *nbt_name_refresh_send(struct nbt_name_socket *nbtsock,
					       struct nbt_name_refresh *io)
{
	struct nbt_name_request *req;
	struct nbt_name_packet *packet;
	struct socket_address *dest;

	packet = talloc_zero(nbtsock, struct nbt_name_packet);
	if (packet == NULL) return NULL;

	packet->qdcount = 1;
	packet->arcount = 1;
	packet->operation = NBT_OPCODE_REFRESH;
	if (io->in.broadcast) {
		packet->operation |= NBT_FLAG_BROADCAST;
	}

	packet->questions = talloc_array(packet, struct nbt_name_question, 1);
	if (packet->questions == NULL) goto failed;

	packet->questions[0].name           = io->in.name;
	packet->questions[0].question_type  = NBT_QTYPE_NETBIOS;
	packet->questions[0].question_class = NBT_QCLASS_IP;

	packet->additional = talloc_array(packet, struct nbt_res_rec, 1);
	if (packet->additional == NULL) goto failed;

	packet->additional[0].name                   = io->in.name;
	packet->additional[0].rr_type                = NBT_QTYPE_NETBIOS;
	packet->additional[0].rr_class               = NBT_QCLASS_IP;
	packet->additional[0].ttl                    = io->in.ttl;
	packet->additional[0].rdata.netbios.length   = 6;
	packet->additional[0].rdata.netbios.addresses = talloc_array(packet->additional,
								     struct nbt_rdata_address, 1);
	if (packet->additional[0].rdata.netbios.addresses == NULL) goto failed;
	packet->additional[0].rdata.netbios.addresses[0].nb_flags = io->in.nb_flags;
	packet->additional[0].rdata.netbios.addresses[0].ipaddr =
		talloc_strdup(packet->additional, io->in.address);

	dest = socket_address_from_strings(nbtsock,
					   nbtsock->sock->backend_name,
					   io->in.dest_addr, io->in.dest_port);
	if (dest == NULL) goto failed;
	req = nbt_name_request_send(nbtsock, dest, packet,
				    io->in.timeout, io->in.retries, false);
	if (req == NULL) goto failed;

	talloc_free(packet);
	return req;

failed:
	talloc_free(packet);
	return NULL;
}

/*
  wait for a refresh reply
*/
_PUBLIC_ NTSTATUS nbt_name_refresh_recv(struct nbt_name_request *req,
			       TALLOC_CTX *mem_ctx, struct nbt_name_refresh *io)
{
	NTSTATUS status;
	struct nbt_name_packet *packet;

	status = nbt_name_request_recv(req);
	if (!NT_STATUS_IS_OK(status) ||
	    req->num_replies == 0) {
		talloc_free(req);
		return status;
	}

	packet = req->replies[0].packet;
	io->out.reply_from = talloc_steal(mem_ctx, req->replies[0].dest->addr);

	if (packet->ancount != 1 ||
	    packet->answers[0].rr_type != NBT_QTYPE_NETBIOS ||
	    packet->answers[0].rr_class != NBT_QCLASS_IP) {
		talloc_free(req);
		return NT_STATUS_INVALID_NETWORK_RESPONSE;
	}

	io->out.rcode = packet->operation & NBT_RCODE;
	io->out.name = packet->answers[0].name;
	if (packet->answers[0].rdata.netbios.length < 6) {
		talloc_free(req);
		return NT_STATUS_INVALID_NETWORK_RESPONSE;
	}
	io->out.reply_addr = talloc_steal(mem_ctx,
					  packet->answers[0].rdata.netbios.addresses[0].ipaddr);
	talloc_steal(mem_ctx, io->out.name.name);
	talloc_steal(mem_ctx, io->out.name.scope);

	talloc_free(req);

	return NT_STATUS_OK;
}

/*
  synchronous name refresh request
*/
_PUBLIC_ NTSTATUS nbt_name_refresh(struct nbt_name_socket *nbtsock,
			   TALLOC_CTX *mem_ctx, struct nbt_name_refresh *io)
{
	struct nbt_name_request *req = nbt_name_refresh_send(nbtsock, io);
	return nbt_name_refresh_recv(req, mem_ctx, io);
}



/**
  a wins name refresh with multiple WINS servers and multiple
  addresses to refresh. Try each WINS server in turn, until we get a
  reply for each address
*/
struct nbt_name_refresh_wins_state {
	struct nbt_name_socket *nbtsock;
	struct nbt_name_refresh *io;
	char **wins_servers;
	uint16_t wins_port;
	char **addresses;
	int address_idx;
};

static void nbt_name_refresh_wins_handler(struct nbt_name_request *subreq);

/**
  the async send call for a multi-server WINS refresh
*/
_PUBLIC_ struct tevent_req *nbt_name_refresh_wins_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct nbt_name_socket *nbtsock,
						struct nbt_name_refresh_wins *io)
{
	struct tevent_req *req;
	struct nbt_name_refresh_wins_state *state;
	struct nbt_name_request *subreq;

	req = tevent_req_create(mem_ctx, &state,
				struct nbt_name_refresh_wins_state);
	if (req == NULL) {
		return NULL;
	}

	state->io = talloc(state, struct nbt_name_refresh);
	if (tevent_req_nomem(state->io, req)) {
		return tevent_req_post(req, ev);
	}

	if (io->in.wins_servers == NULL) {
		tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return tevent_req_post(req, ev);
	}

	if (io->in.wins_servers[0] == NULL) {
		tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return tevent_req_post(req, ev);
	}

	if (io->in.addresses == NULL) {
		tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return tevent_req_post(req, ev);
	}

	if (io->in.addresses[0] == NULL) {
		tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return tevent_req_post(req, ev);
	}

	state->wins_port = io->in.wins_port;
	state->wins_servers = str_list_copy(state, io->in.wins_servers);
	if (tevent_req_nomem(state->wins_servers, req)) {
		return tevent_req_post(req, ev);
	}

	state->addresses = str_list_copy(state, io->in.addresses);
	if (tevent_req_nomem(state->addresses, req)) {
		return tevent_req_post(req, ev);
	}

	state->io->in.name            = io->in.name;
	state->io->in.dest_addr       = state->wins_servers[0];
	state->io->in.dest_port       = state->wins_port;
	state->io->in.address         = io->in.addresses[0];
	state->io->in.nb_flags        = io->in.nb_flags;
	state->io->in.broadcast       = false;
	state->io->in.ttl             = io->in.ttl;
	state->io->in.timeout         = 2;
	state->io->in.retries         = 2;

	state->nbtsock     = nbtsock;
	state->address_idx = 0;

	subreq = nbt_name_refresh_send(nbtsock, state->io);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}

	subreq->async.fn = nbt_name_refresh_wins_handler;
	subreq->async.private_data = req;

	return req;
}

static void nbt_name_refresh_wins_handler(struct nbt_name_request *subreq)
{
	struct tevent_req *req =
		talloc_get_type_abort(subreq->async.private_data,
		struct tevent_req);
	struct nbt_name_refresh_wins_state *state =
		tevent_req_data(req,
		struct nbt_name_refresh_wins_state);
	NTSTATUS status;

	status = nbt_name_refresh_recv(subreq, state, state->io);
	if (NT_STATUS_EQUAL(status, NT_STATUS_IO_TIMEOUT)) {
		/* the refresh timed out - try the next WINS server */
		state->wins_servers++;
		if (state->wins_servers[0] == NULL) {
			tevent_req_nterror(req, status);
			return;
		}

		state->address_idx = 0;
		state->io->in.dest_addr = state->wins_servers[0];
		state->io->in.dest_port = state->wins_port;
		state->io->in.address   = state->addresses[0];

		subreq = nbt_name_refresh_send(state->nbtsock, state->io);
		if (tevent_req_nomem(subreq, req)) {
			return;
		}
		subreq->async.fn = nbt_name_refresh_wins_handler;
		subreq->async.private_data = req;
	} else if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}

	if (state->io->out.rcode == 0 &&
	    state->addresses[state->address_idx+1] != NULL) {
		/* refresh our next address */
		state->io->in.address = state->addresses[++(state->address_idx)];
		subreq = nbt_name_refresh_send(state->nbtsock, state->io);
		if (tevent_req_nomem(subreq, req)) {
			return;
		}
		subreq->async.fn = nbt_name_refresh_wins_handler;
		subreq->async.private_data = req;
		return;
	}

	tevent_req_done(req);
}

/*
  multi-homed WINS name refresh - recv side
*/
_PUBLIC_ NTSTATUS nbt_name_refresh_wins_recv(struct tevent_req *req,
					     TALLOC_CTX *mem_ctx,
					     struct nbt_name_refresh_wins *io)
{
	struct nbt_name_refresh_wins_state *state =
		tevent_req_data(req,
		struct nbt_name_refresh_wins_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		tevent_req_received(req);
		return status;
	}

	io->out.wins_server = talloc_move(mem_ctx, &state->wins_servers[0]);
	io->out.rcode = state->io->out.rcode;

	tevent_req_received(req);
	return NT_STATUS_OK;
}

/*
  multi-homed WINS refresh - sync interface
*/
_PUBLIC_ NTSTATUS nbt_name_refresh_wins(struct nbt_name_socket *nbtsock,
					TALLOC_CTX *mem_ctx,
					struct nbt_name_refresh_wins *io)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct tevent_context *ev;
	struct tevent_req *subreq;
	NTSTATUS status;

	/*
	 * TODO: create a temporary event context
	 */
	ev = nbtsock->event_ctx;

	subreq = nbt_name_refresh_wins_send(frame, ev, nbtsock, io);
	if (subreq == NULL) {
		talloc_free(frame);
		return NT_STATUS_NO_MEMORY;
	}

	if (!tevent_req_poll(subreq, ev)) {
		status = map_nt_error_from_unix(errno);
		talloc_free(frame);
		return status;
	}

	status = nbt_name_refresh_wins_recv(subreq, mem_ctx, io);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(frame);
		return status;
	}

	TALLOC_FREE(frame);
	return NT_STATUS_OK;
}
