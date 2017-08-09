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
#include "auth/kerberos/kerberos.h"
#include "util.h"

struct dn_list {
	struct ldb_message *msg;
	bool do_delete;
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
static int add_modified(struct ldb_module *module, struct ldb_dn *dn, bool do_delete,
			struct ldb_request *parent)
{
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	struct update_kt_private *data = talloc_get_type(ldb_module_get_private(module), struct update_kt_private);
	struct dn_list *item;
	char *filter;
	struct ldb_result *res;
	int ret;

	filter = talloc_asprintf(data, "(&(dn=%s)(&(objectClass=kerberosSecret)(privateKeytab=*)))",
				 ldb_dn_get_linearized(dn));
	if (!filter) {
		return ldb_oom(ldb);
	}

	ret = dsdb_module_search(module, data, &res,
				 dn, LDB_SCOPE_BASE, NULL,
				 DSDB_FLAG_NEXT_MODULE, parent,
				 "%s", filter);
	talloc_free(filter);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	if (res->count != 1) {
		/* if it's not a kerberosSecret then we don't have anything to update */
		talloc_free(res);
		talloc_free(filter);
		return LDB_SUCCESS;
	}

	item = talloc(data->changed_dns? (void *)data->changed_dns: (void *)data, struct dn_list);
	if (!item) {
		talloc_free(res);
		talloc_free(filter);
		return ldb_oom(ldb);
	}

	item->msg = talloc_steal(item, res->msgs[0]);
	item->do_delete = do_delete;
	talloc_free(res);

	DLIST_ADD_END(data->changed_dns, item, struct dn_list *);
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
	LDB_REQ_SET_LOCATION(down_req);
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
			ret = add_modified(ac->module, ac->dn, ac->do_delete, ac->req);
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
	static const char * const no_attrs[] = { NULL };
	struct ldb_request *search_req;
	int ret;

	ldb = ldb_module_get_ctx(ac->module);

	ret = ldb_build_search_req(&search_req, ldb, ac,
				   ac->dn, LDB_SCOPE_BASE,
				   "(&(objectClass=kerberosSecret)"
				     "(privateKeytab=*))", no_attrs,
				   NULL,
				   ac, ukt_search_modified_callback,
				   ac->req);
	LDB_REQ_SET_LOCATION(search_req);
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
		return ldb_operr(ldb);
	}

	ac->dn = req->op.add.message->dn;

	ret = ldb_build_add_req(&down_req, ldb, ac,
				req->op.add.message,
				req->controls,
				ac, update_kt_op_callback,
				req);
	LDB_REQ_SET_LOCATION(down_req);
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
		return ldb_operr(ldb);
	}

	ac->dn = req->op.mod.message->dn;

	ret = ldb_build_mod_req(&down_req, ldb, ac,
				req->op.mod.message,
				req->controls,
				ac, update_kt_op_callback,
				req);
	LDB_REQ_SET_LOCATION(down_req);
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
		return ldb_operr(ldb_module_get_ctx(module));
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
		return ldb_operr(ldb);
	}

	ac->dn = req->op.rename.newdn;

	ret = ldb_build_rename_req(&down_req, ldb, ac,
				req->op.rename.olddn,
				req->op.rename.newdn,
				req->controls,
				ac, update_kt_op_callback,
				req);
	LDB_REQ_SET_LOCATION(down_req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	return ldb_next_request(module, down_req);
}

/* prepare for a commit */
static int update_kt_prepare_commit(struct ldb_module *module)
{
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	struct update_kt_private *data = talloc_get_type(ldb_module_get_private(module), struct update_kt_private);
	struct dn_list *p;
	struct smb_krb5_context *smb_krb5_context;
	int krb5_ret = smb_krb5_init_context(data, ldb_get_event_context(ldb), ldb_get_opaque(ldb, "loadparm"),
					     &smb_krb5_context);
	if (krb5_ret != 0) {
		talloc_free(data->changed_dns);
		data->changed_dns = NULL;
		ldb_asprintf_errstring(ldb, "Failed to setup krb5_context: %s", error_message(krb5_ret));
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ldb = ldb_module_get_ctx(module);

	for (p=data->changed_dns; p; p = p->next) {
		const char *error_string;
		krb5_ret = smb_krb5_update_keytab(data, smb_krb5_context, ldb, p->msg, p->do_delete, &error_string);
		if (krb5_ret != 0) {
			talloc_free(data->changed_dns);
			data->changed_dns = NULL;
			ldb_asprintf_errstring(ldb, "Failed to update keytab from entry %s in %s: %s",
					       ldb_dn_get_linearized(p->msg->dn),
					       (const char *)ldb_get_opaque(ldb, "ldb_url"),
					       error_string);
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
		return ldb_oom(ldb);
	}

	data->changed_dns = NULL;

	ldb_module_set_private(module, data);

	return ldb_next_init(module);
}

static const struct ldb_module_ops ldb_update_keytab_module_ops = {
	.name		   = "update_keytab",
	.init_context	   = update_kt_init,
	.add               = update_kt_add,
	.modify            = update_kt_modify,
	.rename            = update_kt_rename,
	.del               = update_kt_delete,
	.prepare_commit    = update_kt_prepare_commit,
	.del_transaction   = update_kt_del_trans,
};

int ldb_update_keytab_module_init(const char *version)
{
	LDB_MODULE_CHECK_VERSION(version);
	return ldb_register_module(&ldb_update_keytab_module_ops);
}
