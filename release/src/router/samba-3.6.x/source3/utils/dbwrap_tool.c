/*
   Samba Unix/Linux CIFS implementation

   low level TDB/CTDB tool using the dbwrap interface

   Copyright (C) 2009 Michael Adam <obnox@samba.org>

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
#include "messages.h"

typedef enum { OP_FETCH, OP_STORE, OP_DELETE, OP_ERASE, OP_LISTKEYS } dbwrap_op;

typedef enum { TYPE_INT32, TYPE_UINT32 } dbwrap_type;

static int dbwrap_tool_fetch_int32(struct db_context *db,
				   const char *keyname,
				   void *data)
{
	int32_t value;

	value = dbwrap_fetch_int32(db, keyname);
	d_printf("%d\n", value);

	return 0;
}

static int dbwrap_tool_fetch_uint32(struct db_context *db,
				    const char *keyname,
				    void *data)
{
	uint32_t value;
	bool ret;

	ret = dbwrap_fetch_uint32(db, keyname, &value);
	if (ret) {
		d_printf("%u\n", value);
		return 0;
	} else {
		d_fprintf(stderr, "ERROR: could not fetch uint32 key '%s'\n",
			  keyname);
		return -1;
	}
}

static int dbwrap_tool_store_int32(struct db_context *db,
				   const char *keyname,
				   void *data)
{
	NTSTATUS status;
	int32_t value = *((int32_t *)data);

	status = dbwrap_trans_store_int32(db, keyname, value);

	if (!NT_STATUS_IS_OK(status)) {
		d_fprintf(stderr, "ERROR: could not store int32 key '%s': %s\n",
			  keyname, nt_errstr(status));
		return -1;
	}

	return 0;
}

static int dbwrap_tool_store_uint32(struct db_context *db,
				    const char *keyname,
				    void *data)
{
	NTSTATUS status;
	uint32_t value = *((uint32_t *)data);

	status = dbwrap_trans_store_uint32(db, keyname, value);

	if (!NT_STATUS_IS_OK(status)) {
		d_fprintf(stderr,
			  "ERROR: could not store uint32 key '%s': %s\n",
			  keyname, nt_errstr(status));
		return -1;
	}

	return 0;
}

static int dbwrap_tool_delete(struct db_context *db,
			      const char *keyname,
			      void *data)
{
	NTSTATUS status;

	status = dbwrap_trans_delete_bystring(db, keyname);

	if (!NT_STATUS_IS_OK(status)) {
		d_fprintf(stderr, "ERROR deleting record %s : %s\n",
			  keyname, nt_errstr(status));
		return -1;
	}

	return 0;
}

static int delete_fn(struct db_record *rec, void *priv)
{
	rec->delete_rec(rec);
	return 0;
}

/**
 * dbwrap_tool_erase: erase the whole data base
 * the keyname argument is not used.
 */
static int dbwrap_tool_erase(struct db_context *db,
			     const char *keyname,
			     void *data)
{
	int ret;

	ret = db->traverse(db, delete_fn, NULL);

	if (ret < 0) {
		d_fprintf(stderr, "ERROR erasing the database\n");
		return -1;
	}

	return 0;
}

static int listkey_fn(struct db_record *rec, void *private_data)
{
	int length = rec->key.dsize;
	unsigned char *p = (unsigned char *)rec->key.dptr;

	while (length--) {
		if (isprint(*p) && !strchr("\"\\", *p)) {
			d_printf("%c", *p);
		} else {
			d_printf("\\%02X", *p);
		}
		p++;
	}

	d_printf("\n");

	return 0;
}

static int dbwrap_tool_listkeys(struct db_context *db,
				const char *keyname,
				void *data)
{
	int ret;

	ret = db->traverse_read(db, listkey_fn, NULL);

	if (ret < 0) {
		d_fprintf(stderr, "ERROR listing db keys\n");
		return -1;
	}

	return 0;
}

struct dbwrap_op_dispatch_table {
	dbwrap_op op;
	dbwrap_type type;
	int (*cmd)(struct db_context *db,
		   const char *keyname,
		   void *data);
};

struct dbwrap_op_dispatch_table dispatch_table[] = {
	{ OP_FETCH,  TYPE_INT32,  dbwrap_tool_fetch_int32 },
	{ OP_FETCH,  TYPE_UINT32, dbwrap_tool_fetch_uint32 },
	{ OP_STORE,  TYPE_INT32,  dbwrap_tool_store_int32 },
	{ OP_STORE,  TYPE_UINT32, dbwrap_tool_store_uint32 },
	{ OP_DELETE, TYPE_INT32,  dbwrap_tool_delete },
	{ OP_ERASE,  TYPE_INT32,  dbwrap_tool_erase },
	{ OP_LISTKEYS, TYPE_INT32, dbwrap_tool_listkeys },
	{ 0, 0, NULL },
};

