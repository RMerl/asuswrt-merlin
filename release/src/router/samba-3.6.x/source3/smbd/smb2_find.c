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
#include "trans2.h"
#include "../lib/util/tevent_ntstatus.h"

static struct tevent_req *smbd_smb2_find_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct smbd_smb2_request *smb2req,
					      struct files_struct *in_fsp,
					      uint8_t in_file_info_class,
					      uint8_t in_flags,
					      uint32_t in_file_index,
					      uint32_t in_output_buffer_length,
					      const char *in_file_name);
static NTSTATUS smbd_smb2_find_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx,
				    DATA_BLOB *out_output_buffer);

static void smbd_smb2_request_find_done(struct tevent_req *subreq);
NTSTATUS smbd_smb2_request_process_find(struct smbd_smb2_request *req)
{
	NTSTATUS status;
	const uint8_t *inbody;
	int i = req->current_idx;
	uint8_t in_file_info_class;
	uint8_t in_flags;
	uint32_t in_file_index;
	uint64_t in_file_id_persistent;
	uint64_t in_file_id_volatile;
	struct files_struct *in_fsp;
	uint16_t in_file_name_offset;
	uint16_t in_file_name_length;
	DATA_BLOB in_file_name_buffer;
	char *in_file_name_string;
	size_t in_file_name_string_size;
	uint32_t in_output_buffer_length;
	struct tevent_req *subreq;
	bool ok;

	status = smbd_smb2_request_verify_sizes(req, 0x21);
	if (!NT_STATUS_IS_OK(status)) {
		return smbd_smb2_request_error(req, status);
	}
	inbody = (const uint8_t *)req->in.vector[i+1].iov_base;

	in_file_info_class		= CVAL(inbody, 0x02);
	in_flags			= CVAL(inbody, 0x03);
	in_file_index			= IVAL(inbody, 0x04);
	in_file_id_persistent		= BVAL(inbody, 0x08);
	in_file_id_volatile		= BVAL(inbody, 0x10);
	in_file_name_offset		= SVAL(inbody, 0x18);
	in_file_name_length		= SVAL(inbody, 0x1A);
	in_output_buffer_length		= IVAL(inbody, 0x1C);

	if (in_file_name_offset == 0 && in_file_name_length == 0) {
		/* This is ok */
	} else if (in_file_name_offset !=
		   (SMB2_HDR_BODY + req->in.vector[i+1].iov_len)) {
		return smbd_smb2_request_error(req, NT_STATUS_INVALID_PARAMETER);
	}

	if (in_file_name_length > req->in.vector[i+2].iov_len) {
		return smbd_smb2_request_error(req, NT_STATUS_INVALID_PARAMETER);
	}

	/* The output header is 8 bytes. */
	if (in_output_buffer_length <= 8) {
		return smbd_smb2_request_error(req, NT_STATUS_INVALID_PARAMETER);
	}

	DEBUG(10,("smbd_smb2_request_find_done: in_output_buffer_length = %u\n",
		(unsigned int)in_output_buffer_length ));

	/* Take into account the output header. */
	in_output_buffer_length -= 8;

	in_file_name_buffer.data = (uint8_t *)req->in.vector[i+2].iov_base;
	in_file_name_buffer.length = in_file_name_length;

	ok = convert_string_talloc(req, CH_UTF16, CH_UNIX,
				   in_file_name_buffer.data,
				   in_file_name_buffer.length,
				   &in_file_name_string,
				   &in_file_name_string_size, false);
	if (!ok) {
		return smbd_smb2_request_error(req, NT_STATUS_ILLEGAL_CHARACTER);
	}

	if (in_file_name_buffer.length == 0) {
		in_file_name_string_size = 0;
	}

	if (strlen(in_file_name_string) != in_file_name_string_size) {
		return smbd_smb2_request_error(req, NT_STATUS_OBJECT_NAME_INVALID);
	}

	in_fsp = file_fsp_smb2(req, in_file_id_persistent, in_file_id_volatile);
	if (in_fsp == NULL) {
		return smbd_smb2_request_error(req, NT_STATUS_FILE_CLOSED);
	}

	subreq = smbd_smb2_find_send(req, req->sconn->smb2.event_ctx,
				     req, in_fsp,
				     in_file_info_class,
				     in_flags,
				     in_file_index,
				     in_output_buffer_length,
				     in_file_name_string);
	if (subreq == NULL) {
		return smbd_smb2_request_error(req, NT_STATUS_NO_MEMORY);
	}
	tevent_req_set_callback(subreq, smbd_smb2_request_find_done, req);

	return smbd_smb2_request_pending_queue(req, subreq);
}

