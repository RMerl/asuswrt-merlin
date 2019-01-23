/*
   Unix SMB/CIFS implementation.
   handle unexpected packets
   Copyright (C) Andrew Tridgell 2000

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
#include "../lib/util/tevent_ntstatus.h"
#include "lib/async_req/async_sock.h"
#include "libsmb/nmblib.h"

static const char *nmbd_socket_dir(void)
{
	return lp_parm_const_string(-1, "nmbd", "socket dir",
				    get_dyn_NMBDSOCKETDIR());
}

struct nb_packet_query {
	enum packet_type type;
	size_t mailslot_namelen;
	int trn_id;
};

struct nb_packet_client;

struct nb_packet_server {
	struct tevent_context *ev;
	int listen_sock;
	int max_clients;
	int num_clients;
	struct nb_packet_client *clients;
};

struct nb_packet_client {
	struct nb_packet_client *prev, *next;
	struct nb_packet_server *server;

	enum packet_type type;
	int trn_id;
	char *mailslot_name;

	int sock;
	struct tevent_req *read_req;
	struct tevent_queue *out_queue;
};

static int nb_packet_server_destructor(struct nb_packet_server *s);
static void nb_packet_server_listener(struct tevent_context *ev,
				      struct tevent_fd *fde,
				      uint16_t flags,
				      void *private_data);

NTSTATUS nb_packet_server_create(TALLOC_CTX *mem_ctx,
				 struct tevent_context *ev,
				 int max_clients,
				 struct nb_packet_server **presult)
{
	struct nb_packet_server *result;
	struct tevent_fd *fde;
	NTSTATUS status;

	result = TALLOC_ZERO_P(mem_ctx, struct nb_packet_server);
	if (result == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}
	result->ev = ev;
	result->max_clients = max_clients;

	result->listen_sock = create_pipe_sock(
		nmbd_socket_dir(), "unexpected", 0755);
	if (result->listen_sock == -1) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}
	talloc_set_destructor(result, nb_packet_server_destructor);

	fde = tevent_add_fd(ev, result, result->listen_sock, TEVENT_FD_READ,
			    nb_packet_server_listener, result);
	if (fde == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	*presult = result;
	return NT_STATUS_OK;
fail:
	TALLOC_FREE(result);
	return status;
}

static int nb_packet_server_destructor(struct nb_packet_server *s)
{
	if (s->listen_sock != -1) {
		close(s->listen_sock);
		s->listen_sock = -1;
	}
	return 0;
}

static int nb_packet_client_destructor(struct nb_packet_client *c);
static ssize_t nb_packet_client_more(uint8_t *buf, size_t buflen,
				     void *private_data);
static void nb_packet_got_query(struct tevent_req *req);
static void nb_packet_client_read_done(struct tevent_req *req);

static void nb_packet_server_listener(struct tevent_context *ev,
				      struct tevent_fd *fde,
				      uint16_t flags,
				      void *private_data)
{
	struct nb_packet_server *server = talloc_get_type_abort(
		private_data, struct nb_packet_server);
	struct nb_packet_client *client;
	struct tevent_req *req;
	struct sockaddr_un sunaddr;
	socklen_t len;
	int sock;

	len = sizeof(sunaddr);

	sock = accept(server->listen_sock, (struct sockaddr *)(void *)&sunaddr,
		      &len);
	if (sock == -1) {
		return;
	}
	DEBUG(6,("accepted socket %d\n", sock));

	client = TALLOC_ZERO_P(server, struct nb_packet_client);
	if (client == NULL) {
		DEBUG(10, ("talloc failed\n"));
		close(sock);
		return;
	}
	client->sock = sock;
	client->server = server;
	talloc_set_destructor(client, nb_packet_client_destructor);

	client->out_queue = tevent_queue_create(
		client, "unexpected packet output");
	if (client->out_queue == NULL) {
		DEBUG(10, ("tevent_queue_create failed\n"));
		TALLOC_FREE(client);
		return;
	}

	req = read_packet_send(client, ev, client->sock,
			       sizeof(struct nb_packet_query),
			       nb_packet_client_more, NULL);
	if (req == NULL) {
		DEBUG(10, ("read_packet_send failed\n"));
		TALLOC_FREE(client);
		return;
	}
	tevent_req_set_callback(req, nb_packet_got_query, client);

	DLIST_ADD(server->clients, client);
	server->num_clients += 1;

	if (server->num_clients > server->max_clients) {
		DEBUG(10, ("Too many clients, dropping oldest\n"));

		/*
		 * no TALLOC_FREE here, don't mess with the list structs
		 */
		talloc_free(server->clients->prev);
	}
}

