/* 
   ldb database library

   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2007

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
 *  Component: ldb update_keytabs module
 *
 *  Description: Update keytabs whenever their matching secret record changes
 *
 *  Author: Andrew Bartlett
 */

#include "includes.h"
#include "ldb_module.h"
#include "lib/util/dlinklist.h"
#include "auth/credentials/credentials.h"
#include "auth/credentials/credentials_krb5.h"
#include "system/kerberos.h"

struct dn_list {
	struct cli_credentials *creds;
	struct dn_list *prev, *next;
};

struct update_kt_private {
	struct dn_list *changed_dns;
};

struct update_kt_ctx {
	struct ldb_module *module;
	struct ldb_request *req;

	struct ldb_dn *dn;
	bool do_delete;

	struct ldb_reply *op_reply;
	bool found;
};

static struct update_kt_ctx *update_kt_ctx_init(struct ldb_module *module,
						struct ldb_request *req)
{
	struct update_kt_ctx *ac;

	ac = talloc_zero(req, struct update_kt_ctx);
	if (ac == NULL) {
		ldb_oom(ldb_module_get_ctx(module));
		return NULL;
	}

	ac->module = module;
	ac->req = req;

	return ac;
}

/* FIXME: too many semi-async searches here for my taste, direct and indirect as
 * cli_credentials_set_secrets() performs a sync ldb search.
 * Just hope we are lucky and nothing breaks (using the tdb backend masks a lot
 * of async issues). -SSS
 */
static int add_modified(struct ldb_module *module, struct ldb_dn *dn, bool do_delete) {
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	struct update_kt_private *data = talloc_get_type(ldb_module_get_private(module), struct update_kt_private);
	struct dn_list *item;
	char *filter;
	struct ldb_result *res;
	const char *attrs[] = { NULL };
	int ret;
	NTSTATUS status;

	filter = talloc_asprintf(data, "(&(dn=%s)(&(objectClass=kerberosSecret)(privateKeytab=*)))",
				 ldb_dn_get_linearized(dn));
	if (!filter) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = ldb_search(ldb, data, &res,
			 dn, LDB_SCOPE_BASE, attrs, "%s", filter);
	if (ret != LDB_SUCCESS) {
		talloc_free(filter);
		return ret;
	}

	if (res->count != 1) {
		/* if it's not a kerberosSecret then we don't have anything to update */
		talloc_free(res);
		talloc_free(filter);
		return LDB_SUCCESS;
	}
	talloc_free(res);

	item = talloc(data->changed_dns? (void *)data->changed_dns: (void *)data, struct dn_list);
	if (!item) {
		talloc_free(filter);
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	item->creds = cli_credentials_init(item);
	if (!item->creds) {
		DEBUG(1, ("cli_credentials_init failed!"));
		talloc_free(filter);
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	cli_credentials_set_conf(item->creds, ldb_get_opaque(ldb, "loadparm"));
	status = cli_credentials_set_secrets(item->creds, ldb_get_event_context(ldb), ldb_get_opaque(ldb, "loadparm"), ldb, NULL, filter);
	talloc_free(filter);
	if (NT_STATUS_IS_OK(status)) {
		if (do_delete) {
			/* Ensure we don't helpfully keep an old keytab entry */
			cli_credentials_set_kvno(item->creds, cli_credentials_get_kvno(item->creds)+2);	
			/* Wipe passwords */
			cli_credentials_set_nt_hash(item->creds, NULL, 
						    CRED_SPECIFIED);
		}
		DLIST_ADD_END(data->changed_dns, item, struct dn_list *);
	}
	return LDB_SUCCESS;
}

static int ukt_search_modified(struct update_kt_ctx *ac);

static int update_kt_op_callback(struct ldb_request *req,
				 struct ldb_reply *ares)
{
	struct ldb_context *ldb;
	struct update_kt_ctx *ac;
	int ret;

	ac = talloc_get_type(req->context, struct update_kt_ctx);
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
		ldb_set_errstring(ldb, "Invalid request type!\n");
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}

	if (ac->do_delete) {
		return ldb_module_done(ac->req, ares->controls,
					ares->response, LDB_SUCCESS);
	}

	ac->op_reply = talloc_steal(ac, ares);

	ret = ukt_search_modified(ac);
	if (ret != LDB_SUCCESS) {
		return ldb_module_done(ac->req, NULL, NULL, ret);
	}

	return LDB_SUCCESS;
}

static int ukt_del_op(struct update_kt_ctx *ac)
{
	struct ldb_context *ldb;
	struct ldb_request *down_req;
	int ret;

	ldb = ldb_module_get_ctx(ac->module);

	ret = ldb_build_del_req(&down_req, ldb, ac,
				ac->dn,
				ac->req->controls,
				ac, update_kt_op_callback,
				ac->req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}
	return ldb_next_request(ac->module, down_req);
}

static int ukt_search_modified_callback(struct ldb_request *req,
					struct ldb_reply *ares)
{
	struct update_kt_ctx *ac;
	int ret;

	ac = talloc_get_type(req->context, struct update_kt_ctx);

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

		ac->found = true;
		break;

	case LDB_REPLY_REFERRAL:
		/* ignore */
		break;

	case LDB_REPLY_DONE:

		if (ac->found) {
			/* do the dirty sync job here :/ */
			ret = add_modified(ac->module, ac->dn, ac->do_delete);
		}

		if (ac->do_delete) {
			ret = ukt_del_op(ac);
			if (ret != LDB_SUCCESS) {
				return ldb_module_done(ac->req,
							NULL, NULL, ret);
			}
			break;
		}

		return ldb_module_done(ac->req, ac->op_reply->controls,
					ac->op_reply->response, LDB_SUCCESS);
	}

	talloc_free(ares);
	return LDB_SUCCESS;
}

