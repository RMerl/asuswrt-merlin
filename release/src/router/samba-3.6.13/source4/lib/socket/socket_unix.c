/* 
   Unix SMB/CIFS implementation.

   unix domain socket functions

   Copyright (C) Stefan Metzmacher 2004
   Copyright (C) Andrew Tridgell 2004-2005
   
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
#include "system/network.h"
#include "system/filesys.h"



/*
  approximate errno mapping
*/
static NTSTATUS unixdom_error(int ernum)
{
	return map_nt_error_from_unix(ernum);
}

static NTSTATUS unixdom_init(struct socket_context *sock)
{
	int type;

	switch (sock->type) {
	case SOCKET_TYPE_STREAM:
		type = SOCK_STREAM;
		break;
	case SOCKET_TYPE_DGRAM:
		type = SOCK_DGRAM;
		break;
	default:
		return NT_STATUS_INVALID_PARAMETER;
	}

	sock->fd = socket(PF_UNIX, type, 0);
	if (sock->fd == -1) {
		return map_nt_error_from_unix(errno);
	}
	sock->private_data = NULL;

	sock->backend_name = "unix";

	return NT_STATUS_OK;
}

static void unixdom_close(struct socket_context *sock)
{
	close(sock->fd);
}

static NTSTATUS unixdom_connect_complete(struct socket_context *sock, uint32_t flags)
{
	int error=0, ret;
	socklen_t len = sizeof(error);

	/* check for any errors that may have occurred - this is needed
	   for non-blocking connect */
	ret = getsockopt(sock->fd, SOL_SOCKET, SO_ERROR, &error, &len);
	if (ret == -1) {
		return map_nt_error_from_unix(errno);
	}
	if (error != 0) {
		return map_nt_error_from_unix(error);
	}

	if (!(flags & SOCKET_FLAG_BLOCK)) {
		ret = set_blocking(sock->fd, false);
		if (ret == -1) {
			return map_nt_error_from_unix(errno);
		}
	}

	sock->state = SOCKET_STATE_CLIENT_CONNECTED;

	return NT_STATUS_OK;
}

static NTSTATUS unixdom_connect(struct socket_context *sock,
				const struct socket_address *my_address, 
				const struct socket_address *srv_address, 
				uint32_t flags)
{
	int ret;

	if (srv_address->sockaddr) {
		ret = connect(sock->fd, srv_address->sockaddr, srv_address->sockaddrlen);
	} else {
		struct sockaddr_un srv_addr;
		if (strlen(srv_address->addr)+1 > sizeof(srv_addr.sun_path)) {
			return NT_STATUS_OBJECT_PATH_INVALID;
		}
		
		ZERO_STRUCT(srv_addr);
		srv_addr.sun_family = AF_UNIX;
		strncpy(srv_addr.sun_path, srv_address->addr, sizeof(srv_addr.sun_path));

		ret = connect(sock->fd, (const struct sockaddr *)&srv_addr, sizeof(srv_addr));
	}
	if (ret == -1) {
		return unixdom_error(errno);
	}

	return unixdom_connect_complete(sock, flags);
}

static NTSTATUS unixdom_listen(struct socket_context *sock,
			       const struct socket_address *my_address, 
			       int queue_size, uint32_t flags)
{
	struct sockaddr_un my_addr;
	int ret;

	/* delete if it already exists */
	if (my_address->addr) {
		unlink(my_address->addr);
	}

	if (my_address->sockaddr) {
		ret = bind(sock->fd, (struct sockaddr *)&my_addr, sizeof(my_addr));
	} else if (my_address->addr == NULL) {
		return NT_STATUS_INVALID_PARAMETER;
	} else {
		if (strlen(my_address->addr)+1 > sizeof(my_addr.sun_path)) {
			return NT_STATUS_OBJECT_PATH_INVALID;
		}
		
		
		ZERO_STRUCT(my_addr);
		my_addr.sun_family = AF_UNIX;
		strncpy(my_addr.sun_path, my_address->addr, sizeof(my_addr.sun_path));
		
		ret = bind(sock->fd, (struct sockaddr *)&my_addr, sizeof(my_addr));
	}
	if (ret == -1) {
		return unixdom_error(errno);
	}

	if (sock->type == SOCKET_TYPE_STREAM) {
		ret = listen(sock->fd, queue_size);
		if (ret == -1) {
			return unixdom_error(errno);
		}
	}

	if (!(flags & SOCKET_FLAG_BLOCK)) {
		ret = set_blocking(sock->fd, false);
		if (ret == -1) {
			return unixdom_error(errno);
		}
	}

	sock->state = SOCKET_STATE_SERVER_LISTEN;
	sock->private_data = (void *)talloc_strdup(sock, my_address->addr);

	return NT_STATUS_OK;
}

