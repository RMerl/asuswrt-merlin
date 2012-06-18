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

static struct tevent_req *smbd_smb2_create_send(TALLOC_CTX *mem_ctx,
			struct tevent_context *ev,
			struct smbd_smb2_request *smb2req,
			uint8_t in_oplock_level,
			uint32_t in_impersonation_level,
			uint32_t in_desired_access,
			uint32_t in_file_attributes,
			uint32_t in_share_access,
			uint32_t in_create_disposition,
			uint32_t in_create_options,
			const char *in_name,
			struct smb2_create_blobs in_context_blobs);
static NTSTATUS smbd_smb2_create_recv(struct tevent_req *req,
			TALLOC_CTX *mem_ctx,
			uint8_t *out_oplock_level,
			uint32_t *out_create_action,
			NTTIME *out_creation_time,
			NTTIME *out_last_access_time,
			NTTIME *out_last_write_time,
			NTTIME *out_change_time,
			uint64_t *out_allocation_size,
			uint64_t *out_end_of_file,
			uint32_t *out_file_attributes,
			uint64_t *out_file_id_volatile,
			struct smb2_create_blobs *out_context_blobs);

static void smbd_smb2_request_create_done(struct tevent_req *subreq);
NTSTATUS smbd_smb2_request_process_create(struct smbd_smb2_request *req)
{
	const uint8_t *inbody;
	int i = req->current_idx;
	size_t expected_body_size = 0x39;
	size_t body_size;
	uint8_t in_oplock_level;
	uint32_t in_impersonation_level;
	uint32_t in_desired_access;
	uint32_t in_file_attributes;
	uint32_t in_share_access;
	uint32_t in_create_disposition;
	uint32_t in_create_options;
	uint16_t in_name_offset;
	uint16_t in_name_length;
	DATA_BLOB in_name_buffer;
	char *in_name_string;
	size_t in_name_string_size;
	uint32_t name_offset = 0;
	uint32_t name_available_length = 0;
	uint32_t in_context_offset;
	uint32_t in_context_length;
	DATA_BLOB in_context_buffer;
	struct smb2_create_blobs in_context_blobs;
	uint32_t context_offset = 0;
	uint32_t context_available_length = 0;
	uint32_t dyn_offset;
	NTSTATUS status;
	bool ok;
	struct tevent_req *subreq;

	if (req->in.vector[i+1].iov_len != (expected_body_size & 0xFFFFFFFE)) {
		return smbd_smb2_request_error(req, NT_STATUS_INVALID_PARAMETER);
	}

	inbody = (const uint8_t *)req->in.vector[i+1].iov_base;

	body_size = SVAL(inbody, 0x00);
	if (body_size != expected_body_size) {
		return smbd_smb2_request_error(req, NT_STATUS_INVALID_PARAMETER);
	}

	in_oplock_level		= CVAL(inbody, 0x03);
	in_impersonation_level	= IVAL(inbody, 0x04);
	in_desired_access	= IVAL(inbody, 0x18);
	in_file_attributes	= IVAL(inbody, 0x1C);
	in_share_access		= IVAL(inbody, 0x20);
	in_create_disposition	= IVAL(inbody, 0x24);
	in_create_options	= IVAL(inbody, 0x28);
	in_name_offset		= SVAL(inbody, 0x2C);
	in_name_length		= SVAL(inbody, 0x2E);
	in_context_offset	= IVAL(inbody, 0x30);
	in_context_length	= IVAL(inbody, 0x34);

	/*
	 * First check if the dynamic name and context buffers
	 * are correctly specified.
	 *
	 * Note: That we don't check if the name and context buffers
	 *       overlap
	 */

	dyn_offset = SMB2_HDR_BODY + (body_size & 0xFFFFFFFE);

	if (in_name_offset == 0 && in_name_length == 0) {
		/* This is ok */
		name_offset = 0;
	} else if (in_name_offset < dyn_offset) {
		return smbd_smb2_request_error(req, NT_STATUS_INVALID_PARAMETER);
	} else {
		name_offset = in_name_offset - dyn_offset;
	}

	if (name_offset > req->in.vector[i+2].iov_len) {
		return smbd_smb2_request_error(req, NT_STATUS_INVALID_PARAMETER);
	}

	name_available_length = req->in.vector[i+2].iov_len - name_offset;

	if (in_name_length > name_available_length) {
		return smbd_smb2_request_error(req, NT_STATUS_INVALID_PARAMETER);
	}

	in_name_buffer.data = (uint8_t *)req->in.vector[i+2].iov_base +
			      name_offset;
	in_name_buffer.length = in_name_length;

	if (in_context_offset == 0 && in_context_length == 0) {
		/* This is ok */
		context_offset = 0;
	} else if (in_context_offset < dyn_offset) {
		return smbd_smb2_request_error(req, NT_STATUS_INVALID_PARAMETER);
	} else {
		context_offset = in_context_offset - dyn_offset;
	}

	if (context_offset > req->in.vector[i+2].iov_len) {
		return smbd_smb2_request_error(req, NT_STATUS_INVALID_PARAMETER);
	}

	context_available_length = req->in.vector[i+2].iov_len - context_offset;

	if (in_context_length > context_available_length) {
		return smbd_smb2_request_error(req, NT_STATUS_INVALID_PARAMETER);
	}

	in_context_buffer.data = (uint8_t *)req->in.vector[i+2].iov_base +
				  context_offset;
	in_context_buffer.length = in_context_length;

	/*
	 * Now interpret the name and context buffers
	 */

	ok = convert_string_talloc(req, CH_UTF16, CH_UNIX,
				   in_name_buffer.data,
				   in_name_buffer.length,
				   &in_name_string,
				   &in_name_string_size, false);
	if (!ok) {
		return smbd_smb2_request_error(req, NT_STATUS_ILLEGAL_CHARACTER);
	}

	ZERO_STRUCT(in_context_blobs);
	status = smb2_create_blob_parse(req, in_context_buffer, &in_context_blobs);
	if (!NT_STATUS_IS_OK(status)) {
		return smbd_smb2_request_error(req, status);
	}

	subreq = smbd_smb2_create_send(req,
				       req->sconn->smb2.event_ctx,
				       req,
				       in_oplock_level,
				       in_impersonation_level,
				       in_desired_access,
				       in_file_attributes,
				       in_share_access,
				       in_create_disposition,
				       in_create_options,
				       in_name_string,
				       in_context_blobs);
	if (subreq == NULL) {
		return smbd_smb2_request_error(req, NT_STATUS_NO_MEMORY);
	}
	tevent_req_set_callback(subreq, smbd_smb2_request_create_done, req);

	return smbd_smb2_request_pending_queue(req, subreq);
}

