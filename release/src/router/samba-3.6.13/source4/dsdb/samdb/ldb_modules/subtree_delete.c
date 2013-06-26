/* 
   ldb database library

   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2006-2007
   Copyright (C) Andrew Tridgell <tridge@samba.org> 2009
   Copyright (C) Stefan Metzmacher <metze@samba.org> 2007
   Copyright (C) Simo Sorce <idra@samba.org> 2008
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
 *  Component: ldb subtree delete module
 *
 *  Description: Delete of a subtree in LDB
 *
 *  Author: Andrew Bartlett
 */

#include "includes.h"
#include <ldb.h>
#include <ldb_module.h>
#include "dsdb/samdb/ldb_modules/util.h"
#include "dsdb/common/util.h"


static int subtree_delete(struct ldb_module *module, struct ldb_request *req)
{
	static const char * const attrs[] = { NULL };
	struct ldb_result *res = NULL;
	uint32_t flags;
	unsigned int i;
	int ret;

	if (ldb_dn_is_special(req->op.del.dn)) {
		/* do not manipulate our control entries */
		return ldb_next_request(module, req);
	}

	/* see if we have any children */
	ret = dsdb_module_search(module, req, &res, req->op.del.dn,
				 LDB_SCOPE_ONELEVEL, attrs,
				 DSDB_FLAG_NEXT_MODULE,
				 req,
				 "(objectClass=*)");
	if (ret != LDB_SUCCESS) {
		talloc_free(res);
		return ret;
	}
	if (res->count > 0) {
		if (ldb_request_get_control(req, LDB_CONTROL_TREE_DELETE_OID) == NULL) {
			/* Do not add any DN outputs to this error string!
			 * Some MMC consoles (eg release 2000) have a strange
			 * bug and prevent subtree deletes afterwards. */
			ldb_asprintf_errstring(ldb_module_get_ctx(module),
					       "subtree_delete: Unable to "
					       "delete a non-leaf node "
					       "(it has %u children)!",
					       res->count);
			talloc_free(res);
			return LDB_ERR_NOT_ALLOWED_ON_NON_LEAF;
		}

		/* we need to start from the top since other LDB modules could
		 * enforce constraints (eg "objectclass" and "samldb" do so). */
		flags = DSDB_FLAG_TOP_MODULE | DSDB_TREE_DELETE;
		if (ldb_request_get_control(req, LDB_CONTROL_RELAX_OID) != NULL) {
			flags |= DSDB_MODIFY_RELAX;
		}

		for (i = 0; i < res->count; i++) {
			ret = dsdb_module_del(module, res->msgs[i]->dn, flags, req);
			if (ret != LDB_SUCCESS) {
				return ret;
			}
		}
	}
	talloc_free(res);

	return ldb_next_request(module, req);
}

static int subtree_delete_init(struct ldb_module *module)
{
	struct ldb_context *ldb;
	int ret;

	ldb = ldb_module_get_ctx(module);

	ret = ldb_mod_register_control(module, LDB_CONTROL_TREE_DELETE_OID);
	if (ret != LDB_SUCCESS) {
		ldb_debug(ldb, LDB_DEBUG_ERROR,
			"subtree_delete: Unable to register control with rootdse!\n");
		return ldb_operr(ldb);
	}

	return ldb_next_init(module);
}

static const struct ldb_module_ops ldb_subtree_delete_module_ops = {
	.name		   = "subtree_delete",
	.init_context      = subtree_delete_init,
	.del               = subtree_delete
};

int ldb_subtree_delete_module_init(const char *version)
{
	LDB_MODULE_CHECK_VERSION(version);
	return ldb_register_module(&ldb_subtree_delete_module_ops);
}
