/*
 *  Unix SMB/CIFS implementation.
 *  Group Policy Support
 *  Copyright (C) Guenther Deschner 2005-2008
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

#define GP_EXT_NAME "security"

#define GPTTMPL_UNIX_PATH  "Microsoft/Windows NT/SecEdit/GptTmpl.inf"

#define GPTTMPL_SECTION_UNICODE			"Unicode"
#define GPTTMPL_SECTION_VERSION			"Version"

#define GPTTMPL_SECTION_REGISTRY_VALUES		"Registry Values"
#define GPTTMPL_SECTION_SYSTEM_ACCESS		"System Access"
#define GPTTMPL_SECTION_KERBEROS_POLICY		"Kerberos Policy"
#define GPTTMPL_SECTION_EVENT_AUDIT		"Event Audit"
#define GPTTMPL_SECTION_PRIVILEGE_RIGHTS	"Privilege Rights"
#define GPTTMPL_SECTION_APPLICATION_LOG		"Application Log"
#define GPTTMPL_SECTION_SECURITY_LOG		"Security Log"
#define GPTTMPL_SECTION_SYSTEM_LOG		"System Log"
#define GPTTMPL_SECTION_GROUP_MEMBERSHIP	"Group Membership"
#define GPTTMPL_SECTION_FILE_SECURITY		"File Security"
#define GPTTMPL_SECTION_SERVICE_GENERAL_SETTING "Service General Setting"

static TALLOC_CTX *ctx = NULL;

struct gpttmpl_table {
	const char *section;
	const char *parameter;
	enum winreg_Type type;
};

/****************************************************************
 parse the Version section from gpttmpl file
****************************************************************/

#define GPTTMPL_PARAMETER_REVISION "Revision"
#define GPTTMPL_PARAMETER_SIGNATURE "signature"
#define GPTTMPL_VALUE_CHICAGO "$CHICAGO$" /* whatever this is good for... */
#define GPTTMPL_PARAMETER_UNICODE "Unicode"

static NTSTATUS gpttmpl_parse_header(struct gp_inifile_context *ini_ctx,
				     uint32_t *version_out)
{
	char *signature = NULL;
	NTSTATUS result;
	int version;
	int is_unicode;

	if (!ini_ctx) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	result = gp_inifile_getstring(ini_ctx, GPTTMPL_SECTION_VERSION
			":"GPTTMPL_PARAMETER_SIGNATURE, &signature);
	if (!NT_STATUS_IS_OK(result)) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	if (!strequal(signature, GPTTMPL_VALUE_CHICAGO)) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}
	result = gp_inifile_getint(ini_ctx, GPTTMPL_SECTION_VERSION
			":"GPTTMPL_PARAMETER_REVISION, &version);
	if (!NT_STATUS_IS_OK(result)) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	if (version_out) {
		*version_out = version;
	}

	result = gp_inifile_getint(ini_ctx, GPTTMPL_SECTION_UNICODE
			":"GPTTMPL_PARAMETER_UNICODE, &is_unicode);
	if (!NT_STATUS_IS_OK(result) || !is_unicode) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static NTSTATUS gpttmpl_init_context(TALLOC_CTX *mem_ctx,
				     uint32_t flags,
				     const char *unix_path,
				     struct gp_inifile_context **ini_ctx)
{
	NTSTATUS status;
	uint32_t version;
	struct gp_inifile_context *tmp_ctx = NULL;

	status = gp_inifile_init_context(mem_ctx, flags, unix_path,
					 GPTTMPL_UNIX_PATH, &tmp_ctx);
	NT_STATUS_NOT_OK_RETURN(status);

	status = gpttmpl_parse_header(tmp_ctx, &version);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1,("gpttmpl_init_context: failed: %s\n",
			nt_errstr(status)));
		TALLOC_FREE(tmp_ctx);
		return status;
	}

	*ini_ctx = tmp_ctx;

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static NTSTATUS gpttmpl_process(struct gp_inifile_context *ini_ctx,
				struct registry_key *root_key,
				uint32_t flags)
{
	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static NTSTATUS security_process_group_policy(ADS_STRUCT *ads,
					      TALLOC_CTX *mem_ctx,
					      uint32_t flags,
					      struct registry_key *root_key,
					      const struct security_token *token,
					      struct GROUP_POLICY_OBJECT *gpo,
					      const char *extension_guid,
					      const char *snapin_guid)
{
	NTSTATUS status;
	char *unix_path = NULL;
	struct gp_inifile_context *ini_ctx = NULL;

	debug_gpext_header(0, "security_process_group_policy", flags, gpo,
			   extension_guid, snapin_guid);

	/* this handler processes the gpttmpl files and merge output to the
	 * registry */

	status = gpo_get_unix_path(mem_ctx, cache_path(GPO_CACHE_DIR), gpo, &unix_path);
	if (!NT_STATUS_IS_OK(status)) {
		goto out;
	}

	status = gpttmpl_init_context(mem_ctx, flags, unix_path, &ini_ctx);
	if (!NT_STATUS_IS_OK(status)) {
		goto out;
	}

	status = gpttmpl_process(ini_ctx, root_key, flags);
	if (!NT_STATUS_IS_OK(status)) {
		goto out;
	}

 out:
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("security_process_group_policy: %s\n",
			nt_errstr(status)));
	}
	TALLOC_FREE(ini_ctx);

	return status;
}

/****************************************************************
****************************************************************/

static NTSTATUS security_get_reg_config(TALLOC_CTX *mem_ctx,
					struct gp_extension_reg_info **reg_info)
{
	NTSTATUS status;
	struct gp_extension_reg_info *info = NULL;

	struct gp_extension_reg_table table[] = {
		/* FIXME: how can we store the "(Default)" value ??? */
		/* { "", REG_SZ, "Security" }, */
		{ "ProcessGroupPolicy", REG_SZ, "security_process_group_policy" },
		{ "NoUserPolicy", REG_DWORD, "1" },
		{ "ExtensionDebugLevel", REG_DWORD, "1" },
		{ NULL, REG_NONE, NULL }
	};

	info = TALLOC_ZERO_P(mem_ctx, struct gp_extension_reg_info);
	NT_STATUS_HAVE_NO_MEMORY(info);

	status = gp_ext_info_add_entry(mem_ctx, GP_EXT_NAME,
				       GP_EXT_GUID_SECURITY,
				       table, info);
	NT_STATUS_NOT_OK_RETURN(status);

	*reg_info = info;

	return NT_STATUS_OK;
}


/****************************************************************
****************************************************************/

static NTSTATUS security_initialize(TALLOC_CTX *mem_ctx)
{
	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static NTSTATUS security_shutdown(void)
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

static struct gp_extension_methods security_methods = {
	.initialize		= security_initialize,
	.process_group_policy	= security_process_group_policy,
	.get_reg_config		= security_get_reg_config,
	.shutdown		= security_shutdown
};

/****************************************************************
****************************************************************/

NTSTATUS gpext_security_init(void)
{
	NTSTATUS status;

	ctx = talloc_init("gpext_security_init");
	NT_STATUS_HAVE_NO_MEMORY(ctx);

	status = register_gp_extension(ctx, SMB_GPEXT_INTERFACE_VERSION,
				       GP_EXT_NAME, GP_EXT_GUID_SECURITY,
				       &security_methods);
	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(ctx);
	}

	return status;
}