int main(int argc, const char **argv)
{
	struct tevent_context *evt_ctx;
	struct messaging_context *msg_ctx;
	struct db_context *db;

	uint16_t count;

	const char *dbname;
	const char *opname;
	dbwrap_op op;
	const char *keyname = "";
	const char *keytype = "int32";
	dbwrap_type type;
	const char *valuestr = "0";
	int32_t value = 0;

	TALLOC_CTX *mem_ctx = talloc_stackframe();

	int ret = 1;

	load_case_tables();
	lp_set_cmdline("log level", "0");
	setup_logging(argv[0], DEBUG_STDERR);
	lp_load(get_dyn_CONFIGFILE(), true, false, false, true);

	if ((argc < 3) || (argc > 6)) {
		d_fprintf(stderr,
			  "USAGE: %s <database> <op> [<key> [<type> [<value>]]]\n"
			  "       ops: fetch, store, delete, erase, listkeys\n"
			  "       types: int32, uint32\n",
			 argv[0]);
		goto done;
	}

	dbname = argv[1];
	opname = argv[2];

	if (strcmp(opname, "store") == 0) {
		if (argc != 6) {
			d_fprintf(stderr, "ERROR: operation 'store' requires "
				  "value argument\n");
			goto done;
		}
		valuestr = argv[5];
		keytype = argv[4];
		keyname = argv[3];
		op = OP_STORE;
	} else if (strcmp(opname, "fetch") == 0) {
		if (argc != 5) {
			d_fprintf(stderr, "ERROR: operation 'fetch' requires "
				  "type but not value argument\n");
			goto done;
		}
		op = OP_FETCH;
		keytype = argv[4];
		keyname = argv[3];
	} else if (strcmp(opname, "delete") == 0) {
		if (argc != 4) {
			d_fprintf(stderr, "ERROR: operation 'delete' does "
				  "not allow type nor value argument\n");
			goto done;
		}
		keyname = argv[3];
		op = OP_DELETE;
	} else if (strcmp(opname, "erase") == 0) {
		if (argc != 3) {
			d_fprintf(stderr, "ERROR: operation 'erase' does "
				  "not take a key argument\n");
			goto done;
		}
		op = OP_ERASE;
	} else if (strcmp(opname, "listkeys") == 0) {
		if (argc != 3) {
			d_fprintf(stderr, "ERROR: operation 'listkeys' does "
				  "not take a key argument\n");
			goto done;
		}
		op = OP_LISTKEYS;
	} else {
		d_fprintf(stderr,
			  "ERROR: invalid op '%s' specified\n"
			  "       supported ops: fetch, store, delete\n",
			  opname);
		goto done;
	}

	if (strcmp(keytype, "int32") == 0) {
		type = TYPE_INT32;
		value = (int32_t)strtol(valuestr, NULL, 10);
	} else if (strcmp(keytype, "uint32") == 0) {
		type = TYPE_UINT32;
		value = (int32_t)strtoul(valuestr, NULL, 10);
	} else {
		d_fprintf(stderr, "ERROR: invalid type '%s' specified.\n"
				  "       supported types: int32, uint32\n",
				  keytype);
		goto done;
	}

	evt_ctx = tevent_context_init(mem_ctx);
	if (evt_ctx == NULL) {
		d_fprintf(stderr, "ERROR: could not init event context\n");
		goto done;
	}

	msg_ctx = messaging_init(mem_ctx, procid_self(), evt_ctx);
	if (msg_ctx == NULL) {
		d_fprintf(stderr, "ERROR: could not init messaging context\n");
		goto done;
	}

	db = db_open(mem_ctx, dbname, 0, TDB_DEFAULT, O_RDWR | O_CREAT, 0644);
	if (db == NULL) {
		d_fprintf(stderr, "ERROR: could not open dbname\n");
		goto done;
	}

	for (count = 0; dispatch_table[count].cmd != NULL; count++) {
		if ((op == dispatch_table[count].op) &&
		    (type == dispatch_table[count].type))
		{
			ret = dispatch_table[count].cmd(db, keyname, &value);
			break;
		}
	}

done:
	TALLOC_FREE(mem_ctx);
	return ret;
}
