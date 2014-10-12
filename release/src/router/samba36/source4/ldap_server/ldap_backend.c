/* 
   Unix SMB/CIFS implementation.
   LDAP server
   Copyright (C) Stefan Metzmacher 2004
   Copyright (C) Matthias Dieter Wallnöfer 2009
   
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

#include "includes.h"
#include "ldap_server/ldap_server.h"
#include "../lib/util/dlinklist.h"
#include "auth/credentials/credentials.h"
#include "auth/gensec/gensec.h"
#include "param/param.h"
#include "smbd/service_stream.h"
#include "dsdb/samdb/samdb.h"
#include <ldb_errors.h>
#include <ldb_module.h>
#include "ldb_wrap.h"

static int map_ldb_error(TALLOC_CTX *mem_ctx, int ldb_err,
	const char *add_err_string, const char **errstring)
{
	WERROR err;

	/* Certain LDB modules need to return very special WERROR codes. Proof
	 * for them here and if they exist skip the rest of the mapping. */
	if (add_err_string != NULL) {
		char *endptr;
		strtol(add_err_string, &endptr, 16);
		if (endptr != add_err_string) {
			*errstring = add_err_string;
			return ldb_err;
		}
	}

	/* Otherwise we calculate here a generic, but appropriate WERROR. */

	switch (ldb_err) {
	case LDB_SUCCESS:
		err = WERR_OK;
	break;
	case LDB_ERR_OPERATIONS_ERROR:
		err = WERR_DS_OPERATIONS_ERROR;
	break;
	case LDB_ERR_PROTOCOL_ERROR:
		err = WERR_DS_PROTOCOL_ERROR;
	break;
	case LDB_ERR_TIME_LIMIT_EXCEEDED:
		err = WERR_DS_TIMELIMIT_EXCEEDED;
	break;
	case LDB_ERR_SIZE_LIMIT_EXCEEDED:
		err = WERR_DS_SIZELIMIT_EXCEEDED;
	break;
	case LDB_ERR_COMPARE_FALSE:
		err = WERR_DS_COMPARE_FALSE;
	break;
	case LDB_ERR_COMPARE_TRUE:
		err = WERR_DS_COMPARE_TRUE;
	break;
	case LDB_ERR_AUTH_METHOD_NOT_SUPPORTED:
		err = WERR_DS_AUTH_METHOD_NOT_SUPPORTED;
	break;
	case LDB_ERR_STRONG_AUTH_REQUIRED:
		err = WERR_DS_STRONG_AUTH_REQUIRED;
	break;
	case LDB_ERR_REFERRAL:
		err = WERR_DS_REFERRAL;
	break;
	case LDB_ERR_ADMIN_LIMIT_EXCEEDED:
		err = WERR_DS_ADMIN_LIMIT_EXCEEDED;
	break;
	case LDB_ERR_UNSUPPORTED_CRITICAL_EXTENSION:
		err = WERR_DS_UNAVAILABLE_CRIT_EXTENSION;
	break;
	case LDB_ERR_CONFIDENTIALITY_REQUIRED:
		err = WERR_DS_CONFIDENTIALITY_REQUIRED;
	break;
	case LDB_ERR_SASL_BIND_IN_PROGRESS:
		err = WERR_DS_BUSY;
	break;
	case LDB_ERR_NO_SUCH_ATTRIBUTE:
		err = WERR_DS_NO_ATTRIBUTE_OR_VALUE;
	break;
	case LDB_ERR_UNDEFINED_ATTRIBUTE_TYPE:
		err = WERR_DS_ATTRIBUTE_TYPE_UNDEFINED;
	break;
	case LDB_ERR_INAPPROPRIATE_MATCHING:
		err = WERR_DS_INAPPROPRIATE_MATCHING;
	break;
	case LDB_ERR_CONSTRAINT_VIOLATION:
		err = WERR_DS_CONSTRAINT_VIOLATION;
	break;
	case LDB_ERR_ATTRIBUTE_OR_VALUE_EXISTS:
		err = WERR_DS_ATTRIBUTE_OR_VALUE_EXISTS;
	break;
	case LDB_ERR_INVALID_ATTRIBUTE_SYNTAX:
		err = WERR_DS_INVALID_ATTRIBUTE_SYNTAX;
	break;
	case LDB_ERR_NO_SUCH_OBJECT:
		err = WERR_DS_NO_SUCH_OBJECT;
	break;
	case LDB_ERR_ALIAS_PROBLEM:
		err = WERR_DS_ALIAS_PROBLEM;
	break;
	case LDB_ERR_INVALID_DN_SYNTAX:
		err = WERR_DS_INVALID_DN_SYNTAX;
	break;
	case LDB_ERR_ALIAS_DEREFERENCING_PROBLEM:
		err = WERR_DS_ALIAS_DEREF_PROBLEM;
	break;
	case LDB_ERR_INAPPROPRIATE_AUTHENTICATION:
		err = WERR_DS_INAPPROPRIATE_AUTH;
	break;
	case LDB_ERR_INVALID_CREDENTIALS:
		err = WERR_ACCESS_DENIED;
	break;
	case LDB_ERR_INSUFFICIENT_ACCESS_RIGHTS:
		err = WERR_DS_INSUFF_ACCESS_RIGHTS;
	break;
	case LDB_ERR_BUSY:
		err = WERR_DS_BUSY;
	break;
	case LDB_ERR_UNAVAILABLE:
		err = WERR_DS_UNAVAILABLE;
	break;
	case LDB_ERR_UNWILLING_TO_PERFORM:
		err = WERR_DS_UNWILLING_TO_PERFORM;
	break;
	case LDB_ERR_LOOP_DETECT:
		err = WERR_DS_LOOP_DETECT;
	break;
	case LDB_ERR_NAMING_VIOLATION:
		err = WERR_DS_NAMING_VIOLATION;
	break;
	case LDB_ERR_OBJECT_CLASS_VIOLATION:
		err = WERR_DS_OBJ_CLASS_VIOLATION;
	break;
	case LDB_ERR_NOT_ALLOWED_ON_NON_LEAF:
		err = WERR_DS_CANT_ON_NON_LEAF;
	break;
	case LDB_ERR_NOT_ALLOWED_ON_RDN:
		err = WERR_DS_CANT_ON_RDN;
	break;
	case LDB_ERR_ENTRY_ALREADY_EXISTS:
		err = WERR_DS_OBJ_STRING_NAME_EXISTS;
	break;
	case LDB_ERR_OBJECT_CLASS_MODS_PROHIBITED:
		err = WERR_DS_CANT_MOD_OBJ_CLASS;
	break;
	case LDB_ERR_AFFECTS_MULTIPLE_DSAS:
		err = WERR_DS_AFFECTS_MULTIPLE_DSAS;
	break;
	default:
		err = WERR_DS_GENERIC_ERROR;
	break;
	}

	*errstring = talloc_asprintf(mem_ctx, "%08X: %s", W_ERROR_V(err),
		ldb_strerror(ldb_err));
	if (add_err_string != NULL) {
		*errstring = talloc_asprintf(mem_ctx, "%s - %s", *errstring,
					     add_err_string);
	}
	
	/* result is 1:1 for now */
	return ldb_err;
}

