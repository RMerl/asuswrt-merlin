/*
   Unix SMB/CIFS implementation.

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
#include "system/network.h"
#include "../util/tevent_unix.h"
#include "../lib/tsocket/tsocket.h"
#include "../lib/tsocket/tsocket_internal.h"
#include "../librpc/gen_ndr/ndr_named_pipe_auth.h"
#include "../libcli/named_pipe_auth/npa_tstream.h"
#if _SAMBA_BUILD_ == 4
#include "libcli/raw/smb.h"
#endif

static const struct tstream_context_ops tstream_npa_ops;

struct tstream_npa {
	struct tstream_context *unix_stream;

	uint16_t file_type;

	struct iovec pending;
};

struct tstream_npa_connect_state {
	struct {
		struct tevent_context *ev;
	} caller;

	const char *unix_path;
	struct tsocket_address *unix_laddr;
	struct tsocket_address *unix_raddr;
	struct tstream_context *unix_stream;

	struct named_pipe_auth_req auth_req;
	DATA_BLOB auth_req_blob;
	struct iovec auth_req_iov;

	struct named_pipe_auth_rep auth_rep;
	DATA_BLOB auth_rep_blob;
};

static void tstream_npa_connect_unix_done(struct tevent_req *subreq);

struct tevent_req *tstream_npa_connect_send(TALLOC_CTX *mem_ctx,
					    struct tevent_context *ev,
					    const char *directory,
					    const char *npipe,
					    const struct tsocket_address *client,
					    const char *client_name_in,
					    const struct tsocket_address *server,
					    const char *server_name,
					    const struct auth_session_info_transport *session_info)
{
	struct tevent_req *req;
	struct tstream_npa_connect_state *state;
	struct tevent_req *subreq;
	int ret;
	enum ndr_err_code ndr_err;
	char *lower_case_npipe;
	struct named_pipe_auth_req_info4 *info4;

	req = tevent_req_create(mem_ctx, &state,
				struct tstream_npa_connect_state);
	if (!req) {
		return NULL;
	}

	state->caller.ev = ev;

	lower_case_npipe = strlower_talloc(state, npipe);
	if (tevent_req_nomem(lower_case_npipe, req)) {
		goto post;
	}

	state->unix_path = talloc_asprintf(state, "%s/%s",
					   directory,
					   lower_case_npipe);
	talloc_free(lower_case_npipe);
	if (tevent_req_nomem(state->unix_path, req)) {
		goto post;
	}

	ret = tsocket_address_unix_from_path(state,
					     "",
					     &state->unix_laddr);
	if (ret == -1) {
		tevent_req_error(req, errno);
		goto post;
	}

	ret = tsocket_address_unix_from_path(state,
					     state->unix_path,
					     &state->unix_raddr);
	if (ret == -1) {
		tevent_req_error(req, errno);
		goto post;
	}

	ZERO_STRUCT(state->auth_req);

	if (!server) {
		tevent_req_error(req, EINVAL);
		goto post;
	}

	state->auth_req.level = 4;
	info4 = &state->auth_req.info.info4;

	info4->client_name = client_name_in;
	info4->client_addr = tsocket_address_inet_addr_string(client, state);
	if (!info4->client_addr) {
		/* errno might be EINVAL */
		tevent_req_error(req, errno);
		goto post;
	}
	info4->client_port = tsocket_address_inet_port(client);
	if (!info4->client_name) {
		info4->client_name = info4->client_addr;
	}

	info4->server_addr = tsocket_address_inet_addr_string(server, state);
	if (!info4->server_addr) {
		/* errno might be EINVAL */
		tevent_req_error(req, errno);
		goto post;
	}
	info4->server_port = tsocket_address_inet_port(server);
	if (!info4->server_name) {
		info4->server_name = info4->server_addr;
	}

	info4->session_info = discard_const_p(struct auth_session_info_transport, session_info);

	if (DEBUGLVL(10)) {
		NDR_PRINT_DEBUG(named_pipe_auth_req, &state->auth_req);
	}

	ndr_err = ndr_push_struct_blob(&state->auth_req_blob,
			state, &state->auth_req,
			(ndr_push_flags_fn_t)ndr_push_named_pipe_auth_req);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		tevent_req_error(req, EINVAL);
		goto post;
	}

	state->auth_req_iov.iov_base = (char *) state->auth_req_blob.data;
	state->auth_req_iov.iov_len = state->auth_req_blob.length;

	subreq = tstream_unix_connect_send(state,
					   state->caller.ev,
					   state->unix_laddr,
					   state->unix_raddr);
	if (tevent_req_nomem(subreq, req)) {
		goto post;
	}
	tevent_req_set_callback(subreq, tstream_npa_connect_unix_done, req);

	return req;

post:
	tevent_req_post(req, ev);
	return req;
}

static void tstream_npa_connect_writev_done(struct tevent_req *subreq);

