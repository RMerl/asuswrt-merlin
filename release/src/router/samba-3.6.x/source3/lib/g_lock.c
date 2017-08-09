/*
   Unix SMB/CIFS implementation.
   global locks based on dbwrap and messaging
   Copyright (C) 2009 by Volker Lendecke

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
#include "system/filesys.h"
#include "g_lock.h"
#include "util_tdb.h"
#include "ctdbd_conn.h"
#include "../lib/util/select.h"
#include "system/select.h"
#include "messages.h"

static NTSTATUS g_lock_force_unlock(struct g_lock_ctx *ctx, const char *name,
				    struct server_id pid);

struct g_lock_ctx {
	struct db_context *db;
	struct messaging_context *msg;
};

/*
 * The "g_lock.tdb" file contains records, indexed by the 0-terminated
 * lockname. The record contains an array of "struct g_lock_rec"
 * structures. Waiters have the lock_type with G_LOCK_PENDING or'ed.
 */

struct g_lock_rec {
	enum g_lock_type lock_type;
	struct server_id pid;
};

struct g_lock_ctx *g_lock_ctx_init(TALLOC_CTX *mem_ctx,
				   struct messaging_context *msg)
{
	struct g_lock_ctx *result;

	result = talloc(mem_ctx, struct g_lock_ctx);
	if (result == NULL) {
		return NULL;
	}
	result->msg = msg;

	result->db = db_open(result, lock_path("g_lock.tdb"), 0,
			     TDB_CLEAR_IF_FIRST|TDB_INCOMPATIBLE_HASH, O_RDWR|O_CREAT, 0700);
	if (result->db == NULL) {
		DEBUG(1, ("g_lock_init: Could not open g_lock.tdb"));
		TALLOC_FREE(result);
		return NULL;
	}
	return result;
}

static bool g_lock_conflicts(enum g_lock_type lock_type,
			     const struct g_lock_rec *rec)
{
	enum g_lock_type rec_lock = rec->lock_type;

	if ((rec_lock & G_LOCK_PENDING) != 0) {
		return false;
	}

	/*
	 * Only tested write locks so far. Very likely this routine
	 * needs to be fixed for read locks....
	 */
	if ((lock_type == G_LOCK_READ) && (rec_lock == G_LOCK_READ)) {
		return false;
	}
	return true;
}

static bool g_lock_parse(TALLOC_CTX *mem_ctx, TDB_DATA data,
			 int *pnum_locks, struct g_lock_rec **plocks)
{
	int i, num_locks;
	struct g_lock_rec *locks;

	if ((data.dsize % sizeof(struct g_lock_rec)) != 0) {
		DEBUG(1, ("invalid lock record length %d\n", (int)data.dsize));
		return false;
	}

	num_locks = data.dsize / sizeof(struct g_lock_rec);
	locks = talloc_array(mem_ctx, struct g_lock_rec, num_locks);
	if (locks == NULL) {
		DEBUG(1, ("talloc failed\n"));
		return false;
	}

	memcpy(locks, data.dptr, data.dsize);

	DEBUG(10, ("locks:\n"));
	for (i=0; i<num_locks; i++) {
		DEBUGADD(10, ("%s: %s %s\n",
			      procid_str(talloc_tos(), &locks[i].pid),
			      ((locks[i].lock_type & 1) == G_LOCK_READ) ?
			      "read" : "write",
			      (locks[i].lock_type & G_LOCK_PENDING) ?
			      "(pending)" : "(owner)"));

		if (((locks[i].lock_type & G_LOCK_PENDING) == 0)
		    && !process_exists(locks[i].pid)) {

			DEBUGADD(10, ("lock owner %s died -- discarding\n",
				      procid_str(talloc_tos(),
						 &locks[i].pid)));

			if (i < (num_locks-1)) {
				locks[i] = locks[num_locks-1];
			}
			num_locks -= 1;
		}
	}

	*plocks = locks;
	*pnum_locks = num_locks;
	return true;
}

