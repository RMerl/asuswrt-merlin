/* 
   Unix SMB/CIFS implementation.
   negprot reply code
   Copyright (C) Andrew Tridgell 1992-1998
   
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
#include "auth/credentials/credentials.h"
#include "auth/gensec/gensec.h"
#include "auth/auth.h"
#include "smb_server/smb_server.h"
#include "libcli/smb2/smb2.h"
#include "smb_server/smb2/smb2_server.h"
#include "smbd/service_stream.h"
#include "lib/stream/packet.h"
#include "param/param.h"


/* initialise the auth_context for this server and return the cryptkey */
static NTSTATUS get_challenge(struct smbsrv_connection *smb_conn, uint8_t buff[8]) 
{
	NTSTATUS nt_status;

	/* muliple negprots are not premitted */
	if (smb_conn->negotiate.auth_context) {
		DEBUG(3,("get challenge: is this a secondary negprot?  auth_context is non-NULL!\n"));
		return NT_STATUS_FOOBAR;
	}

	DEBUG(10, ("get challenge: creating negprot_global_auth_context\n"));

	nt_status = auth_context_create(smb_conn, 
					smb_conn->connection->event.ctx,
					smb_conn->connection->msg_ctx,
					smb_conn->lp_ctx,
					&smb_conn->negotiate.auth_context);
	if (!NT_STATUS_IS_OK(nt_status)) {
		DEBUG(0, ("auth_context_create() returned %s", nt_errstr(nt_status)));
		return nt_status;
	}

	nt_status = auth_get_challenge(smb_conn->negotiate.auth_context, buff);
	if (!NT_STATUS_IS_OK(nt_status)) {
		DEBUG(0, ("auth_get_challenge() returned %s", nt_errstr(nt_status)));
		return nt_status;
	}

	return NT_STATUS_OK;
}

/****************************************************************************
 Reply for the core protocol.
****************************************************************************/
static void reply_corep(struct smbsrv_request *req, uint16_t choice)
{
	smbsrv_setup_reply(req, 1, 0);

	SSVAL(req->out.vwv, VWV(0), choice);

	req->smb_conn->negotiate.protocol = PROTOCOL_CORE;

	if (req->smb_conn->signing.mandatory_signing) {
		smbsrv_terminate_connection(req->smb_conn, 
					    "CORE does not support SMB signing, and it is mandatory\n");
		return;
	}

	smbsrv_send_reply(req);
}

/****************************************************************************
 Reply for the coreplus protocol.
this is quite incomplete - we only fill in a small part of the reply, but as nobody uses
this any more it probably doesn't matter
****************************************************************************/
static void reply_coreplus(struct smbsrv_request *req, uint16_t choice)
{
	uint16_t raw = (lpcfg_readraw(req->smb_conn->lp_ctx)?1:0) | (lpcfg_writeraw(req->smb_conn->lp_ctx)?2:0);

	smbsrv_setup_reply(req, 13, 0);

	/* Reply, SMBlockread, SMBwritelock supported. */
	SCVAL(req->out.hdr,HDR_FLG,
	      CVAL(req->out.hdr, HDR_FLG) | FLAG_SUPPORT_LOCKREAD);

	SSVAL(req->out.vwv, VWV(0), choice);
	SSVAL(req->out.vwv, VWV(1), 0x1); /* user level security, don't encrypt */	

	/* tell redirector we support
	   readbraw and writebraw (possibly) */
	SSVAL(req->out.vwv, VWV(5), raw); 

	req->smb_conn->negotiate.protocol = PROTOCOL_COREPLUS;

	if (req->smb_conn->signing.mandatory_signing) {
		smbsrv_terminate_connection(req->smb_conn, 
					    "COREPLUS does not support SMB signing, and it is mandatory\n");
		return;
	}

	smbsrv_send_reply(req);
}

