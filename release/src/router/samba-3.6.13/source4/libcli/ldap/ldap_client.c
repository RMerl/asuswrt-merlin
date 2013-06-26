/* 
   Unix SMB/CIFS mplementation.
   LDAP protocol helper functions for SAMBA
   
   Copyright (C) Andrew Tridgell  2004
   Copyright (C) Volker Lendecke 2004
   Copyright (C) Stefan Metzmacher 2004
   Copyright (C) Simo Sorce 2004
    
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
#include <tevent.h>
#include "lib/socket/socket.h"
#include "../lib/util/asn1.h"
#include "../lib/util/dlinklist.h"
#include "libcli/ldap/libcli_ldap.h"
#include "libcli/ldap/ldap_proto.h"
#include "libcli/ldap/ldap_client.h"
#include "libcli/composite/composite.h"
#include "lib/stream/packet.h"
#include "lib/tls/tls.h"
#include "auth/gensec/gensec.h"
#include "system/time.h"
#include "param/param.h"
#include "libcli/resolve/resolve.h"

/**
  create a new ldap_connection stucture. The event context is optional
*/
_PUBLIC_ struct ldap_connection *ldap4_new_connection(TALLOC_CTX *mem_ctx, 
					     struct loadparm_context *lp_ctx,
					     struct tevent_context *ev)
{
	struct ldap_connection *conn;

	if (ev == NULL) {
		return NULL;
	}

	conn = talloc_zero(mem_ctx, struct ldap_connection);
	if (conn == NULL) {
		return NULL;
	}

	conn->next_messageid  = 1;
	conn->event.event_ctx = ev;

	conn->lp_ctx = lp_ctx;

	/* set a reasonable request timeout */
	conn->timeout = 60;

	/* explicitly avoid reconnections by default */
	conn->reconnect.max_retries = 0;
	
	return conn;
}

/*
  the connection is dead
*/
static void ldap_connection_dead(struct ldap_connection *conn)
{
	struct ldap_request *req;

	talloc_free(conn->sock);  /* this will also free event.fde */
	talloc_free(conn->packet);
	conn->sock = NULL;
	conn->event.fde = NULL;
	conn->packet = NULL;

	/* return an error for any pending request ... */
	while (conn->pending) {
		req = conn->pending;
		DLIST_REMOVE(req->conn->pending, req);
		req->state = LDAP_REQUEST_DONE;
		req->status = NT_STATUS_UNEXPECTED_NETWORK_ERROR;
		if (req->async.fn) {
			req->async.fn(req);
		}
	}
}

static void ldap_reconnect(struct ldap_connection *conn);

/*
  handle packet errors
*/
static void ldap_error_handler(void *private_data, NTSTATUS status)
{
	struct ldap_connection *conn = talloc_get_type(private_data, 
						       struct ldap_connection);
	ldap_connection_dead(conn);

	/* but try to reconnect so that the ldb client can go on */
	ldap_reconnect(conn);
}


/*
  match up with a pending message, adding to the replies list
*/
static void ldap_match_message(struct ldap_connection *conn, struct ldap_message *msg)
{
	struct ldap_request *req;
	int i;

	for (req=conn->pending; req; req=req->next) {
		if (req->messageid == msg->messageid) break;
	}
	/* match a zero message id to the last request sent.
	   It seems that servers send 0 if unable to parse */
	if (req == NULL && msg->messageid == 0) {
		req = conn->pending;
	}
	if (req == NULL) {
		DEBUG(0,("ldap: no matching message id for %u\n",
			 msg->messageid));
		talloc_free(msg);
		return;
	}

	/* Check for undecoded critical extensions */
	for (i=0; msg->controls && msg->controls[i]; i++) {
		if (!msg->controls_decoded[i] && 
		    msg->controls[i]->critical) {
			req->status = NT_STATUS_LDAP(LDAP_UNAVAILABLE_CRITICAL_EXTENSION);
			req->state = LDAP_REQUEST_DONE;
			DLIST_REMOVE(conn->pending, req);
			if (req->async.fn) {
				req->async.fn(req);
			}
			return;
		}
	}

	/* add to the list of replies received */
	talloc_steal(req, msg);
	req->replies = talloc_realloc(req, req->replies, 
				      struct ldap_message *, req->num_replies+1);
	if (req->replies == NULL) {
		req->status = NT_STATUS_NO_MEMORY;
		req->state = LDAP_REQUEST_DONE;
		DLIST_REMOVE(conn->pending, req);
		if (req->async.fn) {
			req->async.fn(req);
		}
		return;
	}

	req->replies[req->num_replies] = talloc_steal(req->replies, msg);
	req->num_replies++;

	if (msg->type != LDAP_TAG_SearchResultEntry &&
	    msg->type != LDAP_TAG_SearchResultReference) {
		/* currently only search results expect multiple
		   replies */
		req->state = LDAP_REQUEST_DONE;
		DLIST_REMOVE(conn->pending, req);
	}

	if (req->async.fn) {
		req->async.fn(req);
	}
}


