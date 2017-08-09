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
#include "../util/tevent_unix.h"
#include "../lib/tsocket/tsocket.h"
#include "../lib/tsocket/tsocket_internal.h"
#include "lib/tls/tls.h"

#if ENABLE_GNUTLS
#include "gnutls/gnutls.h"

#define DH_BITS 1024

#if defined(HAVE_GNUTLS_DATUM) && !defined(HAVE_GNUTLS_DATUM_T)
typedef gnutls_datum gnutls_datum_t;
#endif

#endif /* ENABLE_GNUTLS */

static const struct tstream_context_ops tstream_tls_ops;

struct tstream_tls {
	struct tstream_context *plain_stream;
	int error;

#if ENABLE_GNUTLS
	gnutls_session tls_session;
#endif /* ENABLE_GNUTLS */

	struct tevent_context *current_ev;

	struct tevent_immediate *retry_im;

	struct {
		uint8_t *buf;
		off_t ofs;
		struct iovec iov;
		struct tevent_req *subreq;
		struct tevent_immediate *im;
	} push;

	struct {
		uint8_t *buf;
		struct iovec iov;
		struct tevent_req *subreq;
	} pull;

	struct {
		struct tevent_req *req;
	} handshake;

	struct {
		off_t ofs;
		size_t left;
		uint8_t buffer[1024];
		struct tevent_req *req;
	} write;

	struct {
		off_t ofs;
		size_t left;
		uint8_t buffer[1024];
		struct tevent_req *req;
	} read;

	struct {
		struct tevent_req *req;
	} disconnect;
};

static void tstream_tls_retry_handshake(struct tstream_context *stream);
static void tstream_tls_retry_read(struct tstream_context *stream);
static void tstream_tls_retry_write(struct tstream_context *stream);
static void tstream_tls_retry_disconnect(struct tstream_context *stream);
static void tstream_tls_retry_trigger(struct tevent_context *ctx,
				      struct tevent_immediate *im,
				      void *private_data);

static void tstream_tls_retry(struct tstream_context *stream, bool deferred)
{

	struct tstream_tls *tlss =
		tstream_context_data(stream,
		struct tstream_tls);

	if (tlss->disconnect.req) {
		tstream_tls_retry_disconnect(stream);
		return;
	}

	if (tlss->handshake.req) {
		tstream_tls_retry_handshake(stream);
		return;
	}

	if (tlss->write.req && tlss->read.req && !deferred) {
		tevent_schedule_immediate(tlss->retry_im, tlss->current_ev,
					  tstream_tls_retry_trigger,
					  stream);
	}

	if (tlss->write.req) {
		tstream_tls_retry_write(stream);
		return;
	}

	if (tlss->read.req) {
		tstream_tls_retry_read(stream);
		return;
	}
}

static void tstream_tls_retry_trigger(struct tevent_context *ctx,
				      struct tevent_immediate *im,
				      void *private_data)
{
	struct tstream_context *stream =
		talloc_get_type_abort(private_data,
		struct tstream_context);

	tstream_tls_retry(stream, true);
}

#if ENABLE_GNUTLS
static void tstream_tls_push_trigger_write(struct tevent_context *ev,
					   struct tevent_immediate *im,
					   void *private_data);

static ssize_t tstream_tls_push_function(gnutls_transport_ptr ptr,
					 const void *buf, size_t size)
{
	struct tstream_context *stream =
		talloc_get_type_abort(ptr,
		struct tstream_context);
	struct tstream_tls *tlss =
		tstream_context_data(stream,
		struct tstream_tls);
	uint8_t *nbuf;
	size_t len;

	if (tlss->error != 0) {
		errno = tlss->error;
		return -1;
	}

	if (tlss->push.subreq) {
		errno = EAGAIN;
		return -1;
	}

	len = MIN(size, UINT16_MAX - tlss->push.ofs);

	if (len == 0) {
		errno = EAGAIN;
		return -1;
	}

	nbuf = talloc_realloc(tlss, tlss->push.buf,
			      uint8_t, tlss->push.ofs + len);
	if (nbuf == NULL) {
		if (tlss->push.buf) {
			errno = EAGAIN;
			return -1;
		}

		return -1;
	}
	tlss->push.buf = nbuf;

	memcpy(tlss->push.buf + tlss->push.ofs, buf, len);

	if (tlss->push.im == NULL) {
		tlss->push.im = tevent_create_immediate(tlss);
		if (tlss->push.im == NULL) {
			errno = ENOMEM;
			return -1;
		}
	}

	if (tlss->push.ofs == 0) {
		/*
		 * We'll do start the tstream_writev
		 * in the next event cycle.
		 *
		 * This way we can batch all push requests,
		 * if they fit into a UINT16_MAX buffer.
		 *
		 * This is important as gnutls_handshake()
		 * had a bug in some versions e.g. 2.4.1
		 * and others (See bug #7218) and it doesn't
		 * handle EAGAIN.
		 */
		tevent_schedule_immediate(tlss->push.im,
					  tlss->current_ev,
					  tstream_tls_push_trigger_write,
					  stream);
	}

	tlss->push.ofs += len;
	return len;
}

