/* 
   Unix SMB/CIFS implementation.

   simple RPC benchmark

   Copyright (C) Andrew Tridgell 2005
   
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
/* srvsvc_NetShare        */
/**************************/
static bool test_NetShareEnumAll(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx)
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
	uint32_t levels[] = {0, 1, 2, 501, 502};
	int i;
	bool ret = true;
	uint32_t resume_handle;
	struct dcerpc_binding_handle *b = p->binding_handle;

	ZERO_STRUCT(info_ctr);

	r.in.server_unc = talloc_asprintf(mem_ctx,"\\\\%s",dcerpc_server_name(p));
	r.in.info_ctr = &info_ctr;
	r.in.max_buffer = (uint32_t)-1;
	r.in.resume_handle = &resume_handle;
	r.out.resume_handle = &resume_handle;
	r.out.totalentries = &totalentries;
	r.out.info_ctr = &info_ctr;

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		resume_handle = 0;
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
		case 501:
			ZERO_STRUCT(c501);
			info_ctr.ctr.ctr501 = &c501;
			break;
		case 502:
			ZERO_STRUCT(c502);
			info_ctr.ctr.ctr502 = &c502;
			break;
		}

		status = dcerpc_srvsvc_NetShareEnumAll_r(b, mem_ctx, &r);
		if (!NT_STATUS_IS_OK(status)) {
			printf("NetShareEnumAll level %u failed - %s\n", info_ctr.level, nt_errstr(status));
			ret = false;
			continue;
		}
		if (!W_ERROR_IS_OK(r.out.result)) {
			printf("NetShareEnumAll level %u failed - %s\n", info_ctr.level, win_errstr(r.out.result));
			continue;
		}
	}

	return ret;
}

/*
  benchmark srvsvc netshareenumall queries
*/
static bool bench_NetShareEnumAll(struct torture_context *tctx, struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx)
{
	struct timeval tv = timeval_current();
	bool ret = true;
	int timelimit = torture_setting_int(tctx, "timelimit", 10);
	int count=0;

	printf("Running for %d seconds\n", timelimit);
	while (timeval_elapsed(&tv) < timelimit) {
		TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
		if (!test_NetShareEnumAll(p, tmp_ctx)) break;
		talloc_free(tmp_ctx);
		count++;
		if (count % 50 == 0) {
			if (torture_setting_bool(tctx, "progress", true)) {
				printf("%.1f queries per second  \r", 
				       count / timeval_elapsed(&tv));
			}
		}
	}

	printf("%.1f queries per second  \n", count / timeval_elapsed(&tv));

	return ret;
}


bool torture_bench_rpc(struct torture_context *torture)
{
        NTSTATUS status;
        struct dcerpc_pipe *p;
	TALLOC_CTX *mem_ctx;
	bool ret = true;

	mem_ctx = talloc_init("torture_rpc_srvsvc");

	status = torture_rpc_connection(torture, 
					&p,
					&ndr_table_srvsvc);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(mem_ctx);
		return false;
	}

	if (!bench_NetShareEnumAll(torture, p, mem_ctx)) {
		ret = false;
	}

	talloc_free(mem_ctx);

	return ret;
}
