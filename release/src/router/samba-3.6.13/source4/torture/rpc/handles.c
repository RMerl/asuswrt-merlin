/* 
   Unix SMB/CIFS implementation.

   test suite for behaviour of rpc policy handles

   Copyright (C) Andrew Tridgell 2007
   
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
#include "librpc/gen_ndr/ndr_samr_c.h"
#include "librpc/gen_ndr/ndr_lsa_c.h"
#include "librpc/gen_ndr/ndr_drsuapi_c.h"
#include "torture/rpc/torture_rpc.h"

/*
  this tests the use of policy handles between connections
*/

static bool test_handles_lsa(struct torture_context *torture)
{
	NTSTATUS status;
	struct dcerpc_pipe *p1, *p2;
	struct dcerpc_binding_handle *b1, *b2;
	struct policy_handle handle;
	struct policy_handle handle2;
	struct lsa_ObjectAttribute attr;
	struct lsa_QosInfo qos;
	struct lsa_OpenPolicy r;
	struct lsa_Close c;
	uint16_t system_name = '\\';
	TALLOC_CTX *mem_ctx = talloc_new(torture);

	torture_comment(torture, "RPC-HANDLE-LSARPC\n");

	status = torture_rpc_connection(torture, &p1, &ndr_table_lsarpc);
	torture_assert_ntstatus_ok(torture, status, "opening lsa pipe1");
	b1 = p1->binding_handle;

	status = torture_rpc_connection(torture, &p2, &ndr_table_lsarpc);
	torture_assert_ntstatus_ok(torture, status, "opening lsa pipe1");
	b2 = p2->binding_handle;

	qos.len = 0;
	qos.impersonation_level = 2;
	qos.context_mode = 1;
	qos.effective_only = 0;

	attr.len = 0;
	attr.root_dir = NULL;
	attr.object_name = NULL;
	attr.attributes = 0;
	attr.sec_desc = NULL;
	attr.sec_qos = &qos;

	r.in.system_name = &system_name;
	r.in.attr = &attr;
	r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r.out.handle = &handle;

	torture_assert_ntstatus_ok(torture, dcerpc_lsa_OpenPolicy_r(b1, mem_ctx, &r),
		"OpenPolicy failed");
	if (!NT_STATUS_IS_OK(r.out.result)) {
		torture_comment(torture, "lsa_OpenPolicy not supported - skipping\n");
		talloc_free(mem_ctx);
		return true;
	}

	c.in.handle = &handle;
	c.out.handle = &handle2;

	status = dcerpc_lsa_Close_r(b2, mem_ctx, &c);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_RPC_SS_CONTEXT_MISMATCH,
				      "closing policy handle on p2");

	torture_assert_ntstatus_ok(torture, dcerpc_lsa_Close_r(b1, mem_ctx, &c),
		"Close failed");
	torture_assert_ntstatus_ok(torture, c.out.result, "closing policy handle on p1");

	status = dcerpc_lsa_Close_r(b1, mem_ctx, &c);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_RPC_SS_CONTEXT_MISMATCH,
				      "closing policy handle on p1 again");

	talloc_free(mem_ctx);

	return true;
}

