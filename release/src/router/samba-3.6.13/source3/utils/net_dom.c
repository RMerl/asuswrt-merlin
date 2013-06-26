/*
   Samba Unix/Linux SMB client library
   net dom commands for remote join/unjoin
   Copyright (C) 2007,2009 Günther Deschner

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
#include "utils/net.h"
#include "../librpc/gen_ndr/ndr_initshutdown.h"
#include "../librpc/gen_ndr/ndr_winreg.h"
#include "lib/netapi/netapi.h"
#include "lib/netapi/netapi_net.h"
#include "libsmb/libsmb.h"

int net_dom_usage(struct net_context *c, int argc, const char **argv)
{
	d_printf("%s\n%s",
		_("Usage:"),
		_("net dom join "
		   "<domain=DOMAIN> <ou=OU> <account=ACCOUNT> "
		   "<password=PASSWORD> <reboot>\n  Join a remote machine\n"));
	d_printf("%s\n%s",
		 _("Usage:"),
		 _("net dom unjoin "
		   "<account=ACCOUNT> <password=PASSWORD> <reboot>\n"
		   "  Unjoin a remote machine\n"));
	d_printf("%s\n%s",
		 _("Usage:"),
		 _("net dom renamecomputer "
		   "<newname=NEWNAME> "
		   "<account=ACCOUNT> <password=PASSWORD> <reboot>\n"
		   "  Rename joined computer\n"));

	return -1;
}

static int net_dom_unjoin(struct net_context *c, int argc, const char **argv)
{
	const char *server_name = NULL;
	const char *account = NULL;
	const char *password = NULL;
	uint32_t unjoin_flags = NETSETUP_ACCT_DELETE |
				NETSETUP_JOIN_DOMAIN |
				NETSETUP_IGNORE_UNSUPPORTED_FLAGS;
	struct cli_state *cli = NULL;
	bool do_reboot = false;
	NTSTATUS ntstatus;
	NET_API_STATUS status;
	int ret = -1;
	int i;

	if (argc < 1 || c->display_usage) {
		return net_dom_usage(c, argc, argv);
	}

	if (c->opt_host) {
		server_name = c->opt_host;
	}

	for (i=0; i<argc; i++) {
		if (strnequal(argv[i], "account", strlen("account"))) {
			account = get_string_param(argv[i]);
			if (!account) {
				return -1;
			}
		}
		if (strnequal(argv[i], "password", strlen("password"))) {
			password = get_string_param(argv[i]);
			if (!password) {
				return -1;
			}
		}
		if (strequal(argv[i], "reboot")) {
			do_reboot = true;
		}
	}

	if (do_reboot) {
		ntstatus = net_make_ipc_connection_ex(c, c->opt_workgroup,
						      server_name, NULL, 0,
						      &cli);
		if (!NT_STATUS_IS_OK(ntstatus)) {
			return -1;
		}
	}

	status = NetUnjoinDomain(server_name, account, password, unjoin_flags);
	if (status != 0) {
		printf(_("Failed to unjoin domain: %s\n"),
			libnetapi_get_error_string(c->netapi_ctx, status));
		goto done;
	}

	if (do_reboot) {
		c->opt_comment = _("Shutting down due to a domain membership "
				   "change");
		c->opt_reboot = true;
		c->opt_timeout = 30;

		ret = run_rpc_command(c, cli,
				      &ndr_table_initshutdown.syntax_id,
				      0, rpc_init_shutdown_internals,
				      argc, argv);
		if (ret == 0) {
			goto done;
		}

		ret = run_rpc_command(c, cli, &ndr_table_winreg.syntax_id, 0,
				      rpc_reg_shutdown_internals,
				      argc, argv);
		goto done;
	}

	ret = 0;

 done:
	if (cli) {
		cli_shutdown(cli);
	}

	return ret;
}

static int net_dom_join(struct net_context *c, int argc, const char **argv)
{
	const char *server_name = NULL;
	const char *domain_name = NULL;
	const char *account_ou = NULL;
	const char *Account = NULL;
	const char *password = NULL;
	uint32_t join_flags = NETSETUP_ACCT_CREATE |
			      NETSETUP_JOIN_DOMAIN;
	struct cli_state *cli = NULL;
	bool do_reboot = false;
	NTSTATUS ntstatus;
	NET_API_STATUS status;
	int ret = -1;
	int i;

	if (argc < 1 || c->display_usage) {
		return net_dom_usage(c, argc, argv);
	}

	if (c->opt_host) {
		server_name = c->opt_host;
	}

	if (c->opt_force) {
		join_flags |= NETSETUP_DOMAIN_JOIN_IF_JOINED;
	}

	for (i=0; i<argc; i++) {
		if (strnequal(argv[i], "ou", strlen("ou"))) {
			account_ou = get_string_param(argv[i]);
			if (!account_ou) {
				return -1;
			}
		}
		if (strnequal(argv[i], "domain", strlen("domain"))) {
			domain_name = get_string_param(argv[i]);
			if (!domain_name) {
				return -1;
			}
		}
		if (strnequal(argv[i], "account", strlen("account"))) {
			Account = get_string_param(argv[i]);
			if (!Account) {
				return -1;
			}
		}
		if (strnequal(argv[i], "password", strlen("password"))) {
			password = get_string_param(argv[i]);
			if (!password) {
				return -1;
			}
		}
		if (strequal(argv[i], "reboot")) {
			do_reboot = true;
		}
	}

	if (do_reboot) {
		ntstatus = net_make_ipc_connection_ex(c, c->opt_workgroup,
						      server_name, NULL, 0,
						      &cli);
		if (!NT_STATUS_IS_OK(ntstatus)) {
			return -1;
		}
	}

	/* check if domain is a domain or a workgroup */

	status = NetJoinDomain(server_name, domain_name, account_ou,
			       Account, password, join_flags);
	if (status != 0) {
		printf(_("Failed to join domain: %s\n"),
			libnetapi_get_error_string(c->netapi_ctx, status));
		goto done;
	}

	if (do_reboot) {
		c->opt_comment = _("Shutting down due to a domain membership "
				   "change");
		c->opt_reboot = true;
		c->opt_timeout = 30;

		ret = run_rpc_command(c, cli, &ndr_table_initshutdown.syntax_id, 0,
				      rpc_init_shutdown_internals,
				      argc, argv);
		if (ret == 0) {
			goto done;
		}

		ret = run_rpc_command(c, cli, &ndr_table_winreg.syntax_id, 0,
				      rpc_reg_shutdown_internals,
				      argc, argv);
		goto done;
	}

	ret = 0;

 done:
	if (cli) {
		cli_shutdown(cli);
	}

	return ret;
}

