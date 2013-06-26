/* 
   Unix SMB/CIFS implementation.
   Samba utility functions
   Copyright (C) Jelmer Vernooij <jelmer@samba.org> 2008
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _PROVISION_H_
#define _PROVISION_H_

struct provision_settings {
	const char *site_name;
	const char *root_dn_str; 
	const char *domain_dn_str;
	const char *config_dn_str;
	const char *schema_dn_str;
	const char *server_dn_str;
	const struct GUID *invocation_id;
	const char *netbios_name;
	const char *host_ip;
	const char *realm;
	const char *domain;
	const char *ntds_dn_str;
	const char *machine_password;
	const char *targetdir;
};

/* FIXME: Rename this to hostconfig ? */
struct provision_result {
	const char *domaindn;
        struct ldb_context *samdb;
        struct loadparm_context *lp_ctx;
};

struct provision_store_self_join_settings {
	const char *domain_name;
	const char *realm;
	const char *netbios_name;
	enum netr_SchannelType secure_channel_type;
	const char *machine_password;
	int key_version_number;
	struct dom_sid *domain_sid;
};

NTSTATUS provision_bare(TALLOC_CTX *mem_ctx, struct loadparm_context *lp_ctx,
						struct provision_settings *settings,
						struct provision_result *result);

NTSTATUS provision_store_self_join(TALLOC_CTX *mem_ctx, struct loadparm_context *lp_ctx,
				   struct tevent_context *ev_ctx,
				   struct provision_store_self_join_settings *settings,
				   const char **error_string);

struct ldb_context *provision_get_schema(TALLOC_CTX *mem_ctx, struct loadparm_context *lp_ctx,
					 DATA_BLOB *override_prefixmap);

#endif /* _PROVISION_H_ */
