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
#include "messages.h"

struct smbd_smb2_lock_element {
	uint64_t offset;
	uint64_t length;
	uint32_t flags;
};

struct smbd_smb2_lock_state {
	struct smbd_smb2_request *smb2req;
	struct smb_request *smb1req;
	struct blocking_lock_record *blr;
	uint16_t lock_count;
	struct smbd_lock_element *locks;
};

static void remove_pending_lock(struct smbd_smb2_lock_state *state,
				struct blocking_lock_record *blr);

static struct tevent_req *smbd_smb2_lock_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct smbd_smb2_request *smb2req,
						 struct files_struct *in_fsp,
						 uint32_t in_smbpid,
						 uint16_t in_lock_count,
						 struct smbd_smb2_lock_element *in_locks);
static NTSTATUS smbd_smb2_lock_recv(struct tevent_req *req);

static void smbd_smb2_request_lock_done(struct tevent_req *subreq);
NTSTATUS smbd_smb2_request_process_lock(struct smbd_smb2_request *req)
{
	const uint8_t *inhdr;
	const uint8_t *inbody;
	const int i = req->current_idx;
	uint32_t in_smbpid;
	uint16_t in_lock_count;
	uint64_t in_file_id_persistent;
	uint64_t in_file_id_volatile;
	struct files_struct *in_fsp;
	struct smbd_smb2_lock_element *in_locks;
	struct tevent_req *subreq;
	const uint8_t *lock_buffer;
	uint16_t l;
	NTSTATUS status;

	status = smbd_smb2_request_verify_sizes(req, 0x30);
	if (!NT_STATUS_IS_OK(status)) {
		return smbd_smb2_request_error(req, status);
	}
	inhdr = (const uint8_t *)req->in.vector[i+0].iov_base;
	inbody = (const uint8_t *)req->in.vector[i+1].iov_base;

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

	in_fsp = file_fsp_smb2(req, in_file_id_persistent, in_file_id_volatile);
	if (in_fsp == NULL) {
		return smbd_smb2_request_error(req, NT_STATUS_FILE_CLOSED);
	}

	subreq = smbd_smb2_lock_send(req, req->sconn->smb2.event_ctx,
				     req, in_fsp,
				     in_smbpid,
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
	struct smbd_smb2_request *smb2req = tevent_req_callback_data(subreq,
					struct smbd_smb2_request);
	DATA_BLOB outbody;
	NTSTATUS status;
	NTSTATUS error; /* transport error */

	if (smb2req->cancelled) {
		const uint8_t *inhdr = (const uint8_t *)
			smb2req->in.vector[smb2req->current_idx].iov_base;
		uint64_t mid = BVAL(inhdr, SMB2_HDR_MESSAGE_ID);
		struct smbd_smb2_lock_state *state;

		DEBUG(10,("smbd_smb2_request_lock_done: cancelled mid %llu\n",
			(unsigned long long)mid ));

		state = tevent_req_data(smb2req->subreq,
				struct smbd_smb2_lock_state);

		SMB_ASSERT(state);
		SMB_ASSERT(state->blr);

		remove_pending_lock(state, state->blr);

		error = smbd_smb2_request_error(smb2req, NT_STATUS_CANCELLED);
		if (!NT_STATUS_IS_OK(error)) {
			smbd_server_connection_terminate(smb2req->sconn,
				nt_errstr(error));
			return;
		}
		return;
	}

	status = smbd_smb2_lock_recv(subreq);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		error = smbd_smb2_request_error(smb2req, status);
		if (!NT_STATUS_IS_OK(error)) {
			smbd_server_connection_terminate(smb2req->sconn,
							 nt_errstr(error));
			return;
		}
		return;
	}

	outbody = data_blob_talloc(smb2req->out.vector, NULL, 0x04);
	if (outbody.data == NULL) {
		error = smbd_smb2_request_error(smb2req, NT_STATUS_NO_MEMORY);
		if (!NT_STATUS_IS_OK(error)) {
			smbd_server_connection_terminate(smb2req->sconn,
							 nt_errstr(error));
			return;
		}
		return;
	}

	SSVAL(outbody.data, 0x00, 0x04);	/* struct size */
	SSVAL(outbody.data, 0x02, 0);		/* reserved */

	error = smbd_smb2_request_done(smb2req, outbody, NULL);
	if (!NT_STATUS_IS_OK(error)) {
		smbd_server_connection_terminate(smb2req->sconn,
						 nt_errstr(error));
		return;
	}
}

