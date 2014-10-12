/*
   Samba Unix/Linux SMB client library
   Distributed SMB/CIFS Server Management Utility
   Copyright (C) 2001 Andrew Bartlett (abartlet@samba.org)
   Copyright (C) 2002 Jim McDonough (jmcd@us.ibm.com)
   Copyright (C) 2004,2008 Guenther Deschner (gd@samba.org)
   Copyright (C) 2005 Jeremy Allison (jra@samba.org)
   Copyright (C) 2006 Jelmer Vernooij (jelmer@samba.org)

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include "includes.h"
#include "utils/net.h"
#include "rpc_client/cli_pipe.h"
#include "../libcli/auth/libcli_auth.h"
#include "../librpc/gen_ndr/ndr_samr_c.h"
#include "rpc_client/cli_samr.h"
#include "rpc_client/init_samr.h"
#include "../librpc/gen_ndr/ndr_lsa_c.h"
#include "rpc_client/cli_lsarpc.h"
#include "../librpc/gen_ndr/ndr_netlogon_c.h"
#include "../librpc/gen_ndr/ndr_srvsvc_c.h"
#include "../librpc/gen_ndr/ndr_spoolss.h"
#include "../librpc/gen_ndr/ndr_initshutdown_c.h"
#include "../librpc/gen_ndr/ndr_winreg_c.h"
#include "secrets.h"
#include "lib/netapi/netapi.h"
#include "lib/netapi/netapi_net.h"
#include "rpc_client/init_lsa.h"
#include "../libcli/security/security.h"
#include "libsmb/libsmb.h"
#include "libsmb/clirap.h"
#include "nsswitch/libwbclient/wbclient.h"
#include "passdb.h"

static int net_mode_share;
static NTSTATUS sync_files(struct copy_clistate *cp_clistate, const char *mask);

/**
 * @file net_rpc.c
 *
 * @brief RPC based subcommands for the 'net' utility.
 *
 * This file should contain much of the functionality that used to
 * be found in rpcclient, execpt that the commands should change
 * less often, and the fucntionality should be sane (the user is not
 * expected to know a rid/sid before they conduct an operation etc.)
 *
 * @todo Perhaps eventually these should be split out into a number
 * of files, as this could get quite big.
 **/


/**
 * Many of the RPC functions need the domain sid.  This function gets
 *  it at the start of every run
 *
 * @param cli A cli_state already connected to the remote machine
 *
 * @return The Domain SID of the remote machine.
 **/

NTSTATUS net_get_remote_domain_sid(struct cli_state *cli, TALLOC_CTX *mem_ctx,
				   struct dom_sid **domain_sid,
				   const char **domain_name)
{
	struct rpc_pipe_client *lsa_pipe = NULL;
	struct policy_handle pol;
	NTSTATUS status, result;
	union lsa_PolicyInformation *info = NULL;
	struct dcerpc_binding_handle *b;

	status = cli_rpc_pipe_open_noauth(cli, &ndr_table_lsarpc.syntax_id,
					  &lsa_pipe);
	if (!NT_STATUS_IS_OK(status)) {
		d_fprintf(stderr, _("Could not initialise lsa pipe\n"));
		return status;
	}

	b = lsa_pipe->binding_handle;

	status = rpccli_lsa_open_policy(lsa_pipe, mem_ctx, false,
				     SEC_FLAG_MAXIMUM_ALLOWED,
				     &pol);
	if (!NT_STATUS_IS_OK(status)) {
		d_fprintf(stderr, "open_policy %s: %s\n",
			  _("failed"),
			  nt_errstr(status));
		return status;
	}

	status = dcerpc_lsa_QueryInfoPolicy(b, mem_ctx,
					    &pol,
					    LSA_POLICY_INFO_ACCOUNT_DOMAIN,
					    &info,
					    &result);
	if (!NT_STATUS_IS_OK(status)) {
		d_fprintf(stderr, "lsaquery %s: %s\n",
			  _("failed"),
			  nt_errstr(status));
		return status;
	}
	if (!NT_STATUS_IS_OK(result)) {
		d_fprintf(stderr, "lsaquery %s: %s\n",
			  _("failed"),
			  nt_errstr(result));
		return result;
	}

	*domain_name = info->account_domain.name.string;
	*domain_sid = info->account_domain.sid;

	dcerpc_lsa_Close(b, mem_ctx, &pol, &result);
	TALLOC_FREE(lsa_pipe);

	return NT_STATUS_OK;
}

/**
 * Run a single RPC command, from start to finish.
 *
 * @param pipe_name the pipe to connect to (usually a PIPE_ constant)
 * @param conn_flag a NET_FLAG_ combination.  Passed to
 *                   net_make_ipc_connection.
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 * @return A shell status integer (0 for success).
 */

int run_rpc_command(struct net_context *c,
			struct cli_state *cli_arg,
			const struct ndr_syntax_id *interface,
			int conn_flags,
			rpc_command_fn fn,
			int argc,
			const char **argv)
{
	struct cli_state *cli = NULL;
	struct rpc_pipe_client *pipe_hnd = NULL;
	TALLOC_CTX *mem_ctx;
	NTSTATUS nt_status;
	struct dom_sid *domain_sid;
	const char *domain_name;
	int ret = -1;

	/* make use of cli_state handed over as an argument, if possible */
	if (!cli_arg) {
		nt_status = net_make_ipc_connection(c, conn_flags, &cli);
		if (!NT_STATUS_IS_OK(nt_status)) {
			DEBUG(1, ("failed to make ipc connection: %s\n",
				  nt_errstr(nt_status)));
			return -1;
		}
	} else {
		cli = cli_arg;
	}

	if (!cli) {
		return -1;
	}

	/* Create mem_ctx */

	if (!(mem_ctx = talloc_init("run_rpc_command"))) {
		DEBUG(0, ("talloc_init() failed\n"));
		goto fail;
	}

	nt_status = net_get_remote_domain_sid(cli, mem_ctx, &domain_sid,
					      &domain_name);
	if (!NT_STATUS_IS_OK(nt_status)) {
		goto fail;
	}

	if (!(conn_flags & NET_FLAGS_NO_PIPE)) {
		if (lp_client_schannel()
		    && (ndr_syntax_id_equal(interface,
					    &ndr_table_netlogon.syntax_id))) {
			/* Always try and create an schannel netlogon pipe. */
			nt_status = cli_rpc_pipe_open_schannel(
				cli, interface, NCACN_NP,
				DCERPC_AUTH_LEVEL_PRIVACY, domain_name,
				&pipe_hnd);
			if (!NT_STATUS_IS_OK(nt_status)) {
				DEBUG(0, ("Could not initialise schannel netlogon pipe. Error was %s\n",
					nt_errstr(nt_status) ));
				goto fail;
			}
		} else {
			if (conn_flags & NET_FLAGS_SEAL) {
				nt_status = cli_rpc_pipe_open_ntlmssp(
					cli, interface,
					(conn_flags & NET_FLAGS_TCP) ?
					NCACN_IP_TCP : NCACN_NP,
					DCERPC_AUTH_LEVEL_PRIVACY,
					lp_workgroup(), c->opt_user_name,
					c->opt_password, &pipe_hnd);
			} else {
				nt_status = cli_rpc_pipe_open_noauth(
					cli, interface,
					&pipe_hnd);
			}
			if (!NT_STATUS_IS_OK(nt_status)) {
				DEBUG(0, ("Could not initialise pipe %s. Error was %s\n",
					  get_pipe_name_from_syntax(
						  talloc_tos(), interface),
					nt_errstr(nt_status) ));
				goto fail;
			}
		}
	}

	nt_status = fn(c, domain_sid, domain_name, cli, pipe_hnd, mem_ctx, argc, argv);

	if (!NT_STATUS_IS_OK(nt_status)) {
		DEBUG(1, ("rpc command function failed! (%s)\n", nt_errstr(nt_status)));
	} else {
		ret = 0;
		DEBUG(5, ("rpc command function succedded\n"));
	}

	if (!(conn_flags & NET_FLAGS_NO_PIPE)) {
		if (pipe_hnd) {
			TALLOC_FREE(pipe_hnd);
		}
	}

fail:
	/* close the connection only if it was opened here */
	if (!cli_arg) {
		cli_shutdown(cli);
	}

	talloc_destroy(mem_ctx);
	return ret;
}

/**
 * Force a change of the trust acccount password.
 *
 * All parameters are provided by the run_rpc_command function, except for
 * argc, argv which are passed through.
 *
 * @param domain_sid The domain sid acquired from the remote server.
 * @param cli A cli_state connected to the server.
 * @param mem_ctx Talloc context, destroyed on completion of the function.
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 *
 * @return Normal NTSTATUS return.
 **/

static NTSTATUS rpc_changetrustpw_internals(struct net_context *c,
					const struct dom_sid *domain_sid,
					const char *domain_name,
					struct cli_state *cli,
					struct rpc_pipe_client *pipe_hnd,
					TALLOC_CTX *mem_ctx,
					int argc,
					const char **argv)
{
	NTSTATUS status;

	status = trust_pw_find_change_and_store_it(pipe_hnd, mem_ctx, c->opt_target_workgroup);
	if (!NT_STATUS_IS_OK(status)) {
		d_fprintf(stderr, _("Failed to change machine account password: %s\n"),
			nt_errstr(status));
		return status;
	}

	return NT_STATUS_OK;
}

/**
 * Force a change of the trust acccount password.
 *
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 *
 * @return A shell status integer (0 for success).
 **/

int net_rpc_changetrustpw(struct net_context *c, int argc, const char **argv)
{
	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net rpc changetrustpw\n"
			   "    %s\n",
			 _("Usage:"),
			 _("Change the machine trust password"));
		return 0;
	}

	return run_rpc_command(c, NULL, &ndr_table_netlogon.syntax_id,
			       NET_FLAGS_ANONYMOUS | NET_FLAGS_PDC,
			       rpc_changetrustpw_internals,
			       argc, argv);
}

/**
 * Join a domain, the old way.
 *
 * This uses 'machinename' as the inital password, and changes it.
 *
 * The password should be created with 'server manager' or equiv first.
 *
 * All parameters are provided by the run_rpc_command function, except for
 * argc, argv which are passed through.
 *
 * @param domain_sid The domain sid acquired from the remote server.
 * @param cli A cli_state connected to the server.
 * @param mem_ctx Talloc context, destroyed on completion of the function.
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 *
 * @return Normal NTSTATUS return.
 **/

static NTSTATUS rpc_oldjoin_internals(struct net_context *c,
					const struct dom_sid *domain_sid,
					const char *domain_name,
					struct cli_state *cli,
					struct rpc_pipe_client *pipe_hnd,
					TALLOC_CTX *mem_ctx,
					int argc,
					const char **argv)
{

	fstring trust_passwd;
	unsigned char orig_trust_passwd_hash[16];
	NTSTATUS result;
	enum netr_SchannelType sec_channel_type;

	result = cli_rpc_pipe_open_noauth(cli, &ndr_table_netlogon.syntax_id,
					  &pipe_hnd);
	if (!NT_STATUS_IS_OK(result)) {
		DEBUG(0,("rpc_oldjoin_internals: netlogon pipe open to machine %s failed. "
			"error was %s\n",
			cli->desthost,
			nt_errstr(result) ));
		return result;
	}

	/*
	   check what type of join - if the user want's to join as
	   a BDC, the server must agree that we are a BDC.
	*/
	if (argc >= 0) {
		sec_channel_type = get_sec_channel_type(argv[0]);
	} else {
		sec_channel_type = get_sec_channel_type(NULL);
	}

	fstrcpy(trust_passwd, global_myname());
	strlower_m(trust_passwd);

	/*
	 * Machine names can be 15 characters, but the max length on
	 * a password is 14.  --jerry
	 */

	trust_passwd[14] = '\0';

	E_md4hash(trust_passwd, orig_trust_passwd_hash);

	result = trust_pw_change_and_store_it(pipe_hnd, mem_ctx, c->opt_target_workgroup,
					      global_myname(),
					      orig_trust_passwd_hash,
					      sec_channel_type);

	if (NT_STATUS_IS_OK(result))
		printf(_("Joined domain %s.\n"), c->opt_target_workgroup);


	if (!secrets_store_domain_sid(c->opt_target_workgroup, domain_sid)) {
		DEBUG(0, ("error storing domain sid for %s\n", c->opt_target_workgroup));
		result = NT_STATUS_UNSUCCESSFUL;
	}

	return result;
}

/**
 * Join a domain, the old way.
 *
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 *
 * @return A shell status integer (0 for success).
 **/

static int net_rpc_perform_oldjoin(struct net_context *c, int argc, const char **argv)
{
	return run_rpc_command(c, NULL, &ndr_table_netlogon.syntax_id,
			       NET_FLAGS_NO_PIPE | NET_FLAGS_ANONYMOUS | NET_FLAGS_PDC,
			       rpc_oldjoin_internals,
			       argc, argv);
}

/**
 * Join a domain, the old way.  This function exists to allow
 * the message to be displayed when oldjoin was explicitly
 * requested, but not when it was implied by "net rpc join".
 *
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 *
 * @return A shell status integer (0 for success).
 **/

static int net_rpc_oldjoin(struct net_context *c, int argc, const char **argv)
{
	int rc = -1;

	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net rpc oldjoin\n"
			   "    %s\n",
			 _("Usage:"),
			 _("Join a domain the old way"));
		return 0;
	}

	rc = net_rpc_perform_oldjoin(c, argc, argv);

	if (rc) {
		d_fprintf(stderr, _("Failed to join domain\n"));
	}

	return rc;
}

/**
 * 'net rpc join' entrypoint.
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped
 *
 * Main 'net_rpc_join()' (where the admin username/password is used) is
 * in net_rpc_join.c.
 * Try to just change the password, but if that doesn't work, use/prompt
 * for a username/password.
 **/

int net_rpc_join(struct net_context *c, int argc, const char **argv)
{
	if (c->display_usage) {
		d_printf("%s\n%s",
			 _("Usage:"),
			 _("net rpc join -U <username>[%%password] <type>\n"
			   "  Join a domain\n"
			   "    username\tName of the admin user"
			   "    password\tPassword of the admin user, will "
			   "prompt if not specified\n"
			   "    type\tCan be one of the following:\n"
			   "\t\tMEMBER\tJoin as member server (default)\n"
			   "\t\tBDC\tJoin as BDC\n"
			   "\t\tPDC\tJoin as PDC\n"));
		return 0;
	}

	if (lp_server_role() == ROLE_STANDALONE) {
		d_printf(_("cannot join as standalone machine\n"));
		return -1;
	}

	if (strlen(global_myname()) > 15) {
		d_printf(_("Our netbios name can be at most 15 chars long, "
			   "\"%s\" is %u chars long\n"),
			 global_myname(), (unsigned int)strlen(global_myname()));
		return -1;
	}

	if ((net_rpc_perform_oldjoin(c, argc, argv) == 0))
		return 0;

	return net_rpc_join_newstyle(c, argc, argv);
}

/**
 * display info about a rpc domain
 *
 * All parameters are provided by the run_rpc_command function, except for
 * argc, argv which are passed through.
 *
 * @param domain_sid The domain sid acquired from the remote server
 * @param cli A cli_state connected to the server.
 * @param mem_ctx Talloc context, destroyed on completion of the function.
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 *
 * @return Normal NTSTATUS return.
 **/

NTSTATUS rpc_info_internals(struct net_context *c,
			const struct dom_sid *domain_sid,
			const char *domain_name,
			struct cli_state *cli,
			struct rpc_pipe_client *pipe_hnd,
			TALLOC_CTX *mem_ctx,
			int argc,
			const char **argv)
{
	struct policy_handle connect_pol, domain_pol;
	NTSTATUS status, result;
	union samr_DomainInfo *info = NULL;
	fstring sid_str;
	struct dcerpc_binding_handle *b = pipe_hnd->binding_handle;

	sid_to_fstring(sid_str, domain_sid);

	/* Get sam policy handle */
	status = dcerpc_samr_Connect2(b, mem_ctx,
				      pipe_hnd->desthost,
				      MAXIMUM_ALLOWED_ACCESS,
				      &connect_pol,
				      &result);
	if (!NT_STATUS_IS_OK(status)) {
		d_fprintf(stderr, _("Could not connect to SAM: %s\n"),
			  nt_errstr(status));
		goto done;
	}

	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		d_fprintf(stderr, _("Could not connect to SAM: %s\n"),
			  nt_errstr(result));
		goto done;
	}

	/* Get domain policy handle */
	status = dcerpc_samr_OpenDomain(b, mem_ctx,
					&connect_pol,
					MAXIMUM_ALLOWED_ACCESS,
					CONST_DISCARD(struct dom_sid2 *, domain_sid),
					&domain_pol,
					&result);
	if (!NT_STATUS_IS_OK(status)) {
		d_fprintf(stderr, _("Could not open domain: %s\n"),
			  nt_errstr(status));
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		d_fprintf(stderr, _("Could not open domain: %s\n"),
			  nt_errstr(result));
		goto done;
	}

	status = dcerpc_samr_QueryDomainInfo(b, mem_ctx,
					     &domain_pol,
					     2,
					     &info,
					     &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	status = result;
	if (NT_STATUS_IS_OK(result)) {
		d_printf(_("Domain Name: %s\n"),
			 info->general.domain_name.string);
		d_printf(_("Domain SID: %s\n"), sid_str);
		d_printf(_("Sequence number: %llu\n"),
			(unsigned long long)info->general.sequence_num);
		d_printf(_("Num users: %u\n"), info->general.num_users);
		d_printf(_("Num domain groups: %u\n"),info->general.num_groups);
		d_printf(_("Num local groups: %u\n"),info->general.num_aliases);
	}

 done:
	return status;
}

/**
 * 'net rpc info' entrypoint.
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 **/

int net_rpc_info(struct net_context *c, int argc, const char **argv)
{
	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net rpc info\n"
			   "  %s\n",
			 _("Usage:"),
			 _("Display information about the domain"));
		return 0;
	}

	return run_rpc_command(c, NULL, &ndr_table_samr.syntax_id,
			       NET_FLAGS_PDC, rpc_info_internals,
			       argc, argv);
}

/**
 * Fetch domain SID into the local secrets.tdb.
 *
 * All parameters are provided by the run_rpc_command function, except for
 * argc, argv which are passed through.
 *
 * @param domain_sid The domain sid acquired from the remote server.
 * @param cli A cli_state connected to the server.
 * @param mem_ctx Talloc context, destroyed on completion of the function.
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 *
 * @return Normal NTSTATUS return.
 **/

static NTSTATUS rpc_getsid_internals(struct net_context *c,
			const struct dom_sid *domain_sid,
			const char *domain_name,
			struct cli_state *cli,
			struct rpc_pipe_client *pipe_hnd,
			TALLOC_CTX *mem_ctx,
			int argc,
			const char **argv)
{
	fstring sid_str;

	sid_to_fstring(sid_str, domain_sid);
	d_printf(_("Storing SID %s for Domain %s in secrets.tdb\n"),
		 sid_str, domain_name);

	if (!secrets_store_domain_sid(domain_name, domain_sid)) {
		DEBUG(0,("Can't store domain SID\n"));
		return NT_STATUS_UNSUCCESSFUL;
	}

	return NT_STATUS_OK;
}

/**
 * 'net rpc getsid' entrypoint.
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 **/

int net_rpc_getsid(struct net_context *c, int argc, const char **argv)
{
	int conn_flags = NET_FLAGS_PDC;

	if (!c->opt_user_specified) {
		conn_flags |= NET_FLAGS_ANONYMOUS;
	}

	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net rpc getsid\n"
			   "    %s\n",
			 _("Usage:"),
			 _("Fetch domain SID into local secrets.tdb"));
		return 0;
	}

	return run_rpc_command(c, NULL, &ndr_table_samr.syntax_id,
			       conn_flags,
			       rpc_getsid_internals,
			       argc, argv);
}

/****************************************************************************/

/**
 * Basic usage function for 'net rpc user'.
 * @param argc	Standard main() style argc.
 * @param argv	Standard main() style argv. Initial components are already
 *		stripped.
 **/

static int rpc_user_usage(struct net_context *c, int argc, const char **argv)
{
	return net_user_usage(c, argc, argv);
}

/**
 * Add a new user to a remote RPC server.
 *
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 *
 * @return A shell status integer (0 for success).
 **/

static int rpc_user_add(struct net_context *c, int argc, const char **argv)
{
	NET_API_STATUS status;
	struct USER_INFO_1 info1;
	uint32_t parm_error = 0;

	if (argc < 1 || c->display_usage) {
		rpc_user_usage(c, argc, argv);
		return 0;
	}

	ZERO_STRUCT(info1);

	info1.usri1_name = argv[0];
	if (argc == 2) {
		info1.usri1_password = argv[1];
	}

	status = NetUserAdd(c->opt_host, 1, (uint8_t *)&info1, &parm_error);

	if (status != 0) {
		d_fprintf(stderr,_("Failed to add user '%s' with error: %s.\n"),
			argv[0], libnetapi_get_error_string(c->netapi_ctx,
							    status));
		return -1;
	} else {
		d_printf(_("Added user '%s'.\n"), argv[0]);
	}

	return 0;
}

/**
 * Rename a user on a remote RPC server.
 *
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 *
 * @return A shell status integer (0 for success).
 **/

static int rpc_user_rename(struct net_context *c, int argc, const char **argv)
{
	NET_API_STATUS status;
	struct USER_INFO_0 u0;
	uint32_t parm_err = 0;

	if (argc != 2 || c->display_usage) {
		rpc_user_usage(c, argc, argv);
		return 0;
	}

	u0.usri0_name = argv[1];

	status = NetUserSetInfo(c->opt_host, argv[0],
				0, (uint8_t *)&u0, &parm_err);
	if (status) {
		d_fprintf(stderr,
			  _("Failed to rename user from %s to %s - %s\n"),
			  argv[0], argv[1],
			  libnetapi_get_error_string(c->netapi_ctx, status));
	} else {
		d_printf(_("Renamed user from %s to %s\n"), argv[0], argv[1]);
	}

	return status;
}

/**
 * Set a user's primary group
 *
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 *
 * @return A shell status integer (0 for success).
 **/

static int rpc_user_setprimarygroup(struct net_context *c, int argc,
				    const char **argv)
{
	NET_API_STATUS status;
	uint8_t *buffer;
	struct GROUP_INFO_2 *g2;
	struct USER_INFO_1051 u1051;
	uint32_t parm_err = 0;

	if (argc != 2 || c->display_usage) {
		rpc_user_usage(c, argc, argv);
		return 0;
	}

	status = NetGroupGetInfo(c->opt_host, argv[1], 2, &buffer);
	if (status) {
		d_fprintf(stderr, _("Failed to find group name %s -- %s\n"),
			  argv[1],
			  libnetapi_get_error_string(c->netapi_ctx, status));
		return status;
	}
	g2 = (struct GROUP_INFO_2 *)buffer;

	u1051.usri1051_primary_group_id = g2->grpi2_group_id;

	NetApiBufferFree(buffer);

	status = NetUserSetInfo(c->opt_host, argv[0], 1051,
				(uint8_t *)&u1051, &parm_err);
	if (status) {
		d_fprintf(stderr,
			  _("Failed to set user's primary group %s to %s - "
			    "%s\n"), argv[0], argv[1],
			  libnetapi_get_error_string(c->netapi_ctx, status));
	} else {
		d_printf(_("Set primary group of user %s to %s\n"), argv[0],
			 argv[1]);
	}
	return status;
}

/**
 * Delete a user from a remote RPC server.
 *
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 *
 * @return A shell status integer (0 for success).
 **/

static int rpc_user_delete(struct net_context *c, int argc, const char **argv)
{
	NET_API_STATUS status;

	if (argc < 1 || c->display_usage) {
		rpc_user_usage(c, argc, argv);
		return 0;
	}

	status = NetUserDel(c->opt_host, argv[0]);

	if (status != 0) {
                d_fprintf(stderr, _("Failed to delete user '%s' with: %s.\n"),
			  argv[0],
			  libnetapi_get_error_string(c->netapi_ctx, status));
		return -1;
        } else {
                d_printf(_("Deleted user '%s'.\n"), argv[0]);
        }

	return 0;
}

/**
 * Set a user's password on a remote RPC server.
 *
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 *
 * @return A shell status integer (0 for success).
 **/

static int rpc_user_password(struct net_context *c, int argc, const char **argv)
{
	NET_API_STATUS status;
	char *prompt = NULL;
	struct USER_INFO_1003 u1003;
	uint32_t parm_err = 0;
	int ret;

	if (argc < 1 || c->display_usage) {
		rpc_user_usage(c, argc, argv);
		return 0;
	}

	if (argv[1]) {
		u1003.usri1003_password = argv[1];
	} else {
		ret = asprintf(&prompt, _("Enter new password for %s:"),
			       argv[0]);
		if (ret == -1) {
			return -1;
		}
		u1003.usri1003_password = talloc_strdup(c, getpass(prompt));
		SAFE_FREE(prompt);
		if (u1003.usri1003_password == NULL) {
			return -1;
		}
	}

	status = NetUserSetInfo(c->opt_host, argv[0], 1003, (uint8_t *)&u1003, &parm_err);

	/* Display results */
	if (status != 0) {
		d_fprintf(stderr,
			_("Failed to set password for '%s' with error: %s.\n"),
			argv[0], libnetapi_get_error_string(c->netapi_ctx,
							    status));
		return -1;
	}

	return 0;
}

/**
 * List a user's groups from a remote RPC server.
 *
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 *
 * @return A shell status integer (0 for success)
 **/

static int rpc_user_info(struct net_context *c, int argc, const char **argv)

{
	NET_API_STATUS status;
	struct GROUP_USERS_INFO_0 *u0 = NULL;
	uint32_t entries_read = 0;
	uint32_t total_entries = 0;
	int i;


	if (argc < 1 || c->display_usage) {
		rpc_user_usage(c, argc, argv);
		return 0;
	}

	status = NetUserGetGroups(c->opt_host,
				  argv[0],
				  0,
				  (uint8_t **)(void *)&u0,
				  (uint32_t)-1,
				  &entries_read,
				  &total_entries);
	if (status != 0) {
		d_fprintf(stderr,
			_("Failed to get groups for '%s' with error: %s.\n"),
			argv[0], libnetapi_get_error_string(c->netapi_ctx,
							    status));
		return -1;
	}

	for (i=0; i < entries_read; i++) {
		printf("%s\n", u0->grui0_name);
		u0++;
	}

	return 0;
}

/**
 * List users on a remote RPC server.
 *
 * All parameters are provided by the run_rpc_command function, except for
 * argc, argv which are passed through.
 *
 * @param domain_sid The domain sid acquired from the remote server.
 * @param cli A cli_state connected to the server.
 * @param mem_ctx Talloc context, destroyed on completion of the function.
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 *
 * @return Normal NTSTATUS return.
 **/

static int rpc_user_list(struct net_context *c, int argc, const char **argv)
{
	NET_API_STATUS status;
	uint32_t start_idx=0, num_entries, i, loop_count = 0;
	struct NET_DISPLAY_USER *info = NULL;
	void *buffer = NULL;

	/* Query domain users */
	if (c->opt_long_list_entries)
		d_printf(_("\nUser name             Comment"
			   "\n-----------------------------\n"));
	do {
		uint32_t max_entries, max_size;

		dcerpc_get_query_dispinfo_params(
			loop_count, &max_entries, &max_size);

		status = NetQueryDisplayInformation(c->opt_host,
						    1,
						    start_idx,
						    max_entries,
						    max_size,
						    &num_entries,
						    &buffer);
		if (status != 0 && status != ERROR_MORE_DATA) {
			return status;
		}

		info = (struct NET_DISPLAY_USER *)buffer;

		for (i = 0; i < num_entries; i++) {

			if (c->opt_long_list_entries)
				printf("%-21.21s %s\n", info->usri1_name,
					info->usri1_comment);
			else
				printf("%s\n", info->usri1_name);
			info++;
		}

		NetApiBufferFree(buffer);

		loop_count++;
		start_idx += num_entries;

	} while (status == ERROR_MORE_DATA);

	return status;
}

/**
 * 'net rpc user' entrypoint.
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 **/

