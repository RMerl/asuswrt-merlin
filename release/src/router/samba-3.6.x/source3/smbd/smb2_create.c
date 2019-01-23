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
#include "printing.h"
#include "smbd/smbd.h"
#include "smbd/globals.h"
#include "../libcli/smb/smb_common.h"
#include "../librpc/gen_ndr/ndr_security.h"
#include "../lib/util/tevent_ntstatus.h"

int map_smb2_oplock_levels_to_samba(uint8_t in_oplock_level)
{
	switch(in_oplock_level) {
	case SMB2_OPLOCK_LEVEL_NONE:
		return NO_OPLOCK;
	case SMB2_OPLOCK_LEVEL_II:
		return LEVEL_II_OPLOCK;
	case SMB2_OPLOCK_LEVEL_EXCLUSIVE:
		return EXCLUSIVE_OPLOCK;
	case SMB2_OPLOCK_LEVEL_BATCH:
		return BATCH_OPLOCK;
	case SMB2_OPLOCK_LEVEL_LEASE:
		DEBUG(2,("map_smb2_oplock_levels_to_samba: "
			"LEASE_OPLOCK_REQUESTED\n"));
		return NO_OPLOCK;
	default:
		DEBUG(2,("map_smb2_oplock_levels_to_samba: "
			"unknown level %u\n",
			(unsigned int)in_oplock_level));
		return NO_OPLOCK;
	}
}

static uint8_t map_samba_oplock_levels_to_smb2(int oplock_type)
{
	if (BATCH_OPLOCK_TYPE(oplock_type)) {
		return SMB2_OPLOCK_LEVEL_BATCH;
	} else if (EXCLUSIVE_OPLOCK_TYPE(oplock_type)) {
		return SMB2_OPLOCK_LEVEL_EXCLUSIVE;
	} else if (oplock_type == LEVEL_II_OPLOCK) {
		/*
		 * Don't use LEVEL_II_OPLOCK_TYPE here as
		 * this also includes FAKE_LEVEL_II_OPLOCKs
		 * which are internal only.
		 */
		return SMB2_OPLOCK_LEVEL_II;
	} else {
		return SMB2_OPLOCK_LEVEL_NONE;
	}
}

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
			uint64_t *out_file_id_persistent,
			uint64_t *out_file_id_volatile,
			struct smb2_create_blobs *out_context_blobs);

