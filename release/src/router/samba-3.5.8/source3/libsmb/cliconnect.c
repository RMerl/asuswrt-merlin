/* 
   Unix SMB/CIFS implementation.
   client connect/disconnect routines
   Copyright (C) Andrew Tridgell 1994-1998
   Copyright (C) Andrew Bartlett 2001-2003
   
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
#include "../libcli/auth/libcli_auth.h"
#include "../libcli/auth/spnego.h"
#include "smb_krb5.h"

static const struct {
	int prot;
	const char name[24];
} prots[10] = {
	{PROTOCOL_CORE,		"PC NETWORK PROGRAM 1.0"},
	{PROTOCOL_COREPLUS,	"MICROSOFT NETWORKS 1.03"},
	{PROTOCOL_LANMAN1,	"MICROSOFT NETWORKS 3.0"},
	{PROTOCOL_LANMAN1,	"LANMAN1.0"},
	{PROTOCOL_LANMAN2,	"LM1.2X002"},
	{PROTOCOL_LANMAN2,	"DOS LANMAN2.1"},
	{PROTOCOL_LANMAN2,	"LANMAN2.1"},
	{PROTOCOL_LANMAN2,	"Samba"},
	{PROTOCOL_NT1,		"NT LANMAN 1.0"},
	{PROTOCOL_NT1,		"NT LM 0.12"},
};

#define STAR_SMBSERVER "*SMBSERVER"

/**
 * Set the user session key for a connection
 * @param cli The cli structure to add it too
 * @param session_key The session key used.  (A copy of this is taken for the cli struct)
 *
 */

static void cli_set_session_key (struct cli_state *cli, const DATA_BLOB session_key) 
{
	cli->user_session_key = data_blob(session_key.data, session_key.length);
}

/****************************************************************************
 Do an old lanman2 style session setup.
****************************************************************************/

static NTSTATUS cli_session_setup_lanman2(struct cli_state *cli,
					  const char *user, 
					  const char *pass, size_t passlen,
					  const char *workgroup)
{
	DATA_BLOB session_key = data_blob_null;
	DATA_BLOB lm_response = data_blob_null;
	NTSTATUS status;
	fstring pword;
	char *p;

	if (passlen > sizeof(pword)-1) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* LANMAN servers predate NT status codes and Unicode and ignore those 
	   smb flags so we must disable the corresponding default capabilities  
	   that would otherwise cause the Unicode and NT Status flags to be
	   set (and even returned by the server) */

	cli->capabilities &= ~(CAP_UNICODE | CAP_STATUS32);

	/* if in share level security then don't send a password now */
	if (!(cli->sec_mode & NEGOTIATE_SECURITY_USER_LEVEL))
		passlen = 0;

	if (passlen > 0 && (cli->sec_mode & NEGOTIATE_SECURITY_CHALLENGE_RESPONSE) && passlen != 24) {
		/* Encrypted mode needed, and non encrypted password supplied. */
		lm_response = data_blob(NULL, 24);
		if (!SMBencrypt(pass, cli->secblob.data,(uchar *)lm_response.data)) {
			DEBUG(1, ("Password is > 14 chars in length, and is therefore incompatible with Lanman authentication\n"));
			return NT_STATUS_ACCESS_DENIED;
		}
	} else if ((cli->sec_mode & NEGOTIATE_SECURITY_CHALLENGE_RESPONSE) && passlen == 24) {
		/* Encrypted mode needed, and encrypted password supplied. */
		lm_response = data_blob(pass, passlen);
	} else if (passlen > 0) {
		/* Plaintext mode needed, assume plaintext supplied. */
		passlen = clistr_push(cli, pword, pass, sizeof(pword), STR_TERMINATE);
		lm_response = data_blob(pass, passlen);
	}

	/* send a session setup command */
	memset(cli->outbuf,'\0',smb_size);
	cli_set_message(cli->outbuf,10, 0, True);
	SCVAL(cli->outbuf,smb_com,SMBsesssetupX);
	cli_setup_packet(cli);
	
	SCVAL(cli->outbuf,smb_vwv0,0xFF);
	SSVAL(cli->outbuf,smb_vwv2,cli->max_xmit);
	SSVAL(cli->outbuf,smb_vwv3,2);
	SSVAL(cli->outbuf,smb_vwv4,1);
	SIVAL(cli->outbuf,smb_vwv5,cli->sesskey);
	SSVAL(cli->outbuf,smb_vwv7,lm_response.length);

	p = smb_buf(cli->outbuf);
	memcpy(p,lm_response.data,lm_response.length);
	p += lm_response.length;
	p += clistr_push(cli, p, user, -1, STR_TERMINATE|STR_UPPER);
	p += clistr_push(cli, p, workgroup, -1, STR_TERMINATE|STR_UPPER);
	p += clistr_push(cli, p, "Unix", -1, STR_TERMINATE);
	p += clistr_push(cli, p, "Samba", -1, STR_TERMINATE);
	cli_setup_bcc(cli, p);

	if (!cli_send_smb(cli) || !cli_receive_smb(cli)) {
		return cli_nt_error(cli);
	}

	show_msg(cli->inbuf);

	if (cli_is_error(cli)) {
		return cli_nt_error(cli);
	}
	
	/* use the returned vuid from now on */
	cli->vuid = SVAL(cli->inbuf,smb_uid);	
	status = cli_set_username(cli, user);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (session_key.data) {
		/* Have plaintext orginal */
		cli_set_session_key(cli, session_key);
	}

	return NT_STATUS_OK;
}

/****************************************************************************
 Work out suitable capabilities to offer the server.
****************************************************************************/

static uint32 cli_session_setup_capabilities(struct cli_state *cli)
{
	uint32 capabilities = CAP_NT_SMBS;

	if (!cli->force_dos_errors)
		capabilities |= CAP_STATUS32;

	if (cli->use_level_II_oplocks)
		capabilities |= CAP_LEVEL_II_OPLOCKS;

	capabilities |= (cli->capabilities & (CAP_UNICODE|CAP_LARGE_FILES|CAP_LARGE_READX|CAP_LARGE_WRITEX|CAP_DFS));
	return capabilities;
}

/****************************************************************************
 Do a NT1 guest session setup.
****************************************************************************/

struct cli_session_setup_guest_state {
	struct cli_state *cli;
	uint16_t vwv[16];
	struct iovec bytes;
};

static void cli_session_setup_guest_done(struct tevent_req *subreq);

struct tevent_req *cli_session_setup_guest_create(TALLOC_CTX *mem_ctx,
						  struct event_context *ev,
						  struct cli_state *cli,
						  struct tevent_req **psmbreq)
{
	struct tevent_req *req, *subreq;
	struct cli_session_setup_guest_state *state;
	uint16_t *vwv;
	uint8_t *bytes;

	req = tevent_req_create(mem_ctx, &state,
				struct cli_session_setup_guest_state);
	if (req == NULL) {
		return NULL;
	}
	state->cli = cli;
	vwv = state->vwv;

	SCVAL(vwv+0, 0, 0xFF);
	SCVAL(vwv+0, 1, 0);
	SSVAL(vwv+1, 0, 0);
	SSVAL(vwv+2, 0, CLI_BUFFER_SIZE);
	SSVAL(vwv+3, 0, 2);
	SSVAL(vwv+4, 0, cli->pid);
	SIVAL(vwv+5, 0, cli->sesskey);
	SSVAL(vwv+7, 0, 0);
	SSVAL(vwv+8, 0, 0);
	SSVAL(vwv+9, 0, 0);
	SSVAL(vwv+10, 0, 0);
	SIVAL(vwv+11, 0, cli_session_setup_capabilities(cli));

	bytes = talloc_array(state, uint8_t, 0);

	bytes = smb_bytes_push_str(bytes, cli_ucs2(cli), "",  1, /* username */
				   NULL);
	bytes = smb_bytes_push_str(bytes, cli_ucs2(cli), "", 1, /* workgroup */
				   NULL);
	bytes = smb_bytes_push_str(bytes, cli_ucs2(cli), "Unix", 5, NULL);
	bytes = smb_bytes_push_str(bytes, cli_ucs2(cli), "Samba", 6, NULL);

	if (bytes == NULL) {
		TALLOC_FREE(req);
		return NULL;
	}

	state->bytes.iov_base = (void *)bytes;
	state->bytes.iov_len = talloc_get_size(bytes);

	subreq = cli_smb_req_create(state, ev, cli, SMBsesssetupX, 0, 13, vwv,
				    1, &state->bytes);
	if (subreq == NULL) {
		TALLOC_FREE(req);
		return NULL;
	}
	tevent_req_set_callback(subreq, cli_session_setup_guest_done, req);
	*psmbreq = subreq;
	return req;
}

struct tevent_req *cli_session_setup_guest_send(TALLOC_CTX *mem_ctx,
						struct event_context *ev,
						struct cli_state *cli)
{
	struct tevent_req *req, *subreq;
	NTSTATUS status;

	req = cli_session_setup_guest_create(mem_ctx, ev, cli, &subreq);
	if (req == NULL) {
		return NULL;
	}

	status = cli_smb_req_send(subreq);
	if (NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return tevent_req_post(req, ev);
	}
	return req;
}

static void cli_session_setup_guest_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct cli_session_setup_guest_state *state = tevent_req_data(
		req, struct cli_session_setup_guest_state);
	struct cli_state *cli = state->cli;
	uint32_t num_bytes;
	char *inbuf;
	uint8_t *bytes;
	uint8_t *p;
	NTSTATUS status;

	status = cli_smb_recv(subreq, 0, NULL, NULL, &num_bytes, &bytes);
	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(subreq);
		tevent_req_nterror(req, status);
		return;
	}

	inbuf = (char *)cli_smb_inbuf(subreq);
	p = bytes;

	cli->vuid = SVAL(inbuf, smb_uid);

	p += clistr_pull(inbuf, cli->server_os, (char *)p, sizeof(fstring),
			 bytes+num_bytes-p, STR_TERMINATE);
	p += clistr_pull(inbuf, cli->server_type, (char *)p, sizeof(fstring),
			 bytes+num_bytes-p, STR_TERMINATE);
	p += clistr_pull(inbuf, cli->server_domain, (char *)p, sizeof(fstring),
			 bytes+num_bytes-p, STR_TERMINATE);

	if (strstr(cli->server_type, "Samba")) {
		cli->is_samba = True;
	}

	TALLOC_FREE(subreq);

	status = cli_set_username(cli, "");
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}
	tevent_req_done(req);
}

NTSTATUS cli_session_setup_guest_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}

