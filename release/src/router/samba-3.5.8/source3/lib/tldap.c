/*
   Unix SMB/CIFS implementation.
   Infrastructure for async ldap client requests
   Copyright (C) Volker Lendecke 2009

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

bool tevent_req_is_ldap_error(struct tevent_req *req, int *perr)
{
	enum tevent_req_state state;
	uint64_t err;

	if (!tevent_req_is_error(req, &state, &err)) {
		return false;
	}
	switch (state) {
	case TEVENT_REQ_TIMED_OUT:
		*perr = TLDAP_TIMEOUT;
		break;
	case TEVENT_REQ_NO_MEMORY:
		*perr = TLDAP_NO_MEMORY;
		break;
	case TEVENT_REQ_USER_ERROR:
		*perr = err;
		break;
	default:
		*perr = TLDAP_OPERATIONS_ERROR;
		break;
	}
	return true;
}

struct tldap_ctx_attribute {
	char *name;
	void *ptr;
};

struct tldap_context {
	int ld_version;
	int ld_deref;
	int ld_sizelimit;
	int ld_timelimit;
	struct tstream_context *conn;
	bool server_down;
	int msgid;
	struct tevent_queue *outgoing;
	struct tevent_req **pending;

	/* For the sync wrappers we need something like get_last_error... */
	struct tldap_message *last_msg;

	/* debug */
	void (*log_fn)(void *context, enum tldap_debug_level level,
		       const char *fmt, va_list ap);
	void *log_private;

	struct tldap_ctx_attribute *ctx_attrs;
};

struct tldap_message {
	struct asn1_data *data;
	uint8_t *inbuf;
	int type;
	int id;

	/* RESULT_ENTRY */
	char *dn;
	struct tldap_attribute *attribs;

	/* Error data sent by the server */
	int lderr;
	char *res_matcheddn;
	char *res_diagnosticmessage;
	char *res_referral;
	struct tldap_control *res_sctrls;

	/* Controls sent by the server */
	struct tldap_control *ctrls;
};

void tldap_set_debug(struct tldap_context *ld,
		     void (*log_fn)(void *log_private,
				    enum tldap_debug_level level,
				    const char *fmt,
				    va_list ap) PRINTF_ATTRIBUTE(3,0),
		     void *log_private)
{
	ld->log_fn = log_fn;
	ld->log_private = log_private;
}

static void tldap_debug(struct tldap_context *ld,
			 enum tldap_debug_level level,
			 const char *fmt, ...)
{
	va_list ap;
	if (!ld) {
		return;
	}
	if (ld->log_fn == NULL) {
		return;
	}
	va_start(ap, fmt);
	ld->log_fn(ld->log_private, level, fmt, ap);
	va_end(ap);
}

static int tldap_next_msgid(struct tldap_context *ld)
{
	int result;

	result = ld->msgid++;
	if (ld->msgid == 2147483647) {
		ld->msgid = 1;
	}
	return result;
}

struct tldap_context *tldap_context_create(TALLOC_CTX *mem_ctx, int fd)
{
	struct tldap_context *ctx;
	int ret;

	ctx = talloc_zero(mem_ctx, struct tldap_context);
	if (ctx == NULL) {
		return NULL;
	}
	ret = tstream_bsd_existing_socket(ctx, fd, &ctx->conn);
	if (ret == -1) {
		TALLOC_FREE(ctx);
		return NULL;
	}
	ctx->msgid = 1;
	ctx->ld_version = 3;
	ctx->outgoing = tevent_queue_create(ctx, "tldap_outgoing");
	if (ctx->outgoing == NULL) {
		TALLOC_FREE(ctx);
		return NULL;
	}
	return ctx;
}

bool tldap_connection_ok(struct tldap_context *ld)
{
	if (ld == NULL) {
		return false;
	}
	return !ld->server_down;
}

static struct tldap_ctx_attribute *tldap_context_findattr(
	struct tldap_context *ld, const char *name)
{
	int i, num_attrs;

	num_attrs = talloc_array_length(ld->ctx_attrs);

	for (i=0; i<num_attrs; i++) {
		if (strcmp(ld->ctx_attrs[i].name, name) == 0) {
			return &ld->ctx_attrs[i];
		}
	}
	return NULL;
}

bool tldap_context_setattr(struct tldap_context *ld,
			   const char *name, const void *_pptr)
{
	struct tldap_ctx_attribute *tmp, *attr;
	char *tmpname;
	int num_attrs;
	void **pptr = (void **)_pptr;

	attr = tldap_context_findattr(ld, name);
	if (attr != NULL) {
		/*
		 * We don't actually delete attrs, we don't expect tons of
		 * attributes being shuffled around.
		 */
		TALLOC_FREE(attr->ptr);
		if (*pptr != NULL) {
			attr->ptr = talloc_move(ld->ctx_attrs, pptr);
			*pptr = NULL;
		}
		return true;
	}

	tmpname = talloc_strdup(ld, name);
	if (tmpname == NULL) {
		return false;
	}

	num_attrs = talloc_array_length(ld->ctx_attrs);

	tmp = talloc_realloc(ld, ld->ctx_attrs, struct tldap_ctx_attribute,
			     num_attrs+1);
	if (tmp == NULL) {
		TALLOC_FREE(tmpname);
		return false;
	}
	tmp[num_attrs].name = talloc_move(tmp, &tmpname);
	if (*pptr != NULL) {
		tmp[num_attrs].ptr = talloc_move(tmp, pptr);
	} else {
		tmp[num_attrs].ptr = NULL;
	}
	*pptr = NULL;
	ld->ctx_attrs = tmp;
	return true;
}

void *tldap_context_getattr(struct tldap_context *ld, const char *name)
{
	struct tldap_ctx_attribute *attr = tldap_context_findattr(ld, name);

	if (attr == NULL) {
		return NULL;
	}
	return attr->ptr;
}

struct read_ldap_state {
	uint8_t *buf;
	bool done;
};

static ssize_t read_ldap_more(uint8_t *buf, size_t buflen, void *private_data);
static void read_ldap_done(struct tevent_req *subreq);

static struct tevent_req *read_ldap_send(TALLOC_CTX *mem_ctx,
					 struct tevent_context *ev,
					 struct tstream_context *conn)
{
	struct tevent_req *req, *subreq;
	struct read_ldap_state *state;

	req = tevent_req_create(mem_ctx, &state, struct read_ldap_state);
	if (req == NULL) {
		return NULL;
	}
	state->done = false;

	subreq = tstream_read_packet_send(state, ev, conn, 2, read_ldap_more,
					  state);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, read_ldap_done, req);
	return req;
}

static ssize_t read_ldap_more(uint8_t *buf, size_t buflen, void *private_data)
{
	struct read_ldap_state *state = talloc_get_type_abort(
		private_data, struct read_ldap_state);
	size_t len;
	int i, lensize;

	if (state->done) {
		/* We've been here, we're done */
		return 0;
	}

	/*
	 * From ldap.h: LDAP_TAG_MESSAGE is 0x30
	 */
	if (buf[0] != 0x30) {
		return -1;
	}

	len = buf[1];
	if ((len & 0x80) == 0) {
		state->done = true;
		return len;
	}

	lensize = (len & 0x7f);
	len = 0;

	if (buflen == 2) {
		/* Please get us the full length */
		return lensize;
	}
	if (buflen > 2 + lensize) {
		state->done = true;
		return 0;
	}
	if (buflen != 2 + lensize) {
		return -1;
	}

	for (i=0; i<lensize; i++) {
		len = (len << 8) | buf[2+i];
	}
	return len;
}

static void read_ldap_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct read_ldap_state *state = tevent_req_data(
		req, struct read_ldap_state);
	ssize_t nread;
	int err;

	nread = tstream_read_packet_recv(subreq, state, &state->buf, &err);
	TALLOC_FREE(subreq);
	if (nread == -1) {
		tevent_req_error(req, err);
		return;
	}
	tevent_req_done(req);
}