static void smbd_smb2_request_find_done(struct tevent_req *subreq)
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

	status = smbd_smb2_find_recv(subreq,
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

	DEBUG(10,("smbd_smb2_request_find_done: out_output_buffer.length = %u\n",
		(unsigned int)out_output_buffer.length ));

	outdyn = out_output_buffer;

	error = smbd_smb2_request_done(req, outbody, &outdyn);
	if (!NT_STATUS_IS_OK(error)) {
		smbd_server_connection_terminate(req->sconn,
						 nt_errstr(error));
		return;
	}
}

struct smbd_smb2_find_state {
	struct smbd_smb2_request *smb2req;
	DATA_BLOB out_output_buffer;
};

static struct tevent_req *smbd_smb2_find_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct smbd_smb2_request *smb2req,
					      struct files_struct *fsp,
					      uint8_t in_file_info_class,
					      uint8_t in_flags,
					      uint32_t in_file_index,
					      uint32_t in_output_buffer_length,
					      const char *in_file_name)
{
	struct tevent_req *req;
	struct smbd_smb2_find_state *state;
	struct smb_request *smbreq;
	connection_struct *conn = smb2req->tcon->compat_conn;
	NTSTATUS status;
	NTSTATUS empty_status;
	uint32_t info_level;
	uint32_t max_count;
	char *pdata;
	char *base_data;
	char *end_data;
	int last_entry_off = 0;
	int off = 0;
	uint32_t num = 0;
	uint32_t dirtype = FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_DIRECTORY;
	bool dont_descend = false;
	bool ask_sharemode = true;

	req = tevent_req_create(mem_ctx, &state,
				struct smbd_smb2_find_state);
	if (req == NULL) {
		return NULL;
	}
	state->smb2req = smb2req;
	state->out_output_buffer = data_blob_null;

	DEBUG(10,("smbd_smb2_find_send: %s - fnum[%d]\n",
		  fsp_str_dbg(fsp), fsp->fnum));

	smbreq = smbd_smb2_fake_smb_request(smb2req);
	if (tevent_req_nomem(smbreq, req)) {
		return tevent_req_post(req, ev);
	}

	if (!fsp->is_directory) {
		tevent_req_nterror(req, NT_STATUS_NOT_SUPPORTED);
		return tevent_req_post(req, ev);
	}

	if (strcmp(in_file_name, "") == 0) {
		tevent_req_nterror(req, NT_STATUS_OBJECT_NAME_INVALID);
		return tevent_req_post(req, ev);
	}
	if (strcmp(in_file_name, "\\") == 0) {
		tevent_req_nterror(req, NT_STATUS_OBJECT_NAME_INVALID);
		return tevent_req_post(req, ev);
	}
	if (strcmp(in_file_name, "/") == 0) {
		tevent_req_nterror(req, NT_STATUS_OBJECT_NAME_INVALID);
		return tevent_req_post(req, ev);
	}

	if (in_output_buffer_length > smb2req->sconn->smb2.max_trans) {
		tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return tevent_req_post(req, ev);
	}

	switch (in_file_info_class) {
	case SMB2_FIND_DIRECTORY_INFO:
		info_level = SMB_FIND_FILE_DIRECTORY_INFO;
		break;

	case SMB2_FIND_FULL_DIRECTORY_INFO:
		info_level = SMB_FIND_FILE_FULL_DIRECTORY_INFO;
		break;

	case SMB2_FIND_BOTH_DIRECTORY_INFO:
		info_level = SMB_FIND_FILE_BOTH_DIRECTORY_INFO;
		break;

	case SMB2_FIND_NAME_INFO:
		info_level = SMB_FIND_FILE_NAMES_INFO;
		break;

	case SMB2_FIND_ID_BOTH_DIRECTORY_INFO:
		info_level = SMB_FIND_ID_BOTH_DIRECTORY_INFO;
		break;

	case SMB2_FIND_ID_FULL_DIRECTORY_INFO:
		info_level = SMB_FIND_ID_FULL_DIRECTORY_INFO;
		break;

	default:
		tevent_req_nterror(req, NT_STATUS_INVALID_INFO_CLASS);
		return tevent_req_post(req, ev);
	}

	if (in_flags & SMB2_CONTINUE_FLAG_REOPEN) {
		dptr_CloseDir(fsp);
	}

	if (fsp->dptr == NULL) {
		bool wcard_has_wild;

		if (!(fsp->access_mask & SEC_DIR_LIST)) {
			tevent_req_nterror(req, NT_STATUS_ACCESS_DENIED);
			return tevent_req_post(req, ev);
		}

		wcard_has_wild = ms_has_wild(in_file_name);

		status = dptr_create(conn,
				     fsp,
				     fsp->fsp_name->base_name,
				     false, /* old_handle */
				     false, /* expect_close */
				     0, /* spid */
				     in_file_name, /* wcard */
				     wcard_has_wild,
				     dirtype,
				     &fsp->dptr);
		if (!NT_STATUS_IS_OK(status)) {
			tevent_req_nterror(req, status);
			return tevent_req_post(req, ev);
		}

		empty_status = NT_STATUS_NO_SUCH_FILE;
	} else {
		empty_status = STATUS_NO_MORE_FILES;
	}

	if (in_flags & SMB2_CONTINUE_FLAG_RESTART) {
		dptr_SeekDir(fsp->dptr, 0);
	}

	if (in_flags & SMB2_CONTINUE_FLAG_SINGLE) {
		max_count = 1;
	} else {
		max_count = UINT16_MAX;
	}

#define DIR_ENTRY_SAFETY_MARGIN 4096

	state->out_output_buffer = data_blob_talloc(state, NULL,
			in_output_buffer_length + DIR_ENTRY_SAFETY_MARGIN);
	if (tevent_req_nomem(state->out_output_buffer.data, req)) {
		return tevent_req_post(req, ev);
	}

	state->out_output_buffer.length = 0;
	pdata = (char *)state->out_output_buffer.data;
	base_data = pdata;
	/*
	 * end_data must include the safety margin as it's what is
	 * used to determine if pushed strings have been truncated.
	 */
	end_data = pdata + in_output_buffer_length + DIR_ENTRY_SAFETY_MARGIN - 1;
	last_entry_off = 0;
	off = 0;
	num = 0;

	DEBUG(8,("smbd_smb2_find_send: dirpath=<%s> dontdescend=<%s>, "
		"in_output_buffer_length = %u\n",
		fsp->fsp_name->base_name, lp_dontdescend(SNUM(conn)),
		(unsigned int)in_output_buffer_length ));
	if (in_list(fsp->fsp_name->base_name,lp_dontdescend(SNUM(conn)),
			conn->case_sensitive)) {
		dont_descend = true;
	}

	ask_sharemode = lp_parm_bool(SNUM(conn),
				     "smbd", "search ask sharemode",
				     true);

	while (true) {
		bool ok;
		bool got_exact_match = false;
		bool out_of_space = false;
		int space_remaining = in_output_buffer_length - off;

		SMB_ASSERT(space_remaining >= 0);

		ok = smbd_dirptr_lanman2_entry(state,
					       conn,
					       fsp->dptr,
					       smbreq->flags2,
					       in_file_name,
					       dirtype,
					       info_level,
					       false, /* requires_resume_key */
					       dont_descend,
					       ask_sharemode,
					       8, /* align to 8 bytes */
					       false, /* no padding */
					       &pdata,
					       base_data,
					       end_data,
					       space_remaining,
					       &out_of_space,
					       &got_exact_match,
					       &last_entry_off,
					       NULL);

		off = (int)PTR_DIFF(pdata, base_data);

		if (!ok) {
			if (num > 0) {
				SIVAL(state->out_output_buffer.data, last_entry_off, 0);
				tevent_req_done(req);
				return tevent_req_post(req, ev);
			} else if (out_of_space) {
				tevent_req_nterror(req, NT_STATUS_INFO_LENGTH_MISMATCH);
				return tevent_req_post(req, ev);
			} else {
				tevent_req_nterror(req, empty_status);
				return tevent_req_post(req, ev);
			}
		}

		num++;
		state->out_output_buffer.length = off;

		if (num < max_count) {
			continue;
		}

		SIVAL(state->out_output_buffer.data, last_entry_off, 0);
		tevent_req_done(req);
		return tevent_req_post(req, ev);
	}

	tevent_req_nterror(req, NT_STATUS_INTERNAL_ERROR);
	return tevent_req_post(req, ev);
}

static NTSTATUS smbd_smb2_find_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx,
				    DATA_BLOB *out_output_buffer)
{
	NTSTATUS status;
	struct smbd_smb2_find_state *state = tevent_req_data(req,
					     struct smbd_smb2_find_state);

	if (tevent_req_is_nterror(req, &status)) {
		tevent_req_received(req);
		return status;
	}

	*out_output_buffer = state->out_output_buffer;
	talloc_steal(mem_ctx, out_output_buffer->data);

	tevent_req_received(req);
	return NT_STATUS_OK;
}