static NTSTATUS cli_session_setup_guest(struct cli_state *cli)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev;
	struct tevent_req *req;
	NTSTATUS status = NT_STATUS_OK;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}

	ev = event_context_init(frame);
	if (ev == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	req = cli_session_setup_guest_send(frame, ev, cli);
	if (req == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	if (!tevent_req_poll(req, ev)) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}

	status = cli_session_setup_guest_recv(req);
 fail:
	TALLOC_FREE(frame);
	if (!NT_STATUS_IS_OK(status)) {
		cli_set_error(cli, status);
	}
	return status;
}

/****************************************************************************
 Do a NT1 plaintext session setup.
****************************************************************************/

static NTSTATUS cli_session_setup_plaintext(struct cli_state *cli,
					    const char *user, const char *pass,
					    const char *workgroup)
{
	uint32 capabilities = cli_session_setup_capabilities(cli);
	char *p;
	NTSTATUS status;
	fstring lanman;
	
	fstr_sprintf( lanman, "Samba %s", samba_version_string());

	memset(cli->outbuf, '\0', smb_size);
	cli_set_message(cli->outbuf,13,0,True);
	SCVAL(cli->outbuf,smb_com,SMBsesssetupX);
	cli_setup_packet(cli);
			
	SCVAL(cli->outbuf,smb_vwv0,0xFF);
	SSVAL(cli->outbuf,smb_vwv2,CLI_BUFFER_SIZE);
	SSVAL(cli->outbuf,smb_vwv3,2);
	SSVAL(cli->outbuf,smb_vwv4,cli->pid);
	SIVAL(cli->outbuf,smb_vwv5,cli->sesskey);
	SSVAL(cli->outbuf,smb_vwv8,0);
	SIVAL(cli->outbuf,smb_vwv11,capabilities); 
	p = smb_buf(cli->outbuf);
	
	/* check wether to send the ASCII or UNICODE version of the password */
	
	if ( (capabilities & CAP_UNICODE) == 0 ) {
		p += clistr_push(cli, p, pass, -1, STR_TERMINATE); /* password */
		SSVAL(cli->outbuf,smb_vwv7,PTR_DIFF(p, smb_buf(cli->outbuf)));
	}
	else {
		/* For ucs2 passwords clistr_push calls ucs2_align, which causes
		 * the space taken by the unicode password to be one byte too
		 * long (as we're on an odd byte boundary here). Reduce the
		 * count by 1 to cope with this. Fixes smbclient against NetApp
		 * servers which can't cope. Fix from
		 * bryan.kolodziej@allenlund.com in bug #3840.
		 */
		p += clistr_push(cli, p, pass, -1, STR_UNICODE|STR_TERMINATE); /* unicode password */
		SSVAL(cli->outbuf,smb_vwv8,PTR_DIFF(p, smb_buf(cli->outbuf))-1);	
	}
	
	p += clistr_push(cli, p, user, -1, STR_TERMINATE); /* username */
	p += clistr_push(cli, p, workgroup, -1, STR_TERMINATE); /* workgroup */
	p += clistr_push(cli, p, "Unix", -1, STR_TERMINATE);
	p += clistr_push(cli, p, lanman, -1, STR_TERMINATE);
	cli_setup_bcc(cli, p);

	if (!cli_send_smb(cli) || !cli_receive_smb(cli)) {
		return cli_nt_error(cli);
	}
	
	show_msg(cli->inbuf);
	
	if (cli_is_error(cli)) {
		return cli_nt_error(cli);
	}

	cli->vuid = SVAL(cli->inbuf,smb_uid);
	p = smb_buf(cli->inbuf);
	p += clistr_pull(cli->inbuf, cli->server_os, p, sizeof(fstring),
			 -1, STR_TERMINATE);
	p += clistr_pull(cli->inbuf, cli->server_type, p, sizeof(fstring),
			 -1, STR_TERMINATE);
	p += clistr_pull(cli->inbuf, cli->server_domain, p, sizeof(fstring),
			 -1, STR_TERMINATE);
	status = cli_set_username(cli, user);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if (strstr(cli->server_type, "Samba")) {
		cli->is_samba = True;
	}

	return NT_STATUS_OK;
}

/****************************************************************************
   do a NT1 NTLM/LM encrypted session setup - for when extended security
   is not negotiated.
   @param cli client state to create do session setup on
   @param user username
   @param pass *either* cleartext password (passlen !=24) or LM response.
   @param ntpass NT response, implies ntpasslen >=24, implies pass is not clear
   @param workgroup The user's domain.
****************************************************************************/

static NTSTATUS cli_session_setup_nt1(struct cli_state *cli, const char *user, 
				      const char *pass, size_t passlen,
				      const char *ntpass, size_t ntpasslen,
				      const char *workgroup)
{
	uint32 capabilities = cli_session_setup_capabilities(cli);
	DATA_BLOB lm_response = data_blob_null;
	DATA_BLOB nt_response = data_blob_null;
	DATA_BLOB session_key = data_blob_null;
	NTSTATUS result;
	char *p;
	bool ok;

	if (passlen == 0) {
		/* do nothing - guest login */
	} else if (passlen != 24) {
		if (lp_client_ntlmv2_auth()) {
			DATA_BLOB server_chal;
			DATA_BLOB names_blob;
			server_chal = data_blob(cli->secblob.data, MIN(cli->secblob.length, 8)); 

			/* note that the 'workgroup' here is a best guess - we don't know
			   the server's domain at this point.  The 'server name' is also
			   dodgy... 
			*/
			names_blob = NTLMv2_generate_names_blob(NULL, cli->called.name, workgroup);

			if (!SMBNTLMv2encrypt(NULL, user, workgroup, pass, &server_chal, 
					      &names_blob,
					      &lm_response, &nt_response, NULL, &session_key)) {
				data_blob_free(&names_blob);
				data_blob_free(&server_chal);
				return NT_STATUS_ACCESS_DENIED;
			}
			data_blob_free(&names_blob);
			data_blob_free(&server_chal);

		} else {
			uchar nt_hash[16];
			E_md4hash(pass, nt_hash);

#ifdef LANMAN_ONLY
			nt_response = data_blob_null;
#else
			nt_response = data_blob(NULL, 24);
			SMBNTencrypt(pass,cli->secblob.data,nt_response.data);
#endif
			/* non encrypted password supplied. Ignore ntpass. */
			if (lp_client_lanman_auth()) {
				lm_response = data_blob(NULL, 24);
				if (!SMBencrypt(pass,cli->secblob.data, lm_response.data)) {
					/* Oops, the LM response is invalid, just put 
					   the NT response there instead */
					data_blob_free(&lm_response);
					lm_response = data_blob(nt_response.data, nt_response.length);
				}
			} else {
				/* LM disabled, place NT# in LM field instead */
				lm_response = data_blob(nt_response.data, nt_response.length);
			}

			session_key = data_blob(NULL, 16);
#ifdef LANMAN_ONLY
			E_deshash(pass, session_key.data);
			memset(&session_key.data[8], '\0', 8);
#else
			SMBsesskeygen_ntv1(nt_hash, session_key.data);
#endif
		}
		cli_temp_set_signing(cli);
	} else {
		/* pre-encrypted password supplied.  Only used for 
		   security=server, can't do
		   signing because we don't have original key */

		lm_response = data_blob(pass, passlen);
		nt_response = data_blob(ntpass, ntpasslen);
	}

	/* send a session setup command */
	memset(cli->outbuf,'\0',smb_size);

	cli_set_message(cli->outbuf,13,0,True);
	SCVAL(cli->outbuf,smb_com,SMBsesssetupX);
	cli_setup_packet(cli);
			
	SCVAL(cli->outbuf,smb_vwv0,0xFF);
	SSVAL(cli->outbuf,smb_vwv2,CLI_BUFFER_SIZE);
	SSVAL(cli->outbuf,smb_vwv3,2);
	SSVAL(cli->outbuf,smb_vwv4,cli->pid);
	SIVAL(cli->outbuf,smb_vwv5,cli->sesskey);
	SSVAL(cli->outbuf,smb_vwv7,lm_response.length);
	SSVAL(cli->outbuf,smb_vwv8,nt_response.length);
	SIVAL(cli->outbuf,smb_vwv11,capabilities); 
	p = smb_buf(cli->outbuf);
	if (lm_response.length) {
		memcpy(p,lm_response.data, lm_response.length); p += lm_response.length;
	}
	if (nt_response.length) {
		memcpy(p,nt_response.data, nt_response.length); p += nt_response.length;
	}
	p += clistr_push(cli, p, user, -1, STR_TERMINATE);

	/* Upper case here might help some NTLMv2 implementations */
	p += clistr_push(cli, p, workgroup, -1, STR_TERMINATE|STR_UPPER);
	p += clistr_push(cli, p, "Unix", -1, STR_TERMINATE);
	p += clistr_push(cli, p, "Samba", -1, STR_TERMINATE);
	cli_setup_bcc(cli, p);

	if (!cli_send_smb(cli) || !cli_receive_smb(cli)) {
		result = cli_nt_error(cli);
		goto end;
	}

	/* show_msg(cli->inbuf); */

	if (cli_is_error(cli)) {
		result = cli_nt_error(cli);
		goto end;
	}

#ifdef LANMAN_ONLY
	ok = cli_simple_set_signing(cli, session_key, lm_response);
#else
	ok = cli_simple_set_signing(cli, session_key, nt_response);
#endif
	if (ok) {
		if (!cli_check_sign_mac(cli, cli->inbuf, 1)) {
			result = NT_STATUS_ACCESS_DENIED;
			goto end;
		}
	}

	/* use the returned vuid from now on */
	cli->vuid = SVAL(cli->inbuf,smb_uid);
	
	p = smb_buf(cli->inbuf);
	p += clistr_pull(cli->inbuf, cli->server_os, p, sizeof(fstring),
			 -1, STR_TERMINATE);
	p += clistr_pull(cli->inbuf, cli->server_type, p, sizeof(fstring),
			 -1, STR_TERMINATE);
	p += clistr_pull(cli->inbuf, cli->server_domain, p, sizeof(fstring),
			 -1, STR_TERMINATE);

	if (strstr(cli->server_type, "Samba")) {
		cli->is_samba = True;
	}

	result = cli_set_username(cli, user);
	if (!NT_STATUS_IS_OK(result)) {
		goto end;
	}

	if (session_key.data) {
		/* Have plaintext orginal */
		cli_set_session_key(cli, session_key);
	}

	result = NT_STATUS_OK;
end:	
	data_blob_free(&lm_response);
	data_blob_free(&nt_response);
	data_blob_free(&session_key);
	return result;
}

/****************************************************************************
 Send a extended security session setup blob
****************************************************************************/

