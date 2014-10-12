/*
 *  Unix SMB/CIFS implementation.
 *  Group Policy Object Support
 *  Copyright (C) Guenther Deschner 2005-2008 (from samba 3 gpo.h)
 *  Copyright (C) Wilco Baan Hofman 2010
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

#ifndef __POLICY_H__
#define __POLICY_H__
#include "libcli/libcli.h"

#define GPLINK_OPT_DISABLE		(1 << 0)
#define GPLINK_OPT_ENFORCE		(1 << 1)


#define GPO_FLAG_USER_DISABLE		(1 << 0)
#define GPO_FLAG_MACHINE_DISABLE	(1 << 1)

struct security_token;

enum gpo_inheritance {
	GPO_INHERIT = 0,
	GPO_BLOCK_INHERITANCE = 1,
};

struct gp_context {
	struct ldb_context *ldb_ctx;
	struct loadparm_context *lp_ctx;
	struct cli_credentials *credentials;
	struct tevent_context *ev_ctx;
	struct smbcli_state *cli;
	struct nbt_dc_name active_dc;
};

struct gp_object {
	uint32_t version;
	uint32_t flags;
	const char *display_name;
	const char *name;
	const char *dn;
	const char *file_sys_path;
	struct security_descriptor *security_descriptor;
};


struct gp_link {
	uint32_t options;
	const char *dn;
};

struct gp_ini_param {
	char *name;
	char *value;
};

struct gp_ini_section {
	char *name;
	uint16_t num_params;
	struct gp_ini_param *params;
};

struct gp_ini_context {
	uint16_t num_sections;
	struct gp_ini_section *sections;
};

NTSTATUS gp_init(TALLOC_CTX *mem_ctx,
				struct loadparm_context *lp_ctx,
				struct cli_credentials *creds,
				struct tevent_context *ev_ctx,
				struct gp_context **gp_ctx);


/* LDAP functions */
NTSTATUS gp_list_all_gpos(struct gp_context *gp_ctx, struct gp_object ***ret);
NTSTATUS gp_get_gplinks(struct gp_context *gp_ctx, const char *req_dn, struct gp_link ***ret);
NTSTATUS gp_list_gpos(struct gp_context *gp_ctx, struct security_token *token, const char ***ret);

NTSTATUS gp_get_gpo_info(struct gp_context *gp_ctx, const char *dn_str, struct gp_object **ret);
NTSTATUS gp_set_gpo_info(struct gp_context *gp_ctx, const char *dn_str, struct gp_object *gpo);
NTSTATUS gp_del_gpo(struct gp_context *gp_ctx, const char *dn_str);


NTSTATUS gp_get_gplink_options(TALLOC_CTX *mem_ctx, uint32_t flags, const char ***ret);
NTSTATUS gp_get_gpo_flags(TALLOC_CTX *mem_ctx, uint32_t flags, const char ***ret);

NTSTATUS gp_set_gplink(struct gp_context *gp_ctx, const char *dn_str, struct gp_link *gplink);
NTSTATUS gp_del_gplink(struct gp_context *gp_ctx, const char *dn_str, const char *gp_dn);
NTSTATUS gp_get_inheritance(struct gp_context *gp_ctx, const char *dn_str, enum gpo_inheritance *inheritance);
NTSTATUS gp_set_inheritance(struct gp_context *gp_ctx, const char *dn_str, enum gpo_inheritance inheritance);

NTSTATUS gp_create_ldap_gpo(struct gp_context *gp_ctx, struct gp_object *gpo);
NTSTATUS gp_set_ads_acl (struct gp_context *gp_ctx, const char *dn_str, const struct security_descriptor *sd);
NTSTATUS gp_push_gpo (struct gp_context *gp_ctx, const char *local_path, struct gp_object *gpo);
NTSTATUS gp_set_ldap_gpo(struct gp_context *gp_ctx, struct gp_object *gpo);

/* File system functions */
NTSTATUS gp_fetch_gpt (struct gp_context *gp_ctx, struct gp_object *gpo, const char **path);
NTSTATUS gp_create_gpt(struct gp_context *gp_ctx, const char *name, const char *file_sys_path);
NTSTATUS gp_set_gpt_security_descriptor(struct gp_context *gp_ctx, struct gp_object *gpo, struct security_descriptor *sd);
NTSTATUS gp_push_gpt(struct gp_context *gp_ctx, const char *local_path,
                     const char *file_sys_path);

/* Ini functions */
NTSTATUS gp_parse_ini(TALLOC_CTX *mem_ctx, struct gp_context *gp_ctx, const char *filename, struct gp_ini_context **ret);
NTSTATUS gp_get_ini_string(struct gp_ini_context *ini, const char *section, const char *name, char **ret);
NTSTATUS gp_get_ini_uint(struct gp_ini_context *ini, const char *section, const char *name, uint32_t *ret);

/* Managing functions */
NTSTATUS gp_create_gpo (struct gp_context *gp_ctx, const char *display_name, struct gp_object **ret);
NTSTATUS gp_create_gpt_security_descriptor (TALLOC_CTX *mem_ctx, struct security_descriptor *ds_sd, struct security_descriptor **ret);
NTSTATUS gp_set_acl (struct gp_context *gp_ctx, const char *dn_str, const struct security_descriptor *sd);

#endif
