/* 
   Unix SMB/CIFS implementation.
   Password and authentication handling
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2001-2009
   Copyright (C) Gerald Carter                             2003
   Copyright (C) Stefan Metzmacher                         2005
   
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

#include "includes.h"
#include "librpc/gen_ndr/ndr_netlogon.h"
#include "system/time.h"
#include "lib/ldb/include/ldb.h"
#include "../lib/util/util_ldb.h"
#include "auth/auth.h"
#include "../libcli/auth/ntlm_check.h"
#include "auth/ntlm/auth_proto.h"
#include "auth/auth_sam.h"
#include "dsdb/samdb/samdb.h"
#include "libcli/security/security.h"
#include "libcli/ldap/ldap_ndr.h"
#include "param/param.h"

extern const char *user_attrs[];
extern const char *domain_ref_attrs[];

/****************************************************************************
 Look for the specified user in the sam, return ldb result structures
****************************************************************************/

static NTSTATUS authsam_search_account(TALLOC_CTX *mem_ctx, struct ldb_context *sam_ctx,
				       const char *account_name,
				       struct ldb_dn *domain_dn,
				       struct ldb_message **ret_msg)
{
	int ret;

	/* pull the user attributes */
	ret = gendb_search_single_extended_dn(sam_ctx, mem_ctx, domain_dn, LDB_SCOPE_SUBTREE,
					      ret_msg, user_attrs,
					      "(&(sAMAccountName=%s)(objectclass=user))", 
					      ldb_binary_encode_string(mem_ctx, account_name));
	if (ret == LDB_ERR_NO_SUCH_OBJECT) {
		DEBUG(3,("sam_search_user: Couldn't find user [%s] in samdb, under %s\n", 
			 account_name, ldb_dn_get_linearized(domain_dn)));
		return NT_STATUS_NO_SUCH_USER;		
	}
	if (ret != LDB_SUCCESS) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}
	
	return NT_STATUS_OK;
}

/****************************************************************************
 Do a specific test for an smb password being correct, given a smb_password and
 the lanman and NT responses.
****************************************************************************/
static NTSTATUS authsam_password_ok(struct auth_context *auth_context,
				    TALLOC_CTX *mem_ctx,
				    uint16_t acct_flags,
				    const struct samr_Password *lm_pwd, 
				    const struct samr_Password *nt_pwd,
				    const struct auth_usersupplied_info *user_info, 
				    DATA_BLOB *user_sess_key, 
				    DATA_BLOB *lm_sess_key)
{
	NTSTATUS status;

	switch (user_info->password_state) {
	case AUTH_PASSWORD_PLAIN: 
	{
		const struct auth_usersupplied_info *user_info_temp;	
		status = encrypt_user_info(mem_ctx, auth_context, 
					   AUTH_PASSWORD_HASH, 
					   user_info, &user_info_temp);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(1, ("Failed to convert plaintext password to password HASH: %s\n", nt_errstr(status)));
			return status;
		}
		user_info = user_info_temp;

		/*fall through*/
	}
	case AUTH_PASSWORD_HASH:
		*lm_sess_key = data_blob(NULL, 0);
		*user_sess_key = data_blob(NULL, 0);
		status = hash_password_check(mem_ctx, 
					     lp_lanman_auth(auth_context->lp_ctx),
					     user_info->password.hash.lanman,
					     user_info->password.hash.nt,
					     user_info->mapped.account_name,
					     lm_pwd, nt_pwd);
		NT_STATUS_NOT_OK_RETURN(status);
		break;
		
	case AUTH_PASSWORD_RESPONSE:
		status = ntlm_password_check(mem_ctx, 
					     lp_lanman_auth(auth_context->lp_ctx),
						 lp_ntlm_auth(auth_context->lp_ctx),
					     user_info->logon_parameters, 
					     &auth_context->challenge.data, 
					     &user_info->password.response.lanman, 
					     &user_info->password.response.nt,
					     user_info->mapped.account_name,
					     user_info->client.account_name, 
					     user_info->client.domain_name, 
					     lm_pwd, nt_pwd,
					     user_sess_key, lm_sess_key);
		NT_STATUS_NOT_OK_RETURN(status);
		break;
	}

	if (user_sess_key && user_sess_key->data) {
		talloc_steal(auth_context, user_sess_key->data);
	}
	if (lm_sess_key && lm_sess_key->data) {
		talloc_steal(auth_context, lm_sess_key->data);
	}

	return NT_STATUS_OK;
}



