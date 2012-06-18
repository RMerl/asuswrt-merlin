/*
 *  Unix SMB/CIFS implementation.
 *  Helper routines for net
 *  Copyright (C) Volker Lendecke 2006
 *  Copyright (C) Kai Blin 2008
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
#include "utils/net.h"
#include "../librpc/gen_ndr/cli_lsa.h"
#include "../librpc/gen_ndr/cli_dssetup.h"

NTSTATUS net_rpc_lookup_name(struct net_context *c,
			     TALLOC_CTX *mem_ctx, struct cli_state *cli,
			     const char *name, const char **ret_domain,
			     const char **ret_name, DOM_SID *ret_sid,
			     enum lsa_SidType *ret_type)
{
	struct rpc_pipe_client *lsa_pipe = NULL;
	struct policy_handle pol;
	NTSTATUS result = NT_STATUS_OK;
	const char **dom_names;
	DOM_SID *sids;
	enum lsa_SidType *types;

	ZERO_STRUCT(pol);

	result = cli_rpc_pipe_open_noauth(cli, &ndr_table_lsarpc.syntax_id,
					  &lsa_pipe);
	if (!NT_STATUS_IS_OK(result)) {
		d_fprintf(stderr, _("Could not initialise lsa pipe\n"));
		return result;
	}

	result = rpccli_lsa_open_policy(lsa_pipe, mem_ctx, false,
					SEC_FLAG_MAXIMUM_ALLOWED,
					&pol);
	if (!NT_STATUS_IS_OK(result)) {
		d_fprintf(stderr, "open_policy %s: %s\n", _("failed"),
			  nt_errstr(result));
		return result;
	}

	result = rpccli_lsa_lookup_names(lsa_pipe, mem_ctx, &pol, 1,
					 &name, &dom_names, 1, &sids, &types);

	if (!NT_STATUS_IS_OK(result)) {
		/* This can happen easily, don't log an error */
		goto done;
	}

	if (ret_domain != NULL) {
		*ret_domain = dom_names[0];
	}
	if (ret_name != NULL) {
		*ret_name = talloc_strdup(mem_ctx, name);
	}
	if (ret_sid != NULL) {
		sid_copy(ret_sid, &sids[0]);
	}
	if (ret_type != NULL) {
		*ret_type = types[0];
	}

 done:
	if (is_valid_policy_hnd(&pol)) {
		rpccli_lsa_Close(lsa_pipe, mem_ctx, &pol);
	}
	TALLOC_FREE(lsa_pipe);

	return result;
}

/****************************************************************************
 Connect to \\server\service.
****************************************************************************/

NTSTATUS connect_to_service(struct net_context *c,
					struct cli_state **cli_ctx,
					struct sockaddr_storage *server_ss,
					const char *server_name,
					const char *service_name,
					const char *service_type)
{
	NTSTATUS nt_status;
	int flags = 0;

	c->opt_password = net_prompt_pass(c, c->opt_user_name);

	if (c->opt_kerberos) {
		flags |= CLI_FULL_CONNECTION_USE_KERBEROS;
	}

	if (c->opt_kerberos && c->opt_password) {
		flags |= CLI_FULL_CONNECTION_FALLBACK_AFTER_KERBEROS;
	}

	if (c->opt_ccache) {
		flags |= CLI_FULL_CONNECTION_USE_CCACHE;
	}

	nt_status = cli_full_connection(cli_ctx, NULL, server_name,
					server_ss, c->opt_port,
					service_name, service_type,
					c->opt_user_name, c->opt_workgroup,
					c->opt_password, flags, Undefined, NULL);
	if (!NT_STATUS_IS_OK(nt_status)) {
		d_fprintf(stderr, _("Could not connect to server %s\n"),
			  server_name);

		/* Display a nicer message depending on the result */

		if (NT_STATUS_V(nt_status) ==
		    NT_STATUS_V(NT_STATUS_LOGON_FAILURE))
			d_fprintf(stderr,
				  _("The username or password was not "
				    "correct.\n"));

		if (NT_STATUS_V(nt_status) ==
		    NT_STATUS_V(NT_STATUS_ACCOUNT_LOCKED_OUT))
			d_fprintf(stderr, _("The account was locked out.\n"));

		if (NT_STATUS_V(nt_status) ==
		    NT_STATUS_V(NT_STATUS_ACCOUNT_DISABLED))
			d_fprintf(stderr, _("The account was disabled.\n"));
		return nt_status;
	}

	if (c->smb_encrypt) {
		nt_status = cli_force_encryption(*cli_ctx,
					c->opt_user_name,
					c->opt_password,
					c->opt_workgroup);

		if (NT_STATUS_EQUAL(nt_status,NT_STATUS_NOT_SUPPORTED)) {
			d_printf(_("Encryption required and "
				"server that doesn't support "
				"UNIX extensions - failing connect\n"));
		} else if (NT_STATUS_EQUAL(nt_status,NT_STATUS_UNKNOWN_REVISION)) {
			d_printf(_("Encryption required and "
				"can't get UNIX CIFS extensions "
				"version from server.\n"));
		} else if (NT_STATUS_EQUAL(nt_status,NT_STATUS_UNSUPPORTED_COMPRESSION)) {
			d_printf(_("Encryption required and "
				"share %s doesn't support "
				"encryption.\n"), service_name);
		} else if (!NT_STATUS_IS_OK(nt_status)) {
			d_printf(_("Encryption required and "
				"setup failed with error %s.\n"),
				nt_errstr(nt_status));
		}

		if (!NT_STATUS_IS_OK(nt_status)) {
			cli_shutdown(*cli_ctx);
			*cli_ctx = NULL;
		}
	}

	return nt_status;
}

