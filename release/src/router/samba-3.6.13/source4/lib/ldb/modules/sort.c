/* 
   ldb database library

   Copyright (C) Simo Sorce  2005-2008

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
 *  Component: ldb server side sort control module
 *
 *  Description: this module sorts the results of a search
 *
 *  Author: Simo Sorce
 */

#include "replace.h"
#include "system/filesys.h"
#include "system/time.h"
#include "ldb_module.h"

struct opaque {
	struct ldb_context *ldb;
	const struct ldb_attrib_handler *h;
	const char *attribute;
	int reverse;
	int result;
};

struct sort_context {
	struct ldb_module *module;

	const char *attributeName;
	const char *orderingRule;
	int reverse;

	struct ldb_request *req;
	struct ldb_message **msgs;
	char **referrals;
	unsigned int num_msgs;
	unsigned int num_refs;

	const struct ldb_schema_attribute *a;
	int sort_result;
};

static int build_response(void *mem_ctx, struct ldb_control ***ctrls, int result, const char *desc)
{
	struct ldb_control **controls;
	struct ldb_sort_resp_control *resp;
	unsigned int i;

	if (*ctrls) {
		controls = *ctrls;
		for (i = 0; controls[i]; i++);
		controls = talloc_realloc(mem_ctx, controls, struct ldb_control *, i + 2);
	} else {
		i = 0;
		controls = talloc_array(mem_ctx, struct ldb_control *, 2);
	}
	if (! controls )
		return LDB_ERR_OPERATIONS_ERROR;

	*ctrls = controls;

	controls[i+1] = NULL;
	controls[i] = talloc(controls, struct ldb_control);
	if (! controls[i] )
		return LDB_ERR_OPERATIONS_ERROR;

	controls[i]->oid = LDB_CONTROL_SORT_RESP_OID;
	controls[i]->critical = 0;

	resp = talloc(controls[i], struct ldb_sort_resp_control);
	if (! resp )
		return LDB_ERR_OPERATIONS_ERROR;

	resp->result = result;
	resp->attr_desc = talloc_strdup(resp, desc);

	if (! resp->attr_desc )
		return LDB_ERR_OPERATIONS_ERROR;
	
	controls[i]->data = resp;

	return LDB_SUCCESS;
}

static int sort_compare(struct ldb_message **msg1, struct ldb_message **msg2, void *opaque)
{
	struct sort_context *ac = talloc_get_type(opaque, struct sort_context);
	struct ldb_message_element *el1, *el2;
	struct ldb_context *ldb;

	ldb = ldb_module_get_ctx(ac->module);

	if (ac->sort_result != 0) {
		/* an error occurred previously,
		 * let's exit the sorting by returning always 0 */
		return 0;
	}

	el1 = ldb_msg_find_element(*msg1, ac->attributeName);
	el2 = ldb_msg_find_element(*msg2, ac->attributeName);

	if (!el1 && el2) {
		return 1;
	}
	if (el1 && !el2) {
		return -1;
	}
	if (!el1 && !el2) {
		return 0;
	}

	if (ac->reverse)
		return ac->a->syntax->comparison_fn(ldb, ac, &el2->values[0], &el1->values[0]);

	return ac->a->syntax->comparison_fn(ldb, ac, &el1->values[0], &el2->values[0]);
}

static int server_sort_results(struct sort_context *ac)
{
	struct ldb_context *ldb;
	struct ldb_reply *ares;
	unsigned int i;
	int ret;

	ldb = ldb_module_get_ctx(ac->module);

	ac->a = ldb_schema_attribute_by_name(ldb, ac->attributeName);
	ac->sort_result = 0;

	LDB_TYPESAFE_QSORT(ac->msgs, ac->num_msgs, ac, sort_compare);

	if (ac->sort_result != LDB_SUCCESS) {
		return ac->sort_result;
	}

	for (i = 0; i < ac->num_msgs; i++) {
		ares = talloc_zero(ac, struct ldb_reply);
		if (!ares) {
			return LDB_ERR_OPERATIONS_ERROR;
		}

		ares->type = LDB_REPLY_ENTRY;
		ares->message = talloc_move(ares, &ac->msgs[i]);

		ret = ldb_module_send_entry(ac->req, ares->message, ares->controls);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}

	for (i = 0; i < ac->num_refs; i++) {
		ares = talloc_zero(ac, struct ldb_reply);
		if (!ares) {
			return LDB_ERR_OPERATIONS_ERROR;
		}

		ares->type = LDB_REPLY_REFERRAL;
		ares->referral = talloc_move(ares, &ac->referrals[i]);

		ret = ldb_module_send_referral(ac->req, ares->referral);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}

	return LDB_SUCCESS;
}

