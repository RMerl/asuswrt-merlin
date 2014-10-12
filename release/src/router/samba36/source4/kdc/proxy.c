/*
   Unix SMB/CIFS implementation.

   KDC Server request proxying

   Copyright (C) Andrew Tridgell	2010
   Copyright (C) Andrew Bartlett        2010
   Copyright (C) Stefan Metzmacher      2011

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
#include "smbd/process_model.h"
#include "lib/tsocket/tsocket.h"
#include "libcli/util/tstream.h"
#include "lib/util/tevent_ntstatus.h"
#include "lib/stream/packet.h"
#include "kdc/kdc-glue.h"
#include "dsdb/samdb/samdb.h"
#include "libcli/composite/composite.h"
#include "libcli/resolve/resolve.h"


/*
  get a list of our replication partners from repsFrom, returning it in *proxy_list
 */
static WERROR kdc_proxy_get_writeable_dcs(struct kdc_server *kdc, TALLOC_CTX *mem_ctx, char ***proxy_list)
{
	WERROR werr;
	uint32_t count, i;
	struct repsFromToBlob *reps;

	werr = dsdb_loadreps(kdc->samdb, mem_ctx, ldb_get_default_basedn(kdc->samdb), "repsFrom", &reps, &count);
	W_ERROR_NOT_OK_RETURN(werr);

	if (count == 0) {
		/* we don't have any DCs to replicate with. Very
		   strange for a RODC */
		DEBUG(1,(__location__ ": No replication sources for RODC in KDC proxy\n"));
		talloc_free(reps);
		return WERR_DS_DRA_NO_REPLICA;
	}

	(*proxy_list) = talloc_array(mem_ctx, char *, count+1);
	W_ERROR_HAVE_NO_MEMORY_AND_FREE(*proxy_list, reps);

	talloc_steal(*proxy_list, reps);

	for (i=0; i<count; i++) {
		const char *dns_name = NULL;
		if (reps->version == 1) {
			dns_name = reps->ctr.ctr1.other_info->dns_name;
		} else if (reps->version == 2) {
			dns_name = reps->ctr.ctr2.other_info->dns_name1;
		}
		(*proxy_list)[i] = talloc_strdup(*proxy_list, dns_name);
		W_ERROR_HAVE_NO_MEMORY_AND_FREE((*proxy_list)[i], *proxy_list);
	}
	(*proxy_list)[i] = NULL;

	talloc_free(reps);

	return WERR_OK;
}


struct kdc_udp_proxy_state {
	struct tevent_context *ev;
	struct kdc_server *kdc;
	uint16_t port;
	DATA_BLOB in;
	DATA_BLOB out;
	char **proxy_list;
	uint32_t next_proxy;
	struct {
		struct nbt_name name;
		const char *ip;
		struct tdgram_context *dgram;
	} proxy;
};


static void kdc_udp_next_proxy(struct tevent_req *req);

struct tevent_req *kdc_udp_proxy_send(TALLOC_CTX *mem_ctx,
				      struct tevent_context *ev,
				      struct kdc_server *kdc,
				      uint16_t port,
				      DATA_BLOB in)
{
	struct tevent_req *req;
	struct kdc_udp_proxy_state *state;
	WERROR werr;

	req = tevent_req_create(mem_ctx, &state,
				struct kdc_udp_proxy_state);
	if (req == NULL) {
		return NULL;
	}
	state->ev = ev;
	state->kdc  = kdc;
	state->port = port;
	state->in = in;

	werr = kdc_proxy_get_writeable_dcs(kdc, state, &state->proxy_list);
	if (!W_ERROR_IS_OK(werr)) {
		NTSTATUS status = werror_to_ntstatus(werr);
		tevent_req_nterror(req, status);
		return tevent_req_post(req, ev);
	}

	kdc_udp_next_proxy(req);
	if (!tevent_req_is_in_progress(req)) {
		return tevent_req_post(req, ev);
	}

	return req;
}

static void kdc_udp_proxy_resolve_done(struct composite_context *csubreq);

/*
  try the next proxy in the list
 */
