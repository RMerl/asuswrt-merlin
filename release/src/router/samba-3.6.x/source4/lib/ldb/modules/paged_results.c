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
 *  Name: paged_result
 *
 *  Component: ldb paged results control module
 *
 *  Description: this module caches a complete search and sends back
 *  		 results in chunks as asked by the client
 *
 *  Author: Simo Sorce
 */

#include "replace.h"
#include "system/filesys.h"
#include "system/time.h"
#include "ldb_module.h"

struct message_store {
	/* keep the whole ldb_reply as an optimization
	 * instead of freeing and talloc-ing the container
	 * on each result */
	struct ldb_reply *r;
	struct message_store *next;
};

struct private_data;

struct results_store {

	struct private_data *priv;

	char *cookie;
	time_t timestamp;

	struct results_store *next;

	struct message_store *first;
	struct message_store *last;
	int num_entries;

	struct message_store *first_ref;
	struct message_store *last_ref;

	struct ldb_control **controls;
};

struct private_data {
	unsigned int next_free_id;
	struct results_store *store;
	
};

static int store_destructor(struct results_store *del)
{
	struct private_data *priv = del->priv;
	struct results_store *loop;

	if (priv->store == del) {
		priv->store = del->next;
		return 0;
	}

	for (loop = priv->store; loop; loop = loop->next) {
		if (loop->next == del) {
			loop->next = del->next;
			return 0;
		}
	}

	/* is not in list ? */
	return -1;
}

static struct results_store *new_store(struct private_data *priv)
{
	struct results_store *newr;
	unsigned int new_id = priv->next_free_id++;

	/* TODO: we should have a limit on the number of
	 * outstanding paged searches
	 */

	newr = talloc(priv, struct results_store);
	if (!newr) return NULL;

	newr->priv = priv;

	newr->cookie = talloc_asprintf(newr, "%d", new_id);
	if (!newr->cookie) {
		talloc_free(newr);
		return NULL;
	}

	newr->timestamp = time(NULL);

	newr->first = NULL;
	newr->num_entries = 0;
	newr->first_ref = NULL;
	newr->controls = NULL;

	newr->next = priv->store;
	priv->store = newr;

	talloc_set_destructor(newr, store_destructor);

	return newr;
}

struct paged_context {
	struct ldb_module *module;
	struct ldb_request *req;

	struct results_store *store;
	int size;
	struct ldb_control **controls;
};

