/* 
   Unix SMB/CIFS implementation.
   Password and authentication handling
   Copyright (C) Andrew Bartlett		2001
   Copyright (C) Jeremy Allison			2001
   Copyright (C) Simo Sorce			2005
   
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
#include "auth/auth.h"
#include "auth/ntlm/auth_proto.h"
#include "system/passwd.h" /* needed by some systems for struct passwd */
#include "lib/socket/socket.h"
#include "lib/tsocket/tsocket.h"
#include "../libcli/auth/pam_errors.h"
#include "param/param.h"

/* TODO: look at how to best fill in parms retrieveing a struct passwd info
 * except in case USER_INFO_DONT_CHECK_UNIX_ACCOUNT is set
 */
static NTSTATUS authunix_make_user_info_dc(TALLOC_CTX *mem_ctx,
					  const char *netbios_name,
					  const struct auth_usersupplied_info *user_info,
					  struct passwd *pwd,
					  struct auth_user_info_dc **_user_info_dc)
{
	struct auth_user_info_dc *user_info_dc;
	struct auth_user_info *info;
	NTSTATUS status;

	/* This is a real, real hack */
	if (pwd->pw_uid == 0) {
		status = auth_system_user_info_dc(mem_ctx, netbios_name, &user_info_dc);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}

		user_info_dc->info = info = talloc_zero(user_info_dc, struct auth_user_info);
		NT_STATUS_HAVE_NO_MEMORY(user_info_dc->info);

		info->account_name = talloc_steal(info, pwd->pw_name);
		NT_STATUS_HAVE_NO_MEMORY(info->account_name);
		
		info->domain_name = talloc_strdup(info, "unix");
		NT_STATUS_HAVE_NO_MEMORY(info->domain_name);
	} else {
		user_info_dc = talloc(mem_ctx, struct auth_user_info_dc);
		NT_STATUS_HAVE_NO_MEMORY(user_info_dc);
		
		user_info_dc->info = info = talloc_zero(user_info_dc, struct auth_user_info);
		NT_STATUS_HAVE_NO_MEMORY(user_info_dc->info);

		info->authenticated = true;
		
		info->account_name = talloc_steal(info, pwd->pw_name);
		NT_STATUS_HAVE_NO_MEMORY(info->account_name);
		
		info->domain_name = talloc_strdup(info, "unix");
		NT_STATUS_HAVE_NO_MEMORY(info->domain_name);

		/* This isn't in any way correct.. */
		user_info_dc->num_sids = 0;
		user_info_dc->sids = NULL;
	}
	user_info_dc->user_session_key = data_blob(NULL,0);
	user_info_dc->lm_session_key = data_blob(NULL,0);

	info->full_name = talloc_steal(info, pwd->pw_gecos);
	NT_STATUS_HAVE_NO_MEMORY(info->full_name);
	info->logon_script = talloc_strdup(info, "");
	NT_STATUS_HAVE_NO_MEMORY(info->logon_script);
	info->profile_path = talloc_strdup(info, "");
	NT_STATUS_HAVE_NO_MEMORY(info->profile_path);
	info->home_directory = talloc_strdup(info, "");
	NT_STATUS_HAVE_NO_MEMORY(info->home_directory);
	info->home_drive = talloc_strdup(info, "");
	NT_STATUS_HAVE_NO_MEMORY(info->home_drive);

	info->last_logon = 0;
	info->last_logoff = 0;
	info->acct_expiry = 0;
	info->last_password_change = 0;
	info->allow_password_change = 0;
	info->force_password_change = 0;
	info->logon_count = 0;
	info->bad_password_count = 0;
	info->acct_flags = 0;

	*_user_info_dc = user_info_dc;

	return NT_STATUS_OK;
}