/*
  connect to the sam database
*/
NTSTATUS ldapsrv_backend_Init(struct ldapsrv_connection *conn) 
{
	conn->ldb = samdb_connect(conn, 
				     conn->connection->event.ctx,
				     conn->lp_ctx,
				     conn->session_info,
				     conn->global_catalog ? LDB_FLG_RDONLY : 0);
	if (conn->ldb == NULL) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	if (conn->server_credentials) {
		char **sasl_mechs = NULL;
		struct gensec_security_ops **backends = gensec_security_all();
		struct gensec_security_ops **ops
			= gensec_use_kerberos_mechs(conn, backends, conn->server_credentials);
		unsigned int i, j = 0;
		for (i = 0; ops && ops[i]; i++) {
			if (!lpcfg_parm_bool(conn->lp_ctx,  NULL, "gensec", ops[i]->name, ops[i]->enabled))
				continue;

			if (ops[i]->sasl_name && ops[i]->server_start) {
				char *sasl_name = talloc_strdup(conn, ops[i]->sasl_name);

				if (!sasl_name) {
					return NT_STATUS_NO_MEMORY;
				}
				sasl_mechs = talloc_realloc(conn, sasl_mechs, char *, j + 2);
				if (!sasl_mechs) {
					return NT_STATUS_NO_MEMORY;
				}
				sasl_mechs[j] = sasl_name;
				talloc_steal(sasl_mechs, sasl_name);
				sasl_mechs[j+1] = NULL;
				j++;
			}
		}
		talloc_unlink(conn, ops);

		/* ldb can have a different lifetime to conn, so we
		   need to ensure that sasl_mechs lives as long as the
		   ldb does */
		talloc_steal(conn->ldb, sasl_mechs);

		ldb_set_opaque(conn->ldb, "supportedSASLMechanisms", sasl_mechs);
	}

	return NT_STATUS_OK;
}

struct ldapsrv_reply *ldapsrv_init_reply(struct ldapsrv_call *call, uint8_t type)
{
	struct ldapsrv_reply *reply;

	reply = talloc(call, struct ldapsrv_reply);
	if (!reply) {
		return NULL;
	}
	reply->msg = talloc(reply, struct ldap_message);
	if (reply->msg == NULL) {
		talloc_free(reply);
		return NULL;
	}

	reply->msg->messageid = call->request->messageid;
	reply->msg->type = type;
	reply->msg->controls = NULL;

	return reply;
}

void ldapsrv_queue_reply(struct ldapsrv_call *call, struct ldapsrv_reply *reply)
{
	DLIST_ADD_END(call->replies, reply, struct ldapsrv_reply *);
}

static NTSTATUS ldapsrv_unwilling(struct ldapsrv_call *call, int error)
{
	struct ldapsrv_reply *reply;
	struct ldap_ExtendedResponse *r;

	DEBUG(10,("Unwilling type[%d] id[%d]\n", call->request->type, call->request->messageid));

	reply = ldapsrv_init_reply(call, LDAP_TAG_ExtendedResponse);
	if (!reply) {
		return NT_STATUS_NO_MEMORY;
	}

	r = &reply->msg->r.ExtendedResponse;
	r->response.resultcode = error;
	r->response.dn = NULL;
	r->response.errormessage = NULL;
	r->response.referral = NULL;
	r->oid = NULL;
	r->value = NULL;

	ldapsrv_queue_reply(call, reply);
	return NT_STATUS_OK;
}