/*
  decode/process LDAP data
*/
static NTSTATUS ldap_recv_handler(void *private_data, DATA_BLOB blob)
{
	NTSTATUS status;
	struct ldap_connection *conn = talloc_get_type(private_data, 
						       struct ldap_connection);
	struct ldap_message *msg = talloc(conn, struct ldap_message);
	struct asn1_data *asn1 = asn1_init(conn);

	if (asn1 == NULL || msg == NULL) {
		return NT_STATUS_LDAP(LDAP_PROTOCOL_ERROR);
	}

	if (!asn1_load(asn1, blob)) {
		talloc_free(msg);
		talloc_free(asn1);
		return NT_STATUS_LDAP(LDAP_PROTOCOL_ERROR);
	}
	
	status = ldap_decode(asn1, samba_ldap_control_handlers(), msg);
	if (!NT_STATUS_IS_OK(status)) {
		asn1_free(asn1);
		return status;
	}

	ldap_match_message(conn, msg);

	data_blob_free(&blob);
	asn1_free(asn1);
	return NT_STATUS_OK;
}

/* Handle read events, from the GENSEC socket callback, or real events */
void ldap_read_io_handler(void *private_data, uint16_t flags) 
{
	struct ldap_connection *conn = talloc_get_type(private_data, 
						       struct ldap_connection);
	packet_recv(conn->packet);
}

/*
  handle ldap socket events
*/
static void ldap_io_handler(struct tevent_context *ev, struct tevent_fd *fde, 
			    uint16_t flags, void *private_data)
{
	struct ldap_connection *conn = talloc_get_type(private_data, 
						       struct ldap_connection);
	if (flags & TEVENT_FD_WRITE) {
		packet_queue_run(conn->packet);
		if (!tls_enabled(conn->sock)) return;
	}
	if (flags & TEVENT_FD_READ) {
		ldap_read_io_handler(private_data, flags);
	}
}

/*
  parse a ldap URL
*/
static NTSTATUS ldap_parse_basic_url(TALLOC_CTX *mem_ctx, const char *url,
				     char **host, uint16_t *port, bool *ldaps)
{
	int tmp_port = 0;
	char protocol[11];
	char tmp_host[1025];
	int ret;

	/* Paranoia check */
	SMB_ASSERT(sizeof(protocol)>10 && sizeof(tmp_host)>254);
		
	ret = sscanf(url, "%10[^:]://%254[^:/]:%d", protocol, tmp_host, &tmp_port);
	if (ret < 2) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (strequal(protocol, "ldap")) {
		*port = 389;
		*ldaps = false;
	} else if (strequal(protocol, "ldaps")) {
		*port = 636;
		*ldaps = true;
	} else {
		DEBUG(0, ("unrecognised ldap protocol (%s)!\n", protocol));
		return NT_STATUS_PROTOCOL_UNREACHABLE;
	}

	if (tmp_port != 0)
		*port = tmp_port;

	*host = talloc_strdup(mem_ctx, tmp_host);
	NT_STATUS_HAVE_NO_MEMORY(*host);

	return NT_STATUS_OK;
}

