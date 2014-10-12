/* 
   Unix SMB/CIFS implementation.

   Copyright (C) Andrew Tridgell 2006

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

/*
  this is the change notify database. It implements mechanisms for
  storing current change notify waiters in a tdb, and checking if a
  given event matches any of the stored notify waiiters.
*/

#include "includes.h"
#include "system/filesys.h"
#include "librpc/gen_ndr/ndr_notify.h"
#include "dbwrap.h"
#include "smbd/smbd.h"
#include "messages.h"
#include "lib/util/tdb_wrap.h"
#include "util_tdb.h"

struct notify_context {
	struct db_context *db_recursive;
	struct db_context *db_onelevel;
	struct server_id server;
	struct messaging_context *messaging_ctx;
	struct notify_list *list;
	struct notify_array *array;
	int seqnum;
	struct sys_notify_context *sys_notify_ctx;
	TDB_DATA key;
};


struct notify_list {
	struct notify_list *next, *prev;
	void *private_data;
	void (*callback)(void *, const struct notify_event *);
	void *sys_notify_handle;
	int depth;
};

#define NOTIFY_KEY "notify array"

#define NOTIFY_ENABLE		"notify:enable"
#define NOTIFY_ENABLE_DEFAULT	True

static NTSTATUS notify_remove_all(struct notify_context *notify,
				  const struct server_id *server);
static void notify_handler(struct messaging_context *msg_ctx, void *private_data, 
			   uint32_t msg_type, struct server_id server_id, DATA_BLOB *data);

/*
  destroy the notify context
*/
static int notify_destructor(struct notify_context *notify)
{
	messaging_deregister(notify->messaging_ctx, MSG_PVFS_NOTIFY, notify);

	if (notify->list != NULL) {
		notify_remove_all(notify, &notify->server);
	}

	return 0;
}

/*
  Open up the notify.tdb database. You should close it down using
  talloc_free(). We need the messaging_ctx to allow for notifications
  via internal messages
*/
struct notify_context *notify_init(TALLOC_CTX *mem_ctx, struct server_id server, 
				   struct messaging_context *messaging_ctx,
				   struct event_context *ev,
				   connection_struct *conn)
{
	struct notify_context *notify;

	if (!lp_change_notify(conn->params)) {
		return NULL;
	}

	notify = talloc(mem_ctx, struct notify_context);
	if (notify == NULL) {
		return NULL;
	}

	notify->db_recursive = db_open(notify, lock_path("notify.tdb"),
				       0, TDB_SEQNUM|TDB_CLEAR_IF_FIRST|TDB_INCOMPATIBLE_HASH,
				       O_RDWR|O_CREAT, 0644);
	if (notify->db_recursive == NULL) {
		talloc_free(notify);
		return NULL;
	}

	notify->db_onelevel = db_open(notify, lock_path("notify_onelevel.tdb"),
				      0, TDB_CLEAR_IF_FIRST|TDB_INCOMPATIBLE_HASH,
				      O_RDWR|O_CREAT, 0644);
	if (notify->db_onelevel == NULL) {
		talloc_free(notify);
		return NULL;
	}

	notify->server = server;
	notify->messaging_ctx = messaging_ctx;
	notify->list = NULL;
	notify->array = NULL;
	notify->seqnum = notify->db_recursive->get_seqnum(
		notify->db_recursive);
	notify->key = string_term_tdb_data(NOTIFY_KEY);

	talloc_set_destructor(notify, notify_destructor);

	/* register with the messaging subsystem for the notify
	   message type */
	messaging_register(notify->messaging_ctx, notify, 
			   MSG_PVFS_NOTIFY, notify_handler);

	notify->sys_notify_ctx = sys_notify_context_create(conn, notify, ev);

	return notify;
}