static void smbd_smb2_request_create_done(struct tevent_req *subreq)
{
	struct smbd_smb2_request *req = tevent_req_callback_data(subreq,
					struct smbd_smb2_request);
	int i = req->current_idx;
	uint8_t *outhdr;
	DATA_BLOB outbody;
	DATA_BLOB outdyn;
	uint8_t out_oplock_level = 0;
	uint32_t out_create_action = 0;
	NTTIME out_creation_time = 0;
	NTTIME out_last_access_time = 0;
	NTTIME out_last_write_time = 0;
	NTTIME out_change_time = 0;
	uint64_t out_allocation_size = 0;
	uint64_t out_end_of_file = 0;
	uint32_t out_file_attributes = 0;
	uint64_t out_file_id_volatile = 0;
	struct smb2_create_blobs out_context_blobs;
	DATA_BLOB out_context_buffer;
	uint16_t out_context_buffer_offset = 0;
	NTSTATUS status;
	NTSTATUS error; /* transport error */

	status = smbd_smb2_create_recv(subreq,
				       req,
				       &out_oplock_level,
				       &out_create_action,
				       &out_creation_time,
				       &out_last_access_time,
				       &out_last_write_time,
				       &out_change_time,
				       &out_allocation_size,
				       &out_end_of_file,
				       &out_file_attributes,
				       &out_file_id_volatile,
				       &out_context_blobs);
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

	status = smb2_create_blob_push(req, &out_context_buffer, out_context_blobs);
	if (!NT_STATUS_IS_OK(status)) {
		error = smbd_smb2_request_error(req, status);
		if (!NT_STATUS_IS_OK(error)) {
			smbd_server_connection_terminate(req->sconn,
							 nt_errstr(error));
			return;
		}
		return;
	}

	if (out_context_buffer.length > 0) {
		out_context_buffer_offset = SMB2_HDR_BODY + 0x58;
	}

	outhdr = (uint8_t *)req->out.vector[i].iov_base;

	outbody = data_blob_talloc(req->out.vector, NULL, 0x58);
	if (outbody.data == NULL) {
		error = smbd_smb2_request_error(req, NT_STATUS_NO_MEMORY);
		if (!NT_STATUS_IS_OK(error)) {
			smbd_server_connection_terminate(req->sconn,
							 nt_errstr(error));
			return;
		}
		return;
	}

	SSVAL(outbody.data, 0x00, 0x58 + 1);	/* struct size */
	SCVAL(outbody.data, 0x02,
	      out_oplock_level);		/* oplock level */
	SCVAL(outbody.data, 0x03, 0);		/* reserved */
	SIVAL(outbody.data, 0x04,
	      out_create_action);		/* create action */
	SBVAL(outbody.data, 0x08,
	      out_creation_time);		/* creation time */
	SBVAL(outbody.data, 0x10,
	      out_last_access_time);		/* last access time */
	SBVAL(outbody.data, 0x18,
	      out_last_write_time);		/* last write time */
	SBVAL(outbody.data, 0x20,
	      out_change_time);			/* change time */
	SBVAL(outbody.data, 0x28,
	      out_allocation_size);		/* allocation size */
	SBVAL(outbody.data, 0x30,
	      out_end_of_file);			/* end of file */
	SIVAL(outbody.data, 0x38,
	      out_file_attributes);		/* file attributes */
	SIVAL(outbody.data, 0x3C, 0);		/* reserved */
	SBVAL(outbody.data, 0x40, 0);		/* file id (persistent) */
	SBVAL(outbody.data, 0x48,
	      out_file_id_volatile);		/* file id (volatile) */
	SIVAL(outbody.data, 0x50,
	      out_context_buffer_offset);	/* create contexts offset */
	SIVAL(outbody.data, 0x54,
	      out_context_buffer.length);	/* create contexts length */

	outdyn = out_context_buffer;

	error = smbd_smb2_request_done(req, outbody, &outdyn);
	if (!NT_STATUS_IS_OK(error)) {
		smbd_server_connection_terminate(req->sconn,
						 nt_errstr(error));
		return;
	}
}