int net_rpc_user(struct net_context *c, int argc, const char **argv)
{
	NET_API_STATUS status;

	struct functable func[] = {
		{
			"add",
			rpc_user_add,
			NET_TRANSPORT_RPC,
			N_("Add specified user"),
			N_("net rpc user add\n"
			   "    Add specified user")
		},
		{
			"info",
			rpc_user_info,
			NET_TRANSPORT_RPC,
			N_("List domain groups of user"),
			N_("net rpc user info\n"
			   "    List domain groups of user")
		},
		{
			"delete",
			rpc_user_delete,
			NET_TRANSPORT_RPC,
			N_("Remove specified user"),
			N_("net rpc user delete\n"
			   "    Remove specified user")
		},
		{
			"password",
			rpc_user_password,
			NET_TRANSPORT_RPC,
			N_("Change user password"),
			N_("net rpc user password\n"
			   "    Change user password")
		},
		{
			"rename",
			rpc_user_rename,
			NET_TRANSPORT_RPC,
			N_("Rename specified user"),
			N_("net rpc user rename\n"
			   "    Rename specified user")
		},
		{
			"setprimarygroup",
			rpc_user_setprimarygroup,
			NET_TRANSPORT_RPC,
			"Set a user's primary group",
			"net rpc user setprimarygroup\n"
			"    Set a user's primary group"
		},
		{NULL, NULL, 0, NULL, NULL}
	};

	status = libnetapi_net_init(&c->netapi_ctx);
	if (status != 0) {
		return -1;
	}
	libnetapi_set_username(c->netapi_ctx, c->opt_user_name);
	libnetapi_set_password(c->netapi_ctx, c->opt_password);
	if (c->opt_kerberos) {
		libnetapi_set_use_kerberos(c->netapi_ctx);
	}

	if (argc == 0) {
		if (c->display_usage) {
			d_printf(  "%s\n"
				   "net rpc user\n"
				   "    %s\n",
				 _("Usage:"),
				 _("List all users"));
			net_display_usage_from_functable(func);
			return 0;
		}

		return rpc_user_list(c, argc, argv);
	}

	return net_run_function(c, argc, argv, "net rpc user", func);
}

static NTSTATUS rpc_sh_user_list(struct net_context *c,
				 TALLOC_CTX *mem_ctx,
				 struct rpc_sh_ctx *ctx,
				 struct rpc_pipe_client *pipe_hnd,
				 int argc, const char **argv)
{
	return werror_to_ntstatus(W_ERROR(rpc_user_list(c, argc, argv)));
}

static NTSTATUS rpc_sh_user_info(struct net_context *c,
				 TALLOC_CTX *mem_ctx,
				 struct rpc_sh_ctx *ctx,
				 struct rpc_pipe_client *pipe_hnd,
				 int argc, const char **argv)
{
	return werror_to_ntstatus(W_ERROR(rpc_user_info(c, argc, argv)));
}

static NTSTATUS rpc_sh_handle_user(struct net_context *c,
				   TALLOC_CTX *mem_ctx,
				   struct rpc_sh_ctx *ctx,
				   struct rpc_pipe_client *pipe_hnd,
				   int argc, const char **argv,
				   NTSTATUS (*fn)(
					   struct net_context *c,
					   TALLOC_CTX *mem_ctx,
					   struct rpc_sh_ctx *ctx,
					   struct rpc_pipe_client *pipe_hnd,
					   struct policy_handle *user_hnd,
					   int argc, const char **argv))
{
	struct policy_handle connect_pol, domain_pol, user_pol;
	NTSTATUS status, result;
	struct dom_sid sid;
	uint32 rid;
	enum lsa_SidType type;
	struct dcerpc_binding_handle *b = pipe_hnd->binding_handle;

	if (argc == 0) {
		d_fprintf(stderr, "%s %s <username>\n", _("Usage:"),
			  ctx->whoami);
		return NT_STATUS_INVALID_PARAMETER;
	}

	ZERO_STRUCT(connect_pol);
	ZERO_STRUCT(domain_pol);
	ZERO_STRUCT(user_pol);

	status = net_rpc_lookup_name(c, mem_ctx, rpc_pipe_np_smb_conn(pipe_hnd),
				     argv[0], NULL, NULL, &sid, &type);
	if (!NT_STATUS_IS_OK(status)) {
		d_fprintf(stderr, _("Could not lookup %s: %s\n"), argv[0],
			  nt_errstr(status));
		goto done;
	}

	if (type != SID_NAME_USER) {
		d_fprintf(stderr, _("%s is a %s, not a user\n"), argv[0],
			  sid_type_lookup(type));
		status = NT_STATUS_NO_SUCH_USER;
		goto done;
	}

	if (!sid_peek_check_rid(ctx->domain_sid, &sid, &rid)) {
		d_fprintf(stderr, _("%s is not in our domain\n"), argv[0]);
		status = NT_STATUS_NO_SUCH_USER;
		goto done;
	}

	status = dcerpc_samr_Connect2(b, mem_ctx,
				      pipe_hnd->desthost,
				      MAXIMUM_ALLOWED_ACCESS,
				      &connect_pol,
				      &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto done;
	}

	status = dcerpc_samr_OpenDomain(b, mem_ctx,
					&connect_pol,
					MAXIMUM_ALLOWED_ACCESS,
					ctx->domain_sid,
					&domain_pol,
					&result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto done;
	}

	status = dcerpc_samr_OpenUser(b, mem_ctx,
				      &domain_pol,
				      MAXIMUM_ALLOWED_ACCESS,
				      rid,
				      &user_pol,
				      &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto done;
	}

	status = fn(c, mem_ctx, ctx, pipe_hnd, &user_pol, argc-1, argv+1);

 done:
	if (is_valid_policy_hnd(&user_pol)) {
		dcerpc_samr_Close(b, mem_ctx, &user_pol, &result);
	}
	if (is_valid_policy_hnd(&domain_pol)) {
		dcerpc_samr_Close(b, mem_ctx, &domain_pol, &result);
	}
	if (is_valid_policy_hnd(&connect_pol)) {
		dcerpc_samr_Close(b, mem_ctx, &connect_pol, &result);
	}
	return status;
}

