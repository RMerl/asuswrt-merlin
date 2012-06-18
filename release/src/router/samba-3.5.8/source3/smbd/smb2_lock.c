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

struct smbd_smb2_lock_element {
	uint64_t offset;
	uint64_t length;
	uint32_t flags;
};

static struct tevent_req *smbd_smb2_lock_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct smbd_smb2_request *smb2req,
						 uint32_t in_smbpid,
						 uint64_t in_file_id_volatile,
						 uint16_t in_lock_count,
						 struct smbd_smb2_lock_element *in_locks);
static NTSTATUS smbd_smb2_lock_recv(struct tevent_req *req);

static void smbd_smb2_request_lock_done(struct tevent_req *subreq);
NTSTATUS smbd_smb2_request_process_lock(struct smbd_smb2_request *req)
{
	const uint8_t *inhdr;
	const uint8_t *inbody;
	const int i = req->current_idx;
	size_t expected_body_size = 0x30;
	size_t body_size;
	uint32_t in_smbpid;
	uint16_t in_lock_count;
	uint64_t in_file_id_persistent;
	uint64_t in_file_id_volatile;
	struct smbd_smb2_lock_element *in_locks;
	struct tevent_req *subreq;
	const uint8_t *lock_buffer;
	uint16_t l;

	inhdr = (const uint8_t *)req->in.vector[i+0].iov_base;
	if (req->in.vector[i+1].iov_len != (expected_body_size & 0xFFFFFFFE)) {
		return smbd_smb2_request_error(req, NT_STATUS_INVALID_PARAMETER);
	}

	inbody = (const uint8_t *)req->in.vector[i+1].iov_base;

	body_size = SVAL(inbody, 0x00);
	if (body_size != expected_body_size) {
		return smbd_smb2_request_error(req, NT_STATUS_INVALID_PARAMETER);
	}

	in_smbpid			= IVAL(inhdr, SMB2_HDR_PID);

	in_lock_count			= CVAL(inbody, 0x02);
	/* 0x04 - 4 bytes reserved */
	in_file_id_persistent		= BVAL(inbody, 0x08);
	in_file_id_volatile		= BVAL(inbody, 0x10);

	if (in_lock_count < 1) {
		return smbd_smb2_request_error(req, NT_STATUS_INVALID_PARAMETER);
	}

	if (((in_lock_count - 1) * 0x18) > req->in.vector[i+2].iov_len) {
		return smbd_smb2_request_error(req, NT_STATUS_INVALID_PARAMETER);
	}

	if (req->compat_chain_fsp) {
		/* skip check */
	} else if (in_file_id_persistent != 0) {
		return smbd_smb2_request_error(req, NT_STATUS_FILE_CLOSED);
	}

	in_locks = talloc_array(req, struct smbd_smb2_lock_element,
				in_lock_count);
	if (in_locks == NULL) {
		return smbd_smb2_request_error(req, NT_STATUS_NO_MEMORY);
	}

	l = 0;
	lock_buffer = inbody + 0x18;

	in_locks[l].offset	= BVAL(lock_buffer, 0x00);
	in_locks[l].length	= BVAL(lock_buffer, 0x08);
	in_locks[l].flags	= IVAL(lock_buffer, 0x10);
	/* 0x14 - 4 reserved bytes */

	lock_buffer = (const uint8_t *)req->in.vector[i+2].iov_base;

	for (l=1; l < in_lock_count; l++) {
		in_locks[l].offset	= BVAL(lock_buffer, 0x00);
		in_locks[l].length	= BVAL(lock_buffer, 0x08);
		in_locks[l].flags	= IVAL(lock_buffer, 0x10);
		/* 0x14 - 4 reserved bytes */

		lock_buffer += 0x18;
	}

	subreq = smbd_smb2_lock_send(req,
				     req->sconn->smb2.event_ctx,
				     req,
				     in_smbpid,
				     in_file_id_volatile,
				     in_lock_count,
				     in_locks);
	if (subreq == NULL) {
		return smbd_smb2_request_error(req, NT_STATUS_NO_MEMORY);
	}
	tevent_req_set_callback(subreq, smbd_smb2_request_lock_done, req);

	return smbd_smb2_request_pending_queue(req, subreq);
}

static void smbd_smb2_request_lock_done(struct tevent_req *subreq)
{
	struct smbd_smb2_request *req = tevent_req_callback_data(subreq,
					struct smbd_smb2_request);
	DATA_BLOB outbody;
	NTSTATUS status;
	NTSTATUS error; /* transport error */

	status = smbd_smb2_lock_recv(subreq);
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

	outbody = data_blob_talloc(req->out.vector, NULL, 0x04);
	if (outbody.data == NULL) {
		error = smbd_smb2_request_error(req, NT_STATUS_NO_MEMORY);
		if (!NT_STATUS_IS_OK(error)) {
			smbd_server_connection_terminate(req->sconn,
							 nt_errstr(error));
			return;
		}
		return;
	}

	SSVAL(outbody.data, 0x00, 0x04);	/* struct size */
	SSVAL(outbody.data, 0x02, 0);		/* reserved */

	error = smbd_smb2_request_done(req, outbody, NULL);
	if (!NT_STATUS_IS_OK(error)) {
		smbd_server_connection_terminate(req->sconn,
						 nt_errstr(error));
		return;
	}
}

struct smbd_smb2_lock_state {
	struct smbd_smb2_request *smb2req;
};

