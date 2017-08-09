/*
   ldb database library - ildap backend

   Copyright (C) Andrew Tridgell  2005
   Copyright (C) Simo Sorce       2008

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
 *  Name: ldb_ildap
 *
 *  Component: ldb ildap backend
 *
 *  Description: This is a ldb backend for the internal ldap
 *  client library in Samba4. By using this backend we are
 *  independent of a system ldap library
 *
 *  Author: Andrew Tridgell
 *
 *  Modifications:
 *
 *  - description: make the module use asynchronous calls
 *    date: Feb 2006
 *    author: Simo Sorce
 */

#include "includes.h"
#include "ldb_module.h"
#include "util/dlinklist.h"

#include "libcli/ldap/libcli_ldap.h"
#include "libcli/ldap/ldap_client.h"
#include "auth/auth.h"
#include "auth/credentials/credentials.h"

struct ildb_private {
	struct ldap_connection *ldap;
	struct tevent_context *event_ctx;
};

struct ildb_context {
	struct ldb_module *module;
	struct ldb_request *req;

	struct ildb_private *ildb;
	struct ldap_request *ireq;

	/* indicate we are already processing
	 * the ldap_request in ildb_callback() */
	bool in_ildb_callback;

	bool done;

	struct ildb_destructor_ctx *dc;
};

static void ildb_request_done(struct ildb_context *ctx,
			      struct ldb_control **ctrls, int error)
{
	struct ldb_context *ldb;
	struct ldb_reply *ares;

	ldb = ldb_module_get_ctx(ctx->module);

	ctx->done = true;

	if (ctx->req == NULL) {
		/* if the req has been freed already just return */
		return;
	}

	ares = talloc_zero(ctx->req, struct ldb_reply);
	if (!ares) {
		ldb_oom(ldb);
		ctx->req->callback(ctx->req, NULL);
		return;
	}
	ares->type = LDB_REPLY_DONE;
	ares->controls = talloc_steal(ares, ctrls);
	ares->error = error;

	ctx->req->callback(ctx->req, ares);
}

static void ildb_auto_done_callback(struct tevent_context *ev,
				    struct tevent_timer *te,
				    struct timeval t,
				    void *private_data)
{
	struct ildb_context *ac;

	ac = talloc_get_type(private_data, struct ildb_context);
	ildb_request_done(ac, NULL, LDB_SUCCESS);
}

/*
  convert a ldb_message structure to a list of ldap_mod structures
  ready for ildap_add() or ildap_modify()
*/
static struct ldap_mod **ildb_msg_to_mods(void *mem_ctx, int *num_mods,
					  const struct ldb_message *msg,
					  int use_flags)
{
	struct ldap_mod **mods;
	unsigned int i;
	int n = 0;

	/* allocate maximum number of elements needed */
	mods = talloc_array(mem_ctx, struct ldap_mod *, msg->num_elements+1);
	if (!mods) {
		errno = ENOMEM;
		return NULL;
	}
	mods[0] = NULL;

	for (i = 0; i < msg->num_elements; i++) {
		const struct ldb_message_element *el = &msg->elements[i];

		mods[n] = talloc(mods, struct ldap_mod);
		if (!mods[n]) {
			goto failed;
		}
		mods[n + 1] = NULL;
		mods[n]->type = 0;
		mods[n]->attrib = *el;
		if (use_flags) {
			switch (el->flags & LDB_FLAG_MOD_MASK) {
			case LDB_FLAG_MOD_ADD:
				mods[n]->type = LDAP_MODIFY_ADD;
				break;
			case LDB_FLAG_MOD_DELETE:
				mods[n]->type = LDAP_MODIFY_DELETE;
				break;
			case LDB_FLAG_MOD_REPLACE:
				mods[n]->type = LDAP_MODIFY_REPLACE;
				break;
			}
		}
		n++;
	}

	*num_mods = n;
	return mods;

failed:
	talloc_free(mods);
	return NULL;
}


