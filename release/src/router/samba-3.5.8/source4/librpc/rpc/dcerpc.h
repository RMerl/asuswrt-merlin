/* 
   Unix SMB/CIFS implementation.

   DCERPC client side interface structures

   Copyright (C) Tim Potter 2003
   Copyright (C) Andrew Tridgell 2003-2005
   
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

/* This is a public header file that is installed as part of Samba. 
 * If you remove any functions or change their signature, update 
 * the so version number. */

#ifndef __DCERPC_H__
#define __DCERPC_H__

#include "../lib/util/data_blob.h"
#include "librpc/gen_ndr/dcerpc.h"
#include "../librpc/ndr/libndr.h"

enum dcerpc_transport_t {
	NCA_UNKNOWN, NCACN_NP, NCACN_IP_TCP, NCACN_IP_UDP, NCACN_VNS_IPC, 
	NCACN_VNS_SPP, NCACN_AT_DSP, NCADG_AT_DDP, NCALRPC, NCACN_UNIX_STREAM, 
	NCADG_UNIX_DGRAM, NCACN_HTTP, NCADG_IPX, NCACN_SPX };

/*
  this defines a generic security context for signed/sealed dcerpc pipes.
*/
struct dcerpc_connection;
struct gensec_settings;
struct dcerpc_security {
	struct dcerpc_auth *auth_info;
	struct gensec_security *generic_state;

	/* get the session key */
	NTSTATUS (*session_key)(struct dcerpc_connection *, DATA_BLOB *);
};

/*
  this holds the information that is not specific to a particular rpc context_id
*/
struct dcerpc_connection {
	uint32_t call_id;
	uint32_t srv_max_xmit_frag;
	uint32_t srv_max_recv_frag;
	uint32_t flags;
	struct dcerpc_security security_state;
	const char *binding_string;
	struct tevent_context *event_ctx;
	struct smb_iconv_convenience *iconv_convenience;

	/** Directory in which to save ndrdump-parseable files */
	const char *packet_log_dir;

	bool dead;
	bool free_skipped;

	struct dcerpc_transport {
		enum dcerpc_transport_t transport;
		void *private_data;

		NTSTATUS (*shutdown_pipe)(struct dcerpc_connection *, NTSTATUS status);

		const char *(*peer_name)(struct dcerpc_connection *);

		const char *(*target_hostname)(struct dcerpc_connection *);

		/* send a request to the server */
		NTSTATUS (*send_request)(struct dcerpc_connection *, DATA_BLOB *, bool trigger_read);

		/* send a read request to the server */
		NTSTATUS (*send_read)(struct dcerpc_connection *);

		/* a callback to the dcerpc code when a full fragment
		   has been received */
		void (*recv_data)(struct dcerpc_connection *, DATA_BLOB *, NTSTATUS status);
	} transport;

	/* Requests that have been sent, waiting for a reply */
	struct rpc_request *pending;

	/* Sync requests waiting to be shipped */
	struct rpc_request *request_queue;

	/* the next context_id to be assigned */
	uint32_t next_context_id;
};

/*
  this encapsulates a full dcerpc client side pipe 
*/
struct dcerpc_pipe {
	uint32_t context_id;

	uint32_t assoc_group_id;

	struct ndr_syntax_id syntax;
	struct ndr_syntax_id transfer_syntax;

	struct dcerpc_connection *conn;
	struct dcerpc_binding *binding;

	/** the last fault code from a DCERPC fault */
	uint32_t last_fault_code;

	/** timeout for individual rpc requests, in seconds */
	uint32_t request_timeout;
};

/* default timeout for all rpc requests, in seconds */
#define DCERPC_REQUEST_TIMEOUT 60


/* dcerpc pipe flags */
#define DCERPC_DEBUG_PRINT_IN          (1<<0)
#define DCERPC_DEBUG_PRINT_OUT         (1<<1)
#define DCERPC_DEBUG_PRINT_BOTH (DCERPC_DEBUG_PRINT_IN | DCERPC_DEBUG_PRINT_OUT)

