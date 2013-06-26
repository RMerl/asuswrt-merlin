/*
   ldb database library

   Copyright (C) Andrew Bartlett 2010

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
 *  Name: ldb
 *
 *  Component: ldb dn simplification module
 *
 *  Description: Module to strip off extended componenets from search DNs (not accepted by OpenLDAP backends)
 *
 *  Author: Andrew Bartlett
 */



#include "includes.h"
#include "ldb_module.h"
#include "dsdb/samdb/ldb_modules/util.h"

/* search */
static int simple_dn_search(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb;
	struct ldb_request *down_req;
	struct ldb_dn *new_base;
	int ret;

	ldb = ldb_module_get_ctx(module);

	new_base = ldb_dn_copy(req, req->op.search.base);
	if (!new_base) {
		ldb_module_oom(module);
	}

	ldb_dn_remove_extended_components(new_base);

	ret = ldb_build_search_req_ex(&down_req,
				      ldb, req,
				      new_base,
				      req->op.search.scope,
				      req->op.search.tree,
				      req->op.search.attrs,
				      req->controls,
				      req, dsdb_next_callback,
				      req);
	LDB_REQ_SET_LOCATION(down_req);
	if (ret != LDB_SUCCESS) {
		return ldb_operr(ldb);
	}
	talloc_steal(down_req, new_base);

	return ldb_next_request(module, down_req);
}

static const struct ldb_module_ops ldb_simple_dn_module_ops = {
	.name		   = "simple_dn",
	.search = simple_dn_search
};

int ldb_simple_dn_module_init(const char *version)
{
	LDB_MODULE_CHECK_VERSION(version);
	return ldb_register_module(&ldb_simple_dn_module_ops);
}