static bool cli_session_setup_blob_send(struct cli_state *cli, DATA_BLOB blob)
{
	uint32 capabilities = cli_session_setup_capabilities(cli);
	char *p;

	capabilities |= CAP_EXTENDED_SECURITY;

	/* send a session setup command */
	memset(cli->outbuf,'\0',smb_size);

	cli_set_message(cli->outbuf,12,0,True);
	SCVAL(cli->outbuf,smb_com,SMBsesssetupX);

	cli_setup_packet(cli);

	SCVAL(cli->outbuf,smb_vwv0,0xFF);
	SSVAL(cli->outbuf,smb_vwv2,CLI_BUFFER_SIZE);
	SSVAL(cli->outbuf,smb_vwv3,2);
	SSVAL(cli->outbuf,smb_vwv4,1);
	SIVAL(cli->outbuf,smb_vwv5,0);
	SSVAL(cli->outbuf,smb_vwv7,blob.length);
	SIVAL(cli->outbuf,smb_vwv10,capabilities); 
	p = smb_buf(cli->outbuf);
	memcpy(p, blob.data, blob.length);
	p += blob.length;
	p += clistr_push(cli, p, "Unix", -1, STR_TERMINATE);
	p += clistr_push(cli, p, "Samba", -1, STR_TERMINATE);
	cli_setup_bcc(cli, p);
	return cli_send_smb(cli);
}

/****************************************************************************
 Send a extended security session setup blob, returning a reply blob.
****************************************************************************/

static DATA_BLOB cli_session_setup_blob_receive(struct cli_state *cli)
{
	DATA_BLOB blob2 = data_blob_null;
	char *p;
	size_t len;

	if (!cli_receive_smb(cli))
		return blob2;

	show_msg(cli->inbuf);

	if (cli_is_error(cli) && !NT_STATUS_EQUAL(cli_nt_error(cli),
						  NT_STATUS_MORE_PROCESSING_REQUIRED)) {
		return blob2;
	}

	/* use the returned vuid from now on */
	cli->vuid = SVAL(cli->inbuf,smb_uid);

	p = smb_buf(cli->inbuf);

	blob2 = data_blob(p, SVAL(cli->inbuf, smb_vwv3));

	p += blob2.length;
	p += clistr_pull(cli->inbuf, cli->server_os, p, sizeof(fstring),
			 -1, STR_TERMINATE);

	/* w2k with kerberos doesn't properly null terminate this field */
	len = smb_bufrem(cli->inbuf, p);
	if (p + len < cli->inbuf + cli->bufsize+SAFETY_MARGIN - 2) {
		char *end_of_buf = p + len;

		SSVAL(p, len, 0);
		/* Now it's null terminated. */
		p += clistr_pull(cli->inbuf, cli->server_type, p, sizeof(fstring),
			-1, STR_TERMINATE);
		/*
		 * See if there's another string. If so it's the
		 * server domain (part of the 'standard' Samba
		 * server signature).
		 */
		if (p < end_of_buf) {
			p += clistr_pull(cli->inbuf, cli->server_domain, p, sizeof(fstring),
				-1, STR_TERMINATE);
		}
	} else {
		/*
		 * No room to null terminate so we can't see if there
		 * is another string (server_domain) afterwards.
		 */
		p += clistr_pull(cli->inbuf, cli->server_type, p, sizeof(fstring),
				 len, 0);
	}
	return blob2;
}

#ifdef HAVE_KRB5
/****************************************************************************
 Send a extended security session setup blob, returning a reply blob.
****************************************************************************/

/* The following is calculated from :
 * (smb_size-4) = 35
 * (smb_wcnt * 2) = 24 (smb_wcnt == 12 in cli_session_setup_blob_send() )
 * (strlen("Unix") + 1 + strlen("Samba") + 1) * 2 = 22 (unicode strings at
 * end of packet.
 */

#define BASE_SESSSETUP_BLOB_PACKET_SIZE (35 + 24 + 22)

static bool cli_session_setup_blob(struct cli_state *cli, DATA_BLOB blob)
{
	int32 remaining = blob.length;
	int32 cur = 0;
	DATA_BLOB send_blob = data_blob_null;
	int32 max_blob_size = 0;
	DATA_BLOB receive_blob = data_blob_null;

	if (cli->max_xmit < BASE_SESSSETUP_BLOB_PACKET_SIZE + 1) {
		DEBUG(0,("cli_session_setup_blob: cli->max_xmit too small "
			"(was %u, need minimum %u)\n",
			(unsigned int)cli->max_xmit,
			BASE_SESSSETUP_BLOB_PACKET_SIZE));
		cli_set_nt_error(cli, NT_STATUS_INVALID_PARAMETER);
		return False;
	}

	max_blob_size = cli->max_xmit - BASE_SESSSETUP_BLOB_PACKET_SIZE;

	while ( remaining > 0) {
		if (remaining >= max_blob_size) {
			send_blob.length = max_blob_size;
			remaining -= max_blob_size;
		} else {
			send_blob.length = remaining; 
                        remaining = 0;
		}

		send_blob.data =  &blob.data[cur];
		cur += send_blob.length;

		DEBUG(10, ("cli_session_setup_blob: Remaining (%u) sending (%u) current (%u)\n", 
			(unsigned int)remaining,
			(unsigned int)send_blob.length,
			(unsigned int)cur ));

		if (!cli_session_setup_blob_send(cli, send_blob)) {
			DEBUG(0, ("cli_session_setup_blob: send failed\n"));
			return False;
		}

		receive_blob = cli_session_setup_blob_receive(cli);
		data_blob_free(&receive_blob);

		if (cli_is_error(cli) &&
				!NT_STATUS_EQUAL( cli_get_nt_error(cli), 
					NT_STATUS_MORE_PROCESSING_REQUIRED)) {
			DEBUG(0, ("cli_session_setup_blob: receive failed "
				  "(%s)\n", nt_errstr(cli_get_nt_error(cli))));
			cli->vuid = 0;
			return False;
		}
	}

	return True;
}

/****************************************************************************
 Use in-memory credentials cache
****************************************************************************/

static void use_in_memory_ccache(void) {
	setenv(KRB5_ENV_CCNAME, "MEMORY:cliconnect", 1);
}

/****************************************************************************
 Do a spnego/kerberos encrypted session setup.
****************************************************************************/

static ADS_STATUS cli_session_setup_kerberos(struct cli_state *cli, const char *principal, const char *workgroup)
{
	DATA_BLOB negTokenTarg;
	DATA_BLOB session_key_krb5;
	NTSTATUS nt_status;
	int rc;

	cli_temp_set_signing(cli);

	DEBUG(2,("Doing kerberos session setup\n"));

	/* generate the encapsulated kerberos5 ticket */
	rc = spnego_gen_negTokenTarg(principal, 0, &negTokenTarg, &session_key_krb5, 0, NULL);

	if (rc) {
		DEBUG(1, ("cli_session_setup_kerberos: spnego_gen_negTokenTarg failed: %s\n",
			error_message(rc)));
		return ADS_ERROR_KRB5(rc);
	}

#if 0
	file_save("negTokenTarg.dat", negTokenTarg.data, negTokenTarg.length);
#endif

	if (!cli_session_setup_blob(cli, negTokenTarg)) {
		nt_status = cli_nt_error(cli);
		goto nt_error;
	}

	if (cli_is_error(cli)) {
		nt_status = cli_nt_error(cli);
		if (NT_STATUS_IS_OK(nt_status)) {
			nt_status = NT_STATUS_UNSUCCESSFUL;
		}
		goto nt_error;
	}

	cli_set_session_key(cli, session_key_krb5);

	if (cli_simple_set_signing(
		    cli, session_key_krb5, data_blob_null)) {

		if (!cli_check_sign_mac(cli, cli->inbuf, 1)) {
			nt_status = NT_STATUS_ACCESS_DENIED;
			goto nt_error;
		}
	}

	data_blob_free(&negTokenTarg);
	data_blob_free(&session_key_krb5);

	return ADS_ERROR_NT(NT_STATUS_OK);

nt_error:
	data_blob_free(&negTokenTarg);
	data_blob_free(&session_key_krb5);
	cli->vuid = 0;
	return ADS_ERROR_NT(nt_status);
}
#endif	/* HAVE_KRB5 */


/****************************************************************************
 Do a spnego/NTLMSSP encrypted session setup.
****************************************************************************/

