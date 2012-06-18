/* 
   Unix SMB/CIFS implementation.
   Test suite for libnet calls.

   Copyright (C) Gregory LEOCADIE <gleocadie@idealx.com> 2005
   Copyright (C) Rafal Szczesniak  2005
   
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
#include "lib/cmdline/popt_common.h"
#include "librpc/gen_ndr/ndr_srvsvc_c.h"


#define TEST_SHARENAME "libnetsharetest"


static void test_displayshares(struct libnet_ListShares s)
{
	int i, j;

	struct share_type {
		enum srvsvc_ShareType type;
		const char *desc;
	} share_types[] = {
		{ STYPE_DISKTREE, "STYPE_DISKTREE" },
		{ STYPE_DISKTREE_TEMPORARY, "STYPE_DISKTREE_TEMPORARY" },
		{ STYPE_DISKTREE_HIDDEN, "STYPE_DISKTREE_HIDDEN" },
		{ STYPE_PRINTQ, "STYPE_PRINTQ" },
		{ STYPE_PRINTQ_TEMPORARY, "STYPE_PRINTQ_TEMPORARY" },
		{ STYPE_PRINTQ_HIDDEN, "STYPE_PRINTQ_HIDDEN" },
		{ STYPE_DEVICE, "STYPE_DEVICE" },
		{ STYPE_DEVICE_TEMPORARY, "STYPE_DEVICE_TEMPORARY" },
		{ STYPE_DEVICE_HIDDEN, "STYPE_DEVICE_HIDDEN" },
		{ STYPE_IPC, "STYPE_IPC" },
		{ STYPE_IPC_TEMPORARY, "STYPE_IPC_TEMPORARY" },
		{ STYPE_IPC_HIDDEN, "STYPE_IPC_HIDDEN" }
	};

	switch (s.in.level) {
	case 0:
		for (i = 0; i < s.out.ctr.ctr0->count; i++) {
			struct srvsvc_NetShareInfo0 *info = &s.out.ctr.ctr0->array[i];
			d_printf("\t[%d] %s\n", i, info->name);
		}
		break;

	case 1:
		for (i = 0; i < s.out.ctr.ctr1->count; i++) {
			struct srvsvc_NetShareInfo1 *info = &s.out.ctr.ctr1->array[i];
			for (j = 0; j < ARRAY_SIZE(share_types); j++) {
				if (share_types[j].type == info->type) break;
			}
			d_printf("\t[%d] %s (%s)\t%s\n", i, info->name,
			       info->comment, share_types[j].desc);
		}
		break;

	case 2:
		for (i = 0; i < s.out.ctr.ctr2->count; i++) {
			struct srvsvc_NetShareInfo2 *info = &s.out.ctr.ctr2->array[i];
			for (j = 0; j < ARRAY_SIZE(share_types); j++) {
				if (share_types[j].type == info->type) break;
			}
			d_printf("\t[%d] %s\t%s\n\t    %s\n\t    [perms=0x%08x, max_usr=%d, cur_usr=%d, path=%s, pass=%s]\n",
				 i, info->name, share_types[j].desc, info->comment,
				 info->permissions, info->max_users,
				 info->current_users, info->path,
				 info->password);
		}
		break;

	case 501:
		for (i = 0; i < s.out.ctr.ctr501->count; i++) {
			struct srvsvc_NetShareInfo501 *info = &s.out.ctr.ctr501->array[i];
			for (j = 0; j < ARRAY_SIZE(share_types); j++) {
				if (share_types[j].type == info->type) break;
			}
			d_printf("\t[%d] %s\t%s [csc_policy=0x%08x]\n\t    %s\n", i, info->name,
				 share_types[j].desc, info->csc_policy,
				 info->comment);
		}
		break;

	case 502:
		for (i = 0; i < s.out.ctr.ctr502->count; i++) {
			struct srvsvc_NetShareInfo502 *info = &s.out.ctr.ctr502->array[i];
			for (j = 0; j < ARRAY_SIZE(share_types); j++) {
				if (share_types[j].type == info->type) break;
			}
			d_printf("\t[%d] %s\t%s\n\t    %s\n\t    [perms=0x%08x, max_usr=%d, cur_usr=%d, path=%s, pass=%s]\n",
				 i, info->name, share_types[j].desc, info->comment,
				 info->permissions, info->max_users,
				 info->current_users, info->path,
				 info->password);
		}
		break;
	}
}


bool torture_listshares(struct torture_context *torture)
{
	struct libnet_ListShares share;
	NTSTATUS  status;
	uint32_t levels[] = { 0, 1, 2, 501, 502 };
	int i;
	bool ret = true;
	struct libnet_context* libnetctx;
	struct dcerpc_binding *binding;
	TALLOC_CTX *mem_ctx;

	mem_ctx = talloc_init("test_listshares");
	status = torture_rpc_binding(torture, &binding);
	if (!NT_STATUS_IS_OK(status)) {
		ret = false;
		goto done;
	}

	libnetctx = libnet_context_init(torture->ev, torture->lp_ctx);
	if (!libnetctx) {
		printf("Couldn't allocate libnet context\n");
		ret = false;
		goto done;
	}

	libnetctx->cred = cmdline_credentials;
	
	printf("Testing libnet_ListShare\n");
	
	share.in.server_name = talloc_asprintf(mem_ctx, "%s", binding->host);

	for (i = 0; i < ARRAY_SIZE(levels); i++) {
		share.in.level = levels[i];
		printf("testing libnet_ListShare level %u\n", share.in.level);

		status = libnet_ListShares(libnetctx, mem_ctx, &share);
		if (!NT_STATUS_IS_OK(status)) {
			printf("libnet_ListShare level %u failed - %s\n", share.in.level, share.out.error_string);
			ret = false;
			goto done;
		}

		printf("listing shares:\n");
		test_displayshares(share);
	}

done:
	talloc_free(mem_ctx);
	return ret;
}


static bool test_addshare(struct dcerpc_pipe *svc_pipe, TALLOC_CTX *mem_ctx, const char *host,
			  const char* share)
{
	NTSTATUS status;
	struct srvsvc_NetShareAdd add;
	union srvsvc_NetShareInfo info;
	struct srvsvc_NetShareInfo2 i;
	
	i.name         = share;
	i.type         = STYPE_DISKTREE;
	i.path         = "C:\\WINDOWS\\TEMP";
	i.max_users    = 5;
	i.comment      = "Comment to the test share";
	i.password     = NULL;
	i.permissions  = 0x0;

	info.info2 = &i;

	add.in.server_unc = host;
	add.in.level      = 2;
	add.in.info       = &info;
	add.in.parm_error = NULL;

	status = dcerpc_srvsvc_NetShareAdd(svc_pipe, mem_ctx, &add);
	if (!NT_STATUS_IS_OK(status)) {
		printf("Failed to add a new share\n");
		return false;
	}

	printf("share added\n");
	return true;
}


bool torture_delshare(struct torture_context *torture)
{
	struct dcerpc_pipe *p;
	struct dcerpc_binding *binding;
	struct libnet_context* libnetctx;
	const char *host;
	NTSTATUS  status;
	bool ret = true;
	struct libnet_DelShare share;
	
	host = torture_setting_string(torture, "host", NULL);
	status = torture_rpc_binding(torture, &binding);
	torture_assert_ntstatus_ok(torture, status, "Failed to get binding");

	libnetctx = libnet_context_init(torture->ev, torture->lp_ctx);
	libnetctx->cred = cmdline_credentials;

	status = torture_rpc_connection(torture,
					&p,
					&ndr_table_srvsvc);

	torture_assert_ntstatus_ok(torture, status, "Failed to get rpc connection");

	if (!test_addshare(p, torture, host, TEST_SHARENAME)) {
		return false;
	}

	share.in.server_name	= binding->host;
	share.in.share_name	= TEST_SHARENAME;

	status = libnet_DelShare(libnetctx, torture, &share);
	torture_assert_ntstatus_ok(torture, status, "Failed to delete share");

	return ret;
}
