/*
   Unix SMB/CIFS implementation.

   DsGetReplInfo test. Based on code from dssync.c

   Copyright (C) Erick Nogueira do Nascimento 2009

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
#include "lib/cmdline/popt_common.h"
#include "librpc/gen_ndr/ndr_drsuapi_c.h"
#include "librpc/gen_ndr/ndr_drsblobs.h"
#include "libcli/cldap/cldap.h"
#include "torture/torture.h"
#include "../libcli/drsuapi/drsuapi.h"
#include "auth/gensec/gensec.h"
#include "param/param.h"
#include "dsdb/samdb/samdb.h"
#include "torture/rpc/torture_rpc.h"
#include "torture/drs/proto.h"


struct DsGetinfoBindInfo {
	struct dcerpc_pipe *drs_pipe;
	struct dcerpc_binding_handle *drs_handle;
	struct drsuapi_DsBind req;
	struct GUID bind_guid;
	struct drsuapi_DsBindInfoCtr our_bind_info_ctr;
	struct drsuapi_DsBindInfo28 our_bind_info28;
	struct drsuapi_DsBindInfo28 peer_bind_info28;
	struct policy_handle bind_handle;
};

struct DsGetinfoTest {
	struct dcerpc_binding *drsuapi_binding;

	const char *ldap_url;
	const char *site_name;

	const char *domain_dn;

	/* what we need to do as 'Administrator' */
	struct {
		struct cli_credentials *credentials;
		struct DsGetinfoBindInfo drsuapi;
	} admin;
};



/*
  return the default DN for a ldap server given a connected RPC pipe to the
  server
 */
static const char *torture_get_ldap_base_dn(struct torture_context *tctx, struct dcerpc_pipe *p)
{
	const char *hostname = p->binding->host;
	struct ldb_context *ldb;
	const char *ldap_url = talloc_asprintf(p, "ldap://%s", hostname);
	const char *attrs[] = { "defaultNamingContext", NULL };
	const char *dnstr;
	TALLOC_CTX *tmp_ctx = talloc_new(tctx);
	int ret;
	struct ldb_result *res;

	ldb = ldb_init(tmp_ctx, tctx->ev);
	if (ldb == NULL) {
		talloc_free(tmp_ctx);
		return NULL;
	}

	if (ldb_set_opaque(ldb, "loadparm", tctx->lp_ctx)) {
		talloc_free(ldb);
		return NULL;
	}

	ldb_set_modules_dir(ldb,
		talloc_asprintf(ldb, "%s/ldb", lpcfg_modulesdir(tctx->lp_ctx)));

	ret = ldb_connect(ldb, ldap_url, 0, NULL);
	if (ret != LDB_SUCCESS) {
		torture_comment(tctx, "Failed to make LDB connection to target");
		talloc_free(tmp_ctx);
		return NULL;
	}

	ret = dsdb_search_dn(ldb, tmp_ctx, &res, ldb_dn_new(tmp_ctx, ldb, ""),
			     attrs, 0);
	if (ret != LDB_SUCCESS) {
		torture_comment(tctx, "Failed to get defaultNamingContext");
		talloc_free(tmp_ctx);
		return NULL;
	}

	dnstr = ldb_msg_find_attr_as_string(res->msgs[0], "defaultNamingContext", NULL);
	dnstr = talloc_strdup(tctx, dnstr);
	talloc_free(tmp_ctx);
	return dnstr;
}


static struct DsGetinfoTest *test_create_context(struct torture_context *tctx)
{
	NTSTATUS status;
	struct DsGetinfoTest *ctx;
	struct drsuapi_DsBindInfo28 *our_bind_info28;
	struct drsuapi_DsBindInfoCtr *our_bind_info_ctr;
	const char *binding = torture_setting_string(tctx, "binding", NULL);
	ctx = talloc_zero(tctx, struct DsGetinfoTest);
	if (!ctx) return NULL;

	status = dcerpc_parse_binding(ctx, binding, &ctx->drsuapi_binding);
	if (!NT_STATUS_IS_OK(status)) {
		printf("Bad binding string %s\n", binding);
		return NULL;
	}
	ctx->drsuapi_binding->flags |= DCERPC_SIGN | DCERPC_SEAL;

