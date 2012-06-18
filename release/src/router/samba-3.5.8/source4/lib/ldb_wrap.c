/* 
   Unix SMB/CIFS implementation.

   LDB wrap functions

   Copyright (C) Andrew Tridgell 2004
   
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

/*
  the stupidity of the unix fcntl locking design forces us to never
  allow a database file to be opened twice in the same process. These
  wrappers provide convenient access to a tdb or ldb, taking advantage
  of talloc destructors to ensure that only a single open is done
*/

#include "includes.h"
#include "lib/events/events.h"
#include "lib/ldb/include/ldb.h"
#include "lib/ldb/include/ldb_errors.h"
#include "lib/ldb-samba/ldif_handlers.h"
#include "ldb_wrap.h"
#include "dsdb/samdb/samdb.h"
#include "param/param.h"

/*
  this is used to catch debug messages from ldb
*/
static void ldb_wrap_debug(void *context, enum ldb_debug_level level, 
			   const char *fmt, va_list ap)  PRINTF_ATTRIBUTE(3,0);

static void ldb_wrap_debug(void *context, enum ldb_debug_level level, 
			   const char *fmt, va_list ap)
{
	int samba_level = -1;
	char *s = NULL;
	switch (level) {
	case LDB_DEBUG_FATAL:
		samba_level = 0;
		break;
	case LDB_DEBUG_ERROR:
		samba_level = 1;
		break;
	case LDB_DEBUG_WARNING:
		samba_level = 2;
		break;
	case LDB_DEBUG_TRACE:
		samba_level = 5;
		break;
		
	};
	vasprintf(&s, fmt, ap);
	if (!s) return;
	DEBUG(samba_level, ("ldb: %s\n", s));
	free(s);
}

/* check for memory leaks on the ldb context */
static int ldb_wrap_destructor(struct ldb_context *ldb)
{
	size_t *startup_blocks = (size_t *)ldb_get_opaque(ldb, "startup_blocks");

	if (startup_blocks &&
	    talloc_total_blocks(ldb) > *startup_blocks + 400) {
		DEBUG(0,("WARNING: probable memory leak in ldb %s - %lu blocks (startup %lu) %lu bytes\n",
			 (char *)ldb_get_opaque(ldb, "wrap_url"), 
			 (unsigned long)talloc_total_blocks(ldb), 
			 (unsigned long)*startup_blocks,
			 (unsigned long)talloc_total_size(ldb)));
#if 0
		talloc_report_full(ldb, stdout);
		call_backtrace();
		smb_panic("probable memory leak in ldb");
#endif
	}
	return 0;
}				 

/*
  wrapped connection to a ldb database
  to close just talloc_free() the returned ldb_context

  TODO:  We need an error_string parameter
 */
struct ldb_context *ldb_wrap_connect(TALLOC_CTX *mem_ctx,
				     struct tevent_context *ev,
				     struct loadparm_context *lp_ctx,
				     const char *url,
				     struct auth_session_info *session_info,
				     struct cli_credentials *credentials,
				     unsigned int flags,
				     const char *options[])
{
	struct ldb_context *ldb;
	int ret;
	char *real_url = NULL;
	size_t *startup_blocks;

	/* we want to use the existing event context if possible. This
	   relies on the fact that in smbd, everything is a child of
	   the main event_context */
	if (ev == NULL) {
		return NULL;
	}

	ldb = ldb_init(mem_ctx, ev);
	if (ldb == NULL) {
		return NULL;
	}

	ldb_set_modules_dir(ldb,
			    talloc_asprintf(ldb,
					    "%s/ldb",
					    lp_modulesdir(lp_ctx)));

	if (ldb_set_opaque(ldb, "sessionInfo", session_info)) {
		talloc_free(ldb);
		return NULL;
	}

	if (ldb_set_opaque(ldb, "credentials", credentials)) {
		talloc_free(ldb);
		return NULL;
	}

	if (ldb_set_opaque(ldb, "loadparm", lp_ctx)) {
		talloc_free(ldb);
		return NULL;
	}

	/* This must be done before we load the schema, as these
	 * handlers for objectSid and objectGUID etc must take
	 * precedence over the 'binary attribute' declaration in the
	 * schema */
	ret = ldb_register_samba_handlers(ldb);
	if (ret == -1) {
		talloc_free(ldb);
		return NULL;
	}

	if (lp_ctx != NULL && strcmp(lp_sam_url(lp_ctx), url) == 0) {
		dsdb_set_global_schema(ldb);
	}

	ldb_set_debug(ldb, ldb_wrap_debug, NULL);

	ldb_set_utf8_fns(ldb, NULL, wrap_casefold);

	real_url = private_path(ldb, lp_ctx, url);
	if (real_url == NULL) {
		talloc_free(ldb);
		return NULL;
	}

	/* allow admins to force non-sync ldb for all databases */
	if (lp_parm_bool(lp_ctx, NULL, "ldb", "nosync", false)) {
		flags |= LDB_FLG_NOSYNC;
	}

	if (DEBUGLVL(10)) {
		flags |= LDB_FLG_ENABLE_TRACING;
	}

	/* we usually want Samba databases to be private. If we later
	   find we need one public, we will need to add a parameter to
	   ldb_wrap_connect() */
	ldb_set_create_perms(ldb, 0600);
	
	ret = ldb_connect(ldb, real_url, flags, options);
	if (ret != LDB_SUCCESS) {
		talloc_free(ldb);
		return NULL;
	}

	/* setup for leak detection */
	ldb_set_opaque(ldb, "wrap_url", real_url);
	startup_blocks = talloc(ldb, size_t);
	*startup_blocks = talloc_total_blocks(ldb);
	ldb_set_opaque(ldb, "startup_blocks", startup_blocks);
	
	talloc_set_destructor(ldb, ldb_wrap_destructor);

	return ldb;
}
