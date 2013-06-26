/* 
   Unix SMB/CIFS implementation.
   Database interface wrapper around ctdbd
   Copyright (C) Volker Lendecke 2007-2009
   Copyright (C) Michael Adam 2009

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
#include "lib/util/tdb_wrap.h"
#include "util_tdb.h"

#ifdef CLUSTER_SUPPORT
#include "ctdb.h"
#include "ctdb_private.h"
#include "ctdbd_conn.h"
#include "g_lock.h"
#include "messages.h"

struct db_ctdb_transaction_handle {
	struct db_ctdb_ctx *ctx;
	/*
	 * we store the reads and writes done under a transaction:
	 * - one list stores both reads and writes (m_all),
	 * - the other just writes (m_write)
	 */
	struct ctdb_marshall_buffer *m_all;
	struct ctdb_marshall_buffer *m_write;
	uint32_t nesting;
	bool nested_cancel;
	char *lock_name;
};

struct db_ctdb_ctx {
	struct db_context *db;
	struct tdb_wrap *wtdb;
	uint32 db_id;
	struct db_ctdb_transaction_handle *transaction;
	struct g_lock_ctx *lock_ctx;
};

struct db_ctdb_rec {
	struct db_ctdb_ctx *ctdb_ctx;
	struct ctdb_ltdb_header header;
	struct timeval lock_time;
};

static NTSTATUS tdb_error_to_ntstatus(struct tdb_context *tdb)
{
	NTSTATUS status;
	enum TDB_ERROR tret = tdb_error(tdb);

	switch (tret) {
	case TDB_ERR_EXISTS:
		status = NT_STATUS_OBJECT_NAME_COLLISION;
		break;
	case TDB_ERR_NOEXIST:
		status = NT_STATUS_OBJECT_NAME_NOT_FOUND;
		break;
	default:
		status = NT_STATUS_INTERNAL_DB_CORRUPTION;
		break;
	}

	return status;
}


/**
 * fetch a record from the tdb, separating out the header
 * information and returning the body of the record.
 */
static NTSTATUS db_ctdb_ltdb_fetch(struct db_ctdb_ctx *db,
				   TDB_DATA key,
				   struct ctdb_ltdb_header *header,
				   TALLOC_CTX *mem_ctx,
				   TDB_DATA *data)
{
	TDB_DATA rec;
	NTSTATUS status;

	rec = tdb_fetch(db->wtdb->tdb, key);
	if (rec.dsize < sizeof(struct ctdb_ltdb_header)) {
		status = NT_STATUS_NOT_FOUND;
		if (data) {
			ZERO_STRUCTP(data);
		}
		if (header) {
			header->dmaster = (uint32_t)-1;
			header->rsn = 0;
		}
		goto done;
	}

	if (header) {
		*header = *(struct ctdb_ltdb_header *)rec.dptr;
	}

	if (data) {
		data->dsize = rec.dsize - sizeof(struct ctdb_ltdb_header);
		if (data->dsize == 0) {
			data->dptr = NULL;
		} else {
			data->dptr = (unsigned char *)talloc_memdup(mem_ctx,
					rec.dptr
					 + sizeof(struct ctdb_ltdb_header),
					data->dsize);
			if (data->dptr == NULL) {
				status = NT_STATUS_NO_MEMORY;
				goto done;
			}
		}
	}

	status = NT_STATUS_OK;

done:
	SAFE_FREE(rec.dptr);
	return status;
}

/*
 * Store a record together with the ctdb record header
 * in the local copy of the database.
 */
static NTSTATUS db_ctdb_ltdb_store(struct db_ctdb_ctx *db,
				   TDB_DATA key,
				   struct ctdb_ltdb_header *header,
				   TDB_DATA data)
{
	TALLOC_CTX *tmp_ctx = talloc_stackframe();
	TDB_DATA rec;
	int ret;

	rec.dsize = data.dsize + sizeof(struct ctdb_ltdb_header);
	rec.dptr = (uint8_t *)talloc_size(tmp_ctx, rec.dsize);

	if (rec.dptr == NULL) {
		talloc_free(tmp_ctx);
		return NT_STATUS_NO_MEMORY;
	}

	memcpy(rec.dptr, header, sizeof(struct ctdb_ltdb_header));
	memcpy(sizeof(struct ctdb_ltdb_header) + (uint8_t *)rec.dptr, data.dptr, data.dsize);

	ret = tdb_store(db->wtdb->tdb, key, rec, TDB_REPLACE);

	talloc_free(tmp_ctx);

	return (ret == 0) ? NT_STATUS_OK
			  : tdb_error_to_ntstatus(db->wtdb->tdb);

}

/*
  form a ctdb_rec_data record from a key/data pair

  note that header may be NULL. If not NULL then it is included in the data portion
  of the record
 */
static struct ctdb_rec_data *db_ctdb_marshall_record(TALLOC_CTX *mem_ctx, uint32_t reqid,	
						  TDB_DATA key, 
						  struct ctdb_ltdb_header *header,
						  TDB_DATA data)
{
	size_t length;
	struct ctdb_rec_data *d;

	length = offsetof(struct ctdb_rec_data, data) + key.dsize + 
		data.dsize + (header?sizeof(*header):0);
	d = (struct ctdb_rec_data *)talloc_size(mem_ctx, length);
	if (d == NULL) {
		return NULL;
	}
	d->length = length;
	d->reqid = reqid;
	d->keylen = key.dsize;
	memcpy(&d->data[0], key.dptr, key.dsize);
	if (header) {
		d->datalen = data.dsize + sizeof(*header);
		memcpy(&d->data[key.dsize], header, sizeof(*header));
		memcpy(&d->data[key.dsize+sizeof(*header)], data.dptr, data.dsize);
	} else {
		d->datalen = data.dsize;
		memcpy(&d->data[key.dsize], data.dptr, data.dsize);
	}
	return d;
}