static NTSTATUS rpc_sh_user_show_internals(struct net_context *c,
					   TALLOC_CTX *mem_ctx,
					   struct rpc_sh_ctx *ctx,
					   struct rpc_pipe_client *pipe_hnd,
					   struct policy_handle *user_hnd,
					   int argc, const char **argv)
{
	NTSTATUS status, result;
	union samr_UserInfo *info = NULL;
	struct dcerpc_binding_handle *b = pipe_hnd->binding_handle;

	if (argc != 0) {
		d_fprintf(stderr, "%s %s show <username>\n", _("Usage:"),
			  ctx->whoami);
		return NT_STATUS_INVALID_PARAMETER;
	}

	status = dcerpc_samr_QueryUserInfo(b, mem_ctx,
					   user_hnd,
					   21,
					   &info,
					   &result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if (!NT_STATUS_IS_OK(result)) {
		return result;
	}

	d_printf(_("user rid: %d, group rid: %d\n"),
		info->info21.rid,
		info->info21.primary_gid);

	return result;
}

static NTSTATUS rpc_sh_user_show(struct net_context *c,
				 TALLOC_CTX *mem_ctx,
				 struct rpc_sh_ctx *ctx,
				 struct rpc_pipe_client *pipe_hnd,
				 int argc, const char **argv)
{
	return rpc_sh_handle_user(c, mem_ctx, ctx, pipe_hnd, argc, argv,
				  rpc_sh_user_show_internals);
}

#define FETCHSTR(name, rec) \
do { if (strequal(ctx->thiscmd, name)) { \
	oldval = talloc_strdup(mem_ctx, info->info21.rec.string); } \
} while (0);

#define SETSTR(name, rec, flag) \
do { if (strequal(ctx->thiscmd, name)) { \
	init_lsa_String(&(info->info21.rec), argv[0]); \
	info->info21.fields_present |= SAMR_FIELD_##flag; } \
} while (0);

static NTSTATUS rpc_sh_user_str_edit_internals(struct net_context *c,
					       TALLOC_CTX *mem_ctx,
					       struct rpc_sh_ctx *ctx,
					       struct rpc_pipe_client *pipe_hnd,
					       struct policy_handle *user_hnd,
					       int argc, const char **argv)
{
	NTSTATUS status, result;
	const char *username;
	const char *oldval = "";
	union samr_UserInfo *info = NULL;
	struct dcerpc_binding_handle *b = pipe_hnd->binding_handle;

	if (argc > 1) {
		d_fprintf(stderr, "%s %s <username> [new value|NULL]\n",
			  _("Usage:"), ctx->whoami);
		return NT_STATUS_INVALID_PARAMETER;
	}

	status = dcerpc_samr_QueryUserInfo(b, mem_ctx,
					   user_hnd,
					   21,
					   &info,
					   &result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if (!NT_STATUS_IS_OK(result)) {
		return result;
	}

	username = talloc_strdup(mem_ctx, info->info21.account_name.string);

	FETCHSTR("fullname", full_name);
	FETCHSTR("homedir", home_directory);
	FETCHSTR("homedrive", home_drive);
	FETCHSTR("logonscript", logon_script);
	FETCHSTR("profilepath", profile_path);
	FETCHSTR("description", description);

	if (argc == 0) {
		d_printf(_("%s's %s: [%s]\n"), username, ctx->thiscmd, oldval);
		goto done;
	}

	if (strcmp(argv[0], "NULL") == 0) {
		argv[0] = "";
	}

	ZERO_STRUCT(info->info21);

	SETSTR("fullname", full_name, FULL_NAME);
	SETSTR("homedir", home_directory, HOME_DIRECTORY);
	SETSTR("homedrive", home_drive, HOME_DRIVE);
	SETSTR("logonscript", logon_script, LOGON_SCRIPT);
	SETSTR("profilepath", profile_path, PROFILE_PATH);
	SETSTR("description", description, DESCRIPTION);

	status = dcerpc_samr_SetUserInfo(b, mem_ctx,
					 user_hnd,
					 21,
					 info,
					 &result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = result;

	d_printf(_("Set %s's %s from [%s] to [%s]\n"), username,
		 ctx->thiscmd, oldval, argv[0]);

 done:

	return status;
}

#define HANDLEFLG(name, rec) \
do { if (strequal(ctx->thiscmd, name)) { \
	oldval = (oldflags & ACB_##rec) ? "yes" : "no"; \
	if (newval) { \
		newflags = oldflags | ACB_##rec; \
	} else { \
		newflags = oldflags & ~ACB_##rec; \
	} } } while (0);

static NTSTATUS rpc_sh_user_str_edit(struct net_context *c,
				     TALLOC_CTX *mem_ctx,
				     struct rpc_sh_ctx *ctx,
				     struct rpc_pipe_client *pipe_hnd,
				     int argc, const char **argv)
{
	return rpc_sh_handle_user(c, mem_ctx, ctx, pipe_hnd, argc, argv,
				  rpc_sh_user_str_edit_internals);
}

static NTSTATUS rpc_sh_user_flag_edit_internals(struct net_context *c,
						TALLOC_CTX *mem_ctx,
						struct rpc_sh_ctx *ctx,
						struct rpc_pipe_client *pipe_hnd,
						struct policy_handle *user_hnd,
						int argc, const char **argv)
{
	NTSTATUS status, result;
	const char *username;
	const char *oldval = "unknown";
	uint32 oldflags, newflags;
	bool newval;
	union samr_UserInfo *info = NULL;
	struct dcerpc_binding_handle *b = pipe_hnd->binding_handle;

	if ((argc > 1) ||
	    ((argc == 1) && !strequal(argv[0], "yes") &&
	     !strequal(argv[0], "no"))) {
		/* TRANSATORS: The yes|no here are program keywords. Please do
		   not translate. */
		d_fprintf(stderr, _("Usage: %s <username> [yes|no]\n"),
			  ctx->whoami);
		return NT_STATUS_INVALID_PARAMETER;
	}

	newval = strequal(argv[0], "yes");

	status = dcerpc_samr_QueryUserInfo(b, mem_ctx,
					   user_hnd,
					   21,
					   &info,
					   &result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if (!NT_STATUS_IS_OK(result)) {
		return result;
	}

	username = talloc_strdup(mem_ctx, info->info21.account_name.string);
	oldflags = info->info21.acct_flags;
	newflags = info->info21.acct_flags;

	HANDLEFLG("disabled", DISABLED);
	HANDLEFLG("pwnotreq", PWNOTREQ);
	HANDLEFLG("autolock", AUTOLOCK);
	HANDLEFLG("pwnoexp", PWNOEXP);

	if (argc == 0) {
		d_printf(_("%s's %s flag: %s\n"), username, ctx->thiscmd,
			 oldval);
		goto done;
	}

	ZERO_STRUCT(info->info21);

	info->info21.acct_flags = newflags;
	info->info21.fields_present = SAMR_FIELD_ACCT_FLAGS;

	status = dcerpc_samr_SetUserInfo(b, mem_ctx,
					 user_hnd,
					 21,
					 info,
					 &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	status = result;
	if (NT_STATUS_IS_OK(result)) {
		d_printf(_("Set %s's %s flag from [%s] to [%s]\n"), username,
			 ctx->thiscmd, oldval, argv[0]);
	}

 done:

	return status;
}

static NTSTATUS rpc_sh_user_flag_edit(struct net_context *c,
				      TALLOC_CTX *mem_ctx,
				      struct rpc_sh_ctx *ctx,
				      struct rpc_pipe_client *pipe_hnd,
				      int argc, const char **argv)
{
	return rpc_sh_handle_user(c, mem_ctx, ctx, pipe_hnd, argc, argv,
				  rpc_sh_user_flag_edit_internals);
}

struct rpc_sh_cmd *net_rpc_user_edit_cmds(struct net_context *c,
					  TALLOC_CTX *mem_ctx,
					  struct rpc_sh_ctx *ctx)
{
	static struct rpc_sh_cmd cmds[] = {

		{ "fullname", NULL, &ndr_table_samr.syntax_id, rpc_sh_user_str_edit,
		  N_("Show/Set a user's full name") },

		{ "homedir", NULL, &ndr_table_samr.syntax_id, rpc_sh_user_str_edit,
		  N_("Show/Set a user's home directory") },

		{ "homedrive", NULL, &ndr_table_samr.syntax_id, rpc_sh_user_str_edit,
		  N_("Show/Set a user's home drive") },

		{ "logonscript", NULL, &ndr_table_samr.syntax_id, rpc_sh_user_str_edit,
		  N_("Show/Set a user's logon script") },

		{ "profilepath", NULL, &ndr_table_samr.syntax_id, rpc_sh_user_str_edit,
		  N_("Show/Set a user's profile path") },

		{ "description", NULL, &ndr_table_samr.syntax_id, rpc_sh_user_str_edit,
		  N_("Show/Set a user's description") },

		{ "disabled", NULL, &ndr_table_samr.syntax_id, rpc_sh_user_flag_edit,
		  N_("Show/Set whether a user is disabled") },

		{ "autolock", NULL, &ndr_table_samr.syntax_id, rpc_sh_user_flag_edit,
		  N_("Show/Set whether a user locked out") },

		{ "pwnotreq", NULL, &ndr_table_samr.syntax_id, rpc_sh_user_flag_edit,
		  N_("Show/Set whether a user does not need a password") },

		{ "pwnoexp", NULL, &ndr_table_samr.syntax_id, rpc_sh_user_flag_edit,
		  N_("Show/Set whether a user's password does not expire") },

		{ NULL, NULL, 0, NULL, NULL }
	};

	return cmds;
}

struct rpc_sh_cmd *net_rpc_user_cmds(struct net_context *c,
				     TALLOC_CTX *mem_ctx,
				     struct rpc_sh_ctx *ctx)
{
	static struct rpc_sh_cmd cmds[] = {

		{ "list", NULL, &ndr_table_samr.syntax_id, rpc_sh_user_list,
		  N_("List available users") },

		{ "info", NULL, &ndr_table_samr.syntax_id, rpc_sh_user_info,
		  N_("List the domain groups a user is member of") },

		{ "show", NULL, &ndr_table_samr.syntax_id, rpc_sh_user_show,
		  N_("Show info about a user") },

		{ "edit", net_rpc_user_edit_cmds, 0, NULL,
		  N_("Show/Modify a user's fields") },

		{ NULL, NULL, 0, NULL, NULL }
	};

	return cmds;
}

/****************************************************************************/

/**
 * Basic usage function for 'net rpc group'.
 * @param argc	Standard main() style argc.
 * @param argv	Standard main() style argv. Initial components are already
 *		stripped.
 **/

static int rpc_group_usage(struct net_context *c, int argc, const char **argv)
{
	return net_group_usage(c, argc, argv);
}

/**
 * Delete group on a remote RPC server.
 *
 * All parameters are provided by the run_rpc_command function, except for
 * argc, argv which are passed through.
 *
 * @param domain_sid The domain sid acquired from the remote server.
 * @param cli A cli_state connected to the server.
 * @param mem_ctx Talloc context, destroyed on completion of the function.
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 *
 * @return Normal NTSTATUS return.
 **/

static NTSTATUS rpc_group_delete_internals(struct net_context *c,
					const struct dom_sid *domain_sid,
					const char *domain_name,
					struct cli_state *cli,
					struct rpc_pipe_client *pipe_hnd,
					TALLOC_CTX *mem_ctx,
					int argc,
					const char **argv)
{
	struct policy_handle connect_pol, domain_pol, group_pol, user_pol;
	bool group_is_primary = false;
	NTSTATUS status, result;
	uint32_t group_rid;
	struct samr_RidAttrArray *rids = NULL;
	/* char **names; */
	int i;
	/* struct samr_RidWithAttribute *user_gids; */
	struct dcerpc_binding_handle *b = pipe_hnd->binding_handle;

	struct samr_Ids group_rids, name_types;
	struct lsa_String lsa_acct_name;
	union samr_UserInfo *info = NULL;

	if (argc < 1 || c->display_usage) {
		rpc_group_usage(c, argc,argv);
		return NT_STATUS_OK; /* ok? */
	}

	status = dcerpc_samr_Connect2(b, mem_ctx,
				      pipe_hnd->desthost,
				      MAXIMUM_ALLOWED_ACCESS,
				      &connect_pol,
				      &result);
        if (!NT_STATUS_IS_OK(status)) {
		d_fprintf(stderr, _("Request samr_Connect2 failed\n"));
		goto done;
        }

        if (!NT_STATUS_IS_OK(result)) {
		status = result;
		d_fprintf(stderr, _("Request samr_Connect2 failed\n"));
		goto done;
        }

	status = dcerpc_samr_OpenDomain(b, mem_ctx,
					&connect_pol,
					MAXIMUM_ALLOWED_ACCESS,
					CONST_DISCARD(struct dom_sid2 *, domain_sid),
					&domain_pol,
					&result);
        if (!NT_STATUS_IS_OK(status)) {
		d_fprintf(stderr, _("Request open_domain failed\n"));
		goto done;
        }

        if (!NT_STATUS_IS_OK(result)) {
		status = result;
		d_fprintf(stderr, _("Request open_domain failed\n"));
		goto done;
        }

	init_lsa_String(&lsa_acct_name, argv[0]);

	status = dcerpc_samr_LookupNames(b, mem_ctx,
					 &domain_pol,
					 1,
					 &lsa_acct_name,
					 &group_rids,
					 &name_types,
					 &result);
	if (!NT_STATUS_IS_OK(status)) {
		d_fprintf(stderr, _("Lookup of '%s' failed\n"),argv[0]);
		goto done;
	}

	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		d_fprintf(stderr, _("Lookup of '%s' failed\n"),argv[0]);
		goto done;
	}
	if (group_rids.count != 1) {
		status = NT_STATUS_INVALID_NETWORK_RESPONSE;
		goto done;
	}
	if (name_types.count != 1) {
		status = NT_STATUS_INVALID_NETWORK_RESPONSE;
		goto done;
	}

	switch (name_types.ids[0])
	{
	case SID_NAME_DOM_GRP:
		status = dcerpc_samr_OpenGroup(b, mem_ctx,
					       &domain_pol,
					       MAXIMUM_ALLOWED_ACCESS,
					       group_rids.ids[0],
					       &group_pol,
					       &result);
		if (!NT_STATUS_IS_OK(status)) {
			d_fprintf(stderr, _("Request open_group failed"));
			goto done;
		}

		if (!NT_STATUS_IS_OK(result)) {
			status = result;
			d_fprintf(stderr, _("Request open_group failed"));
			goto done;
		}

		group_rid = group_rids.ids[0];

		status = dcerpc_samr_QueryGroupMember(b, mem_ctx,
						      &group_pol,
						      &rids,
						      &result);
		if (!NT_STATUS_IS_OK(status)) {
			d_fprintf(stderr,
				  _("Unable to query group members of %s"),
				  argv[0]);
			goto done;
		}

		if (!NT_STATUS_IS_OK(result)) {
			status = result;
			d_fprintf(stderr,
				  _("Unable to query group members of %s"),
				  argv[0]);
			goto done;
		}

		if (c->opt_verbose) {
			d_printf(
				_("Domain Group %s (rid: %d) has %d members\n"),
				argv[0],group_rid, rids->count);
		}

		/* Check if group is anyone's primary group */
                for (i = 0; i < rids->count; i++)
		{
	                status = dcerpc_samr_OpenUser(b, mem_ctx,
						      &domain_pol,
						      MAXIMUM_ALLOWED_ACCESS,
						      rids->rids[i],
						      &user_pol,
						      &result);
			if (!NT_STATUS_IS_OK(status)) {
				d_fprintf(stderr,
					_("Unable to open group member %d\n"),
					rids->rids[i]);
				goto done;
			}

			if (!NT_STATUS_IS_OK(result)) {
				status = result;
				d_fprintf(stderr,
					_("Unable to open group member %d\n"),
					rids->rids[i]);
				goto done;
			}

			status = dcerpc_samr_QueryUserInfo(b, mem_ctx,
							   &user_pol,
							   21,
							   &info,
							   &result);
			if (!NT_STATUS_IS_OK(status)) {
				d_fprintf(stderr,
					_("Unable to lookup userinfo for group "
					  "member %d\n"),
					rids->rids[i]);
				goto done;
			}

			if (!NT_STATUS_IS_OK(result)) {
				status = result;
				d_fprintf(stderr,
					_("Unable to lookup userinfo for group "
					  "member %d\n"),
					rids->rids[i]);
				goto done;
			}

			if (info->info21.primary_gid == group_rid) {
				if (c->opt_verbose) {
					d_printf(_("Group is primary group "
						   "of %s\n"),
						info->info21.account_name.string);
				}
				group_is_primary = true;
                        }

			dcerpc_samr_Close(b, mem_ctx, &user_pol, &result);
		}

		if (group_is_primary) {
			d_fprintf(stderr, _("Unable to delete group because "
				 "some of it's members have it as primary "
				 "group\n"));
			status = NT_STATUS_MEMBERS_PRIMARY_GROUP;
			goto done;
		}

		/* remove all group members */
		for (i = 0; i < rids->count; i++)
		{
			if (c->opt_verbose)
				d_printf(_("Remove group member %d..."),
					rids->rids[i]);
			status = dcerpc_samr_DeleteGroupMember(b, mem_ctx,
							       &group_pol,
							       rids->rids[i],
							       &result);
			if (!NT_STATUS_IS_OK(status)) {
				goto done;
			}
			status = result;
			if (NT_STATUS_IS_OK(result)) {
				if (c->opt_verbose)
					d_printf(_("ok\n"));
			} else {
				if (c->opt_verbose)
					d_printf("%s\n", _("failed"));
				goto done;
			}
		}

		status = dcerpc_samr_DeleteDomainGroup(b, mem_ctx,
						       &group_pol,
						       &result);
		if (!NT_STATUS_IS_OK(status)) {
			break;
		}

		status = result;

		break;
	/* removing a local group is easier... */
	case SID_NAME_ALIAS:
		status = dcerpc_samr_OpenAlias(b, mem_ctx,
					       &domain_pol,
					       MAXIMUM_ALLOWED_ACCESS,
					       group_rids.ids[0],
					       &group_pol,
					       &result);
		if (!NT_STATUS_IS_OK(status)) {
			d_fprintf(stderr, _("Request open_alias failed\n"));
			goto done;
		}
		if (!NT_STATUS_IS_OK(result)) {
			status = result;
			d_fprintf(stderr, _("Request open_alias failed\n"));
			goto done;
		}

		status = dcerpc_samr_DeleteDomAlias(b, mem_ctx,
						    &group_pol,
						    &result);
		if (!NT_STATUS_IS_OK(status)) {
			break;
		}

		status = result;

		break;
	default:
		d_fprintf(stderr, _("%s is of type %s. This command is only "
				    "for deleting local or global groups\n"),
			argv[0],sid_type_lookup(name_types.ids[0]));
		status = NT_STATUS_UNSUCCESSFUL;
		goto done;
	}

	if (NT_STATUS_IS_OK(status)) {
		if (c->opt_verbose)
			d_printf(_("Deleted %s '%s'\n"),
				 sid_type_lookup(name_types.ids[0]), argv[0]);
	} else {
		d_fprintf(stderr, _("Deleting of %s failed: %s\n"), argv[0],
			get_friendly_nt_error_msg(status));
	}

 done:
	return status;

}

static int rpc_group_delete(struct net_context *c, int argc, const char **argv)
{
	return run_rpc_command(c, NULL, &ndr_table_samr.syntax_id, 0,
			       rpc_group_delete_internals, argc,argv);
}

static int rpc_group_add_internals(struct net_context *c, int argc, const char **argv)
{
	NET_API_STATUS status;
	struct GROUP_INFO_1 info1;
	uint32_t parm_error = 0;

	if (argc != 1 || c->display_usage) {
		rpc_group_usage(c, argc, argv);
		return 0;
	}

	ZERO_STRUCT(info1);

	info1.grpi1_name = argv[0];
	if (c->opt_comment && strlen(c->opt_comment) > 0) {
		info1.grpi1_comment = c->opt_comment;
	}

	status = NetGroupAdd(c->opt_host, 1, (uint8_t *)&info1, &parm_error);

	if (status != 0) {
		d_fprintf(stderr,
			_("Failed to add group '%s' with error: %s.\n"),
			argv[0], libnetapi_get_error_string(c->netapi_ctx,
							    status));
		return -1;
	} else {
		d_printf(_("Added group '%s'.\n"), argv[0]);
	}

	return 0;
}

static int rpc_alias_add_internals(struct net_context *c, int argc, const char **argv)
{
	NET_API_STATUS status;
	struct LOCALGROUP_INFO_1 info1;
	uint32_t parm_error = 0;

	if (argc != 1 || c->display_usage) {
		rpc_group_usage(c, argc, argv);
		return 0;
	}

	ZERO_STRUCT(info1);

	info1.lgrpi1_name = argv[0];
	if (c->opt_comment && strlen(c->opt_comment) > 0) {
		info1.lgrpi1_comment = c->opt_comment;
	}

	status = NetLocalGroupAdd(c->opt_host, 1, (uint8_t *)&info1, &parm_error);

	if (status != 0) {
		d_fprintf(stderr,
			_("Failed to add alias '%s' with error: %s.\n"),
			argv[0], libnetapi_get_error_string(c->netapi_ctx,
							    status));
		return -1;
	} else {
		d_printf(_("Added alias '%s'.\n"), argv[0]);
	}

	return 0;
}

static int rpc_group_add(struct net_context *c, int argc, const char **argv)
{
	if (c->opt_localgroup)
		return rpc_alias_add_internals(c, argc, argv);

	return rpc_group_add_internals(c, argc, argv);
}

static NTSTATUS get_sid_from_name(struct cli_state *cli,
				TALLOC_CTX *mem_ctx,
				const char *name,
				struct dom_sid *sid,
				enum lsa_SidType *type)
{
	struct dom_sid *sids = NULL;
	enum lsa_SidType *types = NULL;
	struct rpc_pipe_client *pipe_hnd = NULL;
	struct policy_handle lsa_pol;
	NTSTATUS status, result;
	struct dcerpc_binding_handle *b;

	status = cli_rpc_pipe_open_noauth(cli, &ndr_table_lsarpc.syntax_id,
					  &pipe_hnd);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	b = pipe_hnd->binding_handle;

	status = rpccli_lsa_open_policy(pipe_hnd, mem_ctx, false,
				     SEC_FLAG_MAXIMUM_ALLOWED, &lsa_pol);

	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	status = rpccli_lsa_lookup_names(pipe_hnd, mem_ctx, &lsa_pol, 1,
				      &name, NULL, 1, &sids, &types);

	if (NT_STATUS_IS_OK(status)) {
		sid_copy(sid, &sids[0]);
		*type = types[0];
	}

	dcerpc_lsa_Close(b, mem_ctx, &lsa_pol, &result);

 done:
	if (pipe_hnd) {
		TALLOC_FREE(pipe_hnd);
	}

	if (!NT_STATUS_IS_OK(status) && (StrnCaseCmp(name, "S-", 2) == 0)) {

		/* Try as S-1-5-whatever */

		struct dom_sid tmp_sid;

		if (string_to_sid(&tmp_sid, name)) {
			sid_copy(sid, &tmp_sid);
			*type = SID_NAME_UNKNOWN;
			status = NT_STATUS_OK;
		}
	}

	return status;
}

static NTSTATUS rpc_add_groupmem(struct rpc_pipe_client *pipe_hnd,
				TALLOC_CTX *mem_ctx,
				const struct dom_sid *group_sid,
				const char *member)
{
	struct policy_handle connect_pol, domain_pol;
	NTSTATUS status, result;
	uint32 group_rid;
	struct policy_handle group_pol;
	struct dcerpc_binding_handle *b = pipe_hnd->binding_handle;

	struct samr_Ids rids, rid_types;
	struct lsa_String lsa_acct_name;

	struct dom_sid sid;

	sid_copy(&sid, group_sid);

	if (!sid_split_rid(&sid, &group_rid)) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	/* Get sam policy handle */
	status = dcerpc_samr_Connect2(b, mem_ctx,
				      pipe_hnd->desthost,
				      MAXIMUM_ALLOWED_ACCESS,
				      &connect_pol,
				      &result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if (!NT_STATUS_IS_OK(result)) {
		return result;
	}

	/* Get domain policy handle */
	status = dcerpc_samr_OpenDomain(b, mem_ctx,
					&connect_pol,
					MAXIMUM_ALLOWED_ACCESS,
					&sid,
					&domain_pol,
					&result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if (!NT_STATUS_IS_OK(result)) {
		return result;
	}

	init_lsa_String(&lsa_acct_name, member);

	status = dcerpc_samr_LookupNames(b, mem_ctx,
					 &domain_pol,
					 1,
					 &lsa_acct_name,
					 &rids,
					 &rid_types,
					 &result);
	if (!NT_STATUS_IS_OK(status)) {
		d_fprintf(stderr, _("Could not lookup up group member %s\n"),
			  member);
		goto done;
	}

	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		d_fprintf(stderr, _("Could not lookup up group member %s\n"),
			  member);
		goto done;
	}
	if (rids.count != 1) {
		status = NT_STATUS_INVALID_NETWORK_RESPONSE;
		goto done;
	}
	if (rid_types.count != 1) {
		status = NT_STATUS_INVALID_NETWORK_RESPONSE;
		goto done;
	}

	status = dcerpc_samr_OpenGroup(b, mem_ctx,
				       &domain_pol,
				       MAXIMUM_ALLOWED_ACCESS,
				       group_rid,
				       &group_pol,
				       &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto done;
	}

	status = dcerpc_samr_AddGroupMember(b, mem_ctx,
					    &group_pol,
					    rids.ids[0],
					    0x0005, /* unknown flags */
					    &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	status = result;

 done:
	dcerpc_samr_Close(b, mem_ctx, &connect_pol, &result);
	return status;
}

static NTSTATUS rpc_add_aliasmem(struct rpc_pipe_client *pipe_hnd,
				TALLOC_CTX *mem_ctx,
				const struct dom_sid *alias_sid,
				const char *member)
{
	struct policy_handle connect_pol, domain_pol;
	NTSTATUS status, result;
	uint32 alias_rid;
	struct policy_handle alias_pol;
	struct dcerpc_binding_handle *b = pipe_hnd->binding_handle;

	struct dom_sid member_sid;
	enum lsa_SidType member_type;

	struct dom_sid sid;

	sid_copy(&sid, alias_sid);

	if (!sid_split_rid(&sid, &alias_rid)) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	result = get_sid_from_name(rpc_pipe_np_smb_conn(pipe_hnd), mem_ctx,
				   member, &member_sid, &member_type);

	if (!NT_STATUS_IS_OK(result)) {
		d_fprintf(stderr, _("Could not lookup up group member %s\n"),
			  member);
		return result;
	}

	/* Get sam policy handle */
	status = dcerpc_samr_Connect2(b, mem_ctx,
				      pipe_hnd->desthost,
				      MAXIMUM_ALLOWED_ACCESS,
				      &connect_pol,
				      &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto done;
	}

	/* Get domain policy handle */
	status = dcerpc_samr_OpenDomain(b, mem_ctx,
					&connect_pol,
					MAXIMUM_ALLOWED_ACCESS,
					&sid,
					&domain_pol,
					&result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto done;
	}

	status = dcerpc_samr_OpenAlias(b, mem_ctx,
				       &domain_pol,
				       MAXIMUM_ALLOWED_ACCESS,
				       alias_rid,
				       &alias_pol,
				       &result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if (!NT_STATUS_IS_OK(result)) {
		return result;
	}

	status = dcerpc_samr_AddAliasMember(b, mem_ctx,
					    &alias_pol,
					    &member_sid,
					    &result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = result;

 done:
	dcerpc_samr_Close(b, mem_ctx, &connect_pol, &result);
	return status;
}

static NTSTATUS rpc_group_addmem_internals(struct net_context *c,
					const struct dom_sid *domain_sid,
					const char *domain_name,
					struct cli_state *cli,
					struct rpc_pipe_client *pipe_hnd,
					TALLOC_CTX *mem_ctx,
					int argc,
					const char **argv)
{
	struct dom_sid group_sid;
	enum lsa_SidType group_type;

	if (argc != 2 || c->display_usage) {
		d_printf("%s\n%s",
			 _("Usage:"),
			 _("net rpc group addmem <group> <member>\n"
			   "  Add a member to a group\n"
			   "    group\tGroup to add member to\n"
			   "    member\tMember to add to group\n"));
		return NT_STATUS_UNSUCCESSFUL;
	}

	if (!NT_STATUS_IS_OK(get_sid_from_name(cli, mem_ctx, argv[0],
					       &group_sid, &group_type))) {
		d_fprintf(stderr, _("Could not lookup group name %s\n"),
			  argv[0]);
		return NT_STATUS_UNSUCCESSFUL;
	}

	if (group_type == SID_NAME_DOM_GRP) {
		NTSTATUS result = rpc_add_groupmem(pipe_hnd, mem_ctx,
						   &group_sid, argv[1]);

		if (!NT_STATUS_IS_OK(result)) {
			d_fprintf(stderr, _("Could not add %s to %s: %s\n"),
				 argv[1], argv[0], nt_errstr(result));
		}
		return result;
	}

	if (group_type == SID_NAME_ALIAS) {
		NTSTATUS result = rpc_add_aliasmem(pipe_hnd, mem_ctx,
						   &group_sid, argv[1]);

		if (!NT_STATUS_IS_OK(result)) {
			d_fprintf(stderr, _("Could not add %s to %s: %s\n"),
				 argv[1], argv[0], nt_errstr(result));
		}
		return result;
	}

	d_fprintf(stderr, _("Can only add members to global or local groups "
		 "which %s is not\n"), argv[0]);

	return NT_STATUS_UNSUCCESSFUL;
}

static int rpc_group_addmem(struct net_context *c, int argc, const char **argv)
{
	return run_rpc_command(c, NULL, &ndr_table_samr.syntax_id, 0,
			       rpc_group_addmem_internals,
			       argc, argv);
}

static NTSTATUS rpc_del_groupmem(struct net_context *c,
				struct rpc_pipe_client *pipe_hnd,
				TALLOC_CTX *mem_ctx,
				const struct dom_sid *group_sid,
				const char *member)
{
	struct policy_handle connect_pol, domain_pol;
	NTSTATUS status, result;
	uint32 group_rid;
	struct policy_handle group_pol;
	struct dcerpc_binding_handle *b = pipe_hnd->binding_handle;

	struct samr_Ids rids, rid_types;
	struct lsa_String lsa_acct_name;

	struct dom_sid sid;

	sid_copy(&sid, group_sid);

	if (!sid_split_rid(&sid, &group_rid))
		return NT_STATUS_UNSUCCESSFUL;

	/* Get sam policy handle */
	status = dcerpc_samr_Connect2(b, mem_ctx,
				      pipe_hnd->desthost,
				      MAXIMUM_ALLOWED_ACCESS,
				      &connect_pol,
				      &result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if (!NT_STATUS_IS_OK(result)) {
		return result;
	}


	/* Get domain policy handle */
	status = dcerpc_samr_OpenDomain(b, mem_ctx,
					&connect_pol,
					MAXIMUM_ALLOWED_ACCESS,
					&sid,
					&domain_pol,
					&result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if (!NT_STATUS_IS_OK(result)) {
		return result;
	}

	init_lsa_String(&lsa_acct_name, member);

	status = dcerpc_samr_LookupNames(b, mem_ctx,
					 &domain_pol,
					 1,
					 &lsa_acct_name,
					 &rids,
					 &rid_types,
					 &result);
	if (!NT_STATUS_IS_OK(status)) {
		d_fprintf(stderr, _("Could not lookup up group member %s\n"),
			  member);
		goto done;
	}

	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		d_fprintf(stderr, _("Could not lookup up group member %s\n"),
			  member);
		goto done;
	}
	if (rids.count != 1) {
		status = NT_STATUS_INVALID_NETWORK_RESPONSE;
		goto done;
	}
	if (rid_types.count != 1) {
		status = NT_STATUS_INVALID_NETWORK_RESPONSE;
		goto done;
	}

	status = dcerpc_samr_OpenGroup(b, mem_ctx,
				       &domain_pol,
				       MAXIMUM_ALLOWED_ACCESS,
				       group_rid,
				       &group_pol,
				       &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto done;
	}

	status = dcerpc_samr_DeleteGroupMember(b, mem_ctx,
					       &group_pol,
					       rids.ids[0],
					       &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	status = result;
 done:
	dcerpc_samr_Close(b, mem_ctx, &connect_pol, &result);
	return status;
}

static NTSTATUS rpc_del_aliasmem(struct rpc_pipe_client *pipe_hnd,
				TALLOC_CTX *mem_ctx,
				const struct dom_sid *alias_sid,
				const char *member)
{
	struct policy_handle connect_pol, domain_pol;
	NTSTATUS status, result;
	uint32 alias_rid;
	struct policy_handle alias_pol;
	struct dcerpc_binding_handle *b = pipe_hnd->binding_handle;

	struct dom_sid member_sid;
	enum lsa_SidType member_type;

	struct dom_sid sid;

	sid_copy(&sid, alias_sid);

	if (!sid_split_rid(&sid, &alias_rid))
		return NT_STATUS_UNSUCCESSFUL;

	result = get_sid_from_name(rpc_pipe_np_smb_conn(pipe_hnd), mem_ctx,
				   member, &member_sid, &member_type);

	if (!NT_STATUS_IS_OK(result)) {
		d_fprintf(stderr, _("Could not lookup up group member %s\n"),
			  member);
		return result;
	}

	/* Get sam policy handle */
	status = dcerpc_samr_Connect2(b, mem_ctx,
				      pipe_hnd->desthost,
				      MAXIMUM_ALLOWED_ACCESS,
				      &connect_pol,
				      &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto done;
	}

	/* Get domain policy handle */
	status = dcerpc_samr_OpenDomain(b, mem_ctx,
					&connect_pol,
					MAXIMUM_ALLOWED_ACCESS,
					&sid,
					&domain_pol,
					&result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto done;
	}

	status = dcerpc_samr_OpenAlias(b, mem_ctx,
				       &domain_pol,
				       MAXIMUM_ALLOWED_ACCESS,
				       alias_rid,
				       &alias_pol,
				       &result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (!NT_STATUS_IS_OK(result)) {
		return result;
	}

	status = dcerpc_samr_DeleteAliasMember(b, mem_ctx,
					       &alias_pol,
					       &member_sid,
					       &result);

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = result;

 done:
	dcerpc_samr_Close(b, mem_ctx, &connect_pol, &result);
	return status;
}

static NTSTATUS rpc_group_delmem_internals(struct net_context *c,
					const struct dom_sid *domain_sid,
					const char *domain_name,
					struct cli_state *cli,
					struct rpc_pipe_client *pipe_hnd,
					TALLOC_CTX *mem_ctx,
					int argc,
					const char **argv)
{
	struct dom_sid group_sid;
	enum lsa_SidType group_type;

	if (argc != 2 || c->display_usage) {
		d_printf("%s\n%s",
			 _("Usage:"),
			 _("net rpc group delmem <group> <member>\n"
			   "  Delete a member from a group\n"
			   "    group\tGroup to delete member from\n"
			   "    member\tMember to delete from group\n"));
		return NT_STATUS_UNSUCCESSFUL;
	}

	if (!NT_STATUS_IS_OK(get_sid_from_name(cli, mem_ctx, argv[0],
					       &group_sid, &group_type))) {
		d_fprintf(stderr, _("Could not lookup group name %s\n"),
			  argv[0]);
		return NT_STATUS_UNSUCCESSFUL;
	}

	if (group_type == SID_NAME_DOM_GRP) {
		NTSTATUS result = rpc_del_groupmem(c, pipe_hnd, mem_ctx,
						   &group_sid, argv[1]);

		if (!NT_STATUS_IS_OK(result)) {
			d_fprintf(stderr, _("Could not del %s from %s: %s\n"),
				 argv[1], argv[0], nt_errstr(result));
		}
		return result;
	}

	if (group_type == SID_NAME_ALIAS) {
		NTSTATUS result = rpc_del_aliasmem(pipe_hnd, mem_ctx,
						   &group_sid, argv[1]);

		if (!NT_STATUS_IS_OK(result)) {
			d_fprintf(stderr, _("Could not del %s from %s: %s\n"),
				 argv[1], argv[0], nt_errstr(result));
		}
		return result;
	}

	d_fprintf(stderr, _("Can only delete members from global or local "
		 "groups which %s is not\n"), argv[0]);

	return NT_STATUS_UNSUCCESSFUL;
}

static int rpc_group_delmem(struct net_context *c, int argc, const char **argv)
{
	return run_rpc_command(c, NULL, &ndr_table_samr.syntax_id, 0,
			       rpc_group_delmem_internals,
			       argc, argv);
}

/**
 * List groups on a remote RPC server.
 *
 * All parameters are provided by the run_rpc_command function, except for
 * argc, argv which are passes through.
 *
 * @param domain_sid The domain sid acquired from the remote server.
 * @param cli A cli_state connected to the server.
 * @param mem_ctx Talloc context, destroyed on completion of the function.
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 *
 * @return Normal NTSTATUS return.
 **/

static NTSTATUS rpc_group_list_internals(struct net_context *c,
					const struct dom_sid *domain_sid,
					const char *domain_name,
					struct cli_state *cli,
					struct rpc_pipe_client *pipe_hnd,
					TALLOC_CTX *mem_ctx,
					int argc,
					const char **argv)
{
	struct policy_handle connect_pol, domain_pol;
	NTSTATUS status, result;
	uint32 start_idx=0, max_entries=250, num_entries, i, loop_count = 0;
	struct samr_SamArray *groups = NULL;
	bool global = false;
	bool local = false;
	bool builtin = false;
	struct dcerpc_binding_handle *b = pipe_hnd->binding_handle;

	if (c->display_usage) {
		d_printf("%s\n%s",
			 _("Usage:"),
			 _("net rpc group list [global] [local] [builtin]\n"
			   "  List groups on RPC server\n"
			   "    global\tList global groups\n"
			   "    local\tList local groups\n"
			   "    builtin\tList builtin groups\n"
			   "    If none of global, local or builtin is "
			   "specified, all three options are considered "
			   "set\n"));
		return NT_STATUS_OK;
	}

	if (argc == 0) {
		global = true;
		local = true;
		builtin = true;
	}

	for (i=0; i<argc; i++) {
		if (strequal(argv[i], "global"))
			global = true;

		if (strequal(argv[i], "local"))
			local = true;

		if (strequal(argv[i], "builtin"))
			builtin = true;
	}

	/* Get sam policy handle */

	status = dcerpc_samr_Connect2(b, mem_ctx,
				      pipe_hnd->desthost,
				      MAXIMUM_ALLOWED_ACCESS,
				      &connect_pol,
				      &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto done;
	}

	/* Get domain policy handle */

	status = dcerpc_samr_OpenDomain(b, mem_ctx,
					&connect_pol,
					MAXIMUM_ALLOWED_ACCESS,
					CONST_DISCARD(struct dom_sid2 *, domain_sid),
					&domain_pol,
					&result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto done;
	}

	/* Query domain groups */
	if (c->opt_long_list_entries)
		d_printf(_("\nGroup name            Comment"
			   "\n-----------------------------\n"));
	do {
		uint32_t max_size, total_size, returned_size;
		union samr_DispInfo info;

		if (!global) break;

		dcerpc_get_query_dispinfo_params(
			loop_count, &max_entries, &max_size);

		status = dcerpc_samr_QueryDisplayInfo(b, mem_ctx,
						      &domain_pol,
						      3,
						      start_idx,
						      max_entries,
						      max_size,
						      &total_size,
						      &returned_size,
						      &info,
						      &result);
		if (!NT_STATUS_IS_OK(status)) {
			goto done;
		}
		num_entries = info.info3.count;
		start_idx += info.info3.count;

		if (!NT_STATUS_IS_OK(result) &&
		    !NT_STATUS_EQUAL(result, STATUS_MORE_ENTRIES))
			break;

		for (i = 0; i < num_entries; i++) {

			const char *group = NULL;
			const char *desc = NULL;

			group = info.info3.entries[i].account_name.string;
			desc = info.info3.entries[i].description.string;

			if (c->opt_long_list_entries)
				printf("%-21.21s %-50.50s\n",
				       group, desc);
			else
				printf("%s\n", group);
		}
	} while (NT_STATUS_EQUAL(result, STATUS_MORE_ENTRIES));
	/* query domain aliases */
	start_idx = 0;
	do {
		if (!local) break;

		status = dcerpc_samr_EnumDomainAliases(b, mem_ctx,
						       &domain_pol,
						       &start_idx,
						       &groups,
						       0xffff,
						       &num_entries,
						       &result);
		if (!NT_STATUS_IS_OK(status)) {
			goto done;
		}
		if (!NT_STATUS_IS_OK(result) &&
		    !NT_STATUS_EQUAL(result, STATUS_MORE_ENTRIES))
			break;

		for (i = 0; i < num_entries; i++) {

			const char *description = NULL;

			if (c->opt_long_list_entries) {

				struct policy_handle alias_pol;
				union samr_AliasInfo *info = NULL;
				NTSTATUS _result;

				status = dcerpc_samr_OpenAlias(b, mem_ctx,
							       &domain_pol,
							       0x8,
							       groups->entries[i].idx,
							       &alias_pol,
							       &_result);
				if (NT_STATUS_IS_OK(status) && NT_STATUS_IS_OK(_result)) {
					status = dcerpc_samr_QueryAliasInfo(b, mem_ctx,
									    &alias_pol,
									    3,
									    &info,
									    &_result);
					if (NT_STATUS_IS_OK(status) && NT_STATUS_IS_OK(_result)) {
						status = dcerpc_samr_Close(b, mem_ctx,
									   &alias_pol,
									   &_result);
						if (NT_STATUS_IS_OK(status) && NT_STATUS_IS_OK(_result)) {
							description = info->description.string;
						}
					}
				}
			}

			if (description != NULL) {
				printf("%-21.21s %-50.50s\n",
				       groups->entries[i].name.string,
				       description);
			} else {
				printf("%s\n", groups->entries[i].name.string);
			}
		}
	} while (NT_STATUS_EQUAL(result, STATUS_MORE_ENTRIES));
	dcerpc_samr_Close(b, mem_ctx, &domain_pol, &result);
	/* Get builtin policy handle */

	status = dcerpc_samr_OpenDomain(b, mem_ctx,
					&connect_pol,
					MAXIMUM_ALLOWED_ACCESS,
					CONST_DISCARD(struct dom_sid2 *, &global_sid_Builtin),
					&domain_pol,
					&result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto done;
	}

	/* query builtin aliases */
	start_idx = 0;
	do {
		if (!builtin) break;

		status = dcerpc_samr_EnumDomainAliases(b, mem_ctx,
						       &domain_pol,
						       &start_idx,
						       &groups,
						       max_entries,
						       &num_entries,
						       &result);
		if (!NT_STATUS_IS_OK(status)) {
			break;
		}
		if (!NT_STATUS_IS_OK(result) &&
		    !NT_STATUS_EQUAL(result, STATUS_MORE_ENTRIES)) {
			status = result;
			break;
		}

		for (i = 0; i < num_entries; i++) {

			const char *description = NULL;

			if (c->opt_long_list_entries) {

				struct policy_handle alias_pol;
				union samr_AliasInfo *info = NULL;
				NTSTATUS _result;

				status = dcerpc_samr_OpenAlias(b, mem_ctx,
							       &domain_pol,
							       0x8,
							       groups->entries[i].idx,
							       &alias_pol,
							       &_result);
				if (NT_STATUS_IS_OK(status) && NT_STATUS_IS_OK(_result)) {
					status = dcerpc_samr_QueryAliasInfo(b, mem_ctx,
									    &alias_pol,
									    3,
									    &info,
									    &_result);
					if (NT_STATUS_IS_OK(status) && NT_STATUS_IS_OK(_result)) {
						status = dcerpc_samr_Close(b, mem_ctx,
									   &alias_pol,
									   &_result);
						if (NT_STATUS_IS_OK(status) && NT_STATUS_IS_OK(_result)) {
							description = info->description.string;
						}
					}
				}
			}

			if (description != NULL) {
				printf("%-21.21s %-50.50s\n",
				       groups->entries[i].name.string,
				       description);
			} else {
				printf("%s\n", groups->entries[i].name.string);
			}
		}
	} while (NT_STATUS_EQUAL(result, STATUS_MORE_ENTRIES));

	status = result;

 done:
	return status;
}

static int rpc_group_list(struct net_context *c, int argc, const char **argv)
{
	return run_rpc_command(c, NULL, &ndr_table_samr.syntax_id, 0,
			       rpc_group_list_internals,
			       argc, argv);
}

static NTSTATUS rpc_list_group_members(struct net_context *c,
					struct rpc_pipe_client *pipe_hnd,
					TALLOC_CTX *mem_ctx,
					const char *domain_name,
					const struct dom_sid *domain_sid,
					struct policy_handle *domain_pol,
					uint32 rid)
{
	NTSTATUS result, status;
	struct policy_handle group_pol;
	uint32 num_members, *group_rids;
	int i;
	struct samr_RidAttrArray *rids = NULL;
	struct lsa_Strings names;
	struct samr_Ids types;
	struct dcerpc_binding_handle *b = pipe_hnd->binding_handle;

	fstring sid_str;
	sid_to_fstring(sid_str, domain_sid);

	status = dcerpc_samr_OpenGroup(b, mem_ctx,
				       domain_pol,
				       MAXIMUM_ALLOWED_ACCESS,
				       rid,
				       &group_pol,
				       &result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if (!NT_STATUS_IS_OK(result)) {
		return result;
	}

	status = dcerpc_samr_QueryGroupMember(b, mem_ctx,
					      &group_pol,
					      &rids,
					      &result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if (!NT_STATUS_IS_OK(result)) {
		return result;
	}

	num_members = rids->count;
	group_rids = rids->rids;

	while (num_members > 0) {
		int this_time = 512;

		if (num_members < this_time)
			this_time = num_members;

		status = dcerpc_samr_LookupRids(b, mem_ctx,
						domain_pol,
						this_time,
						group_rids,
						&names,
						&types,
						&result);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
		if (!NT_STATUS_IS_OK(result)) {
			return result;
		}
		if (names.count != this_time) {
			return NT_STATUS_INVALID_NETWORK_RESPONSE;
		}
		if (types.count != this_time) {
			return NT_STATUS_INVALID_NETWORK_RESPONSE;
		}
		/* We only have users as members, but make the output
		   the same as the output of alias members */

		for (i = 0; i < this_time; i++) {

			if (c->opt_long_list_entries) {
				printf("%s-%d %s\\%s %d\n", sid_str,
				       group_rids[i], domain_name,
				       names.names[i].string,
				       SID_NAME_USER);
			} else {
				printf("%s\\%s\n", domain_name,
					names.names[i].string);
			}
		}

		num_members -= this_time;
		group_rids += 512;
	}

	return NT_STATUS_OK;
}

static NTSTATUS rpc_list_alias_members(struct net_context *c,
					struct rpc_pipe_client *pipe_hnd,
					TALLOC_CTX *mem_ctx,
					struct policy_handle *domain_pol,
					uint32 rid)
{
	NTSTATUS result, status;
	struct rpc_pipe_client *lsa_pipe;
	struct policy_handle alias_pol, lsa_pol;
	uint32 num_members;
	struct dom_sid *alias_sids;
	char **domains;
	char **names;
	enum lsa_SidType *types;
	int i;
	struct lsa_SidArray sid_array;
	struct dcerpc_binding_handle *b = pipe_hnd->binding_handle;

	status = dcerpc_samr_OpenAlias(b, mem_ctx,
				       domain_pol,
				       MAXIMUM_ALLOWED_ACCESS,
				       rid,
				       &alias_pol,
				       &result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if (!NT_STATUS_IS_OK(result)) {
		return result;
	}

	status = dcerpc_samr_GetMembersInAlias(b, mem_ctx,
					       &alias_pol,
					       &sid_array,
					       &result);
	if (!NT_STATUS_IS_OK(status)) {
		d_fprintf(stderr, _("Couldn't list alias members\n"));
		return status;
	}
	if (!NT_STATUS_IS_OK(result)) {
		d_fprintf(stderr, _("Couldn't list alias members\n"));
		return result;
	}

	num_members = sid_array.num_sids;

	if (num_members == 0) {
		return NT_STATUS_OK;
	}

	result = cli_rpc_pipe_open_noauth(rpc_pipe_np_smb_conn(pipe_hnd),
					  &ndr_table_lsarpc.syntax_id,
					  &lsa_pipe);
	if (!NT_STATUS_IS_OK(result)) {
		d_fprintf(stderr, _("Couldn't open LSA pipe. Error was %s\n"),
			nt_errstr(result) );
		return result;
	}

	result = rpccli_lsa_open_policy(lsa_pipe, mem_ctx, true,
				     SEC_FLAG_MAXIMUM_ALLOWED, &lsa_pol);

	if (!NT_STATUS_IS_OK(result)) {
		d_fprintf(stderr, _("Couldn't open LSA policy handle\n"));
		TALLOC_FREE(lsa_pipe);
		return result;
	}

	alias_sids = TALLOC_ZERO_ARRAY(mem_ctx, struct dom_sid, num_members);
	if (!alias_sids) {
		d_fprintf(stderr, _("Out of memory\n"));
		TALLOC_FREE(lsa_pipe);
		return NT_STATUS_NO_MEMORY;
	}

	for (i=0; i<num_members; i++) {
		sid_copy(&alias_sids[i], sid_array.sids[i].sid);
	}

	result = rpccli_lsa_lookup_sids(lsa_pipe, mem_ctx, &lsa_pol,
				     num_members,  alias_sids,
				     &domains, &names, &types);

	if (!NT_STATUS_IS_OK(result) &&
	    !NT_STATUS_EQUAL(result, STATUS_SOME_UNMAPPED)) {
		d_fprintf(stderr, _("Couldn't lookup SIDs\n"));
		TALLOC_FREE(lsa_pipe);
		return result;
	}

	for (i = 0; i < num_members; i++) {
		fstring sid_str;
		sid_to_fstring(sid_str, &alias_sids[i]);

		if (c->opt_long_list_entries) {
			printf("%s %s\\%s %d\n", sid_str,
			       domains[i] ? domains[i] : _("*unknown*"),
			       names[i] ? names[i] : _("*unknown*"), types[i]);
		} else {
			if (domains[i])
				printf("%s\\%s\n", domains[i], names[i]);
			else
				printf("%s\n", sid_str);
		}
	}

	TALLOC_FREE(lsa_pipe);
	return NT_STATUS_OK;
}

static NTSTATUS rpc_group_members_internals(struct net_context *c,
					const struct dom_sid *domain_sid,
					const char *domain_name,
					struct cli_state *cli,
					struct rpc_pipe_client *pipe_hnd,
					TALLOC_CTX *mem_ctx,
					int argc,
					const char **argv)
{
	NTSTATUS result, status;
	struct policy_handle connect_pol, domain_pol;
	struct samr_Ids rids, rid_types;
	struct lsa_String lsa_acct_name;
	struct dcerpc_binding_handle *b = pipe_hnd->binding_handle;

	/* Get sam policy handle */

	status = dcerpc_samr_Connect2(b, mem_ctx,
				      pipe_hnd->desthost,
				      MAXIMUM_ALLOWED_ACCESS,
				      &connect_pol,
				      &result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if (!NT_STATUS_IS_OK(result)) {
		return result;
	}

	/* Get domain policy handle */

	status = dcerpc_samr_OpenDomain(b, mem_ctx,
					&connect_pol,
					MAXIMUM_ALLOWED_ACCESS,
					CONST_DISCARD(struct dom_sid2 *, domain_sid),
					&domain_pol,
					&result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if (!NT_STATUS_IS_OK(result)) {
		return result;
	}

	init_lsa_String(&lsa_acct_name, argv[0]); /* sure? */

	status = dcerpc_samr_LookupNames(b, mem_ctx,
					 &domain_pol,
					 1,
					 &lsa_acct_name,
					 &rids,
					 &rid_types,
					 &result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (!NT_STATUS_IS_OK(result)) {

		/* Ok, did not find it in the global sam, try with builtin */

		struct dom_sid sid_Builtin;

		dcerpc_samr_Close(b, mem_ctx, &domain_pol, &result);

		sid_copy(&sid_Builtin, &global_sid_Builtin);

		status = dcerpc_samr_OpenDomain(b, mem_ctx,
						&connect_pol,
						MAXIMUM_ALLOWED_ACCESS,
						&sid_Builtin,
						&domain_pol,
						&result);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
		if (!NT_STATUS_IS_OK(result)) {
			d_fprintf(stderr, _("Couldn't find group %s\n"),
				  argv[0]);
			return result;
		}

		status = dcerpc_samr_LookupNames(b, mem_ctx,
						 &domain_pol,
						 1,
						 &lsa_acct_name,
						 &rids,
						 &rid_types,
						 &result);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
		if (!NT_STATUS_IS_OK(result)) {
			d_fprintf(stderr, _("Couldn't find group %s\n"),
				  argv[0]);
			return result;
		}
	}

	if (rids.count != 1) {
		d_fprintf(stderr, _("Couldn't find group %s\n"),
			  argv[0]);
		return NT_STATUS_INVALID_NETWORK_RESPONSE;
	}
	if (rid_types.count != 1) {
		d_fprintf(stderr, _("Couldn't find group %s\n"),
			  argv[0]);
		return NT_STATUS_INVALID_NETWORK_RESPONSE;
	}


	if (rid_types.ids[0] == SID_NAME_DOM_GRP) {
		return rpc_list_group_members(c, pipe_hnd, mem_ctx, domain_name,
					      domain_sid, &domain_pol,
					      rids.ids[0]);
	}

	if (rid_types.ids[0] == SID_NAME_ALIAS) {
		return rpc_list_alias_members(c, pipe_hnd, mem_ctx, &domain_pol,
					      rids.ids[0]);
	}

	return NT_STATUS_NO_SUCH_GROUP;
}

static int rpc_group_members(struct net_context *c, int argc, const char **argv)
{
	if (argc != 1 || c->display_usage) {
		return rpc_group_usage(c, argc, argv);
	}

	return run_rpc_command(c, NULL, &ndr_table_samr.syntax_id, 0,
			       rpc_group_members_internals,
			       argc, argv);
}

static int rpc_group_rename_internals(struct net_context *c, int argc, const char **argv)
{
	NET_API_STATUS status;
	struct GROUP_INFO_0 g0;
	uint32_t parm_err;

	if (argc != 2) {
		d_printf(_("Usage:\n"));
		d_printf("net rpc group rename group newname\n");
		return -1;
	}

	g0.grpi0_name = argv[1];

	status = NetGroupSetInfo(c->opt_host,
				 argv[0],
				 0,
				 (uint8_t *)&g0,
				 &parm_err);

	if (status != 0) {
		d_fprintf(stderr, _("Renaming group %s failed with: %s\n"),
			argv[0], libnetapi_get_error_string(c->netapi_ctx,
			status));
		return -1;
	}

	return 0;
}

static int rpc_group_rename(struct net_context *c, int argc, const char **argv)
{
	if (argc != 2 || c->display_usage) {
		return rpc_group_usage(c, argc, argv);
	}

	return rpc_group_rename_internals(c, argc, argv);
}

/**
 * 'net rpc group' entrypoint.
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 **/

int net_rpc_group(struct net_context *c, int argc, const char **argv)
{
	NET_API_STATUS status;

	struct functable func[] = {
		{
			"add",
			rpc_group_add,
			NET_TRANSPORT_RPC,
			N_("Create specified group"),
			N_("net rpc group add\n"
			   "    Create specified group")
		},
		{
			"delete",
			rpc_group_delete,
			NET_TRANSPORT_RPC,
			N_("Delete specified group"),
			N_("net rpc group delete\n"
			   "    Delete specified group")
		},
		{
			"addmem",
			rpc_group_addmem,
			NET_TRANSPORT_RPC,
			N_("Add member to group"),
			N_("net rpc group addmem\n"
			   "    Add member to group")
		},
		{
			"delmem",
			rpc_group_delmem,
			NET_TRANSPORT_RPC,
			N_("Remove member from group"),
			N_("net rpc group delmem\n"
			   "    Remove member from group")
		},
		{
			"list",
			rpc_group_list,
			NET_TRANSPORT_RPC,
			N_("List groups"),
			N_("net rpc group list\n"
			   "    List groups")
		},
		{
			"members",
			rpc_group_members,
			NET_TRANSPORT_RPC,
			N_("List group members"),
			N_("net rpc group members\n"
			   "    List group members")
		},
		{
			"rename",
			rpc_group_rename,
			NET_TRANSPORT_RPC,
			N_("Rename group"),
			N_("net rpc group rename\n"
			   "    Rename group")
		},
		{NULL, NULL, 0, NULL, NULL}
	};

	status = libnetapi_net_init(&c->netapi_ctx);
	if (status != 0) {
		return -1;
	}
	libnetapi_set_username(c->netapi_ctx, c->opt_user_name);
	libnetapi_set_password(c->netapi_ctx, c->opt_password);
	if (c->opt_kerberos) {
		libnetapi_set_use_kerberos(c->netapi_ctx);
	}

	if (argc == 0) {
		if (c->display_usage) {
			d_printf(_("Usage:\n"));
			d_printf(_("net rpc group\n"
				   "    Alias for net rpc group list global "
				   "local builtin\n"));
			net_display_usage_from_functable(func);
			return 0;
		}

		return run_rpc_command(c, NULL, &ndr_table_samr.syntax_id, 0,
				       rpc_group_list_internals,
				       argc, argv);
	}

	return net_run_function(c, argc, argv, "net rpc group", func);
}

/****************************************************************************/

static int rpc_share_usage(struct net_context *c, int argc, const char **argv)
{
	return net_share_usage(c, argc, argv);
}

/**
 * Add a share on a remote RPC server.
 *
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 *
 * @return A shell status integer (0 for success).
 **/

static int rpc_share_add(struct net_context *c, int argc, const char **argv)
{
	NET_API_STATUS status;
	char *sharename;
	char *path;
	uint32 type = STYPE_DISKTREE; /* only allow disk shares to be added */
	uint32 num_users=0, perms=0;
	char *password=NULL; /* don't allow a share password */
	struct SHARE_INFO_2 i2;
	uint32_t parm_error = 0;

	if ((argc < 1) || !strchr(argv[0], '=') || c->display_usage) {
		return rpc_share_usage(c, argc, argv);
	}

	if ((sharename = talloc_strdup(c, argv[0])) == NULL) {
		return -1;
	}

	path = strchr(sharename, '=');
	if (!path) {
		return -1;
	}

	*path++ = '\0';

	i2.shi2_netname		= sharename;
	i2.shi2_type		= type;
	i2.shi2_remark		= c->opt_comment;
	i2.shi2_permissions	= perms;
	i2.shi2_max_uses	= c->opt_maxusers;
	i2.shi2_current_uses	= num_users;
	i2.shi2_path		= path;
	i2.shi2_passwd		= password;

	status = NetShareAdd(c->opt_host,
			     2,
			     (uint8_t *)&i2,
			     &parm_error);
	if (status != 0) {
		printf(_("NetShareAdd failed with: %s\n"),
			libnetapi_get_error_string(c->netapi_ctx, status));
	}

	return status;
}

/**
 * Delete a share on a remote RPC server.
 *
 * @param domain_sid The domain sid acquired from the remote server.
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 *
 * @return A shell status integer (0 for success).
 **/
static int rpc_share_delete(struct net_context *c, int argc, const char **argv)
{
	if (argc < 1 || c->display_usage) {
		return rpc_share_usage(c, argc, argv);
	}

	return NetShareDel(c->opt_host, argv[0], 0);
}

/**
 * Formatted print of share info
 *
 * @param r  pointer to SHARE_INFO_1 to format
 **/

static void display_share_info_1(struct net_context *c,
				 struct SHARE_INFO_1 *r)
{
	if (c->opt_long_list_entries) {
		d_printf("%-12s %-8.8s %-50s\n",
			 r->shi1_netname,
			 net_share_type_str(r->shi1_type & ~(STYPE_TEMPORARY|STYPE_HIDDEN)),
			 r->shi1_remark);
	} else {
		d_printf("%s\n", r->shi1_netname);
	}
}

static WERROR get_share_info(struct net_context *c,
			     struct rpc_pipe_client *pipe_hnd,
			     TALLOC_CTX *mem_ctx,
			     uint32 level,
			     int argc,
			     const char **argv,
			     struct srvsvc_NetShareInfoCtr *info_ctr)
{
	WERROR result;
	NTSTATUS status;
	union srvsvc_NetShareInfo info;
	struct dcerpc_binding_handle *b = pipe_hnd->binding_handle;

	/* no specific share requested, enumerate all */
	if (argc == 0) {

		uint32_t preferred_len = 0xffffffff;
		uint32_t total_entries = 0;
		uint32_t resume_handle = 0;

		info_ctr->level = level;

		status = dcerpc_srvsvc_NetShareEnumAll(b, mem_ctx,
						       pipe_hnd->desthost,
						       info_ctr,
						       preferred_len,
						       &total_entries,
						       &resume_handle,
						       &result);
		if (!NT_STATUS_IS_OK(status)) {
			return ntstatus_to_werror(status);
		}
		return result;
	}

	/* request just one share */
	status = dcerpc_srvsvc_NetShareGetInfo(b, mem_ctx,
					       pipe_hnd->desthost,
					       argv[0],
					       level,
					       &info,
					       &result);

	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
		goto done;
	}

	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	/* construct ctr */
	ZERO_STRUCTP(info_ctr);

	info_ctr->level = level;

	switch (level) {
	case 1:
	{
		struct srvsvc_NetShareCtr1 *ctr1;

		ctr1 = TALLOC_ZERO_P(mem_ctx, struct srvsvc_NetShareCtr1);
		W_ERROR_HAVE_NO_MEMORY(ctr1);

		ctr1->count = 1;
		ctr1->array = info.info1;

		info_ctr->ctr.ctr1 = ctr1;

		break;
	}
	case 2:
	{
		struct srvsvc_NetShareCtr2 *ctr2;

		ctr2 = TALLOC_ZERO_P(mem_ctx, struct srvsvc_NetShareCtr2);
		W_ERROR_HAVE_NO_MEMORY(ctr2);

		ctr2->count = 1;
		ctr2->array = info.info2;

		info_ctr->ctr.ctr2 = ctr2;

		break;
	}
	case 502:
	{
		struct srvsvc_NetShareCtr502 *ctr502;

		ctr502 = TALLOC_ZERO_P(mem_ctx, struct srvsvc_NetShareCtr502);
		W_ERROR_HAVE_NO_MEMORY(ctr502);

		ctr502->count = 1;
		ctr502->array = info.info502;

		info_ctr->ctr.ctr502 = ctr502;

		break;
	}
	} /* switch */
done:
	return result;
}

/***
 * 'net rpc share list' entrypoint.
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 **/
static int rpc_share_list(struct net_context *c, int argc, const char **argv)
{
	NET_API_STATUS status;
	struct SHARE_INFO_1 *i1 = NULL;
	uint32_t entries_read = 0;
	uint32_t total_entries = 0;
	uint32_t resume_handle = 0;
	uint32_t i, level = 1;

	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net rpc share list\n"
			   "    %s\n",
			 _("Usage:"),
			 _("List shares on remote server"));
		return 0;
	}

	status = NetShareEnum(c->opt_host,
			      level,
			      (uint8_t **)(void *)&i1,
			      (uint32_t)-1,
			      &entries_read,
			      &total_entries,
			      &resume_handle);
	if (status != 0) {
		goto done;
	}

	/* Display results */

	if (c->opt_long_list_entries) {
		d_printf(_(
	"\nEnumerating shared resources (exports) on remote server:\n\n"
	"\nShare name   Type     Description\n"
	"----------   ----     -----------\n"));
	}
	for (i = 0; i < entries_read; i++)
		display_share_info_1(c, &i1[i]);
 done:
	return status;
}

static bool check_share_availability(struct cli_state *cli, const char *netname)
{
	NTSTATUS status;

	status = cli_tcon_andx(cli, netname, "A:", "", 0);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf(_("skipping   [%s]: not a file share.\n"), netname);
		return false;
	}

	status = cli_tdis(cli);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf(_("cli_tdis returned %s\n"), nt_errstr(status));
		return false;
	}

	return true;
}

static bool check_share_sanity(struct net_context *c, struct cli_state *cli,
			       const char *netname, uint32 type)
{
	/* only support disk shares */
	if (! ( type == STYPE_DISKTREE || type == (STYPE_DISKTREE | STYPE_HIDDEN)) ) {
		printf(_("share [%s] is not a diskshare (type: %x)\n"), netname,
		       type);
		return false;
	}

	/* skip builtin shares */
	/* FIXME: should print$ be added too ? */
	if (strequal(netname,"IPC$") || strequal(netname,"ADMIN$") ||
	    strequal(netname,"global"))
		return false;

	if (c->opt_exclude && in_list(netname, c->opt_exclude, false)) {
		printf(_("excluding  [%s]\n"), netname);
		return false;
	}

	return check_share_availability(cli, netname);
}

/**
 * Migrate shares from a remote RPC server to the local RPC server.
 *
 * All parameters are provided by the run_rpc_command function, except for
 * argc, argv which are passed through.
 *
 * @param domain_sid The domain sid acquired from the remote server.
 * @param cli A cli_state connected to the server.
 * @param mem_ctx Talloc context, destroyed on completion of the function.
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 *
 * @return Normal NTSTATUS return.
 **/

static NTSTATUS rpc_share_migrate_shares_internals(struct net_context *c,
						const struct dom_sid *domain_sid,
						const char *domain_name,
						struct cli_state *cli,
						struct rpc_pipe_client *pipe_hnd,
						TALLOC_CTX *mem_ctx,
						int argc,
						const char **argv)
{
	WERROR result;
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;
	struct srvsvc_NetShareInfoCtr ctr_src;
	uint32 i;
	struct rpc_pipe_client *srvsvc_pipe = NULL;
	struct cli_state *cli_dst = NULL;
	uint32 level = 502; /* includes secdesc */
	uint32_t parm_error = 0;
	struct dcerpc_binding_handle *b;

	result = get_share_info(c, pipe_hnd, mem_ctx, level, argc, argv,
				&ctr_src);
	if (!W_ERROR_IS_OK(result))
		goto done;

	/* connect destination PI_SRVSVC */
        nt_status = connect_dst_pipe(c, &cli_dst, &srvsvc_pipe,
				     &ndr_table_srvsvc.syntax_id);
        if (!NT_STATUS_IS_OK(nt_status))
                return nt_status;

	b = srvsvc_pipe->binding_handle;

	for (i = 0; i < ctr_src.ctr.ctr502->count; i++) {

		union srvsvc_NetShareInfo info;
		struct srvsvc_NetShareInfo502 info502 =
			ctr_src.ctr.ctr502->array[i];

		/* reset error-code */
		nt_status = NT_STATUS_UNSUCCESSFUL;

		if (!check_share_sanity(c, cli, info502.name, info502.type))
			continue;

		/* finally add the share on the dst server */

		printf(_("migrating: [%s], path: %s, comment: %s, without "
			 "share-ACLs\n"),
			info502.name, info502.path, info502.comment);

		info.info502 = &info502;

		nt_status = dcerpc_srvsvc_NetShareAdd(b, mem_ctx,
						      srvsvc_pipe->desthost,
						      502,
						      &info,
						      &parm_error,
						      &result);
		if (!NT_STATUS_IS_OK(nt_status)) {
			printf(_("cannot add share: %s\n"),
				nt_errstr(nt_status));
			goto done;
		}
                if (W_ERROR_V(result) == W_ERROR_V(WERR_FILE_EXISTS)) {
			printf(_("           [%s] does already exist\n"),
				info502.name);
			continue;
		}

		if (!W_ERROR_IS_OK(result)) {
			nt_status = werror_to_ntstatus(result);
			printf(_("cannot add share: %s\n"),
				win_errstr(result));
			goto done;
		}

	}

	nt_status = NT_STATUS_OK;

done:
	if (cli_dst) {
		cli_shutdown(cli_dst);
	}

	return nt_status;

}

/**
 * Migrate shares from a RPC server to another.
 *
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 *
 * @return A shell status integer (0 for success).
 **/
static int rpc_share_migrate_shares(struct net_context *c, int argc,
				    const char **argv)
{
	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net rpc share migrate shares\n"
			   "    %s\n",
			 _("Usage:"),
			 _("Migrate shares to local server"));
		return 0;
	}

	if (!c->opt_host) {
		printf(_("no server to migrate\n"));
		return -1;
	}

	return run_rpc_command(c, NULL, &ndr_table_srvsvc.syntax_id, 0,
			       rpc_share_migrate_shares_internals,
			       argc, argv);
}

/**
 * Copy a file/dir
 *
 * @param f	file_info
 * @param mask	current search mask
 * @param state	arg-pointer
 *
 **/
static NTSTATUS copy_fn(const char *mnt, struct file_info *f,
		    const char *mask, void *state)
{
	static NTSTATUS nt_status;
	static struct copy_clistate *local_state;
	static fstring filename, new_mask;
	fstring dir;
	char *old_dir;
	struct net_context *c;

	local_state = (struct copy_clistate *)state;
	nt_status = NT_STATUS_UNSUCCESSFUL;

	c = local_state->c;

	if (strequal(f->name, ".") || strequal(f->name, ".."))
		return NT_STATUS_OK;

	DEBUG(3,("got mask: %s, name: %s\n", mask, f->name));

	/* DIRECTORY */
	if (f->mode & FILE_ATTRIBUTE_DIRECTORY) {

		DEBUG(3,("got dir: %s\n", f->name));

		fstrcpy(dir, local_state->cwd);
		fstrcat(dir, "\\");
		fstrcat(dir, f->name);

		switch (net_mode_share)
		{
		case NET_MODE_SHARE_MIGRATE:
			/* create that directory */
			nt_status = net_copy_file(c, local_state->mem_ctx,
						  local_state->cli_share_src,
						  local_state->cli_share_dst,
						  dir, dir,
						  c->opt_acls? true : false,
						  c->opt_attrs? true : false,
						  c->opt_timestamps? true:false,
						  false);
			break;
		default:
			d_fprintf(stderr, _("Unsupported mode %d\n"), net_mode_share);
			return NT_STATUS_INTERNAL_ERROR;
		}

		if (!NT_STATUS_IS_OK(nt_status)) {
			printf(_("could not handle dir %s: %s\n"),
				dir, nt_errstr(nt_status));
			return nt_status;
		}

		/* search below that directory */
		fstrcpy(new_mask, dir);
		fstrcat(new_mask, "\\*");

		old_dir = local_state->cwd;
		local_state->cwd = dir;
		nt_status = sync_files(local_state, new_mask);
		if (!NT_STATUS_IS_OK(nt_status)) {
			printf(_("could not handle files\n"));
		}
		local_state->cwd = old_dir;

		return nt_status;
	}


	/* FILE */
	fstrcpy(filename, local_state->cwd);
	fstrcat(filename, "\\");
	fstrcat(filename, f->name);

	DEBUG(3,("got file: %s\n", filename));

	switch (net_mode_share)
	{
	case NET_MODE_SHARE_MIGRATE:
		nt_status = net_copy_file(c, local_state->mem_ctx,
					  local_state->cli_share_src,
					  local_state->cli_share_dst,
					  filename, filename,
					  c->opt_acls? true : false,
					  c->opt_attrs? true : false,
					  c->opt_timestamps? true: false,
					  true);
		break;
	default:
		d_fprintf(stderr, _("Unsupported file mode %d\n"),
			  net_mode_share);
		return NT_STATUS_INTERNAL_ERROR;
	}

	if (!NT_STATUS_IS_OK(nt_status))
		printf(_("could not handle file %s: %s\n"),
			filename, nt_errstr(nt_status));
	return nt_status;
}

/**
 * sync files, can be called recursivly to list files
 * and then call copy_fn for each file
 *
 * @param cp_clistate	pointer to the copy_clistate we work with
 * @param mask		the current search mask
 *
 * @return 		Boolean result
 **/
static NTSTATUS sync_files(struct copy_clistate *cp_clistate, const char *mask)
{
	struct cli_state *targetcli;
	char *targetpath = NULL;
	NTSTATUS status;

	DEBUG(3,("calling cli_list with mask: %s\n", mask));

	if ( !cli_resolve_path(talloc_tos(), "", NULL, cp_clistate->cli_share_src,
				mask, &targetcli, &targetpath ) ) {
		d_fprintf(stderr, _("cli_resolve_path %s failed with error: "
				    "%s\n"),
			mask, cli_errstr(cp_clistate->cli_share_src));
		return cli_nt_error(cp_clistate->cli_share_src);
	}

	status = cli_list(targetcli, targetpath, cp_clistate->attribute,
			  copy_fn, cp_clistate);
	if (!NT_STATUS_IS_OK(status)) {
		d_fprintf(stderr, _("listing %s failed with error: %s\n"),
			  mask, nt_errstr(status));
	}

	return status;
}


/**
 * Set the top level directory permissions before we do any further copies.
 * Should set up ACL inheritance.
 **/

bool copy_top_level_perms(struct net_context *c,
				struct copy_clistate *cp_clistate,
				const char *sharename)
{
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;

	switch (net_mode_share) {
	case NET_MODE_SHARE_MIGRATE:
		DEBUG(3,("calling net_copy_fileattr for '.' directory in share %s\n", sharename));
		nt_status = net_copy_fileattr(c,
						cp_clistate->mem_ctx,
						cp_clistate->cli_share_src,
						cp_clistate->cli_share_dst,
						"\\", "\\",
						c->opt_acls? true : false,
						c->opt_attrs? true : false,
						c->opt_timestamps? true: false,
						false);
		break;
	default:
		d_fprintf(stderr, _("Unsupported mode %d\n"), net_mode_share);
		break;
	}

	if (!NT_STATUS_IS_OK(nt_status))  {
		printf(_("Could handle directory attributes for top level "
			 "directory of share %s. Error %s\n"),
			sharename, nt_errstr(nt_status));
		return false;
	}

	return true;
}

/**
 * Sync all files inside a remote share to another share (over smb).
 *
 * All parameters are provided by the run_rpc_command function, except for
 * argc, argv which are passed through.
 *
 * @param domain_sid The domain sid acquired from the remote server.
 * @param cli A cli_state connected to the server.
 * @param mem_ctx Talloc context, destroyed on completion of the function.
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 *
 * @return Normal NTSTATUS return.
 **/

static NTSTATUS rpc_share_migrate_files_internals(struct net_context *c,
						const struct dom_sid *domain_sid,
						const char *domain_name,
						struct cli_state *cli,
						struct rpc_pipe_client *pipe_hnd,
						TALLOC_CTX *mem_ctx,
						int argc,
						const char **argv)
{
	WERROR result;
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;
	struct srvsvc_NetShareInfoCtr ctr_src;
	uint32 i;
	uint32 level = 502;
	struct copy_clistate cp_clistate;
	bool got_src_share = false;
	bool got_dst_share = false;
	const char *mask = "\\*";
	char *dst = NULL;

	dst = SMB_STRDUP(c->opt_destination?c->opt_destination:"127.0.0.1");
	if (dst == NULL) {
		nt_status = NT_STATUS_NO_MEMORY;
		goto done;
	}

	result = get_share_info(c, pipe_hnd, mem_ctx, level, argc, argv,
				&ctr_src);

	if (!W_ERROR_IS_OK(result))
		goto done;

	for (i = 0; i < ctr_src.ctr.ctr502->count; i++) {

		struct srvsvc_NetShareInfo502 info502 =
			ctr_src.ctr.ctr502->array[i];

		if (!check_share_sanity(c, cli, info502.name, info502.type))
			continue;

		/* one might not want to mirror whole discs :) */
		if (strequal(info502.name, "print$") || info502.name[1] == '$') {
			d_printf(_("skipping   [%s]: builtin/hidden share\n"),
				 info502.name);
			continue;
		}

		switch (net_mode_share)
		{
		case NET_MODE_SHARE_MIGRATE:
			printf("syncing");
			break;
		default:
			d_fprintf(stderr, _("Unsupported mode %d\n"),
				  net_mode_share);
			break;
		}
		printf(_("    [%s] files and directories %s ACLs, %s DOS "
			 "Attributes %s\n"),
			info502.name,
			c->opt_acls ? _("including") : _("without"),
			c->opt_attrs ? _("including") : _("without"),
			c->opt_timestamps ? _("(preserving timestamps)") : "");

		cp_clistate.mem_ctx = mem_ctx;
		cp_clistate.cli_share_src = NULL;
		cp_clistate.cli_share_dst = NULL;
		cp_clistate.cwd = NULL;
		cp_clistate.attribute = FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_DIRECTORY;
		cp_clistate.c = c;

	        /* open share source */
		nt_status = connect_to_service(c, &cp_clistate.cli_share_src,
					       &cli->dest_ss, cli->desthost,
					       info502.name, "A:");
		if (!NT_STATUS_IS_OK(nt_status))
			goto done;

		got_src_share = true;

		if (net_mode_share == NET_MODE_SHARE_MIGRATE) {
			/* open share destination */
			nt_status = connect_to_service(c, &cp_clistate.cli_share_dst,
						       NULL, dst, info502.name, "A:");
			if (!NT_STATUS_IS_OK(nt_status))
				goto done;

			got_dst_share = true;
		}

		if (!copy_top_level_perms(c, &cp_clistate, info502.name)) {
			d_fprintf(stderr, _("Could not handle the top level "
					    "directory permissions for the "
					    "share: %s\n"), info502.name);
			nt_status = NT_STATUS_UNSUCCESSFUL;
			goto done;
		}

		nt_status = sync_files(&cp_clistate, mask);
		if (!NT_STATUS_IS_OK(nt_status)) {
			d_fprintf(stderr, _("could not handle files for share: "
					    "%s\n"), info502.name);
			goto done;
		}
	}

	nt_status = NT_STATUS_OK;

done:

	if (got_src_share)
		cli_shutdown(cp_clistate.cli_share_src);

	if (got_dst_share)
		cli_shutdown(cp_clistate.cli_share_dst);

	SAFE_FREE(dst);
	return nt_status;

}

static int rpc_share_migrate_files(struct net_context *c, int argc, const char **argv)
{
	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net share migrate files\n"
			   "    %s\n",
			 _("Usage:"),
			 _("Migrate files to local server"));
		return 0;
	}

	if (!c->opt_host) {
		d_printf(_("no server to migrate\n"));
		return -1;
	}

	return run_rpc_command(c, NULL, &ndr_table_srvsvc.syntax_id, 0,
			       rpc_share_migrate_files_internals,
			       argc, argv);
}

/**
 * Migrate share-ACLs from a remote RPC server to the local RPC server.
 *
 * All parameters are provided by the run_rpc_command function, except for
 * argc, argv which are passed through.
 *
 * @param domain_sid The domain sid acquired from the remote server.
 * @param cli A cli_state connected to the server.
 * @param mem_ctx Talloc context, destroyed on completion of the function.
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 *
 * @return Normal NTSTATUS return.
 **/

static NTSTATUS rpc_share_migrate_security_internals(struct net_context *c,
						const struct dom_sid *domain_sid,
						const char *domain_name,
						struct cli_state *cli,
						struct rpc_pipe_client *pipe_hnd,
						TALLOC_CTX *mem_ctx,
						int argc,
						const char **argv)
{
	WERROR result;
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;
	struct srvsvc_NetShareInfoCtr ctr_src;
	union srvsvc_NetShareInfo info;
	uint32 i;
	struct rpc_pipe_client *srvsvc_pipe = NULL;
	struct cli_state *cli_dst = NULL;
	uint32 level = 502; /* includes secdesc */
	uint32_t parm_error = 0;
	struct dcerpc_binding_handle *b;

	result = get_share_info(c, pipe_hnd, mem_ctx, level, argc, argv,
				&ctr_src);

	if (!W_ERROR_IS_OK(result))
		goto done;

	/* connect destination PI_SRVSVC */
        nt_status = connect_dst_pipe(c, &cli_dst, &srvsvc_pipe,
				     &ndr_table_srvsvc.syntax_id);
        if (!NT_STATUS_IS_OK(nt_status))
                return nt_status;

	b = srvsvc_pipe->binding_handle;

	for (i = 0; i < ctr_src.ctr.ctr502->count; i++) {

		struct srvsvc_NetShareInfo502 info502 =
			ctr_src.ctr.ctr502->array[i];

		/* reset error-code */
		nt_status = NT_STATUS_UNSUCCESSFUL;

		if (!check_share_sanity(c, cli, info502.name, info502.type))
			continue;

		printf(_("migrating: [%s], path: %s, comment: %s, including "
			 "share-ACLs\n"),
			info502.name, info502.path, info502.comment);

		if (c->opt_verbose)
			display_sec_desc(info502.sd_buf.sd);

		/* FIXME: shouldn't we be able to just set the security descriptor ? */
		info.info502 = &info502;

		/* finally modify the share on the dst server */
		nt_status = dcerpc_srvsvc_NetShareSetInfo(b, mem_ctx,
							  srvsvc_pipe->desthost,
							  info502.name,
							  level,
							  &info,
							  &parm_error,
							  &result);
		if (!NT_STATUS_IS_OK(nt_status)) {
			printf(_("cannot set share-acl: %s\n"),
			       nt_errstr(nt_status));
			goto done;
		}
		if (!W_ERROR_IS_OK(result)) {
			nt_status = werror_to_ntstatus(result);
			printf(_("cannot set share-acl: %s\n"),
			       win_errstr(result));
			goto done;
		}

	}

	nt_status = NT_STATUS_OK;

done:
	if (cli_dst) {
		cli_shutdown(cli_dst);
	}

	return nt_status;

}

/**
 * Migrate share-acls from a RPC server to another.
 *
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 *
 * @return A shell status integer (0 for success).
 **/
static int rpc_share_migrate_security(struct net_context *c, int argc,
				      const char **argv)
{
	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net rpc share migrate security\n"
			   "    %s\n",
			 _("Usage:"),
			 _("Migrate share-acls to local server"));
		return 0;
	}

	if (!c->opt_host) {
		d_printf(_("no server to migrate\n"));
		return -1;
	}

	return run_rpc_command(c, NULL, &ndr_table_srvsvc.syntax_id, 0,
			       rpc_share_migrate_security_internals,
			       argc, argv);
}

/**
 * Migrate shares (including share-definitions, share-acls and files with acls/attrs)
 * from one server to another.
 *
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 *
 * @return A shell status integer (0 for success).
 *
 **/
static int rpc_share_migrate_all(struct net_context *c, int argc,
				 const char **argv)
{
	int ret;

	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net rpc share migrate all\n"
			   "    %s\n",
			 _("Usage:"),
			 _("Migrates shares including all share settings"));
		return 0;
	}

	if (!c->opt_host) {
		d_printf(_("no server to migrate\n"));
		return -1;
	}

	/* order is important. we don't want to be locked out by the share-acl
	 * before copying files - gd */

	ret = run_rpc_command(c, NULL, &ndr_table_srvsvc.syntax_id, 0,
			      rpc_share_migrate_shares_internals, argc, argv);
	if (ret)
		return ret;

	ret = run_rpc_command(c, NULL, &ndr_table_srvsvc.syntax_id, 0,
			      rpc_share_migrate_files_internals, argc, argv);
	if (ret)
		return ret;

	return run_rpc_command(c, NULL, &ndr_table_srvsvc.syntax_id, 0,
			       rpc_share_migrate_security_internals, argc,
			       argv);
}


/**
 * 'net rpc share migrate' entrypoint.
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 **/
static int rpc_share_migrate(struct net_context *c, int argc, const char **argv)
{

	struct functable func[] = {
		{
			"all",
			rpc_share_migrate_all,
			NET_TRANSPORT_RPC,
			N_("Migrate shares from remote to local server"),
			N_("net rpc share migrate all\n"
			   "    Migrate shares from remote to local server")
		},
		{
			"files",
			rpc_share_migrate_files,
			NET_TRANSPORT_RPC,
			N_("Migrate files from remote to local server"),
			N_("net rpc share migrate files\n"
			   "    Migrate files from remote to local server")
		},
		{
			"security",
			rpc_share_migrate_security,
			NET_TRANSPORT_RPC,
			N_("Migrate share-ACLs from remote to local server"),
			N_("net rpc share migrate security\n"
			   "    Migrate share-ACLs from remote to local server")
		},
		{
			"shares",
			rpc_share_migrate_shares,
			NET_TRANSPORT_RPC,
			N_("Migrate shares from remote to local server"),
			N_("net rpc share migrate shares\n"
			   "    Migrate shares from remote to local server")
		},
		{NULL, NULL, 0, NULL, NULL}
	};

	net_mode_share = NET_MODE_SHARE_MIGRATE;

	return net_run_function(c, argc, argv, "net rpc share migrate", func);
}

struct full_alias {
	struct dom_sid sid;
	uint32 num_members;
	struct dom_sid *members;
};

static int num_server_aliases;
static struct full_alias *server_aliases;

/*
 * Add an alias to the static list.
 */
static void push_alias(TALLOC_CTX *mem_ctx, struct full_alias *alias)
{
	if (server_aliases == NULL)
		server_aliases = SMB_MALLOC_ARRAY(struct full_alias, 100);

	server_aliases[num_server_aliases] = *alias;
	num_server_aliases += 1;
}

/*
 * For a specific domain on the server, fetch all the aliases
 * and their members. Add all of them to the server_aliases.
 */

static NTSTATUS rpc_fetch_domain_aliases(struct rpc_pipe_client *pipe_hnd,
					TALLOC_CTX *mem_ctx,
					struct policy_handle *connect_pol,
					const struct dom_sid *domain_sid)
{
	uint32 start_idx, max_entries, num_entries, i;
	struct samr_SamArray *groups = NULL;
	NTSTATUS result, status;
	struct policy_handle domain_pol;
	struct dcerpc_binding_handle *b = pipe_hnd->binding_handle;

	/* Get domain policy handle */

	status = dcerpc_samr_OpenDomain(b, mem_ctx,
					connect_pol,
					MAXIMUM_ALLOWED_ACCESS,
					CONST_DISCARD(struct dom_sid2 *, domain_sid),
					&domain_pol,
					&result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if (!NT_STATUS_IS_OK(result)) {
		return result;
	}

	start_idx = 0;
	max_entries = 250;

	do {
		status = dcerpc_samr_EnumDomainAliases(b, mem_ctx,
						       &domain_pol,
						       &start_idx,
						       &groups,
						       max_entries,
						       &num_entries,
						       &result);
		if (!NT_STATUS_IS_OK(status)) {
			goto done;
		}
		for (i = 0; i < num_entries; i++) {

			struct policy_handle alias_pol;
			struct full_alias alias;
			struct lsa_SidArray sid_array;
			int j;
			NTSTATUS _result;

			status = dcerpc_samr_OpenAlias(b, mem_ctx,
						       &domain_pol,
						       MAXIMUM_ALLOWED_ACCESS,
						       groups->entries[i].idx,
						       &alias_pol,
						       &_result);
			if (!NT_STATUS_IS_OK(status)) {
				goto done;
			}
			if (!NT_STATUS_IS_OK(_result)) {
				status = _result;
				goto done;
			}

			status = dcerpc_samr_GetMembersInAlias(b, mem_ctx,
							       &alias_pol,
							       &sid_array,
							       &_result);
			if (!NT_STATUS_IS_OK(status)) {
				goto done;
			}
			if (!NT_STATUS_IS_OK(_result)) {
				status = _result;
				goto done;
			}

			alias.num_members = sid_array.num_sids;

			status = dcerpc_samr_Close(b, mem_ctx, &alias_pol, &_result);
			if (!NT_STATUS_IS_OK(status)) {
				goto done;
			}
			if (!NT_STATUS_IS_OK(_result)) {
				status = _result;
				goto done;
			}

			alias.members = NULL;

			if (alias.num_members > 0) {
				alias.members = SMB_MALLOC_ARRAY(struct dom_sid, alias.num_members);

				for (j = 0; j < alias.num_members; j++)
					sid_copy(&alias.members[j],
						 sid_array.sids[j].sid);
			}

			sid_compose(&alias.sid, domain_sid,
				    groups->entries[i].idx);

			push_alias(mem_ctx, &alias);
		}
	} while (NT_STATUS_EQUAL(result, STATUS_MORE_ENTRIES));

	status = NT_STATUS_OK;

 done:
	dcerpc_samr_Close(b, mem_ctx, &domain_pol, &result);

	return status;
}

/*
 * Dump server_aliases as names for debugging purposes.
 */

static NTSTATUS rpc_aliaslist_dump(struct net_context *c,
				const struct dom_sid *domain_sid,
				const char *domain_name,
				struct cli_state *cli,
				struct rpc_pipe_client *pipe_hnd,
				TALLOC_CTX *mem_ctx,
				int argc,
				const char **argv)
{
	int i;
	NTSTATUS result;
	struct policy_handle lsa_pol;
	struct dcerpc_binding_handle *b = pipe_hnd->binding_handle;

	result = rpccli_lsa_open_policy(pipe_hnd, mem_ctx, true,
				     SEC_FLAG_MAXIMUM_ALLOWED,
				     &lsa_pol);
	if (!NT_STATUS_IS_OK(result))
		return result;

	for (i=0; i<num_server_aliases; i++) {
		char **names;
		char **domains;
		enum lsa_SidType *types;
		int j;

		struct full_alias *alias = &server_aliases[i];

		result = rpccli_lsa_lookup_sids(pipe_hnd, mem_ctx, &lsa_pol, 1,
					     &alias->sid,
					     &domains, &names, &types);
		if (!NT_STATUS_IS_OK(result))
			continue;

		DEBUG(1, ("%s\\%s %d: ", domains[0], names[0], types[0]));

		if (alias->num_members == 0) {
			DEBUG(1, ("\n"));
			continue;
		}

		result = rpccli_lsa_lookup_sids(pipe_hnd, mem_ctx, &lsa_pol,
					     alias->num_members,
					     alias->members,
					     &domains, &names, &types);

		if (!NT_STATUS_IS_OK(result) &&
		    !NT_STATUS_EQUAL(result, STATUS_SOME_UNMAPPED))
			continue;

		for (j=0; j<alias->num_members; j++)
			DEBUG(1, ("%s\\%s (%d); ",
				  domains[j] ? domains[j] : "*unknown*", 
				  names[j] ? names[j] : "*unknown*",types[j]));
		DEBUG(1, ("\n"));
	}

	dcerpc_lsa_Close(b, mem_ctx, &lsa_pol, &result);

	return NT_STATUS_OK;
}

/*
 * Fetch a list of all server aliases and their members into
 * server_aliases.
 */

static NTSTATUS rpc_aliaslist_internals(struct net_context *c,
					const struct dom_sid *domain_sid,
					const char *domain_name,
					struct cli_state *cli,
					struct rpc_pipe_client *pipe_hnd,
					TALLOC_CTX *mem_ctx,
					int argc,
					const char **argv)
{
	NTSTATUS result, status;
	struct policy_handle connect_pol;
	struct dcerpc_binding_handle *b = pipe_hnd->binding_handle;

	status = dcerpc_samr_Connect2(b, mem_ctx,
				      pipe_hnd->desthost,
				      MAXIMUM_ALLOWED_ACCESS,
				      &connect_pol,
				      &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto done;
	}

	status = rpc_fetch_domain_aliases(pipe_hnd, mem_ctx, &connect_pol,
					  &global_sid_Builtin);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	status = rpc_fetch_domain_aliases(pipe_hnd, mem_ctx, &connect_pol,
					  domain_sid);

	dcerpc_samr_Close(b, mem_ctx, &connect_pol, &result);
 done:
	return status;
}

static void init_user_token(struct security_token *token, struct dom_sid *user_sid)
{
	token->num_sids = 4;

	if (!(token->sids = SMB_MALLOC_ARRAY(struct dom_sid, 4))) {
		d_fprintf(stderr, "malloc %s\n",_("failed"));
		token->num_sids = 0;
		return;
	}

	token->sids[0] = *user_sid;
	sid_copy(&token->sids[1], &global_sid_World);
	sid_copy(&token->sids[2], &global_sid_Network);
	sid_copy(&token->sids[3], &global_sid_Authenticated_Users);
}

static void free_user_token(struct security_token *token)
{
	SAFE_FREE(token->sids);
}

static void add_sid_to_token(struct security_token *token, struct dom_sid *sid)
{
	if (security_token_has_sid(token, sid))
		return;

	token->sids = SMB_REALLOC_ARRAY(token->sids, struct dom_sid, token->num_sids+1);
	if (!token->sids) {
		return;
	}

	sid_copy(&token->sids[token->num_sids], sid);

	token->num_sids += 1;
}

struct user_token {
	fstring name;
	struct security_token token;
};

static void dump_user_token(struct user_token *token)
{
	int i;

	d_printf("%s\n", token->name);

	for (i=0; i<token->token.num_sids; i++) {
		d_printf(" %s\n", sid_string_tos(&token->token.sids[i]));
	}
}

static bool is_alias_member(struct dom_sid *sid, struct full_alias *alias)
{
	int i;

	for (i=0; i<alias->num_members; i++) {
		if (dom_sid_compare(sid, &alias->members[i]) == 0)
			return true;
	}

	return false;
}

static void collect_sid_memberships(struct security_token *token, struct dom_sid sid)
{
	int i;

	for (i=0; i<num_server_aliases; i++) {
		if (is_alias_member(&sid, &server_aliases[i]))
			add_sid_to_token(token, &server_aliases[i].sid);
	}
}

/*
 * We got a user token with all the SIDs we can know about without asking the
 * server directly. These are the user and domain group sids. All of these can
 * be members of aliases. So scan the list of aliases for each of the SIDs and
 * add them to the token.
 */

static void collect_alias_memberships(struct security_token *token)
{
	int num_global_sids = token->num_sids;
	int i;

	for (i=0; i<num_global_sids; i++) {
		collect_sid_memberships(token, token->sids[i]);
	}
}

static bool get_user_sids(const char *domain, const char *user, struct security_token *token)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	enum wbcSidType type;
	fstring full_name;
	struct wbcDomainSid wsid;
	char sid_str[WBC_SID_STRING_BUFLEN];
	struct dom_sid user_sid;
	uint32_t num_groups;
	gid_t *groups = NULL;
	uint32_t i;

	fstr_sprintf(full_name, "%s%c%s",
		     domain, *lp_winbind_separator(), user);

	/* First let's find out the user sid */

	wbc_status = wbcLookupName(domain, user, &wsid, &type);

	if (!WBC_ERROR_IS_OK(wbc_status)) {
		DEBUG(1, ("winbind could not find %s: %s\n",
			  full_name, wbcErrorString(wbc_status)));
		return false;
	}

	wbcSidToStringBuf(&wsid, sid_str, sizeof(sid_str));

	if (type != WBC_SID_NAME_USER) {
		DEBUG(1, ("%s is not a user\n", full_name));
		return false;
	}

	if (!string_to_sid(&user_sid, sid_str)) {
		DEBUG(1,("Could not convert sid %s from string\n", sid_str));
		return false;
	}

	init_user_token(token, &user_sid);

	/* And now the groups winbind knows about */

	wbc_status = wbcGetGroups(full_name, &num_groups, &groups);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		DEBUG(1, ("winbind could not get groups of %s: %s\n",
			full_name, wbcErrorString(wbc_status)));
		return false;
	}

	for (i = 0; i < num_groups; i++) {
		gid_t gid = groups[i];
		struct dom_sid sid;

		wbc_status = wbcGidToSid(gid, &wsid);
		if (!WBC_ERROR_IS_OK(wbc_status)) {
			DEBUG(1, ("winbind could not find SID of gid %u: %s\n",
				  (unsigned int)gid, wbcErrorString(wbc_status)));
			wbcFreeMemory(groups);
			return false;
		}

		wbcSidToStringBuf(&wsid, sid_str, sizeof(sid_str));

		DEBUG(3, (" %s\n", sid_str));

		string_to_sid(&sid, sid_str);
		add_sid_to_token(token, &sid);
	}
	wbcFreeMemory(groups);

	return true;
}

/**
 * Get a list of all user tokens we want to look at
 **/

static bool get_user_tokens(struct net_context *c, int *num_tokens,
			    struct user_token **user_tokens)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	uint32_t i, num_users;
	const char **users;
	struct user_token *result;
	TALLOC_CTX *frame = NULL;

	if (lp_winbind_use_default_domain() &&
	    (c->opt_target_workgroup == NULL)) {
		d_fprintf(stderr, _("winbind use default domain = yes set, "
			 "please specify a workgroup\n"));
		return false;
	}

	/* Send request to winbind daemon */

	wbc_status = wbcListUsers(NULL, &num_users, &users);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		DEBUG(1, (_("winbind could not list users: %s\n"),
			  wbcErrorString(wbc_status)));
		return false;
	}

	result = SMB_MALLOC_ARRAY(struct user_token, num_users);

	if (result == NULL) {
		DEBUG(1, ("Could not malloc sid array\n"));
		wbcFreeMemory(users);
		return false;
	}

	frame = talloc_stackframe();
	for (i=0; i < num_users; i++) {
		fstring domain, user;
		char *p;

		fstrcpy(result[i].name, users[i]);

		p = strchr(users[i], *lp_winbind_separator());

		DEBUG(3, ("%s\n", users[i]));

		if (p == NULL) {
			fstrcpy(domain, c->opt_target_workgroup);
			fstrcpy(user, users[i]);
		} else {
			*p++ = '\0';
			fstrcpy(domain, users[i]);
			strupper_m(domain);
			fstrcpy(user, p);
		}

		get_user_sids(domain, user, &(result[i].token));
	}
	TALLOC_FREE(frame);
	wbcFreeMemory(users);

	*num_tokens = num_users;
	*user_tokens = result;

	return true;
}

static bool get_user_tokens_from_file(FILE *f,
				      int *num_tokens,
				      struct user_token **tokens)
{
	struct user_token *token = NULL;

	while (!feof(f)) {
		fstring line;

		if (fgets(line, sizeof(line)-1, f) == NULL) {
			return true;
		}

		if ((strlen(line) > 0) && (line[strlen(line)-1] == '\n')) {
			line[strlen(line)-1] = '\0';
		}

		if (line[0] == ' ') {
			/* We have a SID */

			struct dom_sid sid;
			if(!string_to_sid(&sid, &line[1])) {
				DEBUG(1,("get_user_tokens_from_file: Could "
					"not convert sid %s \n",&line[1]));
				return false;
			}

			if (token == NULL) {
				DEBUG(0, ("File does not begin with username"));
				return false;
			}

			add_sid_to_token(&token->token, &sid);
			continue;
		}

		/* And a new user... */

		*num_tokens += 1;
		*tokens = SMB_REALLOC_ARRAY(*tokens, struct user_token, *num_tokens);
		if (*tokens == NULL) {
			DEBUG(0, ("Could not realloc tokens\n"));
			return false;
		}

		token = &((*tokens)[*num_tokens-1]);

		fstrcpy(token->name, line);
		token->token.num_sids = 0;
		token->token.sids = NULL;
		continue;
	}
	
	return false;
}


/*
 * Show the list of all users that have access to a share
 */

static void show_userlist(struct rpc_pipe_client *pipe_hnd,
			TALLOC_CTX *mem_ctx,
			const char *netname,
			int num_tokens,
			struct user_token *tokens)
{
	uint16_t fnum;
	struct security_descriptor *share_sd = NULL;
	struct security_descriptor *root_sd = NULL;
	struct cli_state *cli = rpc_pipe_np_smb_conn(pipe_hnd);
	int i;
	union srvsvc_NetShareInfo info;
	WERROR result;
	NTSTATUS status;
	uint16 cnum;
	struct dcerpc_binding_handle *b = pipe_hnd->binding_handle;

	status = dcerpc_srvsvc_NetShareGetInfo(b, mem_ctx,
					       pipe_hnd->desthost,
					       netname,
					       502,
					       &info,
					       &result);

	if (!NT_STATUS_IS_OK(status) || !W_ERROR_IS_OK(result)) {
		DEBUG(1, ("Coult not query secdesc for share %s\n",
			  netname));
		return;
	}

	share_sd = info.info502->sd_buf.sd;
	if (share_sd == NULL) {
		DEBUG(1, ("Got no secdesc for share %s\n",
			  netname));
	}

	cnum = cli->cnum;

	if (!NT_STATUS_IS_OK(cli_tcon_andx(cli, netname, "A:", "", 0))) {
		return;
	}

	if (!NT_STATUS_IS_OK(cli_ntcreate(cli, "\\", 0, READ_CONTROL_ACCESS, 0,
			FILE_SHARE_READ|FILE_SHARE_WRITE, FILE_OPEN, 0x0, 0x0, &fnum))) {
		root_sd = cli_query_secdesc(cli, fnum, mem_ctx);
	}

	for (i=0; i<num_tokens; i++) {
		uint32 acc_granted;

		if (share_sd != NULL) {
			status = se_access_check(share_sd, &tokens[i].token,
					     1, &acc_granted);

			if (!NT_STATUS_IS_OK(status)) {
				DEBUG(1, ("Could not check share_sd for "
					  "user %s\n",
					  tokens[i].name));
				continue;
			}
		}

		if (root_sd == NULL) {
			d_printf(" %s\n", tokens[i].name);
			continue;
		}

		status = se_access_check(root_sd, &tokens[i].token,
				     1, &acc_granted);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(1, ("Could not check root_sd for user %s\n",
				  tokens[i].name));
			continue;
		}
		d_printf(" %s\n", tokens[i].name);
	}

	if (fnum != (uint16_t)-1)
		cli_close(cli, fnum);
	cli_tdis(cli);
	cli->cnum = cnum;
	
	return;
}

/**
 * List shares on a remote RPC server, including the security descriptors.
 *
 * All parameters are provided by the run_rpc_command function, except for
 * argc, argv which are passed through.
 *
 * @param domain_sid The domain sid acquired from the remote server.
 * @param cli A cli_state connected to the server.
 * @param mem_ctx Talloc context, destroyed on completion of the function.
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 *
 * @return Normal NTSTATUS return.
 **/

static NTSTATUS rpc_share_allowedusers_internals(struct net_context *c,
						const struct dom_sid *domain_sid,
						const char *domain_name,
						struct cli_state *cli,
						struct rpc_pipe_client *pipe_hnd,
						TALLOC_CTX *mem_ctx,
						int argc,
						const char **argv)
{
	bool r;
	FILE *f;
	NTSTATUS nt_status = NT_STATUS_OK;
	uint32_t total_entries = 0;
	uint32_t resume_handle = 0;
	uint32_t preferred_len = 0xffffffff;
	uint32_t i;
	struct dcerpc_binding_handle *b = NULL;
	struct srvsvc_NetShareInfoCtr info_ctr;
	struct srvsvc_NetShareCtr1 ctr1;
	WERROR result;

	struct user_token *tokens = NULL;
	int num_tokens = 0;

	if (argc == 0) {
		f = stdin;
	} else {
		f = fopen(argv[0], "r");
	}

	if (f == NULL) {
		DEBUG(0, ("Could not open userlist: %s\n", strerror(errno)));
		return NT_STATUS_UNSUCCESSFUL;
	}

	r = get_user_tokens_from_file(f, &num_tokens, &tokens);

	if (f != stdin)
		fclose(f);

	if (!r) {
		DEBUG(0, ("Could not read users from file\n"));
		return NT_STATUS_UNSUCCESSFUL;
	}

	for (i=0; i<num_tokens; i++)
		collect_alias_memberships(&tokens[i].token);

	ZERO_STRUCT(info_ctr);
	ZERO_STRUCT(ctr1);

	info_ctr.level = 1;
	info_ctr.ctr.ctr1 = &ctr1;

	b = pipe_hnd->binding_handle;

	/* Issue the NetShareEnum RPC call and retrieve the response */
	nt_status = dcerpc_srvsvc_NetShareEnumAll(b,
					talloc_tos(),
					pipe_hnd->desthost,
					&info_ctr,
					preferred_len,
					&total_entries,
					&resume_handle,
					&result);

	/* Was it successful? */
	if (!NT_STATUS_IS_OK(nt_status)) {
		/*  Nope.  Go clean up. */
		goto done;
	}

	if (!W_ERROR_IS_OK(result)) {
		/*  Nope.  Go clean up. */
		nt_status = werror_to_ntstatus(result);
		goto done;
	}

	if (total_entries == 0) {
		goto done;
	}

        /* For each returned entry... */
	for (i = 0; i < info_ctr.ctr.ctr1->count; i++) {
		const char *netname = info_ctr.ctr.ctr1->array[i].name;

		if (info_ctr.ctr.ctr1->array[i].type != STYPE_DISKTREE) {
			continue;
		}

		d_printf("%s\n", netname);

		show_userlist(pipe_hnd, mem_ctx, netname,
			      num_tokens, tokens);
	}
 done:
	for (i=0; i<num_tokens; i++) {
		free_user_token(&tokens[i].token);
	}
	SAFE_FREE(tokens);

	return nt_status;
}

static int rpc_share_allowedusers(struct net_context *c, int argc,
				  const char **argv)
{
	int result;

	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net rpc share allowedusers\n"
			    "    %s\n",
			  _("Usage:"),
			  _("List allowed users"));
		return 0;
	}

	result = run_rpc_command(c, NULL, &ndr_table_samr.syntax_id, 0,
				 rpc_aliaslist_internals,
				 argc, argv);
	if (result != 0)
		return result;

	result = run_rpc_command(c, NULL, &ndr_table_lsarpc.syntax_id, 0,
				 rpc_aliaslist_dump,
				 argc, argv);
	if (result != 0)
		return result;

	return run_rpc_command(c, NULL, &ndr_table_srvsvc.syntax_id, 0,
			       rpc_share_allowedusers_internals,
			       argc, argv);
}

