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
#include "torture/torture.h"
#include "torture/smbtorture.h"
#include <tevent.h>
#include "lib/socket/socket.h"
#include "lib/stream/packet.h"
#include "auth/credentials/credentials.h"
#include "torture/rpc/rpc.h"
#include "torture/rpc/netlogon.h"
#include "../lib/crypto/crypto.h"
#include "libcli/auth/libcli_auth.h"
#include "librpc/gen_ndr/ndr_netlogon_c.h"
#include "librpc/gen_ndr/ndr_netlogon.h"
#include "librpc/gen_ndr/ndr_ntp_signd.h"
#include "param/param.h"

#define TEST_MACHINE_NAME "ntpsigndtest"

struct signd_client_socket {
	struct socket_context *sock;
	
	/* the fd event */
	struct tevent_fd *fde;
	
	NTSTATUS status;
	DATA_BLOB request, reply;
	
	struct packet_context *packet;
		
	size_t partial_read;
};

static NTSTATUS signd_client_full_packet(void *private_data, DATA_BLOB data)
{
	struct signd_client_socket *signd_client = talloc_get_type(private_data, struct signd_client_socket);
	talloc_steal(signd_client, data.data);
	signd_client->reply = data;
	signd_client->reply.length -= 4;
	signd_client->reply.data += 4;
	return NT_STATUS_OK;
}

static void signd_client_error_handler(void *private_data, NTSTATUS status)
{
	struct signd_client_socket *signd_client = talloc_get_type(private_data, struct signd_client_socket);
	signd_client->status = status;
}

/*
  handle fd events on a signd_client_socket
*/
static void signd_client_socket_handler(struct tevent_context *ev, struct tevent_fd *fde,
				 uint16_t flags, void *private_data)
{
	struct signd_client_socket *signd_client = talloc_get_type(private_data, struct signd_client_socket);
	if (flags & TEVENT_FD_READ) {
		packet_recv(signd_client->packet);
		return;
	}
	if (flags & TEVENT_FD_WRITE) {
		packet_queue_run(signd_client->packet);
		return;
	}
	/* not reached */
	return;
}

/* A torture test to show that the unix domain socket protocol is
 * operating correctly, and the signatures are as expected */

static bool test_ntp_signd(struct torture_context *tctx, 
			   struct dcerpc_pipe *p,
			   struct cli_credentials *credentials)
{
	struct netlogon_creds_CredentialState *creds;
	TALLOC_CTX *mem_ctx = talloc_new(tctx);

	NTSTATUS status;
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

	struct signd_client_socket *signd_client;
	char *signd_socket_address;

	struct MD5Context ctx;
	uint8_t sig[16];
	enum ndr_err_code ndr_err;

	machine_name = cli_credentials_get_workstation(credentials);

	torture_comment(tctx, "Testing ServerReqChallenge\n");

	r.in.server_name = NULL;
	r.in.computer_name = machine_name;
	r.in.credentials = &credentials1;
	r.out.return_credentials = &credentials2;

	generate_random_buffer(credentials1.data, sizeof(credentials1.data));

	status = dcerpc_netr_ServerReqChallenge(p, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "ServerReqChallenge");

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

	status = dcerpc_netr_ServerAuthenticate3(p, tctx, &a);
	torture_assert_ntstatus_ok(tctx, status, "ServerAuthenticate3");
	torture_assert(tctx, netlogon_creds_client_check(creds, &credentials3), "Credential chaining failed");

	sign_req.op = SIGN_TO_CLIENT;
	sign_req.packet_id = 1;
	sign_req.key_id = rid;
	sign_req.packet_to_sign = data_blob_string_const("I am a tea pot");
	
	ndr_err = ndr_push_struct_blob(&sign_req_blob, mem_ctx, NULL, &sign_req, (ndr_push_flags_fn_t)ndr_push_sign_request);
	torture_assert(tctx, NDR_ERR_CODE_IS_SUCCESS(ndr_err), "Failed to push sign_req");
	
	signd_client = talloc(mem_ctx, struct signd_client_socket);

	status = socket_create("unix", SOCKET_TYPE_STREAM, &signd_client->sock, 0);
	
	signd_socket_address = talloc_asprintf(signd_client, "%s/socket", 
					       lp_ntp_signd_socket_directory(tctx->lp_ctx));

	status = socket_connect_ev(signd_client->sock, NULL, 
				   socket_address_from_strings(signd_client, 
							       "unix", signd_socket_address, 0), 0, tctx->ev);
	torture_assert_ntstatus_ok(tctx, status, "Failed to connect to signd!");
	
