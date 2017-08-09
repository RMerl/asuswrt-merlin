/*
   Unix SMB/CIFS implementation.
   Share Database of available printers.
   Copyright (C) Simo Sorce 2010

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
#include "dbwrap.h"
#include "util_tdb.h"
#include "printer_list.h"

#define PL_DB_NAME() lock_path("printer_list.tdb")
#define PL_KEY_PREFIX "PRINTERLIST/PRN/"
#define PL_KEY_FORMAT PL_KEY_PREFIX"%s"
#define PL_TIMESTAMP_KEY "PRINTERLIST/GLOBAL/LAST_REFRESH"
#define PL_DATA_FORMAT "ddPPP"
#define PL_TSTAMP_FORMAT "dd"

static struct db_context *get_printer_list_db(void)
{
	static struct db_context *db;

	if (db != NULL) {
		return db;
	}
	db = db_open(NULL, PL_DB_NAME(), 0,
		     TDB_DEFAULT|TDB_CLEAR_IF_FIRST|TDB_INCOMPATIBLE_HASH,
		     O_RDWR|O_CREAT, 0644);
	return db;
}

bool printer_list_parent_init(void)
{
	struct db_context *db;

	/*
	 * Open the tdb in the parent process (smbd) so that our
	 * CLEAR_IF_FIRST optimization in tdb_reopen_all can properly
	 * work.
	 */

	db = get_printer_list_db();
	if (db == NULL) {
		DEBUG(1, ("could not open Printer List Database: %s\n",
			  strerror(errno)));
		return false;
	}
	return true;
}

NTSTATUS printer_list_get_printer(TALLOC_CTX *mem_ctx,
				  const char *name,
				  const char **comment,
				  const char **location,
				  time_t *last_refresh)
{
	struct db_context *db;
	char *key;
	TDB_DATA data;
	uint32_t time_h, time_l;
	char *nstr = NULL;
	char *cstr = NULL;
	char *lstr = NULL;
	NTSTATUS status;
	int ret;

	db = get_printer_list_db();
	if (db == NULL) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	key = talloc_asprintf(mem_ctx, PL_KEY_FORMAT, name);
	if (!key) {
		DEBUG(0, ("Failed to allocate key name!\n"));
		return NT_STATUS_NO_MEMORY;
	}

	data = dbwrap_fetch_bystring_upper(db, key, key);
	if (data.dptr == NULL) {
		DEBUG(6, ("Failed to fetch record! "
			  "The printer database is empty?\n"));
		status = NT_STATUS_NOT_FOUND;
		goto done;
	}

	ret = tdb_unpack(data.dptr, data.dsize,
			 PL_DATA_FORMAT,
			 &time_h, &time_l, &nstr, &cstr, &lstr);
	if (ret == -1) {
		DEBUG(1, ("Failed to un pack printer data"));
		status = NT_STATUS_INTERNAL_DB_CORRUPTION;
		goto done;
	}

	if (last_refresh) {
		*last_refresh = (time_t)(((uint64_t)time_h << 32) + time_l);
	}

	if (comment) {
		*comment = talloc_strdup(mem_ctx, cstr);
		if (!*comment) {
			DEBUG(1, ("Failed to strdup comment!\n"));
			status = NT_STATUS_NO_MEMORY;
			goto done;
		}
	}

	if (location) {
		*location = talloc_strdup(mem_ctx, lstr);
		if (*location == NULL) {
			DEBUG(1, ("Failed to strdup location!\n"));
			status = NT_STATUS_NO_MEMORY;
			goto done;
		}
	}

	status = NT_STATUS_OK;

done:
	SAFE_FREE(nstr);
	SAFE_FREE(cstr);
	TALLOC_FREE(key);
	return status;
}

NTSTATUS printer_list_set_printer(TALLOC_CTX *mem_ctx,
				  const char *name,
				  const char *comment,
				  const char *location,
				  time_t last_refresh)
{
	struct db_context *db;
	char *key;
	TDB_DATA data;
	uint64_t time_64;
	uint32_t time_h, time_l;
	const char *str = NULL;
	const char *str2 = NULL;
	NTSTATUS status;
	int len;

	db = get_printer_list_db();
	if (db == NULL) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	key = talloc_asprintf(mem_ctx, PL_KEY_FORMAT, name);
	if (!key) {
		DEBUG(0, ("Failed to allocate key name!\n"));
		return NT_STATUS_NO_MEMORY;
	}

	if (comment) {
		str = comment;
	} else {
		str = "";
	}

	if (location) {
		str2 = location;
	} else {
		str2 = "";
	}


	time_64 = last_refresh;
	time_l = time_64 & 0xFFFFFFFFL;
	time_h = time_64 >> 32;

	len = tdb_pack(NULL, 0, PL_DATA_FORMAT, time_h, time_l, name, str, str2);

	data.dptr = talloc_array(key, uint8_t, len);
	if (!data.dptr) {
		DEBUG(0, ("Failed to allocate tdb data buffer!\n"));
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}
	data.dsize = len;

	len = tdb_pack(data.dptr, data.dsize,
		       PL_DATA_FORMAT, time_h, time_l, name, str, str2);

	status = dbwrap_store_bystring_upper(db, key, data, TDB_REPLACE);

done:
	TALLOC_FREE(key);
	return status;
}

