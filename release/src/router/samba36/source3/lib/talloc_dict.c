/*
   Unix SMB/CIFS implementation.
   Little dictionary style data structure based on dbwrap_rbt
   Copyright (C) Volker Lendecke 2009

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
#include "talloc_dict.h"
#include "util_tdb.h"

struct talloc_dict {
	struct db_context *db;
};

struct talloc_dict *talloc_dict_init(TALLOC_CTX *mem_ctx)
{
	struct talloc_dict *result;

	result = talloc(mem_ctx, struct talloc_dict);
	if (result == NULL) {
		return NULL;
	}
	result->db = db_open_rbt(result);
	if (result->db == NULL) {
		TALLOC_FREE(result);
		return NULL;
	}
	return result;
}

/*
 * Add a talloced object to the dict. Nulls out the pointer to indicate that
 * the talloc ownership has been taken. If an object for "key" already exists,
 * the existing object is talloc_free()ed and overwritten by the new
 * object. If "data" is NULL, object for key "key" is deleted. Return false
 * for "no memory".
 */

bool talloc_dict_set(struct talloc_dict *dict, DATA_BLOB key, void *pdata)
{
	struct db_record *rec;
	NTSTATUS status = NT_STATUS_OK;
	void *data = *(void **)pdata;

	rec = dict->db->fetch_locked(dict->db, talloc_tos(),
				     make_tdb_data(key.data, key.length));
	if (rec == NULL) {
		return false;
	}
	if (rec->value.dsize != 0) {
		void *old_data;
		if (rec->value.dsize != sizeof(void *)) {
			TALLOC_FREE(rec);
			return false;
		}
		old_data = *(void **)(rec->value.dptr);
		TALLOC_FREE(old_data);
		if (data == NULL) {
			status = rec->delete_rec(rec);
		}
	}
	if (data != NULL) {
		void *mydata = talloc_move(dict->db, &data);
		*(void **)pdata = NULL;
		status = rec->store(rec, make_tdb_data((uint8_t *)&mydata,
						       sizeof(mydata)), 0);
	}
	TALLOC_FREE(rec);
	return NT_STATUS_IS_OK(status);
}

/*
 * Fetch a talloced object. If "mem_ctx!=NULL", talloc_move the object there
 * and delete it from the dict.
 */

void *talloc_dict_fetch(struct talloc_dict *dict, DATA_BLOB key,
			TALLOC_CTX *mem_ctx)
{
	struct db_record *rec;
	void *result;

	rec = dict->db->fetch_locked(dict->db, talloc_tos(),
				     make_tdb_data(key.data, key.length));
	if (rec == NULL) {
		return NULL;
	}
	if (rec->value.dsize != sizeof(void *)) {
		TALLOC_FREE(rec);
		return NULL;
	}
	result = *(void **)rec->value.dptr;

	if (mem_ctx != NULL) {
		NTSTATUS status;
		status = rec->delete_rec(rec);
		if (!NT_STATUS_IS_OK(status)) {
			TALLOC_FREE(rec);
			return NULL;
		}
		result = talloc_move(mem_ctx, &result);
	}

	return result;
}

struct talloc_dict_traverse_state {
	int (*fn)(DATA_BLOB key, void *data, void *private_data);
	void *private_data;
};

static int talloc_dict_traverse_fn(struct db_record *rec, void *private_data)
{
	struct talloc_dict_traverse_state *state =
		(struct talloc_dict_traverse_state *)private_data;

	if (rec->value.dsize != sizeof(void *)) {
		return -1;
	}
	return state->fn(data_blob_const(rec->key.dptr, rec->key.dsize),
			 *(void **)rec->value.dptr, state->private_data);
}

/*
 * Traverse a talloc_dict. If "fn" returns non-null, quit the traverse
 */

int talloc_dict_traverse(struct talloc_dict *dict,
			 int (*fn)(DATA_BLOB key, void *data,
				   void *private_data),
			 void *private_data)
{
	struct talloc_dict_traverse_state state;
	state.fn = fn;
	state.private_data = private_data;
	return dict->db->traverse(dict->db, talloc_dict_traverse_fn, &state);
}
