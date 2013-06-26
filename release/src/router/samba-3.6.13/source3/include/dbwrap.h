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

#ifndef __DBWRAP_H__
#define __DBWRAP_H__

#include <tdb.h>

struct db_record {
	TDB_DATA key, value;
	NTSTATUS (*store)(struct db_record *rec, TDB_DATA data, int flag);
	NTSTATUS (*delete_rec)(struct db_record *rec);
	void *private_data;
};

struct db_context {
	struct db_record *(*fetch_locked)(struct db_context *db,
					  TALLOC_CTX *mem_ctx,
					  TDB_DATA key);
	int (*fetch)(struct db_context *db, TALLOC_CTX *mem_ctx,
		     TDB_DATA key, TDB_DATA *data);
	int (*traverse)(struct db_context *db,
			int (*f)(struct db_record *rec,
				 void *private_data),
			void *private_data);
	int (*traverse_read)(struct db_context *db,
			     int (*f)(struct db_record *rec,
				      void *private_data),
			     void *private_data);
	int (*get_seqnum)(struct db_context *db);
	int (*get_flags)(struct db_context *db);
	int (*transaction_start)(struct db_context *db);
	int (*transaction_commit)(struct db_context *db);
	int (*transaction_cancel)(struct db_context *db);
	int (*parse_record)(struct db_context *db, TDB_DATA key,
			    int (*parser)(TDB_DATA key, TDB_DATA data,
					  void *private_data),
			    void *private_data);
	void *private_data;
	bool persistent;
};

bool db_is_local(const char *name);

struct db_context *db_open(TALLOC_CTX *mem_ctx,
			   const char *name,
			   int hash_size, int tdb_flags,
			   int open_flags, mode_t mode);

struct db_context *db_open_rbt(TALLOC_CTX *mem_ctx);

struct db_context *db_open_tdb(TALLOC_CTX *mem_ctx,
			       const char *name,
			       int hash_size, int tdb_flags,
			       int open_flags, mode_t mode);

struct messaging_context;

#ifdef CLUSTER_SUPPORT
struct db_context *db_open_ctdb(TALLOC_CTX *mem_ctx,
				const char *name,
				int hash_size, int tdb_flags,
				int open_flags, mode_t mode);
#endif

struct db_context *db_open_file(TALLOC_CTX *mem_ctx,
				struct messaging_context *msg_ctx,
				const char *name,
				int hash_size, int tdb_flags,
				int open_flags, mode_t mode);


NTSTATUS dbwrap_delete(struct db_context *db, TDB_DATA key);
NTSTATUS dbwrap_store(struct db_context *db, TDB_DATA key,
		      TDB_DATA data, int flags);
TDB_DATA dbwrap_fetch(struct db_context *db, TALLOC_CTX *mem_ctx,
		      TDB_DATA key);
NTSTATUS dbwrap_delete_bystring(struct db_context *db, const char *key);
NTSTATUS dbwrap_store_bystring(struct db_context *db, const char *key,
			       TDB_DATA data, int flags);
TDB_DATA dbwrap_fetch_bystring(struct db_context *db, TALLOC_CTX *mem_ctx,
			       const char *key);

/* The following definitions come from lib/dbwrap_util.c  */

int32_t dbwrap_fetch_int32(struct db_context *db, const char *keystr);
int dbwrap_store_int32(struct db_context *db, const char *keystr, int32_t v);
bool dbwrap_fetch_uint32(struct db_context *db, const char *keystr,
			 uint32_t *val);
int dbwrap_store_uint32(struct db_context *db, const char *keystr, uint32_t v);
NTSTATUS dbwrap_change_uint32_atomic(struct db_context *db, const char *keystr,
				     uint32_t *oldval, uint32_t change_val);
NTSTATUS dbwrap_trans_change_uint32_atomic(struct db_context *db,
					   const char *keystr,
					   uint32_t *oldval,
					   uint32_t change_val);
NTSTATUS dbwrap_change_int32_atomic(struct db_context *db, const char *keystr,
				    int32_t *oldval, int32_t change_val);
NTSTATUS dbwrap_trans_change_int32_atomic(struct db_context *db,
					  const char *keystr,
					  int32_t *oldval,
					  int32_t change_val);
NTSTATUS dbwrap_trans_store(struct db_context *db, TDB_DATA key, TDB_DATA dbuf,
			    int flag);
NTSTATUS dbwrap_trans_delete(struct db_context *db, TDB_DATA key);
NTSTATUS dbwrap_trans_store_int32(struct db_context *db, const char *keystr,
				  int32_t v);
NTSTATUS dbwrap_trans_store_uint32(struct db_context *db, const char *keystr,
				   uint32_t v);
NTSTATUS dbwrap_trans_store_bystring(struct db_context *db, const char *key,
				     TDB_DATA data, int flags);
NTSTATUS dbwrap_trans_delete_bystring(struct db_context *db, const char *key);
NTSTATUS dbwrap_trans_do(struct db_context *db,
			 NTSTATUS (*action)(struct db_context *, void *),
			 void *private_data);
NTSTATUS dbwrap_trans_traverse(struct db_context *db,
			       int (*f)(struct db_record*, void*),
			       void *private_data);
NTSTATUS dbwrap_traverse(struct db_context *db,
			 int (*f)(struct db_record*, void*),
			 void *private_data,
			 int *count);

NTSTATUS dbwrap_delete_bystring_upper(struct db_context *db, const char *key);
NTSTATUS dbwrap_store_bystring_upper(struct db_context *db, const char *key,
				     TDB_DATA data, int flags);
TDB_DATA dbwrap_fetch_bystring_upper(struct db_context *db, TALLOC_CTX *mem_ctx,
				     const char *key);

#endif /* __DBWRAP_H__ */
