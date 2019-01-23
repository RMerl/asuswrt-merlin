/*
   Unix SMB/CIFS implementation.

   Copyright (C) Stefan Metzmacher 2010

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
#include "system/network.h"
#include "libsmb/libsmb.h"
#include "../lib/util/tevent_ntstatus.h"
#include "../lib/tsocket/tsocket.h"
#include "../lib/tsocket/tsocket_internal.h"
#include "cli_np_tstream.h"

static const struct tstream_context_ops tstream_cli_np_ops;

/*
 * Windows uses 4280 (the max xmit/recv size negotiated on DCERPC).
 * This is fits into the max_xmit negotiated at the SMB layer.
 *
 * On the sending side they may use SMBtranss if the request does not
 * fit into a single SMBtrans call.
 *
 * Windows uses 1024 as max data size of a SMBtrans request and then
 * possibly reads the rest of the DCERPC fragment (up to 3256 bytes)
 * via a SMBreadX.
 *
 * For now we just ask for the full 4280 bytes (max data size) in the SMBtrans
 * request to get the whole fragment at once (like samba 3.5.x and below did.
 *
 * It is important that we use do SMBwriteX with the size of a full fragment,
 * otherwise we may get NT_STATUS_PIPE_BUSY on the SMBtrans request
 * from NT4 servers. (See bug #8195)
 */
#define TSTREAM_CLI_NP_MAX_BUF_SIZE 4280

struct tstream_cli_np {
	struct cli_state *cli;
	const char *npipe;
	uint16_t fnum;
	unsigned int default_timeout;

	struct {
		bool active;
		struct tevent_req *read_req;
		struct tevent_req *write_req;
		uint16_t setup[2];
	} trans;

	struct {
		off_t ofs;
		size_t left;
		uint8_t *buf;
	} read, write;
};

static int tstream_cli_np_destructor(struct tstream_cli_np *cli_nps)
{
	NTSTATUS status;

	if (!cli_state_is_connected(cli_nps->cli)) {
		return 0;
	}

	/*
	 * TODO: do not use a sync call with a destructor!!!
	 *
	 * This only happens, if a caller does talloc_free(),
	 * while the everything was still ok.
	 *
	 * If we get an unexpected failure within a normal
	 * operation, we already do an async cli_close_send()/_recv().
	 *
	 * Once we've fixed all callers to call
	 * tstream_disconnect_send()/_recv(), this will
	 * never be called.
	 */
	status = cli_close(cli_nps->cli, cli_nps->fnum);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("tstream_cli_np_destructor: cli_close "
			  "failed on pipe %s. Error was %s\n",
			  cli_nps->npipe, nt_errstr(status)));
	}
	/*
	 * We can't do much on failure
	 */
	return 0;
}

struct tstream_cli_np_open_state {
	struct cli_state *cli;
	uint16_t fnum;
	const char *npipe;
};

static void tstream_cli_np_open_done(struct tevent_req *subreq);

struct tevent_req *tstream_cli_np_open_send(TALLOC_CTX *mem_ctx,
					    struct tevent_context *ev,
					    struct cli_state *cli,
					    const char *npipe)
{
	struct tevent_req *req;
	struct tstream_cli_np_open_state *state;
	struct tevent_req *subreq;

	req = tevent_req_create(mem_ctx, &state,
				struct tstream_cli_np_open_state);
	if (!req) {
		return NULL;
	}
	state->cli = cli;

	state->npipe = talloc_strdup(state, npipe);
	if (tevent_req_nomem(state->npipe, req)) {
		return tevent_req_post(req, ev);
	}

	subreq = cli_ntcreate_send(state, ev, cli,
				   npipe,
				   0,
				   DESIRED_ACCESS_PIPE,
				   0,
				   FILE_SHARE_READ|FILE_SHARE_WRITE,
				   FILE_OPEN,
				   0,
				   0);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, tstream_cli_np_open_done, req);

	return req;
}

