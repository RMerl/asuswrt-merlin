/*
   Unix SMB/CIFS implementation.

   LDB wrap functions

   Copyright (C) Andrew Tridgell 2004-2009

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
#include <ldb.h>
#include <ldb_errors.h>
#include "lib/ldb-samba/ldif_handlers.h"
#include "ldb_wrap.h"
#include "dsdb/samdb/samdb.h"
#include "param/param.h"
#include "../lib/util/dlinklist.h"
#include <tdb.h>

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


/*
  connecting to a ldb can be a relatively expensive operation because
  of the schema and partition loads. We keep a list of open ldb
  contexts here, and try to re-use when possible.

  This means callers of ldb_wrap_connect() must use talloc_unlink() or
  the free of a parent to destroy the context
 */
static struct ldb_wrap {
	struct ldb_wrap *next, *prev;
	struct ldb_wrap_context {
		/* the context is what we use to tell if two ldb
		 * connections are exactly equivalent
		 */
		const char *url;
		struct tevent_context *ev;
		struct loadparm_context *lp_ctx;
		struct auth_session_info *session_info;
		struct cli_credentials *credentials;
		unsigned int flags;
	} context;
	struct ldb_context *ldb;
} *ldb_wrap_list;

/*
   free a ldb_wrap structure
 */
static int ldb_wrap_destructor(struct ldb_wrap *w)
{
	DLIST_REMOVE(ldb_wrap_list, w);
	return 0;
}

/*
 * The casefolder for s4's LDB databases - Unicode-safe
 */
char *wrap_casefold(void *context, void *mem_ctx, const char *s, size_t n)
{
	return strupper_talloc_n(mem_ctx, s, n);
}


 struct ldb_context *samba_ldb_init(TALLOC_CTX *mem_ctx,
				    struct tevent_context *ev,
				    struct loadparm_context *lp_ctx,
				    struct auth_session_info *session_info,
				    struct cli_credentials *credentials)
{
	struct ldb_context *ldb;
	int ret;

	ldb = ldb_init(mem_ctx, ev);
	if (ldb == NULL) {
		return NULL;
	}

	ldb_set_modules_dir(ldb,
			    talloc_asprintf(ldb,
					    "%s/ldb",
					    lpcfg_modulesdir(lp_ctx)));

	ldb_set_debug(ldb, ldb_wrap_debug, NULL);

	ldb_set_utf8_fns(ldb, NULL, wrap_casefold);

	if (session_info) {
		if (ldb_set_opaque(ldb, "sessionInfo", session_info)) {
			talloc_free(ldb);
			return NULL;
		}
	}

	if (credentials) {
		if (ldb_set_opaque(ldb, "credentials", credentials)) {
			talloc_free(ldb);
			return NULL;
		}
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
	if (ret != LDB_SUCCESS) {
		talloc_free(ldb);
		return NULL;
	}

	/* we usually want Samba databases to be private. If we later
	   find we need one public, we will need to add a parameter to
	   ldb_wrap_connect() */
	ldb_set_create_perms(ldb, 0600);

	return ldb;
}

