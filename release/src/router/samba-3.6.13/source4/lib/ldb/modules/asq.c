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
 *  Component: ldb attribute scoped query control module
 *
 *  Description: this module searches all the objects pointed by
 *  		 the DNs contained in the references attribute
 *
 *  Author: Simo Sorce
 */

#include "replace.h"
#include "system/filesys.h"
#include "system/time.h"
#include "ldb_module.h"

struct asq_context {

	enum {ASQ_SEARCH_BASE, ASQ_SEARCH_MULTI} step;

	struct ldb_module *module;
	struct ldb_request *req;

	struct ldb_asq_control *asq_ctrl;

	const char * const *req_attrs;
	char *req_attribute;
	enum {
		ASQ_CTRL_SUCCESS			= 0,
		ASQ_CTRL_INVALID_ATTRIBUTE_SYNTAX	= 21,
		ASQ_CTRL_UNWILLING_TO_PERFORM		= 53,
		ASQ_CTRL_AFFECTS_MULTIPLE_DSA		= 71
	} asq_ret;

	struct ldb_reply *base_res;

	struct ldb_request **reqs;
	unsigned int num_reqs;
	unsigned int cur_req;

	struct ldb_control **controls;
};

static struct asq_context *asq_context_init(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb;
	struct asq_context *ac;

	ldb = ldb_module_get_ctx(module);

	ac = talloc_zero(req, struct asq_context);
	if (ac == NULL) {
		ldb_oom(ldb);
		return NULL;
	}

	ac->module = module;
	ac->req = req;

	return ac;
}

static int asq_search_continue(struct asq_context *ac);

static int asq_search_terminate(struct asq_context *ac)
{
	struct ldb_asq_control *asq;
	unsigned int i;

	if (ac->controls) {
		for (i = 0; ac->controls[i]; i++) /* count em */ ;
	} else {
		i = 0;
	}

	ac->controls = talloc_realloc(ac, ac->controls, struct ldb_control *, i + 2);

	if (ac->controls == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ac->controls[i] = talloc(ac->controls, struct ldb_control);
	if (ac->controls[i] == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ac->controls[i]->oid = LDB_CONTROL_ASQ_OID;
	ac->controls[i]->critical = 0;

	asq = talloc_zero(ac->controls[i], struct ldb_asq_control);
	if (asq == NULL)
		return LDB_ERR_OPERATIONS_ERROR;

	asq->result = ac->asq_ret;

	ac->controls[i]->data = asq;

	ac->controls[i + 1] = NULL;

	return ldb_module_done(ac->req, ac->controls, NULL, LDB_SUCCESS);
}

static int asq_base_callback(struct ldb_request *req, struct ldb_reply *ares)
{
	struct asq_context *ac;
	int ret;

	ac = talloc_get_type(req->context, struct asq_context);

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
		ac->base_res = talloc_move(ac, &ares);
		break;

	case LDB_REPLY_REFERRAL:
		/* ignore referrals */
		talloc_free(ares);
		break;

	case LDB_REPLY_DONE:

		talloc_free(ares);

		/* next step */
		ret = asq_search_continue(ac);
		if (ret != LDB_SUCCESS) {
			return ldb_module_done(ac->req, NULL, NULL, ret);
		}
		break;

	}
	return LDB_SUCCESS;
}

static int asq_reqs_callback(struct ldb_request *req, struct ldb_reply *ares)
{
	struct asq_context *ac;
	int ret;

	ac = talloc_get_type(req->context, struct asq_context);

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
		/* pass the message up to the original callback as we
		 * do not have to elaborate on it any further */
		ret = ldb_module_send_entry(ac->req, ares->message, ares->controls);
		if (ret != LDB_SUCCESS) {
			return ldb_module_done(ac->req, NULL, NULL, ret);
		}
		talloc_free(ares);
		break;

	case LDB_REPLY_REFERRAL:
		/* ignore referrals */
		talloc_free(ares);
		break;

	case LDB_REPLY_DONE:

		talloc_free(ares);

		ret = asq_search_continue(ac);
		if (ret != LDB_SUCCESS) {
			return ldb_module_done(ac->req, NULL, NULL, ret);
		}
		break;
	}

	return LDB_SUCCESS;
}