static void tstream_cli_np_open_done(struct tevent_req *subreq)
{
	struct tevent_req *req =
		tevent_req_callback_data(subreq, struct tevent_req);
	struct tstream_cli_np_open_state *state =
		tevent_req_data(req, struct tstream_cli_np_open_state);
	NTSTATUS status;

	status = cli_ntcreate_recv(subreq, &state->fnum);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}

	tevent_req_done(req);
}

NTSTATUS _tstream_cli_np_open_recv(struct tevent_req *req,
				   TALLOC_CTX *mem_ctx,
				   struct tstream_context **_stream,
				   const char *location)
{
	struct tstream_cli_np_open_state *state =
		tevent_req_data(req, struct tstream_cli_np_open_state);
	struct tstream_context *stream;
	struct tstream_cli_np *cli_nps;
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		tevent_req_received(req);
		return status;
	}

	stream = tstream_context_create(mem_ctx,
					&tstream_cli_np_ops,
					&cli_nps,
					struct tstream_cli_np,
					location);
	if (!stream) {
		tevent_req_received(req);
		return NT_STATUS_NO_MEMORY;
	}
	ZERO_STRUCTP(cli_nps);

	cli_nps->cli = state->cli;
	cli_nps->npipe = talloc_move(cli_nps, &state->npipe);
	cli_nps->fnum = state->fnum;
	cli_nps->default_timeout = state->cli->timeout;

	talloc_set_destructor(cli_nps, tstream_cli_np_destructor);

	cli_nps->trans.active = false;
	cli_nps->trans.read_req = NULL;
	cli_nps->trans.write_req = NULL;
	SSVAL(cli_nps->trans.setup+0, 0, TRANSACT_DCERPCCMD);
	SSVAL(cli_nps->trans.setup+1, 0, cli_nps->fnum);

	*_stream = stream;
	tevent_req_received(req);
	return NT_STATUS_OK;
}

static ssize_t tstream_cli_np_pending_bytes(struct tstream_context *stream)
{
	struct tstream_cli_np *cli_nps = tstream_context_data(stream,
					 struct tstream_cli_np);

	if (!cli_state_is_connected(cli_nps->cli)) {
		errno = ENOTCONN;
		return -1;
	}

	return cli_nps->read.left;
}

bool tstream_is_cli_np(struct tstream_context *stream)
{
	struct tstream_cli_np *cli_nps =
		talloc_get_type(_tstream_context_data(stream),
		struct tstream_cli_np);

	if (!cli_nps) {
		return false;
	}

	return true;
}

NTSTATUS tstream_cli_np_use_trans(struct tstream_context *stream)
{
	struct tstream_cli_np *cli_nps = tstream_context_data(stream,
					 struct tstream_cli_np);

	if (cli_nps->trans.read_req) {
		return NT_STATUS_PIPE_BUSY;
	}

	if (cli_nps->trans.write_req) {
		return NT_STATUS_PIPE_BUSY;
	}

	if (cli_nps->trans.active) {
		return NT_STATUS_PIPE_BUSY;
	}

	cli_nps->trans.active = true;

	return NT_STATUS_OK;
}

unsigned int tstream_cli_np_set_timeout(struct tstream_context *stream,
					unsigned int timeout)
{
	struct tstream_cli_np *cli_nps = tstream_context_data(stream,
					 struct tstream_cli_np);

	if (!cli_state_is_connected(cli_nps->cli)) {
		return cli_nps->default_timeout;
	}

	return cli_set_timeout(cli_nps->cli, timeout);
}

struct cli_state *tstream_cli_np_get_cli_state(struct tstream_context *stream)
{
	struct tstream_cli_np *cli_nps = tstream_context_data(stream,
					 struct tstream_cli_np);

	return cli_nps->cli;
}

struct tstream_cli_np_writev_state {
	struct tstream_context *stream;
	struct tevent_context *ev;

