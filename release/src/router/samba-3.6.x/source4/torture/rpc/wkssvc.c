/*
   Unix SMB/CIFS implementation.
   test suite for wkssvc rpc operations

   Copyright (C) Andrew Tridgell 2003
   Copyright (C) Günther Deschner 2007

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
#include "torture/torture.h"
#include "librpc/gen_ndr/ndr_wkssvc_c.h"
#include "torture/rpc/torture_rpc.h"
#include "lib/cmdline/popt_common.h"
#include "param/param.h"
#include "../lib/crypto/crypto.h"
#include "libcli/auth/libcli_auth.h"

#define SMBTORTURE_MACHINE_NAME "smbtrt_name"
#define SMBTORTURE_ALTERNATE_NAME "smbtrt_altname"
#define SMBTORTURE_TRANSPORT_NAME "\\Device\\smbtrt_transport_name"
#define SMBTORTURE_USE_NAME "S:"
#define SMBTORTURE_MESSAGE "You are currently tortured by Samba"

static bool test_NetWkstaGetInfo(struct torture_context *tctx,
				 struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct wkssvc_NetWkstaGetInfo r;
	union wkssvc_NetWkstaInfo info;
	uint16_t levels[] = {100, 101, 102, 502};
	int i;
	struct dcerpc_binding_handle *b = p->binding_handle;

	r.in.server_name = dcerpc_server_name(p);
	r.out.info = &info;

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		r.in.level = levels[i];
		torture_comment(tctx, "Testing NetWkstaGetInfo level %u\n",
				r.in.level);
		status = dcerpc_wkssvc_NetWkstaGetInfo_r(b, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status,
			talloc_asprintf(tctx, "NetWkstaGetInfo level %u failed",
					r.in.level));
		torture_assert_werr_ok(tctx, r.out.result,
			talloc_asprintf(tctx, "NetWkstaGetInfo level %u failed",
					r.in.level));
	}

	return true;
}

static bool test_NetWkstaTransportEnum(struct torture_context *tctx,
				       struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct wkssvc_NetWkstaTransportEnum r;
	uint32_t resume_handle = 0;
	struct wkssvc_NetWkstaTransportInfo info;
	union wkssvc_NetWkstaTransportCtr ctr;
	struct wkssvc_NetWkstaTransportCtr0 ctr0;
	uint32_t total_entries = 0;
	struct dcerpc_binding_handle *b = p->binding_handle;

	ZERO_STRUCT(ctr0);
	ctr.ctr0 = &ctr0;

	info.level = 0;
	info.ctr = ctr;

	r.in.server_name = dcerpc_server_name(p);
	r.in.info = &info;
	r.in.max_buffer = (uint32_t)-1;
	r.in.resume_handle = &resume_handle;
	r.out.total_entries = &total_entries;
	r.out.info = &info;
	r.out.resume_handle = &resume_handle;

	torture_comment(tctx, "Testing NetWkstaTransportEnum level 0\n");

	status = dcerpc_wkssvc_NetWkstaTransportEnum_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status,
				   "NetWkstaTransportEnum failed");
	torture_assert_werr_ok(tctx, r.out.result, talloc_asprintf(tctx,
			       "NetWkstaTransportEnum level %u failed",
			       info.level));

	return true;
}

static bool test_NetrWkstaTransportAdd(struct torture_context *tctx,
				       struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct wkssvc_NetrWkstaTransportAdd r;
	struct wkssvc_NetWkstaTransportInfo0 info0;
	uint32_t parm_err = 0;
	struct dcerpc_binding_handle *b = p->binding_handle;

	ZERO_STRUCT(info0);

	info0.quality_of_service = 0xffff;
	info0.vc_count = 0;
	info0.name = SMBTORTURE_TRANSPORT_NAME;
	info0.address = "000000000000";
	info0.wan_link = 0x400;

	r.in.server_name = dcerpc_server_name(p);
	r.in.level = 0;
	r.in.info0 = &info0;
	r.in.parm_err = r.out.parm_err = &parm_err;

	torture_comment(tctx, "Testing NetrWkstaTransportAdd level 0\n");

	status = dcerpc_wkssvc_NetrWkstaTransportAdd_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status,
				   "NetrWkstaTransportAdd failed");
	torture_assert_werr_equal(tctx, r.out.result,
				  WERR_INVALID_PARAM,
				  "NetrWkstaTransportAdd level 0 failed");

	return true;
}

static bool test_NetrWkstaTransportDel(struct torture_context *tctx,
				       struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct wkssvc_NetrWkstaTransportDel r;
	struct dcerpc_binding_handle *b = p->binding_handle;

	r.in.server_name = dcerpc_server_name(p);
	r.in.transport_name = SMBTORTURE_TRANSPORT_NAME;
	r.in.unknown3 = 0;

	torture_comment(tctx, "Testing NetrWkstaTransportDel\n");

	status = dcerpc_wkssvc_NetrWkstaTransportDel_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status,
				   "NetrWkstaTransportDel failed");
	torture_assert_werr_ok(tctx, r.out.result,
			       "NetrWkstaTransportDel");

	return true;
}

static bool test_NetWkstaEnumUsers(struct torture_context *tctx,
				   struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct wkssvc_NetWkstaEnumUsers r;
	uint32_t handle = 0;
	uint32_t entries_read = 0;
	struct wkssvc_NetWkstaEnumUsersInfo info;
	struct wkssvc_NetWkstaEnumUsersCtr0 *user0;
	struct wkssvc_NetWkstaEnumUsersCtr1 *user1;
	uint32_t levels[] = { 0, 1 };
	int i;
	struct dcerpc_binding_handle *b = p->binding_handle;

	for (i=0; i<ARRAY_SIZE(levels); i++) {

		ZERO_STRUCT(info);

		info.level = levels[i];
		switch (info.level) {
		case 0:
			user0 = talloc_zero(tctx,
					    struct wkssvc_NetWkstaEnumUsersCtr0);
			info.ctr.user0 = user0;
			break;
		case 1:
			user1 = talloc_zero(tctx,
					    struct wkssvc_NetWkstaEnumUsersCtr1);
			info.ctr.user1 = user1;
			break;
		default:
			break;
		}

		r.in.server_name = dcerpc_server_name(p);
		r.in.prefmaxlen = (uint32_t)-1;
		r.in.info = r.out.info = &info;
		r.in.resume_handle = r.out.resume_handle = &handle;

		r.out.entries_read = &entries_read;

		torture_comment(tctx, "Testing NetWkstaEnumUsers level %u\n",
				levels[i]);

		status = dcerpc_wkssvc_NetWkstaEnumUsers_r(b, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status,
					   "NetWkstaEnumUsers failed");
		torture_assert_werr_ok(tctx, r.out.result,
				       "NetWkstaEnumUsers failed");
	}

	return true;
}

static bool test_NetrWkstaUserGetInfo(struct torture_context *tctx,
				      struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct wkssvc_NetrWkstaUserGetInfo r;
	union wkssvc_NetrWkstaUserInfo info;
	const char *dom = lpcfg_workgroup(tctx->lp_ctx);
	struct cli_credentials *creds = cmdline_credentials;
	const char *user = cli_credentials_get_username(creds);
	int i;
	struct dcerpc_binding_handle *b = p->binding_handle;

	const struct {
		const char *unknown;
		uint32_t level;
		WERROR result;
	} tests[] = {
		{ NULL, 0, WERR_NO_SUCH_LOGON_SESSION },
		{ NULL, 1, WERR_NO_SUCH_LOGON_SESSION },
		{ NULL, 1101, WERR_OK },
		{ dom, 0, WERR_INVALID_PARAM },
		{ dom, 1, WERR_INVALID_PARAM },
		{ dom, 1101, WERR_INVALID_PARAM },
		{ user, 0, WERR_INVALID_PARAM },
		{ user, 1, WERR_INVALID_PARAM },
		{ user, 1101, WERR_INVALID_PARAM },
	};

	for (i=0; i<ARRAY_SIZE(tests); i++) {
		r.in.unknown = tests[i].unknown;
		r.in.level = tests[i].level;
		r.out.info = &info;

		torture_comment(tctx, "Testing NetrWkstaUserGetInfo level %u\n",
				r.in.level);

		status = dcerpc_wkssvc_NetrWkstaUserGetInfo_r(b, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status,
					   "NetrWkstaUserGetInfo failed");
		torture_assert_werr_equal(tctx, r.out.result,
					  tests[i].result,
					  "NetrWkstaUserGetInfo failed");
	}

	return true;
}

static bool test_NetrUseEnum(struct torture_context *tctx,
			     struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct wkssvc_NetrUseEnum r;
	uint32_t handle = 0;
	uint32_t entries_read = 0;
	struct wkssvc_NetrUseEnumInfo info;
	struct wkssvc_NetrUseEnumCtr0 *use0;
	struct wkssvc_NetrUseEnumCtr1 *use1;
	struct wkssvc_NetrUseEnumCtr2 *use2;
	uint32_t levels[] = { 0, 1, 2 };
	int i;
	struct dcerpc_binding_handle *b = p->binding_handle;

	for (i=0; i<ARRAY_SIZE(levels); i++) {

		ZERO_STRUCT(info);

		info.level = levels[i];
		switch (info.level) {
		case 0:
			use0 = talloc_zero(tctx, struct wkssvc_NetrUseEnumCtr0);
			info.ctr.ctr0 = use0;
			break;
		case 1:
			use1 = talloc_zero(tctx, struct wkssvc_NetrUseEnumCtr1);
			info.ctr.ctr1 = use1;
			break;
		case 2:
			use2 = talloc_zero(tctx, struct wkssvc_NetrUseEnumCtr2);
			info.ctr.ctr2 = use2;
			break;
		default:
			break;
		}

		r.in.server_name = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
		r.in.prefmaxlen = (uint32_t)-1;
		r.in.info = r.out.info = &info;
		r.in.resume_handle = r.out.resume_handle = &handle;

		r.out.entries_read = &entries_read;

		torture_comment(tctx, "Testing NetrUseEnum level %u\n",
				levels[i]);

		status = dcerpc_wkssvc_NetrUseEnum_r(b, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status,
					   "NetrUseEnum failed");
		torture_assert_werr_ok(tctx, r.out.result,
				       "NetrUseEnum failed");
	}

	return true;
}

static bool test_NetrUseAdd(struct torture_context *tctx,
			    struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct wkssvc_NetrUseAdd r;
	struct wkssvc_NetrUseInfo0 info0;
	struct wkssvc_NetrUseInfo1 info1;
	union wkssvc_NetrUseGetInfoCtr *ctr;
	uint32_t parm_err = 0;
	struct dcerpc_binding_handle *b = p->binding_handle;

	ctr = talloc(tctx, union wkssvc_NetrUseGetInfoCtr);

	ZERO_STRUCT(info0);

	info0.local = SMBTORTURE_USE_NAME;
	info0.remote = "\\\\localhost\\c$";

	ctr->info0 = &info0;

	r.in.server_name = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	r.in.level = 0;
	r.in.ctr = ctr;
	r.in.parm_err = r.out.parm_err = &parm_err;

	torture_comment(tctx, "Testing NetrUseAdd level %u\n",
			r.in.level);

	status = dcerpc_wkssvc_NetrUseAdd_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status,
				   "NetrUseAdd failed");
	torture_assert_werr_equal(tctx, r.out.result, WERR_UNKNOWN_LEVEL,
			       "NetrUseAdd failed");

	ZERO_STRUCT(r);
	ZERO_STRUCT(info1);

	info1.local = SMBTORTURE_USE_NAME;
	info1.remote = "\\\\localhost\\sysvol";
	info1.password = NULL;

	ctr->info1 = &info1;

	r.in.server_name = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	r.in.level = 1;
	r.in.ctr = ctr;
	r.in.parm_err = r.out.parm_err = &parm_err;

	torture_comment(tctx, "Testing NetrUseAdd level %u\n",
			r.in.level);

	status = dcerpc_wkssvc_NetrUseAdd_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status,
				   "NetrUseAdd failed");
	torture_assert_werr_ok(tctx, r.out.result,
			       "NetrUseAdd failed");

	return true;
}

static bool test_NetrUseDel(struct torture_context *tctx,
			    struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct wkssvc_NetrUseDel r;
	struct dcerpc_binding_handle *b = p->binding_handle;

	r.in.server_name = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	r.in.use_name = SMBTORTURE_USE_NAME;
	r.in.force_cond = 0;

	torture_comment(tctx, "Testing NetrUseDel\n");

	status = dcerpc_wkssvc_NetrUseDel_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status,
				   "NetrUseDel failed");
	torture_assert_werr_ok(tctx, r.out.result,
			       "NetrUseDel failed");
	return true;
}

static bool test_NetrUseGetInfo_level(struct torture_context *tctx,
				      struct dcerpc_pipe *p,
				      const char *use_name,
				      uint32_t level,
				      WERROR werr)
{
	NTSTATUS status;
	struct wkssvc_NetrUseGetInfo r;
	union wkssvc_NetrUseGetInfoCtr ctr;
	struct dcerpc_binding_handle *b = p->binding_handle;

	ZERO_STRUCT(ctr);

	r.in.server_name = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	r.in.use_name = use_name;
	r.in.level = level;
	r.out.ctr = &ctr;
	status = dcerpc_wkssvc_NetrUseGetInfo_r(b, tctx, &r);

	torture_assert_ntstatus_ok(tctx, status,
				   "NetrUseGetInfo failed");
	torture_assert_werr_equal(tctx, r.out.result, werr,
				  "NetrUseGetInfo failed");
	return true;
}

static bool test_NetrUseGetInfo(struct torture_context *tctx,
				struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct wkssvc_NetrUseEnum r;
	uint32_t handle = 0;
	uint32_t entries_read = 0;
	struct wkssvc_NetrUseEnumInfo info;
	struct wkssvc_NetrUseEnumCtr0 *use0;
	uint32_t levels[] = { 0, 1, 2 };
	const char *use_name = NULL;
	int i, k;
	struct dcerpc_binding_handle *b = p->binding_handle;

	ZERO_STRUCT(info);

	info.level = 0;
	use0 = talloc_zero(tctx, struct wkssvc_NetrUseEnumCtr0);
	info.ctr.ctr0 = use0;

	r.in.server_name = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	r.in.prefmaxlen = (uint32_t)-1;
	r.in.info = r.out.info = &info;
	r.in.resume_handle = r.out.resume_handle = &handle;
	r.out.entries_read = &entries_read;

	status = dcerpc_wkssvc_NetrUseEnum_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status,
				   "NetrUseEnum failed");
	torture_assert_werr_ok(tctx, r.out.result,
			       "NetrUseEnum failed");

	for (k=0; k < r.out.info->ctr.ctr0->count; k++) {

		use_name = r.out.info->ctr.ctr0->array[k].local;

		for (i=0; i<ARRAY_SIZE(levels); i++) {

			if (!test_NetrUseGetInfo_level(tctx, p, use_name,
						       levels[i],
						       WERR_OK))
			{
				if (levels[i] != 0) {
					return false;
				}
			}
		}

		use_name = r.out.info->ctr.ctr0->array[k].remote;

		for (i=0; i<ARRAY_SIZE(levels); i++) {

			if (!test_NetrUseGetInfo_level(tctx, p, use_name,
						       levels[i],
						       WERR_NOT_CONNECTED))
			{
				if (levels[i] != 0) {
					return false;
				}
			}
		}
	}

	return true;
}

static bool test_NetrLogonDomainNameAdd(struct torture_context *tctx,
					struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct wkssvc_NetrLogonDomainNameAdd r;
	struct dcerpc_binding_handle *b = p->binding_handle;

	r.in.domain_name = lpcfg_workgroup(tctx->lp_ctx);

	torture_comment(tctx, "Testing NetrLogonDomainNameAdd\n");

	status = dcerpc_wkssvc_NetrLogonDomainNameAdd_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status,
				   "NetrLogonDomainNameAdd failed");
	torture_assert_werr_equal(tctx, r.out.result, WERR_NOT_SUPPORTED,
				  "NetrLogonDomainNameAdd failed");
	return true;
}

static bool test_NetrLogonDomainNameDel(struct torture_context *tctx,
					struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct wkssvc_NetrLogonDomainNameDel r;
	struct dcerpc_binding_handle *b = p->binding_handle;

	r.in.domain_name = lpcfg_workgroup(tctx->lp_ctx);

	torture_comment(tctx, "Testing NetrLogonDomainNameDel\n");

	status = dcerpc_wkssvc_NetrLogonDomainNameDel_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status,
				   "NetrLogonDomainNameDel failed");
	torture_assert_werr_equal(tctx, r.out.result, WERR_NOT_SUPPORTED,
				  "NetrLogonDomainNameDel failed");
	return true;
}

static bool test_NetrEnumerateComputerNames_level(struct torture_context *tctx,
						  struct dcerpc_pipe *p,
						  uint16_t level,
						  const char ***names,
						  int *num_names)
{
	NTSTATUS status;
	struct wkssvc_NetrEnumerateComputerNames r;
	struct wkssvc_ComputerNamesCtr *ctr;
	int i;
	struct dcerpc_binding_handle *b = p->binding_handle;

	ctr = talloc_zero(tctx, struct wkssvc_ComputerNamesCtr);

	r.in.server_name = dcerpc_server_name(p);
	r.in.name_type = level;
	r.in.Reserved = 0;
	r.out.ctr = &ctr;

	torture_comment(tctx, "Testing NetrEnumerateComputerNames level %u\n",
			r.in.name_type);

	status = dcerpc_wkssvc_NetrEnumerateComputerNames_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status,
				   "NetrEnumerateComputerNames failed");
	torture_assert_werr_ok(tctx, r.out.result,
			       "NetrEnumerateComputerNames failed");

	if ((level == NetPrimaryComputerName) && ctr->count != 1) {
		torture_comment(tctx,
				"NetrEnumerateComputerNames did not return one "
				"name but %u\n", ctr->count);
		return false;
	}

	if (names && num_names) {
		*num_names = 0;
		*names = NULL;
		for (i=0; i<ctr->count; i++) {
			if (!add_string_to_array(tctx,
						 ctr->computer_name[i].string,
						 names,
						 num_names))
			{
				return false;
			}
		}
	}

	return true;
}

static bool test_NetrEnumerateComputerNames(struct torture_context *tctx,
					    struct dcerpc_pipe *p)
{
	uint16_t levels[] = {0,1,2};
	int i;

	for (i=0; i<ARRAY_SIZE(levels); i++) {

		if (!test_NetrEnumerateComputerNames_level(tctx,
							   p,
							   levels[i],
							   NULL, NULL))
		{
			return false;
		}
	}

	return true;
}

static bool test_NetrValidateName(struct torture_context *tctx,
				  struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct wkssvc_NetrValidateName r;
	uint16_t levels[] = {0,1,2,3,4,5};
	int i;
	struct dcerpc_binding_handle *b = p->binding_handle;

	for (i=0; i<ARRAY_SIZE(levels); i++) {

		r.in.server_name = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
		r.in.name = lpcfg_workgroup(tctx->lp_ctx);
		r.in.Account = NULL;
		r.in.Password = NULL;
		r.in.name_type = levels[i];

		torture_comment(tctx, "Testing NetrValidateName level %u\n",
				r.in.name_type);

		status = dcerpc_wkssvc_NetrValidateName_r(b, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status,
					   "NetrValidateName failed");
		torture_assert_werr_equal(tctx, r.out.result,
					  WERR_NOT_SUPPORTED,
					  "NetrValidateName failed");
	}

	return true;
}

static bool test_NetrValidateName2(struct torture_context *tctx,
				   struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct wkssvc_NetrValidateName2 r;
	uint16_t levels[] = {0,1,2,3,4,5};
	int i;
	struct dcerpc_binding_handle *b = p->binding_handle;

	for (i=0; i<ARRAY_SIZE(levels); i++) {

		r.in.server_name = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
		r.in.name = lpcfg_workgroup(tctx->lp_ctx);
		r.in.Account = NULL;
		r.in.EncryptedPassword = NULL;
		r.in.name_type = levels[i];

		torture_comment(tctx, "Testing NetrValidateName2 level %u\n",
				r.in.name_type);

		status = dcerpc_wkssvc_NetrValidateName2_r(b, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status,
					   "NetrValidateName2 failed");
		torture_assert_werr_equal(tctx, r.out.result,
					  WERR_RPC_E_REMOTE_DISABLED,
					  "NetrValidateName2 failed");
	}

	return true;
}

static bool test_NetrAddAlternateComputerName(struct torture_context *tctx,
					      struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct wkssvc_NetrAddAlternateComputerName r;
	const char **names = NULL;
	int num_names = 0;
	int i;
	struct dcerpc_binding_handle *b = p->binding_handle;

	r.in.server_name = dcerpc_server_name(p);
	r.in.NewAlternateMachineName = SMBTORTURE_ALTERNATE_NAME;
	r.in.Account = NULL;
	r.in.EncryptedPassword = NULL;
	r.in.Reserved = 0;

	torture_comment(tctx, "Testing NetrAddAlternateComputerName\n");

	status = dcerpc_wkssvc_NetrAddAlternateComputerName_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status,
				   "NetrAddAlternateComputerName failed");
	torture_assert_werr_ok(tctx, r.out.result,
			       "NetrAddAlternateComputerName failed");

	if (!test_NetrEnumerateComputerNames_level(tctx, p,
						   NetAlternateComputerNames,
						   &names, &num_names))
	{
		return false;
	}

	for (i=0; i<num_names; i++) {
		if (strequal(names[i], SMBTORTURE_ALTERNATE_NAME)) {
			return true;
		}
	}

	torture_comment(tctx, "new alternate name not set\n");

	return false;
}

static bool test_NetrRemoveAlternateComputerName(struct torture_context *tctx,
						 struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct wkssvc_NetrRemoveAlternateComputerName r;
	const char **names = NULL;
	int num_names = 0;
	int i;
	struct dcerpc_binding_handle *b = p->binding_handle;

	r.in.server_name = dcerpc_server_name(p);
	r.in.AlternateMachineNameToRemove = SMBTORTURE_ALTERNATE_NAME;
	r.in.Account = NULL;
	r.in.EncryptedPassword = NULL;
	r.in.Reserved = 0;

	torture_comment(tctx, "Testing NetrRemoveAlternateComputerName\n");

	status = dcerpc_wkssvc_NetrRemoveAlternateComputerName_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status,
				   "NetrRemoveAlternateComputerName failed");
	torture_assert_werr_ok(tctx, r.out.result,
			       "NetrRemoveAlternateComputerName failed");

	if (!test_NetrEnumerateComputerNames_level(tctx, p,
						   NetAlternateComputerNames,
						   &names, &num_names))
	{
		return false;
	}

	for (i=0; i<num_names; i++) {
		if (strequal(names[i], SMBTORTURE_ALTERNATE_NAME)) {
			return false;
		}
	}

	return true;
}

static bool test_NetrSetPrimaryComputername_name(struct torture_context *tctx,
						 struct dcerpc_pipe *p,
						 const char *name)
{
	NTSTATUS status;
	struct wkssvc_NetrSetPrimaryComputername r;
	struct dcerpc_binding_handle *b = p->binding_handle;

	r.in.server_name = dcerpc_server_name(p);
	r.in.primary_name = name;
	r.in.Account = NULL;
	r.in.EncryptedPassword = NULL;
	r.in.Reserved = 0;

	status = dcerpc_wkssvc_NetrSetPrimaryComputername_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status,
				   "NetrSetPrimaryComputername failed");
	torture_assert_werr_ok(tctx, r.out.result,
			       "NetrSetPrimaryComputername failed");
	return true;
}


static bool test_NetrSetPrimaryComputername(struct torture_context *tctx,
					    struct dcerpc_pipe *p)
{
	/*
	  add alternate,
	  check if there
	  store old primary
	  set new primary (alternate)
	  check if there
	  later: check if del is possible
	  set primary back to origin
	  check if there
	  del alternate
	*/

	const char **names_o = NULL, **names = NULL;
	int num_names_o = 0, num_names = 0;

	torture_comment(tctx, "Testing NetrSetPrimaryComputername\n");

	if (!test_NetrAddAlternateComputerName(tctx, p)) {
		return false;
	}

	if (!test_NetrEnumerateComputerNames_level(tctx, p,
						   NetPrimaryComputerName,
						   &names_o, &num_names_o))
	{
		return false;
	}

	if (num_names_o != 1) {
		return false;
	}

	if (!test_NetrSetPrimaryComputername_name(tctx, p,
						  SMBTORTURE_ALTERNATE_NAME))
	{
		return false;
	}

	if (!test_NetrEnumerateComputerNames_level(tctx, p,
						   NetPrimaryComputerName,
						   &names, &num_names))
	{
		return false;
	}

	if (num_names != 1) {
		return false;
	}

	if (!strequal(names[0], SMBTORTURE_ALTERNATE_NAME)) {
		torture_comment(tctx,
				"name mismatch (%s != %s) after NetrSetPrimaryComputername!\n",
				names[0], SMBTORTURE_ALTERNATE_NAME);
		/*return false */;
	}

	if (!test_NetrSetPrimaryComputername_name(tctx, p,
						  names_o[0]))
	{
		return false;
	}

	if (!test_NetrRemoveAlternateComputerName(tctx, p)) {
		return false;
	}


	return true;
}

