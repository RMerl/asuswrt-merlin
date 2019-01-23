/*
   Unix SMB/CIFS implementation.
   test suite for spoolss rpc operations as performed by various win versions

   Copyright (C) Kai Blin 2007

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
#include "librpc/gen_ndr/ndr_spoolss_c.h"
#include "librpc/gen_ndr/ndr_misc.h"
#include "ntvfs/ntvfs.h"
#include "param/param.h"

struct test_spoolss_win_context {
	/* EnumPrinters */
	uint32_t printer_count;
	union spoolss_PrinterInfo *printer_info;
	union spoolss_PrinterInfo *current_info;

	/* EnumPrinterKeys */
	const char **printer_keys;

	bool printer_has_driver;
};

/* This is a convenience function for all OpenPrinterEx calls */
static bool test_OpenPrinterEx(struct torture_context *tctx,
				struct dcerpc_binding_handle *b,
				struct policy_handle *handle,
				const char *printer_name,
				uint32_t access_mask)
{
	NTSTATUS status;
	struct spoolss_OpenPrinterEx op;
	struct spoolss_UserLevel1 ul_1;

	torture_comment(tctx, "Opening printer '%s'\n", printer_name);

	op.in.printername		= talloc_strdup(tctx, printer_name);
	op.in.datatype			= NULL;
	op.in.devmode_ctr.devmode	= NULL;
	op.in.access_mask		= access_mask;
	op.in.level			= 1;
	op.in.userlevel.level1		= &ul_1;
	op.out.handle			= handle;

	ul_1.size 			= 1234;
	ul_1.client			= "\\clientname";
	ul_1.user			= "username";
	ul_1.build			= 1;
	ul_1.major			= 2;
	ul_1.minor			= 3;
	ul_1.processor			= 4567;

	status = dcerpc_spoolss_OpenPrinterEx_r(b, tctx, &op);
	torture_assert_ntstatus_ok(tctx, status, "OpenPrinterEx failed");
	torture_assert_werr_ok(tctx, op.out.result, "OpenPrinterEx failed");

	return true;
}

static bool test_OpenPrinterAsAdmin(struct torture_context *tctx,
					struct dcerpc_binding_handle *b,
					const char *printername)
{
	NTSTATUS status;
	struct spoolss_OpenPrinterEx op;
	struct spoolss_ClosePrinter cp;
	struct spoolss_UserLevel1 ul_1;
	struct policy_handle handle;

	ul_1.size 			= 1234;
	ul_1.client			= "\\clientname";
	ul_1.user			= "username";
	ul_1.build			= 1;
	ul_1.major			= 2;
	ul_1.minor			= 3;
	ul_1.processor			= 4567;

	op.in.printername		= talloc_strdup(tctx, printername);
	op.in.datatype			= NULL;
	op.in.devmode_ctr.devmode	= NULL;
	op.in.access_mask		= SERVER_ALL_ACCESS;
	op.in.level			= 1;
	op.in.userlevel.level1		= &ul_1;
	op.out.handle			= &handle;

	cp.in.handle			= &handle;
	cp.out.handle			= &handle;

	torture_comment(tctx, "Testing OpenPrinterEx(%s) with admin rights\n",
			op.in.printername);

	status = dcerpc_spoolss_OpenPrinterEx_r(b, tctx, &op);

	if (NT_STATUS_IS_OK(status) && W_ERROR_IS_OK(op.out.result)) {
		status = dcerpc_spoolss_ClosePrinter_r(b, tctx, &cp);
		torture_assert_ntstatus_ok(tctx, status, "ClosePrinter failed");
	}

	return true;
}



/* This replicates the opening sequence of OpenPrinterEx calls XP does */
static bool test_OpenPrinterSequence(struct torture_context *tctx,
					struct dcerpc_pipe *p,
					struct policy_handle *handle)
{
	bool ret;
	char *printername = talloc_asprintf(tctx, "\\\\%s",
			dcerpc_server_name(p));
	struct dcerpc_binding_handle *b = p->binding_handle;