static ssize_t read_ldap_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
			      uint8_t **pbuf, int *perrno)
{
	struct read_ldap_state *state = tevent_req_data(
		req, struct read_ldap_state);

	if (tevent_req_is_unix_error(req, perrno)) {
		return -1;
	}
	*pbuf = talloc_move(mem_ctx, &state->buf);
	return talloc_get_size(*pbuf);
}

struct tldap_msg_state {
	struct tldap_context *ld;
	struct tevent_context *ev;
	int id;
	struct iovec iov;

	struct asn1_data *data;
	uint8_t *inbuf;
};

static void tldap_push_controls(struct asn1_data *data,
				struct tldap_control *sctrls,
				int num_sctrls)
{
	int i;

	if ((sctrls == NULL) || (num_sctrls == 0)) {
		return;
	}

	asn1_push_tag(data, ASN1_CONTEXT(0));

	for (i=0; i<num_sctrls; i++) {
		struct tldap_control *c = &sctrls[i];
		asn1_push_tag(data, ASN1_SEQUENCE(0));
		asn1_write_OctetString(data, c->oid, strlen(c->oid));
		if (c->critical) {
			asn1_write_BOOLEAN(data, true);
		}
		if (c->value.data != NULL) {
			asn1_write_OctetString(data, c->value.data,
					       c->value.length);
		}
		asn1_pop_tag(data); /* ASN1_SEQUENCE(0) */
	}

	asn1_pop_tag(data); /* ASN1_CONTEXT(0) */
}

static void tldap_msg_sent(struct tevent_req *subreq);
static void tldap_msg_received(struct tevent_req *subreq);

static struct tevent_req *tldap_msg_send(TALLOC_CTX *mem_ctx,
					 struct tevent_context *ev,
					 struct tldap_context *ld,
					 int id, struct asn1_data *data,
					 struct tldap_control *sctrls,
					 int num_sctrls)
{
	struct tevent_req *req, *subreq;
	struct tldap_msg_state *state;
	DATA_BLOB blob;

	tldap_debug(ld, TLDAP_DEBUG_TRACE, "tldap_msg_send: sending msg %d\n",
		    id);

	req = tevent_req_create(mem_ctx, &state, struct tldap_msg_state);
	if (req == NULL) {
		return NULL;
	}
	state->ld = ld;
	state->ev = ev;
	state->id = id;

	if (state->ld->server_down) {
		tevent_req_error(req, TLDAP_SERVER_DOWN);
		return tevent_req_post(req, ev);
	}

	tldap_push_controls(data, sctrls, num_sctrls);

	asn1_pop_tag(data);

	if (!asn1_blob(data, &blob)) {
		tevent_req_error(req, TLDAP_ENCODING_ERROR);
		return tevent_req_post(req, ev);
	}

	state->iov.iov_base = blob.data;
	state->iov.iov_len = blob.length;

	subreq = tstream_writev_queue_send(state, ev, ld->conn, ld->outgoing,
					   &state->iov, 1);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, tldap_msg_sent, req);
	return req;
}

static void tldap_msg_unset_pending(struct tevent_req *req)
{
	struct tldap_msg_state *state = tevent_req_data(
		req, struct tldap_msg_state);
	struct tldap_context *ld = state->ld;
	int num_pending = talloc_array_length(ld->pending);
	int i;

	if (num_pending == 1) {
		TALLOC_FREE(ld->pending);
		return;
	}

	for (i=0; i<num_pending; i++) {
		if (req == ld->pending[i]) {
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
		ld->pending[i] = ld->pending[num_pending-1];
	}

	/*
	 * No NULL check here, we're shrinking by sizeof(void *), and
	 * talloc_realloc just adjusts the size for this.
	 */
	ld->pending = talloc_realloc(NULL, ld->pending, struct tevent_req *,
				     num_pending - 1);
	return;
}

static int tldap_msg_destructor(struct tevent_req *req)
{
	tldap_msg_unset_pending(req);
	return 0;
}

static bool tldap_msg_set_pending(struct tevent_req *req)
{
	struct tldap_msg_state *state = tevent_req_data(
		req, struct tldap_msg_state);
	struct tldap_context *ld;
	struct tevent_req **pending;
	int num_pending;
	struct tevent_req *subreq;

	ld = state->ld;
	num_pending = talloc_array_length(ld->pending);

	pending = talloc_realloc(ld, ld->pending, struct tevent_req *,
				 num_pending+1);
	if (pending == NULL) {
		return false;
	}
	pending[num_pending] = req;
	ld->pending = pending;
	talloc_set_destructor(req, tldap_msg_destructor);

	if (num_pending > 0) {
		return true;
	}

	/*
	 * We're the first ones, add the read_ldap request that waits for the
	 * answer from the server
	 */
	subreq = read_ldap_send(ld->pending, state->ev, ld->conn);
	if (subreq == NULL) {
		tldap_msg_unset_pending(req);
		return false;
	}
	tevent_req_set_callback(subreq, tldap_msg_received, ld);
	return true;
}

static void tldap_msg_sent(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct tldap_msg_state *state = tevent_req_data(
		req, struct tldap_msg_state);
	ssize_t nwritten;
	int err;

	nwritten = tstream_writev_queue_recv(subreq, &err);
	TALLOC_FREE(subreq);
	if (nwritten == -1) {
		state->ld->server_down = true;
		tevent_req_error(req, TLDAP_SERVER_DOWN);
		return;
	}

	if (!tldap_msg_set_pending(req)) {
		tevent_req_nomem(NULL, req);
		return;
	}
}

static int tldap_msg_msgid(struct tevent_req *req)
{
	struct tldap_msg_state *state = tevent_req_data(
		req, struct tldap_msg_state);

	return state->id;
}

static void tldap_msg_received(struct tevent_req *subreq)
{
	struct tldap_context *ld = tevent_req_callback_data(
		subreq, struct tldap_context);
	struct tevent_req *req;
	struct tldap_msg_state *state;
	struct tevent_context *ev;
	struct asn1_data *data;
	uint8_t *inbuf;
	ssize_t received;
	size_t num_pending;
	int i, err, status;
	int id;
	uint8_t type;
	bool ok;

	received = read_ldap_recv(subreq, talloc_tos(), &inbuf, &err);
	TALLOC_FREE(subreq);
	if (received == -1) {
		status = TLDAP_SERVER_DOWN;
		goto fail;
	}

	data = asn1_init(talloc_tos());
	if (data == NULL) {
		status = TLDAP_NO_MEMORY;
		goto fail;
	}
	asn1_load_nocopy(data, inbuf, received);

	ok = true;
	ok &= asn1_start_tag(data, ASN1_SEQUENCE(0));
	ok &= asn1_read_Integer(data, &id);
	ok &= asn1_peek_uint8(data, &type);

	if (!ok) {
		status = TLDAP_PROTOCOL_ERROR;
		goto fail;
	}

	tldap_debug(ld, TLDAP_DEBUG_TRACE, "tldap_msg_received: got msg %d "
		    "type %d\n", id, (int)type);

	num_pending = talloc_array_length(ld->pending);

	for (i=0; i<num_pending; i++) {
		if (id == tldap_msg_msgid(ld->pending[i])) {
			break;
		}
	}
	if (i == num_pending) {
		/* Dump unexpected reply */
		tldap_debug(ld, TLDAP_DEBUG_WARNING, "tldap_msg_received: "
			    "No request pending for msg %d\n", id);
		TALLOC_FREE(data);
		TALLOC_FREE(inbuf);
		goto done;
	}

	req = ld->pending[i];
	state = tevent_req_data(req, struct tldap_msg_state);

	state->inbuf = talloc_move(state, &inbuf);
	state->data = talloc_move(state, &data);

	ev = state->ev;

	talloc_set_destructor(req, NULL);
	tldap_msg_unset_pending(req);
	num_pending = talloc_array_length(ld->pending);

	tevent_req_done(req);

 done:
	if (num_pending == 0) {
		return;
	}
	if (talloc_array_length(ld->pending) > num_pending) {
		/*
		 * The callback functions called from tevent_req_done() above
		 * have put something on the pending queue. We don't have to
		 * trigger the read_ldap_send(), tldap_msg_set_pending() has
		 * done it for us already.
		 */
		return;
	}

	state = tevent_req_data(ld->pending[0],	struct tldap_msg_state);
	subreq = read_ldap_send(ld->pending, state->ev, ld->conn);
	if (subreq == NULL) {
		status = TLDAP_NO_MEMORY;
		goto fail;
	}
	tevent_req_set_callback(subreq, tldap_msg_received, ld);
	return;

 fail:
	while (talloc_array_length(ld->pending) > 0) {
		req = ld->pending[0];
		talloc_set_destructor(req, NULL);
		tldap_msg_destructor(req);
		tevent_req_error(req, status);
	}
}

static int tldap_msg_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
			  struct tldap_message **pmsg)
{
	struct tldap_msg_state *state = tevent_req_data(
		req, struct tldap_msg_state);
	struct tldap_message *msg;
	int err;
	uint8_t msgtype;

