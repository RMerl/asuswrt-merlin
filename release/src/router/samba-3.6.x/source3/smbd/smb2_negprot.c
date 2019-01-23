/*
   Unix SMB/CIFS implementation.
   Core SMB2 server

   Copyright (C) Stefan Metzmacher 2009

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
#include "smbd/smbd.h"
#include "smbd/globals.h"
#include "../libcli/smb/smb_common.h"

/*
 * this is the entry point if SMB2 is selected via
 * the SMB negprot
 */
void reply_smb2002(struct smb_request *req, uint16_t choice)
{
	uint8_t *smb2_inbuf;
	uint8_t *smb2_hdr;
	uint8_t *smb2_body;
	uint8_t *smb2_dyn;
	size_t len = 4 + SMB2_HDR_BODY + 0x24 + 2;

	smb2_inbuf = talloc_zero_array(talloc_tos(), uint8_t, len);
	if (smb2_inbuf == NULL) {
		DEBUG(0, ("Could not push spnego blob\n"));
		reply_nterror(req, NT_STATUS_NO_MEMORY);
		return;
	}
	smb2_hdr = smb2_inbuf + 4;
	smb2_body = smb2_hdr + SMB2_HDR_BODY;
	smb2_dyn = smb2_body + 0x24;

	SIVAL(smb2_hdr, SMB2_HDR_PROTOCOL_ID,	SMB2_MAGIC);
	SIVAL(smb2_hdr, SMB2_HDR_LENGTH,	SMB2_HDR_BODY);

	SSVAL(smb2_body, 0x00, 0x0024);	/* struct size */
	SSVAL(smb2_body, 0x02, 0x0001);	/* dialect count */

	SSVAL(smb2_dyn,  0x00, 0x0202);	/* dialect 2.002 */

	req->outbuf = NULL;

	smbd_smb2_first_negprot(req->sconn, smb2_inbuf, len);
	return;
}

NTSTATUS smbd_smb2_request_process_negprot(struct smbd_smb2_request *req)
{
	NTSTATUS status;
	const uint8_t *inbody;
	const uint8_t *indyn = NULL;
	int i = req->current_idx;
	DATA_BLOB outbody;
	DATA_BLOB outdyn;
	DATA_BLOB negprot_spnego_blob;
	uint16_t security_offset;
	DATA_BLOB security_buffer;
	size_t expected_dyn_size = 0;
	size_t c;
	uint16_t security_mode;
	uint16_t dialect_count;
	uint16_t dialect = 0;
	uint32_t capabilities;
	uint32_t max_limit;
	uint32_t max_trans = lp_smb2_max_trans();
	uint32_t max_read = lp_smb2_max_read();
	uint32_t max_write = lp_smb2_max_write();

	status = smbd_smb2_request_verify_sizes(req, 0x24);
	if (!NT_STATUS_IS_OK(status)) {
		return smbd_smb2_request_error(req, status);
	}
	inbody = (const uint8_t *)req->in.vector[i+1].iov_base;

	dialect_count = SVAL(inbody, 0x02);
	if (dialect_count == 0) {
		return smbd_smb2_request_error(req, NT_STATUS_INVALID_PARAMETER);
	}

	expected_dyn_size = dialect_count * 2;
	if (req->in.vector[i+2].iov_len < expected_dyn_size) {
		return smbd_smb2_request_error(req, NT_STATUS_INVALID_PARAMETER);
	}
	indyn = (const uint8_t *)req->in.vector[i+2].iov_base;

	for (c=0; c < dialect_count; c++) {
		dialect = SVAL(indyn, c*2);
		if (dialect == SMB2_DIALECT_REVISION_202) {
			break;
		}
	}

	if (dialect != SMB2_DIALECT_REVISION_202) {
		return smbd_smb2_request_error(req, NT_STATUS_INVALID_PARAMETER);
	}

	set_Protocol(PROTOCOL_SMB2);

	if (get_remote_arch() != RA_SAMBA) {
		set_remote_arch(RA_VISTA);
	}

	/* negprot_spnego() returns a the server guid in the first 16 bytes */
	negprot_spnego_blob = negprot_spnego(req, req->sconn);
	if (negprot_spnego_blob.data == NULL) {
		return smbd_smb2_request_error(req, NT_STATUS_NO_MEMORY);
	}

	if (negprot_spnego_blob.length < 16) {
		return smbd_smb2_request_error(req, NT_STATUS_INTERNAL_ERROR);
	}

	security_mode = SMB2_NEGOTIATE_SIGNING_ENABLED;
	if (lp_server_signing() == Required) {
		security_mode |= SMB2_NEGOTIATE_SIGNING_REQUIRED;
	}

	capabilities = 0;
	if (lp_host_msdfs()) {
		capabilities |= SMB2_CAP_DFS;
	}

	/*
	 * Unless we implement SMB2_CAP_LARGE_MTU,
	 * 0x10000 (65536) is the maximum allowed message size
	 */
	max_limit = 0x10000;

	max_trans = MIN(max_limit, max_trans);
	max_read  = MIN(max_limit, max_read);
	max_write = MIN(max_limit, max_write);

	security_offset = SMB2_HDR_BODY + 0x40;

#if 1
	/* Try SPNEGO auth... */
	security_buffer = data_blob_const(negprot_spnego_blob.data + 16,
					  negprot_spnego_blob.length - 16);
#else
	/* for now we want raw NTLMSSP */
	security_buffer = data_blob_const(NULL, 0);
#endif

	outbody = data_blob_talloc(req->out.vector, NULL, 0x40);
	if (outbody.data == NULL) {
		return smbd_smb2_request_error(req, NT_STATUS_NO_MEMORY);
	}

	SSVAL(outbody.data, 0x00, 0x40 + 1);	/* struct size */
	SSVAL(outbody.data, 0x02,
	      security_mode);			/* security mode */
	SSVAL(outbody.data, 0x04, dialect);	/* dialect revision */
	SSVAL(outbody.data, 0x06, 0);		/* reserved */
	memcpy(outbody.data + 0x08,
	       negprot_spnego_blob.data, 16);	/* server guid */
	SIVAL(outbody.data, 0x18,
	      capabilities);			/* capabilities */
	SIVAL(outbody.data, 0x1C, max_trans);	/* max transact size */
	SIVAL(outbody.data, 0x20, max_trans);	/* max read size */
	SIVAL(outbody.data, 0x24, max_trans);	/* max write size */
	SBVAL(outbody.data, 0x28, 0);		/* system time */
	SBVAL(outbody.data, 0x30, 0);		/* server start time */
	SSVAL(outbody.data, 0x38,
	      security_offset);			/* security buffer offset */
	SSVAL(outbody.data, 0x3A,
	      security_buffer.length);		/* security buffer length */
	SIVAL(outbody.data, 0x3C, 0);		/* reserved */

	outdyn = security_buffer;

	req->sconn->using_smb2 = true;
	req->sconn->smb2.max_trans = max_trans;
	req->sconn->smb2.max_read  = max_read;
	req->sconn->smb2.max_write = max_write;

	return smbd_smb2_request_done(req, outbody, &outdyn);
}
