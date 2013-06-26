/* 
   Unix SMB/CIFS implementation.

   cldap client library

   Copyright (C) Andrew Tridgell 2005
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

/*
  see RFC1798 for details of CLDAP

  basic properties
    - carried over UDP on port 389
    - request and response matched by message ID
    - request consists of only a single searchRequest element
    - response can be in one of two forms
       - a single searchResponse, followed by a searchResult
       - a single searchResult
*/

#include "includes.h"
#include <tevent.h>
#include "../lib/util/dlinklist.h"
#include "../libcli/ldap/ldap_message.h"
#include "../libcli/ldap/ldap_ndr.h"
#include "../libcli/cldap/cldap.h"
#include "../lib/tsocket/tsocket.h"
#include "../libcli/security/dom_sid.h"
#include "../librpc/gen_ndr/ndr_nbt.h"
#include "../lib/util/asn1.h"
#include "../lib/util/tevent_ntstatus.h"

#undef strcasecmp

/*
  context structure for operations on cldap packets
*/
struct cldap_socket {
	/* the low level socket */
	struct tdgram_context *sock;

	/*
	 * Are we in connected mode, which means
	 * we get ICMP errors back instead of timing
	 * out requests. And we can only send requests
	 * to the connected peer.
	 */
	bool connected;

	/*
	 * we allow sync requests only, if the caller
	 * did not pass an event context to cldap_socket_init()
	 */
	struct {
		bool allow_poll;
		struct tevent_context *ctx;
	} event;

	/* the queue for outgoing dgrams */
	struct tevent_queue *send_queue;

	/* do we have an async tsocket_recvfrom request pending */
	struct tevent_req *recv_subreq;

	struct {
		/* a queue of pending search requests */
		struct cldap_search_state *list;

		/* mapping from message_id to pending request */
		struct idr_context *idr;
	} searches;

	/* what to do with incoming request packets */
	struct {
		void (*handler)(struct cldap_socket *,
				void *private_data,
				struct cldap_incoming *);
		void *private_data;
	} incoming;
};

struct cldap_search_state {
	struct cldap_search_state *prev, *next;

	struct {
		struct cldap_socket *cldap;
	} caller;

	int message_id;

	struct {
		uint32_t idx;
		uint32_t delay;
		uint32_t count;
		struct tsocket_address *dest;
		DATA_BLOB blob;
	} request;

	struct {
		struct cldap_incoming *in;
		struct asn1_data *asn1;
	} response;

	struct tevent_req *req;
};

static int cldap_socket_destructor(struct cldap_socket *c)
{
	while (c->searches.list) {
		struct cldap_search_state *s = c->searches.list;
		DLIST_REMOVE(c->searches.list, s);
		ZERO_STRUCT(s->caller);
	}

	talloc_free(c->recv_subreq);
	talloc_free(c->send_queue);
	talloc_free(c->sock);
	return 0;
}

static void cldap_recvfrom_done(struct tevent_req *subreq);

static bool cldap_recvfrom_setup(struct cldap_socket *c)
{
	if (c->recv_subreq) {
		return true;
	}

	if (!c->searches.list && !c->incoming.handler) {
		return true;
	}

	c->recv_subreq = tdgram_recvfrom_send(c, c->event.ctx, c->sock);
	if (!c->recv_subreq) {
		return false;
	}
	tevent_req_set_callback(c->recv_subreq, cldap_recvfrom_done, c);

	return true;
}

static void cldap_recvfrom_stop(struct cldap_socket *c)
{
	if (!c->recv_subreq) {
		return;
	}

	if (c->searches.list || c->incoming.handler) {
		return;
	}

	talloc_free(c->recv_subreq);
	c->recv_subreq = NULL;
}

static bool cldap_socket_recv_dgram(struct cldap_socket *c,
				    struct cldap_incoming *in);