bool notify_internal_parent_init(TALLOC_CTX *mem_ctx)
{
	struct tdb_wrap *db1, *db2;

	if (lp_clustering()) {
		return true;
	}

	/*
	 * Open the tdbs in the parent process (smbd) so that our
	 * CLEAR_IF_FIRST optimization in tdb_reopen_all can properly
	 * work.
	 */

	db1 = tdb_wrap_open(mem_ctx, lock_path("notify.tdb"),
			    0, TDB_SEQNUM|TDB_CLEAR_IF_FIRST|TDB_INCOMPATIBLE_HASH,
			   O_RDWR|O_CREAT, 0644);
	if (db1 == NULL) {
		DEBUG(1, ("could not open notify.tdb: %s\n", strerror(errno)));
		return false;
	}
	db2 = tdb_wrap_open(mem_ctx, lock_path("notify_onelevel.tdb"),
			    0, TDB_CLEAR_IF_FIRST|TDB_INCOMPATIBLE_HASH, O_RDWR|O_CREAT, 0644);
	if (db2 == NULL) {
		DEBUG(1, ("could not open notify_onelevel.tdb: %s\n",
			  strerror(errno)));
		TALLOC_FREE(db1);
		return false;
	}
	return true;
}

/*
  lock and fetch the record
*/
static NTSTATUS notify_fetch_locked(struct notify_context *notify, struct db_record **rec)
{
	*rec = notify->db_recursive->fetch_locked(notify->db_recursive,
						  notify, notify->key);
	if (*rec == NULL) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}
	return NT_STATUS_OK;
}

/*
  load the notify array
*/
static NTSTATUS notify_load(struct notify_context *notify, struct db_record *rec)
{
	TDB_DATA dbuf;
	DATA_BLOB blob;
	NTSTATUS status;
	int seqnum;

	seqnum = notify->db_recursive->get_seqnum(notify->db_recursive);

	if (seqnum == notify->seqnum && notify->array != NULL) {
		return NT_STATUS_OK;
	}

	notify->seqnum = seqnum;

	talloc_free(notify->array);
	notify->array = TALLOC_ZERO_P(notify, struct notify_array);
	NT_STATUS_HAVE_NO_MEMORY(notify->array);

	if (!rec) {
		if (notify->db_recursive->fetch(notify->db_recursive, notify,
						notify->key, &dbuf) != 0) {
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}
	} else {
		dbuf = rec->value;
	}

	blob.data = (uint8 *)dbuf.dptr;
	blob.length = dbuf.dsize;

	status = NT_STATUS_OK;
	if (blob.length > 0) {
		enum ndr_err_code ndr_err;
		ndr_err = ndr_pull_struct_blob(&blob, notify->array, notify->array,
					       (ndr_pull_flags_fn_t)ndr_pull_notify_array);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			/* 1. log that we got a corrupt notify_array
			 * 2. clear the variable the garbage was stored into to not trip
			 *  over it next time this method is entered with the same seqnum
			 * 3. delete it from the database */
			DEBUG(2, ("notify_array is corrupt, discarding it\n"));

			ZERO_STRUCTP(notify->array);
			if (rec != NULL) {
				rec->delete_rec(rec);
			}

		} else {
			if (DEBUGLEVEL >= 10) {
				DEBUG(10, ("notify_load:\n"));
				NDR_PRINT_DEBUG(notify_array, notify->array);
			}
		}
	}


	if (!rec) {
		talloc_free(dbuf.dptr);
	}

	return status;
}

/*
  compare notify entries for sorting
*/
static int notify_compare(const struct notify_entry *e1, const struct notify_entry *e2)
{
	return strcmp(e1->path, e2->path);
}

/*
  save the notify array
*/
static NTSTATUS notify_save(struct notify_context *notify, struct db_record *rec)
{
	TDB_DATA dbuf;
	DATA_BLOB blob;
	NTSTATUS status;
	enum ndr_err_code ndr_err;
	TALLOC_CTX *tmp_ctx;

	/* if possible, remove some depth arrays */
	while (notify->array->num_depths > 0 &&
	       notify->array->depth[notify->array->num_depths-1].num_entries == 0) {
		notify->array->num_depths--;
	}

	/* we might just be able to delete the record */
	if (notify->array->num_depths == 0) {
		return rec->delete_rec(rec);
	}

	tmp_ctx = talloc_new(notify);
	NT_STATUS_HAVE_NO_MEMORY(tmp_ctx);

	ndr_err = ndr_push_struct_blob(&blob, tmp_ctx, notify->array,
				      (ndr_push_flags_fn_t)ndr_push_notify_array);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(tmp_ctx);
		return ndr_map_error2ntstatus(ndr_err);
	}

	if (DEBUGLEVEL >= 10) {
		DEBUG(10, ("notify_save:\n"));
		NDR_PRINT_DEBUG(notify_array, notify->array);
	}

	dbuf.dptr = blob.data;
	dbuf.dsize = blob.length;

	status = rec->store(rec, dbuf, TDB_REPLACE);
	talloc_free(tmp_ctx);

	return status;
}


