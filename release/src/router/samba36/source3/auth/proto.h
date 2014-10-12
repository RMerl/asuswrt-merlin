/*
 *  Unix SMB/CIFS implementation.
 *  Password and authentication handling
 *
 *  Copyright (C) Andrew Tridgell		1992-2001
 *  Copyright (C) Luke Kenneth Casson Leighton	1996-2000
 *  Copyright (C) Jeremy Allison		1997-2001
 *  Copyright (C) John H Terpsta		1999-2001
 *  Copyright (C) Tim Potter			2000
 *  Copyright (C) Andrew Bartlett		2001-2003
 *  Copyright (C) Jelmer Vernooij		2002
 *  Copyright (C) Rafal Szczesniak		2002
 *  Copyright (C) Gerald Carter			2003
 *  Copyright (C) Volker Lendecke		2006,2010
 *  Copyright (C) Michael Adam			2007
 *  Copyright (C) Dan Sledz			2009
 *  Copyright (C) Simo Sorce			2010
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

#ifndef _AUTH_PROTO_H_
#define _AUTH_PROTO_H_

/* The following definitions come from auth/auth.c  */

NTSTATUS smb_register_auth(int version, const char *name, auth_init_function init);
bool load_auth_module(struct auth_context *auth_context,
		      const char *module, auth_methods **ret) ;
NTSTATUS make_auth_context_subsystem(TALLOC_CTX *mem_ctx,
				     struct auth_context **auth_context);
NTSTATUS make_auth_context_fixed(TALLOC_CTX *mem_ctx,
				 struct auth_context **auth_context,
				 uchar chal[8]) ;

/* The following definitions come from auth/auth_builtin.c  */

NTSTATUS auth_builtin_init(void);

/* The following definitions come from auth/auth_compat.c  */

NTSTATUS check_plaintext_password(const char *smb_name,
				  DATA_BLOB plaintext_password,
				  struct auth_serversupplied_info **server_info);
bool password_ok(struct auth_context *actx, bool global_encrypted,
		 const char *session_workgroup,
		 const char *smb_name, DATA_BLOB password_blob);

/* The following definitions come from auth/auth_domain.c  */

void attempt_machine_password_change(void);
NTSTATUS auth_domain_init(void);

NTSTATUS auth_netlogond_init(void);

/* The following definitions come from auth/auth_ntlmssp.c  */

NTSTATUS auth_ntlmssp_steal_session_info(TALLOC_CTX *mem_ctx,
				struct auth_ntlmssp_state *auth_ntlmssp_state,
				struct auth_serversupplied_info **session_info);
NTSTATUS auth_ntlmssp_start(struct auth_ntlmssp_state **auth_ntlmssp_state);


/* The following definitions come from auth/auth_sam.c  */

NTSTATUS check_sam_security(const DATA_BLOB *challenge,
			    TALLOC_CTX *mem_ctx,
			    const struct auth_usersupplied_info *user_info,
			    struct auth_serversupplied_info **server_info);
NTSTATUS check_sam_security_info3(const DATA_BLOB *challenge,
				  TALLOC_CTX *mem_ctx,
				  const struct auth_usersupplied_info *user_info,
				  struct netr_SamInfo3 **pinfo3);
NTSTATUS auth_sam_init(void);

/* The following definitions come from auth/auth_server.c  */

NTSTATUS auth_server_init(void);

/* The following definitions come from auth/auth_unix.c  */

NTSTATUS auth_unix_init(void);

/* The following definitions come from auth/auth_util.c  */

NTSTATUS make_user_info_map(struct auth_usersupplied_info **user_info,
			    const char *smb_name,
			    const char *client_domain,
			    const char *workstation_name,
			    DATA_BLOB *lm_pwd,
			    DATA_BLOB *nt_pwd,
			    const struct samr_Password *lm_interactive_pwd,
			    const struct samr_Password *nt_interactive_pwd,
			    const char *plaintext,
			    enum auth_password_state password_state);
bool make_user_info_netlogon_network(struct auth_usersupplied_info **user_info,
				     const char *smb_name,
				     const char *client_domain,
				     const char *workstation_name,
				     uint32 logon_parameters,
				     const uchar *lm_network_pwd,
				     int lm_pwd_len,
				     const uchar *nt_network_pwd,
				     int nt_pwd_len);