static void tstream_tls_push_done(struct tevent_req *subreq);

static void tstream_tls_push_trigger_write(struct tevent_context *ev,
					   struct tevent_immediate *im,
					   void *private_data)
{
	struct tstream_context *stream =
		talloc_get_type_abort(private_data,
		struct tstream_context);
	struct tstream_tls *tlss =
		tstream_context_data(stream,
		struct tstream_tls);
	struct tevent_req *subreq;

	if (tlss->push.subreq) {
		/* nothing todo */
		return;
	}

	tlss->push.iov.iov_base = (char *)tlss->push.buf;
	tlss->push.iov.iov_len = tlss->push.ofs;

	subreq = tstream_writev_send(tlss,
				     tlss->current_ev,
				     tlss->plain_stream,
				     &tlss->push.iov, 1);
	if (subreq == NULL) {
		tlss->error = ENOMEM;
		tstream_tls_retry(stream, false);
		return;
	}
	tevent_req_set_callback(subreq, tstream_tls_push_done, stream);

	tlss->push.subreq = subreq;
}

static void tstream_tls_push_done(struct tevent_req *subreq)
{
	struct tstream_context *stream =
		tevent_req_callback_data(subreq,
		struct tstream_context);
	struct tstream_tls *tlss =
		tstream_context_data(stream,
		struct tstream_tls);
	int ret;
	int sys_errno;

	tlss->push.subreq = NULL;
	ZERO_STRUCT(tlss->push.iov);
	TALLOC_FREE(tlss->push.buf);
	tlss->push.ofs = 0;

	ret = tstream_writev_recv(subreq, &sys_errno);
	TALLOC_FREE(subreq);
	if (ret == -1) {
		tlss->error = sys_errno;
		tstream_tls_retry(stream, false);
		return;
	}

	tstream_tls_retry(stream, false);
}

static void tstream_tls_pull_done(struct tevent_req *subreq);

static ssize_t tstream_tls_pull_function(gnutls_transport_ptr ptr,
					 void *buf, size_t size)
{
	struct tstream_context *stream =
		talloc_get_type_abort(ptr,
		struct tstream_context);
	struct tstream_tls *tlss =
		tstream_context_data(stream,
		struct tstream_tls);
	struct tevent_req *subreq;
	size_t len;

	if (tlss->error != 0) {
		errno = tlss->error;
		return -1;
	}

	if (tlss->pull.subreq) {
		errno = EAGAIN;
		return -1;
	}

	if (tlss->pull.iov.iov_base) {
		uint8_t *b;
		size_t n;

		b = (uint8_t *)tlss->pull.iov.iov_base;

		n = MIN(tlss->pull.iov.iov_len, size);
		memcpy(buf, b, n);

		tlss->pull.iov.iov_len -= n;
		b += n;
		tlss->pull.iov.iov_base = (char *)b;
		if (tlss->pull.iov.iov_len == 0) {
			tlss->pull.iov.iov_base = NULL;
			TALLOC_FREE(tlss->pull.buf);
		}

		return n;
	}

	if (size == 0) {
		return 0;
	}

	len = MIN(size, UINT16_MAX);

	tlss->pull.buf = talloc_array(tlss, uint8_t, len);
	if (tlss->pull.buf == NULL) {
		return -1;
	}

	tlss->pull.iov.iov_base = (char *)tlss->pull.buf;
	tlss->pull.iov.iov_len = len;

	subreq = tstream_readv_send(tlss,
				    tlss->current_ev,
				    tlss->plain_stream,
				    &tlss->pull.iov, 1);
	if (subreq == NULL) {
		errno = ENOMEM;
		return -1;
	}
	tevent_req_set_callback(subreq, tstream_tls_pull_done, stream);

	tlss->pull.subreq = subreq;
	errno = EAGAIN;
	return -1;
}

static void tstream_tls_pull_done(struct tevent_req *subreq)
{
	struct tstream_context *stream =
		tevent_req_callback_data(subreq,
		struct tstream_context);
	struct tstream_tls *tlss =
		tstream_context_data(stream,
		struct tstream_tls);
	int ret;
	int sys_errno;

	tlss->pull.subreq = NULL;

	ret = tstream_readv_recv(subreq, &sys_errno);
	TALLOC_FREE(subreq);
	if (ret == -1) {
		tlss->error = sys_errno;
		tstream_tls_retry(stream, false);
		return;
	}

	tstream_tls_retry(stream, false);
}
#endif /* ENABLE_GNUTLS */

static int tstream_tls_destructor(struct tstream_tls *tlss)
{
#if ENABLE_GNUTLS
	if (tlss->tls_session) {
		gnutls_deinit(tlss->tls_session);
		tlss->tls_session = NULL;
	}
#endif /* ENABLE_GNUTLS */
	return 0;
}

