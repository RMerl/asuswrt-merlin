/* 
   Unix SMB/CIFS implementation.

   Samba internal rpc code - header

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

#ifndef IRPC_H
#define IRPC_H

#include "lib/messaging/messaging.h"
#include "librpc/gen_ndr/irpc.h"

/*
  an incoming irpc message
*/
struct irpc_message {
	struct server_id from;
	void *private_data;
	struct irpc_header header;
	struct ndr_pull *ndr;
	bool defer_reply;
	bool no_reply;
	struct messaging_context *msg_ctx;
	struct irpc_list *irpc;
	void *data;
	struct tevent_context *ev;
};

/* don't allow calls to take too long */
#define IRPC_CALL_TIMEOUT	10
/* wait for the calls as long as it takes */
#define IRPC_CALL_TIMEOUT_INF	0


/* the server function type */
typedef NTSTATUS (*irpc_function_t)(struct irpc_message *, void *r);

/* register a server function with the irpc messaging system */
#define IRPC_REGISTER(msg_ctx, pipename, funcname, function, private_data) \
   irpc_register(msg_ctx, &ndr_table_ ## pipename, \
                          NDR_ ## funcname, \
			  (irpc_function_t)function, private_data)

struct ndr_interface_table;

NTSTATUS irpc_register(struct messaging_context *msg_ctx, 
		       const struct ndr_interface_table *table, 
		       int call, irpc_function_t fn, void *private_data);

struct dcerpc_binding_handle *irpc_binding_handle(TALLOC_CTX *mem_ctx,
					struct messaging_context *msg_ctx,
					struct server_id server_id,
					const struct ndr_interface_table *table);
struct dcerpc_binding_handle *irpc_binding_handle_by_name(TALLOC_CTX *mem_ctx,
					struct messaging_context *msg_ctx,
					const char *dest_task,
					const struct ndr_interface_table *table);
void irpc_binding_handle_add_security_token(struct dcerpc_binding_handle *h,
					    struct security_token *token);

NTSTATUS irpc_add_name(struct messaging_context *msg_ctx, const char *name);
struct server_id *irpc_servers_byname(struct messaging_context *msg_ctx, TALLOC_CTX *mem_ctx, const char *name);
void irpc_remove_name(struct messaging_context *msg_ctx, const char *name);
NTSTATUS irpc_send_reply(struct irpc_message *m, NTSTATUS status);

#endif

