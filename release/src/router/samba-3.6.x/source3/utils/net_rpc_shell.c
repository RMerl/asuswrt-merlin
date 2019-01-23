/*
 *  Unix SMB/CIFS implementation.
 *  Shell around net rpc subcommands
 *  Copyright (C) Volker Lendecke 2006
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
#include "popt_common.h"
#include "utils/net.h"
#include "rpc_client/cli_pipe.h"
#include "../librpc/gen_ndr/ndr_samr.h"
#include "lib/netapi/netapi.h"
#include "lib/netapi/netapi_net.h"
#include "../libcli/smbreadline/smbreadline.h"
#include "libsmb/libsmb.h"

static NTSTATUS rpc_sh_info(struct net_context *c,
			    TALLOC_CTX *mem_ctx, struct rpc_sh_ctx *ctx,
			    struct rpc_pipe_client *pipe_hnd,
			    int argc, const char **argv)
{
	return rpc_info_internals(c, ctx->domain_sid, ctx->domain_name,
				  ctx->cli, pipe_hnd, mem_ctx,
				  argc, argv);
}

static struct rpc_sh_ctx *this_ctx;

static char **completion_fn(const char *text, int start, int end)
{
	char **cmds = NULL;
	int n_cmds = 0;
	struct rpc_sh_cmd *c;

	if (start != 0) {
		return NULL;
	}

	ADD_TO_ARRAY(NULL, char *, SMB_STRDUP(text), &cmds, &n_cmds);

	for (c = this_ctx->cmds; c->name != NULL; c++) {
		bool match = (strncmp(text, c->name, strlen(text)) == 0);

		if (match) {
			ADD_TO_ARRAY(NULL, char *, SMB_STRDUP(c->name),
				     &cmds, &n_cmds);
		}
	}

	if (n_cmds == 2) {
		SAFE_FREE(cmds[0]);
		cmds[0] = cmds[1];
		n_cmds -= 1;
	}

	ADD_TO_ARRAY(NULL, char *, NULL, &cmds, &n_cmds);
	return cmds;
}

static NTSTATUS net_sh_run(struct net_context *c,
			   struct rpc_sh_ctx *ctx, struct rpc_sh_cmd *cmd,
			   int argc, const char **argv)
{
	TALLOC_CTX *mem_ctx;
	struct rpc_pipe_client *pipe_hnd = NULL;
	NTSTATUS status;

	mem_ctx = talloc_new(ctx);
	if (mem_ctx == NULL) {
		d_fprintf(stderr, _("talloc_new failed\n"));
		return NT_STATUS_NO_MEMORY;
	}

	status = cli_rpc_pipe_open_noauth(ctx->cli, cmd->interface,
					  &pipe_hnd);
	if (!NT_STATUS_IS_OK(status)) {
		d_fprintf(stderr, _("Could not open pipe: %s\n"),
			  nt_errstr(status));
		return status;
	}

	status = cmd->fn(c, mem_ctx, ctx, pipe_hnd, argc, argv);

	TALLOC_FREE(pipe_hnd);

	talloc_destroy(mem_ctx);

	return status;
}

static bool net_sh_process(struct net_context *c,
			   struct rpc_sh_ctx *ctx,
			   int argc, const char **argv)
{
	struct rpc_sh_cmd *cmd;
	struct rpc_sh_ctx *new_ctx;
	NTSTATUS status;

	if (argc == 0) {
		return true;
	}

	if (ctx == this_ctx) {

		/* We've been called from the cmd line */
		if (strequal(argv[0], "..") &&
		    (this_ctx->parent != NULL)) {
			new_ctx = this_ctx->parent;
			TALLOC_FREE(this_ctx);
			this_ctx = new_ctx;
			return true;
		}
	}

	if (strequal(argv[0], "exit") ||
	    strequal(argv[0], "quit") ||
	    strequal(argv[0], "q")) {
		return false;
	}

	if (strequal(argv[0], "help") || strequal(argv[0], "?")) {
		for (cmd = ctx->cmds; cmd->name != NULL; cmd++) {
			if (ctx != this_ctx) {
				d_printf("%s ", ctx->whoami);
			}
			d_printf("%-15s %s\n", cmd->name, cmd->help);
		}
		return true;
	}

	for (cmd = ctx->cmds; cmd->name != NULL; cmd++) {
		if (strequal(cmd->name, argv[0])) {
			break;
		}
	}

	if (cmd->name == NULL) {
		/* None found */
		d_fprintf(stderr,_( "%s: unknown cmd\n"), argv[0]);
		return true;
	}

	new_ctx = TALLOC_P(ctx, struct rpc_sh_ctx);
	if (new_ctx == NULL) {
		d_fprintf(stderr, _("talloc failed\n"));
		return false;
	}
	new_ctx->cli = ctx->cli;
	new_ctx->whoami = talloc_asprintf(new_ctx, "%s %s",
					  ctx->whoami, cmd->name);
	new_ctx->thiscmd = talloc_strdup(new_ctx, cmd->name);

	if (cmd->sub != NULL) {
		new_ctx->cmds = cmd->sub(c, new_ctx, ctx);
	} else {
		new_ctx->cmds = NULL;
	}

	new_ctx->parent = ctx;
	new_ctx->domain_name = ctx->domain_name;
	new_ctx->domain_sid = ctx->domain_sid;

	argc -= 1;
	argv += 1;

	if (cmd->sub != NULL) {
		if (argc == 0) {
			this_ctx = new_ctx;
			return true;
		}
		return net_sh_process(c, new_ctx, argc, argv);
	}

	status = net_sh_run(c, new_ctx, cmd, argc, argv);

	if (!NT_STATUS_IS_OK(status)) {
		d_fprintf(stderr, _("%s failed: %s\n"), new_ctx->whoami,
			  nt_errstr(status));
	}

	return true;
}