/****************************************************************************
 Reply for the lanman 1.0 protocol.
****************************************************************************/
static void reply_lanman1(struct smbsrv_request *req, uint16_t choice)
{
	int raw = (lpcfg_readraw(req->smb_conn->lp_ctx)?1:0) | (lpcfg_writeraw(req->smb_conn->lp_ctx)?2:0);
	int secword=0;
	time_t t = req->request_time.tv_sec;

	req->smb_conn->negotiate.encrypted_passwords = lpcfg_encrypted_passwords(req->smb_conn->lp_ctx);

	if (lpcfg_security(req->smb_conn->lp_ctx) != SEC_SHARE)
		secword |= NEGOTIATE_SECURITY_USER_LEVEL;

	if (req->smb_conn->negotiate.encrypted_passwords)
		secword |= NEGOTIATE_SECURITY_CHALLENGE_RESPONSE;

	req->smb_conn->negotiate.protocol = PROTOCOL_LANMAN1;

	smbsrv_setup_reply(req, 13, req->smb_conn->negotiate.encrypted_passwords ? 8 : 0);

	/* SMBlockread, SMBwritelock supported. */
	SCVAL(req->out.hdr,HDR_FLG,
	      CVAL(req->out.hdr, HDR_FLG) | FLAG_SUPPORT_LOCKREAD);

	SSVAL(req->out.vwv, VWV(0), choice);
	SSVAL(req->out.vwv, VWV(1), secword); 
	SSVAL(req->out.vwv, VWV(2), req->smb_conn->negotiate.max_recv);
	SSVAL(req->out.vwv, VWV(3), lpcfg_maxmux(req->smb_conn->lp_ctx));
	SSVAL(req->out.vwv, VWV(4), 1);
	SSVAL(req->out.vwv, VWV(5), raw); 
	SIVAL(req->out.vwv, VWV(6), req->smb_conn->connection->server_id.id);
	srv_push_dos_date(req->smb_conn, req->out.vwv, VWV(8), t);
	SSVAL(req->out.vwv, VWV(10), req->smb_conn->negotiate.zone_offset/60);
	SIVAL(req->out.vwv, VWV(11), 0); /* reserved */

	/* Create a token value and add it to the outgoing packet. */
	if (req->smb_conn->negotiate.encrypted_passwords) {
		NTSTATUS nt_status;

		SSVAL(req->out.vwv, VWV(11), 8);

		nt_status = get_challenge(req->smb_conn, req->out.data);
		if (!NT_STATUS_IS_OK(nt_status)) {
			smbsrv_terminate_connection(req->smb_conn, "LANMAN1 get_challenge failed\n");
			return;
		}
	}

	if (req->smb_conn->signing.mandatory_signing) {
		smbsrv_terminate_connection(req->smb_conn, 
					    "LANMAN1 does not support SMB signing, and it is mandatory\n");
		return;
	}

	smbsrv_send_reply(req);	
}