static int ldapsrv_add_with_controls(struct ldapsrv_call *call,
				     const struct ldb_message *message,
				     struct ldb_control **controls,
				     void *context)
{
	struct ldb_context *ldb = call->conn->ldb;
	struct ldb_request *req;
	int ret;

	ret = ldb_msg_sanity_check(ldb, message);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ret = ldb_build_add_req(&req, ldb, ldb,
					message,
					controls,
					context,
					ldb_modify_default_callback,
					NULL);

	if (ret != LDB_SUCCESS) return ret;

	ret = ldb_transaction_start(ldb);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	if (!call->conn->is_privileged) {
		ldb_req_mark_untrusted(req);
	}

	LDB_REQ_SET_LOCATION(req);

	ret = ldb_request(ldb, req);
	if (ret == LDB_SUCCESS) {
		ret = ldb_wait(req->handle, LDB_WAIT_ALL);
	}

	if (ret == LDB_SUCCESS) {
		ret = ldb_transaction_commit(ldb);
	}
	else {
		ldb_transaction_cancel(ldb);
	}

	talloc_free(req);
	return ret;
}

/* create and execute a modify request */
static int ldapsrv_mod_with_controls(struct ldapsrv_call *call,
				     const struct ldb_message *message,
				     struct ldb_control **controls,
				     void *context)
{
	struct ldb_context *ldb = call->conn->ldb;
	struct ldb_request *req;
	int ret;

	ret = ldb_msg_sanity_check(ldb, message);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ret = ldb_build_mod_req(&req, ldb, ldb,
					message,
					controls,
					context,
					ldb_modify_default_callback,
					NULL);

	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ret = ldb_transaction_start(ldb);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	if (!call->conn->is_privileged) {
		ldb_req_mark_untrusted(req);
	}

	LDB_REQ_SET_LOCATION(req);

	ret = ldb_request(ldb, req);
	if (ret == LDB_SUCCESS) {
		ret = ldb_wait(req->handle, LDB_WAIT_ALL);
	}

	if (ret == LDB_SUCCESS) {
		ret = ldb_transaction_commit(ldb);
	}
	else {
		ldb_transaction_cancel(ldb);
	}

	talloc_free(req);
	return ret;
}

/* create and execute a delete request */
static int ldapsrv_del_with_controls(struct ldapsrv_call *call,
				     struct ldb_dn *dn,
				     struct ldb_control **controls,
				     void *context)
{
	struct ldb_context *ldb = call->conn->ldb;
	struct ldb_request *req;
	int ret;

	ret = ldb_build_del_req(&req, ldb, ldb,
					dn,
					controls,
					context,
					ldb_modify_default_callback,
					NULL);

	if (ret != LDB_SUCCESS) return ret;

	ret = ldb_transaction_start(ldb);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	if (!call->conn->is_privileged) {
		ldb_req_mark_untrusted(req);
	}

	LDB_REQ_SET_LOCATION(req);

	ret = ldb_request(ldb, req);
	if (ret == LDB_SUCCESS) {
		ret = ldb_wait(req->handle, LDB_WAIT_ALL);
	}

	if (ret == LDB_SUCCESS) {
		ret = ldb_transaction_commit(ldb);
	}
	else {
		ldb_transaction_cancel(ldb);
	}

	talloc_free(req);
	return ret;
}

static int ldapsrv_rename_with_controls(struct ldapsrv_call *call,
					struct ldb_dn *olddn,
					struct ldb_dn *newdn,
					struct ldb_control **controls,
					void *context)
{
	struct ldb_context *ldb = call->conn->ldb;
	struct ldb_request *req;
	int ret;

	ret = ldb_build_rename_req(&req, ldb, ldb,
					olddn,
					newdn,
					NULL,
					context,
					ldb_modify_default_callback,
					NULL);

	if (ret != LDB_SUCCESS) return ret;

	ret = ldb_transaction_start(ldb);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	if (!call->conn->is_privileged) {
		ldb_req_mark_untrusted(req);
	}

	LDB_REQ_SET_LOCATION(req);

	ret = ldb_request(ldb, req);
	if (ret == LDB_SUCCESS) {
		ret = ldb_wait(req->handle, LDB_WAIT_ALL);
	}

	if (ret == LDB_SUCCESS) {
		ret = ldb_transaction_commit(ldb);
	}
	else {
		ldb_transaction_cancel(ldb);
	}

	talloc_free(req);
	return ret;
}

