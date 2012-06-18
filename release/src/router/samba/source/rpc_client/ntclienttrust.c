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


/************************************************************************
 check workstation trust account status
 ************************************************************************/
BOOL trust_account_check(struct in_addr dest_ip, char *dest_host,
				char *hostname, char *domain, fstring mach_acct,
				fstring new_mach_pwd)
{
	pstring tmp;
	fstring mach_pwd;
	struct cli_state cli_trust;
	uchar lm_owf_mach_pwd[16];
	uchar nt_owf_mach_pwd[16];
	uchar lm_sess_pwd[24];
	uchar nt_sess_pwd[24];

	BOOL right_error_code = False;
	uint8 err_cls;
	uint32 err_num;

	char *start_mach_pwd;
	char *change_mach_pwd;

	/* initial machine password */
	fstrcpy(mach_pwd, hostname);
	strlower(mach_pwd);

	slprintf(tmp, sizeof(tmp) - 1,"Enter Workstation Trust Account password for [%s].\nDefault is [%s].\nPassword:",
				mach_acct, mach_pwd);

	start_mach_pwd = (char*)getpass(tmp);

	if (start_mach_pwd[0] != 0)
	{
		fstrcpy(mach_pwd, start_mach_pwd);
	}

	slprintf(tmp, sizeof(tmp)-1, "Enter new Workstation Trust Account password for [%s]\nPress Return to leave at old value.\nNew Password:",
				mach_acct);

	change_mach_pwd = (char*)getpass(tmp);

	if (change_mach_pwd[0] != 0)
	{
		fstrcpy(new_mach_pwd, change_mach_pwd);
	}
	else
	{
		DEBUG(1,("trust_account_check: password change not requested\n"));
		change_mach_pwd[0] = 0;
	}

	DEBUG(1,("initialise cli_trust connection\n"));

	if (!cli_initialise(&cli_trust))
	{
		DEBUG(1,("cli_initialise failed for cli_trust\n"));
		return False;
	}

	DEBUG(1,("server connect for cli_trust\n"));

	if (!server_connect_init(&cli_trust, hostname, dest_ip, dest_host))
	{
		cli_error(&cli_trust, &err_cls, &err_num, NULL);
		DEBUG(1,("server_connect_init failed (%s)\n", cli_errstr(&cli_trust)));

		cli_shutdown(&cli_trust);
		return False;
	}

	DEBUG(1,("server connect cli_trust succeeded\n"));

	nt_lm_owf_gen(mach_pwd, nt_owf_mach_pwd, lm_owf_mach_pwd);

	DEBUG(5,("generating nt owf from initial machine pwd: %s\n", mach_pwd));

#ifdef DEBUG_PASSWORD
	DEBUG(100,("client cryptkey: "));
	dump_data(100, cli_trust.cryptkey, sizeof(cli_trust.cryptkey));
#endif

	SMBencrypt(nt_owf_mach_pwd, cli_trust.cryptkey, nt_sess_pwd);

#ifdef DEBUG_PASSWORD
	DEBUG(100,("nt_owf_mach_pwd: "));
	dump_data(100, nt_owf_mach_pwd, sizeof(lm_owf_mach_pwd));
	DEBUG(100,("nt_sess_pwd: "));
	dump_data(100, nt_sess_pwd, sizeof(nt_sess_pwd));
#endif

	SMBencrypt(lm_owf_mach_pwd, cli_trust.cryptkey, lm_sess_pwd);

#ifdef DEBUG_PASSWORD
	DEBUG(100,("lm_owf_mach_pwd: "));
	dump_data(100, lm_owf_mach_pwd, sizeof(lm_owf_mach_pwd));
	DEBUG(100,("lm_sess_pwd: "));
	dump_data(100, lm_sess_pwd, sizeof(lm_sess_pwd));
#endif

	right_error_code = False;

	if (cli_session_setup(&cli_trust, mach_acct, 
			nt_owf_mach_pwd, sizeof(nt_owf_mach_pwd),
			nt_owf_mach_pwd, sizeof(nt_owf_mach_pwd), domain))
	{
		DEBUG(0,("cli_session_setup: NO ERROR! AAAGH! BUG IN SERVER DETECTED!!!\n"));
		cli_shutdown(&cli_trust);
	
		return False;
	}

	cli_error(&cli_trust, &err_cls, &err_num, NULL);

	if (err_num == (0xC0000000 | NT_STATUS_NOLOGON_WORKSTATION_TRUST_ACCOUNT))
	{
		DEBUG(1,("cli_send_tconX: valid workstation trust account exists\n"));
		right_error_code = True;
	}

	if (err_num == (0xC0000000 | NT_STATUS_NO_SUCH_USER))
	{
		DEBUG(1,("cli_send_tconX: workstation trust account does not exist\n"));
		right_error_code = False;
	}

	if (!right_error_code)
	{
		DEBUG(1,("server_validate failed (%s)\n", cli_errstr(&cli_trust)));
	}

	cli_shutdown(&cli_trust);
	return right_error_code;
}


