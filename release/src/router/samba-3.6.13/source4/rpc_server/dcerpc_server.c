/* 
   Unix SMB/CIFS implementation.

   server side dcerpc core code

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

#include "includes.h"
#include "auth/auth.h"
#include "auth/gensec/gensec.h"
#include "../lib/util/dlinklist.h"
#include "rpc_server/dcerpc_server.h"
#include "rpc_server/dcerpc_server_proto.h"
#include "rpc_server/common/proto.h"
#include "librpc/rpc/dcerpc_proto.h"
#include "system/filesys.h"
#include "libcli/security/security.h"
#include "param/param.h"
#include "../lib/tsocket/tsocket.h"
#include "../libcli/named_pipe_auth/npa_tstream.h"
#include "smbd/service_stream.h"
#include "../lib/tsocket/tsocket.h"
#include "lib/socket/socket.h"
#include "smbd/process_model.h"
#include "lib/messaging/irpc.h"
#include "librpc/rpc/rpc_common.h"

/* this is only used when the client asks for an unknown interface */
#define DUMMY_ASSOC_GROUP 0x0FFFFFFF

extern const struct dcesrv_interface dcesrv_mgmt_interface;


/*
  find an association group given a assoc_group_id
 */
static struct dcesrv_assoc_group *dcesrv_assoc_group_find(struct dcesrv_context *dce_ctx,
							  uint32_t id)
{
	void *id_ptr;

	id_ptr = idr_find(dce_ctx->assoc_groups_idr, id);
	if (id_ptr == NULL) {
		return NULL;
	}
	return talloc_get_type_abort(id_ptr, struct dcesrv_assoc_group);
}

/*
  take a reference to an existing association group
 */
static struct dcesrv_assoc_group *dcesrv_assoc_group_reference(TALLOC_CTX *mem_ctx,
							       struct dcesrv_context *dce_ctx,
							       uint32_t id)
{
	struct dcesrv_assoc_group *assoc_group;

	assoc_group = dcesrv_assoc_group_find(dce_ctx, id);
	if (assoc_group == NULL) {
		DEBUG(0,(__location__ ": Failed to find assoc_group 0x%08x\n", id));
		return NULL;
	}
	return talloc_reference(mem_ctx, assoc_group);
}

static int dcesrv_assoc_group_destructor(struct dcesrv_assoc_group *assoc_group)
{
	int ret;
	ret = idr_remove(assoc_group->dce_ctx->assoc_groups_idr, assoc_group->id);
	if (ret != 0) {
		DEBUG(0,(__location__ ": Failed to remove assoc_group 0x%08x\n",
			 assoc_group->id));
	}
	return 0;
}

/*
  allocate a new association group
 */
static struct dcesrv_assoc_group *dcesrv_assoc_group_new(TALLOC_CTX *mem_ctx,
							 struct dcesrv_context *dce_ctx)
{
	struct dcesrv_assoc_group *assoc_group;
	int id;

	assoc_group = talloc_zero(mem_ctx, struct dcesrv_assoc_group);
	if (assoc_group == NULL) {
		return NULL;
	}
	
	id = idr_get_new_random(dce_ctx->assoc_groups_idr, assoc_group, UINT16_MAX);
	if (id == -1) {
		talloc_free(assoc_group);
		DEBUG(0,(__location__ ": Out of association groups!\n"));
		return NULL;
	}

	assoc_group->id = id;
	assoc_group->dce_ctx = dce_ctx;

	talloc_set_destructor(assoc_group, dcesrv_assoc_group_destructor);

	return assoc_group;
}


/*
  see if two endpoints match
*/
static bool endpoints_match(const struct dcerpc_binding *ep1,
			    const struct dcerpc_binding *ep2)
{
	if (ep1->transport != ep2->transport) {
		return false;
	}

	if (!ep1->endpoint || !ep2->endpoint) {
		return ep1->endpoint == ep2->endpoint;
	}

	if (strcasecmp(ep1->endpoint, ep2->endpoint) != 0) 
		return false;

	return true;
}

/*
  find an endpoint in the dcesrv_context
*/
static struct dcesrv_endpoint *find_endpoint(struct dcesrv_context *dce_ctx,
					     const struct dcerpc_binding *ep_description)
{
	struct dcesrv_endpoint *ep;
	for (ep=dce_ctx->endpoint_list; ep; ep=ep->next) {
		if (endpoints_match(ep->ep_description, ep_description)) {
			return ep;
		}
	}
	return NULL;
}

/*
  find a registered context_id from a bind or alter_context
*/
static struct dcesrv_connection_context *dcesrv_find_context(struct dcesrv_connection *conn, 
								   uint32_t context_id)
{
	struct dcesrv_connection_context *c;
	for (c=conn->contexts;c;c=c->next) {
		if (c->context_id == context_id) return c;
	}
	return NULL;
}

/*
  see if a uuid and if_version match to an interface
*/
static bool interface_match(const struct dcesrv_interface *if1,
							const struct dcesrv_interface *if2)
{
	return (if1->syntax_id.if_version == if2->syntax_id.if_version && 
			GUID_equal(&if1->syntax_id.uuid, &if2->syntax_id.uuid));
}

/*
  find the interface operations on an endpoint
*/
static const struct dcesrv_interface *find_interface(const struct dcesrv_endpoint *endpoint,
						     const struct dcesrv_interface *iface)
{
	struct dcesrv_if_list *ifl;
	for (ifl=endpoint->interface_list; ifl; ifl=ifl->next) {
		if (interface_match(&(ifl->iface), iface)) {
			return &(ifl->iface);
		}
	}
	return NULL;
}

/*
  see if a uuid and if_version match to an interface
*/
static bool interface_match_by_uuid(const struct dcesrv_interface *iface,
				    const struct GUID *uuid, uint32_t if_version)
{
	return (iface->syntax_id.if_version == if_version && 
			GUID_equal(&iface->syntax_id.uuid, uuid));
}

/*
  find the interface operations on an endpoint by uuid
*/
static const struct dcesrv_interface *find_interface_by_uuid(const struct dcesrv_endpoint *endpoint,
							     const struct GUID *uuid, uint32_t if_version)
{
	struct dcesrv_if_list *ifl;
	for (ifl=endpoint->interface_list; ifl; ifl=ifl->next) {
		if (interface_match_by_uuid(&(ifl->iface), uuid, if_version)) {
			return &(ifl->iface);
		}
	}
	return NULL;
}

/*
  find the earlier parts of a fragmented call awaiting reassembily
*/
static struct dcesrv_call_state *dcesrv_find_fragmented_call(struct dcesrv_connection *dce_conn, uint16_t call_id)
{
	struct dcesrv_call_state *c;
	for (c=dce_conn->incoming_fragmented_call_list;c;c=c->next) {
		if (c->pkt.call_id == call_id) {
			return c;
		}
	}
	return NULL;
}

