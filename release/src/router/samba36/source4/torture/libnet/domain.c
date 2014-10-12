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
#include "librpc/gen_ndr/ndr_samr_c.h"
#include "param/param.h"

static bool test_domainopen(struct torture_context *tctx,
			    struct libnet_context *net_ctx, TALLOC_CTX *mem_ctx,
			    struct lsa_String *domname,
			    struct policy_handle *domain_handle)
{
	NTSTATUS status;
	struct libnet_DomainOpen io;

	torture_comment(tctx, "opening domain\n");

	io.in.domain_name  = talloc_strdup(mem_ctx, domname->string);
	io.in.access_mask  = SEC_FLAG_MAXIMUM_ALLOWED;

	status = libnet_DomainOpen(net_ctx, mem_ctx, &io);
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(tctx, "Composite domain open failed for domain '%s' - %s\n",
				domname->string, nt_errstr(status));
		return false;
	}

	*domain_handle = io.out.domain_handle;
	return true;
}


static bool test_cleanup(struct torture_context *tctx,
			 struct dcerpc_binding_handle *b, TALLOC_CTX *mem_ctx,
			 struct policy_handle *domain_handle)
{
	struct samr_Close r;
	struct policy_handle handle;

	r.in.handle   = domain_handle;
	r.out.handle  = &handle;

	torture_comment(tctx, "closing domain handle\n");

	torture_assert_ntstatus_ok(tctx,
		dcerpc_samr_Close_r(b, mem_ctx, &r),
		"Close failed");
	torture_assert_ntstatus_ok(tctx, r.out.result,
		"Close failed");

	return true;
}


bool torture_domainopen(struct torture_context *torture)
{
	NTSTATUS status;
	struct libnet_context *net_ctx;
	TALLOC_CTX *mem_ctx;
	bool ret = true;
	struct policy_handle h;
	struct lsa_String name;

	mem_ctx = talloc_init("test_domain_open");

	net_ctx = libnet_context_init(torture->ev, torture->lp_ctx);

	status = torture_rpc_connection(torture,
					&net_ctx->samr.pipe,
					&ndr_table_samr);

	if (!NT_STATUS_IS_OK(status)) {
		return false;
	}

	name.string = lpcfg_workgroup(torture->lp_ctx);

	/*
	 * Testing synchronous version
	 */
	if (!test_domainopen(torture, net_ctx, mem_ctx, &name, &h)) {
		ret = false;
		goto done;
	}

	if (!test_cleanup(torture, net_ctx->samr.pipe->binding_handle, mem_ctx, &h)) {
		ret = false;
		goto done;
	}

done:
	talloc_free(mem_ctx);

	return ret;
}
