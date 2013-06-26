/* 
   Unix SMB/CIFS implementation.
   remote dcerpc operations

   Copyright (C) Stefan (metze) Metzmacher 2004
   Copyright (C) Julien Kerihuel 2008-2009
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2010

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
#include "auth/auth.h"
#include "auth/credentials/credentials.h"
#include "librpc/ndr/ndr_table.h"
#include "param/param.h"


struct dcesrv_remote_private {
	struct dcerpc_pipe *c_pipe;
};

static NTSTATUS remote_op_reply(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx, void *r)
{
	return NT_STATUS_OK;
}

static NTSTATUS remote_op_bind(struct dcesrv_call_state *dce_call, const struct dcesrv_interface *iface, uint32_t if_version)
{
        NTSTATUS status;
	const struct ndr_interface_table *table;
	struct dcesrv_remote_private *priv;
	const char *binding = lpcfg_parm_string(dce_call->conn->dce_ctx->lp_ctx, NULL, "dcerpc_remote", "binding");
	const char *user, *pass, *domain;
	struct cli_credentials *credentials;
	bool must_free_credentials = true;
	bool machine_account;
	struct dcerpc_binding		*b;
	struct composite_context	*pipe_conn_req;

	machine_account = lpcfg_parm_bool(dce_call->conn->dce_ctx->lp_ctx, NULL, "dcerpc_remote", "use_machine_account", false);

	priv = talloc(dce_call->conn, struct dcesrv_remote_private);
	if (!priv) {
		return NT_STATUS_NO_MEMORY;	
	}
	
	priv->c_pipe = NULL;
	dce_call->context->private_data = priv;

	if (!binding) {
		DEBUG(0,("You must specify a DCE/RPC binding string\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	user = lpcfg_parm_string(dce_call->conn->dce_ctx->lp_ctx, NULL, "dcerpc_remote", "user");
	pass = lpcfg_parm_string(dce_call->conn->dce_ctx->lp_ctx, NULL, "dcerpc_remote", "password");
	domain = lpcfg_parm_string(dce_call->conn->dce_ctx->lp_ctx, NULL, "dceprc_remote", "domain");

	table = ndr_table_by_uuid(&iface->syntax_id.uuid); /* FIXME: What about if_version ? */
	if (!table) {
		dce_call->fault_code = DCERPC_FAULT_UNK_IF;
		return NT_STATUS_NET_WRITE_FAULT;
	}

	if (user && pass) {
		DEBUG(5, ("dcerpc_remote: RPC Proxy: Using specified account\n"));
		credentials = cli_credentials_init(priv);
		if (!credentials) {
			return NT_STATUS_NO_MEMORY;
		}
		cli_credentials_set_conf(credentials, dce_call->conn->dce_ctx->lp_ctx);
		cli_credentials_set_username(credentials, user, CRED_SPECIFIED);
		if (domain) {
			cli_credentials_set_domain(credentials, domain, CRED_SPECIFIED);
		}
		cli_credentials_set_password(credentials, pass, CRED_SPECIFIED);
	} else if (machine_account) {
		DEBUG(5, ("dcerpc_remote: RPC Proxy: Using machine account\n"));
		credentials = cli_credentials_init(priv);
		cli_credentials_set_conf(credentials, dce_call->conn->dce_ctx->lp_ctx);
		if (domain) {
			cli_credentials_set_domain(credentials, domain, CRED_SPECIFIED);
		}
		status = cli_credentials_set_machine_account(credentials, dce_call->conn->dce_ctx->lp_ctx);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
	} else if (dce_call->conn->auth_state.session_info->credentials) {
		DEBUG(5, ("dcerpc_remote: RPC Proxy: Using delegated credentials\n"));
		credentials = dce_call->conn->auth_state.session_info->credentials;
		must_free_credentials = false;
	} else {
		DEBUG(1,("dcerpc_remote: RPC Proxy: You must supply binding, user and password or have delegated credentials\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* parse binding string to the structure */
	status = dcerpc_parse_binding(dce_call->context, binding, &b);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Failed to parse dcerpc binding '%s'\n", binding));
		return status;
	}
	
	DEBUG(3, ("Using binding %s\n", dcerpc_binding_string(dce_call->context, b)));
	
	/* If we already have a remote association group ID, then use that */
	if (dce_call->context->assoc_group->proxied_id != 0) {
		b->assoc_group_id = dce_call->context->assoc_group->proxied_id;
	}

	b->object.if_version = if_version;

	pipe_conn_req = dcerpc_pipe_connect_b_send(dce_call->context, b, table,
						   credentials, dce_call->event_ctx, dce_call->conn->dce_ctx->lp_ctx);
	status = dcerpc_pipe_connect_b_recv(pipe_conn_req, dce_call->context, &(priv->c_pipe));
	
	if (must_free_credentials) {
		talloc_free(credentials);
	}

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (dce_call->context->assoc_group->proxied_id == 0) {
		dce_call->context->assoc_group->proxied_id = priv->c_pipe->assoc_group_id;
	}

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	return NT_STATUS_OK;	
}

static NTSTATUS remote_op_ndr_pull(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx, struct ndr_pull *pull, void **r)
{
	enum ndr_err_code ndr_err;
	const struct ndr_interface_table *table = (const struct ndr_interface_table *)dce_call->context->iface->private_data;
	uint16_t opnum = dce_call->pkt.u.request.opnum;

	dce_call->fault_code = 0;

	if (opnum >= table->num_calls) {
		dce_call->fault_code = DCERPC_FAULT_OP_RNG_ERROR;
		return NT_STATUS_NET_WRITE_FAULT;
	}

	*r = talloc_size(mem_ctx, table->calls[opnum].struct_size);
	if (!*r) {
		return NT_STATUS_NO_MEMORY;
	}

        /* unravel the NDR for the packet */
	ndr_err = table->calls[opnum].ndr_pull(pull, NDR_IN, *r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		dcerpc_log_packet(dce_call->conn->packet_log_dir,
						  table, opnum, NDR_IN,
				  &dce_call->pkt.u.request.stub_and_verifier);
		dce_call->fault_code = DCERPC_FAULT_NDR;
		return NT_STATUS_NET_WRITE_FAULT;
	}

	return NT_STATUS_OK;
}

static void remote_op_dispatch_done(struct tevent_req *subreq);

static NTSTATUS remote_op_dispatch(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx, void *r)
{
	struct dcesrv_remote_private *priv = talloc_get_type_abort(dce_call->context->private_data,
								   struct dcesrv_remote_private);
	uint16_t opnum = dce_call->pkt.u.request.opnum;
	const struct ndr_interface_table *table = dce_call->context->iface->private_data;
	const struct ndr_interface_call *call;
	const char *name;
	struct tevent_req *subreq;

	name = table->calls[opnum].name;
	call = &table->calls[opnum];

	if (priv->c_pipe->conn->flags & DCERPC_DEBUG_PRINT_IN) {
		ndr_print_function_debug(call->ndr_print, name, NDR_IN | NDR_SET_VALUES, r);		
	}

	priv->c_pipe->conn->flags |= DCERPC_NDR_REF_ALLOC;

	/* we didn't use the return code of this function as we only check the last_fault_code */
	subreq = dcerpc_binding_handle_call_send(dce_call, dce_call->event_ctx,
						 priv->c_pipe->binding_handle,
						 NULL, table,
						 opnum, mem_ctx, r);
	if (subreq == NULL) {
		DEBUG(0,("dcesrv_remote: call[%s] dcerpc_binding_handle_call_send() failed!\n", name));
		return NT_STATUS_NO_MEMORY;
	}
	tevent_req_set_callback(subreq, remote_op_dispatch_done, dce_call);

	dce_call->state_flags |= DCESRV_CALL_STATE_FLAG_ASYNC;
	return NT_STATUS_OK;
}

static void remote_op_dispatch_done(struct tevent_req *subreq)
{
	struct dcesrv_call_state *dce_call = tevent_req_callback_data(subreq,
					     struct dcesrv_call_state);
	struct dcesrv_remote_private *priv = talloc_get_type_abort(dce_call->context->private_data,
								   struct dcesrv_remote_private);
	uint16_t opnum = dce_call->pkt.u.request.opnum;
	const struct ndr_interface_table *table = dce_call->context->iface->private_data;
	const struct ndr_interface_call *call;
	const char *name;
	NTSTATUS status;

	name = table->calls[opnum].name;
	call = &table->calls[opnum];

	/* we didn't use the return code of this function as we only check the last_fault_code */
	status = dcerpc_binding_handle_call_recv(subreq);
	TALLOC_FREE(subreq);

	dce_call->fault_code = priv->c_pipe->last_fault_code;
	if (dce_call->fault_code != 0) {
		DEBUG(0,("dcesrv_remote: call[%s] failed with: %s!\n",
			name, dcerpc_errstr(dce_call, dce_call->fault_code)));
		goto reply;
	}

	if (NT_STATUS_IS_OK(status) &&
	    (priv->c_pipe->conn->flags & DCERPC_DEBUG_PRINT_OUT)) {
		ndr_print_function_debug(call->ndr_print, name, NDR_OUT, dce_call->r);
	}

reply:
	status = dcesrv_reply(dce_call);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("dcesrv_remote: call[%s]: dcesrv_reply() failed - %s\n",
			name, nt_errstr(status)));
	}
}

