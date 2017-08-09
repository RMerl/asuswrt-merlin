/*
   Unix SMB/CIFS implementation.
   Infrastructure for async SMB client requests
   Copyright (C) Volker Lendecke 2008

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
#include "libsmb/libsmb.h"
#include "../lib/async_req/async_sock.h"
#include "../lib/util/tevent_ntstatus.h"
#include "../lib/util/tevent_unix.h"
#include "async_smb.h"
#include "smb_crypt.h"
#include "libsmb/nmblib.h"

/*
 * Read an smb packet asynchronously, discard keepalives
 */

struct read_smb_state {
	struct tevent_context *ev;
	int fd;
	uint8_t *buf;
};

static ssize_t read_smb_more(uint8_t *buf, size_t buflen, void *private_data);
static void read_smb_done(struct tevent_req *subreq);

static struct tevent_req *read_smb_send(TALLOC_CTX *mem_ctx,
					struct tevent_context *ev,
					int fd)
{
	struct tevent_req *result, *subreq;
	struct read_smb_state *state;

	result = tevent_req_create(mem_ctx, &state, struct read_smb_state);
	if (result == NULL) {
		return NULL;
	}
	state->ev = ev;
	state->fd = fd;

	subreq = read_packet_send(state, ev, fd, 4, read_smb_more, NULL);
	if (subreq == NULL) {
		goto fail;
	}
	tevent_req_set_callback(subreq, read_smb_done, result);
	return result;
 fail:
	TALLOC_FREE(result);
	return NULL;
}

static ssize_t read_smb_more(uint8_t *buf, size_t buflen, void *private_data)
{
	if (buflen > 4) {
		return 0;	/* We've been here, we're done */
	}
	return smb_len_large(buf);
}

static void read_smb_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct read_smb_state *state = tevent_req_data(
		req, struct read_smb_state);
	ssize_t len;
	int err;

	len = read_packet_recv(subreq, state, &state->buf, &err);
	TALLOC_FREE(subreq);
	if (len == -1) {
		tevent_req_error(req, err);
		return;
	}

	if (CVAL(state->buf, 0) == SMBkeepalive) {
		subreq = read_packet_send(state, state->ev, state->fd, 4,
					  read_smb_more, NULL);
		if (tevent_req_nomem(subreq, req)) {
			return;
		}
		tevent_req_set_callback(subreq, read_smb_done, req);
		return;
	}
	tevent_req_done(req);
}

static ssize_t read_smb_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
			     uint8_t **pbuf, int *perrno)
{
	struct read_smb_state *state = tevent_req_data(
		req, struct read_smb_state);

	if (tevent_req_is_unix_error(req, perrno)) {
		return -1;
	}
	*pbuf = talloc_move(mem_ctx, &state->buf);
	return talloc_get_size(*pbuf);
}

/**
 * Fetch an error out of a NBT packet
 * @param[in] buf	The SMB packet
 * @retval		The error, converted to NTSTATUS
 */

NTSTATUS cli_pull_error(char *buf)
{
	uint32_t flags2 = SVAL(buf, smb_flg2);

	if (flags2 & FLAGS2_32_BIT_ERROR_CODES) {
		return NT_STATUS(IVAL(buf, smb_rcls));
	}

	return dos_to_ntstatus(CVAL(buf, smb_rcls), SVAL(buf,smb_err));
}

/**
 * Compatibility helper for the sync APIs: Fake NTSTATUS in cli->inbuf
 * @param[in] cli	The client connection that just received an error
 * @param[in] status	The error to set on "cli"
 */

void cli_set_error(struct cli_state *cli, NTSTATUS status)
{
	uint32_t flags2 = SVAL(cli->inbuf, smb_flg2);

	if (NT_STATUS_IS_DOS(status)) {
		SSVAL(cli->inbuf, smb_flg2,
		      flags2 & ~FLAGS2_32_BIT_ERROR_CODES);
		SCVAL(cli->inbuf, smb_rcls, NT_STATUS_DOS_CLASS(status));
		SSVAL(cli->inbuf, smb_err, NT_STATUS_DOS_CODE(status));
		return;
	}

	SSVAL(cli->inbuf, smb_flg2, flags2 | FLAGS2_32_BIT_ERROR_CODES);
	SIVAL(cli->inbuf, smb_rcls, NT_STATUS_V(status));
	return;
}

/**
 * Figure out if there is an andx command behind the current one
 * @param[in] buf	The smb buffer to look at
 * @param[in] ofs	The offset to the wct field that is followed by the cmd
 * @retval Is there a command following?
 */