	if (tevent_req_is_ldap_error(req, &err)) {
		return err;
	}

	if (!asn1_peek_uint8(state->data, &msgtype)) {
		return TLDAP_PROTOCOL_ERROR;
	}

	if (pmsg == NULL) {
		return TLDAP_SUCCESS;
	}

	msg = talloc_zero(mem_ctx, struct tldap_message);
	if (msg == NULL) {
		return TLDAP_NO_MEMORY;
	}
	msg->id = state->id;

	msg->inbuf = talloc_move(msg, &state->inbuf);
	msg->data = talloc_move(msg, &state->data);
	msg->type = msgtype;

	*pmsg = msg;
	return TLDAP_SUCCESS;
}

struct tldap_req_state {
	int id;
	struct asn1_data *out;
	struct tldap_message *result;
};

static struct tevent_req *tldap_req_create(TALLOC_CTX *mem_ctx,
					   struct tldap_context *ld,
					   struct tldap_req_state **pstate)
{
	struct tevent_req *req;
	struct tldap_req_state *state;

	req = tevent_req_create(mem_ctx, &state, struct tldap_req_state);
	if (req == NULL) {
		return NULL;
	}
	ZERO_STRUCTP(state);
	state->out = asn1_init(state);
	if (state->out == NULL) {
		TALLOC_FREE(req);
		return NULL;
	}
	state->result = NULL;
	state->id = tldap_next_msgid(ld);

	asn1_push_tag(state->out, ASN1_SEQUENCE(0));
	asn1_write_Integer(state->out, state->id);

	*pstate = state;
	return req;
}

static void tldap_save_msg(struct tldap_context *ld, struct tevent_req *req)
{
	struct tldap_req_state *state = tevent_req_data(
		req, struct tldap_req_state);

	TALLOC_FREE(ld->last_msg);
	ld->last_msg = talloc_move(ld, &state->result);
}

static char *blob2string_talloc(TALLOC_CTX *mem_ctx, DATA_BLOB blob)
{
	char *result = talloc_array(mem_ctx, char, blob.length+1);
	memcpy(result, blob.data, blob.length);
	result[blob.length] = '\0';
	return result;
}

static bool asn1_read_OctetString_talloc(TALLOC_CTX *mem_ctx,
					 struct asn1_data *data,
					 char **result)
{
	DATA_BLOB string;
	if (!asn1_read_OctetString(data, mem_ctx, &string))
		return false;
	*result = blob2string_talloc(mem_ctx, string);
	data_blob_free(&string);
	return true;
}

static bool tldap_decode_controls(struct tldap_req_state *state);

static bool tldap_decode_response(struct tldap_req_state *state)
{
	struct asn1_data *data = state->result->data;
	struct tldap_message *msg = state->result;
	bool ok = true;

	ok &= asn1_read_enumerated(data, &msg->lderr);
	ok &= asn1_read_OctetString_talloc(msg, data, &msg->res_matcheddn);
	ok &= asn1_read_OctetString_talloc(msg, data,
					   &msg->res_diagnosticmessage);
	if (asn1_peek_tag(data, ASN1_CONTEXT(3))) {
		ok &= asn1_start_tag(data, ASN1_CONTEXT(3));
		ok &= asn1_read_OctetString_talloc(msg, data,
						   &msg->res_referral);
		ok &= asn1_end_tag(data);
	} else {
		msg->res_referral = NULL;
	}

	return ok;
}

static void tldap_sasl_bind_done(struct tevent_req *subreq);

struct tevent_req *tldap_sasl_bind_send(TALLOC_CTX *mem_ctx,
					struct tevent_context *ev,
					struct tldap_context *ld,
					const char *dn,
					const char *mechanism,
					DATA_BLOB *creds,
					struct tldap_control *sctrls,
					int num_sctrls,
					struct tldap_control *cctrls,
					int num_cctrls)
{
	struct tevent_req *req, *subreq;
	struct tldap_req_state *state;

	req = tldap_req_create(mem_ctx, ld, &state);
	if (req == NULL) {
		return NULL;
	}

	if (dn == NULL) {
		dn = "";
	}

	asn1_push_tag(state->out, TLDAP_REQ_BIND);
	asn1_write_Integer(state->out, ld->ld_version);
	asn1_write_OctetString(state->out, dn, (dn != NULL) ? strlen(dn) : 0);

	if (mechanism == NULL) {
		asn1_push_tag(state->out, ASN1_CONTEXT_SIMPLE(0));
		asn1_write(state->out, creds->data, creds->length);
		asn1_pop_tag(state->out);
	} else {
		asn1_push_tag(state->out, ASN1_CONTEXT(3));
		asn1_write_OctetString(state->out, mechanism,
				       strlen(mechanism));
		if ((creds != NULL) && (creds->data != NULL)) {
			asn1_write_OctetString(state->out, creds->data,
					       creds->length);
		}
		asn1_pop_tag(state->out);
	}

	if (!asn1_pop_tag(state->out)) {
		tevent_req_error(req, TLDAP_ENCODING_ERROR);
		return tevent_req_post(req, ev);
	}

	subreq = tldap_msg_send(state, ev, ld, state->id, state->out,
				sctrls, num_sctrls);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, tldap_sasl_bind_done, req);
	return req;
}

static void tldap_sasl_bind_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct tldap_req_state *state = tevent_req_data(
		req, struct tldap_req_state);
	int err;

	err = tldap_msg_recv(subreq, state, &state->result);
	TALLOC_FREE(subreq);
	if (err != TLDAP_SUCCESS) {
		tevent_req_error(req, err);
		return;
	}
	if (state->result->type != TLDAP_RES_BIND) {
		tevent_req_error(req, TLDAP_PROTOCOL_ERROR);
		return;
	}
	if (!asn1_start_tag(state->result->data, state->result->type) ||
	    !tldap_decode_response(state) ||
	    !asn1_end_tag(state->result->data)) {
		tevent_req_error(req, TLDAP_DECODING_ERROR);
		return;
	}
	/*
	 * TODO: pull the reply blob
	 */
	if (state->result->lderr != TLDAP_SUCCESS) {
		tevent_req_error(req, state->result->lderr);
		return;
	}
	tevent_req_done(req);
}