static NTSTATUS ldapsrv_SearchRequest(struct ldapsrv_call *call)
{
	struct ldap_SearchRequest *req = &call->request->r.SearchRequest;
	struct ldap_SearchResEntry *ent;
	struct ldap_Result *done;
	struct ldapsrv_reply *ent_r, *done_r;
	TALLOC_CTX *local_ctx;
	struct ldb_context *samdb = talloc_get_type(call->conn->ldb, struct ldb_context);
	struct ldb_dn *basedn;
	struct ldb_result *res = NULL;
	struct ldb_request *lreq;
	struct ldb_control *search_control;
	struct ldb_search_options_control *search_options;
	struct ldb_control *extended_dn_control;
	struct ldb_extended_dn_control *extended_dn_decoded = NULL;
	enum ldb_scope scope = LDB_SCOPE_DEFAULT;
	const char **attrs = NULL;
	const char *scope_str, *errstr = NULL;
	int success_limit = 1;
	int result = -1;
	int ldb_ret = -1;
	unsigned int i, j;
	int extended_type = 1;

	DEBUG(10, ("SearchRequest"));
	DEBUGADD(10, (" basedn: %s", req->basedn));
	DEBUGADD(10, (" filter: %s\n", ldb_filter_from_tree(call, req->tree)));

	local_ctx = talloc_new(call);
	NT_STATUS_HAVE_NO_MEMORY(local_ctx);

	basedn = ldb_dn_new(local_ctx, samdb, req->basedn);
	NT_STATUS_HAVE_NO_MEMORY(basedn);

	DEBUG(10, ("SearchRequest: basedn: [%s]\n", req->basedn));
	DEBUG(10, ("SearchRequest: filter: [%s]\n", ldb_filter_from_tree(call, req->tree)));

	switch (req->scope) {
		case LDAP_SEARCH_SCOPE_BASE:
			scope_str = "BASE";
			scope = LDB_SCOPE_BASE;
			success_limit = 0;
			break;
		case LDAP_SEARCH_SCOPE_SINGLE:
			scope_str = "ONE";
			scope = LDB_SCOPE_ONELEVEL;
			success_limit = 0;
			break;
		case LDAP_SEARCH_SCOPE_SUB:
			scope_str = "SUB";
			scope = LDB_SCOPE_SUBTREE;
			success_limit = 0;
			break;
	        default:
			result = LDAP_PROTOCOL_ERROR;
			map_ldb_error(local_ctx, LDB_ERR_PROTOCOL_ERROR, NULL,
				&errstr);
			errstr = talloc_asprintf(local_ctx,
				"%s. Invalid scope", errstr);
			goto reply;
	}
	DEBUG(10,("SearchRequest: scope: [%s]\n", scope_str));

	if (req->num_attributes >= 1) {
		attrs = talloc_array(local_ctx, const char *, req->num_attributes+1);
		NT_STATUS_HAVE_NO_MEMORY(attrs);

		for (i=0; i < req->num_attributes; i++) {
			DEBUG(10,("SearchRequest: attrs: [%s]\n",req->attributes[i]));
			attrs[i] = req->attributes[i];
		}
		attrs[i] = NULL;
	}

	DEBUG(5,("ldb_request %s dn=%s filter=%s\n", 
		 scope_str, req->basedn, ldb_filter_from_tree(call, req->tree)));

	res = talloc_zero(local_ctx, struct ldb_result);
	NT_STATUS_HAVE_NO_MEMORY(res);

	ldb_ret = ldb_build_search_req_ex(&lreq, samdb, local_ctx,
					  basedn, scope,
					  req->tree, attrs,
					  call->request->controls,
					  res, ldb_search_default_callback,
					  NULL);

	if (ldb_ret != LDB_SUCCESS) {
		goto reply;
	}

	if (call->conn->global_catalog) {
		search_control = ldb_request_get_control(lreq, LDB_CONTROL_SEARCH_OPTIONS_OID);

		search_options = NULL;
		if (search_control) {
			search_options = talloc_get_type(search_control->data, struct ldb_search_options_control);
			search_options->search_options |= LDB_SEARCH_OPTION_PHANTOM_ROOT;
		} else {
			search_options = talloc(lreq, struct ldb_search_options_control);
			NT_STATUS_HAVE_NO_MEMORY(search_options);
			search_options->search_options = LDB_SEARCH_OPTION_PHANTOM_ROOT;
			ldb_request_add_control(lreq, LDB_CONTROL_SEARCH_OPTIONS_OID, false, search_options);
		}
	}

	extended_dn_control = ldb_request_get_control(lreq, LDB_CONTROL_EXTENDED_DN_OID);

	if (extended_dn_control) {
		if (extended_dn_control->data) {
			extended_dn_decoded = talloc_get_type(extended_dn_control->data, struct ldb_extended_dn_control);
			extended_type = extended_dn_decoded->type;
		} else {
			extended_type = 0;
		}
	}

	ldb_set_timeout(samdb, lreq, req->timelimit);

	if (!call->conn->is_privileged) {
		ldb_req_mark_untrusted(lreq);
	}

	LDB_REQ_SET_LOCATION(lreq);

	ldb_ret = ldb_request(samdb, lreq);

	if (ldb_ret != LDB_SUCCESS) {
		goto reply;
	}

	ldb_ret = ldb_wait(lreq->handle, LDB_WAIT_ALL);

	if (ldb_ret == LDB_SUCCESS) {
		for (i = 0; i < res->count; i++) {
			ent_r = ldapsrv_init_reply(call, LDAP_TAG_SearchResultEntry);
			NT_STATUS_HAVE_NO_MEMORY(ent_r);

			/* Better to have the whole message kept here,
			 * than to find someone further up didn't put
			 * a value in the right spot in the talloc tree */
			talloc_steal(ent_r, res->msgs[i]);
			
			ent = &ent_r->msg->r.SearchResultEntry;
			ent->dn = ldb_dn_get_extended_linearized(ent_r, res->msgs[i]->dn, extended_type);
			ent->num_attributes = 0;
			ent->attributes = NULL;
			if (res->msgs[i]->num_elements == 0) {
				goto queue_reply;
			}
			ent->num_attributes = res->msgs[i]->num_elements;
			ent->attributes = talloc_array(ent_r, struct ldb_message_element, ent->num_attributes);
			NT_STATUS_HAVE_NO_MEMORY(ent->attributes);
			for (j=0; j < ent->num_attributes; j++) {
				ent->attributes[j].name = res->msgs[i]->elements[j].name;
				ent->attributes[j].num_values = 0;
				ent->attributes[j].values = NULL;
				if (req->attributesonly && (res->msgs[i]->elements[j].num_values == 0)) {
					continue;
				}
				ent->attributes[j].num_values = res->msgs[i]->elements[j].num_values;
				ent->attributes[j].values = res->msgs[i]->elements[j].values;
			}
queue_reply:
			ldapsrv_queue_reply(call, ent_r);
		}

		/* Send back referrals if they do exist (search operations) */
		if (res->refs != NULL) {
			char **ref;
			struct ldap_SearchResRef *ent_ref;

			for (ref = res->refs; *ref != NULL; ++ref) {
				ent_r = ldapsrv_init_reply(call, LDAP_TAG_SearchResultReference);
				NT_STATUS_HAVE_NO_MEMORY(ent_r);

				/* Better to have the whole referrals kept here,
				 * than to find someone further up didn't put
				 * a value in the right spot in the talloc tree
				 */
				talloc_steal(ent_r, *ref);

				ent_ref = &ent_r->msg->r.SearchResultReference;
				ent_ref->referral = *ref;

				ldapsrv_queue_reply(call, ent_r);
			}
		}
	}

reply:
	done_r = ldapsrv_init_reply(call, LDAP_TAG_SearchResultDone);
	NT_STATUS_HAVE_NO_MEMORY(done_r);

	done = &done_r->msg->r.SearchResultDone;
	done->dn = NULL;
	done->referral = NULL;

	if (result != -1) {
	} else if (ldb_ret == LDB_SUCCESS) {
		if (res->count >= success_limit) {
			DEBUG(10,("SearchRequest: results: [%d]\n", res->count));
			result = LDAP_SUCCESS;
			errstr = NULL;
		}
		if (res->controls) {
			done_r->msg->controls = res->controls;
			talloc_steal(done_r, res->controls);
		}
	} else {
		DEBUG(10,("SearchRequest: error\n"));
		result = map_ldb_error(local_ctx, ldb_ret, ldb_errstring(samdb),
				       &errstr);
	}

	done->resultcode = result;
	done->errormessage = (errstr?talloc_strdup(done_r, errstr):NULL);

	talloc_free(local_ctx);

	ldapsrv_queue_reply(call, done_r);
	return NT_STATUS_OK;
}

