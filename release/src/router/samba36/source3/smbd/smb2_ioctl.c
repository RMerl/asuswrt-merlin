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
#include "../lib/util/tevent_ntstatus.h"
#include "rpc_server/srv_pipe_hnd.h"
#include "include/ntioctl.h"

static struct tevent_req *smbd_smb2_ioctl_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct smbd_smb2_request *smb2req,
					       struct files_struct *in_fsp,
					       uint32_t in_ctl_code,
					       DATA_BLOB in_input,
					       uint32_t in_max_output,
					       uint32_t in_flags);
static NTSTATUS smbd_smb2_ioctl_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     DATA_BLOB *out_output);

static void smbd_smb2_request_ioctl_done(struct tevent_req *subreq);
NTSTATUS smbd_smb2_request_process_ioctl(struct smbd_smb2_request *req)
{
	NTSTATUS status;
	const uint8_t *inbody;
	int i = req->current_idx;
	uint32_t in_ctl_code;
	uint64_t in_file_id_persistent;
	uint64_t in_file_id_volatile;
	struct files_struct *in_fsp = NULL;
	uint32_t in_input_offset;
	uint32_t in_input_length;
	DATA_BLOB in_input_buffer;
	uint32_t in_max_output_length;
	uint32_t in_flags;
	struct tevent_req *subreq;

	status = smbd_smb2_request_verify_sizes(req, 0x39);
	if (!NT_STATUS_IS_OK(status)) {
		return smbd_smb2_request_error(req, status);
	}
	inbody = (const uint8_t *)req->in.vector[i+1].iov_base;

	in_ctl_code		= IVAL(inbody, 0x04);
	in_file_id_persistent	= BVAL(inbody, 0x08);
	in_file_id_volatile	= BVAL(inbody, 0x10);
	in_input_offset		= IVAL(inbody, 0x18);
	in_input_length		= IVAL(inbody, 0x1C);
	in_max_output_length	= IVAL(inbody, 0x2C);
	in_flags		= IVAL(inbody, 0x30);

	/*
	 * InputOffset (4 bytes): The offset, in bytes, from the beginning of
	 * the SMB2 header to the input data buffer. If no input data is
	 * required for the FSCTL/IOCTL command being issued, the client SHOULD
	 * set this value to 0.<49>
	 * <49> If no input data is required for the FSCTL/IOCTL command being
	 * issued, Windows-based clients set this field to any value.
	 */
	if ((in_input_length > 0)
	 && (in_input_offset != (SMB2_HDR_BODY + req->in.vector[i+1].iov_len))) {
		return smbd_smb2_request_error(req, NT_STATUS_INVALID_PARAMETER);
	}

	if (in_input_length > req->in.vector[i+2].iov_len) {
		return smbd_smb2_request_error(req, NT_STATUS_INVALID_PARAMETER);
	}

	in_input_buffer.data = (uint8_t *)req->in.vector[i+2].iov_base;
	in_input_buffer.length = in_input_length;

	switch (in_ctl_code) {
	case FSCTL_DFS_GET_REFERRALS:
	case FSCTL_DFS_GET_REFERRALS_EX:
	case FSCTL_PIPE_WAIT:
	case FSCTL_VALIDATE_NEGOTIATE_INFO_224:
	case FSCTL_VALIDATE_NEGOTIATE_INFO:
	case FSCTL_QUERY_NETWORK_INTERFACE_INFO:
		/*
		 * Some SMB2 specific CtlCodes like FSCTL_DFS_GET_REFERRALS or
		 * FSCTL_PIPE_WAIT does not take a file handle.
		 *
		 * If FileId in the SMB2 Header of the request is not
		 * 0xFFFFFFFFFFFFFFFF, then the server MUST fail the request
		 * with STATUS_INVALID_PARAMETER.
		 */
		if (in_file_id_persistent != UINT64_MAX ||
		    in_file_id_volatile != UINT64_MAX) {
			return smbd_smb2_request_error(req,
				NT_STATUS_INVALID_PARAMETER);
		}
		break;
	default:
		in_fsp = file_fsp_smb2(req, in_file_id_persistent,
					in_file_id_volatile);
		if (in_fsp == NULL) {
			return smbd_smb2_request_error(req, NT_STATUS_FILE_CLOSED);
		}
		break;
	}

	subreq = smbd_smb2_ioctl_send(req,
				      req->sconn->smb2.event_ctx,
				      req, in_fsp,
				      in_ctl_code,
				      in_input_buffer,
				      in_max_output_length,
				      in_flags);
	if (subreq == NULL) {
		return smbd_smb2_request_error(req, NT_STATUS_NO_MEMORY);
	}
	tevent_req_set_callback(subreq, smbd_smb2_request_ioctl_done, req);

	return smbd_smb2_request_pending_queue(req, subreq);
}