	struct iovec *vector;
	size_t count;

	int ret;

	struct {
		int val;
		const char *location;
	} error;
};

static int tstream_cli_np_writev_state_destructor(struct tstream_cli_np_writev_state *state)
{
	struct tstream_cli_np *cli_nps =
		tstream_context_data(state->stream,
		struct tstream_cli_np);

	cli_nps->trans.write_req = NULL;

	return 0;
}

static void tstream_cli_np_writev_write_next(struct tevent_req *req);

static struct tevent_req *tstream_cli_np_writev_send(TALLOC_CTX *mem_ctx,
					struct tevent_context *ev,
					struct tstream_context *stream,
					const struct iovec *vector,
					size_t count)
{
	struct tevent_req *req;
	struct tstream_cli_np_writev_state *state;
	struct tstream_cli_np *cli_nps = tstream_context_data(stream,
					 struct tstream_cli_np);

	req = tevent_req_create(mem_ctx, &state,
				struct tstream_cli_np_writev_state);
	if (!req) {
		return NULL;
	}
	state->stream = stream;
	state->ev = ev;
	state->ret = 0;

	talloc_set_destructor(state, tstream_cli_np_writev_state_destructor);

	if (!cli_state_is_connected(cli_nps->cli)) {
		tevent_req_error(req, ENOTCONN);
		return tevent_req_post(req, ev);
	}

	/*
	 * we make a copy of the vector so we can change the structure
	 */
	state->vector = talloc_array(state, struct iovec, count);
	if (tevent_req_nomem(state->vector, req)) {
		return tevent_req_post(req, ev);
	}
	memcpy(state->vector, vector, sizeof(struct iovec) * count);
	state->count = count;

	tstream_cli_np_writev_write_next(req);
	if (!tevent_req_is_in_progress(req)) {
		return tevent_req_post(req, ev);
	}

	return req;
}

static void tstream_cli_np_readv_trans_start(struct tevent_req *req);
static void tstream_cli_np_writev_write_done(struct tevent_req *subreq);

static void tstream_cli_np_writev_write_next(struct tevent_req *req)
{
	struct tstream_cli_np_writev_state *state =
		tevent_req_data(req,
		struct tstream_cli_np_writev_state);
	struct tstream_cli_np *cli_nps =
		tstream_context_data(state->stream,
		struct tstream_cli_np);
	struct tevent_req *subreq;
	size_t i;
	size_t left = 0;

	for (i=0; i < state->count; i++) {
		left += state->vector[i].iov_len;
	}

	if (left == 0) {
		TALLOC_FREE(cli_nps->write.buf);
		tevent_req_done(req);
		return;
	}

	cli_nps->write.ofs = 0;
	cli_nps->write.left = MIN(left, TSTREAM_CLI_NP_MAX_BUF_SIZE);
	cli_nps->write.buf = talloc_realloc(cli_nps, cli_nps->write.buf,
					    uint8_t, cli_nps->write.left);
	if (tevent_req_nomem(cli_nps->write.buf, req)) {
		return;
	}

	/*
	 * copy the pending buffer first
	 */
	while (cli_nps->write.left > 0 && state->count > 0) {
		uint8_t *base = (uint8_t *)state->vector[0].iov_base;
		size_t len = MIN(cli_nps->write.left, state->vector[0].iov_len);

		memcpy(cli_nps->write.buf + cli_nps->write.ofs, base, len);

		base += len;
		state->vector[0].iov_base = base;
		state->vector[0].iov_len -= len;

		cli_nps->write.ofs += len;
		cli_nps->write.left -= len;

		if (state->vector[0].iov_len == 0) {
			state->vector += 1;
			state->count -= 1;
		}

		state->ret += len;
	}

	if (cli_nps->trans.active && state->count == 0) {
		cli_nps->trans.active = false;
		cli_nps->trans.write_req = req;
		return;
	}

	if (cli_nps->trans.read_req && state->count == 0) {
		cli_nps->trans.write_req = req;
		tstream_cli_np_readv_trans_start(cli_nps->trans.read_req);
		return;
	}

	subreq = cli_write_andx_send(state, state->ev, cli_nps->cli,
				     cli_nps->fnum,
				     8, /* 8 means message mode. */
				     cli_nps->write.buf, 0,
				     cli_nps->write.ofs);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq,
				tstream_cli_np_writev_write_done,
				req);
}