static NTSTATUS cli_session_setup_ntlmssp(struct cli_state *cli, const char *user, 
					  const char *pass, const char *domain)
{
	struct ntlmssp_state *ntlmssp_state;
	NTSTATUS nt_status;
	int turn = 1;
	DATA_BLOB msg1;
	DATA_BLOB blob = data_blob_null;
	DATA_BLOB blob_in = data_blob_null;
	DATA_BLOB blob_out = data_blob_null;

	cli_temp_set_signing(cli);

	if (!NT_STATUS_IS_OK(nt_status = ntlmssp_client_start(&ntlmssp_state))) {
		return nt_status;
	}
	ntlmssp_want_feature(ntlmssp_state, NTLMSSP_FEATURE_SESSION_KEY);
	if (cli->use_ccache) {
		ntlmssp_want_feature(ntlmssp_state, NTLMSSP_FEATURE_CCACHE);
	}

	if (!NT_STATUS_IS_OK(nt_status = ntlmssp_set_username(ntlmssp_state, user))) {
		return nt_status;
	}
	if (!NT_STATUS_IS_OK(nt_status = ntlmssp_set_domain(ntlmssp_state, domain))) {
		return nt_status;
	}
	if (!NT_STATUS_IS_OK(nt_status = ntlmssp_set_password(ntlmssp_state, pass))) {
		return nt_status;
	}

	do {
		nt_status = ntlmssp_update(ntlmssp_state, 
						  blob_in, &blob_out);
		data_blob_free(&blob_in);
		if (NT_STATUS_EQUAL(nt_status, NT_STATUS_MORE_PROCESSING_REQUIRED) || NT_STATUS_IS_OK(nt_status)) {
			if (turn == 1) {
				/* and wrap it in a SPNEGO wrapper */
				msg1 = gen_negTokenInit(OID_NTLMSSP, blob_out);
			} else {
				/* wrap it in SPNEGO */
				msg1 = spnego_gen_auth(blob_out);
			}

			/* now send that blob on its way */
			if (!cli_session_setup_blob_send(cli, msg1)) {
				DEBUG(3, ("Failed to send NTLMSSP/SPNEGO blob to server!\n"));
				nt_status = NT_STATUS_UNSUCCESSFUL;
			} else {
				blob = cli_session_setup_blob_receive(cli);

				nt_status = cli_nt_error(cli);
				if (cli_is_error(cli) && NT_STATUS_IS_OK(nt_status)) {
					if (cli->smb_rw_error == SMB_READ_BAD_SIG) {
						nt_status = NT_STATUS_ACCESS_DENIED;
					} else {
						nt_status = NT_STATUS_UNSUCCESSFUL;
					}
				}
			}
			data_blob_free(&msg1);
		}

		if (!blob.length) {
			if (NT_STATUS_IS_OK(nt_status)) {
				nt_status = NT_STATUS_UNSUCCESSFUL;
			}
		} else if ((turn == 1) && 
			   NT_STATUS_EQUAL(nt_status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {
			DATA_BLOB tmp_blob = data_blob_null;
			/* the server might give us back two challenges */
			if (!spnego_parse_challenge(blob, &blob_in, 
						    &tmp_blob)) {
				DEBUG(3,("Failed to parse challenges\n"));
				nt_status = NT_STATUS_INVALID_PARAMETER;
			}
			data_blob_free(&tmp_blob);
		} else {
			if (!spnego_parse_auth_response(blob, nt_status, OID_NTLMSSP, 
							&blob_in)) {
				DEBUG(3,("Failed to parse auth response\n"));
				if (NT_STATUS_IS_OK(nt_status) 
				    || NT_STATUS_EQUAL(nt_status, NT_STATUS_MORE_PROCESSING_REQUIRED)) 
					nt_status = NT_STATUS_INVALID_PARAMETER;
			}
		}
		data_blob_free(&blob);
		data_blob_free(&blob_out);
		turn++;
	} while (NT_STATUS_EQUAL(nt_status, NT_STATUS_MORE_PROCESSING_REQUIRED));

	data_blob_free(&blob_in);

	if (NT_STATUS_IS_OK(nt_status)) {

		if (cli->server_domain[0] == '\0') {
			fstrcpy(cli->server_domain, ntlmssp_state->server_domain);
		}
		cli_set_session_key(cli, ntlmssp_state->session_key);

		if (cli_simple_set_signing(
			    cli, ntlmssp_state->session_key, data_blob_null)) {

			if (!cli_check_sign_mac(cli, cli->inbuf, 1)) {
				nt_status = NT_STATUS_ACCESS_DENIED;
			}
		}
	}

	/* we have a reference conter on ntlmssp_state, if we are signing
	   then the state will be kept by the signing engine */

	ntlmssp_end(&ntlmssp_state);

	if (!NT_STATUS_IS_OK(nt_status)) {
		cli->vuid = 0;
	}
	return nt_status;
}

/****************************************************************************
 Do a spnego encrypted session setup.

 user_domain: The shortname of the domain the user/machine is a member of.
 dest_realm: The realm we're connecting to, if NULL we use our default realm.
****************************************************************************/

ADS_STATUS cli_session_setup_spnego(struct cli_state *cli, const char *user, 
			      const char *pass, const char *user_domain,
			      const char * dest_realm)
{
	char *principal = NULL;
	char *OIDs[ASN1_MAX_OIDS];
	int i;
	DATA_BLOB blob;
	const char *p = NULL;
	char *account = NULL;
	NTSTATUS status;

	DEBUG(3,("Doing spnego session setup (blob length=%lu)\n", (unsigned long)cli->secblob.length));

	/* the server might not even do spnego */
	if (cli->secblob.length <= 16) {
		DEBUG(3,("server didn't supply a full spnego negprot\n"));
		goto ntlmssp;
	}

#if 0
	file_save("negprot.dat", cli->secblob.data, cli->secblob.length);
#endif

	/* there is 16 bytes of GUID before the real spnego packet starts */
	blob = data_blob(cli->secblob.data+16, cli->secblob.length-16);

	/* The server sent us the first part of the SPNEGO exchange in the
	 * negprot reply. It is WRONG to depend on the principal sent in the
	 * negprot reply, but right now we do it. If we don't receive one,
	 * we try to best guess, then fall back to NTLM.  */
	if (!spnego_parse_negTokenInit(blob, OIDs, &principal) ||
			OIDs[0] == NULL) {
		data_blob_free(&blob);
		return ADS_ERROR_NT(NT_STATUS_INVALID_PARAMETER);
	}
	data_blob_free(&blob);

	/* make sure the server understands kerberos */
	for (i=0;OIDs[i];i++) {
		if (i == 0)
			DEBUG(3,("got OID=%s\n", OIDs[i]));
		else
			DEBUGADD(3,("got OID=%s\n", OIDs[i]));
		if (strcmp(OIDs[i], OID_KERBEROS5_OLD) == 0 ||
		    strcmp(OIDs[i], OID_KERBEROS5) == 0) {
			cli->got_kerberos_mechanism = True;
		}
		talloc_free(OIDs[i]);
	}

	DEBUG(3,("got principal=%s\n", principal ? principal : "<null>"));

	status = cli_set_username(cli, user);
	if (!NT_STATUS_IS_OK(status)) {
		return ADS_ERROR_NT(status);
	}

#ifdef HAVE_KRB5
	/* If password is set we reauthenticate to kerberos server
	 * and do not store results */

	if (cli->got_kerberos_mechanism && cli->use_kerberos) {
		ADS_STATUS rc;

		if (pass && *pass) {
			int ret;

			use_in_memory_ccache();
			ret = kerberos_kinit_password(user, pass, 0 /* no time correction for now */, NULL);

			if (ret){
				TALLOC_FREE(principal);
				DEBUG(0, ("Kinit failed: %s\n", error_message(ret)));
				if (cli->fallback_after_kerberos)
					goto ntlmssp;
				return ADS_ERROR_KRB5(ret);
			}
		}

		/* If we get a bad principal, try to guess it if
		   we have a valid host NetBIOS name.
		 */
		if (strequal(principal, ADS_IGNORE_PRINCIPAL)) {
			TALLOC_FREE(principal);
		}

		if (principal == NULL &&
			!is_ipaddress(cli->desthost) &&
			!strequal(STAR_SMBSERVER,
				cli->desthost)) {
			char *realm = NULL;
			char *machine = NULL;
			char *host = NULL;
			DEBUG(3,("cli_session_setup_spnego: got a "
				"bad server principal, trying to guess ...\n"));

			host = strchr_m(cli->desthost, '.');
			if (host) {
				/* We had a '.' in the name. */
				machine = SMB_STRNDUP(cli->desthost,
					host - cli->desthost);
			} else {
				machine = SMB_STRDUP(cli->desthost);
			}
			if (machine == NULL) {
				return ADS_ERROR_NT(NT_STATUS_NO_MEMORY);
			}

			if (dest_realm) {
				realm = SMB_STRDUP(dest_realm);
				strupper_m(realm);
			} else {
				if (host) {
					/* DNS name. */
					realm = kerberos_get_realm_from_hostname(cli->desthost);
				} else {
					/* NetBIOS name - use our realm. */
					realm = kerberos_get_default_realm_from_ccache();
				}
			}

			if (realm && *realm) {
				if (host) {
					/* DNS name. */
					principal = talloc_asprintf(talloc_tos(),
							"cifs/%s@%s",
							cli->desthost,
							realm);
				} else {
					/* NetBIOS name, use machine account. */
					principal = talloc_asprintf(talloc_tos(),
							"%s$@%s",
							machine,
							realm);
				}
				if (!principal) {
					SAFE_FREE(machine);
					SAFE_FREE(realm);
					return ADS_ERROR_NT(NT_STATUS_NO_MEMORY);
				}
				DEBUG(3,("cli_session_setup_spnego: guessed "
					"server principal=%s\n",
					principal ? principal : "<null>"));
			}
			SAFE_FREE(machine);
			SAFE_FREE(realm);
		}

		if (principal) {
			rc = cli_session_setup_kerberos(cli, principal,
				dest_realm);
			if (ADS_ERR_OK(rc) || !cli->fallback_after_kerberos) {
				TALLOC_FREE(principal);
				return rc;
			}
		}
	}
#endif

	TALLOC_FREE(principal);

ntlmssp:

	account = talloc_strdup(talloc_tos(), user);
	if (!account) {
		return ADS_ERROR_NT(NT_STATUS_NO_MEMORY);
	}

	/* when falling back to ntlmssp while authenticating with a machine
	 * account strip off the realm - gd */

	if ((p = strchr_m(user, '@')) != NULL) {
		account[PTR_DIFF(p,user)] = '\0';
	}

	return ADS_ERROR_NT(cli_session_setup_ntlmssp(cli, account, pass, user_domain));
}

/****************************************************************************
 Send a session setup. The username and workgroup is in UNIX character
 format and must be converted to DOS codepage format before sending. If the
 password is in plaintext, the same should be done.
****************************************************************************/

NTSTATUS cli_session_setup(struct cli_state *cli,
			   const char *user,
			   const char *pass, int passlen,
			   const char *ntpass, int ntpasslen,
			   const char *workgroup)
{
	char *p;
	fstring user2;

	if (user) {
		fstrcpy(user2, user);
	} else {
		user2[0] ='\0';
	}

	if (!workgroup) {
		workgroup = "";
	}

	/* allow for workgroups as part of the username */
	if ((p=strchr_m(user2,'\\')) || (p=strchr_m(user2,'/')) ||
	    (p=strchr_m(user2,*lp_winbind_separator()))) {
		*p = 0;
		user = p+1;
		workgroup = user2;
	}

	if (cli->protocol < PROTOCOL_LANMAN1) {
		return NT_STATUS_OK;
	}

	/* now work out what sort of session setup we are going to
           do. I have split this into separate functions to make the
           flow a bit easier to understand (tridge) */

	/* if its an older server then we have to use the older request format */

	if (cli->protocol < PROTOCOL_NT1) {
		if (!lp_client_lanman_auth() && passlen != 24 && (*pass)) {
			DEBUG(1, ("Server requested LM password but 'client lanman auth'"
				  " is disabled\n"));
			return NT_STATUS_ACCESS_DENIED;
		}

		if ((cli->sec_mode & NEGOTIATE_SECURITY_CHALLENGE_RESPONSE) == 0 &&
		    !lp_client_plaintext_auth() && (*pass)) {
			DEBUG(1, ("Server requested plaintext password but "
				  "'client plaintext auth' is disabled\n"));
			return NT_STATUS_ACCESS_DENIED;
		}

		return cli_session_setup_lanman2(cli, user, pass, passlen,
						 workgroup);
	}

	/* if no user is supplied then we have to do an anonymous connection.
	   passwords are ignored */

	if (!user || !*user)
		return cli_session_setup_guest(cli);

	/* if the server is share level then send a plaintext null
           password at this point. The password is sent in the tree
           connect */

	if ((cli->sec_mode & NEGOTIATE_SECURITY_USER_LEVEL) == 0) 
		return cli_session_setup_plaintext(cli, user, "", workgroup);

	/* if the server doesn't support encryption then we have to use 
	   plaintext. The second password is ignored */

	if ((cli->sec_mode & NEGOTIATE_SECURITY_CHALLENGE_RESPONSE) == 0) {
		if (!lp_client_plaintext_auth() && (*pass)) {
			DEBUG(1, ("Server requested plaintext password but "
				  "'client plaintext auth' is disabled\n"));
			return NT_STATUS_ACCESS_DENIED;
		}
		return cli_session_setup_plaintext(cli, user, pass, workgroup);
	}

	/* if the server supports extended security then use SPNEGO */

	if (cli->capabilities & CAP_EXTENDED_SECURITY) {
		ADS_STATUS status = cli_session_setup_spnego(cli, user, pass,
							     workgroup, NULL);
		if (!ADS_ERR_OK(status)) {
			DEBUG(3, ("SPNEGO login failed: %s\n", ads_errstr(status)));
			return ads_ntstatus(status);
		}
	} else {
		NTSTATUS status;

		/* otherwise do a NT1 style session setup */
		status = cli_session_setup_nt1(cli, user, pass, passlen,
					       ntpass, ntpasslen, workgroup);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(3,("cli_session_setup: NT1 session setup "
				 "failed: %s\n", nt_errstr(status)));
			return status;
		}
	}

	if (strstr(cli->server_type, "Samba")) {
		cli->is_samba = True;
	}

	return NT_STATUS_OK;
}

/****************************************************************************
 Send a uloggoff.
*****************************************************************************/

bool cli_ulogoff(struct cli_state *cli)
{
	memset(cli->outbuf,'\0',smb_size);
	cli_set_message(cli->outbuf,2,0,True);
	SCVAL(cli->outbuf,smb_com,SMBulogoffX);
	cli_setup_packet(cli);
	SSVAL(cli->outbuf,smb_vwv0,0xFF);
	SSVAL(cli->outbuf,smb_vwv2,0);  /* no additional info */

	cli_send_smb(cli);
	if (!cli_receive_smb(cli))
		return False;

	if (cli_is_error(cli)) {
		return False;
	}

        cli->vuid = -1;
        return True;
}

/****************************************************************************
 Send a tconX.
****************************************************************************/

struct cli_tcon_andx_state {
	struct cli_state *cli;
	uint16_t vwv[4];
	struct iovec bytes;
};

static void cli_tcon_andx_done(struct tevent_req *subreq);

struct tevent_req *cli_tcon_andx_create(TALLOC_CTX *mem_ctx,
					struct event_context *ev,
					struct cli_state *cli,
					const char *share, const char *dev,
					const char *pass, int passlen,
					struct tevent_req **psmbreq)
{
	struct tevent_req *req, *subreq;
	struct cli_tcon_andx_state *state;
	fstring pword;
	uint16_t *vwv;
	char *tmp = NULL;
	uint8_t *bytes;

	*psmbreq = NULL;

	req = tevent_req_create(mem_ctx, &state, struct cli_tcon_andx_state);
	if (req == NULL) {
		return NULL;
	}
	state->cli = cli;
	vwv = state->vwv;

	fstrcpy(cli->share, share);

	/* in user level security don't send a password now */
	if (cli->sec_mode & NEGOTIATE_SECURITY_USER_LEVEL) {
		passlen = 1;
		pass = "";
	} else if (pass == NULL) {
		DEBUG(1, ("Server not using user level security and no "
			  "password supplied.\n"));
		goto access_denied;
	}

	if ((cli->sec_mode & NEGOTIATE_SECURITY_CHALLENGE_RESPONSE) &&
	    *pass && passlen != 24) {
		if (!lp_client_lanman_auth()) {
			DEBUG(1, ("Server requested LANMAN password "
				  "(share-level security) but "
				  "'client lanman auth' is disabled\n"));
			goto access_denied;
		}

		/*
		 * Non-encrypted passwords - convert to DOS codepage before
		 * encryption.
		 */
		passlen = 24;
		SMBencrypt(pass, cli->secblob.data, (uchar *)pword);
	} else {
		if((cli->sec_mode & (NEGOTIATE_SECURITY_USER_LEVEL
				     |NEGOTIATE_SECURITY_CHALLENGE_RESPONSE))
		   == 0) {
			if (!lp_client_plaintext_auth() && (*pass)) {
				DEBUG(1, ("Server requested plaintext "
					  "password but 'client plaintext "
					  "auth' is disabled\n"));
				goto access_denied;
			}

			/*
			 * Non-encrypted passwords - convert to DOS codepage
			 * before using.
			 */
			passlen = clistr_push(cli, pword, pass, sizeof(pword),
					      STR_TERMINATE);
			if (passlen == -1) {
				DEBUG(1, ("clistr_push(pword) failed\n"));
				goto access_denied;
			}
		} else {
			if (passlen) {
				memcpy(pword, pass, passlen);
			}
		}
	}

	SCVAL(vwv+0, 0, 0xFF);
	SCVAL(vwv+0, 1, 0);
	SSVAL(vwv+1, 0, 0);
	SSVAL(vwv+2, 0, TCONX_FLAG_EXTENDED_RESPONSE);
	SSVAL(vwv+3, 0, passlen);

	if (passlen) {
		bytes = (uint8_t *)talloc_memdup(state, pword, passlen);
	} else {
		bytes = talloc_array(state, uint8_t, 0);
	}

	/*
	 * Add the sharename
	 */
	tmp = talloc_asprintf_strupper_m(talloc_tos(), "\\\\%s\\%s",
					 cli->desthost, share);
	if (tmp == NULL) {
		TALLOC_FREE(req);
		return NULL;
	}
	bytes = smb_bytes_push_str(bytes, cli_ucs2(cli), tmp, strlen(tmp)+1,
				   NULL);
	TALLOC_FREE(tmp);

	/*
	 * Add the devicetype
	 */
	tmp = talloc_strdup_upper(talloc_tos(), dev);
	if (tmp == NULL) {
		TALLOC_FREE(req);
		return NULL;
	}
	bytes = smb_bytes_push_str(bytes, false, tmp, strlen(tmp)+1, NULL);
	TALLOC_FREE(tmp);

	if (bytes == NULL) {
		TALLOC_FREE(req);
		return NULL;
	}

	state->bytes.iov_base = (void *)bytes;
	state->bytes.iov_len = talloc_get_size(bytes);

	subreq = cli_smb_req_create(state, ev, cli, SMBtconX, 0, 4, vwv,
				    1, &state->bytes);
	if (subreq == NULL) {
		TALLOC_FREE(req);
		return NULL;
	}
	tevent_req_set_callback(subreq, cli_tcon_andx_done, req);
	*psmbreq = subreq;
	return req;

 access_denied:
	tevent_req_nterror(req, NT_STATUS_ACCESS_DENIED);
	return tevent_req_post(req, ev);
}

struct tevent_req *cli_tcon_andx_send(TALLOC_CTX *mem_ctx,
				      struct event_context *ev,
				      struct cli_state *cli,
				      const char *share, const char *dev,
				      const char *pass, int passlen)
{
	struct tevent_req *req, *subreq;
	NTSTATUS status;

	req = cli_tcon_andx_create(mem_ctx, ev, cli, share, dev, pass, passlen,
				   &subreq);
	if (req == NULL) {
		return NULL;
	}
	if (subreq == NULL) {
		return req;
	}
	status = cli_smb_req_send(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return tevent_req_post(req, ev);
	}
	return req;
}

static void cli_tcon_andx_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct cli_tcon_andx_state *state = tevent_req_data(
		req, struct cli_tcon_andx_state);
	struct cli_state *cli = state->cli;
	char *inbuf = (char *)cli_smb_inbuf(subreq);
	uint8_t wct;
	uint16_t *vwv;
	uint32_t num_bytes;
	uint8_t *bytes;
	NTSTATUS status;

	status = cli_smb_recv(subreq, 0, &wct, &vwv, &num_bytes, &bytes);
	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(subreq);
		tevent_req_nterror(req, status);
		return;
	}

	clistr_pull(inbuf, cli->dev, bytes, sizeof(fstring), num_bytes,
		    STR_TERMINATE|STR_ASCII);

	if ((cli->protocol >= PROTOCOL_NT1) && (num_bytes == 3)) {
		/* almost certainly win95 - enable bug fixes */
		cli->win95 = True;
	}

	/*
	 * Make sure that we have the optional support 16-bit field. WCT > 2.
	 * Avoids issues when connecting to Win9x boxes sharing files
	 */

	cli->dfsroot = false;

	if ((wct > 2) && (cli->protocol >= PROTOCOL_LANMAN2)) {
		cli->dfsroot = ((SVAL(vwv+2, 0) & SMB_SHARE_IN_DFS) != 0);
	}

	cli->cnum = SVAL(inbuf,smb_tid);
	tevent_req_done(req);
}