/*
  register an interface on an endpoint
*/
_PUBLIC_ NTSTATUS dcesrv_interface_register(struct dcesrv_context *dce_ctx,
				   const char *ep_name,
				   const struct dcesrv_interface *iface,
				   const struct security_descriptor *sd)
{
	struct dcesrv_endpoint *ep;
	struct dcesrv_if_list *ifl;
	struct dcerpc_binding *binding;
	bool add_ep = false;
	NTSTATUS status;
	
	status = dcerpc_parse_binding(dce_ctx, ep_name, &binding);

	if (NT_STATUS_IS_ERR(status)) {
		DEBUG(0, ("Trouble parsing binding string '%s'\n", ep_name));
		return status;
	}

	/* check if this endpoint exists
	 */
	if ((ep=find_endpoint(dce_ctx, binding))==NULL) {
		ep = talloc(dce_ctx, struct dcesrv_endpoint);
		if (!ep) {
			return NT_STATUS_NO_MEMORY;
		}
		ZERO_STRUCTP(ep);
		ep->ep_description = talloc_reference(ep, binding);
		add_ep = true;

		/* add mgmt interface */
		ifl = talloc(dce_ctx, struct dcesrv_if_list);
		if (!ifl) {
			return NT_STATUS_NO_MEMORY;
		}

		memcpy(&(ifl->iface), &dcesrv_mgmt_interface, 
			   sizeof(struct dcesrv_interface));

		DLIST_ADD(ep->interface_list, ifl);
	}

	/* see if the interface is already registered on te endpoint */
	if (find_interface(ep, iface)!=NULL) {
		DEBUG(0,("dcesrv_interface_register: interface '%s' already registered on endpoint '%s'\n",
			iface->name, ep_name));
		return NT_STATUS_OBJECT_NAME_COLLISION;
	}

	/* talloc a new interface list element */
	ifl = talloc(dce_ctx, struct dcesrv_if_list);
	if (!ifl) {
		return NT_STATUS_NO_MEMORY;
	}

	/* copy the given interface struct to the one on the endpoints interface list */
	memcpy(&(ifl->iface),iface, sizeof(struct dcesrv_interface));

	/* if we have a security descriptor given,
	 * we should see if we can set it up on the endpoint
	 */
	if (sd != NULL) {
		/* if there's currently no security descriptor given on the endpoint
		 * we try to set it
		 */
		if (ep->sd == NULL) {
			ep->sd = security_descriptor_copy(dce_ctx, sd);
		}

		/* if now there's no security descriptor given on the endpoint
		 * something goes wrong, either we failed to copy the security descriptor
		 * or there was already one on the endpoint
		 */
		if (ep->sd != NULL) {
			DEBUG(0,("dcesrv_interface_register: interface '%s' failed to setup a security descriptor\n"
			         "                           on endpoint '%s'\n",
				iface->name, ep_name));
			if (add_ep) free(ep);
			free(ifl);
			return NT_STATUS_OBJECT_NAME_COLLISION;
		}
	}

	/* finally add the interface on the endpoint */
	DLIST_ADD(ep->interface_list, ifl);

	/* if it's a new endpoint add it to the dcesrv_context */
	if (add_ep) {
		DLIST_ADD(dce_ctx->endpoint_list, ep);
	}

	DEBUG(4,("dcesrv_interface_register: interface '%s' registered on endpoint '%s'\n",
		iface->name, ep_name));

	return NT_STATUS_OK;
}

NTSTATUS dcesrv_inherited_session_key(struct dcesrv_connection *p,
				      DATA_BLOB *session_key)
{
	if (p->auth_state.session_info->session_key.length) {
		*session_key = p->auth_state.session_info->session_key;
		return NT_STATUS_OK;
	}
	return NT_STATUS_NO_USER_SESSION_KEY;
}

/*
  fetch the user session key - may be default (above) or the SMB session key

  The key is always truncated to 16 bytes 
*/
_PUBLIC_ NTSTATUS dcesrv_fetch_session_key(struct dcesrv_connection *p,
				  DATA_BLOB *session_key)
{
	NTSTATUS status = p->auth_state.session_key(p, session_key);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	session_key->length = MIN(session_key->length, 16);

	return NT_STATUS_OK;
}

/*
  connect to a dcerpc endpoint
*/
_PUBLIC_ NTSTATUS dcesrv_endpoint_connect(struct dcesrv_context *dce_ctx,
				 TALLOC_CTX *mem_ctx,
				 const struct dcesrv_endpoint *ep,
				 struct auth_session_info *session_info,
				 struct tevent_context *event_ctx,
				 struct messaging_context *msg_ctx,
				 struct server_id server_id,
				 uint32_t state_flags,
				 struct dcesrv_connection **_p)
{
	struct dcesrv_connection *p;

	if (!session_info) {
		return NT_STATUS_ACCESS_DENIED;
	}

	p = talloc(mem_ctx, struct dcesrv_connection);
	NT_STATUS_HAVE_NO_MEMORY(p);

	if (!talloc_reference(p, session_info)) {
		talloc_free(p);
		return NT_STATUS_NO_MEMORY;
	}

	p->dce_ctx = dce_ctx;
	p->endpoint = ep;
	p->contexts = NULL;
	p->call_list = NULL;
	p->packet_log_dir = lpcfg_lockdir(dce_ctx->lp_ctx);
	p->incoming_fragmented_call_list = NULL;
	p->pending_call_list = NULL;
	p->cli_max_recv_frag = 0;
	p->partial_input = data_blob(NULL, 0);
	p->auth_state.auth_info = NULL;
	p->auth_state.gensec_security = NULL;
	p->auth_state.session_info = session_info;
	p->auth_state.session_key = dcesrv_generic_session_key;
	p->event_ctx = event_ctx;
	p->msg_ctx = msg_ctx;
	p->server_id = server_id;
	p->processing = false;
	p->state_flags = state_flags;
	ZERO_STRUCT(p->transport);

	*_p = p;
	return NT_STATUS_OK;
}

/*
  move a call from an existing linked list to the specified list. This
  prevents bugs where we forget to remove the call from a previous
  list when moving it.
 */
static void dcesrv_call_set_list(struct dcesrv_call_state *call, 
				 enum dcesrv_call_list list)
{
	switch (call->list) {
	case DCESRV_LIST_NONE:
		break;
	case DCESRV_LIST_CALL_LIST:
		DLIST_REMOVE(call->conn->call_list, call);
		break;
	case DCESRV_LIST_FRAGMENTED_CALL_LIST:
		DLIST_REMOVE(call->conn->incoming_fragmented_call_list, call);
		break;
	case DCESRV_LIST_PENDING_CALL_LIST:
		DLIST_REMOVE(call->conn->pending_call_list, call);
		break;
	}
	call->list = list;
	switch (list) {
	case DCESRV_LIST_NONE:
		break;
	case DCESRV_LIST_CALL_LIST:
		DLIST_ADD_END(call->conn->call_list, call, struct dcesrv_call_state *);
		break;
	case DCESRV_LIST_FRAGMENTED_CALL_LIST:
		DLIST_ADD_END(call->conn->incoming_fragmented_call_list, call, struct dcesrv_call_state *);
		break;
	case DCESRV_LIST_PENDING_CALL_LIST:
		DLIST_ADD_END(call->conn->pending_call_list, call, struct dcesrv_call_state *);
		break;
	}
}


/*
  return a dcerpc bind_nak
*/
static NTSTATUS dcesrv_bind_nak(struct dcesrv_call_state *call, uint32_t reason)
{
	struct ncacn_packet pkt;
	struct data_blob_list_item *rep;
	NTSTATUS status;

	/* setup a bind_nak */
	dcesrv_init_hdr(&pkt, lpcfg_rpc_big_endian(call->conn->dce_ctx->lp_ctx));
	pkt.auth_length = 0;
	pkt.call_id = call->pkt.call_id;
	pkt.ptype = DCERPC_PKT_BIND_NAK;
	pkt.pfc_flags = DCERPC_PFC_FLAG_FIRST | DCERPC_PFC_FLAG_LAST;
	pkt.u.bind_nak.reject_reason = reason;
	if (pkt.u.bind_nak.reject_reason == DECRPC_BIND_PROTOCOL_VERSION_NOT_SUPPORTED) {
		pkt.u.bind_nak.versions.v.num_versions = 0;
	}

	rep = talloc(call, struct data_blob_list_item);
	if (!rep) {
		return NT_STATUS_NO_MEMORY;
	}

	status = ncacn_push_auth(&rep->blob, call, &pkt, NULL);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	dcerpc_set_frag_length(&rep->blob, rep->blob.length);

	DLIST_ADD_END(call->replies, rep, struct data_blob_list_item *);
	dcesrv_call_set_list(call, DCESRV_LIST_CALL_LIST);

	if (call->conn->call_list && call->conn->call_list->replies) {
		if (call->conn->transport.report_output_data) {
			call->conn->transport.report_output_data(call->conn);
		}
	}

	return NT_STATUS_OK;	
}