static int paged_results(struct paged_context *ac)
{
	struct ldb_paged_control *paged;
	struct message_store *msg;
	unsigned int i, num_ctrls;
	int ret;

	if (ac->store == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	while (ac->store->num_entries > 0 && ac->size > 0) {
		msg = ac->store->first;
		ret = ldb_module_send_entry(ac->req, msg->r->message, msg->r->controls);
		if (ret != LDB_SUCCESS) {
			return ret;
		}

		ac->store->first = msg->next;
		talloc_free(msg);
		ac->store->num_entries--;
		ac->size--;
	}

	while (ac->store->first_ref != NULL) {
		msg = ac->store->first_ref;
		ret = ldb_module_send_referral(ac->req, msg->r->referral);
		if (ret != LDB_SUCCESS) {
			return ret;
		}

		ac->store->first_ref = msg->next;
		talloc_free(msg);
	}

	/* return result done */
	num_ctrls = 1;
	i = 0;

	if (ac->store->controls != NULL) {
		while (ac->store->controls[i]) i++; /* counting */

		num_ctrls += i;
	}

	ac->controls = talloc_array(ac, struct ldb_control *, num_ctrls +1);
	if (ac->controls == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}
	ac->controls[num_ctrls] = NULL;

	for (i = 0; i < (num_ctrls -1); i++) {
		ac->controls[i] = talloc_reference(ac->controls, ac->store->controls[i]);
	}

	ac->controls[i] = talloc(ac->controls, struct ldb_control);
	if (ac->controls[i] == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ac->controls[i]->oid = talloc_strdup(ac->controls[i],
						LDB_CONTROL_PAGED_RESULTS_OID);
	if (ac->controls[i]->oid == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ac->controls[i]->critical = 0;

	paged = talloc(ac->controls[i], struct ldb_paged_control);
	if (paged == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ac->controls[i]->data = paged;

	if (ac->size > 0) {
		paged->size = 0;
		paged->cookie = NULL;
		paged->cookie_len = 0;
	} else {
		paged->size = ac->store->num_entries;
		paged->cookie = talloc_strdup(paged, ac->store->cookie);
		paged->cookie_len = strlen(paged->cookie) + 1;
	}

	return LDB_SUCCESS;
}

static int paged_search_callback(struct ldb_request *req, struct ldb_reply *ares)
{
	struct paged_context *ac ;
	struct message_store *msg_store;
	int ret;

	ac = talloc_get_type(req->context, struct paged_context);

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
		msg_store = talloc(ac->store, struct message_store);
		if (msg_store == NULL) {
			return ldb_module_done(ac->req, NULL, NULL,
						LDB_ERR_OPERATIONS_ERROR);
		}
		msg_store->next = NULL;
		msg_store->r = talloc_steal(msg_store, ares);

		if (ac->store->first == NULL) {
			ac->store->first = msg_store;
		} else {
			ac->store->last->next = msg_store;
		}
		ac->store->last = msg_store;

		ac->store->num_entries++;

		break;

	case LDB_REPLY_REFERRAL:
		msg_store = talloc(ac->store, struct message_store);
		if (msg_store == NULL) {
			return ldb_module_done(ac->req, NULL, NULL,
						LDB_ERR_OPERATIONS_ERROR);
		}
		msg_store->next = NULL;
		msg_store->r = talloc_steal(msg_store, ares);

		if (ac->store->first_ref == NULL) {
			ac->store->first_ref = msg_store;
		} else {
			ac->store->last_ref->next = msg_store;
		}
		ac->store->last_ref = msg_store;

		break;

	case LDB_REPLY_DONE:
		ac->store->controls = talloc_move(ac->store, &ares->controls);
		ret = paged_results(ac);
		return ldb_module_done(ac->req, ac->controls,
					ares->response, ret);
	}

	return LDB_SUCCESS;
}

static int paged_search(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb;
	struct ldb_control *control;
	struct private_data *private_data;
	struct ldb_paged_control *paged_ctrl;
	struct ldb_control **saved_controls;
	struct ldb_request *search_req;
	struct paged_context *ac;
	int ret;

	ldb = ldb_module_get_ctx(module);

	/* check if there's a paged request control */
	control = ldb_request_get_control(req, LDB_CONTROL_PAGED_RESULTS_OID);
	if (control == NULL) {
		/* not found go on */
		return ldb_next_request(module, req);
	}

	paged_ctrl = talloc_get_type(control->data, struct ldb_paged_control);
	if (!paged_ctrl) {
		return LDB_ERR_PROTOCOL_ERROR;
	}

	private_data = talloc_get_type(ldb_module_get_private(module),
					struct private_data);

	ac = talloc_zero(req, struct paged_context);
	if (ac == NULL) {
		ldb_set_errstring(ldb, "Out of Memory");
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ac->module = module;
	ac->req = req;
	ac->size = paged_ctrl->size;
	if (ac->size < 0) {
		/* apparently some clients send more than 2^31. This
		   violates the ldap standard, but we need to cope */
		ac->size = 0x7FFFFFFF;
	}

	/* check if it is a continuation search the store */
	if (paged_ctrl->cookie_len == 0) {
		if (paged_ctrl->size == 0) {
			return LDB_ERR_OPERATIONS_ERROR;
		}

		ac->store = new_store(private_data);
		if (ac->store == NULL) {
			return LDB_ERR_OPERATIONS_ERROR;
		}

		ret = ldb_build_search_req_ex(&search_req, ldb, ac,
						req->op.search.base,
						req->op.search.scope,
						req->op.search.tree,
						req->op.search.attrs,
						req->controls,
						ac,
						paged_search_callback,
						req);
		if (ret != LDB_SUCCESS) {
			return ret;
		}

		/* save it locally and remove it from the list */
		/* we do not need to replace them later as we
		 * are keeping the original req intact */
		if (!ldb_save_controls(control, search_req, &saved_controls)) {
			return LDB_ERR_OPERATIONS_ERROR;
		}

		return ldb_next_request(module, search_req);

	} else {
		struct results_store *current = NULL;

		/* TODO: age out old outstanding requests */
		for (current = private_data->store; current; current = current->next) {
			if (strcmp(current->cookie, paged_ctrl->cookie) == 0) {
				current->timestamp = time(NULL);
				break;
			}
		}
		if (current == NULL) {
			return LDB_ERR_UNWILLING_TO_PERFORM;
		}

		ac->store = current;

		/* check if it is an abandon */
		if (ac->size == 0) {
			return ldb_module_done(req, NULL, NULL,
								LDB_SUCCESS);
		}

		ret = paged_results(ac);
		if (ret != LDB_SUCCESS) {
			return ldb_module_done(req, NULL, NULL, ret);
		}
		return ldb_module_done(req, ac->controls, NULL,
								LDB_SUCCESS);
	}
}

static int paged_request_init(struct ldb_module *module)
{
	struct ldb_context *ldb;
	struct private_data *data;
	int ret;

	ldb = ldb_module_get_ctx(module);

	data = talloc(module, struct private_data);
	if (data == NULL) {
		return LDB_ERR_OTHER;
	}

	data->next_free_id = 1;
	data->store = NULL;
	ldb_module_set_private(module, data);

	ret = ldb_mod_register_control(module, LDB_CONTROL_PAGED_RESULTS_OID);
	if (ret != LDB_SUCCESS) {
		ldb_debug(ldb, LDB_DEBUG_WARNING,
			"paged_results:"
			"Unable to register control with rootdse!");
	}

	return ldb_next_init(module);
}

static const struct ldb_module_ops ldb_paged_results_module_ops = {
	.name           = "paged_results",
	.search         = paged_search,
	.init_context 	= paged_request_init
};

int ldb_paged_results_init(const char *version)
{
	LDB_MODULE_CHECK_VERSION(version);
	return ldb_register_module(&ldb_paged_results_module_ops);
}