static void smbd_smb2_request_ioctl_done(struct tevent_req *subreq)
{
	struct smbd_smb2_request *req = tevent_req_callback_data(subreq,
					struct smbd_smb2_request);
	const uint8_t *inbody;
	int i = req->current_idx;
	uint8_t *outhdr;
	DATA_BLOB outbody;
	DATA_BLOB outdyn;
	uint32_t in_ctl_code;
	uint64_t in_file_id_persistent;
	uint64_t in_file_id_volatile;
	uint32_t out_input_offset;
	uint32_t out_output_offset;
	DATA_BLOB out_output_buffer = data_blob_null;
	NTSTATUS status;
	NTSTATUS error; /* transport error */

	status = smbd_smb2_ioctl_recv(subreq, req, &out_output_buffer);

	DEBUG(10,("smbd_smb2_request_ioctl_done: smbd_smb2_ioctl_recv returned "
		"%u status %s\n",
		(unsigned int)out_output_buffer.length,
		nt_errstr(status) ));

	TALLOC_FREE(subreq);
	if (NT_STATUS_EQUAL(status, STATUS_BUFFER_OVERFLOW)) {
		/* also ok */
	} else if (!NT_STATUS_IS_OK(status)) {
		error = smbd_smb2_request_error(req, status);
		if (!NT_STATUS_IS_OK(error)) {
			smbd_server_connection_terminate(req->sconn,
							 nt_errstr(error));
			return;
		}
		return;
	}

	out_input_offset = SMB2_HDR_BODY + 0x30;
	out_output_offset = SMB2_HDR_BODY + 0x30;

	inbody = (const uint8_t *)req->in.vector[i+1].iov_base;

	in_ctl_code		= IVAL(inbody, 0x04);
	in_file_id_persistent	= BVAL(inbody, 0x08);
	in_file_id_volatile	= BVAL(inbody, 0x10);

	outhdr = (uint8_t *)req->out.vector[i].iov_base;

	outbody = data_blob_talloc(req->out.vector, NULL, 0x30);
	if (outbody.data == NULL) {
		error = smbd_smb2_request_error(req, NT_STATUS_NO_MEMORY);
		if (!NT_STATUS_IS_OK(error)) {
			smbd_server_connection_terminate(req->sconn,
							 nt_errstr(error));
			return;
		}
		return;
	}

	SSVAL(outbody.data, 0x00, 0x30 + 1);	/* struct size */
	SSVAL(outbody.data, 0x02, 0);		/* reserved */
	SIVAL(outbody.data, 0x04,
	      in_ctl_code);			/* ctl code */
	SBVAL(outbody.data, 0x08,
	      in_file_id_persistent);		/* file id (persistent) */
	SBVAL(outbody.data, 0x10,
	      in_file_id_volatile);		/* file id (volatile) */
	SIVAL(outbody.data, 0x18,
	      out_input_offset);		/* input offset */
	SIVAL(outbody.data, 0x1C, 0);		/* input count */
	SIVAL(outbody.data, 0x20,
	      out_output_offset);		/* output offset */
	SIVAL(outbody.data, 0x24,
	      out_output_buffer.length);	/* output count */
	SIVAL(outbody.data, 0x28, 0);		/* flags */
	SIVAL(outbody.data, 0x2C, 0);		/* reserved */

	/*
	 * Note: Windows Vista and 2008 send back also the
	 *       input from the request. But it was fixed in
	 *       Windows 7.
	 */
	outdyn = out_output_buffer;

	error = smbd_smb2_request_done_ex(req, status, outbody, &outdyn,
					  __location__);
	if (!NT_STATUS_IS_OK(error)) {
		smbd_server_connection_terminate(req->sconn,
						 nt_errstr(error));
		return;
	}
}

struct smbd_smb2_ioctl_state {
	struct smbd_smb2_request *smb2req;
	struct smb_request *smbreq;
	files_struct *fsp;
	DATA_BLOB in_input;
	uint32_t in_max_output;
	DATA_BLOB out_output;
};

static void smbd_smb2_ioctl_pipe_write_done(struct tevent_req *subreq);
static void smbd_smb2_ioctl_pipe_read_done(struct tevent_req *subreq);