static void tstream_npa_connect_unix_done(struct tevent_req *subreq)
{
	struct tevent_req *req =
		tevent_req_callback_data(subreq,
		struct tevent_req);
	struct tstream_npa_connect_state *state =
		tevent_req_data(req,
		struct tstream_npa_connect_state);
	int ret;
	int sys_errno;

	ret = tstream_unix_connect_recv(subreq, &sys_errno,
					state, &state->unix_stream);
	TALLOC_FREE(subreq);
	if (ret == -1) {
		tevent_req_error(req, sys_errno);
		return;
	}

	subreq = tstream_writev_send(state,
				     state->caller.ev,
				     state->unix_stream,
				     &state->auth_req_iov, 1);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, tstream_npa_connect_writev_done, req);
}

static int tstream_npa_connect_next_vector(struct tstream_context *unix_stream,
					   void *private_data,
					   TALLOC_CTX *mem_ctx,
					   struct iovec **_vector,
					   size_t *_count);
static void tstream_npa_connect_readv_done(struct tevent_req *subreq);

static void tstream_npa_connect_writev_done(struct tevent_req *subreq)
{
	struct tevent_req *req =
		tevent_req_callback_data(subreq,
		struct tevent_req);
	struct tstream_npa_connect_state *state =
		tevent_req_data(req,
		struct tstream_npa_connect_state);
	int ret;
	int sys_errno;

	ret = tstream_writev_recv(subreq, &sys_errno);
	TALLOC_FREE(subreq);
	if (ret == -1) {
		tevent_req_error(req, sys_errno);
		return;
	}

	state->auth_rep_blob = data_blob_const(NULL, 0);

	subreq = tstream_readv_pdu_send(state, state->caller.ev,
					state->unix_stream,
					tstream_npa_connect_next_vector,
					state);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, tstream_npa_connect_readv_done, req);
}

static int tstream_npa_connect_next_vector(struct tstream_context *unix_stream,
					   void *private_data,
					   TALLOC_CTX *mem_ctx,
					   struct iovec **_vector,
					   size_t *_count)
{
	struct tstream_npa_connect_state *state = talloc_get_type_abort(private_data,
					struct tstream_npa_connect_state);
	struct iovec *vector;
	size_t count;
	off_t ofs = 0;

	if (state->auth_rep_blob.length == 0) {
		state->auth_rep_blob = data_blob_talloc(state, NULL, 4);
		if (!state->auth_rep_blob.data) {
			return -1;
		}
	} else if (state->auth_rep_blob.length == 4) {
		uint32_t msg_len;

		ofs = 4;

		msg_len = RIVAL(state->auth_rep_blob.data, 0);

		if (msg_len > 0x00FFFFFF) {
			errno = EMSGSIZE;
			return -1;
		}

		if (msg_len == 0) {
			errno = EMSGSIZE;
			return -1;
		}

		msg_len += ofs;

		state->auth_rep_blob.data = talloc_realloc(state,
						state->auth_rep_blob.data,
						uint8_t, msg_len);
		if (!state->auth_rep_blob.data) {
			return -1;
		}
		state->auth_rep_blob.length = msg_len;
	} else {
		*_vector = NULL;
		*_count = 0;
		return 0;
	}

	/* we need to get a message header */
	vector = talloc_array(mem_ctx, struct iovec, 1);
	if (!vector) {
		return -1;
	}
	vector[0].iov_base = (char *) (state->auth_rep_blob.data + ofs);
	vector[0].iov_len = state->auth_rep_blob.length - ofs;
	count = 1;

	*_vector = vector;
	*_count = count;
	return 0;
}

static void tstream_npa_connect_readv_done(struct tevent_req *subreq)
{
	struct tevent_req *req =
		tevent_req_callback_data(subreq,
		struct tevent_req);
	struct tstream_npa_connect_state *state =
		tevent_req_data(req,
		struct tstream_npa_connect_state);
	int ret;
	int sys_errno;
	enum ndr_err_code ndr_err;

	ret = tstream_readv_pdu_recv(subreq, &sys_errno);
	TALLOC_FREE(subreq);
	if (ret == -1) {
		tevent_req_error(req, sys_errno);
		return;
	}

	DEBUG(10,("name_pipe_auth_rep(client)[%u]\n",
		 (uint32_t)state->auth_rep_blob.length));
	dump_data(11, state->auth_rep_blob.data, state->auth_rep_blob.length);

	ndr_err = ndr_pull_struct_blob(
		&state->auth_rep_blob, state,
		&state->auth_rep,
		(ndr_pull_flags_fn_t)ndr_pull_named_pipe_auth_rep);

	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DEBUG(0, ("ndr_pull_named_pipe_auth_rep failed: %s\n",
			  ndr_map_error2string(ndr_err)));
		tevent_req_error(req, EIO);
		return;
	}

	if (DEBUGLVL(10)) {
		NDR_PRINT_DEBUG(named_pipe_auth_rep, &state->auth_rep);
	}

	if (state->auth_rep.length < 16) {
		DEBUG(0, ("req invalid length: %u < 16\n",
			  state->auth_rep.length));
		tevent_req_error(req, EIO);
		return;
	}

	if (strcmp(NAMED_PIPE_AUTH_MAGIC, state->auth_rep.magic) != 0) {
		DEBUG(0, ("req invalid magic: %s != %s\n",
			  state->auth_rep.magic, NAMED_PIPE_AUTH_MAGIC));
		tevent_req_error(req, EIO);
		return;
	}

	if (!NT_STATUS_IS_OK(state->auth_rep.status)) {
		DEBUG(0, ("req failed: %s\n",
			  nt_errstr(state->auth_rep.status)));
		tevent_req_error(req, EACCES);
		return;
	}

	if (state->auth_rep.level != state->auth_req.level) {
		DEBUG(0, ("req invalid level: %u != %u\n",
			  state->auth_rep.level, state->auth_req.level));
		tevent_req_error(req, EIO);
		return;
	}

	tevent_req_done(req);
}

