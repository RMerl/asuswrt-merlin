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
#include "../lib/tsocket/tsocket.h"
#include "../lib/util/tevent_ntstatus.h"
#include "smbprofile.h"
#include "../lib/util/bitmap.h"

#define OUTVEC_ALLOC_SIZE (SMB2_HDR_BODY + 9)

static const char *smb2_names[] = {
	"SMB2_NEGPROT",
	"SMB2_SESSSETUP",
	"SMB2_LOGOFF",
	"SMB2_TCON",
	"SMB2_TDIS",
	"SMB2_CREATE",
	"SMB2_CLOSE",
	"SMB2_FLUSH",
	"SMB2_READ",
	"SMB2_WRITE",
	"SMB2_LOCK",
	"SMB2_IOCTL",
	"SMB2_CANCEL",
	"SMB2_KEEPALIVE",
	"SMB2_FIND",
	"SMB2_NOTIFY",
	"SMB2_GETINFO",
	"SMB2_SETINFO",
	"SMB2_BREAK"
};

const char *smb2_opcode_name(uint16_t opcode)
{
	if (opcode > 0x12) {
		return "Bad SMB2 opcode";
	}
	return smb2_names[opcode];
}

static void print_req_vectors(struct smbd_smb2_request *req)
{
	int i;

	for (i = 0; i < req->in.vector_count; i++) {
		dbgtext("\treq->in.vector[%u].iov_len = %u\n",
			(unsigned int)i,
			(unsigned int)req->in.vector[i].iov_len);
	}
	for (i = 0; i < req->out.vector_count; i++) {
		dbgtext("\treq->out.vector[%u].iov_len = %u\n",
			(unsigned int)i,
			(unsigned int)req->out.vector[i].iov_len);
	}
}

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
	sconn->smb2.seqnum_low = 0;
	sconn->smb2.seqnum_range = 1;
	sconn->smb2.credits_granted = 1;
	sconn->smb2.max_credits = lp_smb2_max_credits();
	sconn->smb2.credits_bitmap = bitmap_talloc(sconn,
						   sconn->smb2.max_credits);
	if (sconn->smb2.credits_bitmap == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	ret = tstream_bsd_existing_socket(sconn, sconn->sock,
					  &sconn->smb2.stream);
	if (ret == -1) {
		status = map_nt_error_from_unix(errno);
		return status;
	}

	/* Ensure child is set to non-blocking mode */
	set_blocking(sconn->sock, false);
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

#if 0
	/* Enable this to find subtle valgrind errors. */
	mem_pool = talloc_init("smbd_smb2_request_allocate");
#else
	mem_pool = talloc_pool(mem_ctx, 8192);
#endif
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

	req->last_session_id = UINT64_MAX;
	req->last_tid = UINT32_MAX;

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

static bool smb2_validate_sequence_number(struct smbd_server_connection *sconn,
					  uint64_t message_id, uint64_t seq_id)
{
	struct bitmap *credits_bm = sconn->smb2.credits_bitmap;
	unsigned int offset;

	if (seq_id < sconn->smb2.seqnum_low) {
		DEBUG(0,("smb2_validate_sequence_number: bad message_id "
			"%llu (sequence id %llu) "
			"(granted = %u, low = %llu, range = %u)\n",
			(unsigned long long)message_id,
			(unsigned long long)seq_id,
			(unsigned int)sconn->smb2.credits_granted,
			(unsigned long long)sconn->smb2.seqnum_low,
			(unsigned int)sconn->smb2.seqnum_range));
		return false;
	}

	if (seq_id >= sconn->smb2.seqnum_low + sconn->smb2.seqnum_range) {
		DEBUG(0,("smb2_validate_sequence_number: bad message_id "
			"%llu (sequence id %llu) "
			"(granted = %u, low = %llu, range = %u)\n",
			(unsigned long long)message_id,
			(unsigned long long)seq_id,
			(unsigned int)sconn->smb2.credits_granted,
			(unsigned long long)sconn->smb2.seqnum_low,
			(unsigned int)sconn->smb2.seqnum_range));
		return false;
	}

	offset = seq_id % sconn->smb2.max_credits;

	if (bitmap_query(credits_bm, offset)) {
		DEBUG(0,("smb2_validate_sequence_number: duplicate message_id "
			"%llu (sequence id %llu) "
			"(granted = %u, low = %llu, range = %u) "
			"(bm offset %u)\n",
			(unsigned long long)message_id,
			(unsigned long long)seq_id,
			(unsigned int)sconn->smb2.credits_granted,
			(unsigned long long)sconn->smb2.seqnum_low,
			(unsigned int)sconn->smb2.seqnum_range,
			offset));
		return false;
	}

	/* Mark the message_ids as seen in the bitmap. */
	bitmap_set(credits_bm, offset);

	if (seq_id != sconn->smb2.seqnum_low) {
		return true;
	}

	/*
	 * Move the window forward by all the message_id's
	 * already seen.
	 */
	while (bitmap_query(credits_bm, offset)) {
		DEBUG(10,("smb2_validate_sequence_number: clearing "
			  "id %llu (position %u) from bitmap\n",
			  (unsigned long long)(sconn->smb2.seqnum_low),
			  offset));
		bitmap_clear(credits_bm, offset);

		sconn->smb2.seqnum_low += 1;
		sconn->smb2.seqnum_range -= 1;
		offset = sconn->smb2.seqnum_low % sconn->smb2.max_credits;
	}

	return true;
}

static bool smb2_validate_message_id(struct smbd_server_connection *sconn,
				const uint8_t *inhdr)
{
	uint64_t message_id = BVAL(inhdr, SMB2_HDR_MESSAGE_ID);
	uint16_t opcode = IVAL(inhdr, SMB2_HDR_OPCODE);
	bool ok;

	if (opcode == SMB2_OP_CANCEL) {
		/* SMB2_CANCEL requests by definition resend messageids. */
		return true;
	}

	DEBUG(11, ("smb2_validate_message_id: mid %llu, credits_granted %llu, "
		   "seqnum low/range: %llu/%llu\n",
		   (unsigned long long) message_id,
		   (unsigned long long) sconn->smb2.credits_granted,
		   (unsigned long long) sconn->smb2.seqnum_low,
		   (unsigned long long) sconn->smb2.seqnum_range));

	if (sconn->smb2.credits_granted < 1) {
		DEBUG(0, ("smb2_validate_message_id: client used more "
			  "credits than granted, mid %llu, credits_granted %llu, "
			  "seqnum low/range: %llu/%llu\n",
			  (unsigned long long) message_id,
			  (unsigned long long) sconn->smb2.credits_granted,
			  (unsigned long long) sconn->smb2.seqnum_low,
			  (unsigned long long) sconn->smb2.seqnum_range));
		return false;
	}

	ok = smb2_validate_sequence_number(sconn, message_id, message_id);
	if (!ok) {
		return false;
	}

	/* substract used credits */
	sconn->smb2.credits_granted -= 1;

	return true;
}

static NTSTATUS smbd_smb2_request_validate(struct smbd_smb2_request *req)
{
	int count;
	int idx;

	count = req->in.vector_count;

	if (count < 4) {
		/* It's not a SMB2 request */
		return NT_STATUS_INVALID_PARAMETER;
	}

	for (idx=1; idx < count; idx += 3) {
		const uint8_t *inhdr = NULL;

		if (req->in.vector[idx].iov_len != SMB2_HDR_BODY) {
			return NT_STATUS_INVALID_PARAMETER;
		}

		if (req->in.vector[idx+1].iov_len < 2) {
			return NT_STATUS_INVALID_PARAMETER;
		}

		inhdr = (const uint8_t *)req->in.vector[idx].iov_base;

		/* Check the SMB2 header */
		if (IVAL(inhdr, SMB2_HDR_PROTOCOL_ID) != SMB2_MAGIC) {
			return NT_STATUS_INVALID_PARAMETER;
		}

		if (!smb2_validate_message_id(req->sconn, inhdr)) {
			return NT_STATUS_INVALID_PARAMETER;
		}
	}

	return NT_STATUS_OK;
}

static void smb2_set_operation_credit(struct smbd_server_connection *sconn,
			const struct iovec *in_vector,
			struct iovec *out_vector)
{
	const uint8_t *inhdr = (const uint8_t *)in_vector->iov_base;
	uint8_t *outhdr = (uint8_t *)out_vector->iov_base;
	uint16_t credits_requested;
	uint32_t out_flags;
	uint16_t cmd;
	NTSTATUS out_status;
	uint16_t credits_granted = 0;
	uint64_t credits_possible;
	uint16_t current_max_credits;

	/*
	 * first we grant only 1/16th of the max range.
	 *
	 * Windows also starts with the 1/16th and then grants
	 * more later. I was only able to trigger higher
	 * values, when using a verify high credit charge.
	 *
	 * TODO: scale up depending one load, free memory
	 *       or other stuff.
	 *       Maybe also on the relationship between number
	 *       of requests and the used sequence number.
	 *       Which means we would grant more credits
	 *       for client which use multi credit requests.
	 */
	current_max_credits = sconn->smb2.max_credits / 16;
	current_max_credits = MAX(current_max_credits, 1);

	cmd = SVAL(inhdr, SMB2_HDR_OPCODE);
	credits_requested = SVAL(inhdr, SMB2_HDR_CREDIT);
	out_flags = IVAL(outhdr, SMB2_HDR_FLAGS);
	out_status = NT_STATUS(IVAL(outhdr, SMB2_HDR_STATUS));

	SMB_ASSERT(sconn->smb2.max_credits >= sconn->smb2.credits_granted);

	if (out_flags & SMB2_HDR_FLAG_ASYNC) {
		/*
		 * In case we already send an async interim
		 * response, we should not grant
		 * credits on the final response.
		 */
		credits_granted = 0;
	} else if (credits_requested > 0) {
		uint16_t additional_max = 0;
		uint16_t additional_credits = credits_requested - 1;

		switch (cmd) {
		case SMB2_OP_NEGPROT:
			break;
		case SMB2_OP_SESSSETUP:
			/*
			 * Windows 2012 RC1 starts to grant
			 * additional credits
			 * with a successful session setup
			 */
			if (NT_STATUS_IS_OK(out_status)) {
				additional_max = 32;
			}
			break;
		default:
			/*
			 * We match windows and only grant additional credits
			 * in chunks of 32.
			 */
			additional_max = 32;
			break;
		}

		additional_credits = MIN(additional_credits, additional_max);

		credits_granted = 1 + additional_credits;
	} else if (sconn->smb2.credits_granted == 0) {
		/*
		 * Make sure the client has always at least one credit
		 */
		credits_granted = 1;
	}

	/*
	 * sequence numbers should not wrap
	 *
	 * 1. calculate the possible credits until
	 *    the sequence numbers start to wrap on 64-bit.
	 *
	 * 2. UINT64_MAX is used for Break Notifications.
	 *
	 * 2. truncate the possible credits to the maximum
	 *    credits we want to grant to the client in total.
	 *
	 * 3. remove the range we'll already granted to the client
	 *    this makes sure the client consumes the lowest sequence
	 *    number, before we can grant additional credits.
	 */
	credits_possible = UINT64_MAX - sconn->smb2.seqnum_low;
	if (credits_possible > 0) {
		/* remove UINT64_MAX */
		credits_possible -= 1;
	}
	credits_possible = MIN(credits_possible, current_max_credits);
	credits_possible -= sconn->smb2.seqnum_range;

	credits_granted = MIN(credits_granted, credits_possible);

	SSVAL(outhdr, SMB2_HDR_CREDIT, credits_granted);
	sconn->smb2.credits_granted += credits_granted;
	sconn->smb2.seqnum_range += credits_granted;

	DEBUG(10,("smb2_set_operation_credit: requested %u, "
		"granted %u, current possible/max %u/%u, "
		"total granted/max/low/range %u/%u/%llu/%u\n",
		(unsigned int)credits_requested,
		(unsigned int)credits_granted,
		(unsigned int)credits_possible,
		(unsigned int)current_max_credits,
		(unsigned int)sconn->smb2.credits_granted,
		(unsigned int)sconn->smb2.max_credits,
		(unsigned long long)sconn->smb2.seqnum_low,
		(unsigned int)sconn->smb2.seqnum_range));
}

static void smb2_calculate_credits(const struct smbd_smb2_request *inreq,
				struct smbd_smb2_request *outreq)
{
	int count, idx;
	uint16_t total_credits = 0;

	count = outreq->out.vector_count;

	for (idx=1; idx < count; idx += 3) {
		uint8_t *outhdr = (uint8_t *)outreq->out.vector[idx].iov_base;
		smb2_set_operation_credit(outreq->sconn,
			&inreq->in.vector[idx],
			&outreq->out.vector[idx]);
		/* To match Windows, count up what we
		   just granted. */
		total_credits += SVAL(outhdr, SMB2_HDR_CREDIT);
		/* Set to zero in all but the last reply. */
		if (idx + 3 < count) {
			SSVAL(outhdr, SMB2_HDR_CREDIT, 0);
		} else {
			SSVAL(outhdr, SMB2_HDR_CREDIT, total_credits);
		}
	}
}

static NTSTATUS smbd_smb2_request_setup_out(struct smbd_smb2_request *req)
{
	struct iovec *vector;
	int count;
	int idx;

	count = req->in.vector_count;
	vector = talloc_zero_array(req, struct iovec, count);
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
			/* we have a next command -
			 * setup for the error case. */
			next_command_ofs = SMB2_HDR_BODY + 9;
		}

		inhdr = (const uint8_t *)req->in.vector[idx].iov_base;
		in_flags = IVAL(inhdr, SMB2_HDR_FLAGS);

		outhdr = talloc_zero_array(vector, uint8_t,
				      OUTVEC_ALLOC_SIZE);
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
		SSVAL(outhdr, SMB2_HDR_CREDIT_CHARGE,
		      SVAL(inhdr, SMB2_HDR_CREDIT_CHARGE));
		SIVAL(outhdr, SMB2_HDR_STATUS,
		      NT_STATUS_V(NT_STATUS_INTERNAL_ERROR));
		SSVAL(outhdr, SMB2_HDR_OPCODE,
		      SVAL(inhdr, SMB2_HDR_OPCODE));
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
		memcpy(outhdr + SMB2_HDR_SIGNATURE,
		       inhdr + SMB2_HDR_SIGNATURE, 16);

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

static bool dup_smb2_vec3(TALLOC_CTX *ctx,
			struct iovec *outvec,
			const struct iovec *srcvec)
{
	/* vec[0] is always boilerplate and must
	 * be allocated with size OUTVEC_ALLOC_SIZE. */

	outvec[0].iov_base = talloc_memdup(ctx,
				srcvec[0].iov_base,
				OUTVEC_ALLOC_SIZE);
	if (!outvec[0].iov_base) {
		return false;
	}
	outvec[0].iov_len = SMB2_HDR_BODY;

	/*
	 * If this is a "standard" vec[1] of length 8,
	 * pointing to srcvec[0].iov_base + SMB2_HDR_BODY,
	 * then duplicate this. Else use talloc_memdup().
	 */

	if (srcvec[1].iov_len == 8 &&
			srcvec[1].iov_base ==
				((uint8_t *)srcvec[0].iov_base) +
					SMB2_HDR_BODY) {
		outvec[1].iov_base = ((uint8_t *)outvec[0].iov_base) +
					SMB2_HDR_BODY;
		outvec[1].iov_len = 8;
	} else {
		outvec[1].iov_base = talloc_memdup(ctx,
				srcvec[1].iov_base,
				srcvec[1].iov_len);
		if (!outvec[1].iov_base) {
			return false;
		}
		outvec[1].iov_len = srcvec[1].iov_len;
	}

	/*
	 * If this is a "standard" vec[2] of length 1,
	 * pointing to srcvec[0].iov_base + (OUTVEC_ALLOC_SIZE - 1)
	 * then duplicate this. Else use talloc_memdup().
	 */

	if (srcvec[2].iov_base &&
			srcvec[2].iov_len) {
		if (srcvec[2].iov_base ==
				((uint8_t *)srcvec[0].iov_base) +
					(OUTVEC_ALLOC_SIZE - 1) &&
				srcvec[2].iov_len == 1) {
			/* Common SMB2 error packet case. */
			outvec[2].iov_base = ((uint8_t *)outvec[0].iov_base) +
				(OUTVEC_ALLOC_SIZE - 1);
		} else {
			outvec[2].iov_base = talloc_memdup(ctx,
					srcvec[2].iov_base,
					srcvec[2].iov_len);
			if (!outvec[2].iov_base) {
				return false;
			}
		}
		outvec[2].iov_len = srcvec[2].iov_len;
	} else {
		outvec[2].iov_base = NULL;
		outvec[2].iov_len = 0;
	}
	return true;
}

static struct smbd_smb2_request *dup_smb2_req(const struct smbd_smb2_request *req)
{
	struct smbd_smb2_request *newreq = NULL;
	struct iovec *outvec = NULL;
	int count = req->out.vector_count;
	int i;

	newreq = smbd_smb2_request_allocate(req->sconn);
	if (!newreq) {
		return NULL;
	}

	newreq->sconn = req->sconn;
	newreq->session = req->session;
	newreq->do_signing = req->do_signing;
	newreq->current_idx = req->current_idx;
	newreq->async = false;
	newreq->cancelled = false;
	/* Note we are leaving:
		->tcon
		->smb1req
		->compat_chain_fsp
	   uninitialized as NULL here as
	   they're not used in the interim
	   response code. JRA. */

	outvec = talloc_zero_array(newreq, struct iovec, count);
	if (!outvec) {
		TALLOC_FREE(newreq);
		return NULL;
	}
	newreq->out.vector = outvec;
	newreq->out.vector_count = count;

	/* Setup the outvec's identically to req. */
	outvec[0].iov_base = newreq->out.nbt_hdr;
	outvec[0].iov_len = 4;
	memcpy(newreq->out.nbt_hdr, req->out.nbt_hdr, 4);

	/* Setup the vectors identically to the ones in req. */
	for (i = 1; i < count; i += 3) {
		if (!dup_smb2_vec3(outvec, &outvec[i], &req->out.vector[i])) {
			break;
		}
	}

	if (i < count) {
		/* Alloc failed. */
		TALLOC_FREE(newreq);
		return NULL;
	}

	smb2_setup_nbt_length(newreq->out.vector,
		newreq->out.vector_count);

	return newreq;
}

static void smbd_smb2_request_writev_done(struct tevent_req *subreq);

static NTSTATUS smb2_send_async_interim_response(const struct smbd_smb2_request *req)
{
	int i = 0;
	uint8_t *outhdr = NULL;
	struct smbd_smb2_request *nreq = NULL;

	/* Create a new smb2 request we'll use
	   for the interim return. */
	nreq = dup_smb2_req(req);
	if (!nreq) {
		return NT_STATUS_NO_MEMORY;
	}

	/* Lose the last 3 out vectors. They're the
	   ones we'll be using for the async reply. */
	nreq->out.vector_count -= 3;

	smb2_setup_nbt_length(nreq->out.vector,
		nreq->out.vector_count);

	/* Step back to the previous reply. */
	i = nreq->current_idx - 3;
	outhdr = (uint8_t *)nreq->out.vector[i].iov_base;
	/* And end the chain. */
	SIVAL(outhdr, SMB2_HDR_NEXT_COMMAND, 0);

	/* Calculate outgoing credits */
	smb2_calculate_credits(req, nreq);

	/* Re-sign if needed. */
	if (nreq->do_signing) {
		NTSTATUS status;
		status = smb2_signing_sign_pdu(nreq->session->session_key,
					&nreq->out.vector[i], 3);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
	}
	if (DEBUGLEVEL >= 10) {
		dbgtext("smb2_send_async_interim_response: nreq->current_idx = %u\n",
			(unsigned int)nreq->current_idx );
		dbgtext("smb2_send_async_interim_response: returning %u vectors\n",
			(unsigned int)nreq->out.vector_count );
		print_req_vectors(nreq);
	}
	nreq->subreq = tstream_writev_queue_send(nreq,
					nreq->sconn->smb2.event_ctx,
					nreq->sconn->smb2.stream,
					nreq->sconn->smb2.send_queue,
					nreq->out.vector,
					nreq->out.vector_count);

	if (nreq->subreq == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	tevent_req_set_callback(nreq->subreq,
			smbd_smb2_request_writev_done,
			nreq);

	return NT_STATUS_OK;
}

struct smbd_smb2_request_pending_state {
        struct smbd_server_connection *sconn;
        uint8_t buf[4 + SMB2_HDR_BODY + 0x08 + 1];
        struct iovec vector[3];
};

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

NTSTATUS smbd_smb2_request_pending_queue(struct smbd_smb2_request *req,
					 struct tevent_req *subreq)
{
	NTSTATUS status;
	struct smbd_smb2_request_pending_state *state = NULL;
	int i = req->current_idx;
	uint8_t *reqhdr = NULL;
	uint8_t *hdr = NULL;
	uint8_t *body = NULL;
	uint32_t flags = 0;
	uint64_t message_id = 0;
	uint64_t async_id = 0;

	if (!tevent_req_is_in_progress(subreq)) {
		return NT_STATUS_OK;
	}

	req->subreq = subreq;
	subreq = NULL;

	if (req->async) {
		/* We're already async. */
		return NT_STATUS_OK;
	}

	if (req->in.vector_count > i + 3) {
		/*
		 * We're trying to go async in a compound
		 * request chain.
		 * This is only allowed for opens that
		 * cause an oplock break, otherwise it
		 * is not allowed. See [MS-SMB2].pdf
		 * note <194> on Section 3.3.5.2.7.
		 */
		const uint8_t *inhdr =
			(const uint8_t *)req->in.vector[i].iov_base;

		if (SVAL(inhdr, SMB2_HDR_OPCODE) != SMB2_OP_CREATE) {
			/*
			 * Cancel the outstanding request.
			 */
			bool ok = tevent_req_cancel(req->subreq);
			if (ok) {
				return NT_STATUS_OK;
			}
			TALLOC_FREE(req->subreq);
			return smbd_smb2_request_error(req,
				NT_STATUS_INTERNAL_ERROR);
                }
	}

	if (DEBUGLEVEL >= 10) {
		dbgtext("smbd_smb2_request_pending_queue: req->current_idx = %u\n",
			(unsigned int)req->current_idx );
		print_req_vectors(req);
	}

	if (i > 1) {
		/*
		 * We're going async in a compound
		 * chain after the first request has
		 * already been processed. Send an
		 * interim response containing the
		 * set of replies already generated.
		 */
		status = smb2_send_async_interim_response(req);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}

		req->current_idx = 1;

		/*
		 * Re-arrange the in.vectors to remove what
		 * we just sent.
		 */
		memmove(&req->in.vector[1],
			&req->in.vector[i],
			sizeof(req->in.vector[0])*(req->in.vector_count - i));
		req->in.vector_count = 1 + (req->in.vector_count - i);

		smb2_setup_nbt_length(req->in.vector, req->in.vector_count);

		/* Re-arrange the out.vectors to match. */
		memmove(&req->out.vector[1],
			&req->out.vector[i],
			sizeof(req->out.vector[0])*(req->out.vector_count - i));
		req->out.vector_count = 1 + (req->out.vector_count - i);

		if (req->in.vector_count == 4) {
			uint8_t *outhdr = (uint8_t *)req->out.vector[i].iov_base;
			/*
			 * We only have one remaining request as
			 * we've processed everything else.
			 * This is no longer a compound request.
			 */
			req->compound_related = false;
			flags = (IVAL(outhdr, SMB2_HDR_FLAGS) & ~SMB2_HDR_FLAG_CHAINED);
			SIVAL(outhdr, SMB2_HDR_FLAGS, flags);
		}
	}

	/* Don't return an intermediate packet on a pipe read/write. */
	if (req->tcon && req->tcon->compat_conn && IS_IPC(req->tcon->compat_conn)) {
		goto out;
	}

	reqhdr = (uint8_t *)req->out.vector[i].iov_base;
	flags = (IVAL(reqhdr, SMB2_HDR_FLAGS) & ~SMB2_HDR_FLAG_CHAINED);
	message_id = BVAL(reqhdr, SMB2_HDR_MESSAGE_ID);
	async_id = message_id; /* keep it simple for now... */

	/*
	 * What we send is identical to a smbd_smb2_request_error
	 * packet with an error status of STATUS_PENDING. Make use
	 * of this fact sometime when refactoring. JRA.
	 */

	state = talloc_zero(req->sconn, struct smbd_smb2_request_pending_state);
	if (state == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	state->sconn = req->sconn;

	state->vector[0].iov_base = (void *)state->buf;
	state->vector[0].iov_len = 4;

	state->vector[1].iov_base = state->buf + 4;
	state->vector[1].iov_len = SMB2_HDR_BODY;

	state->vector[2].iov_base = state->buf + 4 + SMB2_HDR_BODY;
	state->vector[2].iov_len = 9;

	smb2_setup_nbt_length(state->vector, 3);

	hdr = (uint8_t *)state->vector[1].iov_base;
	body = (uint8_t *)state->vector[2].iov_base;

	SIVAL(hdr, SMB2_HDR_PROTOCOL_ID, SMB2_MAGIC);
	SSVAL(hdr, SMB2_HDR_LENGTH, SMB2_HDR_BODY);
	SSVAL(hdr, SMB2_HDR_EPOCH, 0);
	SIVAL(hdr, SMB2_HDR_STATUS, NT_STATUS_V(STATUS_PENDING));
	SSVAL(hdr, SMB2_HDR_OPCODE, SVAL(reqhdr, SMB2_HDR_OPCODE));

	SIVAL(hdr, SMB2_HDR_FLAGS, flags);
	SIVAL(hdr, SMB2_HDR_NEXT_COMMAND, 0);
	SBVAL(hdr, SMB2_HDR_MESSAGE_ID, message_id);
	SBVAL(hdr, SMB2_HDR_PID, async_id);
	SBVAL(hdr, SMB2_HDR_SESSION_ID,
		BVAL(reqhdr, SMB2_HDR_SESSION_ID));
	memset(hdr+SMB2_HDR_SIGNATURE, 0, 16);

	SSVAL(body, 0x00, 0x08 + 1);

	SCVAL(body, 0x02, 0);
	SCVAL(body, 0x03, 0);
	SIVAL(body, 0x04, 0);
	/* Match W2K8R2... */
	SCVAL(body, 0x08, 0x21);

	/* Ensure we correctly go through crediting. Grant
	   the credits now, and zero credits on the final
	   response. */
	smb2_set_operation_credit(req->sconn,
			&req->in.vector[i],
			&state->vector[1]);

	SIVAL(hdr, SMB2_HDR_FLAGS, flags | SMB2_HDR_FLAG_ASYNC);

	if (req->do_signing) {
		status = smb2_signing_sign_pdu(req->session->session_key,
					&state->vector[1], 2);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
	}

	subreq = tstream_writev_queue_send(state,
					req->sconn->smb2.event_ctx,
					req->sconn->smb2.stream,
					req->sconn->smb2.send_queue,
					state->vector,
					3);

	if (subreq == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	tevent_req_set_callback(subreq,
			smbd_smb2_request_pending_writev_done,
			state);

	/* Note we're going async with this request. */
	req->async = true;

  out:

	smb2_setup_nbt_length(req->out.vector,
		req->out.vector_count);

	if (req->async) {
		/* Ensure our final reply matches the interim one. */
		reqhdr = (uint8_t *)req->out.vector[1].iov_base;
		SIVAL(reqhdr, SMB2_HDR_FLAGS, flags | SMB2_HDR_FLAG_ASYNC);
		SBVAL(reqhdr, SMB2_HDR_PID, async_id);

		{
			const uint8_t *inhdr =
				(const uint8_t *)req->in.vector[1].iov_base;
			DEBUG(10,("smbd_smb2_request_pending_queue: opcode[%s] mid %llu "
				"going async\n",
				smb2_opcode_name((uint16_t)IVAL(inhdr, SMB2_HDR_OPCODE)),
				(unsigned long long)async_id ));
		}
	}

	return NT_STATUS_OK;
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
	uint64_t found_id;

	inhdr = (const uint8_t *)req->in.vector[i].iov_base;

	flags = IVAL(inhdr, SMB2_HDR_FLAGS);
	search_message_id = BVAL(inhdr, SMB2_HDR_MESSAGE_ID);
	search_async_id = BVAL(inhdr, SMB2_HDR_PID);

	/*
	 * we don't need the request anymore
	 * cancel requests never have a response
	 */
	DLIST_REMOVE(req->sconn->smb2.requests, req);
	TALLOC_FREE(req);

	for (cur = sconn->smb2.requests; cur; cur = cur->next) {
		const uint8_t *outhdr;
		uint64_t message_id;
		uint64_t async_id;

		if (cur->compound_related) {
			/*
			 * Never cancel anything in a compound request.
			 * Way too hard to deal with the result.
			 */
			continue;
		}

		i = cur->current_idx;

		outhdr = (const uint8_t *)cur->out.vector[i].iov_base;

		message_id = BVAL(outhdr, SMB2_HDR_MESSAGE_ID);
		async_id = BVAL(outhdr, SMB2_HDR_PID);

		if (flags & SMB2_HDR_FLAG_ASYNC) {
			if (search_async_id == async_id) {
				found_id = async_id;
				break;
			}
		} else {
			if (search_message_id == message_id) {
				found_id = message_id;
				break;
			}
		}
	}

	if (cur && cur->subreq) {
		inhdr = (const uint8_t *)cur->in.vector[i].iov_base;
		DEBUG(10,("smbd_smb2_request_process_cancel: attempting to "
			"cancel opcode[%s] mid %llu\n",
			smb2_opcode_name((uint16_t)IVAL(inhdr, SMB2_HDR_OPCODE)),
                        (unsigned long long)found_id ));
		tevent_req_cancel(cur->subreq);
	}

	return NT_STATUS_OK;
}

NTSTATUS smbd_smb2_request_verify_sizes(struct smbd_smb2_request *req,
					size_t expected_body_size)
{
	const uint8_t *inhdr;
	uint16_t opcode;
	const uint8_t *inbody;
	int i = req->current_idx;
	size_t body_size;
	size_t min_dyn_size = expected_body_size & 0x00000001;

	/*
	 * The following should be checked already.
	 */
	if ((i+2) > req->in.vector_count) {
		return NT_STATUS_INTERNAL_ERROR;
	}
	if (req->in.vector[i+0].iov_len != SMB2_HDR_BODY) {
		return NT_STATUS_INTERNAL_ERROR;
	}
	if (req->in.vector[i+1].iov_len < 2) {
		return NT_STATUS_INTERNAL_ERROR;
	}

	inhdr = (const uint8_t *)req->in.vector[i+0].iov_base;
	opcode = SVAL(inhdr, SMB2_HDR_OPCODE);

	switch (opcode) {
	case SMB2_OP_IOCTL:
	case SMB2_OP_GETINFO:
		min_dyn_size = 0;
		break;
	}

	/*
	 * Now check the expected body size,
	 * where the last byte might be in the
	 * dynnamic section..
	 */
	if (req->in.vector[i+1].iov_len != (expected_body_size & 0xFFFFFFFE)) {
		return NT_STATUS_INVALID_PARAMETER;
	}
	if (req->in.vector[i+2].iov_len < min_dyn_size) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	inbody = (const uint8_t *)req->in.vector[i+1].iov_base;

	body_size = SVAL(inbody, 0x00);
	if (body_size != expected_body_size) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	return NT_STATUS_OK;
}

NTSTATUS smbd_smb2_request_dispatch(struct smbd_smb2_request *req)
{
	const uint8_t *inhdr;
	int i = req->current_idx;
	uint16_t opcode;
	uint32_t flags;
	uint64_t mid;
	NTSTATUS status;
	NTSTATUS session_status;
	uint32_t allowed_flags;
	NTSTATUS return_value;

	inhdr = (const uint8_t *)req->in.vector[i].iov_base;

	/* TODO: verify more things */

	flags = IVAL(inhdr, SMB2_HDR_FLAGS);
	opcode = IVAL(inhdr, SMB2_HDR_OPCODE);
	mid = BVAL(inhdr, SMB2_HDR_MESSAGE_ID);
	DEBUG(10,("smbd_smb2_request_dispatch: opcode[%s] mid = %llu\n",
		smb2_opcode_name(opcode),
		(unsigned long long)mid));

	if (get_Protocol() >= PROTOCOL_SMB2) {
		/*
		 * once the protocol is negotiated
		 * SMB2_OP_NEGPROT is not allowed anymore
		 */
		if (opcode == SMB2_OP_NEGPROT) {
			/* drop the connection */
			return NT_STATUS_INVALID_PARAMETER;
		}
	} else {
		/*
		 * if the protocol is not negotiated yet
		 * only SMB2_OP_NEGPROT is allowed.
		 */
		if (opcode != SMB2_OP_NEGPROT) {
			/* drop the connection */
			return NT_STATUS_INVALID_PARAMETER;
		}
	}

	allowed_flags = SMB2_HDR_FLAG_CHAINED |
			SMB2_HDR_FLAG_SIGNED |
			SMB2_HDR_FLAG_DFS;
	if (opcode == SMB2_OP_CANCEL) {
		allowed_flags |= SMB2_HDR_FLAG_ASYNC;
	}
	if ((flags & ~allowed_flags) != 0) {
		return smbd_smb2_request_error(req, NT_STATUS_INVALID_PARAMETER);
	}

	/*
	 * Check if the client provided a valid session id,
	 * if so smbd_smb2_request_check_session() calls
	 * set_current_user_info().
	 *
	 * As some command don't require a valid session id
	 * we defer the check of the session_status
	 */
	session_status = smbd_smb2_request_check_session(req);

	if (flags & SMB2_HDR_FLAG_CHAINED) {
		/*
		 * This check is mostly for giving the correct error code
		 * for compounded requests.
		 */
		if (!NT_STATUS_IS_OK(session_status)) {
			return smbd_smb2_request_error(req, NT_STATUS_INVALID_PARAMETER);
		}
	} else {
		req->compat_chain_fsp = NULL;
	}

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
	} else if (opcode == SMB2_OP_CANCEL) {
		/* Cancel requests are allowed to skip the signing */
	} else if (req->session && req->session->do_signing) {
		return smbd_smb2_request_error(req, NT_STATUS_ACCESS_DENIED);
	}

	if (flags & SMB2_HDR_FLAG_CHAINED) {
		req->compound_related = true;
	}

	switch (opcode) {
	case SMB2_OP_NEGPROT:
		/* This call needs to be run as root */
		change_to_root_user();

		{
			START_PROFILE(smb2_negprot);
			return_value = smbd_smb2_request_process_negprot(req);
			END_PROFILE(smb2_negprot);
		}
		break;

	case SMB2_OP_SESSSETUP:
		/* This call needs to be run as root */
		change_to_root_user();

		{
			START_PROFILE(smb2_sesssetup);
			return_value = smbd_smb2_request_process_sesssetup(req);
			END_PROFILE(smb2_sesssetup);
		}
		break;

	case SMB2_OP_LOGOFF:
		if (!NT_STATUS_IS_OK(session_status)) {
			return_value = smbd_smb2_request_error(req, session_status);
			break;
		}

		/* This call needs to be run as root */
		change_to_root_user();

		{
			START_PROFILE(smb2_logoff);
			return_value = smbd_smb2_request_process_logoff(req);
			END_PROFILE(smb2_logoff);
		}
		break;

	case SMB2_OP_TCON:
		if (!NT_STATUS_IS_OK(session_status)) {
			return_value = smbd_smb2_request_error(req, session_status);
			break;
		}

		/*
		 * This call needs to be run as root.
		 *
		 * smbd_smb2_request_process_tcon()
		 * calls make_connection_snum(), which will call
		 * change_to_user(), when needed.
		 */
		change_to_root_user();

		{
			START_PROFILE(smb2_tcon);
			return_value = smbd_smb2_request_process_tcon(req);
			END_PROFILE(smb2_tcon);
		}
		break;

	case SMB2_OP_TDIS:
		if (!NT_STATUS_IS_OK(session_status)) {
			return_value = smbd_smb2_request_error(req, session_status);
			break;
		}
		/*
		 * This call needs to be run as user.
		 *
		 * smbd_smb2_request_check_tcon()
		 * calls change_to_user() on success.
		 */
		status = smbd_smb2_request_check_tcon(req);
		if (!NT_STATUS_IS_OK(status)) {
			return_value = smbd_smb2_request_error(req, status);
			break;
		}
		/* This call needs to be run as root */
		change_to_root_user();


		{
			START_PROFILE(smb2_tdis);
			return_value = smbd_smb2_request_process_tdis(req);
			END_PROFILE(smb2_tdis);
		}
		break;

	case SMB2_OP_CREATE:
		if (!NT_STATUS_IS_OK(session_status)) {
			return_value = smbd_smb2_request_error(req, session_status);
			break;
		}
		/*
		 * This call needs to be run as user.
		 *
		 * smbd_smb2_request_check_tcon()
		 * calls change_to_user() on success.
		 */
		status = smbd_smb2_request_check_tcon(req);
		if (!NT_STATUS_IS_OK(status)) {
			return_value = smbd_smb2_request_error(req, status);
			break;
		}

		{
			START_PROFILE(smb2_create);
			return_value = smbd_smb2_request_process_create(req);
			END_PROFILE(smb2_create);
		}
		break;

	case SMB2_OP_CLOSE:
		if (!NT_STATUS_IS_OK(session_status)) {
			return_value = smbd_smb2_request_error(req, session_status);
			break;
		}
		/*
		 * This call needs to be run as user.
		 *
		 * smbd_smb2_request_check_tcon()
		 * calls change_to_user() on success.
		 */
		status = smbd_smb2_request_check_tcon(req);
		if (!NT_STATUS_IS_OK(status)) {
			return_value = smbd_smb2_request_error(req, status);
			break;
		}

		{
			START_PROFILE(smb2_close);
			return_value = smbd_smb2_request_process_close(req);
			END_PROFILE(smb2_close);
		}
		break;

	case SMB2_OP_FLUSH:
		if (!NT_STATUS_IS_OK(session_status)) {
			return_value = smbd_smb2_request_error(req, session_status);
			break;
		}
		/*
		 * This call needs to be run as user.
		 *
		 * smbd_smb2_request_check_tcon()
		 * calls change_to_user() on success.
		 */
		status = smbd_smb2_request_check_tcon(req);
		if (!NT_STATUS_IS_OK(status)) {
			return_value = smbd_smb2_request_error(req, status);
			break;
		}

		{
			START_PROFILE(smb2_flush);
			return_value = smbd_smb2_request_process_flush(req);
			END_PROFILE(smb2_flush);
		}
		break;

	case SMB2_OP_READ:
		if (!NT_STATUS_IS_OK(session_status)) {
			return_value = smbd_smb2_request_error(req, session_status);
			break;
		}
		/*
		 * This call needs to be run as user.
		 *
		 * smbd_smb2_request_check_tcon()
		 * calls change_to_user() on success.
		 */
		status = smbd_smb2_request_check_tcon(req);
		if (!NT_STATUS_IS_OK(status)) {
			return_value = smbd_smb2_request_error(req, status);
			break;
		}

		{
			START_PROFILE(smb2_read);
			return_value = smbd_smb2_request_process_read(req);
			END_PROFILE(smb2_read);
		}
		break;

	case SMB2_OP_WRITE:
		if (!NT_STATUS_IS_OK(session_status)) {
			return_value = smbd_smb2_request_error(req, session_status);
			break;
		}
		/*
		 * This call needs to be run as user.
		 *
		 * smbd_smb2_request_check_tcon()
		 * calls change_to_user() on success.
		 */
		status = smbd_smb2_request_check_tcon(req);
		if (!NT_STATUS_IS_OK(status)) {
			return_value = smbd_smb2_request_error(req, status);
			break;
		}

		{
			START_PROFILE(smb2_write);
			return_value = smbd_smb2_request_process_write(req);
			END_PROFILE(smb2_write);
		}
		break;

	case SMB2_OP_LOCK:
		if (!NT_STATUS_IS_OK(session_status)) {
			/* Too ugly to live ? JRA. */
			if (NT_STATUS_EQUAL(session_status,NT_STATUS_USER_SESSION_DELETED)) {
				session_status = NT_STATUS_FILE_CLOSED;
			}
			return_value = smbd_smb2_request_error(req, session_status);
			break;
		}
		/*
		 * This call needs to be run as user.
		 *
		 * smbd_smb2_request_check_tcon()
		 * calls change_to_user() on success.
		 */
		status = smbd_smb2_request_check_tcon(req);
		if (!NT_STATUS_IS_OK(status)) {
			/* Too ugly to live ? JRA. */
			if (NT_STATUS_EQUAL(status,NT_STATUS_NETWORK_NAME_DELETED)) {
				status = NT_STATUS_FILE_CLOSED;
			}
			return_value = smbd_smb2_request_error(req, status);
			break;
		}

		{
			START_PROFILE(smb2_lock);
			return_value = smbd_smb2_request_process_lock(req);
			END_PROFILE(smb2_lock);
		}
		break;

	case SMB2_OP_IOCTL:
		if (!NT_STATUS_IS_OK(session_status)) {
			return_value = smbd_smb2_request_error(req, session_status);
			break;
		}
		/*
		 * This call needs to be run as user.
		 *
		 * smbd_smb2_request_check_tcon()
		 * calls change_to_user() on success.
		 */
		status = smbd_smb2_request_check_tcon(req);
		if (!NT_STATUS_IS_OK(status)) {
			return_value = smbd_smb2_request_error(req, status);
			break;
		}

		{
			START_PROFILE(smb2_ioctl);
			return_value = smbd_smb2_request_process_ioctl(req);
			END_PROFILE(smb2_ioctl);
		}
		break;

	case SMB2_OP_CANCEL:
		/*
		 * This call needs to be run as root
		 *
		 * That is what we also do in the SMB1 case.
		 */
		change_to_root_user();

		{
			START_PROFILE(smb2_cancel);
			return_value = smbd_smb2_request_process_cancel(req);
			END_PROFILE(smb2_cancel);
		}
		break;

	case SMB2_OP_KEEPALIVE:
		/* This call needs to be run as root */
		change_to_root_user();

		{
			START_PROFILE(smb2_keepalive);
			return_value = smbd_smb2_request_process_keepalive(req);
			END_PROFILE(smb2_keepalive);
		}
		break;

	case SMB2_OP_FIND:
		if (!NT_STATUS_IS_OK(session_status)) {
			return_value = smbd_smb2_request_error(req, session_status);
			break;
		}
		/*
		 * This call needs to be run as user.
		 *
		 * smbd_smb2_request_check_tcon()
		 * calls change_to_user() on success.
		 */
		status = smbd_smb2_request_check_tcon(req);
		if (!NT_STATUS_IS_OK(status)) {
			return_value = smbd_smb2_request_error(req, status);
			break;
		}

		{
			START_PROFILE(smb2_find);
			return_value = smbd_smb2_request_process_find(req);
			END_PROFILE(smb2_find);
		}
		break;

	case SMB2_OP_NOTIFY:
		if (!NT_STATUS_IS_OK(session_status)) {
			return_value = smbd_smb2_request_error(req, session_status);
			break;
		}
		/*
		 * This call needs to be run as user.
		 *
		 * smbd_smb2_request_check_tcon()
		 * calls change_to_user() on success.
		 */
		status = smbd_smb2_request_check_tcon(req);
		if (!NT_STATUS_IS_OK(status)) {
			return_value = smbd_smb2_request_error(req, status);
			break;
		}

		{
			START_PROFILE(smb2_notify);
			return_value = smbd_smb2_request_process_notify(req);
			END_PROFILE(smb2_notify);
		}
		break;

	case SMB2_OP_GETINFO:
		if (!NT_STATUS_IS_OK(session_status)) {
			return_value = smbd_smb2_request_error(req, session_status);
			break;
		}
		/*
		 * This call needs to be run as user.
		 *
		 * smbd_smb2_request_check_tcon()
		 * calls change_to_user() on success.
		 */
		status = smbd_smb2_request_check_tcon(req);
		if (!NT_STATUS_IS_OK(status)) {
			return_value = smbd_smb2_request_error(req, status);
			break;
		}

		{
			START_PROFILE(smb2_getinfo);
			return_value = smbd_smb2_request_process_getinfo(req);
			END_PROFILE(smb2_getinfo);
		}
		break;

	case SMB2_OP_SETINFO:
		if (!NT_STATUS_IS_OK(session_status)) {
			return_value = smbd_smb2_request_error(req, session_status);
			break;
		}
		/*
		 * This call needs to be run as user.
		 *
		 * smbd_smb2_request_check_tcon()
		 * calls change_to_user() on success.
		 */
		status = smbd_smb2_request_check_tcon(req);
		if (!NT_STATUS_IS_OK(status)) {
			return_value = smbd_smb2_request_error(req, status);
			break;
		}

		{
			START_PROFILE(smb2_setinfo);
			return_value = smbd_smb2_request_process_setinfo(req);
			END_PROFILE(smb2_setinfo);
		}
		break;

	case SMB2_OP_BREAK:
		if (!NT_STATUS_IS_OK(session_status)) {
			return_value = smbd_smb2_request_error(req, session_status);
			break;
		}
		/*
		 * This call needs to be run as user.
		 *
		 * smbd_smb2_request_check_tcon()
		 * calls change_to_user() on success.
		 */
		status = smbd_smb2_request_check_tcon(req);
		if (!NT_STATUS_IS_OK(status)) {
			return_value = smbd_smb2_request_error(req, status);
			break;
		}

		{
			START_PROFILE(smb2_break);
			return_value = smbd_smb2_request_process_break(req);
			END_PROFILE(smb2_break);
		}
		break;

	default:
		return_value = smbd_smb2_request_error(req, NT_STATUS_INVALID_PARAMETER);
		break;
	}
	return return_value;
}

static NTSTATUS smbd_smb2_request_reply(struct smbd_smb2_request *req)
{
	struct tevent_req *subreq;
	int i = req->current_idx;

	req->subreq = NULL;

	req->current_idx += 3;

	if (req->current_idx < req->out.vector_count) {
		/*
		 * We must process the remaining compound
		 * SMB2 requests before any new incoming SMB2
		 * requests. This is because incoming SMB2
		 * requests may include a cancel for a
		 * compound request we haven't processed
		 * yet.
		 */
		struct tevent_immediate *im = tevent_create_immediate(req);
		if (!im) {
			return NT_STATUS_NO_MEMORY;
		}

		if (req->do_signing) {
			/*
			 * We sign each reply as we go along.
			 * We can do this as smb2_calculate_credits()
			 * grants zero credits on every part of a
			 * compound reply except the last one,
			 * which is signed just before calling
			 * tstream_writev_queue_send().
			 */
			NTSTATUS status;
			status = smb2_signing_sign_pdu(req->session->session_key,
					       &req->out.vector[i], 3);
			if (!NT_STATUS_IS_OK(status)) {
				TALLOC_FREE(im);
				return status;
			}
		}

		tevent_schedule_immediate(im,
					req->sconn->smb2.event_ctx,
					smbd_smb2_request_dispatch_immediate,
					req);
		return NT_STATUS_OK;
	}

	if (req->compound_related) {
		req->compound_related = false;
	}

	smb2_setup_nbt_length(req->out.vector, req->out.vector_count);

	/* Set credit for these operations (zero credits if this
	   is a final reply for an async operation). */
	smb2_calculate_credits(req, req);

	if (req->do_signing) {
		NTSTATUS status;
		status = smb2_signing_sign_pdu(req->session->session_key,
					       &req->out.vector[i], 3);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
	}

	if (DEBUGLEVEL >= 10) {
		dbgtext("smbd_smb2_request_reply: sending...\n");
		print_req_vectors(req);
	}

	/* I am a sick, sick man... :-). Sendfile hack ... JRA. */
	if (req->out.vector_count == 4 &&
			req->out.vector[3].iov_base == NULL &&
			req->out.vector[3].iov_len != 0) {
		/* Dynamic part is NULL. Chop it off,
		   We're going to send it via sendfile. */
		req->out.vector_count -= 1;
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
	/*
	 * We're done with this request -
	 * move it off the "being processed" queue.
	 */
	DLIST_REMOVE(req->sconn->smb2.requests, req);

	return NT_STATUS_OK;
}

static NTSTATUS smbd_smb2_request_next_incoming(struct smbd_server_connection *sconn);

void smbd_smb2_request_dispatch_immediate(struct tevent_context *ctx,
					struct tevent_immediate *im,
					void *private_data)
{
	struct smbd_smb2_request *req = talloc_get_type_abort(private_data,
					struct smbd_smb2_request);
	struct smbd_server_connection *sconn = req->sconn;
	NTSTATUS status;

	TALLOC_FREE(im);

	if (DEBUGLEVEL >= 10) {
		DEBUG(10,("smbd_smb2_request_dispatch_immediate: idx[%d] of %d vectors\n",
			req->current_idx, req->in.vector_count));
		print_req_vectors(req);
	}

	status = smbd_smb2_request_dispatch(req);
	if (!NT_STATUS_IS_OK(status)) {
		smbd_server_connection_terminate(sconn, nt_errstr(status));
		return;
	}

	status = smbd_smb2_request_next_incoming(sconn);
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
	NTSTATUS status;

	ret = tstream_writev_queue_recv(subreq, &sys_errno);
	TALLOC_FREE(subreq);
	TALLOC_FREE(req);
	if (ret == -1) {
		status = map_nt_error_from_unix(sys_errno);
		DEBUG(2,("smbd_smb2_request_writev_done: client write error %s\n",
			nt_errstr(status)));
		smbd_server_connection_terminate(sconn, nt_errstr(status));
		return;
	}

	status = smbd_smb2_request_next_incoming(sconn);
	if (!NT_STATUS_IS_OK(status)) {
		smbd_server_connection_terminate(sconn, nt_errstr(status));
		return;
	}
}

NTSTATUS smbd_smb2_request_done_ex(struct smbd_smb2_request *req,
				   NTSTATUS status,
				   DATA_BLOB body, DATA_BLOB *dyn,
				   const char *location)
{
	uint8_t *outhdr;
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
			new_dyn = talloc_zero_array(req->out.vector,
					       uint8_t, new_size);
			if (new_dyn == NULL) {
				return smbd_smb2_request_error(req,
						NT_STATUS_NO_MEMORY);
			}

			memcpy(new_dyn, old_dyn, old_size);
			memset(new_dyn + old_size, 0, pad_size);

			req->out.vector[i+2].iov_base = (void *)new_dyn;
			req->out.vector[i+2].iov_len = new_size;
		}
		next_command_ofs += pad_size;
	}

	SIVAL(outhdr, SMB2_HDR_NEXT_COMMAND, next_command_ofs);

	return smbd_smb2_request_reply(req);
}

NTSTATUS smbd_smb2_request_error_ex(struct smbd_smb2_request *req,
				    NTSTATUS status,
				    DATA_BLOB *info,
				    const char *location)
{
	DATA_BLOB body;
	int i = req->current_idx;
	uint8_t *outhdr = (uint8_t *)req->out.vector[i].iov_base;

	DEBUG(10,("smbd_smb2_request_error_ex: idx[%d] status[%s] |%s| at %s\n",
		  i, nt_errstr(status), info ? " +info" : "",
		  location));

	body.data = outhdr + SMB2_HDR_BODY;
	body.length = 8;
	SSVAL(body.data, 0, 9);

	if (info) {
		SIVAL(body.data, 0x04, info->length);
	} else {
		/* Allocated size of req->out.vector[i].iov_base
		 * *MUST BE* OUTVEC_ALLOC_SIZE. So we have room for
		 * 1 byte without having to do an alloc.
		 */
		info = talloc_zero_array(req->out.vector,
					DATA_BLOB,
					1);
		if (!info) {
			return NT_STATUS_NO_MEMORY;
		}
		info->data = ((uint8_t *)outhdr) +
			OUTVEC_ALLOC_SIZE - 1;
		info->length = 1;
		SCVAL(info->data, 0, 0);
	}

	/*
	 * Note: Even if there is an error, continue to process the request.
	 * per MS-SMB2.
	 */

	return smbd_smb2_request_done_ex(req, status, body, info, __location__);
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

static NTSTATUS smbd_smb2_request_next_incoming(struct smbd_server_connection *sconn)
{
	size_t max_send_queue_len;
	size_t cur_send_queue_len;
	struct tevent_req *subreq;

	if (tevent_queue_length(sconn->smb2.recv_queue) > 0) {
		/*
		 * if there is already a smbd_smb2_request_read
		 * pending, we are done.
		 */
		return NT_STATUS_OK;
	}

	max_send_queue_len = MAX(1, sconn->smb2.max_credits/16);
	cur_send_queue_len = tevent_queue_length(sconn->smb2.send_queue);

	if (cur_send_queue_len > max_send_queue_len) {
		/*
		 * if we have a lot of requests to send,
		 * we wait until they are on the wire until we
		 * ask for the next request.
		 */
		return NT_STATUS_OK;
	}

	/* ask for the next request */
	subreq = smbd_smb2_request_read_send(sconn, sconn->smb2.event_ctx, sconn);
	if (subreq == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	tevent_req_set_callback(subreq, smbd_smb2_request_incoming, sconn);

	return NT_STATUS_OK;
}

void smbd_smb2_first_negprot(struct smbd_server_connection *sconn,
			     const uint8_t *inbuf, size_t size)
{
	NTSTATUS status;
	struct smbd_smb2_request *req = NULL;

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

	status = smbd_smb2_request_next_incoming(sconn);
	if (!NT_STATUS_IS_OK(status)) {
		smbd_server_connection_terminate(sconn, nt_errstr(status));
		return;
	}

	sconn->num_requests++;
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
		DEBUG(2,("smbd_smb2_request_incoming: client read error %s\n",
			nt_errstr(status)));
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
	status = smbd_smb2_request_next_incoming(sconn);
	if (!NT_STATUS_IS_OK(status)) {
		smbd_server_connection_terminate(sconn, nt_errstr(status));
		return;
	}

	sconn->num_requests++;

	/* The timeout_processing function isn't run nearly
	   often enough to implement 'max log size' without
	   overrunning the size of the file by many megabytes.
	   This is especially true if we are running at debug
	   level 10.  Checking every 50 SMB2s is a nice
	   tradeoff of performance vs log file size overrun. */

	if ((sconn->num_requests % 50) == 0 &&
	    need_to_check_log_size()) {
		change_to_root_user();
		check_log_size();
	}
}
