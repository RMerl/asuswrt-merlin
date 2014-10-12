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
#include "../libgpo/gpo.h"
#include "../libgpo/gpext/gpext.h"
#include "librpc/gen_ndr/ndr_misc.h"
#include "lib/util/dlinklist.h"
#include "../libcli/registry/util_reg.h"
#if _SAMBA_BUILD_ == 3
#include "libgpo/gpo_proto.h"
#include "registry.h"
#include "registry/reg_api.h"
#endif

static struct gp_extension *extensions = NULL;

/****************************************************************
****************************************************************/

struct gp_extension *get_gp_extension_list(void)
{
	return extensions;
}

/****************************************************************
****************************************************************/

/* see http://support.microsoft.com/kb/216358/en-us/ for more info */

struct gp_extension_reg_table gpext_reg_vals[] = {
	{ "DllName", REG_EXPAND_SZ },
	{ "ProcessGroupPolicy", REG_SZ },
	{ "NoMachinePolicy", REG_DWORD },
	{ "NoUserPolicy", REG_DWORD },
	{ "NoSlowLink", REG_DWORD },
	{ "NoBackgroundPolicy", REG_DWORD },
	{ "NoGPOListChanges", REG_DWORD },
	{ "PerUserLocalSettings", REG_DWORD },
	{ "RequiresSuccessfulRegistry", REG_DWORD },
	{ "EnableAsynchronousProcessing", REG_DWORD },
	{ "ExtensionDebugLevel", REG_DWORD },
	/* new */
	{ "GenerateGroupPolicy", REG_SZ }, /* not supported on w2k */
	{ "NotifyLinkTransition", REG_DWORD },
	{ "ProcessGroupPolicyEx", REG_SZ }, /* not supported on w2k */
	{ "ExtensionEventSource", REG_MULTI_SZ }, /* not supported on w2k */
	{ "GenerateGroupPolicy", REG_SZ },
	{ "MaxNoGPOListChangesInterval", REG_DWORD },
	{ NULL, REG_NONE }
};

/****************************************************************
****************************************************************/

static struct gp_extension *get_extension_by_name(struct gp_extension *be,
						  const char *name)
{
	struct gp_extension *b;

	for (b = be; b; b = b->next) {
		if (strequal(b->name, name)) {
			return b;
		}
	}

	return NULL;
}

/****************************************************************
****************************************************************/

static struct gp_extension_methods *get_methods_by_name(struct gp_extension *be,
							const char *name)
{
	struct gp_extension *b;

	for (b = be; b; b = b->next) {
		if (strequal(b->name, name)) {
			return b->methods;
		}
	}

	return NULL;
}

/****************************************************************
****************************************************************/

