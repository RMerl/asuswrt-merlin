/* 
   Unix SMB/CIFS implementation.

   NTP packet signing server

   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2005
   Copyright (C) Andrew Tridgell	2005
   Copyright (C) Stefan Metzmacher	2005

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
#include "smbd/service_task.h"
#include "smbd/service.h"
#include "smbd/service_stream.h"
#include "smbd/process_model.h"
#include "lib/stream/packet.h"
#include "lib/tsocket/tsocket.h"
#include "libcli/util/tstream.h"
#include "librpc/gen_ndr/ndr_ntp_signd.h"
#include "param/param.h"
#include "dsdb/samdb/samdb.h"
#include "auth/auth.h"
#include "libcli/security/security.h"
#include "libcli/ldap/ldap_ndr.h"
#include <ldb.h>
#include <ldb_errors.h>
#include "../lib/crypto/md5.h"
#include "system/network.h"
#include "system/passwd.h"

/*
  top level context structure for the ntp_signd server
*/
struct ntp_signd_server {
	struct task_server *task;
	struct ldb_context *samdb;
};

/*
  state of an open connection
*/
struct ntp_signd_connection {
	/* stream connection we belong to */
	struct stream_connection *conn;

	/* the ntp_signd_server the connection belongs to */
	struct ntp_signd_server *ntp_signd;

	struct tstream_context *tstream;

	struct tevent_queue *send_queue;
};

static void ntp_signd_terminate_connection(struct ntp_signd_connection *ntp_signd_conn, const char *reason)
{
	stream_terminate_connection(ntp_signd_conn->conn, reason);
}

static NTSTATUS signing_failure(struct ntp_signd_connection *ntp_signdconn,
				TALLOC_CTX *mem_ctx,
				DATA_BLOB *output,
				uint32_t packet_id)
{
	struct signed_reply signed_reply;
	enum ndr_err_code ndr_err;

	signed_reply.op = SIGNING_FAILURE;
	signed_reply.packet_id = packet_id;
	signed_reply.signed_packet = data_blob(NULL, 0);
	
	ndr_err = ndr_push_struct_blob(output, mem_ctx, &signed_reply,
				       (ndr_push_flags_fn_t)ndr_push_signed_reply);

	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DEBUG(1,("failed to push ntp error reply\n"));
		return ndr_map_error2ntstatus(ndr_err);
	}

	return NT_STATUS_OK;
}