static void g_lock_cleanup(int *pnum_locks, struct g_lock_rec *locks)
{
	int i, num_locks;

	num_locks = *pnum_locks;

	DEBUG(10, ("g_lock_cleanup: %d locks\n", num_locks));

	for (i=0; i<num_locks; i++) {
		if (process_exists(locks[i].pid)) {
			continue;
		}
		DEBUGADD(10, ("%s does not exist -- discarding\n",
			      procid_str(talloc_tos(), &locks[i].pid)));

		if (i < (num_locks-1)) {
			locks[i] = locks[num_locks-1];
		}
		num_locks -= 1;
	}
	*pnum_locks = num_locks;
	return;
}

static struct g_lock_rec *g_lock_addrec(TALLOC_CTX *mem_ctx,
					struct g_lock_rec *locks,
					int *pnum_locks,
					const struct server_id pid,
					enum g_lock_type lock_type)
{
	struct g_lock_rec *result;
	int num_locks = *pnum_locks;

	result = talloc_realloc(mem_ctx, locks, struct g_lock_rec,
				num_locks+1);
	if (result == NULL) {
		return NULL;
	}

	result[num_locks].pid = pid;
	result[num_locks].lock_type = lock_type;
	*pnum_locks += 1;
	return result;
}

static void g_lock_got_retry(struct messaging_context *msg,
			     void *private_data,
			     uint32_t msg_type,
			     struct server_id server_id,
			     DATA_BLOB *data);

static NTSTATUS g_lock_trylock(struct g_lock_ctx *ctx, const char *name,
			       enum g_lock_type lock_type)
{
	struct db_record *rec = NULL;
	struct g_lock_rec *locks = NULL;
	int i, num_locks;
	struct server_id self;
	int our_index;
	TDB_DATA data;
	NTSTATUS status = NT_STATUS_OK;
	NTSTATUS store_status;

again:
	rec = ctx->db->fetch_locked(ctx->db, talloc_tos(),
				    string_term_tdb_data(name));
	if (rec == NULL) {
		DEBUG(10, ("fetch_locked(\"%s\") failed\n", name));
		status = NT_STATUS_LOCK_NOT_GRANTED;
		goto done;
	}

	if (!g_lock_parse(talloc_tos(), rec->value, &num_locks, &locks)) {
		DEBUG(10, ("g_lock_parse for %s failed\n", name));
		status = NT_STATUS_INTERNAL_ERROR;
		goto done;
	}

	self = messaging_server_id(ctx->msg);
	our_index = -1;

	for (i=0; i<num_locks; i++) {
		if (procid_equal(&self, &locks[i].pid)) {
			if (our_index != -1) {
				DEBUG(1, ("g_lock_trylock: Added ourself "
					  "twice!\n"));
				status = NT_STATUS_INTERNAL_ERROR;
				goto done;
			}
			if ((locks[i].lock_type & G_LOCK_PENDING) == 0) {
				DEBUG(1, ("g_lock_trylock: Found ourself not "
					  "pending!\n"));
				status = NT_STATUS_INTERNAL_ERROR;
				goto done;
			}

			our_index = i;

			/* never conflict with ourself */
			continue;
		}
		if (g_lock_conflicts(lock_type, &locks[i])) {
			struct server_id pid = locks[i].pid;

			if (!process_exists(pid)) {
				TALLOC_FREE(locks);
				TALLOC_FREE(rec);
				status = g_lock_force_unlock(ctx, name, pid);
				if (!NT_STATUS_IS_OK(status)) {
					DEBUG(1, ("Could not unlock dead lock "
						  "holder!\n"));
					goto done;
				}
				goto again;
			}
			lock_type |= G_LOCK_PENDING;
		}
	}

	if (our_index == -1) {
		/* First round, add ourself */

		locks = g_lock_addrec(talloc_tos(), locks, &num_locks,
				      self, lock_type);
		if (locks == NULL) {
			DEBUG(10, ("g_lock_addrec failed\n"));
			status = NT_STATUS_NO_MEMORY;
			goto done;
		}
	} else {
		/*
		 * Retry. We were pending last time. Overwrite the
		 * stored lock_type with what we calculated, we might
		 * have acquired the lock this time.
		 */
		locks[our_index].lock_type = lock_type;
	}

