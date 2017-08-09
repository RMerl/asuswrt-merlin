/* 
   Unix SMB/CIFS implementation.

   broadcast name resolution module

   Copyright (C) Andrew Tridgell 2005
   Copyright (C) Jelmer Vernooij 2007
   
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
#include "libcli/resolve/resolve.h"
#include "system/network.h"
#include "lib/socket/netif.h"
#include "param/param.h"

struct resolve_bcast_data {
	struct interface *ifaces;
	uint16_t nbt_port;
	int nbt_timeout;
};

/**
  broadcast name resolution method - async send
 */
struct composite_context *resolve_name_bcast_send(TALLOC_CTX *mem_ctx, 
						  struct tevent_context *event_ctx,
						  void *userdata, uint32_t flags,
						  uint16_t port,
						  struct nbt_name *name)
{
	int num_interfaces;
	const char **address_list;
	struct composite_context *c;
	int i, count=0;
	struct resolve_bcast_data *data = talloc_get_type(userdata, struct resolve_bcast_data);

	num_interfaces = iface_count(data->ifaces);

	address_list = talloc_array(mem_ctx, const char *, num_interfaces+1);
	if (address_list == NULL) return NULL;

	for (i=0;i<num_interfaces;i++) {
		const char *bcast = iface_n_bcast(data->ifaces, i);
		if (bcast == NULL) continue;
		address_list[count] = talloc_strdup(address_list, bcast);
		if (address_list[count] == NULL) {
			talloc_free(address_list);
			return NULL;
		}
		count++;
	}
	address_list[count] = NULL;

	c = resolve_name_nbtlist_send(mem_ctx, event_ctx, flags, port, name,
				      address_list, data->ifaces, data->nbt_port,
				      data->nbt_timeout, true, false);
	talloc_free(address_list);

	return c;	
}

/*
  broadcast name resolution method - recv side
 */
NTSTATUS resolve_name_bcast_recv(struct composite_context *c, 
				 TALLOC_CTX *mem_ctx,
				 struct socket_address ***addrs,
				 char ***names)
{
	NTSTATUS status = resolve_name_nbtlist_recv(c, mem_ctx, addrs, names);
	if (NT_STATUS_EQUAL(status, NT_STATUS_IO_TIMEOUT)) {
		/* this makes much more sense for a bcast name resolution
		   timeout */
		status = NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}
	return status;
}

bool resolve_context_add_bcast_method(struct resolve_context *ctx, struct interface *ifaces, uint16_t nbt_port, int nbt_timeout)
{
	struct resolve_bcast_data *data = talloc(ctx, struct resolve_bcast_data);
	data->ifaces = ifaces;
	data->nbt_port = nbt_port;
	data->nbt_timeout = nbt_timeout;
	return resolve_context_add_method(ctx, resolve_name_bcast_send, resolve_name_bcast_recv, data);
}

bool resolve_context_add_bcast_method_lp(struct resolve_context *ctx, struct loadparm_context *lp_ctx)
{
	struct interface *ifaces;
	load_interfaces(ctx, lpcfg_interfaces(lp_ctx), &ifaces);
	return resolve_context_add_bcast_method(ctx, ifaces, lpcfg_nbt_port(lp_ctx), lpcfg_parm_int(lp_ctx, NULL, "nbt", "timeout", 1));
}
