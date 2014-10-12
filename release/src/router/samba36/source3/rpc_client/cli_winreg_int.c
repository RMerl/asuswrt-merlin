/*
 *  Unix SMB/CIFS implementation.
 *
 *  WINREG internal client routines
 *
 *  Copyright (c) 2011      Andreas Schneider <asn@samba.org>
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
#include "include/registry.h"
#include "librpc/gen_ndr/ndr_winreg_c.h"
#include "rpc_client/cli_winreg_int.h"
#include "rpc_server/rpc_ncacn_np.h"

/**
 * Split path into hive name and subkeyname
 * normalizations performed:
 *  - if the path contains no '\\' characters,
 *    assume that the legacy format of using '/'
 *    as a separator is used and  convert '/' to '\\'
 *  - strip trailing '\\' chars
 */
static WERROR _split_hive_key(TALLOC_CTX *mem_ctx,
			      const char *path,
			      char **hivename,
			      char **subkeyname)
{
	char *p;
	const char *tmp_subkeyname;

	if ((path == NULL) || (hivename == NULL) || (subkeyname == NULL)) {
		return WERR_INVALID_PARAM;
	}

	if (strlen(path) == 0) {
		return WERR_INVALID_PARAM;
	}

	if (strchr(path, '\\') == NULL) {
		*hivename = talloc_string_sub(mem_ctx, path, "/", "\\");
	} else {
		*hivename = talloc_strdup(mem_ctx, path);
	}

	if (*hivename == NULL) {
		return WERR_NOMEM;
	}

	/* strip trailing '\\' chars */
	p = strrchr(*hivename, '\\');
	while ((p != NULL) && (p[1] == '\0')) {
		*p = '\0';
		p = strrchr(*hivename, '\\');
	}

	p = strchr(*hivename, '\\');

	if ((p == NULL) || (*p == '\0')) {
		/* just the hive - no subkey given */
		tmp_subkeyname = "";
	} else {
		*p = '\0';
		tmp_subkeyname = p+1;
	}
	*subkeyname = talloc_strdup(mem_ctx, tmp_subkeyname);
	if (*subkeyname == NULL) {
		return WERR_NOMEM;
	}

	return WERR_OK;
}

static NTSTATUS _winreg_int_openkey(TALLOC_CTX *mem_ctx,
				    const struct auth_serversupplied_info *session_info,
				    struct messaging_context *msg_ctx,
				    struct dcerpc_binding_handle **h,
				    uint32_t reg_type,
				    const char *key,
				    bool create_key,
				    uint32_t access_mask,
				    struct policy_handle *hive_handle,
				    struct policy_handle *key_handle,
				    WERROR *pwerr)
{
	static struct client_address client_id;
	struct dcerpc_binding_handle *binding_handle;
	struct winreg_String wkey, wkeyclass;
	NTSTATUS status;
	WERROR result = WERR_OK;

	strlcpy(client_id.addr, "127.0.0.1", sizeof(client_id.addr));
	client_id.name = "127.0.0.1";

	status = rpcint_binding_handle(mem_ctx,
				       &ndr_table_winreg,
				       &client_id,
				       session_info,
				       msg_ctx,
				       &binding_handle);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("dcerpc_winreg_int_openkey: Could not connect to winreg pipe: %s\n",
			  nt_errstr(status)));
		return status;
	}

	switch (reg_type) {
	case HKEY_LOCAL_MACHINE:
		status = dcerpc_winreg_OpenHKLM(binding_handle,
						mem_ctx,
						NULL,
						access_mask,
						hive_handle,
						&result);
		break;
	case HKEY_CLASSES_ROOT:
		status = dcerpc_winreg_OpenHKCR(binding_handle,
						mem_ctx,
						NULL,
						access_mask,
						hive_handle,
						&result);
		break;
	case HKEY_USERS:
		status = dcerpc_winreg_OpenHKU(binding_handle,
					       mem_ctx,
					       NULL,
					       access_mask,
					       hive_handle,
					       &result);
		break;
	case HKEY_CURRENT_USER:
		status = dcerpc_winreg_OpenHKCU(binding_handle,
						mem_ctx,
						NULL,
						access_mask,
						hive_handle,
						&result);
		break;
	case HKEY_PERFORMANCE_DATA:
		status = dcerpc_winreg_OpenHKPD(binding_handle,
						mem_ctx,
						NULL,
						access_mask,
						hive_handle,
						&result);
		break;
	default:
		result = WERR_INVALID_PARAMETER;
		status = NT_STATUS_OK;
	}
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(binding_handle);
		return status;
	}
	if (!W_ERROR_IS_OK(result)) {
		talloc_free(binding_handle);
		*pwerr = result;
		return status;
	}

	ZERO_STRUCT(wkey);
	wkey.name = key;

	if (create_key) {
		enum winreg_CreateAction action = REG_ACTION_NONE;

		ZERO_STRUCT(wkeyclass);
		wkeyclass.name = "";

		status = dcerpc_winreg_CreateKey(binding_handle,
						 mem_ctx,
						 hive_handle,
						 wkey,
						 wkeyclass,
						 0,
						 access_mask,
						 NULL,
						 key_handle,
						 &action,
						 &result);
		switch (action) {
			case REG_ACTION_NONE:
				DEBUG(8, ("dcerpc_winreg_int_openkey: createkey"
					  " did nothing -- huh?\n"));
				break;
			case REG_CREATED_NEW_KEY:
				DEBUG(8, ("dcerpc_winreg_int_openkey: createkey"
					  " created %s\n", key));
				break;
			case REG_OPENED_EXISTING_KEY:
				DEBUG(8, ("dcerpc_winreg_int_openkey: createkey"
					  " opened existing %s\n", key));
				break;
		}
	} else {
		status = dcerpc_winreg_OpenKey(binding_handle,
					       mem_ctx,
					       hive_handle,
					       wkey,
					       0,
					       access_mask,
					       key_handle,
					       &result);
	}
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(binding_handle);
		return status;
	}
	if (!W_ERROR_IS_OK(result)) {
		talloc_free(binding_handle);
		*pwerr = result;
		return status;
	}

	*h = binding_handle;

	return status;
}