/*
  handle incoming notify messages
*/
static void notify_handler(struct messaging_context *msg_ctx, void *private_data, 
			   uint32_t msg_type, struct server_id server_id, DATA_BLOB *data)
{
	struct notify_context *notify = talloc_get_type(private_data, struct notify_context);
	enum ndr_err_code ndr_err;
	struct notify_event ev;
	TALLOC_CTX *tmp_ctx = talloc_new(notify);
	struct notify_list *listel;

	if (tmp_ctx == NULL) {
		return;
	}

	ndr_err = ndr_pull_struct_blob(data, tmp_ctx, &ev,
				       (ndr_pull_flags_fn_t)ndr_pull_notify_event);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(tmp_ctx);
		return;
	}

	for (listel=notify->list;listel;listel=listel->next) {
		if (listel->private_data == ev.private_data) {
			listel->callback(listel->private_data, &ev);
			break;
		}
	}

	talloc_free(tmp_ctx);	
}

/*
  callback from sys_notify telling us about changes from the OS
*/
static void sys_notify_callback(struct sys_notify_context *ctx, 
				void *ptr, struct notify_event *ev)
{
	struct notify_list *listel = talloc_get_type(ptr, struct notify_list);
	ev->private_data = listel;
	DEBUG(10, ("sys_notify_callback called with action=%d, for %s\n",
		   ev->action, ev->path));
	listel->callback(listel->private_data, ev);
}

/*
  add an entry to the notify array
*/
static NTSTATUS notify_add_array(struct notify_context *notify, struct db_record *rec,
				 struct notify_entry *e,
				 void *private_data, int depth)
{
	int i;
	struct notify_depth *d;
	struct notify_entry *ee;

	/* possibly expand the depths array */
	if (depth >= notify->array->num_depths) {
		d = talloc_realloc(notify->array, notify->array->depth, 
				   struct notify_depth, depth+1);
		NT_STATUS_HAVE_NO_MEMORY(d);
		for (i=notify->array->num_depths;i<=depth;i++) {
			ZERO_STRUCT(d[i]);
		}
		notify->array->depth = d;
		notify->array->num_depths = depth+1;
	}
	d = &notify->array->depth[depth];

	/* expand the entries array */
	ee = talloc_realloc(notify->array->depth, d->entries, struct notify_entry,
			    d->num_entries+1);
	NT_STATUS_HAVE_NO_MEMORY(ee);
	d->entries = ee;

	d->entries[d->num_entries] = *e;
	d->entries[d->num_entries].private_data = private_data;
	d->entries[d->num_entries].server = notify->server;
	d->entries[d->num_entries].path_len = strlen(e->path);
	d->num_entries++;

	d->max_mask |= e->filter;
	d->max_mask_subdir |= e->subdir_filter;

	TYPESAFE_QSORT(d->entries, d->num_entries, notify_compare);

	/* recalculate the maximum masks */
	d->max_mask = 0;
	d->max_mask_subdir = 0;

	for (i=0;i<d->num_entries;i++) {
		d->max_mask |= d->entries[i].filter;
		d->max_mask_subdir |= d->entries[i].subdir_filter;
	}

	return notify_save(notify, rec);
}

/*
  Add a non-recursive watch
*/

static void notify_add_onelevel(struct notify_context *notify,
				struct notify_entry *e, void *private_data)
{
	struct notify_entry_array *array;
	struct db_record *rec;
	DATA_BLOB blob;
	TDB_DATA dbuf;
	enum ndr_err_code ndr_err;
	NTSTATUS status;

	array = talloc_zero(talloc_tos(), struct notify_entry_array);
	if (array == NULL) {
		return;
	}

	rec = notify->db_onelevel->fetch_locked(
		notify->db_onelevel, talloc_tos(),
		make_tdb_data((uint8_t *)&e->dir_id, sizeof(e->dir_id)));
	if (rec == NULL) {
		DEBUG(10, ("notify_add_onelevel: fetch_locked for %s failed"
			   "\n", file_id_string_tos(&e->dir_id)));
		TALLOC_FREE(array);
		return;
	}

