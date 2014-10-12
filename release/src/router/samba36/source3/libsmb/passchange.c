/* 
   Unix SMB/CIFS implementation.
   SMB client password change routine
   Copyright (C) Andrew Tridgell 1994-1998
   
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
#include "../librpc/gen_ndr/ndr_samr.h"
#include "rpc_client/cli_pipe.h"
#include "rpc_client/cli_samr.h"
#include "libsmb/libsmb.h"
#include "libsmb/clirap.h"
#include "libsmb/nmblib.h"

/*************************************************************
 Change a password on a remote machine using IPC calls.
*************************************************************/

NTSTATUS remote_password_change(const char *remote_machine, const char *user_name, 
				const char *old_passwd, const char *new_passwd,
				char **err_str)
{
	struct nmb_name calling, called;
	struct cli_state *cli = NULL;
	struct rpc_pipe_client *pipe_hnd = NULL;
	struct sockaddr_storage ss;
	char *user, *domain, *p;

	NTSTATUS result;
	bool pass_must_change = False;

	user = talloc_strdup(talloc_tos(), user_name);
	SMB_ASSERT(user != NULL);
	domain = talloc_strdup(talloc_tos(), "");
	SMB_ASSERT(domain != NULL);

	/* allow usernames of the form domain\\user or domain/user */
	if ((p = strchr_m(user,'\\')) || (p = strchr_m(user,'/')) ||
	    (p = strchr_m(user,*lp_winbind_separator()))) {
		*p = 0;
		domain = user;
		user = p+1;
	}

	*err_str = NULL;

	if(!resolve_name( remote_machine, &ss, 0x20, false)) {
		if (asprintf(err_str, "Unable to find an IP address for machine "
			 "%s.\n", remote_machine) == -1) {
			*err_str = NULL;
		}
		return NT_STATUS_UNSUCCESSFUL;
	}

	cli = cli_initialise();
	if (!cli) {
		return NT_STATUS_NO_MEMORY;
	}

	result = cli_connect(cli, remote_machine, &ss);
	if (!NT_STATUS_IS_OK(result)) {
		if (asprintf(err_str, "Unable to connect to SMB server on "
			 "machine %s. Error was : %s.\n",
			 remote_machine, nt_errstr(result))==-1) {
			*err_str = NULL;
		}
		cli_shutdown(cli);
		return result;
	}

	make_nmb_name(&calling, global_myname() , 0x0);
	make_nmb_name(&called , remote_machine, 0x20);

	if (!cli_session_request(cli, &calling, &called)) {
		if (asprintf(err_str, "machine %s rejected the session setup. "
			 "Error was : %s.\n",
			 remote_machine, cli_errstr(cli)) == -1) {
			*err_str = NULL;
		}
		result = cli_nt_error(cli);
		cli_shutdown(cli);
		return result;
	}

	cli->protocol = PROTOCOL_NT1;

	result = cli_negprot(cli);

	if (!NT_STATUS_IS_OK(result)) {
		if (asprintf(err_str, "machine %s rejected the negotiate "
			 "protocol. Error was : %s.\n",        
			 remote_machine, nt_errstr(result)) == -1) {
			*err_str = NULL;
		}
		result = cli_nt_error(cli);
		cli_shutdown(cli);
		return result;
	}

	/* Given things like SMB signing, restrict anonymous and the like, 
	   try an authenticated connection first */
	result = cli_session_setup(cli, user_name,
				   old_passwd, strlen(old_passwd)+1,
				   old_passwd, strlen(old_passwd)+1, "");

	if (!NT_STATUS_IS_OK(result)) {

		/* Password must change or Password expired are the only valid
		 * error conditions here from where we can proceed, the rest like
		 * account locked out or logon failure will lead to errors later
		 * anyway */

		if (!NT_STATUS_EQUAL(result, NT_STATUS_PASSWORD_MUST_CHANGE) &&
		    !NT_STATUS_EQUAL(result, NT_STATUS_PASSWORD_EXPIRED)) {
			if (asprintf(err_str, "Could not connect to machine %s: "
				 "%s\n", remote_machine, cli_errstr(cli)) == -1) {
				*err_str = NULL;
			}
			cli_shutdown(cli);
			return result;
		}

		pass_must_change = True;

		/*
		 * We should connect as the anonymous user here, in case
		 * the server has "must change password" checked...
		 * Thanks to <Nicholas.S.Jenkins@cdc.com> for this fix.
		 */

		result = cli_session_setup(cli, "", "", 0, "", 0, "");

		if (!NT_STATUS_IS_OK(result)) {
			if (asprintf(err_str, "machine %s rejected the session "
				 "setup. Error was : %s.\n",        
				 remote_machine, cli_errstr(cli)) == -1) {
				*err_str = NULL;
			}
			cli_shutdown(cli);
			return result;
		}

		result = cli_init_creds(cli, "", "", NULL);
		if (!NT_STATUS_IS_OK(result)) {
			cli_shutdown(cli);
			return result;
		}
	} else {
		result = cli_init_creds(cli, user, domain, old_passwd);
		if (!NT_STATUS_IS_OK(result)) {
			cli_shutdown(cli);
			return result;
		}
	}

	result = cli_tcon_andx(cli, "IPC$", "IPC", "", 1);
	if (!NT_STATUS_IS_OK(result)) {
		if (asprintf(err_str, "machine %s rejected the tconX on the "
			     "IPC$ share. Error was : %s.\n",
			     remote_machine, nt_errstr(result))) {
			*err_str = NULL;
		}
		cli_shutdown(cli);
		return result;
	}

	/* Try not to give the password away too easily */

	if (!pass_must_change) {
		result = cli_rpc_pipe_open_ntlmssp(cli,
						   &ndr_table_samr.syntax_id,
						   NCACN_NP,
						   DCERPC_AUTH_LEVEL_PRIVACY,
						   domain, user,
						   old_passwd,
						   &pipe_hnd);
	} else {
		/*
		 * If the user password must be changed the ntlmssp bind will
		 * fail the same way as the session setup above did. The
		 * difference ist that with a pipe bind we don't get a good
		 * error message, the result will be that the rpc call below
		 * will just fail. So we do it anonymously, there's no other
		 * way.
		 */
		result = cli_rpc_pipe_open_noauth(
			cli, &ndr_table_samr.syntax_id, &pipe_hnd);
	}

	if (!NT_STATUS_IS_OK(result)) {
		if (lp_client_lanman_auth()) {
			/* Use the old RAP method. */
			if (!cli_oem_change_password(cli, user_name, new_passwd, old_passwd)) {
				if (asprintf(err_str, "machine %s rejected the "
					 "password change: Error was : %s.\n",
					 remote_machine, cli_errstr(cli)) == -1) {
					*err_str = NULL;
				}
				result = cli_nt_error(cli);
				cli_shutdown(cli);
				return result;
			}
		} else {
			if (asprintf(err_str, "SAMR connection to machine %s "
				 "failed. Error was %s, but LANMAN password "
				 "changes are disabled\n",
				 remote_machine, nt_errstr(result)) == -1) {
				*err_str = NULL;
			}
			result = cli_nt_error(cli);
			cli_shutdown(cli);
			return result;
		}
	}

	result = rpccli_samr_chgpasswd_user2(pipe_hnd, talloc_tos(),
					     user_name, new_passwd, old_passwd);
	if (NT_STATUS_IS_OK(result)) {
		/* Great - it all worked! */
		cli_shutdown(cli);
		return NT_STATUS_OK;

	} else if (!(NT_STATUS_EQUAL(result, NT_STATUS_ACCESS_DENIED) 
		     || NT_STATUS_EQUAL(result, NT_STATUS_UNSUCCESSFUL))) {
		/* it failed, but for reasons such as wrong password, too short etc ... */

		if (asprintf(err_str, "machine %s rejected the password change: "
			 "Error was : %s.\n",
			 remote_machine, get_friendly_nt_error_msg(result)) == -1) {
			*err_str = NULL;
		}
		cli_shutdown(cli);
		return result;
	}

	/* OK, that failed, so try again... */
	TALLOC_FREE(pipe_hnd);

	/* Try anonymous NTLMSSP... */
	result = cli_init_creds(cli, "", "", NULL);
	if (!NT_STATUS_IS_OK(result)) {
		cli_shutdown(cli);
		return result;
	}

	result = NT_STATUS_UNSUCCESSFUL;

	/* OK, this is ugly, but... try an anonymous pipe. */
	result = cli_rpc_pipe_open_noauth(cli, &ndr_table_samr.syntax_id,
					  &pipe_hnd);

	if ( NT_STATUS_IS_OK(result) &&
		(NT_STATUS_IS_OK(result = rpccli_samr_chgpasswd_user2(
					 pipe_hnd, talloc_tos(), user_name,
					 new_passwd, old_passwd)))) {
		/* Great - it all worked! */
		cli_shutdown(cli);
		return NT_STATUS_OK;
	} else {
		if (!(NT_STATUS_EQUAL(result, NT_STATUS_ACCESS_DENIED) 
		      || NT_STATUS_EQUAL(result, NT_STATUS_UNSUCCESSFUL))) {
			/* it failed, but again it was due to things like new password too short */

			if (asprintf(err_str, "machine %s rejected the "
				 "(anonymous) password change: Error was : "
				 "%s.\n", remote_machine,
				 get_friendly_nt_error_msg(result)) == -1) {
				*err_str = NULL;
			}
			cli_shutdown(cli);
			return result;
		}

		/* We have failed to change the user's password, and we think the server
		   just might not support SAMR password changes, so fall back */

		if (lp_client_lanman_auth()) {
			/* Use the old RAP method. */
			if (cli_oem_change_password(cli, user_name, new_passwd, old_passwd)) {
				/* SAMR failed, but the old LanMan protocol worked! */

				cli_shutdown(cli);
				return NT_STATUS_OK;
			}
			if (asprintf(err_str, "machine %s rejected the password "
				 "change: Error was : %s.\n",
				 remote_machine, cli_errstr(cli)) == -1) {
				*err_str = NULL;
			}
			result = cli_nt_error(cli);
			cli_shutdown(cli);
			return result;
		} else {
			if (asprintf(err_str, "SAMR connection to machine %s "
				 "failed. Error was %s, but LANMAN password "
				 "changes are disabled\n",
				nt_errstr(result), remote_machine) == -1) {
				*err_str = NULL;
			}
			cli_shutdown(cli);
			return NT_STATUS_UNSUCCESSFUL;
		}
	}
}
