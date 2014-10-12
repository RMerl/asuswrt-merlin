/*
 * Unix SMB/CIFS implementation.
 * Support for OneFS kernel oplocks
 *
 * Copyright (C) Volker Lendecke 2007
 * Copyright (C) Tim Prouty, 2009
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#define DBGC_CLASS DBGC_LOCKING

#include "includes.h"

#if HAVE_ONEFS
#include "oplock_onefs.h"
#include "smbd/smbd.h"
#include "smbd/globals.h"

#include <ifs/ifs_syscalls.h>
#include <isi_ecs/isi_ecs_oplocks.h>
#include <sys/proc.h>

struct onefs_oplocks_context {
	struct kernel_oplocks *ctx;
	const struct oplocks_event_ops *onefs_ops;
	int onefs_event_fd;
	struct fd_event *read_fde;
};

enum onefs_callback_state {
	ONEFS_OPEN_FILE,
	ONEFS_WAITING_FOR_OPLOCK
};

struct onefs_callback_record {
	struct onefs_callback_record *prev, *next;
	uint64_t id;
	enum onefs_callback_state state;
	union {
		files_struct *fsp;	/* ONEFS_OPEN_FILE */
		uint64_t mid;		/* ONEFS_WAITING_FOR_OPLOCK */
	} data;
};

/**
 * Internal list of files (along with additional state) that have outstanding
 * oplocks or requests for oplocks.
 */
struct onefs_callback_record *callback_recs;

/**
 * Convert a onefs_callback_record to a debug string using the dbg_ctx().
 */
const char *onefs_cb_record_str_dbg(const struct onefs_callback_record *r)
{
	char *result;

	if (r == NULL) {
		result = talloc_strdup(talloc_tos(), "NULL callback record");
		return result;
	}

	switch (r->state) {
	case ONEFS_OPEN_FILE:
		result = talloc_asprintf(talloc_tos(), "cb record %llu for "
					 "file %s", r->id,
					 fsp_str_dbg(r->data.fsp));
	case ONEFS_WAITING_FOR_OPLOCK:
		result = talloc_asprintf(talloc_tos(), "cb record %llu for "
					 "pending mid %llu", r->id,
					 (unsigned long long)r->data.mid);
		break;
	default:
		result = talloc_asprintf(talloc_tos(), "cb record %llu unknown "
					 "state %d", r->id, r->state);
		break;
	}

	return result;
}

/**
 * Traverse the list of onefs_callback_records and print all entries.
 */
static void debug_cb_records(const char *fn)
{
	struct onefs_callback_record *rec;

	if (DEBUGLEVEL < 10)
		return;

	DEBUG(10, ("cb records (%s):\n", fn));

	for (rec = callback_recs; rec; rec = rec->next) {
		DEBUGADD(10, ("%s\n", onefs_cb_record_str_dbg(rec)));
	}
}

/**
 * Find a callback record in the list of outstanding oplock operations.
 *
 * Once n ifs_createfile requests an oplock on a file, the kernel communicates
 * with samba via the oplock event channel by sending events that reference an
 * id.  This function maps that id to the onefs_callback_record that was
 * created for it during the initial setup on open (onefs_oplock_wait_record).
 * When a matching id is found in the onefs_callback_record list, the
 * callback_type is checked to make sure the record is in in the correct
 * state.
 */
static struct onefs_callback_record *onefs_find_cb(uint64_t id,
    enum onefs_callback_state expected_state)
{
	struct onefs_callback_record *rec;

	debug_cb_records("onefs_find_cb");

	for (rec = callback_recs; rec; rec = rec->next) {
		if (rec->id == id) {
			DEBUG(10, ("found %s\n",
				   onefs_cb_record_str_dbg(rec)));
			break;
		}
	}

	if (rec == NULL) {
		DEBUG(5, ("Could not find callback record for id %llu\n", id));
		return NULL;
	}

	if (rec->state != expected_state) {
		DEBUG(0, ("Expected cb type %d, got %s", expected_state,
			  onefs_cb_record_str_dbg(rec)));
		SMB_ASSERT(0);
		return NULL;
	}