	if (NT_STATUS_IS_OK(status) && ((lock_type & G_LOCK_PENDING) == 0)) {
		/*
		 * Walk through the list of locks, search for dead entries
		 */
		g_lock_cleanup(&num_locks, locks);
	}

	data = make_tdb_data((uint8_t *)locks, num_locks * sizeof(*locks));
	store_status = rec->store(rec, data, 0);
	if (!NT_STATUS_IS_OK(store_status)) {
		DEBUG(1, ("rec->store failed: %s\n",
			  nt_errstr(store_status)));
		status = store_status;
	}

done:
	TALLOC_FREE(locks);
	TALLOC_FREE(rec);

	if (NT_STATUS_IS_OK(status) && (lock_type & G_LOCK_PENDING) != 0) {
		return STATUS_PENDING;
	}

	return NT_STATUS_OK;
}

NTSTATUS g_lock_lock(struct g_lock_ctx *ctx, const char *name,
		     enum g_lock_type lock_type, struct timeval timeout)
{
	struct tevent_timer *te = NULL;
	NTSTATUS status;
	bool retry = false;
	struct timeval timeout_end;
	struct timeval time_now;

	DEBUG(10, ("Trying to acquire lock %d for %s\n", (int)lock_type,
		   name));

	if (lock_type & ~1) {
		DEBUG(1, ("Got invalid lock type %d for %s\n",
			  (int)lock_type, name));
		return NT_STATUS_INVALID_PARAMETER;
	}

#ifdef CLUSTER_SUPPORT
	if (lp_clustering()) {
		status = ctdb_watch_us(messaging_ctdbd_connection());
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(10, ("could not register retry with ctdb: %s\n",
				   nt_errstr(status)));
			goto done;
		}
	}
