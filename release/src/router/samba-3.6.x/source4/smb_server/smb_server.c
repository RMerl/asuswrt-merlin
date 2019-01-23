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

static NTSTATUS smbsrv_recv_generic_request(void *private_data, DATA_BLOB blob)
{
	NTSTATUS status;
	struct smbsrv_connection *smb_conn = talloc_get_type(private_data, struct smbsrv_connection);
	uint32_t protocol_version;

	/* see if its a special NBT packet */
	if (CVAL(blob.data,0) != 0) {
		status = smbsrv_init_smb_connection(smb_conn, smb_conn->lp_ctx);
		NT_STATUS_NOT_OK_RETURN(status);
		packet_set_callback(smb_conn->packet, smbsrv_recv_smb_request);
		return smbsrv_recv_smb_request(smb_conn, blob);
	}

	if (blob.length < (NBT_HDR_SIZE + MIN_SMB_SIZE)) {
		DEBUG(2,("Invalid SMB packet length count %ld\n", (long)blob.length));
		smbsrv_terminate_connection(smb_conn, "Invalid SMB packet");
		return NT_STATUS_OK;
	}

	protocol_version = IVAL(blob.data, NBT_HDR_SIZE);

	switch (protocol_version) {
	case SMB_MAGIC:
		status = smbsrv_init_smb_connection(smb_conn, smb_conn->lp_ctx);
		NT_STATUS_NOT_OK_RETURN(status);
		packet_set_callback(smb_conn->packet, smbsrv_recv_smb_request);
		return smbsrv_recv_smb_request(smb_conn, blob);
	case SMB2_MAGIC:
		if (lpcfg_srv_maxprotocol(smb_conn->lp_ctx) < PROTOCOL_SMB2) break;
		status = smbsrv_init_smb2_connection(smb_conn);
		NT_STATUS_NOT_OK_RETURN(status);
		packet_set_callback(smb_conn->packet, smbsrv_recv_smb2_request);
		return smbsrv_recv_smb2_request(smb_conn, blob);
	}

	DEBUG(2,("Invalid SMB packet: protocol prefix: 0x%08X\n", protocol_version));
	smbsrv_terminate_connection(smb_conn, "NON-SMB packet");
	return NT_STATUS_OK;
}

/*
  close the socket and shutdown a server_context
*/
void smbsrv_terminate_connection(struct smbsrv_connection *smb_conn, const char *reason)
{
	stream_terminate_connection(smb_conn->connection, reason);
}

/*
  called when a SMB socket becomes readable
*/
static void smbsrv_recv(struct stream_connection *conn, uint16_t flags)
{
	struct smbsrv_connection *smb_conn = talloc_get_type(conn->private_data,
							     struct smbsrv_connection);

	DEBUG(10,("smbsrv_recv\n"));

	packet_recv(smb_conn->packet);
}

/*
  called when a SMB socket becomes writable
*/
static void smbsrv_send(struct stream_connection *conn, uint16_t flags)
{
	struct smbsrv_connection *smb_conn = talloc_get_type(conn->private_data,
							     struct smbsrv_connection);
	packet_queue_run(smb_conn->packet);
}

/*
  handle socket recv errors
*/
static void smbsrv_recv_error(void *private_data, NTSTATUS status)
{
	struct smbsrv_connection *smb_conn = talloc_get_type(private_data, struct smbsrv_connection);
	
	smbsrv_terminate_connection(smb_conn, nt_errstr(status));
}

/*
  initialise a server_context from a open socket and register a event handler
  for reading from that socket
*/
static void smbsrv_accept(struct stream_connection *conn)
{
	struct smbsrv_connection *smb_conn;

	DEBUG(5,("smbsrv_accept\n"));

	smb_conn = talloc_zero(conn, struct smbsrv_connection);
	if (!smb_conn) {
		stream_terminate_connection(conn, "out of memory");
		return;
	}

	smb_conn->packet = packet_init(smb_conn);
	if (!smb_conn->packet) {
		smbsrv_terminate_connection(smb_conn, "out of memory");
		return;
	}
	packet_set_private(smb_conn->packet, smb_conn);
	packet_set_socket(smb_conn->packet, conn->socket);
	packet_set_callback(smb_conn->packet, smbsrv_recv_generic_request);
	packet_set_full_request(smb_conn->packet, packet_full_request_nbt);
	packet_set_error_handler(smb_conn->packet, smbsrv_recv_error);
	packet_set_event_context(smb_conn->packet, conn->event.ctx);
	packet_set_fde(smb_conn->packet, conn->event.fde);
	packet_set_serialise(smb_conn->packet);
	packet_set_initial_read(smb_conn->packet, 4);

	smb_conn->lp_ctx = conn->lp_ctx;
	smb_conn->connection = conn;
	conn->private_data = smb_conn;

	smb_conn->statistics.connect_time = timeval_current();

	smbsrv_management_init(smb_conn);

	irpc_add_name(conn->msg_ctx, "smb_server");

	if (!NT_STATUS_IS_OK(share_get_context_by_name(smb_conn, lpcfg_share_backend(smb_conn->lp_ctx),
						       smb_conn->connection->event.ctx,
						       smb_conn->lp_ctx, &(smb_conn->share_context)))) {
		smbsrv_terminate_connection(smb_conn, "share_init failed!");
		return;
	}
}

static const struct stream_server_ops smb_stream_ops = {
	.name			= "smbsrv",
	.accept_connection	= smbsrv_accept,
	.recv_handler		= smbsrv_recv,
	.send_handler		= smbsrv_send,
};

/*
  setup a listening socket on all the SMB ports for a particular address
*/
_PUBLIC_ NTSTATUS smbsrv_add_socket(TALLOC_CTX *mem_ctx,
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
		status = stream_setup_socket(mem_ctx, event_context, lp_ctx,
					     model_ops, &smb_stream_ops, 
					     "ipv4", address, &port, 
					     lpcfg_socket_options(lp_ctx),
					     NULL);
		NT_STATUS_NOT_OK_RETURN(status);
	}

	return NT_STATUS_OK;
}



