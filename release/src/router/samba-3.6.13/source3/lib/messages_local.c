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
#include "system/filesys.h"
#include "messages.h"
#include "lib/util/tdb_wrap.h"

struct messaging_tdb_context {
	struct messaging_context *msg_ctx;
	struct tdb_wrap *tdb;
	struct tevent_signal *se;
	int received_messages;
};

static NTSTATUS messaging_tdb_send(struct messaging_context *msg_ctx,
				   struct server_id pid, int msg_type,
				   const DATA_BLOB *data,
				   struct messaging_backend *backend);
static void message_dispatch(struct messaging_context *msg_ctx);

static void messaging_tdb_signal_handler(struct tevent_context *ev_ctx,
					 struct tevent_signal *se,
					 int signum, int count,
					 void *_info, void *private_data)
{
	struct messaging_tdb_context *ctx = talloc_get_type(private_data,
					    struct messaging_tdb_context);

	ctx->received_messages++;

	DEBUG(10, ("messaging_tdb_signal_handler: sig[%d] count[%d] msgs[%d]\n",
		   signum, count, ctx->received_messages));

	message_dispatch(ctx->msg_ctx);
}

/****************************************************************************
 Initialise the messaging functions. 
****************************************************************************/

NTSTATUS messaging_tdb_init(struct messaging_context *msg_ctx,
			    TALLOC_CTX *mem_ctx,
			    struct messaging_backend **presult)
{
	struct messaging_backend *result;
	struct messaging_tdb_context *ctx;

	if (!(result = TALLOC_P(mem_ctx, struct messaging_backend))) {
		DEBUG(0, ("talloc failed\n"));
		return NT_STATUS_NO_MEMORY;
	}

	ctx = TALLOC_ZERO_P(result, struct messaging_tdb_context);
	if (!ctx) {
		DEBUG(0, ("talloc failed\n"));
		TALLOC_FREE(result);
		return NT_STATUS_NO_MEMORY;
	}
	result->private_data = ctx;
	result->send_fn = messaging_tdb_send;

	ctx->msg_ctx = msg_ctx;

	ctx->tdb = tdb_wrap_open(ctx, lock_path("messages.tdb"), 0,
				 TDB_CLEAR_IF_FIRST|TDB_DEFAULT|TDB_VOLATILE|TDB_INCOMPATIBLE_HASH,
				 O_RDWR|O_CREAT,0600);

	if (!ctx->tdb) {
		NTSTATUS status = map_nt_error_from_unix(errno);
		DEBUG(2, ("ERROR: Failed to initialise messages database: "
			  "%s\n", strerror(errno)));
		TALLOC_FREE(result);
		return status;
	}

	ctx->se = tevent_add_signal(msg_ctx->event_ctx,
				    ctx,
				    SIGUSR1, 0,
				    messaging_tdb_signal_handler,
				    ctx);
	if (!ctx->se) {
		NTSTATUS status = map_nt_error_from_unix(errno);
		DEBUG(0, ("ERROR: Failed to initialise messages signal handler: "
			  "%s\n", strerror(errno)));
		TALLOC_FREE(result);
		return status;
	}

	sec_init();

	*presult = result;
	return NT_STATUS_OK;
}

bool messaging_tdb_parent_init(TALLOC_CTX *mem_ctx)
{
	struct tdb_wrap *db;

	/*
	 * Open the tdb in the parent process (smbd) so that our
	 * CLEAR_IF_FIRST optimization in tdb_reopen_all can properly
	 * work.
	 */

	db = tdb_wrap_open(mem_ctx, lock_path("messages.tdb"), 0,
			   TDB_CLEAR_IF_FIRST|TDB_DEFAULT|TDB_VOLATILE|TDB_INCOMPATIBLE_HASH,
			   O_RDWR|O_CREAT,0600);
	if (db == NULL) {
		DEBUG(1, ("could not open messaging.tdb: %s\n",
			  strerror(errno)));
		return false;
	}
	return true;
}

/*******************************************************************
 Form a static tdb key from a pid.
******************************************************************/

static TDB_DATA message_key_pid(TALLOC_CTX *mem_ctx, struct server_id pid)
{
	char *key;
	TDB_DATA kbuf;

	key = talloc_asprintf(talloc_tos(), "PID/%s", procid_str_static(&pid));

	SMB_ASSERT(key != NULL);

	kbuf.dptr = (uint8 *)key;
	kbuf.dsize = strlen(key)+1;
	return kbuf;
}