/*
  receive a full packet on a NTP_SIGND connection
*/
static NTSTATUS ntp_signd_process(struct ntp_signd_connection *ntp_signd_conn,
				  TALLOC_CTX *mem_ctx,
				  DATA_BLOB *input,
				  DATA_BLOB *output)
{
	const struct dom_sid *domain_sid;
	struct dom_sid *sid;
	struct sign_request sign_request;
	struct signed_reply signed_reply;
	enum ndr_err_code ndr_err;
	struct ldb_result *res;
	const char *attrs[] = { "unicodePwd", "userAccountControl", "cn", NULL };
	MD5_CTX ctx;
	struct samr_Password *nt_hash;
	uint32_t user_account_control;
	int ret;

	ndr_err = ndr_pull_struct_blob_all(input, mem_ctx,
					   &sign_request,
					   (ndr_pull_flags_fn_t)ndr_pull_sign_request);

	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DEBUG(1,("failed to parse ntp signing request\n"));
		dump_data(1, input->data, input->length);
		return ndr_map_error2ntstatus(ndr_err);
	}

	/* We need to implement 'check signature' and 'request server
	 * to sign' operations at some point */
	if (sign_request.op != SIGN_TO_CLIENT) {
		return signing_failure(ntp_signd_conn,
				       mem_ctx,
				       output,
				       sign_request.packet_id);
	}

	/* We need to implement 'check signature' and 'request server
	 * to sign' operations at some point */
	if (sign_request.version != NTP_SIGND_PROTOCOL_VERSION_0) {
		return signing_failure(ntp_signd_conn,
				       mem_ctx,
				       output,
				       sign_request.packet_id);
	}

	domain_sid = samdb_domain_sid(ntp_signd_conn->ntp_signd->samdb);
	if (domain_sid == NULL) {
		return signing_failure(ntp_signd_conn,
				       mem_ctx,
				       output,
				       sign_request.packet_id);
	}
	
	/* The top bit is a 'key selector' */
	sid = dom_sid_add_rid(mem_ctx, domain_sid,
			      sign_request.key_id & 0x7FFFFFFF);
	if (sid == NULL) {
		talloc_free(mem_ctx);
		return signing_failure(ntp_signd_conn,
				       mem_ctx,
				       output,
				       sign_request.packet_id);
	}

	ret = ldb_search(ntp_signd_conn->ntp_signd->samdb, mem_ctx,
				 &res,
				 ldb_get_default_basedn(ntp_signd_conn->ntp_signd->samdb),
				 LDB_SCOPE_SUBTREE,
				 attrs,
				 "(&(objectSid=%s)(objectClass=user))",
				 ldap_encode_ndr_dom_sid(mem_ctx, sid));
	if (ret != LDB_SUCCESS) {
		DEBUG(2, ("Failed to search for SID %s in SAM for NTP signing: "
			  "%s\n",
			  dom_sid_string(mem_ctx, sid),
			  ldb_errstring(ntp_signd_conn->ntp_signd->samdb)));
		return signing_failure(ntp_signd_conn,
				       mem_ctx,
				       output,
				       sign_request.packet_id);
	}

	if (res->count == 0) {
		DEBUG(5, ("Failed to find SID %s in SAM for NTP signing\n",
			  dom_sid_string(mem_ctx, sid)));
	} else if (res->count != 1) {
		DEBUG(1, ("Found SID %s %u times in SAM for NTP signing\n",
			  dom_sid_string(mem_ctx, sid), res->count));
		return signing_failure(ntp_signd_conn,
				       mem_ctx,
				       output,
				       sign_request.packet_id);
	}

	user_account_control = ldb_msg_find_attr_as_uint(res->msgs[0],
							 "userAccountControl",
							 0);

	if (user_account_control & UF_ACCOUNTDISABLE) {
		DEBUG(1, ("Account %s for SID [%s] is disabled\n",
			  ldb_dn_get_linearized(res->msgs[0]->dn),
			  dom_sid_string(mem_ctx, sid)));
		return NT_STATUS_ACCESS_DENIED;
	}

	if (!(user_account_control & (UF_INTERDOMAIN_TRUST_ACCOUNT|UF_SERVER_TRUST_ACCOUNT|UF_WORKSTATION_TRUST_ACCOUNT))) {
		DEBUG(1, ("Account %s for SID [%s] is not a trust account\n",
			  ldb_dn_get_linearized(res->msgs[0]->dn),
			  dom_sid_string(mem_ctx, sid)));
		return NT_STATUS_ACCESS_DENIED;
	}

	nt_hash = samdb_result_hash(mem_ctx, res->msgs[0], "unicodePwd");
	if (!nt_hash) {
		DEBUG(1, ("No unicodePwd found on record of SID %s "
			  "for NTP signing\n", dom_sid_string(mem_ctx, sid)));
		return signing_failure(ntp_signd_conn,
				       mem_ctx,
				       output,
				       sign_request.packet_id);
	}

	/* Generate the reply packet */
	signed_reply.packet_id = sign_request.packet_id;
	signed_reply.op = SIGNING_SUCCESS;
	signed_reply.signed_packet = data_blob_talloc(mem_ctx,
						      NULL,
						      sign_request.packet_to_sign.length + 20);

	if (!signed_reply.signed_packet.data) {
		return signing_failure(ntp_signd_conn,
				       mem_ctx,
				       output,
				       sign_request.packet_id);
	}

	memcpy(signed_reply.signed_packet.data, sign_request.packet_to_sign.data, sign_request.packet_to_sign.length);
	SIVAL(signed_reply.signed_packet.data, sign_request.packet_to_sign.length, sign_request.key_id);

	/* Sign the NTP response with the unicodePwd */
	MD5Init(&ctx);
	MD5Update(&ctx, nt_hash->hash, sizeof(nt_hash->hash));
	MD5Update(&ctx, sign_request.packet_to_sign.data, sign_request.packet_to_sign.length);
	MD5Final(signed_reply.signed_packet.data + sign_request.packet_to_sign.length + 4, &ctx);


	/* Place it into the packet for the wire */
	ndr_err = ndr_push_struct_blob(output, mem_ctx, &signed_reply,
				       (ndr_push_flags_fn_t)ndr_push_signed_reply);

	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DEBUG(1,("failed to push ntp error reply\n"));
		return ndr_map_error2ntstatus(ndr_err);
	}

	return NT_STATUS_OK;
}

/*
  called on a tcp recv
*/
static void ntp_signd_recv(struct stream_connection *conn, uint16_t flags)
{
	struct ntp_signd_connection *ntp_signd_conn = talloc_get_type(conn->private_data,
							struct ntp_signd_connection);
	ntp_signd_terminate_connection(ntp_signd_conn,
				       "ntp_signd_recv: called");
}

