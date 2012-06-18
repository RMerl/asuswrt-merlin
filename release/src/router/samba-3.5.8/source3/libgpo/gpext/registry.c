/*
 *  Unix SMB/CIFS implementation.
 *  Group Policy Support
 *  Copyright (C) Guenther Deschner 2007-2008
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

#define GP_EXT_NAME "registry"

/* more info can be found at:
 * http://msdn2.microsoft.com/en-us/library/aa374407.aspx */

#define GP_REGPOL_FILE  "Registry.pol"

#define GP_REGPOL_FILE_SIGNATURE 0x67655250 /* 'PReg' */
#define GP_REGPOL_FILE_VERSION 1

static TALLOC_CTX *ctx = NULL;

struct gp_registry_file_header {
	uint32_t signature;
	uint32_t version;
};

struct gp_registry_file_entry {
	UNISTR key;
	UNISTR value;
	enum winreg_Type type;
	size_t size;
	uint8_t *data;
};

struct gp_registry_file {
	struct gp_registry_file_header header;
	size_t num_entries;
	struct gp_registry_entry *entries;
};

/****************************************************************
****************************************************************/

static bool reg_parse_header(const char *desc,
			     struct gp_registry_file_header *header,
			     prs_struct *ps,
			     int depth)
{
	if (!header)
		return false;

	prs_debug(ps, depth, desc, "reg_parse_header");
	depth++;

	if (!prs_uint32("signature", ps, depth, &header->signature))
		return false;

	if (!prs_uint32("version", ps, depth, &header->version))
		return false;

	return true;
}

/****************************************************************
****************************************************************/

static bool reg_parse_and_verify_ucs2_char(const char *desc,
					   char character,
					   prs_struct *ps,
					   int depth)
{
	uint16_t tmp;

	if (!prs_uint16(desc, ps, depth, &tmp))
		return false;

	if (tmp != UCS2_CHAR(character))
		return false;

	return true;
}

/****************************************************************
****************************************************************/

static bool reg_parse_init(prs_struct *ps, int depth)
{
	return reg_parse_and_verify_ucs2_char("initiator '['", '[',
					      ps, depth);
}

/****************************************************************
****************************************************************/

static bool reg_parse_sep(prs_struct *ps, int depth)
{
	return reg_parse_and_verify_ucs2_char("separator ';'", ';',
					      ps, depth);
}

/****************************************************************
****************************************************************/

static bool reg_parse_term(prs_struct *ps, int depth)
{
	return reg_parse_and_verify_ucs2_char("terminator ']'", ']',
					      ps, depth);
}


/****************************************************************
* [key;value;type;size;data]
****************************************************************/

static bool reg_parse_entry(TALLOC_CTX *mem_ctx,
			    const char *desc,
			    struct gp_registry_file_entry *entry,
			    prs_struct *ps,
			    int depth)
{
	uint32_t size = 0;

	if (!entry)
		return false;

	prs_debug(ps, depth, desc, "reg_parse_entry");
	depth++;

	ZERO_STRUCTP(entry);

	if (!reg_parse_init(ps, depth))
		return false;

	if (!prs_unistr("key", ps, depth, &entry->key))
		return false;

	if (!reg_parse_sep(ps, depth))
		return false;

	if (!prs_unistr("value", ps, depth, &entry->value))
		return false;

	if (!reg_parse_sep(ps, depth))
		return false;

	if (!prs_uint32("type", ps, depth, &entry->type))
		return false;

	if (!reg_parse_sep(ps, depth))
		return false;

	if (!prs_uint32("size", ps, depth, &size))
		return false;

	entry->size = size;

	if (!reg_parse_sep(ps, depth))
		return false;

	if (entry->size) {
		entry->data = TALLOC_ZERO_ARRAY(mem_ctx, uint8, entry->size);
		if (!entry->data)
			return false;
	}

	if (!prs_uint8s(false, "data", ps, depth, entry->data, entry->size))
		return false;

	if (!reg_parse_term(ps, depth))
		return false;

	return true;
}

/****************************************************************
****************************************************************/

static bool reg_parse_value(TALLOC_CTX *mem_ctx,
			    char **value,
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
					 struct gp_registry_file_entry *file_entry,
					 struct gp_registry_entry **reg_entry)
{
	struct registry_value *data = NULL;
	struct gp_registry_entry *entry = NULL;
	char *key = NULL;
	char *value = NULL;
	enum gp_reg_action action = GP_REG_ACTION_NONE;
	size_t converted_size;

	ZERO_STRUCTP(*reg_entry);

	data = TALLOC_ZERO_P(mem_ctx, struct registry_value);
	if (!data)
		return false;

	if (strlen_w((const smb_ucs2_t *)file_entry->key.buffer) <= 0)
		return false;

	if (!pull_ucs2_talloc(mem_ctx, &key, file_entry->key.buffer,
			      &converted_size))
	{
		return false;
	}

	if (strlen_w((const smb_ucs2_t *)file_entry->value.buffer) > 0 &&
	    !pull_ucs2_talloc(mem_ctx, &value, file_entry->value.buffer,
			      &converted_size))
	{
			return false;
	}

