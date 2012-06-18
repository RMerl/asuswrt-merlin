/*
   Unix SMB/CIFS implementation.

   module to store/fetch session keys for the schannel server

   Copyright (C) Andrew Tridgell 2004
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2006-2009

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
#include "lib/ldb/include/ldb.h"
#include "librpc/gen_ndr/ndr_security.h"
#include "ldb_wrap.h"
#include "../lib/util/util_ldb.h"
#include "libcli/auth/libcli_auth.h"
#include "auth/auth.h"
#include "param/param.h"
#include "auth/gensec/schannel_state.h"
#include "../libcli/auth/schannel_state_proto.h"

static struct ldb_val *schannel_dom_sid_ldb_val(TALLOC_CTX *mem_ctx,
						struct dom_sid *sid)
{
	enum ndr_err_code ndr_err;
	struct ldb_val *v;

	v = talloc(mem_ctx, struct ldb_val);
	if (!v) return NULL;

	ndr_err = ndr_push_struct_blob(v, mem_ctx, NULL, sid,
				       (ndr_push_flags_fn_t)ndr_push_dom_sid);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(v);
		return NULL;
	}

	return v;
}

static struct dom_sid *schannel_ldb_val_dom_sid(TALLOC_CTX *mem_ctx,
						 const struct ldb_val *v)
{
	enum ndr_err_code ndr_err;
	struct dom_sid *sid;

	sid = talloc(mem_ctx, struct dom_sid);
	if (!sid) return NULL;

	ndr_err = ndr_pull_struct_blob(v, sid, NULL, sid,
					(ndr_pull_flags_fn_t)ndr_pull_dom_sid);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(sid);
		return NULL;
	}
	return sid;
}


/*
  remember an established session key for a netr server authentication
  use a simple ldb structure
*/
NTSTATUS schannel_store_session_key_ldb(struct ldb_context *ldb,
					TALLOC_CTX *mem_ctx,
					struct netlogon_creds_CredentialState *creds)
{
	struct ldb_message *msg;
	struct ldb_val val, seed, client_state, server_state;
	struct ldb_val *sid_val;
	char *f;
	char *sct;
	int ret;

	f = talloc_asprintf(mem_ctx, "%u", (unsigned int)creds->negotiate_flags);

