/* 
   Unix SMB/CIFS implementation.
   
   Copyright (C) Andrew Tridgell              2003
   Copyright (C) James J Myers 		      2003 <myersjj@samba.org>
   Copyright (C) Stefan Metzmacher            2004-2005
   
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

#include "libcli/raw/request.h"
#include "libcli/raw/interfaces.h"
#include "lib/socket/socket.h"
#include "../lib/util/dlinklist.h"

struct tevent_context;

/*
  this header declares the core context structures associated with smb
  sockets, tree connects, requests etc

  the idea is that we will eventually get rid of all our global
  variables and instead store our state from structures hanging off
  these basic elements
*/

struct smbsrv_tcons_context {
	/* an id tree used to allocate tids */
	struct idr_context *idtree_tid;

	/* this is the limit of vuid values for this connection */
	uint32_t idtree_limit;

	/* list of open tree connects */
	struct smbsrv_tcon *list;
};

struct smbsrv_sessions_context {
	/* an id tree used to allocate vuids */
	/* this holds info on session vuids that are already
	 * validated for this VC */
	struct idr_context *idtree_vuid;

	/* this is the limit of vuid values for this connection */
	uint64_t idtree_limit;

	/* also kept as a link list so it can be enumerated by
	   the management code */
	struct smbsrv_session *list;
};

struct smbsrv_handles_context {
	/* an id tree used to allocate file handles */
	struct idr_context *idtree_hid;

	/* this is the limit of handle values for this context */
	uint64_t idtree_limit;

	/* also kept as a link list so it can be enumerated by
	   the management code */
	struct smbsrv_handle *list;
};

/* the current user context for a request */
struct smbsrv_session {
	struct smbsrv_session *prev, *next;

	struct smbsrv_connection *smb_conn;

	/*
	 * in SMB2 tcons belong to just one session
	 * and not to the whole connection
	 */
	struct smbsrv_tcons_context smb2_tcons;

	/*
	 * the open file handles for this session,
	 * used for SMBexit, SMBulogoff and SMB2 SessionLogoff
	 */
	struct smbsrv_handle_session_item *handles;

	/* 
	 * an index passed over the wire:
	 * - 16 bit for smb
	 * - 64 bit for smb2
	 */
	uint64_t vuid;

	struct gensec_security *gensec_ctx;

	struct auth_session_info *session_info;

	struct {
		bool required;
		bool active;
	} smb2_signing;

	/* some statistics for the management tools */
	struct {
		/* the time when the session setup started */
		struct timeval connect_time;
		/* the time when the session setup was finished */
		struct timeval auth_time;
		/* the time when the last request comes in */
		struct timeval last_request_time;
	} statistics;
};

/* we need a forward declaration of the ntvfs_ops strucutre to prevent
   include recursion */
struct ntvfs_context;

struct smbsrv_tcon {
	struct smbsrv_tcon *next, *prev;

	/* the server context that this was created on */
	struct smbsrv_connection *smb_conn;

	/* the open file handles on this tcon */
	struct smbsrv_handles_context handles;

	/* 
	 * an index passed over the wire:
	 * - 16 bit for smb
	 * - 32 bit for smb2
	 */
	uint32_t tid; /* an index passed over the wire (the TID) */

	/* the share name */
	const char *share_name;

	/* the NTVFS context - see source/ntvfs/ for details */
	struct ntvfs_context *ntvfs;

	/* some stuff to support share level security */
	struct {
		/* in share level security we need to fake up a session */
		struct smbsrv_session *session;
	} sec_share;

	/* some stuff to support share level security */
	struct {
		/* in SMB2 a tcon always belongs to one session */
		struct smbsrv_session *session;
	} smb2;

	/* some statistics for the management tools */
	struct {
		/* the time when the tree connect started */
		struct timeval connect_time;
		/* the time when the last request comes in */
		struct timeval last_request_time;
	} statistics;
};

struct smbsrv_handle {
	struct smbsrv_handle *next, *prev;

