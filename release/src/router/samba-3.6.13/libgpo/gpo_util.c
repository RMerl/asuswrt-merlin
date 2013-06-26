/*
 *  Unix SMB/CIFS implementation.
 *  Group Policy Object Support
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
#define TALLOC_DEPRECATED 1
#include "includes.h"
#include "system/filesys.h"
#include "librpc/gen_ndr/ndr_misc.h"
#include "../librpc/gen_ndr/ndr_security.h"
#include "../libgpo/gpo.h"
#include "../libcli/security/security.h"
#if _SAMBA_BUILD_ == 4
#include "auth/auth.h"
#include <talloc.h>
#include "source4/libgpo/ads_convenience.h"
#endif
#undef strdup

#if 0
#define DEFAULT_DOMAIN_POLICY "Default Domain Policy"
#define DEFAULT_DOMAIN_CONTROLLERS_POLICY "Default Domain Controllers Policy"
#endif

/* should we store a parsed guid ? */
struct gp_table {
	const char *name;
	const char *guid_string;
};

#if 0 /* unused */
static struct gp_table gpo_default_policy[] = {
	{ DEFAULT_DOMAIN_POLICY,
		"31B2F340-016D-11D2-945F-00C04FB984F9" },
	{ DEFAULT_DOMAIN_CONTROLLERS_POLICY,
		"6AC1786C-016F-11D2-945F-00C04fB984F9" },
	{ NULL, NULL }
};
#endif

/* the following is seen in gPCMachineExtensionNames / gPCUserExtensionNames */

static struct gp_table gpo_cse_extensions[] = {
	/* used to be "Administrative Templates Extension" */
	/* "Registry Settings"
	(http://support.microsoft.com/kb/216357/EN-US/) */
	{ "Registry Settings",
		GP_EXT_GUID_REGISTRY },
	{ "Microsoft Disc Quota",
		"3610EDA5-77EF-11D2-8DC5-00C04FA31A66" },
	{ "EFS recovery",
		"B1BE8D72-6EAC-11D2-A4EA-00C04F79F83A" },
	{ "Folder Redirection",
		"25537BA6-77A8-11D2-9B6C-0000F8080861" },
	{ "IP Security",
		"E437BC1C-AA7D-11D2-A382-00C04F991E27" },
	{ "Internet Explorer Branding",
		"A2E30F80-D7DE-11d2-BBDE-00C04F86AE3B" },
	{ "QoS Packet Scheduler",
		"426031c0-0b47-4852-b0ca-ac3d37bfcb39" },
	{ "Scripts",
		GP_EXT_GUID_SCRIPTS },
	{ "Security",
		GP_EXT_GUID_SECURITY },
	{ "Software Installation",
		"C6DC5466-785A-11D2-84D0-00C04FB169F7" },
	{ "Wireless Group Policy",
		"0ACDD40C-75AC-BAA0-BF6DE7E7FE63" },
	{ "Application Management",
		"C6DC5466-785A-11D2-84D0-00C04FB169F7" },
	{ "unknown",
		"3060E8D0-7020-11D2-842D-00C04FA372D4" },
	{ NULL, NULL }
};

/* guess work */
static struct gp_table gpo_cse_snapin_extensions[] = {
	{ "Administrative Templates",
		"0F6B957D-509E-11D1-A7CC-0000F87571E3" },
	{ "Certificates",
		"53D6AB1D-2488-11D1-A28C-00C04FB94F17" },
	{ "EFS recovery policy processing",
		"B1BE8D72-6EAC-11D2-A4EA-00C04F79F83A" },
	{ "Folder Redirection policy processing",
		"25537BA6-77A8-11D2-9B6C-0000F8080861" },
	{ "Folder Redirection",
		"88E729D6-BDC1-11D1-BD2A-00C04FB9603F" },
	{ "Registry policy processing",
		"35378EAC-683F-11D2-A89A-00C04FBBCFA2" },
	{ "Remote Installation Services",
		"3060E8CE-7020-11D2-842D-00C04FA372D4" },
	{ "Security Settings",
		"803E14A0-B4FB-11D0-A0D0-00A0C90F574B" },
	{ "Security policy processing",
		"827D319E-6EAC-11D2-A4EA-00C04F79F83A" },
	{ "unknown",
		"3060E8D0-7020-11D2-842D-00C04FA372D4" },
	{ "unknown2",
		"53D6AB1B-2488-11D1-A28C-00C04FB94F17" },
	{ NULL, NULL }
};