/*
  connect to a ldap server
*/

struct ldap_connect_state {
	struct composite_context *ctx;
	struct ldap_connection *conn;
};

static void ldap_connect_recv_unix_conn(struct composite_context *ctx);
static void ldap_connect_recv_tcp_conn(struct composite_context *ctx);

_PUBLIC_ struct composite_context *ldap_connect_send(struct ldap_connection *conn,
					    const char *url)
{
	struct composite_context *result, *ctx;
	struct ldap_connect_state *state;
	char protocol[11];
	int ret;

	result = talloc_zero(conn, struct composite_context);
	if (result == NULL) goto failed;
	result->state = COMPOSITE_STATE_IN_PROGRESS;
	result->async.fn = NULL;
	result->event_ctx = conn->event.event_ctx;

	state = talloc(result, struct ldap_connect_state);
	if (state == NULL) goto failed;
	state->ctx = result;
	result->private_data = state;

	state->conn = conn;

	if (conn->reconnect.url == NULL) {
		conn->reconnect.url = talloc_strdup(conn, url);
		if (conn->reconnect.url == NULL) goto failed;
	}

	/* Paranoia check */
	SMB_ASSERT(sizeof(protocol)>10);

	ret = sscanf(url, "%10[^:]://", protocol);
	if (ret < 1) {
		return NULL;
	}

	if (strequal(protocol, "ldapi")) {
		struct socket_address *unix_addr;
		char path[1025];
	
		NTSTATUS status = socket_create("unix", SOCKET_TYPE_STREAM, &conn->sock, 0);
		if (!NT_STATUS_IS_OK(status)) {
			return NULL;
		}
		talloc_steal(conn, conn->sock);
		SMB_ASSERT(sizeof(protocol)>10);
		SMB_ASSERT(sizeof(path)>1024);
	
		/* LDAPI connections are to localhost, so give the
		 * local host name as the target for gensec's
		 * DIGEST-MD5 mechanism */
		conn->host = talloc_asprintf(conn, "%s.%s",
					     lpcfg_netbios_name(conn->lp_ctx),
					     lpcfg_dnsdomain(conn->lp_ctx));
		if (composite_nomem(conn->host, state->ctx)) {
			return result;
		}

		/* The %c specifier doesn't null terminate :-( */
		ZERO_STRUCT(path);
		ret = sscanf(url, "%10[^:]://%1025c", protocol, path);
		if (ret < 2) {
			composite_error(state->ctx, NT_STATUS_INVALID_PARAMETER);
			return result;
		}

		rfc1738_unescape(path);
	
		unix_addr = socket_address_from_strings(conn, conn->sock->backend_name, 
							path, 0);
		if (!unix_addr) {
			return NULL;
		}

		ctx = socket_connect_send(conn->sock, NULL, unix_addr, 
					  0, conn->event.event_ctx);
		ctx->async.fn = ldap_connect_recv_unix_conn;
		ctx->async.private_data = state;
		return result;
	} else {
		NTSTATUS status = ldap_parse_basic_url(conn, url, &conn->host,
							  &conn->port, &conn->ldaps);
		if (!NT_STATUS_IS_OK(state->ctx->status)) {
			composite_error(state->ctx, status);
			return result;
		}
		
		ctx = socket_connect_multi_send(state, conn->host, 1, &conn->port,
						lpcfg_resolve_context(conn->lp_ctx), conn->event.event_ctx);
		if (ctx == NULL) goto failed;

		ctx->async.fn = ldap_connect_recv_tcp_conn;
		ctx->async.private_data = state;
		return result;
	}
 failed:
	talloc_free(result);
	return NULL;
}

