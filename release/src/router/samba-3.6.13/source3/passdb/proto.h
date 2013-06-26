/*
 *  Unix SMB/CIFS implementation.
 *  passdb - password and authentication handling
 *
 *  Copyright (C) Andrew Tridgell		1992-1998
 *  Copyright (C) Jeremy Allison 		1995-2009
 *  Copyright (C) Luke Kenneth Casson Leighton	1996-1998
 *  Copyright (C) Jean François Micouleau	1998-2001
 *  Copyright (C) Gerald (Jerry) Carter		2000-2006
 *  Copyright (C) Simo Sorce			2000-2003,2006
 *  Copyright (C) Andrew Bartlett		2001-2002
 *  Copyright (C) Shahms King			2001
 *  Copyright (C) Jelmer Vernooij		2002
 *  Copyright (C) Rafal Szczesniak		2002
 *  Copyright (C) Stefan (metze) Metzmacher	2002-2003
 *  Copyright (C) Guenther Deschner		2004-2005
 *  Copyright (C) Jim McDonough (jmcd@us.ibm.com) 2004-2005
 *  Copyright (C) Vince Brimhall		2004-2005
 *  Copyright (C) Volker Lendecke 		2006
 *  Copyright (C) Michael Adam			2007
 *  Copyright (C) Dan Sledz			2009
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

#ifndef _PASSDB_PROTO_H_
#define _PASSDB_PROTO_H_

/* The following definitions come from passdb/account_pol.c  */

void account_policy_names_list(const char ***names, int *num_names);
const char *decode_account_policy_name(enum pdb_policy_type type);
const char *get_account_policy_attr(enum pdb_policy_type type);
const char *account_policy_get_desc(enum pdb_policy_type type);
enum pdb_policy_type account_policy_name_to_typenum(const char *name);
bool account_policy_get_default(enum pdb_policy_type type, uint32_t *val);
bool init_account_policy(void);
bool account_policy_get(enum pdb_policy_type type, uint32_t *value);
bool account_policy_set(enum pdb_policy_type type, uint32_t value);
bool cache_account_policy_set(enum pdb_policy_type type, uint32_t value);
bool cache_account_policy_get(enum pdb_policy_type type, uint32_t *value);
struct db_context *get_account_pol_db( void );

/* The following definitions come from passdb/login_cache.c  */

bool login_cache_init(void);
bool login_cache_shutdown(void);
bool login_cache_read(struct samu *sampass, struct login_cache *entry);
bool login_cache_write(const struct samu *sampass,
		       const struct login_cache *entry);
bool login_cache_delentry(const struct samu *sampass);

/* The following definitions come from passdb/passdb.c  */

const char *my_sam_name(void);
struct samu *samu_new( TALLOC_CTX *ctx );
NTSTATUS samu_set_unix(struct samu *user, const struct passwd *pwd);
NTSTATUS samu_alloc_rid_unix(struct samu *user, const struct passwd *pwd);
char *pdb_encode_acct_ctrl(uint32_t acct_ctrl, size_t length);
uint32_t pdb_decode_acct_ctrl(const char *p);
void pdb_sethexpwd(char p[33], const unsigned char *pwd, uint32_t acct_ctrl);
bool pdb_gethexpwd(const char *p, unsigned char *pwd);
void pdb_sethexhours(char *p, const unsigned char *hours);
bool pdb_gethexhours(const char *p, unsigned char *hours);
int algorithmic_rid_base(void);
uid_t algorithmic_pdb_user_rid_to_uid(uint32_t user_rid);
uid_t max_algorithmic_uid(void);
uint32_t algorithmic_pdb_uid_to_user_rid(uid_t uid);
gid_t pdb_group_rid_to_gid(uint32_t group_rid);
gid_t max_algorithmic_gid(void);
uint32_t algorithmic_pdb_gid_to_group_rid(gid_t gid);
bool algorithmic_pdb_rid_is_user(uint32_t rid);
bool lookup_global_sam_name(const char *name, int flags, uint32_t *rid,
			    enum lsa_SidType *type);
NTSTATUS local_password_change(const char *user_name,
				int local_flags,
				const char *new_passwd,
				char **pp_err_str,
				char **pp_msg_str);
bool init_samu_from_buffer(struct samu *sampass, uint32_t level,
			   uint8_t *buf, uint32_t buflen);
