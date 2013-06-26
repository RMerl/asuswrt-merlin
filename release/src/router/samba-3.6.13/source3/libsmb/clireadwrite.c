/*
   Unix SMB/CIFS implementation.
   client file read/write routines
   Copyright (C) Andrew Tridgell 1994-1998

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
#include "../lib/util/tevent_ntstatus.h"
#include "async_smb.h"
#include "trans2.h"

/****************************************************************************
  Calculate the recommended read buffer size
****************************************************************************/
static size_t cli_read_max_bufsize(struct cli_state *cli)
{
	size_t data_offset = smb_size - 4;
	size_t wct = 12;

	size_t useable_space;

	if (!client_is_signing_on(cli) && !cli_encryption_on(cli)
	    && (cli->server_posix_capabilities & CIFS_UNIX_LARGE_READ_CAP)) {
		return CLI_SAMBA_MAX_POSIX_LARGE_READX_SIZE;
	}
	if (cli->capabilities & CAP_LARGE_READX) {
		return cli->is_samba
			? CLI_SAMBA_MAX_LARGE_READX_SIZE
			: CLI_WINDOWS_MAX_LARGE_READX_SIZE;
	}

	data_offset += wct * sizeof(uint16_t);
	data_offset += 1; /* pad */

	useable_space = cli->max_xmit - data_offset;

	return useable_space;
}

/****************************************************************************
  Calculate the recommended write buffer size
****************************************************************************/
static size_t cli_write_max_bufsize(struct cli_state *cli,
				    uint16_t write_mode,
				    uint8_t wct)
{
        if (write_mode == 0 &&
	    !client_is_signing_on(cli) &&
	    !cli_encryption_on(cli) &&
	    (cli->server_posix_capabilities & CIFS_UNIX_LARGE_WRITE_CAP) &&
	    (cli->capabilities & CAP_LARGE_FILES)) {
		/* Only do massive writes if we can do them direct
		 * with no signing or encrypting - not on a pipe. */
		return CLI_SAMBA_MAX_POSIX_LARGE_WRITEX_SIZE;
	}

	if (cli->is_samba) {
		return CLI_SAMBA_MAX_LARGE_WRITEX_SIZE;
	}

	if (((cli->capabilities & CAP_LARGE_WRITEX) == 0)
	    || client_is_signing_on(cli)
	    || strequal(cli->dev, "LPT1:")) {
		size_t data_offset = smb_size - 4;
		size_t useable_space;

		data_offset += wct * sizeof(uint16_t);
		data_offset += 1; /* pad */

		useable_space = cli->max_xmit - data_offset;

		return useable_space;
	}

	return CLI_WINDOWS_MAX_LARGE_WRITEX_SIZE;
}

struct cli_read_andx_state {
	size_t size;
	uint16_t vwv[12];
	NTSTATUS status;
	size_t received;
	uint8_t *buf;
};

static void cli_read_andx_done(struct tevent_req *subreq);

struct tevent_req *cli_read_andx_create(TALLOC_CTX *mem_ctx,
					struct event_context *ev,
					struct cli_state *cli, uint16_t fnum,
					off_t offset, size_t size,
					struct tevent_req **psmbreq)
{
	struct tevent_req *req, *subreq;
	struct cli_read_andx_state *state;
	uint8_t wct = 10;

	if (size > cli_read_max_bufsize(cli)) {
		DEBUG(0, ("cli_read_andx_send got size=%d, can only handle "
			  "size=%d\n", (int)size,
			  (int)cli_read_max_bufsize(cli)));
		return NULL;
	}

	req = tevent_req_create(mem_ctx, &state, struct cli_read_andx_state);
	if (req == NULL) {
		return NULL;
	}
	state->size = size;

	SCVAL(state->vwv + 0, 0, 0xFF);
	SCVAL(state->vwv + 0, 1, 0);
	SSVAL(state->vwv + 1, 0, 0);
	SSVAL(state->vwv + 2, 0, fnum);
	SIVAL(state->vwv + 3, 0, offset);
	SSVAL(state->vwv + 5, 0, size);
	SSVAL(state->vwv + 6, 0, size);
	SSVAL(state->vwv + 7, 0, (size >> 16));
	SSVAL(state->vwv + 8, 0, 0);
	SSVAL(state->vwv + 9, 0, 0);

