/* 
   ldb database module

   Copyright (C) Simo Sorce  2004-2008
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2005-2006
   Copyright (C) Andrew Tridgell 2004

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
 *  Component: ldb local_password module
 *
 *  Description: correctly update hash values based on changes to userPassword and friends
 *
 *  Author: Andrew Bartlett
 */

#include "includes.h"
#include "libcli/ldap/ldap.h"
#include "ldb_module.h"
#include "dsdb/samdb/samdb.h"
#include "librpc/ndr/libndr.h"
#include "dsdb/samdb/ldb_modules/password_modules.h"

#define PASSWORD_GUID_ATTR "masterGUID"

/* This module maintains a local password database, seperate from the main LDAP server.

   This allows the password database to be syncronised in a multi-master
   fashion, seperate to the more difficult concerns of the main
   database.  (With passwords, the last writer always wins)

   Each incoming add/modify is split into a remote, and a local request, done in that order.

   We maintain a list of attributes that are kept locally - perhaps
   this should use the @KLUDGE_ACL list of passwordAttribute
 */

static const char * const password_attrs[] = {
	"supplementalCredentials",
	"unicodePwd",
	"dBCSPwd",
	"lmPwdHistory", 
	"ntPwdHistory", 
	"msDS-KeyVersionNumber",
	"pwdLastSet"
};

/* And we merge them back into search requests when asked to do so */

struct lpdb_reply {
	struct lpdb_reply *next;
	struct ldb_reply *remote;
	struct ldb_dn *local_dn;
};

struct lpdb_context {

	struct ldb_module *module;
	struct ldb_request *req;

	struct ldb_message *local_message;

	struct lpdb_reply *list;
	struct lpdb_reply *current;
	struct ldb_reply *remote_done;
	struct ldb_reply *remote;

	bool added_objectGUID;
	bool added_objectClass;

};

static struct lpdb_context *lpdb_init_context(struct ldb_module *module,
					      struct ldb_request *req)
{
	struct ldb_context *ldb;
	struct lpdb_context *ac;

	ldb = ldb_module_get_ctx(module);

	ac = talloc_zero(req, struct lpdb_context);
	if (ac == NULL) {
		ldb_set_errstring(ldb, "Out of Memory");
		return NULL;
	}

	ac->module = module;
	ac->req = req;

	return ac;
}

static int lpdb_local_callback(struct ldb_request *req, struct ldb_reply *ares)
{
	struct ldb_context *ldb;
	struct lpdb_context *ac;

	ac = talloc_get_type(req->context, struct lpdb_context);
	ldb = ldb_module_get_ctx(ac->module);

	if (!ares) {
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}
	if (ares->error != LDB_SUCCESS) {
		return ldb_module_done(ac->req, ares->controls,
					ares->response, ares->error);
	}

	if (ares->type != LDB_REPLY_DONE) {
		ldb_set_errstring(ldb, "Unexpected reply type");
		talloc_free(ares);
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}

	talloc_free(ares);
	return ldb_module_done(ac->req,
				ac->remote_done->controls,
				ac->remote_done->response,
				ac->remote_done->error);
}

/*****************************************************************************
 * ADD
 ****************************************************************************/

static int lpdb_add_callback(struct ldb_request *req,
				struct ldb_reply *ares);