	/* First, see if we can open the printer read_only */
	ret = test_OpenPrinterEx(tctx, b, handle, printername, 0);
	torture_assert(tctx, ret == true, "OpenPrinterEx failed.");

	ret = test_ClosePrinter(tctx, b, handle);
	torture_assert(tctx, ret, "ClosePrinter failed");

	/* Now let's see if we have admin rights to it. */
	ret = test_OpenPrinterAsAdmin(tctx, b, printername);
	torture_assert(tctx, ret == true,
			"OpenPrinterEx as admin failed unexpectedly.");

	ret = test_OpenPrinterEx(tctx, b, handle, printername, SERVER_EXECUTE);
	torture_assert(tctx, ret == true, "OpenPrinterEx failed.");

	return true;
}

static bool test_GetPrinterData(struct torture_context *tctx,
				struct dcerpc_binding_handle *b,
				struct policy_handle *handle,
				const char *value_name,
				WERROR expected_werr,
				uint32_t expected_value)
{
	NTSTATUS status;
	struct spoolss_GetPrinterData gpd;
	uint32_t needed;
	enum winreg_Type type;
	uint8_t *data = talloc_zero_array(tctx, uint8_t, 4);

	torture_comment(tctx, "Testing GetPrinterData(%s).\n", value_name);
	gpd.in.handle = handle;
	gpd.in.value_name = value_name;
	gpd.in.offered = 4;
	gpd.out.needed = &needed;
	gpd.out.type = &type;
	gpd.out.data = data;

	status = dcerpc_spoolss_GetPrinterData_r(b, tctx, &gpd);
	torture_assert_ntstatus_ok(tctx, status, "GetPrinterData failed.");
	torture_assert_werr_equal(tctx, gpd.out.result, expected_werr,
			"GetPrinterData did not return expected error value.");

	if (W_ERROR_IS_OK(expected_werr)) {
		uint32_t value = IVAL(data, 0);
		torture_assert_int_equal(tctx, value,
			expected_value,
			talloc_asprintf(tctx, "GetPrinterData for %s did not return expected value.", value_name));
	}
	return true;
}

static bool test_EnumPrinters(struct torture_context *tctx,
				struct dcerpc_pipe *p,
				struct test_spoolss_win_context *ctx,
				uint32_t initial_blob_size)
{
	NTSTATUS status;
	struct spoolss_EnumPrinters ep;
	DATA_BLOB blob = data_blob_talloc_zero(ctx, initial_blob_size);
	uint32_t needed;
	uint32_t count;
	union spoolss_PrinterInfo *info;
	struct dcerpc_binding_handle *b = p->binding_handle;

	ep.in.flags = PRINTER_ENUM_NAME;
	ep.in.server = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	ep.in.level = 2;
	ep.in.buffer = &blob;
	ep.in.offered = initial_blob_size;
	ep.out.needed = &needed;
	ep.out.count = &count;
	ep.out.info = &info;

	status = dcerpc_spoolss_EnumPrinters_r(b, ctx, &ep);
	torture_assert_ntstatus_ok(tctx, status, "EnumPrinters failed.");

	if (W_ERROR_EQUAL(ep.out.result, WERR_INSUFFICIENT_BUFFER)) {
		blob = data_blob_talloc_zero(ctx, needed);
		ep.in.buffer = &blob;
		ep.in.offered = needed;
		status = dcerpc_spoolss_EnumPrinters_r(b, ctx, &ep);
		torture_assert_ntstatus_ok(tctx, status,"EnumPrinters failed.");
	}

	torture_assert_werr_ok(tctx, ep.out.result, "EnumPrinters failed.");

	ctx->printer_count = count;
	ctx->printer_info = info;

	torture_comment(tctx, "Found %d printer(s).\n", ctx->printer_count);

	return true;
}

