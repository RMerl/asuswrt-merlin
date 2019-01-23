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
#include "lib/cmdline/popt_common.h"
#include "libnet/libnet.h"
#include "libcli/security/security.h"
#include "librpc/gen_ndr/ndr_lsa.h"
#include "librpc/gen_ndr/ndr_samr.h"
#include "librpc/gen_ndr/ndr_srvsvc.h"
#include "torture/rpc/torture_rpc.h"
#include "param/param.h"


static bool test_connect_service(struct torture_context *tctx,
				 struct libnet_context *ctx,
				 const struct ndr_interface_table *iface,
				 const char *binding_string,
				 const char *hostname,
				 const enum libnet_RpcConnect_level level,
				 bool badcreds, NTSTATUS expected_status)
{
	NTSTATUS status;
	struct libnet_RpcConnect connect_r;
	ZERO_STRUCT(connect_r);

	connect_r.level            = level;
	connect_r.in.binding       = binding_string;
	connect_r.in.name          = hostname;
	connect_r.in.dcerpc_iface  = iface;

	/* if bad credentials are needed, set baduser%badpassword instead
	   of default commandline-passed credentials */
	if (badcreds) {
		cli_credentials_set_username(ctx->cred, "baduser", CRED_SPECIFIED);
		cli_credentials_set_password(ctx->cred, "badpassword", CRED_SPECIFIED);
	}

	status = libnet_RpcConnect(ctx, ctx, &connect_r);

	if (!NT_STATUS_EQUAL(status, expected_status)) {
		torture_comment(tctx, "Connecting to rpc service %s on %s.\n\tFAILED. Expected: %s."
		       "Received: %s\n",
		       connect_r.in.dcerpc_iface->name, connect_r.in.binding, nt_errstr(expected_status),
		       nt_errstr(status));

		return false;
	}

	torture_comment(tctx, "PASSED. Expected: %s, received: %s\n", nt_errstr(expected_status),
	       nt_errstr(status));

	if (connect_r.level == LIBNET_RPC_CONNECT_DC_INFO && NT_STATUS_IS_OK(status)) {
		torture_comment(tctx, "Domain Controller Info:\n");
		torture_comment(tctx, "\tDomain Name:\t %s\n", connect_r.out.domain_name);
		torture_comment(tctx, "\tDomain SID:\t %s\n", dom_sid_string(ctx, connect_r.out.domain_sid));
		torture_comment(tctx, "\tRealm:\t\t %s\n", connect_r.out.realm);
		torture_comment(tctx, "\tGUID:\t\t %s\n", GUID_string(ctx, connect_r.out.guid));

	} else if (!NT_STATUS_IS_OK(status)) {
		torture_comment(tctx, "Error string: %s\n", connect_r.out.error_string);
	}

	return true;
}


static bool torture_rpc_connect(struct torture_context *torture,
				const enum libnet_RpcConnect_level level,
				const char *bindstr, const char *hostname)
{
	struct libnet_context *ctx;

	ctx = libnet_context_init(torture->ev, torture->lp_ctx);
	ctx->cred = cmdline_credentials;

	torture_comment(torture, "Testing connection to LSA interface\n");

	if (!test_connect_service(torture, ctx, &ndr_table_lsarpc, bindstr,
				  hostname, level, false, NT_STATUS_OK)) {
		torture_comment(torture, "failed to connect LSA interface\n");
		return false;
	}

	torture_comment(torture, "Testing connection to SAMR interface\n");
	if (!test_connect_service(torture, ctx, &ndr_table_samr, bindstr,
				  hostname, level, false, NT_STATUS_OK)) {
		torture_comment(torture, "failed to connect SAMR interface\n");
		return false;
	}

	torture_comment(torture, "Testing connection to SRVSVC interface\n");
	if (!test_connect_service(torture, ctx, &ndr_table_srvsvc, bindstr,
				  hostname, level, false, NT_STATUS_OK)) {
		torture_comment(torture, "failed to connect SRVSVC interface\n");
		return false;
	}

	torture_comment(torture, "Testing connection to LSA interface with wrong credentials\n");
	if (!test_connect_service(torture, ctx, &ndr_table_lsarpc, bindstr,
				  hostname, level, true, NT_STATUS_LOGON_FAILURE)) {
		torture_comment(torture, "failed to test wrong credentials on LSA interface\n");
		return false;
	}

	torture_comment(torture, "Testing connection to SAMR interface with wrong credentials\n");
	if (!test_connect_service(torture, ctx, &ndr_table_samr, bindstr,
				  hostname, level, true, NT_STATUS_LOGON_FAILURE)) {
		torture_comment(torture, "failed to test wrong credentials on SAMR interface\n");
		return false;
	}

	talloc_free(ctx);

	return true;
}