int net_usersidlist(struct net_context *c, int argc, const char **argv)
{
	int num_tokens = 0;
	struct user_token *tokens = NULL;
	int i;

	if (argc != 0) {
		net_usersidlist_usage(c, argc, argv);
		return 0;
	}

	if (!get_user_tokens(c, &num_tokens, &tokens)) {
		DEBUG(0, ("Could not get the user/sid list\n"));
		return 0;
	}

	for (i=0; i<num_tokens; i++) {
		dump_user_token(&tokens[i]);
		free_user_token(&tokens[i].token);
	}

	SAFE_FREE(tokens);
	return 1;
}

int net_usersidlist_usage(struct net_context *c, int argc, const char **argv)
{
	d_printf(_("net usersidlist\n"
		   "\tprints out a list of all users the running winbind knows\n"
		   "\tabout, together with all their SIDs. This is used as\n"
		   "\tinput to the 'net rpc share allowedusers' command.\n\n"));

	net_common_flags_usage(c, argc, argv);
	return -1;
}

/**
 * 'net rpc share' entrypoint.
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 **/

int net_rpc_share(struct net_context *c, int argc, const char **argv)
{
	NET_API_STATUS status;

	struct functable func[] = {
		{
			"add",
			rpc_share_add,
			NET_TRANSPORT_RPC,
			N_("Add share"),
			N_("net rpc share add\n"
			   "    Add share")
		},
		{
			"delete",
			rpc_share_delete,
			NET_TRANSPORT_RPC,
			N_("Remove share"),
			N_("net rpc share delete\n"
			   "    Remove share")
		},
		{
			"allowedusers",
			rpc_share_allowedusers,
			NET_TRANSPORT_RPC,
			N_("Modify allowed users"),
			N_("net rpc share allowedusers\n"
			   "    Modify allowed users")
		},
		{
			"migrate",
			rpc_share_migrate,
			NET_TRANSPORT_RPC,
			N_("Migrate share to local server"),
			N_("net rpc share migrate\n"
			   "    Migrate share to local server")
		},
		{
			"list",
			rpc_share_list,
			NET_TRANSPORT_RPC,
			N_("List shares"),
			N_("net rpc share list\n"
			   "    List shares")
		},
		{NULL, NULL, 0, NULL, NULL}
	};

	status = libnetapi_net_init(&c->netapi_ctx);
	if (status != 0) {
		return -1;
	}
	libnetapi_set_username(c->netapi_ctx, c->opt_user_name);
	libnetapi_set_password(c->netapi_ctx, c->opt_password);
	if (c->opt_kerberos) {
		libnetapi_set_use_kerberos(c->netapi_ctx);
	}

	if (argc == 0) {
		if (c->display_usage) {
			d_printf("%s\n%s",
				 _("Usage:"),
				 _("net rpc share\n"
				   "    List shares\n"
				   "    Alias for net rpc share list\n"));
			net_display_usage_from_functable(func);
			return 0;
		}

		return rpc_share_list(c, argc, argv);
	}

	return net_run_function(c, argc, argv, "net rpc share", func);
}

