/* 
   Unix SMB/CIFS implementation.

   SMB2 client negprot handling

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
#include "libcli/raw/libcliraw.h"
#include "libcli/raw/raw_proto.h"
#include "libcli/smb2/smb2.h"
#include "libcli/smb2/smb2_calls.h"
#include "librpc/ndr/libndr.h"

/*
  send a negprot request
*/
struct smb2_request *smb2_negprot_send(struct smb2_transport *transport, 
				       struct smb2_negprot *io)
{
	struct smb2_request *req;
	uint16_t size = 0x24 + io->in.dialect_count*2;
	enum ndr_err_code ndr_err;
	int i;

	req = smb2_request_init(transport, SMB2_OP_NEGPROT, size, false, 0);
	if (req == NULL) return NULL;


	SSVAL(req->out.body, 0x00, 0x24);
	SSVAL(req->out.body, 0x02, io->in.dialect_count);
	SSVAL(req->out.body, 0x04, io->in.security_mode);
	SSVAL(req->out.body, 0x06, io->in.reserved);
	SIVAL(req->out.body, 0x08, io->in.capabilities);
	ndr_err = smbcli_push_guid(req->out.body, 0x0C, &io->in.client_guid);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(req);
		return NULL;
	}
	smbcli_push_nttime(req->out.body, 0x1C, io->in.start_time);
	for (i=0;i<io->in.dialect_count;i++) {
		SSVAL(req->out.body, 0x24 + i*2, io->in.dialects[i]);		
	}

	smb2_transport_send(req);

	return req;
}

/*
  recv a negprot reply
*/
NTSTATUS smb2_negprot_recv(struct smb2_request *req, TALLOC_CTX *mem_ctx, 
			   struct smb2_negprot *io)
{
	NTSTATUS status;
	enum ndr_err_code ndr_err;

	if (!smb2_request_receive(req) ||
	    smb2_request_is_error(req)) {
		return smb2_request_destroy(req);
	}

	SMB2_CHECK_PACKET_RECV(req, 0x40, true);

	io->out.security_mode      = SVAL(req->in.body, 0x02);
	io->out.dialect_revision   = SVAL(req->in.body, 0x04);
	io->out.reserved           = SVAL(req->in.body, 0x06);
	ndr_err = smbcli_pull_guid(req->in.body, 0x08, &io->in.client_guid);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		smb2_request_destroy(req);
		return NT_STATUS_INTERNAL_ERROR;
	}
	io->out.capabilities       = IVAL(req->in.body, 0x18);
	io->out.max_transact_size  = IVAL(req->in.body, 0x1C);
	io->out.max_read_size      = IVAL(req->in.body, 0x20);
	io->out.max_write_size     = IVAL(req->in.body, 0x24);
	io->out.system_time        = smbcli_pull_nttime(req->in.body, 0x28);
	io->out.server_start_time  = smbcli_pull_nttime(req->in.body, 0x30);
	io->out.reserved2          = IVAL(req->in.body, 0x3C);

	status = smb2_pull_o16s16_blob(&req->in, mem_ctx, req->in.body+0x38, &io->out.secblob);
	if (!NT_STATUS_IS_OK(status)) {
		smb2_request_destroy(req);
		return status;
	}

	return smb2_request_destroy(req);
}

/*
  sync negprot request
*/
NTSTATUS smb2_negprot(struct smb2_transport *transport, 
		      TALLOC_CTX *mem_ctx, struct smb2_negprot *io)
{
	struct smb2_request *req = smb2_negprot_send(transport, io);
	return smb2_negprot_recv(req, mem_ctx, io);
}