/* helper function for marshalling multiple records */
static struct ctdb_marshall_buffer *db_ctdb_marshall_add(TALLOC_CTX *mem_ctx, 
					       struct ctdb_marshall_buffer *m,
					       uint64_t db_id,
					       uint32_t reqid,
					       TDB_DATA key,
					       struct ctdb_ltdb_header *header,
					       TDB_DATA data)
{
	struct ctdb_rec_data *r;
	size_t m_size, r_size;
	struct ctdb_marshall_buffer *m2 = NULL;

	r = db_ctdb_marshall_record(talloc_tos(), reqid, key, header, data);
	if (r == NULL) {
		talloc_free(m);
		return NULL;
	}

	if (m == NULL) {
		m = (struct ctdb_marshall_buffer *)talloc_zero_size(
			mem_ctx, offsetof(struct ctdb_marshall_buffer, data));
		if (m == NULL) {
			goto done;
		}
		m->db_id = db_id;
	}

	m_size = talloc_get_size(m);
	r_size = talloc_get_size(r);

	m2 = (struct ctdb_marshall_buffer *)talloc_realloc_size(
		mem_ctx, m,  m_size + r_size);
	if (m2 == NULL) {
		talloc_free(m);
		goto done;
	}

	memcpy(m_size + (uint8_t *)m2, r, r_size);

	m2->count++;

done:
	talloc_free(r);
	return m2;
}

/* we've finished marshalling, return a data blob with the marshalled records */
static TDB_DATA db_ctdb_marshall_finish(struct ctdb_marshall_buffer *m)
{
	TDB_DATA data;
	data.dptr = (uint8_t *)m;
	data.dsize = talloc_get_size(m);
	return data;
}

/* 
   loop over a marshalling buffer 

     - pass r==NULL to start
     - loop the number of times indicated by m->count
*/
static struct ctdb_rec_data *db_ctdb_marshall_loop_next(struct ctdb_marshall_buffer *m, struct ctdb_rec_data *r,
						     uint32_t *reqid,
						     struct ctdb_ltdb_header *header,
						     TDB_DATA *key, TDB_DATA *data)
{
	if (r == NULL) {
		r = (struct ctdb_rec_data *)&m->data[0];
	} else {
		r = (struct ctdb_rec_data *)(r->length + (uint8_t *)r);
	}

	if (reqid != NULL) {
		*reqid = r->reqid;
	}

	if (key != NULL) {
		key->dptr   = &r->data[0];
		key->dsize  = r->keylen;
	}
	if (data != NULL) {
		data->dptr  = &r->data[r->keylen];
		data->dsize = r->datalen;
		if (header != NULL) {
			data->dptr += sizeof(*header);
			data->dsize -= sizeof(*header);
		}
	}

	if (header != NULL) {
		if (r->datalen < sizeof(*header)) {
			return NULL;
		}
		*header = *(struct ctdb_ltdb_header *)&r->data[r->keylen];
	}

	return r;
}

/**
 * CTDB transaction destructor
 */
static int db_ctdb_transaction_destructor(struct db_ctdb_transaction_handle *h)
{
	NTSTATUS status;

	status = g_lock_unlock(h->ctx->lock_ctx, h->lock_name);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("g_lock_unlock failed: %s\n", nt_errstr(status)));
		return -1;
	}
	return 0;
}

/**
 * CTDB dbwrap API: transaction_start function
 * starts a transaction on a persistent database
 */
static int db_ctdb_transaction_start(struct db_context *db)
{
	struct db_ctdb_transaction_handle *h;
	NTSTATUS status;
	struct db_ctdb_ctx *ctx = talloc_get_type_abort(db->private_data,
							struct db_ctdb_ctx);

	if (!db->persistent) {
		DEBUG(0,("transactions not supported on non-persistent database 0x%08x\n", 
			 ctx->db_id));
		return -1;
	}

	if (ctx->transaction) {
		ctx->transaction->nesting++;
		return 0;
	}

	h = talloc_zero(db, struct db_ctdb_transaction_handle);
	if (h == NULL) {
		DEBUG(0,(__location__ " oom for transaction handle\n"));		
		return -1;
	}

	h->ctx = ctx;

	h->lock_name = talloc_asprintf(h, "transaction_db_0x%08x",
				       (unsigned int)ctx->db_id);
	if (h->lock_name == NULL) {
		DEBUG(0, ("talloc_asprintf failed\n"));
		TALLOC_FREE(h);
		return -1;
	}

	/*
	 * Wait a day, i.e. forever...
	 */
	status = g_lock_lock(ctx->lock_ctx, h->lock_name, G_LOCK_WRITE,
			     timeval_set(86400, 0));
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("g_lock_lock failed: %s\n", nt_errstr(status)));
		TALLOC_FREE(h);
		return -1;
	}

	talloc_set_destructor(h, db_ctdb_transaction_destructor);

	ctx->transaction = h;

	DEBUG(5,(__location__ " Started transaction on db 0x%08x\n", ctx->db_id));

	return 0;
}

static bool pull_newest_from_marshall_buffer(struct ctdb_marshall_buffer *buf,
					     TDB_DATA key,
					     struct ctdb_ltdb_header *pheader,
					     TALLOC_CTX *mem_ctx,
					     TDB_DATA *pdata)
{
	struct ctdb_rec_data *rec = NULL;
	struct ctdb_ltdb_header h;
	bool found = false;
	TDB_DATA data;
	int i;

	if (buf == NULL) {
		return false;
	}

	ZERO_STRUCT(h);
	ZERO_STRUCT(data);

	/*
	 * Walk the list of records written during this
	 * transaction. If we want to read one we have already
	 * written, return the last written sample. Thus we do not do
	 * a "break;" for the first hit, this record might have been
	 * overwritten later.
	 */

	for (i=0; i<buf->count; i++) {
		TDB_DATA tkey, tdata;
		uint32_t reqid;
		struct ctdb_ltdb_header hdr;

		ZERO_STRUCT(hdr);

		rec = db_ctdb_marshall_loop_next(buf, rec, &reqid, &hdr, &tkey,
						 &tdata);
		if (rec == NULL) {
			return false;
		}

		if (tdb_data_equal(key, tkey)) {
			found = true;
			data = tdata;
			h = hdr;
		}
	}

	if (!found) {
		return false;
	}

	if (pdata != NULL) {
		data.dptr = (uint8_t *)talloc_memdup(mem_ctx, data.dptr,
						     data.dsize);
		if ((data.dsize != 0) && (data.dptr == NULL)) {
			return false;
		}
		*pdata = data;
	}

	if (pheader != NULL) {
		*pheader = h;
	}

	return true;
}