static struct tevent_req *smbd_smb2_lock_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct smbd_smb2_request *smb2req,
						 uint32_t in_smbpid,
						 uint64_t in_file_id_volatile,
						 uint16_t in_lock_count,
						 struct smbd_smb2_lock_element *in_locks)
{
	struct tevent_req *req;
	struct smbd_smb2_lock_state *state;
	struct smb_request *smbreq;
	connection_struct *conn = smb2req->tcon->compat_conn;
	files_struct *fsp;
	int32_t timeout = -1;
	bool isunlock = false;
	uint16_t i;
	struct smbd_lock_element *locks;
	NTSTATUS status;
	bool async = false;

	req = tevent_req_create(mem_ctx, &state,
				struct smbd_smb2_lock_state);
	if (req == NULL) {
		return NULL;
	}
	state->smb2req = smb2req;

	DEBUG(10,("smbd_smb2_lock_send: file_id[0x%016llX]\n",
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

	locks = talloc_array(state, struct smbd_lock_element, in_lock_count);
	if (locks == NULL) {
		tevent_req_nterror(req, NT_STATUS_NO_MEMORY);
		return tevent_req_post(req, ev);
	}

	switch (in_locks[0].flags) {
	case SMB2_LOCK_FLAG_SHARED:
	case SMB2_LOCK_FLAG_EXCLUSIVE:
		if (in_lock_count > 1) {
			tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
			return tevent_req_post(req, ev);
		}
		timeout = -1;
		break;

	case SMB2_LOCK_FLAG_SHARED|SMB2_LOCK_FLAG_FAIL_IMMEDIATELY:
	case SMB2_LOCK_FLAG_EXCLUSIVE|SMB2_LOCK_FLAG_FAIL_IMMEDIATELY:
		timeout = 0;
		break;

	case SMB2_LOCK_FLAG_UNLOCK:
		/* only the first lock gives the UNLOCK bit - see
		   MS-SMB2 3.3.5.14 */
		isunlock = true;
		timeout = 0;
		break;

	default:
		tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return tevent_req_post(req, ev);
	}

	for (i=0; i<in_lock_count; i++) {
		uint64_t max_count;
		bool invalid = false;

		switch (in_locks[i].flags) {
		case SMB2_LOCK_FLAG_SHARED:
		case SMB2_LOCK_FLAG_EXCLUSIVE:
			if (i > 0) {
				tevent_req_nterror(req,
						   NT_STATUS_INVALID_PARAMETER);
				return tevent_req_post(req, ev);
			}
			if (isunlock) {
				tevent_req_nterror(req,
						   NT_STATUS_INVALID_PARAMETER);
				return tevent_req_post(req, ev);
			}
			break;

		case SMB2_LOCK_FLAG_SHARED|SMB2_LOCK_FLAG_FAIL_IMMEDIATELY:
		case SMB2_LOCK_FLAG_EXCLUSIVE|SMB2_LOCK_FLAG_FAIL_IMMEDIATELY:
			if (isunlock) {
				tevent_req_nterror(req,
						   NT_STATUS_INVALID_PARAMETER);
				return tevent_req_post(req, ev);
			}
			break;

		case SMB2_LOCK_FLAG_UNLOCK:
			if (!isunlock) {
				tevent_req_nterror(req,
						   NT_STATUS_INVALID_PARAMETER);
				return tevent_req_post(req, ev);
			}
			break;

		default:
			if (isunlock) {
				/*
				 * is the first element was a UNLOCK
				 * we need to deferr the error response
				 * to the backend, because we need to process
				 * all unlock elements before
				 */
				invalid = true;
				break;
			}
			tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
			return tevent_req_post(req, ev);
		}

		locks[i].smbpid = in_smbpid;
		locks[i].offset = in_locks[i].offset;
		locks[i].count  = in_locks[i].length;

		if (in_locks[i].flags & SMB2_LOCK_FLAG_EXCLUSIVE) {
			locks[i].brltype = WRITE_LOCK;
		} else if (in_locks[i].flags & SMB2_LOCK_FLAG_SHARED) {
			locks[i].brltype = READ_LOCK;
		} else if (invalid) {
			/*
			 * this is an invalid UNLOCK element
			 * and the backend needs to test for
			 * brltype != UNLOCK_LOCK and return
			 * NT_STATUS_INVALID_PARAMER
			 */
			locks[i].brltype = READ_LOCK;
		} else {
			locks[i].brltype = UNLOCK_LOCK;
		}

		max_count = UINT64_MAX - locks[i].offset;
		if (locks[i].count > max_count) {
			tevent_req_nterror(req, NT_STATUS_INVALID_LOCK_RANGE);
			return tevent_req_post(req, ev);
		}
	}

	if (isunlock) {
		status = smbd_do_locking(smbreq, fsp,
					 0,
					 timeout,
					 in_lock_count,
					 locks,
					 0,
					 NULL,
					 &async);
	} else {
		status = smbd_do_locking(smbreq, fsp,
					 0,
					 timeout,
					 0,
					 NULL,
					 in_lock_count,
					 locks,
					 &async);
	}
	if (!NT_STATUS_IS_OK(status)) {
		if (NT_STATUS_EQUAL(status, NT_STATUS_FILE_LOCK_CONFLICT)) {
		       status = NT_STATUS_LOCK_NOT_GRANTED;
		}
		tevent_req_nterror(req, status);
		return tevent_req_post(req, ev);
	}

	if (async) {
		tevent_req_nterror(req, NT_STATUS_INTERNAL_ERROR);
		return tevent_req_post(req, ev);
	}

	tevent_req_done(req);
	return tevent_req_post(req, ev);
}

static NTSTATUS smbd_smb2_lock_recv(struct tevent_req *req)
{
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		tevent_req_received(req);
		return status;
	}

	tevent_req_received(req);
	return NT_STATUS_OK;
}