static ssize_t nb_packet_client_more(uint8_t *buf, size_t buflen,
				     void *private_data)
{
	struct nb_packet_query q;
	if (buflen > sizeof(struct nb_packet_query)) {
		return 0;
	}
	/* Take care of alignment */
	memcpy(&q, buf, sizeof(q));
	if (q.mailslot_namelen > 1024) {
		DEBUG(10, ("Got invalid mailslot namelen %d\n",
			   (int)q.mailslot_namelen));
		return -1;
	}
	return q.mailslot_namelen;
}

static int nb_packet_client_destructor(struct nb_packet_client *c)
{
	if (c->sock != -1) {
		close(c->sock);
		c->sock = -1;
	}
	DLIST_REMOVE(c->server->clients, c);
	c->server->num_clients -= 1;
	return 0;
}

static void nb_packet_got_query(struct tevent_req *req)
{
	struct nb_packet_client *client = tevent_req_callback_data(
		req, struct nb_packet_client);
	struct nb_packet_query q;
	uint8_t *buf;
	ssize_t nread, nwritten;
	int err;
	char c;

	nread = read_packet_recv(req, talloc_tos(), &buf, &err);
	TALLOC_FREE(req);
	if (nread < (ssize_t)sizeof(struct nb_packet_query)) {
		DEBUG(10, ("read_packet_recv returned %d (%s)\n",
			   (int)nread,
			   (nread == -1) ? strerror(err) : "wrong length"));
		TALLOC_FREE(client);
		return;
	}

	/* Take care of alignment */
	memcpy(&q, buf, sizeof(q));

	if (nread != sizeof(struct nb_packet_query) + q.mailslot_namelen) {
		DEBUG(10, ("nb_packet_got_query: Invalid mailslot namelength\n"));
		TALLOC_FREE(client);
		return;
	}

	client->trn_id = q.trn_id;
	client->type = q.type;
	if (q.mailslot_namelen > 0) {
		client->mailslot_name = talloc_strndup(
			client, (char *)buf + sizeof(q),
			q.mailslot_namelen);
		if (client->mailslot_name == NULL) {
			TALLOC_FREE(client);
			return;
		}
	}

	/*
	 * Yes, this is a blocking write of 1 byte into a unix
	 * domain socket that has never been written to. Highly
	 * unlikely that this actually blocks.
	 */
	c = 0;
	nwritten = sys_write(client->sock, &c, sizeof(c));
	if (nwritten != sizeof(c)) {
		DEBUG(10, ("Could not write success indicator to client: %s\n",
			   strerror(errno)));
		TALLOC_FREE(client);
		return;
	}

	client->read_req = read_packet_send(client, client->server->ev,
					    client->sock, 1, NULL, NULL);
	if (client->read_req == NULL) {
		DEBUG(10, ("Could not activate reader for client exit "
			   "detection\n"));
		TALLOC_FREE(client);
		return;
	}
	tevent_req_set_callback(client->read_req, nb_packet_client_read_done,
				client);
}

static void nb_packet_client_read_done(struct tevent_req *req)
{
	struct nb_packet_client *client = tevent_req_callback_data(
		req, struct nb_packet_client);
	ssize_t nread;
	uint8_t *buf;
	int err;

	nread = read_packet_recv(req, talloc_tos(), &buf, &err);
	TALLOC_FREE(req);
	if (nread == 1) {
		DEBUG(10, ("Protocol error, received data on write-only "
			   "unexpected socket: 0x%2.2x\n", (*buf)));
	}
	TALLOC_FREE(client);
}