static NTSTATUS rpc_sh_share_list(struct net_context *c,
				  TALLOC_CTX *mem_ctx,
				  struct rpc_sh_ctx *ctx,
				  struct rpc_pipe_client *pipe_hnd,
				  int argc, const char **argv)
{

	return werror_to_ntstatus(W_ERROR(rpc_share_list(c, argc, argv)));
}

static NTSTATUS rpc_sh_share_add(struct net_context *c,
				 TALLOC_CTX *mem_ctx,
				 struct rpc_sh_ctx *ctx,
				 struct rpc_pipe_client *pipe_hnd,
				 int argc, const char **argv)
{
	NET_API_STATUS status;
	uint32_t parm_err = 0;
	struct SHARE_INFO_2 i2;

	if ((argc < 2) || (argc > 3)) {
		d_fprintf(stderr, _("Usage: %s <share> <path> [comment]\n"),
			  ctx->whoami);
		return NT_STATUS_INVALID_PARAMETER;
	}

	i2.shi2_netname		= argv[0];
	i2.shi2_type		= STYPE_DISKTREE;
	i2.shi2_remark		= (argc == 3) ? argv[2] : "";
	i2.shi2_permissions	= 0;
	i2.shi2_max_uses	= 0;
	i2.shi2_current_uses	= 0;
	i2.shi2_path		= argv[1];
	i2.shi2_passwd		= NULL;

	status = NetShareAdd(pipe_hnd->desthost,
			     2,
			     (uint8_t *)&i2,
			     &parm_err);

	return werror_to_ntstatus(W_ERROR(status));
}

