/* 
   Unix SMB/CIFS implementation.

   SMB2 client logoff handling

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
  send a logoff request
*/
struct smb2_request *smb2_logoff_send(struct smb2_session *session)
{
	struct smb2_request *req;

	req = smb2_request_init(session->transport, SMB2_OP_LOGOFF, 0x04, false, 0);
	if (req == NULL) return NULL;

	req->session = session;

	SBVAL(req->out.hdr,  SMB2_HDR_SESSION_ID, session->uid);

	SSVAL(req->out.body, 0x02, 0);

	smb2_transport_send(req);

	return req;
}


/*
  recv a logoff reply
*/
NTSTATUS smb2_logoff_recv(struct smb2_request *req)
{
	if (!smb2_request_receive(req) || 
	    !smb2_request_is_ok(req)) {
		return smb2_request_destroy(req);
	}

	SMB2_CHECK_PACKET_RECV(req, 0x04, false);
	return smb2_request_destroy(req);
}

/*
  sync logoff request
*/
NTSTATUS smb2_logoff(struct smb2_session *session)
{
	struct smb2_request *req = smb2_logoff_send(session);
	return smb2_logoff_recv(req);
}
