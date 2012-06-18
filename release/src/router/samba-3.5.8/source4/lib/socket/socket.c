/* 
   Unix SMB/CIFS implementation.
   Socket functions
   Copyright (C) Andrew Tridgell 1992-1998
   Copyright (C) Tim Potter      2000-2001
   Copyright (C) Stefan Metzmacher 2004
   
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
#include "lib/socket/socket.h"
#include "system/filesys.h"
#include "system/network.h"
#include "param/param.h"

/*
  auto-close sockets on free
*/
static int socket_destructor(struct socket_context *sock)
{
	if (sock->ops->fn_close && 
	    !(sock->flags & SOCKET_FLAG_NOCLOSE)) {
		sock->ops->fn_close(sock);
	}
	return 0;
}

_PUBLIC_ void socket_tevent_fd_close_fn(struct tevent_context *ev,
					struct tevent_fd *fde,
					int fd,
					void *private_data)
{
	/* this might be the socket_wrapper swrap_close() */
	close(fd);
}

_PUBLIC_ NTSTATUS socket_create_with_ops(TALLOC_CTX *mem_ctx, const struct socket_ops *ops,
					 struct socket_context **new_sock, 
					 enum socket_type type, uint32_t flags)
{
	NTSTATUS status;

	(*new_sock) = talloc(mem_ctx, struct socket_context);
	if (!(*new_sock)) {
		return NT_STATUS_NO_MEMORY;
	}

	(*new_sock)->type = type;
	(*new_sock)->state = SOCKET_STATE_UNDEFINED;
	(*new_sock)->flags = flags;

	(*new_sock)->fd = -1;

	(*new_sock)->private_data = NULL;
	(*new_sock)->ops = ops;
	(*new_sock)->backend_name = NULL;

	status = (*new_sock)->ops->fn_init((*new_sock));
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(*new_sock);
		return status;
	}

	/* by enabling "testnonblock" mode, all socket receive and
	   send calls on non-blocking sockets will randomly recv/send
	   less data than requested */

	if (!(flags & SOCKET_FLAG_BLOCK) &&
	    type == SOCKET_TYPE_STREAM &&
		getenv("SOCKET_TESTNONBLOCK") != NULL) {
		(*new_sock)->flags |= SOCKET_FLAG_TESTNONBLOCK;
	}

	/* we don't do a connect() on dgram sockets, so need to set
	   non-blocking at socket create time */
	if (!(flags & SOCKET_FLAG_BLOCK) && type == SOCKET_TYPE_DGRAM) {
		set_blocking(socket_get_fd(*new_sock), false);
	}

	talloc_set_destructor(*new_sock, socket_destructor);

	return NT_STATUS_OK;
}

_PUBLIC_ NTSTATUS socket_create(const char *name, enum socket_type type, 
			        struct socket_context **new_sock, uint32_t flags)
{
	const struct socket_ops *ops;

	ops = socket_getops_byname(name, type);
	if (!ops) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	return socket_create_with_ops(NULL, ops, new_sock, type, flags);
}

_PUBLIC_ NTSTATUS socket_connect(struct socket_context *sock,
				 const struct socket_address *my_address, 
				 const struct socket_address *server_address,
				 uint32_t flags)
{
	if (sock == NULL) {
		return NT_STATUS_CONNECTION_DISCONNECTED;
	}
	if (sock->state != SOCKET_STATE_UNDEFINED) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (!sock->ops->fn_connect) {
		return NT_STATUS_NOT_IMPLEMENTED;
	}

	return sock->ops->fn_connect(sock, my_address, server_address, flags);
}

_PUBLIC_ NTSTATUS socket_connect_complete(struct socket_context *sock, uint32_t flags)
{
	if (!sock->ops->fn_connect_complete) {
		return NT_STATUS_NOT_IMPLEMENTED;
	}
	return sock->ops->fn_connect_complete(sock, flags);
}

_PUBLIC_ NTSTATUS socket_listen(struct socket_context *sock, 
			        const struct socket_address *my_address, 
			        int queue_size, uint32_t flags)
{
	if (sock == NULL) {
		return NT_STATUS_CONNECTION_DISCONNECTED;
	}
	if (sock->state != SOCKET_STATE_UNDEFINED) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (!sock->ops->fn_listen) {
		return NT_STATUS_NOT_IMPLEMENTED;
	}

	return sock->ops->fn_listen(sock, my_address, queue_size, flags);
}