	/* Setup the FDE, start listening for read events
	 * from the start (otherwise we may miss a socket
	 * drop) and mark as AUTOCLOSE along with the fde */
	
	/* Ths is equivilant to EVENT_FD_READABLE(signd_client->fde) */
	signd_client->fde = tevent_add_fd(tctx->ev, signd_client->sock,
			    socket_get_fd(signd_client->sock),
			    TEVENT_FD_READ,
			    signd_client_socket_handler, signd_client);
	/* its now the job of the event layer to close the socket */
	tevent_fd_set_close_fn(signd_client->fde, socket_tevent_fd_close_fn);
	socket_set_flags(signd_client->sock, SOCKET_FLAG_NOCLOSE);
	
	signd_client->status = NT_STATUS_OK;
	signd_client->reply = data_blob(NULL, 0);

	signd_client->packet = packet_init(signd_client);
	if (signd_client->packet == NULL) {
		talloc_free(signd_client);
		return ENOMEM;
	}
	packet_set_private(signd_client->packet, signd_client);
	packet_set_socket(signd_client->packet, signd_client->sock);
	packet_set_callback(signd_client->packet, signd_client_full_packet);
	packet_set_full_request(signd_client->packet, packet_full_request_u32);
	packet_set_error_handler(signd_client->packet, signd_client_error_handler);
	packet_set_event_context(signd_client->packet, tctx->ev);
	packet_set_fde(signd_client->packet, signd_client->fde);
	
	signd_client->request = data_blob_talloc(signd_client, NULL, sign_req_blob.length + 4);
	RSIVAL(signd_client->request.data, 0, sign_req_blob.length);
	memcpy(signd_client->request.data+4, sign_req_blob.data, sign_req_blob.length);
	packet_send(signd_client->packet, signd_client->request);

	while ((NT_STATUS_IS_OK(signd_client->status)) && !signd_client->reply.length) {
		if (tevent_loop_once(tctx->ev) != 0) {
			talloc_free(signd_client);
			return EINVAL;
		}
	}

	torture_assert_ntstatus_ok(tctx, signd_client->status, "Error reading signd_client reply packet");

	ndr_err = ndr_pull_struct_blob_all(&signd_client->reply, mem_ctx, 
					   lp_iconv_convenience(tctx->lp_ctx),
					   &signed_reply,
					   (ndr_pull_flags_fn_t)ndr_pull_signed_reply);
	torture_assert(tctx, NDR_ERR_CODE_IS_SUCCESS(ndr_err), ndr_map_error2string(ndr_err));

	torture_assert_u64_equal(tctx, signed_reply.version, 
				 NTP_SIGND_PROTOCOL_VERSION_0, "Invalid Version");
	torture_assert_u64_equal(tctx, signed_reply.packet_id, 
				 sign_req.packet_id, "Invalid Packet ID");
	torture_assert_u64_equal(tctx, signed_reply.op, 
				 SIGNING_SUCCESS, "Should have replied with signing success");
	torture_assert_u64_equal(tctx, signed_reply.signed_packet.length, 
				 sign_req.packet_to_sign.length + 20, "Invalid reply length from signd");
	torture_assert_u64_equal(tctx, rid, 
				 IVAL(signed_reply.signed_packet.data, sign_req.packet_to_sign.length), 
				 "Incorrect RID in reply");

	/* Check computed signature */

	MD5Init(&ctx);
	MD5Update(&ctx, pwhash->hash, sizeof(pwhash->hash));
	MD5Update(&ctx, sign_req.packet_to_sign.data, sign_req.packet_to_sign.length);
	MD5Final(sig, &ctx);

	torture_assert_mem_equal(tctx, &signed_reply.signed_packet.data[sign_req.packet_to_sign.length + 4],
				 sig, 16, "Signature on reply was incorrect!");
	
	talloc_free(mem_ctx);
		
	return true;
}

NTSTATUS torture_ntp_init(void)
{
	struct torture_suite *suite = torture_suite_create(talloc_autofree_context(), "NTP");
	struct torture_rpc_tcase *tcase;

	tcase = torture_suite_add_machine_workstation_rpc_iface_tcase(suite, "SIGND",
								      &ndr_table_netlogon, TEST_MACHINE_NAME);

	torture_rpc_tcase_add_test_creds(tcase, "ntp_signd", test_ntp_signd);

	suite->description = talloc_strdup(suite, "NTP tests");

	torture_register_suite(suite);

	return NT_STATUS_OK;
}

