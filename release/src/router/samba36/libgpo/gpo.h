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

#ifndef __GPO_H__
#define __GPO_H__

#if _SAMBA_BUILD_ == 4
#include "source4/libgpo/ads_convenience.h"
#else
struct loadparm_context;
#include "ads.h"
#endif

enum GPO_LINK_TYPE {
	GP_LINK_UNKOWN	= 0,
	GP_LINK_MACHINE	= 1,
	GP_LINK_SITE	= 2,
	GP_LINK_DOMAIN	= 3,
	GP_LINK_OU	= 4,
	GP_LINK_LOCAL	= 5 /* for convenience */
};

/* GPO_OPTIONS */
#define GPO_FLAG_DISABLE	0x00000001
#define GPO_FLAG_FORCE		0x00000002

/* GPO_LIST_FLAGS */
#define GPO_LIST_FLAG_MACHINE	0x00000001
#define GPO_LIST_FLAG_SITEONLY	0x00000002

/* following flags from http://support.microsoft.com/kb/312164/EN-US/ */
#define GPO_INFO_FLAG_MACHINE			0x00000001
#define GPO_INFO_FLAG_BACKGROUND	 	0x00000010
#define GPO_INFO_FLAG_SLOWLINK			0x00000020
#define GPO_INFO_FLAG_VERBOSE			0x00000040
#define GPO_INFO_FLAG_NOCHANGES			0x00000080
#define GPO_INFO_FLAG_LINKTRANSITION		0x00000100
#define GPO_INFO_FLAG_LOGRSOP_TRANSITION	0x00000200
#define GPO_INFO_FLAG_FORCED_REFRESH		0x00000400
#define GPO_INFO_FLAG_SAFEMODE_BOOT		0x00000800

#define GPO_VERSION_USER(x) (x >> 16)
#define GPO_VERSION_MACHINE(x) (x & 0xffff)

struct GROUP_POLICY_OBJECT {
	uint32_t options;	/* GPFLAGS_* */
	uint32_t version;
	const char *ds_path;
	const char *file_sys_path;
	const char *display_name;
	const char *name;
	const char *link;
	enum GPO_LINK_TYPE link_type;
	const char *user_extensions;
	const char *machine_extensions;
	struct security_descriptor *security_descriptor;
	struct GROUP_POLICY_OBJECT *next, *prev;
};

/* the following is seen on the DS (see adssearch.pl for details) */

/* the type field in a 'gPLink', the same as GPO_FLAG ? */
#define GPO_LINK_OPT_NONE	0x00000000
#define GPO_LINK_OPT_DISABLED	0x00000001
#define GPO_LINK_OPT_ENFORCED	0x00000002

/* GPO_LINK_OPT_ENFORCED takes precedence over GPOPTIONS_BLOCK_INHERITANCE */

/* 'gPOptions', maybe a bitmask as well */
enum GPO_INHERIT {
	GPOPTIONS_INHERIT		= 0,
	GPOPTIONS_BLOCK_INHERITANCE	= 1
};

/* 'flags' in a 'groupPolicyContainer' object */
#define GPFLAGS_ALL_ENABLED			0x00000000
#define GPFLAGS_USER_SETTINGS_DISABLED		0x00000001
#define GPFLAGS_MACHINE_SETTINGS_DISABLED	0x00000002
#define GPFLAGS_ALL_DISABLED (GPFLAGS_USER_SETTINGS_DISABLED | \
			      GPFLAGS_MACHINE_SETTINGS_DISABLED)

struct GP_LINK {
	const char *gp_link;	/* raw link name */
	uint32_t gp_opts;		/* inheritance options GPO_INHERIT */
	uint32_t num_links;	/* number of links */
	char **link_names;	/* array of parsed link names */
	uint32_t *link_opts;	/* array of parsed link opts GPO_LINK_OPT_* */
};

struct GP_EXT {
	const char *gp_extension;	/* raw extension name */
	uint32_t num_exts;
	char **extensions;
	char **extensions_guid;
	char **snapins;
	char **snapins_guid;
	struct GP_EXT *next, *prev;
};

#define GPO_CACHE_DIR "gpo_cache"
#define GPT_INI "GPT.INI"
#define GPO_REFRESH_INTERVAL 60*90

#define GPO_REG_STATE_MACHINE "State\\Machine"

enum gp_reg_action {
	GP_REG_ACTION_NONE = 0,
	GP_REG_ACTION_ADD_VALUE = 1,
	GP_REG_ACTION_ADD_KEY = 2,
	GP_REG_ACTION_DEL_VALUES = 3,
	GP_REG_ACTION_DEL_VALUE = 4,
	GP_REG_ACTION_DEL_ALL_VALUES = 5,
	GP_REG_ACTION_DEL_KEYS = 6,
	GP_REG_ACTION_SEC_KEY_SET = 7,
	GP_REG_ACTION_SEC_KEY_RESET = 8
};

struct gp_registry_entry {
	enum gp_reg_action action;
	const char *key;
	const char *value;
	struct registry_value *data;
};

struct gp_registry_value {
	const char *value;
	struct registry_value *data;
};

struct gp_registry_entry2 {
	enum gp_reg_action action;
	const char *key;
	size_t num_values;
	struct gp_registry_value **values;
};

struct gp_registry_entries {
	size_t num_entries;
	struct gp_registry_entry **entries;
};

struct gp_registry_context {
	const struct security_token *token;
	const char *path;
	struct registry_key *curr_key;
};

