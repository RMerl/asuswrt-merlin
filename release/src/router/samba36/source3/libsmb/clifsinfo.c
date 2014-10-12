/* 
   Unix SMB/CIFS implementation.
   FS info functions
   Copyright (C) Stefan (metze) Metzmacher	2003
   Copyright (C) Jeremy Allison 2007

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
#include "../libcli/auth/spnego.h"
#include "../libcli/auth/ntlmssp.h"
#include "../lib/util/tevent_ntstatus.h"
#include "async_smb.h"
#include "smb_crypt.h"
#include "trans2.h"

/****************************************************************************
 Get UNIX extensions version info.
****************************************************************************/

struct cli_unix_extensions_version_state {
	struct cli_state *cli;
	uint16_t setup[1];
	uint8_t param[2];
	uint16_t major, minor;
	uint32_t caplow, caphigh;
};

static void cli_unix_extensions_version_done(struct tevent_req *subreq);

struct tevent_req *cli_unix_extensions_version_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct cli_state *cli)
{
	struct tevent_req *req, *subreq;
	struct cli_unix_extensions_version_state *state;

	req = tevent_req_create(mem_ctx, &state,
				struct cli_unix_extensions_version_state);
	if (req == NULL) {
		return NULL;
	}
	state->cli = cli;
	SSVAL(state->setup, 0, TRANSACT2_QFSINFO);
	SSVAL(state->param, 0, SMB_QUERY_CIFS_UNIX_INFO);

	subreq = cli_trans_send(state, ev, cli, SMBtrans2,
				NULL, 0, 0, 0,
				state->setup, 1, 0,
				state->param, 2, 0,
				NULL, 0, 560);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_unix_extensions_version_done, req);
	return req;
}

static void cli_unix_extensions_version_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct cli_unix_extensions_version_state *state = tevent_req_data(
		req, struct cli_unix_extensions_version_state);
	uint8_t *data;
	uint32_t num_data;
	NTSTATUS status;

	status = cli_trans_recv(subreq, state, NULL, NULL, 0, NULL,
				NULL, 0, NULL, &data, 12, &num_data);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}

	state->major = SVAL(data, 0);
	state->minor = SVAL(data, 2);
	state->caplow = IVAL(data, 4);
	state->caphigh = IVAL(data, 8);
	TALLOC_FREE(data);
	tevent_req_done(req);
}

NTSTATUS cli_unix_extensions_version_recv(struct tevent_req *req,
					  uint16_t *pmajor, uint16_t *pminor,
					  uint32_t *pcaplow,
					  uint32_t *pcaphigh)
{
	struct cli_unix_extensions_version_state *state = tevent_req_data(
		req, struct cli_unix_extensions_version_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}
	*pmajor = state->major;
	*pminor = state->minor;
	*pcaplow = state->caplow;
	*pcaphigh = state->caphigh;
	state->cli->server_posix_capabilities = *pcaplow;
	return NT_STATUS_OK;
}

NTSTATUS cli_unix_extensions_version(struct cli_state *cli, uint16 *pmajor,
				     uint16 *pminor, uint32 *pcaplow,
				     uint32 *pcaphigh)
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

	req = cli_unix_extensions_version_send(frame, ev, cli);
	if (req == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	if (!tevent_req_poll(req, ev)) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}

	status = cli_unix_extensions_version_recv(req, pmajor, pminor, pcaplow,
						  pcaphigh);
 fail:
	TALLOC_FREE(frame);
	return status;
}

/****************************************************************************
 Set UNIX extensions capabilities.
****************************************************************************/

struct cli_set_unix_extensions_capabilities_state {
	struct cli_state *cli;
	uint16_t setup[1];
	uint8_t param[4];
	uint8_t data[12];
};

static void cli_set_unix_extensions_capabilities_done(
	struct tevent_req *subreq);

