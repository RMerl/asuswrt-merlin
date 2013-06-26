/* 
   Unix SMB/CIFS implementation.
   LDAP server
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
#include "ldap_server/ldap_server.h"
#include "../lib/util/dlinklist.h"
#include "lib/tls/tls.h"
#include "smbd/service_stream.h"
#include "../lib/util/tevent_ntstatus.h"

struct ldapsrv_starttls_postprocess_context {
	struct ldapsrv_connection *conn;
};

struct ldapsrv_starttls_postprocess_state {
	struct ldapsrv_connection *conn;
};

static void ldapsrv_starttls_postprocess_done(struct tevent_req *subreq);

static struct tevent_req *ldapsrv_starttls_postprocess_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						void *private_data)
{
	struct ldapsrv_starttls_postprocess_context *context =
		talloc_get_type_abort(private_data,
		struct ldapsrv_starttls_postprocess_context);
	struct ldapsrv_connection *conn = context->conn;
	struct tevent_req *req;
	struct ldapsrv_starttls_postprocess_state *state;
	struct tevent_req *subreq;

	req = tevent_req_create(mem_ctx, &state,
				struct ldapsrv_starttls_postprocess_state);
	if (req == NULL) {
		return NULL;
	}

	state->conn = conn;

	subreq = tstream_tls_accept_send(conn,
					 conn->connection->event.ctx,
					 conn->sockets.raw,
					 conn->service->tls_params);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, ldapsrv_starttls_postprocess_done, req);

	return req;
}

static void ldapsrv_starttls_postprocess_done(struct tevent_req *subreq)
{
	struct tevent_req *req =
		tevent_req_callback_data(subreq,
		struct tevent_req);
	struct ldapsrv_starttls_postprocess_state *state =
		tevent_req_data(req,
		struct ldapsrv_starttls_postprocess_state);
	struct ldapsrv_connection *conn = state->conn;
	int ret;
	int sys_errno;

	ret = tstream_tls_accept_recv(subreq, &sys_errno,
				      conn, &conn->sockets.tls);
	TALLOC_FREE(subreq);
	if (ret == -1) {
		NTSTATUS status = map_nt_error_from_unix(sys_errno);

		DEBUG(1,("ldapsrv_starttls_postprocess_done: accept_tls_loop: "
			 "tstream_tls_accept_recv() - %d:%s => %s",
			 sys_errno, strerror(sys_errno), nt_errstr(status)));

		tevent_req_nterror(req, status);
		return;
	}

	conn->sockets.active = conn->sockets.tls;

	tevent_req_done(req);
}

static NTSTATUS ldapsrv_starttls_postprocess_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}

static NTSTATUS ldapsrv_StartTLS(struct ldapsrv_call *call,
				 struct ldapsrv_reply *reply,
				 const char **errstr)
{
	struct ldapsrv_starttls_postprocess_context *context;

	(*errstr) = NULL;

	/*
	 * TODO: give LDAP_OPERATIONS_ERROR also when
	 *       there're pending requests or there's
	 *       a SASL bind in progress
	 *       (see rfc4513 section 3.1.1)
	 */
	if (call->conn->sockets.tls) {
		(*errstr) = talloc_asprintf(reply, "START-TLS: TLS is already enabled on this LDAP session");
		return NT_STATUS_LDAP(LDAP_OPERATIONS_ERROR);
	}

	if (call->conn->sockets.sasl) {
		(*errstr) = talloc_asprintf(reply, "START-TLS: SASL is already enabled on this LDAP session");
		return NT_STATUS_LDAP(LDAP_OPERATIONS_ERROR);
	}

	context = talloc(call, struct ldapsrv_starttls_postprocess_context);
	NT_STATUS_HAVE_NO_MEMORY(context);

	context->conn = call->conn;

	call->postprocess_send = ldapsrv_starttls_postprocess_send;
	call->postprocess_recv = ldapsrv_starttls_postprocess_recv;
	call->postprocess_private = context;

	reply->msg->r.ExtendedResponse.response.resultcode = LDAP_SUCCESS;
	reply->msg->r.ExtendedResponse.response.errormessage = NULL;

	ldapsrv_queue_reply(call, reply);
	return NT_STATUS_OK;
}

struct ldapsrv_extended_operation {
	const char *oid;
	NTSTATUS (*fn)(struct ldapsrv_call *call, struct ldapsrv_reply *reply, const char **errorstr);
};

static struct ldapsrv_extended_operation extended_ops[] = {
	{
		.oid	= LDB_EXTENDED_START_TLS_OID,
		.fn	= ldapsrv_StartTLS,
	},{
		.oid	= NULL,
		.fn	= NULL,
	}
};

NTSTATUS ldapsrv_ExtendedRequest(struct ldapsrv_call *call)
{
	struct ldap_ExtendedRequest *req = &call->request->r.ExtendedRequest;
	struct ldapsrv_reply *reply;
	int result = LDAP_PROTOCOL_ERROR;
	const char *error_str = NULL;
	NTSTATUS status = NT_STATUS_OK;
	unsigned int i;

	DEBUG(10, ("Extended\n"));

	reply = ldapsrv_init_reply(call, LDAP_TAG_ExtendedResponse);
	NT_STATUS_HAVE_NO_MEMORY(reply);
 
 	ZERO_STRUCT(reply->msg->r);
	reply->msg->r.ExtendedResponse.oid = talloc_steal(reply, req->oid);
	reply->msg->r.ExtendedResponse.response.resultcode = LDAP_PROTOCOL_ERROR;
	reply->msg->r.ExtendedResponse.response.errormessage = NULL;
 
	for (i=0; extended_ops[i].oid; i++) {
		if (strcmp(extended_ops[i].oid,req->oid) != 0) continue;
 
		/* 
		 * if the backend function returns an error we
		 * need to send the reply otherwise the reply is already
		 * send and we need to return directly
		 */
		status = extended_ops[i].fn(call, reply, &error_str);
		NT_STATUS_IS_OK_RETURN(status);
 
		if (NT_STATUS_IS_LDAP(status)) {
			result = NT_STATUS_LDAP_CODE(status);
		} else {
 			result = LDAP_OPERATIONS_ERROR;
			error_str = talloc_asprintf(reply, "Extended Operation(%s) failed: %s",
						    req->oid, nt_errstr(status));
 		}
 	}
	/* if we haven't found the oid, then status is still NT_STATUS_OK */
	if (NT_STATUS_IS_OK(status)) {
		error_str = talloc_asprintf(reply, "Extended Operation(%s) not supported",
					    req->oid);
	}
 
	reply->msg->r.ExtendedResponse.response.resultcode = result;
	reply->msg->r.ExtendedResponse.response.errormessage = error_str;
 
 	ldapsrv_queue_reply(call, reply);
 	return NT_STATUS_OK;
}
