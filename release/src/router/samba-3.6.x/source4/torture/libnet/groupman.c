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

#include "includes.h"
#include "torture/rpc/torture_rpc.h"
#include "torture/libnet/grouptest.h"
#include "libnet/libnet.h"
#include "librpc/gen_ndr/ndr_samr_c.h"
#include "param/param.h"
#include "torture/libnet/proto.h"


static bool test_groupadd(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx,
			  struct policy_handle *domain_handle,
			  const char *name)
{
	NTSTATUS status;
	bool ret = true;
	struct libnet_rpc_groupadd group;

	group.in.domain_handle = *domain_handle;
	group.in.groupname     = name;

	printf("Testing libnet_rpc_groupadd\n");

	status = libnet_rpc_groupadd(p, mem_ctx, &group);
	if (!NT_STATUS_IS_OK(status)) {
		printf("Failed to call sync libnet_rpc_groupadd - %s\n", nt_errstr(status));
		return false;
	}

	return ret;
}


bool torture_groupadd(struct torture_context *torture)
{
	NTSTATUS status;
	struct dcerpc_pipe *p;
	struct policy_handle h;
	struct lsa_String domain_name;
	struct dom_sid2 sid;
	const char *name = TEST_GROUPNAME;
	TALLOC_CTX *mem_ctx;
	bool ret = true;
	struct dcerpc_binding_handle *b;

	mem_ctx = talloc_init("test_groupadd");

	status = torture_rpc_connection(torture,
					&p,
					&ndr_table_samr);

	torture_assert_ntstatus_ok(torture, status, "RPC connection");
	b = p->binding_handle;

	domain_name.string = lpcfg_workgroup(torture->lp_ctx);
	if (!test_domain_open(torture, b, &domain_name, mem_ctx, &h, &sid)) {
		ret = false;
		goto done;
	}

	if (!test_groupadd(p, mem_ctx, &h, name)) {
		ret = false;
		goto done;
	}

	if (!test_group_cleanup(torture, b, mem_ctx, &h, name)) {
		ret = false;
		goto done;
	}

done:
	talloc_free(mem_ctx);
	return ret;
}