static int dcesrv_connection_context_destructor(struct dcesrv_connection_context *c)
{
	DLIST_REMOVE(c->conn->contexts, c);

	if (c->iface && c->iface->unbind) {
		c->iface->unbind(c, c->iface);
	}

	return 0;
}

/*
  handle a bind request
*/
static NTSTATUS dcesrv_bind(struct dcesrv_call_state *call)
{
	uint32_t if_version, transfer_syntax_version;
	struct GUID uuid, *transfer_syntax_uuid;
	struct ncacn_packet pkt;
	struct data_blob_list_item *rep;
	NTSTATUS status;
	uint32_t result=0, reason=0;
	uint32_t context_id;
	const struct dcesrv_interface *iface;
	uint32_t extra_flags = 0;

	/*
	  if provided, check the assoc_group is valid
	 */
	if (call->pkt.u.bind.assoc_group_id != 0 &&
	    lpcfg_parm_bool(call->conn->dce_ctx->lp_ctx, NULL, "dcesrv","assoc group checking", true) &&
	    dcesrv_assoc_group_find(call->conn->dce_ctx, call->pkt.u.bind.assoc_group_id) == NULL) {
		return dcesrv_bind_nak(call, 0);	
	}

	if (call->pkt.u.bind.num_contexts < 1 ||
	    call->pkt.u.bind.ctx_list[0].num_transfer_syntaxes < 1) {
		return dcesrv_bind_nak(call, 0);
	}

	context_id = call->pkt.u.bind.ctx_list[0].context_id;

	/* you can't bind twice on one context */
	if (dcesrv_find_context(call->conn, context_id) != NULL) {
		return dcesrv_bind_nak(call, 0);
	}

	if_version = call->pkt.u.bind.ctx_list[0].abstract_syntax.if_version;
	uuid = call->pkt.u.bind.ctx_list[0].abstract_syntax.uuid;

	transfer_syntax_version = call->pkt.u.bind.ctx_list[0].transfer_syntaxes[0].if_version;
	transfer_syntax_uuid = &call->pkt.u.bind.ctx_list[0].transfer_syntaxes[0].uuid;
	if (!GUID_equal(&ndr_transfer_syntax.uuid, transfer_syntax_uuid) != 0 ||
	    ndr_transfer_syntax.if_version != transfer_syntax_version) {
		char *uuid_str = GUID_string(call, transfer_syntax_uuid);
		/* we only do NDR encoded dcerpc */
		DEBUG(0,("Non NDR transfer syntax requested - %s\n", uuid_str));
		talloc_free(uuid_str);
		return dcesrv_bind_nak(call, 0);
	}

	iface = find_interface_by_uuid(call->conn->endpoint, &uuid, if_version);
	if (iface == NULL) {
		char *uuid_str = GUID_string(call, &uuid);
		DEBUG(2,("Request for unknown dcerpc interface %s/%d\n", uuid_str, if_version));
		talloc_free(uuid_str);

		/* we don't know about that interface */
		result = DCERPC_BIND_PROVIDER_REJECT;
		reason = DCERPC_BIND_REASON_ASYNTAX;		
	}

	if (iface) {
		/* add this context to the list of available context_ids */
		struct dcesrv_connection_context *context = talloc(call->conn, 
								   struct dcesrv_connection_context);
		if (context == NULL) {
			return dcesrv_bind_nak(call, 0);
		}
		context->conn = call->conn;
		context->iface = iface;
		context->context_id = context_id;
		if (call->pkt.u.bind.assoc_group_id != 0) {
			context->assoc_group = dcesrv_assoc_group_reference(context,
									    call->conn->dce_ctx, 
									    call->pkt.u.bind.assoc_group_id);
		} else {
			context->assoc_group = dcesrv_assoc_group_new(context, call->conn->dce_ctx);
		}
		if (context->assoc_group == NULL) {
			talloc_free(context);
			return dcesrv_bind_nak(call, 0);
		}
		context->private_data = NULL;
		DLIST_ADD(call->conn->contexts, context);
		call->context = context;
		talloc_set_destructor(context, dcesrv_connection_context_destructor);

		status = iface->bind(call, iface, if_version);
		if (!NT_STATUS_IS_OK(status)) {
			char *uuid_str = GUID_string(call, &uuid);
			DEBUG(2,("Request for dcerpc interface %s/%d rejected: %s\n",
				 uuid_str, if_version, nt_errstr(status)));
			talloc_free(uuid_str);
			/* we don't want to trigger the iface->unbind() hook */
			context->iface = NULL;
			talloc_free(call->context);
			call->context = NULL;
			return dcesrv_bind_nak(call, 0);
		}
	}

	if (call->conn->cli_max_recv_frag == 0) {
		call->conn->cli_max_recv_frag = MIN(0x2000, call->pkt.u.bind.max_recv_frag);
	}

	if ((call->pkt.pfc_flags & DCERPC_PFC_FLAG_SUPPORT_HEADER_SIGN) &&
	    lpcfg_parm_bool(call->conn->dce_ctx->lp_ctx, NULL, "dcesrv","header signing", false)) {
		call->conn->state_flags |= DCESRV_CALL_STATE_FLAG_HEADER_SIGNING;
		extra_flags |= DCERPC_PFC_FLAG_SUPPORT_HEADER_SIGN;
	}

	/* handle any authentication that is being requested */
	if (!dcesrv_auth_bind(call)) {
		talloc_free(call->context);
		call->context = NULL;
		return dcesrv_bind_nak(call, DCERPC_BIND_REASON_INVALID_AUTH_TYPE);
	}

	/* setup a bind_ack */
	dcesrv_init_hdr(&pkt, lpcfg_rpc_big_endian(call->conn->dce_ctx->lp_ctx));
	pkt.auth_length = 0;
	pkt.call_id = call->pkt.call_id;
	pkt.ptype = DCERPC_PKT_BIND_ACK;
	pkt.pfc_flags = DCERPC_PFC_FLAG_FIRST | DCERPC_PFC_FLAG_LAST | extra_flags;
	pkt.u.bind_ack.max_xmit_frag = call->conn->cli_max_recv_frag;
	pkt.u.bind_ack.max_recv_frag = 0x2000;

	/*
	  make it possible for iface->bind() to specify the assoc_group_id
	  This helps the openchange mapiproxy plugin to work correctly.
	  
	  metze
	*/
	if (call->context) {
		pkt.u.bind_ack.assoc_group_id = call->context->assoc_group->id;
	} else {
		pkt.u.bind_ack.assoc_group_id = DUMMY_ASSOC_GROUP;
	}

	if (iface) {
		/* FIXME: Use pipe name as specified by endpoint instead of interface name */
		pkt.u.bind_ack.secondary_address = talloc_asprintf(call, "\\PIPE\\%s", iface->name);
	} else {
		pkt.u.bind_ack.secondary_address = "";
	}
	pkt.u.bind_ack.num_results = 1;
	pkt.u.bind_ack.ctx_list = talloc(call, struct dcerpc_ack_ctx);
	if (!pkt.u.bind_ack.ctx_list) {
		talloc_free(call->context);
		call->context = NULL;
		return NT_STATUS_NO_MEMORY;
	}
	pkt.u.bind_ack.ctx_list[0].result = result;
	pkt.u.bind_ack.ctx_list[0].reason = reason;
	pkt.u.bind_ack.ctx_list[0].syntax = ndr_transfer_syntax;
	pkt.u.bind_ack.auth_info = data_blob(NULL, 0);

	status = dcesrv_auth_bind_ack(call, &pkt);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(call->context);
		call->context = NULL;
		return dcesrv_bind_nak(call, 0);
	}

	rep = talloc(call, struct data_blob_list_item);
	if (!rep) {
		talloc_free(call->context);
		call->context = NULL;
		return NT_STATUS_NO_MEMORY;
	}

	status = ncacn_push_auth(&rep->blob, call, &pkt,
							 call->conn->auth_state.auth_info);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(call->context);
		call->context = NULL;
		return status;
	}

	dcerpc_set_frag_length(&rep->blob, rep->blob.length);

	DLIST_ADD_END(call->replies, rep, struct data_blob_list_item *);
	dcesrv_call_set_list(call, DCESRV_LIST_CALL_LIST);

	if (call->conn->call_list && call->conn->call_list->replies) {
		if (call->conn->transport.report_output_data) {
			call->conn->transport.report_output_data(call->conn);
		}
	}

	return NT_STATUS_OK;
}


