/*
 *  Unix SMB/CIFS implementation.
 *  RPC client transport over a socket
 *  Copyright (C) Volker Lendecke 2009
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
#include "../lib/tsocket/tsocket.h"
#include "rpc_client/rpc_transport.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_RPC_CLI

NTSTATUS rpc_transport_sock_init(TALLOC_CTX *mem_ctx, int fd,
				 struct rpc_cli_transport **presult)
{
	struct rpc_cli_transport *result;
	struct tstream_context *stream;
	int ret;
	NTSTATUS status;

	set_blocking(fd, false);

	ret = tstream_bsd_existing_socket(mem_ctx, fd, &stream);
	if (ret != 0) {
		status = map_nt_error_from_unix(errno);
		return status;
	}

	status = rpc_transport_tstream_init(mem_ctx,
					    &stream,
					    &result);
	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(stream);
		return status;
	}

	*presult = result;
	return NT_STATUS_OK;
}
