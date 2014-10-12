/* 
   Unix SMB/CIFS implementation.
   Database interface wrapper around tdb
   Copyright (C) Volker Lendecke 2005-2007
   
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
#include "dbwrap.h"
#include "lib/util/tdb_wrap.h"

struct db_tdb_ctx {
	struct tdb_wrap *wtdb;
};

static NTSTATUS db_tdb_store(struct db_record *rec, TDB_DATA data, int flag);
static NTSTATUS db_tdb_delete(struct db_record *rec);

static int db_tdb_record_destr(struct db_record* data)
{
	struct db_tdb_ctx *ctx =
		talloc_get_type_abort(data->private_data, struct db_tdb_ctx);

	/* This hex_encode_talloc() call allocates memory on data context. By way how current 
	   __talloc_free() code works, it is OK to allocate in the destructor as 
	   the children of data will be freed after call to the destructor and this 
	   new 'child' will be caught and freed correctly.
	 */
	DEBUG(10, (DEBUGLEVEL > 10
		   ? "Unlocking key %s\n" : "Unlocking key %.20s\n",
		   hex_encode_talloc(data, (unsigned char *)data->key.dptr,
			      data->key.dsize)));

	if (tdb_chainunlock(ctx->wtdb->tdb, data->key) != 0) {
		DEBUG(0, ("tdb_chainunlock failed\n"));
		return -1;
	}
	return 0;
}

struct tdb_fetch_locked_state {
	TALLOC_CTX *mem_ctx;
	struct db_record *result;
};

static int db_tdb_fetchlock_parse(TDB_DATA key, TDB_DATA data,
				  void *private_data)
{
	struct tdb_fetch_locked_state *state =
		(struct tdb_fetch_locked_state *)private_data;

	state->result = (struct db_record *)talloc_size(
		state->mem_ctx,
		sizeof(struct db_record) + key.dsize + data.dsize);

	if (state->result == NULL) {
		return 0;
	}

	state->result->key.dsize = key.dsize;
	state->result->key.dptr = ((uint8 *)state->result)
		+ sizeof(struct db_record);
	memcpy(state->result->key.dptr, key.dptr, key.dsize);

	state->result->value.dsize = data.dsize;

	if (data.dsize > 0) {
		state->result->value.dptr = state->result->key.dptr+key.dsize;
		memcpy(state->result->value.dptr, data.dptr, data.dsize);
	}
	else {
		state->result->value.dptr = NULL;
	}

	return 0;
}

static struct db_record *db_tdb_fetch_locked(struct db_context *db,
				     TALLOC_CTX *mem_ctx, TDB_DATA key)
{
	struct db_tdb_ctx *ctx = talloc_get_type_abort(db->private_data,
						       struct db_tdb_ctx);
	struct tdb_fetch_locked_state state;

	/* Do not accidently allocate/deallocate w/o need when debug level is lower than needed */
	if(DEBUGLEVEL >= 10) {
		char *keystr = hex_encode_talloc(talloc_tos(), (unsigned char*)key.dptr, key.dsize);
		DEBUG(10, (DEBUGLEVEL > 10
			   ? "Locking key %s\n" : "Locking key %.20s\n",
			   keystr));
		TALLOC_FREE(keystr);
	}

	if (tdb_chainlock(ctx->wtdb->tdb, key) != 0) {
		DEBUG(3, ("tdb_chainlock failed\n"));
		return NULL;
	}

	state.mem_ctx = mem_ctx;
	state.result = NULL;

	tdb_parse_record(ctx->wtdb->tdb, key, db_tdb_fetchlock_parse, &state);

	if (state.result == NULL) {
		db_tdb_fetchlock_parse(key, tdb_null, &state);
	}

	if (state.result == NULL) {
		tdb_chainunlock(ctx->wtdb->tdb, key);
		return NULL;
	}

	talloc_set_destructor(state.result, db_tdb_record_destr);

	state.result->private_data = talloc_reference(state.result, ctx);
	state.result->store = db_tdb_store;
	state.result->delete_rec = db_tdb_delete;

	DEBUG(10, ("Allocated locked data 0x%p\n", state.result));

	return state.result;
}

struct tdb_fetch_state {
	TALLOC_CTX *mem_ctx;
	int result;
	TDB_DATA data;
};

