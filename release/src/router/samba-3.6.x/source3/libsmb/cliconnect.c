/* 
   Unix SMB/CIFS implementation.
   client connect/disconnect routines
   Copyright (C) Andrew Tridgell 1994-1998
   Copyright (C) Andrew Bartlett 2001-2003
   Copyright (C) Volker Lendecke 2011
   Copyright (C) Jeremy Allison 2011

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
#include "libsmb/libsmb.h"
#include "popt_common.h"
#include "../libcli/auth/libcli_auth.h"
#include "../libcli/auth/spnego.h"
#include "smb_krb5.h"
#include "../libcli/auth/ntlmssp.h"
#include "libads/kerberos_proto.h"
#include "krb5_env.h"
#include "../lib/util/tevent_ntstatus.h"
#include "async_smb.h"
#include "libsmb/nmblib.h"

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

/********************************************************
 Utility function to ensure we always return at least
 a valid char * pointer to an empty string for the
 cli->server_os, cli->server_type and cli->server_domain
 strings.
*******************************************************/

static NTSTATUS smb_bytes_talloc_string(struct cli_state *cli,
					char *inbuf,
					char **dest,
					uint8_t *src,
					size_t srclen,
					ssize_t *destlen)
{
	*destlen = clistr_pull_talloc(cli,
				inbuf,
				SVAL(inbuf, smb_flg2),
				dest,
				(char *)src,
				srclen,
				STR_TERMINATE);
	if (*destlen == -1) {
		return NT_STATUS_NO_MEMORY;
	}

	if (*dest == NULL) {
		*dest = talloc_strdup(cli, "");
		if (*dest == NULL) {
			return NT_STATUS_NO_MEMORY;
		}
	}
	return NT_STATUS_OK;
}

/**
 * Set the user session key for a connection
 * @param cli The cli structure to add it too
 * @param session_key The session key used.  (A copy of this is taken for the cli struct)
 *
 */

static void cli_set_session_key (struct cli_state *cli, const DATA_BLOB session_key) 
{
	cli->user_session_key = data_blob(NULL, 16);
	data_blob_clear(&cli->user_session_key);
	memcpy(cli->user_session_key.data, session_key.data, MIN(session_key.length, 16));
}

/****************************************************************************
 Do an old lanman2 style session setup.
****************************************************************************/

struct cli_session_setup_lanman2_state {
	struct cli_state *cli;
	uint16_t vwv[10];
	const char *user;
};

static void cli_session_setup_lanman2_done(struct tevent_req *subreq);

static struct tevent_req *cli_session_setup_lanman2_send(
	TALLOC_CTX *mem_ctx, struct tevent_context *ev,
	struct cli_state *cli, const char *user,
	const char *pass, size_t passlen,
	const char *workgroup)
{
	struct tevent_req *req, *subreq;
	struct cli_session_setup_lanman2_state *state;
	DATA_BLOB lm_response = data_blob_null;
	uint16_t *vwv;
	uint8_t *bytes;
	char *tmp;

	req = tevent_req_create(mem_ctx, &state,
				struct cli_session_setup_lanman2_state);
	if (req == NULL) {
		return NULL;
	}
	state->cli = cli;
	state->user = user;
	vwv = state->vwv;

	/*
	 * LANMAN servers predate NT status codes and Unicode and
	 * ignore those smb flags so we must disable the corresponding
	 * default capabilities that would otherwise cause the Unicode
	 * and NT Status flags to be set (and even returned by the
	 * server)
	 */

	cli->capabilities &= ~(CAP_UNICODE | CAP_STATUS32);

	/*
	 * if in share level security then don't send a password now
	 */
	if (!(cli->sec_mode & NEGOTIATE_SECURITY_USER_LEVEL)) {
		passlen = 0;
	}

	if (passlen > 0
	    && (cli->sec_mode & NEGOTIATE_SECURITY_CHALLENGE_RESPONSE)
	    && passlen != 24) {
		/*
		 * Encrypted mode needed, and non encrypted password
		 * supplied.
		 */
		lm_response = data_blob(NULL, 24);
		if (tevent_req_nomem(lm_response.data, req)) {
			return tevent_req_post(req, ev);
		}

		if (!SMBencrypt(pass, cli->secblob.data,
				(uint8_t *)lm_response.data)) {
			DEBUG(1, ("Password is > 14 chars in length, and is "
				  "therefore incompatible with Lanman "
				  "authentication\n"));
			tevent_req_nterror(req, NT_STATUS_ACCESS_DENIED);
			return tevent_req_post(req, ev);
		}
	} else if ((cli->sec_mode & NEGOTIATE_SECURITY_CHALLENGE_RESPONSE)
		   && passlen == 24) {
		/*
		 * Encrypted mode needed, and encrypted password
		 * supplied.
		 */
		lm_response = data_blob(pass, passlen);
		if (tevent_req_nomem(lm_response.data, req)) {
			return tevent_req_post(req, ev);
		}
	} else if (passlen > 0) {
		uint8_t *buf;
		size_t converted_size;
		/*
		 * Plaintext mode needed, assume plaintext supplied.
		 */
		buf = talloc_array(talloc_tos(), uint8_t, 0);
		buf = smb_bytes_push_str(buf, cli_ucs2(cli), pass, passlen+1,
					 &converted_size);
		if (tevent_req_nomem(buf, req)) {
			return tevent_req_post(req, ev);
		}
		lm_response = data_blob(pass, passlen);
		TALLOC_FREE(buf);
		if (tevent_req_nomem(lm_response.data, req)) {
			return tevent_req_post(req, ev);
		}
	}

	SCVAL(vwv+0, 0, 0xff);
	SCVAL(vwv+0, 1, 0);
	SSVAL(vwv+1, 0, 0);
	SSVAL(vwv+2, 0, CLI_BUFFER_SIZE);
	SSVAL(vwv+3, 0, 2);
	SSVAL(vwv+4, 0, 1);
	SIVAL(vwv+5, 0, cli->sesskey);
	SSVAL(vwv+7, 0, lm_response.length);

	bytes = talloc_array(state, uint8_t, lm_response.length);
	if (tevent_req_nomem(bytes, req)) {
		return tevent_req_post(req, ev);
	}
	if (lm_response.length != 0) {
		memcpy(bytes, lm_response.data, lm_response.length);
	}
	data_blob_free(&lm_response);

	tmp = talloc_strdup_upper(talloc_tos(), user);
	if (tevent_req_nomem(tmp, req)) {
		return tevent_req_post(req, ev);
	}
	bytes = smb_bytes_push_str(bytes, cli_ucs2(cli), tmp, strlen(tmp)+1,
				   NULL);
	TALLOC_FREE(tmp);

	tmp = talloc_strdup_upper(talloc_tos(), workgroup);
	if (tevent_req_nomem(tmp, req)) {
		return tevent_req_post(req, ev);
	}
	bytes = smb_bytes_push_str(bytes, cli_ucs2(cli), tmp, strlen(tmp)+1,
				   NULL);
	bytes = smb_bytes_push_str(bytes, cli_ucs2(cli), "Unix", 5, NULL);
	bytes = smb_bytes_push_str(bytes, cli_ucs2(cli), "Samba", 6, NULL);

	if (tevent_req_nomem(bytes, req)) {
		return tevent_req_post(req, ev);
	}

	subreq = cli_smb_send(state, ev, cli, SMBsesssetupX, 0, 10, vwv,
			      talloc_get_size(bytes), bytes);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_session_setup_lanman2_done, req);
	return req;
}

