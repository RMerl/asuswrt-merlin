/*
   Unix SMB/CIFS implementation.
   Low-level sessionid.tdb access functions
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
#include "dbwrap.h"
#include "session.h"
#include "util_tdb.h"

static struct db_context *session_db_ctx_ptr = NULL;

static struct db_context *session_db_ctx(void)
{
	return session_db_ctx_ptr;
}

static struct db_context *session_db_ctx_init(bool readonly)
{
	session_db_ctx_ptr = db_open(NULL, lock_path("sessionid.tdb"), 0,
				     TDB_CLEAR_IF_FIRST|TDB_DEFAULT|TDB_INCOMPATIBLE_HASH,
				     readonly ? O_RDONLY : O_RDWR | O_CREAT, 0644);
	return session_db_ctx_ptr;
}

bool sessionid_init(void)
{
	if (session_db_ctx_init(false) == NULL) {
		DEBUG(1,("session_init: failed to open sessionid tdb\n"));
		return False;
	}

	return True;
}

bool sessionid_init_readonly(void)
{
	if (session_db_ctx_init(true) == NULL) {
		DEBUG(1,("session_init: failed to open sessionid tdb\n"));
		return False;
	}

	return True;
}

struct db_record *sessionid_fetch_record(TALLOC_CTX *mem_ctx, const char *key)
{
	struct db_context *db;

	db = session_db_ctx();
	if (db == NULL) {
		return NULL;
	}
	return db->fetch_locked(db, mem_ctx, string_term_tdb_data(key));
}

struct sessionid_traverse_state {
	int (*fn)(struct db_record *rec, const char *key,
		  struct sessionid *session, void *private_data);
	void *private_data;
};

static int sessionid_traverse_fn(struct db_record *rec, void *private_data)
{
	struct sessionid_traverse_state *state =
		(struct sessionid_traverse_state *)private_data;
	struct sessionid session;

	if ((rec->key.dptr[rec->key.dsize-1] != '\0')
	    || (rec->value.dsize != sizeof(struct sessionid))) {
		DEBUG(1, ("Found invalid record in sessionid.tdb\n"));
		return 0;
	}

	memcpy(&session, rec->value.dptr, sizeof(session));

	return state->fn(rec, (char *)rec->key.dptr, &session,
			 state->private_data);
}

int sessionid_traverse(int (*fn)(struct db_record *rec, const char *key,
				 struct sessionid *session,
				 void *private_data),
		       void *private_data)
{
	struct db_context *db;
	struct sessionid_traverse_state state;

	db = session_db_ctx();
	if (db == NULL) {
		return -1;
	}
	state.fn = fn;
	state.private_data = private_data;
	return db->traverse(db, sessionid_traverse_fn, &state);
}

struct sessionid_traverse_read_state {
	int (*fn)(const char *key, struct sessionid *session,
		  void *private_data);
	void *private_data;
};

static int sessionid_traverse_read_fn(struct db_record *rec,
				      void *private_data)
{
	struct sessionid_traverse_read_state *state =
		(struct sessionid_traverse_read_state *)private_data;
	struct sessionid session;

	if ((rec->key.dptr[rec->key.dsize-1] != '\0')
	    || (rec->value.dsize != sizeof(struct sessionid))) {
		DEBUG(1, ("Found invalid record in sessionid.tdb\n"));
		return 0;
	}

	memcpy(&session, rec->value.dptr, sizeof(session));

	return state->fn((char *)rec->key.dptr, &session,
			 state->private_data);
}

int sessionid_traverse_read(int (*fn)(const char *key,
				      struct sessionid *session,
				      void *private_data),
			    void *private_data)
{
	struct db_context *db;
	struct sessionid_traverse_read_state state;

	db = session_db_ctx();
	if (db == NULL) {
		return -1;
	}
	state.fn = fn;
	state.private_data = private_data;
	return db->traverse(db, sessionid_traverse_read_fn, &state);
}