static bool test_NetrRenameMachineInDomain(struct torture_context *tctx,
					   struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct wkssvc_NetrRenameMachineInDomain r;
	struct dcerpc_binding_handle *b = p->binding_handle;

	r.in.server_name = dcerpc_server_name(p);
	r.in.NewMachineName = SMBTORTURE_MACHINE_NAME;
	r.in.Account = NULL;
	r.in.password = NULL;
	r.in.RenameOptions = 0;

	torture_comment(tctx, "Testing NetrRenameMachineInDomain\n");

	status = dcerpc_wkssvc_NetrRenameMachineInDomain_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status,
				   "NetrRenameMachineInDomain failed");
	torture_assert_werr_equal(tctx, r.out.result, WERR_NOT_SUPPORTED,
				  "NetrRenameMachineInDomain failed");
	return true;
}

static bool test_NetrRenameMachineInDomain2_name(struct torture_context *tctx,
						 struct dcerpc_pipe *p,
						 const char *new_name)
{
	NTSTATUS status;
	struct wkssvc_NetrRenameMachineInDomain2 r;
	struct dcerpc_binding_handle *b = p->binding_handle;

	r.in.server_name = dcerpc_server_name(p);
	r.in.NewMachineName = new_name;
	r.in.Account = NULL;
	r.in.EncryptedPassword = NULL;
	r.in.RenameOptions = 0;

	status = dcerpc_wkssvc_NetrRenameMachineInDomain2_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status,
				   "NetrRenameMachineInDomain2 failed");
	torture_assert_werr_ok(tctx, r.out.result,
			       "NetrRenameMachineInDomain2 failed");
	return true;
}

