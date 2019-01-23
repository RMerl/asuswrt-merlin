/* 
   ldb database library

   Copyright (C) Andrew Bartlett 2007

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
 *  Component: ldb ranged results module
 *
 *  Description: munge AD-style 'ranged results' requests into
 *  requests for all values in an attribute, then return the range to
 *  the client.
 *
 *  Author: Andrew Bartlett
 */

#include "includes.h"
#include "ldb_module.h"

struct rr_context {
	struct ldb_module *module;
	struct ldb_request *req;
};

static struct rr_context *rr_init_context(struct ldb_module *module,
					  struct ldb_request *req)
{
	struct rr_context *ac;

	ac = talloc_zero(req, struct rr_context);
	if (ac == NULL) {
		ldb_set_errstring(ldb_module_get_ctx(module), "Out of Memory");
		return NULL;
	}

	ac->module = module;
	ac->req = req;

	return ac;
}

static int rr_search_callback(struct ldb_request *req, struct ldb_reply *ares)
{
	struct ldb_context *ldb;
	struct rr_context *ac;
	unsigned int i, j;
	TALLOC_CTX *temp_ctx;

	ac = talloc_get_type(req->context, struct rr_context);
	ldb = ldb_module_get_ctx(ac->module);

	if (!ares) {
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}
	if (ares->error != LDB_SUCCESS) {
		return ldb_module_done(ac->req, ares->controls,
					ares->response, ares->error);
	}

	if (ares->type == LDB_REPLY_REFERRAL) {
		return ldb_module_send_referral(ac->req, ares->referral);
	}

	if (ares->type == LDB_REPLY_DONE) {
		return ldb_module_done(ac->req, ares->controls,
					ares->response, ares->error);
	}

	/* LDB_REPLY_ENTRY */

	temp_ctx = talloc_new(ac->req);
	if (!temp_ctx) {
		ldb_module_oom(ac->module);
		return ldb_module_done(ac->req, NULL, NULL,
				       LDB_ERR_OPERATIONS_ERROR);
	}

	/* Find those that are range requests from the attribute list */
	for (i = 0; ac->req->op.search.attrs[i]; i++) {
		char *p, *new_attr;
		const char *end_str;
		unsigned int start, end, orig_num_values;
		struct ldb_message_element *el;
		struct ldb_val *orig_values;

		p = strchr(ac->req->op.search.attrs[i], ';');
		if (!p) {
			continue;
		}
		if (strncasecmp(p, ";range=", strlen(";range=")) != 0) {
			continue;
		}
		if (sscanf(p, ";range=%u-%u", &start, &end) != 2) {
			if (sscanf(p, ";range=%u-*", &start) == 1) {
				end = (unsigned int)-1;
			} else {
				continue;
			}
		}
		new_attr = talloc_strndup(temp_ctx,
					  ac->req->op.search.attrs[i],
					  (size_t)(p - ac->req->op.search.attrs[i]));

		if (!new_attr) {
			ldb_oom(ldb);
			return ldb_module_done(ac->req, NULL, NULL,
						LDB_ERR_OPERATIONS_ERROR);
		}
		el = ldb_msg_find_element(ares->message, new_attr);
		talloc_free(new_attr);
		if (!el) {
			continue;
		}
		if (end >= (el->num_values - 1)) {
			/* Need to leave the requested attribute in
			 * there (so add an empty one to match) */
			end_str = "*";
			end = el->num_values - 1;
		} else {
			end_str = talloc_asprintf(temp_ctx, "%u", end);
			if (!end_str) {
				ldb_oom(ldb);
				return ldb_module_done(ac->req, NULL, NULL,
							LDB_ERR_OPERATIONS_ERROR);
			}
		}
		/* If start is greater then where we are find the end to be */
		if (start > end) {
			el->num_values = 0;
			el->values = NULL;
		} else {
			orig_values = el->values;
			orig_num_values = el->num_values;
			
			if ((start + end < start) || (start + end < end)) {
				ldb_asprintf_errstring(ldb,
					"range request error: start or end would overflow!");
				return ldb_module_done(ac->req, NULL, NULL,
							LDB_ERR_UNWILLING_TO_PERFORM);
			}
			
			el->num_values = 0;
			
			el->values = talloc_array(ares->message->elements,
						  struct ldb_val,
						  (end - start) + 1);
			if (!el->values) {
				ldb_oom(ldb);
				return ldb_module_done(ac->req, NULL, NULL,
							LDB_ERR_OPERATIONS_ERROR);
			}
			for (j=start; j <= end; j++) {
				el->values[el->num_values] = orig_values[j];
				el->num_values++;
			}
		}
		el->name = talloc_asprintf(ares->message->elements,
					   "%s;range=%u-%s", el->name, start,
					   end_str);
		if (!el->name) {
			ldb_oom(ldb);
			return ldb_module_done(ac->req, NULL, NULL,
						LDB_ERR_OPERATIONS_ERROR);
		}
	}

	talloc_free(temp_ctx);

	return ldb_module_send_entry(ac->req, ares->message, ares->controls);
}

/* search */
static int rr_search(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb;
	unsigned int i;
	unsigned int start, end;
	const char **new_attrs = NULL;
	bool found_rr = false;
	struct ldb_request *down_req;
	struct rr_context *ac;
	int ret;

	ldb = ldb_module_get_ctx(module);

	/* Strip the range request from the attribute */
	for (i = 0; req->op.search.attrs && req->op.search.attrs[i]; i++) {
		char *p;
		new_attrs = talloc_realloc(req, new_attrs, const char *, i+2);
		new_attrs[i] = req->op.search.attrs[i];
		new_attrs[i+1] = NULL;
		p = strchr(new_attrs[i], ';');
		if (!p) {
			continue;
		}
		if (strncasecmp(p, ";range=", strlen(";range=")) != 0) {
			continue;
		}
		end = (unsigned int)-1;
		if (sscanf(p, ";range=%u-*", &start) != 1) {
			if (sscanf(p, ";range=%u-%u", &start, &end) != 2) {
				ldb_asprintf_errstring(ldb,
					"range request error: "
					"range request malformed");
				return LDB_ERR_UNWILLING_TO_PERFORM;
			}
		}
		if (start > end) {
			ldb_asprintf_errstring(ldb, "range request error: start must not be greater than end");
			return LDB_ERR_UNWILLING_TO_PERFORM;
		}

		found_rr = true;
		new_attrs[i] = talloc_strndup(new_attrs, new_attrs[i],
					      (size_t)(p - new_attrs[i]));

		if (!new_attrs[i]) {
			return ldb_oom(ldb);
		}
	}

	if (found_rr) {
		ac = rr_init_context(module, req);
		if (!ac) {
			return ldb_operr(ldb);
		}

		ret = ldb_build_search_req_ex(&down_req, ldb, ac,
					      req->op.search.base,
					      req->op.search.scope,
					      req->op.search.tree,
					      new_attrs,
					      req->controls,
					      ac, rr_search_callback,
					      req);
		LDB_REQ_SET_LOCATION(down_req);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
		return ldb_next_request(module, down_req);
	}

	/* No change, just run the original request as if we were never here */
	talloc_free(new_attrs);
	return ldb_next_request(module, req);
}

static const struct ldb_module_ops ldb_ranged_results_module_ops = {
	.name		   = "ranged_results",
	.search            = rr_search,
};

int ldb_ranged_results_module_init(const char *version)
{
	LDB_MODULE_CHECK_VERSION(version);
	return ldb_register_module(&ldb_ranged_results_module_ops);
}
