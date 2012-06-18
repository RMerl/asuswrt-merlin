/* Unix NT password database implementation, version 0.7.5.
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 3 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, see <http://www.gnu.org/licenses/>.
*/

/* indicate the following groups are defined */
#define PAM_SM_AUTH

#include "includes.h"
#include "debug.h"

#ifndef LINUX

/* This is only used in the Sun implementation. */
#if defined(HAVE_SECURITY_PAM_APPL_H)
#include <security/pam_appl.h>
#elif defined(HAVE_PAM_PAM_APPL_H)
#include <pam/pam_appl.h>
#endif

#endif  /* LINUX */

#if defined(HAVE_SECURITY_PAM_MODULES_H)
#include <security/pam_modules.h>
#elif defined(HAVE_PAM_PAM_MODULES_H)
#include <pam/pam_modules.h>
#endif

#include "general.h"

#include "support.h"

#define AUTH_RETURN						\
do {								\
	/* Restore application signal handler */		\
	CatchSignal(SIGPIPE, SIGNAL_CAST oldsig_handler);	\
	if(ret_data) {						\
		*ret_data = retval;				\
		pam_set_data( pamh, "smb_setcred_return"	\
		              , (void *) ret_data, NULL );	\
	}							\
	return retval;						\
} while (0)

static int _smb_add_user(pam_handle_t *pamh, unsigned int ctrl,
                         const char *name, struct samu *sampass, bool exist);


/*
 * pam_sm_authenticate() authenticates users against the samba password file.
 *
 *	First, obtain the password from the user. Then use a
 *      routine in 'support.c' to authenticate the user.
 */

#define _SMB_AUTHTOK  "-SMB-PASS"

int pam_sm_authenticate(pam_handle_t *pamh, int flags,
                        int argc, const char **argv)
{
	unsigned int ctrl;
	int retval, *ret_data = NULL;
	struct samu *sampass = NULL;
	const char *name;
	void (*oldsig_handler)(int) = NULL;
	bool found;

	/* Points to memory managed by the PAM library. Do not free. */
	char *p = NULL;

	/* Samba initialization. */
	load_case_tables();
        lp_set_in_client(True);

	ctrl = set_ctrl(pamh, flags, argc, argv);

	/* Get a few bytes so we can pass our return value to
		pam_sm_setcred(). */
	ret_data = SMB_MALLOC_P(int);

	/* we need to do this before we call AUTH_RETURN */
	/* Getting into places that might use LDAP -- protect the app
	from a SIGPIPE it's not expecting */
	oldsig_handler = CatchSignal(SIGPIPE, SIGNAL_CAST SIG_IGN);

	/* get the username */
	retval = pam_get_user( pamh, &name, "Username: " );
	if ( retval != PAM_SUCCESS ) {
		if (on( SMB_DEBUG, ctrl )) {
			_log_err(pamh, LOG_DEBUG, "auth: could not identify user");
		}
		AUTH_RETURN;
	}
	if (on( SMB_DEBUG, ctrl )) {
		_log_err(pamh, LOG_DEBUG, "username [%s] obtained", name );
	}

	if (geteuid() != 0) {
		_log_err(pamh, LOG_DEBUG, "Cannot access samba password database, not running as root.");
		retval = PAM_AUTHINFO_UNAVAIL;
		AUTH_RETURN;
	}

	if (!initialize_password_db(True, NULL)) {
		_log_err(pamh, LOG_ALERT, "Cannot access samba password database" );
		retval = PAM_AUTHINFO_UNAVAIL;
		AUTH_RETURN;
	}

	sampass = samu_new( NULL );
    	if (!sampass) {
		_log_err(pamh, LOG_ALERT, "Cannot talloc a samu struct" );
		retval = nt_status_to_pam(NT_STATUS_NO_MEMORY);
		AUTH_RETURN;
	}

	found = pdb_getsampwnam( sampass, name );

	if (on( SMB_MIGRATE, ctrl )) {
		retval = _smb_add_user(pamh, ctrl, name, sampass, found);
		TALLOC_FREE(sampass);
		AUTH_RETURN;
	}

	if (!found) {
		_log_err(pamh, LOG_ALERT, "Failed to find entry for user %s.", name);
		retval = PAM_USER_UNKNOWN;
		TALLOC_FREE(sampass);
		sampass = NULL;
		AUTH_RETURN;
	}

	/* if this user does not have a password... */

	if (_smb_blankpasswd( ctrl, sampass )) {
		TALLOC_FREE(sampass);
		retval = PAM_SUCCESS;
		AUTH_RETURN;
	}

	/* get this user's authentication token */

	retval = _smb_read_password(pamh, ctrl, NULL, "Password: ", NULL, _SMB_AUTHTOK, &p);
	if (retval != PAM_SUCCESS ) {
		_log_err(pamh,LOG_CRIT, "auth: no password provided for [%s]", name);
		TALLOC_FREE(sampass);
		AUTH_RETURN;
	}

	/* verify the password of this user */

	retval = _smb_verify_password( pamh, sampass, p, ctrl );
	TALLOC_FREE(sampass);
	p = NULL;
	AUTH_RETURN;
}