static bool test_GetPrinter(struct torture_context *tctx,
				struct dcerpc_binding_handle *b,
				struct policy_handle *handle,
				struct test_spoolss_win_context *ctx,
				uint32_t level,
				uint32_t initial_blob_size)
{
	NTSTATUS status;
	struct spoolss_GetPrinter gp;
	DATA_BLOB blob = data_blob_talloc_zero(ctx, initial_blob_size);
	uint32_t needed;

	torture_comment(tctx, "Test GetPrinter level %d\n", level);

	gp.in.handle = handle;
	gp.in.level = level;
	gp.in.buffer = (initial_blob_size == 0)?NULL:&blob;
	gp.in.offered = initial_blob_size;
	gp.out.needed = &needed;

	status = dcerpc_spoolss_GetPrinter_r(b, tctx, &gp);
	torture_assert_ntstatus_ok(tctx, status, "GetPrinter failed");

	if (W_ERROR_EQUAL(gp.out.result, WERR_INSUFFICIENT_BUFFER)) {
		blob = data_blob_talloc_zero(ctx, needed);
		gp.in.buffer = &blob;
		gp.in.offered = needed;
		status = dcerpc_spoolss_GetPrinter_r(b, tctx, &gp);
		torture_assert_ntstatus_ok(tctx, status, "GetPrinter failed");
	}

	torture_assert_werr_ok(tctx, gp.out.result, "GetPrinter failed");

	ctx->current_info = gp.out.info;

	if (level == 2 && gp.out.info) {
		ctx->printer_has_driver = gp.out.info->info2.drivername &&
					  strlen(gp.out.info->info2.drivername);
	}

	return true;
}

static bool test_EnumJobs(struct torture_context *tctx,
				struct dcerpc_binding_handle *b,
				struct policy_handle *handle)
{
	NTSTATUS status;
	struct spoolss_EnumJobs ej;
	DATA_BLOB blob = data_blob_talloc_zero(tctx, 1024);
	uint32_t needed;
	uint32_t count;
	union spoolss_JobInfo *info;

	torture_comment(tctx, "Test EnumJobs\n");

	ej.in.handle = handle;
	ej.in.level = 2;
	ej.in.buffer = &blob;
	ej.in.offered = 1024;
	ej.out.needed = &needed;
	ej.out.count = &count;
	ej.out.info = &info;

	status = dcerpc_spoolss_EnumJobs_r(b, tctx, &ej);
	torture_assert_ntstatus_ok(tctx, status, "EnumJobs failed");
	if (W_ERROR_EQUAL(ej.out.result, WERR_INSUFFICIENT_BUFFER)) {
		blob = data_blob_talloc_zero(tctx, needed);
		ej.in.offered = needed;
		ej.in.buffer = &blob;
		status = dcerpc_spoolss_EnumJobs_r(b, tctx, &ej);
		torture_assert_ntstatus_ok(tctx, status, "EnumJobs failed");
	}
	torture_assert_werr_ok(tctx, ej.out.result, "EnumJobs failed");

	return true;
}

static bool test_GetPrinterDriver2(struct torture_context *tctx,
					struct dcerpc_binding_handle *b,
					struct test_spoolss_win_context *ctx,
					struct policy_handle *handle)
{
	NTSTATUS status;
	struct spoolss_GetPrinterDriver2 gpd2;
	DATA_BLOB blob = data_blob_talloc_zero(tctx, 87424);
	uint32_t needed;
	uint32_t server_major_version;
	uint32_t server_minor_version;

	torture_comment(tctx, "Testing GetPrinterDriver2\n");

	gpd2.in.handle = handle;
	gpd2.in.architecture = "Windows NT x86";
	gpd2.in.level = 101;
	gpd2.in.buffer = &blob;
	gpd2.in.offered = 87424;
	gpd2.in.client_major_version = 3;
	gpd2.in.client_minor_version = 0;
	gpd2.out.needed = &needed;
	gpd2.out.server_major_version = &server_major_version;
	gpd2.out.server_minor_version = &server_minor_version;

	status = dcerpc_spoolss_GetPrinterDriver2_r(b, tctx, &gpd2);
	torture_assert_ntstatus_ok(tctx, status, "GetPrinterDriver2 failed");