	return rec;
}

/**
 * Remove and free a callback record from the callback record list.
 */
void destroy_onefs_callback_record(uint64_t id)
{
	struct onefs_callback_record *rec;

	debug_cb_records("destroy_onefs_callback_record");

	if (id == 0) {
		DEBUG(10, ("destroy_onefs_callback_record: Nothing to "
			   "destroy\n"));
		return;
	}

	for (rec = callback_recs; rec; rec = rec->next) {
		if (rec->id == id) {
			DLIST_REMOVE(callback_recs, rec);
			SAFE_FREE(rec);
			DEBUG(10, ("removed cb rec %llu\n", id));
			return;
		}
	}

	DEBUG(0, ("Could not find cb rec %llu to delete", id));
	SMB_ASSERT(0);
}

/**
 * Initialize a callback record and add it to the list of outstanding callback
 * records.
 *
 * This is called in the open path before ifs_createfile so an id can be
 * passed in.  Each callback record can be in one of two states:
 *
 *   1. WAITING_FOR_OPLOCK: This is the initial state for all callback
 *   records.  If ifs_createfile can be completed syncronously without needing
 *   to break any level I oplocks, the state is transitioned to OPEN_FILE.
 *   Otherwise ifs_createfile will finish asynchronously and the open is
 *   deferred.  When the necessary level I opocks have been broken, and the
 *   open can be done, an event is sent by the kernel on the oplock event
 *   channel, which is handled by semlock_available_handler.  At this point
 *   the deferred open is retried.  Unless a level I oplock was acquired by
 *   another client, ifs_createfile will now complete synchronously.
 *
 *   2. OPEN_FILE: Once ifs_createfile completes, the callback record is
 *   transitioned to this state via onefs_set_oplock_callback.
 */
uint64_t onefs_oplock_wait_record(uint64_t mid)
{
	struct onefs_callback_record *result;
	static uint64_t id_generator = 0;

	if (!(result = SMB_MALLOC_P(struct onefs_callback_record))) {
		DEBUG(0, ("talloc failed\n"));
		return 0;
	}

	memset(result, '\0', sizeof(result));

	id_generator += 1;
	if (id_generator == 0) {
		/* Wow, that's a long-running smbd... */
		id_generator += 1;
	}

	result->id = id_generator;

	result->state = ONEFS_WAITING_FOR_OPLOCK;
	result->data.mid = mid;
	DLIST_ADD(callback_recs, result);

	DEBUG(10, ("New cb rec %llu created\n", result->id));

	return result->id;
}

/**
 * Transition the callback record state to OPEN_FILE.
 *
 * This is called after the file is opened and an fsp struct has been
 * allocated.  The mid is dropped in favor of storing the fsp.
 */
void onefs_set_oplock_callback(uint64_t id, files_struct *fsp)
{
	struct onefs_callback_record *cb;
	char *msg;

	DEBUG(10, ("onefs_set_oplock_callback called for cb rec %llu\n", id));

	if (!(cb = onefs_find_cb(id, ONEFS_WAITING_FOR_OPLOCK))) {
		if (asprintf(&msg, "Got invalid callback %lld\n", id) != -1) {
			smb_panic(msg);
		}
		smb_panic("Got invalid callback id\n");
	}

	/*
	 * Paranoia check
	 */
	if (open_was_deferred(cb->data.mid)) {
		if (asprintf(&msg, "Trying to upgrade callback for deferred "
			     "open mid=%llu\n", (unsigned long long)cb->data.mid) != -1) {
			smb_panic(msg);
		}
		smb_panic("Trying to upgrade callback for deferred open "
			  "mid\n");
	}

	cb->state = ONEFS_OPEN_FILE;
	cb->data.fsp = fsp;
}

/**
 * Using a callback record, initialize a share mode entry to pass to
 * share_mode_entry_to_message to send samba IPC messages.
 */
