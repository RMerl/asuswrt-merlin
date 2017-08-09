/*
   Unix SMB/CIFS implementation.
   test suite for various Domain DFS
   Copyright (C) Matthieu Patou 2010

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
#include "../librpc/gen_ndr/ndr_dfsblobs.h"
#include "librpc/ndr/libndr.h"
#include "param/param.h"
#include "torture/torture.h"
#include "torture/dfs/proto.h"

static bool test_getdomainreferral(struct torture_context *tctx,
			       struct smbcli_state *cli)
{
	struct dfs_GetDFSReferral r;
	struct dfs_referral_resp resp;

	r.in.req.max_referral_level = 3;
	r.in.req.servername = "";
	r.out.resp = &resp;

	torture_assert_ntstatus_ok(tctx,
		   dfs_cli_do_call(cli->tree, &r),
		   "Get Domain referral failed");

	torture_assert_int_equal(tctx, resp.path_consumed, 0,
				 "Path consumed not equal to 0");
	torture_assert_int_equal(tctx, resp.nb_referrals != 0, 1,
				 "0 domains referrals returned");
	torture_assert_int_equal(tctx, resp.header_flags, 0,
				 "Header flag different it's not a referral server");
	torture_assert_int_equal(tctx, resp.referral_entries[1].version, 3,
				 talloc_asprintf(tctx,
					"Not expected version for referral entry 1 got %d expected 3",
					resp.referral_entries[1].version));
	torture_assert_int_equal(tctx, resp.referral_entries[0].version, 3,
				 talloc_asprintf(tctx,
					"Not expected version for referral entry 0 got %d expected 3",
					resp.referral_entries[0].version));
	torture_assert_int_equal(tctx, resp.referral_entries[0].referral.v3.data.server_type,
				 DFS_SERVER_NON_ROOT,
				 talloc_asprintf(tctx,
					"Wrong server type, expected non root server and got %d",
					resp.referral_entries[0].referral.v3.data.server_type));
	torture_assert_int_equal(tctx, resp.referral_entries[0].referral.v3.data.entry_flags,
				 DFS_FLAG_REFERRAL_DOMAIN_RESP,
				 talloc_asprintf(tctx,
					"Wrong entry flag expected to have a domain response and got %d",
					resp.referral_entries[0].referral.v3.data.entry_flags));
	torture_assert_int_equal(tctx, strlen(
				 resp.referral_entries[0].referral.v3.data.referrals.r2.special_name) > 0,
				 1,
				 "Length of domain is 0 or less");
	return true;
}

static bool test_getdcreferral(struct torture_context *tctx,
			       struct smbcli_state *cli)
{
	struct dfs_GetDFSReferral r, r2, r3;
	struct dfs_referral_resp resp, resp2, resp3;
	const char* str;
	const char* str2;

	r.in.req.max_referral_level = 3;
	r.in.req.servername = "";
	r.out.resp = &resp;

	torture_assert_ntstatus_ok(tctx,
		   dfs_cli_do_call(cli->tree, &r),
		   "Get Domain referral failed");

	str = resp.referral_entries[0].referral.v3.data.referrals.r2.special_name;
	if( strchr(str, '.') == NULL ) {
		str = resp.referral_entries[1].referral.v3.data.referrals.r2.special_name;
	}

	r2.in.req.max_referral_level = 3;
	r2.in.req.servername = str;
	r2.out.resp = &resp2;

	torture_assert_ntstatus_ok(tctx,
		   dfs_cli_do_call(cli->tree, &r2),
		   "Get DC Domain referral failed");


	torture_assert_int_equal(tctx, resp2.path_consumed, 0,
				 "Path consumed not equal to 0");
	torture_assert_int_equal(tctx, resp2.nb_referrals , 1,
				 "We do not received only 1 referral");
	torture_assert_int_equal(tctx, resp2.header_flags, 0,
				 "Header flag different it's not a referral server");
	torture_assert_int_equal(tctx, resp2.referral_entries[0].version, 3,
				 talloc_asprintf(tctx,
					"Not expected version for referral entry 0 got %d expected 3",
					resp2.referral_entries[0].version));
	torture_assert_int_equal(tctx, resp2.referral_entries[0].referral.v3.data.server_type,
				 DFS_SERVER_NON_ROOT,
				 talloc_asprintf(tctx,
					"Wrong server type, expected non root server and got %d",
					resp2.referral_entries[0].referral.v3.data.server_type));
	torture_assert_int_equal(tctx, resp2.referral_entries[0].referral.v3.data.entry_flags,
				 DFS_FLAG_REFERRAL_DOMAIN_RESP,
				 talloc_asprintf(tctx,
					"Wrong entry flag expected to have a domain response and got %d",
					resp2.referral_entries[0].referral.v3.data.entry_flags));
	torture_assert_int_equal(tctx, strlen(
				 resp2.referral_entries[0].referral.v3.data.referrals.r2.special_name) > 0,
				 1,
				 "Length of domain is 0 or less");
	torture_assert_int_equal(tctx, strlen(
				 resp2.referral_entries[0].referral.v3.data.referrals.r2.expanded_names[0]) > 0,
				 1,
				 "Length of first dc is less than 0");
	str = strchr(resp2.referral_entries[0].referral.v3.data.referrals.r2.expanded_names[0], '.');
	str2 = resp2.referral_entries[0].referral.v3.data.referrals.r2.special_name;
	if (str2[0] == '\\') {
		str2++;
	}
	torture_assert_int_equal(tctx, strlen(str) >0, 1 ,"Length of domain too short");
	str++;
	torture_assert_int_equal(tctx, strcmp(str,str2), 0,
					talloc_asprintf(tctx, "Pb domain of the dc is not"\
								"the same as the requested: domain = %s got =%s",str2 ,str));

	r3.in.req.max_referral_level = 3;
	/*
	 * Windows 7 and at least windows 2008 server sends domain.fqdn instead of \domain.fqdn
	 * (as it is specified in the spec)
	 * Let's check that we are able to support it too
	 */
	r3.in.req.servername = str;
	r3.out.resp = &resp3;

	torture_assert_ntstatus_ok(tctx,
		   dfs_cli_do_call(cli->tree, &r3),
		   "Get DC Domain referral failed");

	torture_assert_int_equal(tctx, resp3.path_consumed, 0,
				 "Path consumed not equal to 0");
	torture_assert_int_equal(tctx, resp3.nb_referrals , 1,
				 "We do not received only 1 referral");
	torture_assert_int_equal(tctx, resp3.header_flags, 0,
				 "Header flag different it's not a referral server");
	torture_assert_int_equal(tctx, resp3.referral_entries[0].version, 3,
				 talloc_asprintf(tctx,
					"Not expected version for referral entry 0 got %d expected 3",
					resp3.referral_entries[0].version));
	torture_assert_int_equal(tctx, resp3.referral_entries[0].referral.v3.data.server_type,
				 DFS_SERVER_NON_ROOT,
				 talloc_asprintf(tctx,
					"Wrong server type, expected non root server and got %d",
					resp3.referral_entries[0].referral.v3.data.server_type));
	torture_assert_int_equal(tctx, resp3.referral_entries[0].referral.v3.data.entry_flags,
				 DFS_FLAG_REFERRAL_DOMAIN_RESP,
				 talloc_asprintf(tctx,
					"Wrong entry flag expected to have a domain response and got %d",
					resp3.referral_entries[0].referral.v3.data.entry_flags));
	torture_assert_int_equal(tctx, strlen(
				 resp3.referral_entries[0].referral.v3.data.referrals.r2.special_name) > 0,
				 1,
				 "Length of domain is 0 or less");
	torture_assert_int_equal(tctx, strlen(
				 resp3.referral_entries[0].referral.v3.data.referrals.r2.expanded_names[0]) > 0,
				 1,
				 "Length of first dc is less than 0");
	return true;
}