/*
  fetch a record inside a transaction
 */
static int db_ctdb_transaction_fetch(struct db_ctdb_ctx *db, 
				     TALLOC_CTX *mem_ctx, 
				     TDB_DATA key, TDB_DATA *data)
{
	struct db_ctdb_transaction_handle *h = db->transaction;
	NTSTATUS status;
	bool found;

	found = pull_newest_from_marshall_buffer(h->m_write, key, NULL,
						 mem_ctx, data);
	if (found) {
		return 0;
	}

	status = db_ctdb_ltdb_fetch(h->ctx, key, NULL, mem_ctx, data);

	if (NT_STATUS_EQUAL(status, NT_STATUS_NOT_FOUND)) {
		*data = tdb_null;
	} else if (!NT_STATUS_IS_OK(status)) {
		return -1;
	}

	h->m_all = db_ctdb_marshall_add(h, h->m_all, h->ctx->db_id, 1, key,
					NULL, *data);
	if (h->m_all == NULL) {
		DEBUG(0,(__location__ " Failed to add to marshalling "
			 "record\n"));
		data->dsize = 0;
		talloc_free(data->dptr);
		return -1;
	}

	return 0;
}

/**
 * Fetch a record from a persistent database
 * without record locking and without an active transaction.
 *
 * This just fetches from the local database copy.
 * Since the databases are kept in syc cluster-wide,
 * there is no point in doing a ctdb call to fetch the
 * record from the lmaster. It does even harm since migration
 * of records bump their RSN and hence render the persistent
 * database inconsistent.
 */
static int db_ctdb_fetch_persistent(struct db_ctdb_ctx *db,
				    TALLOC_CTX *mem_ctx,
				    TDB_DATA key, TDB_DATA *data)
{
	NTSTATUS status;

	status = db_ctdb_ltdb_fetch(db, key, NULL, mem_ctx, data);

	if (NT_STATUS_EQUAL(status, NT_STATUS_NOT_FOUND)) {
		*data = tdb_null;
	} else if (!NT_STATUS_IS_OK(status)) {
		return -1;
	}

	return 0;
}

static NTSTATUS db_ctdb_store_transaction(struct db_record *rec, TDB_DATA data, int flag);
static NTSTATUS db_ctdb_delete_transaction(struct db_record *rec);

static struct db_record *db_ctdb_fetch_locked_transaction(struct db_ctdb_ctx *ctx,
							  TALLOC_CTX *mem_ctx,
							  TDB_DATA key)
{
	struct db_record *result;
	TDB_DATA ctdb_data;

	if (!(result = talloc(mem_ctx, struct db_record))) {
		DEBUG(0, ("talloc failed\n"));
		return NULL;
	}

	result->private_data = ctx->transaction;

	result->key.dsize = key.dsize;
	result->key.dptr = (uint8 *)talloc_memdup(result, key.dptr, key.dsize);
	if (result->key.dptr == NULL) {
		DEBUG(0, ("talloc failed\n"));
		TALLOC_FREE(result);
		return NULL;
	}

	result->store = db_ctdb_store_transaction;
	result->delete_rec = db_ctdb_delete_transaction;

	if (pull_newest_from_marshall_buffer(ctx->transaction->m_write, key,
					     NULL, result, &result->value)) {
		return result;
	}

	ctdb_data = tdb_fetch(ctx->wtdb->tdb, key);
	if (ctdb_data.dptr == NULL) {
		/* create the record */
		result->value = tdb_null;
		return result;
	}

	result->value.dsize = ctdb_data.dsize - sizeof(struct ctdb_ltdb_header);
	result->value.dptr = NULL;

	if ((result->value.dsize != 0)
	    && !(result->value.dptr = (uint8 *)talloc_memdup(
			 result, ctdb_data.dptr + sizeof(struct ctdb_ltdb_header),
			 result->value.dsize))) {
		DEBUG(0, ("talloc failed\n"));
		TALLOC_FREE(result);
	}

	SAFE_FREE(ctdb_data.dptr);

	return result;
}

static int db_ctdb_record_destructor(struct db_record **recp)
{
	struct db_record *rec = talloc_get_type_abort(*recp, struct db_record);
	struct db_ctdb_transaction_handle *h = talloc_get_type_abort(
		rec->private_data, struct db_ctdb_transaction_handle);
	int ret = h->ctx->db->transaction_commit(h->ctx->db);
	if (ret != 0) {
		DEBUG(0,(__location__ " transaction_commit failed\n"));
	}
	return 0;
}

/*
  auto-create a transaction for persistent databases
 */
static struct db_record *db_ctdb_fetch_locked_persistent(struct db_ctdb_ctx *ctx,
							 TALLOC_CTX *mem_ctx,
							 TDB_DATA key)
{
	int res;
	struct db_record *rec, **recp;

	res = db_ctdb_transaction_start(ctx->db);
	if (res == -1) {
		return NULL;
	}

	rec = db_ctdb_fetch_locked_transaction(ctx, mem_ctx, key);
	if (rec == NULL) {
		ctx->db->transaction_cancel(ctx->db);		
		return NULL;
	}

	/* destroy this transaction when we release the lock */
	recp = talloc(rec, struct db_record *);
	if (recp == NULL) {
		ctx->db->transaction_cancel(ctx->db);
		talloc_free(rec);
		return NULL;
	}
	*recp = rec;
	talloc_set_destructor(recp, db_ctdb_record_destructor);
	return rec;
}