struct tevent_req *cli_set_unix_extensions_capabilities_send(
	TALLOC_CTX *mem_ctx, struct tevent_context *ev, struct cli_state *cli,
	uint16_t major, uint16_t minor, uint32_t caplow, uint32_t caphigh)
{
	struct tevent_req *req, *subreq;
	struct cli_set_unix_extensions_capabilities_state *state;

	req = tevent_req_create(
		mem_ctx, &state,
		struct cli_set_unix_extensions_capabilities_state);
	if (req == NULL) {
		return NULL;
	}

	state->cli = cli;
	SSVAL(state->setup+0, 0, TRANSACT2_SETFSINFO);

	SSVAL(state->param, 0, 0);
	SSVAL(state->param, 2, SMB_SET_CIFS_UNIX_INFO);

	SSVAL(state->data, 0, major);
	SSVAL(state->data, 2, minor);
	SIVAL(state->data, 4, caplow);
	SIVAL(state->data, 8, caphigh);

	subreq = cli_trans_send(state, ev, cli, SMBtrans2,
				NULL, 0, 0, 0,
				state->setup, 1, 0,
				state->param, 4, 0,
				state->data, 12, 560);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(
		subreq, cli_set_unix_extensions_capabilities_done, req);
	return req;
}

static void cli_set_unix_extensions_capabilities_done(
	struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct cli_set_unix_extensions_capabilities_state *state = tevent_req_data(
		req, struct cli_set_unix_extensions_capabilities_state);

	NTSTATUS status = cli_trans_recv(subreq, NULL, NULL, NULL, 0, NULL,
					 NULL, 0, NULL, NULL, 0, NULL);
	if (NT_STATUS_IS_OK(status)) {
		state->cli->requested_posix_capabilities = IVAL(state->data, 4);
	}
	tevent_req_simple_finish_ntstatus(subreq, status);
}

NTSTATUS cli_set_unix_extensions_capabilities_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}

NTSTATUS cli_set_unix_extensions_capabilities(struct cli_state *cli,
					      uint16 major, uint16 minor,
					      uint32 caplow, uint32 caphigh)
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
	req = cli_set_unix_extensions_capabilities_send(
		ev, ev, cli, major, minor, caplow, caphigh);
	if (req == NULL) {
		goto fail;
	}
	if (!tevent_req_poll_ntstatus(req, ev, &status)) {
		goto fail;
	}
	status = cli_set_unix_extensions_capabilities_recv(req);
fail:
	TALLOC_FREE(ev);
	return status;
}

struct cli_get_fs_attr_info_state {
	uint16_t setup[1];
	uint8_t param[2];
	uint32_t fs_attr;
};

static void cli_get_fs_attr_info_done(struct tevent_req *subreq);

struct tevent_req *cli_get_fs_attr_info_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct cli_state *cli)
{
	struct tevent_req *subreq, *req;
	struct cli_get_fs_attr_info_state *state;

	req = tevent_req_create(mem_ctx, &state,
				struct cli_get_fs_attr_info_state);
	if (req == NULL) {
		return NULL;
	}
	SSVAL(state->setup+0, 0, TRANSACT2_QFSINFO);
	SSVAL(state->param+0, 0, SMB_QUERY_FS_ATTRIBUTE_INFO);

	subreq = cli_trans_send(state, ev, cli, SMBtrans2,
				NULL, 0, 0, 0,
				state->setup, 1, 0,
				state->param, 2, 0,
				NULL, 0, 560);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_get_fs_attr_info_done, req);
	return req;
}

static void cli_get_fs_attr_info_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct cli_get_fs_attr_info_state *state = tevent_req_data(
		req, struct cli_get_fs_attr_info_state);
	uint8_t *data;
	uint32_t num_data;
	NTSTATUS status;

	status = cli_trans_recv(subreq, talloc_tos(), NULL, NULL, 0, NULL,
				NULL, 0, NULL, &data, 12, &num_data);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}
	state->fs_attr = IVAL(data, 0);
	TALLOC_FREE(data);
	tevent_req_done(req);
}