static bool test_getdcreferral_netbios(struct torture_context *tctx,
			       struct smbcli_state *cli)
{
	struct dfs_GetDFSReferral r, r2, r3;
	struct dfs_referral_resp resp, resp2, resp3;
	const char* str;

	r.in.req.max_referral_level = 3;
	r.in.req.servername = "";
	r.out.resp = &resp;

	torture_assert_ntstatus_ok(tctx,
		   dfs_cli_do_call(cli->tree, &r),
		   "Get Domain referral failed");

	r2.in.req.max_referral_level = 3;

	str = resp.referral_entries[0].referral.v3.data.referrals.r2.special_name;
	if( strchr(str, '.') != NULL ) {
		str = resp.referral_entries[1].referral.v3.data.referrals.r2.special_name;
	}

	r2.in.req.servername = str;
	r2.out.resp = &resp2;

	torture_assert_ntstatus_ok(tctx,
		   dfs_cli_do_call(cli->tree, &r2),
		   "Get DC Domain referral failed");

	torture_assert_int_equal(tctx, resp2.path_consumed, 0,
				 "Path consumed not equal to 0");
	torture_assert_int_equal(tctx, resp2.nb_referrals , 1,
				 "We do not received only 1 referral");
	torture_assert_int_equal(tctx, resp2.header_flags, 0,
				 "Header flag different it's not a referral server");
	torture_assert_int_equal(tctx, resp2.referral_entries[0].version, 3,
				 talloc_asprintf(tctx,
					"Not expected version for referral entry 0 got %d expected 3",
					resp2.referral_entries[0].version));
	torture_assert_int_equal(tctx, resp2.referral_entries[0].referral.v3.data.server_type,
				 DFS_SERVER_NON_ROOT,
				 talloc_asprintf(tctx,
					"Wrong server type, expected non root server and got %d",
					resp2.referral_entries[0].referral.v3.data.server_type));
	torture_assert_int_equal(tctx, resp2.referral_entries[0].referral.v3.data.entry_flags,
				 DFS_FLAG_REFERRAL_DOMAIN_RESP,
				 talloc_asprintf(tctx,
					"Wrong entry flag expected to have a domain response and got %d",
					resp2.referral_entries[0].referral.v3.data.entry_flags));
	torture_assert_int_equal(tctx, strlen(
				 resp2.referral_entries[0].referral.v3.data.referrals.r2.special_name) > 0,
				 1,
				 "Length of domain is 0 or less");
	torture_assert_int_equal(tctx, strlen(
				 resp2.referral_entries[0].referral.v3.data.referrals.r2.expanded_names[0]) > 0,
				 1,
				 "Length of first dc is less than 0");
	torture_assert(tctx, strchr(
		       resp2.referral_entries[0].referral.v3.data.referrals.r2.expanded_names[0],'.') == NULL,
		       "referral contains dots it's not a netbios name");

