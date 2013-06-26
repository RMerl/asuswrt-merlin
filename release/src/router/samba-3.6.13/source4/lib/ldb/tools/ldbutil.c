/*
   ldb database library utility

   Copyright (C) Matthieu Patou 2009

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
 *  Description: Common function used by ldb_add/ldb_modify/ldb_delete
 *
 *  Author: Matthieu Patou
 */

#include "ldb.h"
#include "ldb_module.h"
#include "ldbutil.h"


/* autostarts a transacion if none active */
static int ldb_do_autotransaction(struct ldb_context *ldb,
				       struct ldb_request *req)
{
	int ret;

	ret = ldb_transaction_start(ldb);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ret = ldb_request(ldb, req);
	if (ret == LDB_SUCCESS) {
		ret = ldb_wait(req->handle, LDB_WAIT_ALL);
	}

	if (ret == LDB_SUCCESS) {
		return ldb_transaction_commit(ldb);
	}
	ldb_transaction_cancel(ldb);

	if (ldb_errstring(ldb) == NULL) {
		/* no error string was setup by the backend */
		ldb_asprintf_errstring(ldb, "%s (%d)", ldb_strerror(ret), ret);
	}

	return ret;
}
/*
  Same as ldb_add but accept control
*/
int ldb_add_ctrl(struct ldb_context *ldb,
		const struct ldb_message *message,
		struct ldb_control **controls)
{
	struct ldb_request *req;
	int ret;

	ret = ldb_msg_sanity_check(ldb, message);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ret = ldb_build_add_req(&req, ldb, ldb,
					message,
					controls,
					NULL,
					ldb_op_default_callback,
					NULL);

	if (ret != LDB_SUCCESS) return ret;

	/* do request and autostart a transaction */
	ret = ldb_do_autotransaction(ldb, req);

	talloc_free(req);
	return ret;
}

/*
  same as ldb_delete but accept control
*/
int ldb_delete_ctrl(struct ldb_context *ldb, struct ldb_dn *dn,
		struct ldb_control **controls)
{
	struct ldb_request *req;
	int ret;

	ret = ldb_build_del_req(&req, ldb, ldb,
					dn,
					controls,
					NULL,
					ldb_op_default_callback,
					NULL);

	if (ret != LDB_SUCCESS) return ret;

	/* do request and autostart a transaction */
	ret = ldb_do_autotransaction(ldb, req);

	talloc_free(req);
	return ret;
}


/*
  same as ldb_modify, but accepts controls
*/
int ldb_modify_ctrl(struct ldb_context *ldb,
                    const struct ldb_message *message,
                    struct ldb_control **controls)
{
        struct ldb_request *req;
        int ret;

        ret = ldb_msg_sanity_check(ldb, message);
        if (ret != LDB_SUCCESS) {
                return ret;
        }

        ret = ldb_build_mod_req(&req, ldb, ldb,
                                        message,
                                        controls,
                                        NULL,
                                        ldb_op_default_callback,
                                        NULL);

        if (ret != LDB_SUCCESS) return ret;

        /* do request and autostart a transaction */
        ret = ldb_do_autotransaction(ldb, req);

        talloc_free(req);
        return ret;
}


/*
  ldb_search with controls
*/
int ldb_search_ctrl(struct ldb_context *ldb, TALLOC_CTX *mem_ctx,
		    struct ldb_result **result, struct ldb_dn *base,
		    enum ldb_scope scope, const char * const *attrs,
		    struct ldb_control **controls,
		    const char *exp_fmt, ...)
{
	struct ldb_request *req;
	struct ldb_result *res;
	char *expression;
	va_list ap;
	int ret;

	expression = NULL;
	*result = NULL;
	req = NULL;

	res = talloc_zero(mem_ctx, struct ldb_result);
	if (!res) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if (exp_fmt) {
		va_start(ap, exp_fmt);
		expression = talloc_vasprintf(mem_ctx, exp_fmt, ap);
		va_end(ap);

		if (!expression) {
			talloc_free(res);
			return LDB_ERR_OPERATIONS_ERROR;
		}
	}

	ret = ldb_build_search_req(&req, ldb, mem_ctx,
				   base?base:ldb_get_default_basedn(ldb),
				   scope,
				   expression,
				   attrs,
				   controls,
				   res,
				   ldb_search_default_callback,
				   NULL);
	ldb_req_set_location(req, "ldb_search_ctrl");

	if (ret != LDB_SUCCESS) goto done;

	ret = ldb_request(ldb, req);

	if (ret == LDB_SUCCESS) {
		ret = ldb_wait(req->handle, LDB_WAIT_ALL);
	}

done:
	if (ret != LDB_SUCCESS) {
		talloc_free(res);
		res = NULL;
	}

	talloc_free(expression);
	talloc_free(req);

	*result = res;
	return ret;
}