static ssize_t tstream_tls_pending_bytes(struct tstream_context *stream)
{
	struct tstream_tls *tlss =
		tstream_context_data(stream,
		struct tstream_tls);
	size_t ret;

	if (tlss->error != 0) {
		errno = tlss->error;
		return -1;
	}

#if ENABLE_GNUTLS
	ret = gnutls_record_check_pending(tlss->tls_session);
	ret += tlss->read.left;
#else /* ENABLE_GNUTLS */
	errno = ENOSYS;
	ret = -1;
#endif /* ENABLE_GNUTLS */
	return ret;
}

struct tstream_tls_readv_state {
	struct tstream_context *stream;

	struct iovec *vector;
	int count;

	int ret;
};

static void tstream_tls_readv_crypt_next(struct tevent_req *req);

static struct tevent_req *tstream_tls_readv_send(TALLOC_CTX *mem_ctx,
					struct tevent_context *ev,
					struct tstream_context *stream,
					struct iovec *vector,
					size_t count)
{
	struct tstream_tls *tlss =
		tstream_context_data(stream,
		struct tstream_tls);
	struct tevent_req *req;
	struct tstream_tls_readv_state *state;

	tlss->read.req = NULL;
	tlss->current_ev = ev;

	req = tevent_req_create(mem_ctx, &state,
				struct tstream_tls_readv_state);
	if (req == NULL) {
		return NULL;
	}

	state->stream = stream;
	state->ret = 0;

	if (tlss->error != 0) {
		tevent_req_error(req, tlss->error);
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

	tstream_tls_readv_crypt_next(req);
	if (!tevent_req_is_in_progress(req)) {
		return tevent_req_post(req, ev);
	}

	return req;
}

static void tstream_tls_readv_crypt_next(struct tevent_req *req)
{
	struct tstream_tls_readv_state *state =
		tevent_req_data(req,
		struct tstream_tls_readv_state);
	struct tstream_tls *tlss =
		tstream_context_data(state->stream,
		struct tstream_tls);

	/*
	 * copy the pending buffer first
	 */
	while (tlss->read.left > 0 && state->count > 0) {
		uint8_t *base = (uint8_t *)state->vector[0].iov_base;
		size_t len = MIN(tlss->read.left, state->vector[0].iov_len);

		memcpy(base, tlss->read.buffer + tlss->read.ofs, len);

		base += len;
		state->vector[0].iov_base = (char *) base;
		state->vector[0].iov_len -= len;

		tlss->read.ofs += len;
		tlss->read.left -= len;

		if (state->vector[0].iov_len == 0) {
			state->vector += 1;
			state->count -= 1;
		}

		state->ret += len;
	}

	if (state->count == 0) {
		tevent_req_done(req);
		return;
	}

	tlss->read.req = req;
	tstream_tls_retry_read(state->stream);
}

static void tstream_tls_retry_read(struct tstream_context *stream)
{
	struct tstream_tls *tlss =
		tstream_context_data(stream,
		struct tstream_tls);
	struct tevent_req *req = tlss->read.req;
#if ENABLE_GNUTLS
	int ret;

	if (tlss->error != 0) {
		tevent_req_error(req, tlss->error);
		return;
	}

	tlss->read.left = 0;
	tlss->read.ofs = 0;

	ret = gnutls_record_recv(tlss->tls_session,
				 tlss->read.buffer,
				 sizeof(tlss->read.buffer));
	if (ret == GNUTLS_E_INTERRUPTED || ret == GNUTLS_E_AGAIN) {
		return;
	}

	tlss->read.req = NULL;

	if (gnutls_error_is_fatal(ret) != 0) {
		DEBUG(1,("TLS %s - %s\n", __location__, gnutls_strerror(ret)));
		tlss->error = EIO;
		tevent_req_error(req, tlss->error);
		return;
	}

	if (ret == 0) {
		tlss->error = EPIPE;
		tevent_req_error(req, tlss->error);
		return;
	}

	tlss->read.left = ret;
	tstream_tls_readv_crypt_next(req);
#else /* ENABLE_GNUTLS */
	tevent_req_error(req, ENOSYS);
#endif /* ENABLE_GNUTLS */
}

static int tstream_tls_readv_recv(struct tevent_req *req,
				  int *perrno)
{
	struct tstream_tls_readv_state *state =
		tevent_req_data(req,
		struct tstream_tls_readv_state);
	struct tstream_tls *tlss =
		tstream_context_data(state->stream,
		struct tstream_tls);
	int ret;

	tlss->read.req = NULL;

	ret = tsocket_simple_int_recv(req, perrno);
	if (ret == 0) {
		ret = state->ret;
	}

	tevent_req_received(req);
	return ret;
}

struct tstream_tls_writev_state {
	struct tstream_context *stream;

	struct iovec *vector;
	int count;

	int ret;
};

static void tstream_tls_writev_crypt_next(struct tevent_req *req);

static struct tevent_req *tstream_tls_writev_send(TALLOC_CTX *mem_ctx,
					struct tevent_context *ev,
					struct tstream_context *stream,
					const struct iovec *vector,
					size_t count)
{
	struct tstream_tls *tlss =
		tstream_context_data(stream,
		struct tstream_tls);
	struct tevent_req *req;
	struct tstream_tls_writev_state *state;

	tlss->write.req = NULL;
	tlss->current_ev = ev;

	req = tevent_req_create(mem_ctx, &state,
				struct tstream_tls_writev_state);
	if (req == NULL) {
		return NULL;
	}

	state->stream = stream;
	state->ret = 0;

	if (tlss->error != 0) {
		tevent_req_error(req, tlss->error);
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

	tstream_tls_writev_crypt_next(req);
	if (!tevent_req_is_in_progress(req)) {
		return tevent_req_post(req, ev);
	}

	return req;
}

static void tstream_tls_writev_crypt_next(struct tevent_req *req)
{
	struct tstream_tls_writev_state *state =
		tevent_req_data(req,
		struct tstream_tls_writev_state);
	struct tstream_tls *tlss =
		tstream_context_data(state->stream,
		struct tstream_tls);

	tlss->write.left = sizeof(tlss->write.buffer);
	tlss->write.ofs = 0;

	/*
	 * first fill our buffer
	 */
	while (tlss->write.left > 0 && state->count > 0) {
		uint8_t *base = (uint8_t *)state->vector[0].iov_base;
		size_t len = MIN(tlss->write.left, state->vector[0].iov_len);

		memcpy(tlss->write.buffer + tlss->write.ofs, base, len);

		base += len;
		state->vector[0].iov_base = (char *) base;
		state->vector[0].iov_len -= len;

		tlss->write.ofs += len;
		tlss->write.left -= len;

		if (state->vector[0].iov_len == 0) {
			state->vector += 1;
			state->count -= 1;
		}

		state->ret += len;
	}

	if (tlss->write.ofs == 0) {
		tevent_req_done(req);
		return;
	}

	tlss->write.left = tlss->write.ofs;
	tlss->write.ofs = 0;

	tlss->write.req = req;
	tstream_tls_retry_write(state->stream);
}

static void tstream_tls_retry_write(struct tstream_context *stream)
{
	struct tstream_tls *tlss =
		tstream_context_data(stream,
		struct tstream_tls);
	struct tevent_req *req = tlss->write.req;
#if ENABLE_GNUTLS
	int ret;

	if (tlss->error != 0) {
		tevent_req_error(req, tlss->error);
		return;
	}

	ret = gnutls_record_send(tlss->tls_session,
				 tlss->write.buffer + tlss->write.ofs,
				 tlss->write.left);
	if (ret == GNUTLS_E_INTERRUPTED || ret == GNUTLS_E_AGAIN) {
		return;
	}

	tlss->write.req = NULL;

	if (gnutls_error_is_fatal(ret) != 0) {
		DEBUG(1,("TLS %s - %s\n", __location__, gnutls_strerror(ret)));
		tlss->error = EIO;
		tevent_req_error(req, tlss->error);
		return;
	}

	if (ret == 0) {
		tlss->error = EPIPE;
		tevent_req_error(req, tlss->error);
		return;
	}

	tlss->write.ofs += ret;
	tlss->write.left -= ret;

	if (tlss->write.left > 0) {
		tlss->write.req = req;
		tstream_tls_retry_write(stream);
		return;
	}

	tstream_tls_writev_crypt_next(req);
#else /* ENABLE_GNUTLS */
	tevent_req_error(req, ENOSYS);
#endif /* ENABLE_GNUTLS */
}

static int tstream_tls_writev_recv(struct tevent_req *req,
				   int *perrno)
{
	struct tstream_tls_writev_state *state =
		tevent_req_data(req,
		struct tstream_tls_writev_state);
	struct tstream_tls *tlss =
		tstream_context_data(state->stream,
		struct tstream_tls);
	int ret;

	tlss->write.req = NULL;

	ret = tsocket_simple_int_recv(req, perrno);
	if (ret == 0) {
		ret = state->ret;
	}

	tevent_req_received(req);
	return ret;
}

struct tstream_tls_disconnect_state {
	uint8_t _dummy;
};

static struct tevent_req *tstream_tls_disconnect_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct tstream_context *stream)
{
	struct tstream_tls *tlss =
		tstream_context_data(stream,
		struct tstream_tls);
	struct tevent_req *req;
	struct tstream_tls_disconnect_state *state;

	tlss->disconnect.req = NULL;
	tlss->current_ev = ev;

	req = tevent_req_create(mem_ctx, &state,
				struct tstream_tls_disconnect_state);
	if (req == NULL) {
		return NULL;
	}

	if (tlss->error != 0) {
		tevent_req_error(req, tlss->error);
		return tevent_req_post(req, ev);
	}

	tlss->disconnect.req = req;
	tstream_tls_retry_disconnect(stream);
	if (!tevent_req_is_in_progress(req)) {
		return tevent_req_post(req, ev);
	}

	return req;
}

static void tstream_tls_retry_disconnect(struct tstream_context *stream)
{
	struct tstream_tls *tlss =
		tstream_context_data(stream,
		struct tstream_tls);
	struct tevent_req *req = tlss->disconnect.req;
#if ENABLE_GNUTLS
	int ret;

	if (tlss->error != 0) {
		tevent_req_error(req, tlss->error);
		return;
	}

	ret = gnutls_bye(tlss->tls_session, GNUTLS_SHUT_WR);
	if (ret == GNUTLS_E_INTERRUPTED || ret == GNUTLS_E_AGAIN) {
		return;
	}

	tlss->disconnect.req = NULL;

	if (gnutls_error_is_fatal(ret) != 0) {
		DEBUG(1,("TLS %s - %s\n", __location__, gnutls_strerror(ret)));
		tlss->error = EIO;
		tevent_req_error(req, tlss->error);
		return;
	}

	if (ret != GNUTLS_E_SUCCESS) {
		DEBUG(1,("TLS %s - %s\n", __location__, gnutls_strerror(ret)));
		tlss->error = EIO;
		tevent_req_error(req, tlss->error);
		return;
	}

	tevent_req_done(req);
#else /* ENABLE_GNUTLS */
	tevent_req_error(req, ENOSYS);
#endif /* ENABLE_GNUTLS */
}

static int tstream_tls_disconnect_recv(struct tevent_req *req,
				       int *perrno)
{
	int ret;

	ret = tsocket_simple_int_recv(req, perrno);

	tevent_req_received(req);
	return ret;
}

static const struct tstream_context_ops tstream_tls_ops = {
	.name			= "tls",

	.pending_bytes		= tstream_tls_pending_bytes,

	.readv_send		= tstream_tls_readv_send,
	.readv_recv		= tstream_tls_readv_recv,

	.writev_send		= tstream_tls_writev_send,
	.writev_recv		= tstream_tls_writev_recv,

	.disconnect_send	= tstream_tls_disconnect_send,
	.disconnect_recv	= tstream_tls_disconnect_recv,
};

struct tstream_tls_params {
#if ENABLE_GNUTLS
	gnutls_certificate_credentials x509_cred;
	gnutls_dh_params dh_params;
#endif /* ENABLE_GNUTLS */
	bool tls_enabled;
};

static int tstream_tls_params_destructor(struct tstream_tls_params *tlsp)
{
#if ENABLE_GNUTLS
	if (tlsp->x509_cred) {
		gnutls_certificate_free_credentials(tlsp->x509_cred);
		tlsp->x509_cred = NULL;
	}
	if (tlsp->dh_params) {
		gnutls_dh_params_deinit(tlsp->dh_params);
		tlsp->dh_params = NULL;
	}
#endif /* ENABLE_GNUTLS */
	return 0;
}

bool tstream_tls_params_enabled(struct tstream_tls_params *tlsp)
{
	return tlsp->tls_enabled;
}

NTSTATUS tstream_tls_params_client(TALLOC_CTX *mem_ctx,
				   const char *ca_file,
				   const char *crl_file,
				   struct tstream_tls_params **_tlsp)
{
#if ENABLE_GNUTLS
	struct tstream_tls_params *tlsp;
	int ret;

	ret = gnutls_global_init();
	if (ret != GNUTLS_E_SUCCESS) {
		DEBUG(0,("TLS %s - %s\n", __location__, gnutls_strerror(ret)));
		return NT_STATUS_NOT_SUPPORTED;
	}

	tlsp = talloc_zero(mem_ctx, struct tstream_tls_params);
	NT_STATUS_HAVE_NO_MEMORY(tlsp);

	talloc_set_destructor(tlsp, tstream_tls_params_destructor);

	ret = gnutls_certificate_allocate_credentials(&tlsp->x509_cred);
	if (ret != GNUTLS_E_SUCCESS) {
		DEBUG(0,("TLS %s - %s\n", __location__, gnutls_strerror(ret)));
		talloc_free(tlsp);
		return NT_STATUS_NO_MEMORY;
	}

	if (ca_file && *ca_file) {
		ret = gnutls_certificate_set_x509_trust_file(tlsp->x509_cred,
							     ca_file,
							     GNUTLS_X509_FMT_PEM);
		if (ret < 0) {
			DEBUG(0,("TLS failed to initialise cafile %s - %s\n",
				 ca_file, gnutls_strerror(ret)));
			talloc_free(tlsp);
			return NT_STATUS_CANT_ACCESS_DOMAIN_INFO;
		}
	}

	if (crl_file && *crl_file) {
		ret = gnutls_certificate_set_x509_crl_file(tlsp->x509_cred,
							   crl_file, 
							   GNUTLS_X509_FMT_PEM);
		if (ret < 0) {
			DEBUG(0,("TLS failed to initialise crlfile %s - %s\n",
				 crl_file, gnutls_strerror(ret)));
			talloc_free(tlsp);
			return NT_STATUS_CANT_ACCESS_DOMAIN_INFO;
		}
	}

	tlsp->tls_enabled = true;

	*_tlsp = tlsp;
	return NT_STATUS_OK;
#else /* ENABLE_GNUTLS */
	return NT_STATUS_NOT_IMPLEMENTED;
#endif /* ENABLE_GNUTLS */
}

struct tstream_tls_connect_state {
	struct tstream_context *tls_stream;
};

struct tevent_req *_tstream_tls_connect_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct tstream_context *plain_stream,
					     struct tstream_tls_params *tls_params,
					     const char *location)
{
	struct tevent_req *req;
	struct tstream_tls_connect_state *state;
#if ENABLE_GNUTLS
	struct tstream_tls *tlss;
	int ret;
	static const int cert_type_priority[] = {
		GNUTLS_CRT_X509,
		GNUTLS_CRT_OPENPGP,
		0
	};
#endif /* ENABLE_GNUTLS */

	req = tevent_req_create(mem_ctx, &state,
				struct tstream_tls_connect_state);
	if (req == NULL) {
		return NULL;
	}

#if ENABLE_GNUTLS
	state->tls_stream = tstream_context_create(state,
						   &tstream_tls_ops,
						   &tlss,
						   struct tstream_tls,
						   location);
	if (tevent_req_nomem(state->tls_stream, req)) {
		return tevent_req_post(req, ev);
	}
	ZERO_STRUCTP(tlss);
	talloc_set_destructor(tlss, tstream_tls_destructor);

	tlss->plain_stream = plain_stream;

	tlss->current_ev = ev;
	tlss->retry_im = tevent_create_immediate(tlss);
	if (tevent_req_nomem(tlss->retry_im, req)) {
		return tevent_req_post(req, ev);
	}

	ret = gnutls_init(&tlss->tls_session, GNUTLS_CLIENT);
	if (ret != GNUTLS_E_SUCCESS) {
		DEBUG(0,("TLS %s - %s\n", __location__, gnutls_strerror(ret)));
		tevent_req_error(req, EINVAL);
		return tevent_req_post(req, ev);
	}

	ret = gnutls_set_default_priority(tlss->tls_session);
	if (ret != GNUTLS_E_SUCCESS) {
		DEBUG(0,("TLS %s - %s\n", __location__, gnutls_strerror(ret)));
		tevent_req_error(req, EINVAL);
		return tevent_req_post(req, ev);
	}

	gnutls_certificate_type_set_priority(tlss->tls_session, cert_type_priority);

	ret = gnutls_credentials_set(tlss->tls_session,
				     GNUTLS_CRD_CERTIFICATE,
				     tls_params->x509_cred);
	if (ret != GNUTLS_E_SUCCESS) {
		DEBUG(0,("TLS %s - %s\n", __location__, gnutls_strerror(ret)));
		tevent_req_error(req, EINVAL);
		return tevent_req_post(req, ev);
	}

	gnutls_transport_set_ptr(tlss->tls_session, (gnutls_transport_ptr)state->tls_stream);
	gnutls_transport_set_pull_function(tlss->tls_session,
					   (gnutls_pull_func)tstream_tls_pull_function);
	gnutls_transport_set_push_function(tlss->tls_session,
					   (gnutls_push_func)tstream_tls_push_function);
	gnutls_transport_set_lowat(tlss->tls_session, 0);

	tlss->handshake.req = req;
	tstream_tls_retry_handshake(state->tls_stream);
	if (!tevent_req_is_in_progress(req)) {
		return tevent_req_post(req, ev);
	}

	return req;
#else /* ENABLE_GNUTLS */
	tevent_req_error(req, ENOSYS);
	return tevent_req_post(req, ev);
#endif /* ENABLE_GNUTLS */
}

