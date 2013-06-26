/* 
   Unix SMB/CIFS implementation.
   process incoming packets - main loop
   Copyright (C) Andrew Tridgell	2004-2005
   Copyright (C) Stefan Metzmacher	2004-2005
   
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
#include "smbd/service_stream.h"
#include "smbd/service.h"
#include "smb_server/smb_server.h"
#include "smb_server/service_smb_proto.h"
#include "lib/messaging/irpc.h"
#include "lib/stream/packet.h"
#include "libcli/smb2/smb2.h"
#include "smb_server/smb2/smb2_server.h"
#include "system/network.h"
#include "lib/socket/netif.h"
#include "param/share.h"
#include "dsdb/samdb/samdb.h"
#include "param/param.h"

/*
  open the smb server sockets
*/
static void smbsrv_task_init(struct task_server *task)
{	
	NTSTATUS status;

	task_server_set_title(task, "task[smbsrv]");

	if (lpcfg_interfaces(task->lp_ctx) && lpcfg_bind_interfaces_only(task->lp_ctx)) {
		int num_interfaces;
		int i;
		struct interface *ifaces;

		load_interfaces(task, lpcfg_interfaces(task->lp_ctx), &ifaces);

		num_interfaces = iface_count(ifaces);

		/* We have been given an interfaces line, and been 
		   told to only bind to those interfaces. Create a
		   socket per interface and bind to only these.
		*/
		for(i = 0; i < num_interfaces; i++) {
			const char *address = iface_n_ip(ifaces, i);
			status = smbsrv_add_socket(task, task->event_ctx, task->lp_ctx, task->model_ops, address);
			if (!NT_STATUS_IS_OK(status)) goto failed;
		}
	} else {
		/* Just bind to lpcfg_socket_address() (usually 0.0.0.0) */
		status = smbsrv_add_socket(task, task->event_ctx, task->lp_ctx, task->model_ops,
					   lpcfg_socket_address(task->lp_ctx));
		if (!NT_STATUS_IS_OK(status)) goto failed;
	}

	return;
failed:
	task_server_terminate(task, "Failed to startup smb server task", true);	
}

/* called at smbd startup - register ourselves as a server service */
NTSTATUS server_service_smb_init(void)
{
	share_init();
	return register_server_service("smb", smbsrv_task_init);
}
