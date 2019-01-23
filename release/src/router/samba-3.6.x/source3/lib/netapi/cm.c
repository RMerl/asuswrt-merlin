/*
 *  Unix SMB/CIFS implementation.
 *  NetApi Support
 *  Copyright (C) Guenther Deschner 2008
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

#include "lib/netapi/netapi.h"
#include "lib/netapi/netapi_private.h"
#include "libsmb/libsmb.h"
#include "rpc_client/cli_pipe.h"

/********************************************************************
********************************************************************/

struct client_ipc_connection {
	struct client_ipc_connection *prev, *next;
	struct cli_state *cli;
	struct client_pipe_connection *pipe_connections;
};

struct client_pipe_connection {
	struct client_pipe_connection *prev, *next;
	struct rpc_pipe_client *pipe;
};

/********************************************************************
********************************************************************/

static struct client_ipc_connection *ipc_cm_find(
	struct libnetapi_private_ctx *priv_ctx, const char *server_name)
{
	struct client_ipc_connection *p;

	for (p = priv_ctx->ipc_connections; p; p = p->next) {
		if (strequal(p->cli->desthost, server_name)) {
			return p;
		}
	}

	return NULL;
}

/********************************************************************
********************************************************************/

static WERROR libnetapi_open_ipc_connection(struct libnetapi_ctx *ctx,
					    const char *server_name,
					    struct client_ipc_connection **pp)
{
	struct libnetapi_private_ctx *priv_ctx =
		(struct libnetapi_private_ctx *)ctx->private_data;
	struct user_auth_info *auth_info = NULL;
	struct cli_state *cli_ipc = NULL;
	struct client_ipc_connection *p;

	if (!ctx || !pp || !server_name) {
		return WERR_INVALID_PARAM;
	}

	p = ipc_cm_find(priv_ctx, server_name);
	if (p) {
		*pp = p;
		return WERR_OK;
	}

	auth_info = user_auth_info_init(ctx);
	if (!auth_info) {
		return WERR_NOMEM;
	}
	auth_info->signing_state = Undefined;
	set_cmdline_auth_info_use_kerberos(auth_info, ctx->use_kerberos);
	set_cmdline_auth_info_username(auth_info, ctx->username);
	if (ctx->password) {
		set_cmdline_auth_info_password(auth_info, ctx->password);
	} else {
		set_cmdline_auth_info_getpass(auth_info);
	}

	if (ctx->username && ctx->username[0] &&
	    ctx->password && ctx->password[0] &&
	    ctx->use_kerberos) {
		set_cmdline_auth_info_fallback_after_kerberos(auth_info, true);
	}

	if (ctx->use_ccache) {
		set_cmdline_auth_info_use_ccache(auth_info, true);
	}

	cli_ipc = cli_cm_open(ctx, NULL,
				server_name, "IPC$",
				auth_info,
				false, false,
				PROTOCOL_NT1,
				0, 0x20);
	if (cli_ipc) {
		cli_set_username(cli_ipc, ctx->username);
		cli_set_password(cli_ipc, ctx->password);
		cli_set_domain(cli_ipc, ctx->workgroup);
	}
	TALLOC_FREE(auth_info);

	if (!cli_ipc) {
		libnetapi_set_error_string(ctx,
			"Failed to connect to IPC$ share on %s", server_name);
		return WERR_CAN_NOT_COMPLETE;
	}

	p = TALLOC_ZERO_P(ctx, struct client_ipc_connection);
	if (p == NULL) {
		return WERR_NOMEM;
	}

	p->cli = cli_ipc;
	DLIST_ADD(priv_ctx->ipc_connections, p);

	*pp = p;

	return WERR_OK;
}

/********************************************************************
********************************************************************/