/*
  map an ildap NTSTATUS to a ldb error code
*/
static int ildb_map_error(struct ldb_module *module, NTSTATUS status)
{
	struct ildb_private *ildb;
	struct ldb_context *ldb;
	TALLOC_CTX *mem_ctx;

	ildb = talloc_get_type(ldb_module_get_private(module), struct ildb_private);
	ldb = ldb_module_get_ctx(module);

	if (NT_STATUS_IS_OK(status)) {
		return LDB_SUCCESS;
	}

	mem_ctx = talloc_new(ildb);
	if (!mem_ctx) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	ldb_set_errstring(ldb,
			  ldap_errstr(ildb->ldap, mem_ctx, status));
	talloc_free(mem_ctx);
	if (NT_STATUS_IS_LDAP(status)) {
		return NT_STATUS_LDAP_CODE(status);
	}
	return LDB_ERR_OPERATIONS_ERROR;
}

static void ildb_request_timeout(struct tevent_context *ev, struct tevent_timer *te,
				 struct timeval t, void *private_data)
{
	struct ildb_context *ac = talloc_get_type(private_data, struct ildb_context);

	if (ac->ireq->state == LDAP_REQUEST_PENDING) {
		DLIST_REMOVE(ac->ireq->conn->pending, ac->ireq);
	}

	ildb_request_done(ac, NULL, LDB_ERR_TIME_LIMIT_EXCEEDED);
}

static void ildb_callback(struct ldap_request *req)
{
	struct ldb_context *ldb;
	struct ildb_context *ac;
	NTSTATUS status;
	struct ldap_SearchResEntry *search;
	struct ldap_message *msg;
	struct ldb_control **controls;
	struct ldb_message *ldbmsg;
	char *referral;
	bool callback_failed;
	bool request_done;
	int ret;
	int i;

	ac = talloc_get_type(req->async.private_data, struct ildb_context);
	ldb = ldb_module_get_ctx(ac->module);
	callback_failed = false;
	request_done = false;
	controls = NULL;

	/* check if we are already processing this request */
	if (ac->in_ildb_callback) {
		return;
	}
	/* mark the request as being in process */
	ac->in_ildb_callback = true;

	if (!NT_STATUS_IS_OK(req->status)) {
		ret = ildb_map_error(ac->module, req->status);
		ildb_request_done(ac, NULL, ret);
		return;
	}

	if (req->num_replies < 1) {
		ret = LDB_ERR_OPERATIONS_ERROR;
		ildb_request_done(ac, NULL, ret);
		return;
	}

	switch (req->type) {

	case LDAP_TAG_ModifyRequest:
		if (req->replies[0]->type != LDAP_TAG_ModifyResponse) {
			ret = LDB_ERR_PROTOCOL_ERROR;
			break;
		}
		status = ldap_check_response(ac->ireq->conn, &req->replies[0]->r.GeneralResult);
		ret = ildb_map_error(ac->module, status);
		request_done = true;
		break;

	case LDAP_TAG_AddRequest:
		if (req->replies[0]->type != LDAP_TAG_AddResponse) {
			ret = LDB_ERR_PROTOCOL_ERROR;
			return;
		}
		status = ldap_check_response(ac->ireq->conn, &req->replies[0]->r.GeneralResult);
		ret = ildb_map_error(ac->module, status);
		request_done = true;
		break;

	case LDAP_TAG_DelRequest:
		if (req->replies[0]->type != LDAP_TAG_DelResponse) {
			ret = LDB_ERR_PROTOCOL_ERROR;
			return;
		}
		status = ldap_check_response(ac->ireq->conn, &req->replies[0]->r.GeneralResult);
		ret = ildb_map_error(ac->module, status);
		request_done = true;
		break;

	case LDAP_TAG_ModifyDNRequest:
		if (req->replies[0]->type != LDAP_TAG_ModifyDNResponse) {
			ret = LDB_ERR_PROTOCOL_ERROR;
			return;
		}
		status = ldap_check_response(ac->ireq->conn, &req->replies[0]->r.GeneralResult);
		ret = ildb_map_error(ac->module, status);
		request_done = true;
		break;

	case LDAP_TAG_SearchRequest:
		/* loop over all messages */
		for (i = 0; i < req->num_replies; i++) {

			msg = req->replies[i];
			switch (msg->type) {

			case LDAP_TAG_SearchResultDone:

				status = ldap_check_response(ac->ireq->conn, &msg->r.GeneralResult);
				if (!NT_STATUS_IS_OK(status)) {
					ret = ildb_map_error(ac->module, status);
					break;
				}

				controls = talloc_steal(ac, msg->controls);
				if (msg->r.SearchResultDone.resultcode) {
					if (msg->r.SearchResultDone.errormessage) {
						ldb_set_errstring(ldb, msg->r.SearchResultDone.errormessage);
					}
				}

				ret = msg->r.SearchResultDone.resultcode;
				request_done = true;
				break;

			case LDAP_TAG_SearchResultEntry:

				ldbmsg = ldb_msg_new(ac);
				if (!ldbmsg) {
					ret = LDB_ERR_OPERATIONS_ERROR;
					break;
				}

				search = &(msg->r.SearchResultEntry);

				ldbmsg->dn = ldb_dn_new(ldbmsg, ldb, search->dn);
				if ( ! ldb_dn_validate(ldbmsg->dn)) {
					ret = LDB_ERR_OPERATIONS_ERROR;
					break;
				}
				ldbmsg->num_elements = search->num_attributes;
				ldbmsg->elements = talloc_move(ldbmsg, &search->attributes);

				controls = talloc_steal(ac, msg->controls);

				ret = ldb_module_send_entry(ac->req, ldbmsg, controls);
				if (ret != LDB_SUCCESS) {
					callback_failed = true;
				}

				break;

			case LDAP_TAG_SearchResultReference:

				referral = talloc_strdup(ac, msg->r.SearchResultReference.referral);

				ret = ldb_module_send_referral(ac->req, referral);
				if (ret != LDB_SUCCESS) {
					callback_failed = true;
				}

				break;

			default:
				/* TAG not handled, fail ! */
				ret = LDB_ERR_PROTOCOL_ERROR;
				break;
			}

			if (ret != LDB_SUCCESS) {
				break;
			}
		}

		talloc_free(req->replies);
		req->replies = NULL;
		req->num_replies = 0;

		break;

	default:
		ret = LDB_ERR_PROTOCOL_ERROR;
		break;
	}

	if (ret != LDB_SUCCESS) {

		/* if the callback failed the caller will have freed the
		 * request. Just return and don't try to use it */
		if ( ! callback_failed) {
			request_done = true;
		}
	}

	/* mark the request as not being in progress */
	ac->in_ildb_callback = false;

	if (request_done) {
		ildb_request_done(ac, controls, ret);
	}

	return;
}

