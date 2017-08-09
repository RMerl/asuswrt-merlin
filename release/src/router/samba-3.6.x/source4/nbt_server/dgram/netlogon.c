/* 
   Unix SMB/CIFS implementation.

   NBT datagram netlogon server

   Copyright (C) Andrew Tridgell	2005
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2008
  
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
#include "lib/socket/socket.h"
#include <ldb.h>
#include "dsdb/samdb/samdb.h"
#include "auth/auth.h"
#include "param/param.h"
#include "smbd/service_task.h"
#include "cldap_server/cldap_server.h"
#include "libcli/security/security.h"
#include "nbt_server/dgram/proto.h"

/*
  reply to a GETDC request
 */
static void nbtd_netlogon_getdc(struct dgram_mailslot_handler *dgmslot, 
				struct nbtd_interface *iface,
				struct nbt_dgram_packet *packet, 
				const struct socket_address *src,
				struct nbt_netlogon_packet *netlogon)
{
	struct nbt_name *name = &packet->data.msg.dest_name;
	struct nbtd_interface *reply_iface = nbtd_find_reply_iface(iface, src->addr, false);
	struct nbt_netlogon_response_from_pdc *pdc;
	struct ldb_context *samctx;
	struct nbt_netlogon_response netlogon_response;

	/* only answer getdc requests on the PDC or LOGON names */
	if (name->type != NBT_NAME_PDC && name->type != NBT_NAME_LOGON) {
		return;
	}

	samctx = iface->nbtsrv->sam_ctx;

	if (lpcfg_server_role(iface->nbtsrv->task->lp_ctx) != ROLE_DOMAIN_CONTROLLER
	    || !samdb_is_pdc(samctx)) {
		DEBUG(2, ("Not a PDC, so not processing LOGON_PRIMARY_QUERY\n"));
		return;		
	}

	if (strcasecmp_m(name->name, lpcfg_workgroup(iface->nbtsrv->task->lp_ctx)) != 0) {
		DEBUG(5,("GetDC requested for a domian %s that we don't host\n", name->name));
		return;
	}

	/* setup a GETDC reply */
	ZERO_STRUCT(netlogon_response);
	netlogon_response.response_type = NETLOGON_GET_PDC;
	pdc = &netlogon_response.data.get_pdc;

	pdc->command = NETLOGON_RESPONSE_FROM_PDC;
	pdc->pdc_name         = lpcfg_netbios_name(iface->nbtsrv->task->lp_ctx);
	pdc->unicode_pdc_name = pdc->pdc_name;
	pdc->domain_name      = lpcfg_workgroup(iface->nbtsrv->task->lp_ctx);
	pdc->nt_version       = 1;
	pdc->lmnt_token       = 0xFFFF;
	pdc->lm20_token       = 0xFFFF;

	dgram_mailslot_netlogon_reply(reply_iface->dgmsock, 
				      packet, 
				      lpcfg_netbios_name(iface->nbtsrv->task->lp_ctx),
				      netlogon->req.pdc.mailslot_name,
				      &netlogon_response);
}


/*
  reply to a ADS style GETDC request
 */
static void nbtd_netlogon_samlogon(struct dgram_mailslot_handler *dgmslot,
				   struct nbtd_interface *iface,
				   struct nbt_dgram_packet *packet, 
				   const struct socket_address *src,
				   struct nbt_netlogon_packet *netlogon)
{
	struct nbt_name *name = &packet->data.msg.dest_name;
	struct nbtd_interface *reply_iface = nbtd_find_reply_iface(iface, src->addr, false);
	struct ldb_context *samctx;
	const char *my_ip = reply_iface->ip_address; 
	struct dom_sid *sid;
	struct nbt_netlogon_response netlogon_response;
	NTSTATUS status;

	if (!my_ip) {
		DEBUG(0, ("Could not obtain own IP address for datagram socket\n"));
		return;
	}

	/* only answer getdc requests on the PDC or LOGON names */
	if (name->type != NBT_NAME_PDC && name->type != NBT_NAME_LOGON) {
		return;
	}

	samctx = iface->nbtsrv->sam_ctx;

	if (netlogon->req.logon.sid_size) {
		sid = &netlogon->req.logon.sid;
	} else {
		sid = NULL;
	}

	status = fill_netlogon_samlogon_response(samctx, packet, NULL, name->name, sid, NULL, 
						 netlogon->req.logon.user_name, netlogon->req.logon.acct_control, src->addr, 
						 netlogon->req.logon.nt_version, iface->nbtsrv->task->lp_ctx, &netlogon_response.data.samlogon, false);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(2,("NBT netlogon query failed domain=%s sid=%s version=%d - %s\n",
			 name->name, dom_sid_string(packet, sid), netlogon->req.logon.nt_version, nt_errstr(status)));
		return;
	}

	netlogon_response.response_type = NETLOGON_SAMLOGON;

	packet->data.msg.dest_name.type = 0;

	dgram_mailslot_netlogon_reply(reply_iface->dgmsock, 
				      packet, 
				      lpcfg_netbios_name(iface->nbtsrv->task->lp_ctx),
				      netlogon->req.logon.mailslot_name,
				      &netlogon_response);
}


/*
  handle incoming netlogon mailslot requests
*/
void nbtd_mailslot_netlogon_handler(struct dgram_mailslot_handler *dgmslot, 
				    struct nbt_dgram_packet *packet, 
				    struct socket_address *src)
{
	NTSTATUS status = NT_STATUS_NO_MEMORY;
	struct nbtd_interface *iface = 
		talloc_get_type(dgmslot->private_data, struct nbtd_interface);
	struct nbt_netlogon_packet *netlogon = 
		talloc(dgmslot, struct nbt_netlogon_packet);
	struct nbtd_iface_name *iname;
	struct nbt_name *name = &packet->data.msg.dest_name;

	if (netlogon == NULL) goto failed;

	/*
	  see if the we are listening on the destination netbios name
	*/
	iname = nbtd_find_iname(iface, name, 0);
	if (iname == NULL) {
		status = NT_STATUS_BAD_NETWORK_NAME;
		goto failed;
	}

	DEBUG(2,("netlogon request to %s from %s:%d\n", 
		 nbt_name_string(netlogon, name), src->addr, src->port));
	status = dgram_mailslot_netlogon_parse_request(dgmslot, netlogon, packet, netlogon);
	if (!NT_STATUS_IS_OK(status)) goto failed;

	switch (netlogon->command) {
	case LOGON_PRIMARY_QUERY:
		nbtd_netlogon_getdc(dgmslot, iface, packet, 
				    src, netlogon);
		break;
	case LOGON_SAM_LOGON_REQUEST:
		nbtd_netlogon_samlogon(dgmslot, iface, packet, 
				       src, netlogon);
		break;
	default:
		DEBUG(2,("unknown netlogon op %d from %s:%d\n", 
			 netlogon->command, src->addr, src->port));
		NDR_PRINT_DEBUG(nbt_netlogon_packet, netlogon);
		break;
	}

	talloc_free(netlogon);
	return;

failed:
	DEBUG(2,("nbtd netlogon handler failed from %s:%d to %s - %s\n",
		 src->addr, src->port, nbt_name_string(netlogon, name),
		 nt_errstr(status)));
	talloc_free(netlogon);
}
