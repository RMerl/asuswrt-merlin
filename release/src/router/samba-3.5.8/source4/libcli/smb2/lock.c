/* 
   Unix SMB/CIFS implementation.

   SMB2 client lock handling

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
#include "libcli/smb2/smb2.h"
#include "libcli/smb2/smb2_calls.h"

/*
  send a lock request
*/
struct smb2_request *smb2_lock_send(struct smb2_tree *tree, struct smb2_lock *io)
{
	struct smb2_request *req;
	int i;

	req = smb2_request_init_tree(tree, SMB2_OP_LOCK, 
				     24 + io->in.lock_count*24, false, 0);
	if (req == NULL) return NULL;

	/* this is quite bizarre - the spec says we must lie about the length! */
	SSVAL(req->out.body, 0, 0x30);

	SSVAL(req->out.body, 0x02, io->in.lock_count);
	SIVAL(req->out.body, 0x04, io->in.reserved);
	smb2_push_handle(req->out.body+0x08, &io->in.file.handle);

	for (i=0;i<io->in.lock_count;i++) {
		SBVAL(req->out.body, 0x18 + i*24, io->in.locks[i].offset);
		SBVAL(req->out.body, 0x20 + i*24, io->in.locks[i].length);
		SIVAL(req->out.body, 0x28 + i*24, io->in.locks[i].flags);
		SIVAL(req->out.body, 0x2C + i*24, io->in.locks[i].reserved);
	}

	smb2_transport_send(req);

	return req;
}


/*
  recv a lock reply
*/
NTSTATUS smb2_lock_recv(struct smb2_request *req, struct smb2_lock *io)
{
	if (!smb2_request_receive(req) || 
	    smb2_request_is_error(req)) {
		return smb2_request_destroy(req);
	}

	SMB2_CHECK_PACKET_RECV(req, 0x04, false);

	io->out.reserved = SVAL(req->in.body, 0x02);

	return smb2_request_destroy(req);
}

/*
  sync lock request
*/
NTSTATUS smb2_lock(struct smb2_tree *tree, struct smb2_lock *io)
{
	struct smb2_request *req = smb2_lock_send(tree, io);
	return smb2_lock_recv(req, io);
}