static void init_share_mode_entry(struct share_mode_entry *sme,
				  struct onefs_callback_record *cb,
				  int op_type)
{
	ZERO_STRUCT(*sme);

	sme->pid = procid_self();
	sme->op_type = op_type;
	sme->id = cb->data.fsp->file_id;
	sme->share_file_id = cb->data.fsp->fh->gen_id;
}

/**
 * Callback when a break-to-none event is received from the kernel.
 *
 * On OneFS level 1 oplocks are always broken to level 2 first, therefore an
 * async level 2 break message is always sent when breaking to none.  The
 * downside of this is that OneFS currently has no way to express breaking
 * directly from level 1 to none.
 */
static void oplock_break_to_none_handler(uint64_t id)
{
	struct onefs_callback_record *cb;
	struct share_mode_entry sme;
	char msg[MSG_SMB_SHARE_MODE_ENTRY_SIZE];

	DEBUG(10, ("oplock_break_to_none_handler called for id %llu\n", id));

	if (!(cb = onefs_find_cb(id, ONEFS_OPEN_FILE))) {
		DEBUG(3, ("oplock_break_to_none_handler: could not find "
			  "callback id %llu\n", id));
		return;
	}

	DEBUG(10, ("oplock_break_to_none_handler called for file %s\n",
		   fsp_str_dbg(cb->data.fsp)));

	init_share_mode_entry(&sme, cb, FORCE_OPLOCK_BREAK_TO_NONE);
	share_mode_entry_to_message(msg, &sme);
	messaging_send_buf(smbd_messaging_context(),
			   sme.pid,
			   MSG_SMB_ASYNC_LEVEL2_BREAK,
			   (uint8_t *)msg,
			   MSG_SMB_SHARE_MODE_ENTRY_SIZE);

	/*
	 * We could still receive an OPLOCK_REVOKED message, so keep the
	 * oplock_callback_id around.
	 */
}

/**
 * Callback when a break-to-level2 event is received from the kernel.
 *
 * Breaks from level 1 to level 2.
 */
static void oplock_break_to_level_two_handler(uint64_t id)
{
	struct onefs_callback_record *cb;
	struct share_mode_entry sme;
	char msg[MSG_SMB_SHARE_MODE_ENTRY_SIZE];

	DEBUG(10, ("oplock_break_to_level_two_handler called for id %llu\n",
		   id));

	if (!(cb = onefs_find_cb(id, ONEFS_OPEN_FILE))) {
		DEBUG(3, ("oplock_break_to_level_two_handler: could not find "
			  "callback id %llu\n", id));
		return;
	}

	DEBUG(10, ("oplock_break_to_level_two_handler called for file %s\n",
		   fsp_str_dbg(cb->data.fsp)));

	init_share_mode_entry(&sme, cb, LEVEL_II_OPLOCK);
	share_mode_entry_to_message(msg, &sme);
	messaging_send_buf(smbd_messaging_context(),
			  sme.pid,
			  MSG_SMB_BREAK_REQUEST,
			  (uint8_t *)msg,
			  MSG_SMB_SHARE_MODE_ENTRY_SIZE);

	/*
	 * We could still receive an OPLOCK_REVOKED or OPLOCK_BREAK_TO_NONE
	 * message, so keep the oplock_callback_id around.
	 */
}

/**
 * Revoke an oplock from an unresponsive client.
 *
 * The kernel will send this message when it times out waiting for a level 1
 * oplock break to be acknowledged by the client.  The oplock is then
 * immediately removed.
 */
static void oplock_revoked_handler(uint64_t id)
{
	struct onefs_callback_record *cb;
	files_struct *fsp = NULL;

	DEBUG(10, ("oplock_revoked_handler called for id %llu\n", id));

	if (!(cb = onefs_find_cb(id, ONEFS_OPEN_FILE))) {
		DEBUG(3, ("oplock_revoked_handler: could not find "
			  "callback id %llu\n", id));
		return;
	}

	fsp = cb->data.fsp;

	SMB_ASSERT(fsp->oplock_timeout == NULL);

	DEBUG(0,("Level 1 oplock break failed for file %s. Forcefully "
		 "revoking oplock\n", fsp_str_dbg(fsp)));

	remove_oplock(fsp);

	/*
	 * cb record is cleaned up in fsp ext data destructor on close, so
	 * leave it in the list.
	 */
}