static bool test_NetrRenameMachineInDomain2(struct torture_context *tctx,
					    struct dcerpc_pipe *p)
{
	const char **names_o = NULL, **names = NULL;
	int num_names_o = 0, num_names = 0;

	torture_comment(tctx, "Testing NetrRenameMachineInDomain2\n");

	if (!test_NetrEnumerateComputerNames_level(tctx, p,
						   NetPrimaryComputerName,
						   &names_o, &num_names_o))
	{
		return false;
	}

	if (num_names_o != 1) {
		return false;
	}

	if (!test_NetrRenameMachineInDomain2_name(tctx, p,
						  SMBTORTURE_MACHINE_NAME))
	{
		return false;
	}

	if (!test_NetrEnumerateComputerNames_level(tctx, p,
						   NetPrimaryComputerName,
						   &names, &num_names))
	{
		return false;
	}

	if (num_names != 1) {
		return false;
	}

	if (strequal(names[0], names_o[0])) {
		test_NetrRenameMachineInDomain2_name(tctx, p, names_o[0]);
		return false;
	}

	if (!strequal(names[0], SMBTORTURE_MACHINE_NAME)) {
		test_NetrRenameMachineInDomain2_name(tctx, p, names_o[0]);
		return false;
	}

	if (!test_NetrRenameMachineInDomain2_name(tctx, p, names_o[0]))
	{
		return false;
	}

