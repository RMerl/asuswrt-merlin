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
#include <tevent.h>
#include "rpc_server/dcerpc_server.h"
#include "librpc/gen_ndr/dcerpc.h"
#include "rpc_server/common/common.h"
#include "messaging/irpc.h"
#include "auth/auth.h"


struct dcesrv_forward_state {
	const char *opname;
	struct dcesrv_call_state *dce_call;
};

/*
  called when the forwarded rpc request is finished
 */
static void dcesrv_irpc_forward_callback(struct tevent_req *subreq)
{
	struct dcesrv_forward_state *st =
		tevent_req_callback_data(subreq,
		struct dcesrv_forward_state);
	const char *opname = st->opname;
	NTSTATUS status;

	status = dcerpc_binding_handle_call_recv(subreq);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("IRPC callback failed for %s - %s\n",
			 opname, nt_errstr(status)));
		st->dce_call->fault_code = DCERPC_FAULT_CANT_PERFORM;
	}
	status = dcesrv_reply(st->dce_call);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("%s_handler: dcesrv_reply() failed - %s\n",
			 opname, nt_errstr(status)));
	}
}



/**
 * Forward a RPC call using IRPC to another task
 */
void dcesrv_irpc_forward_rpc_call(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				  void *r, uint32_t callid,
				  const struct ndr_interface_table *ndr_table,
				  const char *dest_task, const char *opname,
				  uint32_t timeout)
{
	struct dcesrv_forward_state *st;
	struct dcerpc_binding_handle *binding_handle;
	struct tevent_req *subreq;
	struct security_token *token;

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

	binding_handle = irpc_binding_handle_by_name(st, dce_call->msg_ctx,
						     dest_task, ndr_table);
	if (binding_handle == NULL) {
		DEBUG(0,("%s: Failed to forward request to %s task\n",
			 opname, dest_task));
		dce_call->fault_code = DCERPC_FAULT_CANT_PERFORM;
		return;
	}

	/* reset timeout for the handle */
	dcerpc_binding_handle_set_timeout(binding_handle, timeout);

	/* add security token to the handle*/
	token = dce_call->conn->auth_state.session_info->security_token;
	irpc_binding_handle_add_security_token(binding_handle, token);

	/* forward the call */
	subreq = dcerpc_binding_handle_call_send(st, dce_call->event_ctx,
						 binding_handle,
						 NULL, ndr_table,
						 callid,
						 dce_call, r);
	if (subreq == NULL) {
		DEBUG(0,("%s: Failed to forward request to %s task\n", 
			 opname, dest_task));
		dce_call->fault_code = DCERPC_FAULT_CANT_PERFORM;
		return;
	}

	/* mark the request as replied async */
	dce_call->state_flags |= DCESRV_CALL_STATE_FLAG_ASYNC;

	/* setup the callback */
	tevent_req_set_callback(subreq, dcesrv_irpc_forward_callback, st);
}