static struct rpc_sh_cmd sh_cmds[6] = {

	{ "info", NULL, &ndr_table_samr.syntax_id, rpc_sh_info,
	  N_("Print information about the domain connected to") },

	{ "rights", net_rpc_rights_cmds, 0, NULL,
	  N_("List/Grant/Revoke user rights") },

	{ "share", net_rpc_share_cmds, 0, NULL,
	  N_("List/Add/Remove etc shares") },

	{ "user", net_rpc_user_cmds, 0, NULL,
	  N_("List/Add/Remove user info") },

	{ "account", net_rpc_acct_cmds, 0, NULL,
	  N_("Show/Change account policy settings") },

	{ NULL, NULL, 0, NULL, NULL }
};

int net_rpc_shell(struct net_context *c, int argc, const char **argv)
{
	NTSTATUS status;
	struct rpc_sh_ctx *ctx;

	if (argc != 0 || c->display_usage) {
		d_printf("%s\nnet rpc shell\n", _("Usage:"));
		return -1;
	}

	if (libnetapi_net_init(&c->netapi_ctx) != 0) {
		return -1;
	}
	libnetapi_set_username(c->netapi_ctx, c->opt_user_name);
	libnetapi_set_password(c->netapi_ctx, c->opt_password);
	if (c->opt_kerberos) {
		libnetapi_set_use_kerberos(c->netapi_ctx);
	}

	ctx = TALLOC_P(NULL, struct rpc_sh_ctx);
	if (ctx == NULL) {
		d_fprintf(stderr, _("talloc failed\n"));
		return -1;
	}

	status = net_make_ipc_connection(c, 0, &(ctx->cli));
	if (!NT_STATUS_IS_OK(status)) {
		d_fprintf(stderr, _("Could not open connection: %s\n"),
			  nt_errstr(status));
		return -1;
	}

	ctx->cmds = sh_cmds;
	ctx->whoami = "net rpc";
	ctx->parent = NULL;

	status = net_get_remote_domain_sid(ctx->cli, ctx, &ctx->domain_sid,
					   &ctx->domain_name);
	if (!NT_STATUS_IS_OK(status)) {
		return -1;
	}

	d_printf(_("Talking to domain %s (%s)\n"), ctx->domain_name,
		 sid_string_tos(ctx->domain_sid));

	this_ctx = ctx;

	while(1) {
		char *prompt = NULL;
		char *line = NULL;
		int ret;

		if (asprintf(&prompt, "%s> ", this_ctx->whoami) < 0) {
			break;
		}

		line = smb_readline(prompt, NULL, completion_fn);
		SAFE_FREE(prompt);

		if (line == NULL) {
			break;
		}

		ret = poptParseArgvString(line, &argc, &argv);
		if (ret == POPT_ERROR_NOARG) {
			SAFE_FREE(line);
			continue;
		}
		if (ret != 0) {
			d_fprintf(stderr, _("cmdline invalid: %s\n"),
				  poptStrerror(ret));
			SAFE_FREE(line);
			return false;
		}

		if ((line[0] != '\n') &&
		    (!net_sh_process(c, this_ctx, argc, argv))) {
			SAFE_FREE(line);
			break;
		}
		SAFE_FREE(line);
	}

	cli_shutdown(ctx->cli);

	TALLOC_FREE(ctx);

	return 0;
}