static bool have_andx_command(const char *buf, uint16_t ofs)
{
	uint8_t wct;
	size_t buflen = talloc_get_size(buf);

	if ((ofs == buflen-1) || (ofs == buflen)) {
		return false;
	}

	wct = CVAL(buf, ofs);
	if (wct < 2) {
		/*
		 * Not enough space for the command and a following pointer
		 */
		return false;
	}
	return (CVAL(buf, ofs+1) != 0xff);
}

#define MAX_SMB_IOV 5

struct cli_smb_state {
	struct tevent_context *ev;
	struct cli_state *cli;
	uint8_t header[smb_wct+1]; /* Space for the header including the wct */

	/*
	 * For normal requests, cli_smb_req_send chooses a mid. Secondary
	 * trans requests need to use the mid of the primary request, so we
	 * need a place to store it. Assume it's set if != 0.
	 */
	uint16_t mid;

	uint16_t *vwv;
	uint8_t bytecount_buf[2];

	struct iovec iov[MAX_SMB_IOV+3];
	int iov_count;

	uint8_t *inbuf;
	uint32_t seqnum;
	int chain_num;
	int chain_length;
	struct tevent_req **chained_requests;
};

static uint16_t cli_alloc_mid(struct cli_state *cli)
{
	int num_pending = talloc_array_length(cli->pending);
	uint16_t result;

	while (true) {
		int i;

		result = cli->mid++;
		if ((result == 0) || (result == 0xffff)) {
			continue;
		}

		for (i=0; i<num_pending; i++) {
			if (result == cli_smb_req_mid(cli->pending[i])) {
				break;
			}
		}

		if (i == num_pending) {
			return result;
		}
	}
}

void cli_smb_req_unset_pending(struct tevent_req *req)
{
	struct cli_smb_state *state = tevent_req_data(
		req, struct cli_smb_state);
	struct cli_state *cli = state->cli;
	int num_pending = talloc_array_length(cli->pending);
	int i;

	if (state->mid != 0) {
		/*
		 * This is a [nt]trans[2] request which waits
		 * for more than one reply.
		 */
		return;
	}

	if (num_pending == 1) {
		/*
		 * The pending read_smb tevent_req is a child of
		 * cli->pending. So if nothing is pending anymore, we need to
		 * delete the socket read fde.
		 */
		TALLOC_FREE(cli->pending);
		return;
	}

	for (i=0; i<num_pending; i++) {
		if (req == cli->pending[i]) {
			break;
		}
	}
	if (i == num_pending) {
		/*
		 * Something's seriously broken. Just returning here is the
		 * right thing nevertheless, the point of this routine is to
		 * remove ourselves from cli->pending.
		 */
		return;
	}

	/*
	 * Remove ourselves from the cli->pending array
	 */
	if (num_pending > 1) {
		cli->pending[i] = cli->pending[num_pending-1];
	}

	/*
	 * No NULL check here, we're shrinking by sizeof(void *), and
	 * talloc_realloc just adjusts the size for this.
	 */
	cli->pending = talloc_realloc(NULL, cli->pending, struct tevent_req *,
				      num_pending - 1);
	return;
}

static int cli_smb_req_destructor(struct tevent_req *req)
{
	struct cli_smb_state *state = tevent_req_data(
		req, struct cli_smb_state);
	/*
	 * Make sure we really remove it from
	 * the pending array on destruction.
	 */
	state->mid = 0;
	cli_smb_req_unset_pending(req);
	return 0;
}

static void cli_smb_received(struct tevent_req *subreq);

bool cli_smb_req_set_pending(struct tevent_req *req)
{
	struct cli_smb_state *state = tevent_req_data(
		req, struct cli_smb_state);
	struct cli_state *cli;
	struct tevent_req **pending;
	int num_pending;
	struct tevent_req *subreq;

	cli = state->cli;
	num_pending = talloc_array_length(cli->pending);

	pending = talloc_realloc(cli, cli->pending, struct tevent_req *,
				 num_pending+1);
	if (pending == NULL) {
		return false;
	}
	pending[num_pending] = req;
	cli->pending = pending;
	talloc_set_destructor(req, cli_smb_req_destructor);

	if (num_pending > 0) {
		return true;
	}

	/*
	 * We're the first ones, add the read_smb request that waits for the
	 * answer from the server
	 */
	subreq = read_smb_send(cli->pending, state->ev, cli->fd);
	if (subreq == NULL) {
		cli_smb_req_unset_pending(req);
		return false;
	}
	tevent_req_set_callback(subreq, cli_smb_received, cli);
	return true;
}

/*
 * Fetch a smb request's mid. Only valid after the request has been sent by
 * cli_smb_req_send().
 */
uint16_t cli_smb_req_mid(struct tevent_req *req)
{
	struct cli_smb_state *state = tevent_req_data(
		req, struct cli_smb_state);
	return SVAL(state->header, smb_mid);
}

