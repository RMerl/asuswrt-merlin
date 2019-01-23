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

#include "includes.h"
#include <ldb.h>
#include <ldb_module.h>
#include "libds/common/flags.h"
#include "dsdb/samdb/samdb.h"

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

	if (ares->type == LDB_REPLY_REFERRAL) {
		return ldb_module_send_referral(ac->req, ares->referral);
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
		return ldb_operr(ldb);
	}

	ret = ldb_build_rename_req(&req, ldb, ac->current,
				   ac->current->olddn,
				   ac->current->newdn,
				   ac->req->controls,
				   ac, subtree_rename_callback,
				   ac->req);
	LDB_REQ_SET_LOCATION(req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ac->current = ac->current->next;

	return ldb_next_request(ac->module, req);
}

static int check_constraints(struct ldb_message *msg,
			     struct subtree_rename_context *ac,
			     struct ldb_dn *olddn, struct ldb_dn *newdn)
{
	struct ldb_context *ldb = ldb_module_get_ctx(ac->module);
	struct ldb_dn *dn1, *dn2, *nc_root;
	int32_t systemFlags;
	bool move_op = false;
	bool rename_op = false;
	int ret;

	/* Skip the checks if old and new DN are the same, or if we have the
	 * relax control specified or if the returned objects is already
	 * deleted and needs only to be moved for consistency. */

	if (ldb_dn_compare(olddn, newdn) == 0) {
		return LDB_SUCCESS;
	}
	if (ldb_request_get_control(ac->req, LDB_CONTROL_RELAX_OID) != NULL) {
		return LDB_SUCCESS;
	}
	if (ldb_msg_find_attr_as_bool(msg, "isDeleted", false)) {
		return LDB_SUCCESS;
	}

	/* Objects under CN=System */

	dn1 = ldb_dn_copy(ac, ldb_get_default_basedn(ldb));
	if (dn1 == NULL) return ldb_oom(ldb);