NTSTATUS unregister_gp_extension(const char *name)
{
	struct gp_extension *ext;

	ext = get_extension_by_name(extensions, name);
	if (!ext) {
		return NT_STATUS_OK;
	}

	DLIST_REMOVE(extensions, ext);
	talloc_free(ext);

	DEBUG(2,("Successfully removed GP extension '%s'\n", name));

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

NTSTATUS register_gp_extension(TALLOC_CTX *gpext_ctx,
			       int version,
			       const char *name,
			       const char *guid,
			       struct gp_extension_methods *methods)
{
	struct gp_extension_methods *test;
	struct gp_extension *entry;
	NTSTATUS status;

	if (!gpext_ctx) {
		return NT_STATUS_INTERNAL_DB_ERROR;
	}

	if ((version != SMB_GPEXT_INTERFACE_VERSION)) {
		DEBUG(0,("Failed to register gp extension.\n"
		         "The module was compiled against "
			 "SMB_GPEXT_INTERFACE_VERSION %d,\n"
		         "current SMB_GPEXT_INTERFACE_VERSION is %d.\n"
		         "Please recompile against the current "
			 "version of samba!\n",
			 version, SMB_GPEXT_INTERFACE_VERSION));
		return NT_STATUS_OBJECT_TYPE_MISMATCH;
	}

	if (!guid || !name || !name[0] || !methods) {
		DEBUG(0,("Called with NULL pointer or empty name!\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	test = get_methods_by_name(extensions, name);
	if (test) {
		DEBUG(0,("GP extension module %s already registered!\n",
			name));
		return NT_STATUS_OBJECT_NAME_COLLISION;
	}

	entry = talloc_zero(gpext_ctx, struct gp_extension);
	NT_STATUS_HAVE_NO_MEMORY(entry);

	entry->name = talloc_strdup(gpext_ctx, name);
	NT_STATUS_HAVE_NO_MEMORY(entry->name);

	entry->guid = talloc_zero(gpext_ctx, struct GUID);
	NT_STATUS_HAVE_NO_MEMORY(entry->guid);
	status = GUID_from_string(guid, entry->guid);
	NT_STATUS_NOT_OK_RETURN(status);

	entry->methods = methods;
	DLIST_ADD(extensions, entry);

	DEBUG(2,("Successfully added GP extension '%s' %s\n",
		name, GUID_string2(gpext_ctx, entry->guid)));

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static NTSTATUS gp_extension_init_module(TALLOC_CTX *mem_ctx,
					 const char *name,
					 struct gp_extension **gpext)
{
	NTSTATUS status;
	struct gp_extension *ext = NULL;

	ext = talloc_zero(mem_ctx, struct gp_extension);
	NT_STATUS_HAVE_NO_MEMORY(gpext);

	ext->methods = get_methods_by_name(extensions, name);
	if (!ext->methods) {

		status = smb_probe_module(SAMBA_SUBSYSTEM_GPEXT,
					  name);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}

		ext->methods = get_methods_by_name(extensions, name);
		if (!ext->methods) {
			return NT_STATUS_DLL_INIT_FAILED;
		}
	}

	*gpext = ext;

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static bool add_gp_extension_reg_entry_to_array(TALLOC_CTX *mem_ctx,
						struct gp_extension_reg_entry *entry,
						struct gp_extension_reg_entry **entries,
						size_t *num)
{
	*entries = talloc_realloc(mem_ctx, *entries,
					struct gp_extension_reg_entry,
					(*num)+1);
	if (*entries == NULL) {
		*num = 0;
		return false;
	}

	(*entries)[*num].value = entry->value;
	(*entries)[*num].data = entry->data;

	*num += 1;
	return true;
}

/****************************************************************
****************************************************************/

static bool add_gp_extension_reg_info_entry_to_array(TALLOC_CTX *mem_ctx,
						     struct gp_extension_reg_info_entry *entry,
						     struct gp_extension_reg_info_entry **entries,
						     size_t *num)
{
	*entries = talloc_realloc(mem_ctx, *entries,
					struct gp_extension_reg_info_entry,
					(*num)+1);
	if (*entries == NULL) {
		*num = 0;
		return false;
	}

	(*entries)[*num].guid = entry->guid;
	(*entries)[*num].num_entries = entry->num_entries;
	(*entries)[*num].entries = entry->entries;

	*num += 1;
	return true;
}

/****************************************************************
****************************************************************/

static NTSTATUS gp_ext_info_add_reg(TALLOC_CTX *mem_ctx,
				    struct gp_extension_reg_info_entry *entry,
				    const char *value,
				    enum winreg_Type type,
				    const char *data_s)
{
	struct gp_extension_reg_entry *reg_entry = NULL;
	struct registry_value *data = NULL;

	reg_entry = talloc_zero(mem_ctx, struct gp_extension_reg_entry);
	NT_STATUS_HAVE_NO_MEMORY(reg_entry);

	data = talloc_zero(mem_ctx, struct registry_value);
	NT_STATUS_HAVE_NO_MEMORY(data);

	data->type = type;

	switch (type) {
		case REG_SZ:
		case REG_EXPAND_SZ:
			if (!push_reg_sz(mem_ctx, &data->data, data_s)) {
				return NT_STATUS_NO_MEMORY;
			}
			break;
		case REG_DWORD: {
			uint32_t v = atoi(data_s);
			data->data = data_blob_talloc(mem_ctx, NULL, 4);
			SIVAL(data->data.data, 0, v);
			break;
		}
		default:
			return NT_STATUS_NOT_SUPPORTED;
	}

	reg_entry->value = value;
	reg_entry->data = data;

	if (!add_gp_extension_reg_entry_to_array(mem_ctx, reg_entry,
						 &entry->entries,
						 &entry->num_entries)) {
		return NT_STATUS_NO_MEMORY;
	}

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static NTSTATUS gp_ext_info_add_reg_table(TALLOC_CTX *mem_ctx,
					  const char *module,
					  struct gp_extension_reg_info_entry *entry,
					  struct gp_extension_reg_table *table)
{
	NTSTATUS status;
	const char *module_name = NULL;
	int i;

	module_name = talloc_asprintf(mem_ctx, "%s.%s", module, shlib_ext());
	NT_STATUS_HAVE_NO_MEMORY(module_name);

	status = gp_ext_info_add_reg(mem_ctx, entry,
				     "DllName", REG_EXPAND_SZ, module_name);
	NT_STATUS_NOT_OK_RETURN(status);

	for (i=0; table[i].val; i++) {
		status = gp_ext_info_add_reg(mem_ctx, entry,
					     table[i].val,
					     table[i].type,
					     table[i].data);
		NT_STATUS_NOT_OK_RETURN(status);
	}

	return status;
}

/****************************************************************
****************************************************************/

NTSTATUS gp_ext_info_add_entry(TALLOC_CTX *mem_ctx,
			       const char *module,
			       const char *ext_guid,
			       struct gp_extension_reg_table *table,
			       struct gp_extension_reg_info *info)
{
	NTSTATUS status;
	struct gp_extension_reg_info_entry *entry = NULL;

	entry = TALLOC_ZERO_P(mem_ctx, struct gp_extension_reg_info_entry);
	NT_STATUS_HAVE_NO_MEMORY(entry);

	status = GUID_from_string(ext_guid, &entry->guid);
	NT_STATUS_NOT_OK_RETURN(status);

	status = gp_ext_info_add_reg_table(mem_ctx, module, entry, table);
	NT_STATUS_NOT_OK_RETURN(status);

	if (!add_gp_extension_reg_info_entry_to_array(mem_ctx, entry,
						      &info->entries,
						      &info->num_entries)) {
		return NT_STATUS_NO_MEMORY;
	}

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static bool gp_extension_reg_info_verify_entry(struct gp_extension_reg_entry *entry)
{
	int i;

	for (i=0; gpext_reg_vals[i].val; i++) {

		if ((strequal(entry->value, gpext_reg_vals[i].val)) &&
		    (entry->data->type == gpext_reg_vals[i].type)) {
			return true;
		}
	}

	return false;
}

/****************************************************************
****************************************************************/

static bool gp_extension_reg_info_verify(struct gp_extension_reg_info_entry *entry)
{
	int i;

	for (i=0; i < entry->num_entries; i++) {
		if (!gp_extension_reg_info_verify_entry(&entry->entries[i])) {
			return false;
		}
	}

	return true;
}

/****************************************************************
****************************************************************/

static WERROR gp_extension_store_reg_vals(TALLOC_CTX *mem_ctx,
					  struct registry_key *key,
					  struct gp_extension_reg_info_entry *entry)
{
	WERROR werr = WERR_OK;
	size_t i;

	for (i=0; i < entry->num_entries; i++) {

		werr = reg_setvalue(key,
				    entry->entries[i].value,
				    entry->entries[i].data);
		W_ERROR_NOT_OK_RETURN(werr);
	}

	return werr;
}

/****************************************************************
****************************************************************/

static WERROR gp_extension_store_reg_entry(TALLOC_CTX *mem_ctx,
					   struct gp_registry_context *reg_ctx,
					   struct gp_extension_reg_info_entry *entry)
{
	WERROR werr;
	struct registry_key *key = NULL;
	const char *subkeyname = NULL;

	if (!gp_extension_reg_info_verify(entry)) {
		return WERR_INVALID_PARAM;
	}

	subkeyname = GUID_string2(mem_ctx, &entry->guid);
	W_ERROR_HAVE_NO_MEMORY(subkeyname);

	strupper_m(CONST_DISCARD(char *,subkeyname));

	werr = gp_store_reg_subkey(mem_ctx,
				   subkeyname,
				   reg_ctx->curr_key,
				   &key);
	W_ERROR_NOT_OK_RETURN(werr);

	werr = gp_extension_store_reg_vals(mem_ctx,
					   key,
					   entry);
	W_ERROR_NOT_OK_RETURN(werr);

	return werr;
}

/****************************************************************
****************************************************************/

static WERROR gp_extension_store_reg(TALLOC_CTX *mem_ctx,
				     struct gp_registry_context *reg_ctx,
				     struct gp_extension_reg_info *info)
{
	WERROR werr = WERR_OK;
	int i;

	if (!info) {
		return WERR_OK;
	}

	for (i=0; i < info->num_entries; i++) {
		werr = gp_extension_store_reg_entry(mem_ctx,
						    reg_ctx,
						    &info->entries[i]);
		W_ERROR_NOT_OK_RETURN(werr);
	}

	return werr;
}

/****************************************************************
****************************************************************/

static NTSTATUS gp_glob_ext_list(TALLOC_CTX *mem_ctx,
				 const char ***ext_list,
				 size_t *ext_list_len)
{
	SMB_STRUCT_DIR *dir = NULL;
	SMB_STRUCT_DIRENT *dirent = NULL;

	dir = sys_opendir(modules_path(SAMBA_SUBSYSTEM_GPEXT));
	if (!dir) {
		return map_nt_error_from_unix(errno);
	}

	while ((dirent = sys_readdir(dir))) {

		fstring name; /* forgive me... */
		char *p;

		if ((strequal(dirent->d_name, ".")) ||
		    (strequal(dirent->d_name, ".."))) {
			continue;
		}

		p = strrchr(dirent->d_name, '.');
		if (!p) {
			sys_closedir(dir);
			return NT_STATUS_NO_MEMORY;
		}

		if (!strcsequal(p+1, shlib_ext())) {
			DEBUG(10,("gp_glob_ext_list: not a *.so file: %s\n",
				dirent->d_name));
			continue;
		}

		fstrcpy(name, dirent->d_name);
		name[PTR_DIFF(p, dirent->d_name)] = 0;

		if (!add_string_to_array(mem_ctx, name, ext_list,
					 (int *)ext_list_len)) {
			sys_closedir(dir);
			return NT_STATUS_NO_MEMORY;
		}
	}

	sys_closedir(dir);

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

NTSTATUS shutdown_gp_extensions(void)
{
	struct gp_extension *ext = NULL;

	for (ext = extensions; ext; ext = ext->next) {
		if (ext->methods && ext->methods->shutdown) {
			ext->methods->shutdown();
		}
	}

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

NTSTATUS init_gp_extensions(TALLOC_CTX *mem_ctx)
{
	NTSTATUS status;
	WERROR werr;
	int i = 0;
	const char **ext_array = NULL;
	size_t ext_array_len = 0;
	struct gp_extension *gpext = NULL;
	struct gp_registry_context *reg_ctx = NULL;

	if (get_gp_extension_list()) {
		return NT_STATUS_OK;
	}

	status = gp_glob_ext_list(mem_ctx, &ext_array, &ext_array_len);
	NT_STATUS_NOT_OK_RETURN(status);

	for (i=0; i<ext_array_len; i++) {

		struct gp_extension_reg_info *info = NULL;

		status = gp_extension_init_module(mem_ctx, ext_array[i],
						  &gpext);
		if (!NT_STATUS_IS_OK(status)) {
			goto out;
		}

		if (gpext->methods->get_reg_config) {

			status = gpext->methods->initialize(mem_ctx);
			if (!NT_STATUS_IS_OK(status)) {
				gpext->methods->shutdown();
				goto out;
			}

			status = gpext->methods->get_reg_config(mem_ctx,
								&info);
			if (!NT_STATUS_IS_OK(status)) {
				gpext->methods->shutdown();
				goto out;
			}

			if (!reg_ctx) {
				struct security_token *token;

				token = registry_create_system_token(mem_ctx);
				NT_STATUS_HAVE_NO_MEMORY(token);

				werr = gp_init_reg_ctx(mem_ctx,
						       KEY_WINLOGON_GPEXT_PATH,
						       REG_KEY_WRITE,
						       token,
						       &reg_ctx);
				if (!W_ERROR_IS_OK(werr)) {
					status = werror_to_ntstatus(werr);
					gpext->methods->shutdown();
					goto out;
				}
			}

			werr = gp_extension_store_reg(mem_ctx, reg_ctx, info);
			if (!W_ERROR_IS_OK(werr)) {
				DEBUG(1,("gp_extension_store_reg failed: %s\n",
					win_errstr(werr)));
				TALLOC_FREE(info);
				gpext->methods->shutdown();
				status = werror_to_ntstatus(werr);
				goto out;
			}
			TALLOC_FREE(info);
		}

	}

 out:
	TALLOC_FREE(reg_ctx);

	return status;
}

/****************************************************************
****************************************************************/

NTSTATUS free_gp_extensions(void)
{
	struct gp_extension *ext, *ext_next = NULL;

	for (ext = extensions; ext; ext = ext_next) {
		ext_next = ext->next;
		DLIST_REMOVE(extensions, ext);
		TALLOC_FREE(ext);
	}

	extensions = NULL;

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

void debug_gpext_header(int lvl,
			const char *name,
			uint32_t flags,
			struct GROUP_POLICY_OBJECT *gpo,
			const char *extension_guid,
			const char *snapin_guid)
{
	char *flags_str = NULL;

	DEBUG(lvl,("%s\n", name));
	DEBUGADD(lvl,("\tgpo:           %s (%s)\n", gpo->name,
		gpo->display_name));
	DEBUGADD(lvl,("\tcse extension: %s (%s)\n", extension_guid,
		cse_gpo_guid_string_to_name(extension_guid)));
	DEBUGADD(lvl,("\tgplink:        %s\n", gpo->link));
	DEBUGADD(lvl,("\tsnapin:        %s (%s)\n", snapin_guid,
		cse_snapin_gpo_guid_string_to_name(snapin_guid)));

	flags_str = gpo_flag_str(NULL, flags);
	DEBUGADD(lvl,("\tflags:         0x%08x %s\n", flags, flags_str));
	TALLOC_FREE(flags_str);
}

NTSTATUS process_gpo_list_with_extension(ADS_STRUCT *ads,
			   TALLOC_CTX *mem_ctx,
			   uint32_t flags,
			   const struct security_token *token,
			   struct GROUP_POLICY_OBJECT *gpo_list,
			   const char *extension_guid,
			   const char *snapin_guid)
{
	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

NTSTATUS gpext_process_extension(ADS_STRUCT *ads,
				 TALLOC_CTX *mem_ctx,
				 uint32_t flags,
				 const struct security_token *token,
				 struct registry_key *root_key,
				 struct GROUP_POLICY_OBJECT *gpo,
				 const char *extension_guid,
				 const char *snapin_guid)
{
	NTSTATUS status;
	struct gp_extension *ext = NULL;
	struct GUID guid;
	bool cse_found = false;

	status = init_gp_extensions(mem_ctx);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1,("init_gp_extensions failed: %s\n",
			nt_errstr(status)));
		return status;
	}

	status = GUID_from_string(extension_guid, &guid);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	for (ext = extensions; ext; ext = ext->next) {

		if (GUID_equal(ext->guid, &guid)) {
			cse_found = true;
			break;
		}
	}

	if (!cse_found) {
		goto no_ext;
	}

	status = ext->methods->initialize(mem_ctx);
	NT_STATUS_NOT_OK_RETURN(status);

	status = ext->methods->process_group_policy(ads,
						    mem_ctx,
						    flags,
						    root_key,
						    token,
						    gpo,
						    extension_guid,
						    snapin_guid);
	if (!NT_STATUS_IS_OK(status)) {
		ext->methods->shutdown();
	}

	return status;

 no_ext:
	if (flags & GPO_INFO_FLAG_VERBOSE) {
		DEBUG(0,("process_extension: no extension available for:\n"));
		DEBUGADD(0,("%s (%s) (snapin: %s)\n",
			extension_guid,
			cse_gpo_guid_string_to_name(extension_guid),
			snapin_guid));
	}

	return NT_STATUS_OK;
}