/*
 * This function is for setting samba credentials.  If anyone comes up
 * with any credentials they think should be set, let me know.
 */

int pam_sm_setcred(pam_handle_t *pamh, int flags,
                   int argc, const char **argv)
{
	int retval, *pretval = NULL;

	retval = PAM_SUCCESS;

	_pam_get_data(pamh, "smb_setcred_return", &pretval);
	if(pretval) {
		retval = *pretval;
		SAFE_FREE(pretval);
	}
	pam_set_data(pamh, "smb_setcred_return", NULL, NULL);

	return retval;
}

/* Helper function for adding a user to the db. */
static int _smb_add_user(pam_handle_t *pamh, unsigned int ctrl,
                         const char *name, struct samu *sampass, bool exist)
{
	char *err_str = NULL;
	char *msg_str = NULL;
	const char *pass = NULL;
	int retval;

	/* Get the authtok; if we don't have one, silently fail. */
	retval = _pam_get_item( pamh, PAM_AUTHTOK, &pass );

	if (retval != PAM_SUCCESS) {
		_log_err(pamh, LOG_ALERT
			, "pam_get_item returned error to pam_sm_authenticate" );
		return PAM_AUTHTOK_RECOVER_ERR;
	} else if (pass == NULL) {
		return PAM_AUTHTOK_RECOVER_ERR;
	}

	/* Add the user to the db if they aren't already there. */
	if (!exist) {
		retval = NT_STATUS_IS_OK(local_password_change(name, LOCAL_ADD_USER|LOCAL_SET_PASSWORD,
					pass, &err_str, &msg_str));
		if (!retval && err_str) {
			make_remark(pamh, ctrl, PAM_ERROR_MSG, err_str );
		} else if (msg_str) {
			make_remark(pamh, ctrl, PAM_TEXT_INFO, msg_str );
		}
		pass = NULL;

		SAFE_FREE(err_str);
		SAFE_FREE(msg_str);
		return PAM_IGNORE;
	} else {
		/* mimick 'update encrypted' as long as the 'no pw req' flag is not set */
		if ( pdb_get_acct_ctrl(sampass) & ~ACB_PWNOTREQ ) {
			retval = NT_STATUS_IS_OK(local_password_change(name, LOCAL_SET_PASSWORD,
					pass, &err_str, &msg_str));
			if (!retval && err_str) {
				make_remark(pamh, ctrl, PAM_ERROR_MSG, err_str );
			} else if (msg_str) {
				make_remark(pamh, ctrl, PAM_TEXT_INFO, msg_str );
			}
		}
	}
    
	SAFE_FREE(err_str);
	SAFE_FREE(msg_str);
	pass = NULL;
	return PAM_IGNORE;
}

/* static module data */
#ifdef PAM_STATIC
struct pam_module _pam_smbpass_auth_modstruct = {
	"pam_smbpass",
	pam_sm_authenticate,
	pam_sm_setcred,
	NULL,
	NULL,
	NULL,
	NULL
};
#endif