static NTSTATUS remote_op_ndr_push(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx, struct ndr_push *push, const void *r)
{
	enum ndr_err_code ndr_err;
	const struct ndr_interface_table *table = dce_call->context->iface->private_data;
	uint16_t opnum = dce_call->pkt.u.request.opnum;

        /* unravel the NDR for the packet */
	ndr_err = table->calls[opnum].ndr_push(push, NDR_OUT, r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		dce_call->fault_code = DCERPC_FAULT_NDR;
		return NT_STATUS_NET_WRITE_FAULT;
	}

	return NT_STATUS_OK;
}

static NTSTATUS remote_register_one_iface(struct dcesrv_context *dce_ctx, const struct dcesrv_interface *iface)
{
	unsigned int i;
	const struct ndr_interface_table *table = iface->private_data;

	for (i=0;i<table->endpoints->count;i++) {
		NTSTATUS ret;
		const char *name = table->endpoints->names[i];

		ret = dcesrv_interface_register(dce_ctx, name, iface, NULL);
		if (!NT_STATUS_IS_OK(ret)) {
			DEBUG(1,("remote_op_init_server: failed to register endpoint '%s'\n",name));
			return ret;
		}
	}

	return NT_STATUS_OK;
}

static NTSTATUS remote_op_init_server(struct dcesrv_context *dce_ctx, const struct dcesrv_endpoint_server *ep_server)
{
	unsigned int i;
	const char **ifaces = (const char **)str_list_make(dce_ctx, lpcfg_parm_string(dce_ctx->lp_ctx, NULL, "dcerpc_remote", "interfaces"),NULL);

	if (!ifaces) {
		DEBUG(3,("remote_op_init_server: no interfaces configured\n"));
		return NT_STATUS_OK;
	}

	for (i=0;ifaces[i];i++) {
		NTSTATUS ret;
		struct dcesrv_interface iface;
		
		if (!ep_server->interface_by_name(&iface, ifaces[i])) {
			DEBUG(0,("remote_op_init_server: failed to find interface = '%s'\n", ifaces[i]));
			talloc_free(ifaces);
			return NT_STATUS_UNSUCCESSFUL;
		}

		ret = remote_register_one_iface(dce_ctx, &iface);
		if (!NT_STATUS_IS_OK(ret)) {
			DEBUG(0,("remote_op_init_server: failed to register interface = '%s'\n", ifaces[i]));
			talloc_free(ifaces);
			return ret;
		}
	}

	talloc_free(ifaces);
	return NT_STATUS_OK;
}

