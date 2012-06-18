/*
 *  Unix SMB/CIFS implementation.
 *  NetApi Shutdown Support
 *  Copyright (C) Guenther Deschner 2009
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

#include "librpc/gen_ndr/libnetapi.h"
#include "lib/netapi/netapi.h"
#include "lib/netapi/netapi_private.h"
#include "lib/netapi/libnetapi.h"
#include "../librpc/gen_ndr/cli_initshutdown.h"

/****************************************************************
****************************************************************/

WERROR NetShutdownInit_r(struct libnetapi_ctx *ctx,
			 struct NetShutdownInit *r)
{
	WERROR werr;
	NTSTATUS status;
	struct rpc_pipe_client *pipe_cli = NULL;
	struct lsa_StringLarge message;

	werr = libnetapi_open_pipe(ctx, r->in.server_name,
				   &ndr_table_initshutdown.syntax_id,
				   &pipe_cli);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	init_lsa_StringLarge(&message, r->in.message);

	status = rpccli_initshutdown_Init(pipe_cli, talloc_tos(),
					  NULL,
					  &message,
					  r->in.timeout,
					  r->in.force_apps,
					  r->in.do_reboot,
					  &werr);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}

 done:
	return werr;
}

/****************************************************************
****************************************************************/

WERROR NetShutdownInit_l(struct libnetapi_ctx *ctx,
			 struct NetShutdownInit *r)
{
	LIBNETAPI_REDIRECT_TO_LOCALHOST(ctx, r, NetShutdownInit);
}

/****************************************************************
****************************************************************/

WERROR NetShutdownAbort_r(struct libnetapi_ctx *ctx,
			  struct NetShutdownAbort *r)
{
	WERROR werr;
	NTSTATUS status;
	struct rpc_pipe_client *pipe_cli = NULL;

	werr = libnetapi_open_pipe(ctx, r->in.server_name,
				   &ndr_table_initshutdown.syntax_id,
				   &pipe_cli);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	status = rpccli_initshutdown_Abort(pipe_cli, talloc_tos(),
					   NULL,
					   &werr);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}

 done:
	return werr;
}

/****************************************************************
****************************************************************/

WERROR NetShutdownAbort_l(struct libnetapi_ctx *ctx,
			  struct NetShutdownAbort *r)
{
	LIBNETAPI_REDIRECT_TO_LOCALHOST(ctx, r, NetShutdownAbort);
}
