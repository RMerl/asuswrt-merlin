/* 
   ldb database library

   Copyright (C) Simo Sorce  2005-2006

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
 *  Name: paged_searches
 *
 *  Component: ldb paged searches module
 *
 *  Description: this module detects if the remote ldap server supports
 *               paged results and use them to transparently access all objects
 *
 *  Author: Simo Sorce
 */

#include "includes.h"
#include "ldb/include/includes.h"

#define PS_DEFAULT_PAGE_SIZE 500
/* 500 objects per query seem to be a decent compromise
 * the default AD limit per request is 1000 entries */

struct private_data {

	bool paged_supported;
};

struct ps_context {
	struct ldb_module *module;
	void *up_context;
	int (*up_callback)(struct ldb_context *, void *, struct ldb_reply *);

	struct ldb_request *orig_req;

	struct ldb_request *new_req;

	bool pending;

	char **saved_referrals;
	int num_referrals;
};

static struct ldb_handle *init_handle(void *mem_ctx, struct ldb_module *module,
					    void *context,
					    int (*callback)(struct ldb_context *, void *, struct ldb_reply *))
{
	struct ps_context *ac;
	struct ldb_handle *h;

	h = talloc_zero(mem_ctx, struct ldb_handle);
	if (h == NULL) {
		ldb_set_errstring(module->ldb, "Out of Memory");
		return NULL;
	}

	h->module = module;

	ac = talloc_zero(h, struct ps_context);
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

	ac->pending = False;
	ac->saved_referrals = NULL;
	ac->num_referrals = 0;

	return h;
}

static int check_ps_continuation(struct ldb_reply *ares, struct ps_context *ac)
{
	struct ldb_paged_control *rep_control, *req_control;

	/* look up our paged control */
	if (!ares->controls || strcmp(LDB_CONTROL_PAGED_RESULTS_OID, ares->controls[0]->oid) != 0) {
		/* something wrong here */
		return LDB_ERR_OPERATIONS_ERROR;
	}

	rep_control = talloc_get_type(ares->controls[0]->data, struct ldb_paged_control);
	if (rep_control->cookie_len == 0) {
		/* we are done */
		ac->pending = False;
		return LDB_SUCCESS;
	}

	/* more processing required */
	/* let's fill in the request control with the new cookie */
	/* if there's a reply control we must find a request
	 * control matching it */

	if (strcmp(LDB_CONTROL_PAGED_RESULTS_OID, ac->new_req->controls[0]->oid) != 0) {
		/* something wrong here */
		return LDB_ERR_OPERATIONS_ERROR;
	}

	req_control = talloc_get_type(ac->new_req->controls[0]->data, struct ldb_paged_control);

	if (req_control->cookie) {
		talloc_free(req_control->cookie);
	}

	req_control->cookie = talloc_memdup(req_control,
					    rep_control->cookie,
					    rep_control->cookie_len);
	req_control->cookie_len = rep_control->cookie_len;

	ac->pending = True;
	return LDB_SUCCESS;
}

static int store_referral(char *referral, struct ps_context *ac)
{
	ac->saved_referrals = talloc_realloc(ac, ac->saved_referrals, char *, ac->num_referrals + 2);
	if (!ac->saved_referrals) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ac->saved_referrals[ac->num_referrals] = talloc_strdup(ac->saved_referrals, referral);
	if (!ac->saved_referrals[ac->num_referrals]) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ac->num_referrals++;
	ac->saved_referrals[ac->num_referrals] = NULL;

	return LDB_SUCCESS;
}

static int send_referrals(struct ldb_context *ldb, struct ps_context *ac)
{
	struct ldb_reply *ares;
	int i;

	for (i = 0; i < ac->num_referrals; i++) {
		ares = talloc_zero(ac, struct ldb_reply);
		if (!ares) {
			return LDB_ERR_OPERATIONS_ERROR;
		}

		ares->type = LDB_REPLY_REFERRAL;
		ares->referral = ac->saved_referrals[i];

		ac->up_callback(ldb, ac->up_context, ares);
	}

	return LDB_SUCCESS;
}