static void nb_packet_client_send(struct nb_packet_client *client,
				  struct packet_struct *p);

void nb_packet_dispatch(struct nb_packet_server *server,
			struct packet_struct *p)
{
	struct nb_packet_client *c;
	uint16_t trn_id;

	switch (p->packet_type) {
	case NMB_PACKET:
		trn_id = p->packet.nmb.header.name_trn_id;
		break;
	case DGRAM_PACKET:
		trn_id = p->packet.dgram.header.dgm_id;
		break;
	default:
		DEBUG(10, ("Got invalid packet type %d\n",
			   (int)p->packet_type));
		return;
	}
	for (c = server->clients; c != NULL; c = c->next) {

		if (c->type != p->packet_type) {
			DEBUG(10, ("client expects packet %d, got %d\n",
				   c->type, p->packet_type));
			continue;
		}

		if (p->packet_type == NMB_PACKET) {
			/*
			 * See if the client specified transaction
			 * ID. Filter if it did.
			 */
			if ((c->trn_id != -1) &&
			    (c->trn_id != trn_id)) {
				DEBUG(10, ("client expects trn %d, got %d\n",
					   c->trn_id, trn_id));
				continue;
			}
		} else {
			/*
			 * See if the client specified a mailslot
			 * name. Filter if it did.
			 */
			if ((c->mailslot_name != NULL) &&
			    !match_mailslot_name(p, c->mailslot_name)) {
				continue;
			}
		}
		nb_packet_client_send(c, p);
	}
}

struct nb_packet_client_header {
	size_t len;
	enum packet_type type;
	time_t timestamp;
	struct in_addr ip;
	int port;
};

struct nb_packet_client_state {
	struct nb_packet_client *client;
	struct iovec iov[2];
	struct nb_packet_client_header hdr;
	char buf[1024];
};

static void nb_packet_client_send_done(struct tevent_req *req);

static void nb_packet_client_send(struct nb_packet_client *client,
				  struct packet_struct *p)
{
	struct nb_packet_client_state *state;
	struct tevent_req *req;

	if (tevent_queue_length(client->out_queue) > 10) {
		/*
		 * Skip clients that don't listen anyway, some form of DoS
		 * protection
		 */
		return;
	}

	state = TALLOC_ZERO_P(client, struct nb_packet_client_state);
	if (state == NULL) {
		DEBUG(10, ("talloc failed\n"));
		return;
	}

	state->client = client;

	state->hdr.ip = p->ip;
	state->hdr.port = p->port;
	state->hdr.timestamp = p->timestamp;
	state->hdr.type = p->packet_type;
	state->hdr.len = build_packet(state->buf, sizeof(state->buf), p);

	state->iov[0].iov_base = (char *)&state->hdr;
	state->iov[0].iov_len = sizeof(state->hdr);
	state->iov[1].iov_base = state->buf;
	state->iov[1].iov_len = state->hdr.len;

	TALLOC_FREE(client->read_req);

	req = writev_send(client, client->server->ev, client->out_queue,
			  client->sock, true, state->iov, 2);
	if (req == NULL) {
		DEBUG(10, ("writev_send failed\n"));
		return;
	}
	tevent_req_set_callback(req, nb_packet_client_send_done, state);
}

static void nb_packet_client_send_done(struct tevent_req *req)
{
	struct nb_packet_client_state *state = tevent_req_callback_data(
		req, struct nb_packet_client_state);
	struct nb_packet_client *client = state->client;
	ssize_t nwritten;
	int err;

	nwritten = writev_recv(req, &err);

	TALLOC_FREE(req);
	TALLOC_FREE(state);

	if (nwritten == -1) {
		DEBUG(10, ("writev failed: %s\n", strerror(err)));
		TALLOC_FREE(client);
	}

	if (tevent_queue_length(client->out_queue) == 0) {
		client->read_req = read_packet_send(client, client->server->ev,
						    client->sock, 1,
						    NULL, NULL);
		if (client->read_req == NULL) {
			DEBUG(10, ("Could not activate reader for client exit "
				   "detection\n"));
			TALLOC_FREE(client);
			return;
		}
		tevent_req_set_callback(client->read_req,
					nb_packet_client_read_done,
					client);
	}
}

