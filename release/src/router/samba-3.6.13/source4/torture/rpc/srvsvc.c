/* 
   Unix SMB/CIFS implementation.
   test suite for srvsvc rpc operations

   Copyright (C) Stefan (metze) Metzmacher 2003
   
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
#include "librpc/gen_ndr/ndr_srvsvc_c.h"
#include "torture/rpc/torture_rpc.h"

/**************************/
/* srvsvc_NetCharDev      */
/**************************/
static bool test_NetCharDevGetInfo(struct dcerpc_pipe *p, struct torture_context *tctx,
				const char *devname)
{
	NTSTATUS status;
	struct srvsvc_NetCharDevGetInfo r;
	union srvsvc_NetCharDevInfo info;
	uint32_t levels[] = {0, 1};
	int i;
	struct dcerpc_binding_handle *b = p->binding_handle;

	r.in.server_unc = talloc_asprintf(tctx,"\\\\%s",dcerpc_server_name(p));
	r.in.device_name = devname;
	r.out.info = &info;

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		r.in.level = levels[i];
		torture_comment(tctx, "Testing NetCharDevGetInfo level %u on device '%s'\n",
			r.in.level, r.in.device_name);
		status = dcerpc_srvsvc_NetCharDevGetInfo_r(b, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "NetCharDevGetInfo failed");
		torture_assert_werr_ok(tctx, r.out.result, "NetCharDevGetInfo failed");
	}

	return true;
}

static bool test_NetCharDevControl(struct dcerpc_pipe *p, struct torture_context *tctx,
				const char *devname)
{
	NTSTATUS status;
	struct srvsvc_NetCharDevControl r;
	uint32_t opcodes[] = {0, 1};
	int i;
	struct dcerpc_binding_handle *b = p->binding_handle;

	r.in.server_unc = talloc_asprintf(tctx,"\\\\%s",dcerpc_server_name(p));
	r.in.device_name = devname;

	for (i=0;i<ARRAY_SIZE(opcodes);i++) {
		ZERO_STRUCT(r.out);
		r.in.opcode = opcodes[i];
		torture_comment(tctx, "Testing NetCharDevControl opcode %u on device '%s'\n", 
			r.in.opcode, r.in.device_name);
		status = dcerpc_srvsvc_NetCharDevControl_r(b, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "NetCharDevControl failed");
		torture_assert_werr_ok(tctx, r.out.result, "NetCharDevControl failed");
	}

	return true;
}

static bool test_NetCharDevEnum(struct torture_context *tctx, 
								struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct srvsvc_NetCharDevEnum r;
	struct srvsvc_NetCharDevInfoCtr info_ctr;
	struct srvsvc_NetCharDevCtr0 c0;
	struct srvsvc_NetCharDevCtr0 c1;
	uint32_t totalentries = 0;
	uint32_t levels[] = {0, 1};
	int i;
	struct dcerpc_binding_handle *b = p->binding_handle;

	ZERO_STRUCT(info_ctr);

	r.in.server_unc = talloc_asprintf(tctx,"\\\\%s",dcerpc_server_name(p));
	r.in.info_ctr = &info_ctr;
	r.in.max_buffer = (uint32_t)-1;
	r.in.resume_handle = NULL;
	r.out.info_ctr = &info_ctr;
	r.out.totalentries = &totalentries;

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		int j;

		info_ctr.level = levels[i];

		switch(info_ctr.level) {
		case 0:
			ZERO_STRUCT(c0);
			info_ctr.ctr.ctr0 = &c0;
			break;
		case 1:
			ZERO_STRUCT(c1);
			info_ctr.ctr.ctr0 = &c1;
			break;
		}

		torture_comment(tctx, "Testing NetCharDevEnum level %u\n", info_ctr.level);
		status = dcerpc_srvsvc_NetCharDevEnum_r(b, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "NetCharDevEnum failed");
		if (!W_ERROR_IS_OK(r.out.result)) {
			torture_comment(tctx, "NetCharDevEnum failed: %s\n", win_errstr(r.out.result));
			continue;
		}

		/* call test_NetCharDevGetInfo and test_NetCharDevControl for each returned share */
		if (info_ctr.level == 1) {
			for (j=0;j<r.out.info_ctr->ctr.ctr1->count;j++) {
				const char *device;
				device = r.out.info_ctr->ctr.ctr1->array[j].device;
				if (!test_NetCharDevGetInfo(p, tctx, device)) {
					return false;
				}
				if (!test_NetCharDevControl(p, tctx, device)) {
					return false;
				}
			}
		}
	}

	return true;
}

/**************************/
/* srvsvc_NetCharDevQ     */
/**************************/
static bool test_NetCharDevQGetInfo(struct dcerpc_pipe *p, struct torture_context *tctx,
				const char *devicequeue)
{
	NTSTATUS status;
	struct srvsvc_NetCharDevQGetInfo r;
	union srvsvc_NetCharDevQInfo info;
	uint32_t levels[] = {0, 1};
	int i;
	struct dcerpc_binding_handle *b = p->binding_handle;

	r.in.server_unc = talloc_asprintf(tctx,"\\\\%s",dcerpc_server_name(p));
	r.in.queue_name = devicequeue;
	r.in.user = talloc_asprintf(tctx,"Administrator");
	r.out.info = &info;

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		r.in.level = levels[i];
		torture_comment(tctx, "Testing NetCharDevQGetInfo level %u on devicequeue '%s'\n",
			r.in.level, r.in.queue_name);
		status = dcerpc_srvsvc_NetCharDevQGetInfo_r(b, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "NetCharDevQGetInfo failed");
		torture_assert_werr_ok(tctx, r.out.result, "NetCharDevQGetInfo failed");
	}

	return true;
}