static NTSTATUS authsam_authenticate(struct auth_context *auth_context, 
				     TALLOC_CTX *mem_ctx, struct ldb_context *sam_ctx, 
				     struct ldb_dn *domain_dn,
				     struct ldb_message *msg,
				     const struct auth_usersupplied_info *user_info, 
				     DATA_BLOB *user_sess_key, DATA_BLOB *lm_sess_key) 
{
	struct samr_Password *lm_pwd, *nt_pwd;
	NTSTATUS nt_status;

	uint16_t acct_flags = samdb_result_acct_flags(sam_ctx, mem_ctx, msg, domain_dn);
	
	/* Quit if the account was locked out. */
	if (acct_flags & ACB_AUTOLOCK) {
		DEBUG(3,("check_sam_security: Account for user %s was locked out.\n", 
			 user_info->mapped.account_name));
		return NT_STATUS_ACCOUNT_LOCKED_OUT;
	}

	/* You can only do an interactive login to normal accounts */
	if (user_info->flags & USER_INFO_INTERACTIVE_LOGON) {
		if (!(acct_flags & ACB_NORMAL)) {
			return NT_STATUS_NO_SUCH_USER;
		}
	}

	nt_status = samdb_result_passwords(mem_ctx, auth_context->lp_ctx, msg, &lm_pwd, &nt_pwd);
	NT_STATUS_NOT_OK_RETURN(nt_status);

	nt_status = authsam_password_ok(auth_context, mem_ctx, 
					acct_flags, lm_pwd, nt_pwd,
					user_info, user_sess_key, lm_sess_key);
	NT_STATUS_NOT_OK_RETURN(nt_status);

	nt_status = authsam_account_ok(mem_ctx, sam_ctx, 
				       user_info->logon_parameters,
				       domain_dn,
				       msg,
				       user_info->workstation_name,
				       user_info->mapped.account_name,
				       false, false);

	return nt_status;
}



static NTSTATUS authsam_check_password_internals(struct auth_method_context *ctx,
						 TALLOC_CTX *mem_ctx,
						 const struct auth_usersupplied_info *user_info, 
						 struct auth_serversupplied_info **server_info)
{
	NTSTATUS nt_status;
	const char *account_name = user_info->mapped.account_name;
	struct ldb_message *msg;
	struct ldb_context *sam_ctx;
	struct ldb_dn *domain_dn;
	DATA_BLOB user_sess_key, lm_sess_key;
	TALLOC_CTX *tmp_ctx;

	if (!account_name || !*account_name) {
		/* 'not for me' */
		return NT_STATUS_NOT_IMPLEMENTED;
	}

	tmp_ctx = talloc_new(mem_ctx);
	if (!tmp_ctx) {
		return NT_STATUS_NO_MEMORY;
	}

	sam_ctx = samdb_connect(tmp_ctx, ctx->auth_ctx->event_ctx, ctx->auth_ctx->lp_ctx, system_session(mem_ctx, ctx->auth_ctx->lp_ctx));
	if (sam_ctx == NULL) {
		talloc_free(tmp_ctx);
		return NT_STATUS_INVALID_SYSTEM_SERVICE;
	}

	domain_dn = ldb_get_default_basedn(sam_ctx);
	if (domain_dn == NULL) {
		talloc_free(tmp_ctx);
		return NT_STATUS_NO_SUCH_DOMAIN;
	}

	nt_status = authsam_search_account(tmp_ctx, sam_ctx, account_name, domain_dn, &msg);
	if (!NT_STATUS_IS_OK(nt_status)) {
		talloc_free(tmp_ctx);
		return nt_status;
	}

	nt_status = authsam_authenticate(ctx->auth_ctx, tmp_ctx, sam_ctx, domain_dn, msg, user_info,
					 &user_sess_key, &lm_sess_key);
	if (!NT_STATUS_IS_OK(nt_status)) {
		talloc_free(tmp_ctx);
		return nt_status;
	}

	nt_status = authsam_make_server_info(tmp_ctx, sam_ctx, lp_netbios_name(ctx->auth_ctx->lp_ctx), 
 					     lp_sam_name(ctx->auth_ctx->lp_ctx),
					     domain_dn,
					     msg,
					     user_sess_key, lm_sess_key,
					     server_info);
	if (!NT_STATUS_IS_OK(nt_status)) {
		talloc_free(tmp_ctx);
		return nt_status;
	}

	talloc_steal(mem_ctx, *server_info);
	talloc_free(tmp_ctx);

	return NT_STATUS_OK;
}

static NTSTATUS authsam_ignoredomain_want_check(struct auth_method_context *ctx,
						TALLOC_CTX *mem_ctx,
						const struct auth_usersupplied_info *user_info)
{
	if (!user_info->mapped.account_name || !*user_info->mapped.account_name) {
		return NT_STATUS_NOT_IMPLEMENTED;
	}

	return NT_STATUS_OK;
}

