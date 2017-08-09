/* 
   Unix SMB/CIFS implementation.

   SMB2 client read call

   Copyright (C) Andrew Tridgell 2005
   
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
#include "libcli/smb2/smb2_calls.h"

/*
  send a read request
*/
struct smb2_request *smb2_read_send(struct smb2_tree *tree, struct smb2_read *io)
{
	struct smb2_request *req;

	req = smb2_request_init_tree(tree, SMB2_OP_READ, 0x30, true, 0);
	if (req == NULL) return NULL;

	SCVAL(req->out.body, 0x02, 0); /* pad */
	SCVAL(req->out.body, 0x03, 0); /* reserved */
	SIVAL(req->out.body, 0x04, io->in.length);
	SBVAL(req->out.body, 0x08, io->in.offset);
	smb2_push_handle(req->out.body+0x10, &io->in.file.handle);
	SIVAL(req->out.body, 0x20, io->in.min_count);
	SIVAL(req->out.body, 0x24, io->in.channel);
	SIVAL(req->out.body, 0x28, io->in.remaining);
	SSVAL(req->out.body, 0x2C, io->in.channel_offset);
	SSVAL(req->out.body, 0x2E, io->in.channel_length);

	smb2_transport_send(req);

	return req;
}


/*
  recv a read reply
*/
NTSTATUS smb2_read_recv(struct smb2_request *req, 
			TALLOC_CTX *mem_ctx, struct smb2_read *io)
{
	NTSTATUS status;

	if (!smb2_request_receive(req) || 
	    !smb2_request_is_ok(req)) {
		return smb2_request_destroy(req);
	}

	SMB2_CHECK_PACKET_RECV(req, 0x10, true);

	status = smb2_pull_o16s32_blob(&req->in, mem_ctx, req->in.body+0x02, &io->out.data);
	if (!NT_STATUS_IS_OK(status)) {
		smb2_request_destroy(req);
		return status;
	}

	io->out.remaining = IVAL(req->in.body, 0x08);
	io->out.reserved  = IVAL(req->in.body, 0x0C);

	return smb2_request_destroy(req);
}

/*
  sync read request
*/
NTSTATUS smb2_read(struct smb2_tree *tree, TALLOC_CTX *mem_ctx, struct smb2_read *io)
{
	struct smb2_request *req = smb2_read_send(tree, io);
	return smb2_read_recv(req, mem_ctx, io);
}