#if 0
static bool test_NetCharDevQSetInfo(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx,
				const char *devicequeue)
{
	NTSTATUS status;
	struct srvsvc_NetCharDevQSetInfo r;
	uint32_t parm_error;
	uint32_t levels[] = {0, 1};
	int i;
	bool ret = true;
	struct dcerpc_binding_handle *b = p->binding_handle;

	r.in.server_unc = talloc_asprintf(mem_ctx,"\\\\%s",dcerpc_server_name(p));
	r.in.queue_name = devicequeue;

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		ZERO_STRUCT(r.out);
		parm_error = 0;
		r.in.level = levels[i];
		d_printf("testing NetCharDevQSetInfo level %u on devicequeue '%s'\n", 
			r.in.level, devicequeue);
		switch (r.in.level) {
		case 0:
			r.in.info.info0 = talloc(mem_ctx, struct srvsvc_NetCharDevQInfo0);
			r.in.info.info0->device = r.in.queue_name;
			break;
		case 1:
			r.in.info.info1 = talloc(mem_ctx, struct srvsvc_NetCharDevQInfo1);
			r.in.info.info1->device = r.in.queue_name;
			r.in.info.info1->priority = 0x000;
			r.in.info.info1->devices = r.in.queue_name;
			r.in.info.info1->users = 0x000;
			r.in.info.info1->num_ahead = 0x000;
			break;
		default:
			break;
		}
		r.in.parm_error = &parm_error;
		status = dcerpc_srvsvc_NetCharDevQSetInfo_r(b, mem_ctx, &r);
		if (!NT_STATUS_IS_OK(status)) {
			d_printf("NetCharDevQSetInfo level %u on devicequeue '%s' failed - %s\n",
				r.in.level, r.in.queue_name, nt_errstr(status));
			ret = false;
			continue;
		}
		if (!W_ERROR_IS_OK(r.out.result)) {
			d_printf("NetCharDevQSetInfo level %u on devicequeue '%s' failed - %s\n",
				r.in.level, r.in.queue_name, win_errstr(r.out.result));
			continue;
		}
	}

	return ret;
}
#endif

static bool test_NetCharDevQEnum(struct torture_context *tctx, 
				 struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct srvsvc_NetCharDevQEnum r;
	struct srvsvc_NetCharDevQInfoCtr info_ctr;
	struct srvsvc_NetCharDevQCtr0 c0;
	struct srvsvc_NetCharDevQCtr1 c1;
	uint32_t totalentries = 0;
	uint32_t levels[] = {0, 1};
	int i;
	struct dcerpc_binding_handle *b = p->binding_handle;

	ZERO_STRUCT(info_ctr);

	r.in.server_unc = talloc_asprintf(tctx,"\\\\%s",dcerpc_server_name(p));
	r.in.user = talloc_asprintf(tctx,"%s","Administrator");
	r.in.info_ctr = &info_ctr;
	r.in.max_buffer = (uint32_t)-1;
	r.in.resume_handle = NULL;
	r.out.totalentries = &totalentries;
	r.out.info_ctr = &info_ctr;

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		int j;

		info_ctr.level = levels[i];

		switch (info_ctr.level) {
		case 0:
			ZERO_STRUCT(c0);
			info_ctr.ctr.ctr0 = &c0;
			break;
		case 1:
			ZERO_STRUCT(c1);
			info_ctr.ctr.ctr1 = &c1;
			break;
		}
		torture_comment(tctx, "Testing NetCharDevQEnum level %u\n", info_ctr.level);
		status = dcerpc_srvsvc_NetCharDevQEnum_r(b, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "NetCharDevQEnum failed");
		if (!W_ERROR_IS_OK(r.out.result)) {
			torture_comment(tctx, "NetCharDevQEnum failed: %s\n", win_errstr(r.out.result));
			continue;
		}

		/* call test_NetCharDevGetInfo and test_NetCharDevControl for each returned share */
		if (info_ctr.level == 1) {
			for (j=0;j<r.out.info_ctr->ctr.ctr1->count;j++) {
				const char *device;
				device = r.out.info_ctr->ctr.ctr1->array[j].device;
				if (!test_NetCharDevQGetInfo(p, tctx, device)) {
					return false;
				}
			}
		}
	}

	return true;
}