NTSTATUS cli_get_fs_attr_info_recv(struct tevent_req *req, uint32_t *fs_attr)
{
	struct cli_get_fs_attr_info_state *state = tevent_req_data(
		req, struct cli_get_fs_attr_info_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}
	*fs_attr = state->fs_attr;
	return NT_STATUS_OK;
}

NTSTATUS cli_get_fs_attr_info(struct cli_state *cli, uint32_t *fs_attr)
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
	req = cli_get_fs_attr_info_send(ev, ev, cli);
	if (req == NULL) {
		goto fail;
	}
	if (!tevent_req_poll_ntstatus(req, ev, &status)) {
		goto fail;
	}
	status = cli_get_fs_attr_info_recv(req, fs_attr);
fail:
	TALLOC_FREE(ev);
	return status;
}

NTSTATUS cli_get_fs_volume_info(struct cli_state *cli, fstring volume_name,
				uint32 *pserial_number, time_t *pdate)
{
	NTSTATUS status;
	uint16 setup[1];
	uint8_t param[2];
	uint8_t *rdata;
	uint32_t rdata_count;
	unsigned int nlen;

	SSVAL(setup, 0, TRANSACT2_QFSINFO);
	SSVAL(param,0,SMB_QUERY_FS_VOLUME_INFO);

	status = cli_trans(talloc_tos(), cli, SMBtrans2,
			   NULL, 0, 0, 0,
			   setup, 1, 0,
			   param, 2, 0,
			   NULL, 0, 560,
			   NULL,
			   NULL, 0, NULL,
			   NULL, 0, NULL,
			   &rdata, 10, &rdata_count);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (pdate) {
		struct timespec ts;
		ts = interpret_long_date((char *)rdata);
		*pdate = ts.tv_sec;
	}
	if (pserial_number) {
		*pserial_number = IVAL(rdata,8);
	}
	nlen = IVAL(rdata,12);
	clistr_pull(cli->inbuf, volume_name, rdata + 18, sizeof(fstring),
		    nlen, STR_UNICODE);

	/* todo: but not yet needed
	 *       return the other stuff
	 */

	TALLOC_FREE(rdata);
	return NT_STATUS_OK;
}

NTSTATUS cli_get_fs_full_size_info(struct cli_state *cli,
				   uint64_t *total_allocation_units,
				   uint64_t *caller_allocation_units,
				   uint64_t *actual_allocation_units,
				   uint64_t *sectors_per_allocation_unit,
				   uint64_t *bytes_per_sector)
{
	uint16 setup[1];
	uint8_t param[2];
	uint8_t *rdata = NULL;
	uint32_t rdata_count;
	NTSTATUS status;

	SSVAL(setup, 0, TRANSACT2_QFSINFO);
	SSVAL(param, 0, SMB_FS_FULL_SIZE_INFORMATION);

	status = cli_trans(talloc_tos(), cli, SMBtrans2,
			   NULL, 0, 0, 0,
			   setup, 1, 0, /* setup */
			   param, 2, 0,	 /* param */
			   NULL, 0, 560, /* data */
			   NULL,
			   NULL, 0, NULL, /* rsetup */
			   NULL, 0, NULL, /* rparam */
			   &rdata, 32, &rdata_count);  /* rdata */
	if (!NT_STATUS_IS_OK(status)) {
		goto fail;
	}

	if (total_allocation_units) {
                *total_allocation_units = BIG_UINT(rdata, 0);
	}
	if (caller_allocation_units) {
		*caller_allocation_units = BIG_UINT(rdata,8);
	}
	if (actual_allocation_units) {
		*actual_allocation_units = BIG_UINT(rdata,16);
	}
	if (sectors_per_allocation_unit) {
		*sectors_per_allocation_unit = IVAL(rdata,24);
	}
	if (bytes_per_sector) {
		*bytes_per_sector = IVAL(rdata,28);
	}

fail:
	TALLOC_FREE(rdata);
	return status;
}

