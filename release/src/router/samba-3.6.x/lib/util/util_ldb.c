/*
   Unix SMB/CIFS implementation.

   common share info functions

   Copyright (C) Andrew Tridgell 2004
   Copyright (C) Tim Potter 2004

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
#include <ldb.h>
#include "../lib/util/util_ldb.h"

/*
 * search the LDB for the specified attributes - va_list variant
 */
int gendb_search_v(struct ldb_context *ldb,
		   TALLOC_CTX *mem_ctx,
		   struct ldb_dn *basedn,
		   struct ldb_message ***msgs,
		   const char * const *attrs,
		   const char *format,
		   va_list ap)
{
	enum ldb_scope scope = LDB_SCOPE_SUBTREE;
	struct ldb_result *res;
	char *expr = NULL;
	int ret;

	if (format) {
		expr = talloc_vasprintf(mem_ctx, format, ap);
		if (expr == NULL) {
			return -1;
		}
	} else {
		scope = LDB_SCOPE_BASE;
	}

	res = NULL;

	ret = ldb_search(ldb, mem_ctx, &res, basedn, scope, attrs,
			 expr?"%s":NULL, expr);

	if (ret == LDB_SUCCESS) {
		DEBUG(6,("gendb_search_v: %s %s -> %d\n",
			 basedn?ldb_dn_get_linearized(basedn):"NULL",
			 expr?expr:"NULL", res->count));

		ret = res->count;
		if (msgs != NULL) {
			*msgs = talloc_steal(mem_ctx, res->msgs);
		}
		talloc_free(res);
	} else if (scope == LDB_SCOPE_BASE && ret == LDB_ERR_NO_SUCH_OBJECT) {
		ret = 0;
		if (msgs != NULL) *msgs = NULL;
	} else {
		DEBUG(4,("gendb_search_v: search failed: %s\n",
					ldb_errstring(ldb)));
		ret = -1;
		if (msgs != NULL) *msgs = NULL;
	}

	talloc_free(expr);

	return ret;
}

/*
 * search the LDB for the specified attributes - varargs variant
 */
int gendb_search(struct ldb_context *ldb,
		 TALLOC_CTX *mem_ctx,
		 struct ldb_dn *basedn,
		 struct ldb_message ***res,
		 const char * const *attrs,
		 const char *format, ...)
{
	va_list ap;
	int count;

	va_start(ap, format);
	count = gendb_search_v(ldb, mem_ctx, basedn, res, attrs, format, ap);
	va_end(ap);

	return count;
}

/*
 * search the LDB for a specified record (by DN)
 */
int gendb_search_dn(struct ldb_context *ldb,
		 TALLOC_CTX *mem_ctx,
		 struct ldb_dn *dn,
		 struct ldb_message ***res,
		 const char * const *attrs)
{
	return gendb_search(ldb, mem_ctx, dn, res, attrs, NULL);
}