void cli_smb_req_set_mid(struct tevent_req *req, uint16_t mid)
{
	struct cli_smb_state *state = tevent_req_data(
		req, struct cli_smb_state);
	state->mid = mid;
}

uint32_t cli_smb_req_seqnum(struct tevent_req *req)
{
	struct cli_smb_state *state = tevent_req_data(
		req, struct cli_smb_state);
	return state->seqnum;
}

void cli_smb_req_set_seqnum(struct tevent_req *req, uint32_t seqnum)
{
	struct cli_smb_state *state = tevent_req_data(
		req, struct cli_smb_state);
	state->seqnum = seqnum;
}

static size_t iov_len(const struct iovec *iov, int count)
{
	size_t result = 0;
	int i;
	for (i=0; i<count; i++) {
		result += iov[i].iov_len;
	}
	return result;
}

static uint8_t *iov_concat(TALLOC_CTX *mem_ctx, const struct iovec *iov,
			   int count)
{
	size_t len = iov_len(iov, count);
	size_t copied;
	uint8_t *buf;
	int i;

	buf = talloc_array(mem_ctx, uint8_t, len);
	if (buf == NULL) {
		return NULL;
	}
	copied = 0;
	for (i=0; i<count; i++) {
		memcpy(buf+copied, iov[i].iov_base, iov[i].iov_len);
		copied += iov[i].iov_len;
	}
	return buf;
}

struct tevent_req *cli_smb_req_create(TALLOC_CTX *mem_ctx,
				      struct event_context *ev,
				      struct cli_state *cli,
				      uint8_t smb_command,
				      uint8_t additional_flags,
				      uint8_t wct, uint16_t *vwv,
				      int iov_count,
				      struct iovec *bytes_iov)
{
	struct tevent_req *result;
	struct cli_smb_state *state;
	struct timeval endtime;

	if (iov_count > MAX_SMB_IOV) {
		/*
		 * Should not happen :-)
		 */
		return NULL;
	}

	result = tevent_req_create(mem_ctx, &state, struct cli_smb_state);
	if (result == NULL) {
		return NULL;
	}
	state->ev = ev;
	state->cli = cli;
	state->mid = 0;		/* Set to auto-choose in cli_smb_req_send */
	state->chain_num = 0;
	state->chained_requests = NULL;

	cli_setup_packet_buf(cli, (char *)state->header);
	SCVAL(state->header, smb_com, smb_command);
	SSVAL(state->header, smb_tid, cli->cnum);
	SCVAL(state->header, smb_wct, wct);

	state->vwv = vwv;

	SSVAL(state->bytecount_buf, 0, iov_len(bytes_iov, iov_count));

	state->iov[0].iov_base = (void *)state->header;
	state->iov[0].iov_len  = sizeof(state->header);
	state->iov[1].iov_base = (void *)state->vwv;
	state->iov[1].iov_len  = wct * sizeof(uint16_t);
	state->iov[2].iov_base = (void *)state->bytecount_buf;
	state->iov[2].iov_len  = sizeof(uint16_t);

	if (iov_count != 0) {
		memcpy(&state->iov[3], bytes_iov,
		       iov_count * sizeof(*bytes_iov));
	}
	state->iov_count = iov_count + 3;

	if (cli->timeout) {
		endtime = timeval_current_ofs(cli->timeout / 1000,
					      (cli->timeout % 1000) * 1000);
		if (!tevent_req_set_endtime(result, ev, endtime)) {
			tevent_req_nomem(NULL, result);
		}
	}
	return result;
}

