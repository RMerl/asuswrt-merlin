/*
   Unix SMB/CIFS implementation.

   SMB2 client oplock break handling

   Copyright (C) Zachary Loafman 2009

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
  Send a Lease Break Acknowledgement
*/
struct smb2_request *smb2_lease_break_ack_send(struct smb2_tree *tree,
                                               struct smb2_lease_break_ack *io)
{
	struct smb2_request *req;

	req = smb2_request_init_tree(tree, SMB2_OP_BREAK, 0x24, false, 0);
	if (req == NULL) return NULL;

	SIVAL(req->out.body, 0x02, io->in.reserved);
	SIVAL(req->out.body, 0x04, io->in.lease.lease_flags);
	memcpy(req->out.body+0x8, &io->in.lease.lease_key,
	    sizeof(struct smb2_lease_key));
	SIVAL(req->out.body, 0x18, io->in.lease.lease_state);
	SBVAL(req->out.body, 0x1C, io->in.lease.lease_duration);

	smb2_transport_send(req);

	return req;
}


/*
  Receive a Lease Break Response
*/
NTSTATUS smb2_lease_break_ack_recv(struct smb2_request *req,
                                   struct smb2_lease_break_ack *io)
{
	if (!smb2_request_receive(req) ||
	    !smb2_request_is_ok(req)) {
		return smb2_request_destroy(req);
	}

	SMB2_CHECK_PACKET_RECV(req, 0x24, false);

	io->out.reserved		= IVAL(req->in.body, 0x02);
	io->out.lease.lease_flags	= IVAL(req->in.body, 0x04);
	memcpy(&io->out.lease.lease_key, req->in.body+0x8,
	    sizeof(struct smb2_lease_key));
	io->out.lease.lease_state	= IVAL(req->in.body, 0x18);
	io->out.lease.lease_duration	= IVAL(req->in.body, 0x1C);

	return smb2_request_destroy(req);
}

/*
  sync flush request
*/
NTSTATUS smb2_lease_break_ack(struct smb2_tree *tree,
                              struct smb2_lease_break_ack *io)
{
	struct smb2_request *req = smb2_lease_break_ack_send(tree, io);
	return smb2_lease_break_ack_recv(req, io);
}
