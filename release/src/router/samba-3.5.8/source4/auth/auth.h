/* 
   Unix SMB/CIFS implementation.
   Standardised Authentication types
   Copyright (C) Andrew Bartlett   2001
   Copyright (C) Stefan Metzmacher 2005
   
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

#ifndef _SAMBA_AUTH_H
#define _SAMBA_AUTH_H

#include "librpc/gen_ndr/ndr_krb5pac.h"

extern const char *krbtgt_attrs[];
extern const char *server_attrs[];
extern const char *user_attrs[];

union netr_Validation;
struct netr_SamBaseInfo;
struct netr_SamInfo3;
struct loadparm_context;

/* modules can use the following to determine if the interface has changed
 * please increment the version number after each interface change
 * with a comment and maybe update struct auth_critical_sizes.
 */
/* version 1 - version from samba 3.0 - metze */
/* version 2 - initial samba4 version - metze */
/* version 3 - subsequent samba4 version - abartlet */
/* version 4 - subsequent samba4 version - metze */
/* version 0 - till samba4 is stable - metze */
#define AUTH_INTERFACE_VERSION 0

#define USER_INFO_CASE_INSENSITIVE_USERNAME 0x01 /* username may be in any case */
#define USER_INFO_CASE_INSENSITIVE_PASSWORD 0x02 /* password may be in any case */
#define USER_INFO_DONT_CHECK_UNIX_ACCOUNT   0x04 /* dont check unix account status */
#define USER_INFO_INTERACTIVE_LOGON         0x08 /* dont check unix account status */

enum auth_password_state {
	AUTH_PASSWORD_RESPONSE,
	AUTH_PASSWORD_HASH,
	AUTH_PASSWORD_PLAIN
};

struct auth_usersupplied_info
{
	const char *workstation_name;
	struct socket_address *remote_host;

	uint32_t logon_parameters;

	bool mapped_state;
	/* the values the client gives us */
	struct {
		const char *account_name;
		const char *domain_name;
	} client, mapped;

	enum auth_password_state password_state;

	union {
		struct {
			DATA_BLOB lanman;
			DATA_BLOB nt;
		} response;
		struct {
			struct samr_Password *lanman;
			struct samr_Password *nt;
		} hash;
		
		char *plaintext;
	} password;
	uint32_t flags;
};

struct auth_serversupplied_info 
{
	struct dom_sid *account_sid;
	struct dom_sid *primary_group_sid;

	size_t n_domain_groups;
	struct dom_sid **domain_groups;

	DATA_BLOB user_session_key;
	DATA_BLOB lm_session_key;

	const char *account_name;
	const char *domain_name;

	const char *full_name;
	const char *logon_script;
	const char *profile_path;
	const char *home_directory;
	const char *home_drive;
	const char *logon_server;
	
	NTTIME last_logon;
	NTTIME last_logoff;
	NTTIME acct_expiry;
	NTTIME last_password_change;
	NTTIME allow_password_change;
	NTTIME force_password_change;

	uint16_t logon_count;
	uint16_t bad_password_count;

	uint32_t acct_flags;

	bool authenticated;

	struct PAC_SIGNATURE_DATA pac_srv_sig, pac_kdc_sig;
};

struct auth_method_context;
struct auth_check_password_request;
struct auth_context;

struct auth_operations {
	const char *name;

	/* If you are using this interface, then you are probably
	 * getting something wrong.  This interface is only for
	 * security=server, and makes a number of compromises to allow
	 * that.  It is not compatible with being a PDC.  */

	NTSTATUS (*get_challenge)(struct auth_method_context *ctx, TALLOC_CTX *mem_ctx, DATA_BLOB *challenge);

	/* Given the user supplied info, check if this backend want to handle the password checking */

	NTSTATUS (*want_check)(struct auth_method_context *ctx, TALLOC_CTX *mem_ctx,
			       const struct auth_usersupplied_info *user_info);

	/* Given the user supplied info, check a password */

	NTSTATUS (*check_password)(struct auth_method_context *ctx, TALLOC_CTX *mem_ctx,
				   const struct auth_usersupplied_info *user_info,
				   struct auth_serversupplied_info **server_info);

	/* Lookup a 'server info' return based only on the principal */
	NTSTATUS (*get_server_info_principal)(TALLOC_CTX *mem_ctx, 
					      struct auth_context *auth_context,
					      const char *principal,
					      struct auth_serversupplied_info **server_info);
};

struct auth_method_context {
	struct auth_method_context *prev, *next;
	struct auth_context *auth_ctx;
	const struct auth_operations *ops;
	int depth;
	void *private_data;
};

struct auth_context {
	struct {
		/* Who set this up in the first place? */ 
		const char *set_by;

		bool may_be_modified;

		DATA_BLOB data; 
	} challenge;

	/* methods, in the order they should be called */
	struct auth_method_context *methods;

	/* the event context to use for calls that can block */
	struct tevent_context *event_ctx;

