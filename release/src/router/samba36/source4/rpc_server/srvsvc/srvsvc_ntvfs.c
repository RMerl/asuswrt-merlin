/* 
   Unix SMB/CIFS implementation.

   srvsvc pipe ntvfs helper functions

   Copyright (C) Stefan (metze) Metzmacher 2006
   
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
#include "ntvfs/ntvfs.h"
#include "rpc_server/dcerpc_server.h"
#include "param/param.h"

struct srvsvc_ntvfs_ctx {
	struct ntvfs_context *ntvfs;
};

static int srvsvc_ntvfs_ctx_destructor(struct srvsvc_ntvfs_ctx *c)
{
	ntvfs_disconnect(c->ntvfs);
	return 0;
}

NTSTATUS srvsvc_create_ntvfs_context(struct dcesrv_call_state *dce_call,
				     TALLOC_CTX *mem_ctx,
				     const char *share,
				     struct ntvfs_context **_ntvfs)
{
	NTSTATUS status;
	struct srvsvc_ntvfs_ctx	*c;
	struct ntvfs_request *ntvfs_req;
	enum ntvfs_type type;
	struct share_context *sctx;
	struct share_config *scfg;
	const char *sharetype;
	union smb_tcon tcon;
	const struct tsocket_address *local_address;
	const struct tsocket_address *remote_address;

	status = share_get_context_by_name(mem_ctx, lpcfg_share_backend(dce_call->conn->dce_ctx->lp_ctx), dce_call->event_ctx, dce_call->conn->dce_ctx->lp_ctx, &sctx);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = share_get_config(mem_ctx, sctx, share, &scfg);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("srvsvc_create_ntvfs_context: couldn't find service %s\n", share));
		return status;
	}

#if 0 /* TODO: fix access cecking */
	if (!socket_check_access(dce_call->connection->socket, 
				 scfg->name, 
				 share_string_list_option(scfg, SHARE_HOSTS_ALLOW), 
				 share_string_list_option(scfg, SHARE_HOSTS_DENY))) {
		return NT_STATUS_ACCESS_DENIED;
	}
#endif

	/* work out what sort of connection this is */
	sharetype = share_string_option(scfg, SHARE_TYPE, SHARE_TYPE_DEFAULT);
	if (sharetype && strcmp(sharetype, "IPC") == 0) {
		type = NTVFS_IPC;
	} else if (sharetype && strcmp(sharetype, "PRINTER")) {
		type = NTVFS_PRINT;
	} else {
		type = NTVFS_DISK;
	}

	c = talloc(mem_ctx, struct srvsvc_ntvfs_ctx);
	NT_STATUS_HAVE_NO_MEMORY(c);
	
	/* init ntvfs function pointers */
	status = ntvfs_init_connection(c, scfg, type,
				       PROTOCOL_NT1,
				       0,/* ntvfs_client_caps */
				       dce_call->event_ctx,
				       dce_call->conn->msg_ctx,
				       dce_call->conn->dce_ctx->lp_ctx,
				       dce_call->conn->server_id,
				       &c->ntvfs);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("srvsvc_create_ntvfs_context: ntvfs_init_connection failed for service %s\n", 
			  scfg->name));
		return status;
	}
	talloc_set_destructor(c, srvsvc_ntvfs_ctx_destructor);

	/*
	 * NOTE: we only set the addr callbacks as we're not interesseted in oplocks or in getting file handles
	 */
	local_address = dcesrv_connection_get_local_address(dce_call->conn);
	remote_address = dcesrv_connection_get_remote_address(dce_call->conn);
	status = ntvfs_set_addresses(c->ntvfs, local_address, remote_address);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("srvsvc_create_ntvfs_context: NTVFS failed to set the addr callbacks!\n"));
		return status;
	}

	ntvfs_req = ntvfs_request_create(c->ntvfs, mem_ctx,
					 dce_call->conn->auth_state.session_info,
					 0, /* TODO: fill in PID */
					 dce_call->time,
					 NULL, NULL, 0);
	NT_STATUS_HAVE_NO_MEMORY(ntvfs_req);

	/* Invoke NTVFS connection hook */
	tcon.tcon.level = RAW_TCON_TCON;
	ZERO_STRUCT(tcon.tcon.in);
	tcon.tcon.in.service = scfg->name;
	status = ntvfs_connect(ntvfs_req, &tcon);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("srvsvc_create_ntvfs_context: NTVFS ntvfs_connect() failed!\n"));
		return status;
	}

	*_ntvfs = c->ntvfs;
	return NT_STATUS_OK;
}