/****************************************************************************
 Connect to \\server\ipc$.
****************************************************************************/

NTSTATUS connect_to_ipc(struct net_context *c,
			struct cli_state **cli_ctx,
			struct sockaddr_storage *server_ss,
			const char *server_name)
{
	return connect_to_service(c, cli_ctx, server_ss, server_name, "IPC$",
				  "IPC");
}

/****************************************************************************
 Connect to \\server\ipc$ anonymously.
****************************************************************************/

NTSTATUS connect_to_ipc_anonymous(struct net_context *c,
				struct cli_state **cli_ctx,
				struct sockaddr_storage *server_ss,
				const char *server_name)
{
	NTSTATUS nt_status;

	nt_status = cli_full_connection(cli_ctx, c->opt_requester_name,
					server_name, server_ss, c->opt_port,
					"IPC$", "IPC",
					"", "",
					"", 0, Undefined, NULL);

	if (NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	} else {
		DEBUG(1,("Cannot connect to server (anonymously).  Error was %s\n", nt_errstr(nt_status)));
		return nt_status;
	}
}

/****************************************************************************
 Return malloced user@realm for krb5 login.
****************************************************************************/

static char *get_user_and_realm(const char *username)
{
	char *user_and_realm = NULL;

	if (!username) {
		return NULL;
	}
	if (strchr_m(username, '@')) {
		user_and_realm = SMB_STRDUP(username);
	} else {
		if (asprintf(&user_and_realm, "%s@%s", username, lp_realm()) == -1) {
			user_and_realm = NULL;
		}
	}
	return user_and_realm;
}

/****************************************************************************
 Connect to \\server\ipc$ using KRB5.
****************************************************************************/

NTSTATUS connect_to_ipc_krb5(struct net_context *c,
			struct cli_state **cli_ctx,
			struct sockaddr_storage *server_ss,
			const char *server_name)
{
	NTSTATUS nt_status;
	char *user_and_realm = NULL;

	/* FIXME: Should get existing kerberos ticket if possible. */
	c->opt_password = net_prompt_pass(c, c->opt_user_name);
	if (!c->opt_password) {
		return NT_STATUS_NO_MEMORY;
	}

	user_and_realm = get_user_and_realm(c->opt_user_name);
	if (!user_and_realm) {
		return NT_STATUS_NO_MEMORY;
	}

	nt_status = cli_full_connection(cli_ctx, NULL, server_name,
					server_ss, c->opt_port,
					"IPC$", "IPC",
					user_and_realm, c->opt_workgroup,
					c->opt_password,
					CLI_FULL_CONNECTION_USE_KERBEROS,
					Undefined, NULL);

	SAFE_FREE(user_and_realm);

	if (!NT_STATUS_IS_OK(nt_status)) {
		DEBUG(1,("Cannot connect to server using kerberos.  Error was %s\n", nt_errstr(nt_status)));
		return nt_status;
	}

        if (c->smb_encrypt) {
		nt_status = cli_cm_force_encryption(*cli_ctx,
					user_and_realm,
					c->opt_password,
					c->opt_workgroup,
                                        "IPC$");
		if (!NT_STATUS_IS_OK(nt_status)) {
			cli_shutdown(*cli_ctx);
			*cli_ctx = NULL;
		}
	}

	return nt_status;
}