/**
 * Asynchronous ifs_createfile callback
 *
 * If ifs_createfile had to asynchronously break any oplocks, this function is
 * called when the kernel sends an event that the open can be retried.
 */
static void semlock_available_handler(uint64_t id)
{
	struct onefs_callback_record *cb;

	DEBUG(10, ("semlock_available_handler called: %llu\n", id));

	if (!(cb = onefs_find_cb(id, ONEFS_WAITING_FOR_OPLOCK))) {
		DEBUG(5, ("semlock_available_handler: Did not find callback "
			  "%llu\n", id));
		return;
	}

	DEBUG(10, ("Got semlock available for mid %llu\n",
		(unsigned long long)cb->data.mid));

	/* Paranoia check */
	if (!(open_was_deferred(cb->data.mid))) {
		char *msg;
		if (asprintf(&msg, "Semlock available on an open that wasn't "
			     "deferred: %s\n",
			      onefs_cb_record_str_dbg(cb)) != -1) {
			smb_panic(msg);
		}
		smb_panic("Semlock available on an open that wasn't "
			  "deferred\n");
	}

	schedule_deferred_open_smb_message(cb->data.mid);

	/* Cleanup the callback record since the open will be retried. */
	destroy_onefs_callback_record(id);

	return;
}

/**
 * Asynchronous ifs_createfile failure callback
 *
 * If ifs_createfile had to asynchronously break any oplocks, but an error was
 * encountered in the kernel, the open will be retried with the state->failed
 * set to true.  This will prompt the open path to send an INTERNAL_ERROR
 * error message to the client.
 */
static void semlock_async_failure_handler(uint64_t id)
{
	struct onefs_callback_record *cb;
	struct deferred_open_record *state;

	DEBUG(1, ("semlock_async_failure_handler called: %llu\n", id));

	if (!(cb = onefs_find_cb(id, ONEFS_WAITING_FOR_OPLOCK))) {
		DEBUG(5, ("semlock_async_failure_handler: Did not find callback "
			  "%llu\n", id));
		return;
	}

	DEBUG(1, ("Got semlock_async_failure message for mid %llu\n",
		(unsigned long long)cb->data.mid));

	/* Paranoia check */
	if (!(open_was_deferred(cb->data.mid))) {
		char *msg;
		if (asprintf(&msg, "Semlock failure on an open that wasn't "
			     "deferred: %s\n",
			      onefs_cb_record_str_dbg(cb)) != -1) {
			smb_panic(msg);
		}
		smb_panic("Semlock failure on an open that wasn't deferred\n");
	}

	/* Find the actual deferred open record. */
	if (!get_open_deferred_message_state(cb->data.mid, NULL, &state)) {
		DEBUG(0, ("Could not find deferred request for "
			  "mid %d\n", cb->data.mid));
		destroy_onefs_callback_record(id);
		return;
	}

	/* Update to failed so the client can be notified on retried open. */
	state->failed = true;

	/* Schedule deferred open for immediate retry. */
	schedule_deferred_open_smb_message(cb->data.mid);

	/* Cleanup the callback record here since the open will be retried. */
	destroy_onefs_callback_record(id);

	return;
}

/**
 * OneFS acquires all oplocks via ifs_createfile, so this is a no-op.
 */
static bool onefs_set_kernel_oplock(struct kernel_oplocks *_ctx,
				    files_struct *fsp, int oplock_type) {
	return true;
}

/**
 * Release the kernel oplock.
 */
static void onefs_release_kernel_oplock(struct kernel_oplocks *_ctx,
					files_struct *fsp, int oplock_type)
{
	enum oplock_type oplock = onefs_samba_oplock_to_oplock(oplock_type);

	DEBUG(10, ("onefs_release_kernel_oplock: Releasing %s to type %s\n",
		   fsp_str_dbg(fsp), onefs_oplock_str(oplock)));

	if (fsp->fh->fd == -1) {
		DEBUG(1, ("no fd\n"));
		return;
	}

	/* Downgrade oplock to either SHARED or NONE. */
	if (ifs_oplock_downgrade(fsp->fh->fd, oplock)) {
		DEBUG(1,("ifs_oplock_downgrade failed: %s\n",
			 strerror(errno)));
	}
}