/*
  stores a record inside a transaction
 */
static NTSTATUS db_ctdb_transaction_store(struct db_ctdb_transaction_handle *h,
					  TDB_DATA key, TDB_DATA data)
{
	TALLOC_CTX *tmp_ctx = talloc_new(h);
	TDB_DATA rec;
	struct ctdb_ltdb_header header;

	ZERO_STRUCT(header);

	/* we need the header so we can update the RSN */

	if (!pull_newest_from_marshall_buffer(h->m_write, key, &header,
					      NULL, NULL)) {

		rec = tdb_fetch(h->ctx->wtdb->tdb, key);

		if (rec.dptr != NULL) {
			memcpy(&header, rec.dptr,
			       sizeof(struct ctdb_ltdb_header));
			rec.dsize -= sizeof(struct ctdb_ltdb_header);

			/*
			 * a special case, we are writing the same
			 * data that is there now
			 */
			if (data.dsize == rec.dsize &&
			    memcmp(data.dptr,
				   rec.dptr + sizeof(struct ctdb_ltdb_header),
				   data.dsize) == 0) {
				SAFE_FREE(rec.dptr);
				talloc_free(tmp_ctx);
				return NT_STATUS_OK;
			}
		}
		SAFE_FREE(rec.dptr);
	}

	header.dmaster = get_my_vnn();
	header.rsn++;

	h->m_all = db_ctdb_marshall_add(h, h->m_all, h->ctx->db_id, 0, key,
					NULL, data);
	if (h->m_all == NULL) {
		DEBUG(0,(__location__ " Failed to add to marshalling "
			 "record\n"));
		talloc_free(tmp_ctx);
		return NT_STATUS_NO_MEMORY;
	}

	h->m_write = db_ctdb_marshall_add(h, h->m_write, h->ctx->db_id, 0, key, &header, data);
	if (h->m_write == NULL) {
		DEBUG(0,(__location__ " Failed to add to marshalling record\n"));
		talloc_free(tmp_ctx);
		return NT_STATUS_NO_MEMORY;
	}

	talloc_free(tmp_ctx);
	return NT_STATUS_OK;
}


/* 
   a record store inside a transaction
 */
static NTSTATUS db_ctdb_store_transaction(struct db_record *rec, TDB_DATA data, int flag)
{
	struct db_ctdb_transaction_handle *h = talloc_get_type_abort(
		rec->private_data, struct db_ctdb_transaction_handle);
	NTSTATUS status;

	status = db_ctdb_transaction_store(h, rec->key, data);
	return status;
}

/* 
   a record delete inside a transaction
 */
static NTSTATUS db_ctdb_delete_transaction(struct db_record *rec)
{
	struct db_ctdb_transaction_handle *h = talloc_get_type_abort(
		rec->private_data, struct db_ctdb_transaction_handle);
	NTSTATUS status;

	status =  db_ctdb_transaction_store(h, rec->key, tdb_null);
	return status;
}

/**
 * Fetch the db sequence number of a persistent db directly from the db.
 */
static NTSTATUS db_ctdb_fetch_db_seqnum_from_db(struct db_ctdb_ctx *db,
						uint64_t *seqnum)
{
	NTSTATUS status;
	const char *keyname = CTDB_DB_SEQNUM_KEY;
	TDB_DATA key;
	TDB_DATA data;
	struct ctdb_ltdb_header header;
	TALLOC_CTX *mem_ctx = talloc_stackframe();

	if (seqnum == NULL) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	key = string_term_tdb_data(keyname);

	status = db_ctdb_ltdb_fetch(db, key, &header, mem_ctx, &data);
	if (!NT_STATUS_IS_OK(status) &&
	    !NT_STATUS_EQUAL(status, NT_STATUS_NOT_FOUND))
	{
		goto done;
	}

	status = NT_STATUS_OK;

	if (data.dsize != sizeof(uint64_t)) {
		*seqnum = 0;
		goto done;
	}

	*seqnum = *(uint64_t *)data.dptr;

done:
	TALLOC_FREE(mem_ctx);
	return status;
}

/**
 * Store the database sequence number inside a transaction.
 */
static NTSTATUS db_ctdb_store_db_seqnum(struct db_ctdb_transaction_handle *h,
					uint64_t seqnum)
{
	NTSTATUS status;
	const char *keyname = CTDB_DB_SEQNUM_KEY;
	TDB_DATA key;
	TDB_DATA data;

	key = string_term_tdb_data(keyname);

	data.dptr = (uint8_t *)&seqnum;
	data.dsize = sizeof(uint64_t);

	status = db_ctdb_transaction_store(h, key, data);

	return status;
}

/*
  commit a transaction
 */