struct smbd_smb2_create_state {
	struct smbd_smb2_request *smb2req;
	uint8_t out_oplock_level;
	uint32_t out_create_action;
	NTTIME out_creation_time;
	NTTIME out_last_access_time;
	NTTIME out_last_write_time;
	NTTIME out_change_time;
	uint64_t out_allocation_size;
	uint64_t out_end_of_file;
	uint32_t out_file_attributes;
	uint64_t out_file_id_volatile;
	struct smb2_create_blobs out_context_blobs;
};

static struct tevent_req *smbd_smb2_create_send(TALLOC_CTX *mem_ctx,
			struct tevent_context *ev,
			struct smbd_smb2_request *smb2req,
			uint8_t in_oplock_level,
			uint32_t in_impersonation_level,
			uint32_t in_desired_access,
			uint32_t in_file_attributes,
			uint32_t in_share_access,
			uint32_t in_create_disposition,
			uint32_t in_create_options,
			const char *in_name,
			struct smb2_create_blobs in_context_blobs)
{
	struct tevent_req *req;
	struct smbd_smb2_create_state *state;
	NTSTATUS status;
	struct smb_request *smbreq;
	files_struct *result;
	int info;
	struct timespec write_time_ts;
	struct smb2_create_blobs out_context_blobs;

	ZERO_STRUCT(out_context_blobs);

	req = tevent_req_create(mem_ctx, &state,
				struct smbd_smb2_create_state);
	if (req == NULL) {
		return NULL;
	}
	state->smb2req = smb2req;

	DEBUG(10,("smbd_smb2_create: name[%s]\n",
		  in_name));

	smbreq = smbd_smb2_fake_smb_request(smb2req);
	if (tevent_req_nomem(smbreq, req)) {
		return tevent_req_post(req, ev);
	}