static void cli_session_setup_lanman2_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct cli_session_setup_lanman2_state *state = tevent_req_data(
		req, struct cli_session_setup_lanman2_state);
	struct cli_state *cli = state->cli;
	uint32_t num_bytes;
	uint8_t *in;
	char *inbuf;
	uint8_t *bytes;
	uint8_t *p;
	NTSTATUS status;
	ssize_t ret;
	uint8_t wct;
	uint16_t *vwv;

	status = cli_smb_recv(subreq, state, &in, 3, &wct, &vwv,
			      &num_bytes, &bytes);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}

	inbuf = (char *)in;
	p = bytes;

	cli->vuid = SVAL(inbuf, smb_uid);
	cli->is_guestlogin = ((SVAL(vwv+2, 0) & 1) != 0);

	status = smb_bytes_talloc_string(cli,
					inbuf,
					&cli->server_os,
					p,
					bytes+num_bytes-p,
					&ret);

	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}
	p += ret;

	status = smb_bytes_talloc_string(cli,
					inbuf,
					&cli->server_type,
					p,
					bytes+num_bytes-p,
					&ret);

	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}
	p += ret;

	status = smb_bytes_talloc_string(cli,
					inbuf,
					&cli->server_domain,
					p,
					bytes+num_bytes-p,
					&ret);

	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}
	p += ret;

	if (strstr(cli->server_type, "Samba")) {
		cli->is_samba = True;
	}
	status = cli_set_username(cli, state->user);
	if (tevent_req_nterror(req, status)) {
		return;
	}
	tevent_req_done(req);
}

static NTSTATUS cli_session_setup_lanman2_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}

static NTSTATUS cli_session_setup_lanman2(struct cli_state *cli, const char *user,
					  const char *pass, size_t passlen,
					  const char *workgroup)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev;
	struct tevent_req *req;
	NTSTATUS status = NT_STATUS_NO_MEMORY;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}
	ev = event_context_init(frame);
	if (ev == NULL) {
		goto fail;
	}
	req = cli_session_setup_lanman2_send(frame, ev, cli, user, pass, passlen,
					     workgroup);
	if (req == NULL) {
		goto fail;
	}
	if (!tevent_req_poll_ntstatus(req, ev, &status)) {
		goto fail;
	}
	status = cli_session_setup_lanman2_recv(req);
 fail:
	TALLOC_FREE(frame);
	return status;
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
	uint16_t vwv[13];
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
	uint8_t *in;
	char *inbuf;
	uint8_t *bytes;
	uint8_t *p;
	NTSTATUS status;
	ssize_t ret;
	uint8_t wct;
	uint16_t *vwv;

	status = cli_smb_recv(subreq, state, &in, 3, &wct, &vwv,
			      &num_bytes, &bytes);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}

	inbuf = (char *)in;
	p = bytes;

	cli->vuid = SVAL(inbuf, smb_uid);
	cli->is_guestlogin = ((SVAL(vwv+2, 0) & 1) != 0);

	status = smb_bytes_talloc_string(cli,
					inbuf,
					&cli->server_os,
					p,
					bytes+num_bytes-p,
					&ret);

	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}
	p += ret;

	status = smb_bytes_talloc_string(cli,
					inbuf,
					&cli->server_type,
					p,
					bytes+num_bytes-p,
					&ret);

	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}
	p += ret;

	status = smb_bytes_talloc_string(cli,
					inbuf,
					&cli->server_domain,
					p,
					bytes+num_bytes-p,
					&ret);

	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}
	p += ret;

	if (strstr(cli->server_type, "Samba")) {
		cli->is_samba = True;
	}

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
	return status;
}

/****************************************************************************
 Do a NT1 plaintext session setup.
****************************************************************************/

struct cli_session_setup_plain_state {
	struct cli_state *cli;
	uint16_t vwv[13];
	const char *user;
};

static void cli_session_setup_plain_done(struct tevent_req *subreq);

static struct tevent_req *cli_session_setup_plain_send(
	TALLOC_CTX *mem_ctx, struct tevent_context *ev,
	struct cli_state *cli,
	const char *user, const char *pass, const char *workgroup)
{
	struct tevent_req *req, *subreq;
	struct cli_session_setup_plain_state *state;
	uint16_t *vwv;
	uint8_t *bytes;
	size_t passlen;
	char *version;

	req = tevent_req_create(mem_ctx, &state,
				struct cli_session_setup_plain_state);
	if (req == NULL) {
		return NULL;
	}
	state->cli = cli;
	state->user = user;
	vwv = state->vwv;

	SCVAL(vwv+0, 0, 0xff);
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
	bytes = smb_bytes_push_str(bytes, cli_ucs2(cli), pass, strlen(pass)+1,
				   &passlen);
	if (tevent_req_nomem(bytes, req)) {
		return tevent_req_post(req, ev);
	}
	SSVAL(vwv + (cli_ucs2(cli) ? 8 : 7), 0, passlen);

	bytes = smb_bytes_push_str(bytes, cli_ucs2(cli),
				   user, strlen(user)+1, NULL);
	bytes = smb_bytes_push_str(bytes, cli_ucs2(cli),
				   workgroup, strlen(workgroup)+1, NULL);
	bytes = smb_bytes_push_str(bytes, cli_ucs2(cli),
				   "Unix", 5, NULL);

	version = talloc_asprintf(talloc_tos(), "Samba %s",
				  samba_version_string());
	if (tevent_req_nomem(version, req)){
		return tevent_req_post(req, ev);
	}
	bytes = smb_bytes_push_str(bytes, cli_ucs2(cli),
				   version, strlen(version)+1, NULL);
	TALLOC_FREE(version);

	if (tevent_req_nomem(bytes, req)) {
		return tevent_req_post(req, ev);
	}

	subreq = cli_smb_send(state, ev, cli, SMBsesssetupX, 0, 13, vwv,
			      talloc_get_size(bytes), bytes);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_session_setup_plain_done, req);
	return req;
}

static void cli_session_setup_plain_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct cli_session_setup_plain_state *state = tevent_req_data(
		req, struct cli_session_setup_plain_state);
	struct cli_state *cli = state->cli;
	uint32_t num_bytes;
	uint8_t *in;
	char *inbuf;
	uint8_t *bytes;
	uint8_t *p;
	NTSTATUS status;
	ssize_t ret;
	uint8_t wct;
	uint16_t *vwv;

	status = cli_smb_recv(subreq, state, &in, 3, &wct, &vwv,
			      &num_bytes, &bytes);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}

	inbuf = (char *)in;
	p = bytes;

	cli->vuid = SVAL(inbuf, smb_uid);
	cli->is_guestlogin = ((SVAL(vwv+2, 0) & 1) != 0);

	status = smb_bytes_talloc_string(cli,
					inbuf,
					&cli->server_os,
					p,
					bytes+num_bytes-p,
					&ret);

	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}
	p += ret;

	status = smb_bytes_talloc_string(cli,
					inbuf,
					&cli->server_type,
					p,
					bytes+num_bytes-p,
					&ret);

	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}
	p += ret;

	status = smb_bytes_talloc_string(cli,
					inbuf,
					&cli->server_domain,
					p,
					bytes+num_bytes-p,
					&ret);

	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}
	p += ret;

	status = cli_set_username(cli, state->user);
	if (tevent_req_nterror(req, status)) {
		return;
	}
	if (strstr(cli->server_type, "Samba")) {
		cli->is_samba = True;
	}
	tevent_req_done(req);
}

static NTSTATUS cli_session_setup_plain_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}

static NTSTATUS cli_session_setup_plain(struct cli_state *cli,
					const char *user, const char *pass,
					const char *workgroup)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev;
	struct tevent_req *req;
	NTSTATUS status = NT_STATUS_NO_MEMORY;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}
	ev = event_context_init(frame);
	if (ev == NULL) {
		goto fail;
	}
	req = cli_session_setup_plain_send(frame, ev, cli, user, pass,
					   workgroup);
	if (req == NULL) {
		goto fail;
	}
	if (!tevent_req_poll_ntstatus(req, ev, &status)) {
		goto fail;
	}
	status = cli_session_setup_plain_recv(req);
 fail:
	TALLOC_FREE(frame);
	return status;
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

