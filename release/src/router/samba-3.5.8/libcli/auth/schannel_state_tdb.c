/*
   Unix SMB/CIFS implementation.

   module to store/fetch session keys for the schannel server

   Copyright (C) Andrew Tridgell 2004
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2006-2009
   Copyright (C) Guenther Deschner 2009

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
#include "../libcli/auth/schannel_state.h"
#include "../librpc/gen_ndr/ndr_schannel.h"

/********************************************************************
 ********************************************************************/

NTSTATUS schannel_store_session_key_tdb(struct tdb_context *tdb,
					TALLOC_CTX *mem_ctx,
					struct netlogon_creds_CredentialState *creds)
{
	enum ndr_err_code ndr_err;
	DATA_BLOB blob;
	TDB_DATA value;
	int ret;
	char *keystr;

	keystr = talloc_asprintf_strupper_m(mem_ctx, "%s/%s",
					    SECRETS_SCHANNEL_STATE,
					    creds->computer_name);
	if (!keystr) {
		return NT_STATUS_NO_MEMORY;
	}

	ndr_err = ndr_push_struct_blob(&blob, mem_ctx, NULL, creds,
			(ndr_push_flags_fn_t)ndr_push_netlogon_creds_CredentialState);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(keystr);
		return ndr_map_error2ntstatus(ndr_err);
	}

	value.dptr = blob.data;
	value.dsize = blob.length;

	ret = tdb_store_bystring(tdb, keystr, value, TDB_REPLACE);
	if (ret != TDB_SUCCESS) {
		DEBUG(0,("Unable to add %s to session key db - %s\n",
			 keystr, tdb_errorstr(tdb)));
		talloc_free(keystr);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	DEBUG(3,("schannel_store_session_key_tdb: stored schannel info with key %s\n",
		keystr));

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_DEBUG(netlogon_creds_CredentialState, creds);
	}

	talloc_free(keystr);

	return NT_STATUS_OK;
}

/********************************************************************
 ********************************************************************/

NTSTATUS schannel_fetch_session_key_tdb(struct tdb_context *tdb,
					TALLOC_CTX *mem_ctx,
					const char *computer_name,
					struct netlogon_creds_CredentialState **pcreds)
{
	NTSTATUS status;
	TDB_DATA value;
	enum ndr_err_code ndr_err;
	DATA_BLOB blob;
	struct netlogon_creds_CredentialState *creds = NULL;
	char *keystr = NULL;

	*pcreds = NULL;

	keystr = talloc_asprintf_strupper_m(mem_ctx, "%s/%s",
					    SECRETS_SCHANNEL_STATE,
					    computer_name);
	if (!keystr) {
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}

	value = tdb_fetch_bystring(tdb, keystr);
	if (!value.dptr) {
		DEBUG(0,("schannel_fetch_session_key_tdb: Failed to find entry with key %s\n",
			keystr ));
		status = NT_STATUS_OBJECT_NAME_NOT_FOUND;
		goto done;
	}

	creds = talloc_zero(mem_ctx, struct netlogon_creds_CredentialState);
	if (!creds) {
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}

	blob = data_blob_const(value.dptr, value.dsize);

	ndr_err = ndr_pull_struct_blob(&blob, mem_ctx, NULL, creds,
			(ndr_pull_flags_fn_t)ndr_pull_netlogon_creds_CredentialState);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		status = ndr_map_error2ntstatus(ndr_err);
		goto done;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_DEBUG(netlogon_creds_CredentialState, creds);
	}

	DEBUG(3,("schannel_fetch_session_key_tdb: restored schannel info key %s\n",
		keystr));

	status = NT_STATUS_OK;

 done:

	talloc_free(keystr);

	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(creds);
		return status;
	}

	*pcreds = creds;

	return NT_STATUS_OK;
}

/********************************************************************

  Validate an incoming authenticator against the credentials for the remote
  machine.

  The credentials are (re)read and from the schannel database, and
  written back after the caclulations are performed.

  The creds_out parameter (if not NULL) returns the credentials, if
  the caller needs some of that information.

 ********************************************************************/

NTSTATUS schannel_creds_server_step_check_tdb(struct tdb_context *tdb,
					      TALLOC_CTX *mem_ctx,
					      const char *computer_name,
					      bool schannel_required_for_call,
					      bool schannel_in_use,
					      struct netr_Authenticator *received_authenticator,
					      struct netr_Authenticator *return_authenticator,
					      struct netlogon_creds_CredentialState **creds_out)
{
	struct netlogon_creds_CredentialState *creds;
	NTSTATUS status;
	int ret;

	ret = tdb_transaction_start(tdb);
	if (ret != 0) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	/* Because this is a shared structure (even across
	 * disconnects) we must update the database every time we
	 * update the structure */

	status = schannel_fetch_session_key_tdb(tdb, mem_ctx, computer_name,
						&creds);

	/* If we are flaged that schannel is required for a call, and
	 * it is not in use, then make this an error */

	/* It would be good to make this mandatory once schannel is
	 * negotiated, but this is not what windows does */
	if (schannel_required_for_call && !schannel_in_use) {
		DEBUG(0,("schannel_creds_server_step_check_tdb: "
			"client %s not using schannel for netlogon, despite negotiating it\n",
			creds->computer_name ));
		tdb_transaction_cancel(tdb);
		return NT_STATUS_ACCESS_DENIED;
	}

	if (NT_STATUS_IS_OK(status)) {
		status = netlogon_creds_server_step_check(creds,
							  received_authenticator,
							  return_authenticator);
	}

	if (NT_STATUS_IS_OK(status)) {
		status = schannel_store_session_key_tdb(tdb, mem_ctx, creds);
	}

	if (NT_STATUS_IS_OK(status)) {
		tdb_transaction_commit(tdb);
		if (creds_out) {
			*creds_out = creds;
			talloc_steal(mem_ctx, creds);
		}
	} else {
		tdb_transaction_cancel(tdb);
	}

	return status;
}