static bool test_handles_lsa_shared(struct torture_context *torture)
{
	NTSTATUS status;
	struct dcerpc_pipe *p1, *p2, *p3, *p4, *p5;
	struct dcerpc_binding_handle *b1, *b2, *b3, *b4;
	struct policy_handle handle;
	struct policy_handle handle2;
	struct lsa_ObjectAttribute attr;
	struct lsa_QosInfo qos;
	struct lsa_OpenPolicy r;
	struct lsa_Close c;
	struct lsa_QuerySecurity qsec;
	struct sec_desc_buf *sdbuf = NULL;
	uint16_t system_name = '\\';
	TALLOC_CTX *mem_ctx = talloc_new(torture);
	enum dcerpc_transport_t transport;
	uint32_t assoc_group_id;

	torture_comment(torture, "RPC-HANDLE-LSARPC-SHARED\n");

	torture_comment(torture, "connect lsa pipe1\n");
	status = torture_rpc_connection(torture, &p1, &ndr_table_lsarpc);
	torture_assert_ntstatus_ok(torture, status, "opening lsa pipe1");
	b1 = p1->binding_handle;

	transport	= p1->conn->transport.transport;
	assoc_group_id	= p1->assoc_group_id;

	torture_comment(torture, "use assoc_group_id[0x%08X] for new connections\n", assoc_group_id);

	torture_comment(torture, "connect lsa pipe2\n");
	status = torture_rpc_connection_transport(torture, &p2, &ndr_table_lsarpc,
						  transport,
						  assoc_group_id);
	torture_assert_ntstatus_ok(torture, status, "opening lsa pipe2");
	b2 = p2->binding_handle;

	torture_comment(torture, "got assoc_group_id[0x%08X] for p2\n", 
			p2->assoc_group_id);

	qos.len = 0;
	qos.impersonation_level = 2;
	qos.context_mode = 1;
	qos.effective_only = 0;

	attr.len = 0;
	attr.root_dir = NULL;
	attr.object_name = NULL;
	attr.attributes = 0;
	attr.sec_desc = NULL;
	attr.sec_qos = &qos;

	r.in.system_name = &system_name;
	r.in.attr = &attr;
	r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r.out.handle = &handle;

	torture_comment(torture, "open lsa policy handle\n");
	torture_assert_ntstatus_ok(torture, dcerpc_lsa_OpenPolicy_r(b1, mem_ctx, &r),
		"OpenPolicy failed");
	if (!NT_STATUS_IS_OK(r.out.result)) {
		torture_comment(torture, "lsa_OpenPolicy not supported - skipping\n");
		talloc_free(mem_ctx);
		return true;
	}

	/*
	 * connect p3 after the policy handle is opened
	 */
	torture_comment(torture, "connect lsa pipe3 after the policy handle is opened\n");
	status = torture_rpc_connection_transport(torture, &p3, &ndr_table_lsarpc,
						  transport,
						  assoc_group_id);
	torture_assert_ntstatus_ok(torture, status, "opening lsa pipe3");
	b3 = p3->binding_handle;

	qsec.in.handle 		= &handle;
	qsec.in.sec_info	= 0;
	qsec.out.sdbuf		= &sdbuf;
	c.in.handle = &handle;
	c.out.handle = &handle2;

	/*
	 * use policy handle on all 3 connections
	 */
	torture_comment(torture, "use the policy handle on p1,p2,p3\n");
	torture_assert_ntstatus_ok(torture, dcerpc_lsa_QuerySecurity_r(b1, mem_ctx, &qsec),
		"QuerySecurity failed");
	torture_assert_ntstatus_equal(torture, qsec.out.result, NT_STATUS_OK,
				      "use policy handle on p1");

	torture_assert_ntstatus_ok(torture, dcerpc_lsa_QuerySecurity_r(b2, mem_ctx, &qsec),
		"QuerySecurity failed");
	torture_assert_ntstatus_equal(torture, qsec.out.result, NT_STATUS_OK,
				      "use policy handle on p2");

	torture_assert_ntstatus_ok(torture, dcerpc_lsa_QuerySecurity_r(b3, mem_ctx, &qsec),
		"QuerySecurity failed");
	torture_assert_ntstatus_equal(torture, qsec.out.result, NT_STATUS_OK,
				      "use policy handle on p3");

	/*
	 * close policy handle on connection 2 and the others get a fault
	 */
	torture_comment(torture, "close the policy handle on p2 others get a fault\n");
	torture_assert_ntstatus_ok(torture, dcerpc_lsa_Close_r(b2, mem_ctx, &c),
		"Close failed");
	torture_assert_ntstatus_equal(torture, c.out.result, NT_STATUS_OK,
				      "closing policy handle on p2");

	status = dcerpc_lsa_Close_r(b1, mem_ctx, &c);

	torture_assert_ntstatus_equal(torture, status, NT_STATUS_RPC_SS_CONTEXT_MISMATCH,
				      "closing policy handle on p1 again");

	status = dcerpc_lsa_Close_r(b3, mem_ctx, &c);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_RPC_SS_CONTEXT_MISMATCH,
				      "closing policy handle on p3");

	status = dcerpc_lsa_Close_r(b2, mem_ctx, &c);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_RPC_SS_CONTEXT_MISMATCH,
				      "closing policy handle on p2 again");

	/*
	 * open a new policy handle on p3
	 */
	torture_comment(torture, "open a new policy handle on p3\n");
	torture_assert_ntstatus_ok(torture, dcerpc_lsa_OpenPolicy_r(b3, mem_ctx, &r),
		"OpenPolicy failed");
	torture_assert_ntstatus_equal(torture, r.out.result, NT_STATUS_OK,
				      "open policy handle on p3");

	/*
	 * use policy handle on all 3 connections
	 */
	torture_comment(torture, "use the policy handle on p1,p2,p3\n");
	torture_assert_ntstatus_ok(torture, dcerpc_lsa_QuerySecurity_r(b1, mem_ctx, &qsec),
		"Query Security failed");
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_OK, 
				      "use policy handle on p1");

	torture_assert_ntstatus_ok(torture, dcerpc_lsa_QuerySecurity_r(b2, mem_ctx, &qsec),
		"Query Security failed");
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_OK, 
				      "use policy handle on p2");

	torture_assert_ntstatus_ok(torture, dcerpc_lsa_QuerySecurity_r(b3, mem_ctx, &qsec),
		"Query Security failed");
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_OK, 
				      "use policy handle on p3");

	/*
	 * close policy handle on connection 2 and the others get a fault
	 */
	torture_comment(torture, "close the policy handle on p2 others get a fault\n");
	torture_assert_ntstatus_ok(torture, dcerpc_lsa_Close_r(b2, mem_ctx, &c),
		"Close failed");
	torture_assert_ntstatus_equal(torture, c.out.result, NT_STATUS_OK,
				      "closing policy handle on p2");

	status = dcerpc_lsa_Close_r(b1, mem_ctx, &c);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_RPC_SS_CONTEXT_MISMATCH,
				      "closing policy handle on p1 again");

	status = dcerpc_lsa_Close_r(b3, mem_ctx, &c);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_RPC_SS_CONTEXT_MISMATCH,
				      "closing policy handle on p3");

	status = dcerpc_lsa_Close_r(b2, mem_ctx, &c);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_RPC_SS_CONTEXT_MISMATCH,
				      "closing policy handle on p2 again");

	/*
	 * open a new policy handle
	 */
	torture_comment(torture, "open a new policy handle on p1 and use it\n");
	torture_assert_ntstatus_ok(torture, dcerpc_lsa_OpenPolicy_r(b1, mem_ctx, &r),
		"OpenPolicy failed");
	torture_assert_ntstatus_equal(torture, r.out.result, NT_STATUS_OK,
				      "open 2nd policy handle on p1");

	torture_assert_ntstatus_ok(torture, dcerpc_lsa_QuerySecurity_r(b1, mem_ctx, &qsec),
		"QuerySecurity failed");
	torture_assert_ntstatus_equal(torture, qsec.out.result, NT_STATUS_OK,
				      "QuerySecurity handle on p1");

	/* close first connection */
	torture_comment(torture, "disconnect p1\n");
	talloc_free(p1);
	smb_msleep(5);

	/*
	 * and it's still available on p2,p3
	 */
	torture_comment(torture, "use policy handle on p2,p3\n");
	torture_assert_ntstatus_ok(torture, dcerpc_lsa_QuerySecurity_r(b2, mem_ctx, &qsec),
		"QuerySecurity failed");
	torture_assert_ntstatus_equal(torture, qsec.out.result, NT_STATUS_OK,
				      "QuerySecurity handle on p2 after p1 was disconnected");

	torture_assert_ntstatus_ok(torture, dcerpc_lsa_QuerySecurity_r(b3, mem_ctx, &qsec),
		"QuerySecurity failed");
	torture_assert_ntstatus_equal(torture, qsec.out.result, NT_STATUS_OK,
				      "QuerySecurity handle on p3 after p1 was disconnected");

	/*
	 * now open p4
	 * and use the handle on it
	 */
	torture_comment(torture, "connect lsa pipe4 and use policy handle\n");
	status = torture_rpc_connection_transport(torture, &p4, &ndr_table_lsarpc,
						  transport,
						  assoc_group_id);
	torture_assert_ntstatus_ok(torture, status, "opening lsa pipe4");
	b4 = p4->binding_handle;

	torture_assert_ntstatus_ok(torture, dcerpc_lsa_QuerySecurity_r(b4, mem_ctx, &qsec),
		"QuerySecurity failed");
	torture_assert_ntstatus_equal(torture, qsec.out.result, NT_STATUS_OK,
				      "using policy handle on p4");

	/*
	 * now close p2,p3,p4
	 * without closing the policy handle
	 */
	torture_comment(torture, "disconnect p2,p3,p4\n");
	talloc_free(p2);
	talloc_free(p3);
	talloc_free(p4);
	smb_msleep(10);

	/*
	 * now open p5
	 */
	torture_comment(torture, "connect lsa pipe5 - should fail\n");
	status = torture_rpc_connection_transport(torture, &p5, &ndr_table_lsarpc,
						  transport,
						  assoc_group_id);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_UNSUCCESSFUL,
				      "opening lsa pipe5");

	talloc_free(mem_ctx);

	return true;
}