	if (ctx->printer_has_driver) {
		torture_assert_werr_ok(tctx, gpd2.out.result,
				"GetPrinterDriver2 failed.");
	}

	return true;
}

static bool test_EnumForms(struct torture_context *tctx,
				struct dcerpc_binding_handle *b,
				struct policy_handle *handle,
				uint32_t initial_blob_size)
{
	NTSTATUS status;
	struct spoolss_EnumForms ef;
	DATA_BLOB blob = data_blob_talloc_zero(tctx, initial_blob_size);
	uint32_t needed;
	uint32_t count;
	union spoolss_FormInfo *info;

	torture_comment(tctx, "Testing EnumForms\n");

	ef.in.handle = handle;
	ef.in.level = 1;
	ef.in.buffer = (initial_blob_size == 0)?NULL:&blob;
	ef.in.offered = initial_blob_size;
	ef.out.needed = &needed;
	ef.out.count = &count;
	ef.out.info = &info;

	status = dcerpc_spoolss_EnumForms_r(b, tctx, &ef);
	torture_assert_ntstatus_ok(tctx, status, "EnumForms failed");

	if (W_ERROR_EQUAL(ef.out.result, WERR_INSUFFICIENT_BUFFER)) {
		blob = data_blob_talloc_zero(tctx, needed);
		ef.in.buffer = &blob;
		ef.in.offered = needed;
		status = dcerpc_spoolss_EnumForms_r(b, tctx, &ef);
		torture_assert_ntstatus_ok(tctx, status, "EnumForms failed");
	}

	torture_assert_werr_ok(tctx, ef.out.result, "EnumForms failed");

	return true;
}

static bool test_EnumPrinterKey(struct torture_context *tctx,
				struct dcerpc_binding_handle *b,
				struct policy_handle *handle,
				const char* key,
				struct test_spoolss_win_context *ctx)
{
	NTSTATUS status;
	struct spoolss_EnumPrinterKey epk;
	uint32_t needed = 0;
	union spoolss_KeyNames key_buffer;
	uint32_t _ndr_size;

	torture_comment(tctx, "Testing EnumPrinterKey(%s)\n", key);

	epk.in.handle = handle;
	epk.in.key_name = talloc_strdup(tctx, key);
	epk.in.offered = 0;
	epk.out.needed = &needed;
	epk.out.key_buffer = &key_buffer;
	epk.out._ndr_size = &_ndr_size;

	status = dcerpc_spoolss_EnumPrinterKey_r(b, tctx, &epk);
	torture_assert_ntstatus_ok(tctx, status, "EnumPrinterKey failed");

	if (W_ERROR_EQUAL(epk.out.result, WERR_MORE_DATA)) {
		epk.in.offered = needed;
		status = dcerpc_spoolss_EnumPrinterKey_r(b, tctx, &epk);
		torture_assert_ntstatus_ok(tctx, status,
				"EnumPrinterKey failed");
	}

	torture_assert_werr_ok(tctx, epk.out.result, "EnumPrinterKey failed");

	ctx->printer_keys = key_buffer.string_array;

	return true;
}

static bool test_EnumPrinterDataEx(struct torture_context *tctx,
					struct dcerpc_binding_handle *b,
					struct policy_handle *handle,
					const char *key,
					uint32_t initial_blob_size,
					WERROR expected_error)
{
	NTSTATUS status;
	struct spoolss_EnumPrinterDataEx epde;
	struct spoolss_PrinterEnumValues *info;
	uint32_t needed;
	uint32_t count;

	torture_comment(tctx, "Testing EnumPrinterDataEx(%s)\n", key);

	epde.in.handle = handle;
	epde.in.key_name = talloc_strdup(tctx, key);
	epde.in.offered = 0;
	epde.out.needed = &needed;
	epde.out.count = &count;
	epde.out.info = &info;

	status = dcerpc_spoolss_EnumPrinterDataEx_r(b, tctx, &epde);
	torture_assert_ntstatus_ok(tctx, status, "EnumPrinterDataEx failed.");
	if (W_ERROR_EQUAL(epde.out.result, WERR_MORE_DATA)) {
		epde.in.offered = needed;
		status = dcerpc_spoolss_EnumPrinterDataEx_r(b, tctx, &epde);
		torture_assert_ntstatus_ok(tctx, status,
				"EnumPrinterDataEx failed.");
	}

	torture_assert_werr_equal(tctx, epde.out.result, expected_error,
			"EnumPrinterDataEx failed.");

	return true;
}