	/* the messaging context which can be used by backends */
	struct messaging_context *msg_ctx;

	/* loadparm context */
	struct loadparm_context *lp_ctx;

	NTSTATUS (*check_password)(struct auth_context *auth_ctx,
				   TALLOC_CTX *mem_ctx,
				   const struct auth_usersupplied_info *user_info, 
				   struct auth_serversupplied_info **server_info);
	
	NTSTATUS (*get_challenge)(struct auth_context *auth_ctx, const uint8_t **_chal);

	bool (*challenge_may_be_modified)(struct auth_context *auth_ctx);

	NTSTATUS (*set_challenge)(struct auth_context *auth_ctx, const uint8_t chal[8], const char *set_by);
	
	NTSTATUS (*get_server_info_principal)(TALLOC_CTX *mem_ctx, 
					      struct auth_context *auth_context,
					      const char *principal,
					      struct auth_serversupplied_info **server_info);

};

/* this structure is used by backends to determine the size of some critical types */
struct auth_critical_sizes {
	int interface_version;
	int sizeof_auth_operations;
	int sizeof_auth_methods;
	int sizeof_auth_context;
	int sizeof_auth_usersupplied_info;
	int sizeof_auth_serversupplied_info;
};

 NTSTATUS encrypt_user_info(TALLOC_CTX *mem_ctx, struct auth_context *auth_context, 
			   enum auth_password_state to_state,
			   const struct auth_usersupplied_info *user_info_in,
			   const struct auth_usersupplied_info **user_info_encrypted);

#include "auth/session.h"
#include "auth/system_session_proto.h"

struct ldb_message;
struct ldb_context;
struct ldb_dn;
struct gensec_security;

NTSTATUS auth_get_challenge(struct auth_context *auth_ctx, const uint8_t **_chal);
NTSTATUS authsam_account_ok(TALLOC_CTX *mem_ctx,
			    struct ldb_context *sam_ctx,
			    uint32_t logon_parameters,
			    struct ldb_dn *domain_dn,
			    struct ldb_message *msg,
			    const char *logon_workstation,
			    const char *name_for_logs,
			    bool allow_domain_trust,
			    bool password_change);
struct auth_session_info *system_session(TALLOC_CTX *mem_ctx, struct loadparm_context *lp_ctx);
NTSTATUS authsam_make_server_info(TALLOC_CTX *mem_ctx, struct ldb_context *sam_ctx,
					   const char *netbios_name,
					   const char *domain_name,
					   struct ldb_dn *domain_dn, 
					   struct ldb_message *msg,
					   DATA_BLOB user_sess_key, DATA_BLOB lm_sess_key,
				  struct auth_serversupplied_info **_server_info);
NTSTATUS auth_system_session_info(TALLOC_CTX *parent_ctx, 
					   struct loadparm_context *lp_ctx,
					   struct auth_session_info **_session_info) ;
NTSTATUS auth_nt_status_squash(NTSTATUS nt_status);

NTSTATUS auth_context_create_methods(TALLOC_CTX *mem_ctx, const char **methods, 
				     struct tevent_context *ev,
				     struct messaging_context *msg,
				     struct loadparm_context *lp_ctx,
				     struct auth_context **auth_ctx);

NTSTATUS auth_context_create(TALLOC_CTX *mem_ctx, 
			     struct tevent_context *ev,
			     struct messaging_context *msg,
			     struct loadparm_context *lp_ctx,
			     struct auth_context **auth_ctx);

NTSTATUS auth_check_password(struct auth_context *auth_ctx,
			     TALLOC_CTX *mem_ctx,
			     const struct auth_usersupplied_info *user_info, 
			     struct auth_serversupplied_info **server_info);
NTSTATUS auth_init(void);
NTSTATUS auth_register(const struct auth_operations *ops);
NTSTATUS authenticate_username_pw(TALLOC_CTX *mem_ctx,
					   struct tevent_context *ev,
					   struct messaging_context *msg,
					   struct loadparm_context *lp_ctx,
					   const char *nt4_domain,
					   const char *nt4_username,
					   const char *password,
					   struct auth_session_info **session_info);
NTSTATUS auth_check_password_recv(struct auth_check_password_request *req,
				  TALLOC_CTX *mem_ctx,
				  struct auth_serversupplied_info **server_info);

void auth_check_password_send(struct auth_context *auth_ctx,
			      const struct auth_usersupplied_info *user_info,
			      void (*callback)(struct auth_check_password_request *req, void *private_data),
			      void *private_data);
NTSTATUS auth_context_set_challenge(struct auth_context *auth_ctx, const uint8_t chal[8], const char *set_by);

NTSTATUS samba_server_gensec_start(TALLOC_CTX *mem_ctx,
				   struct tevent_context *event_ctx,
				   struct messaging_context *msg_ctx,
				   struct loadparm_context *lp_ctx,
				   struct cli_credentials *server_credentials,
				   const char *target_service,
				   struct gensec_security **gensec_context);

#endif /* _SMBAUTH_H_ */
