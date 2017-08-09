/* 
   ldb database library

   Copyright (C) Andrew Tridgell  2004
   Copyright (C) Simo Sorce       2006

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
 *  Name: ldb_ldap
 *
 *  Component: ldb ldap backend
 *
 *  Description: core files for LDAP backend
 *
 *  Author: Andrew Tridgell
 *
 *  Modifications:
 *
 *  - description: make the module use asynchronous calls
 *    date: Feb 2006
 *    author: Simo Sorce
 */

#include "replace.h"
#include "system/filesys.h"
#include "system/time.h"
#include "ldb_module.h"
#include "ldb_private.h"

#define LDAP_DEPRECATED 1
#include <ldap.h>

struct lldb_private {
	LDAP *ldap;
};

struct lldb_context {
	struct ldb_module *module;
	struct ldb_request *req;

	struct lldb_private *lldb;

	struct ldb_control **controls;
	int msgid;
};

static int lldb_ldap_to_ldb(int err) {
	/* Ldap errors and ldb errors are defined to the same values */
	return err;
}

/*
  convert a ldb_message structure to a list of LDAPMod structures
  ready for ldap_add() or ldap_modify()
*/
static LDAPMod **lldb_msg_to_mods(void *mem_ctx, const struct ldb_message *msg, int use_flags)
{
	LDAPMod **mods;
	unsigned int i, j;
	int num_mods = 0;

	/* allocate maximum number of elements needed */
	mods = talloc_array(mem_ctx, LDAPMod *, msg->num_elements+1);
	if (!mods) {
		errno = ENOMEM;
		return NULL;
	}
	mods[0] = NULL;

	for (i=0;i<msg->num_elements;i++) {
		const struct ldb_message_element *el = &msg->elements[i];

		mods[num_mods] = talloc(mods, LDAPMod);
		if (!mods[num_mods]) {
			goto failed;
		}
		mods[num_mods+1] = NULL;
		mods[num_mods]->mod_op = LDAP_MOD_BVALUES;
		if (use_flags) {
			switch (el->flags & LDB_FLAG_MOD_MASK) {
			case LDB_FLAG_MOD_ADD:
				mods[num_mods]->mod_op |= LDAP_MOD_ADD;
				break;
			case LDB_FLAG_MOD_DELETE:
				mods[num_mods]->mod_op |= LDAP_MOD_DELETE;
				break;
			case LDB_FLAG_MOD_REPLACE:
				mods[num_mods]->mod_op |= LDAP_MOD_REPLACE;
				break;
			}
		}
		mods[num_mods]->mod_type = discard_const_p(char, el->name);
		mods[num_mods]->mod_vals.modv_bvals = talloc_array(mods[num_mods], 
								   struct berval *,
								   1+el->num_values);
		if (!mods[num_mods]->mod_vals.modv_bvals) {
			goto failed;
		}

		for (j=0;j<el->num_values;j++) {
			mods[num_mods]->mod_vals.modv_bvals[j] = talloc(mods[num_mods]->mod_vals.modv_bvals,
									struct berval);
			if (!mods[num_mods]->mod_vals.modv_bvals[j]) {
				goto failed;
			}
			mods[num_mods]->mod_vals.modv_bvals[j]->bv_val = (char *)el->values[j].data;
			mods[num_mods]->mod_vals.modv_bvals[j]->bv_len = el->values[j].length;
		}
		mods[num_mods]->mod_vals.modv_bvals[j] = NULL;
		num_mods++;
	}

	return mods;

failed:
	talloc_free(mods);
	return NULL;
}

