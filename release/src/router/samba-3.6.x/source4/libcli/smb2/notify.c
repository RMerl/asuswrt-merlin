/* 
   Unix SMB/CIFS implementation.

   SMB2 client notify calls

   Copyright (C) Stefan Metzmacher 2006
   
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
#include "libcli/raw/libcliraw.h"
#include "libcli/raw/raw_proto.h"
#include "libcli/smb2/smb2.h"
#include "libcli/smb2/smb2_calls.h"

/*
  send a notify request
*/
struct smb2_request *smb2_notify_send(struct smb2_tree *tree, struct smb2_notify *io)
{
	struct smb2_request *req;
	uint32_t old_timeout;

	req = smb2_request_init_tree(tree, SMB2_OP_NOTIFY, 0x20, false, 0);
	if (req == NULL) return NULL;

	SSVAL(req->out.hdr,  SMB2_HDR_CREDIT,	0x0030);

	SSVAL(req->out.body, 0x02, io->in.recursive);
	SIVAL(req->out.body, 0x04, io->in.buffer_size);
	smb2_push_handle(req->out.body+0x08, &io->in.file.handle);
	SIVAL(req->out.body, 0x18, io->in.completion_filter);
	SIVAL(req->out.body, 0x1C, io->in.unknown);

	old_timeout = req->transport->options.request_timeout;
	req->transport->options.request_timeout = 0;
	smb2_transport_send(req);
	req->transport->options.request_timeout = old_timeout;

	return req;
}


/*
  recv a notify reply
*/
NTSTATUS smb2_notify_recv(struct smb2_request *req, TALLOC_CTX *mem_ctx,
			  struct smb2_notify *io)
{
	NTSTATUS status;
	DATA_BLOB blob;
	uint32_t ofs, i;

	if (!smb2_request_receive(req) || 
	    !smb2_request_is_ok(req)) {
		return smb2_request_destroy(req);
	}

	SMB2_CHECK_PACKET_RECV(req, 0x08, true);

	status = smb2_pull_o16s32_blob(&req->in, mem_ctx, req->in.body+0x02, &blob);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	io->out.changes = NULL;
	io->out.num_changes = 0;

	/* count them */
	for (ofs=0; blob.length - ofs > 12; ) {
		uint32_t next = IVAL(blob.data, ofs);
		io->out.num_changes++;
		if (next == 0 || (ofs + next) >= blob.length) break;
		ofs += next;
	}

	/* allocate array */
	io->out.changes = talloc_array(mem_ctx, struct notify_changes, io->out.num_changes);
	if (!io->out.changes) {
		return NT_STATUS_NO_MEMORY;
	}

	for (i=ofs=0; i<io->out.num_changes; i++) {
		io->out.changes[i].action = IVAL(blob.data, ofs+4);
		smbcli_blob_pull_string(NULL, mem_ctx, &blob,
					&io->out.changes[i].name,
					ofs+8, ofs+12, STR_UNICODE);
		ofs += IVAL(blob.data, ofs);
	}

	return smb2_request_destroy(req);
}

/*
  sync notify request
*/
NTSTATUS smb2_notify(struct smb2_tree *tree, TALLOC_CTX *mem_ctx,
		     struct smb2_notify *io)
{
	struct smb2_request *req = smb2_notify_send(tree, io);
	return smb2_notify_recv(req, mem_ctx, io);
}
