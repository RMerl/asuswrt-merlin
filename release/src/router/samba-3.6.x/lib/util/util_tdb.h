/*
   Unix SMB/CIFS implementation.

   tdb utility functions

   Copyright (C) Andrew Tridgell 1992-2006

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

#ifndef _____LIB_UTIL_UTIL_TDB_H__
#define _____LIB_UTIL_UTIL_TDB_H__

/***************************************************************
 Make a TDB_DATA and keep the const warning in one place
****************************************************************/
TDB_DATA make_tdb_data(const uint8_t *dptr, size_t dsize);
bool tdb_data_equal(TDB_DATA t1, TDB_DATA t2);
TDB_DATA string_tdb_data(const char *string);
TDB_DATA string_term_tdb_data(const char *string);

/****************************************************************************
 Lock a chain by string. Return -1 if lock failed.
****************************************************************************/
int tdb_lock_bystring(struct tdb_context *tdb, const char *keyval);

/****************************************************************************
 Unlock a chain by string.
****************************************************************************/
void tdb_unlock_bystring(struct tdb_context *tdb, const char *keyval);

/****************************************************************************
 Read lock a chain by string. Return -1 if lock failed.
****************************************************************************/
int tdb_read_lock_bystring(struct tdb_context *tdb, const char *keyval);

/****************************************************************************
 Read unlock a chain by string.
****************************************************************************/
void tdb_read_unlock_bystring(struct tdb_context *tdb, const char *keyval);

/****************************************************************************
 Fetch a int32_t value by a arbitrary blob key, return -1 if not found.
 Output is int32_t in native byte order.
****************************************************************************/
int32_t tdb_fetch_int32_byblob(struct tdb_context *tdb, TDB_DATA key);

/****************************************************************************
 Fetch a int32_t value by string key, return -1 if not found.
 Output is int32_t in native byte order.
****************************************************************************/
int32_t tdb_fetch_int32(struct tdb_context *tdb, const char *keystr);

/****************************************************************************
 Store a int32_t value by an arbitrary blob key, return 0 on success, -1 on failure.
 Input is int32_t in native byte order. Output in tdb is in little-endian.
****************************************************************************/
int tdb_store_int32_byblob(struct tdb_context *tdb, TDB_DATA key, int32_t v);

/****************************************************************************
 Store a int32_t value by string key, return 0 on success, -1 on failure.
 Input is int32_t in native byte order. Output in tdb is in little-endian.
****************************************************************************/
int tdb_store_int32(struct tdb_context *tdb, const char *keystr, int32_t v);

/****************************************************************************
 Fetch a uint32_t value by a arbitrary blob key, return -1 if not found.
 Output is uint32_t in native byte order.
****************************************************************************/
bool tdb_fetch_uint32_byblob(struct tdb_context *tdb, TDB_DATA key, uint32_t *value);

/****************************************************************************
 Fetch a uint32_t value by string key, return -1 if not found.
 Output is uint32_t in native byte order.
****************************************************************************/
bool tdb_fetch_uint32(struct tdb_context *tdb, const char *keystr, uint32_t *value);

/****************************************************************************
 Store a uint32_t value by an arbitrary blob key, return 0 on success, -1 on failure.
 Input is uint32_t in native byte order. Output in tdb is in little-endian.
****************************************************************************/
bool tdb_store_uint32_byblob(struct tdb_context *tdb, TDB_DATA key, uint32_t value);

/****************************************************************************
 Store a uint32_t value by string key, return 0 on success, -1 on failure.
 Input is uint32_t in native byte order. Output in tdb is in little-endian.
****************************************************************************/
bool tdb_store_uint32(struct tdb_context *tdb, const char *keystr, uint32_t value);

/****************************************************************************
 Store a buffer by a null terminated string key.  Return 0 on success, -1
 on failure.
****************************************************************************/
int tdb_store_bystring(struct tdb_context *tdb, const char *keystr, TDB_DATA data, int flags);

/****************************************************************************
 Fetch a buffer using a null terminated string key.  Don't forget to call
 free() on the result dptr.
****************************************************************************/
TDB_DATA tdb_fetch_bystring(struct tdb_context *tdb, const char *keystr);

/****************************************************************************
 Delete an entry using a null terminated string key. 
****************************************************************************/
int tdb_delete_bystring(struct tdb_context *tdb, const char *keystr);

/****************************************************************************
 Atomic integer change. Returns old value. To create, set initial value in *oldval. 
****************************************************************************/
int32_t tdb_change_int32_atomic(struct tdb_context *tdb, const char *keystr, int32_t *oldval, int32_t change_val);

/****************************************************************************
 Atomic unsigned integer change. Returns old value. To create, set initial value in *oldval. 
****************************************************************************/
bool tdb_change_uint32_atomic(struct tdb_context *tdb, const char *keystr, uint32_t *oldval, uint32_t change_val);

/****************************************************************************
 Allow tdb_delete to be used as a tdb_traversal_fn.
****************************************************************************/
int tdb_traverse_delete_fn(struct tdb_context *the_tdb, TDB_DATA key, TDB_DATA dbuf,
                     void *state);

#endif /* _____LIB_UTIL_UTIL_TDB_H__ */