/**
 * Connect a server and open a given pipe
 *
 * @param cli_dst		A cli_state
 * @param pipe			The pipe to open
 * @param got_pipe		boolean that stores if we got a pipe
 *
 * @return Normal NTSTATUS return.
 **/
NTSTATUS connect_dst_pipe(struct net_context *c, struct cli_state **cli_dst,
			  struct rpc_pipe_client **pp_pipe_hnd,
			  const struct ndr_syntax_id *interface)
{
	NTSTATUS nt_status;
	char *server_name = SMB_STRDUP("127.0.0.1");
	struct cli_state *cli_tmp = NULL;
	struct rpc_pipe_client *pipe_hnd = NULL;

	if (server_name == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	if (c->opt_destination) {
		SAFE_FREE(server_name);
		if ((server_name = SMB_STRDUP(c->opt_destination)) == NULL) {
			return NT_STATUS_NO_MEMORY;
		}
	}

	/* make a connection to a named pipe */
	nt_status = connect_to_ipc(c, &cli_tmp, NULL, server_name);
	if (!NT_STATUS_IS_OK(nt_status)) {
		SAFE_FREE(server_name);
		return nt_status;
	}

	nt_status = cli_rpc_pipe_open_noauth(cli_tmp, interface,
					     &pipe_hnd);
	if (!NT_STATUS_IS_OK(nt_status)) {
		DEBUG(0, ("couldn't not initialize pipe\n"));
		cli_shutdown(cli_tmp);
		SAFE_FREE(server_name);
		return nt_status;
	}

	*cli_dst = cli_tmp;
	*pp_pipe_hnd = pipe_hnd;
	SAFE_FREE(server_name);

	return nt_status;
}

/****************************************************************************
 Use the local machine account (krb) and password for this session.
****************************************************************************/

int net_use_krb_machine_account(struct net_context *c)
{
	char *user_name = NULL;

	if (!secrets_init()) {
		d_fprintf(stderr,_("ERROR: Unable to open secrets database\n"));
		exit(1);
	}

	c->opt_password = secrets_fetch_machine_password(
				c->opt_target_workgroup, NULL, NULL);
	if (asprintf(&user_name, "%s$@%s", global_myname(), lp_realm()) == -1) {
		return -1;
	}
	c->opt_user_name = user_name;
	return 0;
}

/****************************************************************************
 Use the machine account name and password for this session.
****************************************************************************/

int net_use_machine_account(struct net_context *c)
{
	char *user_name = NULL;

	if (!secrets_init()) {
		d_fprintf(stderr,_("ERROR: Unable to open secrets database\n"));
		exit(1);
	}

	c->opt_password = secrets_fetch_machine_password(
				c->opt_target_workgroup, NULL, NULL);
	if (asprintf(&user_name, "%s$", global_myname()) == -1) {
		return -1;
	}
	c->opt_user_name = user_name;
	return 0;
}

bool net_find_server(struct net_context *c,
			const char *domain,
			unsigned flags,
			struct sockaddr_storage *server_ss,
			char **server_name)
{
	const char *d = domain ? domain : c->opt_target_workgroup;

	if (c->opt_host) {
		*server_name = SMB_STRDUP(c->opt_host);
	}

	if (c->opt_have_ip) {
		*server_ss = c->opt_dest_ip;
		if (!*server_name) {
			char addr[INET6_ADDRSTRLEN];
			print_sockaddr(addr, sizeof(addr), &c->opt_dest_ip);
			*server_name = SMB_STRDUP(addr);
		}
	} else if (*server_name) {
		/* resolve the IP address */
		if (!resolve_name(*server_name, server_ss, 0x20, false))  {
			DEBUG(1,("Unable to resolve server name\n"));
			return false;
		}
	} else if (flags & NET_FLAGS_PDC) {
		fstring dc_name;
		struct sockaddr_storage pdc_ss;

		if (!get_pdc_ip(d, &pdc_ss)) {
			DEBUG(1,("Unable to resolve PDC server address\n"));
			return false;
		}

		if (is_zero_addr((struct sockaddr *)&pdc_ss)) {
			return false;
		}

		if (!name_status_find(d, 0x1b, 0x20, &pdc_ss, dc_name)) {
			return false;
		}

		*server_name = SMB_STRDUP(dc_name);
		*server_ss = pdc_ss;
	} else if (flags & NET_FLAGS_DMB) {
		struct sockaddr_storage msbrow_ss;
		char addr[INET6_ADDRSTRLEN];

		/*  if (!resolve_name(MSBROWSE, &msbrow_ip, 1, false)) */
		if (!resolve_name(d, &msbrow_ss, 0x1B, false))  {
			DEBUG(1,("Unable to resolve domain browser via name lookup\n"));
			return false;
		}
		*server_ss = msbrow_ss;
		print_sockaddr(addr, sizeof(addr), server_ss);
		*server_name = SMB_STRDUP(addr);
	} else if (flags & NET_FLAGS_MASTER) {
		struct sockaddr_storage brow_ss;
		char addr[INET6_ADDRSTRLEN];
		if (!resolve_name(d, &brow_ss, 0x1D, false))  {
				/* go looking for workgroups */
			DEBUG(1,("Unable to resolve master browser via name lookup\n"));
			return false;
		}
		*server_ss = brow_ss;
		print_sockaddr(addr, sizeof(addr), server_ss);
		*server_name = SMB_STRDUP(addr);
	} else if (!(flags & NET_FLAGS_LOCALHOST_DEFAULT_INSANE)) {
		if (!interpret_string_addr(server_ss,
					"127.0.0.1", AI_NUMERICHOST)) {
			DEBUG(1,("Unable to resolve 127.0.0.1\n"));
			return false;
		}
		*server_name = SMB_STRDUP("127.0.0.1");
	}

	if (!*server_name) {
		DEBUG(1,("no server to connect to\n"));
		return false;
	}

	return true;
}

bool net_find_pdc(struct sockaddr_storage *server_ss,
		fstring server_name,
		const char *domain_name)
{
	if (!get_pdc_ip(domain_name, server_ss)) {
		return false;
	}
	if (is_zero_addr((struct sockaddr *)server_ss)) {
		return false;
	}

	if (!name_status_find(domain_name, 0x1b, 0x20, server_ss, server_name)) {
		return false;
	}

	return true;
}

NTSTATUS net_make_ipc_connection(struct net_context *c, unsigned flags,
				 struct cli_state **pcli)
{
	return net_make_ipc_connection_ex(c, c->opt_workgroup, NULL, NULL, flags, pcli);
}

NTSTATUS net_make_ipc_connection_ex(struct net_context *c ,const char *domain,
				    const char *server,
				    struct sockaddr_storage *pss,
				    unsigned flags, struct cli_state **pcli)
{
	char *server_name = NULL;
	struct sockaddr_storage server_ss;
	struct cli_state *cli = NULL;
	NTSTATUS nt_status;

	if ( !server || !pss ) {
		if (!net_find_server(c, domain, flags, &server_ss,
				     &server_name)) {
			d_fprintf(stderr, _("Unable to find a suitable server "
				"for domain %s\n"), domain);
			nt_status = NT_STATUS_UNSUCCESSFUL;
			goto done;
		}
	} else {
		server_name = SMB_STRDUP( server );
		server_ss = *pss;
	}

	if (flags & NET_FLAGS_ANONYMOUS) {
		nt_status = connect_to_ipc_anonymous(c, &cli, &server_ss,
						     server_name);
	} else {
		nt_status = connect_to_ipc(c, &cli, &server_ss,
					   server_name);
	}

	/* store the server in the affinity cache if it was a PDC */

	if ( (flags & NET_FLAGS_PDC) && NT_STATUS_IS_OK(nt_status) )
		saf_store( cli->server_domain, cli->desthost );

	SAFE_FREE(server_name);
	if (!NT_STATUS_IS_OK(nt_status)) {
		d_fprintf(stderr, _("Connection failed: %s\n"),
			  nt_errstr(nt_status));
		cli = NULL;
	} else if (c->opt_request_timeout) {
		cli_set_timeout(cli, c->opt_request_timeout * 1000);
	}

done:
	if (pcli != NULL) {
		*pcli = cli;
	}
	return nt_status;
}

/****************************************************************************
****************************************************************************/

const char *net_prompt_pass(struct net_context *c, const char *user)
{
	char *prompt = NULL;
	const char *pass = NULL;

	if (c->opt_password) {
		return c->opt_password;
	}

	if (c->opt_machine_pass) {
		return NULL;
	}

	if (c->opt_kerberos && !c->opt_user_specified) {
		return NULL;
	}

	if (asprintf(&prompt, _("Enter %s's password:"), user) == -1) {
		return NULL;
	}

	pass = getpass(prompt);
	SAFE_FREE(prompt);

	return pass;
}

int net_run_function(struct net_context *c, int argc, const char **argv,
		      const char *whoami, struct functable *table)
{
	int i;

	if (argc != 0) {
		for (i=0; table[i].funcname != NULL; i++) {
			if (StrCaseCmp(argv[0], table[i].funcname) == 0)
				return table[i].fn(c, argc-1, argv+1);
		}
	}

	if (c->display_usage == false) {
		d_fprintf(stderr, _("Invalid command: %s %s\n"), whoami,
			  (argc > 0)?argv[0]:"");
	}
	d_printf(_("Usage:\n"));
	for (i=0; table[i].funcname != NULL; i++) {
		if(c->display_usage == false)
			d_printf("%s %-15s %s\n", whoami, table[i].funcname,
				 _(table[i].description));
		else
			d_printf("%s\n", _(table[i].usage));
	}

	return c->display_usage?0:-1;
}

void net_display_usage_from_functable(struct functable *table)
{
	int i;
	for (i=0; table[i].funcname != NULL; i++) {
		d_printf("%s\n", _(table[i].usage));
	}
}

const char *net_share_type_str(int num_type)
{
	switch(num_type) {
		case 0: return _("Disk");
		case 1: return _("Print");
		case 2: return _("Dev");
		case 3: return _("IPC");
		default: return _("Unknown");
	}
}

NTSTATUS net_scan_dc(struct net_context *c,
		     struct cli_state *cli,
		     struct net_dc_info *dc_info)
{
	TALLOC_CTX *mem_ctx = talloc_tos();
	struct rpc_pipe_client *dssetup_pipe = NULL;
	union dssetup_DsRoleInfo info;
	NTSTATUS status;

	ZERO_STRUCTP(dc_info);

	status = cli_rpc_pipe_open_noauth(cli, &ndr_table_dssetup.syntax_id,
					  &dssetup_pipe);
        if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = rpccli_dssetup_DsRoleGetPrimaryDomainInformation(dssetup_pipe, mem_ctx,
								  DS_ROLE_BASIC_INFORMATION,
								  &info,
								  NULL);
	TALLOC_FREE(dssetup_pipe);

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	dc_info->is_dc	= (info.basic.role & (DS_ROLE_PRIMARY_DC|DS_ROLE_BACKUP_DC));
	dc_info->is_pdc	= (info.basic.role & DS_ROLE_PRIMARY_DC);
	dc_info->is_ad	= (info.basic.flags & DS_ROLE_PRIMARY_DS_RUNNING);
	dc_info->is_mixed_mode = (info.basic.flags & DS_ROLE_PRIMARY_DS_MIXED_MODE);
	dc_info->netbios_domain_name = talloc_strdup(mem_ctx, info.basic.domain);
	dc_info->dns_domain_name = talloc_strdup(mem_ctx, info.basic.dns_domain);
	dc_info->forest_name = talloc_strdup(mem_ctx, info.basic.forest);

	return NT_STATUS_OK;
}
