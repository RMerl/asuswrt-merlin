/*
   Unix SMB/CIFS implementation.

   samr server password set/change handling

   Copyright (C) Andrew Tridgell 2004
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2005

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
#include "rpc_server/dcerpc_server.h"
#include "rpc_server/samr/dcesrv_samr.h"
#include "system/time.h"
#include "../lib/crypto/crypto.h"
#include "dsdb/samdb/samdb.h"
#include "auth/auth.h"
#include "libcli/auth/libcli_auth.h"
#include "../lib/util/util_ldb.h"

/*
  samr_ChangePasswordUser

  So old it is just not worth implementing
  because it does not supply a plaintext and so we can't do password
  complexity checking and cannot update all the other password hashes.

*/
NTSTATUS dcesrv_samr_ChangePasswordUser(struct dcesrv_call_state *dce_call,
					TALLOC_CTX *mem_ctx,
					struct samr_ChangePasswordUser *r)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

/*
  samr_OemChangePasswordUser2
*/
NTSTATUS dcesrv_samr_OemChangePasswordUser2(struct dcesrv_call_state *dce_call,
					    TALLOC_CTX *mem_ctx,
					    struct samr_OemChangePasswordUser2 *r)
{
	NTSTATUS status;
	DATA_BLOB new_password, new_unicode_password;
	char *new_pass;
	struct samr_CryptPassword *pwbuf = r->in.password;
	struct ldb_context *sam_ctx;
	struct ldb_dn *user_dn;
	int ret;
	struct ldb_message **res;
	const char * const attrs[] = { "objectSid", "dBCSPwd", NULL };
	struct samr_Password *lm_pwd;
	DATA_BLOB lm_pwd_blob;
	uint8_t new_lm_hash[16];
	struct samr_Password lm_verifier;
	size_t unicode_pw_len;

	if (pwbuf == NULL) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (r->in.hash == NULL) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* this call can only work with lanman auth */
	if (!lpcfg_lanman_auth(dce_call->conn->dce_ctx->lp_ctx)) {
		return NT_STATUS_WRONG_PASSWORD;
	}

	/* Connect to a SAMDB with system privileges for fetching the old pw
	 * hashes. */
	sam_ctx = samdb_connect(mem_ctx, dce_call->event_ctx,
				dce_call->conn->dce_ctx->lp_ctx,
				system_session(dce_call->conn->dce_ctx->lp_ctx), 0);
	if (sam_ctx == NULL) {
		return NT_STATUS_INVALID_SYSTEM_SERVICE;
	}

	/* we need the users dn and the domain dn (derived from the
	   user SID). We also need the current lm password hash in
	   order to decrypt the incoming password */
	ret = gendb_search(sam_ctx,
			   mem_ctx, NULL, &res, attrs,
			   "(&(sAMAccountName=%s)(objectclass=user))",
			   r->in.account->string);
	if (ret != 1) {
		/* Don't give the game away:  (don't allow anonymous users to prove the existance of usernames) */
		return NT_STATUS_WRONG_PASSWORD;
	}

	user_dn = res[0]->dn;

	status = samdb_result_passwords(mem_ctx, dce_call->conn->dce_ctx->lp_ctx,
					res[0], &lm_pwd, NULL);
	if (!NT_STATUS_IS_OK(status) || !lm_pwd) {
		return NT_STATUS_WRONG_PASSWORD;
	}

	/* decrypt the password we have been given */
	lm_pwd_blob = data_blob(lm_pwd->hash, sizeof(lm_pwd->hash));
	arcfour_crypt_blob(pwbuf->data, 516, &lm_pwd_blob);
	data_blob_free(&lm_pwd_blob);

	if (!extract_pw_from_buffer(mem_ctx, pwbuf->data, &new_password)) {
		DEBUG(3,("samr: failed to decode password buffer\n"));
		return NT_STATUS_WRONG_PASSWORD;
	}

	if (!convert_string_talloc_convenience(mem_ctx, lpcfg_iconv_convenience(dce_call->conn->dce_ctx->lp_ctx),
				  CH_DOS, CH_UNIX,
				  (const char *)new_password.data,
				  new_password.length,
				  (void **)&new_pass, NULL, false)) {
		DEBUG(3,("samr: failed to convert incoming password buffer to unix charset\n"));
		return NT_STATUS_WRONG_PASSWORD;
	}

	if (!convert_string_talloc_convenience(mem_ctx, lpcfg_iconv_convenience(dce_call->conn->dce_ctx->lp_ctx),
					       CH_DOS, CH_UTF16,
					       (const char *)new_password.data,
					       new_password.length,
					       (void **)&new_unicode_password.data, &unicode_pw_len, false)) {
		DEBUG(3,("samr: failed to convert incoming password buffer to UTF16 charset\n"));
		return NT_STATUS_WRONG_PASSWORD;
	}
	new_unicode_password.length = unicode_pw_len;