int _tstream_npa_connect_recv(struct tevent_req *req,
			      int *perrno,
			      TALLOC_CTX *mem_ctx,
			      struct tstream_context **_stream,
			      uint16_t *_file_type,
			      uint16_t *_device_state,
			      uint64_t *_allocation_size,
			      const char *location)
{
	struct tstream_npa_connect_state *state =
		tevent_req_data(req,
		struct tstream_npa_connect_state);
	struct tstream_context *stream;
	struct tstream_npa *npas;
	uint16_t device_state = 0;
	uint64_t allocation_size = 0;

	if (tevent_req_is_unix_error(req, perrno)) {
		tevent_req_received(req);
		return -1;
	}

	stream = tstream_context_create(mem_ctx,
					&tstream_npa_ops,
					&npas,
					struct tstream_npa,
					location);
	if (!stream) {
		*perrno = ENOMEM;
		tevent_req_received(req);
		return -1;
	}
	ZERO_STRUCTP(npas);

	npas->unix_stream = talloc_move(stream, &state->unix_stream);
	switch (state->auth_rep.level) {
	case 4:
		npas->file_type = state->auth_rep.info.info4.file_type;
		device_state = state->auth_rep.info.info4.device_state;
		allocation_size = state->auth_rep.info.info4.allocation_size;
		break;
	}

	*_stream = stream;
	*_file_type = npas->file_type;
	*_device_state = device_state;
	*_allocation_size = allocation_size;
	tevent_req_received(req);
	return 0;
}

static ssize_t tstream_npa_pending_bytes(struct tstream_context *stream)
{
	struct tstream_npa *npas = tstream_context_data(stream,
				   struct tstream_npa);
	ssize_t ret;

	if (!npas->unix_stream) {
		errno = ENOTCONN;
		return -1;
	}

	switch (npas->file_type) {
	case FILE_TYPE_BYTE_MODE_PIPE:
		ret = tstream_pending_bytes(npas->unix_stream);
		break;

	case FILE_TYPE_MESSAGE_MODE_PIPE:
		ret = npas->pending.iov_len;
		break;

	default:
		ret = -1;
	}

	return ret;
}

struct tstream_npa_readv_state {
	struct tstream_context *stream;

	struct iovec *vector;
	size_t count;

	/* the header for message mode */
	uint8_t hdr[2];
	bool wait_for_hdr;

	int ret;
};

static void tstream_npa_readv_byte_mode_handler(struct tevent_req *subreq);
static int tstream_npa_readv_next_vector(struct tstream_context *stream,
					 void *private_data,
					 TALLOC_CTX *mem_ctx,
					 struct iovec **_vector,
					 size_t *_count);
static void tstream_npa_readv_msg_mode_handler(struct tevent_req *subreq);