static void cldap_recvfrom_done(struct tevent_req *subreq)
{
	struct cldap_socket *c = tevent_req_callback_data(subreq,
				 struct cldap_socket);
	struct cldap_incoming *in = NULL;
	ssize_t ret;
	bool setup_done;

	c->recv_subreq = NULL;

	in = talloc_zero(c, struct cldap_incoming);
	if (!in) {
		goto nomem;
	}

	ret = tdgram_recvfrom_recv(subreq,
				   &in->recv_errno,
				   in,
				   &in->buf,
				   &in->src);
	talloc_free(subreq);
	subreq = NULL;
	if (ret >= 0) {
		in->len = ret;
	}
	if (ret == -1 && in->recv_errno == 0) {
		in->recv_errno = EIO;
	}

	/* this function should free or steal 'in' */
	setup_done = cldap_socket_recv_dgram(c, in);
	in = NULL;

	if (!setup_done && !cldap_recvfrom_setup(c)) {
		goto nomem;
	}

	return;

nomem:
	talloc_free(subreq);
	talloc_free(in);
	/*TODO: call a dead socket handler */
	return;
}

/*
  handle recv events on a cldap socket
*/
static bool cldap_socket_recv_dgram(struct cldap_socket *c,
				    struct cldap_incoming *in)
{
	DATA_BLOB blob;
	struct asn1_data *asn1;
	void *p;
	struct cldap_search_state *search;
	NTSTATUS status;

	if (in->recv_errno != 0) {
		goto error;
	}

	blob = data_blob_const(in->buf, in->len);

	asn1 = asn1_init(in);
	if (!asn1) {
		goto nomem;
	}

	if (!asn1_load(asn1, blob)) {
		goto nomem;
	}

	in->ldap_msg = talloc(in, struct ldap_message);
	if (in->ldap_msg == NULL) {
		goto nomem;
	}

	/* this initial decode is used to find the message id */
	status = ldap_decode(asn1, NULL, in->ldap_msg);
	if (!NT_STATUS_IS_OK(status)) {
		goto nterror;
	}

	/* find the pending request */
	p = idr_find(c->searches.idr, in->ldap_msg->messageid);
	if (p == NULL) {
		if (!c->incoming.handler) {
			goto done;
		}

		/* this function should free or steal 'in' */
		c->incoming.handler(c, c->incoming.private_data, in);
		return false;
	}

	search = talloc_get_type(p, struct cldap_search_state);
	search->response.in = talloc_move(search, &in);
	search->response.asn1 = asn1;
	search->response.asn1->ofs = 0;

	DLIST_REMOVE(c->searches.list, search);

	cldap_recvfrom_setup(c);

	tevent_req_done(search->req);
	return true;

nomem:
	in->recv_errno = ENOMEM;
error:
	status = map_nt_error_from_unix(in->recv_errno);
nterror:
	TALLOC_FREE(in);
	/* in connected mode the first pending search gets the error */
	if (!c->connected) {
		/* otherwise we just ignore the error */
		goto done;
	}
	if (!c->searches.list) {
		goto done;
	}
	cldap_recvfrom_setup(c);
	tevent_req_nterror(c->searches.list->req, status);
	return true;
done:
	TALLOC_FREE(in);
	return false;
}

/*
  initialise a cldap_sock
*/
NTSTATUS cldap_socket_init(TALLOC_CTX *mem_ctx,
			   struct tevent_context *ev,
			   const struct tsocket_address *local_addr,
			   const struct tsocket_address *remote_addr,
			   struct cldap_socket **_cldap)
{
	struct cldap_socket *c = NULL;
	struct tsocket_address *any = NULL;
	NTSTATUS status;
	int ret;
	const char *fam = NULL;

	if (local_addr == NULL && remote_addr == NULL) {
		return NT_STATUS_INVALID_PARAMETER_MIX;
	}