NTSTATUS cli_tcon_andx_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}

NTSTATUS cli_tcon_andx(struct cli_state *cli, const char *share,
		       const char *dev, const char *pass, int passlen)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev;
	struct tevent_req *req;
	NTSTATUS status = NT_STATUS_OK;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}

	ev = event_context_init(frame);
	if (ev == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	req = cli_tcon_andx_send(frame, ev, cli, share, dev, pass, passlen);
	if (req == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	if (!tevent_req_poll(req, ev)) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}
	
	status = cli_tcon_andx_recv(req);
 fail:
	TALLOC_FREE(frame);
	if (!NT_STATUS_IS_OK(status)) {
		cli_set_error(cli, status);
	}
	return status;
}

/****************************************************************************
 Send a tree disconnect.
****************************************************************************/

bool cli_tdis(struct cli_state *cli)
{
	memset(cli->outbuf,'\0',smb_size);
	cli_set_message(cli->outbuf,0,0,True);
	SCVAL(cli->outbuf,smb_com,SMBtdis);
	SSVAL(cli->outbuf,smb_tid,cli->cnum);
	cli_setup_packet(cli);

	cli_send_smb(cli);
	if (!cli_receive_smb(cli))
		return False;

	if (cli_is_error(cli)) {
		return False;
	}

	cli->cnum = -1;
	return True;
}

/****************************************************************************
 Send a negprot command.
****************************************************************************/