struct cli_session_setup_nt1_state {
	struct cli_state *cli;
	uint16_t vwv[13];
	DATA_BLOB response;
	DATA_BLOB session_key;
	const char *user;
};

static void cli_session_setup_nt1_done(struct tevent_req *subreq);

static struct tevent_req *cli_session_setup_nt1_send(
	TALLOC_CTX *mem_ctx, struct tevent_context *ev,
	struct cli_state *cli, const char *user,
	const char *pass, size_t passlen,
	const char *ntpass, size_t ntpasslen,
	const char *workgroup)
{
	struct tevent_req *req, *subreq;
	struct cli_session_setup_nt1_state *state;
	DATA_BLOB lm_response = data_blob_null;
	DATA_BLOB nt_response = data_blob_null;
	DATA_BLOB session_key = data_blob_null;
	uint16_t *vwv;
	uint8_t *bytes;
	char *workgroup_upper;

	req = tevent_req_create(mem_ctx, &state,
				struct cli_session_setup_nt1_state);
	if (req == NULL) {
		return NULL;
	}
	state->cli = cli;
	state->user = user;
	vwv = state->vwv;

	if (passlen == 0) {
		/* do nothing - guest login */
	} else if (passlen != 24) {
		if (lp_client_ntlmv2_auth()) {
			DATA_BLOB server_chal;
			DATA_BLOB names_blob;

			server_chal = data_blob(cli->secblob.data,
						MIN(cli->secblob.length, 8));
			if (tevent_req_nomem(server_chal.data, req)) {
				return tevent_req_post(req, ev);
			}

			/*
			 * note that the 'workgroup' here is a best
			 * guess - we don't know the server's domain
			 * at this point.  The 'server name' is also
			 * dodgy...
			 */
			names_blob = NTLMv2_generate_names_blob(
				NULL, cli->called.name, workgroup);

			if (tevent_req_nomem(names_blob.data, req)) {
				return tevent_req_post(req, ev);
			}

			if (!SMBNTLMv2encrypt(NULL, user, workgroup, pass,
					      &server_chal, &names_blob,
					      &lm_response, &nt_response,
					      NULL, &session_key)) {
				data_blob_free(&names_blob);
				data_blob_free(&server_chal);
				tevent_req_nterror(
					req, NT_STATUS_ACCESS_DENIED);
				return tevent_req_post(req, ev);
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
			if (tevent_req_nomem(nt_response.data, req)) {
				return tevent_req_post(req, ev);
			}

			SMBNTencrypt(pass, cli->secblob.data,
				     nt_response.data);
#endif
			/* non encrypted password supplied. Ignore ntpass. */
			if (lp_client_lanman_auth()) {

				lm_response = data_blob(NULL, 24);
				if (tevent_req_nomem(lm_response.data, req)) {
					return tevent_req_post(req, ev);
				}

				if (!SMBencrypt(pass,cli->secblob.data,
						lm_response.data)) {
					/*
					 * Oops, the LM response is
					 * invalid, just put the NT
					 * response there instead
					 */
					data_blob_free(&lm_response);
					lm_response = data_blob(
						nt_response.data,
						nt_response.length);
				}
			} else {
				/*
				 * LM disabled, place NT# in LM field
				 * instead
				 */
				lm_response = data_blob(
					nt_response.data, nt_response.length);
			}

			if (tevent_req_nomem(lm_response.data, req)) {
				return tevent_req_post(req, ev);
			}

			session_key = data_blob(NULL, 16);
			if (tevent_req_nomem(session_key.data, req)) {
				return tevent_req_post(req, ev);
			}
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
		if (tevent_req_nomem(lm_response.data, req)) {
			return tevent_req_post(req, ev);
		}

		nt_response = data_blob(ntpass, ntpasslen);
		if (tevent_req_nomem(nt_response.data, req)) {
			return tevent_req_post(req, ev);
		}
	}

#ifdef LANMAN_ONLY
	state->response = data_blob_talloc(
		state, lm_response.data, lm_response.length);
#else
	state->response = data_blob_talloc(
		state, nt_response.data, nt_response.length);
#endif
	if (tevent_req_nomem(state->response.data, req)) {
		return tevent_req_post(req, ev);
	}

	if (session_key.data) {
		state->session_key = data_blob_talloc(
			state, session_key.data, session_key.length);
		if (tevent_req_nomem(state->session_key.data, req)) {
			return tevent_req_post(req, ev);
		}
	}
	data_blob_free(&session_key);

	SCVAL(vwv+0, 0, 0xff);
	SCVAL(vwv+0, 1, 0);
	SSVAL(vwv+1, 0, 0);
	SSVAL(vwv+2, 0, CLI_BUFFER_SIZE);
	SSVAL(vwv+3, 0, 2);
	SSVAL(vwv+4, 0, cli->pid);
	SIVAL(vwv+5, 0, cli->sesskey);
	SSVAL(vwv+7, 0, lm_response.length);
	SSVAL(vwv+8, 0, nt_response.length);
	SSVAL(vwv+9, 0, 0);
	SSVAL(vwv+10, 0, 0);
	SIVAL(vwv+11, 0, cli_session_setup_capabilities(cli));

	bytes = talloc_array(state, uint8_t,
			     lm_response.length + nt_response.length);
	if (tevent_req_nomem(bytes, req)) {
		return tevent_req_post(req, ev);
	}
	if (lm_response.length != 0) {
		memcpy(bytes, lm_response.data, lm_response.length);
	}
	if (nt_response.length != 0) {
		memcpy(bytes + lm_response.length,
		       nt_response.data, nt_response.length);
	}
	data_blob_free(&lm_response);
	data_blob_free(&nt_response);

	bytes = smb_bytes_push_str(bytes, cli_ucs2(cli),
				   user, strlen(user)+1, NULL);

	/*
	 * Upper case here might help some NTLMv2 implementations
	 */
	workgroup_upper = talloc_strdup_upper(talloc_tos(), workgroup);
	if (tevent_req_nomem(workgroup_upper, req)) {
		return tevent_req_post(req, ev);
	}
	bytes = smb_bytes_push_str(bytes, cli_ucs2(cli),
				   workgroup_upper, strlen(workgroup_upper)+1,
				   NULL);
	TALLOC_FREE(workgroup_upper);

	bytes = smb_bytes_push_str(bytes, cli_ucs2(cli), "Unix", 5, NULL);
	bytes = smb_bytes_push_str(bytes, cli_ucs2(cli), "Samba", 6, NULL);
	if (tevent_req_nomem(bytes, req)) {
		return tevent_req_post(req, ev);
	}

	subreq = cli_smb_send(state, ev, cli, SMBsesssetupX, 0, 13, vwv,
			      talloc_get_size(bytes), bytes);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_session_setup_nt1_done, req);
	return req;
}

static void cli_session_setup_nt1_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct cli_session_setup_nt1_state *state = tevent_req_data(
		req, struct cli_session_setup_nt1_state);
	struct cli_state *cli = state->cli;
	uint32_t num_bytes;
	uint8_t *in;
	char *inbuf;
	uint8_t *bytes;
	uint8_t *p;
	NTSTATUS status;
	ssize_t ret;
	uint8_t wct;
	uint16_t *vwv;

	status = cli_smb_recv(subreq, state, &in, 3, &wct, &vwv,
			      &num_bytes, &bytes);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}

	inbuf = (char *)in;
	p = bytes;

	cli->vuid = SVAL(inbuf, smb_uid);
	cli->is_guestlogin = ((SVAL(vwv+2, 0) & 1) != 0);

	status = smb_bytes_talloc_string(cli,
					inbuf,
					&cli->server_os,
					p,
					bytes+num_bytes-p,
					&ret);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}
	p += ret;

	status = smb_bytes_talloc_string(cli,
					inbuf,
					&cli->server_type,
					p,
					bytes+num_bytes-p,
					&ret);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}
	p += ret;

	status = smb_bytes_talloc_string(cli,
					inbuf,
					&cli->server_domain,
					p,
					bytes+num_bytes-p,
					&ret);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}
	p += ret;

	if (strstr(cli->server_type, "Samba")) {
		cli->is_samba = True;
	}

	status = cli_set_username(cli, state->user);
	if (tevent_req_nterror(req, status)) {
		return;
	}
	if (cli_simple_set_signing(cli, state->session_key, state->response)
	    && !cli_check_sign_mac(cli, (char *)in, 1)) {
		tevent_req_nterror(req, NT_STATUS_ACCESS_DENIED);
		return;
	}
	if (state->session_key.data) {
		/* Have plaintext orginal */
		cli_set_session_key(cli, state->session_key);
	}
	tevent_req_done(req);
}