int tstream_tls_connect_recv(struct tevent_req *req,
			     int *perrno,
			     TALLOC_CTX *mem_ctx,
			     struct tstream_context **tls_stream)
{
	struct tstream_tls_connect_state *state =
		tevent_req_data(req,
		struct tstream_tls_connect_state);

	if (tevent_req_is_unix_error(req, perrno)) {
		tevent_req_received(req);
		return -1;
	}

	*tls_stream = talloc_move(mem_ctx, &state->tls_stream);
	tevent_req_received(req);
	return 0;
}

extern void tls_cert_generate(TALLOC_CTX *, const char *, const char *, const char *, const char *);

/*
  initialise global tls state
*/
NTSTATUS tstream_tls_params_server(TALLOC_CTX *mem_ctx,
				   const char *dns_host_name,
				   bool enabled,
				   const char *key_file,
				   const char *cert_file,
				   const char *ca_file,
				   const char *crl_file,
				   const char *dhp_file,
				   struct tstream_tls_params **_tlsp)
{
	struct tstream_tls_params *tlsp;
#if ENABLE_GNUTLS
	int ret;

	if (!enabled || key_file == NULL || *key_file == 0) {
		tlsp = talloc_zero(mem_ctx, struct tstream_tls_params);
		NT_STATUS_HAVE_NO_MEMORY(tlsp);
		talloc_set_destructor(tlsp, tstream_tls_params_destructor);
		tlsp->tls_enabled = false;

		*_tlsp = tlsp;
		return NT_STATUS_OK;
	}

