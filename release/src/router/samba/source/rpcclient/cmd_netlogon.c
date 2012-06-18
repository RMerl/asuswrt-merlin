/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   NT Domain Authentication SMB / MSRPC client
   Copyright (C) Andrew Tridgell 1994-1997
   Copyright (C) Luke Kenneth Casson Leighton 1996-1997
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/



#ifdef SYSLOG
#undef SYSLOG
#endif

#include "includes.h"
#include "nterr.h"

extern int DEBUGLEVEL;

#define DEBUG_TESTING

extern struct cli_state *smb_cli;

extern FILE* out_hnd;


/****************************************************************************
experimental nt login.
****************************************************************************/
void cmd_netlogon_login_test(struct client_info *info)
{
	extern BOOL global_machine_password_needs_changing;

	fstring nt_user_name;
	fstring password;
	BOOL res = True;
	char *nt_password;
	unsigned char trust_passwd[16];

#if 0
	/* machine account passwords */
	pstring new_mach_pwd;

	/* initialisation */
	new_mach_pwd[0] = 0;
#endif

	if (!next_token(NULL, nt_user_name, NULL, sizeof(nt_user_name)))
	{
		fstrcpy(nt_user_name, smb_cli->user_name);
		if (nt_user_name[0] == 0)
		{
			fprintf(out_hnd,"ntlogin: must specify username with anonymous connection\n");
			return;
		}
	}

	if (next_token(NULL, password, NULL, sizeof(password)))
	{
		nt_password = password;
	}
	else
	{
		nt_password = getpass("Enter NT Login password:");
	}

	DEBUG(5,("do_nt_login_test: username %s\n", nt_user_name));

	res = res ? trust_get_passwd(trust_passwd, smb_cli->domain, info->myhostname) : False;

#if 0
	/* check whether the user wants to change their machine password */
	res = res ? trust_account_check(info->dest_ip, info->dest_host,
	                                info->myhostname, smb_cli->domain,
	                                info->mach_acct, new_mach_pwd) : False;
#endif
	/* open NETLOGON session.  negotiate credentials */
	res = res ? cli_nt_session_open(smb_cli, PIPE_NETLOGON) : False;

	res = res ? cli_nt_setup_creds(smb_cli, trust_passwd) : False;

	/* change the machine password? */
	if (global_machine_password_needs_changing)
	{
		unsigned char new_trust_passwd[16];
		generate_random_buffer(new_trust_passwd, 16, True);
		res = res ? cli_nt_srv_pwset(smb_cli, new_trust_passwd) : False;

		if (res)
		{
			global_machine_password_needs_changing = !set_trust_account_password(new_trust_passwd);
		}

		memset(new_trust_passwd, 0, 16);
	}

	memset(trust_passwd, 0, 16);

	/* do an NT login */
	res = res ? cli_nt_login_interactive(smb_cli,
	                 smb_cli->domain, nt_user_name,
	                 getuid(), nt_password,
	                 &info->dom.ctr, &info->dom.user_info3) : False;

	/*** clear out the password ***/
	memset(password, 0, sizeof(password));

	/* ok!  you're logged in!  do anything you like, then... */

	/* do an NT logout */
	res = res ? cli_nt_logoff(smb_cli, &info->dom.ctr) : False;

	/* close the session */
	cli_nt_session_close(smb_cli);

	fprintf(out_hnd,"cmd_nt_login: login (%s) test succeeded: %s\n",
		nt_user_name, BOOLSTR(res));
}

