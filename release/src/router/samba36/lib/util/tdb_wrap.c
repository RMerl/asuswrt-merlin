/* 
   Unix SMB/CIFS implementation.
   TDB wrap functions

   Copyright (C) Andrew Tridgell 2004
   Copyright (C) Jelmer Vernooij <jelmer@samba.org> 2007
   
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
#include <tdb.h>
#include "lib/util/dlinklist.h"
#include "lib/util/tdb_wrap.h"
#include <tdb.h>

/*
 Log tdb messages via DEBUG().
*/
static void tdb_wrap_log(TDB_CONTEXT *tdb, enum tdb_debug_level level, 
			 const char *format, ...) PRINTF_ATTRIBUTE(3,4);

static void tdb_wrap_log(TDB_CONTEXT *tdb, enum tdb_debug_level level, 
			 const char *format, ...)
{
	va_list ap;
	char *ptr = NULL;
	int debuglevel = 0;
	int ret;

	switch (level) {
	case TDB_DEBUG_FATAL:
		debuglevel = 0;
		break;
	case TDB_DEBUG_ERROR:
		debuglevel = 1;
		break;
	case TDB_DEBUG_WARNING:
		debuglevel = 2;
		break;
	case TDB_DEBUG_TRACE:
		debuglevel = 5;
		break;
	default:
		debuglevel = 0;
	}		

	va_start(ap, format);
	ret = vasprintf(&ptr, format, ap);
	va_end(ap);

	if (ret != -1) {
		const char *name = tdb_name(tdb);
		DEBUG(debuglevel, ("tdb(%s): %s", name ? name : "unnamed", ptr));
		free(ptr);
	}
}

struct tdb_wrap_private {
	struct tdb_context *tdb;
	const char *name;
	struct tdb_wrap_private *next, *prev;
};

static struct tdb_wrap_private *tdb_list;

/* destroy the last connection to a tdb */
static int tdb_wrap_private_destructor(struct tdb_wrap_private *w)
{
	tdb_close(w->tdb);
	DLIST_REMOVE(tdb_list, w);
	return 0;
}				 

static struct tdb_wrap_private *tdb_wrap_private_open(TALLOC_CTX *mem_ctx,
						      const char *name,
						      int hash_size,
						      int tdb_flags,
						      int open_flags,
						      mode_t mode)
{
	struct tdb_wrap_private *result;
	struct tdb_logging_context log_ctx;

	result = talloc(mem_ctx, struct tdb_wrap_private);
	if (result == NULL) {
		return NULL;
	}
	result->name = talloc_strdup(result, name);
	if (result->name == NULL) {
		goto fail;
	}

	log_ctx.log_fn = tdb_wrap_log;

#if _SAMBA_BUILD_ == 3	
	/* This #if _SAMBA_BUILD == 3 is very unfortunate, as it means
	 * that in the top level build, these options are not
	 * available for these databases.  However, having two
	 * different tdb_wrap lists is a worse fate, so this will do
	 * for now */

	if (!lp_use_mmap()) {
		tdb_flags |= TDB_NOMMAP;
	}

	if ((hash_size == 0) && (name != NULL)) {
		const char *base;
		base = strrchr_m(name, '/');

		if (base != NULL) {
			base += 1;
		} else {
			base = name;
		}
		hash_size = lp_parm_int(-1, "tdb_hashsize", base, 0);
	}
#endif

	result->tdb = tdb_open_ex(name, hash_size, tdb_flags,
				  open_flags, mode, &log_ctx, NULL);
	if (result->tdb == NULL) {
		goto fail;
	}
	talloc_set_destructor(result, tdb_wrap_private_destructor);
	DLIST_ADD(tdb_list, result);
	return result;

fail:
	TALLOC_FREE(result);
	return NULL;
}

/*
  wrapped connection to a tdb database
  to close just talloc_free() the tdb_wrap pointer
 */
struct tdb_wrap *tdb_wrap_open(TALLOC_CTX *mem_ctx,
			       const char *name, int hash_size, int tdb_flags,
			       int open_flags, mode_t mode)
{
	struct tdb_wrap *result;
	struct tdb_wrap_private *w;

	result = talloc(mem_ctx, struct tdb_wrap);
	if (result == NULL) {
		return NULL;
	}

	for (w=tdb_list;w;w=w->next) {
		if (strcmp(name, w->name) == 0) {
			break;
		}
	}

	if (w == NULL) {
		w = tdb_wrap_private_open(result, name, hash_size, tdb_flags,
					  open_flags, mode);
	} else {
		/*
		 * Correctly use talloc_reference: The tdb will be
		 * closed when "w" is being freed. The caller never
		 * sees "w", so an incorrect use of talloc_free(w)
		 * instead of calling talloc_unlink is not possible.
		 * To avoid having to refcount ourselves, "w" will
		 * have multiple parents that hang off all the
		 * tdb_wrap's being returned from here. Those parents
		 * can be freed without problem.
		 */
		if (talloc_reference(result, w) == NULL) {
			goto fail;
		}
	}
	if (w == NULL) {
		goto fail;
	}
	result->tdb = w->tdb;
	return result;
fail:
	TALLOC_FREE(result);
	return NULL;
}

