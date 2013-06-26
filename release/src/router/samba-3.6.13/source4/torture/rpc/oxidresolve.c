/* 
   Unix SMB/CIFS implementation.
   test suite for oxidresolve operations

   Copyright (C) Jelmer Vernooij 2004
   
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
#include "librpc/gen_ndr/ndr_oxidresolver_c.h"
#include "librpc/gen_ndr/ndr_remact_c.h"
#include "librpc/gen_ndr/epmapper.h"
#include "torture/rpc/torture_rpc.h"

#define CLSID_IMAGEDOC "02B01C80-E03D-101A-B294-00DD010F2BF9"

const struct GUID IUnknown_uuid = {
	0x00000000,0x0000,0x0000,{0xc0,0x00},{0x00,0x00,0x00,0x00,0x00,0x46}
};

static bool test_RemoteActivation(struct torture_context *tctx, 
				 uint64_t *oxid, struct GUID *oid)
{
	struct RemoteActivation r;
	NTSTATUS status;
	struct GUID iids[2];
	uint16_t protseq[3] = { EPM_PROTOCOL_TCP, EPM_PROTOCOL_NCALRPC, EPM_PROTOCOL_UUID };
	struct dcerpc_pipe *p;
	struct dcerpc_binding_handle *b;

	status = torture_rpc_connection(tctx, &p, 
					&ndr_table_IRemoteActivation);
			
	if (!NT_STATUS_IS_OK(status)) {
		return false;
	}
	b = p->binding_handle;

	ZERO_STRUCT(r.in);
	r.in.this_object.version.MajorVersion = 5;
	r.in.this_object.version.MinorVersion = 1;
	r.in.this_object.cid = GUID_random();
	GUID_from_string(CLSID_IMAGEDOC, &r.in.Clsid);
	r.in.ClientImpLevel = RPC_C_IMP_LEVEL_IDENTIFY;
	r.in.num_protseqs = 3;
	r.in.protseq = protseq;
	r.in.Interfaces = 1;
	iids[0] = IUnknown_uuid;
	r.in.pIIDs = iids;
	r.out.pOxid = oxid;
	r.out.ipidRemUnknown = oid;

	status = dcerpc_RemoteActivation_r(b, tctx, &r);
	if(NT_STATUS_IS_ERR(status)) {
		fprintf(stderr, "RemoteActivation: %s\n", nt_errstr(status));
		return false;
	}

	if(!W_ERROR_IS_OK(r.out.result)) {
		fprintf(stderr, "RemoteActivation: %s\n", win_errstr(r.out.result));
		return false;
	}

	if(!W_ERROR_IS_OK(*r.out.hr)) {
		fprintf(stderr, "RemoteActivation: %s\n", win_errstr(*r.out.hr));
		return false;
	}

	if(!W_ERROR_IS_OK(r.out.results[0])) {
		fprintf(stderr, "RemoteActivation: %s\n", win_errstr(r.out.results[0]));
		return false;
	}

	return true;
}

static bool test_SimplePing(struct torture_context *tctx, 
			   struct dcerpc_pipe *p)
{
	struct SimplePing r;
	NTSTATUS status;
	uint64_t setid;
	struct dcerpc_binding_handle *b = p->binding_handle;

	r.in.SetId = &setid;

	status = dcerpc_SimplePing_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "SimplePing");
	torture_assert_werr_ok(tctx, r.out.result, "SimplePing");

	return true;
}

static bool test_ComplexPing(struct torture_context *tctx, 
			     struct dcerpc_pipe *p)
{
	struct ComplexPing r;
	NTSTATUS status;
	uint64_t setid;
	struct GUID oid;
	uint64_t oxid;
	struct dcerpc_binding_handle *b = p->binding_handle;

	if (!test_RemoteActivation(tctx, &oxid, &oid))
		return false;

	setid = 0;
	ZERO_STRUCT(r.in);

	r.in.SequenceNum = 0;
	r.in.SetId = &setid;
	r.in.cAddToSet = 1;
	r.in.AddToSet = &oid;

	status = dcerpc_ComplexPing_r(b, tctx, &r);
	if(NT_STATUS_IS_ERR(status)) {
		fprintf(stderr, "ComplexPing: %s\n", nt_errstr(status));
		return 0;
	}

	if(!W_ERROR_IS_OK(r.out.result)) {
		fprintf(stderr, "ComplexPing: %s\n", win_errstr(r.out.result));
		return 0;
	}

	

	return 1;
}

static bool test_ServerAlive(struct torture_context *tctx, 
			    struct dcerpc_pipe *p)
{
	struct ServerAlive r;
	NTSTATUS status;
	struct dcerpc_binding_handle *b = p->binding_handle;

	status = dcerpc_ServerAlive_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "ServerAlive");
	torture_assert_werr_ok(tctx, r.out.result, "ServerAlive");

	return true;
}

static bool test_ResolveOxid(struct torture_context *tctx, 
			     struct dcerpc_pipe *p)
{
	struct ResolveOxid r;
	NTSTATUS status;
	uint16_t protseq[2] = { EPM_PROTOCOL_TCP, EPM_PROTOCOL_SMB };	
	uint64_t oxid;
	struct GUID oid;
	struct dcerpc_binding_handle *b = p->binding_handle;

	if (!test_RemoteActivation(tctx, &oxid, &oid))
		return false;

	r.in.pOxid = oxid;
	r.in.cRequestedProtseqs = 2;
	r.in.arRequestedProtseqs = protseq;

	status = dcerpc_ResolveOxid_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "ResolveOxid");
	torture_assert_werr_ok(tctx, r.out.result, "ResolveOxid");

	return true;
}

static bool test_ResolveOxid2(struct torture_context *tctx, 
			      struct dcerpc_pipe *p)
{
	struct ResolveOxid2 r;
	NTSTATUS status;
	uint16_t protseq[2] = { EPM_PROTOCOL_TCP, EPM_PROTOCOL_SMB };	
	uint64_t oxid;
	struct GUID oid;
	struct dcerpc_binding_handle *b = p->binding_handle;

	if (!test_RemoteActivation(tctx, &oxid, &oid))
		return false;

	r.in.pOxid = oxid;
	r.in.cRequestedProtseqs = 2;
	r.in.arRequestedProtseqs = protseq;

	status = dcerpc_ResolveOxid2_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "ResolveOxid2");

	torture_assert_werr_ok(tctx, r.out.result, "ResolveOxid2");
	
	torture_comment(tctx, "Remote server versions: %d, %d\n", r.out.ComVersion->MajorVersion, r.out.ComVersion->MinorVersion);

	return true;
}

static bool test_ServerAlive2(struct torture_context *tctx, 
			     struct dcerpc_pipe *p)
{
	struct ServerAlive2 r;
	NTSTATUS status;
	struct dcerpc_binding_handle *b = p->binding_handle;

	status = dcerpc_ServerAlive2_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "ServerAlive2");
	torture_assert_werr_ok(tctx, r.out.result, "ServerAlive2");

	return true;
}

struct torture_suite *torture_rpc_oxidresolve(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "oxidresolve");
	struct torture_rpc_tcase *tcase;

	tcase = torture_suite_add_rpc_iface_tcase(suite, "oxidresolver", 
					  &ndr_table_IOXIDResolver);

	torture_rpc_tcase_add_test(tcase, "ServerAlive", test_ServerAlive);

	torture_rpc_tcase_add_test(tcase, "ServerAlive2", test_ServerAlive2);

	torture_rpc_tcase_add_test(tcase, "ComplexPing", test_ComplexPing);

	torture_rpc_tcase_add_test(tcase, "SimplePing", test_SimplePing);
	
	torture_rpc_tcase_add_test(tcase, "ResolveOxid", test_ResolveOxid);

	torture_rpc_tcase_add_test(tcase, "ResolveOxid2", test_ResolveOxid2);

	return suite;
}