static struct tevent_req *smbd_smb2_lock_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct smbd_smb2_request *smb2req,
						 struct files_struct *fsp,
						 uint32_t in_smbpid,
						 uint16_t in_lock_count,
						 struct smbd_smb2_lock_element *in_locks)
{
	struct tevent_req *req;
	struct smbd_smb2_lock_state *state;
	struct smb_request *smb1req;
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
	smb2req->subreq = req; /* So we can find this when going async. */

	smb1req = smbd_smb2_fake_smb_request(smb2req);
	if (tevent_req_nomem(smb1req, req)) {
		return tevent_req_post(req, ev);
	}
	state->smb1req = smb1req;

	DEBUG(10,("smbd_smb2_lock_send: %s - fnum[%d]\n",
		  fsp_str_dbg(fsp), fsp->fnum));

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
		bool invalid = false;

		switch (in_locks[i].flags) {
		case SMB2_LOCK_FLAG_SHARED:
		case SMB2_LOCK_FLAG_EXCLUSIVE:
			if (isunlock) {
				invalid = true;
				break;
			}
			if (i > 0) {
				tevent_req_nterror(req,
						   NT_STATUS_INVALID_PARAMETER);
				return tevent_req_post(req, ev);
			}
			break;

		case SMB2_LOCK_FLAG_SHARED|SMB2_LOCK_FLAG_FAIL_IMMEDIATELY:
		case SMB2_LOCK_FLAG_EXCLUSIVE|SMB2_LOCK_FLAG_FAIL_IMMEDIATELY:
			if (isunlock) {
				invalid = true;
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

		locks[i].smblctx = fsp->fnum;
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

		DEBUG(10,("smbd_smb2_lock_send: index %d offset=%llu, count=%llu, "
			"smblctx = %llu type %d\n",
			i,
			(unsigned long long)locks[i].offset,
			(unsigned long long)locks[i].count,
			(unsigned long long)locks[i].smblctx,
			(int)locks[i].brltype ));
	}

	state->locks = locks;
	state->lock_count = in_lock_count;

	if (isunlock) {
		status = smbd_do_locking(smb1req, fsp,
					 0,
					 timeout,
					 in_lock_count,
					 locks,
					 0,
					 NULL,
					 &async);
	} else {
		status = smbd_do_locking(smb1req, fsp,
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
		return req;
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

/****************************************************************
 Cancel an outstanding blocking lock request.
*****************************************************************/

static bool smbd_smb2_lock_cancel(struct tevent_req *req)
{
        struct smbd_smb2_request *smb2req = NULL;
        struct smbd_smb2_lock_state *state = tevent_req_data(req,
                                struct smbd_smb2_lock_state);
        if (!state) {
                return false;
        }

        if (!state->smb2req) {
                return false;
        }

        smb2req = state->smb2req;
        smb2req->cancelled = true;

        tevent_req_done(req);
        return true;
}

/****************************************************************
 Got a message saying someone unlocked a file. Re-schedule all
 blocking lock requests as we don't know if anything overlapped.
*****************************************************************/

static void received_unlock_msg(struct messaging_context *msg,
				void *private_data,
				uint32_t msg_type,
				struct server_id server_id,
				DATA_BLOB *data)
{
	struct smbd_server_connection *sconn;

	DEBUG(10,("received_unlock_msg (SMB2)\n"));

	sconn = msg_ctx_to_sconn(msg);
	if (sconn == NULL) {
		DEBUG(1, ("could not find sconn\n"));
		return;
	}
	process_blocking_lock_queue_smb2(sconn, timeval_current());
}

/****************************************************************
 Function to get the blr on a pending record.
*****************************************************************/

struct blocking_lock_record *get_pending_smb2req_blr(struct smbd_smb2_request *smb2req)
{
	struct smbd_smb2_lock_state *state = NULL;
	const uint8_t *inhdr;

	if (!smb2req) {
		return NULL;
	}
	if (smb2req->subreq == NULL) {
		return NULL;
	}
	if (!tevent_req_is_in_progress(smb2req->subreq)) {
		return NULL;
	}
	inhdr = (const uint8_t *)smb2req->in.vector[smb2req->current_idx].iov_base;
	if (SVAL(inhdr, SMB2_HDR_OPCODE) != SMB2_OP_LOCK) {
		return NULL;
	}
	state = tevent_req_data(smb2req->subreq,
			struct smbd_smb2_lock_state);
	if (!state) {
		return NULL;
	}
	return state->blr;
}
/****************************************************************
 Set up the next brl timeout.
*****************************************************************/

static bool recalc_smb2_brl_timeout(struct smbd_server_connection *sconn)
{
	struct smbd_smb2_request *smb2req;
	struct timeval next_timeout = timeval_zero();
	int max_brl_timeout = lp_parm_int(-1, "brl", "recalctime", 5);

	TALLOC_FREE(sconn->smb2.locks.brl_timeout);

	for (smb2req = sconn->smb2.requests; smb2req; smb2req = smb2req->next) {
		struct blocking_lock_record *blr =
			get_pending_smb2req_blr(smb2req);
		if (!blr) {
			continue;
		}
		if (timeval_is_zero(&blr->expire_time)) {
			/*
			 * If we're blocked on pid 0xFFFFFFFFFFFFFFFFLL this is
			 * a POSIX lock, so calculate a timeout of
			 * 10 seconds into the future.
			 */
			if (blr->blocking_smblctx == 0xFFFFFFFFFFFFFFFFLL) {
				struct timeval psx_to = timeval_current_ofs(10, 0);
				next_timeout = timeval_brl_min(&next_timeout, &psx_to);
			}

			continue;
		}

		next_timeout = timeval_brl_min(&next_timeout, &blr->expire_time);
	}

	if (timeval_is_zero(&next_timeout)) {
		DEBUG(10, ("recalc_smb2_brl_timeout:Next "
			"timeout = Infinite.\n"));
		return true;
	}

        /*
	 * To account for unclean shutdowns by clients we need a
	 * maximum timeout that we use for checking pending locks. If
	 * we have any pending locks at all, then check if the pending
	 * lock can continue at least every brl:recalctime seconds
	 * (default 5 seconds).
	 *
	 * This saves us needing to do a message_send_all() in the
	 * SIGCHLD handler in the parent daemon. That
	 * message_send_all() caused O(n^2) work to be done when IP
	 * failovers happened in clustered Samba, which could make the
	 * entire system unusable for many minutes.
	 */

	if (max_brl_timeout > 0) {
		struct timeval min_to = timeval_current_ofs(max_brl_timeout, 0);
		next_timeout = timeval_brl_min(&next_timeout, &min_to);
	}

	if (DEBUGLVL(10)) {
		struct timeval cur, from_now;

		cur = timeval_current();
		from_now = timeval_until(&cur, &next_timeout);
		DEBUG(10, ("recalc_smb2_brl_timeout: Next "
			"timeout = %d.%d seconds from now.\n",
			(int)from_now.tv_sec, (int)from_now.tv_usec));
	}

	sconn->smb2.locks.brl_timeout = event_add_timed(
				smbd_event_context(),
				NULL,
				next_timeout,
				brl_timeout_fn,
				NULL);
	if (!sconn->smb2.locks.brl_timeout) {
		return false;
	}
	return true;
}

/****************************************************************
 Get an SMB2 lock reqeust to go async. lock_timeout should
 always be -1 here.
*****************************************************************/

bool push_blocking_lock_request_smb2( struct byte_range_lock *br_lck,
				struct smb_request *smb1req,
				files_struct *fsp,
				int lock_timeout,
				int lock_num,
				uint64_t smblctx,
				enum brl_type lock_type,
				enum brl_flavour lock_flav,
				uint64_t offset,
				uint64_t count,
				uint64_t blocking_smblctx)
{
	struct smbd_server_connection *sconn = smb1req->sconn;
	struct smbd_smb2_request *smb2req = smb1req->smb2req;
	struct tevent_req *req = NULL;
	struct smbd_smb2_lock_state *state = NULL;
	struct blocking_lock_record *blr = NULL;
	NTSTATUS status = NT_STATUS_OK;

	if (!smb2req) {
		return false;
	}
	req = smb2req->subreq;
	if (!req) {
		return false;
	}
	if (!tevent_req_is_in_progress(smb2req->subreq)) {
		return false;
	}
	state = tevent_req_data(req, struct smbd_smb2_lock_state);
	if (!state) {
		return false;
	}

	blr = talloc_zero(state, struct blocking_lock_record);
	if (!blr) {
		return false;
	}
	blr->fsp = fsp;

	if (lock_timeout == -1) {
		blr->expire_time.tv_sec = 0;
		blr->expire_time.tv_usec = 0; /* Never expire. */
	} else {
		blr->expire_time = timeval_current_ofs(
			lock_timeout/1000,
			(lock_timeout % 1000) * 1000);
	}

	blr->lock_num = lock_num;
	blr->smblctx = smblctx;
	blr->blocking_smblctx = blocking_smblctx;
	blr->lock_flav = lock_flav;
	blr->lock_type = lock_type;
	blr->offset = offset;
	blr->count = count;

	/* Specific brl_lock() implementations can fill this in. */
	blr->blr_private = NULL;

	/* Add a pending lock record for this. */
	status = brl_lock(sconn->msg_ctx,
			br_lck,
			smblctx,
			sconn_server_id(sconn),
			offset,
			count,
			lock_type == READ_LOCK ? PENDING_READ_LOCK : PENDING_WRITE_LOCK,
			blr->lock_flav,
			true,
			NULL,
			blr);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("push_blocking_lock_request_smb2: "
			"failed to add PENDING_LOCK record.\n"));
		TALLOC_FREE(blr);
		return false;
	}
	state->blr = blr;

	DEBUG(10,("push_blocking_lock_request_smb2: file %s timeout %d\n",
		fsp_str_dbg(fsp),
		lock_timeout ));

	recalc_smb2_brl_timeout(sconn);

	/* Ensure we'll receive messages when this is unlocked. */
	if (!sconn->smb2.locks.blocking_lock_unlock_state) {
		messaging_register(sconn->msg_ctx, NULL,
				MSG_SMB_UNLOCK, received_unlock_msg);
		sconn->smb2.locks.blocking_lock_unlock_state = true;
        }

	/* allow this request to be canceled */
	tevent_req_set_cancel_fn(req, smbd_smb2_lock_cancel);

	return true;
}

/****************************************************************
 Remove a pending lock record under lock.
*****************************************************************/

static void remove_pending_lock(struct smbd_smb2_lock_state *state,
			struct blocking_lock_record *blr)
{
	int i;
	struct byte_range_lock *br_lck = brl_get_locks(
				state, blr->fsp);

	DEBUG(10, ("remove_pending_lock: BLR = %p\n", blr));

	if (br_lck) {
		brl_lock_cancel(br_lck,
				blr->smblctx,
				sconn_server_id(blr->fsp->conn->sconn),
				blr->offset,
				blr->count,
				blr->lock_flav,
				blr);
		TALLOC_FREE(br_lck);
	}

	/* Remove the locks we already got. */

	for(i = blr->lock_num - 1; i >= 0; i--) {
		struct smbd_lock_element *e = &state->locks[i];

		do_unlock(blr->fsp->conn->sconn->msg_ctx,
			blr->fsp,
			e->smblctx,
			e->count,
			e->offset,
			WINDOWS_LOCK);
	}
}

/****************************************************************
 Re-proccess a blocking lock request.
 This is equivalent to process_lockingX() inside smbd/blocking.c
*****************************************************************/

static void reprocess_blocked_smb2_lock(struct smbd_smb2_request *smb2req,
				struct timeval tv_curr)
{
	NTSTATUS status = NT_STATUS_UNSUCCESSFUL;
	struct blocking_lock_record *blr = NULL;
	struct smbd_smb2_lock_state *state = NULL;
	files_struct *fsp = NULL;

	if (!smb2req->subreq) {
		return;
	}
	state = tevent_req_data(smb2req->subreq, struct smbd_smb2_lock_state);
	if (!state) {
		return;
	}

	blr = state->blr;
	fsp = blr->fsp;

	/* Try and finish off getting all the outstanding locks. */

	for (; blr->lock_num < state->lock_count; blr->lock_num++) {
		struct byte_range_lock *br_lck = NULL;
		struct smbd_lock_element *e = &state->locks[blr->lock_num];

		br_lck = do_lock(fsp->conn->sconn->msg_ctx,
				fsp,
				e->smblctx,
				e->count,
				e->offset,
				e->brltype,
				WINDOWS_LOCK,
				true,
				&status,
				&blr->blocking_smblctx,
				blr);

		TALLOC_FREE(br_lck);

		if (NT_STATUS_IS_ERR(status)) {
			break;
		}
	}

	if(blr->lock_num == state->lock_count) {
		/*
		 * Success - we got all the locks.
		 */

		DEBUG(3,("reprocess_blocked_smb2_lock SUCCESS file = %s, "
			"fnum=%d num_locks=%d\n",
			fsp_str_dbg(fsp),
			fsp->fnum,
			(int)state->lock_count));

		tevent_req_done(smb2req->subreq);
		return;
	}

	if (!NT_STATUS_EQUAL(status,NT_STATUS_LOCK_NOT_GRANTED) &&
			!NT_STATUS_EQUAL(status,NT_STATUS_FILE_LOCK_CONFLICT)) {
		/*
		 * We have other than a "can't get lock"
		 * error. Return an error.
		 */
		remove_pending_lock(state, blr);
		tevent_req_nterror(smb2req->subreq, status);
		return;
        }

	/*
	 * We couldn't get the locks for this record on the list.
	 * If the time has expired, return a lock error.
	 */

	if (!timeval_is_zero(&blr->expire_time) &&
			timeval_compare(&blr->expire_time, &tv_curr) <= 0) {
		remove_pending_lock(state, blr);
		tevent_req_nterror(smb2req->subreq, NT_STATUS_LOCK_NOT_GRANTED);
		return;
	}

	/*
	 * Still can't get all the locks - keep waiting.
	 */

	DEBUG(10,("reprocess_blocked_smb2_lock: only got %d locks of %d needed "
		"for file %s, fnum = %d. Still waiting....\n",
		(int)blr->lock_num,
		(int)state->lock_count,
		fsp_str_dbg(fsp),
		(int)fsp->fnum));

        return;

}

/****************************************************************
 Attempt to proccess all outstanding blocking locks pending on
 the request queue.
*****************************************************************/

void process_blocking_lock_queue_smb2(
	struct smbd_server_connection *sconn, struct timeval tv_curr)
{
	struct smbd_smb2_request *smb2req, *nextreq;

	for (smb2req = sconn->smb2.requests; smb2req; smb2req = nextreq) {
		const uint8_t *inhdr;

		nextreq = smb2req->next;

		if (smb2req->subreq == NULL) {
			/* This message has been processed. */
			continue;
		}
		if (!tevent_req_is_in_progress(smb2req->subreq)) {
			/* This message has been processed. */
			continue;
		}

		inhdr = (const uint8_t *)smb2req->in.vector[smb2req->current_idx].iov_base;
		if (SVAL(inhdr, SMB2_HDR_OPCODE) == SMB2_OP_LOCK) {
			reprocess_blocked_smb2_lock(smb2req, tv_curr);
		}
	}

	recalc_smb2_brl_timeout(sconn);
}

/****************************************************************************
 Remove any locks on this fd. Called from file_close().
****************************************************************************/

void cancel_pending_lock_requests_by_fid_smb2(files_struct *fsp,
			struct byte_range_lock *br_lck,
			enum file_close_type close_type)
{
	struct smbd_server_connection *sconn = fsp->conn->sconn;
	struct smbd_smb2_request *smb2req, *nextreq;

	for (smb2req = sconn->smb2.requests; smb2req; smb2req = nextreq) {
		struct smbd_smb2_lock_state *state = NULL;
		files_struct *fsp_curr = NULL;
		int i = smb2req->current_idx;
		struct blocking_lock_record *blr = NULL;
		const uint8_t *inhdr;

		nextreq = smb2req->next;

		if (smb2req->subreq == NULL) {
			/* This message has been processed. */
			continue;
		}
		if (!tevent_req_is_in_progress(smb2req->subreq)) {
			/* This message has been processed. */
			continue;
		}

		inhdr = (const uint8_t *)smb2req->in.vector[i].iov_base;
		if (SVAL(inhdr, SMB2_HDR_OPCODE) != SMB2_OP_LOCK) {
			/* Not a lock call. */
			continue;
		}

		state = tevent_req_data(smb2req->subreq,
				struct smbd_smb2_lock_state);
		if (!state) {
			/* Strange - is this even possible ? */
			continue;
		}

		fsp_curr = smb2req->compat_chain_fsp;
		if (fsp_curr == NULL) {
			/* Strange - is this even possible ? */
			continue;
		}

		if (fsp_curr != fsp) {
			/* It's not our fid */
			continue;
		}

		blr = state->blr;

		/* Remove the entries from the lock db. */
		brl_lock_cancel(br_lck,
				blr->smblctx,
				sconn_server_id(sconn),
				blr->offset,
				blr->count,
				blr->lock_flav,
				blr);

		/* Finally end the request. */
		if (close_type == SHUTDOWN_CLOSE) {
			tevent_req_done(smb2req->subreq);
		} else {
			tevent_req_nterror(smb2req->subreq,
				NT_STATUS_RANGE_NOT_LOCKED);
		}
	}
}
