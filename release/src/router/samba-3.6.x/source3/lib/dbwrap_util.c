/* 
   Unix SMB/CIFS implementation.
   Utility functions for the dbwrap API
   Copyright (C) Volker Lendecke 2007
   Copyright (C) Michael Adam 2009
   
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

#include "includes.h"
#include "dbwrap.h"
#include "util_tdb.h"

int32_t dbwrap_fetch_int32(struct db_context *db, const char *keystr)
{
	TDB_DATA dbuf;
	int32 ret;

	if (db->fetch(db, NULL, string_term_tdb_data(keystr), &dbuf) != 0) {
		return -1;
	}

	if ((dbuf.dptr == NULL) || (dbuf.dsize != sizeof(int32_t))) {
		TALLOC_FREE(dbuf.dptr);
		return -1;
	}

	ret = IVAL(dbuf.dptr, 0);
	TALLOC_FREE(dbuf.dptr);
	return ret;
}

int dbwrap_store_int32(struct db_context *db, const char *keystr, int32_t v)
{
	struct db_record *rec;
	int32 v_store;
	NTSTATUS status;

	rec = db->fetch_locked(db, NULL, string_term_tdb_data(keystr));
	if (rec == NULL) {
		return -1;
	}

	SIVAL(&v_store, 0, v);

	status = rec->store(rec, make_tdb_data((const uint8 *)&v_store,
					       sizeof(v_store)),
			    TDB_REPLACE);
	TALLOC_FREE(rec);
	return NT_STATUS_IS_OK(status) ? 0 : -1;
}

bool dbwrap_fetch_uint32(struct db_context *db, const char *keystr,
			 uint32_t *val)
{
	TDB_DATA dbuf;

	if (db->fetch(db, NULL, string_term_tdb_data(keystr), &dbuf) != 0) {
		return false;
	}

	if ((dbuf.dptr == NULL) || (dbuf.dsize != sizeof(uint32_t))) {
		TALLOC_FREE(dbuf.dptr);
		return false;
	}

	*val = IVAL(dbuf.dptr, 0);
	TALLOC_FREE(dbuf.dptr);
	return true;
}

int dbwrap_store_uint32(struct db_context *db, const char *keystr, uint32_t v)
{
	struct db_record *rec;
	uint32 v_store;
	NTSTATUS status;

	rec = db->fetch_locked(db, NULL, string_term_tdb_data(keystr));
	if (rec == NULL) {
		return -1;
	}

	SIVAL(&v_store, 0, v);

	status = rec->store(rec, make_tdb_data((const uint8 *)&v_store,
					       sizeof(v_store)),
			    TDB_REPLACE);
	TALLOC_FREE(rec);
	return NT_STATUS_IS_OK(status) ? 0 : -1;
}

/**
 * Atomic unsigned integer change (addition):
 *
 * if value does not exist yet in the db, use *oldval as initial old value.
 * return old value in *oldval.
 * store *oldval + change_val to db.
 */

struct dbwrap_change_uint32_atomic_context {
	const char *keystr;
	uint32_t *oldval;
	uint32_t change_val;
};

static NTSTATUS dbwrap_change_uint32_atomic_action(struct db_context *db,
						   void *private_data)
{
	struct db_record *rec;
	uint32_t val = (uint32_t)-1;
	uint32_t v_store;
	NTSTATUS ret;
	struct dbwrap_change_uint32_atomic_context *state;

	state = (struct dbwrap_change_uint32_atomic_context *)private_data;

	rec = db->fetch_locked(db, NULL, string_term_tdb_data(state->keystr));
	if (!rec) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	if (rec->value.dptr == NULL) {
		val = *(state->oldval);
	} else if (rec->value.dsize == sizeof(val)) {
		val = IVAL(rec->value.dptr, 0);
		*(state->oldval) = val;
	} else {
		ret = NT_STATUS_UNSUCCESSFUL;
		goto done;
	}

	val += state->change_val;

	SIVAL(&v_store, 0, val);

	ret = rec->store(rec,
			 make_tdb_data((const uint8 *)&v_store,
				       sizeof(v_store)),
			 TDB_REPLACE);

done:
	TALLOC_FREE(rec);
	return ret;
}

NTSTATUS dbwrap_change_uint32_atomic(struct db_context *db, const char *keystr,
				     uint32_t *oldval, uint32_t change_val)
{
	NTSTATUS ret;
	struct dbwrap_change_uint32_atomic_context state;

	state.keystr = keystr;
	state.oldval = oldval;
	state.change_val = change_val;

	ret = dbwrap_change_uint32_atomic_action(db, &state);

	return ret;
}