/****************************************************************************
Check SAM security (above) but with a few extra checks.
****************************************************************************/
static NTSTATUS authsam_want_check(struct auth_method_context *ctx,
				   TALLOC_CTX *mem_ctx,
				   const struct auth_usersupplied_info *user_info)
{
	bool is_local_name, is_my_domain;

	if (!user_info->mapped.account_name || !*user_info->mapped.account_name) {
		return NT_STATUS_NOT_IMPLEMENTED;
	}

	is_local_name = lp_is_myname(ctx->auth_ctx->lp_ctx, 
				  user_info->mapped.domain_name);
	is_my_domain  = lp_is_mydomain(ctx->auth_ctx->lp_ctx, 
				       user_info->mapped.domain_name); 

	/* check whether or not we service this domain/workgroup name */
	switch (lp_server_role(ctx->auth_ctx->lp_ctx)) {
		case ROLE_STANDALONE:
			return NT_STATUS_OK;

		case ROLE_DOMAIN_MEMBER:
			if (!is_local_name) {
				DEBUG(6,("authsam_check_password: %s is not one of my local names (DOMAIN_MEMBER)\n",
					user_info->mapped.domain_name));
				return NT_STATUS_NOT_IMPLEMENTED;
			}
			return NT_STATUS_OK;

		case ROLE_DOMAIN_CONTROLLER:
			if (!is_local_name && !is_my_domain) {
				DEBUG(6,("authsam_check_password: %s is not one of my local names or domain name (DC)\n",
					user_info->mapped.domain_name));
				return NT_STATUS_NOT_IMPLEMENTED;
			}
			return NT_STATUS_OK;
	}

	DEBUG(6,("authsam_check_password: lp_server_role() has an undefined value\n"));
	return NT_STATUS_NOT_IMPLEMENTED;
}

				   
/* Used in the gensec_gssapi and gensec_krb5 server-side code, where the PAC isn't available */
NTSTATUS authsam_get_server_info_principal(TALLOC_CTX *mem_ctx, 
					   struct auth_context *auth_context,
					   const char *principal,
					   struct auth_serversupplied_info **server_info)
{
	NTSTATUS nt_status;
	DATA_BLOB user_sess_key = data_blob(NULL, 0);
	DATA_BLOB lm_sess_key = data_blob(NULL, 0);

	struct ldb_message *msg;
	struct ldb_context *sam_ctx;
	struct ldb_dn *domain_dn;
	
	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
	if (!tmp_ctx) {
		return NT_STATUS_NO_MEMORY;
	}

	sam_ctx = samdb_connect(tmp_ctx, auth_context->event_ctx, auth_context->lp_ctx, 
				system_session(tmp_ctx, auth_context->lp_ctx));
	if (sam_ctx == NULL) {
		talloc_free(tmp_ctx);
		return NT_STATUS_INVALID_SYSTEM_SERVICE;
	}

	nt_status = sam_get_results_principal(sam_ctx, tmp_ctx, principal, 
					      user_attrs, &domain_dn, &msg);
	if (!NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	}

	nt_status = authsam_make_server_info(tmp_ctx, sam_ctx, 
					     lp_netbios_name(auth_context->lp_ctx),
 					     lp_workgroup(auth_context->lp_ctx),
					     domain_dn, 
					     msg,
					     user_sess_key, lm_sess_key,
					     server_info);
	if (NT_STATUS_IS_OK(nt_status)) {
		talloc_steal(mem_ctx, *server_info);
	}
	talloc_free(tmp_ctx);
	return nt_status;
}

static const struct auth_operations sam_ignoredomain_ops = {
	.name		           = "sam_ignoredomain",
	.get_challenge	           = auth_get_challenge_not_implemented,
	.want_check	           = authsam_ignoredomain_want_check,
	.check_password	           = authsam_check_password_internals,
	.get_server_info_principal = authsam_get_server_info_principal
};

static const struct auth_operations sam_ops = {
	.name		           = "sam",
	.get_challenge	           = auth_get_challenge_not_implemented,
	.want_check	           = authsam_want_check,
	.check_password	           = authsam_check_password_internals,
	.get_server_info_principal = authsam_get_server_info_principal
};

_PUBLIC_ NTSTATUS auth_sam_init(void)
{
	NTSTATUS ret;

	ret = auth_register(&sam_ops);
	if (!NT_STATUS_IS_OK(ret)) {
		DEBUG(0,("Failed to register 'sam' auth backend!\n"));
		return ret;
	}

	ret = auth_register(&sam_ignoredomain_ops);
	if (!NT_STATUS_IS_OK(ret)) {
		DEBUG(0,("Failed to register 'sam_ignoredomain' auth backend!\n"));
		return ret;
	}

	return ret;
}