static void ldap_connect_got_sock(struct composite_context *ctx, 
				  struct ldap_connection *conn) 
{
	/* setup a handler for events on this socket */
	conn->event.fde = tevent_add_fd(conn->event.event_ctx, conn->sock,
					socket_get_fd(conn->sock),
					TEVENT_FD_READ, ldap_io_handler, conn);
	if (conn->event.fde == NULL) {
		composite_error(ctx, NT_STATUS_INTERNAL_ERROR);
		return;
	}

	tevent_fd_set_close_fn(conn->event.fde, socket_tevent_fd_close_fn);
	socket_set_flags(conn->sock, SOCKET_FLAG_NOCLOSE);

	talloc_steal(conn, conn->sock);
	if (conn->ldaps) {
		struct socket_context *tls_socket;
		char *cafile = lpcfg_tls_cafile(conn->sock, conn->lp_ctx);

		if (!cafile || !*cafile) {
			talloc_free(conn->sock);
			return;
		}

		tls_socket = tls_init_client(conn->sock, conn->event.fde, cafile);
		talloc_free(cafile);

		if (tls_socket == NULL) {
			talloc_free(conn->sock);
			return;
		}

		conn->sock = talloc_steal(conn, tls_socket);
	}

	conn->packet = packet_init(conn);
	if (conn->packet == NULL) {
		talloc_free(conn->sock);
		return;
	}

	packet_set_private(conn->packet, conn);
	packet_set_socket(conn->packet, conn->sock);
	packet_set_callback(conn->packet, ldap_recv_handler);
	packet_set_full_request(conn->packet, ldap_full_packet);
	packet_set_error_handler(conn->packet, ldap_error_handler);
	packet_set_event_context(conn->packet, conn->event.event_ctx);
	packet_set_fde(conn->packet, conn->event.fde);
/*	packet_set_serialise(conn->packet); */

	if (conn->ldaps) {
		packet_set_unreliable_select(conn->packet);
	}

	composite_done(ctx);
}

static void ldap_connect_recv_tcp_conn(struct composite_context *ctx)
{
	struct ldap_connect_state *state =
		talloc_get_type(ctx->async.private_data,
				struct ldap_connect_state);
	struct ldap_connection *conn = state->conn;
	uint16_t port;
	NTSTATUS status = socket_connect_multi_recv(ctx, state, &conn->sock,
						       &port);
	if (!NT_STATUS_IS_OK(status)) {
		composite_error(state->ctx, status);
		return;
	}

	ldap_connect_got_sock(state->ctx, conn);
}

static void ldap_connect_recv_unix_conn(struct composite_context *ctx)
{
	struct ldap_connect_state *state =
		talloc_get_type(ctx->async.private_data,
				struct ldap_connect_state);
	struct ldap_connection *conn = state->conn;

	NTSTATUS status = socket_connect_recv(ctx);

	if (!NT_STATUS_IS_OK(state->ctx->status)) {
		composite_error(state->ctx, status);
		return;
	}

	ldap_connect_got_sock(state->ctx, conn);
}

_PUBLIC_ NTSTATUS ldap_connect_recv(struct composite_context *ctx)
{
	NTSTATUS status = composite_wait(ctx);
	talloc_free(ctx);
	return status;
}

_PUBLIC_ NTSTATUS ldap_connect(struct ldap_connection *conn, const char *url)
{
	struct composite_context *ctx = ldap_connect_send(conn, url);
	return ldap_connect_recv(ctx);
}

/* set reconnect parameters */

_PUBLIC_ void ldap_set_reconn_params(struct ldap_connection *conn, int max_retries)
{
	if (conn) {
		conn->reconnect.max_retries = max_retries;
		conn->reconnect.retries = 0;
		conn->reconnect.previous = time_mono(NULL);
	}
}

