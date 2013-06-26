/* 
   Unix SMB/CIFS implementation.
   client change notify operations
   Copyright (C) Andrew Tridgell 2003
   
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
#include "../lib/util/dlinklist.h"

/****************************************************************************
change notify (async send)
****************************************************************************/
_PUBLIC_ struct smbcli_request *smb_raw_changenotify_send(struct smbcli_tree *tree, union smb_notify *parms)
{
	struct smb_nttrans nt;
	uint8_t setup[8];

	if (parms->nttrans.level != RAW_NOTIFY_NTTRANS) {
		return NULL;
	}

	nt.in.max_setup = 0;
	nt.in.max_param = parms->nttrans.in.buffer_size;
	nt.in.max_data = 0;
	nt.in.setup_count = 4;
	nt.in.setup = setup;
	SIVAL(setup, 0, parms->nttrans.in.completion_filter);
	SSVAL(setup, 4, parms->nttrans.in.file.fnum);
	SSVAL(setup, 6, parms->nttrans.in.recursive);	
	nt.in.function = NT_TRANSACT_NOTIFY_CHANGE;
	nt.in.params = data_blob(NULL, 0);
	nt.in.data = data_blob(NULL, 0);

	return smb_raw_nttrans_send(tree, &nt);
}

/****************************************************************************
change notify (async recv)
****************************************************************************/
_PUBLIC_ NTSTATUS smb_raw_changenotify_recv(struct smbcli_request *req, 
				   TALLOC_CTX *mem_ctx, union smb_notify *parms)
{
	struct smb_nttrans nt;
	NTSTATUS status;
	uint32_t ofs, i;
	struct smbcli_session *session = req?req->session:NULL;

	if (parms->nttrans.level != RAW_NOTIFY_NTTRANS) {
		return NT_STATUS_INVALID_LEVEL;
	}

	status = smb_raw_nttrans_recv(req, mem_ctx, &nt);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	parms->nttrans.out.changes = NULL;
	parms->nttrans.out.num_changes = 0;

	/* count them */
	for (ofs=0; nt.out.params.length - ofs > 12; ) {
		uint32_t next = IVAL(nt.out.params.data, ofs);
		if (next % 4 != 0)
			return NT_STATUS_INVALID_NETWORK_RESPONSE;
		parms->nttrans.out.num_changes++;
		if (next == 0 ||
		    ofs + next >= nt.out.params.length) break;
		ofs += next;
	}

	/* allocate array */
	parms->nttrans.out.changes = talloc_array(mem_ctx, struct notify_changes, parms->nttrans.out.num_changes);
	if (!parms->nttrans.out.changes) {
		return NT_STATUS_NO_MEMORY;
	}

	for (i=ofs=0; i<parms->nttrans.out.num_changes; i++) {
		parms->nttrans.out.changes[i].action = IVAL(nt.out.params.data, ofs+4);
		smbcli_blob_pull_string(session, mem_ctx, &nt.out.params, 
					&parms->nttrans.out.changes[i].name, 
					ofs+8, ofs+12, STR_UNICODE);
		ofs += IVAL(nt.out.params.data, ofs);
	}

	return NT_STATUS_OK;
}

/****************************************************************************
  handle ntcancel replies from the server,
  as the MID of the real reply and the ntcancel reply is the same
  we need to do find out to what request the reply belongs
****************************************************************************/
struct smbcli_request *smbcli_handle_ntcancel_reply(struct smbcli_request *req,
						    size_t len, const uint8_t *hdr)
{
	struct smbcli_request *ntcancel;

	if (!req) return req;

	if (!req->ntcancel) return req;

	if (len >= MIN_SMB_SIZE + NBT_HDR_SIZE &&
	    (CVAL(hdr, HDR_FLG) & FLAG_REPLY) &&
	     CVAL(hdr,HDR_COM) == SMBntcancel) {
		ntcancel = req->ntcancel;
		DLIST_REMOVE(req->ntcancel, ntcancel);

		/*
		 * TODO: untill we understand how the 
		 *       smb_signing works for this case we 
		 *       return NULL, to just ignore the packet
		 */
		/*return ntcancel;*/
		return NULL;
	}

	return req;
}

/****************************************************************************
 Send a NT Cancel request - used to hurry along a pending request. Usually
 used to cancel a pending change notify request
 note that this request does not expect a response!
****************************************************************************/
NTSTATUS smb_raw_ntcancel(struct smbcli_request *oldreq)
{
	struct smbcli_request *req;

	req = smbcli_request_setup_transport(oldreq->transport, SMBntcancel, 0, 0);

	SSVAL(req->out.hdr, HDR_MID, SVAL(oldreq->out.hdr, HDR_MID));	
	SSVAL(req->out.hdr, HDR_PID, SVAL(oldreq->out.hdr, HDR_PID));	
	SSVAL(req->out.hdr, HDR_TID, SVAL(oldreq->out.hdr, HDR_TID));	
	SSVAL(req->out.hdr, HDR_UID, SVAL(oldreq->out.hdr, HDR_UID));	

	/* this request does not expect a reply, so tell the signing
	   subsystem not to allocate an id for a reply */
	req->sign_single_increment = 1;
	req->one_way_request = 1;

	/* 
	 * smbcli_request_send() free's oneway requests
	 * but we want to keep it under oldreq->ntcancel
	 */
	req->do_not_free = true;
	talloc_steal(oldreq, req);

	smbcli_request_send(req);

	DLIST_ADD_END(oldreq->ntcancel, req, struct smbcli_request *);

	return NT_STATUS_OK;
}