/**************************/
/* srvsvc_NetConn         */
/**************************/
static bool test_NetConnEnum(struct torture_context *tctx,
			     struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct srvsvc_NetConnEnum r;
	struct srvsvc_NetConnInfoCtr info_ctr;
	struct srvsvc_NetConnCtr0 c0;
	struct srvsvc_NetConnCtr1 c1;
	uint32_t totalentries = 0;
	uint32_t levels[] = {0, 1};
	int i;
	struct dcerpc_binding_handle *b = p->binding_handle;

	ZERO_STRUCT(info_ctr);

	r.in.server_unc = talloc_asprintf(tctx,"\\\\%s",dcerpc_server_name(p));
	r.in.path = talloc_asprintf(tctx,"%s","IPC$");
	r.in.info_ctr = &info_ctr;
	r.in.max_buffer = (uint32_t)-1;
	r.in.resume_handle = NULL;
	r.out.totalentries = &totalentries;
	r.out.info_ctr = &info_ctr;

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		info_ctr.level = levels[i];

		switch (info_ctr.level) {
		case 0:
			ZERO_STRUCT(c0);
			info_ctr.ctr.ctr0 = &c0;
			break;
		case 1:
			ZERO_STRUCT(c1);
			info_ctr.ctr.ctr1 = &c1;
			break;
		}

		torture_comment(tctx, "Testing NetConnEnum level %u\n", info_ctr.level);
		status = dcerpc_srvsvc_NetConnEnum_r(b, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "NetConnEnum failed");
		if (!W_ERROR_IS_OK(r.out.result)) {
			torture_comment(tctx, "NetConnEnum failed: %s\n", win_errstr(r.out.result));
		}
	}

	return true;
}

/**************************/
/* srvsvc_NetFile         */
/**************************/
static bool test_NetFileEnum(struct torture_context *tctx,
			     struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct srvsvc_NetFileEnum r;
	struct srvsvc_NetFileInfoCtr info_ctr;
	struct srvsvc_NetFileCtr2 c2;
	struct srvsvc_NetFileCtr3 c3;
	uint32_t totalentries = 0;
	uint32_t levels[] = {2, 3};
	int i;
	struct dcerpc_binding_handle *b = p->binding_handle;

	ZERO_STRUCT(info_ctr);

	r.in.server_unc = talloc_asprintf(tctx,"\\\\%s",dcerpc_server_name(p));
	r.in.path = NULL;
	r.in.user = NULL;
	r.in.info_ctr = &info_ctr;
	r.in.max_buffer = (uint32_t)4096;
	r.in.resume_handle = NULL;
	r.out.totalentries = &totalentries;
	r.out.info_ctr = &info_ctr;

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		info_ctr.level = levels[i];

		switch (info_ctr.level) {
		case 2:
			ZERO_STRUCT(c2);
			info_ctr.ctr.ctr2 = &c2;
			break;
		case 3:
			ZERO_STRUCT(c3);
			info_ctr.ctr.ctr3 = &c3;
			break;
		}
		torture_comment(tctx, "Testing NetFileEnum level %u\n", info_ctr.level);
		status = dcerpc_srvsvc_NetFileEnum_r(b, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "NetFileEnum failed");
		if (!W_ERROR_IS_OK(r.out.result)) {
			torture_comment(tctx, "NetFileEnum failed: %s\n", win_errstr(r.out.result));
		}
	}

	return true;
}

/**************************/
/* srvsvc_NetSess         */
/**************************/
static bool test_NetSessEnum(struct torture_context *tctx,
			     struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct srvsvc_NetSessEnum r;
	struct srvsvc_NetSessInfoCtr info_ctr;
	struct srvsvc_NetSessCtr0 c0;
	struct srvsvc_NetSessCtr1 c1;
	struct srvsvc_NetSessCtr2 c2;
	struct srvsvc_NetSessCtr10 c10;
	struct srvsvc_NetSessCtr502 c502;
	uint32_t totalentries = 0;
	uint32_t levels[] = {0, 1, 2, 10, 502};
	int i;
	struct dcerpc_binding_handle *b = p->binding_handle;

	ZERO_STRUCT(info_ctr);

	r.in.server_unc = talloc_asprintf(tctx,"\\\\%s",dcerpc_server_name(p));
	r.in.client = NULL;
	r.in.user = NULL;
	r.in.info_ctr = &info_ctr;
	r.in.max_buffer = (uint32_t)-1;
	r.in.resume_handle = NULL;
	r.out.totalentries = &totalentries;
	r.out.info_ctr = &info_ctr;

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		info_ctr.level = levels[i];

		switch (info_ctr.level) {
		case 0:
			ZERO_STRUCT(c0);
			info_ctr.ctr.ctr0 = &c0;
			break;
		case 1:
			ZERO_STRUCT(c1);
			info_ctr.ctr.ctr1 = &c1;
			break;
		case 2:
			ZERO_STRUCT(c2);
			info_ctr.ctr.ctr2 = &c2;
			break;
		case 10:
			ZERO_STRUCT(c10);
			info_ctr.ctr.ctr10 = &c10;
			break;
		case 502:
			ZERO_STRUCT(c502);
			info_ctr.ctr.ctr502 = &c502;
			break;
		}

		torture_comment(tctx, "Testing NetSessEnum level %u\n", info_ctr.level);
		status = dcerpc_srvsvc_NetSessEnum_r(b, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "NetSessEnum failed");
		if (!W_ERROR_IS_OK(r.out.result)) {
			torture_comment(tctx, "NetSessEnum failed: %s\n", win_errstr(r.out.result));
		}
	}

	return true;
}

