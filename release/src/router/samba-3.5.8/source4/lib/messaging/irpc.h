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
#include "librpc/gen_ndr/server_id.h"

/*
  an incoming irpc message
*/
struct irpc_message {
	struct server_id from;
	void *private_data;
	struct irpc_header header;
	struct ndr_pull *ndr;
	bool defer_reply;
	struct messaging_context *msg_ctx;
	struct irpc_list *irpc;
	void *data;
	struct tevent_context *ev;
};

/* don't allow calls to take too long */
#define IRPC_CALL_TIMEOUT 10


/* the server function type */
typedef NTSTATUS (*irpc_function_t)(struct irpc_message *, void *r);

/* register a server function with the irpc messaging system */
#define IRPC_REGISTER(msg_ctx, pipename, funcname, function, private_data) \
   irpc_register(msg_ctx, &ndr_table_ ## pipename, \
                          NDR_ ## funcname, \
			  (irpc_function_t)function, private_data)

/* make a irpc call */
#define IRPC_CALL(msg_ctx, server_id, pipename, funcname, ptr, ctx) \
   irpc_call(msg_ctx, server_id, &ndr_table_ ## pipename, NDR_ ## funcname, ptr, ctx)

#define IRPC_CALL_SEND(msg_ctx, server_id, pipename, funcname, ptr, ctx) \
   irpc_call_send(msg_ctx, server_id, &ndr_table_ ## pipename, NDR_ ## funcname, ptr, ctx)


/*
  a pending irpc call
*/
struct irpc_request {
	struct messaging_context *msg_ctx;
	const struct ndr_interface_table *table;
	int callnum;
	int callid;
	void *r;
	NTSTATUS status;
	bool done;
	bool reject_free;
	TALLOC_CTX *mem_ctx;
	struct {
		void (*fn)(struct irpc_request *);
		void *private_data;
	} async;
};

struct loadparm_context;

typedef void (*msg_callback_t)(struct messaging_context *msg, void *private_data,
			       uint32_t msg_type, 
			       struct server_id server_id, DATA_BLOB *data);

NTSTATUS messaging_send(struct messaging_context *msg, struct server_id server, 
			uint32_t msg_type, DATA_BLOB *data);
NTSTATUS messaging_register(struct messaging_context *msg, void *private_data,
			    uint32_t msg_type, 
			    msg_callback_t fn);
NTSTATUS messaging_register_tmp(struct messaging_context *msg, void *private_data,
				msg_callback_t fn, uint32_t *msg_type);
struct messaging_context *messaging_init(TALLOC_CTX *mem_ctx, 
					 const char *dir,
					 struct server_id server_id, 
					 struct smb_iconv_convenience *iconv_convenience,
					 struct tevent_context *ev);
struct messaging_context *messaging_client_init(TALLOC_CTX *mem_ctx, 
					 const char *dir,
					 struct smb_iconv_convenience *iconv_convenience,
					 struct tevent_context *ev);
NTSTATUS messaging_send_ptr(struct messaging_context *msg, struct server_id server, 
			    uint32_t msg_type, void *ptr);
void messaging_deregister(struct messaging_context *msg, uint32_t msg_type, void *private_data);




NTSTATUS irpc_register(struct messaging_context *msg_ctx, 
		       const struct ndr_interface_table *table, 
		       int call, irpc_function_t fn, void *private_data);
struct irpc_request *irpc_call_send(struct messaging_context *msg_ctx, 
				    struct server_id server_id, 
				    const struct ndr_interface_table *table, 
				    int callnum, void *r, TALLOC_CTX *ctx);
NTSTATUS irpc_call_recv(struct irpc_request *irpc);
NTSTATUS irpc_call(struct messaging_context *msg_ctx, 
		   struct server_id server_id, 
		   const struct ndr_interface_table *table, 
		   int callnum, void *r, TALLOC_CTX *ctx);

NTSTATUS irpc_add_name(struct messaging_context *msg_ctx, const char *name);
struct server_id *irpc_servers_byname(struct messaging_context *msg_ctx, TALLOC_CTX *mem_ctx, const char *name);
void irpc_remove_name(struct messaging_context *msg_ctx, const char *name);
NTSTATUS irpc_send_reply(struct irpc_message *m, NTSTATUS status);
struct server_id messaging_get_server_id(struct messaging_context *msg_ctx);

#endif