static NTSTATUS cli_signv(struct cli_state *cli, struct iovec *iov, int count,
		          uint32_t *seqnum)
{
	uint8_t *buf;

	/*
	 * Obvious optimization: Make cli_calculate_sign_mac work with struct
	 * iovec directly. MD5Update would do that just fine.
	 */

	if ((count <= 0) || (iov[0].iov_len < smb_wct)) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	buf = iov_concat(talloc_tos(), iov, count);
	if (buf == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	cli_calculate_sign_mac(cli, (char *)buf, seqnum);
	memcpy(iov[0].iov_base, buf, iov[0].iov_len);

	TALLOC_FREE(buf);
	return NT_STATUS_OK;
}

static void cli_smb_sent(struct tevent_req *subreq);

static NTSTATUS cli_smb_req_iov_send(struct tevent_req *req,
				     struct cli_smb_state *state,
				     struct iovec *iov, int iov_count)
{
	struct tevent_req *subreq;
	NTSTATUS status;

	if (state->cli->fd == -1) {
		return NT_STATUS_CONNECTION_INVALID;
	}

	if (iov[0].iov_len < smb_wct) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (state->mid != 0) {
		SSVAL(iov[0].iov_base, smb_mid, state->mid);
	} else {
		uint16_t mid = cli_alloc_mid(state->cli);
		SSVAL(iov[0].iov_base, smb_mid, mid);
	}

	smb_setlen((char *)iov[0].iov_base, iov_len(iov, iov_count) - 4);

	status = cli_signv(state->cli, iov, iov_count, &state->seqnum);

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (cli_encryption_on(state->cli)) {
		char *buf, *enc_buf;

		buf = (char *)iov_concat(talloc_tos(), iov, iov_count);
		if (buf == NULL) {
			return NT_STATUS_NO_MEMORY;
		}
		status = cli_encrypt_message(state->cli, (char *)buf,
					     &enc_buf);
		TALLOC_FREE(buf);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0, ("Error in encrypting client message: %s\n",
				  nt_errstr(status)));
			return status;
		}
		buf = (char *)talloc_memdup(state, enc_buf,
					    smb_len(enc_buf)+4);
		SAFE_FREE(enc_buf);
		if (buf == NULL) {
			return NT_STATUS_NO_MEMORY;
		}
		iov[0].iov_base = (void *)buf;
		iov[0].iov_len = talloc_get_size(buf);
		iov_count = 1;
	}
	subreq = writev_send(state, state->ev, state->cli->outgoing,
			     state->cli->fd, false, iov, iov_count);
	if (subreq == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	tevent_req_set_callback(subreq, cli_smb_sent, req);
	return NT_STATUS_OK;
}

NTSTATUS cli_smb_req_send(struct tevent_req *req)
{
	struct cli_smb_state *state = tevent_req_data(
		req, struct cli_smb_state);

	return cli_smb_req_iov_send(req, state, state->iov, state->iov_count);
}

struct tevent_req *cli_smb_send(TALLOC_CTX *mem_ctx,
				struct event_context *ev,
				struct cli_state *cli,
				uint8_t smb_command,
				uint8_t additional_flags,
				uint8_t wct, uint16_t *vwv,
				uint32_t num_bytes,
				const uint8_t *bytes)
{
	struct tevent_req *req;
	struct iovec iov;
	NTSTATUS status;

	iov.iov_base = CONST_DISCARD(void *, bytes);
	iov.iov_len = num_bytes;

	req = cli_smb_req_create(mem_ctx, ev, cli, smb_command,
				 additional_flags, wct, vwv, 1, &iov);
	if (req == NULL) {
		return NULL;
	}

	status = cli_smb_req_send(req);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return tevent_req_post(req, ev);
	}
	return req;
}

static void cli_smb_sent(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct cli_smb_state *state = tevent_req_data(
		req, struct cli_smb_state);
	ssize_t nwritten;
	int err;

	nwritten = writev_recv(subreq, &err);
	TALLOC_FREE(subreq);
	if (nwritten == -1) {
		if (state->cli->fd != -1) {
			close(state->cli->fd);
			state->cli->fd = -1;
		}
		tevent_req_nterror(req, map_nt_error_from_unix(err));
		return;
	}

	switch (CVAL(state->header, smb_com)) {
	case SMBtranss:
	case SMBtranss2:
	case SMBnttranss:
	case SMBntcancel:
		state->inbuf = NULL;
		tevent_req_done(req);
		return;
	case SMBlockingX:
		if ((CVAL(state->header, smb_wct) == 8) &&
		    (CVAL(state->vwv+3, 0) == LOCKING_ANDX_OPLOCK_RELEASE)) {
			state->inbuf = NULL;
			tevent_req_done(req);
			return;
		}
	}

	if (!cli_smb_req_set_pending(req)) {
		tevent_req_nterror(req, NT_STATUS_NO_MEMORY);
		return;
	}
}