void cli_negprot_sendsync(struct cli_state *cli)
{
	char *p;
	int numprots;

	if (cli->protocol < PROTOCOL_NT1)
		cli->use_spnego = False;

	memset(cli->outbuf,'\0',smb_size);

	/* setup the protocol strings */
	cli_set_message(cli->outbuf,0,0,True);

	p = smb_buf(cli->outbuf);
	for (numprots=0; numprots < ARRAY_SIZE(prots); numprots++) {
		if (prots[numprots].prot > cli->protocol) {
			break;
		}
		*p++ = 2;
		p += clistr_push(cli, p, prots[numprots].name, -1, STR_TERMINATE);
	}

	SCVAL(cli->outbuf,smb_com,SMBnegprot);
	cli_setup_bcc(cli, p);
	cli_setup_packet(cli);

	SCVAL(smb_buf(cli->outbuf),0,2);

	cli_send_smb(cli);
}

/****************************************************************************
 Send a negprot command.
****************************************************************************/

struct cli_negprot_state {
	struct cli_state *cli;
};

static void cli_negprot_done(struct tevent_req *subreq);

struct tevent_req *cli_negprot_send(TALLOC_CTX *mem_ctx,
				    struct event_context *ev,
				    struct cli_state *cli)
{
	struct tevent_req *req, *subreq;
	struct cli_negprot_state *state;
	uint8_t *bytes = NULL;
	int numprots;
	uint16_t cnum;

	req = tevent_req_create(mem_ctx, &state, struct cli_negprot_state);
	if (req == NULL) {
		return NULL;
	}
	state->cli = cli;

	if (cli->protocol < PROTOCOL_NT1){
		cli->use_spnego = False;
	}

	//- 20120530 JerryLin add
	cli->use_spnego = True;
		
	/* setup the protocol strings */
	for (numprots=0; numprots < ARRAY_SIZE(prots); numprots++) {
		uint8_t c = 2;
		if (prots[numprots].prot > cli->protocol) {
			break;
		}
		bytes = (uint8_t *)talloc_append_blob(
			state, bytes, data_blob_const(&c, sizeof(c)));
		if (tevent_req_nomem(bytes, req)) {
			return tevent_req_post(req, ev);
		}
		bytes = smb_bytes_push_str(bytes, false,
					   prots[numprots].name,
					   strlen(prots[numprots].name)+1,
					   NULL);
		if (tevent_req_nomem(bytes, req)) {
			return tevent_req_post(req, ev);
		}
	}

	cnum = cli->cnum;

	cli->cnum = 0;
	subreq = cli_smb_send(state, ev, cli, SMBnegprot, 0, 0, NULL,
			      talloc_get_size(bytes), bytes);
	cli->cnum = cnum;

	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_negprot_done, req);
	return req;
}

static void cli_negprot_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct cli_negprot_state *state = tevent_req_data(
		req, struct cli_negprot_state);
	struct cli_state *cli = state->cli;
	uint8_t wct;
	uint16_t *vwv;
	uint32_t num_bytes;
	uint8_t *bytes;
	NTSTATUS status;
	uint16_t protnum;
	
	status = cli_smb_recv(subreq, 1, &wct, &vwv, &num_bytes, &bytes);
	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(subreq);
		tevent_req_nterror(req, status);
		return;
	}

	protnum = SVAL(vwv, 0);
	
	if ((protnum >= ARRAY_SIZE(prots))
	    || (prots[protnum].prot > cli->protocol)) {
		tevent_req_nterror(req, NT_STATUS_INVALID_NETWORK_RESPONSE);
		return;
	}

	cli->protocol = prots[protnum].prot;
	
	if ((cli->protocol < PROTOCOL_NT1) &&
	    client_is_signing_mandatory(cli)) {
		DEBUG(0,("cli_negprot: SMB signing is mandatory and the selected protocol level doesn't support it.\n"));	
		tevent_req_nterror(req, NT_STATUS_ACCESS_DENIED);
		return;
	}
	
	if (cli->protocol >= PROTOCOL_NT1) {    
		struct timespec ts;
		bool negotiated_smb_signing = false;
		
		/* NT protocol */
		cli->sec_mode = CVAL(vwv + 1, 0);
		cli->max_mux = SVAL(vwv + 1, 1);
		cli->max_xmit = IVAL(vwv + 3, 1);
		cli->sesskey = IVAL(vwv + 7, 1);
		cli->serverzone = SVALS(vwv + 15, 1);
		cli->serverzone *= 60;
		/* this time arrives in real GMT */
		ts = interpret_long_date(((char *)(vwv+11))+1);
		cli->servertime = ts.tv_sec;
		cli->secblob = data_blob(bytes, num_bytes);
		cli->capabilities = IVAL(vwv + 9, 1);
		if (cli->capabilities & CAP_RAW_MODE) {
			cli->readbraw_supported = True;
			cli->writebraw_supported = True;      
		}
		
		/* work out if they sent us a workgroup */
		if (!(cli->capabilities & CAP_EXTENDED_SECURITY) &&
		    smb_buflen(cli->inbuf) > 8) {
			clistr_pull(cli->inbuf, cli->server_domain,
				    bytes+8, sizeof(cli->server_domain),
				    num_bytes-8,
				    STR_UNICODE|STR_NOALIGN);
		}
		
		/*
		 * As signing is slow we only turn it on if either the client or
		 * the server require it. JRA.
		 */

		if (cli->sec_mode & NEGOTIATE_SECURITY_SIGNATURES_REQUIRED) {			
			/* Fail if server says signing is mandatory and we don't want to support it. */
			if (!client_is_signing_allowed(cli)) {
				DEBUG(0,("cli_negprot: SMB signing is mandatory and we have disabled it.\n"));
				tevent_req_nterror(req,
						   NT_STATUS_ACCESS_DENIED);
				return;
			}
			negotiated_smb_signing = true;
		} else if (client_is_signing_mandatory(cli) && client_is_signing_allowed(cli)) {		
			/* Fail if client says signing is mandatory and the server doesn't support it. */
			if (!(cli->sec_mode & NEGOTIATE_SECURITY_SIGNATURES_ENABLED)) {
				DEBUG(1,("cli_negprot: SMB signing is mandatory and the server doesn't support it.\n"));
				tevent_req_nterror(req,
						   NT_STATUS_ACCESS_DENIED);
				return;
			}
			negotiated_smb_signing = true;
		} else if (cli->sec_mode & NEGOTIATE_SECURITY_SIGNATURES_ENABLED) {
			negotiated_smb_signing = true;
		}

		if (negotiated_smb_signing) {			
			cli_set_signing_negotiated(cli);
		}

		if (cli->capabilities & (CAP_LARGE_READX|CAP_LARGE_WRITEX)) {
			SAFE_FREE(cli->outbuf);
			SAFE_FREE(cli->inbuf);
			cli->outbuf = (char *)SMB_MALLOC(CLI_SAMBA_MAX_LARGE_READX_SIZE+LARGE_WRITEX_HDR_SIZE+SAFETY_MARGIN);
			cli->inbuf = (char *)SMB_MALLOC(CLI_SAMBA_MAX_LARGE_READX_SIZE+LARGE_WRITEX_HDR_SIZE+SAFETY_MARGIN);
			cli->bufsize = CLI_SAMBA_MAX_LARGE_READX_SIZE + LARGE_WRITEX_HDR_SIZE;
		}

	} else if (cli->protocol >= PROTOCOL_LANMAN1) {
		cli->use_spnego = False;
		cli->sec_mode = SVAL(vwv + 1, 0);
		cli->max_xmit = SVAL(vwv + 2, 0);
		cli->max_mux = SVAL(vwv + 3, 0);
		cli->sesskey = IVAL(vwv + 6, 0);
		cli->serverzone = SVALS(vwv + 10, 0);
		cli->serverzone *= 60;		
		/* this time is converted to GMT by make_unix_date */
		cli->servertime = cli_make_unix_date(
			cli, (char *)(vwv + 8));
		cli->readbraw_supported = ((SVAL(vwv + 5, 0) & 0x1) != 0);
		cli->writebraw_supported = ((SVAL(vwv + 5, 0) & 0x2) != 0);
		cli->secblob = data_blob(bytes, num_bytes);
	} else {
		/* the old core protocol */
		cli->use_spnego = False;
		cli->sec_mode = 0;
		cli->serverzone = get_time_zone(time(NULL));		
	}

	cli->max_xmit = MIN(cli->max_xmit, CLI_BUFFER_SIZE);

	/* a way to force ascii SMB */
	if (getenv("CLI_FORCE_ASCII"))
		cli->capabilities &= ~CAP_UNICODE;

	tevent_req_done(req);
}

NTSTATUS cli_negprot_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}

NTSTATUS cli_negprot(struct cli_state *cli)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev;
	struct tevent_req *req;
	NTSTATUS status = NT_STATUS_OK;
	
	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}
	
	ev = event_context_init(frame);
	if (ev == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}
	
	req = cli_negprot_send(frame, ev, cli);
	if (req == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}
	
	if (!tevent_req_poll(req, ev)) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}
	
	status = cli_negprot_recv(req);
 fail:
	TALLOC_FREE(frame);
	if (!NT_STATUS_IS_OK(status)) {
		cli_set_error(cli, status);
	}
	return status;
}

/****************************************************************************
 Send a session request. See rfc1002.txt 4.3 and 4.3.2.
****************************************************************************/

bool cli_session_request(struct cli_state *cli,
			 struct nmb_name *calling, struct nmb_name *called)
{
	char *p;
	int len = 4;
	int namelen = 0;
	char *tmp;

	/* 445 doesn't have session request */
	if (cli->port == 445)
		return True;

	memcpy(&(cli->calling), calling, sizeof(*calling));
	memcpy(&(cli->called ), called , sizeof(*called ));

	/* put in the destination name */

	tmp = name_mangle(talloc_tos(), cli->called.name,
			  cli->called.name_type);
	if (tmp == NULL) {
		return false;
	}

	p = cli->outbuf+len;
	namelen = name_len((unsigned char *)tmp, talloc_get_size(tmp));
	if (namelen > 0) {
		memcpy(p, tmp, namelen);
		len += namelen;
	}
	TALLOC_FREE(tmp);

	/* and my name */

	tmp = name_mangle(talloc_tos(), cli->calling.name,
			  cli->calling.name_type);
	if (tmp == NULL) {
		return false;
	}

	p = cli->outbuf+len;
	namelen = name_len((unsigned char *)tmp, talloc_get_size(tmp));
	if (namelen > 0) {
		memcpy(p, tmp, namelen);
		len += namelen;
	}
	TALLOC_FREE(tmp);

	/* send a session request (RFC 1002) */
	/* setup the packet length
         * Remove four bytes from the length count, since the length
         * field in the NBT Session Service header counts the number
         * of bytes which follow.  The cli_send_smb() function knows
         * about this and accounts for those four bytes.
         * CRH.
         */
        len -= 4;
	_smb_setlen(cli->outbuf,len);
	SCVAL(cli->outbuf,0,0x81);

