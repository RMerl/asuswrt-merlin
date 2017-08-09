/* 
   ldb database library

   Copyright (C) Andrew Tridgell  2004

     ** NOTE! The following LGPL license applies to the ldb
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

/*
 *  Name: ldb
 *
 *  Component: ldb debug
 *
 *  Description: functions for printing debug messages
 *
 *  Author: Andrew Tridgell
 */

#include "ldb_private.h"

/*
  this allows the user to choose their own debug function
*/
int ldb_set_debug(struct ldb_context *ldb,
		  void (*debug)(void *context, enum ldb_debug_level level, 
				const char *fmt, va_list ap),
		  void *context)
{
	ldb->debug_ops.debug = debug;
	ldb->debug_ops.context = context;
	return 0;
}

/*
  debug function for ldb_set_debug_stderr
*/
static void ldb_debug_stderr(void *context, enum ldb_debug_level level, 
			     const char *fmt, va_list ap) PRINTF_ATTRIBUTE(3,0);
static void ldb_debug_stderr(void *context, enum ldb_debug_level level, 
			     const char *fmt, va_list ap)
{
	if (level <= LDB_DEBUG_WARNING) {
		vfprintf(stderr, fmt, ap);
		fprintf(stderr, "\n");
	}
}

static void ldb_debug_stderr_all(void *context, enum ldb_debug_level level, 
			     const char *fmt, va_list ap) PRINTF_ATTRIBUTE(3,0);
static void ldb_debug_stderr_all(void *context, enum ldb_debug_level level, 
			     const char *fmt, va_list ap)
{
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
}

/*
  convenience function to setup debug messages on stderr
  messages of level LDB_DEBUG_WARNING and higher are printed
*/
int ldb_set_debug_stderr(struct ldb_context *ldb)
{
	return ldb_set_debug(ldb, ldb_debug_stderr, ldb);
}

/*
  log a message
*/
void ldb_debug(struct ldb_context *ldb, enum ldb_debug_level level, const char *fmt, ...)
{
	va_list ap;
	if (ldb->debug_ops.debug == NULL) {
		if (ldb->flags & LDB_FLG_ENABLE_TRACING) {
			ldb_set_debug(ldb, ldb_debug_stderr_all, ldb);
		} else {
			ldb_set_debug_stderr(ldb);
		}
	}
	va_start(ap, fmt);
	ldb->debug_ops.debug(ldb->debug_ops.context, level, fmt, ap);
	va_end(ap);
}

/*
  add to an accumulated log message
 */
void ldb_debug_add(struct ldb_context *ldb, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	if (ldb->partial_debug == NULL) {
		ldb->partial_debug = talloc_vasprintf(ldb, fmt, ap);
	} else {
		ldb->partial_debug = talloc_vasprintf_append(ldb->partial_debug, 
							     fmt, ap);
	}
	va_end(ap);
}

/*
  send the accumulated log message, and free it
 */
void ldb_debug_end(struct ldb_context *ldb, enum ldb_debug_level level)
{
	ldb_debug(ldb, level, "%s", ldb->partial_debug);
	talloc_free(ldb->partial_debug);
	ldb->partial_debug = NULL;
}

/*
  log a message, and set the ldb error string to the same message
*/
void ldb_debug_set(struct ldb_context *ldb, enum ldb_debug_level level, 
		   const char *fmt, ...)
{
	va_list ap;
	char *msg;
	va_start(ap, fmt);
	msg = talloc_vasprintf(ldb, fmt, ap);
	va_end(ap);
	if (msg != NULL) {
		ldb_set_errstring(ldb, msg);
		ldb_debug(ldb, level, "%s", msg);
	}
	talloc_free(msg);
}