static bool test_handles_samr(struct torture_context *torture)
{
	NTSTATUS status;
	struct dcerpc_pipe *p1, *p2;
	struct dcerpc_binding_handle *b1, *b2;
	struct policy_handle handle;
	struct policy_handle handle2;
	struct samr_Connect r;
	struct samr_Close c;
	TALLOC_CTX *mem_ctx = talloc_new(torture);

	torture_comment(torture, "RPC-HANDLE-SAMR\n");

	status = torture_rpc_connection(torture, &p1, &ndr_table_samr);
	torture_assert_ntstatus_ok(torture, status, "opening samr pipe1");
	b1 = p1->binding_handle;

	status = torture_rpc_connection(torture, &p2, &ndr_table_samr);
	torture_assert_ntstatus_ok(torture, status, "opening samr pipe2");
	b2 = p2->binding_handle;

	r.in.system_name = 0;
	r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r.out.connect_handle = &handle;

	torture_assert_ntstatus_ok(torture, dcerpc_samr_Connect_r(b1, mem_ctx, &r),
		"Connect failed");
	torture_assert_ntstatus_ok(torture, r.out.result, "opening policy handle on p1");

	c.in.handle = &handle;
	c.out.handle = &handle2;

	status = dcerpc_samr_Close_r(b2, mem_ctx, &c);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_RPC_SS_CONTEXT_MISMATCH,
				      "closing policy handle on p2");

	torture_assert_ntstatus_ok(torture, dcerpc_samr_Close_r(b1, mem_ctx, &c),
		"Close failed");
	torture_assert_ntstatus_ok(torture, c.out.result, "closing policy handle on p1");

	status = dcerpc_samr_Close_r(b1, mem_ctx, &c);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_RPC_SS_CONTEXT_MISMATCH,
				      "closing policy handle on p1 again");
	
	talloc_free(mem_ctx);

	return true;
}

