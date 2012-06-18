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
#include "lib/ldb/include/ldb.h"
#include "../lib/util/util_ldb.h"
/*
  search the sam for the specified attributes - va_list variant
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
		talloc_steal(mem_ctx, res->msgs);

		DEBUG(6,("gendb_search_v: %s %s -> %d\n",
			 basedn?ldb_dn_get_linearized(basedn):"NULL",
			 expr?expr:"NULL", res->count));

		ret = res->count;
		*msgs = res->msgs;
		talloc_free(res);
	} else if (scope == LDB_SCOPE_BASE && ret == LDB_ERR_NO_SUCH_OBJECT) {
		ret = 0;
		*msgs = NULL;
	} else {
		DEBUG(4,("gendb_search_v: search failed: %s\n",
					ldb_errstring(ldb)));
		ret = -1;
	}

	talloc_free(expr);

	return ret;
}

/*
  search the LDB for the specified attributes - varargs variant
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
  search the LDB for a specified record (by DN)
*/

int gendb_search_dn(struct ldb_context *ldb,
		 TALLOC_CTX *mem_ctx,
		 struct ldb_dn *dn,
		 struct ldb_message ***res,
		 const char * const *attrs)
{
	return gendb_search(ldb, mem_ctx, dn, res, attrs, NULL);
}

/*
  setup some initial ldif in a ldb
*/
int gendb_add_ldif(struct ldb_context *ldb, const char *ldif_string)
{
	struct ldb_ldif *ldif;
	int ret;
	ldif = ldb_ldif_read_string(ldb, &ldif_string);
	if (ldif == NULL) return -1;
	ret = ldb_add(ldb, ldif->msg);
	talloc_free(ldif);
	return ret;
}

char *wrap_casefold(void *context, void *mem_ctx, const char *s, size_t n)
{
	return strupper_talloc_n(mem_ctx, s, n);
}



/*
  search the LDB for a single record, with the extended_dn control
  return LDB_SUCCESS on success, or an ldb error code on error

  if the search returns 0 entries, return LDB_ERR_NO_SUCH_OBJECT
  if the search returns more than 1 entry, return LDB_ERR_CONSTRAINT_VIOLATION
*/
int gendb_search_single_extended_dn(struct ldb_context *ldb,
				    TALLOC_CTX *mem_ctx,
				    struct ldb_dn *basedn,
				    enum ldb_scope scope,
				    struct ldb_message **msg,
				    const char * const *attrs,
				    const char *format, ...) 
{
	va_list ap;
	int ret;
	struct ldb_request *req;
	char *filter;
	TALLOC_CTX *tmp_ctx;
	struct ldb_result *res;
	struct ldb_extended_dn_control *ctrl;

	tmp_ctx = talloc_new(mem_ctx);

	res = talloc_zero(tmp_ctx, struct ldb_result);
	if (!res) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	va_start(ap, format);
	filter = talloc_vasprintf(tmp_ctx, format, ap);
	va_end(ap);

	if (filter == NULL) {
		talloc_free(tmp_ctx);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = ldb_build_search_req(&req, ldb, tmp_ctx,
				   basedn,
				   scope,
				   filter,
				   attrs,
				   NULL,
				   res,
				   ldb_search_default_callback,
				   NULL);
	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return ret;
	}

	ctrl = talloc(tmp_ctx, struct ldb_extended_dn_control);
	if (ctrl == NULL) {
		talloc_free(tmp_ctx);
		return LDB_ERR_OPERATIONS_ERROR;		
	}

	ctrl->type = 1;

	ret = ldb_request_add_control(req, LDB_CONTROL_EXTENDED_DN_OID, true, ctrl);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ret = ldb_request(ldb, req);
	if (ret == LDB_SUCCESS) {
		ret = ldb_wait(req->handle, LDB_WAIT_ALL);
	}

	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return ret;
	}

	if (res->count == 0) {
		talloc_free(tmp_ctx);
		return LDB_ERR_NO_SUCH_OBJECT;
	}

	if (res->count > 1) {
		/* the function is only supposed to return a single entry */
		DEBUG(0,(__location__ ": More than one return for baseDN %s  filter %s\n",
			 ldb_dn_get_linearized(basedn), filter));
		talloc_free(tmp_ctx);
		return LDB_ERR_CONSTRAINT_VIOLATION;
	}

	*msg = talloc_steal(mem_ctx, res->msgs[0]);

	talloc_free(tmp_ctx);

	return LDB_SUCCESS;
}
