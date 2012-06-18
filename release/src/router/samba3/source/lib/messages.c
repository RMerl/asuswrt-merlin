/* 
   Unix SMB/CIFS implementation.
   Samba internal messaging functions
   Copyright (C) Andrew Tridgell 2000
   Copyright (C) 2001 by Martin Pool
   Copyright (C) 2002 by Jeremy Allison
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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

/* the locking database handle */
static TDB_CONTEXT *tdb;
static int received_signal;

/* change the message version with any incompatible changes in the protocol */
#define MESSAGE_VERSION 1

struct message_rec {
	int msg_version;
	int msg_type;
	struct process_id dest;
	struct process_id src;
	size_t len;
};

/* we have a linked list of dispatch handlers */
static struct dispatch_fns {
	struct dispatch_fns *next, *prev;
	int msg_type;
	void (*fn)(int msg_type, struct process_id pid, void *buf, size_t len,
		   void *private_data);
	void *private_data;
} *dispatch_fns;

/****************************************************************************
 Free global objects.
****************************************************************************/

void gfree_messages(void)
{
	struct dispatch_fns *dfn, *next;

	/* delete the dispatch_fns list */
	dfn = dispatch_fns;
	while( dfn ) {
		next = dfn->next;
		DLIST_REMOVE(dispatch_fns, dfn);
		SAFE_FREE(dfn);
		dfn = next;
	}
}

/****************************************************************************
 Notifications come in as signals.
****************************************************************************/

static void sig_usr1(void)
{
	received_signal = 1;
	sys_select_signal(SIGUSR1);
}

/****************************************************************************
 A useful function for testing the message system.
****************************************************************************/

static void ping_message(int msg_type, struct process_id src,
			 void *buf, size_t len, void *private_data)
{
	const char *msg = buf ? (const char *)buf : "none";

	DEBUG(1,("INFO: Received PING message from PID %s [%s]\n",
		 procid_str_static(&src), msg));
	message_send_pid(src, MSG_PONG, buf, len, True);
}

/****************************************************************************
 Initialise the messaging functions. 
****************************************************************************/

BOOL message_init(void)
{
	sec_init();

	if (tdb)
		return True;

	tdb = tdb_open_log(lock_path("messages.tdb"), 
		       0, TDB_CLEAR_IF_FIRST|TDB_DEFAULT, 
		       O_RDWR|O_CREAT,0600);

	if (!tdb) {
		DEBUG(0,("ERROR: Failed to initialise messages database\n"));
		return False;
	}

	/* Activate the per-hashchain freelist */
	tdb_set_max_dead(tdb, 5);

	CatchSignal(SIGUSR1, SIGNAL_CAST sig_usr1);

	message_register(MSG_PING, ping_message, NULL);

	/* Register some debugging related messages */

	register_msg_pool_usage();
	register_dmalloc_msgs();

	return True;
}

/*******************************************************************
 Form a static tdb key from a pid.
******************************************************************/

static TDB_DATA message_key_pid(struct process_id pid)
{
	static char key[20];
	TDB_DATA kbuf;

	slprintf(key, sizeof(key)-1, "PID/%s", procid_str_static(&pid));
	
	kbuf.dptr = (char *)key;
	kbuf.dsize = strlen(key)+1;
	return kbuf;
}

/****************************************************************************
 Notify a process that it has a message. If the process doesn't exist 
 then delete its record in the database.
****************************************************************************/

static NTSTATUS message_notify(struct process_id procid)
{
	pid_t pid = procid.pid;
	int ret;
	uid_t euid = geteuid();

	/*
	 * Doing kill with a non-positive pid causes messages to be
	 * sent to places we don't want.
	 */

	SMB_ASSERT(pid > 0);

	if (euid != 0) {
		/* If we're not root become so to send the message. */
		save_re_uid();
		set_effective_uid(0);
	}

	ret = kill(pid, SIGUSR1);

	if (euid != 0) {
		/* Go back to who we were. */
		int saved_errno = errno;
		restore_re_uid_fromroot();
		errno = saved_errno;
	}

	if (ret == -1) {
		if (errno == ESRCH) {
			DEBUG(2,("pid %d doesn't exist - deleting messages record\n",
				 (int)pid));
			tdb_delete(tdb, message_key_pid(procid));

			/*
			 * INVALID_HANDLE is the closest I can think of -- vl
			 */
			return NT_STATUS_INVALID_HANDLE;
		}

		DEBUG(2,("message to process %d failed - %s\n", (int)pid,
			 strerror(errno)));

		/*
		 * No call to map_nt_error_from_unix -- don't want to link in
		 * errormap.o into lots of utils.
		 */

		if (errno == EINVAL) return NT_STATUS_INVALID_PARAMETER;
		if (errno == EPERM)  return NT_STATUS_ACCESS_DENIED;
		return NT_STATUS_UNSUCCESSFUL;
	}

	return NT_STATUS_OK;
}

