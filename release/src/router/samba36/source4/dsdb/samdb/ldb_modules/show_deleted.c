/* 
   ldb database library

   Copyright (C) Simo Sorce  2005
   Copyright (C) Stefan Metzmacher <metze@samba.org> 2007
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2009
   Copyright (C) Matthias Dieter Wallnöfer 2010

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
 *  Description: this module hides deleted and recylced objects, and returns
 *  them if the right control is there
 *
 *  Author: Stefan Metzmacher
 */

#include "includes.h"
#include <ldb_module.h>
#include "dsdb/samdb/samdb.h"
#include "dsdb/samdb/ldb_modules/util.h"

static int show_deleted_search(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb;
	struct ldb_control *show_del, *show_rec;
	struct ldb_request *down_req;
	struct ldb_parse_tree *new_tree = req->op.search.tree;
	int ret;

	ldb = ldb_module_get_ctx(module);

	/* check if there's a show deleted control */
	show_del = ldb_request_get_control(req, LDB_CONTROL_SHOW_DELETED_OID);
	/* check if there's a show recycled control */
	show_rec = ldb_request_get_control(req, LDB_CONTROL_SHOW_RECYCLED_OID);

	if ((show_del == NULL) && (show_rec == NULL)) {
		/* Here we have to suppress all deleted objects:
		 * MS-ADTS 3.1.1.3.4.1
		 *
		 * Filter: (&(!(isDeleted=TRUE))(...))
		 */
		/* FIXME: we could use a constant tree here once we are sure
		 * that no ldb modules modify trees in-site */
		new_tree = talloc(req, struct ldb_parse_tree);
		if (!new_tree) {
			return ldb_oom(ldb);
		}
		new_tree->operation = LDB_OP_AND;
		new_tree->u.list.num_elements = 2;
		new_tree->u.list.elements = talloc_array(new_tree, struct ldb_parse_tree *, 2);
		if (!new_tree->u.list.elements) {
			return ldb_oom(ldb);
		}

		new_tree->u.list.elements[0] = talloc(new_tree->u.list.elements, struct ldb_parse_tree);
		new_tree->u.list.elements[0]->operation = LDB_OP_NOT;
		new_tree->u.list.elements[0]->u.isnot.child =
			talloc(new_tree->u.list.elements, struct ldb_parse_tree);
		if (!new_tree->u.list.elements[0]->u.isnot.child) {
			return ldb_oom(ldb);
		}
		new_tree->u.list.elements[0]->u.isnot.child->operation = LDB_OP_EQUALITY;
		new_tree->u.list.elements[0]->u.isnot.child->u.equality.attr = "isDeleted";
		new_tree->u.list.elements[0]->u.isnot.child->u.equality.value = data_blob_string_const("TRUE");

		new_tree->u.list.elements[1] = req->op.search.tree;
	} else if ((show_del != NULL) && (show_rec == NULL)) {
		/* Here we need to suppress all recycled objects:
		 * MS-ADTS 3.1.1.3.4.1
		 *
		 * Filter: (&(!(isRecycled=TRUE))(...))
		 */
		/* FIXME: we could use a constant tree here once we are sure
		 * that no ldb modules modify trees in-site */
		new_tree = talloc(req, struct ldb_parse_tree);
		if (!new_tree) {
			return ldb_oom(ldb);
		}
		new_tree->operation = LDB_OP_AND;
		new_tree->u.list.num_elements = 2;
		new_tree->u.list.elements = talloc_array(new_tree, struct ldb_parse_tree *, 2);
		if (!new_tree->u.list.elements) {
			return ldb_oom(ldb);
		}

		new_tree->u.list.elements[0] = talloc(new_tree->u.list.elements, struct ldb_parse_tree);
		new_tree->u.list.elements[0]->operation = LDB_OP_NOT;
		new_tree->u.list.elements[0]->u.isnot.child =
			talloc(new_tree->u.list.elements, struct ldb_parse_tree);
		if (!new_tree->u.list.elements[0]->u.isnot.child) {
			return ldb_oom(ldb);
		}
		new_tree->u.list.elements[0]->u.isnot.child->operation = LDB_OP_EQUALITY;
		new_tree->u.list.elements[0]->u.isnot.child->u.equality.attr = "isRecycled";
		new_tree->u.list.elements[0]->u.isnot.child->u.equality.value = data_blob_string_const("TRUE");

		new_tree->u.list.elements[1] = req->op.search.tree;
	}

	ret = ldb_build_search_req_ex(&down_req, ldb, req,
				      req->op.search.base,
				      req->op.search.scope,
				      new_tree,
				      req->op.search.attrs,
				      req->controls,
				      req, dsdb_next_callback,
				      req);
	LDB_REQ_SET_LOCATION(down_req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	/* mark the controls as done */
	if (show_del != NULL) {
		show_del->critical = 0;
	}
	if (show_rec != NULL) {
		show_rec->critical = 0;
	}

	/* perform the search */
	return ldb_next_request(module, down_req);
}

static int show_deleted_init(struct ldb_module *module)
{
	struct ldb_context *ldb;
	int ret;

	ldb = ldb_module_get_ctx(module);

	ret = ldb_mod_register_control(module, LDB_CONTROL_SHOW_DELETED_OID);
	if (ret != LDB_SUCCESS) {
		ldb_debug(ldb, LDB_DEBUG_ERROR,
			"show_deleted: Unable to register control with rootdse!\n");
		return ldb_operr(ldb);
	}

	ret = ldb_mod_register_control(module, LDB_CONTROL_SHOW_RECYCLED_OID);
	if (ret != LDB_SUCCESS) {
		ldb_debug(ldb, LDB_DEBUG_ERROR,
			"show_deleted: Unable to register control with rootdse!\n");
		return ldb_operr(ldb);
	}

	return ldb_next_init(module);
}

static const struct ldb_module_ops ldb_show_deleted_module_ops = {
	.name		   = "show_deleted",
	.search            = show_deleted_search,
	.init_context	   = show_deleted_init
};

int ldb_show_deleted_module_init(const char *version)
{
	LDB_MODULE_CHECK_VERSION(version);
	return ldb_register_module(&ldb_show_deleted_module_ops);
}
