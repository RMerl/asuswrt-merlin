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
#include "auth/auth.h"
#include "smbd/service.h"
#include <ldb.h>
#include <ldb_errors.h>
#include "dsdb/samdb/samdb.h"
#include "auth/gensec/gensec.h"
#include "auth/gensec/gensec_tstream.h"
#include "param/param.h"
#include "../lib/util/tevent_ntstatus.h"

static NTSTATUS ldapsrv_BindSimple(struct ldapsrv_call *call)
{
	struct ldap_BindRequest *req = &call->request->r.BindRequest;
	struct ldapsrv_reply *reply;
	struct ldap_BindResponse *resp;

	int result;
	const char *errstr;
	const char *nt4_domain, *nt4_account;

	struct auth_session_info *session_info;

	NTSTATUS status;

	DEBUG(10, ("BindSimple dn: %s\n",req->dn));

	status = crack_auto_name_to_nt4_name(call, call->conn->connection->event.ctx, call->conn->lp_ctx, req->dn, &nt4_domain, &nt4_account);
	if (NT_STATUS_IS_OK(status)) {
		status = authenticate_username_pw(call,
						  call->conn->connection->event.ctx,
						  call->conn->connection->msg_ctx,
						  call->conn->lp_ctx,
						  nt4_domain, nt4_account, 
						  req->creds.password,
						  MSV1_0_ALLOW_SERVER_TRUST_ACCOUNT |
						  MSV1_0_ALLOW_WORKSTATION_TRUST_ACCOUNT,
						  &session_info);
	}

	reply = ldapsrv_init_reply(call, LDAP_TAG_BindResponse);
	if (!reply) {
		return NT_STATUS_NO_MEMORY;
	}

	if (NT_STATUS_IS_OK(status)) {
		result = LDAP_SUCCESS;
		errstr = NULL;

		talloc_unlink(call->conn, call->conn->session_info);
		call->conn->session_info = session_info;
		talloc_steal(call->conn, session_info);

		/* don't leak the old LDB */
		talloc_unlink(call->conn, call->conn->ldb);

		status = ldapsrv_backend_Init(call->conn);		
		
		if (!NT_STATUS_IS_OK(status)) {
			result = LDAP_OPERATIONS_ERROR;
			errstr = talloc_asprintf(reply, "Simple Bind: Failed to advise ldb new credentials: %s", nt_errstr(status));
		}
	} else {
		status = nt_status_squash(status);

		result = LDAP_INVALID_CREDENTIALS;
		errstr = talloc_asprintf(reply, "Simple Bind Failed: %s", nt_errstr(status));
	}

	resp = &reply->msg->r.BindResponse;
	resp->response.resultcode = result;
	resp->response.errormessage = errstr;
	resp->response.dn = NULL;
	resp->response.referral = NULL;
	resp->SASL.secblob = NULL;

	ldapsrv_queue_reply(call, reply);
	return NT_STATUS_OK;
}

struct ldapsrv_sasl_postprocess_context {
	struct ldapsrv_connection *conn;
	struct tstream_context *sasl;
};

struct ldapsrv_sasl_postprocess_state {
	uint8_t dummy;
};

static struct tevent_req *ldapsrv_sasl_postprocess_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						void *private_data)
{
	struct ldapsrv_sasl_postprocess_context *context =
		talloc_get_type_abort(private_data,
		struct ldapsrv_sasl_postprocess_context);
	struct tevent_req *req;
	struct ldapsrv_sasl_postprocess_state *state;

	req = tevent_req_create(mem_ctx, &state,
				struct ldapsrv_sasl_postprocess_state);
	if (req == NULL) {
		return NULL;
	}

	TALLOC_FREE(context->conn->sockets.sasl);
	context->conn->sockets.sasl = talloc_move(context->conn, &context->sasl);
	context->conn->sockets.active = context->conn->sockets.sasl;

	tevent_req_done(req);
	return tevent_req_post(req, ev);
}

static NTSTATUS ldapsrv_sasl_postprocess_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}