struct nb_packet_reader {
	int sock;
};

struct nb_packet_reader_state {
	struct tevent_context *ev;
	struct sockaddr_un addr;
	struct nb_packet_query query;
	const char *mailslot_name;
	struct iovec iov[2];
	char c;
	struct nb_packet_reader *reader;
};

static int nb_packet_reader_destructor(struct nb_packet_reader *r);
static void nb_packet_reader_connected(struct tevent_req *subreq);
static void nb_packet_reader_sent_query(struct tevent_req *subreq);
static void nb_packet_reader_got_ack(struct tevent_req *subreq);

struct tevent_req *nb_packet_reader_send(TALLOC_CTX *mem_ctx,
					 struct tevent_context *ev,
					 enum packet_type type,
					 int trn_id,
					 const char *mailslot_name)
{
	struct tevent_req *req, *subreq;
	struct nb_packet_reader_state *state;
	char *path;

	req = tevent_req_create(mem_ctx, &state,
				struct nb_packet_reader_state);
	if (req == NULL) {
		return NULL;
	}
	state->ev = ev;
	state->query.trn_id = trn_id;
	state->query.type = type;
	state->mailslot_name = mailslot_name;

	if (mailslot_name != NULL) {
		state->query.mailslot_namelen = strlen(mailslot_name);
	}

	state->reader = TALLOC_ZERO_P(state, struct nb_packet_reader);
	if (tevent_req_nomem(state->reader, req)) {
		return tevent_req_post(req, ev);
	}

	path = talloc_asprintf(talloc_tos(), "%s/%s", nmbd_socket_dir(),
			       "unexpected");
	if (tevent_req_nomem(path, req)) {
		return tevent_req_post(req, ev);
	}
	state->addr.sun_family = AF_UNIX;
	strlcpy(state->addr.sun_path, path, sizeof(state->addr.sun_path));
	TALLOC_FREE(path);

	state->reader->sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (state->reader->sock == -1) {
		tevent_req_nterror(req, map_nt_error_from_unix(errno));
		return tevent_req_post(req, ev);
	}
	talloc_set_destructor(state->reader, nb_packet_reader_destructor);

	subreq = async_connect_send(state, ev, state->reader->sock,
				    (struct sockaddr *)(void *)&state->addr,
				    sizeof(state->addr));
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, nb_packet_reader_connected, req);
	return req;
}

static int nb_packet_reader_destructor(struct nb_packet_reader *r)
{
	if (r->sock != -1) {
		close(r->sock);
		r->sock = -1;
	}
	return 0;
}

static void nb_packet_reader_connected(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct nb_packet_reader_state *state = tevent_req_data(
		req, struct nb_packet_reader_state);
	int res, err;
	int num_iovecs = 1;

	res = async_connect_recv(subreq, &err);
	TALLOC_FREE(subreq);
	if (res == -1) {
		DEBUG(10, ("async_connect failed: %s\n", strerror(err)));
		tevent_req_nterror(req, map_nt_error_from_unix(err));
		return;
	}

	state->iov[0].iov_base = (char *)&state->query;
	state->iov[0].iov_len = sizeof(state->query);

	if (state->mailslot_name != NULL) {
		num_iovecs = 2;
		state->iov[1].iov_base = discard_const_p(
			char, state->mailslot_name);
		state->iov[1].iov_len = state->query.mailslot_namelen;
	}

	subreq = writev_send(state, state->ev, NULL, state->reader->sock,
			     true, state->iov, num_iovecs);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, nb_packet_reader_sent_query, req);
}