	/* the tcon the handle belongs to */
	struct smbsrv_tcon *tcon;

	/* the session the handle was opened on */
	struct smbsrv_session *session;

	/* the smbpid used on the open, used for SMBexit */
	uint16_t smbpid;

	/*
	 * this is for adding the handle into a linked list
	 * on the smbsrv_session, we can't use *next,*prev
	 * for this because they're used for the linked list on the 
	 * smbsrv_tcon
	 */
	struct smbsrv_handle_session_item {
		struct smbsrv_handle_session_item *prev, *next;
		struct smbsrv_handle *handle;
	} session_item;

	/*
	 * the value passed over the wire
	 * - 16 bit for smb
	 * - 32 bit for smb2
	 *   Note: for SMB2 handles are 128 bit
	 *         we'll fill them with
	 *	   - 32 bit HID
	 *         - 32 bit TID
	 *	   - 64 bit VUID
	 */
	uint32_t hid;

	/*
	 * the ntvfs handle passed to the ntvfs backend
	 */
	struct ntvfs_handle *ntvfs;

	/* some statistics for the management tools */
	struct {
		/* the time when the tree connect started */
		struct timeval open_time;
		/* the time when the last request comes in */
		struct timeval last_use_time;
	} statistics;
};

/* a set of flags to control handling of request structures */
#define SMBSRV_REQ_CONTROL_LARGE     (1<<1) /* allow replies larger than max_xmit */

#define SMBSRV_REQ_DEFAULT_STR_FLAGS(req) (((req)->flags2 & FLAGS2_UNICODE_STRINGS) ? STR_UNICODE : STR_ASCII)

/* the context for a single SMB request. This is passed to any request-context 
   functions */
struct smbsrv_request {
	/* the smbsrv_connection needs a list of requests queued for send */
	struct smbsrv_request *next, *prev;

	/* the server_context contains all context specific to this SMB socket */
	struct smbsrv_connection *smb_conn;

	/* conn is only set for operations that have a valid TID */
	struct smbsrv_tcon *tcon;

	/* the session context is derived from the vuid */
	struct smbsrv_session *session;

	/* a set of flags to control usage of the request. See SMBSRV_REQ_CONTROL_* */
	uint32_t control_flags;

	/* the system time when the request arrived */
	struct timeval request_time;

	/* a pointer to the per request union smb_* io structure */
	void *io_ptr;

	/* the ntvfs_request */
	struct ntvfs_request *ntvfs;

	/* Now the SMB specific stuff */

	/* the flags from the SMB request, in raw form (host byte order) */
	uint16_t flags2;

	/* this can contain a fnum from an earlier part of a chained
	 * message (such as an SMBOpenX), or -1 */
	int chained_fnum;

	/* how far through the chain of SMB commands have we gone? */
	unsigned chain_count;

	/* the sequence number for signing */
	uint64_t seq_num;

	struct smb_request_buffer in;
	struct smb_request_buffer out;
};

enum security_types {SEC_SHARE,SEC_USER};

/* smb server context structure. This should contain all the state
 * information associated with a SMB server connection 
 */
struct smbsrv_connection {
	/* context that has been negotiated between the client and server */
	struct {
		/* have we already done the NBT session establishment? */
		bool done_nbt_session;
	
		/* only one negprot per connection is allowed */
		bool done_negprot;
	
		/* multiple session setups are allowed, but some parameters are
		   ignored in any but the first */
		bool done_sesssetup;
		
		/* 
		 * Size of data we can send to client. Set
		 *  by the client for all protocols above CORE.
		 *  Set by us for CORE protocol.
		 */
		unsigned max_send; /* init to BUFFER_SIZE */
	
		/*
		 * Size of the data we can receive. Set by us.
		 * Can be modified by the max xmit parameter.
		 */
		unsigned max_recv; /* init to BUFFER_SIZE */
	
		/* the negotiatiated protocol */
		enum protocol_types protocol;

		/* authentication context for multi-part negprot */
		struct auth_context *auth_context;
	