static struct tevent_req *tstream_npa_readv_send(TALLOC_CTX *mem_ctx,
					struct tevent_context *ev,
					struct tstream_context *stream,
					struct iovec *vector,
					size_t count)
{
	struct tevent_req *req;
	struct tstream_npa_readv_state *state;
	struct tstream_npa *npas = tstream_context_data(stream, struct tstream_npa);
	struct tevent_req *subreq;
	off_t ofs;
	size_t left;
	uint8_t *pbase;

	req = tevent_req_create(mem_ctx, &state,
				struct tstream_npa_readv_state);
	if (!req) {
		return NULL;
	}

	state->stream	= stream;
	state->ret	= 0;

	if (!npas->unix_stream) {
		tevent_req_error(req, ENOTCONN);
		goto post;
	}

	switch (npas->file_type) {
	case FILE_TYPE_BYTE_MODE_PIPE:
		state->vector = vector;
		state->count = count;

		subreq = tstream_readv_send(state,
					    ev,
					    npas->unix_stream,
					    state->vector,
					    state->count);
		if (tevent_req_nomem(subreq,req)) {
			goto post;
		}
		tevent_req_set_callback(subreq,
					tstream_npa_readv_byte_mode_handler,
					req);

		return req;

	case FILE_TYPE_MESSAGE_MODE_PIPE:
		/*
		 * we make a copy of the vector and prepend a header
		 * with the length
		 */
		state->vector	= talloc_array(state, struct iovec, count);
		if (tevent_req_nomem(state->vector, req)) {
			goto post;
		}
		memcpy(state->vector, vector, sizeof(struct iovec)*count);
		state->count = count;

		/*
		 * copy the pending buffer first
		 */
		ofs = 0;
		left = npas->pending.iov_len;
		pbase = (uint8_t *)npas->pending.iov_base;

		while (left > 0 && state->count > 0) {
			uint8_t *base;
			base = (uint8_t *)state->vector[0].iov_base;
			if (left < state->vector[0].iov_len) {
				memcpy(base, pbase + ofs, left);

				base += left;
				state->vector[0].iov_base = (char *) base;
				state->vector[0].iov_len -= left;

				ofs += left;
				left = 0;
				TALLOC_FREE(pbase);
				ZERO_STRUCT(npas->pending);
				break;
			}
			memcpy(base, pbase + ofs, state->vector[0].iov_len);

			ofs += state->vector[0].iov_len;
			left -= state->vector[0].iov_len;
			state->vector += 1;
			state->count -= 1;

			if (left == 0) {
				TALLOC_FREE(pbase);
				ZERO_STRUCT(npas->pending);
				break;
			}
		}

		if (left > 0) {
			memmove(pbase, pbase + ofs, left);
			npas->pending.iov_base = (char *) pbase;
			npas->pending.iov_len = left;
			/*
			 * this cannot fail and even if it
			 * fails we can handle it
			 */
			pbase = talloc_realloc(npas, pbase, uint8_t, left);
			if (pbase) {
				npas->pending.iov_base = (char *) pbase;
			}
			pbase = NULL;
		}

		state->ret += ofs;

		if (state->count == 0) {
			tevent_req_done(req);
			goto post;
		}

		ZERO_STRUCT(state->hdr);
		state->wait_for_hdr = false;

		subreq = tstream_readv_pdu_send(state,
						ev,
						npas->unix_stream,
						tstream_npa_readv_next_vector,
						state);
		if (tevent_req_nomem(subreq, req)) {
			goto post;
		}
		tevent_req_set_callback(subreq,
					tstream_npa_readv_msg_mode_handler,
					req);

		return req;
	}

	/* this can't happen */
	tevent_req_error(req, EINVAL);
	goto post;

 post:
	tevent_req_post(req, ev);
	return req;
}

static void tstream_npa_readv_byte_mode_handler(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(subreq,
				 struct tevent_req);
	struct tstream_npa_readv_state *state = tevent_req_data(req,
					struct tstream_npa_readv_state);
	int ret;
	int sys_errno;

	ret = tstream_readv_recv(subreq, &sys_errno);
	TALLOC_FREE(subreq);
	if (ret == -1) {
		tevent_req_error(req, sys_errno);
		return;
	}

	state->ret = ret;

	tevent_req_done(req);
}

static int tstream_npa_readv_next_vector(struct tstream_context *unix_stream,
					 void *private_data,
					 TALLOC_CTX *mem_ctx,
					 struct iovec **_vector,
					 size_t *_count)
{
	struct tstream_npa_readv_state *state = talloc_get_type_abort(private_data,
					struct tstream_npa_readv_state);
	struct tstream_npa *npas = tstream_context_data(state->stream,
				   struct tstream_npa);
	struct iovec *vector;
	size_t count;
	uint16_t msg_len;
	size_t left;

	if (state->count == 0) {
		*_vector = NULL;
		*_count = 0;
		return 0;
	}

	if (!state->wait_for_hdr) {
		/* we need to get a message header */
		vector = talloc_array(mem_ctx, struct iovec, 1);
		if (!vector) {
			return -1;
		}
		ZERO_STRUCT(state->hdr);
		vector[0].iov_base = (char *) state->hdr;
		vector[0].iov_len = sizeof(state->hdr);

		count = 1;

		state->wait_for_hdr = true;

		*_vector = vector;
		*_count = count;
		return 0;
	}

	/* and now fill the callers buffers and maybe the pending buffer */
	state->wait_for_hdr = false;

	msg_len = SVAL(state->hdr, 0);

	if (msg_len == 0) {
		errno = EIO;
		return -1;
	}

	state->wait_for_hdr = false;

	/* +1 because we may need to fill the pending buffer */
	vector = talloc_array(mem_ctx, struct iovec, state->count + 1);
	if (!vector) {
		return -1;
	}

	count = 0;
	left = msg_len;
	while (left > 0 && state->count > 0) {
		if (left < state->vector[0].iov_len) {
			uint8_t *base;
			base = (uint8_t *)state->vector[0].iov_base;
			vector[count].iov_base = (char *) base;
			vector[count].iov_len = left;
			count++;
			base += left;
			state->vector[0].iov_base = (char *) base;
			state->vector[0].iov_len -= left;
			break;
		}
		vector[count] = state->vector[0];
		count++;
		left -= state->vector[0].iov_len;
		state->vector += 1;
		state->count -= 1;
	}

	if (left > 0) {
		/*
		 * if the message is longer than the buffers the caller
		 * requested, we need to consume the rest of the message
		 * into the pending buffer, where the next readv can
		 * be served from.
		 */
		npas->pending.iov_base = talloc_array(npas, char, left);
		if (!npas->pending.iov_base) {
			return -1;
		}
		npas->pending.iov_len = left;

		vector[count] = npas->pending;
		count++;
	}

	state->ret += (msg_len - left);

	*_vector = vector;
	*_count = count;
	return 0;
}