#endif

	status = messaging_register(ctx->msg, &retry, MSG_DBWRAP_G_LOCK_RETRY,
				    g_lock_got_retry);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("messaging_register failed: %s\n",
			   nt_errstr(status)));
		return status;
	}

	time_now = timeval_current();
	timeout_end = timeval_sum(&time_now, &timeout);

	while (true) {
		struct pollfd *pollfds;
		int num_pollfds;
		int saved_errno;
		int ret;
		struct timeval timeout_remaining, select_timeout;

		status = g_lock_trylock(ctx, name, lock_type);
		if (NT_STATUS_IS_OK(status)) {
			DEBUG(10, ("Got lock %s\n", name));
			break;
		}
		if (!NT_STATUS_EQUAL(status, STATUS_PENDING)) {
			DEBUG(10, ("g_lock_trylock failed: %s\n",
				   nt_errstr(status)));
			break;
		}

		DEBUG(10, ("g_lock_trylock: Did not get lock, waiting...\n"));

		/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		 *             !!! HACK ALERT --- FIX ME !!!
		 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		 * What we really want to do here is to react to
		 * MSG_DBWRAP_G_LOCK_RETRY messages that are either sent
		 * by a client doing g_lock_unlock or by ourselves when
		 * we receive a CTDB_SRVID_SAMBA_NOTIFY or
		 * CTDB_SRVID_RECONFIGURE message from ctdbd, i.e. when
		 * either a client holding a lock or a complete node
		 * has died.
		 *
		 * Doing this properly involves calling tevent_loop_once(),
		 * but doing this here with the main ctdbd messaging context
		 * creates a nested event loop when g_lock_lock() is called
		 * from the main event loop, e.g. in a tcon_and_X where the
		 * share_info.tdb needs to be initialized and is locked by
		 * another process, or when the remore registry is accessed
		 * for writing and some other process already holds a lock
		 * on the registry.tdb.
		 *
		 * So as a quick fix, we act a little coarsely here: we do
		 * a select on the ctdb connection fd and when it is readable
		 * or we get EINTR, then we retry without actually parsing
		 * any ctdb packages or dispatching messages. This means that
		 * we retry more often than intended by design, but this does
		 * not harm and it is unobtrusive. When we have finished,
		 * the main loop will pick up all the messages and ctdb
		 * packets. The only extra twist is that we cannot use timed
		 * events here but have to handcode a timeout.
		 */

		/*
		 * We allocate 2 entries here. One is needed anyway for
		 * sys_poll and in the clustering case we might have to add
		 * the ctdb fd. This avoids the realloc then.
		 */
		pollfds = TALLOC_ARRAY(talloc_tos(), struct pollfd, 2);
		if (pollfds == NULL) {
			status = NT_STATUS_NO_MEMORY;
			break;
		}
		num_pollfds = 0;

#ifdef CLUSTER_SUPPORT
		if (lp_clustering()) {
			struct ctdbd_connection *conn;
			conn = messaging_ctdbd_connection();

			pollfds[0].fd = ctdbd_conn_get_fd(conn);
			pollfds[0].events = POLLIN|POLLHUP;

			num_pollfds += 1;
		}
#endif

		time_now = timeval_current();
		timeout_remaining = timeval_until(&time_now, &timeout_end);
		select_timeout = timeval_set(60, 0);

		select_timeout = timeval_min(&select_timeout,
					     &timeout_remaining);

		ret = sys_poll(pollfds, num_pollfds,
			       timeval_to_msec(select_timeout));

		/*
		 * We're not *really interested in the actual flags. We just
		 * need to retry this whole thing.
		 */
		saved_errno = errno;
		TALLOC_FREE(pollfds);
		errno = saved_errno;

		if (ret == -1) {
			if (errno != EINTR) {
				DEBUG(1, ("error calling select: %s\n",
					  strerror(errno)));
				status = NT_STATUS_INTERNAL_ERROR;
				break;
			}
			/*
			 * errno == EINTR:
			 * This means a signal was received.
			 * It might have been a MSG_DBWRAP_G_LOCK_RETRY message.
			 * ==> retry
			 */
		} else if (ret == 0) {
			if (timeval_expired(&timeout_end)) {
				DEBUG(10, ("g_lock_lock timed out\n"));
				status = NT_STATUS_LOCK_NOT_GRANTED;
				break;
			} else {
				DEBUG(10, ("select returned 0 but timeout not "
					   "not expired, retrying\n"));
			}
		} else if (ret != 1) {
			DEBUG(1, ("invalid return code of select: %d\n", ret));
			status = NT_STATUS_INTERNAL_ERROR;
			break;
		}
		/*
		 * ret == 1:
		 * This means ctdbd has sent us some data.
		 * Might be a CTDB_SRVID_RECONFIGURE or a
		 * CTDB_SRVID_SAMBA_NOTIFY message.
		 * ==> retry
		 */
	}

#ifdef CLUSTER_SUPPORT
done:
#endif

	if (!NT_STATUS_IS_OK(status)) {
		NTSTATUS unlock_status;

		unlock_status = g_lock_unlock(ctx, name);

		if (!NT_STATUS_IS_OK(unlock_status)) {
			DEBUG(1, ("Could not remove ourself from the locking "
				  "db: %s\n", nt_errstr(status)));
		}
	}

	messaging_deregister(ctx->msg, MSG_DBWRAP_G_LOCK_RETRY, &retry);
	TALLOC_FREE(te);

	return status;
}

static void g_lock_got_retry(struct messaging_context *msg,
			     void *private_data,
			     uint32_t msg_type,
			     struct server_id server_id,
			     DATA_BLOB *data)
{
	bool *pretry = (bool *)private_data;

	DEBUG(10, ("Got retry message from pid %s\n",
		   procid_str(talloc_tos(), &server_id)));

	*pretry = true;
}

static NTSTATUS g_lock_force_unlock(struct g_lock_ctx *ctx, const char *name,
				    struct server_id pid)
{
	struct db_record *rec = NULL;
	struct g_lock_rec *locks = NULL;
	int i, num_locks;
	enum g_lock_type lock_type;
	NTSTATUS status;