	if (remote_addr) {
		bool is_ipv4;
		bool is_ipv6;

		is_ipv4 = tsocket_address_is_inet(remote_addr, "ipv4");
		is_ipv6 = tsocket_address_is_inet(remote_addr, "ipv6");

		if (is_ipv4) {
			fam = "ipv4";
		} else if (is_ipv6) {
			fam = "ipv6";
		} else {
			return NT_STATUS_INVALID_ADDRESS;
		}
	}

	c = talloc_zero(mem_ctx, struct cldap_socket);
	if (!c) {
		goto nomem;
	}

	if (!ev) {
		ev = tevent_context_init(c);
		if (!ev) {
			goto nomem;
		}
		c->event.allow_poll = true;
	}
	c->event.ctx = ev;

	if (!local_addr) {
		/*
		 * Here we know the address family of the remote address.
		 */
		if (fam == NULL) {
			return NT_STATUS_INVALID_PARAMETER_MIX;
		}

		ret = tsocket_address_inet_from_strings(c, fam,
							NULL, 0,
							&any);
		if (ret != 0) {
			status = map_nt_error_from_unix(errno);
			goto nterror;
		}
		local_addr = any;
	}

	c->searches.idr = idr_init(c);
	if (!c->searches.idr) {
		goto nomem;
	}

	ret = tdgram_inet_udp_socket(local_addr, remote_addr,
				     c, &c->sock);
	if (ret != 0) {
		status = map_nt_error_from_unix(errno);
		goto nterror;
	}
	talloc_free(any);

	if (remote_addr) {
		c->connected = true;
	}

	c->send_queue = tevent_queue_create(c, "cldap_send_queue");
	if (!c->send_queue) {
		goto nomem;
	}

	talloc_set_destructor(c, cldap_socket_destructor);

	*_cldap = c;
	return NT_STATUS_OK;

nomem:
	status = NT_STATUS_NO_MEMORY;
nterror:
	talloc_free(c);
	return status;
}

/*
  setup a handler for incoming requests
*/
NTSTATUS cldap_set_incoming_handler(struct cldap_socket *c,
				    void (*handler)(struct cldap_socket *,
						    void *private_data,
						    struct cldap_incoming *),
				    void *private_data)
{
	if (c->connected) {
		return NT_STATUS_PIPE_CONNECTED;
	}

	/* if sync requests are allowed, we don't allow an incoming handler */
	if (c->event.allow_poll) {
		return NT_STATUS_INVALID_PIPE_STATE;
	}

	c->incoming.handler = handler;
	c->incoming.private_data = private_data;

	if (!cldap_recvfrom_setup(c)) {
		ZERO_STRUCT(c->incoming);
		return NT_STATUS_NO_MEMORY;
	}

	return NT_STATUS_OK;
}

struct cldap_reply_state {
	struct tsocket_address *dest;
	DATA_BLOB blob;
};

static void cldap_reply_state_destroy(struct tevent_req *subreq);

