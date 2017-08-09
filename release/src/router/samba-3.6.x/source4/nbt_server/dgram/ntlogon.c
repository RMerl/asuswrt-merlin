/* 
   Unix SMB/CIFS implementation.

   NBT datagram ntlogon server

   Copyright (C) Andrew Tridgell	2005
   
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
#include "nbt_server/nbt_server.h"
#include "smbd/service_task.h"
#include "lib/socket/socket.h"
#include "librpc/gen_ndr/ndr_nbt.h"
#include "param/param.h"

/*
  reply to a SAM LOGON request
 */
static void nbtd_ntlogon_sam_logon(struct dgram_mailslot_handler *dgmslot, 
				   struct nbtd_interface *iface,
				   struct nbt_dgram_packet *packet,
				   const struct socket_address *src,
				   struct nbt_ntlogon_packet *ntlogon)
{
	struct nbt_name *name = &packet->data.msg.dest_name;
	struct nbtd_interface *reply_iface = nbtd_find_reply_iface(iface, src->addr, false);
	struct nbt_ntlogon_packet reply;
	struct nbt_ntlogon_sam_logon_reply *logon;

	/* only answer sam logon requests on the PDC or LOGON names */
	if (name->type != NBT_NAME_PDC && name->type != NBT_NAME_LOGON) {
		return;
	}

	/* setup a SAM LOGON reply */
	ZERO_STRUCT(reply);
	reply.command = NTLOGON_SAM_LOGON_REPLY;
	logon = &reply.req.reply;

	logon->server           = talloc_asprintf(packet, "\\\\%s", 
						  lpcfg_netbios_name(iface->nbtsrv->task->lp_ctx));
	logon->user_name        = ntlogon->req.logon.user_name;
	logon->domain           = lpcfg_workgroup(iface->nbtsrv->task->lp_ctx);
	logon->nt_version       = 1;
	logon->lmnt_token       = 0xFFFF;
	logon->lm20_token       = 0xFFFF;

	packet->data.msg.dest_name.type = 0;

	dgram_mailslot_ntlogon_reply(reply_iface->dgmsock, 
				     packet, 
				     lpcfg_netbios_name(iface->nbtsrv->task->lp_ctx),
				     ntlogon->req.logon.mailslot_name,
				     &reply);
}

/*
  handle incoming ntlogon mailslot requests
*/
void nbtd_mailslot_ntlogon_handler(struct dgram_mailslot_handler *dgmslot, 
				   struct nbt_dgram_packet *packet, 
				   struct socket_address *src)
{
	NTSTATUS status = NT_STATUS_NO_MEMORY;
	struct nbtd_interface *iface = 
		talloc_get_type(dgmslot->private_data, struct nbtd_interface);
	struct nbt_ntlogon_packet *ntlogon = 
		talloc(dgmslot, struct nbt_ntlogon_packet);
	struct nbtd_iface_name *iname;
	struct nbt_name *name = &packet->data.msg.dest_name;

	if (ntlogon == NULL) goto failed;

	/*
	  see if the we are listening on the destination netbios name
	*/
	iname = nbtd_find_iname(iface, name, 0);
	if (iname == NULL) {
		status = NT_STATUS_BAD_NETWORK_NAME;
		goto failed;
	}

	DEBUG(2,("ntlogon request to %s from %s:%d\n", 
		 nbt_name_string(ntlogon, name), src->addr, src->port));
	status = dgram_mailslot_ntlogon_parse(dgmslot, ntlogon, packet, ntlogon);
	if (!NT_STATUS_IS_OK(status)) goto failed;

	NDR_PRINT_DEBUG(nbt_ntlogon_packet, ntlogon);

	switch (ntlogon->command) {
	case NTLOGON_SAM_LOGON:
		nbtd_ntlogon_sam_logon(dgmslot, iface, packet, src, ntlogon);
		break;
	default:
		DEBUG(2,("unknown ntlogon op %d from %s:%d\n", 
			 ntlogon->command, src->addr, src->port));
		break;
	}

	talloc_free(ntlogon);
	return;

failed:
	DEBUG(2,("nbtd ntlogon handler failed from %s:%d to %s - %s\n",
		 src->addr, src->port, nbt_name_string(ntlogon, name),
		 nt_errstr(status)));
	talloc_free(ntlogon);
}
