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

struct smbd_smb2_notify_state {
	struct smbd_smb2_request *smb2req;
	struct smb_request *smbreq;
	struct tevent_immediate *im;
	NTSTATUS status;
	DATA_BLOB out_output_buffer;
};

static struct tevent_req *smbd_smb2_notify_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct smbd_smb2_request *smb2req,
						struct files_struct *in_fsp,
						uint16_t in_flags,
						uint32_t in_output_buffer_length,
						uint64_t in_completion_filter);
static NTSTATUS smbd_smb2_notify_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      DATA_BLOB *out_output_buffer);

static void smbd_smb2_request_notify_done(struct tevent_req *subreq);
NTSTATUS smbd_smb2_request_process_notify(struct smbd_smb2_request *req)
{
	NTSTATUS status;
	const uint8_t *inbody;
	int i = req->current_idx;
	uint16_t in_flags;
	uint32_t in_output_buffer_length;
	uint64_t in_file_id_persistent;
	uint64_t in_file_id_volatile;
	struct files_struct *in_fsp;
	uint64_t in_completion_filter;
	struct tevent_req *subreq;

	status = smbd_smb2_request_verify_sizes(req, 0x20);
	if (!NT_STATUS_IS_OK(status)) {
		return smbd_smb2_request_error(req, status);
	}
	inbody = (const uint8_t *)req->in.vector[i+1].iov_base;

	in_flags		= SVAL(inbody, 0x02);
	in_output_buffer_length	= IVAL(inbody, 0x04);
	in_file_id_persistent	= BVAL(inbody, 0x08);
	in_file_id_volatile	= BVAL(inbody, 0x10);
	in_completion_filter	= IVAL(inbody, 0x18);

	/*
	 * 0x00010000 is what Windows 7 uses,
	 * Windows 2008 uses 0x00080000
	 */
	if (in_output_buffer_length > req->sconn->smb2.max_trans) {
		return smbd_smb2_request_error(req, NT_STATUS_INVALID_PARAMETER);
	}

	in_fsp = file_fsp_smb2(req, in_file_id_persistent, in_file_id_volatile);
	if (in_fsp == NULL) {
		return smbd_smb2_request_error(req, NT_STATUS_FILE_CLOSED);
	}

	subreq = smbd_smb2_notify_send(req, req->sconn->smb2.event_ctx,
				       req, in_fsp,
				       in_flags,
				       in_output_buffer_length,
				       in_completion_filter);
	if (subreq == NULL) {
		return smbd_smb2_request_error(req, NT_STATUS_NO_MEMORY);
	}
	tevent_req_set_callback(subreq, smbd_smb2_request_notify_done, req);

	return smbd_smb2_request_pending_queue(req, subreq);
}

static void smbd_smb2_request_notify_done(struct tevent_req *subreq)
{
	struct smbd_smb2_request *req = tevent_req_callback_data(subreq,
					struct smbd_smb2_request);
	int i = req->current_idx;
	uint8_t *outhdr;
	DATA_BLOB outbody;
	DATA_BLOB outdyn;
	uint16_t out_output_buffer_offset;
	DATA_BLOB out_output_buffer = data_blob_null;
	NTSTATUS status;
	NTSTATUS error; /* transport error */

	if (req->cancelled) {
		struct smbd_smb2_notify_state *state = tevent_req_data(subreq,
					       struct smbd_smb2_notify_state);
		const uint8_t *inhdr = (const uint8_t *)req->in.vector[i].iov_base;
		uint64_t mid = BVAL(inhdr, SMB2_HDR_MESSAGE_ID);

		DEBUG(10,("smbd_smb2_request_notify_done: cancelled mid %llu\n",
			(unsigned long long)mid ));
		error = smbd_smb2_request_error(req, NT_STATUS_CANCELLED);
		if (!NT_STATUS_IS_OK(error)) {
			smbd_server_connection_terminate(req->sconn,
				nt_errstr(error));
			return;
		}
		TALLOC_FREE(state->im);
		return;
	}

	status = smbd_smb2_notify_recv(subreq,
				       req,
				       &out_output_buffer);
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

	out_output_buffer_offset = SMB2_HDR_BODY + 0x08;

	outhdr = (uint8_t *)req->out.vector[i].iov_base;

	outbody = data_blob_talloc(req->out.vector, NULL, 0x08);
	if (outbody.data == NULL) {
		error = smbd_smb2_request_error(req, NT_STATUS_NO_MEMORY);
		if (!NT_STATUS_IS_OK(error)) {
			smbd_server_connection_terminate(req->sconn,
							 nt_errstr(error));
			return;
		}
		return;
	}

	SSVAL(outbody.data, 0x00, 0x08 + 1);	/* struct size */
	SSVAL(outbody.data, 0x02,
	      out_output_buffer_offset);	/* output buffer offset */
	SIVAL(outbody.data, 0x04,
	      out_output_buffer.length);	/* output buffer length */

	outdyn = out_output_buffer;

	error = smbd_smb2_request_done(req, outbody, &outdyn);
	if (!NT_STATUS_IS_OK(error)) {
		smbd_server_connection_terminate(req->sconn,
						 nt_errstr(error));
		return;
	}
}

static void smbd_smb2_notify_reply(struct smb_request *smbreq,
				   NTSTATUS error_code,
				   uint8_t *buf, size_t len);
static void smbd_smb2_notify_reply_trigger(struct tevent_context *ctx,
					   struct tevent_immediate *im,
					   void *private_data);
static bool smbd_smb2_notify_cancel(struct tevent_req *req);

