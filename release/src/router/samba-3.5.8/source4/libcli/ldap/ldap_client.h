/* 
   Unix SMB/CIFS Implementation.

   ldap client side header

   Copyright (C) Andrew Tridgell 2005
    
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

enum ldap_request_state { LDAP_REQUEST_SEND=1, LDAP_REQUEST_PENDING=2, LDAP_REQUEST_DONE=3, LDAP_REQUEST_ERROR=4 };

/* this is the handle that the caller gets when an async ldap message
   is sent */
struct ldap_request {
	struct ldap_request *next, *prev;
	struct ldap_connection *conn;

	enum ldap_request_tag type;
	int messageid;
	enum ldap_request_state state;

	int num_replies;
	struct ldap_message **replies;

	NTSTATUS status;
	DATA_BLOB data;
	struct {
		void (*fn)(struct ldap_request *);
		void *private_data;
	} async;

	struct tevent_timer *time_event;
};


/* main context for a ldap client connection */
struct ldap_connection {
	struct socket_context *sock;
	struct loadparm_context *lp_ctx;

	char *host;
	uint16_t port;
	bool ldaps;

	const char *auth_dn;
	const char *simple_pw;

	struct {
		char *url;
		int max_retries;
		int retries;
		time_t previous;
	} reconnect;

	struct {
		enum { LDAP_BIND_SIMPLE, LDAP_BIND_SASL } type;
		void *creds;
	} bind;

	/* next message id to assign */
	unsigned next_messageid;

	/* Outstanding LDAP requests that have not yet been replied to */
	struct ldap_request *pending;

	/* Let's support SASL */
	struct gensec_security *gensec;

	/* the default timeout for messages */
	int timeout;

	/* last error message */
	char *last_error;

	struct {
		struct tevent_context *event_ctx;
		struct tevent_fd *fde;
	} event;

	struct packet_context *packet;
};

struct ldap_connection *ldap4_new_connection(TALLOC_CTX *mem_ctx, 
					     struct loadparm_context *lp_ctx,
					     struct tevent_context *ev);

NTSTATUS ldap_connect(struct ldap_connection *conn, const char *url);
struct composite_context *ldap_connect_send(struct ldap_connection *conn,
					    const char *url);

NTSTATUS ldap_rebind(struct ldap_connection *conn);
NTSTATUS ldap_bind_simple(struct ldap_connection *conn, 
			  const char *userdn, const char *password);
NTSTATUS ldap_bind_sasl(struct ldap_connection *conn, 
			struct cli_credentials *creds,
			struct loadparm_context *lp_ctx);
struct ldap_request *ldap_request_send(struct ldap_connection *conn,
				       struct ldap_message *msg);
NTSTATUS ldap_request_wait(struct ldap_request *req);
struct composite_context;
NTSTATUS ldap_connect_recv(struct composite_context *ctx);
NTSTATUS ldap_result_n(struct ldap_request *req, int n, struct ldap_message **msg);
NTSTATUS ldap_result_one(struct ldap_request *req, struct ldap_message **msg, int type);
NTSTATUS ldap_transaction(struct ldap_connection *conn, struct ldap_message *msg);
const char *ldap_errstr(struct ldap_connection *conn, 
			TALLOC_CTX *mem_ctx, 
			NTSTATUS status);
NTSTATUS ldap_check_response(struct ldap_connection *conn, struct ldap_Result *r);
void ldap_set_reconn_params(struct ldap_connection *conn, int max_retries);
int ildap_count_entries(struct ldap_connection *conn, struct ldap_message **res);
NTSTATUS ildap_search_bytree(struct ldap_connection *conn, const char *basedn, 
			     int scope, struct ldb_parse_tree *tree,
			     const char * const *attrs, bool attributesonly, 
			     struct ldb_control **control_req,
			     struct ldb_control ***control_res,
			     struct ldap_message ***results);
NTSTATUS ildap_search(struct ldap_connection *conn, const char *basedn, 
		      int scope, const char *expression, 
		      const char * const *attrs, bool attributesonly, 
		      struct ldb_control **control_req,
		      struct ldb_control ***control_res,
		      struct ldap_message ***results);