static int db_ctdb_transaction_commit(struct db_context *db)
{
	struct db_ctdb_ctx *ctx = talloc_get_type_abort(db->private_data,
							struct db_ctdb_ctx);
	NTSTATUS rets;
	int status;
	struct db_ctdb_transaction_handle *h = ctx->transaction;
	uint64_t old_seqnum, new_seqnum;
	int ret;

	if (h == NULL) {
		DEBUG(0,(__location__ " transaction commit with no open transaction on db 0x%08x\n", ctx->db_id));
		return -1;
	}

	if (h->nested_cancel) {
		db->transaction_cancel(db);
		DEBUG(5,(__location__ " Failed transaction commit after nested cancel\n"));
		return -1;
	}

	if (h->nesting != 0) {
		h->nesting--;
		return 0;
	}

	if (h->m_write == NULL) {
		/*
		 * No changes were made, so don't change the seqnum,
		 * don't push to other node, just exit with success.
		 */
		ret = 0;
		goto done;
	}

	DEBUG(5,(__location__ " Commit transaction on db 0x%08x\n", ctx->db_id));

	/*
	 * As the last db action before committing, bump the database sequence
	 * number. Note that this undoes all changes to the seqnum records
	 * performed under the transaction. This record is not meant to be
	 * modified by user interaction. It is for internal use only...
	 */
	rets = db_ctdb_fetch_db_seqnum_from_db(ctx, &old_seqnum);
	if (!NT_STATUS_IS_OK(rets)) {
		DEBUG(1, (__location__ " failed to fetch the db sequence number "
			  "in transaction commit on db 0x%08x\n", ctx->db_id));
		ret = -1;
		goto done;
	}

	new_seqnum = old_seqnum + 1;

	rets = db_ctdb_store_db_seqnum(h, new_seqnum);
	if (!NT_STATUS_IS_OK(rets)) {
		DEBUG(1, (__location__ "failed to store the db sequence number "
			  " in transaction commit on db 0x%08x\n", ctx->db_id));
		ret = -1;
		goto done;
	}

again:
	/* tell ctdbd to commit to the other nodes */
	rets = ctdbd_control_local(messaging_ctdbd_connection(),
				   CTDB_CONTROL_TRANS3_COMMIT,
				   h->ctx->db_id, 0,
				   db_ctdb_marshall_finish(h->m_write),
				   NULL, NULL, &status);
	if (!NT_STATUS_IS_OK(rets) || status != 0) {
		/*
		 * The TRANS3_COMMIT control should only possibly fail when a
		 * recovery has been running concurrently. In any case, the db
		 * will be the same on all nodes, either the new copy or the
		 * old copy.  This can be detected by comparing the old and new
		 * local sequence numbers.
		 */
		rets = db_ctdb_fetch_db_seqnum_from_db(ctx, &new_seqnum);
		if (!NT_STATUS_IS_OK(rets)) {
			DEBUG(1, (__location__ " failed to refetch db sequence "
				  "number after failed TRANS3_COMMIT\n"));
			ret = -1;
			goto done;
		}

		if (new_seqnum == old_seqnum) {
			/* Recovery prevented all our changes: retry. */
			goto again;
		} else if (new_seqnum != (old_seqnum + 1)) {
			DEBUG(0, (__location__ " ERROR: new_seqnum[%lu] != "
				  "old_seqnum[%lu] + (0 or 1) after failed "
				  "TRANS3_COMMIT - this should not happen!\n",
				  (unsigned long)new_seqnum,
				  (unsigned long)old_seqnum));
			ret = -1;
			goto done;
		}
		/*
		 * Recovery propagated our changes to all nodes, completing
		 * our commit for us - succeed.
		 */
	}

	ret = 0;

done:
	h->ctx->transaction = NULL;
	talloc_free(h);
	return ret;
}


/*
  cancel a transaction
 */
static int db_ctdb_transaction_cancel(struct db_context *db)
{
	struct db_ctdb_ctx *ctx = talloc_get_type_abort(db->private_data,
							struct db_ctdb_ctx);
	struct db_ctdb_transaction_handle *h = ctx->transaction;

	if (h == NULL) {
		DEBUG(0,(__location__ " transaction cancel with no open transaction on db 0x%08x\n", ctx->db_id));
		return -1;
	}

	if (h->nesting != 0) {
		h->nesting--;
		h->nested_cancel = true;
		return 0;
	}

	DEBUG(5,(__location__ " Cancel transaction on db 0x%08x\n", ctx->db_id));

	ctx->transaction = NULL;
	talloc_free(h);
	return 0;
}


static NTSTATUS db_ctdb_store(struct db_record *rec, TDB_DATA data, int flag)
{
	struct db_ctdb_rec *crec = talloc_get_type_abort(
		rec->private_data, struct db_ctdb_rec);

	return db_ctdb_ltdb_store(crec->ctdb_ctx, rec->key, &(crec->header), data);
}



#ifdef HAVE_CTDB_CONTROL_SCHEDULE_FOR_DELETION_DECL
static NTSTATUS db_ctdb_send_schedule_for_deletion(struct db_record *rec)
{
	NTSTATUS status;
	struct ctdb_control_schedule_for_deletion *dd;
	TDB_DATA indata;
	int cstatus;
	struct db_ctdb_rec *crec = talloc_get_type_abort(
		rec->private_data, struct db_ctdb_rec);

	indata.dsize = offsetof(struct ctdb_control_schedule_for_deletion, key) + rec->key.dsize;
	indata.dptr = talloc_zero_array(crec, uint8_t, indata.dsize);
	if (indata.dptr == NULL) {
		DEBUG(0, (__location__ " talloc failed!\n"));
		return NT_STATUS_NO_MEMORY;
	}

	dd = (struct ctdb_control_schedule_for_deletion *)(void *)indata.dptr;
	dd->db_id = crec->ctdb_ctx->db_id;
	dd->hdr = crec->header;
	dd->keylen = rec->key.dsize;
	memcpy(dd->key, rec->key.dptr, rec->key.dsize);

	status = ctdbd_control_local(messaging_ctdbd_connection(),
				     CTDB_CONTROL_SCHEDULE_FOR_DELETION,
				     crec->ctdb_ctx->db_id,
				     CTDB_CTRL_FLAG_NOREPLY, /* flags */
				     indata,
				     NULL, /* outdata */
				     NULL, /* errmsg */
				     &cstatus);
	talloc_free(indata.dptr);

	if (!NT_STATUS_IS_OK(status) || cstatus != 0) {
		DEBUG(1, (__location__ " Error sending local control "
			  "SCHEDULE_FOR_DELETION: %s, cstatus = %d\n",
			  nt_errstr(status), cstatus));
		if (NT_STATUS_IS_OK(status)) {
			status = NT_STATUS_UNSUCCESSFUL;
		}
	}

	return status;
}
#endif