static NTSTATUS talloc_getpwnam(TALLOC_CTX *ctx, const char *username, struct passwd **pws)
{
        struct passwd *ret;
	struct passwd *from;

	*pws = NULL;

	ret = talloc(ctx, struct passwd);
	NT_STATUS_HAVE_NO_MEMORY(ret);

	from = getpwnam(username);
	if (!from) {
		return NT_STATUS_NO_SUCH_USER;
	}

        ret->pw_name = talloc_strdup(ctx, from->pw_name);
	NT_STATUS_HAVE_NO_MEMORY(ret->pw_name);

        ret->pw_passwd = talloc_strdup(ctx, from->pw_passwd);
	NT_STATUS_HAVE_NO_MEMORY(ret->pw_passwd);

        ret->pw_uid = from->pw_uid;
        ret->pw_gid = from->pw_gid;
        ret->pw_gecos = talloc_strdup(ctx, from->pw_gecos);
	NT_STATUS_HAVE_NO_MEMORY(ret->pw_gecos);

        ret->pw_dir = talloc_strdup(ctx, from->pw_dir);
	NT_STATUS_HAVE_NO_MEMORY(ret->pw_dir);

        ret->pw_shell = talloc_strdup(ctx, from->pw_shell);
	NT_STATUS_HAVE_NO_MEMORY(ret->pw_shell);

	*pws = ret;

	return NT_STATUS_OK;
}


#ifdef HAVE_SECURITY_PAM_APPL_H
#include <security/pam_appl.h>

struct smb_pam_user_info {
	const char *account_name;
	const char *plaintext_password;
};

#define COPY_STRING(s) (s) ? strdup(s) : NULL

/* 
 * Check user password
 * Currently it uses PAM only and fails on systems without PAM
 * Samba3 code located in pass_check.c is to ugly to be used directly it will
 * need major rework that's why pass_check.c is still there.
*/

static int smb_pam_conv(int num_msg, const struct pam_message **msg,
			 struct pam_response **reply, void *appdata_ptr)
{
	struct smb_pam_user_info *info = (struct smb_pam_user_info *)appdata_ptr;
	int num;

	if (num_msg <= 0) {
		*reply = NULL;
		return PAM_CONV_ERR;
	}
	
	/*
	 * Apparantly HPUX has a buggy PAM that doesn't support the
	 * data pointer. Fail if this is the case. JRA.
	 */

	if (info == NULL) {
		*reply = NULL;
		return PAM_CONV_ERR;
	}

	/*
	 * PAM frees memory in reply messages by itself
	 * so use malloc instead of talloc here.
	 */
	*reply = malloc_array_p(struct pam_response, num_msg);
	if (*reply == NULL) {
		return PAM_CONV_ERR;
	}

	for (num = 0; num < num_msg; num++) {
		switch  (msg[num]->msg_style) {
			case PAM_PROMPT_ECHO_ON:
				(*reply)[num].resp_retcode = PAM_SUCCESS;
				(*reply)[num].resp = COPY_STRING(info->account_name);
				break;

			case PAM_PROMPT_ECHO_OFF:
				(*reply)[num].resp_retcode = PAM_SUCCESS;
				(*reply)[num].resp = COPY_STRING(info->plaintext_password);
				break;

			case PAM_TEXT_INFO:
				(*reply)[num].resp_retcode = PAM_SUCCESS;
				(*reply)[num].resp = NULL;
				DEBUG(4,("PAM Info message in conversation function: %s\n", (msg[num]->msg)));
				break;

			case PAM_ERROR_MSG:
				(*reply)[num].resp_retcode = PAM_SUCCESS;
				(*reply)[num].resp = NULL;
				DEBUG(4,("PAM Error message in conversation function: %s\n", (msg[num]->msg)));
				break;

			default:
				while (num > 0) {
					SAFE_FREE((*reply)[num-1].resp);
					num--;
				}
				SAFE_FREE(*reply);
				*reply = NULL;
				DEBUG(1,("Error: PAM subsystme sent an UNKNOWN message type to the conversation function!\n"));
				return PAM_CONV_ERR;
		}
	}

	return PAM_SUCCESS;
}

/*
 * Start PAM authentication for specified account
 */

