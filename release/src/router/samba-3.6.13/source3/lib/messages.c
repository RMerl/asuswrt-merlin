/* 
   Unix SMB/CIFS implementation.
   Samba internal messaging functions
   Copyright (C) Andrew Tridgell 2000
   Copyright (C) 2001 by Martin Pool
   Copyright (C) 2002 by Jeremy Allison
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

/**
  @defgroup messages Internal messaging framework
  @{
  @file messages.c
  
  @brief  Module for internal messaging between Samba daemons. 

   The idea is that if a part of Samba wants to do communication with
   another Samba process then it will do a message_register() of a
   dispatch function, and use message_send_pid() to send messages to
   that process.

   The dispatch function is given the pid of the sender, and it can
   use that to reply by message_send_pid().  See ping_message() for a
   simple example.

   @caution Dispatch functions must be able to cope with incoming
   messages on an *odd* byte boundary.

   This system doesn't have any inherent size limitations but is not
   very efficient for large messages or when messages are sent in very
   quick succession.

*/

#include "includes.h"
#include "dbwrap.h"
#include "serverid.h"
#include "messages.h"

struct messaging_callback {
	struct messaging_callback *prev, *next;
	uint32 msg_type;
	void (*fn)(struct messaging_context *msg, void *private_data, 
		   uint32_t msg_type, 
		   struct server_id server_id, DATA_BLOB *data);
	void *private_data;
};

/****************************************************************************
 A useful function for testing the message system.
****************************************************************************/

static void ping_message(struct messaging_context *msg_ctx,
			 void *private_data,
			 uint32_t msg_type,
			 struct server_id src,
			 DATA_BLOB *data)
{
	const char *msg = data->data ? (const char *)data->data : "none";

	DEBUG(1,("INFO: Received PING message from PID %s [%s]\n",
		 procid_str_static(&src), msg));
	messaging_send(msg_ctx, src, MSG_PONG, data);
}

/****************************************************************************
 Register/replace a dispatch function for a particular message type.
 JRA changed Dec 13 2006. Only one message handler now permitted per type.
 *NOTE*: Dispatch functions must be able to cope with incoming
 messages on an *odd* byte boundary.
****************************************************************************/

struct msg_all {
	struct messaging_context *msg_ctx;
	int msg_type;
	uint32 msg_flag;
	const void *buf;
	size_t len;
	int n_sent;
};

/****************************************************************************
 Send one of the messages for the broadcast.
****************************************************************************/

static int traverse_fn(struct db_record *rec, const struct server_id *id,
		       uint32_t msg_flags, void *state)
{
	struct msg_all *msg_all = (struct msg_all *)state;
	NTSTATUS status;

	/* Don't send if the receiver hasn't registered an interest. */

	if((msg_flags & msg_all->msg_flag) == 0) {
		return 0;
	}

	/* If the msg send fails because the pid was not found (i.e. smbd died), 
	 * the msg has already been deleted from the messages.tdb.*/

	status = messaging_send_buf(msg_all->msg_ctx, *id, msg_all->msg_type,
				    (uint8 *)msg_all->buf, msg_all->len);

	if (NT_STATUS_EQUAL(status, NT_STATUS_INVALID_HANDLE)) {
		
		/* If the pid was not found delete the entry from connections.tdb */

		DEBUG(2, ("pid %s doesn't exist\n", procid_str_static(id)));

		rec->delete_rec(rec);
	}
	msg_all->n_sent++;
	return 0;
}

/**
 * Send a message to all smbd processes.
 *
 * It isn't very efficient, but should be OK for the sorts of
 * applications that use it. When we need efficient broadcast we can add
 * it.
 *
 * @param n_sent Set to the number of messages sent.  This should be
 * equal to the number of processes, but be careful for races.
 *
 * @retval True for success.
 **/
bool message_send_all(struct messaging_context *msg_ctx,
		      int msg_type,
		      const void *buf, size_t len,
		      int *n_sent)
{
	struct msg_all msg_all;

	msg_all.msg_type = msg_type;
	if (msg_type < 1000)
		msg_all.msg_flag = FLAG_MSG_GENERAL;
	else if (msg_type > 1000 && msg_type < 2000)
		msg_all.msg_flag = FLAG_MSG_NMBD;
	else if (msg_type > 2000 && msg_type < 2100)
		msg_all.msg_flag = FLAG_MSG_PRINT_NOTIFY;
	else if (msg_type > 2100 && msg_type < 3000)
		msg_all.msg_flag = FLAG_MSG_PRINT_GENERAL;
	else if (msg_type > 3000 && msg_type < 4000)
		msg_all.msg_flag = FLAG_MSG_SMBD;
	else if (msg_type > 4000 && msg_type < 5000)
		msg_all.msg_flag = FLAG_MSG_DBWRAP;
	else
		return False;

	msg_all.buf = buf;
	msg_all.len = len;
	msg_all.n_sent = 0;
	msg_all.msg_ctx = msg_ctx;

	serverid_traverse(traverse_fn, &msg_all);
	if (n_sent)
		*n_sent = msg_all.n_sent;
	return True;
}

struct event_context *messaging_event_context(struct messaging_context *msg_ctx)
{
	return msg_ctx->event_ctx;
}

struct messaging_context *messaging_init(TALLOC_CTX *mem_ctx, 
					 struct server_id server_id, 
					 struct event_context *ev)
{
	struct messaging_context *ctx;
	NTSTATUS status;

	if (!(ctx = TALLOC_ZERO_P(mem_ctx, struct messaging_context))) {
		return NULL;
	}

