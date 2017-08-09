/* 
   Unix SMB/CIFS implementation.
   tdb utility functions
   Copyright (C) Andrew Tridgell   1992-1998
   Copyright (C) Rafal Szczesniak  2002
   Copyright (C) Michael Adam      2007

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
#include "util_tdb.h"

#undef malloc
#undef realloc
#undef calloc
#undef strdup

/* these are little tdb utility functions that are meant to make
   dealing with a tdb database a little less cumbersome in Samba */

static SIG_ATOMIC_T gotalarm;

/***************************************************************
 Signal function to tell us we timed out.
****************************************************************/

static void gotalarm_sig(int signum)
{
	gotalarm = 1;
}

/****************************************************************************
 Lock a chain with timeout (in seconds).
****************************************************************************/

static int tdb_chainlock_with_timeout_internal( TDB_CONTEXT *tdb, TDB_DATA key, unsigned int timeout, int rw_type)
{
	/* Allow tdb_chainlock to be interrupted by an alarm. */
	int ret;
	gotalarm = 0;

	if (timeout) {
		CatchSignal(SIGALRM, gotalarm_sig);
		tdb_setalarm_sigptr(tdb, &gotalarm);
		alarm(timeout);
	}

	if (rw_type == F_RDLCK)
		ret = tdb_chainlock_read(tdb, key);
	else
		ret = tdb_chainlock(tdb, key);

	if (timeout) {
		alarm(0);
		tdb_setalarm_sigptr(tdb, NULL);
		CatchSignal(SIGALRM, SIG_IGN);
		if (gotalarm && (ret == -1)) {
			DEBUG(0,("tdb_chainlock_with_timeout_internal: alarm (%u) timed out for key %s in tdb %s\n",
				timeout, key.dptr, tdb_name(tdb)));
			/* TODO: If we time out waiting for a lock, it might
			 * be nice to use F_GETLK to get the pid of the
			 * process currently holding the lock and print that
			 * as part of the debugging message. -- mbp */
			return -1;
		}
	}

	return ret;
}

/****************************************************************************
 Write lock a chain. Return -1 if timeout or lock failed.
****************************************************************************/

int tdb_chainlock_with_timeout( TDB_CONTEXT *tdb, TDB_DATA key, unsigned int timeout)
{
	return tdb_chainlock_with_timeout_internal(tdb, key, timeout, F_WRLCK);
}

int tdb_lock_bystring_with_timeout(TDB_CONTEXT *tdb, const char *keyval,
				   int timeout)
{
	TDB_DATA key = string_term_tdb_data(keyval);

	return tdb_chainlock_with_timeout(tdb, key, timeout);
}

/****************************************************************************
 Read lock a chain by string. Return -1 if timeout or lock failed.
****************************************************************************/

int tdb_read_lock_bystring_with_timeout(TDB_CONTEXT *tdb, const char *keyval, unsigned int timeout)
{
	TDB_DATA key = string_term_tdb_data(keyval);

	return tdb_chainlock_with_timeout_internal(tdb, key, timeout, F_RDLCK);
}




int tdb_trans_store_bystring(TDB_CONTEXT *tdb, const char *keystr,
			     TDB_DATA data, int flags)
{
	TDB_DATA key = string_term_tdb_data(keystr);

	return tdb_trans_store(tdb, key, data, flags);
}

/****************************************************************************
 Useful pair of routines for packing/unpacking data consisting of
 integers and strings.
****************************************************************************/