static NTSTATUS rpc_sh_share_delete(struct net_context *c,
				    TALLOC_CTX *mem_ctx,
				    struct rpc_sh_ctx *ctx,
				    struct rpc_pipe_client *pipe_hnd,
				    int argc, const char **argv)
{
	if (argc != 1) {
		d_fprintf(stderr, "%s %s <share>\n", _("Usage:"), ctx->whoami);
		return NT_STATUS_INVALID_PARAMETER;
	}

	return werror_to_ntstatus(W_ERROR(NetShareDel(pipe_hnd->desthost, argv[0], 0)));
}

static NTSTATUS rpc_sh_share_info(struct net_context *c,
				  TALLOC_CTX *mem_ctx,
				  struct rpc_sh_ctx *ctx,
				  struct rpc_pipe_client *pipe_hnd,
				  int argc, const char **argv)
{
	union srvsvc_NetShareInfo info;
	WERROR result;
	NTSTATUS status;
	struct dcerpc_binding_handle *b = pipe_hnd->binding_handle;

	if (argc != 1) {
		d_fprintf(stderr, "%s %s <share>\n", _("Usage:"), ctx->whoami);
		return NT_STATUS_INVALID_PARAMETER;
	}

	status = dcerpc_srvsvc_NetShareGetInfo(b, mem_ctx,
					       pipe_hnd->desthost,
					       argv[0],
					       2,
					       &info,
					       &result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
		goto done;
	}
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	d_printf(_("Name:     %s\n"), info.info2->name);
	d_printf(_("Comment:  %s\n"), info.info2->comment);
	d_printf(_("Path:     %s\n"), info.info2->path);
	d_printf(_("Password: %s\n"), info.info2->password);

 done:
	return werror_to_ntstatus(result);
}

struct rpc_sh_cmd *net_rpc_share_cmds(struct net_context *c, TALLOC_CTX *mem_ctx,
				      struct rpc_sh_ctx *ctx)
{
	static struct rpc_sh_cmd cmds[] = {

	{ "list", NULL, &ndr_table_srvsvc.syntax_id, rpc_sh_share_list,
	  N_("List available shares") },

	{ "add", NULL, &ndr_table_srvsvc.syntax_id, rpc_sh_share_add,
	  N_("Add a share") },

	{ "delete", NULL, &ndr_table_srvsvc.syntax_id, rpc_sh_share_delete,
	  N_("Delete a share") },

	{ "info", NULL, &ndr_table_srvsvc.syntax_id, rpc_sh_share_info,
	  N_("Get information about a share") },

	{ NULL, NULL, 0, NULL, NULL }
	};

	return cmds;
}

/****************************************************************************/

static int rpc_file_usage(struct net_context *c, int argc, const char **argv)
{
	return net_file_usage(c, argc, argv);
}

/**
 * Close a file on a remote RPC server.
 *
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 *
 * @return A shell status integer (0 for success).
 **/
static int rpc_file_close(struct net_context *c, int argc, const char **argv)
{
	if (argc < 1 || c->display_usage) {
		return rpc_file_usage(c, argc, argv);
	}

	return NetFileClose(c->opt_host, atoi(argv[0]));
}

/**
 * Formatted print of open file info
 *
 * @param r  struct FILE_INFO_3 contents
 **/

static void display_file_info_3(struct FILE_INFO_3 *r)
{
	d_printf("%-7.1d %-20.20s 0x%-4.2x %-6.1d %s\n",
		 r->fi3_id, r->fi3_username, r->fi3_permissions,
		 r->fi3_num_locks, r->fi3_pathname);
}

/**
 * List files for a user on a remote RPC server.
 *
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 *
 * @return A shell status integer (0 for success)..
 **/

static int rpc_file_user(struct net_context *c, int argc, const char **argv)
{
	NET_API_STATUS status;
	uint32 preferred_len = 0xffffffff, i;
	const char *username=NULL;
	uint32_t total_entries = 0;
	uint32_t entries_read = 0;
	uint32_t resume_handle = 0;
	struct FILE_INFO_3 *i3 = NULL;

	if (c->display_usage) {
		return rpc_file_usage(c, argc, argv);
	}

	/* if argc > 0, must be user command */
	if (argc > 0) {
		username = smb_xstrdup(argv[0]);
	}

	status = NetFileEnum(c->opt_host,
			     NULL,
			     username,
			     3,
			     (uint8_t **)(void *)&i3,
			     preferred_len,
			     &entries_read,
			     &total_entries,
			     &resume_handle);

	if (status != 0) {
		goto done;
	}

	/* Display results */

	d_printf(_(
		 "\nEnumerating open files on remote server:\n\n"
		 "\nFileId  Opened by            Perms  Locks  Path"
		 "\n------  ---------            -----  -----  ---- \n"));
	for (i = 0; i < entries_read; i++) {
		display_file_info_3(&i3[i]);
	}
 done:
	return status;
}

/**
 * 'net rpc file' entrypoint.
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 **/

int net_rpc_file(struct net_context *c, int argc, const char **argv)
{
	NET_API_STATUS status;

	struct functable func[] = {
		{
			"close",
			rpc_file_close,
			NET_TRANSPORT_RPC,
			N_("Close opened file"),
			N_("net rpc file close\n"
			   "    Close opened file")
		},
		{
			"user",
			rpc_file_user,
			NET_TRANSPORT_RPC,
			N_("List files opened by user"),
			N_("net rpc file user\n"
			   "    List files opened by user")
		},
#if 0
		{
			"info",
			rpc_file_info,
			NET_TRANSPORT_RPC,
			N_("Display information about opened file"),
			N_("net rpc file info\n"
			   "    Display information about opened file")
		},
#endif
		{NULL, NULL, 0, NULL, NULL}
	};

	status = libnetapi_net_init(&c->netapi_ctx);
	if (status != 0) {
		return -1;
	}
	libnetapi_set_username(c->netapi_ctx, c->opt_user_name);
	libnetapi_set_password(c->netapi_ctx, c->opt_password);
	if (c->opt_kerberos) {
		libnetapi_set_use_kerberos(c->netapi_ctx);
	}

	if (argc == 0) {
		if (c->display_usage) {
			d_printf(_("Usage:\n"));
			d_printf(_("net rpc file\n"
				   "    List opened files\n"));
			net_display_usage_from_functable(func);
			return 0;
		}

		return rpc_file_user(c, argc, argv);
	}

	return net_run_function(c, argc, argv, "net rpc file", func);
}

/**
 * ABORT the shutdown of a remote RPC Server, over initshutdown pipe.
 *
 * All parameters are provided by the run_rpc_command function, except for
 * argc, argv which are passed through.
 *
 * @param c	A net_context structure.
 * @param domain_sid The domain sid acquired from the remote server.
 * @param cli A cli_state connected to the server.
 * @param mem_ctx Talloc context, destroyed on completion of the function.
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 *
 * @return Normal NTSTATUS return.
 **/

static NTSTATUS rpc_shutdown_abort_internals(struct net_context *c,
					const struct dom_sid *domain_sid,
					const char *domain_name,
					struct cli_state *cli,
					struct rpc_pipe_client *pipe_hnd,
					TALLOC_CTX *mem_ctx,
					int argc,
					const char **argv)
{
	NTSTATUS status = NT_STATUS_UNSUCCESSFUL;
	WERROR result;
	struct dcerpc_binding_handle *b = pipe_hnd->binding_handle;

	status = dcerpc_initshutdown_Abort(b, mem_ctx, NULL, &result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if (W_ERROR_IS_OK(result)) {
		d_printf(_("\nShutdown successfully aborted\n"));
		DEBUG(5,("cmd_shutdown_abort: query succeeded\n"));
	} else
		DEBUG(5,("cmd_shutdown_abort: query failed\n"));

	return werror_to_ntstatus(result);
}

/**
 * ABORT the shutdown of a remote RPC Server, over winreg pipe.
 *
 * All parameters are provided by the run_rpc_command function, except for
 * argc, argv which are passed through.
 *
 * @param c	A net_context structure.
 * @param domain_sid The domain sid acquired from the remote server.
 * @param cli A cli_state connected to the server.
 * @param mem_ctx Talloc context, destroyed on completion of the function.
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 *
 * @return Normal NTSTATUS return.
 **/

static NTSTATUS rpc_reg_shutdown_abort_internals(struct net_context *c,
						const struct dom_sid *domain_sid,
						const char *domain_name,
						struct cli_state *cli,
						struct rpc_pipe_client *pipe_hnd,
						TALLOC_CTX *mem_ctx,
						int argc,
						const char **argv)
{
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;
	WERROR werr;
	struct dcerpc_binding_handle *b = pipe_hnd->binding_handle;

	result = dcerpc_winreg_AbortSystemShutdown(b, mem_ctx, NULL, &werr);

	if (!NT_STATUS_IS_OK(result)) {
		DEBUG(5,("cmd_reg_abort_shutdown: query failed\n"));
		return result;
	}
	if (W_ERROR_IS_OK(werr)) {
		d_printf(_("\nShutdown successfully aborted\n"));
		DEBUG(5,("cmd_reg_abort_shutdown: query succeeded\n"));
	} else
		DEBUG(5,("cmd_reg_abort_shutdown: query failed\n"));

	return werror_to_ntstatus(werr);
}

/**
 * ABORT the shutdown of a remote RPC server.
 *
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 *
 * @return A shell status integer (0 for success).
 **/

static int rpc_shutdown_abort(struct net_context *c, int argc,
			      const char **argv)
{
	int rc = -1;

	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net rpc abortshutdown\n"
			   "    %s\n",
			 _("Usage:"),
			 _("Abort a scheduled shutdown"));
		return 0;
	}

	rc = run_rpc_command(c, NULL, &ndr_table_initshutdown.syntax_id, 0,
			     rpc_shutdown_abort_internals, argc, argv);

	if (rc == 0)
		return rc;

	DEBUG(1, ("initshutdown pipe didn't work, trying winreg pipe\n"));

	return run_rpc_command(c, NULL, &ndr_table_winreg.syntax_id, 0,
			       rpc_reg_shutdown_abort_internals,
			       argc, argv);
}

/**
 * Shut down a remote RPC Server via initshutdown pipe.
 *
 * All parameters are provided by the run_rpc_command function, except for
 * argc, argv which are passed through.
 *
 * @param c	A net_context structure.
 * @param domain_sid The domain sid acquired from the remote server.
 * @param cli A cli_state connected to the server.
 * @param mem_ctx Talloc context, destroyed on completion of the function.
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 *
 * @return Normal NTSTATUS return.
 **/

NTSTATUS rpc_init_shutdown_internals(struct net_context *c,
				     const struct dom_sid *domain_sid,
				     const char *domain_name,
				     struct cli_state *cli,
				     struct rpc_pipe_client *pipe_hnd,
				     TALLOC_CTX *mem_ctx,
				     int argc,
				     const char **argv)
{
	NTSTATUS status = NT_STATUS_UNSUCCESSFUL;
	WERROR result;
        const char *msg = N_("This machine will be shutdown shortly");
	uint32 timeout = 20;
	struct lsa_StringLarge msg_string;
	struct dcerpc_binding_handle *b = pipe_hnd->binding_handle;

	if (c->opt_comment) {
		msg = c->opt_comment;
	}
	if (c->opt_timeout) {
		timeout = c->opt_timeout;
	}

	msg_string.string = msg;

	/* create an entry */
	status = dcerpc_initshutdown_Init(b, mem_ctx, NULL,
			&msg_string, timeout, c->opt_force, c->opt_reboot,
			&result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if (W_ERROR_IS_OK(result)) {
		d_printf(_("\nShutdown of remote machine succeeded\n"));
		DEBUG(5,("Shutdown of remote machine succeeded\n"));
	} else {
		DEBUG(1,("Shutdown of remote machine failed!\n"));
	}
	return werror_to_ntstatus(result);
}

/**
 * Shut down a remote RPC Server via winreg pipe.
 *
 * All parameters are provided by the run_rpc_command function, except for
 * argc, argv which are passed through.
 *
 * @param c	A net_context structure.
 * @param domain_sid The domain sid acquired from the remote server.
 * @param cli A cli_state connected to the server.
 * @param mem_ctx Talloc context, destroyed on completion of the function.
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 *
 * @return Normal NTSTATUS return.
 **/

NTSTATUS rpc_reg_shutdown_internals(struct net_context *c,
				    const struct dom_sid *domain_sid,
				    const char *domain_name,
				    struct cli_state *cli,
				    struct rpc_pipe_client *pipe_hnd,
				    TALLOC_CTX *mem_ctx,
				    int argc,
				    const char **argv)
{
        const char *msg = N_("This machine will be shutdown shortly");
	uint32 timeout = 20;
	struct lsa_StringLarge msg_string;
	NTSTATUS result;
	WERROR werr;
	struct dcerpc_binding_handle *b = pipe_hnd->binding_handle;

	if (c->opt_comment) {
		msg = c->opt_comment;
	}
	msg_string.string = msg;

	if (c->opt_timeout) {
		timeout = c->opt_timeout;
	}

	/* create an entry */
	result = dcerpc_winreg_InitiateSystemShutdown(b, mem_ctx, NULL,
			&msg_string, timeout, c->opt_force, c->opt_reboot,
			&werr);
	if (!NT_STATUS_IS_OK(result)) {
		d_fprintf(stderr, "\nShutdown of remote machine failed\n");
		return result;
	}

	if (W_ERROR_IS_OK(werr)) {
		d_printf(_("\nShutdown of remote machine succeeded\n"));
	} else {
		d_fprintf(stderr, "\nShutdown of remote machine failed\n");
		if ( W_ERROR_EQUAL(werr, WERR_MACHINE_LOCKED) )
			d_fprintf(stderr, "\nMachine locked, use -f switch to force\n");
		else
			d_fprintf(stderr, "\nresult was: %s\n", win_errstr(werr));
	}

	return werror_to_ntstatus(werr);
}

/**
 * Shut down a remote RPC server.
 *
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 *
 * @return A shell status integer (0 for success).
 **/

static int rpc_shutdown(struct net_context *c, int argc, const char **argv)
{
	int rc =  -1;

	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net rpc shutdown\n"
			   "    %s\n",
			 _("Usage:"),
			 _("Shut down a remote RPC server"));
		return 0;
	}

	rc = run_rpc_command(c, NULL, &ndr_table_initshutdown.syntax_id, 0,
			     rpc_init_shutdown_internals, argc, argv);

	if (rc) {
		DEBUG(1, ("initshutdown pipe failed, trying winreg pipe\n"));
		rc = run_rpc_command(c, NULL, &ndr_table_winreg.syntax_id, 0,
				     rpc_reg_shutdown_internals, argc, argv);
	}

	return rc;
}

/***************************************************************************
  NT Domain trusts code (i.e. 'net rpc trustdom' functionality)
 ***************************************************************************/

/**
 * Add interdomain trust account to the RPC server.
 * All parameters (except for argc and argv) are passed by run_rpc_command
 * function.
 *
 * @param c	A net_context structure.
 * @param domain_sid The domain sid acquired from the server.
 * @param cli A cli_state connected to the server.
 * @param mem_ctx Talloc context, destroyed on completion of the function.
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 *
 * @return normal NTSTATUS return code.
 */

