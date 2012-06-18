/* 
   Unix SMB/CIFS implementation.

   forwarding of RPC calls to other tasks

   Copyright (C) Andrew Tridgell 2009
   
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
#include "rpc_server/dcerpc_server.h"
#include "messaging/irpc.h"

struct dcesrv_forward_state {
	const char *opname;
	struct dcesrv_call_state *dce_call;
};

/*
  called when the forwarded rpc request is finished
 */
static void dcesrv_irpc_forward_callback(struct irpc_request *ireq)
{
	struct dcesrv_forward_state *st = talloc_get_type(ireq->async.private_data, 
							  struct dcesrv_forward_state);
	const char *opname = st->opname;
	NTSTATUS status;
	if (!NT_STATUS_IS_OK(ireq->status)) {
		DEBUG(0,("IRPC callback failed for %s - %s\n",
			 opname, nt_errstr(ireq->status)));
		st->dce_call->fault_code = DCERPC_FAULT_CANT_PERFORM;
	}
	talloc_free(ireq);
	status = dcesrv_reply(st->dce_call);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("%s_handler: dcesrv_reply() failed - %s\n",
			 opname, nt_errstr(status)));
	}
}



/*
  forward a RPC call using IRPC to another task
 */
void dcesrv_irpc_forward_rpc_call(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				  void *r, uint32_t callid,
				  const struct ndr_interface_table *ndr_table,
				  const char *dest_task, const char *opname)
{
	struct server_id *sid;
	struct irpc_request *ireq;
	struct dcesrv_forward_state *st;

	st = talloc(mem_ctx, struct dcesrv_forward_state);
	if (st == NULL) {
		dce_call->fault_code = DCERPC_FAULT_CANT_PERFORM;
		return;
	}

	st->dce_call = dce_call;
	st->opname   = opname;

	/* if the caller has said they can't support async calls
	   then fail the call */
	if (!(dce_call->state_flags & DCESRV_CALL_STATE_FLAG_MAY_ASYNC)) {
		/* we're not allowed to reply async */
		DEBUG(0,("%s: Not available synchronously\n", dest_task));
		dce_call->fault_code = DCERPC_FAULT_CANT_PERFORM;
		return;
	}

	/* find the server task */
	sid = irpc_servers_byname(dce_call->msg_ctx, mem_ctx, dest_task);
	if (sid == NULL || sid[0].id == 0) {
		DEBUG(0,("%s: Unable to find %s task\n", dest_task, opname));
		dce_call->fault_code = DCERPC_FAULT_CANT_PERFORM;
		return;
	}

	/* forward the call */
	ireq = irpc_call_send(dce_call->msg_ctx, sid[0], ndr_table, callid, r, mem_ctx);
	if (ireq == NULL) {
		DEBUG(0,("%s: Failed to forward request to %s task\n", 
			 opname, dest_task));
		dce_call->fault_code = DCERPC_FAULT_CANT_PERFORM;
		return;
	}

	/* mark the request as replied async */
	dce_call->state_flags |= DCESRV_CALL_STATE_FLAG_ASYNC;

	/* setup the callback */
	ireq->async.fn = dcesrv_irpc_forward_callback;
	ireq->async.private_data = st;
}