	if (!reg_parse_value(mem_ctx, &value, &action))
		return false;

	data->type = file_entry->type;

	switch (data->type) {
		case REG_DWORD:
			data->v.dword = atoi((char *)file_entry->data);
			break;
		case REG_BINARY:
			data->v.binary = data_blob_talloc(mem_ctx,
							  file_entry->data,
							  file_entry->size);
			break;
		case REG_NONE:
			break;
		case REG_SZ:
			if (!pull_ucs2_talloc(mem_ctx, &data->v.sz.str,
					      (const smb_ucs2_t *)
					      file_entry->data,
					      &data->v.sz.len)) {
				data->v.sz.len = -1;
			}

			break;
		case REG_DWORD_BIG_ENDIAN:
		case REG_EXPAND_SZ:
		case REG_LINK:
		case REG_MULTI_SZ:
		case REG_QWORD:
/*		case REG_DWORD_LITTLE_ENDIAN: */
/*		case REG_QWORD_LITTLE_ENDIAN: */
			printf("not yet implemented: %d\n", data->type);
			return false;
		default:
			printf("invalid reg type defined: %d\n", data->type);
			return false;

	}

	entry = TALLOC_ZERO_P(mem_ctx, struct gp_registry_entry);
	if (!entry)
		return false;

	entry->key = key;
	entry->value = value;
	entry->data = data;
	entry->action = action;

	*reg_entry = entry;

	return true;
}

/****************************************************************
* [key;value;type;size;data][key;value;type;size;data]...
****************************************************************/

static bool reg_parse_entries(TALLOC_CTX *mem_ctx,
			      const char *desc,
			      struct gp_registry_entry **entries,
			      size_t *num_entries,
			      prs_struct *ps,
			      int depth)
{

	if (!entries || !num_entries)
		return false;

	prs_debug(ps, depth, desc, "reg_parse_entries");
	depth++;

	*entries = NULL;
	*num_entries = 0;

	while (ps->buffer_size > ps->data_offset) {

		struct gp_registry_file_entry f_entry;
		struct gp_registry_entry *r_entry = NULL;

		if (!reg_parse_entry(mem_ctx, desc, &f_entry,
				     ps, depth))
			return false;

		if (!gp_reg_entry_from_file_entry(mem_ctx,
						  &f_entry,
						  &r_entry))
			return false;

		if (!add_gp_registry_entry_to_array(mem_ctx,
						    r_entry,
						    entries,
						    num_entries))
			return false;
	}

	return true;
}

/****************************************************************
****************************************************************/

static NTSTATUS reg_parse_registry(TALLOC_CTX *mem_ctx,
				   uint32_t flags,
				   const char *filename,
				   struct gp_registry_entry **entries,
				   size_t *num_entries)
{
	uint16_t *buf = NULL;
	size_t n = 0;
	NTSTATUS status;
	prs_struct ps;
	struct gp_registry_file *reg_file;
	const char *real_filename = NULL;

	reg_file = TALLOC_ZERO_P(mem_ctx, struct gp_registry_file);
	NT_STATUS_HAVE_NO_MEMORY(reg_file);

	status = gp_find_file(mem_ctx,
			      flags,
			      filename,
			      GP_REGPOL_FILE,
			      &real_filename);
	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(reg_file);
		return status;
	}

	buf = (uint16 *)file_load(real_filename, &n, 0, NULL);
	if (!buf) {
		TALLOC_FREE(reg_file);
		return NT_STATUS_CANNOT_LOAD_REGISTRY_FILE;
	}

	if (!prs_init(&ps, n, mem_ctx, UNMARSHALL)) {
		status = NT_STATUS_NO_MEMORY;
		goto out;
	}

	if (!prs_copy_data_in(&ps, (char *)buf, n)) {
		status = NT_STATUS_NO_MEMORY;
		goto out;
	}

	prs_set_offset(&ps, 0);

	if (!reg_parse_header("header", &reg_file->header, &ps, 0)) {
		status = NT_STATUS_REGISTRY_IO_FAILED;
		goto out;
	}

	if (reg_file->header.signature != GP_REGPOL_FILE_SIGNATURE) {
		status = NT_STATUS_INVALID_PARAMETER;
		goto out;
	}

	if (reg_file->header.version != GP_REGPOL_FILE_VERSION) {
		status = NT_STATUS_INVALID_PARAMETER;
		goto out;
	}

	if (!reg_parse_entries(mem_ctx, "entries", &reg_file->entries,
			       &reg_file->num_entries, &ps, 0)) {
		status = NT_STATUS_REGISTRY_IO_FAILED;
		goto out;
	}

	*entries = reg_file->entries;
	*num_entries = reg_file->num_entries;

	status = NT_STATUS_OK;

 out:
	TALLOC_FREE(buf);
	prs_mem_free(&ps);

	return status;
}

/****************************************************************
****************************************************************/

static WERROR reg_apply_registry(TALLOC_CTX *mem_ctx,
				 const struct nt_user_token *token,
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
					      const struct nt_user_token *token,
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