static NTSTATUS db_ctdb_delete(struct db_record *rec)
{
	TDB_DATA data;
	NTSTATUS status;

	/*
	 * We have to store the header with empty data. TODO: Fix the
	 * tdb-level cleanup
	 */

	ZERO_STRUCT(data);

	status = db_ctdb_store(rec, data, 0);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

#ifdef HAVE_CTDB_CONTROL_SCHEDULE_FOR_DELETION_DECL
	status = db_ctdb_send_schedule_for_deletion(rec);
#endif

	return status;
}

static int db_ctdb_record_destr(struct db_record* data)
{
	struct db_ctdb_rec *crec = talloc_get_type_abort(
		data->private_data, struct db_ctdb_rec);
	int threshold;

	DEBUG(10, (DEBUGLEVEL > 10
		   ? "Unlocking db %u key %s\n"
		   : "Unlocking db %u key %.20s\n",
		   (int)crec->ctdb_ctx->db_id,
		   hex_encode_talloc(data, (unsigned char *)data->key.dptr,
			      data->key.dsize)));

	if (tdb_chainunlock(crec->ctdb_ctx->wtdb->tdb, data->key) != 0) {
		DEBUG(0, ("tdb_chainunlock failed\n"));
		return -1;
	}

	threshold = lp_ctdb_locktime_warn_threshold();
	if (threshold != 0) {
		double timediff = timeval_elapsed(&crec->lock_time);
		if ((timediff * 1000) > threshold) {
			DEBUG(0, ("Held tdb lock %f seconds\n", timediff));
		}
	}

	return 0;
}

static struct db_record *fetch_locked_internal(struct db_ctdb_ctx *ctx,
					       TALLOC_CTX *mem_ctx,
					       TDB_DATA key)
{
	struct db_record *result;
	struct db_ctdb_rec *crec;
	NTSTATUS status;
	TDB_DATA ctdb_data;
	int migrate_attempts = 0;

	if (!(result = talloc(mem_ctx, struct db_record))) {
		DEBUG(0, ("talloc failed\n"));
		return NULL;
	}

	if (!(crec = TALLOC_ZERO_P(result, struct db_ctdb_rec))) {
		DEBUG(0, ("talloc failed\n"));
		TALLOC_FREE(result);
		return NULL;
	}

	result->private_data = (void *)crec;
	crec->ctdb_ctx = ctx;

	result->key.dsize = key.dsize;
	result->key.dptr = (uint8 *)talloc_memdup(result, key.dptr, key.dsize);
	if (result->key.dptr == NULL) {
		DEBUG(0, ("talloc failed\n"));
		TALLOC_FREE(result);
		return NULL;
	}

	/*
	 * Do a blocking lock on the record
	 */
again:

	if (DEBUGLEVEL >= 10) {
		char *keystr = hex_encode_talloc(result, key.dptr, key.dsize);
		DEBUG(10, (DEBUGLEVEL > 10
			   ? "Locking db %u key %s\n"
			   : "Locking db %u key %.20s\n",
			   (int)crec->ctdb_ctx->db_id, keystr));
		TALLOC_FREE(keystr);
	}

	if (tdb_chainlock(ctx->wtdb->tdb, key) != 0) {
		DEBUG(3, ("tdb_chainlock failed\n"));
		TALLOC_FREE(result);
		return NULL;
	}

	result->store = db_ctdb_store;
	result->delete_rec = db_ctdb_delete;
	talloc_set_destructor(result, db_ctdb_record_destr);

	ctdb_data = tdb_fetch(ctx->wtdb->tdb, key);

	/*
	 * See if we have a valid record and we are the dmaster. If so, we can
	 * take the shortcut and just return it.
	 */

	if ((ctdb_data.dptr == NULL) ||
	    (ctdb_data.dsize < sizeof(struct ctdb_ltdb_header)) ||
	    ((struct ctdb_ltdb_header *)ctdb_data.dptr)->dmaster != get_my_vnn()
#if 0
	    || (random() % 2 != 0)
#endif
) {
		SAFE_FREE(ctdb_data.dptr);
		tdb_chainunlock(ctx->wtdb->tdb, key);
		talloc_set_destructor(result, NULL);

		migrate_attempts += 1;

		DEBUG(10, ("ctdb_data.dptr = %p, dmaster = %u (%u)\n",
			   ctdb_data.dptr, ctdb_data.dptr ?
			   ((struct ctdb_ltdb_header *)ctdb_data.dptr)->dmaster : -1,
			   get_my_vnn()));

		status = ctdbd_migrate(messaging_ctdbd_connection(), ctx->db_id,
				       key);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(5, ("ctdb_migrate failed: %s\n",
				  nt_errstr(status)));
			TALLOC_FREE(result);
			return NULL;
		}
		/* now its migrated, try again */
		goto again;
	}

	if (migrate_attempts > 10) {
		DEBUG(0, ("db_ctdb_fetch_locked needed %d attempts\n",
			  migrate_attempts));
	}

	GetTimeOfDay(&crec->lock_time);

	memcpy(&crec->header, ctdb_data.dptr, sizeof(crec->header));

	result->value.dsize = ctdb_data.dsize - sizeof(crec->header);
	result->value.dptr = NULL;

	if ((result->value.dsize != 0)
	    && !(result->value.dptr = (uint8 *)talloc_memdup(
			 result, ctdb_data.dptr + sizeof(crec->header),
			 result->value.dsize))) {
		DEBUG(0, ("talloc failed\n"));
		TALLOC_FREE(result);
	}

	SAFE_FREE(ctdb_data.dptr);

	return result;
}

static struct db_record *db_ctdb_fetch_locked(struct db_context *db,
					      TALLOC_CTX *mem_ctx,
					      TDB_DATA key)
{
	struct db_ctdb_ctx *ctx = talloc_get_type_abort(db->private_data,
							struct db_ctdb_ctx);

	if (ctx->transaction != NULL) {
		return db_ctdb_fetch_locked_transaction(ctx, mem_ctx, key);
	}

	if (db->persistent) {
		return db_ctdb_fetch_locked_persistent(ctx, mem_ctx, key);
	}

	return fetch_locked_internal(ctx, mem_ctx, key);
}