	if (cli->capabilities & CAP_LARGE_FILES) {
		SIVAL(state->vwv + 10, 0,
		      (((uint64_t)offset)>>32) & 0xffffffff);
		wct = 12;
	} else {
		if ((((uint64_t)offset) & 0xffffffff00000000LL) != 0) {
			DEBUG(10, ("cli_read_andx_send got large offset where "
				   "the server does not support it\n"));
			tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
			return tevent_req_post(req, ev);
		}
	}

	subreq = cli_smb_req_create(state, ev, cli, SMBreadX, 0, wct,
				    state->vwv, 0, NULL);
	if (subreq == NULL) {
		TALLOC_FREE(req);
		return NULL;
	}
	tevent_req_set_callback(subreq, cli_read_andx_done, req);
	*psmbreq = subreq;
	return req;
}

struct tevent_req *cli_read_andx_send(TALLOC_CTX *mem_ctx,
				      struct event_context *ev,
				      struct cli_state *cli, uint16_t fnum,
				      off_t offset, size_t size)
{
	struct tevent_req *req, *subreq;
	NTSTATUS status;

	req = cli_read_andx_create(mem_ctx, ev, cli, fnum, offset, size,
				   &subreq);
	if (req == NULL) {
		return NULL;
	}

	status = cli_smb_req_send(subreq);
	if (tevent_req_nterror(req, status)) {
		return tevent_req_post(req, ev);
	}
	return req;
}

static void cli_read_andx_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct cli_read_andx_state *state = tevent_req_data(
		req, struct cli_read_andx_state);
	uint8_t *inbuf;
	uint8_t wct;
	uint16_t *vwv;
	uint32_t num_bytes;
	uint8_t *bytes;

	state->status = cli_smb_recv(subreq, state, &inbuf, 12, &wct, &vwv,
				     &num_bytes, &bytes);
	TALLOC_FREE(subreq);
	if (NT_STATUS_IS_ERR(state->status)) {
		tevent_req_nterror(req, state->status);
		return;
	}

	/* size is the number of bytes the server returned.
	 * Might be zero. */
	state->received = SVAL(vwv + 5, 0);
	state->received |= (((unsigned int)SVAL(vwv + 7, 0)) << 16);

	if (state->received > state->size) {
		DEBUG(5,("server returned more than we wanted!\n"));
		tevent_req_nterror(req, NT_STATUS_UNEXPECTED_IO_ERROR);
		return;
	}

	/*
	 * bcc field must be valid for small reads, for large reads the 16-bit
	 * bcc field can't be correct.
	 */

	if ((state->received < 0xffff) && (state->received > num_bytes)) {
		DEBUG(5, ("server announced more bytes than sent\n"));
		tevent_req_nterror(req, NT_STATUS_INVALID_NETWORK_RESPONSE);
		return;
	}

	state->buf = (uint8_t *)smb_base(inbuf) + SVAL(vwv+6, 0);

	if (trans_oob(smb_len_large(inbuf), SVAL(vwv+6, 0), state->received)
	    || ((state->received != 0) && (state->buf < bytes))) {
		DEBUG(5, ("server returned invalid read&x data offset\n"));
		tevent_req_nterror(req, NT_STATUS_INVALID_NETWORK_RESPONSE);
		return;
	}
	tevent_req_done(req);
}

/*
 * Pull the data out of a finished async read_and_x request. rcvbuf is
 * talloced from the request, so better make sure that you copy it away before
 * you talloc_free(req). "rcvbuf" is NOT a talloc_ctx of its own, so do not
 * talloc_move it!
 */

NTSTATUS cli_read_andx_recv(struct tevent_req *req, ssize_t *received,
			    uint8_t **rcvbuf)
{
	struct cli_read_andx_state *state = tevent_req_data(
		req, struct cli_read_andx_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}
	*received = state->received;
	*rcvbuf = state->buf;
	return NT_STATUS_OK;
}

struct cli_readall_state {
	struct tevent_context *ev;
	struct cli_state *cli;
	uint16_t fnum;
	off_t start_offset;
	size_t size;
	size_t received;
	uint8_t *buf;
};

static void cli_readall_done(struct tevent_req *subreq);

