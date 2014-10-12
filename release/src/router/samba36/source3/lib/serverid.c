/*
   Unix SMB/CIFS implementation.
   Implementation of a reliable server_exists()
   Copyright (C) Volker Lendecke 2010

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
#include "serverid.h"
#include "util_tdb.h"
#include "dbwrap.h"
#include "lib/util/tdb_wrap.h"

struct serverid_key {
	pid_t pid;
	uint32_t vnn;
};

struct serverid_data {
	uint64_t unique_id;
	uint32_t msg_flags;
};

static struct db_context *db_ptr = NULL;

static struct db_context *serverid_init(TALLOC_CTX *mem_ctx,
					bool readonly)
{
	db_ptr = db_open(mem_ctx, lock_path("serverid.tdb"),
			 0, TDB_DEFAULT|TDB_CLEAR_IF_FIRST|TDB_INCOMPATIBLE_HASH,
			 readonly ? O_RDONLY : O_RDWR | O_CREAT,
			 0644);
	return db_ptr;
}

bool serverid_parent_init(TALLOC_CTX *mem_ctx)
{
	/*
	 * Open the tdb in the parent process (smbd) so that our
	 * CLEAR_IF_FIRST optimization in tdb_reopen_all can properly
	 * work.
	 */

	if (serverid_init(mem_ctx, false) == NULL) {
		DEBUG(1, ("could not open serverid.tdb: %s\n",
			  strerror(errno)));
		return false;
	}

	return true;
}

bool serverid_init_readonly(TALLOC_CTX *mem_ctx)
{
	if (serverid_init(mem_ctx, true) == NULL) {
		DEBUG(1, ("could not open serverid.tdb: %s\n",
			  strerror(errno)));
		return false;
	}

	return true;
}

static struct db_context *serverid_db(void)
{
	if (db_ptr != NULL) {
		return db_ptr;
	}

	return serverid_init(NULL, false);
}

static void serverid_fill_key(const struct server_id *id,
			      struct serverid_key *key)
{
	ZERO_STRUCTP(key);
	key->pid = id->pid;
	key->vnn = id->vnn;
}

bool serverid_register(const struct server_id id, uint32_t msg_flags)
{
	struct db_context *db;
	struct serverid_key key;
	struct serverid_data data;
	struct db_record *rec;
	TDB_DATA tdbkey, tdbdata;
	NTSTATUS status;
	bool ret = false;

	db = serverid_db();
	if (db == NULL) {
		return false;
	}

	serverid_fill_key(&id, &key);
	tdbkey = make_tdb_data((uint8_t *)&key, sizeof(key));

	rec = db->fetch_locked(db, talloc_tos(), tdbkey);
	if (rec == NULL) {
		DEBUG(1, ("Could not fetch_lock serverid.tdb record\n"));
		return false;
	}

	ZERO_STRUCT(data);
	data.unique_id = id.unique_id;
	data.msg_flags = msg_flags;

	tdbdata = make_tdb_data((uint8_t *)&data, sizeof(data));
	status = rec->store(rec, tdbdata, 0);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("Storing serverid.tdb record failed: %s\n",
			  nt_errstr(status)));
		goto done;
	}
	ret = true;
done:
	TALLOC_FREE(rec);
	return ret;
}

bool serverid_register_msg_flags(const struct server_id id, bool do_reg,
				 uint32_t msg_flags)
{
	struct db_context *db;
	struct serverid_key key;
	struct serverid_data *data;
	struct db_record *rec;
	TDB_DATA tdbkey;
	NTSTATUS status;
	bool ret = false;

	db = serverid_db();
	if (db == NULL) {
		return false;
	}

	serverid_fill_key(&id, &key);
	tdbkey = make_tdb_data((uint8_t *)&key, sizeof(key));

	rec = db->fetch_locked(db, talloc_tos(), tdbkey);
	if (rec == NULL) {
		DEBUG(1, ("Could not fetch_lock serverid.tdb record\n"));
		return false;
	}

	if (rec->value.dsize != sizeof(struct serverid_data)) {
		DEBUG(1, ("serverid record has unexpected size %d "
			  "(wanted %d)\n", (int)rec->value.dsize,
			  (int)sizeof(struct serverid_data)));
		goto done;
	}

	data = (struct serverid_data *)rec->value.dptr;

	if (do_reg) {
		data->msg_flags |= msg_flags;
	} else {
		data->msg_flags &= ~msg_flags;
	}

	status = rec->store(rec, rec->value, 0);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("Storing serverid.tdb record failed: %s\n",
			  nt_errstr(status)));
		goto done;
	}
	ret = true;
done:
	TALLOC_FREE(rec);
	return ret;
}

bool serverid_deregister(struct server_id id)
{
	struct db_context *db;
	struct serverid_key key;
	struct db_record *rec;
	TDB_DATA tdbkey;
	NTSTATUS status;
	bool ret = false;

	db = serverid_db();
	if (db == NULL) {
		return false;
	}

	serverid_fill_key(&id, &key);
	tdbkey = make_tdb_data((uint8_t *)&key, sizeof(key));

	rec = db->fetch_locked(db, talloc_tos(), tdbkey);
	if (rec == NULL) {
		DEBUG(1, ("Could not fetch_lock serverid.tdb record\n"));
		return false;
	}

	status = rec->delete_rec(rec);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("Deleting serverid.tdb record failed: %s\n",
			  nt_errstr(status)));
		goto done;
	}
	ret = true;