	if (!test_NetrEnumerateComputerNames_level(tctx, p,
						   NetPrimaryComputerName,
						   &names, &num_names))
	{
		return false;
	}

	if (num_names != 1) {
		return false;
	}

	if (!strequal(names[0], names_o[0])) {
		return false;
	}

	return true;
}

static bool test_NetrWorkstationStatisticsGet(struct torture_context *tctx,
					      struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct wkssvc_NetrWorkstationStatisticsGet r;
	struct wkssvc_NetrWorkstationStatistics *info;
	struct dcerpc_binding_handle *b = p->binding_handle;

	ZERO_STRUCT(r);

	info = talloc_zero(tctx, struct wkssvc_NetrWorkstationStatistics);

	r.in.server_name = dcerpc_server_name(p);
	r.out.info = &info;

	torture_comment(tctx, "Testing NetrWorkstationStatisticsGet\n");

	status = dcerpc_wkssvc_NetrWorkstationStatisticsGet_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status,
				   "NetrWorkstationStatisticsGet failed");
	torture_assert_werr_ok(tctx, r.out.result,
			       "NetrWorkstationStatisticsGet failed");
	return true;
}

/* only succeeds as long as the local messenger service is running - Guenther */

static bool test_NetrMessageBufferSend(struct torture_context *tctx,
				       struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct wkssvc_NetrMessageBufferSend r;
	const char *message = SMBTORTURE_MESSAGE;
	size_t size;
	uint16_t *msg;
	struct dcerpc_binding_handle *b = p->binding_handle;

	if (!push_ucs2_talloc(tctx, &msg, message, &size)) {
		return false;
	}

	r.in.server_name = dcerpc_server_name(p);
	r.in.message_name = dcerpc_server_name(p);
	r.in.message_sender_name = dcerpc_server_name(p);
	r.in.message_buffer = (uint8_t *)msg;
	r.in.message_size = size;

	torture_comment(tctx, "Testing NetrMessageBufferSend\n");

	status = dcerpc_wkssvc_NetrMessageBufferSend_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status,
				   "NetrMessageBufferSend failed");
	torture_assert_werr_ok(tctx, r.out.result,
			       "NetrMessageBufferSend failed");
	return true;
}