static NTSTATUS smb_pam_start(pam_handle_t **pamh, const char *account_name, const char *remote_host, struct pam_conv *pconv)
{
	int pam_error;

	if (account_name == NULL || remote_host == NULL) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	DEBUG(4,("smb_pam_start: PAM: Init user: %s\n", account_name));

	pam_error = pam_start("samba", account_name, pconv, pamh);
	if (pam_error != PAM_SUCCESS) {
		/* no valid pamh here, can we reliably call pam_strerror ? */
		DEBUG(4,("smb_pam_start: pam_start failed!\n"));
		return NT_STATUS_UNSUCCESSFUL;
	}

#ifdef PAM_RHOST
	DEBUG(4,("smb_pam_start: PAM: setting rhost to: %s\n", remote_host));
	pam_error = pam_set_item(*pamh, PAM_RHOST, remote_host);
	if (pam_error != PAM_SUCCESS) {
		NTSTATUS nt_status;

		DEBUG(4,("smb_pam_start: setting rhost failed with error: %s\n",
			 pam_strerror(*pamh, pam_error)));
		nt_status = pam_to_nt_status(pam_error);

		pam_error = pam_end(*pamh, 0);
		if (pam_error != PAM_SUCCESS) {
			/* no vaild pamh here, can we reliably call pam_strerror ? */
			DEBUG(4,("smb_pam_start: clean up failed, pam_end gave error %d.\n",
				 pam_error));
			return pam_to_nt_status(pam_error);
		}
		return nt_status;
	}
#endif
#ifdef PAM_TTY
	DEBUG(4,("smb_pam_start: PAM: setting tty\n"));
	pam_error = pam_set_item(*pamh, PAM_TTY, "samba");
	if (pam_error != PAM_SUCCESS) {
		NTSTATUS nt_status;

		DEBUG(4,("smb_pam_start: setting tty failed with error: %s\n",
			 pam_strerror(*pamh, pam_error)));
		nt_status = pam_to_nt_status(pam_error);

		pam_error = pam_end(*pamh, 0);
		if (pam_error != PAM_SUCCESS) {
			/* no vaild pamh here, can we reliably call pam_strerror ? */
			DEBUG(4,("smb_pam_start: clean up failed, pam_end gave error %d.\n",
				 pam_error));
			return pam_to_nt_status(pam_error);
		}
		return nt_status;
	}
#endif
	DEBUG(4,("smb_pam_start: PAM: Init passed for user: %s\n", account_name));

	return NT_STATUS_OK;
}

static NTSTATUS smb_pam_end(pam_handle_t *pamh)
{
	int pam_error;

	if (pamh != NULL) {
		pam_error = pam_end(pamh, 0);
		if (pam_error != PAM_SUCCESS) {
			/* no vaild pamh here, can we reliably call pam_strerror ? */
			DEBUG(4,("smb_pam_end: clean up failed, pam_end gave error %d.\n",
				 pam_error));
			return pam_to_nt_status(pam_error);
		}
		return NT_STATUS_OK;
	}

	DEBUG(2,("smb_pam_end: pamh is NULL, PAM not initialized ?\n"));
	return NT_STATUS_UNSUCCESSFUL;
}

/*
 * PAM Authentication Handler
 */
static NTSTATUS smb_pam_auth(pam_handle_t *pamh, bool allow_null_passwords, const char *user)
{
	int pam_error;

	/*
	 * To enable debugging set in /etc/pam.d/samba:
	 *	auth required /lib/security/pam_pwdb.so nullok shadow audit
	 */
	
	DEBUG(4,("smb_pam_auth: PAM: Authenticate User: %s\n", user));

	pam_error = pam_authenticate(pamh, PAM_SILENT | allow_null_passwords ? 0 : PAM_DISALLOW_NULL_AUTHTOK);
	switch( pam_error ){
		case PAM_AUTH_ERR:
			DEBUG(2, ("smb_pam_auth: PAM: Authentication Error for user %s\n", user));
			break;
		case PAM_CRED_INSUFFICIENT:
			DEBUG(2, ("smb_pam_auth: PAM: Insufficient Credentials for user %s\n", user));
			break;
		case PAM_AUTHINFO_UNAVAIL:
			DEBUG(2, ("smb_pam_auth: PAM: Authentication Information Unavailable for user %s\n", user));
			break;
		case PAM_USER_UNKNOWN:
			DEBUG(2, ("smb_pam_auth: PAM: Username %s NOT known to Authentication system\n", user));
			break;
		case PAM_MAXTRIES:
			DEBUG(2, ("smb_pam_auth: PAM: One or more authentication modules reports user limit for user %s exceeeded\n", user));
			break;
		case PAM_ABORT:
			DEBUG(0, ("smb_pam_auth: PAM: One or more PAM modules failed to load for user %s\n", user));
			break;
		case PAM_SUCCESS:
			DEBUG(4, ("smb_pam_auth: PAM: User %s Authenticated OK\n", user));
			break;
		default:
			DEBUG(0, ("smb_pam_auth: PAM: UNKNOWN ERROR while authenticating user %s\n", user));
			break;
	}

	return pam_to_nt_status(pam_error);
}