/*
  queue a cldap reply for send
*/
NTSTATUS cldap_reply_send(struct cldap_socket *cldap, struct cldap_reply *io)
{
	struct cldap_reply_state *state = NULL;
	struct ldap_message *msg;
	DATA_BLOB blob1, blob2;
	NTSTATUS status;
	struct tevent_req *subreq;

	if (cldap->connected) {
		return NT_STATUS_PIPE_CONNECTED;
	}

	if (!io->dest) {
		return NT_STATUS_INVALID_ADDRESS;
	}

	state = talloc(cldap, struct cldap_reply_state);
	NT_STATUS_HAVE_NO_MEMORY(state);

	state->dest = tsocket_address_copy(io->dest, state);
	if (!state->dest) {
		goto nomem;
	}

	msg = talloc(state, struct ldap_message);
	if (!msg) {
		goto nomem;
	}

	msg->messageid       = io->messageid;
	msg->controls        = NULL;

	if (io->response) {
		msg->type = LDAP_TAG_SearchResultEntry;
		msg->r.SearchResultEntry = *io->response;

		if (!ldap_encode(msg, NULL, &blob1, state)) {
			status = NT_STATUS_INVALID_PARAMETER;
			goto failed;
		}
	} else {
		blob1 = data_blob(NULL, 0);
	}

	msg->type = LDAP_TAG_SearchResultDone;
	msg->r.SearchResultDone = *io->result;

	if (!ldap_encode(msg, NULL, &blob2, state)) {
		status = NT_STATUS_INVALID_PARAMETER;
		goto failed;
	}
	talloc_free(msg);

	state->blob = data_blob_talloc(state, NULL, blob1.length + blob2.length);
	if (!state->blob.data) {
		goto nomem;
	}

	memcpy(state->blob.data, blob1.data, blob1.length);
	memcpy(state->blob.data+blob1.length, blob2.data, blob2.length);
	data_blob_free(&blob1);
	data_blob_free(&blob2);

	subreq = tdgram_sendto_queue_send(state,
					  cldap->event.ctx,
					  cldap->sock,
					  cldap->send_queue,
					  state->blob.data,
					  state->blob.length,
					  state->dest);
	if (!subreq) {
		goto nomem;
	}
	/* the callback will just free the state, as we don't need a result */
	tevent_req_set_callback(subreq, cldap_reply_state_destroy, state);

	return NT_STATUS_OK;

nomem:
	status = NT_STATUS_NO_MEMORY;
failed:
	talloc_free(state);
	return status;
}

static void cldap_reply_state_destroy(struct tevent_req *subreq)
{
	struct cldap_reply_state *state = tevent_req_callback_data(subreq,
					  struct cldap_reply_state);

	/* we don't want to know the result here, we just free the state */
	talloc_free(subreq);
	talloc_free(state);
}

static int cldap_search_state_destructor(struct cldap_search_state *s)
{
	if (s->caller.cldap) {
		if (s->message_id != -1) {
			idr_remove(s->caller.cldap->searches.idr, s->message_id);
			s->message_id = -1;
		}
		DLIST_REMOVE(s->caller.cldap->searches.list, s);
		cldap_recvfrom_stop(s->caller.cldap);
		ZERO_STRUCT(s->caller);
	}

	return 0;
}

static void cldap_search_state_queue_done(struct tevent_req *subreq);
static void cldap_search_state_wakeup_done(struct tevent_req *subreq);

