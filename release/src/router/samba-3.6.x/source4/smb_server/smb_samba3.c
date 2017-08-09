/*
   Unix SMB/CIFS implementation.

   process incoming connections and fork a samba3 in inetd mode

   Copyright (C) Stefan Metzmacher	2008

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
#include "smbd/service.h"
#include "libcli/smb2/smb2.h"
#include "system/network.h"
#include "lib/socket/netif.h"
#include "param/param.h"
#include "dynconfig/dynconfig.h"
#include "smbd/process_model.h"

/*
  initialise a server_context from a open socket and register a event handler
  for reading from that socket
*/
static void samba3_smb_accept(struct stream_connection *conn)
{
	int i;
	int fd = socket_get_fd(conn->socket);
	const char *prog;
	char *argv[2];
	char *reason;

	close(0);
	close(1);
	dup2(fd, 0);
	dup2(fd, 1);
	dup2(fd, 2);
	for (i=3;i<256;i++) {
		close(i);
	}

	prog = lpcfg_parm_string(conn->lp_ctx, NULL, "samba3", "smbd");

	if (prog == NULL) {
		argv[0] = talloc_asprintf(conn, "%s/%s", dyn_BINDIR, "smbd3");
	}
	else {
		argv[0] = talloc_strdup(conn, prog);
	}

	if (argv[0] == NULL) {
		stream_terminate_connection(conn, "out of memory");
		return;
	}
	argv[1] = NULL;

	execv(argv[0], argv);

	/*
	 * Should never get here
	 */
	reason = talloc_asprintf(conn, "Could not execute %s", argv[0]);
	if (reason == NULL) {
		stream_terminate_connection(conn, "out of memory");
		return;
	}
	stream_terminate_connection(conn, reason);
	talloc_free(reason);
}

static const struct stream_server_ops samba3_smb_stream_ops = {
	.name			= "samba3",
	.accept_connection	= samba3_smb_accept,
};

/*
  setup a listening socket on all the SMB ports for a particular address
*/
static NTSTATUS samba3_add_socket(struct task_server *task,
				  struct tevent_context *event_context,
				  struct loadparm_context *lp_ctx,
				  const struct model_ops *model_ops,
				  const char *address)
{
	const char **ports = lpcfg_smb_ports(lp_ctx);
	int i;
	NTSTATUS status;

	for (i=0;ports[i];i++) {
		uint16_t port = atoi(ports[i]);
		if (port == 0) continue;
		status = stream_setup_socket(task, event_context, lp_ctx,
					     model_ops, &samba3_smb_stream_ops,
					     "ip", address, &port,
					     lpcfg_socket_options(lp_ctx),
					     NULL);
		NT_STATUS_NOT_OK_RETURN(status);
	}

	return NT_STATUS_OK;
}


/*
  open the smb server sockets
*/
static void samba3_smb_task_init(struct task_server *task)
{
	NTSTATUS status;
	const struct model_ops *model_ops;

	model_ops = process_model_startup("standard");

	if (model_ops == NULL) {
		goto failed;
	}

	task_server_set_title(task, "task[samba3_smb]");

	if (lpcfg_interfaces(task->lp_ctx)
	    && lpcfg_bind_interfaces_only(task->lp_ctx)) {
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
			status = samba3_add_socket(task,
						   task->event_ctx,
						   task->lp_ctx,
						   model_ops, address);
			if (!NT_STATUS_IS_OK(status)) goto failed;
		}
	} else {
		/* Just bind to lpcfg_socket_address() (usually 0.0.0.0) */
		status = samba3_add_socket(task,
					   task->event_ctx, task->lp_ctx,
					   model_ops,
					   lpcfg_socket_address(task->lp_ctx));
		if (!NT_STATUS_IS_OK(status)) goto failed;
	}

	return;
failed:
	task_server_terminate(task, "Failed to startup samba3 smb task", true);
}

/* called at smbd startup - register ourselves as a server service */
NTSTATUS server_service_samba3_smb_init(void)
{
	return register_server_service("samba3_smb", samba3_smb_task_init);
}
