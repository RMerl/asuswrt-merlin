/* 
   Unix SMB/CIFS implementation.
   Socket functions
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

#ifndef _SAMBA_SOCKET_H
#define _SAMBA_SOCKET_H

struct tevent_context;
struct tevent_fd;
struct socket_context;

enum socket_type {
	SOCKET_TYPE_STREAM,
	SOCKET_TYPE_DGRAM
};

struct socket_address {
	const char *family;
	char *addr;
	int port;
	struct sockaddr *sockaddr;
	size_t sockaddrlen;
};

struct socket_ops {
	const char *name;

	NTSTATUS (*fn_init)(struct socket_context *sock);

	/* client ops */
	NTSTATUS (*fn_connect)(struct socket_context *sock,
			       const struct socket_address *my_address,
			       const struct socket_address *server_address,
			       uint32_t flags);

	/* complete a non-blocking connect */
	NTSTATUS (*fn_connect_complete)(struct socket_context *sock,
					uint32_t flags);

	/* server ops */
	NTSTATUS (*fn_listen)(struct socket_context *sock,
			      const struct socket_address *my_address, 
			      int queue_size, uint32_t flags);
	NTSTATUS (*fn_accept)(struct socket_context *sock,	
			      struct socket_context **new_sock);

	/* general ops */
	NTSTATUS (*fn_recv)(struct socket_context *sock, void *buf,
			    size_t wantlen, size_t *nread);
	NTSTATUS (*fn_send)(struct socket_context *sock, 
			    const DATA_BLOB *blob, size_t *sendlen);

	NTSTATUS (*fn_sendto)(struct socket_context *sock, 
			      const DATA_BLOB *blob, size_t *sendlen,
			      const struct socket_address *dest_addr);
	NTSTATUS (*fn_recvfrom)(struct socket_context *sock, 
				void *buf, size_t wantlen, size_t *nread,
				TALLOC_CTX *addr_ctx, struct socket_address **src_addr);
	NTSTATUS (*fn_pending)(struct socket_context *sock, size_t *npending);      

	void (*fn_close)(struct socket_context *sock);

	NTSTATUS (*fn_set_option)(struct socket_context *sock, const char *option, const char *val);

	char *(*fn_get_peer_name)(struct socket_context *sock, TALLOC_CTX *mem_ctx);
	struct socket_address *(*fn_get_peer_addr)(struct socket_context *sock, TALLOC_CTX *mem_ctx);
	struct socket_address *(*fn_get_my_addr)(struct socket_context *sock, TALLOC_CTX *mem_ctx);

	int (*fn_get_fd)(struct socket_context *sock);
};

enum socket_state {
	SOCKET_STATE_UNDEFINED,

	SOCKET_STATE_CLIENT_START,
	SOCKET_STATE_CLIENT_CONNECTED,
	SOCKET_STATE_CLIENT_STARTTLS,
	SOCKET_STATE_CLIENT_ERROR,
	
	SOCKET_STATE_SERVER_LISTEN,
	SOCKET_STATE_SERVER_CONNECTED,
	SOCKET_STATE_SERVER_STARTTLS,
	SOCKET_STATE_SERVER_ERROR
};

#define SOCKET_FLAG_BLOCK        0x00000001
#define SOCKET_FLAG_PEEK         0x00000002
#define SOCKET_FLAG_TESTNONBLOCK 0x00000004
#define SOCKET_FLAG_ENCRYPT      0x00000008 /* This socket
					     * implementation requires
					     * that re-sends be
					     * consistant, because it
					     * is encrypting data.
					     * This modifies the
					     * TESTNONBLOCK case */
#define SOCKET_FLAG_NOCLOSE      0x00000010 /* don't auto-close on free */


struct socket_context {
	enum socket_type type;
	enum socket_state state;
	uint32_t flags;

	int fd;

	void *private_data;
	const struct socket_ops *ops;
	const char *backend_name;

	/* specific to the ip backend */
	int family;
};

struct resolve_context;

/* prototypes */
NTSTATUS socket_create_with_ops(TALLOC_CTX *mem_ctx, const struct socket_ops *ops,
				struct socket_context **new_sock, 
				enum socket_type type, uint32_t flags);
