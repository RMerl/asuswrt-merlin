/* 
   ldb database library

   Copyright (C) Simo Sorce  2005
   Copyright (C) Stefan Metzmacher <metze@samba.org> 2007
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
 *  Component: ldb deleted objects control module
 *
 *  Description: this module hides deleted objects, and returns them if the right control is there
 *
 *  Author: Stefan Metzmacher
 */

#include "includes.h"
#include "ldb/include/ldb_module.h"
#include "dsdb/samdb/samdb.h"


static int show_deleted_search(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb;
	struct ldb_control *control;
	struct ldb_control **saved_controls;
	struct ldb_request *down_req;
	struct ldb_parse_tree *nodeleted_tree;
	struct ldb_parse_tree *new_tree = req->op.search.tree;
	int ret;

	ldb = ldb_module_get_ctx(module);

	/* check if there's a show deleted control */
	control = ldb_request_get_control(req, LDB_CONTROL_SHOW_DELETED_OID);

	if (! control) {
		nodeleted_tree = talloc_get_type(ldb_module_get_private(module), 
						 struct ldb_parse_tree);
		if (nodeleted_tree) {
			new_tree = talloc(req, struct ldb_parse_tree);
			if (!new_tree) {
				ldb_oom(ldb);
				return LDB_ERR_OPERATIONS_ERROR;
			}
			*new_tree = *nodeleted_tree;
			/* Replace dummy part of 'and' with the old, tree,
			   without a parse step */
			new_tree->u.list.elements[0] = req->op.search.tree;
		}
	}
	
	ret = ldb_build_search_req_ex(&down_req, ldb, req,
				      req->op.search.base,
				      req->op.search.scope,
				      new_tree,
				      req->op.search.attrs,
				      req->controls,
				      req->context, req->callback,
				      req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	/* if a control is there remove if from the modified request */
	if (control && !save_controls(control, down_req, &saved_controls)) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* perform the search */
	return ldb_next_request(module, down_req);
}

static int show_deleted_init(struct ldb_module *module)
{
	struct ldb_context *ldb;
	struct ldb_parse_tree *nodeleted_tree;
	int ret;

	ldb = ldb_module_get_ctx(module);

	nodeleted_tree = ldb_parse_tree(module, "(&(replace=me)(!(isDeleted=TRUE)))");
	if (!nodeleted_tree) {
		ldb_debug(ldb, LDB_DEBUG_ERROR,
			"show_deleted: Unable to parse isDeleted master expression!\n");
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ldb_module_set_private(module, nodeleted_tree);

	ret = ldb_mod_register_control(module, LDB_CONTROL_SHOW_DELETED_OID);
	if (ret != LDB_SUCCESS) {
		ldb_debug(ldb, LDB_DEBUG_ERROR,
			"show_deleted: Unable to register control with rootdse!\n");
		return LDB_ERR_OPERATIONS_ERROR;
	}

	return ldb_next_init(module);
}

_PUBLIC_ const struct ldb_module_ops ldb_show_deleted_module_ops = {
	.name		   = "show_deleted",
	.search            = show_deleted_search,
	.init_context	   = show_deleted_init
};