int tldap_sasl_bind_recv(struct tevent_req *req)
{
	int err;

	if (tevent_req_is_ldap_error(req, &err)) {
		return err;
	}
	return TLDAP_SUCCESS;
}

int tldap_sasl_bind(struct tldap_context *ld,
		    const char *dn,
		    const char *mechanism,
		    DATA_BLOB *creds,
		    struct tldap_control *sctrls,
		    int num_sctrls,
		    struct tldap_control *cctrls,
		    int num_cctrls)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct tevent_context *ev;
	struct tevent_req *req;
	int result;

	ev = event_context_init(frame);
	if (ev == NULL) {
		result = TLDAP_NO_MEMORY;
		goto fail;
	}

	req = tldap_sasl_bind_send(frame, ev, ld, dn, mechanism, creds,
				   sctrls, num_sctrls, cctrls, num_cctrls);
	if (req == NULL) {
		result = TLDAP_NO_MEMORY;
		goto fail;
	}

	if (!tevent_req_poll(req, ev)) {
		result = TLDAP_OPERATIONS_ERROR;
		goto fail;
	}

	result = tldap_sasl_bind_recv(req);
	tldap_save_msg(ld, req);
 fail:
	TALLOC_FREE(frame);
	return result;
}

struct tevent_req *tldap_simple_bind_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct tldap_context *ld,
					  const char *dn,
					  const char *passwd)
{
	DATA_BLOB cred;

	if (passwd != NULL) {
		cred.data = (uint8_t *)passwd;
		cred.length = strlen(passwd);
	} else {
		cred.data = (uint8_t *)"";
		cred.length = 0;
	}
	return tldap_sasl_bind_send(mem_ctx, ev, ld, dn, NULL, &cred, NULL, 0,
				    NULL, 0);
}

int tldap_simple_bind_recv(struct tevent_req *req)
{
	return tldap_sasl_bind_recv(req);
}

int tldap_simple_bind(struct tldap_context *ld, const char *dn,
		      const char *passwd)
{
	DATA_BLOB cred;

	if (passwd != NULL) {
		cred.data = (uint8_t *)passwd;
		cred.length = strlen(passwd);
	} else {
		cred.data = (uint8_t *)"";
		cred.length = 0;
	}
	return tldap_sasl_bind(ld, dn, NULL, &cred, NULL, 0, NULL, 0);
}

/*****************************************************************************/

/*
 * This piece has a dependency on ldb, the ldb_parse_tree() function is used.
 * In case we want to separate out tldap, we need to copy or rewrite it.
 */

#include "lib/ldb/include/ldb.h"
#include "lib/ldb/include/ldb_errors.h"

static bool ldap_push_filter(struct asn1_data *data,
			     struct ldb_parse_tree *tree)
{
	int i;

	switch (tree->operation) {
	case LDB_OP_AND:
	case LDB_OP_OR:
		asn1_push_tag(data,
			      ASN1_CONTEXT(tree->operation==LDB_OP_AND?0:1));
		for (i=0; i<tree->u.list.num_elements; i++) {
			if (!ldap_push_filter(data,
					      tree->u.list.elements[i])) {
				return false;
			}
		}
		asn1_pop_tag(data);
		break;

	case LDB_OP_NOT:
		asn1_push_tag(data, ASN1_CONTEXT(2));
		if (!ldap_push_filter(data, tree->u.isnot.child)) {
			return false;
		}
		asn1_pop_tag(data);
		break;

	case LDB_OP_EQUALITY:
		/* equality test */
		asn1_push_tag(data, ASN1_CONTEXT(3));
		asn1_write_OctetString(data, tree->u.equality.attr,
				      strlen(tree->u.equality.attr));
		asn1_write_OctetString(data, tree->u.equality.value.data,
				      tree->u.equality.value.length);
		asn1_pop_tag(data);
		break;

	case LDB_OP_SUBSTRING:
		/*
		  SubstringFilter ::= SEQUENCE {
			  type            AttributeDescription,
			  -- at least one must be present
			  substrings      SEQUENCE OF CHOICE {
				  initial [0] LDAPString,
				  any     [1] LDAPString,
				  final   [2] LDAPString } }
		*/
		asn1_push_tag(data, ASN1_CONTEXT(4));
		asn1_write_OctetString(data, tree->u.substring.attr,
				       strlen(tree->u.substring.attr));
		asn1_push_tag(data, ASN1_SEQUENCE(0));
		i = 0;
		if (!tree->u.substring.start_with_wildcard) {
			asn1_push_tag(data, ASN1_CONTEXT_SIMPLE(0));
			asn1_write_DATA_BLOB_LDAPString(
				data, tree->u.substring.chunks[i]);
			asn1_pop_tag(data);
			i++;
		}
		while (tree->u.substring.chunks[i]) {
			int ctx;

			if ((!tree->u.substring.chunks[i + 1]) &&
			    (tree->u.substring.end_with_wildcard == 0)) {
				ctx = 2;
			} else {
				ctx = 1;
			}
			asn1_push_tag(data, ASN1_CONTEXT_SIMPLE(ctx));
			asn1_write_DATA_BLOB_LDAPString(
				data, tree->u.substring.chunks[i]);
			asn1_pop_tag(data);
			i++;
		}
		asn1_pop_tag(data);
		asn1_pop_tag(data);
		break;

	case LDB_OP_GREATER:
		/* greaterOrEqual test */
		asn1_push_tag(data, ASN1_CONTEXT(5));
		asn1_write_OctetString(data, tree->u.comparison.attr,
				      strlen(tree->u.comparison.attr));
		asn1_write_OctetString(data, tree->u.comparison.value.data,
				      tree->u.comparison.value.length);
		asn1_pop_tag(data);
		break;

	case LDB_OP_LESS:
		/* lessOrEqual test */
		asn1_push_tag(data, ASN1_CONTEXT(6));
		asn1_write_OctetString(data, tree->u.comparison.attr,
				      strlen(tree->u.comparison.attr));
		asn1_write_OctetString(data, tree->u.comparison.value.data,
				      tree->u.comparison.value.length);
		asn1_pop_tag(data);
		break;

	case LDB_OP_PRESENT:
		/* present test */
		asn1_push_tag(data, ASN1_CONTEXT_SIMPLE(7));
		asn1_write_LDAPString(data, tree->u.present.attr);
		asn1_pop_tag(data);
		return !data->has_error;

	case LDB_OP_APPROX:
		/* approx test */
		asn1_push_tag(data, ASN1_CONTEXT(8));
		asn1_write_OctetString(data, tree->u.comparison.attr,
				      strlen(tree->u.comparison.attr));
		asn1_write_OctetString(data, tree->u.comparison.value.data,
				      tree->u.comparison.value.length);
		asn1_pop_tag(data);
		break;

	case LDB_OP_EXTENDED:
		/*
		  MatchingRuleAssertion ::= SEQUENCE {
		  matchingRule    [1] MatchingRuleID OPTIONAL,
		  type            [2] AttributeDescription OPTIONAL,
		  matchValue      [3] AssertionValue,
		  dnAttributes    [4] BOOLEAN DEFAULT FALSE
		  }
		*/
		asn1_push_tag(data, ASN1_CONTEXT(9));
		if (tree->u.extended.rule_id) {
			asn1_push_tag(data, ASN1_CONTEXT_SIMPLE(1));
			asn1_write_LDAPString(data, tree->u.extended.rule_id);
			asn1_pop_tag(data);
		}
		if (tree->u.extended.attr) {
			asn1_push_tag(data, ASN1_CONTEXT_SIMPLE(2));
			asn1_write_LDAPString(data, tree->u.extended.attr);
			asn1_pop_tag(data);
		}
		asn1_push_tag(data, ASN1_CONTEXT_SIMPLE(3));
		asn1_write_DATA_BLOB_LDAPString(data, &tree->u.extended.value);
		asn1_pop_tag(data);
		asn1_push_tag(data, ASN1_CONTEXT_SIMPLE(4));
		asn1_write_uint8(data, tree->u.extended.dnAttributes);
		asn1_pop_tag(data);
		asn1_pop_tag(data);
		break;

	default:
		return false;
	}
	return !data->has_error;
}