/* Actually this function is NOT ASYNC safe, FIXME? */
static void ldap_reconnect(struct ldap_connection *conn)
{
	NTSTATUS status;
	time_t now = time_mono(NULL);

	/* do we have set up reconnect ? */
	if (conn->reconnect.max_retries == 0) return;

	/* is the retry time expired ? */
	if (now > conn->reconnect.previous + 30) {
		conn->reconnect.retries = 0;
		conn->reconnect.previous = now;
	}

	/* are we reconnectind too often and too fast? */
	if (conn->reconnect.retries > conn->reconnect.max_retries) return;

	/* keep track of the number of reconnections */
	conn->reconnect.retries++;

	/* reconnect */
	status = ldap_connect(conn, conn->reconnect.url);
	if ( ! NT_STATUS_IS_OK(status)) {
		return;
	}

	/* rebind */
	status = ldap_rebind(conn);
	if ( ! NT_STATUS_IS_OK(status)) {
		ldap_connection_dead(conn);
	}
}

/* destroy an open ldap request */
static int ldap_request_destructor(struct ldap_request *req)
{
	if (req->state == LDAP_REQUEST_PENDING) {
		DLIST_REMOVE(req->conn->pending, req);
	}
	return 0;
}

/*
  called on timeout of a ldap request
*/
static void ldap_request_timeout(struct tevent_context *ev, struct tevent_timer *te, 
				      struct timeval t, void *private_data)
{
	struct ldap_request *req = talloc_get_type(private_data, struct ldap_request);
	req->status = NT_STATUS_IO_TIMEOUT;
	if (req->state == LDAP_REQUEST_PENDING) {
		DLIST_REMOVE(req->conn->pending, req);
	}
	req->state = LDAP_REQUEST_DONE;
	if (req->async.fn) {
		req->async.fn(req);
	}
}


/*
  called on completion of a failed ldap request
*/
static void ldap_request_failed_complete(struct tevent_context *ev, struct tevent_timer *te,
				      struct timeval t, void *private_data)
{
	struct ldap_request *req = talloc_get_type(private_data, struct ldap_request);
	if (req->async.fn) {
		req->async.fn(req);
	}
}

/*
  called on completion of a one-way ldap request
*/
static void ldap_request_oneway_complete(void *private_data)
{
	struct ldap_request *req = talloc_get_type(private_data, struct ldap_request);
	if (req->state == LDAP_REQUEST_PENDING) {
		DLIST_REMOVE(req->conn->pending, req);
	}
	req->state = LDAP_REQUEST_DONE;
	if (req->async.fn) {
		req->async.fn(req);
	}
}
/*
  send a ldap message - async interface
*/
_PUBLIC_ struct ldap_request *ldap_request_send(struct ldap_connection *conn,
				       struct ldap_message *msg)
{
	struct ldap_request *req;
	NTSTATUS status = NT_STATUS_UNSUCCESSFUL;
	packet_send_callback_fn_t send_callback = NULL;

	req = talloc_zero(conn, struct ldap_request);
	if (req == NULL) return NULL;

	if (conn->sock == NULL) {
		status = NT_STATUS_INVALID_CONNECTION;
		goto failed;
	}

	req->state       = LDAP_REQUEST_SEND;
	req->conn        = conn;
	req->messageid   = conn->next_messageid++;
	if (conn->next_messageid == 0) {
		conn->next_messageid = 1;
	}
	req->type        = msg->type;
	if (req->messageid == -1) {
		goto failed;
	}

	talloc_set_destructor(req, ldap_request_destructor);

	msg->messageid = req->messageid;

	if (!ldap_encode(msg, samba_ldap_control_handlers(), &req->data, req)) {
		status = NT_STATUS_INTERNAL_ERROR;
		goto failed;		
	}

	if (req->type == LDAP_TAG_AbandonRequest ||
	    req->type == LDAP_TAG_UnbindRequest) {
		send_callback = ldap_request_oneway_complete;
	}

	status = packet_send_callback(conn->packet, req->data,
				      send_callback, req);
	if (!NT_STATUS_IS_OK(status)) {
		goto failed;
	}

	req->state = LDAP_REQUEST_PENDING;
	DLIST_ADD(conn->pending, req);

	/* put a timeout on the request */
	req->time_event = tevent_add_timer(conn->event.event_ctx, req,
					   timeval_current_ofs(conn->timeout, 0),
					   ldap_request_timeout, req);