static struct tevent_req *smbd_smb2_ioctl_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct smbd_smb2_request *smb2req,
					       struct files_struct *fsp,
					       uint32_t in_ctl_code,
					       DATA_BLOB in_input,
					       uint32_t in_max_output,
					       uint32_t in_flags)
{
	struct tevent_req *req;
	struct smbd_smb2_ioctl_state *state;
	struct smb_request *smbreq;
	struct tevent_req *subreq;

	req = tevent_req_create(mem_ctx, &state,
				struct smbd_smb2_ioctl_state);
	if (req == NULL) {
		return NULL;
	}
	state->smb2req = smb2req;
	state->smbreq = NULL;
	state->fsp = fsp;
	state->in_input = in_input;
	state->in_max_output = in_max_output;
	state->out_output = data_blob_null;

	DEBUG(10, ("smbd_smb2_ioctl: ctl_code[0x%08x] %s fnum[%d]\n",
		(unsigned)in_ctl_code,
		fsp ? fsp_str_dbg(fsp) : "<no handle>",
		fsp ? fsp->fnum : -1));

	smbreq = smbd_smb2_fake_smb_request(smb2req);
	if (tevent_req_nomem(smbreq, req)) {
		return tevent_req_post(req, ev);
	}
	state->smbreq = smbreq;

	switch (in_ctl_code) {
	case 0x00060194: /* FSCTL_DFS_GET_REFERRALS */
	{
		uint16_t in_max_referral_level;
		DATA_BLOB in_file_name_buffer;
		char *in_file_name_string;
		size_t in_file_name_string_size;
		bool ok;
		bool overflow = false;
		NTSTATUS status;
		int dfs_size;
		char *dfs_data = NULL;

		if (!IS_IPC(smbreq->conn)) {
			tevent_req_nterror(req, NT_STATUS_INVALID_DEVICE_REQUEST);
			return tevent_req_post(req, ev);
		}

		if (!lp_host_msdfs()) {
			tevent_req_nterror(req, NT_STATUS_FS_DRIVER_REQUIRED);
			return tevent_req_post(req, ev);
		}

		if (in_input.length < (2 + 2)) {
			tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
			return tevent_req_post(req, ev);
		}

		in_max_referral_level = SVAL(in_input.data, 0);
		in_file_name_buffer.data = in_input.data + 2;
		in_file_name_buffer.length = in_input.length - 2;

		ok = convert_string_talloc(state, CH_UTF16, CH_UNIX,
					   in_file_name_buffer.data,
					   in_file_name_buffer.length,
					   &in_file_name_string,
					   &in_file_name_string_size, false);
		if (!ok) {
			tevent_req_nterror(req, NT_STATUS_ILLEGAL_CHARACTER);
			return tevent_req_post(req, ev);
		}

		dfs_size = setup_dfs_referral(smbreq->conn,
					      in_file_name_string,
					      in_max_referral_level,
					      &dfs_data, &status);
		if (dfs_size < 0) {
			tevent_req_nterror(req, status);
			return tevent_req_post(req, ev);
		}

		if (dfs_size > in_max_output) {
			/*
			 * TODO: we need a testsuite for this
			 */
			overflow = true;
			dfs_size = in_max_output;
		}

		state->out_output = data_blob_talloc(state,
						     (uint8_t *)dfs_data,
						     dfs_size);
		SAFE_FREE(dfs_data);
		if (dfs_size > 0 &&
		    tevent_req_nomem(state->out_output.data, req)) {
			return tevent_req_post(req, ev);
		}

		if (overflow) {
			tevent_req_nterror(req, STATUS_BUFFER_OVERFLOW);
		} else {
			tevent_req_done(req);
		}
		return tevent_req_post(req, ev);
	}
	case 0x0011C017: /* FSCTL_PIPE_TRANSCEIVE */

		if (!IS_IPC(smbreq->conn)) {
			tevent_req_nterror(req, NT_STATUS_NOT_SUPPORTED);
			return tevent_req_post(req, ev);
		}

		if (fsp == NULL) {
			tevent_req_nterror(req, NT_STATUS_FILE_CLOSED);
			return tevent_req_post(req, ev);
		}

		if (!fsp_is_np(fsp)) {
			tevent_req_nterror(req, NT_STATUS_FILE_CLOSED);
			return tevent_req_post(req, ev);
		}

		DEBUG(10,("smbd_smb2_ioctl_send: np_write_send of size %u\n",
			(unsigned int)in_input.length ));

		subreq = np_write_send(state, ev,
				       fsp->fake_file_handle,
				       in_input.data,
				       in_input.length);
		if (tevent_req_nomem(subreq, req)) {
			return tevent_req_post(req, ev);
		}
		tevent_req_set_callback(subreq,
					smbd_smb2_ioctl_pipe_write_done,
					req);
		return req;

	default: {
		uint8_t *out_data = NULL;
		uint32_t out_data_len = 0;
		NTSTATUS status;

		if (fsp == NULL) {
			tevent_req_nterror(req, NT_STATUS_FILE_CLOSED);
			return tevent_req_post(req, ev);
		}

		status = smb_fsctl(fsp,
				       state,
				       in_ctl_code,
				       smbreq->flags2,
				       in_input.data,
				       in_input.length,
				       &out_data,
				       in_max_output,
				       &out_data_len);
		state->out_output = data_blob_const(out_data, out_data_len);
		if (NT_STATUS_IS_OK(status)) {
			tevent_req_done(req);
			return tevent_req_post(req, ev);
		}

		if (NT_STATUS_EQUAL(status, NT_STATUS_NOT_SUPPORTED)) {
			if (IS_IPC(smbreq->conn)) {
				status = NT_STATUS_FS_DRIVER_REQUIRED;
			} else {
				status = NT_STATUS_INVALID_DEVICE_REQUEST;
			}
		}

		tevent_req_nterror(req, status);
		return tevent_req_post(req, ev);
	}
	}

	tevent_req_nterror(req, NT_STATUS_INTERNAL_ERROR);
	return tevent_req_post(req, ev);
}