static int ps_callback(struct ldb_context *ldb, void *context, struct ldb_reply *ares)
{
	struct ps_context *ac = NULL;
	int ret = LDB_ERR_OPERATIONS_ERROR;

	if (!context || !ares) {
		ldb_set_errstring(ldb, "NULL Context or Result in callback");
		goto error;
	}

	ac = talloc_get_type(context, struct ps_context);

	switch (ares->type) {
	case LDB_REPLY_ENTRY:
		ac->up_callback(ldb, ac->up_context, ares);
		break;

	case LDB_REPLY_REFERRAL:
		ret = store_referral(ares->referral, ac);
		if (ret != LDB_SUCCESS) {
			goto error;
		}
		break;

	case LDB_REPLY_DONE:
		ret = check_ps_continuation(ares, ac);
		if (ret != LDB_SUCCESS) {
			goto error;
		}
		if (!ac->pending) {
			/* send referrals */
			ret = send_referrals(ldb, ac);
			if (ret != LDB_SUCCESS) {
				goto error;
			}

			/* send REPLY_DONE */
			ac->up_callback(ldb, ac->up_context, ares);
		}
		break;
	default:
		goto error;
	}

	return LDB_SUCCESS;

error:
	talloc_free(ares);
	return ret;
}

static int ps_search(struct ldb_module *module, struct ldb_request *req)
{
	struct private_data *private_data;
	struct ldb_paged_control *control;
	struct ps_context *ac;
	struct ldb_handle *h;

	private_data = talloc_get_type(module->private_data, struct private_data);

	/* check if paging is supported and if there is a any control */
	if (!private_data || !private_data->paged_supported || req->controls) {
		/* do not touch this request paged controls not
		 * supported or explicit controls have been set or we
		 * are just not setup yet */
		return ldb_next_request(module, req);
	}

	if (!req->callback || !req->context) {
		ldb_set_errstring(module->ldb,
				  "Async interface called with NULL callback function or NULL context");
		return LDB_ERR_OPERATIONS_ERROR;
	}
	
	h = init_handle(req, module, req->context, req->callback);
	if (!h) {
		return LDB_ERR_OPERATIONS_ERROR;
	}
	ac = talloc_get_type(h->private_data, struct ps_context);

	ac->new_req = talloc(ac, struct ldb_request);
	if (!ac->new_req) return LDB_ERR_OPERATIONS_ERROR;

	ac->new_req->controls = talloc_array(ac->new_req, struct ldb_control *, 2);
	if (!ac->new_req->controls) return LDB_ERR_OPERATIONS_ERROR;

	ac->new_req->controls[0] = talloc(ac->new_req->controls, struct ldb_control);
	if (!ac->new_req->controls[0]) return LDB_ERR_OPERATIONS_ERROR;

	control = talloc(ac->new_req->controls[0], struct ldb_paged_control);
	if (!control) return LDB_ERR_OPERATIONS_ERROR;

	control->size = PS_DEFAULT_PAGE_SIZE;
	control->cookie = NULL;
	control->cookie_len = 0;

	ac->new_req->controls[0]->oid = LDB_CONTROL_PAGED_RESULTS_OID;
	ac->new_req->controls[0]->critical = 1;
	ac->new_req->controls[0]->data = control;

	ac->new_req->controls[1] = NULL;

	ac->new_req->operation = req->operation;
	ac->new_req->op.search.base = req->op.search.base;
	ac->new_req->op.search.scope = req->op.search.scope;
	ac->new_req->op.search.tree = req->op.search.tree;
	ac->new_req->op.search.attrs = req->op.search.attrs;
	ac->new_req->context = ac;
	ac->new_req->callback = ps_callback;
	ldb_set_timeout_from_prev_req(module->ldb, req, ac->new_req);

	req->handle = h;

	return ldb_next_request(module, ac->new_req);
}