/**************************/
/* srvsvc_NetShare        */
/**************************/
static bool test_NetShareCheck(struct dcerpc_pipe *p, struct torture_context *tctx,
			       const char *device_name)
{
	NTSTATUS status;
	struct srvsvc_NetShareCheck r;
	enum srvsvc_ShareType type;
	struct dcerpc_binding_handle *b = p->binding_handle;

	r.in.server_unc = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	r.in.device_name = device_name;
	r.out.type = &type;

	torture_comment(tctx, 
			"Testing NetShareCheck on device '%s'\n", r.in.device_name);

	status = dcerpc_srvsvc_NetShareCheck_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "dcerpc_srvsvc_NetShareCheck failed");
	torture_assert_werr_ok(tctx, r.out.result, "NetShareCheck failed");

	return true;
}

static bool test_NetShareGetInfo(struct torture_context *tctx, 
				 struct dcerpc_pipe *p,
				 const char *sharename, bool admin)
{
	NTSTATUS status;
	struct srvsvc_NetShareGetInfo r;
	union srvsvc_NetShareInfo info;
	struct {
		uint32_t level;
		WERROR anon_status;
		WERROR admin_status;
	} levels[] = {
		 { 0,		WERR_OK,		WERR_OK },
		 { 1,		WERR_OK,		WERR_OK },
		 { 2,		WERR_ACCESS_DENIED,	WERR_OK },
		 { 501,		WERR_OK,		WERR_OK },
		 { 502,		WERR_ACCESS_DENIED,	WERR_OK },
		 { 1005,	WERR_OK,		WERR_OK },
	};
	int i;
	struct dcerpc_binding_handle *b = p->binding_handle;

	r.in.server_unc = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	r.in.share_name = sharename;
	r.out.info = &info;

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		WERROR expected;

		r.in.level = levels[i].level;
		expected = levels[i].anon_status;
		if (admin) expected = levels[i].admin_status;

		torture_comment(tctx, "Testing NetShareGetInfo level %u on share '%s'\n", 
		       r.in.level, r.in.share_name);

		status = dcerpc_srvsvc_NetShareGetInfo_r(b, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "NetShareGetInfo failed");
		torture_assert_werr_equal(tctx, r.out.result, expected, "NetShareGetInfo failed");

		if (r.in.level != 2) continue;
		if (!r.out.info->info2 || !r.out.info->info2->path) continue;
		if (!test_NetShareCheck(p, tctx, r.out.info->info2->path)) {
			return false;
		}
	}

	return true;
}

static bool test_NetShareGetInfoAdminFull(struct torture_context *tctx, 
					  struct dcerpc_pipe *p)
{
	return test_NetShareGetInfo(tctx, p, "IPC$", true);
}

static bool test_NetShareGetInfoAdminAnon(struct torture_context *tctx, 
					  struct dcerpc_pipe *p)
{
	return test_NetShareGetInfo(tctx, p, "IPC$", false);
}

