/* 
   Unix SMB/CIFS implementation.
   
   Copyright (C) Andrew Tridgell              2005
   
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

#ifndef __WEB_SERVER_H__
#define __WEB_SERVER_H__

#include "smbd/process_model.h"

struct websrv_context;

struct web_server_data {
	struct tls_params *tls_params;
	void (*http_process_input)(struct web_server_data *wdata, 
				   struct websrv_context *web);
	void *private_data;
};

struct http_header {
	char *name;
	char *value;
	struct http_header *prev, *next;
};

/*
  context of one open web connection
*/
struct websrv_context {
	struct task_server *task;
	struct stream_connection *conn;
	struct websrv_request_input {
		bool tls_detect;
		bool tls_first_char;
		uint8_t first_byte;
		DATA_BLOB partial;
		bool end_of_headers;
		char *url;
		unsigned content_length;
		bool post_request;
		struct http_header *headers;
	} input;
	struct websrv_request_output {
		bool output_pending;
		DATA_BLOB content;
		bool headers_sent;
		unsigned nsent;
	} output;
	struct session_data *session;
};

bool wsgi_initialize(struct web_server_data *wdata);
void http_error(struct websrv_context *web, const char *status, const char *info);
void websrv_output_headers(struct websrv_context *web, const char *status, struct http_header *headers);
void websrv_output(struct websrv_context *web, void *data, size_t length);
NTSTATUS http_parse_header(struct websrv_context *web, const char *line);

#endif /* __WEB_SERVER_H__ */
