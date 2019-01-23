/*
   Unix SMB/CIFS implementation.

   a raw async NBT library

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

#ifndef __LIBNBT_H__
#define __LIBNBT_H__

#include "librpc/gen_ndr/nbt.h"
#include "librpc/ndr/libndr.h"
#include "system/network.h"
/*
  possible states for pending requests
*/
enum nbt_request_state {NBT_REQUEST_SEND,
			NBT_REQUEST_WAIT,
			NBT_REQUEST_DONE,
			NBT_REQUEST_TIMEOUT,
			NBT_REQUEST_ERROR};

/*
  a nbt name request
*/
struct nbt_name_request {
	struct nbt_name_request *next, *prev;

	enum nbt_request_state state;

	NTSTATUS status;

	/* the socket this was on */
	struct nbt_name_socket *nbtsock;

	/* where to send the request */
	struct socket_address *dest;

	/* timeout between retries */
	int timeout;

	/* how many retries to send on timeout */
	int num_retries;

	/* whether we have received a WACK */
	bool received_wack;

	/* the timeout event */
	struct tevent_timer *te;

	/* the name transaction id */
	uint16_t name_trn_id;

	/* is it a reply? */
	bool is_reply;

	/* the encoded request */
	DATA_BLOB encoded;

	/* shall we allow multiple replies? */
	bool allow_multiple_replies;

	unsigned int num_replies;
	struct nbt_name_reply {
		struct nbt_name_packet *packet;
		struct socket_address *dest;
	} *replies;

	/* information on what to do on completion */
	struct {
		void (*fn)(struct nbt_name_request *);
		void *private_data;
	} async;
};



/*
  context structure for operations on name queries
*/
struct nbt_name_socket {
	struct socket_context *sock;
	struct tevent_context *event_ctx;

	/* a queue of requests pending to be sent */
	struct nbt_name_request *send_queue;

	/* the fd event */
	struct tevent_fd *fde;

	/* mapping from name_trn_id to pending event */
	struct idr_context *idr;

	/* how many requests are waiting for a reply */
	uint16_t num_pending;

	/* what to do with incoming request packets */
	struct {
		void (*handler)(struct nbt_name_socket *, struct nbt_name_packet *,
				struct socket_address *);
		void *private_data;
	} incoming;

	/* what to do with unexpected replies */
	struct {
		void (*handler)(struct nbt_name_socket *, struct nbt_name_packet *,
				struct socket_address *);
		void *private_data;
	} unexpected;
};


/* a simple name query */
struct nbt_name_query {
	struct {
		struct nbt_name name;
		const char *dest_addr;
		uint16_t dest_port;
		bool broadcast;
		bool wins_lookup;
		int timeout; /* in seconds */
		int retries;
	} in;
	struct {
		const char *reply_from;
		struct nbt_name name;
		int16_t num_addrs;
		const char **reply_addrs;
	} out;
};

/* a simple name status query */
struct nbt_name_status {
	struct {
		struct nbt_name name;
		const char *dest_addr;
		uint16_t dest_port;
		int timeout; /* in seconds */
		int retries;
	} in;
	struct {
		const char *reply_from;
		struct nbt_name name;
		struct nbt_rdata_status status;
	} out;
};

/* a name registration request */
struct nbt_name_register {
	struct {
		struct nbt_name name;
		const char *dest_addr;
		uint16_t dest_port;
		const char *address;
		uint16_t nb_flags;
		bool register_demand;
		bool broadcast;
		bool multi_homed;
		uint32_t ttl;
		int timeout; /* in seconds */
		int retries;
	} in;
	struct {
		const char *reply_from;
		struct nbt_name name;
		const char *reply_addr;
		uint8_t rcode;
	} out;
};

/* a send 3 times then demand name broadcast name registration */
struct nbt_name_register_bcast {
	struct {
		struct nbt_name name;
		const char *dest_addr;
		uint16_t dest_port;
		const char *address;
		uint16_t nb_flags;
		uint32_t ttl;
	} in;
};


