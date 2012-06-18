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
#include "torture/torture.h"
#include "torture/rpc/rpc.h"
#include "param/param.h"


#define TEST_GROUPNAME  "libnetgrouptest"


static bool test_cleanup(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx,
			 struct policy_handle *domain_handle, const char *groupname)
{
	NTSTATUS status;
	struct samr_LookupNames r1;
	struct samr_OpenGroup r2;
	struct samr_DeleteDomainGroup r3;
	struct lsa_String names[2];
	uint32_t rid;
	struct policy_handle group_handle;
	struct samr_Ids rids, types;

	names[0].string = groupname;

	r1.in.domain_handle  = domain_handle;
	r1.in.num_names      = 1;
	r1.in.names          = names;
	r1.out.rids          = &rids;
	r1.out.types         = &types;
	
	printf("group account lookup '%s'\n", groupname);

	status = dcerpc_samr_LookupNames(p, mem_ctx, &r1);
	if (!NT_STATUS_IS_OK(status)) {
		printf("LookupNames failed - %s\n", nt_errstr(status));
		return false;
	}

	rid = r1.out.rids->ids[0];
	
	r2.in.domain_handle  = domain_handle;
	r2.in.access_mask    = SEC_FLAG_MAXIMUM_ALLOWED;
	r2.in.rid            = rid;
	r2.out.group_handle  = &group_handle;

	printf("opening group account\n");

	status = dcerpc_samr_OpenGroup(p, mem_ctx, &r2);
	if (!NT_STATUS_IS_OK(status)) {
		printf("OpenGroup failed - %s\n", nt_errstr(status));
		return false;
	}

	r3.in.group_handle  = &group_handle;
	r3.out.group_handle = &group_handle;

	printf("deleting group account\n");
	
	status = dcerpc_samr_DeleteDomainGroup(p, mem_ctx, &r3);
	if (!NT_STATUS_IS_OK(status)) {
		printf("DeleteGroup failed - %s\n", nt_errstr(status));
		return false;
	}

	return true;
}


static bool test_creategroup(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx,
			     struct policy_handle *handle, const char *name)
{
	NTSTATUS status;
	struct lsa_String groupname;
	struct samr_CreateDomainGroup r;
	struct policy_handle group_handle;
	uint32_t group_rid;
	
	groupname.string = name;
	
	r.in.domain_handle  = handle;
	r.in.name           = &groupname;
	r.in.access_mask    = SEC_FLAG_MAXIMUM_ALLOWED;
	r.out.group_handle  = &group_handle;
	r.out.rid           = &group_rid;

	printf("creating group account %s\n", name);

	status = dcerpc_samr_CreateDomainGroup(p, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("CreateGroup failed - %s\n", nt_errstr(status));

		if (NT_STATUS_EQUAL(status, NT_STATUS_GROUP_EXISTS)) {
			printf("Group (%s) already exists - attempting to delete and recreate group again\n", name);
			if (!test_cleanup(p, mem_ctx, handle, TEST_GROUPNAME)) {
				return false;
			}

			printf("creating group account\n");
			
			status = dcerpc_samr_CreateDomainGroup(p, mem_ctx, &r);
			if (!NT_STATUS_IS_OK(status)) {
				printf("CreateGroup failed - %s\n", nt_errstr(status));
				return false;
			}
			return true;
		}
		return false;
	}

	return true;
}


static bool test_opendomain(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx,
			    struct policy_handle *handle, struct lsa_String *domname)
{
	NTSTATUS status;
	struct policy_handle h, domain_handle;
	struct samr_Connect r1;
	struct samr_LookupDomain r2;
	struct dom_sid2 *sid = NULL;
	struct samr_OpenDomain r3;
	
	printf("connecting\n");
	
	r1.in.system_name = 0;
	r1.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r1.out.connect_handle = &h;
	
	status = dcerpc_samr_Connect(p, mem_ctx, &r1);
	if (!NT_STATUS_IS_OK(status)) {
		printf("Connect failed - %s\n", nt_errstr(status));
		return false;
	}
	
	r2.in.connect_handle = &h;
	r2.in.domain_name = domname;
	r2.out.sid = &sid;

	printf("domain lookup on %s\n", domname->string);

	status = dcerpc_samr_LookupDomain(p, mem_ctx, &r2);
	if (!NT_STATUS_IS_OK(status)) {
		printf("LookupDomain failed - %s\n", nt_errstr(status));
		return false;
	}

	r3.in.connect_handle = &h;
	r3.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r3.in.sid = *r2.out.sid;
	r3.out.domain_handle = &domain_handle;

	printf("opening domain\n");

	status = dcerpc_samr_OpenDomain(p, mem_ctx, &r3);
	if (!NT_STATUS_IS_OK(status)) {
		printf("OpenDomain failed - %s\n", nt_errstr(status));
		return false;
	} else {
		*handle = domain_handle;
	}

	return true;
}


