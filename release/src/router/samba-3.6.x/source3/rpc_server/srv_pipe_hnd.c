/*
 *  Unix SMB/CIFS implementation.
 *  RPC Pipe client / server routines
 *  Copyright (C) Andrew Tridgell              1992-1998,
 *  Largely re-written : 2005
 *  Copyright (C) Jeremy Allison		1998 - 2005
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"
#include "rpc_server.h"
#include "fake_file.h"
#include "rpc_dce.h"
#include "ntdomain.h"
#include "rpc_server/rpc_ncacn_np.h"
#include "rpc_server/srv_pipe_hnd.h"
#include "rpc_server/srv_pipe.h"
#include "../lib/tsocket/tsocket.h"
#include "../lib/util/tevent_ntstatus.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_RPC_SRV

/****************************************************************************
 Ensures we have at least RPC_HEADER_LEN amount of data in the incoming buffer.
****************************************************************************/

static ssize_t fill_rpc_header(struct pipes_struct *p, char *data, size_t data_to_copy)
{
	size_t len_needed_to_complete_hdr =
		MIN(data_to_copy, RPC_HEADER_LEN - p->in_data.pdu.length);

	DEBUG(10, ("fill_rpc_header: data_to_copy = %u, "
		   "len_needed_to_complete_hdr = %u, "
		   "receive_len = %u\n",
		   (unsigned int)data_to_copy,
		   (unsigned int)len_needed_to_complete_hdr,
		   (unsigned int)p->in_data.pdu.length ));

	if (p->in_data.pdu.data == NULL) {
		p->in_data.pdu.data = talloc_array(p, uint8_t, RPC_HEADER_LEN);
	}
	if (p->in_data.pdu.data == NULL) {
		DEBUG(0, ("talloc failed\n"));
		return -1;
	}

	memcpy((char *)&p->in_data.pdu.data[p->in_data.pdu.length],
		data, len_needed_to_complete_hdr);
	p->in_data.pdu.length += len_needed_to_complete_hdr;

	return (ssize_t)len_needed_to_complete_hdr;
}

static bool get_pdu_size(struct pipes_struct *p)
{
	uint16_t frag_len;
	/* the fill_rpc_header() call insures we copy only
	 * RPC_HEADER_LEN bytes. If this doesn't match then
	 * somethign is very wrong and we can only abort */
	if (p->in_data.pdu.length != RPC_HEADER_LEN) {
		DEBUG(0, ("Unexpected RPC Header size! "
			  "got %d, expected %d)\n",
			  (int)p->in_data.pdu.length,
			  RPC_HEADER_LEN));
		set_incoming_fault(p);
		return false;
	}

	frag_len = dcerpc_get_frag_length(&p->in_data.pdu);

	/* verify it is a reasonable value */
	if ((frag_len < RPC_HEADER_LEN) ||
	    (frag_len > RPC_MAX_PDU_FRAG_LEN)) {
		DEBUG(0, ("Unexpected RPC Fragment size! (%d)\n",
			  frag_len));
		set_incoming_fault(p);
		return false;
	}

	p->in_data.pdu_needed_len = frag_len - RPC_HEADER_LEN;

	/* allocate the space needed to fill the pdu */
	p->in_data.pdu.data = talloc_realloc(p, p->in_data.pdu.data,
						uint8_t, frag_len);
	if (p->in_data.pdu.data == NULL) {
		DEBUG(0, ("talloc_realloc failed\n"));
		set_incoming_fault(p);
		return false;
	}

	return true;
}

/****************************************************************************
  Call this to free any talloc'ed memory. Do this after processing
  a complete incoming and outgoing request (multiple incoming/outgoing
  PDU's).
****************************************************************************/

static void free_pipe_context(struct pipes_struct *p)
{
	data_blob_free(&p->out_data.frag);
	data_blob_free(&p->out_data.rdata);
	data_blob_free(&p->in_data.data);

	DEBUG(3, ("free_pipe_context: "
		"destroying talloc pool of size %lu\n",
		(unsigned long)talloc_total_size(p->mem_ctx)));
	talloc_free_children(p->mem_ctx);
}