static bool test_WinXP(struct torture_context *tctx, struct dcerpc_pipe *p)
{
	bool ret = true;
	struct test_spoolss_win_context *ctx, *tmp_ctx;
	struct policy_handle handle01, handle02, handle03, handle04;
	/* Sometimes a handle stays unused. In order to make this clear in the
	 * code, the unused_handle structures are used for that. */
	struct policy_handle unused_handle1, unused_handle2;
	char *server_name;
	uint32_t i;
	struct dcerpc_binding_handle *b = p->binding_handle;

	ntvfs_init(tctx->lp_ctx);

	ctx = talloc_zero(tctx, struct test_spoolss_win_context);
	tmp_ctx = talloc_zero(tctx, struct test_spoolss_win_context);

	ret &= test_OpenPrinterSequence(tctx, p, &handle01);
	ret &= test_GetPrinterData(tctx, b, &handle01,"UISingleJobStatusString",
			WERR_INVALID_PARAM, 0);
	torture_comment(tctx, "Skip RemoteFindNextPrinterChangeNotifyEx test\n");

	server_name = talloc_asprintf(ctx, "\\\\%s", dcerpc_server_name(p));
	ret &= test_OpenPrinterEx(tctx, b, &unused_handle1, server_name, 0);

	ret &= test_EnumPrinters(tctx, p, ctx, 1024);

	ret &= test_OpenPrinterEx(tctx, b, &handle02, server_name, 0);
	ret &= test_GetPrinterData(tctx, b, &handle02, "MajorVersion", WERR_OK,
			3);
	ret &= test_ClosePrinter(tctx, b, &handle02);

	/* If no printers were found, skip all tests that need a printer */
	if (ctx->printer_count == 0) {
		goto end_testWinXP;
	}

	ret &= test_OpenPrinterEx(tctx, b, &handle02,
			ctx->printer_info[0].info2.printername,
			PRINTER_ACCESS_USE);
	ret &= test_GetPrinter(tctx, b, &handle02, ctx, 2, 0);

	torture_assert_str_equal(tctx, ctx->current_info->info2.printername,
			ctx->printer_info[0].info2.printername,
			"GetPrinter returned unexpected printername");
	/*FIXME: Test more components of the PrinterInfo2 struct */

	ret &= test_OpenPrinterEx(tctx, b, &handle03,
			ctx->printer_info[0].info2.printername, 0);
	ret &= test_GetPrinter(tctx, b, &handle03, ctx, 0, 1164);
	ret &= test_GetPrinter(tctx, b, &handle03, ctx, 2, 0);

	ret &= test_OpenPrinterEx(tctx, b, &handle04,
			ctx->printer_info[0].info2.printername, 0);
	ret &= test_GetPrinter(tctx, b, &handle04, ctx, 2, 0);
	ret &= test_ClosePrinter(tctx, b, &handle04);

	ret &= test_OpenPrinterEx(tctx, b, &handle04,
			ctx->printer_info[0].info2.printername, 0);
	ret &= test_GetPrinter(tctx, b, &handle04, ctx, 2, 4096);
	ret &= test_ClosePrinter(tctx, b, &handle04);

	ret &= test_OpenPrinterAsAdmin(tctx, b,
			ctx->printer_info[0].info2.printername);

	ret &= test_OpenPrinterEx(tctx, b, &handle04,
			ctx->printer_info[0].info2.printername, PRINTER_READ);
	ret &= test_GetPrinterData(tctx, b, &handle04,"UISingleJobStatusString",
			WERR_BADFILE, 0);
	torture_comment(tctx, "Skip RemoteFindNextPrinterChangeNotifyEx test\n");

	ret &= test_OpenPrinterEx(tctx, b, &unused_handle2,
			ctx->printer_info[0].info2.printername, 0);

	ret &= test_EnumJobs(tctx, b, &handle04);
	ret &= test_GetPrinter(tctx, b, &handle04, ctx, 2, 4096);

	ret &= test_ClosePrinter(tctx, b, &unused_handle2);
	ret &= test_ClosePrinter(tctx, b, &handle04);

	ret &= test_EnumPrinters(tctx, p, ctx, 1556);
	ret &= test_GetPrinterDriver2(tctx, b, ctx, &handle03);
	ret &= test_EnumForms(tctx, b, &handle03, 0);

	ret &= test_EnumPrinterKey(tctx, b, &handle03, "", ctx);

	for (i=0; ctx->printer_keys && ctx->printer_keys[i] != NULL; i++) {

		ret &= test_EnumPrinterKey(tctx, b, &handle03,
					   ctx->printer_keys[i],
					   tmp_ctx);
		ret &= test_EnumPrinterDataEx(tctx, b, &handle03,
					      ctx->printer_keys[i], 0,
					      WERR_OK);
	}

	ret &= test_EnumPrinterDataEx(tctx, b, &handle03, "", 0,
			WERR_INVALID_PARAM);

	ret &= test_GetPrinter(tctx, b, &handle03, tmp_ctx, 2, 0);

	ret &= test_OpenPrinterEx(tctx, b, &unused_handle2,
			ctx->printer_info[0].info2.printername, 0);
	ret &= test_ClosePrinter(tctx, b, &unused_handle2);

	ret &= test_GetPrinter(tctx, b, &handle03, tmp_ctx, 2, 2556);

	ret &= test_OpenPrinterEx(tctx, b, &unused_handle2,
			ctx->printer_info[0].info2.printername, 0);
	ret &= test_ClosePrinter(tctx, b, &unused_handle2);

	ret &= test_OpenPrinterEx(tctx, b, &unused_handle2,
			ctx->printer_info[0].info2.printername, 0);
	ret &= test_ClosePrinter(tctx, b, &unused_handle2);

	ret &= test_GetPrinter(tctx, b, &handle03, tmp_ctx, 7, 0);

	ret &= test_OpenPrinterEx(tctx, b, &unused_handle2,
			ctx->printer_info[0].info2.printername, 0);
	ret &= test_ClosePrinter(tctx, b, &unused_handle2);

	ret &= test_ClosePrinter(tctx, b, &handle03);

	ret &= test_OpenPrinterEx(tctx, b, &unused_handle2,
			ctx->printer_info[0].info2.printername, 0);
	ret &= test_ClosePrinter(tctx, b, &unused_handle2);

	ret &= test_OpenPrinterEx(tctx, b, &handle03, server_name, 0);
	ret &= test_GetPrinterData(tctx, b, &handle03, "W3SvcInstalled",
			WERR_OK, 0);
	ret &= test_ClosePrinter(tctx, b, &handle03);

	ret &= test_ClosePrinter(tctx, b, &unused_handle1);
	ret &= test_ClosePrinter(tctx, b, &handle02);

	ret &= test_OpenPrinterEx(tctx, b, &handle02,
			ctx->printer_info[0].info2.sharename, 0);
	ret &= test_GetPrinter(tctx, b, &handle02, tmp_ctx, 2, 0);
	ret &= test_ClosePrinter(tctx, b, &handle02);

end_testWinXP:
	ret &= test_ClosePrinter(tctx, b, &handle01);

	talloc_free(tmp_ctx);
	talloc_free(ctx);
	return ret;
}

struct torture_suite *torture_rpc_spoolss_win(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "spoolss.win");

	struct torture_rpc_tcase *tcase = torture_suite_add_rpc_iface_tcase(suite, 
							"win", &ndr_table_spoolss);

	torture_rpc_tcase_add_test(tcase, "testWinXP", test_WinXP);

	return suite;
}