/****************************************************************
****************************************************************/

static const char *name_to_guid_string(const char *name,
				       struct gp_table *table)
{
	int i;

	for (i = 0; table[i].name; i++) {
		if (strequal(name, table[i].name)) {
			return table[i].guid_string;
		}
	}

	return NULL;
}

/****************************************************************
****************************************************************/

static const char *guid_string_to_name(const char *guid_string,
				       struct gp_table *table)
{
	int i;

	for (i = 0; table[i].guid_string; i++) {
		if (strequal(guid_string, table[i].guid_string)) {
			return table[i].name;
		}
	}

	return NULL;
}

/****************************************************************
****************************************************************/

static const char *snapin_guid_string_to_name(const char *guid_string,
					      struct gp_table *table)
{
	int i;
	for (i = 0; table[i].guid_string; i++) {
		if (strequal(guid_string, table[i].guid_string)) {
			return table[i].name;
		}
	}
	return NULL;
}

#if 0 /* unused */
static const char *default_gpo_name_to_guid_string(const char *name)
{
	return name_to_guid_string(name, gpo_default_policy);
}

static const char *default_gpo_guid_string_to_name(const char *guid)
{
	return guid_string_to_name(guid, gpo_default_policy);
}
#endif

/****************************************************************
****************************************************************/

const char *cse_gpo_guid_string_to_name(const char *guid)
{
	return guid_string_to_name(guid, gpo_cse_extensions);
}

/****************************************************************
****************************************************************/

const char *cse_gpo_name_to_guid_string(const char *name)
{
	return name_to_guid_string(name, gpo_cse_extensions);
}

/****************************************************************
****************************************************************/

const char *cse_snapin_gpo_guid_string_to_name(const char *guid)
{
	return snapin_guid_string_to_name(guid, gpo_cse_snapin_extensions);
}

/****************************************************************
****************************************************************/

void dump_gp_ext(struct GP_EXT *gp_ext, int debuglevel)
{
	int lvl = debuglevel;
	int i;

	if (gp_ext == NULL) {
		return;
	}

	DEBUG(lvl,("\t---------------------\n\n"));
	DEBUGADD(lvl,("\tname:\t\t\t%s\n", gp_ext->gp_extension));

	for (i=0; i< gp_ext->num_exts; i++) {

		DEBUGADD(lvl,("\textension:\t\t\t%s\n",
			gp_ext->extensions_guid[i]));
		DEBUGADD(lvl,("\textension (name):\t\t\t%s\n",
			gp_ext->extensions[i]));

		DEBUGADD(lvl,("\tsnapin:\t\t\t%s\n",
			gp_ext->snapins_guid[i]));
		DEBUGADD(lvl,("\tsnapin (name):\t\t\t%s\n",
			gp_ext->snapins[i]));
	}
}

#ifdef HAVE_LDAP

/****************************************************************
****************************************************************/

