/* 
   Unix SMB/CIFS implementation.

   SMB2 client write call

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
  send a write request
*/
struct smb2_request *smb2_write_send(struct smb2_tree *tree, struct smb2_write *io)
{
	NTSTATUS status;
	struct smb2_request *req;

	req = smb2_request_init_tree(tree, SMB2_OP_WRITE, 0x30, true, io->in.data.length);
	if (req == NULL) return NULL;

	status = smb2_push_o16s32_blob(&req->out, 0x02, io->in.data);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(req);
		return NULL;
	}

	SBVAL(req->out.body, 0x08, io->in.offset);
	smb2_push_handle(req->out.body+0x10, &io->in.file.handle);

	SBVAL(req->out.body, 0x20, io->in.unknown1);
	SBVAL(req->out.body, 0x28, io->in.unknown2);

	smb2_transport_send(req);

	return req;
}


/*
  recv a write reply
*/
NTSTATUS smb2_write_recv(struct smb2_request *req, struct smb2_write *io)
{
	if (!smb2_request_receive(req) || 
	    smb2_request_is_error(req)) {
		return smb2_request_destroy(req);
	}

	SMB2_CHECK_PACKET_RECV(req, 0x10, true);

	io->out._pad     = SVAL(req->in.body, 0x02);
	io->out.nwritten = IVAL(req->in.body, 0x04);
	io->out.unknown1 = BVAL(req->in.body, 0x08);

	return smb2_request_destroy(req);
}

/*
  sync write request
*/
NTSTATUS smb2_write(struct smb2_tree *tree, struct smb2_write *io)
{
	struct smb2_request *req = smb2_write_send(tree, io);
	return smb2_write_recv(req, io);
}