static NTSTATUS ldapsrv_BindSASL(struct ldapsrv_call *call)
{
	struct ldap_BindRequest *req = &call->request->r.BindRequest;
	struct ldapsrv_reply *reply;
	struct ldap_BindResponse *resp;
	struct ldapsrv_connection *conn;
	int result = 0;
	const char *errstr=NULL;
	NTSTATUS status = NT_STATUS_OK;

	DEBUG(10, ("BindSASL dn: %s\n",req->dn));

	reply = ldapsrv_init_reply(call, LDAP_TAG_BindResponse);
	if (!reply) {
		return NT_STATUS_NO_MEMORY;
	}
	resp = &reply->msg->r.BindResponse;
	
	conn = call->conn;

	/* 
	 * TODO: a SASL bind with a different mechanism
	 *       should cancel an inprogress SASL bind.
	 *       (see RFC 4513)
	 */

	if (!conn->gensec) {
		conn->session_info = NULL;

		status = samba_server_gensec_start(conn,
						   conn->connection->event.ctx,
						   conn->connection->msg_ctx,
						   conn->lp_ctx,
						   conn->server_credentials,
						   "ldap",
						   &conn->gensec);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(1, ("Failed to start GENSEC server code: %s\n", nt_errstr(status)));
			result = LDAP_OPERATIONS_ERROR;
			errstr = talloc_asprintf(reply, "SASL: Failed to start authentication system: %s", 
						 nt_errstr(status));
		} else {

			gensec_want_feature(conn->gensec, GENSEC_FEATURE_SIGN);
			gensec_want_feature(conn->gensec, GENSEC_FEATURE_SEAL);
			gensec_want_feature(conn->gensec, GENSEC_FEATURE_ASYNC_REPLIES);
			
			status = gensec_start_mech_by_sasl_name(conn->gensec, req->creds.SASL.mechanism);
			
			if (!NT_STATUS_IS_OK(status)) {
				DEBUG(1, ("Failed to start GENSEC SASL[%s] server code: %s\n", 
					  req->creds.SASL.mechanism, nt_errstr(status)));
				result = LDAP_OPERATIONS_ERROR;
				errstr = talloc_asprintf(reply, "SASL:[%s]: Failed to start authentication backend: %s", 
							 req->creds.SASL.mechanism, nt_errstr(status));
			}
		}
	}

	if (NT_STATUS_IS_OK(status)) {
		DATA_BLOB input = data_blob(NULL, 0);
		DATA_BLOB output = data_blob(NULL, 0);

		if (req->creds.SASL.secblob) {
			input = *req->creds.SASL.secblob;
		}

		status = gensec_update(conn->gensec, reply,
				       input, &output);

		/* Windows 2000 mmc doesn't like secblob == NULL and reports a decoding error */
		resp->SASL.secblob = talloc(reply, DATA_BLOB);
		NT_STATUS_HAVE_NO_MEMORY(resp->SASL.secblob);
		*resp->SASL.secblob = output;
	} else {
		resp->SASL.secblob = NULL;
	}

	if (NT_STATUS_EQUAL(NT_STATUS_MORE_PROCESSING_REQUIRED, status)) {
		result = LDAP_SASL_BIND_IN_PROGRESS;
		errstr = NULL;
	} else if (NT_STATUS_IS_OK(status)) {
		struct auth_session_info *old_session_info=NULL;
		struct ldapsrv_sasl_postprocess_context *context = NULL;

		result = LDAP_SUCCESS;
		errstr = NULL;

		if (gensec_have_feature(conn->gensec, GENSEC_FEATURE_SIGN) ||
		    gensec_have_feature(conn->gensec, GENSEC_FEATURE_SEAL)) {

			context = talloc(call, struct ldapsrv_sasl_postprocess_context);

			if (!context) {
				status = NT_STATUS_NO_MEMORY;
			}
		}

		if (context && conn->sockets.tls) {
			TALLOC_FREE(context);
			status = NT_STATUS_NOT_SUPPORTED;
			result = LDAP_UNWILLING_TO_PERFORM;
			errstr = talloc_asprintf(reply,
						 "SASL:[%s]: Sign or Seal are not allowed if TLS is used",
						 req->creds.SASL.mechanism);
		}

		if (context && conn->sockets.sasl) {
			TALLOC_FREE(context);
			status = NT_STATUS_NOT_SUPPORTED;
			result = LDAP_UNWILLING_TO_PERFORM;
			errstr = talloc_asprintf(reply,
						 "SASL:[%s]: Sign or Seal are not allowed if SASL encryption has already been set up",
						 req->creds.SASL.mechanism);
		}

		if (context) {
			context->conn = conn;
			status = gensec_create_tstream(context,
						       context->conn->gensec,
						       context->conn->sockets.raw,
						       &context->sasl);
			if (NT_STATUS_IS_OK(status)) {
				if (!talloc_reference(context->sasl, conn->gensec)) {
					status = NT_STATUS_NO_MEMORY;
				}
			}
		}

		if (result != LDAP_SUCCESS) {
			conn->session_info = old_session_info;
		} else if (!NT_STATUS_IS_OK(status)) {
			conn->session_info = old_session_info;
			result = LDAP_OPERATIONS_ERROR;
			errstr = talloc_asprintf(reply, 
						 "SASL:[%s]: Failed to setup SASL socket: %s", 
						 req->creds.SASL.mechanism, nt_errstr(status));
		} else {

			old_session_info = conn->session_info;
			conn->session_info = NULL;
			status = gensec_session_info(conn->gensec, &conn->session_info);
			if (!NT_STATUS_IS_OK(status)) {
				conn->session_info = old_session_info;
				result = LDAP_OPERATIONS_ERROR;
				errstr = talloc_asprintf(reply, 
							 "SASL:[%s]: Failed to get session info: %s", 
							 req->creds.SASL.mechanism, nt_errstr(status));
			} else {
				talloc_unlink(conn, old_session_info);
				talloc_steal(conn, conn->session_info);
				
				/* don't leak the old LDB */
				talloc_unlink(conn, conn->ldb);
				
				status = ldapsrv_backend_Init(conn);		
				
				if (!NT_STATUS_IS_OK(status)) {
					result = LDAP_OPERATIONS_ERROR;
					errstr = talloc_asprintf(reply, 
								 "SASL:[%s]: Failed to advise samdb of new credentials: %s", 
								 req->creds.SASL.mechanism, 
								 nt_errstr(status));
				}
			}
		}

		if (NT_STATUS_IS_OK(status) && context) {
			call->postprocess_send = ldapsrv_sasl_postprocess_send;
			call->postprocess_recv = ldapsrv_sasl_postprocess_recv;
			call->postprocess_private = context;
		}
		talloc_unlink(conn, conn->gensec);
		conn->gensec = NULL;
	} else {
		status = nt_status_squash(status);
		if (result == 0) {
			result = LDAP_INVALID_CREDENTIALS;
			errstr = talloc_asprintf(reply, "SASL:[%s]: %s", req->creds.SASL.mechanism, nt_errstr(status));
		}
		talloc_unlink(conn, conn->gensec);
		conn->gensec = NULL;
	}

	resp->response.resultcode = result;
	resp->response.dn = NULL;
	resp->response.errormessage = errstr;
	resp->response.referral = NULL;

	ldapsrv_queue_reply(call, reply);
	return NT_STATUS_OK;
}