static int local_password_add(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb;
	struct ldb_message *remote_message;
	struct ldb_request *remote_req;
	struct lpdb_context *ac;
	struct GUID objectGUID;
	int ret;
	int i;

	ldb = ldb_module_get_ctx(module);
	ldb_debug(ldb, LDB_DEBUG_TRACE, "local_password_add\n");

	if (ldb_dn_is_special(req->op.add.message->dn)) { /* do not manipulate our control entries */
		return ldb_next_request(module, req);
	}

	/* If the caller is manipulating the local passwords directly, let them pass */
	if (ldb_dn_compare_base(ldb_dn_new(req, ldb, LOCAL_BASE),
				req->op.add.message->dn) == 0) {
		return ldb_next_request(module, req);
	}

	for (i=0; i < ARRAY_SIZE(password_attrs); i++) {
		if (ldb_msg_find_element(req->op.add.message, password_attrs[i])) {
			break;
		}
	}

	/* It didn't match any of our password attributes, go on */
	if (i == ARRAY_SIZE(password_attrs)) {
		return ldb_next_request(module, req);
	}

	/* TODO: remove this when userPassword will be in schema */
	if (!ldb_msg_check_string_attribute(req->op.add.message, "objectClass", "person")) {
		ldb_asprintf_errstring(ldb,
					"Cannot relocate a password on entry: %s, does not have objectClass 'person'",
					ldb_dn_get_linearized(req->op.add.message->dn));
		return LDB_ERR_OBJECT_CLASS_VIOLATION;
	}

	/* From here, we assume we have password attributes to split off */
	ac = lpdb_init_context(module, req);
	if (!ac) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	remote_message = ldb_msg_copy_shallow(remote_req, req->op.add.message);
	if (remote_message == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* Remove any password attributes from the remote message */
	for (i=0; i < ARRAY_SIZE(password_attrs); i++) {
		ldb_msg_remove_attr(remote_message, password_attrs[i]);
	}

	/* Find the objectGUID to use as the key */
	objectGUID = samdb_result_guid(ac->req->op.add.message, "objectGUID");

	ac->local_message = ldb_msg_copy_shallow(ac, req->op.add.message);
	if (ac->local_message == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* Remove anything seen in the remote message from the local
	 * message (leaving only password attributes) */
	for (i=0; i < remote_message->num_elements; i++) {
		ldb_msg_remove_attr(ac->local_message, remote_message->elements[i].name);
	}

	/* We must have an objectGUID already, or we don't know where
	 * to add the password.  This may be changed to an 'add and
	 * search', to allow the directory to create the objectGUID */
	if (ldb_msg_find_ldb_val(req->op.add.message, "objectGUID") == NULL) {
		ldb_set_errstring(ldb,
				  "no objectGUID found in search: "
				  "local_password module must be "
				  "onfigured below objectGUID module!\n");
		return LDB_ERR_CONSTRAINT_VIOLATION;
	}

	ac->local_message->dn = ldb_dn_new(ac->local_message,
					   ldb, LOCAL_BASE);
	if ((ac->local_message->dn == NULL) ||
	    ( ! ldb_dn_add_child_fmt(ac->local_message->dn,
				     PASSWORD_GUID_ATTR "=%s",
				     GUID_string(ac->local_message,
							&objectGUID)))) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = ldb_build_add_req(&remote_req, ldb, ac,
				remote_message,
				req->controls,
				ac, lpdb_add_callback,
				req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	return ldb_next_request(module, remote_req);
}

/* Add a record, splitting password attributes from the user's main
 * record */
static int lpdb_add_callback(struct ldb_request *req,
				struct ldb_reply *ares)
{
	struct ldb_context *ldb;
	struct ldb_request *local_req;
	struct lpdb_context *ac;
	int ret;

	ac = talloc_get_type(req->context, struct lpdb_context);
	ldb = ldb_module_get_ctx(ac->module);

	if (!ares) {
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}
	if (ares->error != LDB_SUCCESS) {
		return ldb_module_done(ac->req, ares->controls,
					ares->response, ares->error);
	}

	if (ares->type != LDB_REPLY_DONE) {
		ldb_set_errstring(ldb, "Unexpected reply type");
		talloc_free(ares);
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}

	ac->remote_done = talloc_steal(ac, ares);

	ret = ldb_build_add_req(&local_req, ldb, ac,
				ac->local_message,
				NULL,
				ac, lpdb_local_callback,
				ac->req);
	if (ret != LDB_SUCCESS) {
		return ldb_module_done(ac->req, NULL, NULL, ret);
	}

	ret = ldb_next_request(ac->module, local_req);
	if (ret != LDB_SUCCESS) {
		return ldb_module_done(ac->req, NULL, NULL, ret);
	}
	return LDB_SUCCESS;
}

/*****************************************************************************
 * MODIFY
 ****************************************************************************/

static int lpdb_modify_callabck(struct ldb_request *req,
				struct ldb_reply *ares);
static int lpdb_mod_search_callback(struct ldb_request *req,
				    struct ldb_reply *ares);

static int local_password_modify(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb;
	struct lpdb_context *ac;
	struct ldb_message *remote_message;
	struct ldb_request *remote_req;
	int ret;
	int i;

	ldb = ldb_module_get_ctx(module);
	ldb_debug(ldb, LDB_DEBUG_TRACE, "local_password_modify\n");

	if (ldb_dn_is_special(req->op.mod.message->dn)) { /* do not manipulate our control entries */
		return ldb_next_request(module, req);
	}

	/* If the caller is manipulating the local passwords directly, let them pass */
	if (ldb_dn_compare_base(ldb_dn_new(req, ldb, LOCAL_BASE),
				req->op.mod.message->dn) == 0) {
		return ldb_next_request(module, req);
	}

	for (i=0; i < ARRAY_SIZE(password_attrs); i++) {
		if (ldb_msg_find_element(req->op.add.message, password_attrs[i])) {
			break;
		}
	}

	/* It didn't match any of our password attributes, then we have nothing to do here */
	if (i == ARRAY_SIZE(password_attrs)) {
		return ldb_next_request(module, req);
	}

	/* From here, we assume we have password attributes to split off */
	ac = lpdb_init_context(module, req);
	if (!ac) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	remote_message = ldb_msg_copy_shallow(ac, ac->req->op.mod.message);
	if (remote_message == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* Remove any password attributes from the remote message */
	for (i=0; i < ARRAY_SIZE(password_attrs); i++) {
		ldb_msg_remove_attr(remote_message, password_attrs[i]);
	}

	ac->local_message = ldb_msg_copy_shallow(ac, ac->req->op.mod.message);
	if (ac->local_message == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* Remove anything seen in the remote message from the local
	 * message (leaving only password attributes) */
	for (i=0; i < remote_message->num_elements;i++) {
		ldb_msg_remove_attr(ac->local_message, remote_message->elements[i].name);
	}

	ret = ldb_build_mod_req(&remote_req, ldb, ac,
				remote_message,
				req->controls,
				ac, lpdb_modify_callabck,
				req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	return ldb_next_request(module, remote_req);
}

/* On a modify, we don't have the objectGUID handy, so we need to
 * search our DN for it */
static int lpdb_modify_callabck(struct ldb_request *req,
				struct ldb_reply *ares)
{
	struct ldb_context *ldb;
	static const char * const attrs[] = { "objectGUID", "objectClass", NULL };
	struct ldb_request *search_req;
	struct lpdb_context *ac;
	int ret;

	ac = talloc_get_type(req->context, struct lpdb_context);
	ldb = ldb_module_get_ctx(ac->module);

	if (!ares) {
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}
	if (ares->error != LDB_SUCCESS) {
		return ldb_module_done(ac->req, ares->controls,
					ares->response, ares->error);
	}

	if (ares->type != LDB_REPLY_DONE) {
		ldb_set_errstring(ldb, "Unexpected reply type");
		talloc_free(ares);
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}

	ac->remote_done = talloc_steal(ac, ares);

	/* prepare the search operation */
	ret = ldb_build_search_req(&search_req, ldb, ac,
				   ac->req->op.mod.message->dn, LDB_SCOPE_BASE,
				   "(objectclass=*)", attrs,
				   NULL,
				   ac, lpdb_mod_search_callback,
				   ac->req);
	if (ret != LDB_SUCCESS) {
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}

	ret = ldb_next_request(ac->module, search_req);
	if (ret != LDB_SUCCESS) {
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}
	return LDB_SUCCESS;
}

/* Called when we search for our own entry.  Stores the one entry we
 * expect (as it is a base search) on the context pointer */
static int lpdb_mod_search_callback(struct ldb_request *req,
				    struct ldb_reply *ares)
{
	struct ldb_context *ldb;
	struct ldb_request *local_req;
	struct lpdb_context *ac;
	struct ldb_dn *local_dn;
	struct GUID objectGUID;
	int ret = LDB_SUCCESS;

	ac = talloc_get_type(req->context, struct lpdb_context);
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
		if (ac->remote != NULL) {
			ldb_set_errstring(ldb, "Too many results");
			talloc_free(ares);
			return ldb_module_done(ac->req, NULL, NULL,
						LDB_ERR_OPERATIONS_ERROR);
		}

		ac->remote = talloc_steal(ac, ares);
		break;

	case LDB_REPLY_REFERRAL:

		/* ignore */
		talloc_free(ares);
		break;

	case LDB_REPLY_DONE:
		/* After we find out the objectGUID for the entry, modify the local
		 * password database as required */

		talloc_free(ares);

		/* if it is not an entry of type person this is an error */
		/* TODO: remove this when sambaPassword will be in schema */
		if (ac->remote == NULL) {
			ldb_asprintf_errstring(ldb,
				"entry just modified (%s) not found!",
				ldb_dn_get_linearized(req->op.search.base));
			return ldb_module_done(ac->req, NULL, NULL,
						LDB_ERR_OPERATIONS_ERROR);
		}
		if (!ldb_msg_check_string_attribute(ac->remote->message,
						    "objectClass", "person")) {
			/* Not relevent to us */
			return ldb_module_done(ac->req,
						ac->remote_done->controls,
						ac->remote_done->response,
						ac->remote_done->error);
		}

		if (ldb_msg_find_ldb_val(ac->remote->message,
					 "objectGUID") == NULL) {
			ldb_set_errstring(ldb,
					  "no objectGUID found in search: "
					  "local_password module must be "
					  "configured below objectGUID "
					  "module!\n");
			return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OBJECT_CLASS_VIOLATION);
		}

		objectGUID = samdb_result_guid(ac->remote->message,
						"objectGUID");

		local_dn = ldb_dn_new(ac, ldb, LOCAL_BASE);
		if ((local_dn == NULL) ||
		    ( ! ldb_dn_add_child_fmt(local_dn,
					    PASSWORD_GUID_ATTR "=%s",
					    GUID_string(ac, &objectGUID)))) {
			return ldb_module_done(ac->req, NULL, NULL,
						LDB_ERR_OPERATIONS_ERROR);
		}
		ac->local_message->dn = local_dn;

		ret = ldb_build_mod_req(&local_req, ldb, ac,
					ac->local_message,
					NULL,
					ac, lpdb_local_callback,
					ac->req);
		if (ret != LDB_SUCCESS) {
			return ldb_module_done(ac->req, NULL, NULL, ret);
		}

		/* perform the local update */
		ret = ldb_next_request(ac->module, local_req);
		if (ret != LDB_SUCCESS) {
			return ldb_module_done(ac->req, NULL, NULL, ret);
		}
	}

	return LDB_SUCCESS;
}

/*****************************************************************************
 * DELETE
 ****************************************************************************/

static int lpdb_delete_callabck(struct ldb_request *req,
				struct ldb_reply *ares);
static int lpdb_del_search_callback(struct ldb_request *req,
				    struct ldb_reply *ares);

static int local_password_delete(struct ldb_module *module,
				 struct ldb_request *req)
{
	struct ldb_context *ldb;
	struct ldb_request *remote_req;
	struct lpdb_context *ac;
	int ret;

	ldb = ldb_module_get_ctx(module);
	ldb_debug(ldb, LDB_DEBUG_TRACE, "local_password_delete\n");

	/* do not manipulate our control entries */
	if (ldb_dn_is_special(req->op.mod.message->dn)) {
		return ldb_next_request(module, req);
	}

	/* If the caller is manipulating the local passwords directly,
	 * let them pass */
	if (ldb_dn_compare_base(ldb_dn_new(req, ldb, LOCAL_BASE),
				req->op.del.dn) == 0) {
		return ldb_next_request(module, req);
	}

	/* From here, we assume we have password attributes to split off */
	ac = lpdb_init_context(module, req);
	if (!ac) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = ldb_build_del_req(&remote_req, ldb, ac,
				req->op.del.dn,
				req->controls,
				ac, lpdb_delete_callabck,
				req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	return ldb_next_request(module, remote_req);
}

/* On a modify, we don't have the objectGUID handy, so we need to
 * search our DN for it */
static int lpdb_delete_callabck(struct ldb_request *req,
				struct ldb_reply *ares)
{
	struct ldb_context *ldb;
	static const char * const attrs[] = { "objectGUID", "objectClass", NULL };
	struct ldb_request *search_req;
	struct lpdb_context *ac;
	int ret;

	ac = talloc_get_type(req->context, struct lpdb_context);
	ldb = ldb_module_get_ctx(ac->module);

	if (!ares) {
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}
	if (ares->error != LDB_SUCCESS) {
		return ldb_module_done(ac->req, ares->controls,
					ares->response, ares->error);
	}

	if (ares->type != LDB_REPLY_DONE) {
		ldb_set_errstring(ldb, "Unexpected reply type");
		talloc_free(ares);
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}

	ac->remote_done = talloc_steal(ac, ares);

	/* prepare the search operation */
	ret = ldb_build_search_req(&search_req, ldb, ac,
				   ac->req->op.del.dn, LDB_SCOPE_BASE,
				   "(objectclass=*)", attrs,
				   NULL,
				   ac, lpdb_del_search_callback,
				   ac->req);
	if (ret != LDB_SUCCESS) {
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}

	ret = ldb_next_request(ac->module, search_req);
	if (ret != LDB_SUCCESS) {
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}
	return LDB_SUCCESS;
}

/* Called when we search for our own entry.  Stores the one entry we
 * expect (as it is a base search) on the context pointer */
static int lpdb_del_search_callback(struct ldb_request *req,
				    struct ldb_reply *ares)
{
	struct ldb_context *ldb;
	struct ldb_request *local_req;
	struct lpdb_context *ac;
	struct ldb_dn *local_dn;
	struct GUID objectGUID;
	int ret = LDB_SUCCESS;

	ac = talloc_get_type(req->context, struct lpdb_context);
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
		if (ac->remote != NULL) {
			ldb_set_errstring(ldb, "Too many results");
			talloc_free(ares);
			return ldb_module_done(ac->req, NULL, NULL,
						LDB_ERR_OPERATIONS_ERROR);
		}

		ac->remote = talloc_steal(ac, ares);
		break;

	case LDB_REPLY_REFERRAL:

		/* ignore */
		talloc_free(ares);
		break;

	case LDB_REPLY_DONE:
		/* After we find out the objectGUID for the entry, modify the local
		 * password database as required */

		talloc_free(ares);

		/* if it is not an entry of type person this is NOT an error */
		/* TODO: remove this when sambaPassword will be in schema */
		if (ac->remote == NULL) {
			return ldb_module_done(ac->req,
						ac->remote_done->controls,
						ac->remote_done->response,
						ac->remote_done->error);
		}
		if (!ldb_msg_check_string_attribute(ac->remote->message,
						    "objectClass", "person")) {
			/* Not relevent to us */
			return ldb_module_done(ac->req,
						ac->remote_done->controls,
						ac->remote_done->response,
						ac->remote_done->error);
		}

		if (ldb_msg_find_ldb_val(ac->remote->message,
					 "objectGUID") == NULL) {
			ldb_set_errstring(ldb,
					  "no objectGUID found in search: "
					  "local_password module must be "
					  "configured below objectGUID "
					  "module!\n");
			return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OBJECT_CLASS_VIOLATION);
		}

		objectGUID = samdb_result_guid(ac->remote->message,
						"objectGUID");

		local_dn = ldb_dn_new(ac, ldb, LOCAL_BASE);
		if ((local_dn == NULL) ||
		    ( ! ldb_dn_add_child_fmt(local_dn,
					    PASSWORD_GUID_ATTR "=%s",
					    GUID_string(ac, &objectGUID)))) {
			return ldb_module_done(ac->req, NULL, NULL,
						LDB_ERR_OPERATIONS_ERROR);
		}

		ret = ldb_build_del_req(&local_req, ldb, ac,
					local_dn,
					NULL,
					ac, lpdb_local_callback,
					ac->req);
		if (ret != LDB_SUCCESS) {
			return ldb_module_done(ac->req, NULL, NULL, ret);
		}

		/* perform the local update */
		ret = ldb_next_request(ac->module, local_req);
		if (ret != LDB_SUCCESS) {
			return ldb_module_done(ac->req, NULL, NULL, ret);
		}
	}

	return LDB_SUCCESS;
}


/*****************************************************************************
 * SEARCH
 ****************************************************************************/

static int lpdb_local_search_callback(struct ldb_request *req,
					struct ldb_reply *ares);

static int lpdb_local_search(struct lpdb_context *ac)
{
	struct ldb_context *ldb;
	struct ldb_request *local_req;
	int ret;

	ldb = ldb_module_get_ctx(ac->module);

	ret = ldb_build_search_req(&local_req, ldb, ac,
				   ac->current->local_dn,
				   LDB_SCOPE_BASE,
				   "(objectclass=*)",
				   ac->req->op.search.attrs,
				   NULL,
				   ac, lpdb_local_search_callback,
				   ac->req);
	if (ret != LDB_SUCCESS) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	return ldb_next_request(ac->module, local_req);
}

static int lpdb_local_search_callback(struct ldb_request *req,
					struct ldb_reply *ares)
{
	struct ldb_context *ldb;
	struct lpdb_context *ac;
	struct ldb_reply *merge;
	struct lpdb_reply *lr;
	int ret;
	int i;

	ac = talloc_get_type(req->context, struct lpdb_context);
	ldb = ldb_module_get_ctx(ac->module);

	if (!ares) {
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}
	if (ares->error != LDB_SUCCESS) {
		return ldb_module_done(ac->req, ares->controls,
					ares->response, ares->error);
	}

	lr = ac->current;

	/* we are interested only in a single reply (base search) */
	switch (ares->type) {
	case LDB_REPLY_ENTRY:

		if (lr->remote == NULL) {
			ldb_set_errstring(ldb,
				"Too many results for password entry search!");
			talloc_free(ares);
			return ldb_module_done(ac->req, NULL, NULL,
						LDB_ERR_OPERATIONS_ERROR);
		}

		merge = lr->remote;
		lr->remote = NULL;

		/* steal the local results on the remote results to be
		 * returned all together */
		talloc_steal(merge, ares->message->elements);

		/* Make sure never to return the internal key attribute */
		ldb_msg_remove_attr(ares->message, PASSWORD_GUID_ATTR);

		for (i=0; i < ares->message->num_elements; i++) {
			struct ldb_message_element *el;
			
			el = ldb_msg_find_element(merge->message,
						  ares->message->elements[i].name);
			if (!el) {
				ret = ldb_msg_add_empty(merge->message,
							ares->message->elements[i].name,
							0, &el);
				if (ret != LDB_SUCCESS) {
					talloc_free(ares);
					return ldb_module_done(ac->req,
								NULL, NULL,
								LDB_ERR_OPERATIONS_ERROR);
				}
				*el = ares->message->elements[i];
			}
		}

		/* free the rest */
		talloc_free(ares);

		return ldb_module_send_entry(ac->req, merge->message, merge->controls);

	case LDB_REPLY_REFERRAL:
		/* ignore */
		talloc_free(ares);
		break;

	case LDB_REPLY_DONE:

		talloc_free(ares);

		/* if this entry was not returned yet, return it now */
		if (lr->remote) {
			ret = ldb_module_send_entry(ac->req, ac->remote->message, ac->remote->controls);
			if (ret != LDB_SUCCESS) {
				return ldb_module_done(ac->req,
							NULL, NULL, ret);
			}
			lr->remote = NULL;
		}

		if (lr->next->remote->type == LDB_REPLY_DONE) {
			/* this was the last one */
			return ldb_module_done(ac->req,
						lr->next->remote->controls,
						lr->next->remote->response,
						lr->next->remote->error);
		} else {
			/* next one */
			ac->current = lr->next;
			talloc_free(lr);

			ret = lpdb_local_search(ac);
			if (ret != LDB_SUCCESS) {
				return ldb_module_done(ac->req,
							NULL, NULL, ret);
			}
		}
	}

	return LDB_SUCCESS;
}

/* For each entry returned in a remote search, do a local base search,
 * based on the objectGUID we asked for as an additional attribute */
static int lpdb_remote_search_callback(struct ldb_request *req,
					struct ldb_reply *ares)
{
	struct ldb_context *ldb;
	struct lpdb_context *ac;
	struct ldb_dn *local_dn;
	struct GUID objectGUID;
	struct lpdb_reply *lr;
	int ret;

	ac = talloc_get_type(req->context, struct lpdb_context);
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
		/* No point searching further if it's not a 'person' entry */
		if (!ldb_msg_check_string_attribute(ares->message, "objectClass", "person")) {

			/* Make sure to remove anything we added */
			if (ac->added_objectGUID) {
				ldb_msg_remove_attr(ares->message, "objectGUID");
			}
			
			if (ac->added_objectClass) {
				ldb_msg_remove_attr(ares->message, "objectClass");
			}
			
			return ldb_module_send_entry(ac->req, ares->message, ares->controls);
		}

		if (ldb_msg_find_ldb_val(ares->message, "objectGUID") == NULL) {
			ldb_set_errstring(ldb, 
					  "no objectGUID found in search: local_password module must be configured below objectGUID module!\n");
			return ldb_module_done(ac->req, NULL, NULL,
						LDB_ERR_OPERATIONS_ERROR);
		}
	
		objectGUID = samdb_result_guid(ares->message, "objectGUID");

		if (ac->added_objectGUID) {
			ldb_msg_remove_attr(ares->message, "objectGUID");
		}

		if (ac->added_objectClass) {
			ldb_msg_remove_attr(ares->message, "objectClass");
		}

		local_dn = ldb_dn_new(ac, ldb, LOCAL_BASE);
		if ((local_dn == NULL) ||
		    (! ldb_dn_add_child_fmt(local_dn,
					    PASSWORD_GUID_ATTR "=%s",
					    GUID_string(ac, &objectGUID)))) {
			return ldb_module_done(ac->req, NULL, NULL,
						LDB_ERR_OPERATIONS_ERROR);
		}

		lr = talloc_zero(ac, struct lpdb_reply);
		if (lr == NULL) {
			return ldb_module_done(ac->req, NULL, NULL,
						LDB_ERR_OPERATIONS_ERROR);
		}
		lr->local_dn = talloc_steal(lr, local_dn);
		lr->remote = talloc_steal(lr, ares);

		if (ac->list) {
			ac->current->next = lr;
		} else {
			ac->list = lr;
		}
		ac->current= lr;

		break;

	case LDB_REPLY_REFERRAL:

		return ldb_module_send_referral(ac->req, ares->referral);

	case LDB_REPLY_DONE:

		if (ac->list == NULL) {
			/* found nothing */
			return ldb_module_done(ac->req, ares->controls,
						ares->response, ares->error);
		}

		lr = talloc_zero(ac, struct lpdb_reply);
		if (lr == NULL) {
			return ldb_module_done(ac->req, NULL, NULL,
						LDB_ERR_OPERATIONS_ERROR);
		}
		lr->remote = talloc_steal(lr, ares);

		ac->current->next = lr;

		/* rewind current and start local searches */
		ac->current= ac->list;

		ret = lpdb_local_search(ac);
		if (ret != LDB_SUCCESS) {
			return ldb_module_done(ac->req, NULL, NULL, ret);
		}
	}

	return LDB_SUCCESS;
}

/* Search for passwords and other attributes.  The passwords are
 * local, but the other attributes are remote, and we need to glue the
 * two search spaces back togeather */

static int local_password_search(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb;
	struct ldb_request *remote_req;
	struct lpdb_context *ac;
	int i;
	int ret;
	const char * const *search_attrs = NULL;

	ldb = ldb_module_get_ctx(module);
	ldb_debug(ldb, LDB_DEBUG_TRACE, "local_password_search\n");

	if (ldb_dn_is_special(req->op.search.base)) { /* do not manipulate our control entries */
		return ldb_next_request(module, req);
	}

	search_attrs = NULL;

	/* If the caller is searching for the local passwords directly, let them pass */
	if (ldb_dn_compare_base(ldb_dn_new(req, ldb, LOCAL_BASE),
				req->op.search.base) == 0) {
		return ldb_next_request(module, req);
	}

	if (req->op.search.attrs && (!ldb_attr_in_list(req->op.search.attrs, "*"))) {
		for (i=0; i < ARRAY_SIZE(password_attrs); i++) {
			if (ldb_attr_in_list(req->op.search.attrs, password_attrs[i])) {
				break;
			}
		}
		
		/* It didn't match any of our password attributes, go on */
		if (i == ARRAY_SIZE(password_attrs)) {
			return ldb_next_request(module, req);
		}
	}

	ac = lpdb_init_context(module, req);
	if (!ac) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* Remote search is for all attributes: if the remote LDAP server has these attributes, then it overrides the local database */
	if (req->op.search.attrs && !ldb_attr_in_list(req->op.search.attrs, "*")) {
		if (!ldb_attr_in_list(req->op.search.attrs, "objectGUID")) {
			search_attrs = ldb_attr_list_copy_add(ac, req->op.search.attrs, "objectGUID");
			ac->added_objectGUID = true;
			if (!search_attrs) {
				return LDB_ERR_OPERATIONS_ERROR;
			}
		} else {
			search_attrs = req->op.search.attrs;
		}
		if (!ldb_attr_in_list(search_attrs, "objectClass")) {
			search_attrs = ldb_attr_list_copy_add(ac, search_attrs, "objectClass");
			ac->added_objectClass = true;
			if (!search_attrs) {
				return LDB_ERR_OPERATIONS_ERROR;
			}
		}
	} else {
		search_attrs = req->op.search.attrs;
	}

	ret = ldb_build_search_req_ex(&remote_req, ldb, ac,
					req->op.search.base,
					req->op.search.scope,
					req->op.search.tree,
					search_attrs,
					req->controls,
					ac, lpdb_remote_search_callback,
					req);
	if (ret != LDB_SUCCESS) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* perform the search */
	return ldb_next_request(module, remote_req);
}

_PUBLIC_ const struct ldb_module_ops ldb_local_password_module_ops = {
	.name          = "local_password",
	.add           = local_password_add,
	.modify        = local_password_modify,
	.del           = local_password_delete,
	.search        = local_password_search
};