static bool test_NetShareAddSetDel(struct torture_context *tctx, 
				   struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct srvsvc_NetShareAdd a;
	struct srvsvc_NetShareSetInfo r;
	struct srvsvc_NetShareGetInfo q;
	struct srvsvc_NetShareDel d;
	struct sec_desc_buf sd_buf;
	union srvsvc_NetShareInfo info;
	struct {
		uint32_t level;
		WERROR expected;
	} levels[] = {
		 { 0,		WERR_UNKNOWN_LEVEL },
		 { 1,		WERR_OK },
		 { 2,		WERR_OK },
		 { 501,		WERR_UNKNOWN_LEVEL },
		 { 502,		WERR_OK },
		 { 1004,	WERR_OK },
		 { 1005,	WERR_OK },
		 { 1006,	WERR_OK },
/*		 { 1007,	WERR_OK }, */
		 { 1501,	WERR_OK },
	};
	int i;
	struct dcerpc_binding_handle *b = p->binding_handle;

	a.in.server_unc = r.in.server_unc = q.in.server_unc = d.in.server_unc =
		talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	r.in.share_name = talloc_strdup(tctx, "testshare");

	info.info2 = talloc(tctx, struct srvsvc_NetShareInfo2);
	info.info2->name = r.in.share_name;
	info.info2->type = STYPE_DISKTREE;
	info.info2->comment = talloc_strdup(tctx, "test comment");
	info.info2->permissions = 123434566;
	info.info2->max_users = -1;
	info.info2->current_users = 0;
	info.info2->path = talloc_strdup(tctx, "C:\\");
	info.info2->password = NULL;

	a.in.info = &info;
	a.in.level = 2;
	a.in.parm_error = NULL;

	status = dcerpc_srvsvc_NetShareAdd_r(b, tctx, &a);
	torture_assert_ntstatus_ok(tctx, status, "NetShareAdd level 2 on share 'testshare' failed");
	torture_assert_werr_ok(tctx, a.out.result, "NetShareAdd level 2 on share 'testshare' failed");

	r.in.parm_error = NULL;

	q.in.level = 502;

	for (i = 0; i < ARRAY_SIZE(levels); i++) {

		r.in.level = levels[i].level;
		ZERO_STRUCT(r.out);

		torture_comment(tctx, "Testing NetShareSetInfo level %u on share '%s'\n", 
		       r.in.level, r.in.share_name);

		switch (levels[i].level) {
		case 0:
			info.info0 = talloc(tctx, struct srvsvc_NetShareInfo0);
			info.info0->name = r.in.share_name;
			break;
		case 1:
			info.info1 = talloc(tctx, struct srvsvc_NetShareInfo1);
			info.info1->name = r.in.share_name;
			info.info1->type = STYPE_DISKTREE;
			info.info1->comment = talloc_strdup(tctx, "test comment 1");
			break;
		case 2:	
			info.info2 = talloc(tctx, struct srvsvc_NetShareInfo2);
			info.info2->name = r.in.share_name;
			info.info2->type = STYPE_DISKTREE;
			info.info2->comment = talloc_strdup(tctx, "test comment 2");
			info.info2->permissions = 0;
			info.info2->max_users = 2;
			info.info2->current_users = 1;
			info.info2->path = talloc_strdup(tctx, "::BLaH::"); /* "C:\\"); */
			info.info2->password = NULL;
			break;
		case 501:
			info.info501 = talloc(tctx, struct srvsvc_NetShareInfo501);
			info.info501->name = r.in.share_name;
			info.info501->type = STYPE_DISKTREE;
			info.info501->comment = talloc_strdup(tctx, "test comment 501");
			info.info501->csc_policy = 0;
			break;
		case 502:
			ZERO_STRUCT(sd_buf);
			info.info502 = talloc(tctx, struct srvsvc_NetShareInfo502);
			info.info502->name = r.in.share_name;
			info.info502->type = STYPE_DISKTREE;
			info.info502->comment = talloc_strdup(tctx, "test comment 502");
			info.info502->permissions = 0;
			info.info502->max_users = 502;
			info.info502->current_users = 1;
			info.info502->path = talloc_strdup(tctx, "C:\\");
			info.info502->password = NULL;
			info.info502->sd_buf = sd_buf;
			break;
		case 1004:
			info.info1004 = talloc(tctx, struct srvsvc_NetShareInfo1004);
			info.info1004->comment = talloc_strdup(tctx, "test comment 1004");
			break;
		case 1005:
			info.info1005 = talloc(tctx, struct srvsvc_NetShareInfo1005);
			info.info1005->dfs_flags = 0;
			break;
		case 1006:
			info.info1006 = talloc(tctx, struct srvsvc_NetShareInfo1006);
			info.info1006->max_users = 1006;
			break;
/*		case 1007:
			info.info1007 = talloc(tctx, struct srvsvc_NetShareInfo1007);
			info.info1007->flags = 0;
			info.info1007->alternate_directory_name = talloc_strdup(tctx, "test");
			break;
*/
		case 1501:
			info.info1501 = talloc_zero(tctx, struct sec_desc_buf);
			break;
		}

		r.in.info = &info;

		status = dcerpc_srvsvc_NetShareSetInfo_r(b, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "NetShareGetInfo failed");
		torture_assert_werr_equal(tctx, r.out.result, levels[i].expected, "NetShareSetInfo failed");
		
		q.in.share_name = r.in.share_name;
		q.out.info = &info;

		status = dcerpc_srvsvc_NetShareGetInfo_r(b, tctx, &q);
		torture_assert_ntstatus_ok(tctx, status, "NetShareGetInfo failed");
		torture_assert_werr_ok(tctx, q.out.result, "NetShareGetInfo failed");

		torture_assert_str_equal(tctx, q.out.info->info502->name, r.in.share_name,
					 "share name invalid");

		switch (levels[i].level) {
		case 0:
			break;
		case 1:
			torture_assert_str_equal(tctx, q.out.info->info502->comment, "test comment 1", "comment");
			break;
		case 2:
			torture_assert_str_equal(tctx, q.out.info->info2->comment, "test comment 2", "comment");
			torture_assert_int_equal(tctx, q.out.info->info2->max_users, 2, "max users");
			torture_assert_str_equal(tctx, q.out.info->info2->path, "C:\\", "path");
			break;
		case 501:
			torture_assert_str_equal(tctx, q.out.info->info501->comment, "test comment 501", "comment");
			break;
		case 502:
			torture_assert_str_equal(tctx, q.out.info->info502->comment, "test comment 502", "comment");
			torture_assert_int_equal(tctx, q.out.info->info502->max_users, 502, "max users");
			torture_assert_str_equal(tctx, q.out.info->info502->path, "C:\\", "path");
			break;
		case 1004:
			torture_assert_str_equal(tctx, q.out.info->info1004->comment, "test comment 1004",
						 "comment");
			break;
		case 1005:
			break;
		case 1006:
			torture_assert_int_equal(tctx, q.out.info->info1006->max_users, 1006, "Max users");
			break;
/*		case 1007:
			break;
*/
		case 1501:
			break;
		}
	}

	d.in.share_name = r.in.share_name;
	d.in.reserved = 0;

	status = dcerpc_srvsvc_NetShareDel_r(b, tctx, &d);
	torture_assert_ntstatus_ok(tctx, status, "NetShareDel on share 'testshare502' failed");
	torture_assert_werr_ok(tctx, a.out.result, "NetShareDel on share 'testshare502' failed");

	return true;
}

