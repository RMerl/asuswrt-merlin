/* 
   Unix SMB/CIFS implementation.
   Samba internal messaging functions
   Copyright (C) 2007 by Volker Lendecke
   
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
#include "messages.h"
#include "util_tdb.h"

#ifdef CLUSTER_SUPPORT

#include "ctdb.h"
#include "ctdb_private.h"
#include "ctdbd_conn.h"


struct messaging_ctdbd_context {
	struct ctdbd_connection *conn;
};

/*
 * This is a Samba3 hack/optimization. Routines like process_exists need to
 * talk to ctdbd, and they don't get handed a messaging context.
 */
static struct ctdbd_connection *global_ctdbd_connection;
static int global_ctdb_connection_pid;

struct ctdbd_connection *messaging_ctdbd_connection(void)
{
	if (global_ctdb_connection_pid == 0 &&
	    global_ctdbd_connection == NULL) {
		struct event_context *ev;
		struct messaging_context *msg;

		ev = event_context_init(NULL);
		if (!ev) {
			DEBUG(0,("event_context_init failed\n"));
		}

		msg = messaging_init(NULL, procid_self(), ev);
		if (!msg) {
			DEBUG(0,("messaging_init failed\n"));
			return NULL;
		}
	}

	if (global_ctdb_connection_pid != getpid()) {
		DEBUG(0,("messaging_ctdbd_connection():"
			 "valid for pid[%d] but it's [%d]\n",
			 global_ctdb_connection_pid, getpid()));
		smb_panic("messaging_ctdbd_connection() invalid process\n");
	}

	return global_ctdbd_connection;
}

static NTSTATUS messaging_ctdb_send(struct messaging_context *msg_ctx,
				    struct server_id pid, int msg_type,
				    const DATA_BLOB *data,
				    struct messaging_backend *backend)
{
	struct messaging_ctdbd_context *ctx = talloc_get_type_abort(
		backend->private_data, struct messaging_ctdbd_context);

	struct messaging_rec msg;

	msg.msg_version	= MESSAGE_VERSION;
	msg.msg_type	= msg_type;
	msg.dest	= pid;
	msg.src		= msg_ctx->id;
	msg.buf		= *data;

	return ctdbd_messaging_send(ctx->conn, pid.vnn, pid.pid, &msg);
}

static int messaging_ctdbd_destructor(struct messaging_ctdbd_context *ctx)
{
	/*
	 * The global connection just went away
	 */
	global_ctdb_connection_pid = 0;
	global_ctdbd_connection = NULL;
	return 0;
}

NTSTATUS messaging_ctdbd_init(struct messaging_context *msg_ctx,
			      TALLOC_CTX *mem_ctx,
			      struct messaging_backend **presult)
{
	struct messaging_backend *result;
	struct messaging_ctdbd_context *ctx;
	NTSTATUS status;

	if (!(result = TALLOC_P(mem_ctx, struct messaging_backend))) {
		DEBUG(0, ("talloc failed\n"));
		return NT_STATUS_NO_MEMORY;
	}

	if (!(ctx = TALLOC_P(result, struct messaging_ctdbd_context))) {
		DEBUG(0, ("talloc failed\n"));
		TALLOC_FREE(result);
		return NT_STATUS_NO_MEMORY;
	}

	status = ctdbd_messaging_connection(ctx, &ctx->conn);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("ctdbd_messaging_connection failed: %s\n",
			   nt_errstr(status)));
		TALLOC_FREE(result);
		return status;
	}

	status = ctdbd_register_msg_ctx(ctx->conn, msg_ctx);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("ctdbd_register_msg_ctx failed: %s\n",
			   nt_errstr(status)));
		TALLOC_FREE(result);
		return status;
	}

	global_ctdb_connection_pid = getpid();
	global_ctdbd_connection = ctx->conn;
	talloc_set_destructor(ctx, messaging_ctdbd_destructor);

	set_my_vnn(ctdbd_vnn(ctx->conn));

	result->send_fn = messaging_ctdb_send;
	result->private_data = (void *)ctx;

	*presult = result;
	return NT_STATUS_OK;
}

#else

NTSTATUS messaging_ctdbd_init(struct messaging_context *msg_ctx,
			      TALLOC_CTX *mem_ctx,
			      struct messaging_backend **presult)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

#endif
