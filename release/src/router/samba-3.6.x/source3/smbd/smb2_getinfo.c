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
#include "trans2.h"
#include "../lib/util/tevent_ntstatus.h"

static struct tevent_req *smbd_smb2_getinfo_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct smbd_smb2_request *smb2req,
						 struct files_struct *in_fsp,
						 uint8_t in_info_type,
						 uint8_t in_file_info_class,
						 uint32_t in_output_buffer_length,
						 DATA_BLOB in_input_buffer,
						 uint32_t in_additional_information,
						 uint32_t in_flags);
static NTSTATUS smbd_smb2_getinfo_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       DATA_BLOB *out_output_buffer,
				       NTSTATUS *p_call_status);

static void smbd_smb2_request_getinfo_done(struct tevent_req *subreq);
NTSTATUS smbd_smb2_request_process_getinfo(struct smbd_smb2_request *req)
{
	NTSTATUS status;
	const uint8_t *inbody;
	int i = req->current_idx;
	uint8_t in_info_type;
	uint8_t in_file_info_class;
	uint32_t in_output_buffer_length;
	uint16_t in_input_buffer_offset;
	uint32_t in_input_buffer_length;
	DATA_BLOB in_input_buffer;
	uint32_t in_additional_information;
	uint32_t in_flags;
	uint64_t in_file_id_persistent;
	uint64_t in_file_id_volatile;
	struct files_struct *in_fsp;
	struct tevent_req *subreq;

	status = smbd_smb2_request_verify_sizes(req, 0x29);
	if (!NT_STATUS_IS_OK(status)) {
		return smbd_smb2_request_error(req, status);
	}
	inbody = (const uint8_t *)req->in.vector[i+1].iov_base;

	in_info_type			= CVAL(inbody, 0x02);
	in_file_info_class		= CVAL(inbody, 0x03);
	in_output_buffer_length		= IVAL(inbody, 0x04);
	in_input_buffer_offset		= SVAL(inbody, 0x08);
	/* 0x0A 2 bytes reserved */
	in_input_buffer_length		= IVAL(inbody, 0x0C);
	in_additional_information	= IVAL(inbody, 0x10);
	in_flags			= IVAL(inbody, 0x14);
	in_file_id_persistent		= BVAL(inbody, 0x18);
	in_file_id_volatile		= BVAL(inbody, 0x20);

	if (in_input_buffer_offset == 0 && in_input_buffer_length == 0) {
		/* This is ok */
	} else if (in_input_buffer_offset !=
		   (SMB2_HDR_BODY + req->in.vector[i+1].iov_len)) {
		return smbd_smb2_request_error(req, NT_STATUS_INVALID_PARAMETER);
	}

	if (in_input_buffer_length > req->in.vector[i+2].iov_len) {
		return smbd_smb2_request_error(req, NT_STATUS_INVALID_PARAMETER);
	}

	in_input_buffer.data = (uint8_t *)req->in.vector[i+2].iov_base;
	in_input_buffer.length = in_input_buffer_length;

	if (in_input_buffer.length > req->sconn->smb2.max_trans) {
		return smbd_smb2_request_error(req, NT_STATUS_INVALID_PARAMETER);
	}
	if (in_output_buffer_length > req->sconn->smb2.max_trans) {
		return smbd_smb2_request_error(req, NT_STATUS_INVALID_PARAMETER);
	}

	in_fsp = file_fsp_smb2(req, in_file_id_persistent, in_file_id_volatile);
	if (in_fsp == NULL) {
		return smbd_smb2_request_error(req, NT_STATUS_FILE_CLOSED);
	}

	subreq = smbd_smb2_getinfo_send(req, req->sconn->smb2.event_ctx,
					req, in_fsp,
					in_info_type,
					in_file_info_class,
					in_output_buffer_length,
					in_input_buffer,
					in_additional_information,
					in_flags);
	if (subreq == NULL) {
		return smbd_smb2_request_error(req, NT_STATUS_NO_MEMORY);
	}
	tevent_req_set_callback(subreq, smbd_smb2_request_getinfo_done, req);

	return smbd_smb2_request_pending_queue(req, subreq);
}