NTSTATUS dbwrap_trans_change_uint32_atomic(struct db_context *db,
					   const char *keystr,
					   uint32_t *oldval,
					   uint32_t change_val)
{
	NTSTATUS ret;
	struct dbwrap_change_uint32_atomic_context state;

	state.keystr = keystr;
	state.oldval = oldval;
	state.change_val = change_val;

	ret = dbwrap_trans_do(db, dbwrap_change_uint32_atomic_action, &state);

	return ret;
}

/**
 * Atomic integer change (addition):
 *
 * if value does not exist yet in the db, use *oldval as initial old value.
 * return old value in *oldval.
 * store *oldval + change_val to db.
 */

struct dbwrap_change_int32_atomic_context {
	const char *keystr;
	int32_t *oldval;
	int32_t change_val;
};

static NTSTATUS dbwrap_change_int32_atomic_action(struct db_context *db,
						  void *private_data)
{
	struct db_record *rec;
	int32_t val = -1;
	int32_t v_store;
	NTSTATUS ret;
	struct dbwrap_change_int32_atomic_context *state;

	state = (struct dbwrap_change_int32_atomic_context *)private_data;

	rec = db->fetch_locked(db, NULL, string_term_tdb_data(state->keystr));
	if (!rec) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	if (rec->value.dptr == NULL) {
		val = *(state->oldval);
	} else if (rec->value.dsize == sizeof(val)) {
		val = IVAL(rec->value.dptr, 0);
		*(state->oldval) = val;
	} else {
		ret = NT_STATUS_UNSUCCESSFUL;
		goto done;
	}

	val += state->change_val;

	SIVAL(&v_store, 0, val);

	ret = rec->store(rec,
			 make_tdb_data((const uint8_t *)&v_store,
				       sizeof(v_store)),
			 TDB_REPLACE);

done:
	TALLOC_FREE(rec);
	return ret;
}

NTSTATUS dbwrap_change_int32_atomic(struct db_context *db, const char *keystr,
				    int32_t *oldval, int32_t change_val)
{
	NTSTATUS ret;
	struct dbwrap_change_int32_atomic_context state;

	state.keystr = keystr;
	state.oldval = oldval;
	state.change_val = change_val;

	ret = dbwrap_change_int32_atomic_action(db, &state);

	return ret;
}

NTSTATUS dbwrap_trans_change_int32_atomic(struct db_context *db,
					  const char *keystr,
					  int32_t *oldval,
					  int32_t change_val)
{
	NTSTATUS ret;
	struct dbwrap_change_int32_atomic_context state;

	state.keystr = keystr;
	state.oldval = oldval;
	state.change_val = change_val;

	ret = dbwrap_trans_do(db, dbwrap_change_int32_atomic_action, &state);

	return ret;
}

struct dbwrap_store_context {
	TDB_DATA *key;
	TDB_DATA *dbuf;
	int flag;
};

static NTSTATUS dbwrap_store_action(struct db_context *db, void *private_data)
{
	struct db_record *rec = NULL;
	NTSTATUS status;
	struct dbwrap_store_context *store_ctx;

	store_ctx = (struct dbwrap_store_context *)private_data;

	rec = db->fetch_locked(db, talloc_tos(), *(store_ctx->key));
	if (rec == NULL) {
		DEBUG(5, ("fetch_locked failed\n"));
		return NT_STATUS_NO_MEMORY;
	}

	status = rec->store(rec, *(store_ctx->dbuf), store_ctx->flag);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(5, ("store returned %s\n", nt_errstr(status)));
	}

	TALLOC_FREE(rec);
	return status;
}

NTSTATUS dbwrap_trans_store(struct db_context *db, TDB_DATA key, TDB_DATA dbuf,
			    int flag)
{
	NTSTATUS status;
	struct dbwrap_store_context store_ctx;

	store_ctx.key = &key;
	store_ctx.dbuf = &dbuf;
	store_ctx.flag = flag;

	status = dbwrap_trans_do(db, dbwrap_store_action, &store_ctx);

	return status;
}

static NTSTATUS dbwrap_delete_action(struct db_context * db, void *private_data)
{
	NTSTATUS status;
	struct db_record *rec;
	TDB_DATA *key = (TDB_DATA *)private_data;

	rec = db->fetch_locked(db, talloc_tos(), *key);
	if (rec == NULL) {
		DEBUG(5, ("fetch_locked failed\n"));
		return NT_STATUS_NO_MEMORY;
	}

	status = rec->delete_rec(rec);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(5, ("delete_rec returned %s\n", nt_errstr(status)));
	}

	talloc_free(rec);
	return  status;
}