/**************************/
/* srvsvc_NetShare        */
/**************************/
static bool test_NetShareEnumAll(struct torture_context *tctx, 
				 struct dcerpc_pipe *p, 
				 bool admin)
{
	NTSTATUS status;
	struct srvsvc_NetShareEnumAll r;
	struct srvsvc_NetShareInfoCtr info_ctr;
	struct srvsvc_NetShareCtr0 c0;
	struct srvsvc_NetShareCtr1 c1;
	struct srvsvc_NetShareCtr2 c2;
	struct srvsvc_NetShareCtr501 c501;
	struct srvsvc_NetShareCtr502 c502;
	uint32_t totalentries = 0;
	struct {
		uint32_t level;
		WERROR anon_status;
		WERROR admin_status;
	} levels[] = {
		 { 0,	WERR_OK,		WERR_OK },
		 { 1,	WERR_OK,		WERR_OK },
		 { 2,	WERR_ACCESS_DENIED,	WERR_OK },
		 { 501,	WERR_ACCESS_DENIED,	WERR_OK },
		 { 502,	WERR_ACCESS_DENIED,	WERR_OK },
	};
	int i;
	uint32_t resume_handle;
	struct dcerpc_binding_handle *b = p->binding_handle;

	ZERO_STRUCT(info_ctr);

	r.in.server_unc = talloc_asprintf(tctx,"\\\\%s",dcerpc_server_name(p));
	r.in.info_ctr = &info_ctr;
	r.in.max_buffer = (uint32_t)-1;
	r.in.resume_handle = &resume_handle;
	r.out.resume_handle = &resume_handle;
	r.out.totalentries = &totalentries;
	r.out.info_ctr = &info_ctr;

	for (i=0;i<ARRAY_SIZE(levels);i++) {

		int j;
		WERROR expected;

		info_ctr.level = levels[i].level;

		switch (info_ctr.level) {
		case 0:
			ZERO_STRUCT(c0);
			info_ctr.ctr.ctr0 = &c0;
			break;
		case 1:
			ZERO_STRUCT(c1);
			info_ctr.ctr.ctr1 = &c1;
			break;
		case 2:
			ZERO_STRUCT(c2);
			info_ctr.ctr.ctr2 = &c2;
			break;
		case 501:
			ZERO_STRUCT(c501);
			info_ctr.ctr.ctr501 = &c501;
			break;
		case 502:
			ZERO_STRUCT(c502);
			info_ctr.ctr.ctr502 = &c502;
			break;
		}

		expected = levels[i].anon_status;
		if (admin) expected = levels[i].admin_status;

		resume_handle = 0;

		torture_comment(tctx, "Testing NetShareEnumAll level %u\n", info_ctr.level);
		status = dcerpc_srvsvc_NetShareEnumAll_r(b, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "NetShareEnumAll failed");
		torture_assert_werr_equal(tctx, r.out.result, expected, "NetShareEnumAll failed");

		/* call srvsvc_NetShareGetInfo for each returned share */
		if (info_ctr.level == 2 && r.out.info_ctr->ctr.ctr2) {
			for (j=0;j<r.out.info_ctr->ctr.ctr2->count;j++) {
				const char *name;
				name = r.out.info_ctr->ctr.ctr2->array[j].name;
				if (!test_NetShareGetInfo(tctx, p, name, admin)) {
					return false;
				}
			}
		}
	}

	return true;
}

static bool test_NetShareEnumAllFull(struct torture_context *tctx,
			      struct dcerpc_pipe *p)
{
	return test_NetShareEnumAll(tctx, p, true);
}

static bool test_NetShareEnumAllAnon(struct torture_context *tctx,
			      struct dcerpc_pipe *p)
{
	return test_NetShareEnumAll(tctx, p, false);
}

static bool test_NetShareEnum(struct torture_context *tctx,
			      struct dcerpc_pipe *p, bool admin)
{
	NTSTATUS status;
	struct srvsvc_NetShareEnum r;
	struct srvsvc_NetShareInfoCtr info_ctr;
	struct srvsvc_NetShareCtr0 c0;
	struct srvsvc_NetShareCtr1 c1;
	struct srvsvc_NetShareCtr2 c2;
	struct srvsvc_NetShareCtr501 c501;
	struct srvsvc_NetShareCtr502 c502;
	uint32_t totalentries = 0;
	struct {
		uint32_t level;
		WERROR anon_status;
		WERROR admin_status;
	} levels[] = {
		 { 0,	WERR_OK,		WERR_OK },
		 { 1,	WERR_OK,		WERR_OK },
		 { 2,	WERR_ACCESS_DENIED,	WERR_OK },
		 { 501,	WERR_UNKNOWN_LEVEL,	WERR_UNKNOWN_LEVEL },
		 { 502,	WERR_ACCESS_DENIED,	WERR_OK },
	};
	int i;
	struct dcerpc_binding_handle *b = p->binding_handle;

	r.in.server_unc = talloc_asprintf(tctx,"\\\\%s",dcerpc_server_name(p));
	r.in.info_ctr = &info_ctr;
	r.in.max_buffer = (uint32_t)-1;
	r.in.resume_handle = NULL;
	r.out.totalentries = &totalentries;
	r.out.info_ctr = &info_ctr;

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		WERROR expected;

		info_ctr.level = levels[i].level;

		switch (info_ctr.level) {
		case 0:
			ZERO_STRUCT(c0);
			info_ctr.ctr.ctr0 = &c0;
			break;
		case 1:
			ZERO_STRUCT(c1);
			info_ctr.ctr.ctr1 = &c1;
			break;
		case 2:
			ZERO_STRUCT(c2);
			info_ctr.ctr.ctr2 = &c2;
			break;
		case 501:
			ZERO_STRUCT(c501);
			info_ctr.ctr.ctr501 = &c501;
			break;
		case 502:
			ZERO_STRUCT(c502);
			info_ctr.ctr.ctr502 = &c502;
			break;
		}