/*
  queue a cldap reply for send
*/
struct tevent_req *cldap_search_send(TALLOC_CTX *mem_ctx,
				    struct cldap_socket *cldap,
				    const struct cldap_search *io)
{
	struct tevent_req *req, *subreq;
	struct cldap_search_state *state = NULL;
	struct ldap_message *msg;
	struct ldap_SearchRequest *search;
	struct timeval now;
	struct timeval end;
	uint32_t i;
	int ret;

	req = tevent_req_create(mem_ctx, &state,
				struct cldap_search_state);
	if (!req) {
		return NULL;
	}
	ZERO_STRUCTP(state);
	state->req = req;
	state->caller.cldap = cldap;
	state->message_id = -1;

	talloc_set_destructor(state, cldap_search_state_destructor);

	if (io->in.dest_address) {
		if (cldap->connected) {
			tevent_req_nterror(req, NT_STATUS_PIPE_CONNECTED);
			goto post;
		}
		ret = tsocket_address_inet_from_strings(state,
						        "ip",
						        io->in.dest_address,
						        io->in.dest_port,
							&state->request.dest);
		if (ret != 0) {
			tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
			goto post;
		}
	} else {
		if (!cldap->connected) {
			tevent_req_nterror(req, NT_STATUS_INVALID_ADDRESS);
			goto post;
		}
		state->request.dest = NULL;
	}

	state->message_id = idr_get_new_random(cldap->searches.idr,
					       state, UINT16_MAX);
	if (state->message_id == -1) {
		tevent_req_nterror(req, NT_STATUS_INSUFFICIENT_RESOURCES);
		goto post;
	}

	msg = talloc(state, struct ldap_message);
	if (tevent_req_nomem(msg, req)) {
		goto post;
	}

	msg->messageid	= state->message_id;
	msg->type	= LDAP_TAG_SearchRequest;
	msg->controls	= NULL;
	search = &msg->r.SearchRequest;

	search->basedn		= "";
	search->scope		= LDAP_SEARCH_SCOPE_BASE;
	search->deref		= LDAP_DEREFERENCE_NEVER;
	search->timelimit	= 0;
	search->sizelimit	= 0;
	search->attributesonly	= false;
	search->num_attributes	= str_list_length(io->in.attributes);
	search->attributes	= io->in.attributes;
	search->tree		= ldb_parse_tree(msg, io->in.filter);
	if (tevent_req_nomem(search->tree, req)) {
		goto post;
	}

	if (!ldap_encode(msg, NULL, &state->request.blob, state)) {
		tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
		goto post;
	}
	talloc_free(msg);

	state->request.idx = 0;
	state->request.delay = 10*1000*1000;
	state->request.count = 3;
	if (io->in.timeout > 0) {
		state->request.delay = io->in.timeout * 1000 * 1000;
		state->request.count = io->in.retries + 1;
	}

	now = tevent_timeval_current();
	end = now;
	for (i = 0; i < state->request.count; i++) {
		end = tevent_timeval_add(&end, 0, state->request.delay);
	}

	if (!tevent_req_set_endtime(req, state->caller.cldap->event.ctx, end)) {
		tevent_req_nomem(NULL, req);
		goto post;
	}

	subreq = tdgram_sendto_queue_send(state,
					  state->caller.cldap->event.ctx,
					  state->caller.cldap->sock,
					  state->caller.cldap->send_queue,
					  state->request.blob.data,
					  state->request.blob.length,
					  state->request.dest);
	if (tevent_req_nomem(subreq, req)) {
		goto post;
	}
	tevent_req_set_callback(subreq, cldap_search_state_queue_done, req);

	DLIST_ADD_END(cldap->searches.list, state, struct cldap_search_state *);

	return req;

 post:
	return tevent_req_post(req, cldap->event.ctx);
}

static void cldap_search_state_queue_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(subreq,
				 struct tevent_req);
	struct cldap_search_state *state = tevent_req_data(req,
					   struct cldap_search_state);
	ssize_t ret;
	int sys_errno = 0;
	struct timeval next;

	ret = tdgram_sendto_queue_recv(subreq, &sys_errno);
	talloc_free(subreq);
	if (ret == -1) {
		NTSTATUS status;
		status = map_nt_error_from_unix(sys_errno);
		DLIST_REMOVE(state->caller.cldap->searches.list, state);
		ZERO_STRUCT(state->caller.cldap);
		tevent_req_nterror(req, status);
		return;
	}

	state->request.idx++;

	/* wait for incoming traffic */
	if (!cldap_recvfrom_setup(state->caller.cldap)) {
		tevent_req_nomem(NULL, req);
		return;
	}

	if (state->request.idx > state->request.count) {
		/* we just wait for the response or a timeout */
		return;
	}

	next = tevent_timeval_current_ofs(0, state->request.delay);
	subreq = tevent_wakeup_send(state,
				    state->caller.cldap->event.ctx,
				    next);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, cldap_search_state_wakeup_done, req);
}

static void cldap_search_state_wakeup_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(subreq,
				 struct tevent_req);
	struct cldap_search_state *state = tevent_req_data(req,
					   struct cldap_search_state);
	bool ok;

	ok = tevent_wakeup_recv(subreq);
	talloc_free(subreq);
	if (!ok) {
		tevent_req_nterror(req, NT_STATUS_INTERNAL_ERROR);
		return;
	}

	subreq = tdgram_sendto_queue_send(state,
					  state->caller.cldap->event.ctx,
					  state->caller.cldap->sock,
					  state->caller.cldap->send_queue,
					  state->request.blob.data,
					  state->request.blob.length,
					  state->request.dest);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, cldap_search_state_queue_done, req);
}