static void kdc_udp_next_proxy(struct tevent_req *req)
{
	struct kdc_udp_proxy_state *state =
		tevent_req_data(req,
		struct kdc_udp_proxy_state);
	const char *proxy_dnsname = state->proxy_list[state->next_proxy];
	struct composite_context *csubreq;

	if (proxy_dnsname == NULL) {
		tevent_req_nterror(req, NT_STATUS_NO_LOGON_SERVERS);
		return;
	}

	state->next_proxy++;

	/* make sure we close the socket of the last try */
	TALLOC_FREE(state->proxy.dgram);
	ZERO_STRUCT(state->proxy);

	make_nbt_name(&state->proxy.name, proxy_dnsname, 0);

	csubreq = resolve_name_ex_send(lpcfg_resolve_context(state->kdc->task->lp_ctx),
				       state,
				       RESOLVE_NAME_FLAG_FORCE_DNS,
				       0,
				       &state->proxy.name,
				       state->ev);
	if (tevent_req_nomem(csubreq, req)) {
		return;
	}
	csubreq->async.fn = kdc_udp_proxy_resolve_done;
	csubreq->async.private_data = req;
}

static void kdc_udp_proxy_sendto_done(struct tevent_req *subreq);
static void kdc_udp_proxy_recvfrom_done(struct tevent_req *subreq);

static void kdc_udp_proxy_resolve_done(struct composite_context *csubreq)
{
	struct tevent_req *req =
		talloc_get_type_abort(csubreq->async.private_data,
		struct tevent_req);
	struct kdc_udp_proxy_state *state =
		tevent_req_data(req,
		struct kdc_udp_proxy_state);
	NTSTATUS status;
	struct tevent_req *subreq;
	struct tsocket_address *local_addr, *proxy_addr;
	int ret;

	status = resolve_name_recv(csubreq, state, &state->proxy.ip);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("Unable to resolve proxy[%s] - %s\n",
			state->proxy.name.name, nt_errstr(status)));
		kdc_udp_next_proxy(req);
		return;
	}

	/* get an address for us to use locally */
	ret = tsocket_address_inet_from_strings(state, "ip", NULL, 0, &local_addr);
	if (ret != 0) {
		kdc_udp_next_proxy(req);
		return;
	}

	ret = tsocket_address_inet_from_strings(state, "ip",
						state->proxy.ip,
						state->port,
						&proxy_addr);
	if (ret != 0) {
		kdc_udp_next_proxy(req);
		return;
	}

	/* create a socket for us to work on */
	ret = tdgram_inet_udp_socket(local_addr, proxy_addr,
				     state, &state->proxy.dgram);
	if (ret != 0) {
		kdc_udp_next_proxy(req);
		return;
	}

	subreq = tdgram_sendto_send(state,
				    state->ev,
				    state->proxy.dgram,
				    state->in.data,
				    state->in.length,
				    NULL);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, kdc_udp_proxy_sendto_done, req);

	/* setup to receive the reply from the proxy */
	subreq = tdgram_recvfrom_send(state, state->ev, state->proxy.dgram);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, kdc_udp_proxy_recvfrom_done, req);
	tevent_req_set_endtime(subreq, state->ev,
			       timeval_current_ofs(state->kdc->proxy_timeout, 0));

	DEBUG(4,("kdc_udp_proxy: proxying request to %s[%s]\n",
		 state->proxy.name.name, state->proxy.ip));
}

/*
  called when the send of the call to the proxy is complete
  this is used to get an errors from the sendto()
 */
static void kdc_udp_proxy_sendto_done(struct tevent_req *subreq)
{
	struct tevent_req *req =
		tevent_req_callback_data(subreq,
		struct tevent_req);
	struct kdc_udp_proxy_state *state =
		tevent_req_data(req,
		struct kdc_udp_proxy_state);
	ssize_t ret;
	int sys_errno;

	ret = tdgram_sendto_recv(subreq, &sys_errno);
	TALLOC_FREE(subreq);
	if (ret == -1) {
		DEBUG(4,("kdc_udp_proxy: sendto for %s[%s] gave %d : %s\n",
			 state->proxy.name.name, state->proxy.ip,
			 sys_errno, strerror(sys_errno)));
		kdc_udp_next_proxy(req);
	}
}

/*
  called when the proxy replies
 */
static void kdc_udp_proxy_recvfrom_done(struct tevent_req *subreq)
{
	struct tevent_req *req =
		tevent_req_callback_data(subreq,
		struct tevent_req);
	struct kdc_udp_proxy_state *state =
		tevent_req_data(req,
		struct kdc_udp_proxy_state);
	int sys_errno;
	uint8_t *buf;
	ssize_t len;

	len = tdgram_recvfrom_recv(subreq, &sys_errno,
				   state, &buf, NULL);
	TALLOC_FREE(subreq);
	if (len == -1) {
		DEBUG(4,("kdc_udp_proxy: reply from %s[%s] gave %d : %s\n",
			 state->proxy.name.name, state->proxy.ip,
			 sys_errno, strerror(sys_errno)));
		kdc_udp_next_proxy(req);
		return;
	}

	/*
	 * Check the reply came from the right IP?
	 * As we use connected udp sockets, that should not be needed...
	 */

	state->out.length = len;
	state->out.data = buf;

	tevent_req_done(req);
}

