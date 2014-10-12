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
#include "torture/rpc/torture_rpc.h"
#include "libnet/libnet.h"
#include "libcli/security/security.h"
#include "librpc/gen_ndr/ndr_samr_c.h"
#include "param/param.h"
#include "torture/libnet/proto.h"


#define TEST_USERNAME  "libnetuserinfotest"

static bool test_userinfo(struct torture_context *tctx,
			  struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx,
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

	torture_comment(tctx, "Testing sync libnet_rpc_userinfo (SID argument)\n");
	status = libnet_rpc_userinfo(p, mem_ctx, &user);
	torture_assert_ntstatus_ok(tctx, status, "Calling sync libnet_rpc_userinfo() failed");

	ZERO_STRUCT(user);

	user.in.domain_handle = *domain_handle;
	user.in.sid           = NULL;
	user.in.username      = user_name;
	user.in.level         = level;

	torture_comment(tctx, "Testing sync libnet_rpc_userinfo (username argument)\n");
	status = libnet_rpc_userinfo(p, mem_ctx, &user);
	torture_assert_ntstatus_ok(tctx, status, "Calling sync libnet_rpc_userinfo failed");

	return true;
}


static bool test_userinfo_async(struct torture_context *tctx,
				struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx,
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

	torture_comment(tctx, "Testing async libnet_rpc_userinfo (SID argument)\n");

	c = libnet_rpc_userinfo_send(p, &user, msg_handler);
	torture_assert(tctx, c != NULL, "Failed to call async libnet_rpc_userinfo_send");

	status = libnet_rpc_userinfo_recv(c, mem_ctx, &user);
	torture_assert_ntstatus_ok(tctx, status, "Calling async libnet_rpc_userinfo_recv failed");

	ZERO_STRUCT(user);

	user.in.domain_handle = *domain_handle;
	user.in.sid           = NULL;
	user.in.username      = user_name;
	user.in.level         = level;

	torture_comment(tctx, "Testing async libnet_rpc_userinfo (username argument)\n");

	c = libnet_rpc_userinfo_send(p, &user, msg_handler);
	torture_assert(tctx, c != NULL, "Failed to call async libnet_rpc_userinfo_send");

	status = libnet_rpc_userinfo_recv(c, mem_ctx, &user);
	torture_assert_ntstatus_ok(tctx, status, "Calling async libnet_rpc_userinfo_recv failed");

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
	struct dcerpc_binding_handle *b;

	mem_ctx = talloc_init("test_userinfo");

	status = torture_rpc_connection(torture,
					&p,
					&ndr_table_samr);

	if (!NT_STATUS_IS_OK(status)) {
		return false;
	}
	b = p->binding_handle;

	name.string = lpcfg_workgroup(torture->lp_ctx);

	/*
	 * Testing synchronous version
	 */
	if (!test_domain_open(torture, b, &name, mem_ctx, &h, &sid)) {
		ret = false;
		goto done;
	}

	if (!test_user_create(torture, b, mem_ctx, &h, TEST_USERNAME, &rid)) {
		ret = false;
		goto done;
	}

	if (!test_userinfo(torture, p, mem_ctx, &h, &sid, TEST_USERNAME, &rid)) {
		ret = false;
		goto done;
	}

	if (!test_user_cleanup(torture, b, mem_ctx, &h, TEST_USERNAME)) {
		ret = false;
		goto done;
	}

	/*
	 * Testing asynchronous version and monitor messages
	 */
	if (!test_domain_open(torture, b, &name, mem_ctx, &h, &sid)) {
		ret = false;
		goto done;
	}

	if (!test_user_create(torture, b, mem_ctx, &h, TEST_USERNAME, &rid)) {
		ret = false;
		goto done;
	}

	if (!test_userinfo_async(torture, p, mem_ctx, &h, &sid, TEST_USERNAME, &rid)) {
		ret = false;
		goto done;
	}

	if (!test_user_cleanup(torture, b, mem_ctx, &h, TEST_USERNAME)) {
		ret = false;
		goto done;
	}

done:
	talloc_free(mem_ctx);

	return ret;
}
