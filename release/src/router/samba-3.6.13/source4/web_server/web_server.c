/* 
   Unix SMB/CIFS implementation.

   web server startup

   Copyright (C) Andrew Tridgell 2005
   Copyright (C) Jelmer Vernooij 2008
   
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
#include "web_server/web_server.h"
#include "../lib/util/dlinklist.h"
#include "lib/tls/tls.h"
#include "lib/events/events.h"
#include "lib/socket/netif.h"
#include "param/param.h"

/* don't allow connections to hang around forever */
#define HTTP_TIMEOUT 120

/*
  destroy a web connection
*/
static int websrv_destructor(struct websrv_context *web)
{
	return 0;
}

/*
  called when a connection times out. This prevents a stuck connection
  from hanging around forever
*/
static void websrv_timeout(struct tevent_context *event_context, 
			   struct tevent_timer *te, 
			   struct timeval t, void *private_data)
{
	struct websrv_context *web = talloc_get_type(private_data, struct websrv_context);
	struct stream_connection *conn = web->conn;
	web->conn = NULL;
	/* TODO: send a message to any running esp context on this connection
	   to stop running */
	stream_terminate_connection(conn, "websrv_timeout: timed out");	
}

/*
  setup for a raw http level error
*/
void http_error(struct websrv_context *web, const char *status, const char *info)
{
	char *s;
	s = talloc_asprintf(web,"<HTML><HEAD><TITLE>Error %s</TITLE></HEAD><BODY><H1>Error %s</H1><pre>%s</pre><p></BODY></HTML>\r\n\r\n", 
			    status, status, info);
	if (s == NULL) {
		stream_terminate_connection(web->conn, "http_error: out of memory");
		return;
	}
	websrv_output_headers(web, status, NULL);
	websrv_output(web, s, strlen(s));
}

void websrv_output_headers(struct websrv_context *web, const char *status, struct http_header *headers)
{
	char *s;
	DATA_BLOB b;
	struct http_header *hdr;

	s = talloc_asprintf(web, "HTTP/1.0 %s\r\n", status);
	if (s == NULL) return;
	for (hdr = headers; hdr; hdr = hdr->next) {
		s = talloc_asprintf_append_buffer(s, "%s: %s\r\n", hdr->name, hdr->value);
	}

	s = talloc_asprintf_append_buffer(s, "\r\n");

	b = web->output.content;
	web->output.content = data_blob_string_const(s);
	websrv_output(web, b.data, b.length);
	data_blob_free(&b);
}

void websrv_output(struct websrv_context *web, void *data, size_t length)
{
	data_blob_append(web, &web->output.content, data, length);
	EVENT_FD_NOT_READABLE(web->conn->event.fde);
	EVENT_FD_WRITEABLE(web->conn->event.fde);
	web->output.output_pending = true;
}


/*
  parse one line of header input
*/
NTSTATUS http_parse_header(struct websrv_context *web, const char *line)
{
	if (line[0] == 0) {
		web->input.end_of_headers = true;
	} else if (strncasecmp(line,"GET ", 4)==0) {
		web->input.url = talloc_strndup(web, &line[4], strcspn(&line[4], " \t"));
	} else if (strncasecmp(line,"POST ", 5)==0) {
		web->input.post_request = true;
		web->input.url = talloc_strndup(web, &line[5], strcspn(&line[5], " \t"));
	} else if (strchr(line, ':') == NULL) {
		http_error(web, "400 Bad request", "This server only accepts GET and POST requests");
		return NT_STATUS_INVALID_PARAMETER;
	} else if (strncasecmp(line, "Content-Length: ", 16)==0) {
		web->input.content_length = strtoul(&line[16], NULL, 10);
	} else {
		struct http_header *hdr = talloc_zero(web, struct http_header);
		char *colon = strchr(line, ':');
		if (colon == NULL) {
			http_error(web, "500 Internal Server Error", "invalidly formatted header");
			return NT_STATUS_INVALID_PARAMETER;
		}

		hdr->name = talloc_strndup(hdr, line, colon-line);
		hdr->value = talloc_strdup(hdr, colon+1);
		DLIST_ADD(web->input.headers, hdr);
	}

	/* ignore all other headers for now */
	return NT_STATUS_OK;
}

/*
  called when a web connection becomes readable
*/
static void websrv_recv(struct stream_connection *conn, uint16_t flags)
{
	struct web_server_data *wdata;
	struct websrv_context *web = talloc_get_type(conn->private_data,
						     struct websrv_context);
	NTSTATUS status;
	uint8_t buf[1024];
	size_t nread;
	uint8_t *p;
	DATA_BLOB b;

	/* not the most efficient http parser ever, but good enough for us */
	status = socket_recv(conn->socket, buf, sizeof(buf), &nread);
	if (NT_STATUS_IS_ERR(status)) goto failed;
	if (!NT_STATUS_IS_OK(status)) return;

	if (!data_blob_append(web, &web->input.partial, buf, nread))
		goto failed;

	/* parse any lines that are available */
	b = web->input.partial;
	while (!web->input.end_of_headers &&
	       (p=(uint8_t *)memchr(b.data, '\n', b.length))) {
		const char *line = (const char *)b.data;
		*p = 0;
		if (p != b.data && p[-1] == '\r') {
			p[-1] = 0;
		}
		status = http_parse_header(web, line);
		if (!NT_STATUS_IS_OK(status)) return;
		b.length -= (p - b.data) + 1;
		b.data = p+1;
	}

	/* keep any remaining bytes in web->input.partial */
	if (b.length == 0) {
		b.data = NULL;
	}
	b = data_blob_talloc(web, b.data, b.length);
	data_blob_free(&web->input.partial);
	web->input.partial = b;

	/* we finish when we have both the full headers (terminated by
	   a blank line) and any post data, as indicated by the
	   content_length */
	if (web->input.end_of_headers &&
	    web->input.partial.length >= web->input.content_length) {
		if (web->input.partial.length > web->input.content_length) {
			web->input.partial.data[web->input.content_length] = 0;
		}
		EVENT_FD_NOT_READABLE(web->conn->event.fde);

		/* the reference/unlink code here is quite subtle. It
		 is needed because the rendering of the web-pages, and
		 in particular the esp/ejs backend, is semi-async.  So
		 we could well end up in the connection timeout code
		 while inside http_process_input(), but we must not
		 destroy the stack variables being used by that
		 rendering process when we handle the timeout. */
		if (!talloc_reference(web->task, web)) goto failed;
		wdata = talloc_get_type(web->task->private_data, struct web_server_data);
		if (wdata == NULL) goto failed;
		wdata->http_process_input(wdata, web);
		talloc_unlink(web->task, web);
	}
	return;

failed:
	stream_terminate_connection(conn, "websrv_recv: failed");
}