static void tstream_npa_readv_msg_mode_handler(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(subreq,
				 struct tevent_req);
	int ret;
	int sys_errno;

	ret = tstream_readv_pdu_recv(subreq, &sys_errno);
	TALLOC_FREE(subreq);
	if (ret == -1) {
		tevent_req_error(req, sys_errno);
		return;
	}

	/*
	 * we do not set state->ret here as ret includes the headr size.
	 * we set it in tstream_npa_readv_pdu_next_vector()
	 */

	tevent_req_done(req);
}

static int tstream_npa_readv_recv(struct tevent_req *req,
				   int *perrno)
{
	struct tstream_npa_readv_state *state = tevent_req_data(req,
					struct tstream_npa_readv_state);
	int ret;

	ret = tsocket_simple_int_recv(req, perrno);
	if (ret == 0) {
		ret = state->ret;
	}

	tevent_req_received(req);
	return ret;
}

struct tstream_npa_writev_state {
	const struct iovec *vector;
	size_t count;

	/* the header for message mode */
	bool hdr_used;
	uint8_t hdr[2];

	int ret;
};

static void tstream_npa_writev_handler(struct tevent_req *subreq);

static struct tevent_req *tstream_npa_writev_send(TALLOC_CTX *mem_ctx,
					struct tevent_context *ev,
					struct tstream_context *stream,
					const struct iovec *vector,
					size_t count)
{
	struct tevent_req *req;
	struct tstream_npa_writev_state *state;
	struct tstream_npa *npas = tstream_context_data(stream, struct tstream_npa);
	struct tevent_req *subreq;
	size_t msg_len;
	size_t i;
	struct iovec *new_vector;

	req = tevent_req_create(mem_ctx, &state,
				struct tstream_npa_writev_state);
	if (!req) {
		return NULL;
	}

	state->ret	= 0;

	if (!npas->unix_stream) {
		tevent_req_error(req, ENOTCONN);
		goto post;
	}

	switch (npas->file_type) {
	case FILE_TYPE_BYTE_MODE_PIPE:
		state->hdr_used	= false;
		state->vector	= vector;
		state->count	= count;
		break;

	case FILE_TYPE_MESSAGE_MODE_PIPE:
		/*
		 * we make a copy of the vector and prepend a header
		 * with the length
		 */
		new_vector	= talloc_array(state, struct iovec, count + 1);
		if (tevent_req_nomem(new_vector, req)) {
			goto post;
		}
		new_vector[0].iov_base = (char *) state->hdr;
		new_vector[0].iov_len = sizeof(state->hdr);
		memcpy(new_vector + 1, vector, sizeof(struct iovec)*count);

		state->hdr_used	= true;
		state->vector	= new_vector;
		state->count	= count + 1;

		msg_len = 0;
		for (i=0; i < count; i++) {
			msg_len += vector[i].iov_len;
		}

		if (msg_len > UINT16_MAX) {
			tevent_req_error(req, EMSGSIZE);
			goto post;
		}

		SSVAL(state->hdr, 0, msg_len);
		break;
	}

	subreq = tstream_writev_send(state,
				     ev,
				     npas->unix_stream,
				     state->vector,
				     state->count);
	if (tevent_req_nomem(subreq, req)) {
		goto post;
	}
	tevent_req_set_callback(subreq, tstream_npa_writev_handler, req);

	return req;

 post:
	tevent_req_post(req, ev);
	return req;
}

static void tstream_npa_writev_handler(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(subreq,
				 struct tevent_req);
	struct tstream_npa_writev_state *state = tevent_req_data(req,
					struct tstream_npa_writev_state);
	int ret;
	int sys_errno;

	ret = tstream_writev_recv(subreq, &sys_errno);
	TALLOC_FREE(subreq);
	if (ret == -1) {
		tevent_req_error(req, sys_errno);
		return;
	}

	/*
	 * in message mode we need to hide the length
	 * of the hdr from the caller
	 */
	if (state->hdr_used) {
		ret -= sizeof(state->hdr);
	}

	state->ret = ret;

	tevent_req_done(req);
}

static int tstream_npa_writev_recv(struct tevent_req *req,
				   int *perrno)
{
	struct tstream_npa_writev_state *state = tevent_req_data(req,
					struct tstream_npa_writev_state);
	int ret;

	ret = tsocket_simple_int_recv(req, perrno);
	if (ret == 0) {
		ret = state->ret;
	}

	tevent_req_received(req);
	return ret;
}

struct tstream_npa_disconnect_state {
	struct tstream_context *stream;
};

static void tstream_npa_disconnect_handler(struct tevent_req *subreq);

static struct tevent_req *tstream_npa_disconnect_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct tstream_context *stream)
{
	struct tstream_npa *npas = tstream_context_data(stream, struct tstream_npa);
	struct tevent_req *req;
	struct tstream_npa_disconnect_state *state;
	struct tevent_req *subreq;