static size_t tdb_pack_va(uint8 *buf, int bufsize, const char *fmt, va_list ap)
{
	uint8 bt;
	uint16 w;
	uint32 d;
	int i;
	void *p;
	int len;
	char *s;
	char c;
	uint8 *buf0 = buf;
	const char *fmt0 = fmt;
	int bufsize0 = bufsize;

	while (*fmt) {
		switch ((c = *fmt++)) {
		case 'b': /* unsigned 8-bit integer */
			len = 1;
			bt = (uint8)va_arg(ap, int);
			if (bufsize && bufsize >= len)
				SSVAL(buf, 0, bt);
			break;
		case 'w': /* unsigned 16-bit integer */
			len = 2;
			w = (uint16)va_arg(ap, int);
			if (bufsize && bufsize >= len)
				SSVAL(buf, 0, w);
			break;
		case 'd': /* signed 32-bit integer (standard int in most systems) */
			len = 4;
			d = va_arg(ap, uint32);
			if (bufsize && bufsize >= len)
				SIVAL(buf, 0, d);
			break;
		case 'p': /* pointer */
			len = 4;
			p = va_arg(ap, void *);
			d = p?1:0;
			if (bufsize && bufsize >= len)
				SIVAL(buf, 0, d);
			break;
		case 'P': /* null-terminated string */
			s = va_arg(ap,char *);
			w = strlen(s);
			len = w + 1;
			if (bufsize && bufsize >= len)
				memcpy(buf, s, len);
			break;
		case 'f': /* null-terminated string */
			s = va_arg(ap,char *);
			w = strlen(s);
			len = w + 1;
			if (bufsize && bufsize >= len)
				memcpy(buf, s, len);
			break;
		case 'B': /* fixed-length string */
			i = va_arg(ap, int);
			s = va_arg(ap, char *);
			len = 4+i;
			if (bufsize && bufsize >= len) {
				SIVAL(buf, 0, i);
				memcpy(buf+4, s, i);
			}
			break;
		default:
			DEBUG(0,("Unknown tdb_pack format %c in %s\n", 
				 c, fmt));
			len = 0;
			break;
		}

		buf += len;
		if (bufsize)
			bufsize -= len;
		if (bufsize < 0)
			bufsize = 0;
	}

	DEBUG(18,("tdb_pack_va(%s, %d) -> %d\n", 
		 fmt0, bufsize0, (int)PTR_DIFF(buf, buf0)));

	return PTR_DIFF(buf, buf0);
}

size_t tdb_pack(uint8 *buf, int bufsize, const char *fmt, ...)
{
	va_list ap;
	size_t result;

	va_start(ap, fmt);
	result = tdb_pack_va(buf, bufsize, fmt, ap);
	va_end(ap);
	return result;
}

bool tdb_pack_append(TALLOC_CTX *mem_ctx, uint8 **buf, size_t *len,
		     const char *fmt, ...)
{
	va_list ap;
	size_t len1, len2;

	va_start(ap, fmt);
	len1 = tdb_pack_va(NULL, 0, fmt, ap);
	va_end(ap);

	if (mem_ctx != NULL) {
		*buf = TALLOC_REALLOC_ARRAY(mem_ctx, *buf, uint8,
					    (*len) + len1);
	} else {
		*buf = SMB_REALLOC_ARRAY(*buf, uint8, (*len) + len1);
	}

	if (*buf == NULL) {
		return False;
	}

	va_start(ap, fmt);
	len2 = tdb_pack_va((*buf)+(*len), len1, fmt, ap);
	va_end(ap);

	if (len1 != len2) {
		return False;
	}

	*len += len2;

	return True;
}

/****************************************************************************
 Useful pair of routines for packing/unpacking data consisting of
 integers and strings.
****************************************************************************/

