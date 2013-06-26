/* 
   ldb database library

   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2009

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
 *  Component: ldb lazy_commit module
 *
 *  Description: module to pretend to support the 'lazy commit' control
 *
 *  Author: Andrew Bartlett
 */

#include "includes.h"
#include "ldb_module.h"
#include "dsdb/samdb/ldb_modules/util.h"

static int unlazy_op(struct ldb_module *module, struct ldb_request *req)
{
	int ret;
	struct ldb_request *new_req;
	struct ldb_control *control = ldb_request_get_control(req, LDB_CONTROL_SERVER_LAZY_COMMIT);
	if (!control) {
		return ldb_next_request(module, req);
	} 
	
	switch (req->operation) {
	case LDB_SEARCH:
		ret = ldb_build_search_req_ex(&new_req, ldb_module_get_ctx(module),
					      req,
					      req->op.search.base,
					      req->op.search.scope,
					      req->op.search.tree,
					      req->op.search.attrs,
					      req->controls,
					      req, dsdb_next_callback,
					      req);
		LDB_REQ_SET_LOCATION(new_req);
		break;
	case LDB_ADD:
		ret = ldb_build_add_req(&new_req, ldb_module_get_ctx(module), req,
					req->op.add.message,
					req->controls,
					req, dsdb_next_callback,
					req);
		LDB_REQ_SET_LOCATION(new_req);
		break;
	case LDB_MODIFY:
		ret = ldb_build_mod_req(&new_req, ldb_module_get_ctx(module), req,
					req->op.mod.message,
					req->controls,
					req, dsdb_next_callback,
					req);
		LDB_REQ_SET_LOCATION(new_req);
		break;
	case LDB_DELETE:
		ret = ldb_build_del_req(&new_req, ldb_module_get_ctx(module), req,
					req->op.del.dn,
					req->controls,
					req, dsdb_next_callback,
					req);
		LDB_REQ_SET_LOCATION(new_req);
		break;
	case LDB_RENAME:
		ret = ldb_build_rename_req(&new_req, ldb_module_get_ctx(module), req,
					   req->op.rename.olddn,
					   req->op.rename.newdn,
					   req->controls,
					   req, dsdb_next_callback,
					   req);
		LDB_REQ_SET_LOCATION(new_req);
		break;
	case LDB_EXTENDED:
		ret = ldb_build_extended_req(&new_req, ldb_module_get_ctx(module),
					     req,
					     req->op.extended.oid,
					     req->op.extended.data,
					     req->controls,
					     req, dsdb_next_callback,
					     req);
		LDB_REQ_SET_LOCATION(new_req);
		break;
	default:
		ldb_set_errstring(ldb_module_get_ctx(module),
				  "Unsupported request type!");
		ret = LDB_ERR_UNWILLING_TO_PERFORM;
	}

	if (ret != LDB_SUCCESS) {
		return ret;
	}

	control->critical = 0;
	return ldb_next_request(module, new_req);
}

static const struct ldb_module_ops ldb_lazy_commit_module_ops = {
	.name		   = "lazy_commit",
	.search            = unlazy_op,
	.add               = unlazy_op,
	.modify            = unlazy_op,
	.del               = unlazy_op,
	.rename            = unlazy_op,
	.request      	   = unlazy_op,
	.extended          = unlazy_op,
};

int ldb_lazy_commit_module_init(const char *version)
{
	LDB_MODULE_CHECK_VERSION(version);
	return ldb_register_module(&ldb_lazy_commit_module_ops);
}