/*
  Fetch the messaging array for a process
 */

static NTSTATUS messaging_tdb_fetch(TDB_CONTEXT *msg_tdb,
				    TDB_DATA key,
				    TALLOC_CTX *mem_ctx,
				    struct messaging_array **presult)
{
	struct messaging_array *result;
	TDB_DATA data;
	DATA_BLOB blob;
	enum ndr_err_code ndr_err;

	if (!(result = TALLOC_ZERO_P(mem_ctx, struct messaging_array))) {
		return NT_STATUS_NO_MEMORY;
	}

	data = tdb_fetch(msg_tdb, key);

	if (data.dptr == NULL) {
		*presult = result;
		return NT_STATUS_OK;
	}

	blob = data_blob_const(data.dptr, data.dsize);

	ndr_err = ndr_pull_struct_blob(
		&blob, result, result,
		(ndr_pull_flags_fn_t)ndr_pull_messaging_array);

	SAFE_FREE(data.dptr);

	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		TALLOC_FREE(result);
		return ndr_map_error2ntstatus(ndr_err);
	}

	if (DEBUGLEVEL >= 10) {
		DEBUG(10, ("messaging_tdb_fetch:\n"));
		NDR_PRINT_DEBUG(messaging_array, result);
	}

	*presult = result;
	return NT_STATUS_OK;
}

/*
  Store a messaging array for a pid
*/

static NTSTATUS messaging_tdb_store(TDB_CONTEXT *msg_tdb,
				    TDB_DATA key,
				    struct messaging_array *array)
{
	TDB_DATA data;
	DATA_BLOB blob;
	enum ndr_err_code ndr_err;
	TALLOC_CTX *mem_ctx;
	int ret;

	if (array->num_messages == 0) {
		tdb_delete(msg_tdb, key);
		return NT_STATUS_OK;
	}

	if (!(mem_ctx = talloc_new(array))) {
		return NT_STATUS_NO_MEMORY;
	}

	ndr_err = ndr_push_struct_blob(&blob, mem_ctx, array,
		(ndr_push_flags_fn_t)ndr_push_messaging_array);

	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(mem_ctx);
		return ndr_map_error2ntstatus(ndr_err);
	}

	if (DEBUGLEVEL >= 10) {
		DEBUG(10, ("messaging_tdb_store:\n"));
		NDR_PRINT_DEBUG(messaging_array, array);
	}

	data.dptr = blob.data;
	data.dsize = blob.length;

	ret = tdb_store(msg_tdb, key, data, TDB_REPLACE);
	TALLOC_FREE(mem_ctx);

	return (ret == 0) ? NT_STATUS_OK : NT_STATUS_INTERNAL_DB_CORRUPTION;
}

/****************************************************************************
 Notify a process that it has a message. If the process doesn't exist 
 then delete its record in the database.
****************************************************************************/

static NTSTATUS message_notify(struct server_id procid)
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

	if (ret == 0) {
		return NT_STATUS_OK;
	}

	/*
	 * Something has gone wrong
	 */

	DEBUG(2,("message to process %d failed - %s\n", (int)pid,
		 strerror(errno)));

	/*
	 * No call to map_nt_error_from_unix -- don't want to link in
	 * errormap.o into lots of utils.
	 */

	if (errno == ESRCH)  return NT_STATUS_INVALID_HANDLE;
	if (errno == EINVAL) return NT_STATUS_INVALID_PARAMETER;
	if (errno == EPERM)  return NT_STATUS_ACCESS_DENIED;
	return NT_STATUS_UNSUCCESSFUL;
}

/****************************************************************************
 Send a message to a particular pid.
****************************************************************************/

static NTSTATUS messaging_tdb_send(struct messaging_context *msg_ctx,
				   struct server_id pid, int msg_type,
				   const DATA_BLOB *data,
				   struct messaging_backend *backend)
{
	struct messaging_tdb_context *ctx = talloc_get_type(backend->private_data,
					    struct messaging_tdb_context);
	struct messaging_array *msg_array;
	struct messaging_rec *rec;
	NTSTATUS status;
	TDB_DATA key;
	struct tdb_wrap *tdb = ctx->tdb;
	TALLOC_CTX *frame = talloc_stackframe();