/*
  handle a auth3 request
*/
static NTSTATUS dcesrv_auth3(struct dcesrv_call_state *call)
{
	/* handle the auth3 in the auth code */
	if (!dcesrv_auth_auth3(call)) {
		return dcesrv_fault(call, DCERPC_FAULT_OTHER);
	}

	talloc_free(call);

	/* we don't send a reply to a auth3 request, except by a
	   fault */
	return NT_STATUS_OK;
}


/*
  handle a bind request
*/
static NTSTATUS dcesrv_alter_new_context(struct dcesrv_call_state *call, uint32_t context_id)
{
	uint32_t if_version, transfer_syntax_version;
	struct dcesrv_connection_context *context;
	const struct dcesrv_interface *iface;
	struct GUID uuid, *transfer_syntax_uuid;
	NTSTATUS status;

	if_version = call->pkt.u.alter.ctx_list[0].abstract_syntax.if_version;
	uuid = call->pkt.u.alter.ctx_list[0].abstract_syntax.uuid;

	transfer_syntax_version = call->pkt.u.alter.ctx_list[0].transfer_syntaxes[0].if_version;
	transfer_syntax_uuid = &call->pkt.u.alter.ctx_list[0].transfer_syntaxes[0].uuid;
	if (!GUID_equal(transfer_syntax_uuid, &ndr_transfer_syntax.uuid) ||
	    ndr_transfer_syntax.if_version != transfer_syntax_version) {
		/* we only do NDR encoded dcerpc */
		return NT_STATUS_RPC_PROTSEQ_NOT_SUPPORTED;
	}

	iface = find_interface_by_uuid(call->conn->endpoint, &uuid, if_version);
	if (iface == NULL) {
		char *uuid_str = GUID_string(call, &uuid);
		DEBUG(2,("Request for unknown dcerpc interface %s/%d\n", uuid_str, if_version));
		talloc_free(uuid_str);
		return NT_STATUS_RPC_PROTSEQ_NOT_SUPPORTED;
	}

	/* add this context to the list of available context_ids */
	context = talloc(call->conn, struct dcesrv_connection_context);
	if (context == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	context->conn = call->conn;
	context->iface = iface;
	context->context_id = context_id;
	if (call->pkt.u.alter.assoc_group_id != 0) {
		context->assoc_group = dcesrv_assoc_group_reference(context,
								    call->conn->dce_ctx, 
								    call->pkt.u.alter.assoc_group_id);
	} else {
		context->assoc_group = dcesrv_assoc_group_new(context, call->conn->dce_ctx);
	}
	if (context->assoc_group == NULL) {
		talloc_free(context);
		call->context = NULL;
		return NT_STATUS_NO_MEMORY;
	}
	context->private_data = NULL;
	DLIST_ADD(call->conn->contexts, context);
	call->context = context;
	talloc_set_destructor(context, dcesrv_connection_context_destructor);

	status = iface->bind(call, iface, if_version);
	if (!NT_STATUS_IS_OK(status)) {
		/* we don't want to trigger the iface->unbind() hook */
		context->iface = NULL;
		talloc_free(context);
		call->context = NULL;
		return status;
	}

	return NT_STATUS_OK;
}


/*
  handle a alter context request
*/
static NTSTATUS dcesrv_alter(struct dcesrv_call_state *call)
{
	struct ncacn_packet pkt;
	struct data_blob_list_item *rep;
	NTSTATUS status;
	uint32_t result=0, reason=0;
	uint32_t context_id;

	/* handle any authentication that is being requested */
	if (!dcesrv_auth_alter(call)) {
		/* TODO: work out the right reject code */
		result = DCERPC_BIND_PROVIDER_REJECT;
		reason = DCERPC_BIND_REASON_ASYNTAX;		
	}

	context_id = call->pkt.u.alter.ctx_list[0].context_id;

	/* see if they are asking for a new interface */
	if (result == 0) {
		call->context = dcesrv_find_context(call->conn, context_id);
		if (!call->context) {
			status = dcesrv_alter_new_context(call, context_id);
			if (!NT_STATUS_IS_OK(status)) {
				result = DCERPC_BIND_PROVIDER_REJECT;
				reason = DCERPC_BIND_REASON_ASYNTAX;
			}
		}
	}

	if (result == 0 &&
	    call->pkt.u.alter.assoc_group_id != 0 &&
	    lpcfg_parm_bool(call->conn->dce_ctx->lp_ctx, NULL, "dcesrv","assoc group checking", true) &&
	    call->pkt.u.alter.assoc_group_id != call->context->assoc_group->id) {
		DEBUG(0,(__location__ ": Failed attempt to use new assoc_group in alter context (0x%08x 0x%08x)\n",
			 call->context->assoc_group->id, call->pkt.u.alter.assoc_group_id));
		/* TODO: can they ask for a new association group? */
		result = DCERPC_BIND_PROVIDER_REJECT;
		reason = DCERPC_BIND_REASON_ASYNTAX;
	}

	/* setup a alter_resp */
	dcesrv_init_hdr(&pkt, lpcfg_rpc_big_endian(call->conn->dce_ctx->lp_ctx));
	pkt.auth_length = 0;
	pkt.call_id = call->pkt.call_id;
	pkt.ptype = DCERPC_PKT_ALTER_RESP;
	pkt.pfc_flags = DCERPC_PFC_FLAG_FIRST | DCERPC_PFC_FLAG_LAST;
	pkt.u.alter_resp.max_xmit_frag = 0x2000;
	pkt.u.alter_resp.max_recv_frag = 0x2000;
	if (result == 0) {
		pkt.u.alter_resp.assoc_group_id = call->context->assoc_group->id;
	} else {
		pkt.u.alter_resp.assoc_group_id = 0;
	}
	pkt.u.alter_resp.num_results = 1;
	pkt.u.alter_resp.ctx_list = talloc_array(call, struct dcerpc_ack_ctx, 1);
	if (!pkt.u.alter_resp.ctx_list) {
		return NT_STATUS_NO_MEMORY;
	}
	pkt.u.alter_resp.ctx_list[0].result = result;
	pkt.u.alter_resp.ctx_list[0].reason = reason;
	pkt.u.alter_resp.ctx_list[0].syntax = ndr_transfer_syntax;
	pkt.u.alter_resp.auth_info = data_blob(NULL, 0);
	pkt.u.alter_resp.secondary_address = "";

	status = dcesrv_auth_alter_ack(call, &pkt);
	if (!NT_STATUS_IS_OK(status)) {
		if (NT_STATUS_EQUAL(status, NT_STATUS_ACCESS_DENIED)
		    || NT_STATUS_EQUAL(status, NT_STATUS_LOGON_FAILURE)
		    || NT_STATUS_EQUAL(status, NT_STATUS_NO_SUCH_USER)
		    || NT_STATUS_EQUAL(status, NT_STATUS_WRONG_PASSWORD)) {
			return dcesrv_fault(call, DCERPC_FAULT_ACCESS_DENIED);
		}
		return dcesrv_fault(call, 0);
	}

	rep = talloc(call, struct data_blob_list_item);
	if (!rep) {
		return NT_STATUS_NO_MEMORY;
	}

	status = ncacn_push_auth(&rep->blob, call, &pkt, call->conn->auth_state.auth_info);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	dcerpc_set_frag_length(&rep->blob, rep->blob.length);

	DLIST_ADD_END(call->replies, rep, struct data_blob_list_item *);
	dcesrv_call_set_list(call, DCESRV_LIST_CALL_LIST);

	if (call->conn->call_list && call->conn->call_list->replies) {
		if (call->conn->transport.report_output_data) {
			call->conn->transport.report_output_data(call->conn);
		}
	}

	return NT_STATUS_OK;
}

/*
  possibly save the call for inspection with ndrdump
 */
static void dcesrv_save_call(struct dcesrv_call_state *call, const char *why)
{
#ifdef DEVELOPER
	char *fname;
	const char *dump_dir;
	dump_dir = lpcfg_parm_string(call->conn->dce_ctx->lp_ctx, NULL, "dcesrv", "stubs directory");
	if (!dump_dir) {
		return;
	}
	fname = talloc_asprintf(call, "%s/RPC-%s-%u-%s.dat",
				dump_dir,
				call->context->iface->name,
				call->pkt.u.request.opnum,
				why);
	if (file_save(fname, call->pkt.u.request.stub_and_verifier.data, call->pkt.u.request.stub_and_verifier.length)) {
		DEBUG(0,("RPC SAVED %s\n", fname));
	}
	talloc_free(fname);
#endif
}

/*
  handle a dcerpc request packet
*/
static NTSTATUS dcesrv_request(struct dcesrv_call_state *call)
{
	struct ndr_pull *pull;
	NTSTATUS status;
	struct dcesrv_connection_context *context;

	/* if authenticated, and the mech we use can't do async replies, don't use them... */
	if (call->conn->auth_state.gensec_security && 
	    !gensec_have_feature(call->conn->auth_state.gensec_security, GENSEC_FEATURE_ASYNC_REPLIES)) {
		call->state_flags &= ~DCESRV_CALL_STATE_FLAG_MAY_ASYNC;
	}

	context = dcesrv_find_context(call->conn, call->pkt.u.request.context_id);
	if (context == NULL) {
		return dcesrv_fault(call, DCERPC_FAULT_UNK_IF);
	}

	pull = ndr_pull_init_blob(&call->pkt.u.request.stub_and_verifier, call);
	NT_STATUS_HAVE_NO_MEMORY(pull);

	pull->flags |= LIBNDR_FLAG_REF_ALLOC;

	call->context	= context;
	call->ndr_pull	= pull;

	if (!(call->pkt.drep[0] & DCERPC_DREP_LE)) {
		pull->flags |= LIBNDR_FLAG_BIGENDIAN;
	}

	/* unravel the NDR for the packet */
	status = context->iface->ndr_pull(call, call, pull, &call->r);
	if (!NT_STATUS_IS_OK(status)) {
		if (call->fault_code == DCERPC_FAULT_OP_RNG_ERROR) {
			/* we got an unknown call */
			DEBUG(3,(__location__ ": Unknown RPC call %u on %s\n",
				 call->pkt.u.request.opnum, context->iface->name));
			dcesrv_save_call(call, "unknown");
		} else {
			dcesrv_save_call(call, "pullfail");
		}
		return dcesrv_fault(call, call->fault_code);
	}

	if (pull->offset != pull->data_size) {
		dcesrv_save_call(call, "extrabytes");
		DEBUG(3,("Warning: %d extra bytes in incoming RPC request\n", 
			 pull->data_size - pull->offset));
	}

	/* call the dispatch function */
	status = context->iface->dispatch(call, call, call->r);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(5,("dcerpc fault in call %s:%02x - %s\n",
			 context->iface->name, 
			 call->pkt.u.request.opnum,
			 dcerpc_errstr(pull, call->fault_code)));
		return dcesrv_fault(call, call->fault_code);
	}

	/* add the call to the pending list */
	dcesrv_call_set_list(call, DCESRV_LIST_PENDING_CALL_LIST);

	if (call->state_flags & DCESRV_CALL_STATE_FLAG_ASYNC) {
		return NT_STATUS_OK;
	}

	return dcesrv_reply(call);
}


