/*
 *  Unix SMB/CIFS implementation.
 *  RPC Pipe client / server routines
 *  Largely rewritten by Jeremy Allison		    2005.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"
#include "../librpc/gen_ndr/ndr_schannel.h"
#include "../librpc/gen_ndr/ndr_netlogon.h"
#include "../libcli/auth/schannel.h"
#include "rpc_client/cli_netlogon.h"
#include "rpc_client/cli_pipe.h"
#include "librpc/gen_ndr/ndr_dcerpc.h"
#include "librpc/rpc/dcerpc.h"
#include "passdb.h"
#include "client.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_RPC_CLI


/****************************************************************************
  Get a the schannel session key out of an already opened netlogon pipe.
 ****************************************************************************/
static NTSTATUS get_schannel_session_key_common(struct rpc_pipe_client *netlogon_pipe,
						struct cli_state *cli,
						const char *domain,
						uint32 *pneg_flags)
{
	enum netr_SchannelType sec_chan_type = 0;
	unsigned char machine_pwd[16];
	const char *machine_account;
	NTSTATUS status;

	/* Get the machine account credentials from secrets.tdb. */
	if (!get_trust_pw_hash(domain, machine_pwd, &machine_account,
			       &sec_chan_type))
	{
		DEBUG(0, ("get_schannel_session_key: could not fetch "
			"trust account password for domain '%s'\n",
			domain));
		return NT_STATUS_CANT_ACCESS_DOMAIN_INFO;
	}

	status = rpccli_netlogon_setup_creds(netlogon_pipe,
					cli->desthost, /* server name */
					domain,	       /* domain */
					global_myname(), /* client name */
					machine_account, /* machine account name */
					machine_pwd,
					sec_chan_type,
					pneg_flags);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(3, ("get_schannel_session_key_common: "
			  "rpccli_netlogon_setup_creds failed with result %s "
			  "to server %s, domain %s, machine account %s.\n",
			  nt_errstr(status), cli->desthost, domain,
			  machine_account ));
		return status;
	}

	if (((*pneg_flags) & NETLOGON_NEG_SCHANNEL) == 0) {
		DEBUG(3, ("get_schannel_session_key: Server %s did not offer schannel\n",
			cli->desthost));
		return NT_STATUS_INVALID_NETWORK_RESPONSE;
	}

	return NT_STATUS_OK;
}

/****************************************************************************
 Open a named pipe to an SMB server and bind using schannel (bind type 68).
 Fetch the session key ourselves using a temporary netlogon pipe. This
 version uses an ntlmssp auth bound netlogon pipe to get the key.
 ****************************************************************************/

static NTSTATUS get_schannel_session_key_auth_ntlmssp(struct cli_state *cli,
						      const char *domain,
						      const char *username,
						      const char *password,
						      uint32 *pneg_flags,
						      struct rpc_pipe_client **presult)
{
	struct rpc_pipe_client *netlogon_pipe = NULL;
	NTSTATUS status;

	status = cli_rpc_pipe_open_spnego_ntlmssp(
		cli, &ndr_table_netlogon.syntax_id, NCACN_NP,
		DCERPC_AUTH_LEVEL_PRIVACY,
		domain, username, password, &netlogon_pipe);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = get_schannel_session_key_common(netlogon_pipe, cli, domain,
						 pneg_flags);
	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(netlogon_pipe);
		return status;
	}

	*presult = netlogon_pipe;
	return NT_STATUS_OK;
}

/****************************************************************************
 Open a named pipe to an SMB server and bind using schannel (bind type 68).
 Fetch the session key ourselves using a temporary netlogon pipe. This version
 uses an ntlmssp bind to get the session key.
 ****************************************************************************/

NTSTATUS cli_rpc_pipe_open_ntlmssp_auth_schannel(struct cli_state *cli,
						 const struct ndr_syntax_id *interface,
						 enum dcerpc_transport_t transport,
						 enum dcerpc_AuthLevel auth_level,
						 const char *domain,
						 const char *username,
						 const char *password,
						 struct rpc_pipe_client **presult)
{
	uint32_t neg_flags = NETLOGON_NEG_AUTH2_ADS_FLAGS;
	struct rpc_pipe_client *netlogon_pipe = NULL;
	struct rpc_pipe_client *result = NULL;
	NTSTATUS status;

	status = get_schannel_session_key_auth_ntlmssp(
		cli, domain, username, password, &neg_flags, &netlogon_pipe);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("cli_rpc_pipe_open_ntlmssp_auth_schannel: failed to get schannel session "
			"key from server %s for domain %s.\n",
			cli->desthost, domain ));
		return status;
	}

	status = cli_rpc_pipe_open_schannel_with_key(
		cli, interface, transport, auth_level, domain, &netlogon_pipe->dc,
		&result);

	/* Now we've bound using the session key we can close the netlog pipe. */
	TALLOC_FREE(netlogon_pipe);

	if (NT_STATUS_IS_OK(status)) {
		*presult = result;
	}
	return status;
}

/****************************************************************************
 Open a named pipe to an SMB server and bind using schannel (bind type 68).
 Fetch the session key ourselves using a temporary netlogon pipe.
 ****************************************************************************/

NTSTATUS cli_rpc_pipe_open_schannel(struct cli_state *cli,
				    const struct ndr_syntax_id *interface,
				    enum dcerpc_transport_t transport,
				    enum dcerpc_AuthLevel auth_level,
				    const char *domain,
				    struct rpc_pipe_client **presult)
{
	uint32_t neg_flags = NETLOGON_NEG_AUTH2_ADS_FLAGS;
	struct rpc_pipe_client *netlogon_pipe = NULL;
	struct rpc_pipe_client *result = NULL;
	NTSTATUS status;

	status = get_schannel_session_key(cli, domain, &neg_flags,
					  &netlogon_pipe);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("cli_rpc_pipe_open_schannel: failed to get schannel session "
			"key from server %s for domain %s.\n",
			cli->desthost, domain ));
		return status;
	}

	status = cli_rpc_pipe_open_schannel_with_key(
		cli, interface, transport, auth_level, domain, &netlogon_pipe->dc,
		&result);

	/* Now we've bound using the session key we can close the netlog pipe. */
	TALLOC_FREE(netlogon_pipe);

	if (NT_STATUS_IS_OK(status)) {
		*presult = result;
	}

	return status;
}

/****************************************************************************
 Open a netlogon pipe and get the schannel session key.
 Now exposed to external callers.
 ****************************************************************************/


NTSTATUS get_schannel_session_key(struct cli_state *cli,
				  const char *domain,
				  uint32 *pneg_flags,
				  struct rpc_pipe_client **presult)
{
	struct rpc_pipe_client *netlogon_pipe = NULL;
	NTSTATUS status;

	status = cli_rpc_pipe_open_noauth(cli, &ndr_table_netlogon.syntax_id,
					  &netlogon_pipe);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = get_schannel_session_key_common(netlogon_pipe, cli, domain,
						 pneg_flags);
	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(netlogon_pipe);
		return status;
	}

	*presult = netlogon_pipe;
	return NT_STATUS_OK;
}
