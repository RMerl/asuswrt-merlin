/*
 *  Unix SMB/CIFS implementation.
 *  ads (active directory) utility library
 *
 *  Copyright (C) Andrew Bartlett			2001
 *  Copyright (C) Andrew Tridgell			2001
 *  Copyright (C) Remus Koos (remuskoos@yahoo.com)	2001
 *  Copyright (C) Alexey Kotovich			2002
 *  Copyright (C) Jim McDonough <jmcd@us.ibm.com>	2002-2003
 *  Copyright (C) Luke Howard				2003
 *  Copyright (C) Guenther Deschner			2003-2008
 *  Copyright (C) Rakesh Patel				2004
 *  Copyright (C) Dan Perry				2004
 *  Copyright (C) Jeremy Allison			2004
 *  Copyright (C) Gerald Carter				2006
 *  Copyright (C) Stefan Metzmacher			2007
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _LIBADS_ADS_PROTO_H_
#define _LIBADS_ADS_PROTO_H_

/* The following definitions come from libads/ads_struct.c  */

char *ads_build_path(const char *realm, const char *sep, const char *field, int reverse);
char *ads_build_dn(const char *realm);
char *ads_build_domain(const char *dn);
ADS_STRUCT *ads_init(const char *realm,
		     const char *workgroup,
		     const char *ldap_server);
bool ads_set_sasl_wrap_flags(ADS_STRUCT *ads, int flags);
void ads_destroy(ADS_STRUCT **ads);

/* The following definitions come from libads/disp_sec.c  */

void ads_disp_sd(ADS_STRUCT *ads, TALLOC_CTX *mem_ctx, struct security_descriptor *sd);

/* The following definitions come from libads/kerberos_keytab.c  */

int ads_keytab_add_entry(ADS_STRUCT *ads, const char *srvPrinc);
int ads_keytab_flush(ADS_STRUCT *ads);
int ads_keytab_create_default(ADS_STRUCT *ads);
int ads_keytab_list(const char *keytab_name);

/* The following definitions come from libads/krb5_errs.c  */

/* The following definitions come from libads/kerberos_util.c  */

ADS_STATUS ads_set_machine_password(ADS_STRUCT *ads,
				    const char *machine_account,
				    const char *password);
int ads_kinit_password(ADS_STRUCT *ads);

/* The following definitions come from libads/ldap.c  */

bool ads_sitename_match(ADS_STRUCT *ads);
bool ads_closest_dc(ADS_STRUCT *ads);
ADS_STATUS ads_connect(ADS_STRUCT *ads);
ADS_STATUS ads_connect_user_creds(ADS_STRUCT *ads);
ADS_STATUS ads_connect_gc(ADS_STRUCT *ads);
void ads_disconnect(ADS_STRUCT *ads);
ADS_STATUS ads_do_search_all_fn(ADS_STRUCT *ads, const char *bind_path,
				int scope, const char *expr, const char **attrs,
				bool (*fn)(ADS_STRUCT *, char *, void **, void *),
				void *data_area);
char *ads_parent_dn(const char *dn);
ADS_MODLIST ads_init_mods(TALLOC_CTX *ctx);
ADS_STATUS ads_mod_str(TALLOC_CTX *ctx, ADS_MODLIST *mods,
		       const char *name, const char *val);
ADS_STATUS ads_mod_strlist(TALLOC_CTX *ctx, ADS_MODLIST *mods,
			   const char *name, const char **vals);
ADS_STATUS ads_gen_mod(ADS_STRUCT *ads, const char *mod_dn, ADS_MODLIST mods);
ADS_STATUS ads_gen_add(ADS_STRUCT *ads, const char *new_dn, ADS_MODLIST mods);
ADS_STATUS ads_del_dn(ADS_STRUCT *ads, char *del_dn);
char *ads_ou_string(ADS_STRUCT *ads, const char *org_unit);
char *ads_default_ou_string(ADS_STRUCT *ads, const char *wknguid);
ADS_STATUS ads_add_strlist(TALLOC_CTX *ctx, ADS_MODLIST *mods,
				const char *name, const char **vals);
uint32 ads_get_kvno(ADS_STRUCT *ads, const char *account_name);
uint32_t ads_get_machine_kvno(ADS_STRUCT *ads, const char *machine_name);
ADS_STATUS ads_clear_service_principal_names(ADS_STRUCT *ads, const char *machine_name);
ADS_STATUS ads_add_service_principal_name(ADS_STRUCT *ads, const char *machine_name,
                                          const char *my_fqdn, const char *spn);
ADS_STATUS ads_create_machine_acct(ADS_STRUCT *ads, const char *machine_name,
                                   const char *org_unit);
ADS_STATUS ads_move_machine_acct(ADS_STRUCT *ads, const char *machine_name,
                                 const char *org_unit, bool *moved);