/*
  remove the call from the right list when freed
 */
static int dcesrv_call_dequeue(struct dcesrv_call_state *call)
{
	dcesrv_call_set_list(call, DCESRV_LIST_NONE);
	return 0;
}

_PUBLIC_ const struct tsocket_address *dcesrv_connection_get_local_address(struct dcesrv_connection *conn)
{
	return conn->local_address;
}

_PUBLIC_ const struct tsocket_address *dcesrv_connection_get_remote_address(struct dcesrv_connection *conn)
{
	return conn->remote_address;
}

/*
  process some input to a dcerpc endpoint server.
*/
NTSTATUS dcesrv_process_ncacn_packet(struct dcesrv_connection *dce_conn,
				     struct ncacn_packet *pkt,
				     DATA_BLOB blob)
{
	NTSTATUS status;
	struct dcesrv_call_state *call;

	call = talloc_zero(dce_conn, struct dcesrv_call_state);
	if (!call) {
		data_blob_free(&blob);
		talloc_free(pkt);
		return NT_STATUS_NO_MEMORY;
	}
	call->conn		= dce_conn;
	call->event_ctx		= dce_conn->event_ctx;
	call->msg_ctx		= dce_conn->msg_ctx;
	call->state_flags	= call->conn->state_flags;
	call->time		= timeval_current();
	call->list              = DCESRV_LIST_NONE;

	talloc_steal(call, pkt);
	talloc_steal(call, blob.data);
	call->pkt = *pkt;

	talloc_set_destructor(call, dcesrv_call_dequeue);

	/* we have to check the signing here, before combining the
	   pdus */
	if (call->pkt.ptype == DCERPC_PKT_REQUEST &&
	    !dcesrv_auth_request(call, &blob)) {
		return dcesrv_fault(call, DCERPC_FAULT_ACCESS_DENIED);		
	}

	/* see if this is a continued packet */
	if (call->pkt.ptype == DCERPC_PKT_REQUEST &&
	    !(call->pkt.pfc_flags & DCERPC_PFC_FLAG_FIRST)) {
		struct dcesrv_call_state *call2 = call;
		uint32_t alloc_size;

		/* we only allow fragmented requests, no other packet types */
		if (call->pkt.ptype != DCERPC_PKT_REQUEST) {
			return dcesrv_fault(call2, DCERPC_FAULT_OTHER);
		}

		/* this is a continuation of an existing call - find the call
		   then tack it on the end */
		call = dcesrv_find_fragmented_call(dce_conn, call2->pkt.call_id);
		if (!call) {
			return dcesrv_fault(call2, DCERPC_FAULT_OTHER);
		}

		if (call->pkt.ptype != call2->pkt.ptype) {
			/* trying to play silly buggers are we? */
			return dcesrv_fault(call2, DCERPC_FAULT_OTHER);
		}

		alloc_size = call->pkt.u.request.stub_and_verifier.length +
			call2->pkt.u.request.stub_and_verifier.length;
		if (call->pkt.u.request.alloc_hint > alloc_size) {
			alloc_size = call->pkt.u.request.alloc_hint;
		}

		call->pkt.u.request.stub_and_verifier.data = 
			talloc_realloc(call, 
				       call->pkt.u.request.stub_and_verifier.data, 
				       uint8_t, alloc_size);
		if (!call->pkt.u.request.stub_and_verifier.data) {
			return dcesrv_fault(call2, DCERPC_FAULT_OTHER);
		}
		memcpy(call->pkt.u.request.stub_and_verifier.data +
		       call->pkt.u.request.stub_and_verifier.length,
		       call2->pkt.u.request.stub_and_verifier.data,
		       call2->pkt.u.request.stub_and_verifier.length);
		call->pkt.u.request.stub_and_verifier.length += 
			call2->pkt.u.request.stub_and_verifier.length;

		call->pkt.pfc_flags |= (call2->pkt.pfc_flags & DCERPC_PFC_FLAG_LAST);

		talloc_free(call2);
	}

	/* this may not be the last pdu in the chain - if its isn't then
	   just put it on the incoming_fragmented_call_list and wait for the rest */
	if (call->pkt.ptype == DCERPC_PKT_REQUEST &&
	    !(call->pkt.pfc_flags & DCERPC_PFC_FLAG_LAST)) {
		dcesrv_call_set_list(call, DCESRV_LIST_FRAGMENTED_CALL_LIST);
		return NT_STATUS_OK;
	} 
	
	/* This removes any fragments we may have had stashed away */
	dcesrv_call_set_list(call, DCESRV_LIST_NONE);

	switch (call->pkt.ptype) {
	case DCERPC_PKT_BIND:
		status = dcesrv_bind(call);
		break;
	case DCERPC_PKT_AUTH3:
		status = dcesrv_auth3(call);
		break;
	case DCERPC_PKT_ALTER:
		status = dcesrv_alter(call);
		break;
	case DCERPC_PKT_REQUEST:
		status = dcesrv_request(call);
		break;
	default:
		status = NT_STATUS_INVALID_PARAMETER;
		break;
	}

	/* if we are going to be sending a reply then add
	   it to the list of pending calls. We add it to the end to keep the call
	   list in the order we will answer */
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(call);
	}

	return status;
}