static NTSTATUS cli_session_setup_nt1_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}

static NTSTATUS cli_session_setup_nt1(struct cli_state *cli, const char *user,
				      const char *pass, size_t passlen,
				      const char *ntpass, size_t ntpasslen,
				      const char *workgroup)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev;
	struct tevent_req *req;
	NTSTATUS status = NT_STATUS_NO_MEMORY;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}
	ev = event_context_init(frame);
	if (ev == NULL) {
		goto fail;
	}
	req = cli_session_setup_nt1_send(frame, ev, cli, user, pass, passlen,
					 ntpass, ntpasslen, workgroup);
	if (req == NULL) {
		goto fail;
	}
	if (!tevent_req_poll_ntstatus(req, ev, &status)) {
		goto fail;
	}
	status = cli_session_setup_nt1_recv(req);
 fail:
	TALLOC_FREE(frame);
	return status;
}

/* The following is calculated from :
 * (smb_size-4) = 35
 * (smb_wcnt * 2) = 24 (smb_wcnt == 12 in cli_session_setup_blob_send() )
 * (strlen("Unix") + 1 + strlen("Samba") + 1) * 2 = 22 (unicode strings at
 * end of packet.
 */

#define BASE_SESSSETUP_BLOB_PACKET_SIZE (35 + 24 + 22)

struct cli_sesssetup_blob_state {
	struct tevent_context *ev;
	struct cli_state *cli;
	DATA_BLOB blob;
	uint16_t max_blob_size;
	uint16_t vwv[12];
	uint8_t *buf;

	NTSTATUS status;
	char *inbuf;
	DATA_BLOB ret_blob;
};

static bool cli_sesssetup_blob_next(struct cli_sesssetup_blob_state *state,
				    struct tevent_req **psubreq);
static void cli_sesssetup_blob_done(struct tevent_req *subreq);

static struct tevent_req *cli_sesssetup_blob_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct cli_state *cli,
						  DATA_BLOB blob)
{
	struct tevent_req *req, *subreq;
	struct cli_sesssetup_blob_state *state;

	req = tevent_req_create(mem_ctx, &state,
				struct cli_sesssetup_blob_state);
	if (req == NULL) {
		return NULL;
	}
	state->ev = ev;
	state->blob = blob;
	state->cli = cli;

	if (cli->max_xmit < BASE_SESSSETUP_BLOB_PACKET_SIZE + 1) {
		DEBUG(1, ("cli_session_setup_blob: cli->max_xmit too small "
			  "(was %u, need minimum %u)\n",
			  (unsigned int)cli->max_xmit,
			  BASE_SESSSETUP_BLOB_PACKET_SIZE));
		tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return tevent_req_post(req, ev);
	}
	state->max_blob_size =
		MIN(cli->max_xmit - BASE_SESSSETUP_BLOB_PACKET_SIZE, 0xFFFF);

	if (!cli_sesssetup_blob_next(state, &subreq)) {
		tevent_req_nterror(req, NT_STATUS_NO_MEMORY);
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_sesssetup_blob_done, req);
	return req;
}

static bool cli_sesssetup_blob_next(struct cli_sesssetup_blob_state *state,
				    struct tevent_req **psubreq)
{
	struct tevent_req *subreq;
	uint16_t thistime;

	SCVAL(state->vwv+0, 0, 0xFF);
	SCVAL(state->vwv+0, 1, 0);
	SSVAL(state->vwv+1, 0, 0);
	SSVAL(state->vwv+2, 0, CLI_BUFFER_SIZE);
	SSVAL(state->vwv+3, 0, 2);
	SSVAL(state->vwv+4, 0, 1);
	SIVAL(state->vwv+5, 0, 0);

	thistime = MIN(state->blob.length, state->max_blob_size);
	SSVAL(state->vwv+7, 0, thistime);

	SSVAL(state->vwv+8, 0, 0);
	SSVAL(state->vwv+9, 0, 0);
	SIVAL(state->vwv+10, 0,
	      cli_session_setup_capabilities(state->cli)
	      | CAP_EXTENDED_SECURITY);

	state->buf = (uint8_t *)talloc_memdup(state, state->blob.data,
					      thistime);
	if (state->buf == NULL) {
		return false;
	}
	state->blob.data += thistime;
	state->blob.length -= thistime;

	state->buf = smb_bytes_push_str(state->buf, cli_ucs2(state->cli),
					"Unix", 5, NULL);
	state->buf = smb_bytes_push_str(state->buf, cli_ucs2(state->cli),
					"Samba", 6, NULL);
	if (state->buf == NULL) {
		return false;
	}
	subreq = cli_smb_send(state, state->ev, state->cli, SMBsesssetupX, 0,
			      12, state->vwv,
			      talloc_get_size(state->buf), state->buf);
	if (subreq == NULL) {
		return false;
	}
	*psubreq = subreq;
	return true;
}

static void cli_sesssetup_blob_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct cli_sesssetup_blob_state *state = tevent_req_data(
		req, struct cli_sesssetup_blob_state);
	struct cli_state *cli = state->cli;
	uint8_t wct;
	uint16_t *vwv;
	uint32_t num_bytes;
	uint8_t *bytes;
	NTSTATUS status;
	uint8_t *p;
	uint16_t blob_length;
	uint8_t *inbuf;
	ssize_t ret;

	status = cli_smb_recv(subreq, state, &inbuf, 4, &wct, &vwv,
			      &num_bytes, &bytes);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)
	    && !NT_STATUS_EQUAL(status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {
		tevent_req_nterror(req, status);
		return;
	}

	state->status = status;
	TALLOC_FREE(state->buf);

	state->inbuf = (char *)inbuf;
	cli->vuid = SVAL(state->inbuf, smb_uid);
	cli->is_guestlogin = ((SVAL(vwv+2, 0) & 1) != 0);

	blob_length = SVAL(vwv+3, 0);
	if (blob_length > num_bytes) {
		tevent_req_nterror(req, NT_STATUS_INVALID_NETWORK_RESPONSE);
		return;
	}
	state->ret_blob = data_blob_const(bytes, blob_length);

	p = bytes + blob_length;

	status = smb_bytes_talloc_string(cli,
					(char *)inbuf,
					&cli->server_os,
					p,
					bytes+num_bytes-p,
					&ret);

	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}
	p += ret;

	status = smb_bytes_talloc_string(cli,
					(char *)inbuf,
					&cli->server_type,
					p,
					bytes+num_bytes-p,
					&ret);

	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}
	p += ret;

	status = smb_bytes_talloc_string(cli,
					(char *)inbuf,
					&cli->server_domain,
					p,
					bytes+num_bytes-p,
					&ret);

	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}
	p += ret;

	if (strstr(cli->server_type, "Samba")) {
		cli->is_samba = True;
	}

	if (state->blob.length != 0) {
		/*
		 * More to send
		 */
		if (!cli_sesssetup_blob_next(state, &subreq)) {
			tevent_req_nomem(NULL, req);
			return;
		}
		tevent_req_set_callback(subreq, cli_sesssetup_blob_done, req);
		return;
	}
	tevent_req_done(req);
}

