/*
 *  SPNEGO Encapsulation
 *  DCERPC Server functions
 *  Copyright (C) Simo Sorce 2010.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"
#include "../libcli/auth/spnego.h"
#include "dcesrv_ntlmssp.h"
#include "dcesrv_gssapi.h"
#include "dcesrv_spnego.h"

static NTSTATUS spnego_init_server(TALLOC_CTX *mem_ctx,
				   bool do_sign, bool do_seal,
				   bool is_dcerpc,
				   struct spnego_context **spnego_ctx)
{
	struct spnego_context *sp_ctx = NULL;

	sp_ctx = talloc_zero(mem_ctx, struct spnego_context);
	if (!sp_ctx) {
		return NT_STATUS_NO_MEMORY;
	}

	sp_ctx->do_sign = do_sign;
	sp_ctx->do_seal = do_seal;
	sp_ctx->is_dcerpc = is_dcerpc;

	*spnego_ctx = sp_ctx;
	return NT_STATUS_OK;
}

static NTSTATUS spnego_server_mech_init(struct spnego_context *sp_ctx,
					DATA_BLOB *token_in,
					DATA_BLOB *token_out)
{
	struct auth_ntlmssp_state *ntlmssp_ctx;
	struct gse_context *gse_ctx;
	NTSTATUS status;

	switch (sp_ctx->mech) {
	case SPNEGO_KRB5:
		status = gssapi_server_auth_start(sp_ctx,
						  sp_ctx->do_sign,
						  sp_ctx->do_seal,
						  sp_ctx->is_dcerpc,
						  token_in,
						  token_out,
						  &gse_ctx);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0, ("Failed to init gssapi server "
				  "(%s)\n", nt_errstr(status)));
			return status;
		}

		sp_ctx->mech_ctx.gssapi_state = gse_ctx;
		break;

	case SPNEGO_NTLMSSP:
		status = ntlmssp_server_auth_start(sp_ctx,
						   sp_ctx->do_sign,
						   sp_ctx->do_seal,
						   sp_ctx->is_dcerpc,
						   token_in,
						   token_out,
						   &ntlmssp_ctx);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0, ("Failed to init ntlmssp server "
				  "(%s)\n", nt_errstr(status)));
			return status;
		}

		sp_ctx->mech_ctx.ntlmssp_state = ntlmssp_ctx;
		break;

	default:
		DEBUG(3, ("No known mechanisms available\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	return NT_STATUS_OK;
}

NTSTATUS spnego_server_step(struct spnego_context *sp_ctx,
			    TALLOC_CTX *mem_ctx,
			    DATA_BLOB *spnego_in,
			    DATA_BLOB *spnego_out)
{
	DATA_BLOB token_in = data_blob_null;
	DATA_BLOB token_out = data_blob_null;
	DATA_BLOB signature = data_blob_null;
	DATA_BLOB MICblob = data_blob_null;
	struct spnego_data sp_in;
	ssize_t len_in = 0;
	NTSTATUS status;
	bool ret;

	len_in = spnego_read_data(mem_ctx, *spnego_in, &sp_in);
	if (len_in == -1) {
		DEBUG(1, (__location__ ": invalid SPNEGO blob.\n"));
		dump_data(10, spnego_in->data, spnego_in->length);
		status = NT_STATUS_INVALID_PARAMETER;
		sp_ctx->state = SPNEGO_CONV_AUTH_DONE;
		goto done;
	}
	if (sp_in.type != SPNEGO_NEG_TOKEN_TARG) {
		status = NT_STATUS_INVALID_PARAMETER;
		goto done;
	}
	token_in = sp_in.negTokenTarg.responseToken;
	signature = sp_in.negTokenTarg.mechListMIC;

	switch (sp_ctx->state) {
	case SPNEGO_CONV_NEGO:
		/* still to initialize */
		status = spnego_server_mech_init(sp_ctx,
						 &token_in,
						 &token_out);
		if (!NT_STATUS_IS_OK(status)) {
			goto done;
		}
		/* server always need at least one reply from client */
		status = NT_STATUS_MORE_PROCESSING_REQUIRED;
		sp_ctx->state = SPNEGO_CONV_AUTH_MORE;
		break;

	case SPNEGO_CONV_AUTH_MORE:

		switch(sp_ctx->mech) {
		case SPNEGO_KRB5:
			status = gssapi_server_step(
					sp_ctx->mech_ctx.gssapi_state,
					mem_ctx, &token_in, &token_out);
			break;
		case SPNEGO_NTLMSSP:
			status = ntlmssp_server_step(
					sp_ctx->mech_ctx.ntlmssp_state,
					mem_ctx, &token_in, &token_out);
			break;
		default:
			status = NT_STATUS_INVALID_PARAMETER;
			goto done;
		}

		break;

	case SPNEGO_CONV_AUTH_DONE:
		/* we are already done, can't step further */
		/* fall thorugh and return error */
	default:
		/* wrong case */
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (NT_STATUS_IS_OK(status) && signature.length != 0) {
		/* last packet and requires signature check */
		ret = spnego_mech_list_blob(talloc_tos(),
					    sp_ctx->oid_list, &MICblob);
		if (ret) {
			status = spnego_sigcheck(talloc_tos(), sp_ctx,
						 &MICblob, &MICblob,
						 &signature);
		} else {
			status = NT_STATUS_UNSUCCESSFUL;
		}
	}
	if (NT_STATUS_IS_OK(status) && signature.length != 0) {
		/* if signature was good, sign our own packet too */
		status = spnego_sign(talloc_tos(), sp_ctx,
				     &MICblob, &MICblob, &signature);
	}

