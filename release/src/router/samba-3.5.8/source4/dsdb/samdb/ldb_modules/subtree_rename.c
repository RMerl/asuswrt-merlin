/* 
   ldb database library

   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2006-2007
   Copyright (C) Stefan Metzmacher <metze@samba.org> 2007

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
 *  Component: ldb subtree rename module
 *
 *  Description: Rename a subtree in LDB
 *
 *  Author: Andrew Bartlett
 */

#include "ldb_module.h"

struct subren_msg_store {
	struct subren_msg_store *next;
	struct ldb_dn *olddn;
	struct ldb_dn *newdn;
};

struct subtree_rename_context {
	struct ldb_module *module;
	struct ldb_request *req;

	struct subren_msg_store *list;
	struct subren_msg_store *current;
};

static struct subtree_rename_context *subren_ctx_init(struct ldb_module *module,
					 struct ldb_request *req)
{
	struct ldb_context *ldb;
	struct subtree_rename_context *ac;

	ldb = ldb_module_get_ctx(module);

	ac = talloc_zero(req, struct subtree_rename_context);
	if (ac == NULL) {
		ldb_oom(ldb);
		return NULL;
	}

	ac->module = module;
	ac->req = req;

	return ac;
}

static int subtree_rename_next_request(struct subtree_rename_context *ac);

static int subtree_rename_callback(struct ldb_request *req,
				   struct ldb_reply *ares)
{
	struct ldb_context *ldb;
	struct subtree_rename_context *ac;
	int ret;

	ac = talloc_get_type(req->context, struct subtree_rename_context);
	ldb = ldb_module_get_ctx(ac->module);

	if (!ares) {
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}

	if (ares->error != LDB_SUCCESS) {
		return ldb_module_done(ac->req, ares->controls,
					ares->response, ares->error);
	}

	if (ares->type != LDB_REPLY_DONE) {
		ldb_set_errstring(ldb, "Invalid reply type!\n");
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}

	if (ac->current == NULL) {
		/* this was the last one */
		return ldb_module_done(ac->req, ares->controls,
					ares->response, LDB_SUCCESS);
	}

	ret = subtree_rename_next_request(ac);
	if (ret != LDB_SUCCESS) {
		return ldb_module_done(ac->req, NULL, NULL, ret);
	}

	talloc_free(ares);
	return LDB_SUCCESS;
}

static int subtree_rename_next_request(struct subtree_rename_context *ac)
{
	struct ldb_context *ldb;
	struct ldb_request *req;
	int ret;

	ldb = ldb_module_get_ctx(ac->module);

	if (ac->current == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = ldb_build_rename_req(&req, ldb, ac->current,
				   ac->current->olddn,
				   ac->current->newdn,
				   ac->req->controls,
				   ac, subtree_rename_callback,
				   ac->req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ac->current = ac->current->next;

	return ldb_next_request(ac->module, req);
}

static int subtree_rename_search_callback(struct ldb_request *req,
					  struct ldb_reply *ares)
{
	struct subren_msg_store *store;
	struct subtree_rename_context *ac;
	int ret;

	ac = talloc_get_type(req->context, struct subtree_rename_context);

	if (!ares || !ac->current) {
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}
	if (ares->error != LDB_SUCCESS) {
		return ldb_module_done(ac->req, ares->controls,
					ares->response, ares->error);
	}

	switch (ares->type) {
	case LDB_REPLY_ENTRY:

		if (ldb_dn_compare(ares->message->dn, ac->list->olddn) == 0) {
			/* this was already stored by the
			 * subtree_rename_search() */
			talloc_free(ares);
			return LDB_SUCCESS;
		}

		store = talloc_zero(ac, struct subren_msg_store);
		if (store == NULL) {
			return ldb_module_done(ac->req, NULL, NULL,
						LDB_ERR_OPERATIONS_ERROR);
		}
		ac->current->next = store;
		ac->current = store;

		/* the first list element contains the base for the rename */
		store->olddn = talloc_steal(store, ares->message->dn);
		store->newdn = ldb_dn_copy(store, store->olddn);

		if ( ! ldb_dn_remove_base_components(store->newdn,
				ldb_dn_get_comp_num(ac->list->olddn))) {
			return ldb_module_done(ac->req, NULL, NULL,
						LDB_ERR_OPERATIONS_ERROR);
		}

		if ( ! ldb_dn_add_base(store->newdn, ac->list->newdn)) {
			return ldb_module_done(ac->req, NULL, NULL,
						LDB_ERR_OPERATIONS_ERROR);
		}

		break;

	case LDB_REPLY_REFERRAL:
		/* ignore */
		break;

	case LDB_REPLY_DONE:

		/* rewind ac->current */
		ac->current = ac->list;

		/* All dns set up, start with the first one */
		ret = subtree_rename_next_request(ac);

		if (ret != LDB_SUCCESS) {
			return ldb_module_done(ac->req, NULL, NULL, ret);
		}
		break;
	}

	talloc_free(ares);
	return LDB_SUCCESS;
}

/* rename */
static int subtree_rename(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb;
	static const char *attrs[2] = { "distinguishedName", NULL };
	struct ldb_request *search_req;
	struct subtree_rename_context *ac;
	int ret;
	if (ldb_dn_is_special(req->op.rename.olddn)) { /* do not manipulate our control entries */
		return ldb_next_request(module, req);
	}

	ldb = ldb_module_get_ctx(module);

	/* This gets complex:  We need to:
	   - Do a search for all entires under this entry 
	   - Wait for these results to appear
	   - In the callback for each result, issue a modify request
	    - That will include this rename, we hope
	   - Wait for each modify result
	   - Regain our sainity 
	*/

	ac = subren_ctx_init(module, req);
	if (!ac) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* add this entry as the first to do */
	ac->current = talloc_zero(ac, struct subren_msg_store);
	if (ac->current == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}
	ac->current->olddn = req->op.rename.olddn;
	ac->current->newdn = req->op.rename.newdn;
	ac->list = ac->current;

	ret = ldb_build_search_req(&search_req, ldb, ac,
				   req->op.rename.olddn, 
				   LDB_SCOPE_SUBTREE,
				   "(objectClass=*)",
				   attrs,
				   NULL,
				   ac, 
				   subtree_rename_search_callback,
				   req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	return ldb_next_request(module, search_req);
}

const struct ldb_module_ops ldb_subtree_rename_module_ops = {
	.name		   = "subtree_rename",
	.rename            = subtree_rename,
};