#define GP_EXT_GUID_SECURITY "827D319E-6EAC-11D2-A4EA-00C04F79F83A"
#define GP_EXT_GUID_REGISTRY "35378EAC-683F-11D2-A89A-00C04FBBCFA2"
#define GP_EXT_GUID_SCRIPTS  "42B5FAAE-6536-11D2-AE5A-0000F87571E3"
#define ADS_EXTENDED_RIGHT_APPLY_GROUP_POLICY "edacfd8f-ffb3-11d1-b41d-00a0c968f939"


struct cli_state;

/* The following definitions come from libgpo/gpo_fetch.c  */

NTSTATUS gpo_explode_filesyspath(TALLOC_CTX *mem_ctx,
                                 const char *cache_dir,
				 const char *file_sys_path,
				 char **server,
				 char **service,
				 char **nt_path,
				 char **unix_path);
NTSTATUS gpo_fetch_files(TALLOC_CTX *mem_ctx,
                         ADS_STRUCT *ads,
                         struct loadparm_context *lp_ctx,
                         const char *cache_dir,
			 struct GROUP_POLICY_OBJECT *gpo);
NTSTATUS gpo_get_sysvol_gpt_version(TALLOC_CTX *mem_ctx,
				    const char *unix_path,
				    uint32_t *sysvol_version,
				    char **display_name);

/* The following definitions come from libgpo/gpo_ldap.c  */

bool ads_parse_gp_ext(TALLOC_CTX *mem_ctx,
		      const char *extension_raw,
		      struct GP_EXT **gp_ext);
ADS_STATUS ads_get_gpo_link(ADS_STRUCT *ads,
			    TALLOC_CTX *mem_ctx,
			    const char *link_dn,
			    struct GP_LINK *gp_link_struct);
ADS_STATUS ads_add_gpo_link(ADS_STRUCT *ads,
			    TALLOC_CTX *mem_ctx,
			    const char *link_dn,
			    const char *gpo_dn,
			    uint32_t gpo_opt);
ADS_STATUS ads_delete_gpo_link(ADS_STRUCT *ads,
			       TALLOC_CTX *mem_ctx,
			       const char *link_dn,
			       const char *gpo_dn);
ADS_STATUS ads_get_gpo(ADS_STRUCT *ads,
		       TALLOC_CTX *mem_ctx,
		       const char *gpo_dn,
		       const char *display_name,
		       const char *guid_name,
		       struct GROUP_POLICY_OBJECT *gpo);
ADS_STATUS ads_get_sid_token(ADS_STRUCT *ads,
			     TALLOC_CTX *mem_ctx,
			     const char *dn,
			     struct security_token **token);
ADS_STATUS ads_get_gpo_list(ADS_STRUCT *ads,
			    TALLOC_CTX *mem_ctx,
			    const char *dn,
			    uint32_t flags,
			    const struct security_token *token,
			    struct GROUP_POLICY_OBJECT **gpo_list);

/* The following definitions come from libgpo/gpo_sec.c  */

NTSTATUS gpo_apply_security_filtering(const struct GROUP_POLICY_OBJECT *gpo,
				      const struct security_token *token);

/* The following definitions come from libgpo/gpo_util.c  */

const char *cse_gpo_guid_string_to_name(const char *guid);
const char *cse_gpo_name_to_guid_string(const char *name);
const char *cse_snapin_gpo_guid_string_to_name(const char *guid);
void dump_gp_ext(struct GP_EXT *gp_ext, int debuglevel);
void dump_gpo(ADS_STRUCT *ads,
	      TALLOC_CTX *mem_ctx,
	      struct GROUP_POLICY_OBJECT *gpo,
	      int debuglevel);
void dump_gpo_list(ADS_STRUCT *ads,
		   TALLOC_CTX *mem_ctx,
		   struct GROUP_POLICY_OBJECT *gpo_list,
		   int debuglevel);
void dump_gplink(ADS_STRUCT *ads, TALLOC_CTX *mem_ctx, struct GP_LINK *gp_link);
ADS_STATUS gpo_process_a_gpo(ADS_STRUCT *ads,
			     TALLOC_CTX *mem_ctx,
			     const struct security_token *token,
			     struct registry_key *root_key,
			     struct GROUP_POLICY_OBJECT *gpo,
			     const char *extension_guid_filter,
			     uint32_t flags);
ADS_STATUS gpo_process_gpo_list(ADS_STRUCT *ads,
				TALLOC_CTX *mem_ctx,
				const struct security_token *token,
				struct GROUP_POLICY_OBJECT *gpo_list,
				const char *extensions_guid_filter,
				uint32_t flags);
NTSTATUS check_refresh_gpo(ADS_STRUCT *ads,
			   TALLOC_CTX *mem_ctx,
                           const char *cache_dir,
                           struct loadparm_context *lp_ctx,
			   uint32_t flags,
			   struct GROUP_POLICY_OBJECT *gpo);
NTSTATUS check_refresh_gpo_list(ADS_STRUCT *ads,
				TALLOC_CTX *mem_ctx,
                                const char *cache_dir,
                                struct loadparm_context *lp_ctx,
				uint32_t flags,
				struct GROUP_POLICY_OBJECT *gpo_list);
NTSTATUS gpo_get_unix_path(TALLOC_CTX *mem_ctx,
                           const char *cache_dir,
			   struct GROUP_POLICY_OBJECT *gpo,
			   char **unix_path);
char *gpo_flag_str(TALLOC_CTX *mem_ctx, uint32_t flags);
NTSTATUS gp_find_file(TALLOC_CTX *mem_ctx,
		      uint32_t flags,
		      const char *filename,
		      const char *suffix,
		      const char **filename_out);
ADS_STATUS gp_get_machine_token(ADS_STRUCT *ads,
				TALLOC_CTX *mem_ctx,
				struct loadparm_context *lp_ctx,
				const char *dn,
				struct security_token **token);


#include "../libgpo/gpext/gpext.h"

#endif