static int ildb_request_send(struct ildb_context *ac, struct ldap_message *msg)
{
	struct ldb_context *ldb;
	struct ldap_request *req;

	if (!ac) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ldb = ldb_module_get_ctx(ac->module);

	ldb_request_set_state(ac->req, LDB_ASYNC_PENDING);

	req = ldap_request_send(ac->ildb->ldap, msg);
	if (req == NULL) {
		ldb_set_errstring(ldb, "async send request failed");
		return LDB_ERR_OPERATIONS_ERROR;
	}
	ac->ireq = talloc_reparent(ac->ildb->ldap, ac, req);

	if (!ac->ireq->conn) {
		ldb_set_errstring(ldb, "connection to remote LDAP server dropped?");
		return LDB_ERR_OPERATIONS_ERROR;
	}

	talloc_free(req->time_event);
	req->time_event = NULL;
	if (ac->req->timeout) {
		req->time_event = tevent_add_timer(ac->ildb->event_ctx, ac,
						   timeval_current_ofs(ac->req->timeout, 0),
						   ildb_request_timeout, ac);
	}

	req->async.fn = ildb_callback;
	req->async.private_data = ac;

	return LDB_SUCCESS;
}

/*
  search for matching records using an asynchronous function
 */
static int ildb_search(struct ildb_context *ac)
{
	struct ldb_context *ldb;
	struct ldb_request *req = ac->req;
	struct ldap_message *msg;
	int n;

	ldb = ldb_module_get_ctx(ac->module);

	if (!req->callback || !req->context) {
		ldb_set_errstring(ldb, "Async interface called with NULL callback function or NULL context");
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if (req->op.search.tree == NULL) {
		ldb_set_errstring(ldb, "Invalid expression parse tree");
		return LDB_ERR_OPERATIONS_ERROR;
	}

	msg = new_ldap_message(req);
	if (msg == NULL) {
		ldb_set_errstring(ldb, "Out of Memory");
		return LDB_ERR_OPERATIONS_ERROR;
	}

	msg->type = LDAP_TAG_SearchRequest;

	if (req->op.search.base == NULL) {
		msg->r.SearchRequest.basedn = talloc_strdup(msg, "");
	} else {
		msg->r.SearchRequest.basedn  = ldb_dn_get_extended_linearized(msg, req->op.search.base, 0);
	}
	if (msg->r.SearchRequest.basedn == NULL) {
		ldb_set_errstring(ldb, "Unable to determine baseDN");
		talloc_free(msg);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if (req->op.search.scope == LDB_SCOPE_DEFAULT) {
		msg->r.SearchRequest.scope = LDB_SCOPE_SUBTREE;
	} else {
		msg->r.SearchRequest.scope = req->op.search.scope;
	}

	msg->r.SearchRequest.deref  = LDAP_DEREFERENCE_NEVER;
	msg->r.SearchRequest.timelimit = 0;
	msg->r.SearchRequest.sizelimit = 0;
	msg->r.SearchRequest.attributesonly = 0;
	msg->r.SearchRequest.tree = discard_const(req->op.search.tree);

	for (n = 0; req->op.search.attrs && req->op.search.attrs[n]; n++) /* noop */ ;
	msg->r.SearchRequest.num_attributes = n;
	msg->r.SearchRequest.attributes = req->op.search.attrs;
	msg->controls = req->controls;

	return ildb_request_send(ac, msg);
}

/*
  add a record
*/
static int ildb_add(struct ildb_context *ac)
{
	struct ldb_request *req = ac->req;
	struct ldap_message *msg;
	struct ldap_mod **mods;
	int i,n;

	msg = new_ldap_message(req);
	if (msg == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	msg->type = LDAP_TAG_AddRequest;

	msg->r.AddRequest.dn = ldb_dn_get_extended_linearized(msg, req->op.add.message->dn, 0);
	if (msg->r.AddRequest.dn == NULL) {
		talloc_free(msg);
		return LDB_ERR_INVALID_DN_SYNTAX;
	}

	mods = ildb_msg_to_mods(msg, &n, req->op.add.message, 0);
	if (mods == NULL) {
		talloc_free(msg);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	msg->r.AddRequest.num_attributes = n;
	msg->r.AddRequest.attributes = talloc_array(msg, struct ldb_message_element, n);
	if (msg->r.AddRequest.attributes == NULL) {
		talloc_free(msg);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	for (i = 0; i < n; i++) {
		msg->r.AddRequest.attributes[i] = mods[i]->attrib;
	}
	msg->controls = req->controls;

	return ildb_request_send(ac, msg);
}

/*
  modify a record
*/
static int ildb_modify(struct ildb_context *ac)
{
	struct ldb_request *req = ac->req;
	struct ldap_message *msg;
	struct ldap_mod **mods;
	int i,n;

	msg = new_ldap_message(req);
	if (msg == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	msg->type = LDAP_TAG_ModifyRequest;

	msg->r.ModifyRequest.dn = ldb_dn_get_extended_linearized(msg, req->op.mod.message->dn, 0);
	if (msg->r.ModifyRequest.dn == NULL) {
		talloc_free(msg);
		return LDB_ERR_INVALID_DN_SYNTAX;
	}

	mods = ildb_msg_to_mods(msg, &n, req->op.mod.message, 1);
	if (mods == NULL) {
		talloc_free(msg);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	msg->r.ModifyRequest.num_mods = n;
	msg->r.ModifyRequest.mods = talloc_array(msg, struct ldap_mod, n);
	if (msg->r.ModifyRequest.mods == NULL) {
		talloc_free(msg);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	for (i = 0; i < n; i++) {
		msg->r.ModifyRequest.mods[i] = *mods[i];
	}
	msg->controls = req->controls;
	return ildb_request_send(ac, msg);
}

/*
  delete a record
*/
static int ildb_delete(struct ildb_context *ac)
{
	struct ldb_request *req = ac->req;
	struct ldap_message *msg;

	msg = new_ldap_message(req);
	if (msg == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	msg->type = LDAP_TAG_DelRequest;

	msg->r.DelRequest.dn = ldb_dn_get_extended_linearized(msg, req->op.del.dn, 0);
	if (msg->r.DelRequest.dn == NULL) {
		talloc_free(msg);
		return LDB_ERR_INVALID_DN_SYNTAX;
	}
	msg->controls = req->controls;

	return ildb_request_send(ac, msg);
}

/*
  rename a record
*/
static int ildb_rename(struct ildb_context *ac)
{
	struct ldb_request *req = ac->req;
	struct ldap_message *msg;
	const char *rdn_name;
	const struct ldb_val *rdn_val;

	msg = new_ldap_message(req);
	if (msg == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	msg->type = LDAP_TAG_ModifyDNRequest;
	msg->r.ModifyDNRequest.dn = ldb_dn_get_extended_linearized(msg, req->op.rename.olddn, 0);
	if (msg->r.ModifyDNRequest.dn == NULL) {
		talloc_free(msg);
		return LDB_ERR_INVALID_DN_SYNTAX;
	}

	rdn_name = ldb_dn_get_rdn_name(req->op.rename.newdn);
	rdn_val = ldb_dn_get_rdn_val(req->op.rename.newdn);

	if ((rdn_name != NULL) && (rdn_val != NULL)) {
		msg->r.ModifyDNRequest.newrdn =
			talloc_asprintf(msg, "%s=%s", rdn_name,
					rdn_val->length > 0 ? ldb_dn_escape_value(msg, *rdn_val) : "");
	} else {
		msg->r.ModifyDNRequest.newrdn = talloc_strdup(msg, "");
	}
	if (msg->r.ModifyDNRequest.newrdn == NULL) {
		talloc_free(msg);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	msg->r.ModifyDNRequest.newsuperior =
		ldb_dn_alloc_linearized(msg, ldb_dn_get_parent(msg, req->op.rename.newdn));
	if (msg->r.ModifyDNRequest.newsuperior == NULL) {
		talloc_free(msg);
		return LDB_ERR_INVALID_DN_SYNTAX;
	}

	msg->r.ModifyDNRequest.deleteolddn = true;
	msg->controls = req->controls;

	return ildb_request_send(ac, msg);
}

static int ildb_start_trans(struct ldb_module *module)
{
	/* TODO implement a local locking mechanism here */

	return LDB_SUCCESS;
}

static int ildb_end_trans(struct ldb_module *module)
{
	/* TODO implement a local transaction mechanism here */

	return LDB_SUCCESS;
}

static int ildb_del_trans(struct ldb_module *module)
{
	/* TODO implement a local locking mechanism here */

	return LDB_SUCCESS;
}

static bool ildb_dn_is_special(struct ldb_request *req)
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

static int ildb_handle_request(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb;
	struct ildb_private *ildb;
	struct ildb_context *ac;
	struct tevent_timer *te;
	int ret;

	ildb = talloc_get_type(ldb_module_get_private(module), struct ildb_private);
	ldb = ldb_module_get_ctx(module);

	if (req->starttime == 0 || req->timeout == 0) {
		ldb_set_errstring(ldb, "Invalid timeout settings");
		return LDB_ERR_TIME_LIMIT_EXCEEDED;
	}

	ac = talloc_zero(req, struct ildb_context);
	if (ac == NULL) {
		ldb_set_errstring(ldb, "Out of Memory");
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ac->module = module;
	ac->req = req;
	ac->ildb = ildb;

	if (ildb_dn_is_special(req)) {

		te = tevent_add_timer(ac->ildb->event_ctx,
				      ac, timeval_zero(),
				      ildb_auto_done_callback, ac);
		if (NULL == te) {
			return LDB_ERR_OPERATIONS_ERROR;
		}

		return LDB_SUCCESS;
	}

	switch (ac->req->operation) {
	case LDB_SEARCH:
		ret = ildb_search(ac);
		break;
	case LDB_ADD:
		ret = ildb_add(ac);
		break;
	case LDB_MODIFY:
		ret = ildb_modify(ac);
		break;
	case LDB_DELETE:
		ret = ildb_delete(ac);
		break;
	case LDB_RENAME:
		ret = ildb_rename(ac);
		break;
	default:
		/* no other op supported */
		ret = LDB_ERR_PROTOCOL_ERROR;
		break;
	}

	return ret;
}

static const struct ldb_module_ops ildb_ops = {
	.name              = "ldap",
	.search            = ildb_handle_request,
	.add               = ildb_handle_request,
	.modify            = ildb_handle_request,
	.del               = ildb_handle_request,
	.rename            = ildb_handle_request,
/*	.request           = ildb_handle_request, */
	.start_transaction = ildb_start_trans,
	.end_transaction   = ildb_end_trans,
	.del_transaction   = ildb_del_trans,
};

/*
  connect to the database
*/
static int ildb_connect(struct ldb_context *ldb, const char *url,
			unsigned int flags, const char *options[],
			struct ldb_module **_module)
{
	struct ldb_module *module;
	struct ildb_private *ildb;
	NTSTATUS status;
	struct cli_credentials *creds;
	struct loadparm_context *lp_ctx;

	module = ldb_module_new(ldb, ldb, "ldb_ildap backend", &ildb_ops);
	if (!module) return LDB_ERR_OPERATIONS_ERROR;

	ildb = talloc(module, struct ildb_private);
	if (!ildb) {
		ldb_oom(ldb);
		goto failed;
	}
	ldb_module_set_private(module, ildb);

	ildb->event_ctx = ldb_get_event_context(ldb);

	lp_ctx = talloc_get_type(ldb_get_opaque(ldb, "loadparm"),
				 struct loadparm_context);

	ildb->ldap = ldap4_new_connection(ildb, lp_ctx,
					  ildb->event_ctx);
	if (!ildb->ldap) {
		ldb_oom(ldb);
		goto failed;
	}

	if (flags & LDB_FLG_RECONNECT) {
		ldap_set_reconn_params(ildb->ldap, 10);
	}

	status = ldap_connect(ildb->ldap, url);
	if (!NT_STATUS_IS_OK(status)) {
		ldb_debug(ldb, LDB_DEBUG_ERROR, "Failed to connect to ldap URL '%s' - %s",
			  url, ldap_errstr(ildb->ldap, module, status));
		goto failed;
	}

	/* caller can optionally setup credentials using the opaque token 'credentials' */
	creds = talloc_get_type(ldb_get_opaque(ldb, "credentials"), struct cli_credentials);
	if (creds == NULL) {
		struct auth_session_info *session_info = talloc_get_type(ldb_get_opaque(ldb, "sessionInfo"), struct auth_session_info);
		if (session_info) {
			creds = session_info->credentials;
		}
	}

	if (creds != NULL && cli_credentials_authentication_requested(creds)) {
		const char *bind_dn = cli_credentials_get_bind_dn(creds);
		if (bind_dn) {
			const char *password = cli_credentials_get_password(creds);
			status = ldap_bind_simple(ildb->ldap, bind_dn, password);
			if (!NT_STATUS_IS_OK(status)) {
				ldb_debug(ldb, LDB_DEBUG_ERROR, "Failed to bind - %s",
					  ldap_errstr(ildb->ldap, module, status));
				goto failed;
			}
		} else {
			status = ldap_bind_sasl(ildb->ldap, creds, lp_ctx);
			if (!NT_STATUS_IS_OK(status)) {
				ldb_debug(ldb, LDB_DEBUG_ERROR, "Failed to bind - %s",
					  ldap_errstr(ildb->ldap, module, status));
				goto failed;
			}
		}
	}

	*_module = module;
	return LDB_SUCCESS;

failed:
	talloc_free(module);
	return LDB_ERR_OPERATIONS_ERROR;
}

/*
  initialise the module
 */
_PUBLIC_ int ldb_ildap_init(const char *ldb_version)
{
	int ret, i;
	const char *names[] = { "ldap", "ldaps", "ldapi", NULL };
	for (i=0; names[i]; i++) {
		ret = ldb_register_backend(names[i], ildb_connect, true);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}
	return LDB_SUCCESS;
}
