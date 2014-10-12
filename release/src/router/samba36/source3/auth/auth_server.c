/* 
   Unix SMB/CIFS implementation.
   Authenticate to a remote server
   Copyright (C) Andrew Tridgell 1992-1998
   Copyright (C) Andrew Bartlett 2001

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
#include "auth.h"
#include "system/passwd.h"
#include "smbd/smbd.h"
#include "libsmb/libsmb.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_AUTH

extern userdom_struct current_user_info;

/****************************************************************************
 Support for server level security.
****************************************************************************/

static struct cli_state *server_cryptkey(TALLOC_CTX *mem_ctx)
{
	struct cli_state *cli = NULL;
	char *desthost = NULL;
	struct sockaddr_storage dest_ss;
	const char *p;
	char *pserver = NULL;
	bool connected_ok = False;
	struct named_mutex *mutex = NULL;
	NTSTATUS status;

	if (!(cli = cli_initialise()))
		return NULL;

	/* security = server just can't function with spnego */
	cli->use_spnego = False;

        pserver = talloc_strdup(mem_ctx, lp_passwordserver());
	p = pserver;

        while(next_token_talloc(mem_ctx, &p, &desthost, LIST_SEP)) {

		desthost = talloc_sub_basic(mem_ctx,
				current_user_info.smb_name,
				current_user_info.domain,
				desthost);
		if (!desthost) {
			return NULL;
		}
		strupper_m(desthost);

		if(!resolve_name( desthost, &dest_ss, 0x20, false)) {
			DEBUG(1,("server_cryptkey: Can't resolve address for %s\n",desthost));
			continue;
		}

		if (ismyaddr((struct sockaddr *)&dest_ss)) {
			DEBUG(1,("Password server loop - disabling password server %s\n",desthost));
			continue;
		}

		/* we use a mutex to prevent two connections at once - when a
		   Win2k PDC get two connections where one hasn't completed a
		   session setup yet it will send a TCP reset to the first
		   connection (tridge) */

		mutex = grab_named_mutex(talloc_tos(), desthost, 10);
		if (mutex == NULL) {
			cli_shutdown(cli);
			return NULL;
		}

		status = cli_connect(cli, desthost, &dest_ss);
		if (NT_STATUS_IS_OK(status)) {
			DEBUG(3,("connected to password server %s\n",desthost));
			connected_ok = True;
			break;
		}
		DEBUG(10,("server_cryptkey: failed to connect to server %s. Error %s\n",
			desthost, nt_errstr(status) ));
		TALLOC_FREE(mutex);
	}

	if (!connected_ok) {
		DEBUG(0,("password server not available\n"));
		cli_shutdown(cli);
		return NULL;
	}

	if (!attempt_netbios_session_request(&cli, global_myname(),
					     desthost, &dest_ss)) {
		TALLOC_FREE(mutex);
		DEBUG(1,("password server fails session request\n"));
		cli_shutdown(cli);
		return NULL;
	}

	if (strequal(desthost,myhostname())) {
		exit_server_cleanly("Password server loop!");
	}

	DEBUG(3,("got session\n"));

	status = cli_negprot(cli);

	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(mutex);
		DEBUG(1, ("%s rejected the negprot: %s\n",
			  desthost, nt_errstr(status)));
		cli_shutdown(cli);
		return NULL;
	}

	if (cli->protocol < PROTOCOL_LANMAN2 ||
	    !(cli->sec_mode & NEGOTIATE_SECURITY_USER_LEVEL)) {
		TALLOC_FREE(mutex);
		DEBUG(1,("%s isn't in user level security mode\n",desthost));
		cli_shutdown(cli);
		return NULL;
	}

	/* Get the first session setup done quickly, to avoid silly
	   Win2k bugs.  (The next connection to the server will kill
	   this one...
	*/

	status = cli_session_setup(cli, "", "", 0, "", 0, "");
	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(mutex);
		DEBUG(0,("%s rejected the initial session setup (%s)\n",
			 desthost, nt_errstr(status)));
		cli_shutdown(cli);
		return NULL;
	}

	TALLOC_FREE(mutex);

	DEBUG(3,("password server OK\n"));

	return cli;
}

struct server_security_state {
	struct cli_state *cli;
};

/****************************************************************************
 Send a 'keepalive' packet down the cli pipe.
****************************************************************************/

static bool send_server_keepalive(const struct timeval *now,
				  void *private_data)
{
	struct server_security_state *state = talloc_get_type_abort(
		private_data, struct server_security_state);

	if (!state->cli || !state->cli->initialised) {
		return False;
	}

	if (send_keepalive(state->cli->fd)) {
		return True;
	}

	DEBUG( 2, ( "send_server_keepalive: password server keepalive "
		    "failed.\n"));
	cli_shutdown(state->cli);
	state->cli = NULL;
	return False;
}

static int destroy_server_security(struct server_security_state *state)
{
	if (state->cli) {
		cli_shutdown(state->cli);
	}
	return 0;
}

