/* 
   Unix SMB/CIFS implementation.
   Test suite for libnet calls.

   Copyright (C) Rafal Szczesniak 2005
   
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
#include "torture/rpc/rpc.h"
#include "libnet/libnet.h"
#include "libcli/security/security.h"
#include "librpc/gen_ndr/ndr_samr_c.h"
#include "param/param.h"
#include "torture/libnet/utils.h"


#define TEST_USERNAME  "libnetuserinfotest"

static bool test_userinfo(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx,
			  struct policy_handle *domain_handle,
			  struct dom_sid2 *domain_sid, const char* user_name,
			  uint32_t *rid)
{
	const uint16_t level = 5;
	NTSTATUS status;
	struct libnet_rpc_userinfo user;
	struct dom_sid *user_sid;
	
	user_sid = dom_sid_add_rid(mem_ctx, domain_sid, *rid);
	
	user.in.domain_handle = *domain_handle;
	user.in.sid           = dom_sid_string(mem_ctx, user_sid);
	user.in.level         = level;       /* this should be extended */

	printf("Testing sync libnet_rpc_userinfo (SID argument)\n");
	status = libnet_rpc_userinfo(p, mem_ctx, &user);
	if (!NT_STATUS_IS_OK(status)) {
		printf("Failed to call sync libnet_rpc_userinfo - %s\n", nt_errstr(status));
		return false;
	}

	ZERO_STRUCT(user);

	user.in.domain_handle = *domain_handle;
	user.in.sid           = NULL;
	user.in.username      = TEST_USERNAME;
	user.in.level         = level;

	printf("Testing sync libnet_rpc_userinfo (username argument)\n");
	status = libnet_rpc_userinfo(p, mem_ctx, &user);
	if (!NT_STATUS_IS_OK(status)) {
		printf("Failed to call sync libnet_rpc_userinfo - %s\n", nt_errstr(status));
		return false;
	}

	return true;
}


static bool test_userinfo_async(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx,
				struct policy_handle *domain_handle,
				struct dom_sid2 *domain_sid, const char* user_name,
				uint32_t *rid)
{
	const uint16_t level = 10;
	NTSTATUS status;
	struct composite_context *c;
	struct libnet_rpc_userinfo user;
	struct dom_sid *user_sid;

	user_sid = dom_sid_add_rid(mem_ctx, domain_sid, *rid);

	user.in.domain_handle = *domain_handle;
	user.in.sid           = dom_sid_string(mem_ctx, user_sid);
	user.in.level         = level;       /* this should be extended */

	printf("Testing async libnet_rpc_userinfo (SID argument)\n");

	c = libnet_rpc_userinfo_send(p, &user, msg_handler);
	if (!c) {
		printf("Failed to call sync libnet_rpc_userinfo_send\n");
		return false;
	}

	status = libnet_rpc_userinfo_recv(c, mem_ctx, &user);
	if (!NT_STATUS_IS_OK(status)) {
		printf("Calling async libnet_rpc_userinfo failed - %s\n", nt_errstr(status));
		return false;
	}

	ZERO_STRUCT(user);

	user.in.domain_handle = *domain_handle;
	user.in.sid           = NULL;
	user.in.username      = TEST_USERNAME;
	user.in.level         = level;

	printf("Testing async libnet_rpc_userinfo (username argument)\n");

	c = libnet_rpc_userinfo_send(p, &user, msg_handler);
	if (!c) {
		printf("Failed to call sync libnet_rpc_userinfo_send\n");
		return false;
	}

	status = libnet_rpc_userinfo_recv(c, mem_ctx, &user);
	if (!NT_STATUS_IS_OK(status)) {
		printf("Calling async libnet_rpc_userinfo failed - %s\n", nt_errstr(status));
		return false;
	}

	return true;
}


bool torture_userinfo(struct torture_context *torture)
{
	NTSTATUS status;
	struct dcerpc_pipe *p;
	TALLOC_CTX *mem_ctx;
	bool ret = true;
	struct policy_handle h;
	struct lsa_String name;
	struct dom_sid2 sid;
	uint32_t rid;

	mem_ctx = talloc_init("test_userinfo");

	status = torture_rpc_connection(torture, 
					&p,
					&ndr_table_samr);
	
	if (!NT_STATUS_IS_OK(status)) {
		return false;
	}

	name.string = lp_workgroup(torture->lp_ctx);

	/*
	 * Testing synchronous version
	 */
	if (!test_opendomain(torture, p, mem_ctx, &h, &name, &sid)) {
		ret = false;
		goto done;
	}

	if (!test_user_create(torture, p, mem_ctx, &h, TEST_USERNAME, &rid)) {
		ret = false;
		goto done;
	}

	if (!test_userinfo(p, mem_ctx, &h, &sid, TEST_USERNAME, &rid)) {
		ret = false;
		goto done;
	}

	if (!test_user_cleanup(torture, p, mem_ctx, &h, TEST_USERNAME)) {
		ret = false;
		goto done;
	}

	/*
	 * Testing asynchronous version and monitor messages
	 */
	if (!test_opendomain(torture, p, mem_ctx, &h, &name, &sid)) {
		ret = false;
		goto done;
	}

	if (!test_user_create(torture, p, mem_ctx, &h, TEST_USERNAME, &rid)) {
		ret = false;
		goto done;
	}

	if (!test_userinfo_async(p, mem_ctx, &h, &sid, TEST_USERNAME, &rid)) {
		ret = false;
		goto done;
	}

	if (!test_user_cleanup(torture, p, mem_ctx, &h, TEST_USERNAME)) {
		ret = false;
		goto done;
	}

done:
	talloc_free(mem_ctx);

	return ret;
}