/****************************************************************************
 Accepts incoming data on an rpc pipe. Processes the data in pdu sized units.
****************************************************************************/

ssize_t process_incoming_data(struct pipes_struct *p, char *data, size_t n)
{
	size_t data_to_copy = MIN(n, RPC_MAX_PDU_FRAG_LEN
					- p->in_data.pdu.length);

	DEBUG(10, ("process_incoming_data: Start: pdu.length = %u, "
		   "pdu_needed_len = %u, incoming data = %u\n",
		   (unsigned int)p->in_data.pdu.length,
		   (unsigned int)p->in_data.pdu_needed_len,
		   (unsigned int)n ));

	if(data_to_copy == 0) {
		/*
		 * This is an error - data is being received and there is no
		 * space in the PDU. Free the received data and go into the
		 * fault state.
		 */
		DEBUG(0, ("process_incoming_data: "
			  "No space in incoming pdu buffer. "
			  "Current size = %u incoming data size = %u\n",
			  (unsigned int)p->in_data.pdu.length,
			  (unsigned int)n));
		set_incoming_fault(p);
		return -1;
	}

	/*
	 * If we have no data already, wait until we get at least
	 * a RPC_HEADER_LEN * number of bytes before we can do anything.
	 */

	if ((p->in_data.pdu_needed_len == 0) &&
	    (p->in_data.pdu.length < RPC_HEADER_LEN)) {
		/*
		 * Always return here. If we have more data then the RPC_HEADER
		 * will be processed the next time around the loop.
		 */
		return fill_rpc_header(p, data, data_to_copy);
	}

	/*
	 * At this point we know we have at least an RPC_HEADER_LEN amount of
	 * data stored in p->in_data.pdu.
	 */

	/*
	 * If pdu_needed_len is zero this is a new pdu.
	 * Check how much more data we need, then loop again.
	 */
	if (p->in_data.pdu_needed_len == 0) {

		bool ok = get_pdu_size(p);
		if (!ok) {
			return -1;
		}
		if (p->in_data.pdu_needed_len > 0) {
			return 0;
		}

		/* If rret == 0 and pdu_needed_len == 0 here we have a PDU
		 * that consists of an RPC_HEADER only. This is a
		 * DCERPC_PKT_SHUTDOWN, DCERPC_PKT_CO_CANCEL or
		 * DCERPC_PKT_ORPHANED pdu type.
		 * Deal with this in process_complete_pdu(). */
	}

	/*
	 * Ok - at this point we have a valid RPC_HEADER.
	 * Keep reading until we have a full pdu.
	 */

	data_to_copy = MIN(data_to_copy, p->in_data.pdu_needed_len);

	/*
	 * Copy as much of the data as we need into the p->in_data.pdu buffer.
	 * pdu_needed_len becomes zero when we have a complete pdu.
	 */

	memcpy((char *)&p->in_data.pdu.data[p->in_data.pdu.length],
		data, data_to_copy);
	p->in_data.pdu.length += data_to_copy;
	p->in_data.pdu_needed_len -= data_to_copy;

	/*
	 * Do we have a complete PDU ?
	 * (return the number of bytes handled in the call)
	 */

	if(p->in_data.pdu_needed_len == 0) {
		process_complete_pdu(p);
		return data_to_copy;
	}

	DEBUG(10, ("process_incoming_data: not a complete PDU yet. "
		   "pdu.length = %u, pdu_needed_len = %u\n",
		   (unsigned int)p->in_data.pdu.length,
		   (unsigned int)p->in_data.pdu_needed_len));

	return (ssize_t)data_to_copy;
}

/****************************************************************************
 Accepts incoming data on an internal rpc pipe.
****************************************************************************/