static struct tevent_req *cli_readall_send(TALLOC_CTX *mem_ctx,
					   struct event_context *ev,
					   struct cli_state *cli,
					   uint16_t fnum,
					   off_t offset, size_t size)
{
	struct tevent_req *req, *subreq;
	struct cli_readall_state *state;

	req = tevent_req_create(mem_ctx, &state, struct cli_readall_state);
	if (req == NULL) {
		return NULL;
	}
	state->ev = ev;
	state->cli = cli;
	state->fnum = fnum;
	state->start_offset = offset;
	state->size = size;
	state->received = 0;
	state->buf = NULL;

	subreq = cli_read_andx_send(state, ev, cli, fnum, offset, size);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_readall_done, req);
	return req;
}

static void cli_readall_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct cli_readall_state *state = tevent_req_data(
		req, struct cli_readall_state);
	ssize_t received;
	uint8_t *buf;
	NTSTATUS status;

	status = cli_read_andx_recv(subreq, &received, &buf);
	if (tevent_req_nterror(req, status)) {
		return;
	}

	if (received == 0) {
		/* EOF */
		tevent_req_done(req);
		return;
	}

	if ((state->received == 0) && (received == state->size)) {
		/* Ideal case: Got it all in one run */
		state->buf = buf;
		state->received += received;
		tevent_req_done(req);
		return;
	}

	/*
	 * We got a short read, issue a read for the
	 * rest. Unfortunately we have to allocate the buffer
	 * ourselves now, as our caller expects to receive a single
	 * buffer. cli_read_andx does it from the buffer received from
	 * the net, but with a short read we have to put it together
	 * from several reads.
	 */

	if (state->buf == NULL) {
		state->buf = talloc_array(state, uint8_t, state->size);
		if (tevent_req_nomem(state->buf, req)) {
			return;
		}
	}
	memcpy(state->buf + state->received, buf, received);
	state->received += received;

	TALLOC_FREE(subreq);

	if (state->received >= state->size) {
		tevent_req_done(req);
		return;
	}

	subreq = cli_read_andx_send(state, state->ev, state->cli, state->fnum,
				    state->start_offset + state->received,
				    state->size - state->received);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, cli_readall_done, req);
}

static NTSTATUS cli_readall_recv(struct tevent_req *req, ssize_t *received,
				 uint8_t **rcvbuf)
{
	struct cli_readall_state *state = tevent_req_data(
		req, struct cli_readall_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}
	*received = state->received;
	*rcvbuf = state->buf;
	return NT_STATUS_OK;
}

struct cli_pull_subreq {
	struct tevent_req *req;
	ssize_t received;
	uint8_t *buf;
};

/*
 * Parallel read support.
 *
 * cli_pull sends as many read&x requests as the server would allow via
 * max_mux at a time. When replies flow back in, the data is written into
 * the callback function "sink" in the right order.
 */

struct cli_pull_state {
	struct tevent_req *req;

	struct event_context *ev;
	struct cli_state *cli;
	uint16_t fnum;
	off_t start_offset;
	SMB_OFF_T size;

	NTSTATUS (*sink)(char *buf, size_t n, void *priv);
	void *priv;

	size_t chunk_size;

	/*
	 * Outstanding requests
	 */
	int num_reqs;
	struct cli_pull_subreq *reqs;

	/*
	 * For how many bytes did we send requests already?
	 */
	SMB_OFF_T requested;

	/*
	 * Next request index to push into "sink". This walks around the "req"
	 * array, taking care that the requests are pushed to "sink" in the
	 * right order. If necessary (i.e. replies don't come in in the right
	 * order), replies are held back in "reqs".
	 */
	int top_req;

	/*
	 * How many bytes did we push into "sink"?
	 */

	SMB_OFF_T pushed;
};

static char *cli_pull_print(struct tevent_req *req, TALLOC_CTX *mem_ctx)
{
	struct cli_pull_state *state = tevent_req_data(
		req, struct cli_pull_state);
	char *result;

	result = tevent_req_default_print(req, mem_ctx);
	if (result == NULL) {
		return NULL;
	}

	return talloc_asprintf_append_buffer(
		result, "num_reqs=%d, top_req=%d",
		state->num_reqs, state->top_req);
}

static void cli_pull_read_done(struct tevent_req *read_req);

/*
 * Prepare an async pull request
 */

