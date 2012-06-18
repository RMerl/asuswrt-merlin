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
#include "libcli/ldap/ldap.h"
#include "lib/tls/tls.h"
#include "smbd/service_stream.h"

struct ldapsrv_starttls_context {
	struct ldapsrv_connection *conn;
	struct socket_context *tls_socket;
};

static void ldapsrv_start_tls(void *private_data)
{
	struct ldapsrv_starttls_context *ctx = talloc_get_type(private_data, struct ldapsrv_starttls_context);
	talloc_steal(ctx->conn->connection, ctx->tls_socket);

	ctx->conn->sockets.tls = ctx->tls_socket;
	ctx->conn->connection->socket = ctx->tls_socket;
	packet_set_socket(ctx->conn->packet, ctx->conn->connection->socket);
	packet_set_unreliable_select(ctx->conn->packet);
}

static NTSTATUS ldapsrv_StartTLS(struct ldapsrv_call *call,
				 struct ldapsrv_reply *reply,
				 const char **errstr)
{
	struct ldapsrv_starttls_context *ctx;

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

	ctx = talloc(call, struct ldapsrv_starttls_context);
	NT_STATUS_HAVE_NO_MEMORY(ctx);

	ctx->conn = call->conn;
	ctx->tls_socket = tls_init_server(call->conn->service->tls_params,
					  call->conn->connection->socket,
					  call->conn->connection->event.fde, 
					  NULL);
	if (!ctx->tls_socket) {
		(*errstr) = talloc_asprintf(reply, "START-TLS: Failed to setup TLS socket");
		return NT_STATUS_LDAP(LDAP_OPERATIONS_ERROR);
	}

	call->send_callback = ldapsrv_start_tls;
	call->send_private  = ctx;

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
	uint32_t i;

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