	if ( ! ldb_dn_add_child_fmt(dn1, "CN=System")) {
		talloc_free(dn1);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if ((ldb_dn_compare_base(dn1, olddn) == 0) &&
	    (ldb_dn_compare_base(dn1, newdn) != 0)) {
		talloc_free(dn1);
		ldb_asprintf_errstring(ldb,
				       "subtree_rename: Cannot move/rename %s. Objects under CN=System have to stay under it!",
				       ldb_dn_get_linearized(olddn));
		return LDB_ERR_OTHER;
	}

	talloc_free(dn1);

	/* LSA objects */

	if ((samdb_find_attribute(ldb, msg, "objectClass", "secret") != NULL) ||
	    (samdb_find_attribute(ldb, msg, "objectClass", "trustedDomain") != NULL)) {
		ldb_asprintf_errstring(ldb,
				       "subtree_rename: Cannot move/rename %s. It's an LSA-specific object!",
				       ldb_dn_get_linearized(olddn));
		return LDB_ERR_UNWILLING_TO_PERFORM;
	}

	/* systemFlags */

	dn1 = ldb_dn_get_parent(ac, olddn);
	if (dn1 == NULL) return ldb_oom(ldb);
	dn2 = ldb_dn_get_parent(ac, newdn);
	if (dn2 == NULL) return ldb_oom(ldb);

	if (ldb_dn_compare(dn1, dn2) == 0) {
		rename_op = true;
	} else {
		move_op = true;
	}

	talloc_free(dn1);
	talloc_free(dn2);

	systemFlags = ldb_msg_find_attr_as_int(msg, "systemFlags", 0);

	/* Fetch name context */

	ret = dsdb_find_nc_root(ldb, ac, olddn, &nc_root);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	if (ldb_dn_compare(nc_root, ldb_get_schema_basedn(ldb)) == 0) {
		if (move_op) {
			ldb_asprintf_errstring(ldb,
					       "subtree_rename: Cannot move %s within schema partition",
					       ldb_dn_get_linearized(olddn));
			return LDB_ERR_UNWILLING_TO_PERFORM;
		}
		if (rename_op &&
		    (systemFlags & SYSTEM_FLAG_SCHEMA_BASE_OBJECT) != 0) {
			ldb_asprintf_errstring(ldb,
					       "subtree_rename: Cannot rename %s within schema partition",
					       ldb_dn_get_linearized(olddn));
			return LDB_ERR_UNWILLING_TO_PERFORM;
		}
	} else if (ldb_dn_compare(nc_root, ldb_get_config_basedn(ldb)) == 0) {
		if (move_op &&
		    (systemFlags & SYSTEM_FLAG_CONFIG_ALLOW_MOVE) == 0) {
			/* Here we have to do more: control the
			 * "ALLOW_LIMITED_MOVE" flag. This means that the
			 * grand-grand-parents of two objects have to be equal
			 * in order to perform the move (this is used for
			 * moving "server" objects in the "sites" container). */
			bool limited_move =
				systemFlags & SYSTEM_FLAG_CONFIG_ALLOW_LIMITED_MOVE;

			if (limited_move) {
				dn1 = ldb_dn_copy(ac, olddn);
				if (dn1 == NULL) return ldb_oom(ldb);
				dn2 = ldb_dn_copy(ac, newdn);
				if (dn2 == NULL) return ldb_oom(ldb);

				limited_move &= ldb_dn_remove_child_components(dn1, 3);
				limited_move &= ldb_dn_remove_child_components(dn2, 3);
				limited_move &= ldb_dn_compare(dn1, dn2) == 0;

				talloc_free(dn1);
				talloc_free(dn2);
			}

			if (!limited_move) {
				ldb_asprintf_errstring(ldb,
						       "subtree_rename: Cannot move %s to %s in config partition",
						       ldb_dn_get_linearized(olddn), ldb_dn_get_linearized(newdn));
				return LDB_ERR_UNWILLING_TO_PERFORM;
			}
		}
		if (rename_op &&
		    (systemFlags & SYSTEM_FLAG_CONFIG_ALLOW_RENAME) == 0) {
			ldb_asprintf_errstring(ldb,
					       "subtree_rename: Cannot rename %s to %s within config partition",
					       ldb_dn_get_linearized(olddn), ldb_dn_get_linearized(newdn));
			return LDB_ERR_UNWILLING_TO_PERFORM;
		}
	} else if (ldb_dn_compare(nc_root, ldb_get_default_basedn(ldb)) == 0) {
		if (move_op &&
		    (systemFlags & SYSTEM_FLAG_DOMAIN_DISALLOW_MOVE) != 0) {
			ldb_asprintf_errstring(ldb,
					       "subtree_rename: Cannot move %s to %s - DISALLOW_MOVE set",
					       ldb_dn_get_linearized(olddn), ldb_dn_get_linearized(newdn));
			return LDB_ERR_UNWILLING_TO_PERFORM;
		}
		if (rename_op &&
		    (systemFlags & SYSTEM_FLAG_DOMAIN_DISALLOW_RENAME) != 0) {
			ldb_asprintf_errstring(ldb,
						       "subtree_rename: Cannot rename %s to %s - DISALLOW_RENAME set",
					       ldb_dn_get_linearized(olddn), ldb_dn_get_linearized(newdn));
			return LDB_ERR_UNWILLING_TO_PERFORM;
		}
	}

	talloc_free(nc_root);

	return LDB_SUCCESS;
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

			ret = check_constraints(ares->message, ac,
						ac->list->olddn,
						ac->list->newdn);
			if (ret != LDB_SUCCESS) {
				return ldb_module_done(ac->req, NULL, NULL,
						       ret);
			}

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

		ret = check_constraints(ares->message, ac,
					store->olddn, store->newdn);
		if (ret != LDB_SUCCESS) {
			return ldb_module_done(ac->req, NULL, NULL, ret);
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
	static const char * const attrs[] = { "objectClass", "systemFlags",
					      "isDeleted", NULL };
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
	   - Regain our sanity
	*/

	ac = subren_ctx_init(module, req);
	if (!ac) {
		return ldb_oom(ldb);
	}

	/* add this entry as the first to do */
	ac->current = talloc_zero(ac, struct subren_msg_store);
	if (ac->current == NULL) {
		return ldb_oom(ldb);
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
	LDB_REQ_SET_LOCATION(search_req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ret = ldb_request_add_control(search_req, LDB_CONTROL_SHOW_RECYCLED_OID,
				      true, NULL);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	return ldb_next_request(module, search_req);
}

static const struct ldb_module_ops ldb_subtree_rename_module_ops = {
	.name		   = "subtree_rename",
	.rename            = subtree_rename
};

int ldb_subtree_rename_module_init(const char *version)
{
	LDB_MODULE_CHECK_VERSION(version);
	return ldb_register_module(&ldb_subtree_rename_module_ops);
}
