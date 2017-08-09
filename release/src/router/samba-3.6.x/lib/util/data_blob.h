/* 
   Unix SMB/CIFS implementation.
   DATA BLOB

   Copyright (C) Andrew Tridgell 2001
   Copyright (C) Andrew Bartlett 2001

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

/* This is a public header file that is installed as part of Samba. 
 * If you remove any functions or change their signature, update 
 * the so version number. */

#ifndef _SAMBA_DATABLOB_H_
#define _SAMBA_DATABLOB_H_

#ifndef _PUBLIC_
#define _PUBLIC_
#endif

#include <talloc.h>
#include <stdint.h>

/* used to hold an arbitrary blob of data */
typedef struct datablob {
	uint8_t *data;
	size_t length;
} DATA_BLOB;

struct data_blob_list_item {
	struct data_blob_list_item *prev,*next;
	DATA_BLOB blob;
};

/* by making struct ldb_val and DATA_BLOB the same, we can simplify
   a fair bit of code */
#define ldb_val datablob

#define data_blob(ptr, size) data_blob_named(ptr, size, "DATA_BLOB: "__location__)
#define data_blob_talloc(ctx, ptr, size) data_blob_talloc_named(ctx, ptr, size, "DATA_BLOB: "__location__)
#define data_blob_dup_talloc(ctx, blob) data_blob_talloc_named(ctx, (blob)->data, (blob)->length, "DATA_BLOB: "__location__)

/**
 construct a data blob, must be freed with data_blob_free()
 you can pass NULL for p and get a blank data blob
**/
_PUBLIC_ DATA_BLOB data_blob_named(const void *p, size_t length, const char *name);

/**
 construct a data blob, using supplied TALLOC_CTX
**/
_PUBLIC_ DATA_BLOB data_blob_talloc_named(TALLOC_CTX *mem_ctx, const void *p, size_t length, const char *name);

/**
 construct a zero data blob, using supplied TALLOC_CTX. 
 use this sparingly as it initialises data - better to initialise
 yourself if you want specific data in the blob
**/
_PUBLIC_ DATA_BLOB data_blob_talloc_zero(TALLOC_CTX *mem_ctx, size_t length);

/**
free a data blob
**/
_PUBLIC_ void data_blob_free(DATA_BLOB *d);

/**
clear a DATA_BLOB's contents
**/
_PUBLIC_ void data_blob_clear(DATA_BLOB *d);

/**
free a data blob and clear its contents
**/
_PUBLIC_ void data_blob_clear_free(DATA_BLOB *d);

/**
check if two data blobs are equal
**/
_PUBLIC_ int data_blob_cmp(const DATA_BLOB *d1, const DATA_BLOB *d2);

/**
print the data_blob as hex string
**/
_PUBLIC_ char *data_blob_hex_string_upper(TALLOC_CTX *mem_ctx, const DATA_BLOB *blob);

/**
print the data_blob as hex string
**/
_PUBLIC_ char *data_blob_hex_string_lower(TALLOC_CTX *mem_ctx, const DATA_BLOB *blob);

/**
  useful for constructing data blobs in test suites, while
  avoiding const warnings
**/
_PUBLIC_ DATA_BLOB data_blob_string_const(const char *str);

/**
  useful for constructing data blobs in test suites, while
  avoiding const warnings

  includes the terminating null character (as opposed to data_blo_string_const)
**/
_PUBLIC_ DATA_BLOB data_blob_string_const_null(const char *str);

/**
 * Create a new data blob from const data 
 */
_PUBLIC_ DATA_BLOB data_blob_const(const void *p, size_t length);

/**
  realloc a data_blob
**/
_PUBLIC_ bool data_blob_realloc(TALLOC_CTX *mem_ctx, DATA_BLOB *blob, size_t length);

/**
  append some data to a data blob
**/
_PUBLIC_ bool data_blob_append(TALLOC_CTX *mem_ctx, DATA_BLOB *blob,
				   const void *p, size_t length);

extern const DATA_BLOB data_blob_null;

#endif /* _SAMBA_DATABLOB_H_ */