	/* NULL pointer means implicit length zero. */
	if (!data->data) {
		SMB_ASSERT(data->length == 0);
	}

	/*
	 * Doing kill with a non-positive pid causes messages to be
	 * sent to places we don't want.
	 */

	SMB_ASSERT(procid_to_pid(&pid) > 0);

	key = message_key_pid(frame, pid);

	if (tdb_chainlock(tdb->tdb, key) == -1) {
		TALLOC_FREE(frame);
		return NT_STATUS_LOCK_NOT_GRANTED;
	}

	status = messaging_tdb_fetch(tdb->tdb, key, talloc_tos(), &msg_array);

	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	if ((msg_type & MSG_FLAG_LOWPRIORITY)
	    && (msg_array->num_messages > 1000)) {
		DEBUG(5, ("Dropping message for PID %s\n",
			  procid_str_static(&pid)));
		status = NT_STATUS_INSUFFICIENT_RESOURCES;
		goto done;
	}

	if (!(rec = TALLOC_REALLOC_ARRAY(talloc_tos(), msg_array->messages,
					 struct messaging_rec,
					 msg_array->num_messages+1))) {
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}

	rec[msg_array->num_messages].msg_version = MESSAGE_VERSION;
	rec[msg_array->num_messages].msg_type = msg_type & MSG_TYPE_MASK;
	rec[msg_array->num_messages].dest = pid;
	rec[msg_array->num_messages].src = msg_ctx->id;
	rec[msg_array->num_messages].buf = *data;

	msg_array->messages = rec;
	msg_array->num_messages += 1;

	status = messaging_tdb_store(tdb->tdb, key, msg_array);

	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	status = message_notify(pid);

	if (NT_STATUS_EQUAL(status, NT_STATUS_INVALID_HANDLE)) {
		DEBUG(2, ("pid %s doesn't exist - deleting messages record\n",
			  procid_str_static(&pid)));
		tdb_delete(tdb->tdb, message_key_pid(talloc_tos(), pid));
	}

 done:
	tdb_chainunlock(tdb->tdb, key);
	TALLOC_FREE(frame);
	return status;
}

/****************************************************************************
 Retrieve all messages for a process.
****************************************************************************/

static NTSTATUS retrieve_all_messages(TDB_CONTEXT *msg_tdb,
				      struct server_id id,
				      TALLOC_CTX *mem_ctx,
				      struct messaging_array **presult)
{
	struct messaging_array *result;
	TDB_DATA key = message_key_pid(mem_ctx, id);
	NTSTATUS status;

	if (tdb_chainlock(msg_tdb, key) == -1) {
		TALLOC_FREE(key.dptr);
		return NT_STATUS_LOCK_NOT_GRANTED;
	}

	status = messaging_tdb_fetch(msg_tdb, key, mem_ctx, &result);

	/*
	 * We delete the record here, tdb_set_max_dead keeps it around
	 */
	tdb_delete(msg_tdb, key);
	tdb_chainunlock(msg_tdb, key);

	if (NT_STATUS_IS_OK(status)) {
		*presult = result;
	}

	TALLOC_FREE(key.dptr);

	return status;
}

/****************************************************************************
 Receive and dispatch any messages pending for this process.
 JRA changed Dec 13 2006. Only one message handler now permitted per type.
 *NOTE*: Dispatch functions must be able to cope with incoming
 messages on an *odd* byte boundary.
****************************************************************************/

static void message_dispatch(struct messaging_context *msg_ctx)
{
	struct messaging_tdb_context *ctx = talloc_get_type(msg_ctx->local->private_data,
					    struct messaging_tdb_context);
	struct messaging_array *msg_array = NULL;
	struct tdb_wrap *tdb = ctx->tdb;
	NTSTATUS status;
	uint32 i;

	if (ctx->received_messages == 0) {
		return;
	}

	DEBUG(10, ("message_dispatch: received_messages = %d\n",
		   ctx->received_messages));

	status = retrieve_all_messages(tdb->tdb, msg_ctx->id, NULL, &msg_array);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("message_dispatch: failed to retrieve messages: %s\n",
			   nt_errstr(status)));
		return;
	}

	ctx->received_messages = 0;

	for (i=0; i<msg_array->num_messages; i++) {
		messaging_dispatch_rec(msg_ctx, &msg_array->messages[i]);
	}

	TALLOC_FREE(msg_array);
}

/** @} **/