	E_deshash(new_pass, new_lm_hash);
	E_old_pw_hash(new_lm_hash, lm_pwd->hash, lm_verifier.hash);
	if (memcmp(lm_verifier.hash, r->in.hash->hash, 16) != 0) {
		return NT_STATUS_WRONG_PASSWORD;
	}

	/* Connect to a SAMDB with user privileges for the password change */
	sam_ctx = samdb_connect(mem_ctx, dce_call->event_ctx,
				dce_call->conn->dce_ctx->lp_ctx,
				dce_call->conn->auth_state.session_info, 0);
	if (sam_ctx == NULL) {
		return NT_STATUS_INVALID_SYSTEM_SERVICE;
	}

	/* Start transaction */
	ret = ldb_transaction_start(sam_ctx);
	if (ret != LDB_SUCCESS) {
		DEBUG(1, ("Failed to start transaction: %s\n", ldb_errstring(sam_ctx)));
		return NT_STATUS_TRANSACTION_ABORTED;
	}

	/* Performs the password modification. We pass the old hashes read out
	 * from the database since they were already checked against the user-
	 * provided ones. */
	status = samdb_set_password(sam_ctx, mem_ctx,
				    user_dn, NULL,
				    &new_unicode_password,
				    NULL, NULL,
				    lm_pwd, NULL, /* this is a user password change */
				    NULL,
				    NULL);
	if (!NT_STATUS_IS_OK(status)) {
		ldb_transaction_cancel(sam_ctx);
		return status;
	}

	/* And this confirms it in a transaction commit */
	ret = ldb_transaction_commit(sam_ctx);
	if (ret != LDB_SUCCESS) {
		DEBUG(1,("Failed to commit transaction to change password on %s: %s\n",
			 ldb_dn_get_linearized(user_dn),
			 ldb_errstring(sam_ctx)));
		return NT_STATUS_TRANSACTION_ABORTED;
	}

	return NT_STATUS_OK;
}


/*
  samr_ChangePasswordUser3
*/
NTSTATUS dcesrv_samr_ChangePasswordUser3(struct dcesrv_call_state *dce_call,
					 TALLOC_CTX *mem_ctx,
					 struct samr_ChangePasswordUser3 *r)
{
	NTSTATUS status;
	DATA_BLOB new_password;
	struct ldb_context *sam_ctx = NULL;
	struct ldb_dn *user_dn;
	int ret;
	struct ldb_message **res;
	const char * const attrs[] = { "unicodePwd", "dBCSPwd", NULL };
	struct samr_Password *nt_pwd, *lm_pwd;
	DATA_BLOB nt_pwd_blob;
	struct samr_DomInfo1 *dominfo = NULL;
	struct userPwdChangeFailureInformation *reject = NULL;
	enum samPwdChangeReason reason = SAM_PWD_CHANGE_NO_ERROR;
	uint8_t new_nt_hash[16], new_lm_hash[16];
	struct samr_Password nt_verifier, lm_verifier;

	*r->out.dominfo = NULL;
	*r->out.reject = NULL;

	if (r->in.nt_password == NULL ||
	    r->in.nt_verifier == NULL) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* Connect to a SAMDB with system privileges for fetching the old pw
	 * hashes. */
	sam_ctx = samdb_connect(mem_ctx, dce_call->event_ctx,
				dce_call->conn->dce_ctx->lp_ctx,
				system_session(dce_call->conn->dce_ctx->lp_ctx), 0);
	if (sam_ctx == NULL) {
		return NT_STATUS_INVALID_SYSTEM_SERVICE;
	}

	/* we need the users dn and the domain dn (derived from the
	   user SID). We also need the current lm and nt password hashes
	   in order to decrypt the incoming passwords */
	ret = gendb_search(sam_ctx,
			   mem_ctx, NULL, &res, attrs,
			   "(&(sAMAccountName=%s)(objectclass=user))",
			   r->in.account->string);
	if (ret != 1) {
		/* Don't give the game away:  (don't allow anonymous users to prove the existance of usernames) */
		status = NT_STATUS_WRONG_PASSWORD;
		goto failed;
	}

	user_dn = res[0]->dn;

	status = samdb_result_passwords(mem_ctx, dce_call->conn->dce_ctx->lp_ctx,
					res[0], &lm_pwd, &nt_pwd);
	if (!NT_STATUS_IS_OK(status) ) {
		goto failed;
	}

	if (!nt_pwd) {
		status = NT_STATUS_WRONG_PASSWORD;
		goto failed;
	}

	/* decrypt the password we have been given */
	nt_pwd_blob = data_blob(nt_pwd->hash, sizeof(nt_pwd->hash));
	arcfour_crypt_blob(r->in.nt_password->data, 516, &nt_pwd_blob);
	data_blob_free(&nt_pwd_blob);