uint32_t init_buffer_from_samu (uint8_t **buf, struct samu *sampass, bool size_only);
bool pdb_copy_sam_account(struct samu *dst, struct samu *src );
bool pdb_update_bad_password_count(struct samu *sampass, bool *updated);
bool pdb_update_autolock_flag(struct samu *sampass, bool *updated);
bool pdb_increment_bad_password_count(struct samu *sampass);
bool is_dc_trusted_domain_situation(const char *domain_name);
bool get_trust_pw_clear(const char *domain, char **ret_pwd,
			const char **account_name,
			enum netr_SchannelType *channel);
bool get_trust_pw_hash(const char *domain, uint8_t ret_pwd[16],
		       const char **account_name,
		       enum netr_SchannelType *channel);

/* The following definitions come from passdb/pdb_compat.c  */

uint32_t pdb_get_user_rid (const struct samu *sampass);
uint32_t pdb_get_group_rid (struct samu *sampass);
bool pdb_set_user_sid_from_rid (struct samu *sampass, uint32_t rid, enum pdb_value_state flag);
bool pdb_set_group_sid_from_rid (struct samu *sampass, uint32_t grid, enum pdb_value_state flag);

/* The following definitions come from passdb/pdb_get_set.c  */

bool pdb_is_password_change_time_max(time_t test_time);
uint32_t pdb_get_acct_ctrl(const struct samu *sampass);
time_t pdb_get_logon_time(const struct samu *sampass);
time_t pdb_get_logoff_time(const struct samu *sampass);
time_t pdb_get_kickoff_time(const struct samu *sampass);
time_t pdb_get_bad_password_time(const struct samu *sampass);
time_t pdb_get_pass_last_set_time(const struct samu *sampass);
time_t pdb_get_pass_can_change_time(const struct samu *sampass);
time_t pdb_get_pass_can_change_time_noncalc(const struct samu *sampass);
time_t pdb_get_pass_must_change_time(const struct samu *sampass);
bool pdb_get_pass_can_change(const struct samu *sampass);
uint16_t pdb_get_logon_divs(const struct samu *sampass);
uint32_t pdb_get_hours_len(const struct samu *sampass);
const uint8_t *pdb_get_hours(const struct samu *sampass);
const uint8_t *pdb_get_nt_passwd(const struct samu *sampass);
const uint8_t *pdb_get_lanman_passwd(const struct samu *sampass);
const uint8_t *pdb_get_pw_history(const struct samu *sampass, uint32_t *current_hist_len);
const char *pdb_get_plaintext_passwd(const struct samu *sampass);
const struct dom_sid *pdb_get_user_sid(const struct samu *sampass);
const struct dom_sid *pdb_get_group_sid(struct samu *sampass);
enum pdb_value_state pdb_get_init_flags(const struct samu *sampass, enum pdb_elements element);
const char *pdb_get_username(const struct samu *sampass);
const char *pdb_get_domain(const struct samu *sampass);
const char *pdb_get_nt_username(const struct samu *sampass);
const char *pdb_get_fullname(const struct samu *sampass);
const char *pdb_get_homedir(const struct samu *sampass);
const char *pdb_get_dir_drive(const struct samu *sampass);
const char *pdb_get_logon_script(const struct samu *sampass);
const char *pdb_get_profile_path(const struct samu *sampass);
const char *pdb_get_acct_desc(const struct samu *sampass);
const char *pdb_get_workstations(const struct samu *sampass);
const char *pdb_get_comment(const struct samu *sampass);
const char *pdb_get_munged_dial(const struct samu *sampass);
uint16_t pdb_get_bad_password_count(const struct samu *sampass);
uint16_t pdb_get_logon_count(const struct samu *sampass);
uint16_t pdb_get_country_code(const struct samu *sampass);
uint16_t pdb_get_code_page(const struct samu *sampass);
uint32_t pdb_get_unknown_6(const struct samu *sampass);
void *pdb_get_backend_private_data(const struct samu *sampass, const struct pdb_methods *my_methods);
bool pdb_set_acct_ctrl(struct samu *sampass, uint32_t acct_ctrl, enum pdb_value_state flag);
bool pdb_set_logon_time(struct samu *sampass, time_t mytime, enum pdb_value_state flag);
bool pdb_set_logoff_time(struct samu *sampass, time_t mytime, enum pdb_value_state flag);
bool pdb_set_kickoff_time(struct samu *sampass, time_t mytime, enum pdb_value_state flag);
bool pdb_set_bad_password_time(struct samu *sampass, time_t mytime, enum pdb_value_state flag);
bool pdb_set_pass_can_change_time(struct samu *sampass, time_t mytime, enum pdb_value_state flag);
bool pdb_set_pass_must_change_time(struct samu *sampass, time_t mytime, enum pdb_value_state flag);
bool pdb_set_pass_last_set_time(struct samu *sampass, time_t mytime, enum pdb_value_state flag);
bool pdb_set_hours_len(struct samu *sampass, uint32_t len, enum pdb_value_state flag);
bool pdb_set_logon_divs(struct samu *sampass, uint16_t hours, enum pdb_value_state flag);
bool pdb_set_init_flags(struct samu *sampass, enum pdb_elements element, enum pdb_value_state value_flag);
bool pdb_set_user_sid(struct samu *sampass, const struct dom_sid *u_sid, enum pdb_value_state flag);
bool pdb_set_user_sid_from_string(struct samu *sampass, fstring u_sid, enum pdb_value_state flag);
bool pdb_set_group_sid(struct samu *sampass, const struct dom_sid *g_sid, enum pdb_value_state flag);
bool pdb_set_username(struct samu *sampass, const char *username, enum pdb_value_state flag);
bool pdb_set_domain(struct samu *sampass, const char *domain, enum pdb_value_state flag);
bool pdb_set_nt_username(struct samu *sampass, const char *nt_username, enum pdb_value_state flag);
bool pdb_set_fullname(struct samu *sampass, const char *full_name, enum pdb_value_state flag);
bool pdb_set_logon_script(struct samu *sampass, const char *logon_script, enum pdb_value_state flag);
bool pdb_set_profile_path(struct samu *sampass, const char *profile_path, enum pdb_value_state flag);
bool pdb_set_dir_drive(struct samu *sampass, const char *dir_drive, enum pdb_value_state flag);
bool pdb_set_homedir(struct samu *sampass, const char *home_dir, enum pdb_value_state flag);
bool pdb_set_acct_desc(struct samu *sampass, const char *acct_desc, enum pdb_value_state flag);
bool pdb_set_workstations(struct samu *sampass, const char *workstations, enum pdb_value_state flag);
bool pdb_set_comment(struct samu *sampass, const char *comment, enum pdb_value_state flag);
bool pdb_set_munged_dial(struct samu *sampass, const char *munged_dial, enum pdb_value_state flag);
bool pdb_set_nt_passwd(struct samu *sampass, const uint8_t pwd[NT_HASH_LEN], enum pdb_value_state flag);
bool pdb_set_lanman_passwd(struct samu *sampass, const uint8_t pwd[LM_HASH_LEN], enum pdb_value_state flag);
bool pdb_set_pw_history(struct samu *sampass, const uint8_t *pwd, uint32_t historyLen, enum pdb_value_state flag);
bool pdb_set_plaintext_pw_only(struct samu *sampass, const char *password, enum pdb_value_state flag);
bool pdb_set_bad_password_count(struct samu *sampass, uint16_t bad_password_count, enum pdb_value_state flag);
bool pdb_set_logon_count(struct samu *sampass, uint16_t logon_count, enum pdb_value_state flag);
bool pdb_set_country_code(struct samu *sampass, uint16_t country_code,
			  enum pdb_value_state flag);