	/* ctx->admin ...*/
	ctx->admin.credentials				= cmdline_credentials;

	our_bind_info28				= &ctx->admin.drsuapi.our_bind_info28;
	our_bind_info28->supported_extensions	= 0xFFFFFFFF;
	our_bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_ADDENTRYREPLY_V3;
	our_bind_info28->site_guid		= GUID_zero();
	our_bind_info28->pid			= 0;
	our_bind_info28->repl_epoch		= 1;

	our_bind_info_ctr			= &ctx->admin.drsuapi.our_bind_info_ctr;
	our_bind_info_ctr->length		= 28;
	our_bind_info_ctr->info.info28		= *our_bind_info28;

	GUID_from_string(DRSUAPI_DS_BIND_GUID, &ctx->admin.drsuapi.bind_guid);

	ctx->admin.drsuapi.req.in.bind_guid		= &ctx->admin.drsuapi.bind_guid;
	ctx->admin.drsuapi.req.in.bind_info		= our_bind_info_ctr;
	ctx->admin.drsuapi.req.out.bind_handle		= &ctx->admin.drsuapi.bind_handle;

	return ctx;
}

static bool _test_DsBind(struct torture_context *tctx,
			 struct DsGetinfoTest *ctx, struct cli_credentials *credentials, struct DsGetinfoBindInfo *b)
{
	NTSTATUS status;
	bool ret = true;

	status = dcerpc_pipe_connect_b(ctx,
				       &b->drs_pipe, ctx->drsuapi_binding,
				       &ndr_table_drsuapi,
				       credentials, tctx->ev, tctx->lp_ctx);

	if (!NT_STATUS_IS_OK(status)) {
		printf("Failed to connect to server as a BDC: %s\n", nt_errstr(status));
		return false;
	}
	b->drs_handle = b->drs_pipe->binding_handle;

	status = dcerpc_drsuapi_DsBind_r(b->drs_handle, ctx, &b->req);
	if (!NT_STATUS_IS_OK(status)) {
		const char *errstr = nt_errstr(status);
		printf("dcerpc_drsuapi_DsBind failed - %s\n", errstr);
		ret = false;
	} else if (!W_ERROR_IS_OK(b->req.out.result)) {
		printf("DsBind failed - %s\n", win_errstr(b->req.out.result));
		ret = false;
	}

	ZERO_STRUCT(b->peer_bind_info28);
	if (b->req.out.bind_info) {
		switch (b->req.out.bind_info->length) {
		case 24: {
			struct drsuapi_DsBindInfo24 *info24;
			info24 = &b->req.out.bind_info->info.info24;
			b->peer_bind_info28.supported_extensions= info24->supported_extensions;
			b->peer_bind_info28.site_guid		= info24->site_guid;
			b->peer_bind_info28.pid			= info24->pid;
			b->peer_bind_info28.repl_epoch		= 0;
			break;
		}
		case 48: {
			struct drsuapi_DsBindInfo48 *info48;
			info48 = &b->req.out.bind_info->info.info48;
			b->peer_bind_info28.supported_extensions= info48->supported_extensions;
			b->peer_bind_info28.site_guid		= info48->site_guid;
			b->peer_bind_info28.pid			= info48->pid;
			b->peer_bind_info28.repl_epoch		= info48->repl_epoch;
			break;
		}
		case 28:
			b->peer_bind_info28 = b->req.out.bind_info->info.info28;
			break;
		default:
			printf("DsBind - warning: unknown BindInfo length: %u\n",
			       b->req.out.bind_info->length);
		}
	}

	return ret;
}