	r3.in.req.max_referral_level = 3;
	/*
	 * Windows 7 and at least windows 2008 server sends domain.fqdn instead of \domain.fqdn
	 * (as it is specified in the spec)
	 * Let's check that we are able to support it too
	 */
	r3.in.req.servername = str + 1;
	r3.out.resp = &resp3;

	torture_assert_ntstatus_ok(tctx,
		   dfs_cli_do_call(cli->tree, &r3),
		   "Get DC Domain referral failed");

	torture_assert_int_equal(tctx, resp3.path_consumed, 0,
				 "Path consumed not equal to 0");
	torture_assert_int_equal(tctx, resp3.nb_referrals , 1,
				 "We do not received only 1 referral");
	torture_assert_int_equal(tctx, resp3.header_flags, 0,
				 "Header flag different it's not a referral server");
	torture_assert_int_equal(tctx, resp3.referral_entries[0].version, 3,
				 talloc_asprintf(tctx,
					"Not expected version for referral entry 0 got %d expected 3",
					resp3.referral_entries[0].version));
	torture_assert_int_equal(tctx, resp3.referral_entries[0].referral.v3.data.server_type,
				 DFS_SERVER_NON_ROOT,
				 talloc_asprintf(tctx,
					"Wrong server type, expected non root server and got %d",
					resp3.referral_entries[0].referral.v3.data.server_type));
	torture_assert_int_equal(tctx, resp3.referral_entries[0].referral.v3.data.entry_flags,
				 DFS_FLAG_REFERRAL_DOMAIN_RESP,
				 talloc_asprintf(tctx,
					"Wrong entry flag expected to have a domain response and got %d",
					resp3.referral_entries[0].referral.v3.data.entry_flags));
	torture_assert_int_equal(tctx, strlen(
				 resp3.referral_entries[0].referral.v3.data.referrals.r2.special_name) > 0,
				 1,
				 "Length of domain is 0 or less");
	torture_assert_int_equal(tctx, strlen(
				 resp3.referral_entries[0].referral.v3.data.referrals.r2.expanded_names[0]) > 0,
				 1,
				 "Length of first dc is less than 0");
	torture_assert(tctx, strchr(
		       resp3.referral_entries[0].referral.v3.data.referrals.r2.expanded_names[0],'.') == NULL,
		       "referral contains dots it's not a netbios name");
	return true;
}