bool pdb_set_code_page(struct samu *sampass, uint16_t code_page,
		       enum pdb_value_state flag);
bool pdb_set_unknown_6(struct samu *sampass, uint32_t unkn, enum pdb_value_state flag);
bool pdb_set_hours(struct samu *sampass, const uint8 *hours, int hours_len,
		   enum pdb_value_state flag);
bool pdb_set_backend_private_data(struct samu *sampass, void *private_data,
				   void (*free_fn)(void **),
				   const struct pdb_methods *my_methods,
				   enum pdb_value_state flag);
bool pdb_set_pass_can_change(struct samu *sampass, bool canchange);
bool pdb_set_plaintext_passwd(struct samu *sampass, const char *plaintext);
uint32_t pdb_build_fields_present(struct samu *sampass);

/* The following definitions come from passdb/pdb_interface.c  */

NTSTATUS smb_register_passdb(int version, const char *name, pdb_init_function init) ;
struct pdb_init_function_entry *pdb_find_backend_entry(const char *name);
struct event_context *pdb_get_event_context(void);
NTSTATUS make_pdb_method_name(struct pdb_methods **methods, const char *selected);
struct pdb_domain_info *pdb_get_domain_info(TALLOC_CTX *mem_ctx);
bool pdb_getsampwnam(struct samu *sam_acct, const char *username) ;
bool pdb_getsampwsid(struct samu *sam_acct, const struct dom_sid *sid) ;
NTSTATUS pdb_create_user(TALLOC_CTX *mem_ctx, const char *name, uint32_t flags,
			 uint32_t *rid);
