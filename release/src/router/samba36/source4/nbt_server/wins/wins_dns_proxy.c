/* 
   Unix SMB/CIFS implementation.

   wins server dns proxy

   Copyright (C) Stefan Metzmacher	2005
      
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
#include "nbt_server/nbt_server.h"
#include "nbt_server/wins/winsdb.h"
#include "nbt_server/wins/winsserver.h"
#include "system/time.h"
#include "libcli/composite/composite.h"
#include "smbd/service_task.h"
#include "libcli/resolve/resolve.h"

struct wins_dns_proxy_state {
	struct nbt_name_socket *nbtsock;
	struct nbt_name_packet *packet;
	struct socket_address *src;
};

static void nbtd_wins_dns_proxy_handler(struct composite_context *creq)
{
	NTSTATUS status;
	struct wins_dns_proxy_state *s = talloc_get_type(creq->async.private_data,
							 struct wins_dns_proxy_state);
	struct nbt_name *name = &s->packet->questions[0].name;
	const char *address;
	const char **addresses;
	uint16_t nb_flags = 0; /* TODO: ... */

	status = resolve_name_recv(creq, s->packet, &address);
	if (!NT_STATUS_IS_OK(status)) {
		goto notfound;
	}

	addresses = str_list_add(NULL, address);
	talloc_steal(s->packet, addresses);
	if (!addresses) goto notfound;

	nbtd_name_query_reply(s->nbtsock, s->packet, s->src, name, 
			      0, nb_flags, addresses);
	return;
notfound:
	nbtd_negative_name_query_reply(s->nbtsock, s->packet, s->src);
}

/*
  dns proxy query a name
*/
void nbtd_wins_dns_proxy_query(struct nbt_name_socket *nbtsock, 
			       struct nbt_name_packet *packet, 
			       struct socket_address *src)
{
	struct nbt_name *name = &packet->questions[0].name;
	struct nbtd_interface *iface = talloc_get_type(nbtsock->incoming.private_data,
						       struct nbtd_interface);
	struct wins_dns_proxy_state *s;
	struct composite_context *creq;
	struct resolve_context *resolve_ctx;

	s = talloc(nbtsock, struct wins_dns_proxy_state);
	if (!s) goto failed;
	s->nbtsock	= nbtsock;
	s->packet	= talloc_steal(s, packet);
	s->src		= src;
	if (!talloc_reference(s, src)) {
		goto failed;
	}

	resolve_ctx = resolve_context_init(s);
	if (resolve_ctx == NULL) goto failed;
	resolve_context_add_host_method(resolve_ctx);

	creq = resolve_name_send(resolve_ctx, s, name, iface->nbtsrv->task->event_ctx);
	if (!creq) goto failed;

	creq->async.fn		= nbtd_wins_dns_proxy_handler;
	creq->async.private_data= s;
	return;
failed:
	nbtd_negative_name_query_reply(nbtsock, packet, src);
}