static struct server_security_state *make_server_security_state(struct cli_state *cli)
{
	struct server_security_state *result;

	if (!(result = talloc(NULL, struct server_security_state))) {
		DEBUG(0, ("talloc failed\n"));
		cli_shutdown(cli);
		return NULL;
	}

	result->cli = cli;
	talloc_set_destructor(result, destroy_server_security);

	if (lp_keepalive() != 0) {
		struct timeval interval;
		interval.tv_sec = lp_keepalive();
		interval.tv_usec = 0;

		if (event_add_idle(server_event_context(), result, interval,
				   "server_security_keepalive",
				   send_server_keepalive,
				   result) == NULL) {
			DEBUG(0, ("event_add_idle failed\n"));
			TALLOC_FREE(result);
			return NULL;
		}
	}

	return result;
}

/****************************************************************************
 Get the challenge out of a password server.
****************************************************************************/

static DATA_BLOB auth_get_challenge_server(const struct auth_context *auth_context,
					   void **my_private_data, 
					   TALLOC_CTX *mem_ctx)
{
	struct cli_state *cli = server_cryptkey(mem_ctx);

	if (cli) {
		DEBUG(3,("using password server validation\n"));

		if ((cli->sec_mode & NEGOTIATE_SECURITY_CHALLENGE_RESPONSE) == 0) {
			/* We can't work with unencrypted password servers
			   unless 'encrypt passwords = no' */
			DEBUG(5,("make_auth_info_server: Server is unencrypted, no challenge available..\n"));

			/* However, it is still a perfectly fine connection
			   to pass that unencrypted password over */
			*my_private_data =
				(void *)make_server_security_state(cli);
			return data_blob_null;
		} else if (cli->secblob.length < 8) {
			/* We can't do much if we don't get a full challenge */
			DEBUG(2,("make_auth_info_server: Didn't receive a full challenge from server\n"));
			cli_shutdown(cli);
			return data_blob_null;
		}

		if (!(*my_private_data = (void *)make_server_security_state(cli))) {
			return data_blob(NULL,0);
		}

		/* The return must be allocated on the caller's mem_ctx, as our own will be
		   destoyed just after the call. */
		return data_blob_talloc((TALLOC_CTX *)auth_context, cli->secblob.data,8);
	} else {
		return data_blob_null;
	}
}


/****************************************************************************
 Check for a valid username and password in security=server mode.
  - Validate a password with the password server.
****************************************************************************/

static NTSTATUS check_smbserver_security(const struct auth_context *auth_context,
					 void *my_private_data, 
					 TALLOC_CTX *mem_ctx,
					 const struct auth_usersupplied_info *user_info,
					 struct auth_serversupplied_info **server_info)
{
	struct server_security_state *state = NULL;
	struct cli_state *cli = NULL;
	static bool tested_password_server = False;
	static bool bad_password_server = False;
	NTSTATUS nt_status = NT_STATUS_NOT_IMPLEMENTED;
	bool locally_made_cli = False;

	DEBUG(10, ("check_smbserver_security: Check auth for: [%s]\n",
		user_info->mapped.account_name));

	if (my_private_data == NULL) {
		DEBUG(10,("check_smbserver_security: "
			"password server is not connected\n"));
		return NT_STATUS_LOGON_FAILURE;
	}

	state = talloc_get_type_abort(my_private_data, struct server_security_state);
	cli = state->cli;

	if (cli) {
	} else {
		cli = server_cryptkey(mem_ctx);
		locally_made_cli = True;
	}

	if (!cli || !cli->initialised) {
		DEBUG(1,("password server is not connected (cli not initialised)\n"));
		return NT_STATUS_LOGON_FAILURE;
	}  

	if ((cli->sec_mode & NEGOTIATE_SECURITY_CHALLENGE_RESPONSE) == 0) {
		if (user_info->password_state != AUTH_PASSWORD_PLAIN) {
			DEBUG(1,("password server %s is plaintext, but we are encrypted. This just can't work :-(\n", cli->desthost));
			return NT_STATUS_LOGON_FAILURE;		
		}
	} else {
		if (memcmp(cli->secblob.data, auth_context->challenge.data, 8) != 0) {
			DEBUG(1,("the challenge that the password server (%s) supplied us is not the one we gave our client. This just can't work :-(\n", cli->desthost));
			return NT_STATUS_LOGON_FAILURE;		
		}
	}

	/*
	 * Attempt a session setup with a totally incorrect password.
	 * If this succeeds with the guest bit *NOT* set then the password
	 * server is broken and is not correctly setting the guest bit. We
	 * need to detect this as some versions of NT4.x are broken. JRA.
	 */

	/* I sure as hell hope that there aren't servers out there that take 
	 * NTLMv2 and have this bug, as we don't test for that... 
	 *  - abartlet@samba.org
	 */