/*
  receive a cldap reply
*/
NTSTATUS cldap_search_recv(struct tevent_req *req,
			   TALLOC_CTX *mem_ctx,
			   struct cldap_search *io)
{
	struct cldap_search_state *state = tevent_req_data(req,
					   struct cldap_search_state);
	struct ldap_message *ldap_msg;
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		goto failed;
	}

	ldap_msg = talloc(mem_ctx, struct ldap_message);
	if (!ldap_msg) {
		goto nomem;
	}

	status = ldap_decode(state->response.asn1, NULL, ldap_msg);
	if (!NT_STATUS_IS_OK(status)) {
		goto failed;
	}

	ZERO_STRUCT(io->out);

	/* the first possible form has a search result in first place */
	if (ldap_msg->type == LDAP_TAG_SearchResultEntry) {
		io->out.response = talloc(mem_ctx, struct ldap_SearchResEntry);
		if (!io->out.response) {
			goto nomem;
		}
		*io->out.response = ldap_msg->r.SearchResultEntry;

		/* decode the 2nd part */
		status = ldap_decode(state->response.asn1, NULL, ldap_msg);
		if (!NT_STATUS_IS_OK(status)) {
			goto failed;
		}
	}

	if (ldap_msg->type != LDAP_TAG_SearchResultDone) {
		status = NT_STATUS_LDAP(LDAP_PROTOCOL_ERROR);
		goto failed;
	}

	io->out.result = talloc(mem_ctx, struct ldap_Result);
	if (!io->out.result) {
		goto nomem;
	}
	*io->out.result = ldap_msg->r.SearchResultDone;

	if (io->out.result->resultcode != LDAP_SUCCESS) {
		status = NT_STATUS_LDAP(io->out.result->resultcode);
		goto failed;
	}

	tevent_req_received(req);
	return NT_STATUS_OK;

nomem:
	status = NT_STATUS_NO_MEMORY;
failed:
	tevent_req_received(req);
	return status;
}


/*
  synchronous cldap search
*/
NTSTATUS cldap_search(struct cldap_socket *cldap,
		      TALLOC_CTX *mem_ctx,
		      struct cldap_search *io)
{
	struct tevent_req *req;
	NTSTATUS status;

	if (!cldap->event.allow_poll) {
		return NT_STATUS_INVALID_PIPE_STATE;
	}

	if (cldap->searches.list) {
		return NT_STATUS_PIPE_BUSY;
	}

	req = cldap_search_send(mem_ctx, cldap, io);
	NT_STATUS_HAVE_NO_MEMORY(req);

	if (!tevent_req_poll(req, cldap->event.ctx)) {
		talloc_free(req);
		return NT_STATUS_INTERNAL_ERROR;
	}

	status = cldap_search_recv(req, mem_ctx, io);
	talloc_free(req);

	return status;
}

struct cldap_netlogon_state {
	struct cldap_search search;
};