		/* reference to the kerberos keytab, or machine trust account */
		struct cli_credentials *server_credentials;
	
		/* did we tell the client we support encrypted passwords? */
		bool encrypted_passwords;
	
		/* Did we choose SPNEGO, or perhaps raw NTLMSSP, or even no extended security at all? */
		const char *oid;
	
		/* client capabilities */
		uint32_t client_caps;
	
		/* the timezone we sent to the client */
		int zone_offset;

		/* NBT names only set when done_nbt_session is true */
		struct nbt_name *called_name;
		struct nbt_name *calling_name;
	} negotiate;

	/* the context associated with open tree connects on a smb socket, not for SMB2 */
	struct smbsrv_tcons_context smb_tcons;

	/* context associated with currently valid session setups */
	struct smbsrv_sessions_context sessions;

	/*
	 * the server_context holds a linked list of pending requests,
	 * this is used for finding the request structures on ntcancel requests
	 * For SMB only
	 */
	struct smbsrv_request *requests;

	/*
	 * the server_context holds a linked list of pending requests,
	 * and an idtree for finding the request structures on SMB2 Cancel
	 * For SMB2 only
	 */
	struct {
		/* an id tree used to allocate ids */
		struct idr_context *idtree_req;

		/* this is the limit of pending requests values for this connection */
		uint32_t idtree_limit;

		/* list of open tree connects */
		struct smb2srv_request *list;
	} requests2;

	struct smb_signing_context signing;

	struct stream_connection *connection;

	/* this holds a partially received request */
	struct packet_context *packet;

	/* a list of partially received transaction requests */
	struct smbsrv_trans_partial {
		struct smbsrv_trans_partial *next, *prev;
		struct smbsrv_request *req;
		uint8_t command;
		union {
			struct smb_trans2 *trans;
			struct smb_nttrans *nttrans;
		} u;
	} *trans_partial;

	/* configuration parameters */
	struct {
		enum security_types security;
		bool nt_status_support;
	} config;

	/* some statictics for the management tools */
	struct {
		/* the time when the client connects */
		struct timeval connect_time;
		/* the time when the last request comes in */
		struct timeval last_request_time;
	} statistics;

	struct share_context *share_context;

	struct loadparm_context *lp_ctx;

	bool smb2_signing_required;

	uint64_t highest_smb2_seqnum;
};

struct model_ops;
struct loadparm_context;

NTSTATUS smbsrv_add_socket(struct tevent_context *event_context,
			   struct loadparm_context *lp_ctx,
			       const struct model_ops *model_ops,
			       const char *address);

struct loadparm_context;

#include "smb_server/smb_server_proto.h"
#include "smb_server/smb/smb_proto.h"

/* useful way of catching wct errors with file and line number */
#define SMBSRV_CHECK_WCT(req, wcount) do { \
	if ((req)->in.wct != (wcount)) { \
		DEBUG(1,("Unexpected WCT %u at %s(%d) - expected %d\n", \
			 (req)->in.wct, __FILE__, __LINE__, wcount)); \
		smbsrv_send_error(req, NT_STATUS_DOS(ERRSRV, ERRerror)); \
		return; \
	} \
} while (0)
	
/* useful wrapper for talloc with NO_MEMORY reply */
#define SMBSRV_TALLOC_IO_PTR(ptr, type) do { \
	ptr = talloc(req, type); \
	if (!ptr) { \
		smbsrv_send_error(req, NT_STATUS_NO_MEMORY); \
		return; \
	} \
	req->io_ptr = ptr; \
} while (0)

#define SMBSRV_SETUP_NTVFS_REQUEST(send_fn, state) do { \
	req->ntvfs = ntvfs_request_create(req->tcon->ntvfs, req, \
					  req->session->session_info,\
					  SVAL(req->in.hdr,HDR_PID), \
					  req->request_time, \
					  req, send_fn, state); \
	if (!req->ntvfs) { \
		smbsrv_send_error(req, NT_STATUS_NO_MEMORY); \
		return; \
	} \
	(void)talloc_steal(req->tcon->ntvfs, req); \
	req->ntvfs->frontend_data.private_data = req; \
} while (0)