NTSTATUS socket_create(const char *name, enum socket_type type, 
		       struct socket_context **new_sock, uint32_t flags);
NTSTATUS socket_connect(struct socket_context *sock,
			const struct socket_address *my_address, 
			const struct socket_address *server_address,
			uint32_t flags);
NTSTATUS socket_connect_complete(struct socket_context *sock, uint32_t flags);
NTSTATUS socket_listen(struct socket_context *sock, 
		       const struct socket_address *my_address, 
		       int queue_size, uint32_t flags);
NTSTATUS socket_accept(struct socket_context *sock, struct socket_context **new_sock);
NTSTATUS socket_recv(struct socket_context *sock, void *buf, 
		     size_t wantlen, size_t *nread);
NTSTATUS socket_recvfrom(struct socket_context *sock, void *buf, 
			 size_t wantlen, size_t *nread, 
			 TALLOC_CTX *addr_ctx, struct socket_address **src_addr);
NTSTATUS socket_send(struct socket_context *sock, 
		     const DATA_BLOB *blob, size_t *sendlen);
NTSTATUS socket_sendto(struct socket_context *sock, 
		       const DATA_BLOB *blob, size_t *sendlen,
		       const struct socket_address *dest_addr);
NTSTATUS socket_pending(struct socket_context *sock, size_t *npending);
NTSTATUS socket_set_option(struct socket_context *sock, const char *option, const char *val);
char *socket_get_peer_name(struct socket_context *sock, TALLOC_CTX *mem_ctx);
struct socket_address *socket_get_peer_addr(struct socket_context *sock, TALLOC_CTX *mem_ctx);
struct socket_address *socket_get_my_addr(struct socket_context *sock, TALLOC_CTX *mem_ctx);
int socket_get_fd(struct socket_context *sock);
NTSTATUS socket_dup(struct socket_context *sock);
struct socket_address *socket_address_from_strings(TALLOC_CTX *mem_ctx,
						   const char *type, 
						   const char *host,
						   int port);
struct socket_address *socket_address_from_sockaddr(TALLOC_CTX *mem_ctx, 
						    struct sockaddr *sockaddr, 
						    size_t addrlen);
struct socket_address *socket_address_copy(TALLOC_CTX *mem_ctx,
					   const struct socket_address *oaddr);
const struct socket_ops *socket_getops_byname(const char *name, enum socket_type type);
bool allow_access(TALLOC_CTX *mem_ctx,
		  const char **deny_list, const char **allow_list,
		  const char *cname, const char *caddr);
bool socket_check_access(struct socket_context *sock, 
			 const char *service_name,
			 const char **allow_list, const char **deny_list);

struct composite_context *socket_connect_send(struct socket_context *sock,
					      struct socket_address *my_address,
					      struct socket_address *server_address, 
					      uint32_t flags,
					      struct tevent_context *event_ctx);
NTSTATUS socket_connect_recv(struct composite_context *ctx);
NTSTATUS socket_connect_ev(struct socket_context *sock,
			   struct socket_address *my_address,
			   struct socket_address *server_address, 
			   uint32_t flags, 
			   struct tevent_context *ev);

struct composite_context *socket_connect_multi_send(TALLOC_CTX *mem_ctx,
						    const char *server_address,
						    int num_server_ports,
						    uint16_t *server_ports,
						    struct resolve_context *resolve_ctx,
						    struct tevent_context *event_ctx);
NTSTATUS socket_connect_multi_recv(struct composite_context *ctx,
				   TALLOC_CTX *mem_ctx,
				   struct socket_context **result,
				   uint16_t *port);
NTSTATUS socket_connect_multi(TALLOC_CTX *mem_ctx, const char *server_address,
			      int num_server_ports, uint16_t *server_ports,
			      struct resolve_context *resolve_ctx,
			      struct tevent_context *event_ctx,
			      struct socket_context **result,
			      uint16_t *port);
void set_socket_options(int fd, const char *options);
void socket_set_flags(struct socket_context *socket, unsigned flags);

void socket_tevent_fd_close_fn(struct tevent_context *ev,
			       struct tevent_fd *fde,
			       int fd,
			       void *private_data);

extern bool testnonblock;

#endif /* _SAMBA_SOCKET_H */