/* 
 * PAM Account Handler
 */
static NTSTATUS smb_pam_account(pam_handle_t *pamh, const char * user)
{
	int pam_error;

	DEBUG(4,("smb_pam_account: PAM: Account Management for User: %s\n", user));

	pam_error = pam_acct_mgmt(pamh, PAM_SILENT); /* Is user account enabled? */
	switch( pam_error ) {
		case PAM_AUTHTOK_EXPIRED:
			DEBUG(2, ("smb_pam_account: PAM: User %s is valid but password is expired\n", user));
			break;
		case PAM_ACCT_EXPIRED:
			DEBUG(2, ("smb_pam_account: PAM: User %s no longer permitted to access system\n", user));
			break;
		case PAM_AUTH_ERR:
			DEBUG(2, ("smb_pam_account: PAM: There was an authentication error for user %s\n", user));
			break;
		case PAM_PERM_DENIED:
			DEBUG(0, ("smb_pam_account: PAM: User %s is NOT permitted to access system at this time\n", user));
			break;
		case PAM_USER_UNKNOWN:
			DEBUG(0, ("smb_pam_account: PAM: User \"%s\" is NOT known to account management\n", user));
			break;
		case PAM_SUCCESS:
			DEBUG(4, ("smb_pam_account: PAM: Account OK for User: %s\n", user));
			break;
		default:
			DEBUG(0, ("smb_pam_account: PAM: UNKNOWN PAM ERROR (%d) during Account Management for User: %s\n", pam_error, user));
			break;
	}

	return pam_to_nt_status(pam_error);
}

/*
 * PAM Credential Setting
 */

static NTSTATUS smb_pam_setcred(pam_handle_t *pamh, const char * user)
{
	int pam_error;

	/*
	 * This will allow samba to aquire a kerberos token. And, when
	 * exporting an AFS cell, be able to /write/ to this cell.
	 */

	DEBUG(4,("PAM: Account Management SetCredentials for User: %s\n", user));

	pam_error = pam_setcred(pamh, (PAM_ESTABLISH_CRED|PAM_SILENT)); 
	switch( pam_error ) {
		case PAM_CRED_UNAVAIL:
			DEBUG(0, ("smb_pam_setcred: PAM: Credentials not found for user:%s\n", user ));
			break;
		case PAM_CRED_EXPIRED:
			DEBUG(0, ("smb_pam_setcred: PAM: Credentials for user: \"%s\" EXPIRED!\n", user ));
			break;
		case PAM_USER_UNKNOWN:
			DEBUG(0, ("smb_pam_setcred: PAM: User: \"%s\" is NOT known so can not set credentials!\n", user ));
			break;
		case PAM_CRED_ERR:
			DEBUG(0, ("smb_pam_setcred: PAM: Unknown setcredentials error - unable to set credentials for %s\n", user ));
			break;
		case PAM_SUCCESS:
			DEBUG(4, ("smb_pam_setcred: PAM: SetCredentials OK for User: %s\n", user));
			break;
		default:
			DEBUG(0, ("smb_pam_setcred: PAM: UNKNOWN PAM ERROR (%d) during SetCredentials for User: %s\n", pam_error, user));
			break;
	}

	return pam_to_nt_status(pam_error);
}