static bool test_NetrGetJoinInformation(struct torture_context *tctx,
					struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct wkssvc_NetrGetJoinInformation r;
	enum wkssvc_NetJoinStatus join_status;
	const char *name_buffer = "";
	struct dcerpc_binding_handle *b = p->binding_handle;

	r.in.server_name = dcerpc_server_name(p);
	r.in.name_buffer = r.out.name_buffer = &name_buffer;
	r.out.name_type = &join_status;

	torture_comment(tctx, "Testing NetrGetJoinInformation\n");

	status = dcerpc_wkssvc_NetrGetJoinInformation_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status,
				   "NetrGetJoinInformation failed");
	torture_assert_werr_ok(tctx, r.out.result,
			       "NetrGetJoinInformation failed");
	return true;
}

static bool test_GetJoinInformation(struct torture_context *tctx,
				    struct dcerpc_pipe *p,
				    enum wkssvc_NetJoinStatus *join_status_p,
				    const char **name)
{
	NTSTATUS status;
	struct wkssvc_NetrGetJoinInformation r;
	enum wkssvc_NetJoinStatus join_status;
	const char *name_buffer = "";
	struct dcerpc_binding_handle *b = p->binding_handle;

	r.in.server_name = dcerpc_server_name(p);
	r.in.name_buffer = r.out.name_buffer = &name_buffer;
	r.out.name_type = &join_status;

	status = dcerpc_wkssvc_NetrGetJoinInformation_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status,
				   "NetrGetJoinInformation failed");
	torture_assert_werr_ok(tctx, r.out.result,
			       "NetrGetJoinInformation failed");

	if (join_status_p) {
		*join_status_p = join_status;
	}

	if (*name) {
		*name = talloc_strdup(tctx, name_buffer);
	}

	return true;

}

