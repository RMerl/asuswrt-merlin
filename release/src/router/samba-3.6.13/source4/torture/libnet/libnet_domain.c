/*
   Unix SMB/CIFS implementation.
   Test suite for libnet calls.

   Copyright (C) Rafal Szczesniak 2006

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
#include "lib/cmdline/popt_common.h"
#include "libnet/libnet.h"
#include "librpc/gen_ndr/ndr_samr_c.h"
#include "librpc/gen_ndr/ndr_lsa_c.h"
#include "torture/rpc/torture_rpc.h"
#include "param/param.h"


static bool test_opendomain_samr(struct torture_context *tctx,
				 struct dcerpc_binding_handle *b, TALLOC_CTX *mem_ctx,
				 struct policy_handle *handle, struct lsa_String *domname,
				 uint32_t *access_mask, struct dom_sid **sid_p)
{
	struct policy_handle h, domain_handle;
	struct samr_Connect r1;
	struct samr_LookupDomain r2;
	struct dom_sid2 *sid = NULL;
	struct samr_OpenDomain r3;

	torture_comment(tctx, "connecting\n");

	*access_mask = SEC_FLAG_MAXIMUM_ALLOWED;

	r1.in.system_name = 0;
	r1.in.access_mask = *access_mask;
	r1.out.connect_handle = &h;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_samr_Connect_r(b, mem_ctx, &r1),
		"Connect failed");
	torture_assert_ntstatus_ok(tctx, r1.out.result,
		"Connect failed");

	r2.in.connect_handle = &h;
	r2.in.domain_name = domname;
	r2.out.sid = &sid;

	torture_comment(tctx, "domain lookup on %s\n", domname->string);

	torture_assert_ntstatus_ok(tctx,
		dcerpc_samr_LookupDomain_r(b, mem_ctx, &r2),
		"LookupDomain failed");
	torture_assert_ntstatus_ok(tctx, r2.out.result,
		"LookupDomain failed");

	r3.in.connect_handle = &h;
	r3.in.access_mask = *access_mask;
	r3.in.sid = *sid_p = *r2.out.sid;
	r3.out.domain_handle = &domain_handle;

	torture_comment(tctx, "opening domain\n");

	torture_assert_ntstatus_ok(tctx,
		dcerpc_samr_OpenDomain_r(b, mem_ctx, &r3),
		"OpenDomain failed");
	torture_assert_ntstatus_ok(tctx, r3.out.result,
		"OpenDomain failed");

	*handle = domain_handle;

	return true;
}


static bool test_opendomain_lsa(struct torture_context *tctx,
				struct dcerpc_binding_handle *b, TALLOC_CTX *mem_ctx,
				struct policy_handle *handle, struct lsa_String *domname,
				uint32_t *access_mask)
{
	struct lsa_OpenPolicy2 open;
	struct lsa_ObjectAttribute attr;
	struct lsa_QosInfo qos;

	*access_mask = SEC_FLAG_MAXIMUM_ALLOWED;

	ZERO_STRUCT(attr);
	ZERO_STRUCT(qos);

	qos.len                 = 0;
	qos.impersonation_level = 2;
	qos.context_mode        = 1;
	qos.effective_only      = 0;

	attr.sec_qos = &qos;

	open.in.system_name = domname->string;
	open.in.attr        = &attr;
	open.in.access_mask = *access_mask;
	open.out.handle     = handle;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_lsa_OpenPolicy2_r(b, mem_ctx, &open),
		"OpenPolicy2 failed");
	torture_assert_ntstatus_ok(tctx, open.out.result,
		"OpenPolicy2 failed");

	return true;
}

bool torture_domain_open_lsa(struct torture_context *torture)
{
	NTSTATUS status;
	bool ret = true;
	struct libnet_context *ctx;
	struct libnet_DomainOpen r;
	struct lsa_Close lsa_close;
	struct policy_handle h;
	const char *domain_name;

	/* we're accessing domain controller so the domain name should be
	   passed (it's going to be resolved to dc name and address) instead
	   of specific server name. */
	domain_name = lpcfg_workgroup(torture->lp_ctx);

	ctx = libnet_context_init(torture->ev, torture->lp_ctx);
	if (ctx == NULL) {
		torture_comment(torture, "failed to create libnet context\n");
		return false;
	}

	ctx->cred = cmdline_credentials;

	ZERO_STRUCT(r);
	r.in.type = DOMAIN_LSA;
	r.in.domain_name = domain_name;
	r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;

	status = libnet_DomainOpen(ctx, torture, &r);
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(torture, "failed to open domain on lsa service: %s\n", nt_errstr(status));
		ret = false;
		goto done;
	}

	ZERO_STRUCT(lsa_close);
	lsa_close.in.handle  = &ctx->lsa.handle;
	lsa_close.out.handle = &h;

	torture_assert_ntstatus_ok(torture,
		dcerpc_lsa_Close_r(ctx->lsa.pipe->binding_handle, ctx, &lsa_close),
		"failed to close domain on lsa service");
	torture_assert_ntstatus_ok(torture, lsa_close.out.result,
		"failed to close domain on lsa service");