	cli_send_smb(cli);
	DEBUG(5,("Sent session request\n"));

	if (!cli_receive_smb(cli))
		return False;

	if (CVAL(cli->inbuf,0) == 0x84) {
		/* C. Hoch  9/14/95 Start */
		/* For information, here is the response structure.
		 * We do the byte-twiddling to for portability.
		struct RetargetResponse{
		unsigned char type;
		unsigned char flags;
		int16 length;
		int32 ip_addr;
		int16 port;
		};
		*/
		uint16_t port = (CVAL(cli->inbuf,8)<<8)+CVAL(cli->inbuf,9);
		struct in_addr dest_ip;
		NTSTATUS status;

		/* SESSION RETARGET */
		putip((char *)&dest_ip,cli->inbuf+4);
		in_addr_to_sockaddr_storage(&cli->dest_ss, dest_ip);

		status = open_socket_out(&cli->dest_ss, port,
					 LONG_CONNECT_TIMEOUT, &cli->fd);
		if (!NT_STATUS_IS_OK(status)) {
			return False;
		}

		DEBUG(3,("Retargeted\n"));

		set_socket_options(cli->fd, lp_socket_options());

		/* Try again */
		{
			static int depth;
			bool ret;
			if (depth > 4) {
				DEBUG(0,("Retarget recursion - failing\n"));
				return False;
			}
			depth++;
			ret = cli_session_request(cli, calling, called);
			depth--;
			return ret;
		}
	} /* C. Hoch 9/14/95 End */

	if (CVAL(cli->inbuf,0) != 0x82) {
                /* This is the wrong place to put the error... JRA. */
		cli->rap_error = CVAL(cli->inbuf,4);
		return False;
	}
	return(True);
}

struct fd_struct {
	int fd;
};

static void smb_sock_connected(struct tevent_req *req)
{
	struct fd_struct *pfd = tevent_req_callback_data(
		req, struct fd_struct);
	int fd;
	NTSTATUS status;

	status = open_socket_out_defer_recv(req, &fd);
	if (NT_STATUS_IS_OK(status)) {
		pfd->fd = fd;
	}
}