/**
 * Wrap ifs_semlock_write so it is only called on operations that aren't
 * already contended in the kernel.
 */
static void onefs_semlock_write(int fd, enum level2_contention_type type,
				enum semlock_operation semlock_op)
{
	int ret;

	switch (type) {
	case LEVEL2_CONTEND_ALLOC_GROW:
	case LEVEL2_CONTEND_POSIX_BRL:
		DEBUG(10, ("Taking %d write semlock for cmd %d on fd: %d\n",
			   semlock_op, type, fd));
		ret = ifs_semlock_write(fd, semlock_op);
		if (ret) {
			DEBUG(0,("ifs_semlock_write failed taking %d write "
				 "semlock for cmd %d on fd: %d: %s",
				 semlock_op, type, fd, strerror(errno)));
		}
		break;
	default:
		DEBUG(10, ("Skipping write semlock for cmd %d on fd: %d\n",
			   type, fd));
	}
}

/**
 * Contend level 2 oplocks in the kernel and smbd.
 *
 * Taking a write semlock will contend all level 2 oplocks in all smbds across
 * the cluster except the fsp's own level 2 oplock.  This lack of
 * self-contention is a limitation of the current OneFS kernel oplocks
 * implementation.  Luckily it is easy to contend our own level 2 oplock by
 * checking the the fsp's oplock_type.  If it's a level2, send a break message
 * to the client and remove the oplock.
 */
static void onefs_contend_level2_oplocks_begin(files_struct *fsp,
					       enum level2_contention_type type)
{
	/* Take care of level 2 kernel contention. */
	onefs_semlock_write(fsp->fh->fd, type, SEMLOCK_LOCK);

	/* Take care of level 2 self contention. */
	if (LEVEL_II_OPLOCK_TYPE(fsp->oplock_type))
		break_level2_to_none_async(fsp);
}

/**
 * Unlock the write semlock when the level 2 contending operation ends.
 */
static void onefs_contend_level2_oplocks_end(files_struct *fsp,
					     enum level2_contention_type type)
{
	/* Take care of level 2 kernel contention. */
	onefs_semlock_write(fsp->fh->fd, type, SEMLOCK_UNLOCK);
}

/**
 * Return string value of onefs oplock types.
 */
const char *onefs_oplock_str(enum oplock_type onefs_oplock_type)
{
	switch (onefs_oplock_type) {
	case OPLOCK_NONE:
		return "OPLOCK_NONE";
	case OPLOCK_EXCLUSIVE:
		return "OPLOCK_EXCLUSIVE";
	case OPLOCK_BATCH:
		return "OPLOCK_BATCH";
	case OPLOCK_SHARED:
		return "OPLOCK_SHARED";
	default:
		break;
	}
	return "UNKNOWN";
}

/**
 * Convert from onefs to samba oplock.
 */
int onefs_oplock_to_samba_oplock(enum oplock_type onefs_oplock)
{
	switch (onefs_oplock) {
	case OPLOCK_NONE:
		return NO_OPLOCK;
	case OPLOCK_EXCLUSIVE:
		return EXCLUSIVE_OPLOCK;
	case OPLOCK_BATCH:
		return BATCH_OPLOCK;
	case OPLOCK_SHARED:
		return LEVEL_II_OPLOCK;
	default:
		DEBUG(0, ("unknown oplock type %d found\n", onefs_oplock));
		break;
	}
	return NO_OPLOCK;
}

/**
 * Convert from samba to onefs oplock.
 */