struct tevent_req *cli_pull_send(TALLOC_CTX *mem_ctx,
				 struct event_context *ev,
				 struct cli_state *cli,
				 uint16_t fnum, off_t start_offset,
				 SMB_OFF_T size, size_t window_size,
				 NTSTATUS (*sink)(char *buf, size_t n,
						  void *priv),
				 void *priv)
{
	struct tevent_req *req;
	struct cli_pull_state *state;
	int i;

	req = tevent_req_create(mem_ctx, &state, struct cli_pull_state);
	if (req == NULL) {
		return NULL;
	}
	tevent_req_set_print_fn(req, cli_pull_print);
	state->req = req;

	state->cli = cli;
	state->ev = ev;
	state->fnum = fnum;
	state->start_offset = start_offset;
	state->size = size;
	state->sink = sink;
	state->priv = priv;

	state->pushed = 0;
	state->top_req = 0;

	if (size == 0) {
		tevent_req_done(req);
		return tevent_req_post(req, ev);
	}

	state->chunk_size = cli_read_max_bufsize(cli);

	state->num_reqs = MAX(window_size/state->chunk_size, 1);
	state->num_reqs = MIN(state->num_reqs, cli->max_mux);

	state->reqs = TALLOC_ZERO_ARRAY(state, struct cli_pull_subreq,
					state->num_reqs);
	if (state->reqs == NULL) {
		goto failed;
	}

	state->requested = 0;

	for (i=0; i<state->num_reqs; i++) {
		struct cli_pull_subreq *subreq = &state->reqs[i];
		SMB_OFF_T size_left;
		size_t request_thistime;

		if (state->requested >= size) {
			state->num_reqs = i;
			break;
		}

		size_left = size - state->requested;
		request_thistime = MIN(size_left, state->chunk_size);

		subreq->req = cli_readall_send(
			state->reqs, ev, cli, fnum,
			state->start_offset + state->requested,
			request_thistime);

		if (subreq->req == NULL) {
			goto failed;
		}
		tevent_req_set_callback(subreq->req, cli_pull_read_done, req);
		state->requested += request_thistime;
	}
	return req;

failed:
	TALLOC_FREE(req);
	return NULL;
}

/*
 * Handle incoming read replies, push the data into sink and send out new
 * requests if necessary.
 */

static void cli_pull_read_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct cli_pull_state *state = tevent_req_data(
		req, struct cli_pull_state);
	struct cli_pull_subreq *pull_subreq = NULL;
	NTSTATUS status;
	int i;

	for (i = 0; i < state->num_reqs; i++) {
		pull_subreq = &state->reqs[i];
		if (subreq == pull_subreq->req) {
			break;
		}
	}
	if (i == state->num_reqs) {
		/* Huh -- received something we did not send?? */
		tevent_req_nterror(req, NT_STATUS_INTERNAL_ERROR);
		return;
	}

	status = cli_readall_recv(subreq, &pull_subreq->received,
				  &pull_subreq->buf);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(state->req, status);
		return;
	}

	/*
	 * This loop is the one to take care of out-of-order replies. All
	 * pending requests are in state->reqs, state->reqs[top_req] is the
	 * one that is to be pushed next. If however a request later than
	 * top_req is replied to, then we can't push yet. If top_req is
	 * replied to at a later point then, we need to push all the finished
	 * requests.
	 */

	while (state->reqs[state->top_req].req != NULL) {
		struct cli_pull_subreq *top_subreq;

		DEBUG(11, ("cli_pull_read_done: top_req = %d\n",
			   state->top_req));

		top_subreq = &state->reqs[state->top_req];

		if (tevent_req_is_in_progress(top_subreq->req)) {
			DEBUG(11, ("cli_pull_read_done: top request not yet "
				   "done\n"));
			return;
		}

		DEBUG(10, ("cli_pull_read_done: Pushing %d bytes, %d already "
			   "pushed\n", (int)top_subreq->received,
			   (int)state->pushed));

		status = state->sink((char *)top_subreq->buf,
				     top_subreq->received, state->priv);
		if (tevent_req_nterror(state->req, status)) {
			return;
		}
		state->pushed += top_subreq->received;

		TALLOC_FREE(state->reqs[state->top_req].req);

		if (state->requested < state->size) {
			struct tevent_req *new_req;
			SMB_OFF_T size_left;
			size_t request_thistime;

			size_left = state->size - state->requested;
			request_thistime = MIN(size_left, state->chunk_size);

			DEBUG(10, ("cli_pull_read_done: Requesting %d bytes "
				   "at %d, position %d\n",
				   (int)request_thistime,
				   (int)(state->start_offset
					 + state->requested),
				   state->top_req));

			new_req = cli_readall_send(
				state->reqs, state->ev, state->cli,
				state->fnum,
				state->start_offset + state->requested,
				request_thistime);

			if (tevent_req_nomem(new_req, state->req)) {
				return;
			}
			tevent_req_set_callback(new_req, cli_pull_read_done,
						req);

			state->reqs[state->top_req].req = new_req;
			state->requested += request_thistime;
		}

		state->top_req = (state->top_req+1) % state->num_reqs;
	}

	tevent_req_done(req);
}