/****************************************************************************
 Reply for the lanman 2.0 protocol.
****************************************************************************/
static void reply_lanman2(struct smbsrv_request *req, uint16_t choice)
{
	int raw = (lpcfg_readraw(req->smb_conn->lp_ctx)?1:0) | (lpcfg_writeraw(req->smb_conn->lp_ctx)?2:0);
	int secword=0;
	time_t t = req->request_time.tv_sec;

	req->smb_conn->negotiate.encrypted_passwords = lpcfg_encrypted_passwords(req->smb_conn->lp_ctx);
  
	if (lpcfg_security(req->smb_conn->lp_ctx) != SEC_SHARE)
		secword |= NEGOTIATE_SECURITY_USER_LEVEL;

	if (req->smb_conn->negotiate.encrypted_passwords)
		secword |= NEGOTIATE_SECURITY_CHALLENGE_RESPONSE;

	req->smb_conn->negotiate.protocol = PROTOCOL_LANMAN2;

	smbsrv_setup_reply(req, 13, 0);

	SSVAL(req->out.vwv, VWV(0), choice);
	SSVAL(req->out.vwv, VWV(1), secword); 
	SSVAL(req->out.vwv, VWV(2), req->smb_conn->negotiate.max_recv);
	SSVAL(req->out.vwv, VWV(3), lpcfg_maxmux(req->smb_conn->lp_ctx));
	SSVAL(req->out.vwv, VWV(4), 1);
	SSVAL(req->out.vwv, VWV(5), raw); 
	SIVAL(req->out.vwv, VWV(6), req->smb_conn->connection->server_id.id);
	srv_push_dos_date(req->smb_conn, req->out.vwv, VWV(8), t);
	SSVAL(req->out.vwv, VWV(10), req->smb_conn->negotiate.zone_offset/60);
	SIVAL(req->out.vwv, VWV(11), 0);

	/* Create a token value and add it to the outgoing packet. */
	if (req->smb_conn->negotiate.encrypted_passwords) {
		SSVAL(req->out.vwv, VWV(11), 8);
		req_grow_data(req, 8);
		get_challenge(req->smb_conn, req->out.data);
	}

	req_push_str(req, NULL, lpcfg_workgroup(req->smb_conn->lp_ctx), -1, STR_TERMINATE);

	if (req->smb_conn->signing.mandatory_signing) {
		smbsrv_terminate_connection(req->smb_conn, 
					    "LANMAN2 does not support SMB signing, and it is mandatory\n");
		return;
	}

	smbsrv_send_reply(req);
}

static void reply_nt1_orig(struct smbsrv_request *req)
{
	/* Create a token value and add it to the outgoing packet. */
	if (req->smb_conn->negotiate.encrypted_passwords) {
		req_grow_data(req, 8);
		/* note that we do not send a challenge at all if
		   we are using plaintext */
		get_challenge(req->smb_conn, req->out.ptr);
		req->out.ptr += 8;
		SCVAL(req->out.vwv+1, VWV(16), 8);
	}
	req_push_str(req, NULL, lpcfg_workgroup(req->smb_conn->lp_ctx), -1, STR_UNICODE|STR_TERMINATE|STR_NOALIGN);
	req_push_str(req, NULL, lpcfg_netbios_name(req->smb_conn->lp_ctx), -1, STR_UNICODE|STR_TERMINATE|STR_NOALIGN);
	DEBUG(3,("not using extended security (SPNEGO or NTLMSSP)\n"));
}

