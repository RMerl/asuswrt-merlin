/* 
   Unix SMB/CIFS implementation.

   Swig interface to tdb.

   Copyright (C) 2004,2005 Tim Potter <tpot@samba.org>

     ** NOTE! The following LGPL license applies to the tdb
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

%module tdb

%{

/* This symbol is used in both includes.h and Python.h which causes an
   annoying compiler warning. */

#ifdef HAVE_FSTAT
#undef HAVE_FSTAT
#endif

#if (__GNUC__ >= 3)
/** Use gcc attribute to check printf fns.  a1 is the 1-based index of
 * the parameter containing the format, and a2 the index of the first
 * argument. Note that some gcc 2.x versions don't handle this
 * properly **/
#define PRINTF_ATTRIBUTE(a1, a2) __attribute__ ((format (__printf__, a1, a2)))
#else
#define PRINTF_ATTRIBUTE(a1, a2)
#endif

/* Include tdb headers */

#include "lib/tdb/include/tdb.h"

%}

/* The tdb functions will crash if a NULL tdb context is passed */

%include exception.i

%typemap(check) TDB_CONTEXT* {
	if ($1 == NULL)
		SWIG_exception(SWIG_ValueError, 
			"tdb context must be non-NULL");
}

/* In and out typemaps for the TDB_DATA structure.  This is converted to
   and from the Python string type which can contain arbitrary binary
   data.. */

%typemap(in) TDB_DATA {
	if (!PyString_Check($input)) {
		PyErr_SetString(PyExc_TypeError, "string arg expected");
		return NULL;
	}
	$1.dsize = PyString_Size($input);
	$1.dptr = PyString_AsString($input);
}

%typemap(out) TDB_DATA {
	if ($1.dptr == NULL && $1.dsize == 0) {
		$result = Py_None;
	} else {
		$result = PyString_FromStringAndSize($1.dptr, $1.dsize);
		free($1.dptr);
	}
}

/* Treat a mode_t as an unsigned integer */

typedef int mode_t;

/* flags to tdb_store() */

#define TDB_REPLACE 1
#define TDB_INSERT 2
#define TDB_MODIFY 3

/* flags for tdb_open() */

#define TDB_DEFAULT 0 /* just a readability place holder */
#define TDB_CLEAR_IF_FIRST 1
#define TDB_INTERNAL 2 /* don't store on disk */
#define TDB_NOLOCK   4 /* don't do any locking */
#define TDB_NOMMAP   8 /* don't use mmap */
#define TDB_CONVERT 16 /* convert endian (internal use) */
#define TDB_BIGENDIAN 32 /* header is big-endian (internal use) */

/* Throw an IOError exception if tdb_open() or tdb_open_ex() returns NULL */

%exception {
	$action
	if (result == NULL) {
		PyErr_SetFromErrno(PyExc_IOError);
		SWIG_fail;
	}
}

TDB_CONTEXT *tdb_open(const char *name, int hash_size, int tdb_flags,
		      int open_flags, mode_t mode);

TDB_CONTEXT *tdb_open_ex(const char *name, int hash_size, int tdb_flags,
			 int open_flags, mode_t mode,
			 tdb_log_func log_fn,
			 tdb_hash_func hash_fn);

%exception;

int tdb_reopen(TDB_CONTEXT *tdb);

int tdb_reopen_all(int parent_longlived);

void tdb_logging_function(TDB_CONTEXT *tdb, tdb_log_func);

enum TDB_ERROR tdb_error(TDB_CONTEXT *tdb);

const char *tdb_errorstr(TDB_CONTEXT *tdb);

TDB_DATA tdb_fetch(TDB_CONTEXT *tdb, TDB_DATA key);

int tdb_delete(TDB_CONTEXT *tdb, TDB_DATA key);

int tdb_store(TDB_CONTEXT *tdb, TDB_DATA key, TDB_DATA dbuf, int flag = TDB_REPLACE);

int tdb_append(TDB_CONTEXT *tdb, TDB_DATA key, TDB_DATA new_dbuf);

int tdb_close(TDB_CONTEXT *tdb);

TDB_DATA tdb_firstkey(TDB_CONTEXT *tdb);

TDB_DATA tdb_nextkey(TDB_CONTEXT *tdb, TDB_DATA key);

int tdb_traverse(TDB_CONTEXT *tdb, tdb_traverse_func fn, void *state);

int tdb_exists(TDB_CONTEXT *tdb, TDB_DATA key);

int tdb_lockall(TDB_CONTEXT *tdb);

void tdb_unlockall(TDB_CONTEXT *tdb);

/* Low level locking functions: use with care */

int tdb_chainlock(TDB_CONTEXT *tdb, TDB_DATA key);

int tdb_chainunlock(TDB_CONTEXT *tdb, TDB_DATA key);

/* Debug functions. Not used in production. */

void tdb_dump_all(TDB_CONTEXT *tdb);

int tdb_printfreelist(TDB_CONTEXT *tdb);