/*
  add a single set of ldap message values to a ldb_message
*/
static int lldb_add_msg_attr(struct ldb_context *ldb,
			     struct ldb_message *msg, 
			     const char *attr, struct berval **bval)
{
	int count, i;
	struct ldb_message_element *el;

	count = ldap_count_values_len(bval);

	if (count <= 0) {
		return -1;
	}

	el = talloc_realloc(msg, msg->elements, struct ldb_message_element, 
			      msg->num_elements + 1);
	if (!el) {
		errno = ENOMEM;
		return -1;
	}

	msg->elements = el;

	el = &msg->elements[msg->num_elements];

	el->name = talloc_strdup(msg->elements, attr);
	if (!el->name) {
		errno = ENOMEM;
		return -1;
	}
	el->flags = 0;

	el->num_values = 0;
	el->values = talloc_array(msg->elements, struct ldb_val, count);
	if (!el->values) {
		errno = ENOMEM;
		return -1;
	}

	for (i=0;i<count;i++) {
		/* we have to ensure this is null terminated so that
		   ldb_msg_find_attr_as_string() can work */
		el->values[i].data = talloc_size(el->values, bval[i]->bv_len+1);
		if (!el->values[i].data) {
			errno = ENOMEM;
			return -1;
		}
		memcpy(el->values[i].data, bval[i]->bv_val, bval[i]->bv_len);
		el->values[i].data[bval[i]->bv_len] = 0;
		el->values[i].length = bval[i]->bv_len;
		el->num_values++;
	}

	msg->num_elements++;

	return 0;
}