static bool test_NetrGetJoinableOus(struct torture_context *tctx,
				    struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct wkssvc_NetrGetJoinableOus r;
	uint32_t num_ous = 0;
	const char **ous = NULL;
	struct dcerpc_binding_handle *b = p->binding_handle;

	r.in.server_name = dcerpc_server_name(p);
	r.in.domain_name = lpcfg_workgroup(tctx->lp_ctx);
	r.in.Account = NULL;
	r.in.unknown = NULL;
	r.in.num_ous = r.out.num_ous = &num_ous;
	r.out.ous = &ous;

	torture_comment(tctx, "Testing NetrGetJoinableOus\n");

	status = dcerpc_wkssvc_NetrGetJoinableOus_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "NetrGetJoinableOus failed");
	torture_assert_werr_equal(tctx, r.out.result,
				  WERR_NOT_SUPPORTED,
				  "NetrGetJoinableOus failed");

	return true;
}

static bool test_NetrGetJoinableOus2(struct torture_context *tctx,
				     struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct wkssvc_NetrGetJoinableOus2 r;
	uint32_t num_ous = 0;
	const char **ous = NULL;
	struct dcerpc_binding_handle *b = p->binding_handle;

	r.in.server_name = dcerpc_server_name(p);
	r.in.domain_name = lpcfg_workgroup(tctx->lp_ctx);
	r.in.Account = NULL;
	r.in.EncryptedPassword = NULL;
	r.in.num_ous = r.out.num_ous = &num_ous;
	r.out.ous = &ous;

	torture_comment(tctx, "Testing NetrGetJoinableOus2\n");

	status = dcerpc_wkssvc_NetrGetJoinableOus2_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "NetrGetJoinableOus2 failed");
	torture_assert_werr_equal(tctx, r.out.result,
				  WERR_RPC_E_REMOTE_DISABLED,
				  "NetrGetJoinableOus2 failed");

	return true;
}

static bool test_NetrUnjoinDomain(struct torture_context *tctx,
				  struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct wkssvc_NetrUnjoinDomain r;
	struct cli_credentials *creds = cmdline_credentials;
	const char *user = cli_credentials_get_username(creds);
	const char *admin_account = NULL;
	struct dcerpc_binding_handle *b = p->binding_handle;

	admin_account = talloc_asprintf(tctx, "%s\\%s",
					lpcfg_workgroup(tctx->lp_ctx),
					user);

	r.in.server_name = dcerpc_server_name(p);
	r.in.Account = admin_account;
	r.in.password = NULL;
	r.in.unjoin_flags = 0;

	torture_comment(tctx, "Testing NetrUnjoinDomain\n");

	status = dcerpc_wkssvc_NetrUnjoinDomain_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status,
				   "NetrUnjoinDomain failed");
	torture_assert_werr_equal(tctx, r.out.result, WERR_NOT_SUPPORTED,
				  "NetrUnjoinDomain failed");
	return true;
}