 struct ldb_context *ldb_wrap_find(const char *url,
				   struct tevent_context *ev,
				   struct loadparm_context *lp_ctx,
				   struct auth_session_info *session_info,
				   struct cli_credentials *credentials,
				   int flags)
{
	struct ldb_wrap *w;
	/* see if we can re-use an existing ldb */
	for (w=ldb_wrap_list; w; w=w->next) {
		if (w->context.ev == ev &&
		    w->context.lp_ctx == lp_ctx &&
		    w->context.session_info == session_info &&
		    w->context.credentials == credentials &&
		    w->context.flags == flags &&
		    (w->context.url == url || strcmp(w->context.url, url) == 0))
			return w->ldb;
	}

	return NULL;
}

int samba_ldb_connect(struct ldb_context *ldb, struct loadparm_context *lp_ctx,
		      const char *url, int flags)
{
	int ret;
	char *real_url = NULL;

	/* allow admins to force non-sync ldb for all databases */
	if (lpcfg_parm_bool(lp_ctx, NULL, "ldb", "nosync", false)) {
		flags |= LDB_FLG_NOSYNC;
	}

	if (DEBUGLVL(10)) {
		flags |= LDB_FLG_ENABLE_TRACING;
	}

	real_url = private_path(ldb, lp_ctx, url);
	if (real_url == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = ldb_connect(ldb, real_url, flags, NULL);

	if (ret != LDB_SUCCESS) {
		return ret;
	}

	/* setup for leak detection */
	ldb_set_opaque(ldb, "wrap_url", real_url);

	return LDB_SUCCESS;
}

 bool ldb_wrap_add(const char *url, struct tevent_context *ev,
		   struct loadparm_context *lp_ctx,
		   struct auth_session_info *session_info,
		   struct cli_credentials *credentials,
		   int flags,
		   struct ldb_context *ldb)
{
	struct ldb_wrap *w;
	struct ldb_wrap_context c;

	/* add to the list of open ldb contexts */
	w = talloc(ldb, struct ldb_wrap);
	if (w == NULL) {
		return false;
	}

	c.url          = url;
	c.ev           = ev;
	c.lp_ctx       = lp_ctx;
	c.session_info = session_info;
	c.credentials  = credentials;
	c.flags        = flags;

	w->context = c;
	w->context.url = talloc_strdup(w, url);
	if (w->context.url == NULL) {
		return false;
	}

	if (session_info) {
		/* take a reference to the session_info, as it is
		 * possible for the ldb to live longer than the
		 * session_info. This happens when a DRS DsBind call
		 * reuses a handle, but the original connection is
		 * shutdown. The token for the new connection is still
		 * valid, so we need the session_info to remain valid for
		 * ldb modules to use
		 */
		if (talloc_reference(w, session_info) == NULL) {
			return false;
		}
	}

	w->ldb = ldb;

	DLIST_ADD(ldb_wrap_list, w);

	talloc_set_destructor(w, ldb_wrap_destructor);

	return true;
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
				      unsigned int flags)
{
	struct ldb_context *ldb;
	int ret;

	ldb = ldb_wrap_find(url, ev, lp_ctx, session_info, credentials, flags);
	if (ldb != NULL)
		return talloc_reference(mem_ctx, ldb);

	ldb = samba_ldb_init(mem_ctx, ev, lp_ctx, session_info, credentials);

	if (ldb == NULL)
		return NULL;

	ret = samba_ldb_connect(ldb, lp_ctx, url, flags);
	if (ret != LDB_SUCCESS) {
		talloc_free(ldb);
		return NULL;
	}

	if (!ldb_wrap_add(url, ev, lp_ctx, session_info, credentials, flags, ldb)) {
		talloc_free(ldb);
		return NULL;
	}

	DEBUG(3,("ldb_wrap open of %s\n", url));

	return ldb;
}

/*
  when we fork() we need to make sure that any open ldb contexts have
  any open transactions cancelled
 */
 void ldb_wrap_fork_hook(void)
{
	struct ldb_wrap *w;

	for (w=ldb_wrap_list; w; w=w->next) {
		if (ldb_transaction_cancel_noerr(w->ldb) != LDB_SUCCESS) {
			smb_panic("Failed to cancel child transactions\n");
		}
	}

	if (tdb_reopen_all(1) == -1) {
		smb_panic("tdb_reopen_all failed\n");
	}
}

 char *ldb_relative_path(struct ldb_context *ldb,
			 TALLOC_CTX *mem_ctx,
			 const char *name)
{
	const char *base_url =
		(const char *)ldb_get_opaque(ldb, "ldb_url");
	char *path, *p, *full_name;
	if (name == NULL) {
		return NULL;
	}
	if (strncmp("tdb://", base_url, 6) == 0) {
		base_url = base_url+6;
	}
	path = talloc_strdup(mem_ctx, base_url);
	if (path == NULL) {
		return NULL;
	}
	if ( (p = strrchr(path, '/')) != NULL) {
		p[0] = '\0';
		full_name = talloc_asprintf(mem_ctx, "%s/%s", path, name);
	} else {
		full_name = talloc_asprintf(mem_ctx, "./%s", name);
	}
	talloc_free(path);
	return full_name;
}