	if (!extract_pw_from_buffer(mem_ctx, r->in.nt_password->data, &new_password)) {
		DEBUG(3,("samr: failed to decode password buffer\n"));
		status =  NT_STATUS_WRONG_PASSWORD;
		goto failed;
	}

	if (r->in.nt_verifier == NULL) {
		status = NT_STATUS_WRONG_PASSWORD;
		goto failed;
	}

	/* check NT verifier */
	mdfour(new_nt_hash, new_password.data, new_password.length);

	E_old_pw_hash(new_nt_hash, nt_pwd->hash, nt_verifier.hash);
	if (memcmp(nt_verifier.hash, r->in.nt_verifier->hash, 16) != 0) {
		status = NT_STATUS_WRONG_PASSWORD;
		goto failed;
	}

	/* check LM verifier (really not needed as we just checked the
	 * much stronger NT hash, but the RPC-SAMR test checks for
	 * this) */
	if (lm_pwd && r->in.lm_verifier != NULL) {
		char *new_pass;
		if (!convert_string_talloc_convenience(mem_ctx, lpcfg_iconv_convenience(dce_call->conn->dce_ctx->lp_ctx),
					  CH_UTF16, CH_UNIX,
					  (const char *)new_password.data,
					  new_password.length,
					  (void **)&new_pass, NULL, false)) {
			E_deshash(new_pass, new_lm_hash);
			E_old_pw_hash(new_nt_hash, lm_pwd->hash, lm_verifier.hash);
			if (memcmp(lm_verifier.hash, r->in.lm_verifier->hash, 16) != 0) {
				status = NT_STATUS_WRONG_PASSWORD;
				goto failed;
			}
		}
	}

	/* Connect to a SAMDB with user privileges for the password change */
	sam_ctx = samdb_connect(mem_ctx, dce_call->event_ctx,
				dce_call->conn->dce_ctx->lp_ctx,
				dce_call->conn->auth_state.session_info, 0);
	if (sam_ctx == NULL) {
		return NT_STATUS_INVALID_SYSTEM_SERVICE;
	}

	ret = ldb_transaction_start(sam_ctx);
	if (ret != LDB_SUCCESS) {
		DEBUG(1, ("Failed to start transaction: %s\n", ldb_errstring(sam_ctx)));
		return NT_STATUS_TRANSACTION_ABORTED;
	}

	/* Performs the password modification. We pass the old hashes read out
	 * from the database since they were already checked against the user-
	 * provided ones. */
	status = samdb_set_password(sam_ctx, mem_ctx,
				    user_dn, NULL,
				    &new_password,
				    NULL, NULL,
				    lm_pwd, nt_pwd, /* this is a user password change */
				    &reason,
				    &dominfo);

	if (!NT_STATUS_IS_OK(status)) {
		ldb_transaction_cancel(sam_ctx);
		goto failed;
	}

	/* And this confirms it in a transaction commit */
	ret = ldb_transaction_commit(sam_ctx);
	if (ret != LDB_SUCCESS) {
		DEBUG(1,("Failed to commit transaction to change password on %s: %s\n",
			 ldb_dn_get_linearized(user_dn),
			 ldb_errstring(sam_ctx)));
		status = NT_STATUS_TRANSACTION_ABORTED;
		goto failed;
	}

	return NT_STATUS_OK;

failed:
	reject = talloc_zero(mem_ctx, struct userPwdChangeFailureInformation);
	if (reject != NULL) {
		reject->extendedFailureReason = reason;

		*r->out.reject = reject;
	}

	*r->out.dominfo = dominfo;

	return status;
}


/*
  samr_ChangePasswordUser2

  easy - just a subset of samr_ChangePasswordUser3
*/
NTSTATUS dcesrv_samr_ChangePasswordUser2(struct dcesrv_call_state *dce_call,
					 TALLOC_CTX *mem_ctx,
					 struct samr_ChangePasswordUser2 *r)
{
	struct samr_ChangePasswordUser3 r2;
	struct samr_DomInfo1 *dominfo = NULL;
	struct userPwdChangeFailureInformation *reject = NULL;

	r2.in.server = r->in.server;
	r2.in.account = r->in.account;
	r2.in.nt_password = r->in.nt_password;
	r2.in.nt_verifier = r->in.nt_verifier;
	r2.in.lm_change = r->in.lm_change;
	r2.in.lm_password = r->in.lm_password;
	r2.in.lm_verifier = r->in.lm_verifier;
	r2.in.password3 = NULL;
	r2.out.dominfo = &dominfo;
	r2.out.reject = &reject;

	return dcesrv_samr_ChangePasswordUser3(dce_call, mem_ctx, &r2);
}


