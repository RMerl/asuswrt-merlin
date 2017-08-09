/* 
   Unix SMB/CIFS implementation.

   wins name resolution module

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
#include "libcli/resolve/resolve.h"
#include "param/param.h"
#include "lib/socket/socket.h"
#include "lib/socket/netif.h"

struct resolve_wins_data {
	const char **address_list;
	struct interface *ifaces;
	uint16_t nbt_port;
	int nbt_timeout;
};

/**
  wins name resolution method - async send
 */
struct composite_context *resolve_name_wins_send(
				TALLOC_CTX *mem_ctx, 
				struct tevent_context *event_ctx,
				void *userdata,
				uint32_t flags,
				uint16_t port,
				struct nbt_name *name)
{
	struct resolve_wins_data *wins_data = talloc_get_type(userdata, struct resolve_wins_data);
	if (wins_data->address_list == NULL) return NULL;
	return resolve_name_nbtlist_send(mem_ctx, event_ctx, flags, port, name,
					 wins_data->address_list, wins_data->ifaces,
					 wins_data->nbt_port, wins_data->nbt_timeout,
					 false, true);
}

/*
  wins name resolution method - recv side
 */
NTSTATUS resolve_name_wins_recv(struct composite_context *c, 
				TALLOC_CTX *mem_ctx,
				struct socket_address ***addrs,
				char ***names)
{
	return resolve_name_nbtlist_recv(c, mem_ctx, addrs, names);
}

bool resolve_context_add_wins_method(struct resolve_context *ctx, const char **address_list, struct interface *ifaces, uint16_t nbt_port, int nbt_timeout)
{
	struct resolve_wins_data *wins_data = talloc(ctx, struct resolve_wins_data);
	wins_data->address_list = (const char **)str_list_copy(wins_data, address_list);
	wins_data->ifaces = talloc_reference(wins_data, ifaces);
	wins_data->nbt_port = nbt_port;
	wins_data->nbt_timeout = nbt_timeout;
	return resolve_context_add_method(ctx, resolve_name_wins_send, resolve_name_wins_recv,
					  wins_data);
}

bool resolve_context_add_wins_method_lp(struct resolve_context *ctx, struct loadparm_context *lp_ctx)
{
	struct interface *ifaces;
	load_interfaces(ctx, lpcfg_interfaces(lp_ctx), &ifaces);
	return resolve_context_add_wins_method(ctx, lpcfg_wins_server_list(lp_ctx), ifaces, lpcfg_nbt_port(lp_ctx), lpcfg_parm_int(lp_ctx, NULL, "nbt", "timeout", 1));
}