/*
  called when a web connection becomes writable
*/
static void websrv_send(struct stream_connection *conn, uint16_t flags)
{
	struct websrv_context *web = talloc_get_type(conn->private_data,
						     struct websrv_context);
	NTSTATUS status;
	size_t nsent;
	DATA_BLOB b;

	b = web->output.content;
	b.data += web->output.nsent;
	b.length -= web->output.nsent;

	status = socket_send(conn->socket, &b, &nsent);
	if (NT_STATUS_IS_ERR(status)) {
		stream_terminate_connection(web->conn, "socket_send: failed");
		return;
	}
	if (!NT_STATUS_IS_OK(status)) {
		return;
	}

	web->output.nsent += nsent;

	if (web->output.content.length == web->output.nsent) {
		stream_terminate_connection(web->conn, "websrv_send: finished sending");
	}
}

/*
  establish a new connection to the web server
*/
static void websrv_accept(struct stream_connection *conn)
{
	struct task_server *task = talloc_get_type(conn->private_data, struct task_server);
	struct web_server_data *wdata = talloc_get_type(task->private_data, struct web_server_data);
	struct websrv_context *web;
	struct socket_context *tls_socket;

	web = talloc_zero(conn, struct websrv_context);
	if (web == NULL) goto failed;

	web->task = task;
	web->conn = conn;
	conn->private_data = web;
	talloc_set_destructor(web, websrv_destructor);

	event_add_timed(conn->event.ctx, web, 
			timeval_current_ofs(HTTP_TIMEOUT, 0),
			websrv_timeout, web);

	/* Overwrite the socket with a (possibly) TLS socket */
	tls_socket = tls_init_server(wdata->tls_params, conn->socket, 
				     conn->event.fde, "GPHO");
	/* We might not have TLS, or it might not have initilised */
	if (tls_socket) {
		talloc_unlink(conn, conn->socket);
		talloc_steal(conn, tls_socket);
		conn->socket = tls_socket;
	} else {
		DEBUG(3, ("TLS not available for web_server connections\n"));
	}

	return;

failed:
	talloc_free(conn);
}


static const struct stream_server_ops web_stream_ops = {
	.name			= "web",
	.accept_connection	= websrv_accept,
	.recv_handler		= websrv_recv,
	.send_handler		= websrv_send,
};

/*
  startup the web server task
*/
static void websrv_task_init(struct task_server *task)
{
	NTSTATUS status;
	uint16_t port = lpcfg_web_port(task->lp_ctx);
	const struct model_ops *model_ops;
	struct web_server_data *wdata;

	task_server_set_title(task, "task[websrv]");

	/* run the web server as a single process */
	model_ops = process_model_startup("single");
	if (!model_ops) goto failed;

	/* startup the Python processor - unfortunately we can't do this
	   per connection as that wouldn't allow for session variables */
	wdata = talloc_zero(task, struct web_server_data);
	if (wdata == NULL) goto failed;

	task->private_data = wdata;

	if (lpcfg_interfaces(task->lp_ctx) && lpcfg_bind_interfaces_only(task->lp_ctx)) {
		int num_interfaces;
		int i;
		struct interface *ifaces;

		load_interfaces(NULL, lpcfg_interfaces(task->lp_ctx), &ifaces);

		num_interfaces = iface_count(ifaces);
		for(i = 0; i < num_interfaces; i++) {
			const char *address = iface_n_ip(ifaces, i);
			status = stream_setup_socket(task,
						     task->event_ctx,
						     task->lp_ctx, model_ops,
						     &web_stream_ops, 
						     "ipv4", address, 
						     &port, lpcfg_socket_options(task->lp_ctx),
						     task);
			if (!NT_STATUS_IS_OK(status)) goto failed;
		}

		talloc_free(ifaces);
	} else {
		status = stream_setup_socket(task, task->event_ctx,
					     task->lp_ctx, model_ops,
					     &web_stream_ops,
					     "ipv4", lpcfg_socket_address(task->lp_ctx),
					     &port, lpcfg_socket_options(task->lp_ctx),
					     task);
		if (!NT_STATUS_IS_OK(status)) goto failed;
	}

	wdata->tls_params = tls_initialise(wdata, task->lp_ctx);
	if (wdata->tls_params == NULL) goto failed;

	if (!wsgi_initialize(wdata)) goto failed;


	return;

failed:
	task_server_terminate(task, "websrv_task_init: failed to startup web server task", true);
}


/* called at smbd startup - register ourselves as a server service */
NTSTATUS server_service_web_init(void)
{
	return register_server_service("web", websrv_task_init);
}
