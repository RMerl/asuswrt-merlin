/* 
   Unix SMB/CIFS implementation.

   async "host" name resolution module

   Copyright (C) Andrew Tridgell 2005
   Copyright (C) Stefan Metzmacher 2008

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
#include "lib/events/events.h"
#include "system/network.h"
#include "system/filesys.h"
#include "lib/socket/socket.h"
#include "libcli/composite/composite.h"
#include "librpc/gen_ndr/ndr_nbt.h"
#include "libcli/resolve/resolve.h"

/*
  getaddrinfo() (with fallback to dns_lookup()) name resolution method - async send
 */
struct composite_context *resolve_name_host_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *event_ctx,
						 void *privdata, uint32_t flags,
						 uint16_t port,
						 struct nbt_name *name)
{
	return resolve_name_dns_ex_send(mem_ctx, event_ctx, NULL, flags,
					port, name, true);
}

/*
  getaddrinfo() (with fallback to dns_lookup()) name resolution method - recv side
*/
NTSTATUS resolve_name_host_recv(struct composite_context *c, 
				TALLOC_CTX *mem_ctx,
				struct socket_address ***addrs,
				char ***names)
{
	return resolve_name_dns_ex_recv(c, mem_ctx, addrs, names);
}

bool resolve_context_add_host_method(struct resolve_context *ctx)
{
	return resolve_context_add_method(ctx, resolve_name_host_send, resolve_name_host_recv,
					  NULL);
}