NTSTATUS cli_pull_recv(struct tevent_req *req, SMB_OFF_T *received)
{
	struct cli_pull_state *state = tevent_req_data(
		req, struct cli_pull_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}
	*received = state->pushed;
	return NT_STATUS_OK;
}

NTSTATUS cli_pull(struct cli_state *cli, uint16_t fnum,
		  off_t start_offset, SMB_OFF_T size, size_t window_size,
		  NTSTATUS (*sink)(char *buf, size_t n, void *priv),
		  void *priv, SMB_OFF_T *received)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev;
	struct tevent_req *req;
	NTSTATUS status = NT_STATUS_OK;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}

	ev = event_context_init(frame);
	if (ev == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	req = cli_pull_send(frame, ev, cli, fnum, start_offset, size,
			    window_size, sink, priv);
	if (req == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	if (!tevent_req_poll(req, ev)) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}

	status = cli_pull_recv(req, received);
 fail:
	TALLOC_FREE(frame);
	return status;
}

static NTSTATUS cli_read_sink(char *buf, size_t n, void *priv)
{
	char **pbuf = (char **)priv;
	memcpy(*pbuf, buf, n);
	*pbuf += n;
	return NT_STATUS_OK;
}

ssize_t cli_read(struct cli_state *cli, uint16_t fnum, char *buf,
		 off_t offset, size_t size)
{
	NTSTATUS status;
	SMB_OFF_T ret;

	status = cli_pull(cli, fnum, offset, size, size,
			  cli_read_sink, &buf, &ret);
	if (!NT_STATUS_IS_OK(status)) {
		return -1;
	}
	return ret;
}

/****************************************************************************
  write to a file using a SMBwrite and not bypassing 0 byte writes
****************************************************************************/