/****************************************************************************
 Reply for the nt protocol.
****************************************************************************/
static void reply_nt1(struct smbsrv_request *req, uint16_t choice)
{
	/* dual names + lock_and_read + nt SMBs + remote API calls */
	int capabilities;
	int secword=0;
	time_t t = req->request_time.tv_sec;
	NTTIME nttime;
	bool negotiate_spnego = false;
	char *large_test_path;

	unix_to_nt_time(&nttime, t);

	capabilities = 
		CAP_NT_FIND | CAP_LOCK_AND_READ | 
		CAP_LEVEL_II_OPLOCKS | CAP_NT_SMBS | CAP_RPC_REMOTE_APIS;

	req->smb_conn->negotiate.encrypted_passwords = lpcfg_encrypted_passwords(req->smb_conn->lp_ctx);

	/* do spnego in user level security if the client
	   supports it and we can do encrypted passwords */
	
	if (req->smb_conn->negotiate.encrypted_passwords && 
	    (lpcfg_security(req->smb_conn->lp_ctx) != SEC_SHARE) &&
	    lpcfg_use_spnego(req->smb_conn->lp_ctx) &&
	    (req->flags2 & FLAGS2_EXTENDED_SECURITY)) {
		negotiate_spnego = true; 
		capabilities |= CAP_EXTENDED_SECURITY;
	}
	
	if (lpcfg_unix_extensions(req->smb_conn->lp_ctx)) {
		capabilities |= CAP_UNIX;
	}
	
	if (lpcfg_large_readwrite(req->smb_conn->lp_ctx)) {
		capabilities |= CAP_LARGE_READX | CAP_LARGE_WRITEX | CAP_W2K_SMBS;
	}

	large_test_path = lock_path(req, req->smb_conn->lp_ctx, "large_test.dat");
	if (large_file_support(large_test_path)) {
		capabilities |= CAP_LARGE_FILES;
	}

	if (lpcfg_readraw(req->smb_conn->lp_ctx) &&
	    lpcfg_writeraw(req->smb_conn->lp_ctx)) {
		capabilities |= CAP_RAW_MODE;
	}
	
	/* allow for disabling unicode */
	if (lpcfg_unicode(req->smb_conn->lp_ctx)) {
		capabilities |= CAP_UNICODE;
	}

	if (lpcfg_nt_status_support(req->smb_conn->lp_ctx)) {
		capabilities |= CAP_STATUS32;
	}
	
	if (lpcfg_host_msdfs(req->smb_conn->lp_ctx)) {
		capabilities |= CAP_DFS;
	}
	
	if (lpcfg_security(req->smb_conn->lp_ctx) != SEC_SHARE) {
		secword |= NEGOTIATE_SECURITY_USER_LEVEL;
	}

	if (req->smb_conn->negotiate.encrypted_passwords) {
		secword |= NEGOTIATE_SECURITY_CHALLENGE_RESPONSE;
	}

	if (req->smb_conn->signing.allow_smb_signing) {
		secword |= NEGOTIATE_SECURITY_SIGNATURES_ENABLED;
	}

	if (req->smb_conn->signing.mandatory_signing) {
		secword |= NEGOTIATE_SECURITY_SIGNATURES_REQUIRED;
	}
	
	req->smb_conn->negotiate.protocol = PROTOCOL_NT1;

	smbsrv_setup_reply(req, 17, 0);
	
	SSVAL(req->out.vwv, VWV(0), choice);
	SCVAL(req->out.vwv, VWV(1), secword);

	/* notice the strange +1 on vwv here? That's because
	   this is the one and only SMB packet that is malformed in
	   the specification - all the command words after the secword
	   are offset by 1 byte */
	SSVAL(req->out.vwv+1, VWV(1), lpcfg_maxmux(req->smb_conn->lp_ctx));
	SSVAL(req->out.vwv+1, VWV(2), 1); /* num vcs */
	SIVAL(req->out.vwv+1, VWV(3), req->smb_conn->negotiate.max_recv);
	SIVAL(req->out.vwv+1, VWV(5), 0x10000); /* raw size. full 64k */
	SIVAL(req->out.vwv+1, VWV(7), req->smb_conn->connection->server_id.id); /* session key */
	SIVAL(req->out.vwv+1, VWV(9), capabilities);
	push_nttime(req->out.vwv+1, VWV(11), nttime);
	SSVALS(req->out.vwv+1,VWV(15), req->smb_conn->negotiate.zone_offset/60);
	
	if (!negotiate_spnego) {
		reply_nt1_orig(req);
	} else {
		struct cli_credentials *server_credentials;
		struct gensec_security *gensec_security;
		DATA_BLOB null_data_blob = data_blob(NULL, 0);
		DATA_BLOB blob;
		const char *oid;
		NTSTATUS nt_status;
		
		server_credentials 
			= cli_credentials_init(req);
		if (!server_credentials) {
			smbsrv_terminate_connection(req->smb_conn, "Failed to init server credentials\n");
			return;
		}
		
		cli_credentials_set_conf(server_credentials, req->smb_conn->lp_ctx);
		nt_status = cli_credentials_set_machine_account(server_credentials, req->smb_conn->lp_ctx);
		if (!NT_STATUS_IS_OK(nt_status)) {
			DEBUG(10, ("Failed to obtain server credentials, perhaps a standalone server?: %s\n", nt_errstr(nt_status)));
			talloc_free(server_credentials);
			server_credentials = NULL;
		}

		nt_status = samba_server_gensec_start(req,
						      req->smb_conn->connection->event.ctx,
						      req->smb_conn->connection->msg_ctx,
						      req->smb_conn->lp_ctx,
						      server_credentials,
						      "cifs",
						      &gensec_security);

		if (!NT_STATUS_IS_OK(nt_status)) {
			DEBUG(0, ("Failed to start GENSEC: %s\n", nt_errstr(nt_status)));
			smbsrv_terminate_connection(req->smb_conn, "Failed to start GENSEC\n");
			return;
		}

		if (req->smb_conn->negotiate.auth_context) {
			smbsrv_terminate_connection(req->smb_conn, "reply_nt1: is this a secondary negprot?  auth_context is non-NULL!\n");
			return;
		}
		req->smb_conn->negotiate.server_credentials = talloc_reparent(req, req->smb_conn, server_credentials);

		gensec_set_target_service(gensec_security, "cifs");

		gensec_set_credentials(gensec_security, server_credentials);

		oid = GENSEC_OID_SPNEGO;
		nt_status = gensec_start_mech_by_oid(gensec_security, oid);
		
		if (NT_STATUS_IS_OK(nt_status)) {
			/* Get and push the proposed OID list into the packets */
			nt_status = gensec_update(gensec_security, req, null_data_blob, &blob);

			if (!NT_STATUS_IS_OK(nt_status) && !NT_STATUS_EQUAL(nt_status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {
				DEBUG(1, ("Failed to get SPNEGO to give us the first token: %s\n", nt_errstr(nt_status)));
			}
		}

		if (NT_STATUS_EQUAL(nt_status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {
			DEBUG(3,("using SPNEGO\n"));
		} else {
			DEBUG(5, ("Failed to start SPNEGO, falling back to NTLMSSP only: %s\n", nt_errstr(nt_status)));
			oid = GENSEC_OID_NTLMSSP;
			nt_status = gensec_start_mech_by_oid(gensec_security, oid);
			
			if (!NT_STATUS_IS_OK(nt_status)) {
				DEBUG(0, ("Failed to start SPNEGO as well as NTLMSSP fallback: %s\n", nt_errstr(nt_status)));
				reply_nt1_orig(req);
				return;
			}
			/* NTLMSSP is a client-first exchange */
			blob = data_blob(NULL, 0);
			DEBUG(3,("using raw-NTLMSSP\n"));
		}

		req->smb_conn->negotiate.oid = oid;
	
		req_grow_data(req, blob.length + 16);
		/* a NOT very random guid, perhaps we should get it
		 * from the credentials (kitchen sink...) */
		memset(req->out.ptr, '\0', 16);
		req->out.ptr += 16;

		memcpy(req->out.ptr, blob.data, blob.length);
		SCVAL(req->out.vwv+1, VWV(16), blob.length + 16);
		req->out.ptr += blob.length;
	}
	
	smbsrv_send_reply_nosign(req);	
}

/****************************************************************************
 Reply for the SMB2 2.001 protocol
****************************************************************************/
static void reply_smb2(struct smbsrv_request *req, uint16_t choice)
{
	struct smbsrv_connection *smb_conn = req->smb_conn;
	NTSTATUS status;

	talloc_free(smb_conn->sessions.idtree_vuid);
	ZERO_STRUCT(smb_conn->sessions);
	talloc_free(smb_conn->smb_tcons.idtree_tid);
	ZERO_STRUCT(smb_conn->smb_tcons);
	ZERO_STRUCT(smb_conn->signing);

	/* reply with a SMB2 packet */
	status = smbsrv_init_smb2_connection(smb_conn);
	if (!NT_STATUS_IS_OK(status)) {
		smbsrv_terminate_connection(smb_conn, nt_errstr(status));
		talloc_free(req);
		return;
	}
	packet_set_callback(smb_conn->packet, smbsrv_recv_smb2_request);
	smb2srv_reply_smb_negprot(req);
	req = NULL;
}

/* List of supported protocols, most desired first */
static const struct {
	const char *proto_name;
	const char *short_name;
	void (*proto_reply_fn)(struct smbsrv_request *req, uint16_t choice);
	int protocol_level;
} supported_protocols[] = {
	{"SMB 2.002",			"SMB2",		reply_smb2,	PROTOCOL_SMB2},
	{"NT LANMAN 1.0",		"NT1",		reply_nt1,	PROTOCOL_NT1},
	{"NT LM 0.12",			"NT1",		reply_nt1,	PROTOCOL_NT1},
	{"LANMAN2.1",			"LANMAN2",	reply_lanman2,	PROTOCOL_LANMAN2},
	{"LM1.2X002",			"LANMAN2",	reply_lanman2,	PROTOCOL_LANMAN2},
	{"Samba",			"LANMAN2",	reply_lanman2,	PROTOCOL_LANMAN2},
	{"DOS LM1.2X002",		"LANMAN2",	reply_lanman2,	PROTOCOL_LANMAN2},
	{"Windows for Workgroups 3.1a",	"LANMAN1",	reply_lanman1,	PROTOCOL_LANMAN1},
	{"LANMAN1.0",			"LANMAN1",	reply_lanman1,	PROTOCOL_LANMAN1},
	{"MICROSOFT NETWORKS 3.0",	"LANMAN1",	reply_lanman1,	PROTOCOL_LANMAN1},
	{"MICROSOFT NETWORKS 1.03",	"COREPLUS",	reply_coreplus,	PROTOCOL_COREPLUS},
	{"PC NETWORK PROGRAM 1.0",	"CORE",		reply_corep,	PROTOCOL_CORE},
	{NULL,NULL,NULL,0},
};

/****************************************************************************
 Reply to a negprot.
****************************************************************************/

void smbsrv_reply_negprot(struct smbsrv_request *req)
{
	int protocol;
	uint8_t *p;
	uint32_t protos_count = 0;
	char **protos = NULL;

	if (req->smb_conn->negotiate.done_negprot) {
		smbsrv_terminate_connection(req->smb_conn, "multiple negprot's are not permitted");
		return;
	}
	req->smb_conn->negotiate.done_negprot = true;

	p = req->in.data;
	while (true) {
		size_t len;

		protos = talloc_realloc(req, protos, char *, protos_count + 1);
		if (!protos) {
			smbsrv_terminate_connection(req->smb_conn, nt_errstr(NT_STATUS_NO_MEMORY));
			return;
		}
		protos[protos_count] = NULL;
		len = req_pull_ascii4(&req->in.bufinfo, (const char **)&protos[protos_count], p, STR_ASCII|STR_TERMINATE);
		p += len;
		if (len == 0 || !protos[protos_count]) break;

		DEBUG(5,("Requested protocol [%d][%s]\n", protos_count, protos[protos_count]));
		protos_count++;
	}

	/* Check for protocols, most desirable first */
	for (protocol = 0; supported_protocols[protocol].proto_name; protocol++) {
		int i;

		if (supported_protocols[protocol].protocol_level > lpcfg_srv_maxprotocol(req->smb_conn->lp_ctx))
			continue;
		if (supported_protocols[protocol].protocol_level < lpcfg_srv_minprotocol(req->smb_conn->lp_ctx))
			continue;

		for (i = 0; i < protos_count; i++) {
			if (strcmp(supported_protocols[protocol].proto_name, protos[i]) != 0) continue;

			supported_protocols[protocol].proto_reply_fn(req, i);
			DEBUG(3,("Selected protocol [%d][%s]\n",
				i, supported_protocols[protocol].proto_name));
			return;
		}
	}

	smbsrv_terminate_connection(req->smb_conn, "No protocol supported !");
}