int tdb_unpack(const uint8 *buf, int bufsize, const char *fmt, ...)
{
	va_list ap;
	uint8 *bt;
	uint16 *w;
	uint32 *d;
	int len;
	int *i;
	void **p;
	char *s, **b, **ps;
	char c;
	const uint8 *buf0 = buf;
	const char *fmt0 = fmt;
	int bufsize0 = bufsize;

	va_start(ap, fmt);

	while (*fmt) {
		switch ((c=*fmt++)) {
		case 'b': /* unsigned 8-bit integer */
			len = 1;
			bt = va_arg(ap, uint8 *);
			if (bufsize < len)
				goto no_space;
			*bt = SVAL(buf, 0);
			break;
		case 'w': /* unsigned 16-bit integer */
			len = 2;
			w = va_arg(ap, uint16 *);
			if (bufsize < len)
				goto no_space;
			*w = SVAL(buf, 0);
			break;
		case 'd': /* signed 32-bit integer (standard int in most systems) */
			len = 4;
			d = va_arg(ap, uint32 *);
			if (bufsize < len)
				goto no_space;
			*d = IVAL(buf, 0);
			break;
		case 'p': /* pointer */
			len = 4;
			p = va_arg(ap, void **);
			if (bufsize < len)
				goto no_space;
			/*
			 * This isn't a real pointer - only a token (1 or 0)
			 * to mark the fact a pointer is present.
			 */

			*p = (void *)(IVAL(buf, 0) ? (void *)1 : NULL);
			break;
		case 'P': /* null-terminated string */
			/* Return malloc'ed string. */
			ps = va_arg(ap,char **);
			len = strlen((const char *)buf) + 1;
			*ps = SMB_STRDUP((const char *)buf);
			break;
		case 'f': /* null-terminated string */
			s = va_arg(ap,char *);
			len = strlen((const char *)buf) + 1;
			if (bufsize < len || len > sizeof(fstring))
				goto no_space;
			memcpy(s, buf, len);
			break;
		case 'B': /* fixed-length string */
			i = va_arg(ap, int *);
			b = va_arg(ap, char **);
			len = 4;
			if (bufsize < len)
				goto no_space;
			*i = IVAL(buf, 0);
			if (! *i) {
				*b = NULL;
				break;
			}
			len += *i;
			if (bufsize < len)
				goto no_space;
			*b = (char *)SMB_MALLOC(*i);
			if (! *b)
				goto no_space;
			memcpy(*b, buf+4, *i);
			break;
		default:
			DEBUG(0,("Unknown tdb_unpack format %c in %s\n",
				 c, fmt));

			len = 0;
			break;
		}

		buf += len;
		bufsize -= len;
	}

	va_end(ap);

	DEBUG(18,("tdb_unpack(%s, %d) -> %d\n",
		 fmt0, bufsize0, (int)PTR_DIFF(buf, buf0)));

	return PTR_DIFF(buf, buf0);

 no_space:
	va_end(ap);
	return -1;
}


/****************************************************************************
 Log tdb messages via DEBUG().
****************************************************************************/

static void tdb_log(TDB_CONTEXT *tdb, enum tdb_debug_level level, const char *format, ...)
{
	va_list ap;
	char *ptr = NULL;
	int ret;

	va_start(ap, format);
	ret = vasprintf(&ptr, format, ap);
	va_end(ap);

	if ((ret == -1) || !*ptr)
		return;

	DEBUG((int)level, ("tdb(%s): %s", tdb_name(tdb) ? tdb_name(tdb) : "unnamed", ptr));
	SAFE_FREE(ptr);
}

/****************************************************************************
 Like tdb_open() but also setup a logging function that redirects to
 the samba DEBUG() system.
****************************************************************************/

TDB_CONTEXT *tdb_open_log(const char *name, int hash_size, int tdb_flags,
			  int open_flags, mode_t mode)
{
	TDB_CONTEXT *tdb;
	struct tdb_logging_context log_ctx;

	if (!lp_use_mmap())
		tdb_flags |= TDB_NOMMAP;

	log_ctx.log_fn = tdb_log;
	log_ctx.log_private = NULL;

	if ((hash_size == 0) && (name != NULL)) {
		const char *base = strrchr_m(name, '/');
		if (base != NULL) {
			base += 1;
		}
		else {
			base = name;
		}
		hash_size = lp_parm_int(-1, "tdb_hashsize", base, 0);
	}

	tdb = tdb_open_ex(name, hash_size, tdb_flags, 
			  open_flags, mode, &log_ctx, NULL);
	if (!tdb)
		return NULL;

	return tdb;
}

/****************************************************************************
 tdb_store, wrapped in a transaction. This way we make sure that a process
 that dies within writing does not leave a corrupt tdb behind.
****************************************************************************/