static NTSTATUS ldapsrv_ModifyRequest(struct ldapsrv_call *call)
{
	struct ldap_ModifyRequest *req = &call->request->r.ModifyRequest;
	struct ldap_Result *modify_result;
	struct ldapsrv_reply *modify_reply;
	TALLOC_CTX *local_ctx;
	struct ldb_context *samdb = call->conn->ldb;
	struct ldb_message *msg = NULL;
	struct ldb_dn *dn;
	const char *errstr = NULL;
	int result = LDAP_SUCCESS;
	int ldb_ret;
	unsigned int i,j;
	struct ldb_result *res = NULL;

	DEBUG(10, ("ModifyRequest"));
	DEBUGADD(10, (" dn: %s\n", req->dn));

	local_ctx = talloc_named(call, 0, "ModifyRequest local memory context");
	NT_STATUS_HAVE_NO_MEMORY(local_ctx);

	dn = ldb_dn_new(local_ctx, samdb, req->dn);
	NT_STATUS_HAVE_NO_MEMORY(dn);

	DEBUG(10, ("ModifyRequest: dn: [%s]\n", req->dn));

	msg = talloc(local_ctx, struct ldb_message);
	NT_STATUS_HAVE_NO_MEMORY(msg);

	msg->dn = dn;
	msg->num_elements = 0;
	msg->elements = NULL;

	if (req->num_mods > 0) {
		msg->num_elements = req->num_mods;
		msg->elements = talloc_array(msg, struct ldb_message_element, req->num_mods);
		NT_STATUS_HAVE_NO_MEMORY(msg->elements);

		for (i=0; i < msg->num_elements; i++) {
			msg->elements[i].name = discard_const_p(char, req->mods[i].attrib.name);
			msg->elements[i].num_values = 0;
			msg->elements[i].values = NULL;

			switch (req->mods[i].type) {
			default:
				result = LDAP_PROTOCOL_ERROR;
				map_ldb_error(local_ctx,
					LDB_ERR_PROTOCOL_ERROR, NULL, &errstr);
				errstr = talloc_asprintf(local_ctx,
					"%s. Invalid LDAP_MODIFY_* type", errstr);
				goto reply;
			case LDAP_MODIFY_ADD:
				msg->elements[i].flags = LDB_FLAG_MOD_ADD;
				break;
			case LDAP_MODIFY_DELETE:
				msg->elements[i].flags = LDB_FLAG_MOD_DELETE;
				break;
			case LDAP_MODIFY_REPLACE:
				msg->elements[i].flags = LDB_FLAG_MOD_REPLACE;
				break;
			}

			msg->elements[i].num_values = req->mods[i].attrib.num_values;
			if (msg->elements[i].num_values > 0) {
				msg->elements[i].values = talloc_array(msg->elements, struct ldb_val,
								       msg->elements[i].num_values);
				NT_STATUS_HAVE_NO_MEMORY(msg->elements[i].values);

				for (j=0; j < msg->elements[i].num_values; j++) {
					msg->elements[i].values[j].length = req->mods[i].attrib.values[j].length;
					msg->elements[i].values[j].data = req->mods[i].attrib.values[j].data;			
				}
			}
		}
	}

reply:
	modify_reply = ldapsrv_init_reply(call, LDAP_TAG_ModifyResponse);
	NT_STATUS_HAVE_NO_MEMORY(modify_reply);

	if (result == LDAP_SUCCESS) {
		res = talloc_zero(local_ctx, struct ldb_result);
		NT_STATUS_HAVE_NO_MEMORY(res);
		ldb_ret = ldapsrv_mod_with_controls(call, msg, call->request->controls, res);
		result = map_ldb_error(local_ctx, ldb_ret, ldb_errstring(samdb),
				       &errstr);
	}

	modify_result = &modify_reply->msg->r.ModifyResponse;
	modify_result->dn = NULL;
	if ((res != NULL) && (res->refs != NULL)) {
		modify_result->resultcode = map_ldb_error(local_ctx,
							  LDB_ERR_REFERRAL,
							  NULL, &errstr);
		modify_result->errormessage = (errstr?talloc_strdup(modify_reply, errstr):NULL);
		modify_result->referral = talloc_strdup(call, *res->refs);
	} else {
		modify_result->resultcode = result;
		modify_result->errormessage = (errstr?talloc_strdup(modify_reply, errstr):NULL);
		modify_result->referral = NULL;
	}
	talloc_free(local_ctx);

	ldapsrv_queue_reply(call, modify_reply);
	return NT_STATUS_OK;

}