	if (IS_IPC(smbreq->conn)) {
		const char *pipe_name = in_name;

		if (!lp_nt_pipe_support()) {
			tevent_req_nterror(req, NT_STATUS_ACCESS_DENIED);
			return tevent_req_post(req, ev);
		}

		/* Strip \\ off the name. */
		if (pipe_name[0] == '\\') {
			pipe_name++;
		}

		status = open_np_file(smbreq, pipe_name, &result);
		if (!NT_STATUS_IS_OK(status)) {
			tevent_req_nterror(req, status);
			return tevent_req_post(req, ev);
		}
		info = FILE_WAS_OPENED;
	} else if (CAN_PRINT(smbreq->conn)) {
		status = file_new(smbreq, smbreq->conn, &result);
		if(!NT_STATUS_IS_OK(status)) {
			tevent_req_nterror(req, status);
			return tevent_req_post(req, ev);
		}

		status = print_fsp_open(smbreq,
					smbreq->conn,
					in_name,
					smbreq->vuid,
					result);
		if (!NT_STATUS_IS_OK(status)) {
			file_free(smbreq, result);
			tevent_req_nterror(req, status);
			return tevent_req_post(req, ev);
		}
		info = FILE_WAS_CREATED;
	} else {
		char *fname;
		struct smb_filename *smb_fname = NULL;
		struct smb2_create_blob *exta = NULL;
		struct ea_list *ea_list = NULL;
		struct smb2_create_blob *mxac = NULL;
		NTTIME max_access_time = 0;
		struct smb2_create_blob *secd = NULL;
		struct security_descriptor *sec_desc = NULL;
		struct smb2_create_blob *dhnq = NULL;
		struct smb2_create_blob *dhnc = NULL;
		struct smb2_create_blob *alsi = NULL;
		uint64_t allocation_size = 0;
		struct smb2_create_blob *twrp = NULL;
		struct smb2_create_blob *qfid = NULL;

		exta = smb2_create_blob_find(&in_context_blobs,
					     SMB2_CREATE_TAG_EXTA);
		mxac = smb2_create_blob_find(&in_context_blobs,
					     SMB2_CREATE_TAG_MXAC);
		secd = smb2_create_blob_find(&in_context_blobs,
					     SMB2_CREATE_TAG_SECD);
		dhnq = smb2_create_blob_find(&in_context_blobs,
					     SMB2_CREATE_TAG_DHNQ);
		dhnc = smb2_create_blob_find(&in_context_blobs,
					     SMB2_CREATE_TAG_DHNC);
		alsi = smb2_create_blob_find(&in_context_blobs,
					     SMB2_CREATE_TAG_ALSI);
		twrp = smb2_create_blob_find(&in_context_blobs,
					     SMB2_CREATE_TAG_TWRP);
		qfid = smb2_create_blob_find(&in_context_blobs,
					     SMB2_CREATE_TAG_QFID);

		fname = talloc_strdup(state, in_name);
		if (tevent_req_nomem(fname, req)) {
			return tevent_req_post(req, ev);
		}

		if (exta) {
			if (dhnc) {
				tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
				return tevent_req_post(req, ev);
			}

			ea_list = read_nttrans_ea_list(mem_ctx,
				(const char *)exta->data.data, exta->data.length);
			if (!ea_list) {
				DEBUG(10,("smbd_smb2_create_send: read_ea_name_list failed.\n"));
				tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
				return tevent_req_post(req, ev);
			}
		}

		if (mxac) {
			if (dhnc) {
				tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
				return tevent_req_post(req, ev);
			}

			if (mxac->data.length == 0) {
				max_access_time = 0;
			} else if (mxac->data.length == 8) {
				max_access_time = BVAL(mxac->data.data, 0);
			} else {
				tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
				return tevent_req_post(req, ev);
			}
		}

		if (secd) {
			enum ndr_err_code ndr_err;

			if (dhnc) {
				tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
				return tevent_req_post(req, ev);
			}

			sec_desc = talloc_zero(state, struct security_descriptor);
			if (tevent_req_nomem(sec_desc, req)) {
				return tevent_req_post(req, ev);
			}

			ndr_err = ndr_pull_struct_blob(&secd->data,
				sec_desc, NULL, sec_desc,
				(ndr_pull_flags_fn_t)ndr_pull_security_descriptor);
			if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
				DEBUG(2,("ndr_pull_security_descriptor failed: %s\n",
					 ndr_errstr(ndr_err)));
				tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
				return tevent_req_post(req, ev);
			}
		}