NTSTATUS printer_list_get_last_refresh(time_t *last_refresh)
{
	struct db_context *db;
	TDB_DATA data;
	uint32_t time_h, time_l;
	NTSTATUS status;
	int ret;

	db = get_printer_list_db();
	if (db == NULL) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	ZERO_STRUCT(data);

	data = dbwrap_fetch_bystring(db, talloc_tos(), PL_TIMESTAMP_KEY);
	if (data.dptr == NULL) {
		DEBUG(1, ("Failed to fetch record!\n"));
		status = NT_STATUS_NOT_FOUND;
		goto done;
	}

	ret = tdb_unpack(data.dptr, data.dsize,
			 PL_TSTAMP_FORMAT, &time_h, &time_l);
	if (ret == -1) {
		DEBUG(1, ("Failed to un pack printer data"));
		status = NT_STATUS_INTERNAL_DB_CORRUPTION;
		goto done;
	}

	*last_refresh = (time_t)(((uint64_t)time_h << 32) + time_l);
	status = NT_STATUS_OK;

done:
	return status;
}

NTSTATUS printer_list_mark_reload(void)
{
	struct db_context *db;
	TDB_DATA data;
	uint32_t time_h, time_l;
	time_t now = time_mono(NULL);
	NTSTATUS status;
	int len;

	db = get_printer_list_db();
	if (db == NULL) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	time_l = ((uint64_t)now) & 0xFFFFFFFFL;
	time_h = ((uint64_t)now) >> 32;

	len = tdb_pack(NULL, 0, PL_TSTAMP_FORMAT, time_h, time_l);

	data.dptr = talloc_array(talloc_tos(), uint8_t, len);
	if (!data.dptr) {
		DEBUG(0, ("Failed to allocate tdb data buffer!\n"));
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}
	data.dsize = len;

	len = tdb_pack(data.dptr, data.dsize,
		       PL_TSTAMP_FORMAT, time_h, time_l);

	status = dbwrap_store_bystring(db, PL_TIMESTAMP_KEY,
						data, TDB_REPLACE);

done:
	TALLOC_FREE(data.dptr);
	return status;
}

typedef int (printer_list_trv_fn_t)(struct db_record *, void *);

static NTSTATUS printer_list_traverse(printer_list_trv_fn_t *fn,
						void *private_data)
{
	struct db_context *db;
	int ret;

	db = get_printer_list_db();
	if (db == NULL) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	ret = db->traverse(db, fn, private_data);
	if (ret < 0) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	return NT_STATUS_OK;
}

struct printer_list_clean_state {
	time_t last_refresh;
	NTSTATUS status;
};

static int printer_list_clean_fn(struct db_record *rec, void *private_data)
{
	struct printer_list_clean_state *state =
			(struct printer_list_clean_state *)private_data;
	uint32_t time_h, time_l;
	time_t refresh;
	char *name;
	char *comment;
	char *location;
	int ret;

	/* skip anything that does not contain PL_DATA_FORMAT data */
	if (strncmp((char *)rec->key.dptr,
		    PL_KEY_PREFIX, sizeof(PL_KEY_PREFIX)-1)) {
		return 0;
	}

	ret = tdb_unpack(rec->value.dptr, rec->value.dsize,
			 PL_DATA_FORMAT, &time_h, &time_l, &name, &comment,
			 &location);
	if (ret == -1) {
		DEBUG(1, ("Failed to un pack printer data"));
		state->status = NT_STATUS_INTERNAL_DB_CORRUPTION;
		return -1;
	}

	SAFE_FREE(name);
	SAFE_FREE(comment);
	SAFE_FREE(location);

	refresh = (time_t)(((uint64_t)time_h << 32) + time_l);

	if (refresh < state->last_refresh) {
		state->status = rec->delete_rec(rec);
		if (!NT_STATUS_IS_OK(state->status)) {
			return -1;
		}
	}

	return 0;
}

NTSTATUS printer_list_clean_old(void)
{
	struct printer_list_clean_state state;
	NTSTATUS status;

	status = printer_list_get_last_refresh(&state.last_refresh);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	state.status = NT_STATUS_OK;

	status = printer_list_traverse(printer_list_clean_fn, &state);
	if (NT_STATUS_EQUAL(status, NT_STATUS_UNSUCCESSFUL) &&
	    !NT_STATUS_IS_OK(state.status)) {
		status = state.status;
	}

	return status;
}

struct printer_list_exec_state {
	void (*fn)(const char *, const char *, const char *, void *);
	void *private_data;
	NTSTATUS status;
};

static int printer_list_exec_fn(struct db_record *rec, void *private_data)
{
	struct printer_list_exec_state *state =
			(struct printer_list_exec_state *)private_data;
	uint32_t time_h, time_l;
	char *name;
	char *comment;
	char *location;
	int ret;

	/* always skip PL_TIMESTAMP_KEY key */
	if (strequal((const char *)rec->key.dptr, PL_TIMESTAMP_KEY)) {
		return 0;
	}

	ret = tdb_unpack(rec->value.dptr, rec->value.dsize,
			 PL_DATA_FORMAT, &time_h, &time_l, &name, &comment,
			 &location);
	if (ret == -1) {
		DEBUG(1, ("Failed to un pack printer data"));
		state->status = NT_STATUS_INTERNAL_DB_CORRUPTION;
		return -1;
	}

	state->fn(name, comment, location, state->private_data);

	SAFE_FREE(name);
	SAFE_FREE(comment);
	SAFE_FREE(location);
	return 0;
}

NTSTATUS printer_list_run_fn(void (*fn)(const char *, const char *, const char *, void *),
			     void *private_data)
{
	struct printer_list_exec_state state;
	NTSTATUS status;

	state.fn = fn;
	state.private_data = private_data;
	state.status = NT_STATUS_OK;

	status = printer_list_traverse(printer_list_exec_fn, &state);
	if (NT_STATUS_EQUAL(status, NT_STATUS_UNSUCCESSFUL) &&
	    !NT_STATUS_IS_OK(state.status)) {
		status = state.status;
	}

	return status;
}