/****************************************************************************
 Send a message to a particular pid.
****************************************************************************/

static NTSTATUS message_send_pid_internal(struct process_id pid, int msg_type,
					  const void *buf, size_t len,
					  BOOL duplicates_allowed,
					  unsigned int timeout)
{
	TDB_DATA kbuf;
	TDB_DATA dbuf;
	TDB_DATA old_dbuf;
	struct message_rec rec;
	char *ptr;
	struct message_rec prec;

	/* NULL pointer means implicit length zero. */
	if (!buf) {
		SMB_ASSERT(len == 0);
	}

	/*
	 * Doing kill with a non-positive pid causes messages to be
	 * sent to places we don't want.
	 */

	SMB_ASSERT(procid_to_pid(&pid) > 0);

	rec.msg_version = MESSAGE_VERSION;
	rec.msg_type = msg_type;
	rec.dest = pid;
	rec.src = procid_self();
	rec.len = buf ? len : 0;

	kbuf = message_key_pid(pid);

	dbuf.dptr = (char *)SMB_MALLOC(len + sizeof(rec));
	if (!dbuf.dptr) {
		return NT_STATUS_NO_MEMORY;
	}

	memcpy(dbuf.dptr, &rec, sizeof(rec));
	if (len > 0 && buf)
		memcpy((void *)((char*)dbuf.dptr+sizeof(rec)), buf, len);

	dbuf.dsize = len + sizeof(rec);

	if (duplicates_allowed) {

		/* If duplicates are allowed we can just append the message and return. */

		/* lock the record for the destination */
		if (timeout) {
			if (tdb_chainlock_with_timeout(tdb, kbuf, timeout) == -1) {
				DEBUG(0,("message_send_pid_internal: failed to get "
					 "chainlock with timeout %ul.\n", timeout));
				SAFE_FREE(dbuf.dptr);
				return NT_STATUS_IO_TIMEOUT;
			}
		} else {
			if (tdb_chainlock(tdb, kbuf) == -1) {
				DEBUG(0,("message_send_pid_internal: failed to get "
					 "chainlock.\n"));
				SAFE_FREE(dbuf.dptr);
				return NT_STATUS_LOCK_NOT_GRANTED;
			}
		}	
		tdb_append(tdb, kbuf, dbuf);
		tdb_chainunlock(tdb, kbuf);

		SAFE_FREE(dbuf.dptr);
		errno = 0;                    /* paranoia */
		return message_notify(pid);
	}

	/* lock the record for the destination */
	if (timeout) {
		if (tdb_chainlock_with_timeout(tdb, kbuf, timeout) == -1) {
			DEBUG(0,("message_send_pid_internal: failed to get chainlock "
				 "with timeout %ul.\n", timeout));
			SAFE_FREE(dbuf.dptr);
			return NT_STATUS_IO_TIMEOUT;
		}
	} else {
		if (tdb_chainlock(tdb, kbuf) == -1) {
			DEBUG(0,("message_send_pid_internal: failed to get "
				 "chainlock.\n"));
			SAFE_FREE(dbuf.dptr);
			return NT_STATUS_LOCK_NOT_GRANTED;
		}
	}	

	old_dbuf = tdb_fetch(tdb, kbuf);

	if (!old_dbuf.dptr) {
		/* its a new record */

		tdb_store(tdb, kbuf, dbuf, TDB_REPLACE);
		tdb_chainunlock(tdb, kbuf);

		SAFE_FREE(dbuf.dptr);
		errno = 0;                    /* paranoia */
		return message_notify(pid);
	}

	/* Not a new record. Check for duplicates. */

	for(ptr = (char *)old_dbuf.dptr; ptr < old_dbuf.dptr + old_dbuf.dsize; ) {
		/*
		 * First check if the message header matches, then, if it's a non-zero
		 * sized message, check if the data matches. If so it's a duplicate and
		 * we can discard it. JRA.
		 */

		if (!memcmp(ptr, &rec, sizeof(rec))) {
			if (!len || (len && !memcmp( ptr + sizeof(rec), buf, len))) {
				tdb_chainunlock(tdb, kbuf);
				DEBUG(10,("message_send_pid_internal: discarding "
					  "duplicate message.\n"));
				SAFE_FREE(dbuf.dptr);
				SAFE_FREE(old_dbuf.dptr);
				return NT_STATUS_OK;
			}
		}
		memcpy(&prec, ptr, sizeof(prec));
		ptr += sizeof(rec) + prec.len;
	}

	/* we're adding to an existing entry */

	tdb_append(tdb, kbuf, dbuf);
	tdb_chainunlock(tdb, kbuf);

	SAFE_FREE(old_dbuf.dptr);
	SAFE_FREE(dbuf.dptr);

	errno = 0;                    /* paranoia */
	return message_notify(pid);
}