static bool test_NetrJoinDomain(struct torture_context *tctx,
				struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct wkssvc_NetrJoinDomain r;
	struct cli_credentials *creds = cmdline_credentials;
	const char *user = cli_credentials_get_username(creds);
	const char *admin_account = NULL;
	struct dcerpc_binding_handle *b = p->binding_handle;

	admin_account = talloc_asprintf(tctx, "%s\\%s",
					lpcfg_workgroup(tctx->lp_ctx),
					user);

	r.in.server_name = dcerpc_server_name(p);
	r.in.domain_name = lpcfg_dnsdomain(tctx->lp_ctx);
	r.in.account_ou = NULL;
	r.in.Account = admin_account;
	r.in.password = NULL;
	r.in.join_flags = 0;

	torture_comment(tctx, "Testing NetrJoinDomain\n");

	status = dcerpc_wkssvc_NetrJoinDomain_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status,
				   "NetrJoinDomain failed");
	torture_assert_werr_equal(tctx, r.out.result, WERR_NOT_SUPPORTED,
				  "NetrJoinDomain failed");
	return true;
}

/*
 * prerequisites for remotely joining an unjoined XP SP2 workstation:
 * - firewall needs to be disabled (or open for ncacn_np access)
 * - HKLM\System\CurrentControlSet\Control\Lsa\forceguest needs to 0
 * see also:
 * http://support.microsoft.com/kb/294355/EN-US/ and
 * http://support.microsoft.com/kb/290403/EN-US/
 */

static bool test_NetrJoinDomain2(struct torture_context *tctx,
				 struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct wkssvc_NetrJoinDomain2 r;
	const char *domain_admin_account = NULL;
	const char *domain_admin_password = NULL;
	const char *domain_name = NULL;
	struct wkssvc_PasswordBuffer *pwd_buf;
	enum wkssvc_NetJoinStatus join_status;
	const char *join_name = NULL;
	WERROR expected_err;
	DATA_BLOB session_key;
	struct dcerpc_binding_handle *b = p->binding_handle;

	/* FIXME: this test assumes to join workstations / servers and does not
	 * handle DCs (WERR_SETUP_DOMAIN_CONTROLLER) */

	if (!test_GetJoinInformation(tctx, p, &join_status, &join_name))
	{
		return false;
	}

	switch (join_status) {
		case NET_SETUP_DOMAIN_NAME:
			expected_err = WERR_SETUP_ALREADY_JOINED;
			break;
		case NET_SETUP_UNKNOWN_STATUS:
		case NET_SETUP_UNJOINED:
		case NET_SETUP_WORKGROUP_NAME:
		default:
			expected_err = WERR_OK;
			break;
	}

	domain_admin_account = torture_setting_string(tctx, "domain_admin_account", NULL);

	domain_admin_password = torture_setting_string(tctx, "domain_admin_password", NULL);

	domain_name = torture_setting_string(tctx, "domain_name", NULL);

	if ((domain_admin_account == NULL) ||
	    (domain_admin_password == NULL) ||
	    (domain_name == NULL)) {
		torture_comment(tctx, "not enough input parameter\n");
	    	return false;
	}

	status = dcerpc_fetch_session_key(p, &session_key);
	if (!NT_STATUS_IS_OK(status)) {
		return false;
	}

	encode_wkssvc_join_password_buffer(tctx, domain_admin_password,
					   &session_key, &pwd_buf);

	r.in.server_name = dcerpc_server_name(p);
	r.in.domain_name = domain_name;
	r.in.account_ou = NULL;
	r.in.admin_account = domain_admin_account;
	r.in.encrypted_password = pwd_buf;
	r.in.join_flags = WKSSVC_JOIN_FLAGS_JOIN_TYPE |
			  WKSSVC_JOIN_FLAGS_ACCOUNT_CREATE;

	torture_comment(tctx, "Testing NetrJoinDomain2 (assuming non-DC)\n");

	status = dcerpc_wkssvc_NetrJoinDomain2_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status,
				   "NetrJoinDomain2 failed");
	torture_assert_werr_equal(tctx, r.out.result, expected_err,
				  "NetrJoinDomain2 failed");

	if (!test_GetJoinInformation(tctx, p, &join_status, &join_name))
	{
		return false;
	}

	if (join_status != NET_SETUP_DOMAIN_NAME) {
		torture_comment(tctx,
				"Join verify failed: got %d\n", join_status);
		return false;
	}

	return true;
}