NTSTATUS dbwrap_trans_delete(struct db_context *db, TDB_DATA key)
{
	NTSTATUS status;

	status = dbwrap_trans_do(db, dbwrap_delete_action, &key);

	return status;
}

NTSTATUS dbwrap_trans_store_int32(struct db_context *db, const char *keystr,
				  int32_t v)
{
	int32 v_store;

	SIVAL(&v_store, 0, v);

	return dbwrap_trans_store(db, string_term_tdb_data(keystr),
				  make_tdb_data((const uint8 *)&v_store,
						sizeof(v_store)),
				  TDB_REPLACE);
}

NTSTATUS dbwrap_trans_store_uint32(struct db_context *db, const char *keystr,
				   uint32_t v)
{
	uint32 v_store;

	SIVAL(&v_store, 0, v);

	return dbwrap_trans_store(db, string_term_tdb_data(keystr),
				  make_tdb_data((const uint8 *)&v_store,
						sizeof(v_store)),
				  TDB_REPLACE);
}

NTSTATUS dbwrap_trans_store_bystring(struct db_context *db, const char *key,
				     TDB_DATA data, int flags)
{
	return dbwrap_trans_store(db, string_term_tdb_data(key), data, flags);
}

NTSTATUS dbwrap_trans_delete_bystring(struct db_context *db, const char *key)
{
	return dbwrap_trans_delete(db, string_term_tdb_data(key));
}

/**
 * Wrap db action(s) into a transaction.
 */
NTSTATUS dbwrap_trans_do(struct db_context *db,
			 NTSTATUS (*action)(struct db_context *, void *),
			 void *private_data)
{
	int res;
	NTSTATUS status;

	res = db->transaction_start(db);
	if (res != 0) {
		DEBUG(5, ("transaction_start failed\n"));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	status = action(db, private_data);
	if (!NT_STATUS_IS_OK(status)) {
		if (db->transaction_cancel(db) != 0) {
			smb_panic("Cancelling transaction failed");
		}
		return status;
	}

	res = db->transaction_commit(db);
	if (res == 0) {
		return NT_STATUS_OK;
	}

	DEBUG(2, ("transaction_commit failed\n"));
	return NT_STATUS_INTERNAL_DB_CORRUPTION;
}

struct dbwrap_trans_traverse_action_ctx {
	int (*f)(struct db_record* rec, void* private_data);
	void* private_data;
};


static NTSTATUS dbwrap_trans_traverse_action(struct db_context* db, void* private_data)
{
	struct dbwrap_trans_traverse_action_ctx* ctx =
		(struct dbwrap_trans_traverse_action_ctx*)private_data;

	int ret = db->traverse(db, ctx->f, ctx->private_data);

	return (ret == -1) ? NT_STATUS_INTERNAL_DB_CORRUPTION : NT_STATUS_OK;
}

NTSTATUS dbwrap_trans_traverse(struct db_context *db,
			       int (*f)(struct db_record*, void*),
			       void *private_data)
{
	struct dbwrap_trans_traverse_action_ctx ctx = {
		.f = f,
		.private_data = private_data,
	};
	return dbwrap_trans_do(db, dbwrap_trans_traverse_action, &ctx);
}

NTSTATUS dbwrap_traverse(struct db_context *db,
			 int (*f)(struct db_record*, void*),
			 void *private_data,
			 int *count)
{
	int ret = db->traverse(db, f, private_data);

	if (ret < 0) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	if (count != NULL) {
		*count = ret;
	}

	return NT_STATUS_OK;
}

NTSTATUS dbwrap_delete_bystring_upper(struct db_context *db, const char *key)
{
	char *key_upper;
	NTSTATUS status;

	key_upper = talloc_strdup_upper(talloc_tos(), key);
	if (key_upper == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	status = dbwrap_delete_bystring(db, key_upper);

	talloc_free(key_upper);
	return status;
}

NTSTATUS dbwrap_store_bystring_upper(struct db_context *db, const char *key,
				     TDB_DATA data, int flags)
{
	char *key_upper;
	NTSTATUS status;

	key_upper = talloc_strdup_upper(talloc_tos(), key);
	if (key_upper == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	status = dbwrap_store_bystring(db, key_upper, data, flags);

	talloc_free(key_upper);
	return status;
}

TDB_DATA dbwrap_fetch_bystring_upper(struct db_context *db, TALLOC_CTX *mem_ctx,
				     const char *key)
{
	char *key_upper;
	TDB_DATA result;

	key_upper = talloc_strdup_upper(talloc_tos(), key);
	if (key_upper == NULL) {
		return make_tdb_data(NULL, 0);
	}

	result = dbwrap_fetch_bystring(db, mem_ctx, key_upper);

	talloc_free(key_upper);
	return result;
}