/****************************************************************************
 Send a message to a particular pid - no timeout.
****************************************************************************/

NTSTATUS message_send_pid(struct process_id pid, int msg_type, const void *buf,
			  size_t len, BOOL duplicates_allowed)
{
	return message_send_pid_internal(pid, msg_type, buf, len,
					 duplicates_allowed, 0);
}

/****************************************************************************
 Send a message to a particular pid, with timeout in seconds.
****************************************************************************/

NTSTATUS message_send_pid_with_timeout(struct process_id pid, int msg_type,
				       const void *buf, size_t len,
				       BOOL duplicates_allowed, unsigned int timeout)
{
	return message_send_pid_internal(pid, msg_type, buf, len, duplicates_allowed,
					 timeout);
}

/****************************************************************************
 Count the messages pending for a particular pid. Expensive....
****************************************************************************/

unsigned int messages_pending_for_pid(struct process_id pid)
{
	TDB_DATA kbuf;
	TDB_DATA dbuf;
	char *buf;
	unsigned int message_count = 0;

	kbuf = message_key_pid(pid);

	dbuf = tdb_fetch(tdb, kbuf);
	if (dbuf.dptr == NULL || dbuf.dsize == 0) {
		SAFE_FREE(dbuf.dptr);
		return 0;
	}

	for (buf = dbuf.dptr; dbuf.dsize > sizeof(struct message_rec);) {
		struct message_rec rec;
		memcpy(&rec, buf, sizeof(rec));
		buf += (sizeof(rec) + rec.len);
		dbuf.dsize -= (sizeof(rec) + rec.len);
		message_count++;
	}

	SAFE_FREE(dbuf.dptr);
	return message_count;
}

/****************************************************************************
 Retrieve all messages for the current process.
****************************************************************************/

static BOOL retrieve_all_messages(char **msgs_buf, size_t *total_len)
{
	TDB_DATA kbuf;
	TDB_DATA dbuf;
	TDB_DATA null_dbuf;

	ZERO_STRUCT(null_dbuf);

	*msgs_buf = NULL;
	*total_len = 0;

	kbuf = message_key_pid(pid_to_procid(sys_getpid()));

	if (tdb_chainlock(tdb, kbuf) == -1)
		return False;

	dbuf = tdb_fetch(tdb, kbuf);
	/*
	 * Replace with an empty record to keep the allocated
	 * space in the tdb.
	 */
	tdb_store(tdb, kbuf, null_dbuf, TDB_REPLACE);
	tdb_chainunlock(tdb, kbuf);

	if (dbuf.dptr == NULL || dbuf.dsize == 0) {
		SAFE_FREE(dbuf.dptr);
		return False;
	}

	*msgs_buf = dbuf.dptr;
	*total_len = dbuf.dsize;

	return True;
}

/****************************************************************************
 Parse out the next message for the current process.
****************************************************************************/

static BOOL message_recv(char *msgs_buf, size_t total_len, int *msg_type,
			 struct process_id *src, char **buf, size_t *len)
{
	struct message_rec rec;
	char *ret_buf = *buf;

	*buf = NULL;
	*len = 0;

	if (total_len - (ret_buf - msgs_buf) < sizeof(rec))
		return False;

	memcpy(&rec, ret_buf, sizeof(rec));
	ret_buf += sizeof(rec);

	if (rec.msg_version != MESSAGE_VERSION) {
		DEBUG(0,("message version %d received (expected %d)\n", rec.msg_version, MESSAGE_VERSION));
		return False;
	}

	if (rec.len > 0) {
		if (total_len - (ret_buf - msgs_buf) < rec.len)
			return False;
	}