NTSTATUS cli_get_posix_fs_info(struct cli_state *cli,
			       uint32 *optimal_transfer_size,
			       uint32 *block_size,
			       uint64_t *total_blocks,
			       uint64_t *blocks_available,
			       uint64_t *user_blocks_available,
			       uint64_t *total_file_nodes,
			       uint64_t *free_file_nodes,
			       uint64_t *fs_identifier)
{
	uint16 setup[1];
	uint8_t param[2];
	uint8_t *rdata = NULL;
	uint32_t rdata_count;
	NTSTATUS status;

	SSVAL(setup, 0, TRANSACT2_QFSINFO);
	SSVAL(param,0,SMB_QUERY_POSIX_FS_INFO);

	status = cli_trans(talloc_tos(), cli, SMBtrans2, NULL, 0, 0, 0,
			   setup, 1, 0,
			   param, 2, 0,
			   NULL, 0, 560,
			   NULL,
			   NULL, 0, NULL, /* rsetup */
			   NULL, 0, NULL, /* rparam */
			   &rdata, 56, &rdata_count);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (optimal_transfer_size) {
                *optimal_transfer_size = IVAL(rdata, 0);
	}
	if (block_size) {
		*block_size = IVAL(rdata,4);
	}
	if (total_blocks) {
		*total_blocks = BIG_UINT(rdata,8);
	}
	if (blocks_available) {
		*blocks_available = BIG_UINT(rdata,16);
	}
	if (user_blocks_available) {
		*user_blocks_available = BIG_UINT(rdata,24);
	}
	if (total_file_nodes) {
		*total_file_nodes = BIG_UINT(rdata,32);
	}
	if (free_file_nodes) {
		*free_file_nodes = BIG_UINT(rdata,40);
	}
	if (fs_identifier) {
		*fs_identifier = BIG_UINT(rdata,48);
	}
	return NT_STATUS_OK;
}


/******************************************************************************
 Send/receive the request encryption blob.
******************************************************************************/