	return req;

failed:
	req->status = status;
	req->state = LDAP_REQUEST_ERROR;
	tevent_add_timer(conn->event.event_ctx, req, timeval_zero(),
			 ldap_request_failed_complete, req);

	return req;
}


/*
  wait for a request to complete
  note that this does not destroy the request
*/
_PUBLIC_ NTSTATUS ldap_request_wait(struct ldap_request *req)
{
	while (req->state < LDAP_REQUEST_DONE) {
		if (tevent_loop_once(req->conn->event.event_ctx) != 0) {
			req->status = NT_STATUS_UNEXPECTED_NETWORK_ERROR;
			break;
		}
	}
	return req->status;
}


/*
  a mapping of ldap response code to strings
*/
static const struct {
	enum ldap_result_code code;
	const char *str;
} ldap_code_map[] = {
#define _LDAP_MAP_CODE(c) { c, #c }
	_LDAP_MAP_CODE(LDAP_SUCCESS),
	_LDAP_MAP_CODE(LDAP_OPERATIONS_ERROR),
	_LDAP_MAP_CODE(LDAP_PROTOCOL_ERROR),
	_LDAP_MAP_CODE(LDAP_TIME_LIMIT_EXCEEDED),
	_LDAP_MAP_CODE(LDAP_SIZE_LIMIT_EXCEEDED),
	_LDAP_MAP_CODE(LDAP_COMPARE_FALSE),
	_LDAP_MAP_CODE(LDAP_COMPARE_TRUE),
	_LDAP_MAP_CODE(LDAP_AUTH_METHOD_NOT_SUPPORTED),
	_LDAP_MAP_CODE(LDAP_STRONG_AUTH_REQUIRED),
	_LDAP_MAP_CODE(LDAP_REFERRAL),
	_LDAP_MAP_CODE(LDAP_ADMIN_LIMIT_EXCEEDED),
	_LDAP_MAP_CODE(LDAP_UNAVAILABLE_CRITICAL_EXTENSION),
	_LDAP_MAP_CODE(LDAP_CONFIDENTIALITY_REQUIRED),
	_LDAP_MAP_CODE(LDAP_SASL_BIND_IN_PROGRESS),
	_LDAP_MAP_CODE(LDAP_NO_SUCH_ATTRIBUTE),
	_LDAP_MAP_CODE(LDAP_UNDEFINED_ATTRIBUTE_TYPE),
	_LDAP_MAP_CODE(LDAP_INAPPROPRIATE_MATCHING),
	_LDAP_MAP_CODE(LDAP_CONSTRAINT_VIOLATION),
	_LDAP_MAP_CODE(LDAP_ATTRIBUTE_OR_VALUE_EXISTS),
	_LDAP_MAP_CODE(LDAP_INVALID_ATTRIBUTE_SYNTAX),
	_LDAP_MAP_CODE(LDAP_NO_SUCH_OBJECT),
	_LDAP_MAP_CODE(LDAP_ALIAS_PROBLEM),
	_LDAP_MAP_CODE(LDAP_INVALID_DN_SYNTAX),
	_LDAP_MAP_CODE(LDAP_ALIAS_DEREFERENCING_PROBLEM),
	_LDAP_MAP_CODE(LDAP_INAPPROPRIATE_AUTHENTICATION),
	_LDAP_MAP_CODE(LDAP_INVALID_CREDENTIALS),
	_LDAP_MAP_CODE(LDAP_INSUFFICIENT_ACCESS_RIGHTS),
	_LDAP_MAP_CODE(LDAP_BUSY),
	_LDAP_MAP_CODE(LDAP_UNAVAILABLE),
	_LDAP_MAP_CODE(LDAP_UNWILLING_TO_PERFORM),
	_LDAP_MAP_CODE(LDAP_LOOP_DETECT),
	_LDAP_MAP_CODE(LDAP_NAMING_VIOLATION),
	_LDAP_MAP_CODE(LDAP_OBJECT_CLASS_VIOLATION),
	_LDAP_MAP_CODE(LDAP_NOT_ALLOWED_ON_NON_LEAF),
	_LDAP_MAP_CODE(LDAP_NOT_ALLOWED_ON_RDN),
	_LDAP_MAP_CODE(LDAP_ENTRY_ALREADY_EXISTS),
	_LDAP_MAP_CODE(LDAP_OBJECT_CLASS_MODS_PROHIBITED),
	_LDAP_MAP_CODE(LDAP_AFFECTS_MULTIPLE_DSAS),
	_LDAP_MAP_CODE(LDAP_OTHER)
};