static void tstream_cli_np_writev_disconnect_now(struct tevent_req *req,
						 int error,
						 const char *location);

static void tstream_cli_np_writev_write_done(struct tevent_req *subreq)
{
	struct tevent_req *req =
		tevent_req_callback_data(subreq, struct tevent_req);
	struct tstream_cli_np_writev_state *state =
		tevent_req_data(req, struct tstream_cli_np_writev_state);
	struct tstream_cli_np *cli_nps =
		tstream_context_data(state->stream,
		struct tstream_cli_np);
	size_t written;
	NTSTATUS status;

	status = cli_write_andx_recv(subreq, &written);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tstream_cli_np_writev_disconnect_now(req, EIO, __location__);
		return;
	}

	if (written != cli_nps->write.ofs) {
		tstream_cli_np_writev_disconnect_now(req, EIO, __location__);
		return;
	}

	tstream_cli_np_writev_write_next(req);
}

static void tstream_cli_np_writev_disconnect_done(struct tevent_req *subreq);

static void tstream_cli_np_writev_disconnect_now(struct tevent_req *req,
						 int error,
						 const char *location)
{
	struct tstream_cli_np_writev_state *state =
		tevent_req_data(req,
		struct tstream_cli_np_writev_state);
	struct tstream_cli_np *cli_nps =
		tstream_context_data(state->stream,
		struct tstream_cli_np);
	struct tevent_req *subreq;

	state->error.val = error;
	state->error.location = location;

	if (!cli_state_is_connected(cli_nps->cli)) {
		/* return the original error */
		_tevent_req_error(req, state->error.val, state->error.location);
		return;
	}

	subreq = cli_close_send(state, state->ev, cli_nps->cli, cli_nps->fnum);
	if (subreq == NULL) {
		/* return the original error */
		_tevent_req_error(req, state->error.val, state->error.location);
		return;
	}
	tevent_req_set_callback(subreq,
				tstream_cli_np_writev_disconnect_done,
				req);
}

static void tstream_cli_np_writev_disconnect_done(struct tevent_req *subreq)
{
	struct tevent_req *req =
		tevent_req_callback_data(subreq, struct tevent_req);
	struct tstream_cli_np_writev_state *state =
		tevent_req_data(req, struct tstream_cli_np_writev_state);
	struct tstream_cli_np *cli_nps =
		tstream_context_data(state->stream, struct tstream_cli_np);
	NTSTATUS status;

	status = cli_close_recv(subreq);
	TALLOC_FREE(subreq);

	cli_nps->cli = NULL;

	/* return the original error */
	_tevent_req_error(req, state->error.val, state->error.location);
}

static int tstream_cli_np_writev_recv(struct tevent_req *req,
				      int *perrno)
{
	struct tstream_cli_np_writev_state *state =
		tevent_req_data(req,
		struct tstream_cli_np_writev_state);
	int ret;

	ret = tsocket_simple_int_recv(req, perrno);
	if (ret == 0) {
		ret = state->ret;
	}

	tevent_req_received(req);
	return ret;
}

struct tstream_cli_np_readv_state {
	struct tstream_context *stream;
	struct tevent_context *ev;

	struct iovec *vector;
	size_t count;

	int ret;

	struct {
		struct tevent_immediate *im;
	} trans;

	struct {
		int val;
		const char *location;
	} error;
};

