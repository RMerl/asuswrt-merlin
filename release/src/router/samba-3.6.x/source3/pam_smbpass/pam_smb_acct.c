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
#define PAM_SM_ACCT

#include "includes.h"

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


/*
 * pam_sm_acct_mgmt() verifies whether or not the account is disabled.
 *
 */

int pam_sm_acct_mgmt( pam_handle_t *pamh, int flags,
                      int argc, const char **argv )
{
	unsigned int ctrl;
	int retval;

	const char *name;
	struct samu *sampass = NULL;
	void (*oldsig_handler)(int);

	/* Samba initialization. */
	load_case_tables_library();
        lp_set_in_client(True);

	ctrl = set_ctrl(pamh, flags, argc, argv );

	/* get the username */

	retval = pam_get_user( pamh, &name, "Username: " );
	if (retval != PAM_SUCCESS) {
		if (on( SMB_DEBUG, ctrl )) {
			_log_err(pamh, LOG_DEBUG, "acct: could not identify user" );
		}
		return retval;
	}
	if (on( SMB_DEBUG, ctrl )) {
		_log_err(pamh, LOG_DEBUG, "acct: username [%s] obtained", name );
	}

	if (geteuid() != 0) {
		_log_err(pamh, LOG_DEBUG, "Cannot access samba password database, not running as root.");
		return PAM_AUTHINFO_UNAVAIL;
	}

	/* Getting into places that might use LDAP -- protect the app
		from a SIGPIPE it's not expecting */
	oldsig_handler = CatchSignal(SIGPIPE, SIG_IGN);
	if (!initialize_password_db(True, NULL)) {
	  _log_err(pamh, LOG_ALERT, "Cannot access samba password database" );
		CatchSignal(SIGPIPE, oldsig_handler);
		return PAM_AUTHINFO_UNAVAIL;
	}

	/* Get the user's record. */

	if (!(sampass = samu_new( NULL ))) {
		CatchSignal(SIGPIPE, oldsig_handler);
		/* malloc fail. */
		return nt_status_to_pam(NT_STATUS_NO_MEMORY);
	}

	if (!pdb_getsampwnam(sampass, name )) {
		_log_err(pamh, LOG_DEBUG, "acct: could not identify user");
		CatchSignal(SIGPIPE, oldsig_handler);
        	return PAM_USER_UNKNOWN;
	}

	/* check for lookup failure */
	if (!strlen(pdb_get_username(sampass)) ) {
		CatchSignal(SIGPIPE, oldsig_handler);
		return PAM_USER_UNKNOWN;
	}

	if (pdb_get_acct_ctrl(sampass) & ACB_DISABLED) {
		if (on( SMB_DEBUG, ctrl )) {
			_log_err(pamh, LOG_DEBUG,
				 "acct: account %s is administratively disabled", name);
		}
		make_remark( pamh, ctrl, PAM_ERROR_MSG
			, "Your account has been disabled; "
			"please see your system administrator." );

		CatchSignal(SIGPIPE, oldsig_handler);
		return PAM_ACCT_EXPIRED;
	}

	/* TODO: support for expired passwords. */

	CatchSignal(SIGPIPE, oldsig_handler);
	return PAM_SUCCESS;
}

/* static module data */
#ifdef PAM_STATIC
struct pam_module _pam_smbpass_acct_modstruct = {
     "pam_smbpass",
     NULL,
     NULL,
     pam_sm_acct_mgmt,
     NULL,
     NULL,
     NULL
};
#endif