static bool test_getinfo(struct torture_context *tctx,
			 struct DsGetinfoTest *ctx)
{
	NTSTATUS status;
	struct dcerpc_pipe *p = ctx->admin.drsuapi.drs_pipe;
	struct dcerpc_binding_handle *b = ctx->admin.drsuapi.drs_handle;
	struct drsuapi_DsReplicaGetInfo r;
	union drsuapi_DsReplicaGetInfoRequest req;
	union drsuapi_DsReplicaInfo info;
	enum drsuapi_DsReplicaInfoType info_type;
	int i;
	int invalid_levels = 0;
	struct {
		int32_t level;
		int32_t infotype;
		const char *obj_dn;
		const char *attribute_name;
		uint32_t flags;
	} array[] = {
		{
			.level = DRSUAPI_DS_REPLICA_GET_INFO,
			.infotype = DRSUAPI_DS_REPLICA_INFO_NEIGHBORS
		},{
			.level = DRSUAPI_DS_REPLICA_GET_INFO,
			.infotype = DRSUAPI_DS_REPLICA_INFO_CURSORS
		},{
			.level = DRSUAPI_DS_REPLICA_GET_INFO,
			.infotype = DRSUAPI_DS_REPLICA_INFO_OBJ_METADATA
		},{
			.level = DRSUAPI_DS_REPLICA_GET_INFO,
			.infotype = DRSUAPI_DS_REPLICA_INFO_KCC_DSA_CONNECT_FAILURES
		},{
			.level = DRSUAPI_DS_REPLICA_GET_INFO,
			.infotype = DRSUAPI_DS_REPLICA_INFO_KCC_DSA_LINK_FAILURES
		},{
			.level = DRSUAPI_DS_REPLICA_GET_INFO,
			.infotype = DRSUAPI_DS_REPLICA_INFO_PENDING_OPS
		},{
			.level = DRSUAPI_DS_REPLICA_GET_INFO2,
			.infotype = DRSUAPI_DS_REPLICA_INFO_ATTRIBUTE_VALUE_METADATA
		},{
			.level = DRSUAPI_DS_REPLICA_GET_INFO2,
			.infotype = DRSUAPI_DS_REPLICA_INFO_CURSORS2
		},{
			.level = DRSUAPI_DS_REPLICA_GET_INFO2,
			.infotype = DRSUAPI_DS_REPLICA_INFO_CURSORS3
		},{
			.level = DRSUAPI_DS_REPLICA_GET_INFO2,
			.infotype = DRSUAPI_DS_REPLICA_INFO_OBJ_METADATA2,
			.obj_dn = "CN=Domain Admins,CN=Users,",
			.flags = 0
		},{
			.level = DRSUAPI_DS_REPLICA_GET_INFO2,
			.infotype = DRSUAPI_DS_REPLICA_INFO_OBJ_METADATA2,
			.obj_dn = "CN=Domain Admins,CN=Users,",
			.flags = DRSUAPI_DS_LINKED_ATTRIBUTE_FLAG_ACTIVE
		},{
			.level = DRSUAPI_DS_REPLICA_GET_INFO2,
			.infotype = DRSUAPI_DS_REPLICA_INFO_ATTRIBUTE_VALUE_METADATA2
		},{
			.level = DRSUAPI_DS_REPLICA_GET_INFO2,
			.infotype = DRSUAPI_DS_REPLICA_INFO_REPSTO
		},{
			.level = DRSUAPI_DS_REPLICA_GET_INFO2,
			.infotype = DRSUAPI_DS_REPLICA_INFO_CLIENT_CONTEXTS
		},{
			.level = DRSUAPI_DS_REPLICA_GET_INFO2,
			.infotype = DRSUAPI_DS_REPLICA_INFO_UPTODATE_VECTOR_V1
		},{
			.level = DRSUAPI_DS_REPLICA_GET_INFO2,
			.infotype = DRSUAPI_DS_REPLICA_INFO_SERVER_OUTGOING_CALLS
		}
	};

	ctx->domain_dn = torture_get_ldap_base_dn(tctx, p);
	torture_assert(tctx, ctx->domain_dn != NULL, "Cannot get domain_dn");

	if (torture_setting_bool(tctx, "samba4", false)) {
		torture_comment(tctx, "skipping DsReplicaGetInfo test against Samba4\n");
		return true;
	}

	r.in.bind_handle	= &ctx->admin.drsuapi.bind_handle;
	r.in.req		= &req;