NTSTATUS kdc_udp_proxy_recv(struct tevent_req *req,
			    TALLOC_CTX *mem_ctx,
			    DATA_BLOB *out)
{
	struct kdc_udp_proxy_state *state =
		tevent_req_data(req,
		struct kdc_udp_proxy_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		tevent_req_received(req);
		return status;
	}

	out->data = talloc_move(mem_ctx, &state->out.data);
	out->length = state->out.length;

	tevent_req_received(req);
	return NT_STATUS_OK;
}

struct kdc_tcp_proxy_state {
	struct tevent_context *ev;
	struct kdc_server *kdc;
	uint16_t port;
	DATA_BLOB in;
	uint8_t in_hdr[4];
	struct iovec in_iov[2];
	DATA_BLOB out;
	char **proxy_list;
	uint32_t next_proxy;
	struct {
		struct nbt_name name;
		const char *ip;
		struct tstream_context *stream;
	} proxy;
};

static void kdc_tcp_next_proxy(struct tevent_req *req);

struct tevent_req *kdc_tcp_proxy_send(TALLOC_CTX *mem_ctx,
				      struct tevent_context *ev,
				      struct kdc_server *kdc,
				      uint16_t port,
				      DATA_BLOB in)
{
	struct tevent_req *req;
	struct kdc_tcp_proxy_state *state;
	WERROR werr;

	req = tevent_req_create(mem_ctx, &state,
				struct kdc_tcp_proxy_state);
	if (req == NULL) {
		return NULL;
	}
	state->ev = ev;
	state->kdc  = kdc;
	state->port = port;
	state->in = in;

	werr = kdc_proxy_get_writeable_dcs(kdc, state, &state->proxy_list);
	if (!W_ERROR_IS_OK(werr)) {
		NTSTATUS status = werror_to_ntstatus(werr);
		tevent_req_nterror(req, status);
		return tevent_req_post(req, ev);
	}

	RSIVAL(state->in_hdr, 0, state->in.length);
	state->in_iov[0].iov_base = (char *)state->in_hdr;
	state->in_iov[0].iov_len = 4;
	state->in_iov[1].iov_base = (char *)state->in.data;
	state->in_iov[1].iov_len = state->in.length;

	kdc_tcp_next_proxy(req);
	if (!tevent_req_is_in_progress(req)) {
		return tevent_req_post(req, ev);
	}

	return req;
}

static void kdc_tcp_proxy_resolve_done(struct composite_context *csubreq);

/*
  try the next proxy in the list
 */
static void kdc_tcp_next_proxy(struct tevent_req *req)
{
	struct kdc_tcp_proxy_state *state =
		tevent_req_data(req,
		struct kdc_tcp_proxy_state);
	const char *proxy_dnsname = state->proxy_list[state->next_proxy];
	struct composite_context *csubreq;

	if (proxy_dnsname == NULL) {
		tevent_req_nterror(req, NT_STATUS_NO_LOGON_SERVERS);
		return;
	}

	state->next_proxy++;

	/* make sure we close the socket of the last try */
	TALLOC_FREE(state->proxy.stream);
	ZERO_STRUCT(state->proxy);

	make_nbt_name(&state->proxy.name, proxy_dnsname, 0);

	csubreq = resolve_name_ex_send(lpcfg_resolve_context(state->kdc->task->lp_ctx),
				       state,
				       RESOLVE_NAME_FLAG_FORCE_DNS,
				       0,
				       &state->proxy.name,
				       state->ev);
	if (tevent_req_nomem(csubreq, req)) {
		return;
	}
	csubreq->async.fn = kdc_tcp_proxy_resolve_done;
	csubreq->async.private_data = req;
}

static void kdc_tcp_proxy_connect_done(struct tevent_req *subreq);