	blob.data = (uint8_t *)rec->value.dptr;
	blob.length = rec->value.dsize;

	if (blob.length > 0) {
		ndr_err = ndr_pull_struct_blob(&blob, array, array,
			(ndr_pull_flags_fn_t)ndr_pull_notify_entry_array);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			DEBUG(10, ("ndr_pull_notify_entry_array failed: %s\n",
				   ndr_errstr(ndr_err)));
			TALLOC_FREE(array);
			return;
		}
		if (DEBUGLEVEL >= 10) {
			DEBUG(10, ("notify_add_onelevel:\n"));
			NDR_PRINT_DEBUG(notify_entry_array, array);
		}
	}

	array->entries = talloc_realloc(array, array->entries,
					struct notify_entry,
					array->num_entries+1);
	if (array->entries == NULL) {
		TALLOC_FREE(array);
		return;
	}
	array->entries[array->num_entries] = *e;
	array->entries[array->num_entries].private_data = private_data;
	array->entries[array->num_entries].server = notify->server;
	array->num_entries += 1;

	ndr_err = ndr_push_struct_blob(&blob, rec, array,
		(ndr_push_flags_fn_t)ndr_push_notify_entry_array);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DEBUG(10, ("ndr_push_notify_entry_array failed: %s\n",
			   ndr_errstr(ndr_err)));
		TALLOC_FREE(array);
		return;
	}

	if (DEBUGLEVEL >= 10) {
		DEBUG(10, ("notify_add_onelevel:\n"));
		NDR_PRINT_DEBUG(notify_entry_array, array);
	}

	dbuf.dptr = blob.data;
	dbuf.dsize = blob.length;

	status = rec->store(rec, dbuf, TDB_REPLACE);
	TALLOC_FREE(array);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("notify_add_onelevel: store failed: %s\n",
			   nt_errstr(status)));
		return;
	}
	e->filter = 0;
	return;
}


/*
  add a notify watch. This is called when a notify is first setup on a open
  directory handle.
*/
NTSTATUS notify_add(struct notify_context *notify, struct notify_entry *e0,
		    void (*callback)(void *, const struct notify_event *), 
		    void *private_data)
{
	struct notify_entry e = *e0;
	NTSTATUS status;
	char *tmp_path = NULL;
	struct notify_list *listel;
	size_t len;
	int depth;
	struct db_record *rec;

	/* see if change notify is enabled at all */
	if (notify == NULL) {
		return NT_STATUS_NOT_IMPLEMENTED;
	}

	status = notify_fetch_locked(notify, &rec);
	NT_STATUS_NOT_OK_RETURN(status);

	status = notify_load(notify, rec);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(rec);
		return status;
	}

	/* cope with /. on the end of the path */
	len = strlen(e.path);
	if (len > 1 && e.path[len-1] == '.' && e.path[len-2] == '/') {
		tmp_path = talloc_strndup(notify, e.path, len-2);
		if (tmp_path == NULL) {
			status = NT_STATUS_NO_MEMORY;
			goto done;
		}
		e.path = tmp_path;
	}

	depth = count_chars(e.path, '/');

	listel = TALLOC_ZERO_P(notify, struct notify_list);
	if (listel == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}

	listel->private_data = private_data;
	listel->callback = callback;
	listel->depth = depth;
	DLIST_ADD(notify->list, listel);

	/* ignore failures from sys_notify */
	if (notify->sys_notify_ctx != NULL) {
		/*
		  this call will modify e.filter and e.subdir_filter
		  to remove bits handled by the backend
		*/
		status = sys_notify_watch(notify->sys_notify_ctx, &e,
					  sys_notify_callback, listel, 
					  &listel->sys_notify_handle);
		if (NT_STATUS_IS_OK(status)) {
			talloc_steal(listel, listel->sys_notify_handle);
		}
	}

	if (e.filter != 0) {
		notify_add_onelevel(notify, &e, private_data);
		status = NT_STATUS_OK;
	}

	/* if the system notify handler couldn't handle some of the
	   filter bits, or couldn't handle a request for recursion
	   then we need to install it in the array used for the
	   intra-samba notify handling */
	if (e.filter != 0 || e.subdir_filter != 0) {
		status = notify_add_array(notify, rec, &e, private_data, depth);
	}