		expected = levels[i].anon_status;
		if (admin) expected = levels[i].admin_status;

		torture_comment(tctx, "Testing NetShareEnum level %u\n", info_ctr.level);
		status = dcerpc_srvsvc_NetShareEnum_r(b, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "NetShareEnum failed");
		torture_assert_werr_equal(tctx, r.out.result, expected, "NetShareEnum failed");
	}

	return true;
}

static bool test_NetShareEnumFull(struct torture_context *tctx,
				  struct dcerpc_pipe *p)
{
	return test_NetShareEnum(tctx, p, true);
}

static bool test_NetShareEnumAnon(struct torture_context *tctx,
				  struct dcerpc_pipe *p)
{
	return test_NetShareEnum(tctx, p, false);
}

/**************************/
/* srvsvc_NetSrv          */
/**************************/
static bool test_NetSrvGetInfo(struct torture_context *tctx, 
			       struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct srvsvc_NetSrvGetInfo r;
	union srvsvc_NetSrvInfo info;
	uint32_t levels[] = {100, 101, 102, 502, 503};
	int i;
	struct dcerpc_binding_handle *b = p->binding_handle;

	r.in.server_unc = talloc_asprintf(tctx,"\\\\%s",dcerpc_server_name(p));

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		r.in.level = levels[i];
		r.out.info = &info;
		torture_comment(tctx, "Testing NetSrvGetInfo level %u\n", r.in.level);
		status = dcerpc_srvsvc_NetSrvGetInfo_r(b, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "NetSrvGetInfo failed");
		if (!W_ERROR_IS_OK(r.out.result)) {
			torture_comment(tctx, "NetSrvGetInfo failed: %s\n", win_errstr(r.out.result));
		}
	}

	return true;
}

/**************************/
/* srvsvc_NetDisk         */
/**************************/
static bool test_NetDiskEnum(struct torture_context *tctx, 
			     struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct srvsvc_NetDiskEnum r;
	struct srvsvc_NetDiskInfo info;
	uint32_t totalentries = 0;
	uint32_t levels[] = {0};
	int i;
	uint32_t resume_handle=0;
	struct dcerpc_binding_handle *b = p->binding_handle;

	ZERO_STRUCT(info);

	r.in.server_unc = NULL;
	r.in.resume_handle = &resume_handle;
	r.in.info = &info;
	r.out.info = &info;
	r.out.totalentries = &totalentries;
	r.out.resume_handle = &resume_handle;

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		ZERO_STRUCTP(r.out.info);
		r.in.level = levels[i];
		torture_comment(tctx, "Testing NetDiskEnum level %u\n", r.in.level);
		status = dcerpc_srvsvc_NetDiskEnum_r(b, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "NetDiskEnum failed");
		torture_assert_werr_ok(tctx, r.out.result, "NetDiskEnum failed");
	}

	return true;
}

/**************************/
/* srvsvc_NetTransport    */
/**************************/
static bool test_NetTransportEnum(struct torture_context *tctx, 
				  struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct srvsvc_NetTransportEnum r;
	struct srvsvc_NetTransportInfoCtr transports;
	struct srvsvc_NetTransportCtr0 ctr0;
	struct srvsvc_NetTransportCtr1 ctr1;

	uint32_t totalentries = 0;
	uint32_t levels[] = {0, 1};
	int i;
	struct dcerpc_binding_handle *b = p->binding_handle;

	ZERO_STRUCT(transports);

	r.in.server_unc = talloc_asprintf(tctx,"\\\\%s", dcerpc_server_name(p));
	r.in.transports = &transports;
	r.in.max_buffer = (uint32_t)-1;
	r.in.resume_handle = NULL;
	r.out.totalentries = &totalentries;
	r.out.transports = &transports;

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		transports.level = levels[i];
		switch (transports.level) {
		case 0:
			ZERO_STRUCT(ctr0);
			transports.ctr.ctr0 = &ctr0;
			break;
		case 1:
			ZERO_STRUCT(ctr1);
			transports.ctr.ctr1 = &ctr1;
			break;
		}
		torture_comment(tctx, "Testing NetTransportEnum level %u\n", transports.level);
		status = dcerpc_srvsvc_NetTransportEnum_r(b, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "NetTransportEnum failed");
		if (!W_ERROR_IS_OK(r.out.result)) {
			torture_comment(tctx, "unexpected result: %s\n", win_errstr(r.out.result));
		}
	}

	return true;
}

/**************************/
/* srvsvc_NetRemoteTOD    */
/**************************/
static bool test_NetRemoteTOD(struct torture_context *tctx, 
			      struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct srvsvc_NetRemoteTOD r;
	struct srvsvc_NetRemoteTODInfo *info = NULL;
	struct dcerpc_binding_handle *b = p->binding_handle;

	r.in.server_unc = talloc_asprintf(tctx,"\\\\%s",dcerpc_server_name(p));
	r.out.info = &info;

	torture_comment(tctx, "Testing NetRemoteTOD\n");
	status = dcerpc_srvsvc_NetRemoteTOD_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "NetRemoteTOD failed");
	torture_assert_werr_ok(tctx, r.out.result, "NetRemoteTOD failed");

	return true;
}