static void kdc_tcp_proxy_resolve_done(struct composite_context *csubreq)
{
	struct tevent_req *req =
		talloc_get_type_abort(csubreq->async.private_data,
		struct tevent_req);
	struct kdc_tcp_proxy_state *state =
		tevent_req_data(req,
		struct kdc_tcp_proxy_state);
	NTSTATUS status;
	struct tevent_req *subreq;
	struct tsocket_address *local_addr, *proxy_addr;
	int ret;

	status = resolve_name_recv(csubreq, state, &state->proxy.ip);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("Unable to resolve proxy[%s] - %s\n",
			state->proxy.name.name, nt_errstr(status)));
		kdc_tcp_next_proxy(req);
		return;
	}

	/* get an address for us to use locally */
	ret = tsocket_address_inet_from_strings(state, "ip", NULL, 0, &local_addr);
	if (ret != 0) {
		kdc_tcp_next_proxy(req);
		return;
	}

	ret = tsocket_address_inet_from_strings(state, "ip",
						state->proxy.ip,
						state->port,
						&proxy_addr);
	if (ret != 0) {
		kdc_tcp_next_proxy(req);
		return;
	}

	subreq = tstream_inet_tcp_connect_send(state, state->ev,
					       local_addr, proxy_addr);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, kdc_tcp_proxy_connect_done, req);
	tevent_req_set_endtime(subreq, state->ev,
			       timeval_current_ofs(state->kdc->proxy_timeout, 0));
}

static void kdc_tcp_proxy_writev_done(struct tevent_req *subreq);
static void kdc_tcp_proxy_read_pdu_done(struct tevent_req *subreq);

static void kdc_tcp_proxy_connect_done(struct tevent_req *subreq)
{
	struct tevent_req *req =
		tevent_req_callback_data(subreq,
		struct tevent_req);
	struct kdc_tcp_proxy_state *state =
		tevent_req_data(req,
		struct kdc_tcp_proxy_state);
	int ret, sys_errno;

	ret = tstream_inet_tcp_connect_recv(subreq, &sys_errno,
					    state, &state->proxy.stream, NULL);
	TALLOC_FREE(subreq);
	if (ret != 0) {
		kdc_tcp_next_proxy(req);
		return;
	}

	subreq = tstream_writev_send(state,
				     state->ev,
				     state->proxy.stream,
				     state->in_iov, 2);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, kdc_tcp_proxy_writev_done, req);

	subreq = tstream_read_pdu_blob_send(state,
					    state->ev,
					    state->proxy.stream,
					    4, /* initial_read_size */
					    packet_full_request_u32,
					    req);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, kdc_tcp_proxy_read_pdu_done, req);
	tevent_req_set_endtime(subreq, state->kdc->task->event_ctx,
			       timeval_current_ofs(state->kdc->proxy_timeout, 0));

	DEBUG(4,("kdc_tcp_proxy: proxying request to %s[%s]\n",
		 state->proxy.name.name, state->proxy.ip));
}

static void kdc_tcp_proxy_writev_done(struct tevent_req *subreq)
{
	struct tevent_req *req =
		tevent_req_callback_data(subreq,
		struct tevent_req);
	int ret, sys_errno;

	ret = tstream_writev_recv(subreq, &sys_errno);
	TALLOC_FREE(subreq);
	if (ret == -1) {
		kdc_tcp_next_proxy(req);
	}
}

static void kdc_tcp_proxy_read_pdu_done(struct tevent_req *subreq)
{
	struct tevent_req *req =
		tevent_req_callback_data(subreq,
		struct tevent_req);
	struct kdc_tcp_proxy_state *state =
		tevent_req_data(req,
		struct kdc_tcp_proxy_state);
	NTSTATUS status;
	DATA_BLOB raw;

	status = tstream_read_pdu_blob_recv(subreq, state, &raw);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		kdc_tcp_next_proxy(req);
		return;
	}

	/*
	 * raw blob has the length in the first 4 bytes,
	 * which we do not need here.
	 */
	state->out = data_blob_talloc(state, raw.data + 4, raw.length - 4);
	if (state->out.length != raw.length - 4) {
		tevent_req_nomem(NULL, req);
		return;
	}

	tevent_req_done(req);
}

NTSTATUS kdc_tcp_proxy_recv(struct tevent_req *req,
			    TALLOC_CTX *mem_ctx,
			    DATA_BLOB *out)
{
	struct kdc_tcp_proxy_state *state =
		tevent_req_data(req,
		struct kdc_tcp_proxy_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		tevent_req_received(req);
		return status;
	}

	out->data = talloc_move(mem_ctx, &state->out.data);
	out->length = state->out.length;

	tevent_req_received(req);
	return NT_STATUS_OK;
}
