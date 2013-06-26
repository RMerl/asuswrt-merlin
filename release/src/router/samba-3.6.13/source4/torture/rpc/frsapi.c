/*
   Unix SMB/CIFS implementation.
   test suite for rpc frsapi operations

   Copyright (C) Guenther Deschner 2007

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "includes.h"
#include "torture/rpc/torture_rpc.h"
#include "librpc/gen_ndr/ndr_frsapi_c.h"
#include "param/param.h"

static bool test_GetDsPollingIntervalW(struct torture_context *tctx,
				       struct dcerpc_binding_handle *b,
				       uint32_t *CurrentInterval,
				       uint32_t *DsPollingLongInterval,
				       uint32_t *DsPollingShortInterval)
{
	struct frsapi_GetDsPollingIntervalW r;

	ZERO_STRUCT(r);

	r.out.CurrentInterval = CurrentInterval;
	r.out.DsPollingLongInterval = DsPollingLongInterval;
	r.out.DsPollingShortInterval = DsPollingShortInterval;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_frsapi_GetDsPollingIntervalW_r(b, tctx, &r),
		"GetDsPollingIntervalW failed");

	torture_assert_werr_ok(tctx, r.out.result,
			       "GetDsPollingIntervalW failed");

	return true;
}

static bool test_SetDsPollingIntervalW(struct torture_context *tctx,
				       struct dcerpc_binding_handle *b,
				       uint32_t CurrentInterval,
				       uint32_t DsPollingLongInterval,
				       uint32_t DsPollingShortInterval)
{
	struct frsapi_SetDsPollingIntervalW r;

	ZERO_STRUCT(r);

	r.in.CurrentInterval = CurrentInterval;
	r.in.DsPollingLongInterval = DsPollingLongInterval;
	r.in.DsPollingShortInterval = DsPollingShortInterval;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_frsapi_SetDsPollingIntervalW_r(b, tctx, &r),
		"SetDsPollingIntervalW failed");

	torture_assert_werr_ok(tctx, r.out.result,
			       "SetDsPollingIntervalW failed");

	return true;
}

static bool test_DsPollingIntervalW(struct torture_context *tctx,
				    struct dcerpc_pipe *p)
{
	struct dcerpc_binding_handle *b = p->binding_handle;
	uint32_t i1, i2, i3;
	uint32_t k1, k2, k3;

	if (!test_GetDsPollingIntervalW(tctx, b, &i1, &i2, &i3)) {
		return false;
	}

	if (!test_SetDsPollingIntervalW(tctx, b, i1, i2, i3)) {
		return false;
	}

	k1 = i1;
	k2 = k3 = 0;

	if (!test_SetDsPollingIntervalW(tctx, b, k1, k2, k3)) {
		return false;
	}

	if (!test_GetDsPollingIntervalW(tctx, b, &k1, &k2, &k3)) {
		return false;
	}

	if ((i1 != k1) || (i2 != k2) || (i3 != k3)) {
		return false;
	}

	return true;
}

static bool test_IsPathReplicated_err(struct torture_context *tctx,
				      struct dcerpc_binding_handle *b,
				      const char *path,
				      uint32_t type,
				      WERROR werr)
{
	struct frsapi_IsPathReplicated r;
	struct GUID guid;
	uint32_t replicated, primary, root;

	ZERO_STRUCT(r);

	r.in.path = path;
	r.in.replica_set_type = type;
	r.out.replicated = &replicated;
	r.out.primary = &primary;
	r.out.root = &root;
	r.out.replica_set_guid = &guid;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_frsapi_IsPathReplicated_r(b, tctx, &r),
		"IsPathReplicated failed");

	torture_assert_werr_equal(tctx, r.out.result, werr,
			          "GetDsPollingIntervalW failed");

	return true;
}

static bool _test_IsPathReplicated(struct torture_context *tctx,
				  struct dcerpc_binding_handle *b,
				  const char *path,
				  uint32_t type)
{
	return test_IsPathReplicated_err(tctx, b, path, type, WERR_OK);
}

static bool test_IsPathReplicated(struct torture_context *tctx,
				  struct dcerpc_pipe *p)
{
	struct dcerpc_binding_handle *b = p->binding_handle;
	const uint32_t lvls[] = {
		FRSAPI_REPLICA_SET_TYPE_0,
		FRSAPI_REPLICA_SET_TYPE_DOMAIN,
		FRSAPI_REPLICA_SET_TYPE_DFS };
	int i;
	bool ret = true;

	if (!test_IsPathReplicated_err(tctx, b, NULL, 0,
				       WERR_FRS_INVALID_SERVICE_PARAMETER)) {
		ret = false;
	}

	for (i=0; i<ARRAY_SIZE(lvls); i++) {
		if (!_test_IsPathReplicated(tctx, b, dcerpc_server_name(p),
					    lvls[i])) {
			ret = false;
		}
	}

	for (i=0; i<ARRAY_SIZE(lvls); i++) {
		const char *path = talloc_asprintf(tctx, "\\\\%s\\SYSVOL",
						   dcerpc_server_name(p));
		if (!_test_IsPathReplicated(tctx, b, path, lvls[i])) {
			ret = false;
		}
	}

	for (i=0; i<ARRAY_SIZE(lvls); i++) {
		if (!_test_IsPathReplicated(tctx, b,
					    "C:\\windows\\sysvol\\domain",
					    lvls[i])) {
			ret = false;
		}
	}

	return ret;
}

static bool test_ForceReplication(struct torture_context *tctx,
				  struct dcerpc_pipe *p)
{
	struct dcerpc_binding_handle *b = p->binding_handle;
	struct frsapi_ForceReplication r;

	ZERO_STRUCT(r);

	r.in.replica_set_guid = NULL;
	r.in.connection_guid = NULL;
	r.in.replica_set_name = lpcfg_dnsdomain(tctx->lp_ctx);
	r.in.partner_dns_name = dcerpc_server_name(p);

	torture_assert_ntstatus_ok(tctx,
		dcerpc_frsapi_ForceReplication_r(b, tctx, &r),
		"ForceReplication failed");

	torture_assert_werr_ok(tctx, r.out.result,
			       "ForceReplication failed");

	return true;
}

static bool test_InfoW(struct torture_context *tctx,
		       struct dcerpc_pipe *p)
{
	struct dcerpc_binding_handle *b = p->binding_handle;
	int i;

	for (i=0; i<10; i++) {

		struct frsapi_InfoW r;
		struct frsapi_Info *info;
		int d;
		DATA_BLOB blob;

		ZERO_STRUCT(r);

		info = talloc_zero(tctx, struct frsapi_Info);

		r.in.length = 0x1000;
		r.in.info = r.out.info = info;

		info->length = r.in.length;
		info->length2 = r.in.length;
		info->level = i;
		info->offset = 0x2c;
		info->blob_len = 0x2c;

		torture_assert_ntstatus_ok(tctx,
			dcerpc_frsapi_InfoW_r(b, tctx, &r),
			"InfoW failed");

		torture_assert_werr_ok(tctx, r.out.result, "InfoW failed");

		/* display the formatted blob text */
		blob = r.out.info->blob;
		for (d = 0; d < blob.length; d++) {
			if (blob.data[d]) {
				printf("%c", blob.data[d]);
			}
		}
		printf("\n");
	}

	return true;
}

struct torture_suite *torture_rpc_frsapi(TALLOC_CTX *mem_ctx)
{
	struct torture_rpc_tcase *tcase;
	struct torture_suite *suite = torture_suite_create(mem_ctx, "frsapi");
	struct torture_test *test;

	tcase = torture_suite_add_rpc_iface_tcase(suite, "frsapi",
						  &ndr_table_frsapi);

	test = torture_rpc_tcase_add_test(tcase, "DsPollingIntervalW",
					  test_DsPollingIntervalW);

	test = torture_rpc_tcase_add_test(tcase, "IsPathReplicated",
					  test_IsPathReplicated);

	test = torture_rpc_tcase_add_test(tcase, "ForceReplication",
					  test_ForceReplication);

	test = torture_rpc_tcase_add_test(tcase, "InfoW",
					  test_InfoW);
	return suite;
}