_PUBLIC_ NTSTATUS socket_accept(struct socket_context *sock, struct socket_context **new_sock)
{
	NTSTATUS status;

	if (sock == NULL) {
		return NT_STATUS_CONNECTION_DISCONNECTED;
	}
	if (sock->type != SOCKET_TYPE_STREAM) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (sock->state != SOCKET_STATE_SERVER_LISTEN) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (!sock->ops->fn_accept) {
		return NT_STATUS_NOT_IMPLEMENTED;
	}

	status = sock->ops->fn_accept(sock, new_sock);

	if (NT_STATUS_IS_OK(status)) {
		talloc_set_destructor(*new_sock, socket_destructor);
		(*new_sock)->flags = 0;
	}

	return status;
}

_PUBLIC_ NTSTATUS socket_recv(struct socket_context *sock, void *buf, 
			      size_t wantlen, size_t *nread)
{
	if (sock == NULL) {
		return NT_STATUS_CONNECTION_DISCONNECTED;
	}
	if (sock->state != SOCKET_STATE_CLIENT_CONNECTED &&
	    sock->state != SOCKET_STATE_SERVER_CONNECTED &&
	    sock->type  != SOCKET_TYPE_DGRAM) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (!sock->ops->fn_recv) {
		return NT_STATUS_NOT_IMPLEMENTED;
	}

	if ((sock->flags & SOCKET_FLAG_TESTNONBLOCK) 
	    && wantlen > 1) {

		if (random() % 10 == 0) {
			*nread = 0;
			return STATUS_MORE_ENTRIES;
		}
		return sock->ops->fn_recv(sock, buf, 1+(random() % wantlen), nread);
	}
	return sock->ops->fn_recv(sock, buf, wantlen, nread);
}

_PUBLIC_ NTSTATUS socket_recvfrom(struct socket_context *sock, void *buf, 
				  size_t wantlen, size_t *nread, 
				  TALLOC_CTX *mem_ctx, struct socket_address **src_addr)
{
	if (sock == NULL) {
		return NT_STATUS_CONNECTION_DISCONNECTED;
	}
	if (sock->type != SOCKET_TYPE_DGRAM) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (!sock->ops->fn_recvfrom) {
		return NT_STATUS_NOT_IMPLEMENTED;
	}

	return sock->ops->fn_recvfrom(sock, buf, wantlen, nread, 
				      mem_ctx, src_addr);
}

_PUBLIC_ NTSTATUS socket_send(struct socket_context *sock, 
			      const DATA_BLOB *blob, size_t *sendlen)
{
	if (sock == NULL) {
		return NT_STATUS_CONNECTION_DISCONNECTED;
	}
	if (sock->state != SOCKET_STATE_CLIENT_CONNECTED &&
	    sock->state != SOCKET_STATE_SERVER_CONNECTED) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (!sock->ops->fn_send) {
		return NT_STATUS_NOT_IMPLEMENTED;
	}
	
	if ((sock->flags & SOCKET_FLAG_TESTNONBLOCK)
	    && blob->length > 1) {
		DATA_BLOB blob2 = *blob;
		if (random() % 10 == 0) {
			*sendlen = 0;
			return STATUS_MORE_ENTRIES;
		}
		/* The random size sends are incompatible with TLS and SASL
		 * sockets, which require re-sends to be consistant */
		if (!(sock->flags & SOCKET_FLAG_ENCRYPT)) {
			blob2.length = 1+(random() % blob2.length);
		} else {
			/* This is particularly stressful on buggy
			 * LDAP clients, that don't expect on LDAP
			 * packet in many SASL packets */
			blob2.length = 1 + blob2.length/2;
		}
		return sock->ops->fn_send(sock, &blob2, sendlen);
	}
	return sock->ops->fn_send(sock, blob, sendlen);
}


_PUBLIC_ NTSTATUS socket_sendto(struct socket_context *sock, 
			        const DATA_BLOB *blob, size_t *sendlen, 
			        const struct socket_address *dest_addr)
{
	if (sock == NULL) {
		return NT_STATUS_CONNECTION_DISCONNECTED;
	}
	if (sock->type != SOCKET_TYPE_DGRAM) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (sock->state == SOCKET_STATE_CLIENT_CONNECTED ||
	    sock->state == SOCKET_STATE_SERVER_CONNECTED) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (!sock->ops->fn_sendto) {
		return NT_STATUS_NOT_IMPLEMENTED;
	}

	return sock->ops->fn_sendto(sock, blob, sendlen, dest_addr);
}


/*
  ask for the number of bytes in a pending incoming packet
*/
_PUBLIC_ NTSTATUS socket_pending(struct socket_context *sock, size_t *npending)
{
	if (sock == NULL) {
		return NT_STATUS_CONNECTION_DISCONNECTED;
	}
	if (!sock->ops->fn_pending) {
		return NT_STATUS_NOT_IMPLEMENTED;
	}
	return sock->ops->fn_pending(sock, npending);
}


