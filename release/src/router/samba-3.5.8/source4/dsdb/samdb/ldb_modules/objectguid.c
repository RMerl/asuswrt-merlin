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
#include "librpc/gen_ndr/ndr_misc.h"
#include "param/param.h"

static struct ldb_message_element *objectguid_find_attribute(const struct ldb_message *msg, const char *name)
{
	int i;

	for (i = 0; i < msg->num_elements; i++) {
		if (ldb_attr_cmp(name, msg->elements[i].name) == 0) {
			return &msg->elements[i];
		}
	}

	return NULL;
}

/*
  add a time element to a record
*/
static int add_time_element(struct ldb_message *msg, const char *attr, time_t t)
{
	struct ldb_message_element *el;
	char *s;

	if (ldb_msg_find_element(msg, attr) != NULL) {
		return 0;
	}

	s = ldb_timestring(msg, t);
	if (s == NULL) {
		return -1;
	}

	if (ldb_msg_add_string(msg, attr, s) != 0) {
		return -1;
	}

	el = ldb_msg_find_element(msg, attr);
	/* always set as replace. This works because on add ops, the flag
	   is ignored */
	el->flags = LDB_FLAG_MOD_REPLACE;

	return 0;
}

/*
  add a uint64_t element to a record
*/
static int add_uint64_element(struct ldb_message *msg, const char *attr, uint64_t v)
{
	struct ldb_message_element *el;

	if (ldb_msg_find_element(msg, attr) != NULL) {
		return 0;
	}

	if (ldb_msg_add_fmt(msg, attr, "%llu", (unsigned long long)v) != 0) {
		return -1;
	}

	el = ldb_msg_find_element(msg, attr);
	/* always set as replace. This works because on add ops, the flag
	   is ignored */
	el->flags = LDB_FLAG_MOD_REPLACE;

	return 0;
}

struct og_context {
	struct ldb_module *module;
	struct ldb_request *req;
};

static int og_op_callback(struct ldb_request *req, struct ldb_reply *ares)
{
	struct og_context *ac;

	ac = talloc_get_type(req->context, struct og_context);

	if (!ares) {
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}
	if (ares->error != LDB_SUCCESS) {
		return ldb_module_done(ac->req, ares->controls,
					ares->response, ares->error);
	}

	if (ares->type != LDB_REPLY_DONE) {
		talloc_free(ares);
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}

	return ldb_module_done(ac->req, ares->controls,
				ares->response, ares->error);
}

/* add_record: add objectGUID attribute */
static int objectguid_add(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb;
	struct ldb_request *down_req;
	struct ldb_message_element *attribute;
	struct ldb_message *msg;
	struct ldb_val v;
	struct GUID guid;
	uint64_t seq_num;
	enum ndr_err_code ndr_err;
	int ret;
	time_t t = time(NULL);
	struct og_context *ac;

	ldb = ldb_module_get_ctx(module);

	ldb_debug(ldb, LDB_DEBUG_TRACE, "objectguid_add_record\n");

	/* do not manipulate our control entries */
	if (ldb_dn_is_special(req->op.add.message->dn)) {
		return ldb_next_request(module, req);
	}

	if ((attribute = objectguid_find_attribute(req->op.add.message, "objectGUID")) != NULL ) {
		return ldb_next_request(module, req);
	}

	ac = talloc(req, struct og_context);
	if (ac == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}
	ac->module = module;
	ac->req = req;

	/* we have to copy the message as the caller might have it as a const */
	msg = ldb_msg_copy_shallow(ac, req->op.add.message);
	if (msg == NULL) {
		talloc_free(down_req);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* a new GUID */
	guid = GUID_random();

	ndr_err = ndr_push_struct_blob(&v, msg, 
				       lp_iconv_convenience(ldb_get_opaque(ldb, "loadparm")),
				       &guid,
				       (ndr_push_flags_fn_t)ndr_push_GUID);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = ldb_msg_add_value(msg, "objectGUID", &v, NULL);
	if (ret) {
		return ret;
	}
	
	if (add_time_element(msg, "whenCreated", t) != 0 ||
	    add_time_element(msg, "whenChanged", t) != 0) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* Get a sequence number from the backend */
	/* FIXME: ldb_sequence_number is a semi-async call,
	 * make sure this function is split and a callback is used */
	ret = ldb_sequence_number(ldb, LDB_SEQ_NEXT, &seq_num);
	if (ret == LDB_SUCCESS) {
		if (add_uint64_element(msg, "uSNCreated", seq_num) != 0 ||
		    add_uint64_element(msg, "uSNChanged", seq_num) != 0) {
			return LDB_ERR_OPERATIONS_ERROR;
		}
	}

	ret = ldb_build_add_req(&down_req, ldb, ac,
				msg,
				req->controls,
				ac, og_op_callback,
				req);
	if (ret != LDB_SUCCESS) {
		return LDB_ERR_OPERATIONS_ERROR;
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
	int ret;
	time_t t = time(NULL);
	uint64_t seq_num;
	struct og_context *ac;

	ldb = ldb_module_get_ctx(module);

	ldb_debug(ldb, LDB_DEBUG_TRACE, "objectguid_add_record\n");

	/* do not manipulate our control entries */
	if (ldb_dn_is_special(req->op.add.message->dn)) {
		return ldb_next_request(module, req);
	}

	ac = talloc(req, struct og_context);
	if (ac == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}
	ac->module = module;
	ac->req = req;

	/* we have to copy the message as the caller might have it as a const */
	msg = ldb_msg_copy_shallow(ac, req->op.mod.message);
	if (msg == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if (add_time_element(msg, "whenChanged", t) != 0) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* Get a sequence number from the backend */
	ret = ldb_sequence_number(ldb, LDB_SEQ_NEXT, &seq_num);
	if (ret == LDB_SUCCESS) {
		if (add_uint64_element(msg, "uSNChanged", seq_num) != 0) {
			return LDB_ERR_OPERATIONS_ERROR;
		}
	}

	ret = ldb_build_mod_req(&down_req, ldb, ac,
				msg,
				req->controls,
				ac, og_op_callback,
				req);
	if (ret != LDB_SUCCESS) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* go on with the call chain */
	return ldb_next_request(module, down_req);
}

_PUBLIC_ const struct ldb_module_ops ldb_objectguid_module_ops = {
	.name          = "objectguid",
	.add           = objectguid_add,
	.modify        = objectguid_modify,
};
