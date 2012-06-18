/* 
   Unix SMB/CIFS implementation.
   Credentials popt routines

   Copyright (C) Jelmer Vernooij 2002,2003,2005

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
#include "lib/cmdline/popt_common.h"
#include "lib/cmdline/credentials.h"
#include "auth/credentials/credentials.h"
#include "auth/gensec/gensec.h"
#include "param/param.h"

/* Handle command line options:
 *		-U,--user
 *		-A,--authentication-file
 *		-k,--use-kerberos
 *		-N,--no-pass
 *		-S,--signing
 *              -P --machine-pass
 *                 --simple-bind-dn
 *                 --password
 */


static bool dont_ask;

enum opt { OPT_SIMPLE_BIND_DN, OPT_PASSWORD, OPT_KERBEROS };

/*
  disable asking for a password
*/
void popt_common_dont_ask(void)
{
	dont_ask = true;
}

static void popt_common_credentials_callback(poptContext con, 
						enum poptCallbackReason reason,
						const struct poptOption *opt,
						const char *arg, const void *data)
{
	if (reason == POPT_CALLBACK_REASON_PRE) {
		cmdline_credentials = cli_credentials_init(talloc_autofree_context());
		return;
	}
	
	if (reason == POPT_CALLBACK_REASON_POST) {
		cli_credentials_guess(cmdline_credentials, cmdline_lp_ctx);

		if (!dont_ask) {
			cli_credentials_set_cmdline_callbacks(cmdline_credentials);
		}
		return;
	}

	switch(opt->val) {
	case 'U':
	{
		char *lp;
		
		cli_credentials_parse_string(cmdline_credentials, arg, CRED_SPECIFIED);
		/* This breaks the abstraction, including the const above */
		if ((lp=strchr_m(arg,'%'))) {
			lp[0]='\0';
			lp++;
			/* Try to prevent this showing up in ps */
			memset(lp,0,strlen(lp));
		}
	}
	break;

	case OPT_PASSWORD:
		cli_credentials_set_password(cmdline_credentials, arg, CRED_SPECIFIED);
		/* Try to prevent this showing up in ps */
		memset(discard_const(arg),0,strlen(arg));
		break;

	case 'A':
		cli_credentials_parse_file(cmdline_credentials, arg, CRED_SPECIFIED);
		break;

	case 'P':
		/* Later, after this is all over, get the machine account details from the secrets.ldb */
		cli_credentials_set_machine_account_pending(cmdline_credentials, cmdline_lp_ctx);
		break;

	case OPT_KERBEROS:
	{
		bool use_kerberos = true;
		/* Force us to only use kerberos */
		if (arg) {
			if (!set_boolean(arg, &use_kerberos)) {
				fprintf(stderr, "Error parsing -k %s\n", arg);
				exit(1);
				break;
			}
		}
		
		cli_credentials_set_kerberos_state(cmdline_credentials, 
						   use_kerberos 
						   ? CRED_MUST_USE_KERBEROS
						   : CRED_DONT_USE_KERBEROS);
		break;
	}
		
	case OPT_SIMPLE_BIND_DN:
		cli_credentials_set_bind_dn(cmdline_credentials, arg);
		break;
	}
}



struct poptOption popt_common_credentials[] = {
	{ NULL, 0, POPT_ARG_CALLBACK|POPT_CBFLAG_PRE|POPT_CBFLAG_POST, (void *)popt_common_credentials_callback },
	{ "user", 'U', POPT_ARG_STRING, NULL, 'U', "Set the network username", "[DOMAIN/]USERNAME[%PASSWORD]" },
	{ "no-pass", 'N', POPT_ARG_NONE, &dont_ask, 'N', "Don't ask for a password" },
	{ "password", 0, POPT_ARG_STRING, NULL, OPT_PASSWORD, "Password" },
	{ "authentication-file", 'A', POPT_ARG_STRING, NULL, 'A', "Get the credentials from a file", "FILE" },
	{ "machine-pass", 'P', POPT_ARG_NONE, NULL, 'P', "Use stored machine account password (implies -k)" },
	{ "simple-bind-dn", 0, POPT_ARG_STRING, NULL, OPT_SIMPLE_BIND_DN, "DN to use for a simple bind" },
	{ "kerberos", 'k', POPT_ARG_STRING, NULL, OPT_KERBEROS, "Use Kerberos" },
	{ NULL }
};