	if (f == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	sct = talloc_asprintf(mem_ctx, "%u", (unsigned int)creds->secure_channel_type);

	if (sct == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	msg = ldb_msg_new(ldb);
	if (msg == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	msg->dn = ldb_dn_new_fmt(msg, ldb, "computerName=%s", creds->computer_name);
	if ( ! msg->dn) {
		return NT_STATUS_NO_MEMORY;
	}

	sid_val = schannel_dom_sid_ldb_val(msg, creds->sid);
	if (sid_val == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	val.data = creds->session_key;
	val.length = sizeof(creds->session_key);

	seed.data = creds->seed.data;
	seed.length = sizeof(creds->seed.data);

	client_state.data = creds->client.data;
	client_state.length = sizeof(creds->client.data);
	server_state.data = creds->server.data;
	server_state.length = sizeof(creds->server.data);

	ldb_msg_add_string(msg, "objectClass", "schannelState");
	ldb_msg_add_value(msg, "sessionKey", &val, NULL);
	ldb_msg_add_value(msg, "seed", &seed, NULL);
	ldb_msg_add_value(msg, "clientState", &client_state, NULL);
	ldb_msg_add_value(msg, "serverState", &server_state, NULL);
	ldb_msg_add_string(msg, "negotiateFlags", f);
	ldb_msg_add_string(msg, "secureChannelType", sct);
	ldb_msg_add_string(msg, "accountName", creds->account_name);
	ldb_msg_add_string(msg, "computerName", creds->computer_name);
	ldb_msg_add_value(msg, "objectSid", sid_val, NULL);

	ret = ldb_add(ldb, msg);
	if (ret == LDB_ERR_ENTRY_ALREADY_EXISTS) {
		int i;
		/* from samdb_replace() */
		/* mark all the message elements as LDB_FLAG_MOD_REPLACE */
		for (i=0;i<msg->num_elements;i++) {
			msg->elements[i].flags = LDB_FLAG_MOD_REPLACE;
		}

		ret = ldb_modify(ldb, msg);
	}

	/* We don't need a transaction here, as we either add or
	 * modify records, never delete them, so it must exist */

	if (ret != LDB_SUCCESS) {
		DEBUG(0,("Unable to add %s to session key db - %s\n",
			 ldb_dn_get_linearized(msg->dn), ldb_errstring(ldb)));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	return NT_STATUS_OK;
}

/*
  read back a credentials back for a computer
*/
NTSTATUS schannel_fetch_session_key_ldb(struct ldb_context *ldb,
					TALLOC_CTX *mem_ctx,
					const char *computer_name,
					struct netlogon_creds_CredentialState **creds)
{
	struct ldb_result *res;
	int ret;
	const struct ldb_val *val;

	*creds = talloc_zero(mem_ctx, struct netlogon_creds_CredentialState);
	if (!*creds) {
		return NT_STATUS_NO_MEMORY;
	}

	ret = ldb_search(ldb, mem_ctx, &res,
				 NULL, LDB_SCOPE_SUBTREE, NULL,
				"(computerName=%s)", computer_name);
	if (ret != LDB_SUCCESS) {
		DEBUG(3,("schannel: Failed to find a record for client %s: %s\n", computer_name, ldb_errstring(ldb)));
		return NT_STATUS_INVALID_HANDLE;
	}
	if (res->count != 1) {
		DEBUG(3,("schannel: Failed to find a record for client: %s (found %d records)\n", computer_name, res->count));
		talloc_free(res);
		return NT_STATUS_INVALID_HANDLE;
	}

	val = ldb_msg_find_ldb_val(res->msgs[0], "sessionKey");
	if (val == NULL || val->length != 16) {
		DEBUG(1,("schannel: record in schannel DB must contain a sessionKey of length 16, when searching for client: %s\n", computer_name));
		talloc_free(res);
		return NT_STATUS_INTERNAL_ERROR;
	}

	memcpy((*creds)->session_key, val->data, 16);

	val = ldb_msg_find_ldb_val(res->msgs[0], "seed");
	if (val == NULL || val->length != 8) {
		DEBUG(1,("schannel: record in schannel DB must contain a vaid seed of length 8, when searching for client: %s\n", computer_name));
		talloc_free(res);
		return NT_STATUS_INTERNAL_ERROR;
	}

	memcpy((*creds)->seed.data, val->data, 8);

	val = ldb_msg_find_ldb_val(res->msgs[0], "clientState");
	if (val == NULL || val->length != 8) {
		DEBUG(1,("schannel: record in schannel DB must contain a vaid clientState of length 8, when searching for client: %s\n", computer_name));
		talloc_free(res);
		return NT_STATUS_INTERNAL_ERROR;
	}
	memcpy((*creds)->client.data, val->data, 8);

	val = ldb_msg_find_ldb_val(res->msgs[0], "serverState");
	if (val == NULL || val->length != 8) {
		DEBUG(1,("schannel: record in schannel DB must contain a vaid serverState of length 8, when searching for client: %s\n", computer_name));
		talloc_free(res);
		return NT_STATUS_INTERNAL_ERROR;
	}
	memcpy((*creds)->server.data, val->data, 8);

	(*creds)->negotiate_flags = ldb_msg_find_attr_as_int(res->msgs[0], "negotiateFlags", 0);

	(*creds)->secure_channel_type = ldb_msg_find_attr_as_int(res->msgs[0], "secureChannelType", 0);

	(*creds)->account_name = talloc_strdup(*creds, ldb_msg_find_attr_as_string(res->msgs[0], "accountName", NULL));
	if ((*creds)->account_name == NULL) {
		talloc_free(res);
		return NT_STATUS_NO_MEMORY;
	}

	(*creds)->computer_name = talloc_strdup(*creds, ldb_msg_find_attr_as_string(res->msgs[0], "computerName", NULL));
	if ((*creds)->computer_name == NULL) {
		talloc_free(res);
		return NT_STATUS_NO_MEMORY;
	}

	val = ldb_msg_find_ldb_val(res->msgs[0], "objectSid");
	if (val) {
		(*creds)->sid = schannel_ldb_val_dom_sid(*creds, val);
		if ((*creds)->sid == NULL) {
			talloc_free(res);
			return NT_STATUS_INTERNAL_ERROR;
		}
	} else {
		(*creds)->sid = NULL;
	}

	talloc_free(res);
	return NT_STATUS_OK;
}

/*
  Validate an incoming authenticator against the credentials for the remote machine.

  The credentials are (re)read and from the schannel database, and
  written back after the caclulations are performed.

  The creds_out parameter (if not NULL) returns the credentials, if
  the caller needs some of that information.

*/
NTSTATUS schannel_creds_server_step_check_ldb(struct ldb_context *ldb,
					      TALLOC_CTX *mem_ctx,
					      const char *computer_name,
					      bool schannel_required_for_call,
					      bool schannel_in_use,
					      struct netr_Authenticator *received_authenticator,
					      struct netr_Authenticator *return_authenticator,
					      struct netlogon_creds_CredentialState **creds_out)
{
	struct netlogon_creds_CredentialState *creds;
	NTSTATUS nt_status;
	int ret;

	ret = ldb_transaction_start(ldb);
	if (ret != 0) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	/* Because this is a shared structure (even across
	 * disconnects) we must update the database every time we
	 * update the structure */

	nt_status = schannel_fetch_session_key_ldb(ldb, ldb, computer_name,
						   &creds);

	/* If we are flaged that schannel is required for a call, and
	 * it is not in use, then make this an error */

	/* It would be good to make this mandetory once schannel is
	 * negoiated, bu this is not what windows does */
	if (schannel_required_for_call && !schannel_in_use) {
		DEBUG(0,("schannel_creds_server_step_check: client %s not using schannel for netlogon, despite negotiating it\n",
			creds->computer_name ));
		ldb_transaction_cancel(ldb);
		return NT_STATUS_ACCESS_DENIED;
	}

	if (NT_STATUS_IS_OK(nt_status)) {
		nt_status = netlogon_creds_server_step_check(creds,
							     received_authenticator,
							     return_authenticator);
	}

	if (NT_STATUS_IS_OK(nt_status)) {
		nt_status = schannel_store_session_key_ldb(ldb, mem_ctx, creds);
	}

	if (NT_STATUS_IS_OK(nt_status)) {
		ldb_transaction_commit(ldb);
		if (creds_out) {
			*creds_out = creds;
			talloc_steal(mem_ctx, creds);
		}
	} else {
		ldb_transaction_cancel(ldb);
	}
	return nt_status;
}