bool make_user_info_netlogon_interactive(struct auth_usersupplied_info **user_info,
					 const char *smb_name,
					 const char *client_domain,
					 const char *workstation_name,
					 uint32 logon_parameters,
					 const uchar chal[8],
					 const uchar lm_interactive_pwd[16],
					 const uchar nt_interactive_pwd[16],
					 const uchar *dc_sess_key);
bool make_user_info_for_reply(struct auth_usersupplied_info **user_info,
			      const char *smb_name,
			      const char *client_domain,
			      const uint8 chal[8],
			      DATA_BLOB plaintext_password);
NTSTATUS make_user_info_for_reply_enc(struct auth_usersupplied_info **user_info,
                                      const char *smb_name,
                                      const char *client_domain,
                                      DATA_BLOB lm_resp, DATA_BLOB nt_resp);
bool make_user_info_guest(struct auth_usersupplied_info **user_info) ;
struct samu;
NTSTATUS make_server_info_sam(struct auth_serversupplied_info **server_info,
			      struct samu *sampass);
NTSTATUS create_local_token(struct auth_serversupplied_info *server_info);
NTSTATUS create_token_from_username(TALLOC_CTX *mem_ctx, const char *username,
				    bool is_guest,
				    uid_t *uid, gid_t *gid,
				    char **found_username,
				    struct security_token **token);
bool user_in_group_sid(const char *username, const struct dom_sid *group_sid);
bool user_in_group(const char *username, const char *groupname);
struct passwd;
NTSTATUS make_server_info_pw(struct auth_serversupplied_info **server_info,
                             char *unix_username,
			     struct passwd *pwd);
NTSTATUS make_serverinfo_from_username(TALLOC_CTX *mem_ctx,
				       const char *username,
				       bool use_guest_token,
				       bool is_guest,
				       struct auth_serversupplied_info **presult);
struct auth_serversupplied_info *copy_serverinfo(TALLOC_CTX *mem_ctx,
						 const struct auth_serversupplied_info *src);
bool init_guest_info(void);
NTSTATUS init_system_info(void);
bool session_info_set_session_key(struct auth_serversupplied_info *info,
				 DATA_BLOB session_key);
NTSTATUS make_server_info_guest(TALLOC_CTX *mem_ctx,
				struct auth_serversupplied_info **server_info);
NTSTATUS make_session_info_system(TALLOC_CTX *mem_ctx,
				 struct auth_serversupplied_info **session_info);
const struct auth_serversupplied_info *get_session_info_system(void);
bool copy_current_user(struct current_user *dst, struct current_user *src);
struct passwd *smb_getpwnam( TALLOC_CTX *mem_ctx, const char *domuser,
			     char **p_save_username, bool create );
NTSTATUS make_server_info_info3(TALLOC_CTX *mem_ctx,
				const char *sent_nt_username,
				const char *domain,
				struct auth_serversupplied_info **server_info,
				struct netr_SamInfo3 *info3);
struct wbcAuthUserInfo;
NTSTATUS make_server_info_wbcAuthUserInfo(TALLOC_CTX *mem_ctx,
					  const char *sent_nt_username,
					  const char *domain,
					  const struct wbcAuthUserInfo *info,
					  struct auth_serversupplied_info **server_info);
void free_user_info(struct auth_usersupplied_info **user_info);
bool make_auth_methods(struct auth_context *auth_context, auth_methods **auth_method) ;
bool is_trusted_domain(const char* dom_name);

/* The following definitions come from auth/user_info.c  */

NTSTATUS make_user_info(struct auth_usersupplied_info **ret_user_info,
			const char *smb_name,
			const char *internal_username,
			const char *client_domain,
			const char *domain,
			const char *workstation_name,
			const DATA_BLOB *lm_pwd,
			const DATA_BLOB *nt_pwd,
			const struct samr_Password *lm_interactive_pwd,
			const struct samr_Password *nt_interactive_pwd,
			const char *plaintext_password,
			enum auth_password_state password_state);
void free_user_info(struct auth_usersupplied_info **user_info);

/* The following definitions come from auth/auth_winbind.c  */

NTSTATUS auth_winbind_init(void);

/* The following definitions come from auth/server_info.c  */

struct netr_SamInfo2;
struct netr_SamInfo3;
struct netr_SamInfo6;