	req = tevent_req_create(mem_ctx, &state,
				struct tstream_npa_disconnect_state);
	if (req == NULL) {
		return NULL;
	}

	state->stream = stream;

	if (!npas->unix_stream) {
		tevent_req_error(req, ENOTCONN);
		goto post;
	}

	subreq = tstream_disconnect_send(state,
					 ev,
					 npas->unix_stream);
	if (tevent_req_nomem(subreq, req)) {
		goto post;
	}
	tevent_req_set_callback(subreq, tstream_npa_disconnect_handler, req);

	return req;

post:
	tevent_req_post(req, ev);
	return req;
}

static void tstream_npa_disconnect_handler(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(subreq,
				 struct tevent_req);
	struct tstream_npa_disconnect_state *state = tevent_req_data(req,
					struct tstream_npa_disconnect_state);
	struct tstream_context *stream = state->stream;
	struct tstream_npa *npas = tstream_context_data(stream, struct tstream_npa);
	int ret;
	int sys_errno;

	ret = tstream_disconnect_recv(subreq, &sys_errno);
	TALLOC_FREE(subreq);
	if (ret == -1) {
		tevent_req_error(req, sys_errno);
		return;
	}

	TALLOC_FREE(npas->unix_stream);

	tevent_req_done(req);
}

static int tstream_npa_disconnect_recv(struct tevent_req *req,
				       int *perrno)
{
	int ret;

	ret = tsocket_simple_int_recv(req, perrno);

	tevent_req_received(req);
	return ret;
}

static const struct tstream_context_ops tstream_npa_ops = {
	.name			= "npa",

	.pending_bytes		= tstream_npa_pending_bytes,

	.readv_send		= tstream_npa_readv_send,
	.readv_recv		= tstream_npa_readv_recv,

	.writev_send		= tstream_npa_writev_send,
	.writev_recv		= tstream_npa_writev_recv,

	.disconnect_send	= tstream_npa_disconnect_send,
	.disconnect_recv	= tstream_npa_disconnect_recv,
};

int _tstream_npa_existing_socket(TALLOC_CTX *mem_ctx,
				 int fd,
				 uint16_t file_type,
				 struct tstream_context **_stream,
				 const char *location)
{
	struct tstream_context *stream;
	struct tstream_npa *npas;
	int ret;

	switch (file_type) {
	case FILE_TYPE_BYTE_MODE_PIPE:
		break;
	case FILE_TYPE_MESSAGE_MODE_PIPE:
		break;
	default:
		errno = EINVAL;
		return -1;
	}

	stream = tstream_context_create(mem_ctx,
					&tstream_npa_ops,
					&npas,
					struct tstream_npa,
					location);
	if (!stream) {
		return -1;
	}
	ZERO_STRUCTP(npas);

	npas->file_type = file_type;

	ret = tstream_bsd_existing_socket(stream, fd,
					  &npas->unix_stream);
	if (ret == -1) {
		int saved_errno = errno;
		talloc_free(stream);
		errno = saved_errno;
		return -1;
	}

	*_stream = stream;
	return 0;
}


struct tstream_npa_accept_state {
	struct tevent_context *ev;
	struct tstream_context *plain;
	uint16_t file_type;
	uint16_t device_state;
	uint64_t alloc_size;

	DATA_BLOB npa_blob;
	struct iovec out_iov;

	/* results */
	NTSTATUS accept_status;
	struct tsocket_address *client;
	char *client_name;
	struct tsocket_address *server;
	char *server_name;
	struct auth_session_info_transport *session_info;
};

static int tstream_npa_accept_next_vector(struct tstream_context *unix_stream,
					  void *private_data,
					  TALLOC_CTX *mem_ctx,
					  struct iovec **_vector,
					  size_t *_count);
static void tstream_npa_accept_existing_reply(struct tevent_req *subreq);
static void tstream_npa_accept_existing_done(struct tevent_req *subreq);

struct tevent_req *tstream_npa_accept_existing_send(TALLOC_CTX *mem_ctx,
					struct tevent_context *ev,
					struct tstream_context *plain,
					uint16_t file_type,
					uint16_t device_state,
					uint64_t allocation_size)
{
	struct tstream_npa_accept_state *state;
	struct tevent_req *req, *subreq;

	req = tevent_req_create(mem_ctx, &state,
				struct tstream_npa_accept_state);
	if (req == NULL) {
		return NULL;
	}

	switch (file_type) {
	case FILE_TYPE_BYTE_MODE_PIPE:
		break;
	case FILE_TYPE_MESSAGE_MODE_PIPE:
		break;
	default:
		tevent_req_error(req, EINVAL);
		goto post;
	}

	ZERO_STRUCTP(state);

	state->ev = ev;
	state->plain = plain;
	state->file_type = file_type;
	state->device_state = device_state;
	state->alloc_size = allocation_size;

	/*
	 * The named pipe pdu's have the length as 8 byte (initial_read_size),
	 * named_pipe_full_request provides the pdu length then.
	 */
	subreq = tstream_readv_pdu_send(state, ev, plain,
					tstream_npa_accept_next_vector,
					state);
	if (tevent_req_nomem(subreq, req)) {
		goto post;
	}

	tevent_req_set_callback(subreq,
				tstream_npa_accept_existing_reply, req);

	return req;

post:
	tevent_req_post(req, ev);
	return req;
}