NTSTATUS pdb_delete_user(TALLOC_CTX *mem_ctx, struct samu *sam_acct);
NTSTATUS pdb_add_sam_account(struct samu *sam_acct) ;
NTSTATUS pdb_update_sam_account(struct samu *sam_acct) ;
NTSTATUS pdb_delete_sam_account(struct samu *sam_acct) ;
NTSTATUS pdb_rename_sam_account(struct samu *oldname, const char *newname);
NTSTATUS pdb_update_login_attempts(struct samu *sam_acct, bool success);
bool pdb_getgrsid(GROUP_MAP *map, struct dom_sid sid);
bool pdb_getgrgid(GROUP_MAP *map, gid_t gid);
bool pdb_getgrnam(GROUP_MAP *map, const char *name);
NTSTATUS pdb_create_dom_group(TALLOC_CTX *mem_ctx, const char *name,
			      uint32_t *rid);
NTSTATUS pdb_delete_dom_group(TALLOC_CTX *mem_ctx, uint32_t rid);
NTSTATUS pdb_add_group_mapping_entry(GROUP_MAP *map);
NTSTATUS pdb_update_group_mapping_entry(GROUP_MAP *map);
NTSTATUS pdb_delete_group_mapping_entry(struct dom_sid sid);
bool pdb_enum_group_mapping(const struct dom_sid *sid, enum lsa_SidType sid_name_use, GROUP_MAP **pp_rmap,
			    size_t *p_num_entries, bool unix_only);
NTSTATUS pdb_enum_group_members(TALLOC_CTX *mem_ctx,
				const struct dom_sid *sid,
				uint32_t **pp_member_rids,
				size_t *p_num_members);
NTSTATUS pdb_enum_group_memberships(TALLOC_CTX *mem_ctx, struct samu *user,
				    struct dom_sid **pp_sids, gid_t **pp_gids,
				    uint32_t *p_num_groups);
NTSTATUS pdb_set_unix_primary_group(TALLOC_CTX *mem_ctx, struct samu *user);
NTSTATUS pdb_add_groupmem(TALLOC_CTX *mem_ctx, uint32_t group_rid,
			  uint32_t member_rid);
NTSTATUS pdb_del_groupmem(TALLOC_CTX *mem_ctx, uint32_t group_rid,
			  uint32_t member_rid);
NTSTATUS pdb_create_alias(const char *name, uint32_t *rid);
NTSTATUS pdb_delete_alias(const struct dom_sid *sid);
NTSTATUS pdb_get_aliasinfo(const struct dom_sid *sid, struct acct_info *info);
NTSTATUS pdb_set_aliasinfo(const struct dom_sid *sid, struct acct_info *info);
NTSTATUS pdb_add_aliasmem(const struct dom_sid *alias, const struct dom_sid *member);
NTSTATUS pdb_del_aliasmem(const struct dom_sid *alias, const struct dom_sid *member);
NTSTATUS pdb_enum_aliasmem(const struct dom_sid *alias, TALLOC_CTX *mem_ctx,
			   struct dom_sid **pp_members, size_t *p_num_members);
NTSTATUS pdb_enum_alias_memberships(TALLOC_CTX *mem_ctx,
				    const struct dom_sid *domain_sid,
				    const struct dom_sid *members, size_t num_members,
				    uint32_t **pp_alias_rids,
				    size_t *p_num_alias_rids);
NTSTATUS pdb_lookup_rids(const struct dom_sid *domain_sid,
			 int num_rids,
			 uint32_t *rids,
			 const char **names,
			 enum lsa_SidType *attrs);
