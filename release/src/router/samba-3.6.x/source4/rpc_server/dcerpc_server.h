/* 
   Unix SMB/CIFS implementation.

   server side dcerpc defines

   Copyright (C) Andrew Tridgell 2003-2005
   Copyright (C) Stefan (metze) Metzmacher 2004-2005
   
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

#ifndef SAMBA_DCERPC_SERVER_H
#define SAMBA_DCERPC_SERVER_H

#include "librpc/gen_ndr/server_id4.h"
#include "librpc/rpc/dcerpc.h"
#include "librpc/ndr/libndr.h"

/* modules can use the following to determine if the interface has changed
 * please increment the version number after each interface change
 * with a comment and maybe update struct dcesrv_critical_sizes.
 */
/* version 1 - initial version - metze */
#define DCERPC_MODULE_VERSION 1

struct dcesrv_connection;
struct dcesrv_call_state;
struct dcesrv_auth;
struct dcesrv_connection_context;

struct dcesrv_interface {
	const char *name;
	struct ndr_syntax_id syntax_id;

	/* this function is called when the client binds to this interface  */
	NTSTATUS (*bind)(struct dcesrv_call_state *, const struct dcesrv_interface *, uint32_t if_version);

	/* this function is called when the client disconnects the endpoint */
	void (*unbind)(struct dcesrv_connection_context *, const struct dcesrv_interface *);

	/* the ndr_pull function for the chosen interface.
	 */
	NTSTATUS (*ndr_pull)(struct dcesrv_call_state *, TALLOC_CTX *, struct ndr_pull *, void **);
	
	/* the dispatch function for the chosen interface.
	 */
	NTSTATUS (*dispatch)(struct dcesrv_call_state *, TALLOC_CTX *, void *);

	/* the reply function for the chosen interface.
	 */
	NTSTATUS (*reply)(struct dcesrv_call_state *, TALLOC_CTX *, void *);

	/* the ndr_push function for the chosen interface.
	 */
	NTSTATUS (*ndr_push)(struct dcesrv_call_state *, TALLOC_CTX *, struct ndr_push *, const void *);

	/* for any private use by the interface code */
	const void *private_data;
};

enum dcesrv_call_list {
	DCESRV_LIST_NONE,
	DCESRV_LIST_CALL_LIST,
	DCESRV_LIST_FRAGMENTED_CALL_LIST,
	DCESRV_LIST_PENDING_CALL_LIST
};

/* the state of an ongoing dcerpc call */
struct dcesrv_call_state {
	struct dcesrv_call_state *next, *prev;
	struct dcesrv_connection *conn;
	struct dcesrv_connection_context *context;
	struct ncacn_packet pkt;

	/*
	  which list this request is in, if any
	 */
	enum dcesrv_call_list list;

	/* the backend can mark the call
	 * with DCESRV_CALL_STATE_FLAG_ASYNC
	 * that will cause the frontend to not touch r->out
	 * and skip the reply
	 *
	 * this is only allowed to the backend when DCESRV_CALL_STATE_FLAG_MAY_ASYNC
	 * is alerady set by the frontend
	 *
	 * the backend then needs to call dcesrv_reply() when it's
	 * ready to send the reply
	 */
#define DCESRV_CALL_STATE_FLAG_ASYNC (1<<0)
#define DCESRV_CALL_STATE_FLAG_MAY_ASYNC (1<<1)
#define DCESRV_CALL_STATE_FLAG_HEADER_SIGNING (1<<2)
	uint32_t state_flags;

	/* the time the request arrived in the server */
	struct timeval time;

	/* the backend can use this event context for async replies */
	struct tevent_context *event_ctx;

	/* the message_context that will be used for async replies */
	struct messaging_context *msg_ctx;

	/* this is the pointer to the allocated function struct */
	void *r;

	/*
	 * that's the ndr pull context used in dcesrv_request()
	 * needed by dcesrv_reply() to carry over information
	 * for full pointer support.
	 */
	struct ndr_pull *ndr_pull;

	DATA_BLOB input;

	struct data_blob_list_item *replies;

	/* this is used by the boilerplate code to generate DCERPC faults */
	uint32_t fault_code;
};

#define DCESRV_HANDLE_ANY 255

/* a dcerpc handle in internal format */
struct dcesrv_handle {
	struct dcesrv_handle *next, *prev;
	struct dcesrv_assoc_group *assoc_group;
	struct policy_handle wire_handle;
	struct dom_sid *sid;
	const struct dcesrv_interface *iface;
	void *data;
};