/*
  set password via a samr_CryptPassword buffer
*/
NTSTATUS samr_set_password(struct dcesrv_call_state *dce_call,
			   struct ldb_context *sam_ctx,
			   struct ldb_dn *account_dn, struct ldb_dn *domain_dn,
			   TALLOC_CTX *mem_ctx,
			   struct samr_CryptPassword *pwbuf)
{
	NTSTATUS nt_status;
	DATA_BLOB new_password;
	DATA_BLOB session_key = data_blob(NULL, 0);

	nt_status = dcesrv_fetch_session_key(dce_call->conn, &session_key);
	if (!NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	}

	arcfour_crypt_blob(pwbuf->data, 516, &session_key);

	if (!extract_pw_from_buffer(mem_ctx, pwbuf->data, &new_password)) {
		DEBUG(3,("samr: failed to decode password buffer\n"));
		return NT_STATUS_WRONG_PASSWORD;
	}

	/* set the password - samdb needs to know both the domain and user DNs,
	   so the domain password policy can be used */
	return samdb_set_password(sam_ctx, mem_ctx,
				  account_dn, domain_dn,
				  &new_password,
				  NULL, NULL,
				  NULL, NULL, /* This is a password set, not change */
				  NULL, NULL);
}


/*
  set password via a samr_CryptPasswordEx buffer
*/
NTSTATUS samr_set_password_ex(struct dcesrv_call_state *dce_call,
			      struct ldb_context *sam_ctx,
			      struct ldb_dn *account_dn,
			      struct ldb_dn *domain_dn,
			      TALLOC_CTX *mem_ctx,
			      struct samr_CryptPasswordEx *pwbuf)
{
	NTSTATUS nt_status;
	DATA_BLOB new_password;
	DATA_BLOB co_session_key;
	DATA_BLOB session_key = data_blob(NULL, 0);
	MD5_CTX ctx;

	nt_status = dcesrv_fetch_session_key(dce_call->conn, &session_key);
	if (!NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	}

	co_session_key = data_blob_talloc(mem_ctx, NULL, 16);
	if (!co_session_key.data) {
		return NT_STATUS_NO_MEMORY;
	}

	MD5Init(&ctx);
	MD5Update(&ctx, &pwbuf->data[516], 16);
	MD5Update(&ctx, session_key.data, session_key.length);
	MD5Final(co_session_key.data, &ctx);

	arcfour_crypt_blob(pwbuf->data, 516, &co_session_key);

	if (!extract_pw_from_buffer(mem_ctx, pwbuf->data, &new_password)) {
		DEBUG(3,("samr: failed to decode password buffer\n"));
		return NT_STATUS_WRONG_PASSWORD;
	}

	/* set the password - samdb needs to know both the domain and user DNs,
	   so the domain password policy can be used */
	return samdb_set_password(sam_ctx, mem_ctx,
				  account_dn, domain_dn,
				  &new_password,
				  NULL, NULL,
				  NULL, NULL, /* This is a password set, not change */
				  NULL, NULL);
}

/*
  set password via encrypted NT and LM hash buffers
*/
NTSTATUS samr_set_password_buffers(struct dcesrv_call_state *dce_call,
				   struct ldb_context *sam_ctx,
				   struct ldb_dn *account_dn,
				   struct ldb_dn *domain_dn,
				   TALLOC_CTX *mem_ctx,
				   const uint8_t *lm_pwd_hash,
				   const uint8_t *nt_pwd_hash)
{
	struct samr_Password *d_lm_pwd_hash = NULL, *d_nt_pwd_hash = NULL;
	DATA_BLOB session_key = data_blob(NULL, 0);
	DATA_BLOB in, out;
	NTSTATUS nt_status = NT_STATUS_OK;

	nt_status = dcesrv_fetch_session_key(dce_call->conn, &session_key);
	if (!NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	}

	if (lm_pwd_hash != NULL) {
		in = data_blob_const(lm_pwd_hash, 16);
		out = data_blob_talloc_zero(mem_ctx, 16);

		sess_crypt_blob(&out, &in, &session_key, false);

		d_lm_pwd_hash = (struct samr_Password *) out.data;
	}
	if (nt_pwd_hash != NULL) {
		in = data_blob_const(nt_pwd_hash, 16);
		out = data_blob_talloc_zero(mem_ctx, 16);

		sess_crypt_blob(&out, &in, &session_key, false);

		d_nt_pwd_hash = (struct samr_Password *) out.data;
	}

	if ((d_lm_pwd_hash != NULL) || (d_nt_pwd_hash != NULL)) {
		nt_status = samdb_set_password(sam_ctx, mem_ctx, account_dn,
					       domain_dn, NULL,
					       d_lm_pwd_hash, d_nt_pwd_hash,
					       NULL, NULL, /* this is a password set */
					       NULL, NULL);
	}

	return nt_status;
}
