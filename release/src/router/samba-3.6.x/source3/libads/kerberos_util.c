/*
   Unix SMB/CIFS implementation.
   krb5 set password implementation
   Copyright (C) Andrew Tridgell 2001
   Copyright (C) Remus Koos 2001 (remuskoos@yahoo.com)

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
#include "smb_krb5.h"
#include "ads.h"

#ifdef HAVE_KRB5

/**
 * Set the machine account password
 * @param ads connection to ads server
 * @param hostname machine whose password is being set
 * @param password new password
 * @return status of password change
 **/
ADS_STATUS ads_set_machine_password(ADS_STRUCT *ads,
				    const char *machine_account,
				    const char *password)
{
	ADS_STATUS status;
	char *principal = NULL;

	/*
	  we need to use the '$' form of the name here (the machine account name),
	  as otherwise the server might end up setting the password for a user
	  instead
	 */
	if (asprintf(&principal, "%s@%s", machine_account, ads->config.realm) < 0) {
		return ADS_ERROR_NT(NT_STATUS_NO_MEMORY);
	}

	status = ads_krb5_set_password(ads->auth.kdc_server, principal,
				       password, ads->auth.time_offset);

	SAFE_FREE(principal);
	return status;
}

/* run kinit to setup our ccache */
int ads_kinit_password(ADS_STRUCT *ads)
{
	char *s;
	int ret;
	const char *account_name;
	fstring acct_name;

	if (ads->auth.flags & ADS_AUTH_USER_CREDS) {
		account_name = ads->auth.user_name;
		goto got_accountname;
	}

	if ( IS_DC ) {
		/* this will end up getting a ticket for DOMAIN@RUSTED.REA.LM */
		account_name = lp_workgroup();
	} else {
		/* always use the sAMAccountName for security = domain */
		/* global_myname()$@REA.LM */
		if ( lp_security() == SEC_DOMAIN ) {
			fstr_sprintf( acct_name, "%s$", global_myname() );
			account_name = acct_name;
		}
		else
			/* This looks like host/global_myname()@REA.LM */
			account_name = ads->auth.user_name;
	}

 got_accountname:
	if (asprintf(&s, "%s@%s", account_name, ads->auth.realm) == -1) {
		return KRB5_CC_NOMEM;
	}

	if (!ads->auth.password) {
		SAFE_FREE(s);
		return KRB5_LIBOS_CANTREADPWD;
	}

	ret = kerberos_kinit_password_ext(s, ads->auth.password, ads->auth.time_offset,
			&ads->auth.tgt_expire, NULL, NULL, False, False, ads->auth.renewable,
			NULL);

	if (ret) {
		DEBUG(0,("kerberos_kinit_password %s failed: %s\n",
			 s, error_message(ret)));
	}
	SAFE_FREE(s);
	return ret;
}

#endif