WERROR libnetapi_shutdown_cm(struct libnetapi_ctx *ctx)
{
	struct libnetapi_private_ctx *priv_ctx =
		(struct libnetapi_private_ctx *)ctx->private_data;
	struct client_ipc_connection *p;

	for (p = priv_ctx->ipc_connections; p; p = p->next) {
		cli_shutdown(p->cli);
	}

	return WERR_OK;
}

/********************************************************************
********************************************************************/

static NTSTATUS pipe_cm_find(struct client_ipc_connection *ipc,
			     const struct ndr_syntax_id *interface,
			     struct rpc_pipe_client **presult)
{
	struct client_pipe_connection *p;

	for (p = ipc->pipe_connections; p; p = p->next) {

		if (!rpc_pipe_np_smb_conn(p->pipe)) {
			return NT_STATUS_PIPE_EMPTY;
		}

		if (strequal(ipc->cli->desthost, p->pipe->desthost)
		    && ndr_syntax_id_equal(&p->pipe->abstract_syntax,
					   interface)) {
			*presult = p->pipe;
			return NT_STATUS_OK;
		}
	}

	return NT_STATUS_PIPE_NOT_AVAILABLE;
}

/********************************************************************
********************************************************************/

static NTSTATUS pipe_cm_connect(TALLOC_CTX *mem_ctx,
				struct client_ipc_connection *ipc,
				const struct ndr_syntax_id *interface,
				struct rpc_pipe_client **presult)
{
	struct client_pipe_connection *p;
	NTSTATUS status;

	p = TALLOC_ZERO_ARRAY(mem_ctx, struct client_pipe_connection, 1);
	if (!p) {
		return NT_STATUS_NO_MEMORY;
	}

	status = cli_rpc_pipe_open_noauth(ipc->cli, interface, &p->pipe);
	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(p);
		return status;
	}

	DLIST_ADD(ipc->pipe_connections, p);

	*presult = p->pipe;
	return NT_STATUS_OK;
}

/********************************************************************
********************************************************************/

static NTSTATUS pipe_cm_open(TALLOC_CTX *ctx,
			     struct client_ipc_connection *ipc,
			     const struct ndr_syntax_id *interface,
			     struct rpc_pipe_client **presult)
{
	if (NT_STATUS_IS_OK(pipe_cm_find(ipc, interface, presult))) {
		return NT_STATUS_OK;
	}

	return pipe_cm_connect(ctx, ipc, interface, presult);
}

/********************************************************************
********************************************************************/

WERROR libnetapi_open_pipe(struct libnetapi_ctx *ctx,
			   const char *server_name,
			   const struct ndr_syntax_id *interface,
			   struct rpc_pipe_client **presult)
{
	struct rpc_pipe_client *result = NULL;
	NTSTATUS status;
	WERROR werr;
	struct client_ipc_connection *ipc = NULL;

	if (!presult) {
		return WERR_INVALID_PARAM;
	}

	werr = libnetapi_open_ipc_connection(ctx, server_name, &ipc);
	if (!W_ERROR_IS_OK(werr)) {
		return werr;
	}

	status = pipe_cm_open(ctx, ipc, interface, &result);
	if (!NT_STATUS_IS_OK(status)) {
		libnetapi_set_error_string(ctx, "failed to open PIPE %s: %s",
			get_pipe_name_from_syntax(talloc_tos(), interface),
			get_friendly_nt_error_msg(status));
		return WERR_DEST_NOT_FOUND;
	}

	*presult = result;

	return WERR_OK;
}

/********************************************************************
********************************************************************/

WERROR libnetapi_get_binding_handle(struct libnetapi_ctx *ctx,
				    const char *server_name,
				    const struct ndr_syntax_id *interface,
				    struct dcerpc_binding_handle **binding_handle)
{
	struct rpc_pipe_client *pipe_cli;
	WERROR result;

	*binding_handle = NULL;

	result = libnetapi_open_pipe(ctx, server_name, interface, &pipe_cli);
	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	*binding_handle = pipe_cli->binding_handle;

	return WERR_OK;
}