static ssize_t write_to_internal_pipe(struct pipes_struct *p, char *data, size_t n)
{
	size_t data_left = n;

	while(data_left) {
		ssize_t data_used;

		DEBUG(10, ("write_to_pipe: data_left = %u\n",
			  (unsigned int)data_left));

		data_used = process_incoming_data(p, data, data_left);

		DEBUG(10, ("write_to_pipe: data_used = %d\n",
			   (int)data_used));

		if(data_used < 0) {
			return -1;
		}

		data_left -= data_used;
		data += data_used;
	}

	return n;
}

/****************************************************************************
 Replies to a request to read data from a pipe.

 Headers are interspersed with the data at PDU intervals. By the time
 this function is called, the start of the data could possibly have been
 read by an SMBtrans (file_offset != 0).

 Calling create_rpc_reply() here is a hack. The data should already
 have been prepared into arrays of headers + data stream sections.
****************************************************************************/

static ssize_t read_from_internal_pipe(struct pipes_struct *p, char *data,
				       size_t n, bool *is_data_outstanding)
{
	uint32 pdu_remaining = 0;
	ssize_t data_returned = 0;

	if (!p) {
		DEBUG(0,("read_from_pipe: pipe not open\n"));
		return -1;
	}

	DEBUG(6,(" name: %s len: %u\n",
		 get_pipe_name_from_syntax(talloc_tos(), &p->syntax),
		 (unsigned int)n));

	/*
	 * We cannot return more than one PDU length per
	 * read request.
	 */

	/*
	 * This condition should result in the connection being closed.
	 * Netapp filers seem to set it to 0xffff which results in domain
	 * authentications failing.  Just ignore it so things work.
	 */

	if(n > RPC_MAX_PDU_FRAG_LEN) {
                DEBUG(5,("read_from_pipe: too large read (%u) requested on "
			 "pipe %s. We can only service %d sized reads.\n",
			 (unsigned int)n,
			 get_pipe_name_from_syntax(talloc_tos(), &p->syntax),
			 RPC_MAX_PDU_FRAG_LEN ));
		n = RPC_MAX_PDU_FRAG_LEN;
	}

	/*
 	 * Determine if there is still data to send in the
	 * pipe PDU buffer. Always send this first. Never
	 * send more than is left in the current PDU. The
	 * client should send a new read request for a new
	 * PDU.
	 */

	pdu_remaining = p->out_data.frag.length
		- p->out_data.current_pdu_sent;

	if (pdu_remaining > 0) {
		data_returned = (ssize_t)MIN(n, pdu_remaining);

		DEBUG(10,("read_from_pipe: %s: current_pdu_len = %u, "
			  "current_pdu_sent = %u returning %d bytes.\n",
			  get_pipe_name_from_syntax(talloc_tos(), &p->syntax),
			  (unsigned int)p->out_data.frag.length,
			  (unsigned int)p->out_data.current_pdu_sent,
			  (int)data_returned));

		memcpy(data,
		       p->out_data.frag.data
		       + p->out_data.current_pdu_sent,
		       data_returned);

		p->out_data.current_pdu_sent += (uint32)data_returned;
		goto out;
	}

	/*
	 * At this point p->current_pdu_len == p->current_pdu_sent (which
	 * may of course be zero if this is the first return fragment.
	 */

	DEBUG(10,("read_from_pipe: %s: fault_state = %d : data_sent_length "
		  "= %u, p->out_data.rdata.length = %u.\n",
		  get_pipe_name_from_syntax(talloc_tos(), &p->syntax),
		  (int)p->fault_state,
		  (unsigned int)p->out_data.data_sent_length,
		  (unsigned int)p->out_data.rdata.length));

	if (p->out_data.data_sent_length >= p->out_data.rdata.length) {
		/*
		 * We have sent all possible data, return 0.
		 */
		data_returned = 0;
		goto out;
	}

	/*
	 * We need to create a new PDU from the data left in p->rdata.
	 * Create the header/data/footers. This also sets up the fields
	 * p->current_pdu_len, p->current_pdu_sent, p->data_sent_length
	 * and stores the outgoing PDU in p->current_pdu.
	 */

	if(!create_next_pdu(p)) {
		DEBUG(0,("read_from_pipe: %s: create_next_pdu failed.\n",
			 get_pipe_name_from_syntax(talloc_tos(), &p->syntax)));
		return -1;
	}

	data_returned = MIN(n, p->out_data.frag.length);

	memcpy(data, p->out_data.frag.data, (size_t)data_returned);
	p->out_data.current_pdu_sent += (uint32)data_returned;

  out:
	(*is_data_outstanding) = p->out_data.frag.length > n;

	if (p->out_data.current_pdu_sent == p->out_data.frag.length) {
		/* We've returned everything in the out_data.frag
		 * so we're done with this pdu. Free it and reset
		 * current_pdu_sent. */
		p->out_data.current_pdu_sent = 0;
		data_blob_free(&p->out_data.frag);

		if (p->out_data.data_sent_length >= p->out_data.rdata.length) {
			/*
			 * We're completely finished with both outgoing and
			 * incoming data streams. It's safe to free all
			 * temporary data from this request.
			 */
			free_pipe_context(p);
		}
	}

	return data_returned;
}