void dump_gpo(ADS_STRUCT *ads,
	      TALLOC_CTX *mem_ctx,
	      struct GROUP_POLICY_OBJECT *gpo,
	      int debuglevel)
{
	int lvl = debuglevel;

	if (gpo == NULL) {
		return;
	}

	DEBUG(lvl,("---------------------\n\n"));

	DEBUGADD(lvl,("name:\t\t\t%s\n", gpo->name));
	DEBUGADD(lvl,("displayname:\t\t%s\n", gpo->display_name));
	DEBUGADD(lvl,("version:\t\t%d (0x%08x)\n", gpo->version, gpo->version));
	DEBUGADD(lvl,("version_user:\t\t%d (0x%04x)\n",
		GPO_VERSION_USER(gpo->version),
		GPO_VERSION_USER(gpo->version)));
	DEBUGADD(lvl,("version_machine:\t%d (0x%04x)\n",
		GPO_VERSION_MACHINE(gpo->version),
		 GPO_VERSION_MACHINE(gpo->version)));
	DEBUGADD(lvl,("filesyspath:\t\t%s\n", gpo->file_sys_path));
	DEBUGADD(lvl,("dspath:\t\t%s\n", gpo->ds_path));

	DEBUGADD(lvl,("options:\t\t%d ", gpo->options));
	switch (gpo->options) {
		case GPFLAGS_ALL_ENABLED:
			DEBUGADD(lvl,("GPFLAGS_ALL_ENABLED\n"));
			break;
		case GPFLAGS_USER_SETTINGS_DISABLED:
			DEBUGADD(lvl,("GPFLAGS_USER_SETTINGS_DISABLED\n"));
			break;
		case GPFLAGS_MACHINE_SETTINGS_DISABLED:
			DEBUGADD(lvl,("GPFLAGS_MACHINE_SETTINGS_DISABLED\n"));
			break;
		case GPFLAGS_ALL_DISABLED:
			DEBUGADD(lvl,("GPFLAGS_ALL_DISABLED\n"));
			break;
		default:
			DEBUGADD(lvl,("unknown option: %d\n", gpo->options));
			break;
	}

	DEBUGADD(lvl,("link:\t\t\t%s\n", gpo->link));
	DEBUGADD(lvl,("link_type:\t\t%d ", gpo->link_type));
	switch (gpo->link_type) {
		case GP_LINK_UNKOWN:
			DEBUGADD(lvl,("GP_LINK_UNKOWN\n"));
			break;
		case GP_LINK_OU:
			DEBUGADD(lvl,("GP_LINK_OU\n"));
			break;
		case GP_LINK_DOMAIN:
			DEBUGADD(lvl,("GP_LINK_DOMAIN\n"));
			break;
		case GP_LINK_SITE:
			DEBUGADD(lvl,("GP_LINK_SITE\n"));
			break;
		case GP_LINK_MACHINE:
			DEBUGADD(lvl,("GP_LINK_MACHINE\n"));
			break;
		default:
			break;
	}

	DEBUGADD(lvl,("machine_extensions:\t%s\n", gpo->machine_extensions));

	if (gpo->machine_extensions) {

		struct GP_EXT *gp_ext = NULL;

		if (!ads_parse_gp_ext(mem_ctx, gpo->machine_extensions,
				      &gp_ext)) {
			return;
		}
		dump_gp_ext(gp_ext, lvl);
	}

	DEBUGADD(lvl,("user_extensions:\t%s\n", gpo->user_extensions));

	if (gpo->user_extensions) {

		struct GP_EXT *gp_ext = NULL;

		if (!ads_parse_gp_ext(mem_ctx, gpo->user_extensions,
				      &gp_ext)) {
			return;
		}
		dump_gp_ext(gp_ext, lvl);
	}
	if (gpo->security_descriptor) {
		DEBUGADD(lvl,("security descriptor:\n"));

		NDR_PRINT_DEBUG(security_descriptor, gpo->security_descriptor);
	}
}

/****************************************************************
****************************************************************/

void dump_gpo_list(ADS_STRUCT *ads,
		   TALLOC_CTX *mem_ctx,
		   struct GROUP_POLICY_OBJECT *gpo_list,
		   int debuglevel)
{
	struct GROUP_POLICY_OBJECT *gpo = NULL;

	for (gpo = gpo_list; gpo; gpo = gpo->next) {
		dump_gpo(ads, mem_ctx, gpo, debuglevel);
	}
}

/****************************************************************
****************************************************************/