	ret = gnutls_global_init();
	if (ret != GNUTLS_E_SUCCESS) {
		DEBUG(0,("TLS %s - %s\n", __location__, gnutls_strerror(ret)));
		return NT_STATUS_NOT_SUPPORTED;
	}

	tlsp = talloc_zero(mem_ctx, struct tstream_tls_params);
	NT_STATUS_HAVE_NO_MEMORY(tlsp);

	talloc_set_destructor(tlsp, tstream_tls_params_destructor);

	if (!file_exist(ca_file)) {
		tls_cert_generate(tlsp, dns_host_name,
				  key_file, cert_file, ca_file);
	}

	ret = gnutls_certificate_allocate_credentials(&tlsp->x509_cred);
	if (ret != GNUTLS_E_SUCCESS) {
		DEBUG(0,("TLS %s - %s\n", __location__, gnutls_strerror(ret)));
		talloc_free(tlsp);
		return NT_STATUS_NO_MEMORY;
	}

	if (ca_file && *ca_file) {
		ret = gnutls_certificate_set_x509_trust_file(tlsp->x509_cred,
							     ca_file,
							     GNUTLS_X509_FMT_PEM);
		if (ret < 0) {
			DEBUG(0,("TLS failed to initialise cafile %s - %s\n",
				 ca_file, gnutls_strerror(ret)));
			talloc_free(tlsp);
			return NT_STATUS_CANT_ACCESS_DOMAIN_INFO;
		}
	}