static NTSTATUS ldapsrv_AddRequest(struct ldapsrv_call *call)
{
	struct ldap_AddRequest *req = &call->request->r.AddRequest;
	struct ldap_Result *add_result;
	struct ldapsrv_reply *add_reply;
	TALLOC_CTX *local_ctx;
	struct ldb_context *samdb = call->conn->ldb;
	struct ldb_message *msg = NULL;
	struct ldb_dn *dn;
	const char *errstr = NULL;
	int result = LDAP_SUCCESS;
	int ldb_ret;
	unsigned int i,j;
	struct ldb_result *res = NULL;

	DEBUG(10, ("AddRequest"));
	DEBUGADD(10, (" dn: %s\n", req->dn));

	local_ctx = talloc_named(call, 0, "AddRequest local memory context");
	NT_STATUS_HAVE_NO_MEMORY(local_ctx);

	dn = ldb_dn_new(local_ctx, samdb, req->dn);
	NT_STATUS_HAVE_NO_MEMORY(dn);

	DEBUG(10, ("AddRequest: dn: [%s]\n", req->dn));

	msg = talloc(local_ctx, struct ldb_message);
	NT_STATUS_HAVE_NO_MEMORY(msg);

	msg->dn = dn;
	msg->num_elements = 0;
	msg->elements = NULL;

	if (req->num_attributes > 0) {
		msg->num_elements = req->num_attributes;
		msg->elements = talloc_array(msg, struct ldb_message_element, msg->num_elements);
		NT_STATUS_HAVE_NO_MEMORY(msg->elements);

		for (i=0; i < msg->num_elements; i++) {
			msg->elements[i].name = discard_const_p(char, req->attributes[i].name);
			msg->elements[i].flags = 0;
			msg->elements[i].num_values = 0;
			msg->elements[i].values = NULL;
			
			if (req->attributes[i].num_values > 0) {
				msg->elements[i].num_values = req->attributes[i].num_values;
				msg->elements[i].values = talloc_array(msg->elements, struct ldb_val,
								       msg->elements[i].num_values);
				NT_STATUS_HAVE_NO_MEMORY(msg->elements[i].values);

				for (j=0; j < msg->elements[i].num_values; j++) {
					msg->elements[i].values[j].length = req->attributes[i].values[j].length;
					msg->elements[i].values[j].data = req->attributes[i].values[j].data;			
				}
			}
		}
	}

	add_reply = ldapsrv_init_reply(call, LDAP_TAG_AddResponse);
	NT_STATUS_HAVE_NO_MEMORY(add_reply);

	if (result == LDAP_SUCCESS) {
		res = talloc_zero(local_ctx, struct ldb_result);
		NT_STATUS_HAVE_NO_MEMORY(res);
		ldb_ret = ldapsrv_add_with_controls(call, msg, call->request->controls, res);
		result = map_ldb_error(local_ctx, ldb_ret, ldb_errstring(samdb),
				       &errstr);
	}

	add_result = &add_reply->msg->r.AddResponse;
	add_result->dn = NULL;
	if ((res != NULL) && (res->refs != NULL)) {
		add_result->resultcode =  map_ldb_error(local_ctx,
							LDB_ERR_REFERRAL, NULL,
							&errstr);
		add_result->errormessage = (errstr?talloc_strdup(add_reply,errstr):NULL);
		add_result->referral = talloc_strdup(call, *res->refs);
	} else {
		add_result->resultcode = result;
		add_result->errormessage = (errstr?talloc_strdup(add_reply,errstr):NULL);
		add_result->referral = NULL;
	}
	talloc_free(local_ctx);

	ldapsrv_queue_reply(call, add_reply);
	return NT_STATUS_OK;

}

