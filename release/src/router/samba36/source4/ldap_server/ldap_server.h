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

#include "libcli/ldap/libcli_ldap.h"
#include "lib/socket/socket.h"
#include "lib/stream/packet.h"
#include "system/network.h"

struct ldapsrv_connection {
	struct loadparm_context *lp_ctx;
	struct stream_connection *connection;
	struct gensec_security *gensec;
	struct auth_session_info *session_info;
	struct ldapsrv_service *service;
	struct cli_credentials *server_credentials;
	struct ldb_context *ldb;

	struct {
		struct tevent_queue *send_queue;
		struct tstream_context *raw;
		struct tstream_context *tls;
		struct tstream_context *sasl;
		struct tstream_context *active;
	} sockets;

	bool global_catalog;
	bool is_privileged;

	struct {
		int initial_timeout;
		int conn_idle_time;
		int max_page_size;
		int search_timeout;
		struct timeval endtime;
		const char *reason;
	} limits;

	struct tevent_req *active_call;
};

struct ldapsrv_call {
	struct ldapsrv_connection *conn;
	struct ldap_message *request;
	struct ldapsrv_reply {
		struct ldapsrv_reply *prev, *next;
		struct ldap_message *msg;
	} *replies;
	struct iovec out_iov;

	struct tevent_req *(*postprocess_send)(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       void *private_data);
	NTSTATUS (*postprocess_recv)(struct tevent_req *req);
	void *postprocess_private;
};

struct ldapsrv_service {
	struct tstream_tls_params *tls_params;
	struct task_server *task;
	struct tevent_queue *call_queue;
};

#include "ldap_server/proto.h"