	if (crl_file && *crl_file) {
		ret = gnutls_certificate_set_x509_crl_file(tlsp->x509_cred,
							   crl_file, 
							   GNUTLS_X509_FMT_PEM);
		if (ret < 0) {
			DEBUG(0,("TLS failed to initialise crlfile %s - %s\n",
				 crl_file, gnutls_strerror(ret)));
			talloc_free(tlsp);
			return NT_STATUS_CANT_ACCESS_DOMAIN_INFO;
		}
	}

	ret = gnutls_certificate_set_x509_key_file(tlsp->x509_cred,
						   cert_file, key_file,
						   GNUTLS_X509_FMT_PEM);
	if (ret != GNUTLS_E_SUCCESS) {
		DEBUG(0,("TLS failed to initialise certfile %s and keyfile %s - %s\n",
			 cert_file, key_file, gnutls_strerror(ret)));
		talloc_free(tlsp);
		return NT_STATUS_CANT_ACCESS_DOMAIN_INFO;
	}

	ret = gnutls_dh_params_init(&tlsp->dh_params);
	if (ret != GNUTLS_E_SUCCESS) {
		DEBUG(0,("TLS %s - %s\n", __location__, gnutls_strerror(ret)));
		talloc_free(tlsp);
		return NT_STATUS_NO_MEMORY;
	}