static NTSTATUS rpc_trustdom_add_internals(struct net_context *c,
						const struct dom_sid *domain_sid,
						const char *domain_name,
						struct cli_state *cli,
						struct rpc_pipe_client *pipe_hnd,
						TALLOC_CTX *mem_ctx,
						int argc,
						const char **argv)
{
	struct policy_handle connect_pol, domain_pol, user_pol;
	NTSTATUS status, result;
	char *acct_name;
	struct lsa_String lsa_acct_name;
	uint32 acb_info;
	uint32 acct_flags=0;
	uint32 user_rid;
	uint32_t access_granted = 0;
	union samr_UserInfo info;
	unsigned int orig_timeout;
	struct dcerpc_binding_handle *b = pipe_hnd->binding_handle;

	if (argc != 2) {
		d_printf("%s\n%s",
			 _("Usage:"),
			 _(" net rpc trustdom add <domain_name> "
			   "<trust password>\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	/*
	 * Make valid trusting domain account (ie. uppercased and with '$' appended)
	 */

	if (asprintf(&acct_name, "%s$", argv[0]) < 0) {
		return NT_STATUS_NO_MEMORY;
	}

	strupper_m(acct_name);

	init_lsa_String(&lsa_acct_name, acct_name);

	/* Get samr policy handle */
	status = dcerpc_samr_Connect2(b, mem_ctx,
				      pipe_hnd->desthost,
				      MAXIMUM_ALLOWED_ACCESS,
				      &connect_pol,
				      &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto done;
	}

	/* Get domain policy handle */
	status = dcerpc_samr_OpenDomain(b, mem_ctx,
					&connect_pol,
					MAXIMUM_ALLOWED_ACCESS,
					CONST_DISCARD(struct dom_sid2 *, domain_sid),
					&domain_pol,
					&result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto done;
	}

        /* This call can take a long time - allow the server to time out.
	 * 35 seconds should do it. */

        orig_timeout = rpccli_set_timeout(pipe_hnd, 35000);

	/* Create trusting domain's account */
	acb_info = ACB_NORMAL;
	acct_flags = SEC_GENERIC_READ | SEC_GENERIC_WRITE | SEC_GENERIC_EXECUTE |
		     SEC_STD_WRITE_DAC | SEC_STD_DELETE |
		     SAMR_USER_ACCESS_SET_PASSWORD |
		     SAMR_USER_ACCESS_GET_ATTRIBUTES |
		     SAMR_USER_ACCESS_SET_ATTRIBUTES;

	status = dcerpc_samr_CreateUser2(b, mem_ctx,
					 &domain_pol,
					 &lsa_acct_name,
					 acb_info,
					 acct_flags,
					 &user_pol,
					 &access_granted,
					 &user_rid,
					 &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	/* And restore our original timeout. */
	rpccli_set_timeout(pipe_hnd, orig_timeout);

	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		d_printf(_("net rpc trustdom add: create user %s failed %s\n"),
			acct_name, nt_errstr(result));
		goto done;
	}

	{
		struct samr_CryptPassword crypt_pwd;

		ZERO_STRUCT(info.info23);

		init_samr_CryptPassword(argv[1],
					&cli->user_session_key,
					&crypt_pwd);

		info.info23.info.fields_present = SAMR_FIELD_ACCT_FLAGS |
						  SAMR_FIELD_NT_PASSWORD_PRESENT;
		info.info23.info.acct_flags = ACB_DOMTRUST;
		info.info23.password = crypt_pwd;

		status = dcerpc_samr_SetUserInfo2(b, mem_ctx,
						  &user_pol,
						  23,
						  &info,
						  &result);
		if (!NT_STATUS_IS_OK(status)) {
			goto done;
		}

		if (!NT_STATUS_IS_OK(result)) {
			status = result;
			DEBUG(0,("Could not set trust account password: %s\n",
				 nt_errstr(result)));
			goto done;
		}
	}

 done:
	SAFE_FREE(acct_name);
	return status;
}

/**
 * Create interdomain trust account for a remote domain.
 *
 * @param argc Standard argc.
 * @param argv Standard argv without initial components.
 *
 * @return Integer status (0 means success).
 **/

static int rpc_trustdom_add(struct net_context *c, int argc, const char **argv)
{
	if (argc > 0 && !c->display_usage) {
		return run_rpc_command(c, NULL, &ndr_table_samr.syntax_id, 0,
				       rpc_trustdom_add_internals, argc, argv);
	} else {
		d_printf("%s\n%s",
			 _("Usage:"),
			 _("net rpc trustdom add <domain_name> <trust "
			   "password>\n"));
		return -1;
	}
}


/**
 * Remove interdomain trust account from the RPC server.
 * All parameters (except for argc and argv) are passed by run_rpc_command
 * function.
 *
 * @param c	A net_context structure.
 * @param domain_sid The domain sid acquired from the server.
 * @param cli A cli_state connected to the server.
 * @param mem_ctx Talloc context, destroyed on completion of the function.
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 *
 * @return normal NTSTATUS return code.
 */

static NTSTATUS rpc_trustdom_del_internals(struct net_context *c,
					const struct dom_sid *domain_sid,
					const char *domain_name,
					struct cli_state *cli,
					struct rpc_pipe_client *pipe_hnd,
					TALLOC_CTX *mem_ctx,
					int argc,
					const char **argv)
{
	struct policy_handle connect_pol, domain_pol, user_pol;
	NTSTATUS status, result;
	char *acct_name;
	struct dom_sid trust_acct_sid;
	struct samr_Ids user_rids, name_types;
	struct lsa_String lsa_acct_name;
	struct dcerpc_binding_handle *b = pipe_hnd->binding_handle;

	if (argc != 1) {
		d_printf("%s\n%s",
			 _("Usage:"),
			 _(" net rpc trustdom del <domain_name>\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	/*
	 * Make valid trusting domain account (ie. uppercased and with '$' appended)
	 */
	acct_name = talloc_asprintf(mem_ctx, "%s$", argv[0]);

	if (acct_name == NULL)
		return NT_STATUS_NO_MEMORY;

	strupper_m(acct_name);

	/* Get samr policy handle */
	status = dcerpc_samr_Connect2(b, mem_ctx,
				      pipe_hnd->desthost,
				      MAXIMUM_ALLOWED_ACCESS,
				      &connect_pol,
				      &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto done;
	}

	/* Get domain policy handle */
	status = dcerpc_samr_OpenDomain(b, mem_ctx,
					&connect_pol,
					MAXIMUM_ALLOWED_ACCESS,
					CONST_DISCARD(struct dom_sid2 *, domain_sid),
					&domain_pol,
					&result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto done;
	}

	init_lsa_String(&lsa_acct_name, acct_name);

	status = dcerpc_samr_LookupNames(b, mem_ctx,
					 &domain_pol,
					 1,
					 &lsa_acct_name,
					 &user_rids,
					 &name_types,
					 &result);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf(_("net rpc trustdom del: LookupNames on user %s "
			   "failed %s\n"),
			acct_name, nt_errstr(status));
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		d_printf(_("net rpc trustdom del: LookupNames on user %s "
			   "failed %s\n"),
			acct_name, nt_errstr(result) );
		goto done;
	}
	if (user_rids.count != 1) {
		status = NT_STATUS_INVALID_NETWORK_RESPONSE;
		goto done;
	}
	if (name_types.count != 1) {
		status = NT_STATUS_INVALID_NETWORK_RESPONSE;
		goto done;
	}

	status = dcerpc_samr_OpenUser(b, mem_ctx,
				      &domain_pol,
				      MAXIMUM_ALLOWED_ACCESS,
				      user_rids.ids[0],
				      &user_pol,
				      &result);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf(_("net rpc trustdom del: OpenUser on user %s failed "
			   "%s\n"),
			acct_name, nt_errstr(status) );
		goto done;
	}

	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		d_printf(_("net rpc trustdom del: OpenUser on user %s failed "
			   "%s\n"),
			acct_name, nt_errstr(result) );
		goto done;
	}

	/* append the rid to the domain sid */
	if (!sid_compose(&trust_acct_sid, domain_sid, user_rids.ids[0])) {
		goto done;
	}

	/* remove the sid */

	status = dcerpc_samr_RemoveMemberFromForeignDomain(b, mem_ctx,
							   &user_pol,
							   &trust_acct_sid,
							   &result);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf(_("net rpc trustdom del: RemoveMemberFromForeignDomain"
			   " on user %s failed %s\n"),
			acct_name, nt_errstr(status));
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		d_printf(_("net rpc trustdom del: RemoveMemberFromForeignDomain"
			   " on user %s failed %s\n"),
			acct_name, nt_errstr(result) );
		goto done;
	}


	/* Delete user */

	status = dcerpc_samr_DeleteUser(b, mem_ctx,
					&user_pol,
					&result);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf(_("net rpc trustdom del: DeleteUser on user %s failed "
			   "%s\n"),
			acct_name, nt_errstr(status));
		goto done;
	}

	if (!NT_STATUS_IS_OK(result)) {
		result = status;
		d_printf(_("net rpc trustdom del: DeleteUser on user %s failed "
			   "%s\n"),
			acct_name, nt_errstr(result) );
		goto done;
	}

	if (!NT_STATUS_IS_OK(result)) {
		d_printf(_("Could not set trust account password: %s\n"),
		   nt_errstr(result));
		goto done;
	}

 done:
	return status;
}

/**
 * Delete interdomain trust account for a remote domain.
 *
 * @param argc Standard argc.
 * @param argv Standard argv without initial components.
 *
 * @return Integer status (0 means success).
 **/

static int rpc_trustdom_del(struct net_context *c, int argc, const char **argv)
{
	if (argc > 0 && !c->display_usage) {
		return run_rpc_command(c, NULL, &ndr_table_samr.syntax_id, 0,
				       rpc_trustdom_del_internals, argc, argv);
	} else {
		d_printf("%s\n%s",
			 _("Usage:"),
			 _("net rpc trustdom del <domain>\n"));
		return -1;
	}
}

static NTSTATUS rpc_trustdom_get_pdc(struct net_context *c,
				     struct cli_state *cli,
				     TALLOC_CTX *mem_ctx,
				     const char *domain_name)
{
	char *dc_name = NULL;
	const char *buffer = NULL;
	struct rpc_pipe_client *netr;
	NTSTATUS status;
	WERROR result;
	struct dcerpc_binding_handle *b;

	/* Use NetServerEnum2 */

	if (cli_get_pdc_name(cli, domain_name, &dc_name)) {
		SAFE_FREE(dc_name);
		return NT_STATUS_OK;
	}

	DEBUG(1,("NetServerEnum2 error: Couldn't find primary domain controller\
		 for domain %s\n", domain_name));

	/* Try netr_GetDcName */

	status = cli_rpc_pipe_open_noauth(cli, &ndr_table_netlogon.syntax_id,
					  &netr);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	b = netr->binding_handle;

	status = dcerpc_netr_GetDcName(b, mem_ctx,
				       cli->desthost,
				       domain_name,
				       &buffer,
				       &result);
	TALLOC_FREE(netr);

	if (NT_STATUS_IS_OK(status) && W_ERROR_IS_OK(result)) {
		return status;
	}

	DEBUG(1,("netr_GetDcName error: Couldn't find primary domain controller\
		 for domain %s\n", domain_name));

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	return werror_to_ntstatus(result);
}

/**
 * Establish trust relationship to a trusting domain.
 * Interdomain account must already be created on remote PDC.
 *
 * @param c    A net_context structure.
 * @param argc Standard argc.
 * @param argv Standard argv without initial components.
 *
 * @return Integer status (0 means success).
 **/

static int rpc_trustdom_establish(struct net_context *c, int argc,
				  const char **argv)
{
	struct cli_state *cli = NULL;
	struct sockaddr_storage server_ss;
	struct rpc_pipe_client *pipe_hnd = NULL;
	struct policy_handle connect_hnd;
	TALLOC_CTX *mem_ctx;
	NTSTATUS nt_status, result;
	struct dom_sid *domain_sid;

	char* domain_name;
	char* acct_name;
	fstring pdc_name;
	union lsa_PolicyInformation *info = NULL;
	struct dcerpc_binding_handle *b;

	/*
	 * Connect to \\server\ipc$ as 'our domain' account with password
	 */

	if (argc != 1 || c->display_usage) {
		d_printf("%s\n%s",
			 _("Usage:"),
			 _("net rpc trustdom establish <domain_name>\n"));
		return -1;
	}

	domain_name = smb_xstrdup(argv[0]);
	strupper_m(domain_name);

	/* account name used at first is our domain's name with '$' */
	if (asprintf(&acct_name, "%s$", lp_workgroup()) == -1) {
		return -1;
	}
	strupper_m(acct_name);

	/*
	 * opt_workgroup will be used by connection functions further,
	 * hence it should be set to remote domain name instead of ours
	 */
	if (c->opt_workgroup) {
		c->opt_workgroup = smb_xstrdup(domain_name);
	};

	c->opt_user_name = acct_name;

	/* find the domain controller */
	if (!net_find_pdc(&server_ss, pdc_name, domain_name)) {
		DEBUG(0, ("Couldn't find domain controller for domain %s\n", domain_name));
		return -1;
	}

	/* connect to ipc$ as username/password */
	nt_status = connect_to_ipc(c, &cli, &server_ss, pdc_name);
	if (!NT_STATUS_EQUAL(nt_status, NT_STATUS_NOLOGON_INTERDOMAIN_TRUST_ACCOUNT)) {

		/* Is it trusting domain account for sure ? */
		DEBUG(0, ("Couldn't verify trusting domain account. Error was %s\n",
			nt_errstr(nt_status)));
		return -1;
	}

	/* store who we connected to */

	saf_store( domain_name, pdc_name );

	/*
	 * Connect to \\server\ipc$ again (this time anonymously)
	 */

	nt_status = connect_to_ipc_anonymous(c, &cli, &server_ss,
					     (char*)pdc_name);

	if (NT_STATUS_IS_ERR(nt_status)) {
		DEBUG(0, ("Couldn't connect to domain %s controller. Error was %s.\n",
			domain_name, nt_errstr(nt_status)));
		return -1;
	}

	if (!(mem_ctx = talloc_init("establishing trust relationship to "
				    "domain %s", domain_name))) {
		DEBUG(0, ("talloc_init() failed\n"));
		cli_shutdown(cli);
		return -1;
	}

	/* Make sure we're talking to a proper server */

	nt_status = rpc_trustdom_get_pdc(c, cli, mem_ctx, domain_name);
	if (!NT_STATUS_IS_OK(nt_status)) {
		cli_shutdown(cli);
		talloc_destroy(mem_ctx);
		return -1;
	}

	/*
	 * Call LsaOpenPolicy and LsaQueryInfo
	 */

	nt_status = cli_rpc_pipe_open_noauth(cli, &ndr_table_lsarpc.syntax_id,
					     &pipe_hnd);
	if (!NT_STATUS_IS_OK(nt_status)) {
		DEBUG(0, ("Could not initialise lsa pipe. Error was %s\n", nt_errstr(nt_status) ));
		cli_shutdown(cli);
		talloc_destroy(mem_ctx);
		return -1;
	}

	b = pipe_hnd->binding_handle;

	nt_status = rpccli_lsa_open_policy2(pipe_hnd, mem_ctx, true, KEY_QUERY_VALUE,
	                                 &connect_hnd);
	if (NT_STATUS_IS_ERR(nt_status)) {
		DEBUG(0, ("Couldn't open policy handle. Error was %s\n",
			nt_errstr(nt_status)));
		cli_shutdown(cli);
		talloc_destroy(mem_ctx);
		return -1;
	}

	/* Querying info level 5 */

	nt_status = dcerpc_lsa_QueryInfoPolicy(b, mem_ctx,
					       &connect_hnd,
					       LSA_POLICY_INFO_ACCOUNT_DOMAIN,
					       &info,
					       &result);
	if (NT_STATUS_IS_ERR(nt_status)) {
		DEBUG(0, ("LSA Query Info failed. Returned error was %s\n",
			nt_errstr(nt_status)));
		cli_shutdown(cli);
		talloc_destroy(mem_ctx);
		return -1;
	}
	if (NT_STATUS_IS_ERR(result)) {
		DEBUG(0, ("LSA Query Info failed. Returned error was %s\n",
			nt_errstr(result)));
		cli_shutdown(cli);
		talloc_destroy(mem_ctx);
		return -1;
	}

	domain_sid = info->account_domain.sid;

	/* There should be actually query info level 3 (following nt serv behaviour),
	   but I still don't know if it's _really_ necessary */

	/*
	 * Store the password in secrets db
	 */

	if (!pdb_set_trusteddom_pw(domain_name, c->opt_password, domain_sid)) {
		DEBUG(0, ("Storing password for trusted domain failed.\n"));
		cli_shutdown(cli);
		talloc_destroy(mem_ctx);
		return -1;
	}

	/*
	 * Close the pipes and clean up
	 */

	nt_status = dcerpc_lsa_Close(b, mem_ctx, &connect_hnd, &result);
	if (NT_STATUS_IS_ERR(nt_status)) {
		DEBUG(0, ("Couldn't close LSA pipe. Error was %s\n",
			nt_errstr(nt_status)));
		cli_shutdown(cli);
		talloc_destroy(mem_ctx);
		return -1;
	}

	cli_shutdown(cli);

	talloc_destroy(mem_ctx);

	d_printf(_("Trust to domain %s established\n"), domain_name);
	return 0;
}

/**
 * Revoke trust relationship to the remote domain.
 *
 * @param c    A net_context structure.
 * @param argc Standard argc.
 * @param argv Standard argv without initial components.
 *
 * @return Integer status (0 means success).
 **/

static int rpc_trustdom_revoke(struct net_context *c, int argc,
			       const char **argv)
{
	char* domain_name;
	int rc = -1;

	if (argc < 1 || c->display_usage) {
		d_printf("%s\n%s",
			 _("Usage:"),
			 _("net rpc trustdom revoke <domain_name>\n"
			   "  Revoke trust relationship\n"
			   "    domain_name\tName of domain to revoke trust\n"));
		return -1;
	}

	/* generate upper cased domain name */
	domain_name = smb_xstrdup(argv[0]);
	strupper_m(domain_name);

	/* delete password of the trust */
	if (!pdb_del_trusteddom_pw(domain_name)) {
		DEBUG(0, ("Failed to revoke relationship to the trusted domain %s\n",
			  domain_name));
		goto done;
	};

	rc = 0;
done:
	SAFE_FREE(domain_name);
	return rc;
}

static NTSTATUS rpc_query_domain_sid(struct net_context *c,
					const struct dom_sid *domain_sid,
					const char *domain_name,
					struct cli_state *cli,
					struct rpc_pipe_client *pipe_hnd,
					TALLOC_CTX *mem_ctx,
					int argc,
					const char **argv)
{
	fstring str_sid;
	if (!sid_to_fstring(str_sid, domain_sid)) {
		return NT_STATUS_UNSUCCESSFUL;
	}
	d_printf("%s\n", str_sid);
	return NT_STATUS_OK;
}

static void print_trusted_domain(struct dom_sid *dom_sid, const char *trusted_dom_name)
{
	fstring ascii_sid;

	/* convert sid into ascii string */
	sid_to_fstring(ascii_sid, dom_sid);

	d_printf("%-20s%s\n", trusted_dom_name, ascii_sid);
}

static NTSTATUS vampire_trusted_domain(struct rpc_pipe_client *pipe_hnd,
				      TALLOC_CTX *mem_ctx,
				      struct policy_handle *pol,
				      struct dom_sid dom_sid,
				      const char *trusted_dom_name)
{
	NTSTATUS nt_status, result;
	union lsa_TrustedDomainInfo *info = NULL;
	char *cleartextpwd = NULL;
	uint8_t session_key[16];
	DATA_BLOB session_key_blob;
	DATA_BLOB data = data_blob_null;
	struct dcerpc_binding_handle *b = pipe_hnd->binding_handle;

	nt_status = dcerpc_lsa_QueryTrustedDomainInfoBySid(b, mem_ctx,
							   pol,
							   &dom_sid,
							   LSA_TRUSTED_DOMAIN_INFO_PASSWORD,
							   &info,
							   &result);
	if (NT_STATUS_IS_ERR(nt_status)) {
		DEBUG(0,("Could not query trusted domain info. Error was %s\n",
		nt_errstr(nt_status)));
		goto done;
	}
	if (NT_STATUS_IS_ERR(result)) {
		nt_status = result;
		DEBUG(0,("Could not query trusted domain info. Error was %s\n",
		nt_errstr(result)));
		goto done;
	}

	data = data_blob(info->password.password->data,
			 info->password.password->length);

	if (!rpccli_get_pwd_hash(pipe_hnd, session_key)) {
		DEBUG(0, ("Could not retrieve password hash\n"));
		goto done;
	}

	session_key_blob = data_blob_const(session_key, sizeof(session_key));
	cleartextpwd = sess_decrypt_string(mem_ctx, &data, &session_key_blob);

	if (cleartextpwd == NULL) {
		DEBUG(0,("retrieved NULL password\n"));
		nt_status = NT_STATUS_UNSUCCESSFUL;
		goto done;
	}

	if (!pdb_set_trusteddom_pw(trusted_dom_name, cleartextpwd, &dom_sid)) {
		DEBUG(0, ("Storing password for trusted domain failed.\n"));
		nt_status = NT_STATUS_UNSUCCESSFUL;
		goto done;
	}

#ifdef DEBUG_PASSWORD
	DEBUG(100,("successfully vampired trusted domain [%s], sid: [%s], "
		   "password: [%s]\n", trusted_dom_name,
		   sid_string_dbg(&dom_sid), cleartextpwd));
#endif

done:
	SAFE_FREE(cleartextpwd);
	data_blob_free(&data);

	return nt_status;
}

static int rpc_trustdom_vampire(struct net_context *c, int argc,
				const char **argv)
{
	/* common variables */
	TALLOC_CTX* mem_ctx;
	struct cli_state *cli = NULL;
	struct rpc_pipe_client *pipe_hnd = NULL;
	NTSTATUS nt_status, result;
	const char *domain_name = NULL;
	struct dom_sid *queried_dom_sid;
	struct policy_handle connect_hnd;
	union lsa_PolicyInformation *info = NULL;

	/* trusted domains listing variables */
	unsigned int enum_ctx = 0;
	int i;
	struct lsa_DomainList dom_list;
	fstring pdc_name;
	struct dcerpc_binding_handle *b;

	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net rpc trustdom vampire\n"
			   "  %s\n",
			 _("Usage:"),
			 _("Vampire trust relationship from remote server"));
		return 0;
	}

	/*
	 * Listing trusted domains (stored in secrets.tdb, if local)
	 */

	mem_ctx = talloc_init("trust relationships vampire");

	/*
	 * set domain and pdc name to local samba server (default)
	 * or to remote one given in command line
	 */

	if (StrCaseCmp(c->opt_workgroup, lp_workgroup())) {
		domain_name = c->opt_workgroup;
		c->opt_target_workgroup = c->opt_workgroup;
	} else {
		fstrcpy(pdc_name, global_myname());
		domain_name = talloc_strdup(mem_ctx, lp_workgroup());
		c->opt_target_workgroup = domain_name;
	};

	/* open \PIPE\lsarpc and open policy handle */
	nt_status = net_make_ipc_connection(c, NET_FLAGS_PDC, &cli);
	if (!NT_STATUS_IS_OK(nt_status)) {
		DEBUG(0, ("Couldn't connect to domain controller: %s\n",
			  nt_errstr(nt_status)));
		talloc_destroy(mem_ctx);
		return -1;
	};

	nt_status = cli_rpc_pipe_open_noauth(cli, &ndr_table_lsarpc.syntax_id,
					     &pipe_hnd);
	if (!NT_STATUS_IS_OK(nt_status)) {
		DEBUG(0, ("Could not initialise lsa pipe. Error was %s\n",
			nt_errstr(nt_status) ));
		cli_shutdown(cli);
		talloc_destroy(mem_ctx);
		return -1;
	};

	b = pipe_hnd->binding_handle;

	nt_status = rpccli_lsa_open_policy2(pipe_hnd, mem_ctx, false, KEY_QUERY_VALUE,
					&connect_hnd);
	if (NT_STATUS_IS_ERR(nt_status)) {
		DEBUG(0, ("Couldn't open policy handle. Error was %s\n",
 			nt_errstr(nt_status)));
		cli_shutdown(cli);
		talloc_destroy(mem_ctx);
		return -1;
	};

	/* query info level 5 to obtain sid of a domain being queried */
	nt_status = dcerpc_lsa_QueryInfoPolicy(b, mem_ctx,
					       &connect_hnd,
					       LSA_POLICY_INFO_ACCOUNT_DOMAIN,
					       &info,
					       &result);

	if (NT_STATUS_IS_ERR(nt_status)) {
		DEBUG(0, ("LSA Query Info failed. Returned error was %s\n",
			nt_errstr(nt_status)));
		cli_shutdown(cli);
		talloc_destroy(mem_ctx);
		return -1;
	}
	if (NT_STATUS_IS_ERR(result)) {
		DEBUG(0, ("LSA Query Info failed. Returned error was %s\n",
			nt_errstr(result)));
		cli_shutdown(cli);
		talloc_destroy(mem_ctx);
		return -1;
	}

	queried_dom_sid = info->account_domain.sid;

	/*
	 * Keep calling LsaEnumTrustdom over opened pipe until
	 * the end of enumeration is reached
	 */

	d_printf(_("Vampire trusted domains:\n\n"));

	do {
		nt_status = dcerpc_lsa_EnumTrustDom(b, mem_ctx,
						    &connect_hnd,
						    &enum_ctx,
						    &dom_list,
						    (uint32_t)-1,
						    &result);
		if (NT_STATUS_IS_ERR(nt_status)) {
			DEBUG(0, ("Couldn't enumerate trusted domains. Error was %s\n",
				nt_errstr(nt_status)));
			cli_shutdown(cli);
			talloc_destroy(mem_ctx);
			return -1;
		};
		if (NT_STATUS_IS_ERR(result)) {
			nt_status = result;
			DEBUG(0, ("Couldn't enumerate trusted domains. Error was %s\n",
				nt_errstr(result)));
			cli_shutdown(cli);
			talloc_destroy(mem_ctx);
			return -1;
		};


		for (i = 0; i < dom_list.count; i++) {

			print_trusted_domain(dom_list.domains[i].sid,
					     dom_list.domains[i].name.string);

			nt_status = vampire_trusted_domain(pipe_hnd, mem_ctx, &connect_hnd, 
							   *dom_list.domains[i].sid,
							   dom_list.domains[i].name.string);
			if (!NT_STATUS_IS_OK(nt_status)) {
				cli_shutdown(cli);
				talloc_destroy(mem_ctx);
				return -1;
			}
		};

		/*
		 * in case of no trusted domains say something rather
		 * than just display blank line
		 */
		if (!dom_list.count) d_printf(_("none\n"));

	} while (NT_STATUS_EQUAL(nt_status, STATUS_MORE_ENTRIES));

	/* close this connection before doing next one */
	nt_status = dcerpc_lsa_Close(b, mem_ctx, &connect_hnd, &result);
	if (NT_STATUS_IS_ERR(nt_status)) {
		DEBUG(0, ("Couldn't properly close lsa policy handle. Error was %s\n",
			nt_errstr(nt_status)));
		cli_shutdown(cli);
		talloc_destroy(mem_ctx);
		return -1;
	};

	/* close lsarpc pipe and connection to IPC$ */
	cli_shutdown(cli);

	talloc_destroy(mem_ctx);
	return 0;
}

static int rpc_trustdom_list(struct net_context *c, int argc, const char **argv)
{
	/* common variables */
	TALLOC_CTX* mem_ctx;
	struct cli_state *cli = NULL, *remote_cli = NULL;
	struct rpc_pipe_client *pipe_hnd = NULL;
	NTSTATUS nt_status, result;
	const char *domain_name = NULL;
	struct dom_sid *queried_dom_sid;
	int ascii_dom_name_len;
	struct policy_handle connect_hnd;
	union lsa_PolicyInformation *info = NULL;
	struct dcerpc_binding_handle *b = NULL;

	/* trusted domains listing variables */
	unsigned int num_domains, enum_ctx = 0;
	int i;
	struct lsa_DomainList dom_list;
	fstring pdc_name;
	bool found_domain;

	/* trusting domains listing variables */
	struct policy_handle domain_hnd;
	struct samr_SamArray *trusts = NULL;

	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net rpc trustdom list\n"
			   "    %s\n",
			 _("Usage:"),
			 _("List incoming and outgoing trust relationships"));
		return 0;
	}

	/*
	 * Listing trusted domains (stored in secrets.tdb, if local)
	 */

	mem_ctx = talloc_init("trust relationships listing");

	/*
	 * set domain and pdc name to local samba server (default)
	 * or to remote one given in command line
	 */

	if (StrCaseCmp(c->opt_workgroup, lp_workgroup())) {
		domain_name = c->opt_workgroup;
		c->opt_target_workgroup = c->opt_workgroup;
	} else {
		fstrcpy(pdc_name, global_myname());
		domain_name = talloc_strdup(mem_ctx, lp_workgroup());
		c->opt_target_workgroup = domain_name;
	};

	/* open \PIPE\lsarpc and open policy handle */
	nt_status = net_make_ipc_connection(c, NET_FLAGS_PDC, &cli);
	if (!NT_STATUS_IS_OK(nt_status)) {
		DEBUG(0, ("Couldn't connect to domain controller: %s\n",
			  nt_errstr(nt_status)));
		talloc_destroy(mem_ctx);
		return -1;
	};

	nt_status = cli_rpc_pipe_open_noauth(cli, &ndr_table_lsarpc.syntax_id,
					     &pipe_hnd);
	if (!NT_STATUS_IS_OK(nt_status)) {
		DEBUG(0, ("Could not initialise lsa pipe. Error was %s\n",
			nt_errstr(nt_status) ));
		cli_shutdown(cli);
		talloc_destroy(mem_ctx);
		return -1;
	};

	b = pipe_hnd->binding_handle;

	nt_status = rpccli_lsa_open_policy2(pipe_hnd, mem_ctx, false, KEY_QUERY_VALUE,
					&connect_hnd);
	if (NT_STATUS_IS_ERR(nt_status)) {
		DEBUG(0, ("Couldn't open policy handle. Error was %s\n",
 			nt_errstr(nt_status)));
		cli_shutdown(cli);
		talloc_destroy(mem_ctx);
		return -1;
	};
	
	/* query info level 5 to obtain sid of a domain being queried */
	nt_status = dcerpc_lsa_QueryInfoPolicy(b, mem_ctx,
					       &connect_hnd,
					       LSA_POLICY_INFO_ACCOUNT_DOMAIN,
					       &info,
					       &result);

	if (NT_STATUS_IS_ERR(nt_status)) {
		DEBUG(0, ("LSA Query Info failed. Returned error was %s\n",
			nt_errstr(nt_status)));
		cli_shutdown(cli);
		talloc_destroy(mem_ctx);
		return -1;
	}
	if (NT_STATUS_IS_ERR(result)) {
		DEBUG(0, ("LSA Query Info failed. Returned error was %s\n",
			nt_errstr(result)));
		cli_shutdown(cli);
		talloc_destroy(mem_ctx);
		return -1;
	}

	queried_dom_sid = info->account_domain.sid;

	/*
	 * Keep calling LsaEnumTrustdom over opened pipe until
	 * the end of enumeration is reached
	 */

	d_printf(_("Trusted domains list:\n\n"));

	found_domain = false;

	do {
		nt_status = dcerpc_lsa_EnumTrustDom(b, mem_ctx,
						    &connect_hnd,
						    &enum_ctx,
						    &dom_list,
						    (uint32_t)-1,
						    &result);
		if (NT_STATUS_IS_ERR(nt_status)) {
			DEBUG(0, ("Couldn't enumerate trusted domains. Error was %s\n",
				nt_errstr(nt_status)));
			cli_shutdown(cli);
			talloc_destroy(mem_ctx);
			return -1;
		};
		if (NT_STATUS_IS_ERR(result)) {
			DEBUG(0, ("Couldn't enumerate trusted domains. Error was %s\n",
				nt_errstr(result)));
			cli_shutdown(cli);
			talloc_destroy(mem_ctx);
			return -1;
		};


		for (i = 0; i < dom_list.count; i++) {
			print_trusted_domain(dom_list.domains[i].sid,
					     dom_list.domains[i].name.string);
			found_domain = true;
		};


	} while (NT_STATUS_EQUAL(nt_status, STATUS_MORE_ENTRIES));

	/*
	 * in case of no trusted domains say something rather
	 * than just display blank line
	 */
	if (!found_domain) {
		d_printf(_("none\n"));
	}

	/* close this connection before doing next one */
	nt_status = dcerpc_lsa_Close(b, mem_ctx, &connect_hnd, &result);
	if (NT_STATUS_IS_ERR(nt_status)) {
		DEBUG(0, ("Couldn't properly close lsa policy handle. Error was %s\n",
			nt_errstr(nt_status)));
		cli_shutdown(cli);
		talloc_destroy(mem_ctx);
		return -1;
	};
	
	TALLOC_FREE(pipe_hnd);

	/*
	 * Listing trusting domains (stored in passdb backend, if local)
	 */
	
	d_printf(_("\nTrusting domains list:\n\n"));

	/*
	 * Open \PIPE\samr and get needed policy handles
	 */
	nt_status = cli_rpc_pipe_open_noauth(cli, &ndr_table_samr.syntax_id,
					     &pipe_hnd);
	if (!NT_STATUS_IS_OK(nt_status)) {
		DEBUG(0, ("Could not initialise samr pipe. Error was %s\n", nt_errstr(nt_status)));
		cli_shutdown(cli);
		talloc_destroy(mem_ctx);
		return -1;
	};

	b = pipe_hnd->binding_handle;

	/* SamrConnect2 */
	nt_status = dcerpc_samr_Connect2(b, mem_ctx,
					 pipe_hnd->desthost,
					 SAMR_ACCESS_LOOKUP_DOMAIN,
					 &connect_hnd,
					 &result);
	if (!NT_STATUS_IS_OK(nt_status)) {
		DEBUG(0, ("Couldn't open SAMR policy handle. Error was %s\n",
			nt_errstr(nt_status)));
		cli_shutdown(cli);
		talloc_destroy(mem_ctx);
		return -1;
	};
	if (!NT_STATUS_IS_OK(result)) {
		nt_status = result;
		DEBUG(0, ("Couldn't open SAMR policy handle. Error was %s\n",
			nt_errstr(result)));
		cli_shutdown(cli);
		talloc_destroy(mem_ctx);
		return -1;
	};

	/* SamrOpenDomain - we have to open domain policy handle in order to be
	   able to enumerate accounts*/
	nt_status = dcerpc_samr_OpenDomain(b, mem_ctx,
					   &connect_hnd,
					   SAMR_DOMAIN_ACCESS_ENUM_ACCOUNTS,
					   queried_dom_sid,
					   &domain_hnd,
					   &result);
	if (!NT_STATUS_IS_OK(nt_status)) {
		DEBUG(0, ("Couldn't open domain object. Error was %s\n",
			nt_errstr(nt_status)));
		cli_shutdown(cli);
		talloc_destroy(mem_ctx);
		return -1;
	};
	if (!NT_STATUS_IS_OK(result)) {
		nt_status = result;
		DEBUG(0, ("Couldn't open domain object. Error was %s\n",
			nt_errstr(result)));
		cli_shutdown(cli);
		talloc_destroy(mem_ctx);
		return -1;
	};

	/*
	 * perform actual enumeration
	 */

	found_domain = false;

	enum_ctx = 0;	/* reset enumeration context from last enumeration */
	do {

		nt_status = dcerpc_samr_EnumDomainUsers(b, mem_ctx,
							&domain_hnd,
							&enum_ctx,
							ACB_DOMTRUST,
							&trusts,
							0xffff,
							&num_domains,
							&result);
		if (NT_STATUS_IS_ERR(nt_status)) {
			DEBUG(0, ("Couldn't enumerate accounts. Error was: %s\n",
				nt_errstr(nt_status)));
			cli_shutdown(cli);
			talloc_destroy(mem_ctx);
			return -1;
		};
		if (NT_STATUS_IS_ERR(result)) {
			nt_status = result;
			DEBUG(0, ("Couldn't enumerate accounts. Error was: %s\n",
				nt_errstr(result)));
			cli_shutdown(cli);
			talloc_destroy(mem_ctx);
			return -1;
		};

		for (i = 0; i < num_domains; i++) {

			char *str = CONST_DISCARD(char *, trusts->entries[i].name.string);

			found_domain = true;

			/*
			 * get each single domain's sid (do we _really_ need this ?):
			 *  1) connect to domain's pdc
			 *  2) query the pdc for domain's sid
			 */

			/* get rid of '$' tail */
			ascii_dom_name_len = strlen(str);
			if (ascii_dom_name_len && ascii_dom_name_len < FSTRING_LEN)
				str[ascii_dom_name_len - 1] = '\0';

			/* set opt_* variables to remote domain */
			strupper_m(str);
			c->opt_workgroup = talloc_strdup(mem_ctx, str);
			c->opt_target_workgroup = c->opt_workgroup;

			d_printf("%-20s", str);

			/* connect to remote domain controller */
			nt_status = net_make_ipc_connection(c,
					NET_FLAGS_PDC | NET_FLAGS_ANONYMOUS,
					&remote_cli);
			if (NT_STATUS_IS_OK(nt_status)) {
				/* query for domain's sid */
				if (run_rpc_command(
					    c, remote_cli,
					    &ndr_table_lsarpc.syntax_id, 0,
					    rpc_query_domain_sid, argc,
					    argv))
					d_printf(_("strange - couldn't get domain's sid\n"));

				cli_shutdown(remote_cli);

			} else {
				d_fprintf(stderr, _("domain controller is not "
					  "responding: %s\n"),
					  nt_errstr(nt_status));
				d_printf(_("couldn't get domain's sid\n"));
			}
		}

	} while (NT_STATUS_EQUAL(result, STATUS_MORE_ENTRIES));

	if (!found_domain) {
		d_printf("none\n");
	}

	/* close opened samr and domain policy handles */
	nt_status = dcerpc_samr_Close(b, mem_ctx, &domain_hnd, &result);
	if (!NT_STATUS_IS_OK(nt_status)) {
		DEBUG(0, ("Couldn't properly close domain policy handle for domain %s\n", domain_name));
	};

	nt_status = dcerpc_samr_Close(b, mem_ctx, &connect_hnd, &result);
	if (!NT_STATUS_IS_OK(nt_status)) {
		DEBUG(0, ("Couldn't properly close samr policy handle for domain %s\n", domain_name));
	};

	/* close samr pipe and connection to IPC$ */
	cli_shutdown(cli);

	talloc_destroy(mem_ctx);
	return 0;
}

