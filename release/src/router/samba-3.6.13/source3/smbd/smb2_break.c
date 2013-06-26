/*
   Unix SMB/CIFS implementation.
   Core SMB2 server

   Copyright (C) Stefan Metzmacher 2009
   Copyright (C) Jeremy Allison 2010

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
#include "../lib/util/tevent_ntstatus.h"

static struct tevent_req *smbd_smb2_oplock_break_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct smbd_smb2_request *smb2req,
						      struct files_struct *in_fsp,
						      uint8_t in_oplock_level);
static NTSTATUS smbd_smb2_oplock_break_recv(struct tevent_req *req,
					    uint8_t *out_oplock_level);

static void smbd_smb2_request_oplock_break_done(struct tevent_req *subreq);
NTSTATUS smbd_smb2_request_process_break(struct smbd_smb2_request *req)
{
	NTSTATUS status;
	const uint8_t *inbody;
	int i = req->current_idx;
	uint8_t in_oplock_level;
	uint64_t in_file_id_persistent;
	uint64_t in_file_id_volatile;
	struct files_struct *in_fsp;
	struct tevent_req *subreq;

	status = smbd_smb2_request_verify_sizes(req, 0x18);
	if (!NT_STATUS_IS_OK(status)) {
		return smbd_smb2_request_error(req, status);
	}
	inbody = (const uint8_t *)req->in.vector[i+1].iov_base;

	in_oplock_level		= CVAL(inbody, 0x02);

	if (in_oplock_level != SMB2_OPLOCK_LEVEL_NONE &&
			in_oplock_level != SMB2_OPLOCK_LEVEL_II) {
		return smbd_smb2_request_error(req, NT_STATUS_INVALID_PARAMETER);
	}

	/* 0x03 1 bytes reserved */
	/* 0x04 4 bytes reserved */
	in_file_id_persistent		= BVAL(inbody, 0x08);
	in_file_id_volatile		= BVAL(inbody, 0x10);

	in_fsp = file_fsp_smb2(req, in_file_id_persistent, in_file_id_volatile);
	if (in_fsp == NULL) {
		return smbd_smb2_request_error(req, NT_STATUS_FILE_CLOSED);
	}

	subreq = smbd_smb2_oplock_break_send(req, req->sconn->smb2.event_ctx,
					     req, in_fsp, in_oplock_level);
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
	      out_oplock_level);		/* SMB2 oplock level */
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
	uint8_t out_oplock_level; /* SMB2 oplock level. */
};

static struct tevent_req *smbd_smb2_oplock_break_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct smbd_smb2_request *smb2req,
						      struct files_struct *fsp,
						      uint8_t in_oplock_level)
{
	struct tevent_req *req;
	struct smbd_smb2_oplock_break_state *state;
	struct smb_request *smbreq;
	int oplocklevel = map_smb2_oplock_levels_to_samba(in_oplock_level);
	bool break_to_none = (oplocklevel == NO_OPLOCK);
	bool result;

	req = tevent_req_create(mem_ctx, &state,
				struct smbd_smb2_oplock_break_state);
	if (req == NULL) {
		return NULL;
	}
	state->smb2req = smb2req;
	state->out_oplock_level = SMB2_OPLOCK_LEVEL_NONE;

	DEBUG(10,("smbd_smb2_oplock_break_send: %s - fnum[%d] "
		  "samba level %d\n",
		  fsp_str_dbg(fsp), fsp->fnum,
		  oplocklevel));

	smbreq = smbd_smb2_fake_smb_request(smb2req);
	if (tevent_req_nomem(smbreq, req)) {
		return tevent_req_post(req, ev);
	}

	DEBUG(5,("smbd_smb2_oplock_break_send: got SMB2 oplock break (%u) from client "
		"for file %s fnum = %d\n",
		(unsigned int)in_oplock_level,
		fsp_str_dbg(fsp),
		fsp->fnum ));

	/* Are we awaiting a break message ? */
	if (fsp->oplock_timeout == NULL) {
		tevent_req_nterror(req, NT_STATUS_INVALID_OPLOCK_PROTOCOL);
		return tevent_req_post(req, ev);
	}

	if ((fsp->sent_oplock_break == BREAK_TO_NONE_SENT) ||
			(break_to_none)) {
		result = remove_oplock(fsp);
		state->out_oplock_level = SMB2_OPLOCK_LEVEL_NONE;
	} else {
		result = downgrade_oplock(fsp);
		state->out_oplock_level = SMB2_OPLOCK_LEVEL_II;
	}

	if (!result) {
		DEBUG(0, ("smbd_smb2_oplock_break_send: error in removing "
			"oplock on file %s\n", fsp_str_dbg(fsp)));
		/* Hmmm. Is this panic justified? */
		smb_panic("internal tdb error");
	}

	reply_to_oplock_break_requests(fsp);

	tevent_req_done(req);
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

/*********************************************************
 Create and send an asynchronous
 SMB2 OPLOCK_BREAK_NOTIFICATION.
*********************************************************/

void send_break_message_smb2(files_struct *fsp, int level)
{
	uint8_t smb2_oplock_level = (level == OPLOCKLEVEL_II) ?
				SMB2_OPLOCK_LEVEL_II :
				SMB2_OPLOCK_LEVEL_NONE;
	NTSTATUS status;
	uint64_t fsp_persistent = fsp_persistent_id(fsp);

	DEBUG(10,("send_break_message_smb2: sending oplock break "
		"for file %s, fnum = %d, smb2 level %u\n",
		fsp_str_dbg(fsp),
		fsp->fnum,
		(unsigned int)smb2_oplock_level ));

	status = smbd_smb2_send_oplock_break(fsp->conn->sconn,
					fsp_persistent,
					(uint64_t)fsp->fnum,
					smb2_oplock_level);
	if (!NT_STATUS_IS_OK(status)) {
		smbd_server_connection_terminate(fsp->conn->sconn,
				 nt_errstr(status));
	}
}
