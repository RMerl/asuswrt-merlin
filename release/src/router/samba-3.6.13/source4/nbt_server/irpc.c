/* 
   Unix SMB/CIFS implementation.

   irpc services for the NBT server

   Copyright (C) Andrew Tridgell	2005
   Copyright (C) Volker Lendecke	2005
   
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
#include "smbd/service_task.h"
#include "smbd/service.h"
#include "nbt_server/nbt_server.h"
#include "nbt_server/wins/winsserver.h"
#include "librpc/gen_ndr/ndr_irpc.h"
#include "lib/socket/socket.h"
#include "libcli/resolve/resolve.h"
#include "librpc/gen_ndr/ndr_nbt.h"

/*
  serve out the nbt statistics
*/
static NTSTATUS nbtd_information(struct irpc_message *msg, 
				 struct nbtd_information *r)
{
	struct nbtd_server *server = talloc_get_type(msg->private_data,
						     struct nbtd_server);

	switch (r->in.level) {
	case NBTD_INFO_STATISTICS:
		r->out.info.stats = &server->stats;
		break;
	}

	return NT_STATUS_OK;
}


/*
  winbind needs to be able to do a getdc request, but most (all?) windows
  servers always send the reply to port 138, regardless of the request
  port. To cope with this we use a irpc request to the NBT server
  which has port 138 open, and thus can receive the replies
*/
struct getdc_state {
	struct irpc_message *msg;
	struct nbtd_getdcname *req;
};

static void getdc_recv_netlogon_reply(struct dgram_mailslot_handler *dgmslot, 
				      struct nbt_dgram_packet *packet, 
				      struct socket_address *src)
{
	struct getdc_state *s =
		talloc_get_type(dgmslot->private_data, struct getdc_state);
	const char *p;
	struct nbt_netlogon_response netlogon;
	NTSTATUS status;

	status = dgram_mailslot_netlogon_parse_response(dgmslot, packet, packet,
							&netlogon);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(5, ("dgram_mailslot_ntlogon_parse failed: %s\n",
			  nt_errstr(status)));
		goto done;
	}

	/* We asked for version 1 only */
	if (netlogon.response_type == NETLOGON_SAMLOGON
	    && netlogon.data.samlogon.ntver != NETLOGON_NT_VERSION_1) {
		status = NT_STATUS_INVALID_NETWORK_RESPONSE;
		goto done;
	}

	p = netlogon.data.samlogon.data.nt4.pdc_name;

	DEBUG(10, ("NTLOGON_SAM_LOGON_REPLY: server: %s, user: %s, "
		   "domain: %s\n", p,
		   netlogon.data.samlogon.data.nt4.user_name,
		   netlogon.data.samlogon.data.nt4.domain_name));

	if (*p == '\\') p += 1;
	if (*p == '\\') p += 1;
	
	s->req->out.dcname = talloc_strdup(s->req, p);
	if (s->req->out.dcname == NULL) {
		DEBUG(0, ("talloc failed\n"));
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}

	status = NT_STATUS_OK;

 done:
	irpc_send_reply(s->msg, status);
}

static NTSTATUS nbtd_getdcname(struct irpc_message *msg, 
			       struct nbtd_getdcname *req)
{
	struct nbtd_server *server =
		talloc_get_type(msg->private_data, struct nbtd_server);
	struct nbtd_interface *iface = nbtd_find_request_iface(server, req->in.ip_address, true);
	struct getdc_state *s;
	struct nbt_netlogon_packet p;
	struct NETLOGON_SAM_LOGON_REQUEST *r;
	struct nbt_name src, dst;
	struct socket_address *dest;
	struct dgram_mailslot_handler *handler;
	NTSTATUS status = NT_STATUS_UNSUCCESSFUL;

	DEBUG(0, ("nbtd_getdcname called\n"));

	s = talloc(msg, struct getdc_state);
        NT_STATUS_HAVE_NO_MEMORY(s);

	s->msg = msg;
	s->req = req;
	
	handler = dgram_mailslot_temp(iface->dgmsock, NBT_MAILSLOT_GETDC,
				      getdc_recv_netlogon_reply, s);
        NT_STATUS_HAVE_NO_MEMORY(handler);
	
	ZERO_STRUCT(p);
	p.command = LOGON_SAM_LOGON_REQUEST;
	r = &p.req.logon;
	r->request_count = 0;
	r->computer_name = req->in.my_computername;
	r->user_name = req->in.my_accountname;
	r->mailslot_name = handler->mailslot_name;
	r->acct_control = req->in.account_control;
	r->sid = *req->in.domain_sid;
	r->nt_version = NETLOGON_NT_VERSION_1;
	r->lmnt_token = 0xffff;
	r->lm20_token = 0xffff;

	make_nbt_name_client(&src, req->in.my_computername);
	make_nbt_name(&dst, req->in.domainname, 0x1c);

	dest = socket_address_from_strings(msg, iface->dgmsock->sock->backend_name, 
					   req->in.ip_address, 138);
	NT_STATUS_HAVE_NO_MEMORY(dest);

	status = dgram_mailslot_netlogon_send(iface->dgmsock, 
					      &dst, dest,
					      NBT_MAILSLOT_NETLOGON, 
					      &src, &p);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("dgram_mailslot_ntlogon_send failed: %s\n",
			  nt_errstr(status)));
		return status;
	}

	msg->defer_reply = true;
	return NT_STATUS_OK;
}


/*
  register the irpc handlers for the nbt server
*/
void nbtd_register_irpc(struct nbtd_server *nbtsrv)
{
	NTSTATUS status;
	struct task_server *task = nbtsrv->task;

	status = IRPC_REGISTER(task->msg_ctx, irpc, NBTD_INFORMATION, 
			       nbtd_information, nbtsrv);
	if (!NT_STATUS_IS_OK(status)) {
		task_server_terminate(task, "nbtd failed to setup monitoring", true);
		return;
	}

	status = IRPC_REGISTER(task->msg_ctx, irpc, NBTD_GETDCNAME,
			       nbtd_getdcname, nbtsrv);
	if (!NT_STATUS_IS_OK(status)) {
		task_server_terminate(task, "nbtd failed to setup getdcname "
				      "handler", true);
		return;
	}

	status = IRPC_REGISTER(task->msg_ctx, irpc, NBTD_PROXY_WINS_CHALLENGE,
			       nbtd_proxy_wins_challenge, nbtsrv);
	if (!NT_STATUS_IS_OK(status)) {
		task_server_terminate(task, "nbtd failed to setup wins challenge "
				      "handler", true);
		return;
	}

	status = IRPC_REGISTER(task->msg_ctx, irpc, NBTD_PROXY_WINS_RELEASE_DEMAND,
			       nbtd_proxy_wins_release_demand, nbtsrv);
	if (!NT_STATUS_IS_OK(status)) {
		task_server_terminate(task, "nbtd failed to setup wins release demand "
				      "handler", true);
		return;
	}
}
