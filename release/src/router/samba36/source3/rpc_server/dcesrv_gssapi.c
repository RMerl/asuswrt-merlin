/*
 *  GSSAPI Acceptor
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
#include "rpc_server/dcesrv_gssapi.h"
#include "../librpc/gen_ndr/ndr_krb5pac.h"
#include "librpc/crypto/gse.h"
#include "auth.h"

NTSTATUS gssapi_server_auth_start(TALLOC_CTX *mem_ctx,
				  bool do_sign,
				  bool do_seal,
				  bool is_dcerpc,
				  DATA_BLOB *token_in,
				  DATA_BLOB *token_out,
				  struct gse_context **ctx)
{
	struct gse_context *gse_ctx = NULL;
	uint32_t add_flags = 0;
        NTSTATUS status;

	if (is_dcerpc) {
		add_flags = GSS_C_DCE_STYLE;
	}

	/* Let's init the gssapi machinery for this connection */
	/* passing a NULL server name means the server will try
	 * to accept any connection regardless of the name used as
	 * long as it can find a decryption key */
	/* by passing NULL, the code will attempt to set a default
	 * keytab based on configuration options */
	status = gse_init_server(mem_ctx, do_sign, do_seal,
				 add_flags, NULL, &gse_ctx);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Failed to init dcerpc gssapi server (%s)\n",
			  nt_errstr(status)));
		return status;
	}

	status = gse_get_server_auth_token(mem_ctx, gse_ctx,
					   token_in, token_out);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Failed to parse initial client token (%s)\n",
			  nt_errstr(status)));
		goto done;
	}

	*ctx = gse_ctx;
	status = NT_STATUS_OK;

done:
	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(gse_ctx);
	}

	return status;
}

NTSTATUS gssapi_server_step(struct gse_context *gse_ctx,
			    TALLOC_CTX *mem_ctx,
			    DATA_BLOB *token_in,
			    DATA_BLOB *token_out)
{
	NTSTATUS status;

	status = gse_get_server_auth_token(mem_ctx, gse_ctx,
					   token_in, token_out);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (gse_require_more_processing(gse_ctx)) {
		/* ask for next leg */
		return NT_STATUS_MORE_PROCESSING_REQUIRED;
	}

	return NT_STATUS_OK;
}

NTSTATUS gssapi_server_check_flags(struct gse_context *gse_ctx)
{
	return gse_verify_server_auth_flags(gse_ctx);
}

NTSTATUS gssapi_server_get_user_info(struct gse_context *gse_ctx,
				     TALLOC_CTX *mem_ctx,
				     struct client_address *client_id,
				     struct auth_serversupplied_info **server_info)
{
	TALLOC_CTX *tmp_ctx;
	DATA_BLOB pac;
	struct PAC_DATA *pac_data;
	struct PAC_LOGON_INFO *logon_info = NULL;
	enum ndr_err_code ndr_err;
	unsigned int i;
	bool is_mapped;
	bool is_guest;
	char *princ_name;
	char *ntuser;
	char *ntdomain;
	char *username;
	struct passwd *pw;
	NTSTATUS status;

	tmp_ctx = talloc_new(mem_ctx);
	if (!tmp_ctx) {
		return NT_STATUS_NO_MEMORY;
	}

	status = gse_get_pac_blob(gse_ctx, tmp_ctx, &pac);
	if (NT_STATUS_EQUAL(status, NT_STATUS_NOT_FOUND)) {
		/* TODO: Fetch user by principal name ? */
		status = NT_STATUS_ACCESS_DENIED;
		goto done;
	}
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	status = gse_get_client_name(gse_ctx, tmp_ctx, &princ_name);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	pac_data = talloc_zero(tmp_ctx, struct PAC_DATA);
	if (!pac_data) {
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}

	ndr_err = ndr_pull_struct_blob(&pac, pac_data, pac_data,
				(ndr_pull_flags_fn_t)ndr_pull_PAC_DATA);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DEBUG(1, ("Failed to parse the PAC for %s\n", princ_name));
		status = ndr_map_error2ntstatus(ndr_err);
		goto done;
	}

	/* get logon name and logon info */
	for (i = 0; i < pac_data->num_buffers; i++) {
		struct PAC_BUFFER *data_buf = &pac_data->buffers[i];

		switch (data_buf->type) {
		case PAC_TYPE_LOGON_INFO:
			if (!data_buf->info) {
				break;
			}
			logon_info = data_buf->info->logon_info.info;
			break;
		default:
			break;
		}
	}
	if (!logon_info) {
		DEBUG(1, ("Invalid PAC data, missing logon info!\n"));
		status = NT_STATUS_NOT_FOUND;
		goto done;
	}

	/* TODO: Should we check princ_name against account_name in
	 * logon_name ? Are they supposed to be identical, or can an
	 * account_name be different from the UPN ? */

	status = get_user_from_kerberos_info(tmp_ctx, client_id->name,
					     princ_name, logon_info,
					     &is_mapped, &is_guest,
					     &ntuser, &ntdomain,
					     &username, &pw);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("Failed to map kerberos principal to system user "
			  "(%s)\n", nt_errstr(status)));
		status = NT_STATUS_ACCESS_DENIED;
		goto done;
	}

	/* TODO: save PAC data in netsamlogon cache ? */

	status = make_server_info_krb5(mem_ctx,
					ntuser, ntdomain, username, pw,
					logon_info, is_guest, server_info);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("Failed to map kerberos pac to server info (%s)\n",
			  nt_errstr(status)));
		status = NT_STATUS_ACCESS_DENIED;
		goto done;
	}

	DEBUG(5, (__location__ "OK: user: %s domain: %s client: %s\n",
		  ntuser, ntdomain, client_id->name));

	status = NT_STATUS_OK;

done:
	TALLOC_FREE(tmp_ctx);
	return status;
}