static struct tevent_req *smbd_smb2_notify_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct smbd_smb2_request *smb2req,
						struct files_struct *fsp,
						uint16_t in_flags,
						uint32_t in_output_buffer_length,
						uint64_t in_completion_filter)
{
	struct tevent_req *req;
	struct smbd_smb2_notify_state *state;
	struct smb_request *smbreq;
	connection_struct *conn = smb2req->tcon->compat_conn;
	bool recursive = (in_flags & 0x0001) ? true : false;
	NTSTATUS status;

	req = tevent_req_create(mem_ctx, &state,
				struct smbd_smb2_notify_state);
	if (req == NULL) {
		return NULL;
	}
	state->smb2req = smb2req;
	state->status = NT_STATUS_INTERNAL_ERROR;
	state->out_output_buffer = data_blob_null;
	state->im = NULL;

	DEBUG(10,("smbd_smb2_notify_send: %s - fnum[%d]\n",
		  fsp_str_dbg(fsp), fsp->fnum));

	smbreq = smbd_smb2_fake_smb_request(smb2req);
	if (tevent_req_nomem(smbreq, req)) {
		return tevent_req_post(req, ev);
	}

	state->smbreq = smbreq;
	smbreq->async_priv = (void *)req;

	{
		char *filter_string;

		filter_string = notify_filter_string(NULL, in_completion_filter);
		if (tevent_req_nomem(filter_string, req)) {
			return tevent_req_post(req, ev);
		}

		DEBUG(3,("smbd_smb2_notify_send: notify change "
			 "called on %s, filter = %s, recursive = %d\n",
			 fsp_str_dbg(fsp), filter_string, recursive));

		TALLOC_FREE(filter_string);
	}

	if ((!fsp->is_directory) || (conn != fsp->conn)) {
		tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return tevent_req_post(req, ev);
	}

	if (fsp->notify == NULL) {

		status = change_notify_create(fsp,
					      in_completion_filter,
					      recursive);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(10, ("change_notify_create returned %s\n",
				   nt_errstr(status)));
			tevent_req_nterror(req, status);
			return tevent_req_post(req, ev);
		}
	}

	if (fsp->notify->num_changes != 0) {

		/*
		 * We've got changes pending, respond immediately
		 */

		/*
		 * TODO: write a torture test to check the filtering behaviour
		 * here.
		 */

		change_notify_reply(smbreq,
				    NT_STATUS_OK,
				    in_output_buffer_length,
				    fsp->notify,
				    smbd_smb2_notify_reply);

		/*
		 * change_notify_reply() above has independently
		 * called tevent_req_done().
		 */
		return tevent_req_post(req, ev);
	}

	state->im = tevent_create_immediate(state);
	if (tevent_req_nomem(state->im, req)) {
		return tevent_req_post(req, ev);
	}

	/*
	 * No changes pending, queue the request
	 */

	status = change_notify_add_request(smbreq,
			in_output_buffer_length,
			in_completion_filter,
			recursive, fsp,
			smbd_smb2_notify_reply);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return tevent_req_post(req, ev);
	}

	/* allow this request to be canceled */
	tevent_req_set_cancel_fn(req, smbd_smb2_notify_cancel);

	return req;
}

static void smbd_smb2_notify_reply(struct smb_request *smbreq,
				   NTSTATUS error_code,
				   uint8_t *buf, size_t len)
{
	struct tevent_req *req = talloc_get_type_abort(smbreq->async_priv,
						       struct tevent_req);
	struct smbd_smb2_notify_state *state = tevent_req_data(req,
					       struct smbd_smb2_notify_state);

	state->status = error_code;
	if (!NT_STATUS_IS_OK(error_code)) {
		/* nothing */
	} else if (len == 0) {
		state->status = STATUS_NOTIFY_ENUM_DIR;
	} else {
		state->out_output_buffer = data_blob_talloc(state, buf, len);
		if (state->out_output_buffer.data == NULL) {
			state->status = NT_STATUS_NO_MEMORY;
		}
	}

	if (state->im == NULL) {
		smbd_smb2_notify_reply_trigger(NULL, NULL, req);
		return;
	}

	/*
	 * if this is called async, we need to go via an immediate event
	 * because the caller replies on the smb_request (a child of req
	 * being arround after calling this function
	 */
	tevent_schedule_immediate(state->im,
				  state->smb2req->sconn->smb2.event_ctx,
				  smbd_smb2_notify_reply_trigger,
				  req);
}

static void smbd_smb2_notify_reply_trigger(struct tevent_context *ctx,
					   struct tevent_immediate *im,
					   void *private_data)
{
	struct tevent_req *req = talloc_get_type_abort(private_data,
						       struct tevent_req);
	struct smbd_smb2_notify_state *state = tevent_req_data(req,
					       struct smbd_smb2_notify_state);

	if (!NT_STATUS_IS_OK(state->status)) {
		tevent_req_nterror(req, state->status);
		return;
	}

	tevent_req_done(req);
}

static bool smbd_smb2_notify_cancel(struct tevent_req *req)
{
	struct smbd_smb2_notify_state *state = tevent_req_data(req,
					       struct smbd_smb2_notify_state);

	state->smb2req->cancelled = true;
	smbd_notify_cancel_by_smbreq(state->smbreq);

	return true;
}

static NTSTATUS smbd_smb2_notify_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      DATA_BLOB *out_output_buffer)
{
	NTSTATUS status;
	struct smbd_smb2_notify_state *state = tevent_req_data(req,
					       struct smbd_smb2_notify_state);

	if (tevent_req_is_nterror(req, &status)) {
		tevent_req_received(req);
		return status;
	}

	*out_output_buffer = state->out_output_buffer;
	talloc_steal(mem_ctx, out_output_buffer->data);

	tevent_req_received(req);
	return NT_STATUS_OK;
}
