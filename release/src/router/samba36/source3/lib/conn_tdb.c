/* 
   Unix SMB/CIFS implementation.
   Low-level connections.tdb access functions
   Copyright (C) Volker Lendecke 2007

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
#include "smbd/globals.h"
#include "dbwrap.h"

static struct db_context *connections_db_ctx(bool rw)
{
	static struct db_context *db_ctx;
	int open_flags;

	if (db_ctx != NULL) {
		return db_ctx;
	}

	open_flags = rw ? (O_RDWR|O_CREAT) : O_RDONLY;

	db_ctx = db_open(NULL, lock_path("connections.tdb"), 0,
			 TDB_CLEAR_IF_FIRST|TDB_INCOMPATIBLE_HASH|TDB_DEFAULT, open_flags, 0644);
	return db_ctx;
}

static struct db_record *connections_fetch_record(TALLOC_CTX *mem_ctx,
						  TDB_DATA key)
{
	struct db_context *ctx = connections_db_ctx(True);

	if (ctx == NULL) {
		return NULL;
	}

	return ctx->fetch_locked(ctx, mem_ctx, key);
}

struct db_record *connections_fetch_entry(TALLOC_CTX *mem_ctx,
					  connection_struct *conn,
					  const char *name)
{
	struct connections_key ckey;
	TDB_DATA key;

	ZERO_STRUCT(ckey);
	ckey.pid = sconn_server_id(conn->sconn);
	ckey.cnum = conn->cnum;
	strlcpy(ckey.name, name, sizeof(ckey.name));

	key.dsize = sizeof(ckey);
	key.dptr = (uint8 *)&ckey;

	return connections_fetch_record(mem_ctx, key);
}

struct conn_traverse_state {
	int (*fn)(struct db_record *rec,
		  const struct connections_key *key,
		  const struct connections_data *data,
		  void *private_data);
	void *private_data;
};

static int conn_traverse_fn(struct db_record *rec, void *private_data)
{
	struct conn_traverse_state *state =
		(struct conn_traverse_state *)private_data;

	if ((rec->key.dsize != sizeof(struct connections_key))
	    || (rec->value.dsize != sizeof(struct connections_data))) {
		return 0;
	}

	return state->fn(rec, (const struct connections_key *)rec->key.dptr,
			 (const struct connections_data *)rec->value.dptr,
			 state->private_data);
}

int connections_traverse(int (*fn)(struct db_record *rec,
				   void *private_data),
			 void *private_data)
{
	struct db_context *ctx = connections_db_ctx(False);

	if (ctx == NULL) {
		return -1;
	}

	return ctx->traverse(ctx, fn, private_data);
}

int connections_forall(int (*fn)(struct db_record *rec,
				 const struct connections_key *key,
				 const struct connections_data *data,
				 void *private_data),
		       void *private_data)
{
	struct db_context *ctx;
	struct conn_traverse_state state;

	ctx = connections_db_ctx(true);
	if (ctx == NULL) {
		return -1;
	}

	state.fn = fn;
	state.private_data = private_data;

	return ctx->traverse(ctx, conn_traverse_fn, (void *)&state);
}

struct conn_traverse_read_state {
	int (*fn)(const struct connections_key *key,
		  const struct connections_data *data,
		  void *private_data);
	void *private_data;
};

static int connections_forall_read_fn(struct db_record *rec,
				      void *private_data)
{
	struct conn_traverse_read_state *state =
		(struct conn_traverse_read_state *)private_data;

	if ((rec->key.dsize != sizeof(struct connections_key))
	    || (rec->value.dsize != sizeof(struct connections_data))) {
		return 0;
	}
	return state->fn((const struct connections_key *)rec->key.dptr,
			 (const struct connections_data *)rec->value.dptr,
			 state->private_data);
}

int connections_forall_read(int (*fn)(const struct connections_key *key,
				      const struct connections_data *data,
				      void *private_data),
			    void *private_data)
{
	struct db_context *ctx;
	struct conn_traverse_read_state state;

	ctx = connections_db_ctx(false);
	if (ctx == NULL) {
		return -1;
	}

	state.fn = fn;
	state.private_data = private_data;

	return ctx->traverse_read(ctx, connections_forall_read_fn,
				  (void *)&state);
}

bool connections_init(bool rw)
{
	return (connections_db_ctx(rw) != NULL);
}
