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
#include "../lib/tsocket/tsocket.h"

bool smbd_is_smb2_header(const uint8_t *inbuf, size_t size)
{
	if (size < (4 + SMB2_HDR_BODY)) {
		return false;
	}

	if (IVAL(inbuf, 4) != SMB2_MAGIC) {
		return false;
	}

	return true;
}

static NTSTATUS smbd_initialize_smb2(struct smbd_server_connection *sconn)
{
	NTSTATUS status;
	int ret;

	TALLOC_FREE(sconn->smb1.fde);

	sconn->smb2.event_ctx = smbd_event_context();

	sconn->smb2.recv_queue = tevent_queue_create(sconn, "smb2 recv queue");
	if (sconn->smb2.recv_queue == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	sconn->smb2.send_queue = tevent_queue_create(sconn, "smb2 send queue");
	if (sconn->smb2.send_queue == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	sconn->smb2.sessions.idtree = idr_init(sconn);
	if (sconn->smb2.sessions.idtree == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	sconn->smb2.sessions.limit = 0x0000FFFE;
	sconn->smb2.sessions.list = NULL;

	ret = tstream_bsd_existing_socket(sconn, smbd_server_fd(),
					  &sconn->smb2.stream);
	if (ret == -1) {
		status = map_nt_error_from_unix(errno);
		return status;
	}

	/* Ensure child is set to non-blocking mode */
	set_blocking(smbd_server_fd(),false);
	return NT_STATUS_OK;
}

#define smb2_len(buf) (PVAL(buf,3)|(PVAL(buf,2)<<8)|(PVAL(buf,1)<<16))
#define _smb2_setlen(_buf,len) do { \
	uint8_t *buf = (uint8_t *)_buf; \
	buf[0] = 0; \
	buf[1] = ((len)&0xFF0000)>>16; \
	buf[2] = ((len)&0xFF00)>>8; \
	buf[3] = (len)&0xFF; \
} while (0)

static void smb2_setup_nbt_length(struct iovec *vector, int count)
{
	size_t len = 0;
	int i;

	for (i=1; i < count; i++) {
		len += vector[i].iov_len;
	}

	_smb2_setlen(vector[0].iov_base, len);
}

static int smbd_smb2_request_parent_destructor(struct smbd_smb2_request **req)
{
	if (*req) {
		(*req)->parent = NULL;
		(*req)->mem_pool = NULL;
	}

	return 0;
}

static int smbd_smb2_request_destructor(struct smbd_smb2_request *req)
{
	if (req->out.vector) {
		DLIST_REMOVE(req->sconn->smb2.requests, req);
	}

	if (req->parent) {
		*req->parent = NULL;
		talloc_free(req->mem_pool);
	}

	return 0;
}

static struct smbd_smb2_request *smbd_smb2_request_allocate(TALLOC_CTX *mem_ctx)
{
	TALLOC_CTX *mem_pool;
	struct smbd_smb2_request **parent;
	struct smbd_smb2_request *req;

	mem_pool = talloc_pool(mem_ctx, 8192);
	if (mem_pool == NULL) {
		return NULL;
	}

	parent = talloc(mem_pool, struct smbd_smb2_request *);
	if (parent == NULL) {
		talloc_free(mem_pool);
		return NULL;
	}

	req = talloc_zero(parent, struct smbd_smb2_request);
	if (req == NULL) {
		talloc_free(mem_pool);
		return NULL;
	}
	*parent		= req;
	req->mem_pool	= mem_pool;
	req->parent	= parent;

	talloc_set_destructor(parent, smbd_smb2_request_parent_destructor);
	talloc_set_destructor(req, smbd_smb2_request_destructor);

	return req;
}

static NTSTATUS smbd_smb2_request_create(struct smbd_server_connection *sconn,
					 const uint8_t *inbuf, size_t size,
					 struct smbd_smb2_request **_req)
{
	struct smbd_smb2_request *req;
	uint32_t protocol_version;
	const uint8_t *inhdr = NULL;
	off_t ofs = 0;
	uint16_t cmd;
	uint32_t next_command_ofs;

	if (size < (4 + SMB2_HDR_BODY + 2)) {
		DEBUG(0,("Invalid SMB2 packet length count %ld\n", (long)size));
		return NT_STATUS_INVALID_PARAMETER;
	}

	inhdr = inbuf + 4;

	protocol_version = IVAL(inhdr, SMB2_HDR_PROTOCOL_ID);
	if (protocol_version != SMB2_MAGIC) {
		DEBUG(0,("Invalid SMB packet: protocol prefix: 0x%08X\n",
			 protocol_version));
		return NT_STATUS_INVALID_PARAMETER;
	}

	cmd = SVAL(inhdr, SMB2_HDR_OPCODE);
	if (cmd != SMB2_OP_NEGPROT) {
		DEBUG(0,("Invalid SMB packet: first request: 0x%04X\n",
			 cmd));
		return NT_STATUS_INVALID_PARAMETER;
	}

	next_command_ofs = IVAL(inhdr, SMB2_HDR_NEXT_COMMAND);
	if (next_command_ofs != 0) {
		DEBUG(0,("Invalid SMB packet: next_command: 0x%08X\n",
			 next_command_ofs));
		return NT_STATUS_INVALID_PARAMETER;
	}

	req = smbd_smb2_request_allocate(sconn);
	if (req == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	req->sconn = sconn;

	talloc_steal(req, inbuf);

	req->in.vector = talloc_array(req, struct iovec, 4);
	if (req->in.vector == NULL) {
		TALLOC_FREE(req);
		return NT_STATUS_NO_MEMORY;
	}
	req->in.vector_count = 4;

	memcpy(req->in.nbt_hdr, inbuf, 4);

	ofs = 0;
	req->in.vector[0].iov_base	= (void *)req->in.nbt_hdr;
	req->in.vector[0].iov_len	= 4;
	ofs += req->in.vector[0].iov_len;

	req->in.vector[1].iov_base	= (void *)(inbuf + ofs);
	req->in.vector[1].iov_len	= SMB2_HDR_BODY;
	ofs += req->in.vector[1].iov_len;

	req->in.vector[2].iov_base	= (void *)(inbuf + ofs);
	req->in.vector[2].iov_len	= SVAL(inbuf, ofs) & 0xFFFE;
	ofs += req->in.vector[2].iov_len;

	if (ofs > size) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	req->in.vector[3].iov_base	= (void *)(inbuf + ofs);
	req->in.vector[3].iov_len	= size - ofs;
	ofs += req->in.vector[3].iov_len;

	req->current_idx = 1;

	*_req = req;
	return NT_STATUS_OK;
}

static NTSTATUS smbd_smb2_request_validate(struct smbd_smb2_request *req)
{
	int count;
	int idx;
	bool compound_related = false;

	count = req->in.vector_count;

	if (count < 4) {
		/* It's not a SMB2 request */
		return NT_STATUS_INVALID_PARAMETER;
	}

	for (idx=1; idx < count; idx += 3) {
		const uint8_t *inhdr = NULL;
		uint32_t flags;

		if (req->in.vector[idx].iov_len != SMB2_HDR_BODY) {
			return NT_STATUS_INVALID_PARAMETER;
		}

		if (req->in.vector[idx+1].iov_len < 2) {
			return NT_STATUS_INVALID_PARAMETER;
		}

		inhdr = (const uint8_t *)req->in.vector[idx].iov_base;

		/* setup the SMB2 header */
		if (IVAL(inhdr, SMB2_HDR_PROTOCOL_ID) != SMB2_MAGIC) {
			return NT_STATUS_INVALID_PARAMETER;
		}

		flags = IVAL(inhdr, SMB2_HDR_FLAGS);
		if (idx == 1) {
			/*
			 * the 1st request should never have the
			 * SMB2_HDR_FLAG_CHAINED flag set
			 */
			if (flags & SMB2_HDR_FLAG_CHAINED) {
				req->next_status = NT_STATUS_INVALID_PARAMETER;
				return NT_STATUS_OK;
			}
		} else if (idx == 4) {
			/*
			 * the 2nd request triggers related vs. unrelated
			 * compounded requests
			 */
			if (flags & SMB2_HDR_FLAG_CHAINED) {
				compound_related = true;
			}
		} else if (idx > 4) {
#if 0
			/*
			 * It seems the this tests are wrong
			 * see the SMB2-COMPOUND test
			 */

			/*
			 * all other requests should match the 2nd one
			 */
			if (flags & SMB2_HDR_FLAG_CHAINED) {
				if (!compound_related) {
					req->next_status =
						NT_STATUS_INVALID_PARAMETER;
					return NT_STATUS_OK;
				}
			} else {
				if (compound_related) {
					req->next_status =
						NT_STATUS_INVALID_PARAMETER;
					return NT_STATUS_OK;
				}
			}
#endif
		}
	}

	return NT_STATUS_OK;
}

static NTSTATUS smbd_smb2_request_setup_out(struct smbd_smb2_request *req)
{
	struct iovec *vector;
	int count;
	int idx;

	count = req->in.vector_count;
	vector = talloc_array(req, struct iovec, count);
	if (vector == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	vector[0].iov_base	= req->out.nbt_hdr;
	vector[0].iov_len	= 4;
	SIVAL(req->out.nbt_hdr, 0, 0);

	for (idx=1; idx < count; idx += 3) {
		const uint8_t *inhdr = NULL;
		uint32_t in_flags;
		uint8_t *outhdr = NULL;
		uint8_t *outbody = NULL;
		uint32_t next_command_ofs = 0;
		struct iovec *current = &vector[idx];

		if ((idx + 3) < count) {
			/* we have a next command */
			next_command_ofs = SMB2_HDR_BODY + 8;
		}

		inhdr = (const uint8_t *)req->in.vector[idx].iov_base;
		in_flags = IVAL(inhdr, SMB2_HDR_FLAGS);

		outhdr = talloc_array(vector, uint8_t,
				      SMB2_HDR_BODY + 8);
		if (outhdr == NULL) {
			return NT_STATUS_NO_MEMORY;
		}

		outbody = outhdr + SMB2_HDR_BODY;

		current[0].iov_base	= (void *)outhdr;
		current[0].iov_len	= SMB2_HDR_BODY;

		current[1].iov_base	= (void *)outbody;
		current[1].iov_len	= 8;

		current[2].iov_base	= NULL;
		current[2].iov_len	= 0;

		/* setup the SMB2 header */
		SIVAL(outhdr, SMB2_HDR_PROTOCOL_ID,	SMB2_MAGIC);
		SSVAL(outhdr, SMB2_HDR_LENGTH,		SMB2_HDR_BODY);
		SSVAL(outhdr, SMB2_HDR_EPOCH,		0);
		SIVAL(outhdr, SMB2_HDR_STATUS,
		      NT_STATUS_V(NT_STATUS_INTERNAL_ERROR));
		SSVAL(outhdr, SMB2_HDR_OPCODE,
		      SVAL(inhdr, SMB2_HDR_OPCODE));
		/* Make up a number for now... JRA. FIXME ! FIXME !*/
		SSVAL(outhdr, SMB2_HDR_CREDIT,		20);
		SIVAL(outhdr, SMB2_HDR_FLAGS,
		      IVAL(inhdr, SMB2_HDR_FLAGS) | SMB2_HDR_FLAG_REDIRECT);
		SIVAL(outhdr, SMB2_HDR_NEXT_COMMAND,	next_command_ofs);
		SBVAL(outhdr, SMB2_HDR_MESSAGE_ID,
		      BVAL(inhdr, SMB2_HDR_MESSAGE_ID));
		SIVAL(outhdr, SMB2_HDR_PID,
		      IVAL(inhdr, SMB2_HDR_PID));
		SIVAL(outhdr, SMB2_HDR_TID,
		      IVAL(inhdr, SMB2_HDR_TID));
		SBVAL(outhdr, SMB2_HDR_SESSION_ID,
		      BVAL(inhdr, SMB2_HDR_SESSION_ID));
		memset(outhdr + SMB2_HDR_SIGNATURE, 0, 16);

		/* setup error body header */
		SSVAL(outbody, 0x00, 0x08 + 1);
		SSVAL(outbody, 0x02, 0);
		SIVAL(outbody, 0x04, 0);
	}

	req->out.vector = vector;
	req->out.vector_count = count;

	/* setup the length of the NBT packet */
	smb2_setup_nbt_length(req->out.vector, req->out.vector_count);

	DLIST_ADD_END(req->sconn->smb2.requests, req, struct smbd_smb2_request *);

	return NT_STATUS_OK;
}

void smbd_server_connection_terminate_ex(struct smbd_server_connection *sconn,
					 const char *reason,
					 const char *location)
{
	DEBUG(10,("smbd_server_connection_terminate_ex: reason[%s] at %s\n",
		  reason, location));
	exit_server_cleanly(reason);
}

struct smbd_smb2_request_pending_state {
	struct smbd_server_connection *sconn;
	uint8_t buf[4 + SMB2_HDR_BODY + 0x08];
	struct iovec vector;
};

static void smbd_smb2_request_pending_writev_done(struct tevent_req *subreq);

NTSTATUS smbd_smb2_request_pending_queue(struct smbd_smb2_request *req,
					 struct tevent_req *subreq)
{
	struct smbd_smb2_request_pending_state *state;
	uint8_t *outhdr;
	int i = req->current_idx;
	uint32_t flags;
	uint64_t message_id;
	uint64_t async_id;
	uint8_t *hdr;
	uint8_t *body;

	if (!tevent_req_is_in_progress(subreq)) {
		return NT_STATUS_OK;
	}

	req->subreq = subreq;
	subreq = NULL;

	outhdr = (uint8_t *)req->out.vector[i].iov_base;

	flags = IVAL(outhdr, SMB2_HDR_FLAGS);
	message_id = BVAL(outhdr, SMB2_HDR_MESSAGE_ID);

	async_id = message_id; /* keep it simple for now... */
	SIVAL(outhdr, SMB2_HDR_FLAGS,	flags | SMB2_HDR_FLAG_ASYNC);
	SBVAL(outhdr, SMB2_HDR_PID,	async_id);

	/* TODO: add a paramter to delay this */
	state = talloc(req->sconn, struct smbd_smb2_request_pending_state);
	if (state == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	state->sconn = req->sconn;

	state->vector.iov_base = (void *)state->buf;
	state->vector.iov_len = sizeof(state->buf);

	_smb2_setlen(state->buf, sizeof(state->buf) - 4);
	hdr = state->buf + 4;
	body = hdr + SMB2_HDR_BODY;

	SIVAL(hdr, SMB2_HDR_PROTOCOL_ID,	SMB2_MAGIC);
	SSVAL(hdr, SMB2_HDR_LENGTH,		SMB2_HDR_BODY);
	SSVAL(hdr, SMB2_HDR_EPOCH,		0);
	SIVAL(hdr, SMB2_HDR_STATUS,		NT_STATUS_V(STATUS_PENDING));
	SSVAL(hdr, SMB2_HDR_OPCODE,
	      SVAL(outhdr, SMB2_HDR_OPCODE));
	SSVAL(hdr, SMB2_HDR_CREDIT,		1);
	SIVAL(hdr, SMB2_HDR_FLAGS,
	      IVAL(outhdr, SMB2_HDR_FLAGS));
	SIVAL(hdr, SMB2_HDR_NEXT_COMMAND,	0);
	SBVAL(hdr, SMB2_HDR_MESSAGE_ID,
	      BVAL(outhdr, SMB2_HDR_MESSAGE_ID));
	SBVAL(hdr, SMB2_HDR_PID,
	      BVAL(outhdr, SMB2_HDR_PID));
	SBVAL(hdr, SMB2_HDR_SESSION_ID,
	      BVAL(outhdr, SMB2_HDR_SESSION_ID));
	memset(hdr+SMB2_HDR_SIGNATURE, 0, 16);

	SSVAL(body, 0x00, 0x08 + 1);

	SCVAL(body, 0x02, 0);
	SCVAL(body, 0x03, 0);
	SIVAL(body, 0x04, 0);

	subreq = tstream_writev_queue_send(state,
					   req->sconn->smb2.event_ctx,
					   req->sconn->smb2.stream,
					   req->sconn->smb2.send_queue,
					   &state->vector, 1);
	if (subreq == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	tevent_req_set_callback(subreq,
				smbd_smb2_request_pending_writev_done,
				state);

	return NT_STATUS_OK;
}

static void smbd_smb2_request_pending_writev_done(struct tevent_req *subreq)
{
	struct smbd_smb2_request_pending_state *state =
		tevent_req_callback_data(subreq,
		struct smbd_smb2_request_pending_state);
	struct smbd_server_connection *sconn = state->sconn;
	int ret;
	int sys_errno;

	ret = tstream_writev_queue_recv(subreq, &sys_errno);
	TALLOC_FREE(subreq);
	if (ret == -1) {
		NTSTATUS status = map_nt_error_from_unix(sys_errno);
		smbd_server_connection_terminate(sconn, nt_errstr(status));
		return;
	}

	TALLOC_FREE(state);
}

static NTSTATUS smbd_smb2_request_process_cancel(struct smbd_smb2_request *req)
{
	struct smbd_server_connection *sconn = req->sconn;
	struct smbd_smb2_request *cur;
	const uint8_t *inhdr;
	int i = req->current_idx;
	uint32_t flags;
	uint64_t search_message_id;
	uint64_t search_async_id;

	inhdr = (const uint8_t *)req->in.vector[i].iov_base;

	flags = IVAL(inhdr, SMB2_HDR_FLAGS);
	search_message_id = BVAL(inhdr, SMB2_HDR_MESSAGE_ID);
	search_async_id = BVAL(inhdr, SMB2_HDR_PID);

	/*
	 * we don't need the request anymore
	 * cancel requests never have a response
	 */
	TALLOC_FREE(req);

	for (cur = sconn->smb2.requests; cur; cur = cur->next) {
		const uint8_t *outhdr;
		uint64_t message_id;
		uint64_t async_id;

		i = cur->current_idx;

		outhdr = (const uint8_t *)cur->out.vector[i].iov_base;

		message_id = BVAL(outhdr, SMB2_HDR_MESSAGE_ID);
		async_id = BVAL(outhdr, SMB2_HDR_PID);

		if (flags & SMB2_HDR_FLAG_ASYNC) {
			if (search_async_id == async_id) {
				break;
			}
		} else {
			if (search_message_id == message_id) {
				break;
			}
		}
	}

	if (cur && cur->subreq) {
		tevent_req_cancel(cur->subreq);
	}

	return NT_STATUS_OK;
}

static NTSTATUS smbd_smb2_request_dispatch(struct smbd_smb2_request *req)
{
	const uint8_t *inhdr;
	int i = req->current_idx;
	uint16_t opcode;
	uint32_t flags;
	NTSTATUS status;
	NTSTATUS session_status;
	uint32_t allowed_flags;

	inhdr = (const uint8_t *)req->in.vector[i].iov_base;

	/* TODO: verify more things */

	flags = IVAL(inhdr, SMB2_HDR_FLAGS);
	opcode = IVAL(inhdr, SMB2_HDR_OPCODE);
	DEBUG(10,("smbd_smb2_request_dispatch: opcode[%u]\n", opcode));

	allowed_flags = SMB2_HDR_FLAG_CHAINED |
			SMB2_HDR_FLAG_SIGNED |
			SMB2_HDR_FLAG_DFS;
	if (opcode == SMB2_OP_CANCEL) {
		allowed_flags |= SMB2_HDR_FLAG_ASYNC;
	}
	if ((flags & ~allowed_flags) != 0) {
		return smbd_smb2_request_error(req, NT_STATUS_INVALID_PARAMETER);
	}

	session_status = smbd_smb2_request_check_session(req);

	req->do_signing = false;
	if (flags & SMB2_HDR_FLAG_SIGNED) {
		if (!NT_STATUS_IS_OK(session_status)) {
			return smbd_smb2_request_error(req, session_status);
		}

		req->do_signing = true;
		status = smb2_signing_check_pdu(req->session->session_key,
						&req->in.vector[i], 3);
		if (!NT_STATUS_IS_OK(status)) {
			return smbd_smb2_request_error(req, status);
		}
	} else if (req->session && req->session->do_signing) {
		return smbd_smb2_request_error(req, NT_STATUS_ACCESS_DENIED);
	}

	if (flags & SMB2_HDR_FLAG_CHAINED) {
		/*
		 * This check is mostly for giving the correct error code
		 * for compounded requests.
		 *
		 * TODO: we may need to move this after the session
		 *       and tcon checks.
		 */
		if (!NT_STATUS_IS_OK(req->next_status)) {
			return smbd_smb2_request_error(req, req->next_status);
		}
	} else {
		req->compat_chain_fsp = NULL;
	}

	switch (opcode) {
	case SMB2_OP_NEGPROT:
		return smbd_smb2_request_process_negprot(req);

	case SMB2_OP_SESSSETUP:
		return smbd_smb2_request_process_sesssetup(req);

	case SMB2_OP_LOGOFF:
		if (!NT_STATUS_IS_OK(session_status)) {
			return smbd_smb2_request_error(req, session_status);
		}
		return smbd_smb2_request_process_logoff(req);

	case SMB2_OP_TCON:
		if (!NT_STATUS_IS_OK(session_status)) {
			return smbd_smb2_request_error(req, session_status);
		}
		status = smbd_smb2_request_check_session(req);
		if (!NT_STATUS_IS_OK(status)) {
			return smbd_smb2_request_error(req, status);
		}
		return smbd_smb2_request_process_tcon(req);

	case SMB2_OP_TDIS:
		if (!NT_STATUS_IS_OK(session_status)) {
			return smbd_smb2_request_error(req, session_status);
		}
		status = smbd_smb2_request_check_tcon(req);
		if (!NT_STATUS_IS_OK(status)) {
			return smbd_smb2_request_error(req, status);
		}
		return smbd_smb2_request_process_tdis(req);

	case SMB2_OP_CREATE:
		if (!NT_STATUS_IS_OK(session_status)) {
			return smbd_smb2_request_error(req, session_status);
		}
		status = smbd_smb2_request_check_tcon(req);
		if (!NT_STATUS_IS_OK(status)) {
			return smbd_smb2_request_error(req, status);
		}
		return smbd_smb2_request_process_create(req);

	case SMB2_OP_CLOSE:
		if (!NT_STATUS_IS_OK(session_status)) {
			return smbd_smb2_request_error(req, session_status);
		}
		status = smbd_smb2_request_check_tcon(req);
		if (!NT_STATUS_IS_OK(status)) {
			return smbd_smb2_request_error(req, status);
		}
		return smbd_smb2_request_process_close(req);

	case SMB2_OP_FLUSH:
		if (!NT_STATUS_IS_OK(session_status)) {
			return smbd_smb2_request_error(req, session_status);
		}
		status = smbd_smb2_request_check_tcon(req);
		if (!NT_STATUS_IS_OK(status)) {
			return smbd_smb2_request_error(req, status);
		}
		return smbd_smb2_request_process_flush(req);

	case SMB2_OP_READ:
		if (!NT_STATUS_IS_OK(session_status)) {
			return smbd_smb2_request_error(req, session_status);
		}
		status = smbd_smb2_request_check_tcon(req);
		if (!NT_STATUS_IS_OK(status)) {
			return smbd_smb2_request_error(req, status);
		}
		return smbd_smb2_request_process_read(req);

	case SMB2_OP_WRITE:
		if (!NT_STATUS_IS_OK(session_status)) {
			return smbd_smb2_request_error(req, session_status);
		}
		status = smbd_smb2_request_check_tcon(req);
		if (!NT_STATUS_IS_OK(status)) {
			return smbd_smb2_request_error(req, status);
		}
		return smbd_smb2_request_process_write(req);

	case SMB2_OP_LOCK:
		if (!NT_STATUS_IS_OK(session_status)) {
			return smbd_smb2_request_error(req, session_status);
		}
		status = smbd_smb2_request_check_tcon(req);
		if (!NT_STATUS_IS_OK(status)) {
			return smbd_smb2_request_error(req, status);
		}
		return smbd_smb2_request_process_lock(req);

	case SMB2_OP_IOCTL:
		if (!NT_STATUS_IS_OK(session_status)) {
			return smbd_smb2_request_error(req, session_status);
		}
		status = smbd_smb2_request_check_tcon(req);
		if (!NT_STATUS_IS_OK(status)) {
			return smbd_smb2_request_error(req, status);
		}
		return smbd_smb2_request_process_ioctl(req);

	case SMB2_OP_CANCEL:
		return smbd_smb2_request_process_cancel(req);

	case SMB2_OP_KEEPALIVE:
		return smbd_smb2_request_process_keepalive(req);

	case SMB2_OP_FIND:
		if (!NT_STATUS_IS_OK(session_status)) {
			return smbd_smb2_request_error(req, session_status);
		}
		status = smbd_smb2_request_check_tcon(req);
		if (!NT_STATUS_IS_OK(status)) {
			return smbd_smb2_request_error(req, status);
		}
		return smbd_smb2_request_process_find(req);

	case SMB2_OP_NOTIFY:
		if (!NT_STATUS_IS_OK(session_status)) {
			return smbd_smb2_request_error(req, session_status);
		}
		status = smbd_smb2_request_check_tcon(req);
		if (!NT_STATUS_IS_OK(status)) {
			return smbd_smb2_request_error(req, status);
		}
		return smbd_smb2_request_process_notify(req);

	case SMB2_OP_GETINFO:
		if (!NT_STATUS_IS_OK(session_status)) {
			return smbd_smb2_request_error(req, session_status);
		}
		status = smbd_smb2_request_check_tcon(req);
		if (!NT_STATUS_IS_OK(status)) {
			return smbd_smb2_request_error(req, status);
		}
		return smbd_smb2_request_process_getinfo(req);

	case SMB2_OP_SETINFO:
		if (!NT_STATUS_IS_OK(session_status)) {
			return smbd_smb2_request_error(req, session_status);
		}
		status = smbd_smb2_request_check_tcon(req);
		if (!NT_STATUS_IS_OK(status)) {
			return smbd_smb2_request_error(req, status);
		}
		return smbd_smb2_request_process_setinfo(req);

	case SMB2_OP_BREAK:
		if (!NT_STATUS_IS_OK(session_status)) {
			return smbd_smb2_request_error(req, session_status);
		}
		status = smbd_smb2_request_check_tcon(req);
		if (!NT_STATUS_IS_OK(status)) {
			return smbd_smb2_request_error(req, status);
		}
		return smbd_smb2_request_process_break(req);
	}

	return smbd_smb2_request_error(req, NT_STATUS_INVALID_PARAMETER);
}

static void smbd_smb2_request_dispatch_compound(struct tevent_req *subreq);
static void smbd_smb2_request_writev_done(struct tevent_req *subreq);

static NTSTATUS smbd_smb2_request_reply(struct smbd_smb2_request *req)
{
	struct tevent_req *subreq;

	req->subreq = NULL;

	smb2_setup_nbt_length(req->out.vector, req->out.vector_count);

	if (req->do_signing) {
		int i = req->current_idx;
		NTSTATUS status;
		status = smb2_signing_sign_pdu(req->session->session_key,
					       &req->out.vector[i], 3);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
	}

	req->current_idx += 3;

	if (req->current_idx < req->out.vector_count) {
		struct timeval zero = timeval_zero();
		subreq = tevent_wakeup_send(req,
					    req->sconn->smb2.event_ctx,
					    zero);
		if (subreq == NULL) {
			return NT_STATUS_NO_MEMORY;
		}
		tevent_req_set_callback(subreq,
					smbd_smb2_request_dispatch_compound,
					req);

		return NT_STATUS_OK;
	}

	subreq = tstream_writev_queue_send(req,
					   req->sconn->smb2.event_ctx,
					   req->sconn->smb2.stream,
					   req->sconn->smb2.send_queue,
					   req->out.vector,
					   req->out.vector_count);
	if (subreq == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	tevent_req_set_callback(subreq, smbd_smb2_request_writev_done, req);

	return NT_STATUS_OK;
}

static void smbd_smb2_request_dispatch_compound(struct tevent_req *subreq)
{
	struct smbd_smb2_request *req = tevent_req_callback_data(subreq,
					struct smbd_smb2_request);
	struct smbd_server_connection *sconn = req->sconn;
	NTSTATUS status;

	tevent_wakeup_recv(subreq);
	TALLOC_FREE(subreq);

	DEBUG(10,("smbd_smb2_request_dispatch_compound: idx[%d] of %d vectors\n",
		  req->current_idx, req->in.vector_count));

	status = smbd_smb2_request_dispatch(req);
	if (!NT_STATUS_IS_OK(status)) {
		smbd_server_connection_terminate(sconn, nt_errstr(status));
		return;
	}
}

static void smbd_smb2_request_writev_done(struct tevent_req *subreq)
{
	struct smbd_smb2_request *req = tevent_req_callback_data(subreq,
					struct smbd_smb2_request);
	struct smbd_server_connection *sconn = req->sconn;
	int ret;
	int sys_errno;

	ret = tstream_writev_queue_recv(subreq, &sys_errno);
	TALLOC_FREE(subreq);
	TALLOC_FREE(req);
	if (ret == -1) {
		NTSTATUS status = map_nt_error_from_unix(sys_errno);
		smbd_server_connection_terminate(sconn, nt_errstr(status));
		return;
	}
}

NTSTATUS smbd_smb2_request_error_ex(struct smbd_smb2_request *req,
				    NTSTATUS status,
				    DATA_BLOB *info,
				    const char *location)
{
	uint8_t *outhdr;
	uint8_t *outbody;
	int i = req->current_idx;

	DEBUG(10,("smbd_smb2_request_error_ex: idx[%d] status[%s] |%s| at %s\n",
		  i, nt_errstr(status), info ? " +info" : "",
		  location));

	outhdr = (uint8_t *)req->out.vector[i].iov_base;

	SIVAL(outhdr, SMB2_HDR_STATUS, NT_STATUS_V(status));

	outbody = outhdr + SMB2_HDR_BODY;

	req->out.vector[i+1].iov_base = (void *)outbody;
	req->out.vector[i+1].iov_len = 8;

	if (info) {
		SIVAL(outbody, 0x04, info->length);
		req->out.vector[i+2].iov_base	= (void *)info->data;
		req->out.vector[i+2].iov_len	= info->length;
	} else {
		req->out.vector[i+2].iov_base = NULL;
		req->out.vector[i+2].iov_len = 0;
	}

	/*
	 * if a request fails, all other remaining
	 * compounded requests should fail too
	 */
	req->next_status = NT_STATUS_INVALID_PARAMETER;

	return smbd_smb2_request_reply(req);
}

NTSTATUS smbd_smb2_request_done_ex(struct smbd_smb2_request *req,
				   NTSTATUS status,
				   DATA_BLOB body, DATA_BLOB *dyn,
				   const char *location)
{
	uint8_t *outhdr;
	uint8_t *outdyn;
	int i = req->current_idx;
	uint32_t next_command_ofs;

	DEBUG(10,("smbd_smb2_request_done_ex: "
		  "idx[%d] status[%s] body[%u] dyn[%s:%u] at %s\n",
		  i, nt_errstr(status), (unsigned int)body.length,
		  dyn ? "yes": "no",
		  (unsigned int)(dyn ? dyn->length : 0),
		  location));

	if (body.length < 2) {
		return smbd_smb2_request_error(req, NT_STATUS_INTERNAL_ERROR);
	}

	if ((body.length % 2) != 0) {
		return smbd_smb2_request_error(req, NT_STATUS_INTERNAL_ERROR);
	}

	outhdr = (uint8_t *)req->out.vector[i].iov_base;
	/* the fallback dynamic buffer */
	outdyn = outhdr + SMB2_HDR_BODY + 8;

	next_command_ofs = IVAL(outhdr, SMB2_HDR_NEXT_COMMAND);
	SIVAL(outhdr, SMB2_HDR_STATUS, NT_STATUS_V(status));

	req->out.vector[i+1].iov_base = (void *)body.data;
	req->out.vector[i+1].iov_len = body.length;

	if (dyn) {
		req->out.vector[i+2].iov_base	= (void *)dyn->data;
		req->out.vector[i+2].iov_len	= dyn->length;
	} else {
		req->out.vector[i+2].iov_base = NULL;
		req->out.vector[i+2].iov_len = 0;
	}

	/* see if we need to recalculate the offset to the next response */
	if (next_command_ofs > 0) {
		next_command_ofs  = SMB2_HDR_BODY;
		next_command_ofs += req->out.vector[i+1].iov_len;
		next_command_ofs += req->out.vector[i+2].iov_len;
	}

	if ((next_command_ofs % 8) != 0) {
		size_t pad_size = 8 - (next_command_ofs % 8);
		if (req->out.vector[i+2].iov_len == 0) {
			/*
			 * if the dyn buffer is empty
			 * we can use it to add padding
			 */
			uint8_t *pad;

			pad = talloc_zero_array(req->out.vector,
						uint8_t, pad_size);
			if (pad == NULL) {
				return smbd_smb2_request_error(req,
						NT_STATUS_NO_MEMORY);
			}

			req->out.vector[i+2].iov_base = (void *)pad;
			req->out.vector[i+2].iov_len = pad_size;
		} else {
			/*
			 * For now we copy the dynamic buffer
			 * and add the padding to the new buffer
			 */
			size_t old_size;
			uint8_t *old_dyn;
			size_t new_size;
			uint8_t *new_dyn;

			old_size = req->out.vector[i+2].iov_len;
			old_dyn = (uint8_t *)req->out.vector[i+2].iov_base;

			new_size = old_size + pad_size;
			new_dyn = talloc_array(req->out.vector,
					       uint8_t, new_size);
			if (new_dyn == NULL) {
				return smbd_smb2_request_error(req,
						NT_STATUS_NO_MEMORY);
			}

			memcpy(new_dyn, old_dyn, old_size);
			memset(new_dyn + old_size, 0, pad_size);

			req->out.vector[i+2].iov_base = (void *)new_dyn;
			req->out.vector[i+2].iov_len = new_size;

			TALLOC_FREE(old_dyn);
		}
		next_command_ofs += pad_size;
	}

	SIVAL(outhdr, SMB2_HDR_NEXT_COMMAND, next_command_ofs);

	return smbd_smb2_request_reply(req);
}

struct smbd_smb2_send_oplock_break_state {
	struct smbd_server_connection *sconn;
	uint8_t buf[4 + SMB2_HDR_BODY + 0x18];
	struct iovec vector;
};

static void smbd_smb2_oplock_break_writev_done(struct tevent_req *subreq);

NTSTATUS smbd_smb2_send_oplock_break(struct smbd_server_connection *sconn,
				     uint64_t file_id_persistent,
				     uint64_t file_id_volatile,
				     uint8_t oplock_level)
{
	struct smbd_smb2_send_oplock_break_state *state;
	struct tevent_req *subreq;
	uint8_t *hdr;
	uint8_t *body;

	state = talloc(sconn, struct smbd_smb2_send_oplock_break_state);
	if (state == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	state->sconn = sconn;

	state->vector.iov_base = (void *)state->buf;
	state->vector.iov_len = sizeof(state->buf);

	_smb2_setlen(state->buf, sizeof(state->buf) - 4);
	hdr = state->buf + 4;
	body = hdr + SMB2_HDR_BODY;

	SIVAL(hdr, 0,				SMB2_MAGIC);
	SSVAL(hdr, SMB2_HDR_LENGTH,		SMB2_HDR_BODY);
	SSVAL(hdr, SMB2_HDR_EPOCH,		0);
	SIVAL(hdr, SMB2_HDR_STATUS,		0);
	SSVAL(hdr, SMB2_HDR_OPCODE,		SMB2_OP_BREAK);
	SSVAL(hdr, SMB2_HDR_CREDIT,		0);
	SIVAL(hdr, SMB2_HDR_FLAGS,		SMB2_HDR_FLAG_REDIRECT);
	SIVAL(hdr, SMB2_HDR_NEXT_COMMAND,	0);
	SBVAL(hdr, SMB2_HDR_MESSAGE_ID,		UINT64_MAX);
	SIVAL(hdr, SMB2_HDR_PID,		0);
	SIVAL(hdr, SMB2_HDR_TID,		0);
	SBVAL(hdr, SMB2_HDR_SESSION_ID,		0);
	memset(hdr+SMB2_HDR_SIGNATURE, 0, 16);

	SSVAL(body, 0x00, 0x18);

	SCVAL(body, 0x02, oplock_level);
	SCVAL(body, 0x03, 0);		/* reserved */
	SIVAL(body, 0x04, 0);		/* reserved */
	SBVAL(body, 0x08, file_id_persistent);
	SBVAL(body, 0x10, file_id_volatile);

	subreq = tstream_writev_queue_send(state,
					   sconn->smb2.event_ctx,
					   sconn->smb2.stream,
					   sconn->smb2.send_queue,
					   &state->vector, 1);
	if (subreq == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	tevent_req_set_callback(subreq,
				smbd_smb2_oplock_break_writev_done,
				state);

	return NT_STATUS_OK;
}

static void smbd_smb2_oplock_break_writev_done(struct tevent_req *subreq)
{
	struct smbd_smb2_send_oplock_break_state *state =
		tevent_req_callback_data(subreq,
		struct smbd_smb2_send_oplock_break_state);
	struct smbd_server_connection *sconn = state->sconn;
	int ret;
	int sys_errno;

	ret = tstream_writev_queue_recv(subreq, &sys_errno);
	TALLOC_FREE(subreq);
	if (ret == -1) {
		NTSTATUS status = map_nt_error_from_unix(sys_errno);
		smbd_server_connection_terminate(sconn, nt_errstr(status));
		return;
	}

	TALLOC_FREE(state);
}

struct smbd_smb2_request_read_state {
	size_t missing;
	bool asked_for_header;
	struct smbd_smb2_request *smb2_req;
};

static int smbd_smb2_request_next_vector(struct tstream_context *stream,
					 void *private_data,
					 TALLOC_CTX *mem_ctx,
					 struct iovec **_vector,
					 size_t *_count);
static void smbd_smb2_request_read_done(struct tevent_req *subreq);

static struct tevent_req *smbd_smb2_request_read_send(TALLOC_CTX *mem_ctx,
					struct tevent_context *ev,
					struct smbd_server_connection *sconn)
{
	struct tevent_req *req;
	struct smbd_smb2_request_read_state *state;
	struct tevent_req *subreq;

	req = tevent_req_create(mem_ctx, &state,
				struct smbd_smb2_request_read_state);
	if (req == NULL) {
		return NULL;
	}
	state->missing = 0;
	state->asked_for_header = false;

	state->smb2_req = smbd_smb2_request_allocate(state);
	if (tevent_req_nomem(state->smb2_req, req)) {
		return tevent_req_post(req, ev);
	}
	state->smb2_req->sconn = sconn;

	subreq = tstream_readv_pdu_queue_send(state, ev, sconn->smb2.stream,
					      sconn->smb2.recv_queue,
					      smbd_smb2_request_next_vector,
					      state);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, smbd_smb2_request_read_done, req);

	return req;
}

static int smbd_smb2_request_next_vector(struct tstream_context *stream,
					 void *private_data,
					 TALLOC_CTX *mem_ctx,
					 struct iovec **_vector,
					 size_t *_count)
{
	struct smbd_smb2_request_read_state *state =
		talloc_get_type_abort(private_data,
		struct smbd_smb2_request_read_state);
	struct smbd_smb2_request *req = state->smb2_req;
	struct iovec *vector;
	int idx = req->in.vector_count;
	size_t len = 0;
	uint8_t *buf = NULL;

	if (req->in.vector_count == 0) {
		/*
		 * first we need to get the NBT header
		 */
		req->in.vector = talloc_array(req, struct iovec,
					      req->in.vector_count + 1);
		if (req->in.vector == NULL) {
			return -1;
		}
		req->in.vector_count += 1;

		req->in.vector[idx].iov_base	= (void *)req->in.nbt_hdr;
		req->in.vector[idx].iov_len	= 4;

		vector = talloc_array(mem_ctx, struct iovec, 1);
		if (vector == NULL) {
			return -1;
		}

		vector[0] = req->in.vector[idx];

		*_vector = vector;
		*_count = 1;
		return 0;
	}

	if (req->in.vector_count == 1) {
		/*
		 * Now we analyze the NBT header
		 */
		state->missing = smb2_len(req->in.vector[0].iov_base);

		if (state->missing == 0) {
			/* if there're no remaining bytes, we're done */
			*_vector = NULL;
			*_count = 0;
			return 0;
		}

		req->in.vector = talloc_realloc(req, req->in.vector,
						struct iovec,
						req->in.vector_count + 1);
		if (req->in.vector == NULL) {
			return -1;
		}
		req->in.vector_count += 1;

		if (CVAL(req->in.vector[0].iov_base, 0) != 0) {
			/*
			 * it's a special NBT message,
			 * so get all remaining bytes
			 */
			len = state->missing;
		} else if (state->missing < (SMB2_HDR_BODY + 2)) {
			/*
			 * it's an invalid message, just read what we can get
			 * and let the caller handle the error
			 */
			len = state->missing;
		} else {
			/*
			 * We assume it's a SMB2 request,
			 * and we first get the header and the
			 * first 2 bytes (the struct size) of the body
			 */
			len = SMB2_HDR_BODY + 2;

			state->asked_for_header = true;
		}

		state->missing -= len;

		buf = talloc_array(req->in.vector, uint8_t, len);
		if (buf == NULL) {
			return -1;
		}

		req->in.vector[idx].iov_base	= (void *)buf;
		req->in.vector[idx].iov_len	= len;

		vector = talloc_array(mem_ctx, struct iovec, 1);
		if (vector == NULL) {
			return -1;
		}

		vector[0] = req->in.vector[idx];

		*_vector = vector;
		*_count = 1;
		return 0;
	}

	if (state->missing == 0) {
		/* if there're no remaining bytes, we're done */
		*_vector = NULL;
		*_count = 0;
		return 0;
	}

	if (state->asked_for_header) {
		const uint8_t *hdr;
		size_t full_size;
		size_t next_command_ofs;
		size_t body_size;
		uint8_t *body;
		size_t dyn_size;
		uint8_t *dyn;
		bool invalid = false;

		state->asked_for_header = false;

		/*
		 * We got the SMB2 header and the first 2 bytes
		 * of the body. We fix the size to just the header
		 * and manually copy the 2 first bytes to the body section
		 */
		req->in.vector[idx-1].iov_len = SMB2_HDR_BODY;
		hdr = (const uint8_t *)req->in.vector[idx-1].iov_base;

		/* allocate vectors for body and dynamic areas */
		req->in.vector = talloc_realloc(req, req->in.vector,
						struct iovec,
						req->in.vector_count + 2);
		if (req->in.vector == NULL) {
			return -1;
		}
		req->in.vector_count += 2;

		full_size = state->missing + SMB2_HDR_BODY + 2;
		next_command_ofs = IVAL(hdr, SMB2_HDR_NEXT_COMMAND);
		body_size = SVAL(hdr, SMB2_HDR_BODY);

		if (next_command_ofs != 0) {
			if (next_command_ofs < (SMB2_HDR_BODY + 2)) {
				/*
				 * this is invalid, just return a zero
				 * body and let the caller deal with the error
				 */
				invalid = true;
			} else if (next_command_ofs > full_size) {
				/*
				 * this is invalid, just return a zero
				 * body and let the caller deal with the error
				 */
				invalid = true;
			} else {
				full_size = next_command_ofs;
			}
		}

		if (!invalid) {
			if (body_size < 2) {
				/*
				 * this is invalid, just return a zero
				 * body and let the caller deal with the error
				 */
				invalid = true;
			}

			if ((body_size % 2) != 0) {
				body_size -= 1;
			}

			if (body_size > (full_size - SMB2_HDR_BODY)) {
				/*
				 * this is invalid, just return a zero
				 * body and let the caller deal with the error
				 */
				invalid = true;
			}
		}

		if (invalid) {
			/* the caller should check this */
			body_size = 2;
		}

		dyn_size = full_size - (SMB2_HDR_BODY + body_size);

		state->missing -= (body_size - 2) + dyn_size;

		body = talloc_array(req->in.vector, uint8_t, body_size);
		if (body == NULL) {
			return -1;
		}

		dyn = talloc_array(req->in.vector, uint8_t, dyn_size);
		if (dyn == NULL) {
			return -1;
		}

		req->in.vector[idx].iov_base	= (void *)body;
		req->in.vector[idx].iov_len	= body_size;
		req->in.vector[idx+1].iov_base	= (void *)dyn;
		req->in.vector[idx+1].iov_len	= dyn_size;

		vector = talloc_array(mem_ctx, struct iovec, 2);
		if (vector == NULL) {
			return -1;
		}

		/*
		 * the first 2 bytes of the body were already fetched
		 * together with the header
		 */
		memcpy(body, hdr + SMB2_HDR_BODY, 2);
		vector[0].iov_base = body + 2;
		vector[0].iov_len = body_size - 2;

		vector[1] = req->in.vector[idx+1];

		*_vector = vector;
		*_count = 2;
		return 0;
	}

	/*
	 * when we endup here, we're looking for a new SMB2 request
	 * next. And we ask for its header and the first 2 bytes of
	 * the body (like we did for the first SMB2 request).
	 */

	req->in.vector = talloc_realloc(req, req->in.vector,
					struct iovec,
					req->in.vector_count + 1);
	if (req->in.vector == NULL) {
		return -1;
	}
	req->in.vector_count += 1;

	/*
	 * We assume it's a SMB2 request,
	 * and we first get the header and the
	 * first 2 bytes (the struct size) of the body
	 */
	len = SMB2_HDR_BODY + 2;

	if (len > state->missing) {
		/* let the caller handle the error */
		len = state->missing;
	}

	state->missing -= len;
	state->asked_for_header = true;

	buf = talloc_array(req->in.vector, uint8_t, len);
	if (buf == NULL) {
		return -1;
	}

	req->in.vector[idx].iov_base	= (void *)buf;
	req->in.vector[idx].iov_len	= len;

	vector = talloc_array(mem_ctx, struct iovec, 1);
	if (vector == NULL) {
		return -1;
	}

	vector[0] = req->in.vector[idx];

	*_vector = vector;
	*_count = 1;
	return 0;
}

static void smbd_smb2_request_read_done(struct tevent_req *subreq)
{
	struct tevent_req *req =
		tevent_req_callback_data(subreq,
		struct tevent_req);
	int ret;
	int sys_errno;
	NTSTATUS status;

	ret = tstream_readv_pdu_queue_recv(subreq, &sys_errno);
	if (ret == -1) {
		status = map_nt_error_from_unix(sys_errno);
		tevent_req_nterror(req, status);
		return;
	}

	tevent_req_done(req);
}

static NTSTATUS smbd_smb2_request_read_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx,
					    struct smbd_smb2_request **_smb2_req)
{
	struct smbd_smb2_request_read_state *state =
		tevent_req_data(req,
		struct smbd_smb2_request_read_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		tevent_req_received(req);
		return status;
	}

	talloc_steal(mem_ctx, state->smb2_req->mem_pool);
	*_smb2_req = state->smb2_req;
	tevent_req_received(req);
	return NT_STATUS_OK;
}

static void smbd_smb2_request_incoming(struct tevent_req *subreq);

void smbd_smb2_first_negprot(struct smbd_server_connection *sconn,
			     const uint8_t *inbuf, size_t size)
{
	NTSTATUS status;
	struct smbd_smb2_request *req;
	struct tevent_req *subreq;

	DEBUG(10,("smbd_smb2_first_negprot: packet length %u\n",
		 (unsigned int)size));

	status = smbd_initialize_smb2(sconn);
	if (!NT_STATUS_IS_OK(status)) {
		smbd_server_connection_terminate(sconn, nt_errstr(status));
		return;
	}

	status = smbd_smb2_request_create(sconn, inbuf, size, &req);
	if (!NT_STATUS_IS_OK(status)) {
		smbd_server_connection_terminate(sconn, nt_errstr(status));
		return;
	}

	status = smbd_smb2_request_setup_out(req);
	if (!NT_STATUS_IS_OK(status)) {
		smbd_server_connection_terminate(sconn, nt_errstr(status));
		return;
	}

	status = smbd_smb2_request_dispatch(req);
	if (!NT_STATUS_IS_OK(status)) {
		smbd_server_connection_terminate(sconn, nt_errstr(status));
		return;
	}

	/* ask for the next request */
	subreq = smbd_smb2_request_read_send(sconn, sconn->smb2.event_ctx, sconn);
	if (subreq == NULL) {
		smbd_server_connection_terminate(sconn, "no memory for reading");
		return;
	}
	tevent_req_set_callback(subreq, smbd_smb2_request_incoming, sconn);
}

static void smbd_smb2_request_incoming(struct tevent_req *subreq)
{
	struct smbd_server_connection *sconn = tevent_req_callback_data(subreq,
					       struct smbd_server_connection);
	NTSTATUS status;
	struct smbd_smb2_request *req = NULL;

	status = smbd_smb2_request_read_recv(subreq, sconn, &req);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		smbd_server_connection_terminate(sconn, nt_errstr(status));
		return;
	}

	if (req->in.nbt_hdr[0] != 0x00) {
		DEBUG(1,("smbd_smb2_request_incoming: ignore NBT[0x%02X] msg\n",
			 req->in.nbt_hdr[0]));
		TALLOC_FREE(req);
		goto next;
	}

	req->current_idx = 1;

	DEBUG(10,("smbd_smb2_request_incoming: idx[%d] of %d vectors\n",
		 req->current_idx, req->in.vector_count));

	status = smbd_smb2_request_validate(req);
	if (!NT_STATUS_IS_OK(status)) {
		smbd_server_connection_terminate(sconn, nt_errstr(status));
		return;
	}

	status = smbd_smb2_request_setup_out(req);
	if (!NT_STATUS_IS_OK(status)) {
		smbd_server_connection_terminate(sconn, nt_errstr(status));
		return;
	}

	status = smbd_smb2_request_dispatch(req);
	if (!NT_STATUS_IS_OK(status)) {
		smbd_server_connection_terminate(sconn, nt_errstr(status));
		return;
	}

next:
	/* ask for the next request (this constructs the main loop) */
	subreq = smbd_smb2_request_read_send(sconn, sconn->smb2.event_ctx, sconn);
	if (subreq == NULL) {
		smbd_server_connection_terminate(sconn, "no memory for reading");
		return;
	}
	tevent_req_set_callback(subreq, smbd_smb2_request_incoming, sconn);
}
