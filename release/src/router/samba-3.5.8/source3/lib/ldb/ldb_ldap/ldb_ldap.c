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
 *  - description: make the module use asyncronous calls
 *    date: Feb 2006
 *    author: Simo Sorce
 */

#include "includes.h"
#include "ldb/include/includes.h"

#define LDAP_DEPRECATED 1
#include <ldap.h>

struct lldb_private {
	LDAP *ldap;
};

struct lldb_context {
	struct ldb_module *module;
	int msgid;
	int timeout;
	time_t starttime;
	void *context;
	int (*callback)(struct ldb_context *, void *, struct ldb_reply *);
};

static int lldb_ldap_to_ldb(int err) {
	/* Ldap errors and ldb errors are defined to the same values */
	return err;
}

static struct ldb_handle *init_handle(struct lldb_private *lldb, struct ldb_module *module,
					    void *context,
					    int (*callback)(struct ldb_context *, void *, struct ldb_reply *),
					    int timeout, time_t starttime)
{
	struct lldb_context *ac;
	struct ldb_handle *h;

	h = talloc_zero(lldb, struct ldb_handle);
	if (h == NULL) {
		ldb_set_errstring(module->ldb, "Out of Memory");
		return NULL;
	}

	h->module = module;

	ac = talloc(h, struct lldb_context);
	if (ac == NULL) {
		ldb_set_errstring(module->ldb, "Out of Memory");
		talloc_free(h);
		return NULL;
	}

	h->private_data = (void *)ac;

	h->state = LDB_ASYNC_INIT;
	h->status = LDB_SUCCESS;

	ac->module = module;
	ac->context = context;
	ac->callback = callback;
	ac->timeout = timeout;
	ac->starttime = starttime;
	ac->msgid = 0;

	return h;
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
			mods[num_mods]->mod_vals.modv_bvals[j]->bv_val =
				(char *)el->values[j].data;
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
		el->values[i].data =
			(uint8_t *)talloc_size(el->values, bval[i]->bv_len+1);
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
static int lldb_search(struct ldb_module *module, struct ldb_request *req)
{
	struct lldb_private *lldb = talloc_get_type(module->private_data, struct lldb_private);
	struct lldb_context *lldb_ac;
	struct timeval tv;
	int ldap_scope;
	char *search_base;
	char *expression;
	int ret;

	if (!req->callback || !req->context) {
		ldb_set_errstring(module->ldb, "Async interface called with NULL callback function or NULL context");
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if (req->op.search.tree == NULL) {
		ldb_set_errstring(module->ldb, "Invalid expression parse tree");
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if (req->controls != NULL) {
		ldb_debug(module->ldb, LDB_DEBUG_WARNING, "Controls are not yet supported by ldb_ldap backend!\n");
	}

	req->handle = init_handle(lldb, module, req->context, req->callback, req->timeout, req->starttime);
	if (req->handle == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	lldb_ac = talloc_get_type(req->handle->private_data, struct lldb_context);

	search_base = ldb_dn_linearize(lldb_ac, req->op.search.base);
	if (req->op.search.base == NULL) {
		search_base = talloc_strdup(lldb_ac, "");
	}
	if (search_base == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	expression = ldb_filter_from_tree(
		lldb_ac,
		discard_const_p(struct ldb_parse_tree, req->op.search.tree));
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
		ldb_set_errstring(module->ldb, ldap_err2string(ret));
	}

	return lldb_ldap_to_ldb(ret);
}

/*
  add a record
*/
static int lldb_add(struct ldb_module *module, struct ldb_request *req)
{
	struct lldb_private *lldb = talloc_get_type(module->private_data, struct lldb_private);
	struct lldb_context *lldb_ac;
	LDAPMod **mods;
	char *dn;
	int ret;

	/* ltdb specials should not reach this point */
	if (ldb_dn_is_special(req->op.add.message->dn)) {
		return LDB_ERR_INVALID_DN_SYNTAX;
	}

	req->handle = init_handle(lldb, module, req->context, req->callback, req->timeout, req->starttime);
	if (req->handle == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	lldb_ac = talloc_get_type(req->handle->private_data, struct lldb_context);

	mods = lldb_msg_to_mods(lldb_ac, req->op.add.message, 0);
	if (mods == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	dn = ldb_dn_linearize(lldb_ac, req->op.add.message->dn);
	if (dn == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = ldap_add_ext(lldb->ldap, dn, mods,
			   NULL,
			   NULL,
			   &lldb_ac->msgid);

	if (ret != LDAP_SUCCESS) {
		ldb_set_errstring(module->ldb, ldap_err2string(ret));
	}

	return lldb_ldap_to_ldb(ret);
}

/*
  modify a record
*/
static int lldb_modify(struct ldb_module *module, struct ldb_request *req)
{
	struct lldb_private *lldb = talloc_get_type(module->private_data, struct lldb_private);
	struct lldb_context *lldb_ac;
	LDAPMod **mods;
	char *dn;
	int ret;

	/* ltdb specials should not reach this point */
	if (ldb_dn_is_special(req->op.mod.message->dn)) {
		return LDB_ERR_INVALID_DN_SYNTAX;
	}

	req->handle = init_handle(lldb, module, req->context, req->callback, req->timeout, req->starttime);
	if (req->handle == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	lldb_ac = talloc_get_type(req->handle->private_data, struct lldb_context);

	mods = lldb_msg_to_mods(lldb_ac, req->op.mod.message, 1);
	if (mods == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	dn = ldb_dn_linearize(lldb_ac, req->op.mod.message->dn);
	if (dn == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = ldap_modify_ext(lldb->ldap, dn, mods,
			      NULL,
			      NULL,
			      &lldb_ac->msgid);

	if (ret != LDAP_SUCCESS) {
		ldb_set_errstring(module->ldb, ldap_err2string(ret));
	}

	return lldb_ldap_to_ldb(ret);
}

/*
  delete a record
*/
static int lldb_delete(struct ldb_module *module, struct ldb_request *req)
{
	struct lldb_private *lldb = talloc_get_type(module->private_data, struct lldb_private);
	struct lldb_context *lldb_ac;
	char *dnstr;
	int ret;
	
	/* ltdb specials should not reach this point */
	if (ldb_dn_is_special(req->op.del.dn)) {
		return LDB_ERR_INVALID_DN_SYNTAX;
	}

	req->handle = init_handle(lldb, module, req->context, req->callback, req->timeout, req->starttime);
	if (req->handle == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	lldb_ac = talloc_get_type(req->handle->private_data, struct lldb_context);

	dnstr = ldb_dn_linearize(lldb_ac, req->op.del.dn);

	ret = ldap_delete_ext(lldb->ldap, dnstr,
			      NULL,
			      NULL,
			      &lldb_ac->msgid);

	if (ret != LDAP_SUCCESS) {
		ldb_set_errstring(module->ldb, ldap_err2string(ret));
	}

	return lldb_ldap_to_ldb(ret);
}

/*
  rename a record
*/
static int lldb_rename(struct ldb_module *module, struct ldb_request *req)
{
	struct lldb_private *lldb = talloc_get_type(module->private_data, struct lldb_private);
	struct lldb_context *lldb_ac;
	char *old_dn;
       	char *newrdn;
	char *parentdn;
	int ret;
	
	/* ltdb specials should not reach this point */
	if (ldb_dn_is_special(req->op.rename.olddn) || ldb_dn_is_special(req->op.rename.newdn)) {
		return LDB_ERR_INVALID_DN_SYNTAX;
	}

	req->handle = init_handle(lldb, module, req->context, req->callback, req->timeout, req->starttime);
	if (req->handle == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	lldb_ac = talloc_get_type(req->handle->private_data, struct lldb_context);

	old_dn = ldb_dn_linearize(lldb_ac, req->op.rename.olddn);
	if (old_dn == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	newrdn = talloc_asprintf(lldb_ac, "%s=%s",
				 ldb_dn_get_rdn_name(req->op.rename.newdn),
				 ldb_dn_escape_value(lldb, *(ldb_dn_get_rdn_val(req->op.rename.newdn))));
	if (!newrdn) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	parentdn = ldb_dn_linearize(lldb_ac, ldb_dn_get_parent(lldb_ac, req->op.rename.newdn));
	if (!parentdn) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = ldap_rename(lldb->ldap, old_dn, newrdn, parentdn,
			  1, NULL, NULL,
			  &lldb_ac->msgid);

	if (ret != LDAP_SUCCESS) {
		ldb_set_errstring(module->ldb, ldap_err2string(ret));
	}

	return lldb_ldap_to_ldb(ret);
}

static int lldb_parse_result(struct ldb_handle *handle, LDAPMessage *result)
{
	struct lldb_context *ac = talloc_get_type(handle->private_data, struct lldb_context);
	struct lldb_private *lldb = talloc_get_type(ac->module->private_data, struct lldb_private);
	struct ldb_reply *ares = NULL;
	LDAPMessage *msg;
	int type;
	char *matcheddnp = NULL;
	char *errmsgp = NULL;
	char **referralsp = NULL;
	LDAPControl **serverctrlsp = NULL;
	int ret = LDB_SUCCESS;
	
	type = ldap_msgtype(result);

	handle->status = 0;

	switch (type) {

	case LDAP_RES_SEARCH_ENTRY:
		msg = ldap_first_entry(lldb->ldap, result);
	       	if (msg != NULL) {
			BerElement *berptr = NULL;
			char *attr, *dn;

			ares = talloc_zero(ac, struct ldb_reply);
			if (!ares) {
				ret = LDB_ERR_OPERATIONS_ERROR;
				goto error;
			}

			ares->message = ldb_msg_new(ares);
			if (!ares->message) {
				ret = LDB_ERR_OPERATIONS_ERROR;
				goto error;
			}

			dn = ldap_get_dn(lldb->ldap, msg);
			if (!dn) {
				ret = LDB_ERR_OPERATIONS_ERROR;
				goto error;
			}
			ares->message->dn = ldb_dn_explode_or_special(ares->message, dn);
			if (ares->message->dn == NULL) {
				ret = LDB_ERR_OPERATIONS_ERROR;
				goto error;
			}
			ldap_memfree(dn);

			ares->message->num_elements = 0;
			ares->message->elements = NULL;
			ares->message->private_data = NULL;

			/* loop over all attributes */
			for (attr=ldap_first_attribute(lldb->ldap, msg, &berptr);
			     attr;
			     attr=ldap_next_attribute(lldb->ldap, msg, berptr)) {
				struct berval **bval;
				bval = ldap_get_values_len(lldb->ldap, msg, attr);

				if (bval) {
					lldb_add_msg_attr(ac->module->ldb, ares->message, attr, bval);
					ldap_value_free_len(bval);
				}					  
			}
			if (berptr) ber_free(berptr, 0);


			ares->type = LDB_REPLY_ENTRY;
			ret = ac->callback(ac->module->ldb, ac->context, ares);
		} else {
			handle->status = LDB_ERR_PROTOCOL_ERROR;
			handle->state = LDB_ASYNC_DONE;
		}
		break;

	case LDAP_RES_SEARCH_REFERENCE:
		if (ldap_parse_result(lldb->ldap, result, &handle->status,
					&matcheddnp, &errmsgp,
					&referralsp, &serverctrlsp, 0) != LDAP_SUCCESS) {
			ret = LDB_ERR_OPERATIONS_ERROR;
			goto error;
		}
		if (referralsp == NULL) {
			handle->status = LDB_ERR_PROTOCOL_ERROR;
			goto error;
		}

		ares = talloc_zero(ac, struct ldb_reply);
		if (!ares) {
			ret = LDB_ERR_OPERATIONS_ERROR;
			goto error;
		}

		ares->referral = talloc_strdup(ares, *referralsp);
		ares->type = LDB_REPLY_REFERRAL;
		ret = ac->callback(ac->module->ldb, ac->context, ares);

		break;

	case LDAP_RES_SEARCH_RESULT:
		if (ldap_parse_result(lldb->ldap, result, &handle->status,
					&matcheddnp, &errmsgp,
					&referralsp, &serverctrlsp, 0) != LDAP_SUCCESS) {
			handle->status = LDB_ERR_OPERATIONS_ERROR;
			goto error;
		}

		ares = talloc_zero(ac, struct ldb_reply);
		if (!ares) {
			ret = LDB_ERR_OPERATIONS_ERROR;
			goto error;
		}

		if (serverctrlsp != NULL) {
			/* FIXME: transform the LDAPControl list into an ldb_control one */
			ares->controls = NULL;
		}
		
		ares->type = LDB_REPLY_DONE;
		handle->state = LDB_ASYNC_DONE;
		ret = ac->callback(ac->module->ldb, ac->context, ares);

		break;

	case LDAP_RES_MODIFY:
	case LDAP_RES_ADD:
	case LDAP_RES_DELETE:
	case LDAP_RES_MODDN:
		if (ldap_parse_result(lldb->ldap, result, &handle->status,
					&matcheddnp, &errmsgp,
					&referralsp, &serverctrlsp, 0) != LDAP_SUCCESS) {
			handle->status = LDB_ERR_OPERATIONS_ERROR;
			goto error;
		}
		if (ac->callback && handle->status == LDB_SUCCESS) {
			ares = NULL; /* FIXME: build a corresponding ares to pass on */
			ret = ac->callback(ac->module->ldb, ac->context, ares);
		}
		handle->state = LDB_ASYNC_DONE;
		break;

	default:
		ret = LDB_ERR_PROTOCOL_ERROR;
		goto error;
	}

	if (matcheddnp) ldap_memfree(matcheddnp);
	if (errmsgp && *errmsgp) {
		ldb_set_errstring(ac->module->ldb, errmsgp);
	} else if (handle->status) {
		ldb_set_errstring(ac->module->ldb, ldap_err2string(handle->status));
	}
	if (errmsgp) {
		ldap_memfree(errmsgp);
	}
	if (referralsp) ldap_value_free(referralsp);
	if (serverctrlsp) ldap_controls_free(serverctrlsp);

	ldap_msgfree(result);
	return lldb_ldap_to_ldb(handle->status);

error:
	handle->state = LDB_ASYNC_DONE;
	ldap_msgfree(result);
	return ret;
}

static int lldb_wait(struct ldb_handle *handle, enum ldb_wait_type type)
{
	struct lldb_context *ac = talloc_get_type(handle->private_data, struct lldb_context);
	struct lldb_private *lldb = talloc_get_type(handle->module->private_data, struct lldb_private);
	struct timeval timeout;
	LDAPMessage *result;
	int ret, lret;

	if (handle->state == LDB_ASYNC_DONE) {
		return handle->status;
	}

	if (!ac || !ac->msgid) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	handle->state = LDB_ASYNC_PENDING;
	handle->status = LDB_SUCCESS;

	switch(type) {
	case LDB_WAIT_NONE:

		if ((ac->timeout != -1) &&
		    ((ac->starttime + ac->timeout) > time(NULL))) {
			return LDB_ERR_TIME_LIMIT_EXCEEDED;
		}

		timeout.tv_sec = 0;
		timeout.tv_usec = 0;

		lret = ldap_result(lldb->ldap, ac->msgid, 0, &timeout, &result);
		if (lret == -1) {
			return LDB_ERR_OPERATIONS_ERROR;
		}
		if (lret == 0) {
			ret = LDB_SUCCESS;
			goto done;
		}

		return lldb_parse_result(handle, result);

	case LDB_WAIT_ALL:
		timeout.tv_usec = 0;
		ret = LDB_ERR_OPERATIONS_ERROR;

		while (handle->status == LDB_SUCCESS && handle->state != LDB_ASYNC_DONE) {

			if (ac->timeout == -1) {
				lret = ldap_result(lldb->ldap, ac->msgid, 0, NULL, &result);
			} else {
				timeout.tv_sec = ac->timeout - (time(NULL) - ac->starttime);
				if (timeout.tv_sec <= 0)
					return LDB_ERR_TIME_LIMIT_EXCEEDED;
				lret = ldap_result(lldb->ldap, ac->msgid, 0, &timeout, &result);
			}
			if (lret == -1) {
				return LDB_ERR_OPERATIONS_ERROR;
			}
			if (lret == 0) {
				return LDB_ERR_TIME_LIMIT_EXCEEDED;
			}

			ret = lldb_parse_result(handle, result);
			if (ret != LDB_SUCCESS) {
				return ret;
			}
		}

		break;
		
	default:
		handle->state = LDB_ASYNC_DONE;
		ret = LDB_ERR_OPERATIONS_ERROR;
	}

done:
	return ret;
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

static int lldb_request(struct ldb_module *module, struct ldb_request *req)
{
	return LDB_ERR_OPERATIONS_ERROR;
}

static const struct ldb_module_ops lldb_ops = {
	.name              = "ldap",
	.search            = lldb_search,
	.add               = lldb_add,
	.modify            = lldb_modify,
	.del               = lldb_delete,
	.rename            = lldb_rename,
	.request           = lldb_request,
	.start_transaction = lldb_start_trans,
	.end_transaction   = lldb_end_trans,
	.del_transaction   = lldb_del_trans,
	.wait              = lldb_wait
};


static int lldb_destructor(struct lldb_private *lldb)
{
	ldap_unbind(lldb->ldap);
	return 0;
}

/*
  connect to the database
*/
static int lldb_connect(struct ldb_context *ldb,
			const char *url, 
			unsigned int flags, 
			const char *options[],
			struct ldb_module **module)
{
	struct lldb_private *lldb = NULL;
	int version = 3;
	int ret;

	lldb = talloc(ldb, struct lldb_private);
	if (!lldb) {
		ldb_oom(ldb);
		goto failed;
	}

	lldb->ldap = NULL;

	ret = ldap_initialize(&lldb->ldap, url);
	if (ret != LDAP_SUCCESS) {
		ldb_debug(ldb, LDB_DEBUG_FATAL, "ldap_initialize failed for URL '%s' - %s\n",
			  url, ldap_err2string(ret));
		goto failed;
	}

	talloc_set_destructor(lldb, lldb_destructor);

	ret = ldap_set_option(lldb->ldap, LDAP_OPT_PROTOCOL_VERSION, &version);
	if (ret != LDAP_SUCCESS) {
		ldb_debug(ldb, LDB_DEBUG_FATAL, "ldap_set_option failed - %s\n",
			  ldap_err2string(ret));
		goto failed;
	}

	*module = talloc(ldb, struct ldb_module);
	if (*module == NULL) {
		ldb_oom(ldb);
		talloc_free(lldb);
		return -1;
	}
	talloc_set_name_const(*module, "ldb_ldap backend");
	(*module)->ldb = ldb;
	(*module)->prev = (*module)->next = NULL;
	(*module)->private_data = lldb;
	(*module)->ops = &lldb_ops;

	return 0;

failed:
	talloc_free(lldb);
	return -1;
}

int ldb_ldap_init(void)
{
	return ldb_register_backend("ldap", lldb_connect) +
		   ldb_register_backend("ldapi", lldb_connect) + 
		   ldb_register_backend("ldaps", lldb_connect);
}