static void smbd_smb2_request_create_done(struct tevent_req *tsubreq);
NTSTATUS smbd_smb2_request_process_create(struct smbd_smb2_request *smb2req)
{
	const uint8_t *inbody;
	int i = smb2req->current_idx;
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
	struct tevent_req *tsubreq;

	status = smbd_smb2_request_verify_sizes(smb2req, 0x39);
	if (!NT_STATUS_IS_OK(status)) {
		return smbd_smb2_request_error(smb2req, status);
	}
	inbody = (const uint8_t *)smb2req->in.vector[i+1].iov_base;

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

	dyn_offset = SMB2_HDR_BODY + smb2req->in.vector[i+1].iov_len;

	if (in_name_offset == 0 && in_name_length == 0) {
		/* This is ok */
		name_offset = 0;
	} else if (in_name_offset < dyn_offset) {
		return smbd_smb2_request_error(smb2req, NT_STATUS_INVALID_PARAMETER);
	} else {
		name_offset = in_name_offset - dyn_offset;
	}

	if (name_offset > smb2req->in.vector[i+2].iov_len) {
		return smbd_smb2_request_error(smb2req, NT_STATUS_INVALID_PARAMETER);
	}

	name_available_length = smb2req->in.vector[i+2].iov_len - name_offset;

	if (in_name_length > name_available_length) {
		return smbd_smb2_request_error(smb2req, NT_STATUS_INVALID_PARAMETER);
	}

	in_name_buffer.data = (uint8_t *)smb2req->in.vector[i+2].iov_base +
			      name_offset;
	in_name_buffer.length = in_name_length;

	if (in_context_offset == 0 && in_context_length == 0) {
		/* This is ok */
		context_offset = 0;
	} else if (in_context_offset < dyn_offset) {
		return smbd_smb2_request_error(smb2req, NT_STATUS_INVALID_PARAMETER);
	} else {
		context_offset = in_context_offset - dyn_offset;
	}

	if (context_offset > smb2req->in.vector[i+2].iov_len) {
		return smbd_smb2_request_error(smb2req, NT_STATUS_INVALID_PARAMETER);
	}

	context_available_length = smb2req->in.vector[i+2].iov_len - context_offset;

	if (in_context_length > context_available_length) {
		return smbd_smb2_request_error(smb2req, NT_STATUS_INVALID_PARAMETER);
	}

	in_context_buffer.data = (uint8_t *)smb2req->in.vector[i+2].iov_base +
				  context_offset;
	in_context_buffer.length = in_context_length;

	/*
	 * Now interpret the name and context buffers
	 */

	ok = convert_string_talloc(smb2req, CH_UTF16, CH_UNIX,
				   in_name_buffer.data,
				   in_name_buffer.length,
				   &in_name_string,
				   &in_name_string_size, false);
	if (!ok) {
		return smbd_smb2_request_error(smb2req, NT_STATUS_ILLEGAL_CHARACTER);
	}

	if (in_name_buffer.length == 0) {
		in_name_string_size = 0;
	}

	if (strlen(in_name_string) != in_name_string_size) {
		return smbd_smb2_request_error(smb2req, NT_STATUS_OBJECT_NAME_INVALID);
	}

	ZERO_STRUCT(in_context_blobs);
	status = smb2_create_blob_parse(smb2req, in_context_buffer, &in_context_blobs);
	if (!NT_STATUS_IS_OK(status)) {
		return smbd_smb2_request_error(smb2req, status);
	}

	tsubreq = smbd_smb2_create_send(smb2req,
				       smb2req->sconn->smb2.event_ctx,
				       smb2req,
				       in_oplock_level,
				       in_impersonation_level,
				       in_desired_access,
				       in_file_attributes,
				       in_share_access,
				       in_create_disposition,
				       in_create_options,
				       in_name_string,
				       in_context_blobs);
	if (tsubreq == NULL) {
		smb2req->subreq = NULL;
		return smbd_smb2_request_error(smb2req, NT_STATUS_NO_MEMORY);
	}
	tevent_req_set_callback(tsubreq, smbd_smb2_request_create_done, smb2req);

	return smbd_smb2_request_pending_queue(smb2req, tsubreq);
}

static uint64_t get_mid_from_smb2req(struct smbd_smb2_request *smb2req)
{
	uint8_t *reqhdr = (uint8_t *)smb2req->out.vector[smb2req->current_idx].iov_base;
	return BVAL(reqhdr, SMB2_HDR_MESSAGE_ID);
}

