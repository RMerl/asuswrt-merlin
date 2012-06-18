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
#include "librpc/gen_ndr/ndr_ntp_signd.h"
#include "param/param.h"
#include "dsdb/samdb/samdb.h"
#include "auth/auth.h"
#include "libcli/security/security.h"
#include "lib/ldb/include/ldb.h"
#include "lib/ldb/include/ldb_errors.h"
#include "../lib/crypto/md5.h"
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

	struct packet_context *packet;
};

static void ntp_signd_terminate_connection(struct ntp_signd_connection *ntp_signdconn, const char *reason)
{
	stream_terminate_connection(ntp_signdconn->conn, reason);
}

static NTSTATUS signing_failure(struct ntp_signd_connection *ntp_signdconn,
				uint32_t packet_id) 
{
	NTSTATUS status;
	struct signed_reply signed_reply;
	TALLOC_CTX *tmp_ctx = talloc_new(ntp_signdconn);
	DATA_BLOB reply, blob;
	enum ndr_err_code ndr_err;

	NT_STATUS_HAVE_NO_MEMORY(tmp_ctx);

	signed_reply.op = SIGNING_FAILURE;
	signed_reply.packet_id = packet_id;
	signed_reply.signed_packet = data_blob(NULL, 0);
	
	ndr_err = ndr_push_struct_blob(&reply, tmp_ctx, 
				       lp_iconv_convenience(ntp_signdconn->ntp_signd->task->lp_ctx),
				       &signed_reply,
				       (ndr_push_flags_fn_t)ndr_push_signed_reply);

	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DEBUG(1,("failed to push ntp error reply\n"));
		talloc_free(tmp_ctx);
		return ndr_map_error2ntstatus(ndr_err);
	}

	blob = data_blob_talloc(ntp_signdconn, NULL, reply.length + 4);
	if (!blob.data) {
		talloc_free(tmp_ctx);
		return NT_STATUS_NO_MEMORY;
	}

	RSIVAL(blob.data, 0, reply.length);
	memcpy(blob.data + 4, reply.data, reply.length);	

	status = packet_send(ntp_signdconn->packet, blob);

	/* the call isn't needed any more */
	talloc_free(tmp_ctx);
	
	return status;
}

