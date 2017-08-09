/* 
   Unix SMB/CIFS implementation.

   SMB client socket context management functions

   Copyright (C) Andrew Tridgell 1994-2005
   Copyright (C) James Myers 2003 <myersjj@samba.org>
   
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
#include "libcli/raw/libcliraw.h"
#include "libcli/composite/composite.h"
#include "lib/socket/socket.h"
#include "libcli/resolve/resolve.h"
#include "param/param.h"
#include "libcli/raw/raw_proto.h"

struct sock_connect_state {
	struct composite_context *ctx;
	const char *host_name;
	int num_ports;
	uint16_t *ports;
	const char *socket_options;
	struct smbcli_socket *result;
};

/*
  connect a smbcli_socket context to an IP/port pair
  if port is 0 then choose 445 then 139
*/

static void smbcli_sock_connect_recv_conn(struct composite_context *ctx);

struct composite_context *smbcli_sock_connect_send(TALLOC_CTX *mem_ctx,
						   const char *host_addr,
						   const char **ports,
						   const char *host_name,
						   struct resolve_context *resolve_ctx,
						   struct tevent_context *event_ctx,
						   const char *socket_options)
{
	struct composite_context *result, *ctx;
	struct sock_connect_state *state;
	int i;

	result = talloc_zero(mem_ctx, struct composite_context);
	if (result == NULL) goto failed;
	result->state = COMPOSITE_STATE_IN_PROGRESS;

	result->event_ctx = event_ctx;
	if (result->event_ctx == NULL) goto failed;

	state = talloc(result, struct sock_connect_state);
	if (state == NULL) goto failed;
	state->ctx = result;
	result->private_data = state;

	state->host_name = talloc_strdup(state, host_name);
	if (state->host_name == NULL) goto failed;

	state->num_ports = str_list_length(ports);
	state->ports = talloc_array(state, uint16_t, state->num_ports);
	if (state->ports == NULL) goto failed;
	for (i=0;ports[i];i++) {
		state->ports[i] = atoi(ports[i]);
	}
	state->socket_options = talloc_reference(state, socket_options);

	if (!host_addr) {
		host_addr = host_name;
	}

	ctx = socket_connect_multi_send(state, host_addr,
					state->num_ports, state->ports,
					resolve_ctx,
					state->ctx->event_ctx);
	if (ctx == NULL) goto failed;
	ctx->async.fn = smbcli_sock_connect_recv_conn;
	ctx->async.private_data = state;
	return result;

failed:
	talloc_free(result);
	return NULL;
}

static void smbcli_sock_connect_recv_conn(struct composite_context *ctx)
{
	struct sock_connect_state *state =
		talloc_get_type(ctx->async.private_data,
				struct sock_connect_state);
	struct socket_context *sock;
	uint16_t port;

	state->ctx->status = socket_connect_multi_recv(ctx, state, &sock,
						       &port);
	if (!composite_is_ok(state->ctx)) return;

	state->ctx->status =
		socket_set_option(sock, state->socket_options, NULL);
	if (!composite_is_ok(state->ctx)) return;


	state->result = talloc_zero(state, struct smbcli_socket);
	if (composite_nomem(state->result, state->ctx)) return;

	state->result->sock = talloc_steal(state->result, sock);
	state->result->port = port;
	state->result->hostname = talloc_steal(sock, state->host_name);

	state->result->event.ctx = state->ctx->event_ctx;
	if (composite_nomem(state->result->event.ctx, state->ctx)) return;

	composite_done(state->ctx);
}

/*
  finish a smbcli_sock_connect_send() operation
*/
NTSTATUS smbcli_sock_connect_recv(struct composite_context *c,
				  TALLOC_CTX *mem_ctx,
				  struct smbcli_socket **result)
{
	NTSTATUS status = composite_wait(c);
	if (NT_STATUS_IS_OK(status)) {
		struct sock_connect_state *state =
			talloc_get_type(c->private_data,
					struct sock_connect_state);
		*result = talloc_steal(mem_ctx, state->result);
	}
	talloc_free(c);
	return status;
}

/*
  connect a smbcli_socket context to an IP/port pair
  if port is 0 then choose the ports listed in smb.conf (normally 445 then 139)

  sync version of the function
*/
NTSTATUS smbcli_sock_connect(TALLOC_CTX *mem_ctx,
			     const char *host_addr, const char **ports,
			     const char *host_name,
			     struct resolve_context *resolve_ctx,
			     struct tevent_context *event_ctx,
				 const char *socket_options,
			     struct smbcli_socket **result)
{
	struct composite_context *c =
		smbcli_sock_connect_send(mem_ctx, host_addr, ports, host_name,
					 resolve_ctx,
					 event_ctx, socket_options);
	return smbcli_sock_connect_recv(c, mem_ctx, result);
}


/****************************************************************************
 mark the socket as dead
****************************************************************************/
_PUBLIC_ void smbcli_sock_dead(struct smbcli_socket *sock)
{
	talloc_free(sock->event.fde);
	sock->event.fde = NULL;
	talloc_free(sock->sock);
	sock->sock = NULL;
}

/****************************************************************************
 Set socket options on a open connection.
****************************************************************************/
void smbcli_sock_set_options(struct smbcli_socket *sock, const char *options)
{
	socket_set_option(sock->sock, options, NULL);
}

/****************************************************************************
resolve a hostname and connect 
****************************************************************************/
_PUBLIC_ struct smbcli_socket *smbcli_sock_connect_byname(const char *host, const char **ports,
						 TALLOC_CTX *mem_ctx,
						 struct resolve_context *resolve_ctx,
						 struct tevent_context *event_ctx,
						 const char *socket_options)
{
	int name_type = NBT_NAME_SERVER;
	const char *address;
	NTSTATUS status;
	struct nbt_name nbt_name;
	char *name, *p;
	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
	struct smbcli_socket *result;

	if (event_ctx == NULL) {
		DEBUG(0, ("Invalid NULL event context passed in as parameter\n"));
		return NULL;
	}

	if (tmp_ctx == NULL) {
		DEBUG(0, ("talloc_new failed\n"));
		return NULL;
	}

	name = talloc_strdup(tmp_ctx, host);
	if (name == NULL) {
		DEBUG(0, ("talloc_strdup failed\n"));
		talloc_free(tmp_ctx);
		return NULL;
	}

	/* allow hostnames of the form NAME#xx and do a netbios lookup */
	if ((p = strchr(name, '#'))) {
		name_type = strtol(p+1, NULL, 16);
		*p = 0;
	}

	make_nbt_name(&nbt_name, host, name_type);
	
	status = resolve_name(resolve_ctx, &nbt_name, tmp_ctx, &address, event_ctx);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(tmp_ctx);
		return NULL;
	}

	status = smbcli_sock_connect(mem_ctx, address, ports, name, resolve_ctx,
				     event_ctx, 
					 socket_options, &result);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(9, ("smbcli_sock_connect failed: %s\n",
			  nt_errstr(status)));
		talloc_free(tmp_ctx);
		return NULL;
	}

	talloc_free(tmp_ctx);

	return result;
}