struct auth_serversupplied_info *make_server_info(TALLOC_CTX *mem_ctx);
NTSTATUS serverinfo_to_SamInfo2(struct auth_serversupplied_info *server_info,
				uint8_t *pipe_session_key,
				size_t pipe_session_key_len,
				struct netr_SamInfo2 *sam2);
NTSTATUS serverinfo_to_SamInfo3(const struct auth_serversupplied_info *server_info,
				uint8_t *pipe_session_key,
				size_t pipe_session_key_len,
				struct netr_SamInfo3 *sam3);
NTSTATUS serverinfo_to_SamInfo6(struct auth_serversupplied_info *server_info,
				uint8_t *pipe_session_key,
				size_t pipe_session_key_len,
				struct netr_SamInfo6 *sam6);
NTSTATUS samu_to_SamInfo3(TALLOC_CTX *mem_ctx,
			  struct samu *samu,
			  const char *login_server,
			  struct netr_SamInfo3 **_info3,
			  struct extra_auth_info *extra);
struct netr_SamInfo3 *copy_netr_SamInfo3(TALLOC_CTX *mem_ctx,
					 struct netr_SamInfo3 *orig);
struct netr_SamInfo3 *wbcAuthUserInfo_to_netr_SamInfo3(TALLOC_CTX *mem_ctx,
					const struct wbcAuthUserInfo *info);

/* The following definitions come from auth/auth_wbc.c  */

NTSTATUS auth_wbc_init(void);

/* The following definitions come from auth/pampass.c  */

bool smb_pam_claim_session(char *user, char *tty, char *rhost);
bool smb_pam_close_session(char *user, char *tty, char *rhost);
NTSTATUS smb_pam_accountcheck(const char *user, const char *rhost);
NTSTATUS smb_pam_passcheck(const char * user, const char * rhost,
			   const char * password);
bool smb_pam_passchange(const char *user, const char *rhost,
			const char *oldpassword, const char *newpassword);
bool smb_pam_claim_session(char *user, char *tty, char *rhost);
bool smb_pam_close_session(char *in_user, char *tty, char *rhost);

/* The following definitions come from auth/pass_check.c  */

void dfs_unlogin(void);
NTSTATUS pass_check(const struct passwd *pass,
		    const char *user,
		    const char *rhost,
		    const char *password,
		    bool run_cracker);

/* The following definitions come from auth/token_util.c  */

bool nt_token_check_sid ( const struct dom_sid *sid, const struct security_token *token );
bool nt_token_check_domain_rid( struct security_token *token, uint32 rid );
struct security_token *get_root_nt_token( void );
NTSTATUS add_aliases(const struct dom_sid *domain_sid,
		     struct security_token *token);
struct security_token *create_local_nt_token(TALLOC_CTX *mem_ctx,
					    const struct dom_sid *user_sid,
					    bool is_guest,
					    int num_groupsids,
					    const struct dom_sid *groupsids);
NTSTATUS create_local_nt_token_from_info3(TALLOC_CTX *mem_ctx,
					  bool is_guest,
					  struct netr_SamInfo3 *info3,
					  struct extra_auth_info *extra,
					  struct security_token **ntok);
void debug_unix_user_token(int dbg_class, int dbg_lev, uid_t uid, gid_t gid,
			   int n_groups, gid_t *groups);

/* The following definitions come from auth/user_util.c  */

bool map_username(TALLOC_CTX *ctx, const char *user_in, char **p_user_out);
bool user_in_netgroup(TALLOC_CTX *ctx, const char *user, const char *ngname);
bool user_in_list(TALLOC_CTX *ctx, const char *user,const char **list);

/* The following definitions come from auth/user_krb5.c  */
struct PAC_LOGON_INFO;
NTSTATUS get_user_from_kerberos_info(TALLOC_CTX *mem_ctx,
				     const char *cli_name,
				     const char *princ_name,
				     struct PAC_LOGON_INFO *logon_info,
				     bool *is_mapped,
				     bool *mapped_to_guest,
				     char **ntuser,
				     char **ntdomain,
				     char **username,
				     struct passwd **_pw);
NTSTATUS make_server_info_krb5(TALLOC_CTX *mem_ctx,
				char *ntuser,
				char *ntdomain,
				char *username,
				struct passwd *pw,
				struct PAC_LOGON_INFO *logon_info,
				bool mapped_to_guest,
				struct auth_serversupplied_info **server_info);

#endif /* _AUTH_PROTO_H_ */