	if (dhp_file && *dhp_file) {
		gnutls_datum_t dhparms;
		size_t size;

		dhparms.data = (uint8_t *)file_load(dhp_file, &size, 0, tlsp);

		if (!dhparms.data) {
			DEBUG(0,("TLS failed to read DH Parms from %s - %d:%s\n",
				 dhp_file, errno, strerror(errno)));
			talloc_free(tlsp);
			return NT_STATUS_CANT_ACCESS_DOMAIN_INFO;
		}
		dhparms.size = size;

		ret = gnutls_dh_params_import_pkcs3(tlsp->dh_params,
						    &dhparms,
						    GNUTLS_X509_FMT_PEM);
		if (ret != GNUTLS_E_SUCCESS) {
			DEBUG(0,("TLS failed to import pkcs3 %s - %s\n",
				 dhp_file, gnutls_strerror(ret)));
			talloc_free(tlsp);
			return NT_STATUS_CANT_ACCESS_DOMAIN_INFO;
		}
	} else {
		ret = gnutls_dh_params_generate2(tlsp->dh_params, DH_BITS);
		if (ret != GNUTLS_E_SUCCESS) {
			DEBUG(0,("TLS failed to generate dh_params - %s\n",
				 gnutls_strerror(ret)));
			talloc_free(tlsp);
			return NT_STATUS_INTERNAL_ERROR;
		}
	}

	gnutls_certificate_set_dh_params(tlsp->x509_cred, tlsp->dh_params);

	tlsp->tls_enabled = true;

#else /* ENABLE_GNUTLS */
	tlsp = talloc_zero(mem_ctx, struct tstream_tls_params);
	NT_STATUS_HAVE_NO_MEMORY(tlsp);
	talloc_set_destructor(tlsp, tstream_tls_params_destructor);
	tlsp->tls_enabled = false;
#endif /* ENABLE_GNUTLS */

	*_tlsp = tlsp;
	return NT_STATUS_OK;
}

struct tstream_tls_accept_state {
	struct tstream_context *tls_stream;
};

