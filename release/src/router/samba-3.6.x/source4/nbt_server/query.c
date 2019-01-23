/* 
   Unix SMB/CIFS implementation.

   answer name queries

   Copyright (C) Andrew Tridgell	2005
   
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
#include "../lib/util/dlinklist.h"
#include "system/network.h"
#include "nbt_server/nbt_server.h"
#include "nbt_server/wins/winsserver.h"
#include "smbd/service_task.h"
#include "librpc/gen_ndr/ndr_nbt.h"
#include "lib/socket/socket.h"
#include "param/param.h"

/*
  answer a name query
*/
void nbtd_request_query(struct nbt_name_socket *nbtsock, 
			struct nbt_name_packet *packet, 
			struct socket_address *src)
{
	struct nbtd_iface_name *iname;
	struct nbt_name *name;
	struct nbtd_interface *iface = talloc_get_type(nbtsock->incoming.private_data,
						       struct nbtd_interface);

	/* see if its a node status query */
	if (packet->qdcount == 1 &&
	    packet->questions[0].question_type == NBT_QTYPE_STATUS) {
		nbtd_query_status(nbtsock, packet, src);
		return;
	}

	NBTD_ASSERT_PACKET(packet, src, packet->qdcount == 1);
	NBTD_ASSERT_PACKET(packet, src, 
			   packet->questions[0].question_type == NBT_QTYPE_NETBIOS);
	NBTD_ASSERT_PACKET(packet, src, 
			   packet->questions[0].question_class == NBT_QCLASS_IP);

	/* see if we have the requested name on this interface */
	name = &packet->questions[0].name;

	iname = nbtd_find_iname(iface, name, 0);
	if (iname == NULL) {
		/* don't send negative replies to broadcast queries */
		if (packet->operation & NBT_FLAG_BROADCAST) {
			return;
		}

		if (packet->operation & NBT_FLAG_RECURSION_DESIRED) {
			nbtd_winsserver_request(nbtsock, packet, src);
			return;
		}

		/* otherwise send a negative reply */
		nbtd_negative_name_query_reply(nbtsock, packet, src);
		return;
	}

	/*
	 * normally we should forward all queries with the
	 * recursion desired flag to the wins server, but this
	 * breaks are winsclient code, when doing mhomed registrations
	 */
	if (!(packet->operation & NBT_FLAG_BROADCAST) &&
	   (packet->operation & NBT_FLAG_RECURSION_DESIRED) &&
	   (iname->nb_flags & NBT_NM_GROUP) &&
	   lpcfg_wins_support(iface->nbtsrv->task->lp_ctx)) {
		nbtd_winsserver_request(nbtsock, packet, src);
		return;
	}

	/* if the name is not yet active and its a broadcast query then
	   ignore it for now */
	if (!(iname->nb_flags & NBT_NM_ACTIVE) && 
	    (packet->operation & NBT_FLAG_BROADCAST)) {
		DEBUG(7,("Query for %s from %s - name not active yet on %s\n",
			 nbt_name_string(packet, name), src->addr, iface->ip_address));
		return;
	}

	nbtd_name_query_reply(nbtsock, packet, src,
			      &iname->name, iname->ttl, iname->nb_flags, 
			      nbtd_address_list(iface, packet));
}