static int tstream_npa_accept_next_vector(struct tstream_context *unix_stream,
					  void *private_data,
					  TALLOC_CTX *mem_ctx,
					  struct iovec **_vector,
					  size_t *_count)
{
	struct tstream_npa_accept_state *state =
		talloc_get_type_abort(private_data,
					struct tstream_npa_accept_state);
	struct iovec *vector;
	size_t count;
	off_t ofs = 0;

	if (state->npa_blob.length == 0) {
		state->npa_blob = data_blob_talloc(state, NULL, 4);
		if (!state->npa_blob.data) {
			return -1;
		}
	} else if (state->npa_blob.length == 4) {
		uint32_t msg_len;

		ofs = 4;

		msg_len = RIVAL(state->npa_blob.data, 0);

		if (msg_len > 0x00FFFFFF) {
			errno = EMSGSIZE;
			return -1;
		}

		if (msg_len == 0) {
			errno = EMSGSIZE;
			return -1;
		}

		msg_len += ofs;

		state->npa_blob.data = talloc_realloc(state,
						      state->npa_blob.data,
						      uint8_t, msg_len);
		if (!state->npa_blob.data) {
			return -1;
		}
		state->npa_blob.length = msg_len;
	} else {
		if (memcmp(&state->npa_blob.data[4],
			   NAMED_PIPE_AUTH_MAGIC, 4) != 0) {
			DEBUG(0, ("Wrong protocol\n"));
#if defined(EPROTONOSUPPORT)
			errno = EPROTONOSUPPORT;
#elif defined(EPROTO)
			errno = EPROTO;
#else
			errno = EINVAL;
#endif
			return -1;
		}
		*_vector = NULL;
		*_count = 0;
		return 0;
	}

	/* we need to get a message header */
	vector = talloc_array(mem_ctx, struct iovec, 1);
	if (!vector) {
		return -1;
	}
	vector[0].iov_base = (char *) (state->npa_blob.data + ofs);
	vector[0].iov_len = state->npa_blob.length - ofs;
	count = 1;

	*_vector = vector;
	*_count = count;
	return 0;
}

static void tstream_npa_accept_existing_reply(struct tevent_req *subreq)
{
	struct tevent_req *req =
			tevent_req_callback_data(subreq, struct tevent_req);
	struct tstream_npa_accept_state *state =
			tevent_req_data(req, struct tstream_npa_accept_state);
	struct named_pipe_auth_req *pipe_request;
	struct named_pipe_auth_rep pipe_reply;
	struct named_pipe_auth_req_info4 i4;
	enum ndr_err_code ndr_err;
	DATA_BLOB out;
	int sys_errno;
	int ret;

	ret = tstream_readv_pdu_recv(subreq, &sys_errno);
	TALLOC_FREE(subreq);
	if (ret == -1) {
		tevent_req_error(req, sys_errno);
		return;
	}

	DEBUG(10, ("Received packet of length %lu\n",
		   (long)state->npa_blob.length));
	dump_data(11, state->npa_blob.data, state->npa_blob.length);

	ZERO_STRUCT(pipe_reply);
	pipe_reply.level = 0;
	pipe_reply.status = NT_STATUS_INTERNAL_ERROR;
	/*
	 * TODO: check it's a root (uid == 0) pipe
	 */

	pipe_request = talloc(state, struct named_pipe_auth_req);
	if (!pipe_request) {
		DEBUG(0, ("Out of memory!\n"));
		goto reply;
	}

	/* parse the passed credentials */
	ndr_err = ndr_pull_struct_blob_all(
			&state->npa_blob, pipe_request, pipe_request,
			(ndr_pull_flags_fn_t)ndr_pull_named_pipe_auth_req);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		pipe_reply.status = ndr_map_error2ntstatus(ndr_err);
		DEBUG(2, ("Could not unmarshall named_pipe_auth_req: %s\n",
			  nt_errstr(pipe_reply.status)));
		goto reply;
	}

	if (DEBUGLVL(10)) {
		NDR_PRINT_DEBUG(named_pipe_auth_req, pipe_request);
	}

	ZERO_STRUCT(i4);

	if (pipe_request->level != 4) {
		DEBUG(0, ("Unknown level %u\n", pipe_request->level));
		pipe_reply.level = 0;
		pipe_reply.status = NT_STATUS_INVALID_LEVEL;
		goto reply;
	}

	pipe_reply.level = 4;
	pipe_reply.status = NT_STATUS_OK;
	pipe_reply.info.info4.file_type = state->file_type;
	pipe_reply.info.info4.device_state = state->device_state;
	pipe_reply.info.info4.allocation_size = state->alloc_size;

	i4 = pipe_request->info.info4;
	if (i4.server_addr == NULL) {
		pipe_reply.status = NT_STATUS_INVALID_ADDRESS;
		DEBUG(2, ("Missing server address\n"));
		goto reply;
	}
	if (i4.client_addr == NULL) {
		pipe_reply.status = NT_STATUS_INVALID_ADDRESS;
		DEBUG(2, ("Missing client address\n"));
		goto reply;
	}

	state->server_name = discard_const_p(char,
					     talloc_move(state, &i4.server_name));
	ret = tsocket_address_inet_from_strings(state, "ip",
						i4.server_addr,
						i4.server_port,
						&state->server);
	if (ret != 0) {
		DEBUG(2, ("Invalid server address[%s:%u] - %s\n",
			  i4.server_addr, i4.server_port,
			  strerror(errno)));
		pipe_reply.status = NT_STATUS_INVALID_ADDRESS;
		goto reply;
	}

	state->client_name = discard_const_p(char,
					     talloc_move(state, &i4.client_name));
	ret = tsocket_address_inet_from_strings(state, "ip",
						i4.client_addr,
						i4.client_port,
						&state->client);
	if (ret != 0) {
		DEBUG(2, ("Invalid server address[%s:%u] - %s\n",
			  i4.client_addr, i4.client_port,
			  strerror(errno)));
		pipe_reply.status = NT_STATUS_INVALID_ADDRESS;
		goto reply;
	}

	state->session_info = talloc_move(state, &i4.session_info);