static int ps_continuation(struct ldb_handle *handle)
{
	struct ps_context *ac;

	if (!handle || !handle->private_data) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ac = talloc_get_type(handle->private_data, struct ps_context);

	/* reset the requests handle */
	ac->new_req->handle = NULL;

	return ldb_next_request(handle->module, ac->new_req);
}

static int ps_wait_none(struct ldb_handle *handle)
{
	struct ps_context *ac;
	int ret;
    
	if (!handle || !handle->private_data) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if (handle->state == LDB_ASYNC_DONE) {
		return handle->status;
	}

	handle->state = LDB_ASYNC_PENDING;
	handle->status = LDB_SUCCESS;

	ac = talloc_get_type(handle->private_data, struct ps_context);

	ret = ldb_wait(ac->new_req->handle, LDB_WAIT_NONE);

	if (ret != LDB_SUCCESS) {
		handle->status = ret;
		goto done;
	}

	if (ac->new_req->handle->status != LDB_SUCCESS) {
		handle->status = ac->new_req->handle->status;
		goto done;
	}

	if (ac->new_req->handle->state != LDB_ASYNC_DONE) {
		return LDB_SUCCESS;
	} 

	/* see if we need to send another request for the next batch */
	if (ac->pending) {
		ret = ps_continuation(handle);
		if (ret != LDB_SUCCESS) {
			handle->status = ret;
			goto done;
		}

		/* continue the search with the next request */
		return LDB_SUCCESS;
	}

	ret = LDB_SUCCESS;

done:
	handle->state = LDB_ASYNC_DONE;
	return ret;
}

static int ps_wait_all(struct ldb_handle *handle)
{
	int ret;

	while (handle->state != LDB_ASYNC_DONE) {
		ret = ps_wait_none(handle);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}

	return handle->status;
}

static int ps_wait(struct ldb_handle *handle, enum ldb_wait_type type)
{
	if (type == LDB_WAIT_ALL) {
		return ps_wait_all(handle);
	} else {
		return ps_wait_none(handle);
	}
}

static int check_supported_paged(struct ldb_context *ldb, void *context, 
				 struct ldb_reply *ares) 
{
	struct private_data *data;
	data = talloc_get_type(context,
			       struct private_data);
	if (ares->type == LDB_REPLY_ENTRY) {
		if (ldb_msg_check_string_attribute(ares->message,
						   "supportedControl",
						   LDB_CONTROL_PAGED_RESULTS_OID)) {
			data->paged_supported = True;
		}
	}
	return LDB_SUCCESS;
}


static int ps_init(struct ldb_module *module)
{
	static const char *attrs[] = { "supportedControl", NULL };
	struct private_data *data;
	int ret;
	struct ldb_request *req;

	data = talloc(module, struct private_data);
	if (data == NULL) {
		return LDB_ERR_OTHER;
	}
	module->private_data = data;
	data->paged_supported = False;

	req = talloc(module, struct ldb_request);
	if (req == NULL) {
		ldb_set_errstring(module->ldb, "Out of Memory");
		return LDB_ERR_OPERATIONS_ERROR;
	}

	req->operation = LDB_SEARCH;
	req->op.search.base = ldb_dn_new(req);
	req->op.search.scope = LDB_SCOPE_BASE;

	req->op.search.tree = ldb_parse_tree(req, "objectClass=*");
	if (req->op.search.tree == NULL) {
		ldb_set_errstring(module->ldb, "Unable to parse search expression");
		talloc_free(req);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	req->op.search.attrs = attrs;
	req->controls = NULL;
	req->context = data;
	req->callback = check_supported_paged;
	ldb_set_timeout(module->ldb, req, 0); /* use default timeout */

	ret = ldb_next_request(module, req);
	
	if (ret == LDB_SUCCESS) {
		ret = ldb_wait(req->handle, LDB_WAIT_ALL);
	}
	
	talloc_free(req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	return ldb_next_init(module);
}

static const struct ldb_module_ops ps_ops = {
	.name           = "paged_searches",
	.search         = ps_search,
	.wait           = ps_wait,
	.init_context 	= ps_init
};

int ldb_paged_searches_init(void)
{
	return ldb_register_module(&ps_ops);
}