static void cli_smb_received(struct tevent_req *subreq)
{
	struct cli_state *cli = tevent_req_callback_data(
		subreq, struct cli_state);
	struct tevent_req *req;
	struct cli_smb_state *state;
	NTSTATUS status;
	uint8_t *inbuf;
	ssize_t received;
	int num_pending;
	int i, err;
	uint16_t mid;
	bool oplock_break;

	received = read_smb_recv(subreq, talloc_tos(), &inbuf, &err);
	TALLOC_FREE(subreq);
	if (received == -1) {
		if (cli->fd != -1) {
			close(cli->fd);
			cli->fd = -1;
		}
		status = map_nt_error_from_unix(err);
		goto fail;
	}

	if ((IVAL(inbuf, 4) != 0x424d53ff) /* 0xFF"SMB" */
	    && (SVAL(inbuf, 4) != 0x45ff)) /* 0xFF"E" */ {
		DEBUG(10, ("Got non-SMB PDU\n"));
		status = NT_STATUS_INVALID_NETWORK_RESPONSE;
		goto fail;
	}

	if (cli_encryption_on(cli) && (CVAL(inbuf, 0) == 0)) {
		uint16_t enc_ctx_num;

		status = get_enc_ctx_num(inbuf, &enc_ctx_num);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(10, ("get_enc_ctx_num returned %s\n",
				   nt_errstr(status)));
			goto fail;
		}

		if (enc_ctx_num != cli->trans_enc_state->enc_ctx_num) {
			DEBUG(10, ("wrong enc_ctx %d, expected %d\n",
				   enc_ctx_num,
				   cli->trans_enc_state->enc_ctx_num));
			status = NT_STATUS_INVALID_HANDLE;
			goto fail;
		}

		status = common_decrypt_buffer(cli->trans_enc_state,
					       (char *)inbuf);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(10, ("common_decrypt_buffer returned %s\n",
				   nt_errstr(status)));
			goto fail;
		}
	}

	mid = SVAL(inbuf, smb_mid);
	num_pending = talloc_array_length(cli->pending);

	for (i=0; i<num_pending; i++) {
		if (mid == cli_smb_req_mid(cli->pending[i])) {
			break;
		}
	}
	if (i == num_pending) {
		/* Dump unexpected reply */
		TALLOC_FREE(inbuf);
		goto done;
	}

	oplock_break = false;

	if (mid == 0xffff) {
		/*
		 * Paranoia checks that this is really an oplock break request.
		 */
		oplock_break = (smb_len(inbuf) == 51); /* hdr + 8 words */
		oplock_break &= ((CVAL(inbuf, smb_flg) & FLAG_REPLY) == 0);
		oplock_break &= (CVAL(inbuf, smb_com) == SMBlockingX);
		oplock_break &= (SVAL(inbuf, smb_vwv6) == 0);
		oplock_break &= (SVAL(inbuf, smb_vwv7) == 0);

		if (!oplock_break) {
			/* Dump unexpected reply */
			TALLOC_FREE(inbuf);
			goto done;
		}
	}

	req = cli->pending[i];
	state = tevent_req_data(req, struct cli_smb_state);

	if (!oplock_break /* oplock breaks are not signed */
	    && !cli_check_sign_mac(cli, (char *)inbuf, state->seqnum+1)) {
		DEBUG(10, ("cli_check_sign_mac failed\n"));
		TALLOC_FREE(inbuf);
		status = NT_STATUS_ACCESS_DENIED;
		close(cli->fd);
		cli->fd = -1;
		goto fail;
	}

	if (state->chained_requests == NULL) {
		state->inbuf = talloc_move(state, &inbuf);
		talloc_set_destructor(req, NULL);
		cli_smb_req_unset_pending(req);
		state->chain_num = 0;
		state->chain_length = 1;
		tevent_req_done(req);
	} else {
		struct tevent_req **chain = talloc_move(
			talloc_tos(), &state->chained_requests);
		int num_chained = talloc_array_length(chain);

		for (i=0; i<num_chained; i++) {
			state = tevent_req_data(chain[i], struct
						cli_smb_state);
			state->inbuf = inbuf;
			state->chain_num = i;
			state->chain_length = num_chained;
			tevent_req_done(chain[i]);
		}
		TALLOC_FREE(inbuf);
		TALLOC_FREE(chain);
	}
 done:
	if (talloc_array_length(cli->pending) > 0) {
		/*
		 * Set up another read request for the other pending cli_smb
		 * requests
		 */
		state = tevent_req_data(cli->pending[0], struct cli_smb_state);
		subreq = read_smb_send(cli->pending, state->ev, cli->fd);
		if (subreq == NULL) {
			status = NT_STATUS_NO_MEMORY;
			goto fail;
		}
		tevent_req_set_callback(subreq, cli_smb_received, cli);
	}
	return;
 fail:
	/*
	 * Cancel all pending requests. We don't do a for-loop walking
	 * cli->pending because that array changes in
	 * cli_smb_req_destructor().
	 */
	while (talloc_array_length(cli->pending) > 0) {
		req = cli->pending[0];
		talloc_set_destructor(req, NULL);
		cli_smb_req_unset_pending(req);
		tevent_req_nterror(req, status);
	}
}

