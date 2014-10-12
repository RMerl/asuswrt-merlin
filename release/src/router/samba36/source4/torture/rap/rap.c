/* 
   Unix SMB/CIFS implementation.
   test suite for various RAP operations
   Copyright (C) Volker Lendecke 2004
   Copyright (C) Tim Potter 2005
   Copyright (C) Jelmer Vernooij 2007
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
#include "param/param.h"
#include "libcli/rap/rap.h"
#include "torture/rap/proto.h"

static bool test_netshareenum(struct torture_context *tctx, 
			      struct smbcli_state *cli)
{
	struct rap_NetShareEnum r;
	int i;

	r.in.level = 1;
	r.in.bufsize = 8192;

	torture_assert_ntstatus_ok(tctx, 
		smbcli_rap_netshareenum(cli->tree, tctx, &r), "");

	for (i=0; i<r.out.count; i++) {
		printf("%s %d %s\n", r.out.info[i].info1.share_name,
		       r.out.info[i].info1.share_type,
		       r.out.info[i].info1.comment);
	}

	return true;
}

static bool test_netserverenum(struct torture_context *tctx, 
			       struct smbcli_state *cli)
{
	struct rap_NetServerEnum2 r;
	int i;

	r.in.level = 0;
	r.in.bufsize = 8192;
	r.in.servertype = 0xffffffff;
	r.in.servertype = 0x80000000;
	r.in.domain = NULL;

	torture_assert_ntstatus_ok(tctx, 
		   smbcli_rap_netserverenum2(cli->tree, tctx, &r), "");

	for (i=0; i<r.out.count; i++) {
		switch (r.in.level) {
		case 0:
			printf("%s\n", r.out.info[i].info0.name);
			break;
		case 1:
			printf("%s %x %s\n", r.out.info[i].info1.name,
			       r.out.info[i].info1.servertype,
			       r.out.info[i].info1.comment);
			break;
		}
	}

	return true;
}

static bool test_netservergetinfo(struct torture_context *tctx, 
				  struct smbcli_state *cli)
{
	struct rap_WserverGetInfo r;
	bool res = true;

	r.in.bufsize = 0xffff;

	r.in.level = 0;
	torture_assert_ntstatus_ok(tctx,
		smbcli_rap_netservergetinfo(cli->tree, tctx, &r),
		"rap_netservergetinfo level 0 failed");
	torture_assert_werr_ok(tctx, W_ERROR(r.out.status),
		"rap_netservergetinfo level 0 failed");

	r.in.level = 1;
	torture_assert_ntstatus_ok(tctx,
		smbcli_rap_netservergetinfo(cli->tree, tctx, &r),
		"rap_netservergetinfo level 1 failed");
	torture_assert_werr_ok(tctx, W_ERROR(r.out.status),
		"rap_netservergetinfo level 1 failed");

	return res;
}

static bool test_netsessionenum(struct torture_context *tctx,
				struct smbcli_state *cli)
{
	struct rap_NetSessionEnum r;
	int i,n;
	uint16_t levels[] = { 2 };

	for (i=0; i < ARRAY_SIZE(levels); i++) {

		r.in.level = levels[i];
		r.in.bufsize = 8192;

		torture_comment(tctx,
			"Testing rap_NetSessionEnum level %d\n", r.in.level);

		torture_assert_ntstatus_ok(tctx,
			smbcli_rap_netsessionenum(cli->tree, tctx, &r),
			"smbcli_rap_netsessionenum failed");
		torture_assert_werr_ok(tctx, W_ERROR(r.out.status),
			"smbcli_rap_netsessionenum failed");

		for (n=0; n < r.out.count; n++) {
			switch (r.in.level) {
			case 2:
				torture_comment(tctx, "ComputerName: %s\n",
					r.out.info[n].info2.ComputerName);

				torture_comment(tctx, "UserName: %s\n",
					r.out.info[n].info2.UserName);

				torture_assert(tctx, r.out.info[n].info2.ComputerName,
					"ComputerName empty");
				torture_assert(tctx, r.out.info[n].info2.UserName,
					"UserName empty");
				break;
			default:
				break;
			}
		}
	}

	return true;
}

static bool test_netsessiongetinfo_bysession(struct torture_context *tctx,
					     struct smbcli_state *cli,
					     const char *session)
{
	struct rap_NetSessionGetInfo r;
	int i;
	uint16_t levels[] = { 2 };

	if (session && session[0] == '\\' && session[1] == '\\') {
		r.in.SessionName = session;
	} else {
		r.in.SessionName = talloc_asprintf(tctx, "\\\\%s", session);
	}
	r.in.bufsize = 0xffff;

	for (i=0; i < ARRAY_SIZE(levels); i++) {

		r.in.level = levels[i];

		torture_assert_ntstatus_ok(tctx,
			smbcli_rap_netsessiongetinfo(cli->tree, tctx, &r),
			"rap_netsessiongetinfo failed");
		torture_assert_werr_ok(tctx, W_ERROR(r.out.status),
			"rap_netsessiongetinfo failed");
	}

	return true;
}

static bool test_netsessiongetinfo(struct torture_context *tctx,
				   struct smbcli_state *cli)
{
	struct rap_NetSessionEnum r;
	int i,n;
	uint16_t levels[] = { 2 };

	for (i=0; i < ARRAY_SIZE(levels); i++) {

		r.in.level = levels[i];
		r.in.bufsize = 8192;

		torture_assert_ntstatus_ok(tctx,
			smbcli_rap_netsessionenum(cli->tree, tctx, &r),
			"smbcli_rap_netsessionenum failed");
		torture_assert_werr_ok(tctx, W_ERROR(r.out.status),
			"smbcli_rap_netsessionenum failed");

		for (n=0; n < r.out.count; n++) {
			torture_assert(tctx,
				test_netsessiongetinfo_bysession(tctx, cli, r.out.info[n].info2.ComputerName),
				"failed to query sessioninfo");
		}
	}

	return true;
}

static bool test_netremotetod(struct torture_context *tctx,
			      struct smbcli_state *cli)
{
	struct rap_NetRemoteTOD r;

	r.in.bufsize = 8192;

	torture_assert_ntstatus_ok(tctx,
		smbcli_rap_netremotetod(cli->tree, tctx, &r),
		"smbcli_rap_netremotetod failed");
	torture_assert_werr_ok(tctx, W_ERROR(r.out.status),
		"smbcli_rap_netremotetod failed");

	return true;
}

bool torture_rap_scan(struct torture_context *torture, struct smbcli_state *cli)
{
	int callno;

	for (callno = 0; callno < 0xffff; callno++) {
		struct rap_call *call = new_rap_cli_call(torture, callno);
		NTSTATUS result;

		result = rap_cli_do_call(cli->tree, call);

		if (!NT_STATUS_EQUAL(result, NT_STATUS_INVALID_PARAMETER))
			continue;

		printf("callno %d is RAP call\n", callno);
	}

	return true;
}

NTSTATUS torture_rap_init(void)
{
	struct torture_suite *suite = torture_suite_create(talloc_autofree_context(), "rap");
	struct torture_suite *suite_basic = torture_suite_create(suite, "basic");

	torture_suite_add_suite(suite, suite_basic);
	torture_suite_add_suite(suite, torture_rap_rpc(suite));
	torture_suite_add_suite(suite, torture_rap_printing(suite));
	torture_suite_add_suite(suite, torture_rap_sam(suite));

	torture_suite_add_1smb_test(suite_basic, "netserverenum", 
				    test_netserverenum);
	torture_suite_add_1smb_test(suite_basic, "netshareenum",
				    test_netshareenum);
	torture_suite_add_1smb_test(suite_basic, "netservergetinfo",
				    test_netservergetinfo);
	torture_suite_add_1smb_test(suite_basic, "netsessionenum",
				    test_netsessionenum);
	torture_suite_add_1smb_test(suite_basic, "netsessiongetinfo",
				    test_netsessiongetinfo);
	torture_suite_add_1smb_test(suite_basic, "netremotetod",
				    test_netremotetod);

	torture_suite_add_1smb_test(suite, "scan", torture_rap_scan);

	suite->description = talloc_strdup(suite, 
						"Remote Administration Protocol tests");

	torture_register_suite(suite);

	return NT_STATUS_OK;
}