static bool test_getsysvolreferral(struct torture_context *tctx,
			       struct smbcli_state *cli)
{
	const char* str;
	struct dfs_GetDFSReferral r, r2, r3;
	struct dfs_referral_resp resp, resp2, resp3;

	r.in.req.max_referral_level = 3;
	r.in.req.servername = "";
	r.out.resp = &resp;

	torture_assert_ntstatus_ok(tctx,
		   dfs_cli_do_call(cli->tree, &r),
		   "Get Domain referral failed");

	str = resp.referral_entries[0].referral.v3.data.referrals.r2.special_name;
	if( strchr(str, '.') == NULL ) {
		str = resp.referral_entries[1].referral.v3.data.referrals.r2.special_name;
	}

	r2.in.req.max_referral_level = 3;
	r2.in.req.servername = str;
	r2.out.resp = &resp2;

	torture_assert_ntstatus_ok(tctx,
		   dfs_cli_do_call(cli->tree, &r2),
		   "Get DC Domain referral failed");

	r3.in.req.max_referral_level = 3;
	r3.in.req.servername = talloc_asprintf(tctx, "%s\\sysvol", str);
	r3.out.resp = &resp3;

	torture_assert_ntstatus_ok(tctx,
		   dfs_cli_do_call(cli->tree, &r3),
		   "Get sysvol Domain referral failed");

	torture_assert_int_equal(tctx, resp3.path_consumed, 2*strlen(r3.in.req.servername),
				 "Path consumed not equal to length of the request");
	torture_assert_int_equal(tctx, resp3.nb_referrals != 0, 1,
				 "We do not receive at least 1 referral");
	torture_assert_int_equal(tctx, resp3.header_flags, DFS_HEADER_FLAG_STORAGE_SVR,
				 "Header flag different it's not a referral for a storage");
	torture_assert_int_equal(tctx, resp3.referral_entries[0].version, 3,
				 talloc_asprintf(tctx,
					"Not expected version for referral entry 0 got %d expected 3",
					resp3.referral_entries[0].version));
	torture_assert_int_equal(tctx, resp3.referral_entries[0].referral.v3.data.server_type,
				 DFS_SERVER_NON_ROOT,
				 talloc_asprintf(tctx,
					"Wrong server type, expected non root server and got %d",
					resp3.referral_entries[0].referral.v3.data.server_type));
	torture_assert_int_equal(tctx, resp3.referral_entries[0].referral.v3.data.entry_flags,
				 0,
				 talloc_asprintf(tctx,
					"Wrong entry flag expected to have a non domain response and got %d",
					resp3.referral_entries[0].referral.v3.data.entry_flags));
	torture_assert_int_equal(tctx, strlen(
				 resp3.referral_entries[0].referral.v3.data.referrals.r2.special_name) > 0,
				 1,
				 "Length of domain is 0 or less");
	torture_assert_int_equal(tctx, strlen(
				 resp2.referral_entries[0].referral.v3.data.referrals.r2.expanded_names[0]) > 0,
				 1,
				 "Length of first referral is less than 0");

	r3.in.req.max_referral_level = 4;

	torture_assert_ntstatus_ok(tctx,
		   dfs_cli_do_call(cli->tree, &r3),
		   "Get sysvol Domain referral failed");

	torture_assert_int_equal(tctx, resp3.referral_entries[0].version, 4,
				 talloc_asprintf(tctx,
					"Not expected version for referral entry 0 got %d expected 4",
					resp3.referral_entries[0].version));
#if 0
	/*
	 * We do not support fallback indication for the moment
	 */
	torture_assert_int_equal(tctx, resp3.header_flags,
					DFS_HEADER_FLAG_STORAGE_SVR | DFS_HEADER_FLAG_TARGET_BCK,
					"Header flag different it's not a referral for a storage with fallback");
#endif
	torture_assert_int_equal(tctx, resp3.referral_entries[0].referral.v4.entry_flags,
				 DFS_FLAG_REFERRAL_FIRST_TARGET_SET,
				 talloc_asprintf(tctx,
					"Wrong entry flag expected to have a non domain response and got %d",
					resp3.referral_entries[0].referral.v4.entry_flags));
	return true;
}