		if (dhnq) {
			if (dhnc) {
				tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
				return tevent_req_post(req, ev);
			}

			if (dhnq->data.length != 16) {
				tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
				return tevent_req_post(req, ev);
			}
			/*
			 * we don't support durable handles yet
			 * and have to ignore this
			 */
		}

		if (dhnc) {
			if (dhnc->data.length != 16) {
				tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
				return tevent_req_post(req, ev);
			}
			/* we don't support durable handles yet */
			tevent_req_nterror(req, NT_STATUS_OBJECT_NAME_NOT_FOUND);
			return tevent_req_post(req, ev);
		}

		if (alsi) {
			if (dhnc) {
				tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
				return tevent_req_post(req, ev);
			}

			if (alsi->data.length != 8) {
				tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
				return tevent_req_post(req, ev);
			}
			allocation_size = BVAL(alsi->data.data, 0);
		}

		if (twrp) {
			NTTIME nttime;
			time_t t;
			struct tm *tm;

			if (dhnc) {
				tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
				return tevent_req_post(req, ev);
			}

			if (twrp->data.length != 8) {
				tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
				return tevent_req_post(req, ev);
			}

			nttime = BVAL(twrp->data.data, 0);
			t = nt_time_to_unix(nttime);
			tm = gmtime(&t);

			TALLOC_FREE(fname);
			fname = talloc_asprintf(state,
					"@GMT-%04u.%02u.%02u-%02u.%02u.%02u\\%s",
					tm->tm_year + 1900,
					tm->tm_mon + 1,
					tm->tm_mday,
					tm->tm_hour,
					tm->tm_min,
					tm->tm_sec,
					in_name);
			if (tevent_req_nomem(fname, req)) {
				return tevent_req_post(req, ev);
			}
		}

		if (qfid) {
			if (qfid->data.length != 0) {
				tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
				return tevent_req_post(req, ev);
			}
		}

		/* these are ignored for SMB2 */
		in_create_options &= ~(0x10);/* NTCREATEX_OPTIONS_SYNC_ALERT */
		in_create_options &= ~(0x20);/* NTCREATEX_OPTIONS_ASYNC_ALERT */

		/* convert '\\' into '/' */
		status = check_path_syntax(fname);
		if (!NT_STATUS_IS_OK(status)) {
			tevent_req_nterror(req, status);
			return tevent_req_post(req, ev);
		}

		status = filename_convert(req,
					  smbreq->conn,
					  smbreq->flags2 & FLAGS2_DFS_PATHNAMES,
					  fname,
					  0,
					  NULL,
					  &smb_fname);
		if (!NT_STATUS_IS_OK(status)) {
			tevent_req_nterror(req, status);
			return tevent_req_post(req, ev);
		}

		status = SMB_VFS_CREATE_FILE(smbreq->conn,
					     smbreq,
					     0, /* root_dir_fid */
					     smb_fname,
					     in_desired_access,
					     in_share_access,
					     in_create_disposition,
					     in_create_options,
					     in_file_attributes,
					     0, /* oplock_request */
					     allocation_size,
					     sec_desc,
					     ea_list,
					     &result,
					     &info);
		if (!NT_STATUS_IS_OK(status)) {
			tevent_req_nterror(req, status);
			return tevent_req_post(req, ev);
		}

		if (mxac) {
			NTTIME last_write_time;

			unix_timespec_to_nt_time(&last_write_time,
						 result->fsp_name->st.st_ex_mtime);
			if (last_write_time != max_access_time) {
				uint8_t p[8];
				uint32_t max_access_granted;
				DATA_BLOB blob = data_blob_const(p, sizeof(p));

				status = smbd_check_open_rights(smbreq->conn,
							result->fsp_name,
							SEC_FLAG_MAXIMUM_ALLOWED,
							&max_access_granted);

				SIVAL(p, 0, NT_STATUS_V(status));
				SIVAL(p, 4, max_access_granted);

				status = smb2_create_blob_add(state,
							&out_context_blobs,
							SMB2_CREATE_TAG_MXAC,
							blob);
				if (!NT_STATUS_IS_OK(status)) {
					tevent_req_nterror(req, status);
					return tevent_req_post(req, ev);
				}
			}
		}