/* hold the authentication state information */
struct dcesrv_auth {
	struct dcerpc_auth *auth_info;
	struct gensec_security *gensec_security;
	struct auth_session_info *session_info;
	NTSTATUS (*session_key)(struct dcesrv_connection *, DATA_BLOB *session_key);
};

struct dcesrv_connection_context {
	struct dcesrv_connection_context *next, *prev;
	uint32_t context_id;

	struct dcesrv_assoc_group *assoc_group;

	/* the connection this is on */
	struct dcesrv_connection *conn;

	/* the ndr function table for the chosen interface */
	const struct dcesrv_interface *iface;

	/* private data for the interface implementation */
	void *private_data;
};


/* the state associated with a dcerpc server connection */
struct dcesrv_connection {
	/* the top level context for this server */
	struct dcesrv_context *dce_ctx;

	/* the endpoint that was opened */
	const struct dcesrv_endpoint *endpoint;

	/* a list of established context_ids */
	struct dcesrv_connection_context *contexts;

	/* the state of the current incoming call fragments */
	struct dcesrv_call_state *incoming_fragmented_call_list;

	/* the state of the async pending calls */
	struct dcesrv_call_state *pending_call_list;

	/* the state of the current outgoing calls */
	struct dcesrv_call_state *call_list;

	/* the maximum size the client wants to receive */
	uint32_t cli_max_recv_frag;

	DATA_BLOB partial_input;

	/* the current authentication state */
	struct dcesrv_auth auth_state;

	/* the event_context that will be used for this connection */
	struct tevent_context *event_ctx;

	/* the message_context that will be used for this connection */
	struct messaging_context *msg_ctx;

	/* the server_id that will be used for this connection */
	struct server_id server_id;

	/* the transport level session key */
	DATA_BLOB transport_session_key;

	bool processing;

	const char *packet_log_dir;

	/* this is the default state_flags for dcesrv_call_state structs */
	uint32_t state_flags;

	struct {
		void *private_data;
		void (*report_output_data)(struct dcesrv_connection *);
	} transport;

	struct tstream_context *stream;
	struct tevent_queue *send_queue;

	const struct tsocket_address *local_address;
	const struct tsocket_address *remote_address;
};


struct dcesrv_endpoint_server {
	/* this is the name of the endpoint server */
	const char *name;

	/* this function should register endpoints and some other setup stuff,
	 * it is called when the dcesrv_context gets initialized.
	 */
	NTSTATUS (*init_server)(struct dcesrv_context *, const struct dcesrv_endpoint_server *);

	/* this function can be used by other endpoint servers to
	 * ask for a dcesrv_interface implementation
	 * - iface must be reference to an already existing struct !
	 */
	bool (*interface_by_uuid)(struct dcesrv_interface *iface, const struct GUID *, uint32_t);

	/* this function can be used by other endpoint servers to
	 * ask for a dcesrv_interface implementation
	 * - iface must be reference to an already existeng struct !
	 */
	bool (*interface_by_name)(struct dcesrv_interface *iface, const char *);
};


/* one association groups */
struct dcesrv_assoc_group {
	/* the wire id */
	uint32_t id;
	
	/* list of handles in this association group */
	struct dcesrv_handle *handles;

	/* parent context */
	struct dcesrv_context *dce_ctx;

	/* Remote association group ID (if proxied) */
	uint32_t proxied_id;
};

/* server-wide context information for the dcerpc server */
struct dcesrv_context {
	/* the list of endpoints that have registered 
	 * by the configured endpoint servers 
	 */
	struct dcesrv_endpoint {
		struct dcesrv_endpoint *next, *prev;
		/* the type and location of the endpoint */
		struct dcerpc_binding *ep_description;
		/* the security descriptor for smb named pipes */
		struct security_descriptor *sd;
		/* the list of interfaces available on this endpoint */
		struct dcesrv_if_list {
			struct dcesrv_if_list *next, *prev;
			struct dcesrv_interface iface;
		} *interface_list;
	} *endpoint_list;

	/* loadparm context to use for this connection */
	struct loadparm_context *lp_ctx;

	struct idr_context *assoc_groups_idr;
};

/* this structure is used by modules to determine the size of some critical types */
struct dcesrv_critical_sizes {
	int interface_version;
	int sizeof_dcesrv_context;
	int sizeof_dcesrv_endpoint;
	int sizeof_dcesrv_endpoint_server;
	int sizeof_dcesrv_interface;
	int sizeof_dcesrv_if_list;
	int sizeof_dcesrv_connection;
	int sizeof_dcesrv_call_state;
	int sizeof_dcesrv_auth;
	int sizeof_dcesrv_handle;
};

struct model_ops;

