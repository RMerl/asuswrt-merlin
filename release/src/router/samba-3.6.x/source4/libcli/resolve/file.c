/* 
   Unix SMB/CIFS implementation.

   broadcast name resolution module

   Copyright (C) Andrew Tridgell 1994-1998,2005
   Copyright (C) Jeremy Allison 2007
   Copyright (C) Jelmer Vernooij 2007
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2009-2010
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
#include "libcli/composite/composite.h"
#include "libcli/resolve/resolve.h"
#include "lib/socket/socket.h"
#include "system/network.h"
#include "lib/socket/netif.h"
#include "param/param.h"
#include "lib/util/util_net.h"
#include "libcli/nbt/libnbt.h"

struct resolve_file_data {
	const char *lookup_file;
	const char *default_domain;
};

struct resolve_file_state {
	struct socket_address **addrs;
	char **names;
};

/**
  broadcast name resolution method - async send
 */
/*
  general name resolution - async send
 */
struct composite_context *resolve_name_file_send(TALLOC_CTX *mem_ctx, 
						  struct tevent_context *event_ctx,
						  void *userdata, uint32_t flags,
						  uint16_t port,
						  struct nbt_name *name)
{
	struct composite_context *c;
	struct resolve_file_data *data = talloc_get_type_abort(userdata, struct resolve_file_data);
	struct resolve_file_state *state;
	struct sockaddr_storage *resolved_iplist;
	int resolved_count, i;
	const char *dns_name;

	bool srv_lookup = (flags & RESOLVE_NAME_FLAG_DNS_SRV);

	if (event_ctx == NULL) {
		return NULL;
	}

	c = composite_create(mem_ctx, event_ctx);
	if (c == NULL) return NULL;

	/* This isn't an NBT layer resolver */
	if (flags & RESOLVE_NAME_FLAG_FORCE_NBT) {
		composite_error(c, NT_STATUS_OBJECT_NAME_NOT_FOUND);
		return c;
	}

	if (composite_nomem(c->event_ctx, c)) return c;

	state = talloc_zero(c, struct resolve_file_state);
	if (composite_nomem(state, c)) return c;
	c->private_data = state;

	dns_name = name->name;
	if (strchr(dns_name, '.') == NULL) {
		dns_name = talloc_asprintf(state, "%s.%s", dns_name, data->default_domain);
	}

	c->status = resolve_dns_hosts_file_as_sockaddr(data->lookup_file, dns_name,
						       srv_lookup, state, &resolved_iplist, &resolved_count);
	if (!composite_is_ok(c)) return c;

	for (i=0; i < resolved_count; i++) {
		state->addrs = talloc_realloc(state, state->addrs, struct socket_address *, i+2);
		if (composite_nomem(state->addrs, c)) return c;
		
		if (!(flags & RESOLVE_NAME_FLAG_OVERWRITE_PORT)) {
			set_sockaddr_port((struct sockaddr *)&resolved_iplist[i], port);
		}

		state->addrs[i] = socket_address_from_sockaddr(state->addrs, (struct sockaddr *)&resolved_iplist[i], sizeof(resolved_iplist[i]));
		if (composite_nomem(state->addrs[i], c)) return c;

		state->addrs[i+1] = NULL;


		state->names = talloc_realloc(state, state->names, char *, i+2);
		if (composite_nomem(state->addrs, c)) return c;

		state->names[i] = talloc_strdup(state->names, dns_name);
		if (composite_nomem(state->names[i], c)) return c;

		state->names[i+1] = NULL;
		
		i++;
	}

	composite_done(c);
	return c;
}

/*
  general name resolution method - recv side
 */
NTSTATUS resolve_name_file_recv(struct composite_context *c, 
				 TALLOC_CTX *mem_ctx,
				 struct socket_address ***addrs,
				 char ***names)
{
	NTSTATUS status;

	status = composite_wait(c);

	if (NT_STATUS_IS_OK(status)) {
		struct resolve_file_state *state = talloc_get_type(c->private_data, struct resolve_file_state);
		*addrs = talloc_steal(mem_ctx, state->addrs);
		if (names) {
			*names = talloc_steal(mem_ctx, state->names);
		}
	}

	talloc_free(c);
	return status;
}


bool resolve_context_add_file_method(struct resolve_context *ctx, const char *lookup_file, const char *default_domain)
{
	struct resolve_file_data *data = talloc(ctx, struct resolve_file_data);
	data->lookup_file    = talloc_strdup(data, lookup_file);
	data->default_domain = talloc_strdup(data, default_domain);
	return resolve_context_add_method(ctx, resolve_name_file_send, resolve_name_file_recv, data);
}

bool resolve_context_add_file_method_lp(struct resolve_context *ctx, struct loadparm_context *lp_ctx)
{
	return resolve_context_add_file_method(ctx,
					       lpcfg_parm_string(lp_ctx, NULL, "resolv", "host file"),
					       lpcfg_dnsdomain(lp_ctx));
}
