/*
   Unix SMB/CIFS implementation.
   Test suite for libnet calls.

   Copyright (C) Rafal Szczesniak  2007

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
#include "libnet/libnet.h"
#include "librpc/gen_ndr/ndr_samr_c.h"
#include "librpc/gen_ndr/ndr_lsa_c.h"
#include "torture/rpc/torture_rpc.h"
#include "torture/libnet/proto.h"
#include "param/param.h"


#define TEST_GROUPNAME  "libnetgrouptest"


bool torture_groupinfo_api(struct torture_context *torture)
{
	const char *name = TEST_GROUPNAME;
	bool ret = true;
	NTSTATUS status;
	TALLOC_CTX *mem_ctx = NULL, *prep_mem_ctx;
	struct libnet_context *ctx = NULL;
	struct dcerpc_pipe *p;
	struct policy_handle h;
	struct lsa_String domain_name;
	struct libnet_GroupInfo req;

	prep_mem_ctx = talloc_init("prepare torture group info");

	status = torture_rpc_connection(torture,
					&p,
					&ndr_table_samr);
	if (!NT_STATUS_IS_OK(status)) {
		return false;
	}

	domain_name.string = lpcfg_workgroup(torture->lp_ctx);
	if (!test_domain_open(torture, p->binding_handle, &domain_name, prep_mem_ctx, &h, NULL)) {
		ret = false;
		goto done;
	}

	if (!test_group_create(torture, p->binding_handle, prep_mem_ctx, &h, name, NULL)) {
		ret = false;
		goto done;
	}

	mem_ctx = talloc_init("torture group info");

	if (!test_libnet_context_init(torture, true, &ctx)) {
		return false;
	}

	ZERO_STRUCT(req);

	req.in.domain_name = domain_name.string;
	req.in.level = GROUP_INFO_BY_NAME;
	req.in.data.group_name = name;

	status = libnet_GroupInfo(ctx, mem_ctx, &req);
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(torture, "libnet_GroupInfo call failed: %s\n", nt_errstr(status));
		ret = false;
		goto done;
	}

	if (!test_group_cleanup(torture, ctx->samr.pipe->binding_handle,
	                        mem_ctx, &ctx->samr.handle, TEST_GROUPNAME)) {
		torture_comment(torture, "cleanup failed\n");
		ret = false;
		goto done;
	}

	if (!test_samr_close_handle(torture,
	                            ctx->samr.pipe->binding_handle, mem_ctx, &ctx->samr.handle)) {
		torture_comment(torture, "domain close failed\n");
		ret = false;
	}

done:
	talloc_free(ctx);
	talloc_free(mem_ctx);
	return ret;
}


bool torture_grouplist(struct torture_context *torture)
{
	bool ret = true;
	NTSTATUS status;
	TALLOC_CTX *mem_ctx = NULL;
	struct libnet_context *ctx;
	struct lsa_String domain_name;
	struct libnet_GroupList req;
	int i;

	ctx = libnet_context_init(torture->ev, torture->lp_ctx);
	ctx->cred = cmdline_credentials;

	domain_name.string = lpcfg_workgroup(torture->lp_ctx);
	mem_ctx = talloc_init("torture group list");

	ZERO_STRUCT(req);

	torture_comment(torture, "listing group accounts:\n");

	do {
		req.in.domain_name  = domain_name.string;
		req.in.page_size    = 128;
		req.in.resume_index = req.out.resume_index;

		status = libnet_GroupList(ctx, mem_ctx, &req);
		if (!NT_STATUS_IS_OK(status) &&
		    !NT_STATUS_EQUAL(status, STATUS_MORE_ENTRIES)) break;

		for (i = 0; i < req.out.count; i++) {
			torture_comment(torture, "\tgroup: %s, sid=%s\n",
			                req.out.groups[i].groupname, req.out.groups[i].sid);
		}

	} while (NT_STATUS_EQUAL(status, STATUS_MORE_ENTRIES));

	if (!(NT_STATUS_IS_OK(status) ||
	      NT_STATUS_EQUAL(status, NT_STATUS_NO_MORE_ENTRIES))) {
		torture_comment(torture, "libnet_GroupList call failed: %s\n", nt_errstr(status));
		ret = false;
		goto done;
	}

	if (!test_samr_close_handle(torture,
	                            ctx->samr.pipe->binding_handle, mem_ctx, &ctx->samr.handle)) {
		torture_comment(torture, "domain close failed\n");
		ret = false;
	}

	if (!test_lsa_close_handle(torture,
	                           ctx->lsa.pipe->binding_handle, mem_ctx, &ctx->lsa.handle)) {
		torture_comment(torture, "lsa domain close failed\n");
		ret = false;
	}

	talloc_free(ctx);

done:
	talloc_free(mem_ctx);
	return ret;
}


bool torture_creategroup(struct torture_context *torture)
{
	bool ret = true;
	NTSTATUS status;
	TALLOC_CTX *mem_ctx = NULL;
	struct libnet_context *ctx;
	struct libnet_CreateGroup req;

	mem_ctx = talloc_init("test_creategroup");

	ctx = libnet_context_init(torture->ev, torture->lp_ctx);
	ctx->cred = cmdline_credentials;

	req.in.group_name = TEST_GROUPNAME;
	req.in.domain_name = lpcfg_workgroup(torture->lp_ctx);
	req.out.error_string = NULL;

	status = libnet_CreateGroup(ctx, mem_ctx, &req);
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(torture, "libnet_CreateGroup call failed: %s\n", nt_errstr(status));
		ret = false;
		goto done;
	}

	if (!test_group_cleanup(torture, ctx->samr.pipe->binding_handle,
	                        mem_ctx, &ctx->samr.handle, TEST_GROUPNAME)) {
		torture_comment(torture, "cleanup failed\n");
		ret = false;
		goto done;
	}

	if (!test_samr_close_handle(torture,
	                            ctx->samr.pipe->binding_handle, mem_ctx, &ctx->samr.handle)) {
		torture_comment(torture, "domain close failed\n");
		ret = false;
	}

done:
	talloc_free(ctx);
	talloc_free(mem_ctx);
	return ret;
}
