/* 
   ldb database library

   Copyright (C) Simo Sorce  2005

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
 *  Name: ldb
 *
 *  Component: ldb attribute scoped query control module
 *
 *  Description: this module searches all the the objects pointed
 *  		 by the DNs contained in the references attribute
 *
 *  Author: Simo Sorce
 */

#include "includes.h"
#include "ldb/include/includes.h"

struct asq_context {

	enum {ASQ_SEARCH_BASE, ASQ_SEARCH_MULTI} step;

	struct ldb_module *module;
	void *up_context;
	int (*up_callback)(struct ldb_context *, void *, struct ldb_reply *);

	const char * const *req_attrs;
	char *req_attribute;
	enum {
		ASQ_CTRL_SUCCESS			= 0,
		ASQ_CTRL_INVALID_ATTRIBUTE_SYNTAX	= 21,
		ASQ_CTRL_UNWILLING_TO_PERFORM		= 53,
		ASQ_CTRL_AFFECTS_MULTIPLE_DSA		= 71
	} asq_ret;

	struct ldb_request *base_req;
	struct ldb_reply *base_res;

	struct ldb_request **reqs;
	int num_reqs;
	int cur_req;

	struct ldb_control **controls;
};

static struct ldb_handle *init_handle(void *mem_ctx, struct ldb_module *module,
					    void *context,
					    int (*callback)(struct ldb_context *, void *, struct ldb_reply *))
{
	struct asq_context *ac;
	struct ldb_handle *h;

	h = talloc_zero(mem_ctx, struct ldb_handle);
	if (h == NULL) {
		ldb_set_errstring(module->ldb, "Out of Memory");
		return NULL;
	}

	h->module = module;

	ac = talloc_zero(h, struct asq_context);
	if (ac == NULL) {
		ldb_set_errstring(module->ldb, "Out of Memory");
		talloc_free(h);
		return NULL;
	}

	h->private_data = (void *)ac;

	h->state = LDB_ASYNC_INIT;
	h->status = LDB_SUCCESS;

	ac->module = module;
	ac->up_context = context;
	ac->up_callback = callback;

	return h;
}

