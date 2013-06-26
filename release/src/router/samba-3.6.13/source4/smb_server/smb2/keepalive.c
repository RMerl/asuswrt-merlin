/* 
   Unix SMB2 implementation.
   
   Copyright (C) Stefan Metzmacher	2005
   
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
#include "libcli/smb2/smb2.h"
#include "smb_server/smb_server.h"
#include "smb_server/smb2/smb2_server.h"

static NTSTATUS smb2srv_keepalive_backend(struct smb2srv_request *req)
{
	/* TODO: maybe update some flags on the connection struct */
	return NT_STATUS_OK;
}

static void smb2srv_keepalive_send(struct smb2srv_request *req)
{
	NTSTATUS status;

	if (NT_STATUS_IS_ERR(req->status)) {
		smb2srv_send_error(req, req->status);
		return;
	}

	status = smb2srv_setup_reply(req, 0x04, false, 0);
	if (!NT_STATUS_IS_OK(status)) {
		smbsrv_terminate_connection(req->smb_conn, nt_errstr(status));
		talloc_free(req);
		return;
	}

	SSVAL(req->out.body, 0x02, 0);

	smb2srv_send_reply(req);
}

void smb2srv_keepalive_recv(struct smb2srv_request *req)
{
	uint16_t _pad;

	if (req->in.body_size != 0x04) {
		smb2srv_send_error(req,  NT_STATUS_INVALID_PARAMETER);
		return;
	}

	if (SVAL(req->in.body, 0x00) != 0x04) {
		smb2srv_send_error(req,  NT_STATUS_INVALID_PARAMETER);
		return;
	}

	_pad	= SVAL(req->in.body, 0x02);

	req->status = smb2srv_keepalive_backend(req);

	if (req->control_flags & SMB2SRV_REQ_CTRL_FLAG_NOT_REPLY) {
		talloc_free(req);
		return;
	}
	smb2srv_keepalive_send(req);
}
