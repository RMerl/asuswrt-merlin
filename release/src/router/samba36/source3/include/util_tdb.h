/* 
   Unix SMB/CIFS implementation.
   tdb utility functions
   Copyright (C) Andrew Tridgell 1999
   
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

#ifndef __TDBUTIL_H__
#define __TDBUTIL_H__

#include <tdb.h>

#include <talloc.h> /* for tdb_wrap_open() */
#include "../libcli/util/ntstatus.h" /* for map_nt_error_from_tdb() */
#include "../../lib/util/util_tdb.h"

int tdb_chainlock_with_timeout( struct tdb_context *tdb, TDB_DATA key,
				unsigned int timeout);
int tdb_lock_bystring_with_timeout(struct tdb_context *tdb, const char *keyval,
				   int timeout);
int tdb_read_lock_bystring_with_timeout(TDB_CONTEXT *tdb, const char *keyval,
					unsigned int timeout);

int tdb_trans_store_bystring(TDB_CONTEXT *tdb, const char *keystr,
			     TDB_DATA data, int flags);
int tdb_trans_store(struct tdb_context *tdb, TDB_DATA key, TDB_DATA dbuf,
		    int flag);
int tdb_trans_delete(struct tdb_context *tdb, TDB_DATA key);

int tdb_unpack(const uint8 *buf, int bufsize, const char *fmt, ...);
size_t tdb_pack(uint8 *buf, int bufsize, const char *fmt, ...);
bool tdb_pack_append(TALLOC_CTX *mem_ctx, uint8 **buf, size_t *len,
		     const char *fmt, ...);

struct tdb_context *tdb_open_log(const char *name, int hash_size,
				 int tdb_flags, int open_flags, mode_t mode);

NTSTATUS map_nt_error_from_tdb(enum TDB_ERROR err);

int tdb_data_cmp(TDB_DATA t1, TDB_DATA t2);

#endif /* __TDBUTIL_H__ */