/**************************/
/* srvsvc_NetName         */
/**************************/

static bool test_NetNameValidate(struct torture_context *tctx, 
								 struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct srvsvc_NetNameValidate r;
	char *invalidc;
	char *name;
	int i, n, min, max;
	struct dcerpc_binding_handle *b = p->binding_handle;

	r.in.server_unc = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	r.in.flags = 0x0;

	d_printf("Testing NetNameValidate\n");

	/* valid path types only between 1 and 13 */
	for (i = 1; i < 14; i++) {

again:
		/* let's limit ourselves to a maximum of 4096 bytes */
		r.in.name = name = talloc_array(tctx, char, 4097);
		max = 4096;
		min = 0;
		n = max;

		while (1) {

			/* Find maximum length accepted by this type */
			ZERO_STRUCT(r.out);
			r.in.name_type = i;
			memset(name, 'A', n);
			name[n] = '\0';

			status = dcerpc_srvsvc_NetNameValidate_r(b, tctx, &r);
			if (!NT_STATUS_IS_OK(status)) {
				d_printf("NetNameValidate failed while checking maximum size (%s)\n",
						nt_errstr(status));
				break;
			}

			if (W_ERROR_IS_OK(r.out.result)) {
				min = n;
				n += (max - min + 1)/2;
				continue;
				
			} else {
				if ((min + 1) >= max) break; /* found it */
				
				max = n;
				n -= (max - min)/2;
				continue;
			}
		}

		talloc_free(name);

		d_printf("Maximum length for type %2d, flags %08x: %d\n", i, r.in.flags, max);

		/* find invalid chars for this type check only ASCII between 0x20 and 0x7e */

		invalidc = talloc_strdup(tctx, "");

		for (n = 0x20; n < 0x7e; n++) {
			r.in.name = name = talloc_asprintf(tctx, "%c", (char)n);

			status = dcerpc_srvsvc_NetNameValidate_r(b, tctx, &r);
			if (!NT_STATUS_IS_OK(status)) {
				d_printf("NetNameValidate failed while checking valid chars (%s)\n",
						nt_errstr(status));
				break;
			}

			if (!W_ERROR_IS_OK(r.out.result)) {
				invalidc = talloc_asprintf_append_buffer(invalidc, "%c", (char)n);
			}

			talloc_free(name);
		}

		d_printf(" Invalid chars for type %2d, flags %08x: \"%s\"\n", i, r.in.flags, invalidc);

		/* only two values are accepted for flags: 0x0 and 0x80000000 */
		if (r.in.flags == 0x0) {
			r.in.flags = 0x80000000;
			goto again;
		}

		r.in.flags = 0x0;
	}

	return true;
}

struct torture_suite *torture_rpc_srvsvc(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "srvsvc");
	struct torture_rpc_tcase *tcase;
	struct torture_test *test;

	tcase = torture_suite_add_rpc_iface_tcase(suite, "srvsvc (admin access)", &ndr_table_srvsvc);

	torture_rpc_tcase_add_test(tcase, "NetCharDevEnum", test_NetCharDevEnum);
	torture_rpc_tcase_add_test(tcase, "NetCharDevQEnum", test_NetCharDevQEnum);
	torture_rpc_tcase_add_test(tcase, "NetConnEnum", test_NetConnEnum);
	torture_rpc_tcase_add_test(tcase, "NetFileEnum", test_NetFileEnum);
	torture_rpc_tcase_add_test(tcase, "NetSessEnum", test_NetSessEnum);
	torture_rpc_tcase_add_test(tcase, "NetShareEnumAll", test_NetShareEnumAllFull);
	torture_rpc_tcase_add_test(tcase, "NetSrvGetInfo", test_NetSrvGetInfo);
	torture_rpc_tcase_add_test(tcase, "NetDiskEnum", test_NetDiskEnum);
	torture_rpc_tcase_add_test(tcase, "NetTransportEnum", test_NetTransportEnum);
	torture_rpc_tcase_add_test(tcase, "NetRemoteTOD", test_NetRemoteTOD);
	torture_rpc_tcase_add_test(tcase, "NetShareEnum", test_NetShareEnumFull);
	torture_rpc_tcase_add_test(tcase, "NetShareGetInfo", test_NetShareGetInfoAdminFull);
	test = torture_rpc_tcase_add_test(tcase, "NetShareAddSetDel", 
					   test_NetShareAddSetDel);
	test->dangerous = true;
	torture_rpc_tcase_add_test(tcase, "NetNameValidate", test_NetNameValidate);
	
	tcase = torture_suite_add_anon_rpc_iface_tcase(suite, 
						    "srvsvc anonymous access", 
						    &ndr_table_srvsvc);

	torture_rpc_tcase_add_test(tcase, "NetShareEnumAll", 
				   test_NetShareEnumAllAnon);
	torture_rpc_tcase_add_test(tcase, "NetShareEnum", 
				   test_NetShareEnumAnon);
	torture_rpc_tcase_add_test(tcase, "NetShareGetInfo", 
				   test_NetShareGetInfoAdminAnon);

	return suite;
}