#define SMBSRV_CHECK_FILE_HANDLE(handle) do { \
	if (!handle) { \
		smbsrv_send_error(req, NT_STATUS_INVALID_HANDLE); \
		return; \
	} \
} while (0)

#define SMBSRV_CHECK_FILE_HANDLE_ERROR(handle, _status) do { \
	if (!handle) { \
		smbsrv_send_error(req, _status); \
		return; \
	} \
} while (0)

#define SMBSRV_CHECK_FILE_HANDLE_NTSTATUS(handle) do { \
	if (!handle) { \
		return NT_STATUS_INVALID_HANDLE; \
	} \
} while (0)

#define SMBSRV_CHECK(cmd) do {\
	NTSTATUS _status; \
	_status = cmd; \
	if (!NT_STATUS_IS_OK(_status)) { \
		smbsrv_send_error(req,  _status); \
		return; \
	} \
} while (0)

/* 
   check if the backend wants to handle the request asynchronously.
   if it wants it handled synchronously then call the send function
   immediately
*/
#define SMBSRV_CALL_NTVFS_BACKEND(cmd) do { \
	req->ntvfs->async_states->status = cmd; \
	if (req->ntvfs->async_states->state & NTVFS_ASYNC_STATE_ASYNC) { \
		DLIST_ADD_END(req->smb_conn->requests, req, struct smbsrv_request *); \
	} else { \
		req->ntvfs->async_states->send_fn(req->ntvfs); \
	} \
} while (0)

/* check req->ntvfs->async_states->status and if not OK then send an error reply */
#define SMBSRV_CHECK_ASYNC_STATUS_ERR_SIMPLE do { \
	req = talloc_get_type(ntvfs->async_states->private_data, struct smbsrv_request); \
	if (ntvfs->async_states->state & NTVFS_ASYNC_STATE_CLOSE || NT_STATUS_EQUAL(ntvfs->async_states->status, NT_STATUS_NET_WRITE_FAULT)) { \
		smbsrv_terminate_connection(req->smb_conn, get_friendly_nt_error_msg (ntvfs->async_states->status)); \
		talloc_free(req); \
		return; \
	} \
	if (NT_STATUS_IS_ERR(ntvfs->async_states->status)) { \
		smbsrv_send_error(req, ntvfs->async_states->status); \
		return; \
	} \
} while (0)
#define SMBSRV_CHECK_ASYNC_STATUS_ERR(ptr, type) do { \
	SMBSRV_CHECK_ASYNC_STATUS_ERR_SIMPLE; \
	ptr = talloc_get_type(req->io_ptr, type); \
} while (0)
#define SMBSRV_CHECK_ASYNC_STATUS_SIMPLE do { \
	req = talloc_get_type(ntvfs->async_states->private_data, struct smbsrv_request); \
	if (ntvfs->async_states->state & NTVFS_ASYNC_STATE_CLOSE || NT_STATUS_EQUAL(ntvfs->async_states->status, NT_STATUS_NET_WRITE_FAULT)) { \
		smbsrv_terminate_connection(req->smb_conn, get_friendly_nt_error_msg (ntvfs->async_states->status)); \
		talloc_free(req); \
		return; \
	} \
	if (!NT_STATUS_IS_OK(ntvfs->async_states->status)) { \
		smbsrv_send_error(req, ntvfs->async_states->status); \
		return; \
	} \
} while (0)
#define SMBSRV_CHECK_ASYNC_STATUS(ptr, type) do { \
	SMBSRV_CHECK_ASYNC_STATUS_SIMPLE; \
	ptr = talloc_get_type(req->io_ptr, type); \
} while (0)

/* zero out some reserved fields in a reply */
#define SMBSRV_VWV_RESERVED(start, count) memset(req->out.vwv + VWV(start), 0, (count)*2)

#include "smb_server/service_smb_proto.h"