NTSTATUS pdb_lookup_names(const struct dom_sid *domain_sid,
			  int num_names,
			  const char **names,
			  uint32_t *rids,
			  enum lsa_SidType *attrs);
bool pdb_get_account_policy(enum pdb_policy_type type, uint32_t *value);
bool pdb_set_account_policy(enum pdb_policy_type type, uint32_t value);
bool pdb_get_seq_num(time_t *seq_num);
bool pdb_uid_to_sid(uid_t uid, struct dom_sid *sid);
bool pdb_gid_to_sid(gid_t gid, struct dom_sid *sid);
bool pdb_sid_to_id(const struct dom_sid *sid, union unid_t *id,
		   enum lsa_SidType *type);
uint32_t pdb_capabilities(void);
bool pdb_new_rid(uint32_t *rid);
bool initialize_password_db(bool reload, struct event_context *event_ctx);
struct pdb_search *pdb_search_init(TALLOC_CTX *mem_ctx,
				   enum pdb_search_type type);
struct pdb_search *pdb_search_users(TALLOC_CTX *mem_ctx, uint32_t acct_flags);
struct pdb_search *pdb_search_groups(TALLOC_CTX *mem_ctx);
struct pdb_search *pdb_search_aliases(TALLOC_CTX *mem_ctx, const struct dom_sid *sid);
uint32_t pdb_search_entries(struct pdb_search *search,
			  uint32_t start_idx, uint32_t max_entries,
			  struct samr_displayentry **result);
bool pdb_get_trusteddom_pw(const char *domain, char** pwd, struct dom_sid *sid,
			   time_t *pass_last_set_time);
bool pdb_set_trusteddom_pw(const char* domain, const char* pwd,
			   const struct dom_sid *sid);
bool pdb_del_trusteddom_pw(const char *domain);
NTSTATUS pdb_enum_trusteddoms(TALLOC_CTX *mem_ctx, uint32_t *num_domains,
			      struct trustdom_info ***domains);
NTSTATUS pdb_get_trusted_domain(TALLOC_CTX *mem_ctx, const char *domain,
				struct pdb_trusted_domain **td);
NTSTATUS pdb_get_trusted_domain_by_sid(TALLOC_CTX *mem_ctx, struct dom_sid *sid,
				struct pdb_trusted_domain **td);
NTSTATUS pdb_set_trusted_domain(const char* domain,
				const struct pdb_trusted_domain *td);
NTSTATUS pdb_del_trusted_domain(const char *domain);
NTSTATUS pdb_enum_trusted_domains(TALLOC_CTX *mem_ctx, uint32_t *num_domains,
				  struct pdb_trusted_domain ***domains);
NTSTATUS make_pdb_method( struct pdb_methods **methods ) ;

/* The following definitions come from passdb/pdb_ldap.c  */

const char** get_userattr_list( TALLOC_CTX *mem_ctx, int schema_ver );
NTSTATUS pdb_init_ldapsam_compat(struct pdb_methods **pdb_method, const char *location);
NTSTATUS pdb_init_ldapsam(struct pdb_methods **pdb_method, const char *location);
NTSTATUS pdb_ldap_init(void);

/* The following definitions come from passdb/pdb_nds.c  */

struct smbldap_state;

int pdb_nds_get_password(
	struct smbldap_state *ldap_state,
	char *object_dn,
	size_t *pwd_len,
	char *pwd );
int pdb_nds_set_password(
	struct smbldap_state *ldap_state,
	char *object_dn,
	const char *pwd );
NTSTATUS pdb_nds_init(void);

/* The following definitions come from passdb/pdb_nds.c  */

NTSTATUS pdb_ipa_init(void);

/* The following definitions come from passdb/pdb_smbpasswd.c  */

NTSTATUS pdb_smbpasswd_init(void) ;

/* The following definitions come from passdb/pdb_wbc_sam.c  */

NTSTATUS pdb_wbc_sam_init(void);

/* The following definitions come from passdb/pdb_tdb.c  */

NTSTATUS pdb_tdbsam_init(void);

/* The following definitions come from passdb/pdb_util.c  */

NTSTATUS create_builtin_users(const struct dom_sid *sid);
NTSTATUS create_builtin_administrators(const struct dom_sid *sid);

#endif /* _PASSDB_PROTO_H_ */
