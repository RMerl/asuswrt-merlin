/* 
   Unix SMB/CIFS implementation.

   Generic, persistent and shared between processes cache mechanism for use
   by various parts of the Samba code

   Copyright (C) Rafal Szczesniak    2002
   Copyright (C) Volker Lendecke     2009

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
#include "system/glob.h"
#include "util_tdb.h"

#undef  DBGC_CLASS
#define DBGC_CLASS DBGC_TDB

#define TIMEOUT_LEN 12
#define CACHE_DATA_FMT	"%12u/"
#define READ_CACHE_DATA_FMT_TEMPLATE "%%12u/%%%us"
#define BLOB_TYPE "DATA_BLOB"
#define BLOB_TYPE_LEN 9

static struct tdb_context *cache;
static struct tdb_context *cache_notrans;

/**
 * @file gencache.c
 * @brief Generic, persistent and shared between processes cache mechanism
 *        for use by various parts of the Samba code
 *
 **/


/**
 * Cache initialisation function. Opens cache tdb file or creates
 * it if does not exist.
 *
 * @return true on successful initialisation of the cache or
 *         false on failure
 **/

static bool gencache_init(void)
{
	char* cache_fname = NULL;
	int open_flags = O_RDWR|O_CREAT;
	bool first_try = true;

	/* skip file open if it's already opened */
	if (cache) return True;

	cache_fname = lock_path("gencache.tdb");

	DEBUG(5, ("Opening cache file at %s\n", cache_fname));

again:
	cache = tdb_open_log(cache_fname, 0, TDB_DEFAULT|TDB_INCOMPATIBLE_HASH, open_flags, 0644);
	if (cache) {
		int ret;
		ret = tdb_check(cache, NULL, NULL);
		if (ret != 0) {
			tdb_close(cache);
			cache = NULL;
			if (!first_try) {
				DEBUG(0, ("gencache_init: tdb_check(%s) failed\n",
					  cache_fname));
				return false;
			}
			first_try = false;
			DEBUG(0, ("gencache_init: tdb_check(%s) failed - retry after CLEAR_IF_FIRST\n",
				  cache_fname));
			cache = tdb_open_log(cache_fname, 0, TDB_CLEAR_IF_FIRST|TDB_INCOMPATIBLE_HASH, open_flags, 0644);
			if (cache) {
				tdb_close(cache);
				cache = NULL;
				goto again;
			}
		}
	}

	if (!cache && (errno == EACCES)) {
		open_flags = O_RDONLY;
		cache = tdb_open_log(cache_fname, 0, TDB_DEFAULT|TDB_INCOMPATIBLE_HASH, open_flags,
				     0644);
		if (cache) {
			DEBUG(5, ("gencache_init: Opening cache file %s read-only.\n", cache_fname));
		}
	}

	if (!cache) {
		DEBUG(5, ("Attempt to open gencache.tdb has failed.\n"));
		return False;
	}

	cache_fname = lock_path("gencache_notrans.tdb");

	DEBUG(5, ("Opening cache file at %s\n", cache_fname));

	cache_notrans = tdb_open_log(cache_fname, 0, TDB_CLEAR_IF_FIRST|TDB_INCOMPATIBLE_HASH,
				     open_flags, 0644);
	if (cache_notrans == NULL) {
		DEBUG(5, ("Opening %s failed: %s\n", cache_fname,
			  strerror(errno)));
		tdb_close(cache);
		cache = NULL;
		return false;
	}

	return True;
}

static TDB_DATA last_stabilize_key(void)
{
	TDB_DATA result;
	result.dptr = (uint8_t *)"@LAST_STABILIZED";
	result.dsize = 17;
	return result;
}

/**
 * Set an entry in the cache file. If there's no such
 * one, then add it.
 *
 * @param keystr string that represents a key of this entry
 * @param blob DATA_BLOB value being cached
 * @param timeout time when the value is expired
 *
 * @retval true when entry is successfuly stored
 * @retval false on failure
 **/

