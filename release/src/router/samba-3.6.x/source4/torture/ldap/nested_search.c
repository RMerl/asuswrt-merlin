/*
   Unix SMB/CIFS implementation.

   BRIEF FILE DESCRIPTION

   Copyright (C) Kamen Mazdrashki <kamen.mazdrashki@postpath.com> 2010

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
#include "ldb.h"
#include "ldb_wrap.h"
#include "lib/cmdline/popt_common.h"
#include "torture/torture.h"

#define torture_assert_res(torture_ctx,expr,cmt,_res) \
	if (!(expr)) { \
		torture_result(torture_ctx, TORTURE_FAIL, __location__": Expression `%s' failed: %s", __STRING(expr), cmt); \
		return _res; \
	}


struct nested_search_context {
	struct torture_context *tctx;
	struct ldb_dn *root_dn;
	struct ldb_context *ldb;
	struct ldb_result *ldb_res;
};

/*
 * ldb_search handler - used to executed a nested
 * ldap search request during LDB_REPLY_ENTRY handling
 */
static int nested_search_callback(struct ldb_request *req,
				  struct ldb_reply *ares)
{
	int i;
	int res;
	struct nested_search_context *sctx;
	struct ldb_result *ldb_res;
	struct ldb_message *ldb_msg;
	static const char *attrs[] = {
		"rootDomainNamingContext",
		"configurationNamingContext",
		"schemaNamingContext",
		"defaultNamingContext",
		NULL
	};

	sctx = talloc_get_type(req->context, struct nested_search_context);

	/* sanity check */
	switch (ares->type) {
	case LDB_REPLY_ENTRY:
		torture_comment(sctx->tctx, "nested_search_callback: LDB_REPLY_ENTRY\n");
		ldb_msg = ares->message;
		torture_assert_res(sctx->tctx, ldb_msg, "ares->message is NULL!", LDB_ERR_OPERATIONS_ERROR);
		torture_assert_res(sctx->tctx, ldb_msg->num_elements, "No elements returned!", LDB_ERR_OPERATIONS_ERROR);
		torture_assert_res(sctx->tctx, ldb_msg->elements, "elements member is NULL!", LDB_ERR_OPERATIONS_ERROR);
		break;
	case LDB_REPLY_DONE:
		torture_comment(sctx->tctx, "nested_search_callback: LDB_REPLY_DONE\n");
		break;
	case LDB_REPLY_REFERRAL:
		torture_comment(sctx->tctx, "nested_search_callback: LDB_REPLY_REFERRAL\n");
		break;
	}

	/* switch context and let default handler do its job */
	req->context = sctx->ldb_res;
	res = ldb_search_default_callback(req, ares);
	req->context = sctx;
	if (res != LDB_SUCCESS) {
		return res;
	}

	/* not a search reply, then get out */
	if (ares->type != LDB_REPLY_ENTRY) {
		return res;
	}


	res = ldb_search(sctx->ldb, sctx, &ldb_res, sctx->root_dn, LDB_SCOPE_BASE, attrs, "(objectClass=*)");
	if (res != LDB_SUCCESS) {
		torture_warning(sctx->tctx,
		                "Search on RootDSE failed in search_entry handler: %s",
		                ldb_errstring(sctx->ldb));
		return LDB_SUCCESS;
	}

	torture_assert_res(sctx->tctx, ldb_res->count == 1, "One message expected here", LDB_ERR_OPERATIONS_ERROR);

	ldb_msg = ldb_res->msgs[0];
	torture_assert_res(sctx->tctx, ldb_msg->num_elements == (ARRAY_SIZE(attrs)-1),
			   "Search returned different number of elts than requested", LDB_ERR_OPERATIONS_ERROR);
	for (i = 0; i < ldb_msg->num_elements; i++) {
		const char *msg;
		struct ldb_message_element *elt1;
		struct ldb_message_element *elt2;

		elt2 = &ldb_msg->elements[i];
		msg = talloc_asprintf(sctx, "Processing element: %s", elt2->name);
		elt1 = ldb_msg_find_element(sctx->ldb_res->msgs[0], elt2->name);
		torture_assert_res(sctx->tctx, elt1, msg, LDB_ERR_OPERATIONS_ERROR);

		/* compare elements */
		torture_assert_res(sctx->tctx, elt2->flags == elt1->flags, "", LDB_ERR_OPERATIONS_ERROR);
		torture_assert_res(sctx->tctx, elt2->num_values == elt1->num_values, "", LDB_ERR_OPERATIONS_ERROR);
	}
	/* TODO: check returned result */

	return LDB_SUCCESS;
}

/**
 * Test nested search execution against RootDSE
 * on remote LDAP server.
 */
bool test_ldap_nested_search(struct torture_context *tctx)
{
	int ret;
	char *url;
	const char *host = torture_setting_string(tctx, "host", NULL);
	struct ldb_request *req;
	struct nested_search_context *sctx;
	static const char *attrs[] = {
/*
		"rootDomainNamingContext",
		"configurationNamingContext",
		"schemaNamingContext",
		"defaultNamingContext",
*/
		"*",
		NULL
	};

	sctx = talloc_zero(tctx, struct nested_search_context);
	torture_assert(tctx, sctx, "Not enough memory");
	sctx->tctx = tctx;

	url = talloc_asprintf(sctx, "ldap://%s/", host);
	if (!url) {
		torture_assert(tctx, url, "Not enough memory");
	}

	torture_comment(tctx, "Connecting to: %s\n", url);
	sctx->ldb = ldb_wrap_connect(sctx, tctx->ev, tctx->lp_ctx, url,
	                             NULL,
	                             cmdline_credentials,
	                             0);
	torture_assert(tctx, sctx->ldb, "Failed to create ldb connection");

	/* prepare context for searching */
	sctx->root_dn = ldb_dn_new(sctx, sctx->ldb, NULL);
	sctx->ldb_res = talloc_zero(sctx, struct ldb_result);

	/* build search request */
	ret = ldb_build_search_req(&req,
	                           sctx->ldb,
	                           sctx,
	                           sctx->root_dn, LDB_SCOPE_BASE,
	                           "(objectClass=*)", attrs, NULL,
	                           sctx, nested_search_callback,
	                           NULL);
	if (ret != LDB_SUCCESS) {
		torture_result(tctx, TORTURE_FAIL,
		               __location__ ": Allocating request failed: %s", ldb_errstring(sctx->ldb));
		return false;
	}

	ret = ldb_request(sctx->ldb, req);
	if (ret != LDB_SUCCESS) {
		torture_result(tctx, TORTURE_FAIL,
		               __location__ ": Search failed: %s", ldb_errstring(sctx->ldb));
		return false;
	}

	ret = ldb_wait(req->handle, LDB_WAIT_ALL);
	if (ret != LDB_SUCCESS) {
		torture_result(tctx, TORTURE_FAIL,
		               __location__ ": Search error: %s", ldb_errstring(sctx->ldb));
		return false;
	}

	/* TODO: check returned result */

	talloc_free(sctx);
	return true;
}