static NTSTATUS cli_sesssetup_blob_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					DATA_BLOB *pblob,
					char **pinbuf)
{
	struct cli_sesssetup_blob_state *state = tevent_req_data(
		req, struct cli_sesssetup_blob_state);
	NTSTATUS status;
	char *inbuf;

	if (tevent_req_is_nterror(req, &status)) {
		state->cli->vuid = 0;
		return status;
	}

	inbuf = talloc_move(mem_ctx, &state->inbuf);
	if (pblob != NULL) {
		*pblob = state->ret_blob;
	}
	if (pinbuf != NULL) {
		*pinbuf = inbuf;
	}
        /* could be NT_STATUS_MORE_PROCESSING_REQUIRED */
	return state->status;
}

#ifdef HAVE_KRB5

/****************************************************************************
 Use in-memory credentials cache
****************************************************************************/

static void use_in_memory_ccache(void) {
	setenv(KRB5_ENV_CCNAME, "MEMORY:cliconnect", 1);
}

/****************************************************************************
 Do a spnego/kerberos encrypted session setup.
****************************************************************************/

struct cli_session_setup_kerberos_state {
	struct cli_state *cli;
	DATA_BLOB negTokenTarg;
	DATA_BLOB session_key_krb5;
	ADS_STATUS ads_status;
};

static void cli_session_setup_kerberos_done(struct tevent_req *subreq);

static struct tevent_req *cli_session_setup_kerberos_send(
	TALLOC_CTX *mem_ctx, struct tevent_context *ev, struct cli_state *cli,
	const char *principal, const char *workgroup)
{
	struct tevent_req *req, *subreq;
	struct cli_session_setup_kerberos_state *state;
	int rc;

	DEBUG(2,("Doing kerberos session setup\n"));

	req = tevent_req_create(mem_ctx, &state,
				struct cli_session_setup_kerberos_state);
	if (req == NULL) {
		return NULL;
	}
	state->cli = cli;
	state->ads_status = ADS_SUCCESS;

	cli_temp_set_signing(cli);

	/*
	 * Ok, this is cheating: spnego_gen_krb5_negTokenInit can block if
	 * we have to acquire a ticket. To be fixed later :-)
	 */
	rc = spnego_gen_krb5_negTokenInit(state, principal, 0, &state->negTokenTarg,
				     &state->session_key_krb5, 0, NULL);
	if (rc) {
		DEBUG(1, ("cli_session_setup_kerberos: "
			  "spnego_gen_krb5_negTokenInit failed: %s\n",
			  error_message(rc)));
		state->ads_status = ADS_ERROR_KRB5(rc);
		tevent_req_nterror(req, NT_STATUS_UNSUCCESSFUL);
		return tevent_req_post(req, ev);
	}

#if 0
	file_save("negTokenTarg.dat", state->negTokenTarg.data,
		  state->negTokenTarg.length);
#endif

	subreq = cli_sesssetup_blob_send(state, ev, cli, state->negTokenTarg);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_session_setup_kerberos_done, req);
	return req;
}

static void cli_session_setup_kerberos_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct cli_session_setup_kerberos_state *state = tevent_req_data(
		req, struct cli_session_setup_kerberos_state);
	char *inbuf = NULL;
	NTSTATUS status;

	status = cli_sesssetup_blob_recv(subreq, talloc_tos(), NULL, &inbuf);
	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(subreq);
		tevent_req_nterror(req, status);
		return;
	}

	cli_set_session_key(state->cli, state->session_key_krb5);

	if (cli_simple_set_signing(state->cli, state->session_key_krb5,
				   data_blob_null)
	    && !cli_check_sign_mac(state->cli, inbuf, 1)) {
		TALLOC_FREE(subreq);
		tevent_req_nterror(req, NT_STATUS_ACCESS_DENIED);
		return;
	}
	TALLOC_FREE(subreq);
	tevent_req_done(req);
}

static ADS_STATUS cli_session_setup_kerberos_recv(struct tevent_req *req)
{
	struct cli_session_setup_kerberos_state *state = tevent_req_data(
		req, struct cli_session_setup_kerberos_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return ADS_ERROR_NT(status);
	}
	return state->ads_status;
}

static ADS_STATUS cli_session_setup_kerberos(struct cli_state *cli,
					     const char *principal,
					     const char *workgroup)
{
	struct tevent_context *ev;
	struct tevent_req *req;
	ADS_STATUS status = ADS_ERROR_NT(NT_STATUS_NO_MEMORY);

	if (cli_has_async_calls(cli)) {
		return ADS_ERROR_NT(NT_STATUS_INVALID_PARAMETER);
	}
	ev = tevent_context_init(talloc_tos());
	if (ev == NULL) {
		goto fail;
	}
	req = cli_session_setup_kerberos_send(ev, ev, cli, principal,
					      workgroup);
	if (req == NULL) {
		goto fail;
	}
	if (!tevent_req_poll(req, ev)) {
		status = ADS_ERROR_SYSTEM(errno);
		goto fail;
	}
	status = cli_session_setup_kerberos_recv(req);
fail:
	TALLOC_FREE(ev);
	return status;
}
#endif	/* HAVE_KRB5 */

/****************************************************************************
 Do a spnego/NTLMSSP encrypted session setup.
****************************************************************************/

struct cli_session_setup_ntlmssp_state {
	struct tevent_context *ev;
	struct cli_state *cli;
	struct ntlmssp_state *ntlmssp_state;
	int turn;
	DATA_BLOB blob_out;
};

static int cli_session_setup_ntlmssp_state_destructor(
	struct cli_session_setup_ntlmssp_state *state)
{
	if (state->ntlmssp_state != NULL) {
		TALLOC_FREE(state->ntlmssp_state);
	}
	return 0;
}

static void cli_session_setup_ntlmssp_done(struct tevent_req *req);