static int asq_build_first_request(struct asq_context *ac, struct ldb_request **base_req)
{
	struct ldb_context *ldb;
	const char **base_attrs;
	int ret;

	ldb = ldb_module_get_ctx(ac->module);

	ac->req_attrs = ac->req->op.search.attrs;
	ac->req_attribute = talloc_strdup(ac, ac->asq_ctrl->source_attribute);
	if (ac->req_attribute == NULL)
		return LDB_ERR_OPERATIONS_ERROR;

	base_attrs = talloc_array(ac, const char *, 2);
	if (base_attrs == NULL) return LDB_ERR_OPERATIONS_ERROR;

	base_attrs[0] = talloc_strdup(base_attrs, ac->asq_ctrl->source_attribute);
	if (base_attrs[0] == NULL) return LDB_ERR_OPERATIONS_ERROR;

	base_attrs[1] = NULL;

	ret = ldb_build_search_req(base_req, ldb, ac,
					ac->req->op.search.base,
					LDB_SCOPE_BASE,
					NULL,
					(const char * const *)base_attrs,
					NULL,
					ac, asq_base_callback,
					ac->req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	return LDB_SUCCESS;
}

static int asq_build_multiple_requests(struct asq_context *ac, bool *terminated)
{
	struct ldb_context *ldb;
	struct ldb_control **saved_controls;
	struct ldb_control *control;
	struct ldb_dn *dn;
	struct ldb_message_element *el;
	unsigned int i;
	int ret;

	if (ac->base_res == NULL) {
		return LDB_ERR_NO_SUCH_OBJECT;
	}

	ldb = ldb_module_get_ctx(ac->module);

	el = ldb_msg_find_element(ac->base_res->message, ac->req_attribute);
	/* no values found */
	if (el == NULL) {
		ac->asq_ret = ASQ_CTRL_SUCCESS;
		*terminated = true;
		return asq_search_terminate(ac);
	}

	ac->num_reqs = el->num_values;
	ac->cur_req = 0;
	ac->reqs = talloc_array(ac, struct ldb_request *, ac->num_reqs);
	if (ac->reqs == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	for (i = 0; i < el->num_values; i++) {

		dn = ldb_dn_new(ac, ldb,
				(const char *)el->values[i].data);
		if ( ! ldb_dn_validate(dn)) {
			ac->asq_ret = ASQ_CTRL_INVALID_ATTRIBUTE_SYNTAX;
			*terminated = true;
			return asq_search_terminate(ac);
		}

		ret = ldb_build_search_req_ex(&ac->reqs[i],
						ldb, ac,
						dn, LDB_SCOPE_BASE,
						ac->req->op.search.tree,
						ac->req_attrs,
						ac->req->controls,
						ac, asq_reqs_callback,
						ac->req);
		if (ret != LDB_SUCCESS) {
			return ret;
		}

		/* remove the ASQ control itself */
		control = ldb_request_get_control(ac->req, LDB_CONTROL_ASQ_OID);
		if (!ldb_save_controls(control, ac->reqs[i], &saved_controls)) {
			return LDB_ERR_OPERATIONS_ERROR;
		}
	}

	return LDB_SUCCESS;
}

static int asq_search_continue(struct asq_context *ac)
{
	struct ldb_context *ldb;
	bool terminated = false;
	int ret;

	ldb = ldb_module_get_ctx(ac->module);

	switch (ac->step) {
	case ASQ_SEARCH_BASE:

		/* build up the requests call chain */
		ret = asq_build_multiple_requests(ac, &terminated);
		if (ret != LDB_SUCCESS || terminated) {
			return ret;
		}

		ac->step = ASQ_SEARCH_MULTI;

		return ldb_request(ldb, ac->reqs[ac->cur_req]);

	case ASQ_SEARCH_MULTI:

		ac->cur_req++;

		if (ac->cur_req == ac->num_reqs) {
			/* done */
			return asq_search_terminate(ac);
		}

		return ldb_request(ldb, ac->reqs[ac->cur_req]);
	}

	return LDB_ERR_OPERATIONS_ERROR;
}

static int asq_search(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb;
	struct ldb_request *base_req;
	struct ldb_control *control;
	struct asq_context *ac;
	int ret;

	ldb = ldb_module_get_ctx(module);

	/* check if there's an ASQ control */
	control = ldb_request_get_control(req, LDB_CONTROL_ASQ_OID);
	if (control == NULL) {
		/* not found go on */
		return ldb_next_request(module, req);
	}

	ac = asq_context_init(module, req);
	if (!ac) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* check the search is well formed */
	if (req->op.search.scope != LDB_SCOPE_BASE) {
		ac->asq_ret = ASQ_CTRL_UNWILLING_TO_PERFORM;
		return asq_search_terminate(ac);
	}

	ac->asq_ctrl = talloc_get_type(control->data, struct ldb_asq_control);
	if (!ac->asq_ctrl) {
		return LDB_ERR_PROTOCOL_ERROR;
	}

	ret = asq_build_first_request(ac, &base_req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ac->step = ASQ_SEARCH_BASE;

	return ldb_request(ldb, base_req);
}

static int asq_init(struct ldb_module *module)
{
	struct ldb_context *ldb;
	int ret;

	ldb = ldb_module_get_ctx(module);

	ret = ldb_mod_register_control(module, LDB_CONTROL_ASQ_OID);
	if (ret != LDB_SUCCESS) {
		ldb_debug(ldb, LDB_DEBUG_WARNING, "asq: Unable to register control with rootdse!");
	}

	return ldb_next_init(module);
}

static const struct ldb_module_ops ldb_asq_module_ops = {
	.name		   = "asq",
	.search		   = asq_search,
	.init_context	   = asq_init
};

int ldb_asq_init(const char *version)
{
	LDB_MODULE_CHECK_VERSION(version);
	return ldb_register_module(&ldb_asq_module_ops);
}
