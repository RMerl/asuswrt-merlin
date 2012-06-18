/* 
   Unix SMB/CIFS implementation.
   LDAP server
   Copyright (C) Volker Lendecke 2004
   Copyright (C) Stefan Metzmacher 2004
   
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

#include "libcli/ldap/ldap.h"
#include "lib/socket/socket.h"
#include "lib/stream/packet.h"

struct ldapsrv_connection {
	struct loadparm_context *lp_ctx;
	struct stream_connection *connection;
	struct gensec_security *gensec;
	struct auth_session_info *session_info;
	struct ldapsrv_service *service;
	struct cli_credentials *server_credentials;
	struct ldb_context *ldb;

	struct {
		struct socket_context *raw;
		struct socket_context *tls;
		struct socket_context *sasl;
	} sockets;

	bool global_catalog;

	struct packet_context *packet;

	struct {
		int initial_timeout;
		int conn_idle_time;
		int max_page_size;
		int search_timeout;
		
		struct tevent_timer *ite;
		struct tevent_timer *te;
	} limits;
};

struct ldapsrv_call {
	struct ldapsrv_connection *conn;
	struct ldap_message *request;
	struct ldapsrv_reply {
		struct ldapsrv_reply *prev, *next;
		struct ldap_message *msg;
	} *replies;
	packet_send_callback_fn_t send_callback;
	void *send_private;
};

struct ldapsrv_service {
	struct tls_params *tls_params;
	struct task_server *task;
};

#include "ldap_server/proto.h"