/*
  called when we can write to a connection
*/
static void ntp_signd_send(struct stream_connection *conn, uint16_t flags)
{
	struct ntp_signd_connection *ntp_signd_conn = talloc_get_type(conn->private_data,
							struct ntp_signd_connection);
	/* this should never be triggered! */
	ntp_signd_terminate_connection(ntp_signd_conn,
				       "ntp_signd_send: called");
}

struct ntp_signd_call {
	struct ntp_signd_connection *ntp_signd_conn;
	DATA_BLOB in;
	DATA_BLOB out;
	uint8_t out_hdr[4];
	struct iovec out_iov[2];
};

static void ntp_signd_call_writev_done(struct tevent_req *subreq);

static void ntp_signd_call_loop(struct tevent_req *subreq)
{
	struct ntp_signd_connection *ntp_signd_conn = tevent_req_callback_data(subreq,
				      struct ntp_signd_connection);
	struct ntp_signd_call *call;
	NTSTATUS status;

	call = talloc(ntp_signd_conn, struct ntp_signd_call);
	if (call == NULL) {
		ntp_signd_terminate_connection(ntp_signd_conn,
				"ntp_signd_call_loop: "
				"no memory for ntp_signd_call");
		return;
	}
	call->ntp_signd_conn = ntp_signd_conn;

	status = tstream_read_pdu_blob_recv(subreq,
					    call,
					    &call->in);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		const char *reason;

		reason = talloc_asprintf(call, "ntp_signd_call_loop: "
					 "tstream_read_pdu_blob_recv() - %s",
					 nt_errstr(status));
		if (reason == NULL) {
			reason = nt_errstr(status);
		}

		ntp_signd_terminate_connection(ntp_signd_conn, reason);
		return;
	}

	DEBUG(10,("Received NTP TCP packet of length %lu from %s\n",
		 (long) call->in.length,
		 tsocket_address_string(ntp_signd_conn->conn->remote_address, call)));

	/* skip length header */
	call->in.data +=4;
	call->in.length -= 4;

	status = ntp_signd_process(ntp_signd_conn,
				     call,
				     &call->in,
				     &call->out);
	if (! NT_STATUS_IS_OK(status)) {
		const char *reason;

		reason = talloc_asprintf(call, "ntp_signd_process failed: %s",
					 nt_errstr(status));
		if (reason == NULL) {
			reason = nt_errstr(status);
		}

		ntp_signd_terminate_connection(ntp_signd_conn, reason);
		return;
	}

	/* First add the length of the out buffer */
	RSIVAL(call->out_hdr, 0, call->out.length);
	call->out_iov[0].iov_base = (char *) call->out_hdr;
	call->out_iov[0].iov_len = 4;

	call->out_iov[1].iov_base = (char *) call->out.data;
	call->out_iov[1].iov_len = call->out.length;

	subreq = tstream_writev_queue_send(call,
					   ntp_signd_conn->conn->event.ctx,
					   ntp_signd_conn->tstream,
					   ntp_signd_conn->send_queue,
					   call->out_iov, 2);
	if (subreq == NULL) {
		ntp_signd_terminate_connection(ntp_signd_conn, "ntp_signd_call_loop: "
				"no memory for tstream_writev_queue_send");
		return;
	}

	tevent_req_set_callback(subreq, ntp_signd_call_writev_done, call);

	/*
	 * The NTP tcp pdu's has the length as 4 byte (initial_read_size),
	 * packet_full_request_u32 provides the pdu length then.
	 */
	subreq = tstream_read_pdu_blob_send(ntp_signd_conn,
					    ntp_signd_conn->conn->event.ctx,
					    ntp_signd_conn->tstream,
					    4, /* initial_read_size */
					    packet_full_request_u32,
					    ntp_signd_conn);
	if (subreq == NULL) {
		ntp_signd_terminate_connection(ntp_signd_conn, "ntp_signd_call_loop: "
				"no memory for tstream_read_pdu_blob_send");
		return;
	}
	tevent_req_set_callback(subreq, ntp_signd_call_loop, ntp_signd_conn);
}

static void ntp_signd_call_writev_done(struct tevent_req *subreq)
{
	struct ntp_signd_call *call = tevent_req_callback_data(subreq,
			struct ntp_signd_call);
	int sys_errno;
	int rc;

	rc = tstream_writev_queue_recv(subreq, &sys_errno);
	TALLOC_FREE(subreq);
	if (rc == -1) {
		const char *reason;

		reason = talloc_asprintf(call, "ntp_signd_call_writev_done: "
					 "tstream_writev_queue_recv() - %d:%s",
					 sys_errno, strerror(sys_errno));
		if (!reason) {
			reason = "ntp_signd_call_writev_done: "
				 "tstream_writev_queue_recv() failed";
		}

		ntp_signd_terminate_connection(call->ntp_signd_conn, reason);
		return;
	}

	/* We don't care about errors */

	talloc_free(call);
}