#define DCERPC_DEBUG_VALIDATE_IN       (1<<2)
#define DCERPC_DEBUG_VALIDATE_OUT      (1<<3)
#define DCERPC_DEBUG_VALIDATE_BOTH (DCERPC_DEBUG_VALIDATE_IN | DCERPC_DEBUG_VALIDATE_OUT)

#define DCERPC_CONNECT                 (1<<4)
#define DCERPC_SIGN                    (1<<5)
#define DCERPC_SEAL                    (1<<6)

#define DCERPC_PUSH_BIGENDIAN          (1<<7)
#define DCERPC_PULL_BIGENDIAN          (1<<8)

#define DCERPC_SCHANNEL                (1<<9)

#define DCERPC_ANON_FALLBACK           (1<<10)

/* use a 128 bit session key */
#define DCERPC_SCHANNEL_128            (1<<12)

/* check incoming pad bytes */
#define DCERPC_DEBUG_PAD_CHECK         (1<<13)

/* set LIBNDR_FLAG_REF_ALLOC flag when decoding NDR */
#define DCERPC_NDR_REF_ALLOC           (1<<14)

#define DCERPC_AUTH_OPTIONS    (DCERPC_SEAL|DCERPC_SIGN|DCERPC_SCHANNEL|DCERPC_AUTH_SPNEGO|DCERPC_AUTH_KRB5|DCERPC_AUTH_NTLM)

/* select spnego auth */
#define DCERPC_AUTH_SPNEGO             (1<<15)

/* select krb5 auth */
#define DCERPC_AUTH_KRB5               (1<<16)

#define DCERPC_SMB2                    (1<<17)

/* select NTLM auth */
#define DCERPC_AUTH_NTLM               (1<<18)

/* this triggers the DCERPC_PFC_FLAG_CONC_MPX flag in the bind request */
#define DCERPC_CONCURRENT_MULTIPLEX     (1<<19)

/* this triggers the DCERPC_PFC_FLAG_SUPPORT_HEADER_SIGN flag in the bind request */
#define DCERPC_HEADER_SIGNING          (1<<20)

/* use NDR64 transport */
#define DCERPC_NDR64                   (1<<21)

/* this describes a binding to a particular transport/pipe */
struct dcerpc_binding {
	enum dcerpc_transport_t transport;
	struct ndr_syntax_id object;
	const char *host;
	const char *target_hostname;
	const char *endpoint;
	const char **options;
	uint32_t flags;
	uint32_t assoc_group_id;
};


struct dcerpc_pipe_connect {
	struct dcerpc_pipe *pipe;
	struct dcerpc_binding *binding;
	const char *pipe_name;
	const struct ndr_interface_table *interface;
	struct cli_credentials *creds;
	struct resolve_context *resolve_ctx;
};


enum rpc_request_state {
	RPC_REQUEST_QUEUED,
	RPC_REQUEST_PENDING,
	RPC_REQUEST_DONE
};

/*
  handle for an async dcerpc request
*/
struct rpc_request {
	struct rpc_request *next, *prev;
	struct dcerpc_pipe *p;
	NTSTATUS status;
	uint32_t call_id;
	enum rpc_request_state state;
	DATA_BLOB payload;
	uint32_t flags;
	uint32_t fault_code;

	/* this is used to distinguish bind and alter_context requests
	   from normal requests */
	void (*recv_handler)(struct rpc_request *conn, 
			     DATA_BLOB *blob, struct ncacn_packet *pkt);

	const struct GUID *object;
	uint16_t opnum;
	DATA_BLOB request_data;
	bool async_call;
	bool ignore_timeout;

	/* use by the ndr level async recv call */
	struct {
		const struct ndr_interface_table *table;
		uint32_t opnum;
		void *struct_ptr;
		TALLOC_CTX *mem_ctx;
	} ndr;

