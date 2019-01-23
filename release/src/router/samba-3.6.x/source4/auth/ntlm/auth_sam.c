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
#include "system/time.h"
#include <ldb.h>
#include "libcli/ldap/ldap_ndr.h"
#include "libcli/security/security.h"
#include "auth/auth.h"
#include "../libcli/auth/ntlm_check.h"
#include "auth/ntlm/auth_proto.h"
#include "auth/auth_sam.h"
#include "dsdb/samdb/samdb.h"
#include "dsdb/common/util.h"
#include "param/param.h"
#include "librpc/gen_ndr/ndr_irpc_c.h"
#include "lib/messaging/irpc.h"

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
	ret = dsdb_search_one(sam_ctx, mem_ctx, ret_msg, domain_dn, LDB_SCOPE_SUBTREE,
			      user_attrs,
			      DSDB_SEARCH_SHOW_EXTENDED_DN,
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
					     lpcfg_lanman_auth(auth_context->lp_ctx),
					     user_info->password.hash.lanman,
					     user_info->password.hash.nt,
					     user_info->mapped.account_name,
					     lm_pwd, nt_pwd);
		NT_STATUS_NOT_OK_RETURN(status);
		break;
		
	case AUTH_PASSWORD_RESPONSE:
		status = ntlm_password_check(mem_ctx, 
					     lpcfg_lanman_auth(auth_context->lp_ctx),
						 lpcfg_ntlm_auth(auth_context->lp_ctx),
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


/*
  send a message to the drepl server telling it to initiate a
  REPL_SECRET getncchanges extended op to fetch the users secrets
 */
static void auth_sam_trigger_repl_secret(TALLOC_CTX *mem_ctx, struct auth_context *auth_context,
					 struct ldb_dn *user_dn)
{
	struct dcerpc_binding_handle *irpc_handle;
	struct drepl_trigger_repl_secret r;
	struct tevent_req *req;

	irpc_handle = irpc_binding_handle_by_name(mem_ctx, auth_context->msg_ctx,
						  "dreplsrv",
						  &ndr_table_irpc);
	if (irpc_handle == NULL) {
		DEBUG(1,(__location__ ": Unable to get binding handle for dreplsrv\n"));
		return;
	}

	r.in.user_dn = ldb_dn_get_linearized(user_dn);

	req = dcerpc_drepl_trigger_repl_secret_r_send(mem_ctx,
						      auth_context->event_ctx,
						      irpc_handle,
						      &r);

	/* we aren't interested in a reply */
	talloc_free(req);
	talloc_free(irpc_handle);
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

	uint16_t acct_flags = samdb_result_acct_flags(auth_context->sam_ctx, mem_ctx, msg, domain_dn);
	
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

	if (lm_pwd == NULL && nt_pwd == NULL) {
		bool am_rodc;
		if (samdb_rodc(auth_context->sam_ctx, &am_rodc) == LDB_SUCCESS && am_rodc) {
			/* we don't have passwords for this
			 * account. We are an RODC, and this account
			 * may be one for which we either are denied
			 * REPL_SECRET replication or we haven't yet
			 * done the replication. We return
			 * NT_STATUS_NOT_IMPLEMENTED which tells the
			 * auth code to try the next authentication
			 * mechanism. We also send a message to our
			 * drepl server to tell it to try and
			 * replicate the secrets for this account.
			 */
			auth_sam_trigger_repl_secret(mem_ctx, auth_context, msg->dn);
			return NT_STATUS_NOT_IMPLEMENTED;
		}
	}

	nt_status = authsam_password_ok(auth_context, mem_ctx, 
					acct_flags, lm_pwd, nt_pwd,
					user_info, user_sess_key, lm_sess_key);
	NT_STATUS_NOT_OK_RETURN(nt_status);

	nt_status = authsam_account_ok(mem_ctx, auth_context->sam_ctx,
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
						 struct auth_user_info_dc **user_info_dc)
{
	NTSTATUS nt_status;
	const char *account_name = user_info->mapped.account_name;
	struct ldb_message *msg;
	struct ldb_dn *domain_dn;
	DATA_BLOB user_sess_key, lm_sess_key;
	TALLOC_CTX *tmp_ctx;

	if (ctx->auth_ctx->sam_ctx == NULL) {
		DEBUG(0, ("No SAM available, cannot log in users\n"));
		return NT_STATUS_INVALID_SYSTEM_SERVICE;
	}

	if (!account_name || !*account_name) {
		/* 'not for me' */
		return NT_STATUS_NOT_IMPLEMENTED;
	}

	tmp_ctx = talloc_new(mem_ctx);
	if (!tmp_ctx) {
		return NT_STATUS_NO_MEMORY;
	}

	domain_dn = ldb_get_default_basedn(ctx->auth_ctx->sam_ctx);
	if (domain_dn == NULL) {
		talloc_free(tmp_ctx);
		return NT_STATUS_NO_SUCH_DOMAIN;
	}

	nt_status = authsam_search_account(tmp_ctx, ctx->auth_ctx->sam_ctx, account_name, domain_dn, &msg);
	if (!NT_STATUS_IS_OK(nt_status)) {
		talloc_free(tmp_ctx);
		return nt_status;
	}

	nt_status = authsam_authenticate(ctx->auth_ctx, tmp_ctx, ctx->auth_ctx->sam_ctx, domain_dn, msg, user_info,
					 &user_sess_key, &lm_sess_key);
	if (!NT_STATUS_IS_OK(nt_status)) {
		talloc_free(tmp_ctx);
		return nt_status;
	}

	nt_status = authsam_make_user_info_dc(tmp_ctx, ctx->auth_ctx->sam_ctx, lpcfg_netbios_name(ctx->auth_ctx->lp_ctx),
					     lpcfg_sam_name(ctx->auth_ctx->lp_ctx),
					     domain_dn,
					     msg,
					     user_sess_key, lm_sess_key,
					     user_info_dc);
	if (!NT_STATUS_IS_OK(nt_status)) {
		talloc_free(tmp_ctx);
		return nt_status;
	}

	talloc_steal(mem_ctx, *user_info_dc);
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

	is_local_name = lpcfg_is_myname(ctx->auth_ctx->lp_ctx,
				  user_info->mapped.domain_name);
	is_my_domain  = lpcfg_is_mydomain(ctx->auth_ctx->lp_ctx,
				       user_info->mapped.domain_name); 

	/* check whether or not we service this domain/workgroup name */
	switch (lpcfg_server_role(ctx->auth_ctx->lp_ctx)) {
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

	DEBUG(6,("authsam_check_password: lpcfg_server_role() has an undefined value\n"));
	return NT_STATUS_NOT_IMPLEMENTED;
}

				   
/* Wrapper for the auth subsystem pointer */
static NTSTATUS authsam_get_user_info_dc_principal_wrapper(TALLOC_CTX *mem_ctx,
							  struct auth_context *auth_context,
							  const char *principal,
							  struct ldb_dn *user_dn,
							  struct auth_user_info_dc **user_info_dc)
{
	return authsam_get_user_info_dc_principal(mem_ctx, auth_context->lp_ctx, auth_context->sam_ctx,
						 principal, user_dn, user_info_dc);
}
static const struct auth_operations sam_ignoredomain_ops = {
	.name		           = "sam_ignoredomain",
	.get_challenge	           = auth_get_challenge_not_implemented,
	.want_check	           = authsam_ignoredomain_want_check,
	.check_password	           = authsam_check_password_internals,
	.get_user_info_dc_principal = authsam_get_user_info_dc_principal_wrapper
};

static const struct auth_operations sam_ops = {
	.name		           = "sam",
	.get_challenge	           = auth_get_challenge_not_implemented,
	.want_check	           = authsam_want_check,
	.check_password	           = authsam_check_password_internals,
	.get_user_info_dc_principal = authsam_get_user_info_dc_principal_wrapper
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