static bool test_unknowndomain(struct torture_context *tctx,
			       struct smbcli_state *cli)
{
	struct dfs_GetDFSReferral r, r2;
	struct dfs_referral_resp resp, resp2;

	r.in.req.max_referral_level = 3;
	r.in.req.servername = "";
	r.out.resp = &resp;

	torture_assert_ntstatus_ok(tctx,
		   dfs_cli_do_call(cli->tree, &r),
		   "Get Domain referral failed");

	r2.in.req.max_referral_level = 3;
	r2.in.req.servername = "foobar.none.net";
	r2.out.resp = &resp2;

	torture_assert_ntstatus_equal(tctx,
		   dfs_cli_do_call(cli->tree, &r2),
		   NT_STATUS_INVALID_PARAMETER,
		   "Get DC Domain didn't return exptected error code");

	return true;
}

static bool test_getsysvolplusreferral(struct torture_context *tctx,
			       struct smbcli_state *cli)
{
	const char* str;
	struct dfs_GetDFSReferral r, r2, r3;
	struct dfs_referral_resp resp, resp2, resp3;

	r.in.req.max_referral_level = 3;
	r.in.req.servername = "";
	r.out.resp = &resp;

	torture_assert_ntstatus_ok(tctx,
		   dfs_cli_do_call(cli->tree, &r),
		   "Get Domain referral failed");

	r2.in.req.max_referral_level = 3;
	r2.in.req.servername = resp.referral_entries[0].referral.v3.data.referrals.r2.special_name;
	r2.out.resp = &resp2;

	torture_assert_ntstatus_ok(tctx,
		   dfs_cli_do_call(cli->tree, &r2),
		   "Get DC Domain referral failed");

	str = resp2.referral_entries[0].referral.v3.data.referrals.r2.special_name;
	r3.in.req.max_referral_level = 3;
	r3.in.req.servername = talloc_asprintf(tctx, "%s\\sysvol\\foo", str);
	r3.out.resp = &resp3;

	torture_assert_ntstatus_equal(tctx,
		   dfs_cli_do_call(cli->tree, &r3),
		   NT_STATUS_NOT_FOUND,
		   "Bad behavior with subtree sysvol referral");

	return true;
}

static bool test_low_referral_level(struct torture_context *tctx,
			       struct smbcli_state *cli)
{
	struct dfs_GetDFSReferral r;
	struct dfs_referral_resp resp;

	r.in.req.max_referral_level = 2;
	r.in.req.servername = "";
	r.out.resp = &resp;

	torture_assert_ntstatus_equal(tctx,
		   dfs_cli_do_call(cli->tree, &r),
		   NT_STATUS_UNSUCCESSFUL,
		   "Unexpected STATUS for invalid deferral retquest");

	return true;
}

NTSTATUS torture_dfs_init(void)
{
	struct torture_suite *suite = torture_suite_create(talloc_autofree_context(), "dfs");
	struct torture_suite *suite_basic = torture_suite_create(suite, "domain");

	torture_suite_add_suite(suite, suite_basic);

	torture_suite_add_1smb_test(suite_basic, "domain referral",
				    test_getdomainreferral);
	torture_suite_add_1smb_test(suite_basic, "dc referral",
				    test_getdcreferral);
	torture_suite_add_1smb_test(suite_basic, "dc referral netbios",
				    test_getdcreferral_netbios);

	torture_suite_add_1smb_test(suite_basic, "sysvol referral",
				    test_getsysvolreferral);

	/* Non standard case */

	torture_suite_add_1smb_test(suite_basic, "dc referral on unknown domain",
				    test_unknowndomain);
	torture_suite_add_1smb_test(suite_basic, "sysvol with subtree referral",
				    test_getsysvolplusreferral);
	torture_suite_add_1smb_test(suite_basic, "referral with a level 2",
				    test_low_referral_level);

	/*
	 * test with invalid level
	 * test with netbios
	 */

	suite->description = talloc_strdup(suite, "DFS referrals calls");

	torture_register_suite(suite);

	return NT_STATUS_OK;
}