_PUBLIC_ NTSTATUS socket_set_option(struct socket_context *sock, const char *option, const char *val)
{
	if (sock == NULL) {
		return NT_STATUS_CONNECTION_DISCONNECTED;
	}
	if (!sock->ops->fn_set_option) {
		return NT_STATUS_NOT_IMPLEMENTED;
	}

	return sock->ops->fn_set_option(sock, option, val);
}

_PUBLIC_ char *socket_get_peer_name(struct socket_context *sock, TALLOC_CTX *mem_ctx)
{
	if (!sock->ops->fn_get_peer_name) {
		return NULL;
	}

	return sock->ops->fn_get_peer_name(sock, mem_ctx);
}

_PUBLIC_ struct socket_address *socket_get_peer_addr(struct socket_context *sock, TALLOC_CTX *mem_ctx)
{
	if (!sock->ops->fn_get_peer_addr) {
		return NULL;
	}

	return sock->ops->fn_get_peer_addr(sock, mem_ctx);
}

_PUBLIC_ struct socket_address *socket_get_my_addr(struct socket_context *sock, TALLOC_CTX *mem_ctx)
{
	if (!sock->ops->fn_get_my_addr) {
		return NULL;
	}

	return sock->ops->fn_get_my_addr(sock, mem_ctx);
}

_PUBLIC_ int socket_get_fd(struct socket_context *sock)
{
	if (!sock->ops->fn_get_fd) {
		return -1;
	}

	return sock->ops->fn_get_fd(sock);
}

/*
  call dup() on a socket, and close the old fd. This is used to change
  the fd to the lowest available number, to make select() more
  efficient (select speed depends on the maxiumum fd number passed to
  it)
*/
_PUBLIC_ NTSTATUS socket_dup(struct socket_context *sock)
{
	int fd;
	if (sock->fd == -1) {
		return NT_STATUS_INVALID_HANDLE;
	}
	fd = dup(sock->fd);
	if (fd == -1) {
		return map_nt_error_from_unix(errno);
	}
	close(sock->fd);
	sock->fd = fd;
	return NT_STATUS_OK;
	
}

/* Create a new socket_address.  The type must match the socket type.
 * The host parameter may be an IP or a hostname 
 */

_PUBLIC_ struct socket_address *socket_address_from_strings(TALLOC_CTX *mem_ctx,
							    const char *family,
							    const char *host,
							    int port)
{
	struct socket_address *addr = talloc(mem_ctx, struct socket_address);
	if (!addr) {
		return NULL;
	}

	addr->family = family;
	addr->addr = talloc_strdup(addr, host);
	if (!addr->addr) {
		talloc_free(addr);
		return NULL;
	}
	addr->port = port;
	addr->sockaddr = NULL;
	addr->sockaddrlen = 0;

	return addr;
}

/* Create a new socket_address.  Copy the struct sockaddr into the new
 * structure.  Used for hooks in the kerberos libraries, where they
 * supply only a struct sockaddr */

_PUBLIC_ struct socket_address *socket_address_from_sockaddr(TALLOC_CTX *mem_ctx, 
							     struct sockaddr *sockaddr, 
							     size_t sockaddrlen)
{
	struct socket_address *addr = talloc(mem_ctx, struct socket_address);
	if (!addr) {
		return NULL;
	}
	addr->family = NULL; 
	addr->addr = NULL;
	addr->port = 0;
	addr->sockaddr = (struct sockaddr *)talloc_memdup(addr, sockaddr, sockaddrlen);
	if (!addr->sockaddr) {
		talloc_free(addr);
		return NULL;
	}
	addr->sockaddrlen = sockaddrlen;
	return addr;
}

/* Copy a socket_address structure */
struct socket_address *socket_address_copy(TALLOC_CTX *mem_ctx,
					   const struct socket_address *oaddr)
{
	struct socket_address *addr = talloc_zero(mem_ctx, struct socket_address);
	if (!addr) {
		return NULL;
	}
	addr->family	= oaddr->family;
	if (oaddr->addr) {
		addr->addr	= talloc_strdup(addr, oaddr->addr);
		if (!addr->addr) {
			goto nomem;
		}
	}
	addr->port	= oaddr->port;
	if (oaddr->sockaddr) {
		addr->sockaddr = (struct sockaddr *)talloc_memdup(addr,
								  oaddr->sockaddr,
								  oaddr->sockaddrlen);
		if (!addr->sockaddr) {
			goto nomem;
		}
		addr->sockaddrlen = oaddr->sockaddrlen;
	}

	return addr;

nomem:
	talloc_free(addr);
	return NULL;
}

