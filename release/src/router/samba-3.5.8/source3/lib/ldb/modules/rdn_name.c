/* 
   ldb database library

   Copyright (C) Andrew Bartlet 2005
   Copyright (C) Simo Sorce     2006

     ** NOTE! The following LGPL license applies to the ldb
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

/*
 *  Name: rdb_name
 *
 *  Component: ldb rdn name module
 *
 *  Description: keep a consistent name attribute on objects manpulations
 *
 *  Author: Andrew Bartlet
 *
 *  Modifications:
 *    - made the module async
 *      Simo Sorce Mar 2006
 */

#include "includes.h"
#include "ldb/include/includes.h"

static struct ldb_message_element *rdn_name_find_attribute(const struct ldb_message *msg, const char *name)
{
	int i;

	for (i = 0; i < msg->num_elements; i++) {
		if (ldb_attr_cmp(name, msg->elements[i].name) == 0) {
			return &msg->elements[i];
		}
	}

	return NULL;
}

static int rdn_name_add(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_request *down_req;
	struct ldb_message *msg;
	struct ldb_message_element *attribute;
	const char *rdn_name;
	struct ldb_val rdn_val;
	int i, ret;

	ldb_debug(module->ldb, LDB_DEBUG_TRACE, "rdn_name_add_record\n");

	/* do not manipulate our control entries */
	if (ldb_dn_is_special(req->op.add.message->dn)) {
		return ldb_next_request(module, req);
	}

	down_req = talloc(req, struct ldb_request);
	if (down_req == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	*down_req = *req;

	down_req->op.add.message = msg = ldb_msg_copy_shallow(down_req, req->op.add.message);
	if (msg == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	rdn_name = ldb_dn_get_rdn_name(msg->dn);
	if (rdn_name == NULL) {
		talloc_free(down_req);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	
	rdn_val = ldb_val_dup(msg, ldb_dn_get_rdn_val(msg->dn));
	
	/* Perhaps someone above us tried to set this? */
	if ((attribute = rdn_name_find_attribute(msg, "name")) != NULL ) {
		attribute->num_values = 0;
	}

	if (ldb_msg_add_value(msg, "name", &rdn_val, NULL) != 0) {
		talloc_free(down_req);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	attribute = rdn_name_find_attribute(msg, rdn_name);

	if (!attribute) {
		if (ldb_msg_add_value(msg, rdn_name, &rdn_val, NULL) != 0) {
			talloc_free(down_req);
			return LDB_ERR_OPERATIONS_ERROR;
		}
	} else {
		const struct ldb_attrib_handler *handler = ldb_attrib_handler(module->ldb, rdn_name);

		for (i = 0; i < attribute->num_values; i++) {
			if (handler->comparison_fn(module->ldb, msg, &rdn_val, &attribute->values[i]) == 0) {
				/* overwrite so it matches in case */
				attribute->values[i] = rdn_val;
				break;
			}
		}
		if (i == attribute->num_values) {
			ldb_debug_set(module->ldb, LDB_DEBUG_FATAL, 
				      "RDN mismatch on %s: %s (%s)", 
				      ldb_dn_linearize(msg, msg->dn), rdn_name, rdn_val.data);
			talloc_free(down_req);
			return LDB_ERR_OPERATIONS_ERROR;
		}
	}

	/* go on with the call chain */
	ret = ldb_next_request(module, down_req);

	/* do not free down_req as the call results may be linked to it,
	 * it will be freed when the upper level request get freed */
	if (ret == LDB_SUCCESS) {
		req->handle = down_req->handle;
	}

	return ret;
}

struct rename_context {

	enum {RENAME_RENAME, RENAME_MODIFY} step;
	struct ldb_request *orig_req;
	struct ldb_request *down_req;
	struct ldb_request *mod_req;
};

static int rdn_name_rename(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_handle *h;
	struct rename_context *ac;

	ldb_debug(module->ldb, LDB_DEBUG_TRACE, "rdn_name_rename\n");

	/* do not manipulate our control entries */
	if (ldb_dn_is_special(req->op.rename.newdn)) {
		return ldb_next_request(module, req);
	}

	h = talloc_zero(req, struct ldb_handle);
	if (h == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	h->module = module;

	ac = talloc_zero(h, struct rename_context);
	if (ac == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	h->private_data = (void *)ac;

	h->state = LDB_ASYNC_INIT;
	h->status = LDB_SUCCESS;

	ac->orig_req = req;
	ac->down_req = talloc(req, struct ldb_request);
	if (ac->down_req == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	*(ac->down_req) = *req;

	ac->step = RENAME_RENAME;

	req->handle = h;

	/* rename first, modify "name" if rename is ok */
	return ldb_next_request(module, ac->down_req);
}

static int rdn_name_rename_do_mod(struct ldb_handle *h) {

	struct rename_context *ac;
	const char *rdn_name;
	struct ldb_val rdn_val;
	struct ldb_message *msg;

	ac = talloc_get_type(h->private_data, struct rename_context);

	ac->mod_req = talloc_zero(ac, struct ldb_request);

	ac->mod_req->operation = LDB_MODIFY;
	ac->mod_req->op.mod.message = msg = ldb_msg_new(ac->mod_req);
	if (msg == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	msg->dn = ldb_dn_copy(msg, ac->orig_req->op.rename.newdn);
	if (msg->dn == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	rdn_name = ldb_dn_get_rdn_name(ac->orig_req->op.rename.newdn);
	if (rdn_name == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}
	
	rdn_val = ldb_val_dup(msg, ldb_dn_get_rdn_val(ac->orig_req->op.rename.newdn));
	
	if (ldb_msg_add_empty(msg, rdn_name, LDB_FLAG_MOD_REPLACE, NULL) != 0) {
		return LDB_ERR_OPERATIONS_ERROR;
	}
	if (ldb_msg_add_value(msg, rdn_name, &rdn_val, NULL) != 0) {
		return LDB_ERR_OPERATIONS_ERROR;
	}
	if (ldb_msg_add_empty(msg, "name", LDB_FLAG_MOD_REPLACE, NULL) != 0) {
		return LDB_ERR_OPERATIONS_ERROR;
	}
	if (ldb_msg_add_value(msg, "name", &rdn_val, NULL) != 0) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ldb_set_timeout_from_prev_req(h->module->ldb, ac->orig_req, ac->mod_req);

	ac->step = RENAME_MODIFY;

	/* do the mod call */
	return ldb_request(h->module->ldb, ac->mod_req);
}

static int rename_wait(struct ldb_handle *handle)
{
	struct rename_context *ac;
	int ret;
    
	if (!handle || !handle->private_data) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if (handle->state == LDB_ASYNC_DONE) {
		return handle->status;
	}

	handle->state = LDB_ASYNC_PENDING;
	handle->status = LDB_SUCCESS;

	ac = talloc_get_type(handle->private_data, struct rename_context);

	switch(ac->step) {
	case RENAME_RENAME:
		ret = ldb_wait(ac->down_req->handle, LDB_WAIT_NONE);
		if (ret != LDB_SUCCESS) {
			handle->status = ret;
			goto done;
		}
		if (ac->down_req->handle->status != LDB_SUCCESS) {
			handle->status = ac->down_req->handle->status;
			goto done;
		}

		if (ac->down_req->handle->state != LDB_ASYNC_DONE) {
			return LDB_SUCCESS;
		}

		/* rename operation done */
		return rdn_name_rename_do_mod(handle);

	case RENAME_MODIFY:
		ret = ldb_wait(ac->mod_req->handle, LDB_WAIT_NONE);
		if (ret != LDB_SUCCESS) {
			handle->status = ret;
			goto done;
		}
		if (ac->mod_req->handle->status != LDB_SUCCESS) {
			handle->status = ac->mod_req->handle->status;
			goto done;
		}

		if (ac->mod_req->handle->state != LDB_ASYNC_DONE) {
			return LDB_SUCCESS;
		}

		break;

	default:
		ret = LDB_ERR_OPERATIONS_ERROR;
		goto done;
	}

	ret = LDB_SUCCESS;

done:
	handle->state = LDB_ASYNC_DONE;
	return ret;
}

static int rename_wait_all(struct ldb_handle *handle) {

	int ret;

	while (handle->state != LDB_ASYNC_DONE) {
		ret = rename_wait(handle);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}

	return handle->status;
}

static int rdn_name_wait(struct ldb_handle *handle, enum ldb_wait_type type)
{
	if (type == LDB_WAIT_ALL) {
		return rename_wait_all(handle);
	} else {
		return rename_wait(handle);
	}
}

static const struct ldb_module_ops rdn_name_ops = {
	.name              = "rdn_name",
	.add               = rdn_name_add,
	.rename            = rdn_name_rename,
	.wait              = rdn_name_wait
};


int ldb_rdn_name_init(void)
{
	return ldb_register_module(&rdn_name_ops);
}