void dump_gplink(ADS_STRUCT *ads, TALLOC_CTX *mem_ctx, struct GP_LINK *gp_link)
{
	ADS_STATUS status;
	int i;
	int lvl = 10;

	if (gp_link == NULL) {
		return;
	}

	DEBUG(lvl,("---------------------\n\n"));

	DEBUGADD(lvl,("gplink: %s\n", gp_link->gp_link));
	DEBUGADD(lvl,("gpopts: %d ", gp_link->gp_opts));
	switch (gp_link->gp_opts) {
		case GPOPTIONS_INHERIT:
			DEBUGADD(lvl,("GPOPTIONS_INHERIT\n"));
			break;
		case GPOPTIONS_BLOCK_INHERITANCE:
			DEBUGADD(lvl,("GPOPTIONS_BLOCK_INHERITANCE\n"));
			break;
		default:
			break;
	}

	DEBUGADD(lvl,("num links: %d\n", gp_link->num_links));

	for (i = 0; i < gp_link->num_links; i++) {

		DEBUGADD(lvl,("---------------------\n\n"));

		DEBUGADD(lvl,("link: #%d\n", i + 1));
		DEBUGADD(lvl,("name: %s\n", gp_link->link_names[i]));

		DEBUGADD(lvl,("opt: %d ", gp_link->link_opts[i]));
		if (gp_link->link_opts[i] & GPO_LINK_OPT_ENFORCED) {
			DEBUGADD(lvl,("GPO_LINK_OPT_ENFORCED "));
		}
		if (gp_link->link_opts[i] & GPO_LINK_OPT_DISABLED) {
			DEBUGADD(lvl,("GPO_LINK_OPT_DISABLED"));
		}
		DEBUGADD(lvl,("\n"));

		if (ads != NULL && mem_ctx != NULL) {

			struct GROUP_POLICY_OBJECT gpo;

			status = ads_get_gpo(ads, mem_ctx,
					     gp_link->link_names[i],
					     NULL, NULL, &gpo);
			if (!ADS_ERR_OK(status)) {
				DEBUG(lvl,("get gpo for %s failed: %s\n",
					gp_link->link_names[i],
					ads_errstr(status)));
				return;
			}
			dump_gpo(ads, mem_ctx, &gpo, lvl);
		}
	}
}

#endif /* HAVE_LDAP */

/****************************************************************
****************************************************************/

static bool gpo_get_gp_ext_from_gpo(TALLOC_CTX *mem_ctx,
				    uint32_t flags,
				    struct GROUP_POLICY_OBJECT *gpo,
				    struct GP_EXT **gp_ext)
{
	ZERO_STRUCTP(*gp_ext);

	if (flags & GPO_INFO_FLAG_MACHINE) {

		if (gpo->machine_extensions) {

			if (!ads_parse_gp_ext(mem_ctx, gpo->machine_extensions,
					      gp_ext)) {
				return false;
			}
		}
	} else {

		if (gpo->user_extensions) {

			if (!ads_parse_gp_ext(mem_ctx, gpo->user_extensions,
					      gp_ext)) {
				return false;
			}
		}
	}

	return true;
}

/****************************************************************
****************************************************************/

ADS_STATUS gpo_process_a_gpo(ADS_STRUCT *ads,
			     TALLOC_CTX *mem_ctx,
			     const struct security_token *token,
			     struct registry_key *root_key,
			     struct GROUP_POLICY_OBJECT *gpo,
			     const char *extension_guid_filter,
			     uint32_t flags)
{
	struct GP_EXT *gp_ext = NULL;
	int i;

	DEBUG(10,("gpo_process_a_gpo: processing gpo %s (%s)\n",
		gpo->name, gpo->display_name));
	if (extension_guid_filter) {
		DEBUGADD(10,("gpo_process_a_gpo: using filter %s (%s)\n",
			extension_guid_filter,
			cse_gpo_guid_string_to_name(extension_guid_filter)));
	}

	if (!gpo_get_gp_ext_from_gpo(mem_ctx, flags, gpo, &gp_ext)) {
		return ADS_ERROR_NT(NT_STATUS_INVALID_PARAMETER);
	}

	if (!gp_ext || !gp_ext->num_exts) {
		if (flags & GPO_INFO_FLAG_VERBOSE) {
			DEBUG(0,("gpo_process_a_gpo: "
				"no policies in %s (%s) for this extension\n",
				gpo->name, gpo->display_name));
		}
		return ADS_SUCCESS;
	}

	for (i=0; i<gp_ext->num_exts; i++) {

		NTSTATUS ntstatus;

		if (extension_guid_filter &&
		    !strequal(extension_guid_filter,
			      gp_ext->extensions_guid[i])) {
			continue;
		}

		ntstatus = gpext_process_extension(ads, mem_ctx,
						   flags, token, root_key, gpo,
						   gp_ext->extensions_guid[i],
						   gp_ext->snapins_guid[i]);
		if (!NT_STATUS_IS_OK(ntstatus)) {
			ADS_ERROR_NT(ntstatus);
		}
	}

	return ADS_SUCCESS;
}