static NTSTATUS ldapsrv_DelRequest(struct ldapsrv_call *call)
{
	struct ldap_DelRequest *req = &call->request->r.DelRequest;
	struct ldap_Result *del_result;
	struct ldapsrv_reply *del_reply;
	TALLOC_CTX *local_ctx;
	struct ldb_context *samdb = call->conn->ldb;
	struct ldb_dn *dn;
	const char *errstr = NULL;
	int result = LDAP_SUCCESS;
	int ldb_ret;
	struct ldb_result *res = NULL;

	DEBUG(10, ("DelRequest"));
	DEBUGADD(10, (" dn: %s\n", req->dn));

	local_ctx = talloc_named(call, 0, "DelRequest local memory context");
	NT_STATUS_HAVE_NO_MEMORY(local_ctx);

	dn = ldb_dn_new(local_ctx, samdb, req->dn);
	NT_STATUS_HAVE_NO_MEMORY(dn);

	DEBUG(10, ("DelRequest: dn: [%s]\n", req->dn));

	del_reply = ldapsrv_init_reply(call, LDAP_TAG_DelResponse);
	NT_STATUS_HAVE_NO_MEMORY(del_reply);

	if (result == LDAP_SUCCESS) {
		res = talloc_zero(local_ctx, struct ldb_result);
		NT_STATUS_HAVE_NO_MEMORY(res);
		ldb_ret = ldapsrv_del_with_controls(call, dn, call->request->controls, res);
		result = map_ldb_error(local_ctx, ldb_ret, ldb_errstring(samdb),
				       &errstr);
	}

	del_result = &del_reply->msg->r.DelResponse;
	del_result->dn = NULL;
	if ((res != NULL) && (res->refs != NULL)) {
		del_result->resultcode = map_ldb_error(local_ctx,
						       LDB_ERR_REFERRAL, NULL,
						       &errstr);
		del_result->errormessage = (errstr?talloc_strdup(del_reply,errstr):NULL);
		del_result->referral = talloc_strdup(call, *res->refs);
	} else {
		del_result->resultcode = result;
		del_result->errormessage = (errstr?talloc_strdup(del_reply,errstr):NULL);
		del_result->referral = NULL;
	}

	talloc_free(local_ctx);

	ldapsrv_queue_reply(call, del_reply);
	return NT_STATUS_OK;
}

static NTSTATUS ldapsrv_ModifyDNRequest(struct ldapsrv_call *call)
{
	struct ldap_ModifyDNRequest *req = &call->request->r.ModifyDNRequest;
	struct ldap_Result *modifydn;
	struct ldapsrv_reply *modifydn_r;
	TALLOC_CTX *local_ctx;
	struct ldb_context *samdb = call->conn->ldb;
	struct ldb_dn *olddn, *newdn=NULL, *newrdn;
	struct ldb_dn *parentdn = NULL;
	const char *errstr = NULL;
	int result = LDAP_SUCCESS;
	int ldb_ret;
	struct ldb_result *res = NULL;

	DEBUG(10, ("ModifyDNRequest"));
	DEBUGADD(10, (" dn: %s", req->dn));
	DEBUGADD(10, (" newrdn: %s\n", req->newrdn));

	local_ctx = talloc_named(call, 0, "ModifyDNRequest local memory context");
	NT_STATUS_HAVE_NO_MEMORY(local_ctx);

	olddn = ldb_dn_new(local_ctx, samdb, req->dn);
	NT_STATUS_HAVE_NO_MEMORY(olddn);

	newrdn = ldb_dn_new(local_ctx, samdb, req->newrdn);
	NT_STATUS_HAVE_NO_MEMORY(newrdn);

	DEBUG(10, ("ModifyDNRequest: olddn: [%s]\n", req->dn));
	DEBUG(10, ("ModifyDNRequest: newrdn: [%s]\n", req->newrdn));

	if (ldb_dn_get_comp_num(newrdn) == 0) {
		result = LDAP_PROTOCOL_ERROR;
		map_ldb_error(local_ctx, LDB_ERR_PROTOCOL_ERROR, NULL,
			      &errstr);
		goto reply;
	}

	if (ldb_dn_get_comp_num(newrdn) > 1) {
		result = LDAP_NAMING_VIOLATION;
		map_ldb_error(local_ctx, LDB_ERR_NAMING_VIOLATION, NULL,
			      &errstr);
		goto reply;
	}

	/* we can't handle the rename if we should not remove the old dn */
	if (!req->deleteolddn) {
		result = LDAP_UNWILLING_TO_PERFORM;
		map_ldb_error(local_ctx, LDB_ERR_UNWILLING_TO_PERFORM, NULL,
			      &errstr);
		errstr = talloc_asprintf(local_ctx,
			"%s. Old RDN must be deleted", errstr);
		goto reply;
	}

	if (req->newsuperior) {
		DEBUG(10, ("ModifyDNRequest: newsuperior: [%s]\n", req->newsuperior));
		parentdn = ldb_dn_new(local_ctx, samdb, req->newsuperior);
	}

	if (!parentdn) {
		parentdn = ldb_dn_get_parent(local_ctx, olddn);
	}
	if (!parentdn) {
		result = LDAP_NO_SUCH_OBJECT;
		map_ldb_error(local_ctx, LDB_ERR_NO_SUCH_OBJECT, NULL, &errstr);
		goto reply;
	}

	if ( ! ldb_dn_add_child(parentdn, newrdn)) {
		result = LDAP_OTHER;
		map_ldb_error(local_ctx, LDB_ERR_OTHER, NULL, &errstr);
		goto reply;
	}
	newdn = parentdn;

reply:
	modifydn_r = ldapsrv_init_reply(call, LDAP_TAG_ModifyDNResponse);
	NT_STATUS_HAVE_NO_MEMORY(modifydn_r);

	if (result == LDAP_SUCCESS) {
		res = talloc_zero(local_ctx, struct ldb_result);
		NT_STATUS_HAVE_NO_MEMORY(res);
		ldb_ret = ldapsrv_rename_with_controls(call, olddn, newdn, call->request->controls, res);
		result = map_ldb_error(local_ctx, ldb_ret, ldb_errstring(samdb),
				       &errstr);
	}

	modifydn = &modifydn_r->msg->r.ModifyDNResponse;
	modifydn->dn = NULL;
	if ((res != NULL) && (res->refs != NULL)) {
		modifydn->resultcode = map_ldb_error(local_ctx,
						     LDB_ERR_REFERRAL, NULL,
						     &errstr);;
		modifydn->errormessage = (errstr?talloc_strdup(modifydn_r,errstr):NULL);
		modifydn->referral = talloc_strdup(call, *res->refs);
	} else {
		modifydn->resultcode = result;
		modifydn->errormessage = (errstr?talloc_strdup(modifydn_r,errstr):NULL);
		modifydn->referral = NULL;
	}

	talloc_free(local_ctx);

	ldapsrv_queue_reply(call, modifydn_r);
	return NT_STATUS_OK;
}