NTSTATUS cli_smbwrite(struct cli_state *cli, uint16_t fnum, char *buf,
		      off_t offset, size_t size1, size_t *ptotal)
{
	uint8_t *bytes;
	ssize_t total = 0;

	/*
	 * 3 bytes prefix
	 */

	bytes = TALLOC_ARRAY(talloc_tos(), uint8_t, 3);
	if (bytes == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	bytes[0] = 1;

	do {
		size_t size = MIN(size1, cli->max_xmit - 48);
		struct tevent_req *req;
		uint16_t vwv[5];
		uint16_t *ret_vwv;
		NTSTATUS status;

		SSVAL(vwv+0, 0, fnum);
		SSVAL(vwv+1, 0, size);
		SIVAL(vwv+2, 0, offset);
		SSVAL(vwv+4, 0, 0);

		bytes = TALLOC_REALLOC_ARRAY(talloc_tos(), bytes, uint8_t,
					     size+3);
		if (bytes == NULL) {
			return NT_STATUS_NO_MEMORY;
		}
		SSVAL(bytes, 1, size);
		memcpy(bytes + 3, buf + total, size);

		status = cli_smb(talloc_tos(), cli, SMBwrite, 0, 5, vwv,
				 size+3, bytes, &req, 1, NULL, &ret_vwv,
				 NULL, NULL);
		if (!NT_STATUS_IS_OK(status)) {
			TALLOC_FREE(bytes);
			return status;
		}

		size = SVAL(ret_vwv+0, 0);
		TALLOC_FREE(req);
		if (size == 0) {
			break;
		}
		size1 -= size;
		total += size;
		offset += size;

	} while (size1);

	TALLOC_FREE(bytes);

	if (ptotal != NULL) {
		*ptotal = total;
	}
	return NT_STATUS_OK;
}

/*
 * Send a write&x request
 */

struct cli_write_andx_state {
	size_t size;
	uint16_t vwv[14];
	size_t written;
	uint8_t pad;
	struct iovec iov[2];
};

static void cli_write_andx_done(struct tevent_req *subreq);

struct tevent_req *cli_write_andx_create(TALLOC_CTX *mem_ctx,
					 struct event_context *ev,
					 struct cli_state *cli, uint16_t fnum,
					 uint16_t mode, const uint8_t *buf,
					 off_t offset, size_t size,
					 struct tevent_req **reqs_before,
					 int num_reqs_before,
					 struct tevent_req **psmbreq)
{
	struct tevent_req *req, *subreq;
	struct cli_write_andx_state *state;
	bool bigoffset = ((cli->capabilities & CAP_LARGE_FILES) != 0);
	uint8_t wct = bigoffset ? 14 : 12;
	size_t max_write = cli_write_max_bufsize(cli, mode, wct);
	uint16_t *vwv;

	req = tevent_req_create(mem_ctx, &state, struct cli_write_andx_state);
	if (req == NULL) {
		return NULL;
	}

	state->size = MIN(size, max_write);

	vwv = state->vwv;

	SCVAL(vwv+0, 0, 0xFF);
	SCVAL(vwv+0, 1, 0);
	SSVAL(vwv+1, 0, 0);
	SSVAL(vwv+2, 0, fnum);
	SIVAL(vwv+3, 0, offset);
	SIVAL(vwv+5, 0, 0);
	SSVAL(vwv+7, 0, mode);
	SSVAL(vwv+8, 0, 0);
	SSVAL(vwv+9, 0, (state->size>>16));
	SSVAL(vwv+10, 0, state->size);

	SSVAL(vwv+11, 0,
	      cli_smb_wct_ofs(reqs_before, num_reqs_before)
	      + 1		/* the wct field */
	      + wct * 2		/* vwv */
	      + 2		/* num_bytes field */
	      + 1		/* pad */);

	if (bigoffset) {
		SIVAL(vwv+12, 0, (((uint64_t)offset)>>32) & 0xffffffff);
	}

	state->pad = 0;
	state->iov[0].iov_base = (void *)&state->pad;
	state->iov[0].iov_len = 1;
	state->iov[1].iov_base = CONST_DISCARD(void *, buf);
	state->iov[1].iov_len = state->size;

	subreq = cli_smb_req_create(state, ev, cli, SMBwriteX, 0, wct, vwv,
				    2, state->iov);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_write_andx_done, req);
	*psmbreq = subreq;
	return req;
}

struct tevent_req *cli_write_andx_send(TALLOC_CTX *mem_ctx,
				       struct event_context *ev,
				       struct cli_state *cli, uint16_t fnum,
				       uint16_t mode, const uint8_t *buf,
				       off_t offset, size_t size)
{
	struct tevent_req *req, *subreq;
	NTSTATUS status;

	req = cli_write_andx_create(mem_ctx, ev, cli, fnum, mode, buf, offset,
				    size, NULL, 0, &subreq);
	if (req == NULL) {
		return NULL;
	}

	status = cli_smb_req_send(subreq);
	if (tevent_req_nterror(req, status)) {
		return tevent_req_post(req, ev);
	}
	return req;
}

static void cli_write_andx_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct cli_write_andx_state *state = tevent_req_data(
		req, struct cli_write_andx_state);
	uint8_t wct;
	uint16_t *vwv;
	uint8_t *inbuf;
	NTSTATUS status;

	status = cli_smb_recv(subreq, state, &inbuf, 6, &wct, &vwv,
			      NULL, NULL);
	TALLOC_FREE(subreq);
	if (NT_STATUS_IS_ERR(status)) {
		tevent_req_nterror(req, status);
		return;
	}
	state->written = SVAL(vwv+2, 0);
	if (state->size > UINT16_MAX) {
		/*
		 * It is important that we only set the
		 * high bits only if we asked for a large write.
		 *
		 * OS/2 print shares get this wrong and may send
		 * invalid values.
		 *
		 * See bug #5326.
		 */
		state->written |= SVAL(vwv+4, 0)<<16;
	}
	tevent_req_done(req);
}

