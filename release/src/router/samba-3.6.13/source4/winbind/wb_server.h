/* 
   Unix SMB/CIFS implementation.
   Main winbindd server routines

   Copyright (C) Stefan Metzmacher	2005
   Copyright (C) Andrew Tridgell	2005
   
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

#include "nsswitch/winbind_nss_config.h"
#include "nsswitch/winbind_struct_protocol.h"
#include "winbind/idmap.h"
#include "libnet/libnet.h"

/* this struct stores global data for the winbind task */
struct wbsrv_service {
	struct task_server *task;

	const struct dom_sid *primary_sid;
	enum netr_SchannelType sec_channel_type;
	struct wbsrv_domain *domains;
	struct idmap_context *idmap_ctx;
	const char *priv_pipe_dir;
	const char *pipe_dir;
};

struct wbsrv_samconn {
	struct wbsrv_domain *domain;
	void *private_data;

	struct composite_context (*seqnum_send)(struct wbsrv_samconn *);
	NTSTATUS (*seqnum_recv)(struct composite_context *, uint64_t *);
};

struct wb_dom_info {
	const char *name;
	const char *dns_name;
	const struct dom_sid *sid;
	struct nbt_dc_name *dc;
};

struct wbsrv_domain {
	struct wbsrv_domain *next, *prev;

	struct wb_dom_info *info;

	/* Details for the server we are currently talking to */
	const char *dc_address;
	const char *dc_name;

	struct libnet_context *libnet_ctx;

	struct dcerpc_binding *lsa_binding;

	struct dcerpc_binding *samr_binding;

	struct dcerpc_pipe *netlogon_pipe;
	struct dcerpc_binding *netlogon_binding;
};

/*
  state of a listen socket and it's protocol information
*/
struct wbsrv_listen_socket {
	const char *socket_path;
	struct wbsrv_service *service;
	bool privileged;
};

/*
  state of an open winbind connection
*/
struct wbsrv_connection {
	/* stream connection we belong to */
	struct stream_connection *conn;

	/* the listening socket we belong to, it holds protocol hooks */
	struct wbsrv_listen_socket *listen_socket;

	/* storage for protocol specific data */
	void *protocol_private_data;

	/* how many calls are pending */
	uint32_t pending_calls;

	struct tstream_context *tstream;

	struct tevent_queue *send_queue;

	struct loadparm_context *lp_ctx;
};

#define WBSRV_SAMBA3_SET_STRING(dest, src) do { \
	memset(dest, 0, sizeof(dest));\
	safe_strcpy(dest, src, sizeof(dest)-1);\
} while(0)

/*
  state of a pwent query
*/
struct wbsrv_pwent {
	/* Current UserList structure, contains 1+ user structs */
	struct libnet_UserList *user_list;

	/* Index of the next user struct in the current UserList struct */
	uint32_t page_index;

	/* The libnet_ctx to use for the libnet_UserList call */
	struct libnet_context *libnet_ctx;
};
/*
  state of a grent query
*/
struct wbsrv_grent {
	/* Current UserList structure, contains 1+ user structs */
	struct libnet_GroupList *group_list;

	/* Index of the next user struct in the current UserList struct */
	uint32_t page_index;

	/* The libnet_ctx to use for the libnet_UserList call */
	struct libnet_context *libnet_ctx;
};

/*
  state of one request

  NOTE about async replies:
   if the backend wants to reply later:

   - it should set the WBSRV_CALL_FLAGS_REPLY_ASYNC flag, and may set a 
     talloc_destructor on the this structure or on the private_data (if it's a
     talloc child of this structure), so that wbsrv_terminate_connection
     called by another call clean up the whole connection correct.
   - When the backend is ready to reply it should call wbsrv_send_reply(call),
     wbsrv_send_reply implies talloc_free(call), so the backend should use 
     talloc_reference(call), if it needs it later. 
   - If wbsrv_send_reply doesn't return NT_STATUS_OK, the backend function 
     should call, wbsrv_terminate_connection(call->wbconn, nt_errstr(status));
     return;

*/
struct wbsrv_samba3_call {
#define WBSRV_CALL_FLAGS_REPLY_ASYNC 0x00000001
	uint32_t flags;

	/* the connection the call belongs to */
	struct wbsrv_connection *wbconn;

	/* here the backend can store stuff like composite_context's ... */
	void *private_data;

	/* the request structure of the samba3 protocol */
	struct winbindd_request *request;
	
	/* the response structure of the samba3 protocol*/
	struct winbindd_response *response;

	DATA_BLOB in;
	DATA_BLOB out;
	struct iovec out_iov[1];
};

struct netr_LMSessionKey;
struct netr_UserSessionKey;
struct winbind_SamLogon;
struct winbind_DsrUpdateReadOnlyServerDnsRecords;

#include "winbind/wb_async_helpers.h"
#include "winbind/wb_proto.h"