	ctx->id = server_id;
	ctx->event_ctx = ev;

	status = messaging_tdb_init(ctx, ctx, &ctx->local);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(2, ("messaging_tdb_init failed: %s\n",
			  nt_errstr(status)));
		TALLOC_FREE(ctx);
		return NULL;
	}

#ifdef CLUSTER_SUPPORT
	if (lp_clustering()) {
		status = messaging_ctdbd_init(ctx, ctx, &ctx->remote);

		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(2, ("messaging_ctdb_init failed: %s\n",
				  nt_errstr(status)));
			TALLOC_FREE(ctx);
			return NULL;
		}
	}
	ctx->id.vnn = get_my_vnn();
#endif

	messaging_register(ctx, NULL, MSG_PING, ping_message);

	/* Register some debugging related messages */

	register_msg_pool_usage(ctx);
	register_dmalloc_msgs(ctx);
	debug_register_msgs(ctx);

	return ctx;
}

struct server_id messaging_server_id(const struct messaging_context *msg_ctx)
{
	return msg_ctx->id;
}

/*
 * re-init after a fork
 */
NTSTATUS messaging_reinit(struct messaging_context *msg_ctx,
			  struct server_id id)
{
	NTSTATUS status;

	TALLOC_FREE(msg_ctx->local);

	msg_ctx->id = id;

	status = messaging_tdb_init(msg_ctx, msg_ctx, &msg_ctx->local);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("messaging_tdb_init failed: %s\n",
			  nt_errstr(status)));
		return status;
	}

#ifdef CLUSTER_SUPPORT
	TALLOC_FREE(msg_ctx->remote);

	if (lp_clustering()) {
		status = messaging_ctdbd_init(msg_ctx, msg_ctx,
					      &msg_ctx->remote);

		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(1, ("messaging_ctdb_init failed: %s\n",
				  nt_errstr(status)));
			return status;
		}
	}

#endif

	return NT_STATUS_OK;
}


/*
 * Register a dispatch function for a particular message type. Allow multiple
 * registrants
*/
NTSTATUS messaging_register(struct messaging_context *msg_ctx,
			    void *private_data,
			    uint32_t msg_type,
			    void (*fn)(struct messaging_context *msg,
				       void *private_data, 
				       uint32_t msg_type, 
				       struct server_id server_id,
				       DATA_BLOB *data))
{
	struct messaging_callback *cb;

	/*
	 * Only one callback per type
	 */

	for (cb = msg_ctx->callbacks; cb != NULL; cb = cb->next) {
		/* we allow a second registration of the same message
		   type if it has a different private pointer. This is
		   needed in, for example, the internal notify code,
		   which creates a new notify context for each tree
		   connect, and expects to receive messages to each of
		   them. */
		if (cb->msg_type == msg_type && private_data == cb->private_data) {
			DEBUG(5,("Overriding messaging pointer for type %u - private_data=%p\n",
				  (unsigned)msg_type, private_data));
			cb->fn = fn;
			cb->private_data = private_data;
			return NT_STATUS_OK;
		}
	}

	if (!(cb = talloc(msg_ctx, struct messaging_callback))) {
		return NT_STATUS_NO_MEMORY;
	}

	cb->msg_type = msg_type;
	cb->fn = fn;
	cb->private_data = private_data;

	DLIST_ADD(msg_ctx->callbacks, cb);
	return NT_STATUS_OK;
}

/*
  De-register the function for a particular message type.
*/
void messaging_deregister(struct messaging_context *ctx, uint32_t msg_type,
			  void *private_data)
{
	struct messaging_callback *cb, *next;

	for (cb = ctx->callbacks; cb; cb = next) {
		next = cb->next;
		if ((cb->msg_type == msg_type)
		    && (cb->private_data == private_data)) {
			DEBUG(5,("Deregistering messaging pointer for type %u - private_data=%p\n",
				  (unsigned)msg_type, private_data));
			DLIST_REMOVE(ctx->callbacks, cb);
			TALLOC_FREE(cb);
		}
	}
}

/*
  Send a message to a particular server
*/
NTSTATUS messaging_send(struct messaging_context *msg_ctx,
			struct server_id server, uint32_t msg_type,
			const DATA_BLOB *data)
{
#ifdef CLUSTER_SUPPORT
	if (!procid_is_local(&server)) {
		return msg_ctx->remote->send_fn(msg_ctx, server,
						msg_type, data,
						msg_ctx->remote);
	}
#endif
	return msg_ctx->local->send_fn(msg_ctx, server, msg_type, data,
				       msg_ctx->local);
}

NTSTATUS messaging_send_buf(struct messaging_context *msg_ctx,
			    struct server_id server, uint32_t msg_type,
			    const uint8 *buf, size_t len)
{
	DATA_BLOB blob = data_blob_const(buf, len);
	return messaging_send(msg_ctx, server, msg_type, &blob);
}

/*
  Dispatch one messaging_rec
*/
void messaging_dispatch_rec(struct messaging_context *msg_ctx,
			    struct messaging_rec *rec)
{
	struct messaging_callback *cb, *next;

	for (cb = msg_ctx->callbacks; cb != NULL; cb = next) {
		next = cb->next;
		if (cb->msg_type == rec->msg_type) {
			cb->fn(msg_ctx, cb->private_data, rec->msg_type,
			       rec->src, &rec->buf);
			/* we continue looking for matching messages
			   after finding one. This matters for
			   subsystems like the internal notify code
			   which register more than one handler for
			   the same message type */
		}
	}
	return;
}

/** @} **/
