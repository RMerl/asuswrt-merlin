/* 
   Unix SMB2 implementation.
   
   Copyright (C) Stefan Metzmacher            2005
   
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

/* the context for a single SMB2 request. This is passed to any request-context 
   functions */
struct smb2srv_request {
	/* the smbsrv_connection needs a list of requests queued for send */
	struct smb2srv_request *next, *prev;

	/* the server_context contains all context specific to this SMB socket */
	struct smbsrv_connection *smb_conn;

	/* conn is only set for operations that have a valid TID */
	struct smbsrv_tcon *tcon;

	/* the session context is derived from the vuid */
	struct smbsrv_session *session;

#define SMB2SRV_REQ_CTRL_FLAG_NOT_REPLY (1<<0)
	uint32_t control_flags;

	/* the system time when the request arrived */
	struct timeval request_time;

	/* a pointer to the per request union smb_* io structure */
	void *io_ptr;

	/* the ntvfs_request */
	struct ntvfs_request *ntvfs;

	/* Now the SMB2 specific stuff */

	/* the status the backend returned */
	NTSTATUS status;

	/* for matching request and reply */
	uint64_t seqnum;

	/* the id that can be used to cancel the request */
	uint32_t pending_id;

	/* the offset to the next SMB2 Header for chained requests */
	uint32_t chain_offset;

	/* the status we return for following chained requests */
	NTSTATUS chain_status;

	/* chained file handle */
	uint8_t _chained_file_handle[16];
	uint8_t *chained_file_handle;

	bool is_signed;

	struct smb2_request_buffer in;
	struct smb2_request_buffer out;
};

struct smbsrv_request;

#include "smb_server/smb2/smb2_proto.h"

/* useful way of catching field size errors with file and line number */
#define SMB2SRV_CHECK_BODY_SIZE(req, size, dynamic) do { \
	size_t is_size = req->in.body_size; \
	uint16_t field_size; \
	uint16_t want_size = ((dynamic)?(size)+1:(size)); \
	if (is_size < (size)) { \
		DEBUG(0,("%s: buffer too small 0x%x. Expected 0x%x\n", \
			 __location__, (unsigned)is_size, (unsigned)want_size)); \
		smb2srv_send_error(req,  NT_STATUS_INVALID_PARAMETER); \
		return; \
	}\
	field_size = SVAL(req->in.body, 0);       \
	if (field_size != want_size) { \
		DEBUG(0,("%s: unexpected fixed body size 0x%x. Expected 0x%x\n", \
			 __location__, (unsigned)field_size, (unsigned)want_size)); \
		smb2srv_send_error(req,  NT_STATUS_INVALID_PARAMETER); \
		return; \
	} \
} while (0)

#define SMB2SRV_CHECK(cmd) do {\
	NTSTATUS _status; \
	_status = cmd; \
	if (!NT_STATUS_IS_OK(_status)) { \
		smb2srv_send_error(req,  _status); \
		return; \
	} \
} while (0)

/* useful wrapper for talloc with NO_MEMORY reply */
#define SMB2SRV_TALLOC_IO_PTR(ptr, type) do { \
	ptr = talloc(req, type); \
	if (!ptr) { \
		smb2srv_send_error(req, NT_STATUS_NO_MEMORY); \
		return; \
	} \
	req->io_ptr = ptr; \
} while (0)

#define SMB2SRV_SETUP_NTVFS_REQUEST(send_fn, state) do { \
	req->ntvfs = ntvfs_request_create(req->tcon->ntvfs, req, \
					  req->session->session_info,\
					  0, \
					  req->request_time, \
					  req, send_fn, state); \
	if (!req->ntvfs) { \
		smb2srv_send_error(req, NT_STATUS_NO_MEMORY); \
		return; \
	} \
	(void)talloc_steal(req->tcon->ntvfs, req); \
	req->ntvfs->frontend_data.private_data = req; \
} while (0)

#define SMB2SRV_CHECK_FILE_HANDLE(handle) do { \
	if (!handle) { \
		smb2srv_send_error(req, NT_STATUS_FILE_CLOSED); \
		return; \
	} \
} while (0)

/* 
   check if the backend wants to handle the request asynchronously.
   if it wants it handled synchronously then call the send function
   immediately
*/
#define SMB2SRV_CALL_NTVFS_BACKEND(cmd) do { \
	req->ntvfs->async_states->status = cmd; \
	if (req->ntvfs->async_states->state & NTVFS_ASYNC_STATE_ASYNC) { \
		NTSTATUS _status; \
		_status = smb2srv_queue_pending(req); \
		if (!NT_STATUS_IS_OK(_status)) { \
			ntvfs_cancel(req->ntvfs); \
		} \
	} else { \
		req->ntvfs->async_states->send_fn(req->ntvfs); \
	} \
} while (0)

/* check req->ntvfs->async_states->status and if not OK then send an error reply */
#define SMB2SRV_CHECK_ASYNC_STATUS_ERR_SIMPLE do { \
	req = talloc_get_type(ntvfs->async_states->private_data, struct smb2srv_request); \
	if (ntvfs->async_states->state & NTVFS_ASYNC_STATE_CLOSE || NT_STATUS_EQUAL(ntvfs->async_states->status, NT_STATUS_NET_WRITE_FAULT)) { \
		smbsrv_terminate_connection(req->smb_conn, get_friendly_nt_error_msg (ntvfs->async_states->status)); \
		talloc_free(req); \
		return; \
	} \
	req->status = ntvfs->async_states->status; \
	if (NT_STATUS_IS_ERR(ntvfs->async_states->status)) { \
		smb2srv_send_error(req, ntvfs->async_states->status); \
		return; \
	} \
} while (0)
#define SMB2SRV_CHECK_ASYNC_STATUS_ERR(ptr, type) do { \
	SMB2SRV_CHECK_ASYNC_STATUS_ERR_SIMPLE; \
	ptr = talloc_get_type(req->io_ptr, type); \
} while (0)
#define SMB2SRV_CHECK_ASYNC_STATUS_SIMPLE do { \
	req = talloc_get_type(ntvfs->async_states->private_data, struct smb2srv_request); \
	if (ntvfs->async_states->state & NTVFS_ASYNC_STATE_CLOSE || NT_STATUS_EQUAL(ntvfs->async_states->status, NT_STATUS_NET_WRITE_FAULT)) { \
		smbsrv_terminate_connection(req->smb_conn, get_friendly_nt_error_msg (ntvfs->async_states->status)); \
		talloc_free(req); \
		return; \
	} \
	req->status = ntvfs->async_states->status; \
	if (!NT_STATUS_IS_OK(ntvfs->async_states->status)) { \
		smb2srv_send_error(req, ntvfs->async_states->status); \
		return; \
	} \
} while (0)
#define SMB2SRV_CHECK_ASYNC_STATUS(ptr, type) do { \
	SMB2SRV_CHECK_ASYNC_STATUS_SIMPLE; \
	ptr = talloc_get_type(req->io_ptr, type); \
} while (0)