bool gencache_set_data_blob(const char *keystr, const DATA_BLOB *blob,
			    time_t timeout)
{
	int ret;
	TDB_DATA databuf;
	char* val;
	time_t last_stabilize;
	static int writecount;

	if (tdb_data_cmp(string_term_tdb_data(keystr),
			 last_stabilize_key()) == 0) {
		DEBUG(10, ("Can't store %s as a key\n", keystr));
		return false;
	}

	if ((keystr == NULL) || (blob == NULL)) {
		return false;
	}

	if (!gencache_init()) return False;

	val = talloc_asprintf(talloc_tos(), CACHE_DATA_FMT, (int)timeout);
	if (val == NULL) {
		return False;
	}
	val = talloc_realloc(NULL, val, char, talloc_array_length(val)-1);
	if (val == NULL) {
		return false;
	}
	val = (char *)talloc_append_blob(NULL, val, *blob);
	if (val == NULL) {
		return false;
	}

	DEBUG(10, ("Adding cache entry with key = %s and timeout ="
	           " %s (%d seconds %s)\n", keystr, ctime(&timeout),
		   (int)(timeout - time(NULL)), 
		   timeout > time(NULL) ? "ahead" : "in the past"));

	ret = tdb_store_bystring(
		cache_notrans, keystr,
		make_tdb_data((uint8_t *)val, talloc_array_length(val)),
		0);
	TALLOC_FREE(val);

	if (ret != 0) {
		return false;
	}

	/*
	 * Every 100 writes within a single process, stabilize the cache with
	 * a transaction. This is done to prevent a single transaction to
	 * become huge and chew lots of memory.
	 */
	writecount += 1;
	if (writecount > lp_parm_int(-1, "gencache", "stabilize_count", 100)) {
		gencache_stabilize();
		writecount = 0;
		goto done;
	}

	/*
	 * Every 5 minutes, call gencache_stabilize() to not let grow
	 * gencache_notrans.tdb too large.
	 */

	last_stabilize = 0;
	databuf = tdb_fetch(cache_notrans, last_stabilize_key());
	if ((databuf.dptr != NULL)
	    && (databuf.dptr[databuf.dsize-1] == '\0')) {
		last_stabilize = atoi((char *)databuf.dptr);
		SAFE_FREE(databuf.dptr);
	}
	if ((last_stabilize
	     + lp_parm_int(-1, "gencache", "stabilize_interval", 300))
	    < time(NULL)) {
		gencache_stabilize();
	}

done:
	return ret == 0;
}

/**
 * Delete one entry from the cache file.
 *
 * @param keystr string that represents a key of this entry
 *
 * @retval true upon successful deletion
 * @retval false in case of failure
 **/

bool gencache_del(const char *keystr)
{
	bool exists, was_expired;
	bool ret = false;
	DATA_BLOB value;

	if (keystr == NULL) {
		return false;
	}

	if (!gencache_init()) return False;	

	DEBUG(10, ("Deleting cache entry (key = %s)\n", keystr));

	/*
	 * We delete an element by setting its timeout to 0. This way we don't
	 * have to do a transaction on gencache.tdb every time we delete an
	 * element.
	 */

	exists = gencache_get_data_blob(keystr, &value, NULL, &was_expired);

	if (!exists && was_expired) {
		/*
		 * gencache_get_data_blob has implicitly deleted this
		 * entry, so we have to return success here.
		 */
		return true;
	}

	if (exists) {
		data_blob_free(&value);
		ret = gencache_set(keystr, "", 0);
	}
	return ret;
}

static bool gencache_pull_timeout(char *val, time_t *pres, char **pendptr)
{
	time_t res;
	char *endptr;

	if (val == NULL) {
		return false;
	}

	res = strtol(val, &endptr, 10);

	if ((endptr == NULL) || (*endptr != '/')) {
		DEBUG(2, ("Invalid gencache data format: %s\n", val));
		return false;
	}
	if (pres != NULL) {
		*pres = res;
	}
	if (pendptr != NULL) {
		*pendptr = endptr;
	}
	return true;
}

struct gencache_parse_state {
	void (*parser)(time_t timeout, DATA_BLOB blob, void *private_data);
	void *private_data;
};

static int gencache_parse_fn(TDB_DATA key, TDB_DATA data, void *private_data)
{
	struct gencache_parse_state *state;
	DATA_BLOB blob;
	time_t t;
	char *endptr;
	bool ret;

	if (data.dptr == NULL) {
		return -1;
	}
	ret = gencache_pull_timeout((char *)data.dptr, &t, &endptr);
	if (!ret) {
		return -1;
	}
	state = (struct gencache_parse_state *)private_data;
	blob = data_blob_const(
		endptr+1, data.dsize - PTR_DIFF(endptr+1, data.dptr));
	state->parser(t, blob, state->private_data);
	return 0;
}