/*
  called when we get a new connection
*/
static void ntp_signd_accept(struct stream_connection *conn)
{
	struct ntp_signd_server *ntp_signd = talloc_get_type(conn->private_data,
						struct ntp_signd_server);
	struct ntp_signd_connection *ntp_signd_conn;
	struct tevent_req *subreq;
	int rc;

	ntp_signd_conn = talloc_zero(conn, struct ntp_signd_connection);
	if (ntp_signd_conn == NULL) {
		stream_terminate_connection(conn,
				"ntp_signd_accept: out of memory");
		return;
	}

	ntp_signd_conn->send_queue = tevent_queue_create(conn,
			"ntp_signd_accept");
	if (ntp_signd_conn->send_queue == NULL) {
		stream_terminate_connection(conn,
				"ntp_signd_accept: out of memory");
		return;
	}

	TALLOC_FREE(conn->event.fde);

	rc = tstream_bsd_existing_socket(ntp_signd_conn,
			socket_get_fd(conn->socket),
			&ntp_signd_conn->tstream);
	if (rc < 0) {
		stream_terminate_connection(conn,
				"ntp_signd_accept: out of memory");
		return;
	}

	ntp_signd_conn->conn = conn;
	ntp_signd_conn->ntp_signd = ntp_signd;
	conn->private_data = ntp_signd_conn;

	/*
	 * The NTP tcp pdu's has the length as 4 byte (initial_read_size),
	 * packet_full_request_u32 provides the pdu length then.
	 */
	subreq = tstream_read_pdu_blob_send(ntp_signd_conn,
					    ntp_signd_conn->conn->event.ctx,
					    ntp_signd_conn->tstream,
					    4, /* initial_read_size */
					    packet_full_request_u32,
					    ntp_signd_conn);
	if (subreq == NULL) {
		ntp_signd_terminate_connection(ntp_signd_conn,
				"ntp_signd_accept: "
				"no memory for tstream_read_pdu_blob_send");
		return;
	}
	tevent_req_set_callback(subreq, ntp_signd_call_loop, ntp_signd_conn);
}

static const struct stream_server_ops ntp_signd_stream_ops = {
	.name			= "ntp_signd",
	.accept_connection	= ntp_signd_accept,
	.recv_handler		= ntp_signd_recv,
	.send_handler		= ntp_signd_send
};

/*
  startup the ntp_signd task
*/
static void ntp_signd_task_init(struct task_server *task)
{
	struct ntp_signd_server *ntp_signd;
	NTSTATUS status;

	const struct model_ops *model_ops;

	const char *address;

	if (!directory_create_or_exist(lpcfg_ntp_signd_socket_directory(task->lp_ctx), geteuid(), 0755)) {
		char *error = talloc_asprintf(task, "Cannot create NTP signd pipe directory: %s", 
					      lpcfg_ntp_signd_socket_directory(task->lp_ctx));
		task_server_terminate(task,
				      error, true);
		return;
	}

	/* within the ntp_signd task we want to be a single process, so
	   ask for the single process model ops and pass these to the
	   stream_setup_socket() call. */
	model_ops = process_model_startup("single");
	if (!model_ops) {
		DEBUG(0,("Can't find 'single' process model_ops\n"));
		return;
	}

	task_server_set_title(task, "task[ntp_signd]");

	ntp_signd = talloc(task, struct ntp_signd_server);
	if (ntp_signd == NULL) {
		task_server_terminate(task, "ntp_signd: out of memory", true);
		return;
	}

	ntp_signd->task = task;

	/* Must be system to get at the password hashes */
	ntp_signd->samdb = samdb_connect(ntp_signd, task->event_ctx, task->lp_ctx, system_session(task->lp_ctx), 0);
	if (ntp_signd->samdb == NULL) {
		task_server_terminate(task, "ntp_signd failed to open samdb", true);
		return;
	}

	address = talloc_asprintf(ntp_signd, "%s/socket", lpcfg_ntp_signd_socket_directory(task->lp_ctx));

	status = stream_setup_socket(ntp_signd->task,
				     ntp_signd->task->event_ctx,
				     ntp_signd->task->lp_ctx,
				     model_ops, 
				     &ntp_signd_stream_ops, 
				     "unix", address, NULL,
				     lpcfg_socket_options(ntp_signd->task->lp_ctx),
				     ntp_signd);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("Failed to bind to %s - %s\n",
			 address, nt_errstr(status)));
		return;
	}

}


/* called at smbd startup - register ourselves as a server service */
NTSTATUS server_service_ntp_signd_init(void)
{
	return register_server_service("ntp_signd", ntp_signd_task_init);
}