done:
	talloc_free(rec);
	talloc_free(tmp_path);

	return status;
}

NTSTATUS notify_remove_onelevel(struct notify_context *notify,
				const struct file_id *fid,
				void *private_data)
{
	struct notify_entry_array *array;
	struct db_record *rec;
	DATA_BLOB blob;
	TDB_DATA dbuf;
	enum ndr_err_code ndr_err;
	NTSTATUS status;
	int i;

	if (notify == NULL) {
		return NT_STATUS_NOT_IMPLEMENTED;
	}

	array = talloc_zero(talloc_tos(), struct notify_entry_array);
	if (array == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	rec = notify->db_onelevel->fetch_locked(
		notify->db_onelevel, array,
		make_tdb_data((uint8_t *)fid, sizeof(*fid)));
	if (rec == NULL) {
		DEBUG(10, ("notify_remove_onelevel: fetch_locked for %s failed"
			   "\n", file_id_string_tos(fid)));
		TALLOC_FREE(array);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	blob.data = (uint8_t *)rec->value.dptr;
	blob.length = rec->value.dsize;

	if (blob.length > 0) {
		ndr_err = ndr_pull_struct_blob(&blob, array, array,
			(ndr_pull_flags_fn_t)ndr_pull_notify_entry_array);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			DEBUG(10, ("ndr_pull_notify_entry_array failed: %s\n",
				   ndr_errstr(ndr_err)));
			TALLOC_FREE(array);
			return ndr_map_error2ntstatus(ndr_err);
		}
		if (DEBUGLEVEL >= 10) {
			DEBUG(10, ("notify_remove_onelevel:\n"));
			NDR_PRINT_DEBUG(notify_entry_array, array);
		}
	}

	for (i=0; i<array->num_entries; i++) {
		if ((private_data == array->entries[i].private_data) &&
		    cluster_id_equal(&notify->server,
				     &array->entries[i].server)) {
			break;
		}
	}

	if (i == array->num_entries) {
		TALLOC_FREE(array);
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	array->entries[i] = array->entries[array->num_entries-1];
	array->num_entries -= 1;

	if (array->num_entries == 0) {
		rec->delete_rec(rec);
		TALLOC_FREE(array);
		return NT_STATUS_OK;
	}

	ndr_err = ndr_push_struct_blob(&blob, rec, array,
		(ndr_push_flags_fn_t)ndr_push_notify_entry_array);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DEBUG(10, ("ndr_push_notify_entry_array failed: %s\n",
			   ndr_errstr(ndr_err)));
		TALLOC_FREE(array);
		return ndr_map_error2ntstatus(ndr_err);
	}

	if (DEBUGLEVEL >= 10) {
		DEBUG(10, ("notify_add_onelevel:\n"));
		NDR_PRINT_DEBUG(notify_entry_array, array);
	}

	dbuf.dptr = blob.data;
	dbuf.dsize = blob.length;

	status = rec->store(rec, dbuf, TDB_REPLACE);
	TALLOC_FREE(array);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("notify_add_onelevel: store failed: %s\n",
			   nt_errstr(status)));
		return status;
	}
	return NT_STATUS_OK;
}

