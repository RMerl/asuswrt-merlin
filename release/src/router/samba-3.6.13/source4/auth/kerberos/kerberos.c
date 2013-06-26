/* 
   Unix SMB/CIFS implementation.
   kerberos utility library
   Copyright (C) Andrew Tridgell 2001
   Copyright (C) Remus Koos 2001
   Copyright (C) Nalin Dahyabhai 2004.
   Copyright (C) Jeremy Allison 2004.
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2004-2005

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
#include "system/kerberos.h"
#include "auth/kerberos/kerberos.h"

#ifdef HAVE_KRB5

/*
  simulate a kinit, putting the tgt in the given credentials cache. 
  Orignally by remus@snapserver.com
 
  This version is built to use a keyblock, rather than needing the
  original password.

  The impersonate_principal is the principal if NULL, or the principal to impersonate

  The target_service defaults to the krbtgt if NULL, but could be kpasswd/realm or the local service (if we are doing s4u2self)
*/
 krb5_error_code kerberos_kinit_keyblock_cc(krb5_context ctx, krb5_ccache cc, 
					    krb5_principal principal, krb5_keyblock *keyblock,
					    const char *target_service,
					    krb5_get_init_creds_opt *krb_options,
					    time_t *expire_time, time_t *kdc_time)
{
	krb5_error_code code = 0;
	krb5_creds my_creds;

	if ((code = krb5_get_init_creds_keyblock(ctx, &my_creds, principal, keyblock,
						 0, target_service, krb_options))) {
		return code;
	}
	
	if ((code = krb5_cc_initialize(ctx, cc, principal))) {
		krb5_free_cred_contents(ctx, &my_creds);
		return code;
	}
	
	if ((code = krb5_cc_store_cred(ctx, cc, &my_creds))) {
		krb5_free_cred_contents(ctx, &my_creds);
		return code;
	}
	
	if (expire_time) {
		*expire_time = (time_t) my_creds.times.endtime;
	}

	if (kdc_time) {
		*kdc_time = (time_t) my_creds.times.starttime;
	}

	krb5_free_cred_contents(ctx, &my_creds);
	
	return 0;
}

/*
  simulate a kinit, putting the tgt in the given credentials cache. 
  Orignally by remus@snapserver.com

  The impersonate_principal is the principal if NULL, or the principal to impersonate

  The target_service defaults to the krbtgt if NULL, but could be kpasswd/realm or the local service (if we are doing s4u2self)

*/
 krb5_error_code kerberos_kinit_password_cc(krb5_context ctx, krb5_ccache cc, 
					    krb5_principal principal, const char *password,
					    krb5_principal impersonate_principal, const char *target_service,
					    krb5_get_init_creds_opt *krb_options,
					    time_t *expire_time, time_t *kdc_time)
{
	krb5_error_code code = 0;
	krb5_creds my_creds;
	krb5_creds *impersonate_creds;
	krb5_get_creds_opt options;

	/* If we are not impersonating, then get this ticket for the
	 * target service, otherwise a krbtgt, and get the next ticket
	 * for the target */
	if ((code = krb5_get_init_creds_password(ctx, &my_creds, principal, password, 
						 NULL, NULL,
						 0,
						 impersonate_principal ? NULL : target_service,
						 krb_options))) {
		return code;
	}

	if ((code = krb5_cc_initialize(ctx, cc, principal))) {
		krb5_free_cred_contents(ctx, &my_creds);
		return code;
	}
	
	if ((code = krb5_cc_store_cred(ctx, cc, &my_creds))) {
		krb5_free_cred_contents(ctx, &my_creds);
		return code;
	}
	
	if (expire_time) {
		*expire_time = (time_t) my_creds.times.endtime;
	}

	if (kdc_time) {
		*kdc_time = (time_t) my_creds.times.starttime;
	}

	krb5_free_cred_contents(ctx, &my_creds);
	
	if (code == 0 && impersonate_principal) {
		krb5_principal target_princ;
		if ((code = krb5_get_creds_opt_alloc(ctx, &options))) {
			return code;
		}

		if ((code = krb5_get_creds_opt_set_impersonate(ctx, options, impersonate_principal))) {
			krb5_get_creds_opt_free(ctx, options);
			return code;
		}

		if ((code = krb5_parse_name(ctx, target_service, &target_princ))) {
			krb5_get_creds_opt_free(ctx, options);
			return code;
		}

		if ((code = krb5_principal_set_realm(ctx, target_princ, krb5_principal_get_realm(ctx, principal)))) {
			krb5_get_creds_opt_free(ctx, options);
			krb5_free_principal(ctx, target_princ);
			return code;
		}

		if ((code = krb5_get_creds(ctx, options, cc, target_princ, &impersonate_creds))) {
			krb5_free_principal(ctx, target_princ);
			krb5_get_creds_opt_free(ctx, options);
			return code;
		}

		krb5_free_principal(ctx, target_princ);

		code = krb5_cc_store_cred(ctx, cc, impersonate_creds);
		krb5_get_creds_opt_free(ctx, options);
		krb5_free_creds(ctx, impersonate_creds);
	}

	return 0;
}


#endif
