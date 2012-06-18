/* 
   Unix SMB/CIFS implementation.
   Test suite for libnet calls.

   Copyright (C) Rafal Szczesniak 2007
   
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
 * These are more general use functions shared among the tests.
 */

#include "includes.h"
#include "torture/rpc/rpc.h"
#include "libnet/libnet.h"
#include "librpc/gen_ndr/ndr_samr_c.h"
#include "torture/libnet/utils.h"


bool test_opendomain(struct torture_context *tctx, 
		     struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx,
		     struct policy_handle *handle, struct lsa_String *domname,
		     struct dom_sid2 *sid_p)
{
	NTSTATUS status;
	struct policy_handle h, domain_handle;
	struct samr_Connect r1;
	struct samr_LookupDomain r2;
	struct dom_sid2 *sid = NULL;
	struct samr_OpenDomain r3;
	
	torture_comment(tctx, "connecting\n");
	
	r1.in.system_name = 0;
	r1.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r1.out.connect_handle = &h;
	
	status = dcerpc_samr_Connect(p, mem_ctx, &r1);
	torture_assert_ntstatus_ok(tctx, status, "Connect failed");
	
	r2.in.connect_handle = &h;
	r2.in.domain_name = domname;
	r2.out.sid = &sid;

	torture_comment(tctx, "domain lookup on %s\n", domname->string);

	status = dcerpc_samr_LookupDomain(p, mem_ctx, &r2);
	torture_assert_ntstatus_ok(tctx, status, "LookupDomain failed");

	r3.in.connect_handle = &h;
	r3.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r3.in.sid = *r2.out.sid;
	r3.out.domain_handle = &domain_handle;

	torture_comment(tctx, "opening domain\n");

	status = dcerpc_samr_OpenDomain(p, mem_ctx, &r3);
	torture_assert_ntstatus_ok(tctx, status, "OpenDomain failed");
	*handle = domain_handle;

	*sid_p = **r2.out.sid;
	return true;
}


bool test_user_cleanup(struct torture_context *tctx, struct dcerpc_pipe *p, 
		       TALLOC_CTX *mem_ctx, struct policy_handle *domain_handle,
		       const char *name)
{
	NTSTATUS status;
	struct samr_LookupNames r1;
	struct samr_OpenUser r2;
	struct samr_DeleteUser r3;
	struct lsa_String names[2];
	uint32_t rid;
	struct policy_handle user_handle;
	struct samr_Ids rids, types;

	names[0].string = name;

	r1.in.domain_handle  = domain_handle;
	r1.in.num_names      = 1;
	r1.in.names          = names;
	r1.out.rids          = &rids;
	r1.out.types         = &types;
	
	torture_comment(tctx, "user account lookup '%s'\n", name);

	status = dcerpc_samr_LookupNames(p, mem_ctx, &r1);
	torture_assert_ntstatus_ok(tctx, status, "LookupNames failed");

	rid = r1.out.rids->ids[0];
	
	r2.in.domain_handle  = domain_handle;
	r2.in.access_mask    = SEC_FLAG_MAXIMUM_ALLOWED;
	r2.in.rid            = rid;
	r2.out.user_handle   = &user_handle;

	torture_comment(tctx, "opening user account\n");

	status = dcerpc_samr_OpenUser(p, mem_ctx, &r2);
	torture_assert_ntstatus_ok(tctx, status, "OpenUser failed");

	r3.in.user_handle  = &user_handle;
	r3.out.user_handle = &user_handle;

	torture_comment(tctx, "deleting user account\n");
	
	status = dcerpc_samr_DeleteUser(p, mem_ctx, &r3);
	torture_assert_ntstatus_ok(tctx, status, "DeleteUser failed");
	
	return true;
}