static int tstream_cli_np_readv_state_destructor(struct tstream_cli_np_readv_state *state)
{
	struct tstream_cli_np *cli_nps =
		tstream_context_data(state->stream,
		struct tstream_cli_np);

	cli_nps->trans.read_req = NULL;

	return 0;
}

static void tstream_cli_np_readv_read_next(struct tevent_req *req);

static struct tevent_req *tstream_cli_np_readv_send(TALLOC_CTX *mem_ctx,
					struct tevent_context *ev,
					struct tstream_context *stream,
					struct iovec *vector,
					size_t count)
{
	struct tevent_req *req;
	struct tstream_cli_np_readv_state *state;
	struct tstream_cli_np *cli_nps =
		tstream_context_data(stream, struct tstream_cli_np);

	req = tevent_req_create(mem_ctx, &state,
				struct tstream_cli_np_readv_state);
	if (!req) {
		return NULL;
	}
	state->stream = stream;
	state->ev = ev;
	state->ret = 0;

	talloc_set_destructor(state, tstream_cli_np_readv_state_destructor);

	if (!cli_state_is_connected(cli_nps->cli)) {
		tevent_req_error(req, ENOTCONN);
		return tevent_req_post(req, ev);
	}

	/*
	 * we make a copy of the vector so we can change the structure
	 */
	state->vector = talloc_array(state, struct iovec, count);
	if (tevent_req_nomem(state->vector, req)) {
		return tevent_req_post(req, ev);
	}
	memcpy(state->vector, vector, sizeof(struct iovec) * count);
	state->count = count;

	tstream_cli_np_readv_read_next(req);
	if (!tevent_req_is_in_progress(req)) {
		return tevent_req_post(req, ev);
	}

	return req;
}

static void tstream_cli_np_readv_read_done(struct tevent_req *subreq);

static void tstream_cli_np_readv_read_next(struct tevent_req *req)
{
	struct tstream_cli_np_readv_state *state =
		tevent_req_data(req,
		struct tstream_cli_np_readv_state);
	struct tstream_cli_np *cli_nps =
		tstream_context_data(state->stream,
		struct tstream_cli_np);
	struct tevent_req *subreq;

	/*
	 * copy the pending buffer first
	 */
	while (cli_nps->read.left > 0 && state->count > 0) {
		uint8_t *base = (uint8_t *)state->vector[0].iov_base;
		size_t len = MIN(cli_nps->read.left, state->vector[0].iov_len);

		memcpy(base, cli_nps->read.buf + cli_nps->read.ofs, len);

		base += len;
		state->vector[0].iov_base = base;
		state->vector[0].iov_len -= len;

		cli_nps->read.ofs += len;
		cli_nps->read.left -= len;

		if (state->vector[0].iov_len == 0) {
			state->vector += 1;
			state->count -= 1;
		}

		state->ret += len;
	}

	if (cli_nps->read.left == 0) {
		TALLOC_FREE(cli_nps->read.buf);
	}

	if (state->count == 0) {
		tevent_req_done(req);
		return;
	}

	if (cli_nps->trans.active) {
		cli_nps->trans.active = false;
		cli_nps->trans.read_req = req;
		return;
	}

	if (cli_nps->trans.write_req) {
		cli_nps->trans.read_req = req;
		tstream_cli_np_readv_trans_start(req);
		return;
	}

	subreq = cli_read_andx_send(state, state->ev, cli_nps->cli,
				    cli_nps->fnum, 0, TSTREAM_CLI_NP_MAX_BUF_SIZE);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq,
				tstream_cli_np_readv_read_done,
				req);
}

static void tstream_cli_np_readv_trans_done(struct tevent_req *subreq);