_PUBLIC_ NTSTATUS dcesrv_init_context(TALLOC_CTX *mem_ctx, 
				      struct loadparm_context *lp_ctx,
				      const char **endpoint_servers, struct dcesrv_context **_dce_ctx)
{
	NTSTATUS status;
	struct dcesrv_context *dce_ctx;
	int i;

	if (!endpoint_servers) {
		DEBUG(0,("dcesrv_init_context: no endpoint servers configured\n"));
		return NT_STATUS_INTERNAL_ERROR;
	}

	dce_ctx = talloc(mem_ctx, struct dcesrv_context);
	NT_STATUS_HAVE_NO_MEMORY(dce_ctx);
	dce_ctx->endpoint_list	= NULL;
	dce_ctx->lp_ctx = lp_ctx;
	dce_ctx->assoc_groups_idr = idr_init(dce_ctx);
	NT_STATUS_HAVE_NO_MEMORY(dce_ctx->assoc_groups_idr);

	for (i=0;endpoint_servers[i];i++) {
		const struct dcesrv_endpoint_server *ep_server;

		ep_server = dcesrv_ep_server_byname(endpoint_servers[i]);
		if (!ep_server) {
			DEBUG(0,("dcesrv_init_context: failed to find endpoint server = '%s'\n", endpoint_servers[i]));
			return NT_STATUS_INTERNAL_ERROR;
		}

		status = ep_server->init_server(dce_ctx, ep_server);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0,("dcesrv_init_context: failed to init endpoint server = '%s': %s\n", endpoint_servers[i],
				nt_errstr(status)));
			return status;
		}
	}

	*_dce_ctx = dce_ctx;
	return NT_STATUS_OK;
}

/* the list of currently registered DCERPC endpoint servers.
 */
static struct ep_server {
	struct dcesrv_endpoint_server *ep_server;
} *ep_servers = NULL;
static int num_ep_servers;

/*
  register a DCERPC endpoint server. 

  The 'name' can be later used by other backends to find the operations
  structure for this backend.  

  The 'type' is used to specify whether this is for a disk, printer or IPC$ share
*/
_PUBLIC_ NTSTATUS dcerpc_register_ep_server(const void *_ep_server)
{
	const struct dcesrv_endpoint_server *ep_server = _ep_server;
	
	if (dcesrv_ep_server_byname(ep_server->name) != NULL) {
		/* its already registered! */
		DEBUG(0,("DCERPC endpoint server '%s' already registered\n", 
			 ep_server->name));
		return NT_STATUS_OBJECT_NAME_COLLISION;
	}

	ep_servers = realloc_p(ep_servers, struct ep_server, num_ep_servers+1);
	if (!ep_servers) {
		smb_panic("out of memory in dcerpc_register");
	}

	ep_servers[num_ep_servers].ep_server = smb_xmemdup(ep_server, sizeof(*ep_server));
	ep_servers[num_ep_servers].ep_server->name = smb_xstrdup(ep_server->name);

	num_ep_servers++;

	DEBUG(3,("DCERPC endpoint server '%s' registered\n", 
		 ep_server->name));

	return NT_STATUS_OK;
}

/*
  return the operations structure for a named backend of the specified type
*/
const struct dcesrv_endpoint_server *dcesrv_ep_server_byname(const char *name)
{
	int i;

	for (i=0;i<num_ep_servers;i++) {
		if (strcmp(ep_servers[i].ep_server->name, name) == 0) {
			return ep_servers[i].ep_server;
		}
	}

	return NULL;
}

void dcerpc_server_init(struct loadparm_context *lp_ctx)
{
	static bool initialized;
#define _MODULE_PROTO(init) extern NTSTATUS init(void);
	STATIC_dcerpc_server_MODULES_PROTO;
	init_module_fn static_init[] = { STATIC_dcerpc_server_MODULES };
	init_module_fn *shared_init;

	if (initialized) {
		return;
	}
	initialized = true;

	shared_init = load_samba_modules(NULL, lp_ctx, "dcerpc_server");

	run_init_functions(static_init);
	run_init_functions(shared_init);

	talloc_free(shared_init);
}

/*
  return the DCERPC module version, and the size of some critical types
  This can be used by endpoint server modules to either detect compilation errors, or provide
  multiple implementations for different smbd compilation options in one module
*/
const struct dcesrv_critical_sizes *dcerpc_module_version(void)
{
	static const struct dcesrv_critical_sizes critical_sizes = {
		DCERPC_MODULE_VERSION,
		sizeof(struct dcesrv_context),
		sizeof(struct dcesrv_endpoint),
		sizeof(struct dcesrv_endpoint_server),
		sizeof(struct dcesrv_interface),
		sizeof(struct dcesrv_if_list),
		sizeof(struct dcesrv_connection),
		sizeof(struct dcesrv_call_state),
		sizeof(struct dcesrv_auth),
		sizeof(struct dcesrv_handle)
	};

	return &critical_sizes;
}

static void dcesrv_terminate_connection(struct dcesrv_connection *dce_conn, const char *reason)
{
	struct stream_connection *srv_conn;
	srv_conn = talloc_get_type(dce_conn->transport.private_data,
				   struct stream_connection);

	stream_terminate_connection(srv_conn, reason);
}
/* We need this include to be able to compile on some plateforms
 * (ie. freebsd 7.2) as it seems that <sys/uio.h> is not included
 * correctly.
 * It has to be that deep because otherwise we have a conflict on
 * const struct dcesrv_interface declaration.
 * This is mostly due to socket_wrapper defining #define bind swrap_bind
 * which conflict with the bind used before.
 */
#include "system/network.h"

struct dcesrv_sock_reply_state {
	struct dcesrv_connection *dce_conn;
	struct dcesrv_call_state *call;
	struct iovec iov;
};

static void dcesrv_sock_reply_done(struct tevent_req *subreq);

static void dcesrv_sock_report_output_data(struct dcesrv_connection *dce_conn)
{
	struct dcesrv_call_state *call;

	call = dce_conn->call_list;
	if (!call || !call->replies) {
		return;
	}

	while (call->replies) {
		struct data_blob_list_item *rep = call->replies;
		struct dcesrv_sock_reply_state *substate;
		struct tevent_req *subreq;

		substate = talloc(call, struct dcesrv_sock_reply_state);
		if (!substate) {
			dcesrv_terminate_connection(dce_conn, "no memory");
			return;
		}

		substate->dce_conn = dce_conn;
		substate->call = NULL;

		DLIST_REMOVE(call->replies, rep);

		if (call->replies == NULL) {
			substate->call = call;
		}

		substate->iov.iov_base = (void *) rep->blob.data;
		substate->iov.iov_len = rep->blob.length;

		subreq = tstream_writev_queue_send(substate,
						   dce_conn->event_ctx,
						   dce_conn->stream,
						   dce_conn->send_queue,
						   &substate->iov, 1);
		if (!subreq) {
			dcesrv_terminate_connection(dce_conn, "no memory");
			return;
		}
		tevent_req_set_callback(subreq, dcesrv_sock_reply_done,
					substate);
	}

	DLIST_REMOVE(call->conn->call_list, call);
	call->list = DCESRV_LIST_NONE;
}