	for (i=0; i < ARRAY_SIZE(array); i++) {
		const char *object_dn;

		torture_comment(tctx, "Testing DsReplicaGetInfo level %d infotype %d\n",
				array[i].level, array[i].infotype);

		if (array[i].obj_dn) {
			object_dn = array[i].obj_dn;
			if (object_dn[strlen(object_dn)-1] == ',') {
				/* add the domain DN on the end */
				object_dn = talloc_asprintf(tctx, "%s%s", object_dn, ctx->domain_dn);
			}
		} else {
			object_dn = ctx->domain_dn;
		}

		r.in.level = array[i].level;
		switch(r.in.level) {
		case DRSUAPI_DS_REPLICA_GET_INFO:
			r.in.req->req1.info_type	= array[i].infotype;
			r.in.req->req1.object_dn	= object_dn;
			ZERO_STRUCT(r.in.req->req1.source_dsa_guid);
			break;
		case DRSUAPI_DS_REPLICA_GET_INFO2:
			r.in.req->req2.info_type	= array[i].infotype;
			r.in.req->req2.object_dn	= object_dn;
			ZERO_STRUCT(r.in.req->req2.source_dsa_guid);
			r.in.req->req2.flags		= 0;
			r.in.req->req2.attribute_name	= NULL;
			r.in.req->req2.value_dn_str	= NULL;
			r.in.req->req2.enumeration_context = 0;
			break;
		}

		/* Construct a different request for some of the infoTypes */
		if (array[i].attribute_name != NULL) {
			r.in.req->req2.attribute_name = array[i].attribute_name;
		}
		if (array[i].flags != 0) {
			r.in.req->req2.flags |= array[i].flags;
		}

		r.out.info		= &info;
		r.out.info_type		= &info_type;

		status = dcerpc_drsuapi_DsReplicaGetInfo_r(b, tctx, &r);
		if (NT_STATUS_EQUAL(status, NT_STATUS_RPC_ENUM_VALUE_OUT_OF_RANGE)) {
			torture_comment(tctx,
					"DsReplicaGetInfo level %d and/or infotype %d not supported by server\n",
					array[i].level, array[i].infotype);
			continue;
		}
		torture_assert_ntstatus_ok(tctx, status, talloc_asprintf(tctx,
					"DsReplicaGetInfo level %d and/or infotype %d failed\n",
					array[i].level, array[i].infotype));
		if (W_ERROR_EQUAL(r.out.result, WERR_INVALID_LEVEL)) {
			/* this is a not yet supported level */
			torture_comment(tctx,
					"DsReplicaGetInfo level %d and/or infotype %d not yet supported by server\n",
					array[i].level, array[i].infotype);
			invalid_levels++;
			continue;
		}

		torture_drsuapi_assert_call(tctx, p, status, &r, "dcerpc_drsuapi_DsReplicaGetInfo");
	}

	if (invalid_levels > 0) {
		return false;
	}

	return true;
}

/**
 * DSGETINFO test case setup
 */
static bool torture_dsgetinfo_tcase_setup(struct torture_context *tctx, void **data)
{
	bool bret;
	struct DsGetinfoTest *ctx;

	*data = ctx = test_create_context(tctx);
	torture_assert(tctx, ctx, "test_create_context() failed");

	bret = _test_DsBind(tctx, ctx, ctx->admin.credentials, &ctx->admin.drsuapi);
	torture_assert(tctx, bret, "_test_DsBind() failed");

	return true;
}

/**
 * DSGETINFO test case cleanup
 */
static bool torture_dsgetinfo_tcase_teardown(struct torture_context *tctx, void *data)
{
	struct DsGetinfoTest *ctx;
	struct drsuapi_DsUnbind r;
	struct policy_handle bind_handle;

	ctx = talloc_get_type(data, struct DsGetinfoTest);

	ZERO_STRUCT(r);
	r.out.bind_handle = &bind_handle;

	/* Unbing admin handle */
	r.in.bind_handle = &ctx->admin.drsuapi.bind_handle;
	dcerpc_drsuapi_DsUnbind_r(ctx->admin.drsuapi.drs_handle, ctx, &r);

	talloc_free(ctx);

	return true;
}

/**
 * DSGETINFO test case implementation
 */
void torture_drs_rpc_dsgetinfo_tcase(struct torture_suite *suite)
{
	typedef bool (*run_func) (struct torture_context *test, void *tcase_data);

	struct torture_test *test;
	struct torture_tcase *tcase = torture_suite_add_tcase(suite, "dsgetinfo");

	torture_tcase_set_fixture(tcase,
				  torture_dsgetinfo_tcase_setup,
				  torture_dsgetinfo_tcase_teardown);

	test = torture_tcase_add_simple_test(tcase, "DsGetReplicaInfo", (run_func)test_getinfo);
}