static void tstream_cli_np_readv_trans_start(struct tevent_req *req)
{
	struct tstream_cli_np_readv_state *state =
		tevent_req_data(req,
		struct tstream_cli_np_readv_state);
	struct tstream_cli_np *cli_nps =
		tstream_context_data(state->stream,
		struct tstream_cli_np);
	struct tevent_req *subreq;

	state->trans.im = tevent_create_immediate(state);
	if (tevent_req_nomem(state->trans.im, req)) {
		return;
	}

	subreq = cli_trans_send(state, state->ev,
				cli_nps->cli,
				SMBtrans,
				"\\PIPE\\",
				0, 0, 0,
				cli_nps->trans.setup, 2,
				0,
				NULL, 0, 0,
				cli_nps->write.buf,
				cli_nps->write.ofs,
				TSTREAM_CLI_NP_MAX_BUF_SIZE);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq,
				tstream_cli_np_readv_trans_done,
				req);
}

static void tstream_cli_np_readv_disconnect_now(struct tevent_req *req,
						int error,
						const char *location);
static void tstream_cli_np_readv_trans_next(struct tevent_context *ctx,
					    struct tevent_immediate *im,
					    void *private_data);

static void tstream_cli_np_readv_trans_done(struct tevent_req *subreq)
{
	struct tevent_req *req =
		tevent_req_callback_data(subreq, struct tevent_req);
	struct tstream_cli_np_readv_state *state =
		tevent_req_data(req, struct tstream_cli_np_readv_state);
	struct tstream_cli_np *cli_nps =
		tstream_context_data(state->stream, struct tstream_cli_np);
	uint8_t *rcvbuf;
	uint32_t received;
	NTSTATUS status;

	status = cli_trans_recv(subreq, state, NULL, NULL, 0, NULL,
				NULL, 0, NULL,
				&rcvbuf, 0, &received);
	TALLOC_FREE(subreq);
	if (NT_STATUS_EQUAL(status, NT_STATUS_BUFFER_TOO_SMALL)) {
		status = NT_STATUS_OK;
	}
	if (!NT_STATUS_IS_OK(status)) {
		tstream_cli_np_readv_disconnect_now(req, EIO, __location__);
		return;
	}

	if (received > TSTREAM_CLI_NP_MAX_BUF_SIZE) {
		tstream_cli_np_readv_disconnect_now(req, EIO, __location__);
		return;
	}

	if (received == 0) {
		tstream_cli_np_readv_disconnect_now(req, EPIPE, __location__);
		return;
	}

	cli_nps->read.ofs = 0;
	cli_nps->read.left = received;
	cli_nps->read.buf = talloc_move(cli_nps, &rcvbuf);

	if (cli_nps->trans.write_req == NULL) {
		tstream_cli_np_readv_read_next(req);
		return;
	}

	tevent_schedule_immediate(state->trans.im, state->ev,
				  tstream_cli_np_readv_trans_next, req);

	tevent_req_done(cli_nps->trans.write_req);
}

static void tstream_cli_np_readv_trans_next(struct tevent_context *ctx,
					    struct tevent_immediate *im,
					    void *private_data)
{
	struct tevent_req *req =
		talloc_get_type_abort(private_data,
		struct tevent_req);

	tstream_cli_np_readv_read_next(req);
}