/*
  fetch (unlocked, no migration) operation on ctdb
 */
static int db_ctdb_fetch(struct db_context *db, TALLOC_CTX *mem_ctx,
			 TDB_DATA key, TDB_DATA *data)
{
	struct db_ctdb_ctx *ctx = talloc_get_type_abort(db->private_data,
							struct db_ctdb_ctx);
	NTSTATUS status;
	TDB_DATA ctdb_data;

	if (ctx->transaction) {
		return db_ctdb_transaction_fetch(ctx, mem_ctx, key, data);
	}

	if (db->persistent) {
		return db_ctdb_fetch_persistent(ctx, mem_ctx, key, data);
	}

	/* try a direct fetch */
	ctdb_data = tdb_fetch(ctx->wtdb->tdb, key);

	/*
	 * See if we have a valid record and we are the dmaster. If so, we can
	 * take the shortcut and just return it.
	 * we bypass the dmaster check for persistent databases
	 */
	if ((ctdb_data.dptr != NULL) &&
	    (ctdb_data.dsize >= sizeof(struct ctdb_ltdb_header)) &&
	    ((struct ctdb_ltdb_header *)ctdb_data.dptr)->dmaster == get_my_vnn())
	{
		/* we are the dmaster - avoid the ctdb protocol op */

		data->dsize = ctdb_data.dsize - sizeof(struct ctdb_ltdb_header);
		if (data->dsize == 0) {
			SAFE_FREE(ctdb_data.dptr);
			data->dptr = NULL;
			return 0;
		}

		data->dptr = (uint8 *)talloc_memdup(
			mem_ctx, ctdb_data.dptr+sizeof(struct ctdb_ltdb_header),
			data->dsize);

		SAFE_FREE(ctdb_data.dptr);

		if (data->dptr == NULL) {
			return -1;
		}
		return 0;
	}

	SAFE_FREE(ctdb_data.dptr);

	/* we weren't able to get it locally - ask ctdb to fetch it for us */
	status = ctdbd_fetch(messaging_ctdbd_connection(), ctx->db_id, key,
			     mem_ctx, data);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(5, ("ctdbd_fetch failed: %s\n", nt_errstr(status)));
		return -1;
	}

	return 0;
}

struct traverse_state {
	struct db_context *db;
	int (*fn)(struct db_record *rec, void *private_data);
	void *private_data;
};

static void traverse_callback(TDB_DATA key, TDB_DATA data, void *private_data)
{
	struct traverse_state *state = (struct traverse_state *)private_data;
	struct db_record *rec;
	TALLOC_CTX *tmp_ctx = talloc_new(state->db);
	/* we have to give them a locked record to prevent races */
	rec = db_ctdb_fetch_locked(state->db, tmp_ctx, key);
	if (rec && rec->value.dsize > 0) {
		state->fn(rec, state->private_data);
	}
	talloc_free(tmp_ctx);
}

static int traverse_persistent_callback(TDB_CONTEXT *tdb, TDB_DATA kbuf, TDB_DATA dbuf,
					void *private_data)
{
	struct traverse_state *state = (struct traverse_state *)private_data;
	struct db_record *rec;
	TALLOC_CTX *tmp_ctx = talloc_new(state->db);
	int ret = 0;
	/* we have to give them a locked record to prevent races */
	rec = db_ctdb_fetch_locked(state->db, tmp_ctx, kbuf);
	if (rec && rec->value.dsize > 0) {
		ret = state->fn(rec, state->private_data);
	}
	talloc_free(tmp_ctx);
	return ret;
}

/* wrapper to use traverse_persistent_callback with dbwrap */
static int traverse_persistent_callback_dbwrap(struct db_record *rec, void* data)
{
	return traverse_persistent_callback(NULL, rec->key, rec->value, data);
}


static int db_ctdb_traverse(struct db_context *db,
			    int (*fn)(struct db_record *rec,
				      void *private_data),
			    void *private_data)
{
        struct db_ctdb_ctx *ctx = talloc_get_type_abort(db->private_data,
                                                        struct db_ctdb_ctx);
	struct traverse_state state;

	state.db = db;
	state.fn = fn;
	state.private_data = private_data;

	if (db->persistent) {
		struct tdb_context *ltdb = ctx->wtdb->tdb;
		int ret;

		/* for persistent databases we don't need to do a ctdb traverse,
		   we can do a faster local traverse */
		ret = tdb_traverse(ltdb, traverse_persistent_callback, &state);
		if (ret < 0) {
			return ret;
		}
		if (ctx->transaction && ctx->transaction->m_write) {
			/*
			 * we now have to handle keys not yet
			 * present at transaction start
			 */
			struct db_context *newkeys = db_open_rbt(talloc_tos());
			struct ctdb_marshall_buffer *mbuf = ctx->transaction->m_write;
			struct ctdb_rec_data *rec=NULL;
			NTSTATUS status;
			int i;
			int count = 0;

			if (newkeys == NULL) {
				return -1;
			}

			for (i=0; i<mbuf->count; i++) {
				TDB_DATA key;
				rec =db_ctdb_marshall_loop_next(mbuf, rec,
								NULL, NULL,
								&key, NULL);
				SMB_ASSERT(rec != NULL);

				if (!tdb_exists(ltdb, key)) {
					dbwrap_store(newkeys, key, tdb_null, 0);
				}
			}
			status = dbwrap_traverse(newkeys,
						 traverse_persistent_callback_dbwrap,
						 &state,
						 &count);
			talloc_free(newkeys);
			if (!NT_STATUS_IS_OK(status)) {
				return -1;
			}
			ret += count;
		}
		return ret;
	}


	ctdbd_traverse(ctx->db_id, traverse_callback, &state);
	return 0;
}

static NTSTATUS db_ctdb_store_deny(struct db_record *rec, TDB_DATA data, int flag)
{
	return NT_STATUS_MEDIA_WRITE_PROTECTED;
}

