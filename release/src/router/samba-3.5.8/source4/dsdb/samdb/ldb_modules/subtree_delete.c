/* 
   ldb database library

   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2006-2007
   Copyright (C) Stefan Metzmacher <metze@samba.org> 2007
   Copyright (C) Simo Sorce <idra@samba.org> 2008

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
 *  Component: ldb subtree delete (prevention) module
 *
 *  Description: Prevent deletion of a subtree in LDB
 *
 *  Author: Andrew Bartlett
 */

#include "ldb_module.h"

struct subtree_delete_context {
	struct ldb_module *module;
	struct ldb_request *req;

	int num_children;
};

static struct subtree_delete_context *subdel_ctx_init(struct ldb_module *module,
						      struct ldb_request *req)
{
	struct ldb_context *ldb;
	struct subtree_delete_context *ac;

	ldb = ldb_module_get_ctx(module);

	ac = talloc_zero(req, struct subtree_delete_context);
	if (ac == NULL) {
		ldb_oom(ldb);
		return NULL;
	}

	ac->module = module;
	ac->req = req;

	return ac;
}

static int subtree_delete_search_callback(struct ldb_request *req,
					  struct ldb_reply *ares)
{
	struct ldb_context *ldb;
	struct subtree_delete_context *ac;
	int ret;

	ac = talloc_get_type(req->context, struct subtree_delete_context);
	ldb = ldb_module_get_ctx(ac->module);

	if (!ares) {
		ret = LDB_ERR_OPERATIONS_ERROR;
		goto done;
	}
	if (ares->error != LDB_SUCCESS) {
		return ldb_module_done(ac->req, ares->controls,
					ares->response, ares->error);
	}

	switch (ares->type) {
	case LDB_REPLY_ENTRY:
		/* count entry */
		++(ac->num_children);

		talloc_free(ares);
		ret = LDB_SUCCESS;
		break;

	case LDB_REPLY_REFERRAL:
		/* ignore */
		talloc_free(ares);
		ret = LDB_SUCCESS;
		break;

	case LDB_REPLY_DONE:
		talloc_free(ares);

		if (ac->num_children > 0) {
			ldb_asprintf_errstring(ldb,
				"Cannot delete %s, not a leaf node "
				"(has %d children)\n",
				ldb_dn_get_linearized(ac->req->op.del.dn),
				ac->num_children);
			return ldb_module_done(ac->req, NULL, NULL,
					       LDB_ERR_NOT_ALLOWED_ON_NON_LEAF);
		}

		/* ok no children, let the original request through */
		ret = ldb_next_request(ac->module, ac->req);
		break;
	}

done:
	if (ret != LDB_SUCCESS) {
		return ldb_module_done(ac->req, NULL, NULL, ret);
	}

	return LDB_SUCCESS;
}

static int subtree_delete(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb;
	static const char * const attrs[2] = { "distinguishedName", NULL };
	struct ldb_request *search_req;
	struct subtree_delete_context *ac;
	int ret;

	if (ldb_dn_is_special(req->op.rename.olddn)) {
		/* do not manipulate our control entries */
		return ldb_next_request(module, req);
	}

	ldb = ldb_module_get_ctx(module);

	/* This gets complex:  We need to:
	   - Do a search for all entires under this entry 
	   - Wait for these results to appear
	   - In the callback for each result, count the children (if any)
	   - return an error if there are any
	*/

	ac = subdel_ctx_init(module, req);
	if (!ac) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* we do not really need to find all descendents,
	 * if there is even one single direct child, that's
	 * enough to bail out */
	ret = ldb_build_search_req(&search_req, ldb, ac,
				   req->op.del.dn, LDB_SCOPE_ONELEVEL,
				   "(objectClass=*)", attrs,
				   req->controls,
				   ac, subtree_delete_search_callback,
				   req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	return ldb_next_request(module, search_req);
}

const struct ldb_module_ops ldb_subtree_delete_module_ops = {
	.name		   = "subtree_delete",
	.del               = subtree_delete,
};