bool gencache_parse(const char *keystr,
		    void (*parser)(time_t timeout, DATA_BLOB blob,
				   void *private_data),
		    void *private_data)
{
	struct gencache_parse_state state;
	TDB_DATA key;
	int ret;

	if (keystr == NULL) {
		return false;
	}
	if (tdb_data_cmp(string_term_tdb_data(keystr),
			 last_stabilize_key()) == 0) {
		return false;
	}
	if (!gencache_init()) {
		return false;
	}

	key = string_term_tdb_data(keystr);
	state.parser = parser;
	state.private_data = private_data;

	ret = tdb_parse_record(cache_notrans, key, gencache_parse_fn, &state);
	if (ret != -1) {
		return true;
	}
	ret = tdb_parse_record(cache, key, gencache_parse_fn, &state);
	return (ret != -1);
}

struct gencache_get_data_blob_state {
	DATA_BLOB *blob;
	time_t timeout;
	bool result;
};

static void gencache_get_data_blob_parser(time_t timeout, DATA_BLOB blob,
					  void *private_data)
{
	struct gencache_get_data_blob_state *state =
		(struct gencache_get_data_blob_state *)private_data;

	if (timeout == 0) {
		state->result = false;
		return;
	}
	state->timeout = timeout;

	if (state->blob == NULL) {
		state->result = true;
		return;
	}

	*state->blob = data_blob(blob.data, blob.length);
	if (state->blob->data == NULL) {
		state->result = false;
		return;
	}
	state->result = true;
}

/**
 * Get existing entry from the cache file.
 *
 * @param keystr string that represents a key of this entry
 * @param blob DATA_BLOB that is filled with entry's blob
 * @param timeout pointer to a time_t that is filled with entry's
 *        timeout
 *
 * @retval true when entry is successfuly fetched
 * @retval False for failure
 **/

bool gencache_get_data_blob(const char *keystr, DATA_BLOB *blob,
			    time_t *timeout, bool *was_expired)
{
	struct gencache_get_data_blob_state state;
	bool expired = false;

	state.result = false;
	state.blob = blob;

	if (!gencache_parse(keystr, gencache_get_data_blob_parser, &state)) {
		goto fail;
	}
	if (!state.result) {
		goto fail;
	}
	if (state.timeout <= time(NULL)) {
		/*
		 * We're expired, delete the entry. We can't use gencache_del
		 * here, because that uses gencache_get_data_blob for checking
		 * the existence of a record. We know the thing exists and
		 * directly store an empty value with 0 timeout.
		 */
		gencache_set(keystr, "", 0);
		expired = true;
		goto fail;
	}
	if (timeout) {
		*timeout = state.timeout;
	}

	return True;

fail:
	if (was_expired != NULL) {
		*was_expired = expired;
	}
	if (state.result && state.blob) {
		data_blob_free(state.blob);
	}
	return false;
} 

struct stabilize_state {
	bool written;
	bool error;
};
static int stabilize_fn(struct tdb_context *tdb, TDB_DATA key, TDB_DATA val,
			void *priv);

/**
 * Stabilize gencache
 *
 * Migrate the clear-if-first gencache data to the stable,
 * transaction-based gencache.tdb
 */