		if (qfid) {
			uint8_t p[32];
			uint64_t file_index = get_FileIndex(result->conn,
							&result->fsp_name->st);
			DATA_BLOB blob = data_blob_const(p, sizeof(p));

			ZERO_STRUCT(p);

			/* From conversations with Microsoft engineers at
			   the MS plugfest. The first 8 bytes are the "volume index"
			   == inode, the second 8 bytes are the "volume id",
			   == dev. This will be updated in the SMB2 doc. */
			SBVAL(p, 0, file_index);
			SIVAL(p, 8, result->fsp_name->st.st_ex_dev);/* FileIndexHigh */

			status = smb2_create_blob_add(state, &out_context_blobs,
						      SMB2_CREATE_TAG_QFID,
						      blob);
			if (!NT_STATUS_IS_OK(status)) {
				tevent_req_nterror(req, status);
				return tevent_req_post(req, ev);
			}
		}
	}

	smb2req->compat_chain_fsp = smbreq->chain_fsp;

	state->out_oplock_level	= 0;
	if ((in_create_disposition == FILE_SUPERSEDE)
	    && (info == FILE_WAS_OVERWRITTEN)) {
		state->out_create_action = FILE_WAS_SUPERSEDED;
	} else {
		state->out_create_action = info;
	}
	state->out_file_attributes = dos_mode(result->conn,
					   result->fsp_name);
	/* Deal with other possible opens having a modified
	   write time. JRA. */
	ZERO_STRUCT(write_time_ts);
	get_file_infos(result->file_id, NULL, &write_time_ts);
	if (!null_timespec(write_time_ts)) {
		update_stat_ex_mtime(&result->fsp_name->st, write_time_ts);
	}

	unix_timespec_to_nt_time(&state->out_creation_time,
			get_create_timespec(smbreq->conn, result,
					result->fsp_name));
	unix_timespec_to_nt_time(&state->out_last_access_time,
			result->fsp_name->st.st_ex_atime);
	unix_timespec_to_nt_time(&state->out_last_write_time,
			result->fsp_name->st.st_ex_mtime);
	unix_timespec_to_nt_time(&state->out_change_time,
			get_change_timespec(smbreq->conn, result,
					result->fsp_name));
	state->out_allocation_size =
			result->fsp_name->st.st_ex_blksize *
			result->fsp_name->st.st_ex_blocks;
	state->out_end_of_file = result->fsp_name->st.st_ex_size;
	if (state->out_file_attributes == 0) {
		state->out_file_attributes = FILE_ATTRIBUTE_NORMAL;
	}
	state->out_file_id_volatile = result->fnum;
	state->out_context_blobs = out_context_blobs;

	tevent_req_done(req);
	return tevent_req_post(req, ev);
}

static NTSTATUS smbd_smb2_create_recv(struct tevent_req *req,
			TALLOC_CTX *mem_ctx,
			uint8_t *out_oplock_level,
			uint32_t *out_create_action,
			NTTIME *out_creation_time,
			NTTIME *out_last_access_time,
			NTTIME *out_last_write_time,
			NTTIME *out_change_time,
			uint64_t *out_allocation_size,
			uint64_t *out_end_of_file,
			uint32_t *out_file_attributes,
			uint64_t *out_file_id_volatile,
			struct smb2_create_blobs *out_context_blobs)
{
	NTSTATUS status;
	struct smbd_smb2_create_state *state = tevent_req_data(req,
					       struct smbd_smb2_create_state);

	if (tevent_req_is_nterror(req, &status)) {
		tevent_req_received(req);
		return status;
	}

	*out_oplock_level	= state->out_oplock_level;
	*out_create_action	= state->out_create_action;
	*out_creation_time	= state->out_creation_time;
	*out_last_access_time	= state->out_last_access_time;
	*out_last_write_time	= state->out_last_write_time;
	*out_change_time	= state->out_change_time;
	*out_allocation_size	= state->out_allocation_size;
	*out_end_of_file	= state->out_end_of_file;
	*out_file_attributes	= state->out_file_attributes;
	*out_file_id_volatile	= state->out_file_id_volatile;
	*out_context_blobs	= state->out_context_blobs;

	talloc_steal(mem_ctx, state->out_context_blobs.blobs);

	tevent_req_received(req);
	return NT_STATUS_OK;
}