bool torture_rpc_connect_srv(struct torture_context *torture)
{
	const enum libnet_RpcConnect_level level = LIBNET_RPC_CONNECT_SERVER;
	NTSTATUS status;
	struct dcerpc_binding *binding;

	status = torture_rpc_binding(torture, &binding);
	if (!NT_STATUS_IS_OK(status)) {
		return false;
	}

	return torture_rpc_connect(torture, level, NULL, binding->host);
}


bool torture_rpc_connect_pdc(struct torture_context *torture)
{
	const enum libnet_RpcConnect_level level = LIBNET_RPC_CONNECT_PDC;
	NTSTATUS status;
	struct dcerpc_binding *binding;
	const char *domain_name;

	status = torture_rpc_binding(torture, &binding);
	if (!NT_STATUS_IS_OK(status)) {
		return false;
	}

	/* we're accessing domain controller so the domain name should be
	   passed (it's going to be resolved to dc name and address) instead
	   of specific server name. */
	domain_name = lpcfg_workgroup(torture->lp_ctx);
	return torture_rpc_connect(torture, level, NULL, domain_name);
}


bool torture_rpc_connect_dc(struct torture_context *torture)
{
	const enum libnet_RpcConnect_level level = LIBNET_RPC_CONNECT_DC;
	NTSTATUS status;
	struct dcerpc_binding *binding;
	const char *domain_name;

	status = torture_rpc_binding(torture, &binding);
	if (!NT_STATUS_IS_OK(status)) {
		return false;
	}

	/* we're accessing domain controller so the domain name should be
	   passed (it's going to be resolved to dc name and address) instead
	   of specific server name. */
	domain_name = lpcfg_workgroup(torture->lp_ctx);
	return torture_rpc_connect(torture, level, NULL, domain_name);
}


bool torture_rpc_connect_dc_info(struct torture_context *torture)
{
	const enum libnet_RpcConnect_level level = LIBNET_RPC_CONNECT_DC_INFO;
	NTSTATUS status;
	struct dcerpc_binding *binding;
	const char *domain_name;

	status = torture_rpc_binding(torture, &binding);
	if (!NT_STATUS_IS_OK(status)) {
		return false;
	}

	/* we're accessing domain controller so the domain name should be
	   passed (it's going to be resolved to dc name and address) instead
	   of specific server name. */
	domain_name = lpcfg_workgroup(torture->lp_ctx);
	return torture_rpc_connect(torture, level, NULL, domain_name);
}


bool torture_rpc_connect_binding(struct torture_context *torture)
{
	const enum libnet_RpcConnect_level level = LIBNET_RPC_CONNECT_BINDING;
	NTSTATUS status;
	struct dcerpc_binding *binding;
	const char *bindstr;

	status = torture_rpc_binding(torture, &binding);
	if (!NT_STATUS_IS_OK(status)) {
		return false;
	}

	bindstr = dcerpc_binding_string(torture, binding);

	return torture_rpc_connect(torture, level, bindstr, NULL);
}