static void smbd_smb2_request_create_done(struct tevent_req *tsubreq)
{
	struct smbd_smb2_request *smb2req = tevent_req_callback_data(tsubreq,
					struct smbd_smb2_request);
	int i = smb2req->current_idx;
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
	uint64_t out_file_id_persistent = 0;
	uint64_t out_file_id_volatile = 0;
	struct smb2_create_blobs out_context_blobs;
	DATA_BLOB out_context_buffer;
	uint16_t out_context_buffer_offset = 0;
	NTSTATUS status;
	NTSTATUS error; /* transport error */

	if (smb2req->cancelled) {
		uint64_t mid = get_mid_from_smb2req(smb2req);
		DEBUG(10,("smbd_smb2_request_create_done: cancelled mid %llu\n",
			(unsigned long long)mid ));
		error = smbd_smb2_request_error(smb2req, NT_STATUS_CANCELLED);
		if (!NT_STATUS_IS_OK(error)) {
			smbd_server_connection_terminate(smb2req->sconn,
				nt_errstr(error));
			return;
		}
		return;
	}

	status = smbd_smb2_create_recv(tsubreq,
				       smb2req,
				       &out_oplock_level,
				       &out_create_action,
				       &out_creation_time,
				       &out_last_access_time,
				       &out_last_write_time,
				       &out_change_time,
				       &out_allocation_size,
				       &out_end_of_file,
				       &out_file_attributes,
				       &out_file_id_persistent,
				       &out_file_id_volatile,
				       &out_context_blobs);
	if (!NT_STATUS_IS_OK(status)) {
		error = smbd_smb2_request_error(smb2req, status);
		if (!NT_STATUS_IS_OK(error)) {
			smbd_server_connection_terminate(smb2req->sconn,
							 nt_errstr(error));
			return;
		}
		return;
	}

	status = smb2_create_blob_push(smb2req, &out_context_buffer, out_context_blobs);
	if (!NT_STATUS_IS_OK(status)) {
		error = smbd_smb2_request_error(smb2req, status);
		if (!NT_STATUS_IS_OK(error)) {
			smbd_server_connection_terminate(smb2req->sconn,
							 nt_errstr(error));
			return;
		}
		return;
	}

	if (out_context_buffer.length > 0) {
		out_context_buffer_offset = SMB2_HDR_BODY + 0x58;
	}

	outhdr = (uint8_t *)smb2req->out.vector[i].iov_base;

	outbody = data_blob_talloc(smb2req->out.vector, NULL, 0x58);
	if (outbody.data == NULL) {
		error = smbd_smb2_request_error(smb2req, NT_STATUS_NO_MEMORY);
		if (!NT_STATUS_IS_OK(error)) {
			smbd_server_connection_terminate(smb2req->sconn,
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
	SBVAL(outbody.data, 0x40,
	      out_file_id_persistent);		/* file id (persistent) */
	SBVAL(outbody.data, 0x48,
	      out_file_id_volatile);		/* file id (volatile) */
	SIVAL(outbody.data, 0x50,
	      out_context_buffer_offset);	/* create contexts offset */
	SIVAL(outbody.data, 0x54,
	      out_context_buffer.length);	/* create contexts length */

	outdyn = out_context_buffer;

	error = smbd_smb2_request_done(smb2req, outbody, &outdyn);
	if (!NT_STATUS_IS_OK(error)) {
		smbd_server_connection_terminate(smb2req->sconn,
						 nt_errstr(error));
		return;
	}
}

struct smbd_smb2_create_state {
	struct smbd_smb2_request *smb2req;
	struct smb_request *smb1req;
	bool open_was_deferred;
	struct timed_event *te;
	struct tevent_immediate *im;
	struct timeval request_time;
	struct file_id id;
	DATA_BLOB private_data;
	uint8_t out_oplock_level;
	uint32_t out_create_action;
	NTTIME out_creation_time;
	NTTIME out_last_access_time;
	NTTIME out_last_write_time;
	NTTIME out_change_time;
	uint64_t out_allocation_size;
	uint64_t out_end_of_file;
	uint32_t out_file_attributes;
	uint64_t out_file_id_persistent;
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
	struct tevent_req *req = NULL;
	struct smbd_smb2_create_state *state = NULL;
	NTSTATUS status;
	struct smb_request *smb1req = NULL;
	files_struct *result = NULL;
	int info;
	struct timespec write_time_ts;
	struct smb2_create_blobs out_context_blobs;
	int requested_oplock_level;

	ZERO_STRUCT(out_context_blobs);

	if(lp_fake_oplocks(SNUM(smb2req->tcon->compat_conn))) {
		requested_oplock_level = SMB2_OPLOCK_LEVEL_NONE;
	} else {
		requested_oplock_level = in_oplock_level;
	}


	if (!smb2req->async) {
		/* New create call. */
		req = tevent_req_create(mem_ctx, &state,
				struct smbd_smb2_create_state);
		if (req == NULL) {
			return NULL;
		}
		state->smb2req = smb2req;
		smb2req->subreq = req; /* So we can find this when going async. */

		smb1req = smbd_smb2_fake_smb_request(smb2req);
		if (tevent_req_nomem(smb1req, req)) {
			return tevent_req_post(req, ev);
		}
		state->smb1req = smb1req;
		DEBUG(10,("smbd_smb2_create: name[%s]\n",
			in_name));
	} else {
		/* Re-entrant create call. */
		req = smb2req->subreq;
		state = tevent_req_data(req,
				struct smbd_smb2_create_state);
		smb1req = state->smb1req;
		DEBUG(10,("smbd_smb2_create_send: reentrant for file %s\n",
			in_name ));
	}

	if (IS_IPC(smb1req->conn)) {
		const char *pipe_name = in_name;

		if (!lp_nt_pipe_support()) {
			tevent_req_nterror(req, NT_STATUS_ACCESS_DENIED);
			return tevent_req_post(req, ev);
		}

		/* Strip \\ off the name. */
		if (pipe_name[0] == '\\') {
			pipe_name++;
		}

		status = open_np_file(smb1req, pipe_name, &result);
		if (!NT_STATUS_IS_OK(status)) {
			tevent_req_nterror(req, status);
			return tevent_req_post(req, ev);
		}
		info = FILE_WAS_OPENED;
	} else if (CAN_PRINT(smb1req->conn)) {
		status = file_new(smb1req, smb1req->conn, &result);
#ifdef PRINTER_SUPPORT
		if(!NT_STATUS_IS_OK(status))
#endif
		{
			tevent_req_nterror(req, status);
			return tevent_req_post(req, ev);
		}

		status = print_spool_open(result, in_name,
					  smb1req->vuid);
		if (!NT_STATUS_IS_OK(status)) {
			file_free(smb1req, result);
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
				tevent_req_nterror(req,NT_STATUS_OBJECT_NAME_NOT_FOUND);
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
				tevent_req_nterror(req, NT_STATUS_OBJECT_NAME_NOT_FOUND);
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
				tevent_req_nterror(req, NT_STATUS_OBJECT_NAME_NOT_FOUND);
				return tevent_req_post(req, ev);
			}

			sec_desc = talloc_zero(state, struct security_descriptor);
			if (tevent_req_nomem(sec_desc, req)) {
				return tevent_req_post(req, ev);
			}

			ndr_err = ndr_pull_struct_blob(&secd->data,
				sec_desc, sec_desc,
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
				tevent_req_nterror(req, NT_STATUS_OBJECT_NAME_NOT_FOUND);
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
				tevent_req_nterror(req, NT_STATUS_OBJECT_NAME_NOT_FOUND);
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
				tevent_req_nterror(req, NT_STATUS_OBJECT_NAME_NOT_FOUND);
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

                /*
		 * For a DFS path the function parse_dfs_path()
		 * will do the path processing.
		 */

		if (!(smb1req->flags2 & FLAGS2_DFS_PATHNAMES)) {
			/* convert '\\' into '/' */
			status = check_path_syntax(fname);
			if (!NT_STATUS_IS_OK(status)) {
				tevent_req_nterror(req, status);
				return tevent_req_post(req, ev);
			}
		}

		status = filename_convert(req,
					  smb1req->conn,
					  smb1req->flags2 & FLAGS2_DFS_PATHNAMES,
					  fname,
					  (in_create_disposition == FILE_CREATE) ?
						UCF_CREATING_FILE : 0,
					  NULL,
					  &smb_fname);
		if (!NT_STATUS_IS_OK(status)) {
			tevent_req_nterror(req, status);
			return tevent_req_post(req, ev);
		}

		in_file_attributes &= ~FILE_FLAG_POSIX_SEMANTICS;

		status = SMB_VFS_CREATE_FILE(smb1req->conn,
					     smb1req,
					     0, /* root_dir_fid */
					     smb_fname,
					     in_desired_access,
					     in_share_access,
					     in_create_disposition,
					     in_create_options,
					     in_file_attributes,
					     map_smb2_oplock_levels_to_samba(requested_oplock_level),
					     allocation_size,
					     0, /* private_flags */
					     sec_desc,
					     ea_list,
					     &result,
					     &info);
		if (!NT_STATUS_IS_OK(status)) {
			if (open_was_deferred(smb1req->mid)) {
				return req;
			}
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

				status = smbd_calculate_access_mask(smb1req->conn,
							result->fsp_name,
							/*
							 * at this stage
							 * it exists
							 */
							true,
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

	smb2req->compat_chain_fsp = smb1req->chain_fsp;

	if(lp_fake_oplocks(SNUM(smb2req->tcon->compat_conn))) {
		state->out_oplock_level	= in_oplock_level;
	} else {
		state->out_oplock_level	= map_samba_oplock_levels_to_smb2(result->oplock_type);
	}

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
	get_file_infos(result->file_id, 0, NULL, &write_time_ts);
	if (!null_timespec(write_time_ts)) {
		update_stat_ex_mtime(&result->fsp_name->st, write_time_ts);
	}

	unix_timespec_to_nt_time(&state->out_creation_time,
			get_create_timespec(smb1req->conn, result,
					result->fsp_name));
	unix_timespec_to_nt_time(&state->out_last_access_time,
			result->fsp_name->st.st_ex_atime);
	unix_timespec_to_nt_time(&state->out_last_write_time,
			result->fsp_name->st.st_ex_mtime);
	unix_timespec_to_nt_time(&state->out_change_time,
			get_change_timespec(smb1req->conn, result,
					result->fsp_name));
	state->out_allocation_size =
			SMB_VFS_GET_ALLOC_SIZE(smb1req->conn, result,
					       &(result->fsp_name->st));
	state->out_end_of_file = result->fsp_name->st.st_ex_size;
	if (state->out_file_attributes == 0) {
		state->out_file_attributes = FILE_ATTRIBUTE_NORMAL;
	}
	state->out_file_id_persistent = fsp_persistent_id(result);
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
			uint64_t *out_file_id_persistent,
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
	*out_file_id_persistent	= state->out_file_id_persistent;
	*out_file_id_volatile	= state->out_file_id_volatile;
	*out_context_blobs	= state->out_context_blobs;

	talloc_steal(mem_ctx, state->out_context_blobs.blobs);

	tevent_req_received(req);
	return NT_STATUS_OK;
}

/*********************************************************
 Code for dealing with deferred opens.
*********************************************************/

bool get_deferred_open_message_state_smb2(struct smbd_smb2_request *smb2req,
			struct timeval *p_request_time,
			void **pp_state)
{
	struct smbd_smb2_create_state *state = NULL;
	struct tevent_req *req = NULL;

	if (!smb2req) {
		return false;
	}
	req = smb2req->subreq;
	if (!req) {
		return false;
	}
	state = tevent_req_data(req, struct smbd_smb2_create_state);
	if (!state) {
		return false;
	}
	if (!state->open_was_deferred) {
		return false;
	}
	if (p_request_time) {
		*p_request_time = state->request_time;
	}
	if (pp_state) {
		*pp_state = (void *)state->private_data.data;
	}
	return true;
}

/*********************************************************
 Re-process this call early - requested by message or
 close.
*********************************************************/

static struct smbd_smb2_request *find_open_smb2req(
	struct smbd_server_connection *sconn, uint64_t mid)
{
	struct smbd_smb2_request *smb2req;

	for (smb2req = sconn->smb2.requests; smb2req; smb2req = smb2req->next) {
		uint64_t message_id;
		if (smb2req->subreq == NULL) {
			/* This message has been processed. */
			continue;
		}
		if (!tevent_req_is_in_progress(smb2req->subreq)) {
			/* This message has been processed. */
			continue;
		}
		message_id = get_mid_from_smb2req(smb2req);
		if (message_id == mid) {
			return smb2req;
		}
	}
	return NULL;
}

bool open_was_deferred_smb2(struct smbd_server_connection *sconn, uint64_t mid)
{
	struct smbd_smb2_create_state *state = NULL;
	struct smbd_smb2_request *smb2req;

	smb2req = find_open_smb2req(sconn, mid);

	if (!smb2req) {
		DEBUG(10,("open_was_deferred_smb2: mid %llu smb2req == NULL\n",
			(unsigned long long)mid));
		return false;
	}
	if (!smb2req->subreq) {
		return false;
	}
	if (!tevent_req_is_in_progress(smb2req->subreq)) {
		return false;
	}
	state = tevent_req_data(smb2req->subreq,
			struct smbd_smb2_create_state);
	if (!state) {
		return false;
	}
	/* It's not in progress if there's no timeout event. */
	if (!state->open_was_deferred) {
		return false;
	}

	DEBUG(10,("open_was_deferred_smb2: mid = %llu\n",
			(unsigned long long)mid));

	return true;
}

static void remove_deferred_open_message_smb2_internal(struct smbd_smb2_request *smb2req,
							uint64_t mid)
{
	struct smbd_smb2_create_state *state = NULL;

	if (!smb2req->subreq) {
		return;
	}
	if (!tevent_req_is_in_progress(smb2req->subreq)) {
		return;
	}
	state = tevent_req_data(smb2req->subreq,
			struct smbd_smb2_create_state);
	if (!state) {
		return;
	}

	DEBUG(10,("remove_deferred_open_message_smb2_internal: "
		"mid %llu\n",
		(unsigned long long)mid ));

	state->open_was_deferred = false;
	/* Ensure we don't have any outstanding timer event. */
	TALLOC_FREE(state->te);
	/* Ensure we don't have any outstanding immediate event. */
	TALLOC_FREE(state->im);
}

void remove_deferred_open_message_smb2(
	struct smbd_server_connection *sconn, uint64_t mid)
{
	struct smbd_smb2_request *smb2req;

	smb2req = find_open_smb2req(sconn, mid);

	if (!smb2req) {
		DEBUG(10,("remove_deferred_open_message_smb2: "
			"can't find mid %llu\n",
			(unsigned long long)mid ));
		return;
	}
	remove_deferred_open_message_smb2_internal(smb2req, mid);
}

static void smbd_smb2_create_request_dispatch_immediate(struct tevent_context *ctx,
					struct tevent_immediate *im,
					void *private_data)
{
	struct smbd_smb2_request *smb2req = talloc_get_type_abort(private_data,
					struct smbd_smb2_request);
	struct smbd_server_connection *sconn = smb2req->sconn;
	uint64_t mid = get_mid_from_smb2req(smb2req);
	NTSTATUS status;

	DEBUG(10,("smbd_smb2_create_request_dispatch_immediate: "
		"re-dispatching mid %llu\n",
		(unsigned long long)mid ));

	status = smbd_smb2_request_dispatch(smb2req);
	if (!NT_STATUS_IS_OK(status)) {
		smbd_server_connection_terminate(sconn, nt_errstr(status));
		return;
	}
}

void schedule_deferred_open_message_smb2(
	struct smbd_server_connection *sconn, uint64_t mid)
{
	struct smbd_smb2_create_state *state = NULL;
	struct smbd_smb2_request *smb2req;

	smb2req = find_open_smb2req(sconn, mid);

	if (!smb2req) {
		DEBUG(10,("schedule_deferred_open_message_smb2: "
			"can't find mid %llu\n",
			(unsigned long long)mid ));
		return;
	}
	if (!smb2req->subreq) {
		return;
	}
	if (!tevent_req_is_in_progress(smb2req->subreq)) {
		return;
	}
	state = tevent_req_data(smb2req->subreq,
			struct smbd_smb2_create_state);
	if (!state) {
		return;
	}

	/* Ensure we don't have any outstanding timer event. */
	TALLOC_FREE(state->te);
	/* Ensure we don't have any outstanding immediate event. */
	TALLOC_FREE(state->im);

	/*
	 * This is subtle. We must null out the callback
	 * before resheduling, else the first call to
	 * tevent_req_nterror() causes the _receive()
	 * function to be called, this causing tevent_req_post()
	 * to crash.
	 */
	tevent_req_set_callback(smb2req->subreq, NULL, NULL);

	state->im = tevent_create_immediate(smb2req);
	if (!state->im) {
		smbd_server_connection_terminate(smb2req->sconn,
			nt_errstr(NT_STATUS_NO_MEMORY));
	}

	DEBUG(10,("schedule_deferred_open_message_smb2: "
		"re-processing mid %llu\n",
		(unsigned long long)mid ));

	tevent_schedule_immediate(state->im,
			smb2req->sconn->smb2.event_ctx,
			smbd_smb2_create_request_dispatch_immediate,
			smb2req);
}

/*********************************************************
 Re-process this call.
*********************************************************/

static void smb2_deferred_open_timer(struct event_context *ev,
					struct timed_event *te,
					struct timeval _tval,
					void *private_data)
{
	NTSTATUS status;
	struct smbd_smb2_create_state *state = NULL;
	struct smbd_smb2_request *smb2req = talloc_get_type(private_data,
						struct smbd_smb2_request);

	DEBUG(10,("smb2_deferred_open_timer: [idx=%d], %s\n",
		smb2req->current_idx,
		tevent_req_default_print(smb2req->subreq, talloc_tos()) ));

	state = tevent_req_data(smb2req->subreq,
			struct smbd_smb2_create_state);
	if (!state) {
		return;
	}
	/*
	 * Null this out, don't talloc_free. It will
	 * be talloc_free'd by the tevent library when
	 * this returns.
	 */
	state->te = NULL;
	/* Ensure we don't have any outstanding immediate event. */
	TALLOC_FREE(state->im);

	/*
	 * This is subtle. We must null out the callback
	 * before resheduling, else the first call to
	 * tevent_req_nterror() causes the _receive()
	 * function to be called, this causing tevent_req_post()
	 * to crash.
	 */
	tevent_req_set_callback(smb2req->subreq, NULL, NULL);

	status = smbd_smb2_request_dispatch(smb2req);

	if (!NT_STATUS_IS_OK(status)) {
		smbd_server_connection_terminate(smb2req->sconn,
				nt_errstr(status));
	}
}

static bool smbd_smb2_create_cancel(struct tevent_req *req)
{
	struct smbd_smb2_request *smb2req = NULL;
	struct smbd_smb2_create_state *state = tevent_req_data(req,
				struct smbd_smb2_create_state);
	uint64_t mid;

	if (!state) {
		return false;
	}

	if (!state->smb2req) {
		return false;
	}

	smb2req = state->smb2req;
	mid = get_mid_from_smb2req(smb2req);

	remove_deferred_open_entry(state->id, mid,
				   sconn_server_id(smb2req->sconn));
	remove_deferred_open_message_smb2_internal(smb2req, mid);
	smb2req->cancelled = true;

	tevent_req_done(req);
	return true;
}

bool push_deferred_open_message_smb2(struct smbd_smb2_request *smb2req,
                                struct timeval request_time,
                                struct timeval timeout,
				struct file_id id,
                                char *private_data,
                                size_t priv_len)
{
	struct tevent_req *req = NULL;
	struct smbd_smb2_create_state *state = NULL;
	struct timeval end_time;

	if (!smb2req) {
		return false;
	}
	req = smb2req->subreq;
	if (!req) {
		return false;
	}
	state = tevent_req_data(req, struct smbd_smb2_create_state);
	if (!state) {
		return false;
	}
	state->id = id;
	state->request_time = request_time;
	state->private_data = data_blob_talloc(state, private_data,
						priv_len);
	if (!state->private_data.data) {
		return false;
	}

#if 1
	/* Boo - turns out this isn't what W2K8R2
	   does. It actually sends the STATUS_PENDING
	   message followed by the STATUS_SHARING_VIOLATION
	   message. Surely this means that all open
	   calls (even on directories) will potentially
	   fail in a chain.... ? And I've seen directory
	   opens as the start of a chain. JRA.

	   Update: 19th May 2010. Talking with Microsoft
	   engineers at the plugfest this is a bug in
	   Windows. Re-enable this code.
	*/
	/*
	 * More subtlety. To match W2K8R2 don't
	 * send a "gone async" message if it's simply
	 * a STATUS_SHARING_VIOLATION (short) wait, not
	 * an oplock break wait. We do this by prematurely
	 * setting smb2req->async flag.
	 */
	if (timeout.tv_sec < 2) {
		DEBUG(10,("push_deferred_open_message_smb2: "
			"short timer wait (usec = %u). "
			"Don't send async message.\n",
			(unsigned int)timeout.tv_usec ));
		smb2req->async = true;
	}
#endif

	/* Re-schedule us to retry on timer expiry. */
	end_time = timeval_sum(&request_time, &timeout);

	DEBUG(10,("push_deferred_open_message_smb2: "
		"timeout at %s\n",
		timeval_string(talloc_tos(),
				&end_time,
				true) ));

	state->open_was_deferred = true;
	state->te = event_add_timed(smb2req->sconn->smb2.event_ctx,
				state,
				end_time,
				smb2_deferred_open_timer,
				smb2req);
        if (!state->te) {
		return false;
	}

	/* allow this request to be canceled */
	tevent_req_set_cancel_fn(req, smbd_smb2_create_cancel);

	return true;
}