static int db_tdb_fetch_parse(TDB_DATA key, TDB_DATA data,
			      void *private_data)
{
	struct tdb_fetch_state *state =
		(struct tdb_fetch_state *)private_data;

	state->data.dptr = (uint8 *)talloc_memdup(state->mem_ctx, data.dptr,
						  data.dsize);
	if (state->data.dptr == NULL) {
		state->result = -1;
		return 0;
	}

	state->data.dsize = data.dsize;
	return 0;
}

static int db_tdb_fetch(struct db_context *db, TALLOC_CTX *mem_ctx,
			TDB_DATA key, TDB_DATA *pdata)
{
	struct db_tdb_ctx *ctx = talloc_get_type_abort(
		db->private_data, struct db_tdb_ctx);

	struct tdb_fetch_state state;

	state.mem_ctx = mem_ctx;
	state.result = 0;
	state.data = tdb_null;

	tdb_parse_record(ctx->wtdb->tdb, key, db_tdb_fetch_parse, &state);

	if (state.result == -1) {
		return -1;
	}

	*pdata = state.data;
	return 0;
}

static int db_tdb_parse(struct db_context *db, TDB_DATA key,
			int (*parser)(TDB_DATA key, TDB_DATA data,
				      void *private_data),
			void *private_data)
{
	struct db_tdb_ctx *ctx = talloc_get_type_abort(
		db->private_data, struct db_tdb_ctx);

	return tdb_parse_record(ctx->wtdb->tdb, key, parser, private_data);
}

static NTSTATUS db_tdb_store(struct db_record *rec, TDB_DATA data, int flag)
{
	struct db_tdb_ctx *ctx = talloc_get_type_abort(rec->private_data,
						       struct db_tdb_ctx);

	/*
	 * This has a bug: We need to replace rec->value for correct
	 * operation, but right now brlock and locking don't use the value
	 * anymore after it was stored.
	 */

	return (tdb_store(ctx->wtdb->tdb, rec->key, data, flag) == 0) ?
		NT_STATUS_OK : NT_STATUS_UNSUCCESSFUL;
}

static NTSTATUS db_tdb_delete(struct db_record *rec)
{
	struct db_tdb_ctx *ctx = talloc_get_type_abort(rec->private_data,
						       struct db_tdb_ctx);

	if (tdb_delete(ctx->wtdb->tdb, rec->key) == 0) {
		return NT_STATUS_OK;
	}

	if (tdb_error(ctx->wtdb->tdb) == TDB_ERR_NOEXIST) {
		return NT_STATUS_NOT_FOUND;
	}

	return NT_STATUS_UNSUCCESSFUL;
}

struct db_tdb_traverse_ctx {
	struct db_context *db;
	int (*f)(struct db_record *rec, void *private_data);
	void *private_data;
};

static int db_tdb_traverse_func(TDB_CONTEXT *tdb, TDB_DATA kbuf, TDB_DATA dbuf,
				void *private_data)
{
	struct db_tdb_traverse_ctx *ctx =
		(struct db_tdb_traverse_ctx *)private_data;
	struct db_record rec;

	rec.key = kbuf;
	rec.value = dbuf;
	rec.store = db_tdb_store;
	rec.delete_rec = db_tdb_delete;
	rec.private_data = ctx->db->private_data;

	return ctx->f(&rec, ctx->private_data);
}

static int db_tdb_traverse(struct db_context *db,
			   int (*f)(struct db_record *rec, void *private_data),
			   void *private_data)
{
	struct db_tdb_ctx *db_ctx =
		talloc_get_type_abort(db->private_data, struct db_tdb_ctx);
	struct db_tdb_traverse_ctx ctx;

	ctx.db = db;
	ctx.f = f;
	ctx.private_data = private_data;
	return tdb_traverse(db_ctx->wtdb->tdb, db_tdb_traverse_func, &ctx);
}

static NTSTATUS db_tdb_store_deny(struct db_record *rec, TDB_DATA data, int flag)
{
	return NT_STATUS_MEDIA_WRITE_PROTECTED;
}

static NTSTATUS db_tdb_delete_deny(struct db_record *rec)
{
	return NT_STATUS_MEDIA_WRITE_PROTECTED;
}