static NTSTATUS check_unix_password(TALLOC_CTX *ctx, struct loadparm_context *lp_ctx,
				    const struct auth_usersupplied_info *user_info, struct passwd **pws)
{
	struct smb_pam_user_info *info;
	struct pam_conv *pamconv;
	pam_handle_t *pamh;
	NTSTATUS nt_status;

	info = talloc(ctx, struct smb_pam_user_info);
	if (info == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	info->account_name = user_info->mapped.account_name;
	info->plaintext_password = user_info->password.plaintext;

	pamconv = talloc(ctx, struct pam_conv);
	if (pamconv == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	pamconv->conv = smb_pam_conv;
	pamconv->appdata_ptr = (void *)info;

	/* TODO:
	 * check for user_info->flags & USER_INFO_CASE_INSENSITIVE_USERNAME
	 * if true set up a crack name routine.
	 */

	nt_status = smb_pam_start(&pamh, user_info->mapped.account_name,
			user_info->remote_host ? tsocket_address_inet_addr_string(user_info->remote_host, ctx) : NULL, pamconv);
	if (!NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	}

	nt_status = smb_pam_auth(pamh, lpcfg_null_passwords(lp_ctx), user_info->mapped.account_name);
	if (!NT_STATUS_IS_OK(nt_status)) {
		smb_pam_end(pamh);
		return nt_status;
	}

	if ( ! (user_info->flags & USER_INFO_DONT_CHECK_UNIX_ACCOUNT)) {

		nt_status = smb_pam_account(pamh, user_info->mapped.account_name);
		if (!NT_STATUS_IS_OK(nt_status)) {
			smb_pam_end(pamh);
			return nt_status;
		}

		nt_status = smb_pam_setcred(pamh, user_info->mapped.account_name);
		if (!NT_STATUS_IS_OK(nt_status)) {
			smb_pam_end(pamh);
			return nt_status;
		}
	}

	smb_pam_end(pamh);

	nt_status = talloc_getpwnam(ctx, user_info->mapped.account_name, pws);
	if (!NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	}

	return NT_STATUS_OK;	
}

#else

/****************************************************************************
core of password checking routine
****************************************************************************/
static NTSTATUS password_check(const char *username, const char *password,
					const char *crypted, const char *salt)
{
	bool ret;

#ifdef WITH_AFS
	if (afs_auth(username, password))
		return NT_STATUS_OK;
#endif /* WITH_AFS */

#ifdef WITH_DFS
	if (dfs_auth(username, password))
		return NT_STATUS_OK;
#endif /* WITH_DFS */

#ifdef OSF1_ENH_SEC
	
	ret = (strcmp(osf1_bigcrypt(password, salt), crypted) == 0);

	if (!ret) {
		DEBUG(2,
		      ("OSF1_ENH_SEC failed. Trying normal crypt.\n"));
		ret = (strcmp((char *)crypt(password, salt), crypted) == 0);
	}
	if (ret) {
		return NT_STATUS_OK;
	} else {
		return NT_STATUS_WRONG_PASSWORD;
	}
	
#endif /* OSF1_ENH_SEC */
	
#ifdef ULTRIX_AUTH
	ret = (strcmp((char *)crypt16(password, salt), crypted) == 0);
	if (ret) {
		return NT_STATUS_OK;
        } else {
		return NT_STATUS_WRONG_PASSWORD;
	}
	
#endif /* ULTRIX_AUTH */
	
#ifdef LINUX_BIGCRYPT
	ret = (linux_bigcrypt(password, salt, crypted));
        if (ret) {
		return NT_STATUS_OK;
	} else {
		return NT_STATUS_WRONG_PASSWORD;
	}
#endif /* LINUX_BIGCRYPT */
	
#if defined(HAVE_BIGCRYPT) && defined(HAVE_CRYPT) && defined(USE_BOTH_CRYPT_CALLS)
	
	/*
	 * Some systems have bigcrypt in the C library but might not
	 * actually use it for the password hashes (HPUX 10.20) is
	 * a noteable example. So we try bigcrypt first, followed
	 * by crypt.
	 */

	if (strcmp(bigcrypt(password, salt), crypted) == 0)
		return NT_STATUS_OK;
	else
		ret = (strcmp((char *)crypt(password, salt), crypted) == 0);
	if (ret) {
		return NT_STATUS_OK;
	} else {
		return NT_STATUS_WRONG_PASSWORD;
	}
#else /* HAVE_BIGCRYPT && HAVE_CRYPT && USE_BOTH_CRYPT_CALLS */
	
#ifdef HAVE_BIGCRYPT
	ret = (strcmp(bigcrypt(password, salt), crypted) == 0);
        if (ret) {
		return NT_STATUS_OK;
	} else {
		return NT_STATUS_WRONG_PASSWORD;
	}
#endif /* HAVE_BIGCRYPT */
	
#ifndef HAVE_CRYPT
	DEBUG(1, ("Warning - no crypt available\n"));
	return NT_STATUS_LOGON_FAILURE;
#else /* HAVE_CRYPT */
	ret = (strcmp((char *)crypt(password, salt), crypted) == 0);
        if (ret) {
		return NT_STATUS_OK;
	} else {
		return NT_STATUS_WRONG_PASSWORD;
	}
#endif /* HAVE_CRYPT */
#endif /* HAVE_BIGCRYPT && HAVE_CRYPT && USE_BOTH_CRYPT_CALLS */
}

static NTSTATUS check_unix_password(TALLOC_CTX *ctx, struct loadparm_context *lp_ctx,
				    const struct auth_usersupplied_info *user_info, struct passwd **ret_passwd)
{
	char *username;
	char *password;
	char *pwcopy;
	char *salt;
	char *crypted;
	struct passwd *pws;
	NTSTATUS nt_status;
	int level = lpcfg_passwordlevel(lp_ctx);

	*ret_passwd = NULL;

	username = talloc_strdup(ctx, user_info->mapped.account_name);
	password = talloc_strdup(ctx, user_info->password.plaintext);

	nt_status = talloc_getpwnam(ctx, username, &pws);
	if (!NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	}

	crypted = pws->pw_passwd;
	salt = pws->pw_passwd;

#ifdef HAVE_GETSPNAM
	{
		struct spwd *spass;

		/* many shadow systems require you to be root to get
		   the password, in most cases this should already be
		   the case when this function is called, except
		   perhaps for IPC password changing requests */

		spass = getspnam(pws->pw_name);
		if (spass && spass->sp_pwdp) {
			crypted = talloc_strdup(ctx, spass->sp_pwdp);
			NT_STATUS_HAVE_NO_MEMORY(crypted);
			salt = talloc_strdup(ctx, spass->sp_pwdp);
			NT_STATUS_HAVE_NO_MEMORY(salt);
		}
	}
#elif defined(IA_UINFO)
	{
		char *ia_password;
		/* Need to get password with SVR4.2's ia_ functions
		   instead of get{sp,pw}ent functions. Required by
		   UnixWare 2.x, tested on version
		   2.1. (tangent@cyberport.com) */
		uinfo_t uinfo;
		if (ia_openinfo(pws->pw_name, &uinfo) != -1) {
			ia_get_logpwd(uinfo, &ia_password);
			crypted = talloc_strdup(ctx, ia_password);
			NT_STATUS_HAVE_NO_MEMORY(crypted);
		}
	}
#endif

#ifdef HAVE_GETPRPWNAM
	{
		struct pr_passwd *pr_pw = getprpwnam(pws->pw_name);
		if (pr_pw && pr_pw->ufld.fd_encrypt) {
			crypted = talloc_strdup(ctx, pr_pw->ufld.fd_encrypt);
			NT_STATUS_HAVE_NO_MEMORY(crypted);
		}
	}
#endif

#ifdef HAVE_GETPWANAM
	{
		struct passwd_adjunct *pwret;
		pwret = getpwanam(s);
		if (pwret && pwret->pwa_passwd) {
			crypted = talloc_strdup(ctx, pwret->pwa_passwd);
			NT_STATUS_HAVE_NO_MEMORY(crypted);
		}
	}
#endif

#ifdef OSF1_ENH_SEC
	{
		struct pr_passwd *mypasswd;
		DEBUG(5,("Checking password for user %s in OSF1_ENH_SEC\n", username));
		mypasswd = getprpwnam(username);
		if (mypasswd) {
			username = talloc_strdup(ctx, mypasswd->ufld.fd_name);
			NT_STATUS_HAVE_NO_MEMORY(username);
			crypted = talloc_strdup(ctx, mypasswd->ufld.fd_encrypt);
			NT_STATUS_HAVE_NO_MEMORY(crypted);
		} else {
			DEBUG(5,("OSF1_ENH_SEC: No entry for user %s in protected database !\n", username));
		}
	}
#endif

#ifdef ULTRIX_AUTH
	{
		AUTHORIZATION *ap = getauthuid(pws->pw_uid);
		if (ap) {
			crypted = talloc_strdup(ctx, ap->a_password);
			endauthent();
			NT_STATUS_HAVE_NO_MEMORY(crypted);
		}
	}
#endif

#if defined(HAVE_TRUNCATED_SALT)
	/* crypt on some platforms (HPUX in particular)
	   won't work with more than 2 salt characters. */
	salt[2] = 0;
#endif

	if (crypted[0] == '\0') {
		if (!lpcfg_null_passwords(lp_ctx)) {
			DEBUG(2, ("Disallowing %s with null password\n", username));
			return NT_STATUS_LOGON_FAILURE;
		}
		if (password == NULL) {
			DEBUG(3, ("Allowing access to %s with null password\n", username));
			*ret_passwd = pws;
			return NT_STATUS_OK;
		}
	}

	/* try it as it came to us */
	nt_status = password_check(username, password, crypted, salt);
        if (NT_STATUS_IS_OK(nt_status)) {
		*ret_passwd = pws;
		return nt_status;
	}
	else if (!NT_STATUS_EQUAL(nt_status, NT_STATUS_WRONG_PASSWORD)) {
		/* No point continuing if its not the password thats to blame (ie PAM disabled). */
		return nt_status;
	}

	if ( user_info->flags | USER_INFO_CASE_INSENSITIVE_PASSWORD) {
		return nt_status;
	}

	/* if the password was given to us with mixed case then we don't
	 * need to proceed as we know it hasn't been case modified by the
	 * client */
	if (strhasupper(password) && strhaslower(password)) {
		return nt_status;
	}

	/* make a copy of it */
	pwcopy = talloc_strdup(ctx, password);
	if (!pwcopy)
		return NT_STATUS_NO_MEMORY;

	/* try all lowercase if it's currently all uppercase */
	if (strhasupper(pwcopy)) {
		strlower(pwcopy);
		nt_status = password_check(username, pwcopy, crypted, salt);
		if NT_STATUS_IS_OK(nt_status) {
			*ret_passwd = pws;
			return nt_status;
		}
	}

	/* give up? */
	if (level < 1) {
		return NT_STATUS_WRONG_PASSWORD;
	}

	/* last chance - all combinations of up to level chars upper! */
	strlower(pwcopy);

#if 0
        if (NT_STATUS_IS_OK(nt_status = string_combinations(pwcopy, password_check, level))) {
		*ret_passwd = pws;
		return nt_status;
	}
#endif   
	return NT_STATUS_WRONG_PASSWORD;
}

#endif

/** Check a plaintext username/password
 *
 **/

static NTSTATUS authunix_want_check(struct auth_method_context *ctx,
				    TALLOC_CTX *mem_ctx,
				    const struct auth_usersupplied_info *user_info)
{
	if (!user_info->mapped.account_name || !*user_info->mapped.account_name) {
		return NT_STATUS_NOT_IMPLEMENTED;
	}

	return NT_STATUS_OK;
}

static NTSTATUS authunix_check_password(struct auth_method_context *ctx,
					TALLOC_CTX *mem_ctx,
					const struct auth_usersupplied_info *user_info,
					struct auth_user_info_dc **user_info_dc)
{
	TALLOC_CTX *check_ctx;
	NTSTATUS nt_status;
	struct passwd *pwd;

	if (user_info->password_state != AUTH_PASSWORD_PLAIN) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	check_ctx = talloc_named_const(mem_ctx, 0, "check_unix_password");
	if (check_ctx == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	nt_status = check_unix_password(check_ctx, ctx->auth_ctx->lp_ctx, user_info, &pwd);
	if (!NT_STATUS_IS_OK(nt_status)) {
		talloc_free(check_ctx);
		return nt_status;
	}

	nt_status = authunix_make_user_info_dc(mem_ctx, lpcfg_netbios_name(ctx->auth_ctx->lp_ctx),
					      user_info, pwd, user_info_dc);
	if (!NT_STATUS_IS_OK(nt_status)) {
		talloc_free(check_ctx);
		return nt_status;
	}

	talloc_free(check_ctx);
	return NT_STATUS_OK;
}

static const struct auth_operations unix_ops = {
	.name		= "unix",
	.get_challenge	= auth_get_challenge_not_implemented,
	.want_check	= authunix_want_check,
	.check_password	= authunix_check_password
};

_PUBLIC_ NTSTATUS auth_unix_init(void)
{
	NTSTATUS ret;

	ret = auth_register(&unix_ops);
	if (!NT_STATUS_IS_OK(ret)) {
		DEBUG(0,("Failed to register unix auth backend!\n"));
		return ret;
	}

	return ret;
}