static NTSTATUS unixdom_accept(struct socket_context *sock, 
			       struct socket_context **new_sock)
{
	struct sockaddr_un cli_addr;
	socklen_t cli_addr_len = sizeof(cli_addr);
	int new_fd;

	if (sock->type != SOCKET_TYPE_STREAM) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	new_fd = accept(sock->fd, (struct sockaddr *)&cli_addr, &cli_addr_len);
	if (new_fd == -1) {
		return unixdom_error(errno);
	}

	if (!(sock->flags & SOCKET_FLAG_BLOCK)) {
		int ret = set_blocking(new_fd, false);
		if (ret == -1) {
			close(new_fd);
			return map_nt_error_from_unix(errno);
		}
	}

	(*new_sock) = talloc(NULL, struct socket_context);
	if (!(*new_sock)) {
		close(new_fd);
		return NT_STATUS_NO_MEMORY;
	}

	/* copy the socket_context */
	(*new_sock)->type		= sock->type;
	(*new_sock)->state		= SOCKET_STATE_SERVER_CONNECTED;
	(*new_sock)->flags		= sock->flags;

	(*new_sock)->fd			= new_fd;

	(*new_sock)->private_data	= NULL;
	(*new_sock)->ops		= sock->ops;
	(*new_sock)->backend_name	= sock->backend_name;

	return NT_STATUS_OK;
}

static NTSTATUS unixdom_recv(struct socket_context *sock, void *buf, 
			     size_t wantlen, size_t *nread)
{
	ssize_t gotlen;

	*nread = 0;

	gotlen = recv(sock->fd, buf, wantlen, 0);
	if (gotlen == 0) {
		return NT_STATUS_END_OF_FILE;
	} else if (gotlen == -1) {
		return unixdom_error(errno);
	}

	*nread = gotlen;

	return NT_STATUS_OK;
}

static NTSTATUS unixdom_send(struct socket_context *sock,
			     const DATA_BLOB *blob, size_t *sendlen)
{
	ssize_t len;

	*sendlen = 0;

	len = send(sock->fd, blob->data, blob->length, 0);
	if (len == -1) {
		return unixdom_error(errno);
	}	

	*sendlen = len;

	return NT_STATUS_OK;
}


static NTSTATUS unixdom_sendto(struct socket_context *sock, 
			       const DATA_BLOB *blob, size_t *sendlen, 
			       const struct socket_address *dest)
{
	ssize_t len;
	*sendlen = 0;
		
	if (dest->sockaddr) {
		len = sendto(sock->fd, blob->data, blob->length, 0, 
			     dest->sockaddr, dest->sockaddrlen);
	} else {
		struct sockaddr_un srv_addr;
		
		if (strlen(dest->addr)+1 > sizeof(srv_addr.sun_path)) {
			return NT_STATUS_OBJECT_PATH_INVALID;
		}
		
		ZERO_STRUCT(srv_addr);
		srv_addr.sun_family = AF_UNIX;
		strncpy(srv_addr.sun_path, dest->addr, sizeof(srv_addr.sun_path));
		
		len = sendto(sock->fd, blob->data, blob->length, 0, 
			     (struct sockaddr *)&srv_addr, sizeof(srv_addr));
	}
	if (len == -1) {
		return map_nt_error_from_unix(errno);
	}	

	*sendlen = len;

	return NT_STATUS_OK;
}