static NTSTATUS db_ctdb_delete_deny(struct db_record *rec)
{
	return NT_STATUS_MEDIA_WRITE_PROTECTED;
}

static void traverse_read_callback(TDB_DATA key, TDB_DATA data, void *private_data)
{
	struct traverse_state *state = (struct traverse_state *)private_data;
	struct db_record rec;
	rec.key = key;
	rec.value = data;
	rec.store = db_ctdb_store_deny;
	rec.delete_rec = db_ctdb_delete_deny;
	rec.private_data = state->db;
	state->fn(&rec, state->private_data);
}

static int traverse_persistent_callback_read(TDB_CONTEXT *tdb, TDB_DATA kbuf, TDB_DATA dbuf,
					void *private_data)
{
	struct traverse_state *state = (struct traverse_state *)private_data;
	struct db_record rec;
	rec.key = kbuf;
	rec.value = dbuf;
	rec.store = db_ctdb_store_deny;
	rec.delete_rec = db_ctdb_delete_deny;
	rec.private_data = state->db;

	if (rec.value.dsize <= sizeof(struct ctdb_ltdb_header)) {
		/* a deleted record */
		return 0;
	}
	rec.value.dsize -= sizeof(struct ctdb_ltdb_header);
	rec.value.dptr += sizeof(struct ctdb_ltdb_header);

	return state->fn(&rec, state->private_data);
}

static int db_ctdb_traverse_read(struct db_context *db,
				 int (*fn)(struct db_record *rec,
					   void *private_data),
				 void *private_data)
{
        struct db_ctdb_ctx *ctx = talloc_get_type_abort(db->private_data,
                                                        struct db_ctdb_ctx);
	struct traverse_state state;

	state.db = db;
	state.fn = fn;
	state.private_data = private_data;

	if (db->persistent) {
		/* for persistent databases we don't need to do a ctdb traverse,
		   we can do a faster local traverse */
		return tdb_traverse_read(ctx->wtdb->tdb, traverse_persistent_callback_read, &state);
	}

	ctdbd_traverse(ctx->db_id, traverse_read_callback, &state);
	return 0;
}

static int db_ctdb_get_seqnum(struct db_context *db)
{
        struct db_ctdb_ctx *ctx = talloc_get_type_abort(db->private_data,
                                                        struct db_ctdb_ctx);
	return tdb_get_seqnum(ctx->wtdb->tdb);
}

static int db_ctdb_get_flags(struct db_context *db)
{
        struct db_ctdb_ctx *ctx = talloc_get_type_abort(db->private_data,
                                                        struct db_ctdb_ctx);
	return tdb_get_flags(ctx->wtdb->tdb);
}

struct db_context *db_open_ctdb(TALLOC_CTX *mem_ctx,
				const char *name,
				int hash_size, int tdb_flags,
				int open_flags, mode_t mode)
{
	struct db_context *result;
	struct db_ctdb_ctx *db_ctdb;
	char *db_path;
	struct ctdbd_connection *conn;

	if (!lp_clustering()) {
		DEBUG(10, ("Clustering disabled -- no ctdb\n"));
		return NULL;
	}

	if (!(result = TALLOC_ZERO_P(mem_ctx, struct db_context))) {
		DEBUG(0, ("talloc failed\n"));
		TALLOC_FREE(result);
		return NULL;
	}

	if (!(db_ctdb = TALLOC_P(result, struct db_ctdb_ctx))) {
		DEBUG(0, ("talloc failed\n"));
		TALLOC_FREE(result);
		return NULL;
	}

	db_ctdb->transaction = NULL;
	db_ctdb->db = result;

	conn = messaging_ctdbd_connection();
	if (conn == NULL) {
		DEBUG(1, ("Could not connect to ctdb\n"));
		TALLOC_FREE(result);
		return NULL;
	}

	if (!NT_STATUS_IS_OK(ctdbd_db_attach(conn, name, &db_ctdb->db_id, tdb_flags))) {
		DEBUG(0, ("ctdbd_db_attach failed for %s\n", name));
		TALLOC_FREE(result);
		return NULL;
	}

	db_path = ctdbd_dbpath(conn, db_ctdb, db_ctdb->db_id);

	result->persistent = ((tdb_flags & TDB_CLEAR_IF_FIRST) == 0);

	/* only pass through specific flags */
	tdb_flags &= TDB_SEQNUM;

	/* honor permissions if user has specified O_CREAT */
	if (open_flags & O_CREAT) {
		chmod(db_path, mode);
	}

	db_ctdb->wtdb = tdb_wrap_open(db_ctdb, db_path, hash_size, tdb_flags, O_RDWR, 0);
	if (db_ctdb->wtdb == NULL) {
		DEBUG(0, ("Could not open tdb %s: %s\n", db_path, strerror(errno)));
		TALLOC_FREE(result);
		return NULL;
	}
	talloc_free(db_path);

	if (result->persistent) {
		db_ctdb->lock_ctx = g_lock_ctx_init(db_ctdb,
						    ctdb_conn_msg_ctx(conn));
		if (db_ctdb->lock_ctx == NULL) {
			DEBUG(0, ("g_lock_ctx_init failed\n"));
			TALLOC_FREE(result);
			return NULL;
		}
	}

	result->private_data = (void *)db_ctdb;
	result->fetch_locked = db_ctdb_fetch_locked;
	result->fetch = db_ctdb_fetch;
	result->traverse = db_ctdb_traverse;
	result->traverse_read = db_ctdb_traverse_read;
	result->get_seqnum = db_ctdb_get_seqnum;
	result->get_flags = db_ctdb_get_flags;
	result->transaction_start = db_ctdb_transaction_start;
	result->transaction_commit = db_ctdb_transaction_commit;
	result->transaction_cancel = db_ctdb_transaction_cancel;

	DEBUG(3,("db_open_ctdb: opened database '%s' with dbid 0x%x\n",
		 name, db_ctdb->db_id));

	return result;
}
#endif