static void tstream_cli_np_readv_read_done(struct tevent_req *subreq)
{
	struct tevent_req *req =
		tevent_req_callback_data(subreq, struct tevent_req);
	struct tstream_cli_np_readv_state *state =
		tevent_req_data(req, struct tstream_cli_np_readv_state);
	struct tstream_cli_np *cli_nps =
		tstream_context_data(state->stream, struct tstream_cli_np);
	uint8_t *rcvbuf;
	ssize_t received;
	NTSTATUS status;

	/*
	 * We must free subreq in this function as there is
	 * a timer event attached to it.
	 */

	status = cli_read_andx_recv(subreq, &received, &rcvbuf);
	/*
	 * We can't TALLOC_FREE(subreq) as usual here, as rcvbuf still is a
	 * child of that.
	 */
	if (NT_STATUS_EQUAL(status, NT_STATUS_BUFFER_TOO_SMALL)) {
		/*
		 * NT_STATUS_BUFFER_TOO_SMALL means that there's
		 * more data to read when the named pipe is used
		 * in message mode (which is the case here).
		 *
		 * But we hide this from the caller.
		 */
		status = NT_STATUS_OK;
	}
	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(subreq);
		tstream_cli_np_readv_disconnect_now(req, EIO, __location__);
		return;
	}

	if (received > TSTREAM_CLI_NP_MAX_BUF_SIZE) {
		TALLOC_FREE(subreq);
		tstream_cli_np_readv_disconnect_now(req, EIO, __location__);
		return;
	}

	if (received == 0) {
		TALLOC_FREE(subreq);
		tstream_cli_np_readv_disconnect_now(req, EPIPE, __location__);
		return;
	}

	cli_nps->read.ofs = 0;
	cli_nps->read.left = received;
	cli_nps->read.buf = talloc_array(cli_nps, uint8_t, received);
	if (cli_nps->read.buf == NULL) {
		TALLOC_FREE(subreq);
		tevent_req_nomem(cli_nps->read.buf, req);
		return;
	}
	memcpy(cli_nps->read.buf, rcvbuf, received);
	TALLOC_FREE(subreq);

	tstream_cli_np_readv_read_next(req);
}

static void tstream_cli_np_readv_disconnect_done(struct tevent_req *subreq);

static void tstream_cli_np_readv_error(struct tevent_req *req);

static void tstream_cli_np_readv_disconnect_now(struct tevent_req *req,
						int error,
						const char *location)
{
	struct tstream_cli_np_readv_state *state =
		tevent_req_data(req,
		struct tstream_cli_np_readv_state);
	struct tstream_cli_np *cli_nps =
		tstream_context_data(state->stream,
		struct tstream_cli_np);
	struct tevent_req *subreq;

	state->error.val = error;
	state->error.location = location;

	if (!cli_state_is_connected(cli_nps->cli)) {
		/* return the original error */
		tstream_cli_np_readv_error(req);
		return;
	}

	subreq = cli_close_send(state, state->ev, cli_nps->cli, cli_nps->fnum);
	if (subreq == NULL) {
		/* return the original error */
		tstream_cli_np_readv_error(req);
		return;
	}
	tevent_req_set_callback(subreq,
				tstream_cli_np_readv_disconnect_done,
				req);
}

static void tstream_cli_np_readv_disconnect_done(struct tevent_req *subreq)
{
	struct tevent_req *req =
		tevent_req_callback_data(subreq, struct tevent_req);
	struct tstream_cli_np_readv_state *state =
		tevent_req_data(req, struct tstream_cli_np_readv_state);
	struct tstream_cli_np *cli_nps =
		tstream_context_data(state->stream, struct tstream_cli_np);
	NTSTATUS status;

	status = cli_close_recv(subreq);
	TALLOC_FREE(subreq);

	cli_nps->cli = NULL;

	tstream_cli_np_readv_error(req);
}

static void tstream_cli_np_readv_error_trigger(struct tevent_context *ctx,
					       struct tevent_immediate *im,
					       void *private_data);

static void tstream_cli_np_readv_error(struct tevent_req *req)
{
	struct tstream_cli_np_readv_state *state =
		tevent_req_data(req,
		struct tstream_cli_np_readv_state);
	struct tstream_cli_np *cli_nps =
		tstream_context_data(state->stream,
		struct tstream_cli_np);

	if (cli_nps->trans.write_req == NULL) {
		/* return the original error */
		_tevent_req_error(req, state->error.val, state->error.location);
		return;
	}

	if (state->trans.im == NULL) {
		/* return the original error */
		_tevent_req_error(req, state->error.val, state->error.location);
		return;
	}

	tevent_schedule_immediate(state->trans.im, state->ev,
				  tstream_cli_np_readv_error_trigger, req);

	/* return the original error for writev */
	_tevent_req_error(cli_nps->trans.write_req,
			  state->error.val, state->error.location);
}