static void dcesrv_sock_reply_done(struct tevent_req *subreq)
{
	struct dcesrv_sock_reply_state *substate = tevent_req_callback_data(subreq,
						struct dcesrv_sock_reply_state);
	int ret;
	int sys_errno;
	NTSTATUS status;
	struct dcesrv_call_state *call = substate->call;

	ret = tstream_writev_queue_recv(subreq, &sys_errno);
	TALLOC_FREE(subreq);
	if (ret == -1) {
		status = map_nt_error_from_unix(sys_errno);
		dcesrv_terminate_connection(substate->dce_conn, nt_errstr(status));
		return;
	}

	talloc_free(substate);
	if (call) {
		talloc_free(call);
	}
}




struct dcesrv_socket_context {
	const struct dcesrv_endpoint *endpoint;
	struct dcesrv_context *dcesrv_ctx;
};


static void dcesrv_read_fragment_done(struct tevent_req *subreq);

static void dcesrv_sock_accept(struct stream_connection *srv_conn)
{
	NTSTATUS status;
	struct dcesrv_socket_context *dcesrv_sock = 
		talloc_get_type(srv_conn->private_data, struct dcesrv_socket_context);
	struct dcesrv_connection *dcesrv_conn = NULL;
	int ret;
	struct tevent_req *subreq;
	struct loadparm_context *lp_ctx = dcesrv_sock->dcesrv_ctx->lp_ctx;

	if (!srv_conn->session_info) {
		status = auth_anonymous_session_info(srv_conn,
						     lp_ctx,
						     &srv_conn->session_info);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0,("dcesrv_sock_accept: auth_anonymous_session_info failed: %s\n",
				nt_errstr(status)));
			stream_terminate_connection(srv_conn, nt_errstr(status));
			return;
		}
	}

	status = dcesrv_endpoint_connect(dcesrv_sock->dcesrv_ctx,
					 srv_conn,
					 dcesrv_sock->endpoint,
					 srv_conn->session_info,
					 srv_conn->event.ctx,
					 srv_conn->msg_ctx,
					 srv_conn->server_id,
					 DCESRV_CALL_STATE_FLAG_MAY_ASYNC,
					 &dcesrv_conn);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("dcesrv_sock_accept: dcesrv_endpoint_connect failed: %s\n", 
			nt_errstr(status)));
		stream_terminate_connection(srv_conn, nt_errstr(status));
		return;
	}

	dcesrv_conn->transport.private_data		= srv_conn;
	dcesrv_conn->transport.report_output_data	= dcesrv_sock_report_output_data;

	TALLOC_FREE(srv_conn->event.fde);

	dcesrv_conn->send_queue = tevent_queue_create(dcesrv_conn, "dcesrv send queue");
	if (!dcesrv_conn->send_queue) {
		status = NT_STATUS_NO_MEMORY;
		DEBUG(0,("dcesrv_sock_accept: tevent_queue_create(%s)\n",
			nt_errstr(status)));
		stream_terminate_connection(srv_conn, nt_errstr(status));
		return;
	}

	if (dcesrv_sock->endpoint->ep_description->transport == NCACN_NP) {
		dcesrv_conn->auth_state.session_key = dcesrv_inherited_session_key;
		dcesrv_conn->stream = talloc_move(dcesrv_conn,
						  &srv_conn->tstream);
	} else {
		ret = tstream_bsd_existing_socket(dcesrv_conn,
						  socket_get_fd(srv_conn->socket),
						  &dcesrv_conn->stream);
		if (ret == -1) {
			status = map_nt_error_from_unix(errno);
			DEBUG(0, ("dcesrv_sock_accept: "
				  "failed to setup tstream: %s\n",
				  nt_errstr(status)));
			stream_terminate_connection(srv_conn, nt_errstr(status));
			return;
		}
		socket_set_flags(srv_conn->socket, SOCKET_FLAG_NOCLOSE);
	}

	dcesrv_conn->local_address = srv_conn->local_address;
	dcesrv_conn->remote_address = srv_conn->remote_address;

	srv_conn->private_data = dcesrv_conn;

	irpc_add_name(srv_conn->msg_ctx, "rpc_server");

	subreq = dcerpc_read_ncacn_packet_send(dcesrv_conn,
					       dcesrv_conn->event_ctx,
					       dcesrv_conn->stream);
	if (!subreq) {
		status = NT_STATUS_NO_MEMORY;
		DEBUG(0,("dcesrv_sock_accept: dcerpc_read_fragment_buffer_send(%s)\n",
			nt_errstr(status)));
		stream_terminate_connection(srv_conn, nt_errstr(status));
		return;
	}
	tevent_req_set_callback(subreq, dcesrv_read_fragment_done, dcesrv_conn);

	return;
}

static void dcesrv_read_fragment_done(struct tevent_req *subreq)
{
	struct dcesrv_connection *dce_conn = tevent_req_callback_data(subreq,
					     struct dcesrv_connection);
	struct ncacn_packet *pkt;
	DATA_BLOB buffer;
	NTSTATUS status;

	status = dcerpc_read_ncacn_packet_recv(subreq, dce_conn,
					       &pkt, &buffer);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		dcesrv_terminate_connection(dce_conn, nt_errstr(status));
		return;
	}

	status = dcesrv_process_ncacn_packet(dce_conn, pkt, buffer);
	if (!NT_STATUS_IS_OK(status)) {
		dcesrv_terminate_connection(dce_conn, nt_errstr(status));
		return;
	}

	subreq = dcerpc_read_ncacn_packet_send(dce_conn,
					       dce_conn->event_ctx,
					       dce_conn->stream);
	if (!subreq) {
		status = NT_STATUS_NO_MEMORY;
		dcesrv_terminate_connection(dce_conn, nt_errstr(status));
		return;
	}
	tevent_req_set_callback(subreq, dcesrv_read_fragment_done, dce_conn);
}

static void dcesrv_sock_recv(struct stream_connection *conn, uint16_t flags)
{
	struct dcesrv_connection *dce_conn = talloc_get_type(conn->private_data,
					     struct dcesrv_connection);
	dcesrv_terminate_connection(dce_conn, "dcesrv_sock_recv triggered");
}

static void dcesrv_sock_send(struct stream_connection *conn, uint16_t flags)
{
	struct dcesrv_connection *dce_conn = talloc_get_type(conn->private_data,
					     struct dcesrv_connection);
	dcesrv_terminate_connection(dce_conn, "dcesrv_sock_send triggered");
}


static const struct stream_server_ops dcesrv_stream_ops = {
	.name			= "rpc",
	.accept_connection	= dcesrv_sock_accept,
	.recv_handler		= dcesrv_sock_recv,
	.send_handler		= dcesrv_sock_send,
};

static NTSTATUS dcesrv_add_ep_unix(struct dcesrv_context *dce_ctx, 
				   struct loadparm_context *lp_ctx,
				   struct dcesrv_endpoint *e,
			    struct tevent_context *event_ctx, const struct model_ops *model_ops)
{
	struct dcesrv_socket_context *dcesrv_sock;
	uint16_t port = 1;
	NTSTATUS status;

	dcesrv_sock = talloc(event_ctx, struct dcesrv_socket_context);
	NT_STATUS_HAVE_NO_MEMORY(dcesrv_sock);

	/* remember the endpoint of this socket */
	dcesrv_sock->endpoint		= e;
	dcesrv_sock->dcesrv_ctx		= talloc_reference(dcesrv_sock, dce_ctx);