static bool tldap_push_filter(struct asn1_data *data, const char *filter)
{
	struct ldb_parse_tree *tree;
	bool ret;

	tree = ldb_parse_tree(talloc_tos(), filter);
	if (tree == NULL) {
		return false;
	}
	ret = ldap_push_filter(data, tree);
	TALLOC_FREE(tree);
	return ret;
}

/*****************************************************************************/

static void tldap_search_done(struct tevent_req *subreq);

struct tevent_req *tldap_search_send(TALLOC_CTX *mem_ctx,
				     struct tevent_context *ev,
				     struct tldap_context *ld,
				     const char *base, int scope,
				     const char *filter,
				     const char **attrs,
				     int num_attrs,
				     int attrsonly,
				     struct tldap_control *sctrls,
				     int num_sctrls,
				     struct tldap_control *cctrls,
				     int num_cctrls,
				     int timelimit,
				     int sizelimit,
				     int deref)
{
	struct tevent_req *req, *subreq;
	struct tldap_req_state *state;
	int i;

	req = tldap_req_create(mem_ctx, ld, &state);
	if (req == NULL) {
		return NULL;
	}

	asn1_push_tag(state->out, TLDAP_REQ_SEARCH);
	asn1_write_OctetString(state->out, base, strlen(base));
	asn1_write_enumerated(state->out, scope);
	asn1_write_enumerated(state->out, deref);
	asn1_write_Integer(state->out, sizelimit);
	asn1_write_Integer(state->out, timelimit);
	asn1_write_BOOLEAN(state->out, attrsonly);

	if (!tldap_push_filter(state->out, filter)) {
		goto encoding_error;
	}

	asn1_push_tag(state->out, ASN1_SEQUENCE(0));
	for (i=0; i<num_attrs; i++) {
		asn1_write_OctetString(state->out, attrs[i], strlen(attrs[i]));
	}
	asn1_pop_tag(state->out);
	asn1_pop_tag(state->out);

	subreq = tldap_msg_send(state, ev, ld, state->id, state->out,
				sctrls, num_sctrls);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, tldap_search_done, req);
	return req;

 encoding_error:
	tevent_req_error(req, TLDAP_ENCODING_ERROR);
	return tevent_req_post(req, ev);
}

static void tldap_search_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct tldap_req_state *state = tevent_req_data(
		req, struct tldap_req_state);
	int err;

	err = tldap_msg_recv(subreq, state, &state->result);
	if (err != TLDAP_SUCCESS) {
		tevent_req_error(req, err);
		return;
	}
	switch (state->result->type) {
	case TLDAP_RES_SEARCH_ENTRY:
	case TLDAP_RES_SEARCH_REFERENCE:
		tevent_req_notify_callback(req);
		if (!tldap_msg_set_pending(subreq)) {
			tevent_req_nomem(NULL, req);
			return;
		}
		break;
	case TLDAP_RES_SEARCH_RESULT:
		TALLOC_FREE(subreq);
		if (!asn1_start_tag(state->result->data,
				    state->result->type) ||
		    !tldap_decode_response(state) ||
		    !asn1_end_tag(state->result->data) ||
		    !tldap_decode_controls(state)) {
			tevent_req_error(req, TLDAP_DECODING_ERROR);
			return;
		}
		tevent_req_done(req);
		break;
	default:
		tevent_req_error(req, TLDAP_PROTOCOL_ERROR);
		return;
	}
}

int tldap_search_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
		      struct tldap_message **pmsg)
{
	struct tldap_req_state *state = tevent_req_data(
		req, struct tldap_req_state);
	int err;

	if (!tevent_req_is_in_progress(req)
	    && tevent_req_is_ldap_error(req, &err)) {
		return err;
	}

	if (tevent_req_is_in_progress(req)) {
		switch (state->result->type) {
		case TLDAP_RES_SEARCH_ENTRY:
		case TLDAP_RES_SEARCH_REFERENCE:
			break;
		default:
			return TLDAP_OPERATIONS_ERROR;
		}
	}

	*pmsg = talloc_move(mem_ctx, &state->result);
	return TLDAP_SUCCESS;
}

struct tldap_sync_search_state {
	TALLOC_CTX *mem_ctx;
	struct tldap_message **entries;
	struct tldap_message **refs;
	int rc;
};

static void tldap_search_cb(struct tevent_req *req)
{
	struct tldap_sync_search_state *state =
		(struct tldap_sync_search_state *)
		tevent_req_callback_data_void(req);
	struct tldap_message *msg, **tmp;
	int rc, num_entries, num_refs;

	rc = tldap_search_recv(req, talloc_tos(), &msg);
	if (rc != TLDAP_SUCCESS) {
		state->rc = rc;
		return;
	}

	switch (tldap_msg_type(msg)) {
	case TLDAP_RES_SEARCH_ENTRY:
		num_entries = talloc_array_length(state->entries);
		tmp = talloc_realloc(state->mem_ctx, state->entries,
				     struct tldap_message *, num_entries + 1);
		if (tmp == NULL) {
			state->rc = TLDAP_NO_MEMORY;
			return;
		}
		state->entries = tmp;
		state->entries[num_entries] = talloc_move(state->entries,
							  &msg);
		break;
	case TLDAP_RES_SEARCH_REFERENCE:
		num_refs = talloc_array_length(state->refs);
		tmp = talloc_realloc(state->mem_ctx, state->refs,
				     struct tldap_message *, num_refs + 1);
		if (tmp == NULL) {
			state->rc = TLDAP_NO_MEMORY;
			return;
		}
		state->refs = tmp;
		state->refs[num_refs] = talloc_move(state->refs, &msg);
		break;
	case TLDAP_RES_SEARCH_RESULT:
		state->rc = TLDAP_SUCCESS;
		break;
	default:
		state->rc = TLDAP_PROTOCOL_ERROR;
		break;
	}
}

int tldap_search(struct tldap_context *ld,
		 const char *base, int scope, const char *filter,
		 const char **attrs, int num_attrs, int attrsonly,
		 struct tldap_control *sctrls, int num_sctrls,
		 struct tldap_control *cctrls, int num_cctrls,
		 int timelimit, int sizelimit, int deref,
		 TALLOC_CTX *mem_ctx, struct tldap_message ***entries,
		 struct tldap_message ***refs)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct tevent_context *ev;
	struct tevent_req *req;
	struct tldap_sync_search_state state;

	ZERO_STRUCT(state);
	state.mem_ctx = mem_ctx;
	state.rc = TLDAP_SUCCESS;

	ev = event_context_init(frame);
	if (ev == NULL) {
		state.rc = TLDAP_NO_MEMORY;
		goto fail;
	}

	req = tldap_search_send(frame, ev, ld, base, scope, filter,
				attrs, num_attrs, attrsonly,
				sctrls, num_sctrls, cctrls, num_cctrls,
				timelimit, sizelimit, deref);
	if (req == NULL) {
		state.rc = TLDAP_NO_MEMORY;
		goto fail;
	}

	tevent_req_set_callback(req, tldap_search_cb, &state);

	while (tevent_req_is_in_progress(req)
	       && (state.rc == TLDAP_SUCCESS)) {
		if (tevent_loop_once(ev) == -1) {
			return TLDAP_OPERATIONS_ERROR;
		}
	}

	if (state.rc != TLDAP_SUCCESS) {
		return state.rc;
	}

	if (entries != NULL) {
		*entries = state.entries;
	} else {
		TALLOC_FREE(state.entries);
	}
	if (refs != NULL) {
		*refs = state.refs;
	} else {
		TALLOC_FREE(state.refs);
	}
	tldap_save_msg(ld, req);
