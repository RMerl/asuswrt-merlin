/*
   Unix SMB/CIFS implementation.
   ads (active directory) utility library
   Copyright (C) Andrew Tridgell 2001
   Copyright (C) Remus Koos 2001
   Copyright (C) Jim McDonough <jmcd@us.ibm.com> 2002
   Copyright (C) Guenther Deschner 2005
   Copyright (C) Gerald Carter 2006

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

#ifndef _LIBADS_ADS_LDAP_PROTOS_H_
#define _LIBADS_ADS_LDAP_PROTOS_H_

/*
 * Prototypes for ads
 */

void ads_msgfree(ADS_STRUCT *ads, LDAPMessage *msg);
char *ads_get_dn(ADS_STRUCT *ads, TALLOC_CTX *mem_ctx, LDAPMessage *msg);

char *ads_pull_string(ADS_STRUCT *ads, TALLOC_CTX *mem_ctx, LDAPMessage *msg,
		      const char *field);
char **ads_pull_strings(ADS_STRUCT *ads, TALLOC_CTX *mem_ctx,
			LDAPMessage *msg, const char *field,
			size_t *num_values);
char **ads_pull_strings_range(ADS_STRUCT *ads,
			      TALLOC_CTX *mem_ctx,
			      LDAPMessage *msg, const char *field,
			      char **current_strings,
			      const char **next_attribute,
			      size_t *num_strings,
			      bool *more_strings);
bool ads_pull_uint32(ADS_STRUCT *ads, LDAPMessage *msg, const char *field,
		     uint32 *v);
bool ads_pull_guid(ADS_STRUCT *ads, LDAPMessage *msg, struct GUID *guid);
bool ads_pull_sid(ADS_STRUCT *ads, LDAPMessage *msg, const char *field,
		  struct dom_sid *sid);
int ads_pull_sids(ADS_STRUCT *ads, TALLOC_CTX *mem_ctx,
		  LDAPMessage *msg, const char *field, struct dom_sid **sids);
bool ads_pull_sd(ADS_STRUCT *ads, TALLOC_CTX *mem_ctx,
		 LDAPMessage *msg, const char *field, struct security_descriptor **sd);
char *ads_pull_username(ADS_STRUCT *ads, TALLOC_CTX *mem_ctx,
			LDAPMessage *msg);
int ads_pull_sids_from_extendeddn(ADS_STRUCT *ads,
				  TALLOC_CTX *mem_ctx,
				  LDAPMessage *msg,
				  const char *field,
				  enum ads_extended_dn_flags flags,
				  struct dom_sid **sids);

ADS_STATUS ads_find_machine_acct(ADS_STRUCT *ads, LDAPMessage **res,
				 const char *machine);
ADS_STATUS ads_find_printer_on_server(ADS_STRUCT *ads, LDAPMessage **res,
				      const char *printer,
				      const char *servername);
ADS_STATUS ads_find_printers(ADS_STRUCT *ads, LDAPMessage **res);
ADS_STATUS ads_find_user_acct(ADS_STRUCT *ads, LDAPMessage **res,
			      const char *user);

ADS_STATUS ads_do_search(ADS_STRUCT *ads, const char *bind_path, int scope,
			 const char *expr,
			 const char **attrs, LDAPMessage **res);
ADS_STATUS ads_search(ADS_STRUCT *ads, LDAPMessage **res,
		      const char *expr, const char **attrs);
ADS_STATUS ads_search_dn(ADS_STRUCT *ads, LDAPMessage **res,
			 const char *dn, const char **attrs);
ADS_STATUS ads_do_search_all_args(ADS_STRUCT *ads, const char *bind_path,
				  int scope, const char *expr,
				  const char **attrs, void *args,
				  LDAPMessage **res);
ADS_STATUS ads_do_search_all(ADS_STRUCT *ads, const char *bind_path,
			     int scope, const char *expr,
			     const char **attrs, LDAPMessage **res);
ADS_STATUS ads_do_search_retry(ADS_STRUCT *ads, const char *bind_path,
			       int scope,
			       const char *expr,
			       const char **attrs, LDAPMessage **res);
ADS_STATUS ads_search_retry(ADS_STRUCT *ads, LDAPMessage **res,
			    const char *expr, const char **attrs);
ADS_STATUS ads_search_retry_dn(ADS_STRUCT *ads, LDAPMessage **res,
			       const char *dn,
			       const char **attrs);
ADS_STATUS ads_search_retry_extended_dn_ranged(ADS_STRUCT *ads, TALLOC_CTX *mem_ctx,
						const char *dn,
						const char **attrs,
						enum ads_extended_dn_flags flags,
						char ***strings,
						size_t *num_strings);
ADS_STATUS ads_search_retry_sid(ADS_STRUCT *ads, LDAPMessage **res,
				const struct dom_sid *sid,
				const char **attrs);


LDAPMessage *ads_first_entry(ADS_STRUCT *ads, LDAPMessage *res);
LDAPMessage *ads_next_entry(ADS_STRUCT *ads, LDAPMessage *res);
LDAPMessage *ads_first_message(ADS_STRUCT *ads, LDAPMessage *res);
LDAPMessage *ads_next_message(ADS_STRUCT *ads, LDAPMessage *res);
void ads_process_results(ADS_STRUCT *ads, LDAPMessage *res,
			 bool (*fn)(ADS_STRUCT *,char *, void **, void *),
			 void *data_area);
void ads_dump(ADS_STRUCT *ads, LDAPMessage *res);

struct GROUP_POLICY_OBJECT;
ADS_STATUS ads_parse_gpo(ADS_STRUCT *ads,
			 TALLOC_CTX *mem_ctx,
			 LDAPMessage *res,
			 const char *gpo_dn,
			 struct GROUP_POLICY_OBJECT *gpo);
ADS_STATUS ads_search_retry_dn_sd_flags(ADS_STRUCT *ads, LDAPMessage **res,
					 uint32 sd_flags,
					 const char *dn,
					 const char **attrs);
ADS_STATUS ads_do_search_all_sd_flags(ADS_STRUCT *ads, const char *bind_path,
				       int scope, const char *expr,
				       const char **attrs, uint32 sd_flags,
				       LDAPMessage **res);
ADS_STATUS ads_get_tokensids(ADS_STRUCT *ads,
			      TALLOC_CTX *mem_ctx,
			      const char *dn,
			      struct dom_sid *user_sid,
			      struct dom_sid *primary_group_sid,
			      struct dom_sid **sids,
			      size_t *num_sids);
ADS_STATUS ads_get_joinable_ous(ADS_STRUCT *ads,
				TALLOC_CTX *mem_ctx,
				char ***ous,
				size_t *num_ous);

#endif /* _LIBADS_ADS_LDAP_PROTOS_H_ */
