/*
   Unix SMB/CIFS implementation.

   send out a name release request

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

/*
  send a nbt name release request
*/
_PUBLIC_ struct nbt_name_request *nbt_name_release_send(struct nbt_name_socket *nbtsock,
					       struct nbt_name_release *io)
{
	struct nbt_name_request *req;
	struct nbt_name_packet *packet;
	struct socket_address *dest;

	packet = talloc_zero(nbtsock, struct nbt_name_packet);
	if (packet == NULL) return NULL;

	packet->qdcount = 1;
	packet->arcount = 1;
	packet->operation = NBT_OPCODE_RELEASE;
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
	packet->additional[0].ttl                    = 0;
	packet->additional[0].rdata.netbios.length   = 6;
	packet->additional[0].rdata.netbios.addresses = talloc_array(packet->additional,
								     struct nbt_rdata_address, 1);
	if (packet->additional[0].rdata.netbios.addresses == NULL) goto failed;
	packet->additional[0].rdata.netbios.addresses[0].nb_flags = io->in.nb_flags;
	packet->additional[0].rdata.netbios.addresses[0].ipaddr =
		talloc_strdup(packet->additional, io->in.address);

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

/*
  wait for a release reply
*/
_PUBLIC_ NTSTATUS nbt_name_release_recv(struct nbt_name_request *req,
			       TALLOC_CTX *mem_ctx, struct nbt_name_release *io)
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
  synchronous name release request
*/
_PUBLIC_ NTSTATUS nbt_name_release(struct nbt_name_socket *nbtsock,
			   TALLOC_CTX *mem_ctx, struct nbt_name_release *io)
{
	struct nbt_name_request *req = nbt_name_release_send(nbtsock, io);
	return nbt_name_release_recv(req, mem_ctx, io);
}