NTSTATUS cli_write_andx_recv(struct tevent_req *req, size_t *pwritten)
{
	struct cli_write_andx_state *state = tevent_req_data(
		req, struct cli_write_andx_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}
	*pwritten = state->written;
	return NT_STATUS_OK;
}

struct cli_writeall_state {
	struct event_context *ev;
	struct cli_state *cli;
	uint16_t fnum;
	uint16_t mode;
	const uint8_t *buf;
	off_t offset;
	size_t size;
	size_t written;
};

static void cli_writeall_written(struct tevent_req *req);

static struct tevent_req *cli_writeall_send(TALLOC_CTX *mem_ctx,
					    struct event_context *ev,
					    struct cli_state *cli,
					    uint16_t fnum,
					    uint16_t mode,
					    const uint8_t *buf,
					    off_t offset, size_t size)
{
	struct tevent_req *req, *subreq;
	struct cli_writeall_state *state;

	req = tevent_req_create(mem_ctx, &state, struct cli_writeall_state);
	if (req == NULL) {
		return NULL;
	}
	state->ev = ev;
	state->cli = cli;
	state->fnum = fnum;
	state->mode = mode;
	state->buf = buf;
	state->offset = offset;
	state->size = size;
	state->written = 0;

	subreq = cli_write_andx_send(state, state->ev, state->cli, state->fnum,
				     state->mode, state->buf, state->offset,
				     state->size);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_writeall_written, req);
	return req;
}

static void cli_writeall_written(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct cli_writeall_state *state = tevent_req_data(
		req, struct cli_writeall_state);
	NTSTATUS status;
	size_t written, to_write;

	status = cli_write_andx_recv(subreq, &written);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}

	state->written += written;

	if (state->written > state->size) {
		tevent_req_nterror(req, NT_STATUS_INVALID_NETWORK_RESPONSE);
		return;
	}

	to_write = state->size - state->written;

	if (to_write == 0) {
		tevent_req_done(req);
		return;
	}

	subreq = cli_write_andx_send(state, state->ev, state->cli, state->fnum,
				     state->mode,
				     state->buf + state->written,
				     state->offset + state->written, to_write);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, cli_writeall_written, req);
}

static NTSTATUS cli_writeall_recv(struct tevent_req *req,
				  size_t *pwritten)
{
	struct cli_writeall_state *state = tevent_req_data(
		req, struct cli_writeall_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}
	if (pwritten != NULL) {
		*pwritten = state->written;
	}
	return NT_STATUS_OK;
}

NTSTATUS cli_writeall(struct cli_state *cli, uint16_t fnum, uint16_t mode,
		      const uint8_t *buf, off_t offset, size_t size,
		      size_t *pwritten)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev;
	struct tevent_req *req;
	NTSTATUS status = NT_STATUS_NO_MEMORY;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}
	ev = event_context_init(frame);
	if (ev == NULL) {
		goto fail;
	}
	req = cli_writeall_send(frame, ev, cli, fnum, mode, buf, offset, size);
	if (req == NULL) {
		goto fail;
	}
	if (!tevent_req_poll(req, ev)) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}
	status = cli_writeall_recv(req, pwritten);
 fail:
	TALLOC_FREE(frame);
	return status;
}

struct cli_push_write_state {
	struct tevent_req *req;/* This is the main request! Not the subreq */
	uint32_t idx;
	off_t ofs;
	uint8_t *buf;
	size_t size;
};

struct cli_push_state {
	struct event_context *ev;
	struct cli_state *cli;
	uint16_t fnum;
	uint16_t mode;
	off_t start_offset;
	size_t window_size;

	size_t (*source)(uint8_t *buf, size_t n, void *priv);
	void *priv;

	bool eof;

	size_t chunk_size;
	off_t next_offset;

	/*
	 * Outstanding requests
	 */
	uint32_t pending;
	uint32_t num_reqs;
	struct cli_push_write_state **reqs;
};

static void cli_push_written(struct tevent_req *req);