	struct {
		void (*callback)(struct rpc_request *);
		void *private_data;
	} async;
};

struct epm_tower;
struct epm_floor;

struct smbcli_tree;
struct smb2_tree;
struct socket_address;

NTSTATUS dcerpc_pipe_connect(TALLOC_CTX *parent_ctx, 
			     struct dcerpc_pipe **pp, 
			     const char *binding,
			     const struct ndr_interface_table *table,
			     struct cli_credentials *credentials,
			     struct tevent_context *ev,
			     struct loadparm_context *lp_ctx);
NTSTATUS dcerpc_ndr_request_recv(struct rpc_request *req);
struct rpc_request *dcerpc_ndr_request_send(struct dcerpc_pipe *p,
						const struct GUID *object,
						const struct ndr_interface_table *table,
						uint32_t opnum, 
						bool async,
						TALLOC_CTX *mem_ctx, 
						void *r);
const char *dcerpc_server_name(struct dcerpc_pipe *p);
struct dcerpc_pipe *dcerpc_pipe_init(TALLOC_CTX *mem_ctx, struct tevent_context *ev,
				     struct smb_iconv_convenience *ic);
NTSTATUS dcerpc_pipe_open_smb(struct dcerpc_pipe *p,
			      struct smbcli_tree *tree,
			      const char *pipe_name);
NTSTATUS dcerpc_bind_auth_none(struct dcerpc_pipe *p,
			       const struct ndr_interface_table *table);
NTSTATUS dcerpc_fetch_session_key(struct dcerpc_pipe *p,
				  DATA_BLOB *session_key);
struct composite_context;
NTSTATUS dcerpc_secondary_connection_recv(struct composite_context *c,
					  struct dcerpc_pipe **p2);
NTSTATUS dcerpc_parse_binding(TALLOC_CTX *mem_ctx, const char *s, struct dcerpc_binding **b_out);

struct composite_context* dcerpc_pipe_connect_b_send(TALLOC_CTX *parent_ctx,
						     struct dcerpc_binding *binding,
						     const struct ndr_interface_table *table,
						     struct cli_credentials *credentials,
						     struct tevent_context *ev,
						     struct loadparm_context *lp_ctx);

NTSTATUS dcerpc_pipe_connect_b_recv(struct composite_context *c, TALLOC_CTX *mem_ctx,
				    struct dcerpc_pipe **p);

NTSTATUS dcerpc_pipe_connect_b(TALLOC_CTX *parent_ctx,
			       struct dcerpc_pipe **pp,
			       struct dcerpc_binding *binding,
			       const struct ndr_interface_table *table,
			       struct cli_credentials *credentials,
			       struct tevent_context *ev,
			       struct loadparm_context *lp_ctx);
const char *dcerpc_errstr(TALLOC_CTX *mem_ctx, uint32_t fault_code);

NTSTATUS dcerpc_pipe_auth(TALLOC_CTX *mem_ctx,
			  struct dcerpc_pipe **p, 
			  struct dcerpc_binding *binding,
			  const struct ndr_interface_table *table,
			  struct cli_credentials *credentials,
			  struct loadparm_context *lp_ctx);
char *dcerpc_binding_string(TALLOC_CTX *mem_ctx, const struct dcerpc_binding *b);
NTSTATUS dcerpc_secondary_connection(struct dcerpc_pipe *p,
				     struct dcerpc_pipe **p2,
				     struct dcerpc_binding *b);
NTSTATUS dcerpc_bind_auth_schannel(TALLOC_CTX *tmp_ctx, 
				   struct dcerpc_pipe *p,
				   const struct ndr_interface_table *table,
				   struct cli_credentials *credentials,
				   struct loadparm_context *lp_ctx,
				   uint8_t auth_level);