static NTSTATUS ldapsrv_CompareRequest(struct ldapsrv_call *call)
{
	struct ldap_CompareRequest *req = &call->request->r.CompareRequest;
	struct ldap_Result *compare;
	struct ldapsrv_reply *compare_r;
	TALLOC_CTX *local_ctx;
	struct ldb_context *samdb = call->conn->ldb;
	struct ldb_result *res = NULL;
	struct ldb_dn *dn;
	const char *attrs[1];
	const char *errstr = NULL;
	const char *filter = NULL;
	int result = LDAP_SUCCESS;
	int ldb_ret;

	DEBUG(10, ("CompareRequest"));
	DEBUGADD(10, (" dn: %s\n", req->dn));

	local_ctx = talloc_named(call, 0, "CompareRequest local_memory_context");
	NT_STATUS_HAVE_NO_MEMORY(local_ctx);

	dn = ldb_dn_new(local_ctx, samdb, req->dn);
	NT_STATUS_HAVE_NO_MEMORY(dn);

	DEBUG(10, ("CompareRequest: dn: [%s]\n", req->dn));
	filter = talloc_asprintf(local_ctx, "(%s=%*s)", req->attribute, 
				 (int)req->value.length, req->value.data);
	NT_STATUS_HAVE_NO_MEMORY(filter);

	DEBUGADD(10, ("CompareRequest: attribute: [%s]\n", filter));

	attrs[0] = NULL;

	compare_r = ldapsrv_init_reply(call, LDAP_TAG_CompareResponse);
	NT_STATUS_HAVE_NO_MEMORY(compare_r);

	if (result == LDAP_SUCCESS) {
		ldb_ret = ldb_search(samdb, local_ctx, &res,
				     dn, LDB_SCOPE_BASE, attrs, "%s", filter);
		if (ldb_ret != LDB_SUCCESS) {
			result = map_ldb_error(local_ctx, ldb_ret,
					       ldb_errstring(samdb), &errstr);
			DEBUG(10,("CompareRequest: error: %s\n", errstr));
		} else if (res->count == 0) {
			DEBUG(10,("CompareRequest: doesn't matched\n"));
			result = LDAP_COMPARE_FALSE;
			errstr = NULL;
		} else if (res->count == 1) {
			DEBUG(10,("CompareRequest: matched\n"));
			result = LDAP_COMPARE_TRUE;
			errstr = NULL;
		} else if (res->count > 1) {
			result = LDAP_OTHER;
			map_ldb_error(local_ctx, LDB_ERR_OTHER, NULL, &errstr);
			errstr = talloc_asprintf(local_ctx,
				"%s. Too many objects match!", errstr);
			DEBUG(10,("CompareRequest: %d results: %s\n", res->count, errstr));
		}
	}

	compare = &compare_r->msg->r.CompareResponse;
	compare->dn = NULL;
	compare->resultcode = result;
	compare->errormessage = (errstr?talloc_strdup(compare_r,errstr):NULL);
	compare->referral = NULL;

	talloc_free(local_ctx);

	ldapsrv_queue_reply(call, compare_r);
	return NT_STATUS_OK;
}

static NTSTATUS ldapsrv_AbandonRequest(struct ldapsrv_call *call)
{
/*	struct ldap_AbandonRequest *req = &call->request.r.AbandonRequest;*/
	DEBUG(10, ("AbandonRequest\n"));
	return NT_STATUS_OK;
}

NTSTATUS ldapsrv_do_call(struct ldapsrv_call *call)
{
	unsigned int i;
	struct ldap_message *msg = call->request;
	/* Check for undecoded critical extensions */
	for (i=0; msg->controls && msg->controls[i]; i++) {
		if (!msg->controls_decoded[i] && 
		    msg->controls[i]->critical) {
			DEBUG(3, ("ldapsrv_do_call: Critical extension %s is not known to this server\n",
				  msg->controls[i]->oid));
			return ldapsrv_unwilling(call, LDAP_UNAVAILABLE_CRITICAL_EXTENSION);
		}
	}

	switch(call->request->type) {
	case LDAP_TAG_BindRequest:
		return ldapsrv_BindRequest(call);
	case LDAP_TAG_UnbindRequest:
		return ldapsrv_UnbindRequest(call);
	case LDAP_TAG_SearchRequest:
		return ldapsrv_SearchRequest(call);
	case LDAP_TAG_ModifyRequest:
		return ldapsrv_ModifyRequest(call);
	case LDAP_TAG_AddRequest:
		return ldapsrv_AddRequest(call);
	case LDAP_TAG_DelRequest:
		return ldapsrv_DelRequest(call);
	case LDAP_TAG_ModifyDNRequest:
		return ldapsrv_ModifyDNRequest(call);
	case LDAP_TAG_CompareRequest:
		return ldapsrv_CompareRequest(call);
	case LDAP_TAG_AbandonRequest:
		return ldapsrv_AbandonRequest(call);
	case LDAP_TAG_ExtendedRequest:
		return ldapsrv_ExtendedRequest(call);
	default:
		return ldapsrv_unwilling(call, LDAP_PROTOCOL_ERROR);
	}
}