fail:
	TALLOC_FREE(frame);
	return state.rc;
}

static bool tldap_parse_search_entry(struct tldap_message *msg)
{
	int num_attribs = 0;

	asn1_start_tag(msg->data, msg->type);

	/* dn */

	asn1_read_OctetString_talloc(msg, msg->data, &msg->dn);
	if (msg->dn == NULL) {
		return false;
	}

	/*
	 * Attributes: We overallocate msg->attribs by one, so that while
	 * looping over the attributes we can directly parse into the last
	 * array element. Same for the values in the inner loop.
	 */

	msg->attribs = talloc_array(msg, struct tldap_attribute, 1);
	if (msg->attribs == NULL) {
		return false;
	}

	asn1_start_tag(msg->data, ASN1_SEQUENCE(0));
	while (asn1_peek_tag(msg->data, ASN1_SEQUENCE(0))) {
		struct tldap_attribute *attrib;
		int num_values = 0;

		attrib = &msg->attribs[num_attribs];
		attrib->values = talloc_array(msg->attribs, DATA_BLOB, 1);
		if (attrib->values == NULL) {
			return false;
		}
		asn1_start_tag(msg->data, ASN1_SEQUENCE(0));
		asn1_read_OctetString_talloc(msg->attribs, msg->data,
					     &attrib->name);
		asn1_start_tag(msg->data, ASN1_SET);

		while (asn1_peek_tag(msg->data, ASN1_OCTET_STRING)) {
			asn1_read_OctetString(msg->data, msg,
					      &attrib->values[num_values]);

			attrib->values = talloc_realloc(
				msg->attribs, attrib->values, DATA_BLOB,
				num_values + 2);
			if (attrib->values == NULL) {
				return false;
			}
			num_values += 1;
		}
		attrib->values = talloc_realloc(msg->attribs, attrib->values,
						DATA_BLOB, num_values);
		attrib->num_values = num_values;

		asn1_end_tag(msg->data); /* ASN1_SET */
		asn1_end_tag(msg->data); /* ASN1_SEQUENCE(0) */
		msg->attribs = talloc_realloc(
			msg, msg->attribs, struct tldap_attribute,
			num_attribs + 2);
		if (msg->attribs == NULL) {
			return false;
		}
		num_attribs += 1;
	}
	msg->attribs = talloc_realloc(
		msg, msg->attribs, struct tldap_attribute, num_attribs);
	asn1_end_tag(msg->data);
	if (msg->data->has_error) {
		return false;
	}
	return true;
}

bool tldap_entry_dn(struct tldap_message *msg, char **dn)
{
	if ((msg->dn == NULL) && (!tldap_parse_search_entry(msg))) {
		return false;
	}
	*dn = msg->dn;
	return true;
}

bool tldap_entry_attributes(struct tldap_message *msg, int *num_attributes,
			    struct tldap_attribute **attributes)
{
	if ((msg->dn == NULL) && (!tldap_parse_search_entry(msg))) {
		return false;
	}
	*attributes = msg->attribs;
	*num_attributes = talloc_array_length(msg->attribs);
	return true;
}

static bool tldap_decode_controls(struct tldap_req_state *state)
{
	struct tldap_message *msg = state->result;
	struct asn1_data *data = msg->data;
	struct tldap_control *sctrls = NULL;
	int num_controls = 0;

	msg->res_sctrls = NULL;

	if (!asn1_peek_tag(data, ASN1_CONTEXT(0))) {
		return true;
	}

	asn1_start_tag(data, ASN1_CONTEXT(0));

	while (asn1_peek_tag(data, ASN1_SEQUENCE(0))) {
		struct tldap_control *c;
		char *oid = NULL;

		sctrls = talloc_realloc(msg, sctrls, struct tldap_control,
					num_controls + 1);
		if (sctrls == NULL) {
			return false;
		}
		c = &sctrls[num_controls];

		asn1_start_tag(data, ASN1_SEQUENCE(0));
		asn1_read_OctetString_talloc(msg, data, &oid);
		if ((data->has_error) || (oid == NULL)) {
			return false;
		}
		c->oid = oid;
		if (asn1_peek_tag(data, ASN1_BOOLEAN)) {
			asn1_read_BOOLEAN(data, &c->critical);
		} else {
			c->critical = false;
		}
		c->value = data_blob_null;
		if (asn1_peek_tag(data, ASN1_OCTET_STRING) &&
		    !asn1_read_OctetString(data, msg, &c->value)) {
			return false;
		}
		asn1_end_tag(data); /* ASN1_SEQUENCE(0) */

		num_controls += 1;
	}

	asn1_end_tag(data); 	/* ASN1_CONTEXT(0) */

	if (data->has_error) {
		TALLOC_FREE(sctrls);
		return false;
	}
	msg->res_sctrls = sctrls;
	return true;
}

static void tldap_simple_done(struct tevent_req *subreq, int type)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct tldap_req_state *state = tevent_req_data(
		req, struct tldap_req_state);
	int err;

	err = tldap_msg_recv(subreq, state, &state->result);
	TALLOC_FREE(subreq);
	if (err != TLDAP_SUCCESS) {
		tevent_req_error(req, err);
		return;
	}
	if (state->result->type != type) {
		tevent_req_error(req, TLDAP_PROTOCOL_ERROR);
		return;
	}
	if (!asn1_start_tag(state->result->data, state->result->type) ||
	    !tldap_decode_response(state) ||
	    !asn1_end_tag(state->result->data) ||
	    !tldap_decode_controls(state)) {
		tevent_req_error(req, TLDAP_DECODING_ERROR);
		return;
	}
	if (state->result->lderr != TLDAP_SUCCESS) {
		tevent_req_error(req, state->result->lderr);
		return;
	}
	tevent_req_done(req);
}

static int tldap_simple_recv(struct tevent_req *req)
{
	int err;
	if (tevent_req_is_ldap_error(req, &err)) {
		return err;
	}
	return TLDAP_SUCCESS;
}

static void tldap_add_done(struct tevent_req *subreq);

struct tevent_req *tldap_add_send(TALLOC_CTX *mem_ctx,
				  struct tevent_context *ev,
				  struct tldap_context *ld,
				  const char *dn,
				  struct tldap_mod *attributes,
				  int num_attributes,
				  struct tldap_control *sctrls,
				  int num_sctrls,
				  struct tldap_control *cctrls,
				  int num_cctrls)
{
	struct tevent_req *req, *subreq;
	struct tldap_req_state *state;
	int i, j;

	req = tldap_req_create(mem_ctx, ld, &state);
	if (req == NULL) {
		return NULL;
	}

	asn1_push_tag(state->out, TLDAP_REQ_ADD);
	asn1_write_OctetString(state->out, dn, strlen(dn));
	asn1_push_tag(state->out, ASN1_SEQUENCE(0));

	for (i=0; i<num_attributes; i++) {
		struct tldap_mod *attrib = &attributes[i];
		asn1_push_tag(state->out, ASN1_SEQUENCE(0));
		asn1_write_OctetString(state->out, attrib->attribute,
				       strlen(attrib->attribute));
		asn1_push_tag(state->out, ASN1_SET);
		for (j=0; j<attrib->num_values; j++) {
			asn1_write_OctetString(state->out,
					       attrib->values[j].data,
					       attrib->values[j].length);
		}
		asn1_pop_tag(state->out);
		asn1_pop_tag(state->out);
	}

	asn1_pop_tag(state->out);
	asn1_pop_tag(state->out);

	subreq = tldap_msg_send(state, ev, ld, state->id, state->out,
				sctrls, num_sctrls);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, tldap_add_done, req);
	return req;
}