NTSTATUS dcerpc_winreg_int_openkey(TALLOC_CTX *mem_ctx,
				   const struct auth_serversupplied_info *server_info,
				   struct messaging_context *msg_ctx,
				   struct dcerpc_binding_handle **h,
				   const char *key,
				   bool create_key,
				   uint32_t access_mask,
				   struct policy_handle *hive_handle,
				   struct policy_handle *key_handle,
				   WERROR *pwerr)
{
	char *hivename = NULL;
	char *subkey = NULL;
	uint32_t reg_type;
	WERROR result;

	result = _split_hive_key(mem_ctx, key, &hivename, &subkey);
	if (!W_ERROR_IS_OK(result)) {
		*pwerr = result;
		return NT_STATUS_OK;
	}

	if (strequal(hivename, "HKLM") ||
	    strequal(hivename, "HKEY_LOCAL_MACHINE")) {
		reg_type = HKEY_LOCAL_MACHINE;
	} else if (strequal(hivename, "HKCR") ||
		   strequal(hivename, "HKEY_CLASSES_ROOT")) {
		reg_type = HKEY_CLASSES_ROOT;
	} else if (strequal(hivename, "HKU") ||
		   strequal(hivename, "HKEY_USERS")) {
		reg_type = HKEY_USERS;
	} else if (strequal(hivename, "HKCU") ||
		   strequal(hivename, "HKEY_CURRENT_USER")) {
		reg_type = HKEY_CURRENT_USER;
	} else if (strequal(hivename, "HKPD") ||
		   strequal(hivename, "HKEY_PERFORMANCE_DATA")) {
		reg_type = HKEY_PERFORMANCE_DATA;
	} else {
		DEBUG(10,("dcerpc_winreg_int_openkey: unrecognised hive key %s\n",
			  key));
		*pwerr = WERR_INVALID_PARAMETER;
		return NT_STATUS_OK;
	}

	return _winreg_int_openkey(mem_ctx,
				   server_info,
				   msg_ctx,
				   h,
				   reg_type,
				   key,
				   create_key,
				   access_mask,
				   hive_handle,
				   key_handle,
				   pwerr);
}

NTSTATUS dcerpc_winreg_int_hklm_openkey(TALLOC_CTX *mem_ctx,
					const struct auth_serversupplied_info *server_info,
					struct messaging_context *msg_ctx,
					struct dcerpc_binding_handle **h,
					const char *key,
					bool create_key,
					uint32_t access_mask,
					struct policy_handle *hive_handle,
					struct policy_handle *key_handle,
					WERROR *pwerr)
{
	return _winreg_int_openkey(mem_ctx,
				   server_info,
				   msg_ctx,
				   h,
				   HKEY_LOCAL_MACHINE,
				   key,
				   create_key,
				   access_mask,
				   hive_handle,
				   key_handle,
				   pwerr);
}

/* vim: set ts=8 sw=8 noet cindent syntax=c.doxygen: */
