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
#include "../tdb/include/tdb.h"
#include "../lib/util/dlinklist.h"
#include "tdb_wrap.h"
#include "tdb.h"

static struct tdb_wrap *tdb_list;

/* destroy the last connection to a tdb */
static int tdb_wrap_destructor(struct tdb_wrap *w)
{
	tdb_close(w->tdb);
	DLIST_REMOVE(tdb_list, w);
	return 0;
}				 

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
	int dl;

	va_start(ap, format);
	vasprintf(&ptr, format, ap);
	va_end(ap);
	
	switch (level) {
	case TDB_DEBUG_FATAL:
		dl = 0;
		break;
	case TDB_DEBUG_ERROR:
		dl = 1;
		break;
	case TDB_DEBUG_WARNING:
		dl = 2;
		break;
	case TDB_DEBUG_TRACE:
		dl = 5;
		break;
	default:
		dl = 0;
	}		

	if (ptr != NULL) {
		const char *name = tdb_name(tdb);
		DEBUG(dl, ("tdb(%s): %s", name ? name : "unnamed", ptr));
		free(ptr);
	}
}


/*
  wrapped connection to a tdb database
  to close just talloc_free() the tdb_wrap pointer
 */
struct tdb_wrap *tdb_wrap_open(TALLOC_CTX *mem_ctx,
			       const char *name, int hash_size, int tdb_flags,
			       int open_flags, mode_t mode)
{
	struct tdb_wrap *w;
	struct tdb_logging_context log_ctx;
	log_ctx.log_fn = tdb_wrap_log;

	for (w=tdb_list;w;w=w->next) {
		if (strcmp(name, w->name) == 0) {
			return talloc_reference(mem_ctx, w);
		}
	}

	w = talloc(mem_ctx, struct tdb_wrap);
	if (w == NULL) {
		return NULL;
	}

	w->name = talloc_strdup(w, name);

	w->tdb = tdb_open_ex(name, hash_size, tdb_flags, 
			     open_flags, mode, &log_ctx, NULL);
	if (w->tdb == NULL) {
		talloc_free(w);
		return NULL;
	}

	talloc_set_destructor(w, tdb_wrap_destructor);

	DLIST_ADD(tdb_list, w);

	return w;
}