static void tldap_add_done(struct tevent_req *subreq)
{
	tldap_simple_done(subreq, TLDAP_RES_ADD);
}

int tldap_add_recv(struct tevent_req *req)
{
	return tldap_simple_recv(req);
}

int tldap_add(struct tldap_context *ld, const char *dn,
	      int num_attributes, struct tldap_mod *attributes,
	      struct tldap_control *sctrls, int num_sctrls,
	      struct tldap_control *cctrls, int num_cctrls)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct tevent_context *ev;
	struct tevent_req *req;
	int result;

	ev = event_context_init(frame);
	if (ev == NULL) {
		result = TLDAP_NO_MEMORY;
		goto fail;
	}

	req = tldap_add_send(frame, ev, ld, dn, attributes, num_attributes,
			     sctrls, num_sctrls, cctrls, num_cctrls);
	if (req == NULL) {
		result = TLDAP_NO_MEMORY;
		goto fail;
	}

	if (!tevent_req_poll(req, ev)) {
		result = TLDAP_OPERATIONS_ERROR;
		goto fail;
	}

	result = tldap_add_recv(req);
	tldap_save_msg(ld, req);
 fail:
	TALLOC_FREE(frame);
	return result;
}

static void tldap_modify_done(struct tevent_req *subreq);

struct tevent_req *tldap_modify_send(TALLOC_CTX *mem_ctx,
				     struct tevent_context *ev,
				     struct tldap_context *ld,
				     const char *dn,
				     int num_mods, struct tldap_mod *mods,
				     struct tldap_control *sctrls,
				     int num_sctrls,
				     struct tldap_control *cctrls,
				     int num_cctrls)
{
	struct tevent_req *req, *subreq;
	struct tldap_req_state *state;
	int i, j;

	req = tldap_req_create(mem_ctx, ld, &state);
	if (req == NULL) {
		return NULL;
	}

	asn1_push_tag(state->out, TLDAP_REQ_MODIFY);
	asn1_write_OctetString(state->out, dn, strlen(dn));
	asn1_push_tag(state->out, ASN1_SEQUENCE(0));

	for (i=0; i<num_mods; i++) {
		struct tldap_mod *mod = &mods[i];
		asn1_push_tag(state->out, ASN1_SEQUENCE(0));
		asn1_write_enumerated(state->out, mod->mod_op),
		asn1_push_tag(state->out, ASN1_SEQUENCE(0));
		asn1_write_OctetString(state->out, mod->attribute,
				       strlen(mod->attribute));
		asn1_push_tag(state->out, ASN1_SET);
		for (j=0; j<mod->num_values; j++) {
			asn1_write_OctetString(state->out,
					       mod->values[j].data,
					       mod->values[j].length);
		}
		asn1_pop_tag(state->out);
		asn1_pop_tag(state->out);
		asn1_pop_tag(state->out);
	}

	asn1_pop_tag(state->out);
	asn1_pop_tag(state->out);

	subreq = tldap_msg_send(state, ev, ld, state->id, state->out,
				sctrls, num_sctrls);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, tldap_modify_done, req);
	return req;
}

static void tldap_modify_done(struct tevent_req *subreq)
{
	tldap_simple_done(subreq, TLDAP_RES_MODIFY);
}

int tldap_modify_recv(struct tevent_req *req)
{
	return tldap_simple_recv(req);
}

int tldap_modify(struct tldap_context *ld, const char *dn,
		 int num_mods, struct tldap_mod *mods,
		 struct tldap_control *sctrls, int num_sctrls,
		 struct tldap_control *cctrls, int num_cctrls)
 {
	TALLOC_CTX *frame = talloc_stackframe();
	struct tevent_context *ev;
	struct tevent_req *req;
	int result;

	ev = event_context_init(frame);
	if (ev == NULL) {
		result = TLDAP_NO_MEMORY;
		goto fail;
	}

	req = tldap_modify_send(frame, ev, ld, dn, num_mods, mods,
				sctrls, num_sctrls, cctrls, num_cctrls);
	if (req == NULL) {
		result = TLDAP_NO_MEMORY;
		goto fail;
	}

	if (!tevent_req_poll(req, ev)) {
		result = TLDAP_OPERATIONS_ERROR;
		goto fail;
	}

	result = tldap_modify_recv(req);
	tldap_save_msg(ld, req);
 fail:
	TALLOC_FREE(frame);
	return result;
}

static void tldap_delete_done(struct tevent_req *subreq);

struct tevent_req *tldap_delete_send(TALLOC_CTX *mem_ctx,
				     struct tevent_context *ev,
				     struct tldap_context *ld,
				     const char *dn,
				     struct tldap_control *sctrls,
				     int num_sctrls,
				     struct tldap_control *cctrls,
				     int num_cctrls)
{
	struct tevent_req *req, *subreq;
	struct tldap_req_state *state;

	req = tldap_req_create(mem_ctx, ld, &state);
	if (req == NULL) {
		return NULL;
	}

	asn1_push_tag(state->out, TLDAP_REQ_DELETE);
	asn1_write(state->out, dn, strlen(dn));
	asn1_pop_tag(state->out);

	subreq = tldap_msg_send(state, ev, ld, state->id, state->out,
				sctrls, num_sctrls);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, tldap_delete_done, req);
	return req;
}

static void tldap_delete_done(struct tevent_req *subreq)
{
	tldap_simple_done(subreq, TLDAP_RES_DELETE);
}

int tldap_delete_recv(struct tevent_req *req)
{
	return tldap_simple_recv(req);
}

int tldap_delete(struct tldap_context *ld, const char *dn,
		 struct tldap_control *sctrls, int num_sctrls,
		 struct tldap_control *cctrls, int num_cctrls)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct tevent_context *ev;
	struct tevent_req *req;
	int result;

	ev = event_context_init(frame);
	if (ev == NULL) {
		result = TLDAP_NO_MEMORY;
		goto fail;
	}

	req = tldap_delete_send(frame, ev, ld, dn, sctrls, num_sctrls,
				cctrls, num_cctrls);
	if (req == NULL) {
		result = TLDAP_NO_MEMORY;
		goto fail;
	}

	if (!tevent_req_poll(req, ev)) {
		result = TLDAP_OPERATIONS_ERROR;
		goto fail;
	}

	result = tldap_delete_recv(req);
	tldap_save_msg(ld, req);
 fail:
	TALLOC_FREE(frame);
	return result;
}

int tldap_msg_id(const struct tldap_message *msg)
{
	return msg->id;
}

int tldap_msg_type(const struct tldap_message *msg)
{
	return msg->type;
}

const char *tldap_msg_matcheddn(struct tldap_message *msg)
{
	if (msg == NULL) {
		return NULL;
	}
	return msg->res_matcheddn;
}

const char *tldap_msg_diagnosticmessage(struct tldap_message *msg)
{
	if (msg == NULL) {
		return NULL;
	}
	return msg->res_diagnosticmessage;
}

const char *tldap_msg_referral(struct tldap_message *msg)
{
	if (msg == NULL) {
		return NULL;
	}
	return msg->res_referral;
}

void tldap_msg_sctrls(struct tldap_message *msg, int *num_sctrls,
		      struct tldap_control **sctrls)
{
	if (msg == NULL) {
		*sctrls = NULL;
		*num_sctrls = 0;
	}
	*sctrls = msg->res_sctrls;
	*num_sctrls = talloc_array_length(msg->res_sctrls);
}

struct tldap_message *tldap_ctx_lastmsg(struct tldap_context *ld)
{
	return ld->last_msg;
}