	status = stream_setup_socket(dcesrv_sock, event_ctx, lp_ctx,
				     model_ops, &dcesrv_stream_ops, 
				     "unix", e->ep_description->endpoint, &port, 
				     lpcfg_socket_options(lp_ctx),
				     dcesrv_sock);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("service_setup_stream_socket(path=%s) failed - %s\n",
			 e->ep_description->endpoint, nt_errstr(status)));
	}

	return status;
}

static NTSTATUS dcesrv_add_ep_ncalrpc(struct dcesrv_context *dce_ctx, 
				      struct loadparm_context *lp_ctx,
				      struct dcesrv_endpoint *e,
				      struct tevent_context *event_ctx, const struct model_ops *model_ops)
{
	struct dcesrv_socket_context *dcesrv_sock;
	uint16_t port = 1;
	char *full_path;
	NTSTATUS status;

	if (!e->ep_description->endpoint) {
		/* No identifier specified: use DEFAULT. 
		 * DO NOT hardcode this value anywhere else. Rather, specify 
		 * no endpoint and let the epmapper worry about it. */
		e->ep_description->endpoint = talloc_strdup(dce_ctx, "DEFAULT");
	}

	full_path = talloc_asprintf(dce_ctx, "%s/%s", lpcfg_ncalrpc_dir(lp_ctx),
				    e->ep_description->endpoint);

	dcesrv_sock = talloc(event_ctx, struct dcesrv_socket_context);
	NT_STATUS_HAVE_NO_MEMORY(dcesrv_sock);

	/* remember the endpoint of this socket */
	dcesrv_sock->endpoint		= e;
	dcesrv_sock->dcesrv_ctx		= talloc_reference(dcesrv_sock, dce_ctx);

	status = stream_setup_socket(dcesrv_sock, event_ctx, lp_ctx,
				     model_ops, &dcesrv_stream_ops, 
				     "unix", full_path, &port, 
				     lpcfg_socket_options(lp_ctx),
				     dcesrv_sock);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("service_setup_stream_socket(identifier=%s,path=%s) failed - %s\n",
			 e->ep_description->endpoint, full_path, nt_errstr(status)));
	}
	return status;
}

static NTSTATUS dcesrv_add_ep_np(struct dcesrv_context *dce_ctx,
				 struct loadparm_context *lp_ctx,
				 struct dcesrv_endpoint *e,
				 struct tevent_context *event_ctx, const struct model_ops *model_ops)
{
	struct dcesrv_socket_context *dcesrv_sock;
	NTSTATUS status;
			
	if (e->ep_description->endpoint == NULL) {
		DEBUG(0, ("Endpoint mandatory for named pipes\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	dcesrv_sock = talloc(event_ctx, struct dcesrv_socket_context);
	NT_STATUS_HAVE_NO_MEMORY(dcesrv_sock);

	/* remember the endpoint of this socket */
	dcesrv_sock->endpoint		= e;
	dcesrv_sock->dcesrv_ctx		= talloc_reference(dcesrv_sock, dce_ctx);

	status = tstream_setup_named_pipe(dce_ctx, event_ctx, lp_ctx,
					  model_ops, &dcesrv_stream_ops,
					  e->ep_description->endpoint,
					  dcesrv_sock);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("stream_setup_named_pipe(pipe=%s) failed - %s\n",
			 e->ep_description->endpoint, nt_errstr(status)));
		return status;
	}

	return NT_STATUS_OK;
}

/*
  add a socket address to the list of events, one event per dcerpc endpoint
*/
static NTSTATUS add_socket_rpc_tcp_iface(struct dcesrv_context *dce_ctx, struct dcesrv_endpoint *e,
					 struct tevent_context *event_ctx, const struct model_ops *model_ops,
					 const char *address)
{
	struct dcesrv_socket_context *dcesrv_sock;
	uint16_t port = 0;
	NTSTATUS status;
			
	if (e->ep_description->endpoint) {
		port = atoi(e->ep_description->endpoint);
	}

	dcesrv_sock = talloc(event_ctx, struct dcesrv_socket_context);
	NT_STATUS_HAVE_NO_MEMORY(dcesrv_sock);

	/* remember the endpoint of this socket */
	dcesrv_sock->endpoint		= e;
	dcesrv_sock->dcesrv_ctx		= talloc_reference(dcesrv_sock, dce_ctx);

	status = stream_setup_socket(dcesrv_sock, event_ctx, dce_ctx->lp_ctx,
				     model_ops, &dcesrv_stream_ops, 
				     "ipv4", address, &port, 
				     lpcfg_socket_options(dce_ctx->lp_ctx),
				     dcesrv_sock);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("service_setup_stream_socket(address=%s,port=%u) failed - %s\n", 
			 address, port, nt_errstr(status)));
	}

	if (e->ep_description->endpoint == NULL) {
		e->ep_description->endpoint = talloc_asprintf(dce_ctx, "%d", port);
	}

	return status;
}

#include "lib/socket/netif.h" /* Included here to work around the fact that socket_wrapper redefines bind() */

static NTSTATUS dcesrv_add_ep_tcp(struct dcesrv_context *dce_ctx, 
				  struct loadparm_context *lp_ctx,
				  struct dcesrv_endpoint *e,
				  struct tevent_context *event_ctx, const struct model_ops *model_ops)
{
	NTSTATUS status;

	/* Add TCP/IP sockets */
	if (lpcfg_interfaces(lp_ctx) && lpcfg_bind_interfaces_only(lp_ctx)) {
		int num_interfaces;
		int i;
		struct interface *ifaces;

		load_interfaces(dce_ctx, lpcfg_interfaces(lp_ctx), &ifaces);

		num_interfaces = iface_count(ifaces);
		for(i = 0; i < num_interfaces; i++) {
			const char *address = iface_n_ip(ifaces, i);
			status = add_socket_rpc_tcp_iface(dce_ctx, e, event_ctx, model_ops, address);
			NT_STATUS_NOT_OK_RETURN(status);
		}
	} else {
		status = add_socket_rpc_tcp_iface(dce_ctx, e, event_ctx, model_ops, 
						  lpcfg_socket_address(lp_ctx));
		NT_STATUS_NOT_OK_RETURN(status);
	}

	return NT_STATUS_OK;
}

NTSTATUS dcesrv_add_ep(struct dcesrv_context *dce_ctx,
		       struct loadparm_context *lp_ctx,
		       struct dcesrv_endpoint *e,
		       struct tevent_context *event_ctx,
		       const struct model_ops *model_ops)
{
	switch (e->ep_description->transport) {
	case NCACN_UNIX_STREAM:
		return dcesrv_add_ep_unix(dce_ctx, lp_ctx, e, event_ctx, model_ops);

	case NCALRPC:
		return dcesrv_add_ep_ncalrpc(dce_ctx, lp_ctx, e, event_ctx, model_ops);

	case NCACN_IP_TCP:
		return dcesrv_add_ep_tcp(dce_ctx, lp_ctx, e, event_ctx, model_ops);

	case NCACN_NP:
		return dcesrv_add_ep_np(dce_ctx, lp_ctx, e, event_ctx, model_ops);

	default:
		return NT_STATUS_NOT_SUPPORTED;
	}
}


/**
 * retrieve credentials from a dce_call
 */
_PUBLIC_ struct cli_credentials *dcesrv_call_credentials(struct dcesrv_call_state *dce_call)
{
	return dce_call->conn->auth_state.session_info->credentials;
}

/**
 * returns true if this is an authenticated call
 */
_PUBLIC_ bool dcesrv_call_authenticated(struct dcesrv_call_state *dce_call)
{
	enum security_user_level level;
	level = security_session_user_level(dce_call->conn->auth_state.session_info, NULL);
	return level >= SECURITY_USER;
}

/**
 * retrieve account_name for a dce_call
 */
_PUBLIC_ const char *dcesrv_call_account_name(struct dcesrv_call_state *dce_call)
{
	return dce_call->context->conn->auth_state.session_info->info->account_name;
}