int ads_count_replies(ADS_STRUCT *ads, void *res);
ADS_STATUS ads_USN(ADS_STRUCT *ads, uint32 *usn);
ADS_STATUS ads_current_time(ADS_STRUCT *ads);
ADS_STATUS ads_domain_func_level(ADS_STRUCT *ads, uint32 *val);
ADS_STATUS ads_domain_sid(ADS_STRUCT *ads, struct dom_sid *sid);
ADS_STATUS ads_site_dn(ADS_STRUCT *ads, TALLOC_CTX *mem_ctx, const char **site_name);
ADS_STATUS ads_site_dn_for_machine(ADS_STRUCT *ads, TALLOC_CTX *mem_ctx, const char *computer_name, const char **site_dn);
ADS_STATUS ads_upn_suffixes(ADS_STRUCT *ads, TALLOC_CTX *mem_ctx, char ***suffixes, size_t *num_suffixes);
ADS_STATUS ads_get_joinable_ous(ADS_STRUCT *ads,
				TALLOC_CTX *mem_ctx,
				char ***ous,
				size_t *num_ous);
ADS_STATUS ads_get_sid_from_extended_dn(TALLOC_CTX *mem_ctx,
					const char *extended_dn,
					enum ads_extended_dn_flags flags,
					struct dom_sid *sid);
char* ads_get_dnshostname( ADS_STRUCT *ads, TALLOC_CTX *ctx, const char *machine_name );
char* ads_get_upn( ADS_STRUCT *ads, TALLOC_CTX *ctx, const char *machine_name );
char* ads_get_samaccountname( ADS_STRUCT *ads, TALLOC_CTX *ctx, const char *machine_name );
ADS_STATUS ads_join_realm(ADS_STRUCT *ads, const char *machine_name,
			uint32 account_type, const char *org_unit);
ADS_STATUS ads_leave_realm(ADS_STRUCT *ads, const char *hostname);
ADS_STATUS ads_find_samaccount(ADS_STRUCT *ads,
			       TALLOC_CTX *mem_ctx,
			       const char *samaccountname,
			       uint32 *uac_ret,
			       const char **dn_ret);
ADS_STATUS ads_config_path(ADS_STRUCT *ads,
			   TALLOC_CTX *mem_ctx,
			   char **config_path);
const char *ads_get_extended_right_name_by_guid(ADS_STRUCT *ads,
						const char *config_path,
						TALLOC_CTX *mem_ctx,
						const struct GUID *rights_guid);
ADS_STATUS ads_check_ou_dn(TALLOC_CTX *mem_ctx,
			   ADS_STRUCT *ads,
			   const char **account_ou);

/* The following definitions come from libads/ldap_printer.c  */

ADS_STATUS ads_mod_printer_entry(ADS_STRUCT *ads, char *prt_dn,
				 TALLOC_CTX *ctx, const ADS_MODLIST *mods);
ADS_STATUS ads_add_printer_entry(ADS_STRUCT *ads, char *prt_dn,
					TALLOC_CTX *ctx, ADS_MODLIST *mods);
WERROR get_remote_printer_publishing_data(struct rpc_pipe_client *cli,
					  TALLOC_CTX *mem_ctx,
					  ADS_MODLIST *mods,
					  const char *printer);

/* The following definitions come from libads/ldap_user.c  */

ADS_STATUS ads_add_user_acct(ADS_STRUCT *ads, const char *user,
			     const char *container, const char *fullname);
ADS_STATUS ads_add_group_acct(ADS_STRUCT *ads, const char *group,
			      const char *container, const char *comment);

/* The following definitions come from libads/ldap_utils.c  */

ADS_STATUS ads_ranged_search(ADS_STRUCT *ads,
			     TALLOC_CTX *mem_ctx,
			     int scope,
			     const char *base,
			     const char *filter,
			     void *args,
			     const char *range_attr,
			     char ***strings,
			     size_t *num_strings);

/* The following definitions come from libads/ndr.c  */

struct ndr_print;
void ndr_print_ads_struct(struct ndr_print *ndr, const char *name, const struct ads_struct *r);

/* The following definitions come from libads/sasl.c  */

ADS_STATUS ads_sasl_bind(ADS_STRUCT *ads);

/* The following definitions come from libads/sasl_wrapping.c  */

ADS_STATUS ads_setup_sasl_wrapping(ADS_STRUCT *ads,
				   const struct ads_saslwrap_ops *ops,
				   void *private_data);
ADS_STATUS ads_setup_sasl_wrapping(ADS_STRUCT *ads,
				   const struct ads_saslwrap_ops *ops,
				   void *private_data);

/* The following definitions come from libads/util.c  */

ADS_STATUS ads_change_trust_account_password(ADS_STRUCT *ads, char *host_principal);

#endif /* _LIBADS_ADS_PROTO_H_ */