static void nb_packet_reader_sent_query(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct nb_packet_reader_state *state = tevent_req_data(
		req, struct nb_packet_reader_state);
	ssize_t written;
	int err;

	written = writev_recv(subreq, &err);
	TALLOC_FREE(subreq);
	if (written == -1) {
		tevent_req_nterror(req, map_nt_error_from_unix(err));
		return;
	}
	if (written != sizeof(state->query) + state->query.mailslot_namelen) {
		tevent_req_nterror(req, NT_STATUS_UNEXPECTED_IO_ERROR);
		return;
	}
	subreq = read_packet_send(state, state->ev, state->reader->sock,
				  sizeof(state->c), NULL, NULL);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, nb_packet_reader_got_ack, req);
}

static void nb_packet_reader_got_ack(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct nb_packet_reader_state *state = tevent_req_data(
		req, struct nb_packet_reader_state);
	ssize_t nread;
	int err;
	uint8_t *buf;

	nread = read_packet_recv(subreq, state, &buf, &err);
	TALLOC_FREE(subreq);
	if (nread == -1) {
		DEBUG(10, ("read_packet_recv returned %s\n",
			   strerror(err)));
		tevent_req_nterror(req, map_nt_error_from_unix(err));
		return;
	}
	if (nread != sizeof(state->c)) {
		DEBUG(10, ("read = %d, expected %d\n", (int)nread,
			   (int)sizeof(state->c)));
		tevent_req_nterror(req, NT_STATUS_UNEXPECTED_IO_ERROR);
		return;
	}
	tevent_req_done(req);
}

NTSTATUS nb_packet_reader_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
			       struct nb_packet_reader **preader)
{
	struct nb_packet_reader_state *state = tevent_req_data(
		req, struct nb_packet_reader_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}
	*preader = talloc_move(mem_ctx, &state->reader);
	return NT_STATUS_OK;
}

struct nb_packet_read_state {
	struct nb_packet_client_header hdr;
	uint8_t *buf;
	size_t buflen;
};

static ssize_t nb_packet_read_more(uint8_t *buf, size_t buflen, void *p);
static void nb_packet_read_done(struct tevent_req *subreq);

struct tevent_req *nb_packet_read_send(TALLOC_CTX *mem_ctx,
				       struct tevent_context *ev,
				       struct nb_packet_reader *reader)
{
	struct tevent_req *req, *subreq;
	struct nb_packet_read_state *state;

	req = tevent_req_create(mem_ctx, &state, struct nb_packet_read_state);
	if (req == NULL) {
		return NULL;
	}
	subreq = read_packet_send(state, ev, reader->sock,
				  sizeof(struct nb_packet_client_header),
				  nb_packet_read_more, state);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, nb_packet_read_done, req);
	return req;
}

static ssize_t nb_packet_read_more(uint8_t *buf, size_t buflen, void *p)
{
	struct nb_packet_read_state *state = talloc_get_type_abort(
		p, struct nb_packet_read_state);

	if (buflen > sizeof(struct nb_packet_client_header)) {
		/*
		 * Been here, done
		 */
		return 0;
	}
	memcpy(&state->hdr, buf, sizeof(struct nb_packet_client_header));
	return state->hdr.len;
}

static void nb_packet_read_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct nb_packet_read_state *state = tevent_req_data(
		req, struct nb_packet_read_state);
	ssize_t nread;
	int err;

	nread = read_packet_recv(subreq, state, &state->buf, &err);
	if (nread == -1) {
		tevent_req_nterror(req, map_nt_error_from_unix(err));
		return;
	}
	state->buflen = nread;
	tevent_req_done(req);
}

NTSTATUS nb_packet_read_recv(struct tevent_req *req,
			     struct packet_struct **ppacket)
{
	struct nb_packet_read_state *state = tevent_req_data(
		req, struct nb_packet_read_state);
	struct nb_packet_client_header hdr;
	struct packet_struct *packet;
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}

	memcpy(&hdr, state->buf, sizeof(hdr));

	packet = parse_packet(
		(char *)state->buf + sizeof(struct nb_packet_client_header),
		state->buflen - sizeof(struct nb_packet_client_header),
		state->hdr.type, state->hdr.ip, state->hdr.port);
	if (packet == NULL) {
		return NT_STATUS_INVALID_NETWORK_RESPONSE;
	}
	*ppacket = packet;
	return NT_STATUS_OK;
}