static struct tevent_req *cli_session_setup_ntlmssp_send(
	TALLOC_CTX *mem_ctx, struct tevent_context *ev, struct cli_state *cli,
	const char *user, const char *pass, const char *domain)
{
	struct tevent_req *req, *subreq;
	struct cli_session_setup_ntlmssp_state *state;
	NTSTATUS status;
	DATA_BLOB blob_out;
	const char *OIDs_ntlm[] = {OID_NTLMSSP, NULL};

	req = tevent_req_create(mem_ctx, &state,
				struct cli_session_setup_ntlmssp_state);
	if (req == NULL) {
		return NULL;
	}
	state->ev = ev;
	state->cli = cli;
	state->turn = 1;

	state->ntlmssp_state = NULL;
	talloc_set_destructor(
		state, cli_session_setup_ntlmssp_state_destructor);

	cli_temp_set_signing(cli);

	status = ntlmssp_client_start(state,
				      global_myname(),
				      lp_workgroup(),
				      lp_client_ntlmv2_auth(),
				      &state->ntlmssp_state);
	if (!NT_STATUS_IS_OK(status)) {
		goto fail;
	}
	ntlmssp_want_feature(state->ntlmssp_state,
			     NTLMSSP_FEATURE_SESSION_KEY);
	if (cli->use_ccache) {
		ntlmssp_want_feature(state->ntlmssp_state,
				     NTLMSSP_FEATURE_CCACHE);
	}
	status = ntlmssp_set_username(state->ntlmssp_state, user);
	if (!NT_STATUS_IS_OK(status)) {
		goto fail;
	}
	status = ntlmssp_set_domain(state->ntlmssp_state, domain);
	if (!NT_STATUS_IS_OK(status)) {
		goto fail;
	}
	status = ntlmssp_set_password(state->ntlmssp_state, pass);
	if (!NT_STATUS_IS_OK(status)) {
		goto fail;
	}
	status = ntlmssp_update(state->ntlmssp_state, data_blob_null,
				&blob_out);
	if (!NT_STATUS_EQUAL(status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {
		goto fail;
	}

	state->blob_out = spnego_gen_negTokenInit(state, OIDs_ntlm, &blob_out, NULL);
	data_blob_free(&blob_out);

	subreq = cli_sesssetup_blob_send(state, ev, cli, state->blob_out);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_session_setup_ntlmssp_done, req);
	return req;
fail:
	tevent_req_nterror(req, status);
	return tevent_req_post(req, ev);
}

static void cli_session_setup_ntlmssp_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct cli_session_setup_ntlmssp_state *state = tevent_req_data(
		req, struct cli_session_setup_ntlmssp_state);
	DATA_BLOB blob_in, msg_in, blob_out;
	char *inbuf = NULL;
	bool parse_ret;
	NTSTATUS status;

	status = cli_sesssetup_blob_recv(subreq, talloc_tos(), &blob_in,
					 &inbuf);
	TALLOC_FREE(subreq);
	data_blob_free(&state->blob_out);

	if (NT_STATUS_IS_OK(status)) {
		if (state->cli->server_domain[0] == '\0') {
			TALLOC_FREE(state->cli->server_domain);
			state->cli->server_domain = talloc_strdup(state->cli,
						state->ntlmssp_state->server.netbios_domain);
			if (state->cli->server_domain == NULL) {
				TALLOC_FREE(subreq);
				tevent_req_nterror(req, NT_STATUS_NO_MEMORY);
				return;
			}
		}
		cli_set_session_key(
			state->cli, state->ntlmssp_state->session_key);

		if (cli_simple_set_signing(
			    state->cli, state->ntlmssp_state->session_key,
			    data_blob_null)
		    && !cli_check_sign_mac(state->cli, inbuf, 1)) {
			TALLOC_FREE(subreq);
			tevent_req_nterror(req, NT_STATUS_ACCESS_DENIED);
			return;
		}
		TALLOC_FREE(subreq);
		TALLOC_FREE(state->ntlmssp_state);
		tevent_req_done(req);
		return;
	}
	if (!NT_STATUS_EQUAL(status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {
		tevent_req_nterror(req, status);
		return;
	}

	if (blob_in.length == 0) {
		tevent_req_nterror(req, NT_STATUS_UNSUCCESSFUL);
		return;
	}

	if ((state->turn == 1)
	    && NT_STATUS_EQUAL(status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {
		DATA_BLOB tmp_blob = data_blob_null;
		/* the server might give us back two challenges */
		parse_ret = spnego_parse_challenge(state, blob_in, &msg_in,
						   &tmp_blob);
		data_blob_free(&tmp_blob);
	} else {
		parse_ret = spnego_parse_auth_response(state, blob_in, status,
						       OID_NTLMSSP, &msg_in);
	}
	state->turn += 1;

	if (!parse_ret) {
		DEBUG(3,("Failed to parse auth response\n"));
		if (NT_STATUS_IS_OK(status)
		    || NT_STATUS_EQUAL(status,
				       NT_STATUS_MORE_PROCESSING_REQUIRED)) {
			tevent_req_nterror(
				req, NT_STATUS_INVALID_NETWORK_RESPONSE);
			return;
		}
	}

	status = ntlmssp_update(state->ntlmssp_state, msg_in, &blob_out);

	if (!NT_STATUS_IS_OK(status)
	    && !NT_STATUS_EQUAL(status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {
		TALLOC_FREE(subreq);
		TALLOC_FREE(state->ntlmssp_state);
		tevent_req_nterror(req, status);
		return;
	}

	state->blob_out = spnego_gen_auth(state, blob_out);
	TALLOC_FREE(subreq);
	if (tevent_req_nomem(state->blob_out.data, req)) {
		return;
	}

	subreq = cli_sesssetup_blob_send(state, state->ev, state->cli,
					 state->blob_out);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, cli_session_setup_ntlmssp_done, req);
}

static NTSTATUS cli_session_setup_ntlmssp_recv(struct tevent_req *req)
{
	struct cli_session_setup_ntlmssp_state *state = tevent_req_data(
		req, struct cli_session_setup_ntlmssp_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		state->cli->vuid = 0;
		return status;
	}
	return NT_STATUS_OK;
}

static NTSTATUS cli_session_setup_ntlmssp(struct cli_state *cli,
					  const char *user,
					  const char *pass,
					  const char *domain)
{
	struct tevent_context *ev;
	struct tevent_req *req;
	NTSTATUS status = NT_STATUS_NO_MEMORY;

	if (cli_has_async_calls(cli)) {
		return NT_STATUS_INVALID_PARAMETER;
	}
	ev = tevent_context_init(talloc_tos());
	if (ev == NULL) {
		goto fail;
	}
	req = cli_session_setup_ntlmssp_send(ev, ev, cli, user, pass, domain);
	if (req == NULL) {
		goto fail;
	}
	if (!tevent_req_poll_ntstatus(req, ev, &status)) {
		goto fail;
	}
	status = cli_session_setup_ntlmssp_recv(req);
fail:
	TALLOC_FREE(ev);
	return status;
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
	if (!spnego_parse_negTokenInit(talloc_tos(), blob, OIDs, &principal, NULL) ||
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
		TALLOC_FREE(principal);
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

		/* We may not be allowed to use the server-supplied SPNEGO principal, or it may not have been supplied to us
		 */
		if (!lp_client_use_spnego_principal() || strequal(principal, ADS_IGNORE_PRINCIPAL)) {
			TALLOC_FREE(principal);
		}

		if (principal == NULL &&
			!is_ipaddress(cli->desthost) &&
			!strequal(STAR_SMBSERVER,
				cli->desthost)) {
			char *realm = NULL;
			char *host = NULL;
			DEBUG(3,("cli_session_setup_spnego: using target "
				 "hostname not SPNEGO principal\n"));

			host = strchr_m(cli->desthost, '.');
			if (dest_realm) {
				realm = SMB_STRDUP(dest_realm);
				if (!realm) {
					return ADS_ERROR_NT(NT_STATUS_NO_MEMORY);
				}
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

			if (realm == NULL || *realm == '\0') {
				realm = SMB_STRDUP(lp_realm());
				if (!realm) {
					return ADS_ERROR_NT(NT_STATUS_NO_MEMORY);
				}
				strupper_m(realm);
				DEBUG(3,("cli_session_setup_spnego: cannot "
					"get realm from dest_realm %s, "
					"desthost %s. Using default "
					"smb.conf realm %s\n",
					dest_realm ? dest_realm : "<null>",
					cli->desthost,
					realm));
			}

			principal = talloc_asprintf(talloc_tos(),
						    "cifs/%s@%s",
						    cli->desthost,
						    realm);
			if (!principal) {
				SAFE_FREE(realm);
				return ADS_ERROR_NT(NT_STATUS_NO_MEMORY);
			}
			DEBUG(3,("cli_session_setup_spnego: guessed "
				"server principal=%s\n",
				principal ? principal : "<null>"));

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
	char *user2;

	if (user) {
		user2 = talloc_strdup(talloc_tos(), user);
	} else {
		user2 = talloc_strdup(talloc_tos(), "");
	}
	if (user2 == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	if (!workgroup) {
		workgroup = "";
	}

	/* allow for workgroups as part of the username */
	if ((p=strchr_m(user2,'\\')) || (p=strchr_m(user2,'/')) ||
	    (p=strchr_m(user2,*lp_winbind_separator()))) {
		*p = 0;
		user = p+1;
		strupper_m(user2);
		workgroup = user2;
	}

	if (cli->protocol < PROTOCOL_LANMAN1) {
		/*
		 * Ensure cli->server_domain,
		 * cli->server_os and cli->server_type
		 * are valid pointers.
		 */
		cli->server_domain = talloc_strdup(cli, "");
		cli->server_os = talloc_strdup(cli, "");
		cli->server_type = talloc_strdup(cli, "");
		if (cli->server_domain == NULL ||
				cli->server_os == NULL ||
				cli->server_type == NULL) {
			return NT_STATUS_NO_MEMORY;
		}
		return NT_STATUS_OK;
	}

	/* now work out what sort of session setup we are going to
           do. I have split this into separate functions to make the
           flow a bit easier to understand (tridge) */

	/* if its an older server then we have to use the older request format */

	if (cli->protocol < PROTOCOL_NT1) {
		if (!lp_client_lanman_auth() && passlen != 24 && (*pass)) {
			DEBUG(1, ("Server requested LM password but 'client lanman auth = no'"
				  " or 'client ntlmv2 auth = yes'\n"));
			return NT_STATUS_ACCESS_DENIED;
		}

		if ((cli->sec_mode & NEGOTIATE_SECURITY_CHALLENGE_RESPONSE) == 0 &&
		    !lp_client_plaintext_auth() && (*pass)) {
			DEBUG(1, ("Server requested LM password but 'client plaintext auth = no'"
				  " or 'client ntlmv2 auth = yes'\n"));
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
		return cli_session_setup_plain(cli, user, "", workgroup);

	/* if the server doesn't support encryption then we have to use 
	   plaintext. The second password is ignored */

	if ((cli->sec_mode & NEGOTIATE_SECURITY_CHALLENGE_RESPONSE) == 0) {
		if (!lp_client_plaintext_auth() && (*pass)) {
			DEBUG(1, ("Server requested LM password but 'client plaintext auth = no'"
				  " or 'client ntlmv2 auth = yes'\n"));
			return NT_STATUS_ACCESS_DENIED;
		}
		return cli_session_setup_plain(cli, user, pass, workgroup);
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
		if (lp_client_ntlmv2_auth() && lp_client_use_spnego()) {
			/*
			 * Don't send an NTLMv2 response without NTLMSSP
			 * if we want to use spnego support
			 */
			DEBUG(1, ("Server does not support EXTENDED_SECURITY "
				  " but 'client use spnego = yes"
				  " and 'client ntlmv2 auth = yes'\n"));
			return NT_STATUS_ACCESS_DENIED;
		}

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

struct cli_ulogoff_state {
	struct cli_state *cli;
	uint16_t vwv[3];
};

static void cli_ulogoff_done(struct tevent_req *subreq);

struct tevent_req *cli_ulogoff_send(TALLOC_CTX *mem_ctx,
				    struct tevent_context *ev,
				    struct cli_state *cli)
{
	struct tevent_req *req, *subreq;
	struct cli_ulogoff_state *state;

	req = tevent_req_create(mem_ctx, &state, struct cli_ulogoff_state);
	if (req == NULL) {
		return NULL;
	}
	state->cli = cli;

	SCVAL(state->vwv+0, 0, 0xFF);
	SCVAL(state->vwv+1, 0, 0);
	SSVAL(state->vwv+2, 0, 0);

	subreq = cli_smb_send(state, ev, cli, SMBulogoffX, 0, 2, state->vwv,
			      0, NULL);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_ulogoff_done, req);
	return req;
}

static void cli_ulogoff_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct cli_ulogoff_state *state = tevent_req_data(
		req, struct cli_ulogoff_state);
	NTSTATUS status;

	status = cli_smb_recv(subreq, NULL, NULL, 0, NULL, NULL, NULL, NULL);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}
	state->cli->vuid = -1;
	tevent_req_done(req);
}

NTSTATUS cli_ulogoff_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}

NTSTATUS cli_ulogoff(struct cli_state *cli)
{
	struct tevent_context *ev;
	struct tevent_req *req;
	NTSTATUS status = NT_STATUS_NO_MEMORY;

	if (cli_has_async_calls(cli)) {
		return NT_STATUS_INVALID_PARAMETER;
	}
	ev = tevent_context_init(talloc_tos());
	if (ev == NULL) {
		goto fail;
	}
	req = cli_ulogoff_send(ev, ev, cli);
	if (req == NULL) {
		goto fail;
	}
	if (!tevent_req_poll_ntstatus(req, ev, &status)) {
		goto fail;
	}
	status = cli_ulogoff_recv(req);
fail:
	TALLOC_FREE(ev);
	return status;
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
	uint8_t p24[24];
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

	cli->share = talloc_strdup(cli, share);
	if (!cli->share) {
		return NULL;
	}

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
				  "'client lanman auth = no' or 'client ntlmv2 auth = yes'\n"));
			goto access_denied;
		}

		/*
		 * Non-encrypted passwords - convert to DOS codepage before
		 * encryption.
		 */
		SMBencrypt(pass, cli->secblob.data, p24);
		passlen = 24;
		pass = (const char *)p24;
	} else {
		if((cli->sec_mode & (NEGOTIATE_SECURITY_USER_LEVEL
				     |NEGOTIATE_SECURITY_CHALLENGE_RESPONSE))
		   == 0) {
			char *tmp_pass;

			if (!lp_client_plaintext_auth() && (*pass)) {
				DEBUG(1, ("Server requested plaintext "
					  "password but "
					  "'client lanman auth = no' or 'client ntlmv2 auth = yes'\n"));
				goto access_denied;
			}

			/*
			 * Non-encrypted passwords - convert to DOS codepage
			 * before using.
			 */
			tmp_pass = talloc_array(talloc_tos(), char, 128);
			if (tmp_pass == NULL) {
				tevent_req_nterror(req, NT_STATUS_NO_MEMORY);
				return tevent_req_post(req, ev);
			}
			passlen = clistr_push(cli,
					tmp_pass,
					pass,
					talloc_get_size(tmp_pass),
					STR_TERMINATE);
			if (passlen == -1) {
				tevent_req_nterror(req, NT_STATUS_NO_MEMORY);
				return tevent_req_post(req, ev);
			}
			pass = tmp_pass;
		}
	}

	SCVAL(vwv+0, 0, 0xFF);
	SCVAL(vwv+0, 1, 0);
	SSVAL(vwv+1, 0, 0);
	SSVAL(vwv+2, 0, TCONX_FLAG_EXTENDED_RESPONSE);
	SSVAL(vwv+3, 0, passlen);

	if (passlen && pass) {
		bytes = (uint8_t *)talloc_memdup(state, pass, passlen);
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
	uint8_t *in;
	char *inbuf;
	uint8_t wct;
	uint16_t *vwv;
	uint32_t num_bytes;
	uint8_t *bytes;
	NTSTATUS status;

	status = cli_smb_recv(subreq, state, &in, 0, &wct, &vwv,
			      &num_bytes, &bytes);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}

	inbuf = (char *)in;

	if (num_bytes) {
		if (clistr_pull_talloc(cli,
				inbuf,
				SVAL(inbuf, smb_flg2),
				&cli->dev,
				bytes,
				num_bytes,
				STR_TERMINATE|STR_ASCII) == -1) {
			tevent_req_nterror(req, NT_STATUS_NO_MEMORY);
			return;
		}
	} else {
		cli->dev = talloc_strdup(cli, "");
		if (cli->dev == NULL) {
			tevent_req_nterror(req, NT_STATUS_NO_MEMORY);
			return;
		}
	}

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
	return status;
}

/****************************************************************************
 Send a tree disconnect.
****************************************************************************/

struct cli_tdis_state {
	struct cli_state *cli;
};

static void cli_tdis_done(struct tevent_req *subreq);

struct tevent_req *cli_tdis_send(TALLOC_CTX *mem_ctx,
				 struct tevent_context *ev,
				 struct cli_state *cli)
{
	struct tevent_req *req, *subreq;
	struct cli_tdis_state *state;

	req = tevent_req_create(mem_ctx, &state, struct cli_tdis_state);
	if (req == NULL) {
		return NULL;
	}
	state->cli = cli;

	subreq = cli_smb_send(state, ev, cli, SMBtdis, 0, 0, NULL, 0, NULL);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_tdis_done, req);
	return req;
}

static void cli_tdis_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct cli_tdis_state *state = tevent_req_data(
		req, struct cli_tdis_state);
	NTSTATUS status;

	status = cli_smb_recv(subreq, NULL, NULL, 0, NULL, NULL, NULL, NULL);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}
	state->cli->cnum = -1;
	tevent_req_done(req);
}

NTSTATUS cli_tdis_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}

NTSTATUS cli_tdis(struct cli_state *cli)
{
	struct tevent_context *ev;
	struct tevent_req *req;
	NTSTATUS status = NT_STATUS_NO_MEMORY;

	if (cli_has_async_calls(cli)) {
		return NT_STATUS_INVALID_PARAMETER;
	}
	ev = tevent_context_init(talloc_tos());
	if (ev == NULL) {
		goto fail;
	}
	req = cli_tdis_send(ev, ev, cli);
	if (req == NULL) {
		goto fail;
	}
	if (!tevent_req_poll_ntstatus(req, ev, &status)) {
		goto fail;
	}
	status = cli_tdis_recv(req);
fail:
	TALLOC_FREE(ev);
	return status;
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

	if (cli->protocol < PROTOCOL_NT1)
		cli->use_spnego = False;

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
	uint8_t *inbuf;

	status = cli_smb_recv(subreq, state, &inbuf, 1, &wct, &vwv,
			      &num_bytes, &bytes);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
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
		DATA_BLOB blob = data_blob_null;

		if (wct != 0x11) {
			tevent_req_nterror(req, NT_STATUS_INVALID_NETWORK_RESPONSE);
			return;
		}

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
		    smb_buflen(inbuf) > 8) {
			blob = data_blob_const(bytes + 8, num_bytes - 8);
		}

		if (blob.length > 0) {
			ssize_t ret;
			char *server_domain = NULL;

			ret = clistr_pull_talloc(cli,
						 (const char *)inbuf,
						 SVAL(inbuf, smb_flg2),
						 &server_domain,
						 (char *)blob.data,
						 blob.length,
						 STR_TERMINATE|
						 STR_UNICODE|
						 STR_NOALIGN);
			if (ret == -1) {
				tevent_req_nterror(req, NT_STATUS_NO_MEMORY);
				return;
			}
			if (server_domain) {
				cli->server_domain = server_domain;
			}
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
			if (!cli->outbuf || !cli->inbuf) {
				tevent_req_nterror(req,
						NT_STATUS_NO_MEMORY);
				return;
			}
			cli->bufsize = CLI_SAMBA_MAX_LARGE_READX_SIZE + LARGE_WRITEX_HDR_SIZE;
		}

	} else if (cli->protocol >= PROTOCOL_LANMAN1) {
		if (wct != 0x0D) {
			tevent_req_nterror(req, NT_STATUS_INVALID_NETWORK_RESPONSE);
			return;
		}

		cli->use_spnego = False;
		cli->sec_mode = SVAL(vwv + 1, 0);
		cli->max_xmit = SVAL(vwv + 2, 0);
		cli->max_mux = SVAL(vwv + 3, 0);
		cli->sesskey = IVAL(vwv + 6, 0);
		cli->serverzone = SVALS(vwv + 10, 0);
		cli->serverzone *= 60;
		/* this time is converted to GMT by make_unix_date */
		cli->servertime = make_unix_date(
			(char *)(vwv + 8), cli->serverzone);
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

	cli->desthost = talloc_strdup(cli, host);
	if (cli->desthost == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	/* allow hostnames of the form NAME#xx and do a netbios lookup */
	if ((p = strchr(cli->desthost, '#'))) {
		name_type = strtol(p+1, NULL, 16);
		*p = 0;
	}

	if (!dest_ss || is_zero_addr(dest_ss)) {
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
*/
NTSTATUS cli_start_connection(struct cli_state **output_cli, 
			      const char *my_name, 
			      const char *dest_host, 
			      struct sockaddr_storage *dest_ss, int port,
			      int signing_state, int flags)
{
	NTSTATUS nt_status;
	struct nmb_name calling;
	struct nmb_name called;
	struct cli_state *cli;
	struct sockaddr_storage ss;

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
*/

NTSTATUS cli_full_connection(struct cli_state **output_cli, 
			     const char *my_name, 
			     const char *dest_host, 
			     struct sockaddr_storage *dest_ss, int port,
			     const char *service, const char *service_type,
			     const char *user, const char *domain, 
			     const char *password, int flags,
			     int signing_state)
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
					 flags);

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
	struct tevent_req *req;
	uint16_t *ret_vwv;
	uint8_t *bytes;
	NTSTATUS status;

	if (!lp_client_plaintext_auth() && (*pass)) {
		DEBUG(1, ("Server requested plaintext password but 'client "
			  "plaintext auth' is disabled\n"));
		return NT_STATUS_ACCESS_DENIED;
	}

	bytes = talloc_array(talloc_tos(), uint8_t, 0);
	bytes = smb_bytes_push_bytes(bytes, 4, NULL, 0);
	bytes = smb_bytes_push_str(bytes, cli_ucs2(cli),
				   service, strlen(service)+1, NULL);
	bytes = smb_bytes_push_bytes(bytes, 4, NULL, 0);
	bytes = smb_bytes_push_str(bytes, cli_ucs2(cli),
				   pass, strlen(pass)+1, NULL);
	bytes = smb_bytes_push_bytes(bytes, 4, NULL, 0);
	bytes = smb_bytes_push_str(bytes, cli_ucs2(cli),
				   dev, strlen(dev)+1, NULL);

	status = cli_smb(talloc_tos(), cli, SMBtcon, 0, 0, NULL,
			 talloc_get_size(bytes), bytes, &req,
			 2, NULL, &ret_vwv, NULL, NULL);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	*max_xmit = SVAL(ret_vwv + 0, 0);
	*tid = SVAL(ret_vwv + 1, 0);

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
					Undefined);

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
				struct sockaddr_storage *mb_ip,
				const struct user_auth_info *user_info,
				char **pp_workgroup_out)
{
	char addr[INET6_ADDRSTRLEN];
        fstring name;
	struct cli_state *cli;
	struct sockaddr_storage server_ss;

	*pp_workgroup_out = NULL;

	print_sockaddr(addr, sizeof(addr), mb_ip);
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
        if (!name_status_find("*", 0, 0x1d, mb_ip, name) &&
            !name_status_find(MSBROWSE, 1, 0x1d, mb_ip, name)) {

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
	struct sockaddr_storage *ip_list;
	struct cli_state *cli;
	int i, count;
	NTSTATUS status;

	*pp_workgroup_out = NULL;

        DEBUG(99, ("Do broadcast lookup for workgroups on local network\n"));

        /* Go looking for workgroups by broadcasting on the local network */

	status = name_resolve_bcast(MSBROWSE, 1, talloc_tos(),
				    &ip_list, &count);
        if (!NT_STATUS_IS_OK(status)) {
                DEBUG(99, ("No master browsers responded: %s\n",
			   nt_errstr(status)));
                return False;
        }

	for (i = 0; i < count; i++) {
		char addr[INET6_ADDRSTRLEN];
		print_sockaddr(addr, sizeof(addr), &ip_list[i]);
		DEBUG(99, ("Found master browser %s\n", addr));

		cli = get_ipc_connect_master_ip(ctx, &ip_list[i],
				user_info, pp_workgroup_out);
		if (cli)
			return(cli);
	}

	return NULL;
}