static NTSTATUS open_smb_socket(const struct sockaddr_storage *pss,
				uint16_t *port, int timeout, int *pfd)
{
	struct event_context *ev;
	struct tevent_req *r139, *r445;
	struct fd_struct *fd139, *fd445;
	NTSTATUS status = NT_STATUS_NO_MEMORY;

	if (*port != 0) {
		return open_socket_out(pss, *port, timeout, pfd);
	}

	ev = event_context_init(talloc_tos());
	if (ev == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	fd139 = talloc(ev, struct fd_struct);
	if (fd139 == NULL) {
		goto done;
	}
	fd139->fd = -1;

	fd445 = talloc(ev, struct fd_struct);
	if (fd445 == NULL) {
		goto done;
	}
	fd445->fd = -1;

	r445 = open_socket_out_defer_send(ev, ev, timeval_set(0, 0),
					  pss, 445, timeout);
	r139 = open_socket_out_defer_send(ev, ev, timeval_set(0, 3000),
					  pss, 139, timeout);
	if ((r445 == NULL) || (r139 == NULL)) {
		goto done;
	}
	tevent_req_set_callback(r445, smb_sock_connected, fd445);
	tevent_req_set_callback(r139, smb_sock_connected, fd139);

	while ((fd445->fd == -1) && (fd139->fd == -1)
	       && (tevent_req_is_in_progress(r139)
		   || tevent_req_is_in_progress(r445))) {
		event_loop_once(ev);
	}

	if ((fd139->fd != -1) && (fd445->fd != -1)) {
		close(fd139->fd);
		fd139->fd = -1;
	}

	if (fd445->fd != -1) {
		*port = 445;
		*pfd = fd445->fd;
		status = NT_STATUS_OK;
		goto done;
	}
	if (fd139->fd != -1) {
		*port = 139;
		*pfd = fd139->fd;
		status = NT_STATUS_OK;
		goto done;
	}

	status = open_socket_out_defer_recv(r445, &fd445->fd);
 done:
	TALLOC_FREE(ev);
	return status;
}

/****************************************************************************
 Open the client sockets.
****************************************************************************/

NTSTATUS cli_connect(struct cli_state *cli,
		const char *host,
		struct sockaddr_storage *dest_ss)

{
	int name_type = 0x20;
	TALLOC_CTX *frame = talloc_stackframe();
	unsigned int num_addrs = 0;
	unsigned int i = 0;
	struct sockaddr_storage *ss_arr = NULL;
	char *p = NULL;

	/* reasonable default hostname */
	if (!host) {
		host = STAR_SMBSERVER;
	}

	fstrcpy(cli->desthost, host);

	/* allow hostnames of the form NAME#xx and do a netbios lookup */
	if ((p = strchr(cli->desthost, '#'))) {
		name_type = strtol(p+1, NULL, 16);
		*p = 0;
	}

	if (!dest_ss || is_zero_addr((struct sockaddr *)dest_ss)) {
		NTSTATUS status =resolve_name_list(frame,
					cli->desthost,
					name_type,
					&ss_arr,
					&num_addrs);
		if (!NT_STATUS_IS_OK(status)) {
			TALLOC_FREE(frame);
			return NT_STATUS_BAD_NETWORK_NAME;
                }
	} else {
		num_addrs = 1;
		ss_arr = TALLOC_P(frame, struct sockaddr_storage);
		if (!ss_arr) {
			TALLOC_FREE(frame);
			return NT_STATUS_NO_MEMORY;
		}
		*ss_arr = *dest_ss;
	}

	for (i = 0; i < num_addrs; i++) {
		cli->dest_ss = ss_arr[i];
		if (getenv("LIBSMB_PROG")) {
			cli->fd = sock_exec(getenv("LIBSMB_PROG"));
		} else {
			uint16_t port = cli->port;
			NTSTATUS status;
			status = open_smb_socket(&cli->dest_ss, &port,
						 cli->timeout, &cli->fd);
			if (NT_STATUS_IS_OK(status)) {
				cli->port = port;
			}
		}
		if (cli->fd == -1) {
			char addr[INET6_ADDRSTRLEN];
			print_sockaddr(addr, sizeof(addr), &ss_arr[i]);
			DEBUG(2,("Error connecting to %s (%s)\n",
				 dest_ss?addr:host,strerror(errno)));
		} else {
			/* Exit from loop on first connection. */
			break;
		}
	}

	if (cli->fd == -1) {
		TALLOC_FREE(frame);
		return map_nt_error_from_unix(errno);
	}

	if (dest_ss) {
		*dest_ss = cli->dest_ss;
	}

	set_socket_options(cli->fd, lp_socket_options());

	TALLOC_FREE(frame);
	return NT_STATUS_OK;
}

/**
   establishes a connection to after the negprot. 
   @param output_cli A fully initialised cli structure, non-null only on success
   @param dest_host The netbios name of the remote host
   @param dest_ss (optional) The the destination IP, NULL for name based lookup
   @param port (optional) The destination port (0 for default)
   @param retry bool. Did this connection fail with a retryable error ?

*/
NTSTATUS cli_start_connection(struct cli_state **output_cli, 
			      const char *my_name, 
			      const char *dest_host, 
			      struct sockaddr_storage *dest_ss, int port,
			      int signing_state, int flags,
			      bool *retry) 
{
	NTSTATUS nt_status;
	struct nmb_name calling;
	struct nmb_name called;
	struct cli_state *cli;
	struct sockaddr_storage ss;

	if (retry)
		*retry = False;

	if (!my_name) 
		my_name = global_myname();

	if (!(cli = cli_initialise_ex(signing_state))) {
		return NT_STATUS_NO_MEMORY;
	}

	make_nmb_name(&calling, my_name, 0x0);
	make_nmb_name(&called , dest_host, 0x20);

	cli_set_port(cli, port);
	cli_set_timeout(cli, 10000); /* 10 seconds. */

	if (dest_ss) {
		ss = *dest_ss;
	} else {
		zero_sockaddr(&ss);
	}

again:

	DEBUG(3,("Connecting to host=%s\n", dest_host));

	nt_status = cli_connect(cli, dest_host, &ss);
	if (!NT_STATUS_IS_OK(nt_status)) {
		char addr[INET6_ADDRSTRLEN];
		print_sockaddr(addr, sizeof(addr), &ss);
		DEBUG(1,("cli_start_connection: failed to connect to %s (%s). Error %s\n",
			 nmb_namestr(&called), addr, nt_errstr(nt_status) ));
		cli_shutdown(cli);
		return nt_status;
	}

	if (retry)
		*retry = True;

	if (!cli_session_request(cli, &calling, &called)) {
		char *p;
		DEBUG(1,("session request to %s failed (%s)\n",
			 called.name, cli_errstr(cli)));
		if ((p=strchr(called.name, '.')) && !is_ipaddress(called.name)) {
			*p = 0;
			goto again;
		}
		if (strcmp(called.name, STAR_SMBSERVER)) {
			make_nmb_name(&called , STAR_SMBSERVER, 0x20);
			goto again;
		}
		return NT_STATUS_BAD_NETWORK_NAME;
	}

	if (flags & CLI_FULL_CONNECTION_DONT_SPNEGO)
		cli->use_spnego = False;
	else if (flags & CLI_FULL_CONNECTION_USE_KERBEROS)
		cli->use_kerberos = True;

	if ((flags & CLI_FULL_CONNECTION_FALLBACK_AFTER_KERBEROS) &&
	     cli->use_kerberos) {
		cli->fallback_after_kerberos = true;
	}
	if (flags & CLI_FULL_CONNECTION_USE_CCACHE) {
		cli->use_ccache = true;
	}

	nt_status = cli_negprot(cli);
	if (!NT_STATUS_IS_OK(nt_status)) {
		DEBUG(1, ("failed negprot: %s\n", nt_errstr(nt_status)));		
		cli_shutdown(cli);
		return nt_status;
	}

	*output_cli = cli;
	return NT_STATUS_OK;
}


/**
   establishes a connection right up to doing tconX, password specified.
   @param output_cli A fully initialised cli structure, non-null only on success
   @param dest_host The netbios name of the remote host
   @param dest_ip (optional) The the destination IP, NULL for name based lookup
   @param port (optional) The destination port (0 for default)
   @param service (optional) The share to make the connection to.  Should be 'unqualified' in any way.
   @param service_type The 'type' of serivice. 
   @param user Username, unix string
   @param domain User's domain
   @param password User's password, unencrypted unix string.
   @param retry bool. Did this connection fail with a retryable error ?
*/

NTSTATUS cli_full_connection(struct cli_state **output_cli, 
			     const char *my_name, 
			     const char *dest_host, 
			     struct sockaddr_storage *dest_ss, int port,
			     const char *service, const char *service_type,
			     const char *user, const char *domain, 
			     const char *password, int flags,
			     int signing_state,
			     bool *retry) 
{
	NTSTATUS nt_status;
	struct cli_state *cli = NULL;
	int pw_len = password ? strlen(password)+1 : 0;

	*output_cli = NULL;

	if (password == NULL) {
		password = "";
	}

	nt_status = cli_start_connection(&cli, my_name, dest_host,
					 dest_ss, port, signing_state,
					 flags, retry);

	if (!NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	}

	cli->use_oplocks = ((flags & CLI_FULL_CONNECTION_OPLOCKS) != 0);
	cli->use_level_II_oplocks =
		((flags & CLI_FULL_CONNECTION_LEVEL_II_OPLOCKS) != 0);

	nt_status = cli_session_setup(cli, user, password, pw_len, password,
				      pw_len, domain);
	if (!NT_STATUS_IS_OK(nt_status)) {

		if (!(flags & CLI_FULL_CONNECTION_ANONYMOUS_FALLBACK)) {
			DEBUG(1,("failed session setup with %s\n",
				 nt_errstr(nt_status)));
			cli_shutdown(cli);
			return nt_status;
		}

		nt_status = cli_session_setup(cli, "", "", 0, "", 0, domain);
		if (!NT_STATUS_IS_OK(nt_status)) {
			DEBUG(1,("anonymous failed session setup with %s\n",
				 nt_errstr(nt_status)));
			cli_shutdown(cli);
			return nt_status;
		}
	}

	if (service) {
		nt_status = cli_tcon_andx(cli, service, service_type, password,
					  pw_len);
		if (!NT_STATUS_IS_OK(nt_status)) {
			DEBUG(1,("failed tcon_X with %s\n", nt_errstr(nt_status)));
			cli_shutdown(cli);
			if (NT_STATUS_IS_OK(nt_status)) {
				nt_status = NT_STATUS_UNSUCCESSFUL;
			}
			return nt_status;
		}
	}

	nt_status = cli_init_creds(cli, user, domain, password);
	if (!NT_STATUS_IS_OK(nt_status)) {
		cli_shutdown(cli);
		return nt_status;
	}

	*output_cli = cli;
	return NT_STATUS_OK;
}

/****************************************************************************
 Attempt a NetBIOS session request, falling back to *SMBSERVER if needed.
****************************************************************************/

bool attempt_netbios_session_request(struct cli_state **ppcli, const char *srchost, const char *desthost,
                                     struct sockaddr_storage *pdest_ss)
{
	struct nmb_name calling, called;

	make_nmb_name(&calling, srchost, 0x0);

	/*
	 * If the called name is an IP address
	 * then use *SMBSERVER immediately.
	 */

	if(is_ipaddress(desthost)) {
		make_nmb_name(&called, STAR_SMBSERVER, 0x20);
	} else {
		make_nmb_name(&called, desthost, 0x20);
	}

	if (!cli_session_request(*ppcli, &calling, &called)) {
		NTSTATUS status;
		struct nmb_name smbservername;

		make_nmb_name(&smbservername, STAR_SMBSERVER, 0x20);

		/*
		 * If the name wasn't *SMBSERVER then
		 * try with *SMBSERVER if the first name fails.
		 */

		if (nmb_name_equal(&called, &smbservername)) {

			/*
			 * The name used was *SMBSERVER, don't bother with another name.
			 */

			DEBUG(0,("attempt_netbios_session_request: %s rejected the session for name *SMBSERVER \
with error %s.\n", desthost, cli_errstr(*ppcli) ));
			return False;
		}

		/* Try again... */
		cli_shutdown(*ppcli);

		*ppcli = cli_initialise();
		if (!*ppcli) {
			/* Out of memory... */
			return False;
		}

		status = cli_connect(*ppcli, desthost, pdest_ss);
		if (!NT_STATUS_IS_OK(status) ||
				!cli_session_request(*ppcli, &calling, &smbservername)) {
			DEBUG(0,("attempt_netbios_session_request: %s rejected the session for \
name *SMBSERVER with error %s\n", desthost, cli_errstr(*ppcli) ));
			return False;
		}
	}

	return True;
}

/****************************************************************************
 Send an old style tcon.
****************************************************************************/
NTSTATUS cli_raw_tcon(struct cli_state *cli, 
		      const char *service, const char *pass, const char *dev,
		      uint16 *max_xmit, uint16 *tid)
{
	char *p;

	if (!lp_client_plaintext_auth() && (*pass)) {
		DEBUG(1, ("Server requested plaintext password but 'client "
			  "plaintext auth' is disabled\n"));
		return NT_STATUS_ACCESS_DENIED;
	}

	memset(cli->outbuf,'\0',smb_size);
	memset(cli->inbuf,'\0',smb_size);

	cli_set_message(cli->outbuf, 0, 0, True);
	SCVAL(cli->outbuf,smb_com,SMBtcon);
	cli_setup_packet(cli);

	p = smb_buf(cli->outbuf);
	*p++ = 4; p += clistr_push(cli, p, service, -1, STR_TERMINATE | STR_NOALIGN);
	*p++ = 4; p += clistr_push(cli, p, pass, -1, STR_TERMINATE | STR_NOALIGN);
	*p++ = 4; p += clistr_push(cli, p, dev, -1, STR_TERMINATE | STR_NOALIGN);

	cli_setup_bcc(cli, p);

	cli_send_smb(cli);
	if (!cli_receive_smb(cli)) {
		return NT_STATUS_UNEXPECTED_NETWORK_ERROR;
	}

	if (cli_is_error(cli)) {
		return cli_nt_error(cli);
	}

	*max_xmit = SVAL(cli->inbuf, smb_vwv0);
	*tid = SVAL(cli->inbuf, smb_vwv1);

	return NT_STATUS_OK;
}

/* Return a cli_state pointing at the IPC$ share for the given server */

struct cli_state *get_ipc_connect(char *server,
				struct sockaddr_storage *server_ss,
				const struct user_auth_info *user_info)
{
        struct cli_state *cli;
	NTSTATUS nt_status;
	uint32_t flags = CLI_FULL_CONNECTION_ANONYMOUS_FALLBACK;

	if (user_info->use_kerberos) {
		flags |= CLI_FULL_CONNECTION_USE_KERBEROS;
	}

	nt_status = cli_full_connection(&cli, NULL, server, server_ss, 0, "IPC$", "IPC", 
					user_info->username ? user_info->username : "",
					lp_workgroup(),
					user_info->password ? user_info->password : "",
					flags,
					Undefined, NULL);

	if (NT_STATUS_IS_OK(nt_status)) {
		return cli;
	} else if (is_ipaddress(server)) {
	    /* windows 9* needs a correct NMB name for connections */
	    fstring remote_name;

	    if (name_status_find("*", 0, 0, server_ss, remote_name)) {
		cli = get_ipc_connect(remote_name, server_ss, user_info);
		if (cli)
		    return cli;
	    }
	}
	return NULL;
}

/*
 * Given the IP address of a master browser on the network, return its
 * workgroup and connect to it.
 *
 * This function is provided to allow additional processing beyond what
 * get_ipc_connect_master_ip_bcast() does, e.g. to retrieve the list of master
 * browsers and obtain each master browsers' list of domains (in case the
 * first master browser is recently on the network and has not yet
 * synchronized with other master browsers and therefore does not yet have the
 * entire network browse list)
 */

struct cli_state *get_ipc_connect_master_ip(TALLOC_CTX *ctx,
				struct ip_service *mb_ip,
				const struct user_auth_info *user_info,
				char **pp_workgroup_out)
{
	char addr[INET6_ADDRSTRLEN];
        fstring name;
	struct cli_state *cli;
	struct sockaddr_storage server_ss;

	*pp_workgroup_out = NULL;

	print_sockaddr(addr, sizeof(addr), &mb_ip->ss);
        DEBUG(99, ("Looking up name of master browser %s\n",
                   addr));

        /*
         * Do a name status query to find out the name of the master browser.
         * We use <01><02>__MSBROWSE__<02>#01 if *#00 fails because a domain
         * master browser will not respond to a wildcard query (or, at least,
         * an NT4 server acting as the domain master browser will not).
         *
         * We might be able to use ONLY the query on MSBROWSE, but that's not
         * yet been tested with all Windows versions, so until it is, leave
         * the original wildcard query as the first choice and fall back to
         * MSBROWSE if the wildcard query fails.
         */
        if (!name_status_find("*", 0, 0x1d, &mb_ip->ss, name) &&
            !name_status_find(MSBROWSE, 1, 0x1d, &mb_ip->ss, name)) {

                DEBUG(99, ("Could not retrieve name status for %s\n",
                           addr));
                return NULL;
        }

        if (!find_master_ip(name, &server_ss)) {
                DEBUG(99, ("Could not find master ip for %s\n", name));
                return NULL;
        }

	*pp_workgroup_out = talloc_strdup(ctx, name);

	DEBUG(4, ("found master browser %s, %s\n", name, addr));

	print_sockaddr(addr, sizeof(addr), &server_ss);
	cli = get_ipc_connect(addr, &server_ss, user_info);

	return cli;
}

/*
 * Return the IP address and workgroup of a master browser on the network, and
 * connect to it.
 */

struct cli_state *get_ipc_connect_master_ip_bcast(TALLOC_CTX *ctx,
					const struct user_auth_info *user_info,
					char **pp_workgroup_out)
{
	struct ip_service *ip_list;
	struct cli_state *cli;
	int i, count;

	*pp_workgroup_out = NULL;

        DEBUG(99, ("Do broadcast lookup for workgroups on local network\n"));

        /* Go looking for workgroups by broadcasting on the local network */

        if (!NT_STATUS_IS_OK(name_resolve_bcast(MSBROWSE, 1, &ip_list,
						&count))) {
                DEBUG(99, ("No master browsers responded\n"));
                return False;
        }

	for (i = 0; i < count; i++) {
		char addr[INET6_ADDRSTRLEN];
		print_sockaddr(addr, sizeof(addr), &ip_list[i].ss);
		DEBUG(99, ("Found master browser %s\n", addr));

		cli = get_ipc_connect_master_ip(ctx, &ip_list[i],
				user_info, pp_workgroup_out);
		if (cli)
			return(cli);
	}

	return NULL;
}