/*
  remove a notify watch. Called when the directory handle is closed
*/
NTSTATUS notify_remove(struct notify_context *notify, void *private_data)
{
	NTSTATUS status;
	struct notify_list *listel;
	int i, depth;
	struct notify_depth *d;
	struct db_record *rec;

	/* see if change notify is enabled at all */
	if (notify == NULL) {
		return NT_STATUS_NOT_IMPLEMENTED;
	}

	for (listel=notify->list;listel;listel=listel->next) {
		if (listel->private_data == private_data) {
			DLIST_REMOVE(notify->list, listel);
			break;
		}
	}
	if (listel == NULL) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	depth = listel->depth;

	talloc_free(listel);

	status = notify_fetch_locked(notify, &rec);
	NT_STATUS_NOT_OK_RETURN(status);

	status = notify_load(notify, rec);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(rec);
		return status;
	}

	if (depth >= notify->array->num_depths) {
		talloc_free(rec);
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	/* we only have to search at the depth of this element */
	d = &notify->array->depth[depth];

	for (i=0;i<d->num_entries;i++) {
		if (private_data == d->entries[i].private_data &&
		    cluster_id_equal(&notify->server, &d->entries[i].server)) {
			break;
		}
	}
	if (i == d->num_entries) {
		talloc_free(rec);
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	if (i < d->num_entries-1) {
		memmove(&d->entries[i], &d->entries[i+1], 
			sizeof(d->entries[i])*(d->num_entries-(i+1)));
	}
	d->num_entries--;

	status = notify_save(notify, rec);

	talloc_free(rec);

	return status;
}

/*
  remove all notify watches for a messaging server
*/
static NTSTATUS notify_remove_all(struct notify_context *notify,
				  const struct server_id *server)
{
	NTSTATUS status;
	int i, depth, del_count=0;
	struct db_record *rec;

	status = notify_fetch_locked(notify, &rec);
	NT_STATUS_NOT_OK_RETURN(status);

	status = notify_load(notify, rec);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(rec);
		return status;
	}

	/* we have to search for all entries across all depths, looking for matches
	   for the server id */
	for (depth=0;depth<notify->array->num_depths;depth++) {
		struct notify_depth *d = &notify->array->depth[depth];
		for (i=0;i<d->num_entries;i++) {
			if (cluster_id_equal(server, &d->entries[i].server)) {
				if (i < d->num_entries-1) {
					memmove(&d->entries[i], &d->entries[i+1], 
						sizeof(d->entries[i])*(d->num_entries-(i+1)));
				}
				i--;
				d->num_entries--;
				del_count++;
			}
		}
	}

	if (del_count > 0) {
		status = notify_save(notify, rec);
	}

	talloc_free(rec);

	return status;
}


/*
  send a notify message to another messaging server
*/
static NTSTATUS notify_send(struct notify_context *notify, struct notify_entry *e,
			    const char *path, uint32_t action)
{
	struct notify_event ev;
	DATA_BLOB data;
	NTSTATUS status;
	enum ndr_err_code ndr_err;
	TALLOC_CTX *tmp_ctx;

	ev.action = action;
	ev.path = path;
	ev.private_data = e->private_data;

	tmp_ctx = talloc_new(notify);

	ndr_err = ndr_push_struct_blob(&data, tmp_ctx, &ev,
				       (ndr_push_flags_fn_t)ndr_push_notify_event);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(tmp_ctx);
		return ndr_map_error2ntstatus(ndr_err);
	}

	status = messaging_send(notify->messaging_ctx, e->server, 
				MSG_PVFS_NOTIFY, &data);
	talloc_free(tmp_ctx);
	return status;
}

void notify_onelevel(struct notify_context *notify, uint32_t action,
		     uint32_t filter, struct file_id fid, const char *name)
{
	struct notify_entry_array *array;
	TDB_DATA dbuf;
	DATA_BLOB blob;
	bool have_dead_entries = false;
	int i;

	if (notify == NULL) {
		return;
	}

	array = talloc_zero(talloc_tos(), struct notify_entry_array);
	if (array == NULL) {
		return;
	}

	if (notify->db_onelevel->fetch(
		    notify->db_onelevel, array,
		    make_tdb_data((uint8_t *)&fid, sizeof(fid)),
		    &dbuf) == -1) {
		TALLOC_FREE(array);
		return;
	}

	blob.data = (uint8 *)dbuf.dptr;
	blob.length = dbuf.dsize;

	if (blob.length > 0) {
		enum ndr_err_code ndr_err;
		ndr_err = ndr_pull_struct_blob(&blob, array, array,
			(ndr_pull_flags_fn_t)ndr_pull_notify_entry_array);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			DEBUG(10, ("ndr_pull_notify_entry_array failed: %s\n",
				   ndr_errstr(ndr_err)));
			TALLOC_FREE(array);
			return;
		}
		if (DEBUGLEVEL >= 10) {
			DEBUG(10, ("notify_onelevel:\n"));
			NDR_PRINT_DEBUG(notify_entry_array, array);
		}
	}

	for (i=0; i<array->num_entries; i++) {
		struct notify_entry *e = &array->entries[i];

		if ((e->filter & filter) != 0) {
			NTSTATUS status;

			status = notify_send(notify, e, name, action);
			if (NT_STATUS_EQUAL(
				    status, NT_STATUS_INVALID_HANDLE)) {
				/*
				 * Mark the entry as dead. All entries have a
				 * path set. The marker used here is setting
				 * that to NULL.
				 */
				e->path = NULL;
				have_dead_entries = true;
			}
		}
	}

	if (!have_dead_entries) {
		TALLOC_FREE(array);
		return;
	}

	for (i=0; i<array->num_entries; i++) {
		struct notify_entry *e = &array->entries[i];
		if (e->path != NULL) {
			continue;
		}
		DEBUG(10, ("Deleting notify entries for process %s because "
			   "it's gone\n", procid_str_static(&e->server)));
		/*
		 * Potential TODO: This might need optimizing,
		 * notify_remove_onelevel() does a fetch_locked() operation at
		 * every call. But this would only matter if a process with
		 * MANY notifies has died without shutting down properly.
		 */
		notify_remove_onelevel(notify, &e->dir_id, e->private_data);
	}

	TALLOC_FREE(array);
	return;
}

