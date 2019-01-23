/*
 *  Unix SMB/CIFS implementation.
 *
 *  Eventlog RPC server keys initialization
 *
 *  Copyright (c) 2005      Marcin Krzysztof Porwit
 *  Copyright (c) 2005      Brian Moran
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
#include "../librpc/gen_ndr/ndr_winreg_c.h"
#include "rpc_client/cli_winreg_int.h"
#include "rpc_client/cli_winreg.h"
#include "rpc_server/eventlog/srv_eventlog_reg.h"
#include "auth.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_REGISTRY

#define TOP_LEVEL_EVENTLOG_KEY "SYSTEM\\CurrentControlSet\\Services\\Eventlog"

bool eventlog_init_winreg(struct messaging_context *msg_ctx)
{
	struct dcerpc_binding_handle *h = NULL;
	uint32_t access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	struct policy_handle hive_hnd, key_hnd;
	uint32_t uiMaxSize = 0x00080000;
	uint32_t uiRetention = 0x93A80;
	const char **elogs = lp_eventlog_list();
	const char **subkeys = NULL;
	uint32_t num_subkeys = 0;
	uint32_t i;
	char *key = NULL;
	NTSTATUS status;
	WERROR result = WERR_OK;
	bool ok = false;
	TALLOC_CTX *tmp_ctx;

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		return false;
	}

	DEBUG(3, ("Initialise the eventlog registry keys if needed.\n"));

	key = talloc_strdup(tmp_ctx, TOP_LEVEL_EVENTLOG_KEY);

	status = dcerpc_winreg_int_hklm_openkey(tmp_ctx,
						get_session_info_system(),
						msg_ctx,
						&h,
						key,
						false,
						access_mask,
						&hive_hnd,
						&key_hnd,
						&result);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("eventlog_init_winreg: Could not open %s - %s\n",
			  key, nt_errstr(status)));
		goto done;
	}
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(0, ("eventlog_init_winreg: Could not open %s - %s\n",
			  key, win_errstr(result)));
		goto done;
	}

	status = dcerpc_winreg_enum_keys(tmp_ctx,
					 h,
					 &key_hnd,
					 &num_subkeys,
					 &subkeys,
					 &result);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("eventlog_init_winreg: Could enum keys at %s - %s\n",
			  key, nt_errstr(status)));
		goto done;
	}
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(0, ("eventlog_init_winreg: Could enum keys at %s - %s\n",
			  key, win_errstr(result)));
		goto done;
	}

	if (is_valid_policy_hnd(&key_hnd)) {
		dcerpc_winreg_CloseKey(h, tmp_ctx, &key_hnd, &result);
	}

	/* create subkeys if they don't exist */
	while (elogs && *elogs) {
		enum winreg_CreateAction action = REG_ACTION_NONE;
		char *evt_tdb = NULL;
		struct winreg_String wkey;
		struct winreg_String wkeyclass;
		bool skip = false;

		for (i = 0; i < num_subkeys; i++) {
			if (strequal(subkeys[i], *elogs)) {
				skip = true;
			}
		}

		if (skip) {
			elogs++;
			continue;
		}

		ZERO_STRUCT(key_hnd);
		ZERO_STRUCT(wkey);

		wkey.name = talloc_asprintf(tmp_ctx, "%s\\%s", key, *elogs);
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
			DEBUG(0, ("eventlog_init_winreg_keys: Could not create key %s: %s\n",
				wkey.name, nt_errstr(status)));
			goto done;
		}
		if (!W_ERROR_IS_OK(result)) {
			DEBUG(0, ("eventlog_init_winreg_keys: Could not create key %s: %s\n",
				wkey.name, win_errstr(result)));
			goto done;
		}

		status = dcerpc_winreg_set_dword(tmp_ctx,
						 h,
						 &key_hnd,
						 "MaxSize",
						 uiMaxSize,
						 &result);

		status = dcerpc_winreg_set_dword(tmp_ctx,
						 h,
						 &key_hnd,
						 "Retention",
						 uiRetention,
						 &result);

		status = dcerpc_winreg_set_sz(tmp_ctx,
					      h,
					      &key_hnd,
					      "PrimaryModule",
					      *elogs,
					      &result);

		evt_tdb = talloc_asprintf(tmp_ctx,
					  "%%SystemRoot%%\\system32\\config\\%s.tdb",
					  *elogs);
		if (evt_tdb == NULL) {
			goto done;
		}
		status = dcerpc_winreg_set_expand_sz(tmp_ctx,
						     h,
						     &key_hnd,
						     "File",
						     evt_tdb,
						     &result);
		TALLOC_FREE(evt_tdb);

		status = dcerpc_winreg_add_multi_sz(tmp_ctx,
						    h,
						    &key_hnd,
						    "Sources",
						    *elogs,
						    &result);

		if (is_valid_policy_hnd(&key_hnd)) {
			dcerpc_winreg_CloseKey(h, tmp_ctx, &key_hnd, &result);
		}

		/* sub-subkeys */
		{
			uint32_t uiCategoryCount = 0x00000007;

			wkey.name = talloc_asprintf(tmp_ctx,
						    "%s\\%s",
						    wkey.name, *elogs);
			if (wkey.name == NULL) {
				result = WERR_NOMEM;
				goto done;
			}

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
				DEBUG(0, ("eventlog_init_winreg_keys: Could not create key %s: %s\n",
					wkey.name, nt_errstr(status)));
				goto done;
			}
			if (!W_ERROR_IS_OK(result)) {
				DEBUG(0, ("eventlog_init_winreg_keys: Could not create key %s: %s\n",
					wkey.name, win_errstr(result)));
				goto done;
			}

			status = dcerpc_winreg_set_dword(tmp_ctx,
							 h,
							 &key_hnd,
							 "CategoryCount",
							 uiCategoryCount,
							 &result);

			status = dcerpc_winreg_set_expand_sz(tmp_ctx,
							     h,
							     &key_hnd,
							     "CategoryMessageFile",
							     "%SystemRoot%\\system32\\eventlog.dll",
							     &result);

			if (is_valid_policy_hnd(&key_hnd)) {
				dcerpc_winreg_CloseKey(h, tmp_ctx, &key_hnd, &result);
			}
		}

		elogs++;
	} /* loop */

	ok = true;
done:
	TALLOC_FREE(tmp_ctx);
	return ok;
}

/* vim: set ts=8 sw=8 noet cindent syntax=c.doxygen: */