static int server_sort_search_callback(struct ldb_request *req, struct ldb_reply *ares)
{
	struct sort_context *ac;
	struct ldb_context *ldb;
	int ret;

	ac = talloc_get_type(req->context, struct sort_context);
	ldb = ldb_module_get_ctx(ac->module);

	if (!ares) {
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}
	if (ares->error != LDB_SUCCESS) {
		return ldb_module_done(ac->req, ares->controls,
					ares->response, ares->error);
	}

	switch (ares->type) {
	case LDB_REPLY_ENTRY:
		ac->msgs = talloc_realloc(ac, ac->msgs, struct ldb_message *, ac->num_msgs + 2);
		if (! ac->msgs) {
			talloc_free(ares);
			ldb_oom(ldb);
			return ldb_module_done(ac->req, NULL, NULL,
						LDB_ERR_OPERATIONS_ERROR);
		}

		ac->msgs[ac->num_msgs] = talloc_steal(ac->msgs, ares->message);
		ac->num_msgs++;
		ac->msgs[ac->num_msgs] = NULL;

		break;

	case LDB_REPLY_REFERRAL:
		ac->referrals = talloc_realloc(ac, ac->referrals, char *, ac->num_refs + 2);
		if (! ac->referrals) {
			talloc_free(ares);
			ldb_oom(ldb);
			return ldb_module_done(ac->req, NULL, NULL,
						LDB_ERR_OPERATIONS_ERROR);
		}

		ac->referrals[ac->num_refs] = talloc_steal(ac->referrals, ares->referral);
		ac->num_refs++;
		ac->referrals[ac->num_refs] = NULL;

		break;

	case LDB_REPLY_DONE:

		ret = server_sort_results(ac);
		return ldb_module_done(ac->req, ares->controls,
					ares->response, ret);
	}

	talloc_free(ares);
	return LDB_SUCCESS;
}

static int server_sort_search(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_control *control;
	struct ldb_server_sort_control **sort_ctrls;
	struct ldb_control **saved_controls;
	struct ldb_control **controls;
	struct ldb_request *down_req;
	struct sort_context *ac;
	struct ldb_context *ldb;
	int ret;

	ldb = ldb_module_get_ctx(module);

	/* check if there's a server sort control */
	control = ldb_request_get_control(req, LDB_CONTROL_SERVER_SORT_OID);
	if (control == NULL) {
		/* not found go on */
		return ldb_next_request(module, req);
	}

	ac = talloc_zero(req, struct sort_context);
	if (ac == NULL) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ac->module = module;
	ac->req = req;

	sort_ctrls = talloc_get_type(control->data, struct ldb_server_sort_control *);
	if (!sort_ctrls) {
		return LDB_ERR_PROTOCOL_ERROR;
	}

	/* FIXME: we do not support more than one attribute for sorting right now */
	/* FIXME: we need to check if the attribute type exist or return an error */
		
	if (sort_ctrls[1] != NULL) {
		if (control->critical) {

			/* callback immediately */
			ret = build_response(req, &controls,
					     LDB_ERR_UNWILLING_TO_PERFORM,
					     "sort control is not complete yet");
			if (ret != LDB_SUCCESS) {
				return ldb_module_done(req, NULL, NULL,
						    LDB_ERR_OPERATIONS_ERROR);
			}

			return ldb_module_done(req, controls, NULL, ret);
		} else {
			/* just pass the call down and don't do any sorting */
			return ldb_next_request(module, req);
		}
	}

	ac->attributeName = sort_ctrls[0]->attributeName;
	ac->orderingRule = sort_ctrls[0]->orderingRule;
	ac->reverse = sort_ctrls[0]->reverse;

	ret = ldb_build_search_req_ex(&down_req, ldb, ac,
					req->op.search.base,
					req->op.search.scope,
					req->op.search.tree,
					req->op.search.attrs,
					req->controls,
					ac,
					server_sort_search_callback,
					req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	/* save it locally and remove it from the list */
	/* we do not need to replace them later as we
	 * are keeping the original req intact */
	if (!ldb_save_controls(control, down_req, &saved_controls)) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	return ldb_next_request(module, down_req);
}

static int server_sort_init(struct ldb_module *module)
{
	struct ldb_context *ldb;
	int ret;

	ldb = ldb_module_get_ctx(module);

	ret = ldb_mod_register_control(module, LDB_CONTROL_SERVER_SORT_OID);
	if (ret != LDB_SUCCESS) {
		ldb_debug(ldb, LDB_DEBUG_WARNING,
			"server_sort:"
			"Unable to register control with rootdse!");
	}

	return ldb_next_init(module);
}

static const struct ldb_module_ops ldb_server_sort_module_ops = {
	.name		   = "server_sort",
	.search            = server_sort_search,
	.init_context	   = server_sort_init
};

int ldb_server_sort_init(const char *version)
{
	LDB_MODULE_CHECK_VERSION(version);
	return ldb_register_module(&ldb_server_sort_module_ops);
}