	*len = rec.len;
	*msg_type = rec.msg_type;
	*src = rec.src;
	*buf = ret_buf;

	return True;
}

/****************************************************************************
 Receive and dispatch any messages pending for this process.
 JRA changed Dec 13 2006. Only one message handler now permitted per type.
 *NOTE*: Dispatch functions must be able to cope with incoming
 messages on an *odd* byte boundary.
****************************************************************************/

void message_dispatch(void)
{
	int msg_type;
	struct process_id src;
	char *buf;
	char *msgs_buf;
	size_t len, total_len;
	int n_handled;

	if (!received_signal)
		return;

	DEBUG(10,("message_dispatch: received_signal = %d\n", received_signal));

	received_signal = 0;

	if (!retrieve_all_messages(&msgs_buf, &total_len))
		return;

	for (buf = msgs_buf; message_recv(msgs_buf, total_len, &msg_type, &src, &buf, &len); buf += len) {
		struct dispatch_fns *dfn;

		DEBUG(10,("message_dispatch: received msg_type=%d "
			  "src_pid=%u\n", msg_type,
			  (unsigned int) procid_to_pid(&src)));

		n_handled = 0;
		for (dfn = dispatch_fns; dfn; dfn = dfn->next) {
			if (dfn->msg_type == msg_type) {
				DEBUG(10,("message_dispatch: processing message of type %d.\n", msg_type));
				dfn->fn(msg_type, src,
					len ? (void *)buf : NULL, len,
					dfn->private_data);
				n_handled++;
				break;
			}
		}
		if (!n_handled) {
			DEBUG(5,("message_dispatch: warning: no handler registed for "
				 "msg_type %d in pid %u\n",
				 msg_type, (unsigned int)sys_getpid()));
		}
	}
	SAFE_FREE(msgs_buf);
}

/****************************************************************************
 Register/replace a dispatch function for a particular message type.
 JRA changed Dec 13 2006. Only one message handler now permitted per type.
 *NOTE*: Dispatch functions must be able to cope with incoming
 messages on an *odd* byte boundary.
****************************************************************************/

void message_register(int msg_type, 
		      void (*fn)(int msg_type, struct process_id pid,
				 void *buf, size_t len,
				 void *private_data),
		      void *private_data)
{
	struct dispatch_fns *dfn;

	for (dfn = dispatch_fns; dfn; dfn = dfn->next) {
		if (dfn->msg_type == msg_type) {
			dfn->fn = fn;
			return;
		}
	}

	dfn = SMB_MALLOC_P(struct dispatch_fns);

	if (dfn != NULL) {

		ZERO_STRUCTPN(dfn);

		dfn->msg_type = msg_type;
		dfn->fn = fn;
		dfn->private_data = private_data;

		DLIST_ADD(dispatch_fns, dfn);
	}
	else {
	
		DEBUG(0,("message_register: Not enough memory. malloc failed!\n"));
	}
}

/****************************************************************************
 De-register the function for a particular message type.
****************************************************************************/

void message_deregister(int msg_type)
{
	struct dispatch_fns *dfn, *next;

	for (dfn = dispatch_fns; dfn; dfn = next) {
		next = dfn->next;
		if (dfn->msg_type == msg_type) {
			DLIST_REMOVE(dispatch_fns, dfn);
			SAFE_FREE(dfn);
			return;
		}
	}	
}

struct msg_all {
	int msg_type;
	uint32 msg_flag;
	const void *buf;
	size_t len;
	BOOL duplicates;
	int n_sent;
};

/****************************************************************************
 Send one of the messages for the broadcast.
****************************************************************************/