static NTSTATUS unixdom_set_option(struct socket_context *sock, 
				   const char *option, const char *val)
{
	return NT_STATUS_OK;
}

static char *unixdom_get_peer_name(struct socket_context *sock, TALLOC_CTX *mem_ctx)
{
	return talloc_strdup(mem_ctx, "LOCAL/unixdom");
}

static struct socket_address *unixdom_get_peer_addr(struct socket_context *sock, TALLOC_CTX *mem_ctx)
{
	struct sockaddr_in *peer_addr;
	socklen_t len = sizeof(*peer_addr);
	struct socket_address *peer;
	int ret;

	peer = talloc(mem_ctx, struct socket_address);
	if (!peer) {
		return NULL;
	}
	
	peer->family = sock->backend_name;
	peer_addr = talloc(peer, struct sockaddr_in);
	if (!peer_addr) {
		talloc_free(peer);
		return NULL;
	}

	peer->sockaddr = (struct sockaddr *)peer_addr;

	ret = getpeername(sock->fd, peer->sockaddr, &len);
	if (ret == -1) {
		talloc_free(peer);
		return NULL;
	}

	peer->sockaddrlen = len;

	peer->port = 0;
	peer->addr = talloc_strdup(peer, "LOCAL/unixdom");
	if (!peer->addr) {
		talloc_free(peer);
		return NULL;
	}

	return peer;
}

static struct socket_address *unixdom_get_my_addr(struct socket_context *sock, TALLOC_CTX *mem_ctx)
{
	struct sockaddr_in *local_addr;
	socklen_t len = sizeof(*local_addr);
	struct socket_address *local;
	int ret;
	
	local = talloc(mem_ctx, struct socket_address);
	if (!local) {
		return NULL;
	}
	
	local->family = sock->backend_name;
	local_addr = talloc(local, struct sockaddr_in);
	if (!local_addr) {
		talloc_free(local);
		return NULL;
	}

	local->sockaddr = (struct sockaddr *)local_addr;

	ret = getsockname(sock->fd, local->sockaddr, &len);
	if (ret == -1) {
		talloc_free(local);
		return NULL;
	}

	local->sockaddrlen = len;

	local->port = 0;
	local->addr = talloc_strdup(local, "LOCAL/unixdom");
	if (!local->addr) {
		talloc_free(local);
		return NULL;
	}

	return local;
}

static int unixdom_get_fd(struct socket_context *sock)
{
	return sock->fd;
}

static NTSTATUS unixdom_pending(struct socket_context *sock, size_t *npending)
{
	int value = 0;
	if (ioctl(sock->fd, FIONREAD, &value) == 0) {
		*npending = value;
		return NT_STATUS_OK;
	}
	return map_nt_error_from_unix(errno);
}

static const struct socket_ops unixdom_ops = {
	.name			= "unix",
	.fn_init		= unixdom_init,
	.fn_connect		= unixdom_connect,
	.fn_connect_complete	= unixdom_connect_complete,
	.fn_listen		= unixdom_listen,
	.fn_accept		= unixdom_accept,
	.fn_recv		= unixdom_recv,
	.fn_send		= unixdom_send,
	.fn_sendto		= unixdom_sendto,
	.fn_close		= unixdom_close,
	.fn_pending		= unixdom_pending,

	.fn_set_option		= unixdom_set_option,

	.fn_get_peer_name	= unixdom_get_peer_name,
	.fn_get_peer_addr	= unixdom_get_peer_addr,
	.fn_get_my_addr		= unixdom_get_my_addr,

	.fn_get_fd		= unixdom_get_fd
};

_PUBLIC_ const struct socket_ops *socket_unixdom_ops(enum socket_type type)
{
	return &unixdom_ops;
}