/****************************************************************
****************************************************************/

static ADS_STATUS gpo_process_gpo_list_by_ext(ADS_STRUCT *ads,
					      TALLOC_CTX *mem_ctx,
					      const struct security_token *token,
					      struct registry_key *root_key,
					      struct GROUP_POLICY_OBJECT *gpo_list,
					      const char *extensions_guid,
					      uint32_t flags)
{
	ADS_STATUS status;
	struct GROUP_POLICY_OBJECT *gpo;

	for (gpo = gpo_list; gpo; gpo = gpo->next) {

		if (gpo->link_type == GP_LINK_LOCAL) {
			continue;
		}


		/* FIXME: we need to pass down the *list* down to the
		 * extension, otherwise we cannot store the e.g. the *list* of
		 * logon-scripts correctly (for more then one GPO) */

		status = gpo_process_a_gpo(ads, mem_ctx, token, root_key,
					   gpo, extensions_guid, flags);

		if (!ADS_ERR_OK(status)) {
			DEBUG(0,("failed to process gpo by ext: %s\n",
				ads_errstr(status)));
			return status;
		}
	}

	return ADS_SUCCESS;
}

/****************************************************************
****************************************************************/

ADS_STATUS gpo_process_gpo_list(ADS_STRUCT *ads,
				TALLOC_CTX *mem_ctx,
				const struct security_token *token,
				struct GROUP_POLICY_OBJECT *gpo_list,
				const char *extensions_guid_filter,
				uint32_t flags)
{
	ADS_STATUS status = ADS_SUCCESS;
	struct gp_extension *gp_ext_list = NULL;
	struct gp_extension *gp_ext = NULL;
	struct registry_key *root_key = NULL;
	struct gp_registry_context *reg_ctx = NULL;
#if 0
	WERROR werr;
#endif
	status = ADS_ERROR_NT(init_gp_extensions(mem_ctx));
	if (!ADS_ERR_OK(status)) {
		return status;
	}

	gp_ext_list = get_gp_extension_list();
	if (!gp_ext_list) {
		return ADS_ERROR_NT(NT_STATUS_DLL_INIT_FAILED);
	}
/* FIXME Needs to be replaced with new patchfile_preg calls */
#if 0
	/* get the key here */
	if (flags & GPO_LIST_FLAG_MACHINE) {
		werr = gp_init_reg_ctx(mem_ctx, KEY_HKLM, REG_KEY_WRITE,
				       get_system_token(),
				       &reg_ctx);
	} else {
		werr = gp_init_reg_ctx(mem_ctx, KEY_HKCU, REG_KEY_WRITE,
				       token,
				       &reg_ctx);
	}
	if (!W_ERROR_IS_OK(werr)) {
		talloc_free(reg_ctx);
		return ADS_ERROR_NT(werror_to_ntstatus(werr));
	}
#endif

	root_key = reg_ctx->curr_key;

	for (gp_ext = gp_ext_list; gp_ext; gp_ext = gp_ext->next) {

		const char *guid_str = NULL;

		guid_str = GUID_string(mem_ctx, gp_ext->guid);
		if (!guid_str) {
			status = ADS_ERROR_NT(NT_STATUS_NO_MEMORY);
			goto done;
		}

		if (extensions_guid_filter &&
		    (!strequal(guid_str, extensions_guid_filter)))  {
			continue;
		}

		DEBUG(0,("-------------------------------------------------\n"));
		DEBUG(0,("gpo_process_gpo_list: processing ext: %s {%s}\n",
			gp_ext->name, guid_str));


		status = gpo_process_gpo_list_by_ext(ads, mem_ctx, token,
						     root_key, gpo_list,
						     guid_str, flags);
		if (!ADS_ERR_OK(status)) {
			goto done;
		}
	}

 done:
	talloc_free(reg_ctx);
	talloc_free(root_key);
	free_gp_extensions();

	return status;
}


/****************************************************************
 check wether the version number in a GROUP_POLICY_OBJECT match those of the
 locally stored version. If not, fetch the required policy via CIFS
****************************************************************/

