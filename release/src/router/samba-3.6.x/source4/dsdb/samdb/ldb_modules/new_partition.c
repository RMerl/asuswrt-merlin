/* 
   ldb database library

   Copyright (C) Simo Sorce  2004-2008
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2005
   Copyright (C) Andrew Tridgell 2005
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
 *  Component: ldb new partition module
 *
 *  Description: Handle the add of new partitions
 *
 *  Author: Andrew Bartlett
 */

#include "includes.h"
#include "ldb.h"
#include "ldb_module.h"
#include "librpc/gen_ndr/ndr_misc.h"
#include "dsdb/samdb/samdb.h"
#include "../libds/common/flags.h"

struct np_context {
	struct ldb_module *module;
	struct ldb_request *req;
	struct ldb_request *search_req;
	struct ldb_request *part_add;
};

static int np_part_mod_callback(struct ldb_request *req, struct ldb_reply *ares)
{
	struct ldb_context *ldb;
	struct np_context *ac;

	ac = talloc_get_type(req->context, struct np_context);
	ldb = ldb_module_get_ctx(ac->module);

	if (!ares) {
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}

	/* We just want to update the @PARTITIONS record if the value does not exist */
	if (ares->error != LDB_SUCCESS && ares->error != LDB_ERR_ATTRIBUTE_OR_VALUE_EXISTS) {
		return ldb_module_done(ac->req, ares->controls,
					ares->response, ares->error);
	}

	if (ares->type != LDB_REPLY_DONE) {
		ldb_set_errstring(ldb, "Invalid reply type!");
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}

	ldb_reset_err_string(ldb);

	/* Do the original add */
	return ldb_next_request(ac->module, ac->req);
}

static int np_part_search_callback(struct ldb_request *req, struct ldb_reply *ares)
{
	struct ldb_context *ldb;
	struct np_context *ac;
	struct dsdb_create_partition_exop *ex_op;
	int ret;

	ac = talloc_get_type(req->context, struct np_context);
	ldb = ldb_module_get_ctx(ac->module);

	if (!ares) {
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}

	/* If this already exists, we really don't want to create a
	 * partition - it would allow a duplicate entry to be
	 * created */
	if (ares->error != LDB_ERR_NO_SUCH_OBJECT) {
		if (ares->error == LDB_SUCCESS) {
			return ldb_module_done(ac->req, ares->controls,
					       ares->response, LDB_ERR_ENTRY_ALREADY_EXISTS);
		} else {
			return ldb_module_done(ac->req, ares->controls,
					       ares->response, ares->error);
		}
	}

	if (ares->type != LDB_REPLY_DONE) {
		ldb_set_errstring(ldb, "Invalid reply type - we must not get a result here!");
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}

	ldb_reset_err_string(ldb);

	/* Now that we know it does not exist, we can try and create the partition */
	ex_op = talloc(ac, struct dsdb_create_partition_exop);
	if (ex_op == NULL) {
		return ldb_oom(ldb);
	}
	
	ex_op->new_dn = ac->req->op.add.message->dn;
	
	ret = ldb_build_extended_req(&ac->part_add, 
				     ldb, ac, DSDB_EXTENDED_CREATE_PARTITION_OID, ex_op, 
				     NULL, ac, np_part_mod_callback, req);
	
	LDB_REQ_SET_LOCATION(ac->part_add);
	if (ret != LDB_SUCCESS) {
		return ret;
	}
	
	return ldb_next_request(ac->module, ac->part_add);
}

/* add_record: add instancetype attribute */
static int new_partition_add(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb;
	struct np_context *ac;
	int ret;

	ldb = ldb_module_get_ctx(module);

	ldb_debug(ldb, LDB_DEBUG_TRACE, "new_partition_add\n");

	/* do not manipulate our control entries */
	if (ldb_dn_is_special(req->op.add.message->dn)) {
		return ldb_next_request(module, req);
	}

	if (ldb_msg_find_element(req->op.add.message, "instanceType")) {
		/* This needs to be 'static' to ensure it does not move, and is not on the stack */
		static const char *no_attrs[] = { NULL };
		uint32_t instanceType = ldb_msg_find_attr_as_uint(req->op.add.message, "instanceType", 0);

		if (!(instanceType & INSTANCE_TYPE_IS_NC_HEAD)) {
			return ldb_next_request(module, req);
		}

		if (instanceType & INSTANCE_TYPE_UNINSTANT) {
			DEBUG(0,(__location__ ": Skipping uninstantiated partition %s\n",
				 ldb_dn_get_linearized(req->op.add.message->dn)));
			return ldb_next_request(module, req);
		}

		if (ldb_msg_find_attr_as_bool(req->op.add.message, "isDeleted", false)) {
			DEBUG(0,(__location__ ": Skipping deleted partition %s\n",
				 ldb_dn_get_linearized(req->op.add.message->dn)));
			return ldb_next_request(module, req);		
		}

		/* Create an @PARTITIONS record for this partition -
		 * by asking the partitions module to do so via an
		 * extended operation, after first checking if the
		 * record already exists */
		ac = talloc(req, struct np_context);
		if (ac == NULL) {
			return ldb_oom(ldb);
		}
		ac->module = module;
		ac->req = req;
		
		ret = ldb_build_search_req(&ac->search_req, ldb, ac, req->op.add.message->dn, 
					   LDB_SCOPE_BASE, NULL, no_attrs, req->controls, ac, 
					   np_part_search_callback,
					   req);
		LDB_REQ_SET_LOCATION(ac->search_req);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
		
		return ldb_next_request(module, ac->search_req);
	}

	/* go on with the call chain */
	return ldb_next_request(module, req);
}

static const struct ldb_module_ops ldb_new_partition_module_ops = {
	.name          = "new_partition",
	.add           = new_partition_add,
};

int ldb_new_partition_module_init(const char *version)
{
	LDB_MODULE_CHECK_VERSION(version);
	return ldb_register_module(&ldb_new_partition_module_ops);
}