/* wins name register with multiple wins servers to try and multiple
   addresses to register */
struct nbt_name_register_wins {
	struct {
		struct nbt_name name;
		const char **wins_servers;
		uint16_t wins_port;
		const char **addresses;
		uint16_t nb_flags;
		uint32_t ttl;
	} in;
	struct {
		const char *wins_server;
		uint8_t rcode;
	} out;
};



/* a name refresh request */
struct nbt_name_refresh {
	struct {
		struct nbt_name name;
		const char *dest_addr;
		uint16_t dest_port;
		const char *address;
		uint16_t nb_flags;
		bool broadcast;
		uint32_t ttl;
		int timeout; /* in seconds */
		int retries;
	} in;
	struct {
		const char *reply_from;
		struct nbt_name name;
		const char *reply_addr;
		uint8_t rcode;
	} out;
};

/* wins name refresh with multiple wins servers to try and multiple
   addresses to register */
struct nbt_name_refresh_wins {
	struct {
		struct nbt_name name;
		const char **wins_servers;
		uint16_t wins_port;
		const char **addresses;
		uint16_t nb_flags;
		uint32_t ttl;
	} in;
	struct {
		const char *wins_server;
		uint8_t rcode;
	} out;
};


/* a name release request */
struct nbt_name_release {
	struct {
		struct nbt_name name;
		const char *dest_addr;
		uint16_t dest_port;
		const char *address;
		uint16_t nb_flags;
		bool broadcast;
		int timeout; /* in seconds */
		int retries;
	} in;
	struct {
		const char *reply_from;
		struct nbt_name name;
		const char *reply_addr;
		uint8_t rcode;
	} out;
};

struct nbt_name_socket *nbt_name_socket_init(TALLOC_CTX *mem_ctx,
					     struct tevent_context *event_ctx);
void nbt_name_socket_handle_response_packet(struct nbt_name_request *req,
					    struct nbt_name_packet *packet,
					    struct socket_address *src);
struct nbt_name_request *nbt_name_query_send(struct nbt_name_socket *nbtsock,
					     struct nbt_name_query *io);
NTSTATUS nbt_name_query_recv(struct nbt_name_request *req,
			     TALLOC_CTX *mem_ctx, struct nbt_name_query *io);
NTSTATUS nbt_name_query(struct nbt_name_socket *nbtsock,
			TALLOC_CTX *mem_ctx, struct nbt_name_query *io);
struct nbt_name_request *nbt_name_status_send(struct nbt_name_socket *nbtsock,
					      struct nbt_name_status *io);
NTSTATUS nbt_name_status_recv(struct nbt_name_request *req,
			     TALLOC_CTX *mem_ctx, struct nbt_name_status *io);
NTSTATUS nbt_name_status(struct nbt_name_socket *nbtsock,
			TALLOC_CTX *mem_ctx, struct nbt_name_status *io);

NTSTATUS nbt_name_dup(TALLOC_CTX *mem_ctx, struct nbt_name *name, struct nbt_name *newname);
NTSTATUS nbt_name_to_blob(TALLOC_CTX *mem_ctx, DATA_BLOB *blob, struct nbt_name *name);
NTSTATUS nbt_name_from_blob(TALLOC_CTX *mem_ctx, const DATA_BLOB *blob, struct nbt_name *name);
void nbt_choose_called_name(TALLOC_CTX *mem_ctx, struct nbt_name *n, const char *name, int type);
char *nbt_name_string(TALLOC_CTX *mem_ctx, const struct nbt_name *name);
NTSTATUS nbt_name_register(struct nbt_name_socket *nbtsock,
			   TALLOC_CTX *mem_ctx, struct nbt_name_register *io);
NTSTATUS nbt_name_refresh(struct nbt_name_socket *nbtsock,
			   TALLOC_CTX *mem_ctx, struct nbt_name_refresh *io);