static void cldap_netlogon_state_done(struct tevent_req *subreq);
/*
  queue a cldap netlogon for send
*/
struct tevent_req *cldap_netlogon_send(TALLOC_CTX *mem_ctx,
				      struct cldap_socket *cldap,
				      const struct cldap_netlogon *io)
{
	struct tevent_req *req, *subreq;
	struct cldap_netlogon_state *state;
	char *filter;
	static const char * const attr[] = { "NetLogon", NULL };

	req = tevent_req_create(mem_ctx, &state,
				struct cldap_netlogon_state);
	if (!req) {
		return NULL;
	}

	filter = talloc_asprintf(state, "(&(NtVer=%s)", 
				 ldap_encode_ndr_uint32(state, io->in.version));
	if (tevent_req_nomem(filter, req)) {
		goto post;
	}
	if (io->in.user) {
		filter = talloc_asprintf_append_buffer(filter, "(User=%s)", io->in.user);
		if (tevent_req_nomem(filter, req)) {
			goto post;
		}
	}
	if (io->in.host) {
		filter = talloc_asprintf_append_buffer(filter, "(Host=%s)", io->in.host);
		if (tevent_req_nomem(filter, req)) {
			goto post;
		}
	}
	if (io->in.realm) {
		filter = talloc_asprintf_append_buffer(filter, "(DnsDomain=%s)", io->in.realm);
		if (tevent_req_nomem(filter, req)) {
			goto post;
		}
	}
	if (io->in.acct_control != -1) {
		filter = talloc_asprintf_append_buffer(filter, "(AAC=%s)", 
						ldap_encode_ndr_uint32(state, io->in.acct_control));
		if (tevent_req_nomem(filter, req)) {
			goto post;
		}
	}
	if (io->in.domain_sid) {
		struct dom_sid *sid = dom_sid_parse_talloc(state, io->in.domain_sid);
		if (tevent_req_nomem(sid, req)) {
			goto post;
		}
		filter = talloc_asprintf_append_buffer(filter, "(domainSid=%s)",
						ldap_encode_ndr_dom_sid(state, sid));
		if (tevent_req_nomem(filter, req)) {
			goto post;
		}
	}
	if (io->in.domain_guid) {
		struct GUID guid;
		NTSTATUS status;
		status = GUID_from_string(io->in.domain_guid, &guid);
		if (tevent_req_nterror(req, status)) {
			goto post;
		}
		filter = talloc_asprintf_append_buffer(filter, "(DomainGuid=%s)",
						ldap_encode_ndr_GUID(state, &guid));
		if (tevent_req_nomem(filter, req)) {
			goto post;
		}
	}
	filter = talloc_asprintf_append_buffer(filter, ")");
	if (tevent_req_nomem(filter, req)) {
		goto post;
	}

	if (io->in.dest_address) {
		state->search.in.dest_address = talloc_strdup(state,
						io->in.dest_address);
		if (tevent_req_nomem(state->search.in.dest_address, req)) {
			goto post;
		}
		state->search.in.dest_port = io->in.dest_port;
	} else {
		state->search.in.dest_address	= NULL;
		state->search.in.dest_port	= 0;
	}
	state->search.in.filter		= filter;
	state->search.in.attributes	= attr;
	state->search.in.timeout	= 2;
	state->search.in.retries	= 2;

	subreq = cldap_search_send(state, cldap, &state->search);
	if (tevent_req_nomem(subreq, req)) {
		goto post;
	}
	tevent_req_set_callback(subreq, cldap_netlogon_state_done, req);

	return req;
post:
	return tevent_req_post(req, cldap->event.ctx);
}

static void cldap_netlogon_state_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(subreq,
				 struct tevent_req);
	struct cldap_netlogon_state *state = tevent_req_data(req,
					     struct cldap_netlogon_state);
	NTSTATUS status;

	status = cldap_search_recv(subreq, state, &state->search);
	talloc_free(subreq);

	if (tevent_req_nterror(req, status)) {
		return;
	}

	tevent_req_done(req);
}

/*
  receive a cldap netlogon reply
*/
NTSTATUS cldap_netlogon_recv(struct tevent_req *req,
			     TALLOC_CTX *mem_ctx,
			     struct cldap_netlogon *io)
{
	struct cldap_netlogon_state *state = tevent_req_data(req,
					     struct cldap_netlogon_state);
	NTSTATUS status;
	DATA_BLOB *data;

	if (tevent_req_is_nterror(req, &status)) {
		goto failed;
	}

	if (state->search.out.response == NULL) {
		status = NT_STATUS_NOT_FOUND;
		goto failed;
	}

	if (state->search.out.response->num_attributes != 1 ||
	    strcasecmp(state->search.out.response->attributes[0].name, "netlogon") != 0 ||
	    state->search.out.response->attributes[0].num_values != 1 ||
	    state->search.out.response->attributes[0].values->length < 2) {
		status = NT_STATUS_UNEXPECTED_NETWORK_ERROR;
		goto failed;
	}
	data = state->search.out.response->attributes[0].values;

