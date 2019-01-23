/*
   Unix SMB/CIFS implementation.

   a async CLDAP library

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

#include "../libcli/netlogon/netlogon.h"

struct ldap_message;
struct tsocket_address;
struct cldap_socket;

struct cldap_incoming {
	int recv_errno;
	uint8_t *buf;
	size_t len;
	struct tsocket_address *src;
	struct ldap_message *ldap_msg;
};

/*
 a general cldap search request
*/
struct cldap_search {
	struct {
		const char *dest_address;
		uint16_t dest_port;
		const char *filter;
		const char * const *attributes;
		int timeout;
		int retries;
	} in;
	struct {
		struct ldap_SearchResEntry *response;
		struct ldap_Result         *result;
	} out;
};

NTSTATUS cldap_socket_init(TALLOC_CTX *mem_ctx,
			   struct tevent_context *ev,
			   const struct tsocket_address *local_addr,
			   const struct tsocket_address *remote_addr,
			   struct cldap_socket **_cldap);

NTSTATUS cldap_set_incoming_handler(struct cldap_socket *cldap,
				    void (*handler)(struct cldap_socket *,
						    void *private_data,
						    struct cldap_incoming *),
				    void *private_data);
struct tevent_req *cldap_search_send(TALLOC_CTX *mem_ctx,
				     struct cldap_socket *cldap,
				     const struct cldap_search *io);
NTSTATUS cldap_search_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
			   struct cldap_search *io);
NTSTATUS cldap_search(struct cldap_socket *cldap, TALLOC_CTX *mem_ctx,
		      struct cldap_search *io);

/*
  a general cldap reply
*/
struct cldap_reply {
	uint32_t messageid;
	struct tsocket_address *dest;
	struct ldap_SearchResEntry *response;
	struct ldap_Result         *result;
};

NTSTATUS cldap_reply_send(struct cldap_socket *cldap, struct cldap_reply *io);

NTSTATUS cldap_empty_reply(struct cldap_socket *cldap,
			   uint32_t message_id,
			   struct tsocket_address *dst);
NTSTATUS cldap_error_reply(struct cldap_socket *cldap,
			   uint32_t message_id,
			   struct tsocket_address *dst,
			   int resultcode,
			   const char *errormessage);

/*
  a netlogon cldap request  
*/
struct cldap_netlogon {
	struct {
		const char *dest_address;
		uint16_t dest_port;
		const char *realm;
		const char *host;
		const char *user;
		const char *domain_guid;
		const char *domain_sid;
		int acct_control;
		uint32_t version;
		bool map_response;
	} in;
	struct {
		struct netlogon_samlogon_response netlogon;
	} out;
};

struct tevent_req *cldap_netlogon_send(TALLOC_CTX *mem_ctx,
				       struct cldap_socket *cldap,
				       const struct cldap_netlogon *io);
NTSTATUS cldap_netlogon_recv(struct tevent_req *req,
			     TALLOC_CTX *mem_ctx,
			     struct cldap_netlogon *io);
NTSTATUS cldap_netlogon(struct cldap_socket *cldap,
			TALLOC_CTX *mem_ctx,
			struct cldap_netlogon *io);

NTSTATUS cldap_netlogon_reply(struct cldap_socket *cldap,
			      uint32_t message_id,
			      struct tsocket_address *dst,
			      uint32_t version,
			      struct netlogon_samlogon_response *netlogon);

