/*
   Unix SMB/CIFS implementation.

   make nbt name query requests

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
#include "../libcli/nbt/libnbt.h"
#include "../libcli/nbt/nbt_proto.h"
#include "lib/socket/socket.h"

/**
  send a nbt name query
*/
_PUBLIC_ struct nbt_name_request *nbt_name_query_send(struct nbt_name_socket *nbtsock,
					     struct nbt_name_query *io)
{
	struct nbt_name_request *req;
	struct nbt_name_packet *packet;
	struct socket_address *dest;

	packet = talloc_zero(nbtsock, struct nbt_name_packet);
	if (packet == NULL) return NULL;

	packet->qdcount = 1;
	packet->operation = NBT_OPCODE_QUERY;
	if (io->in.broadcast) {
		packet->operation |= NBT_FLAG_BROADCAST;
	}
	if (io->in.wins_lookup) {
		packet->operation |= NBT_FLAG_RECURSION_DESIRED;
	}

	packet->questions = talloc_array(packet, struct nbt_name_question, 1);
	if (packet->questions == NULL) goto failed;

	packet->questions[0].name = io->in.name;
	packet->questions[0].question_type = NBT_QTYPE_NETBIOS;
	packet->questions[0].question_class = NBT_QCLASS_IP;

	dest = socket_address_from_strings(packet, nbtsock->sock->backend_name,
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

/**
  wait for a name query reply
*/
_PUBLIC_ NTSTATUS nbt_name_query_recv(struct nbt_name_request *req,
			     TALLOC_CTX *mem_ctx, struct nbt_name_query *io)
{
	NTSTATUS status;
	struct nbt_name_packet *packet;
	int i;

	status = nbt_name_request_recv(req);
	if (!NT_STATUS_IS_OK(status) ||
	    req->num_replies == 0) {
		talloc_free(req);
		return status;
	}

	packet = req->replies[0].packet;
	io->out.reply_from = talloc_steal(mem_ctx, req->replies[0].dest->addr);

	if ((packet->operation & NBT_RCODE) != 0) {
		status = nbt_rcode_to_ntstatus(packet->operation & NBT_RCODE);
		talloc_free(req);
		return status;
	}

	if (packet->ancount != 1 ||
	    packet->answers[0].rr_type != NBT_QTYPE_NETBIOS ||
	    packet->answers[0].rr_class != NBT_QCLASS_IP) {
		talloc_free(req);
		return status;
	}

	io->out.name = packet->answers[0].name;
	io->out.num_addrs = packet->answers[0].rdata.netbios.length / 6;
	io->out.reply_addrs = talloc_array(mem_ctx, const char *, io->out.num_addrs+1);
	if (io->out.reply_addrs == NULL) {
		talloc_free(req);
		return NT_STATUS_NO_MEMORY;
	}

	for (i=0;i<io->out.num_addrs;i++) {
		io->out.reply_addrs[i] = talloc_steal(io->out.reply_addrs,
						      packet->answers[0].rdata.netbios.addresses[i].ipaddr);
	}
	io->out.reply_addrs[i] = NULL;

	talloc_steal(mem_ctx, io->out.name.name);
	talloc_steal(mem_ctx, io->out.name.scope);

	talloc_free(req);

	return NT_STATUS_OK;
}

/**
  wait for a name query reply
*/
_PUBLIC_ NTSTATUS nbt_name_query(struct nbt_name_socket *nbtsock,
			TALLOC_CTX *mem_ctx, struct nbt_name_query *io)
{
	struct nbt_name_request *req = nbt_name_query_send(nbtsock, io);
	return nbt_name_query_recv(req, mem_ctx, io);
}


/**
  send a nbt name status
*/
_PUBLIC_ struct nbt_name_request *nbt_name_status_send(struct nbt_name_socket *nbtsock,
					      struct nbt_name_status *io)
{
	struct nbt_name_request *req;
	struct nbt_name_packet *packet;
	struct socket_address *dest;

	packet = talloc_zero(nbtsock, struct nbt_name_packet);
	if (packet == NULL) return NULL;

	packet->qdcount = 1;
	packet->operation = NBT_OPCODE_QUERY;

	packet->questions = talloc_array(packet, struct nbt_name_question, 1);
	if (packet->questions == NULL) goto failed;

	packet->questions[0].name = io->in.name;
	packet->questions[0].question_type = NBT_QTYPE_STATUS;
	packet->questions[0].question_class = NBT_QCLASS_IP;

	dest = socket_address_from_strings(packet, nbtsock->sock->backend_name,
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

/**
  wait for a name status reply
*/
_PUBLIC_ NTSTATUS nbt_name_status_recv(struct nbt_name_request *req,
			     TALLOC_CTX *mem_ctx, struct nbt_name_status *io)
{
	NTSTATUS status;
	struct nbt_name_packet *packet;
	int i;

	status = nbt_name_request_recv(req);
	if (!NT_STATUS_IS_OK(status) ||
	    req->num_replies == 0) {
		talloc_free(req);
		return status;
	}

	packet = req->replies[0].packet;
	io->out.reply_from = talloc_steal(mem_ctx, req->replies[0].dest->addr);

	if ((packet->operation & NBT_RCODE) != 0) {
		status = nbt_rcode_to_ntstatus(packet->operation & NBT_RCODE);
		talloc_free(req);
		return status;
	}

	if (packet->ancount != 1 ||
	    packet->answers[0].rr_type != NBT_QTYPE_STATUS ||
	    packet->answers[0].rr_class != NBT_QCLASS_IP) {
		talloc_free(req);
		return NT_STATUS_INVALID_NETWORK_RESPONSE;
	}

	io->out.name = packet->answers[0].name;
	talloc_steal(mem_ctx, io->out.name.name);
	talloc_steal(mem_ctx, io->out.name.scope);

	io->out.status = packet->answers[0].rdata.status;
	talloc_steal(mem_ctx, io->out.status.names);
	for (i=0;i<io->out.status.num_names;i++) {
		talloc_steal(io->out.status.names, io->out.status.names[i].name);
	}


	talloc_free(req);

	return NT_STATUS_OK;
}

/**
  wait for a name status reply
*/
_PUBLIC_ NTSTATUS nbt_name_status(struct nbt_name_socket *nbtsock,
			TALLOC_CTX *mem_ctx, struct nbt_name_status *io)
{
	struct nbt_name_request *req = nbt_name_status_send(nbtsock, io);
	return nbt_name_status_recv(req, mem_ctx, io);
}


