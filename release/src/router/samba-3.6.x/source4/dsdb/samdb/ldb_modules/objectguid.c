/* 
   ldb database library

   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2005
   Copyright (C) Andrew Tridgell 2005
   Copyright (C) Simo Sorce  2004-2008

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
 *  Component: ldb objectguid module
 *
 *  Description: add a unique objectGUID onto every new record
 *
 *  Author: Simo Sorce
 */

#include "includes.h"
#include "ldb_module.h"
#include "dsdb/samdb/samdb.h"
#include "dsdb/samdb/ldb_modules/util.h"
#include "librpc/gen_ndr/ndr_misc.h"
#include "param/param.h"

/*
  add a time element to a record
*/
static int add_time_element(struct ldb_message *msg, const char *attr, time_t t)
{
	struct ldb_message_element *el;
	char *s;
	int ret;

	if (ldb_msg_find_element(msg, attr) != NULL) {
		return LDB_SUCCESS;
	}

	s = ldb_timestring(msg, t);
	if (s == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = ldb_msg_add_string(msg, attr, s);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	el = ldb_msg_find_element(msg, attr);
	/* always set as replace. This works because on add ops, the flag
	   is ignored */
	el->flags = LDB_FLAG_MOD_REPLACE;

	return LDB_SUCCESS;
}

/*
  add a uint64_t element to a record
*/
static int add_uint64_element(struct ldb_context *ldb, struct ldb_message *msg,
			      const char *attr, uint64_t v)
{
	struct ldb_message_element *el;
	int ret;

	if (ldb_msg_find_element(msg, attr) != NULL) {
		return LDB_SUCCESS;
	}

	ret = samdb_msg_add_uint64(ldb, msg, msg, attr, v);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	el = ldb_msg_find_element(msg, attr);
	/* always set as replace. This works because on add ops, the flag
	   is ignored */
	el->flags = LDB_FLAG_MOD_REPLACE;

	return LDB_SUCCESS;
}

struct og_context {
	struct ldb_module *module;
	struct ldb_request *req;
};

/* add_record: add objectGUID and timestamp attributes */
static int objectguid_add(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb;
	struct ldb_request *down_req;
	struct ldb_message *msg;
	struct ldb_message_element *el;
	struct GUID guid;
	uint64_t seq_num;
	int ret;
	time_t t = time(NULL);
	struct og_context *ac;

	ldb = ldb_module_get_ctx(module);

	ldb_debug(ldb, LDB_DEBUG_TRACE, "objectguid_add_record\n");

	/* do not manipulate our control entries */
	if (ldb_dn_is_special(req->op.add.message->dn)) {
		return ldb_next_request(module, req);
	}

	el = ldb_msg_find_element(req->op.add.message, "objectGUID");
	if (el != NULL) {
		ldb_set_errstring(ldb,
				  "objectguid: objectGUID must not be specified!");
		return LDB_ERR_UNWILLING_TO_PERFORM;
	}

	ac = talloc(req, struct og_context);
	if (ac == NULL) {
		return ldb_oom(ldb);
	}
	ac->module = module;
	ac->req = req;

	/* we have to copy the message as the caller might have it as a const */
	msg = ldb_msg_copy_shallow(ac, req->op.add.message);
	if (msg == NULL) {
		talloc_free(ac);
		return ldb_operr(ldb);
	}

	/* a new GUID */
	guid = GUID_random();

	ret = dsdb_msg_add_guid(msg, &guid, "objectGUID");
	if (ret != LDB_SUCCESS) {
		return ret;
	}
	
	if (add_time_element(msg, "whenCreated", t) != LDB_SUCCESS ||
	    add_time_element(msg, "whenChanged", t) != LDB_SUCCESS) {
		return ldb_operr(ldb);
	}

	/* Get a sequence number from the backend */
	ret = ldb_sequence_number(ldb, LDB_SEQ_NEXT, &seq_num);
	if (ret == LDB_SUCCESS) {
		if (add_uint64_element(ldb, msg, "uSNCreated",
				       seq_num) != LDB_SUCCESS ||
		    add_uint64_element(ldb, msg, "uSNChanged",
				       seq_num) != LDB_SUCCESS) {
			return ldb_operr(ldb);
		}
	}

	ret = ldb_build_add_req(&down_req, ldb, ac,
				msg,
				req->controls,
				req, dsdb_next_callback,
				req);
	LDB_REQ_SET_LOCATION(down_req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	/* go on with the call chain */
	return ldb_next_request(module, down_req);
}

/* modify_record: update timestamps */
static int objectguid_modify(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb;
	struct ldb_request *down_req;
	struct ldb_message *msg;
	struct ldb_message_element *el;
	int ret;
	time_t t = time(NULL);
	uint64_t seq_num;
	struct og_context *ac;

	ldb = ldb_module_get_ctx(module);

	ldb_debug(ldb, LDB_DEBUG_TRACE, "objectguid_modify_record\n");

	/* do not manipulate our control entries */
	if (ldb_dn_is_special(req->op.add.message->dn)) {
		return ldb_next_request(module, req);
	}

	el = ldb_msg_find_element(req->op.mod.message, "objectGUID");
	if (el != NULL) {
		ldb_set_errstring(ldb,
				  "objectguid: objectGUID must not be specified!");
		return LDB_ERR_CONSTRAINT_VIOLATION;
	}

	ac = talloc(req, struct og_context);
	if (ac == NULL) {
		return ldb_oom(ldb);
	}
	ac->module = module;
	ac->req = req;

	/* we have to copy the message as the caller might have it as a const */
	msg = ldb_msg_copy_shallow(ac, req->op.mod.message);
	if (msg == NULL) {
		return ldb_operr(ldb);
	}

	if (add_time_element(msg, "whenChanged", t) != LDB_SUCCESS) {
		return ldb_operr(ldb);
	}

	/* Get a sequence number from the backend */
	ret = ldb_sequence_number(ldb, LDB_SEQ_NEXT, &seq_num);
	if (ret == LDB_SUCCESS) {
		if (add_uint64_element(ldb, msg, "uSNChanged",
				       seq_num) != LDB_SUCCESS) {
			return ldb_operr(ldb);
		}
	}

	ret = ldb_build_mod_req(&down_req, ldb, ac,
				msg,
				req->controls,
				req, dsdb_next_callback,
				req);
	LDB_REQ_SET_LOCATION(down_req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	/* go on with the call chain */
	return ldb_next_request(module, down_req);
}

static const struct ldb_module_ops ldb_objectguid_module_ops = {
	.name          = "objectguid",
	.add           = objectguid_add,
	.modify        = objectguid_modify
};

int ldb_objectguid_module_init(const char *version)
{
	LDB_MODULE_CHECK_VERSION(version);
	return ldb_register_module(&ldb_objectguid_module_ops);
}