bool fsp_is_np(struct files_struct *fsp)
{
	enum FAKE_FILE_TYPE type;

	if ((fsp == NULL) || (fsp->fake_file_handle == NULL)) {
		return false;
	}

	type = fsp->fake_file_handle->type;

	return ((type == FAKE_FILE_TYPE_NAMED_PIPE)
		|| (type == FAKE_FILE_TYPE_NAMED_PIPE_PROXY));
}

NTSTATUS np_open(TALLOC_CTX *mem_ctx, const char *name,
		 const struct tsocket_address *local_address,
		 const struct tsocket_address *remote_address,
		 struct client_address *client_id,
		 struct auth_serversupplied_info *session_info,
		 struct messaging_context *msg_ctx,
		 struct fake_file_handle **phandle)
{
	const char *rpcsrv_type;
	const char **proxy_list;
	struct fake_file_handle *handle;
	bool external = false;

	proxy_list = lp_parm_string_list(-1, "np", "proxy", NULL);

	handle = talloc(mem_ctx, struct fake_file_handle);
	if (handle == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	/* Check what is the server type for this pipe.
	   Defaults to "embedded" */
	rpcsrv_type = lp_parm_const_string(GLOBAL_SECTION_SNUM,
					   "rpc_server", name,
					   "embedded");
	if (StrCaseCmp(rpcsrv_type, "embedded") != 0) {
		external = true;
	}

	/* Still support the old method for defining external servers */
	if ((proxy_list != NULL) && str_list_check_ci(proxy_list, name)) {
		external = true;
	}

	if (external) {
		struct np_proxy_state *p;

		p = make_external_rpc_pipe_p(handle, name,
					     local_address,
					     remote_address,
					     session_info);

		handle->type = FAKE_FILE_TYPE_NAMED_PIPE_PROXY;
		handle->private_data = p;
	} else {
		struct pipes_struct *p;
		struct ndr_syntax_id syntax;

		if (!is_known_pipename(name, &syntax)) {
			TALLOC_FREE(handle);
			return NT_STATUS_OBJECT_NAME_NOT_FOUND;
		}

		p = make_internal_rpc_pipe_p(handle, &syntax, client_id,
					     session_info, msg_ctx);

		handle->type = FAKE_FILE_TYPE_NAMED_PIPE;
		handle->private_data = p;
	}

	if (handle->private_data == NULL) {
		TALLOC_FREE(handle);
		return NT_STATUS_PIPE_NOT_AVAILABLE;
	}

	*phandle = handle;

	return NT_STATUS_OK;
}

bool np_read_in_progress(struct fake_file_handle *handle)
{
	if (handle->type == FAKE_FILE_TYPE_NAMED_PIPE) {
		return false;
	}

	if (handle->type == FAKE_FILE_TYPE_NAMED_PIPE_PROXY) {
		struct np_proxy_state *p = talloc_get_type_abort(
			handle->private_data, struct np_proxy_state);
		size_t read_count;

		read_count = tevent_queue_length(p->read_queue);
		if (read_count > 0) {
			return true;
		}

		return false;
	}

	return false;
}

struct np_write_state {
	struct event_context *ev;
	struct np_proxy_state *p;
	struct iovec iov;
	ssize_t nwritten;
};

static void np_write_done(struct tevent_req *subreq);

struct tevent_req *np_write_send(TALLOC_CTX *mem_ctx, struct event_context *ev,
				 struct fake_file_handle *handle,
				 const uint8_t *data, size_t len)
{
	struct tevent_req *req;
	struct np_write_state *state;
	NTSTATUS status;

	DEBUG(6, ("np_write_send: len: %d\n", (int)len));
	dump_data(50, data, len);

	req = tevent_req_create(mem_ctx, &state, struct np_write_state);
	if (req == NULL) {
		return NULL;
	}

	if (len == 0) {
		state->nwritten = 0;
		status = NT_STATUS_OK;
		goto post_status;
	}

	if (handle->type == FAKE_FILE_TYPE_NAMED_PIPE) {
		struct pipes_struct *p = talloc_get_type_abort(
			handle->private_data, struct pipes_struct);

		state->nwritten = write_to_internal_pipe(p, (char *)data, len);

		status = (state->nwritten >= 0)
			? NT_STATUS_OK : NT_STATUS_UNEXPECTED_IO_ERROR;
		goto post_status;
	}

	if (handle->type == FAKE_FILE_TYPE_NAMED_PIPE_PROXY) {
		struct np_proxy_state *p = talloc_get_type_abort(
			handle->private_data, struct np_proxy_state);
		struct tevent_req *subreq;

		state->ev = ev;
		state->p = p;
		state->iov.iov_base = CONST_DISCARD(void *, data);
		state->iov.iov_len = len;

		subreq = tstream_writev_queue_send(state, ev,
						   p->npipe,
						   p->write_queue,
						   &state->iov, 1);
		if (subreq == NULL) {
			goto fail;
		}
		tevent_req_set_callback(subreq, np_write_done, req);
		return req;
	}

	status = NT_STATUS_INVALID_HANDLE;
 post_status:
	if (NT_STATUS_IS_OK(status)) {
		tevent_req_done(req);
	} else {
		tevent_req_nterror(req, status);
	}
	return tevent_req_post(req, ev);
 fail:
	TALLOC_FREE(req);
	return NULL;
}

static void np_write_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct np_write_state *state = tevent_req_data(
		req, struct np_write_state);
	ssize_t received;
	int err;