done:
	talloc_free(ctx);
	return ret;
}


bool torture_domain_close_lsa(struct torture_context *torture)
{
	bool ret = true;
	NTSTATUS status;
	TALLOC_CTX *mem_ctx=NULL;
	struct libnet_context *ctx;
	struct lsa_String domain_name;
	struct dcerpc_binding *binding;
	uint32_t access_mask;
	struct policy_handle h;
	struct dcerpc_pipe *p;
	struct libnet_DomainClose r;

	status = torture_rpc_binding(torture, &binding);
	if (!NT_STATUS_IS_OK(status)) {
		return false;
	}

	ctx = libnet_context_init(torture->ev, torture->lp_ctx);
	if (ctx == NULL) {
		torture_comment(torture, "failed to create libnet context\n");
		ret = false;
		goto done;
	}

	ctx->cred = cmdline_credentials;

	mem_ctx = talloc_init("torture_domain_close_lsa");
	status = dcerpc_pipe_connect_b(mem_ctx, &p, binding, &ndr_table_lsarpc,
				     cmdline_credentials, torture->ev, torture->lp_ctx);
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(torture, "failed to connect to server: %s\n", nt_errstr(status));
		ret = false;
		goto done;
	}

	domain_name.string = lpcfg_workgroup(torture->lp_ctx);

	if (!test_opendomain_lsa(torture, p->binding_handle, torture, &h, &domain_name, &access_mask)) {
		torture_comment(torture, "failed to open domain on lsa service\n");
		ret = false;
		goto done;
	}

	ctx->lsa.pipe        = p;
	ctx->lsa.name        = domain_name.string;
	ctx->lsa.access_mask = access_mask;
	ctx->lsa.handle      = h;
	/* we have to use pipe's event context, otherwise the call will
	   hang indefinitely */
	ctx->event_ctx       = p->conn->event_ctx;

	ZERO_STRUCT(r);
	r.in.type = DOMAIN_LSA;
	r.in.domain_name = domain_name.string;

	status = libnet_DomainClose(ctx, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		ret = false;
		goto done;
	}

done:
	talloc_free(mem_ctx);
	talloc_free(ctx);
	return ret;
}


bool torture_domain_open_samr(struct torture_context *torture)
{
	NTSTATUS status;
	struct libnet_context *ctx;
	TALLOC_CTX *mem_ctx;
	struct policy_handle domain_handle, handle;
	struct libnet_DomainOpen io;
	struct samr_Close r;
	const char *domain_name;
	bool ret = true;

	mem_ctx = talloc_init("test_domainopen_lsa");

	ctx = libnet_context_init(torture->ev, torture->lp_ctx);
	ctx->cred = cmdline_credentials;

	/* we're accessing domain controller so the domain name should be
	   passed (it's going to be resolved to dc name and address) instead
	   of specific server name. */
	domain_name = lpcfg_workgroup(torture->lp_ctx);

	/*
	 * Testing synchronous version
	 */
	torture_comment(torture, "opening domain\n");

	io.in.type         = DOMAIN_SAMR;
	io.in.domain_name  = domain_name;
	io.in.access_mask  = SEC_FLAG_MAXIMUM_ALLOWED;

	status = libnet_DomainOpen(ctx, mem_ctx, &io);
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(torture, "Composite domain open failed for domain '%s' - %s\n",
				domain_name, nt_errstr(status));
		ret = false;
		goto done;
	}

	domain_handle = ctx->samr.handle;

	r.in.handle   = &domain_handle;
	r.out.handle  = &handle;

	torture_comment(torture, "closing domain handle\n");

	torture_assert_ntstatus_ok(torture,
		dcerpc_samr_Close_r(ctx->samr.pipe->binding_handle, mem_ctx, &r),
		"Close failed");
	torture_assert_ntstatus_ok(torture, r.out.result,
		"Close failed");