struct tevent_context *dcerpc_event_context(struct dcerpc_pipe *p);
NTSTATUS dcerpc_init(struct loadparm_context *lp_ctx);
struct smbcli_tree *dcerpc_smb_tree(struct dcerpc_connection *c);
uint16_t dcerpc_smb_fnum(struct dcerpc_connection *c);
NTSTATUS dcerpc_secondary_context(struct dcerpc_pipe *p, 
				  struct dcerpc_pipe **pp2,
				  const struct ndr_interface_table *table);
NTSTATUS dcerpc_alter_context(struct dcerpc_pipe *p, 
			      TALLOC_CTX *mem_ctx,
			      const struct ndr_syntax_id *syntax,
			      const struct ndr_syntax_id *transfer_syntax);

NTSTATUS dcerpc_bind_auth(struct dcerpc_pipe *p,
			  const struct ndr_interface_table *table,
			  struct cli_credentials *credentials,
			  struct gensec_settings *gensec_settings,
			  uint8_t auth_type, uint8_t auth_level,
			  const char *service);
struct composite_context* dcerpc_pipe_connect_send(TALLOC_CTX *parent_ctx,
						   const char *binding,
						   const struct ndr_interface_table *table,
						   struct cli_credentials *credentials,
						   struct tevent_context *ev, struct loadparm_context *lp_ctx);
NTSTATUS dcerpc_pipe_connect_recv(struct composite_context *c,
				  TALLOC_CTX *mem_ctx,
				  struct dcerpc_pipe **pp);

NTSTATUS dcerpc_epm_map_binding(TALLOC_CTX *mem_ctx, struct dcerpc_binding *binding,
				const struct ndr_interface_table *table, struct tevent_context *ev,
				struct loadparm_context *lp_ctx);
struct composite_context* dcerpc_secondary_auth_connection_send(struct dcerpc_pipe *p,
								struct dcerpc_binding *binding,
								const struct ndr_interface_table *table,
								struct cli_credentials *credentials,
								struct loadparm_context *lp_ctx);
NTSTATUS dcerpc_secondary_auth_connection_recv(struct composite_context *c, 
					       TALLOC_CTX *mem_ctx,
					       struct dcerpc_pipe **p);

struct composite_context* dcerpc_secondary_connection_send(struct dcerpc_pipe *p,
							   struct dcerpc_binding *b);
void dcerpc_log_packet(const char *lockdir, 
					   const struct ndr_interface_table *ndr,
					   uint32_t opnum, uint32_t flags, 
					   DATA_BLOB *pkt);
NTSTATUS dcerpc_binding_build_tower(TALLOC_CTX *mem_ctx,
				    const struct dcerpc_binding *binding,
				    struct epm_tower *tower);

NTSTATUS dcerpc_floor_get_lhs_data(const struct epm_floor *epm_floor, struct ndr_syntax_id *syntax);

enum dcerpc_transport_t dcerpc_transport_by_tower(const struct epm_tower *tower);
const char *derpc_transport_string_by_transport(enum dcerpc_transport_t t);

NTSTATUS dcerpc_ndr_request(struct dcerpc_pipe *p,
			    const struct GUID *object,
			    const struct ndr_interface_table *table,
			    uint32_t opnum, 
			    TALLOC_CTX *mem_ctx, 
			    void *r);

NTSTATUS dcerpc_binding_from_tower(TALLOC_CTX *mem_ctx, 
				   struct epm_tower *tower, 
				   struct dcerpc_binding **b_out);

NTSTATUS dcerpc_request(struct dcerpc_pipe *p, 
			struct GUID *object,
			uint16_t opnum,
			TALLOC_CTX *mem_ctx,
			DATA_BLOB *stub_data_in,
			DATA_BLOB *stub_data_out);

typedef NTSTATUS (*dcerpc_call_fn) (struct dcerpc_pipe *, TALLOC_CTX *, void *);

enum dcerpc_transport_t dcerpc_transport_by_endpoint_protocol(int prot);

const char *dcerpc_floor_get_rhs_data(TALLOC_CTX *mem_ctx, struct epm_floor *epm_floor);

#endif /* __DCERPC_H__ */
