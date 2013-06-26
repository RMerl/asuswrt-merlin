/*
   Unix SMB/CIFS implementation.
   test suite for rpc ntsvcs operations

   Copyright (C) Guenther Deschner 2008

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "includes.h"
#include "torture/rpc/torture_rpc.h"
#include "librpc/gen_ndr/ndr_ntsvcs_c.h"

static bool test_PNP_GetVersion(struct torture_context *tctx,
				struct dcerpc_pipe *p)
{
	struct dcerpc_binding_handle *b = p->binding_handle;
	NTSTATUS status;
	struct PNP_GetVersion r;
	uint16_t version = 0;

	r.out.version = &version;

	status = dcerpc_PNP_GetVersion_r(b, tctx, &r);

	torture_assert_ntstatus_ok(tctx, status, "PNP_GetVersion");
	torture_assert_werr_ok(tctx, r.out.result, "PNP_GetVersion");
	torture_assert_int_equal(tctx, version, 0x400, "invalid version");

	return true;
}

static bool test_PNP_GetDeviceListSize(struct torture_context *tctx,
				       struct dcerpc_pipe *p)
{
	struct dcerpc_binding_handle *b = p->binding_handle;
	struct PNP_GetDeviceListSize r;
	uint32_t size = 0;

	r.in.devicename = NULL;
	r.in.flags = CM_GETIDLIST_FILTER_SERVICE;
	r.out.size = &size;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_PNP_GetDeviceListSize_r(b, tctx, &r),
		"PNP_GetDeviceListSize");
	torture_assert_werr_equal(tctx, r.out.result, WERR_CM_INVALID_POINTER,
		"PNP_GetDeviceListSize");

	r.in.devicename = "Spooler";

	torture_assert_ntstatus_ok(tctx,
		dcerpc_PNP_GetDeviceListSize_r(b, tctx, &r),
		"PNP_GetDeviceListSize");
	torture_assert_werr_ok(tctx, r.out.result,
		"PNP_GetDeviceListSize");

	return true;
}

static bool test_PNP_GetDeviceList(struct torture_context *tctx,
				   struct dcerpc_pipe *p)
{
	struct dcerpc_binding_handle *b = p->binding_handle;
	struct PNP_GetDeviceList r;
	uint16_t *buffer = NULL;
	uint32_t length = 0;

	buffer = talloc_array(tctx, uint16_t, 0);

	r.in.filter = NULL;
	r.in.flags = CM_GETIDLIST_FILTER_SERVICE;
	r.in.length = &length;
	r.out.length = &length;
	r.out.buffer = buffer;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_PNP_GetDeviceList_r(b, tctx, &r),
		"PNP_GetDeviceList failed");
	torture_assert_werr_equal(tctx, r.out.result, WERR_CM_INVALID_POINTER,
		"PNP_GetDeviceList failed");

	r.in.filter = "Spooler";

	torture_assert_ntstatus_ok(tctx,
		dcerpc_PNP_GetDeviceList_r(b, tctx, &r),
		"PNP_GetDeviceList failed");

	if (W_ERROR_EQUAL(r.out.result, WERR_CM_BUFFER_SMALL)) {
		struct PNP_GetDeviceListSize s;

		s.in.devicename = "Spooler";
		s.in.flags = CM_GETIDLIST_FILTER_SERVICE;
		s.out.size = &length;

		torture_assert_ntstatus_ok(tctx,
			dcerpc_PNP_GetDeviceListSize_r(b, tctx, &s),
			"PNP_GetDeviceListSize failed");
		torture_assert_werr_ok(tctx, s.out.result,
			"PNP_GetDeviceListSize failed");
	}

	buffer = talloc_array(tctx, uint16_t, length);

	r.in.length = &length;
	r.out.length = &length;
	r.out.buffer = buffer;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_PNP_GetDeviceList_r(b, tctx, &r),
		"PNP_GetDeviceList failed");

	torture_assert_werr_ok(tctx, r.out.result,
		"PNP_GetDeviceList failed");

	return true;
}

static bool test_PNP_GetDeviceRegProp(struct torture_context *tctx,
				      struct dcerpc_pipe *p)
{
	struct dcerpc_binding_handle *b = p->binding_handle;
	NTSTATUS status;
	struct PNP_GetDeviceRegProp r;

	enum winreg_Type reg_data_type = REG_NONE;
	uint32_t buffer_size = 0;
	uint32_t needed = 0;
	uint8_t *buffer;

	buffer = talloc(tctx, uint8_t);

	r.in.devicepath = "ACPI\\ACPI0003\\1";
	r.in.property = DEV_REGPROP_DESC;
	r.in.flags = 0;
	r.in.reg_data_type = &reg_data_type;
	r.in.buffer_size = &buffer_size;
	r.in.needed = &needed;
	r.out.buffer = buffer;
	r.out.reg_data_type = &reg_data_type;
	r.out.buffer_size = &buffer_size;
	r.out.needed = &needed;

	status = dcerpc_PNP_GetDeviceRegProp_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "PNP_GetDeviceRegProp");

	if (W_ERROR_EQUAL(r.out.result, WERR_CM_BUFFER_SMALL)) {

		buffer = talloc_array(tctx, uint8_t, needed);
		r.in.buffer_size = &needed;

		status = dcerpc_PNP_GetDeviceRegProp_r(b, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "PNP_GetDeviceRegProp");
	}

	return true;
}

struct torture_suite *torture_rpc_ntsvcs(TALLOC_CTX *mem_ctx)
{
	struct torture_rpc_tcase *tcase;
	struct torture_suite *suite = torture_suite_create(mem_ctx, "ntsvcs");
	struct torture_test *test;

	tcase = torture_suite_add_rpc_iface_tcase(suite, "ntsvcs",
						  &ndr_table_ntsvcs);

	test = torture_rpc_tcase_add_test(tcase, "PNP_GetDeviceRegProp",
					  test_PNP_GetDeviceRegProp);
	test = torture_rpc_tcase_add_test(tcase, "PNP_GetDeviceList",
					  test_PNP_GetDeviceList);
	test = torture_rpc_tcase_add_test(tcase, "PNP_GetDeviceListSize",
					  test_PNP_GetDeviceListSize);
	test = torture_rpc_tcase_add_test(tcase, "PNP_GetVersion",
					  test_PNP_GetVersion);

	return suite;
}
