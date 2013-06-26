/* 
   Unix SMB/CIFS implementation.

   Test NTP authentication support

   Copyright (C) Andrew Bartlet <abartlet@samba.org> 2008
   
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
#include "torture/smbtorture.h"
#include <tevent.h>
#include "lib/stream/packet.h"
#include "lib/tsocket/tsocket.h"
#include "libcli/util/tstream.h"
#include "torture/rpc/torture_rpc.h"
#include "../lib/crypto/crypto.h"
#include "libcli/auth/libcli_auth.h"
#include "librpc/gen_ndr/ndr_netlogon_c.h"
#include "librpc/gen_ndr/ndr_ntp_signd.h"
#include "param/param.h"
#include "system/network.h"

#define TEST_MACHINE_NAME "ntpsigndtest"

struct signd_client_state {
	struct tsocket_address *local_address;
	struct tsocket_address *remote_address;

	struct tstream_context *tstream;
	struct tevent_queue *send_queue;

	uint8_t request_hdr[4];
	struct iovec request_iov[2];

	DATA_BLOB reply;

	NTSTATUS status;
};

/*
 * A torture test to show that the unix domain socket protocol is
 * operating correctly, and the signatures are as expected
 */
static bool test_ntp_signd(struct torture_context *tctx,
			   struct dcerpc_pipe *p,
			   struct cli_credentials *credentials)
{
	struct netlogon_creds_CredentialState *creds;
	TALLOC_CTX *mem_ctx = talloc_new(tctx);

	struct netr_ServerReqChallenge r;
	struct netr_ServerAuthenticate3 a;
	struct netr_Credential credentials1, credentials2, credentials3;
	uint32_t rid;
	const char *machine_name;
	const struct samr_Password *pwhash = cli_credentials_get_nt_hash(credentials, mem_ctx);
	uint32_t negotiate_flags = NETLOGON_NEG_AUTH2_ADS_FLAGS;

	struct sign_request sign_req;
	struct signed_reply signed_reply;
	DATA_BLOB sign_req_blob;

	struct signd_client_state *signd_client;
	struct tevent_req *req;
	char *unix_address;
	int sys_errno;

	MD5_CTX ctx;
	uint8_t sig[16];
	enum ndr_err_code ndr_err;
	bool ok;
	int rc;

	machine_name = cli_credentials_get_workstation(credentials);

	torture_comment(tctx, "Testing ServerReqChallenge\n");

	r.in.server_name = NULL;
	r.in.computer_name = machine_name;
	r.in.credentials = &credentials1;
	r.out.return_credentials = &credentials2;

	generate_random_buffer(credentials1.data, sizeof(credentials1.data));

	torture_assert_ntstatus_ok(tctx,
		dcerpc_netr_ServerReqChallenge_r(p->binding_handle, tctx, &r),
		"ServerReqChallenge failed");
	torture_assert_ntstatus_ok(tctx, r.out.result,
		"ServerReqChallenge failed");

	a.in.server_name = NULL;
	a.in.account_name = talloc_asprintf(tctx, "%s$", machine_name);
	a.in.secure_channel_type = SEC_CHAN_WKSTA;
	a.in.computer_name = machine_name;
	a.in.negotiate_flags = &negotiate_flags;
	a.in.credentials = &credentials3;
	a.out.return_credentials = &credentials3;
	a.out.negotiate_flags = &negotiate_flags;
	a.out.rid = &rid;

	creds = netlogon_creds_client_init(tctx, a.in.account_name,
					   a.in.computer_name,
					   &credentials1, &credentials2, 
					   pwhash, &credentials3,
					   negotiate_flags);
	
	torture_assert(tctx, creds != NULL, "memory allocation");

	torture_comment(tctx, "Testing ServerAuthenticate3\n");

	torture_assert_ntstatus_ok(tctx,
		dcerpc_netr_ServerAuthenticate3_r(p->binding_handle, tctx, &a),
		"ServerAuthenticate3 failed");
	torture_assert_ntstatus_ok(tctx, a.out.result,
		"ServerAuthenticate3 failed");
	torture_assert(tctx,
		       netlogon_creds_client_check(creds, &credentials3),
		       "Credential chaining failed");

	sign_req.op = SIGN_TO_CLIENT;
	sign_req.packet_id = 1;
	sign_req.key_id = rid;
	sign_req.packet_to_sign = data_blob_string_const("I am a tea pot");
	
	ndr_err = ndr_push_struct_blob(&sign_req_blob,
				       mem_ctx,
				       &sign_req,
				       (ndr_push_flags_fn_t)ndr_push_sign_request);
	torture_assert(tctx,
		       NDR_ERR_CODE_IS_SUCCESS(ndr_err),
		       "Failed to push sign_req");

	signd_client = talloc(mem_ctx, struct signd_client_state);

	/* Create socket addresses */
	torture_comment(tctx, "Creating the socket addresses\n");
	rc = tsocket_address_unix_from_path(signd_client, "",
				       &signd_client->local_address);
	torture_assert(tctx, rc == 0,
		       "Failed to create local address from unix path.");

	unix_address = talloc_asprintf(signd_client,
					"%s/socket",
					lpcfg_ntp_signd_socket_directory(tctx->lp_ctx));
	rc = tsocket_address_unix_from_path(mem_ctx,
					    unix_address,
					    &signd_client->remote_address);
	torture_assert(tctx, rc == 0,
		       "Failed to create remote address from unix path.");

