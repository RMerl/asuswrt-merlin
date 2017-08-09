/*
   Unix SMB/CIFS implementation.
   test suite for RAP / DCERPC consistency
   Copyright (C) Guenther Deschner 2010

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
#include "libcli/libcli.h"
#include "torture/smbtorture.h"
#include "torture/util.h"
#include "libcli/rap/rap.h"
#include "torture/rap/proto.h"
#include "param/param.h"
#include "torture/rpc/torture_rpc.h"
#include "librpc/gen_ndr/ndr_srvsvc_c.h"

static bool test_rpc_netservergetinfo(struct torture_context *tctx,
				      struct smbcli_state *cli)
{
	struct rap_WserverGetInfo r;
	struct dcerpc_pipe *p;
	struct dcerpc_binding_handle *b;
	struct srvsvc_NetSrvGetInfo s;
	union srvsvc_NetSrvInfo info;

	const char *server_name;

	torture_assert_ntstatus_ok(tctx,
		torture_rpc_connection(tctx, &p, &ndr_table_srvsvc),
		"failed to open srvsvc");

	b = p->binding_handle;

	s.in.server_unc = NULL;
	s.in.level = 101;
	s.out.info = &info;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_srvsvc_NetSrvGetInfo_r(b, tctx, &s),
		"srvsvc_NetSrvGetInfo level 101 failed");
	torture_assert_werr_ok(tctx, s.out.result,
		"srvsvc_NetSrvGetInfo level 101 failed");

	r.in.bufsize = 0xffff;
	r.in.level = 0;

	torture_assert_ntstatus_ok(tctx,
		smbcli_rap_netservergetinfo(cli->tree, tctx, &r),
		"rap_netservergetinfo level 0 failed");
	torture_assert_int_equal(tctx, r.out.status, 0,
		"rap_netservergetinfo level 0 failed");

	server_name = talloc_strndup(tctx, info.info101->server_name, 16);

	torture_assert_str_equal(tctx, (const char *)r.out.info.info0.name, server_name, "server name");

	r.in.level = 1;

	torture_assert_ntstatus_ok(tctx,
		smbcli_rap_netservergetinfo(cli->tree, tctx, &r),
		"rap_netservergetinfo level 1 failed");
	torture_assert_int_equal(tctx, r.out.status, 0,
		"rap_netservergetinfo level 1 failed");

	torture_assert_str_equal(tctx, (const char *)r.out.info.info1.name, server_name, "server name");
	torture_assert_int_equal(tctx, r.out.info.info1.version_major, info.info101->version_major, "version major");
	torture_assert_int_equal(tctx, r.out.info.info1.version_minor, info.info101->version_minor, "version minor");
	torture_assert_int_equal(tctx, r.out.info.info1.servertype, info.info101->server_type, "server_type");
	torture_assert_str_equal(tctx, r.out.info.info1.comment, info.info101->comment, "comment");

	talloc_free(p);

	return true;
}

struct torture_suite *torture_rap_rpc(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "rpc");

	torture_suite_add_1smb_test(suite, "netservergetinfo",
				    test_rpc_netservergetinfo);

	suite->description = talloc_strdup(suite,
					   "RAP / DCERPC consistency tests");

	return suite;
}