done:
	TALLOC_FREE(rec);
	return ret;
}

struct serverid_exists_state {
	const struct server_id *id;
	bool exists;
};

static int server_exists_parse(TDB_DATA key, TDB_DATA data, void *priv)
{
	struct serverid_exists_state *state =
		(struct serverid_exists_state *)priv;

	if (data.dsize != sizeof(struct serverid_data)) {
		return -1;
	}

	/*
	 * Use memcmp, not direct compare. data.dptr might not be
	 * aligned.
	 */
	state->exists = (memcmp(&state->id->unique_id, data.dptr,
				sizeof(state->id->unique_id)) == 0);
	return 0;
}

bool serverid_exists(const struct server_id *id)
{
	struct db_context *db;
	struct serverid_exists_state state;
	struct serverid_key key;
	TDB_DATA tdbkey;

	if (procid_is_me(id)) {
		return true;
	}

	if (!process_exists(*id)) {
		return false;
	}

	if (id->unique_id == SERVERID_UNIQUE_ID_NOT_TO_VERIFY) {
		return true;
	}

	db = serverid_db();
	if (db == NULL) {
		return false;
	}

	serverid_fill_key(id, &key);
	tdbkey = make_tdb_data((uint8_t *)&key, sizeof(key));

	state.id = id;
	state.exists = false;

	if (db->parse_record(db, tdbkey, server_exists_parse, &state) == -1) {
		return false;
	}
	return state.exists;
}

static bool serverid_rec_parse(const struct db_record *rec,
			       struct server_id *id, uint32_t *msg_flags)
{
	struct serverid_key key;
	struct serverid_data data;

	if (rec->key.dsize != sizeof(key)) {
		DEBUG(1, ("Found invalid key length %d in serverid.tdb\n",
			  (int)rec->key.dsize));
		return false;
	}
	if (rec->value.dsize != sizeof(data)) {
		DEBUG(1, ("Found invalid value length %d in serverid.tdb\n",
			  (int)rec->value.dsize));
		return false;
	}

	memcpy(&key, rec->key.dptr, sizeof(key));
	memcpy(&data, rec->value.dptr, sizeof(data));

	id->pid = key.pid;
	id->vnn = key.vnn;
	id->unique_id = data.unique_id;
	*msg_flags = data.msg_flags;
	return true;
}

struct serverid_traverse_read_state {
	int (*fn)(const struct server_id *id, uint32_t msg_flags,
		  void *private_data);
	void *private_data;
};

static int serverid_traverse_read_fn(struct db_record *rec, void *private_data)
{
	struct serverid_traverse_read_state *state =
		(struct serverid_traverse_read_state *)private_data;
	struct server_id id;
	uint32_t msg_flags;

	if (!serverid_rec_parse(rec, &id, &msg_flags)) {
		return 0;
	}
	return state->fn(&id, msg_flags,state->private_data);
}

bool serverid_traverse_read(int (*fn)(const struct server_id *id,
				      uint32_t msg_flags, void *private_data),
			    void *private_data)
{
	struct db_context *db;
	struct serverid_traverse_read_state state;

	db = serverid_db();
	if (db == NULL) {
		return false;
	}
	state.fn = fn;
	state.private_data = private_data;
	return db->traverse_read(db, serverid_traverse_read_fn, &state);
}

struct serverid_traverse_state {
	int (*fn)(struct db_record *rec, const struct server_id *id,
		  uint32_t msg_flags, void *private_data);
	void *private_data;
};

static int serverid_traverse_fn(struct db_record *rec, void *private_data)
{
	struct serverid_traverse_state *state =
		(struct serverid_traverse_state *)private_data;
	struct server_id id;
	uint32_t msg_flags;

	if (!serverid_rec_parse(rec, &id, &msg_flags)) {
		return 0;
	}
	return state->fn(rec, &id, msg_flags, state->private_data);
}

bool serverid_traverse(int (*fn)(struct db_record *rec,
				 const struct server_id *id,
				 uint32_t msg_flags, void *private_data),
			    void *private_data)
{
	struct db_context *db;
	struct serverid_traverse_state state;

	db = serverid_db();
	if (db == NULL) {
		return false;
	}
	state.fn = fn;
	state.private_data = private_data;
	return db->traverse(db, serverid_traverse_fn, &state);
}

uint64_t serverid_get_random_unique_id(void)
{
	uint64_t unique_id = SERVERID_UNIQUE_ID_NOT_TO_VERIFY;

	while (unique_id == SERVERID_UNIQUE_ID_NOT_TO_VERIFY) {
		generate_random_buffer((uint8_t *)&unique_id,
					sizeof(unique_id));
	}

	return unique_id;
}