enum oplock_type onefs_samba_oplock_to_oplock(int samba_oplock_type)
{
	if (BATCH_OPLOCK_TYPE(samba_oplock_type)) return OPLOCK_BATCH;
	if (EXCLUSIVE_OPLOCK_TYPE(samba_oplock_type)) return OPLOCK_EXCLUSIVE;
	if (LEVEL_II_OPLOCK_TYPE(samba_oplock_type)) return OPLOCK_SHARED;
	return OPLOCK_NONE;
}

/**
 * Oplock event handler.
 *
 * Call into the event system dispatcher to handle each event.
 */
static void onefs_oplocks_read_fde_handler(struct event_context *ev,
					   struct fd_event *fde,
					   uint16_t flags,
					   void *private_data)
{
	struct onefs_oplocks_context *ctx =
	    talloc_get_type(private_data, struct onefs_oplocks_context);

	if (oplocks_event_dispatcher(ctx->onefs_ops)) {
		DEBUG(0, ("oplocks_event_dispatcher failed: %s\n",
			  strerror(errno)));
	}
}

/**
 * Setup kernel oplocks
 */
static const struct kernel_oplocks_ops onefs_koplocks_ops = {
	.set_oplock			= onefs_set_kernel_oplock,
	.release_oplock			= onefs_release_kernel_oplock,
	.contend_level2_oplocks_begin	= onefs_contend_level2_oplocks_begin,
	.contend_level2_oplocks_end	= onefs_contend_level2_oplocks_end,
};

static const struct oplocks_event_ops onefs_dispatch_ops = {
	.oplock_break_to_none = oplock_break_to_none_handler,
	.oplock_break_to_level_two = oplock_break_to_level_two_handler,
	.oplock_revoked = oplock_revoked_handler,
	.semlock_available = semlock_available_handler,
	.semlock_async_failure = semlock_async_failure_handler,
};

struct kernel_oplocks *onefs_init_kernel_oplocks(TALLOC_CTX *mem_ctx)
{
	struct kernel_oplocks *_ctx = NULL;
	struct onefs_oplocks_context *ctx = NULL;
        struct procoptions po = PROCOPTIONS_INIT;

	DEBUG(10, ("onefs_init_kernel_oplocks called\n"));

	/* Set the non-blocking proc flag */
	po.po_flags_on |= P_NON_BLOCKING_SEMLOCK;
	if (setprocoptions(&po) != 0) {
		DEBUG(0, ("setprocoptions failed: %s.\n", strerror(errno)));
		return NULL;
	}

	/* Setup the oplock contexts */
	_ctx = talloc_zero(mem_ctx, struct kernel_oplocks);
	if (!_ctx) {
		return NULL;
	}

	ctx = talloc_zero(_ctx, struct onefs_oplocks_context);
	if (!ctx) {
		goto err_out;
	}

	_ctx->ops = &onefs_koplocks_ops;
	_ctx->flags = (KOPLOCKS_LEVEL2_SUPPORTED |
		       KOPLOCKS_DEFERRED_OPEN_NOTIFICATION |
		       KOPLOCKS_TIMEOUT_NOTIFICATION |
		       KOPLOCKS_OPLOCK_BROKEN_NOTIFICATION);
	_ctx->private_data = ctx;
	ctx->ctx = _ctx;
	ctx->onefs_ops = &onefs_dispatch_ops;

	/* Register an kernel event channel for oplocks */
	ctx->onefs_event_fd = oplocks_event_register();
	if (ctx->onefs_event_fd == -1) {
		DEBUG(0, ("oplocks_event_register failed: %s\n",
			   strerror(errno)));
		goto err_out;
	}

	DEBUG(10, ("oplock event_fd = %d\n", ctx->onefs_event_fd));

	/* Register the oplock event_fd with samba's event system */
	ctx->read_fde = event_add_fd(smbd_event_context(),
				     ctx,
				     ctx->onefs_event_fd,
				     EVENT_FD_READ,
				     onefs_oplocks_read_fde_handler,
				     ctx);
	return _ctx;

 err_out:
	talloc_free(_ctx);
	return NULL;
}

#else
 void oplock_onefs_dummy(void);
 void oplock_onefs_dummy(void) {}
#endif /* HAVE_ONEFS */