static int ukt_search_modified(struct update_kt_ctx *ac)
{
	struct ldb_context *ldb;
	static const char * const attrs[] = { "distinguishedName", NULL };
	struct ldb_request *search_req;
	int ret;

	ldb = ldb_module_get_ctx(ac->module);

	ret = ldb_build_search_req(&search_req, ldb, ac,
				   ac->dn, LDB_SCOPE_BASE,
				   "(&(objectClass=kerberosSecret)"
				     "(privateKeytab=*))", attrs,
				   NULL,
				   ac, ukt_search_modified_callback,
				   ac->req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}
	return ldb_next_request(ac->module, search_req);
}


/* add */
static int update_kt_add(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb;
	struct update_kt_ctx *ac;
	struct ldb_request *down_req;
	int ret;

	ldb = ldb_module_get_ctx(module);

	ac = update_kt_ctx_init(module, req);
	if (ac == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ac->dn = req->op.add.message->dn;

	ret = ldb_build_add_req(&down_req, ldb, ac,
				req->op.add.message,
				req->controls,
				ac, update_kt_op_callback,
				req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	return ldb_next_request(module, down_req);
}

/* modify */
static int update_kt_modify(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb;
	struct update_kt_ctx *ac;
	struct ldb_request *down_req;
	int ret;

	ldb = ldb_module_get_ctx(module);

	ac = update_kt_ctx_init(module, req);
	if (ac == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ac->dn = req->op.mod.message->dn;

	ret = ldb_build_mod_req(&down_req, ldb, ac,
				req->op.mod.message,
				req->controls,
				ac, update_kt_op_callback,
				req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	return ldb_next_request(module, down_req);
}

/* delete */
static int update_kt_delete(struct ldb_module *module, struct ldb_request *req)
{
	struct update_kt_ctx *ac;

	ac = update_kt_ctx_init(module, req);
	if (ac == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ac->dn = req->op.del.dn;
	ac->do_delete = true;

	return ukt_search_modified(ac);
}

/* rename */
static int update_kt_rename(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb;
	struct update_kt_ctx *ac;
	struct ldb_request *down_req;
	int ret;

	ldb = ldb_module_get_ctx(module);

	ac = update_kt_ctx_init(module, req);
	if (ac == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ac->dn = req->op.rename.newdn;

	ret = ldb_build_rename_req(&down_req, ldb, ac,
				req->op.rename.olddn,
				req->op.rename.newdn,
				req->controls,
				ac, update_kt_op_callback,
				req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	return ldb_next_request(module, down_req);
}

/* prepare for a commit */
static int update_kt_prepare_commit(struct ldb_module *module)
{
	struct ldb_context *ldb;
	struct update_kt_private *data = talloc_get_type(ldb_module_get_private(module), struct update_kt_private);
	struct dn_list *p;

	ldb = ldb_module_get_ctx(module);

	for (p=data->changed_dns; p; p = p->next) {
		int kret;
		kret = cli_credentials_update_keytab(p->creds, ldb_get_event_context(ldb), ldb_get_opaque(ldb, "loadparm"));
		if (kret != 0) {
			talloc_free(data->changed_dns);
			data->changed_dns = NULL;
			ldb_asprintf_errstring(ldb, "Failed to update keytab: %s", error_message(kret));
			return LDB_ERR_OPERATIONS_ERROR;
		}
	}

	talloc_free(data->changed_dns);
	data->changed_dns = NULL;

	return ldb_next_prepare_commit(module);
}

/* end a transaction */
static int update_kt_del_trans(struct ldb_module *module)
{
	struct update_kt_private *data = talloc_get_type(ldb_module_get_private(module), struct update_kt_private);

	talloc_free(data->changed_dns);
	data->changed_dns = NULL;

	return ldb_next_del_trans(module);
}

static int update_kt_init(struct ldb_module *module)
{
	struct ldb_context *ldb;
	struct update_kt_private *data;

	ldb = ldb_module_get_ctx(module);

	data = talloc(module, struct update_kt_private);
	if (data == NULL) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	data->changed_dns = NULL;

	ldb_module_set_private(module, data);

	return ldb_next_init(module);
}

_PUBLIC_ const struct ldb_module_ops ldb_update_keytab_module_ops = {
	.name		   = "update_keytab",
	.init_context	   = update_kt_init,
	.add               = update_kt_add,
	.modify            = update_kt_modify,
	.rename            = update_kt_rename,
	.del               = update_kt_delete,
	.prepare_commit    = update_kt_prepare_commit,
	.del_transaction   = update_kt_del_trans,
};
