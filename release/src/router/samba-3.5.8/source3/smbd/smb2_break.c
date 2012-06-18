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

static struct tevent_req *smbd_smb2_oplock_break_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct smbd_smb2_request *smb2req,
						      uint8_t in_oplock_level,
						      uint64_t in_file_id_volatile);
static NTSTATUS smbd_smb2_oplock_break_recv(struct tevent_req *req,
					    uint8_t *out_oplock_level);

static void smbd_smb2_request_oplock_break_done(struct tevent_req *subreq);
NTSTATUS smbd_smb2_request_process_break(struct smbd_smb2_request *req)
{
	const uint8_t *inhdr;
	const uint8_t *inbody;
	int i = req->current_idx;
	size_t expected_body_size = 0x18;
	size_t body_size;
	uint8_t in_oplock_level;
	uint64_t in_file_id_persistent;
	uint64_t in_file_id_volatile;
	struct tevent_req *subreq;

	inhdr = (const uint8_t *)req->in.vector[i+0].iov_base;
	if (req->in.vector[i+1].iov_len != (expected_body_size & 0xFFFFFFFE)) {
		return smbd_smb2_request_error(req, NT_STATUS_INVALID_PARAMETER);
	}

	inbody = (const uint8_t *)req->in.vector[i+1].iov_base;

	body_size = SVAL(inbody, 0x00);
	if (body_size != expected_body_size) {
		return smbd_smb2_request_error(req, NT_STATUS_INVALID_PARAMETER);
	}

	in_oplock_level		= CVAL(inbody, 0x02);
	/* 0x03 1 bytes reserved */
	/* 0x04 4 bytes reserved */
	in_file_id_persistent		= BVAL(inbody, 0x08);
	in_file_id_volatile		= BVAL(inbody, 0x10);

	if (req->compat_chain_fsp) {
		/* skip check */
	} else if (in_file_id_persistent != 0) {
		return smbd_smb2_request_error(req, NT_STATUS_FILE_CLOSED);
	}

	subreq = smbd_smb2_oplock_break_send(req,
					     req->sconn->smb2.event_ctx,
					     req,
					     in_oplock_level,
					     in_file_id_volatile);
	if (subreq == NULL) {
		return smbd_smb2_request_error(req, NT_STATUS_NO_MEMORY);
	}
	tevent_req_set_callback(subreq, smbd_smb2_request_oplock_break_done, req);

	return smbd_smb2_request_pending_queue(req, subreq);
}

static void smbd_smb2_request_oplock_break_done(struct tevent_req *subreq)
{
	struct smbd_smb2_request *req = tevent_req_callback_data(subreq,
					struct smbd_smb2_request);
	const uint8_t *inbody;
	int i = req->current_idx;
	uint64_t in_file_id_persistent;
	uint64_t in_file_id_volatile;
	uint8_t out_oplock_level = 0;
	DATA_BLOB outbody;
	NTSTATUS status;
	NTSTATUS error; /* transport error */

	status = smbd_smb2_oplock_break_recv(subreq, &out_oplock_level);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		error = smbd_smb2_request_error(req, status);
		if (!NT_STATUS_IS_OK(error)) {
			smbd_server_connection_terminate(req->sconn,
							 nt_errstr(error));
			return;
		}
		return;
	}

	inbody = (const uint8_t *)req->in.vector[i+1].iov_base;

	in_file_id_persistent	= BVAL(inbody, 0x08);
	in_file_id_volatile	= BVAL(inbody, 0x10);

	outbody = data_blob_talloc(req->out.vector, NULL, 0x18);
	if (outbody.data == NULL) {
		error = smbd_smb2_request_error(req, NT_STATUS_NO_MEMORY);
		if (!NT_STATUS_IS_OK(error)) {
			smbd_server_connection_terminate(req->sconn,
							 nt_errstr(error));
			return;
		}
		return;
	}

	SSVAL(outbody.data, 0x00, 0x18);	/* struct size */
	SCVAL(outbody.data, 0x02,
	      out_oplock_level);		/* oplock level */
	SCVAL(outbody.data, 0x03, 0);		/* reserved */
	SIVAL(outbody.data, 0x04, 0);		/* reserved */
	SBVAL(outbody.data, 0x08,
	      in_file_id_persistent);		/* file id (persistent) */
	SBVAL(outbody.data, 0x10,
	      in_file_id_volatile);		/* file id (volatile) */

	error = smbd_smb2_request_done(req, outbody, NULL);
	if (!NT_STATUS_IS_OK(error)) {
		smbd_server_connection_terminate(req->sconn,
						 nt_errstr(error));
		return;
	}
}

struct smbd_smb2_oplock_break_state {
	struct smbd_smb2_request *smb2req;
	uint8_t out_oplock_level;
};

static struct tevent_req *smbd_smb2_oplock_break_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct smbd_smb2_request *smb2req,
						      uint8_t in_oplock_level,
						      uint64_t in_file_id_volatile)
{
	struct tevent_req *req;
	struct smbd_smb2_oplock_break_state *state;
	struct smb_request *smbreq;
	connection_struct *conn = smb2req->tcon->compat_conn;
	files_struct *fsp;

	req = tevent_req_create(mem_ctx, &state,
				struct smbd_smb2_oplock_break_state);
	if (req == NULL) {
		return NULL;
	}
	state->smb2req = smb2req;
	state->out_oplock_level = SMB2_OPLOCK_LEVEL_NONE;

	DEBUG(10,("smbd_smb2_oplock_break_send: file_id[0x%016llX]\n",
		  (unsigned long long)in_file_id_volatile));

	smbreq = smbd_smb2_fake_smb_request(smb2req);
	if (tevent_req_nomem(smbreq, req)) {
		return tevent_req_post(req, ev);
	}

	fsp = file_fsp(smbreq, (uint16_t)in_file_id_volatile);
	if (fsp == NULL) {
		tevent_req_nterror(req, NT_STATUS_FILE_CLOSED);
		return tevent_req_post(req, ev);
	}
	if (conn != fsp->conn) {
		tevent_req_nterror(req, NT_STATUS_FILE_CLOSED);
		return tevent_req_post(req, ev);
	}
	if (smb2req->session->vuid != fsp->vuid) {
		tevent_req_nterror(req, NT_STATUS_FILE_CLOSED);
		return tevent_req_post(req, ev);
	}

	tevent_req_nterror(req, NT_STATUS_NOT_IMPLEMENTED);
	return tevent_req_post(req, ev);
}

static NTSTATUS smbd_smb2_oplock_break_recv(struct tevent_req *req,
					    uint8_t *out_oplock_level)
{
	NTSTATUS status;
	struct smbd_smb2_oplock_break_state *state =
		tevent_req_data(req,
		struct smbd_smb2_oplock_break_state);

	if (tevent_req_is_nterror(req, &status)) {
		tevent_req_received(req);
		return status;
	}

	*out_oplock_level = state->out_oplock_level;

	tevent_req_received(req);
	return NT_STATUS_OK;
}