_PUBLIC_ const struct socket_ops *socket_getops_byname(const char *family, enum socket_type type)
{
	extern const struct socket_ops *socket_ipv4_ops(enum socket_type);
	extern const struct socket_ops *socket_ipv6_ops(enum socket_type);
	extern const struct socket_ops *socket_unixdom_ops(enum socket_type);

	if (strcmp("ip", family) == 0 || 
	    strcmp("ipv4", family) == 0) {
		return socket_ipv4_ops(type);
	}

#if HAVE_IPV6
	if (strcmp("ipv6", family) == 0) {
		return socket_ipv6_ops(type);
	}
#endif

	if (strcmp("unix", family) == 0) {
		return socket_unixdom_ops(type);
	}

	return NULL;
}

enum SOCK_OPT_TYPES {OPT_BOOL,OPT_INT,OPT_ON};

static const struct {
	const char *name;
	int level;
	int option;
	int value;
	int opttype;
} socket_options[] = {
  {"SO_KEEPALIVE",      SOL_SOCKET,    SO_KEEPALIVE,    0,                 OPT_BOOL},
  {"SO_REUSEADDR",      SOL_SOCKET,    SO_REUSEADDR,    0,                 OPT_BOOL},
  {"SO_BROADCAST",      SOL_SOCKET,    SO_BROADCAST,    0,                 OPT_BOOL},
#ifdef TCP_NODELAY
  {"TCP_NODELAY",       IPPROTO_TCP,   TCP_NODELAY,     0,                 OPT_BOOL},
#endif
#ifdef IPTOS_LOWDELAY
  {"IPTOS_LOWDELAY",    IPPROTO_IP,    IP_TOS,          IPTOS_LOWDELAY,    OPT_ON},
#endif
#ifdef IPTOS_THROUGHPUT
  {"IPTOS_THROUGHPUT",  IPPROTO_IP,    IP_TOS,          IPTOS_THROUGHPUT,  OPT_ON},
#endif
#ifdef SO_REUSEPORT
  {"SO_REUSEPORT",      SOL_SOCKET,    SO_REUSEPORT,    0,                 OPT_BOOL},
#endif
#ifdef SO_SNDBUF
  {"SO_SNDBUF",         SOL_SOCKET,    SO_SNDBUF,       0,                 OPT_INT},
#endif
#ifdef SO_RCVBUF
  {"SO_RCVBUF",         SOL_SOCKET,    SO_RCVBUF,       0,                 OPT_INT},
#endif
#ifdef SO_SNDLOWAT
  {"SO_SNDLOWAT",       SOL_SOCKET,    SO_SNDLOWAT,     0,                 OPT_INT},
#endif
#ifdef SO_RCVLOWAT
  {"SO_RCVLOWAT",       SOL_SOCKET,    SO_RCVLOWAT,     0,                 OPT_INT},
#endif
#ifdef SO_SNDTIMEO
  {"SO_SNDTIMEO",       SOL_SOCKET,    SO_SNDTIMEO,     0,                 OPT_INT},
#endif
#ifdef SO_RCVTIMEO
  {"SO_RCVTIMEO",       SOL_SOCKET,    SO_RCVTIMEO,     0,                 OPT_INT},
#endif
  {NULL,0,0,0,0}};


/**
 Set user socket options.
**/
_PUBLIC_ void set_socket_options(int fd, const char *options)
{
	const char **options_list = (const char **)str_list_make(NULL, options, " \t,");
	int j;

	if (!options_list)
		return;

	for (j = 0; options_list[j]; j++) {
		const char *tok = options_list[j];
		int ret=0,i;
		int value = 1;
		char *p;
		bool got_value = false;

		if ((p = strchr(tok,'='))) {
			*p = 0;
			value = atoi(p+1);
			got_value = true;
		}

		for (i=0;socket_options[i].name;i++)
			if (strequal(socket_options[i].name,tok))
				break;

		if (!socket_options[i].name) {
			DEBUG(0,("Unknown socket option %s\n",tok));
			continue;
		}

		switch (socket_options[i].opttype) {
		case OPT_BOOL:
		case OPT_INT:
			ret = setsockopt(fd,socket_options[i].level,
						socket_options[i].option,(char *)&value,sizeof(int));
			break;

		case OPT_ON:
			if (got_value)
				DEBUG(0,("syntax error - %s does not take a value\n",tok));

			{
				int on = socket_options[i].value;
				ret = setsockopt(fd,socket_options[i].level,
							socket_options[i].option,(char *)&on,sizeof(int));
			}
			break;	  
		}
      
		if (ret != 0)
			DEBUG(0,("Failed to set socket option %s (Error %s)\n",tok, strerror(errno) ));
	}

	talloc_free(options_list);
}

/*
  set some flags on a socket 
 */
void socket_set_flags(struct socket_context *sock, unsigned flags)
{
	sock->flags |= flags;
}