	received = tstream_writev_queue_recv(subreq, &err);
	if (received < 0) {
		tevent_req_nterror(req, map_nt_error_from_unix(err));
		return;
	}
	state->nwritten = received;
	tevent_req_done(req);
}

NTSTATUS np_write_recv(struct tevent_req *req, ssize_t *pnwritten)
{
	struct np_write_state *state = tevent_req_data(
		req, struct np_write_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}
	*pnwritten = state->nwritten;
	return NT_STATUS_OK;
}

struct np_ipc_readv_next_vector_state {
	uint8_t *buf;
	size_t len;
	off_t ofs;
	size_t remaining;
};

static void np_ipc_readv_next_vector_init(struct np_ipc_readv_next_vector_state *s,
					  uint8_t *buf, size_t len)
{
	ZERO_STRUCTP(s);

	s->buf = buf;
	s->len = MIN(len, UINT16_MAX);
}

static int np_ipc_readv_next_vector(struct tstream_context *stream,
				    void *private_data,
				    TALLOC_CTX *mem_ctx,
				    struct iovec **_vector,
				    size_t *count)
{
	struct np_ipc_readv_next_vector_state *state =
		(struct np_ipc_readv_next_vector_state *)private_data;
	struct iovec *vector;
	ssize_t pending;
	size_t wanted;

	if (state->ofs == state->len) {
		*_vector = NULL;
		*count = 0;
		return 0;
	}

	pending = tstream_pending_bytes(stream);
	if (pending == -1) {
		return -1;
	}

	if (pending == 0 && state->ofs != 0) {
		/* return a short read */
		*_vector = NULL;
		*count = 0;
		return 0;
	}

	if (pending == 0) {
		/* we want at least one byte and recheck again */
		wanted = 1;
	} else {
		size_t missing = state->len - state->ofs;
		if (pending > missing) {
			/* there's more available */
			state->remaining = pending - missing;
			wanted = missing;
		} else {
			/* read what we can get and recheck in the next cycle */
			wanted = pending;
		}
	}

	vector = talloc_array(mem_ctx, struct iovec, 1);
	if (!vector) {
		return -1;
	}

	vector[0].iov_base = state->buf + state->ofs;
	vector[0].iov_len = wanted;

	state->ofs += wanted;

	*_vector = vector;
	*count = 1;
	return 0;
}