done:
	*spnego_out = spnego_gen_auth_response_and_mic(mem_ctx, status,
							sp_ctx->mech_oid,
							&token_out,
							&signature);
	if (!spnego_out->data) {
		DEBUG(1, ("SPNEGO wrapping failed!\n"));
		status = NT_STATUS_UNSUCCESSFUL;
	}

	if (NT_STATUS_IS_OK(status) ||
	    !NT_STATUS_EQUAL(status,
			NT_STATUS_MORE_PROCESSING_REQUIRED)) {
		sp_ctx->state = SPNEGO_CONV_AUTH_DONE;
	}

	data_blob_free(&token_in);
	data_blob_free(&token_out);
	return status;
}

NTSTATUS spnego_server_auth_start(TALLOC_CTX *mem_ctx,
				  bool do_sign,
				  bool do_seal,
				  bool is_dcerpc,
				  DATA_BLOB *spnego_in,
				  DATA_BLOB *spnego_out,
				  struct spnego_context **spnego_ctx)
{
	struct spnego_context *sp_ctx;
	DATA_BLOB token_in = data_blob_null;
	DATA_BLOB token_out = data_blob_null;
	unsigned int i;
	NTSTATUS status;
	bool ret;

	if (!spnego_in->length) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	status = spnego_init_server(mem_ctx, do_sign, do_seal, is_dcerpc, &sp_ctx);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	ret = spnego_parse_negTokenInit(sp_ctx, *spnego_in,
					sp_ctx->oid_list, NULL, &token_in);
	if (!ret || sp_ctx->oid_list[0] == NULL) {
		DEBUG(3, ("Invalid SPNEGO message\n"));
		status = NT_STATUS_INVALID_PARAMETER;
		goto done;
	}

	/* try for krb auth first */
	for (i = 0; sp_ctx->oid_list[i] && sp_ctx->mech == SPNEGO_NONE; i++) {
		if (strcmp(OID_KERBEROS5, sp_ctx->oid_list[i]) == 0 ||
		    strcmp(OID_KERBEROS5_OLD, sp_ctx->oid_list[i]) == 0) {

			if (lp_security() == SEC_ADS || USE_KERBEROS_KEYTAB) {
				sp_ctx->mech = SPNEGO_KRB5;
				sp_ctx->mech_oid = sp_ctx->oid_list[i];
			}
		}
	}

	/* if auth type still undetermined, try for NTLMSSP */
	for (i = 0; sp_ctx->oid_list[i] && sp_ctx->mech == SPNEGO_NONE; i++) {
		if (strcmp(OID_NTLMSSP, sp_ctx->oid_list[i]) == 0) {
			sp_ctx->mech = SPNEGO_NTLMSSP;
			sp_ctx->mech_oid = sp_ctx->oid_list[i];
		}
	}

	if (!sp_ctx->mech_oid) {
		DEBUG(3, ("No known mechanisms available\n"));
		status = NT_STATUS_INVALID_PARAMETER;
		goto done;
	}

	if (DEBUGLEVEL >= 10) {
		DEBUG(10, ("Client Provided OIDs:\n"));
		for (i = 0; sp_ctx->oid_list[i]; i++) {
			DEBUG(10, ("  %d: %s\n", i, sp_ctx->oid_list[i]));
		}
		DEBUG(10, ("Chosen OID: %s\n", sp_ctx->mech_oid));
	}

	/* If it is not the first OID, then token_in is not valid for the
	 * choosen mech */
	if (sp_ctx->mech_oid != sp_ctx->oid_list[0]) {
		/* request more and send back empty token */
		status = NT_STATUS_MORE_PROCESSING_REQUIRED;
		sp_ctx->state = SPNEGO_CONV_NEGO;
		goto done;
	}

	status = spnego_server_mech_init(sp_ctx, &token_in, &token_out);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	DEBUG(10, ("SPNEGO(%d) auth started\n", sp_ctx->mech));

	/* server always need at least one reply from client */
	status = NT_STATUS_MORE_PROCESSING_REQUIRED;
	sp_ctx->state = SPNEGO_CONV_AUTH_MORE;

done:
	*spnego_out = spnego_gen_auth_response(mem_ctx, &token_out,
						status, sp_ctx->mech_oid);
	if (!spnego_out->data) {
		status = NT_STATUS_INVALID_PARAMETER;
		TALLOC_FREE(sp_ctx);
	} else {
		status = NT_STATUS_OK;
		*spnego_ctx = sp_ctx;
	}

	data_blob_free(&token_in);
	data_blob_free(&token_out);

	return status;
}
