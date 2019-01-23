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
#include "librpc/gen_ndr/auth.h"
#include "../auth/common_auth.h"

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

#define AUTH_SESSION_INFO_DEFAULT_GROUPS     0x01 /* Add the user to the default world and network groups */
#define AUTH_SESSION_INFO_AUTHENTICATED      0x02 /* Add the user to the 'authenticated users' group */
#define AUTH_SESSION_INFO_SIMPLE_PRIVILEGES  0x04 /* Use a trivial map between users and privilages, rather than a DB */

struct auth_method_context;
struct auth_check_password_request;
struct auth_context;
struct auth_session_info;
struct ldb_dn;

struct auth_operations {
	const char *name;

	/* If you are using this interface, then you are probably
	 * getting something wrong.  This interface is only for
	 * security=server, and makes a number of compromises to allow
	 * that.  It is not compatible with being a PDC.  */

	NTSTATUS (*get_challenge)(struct auth_method_context *ctx, TALLOC_CTX *mem_ctx, uint8_t chal[8]);

	/* Given the user supplied info, check if this backend want to handle the password checking */

	NTSTATUS (*want_check)(struct auth_method_context *ctx, TALLOC_CTX *mem_ctx,
			       const struct auth_usersupplied_info *user_info);

	/* Given the user supplied info, check a password */

	NTSTATUS (*check_password)(struct auth_method_context *ctx, TALLOC_CTX *mem_ctx,
				   const struct auth_usersupplied_info *user_info,
				   struct auth_user_info_dc **interim_info);

	/* Lookup a 'session info interim' return based only on the principal or DN */
	NTSTATUS (*get_user_info_dc_principal)(TALLOC_CTX *mem_ctx,
						       struct auth_context *auth_context,
						       const char *principal,
						       struct ldb_dn *user_dn,
						       struct auth_user_info_dc **interim_info);
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

	/* SAM database for this local machine - to fill in local groups, or to authenticate local NTLM users */
	struct ldb_context *sam_ctx;

	NTSTATUS (*check_password)(struct auth_context *auth_ctx,
				   TALLOC_CTX *mem_ctx,
				   const struct auth_usersupplied_info *user_info,
				   struct auth_user_info_dc **user_info_dc);

	NTSTATUS (*get_challenge)(struct auth_context *auth_ctx, uint8_t chal[8]);

	bool (*challenge_may_be_modified)(struct auth_context *auth_ctx);

	NTSTATUS (*set_challenge)(struct auth_context *auth_ctx, const uint8_t chal[8], const char *set_by);

	NTSTATUS (*get_user_info_dc_principal)(TALLOC_CTX *mem_ctx,
						       struct auth_context *auth_ctx,
						       const char *principal,
						       struct ldb_dn *user_dn,
						       struct auth_user_info_dc **user_info_dc);

	NTSTATUS (*generate_session_info)(TALLOC_CTX *mem_ctx,
					  struct auth_context *auth_context,
					  struct auth_user_info_dc *user_info_dc,
					  uint32_t session_info_flags,
					  struct auth_session_info **session_info);
};

/* this structure is used by backends to determine the size of some critical types */
struct auth_critical_sizes {
	int interface_version;
	int sizeof_auth_operations;
	int sizeof_auth_methods;
	int sizeof_auth_context;
	int sizeof_auth_usersupplied_info;
	int sizeof_auth_user_info_dc;
};

 NTSTATUS encrypt_user_info(TALLOC_CTX *mem_ctx, struct auth_context *auth_context,
			   enum auth_password_state to_state,
			   const struct auth_usersupplied_info *user_info_in,
			   const struct auth_usersupplied_info **user_info_encrypted);

#include "auth/session.h"
#include "auth/system_session_proto.h"
#include "libcli/security/security.h"

struct ldb_message;
struct ldb_context;
struct gensec_security;
struct cli_credentials;

