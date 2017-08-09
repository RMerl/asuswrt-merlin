/* 
   Unix SMB/CIFS implementation.

   general name resolution interface

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

#ifndef __LIBCLI_RESOLVE_H__
#define __LIBCLI_RESOLVE_H__

struct socket_address;
struct tevent_context;

#include "../libcli/nbt/libnbt.h"

/* force that only NBT name resolution is used */
#define RESOLVE_NAME_FLAG_FORCE_NBT		0x00000001
/* force that only DNS name resolution is used */
#define RESOLVE_NAME_FLAG_FORCE_DNS		0x00000002
/* tell the dns resolver to do a DNS SRV lookup */
#define RESOLVE_NAME_FLAG_DNS_SRV		0x00000004
/* allow the resolver to overwrite the given port, e.g. for DNS SRV */
#define RESOLVE_NAME_FLAG_OVERWRITE_PORT	0x00000008

typedef struct composite_context *(*resolve_name_send_fn)(TALLOC_CTX *mem_ctx,
							  struct tevent_context *,
							  void *privdata,
							  uint32_t flags,
							  uint16_t port,
							  struct nbt_name *);
typedef NTSTATUS (*resolve_name_recv_fn)(struct composite_context *creq,
					 TALLOC_CTX *mem_ctx,
					 struct socket_address ***addrs,
					 char ***names);
#include "libcli/resolve/proto.h"
struct interface;
#include "libcli/resolve/lp_proto.h"

#endif /* __LIBCLI_RESOLVE_H__ */