NTSTATUS dcesrv_interface_register(struct dcesrv_context *dce_ctx,
				   const char *ep_name,
				   const struct dcesrv_interface *iface,
				   const struct security_descriptor *sd);
NTSTATUS dcerpc_register_ep_server(const void *_ep_server);
NTSTATUS dcesrv_init_context(TALLOC_CTX *mem_ctx, 
				      struct loadparm_context *lp_ctx,
				      const char **endpoint_servers, struct dcesrv_context **_dce_ctx);
NTSTATUS dcesrv_endpoint_connect(struct dcesrv_context *dce_ctx,
				 TALLOC_CTX *mem_ctx,
				 const struct dcesrv_endpoint *ep,
				 struct auth_session_info *session_info,
				 struct tevent_context *event_ctx,
				 struct messaging_context *msg_ctx,
				 struct server_id server_id,
				 uint32_t state_flags,
				 struct dcesrv_connection **_p);

NTSTATUS dcesrv_reply(struct dcesrv_call_state *call);
struct dcesrv_handle *dcesrv_handle_new(struct dcesrv_connection_context *context, 
					uint8_t handle_type);

struct dcesrv_handle *dcesrv_handle_fetch(
					  struct dcesrv_connection_context *context, 
					  struct policy_handle *p,
					  uint8_t handle_type);
struct socket_address *dcesrv_connection_get_my_addr(struct dcesrv_connection *conn, TALLOC_CTX *mem_ctx);

struct socket_address *dcesrv_connection_get_peer_addr(struct dcesrv_connection *conn, TALLOC_CTX *mem_ctx);
const struct tsocket_address *dcesrv_connection_get_local_address(struct dcesrv_connection *conn);
const struct tsocket_address *dcesrv_connection_get_remote_address(struct dcesrv_connection *conn);

NTSTATUS dcesrv_fetch_session_key(struct dcesrv_connection *p, DATA_BLOB *session_key);

/* a useful macro for generating a RPC fault in the backend code */
#define DCESRV_FAULT(code) do { \
	dce_call->fault_code = code; \
	return r->out.result; \
} while(0)

/* a useful macro for generating a RPC fault in the backend code */
#define DCESRV_FAULT_VOID(code) do { \
	dce_call->fault_code = code; \
	return; \
} while(0)

/* a useful macro for checking the validity of a dcerpc policy handle
   and giving the right fault code if invalid */
#define DCESRV_CHECK_HANDLE(h) do {if (!(h)) DCESRV_FAULT(DCERPC_FAULT_CONTEXT_MISMATCH); } while (0)

/* this checks for a valid policy handle, and gives a fault if an
   invalid handle or retval if the handle is of the
   wrong type */
#define DCESRV_PULL_HANDLE_RETVAL(h, inhandle, t, retval) do { \
	(h) = dcesrv_handle_fetch(dce_call->context, (inhandle), DCESRV_HANDLE_ANY); \
	DCESRV_CHECK_HANDLE(h); \
	if ((t) != DCESRV_HANDLE_ANY && (h)->wire_handle.handle_type != (t)) { \
		return retval; \
	} \
} while (0)

/* this checks for a valid policy handle and gives a dcerpc fault 
   if its the wrong type of handle */
#define DCESRV_PULL_HANDLE_FAULT(h, inhandle, t) do { \
	(h) = dcesrv_handle_fetch(dce_call->context, (inhandle), t); \
	DCESRV_CHECK_HANDLE(h); \
} while (0)

#define DCESRV_PULL_HANDLE(h, inhandle, t) DCESRV_PULL_HANDLE_RETVAL(h, inhandle, t, NT_STATUS_INVALID_HANDLE)
#define DCESRV_PULL_HANDLE_WERR(h, inhandle, t) DCESRV_PULL_HANDLE_RETVAL(h, inhandle, t, WERR_BADFID)

NTSTATUS dcesrv_add_ep(struct dcesrv_context *dce_ctx,
		       struct loadparm_context *lp_ctx,
		       struct dcesrv_endpoint *e,
		       struct tevent_context *event_ctx,
		       const struct model_ops *model_ops);

/**
 * retrieve credentials from a dce_call
 */
_PUBLIC_ struct cli_credentials *dcesrv_call_credentials(struct dcesrv_call_state *dce_call);

/**
 * returns true if this is an authenticated call
 */
_PUBLIC_ bool dcesrv_call_authenticated(struct dcesrv_call_state *dce_call);

/**
 * retrieve account_name for a dce_call
 */
_PUBLIC_ const char *dcesrv_call_account_name(struct dcesrv_call_state *dce_call);


#endif /* SAMBA_DCERPC_SERVER_H */