/*
  trigger a notify message for anyone waiting on a matching event

  This function is called a lot, and needs to be very fast. The unusual data structure
  and traversal is designed to be fast in the average case, even for large numbers of
  notifies
*/
void notify_trigger(struct notify_context *notify,
		    uint32_t action, uint32_t filter, const char *path)
{
	NTSTATUS status;
	int depth;
	const char *p, *next_p;

	DEBUG(10, ("notify_trigger called action=0x%x, filter=0x%x, "
		   "path=%s\n", (unsigned)action, (unsigned)filter, path));

	/* see if change notify is enabled at all */
	if (notify == NULL) {
		return;
	}

 again:
	status = notify_load(notify, NULL);
	if (!NT_STATUS_IS_OK(status)) {
		return;
	}

	/* loop along the given path, working with each directory depth separately */
	for (depth=0,p=path;
	     p && depth < notify->array->num_depths;
	     p=next_p,depth++) {
		int p_len = p - path;
		int min_i, max_i, i;
		struct notify_depth *d = &notify->array->depth[depth];
		next_p = strchr(p+1, '/');

		/* see if there are any entries at this depth */
		if (d->num_entries == 0) continue;

		/* try to skip based on the maximum mask. If next_p is
		 NULL then we know it will be a 'this directory'
		 match, otherwise it must be a subdir match */
		if (next_p != NULL) {
			if (0 == (filter & d->max_mask_subdir)) {
				continue;
			}
		} else {
			if (0 == (filter & d->max_mask)) {
				continue;
			}
		}

		/* we know there is an entry here worth looking
		 for. Use a bisection search to find the first entry
		 with a matching path */
		min_i = 0;
		max_i = d->num_entries-1;

		while (min_i < max_i) {
			struct notify_entry *e;
			int cmp;
			i = (min_i+max_i)/2;
			e = &d->entries[i];
			cmp = strncmp(path, e->path, p_len);
			if (cmp == 0) {
				if (p_len == e->path_len) {
					max_i = i;
				} else {
					max_i = i-1;
				}
			} else if (cmp < 0) {
				max_i = i-1;
			} else {
				min_i = i+1;
			}
		}

		if (min_i != max_i) {
			/* none match */
			continue;
		}

		/* we now know that the entries start at min_i */
		for (i=min_i;i<d->num_entries;i++) {
			struct notify_entry *e = &d->entries[i];
			if (p_len != e->path_len ||
			    strncmp(path, e->path, p_len) != 0) break;
			if (next_p != NULL) {
				if (0 == (filter & e->subdir_filter)) {
					continue;
				}
			} else {
				if (0 == (filter & e->filter)) {
					continue;
				}
			}
			status = notify_send(notify, e,	path + e->path_len + 1,
					     action);

			if (NT_STATUS_EQUAL(
				    status, NT_STATUS_INVALID_HANDLE)) {
				struct server_id server = e->server;

				DEBUG(10, ("Deleting notify entries for "
					   "process %s because it's gone\n",
					   procid_str_static(&e->server)));
				notify_remove_all(notify, &server);
				goto again;
			}
		}
	}
}