static bool remote_fill_interface(struct dcesrv_interface *iface, const struct ndr_interface_table *if_tabl)
{
	iface->name = if_tabl->name;
	iface->syntax_id = if_tabl->syntax_id;
	
	iface->bind = remote_op_bind;
	iface->unbind = NULL;

	iface->ndr_pull = remote_op_ndr_pull;
	iface->dispatch = remote_op_dispatch;
	iface->reply = remote_op_reply;
	iface->ndr_push = remote_op_ndr_push;

	iface->private_data = if_tabl;

	return true;
}

static bool remote_op_interface_by_uuid(struct dcesrv_interface *iface, const struct GUID *uuid, uint32_t if_version)
{
	const struct ndr_interface_list *l;

	for (l=ndr_table_list();l;l=l->next) {
		if (l->table->syntax_id.if_version == if_version &&
			GUID_equal(&l->table->syntax_id.uuid, uuid)==0) {
			return remote_fill_interface(iface, l->table);
		}
	}

	return false;	
}

static bool remote_op_interface_by_name(struct dcesrv_interface *iface, const char *name)
{
	const struct ndr_interface_table *tbl = ndr_table_by_name(name);

	if (tbl)
		return remote_fill_interface(iface, tbl);

	return false;	
}

NTSTATUS dcerpc_server_remote_init(void)
{
	NTSTATUS ret;
	struct dcesrv_endpoint_server ep_server;

	ZERO_STRUCT(ep_server);

	/* fill in our name */
	ep_server.name = "remote";

	/* fill in all the operations */
	ep_server.init_server = remote_op_init_server;

	ep_server.interface_by_uuid = remote_op_interface_by_uuid;
	ep_server.interface_by_name = remote_op_interface_by_name;

	/* register ourselves with the DCERPC subsystem. */
	ret = dcerpc_register_ep_server(&ep_server);
	if (!NT_STATUS_IS_OK(ret)) {
		DEBUG(0,("Failed to register 'remote' endpoint server!\n"));
		return ret;
	}

	/* We need the full DCE/RPC interface table */
	ndr_table_init();

	return ret;
}