	rec = ctx->db->fetch_locked(ctx->db, talloc_tos(),
				    string_term_tdb_data(name));
	if (rec == NULL) {
		DEBUG(10, ("fetch_locked(\"%s\") failed\n", name));
		status = NT_STATUS_INTERNAL_ERROR;
		goto done;
	}

	if (!g_lock_parse(talloc_tos(), rec->value, &num_locks, &locks)) {
		DEBUG(10, ("g_lock_parse for %s failed\n", name));
		status = NT_STATUS_INTERNAL_ERROR;
		goto done;
	}

	for (i=0; i<num_locks; i++) {
		if (procid_equal(&pid, &locks[i].pid)) {
			break;
		}
	}

	if (i == num_locks) {
		DEBUG(10, ("g_lock_force_unlock: Lock not found\n"));
		status = NT_STATUS_INTERNAL_ERROR;
		goto done;
	}

	lock_type = locks[i].lock_type;

	if (i < (num_locks-1)) {
		locks[i] = locks[num_locks-1];
	}
	num_locks -= 1;

	if (num_locks == 0) {
		status = rec->delete_rec(rec);
	} else {
		TDB_DATA data;
		data = make_tdb_data((uint8_t *)locks,
				     sizeof(struct g_lock_rec) * num_locks);
		status = rec->store(rec, data, 0);
	}

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("g_lock_force_unlock: Could not store record: %s\n",
			  nt_errstr(status)));
		goto done;
	}

	TALLOC_FREE(rec);

	if ((lock_type & G_LOCK_PENDING) == 0) {
		int num_wakeups = 0;

		/*
		 * We've been the lock holder. Others to retry. Don't
		 * tell all others to avoid a thundering herd. In case
		 * this leads to a complete stall because we miss some
		 * processes, the loop in g_lock_lock tries at least
		 * once a minute.
		 */

		for (i=0; i<num_locks; i++) {
			if ((locks[i].lock_type & G_LOCK_PENDING) == 0) {
				continue;
			}
			if (!process_exists(locks[i].pid)) {
				continue;
			}

			/*
			 * Ping all waiters to retry
			 */
			status = messaging_send(ctx->msg, locks[i].pid,
						MSG_DBWRAP_G_LOCK_RETRY,
						&data_blob_null);
			if (!NT_STATUS_IS_OK(status)) {
				DEBUG(1, ("sending retry to %s failed: %s\n",
					  procid_str(talloc_tos(),
						     &locks[i].pid),
					  nt_errstr(status)));
			} else {
				num_wakeups += 1;
			}
			if (num_wakeups > 5) {
				break;
			}
		}
	}
done:
	/*
	 * For the error path, TALLOC_FREE(rec) as well. In the good
	 * path we have already freed it.
	 */
	TALLOC_FREE(rec);

	TALLOC_FREE(locks);
	return status;
}

NTSTATUS g_lock_unlock(struct g_lock_ctx *ctx, const char *name)
{
	NTSTATUS status;

	status = g_lock_force_unlock(ctx, name, messaging_server_id(ctx->msg));

#ifdef CLUSTER_SUPPORT
	if (lp_clustering()) {
		ctdb_unwatch(messaging_ctdbd_connection());
	}
#endif
	return status;
}

struct g_lock_locks_state {
	int (*fn)(const char *name, void *private_data);
	void *private_data;
};

static int g_lock_locks_fn(struct db_record *rec, void *priv)
{
	struct g_lock_locks_state *state = (struct g_lock_locks_state *)priv;

	if ((rec->key.dsize == 0) || (rec->key.dptr[rec->key.dsize-1] != 0)) {
		DEBUG(1, ("invalid key in g_lock.tdb, ignoring\n"));
		return 0;
	}
	return state->fn((char *)rec->key.dptr, state->private_data);
}

int g_lock_locks(struct g_lock_ctx *ctx,
		 int (*fn)(const char *name, void *private_data),
		 void *private_data)
{
	struct g_lock_locks_state state;

	state.fn = fn;
	state.private_data = private_data;

	return ctx->db->traverse_read(ctx->db, g_lock_locks_fn, &state);
}

