/* 
   Unix SMB/CIFS implementation.

   smbd-specific dcerpc server code

   Copyright (C) Andrew Tridgell 2003-2005
   Copyright (C) Stefan (metze) Metzmacher 2004-2005
   Copyright (C) Jelmer Vernooij <jelmer@samba.org> 2004,2007
   
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
#include "librpc/gen_ndr/ndr_dcerpc.h"
#include "auth/auth.h"
#include "../lib/util/dlinklist.h"
#include "rpc_server/dcerpc_server.h"
#include "rpc_server/dcerpc_server_proto.h"
#include "system/filesys.h"
#include "lib/messaging/irpc.h"
#include "system/network.h"
#include "lib/socket/netif.h"
#include "param/param.h"
#include "../lib/tsocket/tsocket.h"
#include "librpc/rpc/dcerpc_proto.h"
#include "../lib/util/tevent_ntstatus.h"
#include "libcli/raw/smb.h"
#include "../libcli/named_pipe_auth/npa_tstream.h"
#include "smbd/process_model.h"


/*
  open the dcerpc server sockets
*/
static void dcesrv_task_init(struct task_server *task)
{
	NTSTATUS status;
	struct dcesrv_context *dce_ctx;
	struct dcesrv_endpoint *e;
	const struct model_ops *model_ops;

	dcerpc_server_init(task->lp_ctx);

	task_server_set_title(task, "task[dcesrv]");

	/* run the rpc server as a single process to allow for shard
	 * handles, and sharing of ldb contexts */
	model_ops = process_model_startup("single");
	if (!model_ops) goto failed;

	status = dcesrv_init_context(task->event_ctx,
				     task->lp_ctx,
				     lpcfg_dcerpc_endpoint_servers(task->lp_ctx),
				     &dce_ctx);
	if (!NT_STATUS_IS_OK(status)) goto failed;

	/* Make sure the directory for NCALRPC exists */
	if (!directory_exist(lpcfg_ncalrpc_dir(task->lp_ctx))) {
		mkdir(lpcfg_ncalrpc_dir(task->lp_ctx), 0755);
	}

	for (e=dce_ctx->endpoint_list;e;e=e->next) {
		status = dcesrv_add_ep(dce_ctx, task->lp_ctx, e, task->event_ctx, model_ops);
		if (!NT_STATUS_IS_OK(status)) goto failed;
	}

	return;
failed:
	task_server_terminate(task, "Failed to startup dcerpc server task", true);	
}

NTSTATUS server_service_rpc_init(void)
{
	return register_server_service("rpc", dcesrv_task_init);
}