static bool test_NetrUnjoinDomain2(struct torture_context *tctx,
				   struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct wkssvc_NetrUnjoinDomain2 r;
	const char *domain_admin_account = NULL;
	const char *domain_admin_password = NULL;
	struct wkssvc_PasswordBuffer *pwd_buf;
	enum wkssvc_NetJoinStatus join_status;
	const char *join_name = NULL;
	WERROR expected_err;
	DATA_BLOB session_key;
	struct dcerpc_binding_handle *b = p->binding_handle;

	/* FIXME: this test assumes to join workstations / servers and does not
	 * handle DCs (WERR_SETUP_DOMAIN_CONTROLLER) */

	if (!test_GetJoinInformation(tctx, p, &join_status, &join_name))
	{
		return false;
	}

	switch (join_status) {
		case NET_SETUP_UNJOINED:
			expected_err = WERR_SETUP_NOT_JOINED;
			break;
		case NET_SETUP_DOMAIN_NAME:
		case NET_SETUP_UNKNOWN_STATUS:
		case NET_SETUP_WORKGROUP_NAME:
		default:
			expected_err = WERR_OK;
			break;
	}

	domain_admin_account = torture_setting_string(tctx, "domain_admin_account", NULL);

	domain_admin_password = torture_setting_string(tctx, "domain_admin_password", NULL);

	if ((domain_admin_account == NULL) ||
	    (domain_admin_password == NULL)) {
		torture_comment(tctx, "not enough input parameter\n");
	    	return false;
	}

	status = dcerpc_fetch_session_key(p, &session_key);
	if (!NT_STATUS_IS_OK(status)) {
		return false;
	}

	encode_wkssvc_join_password_buffer(tctx, domain_admin_password,
					   &session_key, &pwd_buf);

	r.in.server_name = dcerpc_server_name(p);
	r.in.account = domain_admin_account;
	r.in.encrypted_password = pwd_buf;
	r.in.unjoin_flags = 0;

	torture_comment(tctx, "Testing NetrUnjoinDomain2 (assuming non-DC)\n");

	status = dcerpc_wkssvc_NetrUnjoinDomain2_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status,
				   "NetrUnjoinDomain2 failed");
	torture_assert_werr_equal(tctx, r.out.result, expected_err,
				  "NetrUnjoinDomain2 failed");

	if (!test_GetJoinInformation(tctx, p, &join_status, &join_name))
	{
		return false;
	}

	switch (join_status) {
		case NET_SETUP_UNJOINED:
		case NET_SETUP_WORKGROUP_NAME:
			break;
		case NET_SETUP_UNKNOWN_STATUS:
		case NET_SETUP_DOMAIN_NAME:
		default:
			torture_comment(tctx,
				"Unjoin verify failed: got %d\n", join_status);
			return false;
	}

	return true;
}


struct torture_suite *torture_rpc_wkssvc(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite;
	struct torture_rpc_tcase *tcase;
	struct torture_test *test;

	suite = torture_suite_create(mem_ctx, "wkssvc");
	tcase = torture_suite_add_rpc_iface_tcase(suite, "wkssvc",
						  &ndr_table_wkssvc);

	torture_rpc_tcase_add_test(tcase, "NetWkstaGetInfo",
				   test_NetWkstaGetInfo);

	torture_rpc_tcase_add_test(tcase, "NetWkstaTransportEnum",
				   test_NetWkstaTransportEnum);
	torture_rpc_tcase_add_test(tcase, "NetrWkstaTransportDel",
				   test_NetrWkstaTransportDel);
	torture_rpc_tcase_add_test(tcase, "NetrWkstaTransportAdd",
				   test_NetrWkstaTransportAdd);

	torture_rpc_tcase_add_test(tcase, "NetWkstaEnumUsers",
				   test_NetWkstaEnumUsers);
	torture_rpc_tcase_add_test(tcase, "NetrWkstaUserGetInfo",
				   test_NetrWkstaUserGetInfo);

	torture_rpc_tcase_add_test(tcase, "NetrUseDel",
				   test_NetrUseDel);
	torture_rpc_tcase_add_test(tcase, "NetrUseGetInfo",
				   test_NetrUseGetInfo);
	torture_rpc_tcase_add_test(tcase, "NetrUseEnum",
				   test_NetrUseEnum);
	torture_rpc_tcase_add_test(tcase, "NetrUseAdd",
				   test_NetrUseAdd);

	torture_rpc_tcase_add_test(tcase, "NetrValidateName",
				   test_NetrValidateName);
	torture_rpc_tcase_add_test(tcase, "NetrValidateName2",
				   test_NetrValidateName2);
	torture_rpc_tcase_add_test(tcase, "NetrLogonDomainNameDel",
				   test_NetrLogonDomainNameDel);
	torture_rpc_tcase_add_test(tcase, "NetrLogonDomainNameAdd",
				   test_NetrLogonDomainNameAdd);
	torture_rpc_tcase_add_test(tcase, "NetrRemoveAlternateComputerName",
				   test_NetrRemoveAlternateComputerName);
	torture_rpc_tcase_add_test(tcase, "NetrAddAlternateComputerName",
				   test_NetrAddAlternateComputerName);
	test = torture_rpc_tcase_add_test(tcase, "NetrSetPrimaryComputername",
					  test_NetrSetPrimaryComputername);
	test->dangerous = true;
	test = torture_rpc_tcase_add_test(tcase, "NetrRenameMachineInDomain",
					  test_NetrRenameMachineInDomain);
	test->dangerous = true;
	test = torture_rpc_tcase_add_test(tcase, "NetrRenameMachineInDomain2",
					  test_NetrRenameMachineInDomain2);
	test->dangerous = true;
	torture_rpc_tcase_add_test(tcase, "NetrEnumerateComputerNames",
				   test_NetrEnumerateComputerNames);

	test = torture_rpc_tcase_add_test(tcase, "NetrJoinDomain2",
					  test_NetrJoinDomain2);
	test->dangerous = true;
	test = torture_rpc_tcase_add_test(tcase, "NetrUnjoinDomain2",
					  test_NetrUnjoinDomain2);
	test->dangerous = true;

	torture_rpc_tcase_add_test(tcase, "NetrJoinDomain",
				   test_NetrJoinDomain);
	test->dangerous = true;
	torture_rpc_tcase_add_test(tcase, "NetrUnjoinDomain",
				   test_NetrUnjoinDomain);
	test->dangerous = true;
	torture_rpc_tcase_add_test(tcase, "NetrGetJoinInformation",
				   test_NetrGetJoinInformation);
	torture_rpc_tcase_add_test(tcase, "NetrGetJoinableOus",
				   test_NetrGetJoinableOus);
	torture_rpc_tcase_add_test(tcase, "NetrGetJoinableOus2",
				   test_NetrGetJoinableOus2);

	torture_rpc_tcase_add_test(tcase, "NetrWorkstationStatisticsGet",
				   test_NetrWorkstationStatisticsGet);
	torture_rpc_tcase_add_test(tcase, "NetrMessageBufferSend",
				   test_NetrMessageBufferSend);

	return suite;
}