done:
	talloc_free(mem_ctx);
	talloc_free(ctx);

	return ret;
}


bool torture_domain_close_samr(struct torture_context *torture)
{
	bool ret = true;
	NTSTATUS status;
	TALLOC_CTX *mem_ctx = NULL;
	struct libnet_context *ctx;
	struct lsa_String domain_name;
	struct dcerpc_binding *binding;
	uint32_t access_mask;
	struct policy_handle h;
	struct dcerpc_pipe *p;
	struct libnet_DomainClose r;
	struct dom_sid *sid;

	status = torture_rpc_binding(torture, &binding);
	if (!NT_STATUS_IS_OK(status)) {
		return false;
	}

	ctx = libnet_context_init(torture->ev, torture->lp_ctx);
	if (ctx == NULL) {
		torture_comment(torture, "failed to create libnet context\n");
		ret = false;
		goto done;
	}

	ctx->cred = cmdline_credentials;

	mem_ctx = talloc_init("torture_domain_close_samr");
	status = dcerpc_pipe_connect_b(mem_ctx, &p, binding, &ndr_table_samr,
				     ctx->cred, torture->ev, torture->lp_ctx);
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(torture, "failed to connect to server: %s\n", nt_errstr(status));
		ret = false;
		goto done;
	}

	domain_name.string = talloc_strdup(mem_ctx, lpcfg_workgroup(torture->lp_ctx));

	if (!test_opendomain_samr(torture, p->binding_handle, torture, &h, &domain_name, &access_mask, &sid)) {
		torture_comment(torture, "failed to open domain on samr service\n");
		ret = false;
		goto done;
	}

	ctx->samr.pipe        = p;
	ctx->samr.name        = talloc_steal(ctx, domain_name.string);
	ctx->samr.access_mask = access_mask;
	ctx->samr.handle      = h;
	ctx->samr.sid         = talloc_steal(ctx, sid);
	/* we have to use pipe's event context, otherwise the call will
	   hang indefinitely - this wouldn't be the case if pipe was opened
	   by means of libnet call */
	ctx->event_ctx       = p->conn->event_ctx;

	ZERO_STRUCT(r);
	r.in.type = DOMAIN_SAMR;
	r.in.domain_name = domain_name.string;

	status = libnet_DomainClose(ctx, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		ret = false;
		goto done;
	}

done:
	talloc_free(mem_ctx);
	talloc_free(ctx);
	return ret;
}


bool torture_domain_list(struct torture_context *torture)
{
	bool ret = true;
	NTSTATUS status;
	TALLOC_CTX *mem_ctx = NULL;
	struct dcerpc_binding *binding;
	struct libnet_context *ctx;
	struct libnet_DomainList r;
	int i;

	status = torture_rpc_binding(torture, &binding);
	if (!NT_STATUS_IS_OK(status)) {
		return false;
	}

	ctx = libnet_context_init(torture->ev, torture->lp_ctx);
	if (ctx == NULL) {
		torture_comment(torture, "failed to create libnet context\n");
		ret = false;
		goto done;
	}

	ctx->cred = cmdline_credentials;

	mem_ctx = talloc_init("torture_domain_close_samr");

	/*
	 * querying the domain list using default buffer size
	 */

	ZERO_STRUCT(r);
	r.in.hostname = binding->host;

	status = libnet_DomainList(ctx, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		ret = false;
		goto done;
	}

	torture_comment(torture, "Received list or domains (everything in one piece):\n");

	for (i = 0; i < r.out.count; i++) {
		torture_comment(torture, "Name[%d]: %s\n", i, r.out.domains[i].name);
	}

	/*
	 * querying the domain list using specified (much smaller) buffer size
	 */

	ctx->samr.buf_size = 32;

	ZERO_STRUCT(r);
	r.in.hostname = binding->host;

	status = libnet_DomainList(ctx, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		ret = false;
		goto done;
	}

	torture_comment(torture, "Received list or domains (collected in more than one round):\n");

	for (i = 0; i < r.out.count; i++) {
		torture_comment(torture, "Name[%d]: %s\n", i, r.out.domains[i].name);
	}

done:
	torture_comment(torture, "\nStatus: %s\n", nt_errstr(status));

	talloc_free(mem_ctx);
	talloc_free(ctx);
	return ret;
}