NTSTATUS ldapsrv_BindRequest(struct ldapsrv_call *call)
{
	struct ldap_BindRequest *req = &call->request->r.BindRequest;
	struct ldapsrv_reply *reply;
	struct ldap_BindResponse *resp;

	/* 
	 * TODO: we should fail the bind request
	 *       if there're any pending requests.
	 *
	 *       also a simple bind should cancel an
	 *       inprogress SASL bind.
	 *       (see RFC 4513)
	 */
	switch (req->mechanism) {
		case LDAP_AUTH_MECH_SIMPLE:
			return ldapsrv_BindSimple(call);
		case LDAP_AUTH_MECH_SASL:
			return ldapsrv_BindSASL(call);
	}

	reply = ldapsrv_init_reply(call, LDAP_TAG_BindResponse);
	if (!reply) {
		return NT_STATUS_NO_MEMORY;
	}

	resp = &reply->msg->r.BindResponse;
	resp->response.resultcode = 7;
	resp->response.dn = NULL;
	resp->response.errormessage = talloc_asprintf(reply, "Bad AuthenticationChoice [%d]", req->mechanism);
	resp->response.referral = NULL;
	resp->SASL.secblob = NULL;

	ldapsrv_queue_reply(call, reply);
	return NT_STATUS_OK;
}

NTSTATUS ldapsrv_UnbindRequest(struct ldapsrv_call *call)
{
	DEBUG(10, ("UnbindRequest\n"));
	return NT_STATUS_OK;
}
