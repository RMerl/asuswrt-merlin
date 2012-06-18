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
#include "libcli/smb2/smb2.h"
#include "libcli/smb2/smb2_calls.h"

/*
  send a cancel request
*/
NTSTATUS smb2_cancel(struct smb2_request *r)
{
	NTSTATUS status;
	struct smb2_request *c;
	uint32_t old_timeout;
	uint64_t old_seqnum;

	/* 
	 * if we don't get a pending id yet, we just
	 * mark the request for pending, so that we directly
	 * send the cancel after getting the pending id
	 */
	if (!r->cancel.can_cancel) {
		r->cancel.do_cancel++;
		return NT_STATUS_OK;
	}

	/* we don't want a seqmun for a SMB2 Cancel */
	old_seqnum = r->transport->seqnum;
	c = smb2_request_init(r->transport, SMB2_OP_CANCEL, 0x04, false, 0);
	r->transport->seqnum = old_seqnum;
	NT_STATUS_HAVE_NO_MEMORY(c);
	c->seqnum = 0;

	SIVAL(c->out.hdr, SMB2_HDR_FLAGS,	0x00000002);
	SSVAL(c->out.hdr, SMB2_HDR_CREDIT,	0x0030);
	SIVAL(c->out.hdr, SMB2_HDR_PID,		r->cancel.pending_id);
	SBVAL(c->out.hdr, SMB2_HDR_MESSAGE_ID,	c->seqnum);
	if (r->session) {
		SBVAL(c->out.hdr, SMB2_HDR_SESSION_ID,	r->session->uid);
	}

	SSVAL(c->out.body, 0x02, 0);

	old_timeout = c->transport->options.request_timeout;
	c->transport->options.request_timeout = 0;
	smb2_transport_send(c);
	c->transport->options.request_timeout = old_timeout;

	if (c->state == SMB2_REQUEST_ERROR) {
		status = c->status;
	} else {
		status = NT_STATUS_OK;
	}

	talloc_free(c);
	return status;
}
