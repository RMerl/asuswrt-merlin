/*
 *  Unix SMB/CIFS implementation.
 *  Group Policy Support
 *  Copyright (C) Guenther Deschner 2007-2008,2010
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
#include "../libgpo/gpo_ini.h"
#include "../libgpo/gpo.h"
#include "libgpo/gpo_proto.h"
#include "registry.h"
#include "../librpc/gen_ndr/ndr_preg.h"

#define GP_EXT_NAME "registry"

/* more info can be found at:
 * http://msdn2.microsoft.com/en-us/library/aa374407.aspx */

#define GP_REGPOL_FILE  "Registry.pol"

#define GP_REGPOL_FILE_SIGNATURE 0x67655250 /* 'PReg' */
#define GP_REGPOL_FILE_VERSION 1

static TALLOC_CTX *ctx = NULL;

/****************************************************************
****************************************************************/

static bool reg_parse_value(TALLOC_CTX *mem_ctx,
			    const char **value,
			    enum gp_reg_action *action)
{
	if (!*value) {
		*action = GP_REG_ACTION_ADD_KEY;
		return true;
	}

	if (strncmp(*value, "**", 2) != 0) {
		*action = GP_REG_ACTION_ADD_VALUE;
		return true;
	}

	if (strnequal(*value, "**DelVals.", 10)) {
		*action = GP_REG_ACTION_DEL_ALL_VALUES;
		return true;
	}

	if (strnequal(*value, "**Del.", 6)) {
		*value = talloc_strdup(mem_ctx, *value + 6);
		*action = GP_REG_ACTION_DEL_VALUE;
		return true;
	}

	if (strnequal(*value, "**SecureKey", 11)) {
		if (strnequal(*value, "**SecureKey=1", 13)) {
			*action = GP_REG_ACTION_SEC_KEY_SET;
			return true;
		}

 /*************** not tested from here on ***************/
		if (strnequal(*value, "**SecureKey=0", 13)) {
			smb_panic("not supported: **SecureKey=0");
			*action = GP_REG_ACTION_SEC_KEY_RESET;
			return true;
		}
		DEBUG(0,("unknown: SecureKey: %s\n", *value));
		smb_panic("not supported SecureKey method");
		return false;
	}

	if (strnequal(*value, "**DeleteValues", strlen("**DeleteValues"))) {
		smb_panic("not supported: **DeleteValues");
		*action = GP_REG_ACTION_DEL_VALUES;
		return false;
	}

	if (strnequal(*value, "**DeleteKeys", strlen("**DeleteKeys"))) {
		smb_panic("not supported: **DeleteKeys");
		*action = GP_REG_ACTION_DEL_KEYS;
		return false;
	}

	DEBUG(0,("unknown value: %s\n", *value));
	smb_panic(*value);
	return false;
}

/****************************************************************
****************************************************************/

static bool gp_reg_entry_from_file_entry(TALLOC_CTX *mem_ctx,
					 struct preg_entry *r,
					 struct gp_registry_entry **reg_entry)
{
	struct registry_value *data = NULL;
	struct gp_registry_entry *entry = NULL;
	enum gp_reg_action action = GP_REG_ACTION_NONE;

	ZERO_STRUCTP(*reg_entry);

	data = TALLOC_ZERO_P(mem_ctx, struct registry_value);
	if (!data)
		return false;

	data->type = r->type;
	data->data = data_blob_talloc(data, r->data, r->size);

	entry = TALLOC_ZERO_P(mem_ctx, struct gp_registry_entry);
	if (!entry)
		return false;

	if (!reg_parse_value(mem_ctx, &r->valuename, &action))
		return false;

	entry->key = talloc_strdup(entry, r->keyname);
	entry->value = talloc_strdup(entry, r->valuename);
	entry->data = data;
	entry->action = action;

	*reg_entry = entry;

	return true;
}

/****************************************************************
****************************************************************/

static NTSTATUS reg_parse_registry(TALLOC_CTX *mem_ctx,
				   uint32_t flags,
				   const char *filename,
				   struct gp_registry_entry **entries_p,
				   size_t *num_entries_p)
{
	DATA_BLOB blob;
	NTSTATUS status;
	enum ndr_err_code ndr_err;
	const char *real_filename = NULL;
	struct preg_file r;
	struct gp_registry_entry *entries = NULL;
	size_t num_entries = 0;
	int i;

	status = gp_find_file(mem_ctx,
			      flags,
			      filename,
			      GP_REGPOL_FILE,
			      &real_filename);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	blob.data = (uint8_t *)file_load(real_filename, &blob.length, 0, NULL);
	if (!blob.data) {
		return NT_STATUS_CANNOT_LOAD_REGISTRY_FILE;
	}

	ndr_err = ndr_pull_struct_blob(&blob, mem_ctx, &r,
			(ndr_pull_flags_fn_t)ndr_pull_preg_file);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		status = ndr_map_error2ntstatus(ndr_err);
		goto out;
	}

	if (!strequal(r.header.signature, "PReg")) {
		status = NT_STATUS_INVALID_PARAMETER;
		goto out;
	}

	if (r.header.version != GP_REGPOL_FILE_VERSION) {
		status = NT_STATUS_INVALID_PARAMETER;
		goto out;
	}

	for (i=0; i < r.num_entries; i++) {

		struct gp_registry_entry *r_entry = NULL;

		if (!gp_reg_entry_from_file_entry(mem_ctx,
						  &r.entries[i],
						  &r_entry)) {
			status = NT_STATUS_NO_MEMORY;
			goto out;
		}

		if (!add_gp_registry_entry_to_array(mem_ctx,
						    r_entry,
						    &entries,
						    &num_entries)) {
			status = NT_STATUS_NO_MEMORY;
			goto out;
		}
	}

	*entries_p = entries;
	*num_entries_p = num_entries;

	status = NT_STATUS_OK;

 out:
	data_blob_free(&blob);
	return status;
}