NTSTATUS check_refresh_gpo(ADS_STRUCT *ads,
			   TALLOC_CTX *mem_ctx,
                           const char *cache_dir,
                           struct loadparm_context *lp_ctx,
			   uint32_t flags,
			   struct GROUP_POLICY_OBJECT *gpo)
{
	NTSTATUS result;
	char *server = NULL;
	char *share = NULL;
	char *nt_path = NULL;
	char *unix_path = NULL;
	uint32_t sysvol_gpt_version = 0;
	char *display_name = NULL;

	result = gpo_explode_filesyspath(mem_ctx, cache_dir, gpo->file_sys_path,
					 &server, &share, &nt_path, &unix_path);

	if (!NT_STATUS_IS_OK(result)) {
		goto out;
	}

	result = gpo_get_sysvol_gpt_version(mem_ctx,
					    unix_path,
					    &sysvol_gpt_version,
					    &display_name);
	if (!NT_STATUS_IS_OK(result) &&
	    !NT_STATUS_EQUAL(result, NT_STATUS_NO_SUCH_FILE)) {
		DEBUG(10,("check_refresh_gpo: "
			"failed to get local gpt version: %s\n",
			nt_errstr(result)));
		goto out;
	}

	DEBUG(10,("check_refresh_gpo: versions gpo %d sysvol %d\n",
		gpo->version, sysvol_gpt_version));

	/* FIXME: handle GPO_INFO_FLAG_FORCED_REFRESH from flags */

	while (gpo->version > sysvol_gpt_version) {

		DEBUG(1,("check_refresh_gpo: need to refresh GPO\n"));

		result = gpo_fetch_files(mem_ctx, ads, lp_ctx, cache_dir, gpo);
		if (!NT_STATUS_IS_OK(result)) {
			goto out;
		}

		result = gpo_get_sysvol_gpt_version(mem_ctx,
						    unix_path,
						    &sysvol_gpt_version,
						    &display_name);
		if (!NT_STATUS_IS_OK(result)) {
			DEBUG(10,("check_refresh_gpo: "
				"failed to get local gpt version: %s\n",
				nt_errstr(result)));
			goto out;
		}

		if (gpo->version == sysvol_gpt_version) {
			break;
		}
	}

	DEBUG(10,("Name:\t\t\t%s (%s)\n", gpo->display_name, gpo->name));
	DEBUGADD(10,("sysvol GPT version:\t%d (user: %d, machine: %d)\n",
		sysvol_gpt_version,
		GPO_VERSION_USER(sysvol_gpt_version),
		GPO_VERSION_MACHINE(sysvol_gpt_version)));
	DEBUGADD(10,("LDAP GPO version:\t%d (user: %d, machine: %d)\n",
		gpo->version,
		GPO_VERSION_USER(gpo->version),
		GPO_VERSION_MACHINE(gpo->version)));
	DEBUGADD(10,("LDAP GPO link:\t\t%s\n", gpo->link));

	result = NT_STATUS_OK;

 out:
	return result;

}

/****************************************************************
 check wether the version numbers in the gpo_list match the locally stored, if
 not, go and get each required GPO via CIFS
 ****************************************************************/

NTSTATUS check_refresh_gpo_list(ADS_STRUCT *ads,
				TALLOC_CTX *mem_ctx,
				const char *cache_dir,
                                struct loadparm_context *lp_ctx,
				uint32_t flags,
				struct GROUP_POLICY_OBJECT *gpo_list)
{
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;
	struct GROUP_POLICY_OBJECT *gpo;

	if (!gpo_list) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	for (gpo = gpo_list; gpo; gpo = gpo->next) {

		result = check_refresh_gpo(ads, mem_ctx, cache_dir, lp_ctx, flags, gpo);
		if (!NT_STATUS_IS_OK(result)) {
			goto out;
		}
	}

	result = NT_STATUS_OK;

 out:
	/* FIXME close cli connection */

	return result;
}

/****************************************************************
****************************************************************/

NTSTATUS gpo_get_unix_path(TALLOC_CTX *mem_ctx,
                           const char *cache_dir,
			   struct GROUP_POLICY_OBJECT *gpo,
			   char **unix_path)
{
	char *server, *share, *nt_path;
	return gpo_explode_filesyspath(mem_ctx, cache_dir, gpo->file_sys_path,
				       &server, &share, &nt_path, unix_path);
}

/****************************************************************
****************************************************************/