static bool test_handles_mixed_shared(struct torture_context *torture)
{
	NTSTATUS status;
	struct dcerpc_pipe *p1, *p2, *p3, *p4, *p5, *p6;
	struct dcerpc_binding_handle *b1, *b2;
	struct policy_handle handle;
	struct policy_handle handle2;
	struct samr_Connect r;
	struct lsa_Close lc;
	struct samr_Close sc;
	TALLOC_CTX *mem_ctx = talloc_new(torture);
	enum dcerpc_transport_t transport;
	uint32_t assoc_group_id;

	torture_comment(torture, "RPC-HANDLE-MIXED-SHARED\n");

	torture_comment(torture, "connect samr pipe1\n");
	status = torture_rpc_connection(torture, &p1, &ndr_table_samr);
	torture_assert_ntstatus_ok(torture, status, "opening samr pipe1");
	b1 = p1->binding_handle;

	transport	= p1->conn->transport.transport;
	assoc_group_id	= p1->assoc_group_id;

	torture_comment(torture, "use assoc_group_id[0x%08X] for new connections\n", assoc_group_id);

	torture_comment(torture, "connect lsa pipe2\n");
	status = torture_rpc_connection_transport(torture, &p2, &ndr_table_lsarpc,
						  transport,
						  assoc_group_id);
	torture_assert_ntstatus_ok(torture, status, "opening lsa pipe2");
	b2 = p2->binding_handle;

	torture_comment(torture, "got assoc_group_id[0x%08X] for p2\n", 
			p2->assoc_group_id);
	r.in.system_name = 0;
	r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r.out.connect_handle = &handle;

	torture_comment(torture, "samr_Connect to open a policy handle on samr p1\n");
	torture_assert_ntstatus_ok(torture, dcerpc_samr_Connect_r(b1, mem_ctx, &r),
		"Connect failed");
	torture_assert_ntstatus_ok(torture, r.out.result, "opening policy handle on p1");

	lc.in.handle 		= &handle;
	lc.out.handle		= &handle2;
	sc.in.handle		= &handle;
	sc.out.handle		= &handle2;

	torture_comment(torture, "use policy handle on lsa p2 - should fail\n");
	status = dcerpc_lsa_Close_r(b2, mem_ctx, &lc);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_RPC_SS_CONTEXT_MISMATCH,
				      "closing handle on lsa p2");

	torture_comment(torture, "closing policy handle on samr p1\n");
	torture_assert_ntstatus_ok(torture, dcerpc_samr_Close_r(b1, mem_ctx, &sc),
		"Close failed");
	torture_assert_ntstatus_ok(torture, sc.out.result, "closing policy handle on p1");

	talloc_free(p1);
	talloc_free(p2);
	smb_msleep(10);

	torture_comment(torture, "connect samr pipe3 - should fail\n");
	status = torture_rpc_connection_transport(torture, &p3, &ndr_table_samr,
						  transport,
						  assoc_group_id);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_UNSUCCESSFUL,
				      "opening samr pipe3");

	torture_comment(torture, "connect lsa pipe4 - should fail\n");
	status = torture_rpc_connection_transport(torture, &p4, &ndr_table_lsarpc,
						  transport,
						  assoc_group_id);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_UNSUCCESSFUL,
				      "opening lsa pipe4");

	torture_comment(torture, "connect samr pipe5 with assoc_group_id[0x%08X]- should fail\n", ++assoc_group_id);
	status = torture_rpc_connection_transport(torture, &p5, &ndr_table_samr,
						  transport,
						  assoc_group_id);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_UNSUCCESSFUL,
				      "opening samr pipe5");

	torture_comment(torture, "connect lsa pipe6 with assoc_group_id[0x%08X]- should fail\n", ++assoc_group_id);
	status = torture_rpc_connection_transport(torture, &p6, &ndr_table_lsarpc,
						  transport,
						  assoc_group_id);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_UNSUCCESSFUL,
				      "opening lsa pipe6");

	talloc_free(mem_ctx);

	return true;
}