struct np_read_state {
	struct np_proxy_state *p;
	struct np_ipc_readv_next_vector_state next_vector;

	size_t nread;
	bool is_data_outstanding;
};

static void np_read_done(struct tevent_req *subreq);

struct tevent_req *np_read_send(TALLOC_CTX *mem_ctx, struct event_context *ev,
				struct fake_file_handle *handle,
				uint8_t *data, size_t len)
{
	struct tevent_req *req;
	struct np_read_state *state;
	NTSTATUS status;

	req = tevent_req_create(mem_ctx, &state, struct np_read_state);
	if (req == NULL) {
		return NULL;
	}

	if (handle->type == FAKE_FILE_TYPE_NAMED_PIPE) {
		struct pipes_struct *p = talloc_get_type_abort(
			handle->private_data, struct pipes_struct);

		state->nread = read_from_internal_pipe(
			p, (char *)data, len, &state->is_data_outstanding);

		status = (state->nread >= 0)
			? NT_STATUS_OK : NT_STATUS_UNEXPECTED_IO_ERROR;
		goto post_status;
	}

	if (handle->type == FAKE_FILE_TYPE_NAMED_PIPE_PROXY) {
		struct np_proxy_state *p = talloc_get_type_abort(
			handle->private_data, struct np_proxy_state);
		struct tevent_req *subreq;

		np_ipc_readv_next_vector_init(&state->next_vector,
					      data, len);

		subreq = tstream_readv_pdu_queue_send(state,
						      ev,
						      p->npipe,
						      p->read_queue,
						      np_ipc_readv_next_vector,
						      &state->next_vector);
		if (subreq == NULL) {
			status = NT_STATUS_NO_MEMORY;
			goto post_status;
		}
		tevent_req_set_callback(subreq, np_read_done, req);
		return req;
	}

	status = NT_STATUS_INVALID_HANDLE;
 post_status:
	if (NT_STATUS_IS_OK(status)) {
		tevent_req_done(req);
	} else {
		tevent_req_nterror(req, status);
	}
	return tevent_req_post(req, ev);
}

static void np_read_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct np_read_state *state = tevent_req_data(
		req, struct np_read_state);
	ssize_t ret;
	int err;

	ret = tstream_readv_pdu_queue_recv(subreq, &err);
	TALLOC_FREE(subreq);
	if (ret == -1) {
		tevent_req_nterror(req, map_nt_error_from_unix(err));
		return;
	}

	state->nread = ret;
	state->is_data_outstanding = (state->next_vector.remaining > 0);

	tevent_req_done(req);
	return;
}

NTSTATUS np_read_recv(struct tevent_req *req, ssize_t *nread,
		      bool *is_data_outstanding)
{
	struct np_read_state *state = tevent_req_data(
		req, struct np_read_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}

	DEBUG(10, ("Received %d bytes. There is %smore data outstanding\n",
		   (int)state->nread, state->is_data_outstanding?"":"no "));

	*nread = state->nread;
	*is_data_outstanding = state->is_data_outstanding;
	return NT_STATUS_OK;
}