	status = pull_netlogon_samlogon_response(data, mem_ctx,
						 &io->out.netlogon);
	if (!NT_STATUS_IS_OK(status)) {
		goto failed;
	}

	if (io->in.map_response) {
		map_netlogon_samlogon_response(&io->out.netlogon);
	}

	status =  NT_STATUS_OK;
failed:
	tevent_req_received(req);
	return status;
}

/*
  sync cldap netlogon search
*/
NTSTATUS cldap_netlogon(struct cldap_socket *cldap,
			TALLOC_CTX *mem_ctx,
			struct cldap_netlogon *io)
{
	struct tevent_req *req;
	NTSTATUS status;

	if (!cldap->event.allow_poll) {
		return NT_STATUS_INVALID_PIPE_STATE;
	}

	if (cldap->searches.list) {
		return NT_STATUS_PIPE_BUSY;
	}

	req = cldap_netlogon_send(mem_ctx, cldap, io);
	NT_STATUS_HAVE_NO_MEMORY(req);

	if (!tevent_req_poll(req, cldap->event.ctx)) {
		talloc_free(req);
		return NT_STATUS_INTERNAL_ERROR;
	}

	status = cldap_netlogon_recv(req, mem_ctx, io);
	talloc_free(req);

	return status;
}


/*
  send an empty reply (used on any error, so the client doesn't keep waiting
  or send the bad request again)
*/
NTSTATUS cldap_empty_reply(struct cldap_socket *cldap,
			   uint32_t message_id,
			   struct tsocket_address *dest)
{
	NTSTATUS status;
	struct cldap_reply reply;
	struct ldap_Result result;

	reply.messageid    = message_id;
	reply.dest         = dest;
	reply.response     = NULL;
	reply.result       = &result;

	ZERO_STRUCT(result);

	status = cldap_reply_send(cldap, &reply);

	return status;
}

/*
  send an error reply (used on any error, so the client doesn't keep waiting
  or send the bad request again)
*/
NTSTATUS cldap_error_reply(struct cldap_socket *cldap,
			   uint32_t message_id,
			   struct tsocket_address *dest,
			   int resultcode,
			   const char *errormessage)
{
	NTSTATUS status;
	struct cldap_reply reply;
	struct ldap_Result result;

	reply.messageid    = message_id;
	reply.dest         = dest;
	reply.response     = NULL;
	reply.result       = &result;

	ZERO_STRUCT(result);
	result.resultcode	= resultcode;
	result.errormessage	= errormessage;

	status = cldap_reply_send(cldap, &reply);

	return status;
}


/*
  send a netlogon reply 
*/
NTSTATUS cldap_netlogon_reply(struct cldap_socket *cldap,
			      uint32_t message_id,
			      struct tsocket_address *dest,
			      uint32_t version,
			      struct netlogon_samlogon_response *netlogon)
{
	NTSTATUS status;
	struct cldap_reply reply;
	struct ldap_SearchResEntry response;
	struct ldap_Result result;
	TALLOC_CTX *tmp_ctx = talloc_new(cldap);
	DATA_BLOB blob;

	status = push_netlogon_samlogon_response(&blob, tmp_ctx,
						 netlogon);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(tmp_ctx);
		return status;
	}
	reply.messageid    = message_id;
	reply.dest         = dest;
	reply.response     = &response;
	reply.result       = &result;

	ZERO_STRUCT(result);

	response.dn = "";
	response.num_attributes = 1;
	response.attributes = talloc(tmp_ctx, struct ldb_message_element);
	NT_STATUS_HAVE_NO_MEMORY(response.attributes);
	response.attributes->name = "netlogon";
	response.attributes->num_values = 1;
	response.attributes->values = &blob;

	status = cldap_reply_send(cldap, &reply);

	talloc_free(tmp_ctx);

	return status;
}