NTSTATUS auth_get_challenge(struct auth_context *auth_ctx, uint8_t chal[8]);
NTSTATUS authsam_account_ok(TALLOC_CTX *mem_ctx,
			    struct ldb_context *sam_ctx,
			    uint32_t logon_parameters,
			    struct ldb_dn *domain_dn,
			    struct ldb_message *msg,
			    const char *logon_workstation,
			    const char *name_for_logs,
			    bool allow_domain_trust,
			    bool password_change);
NTSTATUS authsam_expand_nested_groups(struct ldb_context *sam_ctx,
				      struct ldb_val *dn_val, const bool only_childs, const char *filter,
				      TALLOC_CTX *res_sids_ctx, struct dom_sid ***res_sids,
				      unsigned int *num_res_sids);
struct auth_session_info *system_session(struct loadparm_context *lp_ctx);
NTSTATUS authsam_make_user_info_dc(TALLOC_CTX *mem_ctx, struct ldb_context *sam_ctx,
					   const char *netbios_name,
					   const char *domain_name,
					   struct ldb_dn *domain_dn,
					   struct ldb_message *msg,
					   DATA_BLOB user_sess_key, DATA_BLOB lm_sess_key,
				  struct auth_user_info_dc **_user_info_dc);
NTSTATUS auth_system_session_info(TALLOC_CTX *parent_ctx,
					   struct loadparm_context *lp_ctx,
					   struct auth_session_info **_session_info) ;

NTSTATUS auth_context_create_methods(TALLOC_CTX *mem_ctx, const char **methods,
				     struct tevent_context *ev,
				     struct messaging_context *msg,
				     struct loadparm_context *lp_ctx,
				     struct ldb_context *sam_ctx,
				     struct auth_context **auth_ctx);
const char **auth_methods_from_lp(TALLOC_CTX *mem_ctx, struct loadparm_context *lp_ctx);

NTSTATUS auth_context_create(TALLOC_CTX *mem_ctx,
			     struct tevent_context *ev,
			     struct messaging_context *msg,
			     struct loadparm_context *lp_ctx,
			     struct auth_context **auth_ctx);
NTSTATUS auth_context_create_from_ldb(TALLOC_CTX *mem_ctx, struct ldb_context *ldb, struct auth_context **auth_ctx);

NTSTATUS auth_check_password(struct auth_context *auth_ctx,
			     TALLOC_CTX *mem_ctx,
			     const struct auth_usersupplied_info *user_info,
			     struct auth_user_info_dc **user_info_dc);
NTSTATUS auth4_init(void);
NTSTATUS auth_register(const struct auth_operations *ops);
NTSTATUS server_service_auth_init(void);
NTSTATUS authenticate_username_pw(TALLOC_CTX *mem_ctx,
				  struct tevent_context *ev,
				  struct messaging_context *msg,
				  struct loadparm_context *lp_ctx,
				  const char *nt4_domain,
				  const char *nt4_username,
				  const char *password,
				  const uint32_t logon_parameters,
				  struct auth_session_info **session_info);

struct tevent_req *auth_check_password_send(TALLOC_CTX *mem_ctx,
					    struct tevent_context *ev,
					    struct auth_context *auth_ctx,
					    const struct auth_usersupplied_info *user_info);
NTSTATUS auth_check_password_recv(struct tevent_req *req,
				  TALLOC_CTX *mem_ctx,
				  struct auth_user_info_dc **user_info_dc);

bool auth_challenge_may_be_modified(struct auth_context *auth_ctx);
NTSTATUS auth_context_set_challenge(struct auth_context *auth_ctx, const uint8_t chal[8], const char *set_by);

NTSTATUS auth_get_user_info_dc_principal(TALLOC_CTX *mem_ctx,
					struct auth_context *auth_ctx,
					const char *principal,
					struct ldb_dn *user_dn,
					struct auth_user_info_dc **user_info_dc);

NTSTATUS samba_server_gensec_start(TALLOC_CTX *mem_ctx,
				   struct tevent_context *event_ctx,
				   struct messaging_context *msg_ctx,
				   struct loadparm_context *lp_ctx,
				   struct cli_credentials *server_credentials,
				   const char *target_service,
				   struct gensec_security **gensec_context);

#endif /* _SMBAUTH_H_ */