static bool cli_push_write_setup(struct tevent_req *req,
				 struct cli_push_state *state,
				 uint32_t idx)
{
	struct cli_push_write_state *substate;
	struct tevent_req *subreq;

	substate = talloc(state->reqs, struct cli_push_write_state);
	if (!substate) {
		return false;
	}
	substate->req = req;
	substate->idx = idx;
	substate->ofs = state->next_offset;
	substate->buf = talloc_array(substate, uint8_t, state->chunk_size);
	if (!substate->buf) {
		talloc_free(substate);
		return false;
	}
	substate->size = state->source(substate->buf,
				       state->chunk_size,
				       state->priv);
	if (substate->size == 0) {
		state->eof = true;
		/* nothing to send */
		talloc_free(substate);
		return true;
	}

	subreq = cli_writeall_send(substate,
				   state->ev, state->cli,
				   state->fnum, state->mode,
				   substate->buf,
				   substate->ofs,
				   substate->size);
	if (!subreq) {
		talloc_free(substate);
		return false;
	}
	tevent_req_set_callback(subreq, cli_push_written, substate);

	state->reqs[idx] = substate;
	state->pending += 1;
	state->next_offset += substate->size;

	return true;
}

struct tevent_req *cli_push_send(TALLOC_CTX *mem_ctx, struct event_context *ev,
				 struct cli_state *cli,
				 uint16_t fnum, uint16_t mode,
				 off_t start_offset, size_t window_size,
				 size_t (*source)(uint8_t *buf, size_t n,
						  void *priv),
				 void *priv)
{
	struct tevent_req *req;
	struct cli_push_state *state;
	uint32_t i;

	req = tevent_req_create(mem_ctx, &state, struct cli_push_state);
	if (req == NULL) {
		return NULL;
	}
	state->cli = cli;
	state->ev = ev;
	state->fnum = fnum;
	state->start_offset = start_offset;
	state->mode = mode;
	state->source = source;
	state->priv = priv;
	state->eof = false;
	state->pending = 0;
	state->next_offset = start_offset;

	state->chunk_size = cli_write_max_bufsize(cli, mode, 14);

	if (window_size == 0) {
		window_size = cli->max_mux * state->chunk_size;
	}
	state->num_reqs = window_size/state->chunk_size;
	if ((window_size % state->chunk_size) > 0) {
		state->num_reqs += 1;
	}
	state->num_reqs = MIN(state->num_reqs, cli->max_mux);
	state->num_reqs = MAX(state->num_reqs, 1);

	state->reqs = TALLOC_ZERO_ARRAY(state, struct cli_push_write_state *,
					state->num_reqs);
	if (state->reqs == NULL) {
		goto failed;
	}

	for (i=0; i<state->num_reqs; i++) {
		if (!cli_push_write_setup(req, state, i)) {
			goto failed;
		}

		if (state->eof) {
			break;
		}
	}

	if (state->pending == 0) {
		tevent_req_done(req);
		return tevent_req_post(req, ev);
	}

	return req;

 failed:
	tevent_req_nterror(req, NT_STATUS_NO_MEMORY);
	return tevent_req_post(req, ev);
}

static void cli_push_written(struct tevent_req *subreq)
{
	struct cli_push_write_state *substate = tevent_req_callback_data(
		subreq, struct cli_push_write_state);
	struct tevent_req *req = substate->req;
	struct cli_push_state *state = tevent_req_data(
		req, struct cli_push_state);
	NTSTATUS status;
	uint32_t idx = substate->idx;

	state->reqs[idx] = NULL;
	state->pending -= 1;

	status = cli_writeall_recv(subreq, NULL);
	TALLOC_FREE(subreq);
	TALLOC_FREE(substate);
	if (tevent_req_nterror(req, status)) {
		return;
	}

	if (!state->eof) {
		if (!cli_push_write_setup(req, state, idx)) {
			tevent_req_nterror(req, NT_STATUS_NO_MEMORY);
			return;
		}
	}

	if (state->pending == 0) {
		tevent_req_done(req);
		return;
	}
}

NTSTATUS cli_push_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}

NTSTATUS cli_push(struct cli_state *cli, uint16_t fnum, uint16_t mode,
		  off_t start_offset, size_t window_size,
		  size_t (*source)(uint8_t *buf, size_t n, void *priv),
		  void *priv)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev;
	struct tevent_req *req;
	NTSTATUS status = NT_STATUS_OK;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}

	ev = event_context_init(frame);
	if (ev == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	req = cli_push_send(frame, ev, cli, fnum, mode, start_offset,
			    window_size, source, priv);
	if (req == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	if (!tevent_req_poll(req, ev)) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}

	status = cli_push_recv(req);
 fail:
	TALLOC_FREE(frame);
	return status;
}