/*
  receive a full packet on a NTP_SIGND connection
*/
static NTSTATUS ntp_signd_recv(void *private_data, DATA_BLOB wrapped_input)
{
	struct ntp_signd_connection *ntp_signdconn = talloc_get_type(private_data,
							     struct ntp_signd_connection);
	NTSTATUS status = NT_STATUS_UNSUCCESSFUL;
	TALLOC_CTX *tmp_ctx = talloc_new(ntp_signdconn);
	DATA_BLOB input, output, wrapped_output;
	const struct dom_sid *domain_sid;
	struct dom_sid *sid;
	struct sign_request sign_request;
	struct signed_reply signed_reply;
	enum ndr_err_code ndr_err;
	struct ldb_result *res;
	const char *attrs[] = { "unicodePwd", "userAccountControl", "cn", NULL };
	struct MD5Context ctx;
	struct samr_Password *nt_hash;
	uint32_t user_account_control;
	int ret;

	NT_STATUS_HAVE_NO_MEMORY(tmp_ctx);

	talloc_steal(tmp_ctx, wrapped_input.data);

	input = data_blob_const(wrapped_input.data + 4, wrapped_input.length - 4); 

	ndr_err = ndr_pull_struct_blob_all(&input, tmp_ctx, 
					   lp_iconv_convenience(ntp_signdconn->ntp_signd->task->lp_ctx),
					   &sign_request,
					   (ndr_pull_flags_fn_t)ndr_pull_sign_request);

	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DEBUG(1,("failed to parse ntp signing request\n"));
		dump_data(1, input.data, input.length);
		return ndr_map_error2ntstatus(ndr_err);
	}

	/* We need to implement 'check signature' and 'request server
	 * to sign' operations at some point */
	if (sign_request.op != SIGN_TO_CLIENT) {
		talloc_free(tmp_ctx);
		return signing_failure(ntp_signdconn, sign_request.packet_id);
	}

	/* We need to implement 'check signature' and 'request server
	 * to sign' operations at some point */
	if (sign_request.version != NTP_SIGND_PROTOCOL_VERSION_0) {
		talloc_free(tmp_ctx);
		return signing_failure(ntp_signdconn, sign_request.packet_id);
	}

	domain_sid = samdb_domain_sid(ntp_signdconn->ntp_signd->samdb);
	if (!domain_sid) {
		talloc_free(tmp_ctx);
		return signing_failure(ntp_signdconn, sign_request.packet_id);
	}
	
	/* The top bit is a 'key selector' */
	sid = dom_sid_add_rid(tmp_ctx, domain_sid, sign_request.key_id & 0x7FFFFFFF);
	if (!sid) {
		talloc_free(tmp_ctx);
		return signing_failure(ntp_signdconn, sign_request.packet_id);
	}

	ret = ldb_search(ntp_signdconn->ntp_signd->samdb, tmp_ctx,
				 &res, samdb_base_dn(ntp_signdconn->ntp_signd->samdb),
				 LDB_SCOPE_SUBTREE, attrs, "(&(objectSid=%s)(objectClass=user))",
				 dom_sid_string(tmp_ctx, sid));
	if (ret != LDB_SUCCESS) {
		DEBUG(2, ("Failed to search for SID %s in SAM for NTP signing: %s\n", dom_sid_string(tmp_ctx, sid),
			  ldb_errstring(ntp_signdconn->ntp_signd->samdb)));
		talloc_free(tmp_ctx);
		return signing_failure(ntp_signdconn, sign_request.packet_id);
	}

	if (res->count == 0) {
		DEBUG(5, ("Failed to find SID %s in SAM for NTP signing\n", dom_sid_string(tmp_ctx, sid)));
	} else if (res->count != 1) {
		DEBUG(1, ("Found SID %s %u times in SAM for NTP signing\n", dom_sid_string(tmp_ctx, sid), res->count));
		talloc_free(tmp_ctx);
		return signing_failure(ntp_signdconn, sign_request.packet_id);
	}

	user_account_control = ldb_msg_find_attr_as_uint(res->msgs[0], "userAccountControl", 0);

	if (user_account_control & UF_ACCOUNTDISABLE) {
		DEBUG(1, ("Account %s for SID [%s] is disabled\n", ldb_dn_get_linearized(res->msgs[0]->dn), dom_sid_string(tmp_ctx, sid)));
		talloc_free(tmp_ctx);
		return NT_STATUS_ACCESS_DENIED;
	}

	if (!(user_account_control & (UF_INTERDOMAIN_TRUST_ACCOUNT|UF_SERVER_TRUST_ACCOUNT|UF_WORKSTATION_TRUST_ACCOUNT))) {
		DEBUG(1, ("Account %s for SID [%s] is not a trust account\n", ldb_dn_get_linearized(res->msgs[0]->dn), dom_sid_string(tmp_ctx, sid)));
		talloc_free(tmp_ctx);
		return NT_STATUS_ACCESS_DENIED;
	}

	nt_hash = samdb_result_hash(tmp_ctx, res->msgs[0], "unicodePwd");
	if (!nt_hash) {
		DEBUG(1, ("No unicodePwd found on record of SID %s for NTP signing\n", dom_sid_string(tmp_ctx, sid)));
		talloc_free(tmp_ctx);
		return signing_failure(ntp_signdconn, sign_request.packet_id);
	}

	/* Generate the reply packet */
	signed_reply.packet_id = sign_request.packet_id;
	signed_reply.op = SIGNING_SUCCESS;
	signed_reply.signed_packet = data_blob_talloc(tmp_ctx, 
						      NULL,
						      sign_request.packet_to_sign.length + 20);

	if (!signed_reply.signed_packet.data) {
		talloc_free(tmp_ctx);
		return signing_failure(ntp_signdconn, sign_request.packet_id);
	}

	memcpy(signed_reply.signed_packet.data, sign_request.packet_to_sign.data, sign_request.packet_to_sign.length);
	SIVAL(signed_reply.signed_packet.data, sign_request.packet_to_sign.length, sign_request.key_id);

	/* Sign the NTP response with the unicodePwd */
	MD5Init(&ctx);
	MD5Update(&ctx, nt_hash->hash, sizeof(nt_hash->hash));
	MD5Update(&ctx, sign_request.packet_to_sign.data, sign_request.packet_to_sign.length);
	MD5Final(signed_reply.signed_packet.data + sign_request.packet_to_sign.length + 4, &ctx);


	/* Place it into the packet for the wire */
	ndr_err = ndr_push_struct_blob(&output, tmp_ctx, 
				       lp_iconv_convenience(ntp_signdconn->ntp_signd->task->lp_ctx),
				       &signed_reply,
				       (ndr_push_flags_fn_t)ndr_push_signed_reply);

	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DEBUG(1,("failed to push ntp error reply\n"));
		talloc_free(tmp_ctx);
		return ndr_map_error2ntstatus(ndr_err);
	}

	wrapped_output = data_blob_talloc(ntp_signdconn, NULL, output.length + 4);
	if (!wrapped_output.data) {
		talloc_free(tmp_ctx);
		return NT_STATUS_NO_MEMORY;
	}

	/* The 'wire' transport for this is wrapped with a 4 byte network byte order length */
	RSIVAL(wrapped_output.data, 0, output.length);
	memcpy(wrapped_output.data + 4, output.data, output.length);	

	status = packet_send(ntp_signdconn->packet, wrapped_output);

	/* the call isn't needed any more */
	talloc_free(tmp_ctx);
	return status;
}