static void smbd_smb2_request_getinfo_done(struct tevent_req *subreq)
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
	NTSTATUS call_status = NT_STATUS_OK;
	NTSTATUS error; /* transport error */

	status = smbd_smb2_getinfo_recv(subreq,
					req,
					&out_output_buffer,
					&call_status);
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

	/* some GetInfo responses set STATUS_BUFFER_OVERFLOW and return partial,
	   but valid data */
	if (!(NT_STATUS_IS_OK(call_status) ||
	      NT_STATUS_EQUAL(call_status, STATUS_BUFFER_OVERFLOW))) {
		/* Return a specific error with data. */
		error = smbd_smb2_request_error_ex(req,
						call_status,
						&out_output_buffer,
						__location__);
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

	error = smbd_smb2_request_done_ex(req, call_status, outbody, &outdyn, __location__);
	if (!NT_STATUS_IS_OK(error)) {
		smbd_server_connection_terminate(req->sconn,
						 nt_errstr(error));
		return;
	}
}

struct smbd_smb2_getinfo_state {
	struct smbd_smb2_request *smb2req;
	NTSTATUS status;
	DATA_BLOB out_output_buffer;
};

static void smb2_ipc_getinfo(struct tevent_req *req,
				struct smbd_smb2_getinfo_state *state,
				struct tevent_context *ev,
				uint8_t in_info_type,
				uint8_t in_file_info_class)
{
	/* We want to reply to SMB2_GETINFO_FILE
	   with a class of SMB2_FILE_STANDARD_INFO as
	   otherwise a Win7 client issues this request
	   twice (2xroundtrips) if we return NOT_SUPPORTED.
	   NB. We do the same for SMB1 in call_trans2qpipeinfo() */

	if (in_info_type == 0x01 && /* SMB2_GETINFO_FILE */
			in_file_info_class == 0x05) { /* SMB2_FILE_STANDARD_INFO */
		state->out_output_buffer = data_blob_talloc(state,
						NULL, 24);
		if (tevent_req_nomem(state->out_output_buffer.data, req)) {
			return;
		}

		memset(state->out_output_buffer.data,0,24);
		SOFF_T(state->out_output_buffer.data,0,4096LL);
		SIVAL(state->out_output_buffer.data,16,1);
		SIVAL(state->out_output_buffer.data,20,1);
		tevent_req_done(req);
	} else {
		tevent_req_nterror(req, NT_STATUS_NOT_SUPPORTED);
	}
}