struct tevent_req *_tstream_tls_accept_send(TALLOC_CTX *mem_ctx,
					    struct tevent_context *ev,
					    struct tstream_context *plain_stream,
					    struct tstream_tls_params *tlsp,
					    const char *location)
{
	struct tevent_req *req;
	struct tstream_tls_accept_state *state;
	struct tstream_tls *tlss;
#if ENABLE_GNUTLS
	int ret;
#endif /* ENABLE_GNUTLS */

	req = tevent_req_create(mem_ctx, &state,
				struct tstream_tls_accept_state);
	if (req == NULL) {
		return NULL;
	}

	state->tls_stream = tstream_context_create(state,
						   &tstream_tls_ops,
						   &tlss,
						   struct tstream_tls,
						   location);
	if (tevent_req_nomem(state->tls_stream, req)) {
		return tevent_req_post(req, ev);
	}
	ZERO_STRUCTP(tlss);
	talloc_set_destructor(tlss, tstream_tls_destructor);

#if ENABLE_GNUTLS
	tlss->plain_stream = plain_stream;

	tlss->current_ev = ev;
	tlss->retry_im = tevent_create_immediate(tlss);
	if (tevent_req_nomem(tlss->retry_im, req)) {
		return tevent_req_post(req, ev);
	}

	ret = gnutls_init(&tlss->tls_session, GNUTLS_SERVER);
	if (ret != GNUTLS_E_SUCCESS) {
		DEBUG(0,("TLS %s - %s\n", __location__, gnutls_strerror(ret)));
		tevent_req_error(req, EINVAL);
		return tevent_req_post(req, ev);
	}

	ret = gnutls_set_default_priority(tlss->tls_session);
	if (ret != GNUTLS_E_SUCCESS) {
		DEBUG(0,("TLS %s - %s\n", __location__, gnutls_strerror(ret)));
		tevent_req_error(req, EINVAL);
		return tevent_req_post(req, ev);
	}

	ret = gnutls_credentials_set(tlss->tls_session, GNUTLS_CRD_CERTIFICATE,
				     tlsp->x509_cred);
	if (ret != GNUTLS_E_SUCCESS) {
		DEBUG(0,("TLS %s - %s\n", __location__, gnutls_strerror(ret)));
		tevent_req_error(req, EINVAL);
		return tevent_req_post(req, ev);
	}

	gnutls_certificate_server_set_request(tlss->tls_session,
					      GNUTLS_CERT_REQUEST);
	gnutls_dh_set_prime_bits(tlss->tls_session, DH_BITS);

	gnutls_transport_set_ptr(tlss->tls_session, (gnutls_transport_ptr)state->tls_stream);
	gnutls_transport_set_pull_function(tlss->tls_session,
					   (gnutls_pull_func)tstream_tls_pull_function);
	gnutls_transport_set_push_function(tlss->tls_session,
					   (gnutls_push_func)tstream_tls_push_function);
	gnutls_transport_set_lowat(tlss->tls_session, 0);

	tlss->handshake.req = req;
	tstream_tls_retry_handshake(state->tls_stream);
	if (!tevent_req_is_in_progress(req)) {
		return tevent_req_post(req, ev);
	}

	return req;
#else /* ENABLE_GNUTLS */
	tevent_req_error(req, ENOSYS);
	return tevent_req_post(req, ev);
#endif /* ENABLE_GNUTLS */
}

static void tstream_tls_retry_handshake(struct tstream_context *stream)
{
	struct tstream_tls *tlss =
		tstream_context_data(stream,
		struct tstream_tls);
	struct tevent_req *req = tlss->handshake.req;
#if ENABLE_GNUTLS
	int ret;

	if (tlss->error != 0) {
		tevent_req_error(req, tlss->error);
		return;
	}

	ret = gnutls_handshake(tlss->tls_session);
	if (ret == GNUTLS_E_INTERRUPTED || ret == GNUTLS_E_AGAIN) {
		return;
	}

	tlss->handshake.req = NULL;

	if (gnutls_error_is_fatal(ret) != 0) {
		DEBUG(1,("TLS %s - %s\n", __location__, gnutls_strerror(ret)));
		tlss->error = EIO;
		tevent_req_error(req, tlss->error);
		return;
	}

	if (ret != GNUTLS_E_SUCCESS) {
		DEBUG(1,("TLS %s - %s\n", __location__, gnutls_strerror(ret)));
		tlss->error = EIO;
		tevent_req_error(req, tlss->error);
		return;
	}

	tevent_req_done(req);
#else /* ENABLE_GNUTLS */
	tevent_req_error(req, ENOSYS);
#endif /* ENABLE_GNUTLS */
}

int tstream_tls_accept_recv(struct tevent_req *req,
			    int *perrno,
			    TALLOC_CTX *mem_ctx,
			    struct tstream_context **tls_stream)
{
	struct tstream_tls_accept_state *state =
		tevent_req_data(req,
		struct tstream_tls_accept_state);

	if (tevent_req_is_unix_error(req, perrno)) {
		tevent_req_received(req);
		return -1;
	}

	*tls_stream = talloc_move(mem_ctx, &state->tls_stream);
	tevent_req_received(req);
	return 0;
}