/*
  receive some data on a NTP_SIGND connection
*/
static void ntp_signd_recv_handler(struct stream_connection *conn, uint16_t flags)
{
	struct ntp_signd_connection *ntp_signdconn = talloc_get_type(conn->private_data,
							     struct ntp_signd_connection);
	packet_recv(ntp_signdconn->packet);
}

/*
  called on a tcp recv error
*/
static void ntp_signd_recv_error(void *private_data, NTSTATUS status)
{
	struct ntp_signd_connection *ntp_signdconn = talloc_get_type(private_data, struct ntp_signd_connection);
	ntp_signd_terminate_connection(ntp_signdconn, nt_errstr(status));
}

/*
  called when we can write to a connection
*/
static void ntp_signd_send(struct stream_connection *conn, uint16_t flags)
{
	struct ntp_signd_connection *ntp_signdconn = talloc_get_type(conn->private_data,
							     struct ntp_signd_connection);
	packet_queue_run(ntp_signdconn->packet);
}

/*
  called when we get a new connection
*/
static void ntp_signd_accept(struct stream_connection *conn)
{
	struct ntp_signd_server *ntp_signd = talloc_get_type(conn->private_data, struct ntp_signd_server);
	struct ntp_signd_connection *ntp_signdconn;

	ntp_signdconn = talloc_zero(conn, struct ntp_signd_connection);
	if (!ntp_signdconn) {
		stream_terminate_connection(conn, "ntp_signd_accept: out of memory");
		return;
	}
	ntp_signdconn->conn	 = conn;
	ntp_signdconn->ntp_signd	 = ntp_signd;
	conn->private_data    = ntp_signdconn;

	ntp_signdconn->packet = packet_init(ntp_signdconn);
	if (ntp_signdconn->packet == NULL) {
		ntp_signd_terminate_connection(ntp_signdconn, "ntp_signd_accept: out of memory");
		return;
	}
	packet_set_private(ntp_signdconn->packet, ntp_signdconn);
	packet_set_socket(ntp_signdconn->packet, conn->socket);
	packet_set_callback(ntp_signdconn->packet, ntp_signd_recv);
	packet_set_full_request(ntp_signdconn->packet, packet_full_request_u32);
	packet_set_error_handler(ntp_signdconn->packet, ntp_signd_recv_error);
	packet_set_event_context(ntp_signdconn->packet, conn->event.ctx);
	packet_set_fde(ntp_signdconn->packet, conn->event.fde);
	packet_set_serialise(ntp_signdconn->packet);
}

static const struct stream_server_ops ntp_signd_stream_ops = {
	.name			= "ntp_signd",
	.accept_connection	= ntp_signd_accept,
	.recv_handler		= ntp_signd_recv_handler,
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

	if (!directory_create_or_exist(lp_ntp_signd_socket_directory(task->lp_ctx), geteuid(), 0755)) {
		char *error = talloc_asprintf(task, "Cannot create NTP signd pipe directory: %s", 
					      lp_ntp_signd_socket_directory(task->lp_ctx));
		task_server_terminate(task,
				      error, true);
		return;
	}

	/* within the ntp_signd task we want to be a single process, so
	   ask for the single process model ops and pass these to the
	   stream_setup_socket() call. */
	model_ops = process_model_startup(task->event_ctx, "single");
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
	ntp_signd->samdb = samdb_connect(ntp_signd, task->event_ctx, task->lp_ctx, system_session(ntp_signd, task->lp_ctx));
	if (ntp_signd->samdb == NULL) {
		task_server_terminate(task, "ntp_signd failed to open samdb", true);
		return;
	}

	address = talloc_asprintf(ntp_signd, "%s/socket", lp_ntp_signd_socket_directory(task->lp_ctx));

	status = stream_setup_socket(ntp_signd->task->event_ctx, 
				     ntp_signd->task->lp_ctx,
				     model_ops, 
				     &ntp_signd_stream_ops, 
				     "unix", address, NULL,
				     lp_socket_options(ntp_signd->task->lp_ctx), 
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