static bool test_handles_random_assoc(struct torture_context *torture)
{
	NTSTATUS status;
	struct dcerpc_pipe *p1, *p2, *p3;
	TALLOC_CTX *mem_ctx = talloc_new(torture);
	enum dcerpc_transport_t transport;
	uint32_t assoc_group_id;

	torture_comment(torture, "RPC-HANDLE-RANDOM-ASSOC\n");

	torture_comment(torture, "connect samr pipe1\n");
	status = torture_rpc_connection(torture, &p1, &ndr_table_samr);
	torture_assert_ntstatus_ok(torture, status, "opening samr pipe1");

	transport	= p1->conn->transport.transport;
	assoc_group_id	= p1->assoc_group_id;

	torture_comment(torture, "pipe1 uses assoc_group_id[0x%08X]\n", assoc_group_id);

	torture_comment(torture, "connect samr pipe2 with assoc_group_id[0x%08X]- should fail\n", ++assoc_group_id);
	status = torture_rpc_connection_transport(torture, &p2, &ndr_table_samr,
						  transport,
						  assoc_group_id);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_UNSUCCESSFUL,
				      "opening samr pipe2");

	torture_comment(torture, "connect samr pipe3 with assoc_group_id[0x%08X]- should fail\n", ++assoc_group_id);
	status = torture_rpc_connection_transport(torture, &p3, &ndr_table_samr,
						  transport,
						  assoc_group_id);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_UNSUCCESSFUL,
				      "opening samr pipe3");

	talloc_free(mem_ctx);

	return true;
}


