/* 
   Unix SMB/CIFS implementation.

   SMB2 client tree handling

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
  initialise a smb2_session structure
 */
struct smb2_tree *smb2_tree_init(struct smb2_session *session,
				 TALLOC_CTX *parent_ctx, bool primary)
{
	struct smb2_tree *tree;

	tree = talloc_zero(parent_ctx, struct smb2_tree);
	if (!session) {
		return NULL;
	}
	if (primary) {
		tree->session = talloc_steal(tree, session);
	} else {
		tree->session = talloc_reference(tree, session);
	}
	return tree;
}

/*
  send a tree connect
*/
struct smb2_request *smb2_tree_connect_send(struct smb2_tree *tree, 
					    struct smb2_tree_connect *io)
{
	struct smb2_request *req;
	NTSTATUS status;

	req = smb2_request_init(tree->session->transport, SMB2_OP_TCON, 
				0x08, true, 0);
	if (req == NULL) return NULL;

	SBVAL(req->out.hdr,  SMB2_HDR_SESSION_ID, tree->session->uid);
	req->session = tree->session;

	SSVAL(req->out.body, 0x02, io->in.reserved);
	status = smb2_push_o16s16_string(&req->out, 0x04, io->in.path);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(req);
		return NULL;
	}

	smb2_transport_send(req);

	return req;
}


/*
  recv a tree connect reply
*/
NTSTATUS smb2_tree_connect_recv(struct smb2_request *req, struct smb2_tree_connect *io)
{
	if (!smb2_request_receive(req) || 
	    smb2_request_is_error(req)) {
		return smb2_request_destroy(req);
	}

	SMB2_CHECK_PACKET_RECV(req, 0x10, false);

	io->out.tid      = IVAL(req->in.hdr,  SMB2_HDR_TID);

	io->out.share_type  = CVAL(req->in.body, 0x02);
	io->out.reserved    = CVAL(req->in.body, 0x03);
	io->out.flags       = IVAL(req->in.body, 0x04);
	io->out.capabilities= IVAL(req->in.body, 0x08);
	io->out.access_mask = IVAL(req->in.body, 0x0C);

	if (io->out.capabilities & ~SMB2_CAP_ALL) {
		DEBUG(0,("Unknown capabilities mask 0x%x\n", io->out.capabilities));
	}
	if (io->out.flags & ~SMB2_SHAREFLAG_ALL) {
		DEBUG(0,("Unknown tcon shareflag 0x%x\n", io->out.flags));
	}
	
	return smb2_request_destroy(req);
}

/*
  sync tree connect request
*/
NTSTATUS smb2_tree_connect(struct smb2_tree *tree, struct smb2_tree_connect *io)
{
	struct smb2_request *req = smb2_tree_connect_send(tree, io);
	return smb2_tree_connect_recv(req, io);
}