NTSTATUS cli_smb_recv(struct tevent_req *req,
		      TALLOC_CTX *mem_ctx, uint8_t **pinbuf,
		      uint8_t min_wct, uint8_t *pwct, uint16_t **pvwv,
		      uint32_t *pnum_bytes, uint8_t **pbytes)
{
	struct cli_smb_state *state = tevent_req_data(
		req, struct cli_smb_state);
	NTSTATUS status = NT_STATUS_OK;
	uint8_t cmd, wct;
	uint16_t num_bytes;
	size_t wct_ofs, bytes_offset;
	int i;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}

	if (state->inbuf == NULL) {
		if (min_wct != 0) {
			return NT_STATUS_INVALID_NETWORK_RESPONSE;
		}
		if (pinbuf) {
			*pinbuf = NULL;
		}
		if (pwct) {
			*pwct = 0;
		}
		if (pvwv) {
			*pvwv = NULL;
		}
		if (pnum_bytes) {
			*pnum_bytes = 0;
		}
		if (pbytes) {
			*pbytes = NULL;
		}
		/* This was a request without a reply */
		return NT_STATUS_OK;
	}

	wct_ofs = smb_wct;
	cmd = CVAL(state->inbuf, smb_com);

	for (i=0; i<state->chain_num; i++) {
		if (i < state->chain_num-1) {
			if (cmd == 0xff) {
				return NT_STATUS_REQUEST_ABORTED;
			}
			if (!is_andx_req(cmd)) {
				return NT_STATUS_INVALID_NETWORK_RESPONSE;
			}
		}

		if (!have_andx_command((char *)state->inbuf, wct_ofs)) {
			/*
			 * This request was not completed because a previous
			 * request in the chain had received an error.
			 */
			return NT_STATUS_REQUEST_ABORTED;
		}

		wct_ofs = SVAL(state->inbuf, wct_ofs + 3);

		/*
		 * Skip the all-present length field. No overflow, we've just
		 * put a 16-bit value into a size_t.
		 */
		wct_ofs += 4;

		if (wct_ofs+2 > talloc_get_size(state->inbuf)) {
			return NT_STATUS_INVALID_NETWORK_RESPONSE;
		}

		cmd = CVAL(state->inbuf, wct_ofs + 1);
	}

	status = cli_pull_error((char *)state->inbuf);

	cli_set_error(state->cli, status);

	if (!have_andx_command((char *)state->inbuf, wct_ofs)) {

		if ((cmd == SMBsesssetupX)
		    && NT_STATUS_EQUAL(
			    status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {
			/*
			 * NT_STATUS_MORE_PROCESSING_REQUIRED is a
			 * valid return code for session setup
			 */
			goto no_err;
		}

		if (NT_STATUS_IS_ERR(status)) {
			/*
			 * The last command takes the error code. All
			 * further commands down the requested chain
			 * will get a NT_STATUS_REQUEST_ABORTED.
			 */
			return status;
		}
	}

no_err:

	wct = CVAL(state->inbuf, wct_ofs);
	bytes_offset = wct_ofs + 1 + wct * sizeof(uint16_t);
	num_bytes = SVAL(state->inbuf, bytes_offset);

	if (wct < min_wct) {
		return NT_STATUS_INVALID_NETWORK_RESPONSE;
	}

	/*
	 * wct_ofs is a 16-bit value plus 4, wct is a 8-bit value, num_bytes
	 * is a 16-bit value. So bytes_offset being size_t should be far from
	 * wrapping.
	 */
	if ((bytes_offset + 2 > talloc_get_size(state->inbuf))
	    || (bytes_offset > 0xffff)) {
		return NT_STATUS_INVALID_NETWORK_RESPONSE;
	}

	if (pwct != NULL) {
		*pwct = wct;
	}
	if (pvwv != NULL) {
		*pvwv = (uint16_t *)(state->inbuf + wct_ofs + 1);
	}
	if (pnum_bytes != NULL) {
		*pnum_bytes = num_bytes;
	}
	if (pbytes != NULL) {
		*pbytes = (uint8_t *)state->inbuf + bytes_offset + 2;
	}
	if ((mem_ctx != NULL) && (pinbuf != NULL)) {
		if (state->chain_num == state->chain_length-1) {
			*pinbuf = talloc_move(mem_ctx, &state->inbuf);
		} else {
			*pinbuf = state->inbuf;
		}
	}

	return status;
}

size_t cli_smb_wct_ofs(struct tevent_req **reqs, int num_reqs)
{
	size_t wct_ofs;
	int i;

	wct_ofs = smb_wct - 4;

	for (i=0; i<num_reqs; i++) {
		struct cli_smb_state *state;
		state = tevent_req_data(reqs[i], struct cli_smb_state);
		wct_ofs += iov_len(state->iov+1, state->iov_count-1);
		wct_ofs = (wct_ofs + 3) & ~3;
	}
	return wct_ofs;
}

NTSTATUS cli_smb_chain_send(struct tevent_req **reqs, int num_reqs)
{
	struct cli_smb_state *first_state = tevent_req_data(
		reqs[0], struct cli_smb_state);
	struct cli_smb_state *last_state = tevent_req_data(
		reqs[num_reqs-1], struct cli_smb_state);
	struct cli_smb_state *state;
	size_t wct_offset;
	size_t chain_padding = 0;
	int i, iovlen;
	struct iovec *iov = NULL;
	struct iovec *this_iov;
	NTSTATUS status;

	iovlen = 0;
	for (i=0; i<num_reqs; i++) {
		state = tevent_req_data(reqs[i], struct cli_smb_state);
		iovlen += state->iov_count;
	}

	iov = talloc_array(last_state, struct iovec, iovlen);
	if (iov == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	first_state->chained_requests = (struct tevent_req **)talloc_memdup(
		last_state, reqs, sizeof(*reqs) * num_reqs);
	if (first_state->chained_requests == NULL) {
		TALLOC_FREE(iov);
		return NT_STATUS_NO_MEMORY;
	}

	wct_offset = smb_wct - 4;
	this_iov = iov;

	for (i=0; i<num_reqs; i++) {
		size_t next_padding = 0;
		uint16_t *vwv;

		state = tevent_req_data(reqs[i], struct cli_smb_state);

		if (i < num_reqs-1) {
			if (!is_andx_req(CVAL(state->header, smb_com))
			    || CVAL(state->header, smb_wct) < 2) {
				TALLOC_FREE(iov);
				TALLOC_FREE(first_state->chained_requests);
				return NT_STATUS_INVALID_PARAMETER;
			}
		}

		wct_offset += iov_len(state->iov+1, state->iov_count-1) + 1;
		if ((wct_offset % 4) != 0) {
			next_padding = 4 - (wct_offset % 4);
		}
		wct_offset += next_padding;
		vwv = state->vwv;

		if (i < num_reqs-1) {
			struct cli_smb_state *next_state = tevent_req_data(
				reqs[i+1], struct cli_smb_state);
			SCVAL(vwv+0, 0, CVAL(next_state->header, smb_com));
			SCVAL(vwv+0, 1, 0);
			SSVAL(vwv+1, 0, wct_offset);
		} else if (is_andx_req(CVAL(state->header, smb_com))) {
			/* properly end the chain */
			SCVAL(vwv+0, 0, 0xff);
			SCVAL(vwv+0, 1, 0xff);
			SSVAL(vwv+1, 0, 0);
		}

		if (i == 0) {
			this_iov[0] = state->iov[0];
		} else {
			/*
			 * This one is a bit subtle. We have to add
			 * chain_padding bytes between the requests, and we
			 * have to also include the wct field of the
			 * subsequent requests. We use the subsequent header
			 * for the padding, it contains the wct field in its
			 * last byte.
			 */
			this_iov[0].iov_len = chain_padding+1;
			this_iov[0].iov_base = (void *)&state->header[
				sizeof(state->header) - this_iov[0].iov_len];
			memset(this_iov[0].iov_base, 0, this_iov[0].iov_len-1);
		}
		memcpy(this_iov+1, state->iov+1,
		       sizeof(struct iovec) * (state->iov_count-1));
		this_iov += state->iov_count;
		chain_padding = next_padding;
	}

	status = cli_smb_req_iov_send(reqs[0], last_state, iov, iovlen);
	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(iov);
		TALLOC_FREE(first_state->chained_requests);
		return status;
	}

	return NT_STATUS_OK;
}

uint8_t *cli_smb_inbuf(struct tevent_req *req)
{
	struct cli_smb_state *state = tevent_req_data(
		req, struct cli_smb_state);
	return state->inbuf;
}

bool cli_has_async_calls(struct cli_state *cli)
{
	return ((tevent_queue_length(cli->outgoing) != 0)
		|| (talloc_array_length(cli->pending) != 0));
}

struct cli_smb_oplock_break_waiter_state {
	uint16_t fnum;
	uint8_t level;
};

static void cli_smb_oplock_break_waiter_done(struct tevent_req *subreq);

struct tevent_req *cli_smb_oplock_break_waiter_send(TALLOC_CTX *mem_ctx,
						    struct event_context *ev,
						    struct cli_state *cli)
{
	struct tevent_req *req, *subreq;
	struct cli_smb_oplock_break_waiter_state *state;
	struct cli_smb_state *smb_state;

	req = tevent_req_create(mem_ctx, &state,
				struct cli_smb_oplock_break_waiter_state);
	if (req == NULL) {
		return NULL;
	}

	/*
	 * Create a fake SMB request that we will never send out. This is only
	 * used to be set into the pending queue with the right mid.
	 */
	subreq = cli_smb_req_create(mem_ctx, ev, cli, 0, 0, 0, NULL, 0, NULL);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	smb_state = tevent_req_data(subreq, struct cli_smb_state);
	SSVAL(smb_state->header, smb_mid, 0xffff);

	if (!cli_smb_req_set_pending(subreq)) {
		tevent_req_nterror(req, NT_STATUS_NO_MEMORY);
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_smb_oplock_break_waiter_done, req);
	return req;
}

static void cli_smb_oplock_break_waiter_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct cli_smb_oplock_break_waiter_state *state = tevent_req_data(
		req, struct cli_smb_oplock_break_waiter_state);
	uint8_t wct;
	uint16_t *vwv;
	uint32_t num_bytes;
	uint8_t *bytes;
	uint8_t *inbuf;
	NTSTATUS status;

	status = cli_smb_recv(subreq, state, &inbuf, 8, &wct, &vwv,
			      &num_bytes, &bytes);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}
	state->fnum = SVAL(vwv+2, 0);
	state->level = CVAL(vwv+3, 1);
	tevent_req_done(req);
}