static bool test_samr_close(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx,
			    struct policy_handle *domain_handle)
{
	NTSTATUS status;
	struct samr_Close r;
  
	r.in.handle = domain_handle;
	r.out.handle = domain_handle;

	status = dcerpc_samr_Close(p, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("Close samr domain failed - %s\n", nt_errstr(status));
		return false;
	}
	
	return true;
}


static bool test_lsa_close(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx,
			   struct policy_handle *domain_handle)
{
	NTSTATUS status;
	struct lsa_Close r;

	r.in.handle = domain_handle;
	r.out.handle = domain_handle;
	
	status = dcerpc_lsa_Close(p, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("Close lsa domain failed - %s\n", nt_errstr(status));
		return false;
	}

	return true;
}


bool torture_groupinfo_api(struct torture_context *torture)
{
	const char *name = TEST_GROUPNAME;
	bool ret = true;
	NTSTATUS status;
	TALLOC_CTX *mem_ctx = NULL, *prep_mem_ctx;
	struct libnet_context *ctx;
	struct dcerpc_pipe *p;
	struct policy_handle h;
	struct lsa_String domain_name;
	struct libnet_GroupInfo req;

	prep_mem_ctx = talloc_init("prepare torture group info");

	ctx = libnet_context_init(torture->ev, torture->lp_ctx);
	ctx->cred = cmdline_credentials;

	status = torture_rpc_connection(torture,
					&p,
					&ndr_table_samr);
	if (!NT_STATUS_IS_OK(status)) {
		return false;
	}

	domain_name.string = lp_workgroup(torture->lp_ctx);
	if (!test_opendomain(p, prep_mem_ctx, &h, &domain_name)) {
		ret = false;
		goto done;
	}

	if (!test_creategroup(p, prep_mem_ctx, &h, name)) {
		ret = false;
		goto done;
	}

	mem_ctx = talloc_init("torture group info");

	ZERO_STRUCT(req);

	req.in.domain_name = domain_name.string;
	req.in.level = GROUP_INFO_BY_NAME;
	req.in.data.group_name = name;

	status = libnet_GroupInfo(ctx, mem_ctx, &req);
	if (!NT_STATUS_IS_OK(status)) {
		printf("libnet_GroupInfo call failed: %s\n", nt_errstr(status));
		ret = false;
		goto done;
	}

	if (!test_cleanup(ctx->samr.pipe, mem_ctx, &ctx->samr.handle, TEST_GROUPNAME)) {
		printf("cleanup failed\n");
		ret = false;
		goto done;
	}

	if (!test_samr_close(ctx->samr.pipe, mem_ctx, &ctx->samr.handle)) {
		printf("domain close failed\n");
		ret = false;
	}

	talloc_free(ctx);

done:
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

	domain_name.string = lp_workgroup(torture->lp_ctx);
	mem_ctx = talloc_init("torture group list");

	ZERO_STRUCT(req);

	printf("listing group accounts:\n");
	
	do {
		req.in.domain_name  = domain_name.string;
		req.in.page_size    = 128;
		req.in.resume_index = req.out.resume_index;

		status = libnet_GroupList(ctx, mem_ctx, &req);
		if (!NT_STATUS_IS_OK(status) &&
		    !NT_STATUS_EQUAL(status, STATUS_MORE_ENTRIES)) break;

		for (i = 0; i < req.out.count; i++) {
			printf("\tgroup: %s, sid=%s\n",
			       req.out.groups[i].groupname, req.out.groups[i].sid);
		}

	} while (NT_STATUS_EQUAL(status, STATUS_MORE_ENTRIES));

	if (!(NT_STATUS_IS_OK(status) ||
	      NT_STATUS_EQUAL(status, NT_STATUS_NO_MORE_ENTRIES))) {
		printf("libnet_GroupList call failed: %s\n", nt_errstr(status));
		ret = false;
		goto done;
	}

	if (!test_samr_close(ctx->samr.pipe, mem_ctx, &ctx->samr.handle)) {
		printf("domain close failed\n");
		ret = false;
	}

	if (!test_lsa_close(ctx->lsa.pipe, mem_ctx, &ctx->lsa.handle)) {
		printf("lsa domain close failed\n");
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
	req.in.domain_name = lp_workgroup(torture->lp_ctx);
	req.out.error_string = NULL;

	status = libnet_CreateGroup(ctx, mem_ctx, &req);
	if (!NT_STATUS_IS_OK(status)) {
		printf("libnet_CreateGroup call failed: %s\n", nt_errstr(status));
		ret = false;
		goto done;
	}

	if (!test_cleanup(ctx->samr.pipe, mem_ctx, &ctx->samr.handle, TEST_GROUPNAME)) {
		printf("cleanup failed\n");
		ret = false;
		goto done;
	}

	if (!test_samr_close(ctx->samr.pipe, mem_ctx, &ctx->samr.handle)) {
		printf("domain close failed\n");
		ret = false;
	}

done:
	talloc_free(ctx);
	talloc_free(mem_ctx);
	return ret;
}