char *gpo_flag_str(TALLOC_CTX *ctx, uint32_t flags)
{
	char *str = NULL;

	if (flags == 0) {
		return NULL;
	}

	if (flags & GPO_INFO_FLAG_SLOWLINK)
		str = talloc_append_string(ctx, str, "GPO_INFO_FLAG_SLOWLINK ");
	if (flags & GPO_INFO_FLAG_VERBOSE)
		str = talloc_append_string(ctx, str, "GPO_INFO_FLAG_VERBOSE ");
	if (flags & GPO_INFO_FLAG_SAFEMODE_BOOT)
		str = talloc_append_string(ctx, str, "GPO_INFO_FLAG_SAFEMODE_BOOT ");
	if (flags & GPO_INFO_FLAG_NOCHANGES)
		str = talloc_append_string(ctx, str, "GPO_INFO_FLAG_NOCHANGES ");
	if (flags & GPO_INFO_FLAG_MACHINE)
		str = talloc_append_string(ctx, str, "GPO_INFO_FLAG_MACHINE ");
	if (flags & GPO_INFO_FLAG_LOGRSOP_TRANSITION)
		str = talloc_append_string(ctx, str, "GPO_INFO_FLAG_LOGRSOP_TRANSITION ");
	if (flags & GPO_INFO_FLAG_LINKTRANSITION)
		str = talloc_append_string(ctx, str, "GPO_INFO_FLAG_LINKTRANSITION ");
	if (flags & GPO_INFO_FLAG_FORCED_REFRESH)
		str = talloc_append_string(ctx, str, "GPO_INFO_FLAG_FORCED_REFRESH ");
	if (flags & GPO_INFO_FLAG_BACKGROUND)
		str = talloc_append_string(ctx, str, "GPO_INFO_FLAG_BACKGROUND ");

	return str;
}

/****************************************************************
****************************************************************/

NTSTATUS gp_find_file(TALLOC_CTX *mem_ctx,
		      uint32_t flags,
		      const char *filename,
		      const char *suffix,
		      const char **filename_out)
{
	const char *tmp = NULL;
	struct stat sbuf;
	const char *path = NULL;

	if (flags & GPO_LIST_FLAG_MACHINE) {
		path = "Machine";
	} else {
		path = "User";
	}

	tmp = talloc_asprintf(mem_ctx, "%s/%s/%s", filename,
			      path, suffix);
	NT_STATUS_HAVE_NO_MEMORY(tmp);

	if (stat(tmp, &sbuf) == 0) {
		*filename_out = tmp;
		return NT_STATUS_OK;
	}

	path = talloc_strdup_upper(mem_ctx, path);
	NT_STATUS_HAVE_NO_MEMORY(path);

	tmp = talloc_asprintf(mem_ctx, "%s/%s/%s", filename,
			      path, suffix);
	NT_STATUS_HAVE_NO_MEMORY(tmp);

	if (stat(tmp, &sbuf) == 0) {
		*filename_out = tmp;
		return NT_STATUS_OK;
	}

	return NT_STATUS_NO_SUCH_FILE;
}

/****************************************************************
****************************************************************/

ADS_STATUS gp_get_machine_token(ADS_STRUCT *ads,
				TALLOC_CTX *mem_ctx,
				struct loadparm_context *lp_ctx,
				const char *dn,
				struct security_token **token)
{
#ifdef HAVE_ADS
	struct security_token *ad_token = NULL;
	ADS_STATUS status;
#if _SAMBA_BUILD_ == 4
	struct auth_session_info *info;
#else
	NTSTATUS ntstatus;
#endif

	status = ads_get_sid_token(ads, mem_ctx, dn, &ad_token);
	if (!ADS_ERR_OK(status)) {
		return status;
	}
#if _SAMBA_BUILD_ == 4
	info = system_session(mem_ctx, lp_ctx);
	*token = info->security_token;
#else
	ntstatus = merge_nt_token(mem_ctx, ad_token, get_system_token(),
				  token);
	if (!NT_STATUS_IS_OK(ntstatus)) {
		return ADS_ERROR_NT(ntstatus);
	}
#endif
	return ADS_SUCCESS;
#else
	return ADS_ERROR_NT(NT_STATUS_NOT_SUPPORTED);
#endif
}
