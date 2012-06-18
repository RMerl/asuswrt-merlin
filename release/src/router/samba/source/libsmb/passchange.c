/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   SMB client password change routine
   Copyright (C) Andrew Tridgell 1994-1998
   
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

#include "includes.h"


extern pstring global_myname;

/*************************************************************
change a password on a remote machine using IPC calls
*************************************************************/
BOOL remote_password_change(const char *remote_machine, const char *user_name, 
			    const char *old_passwd, const char *new_passwd,
			    char *err_str, size_t err_str_len)
{
	struct nmb_name calling, called;
	struct cli_state cli;
	struct in_addr ip;

	*err_str = '\0';

	if(!resolve_name( remote_machine, &ip, 0x20)) {
		slprintf(err_str, err_str_len-1, "unable to find an IP address for machine %s.\n",
			remote_machine );
		return False;
	}
 
	ZERO_STRUCT(cli);
 
	if (!cli_initialise(&cli) || !cli_connect(&cli, remote_machine, &ip)) {
		slprintf(err_str, err_str_len-1, "unable to connect to SMB server on machine %s. Error was : %s.\n",
			remote_machine, cli_errstr(&cli) );
		return False;
	}
  
	make_nmb_name(&calling, global_myname , 0x0);
	make_nmb_name(&called , remote_machine, 0x20);
	
	if (!cli_session_request(&cli, &calling, &called)) {
		slprintf(err_str, err_str_len-1, "machine %s rejected the session setup. Error was : %s.\n",
			remote_machine, cli_errstr(&cli) );
		cli_shutdown(&cli);
		return False;
	}
  
	cli.protocol = PROTOCOL_NT1;

	if (!cli_negprot(&cli)) {
		slprintf(err_str, err_str_len-1, "machine %s rejected the negotiate protocol. Error was : %s.\n",        
			remote_machine, cli_errstr(&cli) );
		cli_shutdown(&cli);
		return False;
	}
  
	/*
	 * We should connect as the anonymous user here, in case
	 * the server has "must change password" checked...
	 * Thanks to <Nicholas.S.Jenkins@cdc.com> for this fix.
	 */

	if (!cli_session_setup(&cli, "", "", 0, "", 0, "")) {
		slprintf(err_str, err_str_len-1, "machine %s rejected the session setup. Error was : %s.\n",        
			remote_machine, cli_errstr(&cli) );
		cli_shutdown(&cli);
		return False;
	}               

	if (!cli_send_tconX(&cli, "IPC$", "IPC", "", 1)) {
		slprintf(err_str, err_str_len-1, "machine %s rejected the tconX on the IPC$ share. Error was : %s.\n",
			remote_machine, cli_errstr(&cli) );
		cli_shutdown(&cli);
		return False;
	}

	if(!cli_oem_change_password(&cli, user_name, new_passwd, old_passwd)) {
		slprintf(err_str, err_str_len-1, "machine %s rejected the password change: Error was : %s.\n",
			remote_machine, cli_errstr(&cli) );
		cli_shutdown(&cli);
		return False;
	}
	
	cli_shutdown(&cli);
	return True;
}