NTSTATUS cli_smb_oplock_break_waiter_recv(struct tevent_req *req,
					  uint16_t *pfnum,
					  uint8_t *plevel)
{
	struct cli_smb_oplock_break_waiter_state *state = tevent_req_data(
		req, struct cli_smb_oplock_break_waiter_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}
	*pfnum = state->fnum;
	*plevel = state->level;
	return NT_STATUS_OK;
}


struct cli_session_request_state {
	struct tevent_context *ev;
	int sock;
	uint32 len_hdr;
	struct iovec iov[3];
	uint8_t nb_session_response;
};

static void cli_session_request_sent(struct tevent_req *subreq);
static void cli_session_request_recvd(struct tevent_req *subreq);

struct tevent_req *cli_session_request_send(TALLOC_CTX *mem_ctx,
					    struct tevent_context *ev,
					    int sock,
					    const struct nmb_name *called,
					    const struct nmb_name *calling)
{
	struct tevent_req *req, *subreq;
	struct cli_session_request_state *state;

	req = tevent_req_create(mem_ctx, &state,
				struct cli_session_request_state);
	if (req == NULL) {
		return NULL;
	}
	state->ev = ev;
	state->sock = sock;

	state->iov[1].iov_base = name_mangle(
		state, called->name, called->name_type);
	if (tevent_req_nomem(state->iov[1].iov_base, req)) {
		return tevent_req_post(req, ev);
	}
	state->iov[1].iov_len = name_len(
		(unsigned char *)state->iov[1].iov_base,
		talloc_get_size(state->iov[1].iov_base));

	state->iov[2].iov_base = name_mangle(
		state, calling->name, calling->name_type);
	if (tevent_req_nomem(state->iov[2].iov_base, req)) {
		return tevent_req_post(req, ev);
	}
	state->iov[2].iov_len = name_len(
		(unsigned char *)state->iov[2].iov_base,
		talloc_get_size(state->iov[2].iov_base));

	_smb_setlen(((char *)&state->len_hdr),
		    state->iov[1].iov_len + state->iov[2].iov_len);
	SCVAL((char *)&state->len_hdr, 0, 0x81);

	state->iov[0].iov_base = &state->len_hdr;
	state->iov[0].iov_len = sizeof(state->len_hdr);

	subreq = writev_send(state, ev, NULL, sock, true, state->iov, 3);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_session_request_sent, req);
	return req;
}