NTSTATUS nbt_name_release(struct nbt_name_socket *nbtsock,
			   TALLOC_CTX *mem_ctx, struct nbt_name_release *io);
NTSTATUS nbt_name_register_wins(struct nbt_name_socket *nbtsock,
				TALLOC_CTX *mem_ctx,
				struct nbt_name_register_wins *io);
NTSTATUS nbt_name_refresh_wins(struct nbt_name_socket *nbtsock,
				TALLOC_CTX *mem_ctx,
				struct nbt_name_refresh_wins *io);
NTSTATUS nbt_name_register_recv(struct nbt_name_request *req,
				TALLOC_CTX *mem_ctx, struct nbt_name_register *io);
struct nbt_name_request *nbt_name_register_send(struct nbt_name_socket *nbtsock,
						struct nbt_name_register *io);
NTSTATUS nbt_name_release_recv(struct nbt_name_request *req,
			       TALLOC_CTX *mem_ctx, struct nbt_name_release *io);

struct nbt_name_request *nbt_name_release_send(struct nbt_name_socket *nbtsock,
					       struct nbt_name_release *io);

NTSTATUS nbt_name_refresh_recv(struct nbt_name_request *req,
			       TALLOC_CTX *mem_ctx, struct nbt_name_refresh *io);

NTSTATUS nbt_set_incoming_handler(struct nbt_name_socket *nbtsock,
				  void (*handler)(struct nbt_name_socket *, struct nbt_name_packet *,
						  struct socket_address *),
				  void *private_data);
NTSTATUS nbt_set_unexpected_handler(struct nbt_name_socket *nbtsock,
				    void (*handler)(struct nbt_name_socket *, struct nbt_name_packet *,
						    struct socket_address *),
				    void *private_data);
NTSTATUS nbt_name_reply_send(struct nbt_name_socket *nbtsock,
			     struct socket_address *dest,
			     struct nbt_name_packet *request);


NDR_SCALAR_PROTO(wrepl_nbt_name, const struct nbt_name *)
NDR_SCALAR_PROTO(nbt_string, const char *)
NDR_BUFFER_PROTO(nbt_name, struct nbt_name)
NTSTATUS nbt_rcode_to_ntstatus(uint8_t rcode);

struct tevent_context;
struct tevent_req;
struct tevent_req *nbt_name_register_bcast_send(TALLOC_CTX *mem_ctx,
					struct tevent_context *ev,
					struct nbt_name_socket *nbtsock,
					struct nbt_name_register_bcast *io);
NTSTATUS nbt_name_register_bcast_recv(struct tevent_req *req);
struct tevent_req *nbt_name_register_wins_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct nbt_name_socket *nbtsock,
					       struct nbt_name_register_wins *io);
NTSTATUS nbt_name_register_wins_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     struct nbt_name_register_wins *io);
struct tevent_req *nbt_name_refresh_wins_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct nbt_name_socket *nbtsock,
					      struct nbt_name_refresh_wins *io);
NTSTATUS nbt_name_refresh_wins_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx,
				    struct nbt_name_refresh_wins *io);

XFILE *startlmhosts(const char *fname);
bool getlmhostsent(TALLOC_CTX *ctx, XFILE *fp, char **pp_name, int *name_type,
		struct sockaddr_storage *pss);
void endlmhosts(XFILE *fp);

NTSTATUS resolve_lmhosts_file_as_sockaddr(const char *lmhosts_file, 
					  const char *name, int name_type,
					  TALLOC_CTX *mem_ctx, 
					  struct sockaddr_storage **return_iplist,
					  int *return_count);

NTSTATUS resolve_dns_hosts_file_as_sockaddr(const char *dns_hosts_file, 
					    const char *name, bool srv_lookup,
					    TALLOC_CTX *mem_ctx, 
					    struct sockaddr_storage **return_iplist,
					    int *return_count);

#endif /* __LIBNBT_H__ */