	if ((!tested_password_server) && (lp_paranoid_server_security())) {
		unsigned char badpass[24];
		char *baduser = NULL;

		memset(badpass, 0x1f, sizeof(badpass));

		if((user_info->password.response.nt.length == sizeof(badpass)) &&
		   !memcmp(badpass, user_info->password.response.nt.data, sizeof(badpass))) {
			/* 
			 * Very unlikely, our random bad password is the same as the users
			 * password.
			 */
			memset(badpass, badpass[0]+1, sizeof(badpass));
		}

		baduser = talloc_asprintf(mem_ctx,
					"%s%s",
					INVALID_USER_PREFIX,
					global_myname());
		if (!baduser) {
			return NT_STATUS_NO_MEMORY;
		}

		if (NT_STATUS_IS_OK(cli_session_setup(cli, baduser,
						      (char *)badpass,
						      sizeof(badpass), 
						      (char *)badpass,
						      sizeof(badpass),
						      user_info->mapped.domain_name))) {

			/*
			 * We connected to the password server so we
			 * can say we've tested it.
			 */
			tested_password_server = True;

			if ((SVAL(cli->inbuf,smb_vwv2) & 1) == 0) {
				DEBUG(0,("server_validate: password server %s allows users as non-guest \
with a bad password.\n", cli->desthost));
				DEBUG(0,("server_validate: This is broken (and insecure) behaviour. Please do not \
use this machine as the password server.\n"));
				cli_ulogoff(cli);

				/*
				 * Password server has the bug.
				 */
				bad_password_server = True;
				return NT_STATUS_LOGON_FAILURE;
			}
			cli_ulogoff(cli);
		}
	} else {

		/*
		 * We have already tested the password server.
		 * Fail immediately if it has the bug.
		 */

		if(bad_password_server) {
			DEBUG(0,("server_validate: [1] password server %s allows users as non-guest \
with a bad password.\n", cli->desthost));
			DEBUG(0,("server_validate: [1] This is broken (and insecure) behaviour. Please do not \
use this machine as the password server.\n"));
			return NT_STATUS_LOGON_FAILURE;
		}
	}

	/*
	 * Now we know the password server will correctly set the guest bit, or is
	 * not guest enabled, we can try with the real password.
	 */
	switch (user_info->password_state) {
	case AUTH_PASSWORD_PLAIN:
		/* Plaintext available */
		nt_status = cli_session_setup(
			cli, user_info->client.account_name,
			user_info->password.plaintext,
			strlen(user_info->password.plaintext),
			NULL, 0, user_info->mapped.domain_name);
		break;

	/* currently the hash values include a challenge-response as well */
	case AUTH_PASSWORD_HASH:
	case AUTH_PASSWORD_RESPONSE:
		nt_status = cli_session_setup(
			cli, user_info->client.account_name,
			(char *)user_info->password.response.lanman.data,
			user_info->password.response.lanman.length,
			(char *)user_info->password.response.nt.data,
			user_info->password.response.nt.length,
			user_info->mapped.domain_name);
		break;
	default:
		DEBUG(0,("user_info constructed for user '%s' was invalid - password_state=%u invalid.\n",user_info->mapped.account_name, user_info->password_state));
		nt_status = NT_STATUS_INTERNAL_ERROR;
	}

	if (!NT_STATUS_IS_OK(nt_status)) {
		DEBUG(1,("password server %s rejected the password: %s\n",
			 cli->desthost, nt_errstr(nt_status)));
	}

	/* if logged in as guest then reject */
	if (cli->is_guestlogin) {
		DEBUG(1,("password server %s gave us guest only\n", cli->desthost));
		nt_status = NT_STATUS_LOGON_FAILURE;
	}

	cli_ulogoff(cli);

	if (NT_STATUS_IS_OK(nt_status)) {
		char *real_username = NULL;
		struct passwd *pass = NULL;

		if ( (pass = smb_getpwnam(talloc_tos(), user_info->mapped.account_name,
			&real_username, True )) != NULL )
		{
			nt_status = make_server_info_pw(server_info, pass->pw_name, pass);
			TALLOC_FREE(pass);
			TALLOC_FREE(real_username);
		}
		else
		{
			nt_status = NT_STATUS_NO_SUCH_USER;
		}
	}

	if (locally_made_cli) {
		cli_shutdown(cli);
	}

	return(nt_status);
}

static NTSTATUS auth_init_smbserver(struct auth_context *auth_context, const char* param, auth_methods **auth_method) 
{
	struct auth_methods *result;

	result = TALLOC_ZERO_P(auth_context, struct auth_methods);
	if (result == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	result->name = "smbserver";
	result->auth = check_smbserver_security;
	result->get_chal = auth_get_challenge_server;

        *auth_method = result;
	return NT_STATUS_OK;
}

NTSTATUS auth_server_init(void)
{
	return smb_register_auth(AUTH_INTERFACE_VERSION, "smbserver", auth_init_smbserver);
}