static void cli_session_request_sent(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct cli_session_request_state *state = tevent_req_data(
		req, struct cli_session_request_state);
	ssize_t ret;
	int err;

	ret = writev_recv(subreq, &err);
	TALLOC_FREE(subreq);
	if (ret == -1) {
		tevent_req_error(req, err);
		return;
	}
	subreq = read_smb_send(state, state->ev, state->sock);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, cli_session_request_recvd, req);
}

static void cli_session_request_recvd(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct cli_session_request_state *state = tevent_req_data(
		req, struct cli_session_request_state);
	uint8_t *buf;
	ssize_t ret;
	int err;

	ret = read_smb_recv(subreq, talloc_tos(), &buf, &err);
	TALLOC_FREE(subreq);

	if (ret < 4) {
		ret = -1;
		err = EIO;
	}
	if (ret == -1) {
		tevent_req_error(req, err);
		return;
	}
	/*
	 * In case of an error there is more information in the data
	 * portion according to RFC1002. We're not subtle enough to
	 * respond to the different error conditions, so drop the
	 * error info here.
	 */
	state->nb_session_response = CVAL(buf, 0);
	tevent_req_done(req);
}

bool cli_session_request_recv(struct tevent_req *req, int *err, uint8_t *resp)
{
	struct cli_session_request_state *state = tevent_req_data(
		req, struct cli_session_request_state);

	if (tevent_req_is_unix_error(req, err)) {
		return false;
	}
	*resp = state->nb_session_response;
	return true;
}