static int db_tdb_traverse_read_func(TDB_CONTEXT *tdb, TDB_DATA kbuf, TDB_DATA dbuf,
				void *private_data)
{
	struct db_tdb_traverse_ctx *ctx =
		(struct db_tdb_traverse_ctx *)private_data;
	struct db_record rec;

	rec.key = kbuf;
	rec.value = dbuf;
	rec.store = db_tdb_store_deny;
	rec.delete_rec = db_tdb_delete_deny;
	rec.private_data = ctx->db->private_data;

	return ctx->f(&rec, ctx->private_data);
}

static int db_tdb_traverse_read(struct db_context *db,
			   int (*f)(struct db_record *rec, void *private_data),
			   void *private_data)
{
	struct db_tdb_ctx *db_ctx =
		talloc_get_type_abort(db->private_data, struct db_tdb_ctx);
	struct db_tdb_traverse_ctx ctx;

	ctx.db = db;
	ctx.f = f;
	ctx.private_data = private_data;
	return tdb_traverse_read(db_ctx->wtdb->tdb, db_tdb_traverse_read_func, &ctx);
}

static int db_tdb_get_seqnum(struct db_context *db)

{
	struct db_tdb_ctx *db_ctx =
		talloc_get_type_abort(db->private_data, struct db_tdb_ctx);
	return tdb_get_seqnum(db_ctx->wtdb->tdb);
}

static int db_tdb_get_flags(struct db_context *db)

{
	struct db_tdb_ctx *db_ctx =
		talloc_get_type_abort(db->private_data, struct db_tdb_ctx);
	return tdb_get_flags(db_ctx->wtdb->tdb);
}

static int db_tdb_transaction_start(struct db_context *db)
{
	struct db_tdb_ctx *db_ctx =
		talloc_get_type_abort(db->private_data, struct db_tdb_ctx);
	return tdb_transaction_start(db_ctx->wtdb->tdb);
}

static int db_tdb_transaction_commit(struct db_context *db)
{
	struct db_tdb_ctx *db_ctx =
		talloc_get_type_abort(db->private_data, struct db_tdb_ctx);
	return tdb_transaction_commit(db_ctx->wtdb->tdb);
}

static int db_tdb_transaction_cancel(struct db_context *db)
{
	struct db_tdb_ctx *db_ctx =
		talloc_get_type_abort(db->private_data, struct db_tdb_ctx);
	return tdb_transaction_cancel(db_ctx->wtdb->tdb);
}

struct db_context *db_open_tdb(TALLOC_CTX *mem_ctx,
			       const char *name,
			       int hash_size, int tdb_flags,
			       int open_flags, mode_t mode)
{
	struct db_context *result = NULL;
	struct db_tdb_ctx *db_tdb;

	result = TALLOC_ZERO_P(mem_ctx, struct db_context);
	if (result == NULL) {
		DEBUG(0, ("talloc failed\n"));
		goto fail;
	}

	result->private_data = db_tdb = TALLOC_P(result, struct db_tdb_ctx);
	if (db_tdb == NULL) {
		DEBUG(0, ("talloc failed\n"));
		goto fail;
	}

	db_tdb->wtdb = tdb_wrap_open(db_tdb, name, hash_size, tdb_flags,
				     open_flags, mode);
	if (db_tdb->wtdb == NULL) {
		DEBUG(3, ("Could not open tdb: %s\n", strerror(errno)));
		goto fail;
	}

	result->fetch_locked = db_tdb_fetch_locked;
	result->fetch = db_tdb_fetch;
	result->traverse = db_tdb_traverse;
	result->traverse_read = db_tdb_traverse_read;
	result->parse_record = db_tdb_parse;
	result->get_seqnum = db_tdb_get_seqnum;
	result->get_flags = db_tdb_get_flags;
	result->persistent = ((tdb_flags & TDB_CLEAR_IF_FIRST) == 0);
	result->transaction_start = db_tdb_transaction_start;
	result->transaction_commit = db_tdb_transaction_commit;
	result->transaction_cancel = db_tdb_transaction_cancel;
	return result;

 fail:
	if (result != NULL) {
		TALLOC_FREE(result);
	}
	return NULL;
}
