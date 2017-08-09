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

#ifndef __S4_DCERPC_H__
#define __S4_DCERPC_H__

#include "../lib/util/data_blob.h"
#include "librpc/gen_ndr/dcerpc.h"
#include "../librpc/ndr/libndr.h"
#include "../librpc/rpc/rpc_common.h"

struct tevent_context;
struct tevent_req;
struct dcerpc_binding_handle;
struct tstream_context;

/*
  this defines a generic security context for signed/sealed dcerpc pipes.
*/
struct dcecli_connection;
struct gensec_settings;
struct dcecli_security {
	struct dcerpc_auth *auth_info;
	struct gensec_security *generic_state;

	/* get the session key */
	NTSTATUS (*session_key)(struct dcecli_connection *, DATA_BLOB *);
};

/*
  this holds the information that is not specific to a particular rpc context_id
*/
struct rpc_request;
struct dcecli_connection {
	uint32_t call_id;
	uint32_t srv_max_xmit_frag;
	uint32_t srv_max_recv_frag;
	uint32_t flags;
	struct dcecli_security security_state;
	const char *binding_string;
	struct tevent_context *event_ctx;

	/** Directory in which to save ndrdump-parseable files */
	const char *packet_log_dir;

	bool dead;
	bool free_skipped;

	struct dcerpc_transport {
		enum dcerpc_transport_t transport;
		void *private_data;

		NTSTATUS (*shutdown_pipe)(struct dcecli_connection *, NTSTATUS status);

		const char *(*peer_name)(struct dcecli_connection *);

		const char *(*target_hostname)(struct dcecli_connection *);

		/* send a request to the server */
		NTSTATUS (*send_request)(struct dcecli_connection *, DATA_BLOB *, bool trigger_read);

		/* send a read request to the server */
		NTSTATUS (*send_read)(struct dcecli_connection *);

		/* a callback to the dcerpc code when a full fragment
		   has been received */
		void (*recv_data)(struct dcecli_connection *, DATA_BLOB *, NTSTATUS status);
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
	struct dcerpc_binding_handle *binding_handle;

	uint32_t context_id;

	uint32_t assoc_group_id;

	struct ndr_syntax_id syntax;
	struct ndr_syntax_id transfer_syntax;

	struct dcecli_connection *conn;
	struct dcerpc_binding *binding;

	/** the last fault code from a DCERPC fault */
	uint32_t last_fault_code;

	/** timeout for individual rpc requests, in seconds */
	uint32_t request_timeout;
};

/* default timeout for all rpc requests, in seconds */
#define DCERPC_REQUEST_TIMEOUT 60


struct dcerpc_pipe_connect {
	struct dcerpc_pipe *pipe;
	struct dcerpc_binding *binding;
	const char *pipe_name;
	const struct ndr_interface_table *interface;
	struct cli_credentials *creds;
	struct resolve_context *resolve_ctx;
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
const char *dcerpc_server_name(struct dcerpc_pipe *p);
struct dcerpc_pipe *dcerpc_pipe_init(TALLOC_CTX *mem_ctx, struct tevent_context *ev);
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

NTSTATUS dcerpc_pipe_auth(TALLOC_CTX *mem_ctx,
			  struct dcerpc_pipe **p, 
			  struct dcerpc_binding *binding,
			  const struct ndr_interface_table *table,
			  struct cli_credentials *credentials,
			  struct loadparm_context *lp_ctx);
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
struct smbcli_tree *dcerpc_smb_tree(struct dcecli_connection *c);
uint16_t dcerpc_smb_fnum(struct dcecli_connection *c);
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
		       const DATA_BLOB *pkt);


enum dcerpc_transport_t dcerpc_transport_by_endpoint_protocol(int prot);

const char *dcerpc_floor_get_rhs_data(TALLOC_CTX *mem_ctx, struct epm_floor *epm_floor);

#endif /* __S4_DCERPC_H__ */