/*
  search for matching records
*/
static int lldb_search(struct lldb_context *lldb_ac)
{
	struct ldb_context *ldb;
	struct lldb_private *lldb = lldb_ac->lldb;
	struct ldb_module *module = lldb_ac->module;
	struct ldb_request *req = lldb_ac->req;
	struct timeval tv;
	int ldap_scope;
	char *search_base;
	char *expression;
	int ret;

	ldb = ldb_module_get_ctx(module);

	if (!req->callback || !req->context) {
		ldb_set_errstring(ldb, "Async interface called with NULL callback function or NULL context");
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if (req->op.search.tree == NULL) {
		ldb_set_errstring(ldb, "Invalid expression parse tree");
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if (req->controls != NULL) {
		ldb_debug(ldb, LDB_DEBUG_WARNING, "Controls are not yet supported by ldb_ldap backend!");
	}

	ldb_request_set_state(req, LDB_ASYNC_PENDING);

	search_base = ldb_dn_alloc_linearized(lldb_ac, req->op.search.base);
	if (req->op.search.base == NULL) {
		search_base = talloc_strdup(lldb_ac, "");
	}
	if (search_base == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	expression = ldb_filter_from_tree(lldb_ac, req->op.search.tree);
	if (expression == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	switch (req->op.search.scope) {
	case LDB_SCOPE_BASE:
		ldap_scope = LDAP_SCOPE_BASE;
		break;
	case LDB_SCOPE_ONELEVEL:
		ldap_scope = LDAP_SCOPE_ONELEVEL;
		break;
	default:
		ldap_scope = LDAP_SCOPE_SUBTREE;
		break;
	}

	tv.tv_sec = req->timeout;
	tv.tv_usec = 0;

	ret = ldap_search_ext(lldb->ldap, search_base, ldap_scope, 
			      expression, 
			      discard_const_p(char *, req->op.search.attrs),
			      0,
			      NULL,
			      NULL,
			      &tv,
			      LDAP_NO_LIMIT,
			      &lldb_ac->msgid);

	if (ret != LDAP_SUCCESS) {
		ldb_set_errstring(ldb, ldap_err2string(ret));
	}

	return lldb_ldap_to_ldb(ret);
}

/*
  add a record
*/
static int lldb_add(struct lldb_context *lldb_ac)
{
	struct ldb_context *ldb;
	struct lldb_private *lldb = lldb_ac->lldb;
	struct ldb_module *module = lldb_ac->module;
	struct ldb_request *req = lldb_ac->req;
	LDAPMod **mods;
	char *dn;
	int ret;

	ldb = ldb_module_get_ctx(module);

	ldb_request_set_state(req, LDB_ASYNC_PENDING);

	mods = lldb_msg_to_mods(lldb_ac, req->op.add.message, 0);
	if (mods == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	dn = ldb_dn_alloc_linearized(lldb_ac, req->op.add.message->dn);
	if (dn == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = ldap_add_ext(lldb->ldap, dn, mods,
			   NULL,
			   NULL,
			   &lldb_ac->msgid);

	if (ret != LDAP_SUCCESS) {
		ldb_set_errstring(ldb, ldap_err2string(ret));
	}

	return lldb_ldap_to_ldb(ret);
}

/*
  modify a record
*/
static int lldb_modify(struct lldb_context *lldb_ac)
{
	struct ldb_context *ldb;
	struct lldb_private *lldb = lldb_ac->lldb;
	struct ldb_module *module = lldb_ac->module;
	struct ldb_request *req = lldb_ac->req;
	LDAPMod **mods;
	char *dn;
	int ret;

	ldb = ldb_module_get_ctx(module);

	ldb_request_set_state(req, LDB_ASYNC_PENDING);

	mods = lldb_msg_to_mods(lldb_ac, req->op.mod.message, 1);
	if (mods == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	dn = ldb_dn_alloc_linearized(lldb_ac, req->op.mod.message->dn);
	if (dn == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = ldap_modify_ext(lldb->ldap, dn, mods,
			      NULL,
			      NULL,
			      &lldb_ac->msgid);

	if (ret != LDAP_SUCCESS) {
		ldb_set_errstring(ldb, ldap_err2string(ret));
	}

	return lldb_ldap_to_ldb(ret);
}

/*
  delete a record
*/
static int lldb_delete(struct lldb_context *lldb_ac)
{
	struct ldb_context *ldb;
	struct lldb_private *lldb = lldb_ac->lldb;
	struct ldb_module *module = lldb_ac->module;
	struct ldb_request *req = lldb_ac->req;
	char *dnstr;
	int ret;

	ldb = ldb_module_get_ctx(module);

	ldb_request_set_state(req, LDB_ASYNC_PENDING);

	dnstr = ldb_dn_alloc_linearized(lldb_ac, req->op.del.dn);

	ret = ldap_delete_ext(lldb->ldap, dnstr,
			      NULL,
			      NULL,
			      &lldb_ac->msgid);

	if (ret != LDAP_SUCCESS) {
		ldb_set_errstring(ldb, ldap_err2string(ret));
	}

	return lldb_ldap_to_ldb(ret);
}

/*
  rename a record
*/
static int lldb_rename(struct lldb_context *lldb_ac)
{
	struct ldb_context *ldb;
	struct lldb_private *lldb = lldb_ac->lldb;
	struct ldb_module *module = lldb_ac->module;
	struct ldb_request *req = lldb_ac->req;
	const char *rdn_name;
	const struct ldb_val *rdn_val;
	char *old_dn;
	char *newrdn;
	char *parentdn;
	int ret;

	ldb = ldb_module_get_ctx(module);

	ldb_request_set_state(req, LDB_ASYNC_PENDING);

	old_dn = ldb_dn_alloc_linearized(lldb_ac, req->op.rename.olddn);
	if (old_dn == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	rdn_name = ldb_dn_get_rdn_name(req->op.rename.newdn);
	rdn_val = ldb_dn_get_rdn_val(req->op.rename.newdn);

	if ((rdn_name != NULL) && (rdn_val != NULL)) {
		newrdn = talloc_asprintf(lldb_ac, "%s=%s", rdn_name,
					 rdn_val->length > 0 ? ldb_dn_escape_value(lldb, *rdn_val) : "");
	} else {
		newrdn = talloc_strdup(lldb_ac, "");
	}
	if (!newrdn) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	parentdn = ldb_dn_alloc_linearized(lldb_ac, ldb_dn_get_parent(lldb_ac, req->op.rename.newdn));
	if (!parentdn) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = ldap_rename(lldb->ldap, old_dn, newrdn, parentdn,
			  1, NULL, NULL,
			  &lldb_ac->msgid);

	if (ret != LDAP_SUCCESS) {
		ldb_set_errstring(ldb, ldap_err2string(ret));
	}

	return lldb_ldap_to_ldb(ret);
}

static int lldb_start_trans(struct ldb_module *module)
{
	/* TODO implement a local transaction mechanism here */

	return LDB_SUCCESS;
}

static int lldb_end_trans(struct ldb_module *module)
{
	/* TODO implement a local transaction mechanism here */

	return LDB_SUCCESS;
}

static int lldb_del_trans(struct ldb_module *module)
{
	/* TODO implement a local transaction mechanism here */

	return LDB_SUCCESS;
}

static void lldb_request_done(struct lldb_context *ac,
			struct ldb_control **ctrls, int error)
{
	struct ldb_request *req;
	struct ldb_reply *ares;

	req = ac->req;

	ares = talloc_zero(req, struct ldb_reply);
	if (!ares) {
		ldb_oom(ldb_module_get_ctx(ac->module));
		req->callback(req, NULL);
		return;
	}
	ares->type = LDB_REPLY_DONE;
	ares->controls = talloc_steal(ares, ctrls);
	ares->error = error;

	req->callback(req, ares);
}

/* return false if the request is still in progress
 * return true if the request is completed
 */
static bool lldb_parse_result(struct lldb_context *ac, LDAPMessage *result)
{
	struct ldb_context *ldb;
	struct lldb_private *lldb = ac->lldb;
	LDAPControl **serverctrlsp = NULL;
	char **referralsp = NULL;
	char *matcheddnp = NULL;
	char *errmsgp = NULL;
	LDAPMessage *msg;
	int type;
	struct ldb_message *ldbmsg;
	char *referral;
	bool callback_failed;
	bool request_done;
	bool lret;
	unsigned int i;
	int ret;

	ldb = ldb_module_get_ctx(ac->module);

	type = ldap_msgtype(result);
	callback_failed = false;
	request_done = false;

	switch (type) {
	case LDAP_RES_SEARCH_ENTRY:

		msg = ldap_first_entry(lldb->ldap, result);
	       	if (msg != NULL) {
			BerElement *berptr = NULL;
			char *attr, *dn;

			ldbmsg = ldb_msg_new(ac);
			if (!ldbmsg) {
				ldb_oom(ldb);
				ret = LDB_ERR_OPERATIONS_ERROR;
				break;
			}

			dn = ldap_get_dn(lldb->ldap, msg);
			if (!dn) {
				ldb_oom(ldb);
				talloc_free(ldbmsg);
				ret = LDB_ERR_OPERATIONS_ERROR;
				break;
			}
			ldbmsg->dn = ldb_dn_new(ldbmsg, ldb, dn);
			if ( ! ldb_dn_validate(ldbmsg->dn)) {
				ldb_asprintf_errstring(ldb, "Invalid DN '%s' in reply", dn);
				talloc_free(ldbmsg);
				ret = LDB_ERR_OPERATIONS_ERROR;
				ldap_memfree(dn);
				break;
			}
			ldap_memfree(dn);

			ldbmsg->num_elements = 0;
			ldbmsg->elements = NULL;

			/* loop over all attributes */
			for (attr=ldap_first_attribute(lldb->ldap, msg, &berptr);
			     attr;
			     attr=ldap_next_attribute(lldb->ldap, msg, berptr)) {
				struct berval **bval;
				bval = ldap_get_values_len(lldb->ldap, msg, attr);

				if (bval) {
					lldb_add_msg_attr(ldb, ldbmsg, attr, bval);
					ldap_value_free_len(bval);
				}
			}
			if (berptr) ber_free(berptr, 0);

			ret = ldb_module_send_entry(ac->req, ldbmsg, NULL /* controls not yet supported */);
			if (ret != LDB_SUCCESS) {
				ldb_asprintf_errstring(ldb, "entry send failed: %s",
						       ldb_errstring(ldb));
				callback_failed = true;
			}
		} else {
			ret = LDB_ERR_OPERATIONS_ERROR;
		}
		break;

	case LDAP_RES_SEARCH_REFERENCE:

		ret = ldap_parse_reference(lldb->ldap, result,
					   &referralsp, &serverctrlsp, 0);
		if (ret != LDAP_SUCCESS) {
			ldb_asprintf_errstring(ldb, "ldap reference parse error: %s : %s",
					       ldap_err2string(ret), errmsgp);
			ret = LDB_ERR_OPERATIONS_ERROR;
			break;
		}
		if (referralsp == NULL) {
			ldb_asprintf_errstring(ldb, "empty ldap referrals list");
			ret = LDB_ERR_PROTOCOL_ERROR;
			break;
		}

		for (i = 0; referralsp[i]; i++) {
			referral = talloc_strdup(ac, referralsp[i]);

			ret = ldb_module_send_referral(ac->req, referral);
			if (ret != LDB_SUCCESS) {
				ldb_asprintf_errstring(ldb, "referral send failed: %s",
						       ldb_errstring(ldb));
				callback_failed = true;
				break;
			}
		}
		break;

	case LDAP_RES_SEARCH_RESULT:
	case LDAP_RES_MODIFY:
	case LDAP_RES_ADD:
	case LDAP_RES_DELETE:
	case LDAP_RES_MODDN:

		if (ldap_parse_result(lldb->ldap, result, &ret,
					&matcheddnp, &errmsgp,
					&referralsp, &serverctrlsp, 0) != LDAP_SUCCESS) {
			ret = LDB_ERR_OPERATIONS_ERROR;
		}
		if (ret != LDB_SUCCESS) {
			ldb_asprintf_errstring(ldb, "ldap parse error for type %d: %s : %s",
					       type, ldap_err2string(ret), errmsgp);
			break;
		}

		if (serverctrlsp != NULL) {
			/* FIXME: transform the LDAPControl list into an ldb_control one */
			ac->controls = NULL;
		}

		request_done = true;
		break;

	default:
		ldb_asprintf_errstring(ldb, "unknown ldap return type: %d", type);
		ret = LDB_ERR_PROTOCOL_ERROR;
		break;
	}

	if (ret != LDB_SUCCESS) {

		/* if the callback failed the caller will have freed the
		 * request. Just return and don't try to use it */
		if (callback_failed) {

			/* tell lldb_wait to remove the request from the
			 *  queue */
			lret = true;
			goto free_and_return;
		}

		request_done = true;
	}

	if (request_done) {
		lldb_request_done(ac, ac->controls, ret);
		lret = true;
		goto free_and_return;
	}

	lret = false;

free_and_return:

	if (matcheddnp) ldap_memfree(matcheddnp);
	if (errmsgp && *errmsgp) {
		ldb_set_errstring(ldb, errmsgp);
	}
	if (errmsgp) {
		ldap_memfree(errmsgp);
	}
	if (referralsp) ldap_value_free(referralsp);
	if (serverctrlsp) ldap_controls_free(serverctrlsp);

	ldap_msgfree(result);

	return lret;
}

static void lldb_timeout(struct tevent_context *ev,
			 struct tevent_timer *te,
			 struct timeval t,
			 void *private_data)
{
	struct lldb_context *ac;
	ac = talloc_get_type(private_data, struct lldb_context);

	lldb_request_done(ac, NULL, LDB_ERR_TIME_LIMIT_EXCEEDED);
}

static void lldb_callback(struct tevent_context *ev,
			  struct tevent_timer *te,
			  struct timeval t,
			  void *private_data)
{
	struct lldb_context *ac;
	struct tevent_timer *lte;
	struct timeval tv;
	LDAPMessage *result;
	int lret;

	ac = talloc_get_type(private_data, struct lldb_context);

	if (!ac->msgid) {
		lldb_request_done(ac, NULL, LDB_ERR_OPERATIONS_ERROR);
		return;
	}

	tv.tv_sec = 0;
	tv.tv_usec = 0;
	lret = ldap_result(ac->lldb->ldap, ac->msgid, 0, &tv, &result);
	if (lret == 0) {
		goto respin;
	}
	if (lret == -1) {
		lldb_request_done(ac, NULL, LDB_ERR_OPERATIONS_ERROR);
		return;
	}

	if ( ! lldb_parse_result(ac, result)) {
		goto respin;
	}

	return;

respin:
	tv.tv_sec = 0;
	tv.tv_usec = 100;
	lte = tevent_add_timer(ev, ac, tv, lldb_callback, ac);
	if (NULL == lte) {
		lldb_request_done(ac, NULL, LDB_ERR_OPERATIONS_ERROR);
	}
}

static bool lldb_dn_is_special(struct ldb_request *req)
{
	struct ldb_dn *dn = NULL;

	switch (req->operation) {
	case LDB_ADD:
		dn = req->op.add.message->dn;
		break;
	case LDB_MODIFY:
		dn = req->op.mod.message->dn;
		break;
	case LDB_DELETE:
		dn = req->op.del.dn;
		break;
	case LDB_RENAME:
		dn = req->op.rename.olddn;
		break;
	default:
		break;
	}

	if (dn && ldb_dn_is_special(dn)) {
		return true;
	}
	return false;
}

static void lldb_auto_done_callback(struct tevent_context *ev,
				    struct tevent_timer *te,
				    struct timeval t,
				    void *private_data)
{
	struct lldb_context *ac;

	ac = talloc_get_type(private_data, struct lldb_context);
	lldb_request_done(ac, NULL, LDB_SUCCESS);
}

static int lldb_handle_request(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb;
	struct lldb_private *lldb;
	struct lldb_context *ac;
	struct tevent_context *ev;
	struct tevent_timer *te;
	struct timeval tv;
	int ret;

	lldb = talloc_get_type(ldb_module_get_private(module), struct lldb_private);
	ldb = ldb_module_get_ctx(module);

	if (req->starttime == 0 || req->timeout == 0) {
		ldb_set_errstring(ldb, "Invalid timeout settings");
		return LDB_ERR_TIME_LIMIT_EXCEEDED;
	}

	ev = ldb_get_event_context(ldb);
	if (NULL == ev) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ac = talloc_zero(ldb, struct lldb_context);
	if (ac == NULL) {
		ldb_set_errstring(ldb, "Out of Memory");
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ac->module = module;
	ac->req = req;
	ac->lldb = lldb;
	ac->msgid = 0;

	if (lldb_dn_is_special(req)) {
		tv.tv_sec = 0;
		tv.tv_usec = 0;
		te = tevent_add_timer(ev, ac, tv,
				     lldb_auto_done_callback, ac);
		if (NULL == te) {
			return LDB_ERR_OPERATIONS_ERROR;
		}

		return LDB_SUCCESS;
	}

	switch (ac->req->operation) {
	case LDB_SEARCH:
		ret = lldb_search(ac);
		break;
	case LDB_ADD:
		ret = lldb_add(ac);
		break;
	case LDB_MODIFY:
		ret = lldb_modify(ac);
		break;
	case LDB_DELETE:
		ret = lldb_delete(ac);
		break;
	case LDB_RENAME:
		ret = lldb_rename(ac);
		break;
	default:
		/* no other op supported */
		ret = LDB_ERR_PROTOCOL_ERROR;
		break;
	}

	if (ret != LDB_SUCCESS) {
		lldb_request_done(ac, NULL, ret);
		return ret;
	}

	tv.tv_sec = 0;
	tv.tv_usec = 0;
	te = tevent_add_timer(ev, ac, tv, lldb_callback, ac);
	if (NULL == te) {
		return LDB_ERR_OPERATIONS_ERROR;
	}


	tv.tv_sec = req->starttime + req->timeout;
	tv.tv_usec = 0;
	te = tevent_add_timer(ev, ac, tv, lldb_timeout, ac);
	if (NULL == te) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	return LDB_SUCCESS;
}

static const struct ldb_module_ops lldb_ops = {
	.name              = "ldap",
	.search            = lldb_handle_request,
	.add               = lldb_handle_request,
	.modify            = lldb_handle_request,
	.del               = lldb_handle_request,
	.rename            = lldb_handle_request,
	.request           = lldb_handle_request,
	.start_transaction = lldb_start_trans,
	.end_transaction   = lldb_end_trans,
	.del_transaction   = lldb_del_trans,
};


static int lldb_destructor(struct lldb_private *lldb)
{
	ldap_unbind(lldb->ldap);
	return 0;
}


/*
  optionally perform a bind
 */
static int lldb_bind(struct ldb_module *module,
		     const char *options[])
{
	const char *bind_mechanism;
	struct lldb_private *lldb;
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	int ret;

	bind_mechanism = ldb_options_find(ldb, options, "bindMech");
	if (bind_mechanism == NULL) {
		/* no bind wanted */
		return LDB_SUCCESS;
	}

	lldb = talloc_get_type(ldb_module_get_private(module), struct lldb_private);

	if (strcmp(bind_mechanism, "simple") == 0) {
		const char *bind_id, *bind_secret;

		bind_id = ldb_options_find(ldb, options, "bindID");
		bind_secret = ldb_options_find(ldb, options, "bindSecret");
		if (bind_id == NULL || bind_secret == NULL) {
			ldb_asprintf_errstring(ldb, "simple bind requires bindID and bindSecret");
			return LDB_ERR_OPERATIONS_ERROR;
		}

		ret = ldap_simple_bind_s(lldb->ldap, bind_id, bind_secret);
		if (ret != LDAP_SUCCESS) {
			ldb_asprintf_errstring(ldb, "bind failed: %s", ldap_err2string(ret));
			return ret;
		}
		return LDB_SUCCESS;
	}

	ldb_asprintf_errstring(ldb, "bind failed: unknown mechanism %s", bind_mechanism);
	return LDB_ERR_INAPPROPRIATE_AUTHENTICATION;
}

/*
  connect to the database
*/
static int lldb_connect(struct ldb_context *ldb,
			const char *url,
			unsigned int flags,
			const char *options[],
			struct ldb_module **_module)
{
	struct ldb_module *module;
	struct lldb_private *lldb;
	int version = 3;
	int ret;

	module = ldb_module_new(ldb, ldb, "ldb_ldap backend", &lldb_ops);
	if (!module) return LDB_ERR_OPERATIONS_ERROR;

	lldb = talloc_zero(module, struct lldb_private);
	if (!lldb) {
		ldb_oom(ldb);
		goto failed;
	}
	ldb_module_set_private(module, lldb);

	ret = ldap_initialize(&lldb->ldap, url);
	if (ret != LDAP_SUCCESS) {
		ldb_debug(ldb, LDB_DEBUG_FATAL, "ldap_initialize failed for URL '%s' - %s",
			  url, ldap_err2string(ret));
		goto failed;
	}

	talloc_set_destructor(lldb, lldb_destructor);

	ret = ldap_set_option(lldb->ldap, LDAP_OPT_PROTOCOL_VERSION, &version);
	if (ret != LDAP_SUCCESS) {
		ldb_debug(ldb, LDB_DEBUG_FATAL, "ldap_set_option failed - %s",
			  ldap_err2string(ret));
		goto failed;
	}

	*_module = module;

	ret = lldb_bind(module, options);
	if (ret != LDB_SUCCESS) {
		goto failed;
	}


	return LDB_SUCCESS;

failed:
	talloc_free(module);
	return LDB_ERR_OPERATIONS_ERROR;
}

/*
  initialise the module
 */
int ldb_ldap_init(const char *version)
{
	int ret, i;
	const char *names[] = { "ldap", "ldaps", "ldapi", NULL };
	LDB_MODULE_CHECK_VERSION(version);
	for (i=0; names[i]; i++) {
		ret = ldb_register_backend(names[i], lldb_connect, false);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}
	return LDB_SUCCESS;
}