static void smbd_smb2_ioctl_pipe_write_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(subreq,
				 struct tevent_req);
	struct smbd_smb2_ioctl_state *state = tevent_req_data(req,
					      struct smbd_smb2_ioctl_state);
	NTSTATUS status;
	ssize_t nwritten = -1;

	status = np_write_recv(subreq, &nwritten);

	DEBUG(10,("smbd_smb2_ioctl_pipe_write_done: received %ld\n",
		(long int)nwritten ));

	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}

	if (nwritten != state->in_input.length) {
		tevent_req_nterror(req, NT_STATUS_PIPE_NOT_AVAILABLE);
		return;
	}

	state->out_output = data_blob_talloc(state, NULL, state->in_max_output);
	if (state->in_max_output > 0 &&
	    tevent_req_nomem(state->out_output.data, req)) {
		return;
	}

	DEBUG(10,("smbd_smb2_ioctl_pipe_write_done: issuing np_read_send "
		"of size %u\n",
		(unsigned int)state->out_output.length ));

	TALLOC_FREE(subreq);
	subreq = np_read_send(state->smbreq->conn,
			      state->smb2req->sconn->smb2.event_ctx,
			      state->fsp->fake_file_handle,
			      state->out_output.data,
			      state->out_output.length);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, smbd_smb2_ioctl_pipe_read_done, req);
}

static void smbd_smb2_ioctl_pipe_read_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(subreq,
				 struct tevent_req);
	struct smbd_smb2_ioctl_state *state = tevent_req_data(req,
					      struct smbd_smb2_ioctl_state);
	NTSTATUS status;
	ssize_t nread = -1;
	bool is_data_outstanding = false;

	status = np_read_recv(subreq, &nread, &is_data_outstanding);

	DEBUG(10,("smbd_smb2_ioctl_pipe_read_done: np_read_recv nread = %d "
		 "is_data_outstanding = %d, status = %s\n",
		(int)nread,
		(int)is_data_outstanding,
		nt_errstr(status) ));

	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}

	state->out_output.length = nread;

	if (is_data_outstanding) {
		tevent_req_nterror(req, STATUS_BUFFER_OVERFLOW);
		return;
	}

	tevent_req_done(req);
}

static NTSTATUS smbd_smb2_ioctl_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     DATA_BLOB *out_output)
{
	NTSTATUS status = NT_STATUS_OK;
	struct smbd_smb2_ioctl_state *state = tevent_req_data(req,
					      struct smbd_smb2_ioctl_state);

	if (tevent_req_is_nterror(req, &status)) {
		if (!NT_STATUS_EQUAL(status, STATUS_BUFFER_OVERFLOW)) {
			tevent_req_received(req);
			return status;
		}
	}

	*out_output = state->out_output;
	talloc_steal(mem_ctx, out_output->data);

	tevent_req_received(req);
	return status;
}