static NTSTATUS enc_blob_send_receive(struct cli_state *cli, DATA_BLOB *in, DATA_BLOB *out, DATA_BLOB *param_out)
{
	uint16_t setup[1];
	uint8_t param[4];
	uint8_t *rparam=NULL, *rdata=NULL;
	uint32_t num_rparam, num_rdata;
	NTSTATUS status;

	SSVAL(setup+0, 0, TRANSACT2_SETFSINFO);
	SSVAL(param,0,0);
	SSVAL(param,2,SMB_REQUEST_TRANSPORT_ENCRYPTION);

	status = cli_trans(talloc_tos(), cli, SMBtrans2, NULL, 0, 0, 0,
			   setup, 1, 0,
			   param, 4, 2,
			   (uint8_t *)in->data, in->length, CLI_BUFFER_SIZE,
			   NULL,	  /* recv_flags */
			   NULL, 0, NULL, /* rsetup */
			   &rparam, 0, &num_rparam,
			   &rdata, 0, &num_rdata);

	if (!NT_STATUS_IS_OK(status) &&
	    !NT_STATUS_EQUAL(status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {
		return status;
	}

	*out = data_blob(rdata, num_rdata);
	*param_out = data_blob(rparam, num_rparam);

	TALLOC_FREE(rparam);
	TALLOC_FREE(rdata);
	return status;
}

/******************************************************************************
 Make a client state struct.
******************************************************************************/

static struct smb_trans_enc_state *make_cli_enc_state(enum smb_trans_enc_type smb_enc_type)
{
	struct smb_trans_enc_state *es = NULL;
	es = SMB_MALLOC_P(struct smb_trans_enc_state);
	if (!es) {
		return NULL;
	}
	ZERO_STRUCTP(es);
	es->smb_enc_type = smb_enc_type;

#if defined(HAVE_GSSAPI) && defined(HAVE_KRB5)
	if (smb_enc_type == SMB_TRANS_ENC_GSS) {
		es->s.gss_state = SMB_MALLOC_P(struct smb_tran_enc_state_gss);
		if (!es->s.gss_state) {
			SAFE_FREE(es);
			return NULL;
		}
		ZERO_STRUCTP(es->s.gss_state);
	}
#endif
	return es;
}

/******************************************************************************
 Start a raw ntlmssp encryption.
******************************************************************************/

NTSTATUS cli_raw_ntlm_smb_encryption_start(struct cli_state *cli, 
				const char *user,
				const char *pass,
				const char *domain)
{
	DATA_BLOB blob_in = data_blob_null;
	DATA_BLOB blob_out = data_blob_null;
	DATA_BLOB param_out = data_blob_null;
	NTSTATUS status = NT_STATUS_UNSUCCESSFUL;
	struct smb_trans_enc_state *es = make_cli_enc_state(SMB_TRANS_ENC_NTLM);

	if (!es) {
		return NT_STATUS_NO_MEMORY;
	}
	status = ntlmssp_client_start(NULL,
				      global_myname(),
				      lp_workgroup(),
				      lp_client_ntlmv2_auth(),
				      &es->s.ntlmssp_state);
	if (!NT_STATUS_IS_OK(status)) {
		goto fail;
	}

	ntlmssp_want_feature(es->s.ntlmssp_state, NTLMSSP_FEATURE_SESSION_KEY);
	es->s.ntlmssp_state->neg_flags |= (NTLMSSP_NEGOTIATE_SIGN|NTLMSSP_NEGOTIATE_SEAL);

	if (!NT_STATUS_IS_OK(status = ntlmssp_set_username(es->s.ntlmssp_state, user))) {
		goto fail;
	}
	if (!NT_STATUS_IS_OK(status = ntlmssp_set_domain(es->s.ntlmssp_state, domain))) {
		goto fail;
	}
	if (!NT_STATUS_IS_OK(status = ntlmssp_set_password(es->s.ntlmssp_state, pass))) {
		goto fail;
	}

	do {
		status = ntlmssp_update(es->s.ntlmssp_state, blob_in, &blob_out);
		data_blob_free(&blob_in);
		data_blob_free(&param_out);
		if (NT_STATUS_EQUAL(status, NT_STATUS_MORE_PROCESSING_REQUIRED) || NT_STATUS_IS_OK(status)) {
			NTSTATUS trans_status = enc_blob_send_receive(cli,
									&blob_out,
									&blob_in,
									&param_out);
			if (!NT_STATUS_EQUAL(trans_status,
					NT_STATUS_MORE_PROCESSING_REQUIRED) &&
					!NT_STATUS_IS_OK(trans_status)) {
				status = trans_status;
			} else {
				if (param_out.length == 2) {
					es->enc_ctx_num = SVAL(param_out.data, 0);
				}
			}
		}
		data_blob_free(&blob_out);
	} while (NT_STATUS_EQUAL(status, NT_STATUS_MORE_PROCESSING_REQUIRED));

	data_blob_free(&blob_in);

	if (NT_STATUS_IS_OK(status)) {
		/* Replace the old state, if any. */
		if (cli->trans_enc_state) {
			common_free_encryption_state(&cli->trans_enc_state);
		}
		cli->trans_enc_state = es;
		cli->trans_enc_state->enc_on = True;
		es = NULL;
	}

  fail:

	common_free_encryption_state(&es);
	return status;
}

#if defined(HAVE_GSSAPI) && defined(HAVE_KRB5)

#ifndef SMB_GSS_REQUIRED_FLAGS
#define SMB_GSS_REQUIRED_FLAGS (GSS_C_CONF_FLAG|GSS_C_INTEG_FLAG|GSS_C_MUTUAL_FLAG|GSS_C_REPLAY_FLAG|GSS_C_SEQUENCE_FLAG)
#endif

/******************************************************************************
 Get client gss blob to send to a server.
******************************************************************************/

static NTSTATUS make_cli_gss_blob(TALLOC_CTX *ctx,
				struct smb_trans_enc_state *es,
				const char *service,
				const char *host,
				NTSTATUS status_in,
				DATA_BLOB spnego_blob_in,
				DATA_BLOB *p_blob_out)
{
	const char *krb_mechs[] = {OID_KERBEROS5, NULL};
	OM_uint32 ret;
	OM_uint32 min;
	gss_name_t srv_name;
	gss_buffer_desc input_name;
	gss_buffer_desc *p_tok_in;
	gss_buffer_desc tok_out, tok_in;
	DATA_BLOB blob_out = data_blob_null;
	DATA_BLOB blob_in = data_blob_null;
	char *host_princ_s = NULL;
	OM_uint32 ret_flags = 0;
	NTSTATUS status = NT_STATUS_OK;

	gss_OID_desc nt_hostbased_service =
	{10, CONST_DISCARD(char *,"\x2a\x86\x48\x86\xf7\x12\x01\x02\x01\x04")};

	memset(&tok_out, '\0', sizeof(tok_out));

	/* Get a ticket for the service@host */
	if (asprintf(&host_princ_s, "%s@%s", service, host) == -1) {
		return NT_STATUS_NO_MEMORY;
	}

	input_name.value = host_princ_s;
	input_name.length = strlen(host_princ_s) + 1;

	ret = gss_import_name(&min,
				&input_name,
				&nt_hostbased_service,
				&srv_name);

	if (ret != GSS_S_COMPLETE) {
		SAFE_FREE(host_princ_s);
		return map_nt_error_from_gss(ret, min);
	}

	if (spnego_blob_in.length == 0) {
		p_tok_in = GSS_C_NO_BUFFER;
	} else {
		/* Remove the SPNEGO wrapper */
		if (!spnego_parse_auth_response(ctx, spnego_blob_in, status_in, OID_KERBEROS5, &blob_in)) {
			status = NT_STATUS_UNSUCCESSFUL;
			goto fail;
		}
		tok_in.value = blob_in.data;
		tok_in.length = blob_in.length;
		p_tok_in = &tok_in;
	}

	ret = gss_init_sec_context(&min,
				GSS_C_NO_CREDENTIAL, /* Use our default cred. */
				&es->s.gss_state->gss_ctx,
				srv_name,
				GSS_C_NO_OID, /* default OID. */
				GSS_C_MUTUAL_FLAG | GSS_C_REPLAY_FLAG | GSS_C_SEQUENCE_FLAG | GSS_C_DELEG_FLAG,
				GSS_C_INDEFINITE,	/* requested ticket lifetime. */
				NULL,   /* no channel bindings */
				p_tok_in,
				NULL,   /* ignore mech type */
				&tok_out,
				&ret_flags,
				NULL);  /* ignore time_rec */

	status = map_nt_error_from_gss(ret, min);
	if (!NT_STATUS_IS_OK(status) && !NT_STATUS_EQUAL(status,NT_STATUS_MORE_PROCESSING_REQUIRED)) {
		ADS_STATUS adss = ADS_ERROR_GSS(ret, min);
		DEBUG(10,("make_cli_gss_blob: gss_init_sec_context failed with %s\n",
			ads_errstr(adss)));
		goto fail;
	}

	if ((ret_flags & SMB_GSS_REQUIRED_FLAGS) != SMB_GSS_REQUIRED_FLAGS) {
		status = NT_STATUS_ACCESS_DENIED;
	}

	blob_out = data_blob_talloc(ctx, tok_out.value, tok_out.length);

	/* Wrap in an SPNEGO wrapper */
	*p_blob_out = spnego_gen_negTokenInit(ctx, krb_mechs, &blob_out, NULL);

  fail:

	data_blob_free(&blob_out);
	data_blob_free(&blob_in);
	SAFE_FREE(host_princ_s);
	gss_release_name(&min, &srv_name);
	if (tok_out.value) {
		gss_release_buffer(&min, &tok_out);
	}
	return status;
}

/******************************************************************************
 Start a SPNEGO gssapi encryption context.
******************************************************************************/

NTSTATUS cli_gss_smb_encryption_start(struct cli_state *cli)
{
	DATA_BLOB blob_recv = data_blob_null;
	DATA_BLOB blob_send = data_blob_null;
	DATA_BLOB param_out = data_blob_null;
	NTSTATUS status = NT_STATUS_UNSUCCESSFUL;
	fstring fqdn;
	const char *servicename;
	struct smb_trans_enc_state *es = make_cli_enc_state(SMB_TRANS_ENC_GSS);

	if (!es) {
		return NT_STATUS_NO_MEMORY;
	}

	name_to_fqdn(fqdn, cli->desthost);
	strlower_m(fqdn);

	servicename = "cifs";
	status = make_cli_gss_blob(talloc_tos(), es, servicename, fqdn, NT_STATUS_OK, blob_recv, &blob_send);
	if (!NT_STATUS_EQUAL(status,NT_STATUS_MORE_PROCESSING_REQUIRED)) {
		servicename = "host";
		status = make_cli_gss_blob(talloc_tos(), es, servicename, fqdn, NT_STATUS_OK, blob_recv, &blob_send);
		if (!NT_STATUS_EQUAL(status,NT_STATUS_MORE_PROCESSING_REQUIRED)) {
			goto fail;
		}
	}

	do {
		data_blob_free(&blob_recv);
		status = enc_blob_send_receive(cli, &blob_send, &blob_recv, &param_out);
		if (param_out.length == 2) {
			es->enc_ctx_num = SVAL(param_out.data, 0);
		}
		data_blob_free(&blob_send);
		status = make_cli_gss_blob(talloc_tos(), es, servicename, fqdn, status, blob_recv, &blob_send);
	} while (NT_STATUS_EQUAL(status, NT_STATUS_MORE_PROCESSING_REQUIRED));
	data_blob_free(&blob_recv);

	if (NT_STATUS_IS_OK(status)) {
		/* Replace the old state, if any. */
		if (cli->trans_enc_state) {
			common_free_encryption_state(&cli->trans_enc_state);
		}
		cli->trans_enc_state = es;
		cli->trans_enc_state->enc_on = True;
		es = NULL;
	}

  fail:

	common_free_encryption_state(&es);
	return status;
}
#else
NTSTATUS cli_gss_smb_encryption_start(struct cli_state *cli)
{
	return NT_STATUS_NOT_SUPPORTED;
}
#endif

/********************************************************************
 Ensure a connection is encrypted.
********************************************************************/

NTSTATUS cli_force_encryption(struct cli_state *c,
			const char *username,
			const char *password,
			const char *domain)
{
	uint16 major, minor;
	uint32 caplow, caphigh;
	NTSTATUS status;

	if (!SERVER_HAS_UNIX_CIFS(c)) {
		return NT_STATUS_NOT_SUPPORTED;
	}

	status = cli_unix_extensions_version(c, &major, &minor, &caplow,
					     &caphigh);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("cli_force_encryption: cli_unix_extensions_version "
			   "returned %s\n", nt_errstr(status)));
		return NT_STATUS_UNKNOWN_REVISION;
	}

	if (!(caplow & CIFS_UNIX_TRANSPORT_ENCRYPTION_CAP)) {
		return NT_STATUS_UNSUPPORTED_COMPRESSION;
	}

	if (c->use_kerberos) {
		return cli_gss_smb_encryption_start(c);
	}
	return cli_raw_ntlm_smb_encryption_start(c,
					username,
					password,
					domain);
}