bool test_user_create(struct torture_context *tctx, 
		      struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx,
		      struct policy_handle *handle, const char *name,
		      uint32_t *rid)
{
	NTSTATUS status;
	struct lsa_String username;
	struct samr_CreateUser r;
	struct policy_handle user_handle;
	
	username.string = name;
	
	r.in.domain_handle = handle;
	r.in.account_name  = &username;
	r.in.access_mask   = SEC_FLAG_MAXIMUM_ALLOWED;
	r.out.user_handle  = &user_handle;
	r.out.rid          = rid;

	torture_comment(tctx, "creating user account %s\n", name);

	status = dcerpc_samr_CreateUser(p, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("CreateUser failed - %s\n", nt_errstr(status));

		if (NT_STATUS_EQUAL(status, NT_STATUS_USER_EXISTS)) {
			torture_comment(tctx, "User (%s) already exists - attempting to delete and recreate account again\n", name);
			if (!test_user_cleanup(tctx, p, mem_ctx, handle, name)) {
				return false;
			}

			torture_comment(tctx, "creating user account\n");
			
			status = dcerpc_samr_CreateUser(p, mem_ctx, &r);
			torture_assert_ntstatus_ok(tctx, status, "CreateUser failed");
			return true;
		}
		return false;
	}

	return true;
}


bool test_group_cleanup(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx,
			struct policy_handle *domain_handle,
			const char *name)
{
	NTSTATUS status;
	struct samr_LookupNames r1;
	struct samr_OpenGroup r2;
	struct samr_DeleteDomainGroup r3;
	struct lsa_String names[2];
	uint32_t rid;
	struct policy_handle group_handle;
	struct samr_Ids rids, types;

	names[0].string = name;

	r1.in.domain_handle  = domain_handle;
	r1.in.num_names      = 1;
	r1.in.names          = names;
	r1.out.rids          = &rids;
	r1.out.types         = &types;
	
	printf("group account lookup '%s'\n", name);

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


bool test_group_create(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx,
		       struct policy_handle *handle, const char *name,
		       uint32_t *rid)
{
	NTSTATUS status;
	struct lsa_String groupname;
	struct samr_CreateDomainGroup r;
	struct policy_handle group_handle;
	
	groupname.string = name;
	
	r.in.domain_handle  = handle;
	r.in.name           = &groupname;
	r.in.access_mask    = SEC_FLAG_MAXIMUM_ALLOWED;
	r.out.group_handle  = &group_handle;
	r.out.rid           = rid;

	printf("creating group account %s\n", name);

	status = dcerpc_samr_CreateDomainGroup(p, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("CreateGroup failed - %s\n", nt_errstr(status));

		if (NT_STATUS_EQUAL(status, NT_STATUS_USER_EXISTS)) {
			printf("Group (%s) already exists - attempting to delete and recreate account again\n", name);
			if (!test_group_cleanup(p, mem_ctx, handle, name)) {
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


void msg_handler(struct monitor_msg *m)
{
	struct msg_rpc_open_user *msg_open;
	struct msg_rpc_query_user *msg_query;
	struct msg_rpc_close_user *msg_close;
	struct msg_rpc_create_user *msg_create;

	switch (m->type) {
	case mon_SamrOpenUser:
		msg_open = (struct msg_rpc_open_user*)m->data;
		printf("monitor_msg: user opened (rid=%d, access_mask=0x%08x)\n",
		       msg_open->rid, msg_open->access_mask);
		break;
	case mon_SamrQueryUser:
		msg_query = (struct msg_rpc_query_user*)m->data;
		printf("monitor_msg: user queried (level=%d)\n", msg_query->level);
		break;
	case mon_SamrCloseUser:
		msg_close = (struct msg_rpc_close_user*)m->data;
		printf("monitor_msg: user closed (rid=%d)\n", msg_close->rid);
		break;
	case mon_SamrCreateUser:
		msg_create = (struct msg_rpc_create_user*)m->data;
		printf("monitor_msg: user created (rid=%d)\n", msg_create->rid);
		break;
	}
}