bool gencache_stabilize(void)
{
	struct stabilize_state state;
	int res;
	char *now;

	if (!gencache_init()) {
		return false;
	}

	res = tdb_transaction_start_nonblock(cache);
	if (res == -1) {

		if (tdb_error(cache) == TDB_ERR_NOLOCK) {
			/*
			 * Someone else already does the stabilize,
			 * this does not have to be done twice
			 */
			return true;
		}

		DEBUG(10, ("Could not start transaction on gencache.tdb: "
			   "%s\n", tdb_errorstr(cache)));
		return false;
	}
	res = tdb_transaction_start(cache_notrans);
	if (res == -1) {
		tdb_transaction_cancel(cache);
		DEBUG(10, ("Could not start transaction on "
			   "gencache_notrans.tdb: %s\n",
			   tdb_errorstr(cache_notrans)));
		return false;
	}

	state.error = false;
	state.written = false;

	res = tdb_traverse(cache_notrans, stabilize_fn, &state);
	if ((res == -1) || state.error) {
		if ((tdb_transaction_cancel(cache_notrans) == -1)
		    || (tdb_transaction_cancel(cache) == -1)) {
			smb_panic("tdb_transaction_cancel failed\n");
		}
		return false;
	}

	if (!state.written) {
		if ((tdb_transaction_cancel(cache_notrans) == -1)
		    || (tdb_transaction_cancel(cache) == -1)) {
			smb_panic("tdb_transaction_cancel failed\n");
		}
		return true;
	}

	res = tdb_transaction_commit(cache);
	if (res == -1) {
		DEBUG(10, ("tdb_transaction_commit on gencache.tdb failed: "
			   "%s\n", tdb_errorstr(cache)));
		if (tdb_transaction_cancel(cache_notrans) == -1) {
			smb_panic("tdb_transaction_cancel failed\n");
		}
		return false;
	}

	res = tdb_transaction_commit(cache_notrans);
	if (res == -1) {
		DEBUG(10, ("tdb_transaction_commit on gencache.tdb failed: "
			   "%s\n", tdb_errorstr(cache)));
		return false;
	}

	now = talloc_asprintf(talloc_tos(), "%d", (int)time(NULL));
	if (now != NULL) {
		tdb_store(cache_notrans, last_stabilize_key(),
			  string_term_tdb_data(now), 0);
		TALLOC_FREE(now);
	}

	return true;
}

static int stabilize_fn(struct tdb_context *tdb, TDB_DATA key, TDB_DATA val,
			void *priv)
{
	struct stabilize_state *state = (struct stabilize_state *)priv;
	int res;
	time_t timeout;

	if (tdb_data_cmp(key, last_stabilize_key()) == 0) {
		return 0;
	}

	if (!gencache_pull_timeout((char *)val.dptr, &timeout, NULL)) {
		DEBUG(10, ("Ignoring invalid entry\n"));
		return 0;
	}
	if ((timeout < time(NULL)) || (val.dsize == 0)) {
		res = tdb_delete(cache, key);
		if ((res == -1) && (tdb_error(cache) == TDB_ERR_NOEXIST)) {
			res = 0;
		} else {
			state->written = true;
		}
	} else {
		res = tdb_store(cache, key, val, 0);
		if (res == 0) {
			state->written = true;
		}
	}

	if (res == -1) {
		DEBUG(10, ("Transfer to gencache.tdb failed: %s\n",
			   tdb_errorstr(cache)));
		state->error = true;
		return -1;
	}

	if (tdb_delete(cache_notrans, key) == -1) {
		DEBUG(10, ("tdb_delete from gencache_notrans.tdb failed: "
			   "%s\n", tdb_errorstr(cache_notrans)));
		state->error = true;
		return -1;
	}
	return 0;
}

/**
 * Get existing entry from the cache file.
 *
 * @param keystr string that represents a key of this entry
 * @param valstr buffer that is allocated and filled with the entry value
 *        buffer's disposing must be done outside
 * @param timeout pointer to a time_t that is filled with entry's
 *        timeout
 *
 * @retval true when entry is successfuly fetched
 * @retval False for failure
 **/

bool gencache_get(const char *keystr, char **value, time_t *ptimeout)
{
	DATA_BLOB blob;
	bool ret = False;

	ret = gencache_get_data_blob(keystr, &blob, ptimeout, NULL);
	if (!ret) {
		return false;
	}
	if ((blob.data == NULL) || (blob.length == 0)) {
		SAFE_FREE(blob.data);
		return false;
	}
	if (blob.data[blob.length-1] != '\0') {
		/* Not NULL terminated, can't be a string */
		SAFE_FREE(blob.data);
		return false;
	}
	if (value) {
		*value = SMB_STRDUP((char *)blob.data);
		data_blob_free(&blob);
		if (*value == NULL) {
			return false;
		}
		return true;
	}
	data_blob_free(&blob);
	return true;
}

/**
 * Set an entry in the cache file. If there's no such
 * one, then add it.
 *
 * @param keystr string that represents a key of this entry
 * @param value text representation value being cached
 * @param timeout time when the value is expired
 *
 * @retval true when entry is successfuly stored
 * @retval false on failure
 **/