reply:
	/* create the output */
	ndr_err = ndr_push_struct_blob(&out, state, &pipe_reply,
			(ndr_push_flags_fn_t)ndr_push_named_pipe_auth_rep);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DEBUG(2, ("Error encoding structure: %s",
			  ndr_map_error2string(ndr_err)));
		tevent_req_error(req, EIO);
		return;
	}

	DEBUG(10, ("named_pipe_auth reply[%u]\n", (unsigned)out.length));
	dump_data(11, out.data, out.length);

	if (DEBUGLVL(10)) {
		NDR_PRINT_DEBUG(named_pipe_auth_rep, &pipe_reply);
	}

	state->accept_status = pipe_reply.status;

	state->out_iov.iov_base = (char *) out.data;
	state->out_iov.iov_len = out.length;

	subreq = tstream_writev_send(state, state->ev,
				     state->plain,
				     &state->out_iov, 1);
	if (tevent_req_nomem(subreq, req)) {
		DEBUG(0, ("no memory for tstream_writev_send"));
		return;
	}

	tevent_req_set_callback(subreq, tstream_npa_accept_existing_done, req);
}

static void tstream_npa_accept_existing_done(struct tevent_req *subreq)
{
	struct tevent_req *req =
			tevent_req_callback_data(subreq, struct tevent_req);
	int sys_errno;
	int ret;

	ret = tstream_writev_recv(subreq, &sys_errno);
	TALLOC_FREE(subreq);
	if (ret == -1) {
		tevent_req_error(req, sys_errno);
		return;
	}

	tevent_req_done(req);
}

int _tstream_npa_accept_existing_recv(struct tevent_req *req,
				      int *perrno,
				      TALLOC_CTX *mem_ctx,
				      struct tstream_context **stream,
				      struct tsocket_address **client,
				      char **_client_name,
				      struct tsocket_address **server,
				      char **server_name,
				      struct auth_session_info_transport **session_info,
				      const char *location)
{
	struct tstream_npa_accept_state *state =
			tevent_req_data(req, struct tstream_npa_accept_state);
	struct tstream_npa *npas;
	int ret;

	ret = tsocket_simple_int_recv(req, perrno);
	if (ret != 0) {
		DEBUG(2, ("Failed to accept named pipe conection: %s\n",
			  strerror(*perrno)));
		tevent_req_received(req);
		return -1;
	}

	if (!NT_STATUS_IS_OK(state->accept_status)) {
#if defined(EPROTONOSUPPORT)
		*perrno = EPROTONOSUPPORT;
#elif defined(EPROTO)
		*perrno = EPROTO;
#else
		*perrno = EINVAL;
#endif
		DEBUG(2, ("Failed to accept named pipe conection: %s => %s\n",
			  nt_errstr(state->accept_status),
			  strerror(*perrno)));
		tevent_req_received(req);
		return -1;
	}

	*stream = tstream_context_create(mem_ctx,
					 &tstream_npa_ops,
					 &npas,
					 struct tstream_npa,
					 location);
	if (!*stream) {
		*perrno = ENOMEM;
		tevent_req_received(req);
		return -1;
	}
	ZERO_STRUCTP(npas);
	npas->unix_stream = state->plain;
	npas->file_type = state->file_type;

	*client = talloc_move(mem_ctx, &state->client);
	*_client_name = talloc_move(mem_ctx, &state->client_name);
	*server = talloc_move(mem_ctx, &state->server);
	*server_name = talloc_move(mem_ctx, &state->server_name);
	*session_info = talloc_move(mem_ctx, &state->session_info);

	tevent_req_received(req);
	return 0;
}