static void tstream_cli_np_readv_error_trigger(struct tevent_context *ctx,
					       struct tevent_immediate *im,
					       void *private_data)
{
	struct tevent_req *req =
		talloc_get_type_abort(private_data,
		struct tevent_req);
	struct tstream_cli_np_readv_state *state =
		tevent_req_data(req,
		struct tstream_cli_np_readv_state);

	/* return the original error */
	_tevent_req_error(req, state->error.val, state->error.location);
}

static int tstream_cli_np_readv_recv(struct tevent_req *req,
				   int *perrno)
{
	struct tstream_cli_np_readv_state *state =
		tevent_req_data(req, struct tstream_cli_np_readv_state);
	int ret;

	ret = tsocket_simple_int_recv(req, perrno);
	if (ret == 0) {
		ret = state->ret;
	}

	tevent_req_received(req);
	return ret;
}

struct tstream_cli_np_disconnect_state {
	struct tstream_context *stream;
};

static void tstream_cli_np_disconnect_done(struct tevent_req *subreq);

static struct tevent_req *tstream_cli_np_disconnect_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct tstream_context *stream)
{
	struct tstream_cli_np *cli_nps = tstream_context_data(stream,
					 struct tstream_cli_np);
	struct tevent_req *req;
	struct tstream_cli_np_disconnect_state *state;
	struct tevent_req *subreq;

	req = tevent_req_create(mem_ctx, &state,
				struct tstream_cli_np_disconnect_state);
	if (req == NULL) {
		return NULL;
	}

	state->stream = stream;

	if (!cli_state_is_connected(cli_nps->cli)) {
		tevent_req_error(req, ENOTCONN);
		return tevent_req_post(req, ev);
	}

	subreq = cli_close_send(state, ev, cli_nps->cli, cli_nps->fnum);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, tstream_cli_np_disconnect_done, req);

	return req;
}

static void tstream_cli_np_disconnect_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(subreq,
							  struct tevent_req);
	struct tstream_cli_np_disconnect_state *state =
		tevent_req_data(req, struct tstream_cli_np_disconnect_state);
	struct tstream_cli_np *cli_nps =
		tstream_context_data(state->stream, struct tstream_cli_np);
	NTSTATUS status;

	status = cli_close_recv(subreq);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_error(req, EIO);
		return;
	}

	cli_nps->cli = NULL;

	tevent_req_done(req);
}

static int tstream_cli_np_disconnect_recv(struct tevent_req *req,
					  int *perrno)
{
	int ret;

	ret = tsocket_simple_int_recv(req, perrno);

	tevent_req_received(req);
	return ret;
}

static const struct tstream_context_ops tstream_cli_np_ops = {
	.name			= "cli_np",

	.pending_bytes		= tstream_cli_np_pending_bytes,

	.readv_send		= tstream_cli_np_readv_send,
	.readv_recv		= tstream_cli_np_readv_recv,

	.writev_send		= tstream_cli_np_writev_send,
	.writev_recv		= tstream_cli_np_writev_recv,

	.disconnect_send	= tstream_cli_np_disconnect_send,
	.disconnect_recv	= tstream_cli_np_disconnect_recv,
};

NTSTATUS _tstream_cli_np_existing(TALLOC_CTX *mem_ctx,
				  struct cli_state *cli,
				  uint16_t fnum,
				  struct tstream_context **_stream,
				  const char *location)
{
	struct tstream_context *stream;
	struct tstream_cli_np *cli_nps;

	stream = tstream_context_create(mem_ctx,
					&tstream_cli_np_ops,
					&cli_nps,
					struct tstream_cli_np,
					location);
	if (!stream) {
		return NT_STATUS_NO_MEMORY;
	}
	ZERO_STRUCTP(cli_nps);

	cli_nps->cli = cli;
	cli_nps->fnum = fnum;

	*_stream = stream;
	return NT_STATUS_OK;
}
