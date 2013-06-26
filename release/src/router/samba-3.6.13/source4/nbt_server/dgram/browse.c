/* 
   Unix SMB/CIFS implementation.

   NBT datagram browse server

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
#include "lib/socket/socket.h"
#include "librpc/gen_ndr/ndr_nbt.h"
#include "nbt_server/dgram/proto.h"

static const char *nbt_browse_opcode_string(enum nbt_browse_opcode r)
{
	const char *val = NULL;

	switch (r) {
		case HostAnnouncement: val = "HostAnnouncement"; break;
		case AnnouncementRequest: val = "AnnouncementRequest"; break;
		case Election: val = "Election"; break;
		case GetBackupListReq: val = "GetBackupListReq"; break;
		case GetBackupListResp: val = "GetBackupListResp"; break;
		case BecomeBackup: val = "BecomeBackup"; break;
		case DomainAnnouncement: val = "DomainAnnouncement"; break;
		case MasterAnnouncement: val = "MasterAnnouncement"; break;
		case ResetBrowserState: val = "ResetBrowserState"; break;
		case LocalMasterAnnouncement: val = "LocalMasterAnnouncement"; break;
	}

	return val;
}

/*
  handle incoming browse mailslot requests
*/
void nbtd_mailslot_browse_handler(struct dgram_mailslot_handler *dgmslot, 
				  struct nbt_dgram_packet *packet, 
				  struct socket_address *src)
{
	struct nbt_browse_packet *browse = talloc(dgmslot, struct nbt_browse_packet);
	struct nbt_name *name = &packet->data.msg.dest_name;
	NTSTATUS status;

	if (browse == NULL) {
		status = NT_STATUS_INVALID_PARAMETER;
		goto failed;
	}

	status = dgram_mailslot_browse_parse(dgmslot, browse, packet, browse);
	if (!NT_STATUS_IS_OK(status)) goto failed;

	DEBUG(4,("Browse %s (Op %d) on '%s' '%s' from %s:%d\n", 
		nbt_browse_opcode_string(browse->opcode), browse->opcode,
		nbt_name_string(browse, name), dgmslot->mailslot_name,
		src->addr, src->port));

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_DEBUG(nbt_browse_packet, browse);
	}

	talloc_free(browse);
	return;

failed:
	DEBUG(2,("nbtd browse handler failed from %s:%d to %s - %s\n",
		 src->addr, src->port, nbt_name_string(browse, name),
		 nt_errstr(status)));
	talloc_free(browse);

}
