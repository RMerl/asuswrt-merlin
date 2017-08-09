/*
 *  Unix SMB/CIFS implementation.
 *
 *  SVC winreg glue
 *
 *  Copyright (c) 2005      Marcin Krzysztof Porwit
 *  Copyright (c) 2005      Gerald (Jerry) Carter
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
#include "services/services.h"
#include "services/svc_winreg_glue.h"
#include "rpc_client/cli_winreg_int.h"
#include "rpc_client/cli_winreg.h"
#include "../librpc/gen_ndr/ndr_winreg_c.h"
#include "../libcli/security/security.h"

#define TOP_LEVEL_SERVICES_KEY "SYSTEM\\CurrentControlSet\\Services"

struct security_descriptor* svcctl_gen_service_sd(TALLOC_CTX *mem_ctx)
{
	struct security_descriptor *sd = NULL;
	struct security_acl *theacl = NULL;
	struct security_ace ace[4];
	size_t sd_size;
	size_t i = 0;

	/* Basic access for everyone */
	init_sec_ace(&ace[i++], &global_sid_World,
		SEC_ACE_TYPE_ACCESS_ALLOWED, SERVICE_READ_ACCESS, 0);

	init_sec_ace(&ace[i++], &global_sid_Builtin_Power_Users,
			SEC_ACE_TYPE_ACCESS_ALLOWED, SERVICE_EXECUTE_ACCESS, 0);

	init_sec_ace(&ace[i++], &global_sid_Builtin_Server_Operators,
		SEC_ACE_TYPE_ACCESS_ALLOWED, SERVICE_ALL_ACCESS, 0);
	init_sec_ace(&ace[i++], &global_sid_Builtin_Administrators,
		SEC_ACE_TYPE_ACCESS_ALLOWED, SERVICE_ALL_ACCESS, 0);

	/* Create the security descriptor */
	theacl = make_sec_acl(mem_ctx,
			      NT4_ACL_REVISION,
			      i,
			      ace);
	if (theacl == NULL) {
		return NULL;
	}

	sd = make_sec_desc(mem_ctx,
			   SECURITY_DESCRIPTOR_REVISION_1,
			   SEC_DESC_SELF_RELATIVE,
			   NULL,
			   NULL,
			   NULL,
			   theacl,
			   &sd_size);
	if (sd == NULL) {
		return NULL;
	}

	return sd;
}

struct security_descriptor *svcctl_get_secdesc(TALLOC_CTX *mem_ctx,
					       struct messaging_context *msg_ctx,
					       const struct auth_serversupplied_info *session_info,
					       const char *name)
{
	struct dcerpc_binding_handle *h = NULL;
	uint32_t access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	struct policy_handle hive_hnd, key_hnd;
	struct security_descriptor *sd = NULL;
	char *key = NULL;
	NTSTATUS status;
	WERROR result = WERR_OK;

#ifndef WINREG_SUPPORT
	return NULL;
#endif

	key = talloc_asprintf(mem_ctx,
			      "%s\\%s\\Security",
			      TOP_LEVEL_SERVICES_KEY, name);
	if (key == NULL) {
		return NULL;
	}

	status = dcerpc_winreg_int_hklm_openkey(mem_ctx,
						session_info,
						msg_ctx,
						&h,
						key,
						false,
						access_mask,
						&hive_hnd,
						&key_hnd,
						&result);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(2, ("svcctl_set_secdesc: Could not open %s - %s\n",
			  key, nt_errstr(status)));
		return NULL;
	}
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(2, ("svcctl_set_secdesc: Could not open %s - %s\n",
			  key, win_errstr(result)));
		return NULL;
	}

	status = dcerpc_winreg_query_sd(mem_ctx,
					h,
					&key_hnd,
					"Security",
					&sd,
					&result);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(2, ("svcctl_get_secdesc: error getting value 'Security': "
			  "%s\n", nt_errstr(status)));
		return NULL;
	}
	if (W_ERROR_EQUAL(result, WERR_BADFILE)) {
		goto fallback_to_default_sd;
	} else if (!W_ERROR_IS_OK(result)) {
		DEBUG(2, ("svcctl_get_secdesc: error getting value 'Security': "
			  "%s\n", win_errstr(result)));
		return NULL;
	}

	goto done;

fallback_to_default_sd:
	DEBUG(6, ("svcctl_get_secdesc: constructing default secdesc for "
		  "service [%s]\n", name));
	sd = svcctl_gen_service_sd(mem_ctx);

done:
	return sd;
}