/**
 * Entrypoint for 'net rpc trustdom' code.
 *
 * @param argc Standard argc.
 * @param argv Standard argv without initial components.
 *
 * @return Integer status (0 means success).
 */

static int rpc_trustdom(struct net_context *c, int argc, const char **argv)
{
	struct functable func[] = {
		{
			"add",
			rpc_trustdom_add,
			NET_TRANSPORT_RPC,
			N_("Add trusting domain's account"),
			N_("net rpc trustdom add\n"
			   "    Add trusting domain's account")
		},
		{
			"del",
			rpc_trustdom_del,
			NET_TRANSPORT_RPC,
			N_("Remove trusting domain's account"),
			N_("net rpc trustdom del\n"
			   "    Remove trusting domain's account")
		},
		{
			"establish",
			rpc_trustdom_establish,
			NET_TRANSPORT_RPC,
			N_("Establish outgoing trust relationship"),
			N_("net rpc trustdom establish\n"
			   "    Establish outgoing trust relationship")
		},
		{
			"revoke",
			rpc_trustdom_revoke,
			NET_TRANSPORT_RPC,
			N_("Revoke outgoing trust relationship"),
			N_("net rpc trustdom revoke\n"
			   "    Revoke outgoing trust relationship")
		},
		{
			"list",
			rpc_trustdom_list,
			NET_TRANSPORT_RPC,
			N_("List in- and outgoing domain trusts"),
			N_("net rpc trustdom list\n"
			   "    List in- and outgoing domain trusts")
		},
		{
			"vampire",
			rpc_trustdom_vampire,
			NET_TRANSPORT_RPC,
			N_("Vampire trusts from remote server"),
			N_("net rpc trustdom vampire\n"
			   "    Vampire trusts from remote server")
		},
		{NULL, NULL, 0, NULL, NULL}
	};

	return net_run_function(c, argc, argv, "net rpc trustdom", func);
}

/**
 * Check if a server will take rpc commands
 * @param flags	Type of server to connect to (PDC, DMB, localhost)
 *		if the host is not explicitly specified
 * @return  bool (true means rpc supported)
 */
bool net_rpc_check(struct net_context *c, unsigned flags)
{
	struct cli_state *cli;
	bool ret = false;
	struct sockaddr_storage server_ss;
	char *server_name = NULL;
	NTSTATUS status;

	/* flags (i.e. server type) may depend on command */
	if (!net_find_server(c, NULL, flags, &server_ss, &server_name))
		return false;

	if ((cli = cli_initialise()) == NULL) {
		return false;
	}

	status = cli_connect(cli, server_name, &server_ss);
	if (!NT_STATUS_IS_OK(status))
		goto done;
	if (!attempt_netbios_session_request(&cli, global_myname(),
					     server_name, &server_ss))
		goto done;
	status = cli_negprot(cli);
	if (!NT_STATUS_IS_OK(status))
		goto done;
	if (cli->protocol < PROTOCOL_NT1)
		goto done;

	ret = true;
 done:
	cli_shutdown(cli);
	return ret;
}

/* dump sam database via samsync rpc calls */
static int rpc_samdump(struct net_context *c, int argc, const char **argv) {
	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net rpc samdump\n"
			   "    %s\n",
			 _("Usage:"),
			 _("Dump remote SAM database"));
		return 0;
	}

	return run_rpc_command(c, NULL, &ndr_table_netlogon.syntax_id,
			       NET_FLAGS_ANONYMOUS,
			       rpc_samdump_internals, argc, argv);
}

/* syncronise sam database via samsync rpc calls */
static int rpc_vampire(struct net_context *c, int argc, const char **argv)
{
	struct functable func[] = {
		{
			"ldif",
			rpc_vampire_ldif,
			NET_TRANSPORT_RPC,
			N_("Dump remote SAM database to ldif"),
			N_("net rpc vampire ldif\n"
			   "    Dump remote SAM database to LDIF file or "
			   "stdout")
		},
		{
			"keytab",
			rpc_vampire_keytab,
			NET_TRANSPORT_RPC,
			N_("Dump remote SAM database to Kerberos Keytab"),
			N_("net rpc vampire keytab\n"
			   "    Dump remote SAM database to Kerberos keytab "
			   "file")
		},
		{
			"passdb",
			rpc_vampire_passdb,
			NET_TRANSPORT_RPC,
			N_("Dump remote SAM database to passdb"),
			N_("net rpc vampire passdb\n"
			   "    Dump remote SAM database to passdb")
		},

		{NULL, NULL, 0, NULL, NULL}
	};

	if (argc == 0) {
		if (c->display_usage) {
			d_printf(  "%s\n"
				   "net rpc vampire\n"
				   "    %s\n",
				 _("Usage:"),
				 _("Vampire remote SAM database"));
			return 0;
		}

		return rpc_vampire_passdb(c, argc, argv);
	}

	return net_run_function(c, argc, argv, "net rpc vampire", func);
}

/**
 * Migrate everything from a print server.
 *
 * @param c	A net_context structure.
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 *
 * @return A shell status integer (0 for success).
 *
 * The order is important !
 * To successfully add drivers the print queues have to exist !
 * Applying ACLs should be the last step, because you're easily locked out.
 *
 **/
static int rpc_printer_migrate_all(struct net_context *c, int argc,
				   const char **argv)
{
	int ret;

	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net rpc printer migrate all\n"
			   "    %s\n",
			 _("Usage:"),
			 _("Migrate everything from a print server"));
		return 0;
	}

	if (!c->opt_host) {
		d_printf(_("no server to migrate\n"));
		return -1;
	}

	ret = run_rpc_command(c, NULL, &ndr_table_spoolss.syntax_id, 0,
			      rpc_printer_migrate_printers_internals, argc,
			      argv);
	if (ret)
		return ret;

	ret = run_rpc_command(c, NULL, &ndr_table_spoolss.syntax_id, 0,
			      rpc_printer_migrate_drivers_internals, argc,
			      argv);
	if (ret)
		return ret;

	ret = run_rpc_command(c, NULL, &ndr_table_spoolss.syntax_id, 0,
			      rpc_printer_migrate_forms_internals, argc, argv);
	if (ret)
		return ret;

	ret = run_rpc_command(c, NULL, &ndr_table_spoolss.syntax_id, 0,
			      rpc_printer_migrate_settings_internals, argc,
			      argv);
	if (ret)
		return ret;

	return run_rpc_command(c, NULL, &ndr_table_spoolss.syntax_id, 0,
			       rpc_printer_migrate_security_internals, argc,
			       argv);

}

/**
 * Migrate print drivers from a print server.
 *
 * @param c	A net_context structure.
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 *
 * @return A shell status integer (0 for success).
 **/
static int rpc_printer_migrate_drivers(struct net_context *c, int argc,
				       const char **argv)
{
	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net rpc printer migrate drivers\n"
			   "     %s\n",
			 _("Usage:"),
			 _("Migrate print-drivers from a print-server"));
		return 0;
	}

	if (!c->opt_host) {
		d_printf(_("no server to migrate\n"));
		return -1;
	}

	return run_rpc_command(c, NULL, &ndr_table_spoolss.syntax_id, 0,
			       rpc_printer_migrate_drivers_internals,
			       argc, argv);
}

/**
 * Migrate print-forms from a print-server.
 *
 * @param c	A net_context structure.
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 *
 * @return A shell status integer (0 for success).
 **/
static int rpc_printer_migrate_forms(struct net_context *c, int argc,
				     const char **argv)
{
	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net rpc printer migrate forms\n"
			   "    %s\n",
			 _("Usage:"),
			 _("Migrate print-forms from a print-server"));
		return 0;
	}

	if (!c->opt_host) {
		d_printf(_("no server to migrate\n"));
		return -1;
	}

	return run_rpc_command(c, NULL, &ndr_table_spoolss.syntax_id, 0,
			       rpc_printer_migrate_forms_internals,
			       argc, argv);
}

/**
 * Migrate printers from a print-server.
 *
 * @param c	A net_context structure.
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 *
 * @return A shell status integer (0 for success).
 **/
static int rpc_printer_migrate_printers(struct net_context *c, int argc,
					const char **argv)
{
	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net rpc printer migrate printers\n"
			   "    %s\n",
			 _("Usage:"),
			 _("Migrate printers from a print-server"));
		return 0;
	}

	if (!c->opt_host) {
		d_printf(_("no server to migrate\n"));
		return -1;
	}

	return run_rpc_command(c, NULL, &ndr_table_spoolss.syntax_id, 0,
			       rpc_printer_migrate_printers_internals,
			       argc, argv);
}

/**
 * Migrate printer-ACLs from a print-server
 *
 * @param c	A net_context structure.
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 *
 * @return A shell status integer (0 for success).
 **/
static int rpc_printer_migrate_security(struct net_context *c, int argc,
					const char **argv)
{
	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net rpc printer migrate security\n"
			   "    %s\n",
			 _("Usage:"),
			 _("Migrate printer-ACLs from a print-server"));
		return 0;
	}

	if (!c->opt_host) {
		d_printf(_("no server to migrate\n"));
		return -1;
	}

	return run_rpc_command(c, NULL, &ndr_table_spoolss.syntax_id, 0,
			       rpc_printer_migrate_security_internals,
			       argc, argv);
}

/**
 * Migrate printer-settings from a print-server.
 *
 * @param c	A net_context structure.
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 *
 * @return A shell status integer (0 for success).
 **/
static int rpc_printer_migrate_settings(struct net_context *c, int argc,
					const char **argv)
{
	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net rpc printer migrate settings\n"
			    "    %s\n",
			  _("Usage:"),
			  _("Migrate printer-settings from a "
			    "print-server"));
		return 0;
	}

	if (!c->opt_host) {
		d_printf(_("no server to migrate\n"));
		return -1;
	}

	return run_rpc_command(c, NULL, &ndr_table_spoolss.syntax_id, 0,
			       rpc_printer_migrate_settings_internals,
			       argc, argv);
}

/**
 * 'net rpc printer' entrypoint.
 *
 * @param c	A net_context structure.
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 **/

int rpc_printer_migrate(struct net_context *c, int argc, const char **argv)
{

	/* ouch: when addriver and setdriver are called from within
	   rpc_printer_migrate_drivers_internals, the printer-queue already
	   *has* to exist */

	struct functable func[] = {
		{
			"all",
			rpc_printer_migrate_all,
			NET_TRANSPORT_RPC,
			N_("Migrate all from remote to local print server"),
			N_("net rpc printer migrate all\n"
			   "    Migrate all from remote to local print server")
		},
		{
			"drivers",
			rpc_printer_migrate_drivers,
			NET_TRANSPORT_RPC,
			N_("Migrate drivers to local server"),
			N_("net rpc printer migrate drivers\n"
			   "    Migrate drivers to local server")
		},
		{
			"forms",
			rpc_printer_migrate_forms,
			NET_TRANSPORT_RPC,
			N_("Migrate froms to local server"),
			N_("net rpc printer migrate forms\n"
			   "    Migrate froms to local server")
		},
		{
			"printers",
			rpc_printer_migrate_printers,
			NET_TRANSPORT_RPC,
			N_("Migrate printers to local server"),
			N_("net rpc printer migrate printers\n"
			   "    Migrate printers to local server")
		},
		{
			"security",
			rpc_printer_migrate_security,
			NET_TRANSPORT_RPC,
			N_("Mirgate printer ACLs to local server"),
			N_("net rpc printer migrate security\n"
			   "    Mirgate printer ACLs to local server")
		},
		{
			"settings",
			rpc_printer_migrate_settings,
			NET_TRANSPORT_RPC,
			N_("Migrate printer settings to local server"),
			N_("net rpc printer migrate settings\n"
			   "    Migrate printer settings to local server")
		},
		{NULL, NULL, 0, NULL, NULL}
	};

	return net_run_function(c, argc, argv, "net rpc printer migrate",func);
}


/**
 * List printers on a remote RPC server.
 *
 * @param c	A net_context structure.
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 *
 * @return A shell status integer (0 for success).
 **/
static int rpc_printer_list(struct net_context *c, int argc, const char **argv)
{
	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net rpc printer list\n"
			   "    %s\n",
			 _("Usage:"),
			 _("List printers on a remote RPC server"));
		return 0;
	}

	return run_rpc_command(c, NULL, &ndr_table_spoolss.syntax_id, 0,
			       rpc_printer_list_internals,
			       argc, argv);
}

/**
 * List printer-drivers on a remote RPC server.
 *
 * @param c	A net_context structure.
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 *
 * @return A shell status integer (0 for success).
 **/
static int rpc_printer_driver_list(struct net_context *c, int argc,
				   const char **argv)
{
	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net rpc printer driver\n"
			   "    %s\n",
			 _("Usage:"),
			 _("List printer-drivers on a remote RPC server"));
		return 0;
	}

	return run_rpc_command(c, NULL, &ndr_table_spoolss.syntax_id, 0,
			       rpc_printer_driver_list_internals,
			       argc, argv);
}

/**
 * Publish printer in ADS via MSRPC.
 *
 * @param c	A net_context structure.
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 *
 * @return A shell status integer (0 for success).
 **/
static int rpc_printer_publish_publish(struct net_context *c, int argc,
				       const char **argv)
{
	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net rpc printer publish publish\n"
			   "     %s\n",
			 _("Usage:"),
			 _("Publish printer in ADS via MSRPC"));
		return 0;
	}

	return run_rpc_command(c, NULL, &ndr_table_spoolss.syntax_id, 0,
			       rpc_printer_publish_publish_internals,
			       argc, argv);
}

/**
 * Update printer in ADS via MSRPC.
 *
 * @param c	A net_context structure.
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 *
 * @return A shell status integer (0 for success).
 **/
static int rpc_printer_publish_update(struct net_context *c, int argc, const char **argv)
{
	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net rpc printer publish update\n"
			   "    %s\n",
			 _("Usage:"),
			 _("Update printer in ADS via MSRPC"));
		return 0;
	}

	return run_rpc_command(c, NULL, &ndr_table_spoolss.syntax_id, 0,
			       rpc_printer_publish_update_internals,
			       argc, argv);
}

/**
 * UnPublish printer in ADS via MSRPC.
 *
 * @param c	A net_context structure.
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 *
 * @return A shell status integer (0 for success).
 **/
static int rpc_printer_publish_unpublish(struct net_context *c, int argc,
					 const char **argv)
{
	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net rpc printer publish unpublish\n"
			   "    %s\n",
			 _("Usage:\n"),
			 _("UnPublish printer in ADS via MSRPC"));
		return 0;
	}

	return run_rpc_command(c, NULL, &ndr_table_spoolss.syntax_id, 0,
			       rpc_printer_publish_unpublish_internals,
			       argc, argv);
}

/**
 * List published printers via MSRPC.
 *
 * @param c	A net_context structure.
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 *
 * @return A shell status integer (0 for success).
 **/
static int rpc_printer_publish_list(struct net_context *c, int argc,
				    const char **argv)
{
	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net rpc printer publish list\n"
			   "    %s\n",
			 _("Usage:"),
			 _("List published printers via MSRPC"));
		return 0;
	}

	return run_rpc_command(c, NULL, &ndr_table_spoolss.syntax_id, 0,
			       rpc_printer_publish_list_internals,
			       argc, argv);
}


/**
 * Publish printer in ADS.
 *
 * @param c	A net_context structure.
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 *
 * @return A shell status integer (0 for success).
 **/
static int rpc_printer_publish(struct net_context *c, int argc,
			       const char **argv)
{

	struct functable func[] = {
		{
			"publish",
			rpc_printer_publish_publish,
			NET_TRANSPORT_RPC,
			N_("Publish printer in AD"),
			N_("net rpc printer publish publish\n"
			   "    Publish printer in AD")
		},
		{
			"update",
			rpc_printer_publish_update,
			NET_TRANSPORT_RPC,
			N_("Update printer in AD"),
			N_("net rpc printer publish update\n"
			   "    Update printer in AD")
		},
		{
			"unpublish",
			rpc_printer_publish_unpublish,
			NET_TRANSPORT_RPC,
			N_("Unpublish printer"),
			N_("net rpc printer publish unpublish\n"
			   "    Unpublish printer")
		},
		{
			"list",
			rpc_printer_publish_list,
			NET_TRANSPORT_RPC,
			N_("List published printers"),
			N_("net rpc printer publish list\n"
			   "    List published printers")
		},
		{NULL, NULL, 0, NULL, NULL}
	};

	if (argc == 0) {
		if (c->display_usage) {
			d_printf(_("Usage:\n"));
			d_printf(_("net rpc printer publish\n"
				   "    List published printers\n"
				   "    Alias of net rpc printer publish "
				   "list\n"));
			net_display_usage_from_functable(func);
			return 0;
		}
		return run_rpc_command(c, NULL, &ndr_table_spoolss.syntax_id, 0,
			       rpc_printer_publish_list_internals,
			       argc, argv);
	}

	return net_run_function(c, argc, argv, "net rpc printer publish",func);

}


/**
 * Display rpc printer help page.
 *
 * @param c	A net_context structure.
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 **/
int rpc_printer_usage(struct net_context *c, int argc, const char **argv)
{
	d_printf(_("net rpc printer LIST [printer] [misc. options] [targets]\n"
		   "\tlists all printers on print-server\n\n"));
	d_printf(_("net rpc printer DRIVER [printer] [misc. options] [targets]\n"
		   "\tlists all printer-drivers on print-server\n\n"));
	d_printf(_("net rpc printer PUBLISH action [printer] [misc. options] [targets]\n"
		   "\tpublishes printer settings in Active Directory\n"
		   "\taction can be one of PUBLISH, UPDATE, UNPUBLISH or LIST\n\n"));
	d_printf(_("net rpc printer MIGRATE PRINTERS [printer] [misc. options] [targets]"
		   "\n\tmigrates printers from remote to local server\n\n"));
	d_printf(_("net rpc printer MIGRATE SETTINGS [printer] [misc. options] [targets]"
		   "\n\tmigrates printer-settings from remote to local server\n\n"));
	d_printf(_("net rpc printer MIGRATE DRIVERS [printer] [misc. options] [targets]"
		   "\n\tmigrates printer-drivers from remote to local server\n\n"));
	d_printf(_("net rpc printer MIGRATE FORMS [printer] [misc. options] [targets]"
		   "\n\tmigrates printer-forms from remote to local server\n\n"));
	d_printf(_("net rpc printer MIGRATE SECURITY [printer] [misc. options] [targets]"
		   "\n\tmigrates printer-ACLs from remote to local server\n\n"));
	d_printf(_("net rpc printer MIGRATE ALL [printer] [misc. options] [targets]"
		   "\n\tmigrates drivers, forms, queues, settings and acls from\n"
		   "\tremote to local print-server\n\n"));
	net_common_methods_usage(c, argc, argv);
	net_common_flags_usage(c, argc, argv);
	d_printf(_(
	 "\t-v or --verbose\t\t\tgive verbose output\n"
	 "\t      --destination\t\tmigration target server (default: localhost)\n"));

	return -1;
}

/**
 * 'net rpc printer' entrypoint.
 *
 * @param c	A net_context structure.
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 **/
int net_rpc_printer(struct net_context *c, int argc, const char **argv)
{
	struct functable func[] = {
		{
			"list",
			rpc_printer_list,
			NET_TRANSPORT_RPC,
			N_("List all printers on print server"),
			N_("net rpc printer list\n"
			   "    List all printers on print server")
		},
		{
			"migrate",
			rpc_printer_migrate,
			NET_TRANSPORT_RPC,
			N_("Migrate printer to local server"),
			N_("net rpc printer migrate\n"
			   "    Migrate printer to local server")
		},
		{
			"driver",
			rpc_printer_driver_list,
			NET_TRANSPORT_RPC,
			N_("List printer drivers"),
			N_("net rpc printer driver\n"
			   "    List printer drivers")
		},
		{
			"publish",
			rpc_printer_publish,
			NET_TRANSPORT_RPC,
			N_("Publish printer in AD"),
			N_("net rpc printer publish\n"
			   "    Publish printer in AD")
		},
		{NULL, NULL, 0, NULL, NULL}
	};

#ifndef PRINTER_SUPPORT
	return 0;
#endif

	if (argc == 0) {
		if (c->display_usage) {
			d_printf(_("Usage:\n"));
			d_printf(_("net rpc printer\n"
				   "    List printers\n"));
			net_display_usage_from_functable(func);
			return 0;
		}
		return run_rpc_command(c, NULL, &ndr_table_spoolss.syntax_id, 0,
			       rpc_printer_list_internals,
			       argc, argv);
	}

	return net_run_function(c, argc, argv, "net rpc printer", func);
}

/**
 * 'net rpc' entrypoint.
 *
 * @param c	A net_context structure.
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 **/

int net_rpc(struct net_context *c, int argc, const char **argv)
{
	NET_API_STATUS status;

	struct functable func[] = {
		{
			"audit",
			net_rpc_audit,
			NET_TRANSPORT_RPC,
			N_("Modify global audit settings"),
			N_("net rpc audit\n"
			   "    Modify global audit settings")
		},
		{
			"info",
			net_rpc_info,
			NET_TRANSPORT_RPC,
			N_("Show basic info about a domain"),
			N_("net rpc info\n"
			   "    Show basic info about a domain")
		},
		{
			"join",
			net_rpc_join,
			NET_TRANSPORT_RPC,
			N_("Join a domain"),
			N_("net rpc join\n"
			   "    Join a domain")
		},
		{
			"oldjoin",
			net_rpc_oldjoin,
			NET_TRANSPORT_RPC,
			N_("Join a domain created in server manager"),
			N_("net rpc oldjoin\n"
			   "    Join a domain created in server manager")
		},
		{
			"testjoin",
			net_rpc_testjoin,
			NET_TRANSPORT_RPC,
			N_("Test that a join is valid"),
			N_("net rpc testjoin\n"
			   "    Test that a join is valid")
		},
		{
			"user",
			net_rpc_user,
			NET_TRANSPORT_RPC,
			N_("List/modify users"),
			N_("net rpc user\n"
			   "    List/modify users")
		},
		{
			"password",
			rpc_user_password,
			NET_TRANSPORT_RPC,
			N_("Change a user password"),
			N_("net rpc password\n"
			   "    Change a user password\n"
			   "    Alias for net rpc user password")
		},
		{
			"group",
			net_rpc_group,
			NET_TRANSPORT_RPC,
			N_("List/modify groups"),
			N_("net rpc group\n"
			   "    List/modify groups")
		},
		{
			"share",
			net_rpc_share,
			NET_TRANSPORT_RPC,
			N_("List/modify shares"),
			N_("net rpc share\n"
			   "    List/modify shares")
		},
		{
			"file",
			net_rpc_file,
			NET_TRANSPORT_RPC,
			N_("List open files"),
			N_("net rpc file\n"
			   "    List open files")
		},
		{
			"printer",
			net_rpc_printer,
			NET_TRANSPORT_RPC,
			N_("List/modify printers"),
			N_("net rpc printer\n"
			   "    List/modify printers")
		},
		{
			"changetrustpw",
			net_rpc_changetrustpw,
			NET_TRANSPORT_RPC,
			N_("Change trust account password"),
			N_("net rpc changetrustpw\n"
			   "    Change trust account password")
		},
		{
			"trustdom",
			rpc_trustdom,
			NET_TRANSPORT_RPC,
			N_("Modify domain trusts"),
			N_("net rpc trustdom\n"
			   "    Modify domain trusts")
		},
		{
			"abortshutdown",
			rpc_shutdown_abort,
			NET_TRANSPORT_RPC,
			N_("Abort a remote shutdown"),
			N_("net rpc abortshutdown\n"
			   "    Abort a remote shutdown")
		},
		{
			"shutdown",
			rpc_shutdown,
			NET_TRANSPORT_RPC,
			N_("Shutdown a remote server"),
			N_("net rpc shutdown\n"
			   "    Shutdown a remote server")
		},
		{
			"samdump",
			rpc_samdump,
			NET_TRANSPORT_RPC,
			N_("Dump SAM data of remote NT PDC"),
			N_("net rpc samdump\n"
			   "    Dump SAM data of remote NT PDC")
		},
		{
			"vampire",
			rpc_vampire,
			NET_TRANSPORT_RPC,
			N_("Sync a remote NT PDC's data into local passdb"),
			N_("net rpc vampire\n"
			   "    Sync a remote NT PDC's data into local passdb")
		},
		{
			"getsid",
			net_rpc_getsid,
			NET_TRANSPORT_RPC,
			N_("Fetch the domain sid into local secrets.tdb"),
			N_("net rpc getsid\n"
			   "    Fetch the domain sid into local secrets.tdb")
		},
		{
			"rights",
			net_rpc_rights,
			NET_TRANSPORT_RPC,
			N_("Manage privileges assigned to SID"),
			N_("net rpc rights\n"
			   "    Manage privileges assigned to SID")
		},
		{
			"service",
			net_rpc_service,
			NET_TRANSPORT_RPC,
			N_("Start/stop/query remote services"),
			N_("net rpc service\n"
			   "    Start/stop/query remote services")
		},
		{
			"registry",
			net_rpc_registry,
			NET_TRANSPORT_RPC,
			N_("Manage registry hives"),
			N_("net rpc registry\n"
			   "    Manage registry hives")
		},
		{
			"shell",
			net_rpc_shell,
			NET_TRANSPORT_RPC,
			N_("Open interactive shell on remote server"),
			N_("net rpc shell\n"
			   "    Open interactive shell on remote server")
		},
		{
			"trust",
			net_rpc_trust,
			NET_TRANSPORT_RPC,
			N_("Manage trusts"),
			N_("net rpc trust\n"
			   "    Manage trusts")
		},
		{NULL, NULL, 0, NULL, NULL}
	};

	status = libnetapi_net_init(&c->netapi_ctx);
	if (status != 0) {
		return -1;
	}
	libnetapi_set_username(c->netapi_ctx, c->opt_user_name);
	libnetapi_set_password(c->netapi_ctx, c->opt_password);
	if (c->opt_kerberos) {
		libnetapi_set_use_kerberos(c->netapi_ctx);
	}
	if (c->opt_ccache) {
		libnetapi_set_use_ccache(c->netapi_ctx);
	}

	return net_run_function(c, argc, argv, "net rpc", func);
}