static int asq_terminate(struct ldb_handle *handle)
{
	struct asq_context *ac;
	struct ldb_reply *ares;
	struct ldb_asq_control *asq;
	int i;

	ac = talloc_get_type(handle->private_data, struct asq_context);
	if (ac == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	handle->status = LDB_SUCCESS;
	handle->state = LDB_ASYNC_DONE;

	ares = talloc_zero(ac, struct ldb_reply);
	if (ares == NULL)
		return LDB_ERR_OPERATIONS_ERROR;

	ares->type = LDB_REPLY_DONE;

	if (ac->controls) {
		for (i = 0; ac->controls[i]; i++);
		ares->controls = talloc_move(ares, &ac->controls);
	} else {
		i = 0;
	}

	ares->controls = talloc_realloc(ares, ares->controls, struct ldb_control *, i + 2);
	
	if (ares->controls == NULL)
		return LDB_ERR_OPERATIONS_ERROR;

	ares->controls[i] = talloc(ares->controls, struct ldb_control);
	if (ares->controls[i] == NULL)
		return LDB_ERR_OPERATIONS_ERROR;

	ares->controls[i]->oid = LDB_CONTROL_ASQ_OID;
	ares->controls[i]->critical = 0;

	asq = talloc_zero(ares->controls[i], struct ldb_asq_control);
	if (asq == NULL)
		return LDB_ERR_OPERATIONS_ERROR;

	asq->result = ac->asq_ret;
	
	ares->controls[i]->data = asq;

	ares->controls[i + 1] = NULL;

	ac->up_callback(ac->module->ldb, ac->up_context, ares);

	return LDB_SUCCESS;
}

static int asq_base_callback(struct ldb_context *ldb, void *context, struct ldb_reply *ares)
{
	struct asq_context *ac;

	if (!context || !ares) {
		ldb_set_errstring(ldb, "NULL Context or Result in callback");
		goto error;
	}

	if (!(ac = talloc_get_type(context, struct asq_context))) {
		goto error;
	}

	/* we are interested only in the single reply (base search) we receive here */
	if (ares->type == LDB_REPLY_ENTRY) {
		ac->base_res = talloc_move(ac, &ares);
	} else {
		talloc_free(ares);
	}

	return LDB_SUCCESS;
error:
	talloc_free(ares);
	return LDB_ERR_OPERATIONS_ERROR;
}

static int asq_reqs_callback(struct ldb_context *ldb, void *context, struct ldb_reply *ares)
{
	struct asq_context *ac;

	if (!context || !ares) {
		ldb_set_errstring(ldb, "NULL Context or Result in callback");
		goto error;
	}

	if (!(ac = talloc_get_type(context, struct asq_context))) {
		goto error;
	}

	/* we are interested only in the single reply (base search) we receive here */
	if (ares->type == LDB_REPLY_ENTRY) {

		/* pass the message up to the original callback as we
		 * do not have to elaborate on it any further */
		return ac->up_callback(ac->module->ldb, ac->up_context, ares);
		
	} else { /* ignore any REFERRAL or DONE reply */
		talloc_free(ares);
	}

	return LDB_SUCCESS;
error:
	talloc_free(ares);
	return LDB_ERR_OPERATIONS_ERROR;
}

static int asq_search(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_control *control;
	struct ldb_asq_control *asq_ctrl;
	struct asq_context *ac;
	struct ldb_handle *h;
	char **base_attrs;
	int ret;

	/* check if there's a paged request control */
	control = get_control_from_list(req->controls, LDB_CONTROL_ASQ_OID);
	if (control == NULL) {
		/* not found go on */
		return ldb_next_request(module, req);
	}

	req->handle = NULL;

	if (!req->callback || !req->context) {
		ldb_set_errstring(module->ldb,
				  "Async interface called with NULL callback function or NULL context");
		return LDB_ERR_OPERATIONS_ERROR;
	}
	
	asq_ctrl = talloc_get_type(control->data, struct ldb_asq_control);
	if (!asq_ctrl) {
		return LDB_ERR_PROTOCOL_ERROR;
	}

	h = init_handle(req, module, req->context, req->callback);
	if (!h) {
		return LDB_ERR_OPERATIONS_ERROR;
	}
	if (!(ac = talloc_get_type(h->private_data, struct asq_context))) {

		return LDB_ERR_OPERATIONS_ERROR;
	}

	req->handle = h;

	/* check the search is well formed */
	if (req->op.search.scope != LDB_SCOPE_BASE) {
		ac->asq_ret = ASQ_CTRL_UNWILLING_TO_PERFORM;
		return asq_terminate(h);
	}

	ac->req_attrs = req->op.search.attrs;
	ac->req_attribute = talloc_strdup(ac, asq_ctrl->source_attribute);
	if (ac->req_attribute == NULL)
		return LDB_ERR_OPERATIONS_ERROR;

	/* get the object to retrieve the DNs to search */
	ac->base_req = talloc_zero(req, struct ldb_request);
	if (ac->base_req == NULL)
		return LDB_ERR_OPERATIONS_ERROR;
	ac->base_req->operation = req->operation;
	ac->base_req->op.search.base = req->op.search.base;
	ac->base_req->op.search.scope = LDB_SCOPE_BASE;
	ac->base_req->op.search.tree = req->op.search.tree;
	base_attrs = talloc_array(ac->base_req, char *, 2);
	if (base_attrs == NULL)
		return LDB_ERR_OPERATIONS_ERROR;
	base_attrs[0] = talloc_strdup(base_attrs, asq_ctrl->source_attribute);
	if (base_attrs[0] == NULL)
		return LDB_ERR_OPERATIONS_ERROR;
	base_attrs[1] = NULL;
	ac->base_req->op.search.attrs = (const char * const *)base_attrs;

	ac->base_req->context = ac;
	ac->base_req->callback = asq_base_callback;
	ldb_set_timeout_from_prev_req(module->ldb, req, ac->base_req);

	ac->step = ASQ_SEARCH_BASE;

	ret = ldb_request(module->ldb, ac->base_req);

	if (ret != LDB_SUCCESS) {
		return ret;
	}

	return LDB_SUCCESS;
}

static int asq_requests(struct ldb_handle *handle) {
	struct asq_context *ac;
	struct ldb_message_element *el;
	int i;

	if (!(ac = talloc_get_type(handle->private_data,
				   struct asq_context))) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* look up the DNs */
	if (ac->base_res == NULL) {
		return LDB_ERR_NO_SUCH_OBJECT;
	}
	el = ldb_msg_find_element(ac->base_res->message, ac->req_attribute);
	/* no values found */
	if (el == NULL) {
		ac->asq_ret = ASQ_CTRL_SUCCESS;
		return asq_terminate(handle);
	}

	/* build up the requests call chain */
	ac->num_reqs = el->num_values;
	ac->cur_req = 0;
	ac->reqs = talloc_array(ac, struct ldb_request *, ac->num_reqs);
	if (ac->reqs == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	for (i = 0; i < el->num_values; i++) {

		ac->reqs[i] = talloc_zero(ac->reqs, struct ldb_request);
		if (ac->reqs[i] == NULL)
			return LDB_ERR_OPERATIONS_ERROR;
		ac->reqs[i]->operation = LDB_SEARCH;
		ac->reqs[i]->op.search.base = ldb_dn_explode(ac->reqs[i], (const char *)el->values[i].data);
		if (ac->reqs[i]->op.search.base == NULL) {
			ac->asq_ret = ASQ_CTRL_INVALID_ATTRIBUTE_SYNTAX;
			return asq_terminate(handle);
		}
		ac->reqs[i]->op.search.scope = LDB_SCOPE_BASE;
		ac->reqs[i]->op.search.tree = ac->base_req->op.search.tree;
		ac->reqs[i]->op.search.attrs = ac->req_attrs;

		ac->reqs[i]->context = ac;
		ac->reqs[i]->callback = asq_reqs_callback;
		ldb_set_timeout_from_prev_req(ac->module->ldb, ac->base_req, ac->reqs[i]);
	}

	ac->step = ASQ_SEARCH_MULTI;

	return LDB_SUCCESS;
}

static int asq_wait_none(struct ldb_handle *handle)
{
	struct asq_context *ac;
	int ret;
    
	if (!handle || !handle->private_data) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if (handle->state == LDB_ASYNC_DONE) {
		return handle->status;
	}

	handle->state = LDB_ASYNC_PENDING;
	handle->status = LDB_SUCCESS;

	if (!(ac = talloc_get_type(handle->private_data,
				   struct asq_context))) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	switch (ac->step) {
	case ASQ_SEARCH_BASE:
		ret = ldb_wait(ac->base_req->handle, LDB_WAIT_NONE);
		
		if (ret != LDB_SUCCESS) {
			handle->status = ret;
			goto done;
		}

		if (ac->base_req->handle->status != LDB_SUCCESS) {
			handle->status = ac->base_req->handle->status;
			goto done;
		}
		if (ac->base_req->handle->state != LDB_ASYNC_DONE) {
			return LDB_SUCCESS;
		}

		ret = asq_requests(handle);

		/* no break nor return,
		 * the set of requests is performed in ASQ_SEARCH_MULTI
		 */
		
	case ASQ_SEARCH_MULTI:

		if (ac->reqs[ac->cur_req]->handle == NULL) {
			ret = ldb_request(ac->module->ldb, ac->reqs[ac->cur_req]);
			if (ret != LDB_SUCCESS) {
				return ret;
			}
		}

		ret = ldb_wait(ac->reqs[ac->cur_req]->handle, LDB_WAIT_NONE);
		
		if (ret != LDB_SUCCESS) {
			handle->status = ret;
			goto done;
		}
		if (ac->reqs[ac->cur_req]->handle->status != LDB_SUCCESS) {
			handle->status = ac->reqs[ac->cur_req]->handle->status;
		}

		if (ac->reqs[ac->cur_req]->handle->state == LDB_ASYNC_DONE) {
			ac->cur_req++;
		}

		if (ac->cur_req < ac->num_reqs) {
			return LDB_SUCCESS;
		}

		return asq_terminate(handle);

	default:
		ret = LDB_ERR_OPERATIONS_ERROR;
		goto done;
	}

	ret = LDB_SUCCESS;

done:
	handle->state = LDB_ASYNC_DONE;
	return ret;
}

static int asq_wait_all(struct ldb_handle *handle)
{
	int ret;

	while (handle->state != LDB_ASYNC_DONE) {
		ret = asq_wait_none(handle);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}

	return handle->status;
}

static int asq_wait(struct ldb_handle *handle, enum ldb_wait_type type)
{
	if (type == LDB_WAIT_ALL) {
		return asq_wait_all(handle);
	} else {
		return asq_wait_none(handle);
	}
}

static int asq_init(struct ldb_module *module)
{
	struct ldb_request *req;
	int ret;

	req = talloc_zero(module, struct ldb_request);
	if (req == NULL) {
		ldb_debug(module->ldb, LDB_DEBUG_ERROR, "asq: Out of memory!\n");
		return LDB_ERR_OPERATIONS_ERROR;
	}

	req->operation = LDB_REQ_REGISTER_CONTROL;
	req->op.reg_control.oid = LDB_CONTROL_ASQ_OID;

	ret = ldb_request(module->ldb, req);
	if (ret != LDB_SUCCESS) {
		ldb_debug(module->ldb, LDB_DEBUG_WARNING, "asq: Unable to register control with rootdse!\n");
	}

	return ldb_next_init(module);
}


static const struct ldb_module_ops asq_ops = {
	.name		   = "asq",
	.search		   = asq_search,
	.wait              = asq_wait,
	.init_context	   = asq_init
};

int ldb_asq_init(void)
{
	return ldb_register_module(&asq_ops);
}