bool svcctl_set_secdesc(struct messaging_context *msg_ctx,
			const struct auth_serversupplied_info *session_info,
			const char *name,
			struct security_descriptor *sd)
{
	struct dcerpc_binding_handle *h = NULL;
	uint32_t access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	struct policy_handle hive_hnd;
	struct policy_handle key_hnd = { 0, };
	char *key = NULL;
	bool ok = false;
	TALLOC_CTX *tmp_ctx;
	NTSTATUS status;
	WERROR result = WERR_OK;

#ifndef WINREG_SUPPORT
	return false;
#endif

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		return false;
	}

	key = talloc_asprintf(tmp_ctx, "%s\\%s", TOP_LEVEL_SERVICES_KEY, name);
	if (key == NULL) {
		goto done;
	}

	status = dcerpc_winreg_int_hklm_openkey(tmp_ctx,
						session_info,
						msg_ctx,
						&h,
						key,
						false,
						access_mask,
						&hive_hnd,
						&key_hnd,
						&result);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("svcctl_set_secdesc: Could not open %s - %s\n",
			  key, nt_errstr(status)));
		goto done;
	}
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(0, ("svcctl_set_secdesc: Could not open %s - %s\n",
			  key, win_errstr(result)));
		goto done;
	}

	if (is_valid_policy_hnd(&key_hnd)) {
		dcerpc_winreg_CloseKey(h, tmp_ctx, &key_hnd, &result);
	}

	{
		enum winreg_CreateAction action = REG_ACTION_NONE;
		struct winreg_String wkey = { 0, };
		struct winreg_String wkeyclass;

		wkey.name = talloc_asprintf(tmp_ctx, "%s\\Security", key);
		if (wkey.name == NULL) {
			result = WERR_NOMEM;
			goto done;
		}

		ZERO_STRUCT(wkeyclass);
		wkeyclass.name = "";

		status = dcerpc_winreg_CreateKey(h,
						 tmp_ctx,
						 &hive_hnd,
						 wkey,
						 wkeyclass,
						 0,
						 access_mask,
						 NULL,
						 &key_hnd,
						 &action,
						 &result);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(2, ("svcctl_set_secdesc: Could not create key %s: %s\n",
				wkey.name, nt_errstr(status)));
			goto done;
		}
		if (!W_ERROR_IS_OK(result)) {
			DEBUG(2, ("svcctl_set_secdesc: Could not create key %s: %s\n",
				wkey.name, win_errstr(result)));
			goto done;
		}

		status = dcerpc_winreg_set_sd(tmp_ctx,
					      h,
					      &key_hnd,
					      "Security",
					      sd,
					      &result);
		if (!NT_STATUS_IS_OK(status)) {
			goto done;
		}
		if (!W_ERROR_IS_OK(result)) {
			goto done;
		}
	}

	ok = true;

done:
	if (is_valid_policy_hnd(&key_hnd)) {
		dcerpc_winreg_CloseKey(h, tmp_ctx, &key_hnd, &result);
	}

	talloc_free(tmp_ctx);
	return ok;
}

const char *svcctl_get_string_value(TALLOC_CTX *mem_ctx,
				    struct messaging_context *msg_ctx,
				    const struct auth_serversupplied_info *session_info,
				    const char *key_name,
				    const char *value_name)
{
	struct dcerpc_binding_handle *h = NULL;
	uint32_t access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	struct policy_handle hive_hnd, key_hnd;
	const char *data = NULL;
	char *path = NULL;
	TALLOC_CTX *tmp_ctx;
	NTSTATUS status;
	WERROR result = WERR_OK;

#ifndef WINREG_SUPPORT
	return NULL;
#endif

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		return NULL;
	}

	path = talloc_asprintf(tmp_ctx, "%s\\%s",
					TOP_LEVEL_SERVICES_KEY, key_name);
	if (path == NULL) {
		goto done;
	}

	status = dcerpc_winreg_int_hklm_openkey(tmp_ctx,
						session_info,
						msg_ctx,
						&h,
						path,
						false,
						access_mask,
						&hive_hnd,
						&key_hnd,
						&result);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(2, ("svcctl_get_string_value: Could not open %s - %s\n",
			  path, nt_errstr(status)));
		goto done;
	}
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(2, ("svcctl_get_string_value: Could not open %s - %s\n",
			  path, win_errstr(result)));
		goto done;
	}

	status = dcerpc_winreg_query_sz(mem_ctx,
					h,
					&key_hnd,
					value_name,
					&data,
					&result);

done:
	talloc_free(tmp_ctx);
	return data;
}

/********************************************************************
********************************************************************/

const char *svcctl_lookup_dispname(TALLOC_CTX *mem_ctx,
				   struct messaging_context *msg_ctx,
				   const struct auth_serversupplied_info *session_info,
				   const char *name)
{
	const char *display_name = NULL;

	display_name = svcctl_get_string_value(mem_ctx,
					       msg_ctx,
					       session_info,
					       name,
					       "DisplayName");

	if (display_name == NULL) {
		display_name = talloc_strdup(mem_ctx, name);
	}

	return display_name;
}

/********************************************************************
********************************************************************/

const char *svcctl_lookup_description(TALLOC_CTX *mem_ctx,
				      struct messaging_context *msg_ctx,
				      const struct auth_serversupplied_info *session_info,
				      const char *name)
{
	const char *description = NULL;

	description = svcctl_get_string_value(mem_ctx,
					      msg_ctx,
					      session_info,
					      name,
					      "Description");

	if (description == NULL) {
		description = talloc_strdup(mem_ctx, "Unix Service");
	}

	return description;
}

/* vim: set ts=8 sw=8 noet cindent syntax=c.doxygen: */