static int traverse_fn(TDB_CONTEXT *the_tdb, TDB_DATA kbuf, TDB_DATA dbuf, void *state)
{
	struct connections_data crec;
	struct msg_all *msg_all = (struct msg_all *)state;
	NTSTATUS status;

	if (dbuf.dsize != sizeof(crec))
		return 0;

	memcpy(&crec, dbuf.dptr, sizeof(crec));

	if (crec.cnum != -1)
		return 0;

	/* Don't send if the receiver hasn't registered an interest. */

	if(!(crec.bcast_msg_flags & msg_all->msg_flag))
		return 0;

	/* If the msg send fails because the pid was not found (i.e. smbd died), 
	 * the msg has already been deleted from the messages.tdb.*/

	status = message_send_pid(crec.pid, msg_all->msg_type,
				  msg_all->buf, msg_all->len,
				  msg_all->duplicates);

	if (NT_STATUS_EQUAL(status, NT_STATUS_INVALID_HANDLE)) {
		
		/* If the pid was not found delete the entry from connections.tdb */

		DEBUG(2,("pid %s doesn't exist - deleting connections %d [%s]\n",
			 procid_str_static(&crec.pid), crec.cnum, crec.servicename));
		tdb_delete(the_tdb, kbuf);
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
BOOL message_send_all(TDB_CONTEXT *conn_tdb, int msg_type,
		      const void *buf, size_t len,
		      BOOL duplicates_allowed,
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
	else
		return False;

	msg_all.buf = buf;
	msg_all.len = len;
	msg_all.duplicates = duplicates_allowed;
	msg_all.n_sent = 0;

	tdb_traverse(conn_tdb, traverse_fn, &msg_all);
	if (n_sent)
		*n_sent = msg_all.n_sent;
	return True;
}

/*
 * Block and unblock receiving of messages. Allows removal of race conditions
 * when doing a fork and changing message disposition.
 */

void message_block(void)
{
	BlockSignals(True, SIGUSR1);
}

void message_unblock(void)
{
	BlockSignals(False, SIGUSR1);
}

/*
 * Samba4 API wrapper around the Samba3 implementation. Yes, I know, we could
 * import the whole Samba4 thing, but I want notify.c from Samba4 in first.
 */

struct messaging_callback {
	struct messaging_callback *prev, *next;
	uint32 msg_type;
	void (*fn)(struct messaging_context *msg, void *private_data, 
		   uint32_t msg_type, 
		   struct server_id server_id, DATA_BLOB *data);
	void *private_data;
};

struct messaging_context {
	struct server_id id;
	struct messaging_callback *callbacks;
};

static int messaging_context_destructor(struct messaging_context *ctx)
{
	struct messaging_callback *cb;

	for (cb = ctx->callbacks; cb; cb = cb->next) {
		/*
		 * We unconditionally remove all instances of our callback
		 * from the tdb basis.
		 */
		message_deregister(cb->msg_type);
	}
	return 0;
}

struct messaging_context *messaging_init(TALLOC_CTX *mem_ctx, 
					 struct server_id server_id, 
					 struct event_context *ev)
{
	struct messaging_context *ctx;

	if (!(ctx = TALLOC_ZERO_P(mem_ctx, struct messaging_context))) {
		return NULL;
	}

	ctx->id = server_id;
	talloc_set_destructor(ctx, messaging_context_destructor);
	return ctx;
}

static void messaging_callback(int msg_type, struct process_id pid,
			       void *buf, size_t len, void *private_data)
{
	struct messaging_context *ctx =	talloc_get_type_abort(
		private_data, struct messaging_context);
	struct messaging_callback *cb, *next;

	for (cb = ctx->callbacks; cb; cb = next) {
		/*
		 * Allow a callback to remove itself
		 */
		next = cb->next;

		if (msg_type == cb->msg_type) {
			DATA_BLOB blob;
			struct server_id id;

			blob.data = (uint8 *)buf;
			blob.length = len;
			id.id = pid;

			cb->fn(ctx, cb->private_data, msg_type, id, &blob);
		}
	}
}

/*
 * Register a dispatch function for a particular message type. Allow multiple
 * registrants
*/
NTSTATUS messaging_register(struct messaging_context *ctx, void *private_data,
			    uint32_t msg_type,
			    void (*fn)(struct messaging_context *msg,
				       void *private_data, 
				       uint32_t msg_type, 
				       struct server_id server_id,
				       DATA_BLOB *data))
{
	struct messaging_callback *cb;

	if (!(cb = talloc(ctx, struct messaging_callback))) {
		return NT_STATUS_NO_MEMORY;
	}

	cb->msg_type = msg_type;
	cb->fn = fn;
	cb->private_data = private_data;

	DLIST_ADD(ctx->callbacks, cb);
	message_register(msg_type, messaging_callback, ctx);
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
			DLIST_REMOVE(ctx->callbacks, cb);
			TALLOC_FREE(cb);
		}
	}
}

/*
  Send a message to a particular server
*/
NTSTATUS messaging_send(struct messaging_context *msg,
			struct server_id server, 
			uint32_t msg_type, DATA_BLOB *data)
{
	return message_send_pid_internal(server.id, msg_type, data->data,
					 data->length, True, 0);
}

/** @} **/