/*
  used to setup the status code from a ldap response
*/
_PUBLIC_ NTSTATUS ldap_check_response(struct ldap_connection *conn, struct ldap_Result *r)
{
	int i;
	const char *codename = "unknown";

	if (r->resultcode == LDAP_SUCCESS) {
		return NT_STATUS_OK;
	}

	if (conn->last_error) {
		talloc_free(conn->last_error);
	}

	for (i=0;i<ARRAY_SIZE(ldap_code_map);i++) {
		if (r->resultcode == ldap_code_map[i].code) {
			codename = ldap_code_map[i].str;
			break;
		}
	}

	conn->last_error = talloc_asprintf(conn, "LDAP error %u %s - %s <%s> <%s>", 
					   r->resultcode,
					   codename,
					   r->dn?r->dn:"(NULL)", 
					   r->errormessage?r->errormessage:"", 
					   r->referral?r->referral:"");
	
	return NT_STATUS_LDAP(r->resultcode);
}

/*
  return error string representing the last error
*/
_PUBLIC_ const char *ldap_errstr(struct ldap_connection *conn, 
			TALLOC_CTX *mem_ctx, 
			NTSTATUS status)
{
	if (NT_STATUS_IS_LDAP(status) && conn->last_error != NULL) {
		return talloc_strdup(mem_ctx, conn->last_error);
	}
	return talloc_asprintf(mem_ctx, "LDAP client internal error: %s", nt_errstr(status));
}


/*
  return the Nth result message, waiting if necessary
*/
_PUBLIC_ NTSTATUS ldap_result_n(struct ldap_request *req, int n, struct ldap_message **msg)
{
	*msg = NULL;

	NT_STATUS_HAVE_NO_MEMORY(req);

	while (req->state < LDAP_REQUEST_DONE && n >= req->num_replies) {
		if (tevent_loop_once(req->conn->event.event_ctx) != 0) {
			return NT_STATUS_UNEXPECTED_NETWORK_ERROR;
		}
	}

	if (n < req->num_replies) {
		*msg = req->replies[n];
		return NT_STATUS_OK;
	}

	if (!NT_STATUS_IS_OK(req->status)) {
		return req->status;
	}

	return NT_STATUS_NO_MORE_ENTRIES;
}


/*
  return a single result message, checking if it is of the expected LDAP type
*/
_PUBLIC_ NTSTATUS ldap_result_one(struct ldap_request *req, struct ldap_message **msg, int type)
{
	NTSTATUS status;
	status = ldap_result_n(req, 0, msg);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if ((*msg)->type != type) {
		*msg = NULL;
		return NT_STATUS_UNEXPECTED_NETWORK_ERROR;
	}
	return status;
}

/*
  a simple ldap transaction, for single result requests that only need a status code
  this relies on single valued requests having the response type == request type + 1
*/
_PUBLIC_ NTSTATUS ldap_transaction(struct ldap_connection *conn, struct ldap_message *msg)
{
	struct ldap_request *req = ldap_request_send(conn, msg);
	struct ldap_message *res;
	NTSTATUS status;
	status = ldap_result_n(req, 0, &res);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(req);
		return status;
	}
	if (res->type != msg->type + 1) {
		talloc_free(req);
		return NT_STATUS_LDAP(LDAP_PROTOCOL_ERROR);
	}
	status = ldap_check_response(conn, &res->r.GeneralResult);
	talloc_free(req);
	return status;
}