int tdb_trans_store(struct tdb_context *tdb, TDB_DATA key, TDB_DATA dbuf,
		    int flag)
{
	int res;

	if ((res = tdb_transaction_start(tdb)) != 0) {
		DEBUG(5, ("tdb_transaction_start failed\n"));
		return res;
	}

	if ((res = tdb_store(tdb, key, dbuf, flag)) != 0) {
		DEBUG(10, ("tdb_store failed\n"));
		if (tdb_transaction_cancel(tdb) != 0) {
			smb_panic("Cancelling transaction failed");
		}
		return res;
	}

	if ((res = tdb_transaction_commit(tdb)) != 0) {
		DEBUG(5, ("tdb_transaction_commit failed\n"));
	}

	return res;
}

/****************************************************************************
 tdb_delete, wrapped in a transaction. This way we make sure that a process
 that dies within deleting does not leave a corrupt tdb behind.
****************************************************************************/

int tdb_trans_delete(struct tdb_context *tdb, TDB_DATA key)
{
	int res;

	if ((res = tdb_transaction_start(tdb)) != 0) {
		DEBUG(5, ("tdb_transaction_start failed\n"));
		return res;
	}

	if ((res = tdb_delete(tdb, key)) != 0) {
		DEBUG(10, ("tdb_delete failed\n"));
		if (tdb_transaction_cancel(tdb) != 0) {
			smb_panic("Cancelling transaction failed");
		}
		return res;
	}

	if ((res = tdb_transaction_commit(tdb)) != 0) {
		DEBUG(5, ("tdb_transaction_commit failed\n"));
	}

	return res;
}

NTSTATUS map_nt_error_from_tdb(enum TDB_ERROR err)
{
	NTSTATUS result = NT_STATUS_INTERNAL_ERROR;

	switch (err) {
	case TDB_SUCCESS:
		result = NT_STATUS_OK;
		break;
	case TDB_ERR_CORRUPT:
		result = NT_STATUS_INTERNAL_DB_CORRUPTION;
		break;
	case TDB_ERR_IO:
		result = NT_STATUS_UNEXPECTED_IO_ERROR;
		break;
	case TDB_ERR_OOM:
		result = NT_STATUS_NO_MEMORY;
		break;
	case TDB_ERR_EXISTS:
		result = NT_STATUS_OBJECT_NAME_COLLISION;
		break;

	case TDB_ERR_LOCK:
		/*
		 * TDB_ERR_LOCK is very broad, we could for example
		 * distinguish between fcntl locks and invalid lock
		 * sequences. So NT_STATUS_FILE_LOCK_CONFLICT is a
		 * compromise.
		 */
		result = NT_STATUS_FILE_LOCK_CONFLICT;
		break;

	case TDB_ERR_NOLOCK:
	case TDB_ERR_LOCK_TIMEOUT:
		/*
		 * These two ones in the enum are not actually used
		 */
		result = NT_STATUS_FILE_LOCK_CONFLICT;
		break;
	case TDB_ERR_NOEXIST:
		result = NT_STATUS_NOT_FOUND;
		break;
	case TDB_ERR_EINVAL:
		result = NT_STATUS_INVALID_PARAMETER;
		break;
	case TDB_ERR_RDONLY:
		result = NT_STATUS_ACCESS_DENIED;
		break;
	case TDB_ERR_NESTING:
		result = NT_STATUS_INTERNAL_ERROR;
		break;
	};
	return result;
}

int tdb_data_cmp(TDB_DATA t1, TDB_DATA t2)
{
	int ret;
	if (t1.dptr == NULL && t2.dptr != NULL) {
		return -1;
	}
	if (t1.dptr != NULL && t2.dptr == NULL) {
		return 1;
	}
	if (t1.dptr == t2.dptr) {
		return t1.dsize - t2.dsize;
	}
	ret = memcmp(t1.dptr, t2.dptr, MIN(t1.dsize, t2.dsize));
	if (ret == 0) {
		return t1.dsize - t2.dsize;
	}
	return ret;
}