NTSTATUS g_lock_dump(struct g_lock_ctx *ctx, const char *name,
		     int (*fn)(struct server_id pid,
			       enum g_lock_type lock_type,
			       void *private_data),
		     void *private_data)
{
	TDB_DATA data;
	int i, num_locks;
	struct g_lock_rec *locks = NULL;
	bool ret;

	if (ctx->db->fetch(ctx->db, talloc_tos(), string_term_tdb_data(name),
			   &data) != 0) {
		return NT_STATUS_NOT_FOUND;
	}

	if ((data.dsize == 0) || (data.dptr == NULL)) {
		return NT_STATUS_OK;
	}

	ret = g_lock_parse(talloc_tos(), data, &num_locks, &locks);

	TALLOC_FREE(data.dptr);

	if (!ret) {
		DEBUG(10, ("g_lock_parse for %s failed\n", name));
		return NT_STATUS_INTERNAL_ERROR;
	}

	for (i=0; i<num_locks; i++) {
		if (fn(locks[i].pid, locks[i].lock_type, private_data) != 0) {
			break;
		}
	}
	TALLOC_FREE(locks);
	return NT_STATUS_OK;
}

struct g_lock_get_state {
	bool found;
	struct server_id *pid;
};

static int g_lock_get_fn(struct server_id pid, enum g_lock_type lock_type,
			 void *priv)
{
	struct g_lock_get_state *state = (struct g_lock_get_state *)priv;

	if ((lock_type & G_LOCK_PENDING) != 0) {
		return 0;
	}

	state->found = true;
	*state->pid = pid;
	return 1;
}

NTSTATUS g_lock_get(struct g_lock_ctx *ctx, const char *name,
		    struct server_id *pid)
{
	struct g_lock_get_state state;
	NTSTATUS status;

	state.found = false;
	state.pid = pid;

	status = g_lock_dump(ctx, name, g_lock_get_fn, &state);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if (!state.found) {
		return NT_STATUS_NOT_FOUND;
	}
	return NT_STATUS_OK;
}

static bool g_lock_init_all(TALLOC_CTX *mem_ctx,
			    struct tevent_context **pev,
			    struct messaging_context **pmsg,
			    const struct server_id self,
			    struct g_lock_ctx **pg_ctx)
{
	struct tevent_context *ev = NULL;
	struct messaging_context *msg = NULL;
	struct g_lock_ctx *g_ctx = NULL;

	ev = tevent_context_init(mem_ctx);
	if (ev == NULL) {
		d_fprintf(stderr, "ERROR: could not init event context\n");
		goto fail;
	}
	msg = messaging_init(mem_ctx, self, ev);
	if (msg == NULL) {
		d_fprintf(stderr, "ERROR: could not init messaging context\n");
		goto fail;
	}
	g_ctx = g_lock_ctx_init(mem_ctx, msg);
	if (g_ctx == NULL) {
		d_fprintf(stderr, "ERROR: could not init g_lock context\n");
		goto fail;
	}

	*pev = ev;
	*pmsg = msg;
	*pg_ctx = g_ctx;
	return true;
fail:
	TALLOC_FREE(g_ctx);
	TALLOC_FREE(msg);
	TALLOC_FREE(ev);
	return false;
}

NTSTATUS g_lock_do(const char *name, enum g_lock_type lock_type,
		   struct timeval timeout, const struct server_id self,
		   void (*fn)(void *private_data), void *private_data)
{
	struct tevent_context *ev = NULL;
	struct messaging_context *msg = NULL;
	struct g_lock_ctx *g_ctx = NULL;
	NTSTATUS status;

	if (!g_lock_init_all(talloc_tos(), &ev, &msg, self, &g_ctx)) {
		status = NT_STATUS_ACCESS_DENIED;
		goto done;
	}

	status = g_lock_lock(g_ctx, name, lock_type, timeout);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	fn(private_data);
	g_lock_unlock(g_ctx, name);

done:
	TALLOC_FREE(g_ctx);
	TALLOC_FREE(msg);
	TALLOC_FREE(ev);
	return status;
}