/****************************************************************
****************************************************************/

static WERROR reg_apply_registry(TALLOC_CTX *mem_ctx,
				 const struct security_token *token,
				 struct registry_key *root_key,
				 uint32_t flags,
				 struct gp_registry_entry *entries,
				 size_t num_entries)
{
	struct gp_registry_context *reg_ctx = NULL;
	WERROR werr;
	size_t i;

	if (num_entries == 0) {
		return WERR_OK;
	}

#if 0
	if (flags & GPO_LIST_FLAG_MACHINE) {
		werr = gp_init_reg_ctx(mem_ctx, KEY_HKLM, REG_KEY_WRITE,
				       get_system_token(),
				       &reg_ctx);
	} else {
		werr = gp_init_reg_ctx(mem_ctx, KEY_HKCU, REG_KEY_WRITE,
				       token,
				       &reg_ctx);
	}
	W_ERROR_NOT_OK_RETURN(werr);
#endif
	for (i=0; i<num_entries; i++) {

		/* FIXME: maybe we should check here if we attempt to go beyond
		 * the 4 allowed reg keys */

		werr = reg_apply_registry_entry(mem_ctx, root_key,
						reg_ctx,
						&(entries)[i],
						token, flags);
		if (!W_ERROR_IS_OK(werr)) {
			DEBUG(0,("failed to apply registry: %s\n",
				win_errstr(werr)));
			goto done;
		}
	}

done:
	gp_free_reg_ctx(reg_ctx);
	return werr;
}


/****************************************************************
****************************************************************/

static NTSTATUS registry_process_group_policy(ADS_STRUCT *ads,
					      TALLOC_CTX *mem_ctx,
					      uint32_t flags,
					      struct registry_key *root_key,
					      const struct security_token *token,
					      struct GROUP_POLICY_OBJECT *gpo,
					      const char *extension_guid,
					      const char *snapin_guid)
{
	NTSTATUS status;
	WERROR werr;
	struct gp_registry_entry *entries = NULL;
	size_t num_entries = 0;
	char *unix_path = NULL;

	debug_gpext_header(0, "registry_process_group_policy", flags, gpo,
			   extension_guid, snapin_guid);

	status = gpo_get_unix_path(mem_ctx, cache_path(GPO_CACHE_DIR), gpo, &unix_path);
	NT_STATUS_NOT_OK_RETURN(status);

	status = reg_parse_registry(mem_ctx,
				    flags,
				    unix_path,
				    &entries,
				    &num_entries);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("failed to parse registry: %s\n",
			nt_errstr(status)));
		return status;
	}

	dump_reg_entries(flags, "READ", entries, num_entries);

	werr = reg_apply_registry(mem_ctx, token, root_key, flags,
				  entries, num_entries);
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(0,("failed to apply registry: %s\n",
			win_errstr(werr)));
		return werror_to_ntstatus(werr);
	}

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static NTSTATUS registry_get_reg_config(TALLOC_CTX *mem_ctx,
					struct gp_extension_reg_info **reg_info)
{
	NTSTATUS status;
	struct gp_extension_reg_info *info = NULL;
	struct gp_extension_reg_table table[] = {
		{ "ProcessGroupPolicy", REG_SZ, "registry_process_group_policy" },
		{ NULL, REG_NONE, NULL }
	};

	info = TALLOC_ZERO_P(mem_ctx, struct gp_extension_reg_info);
	NT_STATUS_HAVE_NO_MEMORY(info);

	status = gp_ext_info_add_entry(mem_ctx, GP_EXT_NAME,
				       GP_EXT_GUID_REGISTRY,
				       table, info);
	NT_STATUS_NOT_OK_RETURN(status);

	*reg_info = info;

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static NTSTATUS registry_initialize(TALLOC_CTX *mem_ctx)
{
	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static NTSTATUS registry_shutdown(void)
{
	NTSTATUS status;

	status = unregister_gp_extension(GP_EXT_NAME);
	if (NT_STATUS_IS_OK(status)) {
		return status;
	}

	TALLOC_FREE(ctx);

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static struct gp_extension_methods registry_methods = {
	.initialize		= registry_initialize,
	.process_group_policy	= registry_process_group_policy,
	.get_reg_config		= registry_get_reg_config,
	.shutdown		= registry_shutdown
};

/****************************************************************
****************************************************************/

NTSTATUS gpext_registry_init(void)
{
	NTSTATUS status;

	ctx = talloc_init("gpext_registry_init");
	NT_STATUS_HAVE_NO_MEMORY(ctx);

	status = register_gp_extension(ctx, SMB_GPEXT_INTERFACE_VERSION,
				       GP_EXT_NAME, GP_EXT_GUID_REGISTRY,
				       &registry_methods);
	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(ctx);
	}

	return status;
}
