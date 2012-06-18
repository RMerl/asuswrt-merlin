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
#include "smbd/globals.h"
#include "../libcli/smb/smb_common.h"

static NTSTATUS smbd_smb2_close(struct smbd_smb2_request *req,
				uint16_t in_flags,
				uint64_t in_file_id_volatile);

NTSTATUS smbd_smb2_request_process_close(struct smbd_smb2_request *req)
{
	const uint8_t *inhdr;
	const uint8_t *inbody;
	int i = req->current_idx;
	uint8_t *outhdr;
	DATA_BLOB outbody;
	size_t expected_body_size = 0x18;
	size_t body_size;
	uint16_t in_flags;
	uint64_t in_file_id_persistent;
	uint64_t in_file_id_volatile;
	NTSTATUS status;

	inhdr = (const uint8_t *)req->in.vector[i+0].iov_base;
	if (req->in.vector[i+1].iov_len != (expected_body_size & 0xFFFFFFFE)) {
		return smbd_smb2_request_error(req, NT_STATUS_INVALID_PARAMETER);
	}

	inbody = (const uint8_t *)req->in.vector[i+1].iov_base;

	body_size = SVAL(inbody, 0x00);
	if (body_size != expected_body_size) {
		return smbd_smb2_request_error(req, NT_STATUS_INVALID_PARAMETER);
	}

	in_flags		= SVAL(inbody, 0x02);
	in_file_id_persistent	= BVAL(inbody, 0x08);
	in_file_id_volatile	= BVAL(inbody, 0x10);

	if (req->compat_chain_fsp) {
		/* skip check */
	} else if (in_file_id_persistent != 0) {
		return smbd_smb2_request_error(req, NT_STATUS_FILE_CLOSED);
	}

	status = smbd_smb2_close(req,
				in_flags,
				in_file_id_volatile);
	if (!NT_STATUS_IS_OK(status)) {
		return smbd_smb2_request_error(req, status);
	}

	outhdr = (uint8_t *)req->out.vector[i].iov_base;

	outbody = data_blob_talloc(req->out.vector, NULL, 0x3C);
	if (outbody.data == NULL) {
		return smbd_smb2_request_error(req, NT_STATUS_NO_MEMORY);
	}

	SSVAL(outbody.data, 0x00, 0x3C);	/* struct size */
	SSVAL(outbody.data, 0x02, 0);		/* flags */
	SIVAL(outbody.data, 0x04, 0);		/* reserved */
	SBVAL(outbody.data, 0x08, 0);		/* creation time */
	SBVAL(outbody.data, 0x10, 0);		/* last access time */
	SBVAL(outbody.data, 0x18, 0);		/* last write time */
	SBVAL(outbody.data, 0x20, 0);		/* change time */
	SBVAL(outbody.data, 0x28, 0);		/* allocation size */
	SBVAL(outbody.data, 0x30, 0);		/* end of size */
	SIVAL(outbody.data, 0x38, 0);		/* file attributes */

	return smbd_smb2_request_done(req, outbody, NULL);
}

static NTSTATUS smbd_smb2_close(struct smbd_smb2_request *req,
				uint16_t in_flags,
				uint64_t in_file_id_volatile)
{
	NTSTATUS status;
	struct smb_request *smbreq;
	connection_struct *conn = req->tcon->compat_conn;
	files_struct *fsp;

	DEBUG(10,("smbd_smb2_close: file_id[0x%016llX]\n",
		  (unsigned long long)in_file_id_volatile));

	smbreq = smbd_smb2_fake_smb_request(req);
	if (smbreq == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	fsp = file_fsp(smbreq, (uint16_t)in_file_id_volatile);
	if (fsp == NULL) {
		return NT_STATUS_FILE_CLOSED;
	}
	if (conn != fsp->conn) {
		return NT_STATUS_FILE_CLOSED;
	}
	if (req->session->vuid != fsp->vuid) {
		return NT_STATUS_FILE_CLOSED;
	}

	status = close_file(smbreq, fsp, NORMAL_CLOSE);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(5,("smbd_smb2_close: close_file[%s]: %s\n",
			 fsp_str_dbg(fsp), nt_errstr(status)));
		return status;
	}

	return NT_STATUS_OK;
}