static int net_dom_renamecomputer(struct net_context *c, int argc, const char **argv)
{
	const char *server_name = NULL;
	const char *account = NULL;
	const char *password = NULL;
	const char *newname = NULL;
	uint32_t rename_options = NETSETUP_ACCT_CREATE;
	struct cli_state *cli = NULL;
	bool do_reboot = false;
	NTSTATUS ntstatus;
	NET_API_STATUS status;
	int ret = -1;
	int i;

	if (argc < 1 || c->display_usage) {
		return net_dom_usage(c, argc, argv);
	}

	if (c->opt_host) {
		server_name = c->opt_host;
	}

	for (i=0; i<argc; i++) {
		if (strnequal(argv[i], "account", strlen("account"))) {
			account = get_string_param(argv[i]);
			if (!account) {
				return -1;
			}
		}
		if (strnequal(argv[i], "password", strlen("password"))) {
			password = get_string_param(argv[i]);
			if (!password) {
				return -1;
			}
		}
		if (strnequal(argv[i], "newname", strlen("newname"))) {
			newname = get_string_param(argv[i]);
			if (!newname) {
				return -1;
			}
		}
		if (strequal(argv[i], "reboot")) {
			do_reboot = true;
		}
	}

	if (do_reboot) {
		ntstatus = net_make_ipc_connection_ex(c, c->opt_workgroup,
						      server_name, NULL, 0,
						      &cli);
		if (!NT_STATUS_IS_OK(ntstatus)) {
			return -1;
		}
	}

	status = NetRenameMachineInDomain(server_name, newname,
					  account, password, rename_options);
	if (status != 0) {
		printf(_("Failed to rename machine: "));
		if (status == W_ERROR_V(WERR_SETUP_NOT_JOINED)) {
			printf(_("Computer is not joined to a Domain\n"));
			goto done;
		}
		printf("%s\n",
			libnetapi_get_error_string(c->netapi_ctx, status));
		goto done;
	}

	if (do_reboot) {
		c->opt_comment = _("Shutting down due to a computer rename");
		c->opt_reboot = true;
		c->opt_timeout = 30;

		ret = run_rpc_command(c, cli,
				      &ndr_table_initshutdown.syntax_id,
				      0, rpc_init_shutdown_internals,
				      argc, argv);
		if (ret == 0) {
			goto done;
		}

		ret = run_rpc_command(c, cli, &ndr_table_winreg.syntax_id, 0,
				      rpc_reg_shutdown_internals,
				      argc, argv);
		goto done;
	}

	ret = 0;

 done:
	if (cli) {
		cli_shutdown(cli);
	}

	return ret;
}

int net_dom(struct net_context *c, int argc, const char **argv)
{
	NET_API_STATUS status;

	struct functable func[] = {
		{
			"join",
			net_dom_join,
			NET_TRANSPORT_LOCAL,
			N_("Join a remote machine"),
			N_("net dom join <domain=DOMAIN> <ou=OU> "
			   "<account=ACCOUNT> <password=PASSWORD> <reboot>\n"
			   "  Join a remote machine")
		},
		{
			"unjoin",
			net_dom_unjoin,
			NET_TRANSPORT_LOCAL,
			N_("Unjoin a remote machine"),
			N_("net dom unjoin <account=ACCOUNT> "
			   "<password=PASSWORD> <reboot>\n"
			   "  Unjoin a remote machine")
		},
		{
			"renamecomputer",
			net_dom_renamecomputer,
			NET_TRANSPORT_LOCAL,
			N_("Rename a computer that is joined to a domain"),
			N_("net dom renamecomputer <newname=NEWNAME> "
			   "<account=ACCOUNT> <password=PASSWORD> "
			   "<reboot>\n"
			   "  Rename joined computer")
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

	return net_run_function(c, argc, argv, "net dom", func);
}