static bool test_handles_drsuapi(struct torture_context *torture)
{
	NTSTATUS status;
	struct dcerpc_pipe *p1, *p2;
	struct dcerpc_binding_handle *b1, *b2;
	struct policy_handle handle;
	struct policy_handle handle2;
	struct GUID bind_guid;
	struct drsuapi_DsBind r;
	struct drsuapi_DsUnbind c;
	TALLOC_CTX *mem_ctx = talloc_new(torture);

	torture_comment(torture, "RPC-HANDLE-DRSUAPI\n");

	status = torture_rpc_connection(torture, &p1, &ndr_table_drsuapi);
	torture_assert_ntstatus_ok(torture, status, "opening drsuapi pipe1");
	b1 = p1->binding_handle;

	status = torture_rpc_connection(torture, &p2, &ndr_table_drsuapi);
	torture_assert_ntstatus_ok(torture, status, "opening drsuapi pipe1");
	b2 = p2->binding_handle;

	GUID_from_string(DRSUAPI_DS_BIND_GUID, &bind_guid);

	r.in.bind_guid = &bind_guid;
	r.in.bind_info = NULL;
	r.out.bind_handle = &handle;

	status = dcerpc_drsuapi_DsBind_r(b1, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(torture, "drsuapi_DsBind not supported - skipping\n");
		talloc_free(mem_ctx);
		return true;
	}

	c.in.bind_handle = &handle;
	c.out.bind_handle = &handle2;

	status = dcerpc_drsuapi_DsUnbind_r(b2, mem_ctx, &c);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_RPC_SS_CONTEXT_MISMATCH,
				      "closing policy handle on p2");

	status = dcerpc_drsuapi_DsUnbind_r(b1, mem_ctx, &c);
	torture_assert_ntstatus_ok(torture, status, "closing policy handle on p1");

	status = dcerpc_drsuapi_DsUnbind_r(b1, mem_ctx, &c);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_RPC_SS_CONTEXT_MISMATCH,
				      "closing policy handle on p1 again");
	
	talloc_free(mem_ctx);

	return true;
}

struct torture_suite *torture_rpc_handles(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite;

	suite = torture_suite_create(mem_ctx, "handles");
	torture_suite_add_simple_test(suite, "lsarpc", test_handles_lsa);
	torture_suite_add_simple_test(suite, "lsarpc-shared", test_handles_lsa_shared);
	torture_suite_add_simple_test(suite, "samr", test_handles_samr);
	torture_suite_add_simple_test(suite, "mixed-shared", test_handles_mixed_shared);
	torture_suite_add_simple_test(suite, "random-assoc", test_handles_random_assoc);
	torture_suite_add_simple_test(suite, "drsuapi", test_handles_drsuapi);
	return suite;
}
