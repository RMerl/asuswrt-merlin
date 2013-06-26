/* 
   Unix SMB/CIFS mplementation.
   LDAP schema tests
   
   Copyright (C) Stefan Metzmacher 2006
    
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
#include "libcli/ldap/ldap_client.h"
#include "lib/cmdline/popt_common.h"
#include "ldb_wrap.h"
#include "dsdb/samdb/samdb.h"
#include "../lib/util/dlinklist.h"

#include "torture/torture.h"


struct test_rootDSE {
	const char *defaultdn;
	const char *rootdn;
	const char *configdn;
	const char *schemadn;
};

struct test_schema_ctx {
	struct ldb_context *ldb;

	struct ldb_paged_control *ctrl;
	uint32_t count;
	bool pending;

	int (*callback)(void *, struct ldb_context *ldb, struct ldb_message *);
	void *private_data;
};

static bool test_search_rootDSE(struct ldb_context *ldb, struct test_rootDSE *root)
{
	int ret;
	struct ldb_message *msg;
	struct ldb_result *r;

	d_printf("Testing RootDSE Search\n");

	ret = ldb_search(ldb, ldb, &r, ldb_dn_new(ldb, ldb, NULL),
			 LDB_SCOPE_BASE, NULL, NULL);
	if (ret != LDB_SUCCESS) {
		return false;
	} else if (r->count != 1) {
		talloc_free(r);
		return false;
	}

	msg = r->msgs[0];

	root->defaultdn	= ldb_msg_find_attr_as_string(msg, "defaultNamingContext", NULL);
	talloc_steal(ldb, root->defaultdn);
	root->rootdn	= ldb_msg_find_attr_as_string(msg, "rootDomainNamingContext", NULL);
	talloc_steal(ldb, root->rootdn);
	root->configdn	= ldb_msg_find_attr_as_string(msg, "configurationNamingContext", NULL);
	talloc_steal(ldb, root->configdn);
	root->schemadn	= ldb_msg_find_attr_as_string(msg, "schemaNamingContext", NULL);
	talloc_steal(ldb, root->schemadn);

	talloc_free(r);

	return true;
}

static int test_schema_search_callback(struct ldb_request *req, struct ldb_reply *ares)
{
	struct test_schema_ctx *actx;
	int ret = LDB_SUCCESS;

	actx = talloc_get_type(req->context, struct test_schema_ctx);

	if (!ares) {
		return ldb_request_done(req, LDB_ERR_OPERATIONS_ERROR);
	}
	if (ares->error != LDB_SUCCESS) {
		return ldb_request_done(req, ares->error);
	}

	switch (ares->type) {
	case LDB_REPLY_ENTRY:
		actx->count++;
		ret = actx->callback(actx->private_data, actx->ldb, ares->message);
		break;

	case LDB_REPLY_REFERRAL:
		break;

	case LDB_REPLY_DONE:
		if (ares->controls) {
			struct ldb_paged_control *ctrl = NULL;
			int i;

			for (i=0; ares->controls[i]; i++) {
				if (strcmp(LDB_CONTROL_PAGED_RESULTS_OID, ares->controls[i]->oid) == 0) {
					ctrl = talloc_get_type(ares->controls[i]->data, struct ldb_paged_control);
					break;
				}
			}

			if (!ctrl) break;

			talloc_free(actx->ctrl->cookie);
			actx->ctrl->cookie = talloc_steal(actx->ctrl->cookie, ctrl->cookie);
			actx->ctrl->cookie_len = ctrl->cookie_len;

			if (actx->ctrl->cookie_len > 0) {
				actx->pending = true;
			}
		}
		talloc_free(ares);
		return ldb_request_done(req, LDB_SUCCESS);

	default:
		d_printf("%s: unknown Reply Type %u\n", __location__, ares->type);
		return ldb_request_done(req, LDB_ERR_OTHER);
	}

	if (talloc_free(ares) == -1) {
		d_printf("talloc_free failed\n");
		actx->pending = 0;
		return ldb_request_done(req, LDB_ERR_OPERATIONS_ERROR);
	}

	if (ret) {
		return ldb_request_done(req, LDB_ERR_OPERATIONS_ERROR);
	}

	return LDB_SUCCESS;
}

static bool test_create_schema_type(struct ldb_context *ldb, struct test_rootDSE *root,
				    const char *filter,
				    int (*callback)(void *, struct ldb_context *ldb, struct ldb_message *),
				    void *private_data)
{
	struct ldb_control **ctrl;
	struct ldb_paged_control *control;
	struct ldb_request *req;
	int ret;
	struct test_schema_ctx *actx;

	actx = talloc(ldb, struct test_schema_ctx);
	actx->ldb = ldb;
	actx->private_data = private_data;
	actx->callback= callback;

	ctrl = talloc_array(actx, struct ldb_control *, 2);
	ctrl[0] = talloc(ctrl, struct ldb_control);
	ctrl[0]->oid = LDB_CONTROL_PAGED_RESULTS_OID;
	ctrl[0]->critical = true;
	control = talloc(ctrl[0], struct ldb_paged_control);
	control->size = 1000;
	control->cookie = NULL;
	control->cookie_len = 0;
	ctrl[0]->data = control;
	ctrl[1] = NULL;

	ret = ldb_build_search_req(&req, ldb, actx,
				   ldb_dn_new(actx, ldb, root->schemadn),
				   LDB_SCOPE_SUBTREE,
				   filter, NULL,
				   ctrl,
				   actx, test_schema_search_callback,
				   NULL);

	actx->ctrl = control;
	actx->count = 0;
again:
	actx->pending		= false;

	ret = ldb_request(ldb, req);
	if (ret != LDB_SUCCESS) {
		d_printf("search failed - %s\n", ldb_errstring(ldb));
		talloc_free(actx);
		return false;
	}

	ret = ldb_wait(req->handle, LDB_WAIT_ALL);
       	if (ret != LDB_SUCCESS) {
		d_printf("search error - %s\n", ldb_errstring(ldb));
		talloc_free(actx);
		return false;
	}

	if (actx->pending)
		goto again;

	d_printf("filter[%s] count[%u]\n", filter, actx->count);
	talloc_free(actx);
	return true;
}

static int test_add_attribute(void *ptr, struct ldb_context *ldb, struct ldb_message *msg)
{
	struct dsdb_schema *schema = talloc_get_type(ptr, struct dsdb_schema);
	WERROR status;

	status = dsdb_attribute_from_ldb(ldb, schema, msg);
	if (!W_ERROR_IS_OK(status)) {
		goto failed;
	}

	return LDB_SUCCESS;
failed:
	return LDB_ERR_OTHER;
}

static int test_add_class(void *ptr, struct ldb_context *ldb, struct ldb_message *msg)
{
	struct dsdb_schema *schema = talloc_get_type(ptr, struct dsdb_schema);
	WERROR status;

	status = dsdb_class_from_ldb(schema, msg);
	if (!W_ERROR_IS_OK(status)) {
		goto failed;
	}

	return LDB_SUCCESS;
failed:
	return LDB_ERR_OTHER;
}

static bool test_create_schema(struct ldb_context *ldb, struct test_rootDSE *root, struct dsdb_schema **_schema)
{
	bool ret = true;
	struct dsdb_schema *schema;

	schema = talloc_zero(ldb, struct dsdb_schema);

	d_printf("Fetching attributes...\n");
	ret &= test_create_schema_type(ldb, root, "(objectClass=attributeSchema)",
				       test_add_attribute, schema);
	d_printf("Fetching objectClasses...\n");
	ret &= test_create_schema_type(ldb, root, "(objectClass=classSchema)",
				       test_add_class, schema);

	if (ret == true) {
		*_schema = schema;
	}
	return ret;
}

static bool test_dump_not_replicated(struct ldb_context *ldb, struct test_rootDSE *root, struct dsdb_schema *schema)
{
	struct dsdb_attribute *a;
	uint32_t a_i = 1;

	d_printf("Dumping not replicated attributes\n");

	for (a=schema->attributes; a; a = a->next) {
		if (!(a->systemFlags & 0x00000001)) continue;
		d_printf("attr[%4u]: '%s'\n", a_i++,
			 a->lDAPDisplayName);
	}

	return true;
}

static bool test_dump_partial(struct ldb_context *ldb, struct test_rootDSE *root, struct dsdb_schema *schema)
{
	struct dsdb_attribute *a;
	uint32_t a_i = 1;

	d_printf("Dumping attributes which are provided by the global catalog\n");

	for (a=schema->attributes; a; a = a->next) {
		if (!(a->systemFlags & 0x00000002) && !a->isMemberOfPartialAttributeSet) continue;
		d_printf("attr[%4u]:  %u %u '%s'\n", a_i++,
			 a->systemFlags & 0x00000002, a->isMemberOfPartialAttributeSet,
			 a->lDAPDisplayName);
	}

	return true;
}

static bool test_dump_contructed(struct ldb_context *ldb, struct test_rootDSE *root, struct dsdb_schema *schema)
{
	struct dsdb_attribute *a;
	uint32_t a_i = 1;

	d_printf("Dumping constructed attributes\n");

	for (a=schema->attributes; a; a = a->next) {
		if (!(a->systemFlags & 0x00000004)) continue;
		d_printf("attr[%4u]: '%s'\n", a_i++,
			 a->lDAPDisplayName);
	}

	return true;
}

static bool test_dump_sorted_syntax(struct ldb_context *ldb, struct test_rootDSE *root, struct dsdb_schema *schema)
{
	struct dsdb_attribute *a;
	uint32_t a_i = 1;
	uint32_t i;
	const char *syntaxes[] = {
		"2.5.5.0",
		"2.5.5.1",
		"2.5.5.2",
		"2.5.5.3",
		"2.5.5.4",
		"2.5.5.5",
		"2.5.5.6",
		"2.5.5.7",
		"2.5.5.8",
		"2.5.5.9",
		"2.5.5.10",
		"2.5.5.11",
		"2.5.5.12",
		"2.5.5.13",
		"2.5.5.14",
		"2.5.5.15",
		"2.5.5.16",
		"2.5.5.17"
	};

	d_printf("Dumping attribute syntaxes\n");

	for (i=0; i < ARRAY_SIZE(syntaxes); i++) {
		for (a=schema->attributes; a; a = a->next) {
			char *om_hex;

			if (strcmp(syntaxes[i], a->attributeSyntax_oid) != 0) continue;

			om_hex = data_blob_hex_string_upper(ldb, &a->oMObjectClass);
			if (!om_hex) {
				return false;
			}

			d_printf("attr[%4u]: %s %u '%s' '%s'\n", a_i++,
				 a->attributeSyntax_oid, a->oMSyntax,
				 om_hex, a->lDAPDisplayName);
			talloc_free(om_hex);
		}
	}

	return true;
}

static bool test_dump_not_in_filtered_replica(struct ldb_context *ldb, struct test_rootDSE *root, struct dsdb_schema *schema)
{
	struct dsdb_attribute *a;
	uint32_t a_i = 1;

	d_printf("Dumping attributes not in filtered replica\n");

	for (a=schema->attributes; a; a = a->next) {
		if (!dsdb_attribute_is_attr_in_filtered_replica(a)) {
			d_printf("attr[%4u]: '%s'\n", a_i++,
				 a->lDAPDisplayName);
		}
	}
	return true;
}

bool torture_ldap_schema(struct torture_context *torture)
{
	struct ldb_context *ldb;
	bool ret = true;
	const char *host = torture_setting_string(torture, "host", NULL);
	char *url;
	struct test_rootDSE rootDSE;
	struct dsdb_schema *schema = NULL;

	ZERO_STRUCT(rootDSE);

	url = talloc_asprintf(torture, "ldap://%s/", host);

	ldb = ldb_wrap_connect(torture, torture->ev, torture->lp_ctx, url,
			       NULL,
			       cmdline_credentials,
			       0);
	if (!ldb) goto failed;

	ret &= test_search_rootDSE(ldb, &rootDSE);
	if (!ret) goto failed;
	ret &= test_create_schema(ldb, &rootDSE, &schema);
	if (!ret) goto failed;

	ret &= test_dump_not_replicated(ldb, &rootDSE, schema);
	ret &= test_dump_partial(ldb, &rootDSE, schema);
	ret &= test_dump_contructed(ldb, &rootDSE, schema);
	ret &= test_dump_sorted_syntax(ldb, &rootDSE, schema);
	ret &= test_dump_not_in_filtered_replica(ldb, &rootDSE, schema);

failed:
	return ret;
}
