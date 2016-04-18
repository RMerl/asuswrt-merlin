/*
 *  Unix SMB/CIFS implementation.
 *
 *  RPC Pipe client routines
 *
 *  Copyright (c) 2005      Jeremy Allison
 *  Copyright (c) 2010      Simo Sorce
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


#ifndef _RPC_CLIENT_H
#define _RPC_CLIENT_H

#include "librpc/gen_ndr/dcerpc.h"
#include "librpc/rpc/dcerpc.h"
#include "../librpc/ndr/libndr.h"
#include "rpc_client/rpc_transport.h"

struct dcerpc_binding_handle;

struct rpc_pipe_client {
	struct rpc_pipe_client *prev, *next;

	struct rpc_cli_transport *transport;
	struct dcerpc_binding_handle *binding_handle;

	struct ndr_syntax_id abstract_syntax;
	struct ndr_syntax_id transfer_syntax;
	bool verified_pcontext;

	char *desthost;
	char *srv_name_slash;

	uint16 max_xmit_frag;
	uint16 max_recv_frag;

	struct pipe_auth_data *auth;

	/* The following is only non-null on a netlogon client pipe. */
	struct netlogon_creds_CredentialState *dc;
};

#endif /* _RPC_CLIENT_H */