bool gencache_set(const char *keystr, const char *value, time_t timeout)
{
	DATA_BLOB blob = data_blob_const(value, strlen(value)+1);
	return gencache_set_data_blob(keystr, &blob, timeout);
}

struct gencache_iterate_blobs_state {
	void (*fn)(const char *key, DATA_BLOB value,
		   time_t timeout, void *private_data);
	const char *pattern;
	void *private_data;
	bool in_persistent;
};

static int gencache_iterate_blobs_fn(struct tdb_context *tdb, TDB_DATA key,
				     TDB_DATA data, void *priv)
{
	struct gencache_iterate_blobs_state *state =
		(struct gencache_iterate_blobs_state *)priv;
	char *keystr;
	char *free_key = NULL;
	time_t timeout;
	char *endptr;

	if (tdb_data_cmp(key, last_stabilize_key()) == 0) {
		return 0;
	}
	if (state->in_persistent && tdb_exists(cache_notrans, key)) {
		return 0;
	}

	if (key.dptr[key.dsize-1] == '\0') {
		keystr = (char *)key.dptr;
	} else {
		/* ensure 0-termination */
		keystr = SMB_STRNDUP((char *)key.dptr, key.dsize);
		free_key = keystr;
	}

	if (!gencache_pull_timeout((char *)data.dptr, &timeout, &endptr)) {
		goto done;
	}
	endptr += 1;

	if (fnmatch(state->pattern, keystr, 0) != 0) {
		goto done;
	}

	DEBUG(10, ("Calling function with arguments (key=%s, timeout=%s)\n",
		   keystr, ctime(&timeout)));

	state->fn(keystr,
		  data_blob_const(endptr,
				  data.dsize - PTR_DIFF(endptr, data.dptr)),
		  timeout, state->private_data);

 done:
	SAFE_FREE(free_key);
	return 0;
}

void gencache_iterate_blobs(void (*fn)(const char *key, DATA_BLOB value,
				       time_t timeout, void *private_data),
			    void *private_data, const char *pattern)
{
	struct gencache_iterate_blobs_state state;

	if ((fn == NULL) || (pattern == NULL) || !gencache_init()) {
		return;
	}

	DEBUG(5, ("Searching cache keys with pattern %s\n", pattern));

	state.fn = fn;
	state.pattern = pattern;
	state.private_data = private_data;

	state.in_persistent = false;
	tdb_traverse(cache_notrans, gencache_iterate_blobs_fn, &state);

	state.in_persistent = true;
	tdb_traverse(cache, gencache_iterate_blobs_fn, &state);
}

/**
 * Iterate through all entries which key matches to specified pattern
 *
 * @param fn pointer to the function that will be supplied with each single
 *        matching cache entry (key, value and timeout) as an arguments
 * @param data void pointer to an arbitrary data that is passed directly to the fn
 *        function on each call
 * @param keystr_pattern pattern the existing entries' keys are matched to
 *
 **/

struct gencache_iterate_state {
	void (*fn)(const char *key, const char *value, time_t timeout,
		   void *priv);
	void *private_data;
};

static void gencache_iterate_fn(const char *key, DATA_BLOB value,
				time_t timeout, void *private_data)
{
	struct gencache_iterate_state *state =
		(struct gencache_iterate_state *)private_data;
	char *valstr;
	char *free_val = NULL;

	if (value.data[value.length-1] == '\0') {
		valstr = (char *)value.data;
	} else {
		/* ensure 0-termination */
		valstr = SMB_STRNDUP((char *)value.data, value.length);
		free_val = valstr;
	}

	DEBUG(10, ("Calling function with arguments "
		   "(key = %s, value = %s, timeout = %s)\n",
		   key, valstr, ctime(&timeout)));

	state->fn(key, valstr, timeout, state->private_data);

	SAFE_FREE(free_val);
}

void gencache_iterate(void (*fn)(const char *key, const char *value,
				 time_t timeout, void *dptr),
                      void *private_data, const char *pattern)
{
	struct gencache_iterate_state state;

	if (fn == NULL) {
		return;
	}
	state.fn = fn;
	state.private_data = private_data;
	gencache_iterate_blobs(gencache_iterate_fn, &state, pattern);
}