static struct tevent_req *smbd_smb2_getinfo_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct smbd_smb2_request *smb2req,
						 struct files_struct *fsp,
						 uint8_t in_info_type,
						 uint8_t in_file_info_class,
						 uint32_t in_output_buffer_length,
						 DATA_BLOB in_input_buffer,
						 uint32_t in_additional_information,
						 uint32_t in_flags)
{
	struct tevent_req *req;
	struct smbd_smb2_getinfo_state *state;
	struct smb_request *smbreq;
	connection_struct *conn = smb2req->tcon->compat_conn;
	NTSTATUS status;

	req = tevent_req_create(mem_ctx, &state,
				struct smbd_smb2_getinfo_state);
	if (req == NULL) {
		return NULL;
	}
	state->smb2req = smb2req;
	state->status = NT_STATUS_OK;
	state->out_output_buffer = data_blob_null;

	DEBUG(10,("smbd_smb2_getinfo_send: %s - fnum[%d]\n",
		  fsp_str_dbg(fsp), fsp->fnum));

	smbreq = smbd_smb2_fake_smb_request(smb2req);
	if (tevent_req_nomem(smbreq, req)) {
		return tevent_req_post(req, ev);
	}

	if (IS_IPC(conn)) {
		smb2_ipc_getinfo(req, state, ev,
			in_info_type, in_file_info_class);
		return tevent_req_post(req, ev);
	}

	switch (in_info_type) {
	case SMB2_GETINFO_FILE:
	{
		uint16_t file_info_level;
		char *data = NULL;
		unsigned int data_size = 0;
		bool delete_pending = false;
		struct timespec write_time_ts;
		struct file_id fileid;
		struct ea_list *ea_list = NULL;
		int lock_data_count = 0;
		char *lock_data = NULL;
		size_t fixed_portion;

		ZERO_STRUCT(write_time_ts);

		switch (in_file_info_class) {
		case 0x0F:/* RAW_FILEINFO_SMB2_ALL_EAS */
			file_info_level = 0xFF00 | in_file_info_class;
			break;

		case 0x12:/* RAW_FILEINFO_SMB2_ALL_INFORMATION */
			file_info_level = 0xFF00 | in_file_info_class;
			break;

		default:
			/* the levels directly map to the passthru levels */
			file_info_level = in_file_info_class + 1000;
			break;
		}

		if (fsp->fake_file_handle) {
			/*
			 * This is actually for the QUOTA_FAKE_FILE --metze
			 */

			/* We know this name is ok, it's already passed the checks. */

		} else if (fsp && fsp->fh->fd == -1) {
			/*
			 * This is actually a QFILEINFO on a directory
			 * handle (returned from an NT SMB). NT5.0 seems
			 * to do this call. JRA.
			 */

			if (INFO_LEVEL_IS_UNIX(file_info_level)) {
				/* Always do lstat for UNIX calls. */
				if (SMB_VFS_LSTAT(conn, fsp->fsp_name)) {
					DEBUG(3,("smbd_smb2_getinfo_send: "
						 "SMB_VFS_LSTAT of %s failed "
						 "(%s)\n", fsp_str_dbg(fsp),
						 strerror(errno)));
					status = map_nt_error_from_unix(errno);
					tevent_req_nterror(req, status);
					return tevent_req_post(req, ev);
				}
			} else if (SMB_VFS_STAT(conn, fsp->fsp_name)) {
				DEBUG(3,("smbd_smb2_getinfo_send: "
					 "SMB_VFS_STAT of %s failed (%s)\n",
					 fsp_str_dbg(fsp),
					 strerror(errno)));
				status = map_nt_error_from_unix(errno);
				tevent_req_nterror(req, status);
				return tevent_req_post(req, ev);
			}

			fileid = vfs_file_id_from_sbuf(conn,
						       &fsp->fsp_name->st);
			get_file_infos(fileid, fsp->name_hash,
				&delete_pending, &write_time_ts);
		} else {
			/*
			 * Original code - this is an open file.
			 */

			if (SMB_VFS_FSTAT(fsp, &fsp->fsp_name->st) != 0) {
				DEBUG(3, ("smbd_smb2_getinfo_send: "
					  "fstat of fnum %d failed (%s)\n",
					  fsp->fnum, strerror(errno)));
				status = map_nt_error_from_unix(errno);
				tevent_req_nterror(req, status);
				return tevent_req_post(req, ev);
			}
			fileid = vfs_file_id_from_sbuf(conn,
						       &fsp->fsp_name->st);
			get_file_infos(fileid, fsp->name_hash,
				&delete_pending, &write_time_ts);
		}

		status = smbd_do_qfilepathinfo(conn, state,
					       file_info_level,
					       fsp,
					       fsp->fsp_name,
					       delete_pending,
					       write_time_ts,
					       ea_list,
					       lock_data_count,
					       lock_data,
					       STR_UNICODE,
					       in_output_buffer_length,
					       &fixed_portion,
					       &data,
					       &data_size);
		if (!NT_STATUS_IS_OK(status)) {
			SAFE_FREE(data);
			if (NT_STATUS_EQUAL(status, NT_STATUS_INVALID_LEVEL)) {
				status = NT_STATUS_INVALID_INFO_CLASS;
			}
			tevent_req_nterror(req, status);
			return tevent_req_post(req, ev);
		}
		if (in_output_buffer_length < fixed_portion) {
			SAFE_FREE(data);
			tevent_req_nterror(
				req, NT_STATUS_INFO_LENGTH_MISMATCH);
			return tevent_req_post(req, ev);
		}
		if (data_size > 0) {
			state->out_output_buffer = data_blob_talloc(state,
								    data,
								    data_size);
			SAFE_FREE(data);
			if (tevent_req_nomem(state->out_output_buffer.data, req)) {
				return tevent_req_post(req, ev);
			}
			if (data_size > in_output_buffer_length) {
				state->out_output_buffer.length =
					in_output_buffer_length;
				status = STATUS_BUFFER_OVERFLOW;
			}
		}
		SAFE_FREE(data);
		break;
	}

	case SMB2_GETINFO_FS:
	{
		uint16_t file_info_level;
		char *data = NULL;
		int data_size = 0;
		size_t fixed_portion;

		/* the levels directly map to the passthru levels */
		file_info_level = in_file_info_class + 1000;

		status = smbd_do_qfsinfo(conn, state,
					 file_info_level,
					 STR_UNICODE,
					 in_output_buffer_length,
					 &fixed_portion,
					 fsp->fsp_name,
					 &data,
					 &data_size);
		/* some responses set STATUS_BUFFER_OVERFLOW and return
		   partial, but valid data */
		if (!(NT_STATUS_IS_OK(status) ||
		      NT_STATUS_EQUAL(status, STATUS_BUFFER_OVERFLOW))) {
			SAFE_FREE(data);
			if (NT_STATUS_EQUAL(status, NT_STATUS_INVALID_LEVEL)) {
				status = NT_STATUS_INVALID_INFO_CLASS;
			}
			tevent_req_nterror(req, status);
			return tevent_req_post(req, ev);
		}
		if (in_output_buffer_length < fixed_portion) {
			SAFE_FREE(data);
			tevent_req_nterror(
				req, NT_STATUS_INFO_LENGTH_MISMATCH);
			return tevent_req_post(req, ev);
		}
		if (data_size > 0) {
			state->out_output_buffer = data_blob_talloc(state,
								    data,
								    data_size);
			SAFE_FREE(data);
			if (tevent_req_nomem(state->out_output_buffer.data, req)) {
				return tevent_req_post(req, ev);
			}
			if (data_size > in_output_buffer_length) {
				state->out_output_buffer.length =
					in_output_buffer_length;
				status = STATUS_BUFFER_OVERFLOW;
			}
		}
		SAFE_FREE(data);
		break;
	}

	case SMB2_GETINFO_SECURITY:
	{
		uint8_t *p_marshalled_sd = NULL;
		size_t sd_size = 0;

		status = smbd_do_query_security_desc(conn,
				state,
				fsp,
				/* Security info wanted. */
				in_additional_information,
				in_output_buffer_length,
				&p_marshalled_sd,
				&sd_size);

		if (NT_STATUS_EQUAL(status, NT_STATUS_BUFFER_TOO_SMALL)) {
			/* Return needed size. */
			state->out_output_buffer = data_blob_talloc(state,
								    NULL,
								    4);
			if (tevent_req_nomem(state->out_output_buffer.data, req)) {
				return tevent_req_post(req, ev);
			}
			SIVAL(state->out_output_buffer.data,0,(uint32_t)sd_size);
			state->status = NT_STATUS_BUFFER_TOO_SMALL;
			break;
		}
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(10,("smbd_smb2_getinfo_send: "
				 "smbd_do_query_security_desc of %s failed "
				 "(%s)\n", fsp_str_dbg(fsp),
				 nt_errstr(status)));
			tevent_req_nterror(req, status);
			return tevent_req_post(req, ev);
		}

		if (sd_size > 0) {
			state->out_output_buffer = data_blob_talloc(state,
								    p_marshalled_sd,
								    sd_size);
			if (tevent_req_nomem(state->out_output_buffer.data, req)) {
				return tevent_req_post(req, ev);
			}
		}
		break;
	}

	default:
		DEBUG(10,("smbd_smb2_getinfo_send: "
			"unknown in_info_type of %u "
			" for file %s\n",
			(unsigned int)in_info_type,
			fsp_str_dbg(fsp) ));

		tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return tevent_req_post(req, ev);
	}

	state->status = status;
	tevent_req_done(req);
	return tevent_req_post(req, ev);
}

static NTSTATUS smbd_smb2_getinfo_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       DATA_BLOB *out_output_buffer,
				       NTSTATUS *pstatus)
{
	NTSTATUS status;
	struct smbd_smb2_getinfo_state *state = tevent_req_data(req,
						struct smbd_smb2_getinfo_state);

	if (tevent_req_is_nterror(req, &status)) {
		tevent_req_received(req);
		return status;
	}

	*out_output_buffer = state->out_output_buffer;
	talloc_steal(mem_ctx, out_output_buffer->data);
	*pstatus = state->status;

	tevent_req_received(req);
	return NT_STATUS_OK;
}
