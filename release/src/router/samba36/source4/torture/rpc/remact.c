/* 
   Unix SMB/CIFS implementation.
   test suite for remoteactivation operations

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
#include "librpc/gen_ndr/ndr_remact_c.h"
#include "librpc/gen_ndr/ndr_epmapper_c.h"
#include "torture/rpc/torture_rpc.h"

#define CLSID_IMAGEDOC "02B01C80-E03D-101A-B294-00DD010F2BF9"
#define DCERPC_IUNKNOWN_UUID "00000000-0000-0000-c000-000000000046"
#define DCERPC_ICLASSFACTORY_UUID "00000001-0000-0000-c000-000000000046"

static bool test_RemoteActivation(struct torture_context *tctx, 
								 struct dcerpc_pipe *p)
{
	struct RemoteActivation r;
	NTSTATUS status;
	struct GUID iids[1];
	uint16_t protseq[3] = { EPM_PROTOCOL_TCP, EPM_PROTOCOL_NCALRPC, EPM_PROTOCOL_UUID };
	struct dcerpc_binding_handle *b = p->binding_handle;

	ZERO_STRUCT(r.in);
	r.in.this_object.version.MajorVersion = 5;
	r.in.this_object.version.MinorVersion = 1;
	r.in.this_object.cid = GUID_random();
	GUID_from_string(CLSID_IMAGEDOC, &r.in.Clsid);
	r.in.ClientImpLevel = RPC_C_IMP_LEVEL_IDENTIFY;
	r.in.num_protseqs = 3;
	r.in.protseq = protseq;
	r.in.Interfaces = 1;
	GUID_from_string(DCERPC_IUNKNOWN_UUID, &iids[0]);
	r.in.pIIDs = iids;

	status = dcerpc_RemoteActivation_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "RemoteActivation");

	torture_assert_werr_ok(tctx, r.out.result, "RemoteActivation");

	torture_assert_werr_ok(tctx, *r.out.hr, "RemoteActivation");

	torture_assert_werr_ok(tctx, r.out.results[0], "RemoteActivation");

	GUID_from_string(DCERPC_ICLASSFACTORY_UUID, &iids[0]);
	r.in.Interfaces = 1;
	r.in.Mode = MODE_GET_CLASS_OBJECT;

	status = dcerpc_RemoteActivation_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, 
							   "RemoteActivation(GetClassObject)");

	torture_assert_werr_ok(tctx, r.out.result, 
						   "RemoteActivation(GetClassObject)");

	torture_assert_werr_ok(tctx, *r.out.hr, "RemoteActivation(GetClassObject)");

	torture_assert_werr_ok(tctx, r.out.results[0], 
						   "RemoteActivation(GetClassObject)");

	return true;
}

struct torture_suite *torture_rpc_remact(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "remact");
	struct torture_rpc_tcase *tcase;

	tcase = torture_suite_add_rpc_iface_tcase(suite, "remact", &ndr_table_IRemoteActivation);

	torture_rpc_tcase_add_test(tcase, "RemoteActivation", test_RemoteActivation);

	return suite;
}