const char *tldap_err2string(int rc)
{
	const char *res = NULL;

	/*
	 * This would normally be a table, but the error codes are not fully
	 * sequential. Let the compiler figure out the optimum implementation
	 * :-)
	 */

	switch (rc) {
	case TLDAP_SUCCESS:
		res = "TLDAP_SUCCESS";
		break;
	case TLDAP_OPERATIONS_ERROR:
		res = "TLDAP_OPERATIONS_ERROR";
		break;
	case TLDAP_PROTOCOL_ERROR:
		res = "TLDAP_PROTOCOL_ERROR";
		break;
	case TLDAP_TIMELIMIT_EXCEEDED:
		res = "TLDAP_TIMELIMIT_EXCEEDED";
		break;
	case TLDAP_SIZELIMIT_EXCEEDED:
		res = "TLDAP_SIZELIMIT_EXCEEDED";
		break;
	case TLDAP_COMPARE_FALSE:
		res = "TLDAP_COMPARE_FALSE";
		break;
	case TLDAP_COMPARE_TRUE:
		res = "TLDAP_COMPARE_TRUE";
		break;
	case TLDAP_STRONG_AUTH_NOT_SUPPORTED:
		res = "TLDAP_STRONG_AUTH_NOT_SUPPORTED";
		break;
	case TLDAP_STRONG_AUTH_REQUIRED:
		res = "TLDAP_STRONG_AUTH_REQUIRED";
		break;
	case TLDAP_REFERRAL:
		res = "TLDAP_REFERRAL";
		break;
	case TLDAP_ADMINLIMIT_EXCEEDED:
		res = "TLDAP_ADMINLIMIT_EXCEEDED";
		break;
	case TLDAP_UNAVAILABLE_CRITICAL_EXTENSION:
		res = "TLDAP_UNAVAILABLE_CRITICAL_EXTENSION";
		break;
	case TLDAP_CONFIDENTIALITY_REQUIRED:
		res = "TLDAP_CONFIDENTIALITY_REQUIRED";
		break;
	case TLDAP_SASL_BIND_IN_PROGRESS:
		res = "TLDAP_SASL_BIND_IN_PROGRESS";
		break;
	case TLDAP_NO_SUCH_ATTRIBUTE:
		res = "TLDAP_NO_SUCH_ATTRIBUTE";
		break;
	case TLDAP_UNDEFINED_TYPE:
		res = "TLDAP_UNDEFINED_TYPE";
		break;
	case TLDAP_INAPPROPRIATE_MATCHING:
		res = "TLDAP_INAPPROPRIATE_MATCHING";
		break;
	case TLDAP_CONSTRAINT_VIOLATION:
		res = "TLDAP_CONSTRAINT_VIOLATION";
		break;
	case TLDAP_TYPE_OR_VALUE_EXISTS:
		res = "TLDAP_TYPE_OR_VALUE_EXISTS";
		break;
	case TLDAP_INVALID_SYNTAX:
		res = "TLDAP_INVALID_SYNTAX";
		break;
	case TLDAP_NO_SUCH_OBJECT:
		res = "TLDAP_NO_SUCH_OBJECT";
		break;
	case TLDAP_ALIAS_PROBLEM:
		res = "TLDAP_ALIAS_PROBLEM";
		break;
	case TLDAP_INVALID_DN_SYNTAX:
		res = "TLDAP_INVALID_DN_SYNTAX";
		break;
	case TLDAP_IS_LEAF:
		res = "TLDAP_IS_LEAF";
		break;
	case TLDAP_ALIAS_DEREF_PROBLEM:
		res = "TLDAP_ALIAS_DEREF_PROBLEM";
		break;
	case TLDAP_INAPPROPRIATE_AUTH:
		res = "TLDAP_INAPPROPRIATE_AUTH";
		break;
	case TLDAP_INVALID_CREDENTIALS:
		res = "TLDAP_INVALID_CREDENTIALS";
		break;
	case TLDAP_INSUFFICIENT_ACCESS:
		res = "TLDAP_INSUFFICIENT_ACCESS";
		break;
	case TLDAP_BUSY:
		res = "TLDAP_BUSY";
		break;
	case TLDAP_UNAVAILABLE:
		res = "TLDAP_UNAVAILABLE";
		break;
	case TLDAP_UNWILLING_TO_PERFORM:
		res = "TLDAP_UNWILLING_TO_PERFORM";
		break;
	case TLDAP_LOOP_DETECT:
		res = "TLDAP_LOOP_DETECT";
		break;
	case TLDAP_NAMING_VIOLATION:
		res = "TLDAP_NAMING_VIOLATION";
		break;
	case TLDAP_OBJECT_CLASS_VIOLATION:
		res = "TLDAP_OBJECT_CLASS_VIOLATION";
		break;
	case TLDAP_NOT_ALLOWED_ON_NONLEAF:
		res = "TLDAP_NOT_ALLOWED_ON_NONLEAF";
		break;
	case TLDAP_NOT_ALLOWED_ON_RDN:
		res = "TLDAP_NOT_ALLOWED_ON_RDN";
		break;
	case TLDAP_ALREADY_EXISTS:
		res = "TLDAP_ALREADY_EXISTS";
		break;
	case TLDAP_NO_OBJECT_CLASS_MODS:
		res = "TLDAP_NO_OBJECT_CLASS_MODS";
		break;
	case TLDAP_RESULTS_TOO_LARGE:
		res = "TLDAP_RESULTS_TOO_LARGE";
		break;
	case TLDAP_AFFECTS_MULTIPLE_DSAS:
		res = "TLDAP_AFFECTS_MULTIPLE_DSAS";
		break;
	case TLDAP_OTHER:
		res = "TLDAP_OTHER";
		break;
	case TLDAP_SERVER_DOWN:
		res = "TLDAP_SERVER_DOWN";
		break;
	case TLDAP_LOCAL_ERROR:
		res = "TLDAP_LOCAL_ERROR";
		break;
	case TLDAP_ENCODING_ERROR:
		res = "TLDAP_ENCODING_ERROR";
		break;
	case TLDAP_DECODING_ERROR:
		res = "TLDAP_DECODING_ERROR";
		break;
	case TLDAP_TIMEOUT:
		res = "TLDAP_TIMEOUT";
		break;
	case TLDAP_AUTH_UNKNOWN:
		res = "TLDAP_AUTH_UNKNOWN";
		break;
	case TLDAP_FILTER_ERROR:
		res = "TLDAP_FILTER_ERROR";
		break;
	case TLDAP_USER_CANCELLED:
		res = "TLDAP_USER_CANCELLED";
		break;
	case TLDAP_PARAM_ERROR:
		res = "TLDAP_PARAM_ERROR";
		break;
	case TLDAP_NO_MEMORY:
		res = "TLDAP_NO_MEMORY";
		break;
	case TLDAP_CONNECT_ERROR:
		res = "TLDAP_CONNECT_ERROR";
		break;
	case TLDAP_NOT_SUPPORTED:
		res = "TLDAP_NOT_SUPPORTED";
		break;
	case TLDAP_CONTROL_NOT_FOUND:
		res = "TLDAP_CONTROL_NOT_FOUND";
		break;
	case TLDAP_NO_RESULTS_RETURNED:
		res = "TLDAP_NO_RESULTS_RETURNED";
		break;
	case TLDAP_MORE_RESULTS_TO_RETURN:
		res = "TLDAP_MORE_RESULTS_TO_RETURN";
		break;
	case TLDAP_CLIENT_LOOP:
		res = "TLDAP_CLIENT_LOOP";
		break;
	case TLDAP_REFERRAL_LIMIT_EXCEEDED:
		res = "TLDAP_REFERRAL_LIMIT_EXCEEDED";
		break;
	default:
		res = talloc_asprintf(talloc_tos(), "Unknown LDAP Error (%d)",
				      rc);
		break;
	}
	if (res == NULL) {
		res = "Unknown LDAP Error";
	}
	return res;
}