	/* Connect to the unix socket */
	torture_comment(tctx, "Connecting to the unix socket\n");
	req = tstream_unix_connect_send(signd_client,
					tctx->ev,
					signd_client->local_address,
					signd_client->remote_address);
	torture_assert(tctx, req != NULL,
		       "Failed to create a tstream unix connect request.");

	ok = tevent_req_poll(req, tctx->ev);
	torture_assert(tctx, ok == true,
		       "Failed to poll for tstream_unix_connect_send.");

	rc = tstream_unix_connect_recv(req,
				       &sys_errno,
				       signd_client,
				       &signd_client->tstream);
	TALLOC_FREE(req);
	torture_assert(tctx, rc == 0, "Failed to connect to signd!");

	/* Allocate the send queue */
	signd_client->send_queue = tevent_queue_create(signd_client,
						       "signd_client_queue");
	torture_assert(tctx, signd_client->send_queue != NULL,
		       "Failed to create send queue!");

	/*
	 * Create the request buffer.
	 * First add the length of the request buffer
	 */
	RSIVAL(signd_client->request_hdr, 0, sign_req_blob.length);
	signd_client->request_iov[0].iov_base = (char *) signd_client->request_hdr;
	signd_client->request_iov[0].iov_len = 4;

	signd_client->request_iov[1].iov_base = (char *) sign_req_blob.data;
	signd_client->request_iov[1].iov_len = sign_req_blob.length;

	/* Fire the request buffer */
	torture_comment(tctx, "Sending the request\n");
	req = tstream_writev_queue_send(signd_client,
					tctx->ev,
					signd_client->tstream,
					signd_client->send_queue,
					signd_client->request_iov, 2);
	torture_assert(tctx, req != NULL,
		       "Failed to send the signd request.");

	ok = tevent_req_poll(req, tctx->ev);
	torture_assert(tctx, ok == true,
		       "Failed to poll for tstream_writev_queue_send.");

	rc = tstream_writev_queue_recv(req, &sys_errno);
	TALLOC_FREE(req);
	torture_assert(tctx, rc > 0, "Failed to send data");

	/* Wait for a reply */
	torture_comment(tctx, "Waiting for the reply\n");
	req = tstream_read_pdu_blob_send(signd_client,
					 tctx->ev,
					 signd_client->tstream,
					 4, /*initial_read_size */
					 packet_full_request_u32,
					 NULL);
	torture_assert(tctx, req != NULL,
		       "Failed to setup a read for pdu_blob.");

	ok = tevent_req_poll(req, tctx->ev);
	torture_assert(tctx, ok == true,
		       "Failed to poll for tstream_read_pdu_blob_send.");

	signd_client->status = tstream_read_pdu_blob_recv(req,
							  signd_client,
							  &signd_client->reply);
	torture_assert_ntstatus_ok(tctx, signd_client->status,
				   "Error reading signd_client reply packet");

	/* Skip length header */
	signd_client->reply.data += 4;
	signd_client->reply.length -= 4;

	/* Check if the reply buffer is valid */
	torture_comment(tctx, "Validating the reply buffer\n");
	ndr_err = ndr_pull_struct_blob_all(&signd_client->reply,
					   mem_ctx,
					   &signed_reply,
					   (ndr_pull_flags_fn_t)ndr_pull_signed_reply);
	torture_assert(tctx, NDR_ERR_CODE_IS_SUCCESS(ndr_err),
			ndr_map_error2string(ndr_err));

	torture_assert_u64_equal(tctx, signed_reply.version,
				 NTP_SIGND_PROTOCOL_VERSION_0,
				 "Invalid Version");
	torture_assert_u64_equal(tctx, signed_reply.packet_id,
				 sign_req.packet_id, "Invalid Packet ID");
	torture_assert_u64_equal(tctx, signed_reply.op,
				 SIGNING_SUCCESS,
				 "Should have replied with signing success");
	torture_assert_u64_equal(tctx, signed_reply.signed_packet.length,
				 sign_req.packet_to_sign.length + 20,
				 "Invalid reply length from signd");
	torture_assert_u64_equal(tctx, rid,
				 IVAL(signed_reply.signed_packet.data,
				 sign_req.packet_to_sign.length),
				 "Incorrect RID in reply");

	/* Check computed signature */
	MD5Init(&ctx);
	MD5Update(&ctx, pwhash->hash, sizeof(pwhash->hash));
	MD5Update(&ctx, sign_req.packet_to_sign.data,
		  sign_req.packet_to_sign.length);
	MD5Final(sig, &ctx);

	torture_assert_mem_equal(tctx,
				 &signed_reply.signed_packet.data[sign_req.packet_to_sign.length + 4],
				 sig, 16, "Signature on reply was incorrect!");

	talloc_free(mem_ctx);

	return true;
}

NTSTATUS torture_ntp_init(void)
{
	struct torture_suite *suite = torture_suite_create(talloc_autofree_context(), "ntp");
	struct torture_rpc_tcase *tcase;

	tcase = torture_suite_add_machine_workstation_rpc_iface_tcase(suite,
				  "signd", &ndr_table_netlogon, TEST_MACHINE_NAME);

	torture_rpc_tcase_add_test_creds(tcase, "ntp_signd", test_ntp_signd);

	suite->description = talloc_strdup(suite, "NTP tests");

	torture_register_suite(suite);

	return NT_STATUS_OK;
}

