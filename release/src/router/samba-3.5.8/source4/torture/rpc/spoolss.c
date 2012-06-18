/*
   Unix SMB/CIFS implementation.
   test suite for spoolss rpc operations

   Copyright (C) Tim Potter 2003
   Copyright (C) Stefan Metzmacher 2005
   Copyright (C) Jelmer Vernooij 2007
   Copyright (C) Guenther Deschner 2009-2010

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
#include "torture/torture.h"
#include "torture/rpc/rpc.h"
#include "librpc/gen_ndr/ndr_misc.h"
#include "librpc/gen_ndr/ndr_spoolss.h"
#include "librpc/gen_ndr/ndr_spoolss_c.h"
#include "param/param.h"
#include "lib/registry/registry.h"

#define TORTURE_WELLKNOWN_PRINTER	"torture_wkn_printer"
#define TORTURE_PRINTER			"torture_printer"
#define TORTURE_WELLKNOWN_PRINTER_EX	"torture_wkn_printer_ex"
#define TORTURE_PRINTER_EX		"torture_printer_ex"

struct test_spoolss_context {
	/* print server handle */
	struct policy_handle server_handle;

	/* for EnumPorts */
	uint32_t port_count[3];
	union spoolss_PortInfo *ports[3];

	/* for EnumPrinterDrivers */
	uint32_t driver_count[8];
	union spoolss_DriverInfo *drivers[8];

	/* for EnumMonitors */
	uint32_t monitor_count[3];
	union spoolss_MonitorInfo *monitors[3];

	/* for EnumPrintProcessors */
	uint32_t print_processor_count[2];
	union spoolss_PrintProcessorInfo *print_processors[2];

	/* for EnumPrinters */
	uint32_t printer_count[6];
	union spoolss_PrinterInfo *printers[6];
};

#define COMPARE_STRING(tctx, c,r,e) \
	torture_assert_str_equal(tctx, c.e, r.e, "invalid value")

/* not every compiler supports __typeof__() */
#if (__GNUC__ >= 3)
#define _CHECK_FIELD_SIZE(c,r,e,type) do {\
	if (sizeof(__typeof__(c.e)) != sizeof(type)) { \
		torture_fail(tctx, #c "." #e "field is not " #type "\n"); \
	}\
	if (sizeof(__typeof__(r.e)) != sizeof(type)) { \
		torture_fail(tctx, #r "." #e "field is not " #type "\n"); \
	}\
} while(0)
#else
#define _CHECK_FIELD_SIZE(c,r,e,type) do {} while(0)
#endif

#define COMPARE_UINT32(tctx, c, r, e) do {\
	_CHECK_FIELD_SIZE(c, r, e, uint32_t); \
	torture_assert_int_equal(tctx, c.e, r.e, "invalid value"); \
} while(0)

#define COMPARE_UINT64(tctx, c, r, e) do {\
	_CHECK_FIELD_SIZE(c, r, e, uint64_t); \
	torture_assert_int_equal(tctx, c.e, r.e, "invalid value"); \
} while(0)


#define COMPARE_NTTIME(tctx, c, r, e) do {\
	_CHECK_FIELD_SIZE(c, r, e, NTTIME); \
	torture_assert_int_equal(tctx, c.e, r.e, "invalid value"); \
} while(0)

#define COMPARE_STRING_ARRAY(tctx, c,r,e) do {\
	int __i; \
	if (!c.e && !r.e) { \
		break; \
	} \
	if (c.e && !r.e) { \
		torture_fail(tctx, #r "." #e " field is NULL and " #c "." #e " is not\n"); \
	} \
	if (!c.e && r.e) { \
		torture_fail(tctx, #c "." #e " field is NULL and " #r "." #e " is not\n"); \
	} \
	for (__i=0;c.e[__i] != NULL; __i++) { \
		torture_assert_str_equal(tctx, c.e[__i], r.e[__i], "invalid value"); \
	} \
} while(0)

#define CHECK_ALIGN(size, n) do {\
	if (size % n) {\
		torture_warning(tctx, "%d is *NOT* %d byte aligned, should be %d",\
			size, n, size + n - (size % n));\
	}\
} while(0)

#define DO_ROUND(size, n) (((size)+((n)-1)) & ~((n)-1))

#define CHECK_NEEDED_SIZE_ENUM_LEVEL(fn, info, level, count, ic, needed, align) do { \
	if (torture_setting_bool(tctx, "spoolss_check_size", false)) {\
	uint32_t size = ndr_size_##fn##_info(tctx, ic, level, count, info);\
	uint32_t round_size = DO_ROUND(size, align);\
	if (round_size != needed) {\
		torture_warning(tctx, __location__": "#fn" level %d (count: %d) got unexpected needed size: %d, we calculated: %d", level, count, needed, round_size);\
		CHECK_ALIGN(size, align);\
	}\
	}\
} while(0)

#define CHECK_NEEDED_SIZE_ENUM(fn, info, count, ic, needed, align) do { \
	if (torture_setting_bool(tctx, "spoolss_check_size", false)) {\
	uint32_t size = ndr_size_##fn##_info(tctx, ic, count, info);\
	uint32_t round_size = DO_ROUND(size, align);\
	if (round_size != needed) {\
		torture_warning(tctx, __location__": "#fn" (count: %d) got unexpected needed size: %d, we calculated: %d", count, needed, round_size);\
		CHECK_ALIGN(size, align);\
	}\
	}\
} while(0)

#define CHECK_NEEDED_SIZE_LEVEL(fn, info, level, ic, needed, align) do { \
	if (torture_setting_bool(tctx, "spoolss_check_size", false)) {\
	uint32_t size = ndr_size_##fn(info, level, ic, 0);\
	uint32_t round_size = DO_ROUND(size, align);\
	if (round_size != needed) {\
		torture_warning(tctx, __location__": "#fn" level %d got unexpected needed size: %d, we calculated: %d", level, needed, round_size);\
		CHECK_ALIGN(size, align);\
	}\
	}\
} while(0)

static bool test_OpenPrinter_server(struct torture_context *tctx,
				    struct dcerpc_pipe *p,
				    struct policy_handle *server_handle)
{
	NTSTATUS status;
	struct spoolss_OpenPrinter op;

	op.in.printername	= talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	op.in.datatype		= NULL;
	op.in.devmode_ctr.devmode= NULL;
	op.in.access_mask	= 0;
	op.out.handle		= server_handle;

	torture_comment(tctx, "Testing OpenPrinter(%s)\n", op.in.printername);

	status = dcerpc_spoolss_OpenPrinter(p, tctx, &op);
	torture_assert_ntstatus_ok(tctx, status, "dcerpc_spoolss_OpenPrinter failed");
	torture_assert_werr_ok(tctx, op.out.result, "dcerpc_spoolss_OpenPrinter failed");

	return true;
}

static bool test_EnumPorts(struct torture_context *tctx,
			   struct dcerpc_pipe *p,
			   struct test_spoolss_context *ctx)
{
	NTSTATUS status;
	struct spoolss_EnumPorts r;
	uint16_t levels[] = { 1, 2 };
	int i, j;

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		int level = levels[i];
		DATA_BLOB blob;
		uint32_t needed;
		uint32_t count;
		union spoolss_PortInfo *info;

		r.in.servername = "";
		r.in.level = level;
		r.in.buffer = NULL;
		r.in.offered = 0;
		r.out.needed = &needed;
		r.out.count = &count;
		r.out.info = &info;

		torture_comment(tctx, "Testing EnumPorts level %u\n", r.in.level);

		status = dcerpc_spoolss_EnumPorts(p, ctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "dcerpc_spoolss_EnumPorts failed");
		if (W_ERROR_IS_OK(r.out.result)) {
			/* TODO: do some more checks here */
			continue;
		}
		torture_assert_werr_equal(tctx, r.out.result, WERR_INSUFFICIENT_BUFFER,
			"EnumPorts unexpected return code");

		blob = data_blob_talloc(ctx, NULL, needed);
		data_blob_clear(&blob);
		r.in.buffer = &blob;
		r.in.offered = needed;

		status = dcerpc_spoolss_EnumPorts(p, ctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "dcerpc_spoolss_EnumPorts failed");

		torture_assert_werr_ok(tctx, r.out.result, "EnumPorts failed");

		torture_assert(tctx, info, "EnumPorts returned no info");

		CHECK_NEEDED_SIZE_ENUM_LEVEL(spoolss_EnumPorts, info, r.in.level, count, lp_iconv_convenience(tctx->lp_ctx), needed, 4);

		ctx->port_count[level]	= count;
		ctx->ports[level]	= info;
	}

	for (i=1;i<ARRAY_SIZE(levels);i++) {
		int level = levels[i];
		int old_level = levels[i-1];
		torture_assert_int_equal(tctx, ctx->port_count[level], ctx->port_count[old_level],
			"EnumPorts invalid value");
	}
	/* if the array sizes are not the same we would maybe segfault in the following code */

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		int level = levels[i];
		for (j=0;j<ctx->port_count[level];j++) {
			union spoolss_PortInfo *cur = &ctx->ports[level][j];
			union spoolss_PortInfo *ref = &ctx->ports[2][j];
			switch (level) {
			case 1:
				COMPARE_STRING(tctx, cur->info1, ref->info2, port_name);
				break;
			case 2:
				/* level 2 is our reference, and it makes no sense to compare it to itself */
				break;
			}
		}
	}

	return true;
}

static bool test_GetPrintProcessorDirectory(struct torture_context *tctx,
					    struct dcerpc_pipe *p,
					    struct test_spoolss_context *ctx)
{
	NTSTATUS status;
	struct spoolss_GetPrintProcessorDirectory r;
	struct {
		uint16_t level;
		const char *server;
	} levels[] = {{
			.level	= 1,
			.server	= NULL
		},{
			.level	= 1,
			.server	= ""
		},{
			.level	= 78,
			.server	= ""
		},{
			.level	= 1,
			.server	= talloc_asprintf(ctx, "\\\\%s", dcerpc_server_name(p))
		},{
			.level	= 1024,
			.server	= talloc_asprintf(ctx, "\\\\%s", dcerpc_server_name(p))
		}
	};
	int i;
	uint32_t needed;

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		int level = levels[i].level;
		DATA_BLOB blob;

		r.in.server		= levels[i].server;
		r.in.environment	= SPOOLSS_ARCHITECTURE_NT_X86;
		r.in.level		= level;
		r.in.buffer		= NULL;
		r.in.offered		= 0;
		r.out.needed		= &needed;

		torture_comment(tctx, "Testing GetPrintProcessorDirectory level %u\n", r.in.level);

		status = dcerpc_spoolss_GetPrintProcessorDirectory(p, ctx, &r);
		torture_assert_ntstatus_ok(tctx, status,
			"dcerpc_spoolss_GetPrintProcessorDirectory failed");
		torture_assert_werr_equal(tctx, r.out.result, WERR_INSUFFICIENT_BUFFER,
			"GetPrintProcessorDirectory unexpected return code");

		blob = data_blob_talloc(ctx, NULL, needed);
		data_blob_clear(&blob);
		r.in.buffer = &blob;
		r.in.offered = needed;

		status = dcerpc_spoolss_GetPrintProcessorDirectory(p, ctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "dcerpc_spoolss_GetPrintProcessorDirectory failed");

		torture_assert_werr_ok(tctx, r.out.result, "GetPrintProcessorDirectory failed");

		CHECK_NEEDED_SIZE_LEVEL(spoolss_PrintProcessorDirectoryInfo, r.out.info, r.in.level, lp_iconv_convenience(tctx->lp_ctx), needed, 2);
	}

	return true;
}


static bool test_GetPrinterDriverDirectory(struct torture_context *tctx,
					   struct dcerpc_pipe *p,
					   struct test_spoolss_context *ctx)
{
	NTSTATUS status;
	struct spoolss_GetPrinterDriverDirectory r;
	struct {
		uint16_t level;
		const char *server;
	} levels[] = {{
			.level	= 1,
			.server	= NULL
		},{
			.level	= 1,
			.server	= ""
		},{
			.level	= 78,
			.server	= ""
		},{
			.level	= 1,
			.server	= talloc_asprintf(ctx, "\\\\%s", dcerpc_server_name(p))
		},{
			.level	= 1024,
			.server	= talloc_asprintf(ctx, "\\\\%s", dcerpc_server_name(p))
		}
	};
	int i;
	uint32_t needed;

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		int level = levels[i].level;
		DATA_BLOB blob;

		r.in.server		= levels[i].server;
		r.in.environment	= SPOOLSS_ARCHITECTURE_NT_X86;
		r.in.level		= level;
		r.in.buffer		= NULL;
		r.in.offered		= 0;
		r.out.needed		= &needed;

		torture_comment(tctx, "Testing GetPrinterDriverDirectory level %u\n", r.in.level);

		status = dcerpc_spoolss_GetPrinterDriverDirectory(p, ctx, &r);
		torture_assert_ntstatus_ok(tctx, status,
			"dcerpc_spoolss_GetPrinterDriverDirectory failed");
		torture_assert_werr_equal(tctx, r.out.result, WERR_INSUFFICIENT_BUFFER,
			"GetPrinterDriverDirectory unexpected return code");

		blob = data_blob_talloc(ctx, NULL, needed);
		data_blob_clear(&blob);
		r.in.buffer = &blob;
		r.in.offered = needed;

		status = dcerpc_spoolss_GetPrinterDriverDirectory(p, ctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "dcerpc_spoolss_GetPrinterDriverDirectory failed");

		torture_assert_werr_ok(tctx, r.out.result, "GetPrinterDriverDirectory failed");

		CHECK_NEEDED_SIZE_LEVEL(spoolss_DriverDirectoryInfo, r.out.info, r.in.level, lp_iconv_convenience(tctx->lp_ctx), needed, 2);
	}

	return true;
}

static bool test_EnumPrinterDrivers(struct torture_context *tctx,
				    struct dcerpc_pipe *p,
				    struct test_spoolss_context *ctx,
				    const char *architecture)
{
	NTSTATUS status;
	struct spoolss_EnumPrinterDrivers r;
	uint16_t levels[] = { 1, 2, 3, 4, 5, 6, 8 };
	int i, j;

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		int level = levels[i];
		DATA_BLOB blob;
		uint32_t needed;
		uint32_t count;
		union spoolss_DriverInfo *info;

		/* FIXME: gd, come back and fix "" as server, and handle
		 * priority of returned error codes in torture test and samba 3
		 * server */

		r.in.server		= talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
		r.in.environment	= architecture;
		r.in.level		= level;
		r.in.buffer		= NULL;
		r.in.offered		= 0;
		r.out.needed		= &needed;
		r.out.count		= &count;
		r.out.info		= &info;

		torture_comment(tctx, "Testing EnumPrinterDrivers level %u (%s)\n", r.in.level, r.in.environment);

		status = dcerpc_spoolss_EnumPrinterDrivers(p, ctx, &r);
		torture_assert_ntstatus_ok(tctx, status,
					   "dcerpc_spoolss_EnumPrinterDrivers failed");
		if (W_ERROR_IS_OK(r.out.result)) {
			/* TODO: do some more checks here */
			continue;
		}
		if (W_ERROR_EQUAL(r.out.result, WERR_INSUFFICIENT_BUFFER)) {
			blob = data_blob_talloc(ctx, NULL, needed);
			data_blob_clear(&blob);
			r.in.buffer = &blob;
			r.in.offered = needed;

			status = dcerpc_spoolss_EnumPrinterDrivers(p, ctx, &r);
			torture_assert_ntstatus_ok(tctx, status, "dcerpc_spoolss_EnumPrinterDrivers failed");
		}

		torture_assert_werr_ok(tctx, r.out.result, "EnumPrinterDrivers failed");

		CHECK_NEEDED_SIZE_ENUM_LEVEL(spoolss_EnumPrinterDrivers, info, r.in.level, count, lp_iconv_convenience(tctx->lp_ctx), needed, 4);

		ctx->driver_count[level]	= count;
		ctx->drivers[level]		= info;
	}

	for (i=1;i<ARRAY_SIZE(levels);i++) {
		int level = levels[i];
		int old_level = levels[i-1];

		torture_assert_int_equal(tctx, ctx->driver_count[level], ctx->driver_count[old_level],
			"EnumPrinterDrivers invalid value");
	}

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		int level = levels[i];

		for (j=0;j<ctx->driver_count[level];j++) {
			union spoolss_DriverInfo *cur = &ctx->drivers[level][j];
			union spoolss_DriverInfo *ref = &ctx->drivers[8][j];

			switch (level) {
			case 1:
				COMPARE_STRING(tctx, cur->info1, ref->info8, driver_name);
				break;
			case 2:
				COMPARE_UINT32(tctx, cur->info2, ref->info8, version);
				COMPARE_STRING(tctx, cur->info2, ref->info8, driver_name);
				COMPARE_STRING(tctx, cur->info2, ref->info8, architecture);
				COMPARE_STRING(tctx, cur->info2, ref->info8, driver_path);
				COMPARE_STRING(tctx, cur->info2, ref->info8, data_file);
				COMPARE_STRING(tctx, cur->info2, ref->info8, config_file);
				break;
			case 3:
				COMPARE_UINT32(tctx, cur->info3, ref->info8, version);
				COMPARE_STRING(tctx, cur->info3, ref->info8, driver_name);
				COMPARE_STRING(tctx, cur->info3, ref->info8, architecture);
				COMPARE_STRING(tctx, cur->info3, ref->info8, driver_path);
				COMPARE_STRING(tctx, cur->info3, ref->info8, data_file);
				COMPARE_STRING(tctx, cur->info3, ref->info8, config_file);
				COMPARE_STRING(tctx, cur->info3, ref->info8, help_file);
				COMPARE_STRING_ARRAY(tctx, cur->info3, ref->info8, dependent_files);
				COMPARE_STRING(tctx, cur->info3, ref->info8, monitor_name);
				COMPARE_STRING(tctx, cur->info3, ref->info8, default_datatype);
				break;
			case 4:
				COMPARE_UINT32(tctx, cur->info4, ref->info8, version);
				COMPARE_STRING(tctx, cur->info4, ref->info8, driver_name);
				COMPARE_STRING(tctx, cur->info4, ref->info8, architecture);
				COMPARE_STRING(tctx, cur->info4, ref->info8, driver_path);
				COMPARE_STRING(tctx, cur->info4, ref->info8, data_file);
				COMPARE_STRING(tctx, cur->info4, ref->info8, config_file);
				COMPARE_STRING(tctx, cur->info4, ref->info8, help_file);
				COMPARE_STRING_ARRAY(tctx, cur->info4, ref->info8, dependent_files);
				COMPARE_STRING(tctx, cur->info4, ref->info8, monitor_name);
				COMPARE_STRING(tctx, cur->info4, ref->info8, default_datatype);
				COMPARE_STRING_ARRAY(tctx, cur->info4, ref->info8, previous_names);
				break;
			case 5:
				COMPARE_UINT32(tctx, cur->info5, ref->info8, version);
				COMPARE_STRING(tctx, cur->info5, ref->info8, driver_name);
				COMPARE_STRING(tctx, cur->info5, ref->info8, architecture);
				COMPARE_STRING(tctx, cur->info5, ref->info8, driver_path);
				COMPARE_STRING(tctx, cur->info5, ref->info8, data_file);
				COMPARE_STRING(tctx, cur->info5, ref->info8, config_file);
				/*COMPARE_UINT32(tctx, cur->info5, ref->info8, driver_attributes);*/
				/*COMPARE_UINT32(tctx, cur->info5, ref->info8, config_version);*/
				/*TODO: ! COMPARE_UINT32(tctx, cur->info5, ref->info8, driver_version); */
				break;
			case 6:
				COMPARE_UINT32(tctx, cur->info6, ref->info8, version);
				COMPARE_STRING(tctx, cur->info6, ref->info8, driver_name);
				COMPARE_STRING(tctx, cur->info6, ref->info8, architecture);
				COMPARE_STRING(tctx, cur->info6, ref->info8, driver_path);
				COMPARE_STRING(tctx, cur->info6, ref->info8, data_file);
				COMPARE_STRING(tctx, cur->info6, ref->info8, config_file);
				COMPARE_STRING(tctx, cur->info6, ref->info8, help_file);
				COMPARE_STRING_ARRAY(tctx, cur->info6, ref->info8, dependent_files);
				COMPARE_STRING(tctx, cur->info6, ref->info8, monitor_name);
				COMPARE_STRING(tctx, cur->info6, ref->info8, default_datatype);
				COMPARE_STRING_ARRAY(tctx, cur->info6, ref->info8, previous_names);
				COMPARE_NTTIME(tctx, cur->info6, ref->info8, driver_date);
				COMPARE_UINT64(tctx, cur->info6, ref->info8, driver_version);
				COMPARE_STRING(tctx, cur->info6, ref->info8, manufacturer_name);
				COMPARE_STRING(tctx, cur->info6, ref->info8, manufacturer_url);
				COMPARE_STRING(tctx, cur->info6, ref->info8, hardware_id);
				COMPARE_STRING(tctx, cur->info6, ref->info8, provider);
				break;
			case 8:
				/* level 8 is our reference, and it makes no sense to compare it to itself */
				break;
			}
		}
	}

	return true;
}

static bool test_EnumMonitors(struct torture_context *tctx,
			      struct dcerpc_pipe *p,
			      struct test_spoolss_context *ctx)
{
	NTSTATUS status;
	struct spoolss_EnumMonitors r;
	uint16_t levels[] = { 1, 2 };
	int i, j;

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		int level = levels[i];
		DATA_BLOB blob;
		uint32_t needed;
		uint32_t count;
		union spoolss_MonitorInfo *info;

		r.in.servername = "";
		r.in.level = level;
		r.in.buffer = NULL;
		r.in.offered = 0;
		r.out.needed = &needed;
		r.out.count = &count;
		r.out.info = &info;

		torture_comment(tctx, "Testing EnumMonitors level %u\n", r.in.level);

		status = dcerpc_spoolss_EnumMonitors(p, ctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "dcerpc_spoolss_EnumMonitors failed");
		if (W_ERROR_IS_OK(r.out.result)) {
			/* TODO: do some more checks here */
			continue;
		}
		torture_assert_werr_equal(tctx, r.out.result, WERR_INSUFFICIENT_BUFFER,
			"EnumMonitors failed");

		blob = data_blob_talloc(ctx, NULL, needed);
		data_blob_clear(&blob);
		r.in.buffer = &blob;
		r.in.offered = needed;

		status = dcerpc_spoolss_EnumMonitors(p, ctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "dcerpc_spoolss_EnumMonitors failed");

		torture_assert_werr_ok(tctx, r.out.result, "EnumMonitors failed");

		CHECK_NEEDED_SIZE_ENUM_LEVEL(spoolss_EnumMonitors, info, r.in.level, count, lp_iconv_convenience(tctx->lp_ctx), needed, 4);

		ctx->monitor_count[level]	= count;
		ctx->monitors[level]		= info;
	}

	for (i=1;i<ARRAY_SIZE(levels);i++) {
		int level = levels[i];
		int old_level = levels[i-1];
		torture_assert_int_equal(tctx, ctx->monitor_count[level], ctx->monitor_count[old_level],
					 "EnumMonitors invalid value");
	}

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		int level = levels[i];
		for (j=0;j<ctx->monitor_count[level];j++) {
			union spoolss_MonitorInfo *cur = &ctx->monitors[level][j];
			union spoolss_MonitorInfo *ref = &ctx->monitors[2][j];
			switch (level) {
			case 1:
				COMPARE_STRING(tctx, cur->info1, ref->info2, monitor_name);
				break;
			case 2:
				/* level 2 is our reference, and it makes no sense to compare it to itself */
				break;
			}
		}
	}

	return true;
}

static bool test_EnumPrintProcessors(struct torture_context *tctx,
				     struct dcerpc_pipe *p,
				     struct test_spoolss_context *ctx)
{
	NTSTATUS status;
	struct spoolss_EnumPrintProcessors r;
	uint16_t levels[] = { 1 };
	int i, j;

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		int level = levels[i];
		DATA_BLOB blob;
		uint32_t needed;
		uint32_t count;
		union spoolss_PrintProcessorInfo *info;

		r.in.servername = "";
		r.in.environment = SPOOLSS_ARCHITECTURE_NT_X86;
		r.in.level = level;
		r.in.buffer = NULL;
		r.in.offered = 0;
		r.out.needed = &needed;
		r.out.count = &count;
		r.out.info = &info;

		torture_comment(tctx, "Testing EnumPrintProcessors level %u\n", r.in.level);

		status = dcerpc_spoolss_EnumPrintProcessors(p, ctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "dcerpc_spoolss_EnumPrintProcessors failed");
		if (W_ERROR_IS_OK(r.out.result)) {
			/* TODO: do some more checks here */
			continue;
		}
		torture_assert_werr_equal(tctx, r.out.result, WERR_INSUFFICIENT_BUFFER,
			"EnumPrintProcessors unexpected return code");

		blob = data_blob_talloc(ctx, NULL, needed);
		data_blob_clear(&blob);
		r.in.buffer = &blob;
		r.in.offered = needed;

		status = dcerpc_spoolss_EnumPrintProcessors(p, ctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "dcerpc_spoolss_EnumPrintProcessors failed");

		torture_assert_werr_ok(tctx, r.out.result, "EnumPrintProcessors failed");

		CHECK_NEEDED_SIZE_ENUM_LEVEL(spoolss_EnumPrintProcessors, info, r.in.level, count, lp_iconv_convenience(tctx->lp_ctx), needed, 4);

		ctx->print_processor_count[level]	= count;
		ctx->print_processors[level]		= info;
	}

	for (i=1;i<ARRAY_SIZE(levels);i++) {
		int level = levels[i];
		int old_level = levels[i-1];
		torture_assert_int_equal(tctx, ctx->print_processor_count[level], ctx->print_processor_count[old_level],
			"EnumPrintProcessors failed");
	}

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		int level = levels[i];
		for (j=0;j<ctx->print_processor_count[level];j++) {
#if 0
			union spoolss_PrintProcessorInfo *cur = &ctx->print_processors[level][j];
			union spoolss_PrintProcessorInfo *ref = &ctx->print_processors[1][j];
#endif
			switch (level) {
			case 1:
				/* level 1 is our reference, and it makes no sense to compare it to itself */
				break;
			}
		}
	}

	return true;
}

static bool test_EnumPrintProcDataTypes(struct torture_context *tctx,
					struct dcerpc_pipe *p,
					struct test_spoolss_context *ctx)
{
	NTSTATUS status;
	struct spoolss_EnumPrintProcDataTypes r;
	uint16_t levels[] = { 1 };
	int i;

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		int level = levels[i];
		DATA_BLOB blob;
		uint32_t needed;
		uint32_t count;
		union spoolss_PrintProcDataTypesInfo *info;

		r.in.servername = "";
		r.in.print_processor_name = "winprint";
		r.in.level = level;
		r.in.buffer = NULL;
		r.in.offered = 0;
		r.out.needed = &needed;
		r.out.count = &count;
		r.out.info = &info;

		torture_comment(tctx, "Testing EnumPrintProcDataTypes level %u\n", r.in.level);

		status = dcerpc_spoolss_EnumPrintProcDataTypes(p, ctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "dcerpc_spoolss_EnumPrintProcDataType failed");
		if (W_ERROR_IS_OK(r.out.result)) {
			/* TODO: do some more checks here */
			continue;
		}
		torture_assert_werr_equal(tctx, r.out.result, WERR_INSUFFICIENT_BUFFER,
			"EnumPrintProcDataTypes unexpected return code");

		blob = data_blob_talloc(ctx, NULL, needed);
		data_blob_clear(&blob);
		r.in.buffer = &blob;
		r.in.offered = needed;

		status = dcerpc_spoolss_EnumPrintProcDataTypes(p, ctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "dcerpc_spoolss_EnumPrintProcDataTypes failed");

		torture_assert_werr_ok(tctx, r.out.result, "EnumPrintProcDataTypes failed");

		CHECK_NEEDED_SIZE_ENUM_LEVEL(spoolss_EnumPrintProcDataTypes, info, r.in.level, count, lp_iconv_convenience(tctx->lp_ctx), needed, 4);

	}

	return true;
}


static bool test_EnumPrinters(struct torture_context *tctx,
			      struct dcerpc_pipe *p,
			      struct test_spoolss_context *ctx)
{
	struct spoolss_EnumPrinters r;
	NTSTATUS status;
	uint16_t levels[] = { 0, 1, 2, 4, 5 };
	int i, j;

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		int level = levels[i];
		DATA_BLOB blob;
		uint32_t needed;
		uint32_t count;
		union spoolss_PrinterInfo *info;

		r.in.flags	= PRINTER_ENUM_LOCAL;
		r.in.server	= "";
		r.in.level	= level;
		r.in.buffer	= NULL;
		r.in.offered	= 0;
		r.out.needed	= &needed;
		r.out.count	= &count;
		r.out.info	= &info;

		torture_comment(tctx, "Testing EnumPrinters level %u\n", r.in.level);

		status = dcerpc_spoolss_EnumPrinters(p, ctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "dcerpc_spoolss_EnumPrinters failed");
		if (W_ERROR_IS_OK(r.out.result)) {
			/* TODO: do some more checks here */
			continue;
		}
		torture_assert_werr_equal(tctx, r.out.result, WERR_INSUFFICIENT_BUFFER,
			"EnumPrinters unexpected return code");

		blob = data_blob_talloc(ctx, NULL, needed);
		data_blob_clear(&blob);
		r.in.buffer = &blob;
		r.in.offered = needed;

		status = dcerpc_spoolss_EnumPrinters(p, ctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "dcerpc_spoolss_EnumPrinters failed");

		torture_assert_werr_ok(tctx, r.out.result, "EnumPrinters failed");

		CHECK_NEEDED_SIZE_ENUM_LEVEL(spoolss_EnumPrinters, info, r.in.level, count, lp_iconv_convenience(tctx->lp_ctx), needed, 4);

		ctx->printer_count[level]	= count;
		ctx->printers[level]		= info;
	}

	for (i=1;i<ARRAY_SIZE(levels);i++) {
		int level = levels[i];
		int old_level = levels[i-1];
		torture_assert_int_equal(tctx, ctx->printer_count[level], ctx->printer_count[old_level],
					 "EnumPrinters invalid value");
	}

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		int level = levels[i];
		for (j=0;j<ctx->printer_count[level];j++) {
			union spoolss_PrinterInfo *cur = &ctx->printers[level][j];
			union spoolss_PrinterInfo *ref = &ctx->printers[2][j];
			switch (level) {
			case 0:
				COMPARE_STRING(tctx, cur->info0, ref->info2, printername);
				COMPARE_STRING(tctx, cur->info0, ref->info2, servername);
				COMPARE_UINT32(tctx, cur->info0, ref->info2, cjobs);
				/*COMPARE_UINT32(tctx, cur->info0, ref->info2, total_jobs);
				COMPARE_UINT32(tctx, cur->info0, ref->info2, total_bytes);
				COMPARE_SPOOLSS_TIME(cur->info0, ref->info2, spoolss_Time time);
				COMPARE_UINT32(tctx, cur->info0, ref->info2, global_counter);
				COMPARE_UINT32(tctx, cur->info0, ref->info2, total_pages);
				COMPARE_UINT32(tctx, cur->info0, ref->info2, version);
				COMPARE_UINT32(tctx, cur->info0, ref->info2, unknown10);
				COMPARE_UINT32(tctx, cur->info0, ref->info2, unknown11);
				COMPARE_UINT32(tctx, cur->info0, ref->info2, unknown12);
				COMPARE_UINT32(tctx, cur->info0, ref->info2, session_counter);
				COMPARE_UINT32(tctx, cur->info0, ref->info2, unknown14);
				COMPARE_UINT32(tctx, cur->info0, ref->info2, printer_errors);
				COMPARE_UINT32(tctx, cur->info0, ref->info2, unknown16);
				COMPARE_UINT32(tctx, cur->info0, ref->info2, unknown17);
				COMPARE_UINT32(tctx, cur->info0, ref->info2, unknown18);
				COMPARE_UINT32(tctx, cur->info0, ref->info2, unknown19);
				COMPARE_UINT32(tctx, cur->info0, ref->info2, change_id);
				COMPARE_UINT32(tctx, cur->info0, ref->info2, unknown21);*/
				COMPARE_UINT32(tctx, cur->info0, ref->info2, status);
				/*COMPARE_UINT32(tctx, cur->info0, ref->info2, unknown23);
				COMPARE_UINT32(tctx, cur->info0, ref->info2, c_setprinter);
				COMPARE_UINT16(cur->info0, ref->info2, unknown25);
				COMPARE_UINT16(cur->info0, ref->info2, unknown26);
				COMPARE_UINT32(tctx, cur->info0, ref->info2, unknown27);
				COMPARE_UINT32(tctx, cur->info0, ref->info2, unknown28);
				COMPARE_UINT32(tctx, cur->info0, ref->info2, unknown29);*/
				break;
			case 1:
				/*COMPARE_UINT32(tctx, cur->info1, ref->info2, flags);*/
				/*COMPARE_STRING(tctx, cur->info1, ref->info2, name);*/
				/*COMPARE_STRING(tctx, cur->info1, ref->info2, description);*/
				COMPARE_STRING(tctx, cur->info1, ref->info2, comment);
				break;
			case 2:
				/* level 2 is our reference, and it makes no sense to compare it to itself */
				break;
			case 4:
				COMPARE_STRING(tctx, cur->info4, ref->info2, printername);
				COMPARE_STRING(tctx, cur->info4, ref->info2, servername);
				COMPARE_UINT32(tctx, cur->info4, ref->info2, attributes);
				break;
			case 5:
				COMPARE_STRING(tctx, cur->info5, ref->info2, printername);
				COMPARE_STRING(tctx, cur->info5, ref->info2, portname);
				COMPARE_UINT32(tctx, cur->info5, ref->info2, attributes);
				/*COMPARE_UINT32(tctx, cur->info5, ref->info2, device_not_selected_timeout);
				COMPARE_UINT32(tctx, cur->info5, ref->info2, transmission_retry_timeout);*/
				break;
			}
		}
	}

	/* TODO:
	 * 	- verify that the port of a printer was in the list returned by EnumPorts
	 */

	return true;
}

static bool test_GetPrinterDriver2(struct torture_context *tctx,
				   struct dcerpc_pipe *p,
				   struct policy_handle *handle,
				   const char *driver_name);

static bool test_GetPrinter_level(struct torture_context *tctx,
				  struct dcerpc_pipe *p,
				  struct policy_handle *handle,
				  uint32_t level,
				  union spoolss_PrinterInfo *info)
{
	struct spoolss_GetPrinter r;
	uint32_t needed;

	r.in.handle = handle;
	r.in.level = level;
	r.in.buffer = NULL;
	r.in.offered = 0;
	r.out.needed = &needed;

	torture_comment(tctx, "Testing GetPrinter level %u\n", r.in.level);

	torture_assert_ntstatus_ok(tctx, dcerpc_spoolss_GetPrinter(p, tctx, &r),
		"GetPrinter failed");

	if (W_ERROR_EQUAL(r.out.result, WERR_INSUFFICIENT_BUFFER)) {
		DATA_BLOB blob = data_blob_talloc(tctx, NULL, needed);
		data_blob_clear(&blob);
		r.in.buffer = &blob;
		r.in.offered = needed;

		torture_assert_ntstatus_ok(tctx, dcerpc_spoolss_GetPrinter(p, tctx, &r),
			"GetPrinter failed");
	}

	torture_assert_werr_ok(tctx, r.out.result, "GetPrinter failed");

	CHECK_NEEDED_SIZE_LEVEL(spoolss_PrinterInfo, r.out.info, r.in.level, lp_iconv_convenience(tctx->lp_ctx), needed, 4);

	if (info && r.out.info) {
		*info = *r.out.info;
	}

	return true;
}


static bool test_GetPrinter(struct torture_context *tctx,
			    struct dcerpc_pipe *p,
			    struct policy_handle *handle)
{
	uint32_t levels[] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
	int i;

	for (i=0;i<ARRAY_SIZE(levels);i++) {

		union spoolss_PrinterInfo info;

		ZERO_STRUCT(info);

		torture_assert(tctx, test_GetPrinter_level(tctx, p, handle, levels[i], &info),
			"failed to call GetPrinter");

		if ((levels[i] == 2) && info.info2.drivername && strlen(info.info2.drivername)) {
			torture_assert(tctx,
				test_GetPrinterDriver2(tctx, p, handle, info.info2.drivername),
				"failed to call test_GetPrinterDriver2");
		}
	}

	return true;
}

static bool test_SetPrinter(struct torture_context *tctx,
			    struct dcerpc_pipe *p,
			    struct policy_handle *handle,
			    struct spoolss_SetPrinterInfoCtr *info_ctr,
			    struct spoolss_DevmodeContainer *devmode_ctr,
			    struct sec_desc_buf *secdesc_ctr,
			    enum spoolss_PrinterControl command)
{
	struct spoolss_SetPrinter r;

	r.in.handle = handle;
	r.in.info_ctr = info_ctr;
	r.in.devmode_ctr = devmode_ctr;
	r.in.secdesc_ctr = secdesc_ctr;
	r.in.command = command;

	torture_comment(tctx, "Testing SetPrinter Level %d\n", r.in.info_ctr->level);

	torture_assert_ntstatus_ok(tctx, dcerpc_spoolss_SetPrinter(p, tctx, &r),
		"failed to call SetPrinter");
	torture_assert_werr_ok(tctx, r.out.result,
		"failed to call SetPrinter");

	return true;
}

static bool test_SetPrinter_errors(struct torture_context *tctx,
				   struct dcerpc_pipe *p,
				   struct policy_handle *handle)
{
	struct spoolss_SetPrinter r;
	uint16_t levels[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
	int i;

	struct spoolss_SetPrinterInfoCtr info_ctr;
	struct spoolss_DevmodeContainer devmode_ctr;
	struct sec_desc_buf secdesc_ctr;

	info_ctr.level = 0;
	info_ctr.info.info0 = NULL;

	ZERO_STRUCT(devmode_ctr);
	ZERO_STRUCT(secdesc_ctr);

	r.in.handle = handle;
	r.in.info_ctr = &info_ctr;
	r.in.devmode_ctr = &devmode_ctr;
	r.in.secdesc_ctr = &secdesc_ctr;
	r.in.command = 0;

	torture_comment(tctx, "Testing SetPrinter all zero\n");

	torture_assert_ntstatus_ok(tctx, dcerpc_spoolss_SetPrinter(p, tctx, &r),
		"failed to call SetPrinter");
	torture_assert_werr_equal(tctx, r.out.result, WERR_INVALID_PARAM,
		"failed to call SetPrinter");

 again:
	for (i=0; i < ARRAY_SIZE(levels); i++) {

		struct spoolss_SetPrinterInfo0 info0;
		struct spoolss_SetPrinterInfo1 info1;
		struct spoolss_SetPrinterInfo2 info2;
		struct spoolss_SetPrinterInfo3 info3;
		struct spoolss_SetPrinterInfo4 info4;
		struct spoolss_SetPrinterInfo5 info5;
		struct spoolss_SetPrinterInfo6 info6;
		struct spoolss_SetPrinterInfo7 info7;
		struct spoolss_SetPrinterInfo8 info8;
		struct spoolss_SetPrinterInfo9 info9;


		info_ctr.level = levels[i];
		switch (levels[i]) {
		case 0:
			ZERO_STRUCT(info0);
			info_ctr.info.info0 = &info0;
			break;
		case 1:
			ZERO_STRUCT(info1);
			info_ctr.info.info1 = &info1;
			break;
		case 2:
			ZERO_STRUCT(info2);
			info_ctr.info.info2 = &info2;
			break;
		case 3:
			ZERO_STRUCT(info3);
			info_ctr.info.info3 = &info3;
			break;
		case 4:
			ZERO_STRUCT(info4);
			info_ctr.info.info4 = &info4;
			break;
		case 5:
			ZERO_STRUCT(info5);
			info_ctr.info.info5 = &info5;
			break;
		case 6:
			ZERO_STRUCT(info6);
			info_ctr.info.info6 = &info6;
			break;
		case 7:
			ZERO_STRUCT(info7);
			info_ctr.info.info7 = &info7;
			break;
		case 8:
			ZERO_STRUCT(info8);
			info_ctr.info.info8 = &info8;
			break;
		case 9:
			ZERO_STRUCT(info9);
			info_ctr.info.info9 = &info9;
			break;
		}

		torture_comment(tctx, "Testing SetPrinter level %d, command %d\n",
			info_ctr.level, r.in.command);

		torture_assert_ntstatus_ok(tctx, dcerpc_spoolss_SetPrinter(p, tctx, &r),
			"failed to call SetPrinter");

		switch (r.in.command) {
		case SPOOLSS_PRINTER_CONTROL_UNPAUSE: /* 0 */
			/* is ignored for all levels other then 0 */
			if (info_ctr.level > 0) {
				/* ignored then */
				break;
			}
		case SPOOLSS_PRINTER_CONTROL_PAUSE: /* 1 */
		case SPOOLSS_PRINTER_CONTROL_RESUME: /* 2 */
		case SPOOLSS_PRINTER_CONTROL_PURGE: /* 3 */
			if (info_ctr.level > 0) {
				/* is invalid for all levels other then 0 */
				torture_assert_werr_equal(tctx, r.out.result, WERR_INVALID_PRINTER_COMMAND,
					"unexpected error code returned");
				continue;
			} else {
				torture_assert_werr_ok(tctx, r.out.result,
					"failed to call SetPrinter with non 0 command");
				continue;
			}
			break;

		case SPOOLSS_PRINTER_CONTROL_SET_STATUS: /* 4 */
			/* FIXME: gd needs further investigation */
		default:
			torture_assert_werr_equal(tctx, r.out.result, WERR_INVALID_PRINTER_COMMAND,
				"unexpected error code returned");
			continue;
		}

		switch (info_ctr.level) {
		case 1:
			torture_assert_werr_equal(tctx, r.out.result, WERR_UNKNOWN_LEVEL,
				"unexpected error code returned");
			break;
		case 2:
			torture_assert_werr_equal(tctx, r.out.result, WERR_UNKNOWN_PRINTER_DRIVER,
				"unexpected error code returned");
			break;
		case 3:
		case 4:
		case 5:
		case 7:
			torture_assert_werr_equal(tctx, r.out.result, WERR_INVALID_PARAM,
				"unexpected error code returned");
			break;
		case 9:
			torture_assert_werr_equal(tctx, r.out.result, WERR_NOT_SUPPORTED,
				"unexpected error code returned");
			break;
		default:
			torture_assert_werr_ok(tctx, r.out.result,
				"failed to call SetPrinter");
			break;
		}
	}

	if (r.in.command < 5) {
		r.in.command++;
		goto again;
	}

	return true;
}

static void clear_info2(struct spoolss_SetPrinterInfoCtr *r)
{
	if ((r->level == 2) && (r->info.info2)) {
		r->info.info2->secdesc_ptr = 0;
		r->info.info2->devmode_ptr = 0;
	}
}

static bool test_PrinterInfo(struct torture_context *tctx,
			     struct dcerpc_pipe *p,
			     struct policy_handle *handle)
{
	NTSTATUS status;
	struct spoolss_SetPrinter s;
	struct spoolss_GetPrinter q;
	struct spoolss_GetPrinter q0;
	struct spoolss_SetPrinterInfoCtr info_ctr;
	union spoolss_PrinterInfo info;
	struct spoolss_DevmodeContainer devmode_ctr;
	struct sec_desc_buf secdesc_ctr;
	uint32_t needed;
	bool ret = true;
	int i;

	uint32_t status_list[] = {
		/* these do not stick
		PRINTER_STATUS_PAUSED,
		PRINTER_STATUS_ERROR,
		PRINTER_STATUS_PENDING_DELETION, */
		PRINTER_STATUS_PAPER_JAM,
		PRINTER_STATUS_PAPER_OUT,
		PRINTER_STATUS_MANUAL_FEED,
		PRINTER_STATUS_PAPER_PROBLEM,
		PRINTER_STATUS_OFFLINE,
		PRINTER_STATUS_IO_ACTIVE,
		PRINTER_STATUS_BUSY,
		PRINTER_STATUS_PRINTING,
		PRINTER_STATUS_OUTPUT_BIN_FULL,
		PRINTER_STATUS_NOT_AVAILABLE,
		PRINTER_STATUS_WAITING,
		PRINTER_STATUS_PROCESSING,
		PRINTER_STATUS_INITIALIZING,
		PRINTER_STATUS_WARMING_UP,
		PRINTER_STATUS_TONER_LOW,
		PRINTER_STATUS_NO_TONER,
		PRINTER_STATUS_PAGE_PUNT,
		PRINTER_STATUS_USER_INTERVENTION,
		PRINTER_STATUS_OUT_OF_MEMORY,
		PRINTER_STATUS_DOOR_OPEN,
		PRINTER_STATUS_SERVER_UNKNOWN,
		PRINTER_STATUS_POWER_SAVE,
		/* these do not stick
		0x02000000,
		0x04000000,
		0x08000000,
		0x10000000,
		0x20000000,
		0x40000000,
		0x80000000 */
	};
	uint32_t default_attribute = PRINTER_ATTRIBUTE_LOCAL;
	uint32_t attribute_list[] = {
		PRINTER_ATTRIBUTE_QUEUED,
		/* fails with WERR_INVALID_DATATYPE:
		PRINTER_ATTRIBUTE_DIRECT, */
		/* does not stick
		PRINTER_ATTRIBUTE_DEFAULT, */
		PRINTER_ATTRIBUTE_SHARED,
		/* does not stick
		PRINTER_ATTRIBUTE_NETWORK, */
		PRINTER_ATTRIBUTE_HIDDEN,
		PRINTER_ATTRIBUTE_LOCAL,
		PRINTER_ATTRIBUTE_ENABLE_DEVQ,
		PRINTER_ATTRIBUTE_KEEPPRINTEDJOBS,
		PRINTER_ATTRIBUTE_DO_COMPLETE_FIRST,
		PRINTER_ATTRIBUTE_WORK_OFFLINE,
		/* does not stick
		PRINTER_ATTRIBUTE_ENABLE_BIDI, */
		/* fails with WERR_INVALID_DATATYPE:
		PRINTER_ATTRIBUTE_RAW_ONLY, */
		/* these do not stick
		PRINTER_ATTRIBUTE_PUBLISHED,
		PRINTER_ATTRIBUTE_FAX,
		PRINTER_ATTRIBUTE_TS,
		0x00010000,
		0x00020000,
		0x00040000,
		0x00080000,
		0x00100000,
		0x00200000,
		0x00400000,
		0x00800000,
		0x01000000,
		0x02000000,
		0x04000000,
		0x08000000,
		0x10000000,
		0x20000000,
		0x40000000,
		0x80000000 */
	};

	ZERO_STRUCT(devmode_ctr);
	ZERO_STRUCT(secdesc_ctr);

	s.in.handle = handle;
	s.in.command = 0;
	s.in.info_ctr = &info_ctr;
	s.in.devmode_ctr = &devmode_ctr;
	s.in.secdesc_ctr = &secdesc_ctr;

	q.in.handle = handle;
	q.out.info = &info;
	q0 = q;

#define TESTGETCALL(call, r) \
		r.in.buffer = NULL; \
		r.in.offered = 0;\
		r.out.needed = &needed; \
		status = dcerpc_spoolss_ ##call(p, tctx, &r); \
		if (!NT_STATUS_IS_OK(status)) { \
			torture_comment(tctx, #call " level %u failed - %s (%s)\n", \
			       r.in.level, nt_errstr(status), __location__); \
			ret = false; \
			break; \
		}\
		if (W_ERROR_EQUAL(r.out.result, WERR_INSUFFICIENT_BUFFER)) {\
			DATA_BLOB blob = data_blob_talloc(tctx, NULL, needed); \
			data_blob_clear(&blob); \
			r.in.buffer = &blob; \
			r.in.offered = needed; \
		}\
		status = dcerpc_spoolss_ ##call(p, tctx, &r); \
		if (!NT_STATUS_IS_OK(status)) { \
			torture_comment(tctx, #call " level %u failed - %s (%s)\n", \
			       r.in.level, nt_errstr(status), __location__); \
			ret = false; \
			break; \
		} \
		if (!W_ERROR_IS_OK(r.out.result)) { \
			torture_comment(tctx, #call " level %u failed - %s (%s)\n", \
			       r.in.level, win_errstr(r.out.result), __location__); \
			ret = false; \
			break; \
		}


#define TESTSETCALL_EXP(call, r, err) \
		clear_info2(&info_ctr);\
		status = dcerpc_spoolss_ ##call(p, tctx, &r); \
		if (!NT_STATUS_IS_OK(status)) { \
			torture_comment(tctx, #call " level %u failed - %s (%s)\n", \
			       r.in.info_ctr->level, nt_errstr(status), __location__); \
			ret = false; \
			break; \
		} \
		if (!W_ERROR_IS_OK(err)) { \
			if (!W_ERROR_EQUAL(err, r.out.result)) { \
				torture_comment(tctx, #call " level %u failed - %s, expected %s (%s)\n", \
				       r.in.info_ctr->level, win_errstr(r.out.result), win_errstr(err), __location__); \
				ret = false; \
			} \
			break; \
		} \
		if (!W_ERROR_IS_OK(r.out.result)) { \
			torture_comment(tctx, #call " level %u failed - %s (%s)\n", \
			       r.in.info_ctr->level, win_errstr(r.out.result), __location__); \
			ret = false; \
			break; \
		}

#define TESTSETCALL(call, r) \
	TESTSETCALL_EXP(call, r, WERR_OK)

#define STRING_EQUAL(s1, s2, field) \
		if ((s1 && !s2) || (s2 && !s1) || strcmp(s1, s2)) { \
			torture_comment(tctx, "Failed to set %s to '%s' (%s)\n", \
			       #field, s2, __location__); \
			ret = false; \
			break; \
		}

#define MEM_EQUAL(s1, s2, length, field) \
		if ((s1 && !s2) || (s2 && !s1) || memcmp(s1, s2, length)) { \
			torture_comment(tctx, "Failed to set %s to '%s' (%s)\n", \
			       #field, (const char *)s2, __location__); \
			ret = false; \
			break; \
		}

#define INT_EQUAL(i1, i2, field) \
		if (i1 != i2) { \
			torture_comment(tctx, "Failed to set %s to 0x%llx - got 0x%llx (%s)\n", \
			       #field, (unsigned long long)i2, (unsigned long long)i1, __location__); \
			ret = false; \
			break; \
		}

#define TEST_PRINTERINFO_STRING_EXP_ERR(lvl1, field1, lvl2, field2, value, err) do { \
		torture_comment(tctx, "field test %d/%s vs %d/%s\n", lvl1, #field1, lvl2, #field2); \
		q.in.level = lvl1; \
		TESTGETCALL(GetPrinter, q) \
		info_ctr.level = lvl1; \
		info_ctr.info.info ## lvl1 = (struct spoolss_SetPrinterInfo ## lvl1 *)&q.out.info->info ## lvl1; \
		info_ctr.info.info ## lvl1->field1 = value;\
		TESTSETCALL_EXP(SetPrinter, s, err) \
		info_ctr.info.info ## lvl1->field1 = ""; \
		TESTGETCALL(GetPrinter, q) \
		info_ctr.info.info ## lvl1->field1 = value; \
		STRING_EQUAL(info_ctr.info.info ## lvl1->field1, value, field1); \
		q.in.level = lvl2; \
		TESTGETCALL(GetPrinter, q) \
		info_ctr.info.info ## lvl2 = (struct spoolss_SetPrinterInfo ## lvl2 *)&q.out.info->info ## lvl2; \
		STRING_EQUAL(info_ctr.info.info ## lvl2->field2, value, field2); \
	} while (0)

#define TEST_PRINTERINFO_STRING(lvl1, field1, lvl2, field2, value) do { \
	TEST_PRINTERINFO_STRING_EXP_ERR(lvl1, field1, lvl2, field2, value, WERR_OK); \
	} while (0);

#define TEST_PRINTERINFO_INT_EXP(lvl1, field1, lvl2, field2, value, exp_value) do { \
		torture_comment(tctx, "field test %d/%s vs %d/%s\n", lvl1, #field1, lvl2, #field2); \
		q.in.level = lvl1; \
		TESTGETCALL(GetPrinter, q) \
		info_ctr.level = lvl1; \
		info_ctr.info.info ## lvl1 = (struct spoolss_SetPrinterInfo ## lvl1 *)&q.out.info->info ## lvl1; \
		info_ctr.info.info ## lvl1->field1 = value; \
		TESTSETCALL(SetPrinter, s) \
		info_ctr.info.info ## lvl1->field1 = 0; \
		TESTGETCALL(GetPrinter, q) \
		info_ctr.info.info ## lvl1 = (struct spoolss_SetPrinterInfo ## lvl1 *)&q.out.info->info ## lvl1; \
		INT_EQUAL(info_ctr.info.info ## lvl1->field1, exp_value, field1); \
		q.in.level = lvl2; \
		TESTGETCALL(GetPrinter, q) \
		info_ctr.info.info ## lvl2 = (struct spoolss_SetPrinterInfo ## lvl2 *)&q.out.info->info ## lvl2; \
		INT_EQUAL(info_ctr.info.info ## lvl2->field2, exp_value, field1); \
	} while (0)

#define TEST_PRINTERINFO_INT(lvl1, field1, lvl2, field2, value) do { \
        TEST_PRINTERINFO_INT_EXP(lvl1, field1, lvl2, field2, value, value); \
        } while (0)

	q0.in.level = 0;
	do { TESTGETCALL(GetPrinter, q0) } while (0);

	TEST_PRINTERINFO_STRING(2, comment,  1, comment, "xx2-1 comment");
	TEST_PRINTERINFO_STRING(2, comment,  2, comment, "xx2-2 comment");

	/* level 0 printername does not stick */
/*	TEST_PRINTERINFO_STRING(2, printername,  0, printername, "xx2-0 printer"); */
	TEST_PRINTERINFO_STRING(2, printername,  1, name,	 "xx2-1 printer");
	TEST_PRINTERINFO_STRING(2, printername,  2, printername, "xx2-2 printer");
	TEST_PRINTERINFO_STRING(2, printername,  4, printername, "xx2-4 printer");
	TEST_PRINTERINFO_STRING(2, printername,  5, printername, "xx2-5 printer");
/*	TEST_PRINTERINFO_STRING(4, printername,  0, printername, "xx4-0 printer"); */
	TEST_PRINTERINFO_STRING(4, printername,  1, name,	 "xx4-1 printer");
	TEST_PRINTERINFO_STRING(4, printername,  2, printername, "xx4-2 printer");
	TEST_PRINTERINFO_STRING(4, printername,  4, printername, "xx4-4 printer");
	TEST_PRINTERINFO_STRING(4, printername,  5, printername, "xx4-5 printer");
/*	TEST_PRINTERINFO_STRING(5, printername,  0, printername, "xx5-0 printer"); */
	TEST_PRINTERINFO_STRING(5, printername,  1, name,	 "xx5-1 printer");
	TEST_PRINTERINFO_STRING(5, printername,  2, printername, "xx5-2 printer");
	TEST_PRINTERINFO_STRING(5, printername,  4, printername, "xx5-4 printer");
	TEST_PRINTERINFO_STRING(5, printername,  5, printername, "xx5-5 printer");

	/* servername can be set but does not stick
	TEST_PRINTERINFO_STRING(2, servername,  0, servername, "xx2-0 servername");
	TEST_PRINTERINFO_STRING(2, servername,  2, servername, "xx2-2 servername");
	TEST_PRINTERINFO_STRING(2, servername,  4, servername, "xx2-4 servername");
	*/

	/* passing an invalid port will result in WERR_UNKNOWN_PORT */
	TEST_PRINTERINFO_STRING_EXP_ERR(2, portname,  2, portname, "xx2-2 portname", WERR_UNKNOWN_PORT);
	TEST_PRINTERINFO_STRING_EXP_ERR(2, portname,  5, portname, "xx2-5 portname", WERR_UNKNOWN_PORT);
	TEST_PRINTERINFO_STRING_EXP_ERR(5, portname,  2, portname, "xx5-2 portname", WERR_UNKNOWN_PORT);
	TEST_PRINTERINFO_STRING_EXP_ERR(5, portname,  5, portname, "xx5-5 portname", WERR_UNKNOWN_PORT);

	TEST_PRINTERINFO_STRING(2, sharename,	2, sharename,	"xx2-2 sharename");
	/* passing an invalid driver will result in WERR_UNKNOWN_PRINTER_DRIVER */
	TEST_PRINTERINFO_STRING_EXP_ERR(2, drivername,	2, drivername,	"xx2-2 drivername", WERR_UNKNOWN_PRINTER_DRIVER);
	TEST_PRINTERINFO_STRING(2, location,	2, location,	"xx2-2 location");
	/* passing an invalid sepfile will result in WERR_INVALID_SEPARATOR_FILE */
	TEST_PRINTERINFO_STRING_EXP_ERR(2, sepfile,	2, sepfile,	"xx2-2 sepfile", WERR_INVALID_SEPARATOR_FILE);
	/* passing an invalid printprocessor will result in WERR_UNKNOWN_PRINTPROCESSOR */
	TEST_PRINTERINFO_STRING_EXP_ERR(2, printprocessor, 2, printprocessor, "xx2-2 printprocessor", WERR_UNKNOWN_PRINTPROCESSOR);
	TEST_PRINTERINFO_STRING(2, datatype,	2, datatype,	"xx2-2 datatype");
	TEST_PRINTERINFO_STRING(2, parameters,	2, parameters,	"xx2-2 parameters");

	for (i=0; i < ARRAY_SIZE(attribute_list); i++) {
/*		TEST_PRINTERINFO_INT_EXP(2, attributes, 1, flags,
			attribute_list[i],
			(attribute_list[i] | default_attribute)
			); */
		TEST_PRINTERINFO_INT_EXP(2, attributes, 2, attributes,
			attribute_list[i],
			(attribute_list[i] | default_attribute)
			);
		TEST_PRINTERINFO_INT_EXP(2, attributes, 4, attributes,
			attribute_list[i],
			(attribute_list[i] | default_attribute)
			);
		TEST_PRINTERINFO_INT_EXP(2, attributes, 5, attributes,
			attribute_list[i],
			(attribute_list[i] | default_attribute)
			);
/*		TEST_PRINTERINFO_INT_EXP(4, attributes, 1, flags,
			attribute_list[i],
			(attribute_list[i] | default_attribute)
			); */
		TEST_PRINTERINFO_INT_EXP(4, attributes, 2, attributes,
			attribute_list[i],
			(attribute_list[i] | default_attribute)
			);
		TEST_PRINTERINFO_INT_EXP(4, attributes, 4, attributes,
			attribute_list[i],
			(attribute_list[i] | default_attribute)
			);
		TEST_PRINTERINFO_INT_EXP(4, attributes, 5, attributes,
			attribute_list[i],
			(attribute_list[i] | default_attribute)
			);
/*		TEST_PRINTERINFO_INT_EXP(5, attributes, 1, flags,
			attribute_list[i],
			(attribute_list[i] | default_attribute)
			); */
		TEST_PRINTERINFO_INT_EXP(5, attributes, 2, attributes,
			attribute_list[i],
			(attribute_list[i] | default_attribute)
			);
		TEST_PRINTERINFO_INT_EXP(5, attributes, 4, attributes,
			attribute_list[i],
			(attribute_list[i] | default_attribute)
			);
		TEST_PRINTERINFO_INT_EXP(5, attributes, 5, attributes,
			attribute_list[i],
			(attribute_list[i] | default_attribute)
			);
	}

	for (i=0; i < ARRAY_SIZE(status_list); i++) {
		/* level 2 sets do not stick
		TEST_PRINTERINFO_INT(2, status,	0, status, status_list[i]);
		TEST_PRINTERINFO_INT(2, status,	2, status, status_list[i]);
		TEST_PRINTERINFO_INT(2, status,	6, status, status_list[i]); */
		TEST_PRINTERINFO_INT(6, status,	0, status, status_list[i]);
		TEST_PRINTERINFO_INT(6, status,	2, status, status_list[i]);
		TEST_PRINTERINFO_INT(6, status,	6, status, status_list[i]);
	}

	/* priorities need to be between 0 and 99
	   passing an invalid priority will result in WERR_INVALID_PRIORITY */
	TEST_PRINTERINFO_INT(2, priority,	2, priority, 0);
	TEST_PRINTERINFO_INT(2, priority,	2, priority, 1);
	TEST_PRINTERINFO_INT(2, priority,	2, priority, 99);
	/* TEST_PRINTERINFO_INT(2, priority,	2, priority, 100); */
	TEST_PRINTERINFO_INT(2, defaultpriority,2, defaultpriority, 0);
	TEST_PRINTERINFO_INT(2, defaultpriority,2, defaultpriority, 1);
	TEST_PRINTERINFO_INT(2, defaultpriority,2, defaultpriority, 99);
	/* TEST_PRINTERINFO_INT(2, defaultpriority,2, defaultpriority, 100); */

	TEST_PRINTERINFO_INT(2, starttime,	2, starttime, __LINE__);
	TEST_PRINTERINFO_INT(2, untiltime,	2, untiltime, __LINE__);

	/* does not stick
	TEST_PRINTERINFO_INT(2, cjobs,		2, cjobs, __LINE__);
	TEST_PRINTERINFO_INT(2, averageppm,	2, averageppm, __LINE__); */

	/* does not stick
	TEST_PRINTERINFO_INT(5, device_not_selected_timeout, 5, device_not_selected_timeout, __LINE__);
	TEST_PRINTERINFO_INT(5, transmission_retry_timeout, 5, transmission_retry_timeout, __LINE__); */

	/* FIXME: gd also test devmode and secdesc behavior */

	{
		/* verify composition of level 1 description field */
		const char *description;
		const char *tmp;

		q0.in.level = 1;
		do { TESTGETCALL(GetPrinter, q0) } while (0);

		description = talloc_strdup(tctx, q0.out.info->info1.description);

		q0.in.level = 2;
		do { TESTGETCALL(GetPrinter, q0) } while (0);

		tmp = talloc_asprintf(tctx, "%s,%s,%s",
			q0.out.info->info2.printername,
			q0.out.info->info2.drivername,
			q0.out.info->info2.location);

		do { STRING_EQUAL(description, tmp, "description")} while (0);
	}

	return ret;
}


static bool test_ClosePrinter(struct torture_context *tctx,
			      struct dcerpc_pipe *p,
			      struct policy_handle *handle)
{
	NTSTATUS status;
	struct spoolss_ClosePrinter r;

	r.in.handle = handle;
	r.out.handle = handle;

	torture_comment(tctx, "Testing ClosePrinter\n");

	status = dcerpc_spoolss_ClosePrinter(p, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "ClosePrinter failed");
	torture_assert_werr_ok(tctx, r.out.result, "ClosePrinter failed");

	return true;
}

static bool test_GetForm(struct torture_context *tctx,
			 struct dcerpc_pipe *p,
			 struct policy_handle *handle,
			 const char *form_name,
			 uint32_t level)
{
	NTSTATUS status;
	struct spoolss_GetForm r;
	uint32_t needed;

	r.in.handle = handle;
	r.in.form_name = form_name;
	r.in.level = level;
	r.in.buffer = NULL;
	r.in.offered = 0;
	r.out.needed = &needed;

	torture_comment(tctx, "Testing GetForm level %d\n", r.in.level);

	status = dcerpc_spoolss_GetForm(p, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "GetForm failed");

	if (W_ERROR_EQUAL(r.out.result, WERR_INSUFFICIENT_BUFFER)) {
		DATA_BLOB blob = data_blob_talloc(tctx, NULL, needed);
		data_blob_clear(&blob);
		r.in.buffer = &blob;
		r.in.offered = needed;
		status = dcerpc_spoolss_GetForm(p, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "GetForm failed");

		torture_assert_werr_ok(tctx, r.out.result, "GetForm failed");

		torture_assert(tctx, r.out.info, "No form info returned");
	}

	torture_assert_werr_ok(tctx, r.out.result, "GetForm failed");

	CHECK_NEEDED_SIZE_LEVEL(spoolss_FormInfo, r.out.info, r.in.level, lp_iconv_convenience(tctx->lp_ctx), needed, 4);

	return true;
}

static bool test_EnumForms(struct torture_context *tctx,
			   struct dcerpc_pipe *p,
			   struct policy_handle *handle, bool print_server)
{
	NTSTATUS status;
	struct spoolss_EnumForms r;
	bool ret = true;
	uint32_t needed;
	uint32_t count;
	uint32_t levels[] = { 1, 2 };
	int i;

	for (i=0; i<ARRAY_SIZE(levels); i++) {

		union spoolss_FormInfo *info;

		r.in.handle = handle;
		r.in.level = levels[i];
		r.in.buffer = NULL;
		r.in.offered = 0;
		r.out.needed = &needed;
		r.out.count = &count;
		r.out.info = &info;

		torture_comment(tctx, "Testing EnumForms level %d\n", levels[i]);

		status = dcerpc_spoolss_EnumForms(p, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "EnumForms failed");

		if ((r.in.level == 2) && (W_ERROR_EQUAL(r.out.result, WERR_UNKNOWN_LEVEL))) {
			break;
		}

		if (print_server && W_ERROR_EQUAL(r.out.result, WERR_BADFID))
			torture_fail(tctx, "EnumForms on the PrintServer isn't supported by test server (NT4)");

		if (W_ERROR_EQUAL(r.out.result, WERR_INSUFFICIENT_BUFFER)) {
			int j;
			DATA_BLOB blob = data_blob_talloc(tctx, NULL, needed);
			data_blob_clear(&blob);
			r.in.buffer = &blob;
			r.in.offered = needed;

			status = dcerpc_spoolss_EnumForms(p, tctx, &r);

			torture_assert(tctx, info, "No forms returned");

			for (j = 0; j < count; j++) {
				if (!print_server)
					ret &= test_GetForm(tctx, p, handle, info[j].info1.form_name, levels[i]);
			}
		}

		torture_assert_ntstatus_ok(tctx, status, "EnumForms failed");

		torture_assert_werr_ok(tctx, r.out.result, "EnumForms failed");

		CHECK_NEEDED_SIZE_ENUM_LEVEL(spoolss_EnumForms, info, r.in.level, count, lp_iconv_convenience(tctx->lp_ctx), needed, 4);
	}

	return true;
}

static bool test_DeleteForm(struct torture_context *tctx,
			    struct dcerpc_pipe *p,
			    struct policy_handle *handle,
			    const char *form_name)
{
	NTSTATUS status;
	struct spoolss_DeleteForm r;

	r.in.handle = handle;
	r.in.form_name = form_name;

	status = dcerpc_spoolss_DeleteForm(p, tctx, &r);

	torture_assert_ntstatus_ok(tctx, status, "DeleteForm failed");

	torture_assert_werr_ok(tctx, r.out.result, "DeleteForm failed");

	return true;
}

static bool test_AddForm(struct torture_context *tctx,
			 struct dcerpc_pipe *p,
			 struct policy_handle *handle, bool print_server)
{
	struct spoolss_AddForm r;
	struct spoolss_AddFormInfo1 addform;
	const char *form_name = "testform3";
	NTSTATUS status;
	bool ret = true;

	r.in.handle	= handle;
	r.in.level	= 1;
	r.in.info.info1 = &addform;
	addform.flags		= SPOOLSS_FORM_USER;
	addform.form_name	= form_name;
	addform.size.width	= 50;
	addform.size.height	= 25;
	addform.area.left	= 5;
	addform.area.top	= 10;
	addform.area.right	= 45;
	addform.area.bottom	= 15;

	status = dcerpc_spoolss_AddForm(p, tctx, &r);

	torture_assert_ntstatus_ok(tctx, status, "AddForm failed");

	torture_assert_werr_ok(tctx, r.out.result, "AddForm failed");

	if (!print_server) ret &= test_GetForm(tctx, p, handle, form_name, 1);

	{
		struct spoolss_SetForm sf;
		struct spoolss_AddFormInfo1 setform;

		sf.in.handle	= handle;
		sf.in.form_name = form_name;
		sf.in.level	= 1;
		sf.in.info.info1= &setform;
		setform.flags		= addform.flags;
		setform.form_name	= addform.form_name;
		setform.size		= addform.size;
		setform.area		= addform.area;

		setform.size.width	= 1234;

		status = dcerpc_spoolss_SetForm(p, tctx, &sf);

		torture_assert_ntstatus_ok(tctx, status, "SetForm failed");

		torture_assert_werr_ok(tctx, r.out.result, "SetForm failed");
	}

	if (!print_server) ret &= test_GetForm(tctx, p, handle, form_name, 1);

	{
		struct spoolss_EnumForms e;
		union spoolss_FormInfo *info;
		uint32_t needed;
		uint32_t count;
		bool found = false;

		e.in.handle = handle;
		e.in.level = 1;
		e.in.buffer = NULL;
		e.in.offered = 0;
		e.out.needed = &needed;
		e.out.count = &count;
		e.out.info = &info;

		torture_comment(tctx, "Testing EnumForms level 1\n");

		status = dcerpc_spoolss_EnumForms(p, tctx, &e);
		torture_assert_ntstatus_ok(tctx, status, "EnumForms failed");

		if (print_server && W_ERROR_EQUAL(e.out.result, WERR_BADFID))
			torture_fail(tctx, "EnumForms on the PrintServer isn't supported by test server (NT4)");

		if (W_ERROR_EQUAL(e.out.result, WERR_INSUFFICIENT_BUFFER)) {
			int j;
			DATA_BLOB blob = data_blob_talloc(tctx, NULL, needed);
			data_blob_clear(&blob);
			e.in.buffer = &blob;
			e.in.offered = needed;

			status = dcerpc_spoolss_EnumForms(p, tctx, &e);

			torture_assert(tctx, info, "No forms returned");

			for (j = 0; j < count; j++) {
				if (strequal(form_name, info[j].info1.form_name)) {
					found = true;
					break;
				}
			}
		}
		torture_assert(tctx, found, "Newly added form not found in enum call");
	}

	if (!test_DeleteForm(tctx, p, handle, form_name)) {
		ret = false;
	}

	return ret;
}

static bool test_EnumPorts_old(struct torture_context *tctx,
			       struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct spoolss_EnumPorts r;
	uint32_t needed;
	uint32_t count;
	union spoolss_PortInfo *info;

	r.in.servername = talloc_asprintf(tctx, "\\\\%s",
					  dcerpc_server_name(p));
	r.in.level = 2;
	r.in.buffer = NULL;
	r.in.offered = 0;
	r.out.needed = &needed;
	r.out.count = &count;
	r.out.info = &info;

	torture_comment(tctx, "Testing EnumPorts\n");

	status = dcerpc_spoolss_EnumPorts(p, tctx, &r);

	torture_assert_ntstatus_ok(tctx, status, "EnumPorts failed");

	if (W_ERROR_EQUAL(r.out.result, WERR_INSUFFICIENT_BUFFER)) {
		DATA_BLOB blob = data_blob_talloc(tctx, NULL, needed);
		data_blob_clear(&blob);
		r.in.buffer = &blob;
		r.in.offered = needed;

		status = dcerpc_spoolss_EnumPorts(p, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "EnumPorts failed");
		torture_assert_werr_ok(tctx, r.out.result, "EnumPorts failed");

		torture_assert(tctx, info, "No ports returned");
	}

	torture_assert_werr_ok(tctx, r.out.result, "EnumPorts failed");

	CHECK_NEEDED_SIZE_ENUM_LEVEL(spoolss_EnumPorts, info, 2, count, lp_iconv_convenience(tctx->lp_ctx), needed, 4);

	return true;
}

static bool test_AddPort(struct torture_context *tctx,
			 struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct spoolss_AddPort r;

	r.in.server_name = talloc_asprintf(tctx, "\\\\%s",
					   dcerpc_server_name(p));
	r.in.unknown = 0;
	r.in.monitor_name = "foo";

	torture_comment(tctx, "Testing AddPort\n");

	status = dcerpc_spoolss_AddPort(p, tctx, &r);

	torture_assert_ntstatus_ok(tctx, status, "AddPort failed");

	/* win2k3 returns WERR_NOT_SUPPORTED */

#if 0

	if (!W_ERROR_IS_OK(r.out.result)) {
		printf("AddPort failed - %s\n", win_errstr(r.out.result));
		return false;
	}

#endif

	return true;
}

static bool test_GetJob(struct torture_context *tctx,
			struct dcerpc_pipe *p,
			struct policy_handle *handle, uint32_t job_id)
{
	NTSTATUS status;
	struct spoolss_GetJob r;
	union spoolss_JobInfo info;
	uint32_t needed;
	uint32_t levels[] = {1, 2 /* 3, 4 */};
	uint32_t i;

	r.in.handle = handle;
	r.in.job_id = job_id;
	r.in.level = 0;
	r.in.buffer = NULL;
	r.in.offered = 0;
	r.out.needed = &needed;
	r.out.info = &info;

	torture_comment(tctx, "Testing GetJob level %d\n", r.in.level);

	status = dcerpc_spoolss_GetJob(p, tctx, &r);
	torture_assert_werr_equal(tctx, r.out.result, WERR_UNKNOWN_LEVEL, "Unexpected return code");

	for (i = 0; i < ARRAY_SIZE(levels); i++) {

		torture_comment(tctx, "Testing GetJob level %d\n", r.in.level);

		needed = 0;

		r.in.level = levels[i];
		r.in.offered = 0;
		r.in.buffer = NULL;

		status = dcerpc_spoolss_GetJob(p, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "GetJob failed");

		if (W_ERROR_EQUAL(r.out.result, WERR_INSUFFICIENT_BUFFER)) {
			DATA_BLOB blob = data_blob_talloc(tctx, NULL, needed);
			data_blob_clear(&blob);
			r.in.buffer = &blob;
			r.in.offered = needed;

			status = dcerpc_spoolss_GetJob(p, tctx, &r);
			torture_assert_ntstatus_ok(tctx, status, "GetJob failed");

		}
		torture_assert(tctx, r.out.info, "No job info returned");
		torture_assert_werr_ok(tctx, r.out.result, "GetJob failed");

		CHECK_NEEDED_SIZE_LEVEL(spoolss_JobInfo, r.out.info, r.in.level, lp_iconv_convenience(tctx->lp_ctx), needed, 4);
	}

	return true;
}

static bool test_SetJob(struct torture_context *tctx,
			struct dcerpc_pipe *p,
			struct policy_handle *handle, uint32_t job_id,
			enum spoolss_JobControl command)
{
	NTSTATUS status;
	struct spoolss_SetJob r;

	r.in.handle	= handle;
	r.in.job_id	= job_id;
	r.in.ctr	= NULL;
	r.in.command	= command;

	switch (command) {
	case SPOOLSS_JOB_CONTROL_PAUSE:
		torture_comment(tctx, "Testing SetJob: SPOOLSS_JOB_CONTROL_PAUSE\n");
		break;
	case SPOOLSS_JOB_CONTROL_RESUME:
		torture_comment(tctx, "Testing SetJob: SPOOLSS_JOB_CONTROL_RESUME\n");
		break;
	case SPOOLSS_JOB_CONTROL_CANCEL:
		torture_comment(tctx, "Testing SetJob: SPOOLSS_JOB_CONTROL_CANCEL\n");
		break;
	case SPOOLSS_JOB_CONTROL_RESTART:
		torture_comment(tctx, "Testing SetJob: SPOOLSS_JOB_CONTROL_RESTART\n");
		break;
	case SPOOLSS_JOB_CONTROL_DELETE:
		torture_comment(tctx, "Testing SetJob: SPOOLSS_JOB_CONTROL_DELETE\n");
		break;
	case SPOOLSS_JOB_CONTROL_SEND_TO_PRINTER:
		torture_comment(tctx, "Testing SetJob: SPOOLSS_JOB_CONTROL_SEND_TO_PRINTER\n");
		break;
	case SPOOLSS_JOB_CONTROL_LAST_PAGE_EJECTED:
		torture_comment(tctx, "Testing SetJob: SPOOLSS_JOB_CONTROL_LAST_PAGE_EJECTED\n");
		break;
	case SPOOLSS_JOB_CONTROL_RETAIN:
		torture_comment(tctx, "Testing SetJob: SPOOLSS_JOB_CONTROL_RETAIN\n");
		break;
	case SPOOLSS_JOB_CONTROL_RELEASE:
		torture_comment(tctx, "Testing SetJob: SPOOLSS_JOB_CONTROL_RELEASE\n");
		break;
	default:
		torture_comment(tctx, "Testing SetJob\n");
		break;
	}

	status = dcerpc_spoolss_SetJob(p, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "SetJob failed");
	torture_assert_werr_ok(tctx, r.out.result, "SetJob failed");

	return true;
}

static bool test_AddJob(struct torture_context *tctx,
			struct dcerpc_pipe *p,
			struct policy_handle *handle)
{
	NTSTATUS status;
	struct spoolss_AddJob r;
	uint32_t needed;

	r.in.level = 0;
	r.in.handle = handle;
	r.in.offered = 0;
	r.out.needed = &needed;
	r.in.buffer = r.out.buffer = NULL;

	torture_comment(tctx, "Testing AddJob\n");

	status = dcerpc_spoolss_AddJob(p, tctx, &r);
	torture_assert_werr_equal(tctx, r.out.result, WERR_UNKNOWN_LEVEL, "AddJob failed");

	r.in.level = 1;

	status = dcerpc_spoolss_AddJob(p, tctx, &r);
	torture_assert_werr_equal(tctx, r.out.result, WERR_INVALID_PARAM, "AddJob failed");

	return true;
}


static bool test_EnumJobs(struct torture_context *tctx,
			  struct dcerpc_pipe *p,
			  struct policy_handle *handle)
{
	NTSTATUS status;
	struct spoolss_EnumJobs r;
	uint32_t needed;
	uint32_t count;
	union spoolss_JobInfo *info;

	r.in.handle = handle;
	r.in.firstjob = 0;
	r.in.numjobs = 0xffffffff;
	r.in.level = 1;
	r.in.buffer = NULL;
	r.in.offered = 0;
	r.out.needed = &needed;
	r.out.count = &count;
	r.out.info = &info;

	torture_comment(tctx, "Testing EnumJobs\n");

	status = dcerpc_spoolss_EnumJobs(p, tctx, &r);

	torture_assert_ntstatus_ok(tctx, status, "EnumJobs failed");

	if (W_ERROR_EQUAL(r.out.result, WERR_INSUFFICIENT_BUFFER)) {
		int j;
		DATA_BLOB blob = data_blob_talloc(tctx, NULL, needed);
		data_blob_clear(&blob);
		r.in.buffer = &blob;
		r.in.offered = needed;

		status = dcerpc_spoolss_EnumJobs(p, tctx, &r);

		torture_assert_ntstatus_ok(tctx, status, "EnumJobs failed");
		torture_assert_werr_ok(tctx, r.out.result, "EnumJobs failed");
		torture_assert(tctx, info, "No jobs returned");

		CHECK_NEEDED_SIZE_ENUM_LEVEL(spoolss_EnumJobs, *r.out.info, r.in.level, count, lp_iconv_convenience(tctx->lp_ctx), needed, 4);

		for (j = 0; j < count; j++) {

			torture_assert(tctx, test_GetJob(tctx, p, handle, info[j].info1.job_id),
				"failed to call test_GetJob");

			/* FIXME - gd */
			if (!torture_setting_bool(tctx, "samba3", false)) {
				test_SetJob(tctx, p, handle, info[j].info1.job_id, SPOOLSS_JOB_CONTROL_PAUSE);
				test_SetJob(tctx, p, handle, info[j].info1.job_id, SPOOLSS_JOB_CONTROL_RESUME);
			}
		}

	} else {
		torture_assert_werr_ok(tctx, r.out.result, "EnumJobs failed");
	}

	return true;
}

static bool test_DoPrintTest(struct torture_context *tctx,
			     struct dcerpc_pipe *p,
			     struct policy_handle *handle)
{
	bool ret = true;
	NTSTATUS status;
	struct spoolss_StartDocPrinter s;
	struct spoolss_DocumentInfo1 info1;
	struct spoolss_StartPagePrinter sp;
	struct spoolss_WritePrinter w;
	struct spoolss_EndPagePrinter ep;
	struct spoolss_EndDocPrinter e;
	int i;
	uint32_t job_id;
	uint32_t num_written;

	torture_comment(tctx, "Testing StartDocPrinter\n");

	s.in.handle		= handle;
	s.in.level		= 1;
	s.in.info.info1		= &info1;
	s.out.job_id		= &job_id;
	info1.document_name	= "TorturePrintJob";
	info1.output_file	= NULL;
	info1.datatype		= "RAW";

	status = dcerpc_spoolss_StartDocPrinter(p, tctx, &s);
	torture_assert_ntstatus_ok(tctx, status, "dcerpc_spoolss_StartDocPrinter failed");
	torture_assert_werr_ok(tctx, s.out.result, "StartDocPrinter failed");

	for (i=1; i < 4; i++) {
		torture_comment(tctx, "Testing StartPagePrinter: Page[%d]\n", i);

		sp.in.handle		= handle;

		status = dcerpc_spoolss_StartPagePrinter(p, tctx, &sp);
		torture_assert_ntstatus_ok(tctx, status,
					   "dcerpc_spoolss_StartPagePrinter failed");
		torture_assert_werr_ok(tctx, sp.out.result, "StartPagePrinter failed");

		torture_comment(tctx, "Testing WritePrinter: Page[%d]\n", i);

		w.in.handle		= handle;
		w.in.data		= data_blob_string_const(talloc_asprintf(tctx,"TortureTestPage: %d\nData\n",i));
		w.out.num_written	= &num_written;

		status = dcerpc_spoolss_WritePrinter(p, tctx, &w);
		torture_assert_ntstatus_ok(tctx, status, "dcerpc_spoolss_WritePrinter failed");
		torture_assert_werr_ok(tctx, w.out.result, "WritePrinter failed");

		torture_comment(tctx, "Testing EndPagePrinter: Page[%d]\n", i);

		ep.in.handle		= handle;

		status = dcerpc_spoolss_EndPagePrinter(p, tctx, &ep);
		torture_assert_ntstatus_ok(tctx, status, "dcerpc_spoolss_EndPagePrinter failed");
		torture_assert_werr_ok(tctx, ep.out.result, "EndPagePrinter failed");
	}

	torture_comment(tctx, "Testing EndDocPrinter\n");

	e.in.handle = handle;

	status = dcerpc_spoolss_EndDocPrinter(p, tctx, &e);
	torture_assert_ntstatus_ok(tctx, status, "dcerpc_spoolss_EndDocPrinter failed");
	torture_assert_werr_ok(tctx, e.out.result, "EndDocPrinter failed");

	ret &= test_AddJob(tctx, p, handle);
	ret &= test_EnumJobs(tctx, p, handle);

	ret &= test_SetJob(tctx, p, handle, job_id, SPOOLSS_JOB_CONTROL_DELETE);

	return ret;
}

static bool test_PausePrinter(struct torture_context *tctx,
			      struct dcerpc_pipe *p,
			      struct policy_handle *handle)
{
	NTSTATUS status;
	struct spoolss_SetPrinter r;
	struct spoolss_SetPrinterInfoCtr info_ctr;
	struct spoolss_DevmodeContainer devmode_ctr;
	struct sec_desc_buf secdesc_ctr;

	info_ctr.level = 0;
	info_ctr.info.info0 = NULL;

	ZERO_STRUCT(devmode_ctr);
	ZERO_STRUCT(secdesc_ctr);

	r.in.handle		= handle;
	r.in.info_ctr		= &info_ctr;
	r.in.devmode_ctr	= &devmode_ctr;
	r.in.secdesc_ctr	= &secdesc_ctr;
	r.in.command		= SPOOLSS_PRINTER_CONTROL_PAUSE;

	torture_comment(tctx, "Testing SetPrinter: SPOOLSS_PRINTER_CONTROL_PAUSE\n");

	status = dcerpc_spoolss_SetPrinter(p, tctx, &r);

	torture_assert_ntstatus_ok(tctx, status, "SetPrinter failed");

	torture_assert_werr_ok(tctx, r.out.result, "SetPrinter failed");

	return true;
}

static bool test_ResumePrinter(struct torture_context *tctx,
			       struct dcerpc_pipe *p,
			       struct policy_handle *handle)
{
	NTSTATUS status;
	struct spoolss_SetPrinter r;
	struct spoolss_SetPrinterInfoCtr info_ctr;
	struct spoolss_DevmodeContainer devmode_ctr;
	struct sec_desc_buf secdesc_ctr;

	info_ctr.level = 0;
	info_ctr.info.info0 = NULL;

	ZERO_STRUCT(devmode_ctr);
	ZERO_STRUCT(secdesc_ctr);

	r.in.handle		= handle;
	r.in.info_ctr		= &info_ctr;
	r.in.devmode_ctr	= &devmode_ctr;
	r.in.secdesc_ctr	= &secdesc_ctr;
	r.in.command		= SPOOLSS_PRINTER_CONTROL_RESUME;

	torture_comment(tctx, "Testing SetPrinter: SPOOLSS_PRINTER_CONTROL_RESUME\n");

	status = dcerpc_spoolss_SetPrinter(p, tctx, &r);

	torture_assert_ntstatus_ok(tctx, status, "SetPrinter failed");

	torture_assert_werr_ok(tctx, r.out.result, "SetPrinter failed");

	return true;
}

static bool test_GetPrinterData(struct torture_context *tctx,
				struct dcerpc_pipe *p,
				struct policy_handle *handle,
				const char *value_name,
				enum winreg_Type *type_p,
				uint8_t **data_p,
				uint32_t *needed_p)
{
	NTSTATUS status;
	struct spoolss_GetPrinterData r;
	uint32_t needed;
	enum winreg_Type type;
	union spoolss_PrinterData data;

	r.in.handle = handle;
	r.in.value_name = value_name;
	r.in.offered = 0;
	r.out.needed = &needed;
	r.out.type = &type;
	r.out.data = talloc_zero_array(tctx, uint8_t, r.in.offered);

	torture_comment(tctx, "Testing GetPrinterData(%s)\n", r.in.value_name);

	status = dcerpc_spoolss_GetPrinterData(p, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "GetPrinterData failed");

	if (W_ERROR_EQUAL(r.out.result, WERR_MORE_DATA)) {
		r.in.offered = needed;
		r.out.data = talloc_zero_array(tctx, uint8_t, r.in.offered);
		status = dcerpc_spoolss_GetPrinterData(p, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "GetPrinterData failed");
	}

	torture_assert_werr_ok(tctx, r.out.result,
		talloc_asprintf(tctx, "GetPrinterData(%s) failed", r.in.value_name));

	CHECK_NEEDED_SIZE_LEVEL(spoolss_PrinterData, &data, type, lp_iconv_convenience(tctx->lp_ctx), needed, 1);

	if (type_p) {
		*type_p = type;
	}

	if (data_p) {
		*data_p = r.out.data;
	}

	if (needed_p) {
		*needed_p = needed;
	}

	return true;
}

static bool test_GetPrinterDataEx(struct torture_context *tctx,
				  struct dcerpc_pipe *p,
				  struct policy_handle *handle,
				  const char *key_name,
				  const char *value_name,
				  enum winreg_Type *type_p,
				  uint8_t **data_p,
				  uint32_t *needed_p)
{
	NTSTATUS status;
	struct spoolss_GetPrinterDataEx r;
	enum winreg_Type type;
	uint32_t needed;
	union spoolss_PrinterData data;

	r.in.handle = handle;
	r.in.key_name = key_name;
	r.in.value_name = value_name;
	r.in.offered = 0;
	r.out.type = &type;
	r.out.needed = &needed;
	r.out.data = talloc_zero_array(tctx, uint8_t, r.in.offered);

	torture_comment(tctx, "Testing GetPrinterDataEx(%s - %s)\n",
		r.in.key_name, r.in.value_name);

	status = dcerpc_spoolss_GetPrinterDataEx(p, tctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		if (NT_STATUS_EQUAL(status,NT_STATUS_NET_WRITE_FAULT) &&
		    p->last_fault_code == DCERPC_FAULT_OP_RNG_ERROR) {
			torture_skip(tctx, "GetPrinterDataEx not supported by server\n");
		}
		torture_assert_ntstatus_ok(tctx, status, "GetPrinterDataEx failed");
	}

	if (W_ERROR_EQUAL(r.out.result, WERR_MORE_DATA)) {
		r.in.offered = needed;
		r.out.data = talloc_zero_array(tctx, uint8_t, r.in.offered);
		status = dcerpc_spoolss_GetPrinterDataEx(p, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "GetPrinterDataEx failed");
	}

	torture_assert_werr_ok(tctx, r.out.result,
		talloc_asprintf(tctx, "GetPrinterDataEx(%s - %s) failed", r.in.key_name, r.in.value_name));

	CHECK_NEEDED_SIZE_LEVEL(spoolss_PrinterData, &data, type, lp_iconv_convenience(tctx->lp_ctx), needed, 1);

	if (type_p) {
		*type_p = type;
	}

	if (data_p) {
		*data_p = r.out.data;
	}

	if (needed_p) {
		*needed_p = needed;
	}

	return true;
}

static bool test_GetPrinterData_list(struct torture_context *tctx,
				     struct dcerpc_pipe *p,
				     struct policy_handle *handle)
{
	const char *list[] = {
		"W3SvcInstalled",
		"BeepEnabled",
		"EventLog",
		/* "NetPopup", not on w2k8 */
		/* "NetPopupToComputer", not on w2k8 */
		"MajorVersion",
		"MinorVersion",
		"DefaultSpoolDirectory",
		"Architecture",
		"DsPresent",
		"OSVersion",
		/* "OSVersionEx", not on s3 */
		"DNSMachineName"
	};
	int i;

	for (i=0; i < ARRAY_SIZE(list); i++) {
		enum winreg_Type type, type_ex;
		uint8_t *data, *data_ex;
		uint32_t needed, needed_ex;

		torture_assert(tctx, test_GetPrinterData(tctx, p, handle, list[i], &type, &data, &needed),
			talloc_asprintf(tctx, "GetPrinterData failed on %s\n", list[i]));
		torture_assert(tctx, test_GetPrinterDataEx(tctx, p, handle, "random_string", list[i], &type_ex, &data_ex, &needed_ex),
			talloc_asprintf(tctx, "GetPrinterDataEx failed on %s\n", list[i]));
		torture_assert_int_equal(tctx, type, type_ex, "type mismatch");
		torture_assert_int_equal(tctx, needed, needed_ex, "needed mismatch");
		torture_assert_mem_equal(tctx, data, data_ex, needed, "data mismatch");
	}

	return true;
}

static bool test_EnumPrinterData(struct torture_context *tctx, struct dcerpc_pipe *p,
				 struct policy_handle *handle)
{
	NTSTATUS status;
	struct spoolss_EnumPrinterData r;

	ZERO_STRUCT(r);
	r.in.handle = handle;
	r.in.enum_index = 0;

	do {
		uint32_t value_size = 0;
		uint32_t data_size = 0;
		enum winreg_Type type = 0;

		r.in.value_offered = value_size;
		r.out.value_needed = &value_size;
		r.in.data_offered = data_size;
		r.out.data_needed = &data_size;

		r.out.type = &type;
		r.out.data = talloc_zero_array(tctx, uint8_t, 0);

		torture_comment(tctx, "Testing EnumPrinterData\n");

		status = dcerpc_spoolss_EnumPrinterData(p, tctx, &r);

		torture_assert_ntstatus_ok(tctx, status, "EnumPrinterData failed");
		if (W_ERROR_EQUAL(r.out.result, WERR_NO_MORE_ITEMS)) {
			break;
		}
		torture_assert_werr_ok(tctx, r.out.result, "EnumPrinterData");

		r.in.value_offered = value_size;
		r.out.value_name = talloc_zero_array(tctx, const char, value_size);
		r.in.data_offered = data_size;
		r.out.data = talloc_zero_array(tctx, uint8_t, data_size);

		status = dcerpc_spoolss_EnumPrinterData(p, tctx, &r);

		torture_assert_ntstatus_ok(tctx, status, "EnumPrinterData failed");
		if (W_ERROR_EQUAL(r.out.result, WERR_NO_MORE_ITEMS)) {
			break;
		}

		torture_assert_werr_ok(tctx, r.out.result, "EnumPrinterData failed");

		torture_assert(tctx, test_GetPrinterData(tctx, p, handle, r.out.value_name, NULL, NULL, NULL),
			talloc_asprintf(tctx, "failed to call GetPrinterData for %s\n", r.out.value_name));

		torture_assert(tctx, test_GetPrinterDataEx(tctx, p, handle, "PrinterDriverData", r.out.value_name, NULL, NULL, NULL),
			talloc_asprintf(tctx, "failed to call GetPrinterDataEx on PrinterDriverData for %s\n", r.out.value_name));

		r.in.enum_index++;

	} while (W_ERROR_IS_OK(r.out.result));

	return true;
}

static bool test_EnumPrinterDataEx(struct torture_context *tctx,
				   struct dcerpc_pipe *p,
				   struct policy_handle *handle,
				   const char *key_name)
{
	struct spoolss_EnumPrinterDataEx r;
	struct spoolss_PrinterEnumValues *info;
	uint32_t needed;
	uint32_t count;

	r.in.handle = handle;
	r.in.key_name = key_name;
	r.in.offered = 0;
	r.out.needed = &needed;
	r.out.count = &count;
	r.out.info = &info;

	torture_comment(tctx, "Testing EnumPrinterDataEx(%s)\n", key_name);

	torture_assert_ntstatus_ok(tctx, dcerpc_spoolss_EnumPrinterDataEx(p, tctx, &r),
		"EnumPrinterDataEx failed");
	if (W_ERROR_EQUAL(r.out.result, WERR_MORE_DATA)) {
		r.in.offered = needed;
		torture_assert_ntstatus_ok(tctx, dcerpc_spoolss_EnumPrinterDataEx(p, tctx, &r),
			"EnumPrinterDataEx failed");
	}

	torture_assert_werr_ok(tctx, r.out.result, "EnumPrinterDataEx failed");

	CHECK_NEEDED_SIZE_ENUM(spoolss_EnumPrinterDataEx, info, count, lp_iconv_convenience(tctx->lp_ctx), needed, 1);

	return true;
}


static bool test_DeletePrinterData(struct torture_context *tctx,
				   struct dcerpc_pipe *p,
				   struct policy_handle *handle,
				   const char *value_name)
{
	NTSTATUS status;
	struct spoolss_DeletePrinterData r;

	r.in.handle = handle;
	r.in.value_name = value_name;

	torture_comment(tctx, "Testing DeletePrinterData(%s)\n",
		r.in.value_name);

	status = dcerpc_spoolss_DeletePrinterData(p, tctx, &r);

	torture_assert_ntstatus_ok(tctx, status, "DeletePrinterData failed");
	torture_assert_werr_ok(tctx, r.out.result, "DeletePrinterData failed");

	return true;
}

static bool test_DeletePrinterDataEx(struct torture_context *tctx,
				     struct dcerpc_pipe *p,
				     struct policy_handle *handle,
				     const char *key_name,
				     const char *value_name)
{
	struct spoolss_DeletePrinterDataEx r;

	r.in.handle = handle;
	r.in.key_name = key_name;
	r.in.value_name = value_name;

	torture_comment(tctx, "Testing DeletePrinterDataEx(%s - %s)\n",
		r.in.key_name, r.in.value_name);

	torture_assert_ntstatus_ok(tctx,
		dcerpc_spoolss_DeletePrinterDataEx(p, tctx, &r),
		"DeletePrinterDataEx failed");
	torture_assert_werr_ok(tctx, r.out.result,
		"DeletePrinterDataEx failed");

	return true;
}

static bool test_DeletePrinterKey(struct torture_context *tctx,
				  struct dcerpc_pipe *p,
				  struct policy_handle *handle,
				  const char *key_name)
{
	struct spoolss_DeletePrinterKey r;

	r.in.handle = handle;
	r.in.key_name = key_name;

	torture_comment(tctx, "Testing DeletePrinterKey(%s)\n", r.in.key_name);

	if (strequal(key_name, "") && !torture_setting_bool(tctx, "dangerous", false)) {
		torture_skip(tctx, "not wiping out printer registry - enable dangerous tests to use\n");
		return true;
	}

	torture_assert_ntstatus_ok(tctx,
		dcerpc_spoolss_DeletePrinterKey(p, tctx, &r),
		"DeletePrinterKey failed");
	torture_assert_werr_ok(tctx, r.out.result,
		"DeletePrinterKey failed");

	return true;
}

static bool test_SetPrinterData(struct torture_context *tctx,
				struct dcerpc_pipe *p,
				struct policy_handle *handle)
{
	NTSTATUS status;
	struct spoolss_SetPrinterData r;
	const char *values[] = {
		"spootyfoot",
		"spooty\\foot",
#if 0
	/* FIXME: not working with s3 atm. */
		"spooty,foot",
		"spooty,fo,ot",
#endif
		"spooty foot",
#if 0
	/* FIXME: not working with s3 atm. */
		"spooty\\fo,ot",
		"spooty,fo\\ot"
#endif
	};
	int i;

	for (i=0; i < ARRAY_SIZE(values); i++) {

		enum winreg_Type type;
		uint8_t *data;
		DATA_BLOB blob;
		uint32_t needed;

		torture_assert(tctx,
			reg_string_to_val(tctx, lp_iconv_convenience(tctx->lp_ctx),
					  "REG_SZ", "dog", &r.in.type, &blob), "");

		r.in.handle = handle;
		r.in.value_name = values[i];
		r.in.type = REG_SZ;
		r.in.data = blob.data;
		r.in.offered = blob.length;

		torture_comment(tctx, "Testing SetPrinterData(%s)\n",
			r.in.value_name);

		status = dcerpc_spoolss_SetPrinterData(p, tctx, &r);

		torture_assert_ntstatus_ok(tctx, status, "SetPrinterData failed");
		torture_assert_werr_ok(tctx, r.out.result, "SetPrinterData failed");

		if (!test_GetPrinterData(tctx, p, handle, r.in.value_name, &type, &data, &needed)) {
			return false;
		}

		torture_assert_int_equal(tctx, r.in.type, type, "type mismatch");
		torture_assert_int_equal(tctx, r.in.offered, needed, "size mismatch");
		torture_assert_mem_equal(tctx, blob.data, data, needed, "buffer mismatch");

		if (!test_DeletePrinterData(tctx, p, handle, r.in.value_name)) {
			return false;
		}
	}

	return true;
}

static bool test_EnumPrinterKey(struct torture_context *tctx,
				struct dcerpc_pipe *p,
				struct policy_handle *handle,
				const char *key_name,
				const char ***array);

static bool test_SetPrinterDataEx(struct torture_context *tctx,
				  struct dcerpc_pipe *p,
				  struct policy_handle *handle)
{
	NTSTATUS status;
	struct spoolss_SetPrinterDataEx r;
	const char *value_name = "dog";
	const char *keys[] = {
		"torturedataex",
		"torture data ex",
#if 0
	/* FIXME: not working with s3 atm. */
		"torturedataex_with_subkey\\subkey",
		"torturedataex_with_subkey\\subkey:0",
		"torturedataex_with_subkey\\subkey:1",
		"torturedataex_with_subkey\\subkey\\subsubkey",
		"torturedataex_with_subkey\\subkey\\subsubkey:0",
		"torturedataex_with_subkey\\subkey\\subsubkey:1",
#endif
		"torture,data",
#if 0
	/* FIXME: not working with s3 atm. */

		"torture,data,ex",
		"torture,data\\ex",
		"torture\\data,ex"
#endif
	};
	int i;
	DATA_BLOB blob = data_blob_string_const("catfoobar");


	for (i=0; i < ARRAY_SIZE(keys); i++) {

		char *c;
		const char *key;
		enum winreg_Type type;
		const char **subkeys;
		uint8_t *data_out;
		uint32_t needed;

		r.in.handle = handle;
		r.in.key_name = keys[i];
		r.in.value_name = value_name;
		r.in.type = REG_BINARY;
		r.in.data = blob.data;
		r.in.offered = blob.length;

		torture_comment(tctx, "Testing SetPrinterDataEx(%s - %s)\n", r.in.key_name, value_name);

		status = dcerpc_spoolss_SetPrinterDataEx(p, tctx, &r);

		torture_assert_ntstatus_ok(tctx, status, "SetPrinterDataEx failed");
		torture_assert_werr_ok(tctx, r.out.result, "SetPrinterDataEx failed");

		if (!test_GetPrinterDataEx(tctx, p, handle, keys[i], value_name, &type, &data_out, &needed)) {
			return false;
		}

		torture_assert_int_equal(tctx, type, REG_BINARY, "type mismatch");
		torture_assert_int_equal(tctx, needed, blob.length, "size mismatch");
		torture_assert_mem_equal(tctx, data_out, blob.data, blob.length, "buffer mismatch");

		key = talloc_strdup(tctx, keys[i]);

		if (!test_EnumPrinterDataEx(tctx, p, handle, r.in.key_name)) {
			return false;
		}

		if (!test_DeletePrinterDataEx(tctx, p, handle, r.in.key_name, value_name)) {
			return false;
		}

		c = strchr(key, '\\');
		if (c) {
			int i;

			/* we have subkeys */

			*c = 0;

			if (!test_EnumPrinterKey(tctx, p, handle, key, &subkeys)) {
				return false;
			}

			for (i=0; subkeys && subkeys[i]; i++) {

				const char *current_key = talloc_asprintf(tctx, "%s\\%s", key, subkeys[i]);

				if (!test_DeletePrinterKey(tctx, p, handle, current_key)) {
					return false;
				}
			}

			if (!test_DeletePrinterKey(tctx, p, handle, key)) {
				return false;
			}

		} else {
			if (!test_DeletePrinterKey(tctx, p, handle, key)) {
				return false;
			}
		}
	}

	return true;
}

static bool test_GetChangeID_PrinterData(struct torture_context *tctx,
					 struct dcerpc_pipe *p,
					 struct policy_handle *handle,
					 uint32_t *change_id)
{
	enum winreg_Type type;
	uint8_t *data;
	uint32_t needed;

	torture_assert(tctx,
		test_GetPrinterData(tctx, p, handle, "ChangeID", &type, &data, &needed),
		"failed to call GetPrinterData");

	torture_assert(tctx, type == REG_DWORD, "unexpected type");
	torture_assert_int_equal(tctx, needed, 4, "unexpected size");

	*change_id = IVAL(data, 0);

	return true;
}

static bool test_GetChangeID_PrinterDataEx(struct torture_context *tctx,
					   struct dcerpc_pipe *p,
					   struct policy_handle *handle,
					   uint32_t *change_id)
{
	enum winreg_Type type;
	uint8_t *data;
	uint32_t needed;

	torture_assert(tctx,
		test_GetPrinterDataEx(tctx, p, handle, "PrinterDriverData", "ChangeID", &type, &data, &needed),
		"failed to call GetPrinterData");

	torture_assert(tctx, type == REG_DWORD, "unexpected type");
	torture_assert_int_equal(tctx, needed, 4, "unexpected size");

	*change_id = IVAL(data, 0);

	return true;
}

static bool test_GetChangeID_PrinterInfo(struct torture_context *tctx,
					 struct dcerpc_pipe *p,
					 struct policy_handle *handle,
					 uint32_t *change_id)
{
	union spoolss_PrinterInfo info;

	torture_assert(tctx, test_GetPrinter_level(tctx, p, handle, 0, &info),
		"failed to query Printer level 0");

	*change_id = info.info0.change_id;

	return true;
}

static bool test_ChangeID(struct torture_context *tctx,
			  struct dcerpc_pipe *p,
			  struct policy_handle *handle)
{
	uint32_t change_id, change_id_ex, change_id_info;
	uint32_t change_id2, change_id_ex2, change_id_info2;
	union spoolss_PrinterInfo info;
	const char *comment;


	torture_comment(tctx, "Testing ChangeID: id change test #1\n");

	torture_assert(tctx, test_GetChangeID_PrinterData(tctx, p, handle, &change_id),
		"failed to query for ChangeID");
	torture_assert(tctx, test_GetChangeID_PrinterDataEx(tctx, p, handle, &change_id_ex),
		"failed to query for ChangeID");
	torture_assert(tctx, test_GetChangeID_PrinterInfo(tctx, p, handle, &change_id_info),
		"failed to query for ChangeID");

	torture_assert_int_equal(tctx, change_id, change_id_ex,
		"change_ids should all be equal");
	torture_assert_int_equal(tctx, change_id_ex, change_id_info,
		"change_ids should all be equal");


	torture_comment(tctx, "Testing ChangeID: id change test #2\n");

	torture_assert(tctx, test_GetChangeID_PrinterData(tctx, p, handle, &change_id),
		"failed to query for ChangeID");
	torture_assert(tctx, test_GetPrinter_level(tctx, p, handle, 2, &info),
		"failed to query Printer level 2");
	torture_assert(tctx, test_GetChangeID_PrinterDataEx(tctx, p, handle, &change_id_ex),
		"failed to query for ChangeID");
	torture_assert(tctx, test_GetChangeID_PrinterInfo(tctx, p, handle, &change_id_info),
		"failed to query for ChangeID");
	torture_assert_int_equal(tctx, change_id, change_id_ex,
		"change_id should not have changed");
	torture_assert_int_equal(tctx, change_id_ex, change_id_info,
		"change_id should not have changed");


	torture_comment(tctx, "Testing ChangeID: id change test #3\n");

	torture_assert(tctx, test_GetChangeID_PrinterData(tctx, p, handle, &change_id),
		"failed to query for ChangeID");
	torture_assert(tctx, test_GetChangeID_PrinterDataEx(tctx, p, handle, &change_id_ex),
		"failed to query for ChangeID");
	torture_assert(tctx, test_GetChangeID_PrinterInfo(tctx, p, handle, &change_id_info),
		"failed to query for ChangeID");
	torture_assert(tctx, test_GetPrinter_level(tctx, p, handle, 2, &info),
		"failed to query Printer level 2");
	comment = talloc_strdup(tctx, info.info2.comment);

	{
		struct spoolss_SetPrinterInfoCtr info_ctr;
		struct spoolss_DevmodeContainer devmode_ctr;
		struct sec_desc_buf secdesc_ctr;
		struct spoolss_SetPrinterInfo2 info2;

		ZERO_STRUCT(info_ctr);
		ZERO_STRUCT(devmode_ctr);
		ZERO_STRUCT(secdesc_ctr);

		info2.servername	= info.info2.servername;
		info2.printername	= info.info2.printername;
		info2.sharename		= info.info2.sharename;
		info2.portname		= info.info2.portname;
		info2.drivername	= info.info2.drivername;
		info2.comment		= "torture_comment";
		info2.location		= info.info2.location;
		info2.devmode_ptr	= 0;
		info2.sepfile		= info.info2.sepfile;
		info2.printprocessor	= info.info2.printprocessor;
		info2.datatype		= info.info2.datatype;
		info2.parameters	= info.info2.parameters;
		info2.secdesc_ptr	= 0;
		info2.attributes	= info.info2.attributes;
		info2.priority		= info.info2.priority;
		info2.defaultpriority	= info.info2.defaultpriority;
		info2.starttime		= info.info2.starttime;
		info2.untiltime		= info.info2.untiltime;
		info2.status		= info.info2.status;
		info2.cjobs		= info.info2.cjobs;
		info2.averageppm	= info.info2.averageppm;

		info_ctr.level = 2;
		info_ctr.info.info2 = &info2;

		torture_assert(tctx, test_SetPrinter(tctx, p, handle, &info_ctr, &devmode_ctr, &secdesc_ctr, 0),
			"failed to call SetPrinter");

		info2.comment		= comment;

		torture_assert(tctx, test_SetPrinter(tctx, p, handle, &info_ctr, &devmode_ctr, &secdesc_ctr, 0),
			"failed to call SetPrinter");

	}

	torture_assert(tctx, test_GetChangeID_PrinterData(tctx, p, handle, &change_id2),
		"failed to query for ChangeID");
	torture_assert(tctx, test_GetChangeID_PrinterDataEx(tctx, p, handle, &change_id_ex2),
		"failed to query for ChangeID");
	torture_assert(tctx, test_GetChangeID_PrinterInfo(tctx, p, handle, &change_id_info2),
		"failed to query for ChangeID");

	torture_assert_int_equal(tctx, change_id2, change_id_ex2,
		"change_ids should all be equal");
	torture_assert_int_equal(tctx, change_id_ex2, change_id_info2,
		"change_ids should all be equal");

	torture_assert(tctx, (change_id < change_id2),
		talloc_asprintf(tctx, "change_id %d needs to be larger than change_id %d",
		change_id2, change_id));
	torture_assert(tctx, (change_id_ex < change_id_ex2),
		talloc_asprintf(tctx, "change_id %d needs to be larger than change_id %d",
		change_id_ex2, change_id_ex));
	torture_assert(tctx, (change_id_info < change_id_info2),
		talloc_asprintf(tctx, "change_id %d needs to be larger than change_id %d",
		change_id_info2, change_id_info));

	return true;
}

static bool test_SecondaryClosePrinter(struct torture_context *tctx,
				       struct dcerpc_pipe *p,
				       struct policy_handle *handle)
{
	NTSTATUS status;
	struct dcerpc_binding *b;
	struct dcerpc_pipe *p2;
	struct spoolss_ClosePrinter cp;

	/* only makes sense on SMB */
	if (p->conn->transport.transport != NCACN_NP) {
		return true;
	}

	torture_comment(tctx, "testing close on secondary pipe\n");

	status = dcerpc_parse_binding(tctx, p->conn->binding_string, &b);
	torture_assert_ntstatus_ok(tctx, status, "Failed to parse dcerpc binding");

	status = dcerpc_secondary_connection(p, &p2, b);
	torture_assert_ntstatus_ok(tctx, status, "Failed to create secondary connection");

	status = dcerpc_bind_auth_none(p2, &ndr_table_spoolss);
	torture_assert_ntstatus_ok(tctx, status, "Failed to create bind on secondary connection");

	cp.in.handle = handle;
	cp.out.handle = handle;

	status = dcerpc_spoolss_ClosePrinter(p2, tctx, &cp);
	torture_assert_ntstatus_equal(tctx, status, NT_STATUS_NET_WRITE_FAULT,
			"ERROR: Allowed close on secondary connection");

	torture_assert_int_equal(tctx, p2->last_fault_code, DCERPC_FAULT_CONTEXT_MISMATCH,
				 "Unexpected fault code");

	talloc_free(p2);

	return true;
}

static bool test_OpenPrinter_badname(struct torture_context *tctx,
				     struct dcerpc_pipe *p, const char *name)
{
	NTSTATUS status;
	struct spoolss_OpenPrinter op;
	struct spoolss_OpenPrinterEx opEx;
	struct policy_handle handle;
	bool ret = true;

	op.in.printername	= name;
	op.in.datatype		= NULL;
	op.in.devmode_ctr.devmode= NULL;
	op.in.access_mask	= 0;
	op.out.handle		= &handle;

	torture_comment(tctx, "\nTesting OpenPrinter(%s) with bad name\n", op.in.printername);

	status = dcerpc_spoolss_OpenPrinter(p, tctx, &op);
	torture_assert_ntstatus_ok(tctx, status, "OpenPrinter failed");
	if (!W_ERROR_EQUAL(WERR_INVALID_PRINTER_NAME,op.out.result)) {
		torture_comment(tctx, "OpenPrinter(%s) unexpected result[%s] should be WERR_INVALID_PRINTER_NAME\n",
			name, win_errstr(op.out.result));
	}

	if (W_ERROR_IS_OK(op.out.result)) {
		ret &=test_ClosePrinter(tctx, p, &handle);
	}

	opEx.in.printername		= name;
	opEx.in.datatype		= NULL;
	opEx.in.devmode_ctr.devmode	= NULL;
	opEx.in.access_mask		= 0;
	opEx.in.level			= 1;
	opEx.in.userlevel.level1	= NULL;
	opEx.out.handle			= &handle;

	torture_comment(tctx, "Testing OpenPrinterEx(%s) with bad name\n", opEx.in.printername);

	status = dcerpc_spoolss_OpenPrinterEx(p, tctx, &opEx);
	torture_assert_ntstatus_ok(tctx, status, "OpenPrinterEx failed");
	if (!W_ERROR_EQUAL(WERR_INVALID_PARAM,opEx.out.result)) {
		torture_comment(tctx, "OpenPrinterEx(%s) unexpected result[%s] should be WERR_INVALID_PARAM\n",
			name, win_errstr(opEx.out.result));
	}

	if (W_ERROR_IS_OK(opEx.out.result)) {
		ret &=test_ClosePrinter(tctx, p, &handle);
	}

	return ret;
}

static bool test_OpenPrinter(struct torture_context *tctx,
			     struct dcerpc_pipe *p,
			     const char *name)
{
	NTSTATUS status;
	struct spoolss_OpenPrinter r;
	struct policy_handle handle;
	bool ret = true;

	r.in.printername	= talloc_asprintf(tctx, "\\\\%s\\%s", dcerpc_server_name(p), name);
	r.in.datatype		= NULL;
	r.in.devmode_ctr.devmode= NULL;
	r.in.access_mask	= SEC_FLAG_MAXIMUM_ALLOWED;
	r.out.handle		= &handle;

	torture_comment(tctx, "Testing OpenPrinter(%s)\n", r.in.printername);

	status = dcerpc_spoolss_OpenPrinter(p, tctx, &r);

	torture_assert_ntstatus_ok(tctx, status, "OpenPrinter failed");

	torture_assert_werr_ok(tctx, r.out.result, "OpenPrinter failed");

	if (!test_GetPrinter(tctx, p, &handle)) {
		ret = false;
	}

	if (!torture_setting_bool(tctx, "samba3", false)) {
		if (!test_SecondaryClosePrinter(tctx, p, &handle)) {
			ret = false;
		}
	}

	if (!test_ClosePrinter(tctx, p, &handle)) {
		ret = false;
	}

	return ret;
}

static bool call_OpenPrinterEx(struct torture_context *tctx,
			       struct dcerpc_pipe *p,
			       const char *name, struct policy_handle *handle)
{
	struct spoolss_OpenPrinterEx r;
	struct spoolss_UserLevel1 userlevel1;
	NTSTATUS status;

	if (name && name[0]) {
		r.in.printername = talloc_asprintf(tctx, "\\\\%s\\%s",
						   dcerpc_server_name(p), name);
	} else {
		r.in.printername = talloc_asprintf(tctx, "\\\\%s",
						   dcerpc_server_name(p));
	}

	r.in.datatype		= NULL;
	r.in.devmode_ctr.devmode= NULL;
	r.in.access_mask	= SEC_FLAG_MAXIMUM_ALLOWED;
	r.in.level		= 1;
	r.in.userlevel.level1	= &userlevel1;
	r.out.handle = handle;

	userlevel1.size = 1234;
	userlevel1.client = "hello";
	userlevel1.user = "spottyfoot!";
	userlevel1.build = 1;
	userlevel1.major = 2;
	userlevel1.minor = 3;
	userlevel1.processor = 4;

	torture_comment(tctx, "Testing OpenPrinterEx(%s)\n", r.in.printername);

	status = dcerpc_spoolss_OpenPrinterEx(p, tctx, &r);

	torture_assert_ntstatus_ok(tctx, status, "OpenPrinterEx failed");

	torture_assert_werr_ok(tctx, r.out.result, "OpenPrinterEx failed");

	return true;
}

static bool test_OpenPrinterEx(struct torture_context *tctx,
			       struct dcerpc_pipe *p,
			       const char *name)
{
	struct policy_handle handle;
	bool ret = true;

	if (!call_OpenPrinterEx(tctx, p, name, &handle)) {
		return false;
	}

	if (!test_GetPrinter(tctx, p, &handle)) {
		ret = false;
	}

	if (!test_EnumForms(tctx, p, &handle, false)) {
		ret = false;
	}

	if (!test_AddForm(tctx, p, &handle, false)) {
		ret = false;
	}

	if (!test_EnumPrinterData(tctx, p, &handle)) {
		ret = false;
	}

	if (!test_EnumPrinterDataEx(tctx, p, &handle, "PrinterDriverData")) {
		ret = false;
	}

	if (!test_printer_keys(tctx, p, &handle)) {
		ret = false;
	}

	if (!test_PausePrinter(tctx, p, &handle)) {
		ret = false;
	}

	if (!test_DoPrintTest(tctx, p, &handle)) {
		ret = false;
	}

	if (!test_ResumePrinter(tctx, p, &handle)) {
		ret = false;
	}

	if (!test_SetPrinterData(tctx, p, &handle)) {
		ret = false;
	}

	if (!test_SetPrinterDataEx(tctx, p, &handle)) {
		ret = false;
	}

	if (!test_ChangeID(tctx, p, &handle)) {
		ret = false;
	}

	if (!torture_setting_bool(tctx, "samba3", false)) {
		if (!test_SecondaryClosePrinter(tctx, p, &handle)) {
			ret = false;
		}
	}

	if (!test_ClosePrinter(tctx, p, &handle)) {
		ret = false;
	}

	return ret;
}

static bool test_EnumPrinters_old(struct torture_context *tctx, struct dcerpc_pipe *p)
{
	struct spoolss_EnumPrinters r;
	NTSTATUS status;
	uint16_t levels[] = {1, 2, 4, 5};
	int i;
	bool ret = true;

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		union spoolss_PrinterInfo *info;
		int j;
		uint32_t needed;
		uint32_t count;

		r.in.flags	= PRINTER_ENUM_LOCAL;
		r.in.server	= "";
		r.in.level	= levels[i];
		r.in.buffer	= NULL;
		r.in.offered	= 0;
		r.out.needed	= &needed;
		r.out.count	= &count;
		r.out.info	= &info;

		torture_comment(tctx, "Testing EnumPrinters level %u\n", r.in.level);

		status = dcerpc_spoolss_EnumPrinters(p, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "EnumPrinters failed");

		if (W_ERROR_EQUAL(r.out.result, WERR_INSUFFICIENT_BUFFER)) {
			DATA_BLOB blob = data_blob_talloc(tctx, NULL, needed);
			data_blob_clear(&blob);
			r.in.buffer = &blob;
			r.in.offered = needed;
			status = dcerpc_spoolss_EnumPrinters(p, tctx, &r);
		}

		torture_assert_ntstatus_ok(tctx, status, "EnumPrinters failed");

		torture_assert_werr_ok(tctx, r.out.result, "EnumPrinters failed");

		CHECK_NEEDED_SIZE_ENUM_LEVEL(spoolss_EnumPrinters, info, r.in.level, count, lp_iconv_convenience(tctx->lp_ctx), needed, 4);

		if (!info) {
			torture_comment(tctx, "No printers returned\n");
			return true;
		}

		for (j=0;j<count;j++) {
			if (r.in.level == 1) {
				char *unc = talloc_strdup(tctx, info[j].info1.name);
				char *slash, *name;
				name = unc;
				if (unc[0] == '\\' && unc[1] == '\\') {
					unc +=2;
				}
				slash = strchr(unc, '\\');
				if (slash) {
					slash++;
					name = slash;
				}
				if (!test_OpenPrinter(tctx, p, name)) {
					ret = false;
				}
				if (!test_OpenPrinterEx(tctx, p, name)) {
					ret = false;
				}
			}
		}
	}

	return ret;
}

static bool test_GetPrinterDriver(struct torture_context *tctx,
				  struct dcerpc_pipe *p,
				  struct policy_handle *handle,
				  const char *driver_name)
{
	struct spoolss_GetPrinterDriver r;
	uint32_t needed;

	r.in.handle = handle;
	r.in.architecture = "W32X86";
	r.in.level = 1;
	r.in.buffer = NULL;
	r.in.offered = 0;
	r.out.needed = &needed;

	torture_comment(tctx, "Testing GetPrinterDriver level %d\n", r.in.level);

	torture_assert_ntstatus_ok(tctx, dcerpc_spoolss_GetPrinterDriver(p, tctx, &r),
		"failed to call GetPrinterDriver");
	if (W_ERROR_EQUAL(r.out.result, WERR_INSUFFICIENT_BUFFER)) {
		DATA_BLOB blob = data_blob_talloc(tctx, NULL, needed);
		data_blob_clear(&blob);
		r.in.buffer = &blob;
		r.in.offered = needed;
		torture_assert_ntstatus_ok(tctx, dcerpc_spoolss_GetPrinterDriver(p, tctx, &r),
			"failed to call GetPrinterDriver");
	}

	torture_assert_werr_ok(tctx, r.out.result,
		"failed to call GetPrinterDriver");

	CHECK_NEEDED_SIZE_LEVEL(spoolss_DriverInfo, r.out.info, r.in.level, lp_iconv_convenience(tctx->lp_ctx), needed, 4);

	return true;
}

static bool test_GetPrinterDriver2(struct torture_context *tctx,
				   struct dcerpc_pipe *p,
				   struct policy_handle *handle,
				   const char *driver_name)
{
	struct spoolss_GetPrinterDriver2 r;
	uint16_t levels[] = {1, 2, 3, 4, 5, 6, 8, 101 };
	uint32_t needed;
	uint32_t server_major_version;
	uint32_t server_minor_version;
	int i;

	r.in.handle = handle;
	r.in.architecture = SPOOLSS_ARCHITECTURE_NT_X86;
	r.in.client_major_version = 3;
	r.in.client_minor_version = 0;
	r.out.needed = &needed;
	r.out.server_major_version = &server_major_version;
	r.out.server_minor_version = &server_minor_version;

	for (i=0;i<ARRAY_SIZE(levels);i++) {

		r.in.buffer = NULL;
		r.in.offered = 0;
		r.in.level = levels[i];

		torture_comment(tctx, "Testing GetPrinterDriver2(%s) level %d\n",
			driver_name, r.in.level);

		torture_assert_ntstatus_ok(tctx, dcerpc_spoolss_GetPrinterDriver2(p, tctx, &r),
			"failed to call GetPrinterDriver2");
		if (W_ERROR_EQUAL(r.out.result, WERR_INSUFFICIENT_BUFFER)) {
			DATA_BLOB blob = data_blob_talloc(tctx, NULL, needed);
			data_blob_clear(&blob);
			r.in.buffer = &blob;
			r.in.offered = needed;
			torture_assert_ntstatus_ok(tctx, dcerpc_spoolss_GetPrinterDriver2(p, tctx, &r),
				"failed to call GetPrinterDriver2");
		}

		if (W_ERROR_EQUAL(r.out.result, WERR_INVALID_LEVEL)) {
			switch (r.in.level) {
			case 101:
			case 8:
				continue;
			default:
				break;
			}
		}

		torture_assert_werr_ok(tctx, r.out.result,
			"failed to call GetPrinterDriver2");

		CHECK_NEEDED_SIZE_LEVEL(spoolss_DriverInfo, r.out.info, r.in.level, lp_iconv_convenience(tctx->lp_ctx), needed, 4);
	}

	return true;
}

static bool test_EnumPrinterDrivers_old(struct torture_context *tctx,
					struct dcerpc_pipe *p)
{
	struct spoolss_EnumPrinterDrivers r;
	NTSTATUS status;
	uint16_t levels[] = {1, 2, 3, 4, 5, 6};
	int i;

	for (i=0;i<ARRAY_SIZE(levels);i++) {

		uint32_t needed;
		uint32_t count;
		union spoolss_DriverInfo *info;

		r.in.server = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
		r.in.environment = SPOOLSS_ARCHITECTURE_NT_X86;
		r.in.level = levels[i];
		r.in.buffer = NULL;
		r.in.offered = 0;
		r.out.needed = &needed;
		r.out.count = &count;
		r.out.info = &info;

		torture_comment(tctx, "Testing EnumPrinterDrivers level %u\n", r.in.level);

		status = dcerpc_spoolss_EnumPrinterDrivers(p, tctx, &r);

		torture_assert_ntstatus_ok(tctx, status, "EnumPrinterDrivers failed");

		if (W_ERROR_EQUAL(r.out.result, WERR_INSUFFICIENT_BUFFER)) {
			DATA_BLOB blob = data_blob_talloc(tctx, NULL, needed);
			data_blob_clear(&blob);
			r.in.buffer = &blob;
			r.in.offered = needed;
			status = dcerpc_spoolss_EnumPrinterDrivers(p, tctx, &r);
		}

		torture_assert_ntstatus_ok(tctx, status, "EnumPrinterDrivers failed");

		torture_assert_werr_ok(tctx, r.out.result, "EnumPrinterDrivers failed");

		if (!info) {
			torture_comment(tctx, "No printer drivers returned\n");
			break;
		}

		CHECK_NEEDED_SIZE_ENUM_LEVEL(spoolss_EnumPrinterDrivers, info, r.in.level, count, lp_iconv_convenience(tctx->lp_ctx), needed, 4);
	}

	return true;
}

static bool test_DeletePrinter(struct torture_context *tctx,
			       struct dcerpc_pipe *p,
			       struct policy_handle *handle)
{
	struct spoolss_DeletePrinter r;

	torture_comment(tctx, "Testing DeletePrinter\n");

	r.in.handle = handle;

	torture_assert_ntstatus_ok(tctx, dcerpc_spoolss_DeletePrinter(p, tctx, &r),
		"failed to delete printer");
	torture_assert_werr_ok(tctx, r.out.result,
		"failed to delete printer");

	return true;
}

static bool test_EnumPrinters_findname(struct torture_context *tctx,
				       struct dcerpc_pipe *p,
				       uint32_t flags,
				       uint32_t level,
				       const char *name,
				       bool *found)
{
	struct spoolss_EnumPrinters e;
	uint32_t count;
	union spoolss_PrinterInfo *info;
	uint32_t needed;
	int i;

	*found = false;

	e.in.flags = flags;
	e.in.server = NULL;
	e.in.level = level;
	e.in.buffer = NULL;
	e.in.offered = 0;
	e.out.count = &count;
	e.out.info = &info;
	e.out.needed = &needed;

	torture_assert_ntstatus_ok(tctx, dcerpc_spoolss_EnumPrinters(p, tctx, &e),
		"failed to enum printers");

	if (W_ERROR_EQUAL(e.out.result, WERR_INSUFFICIENT_BUFFER)) {
		DATA_BLOB blob = data_blob_talloc(tctx, NULL, needed);
		data_blob_clear(&blob);
		e.in.buffer = &blob;
		e.in.offered = needed;

		torture_assert_ntstatus_ok(tctx, dcerpc_spoolss_EnumPrinters(p, tctx, &e),
			"failed to enum printers");
	}

	torture_assert_werr_ok(tctx, e.out.result,
		"failed to enum printers");

	for (i=0; i < count; i++) {

		const char *current = NULL;

		switch (level) {
		case 1:
			current = info[i].info1.name;
			break;
		}

		if (strequal(current, name)) {
			*found = true;
			break;
		}
	}

	return true;
}

static bool test_AddPrinter_wellknown(struct torture_context *tctx,
				      struct dcerpc_pipe *p,
				      const char *printername,
				      bool ex)
{
	WERROR result;
	struct spoolss_AddPrinter r;
	struct spoolss_AddPrinterEx rex;
	struct spoolss_SetPrinterInfoCtr info_ctr;
	struct spoolss_SetPrinterInfo1 info1;
	struct spoolss_DevmodeContainer devmode_ctr;
	struct sec_desc_buf secdesc_ctr;
	struct spoolss_UserLevelCtr userlevel_ctr;
	struct policy_handle handle;
	bool found = false;

	ZERO_STRUCT(devmode_ctr);
	ZERO_STRUCT(secdesc_ctr);
	ZERO_STRUCT(userlevel_ctr);
	ZERO_STRUCT(info1);

	torture_comment(tctx, "Testing AddPrinter%s level 1\n", ex ? "Ex":"");

	/* try to add printer to wellknown printer list (level 1) */

	userlevel_ctr.level = 1;

	info_ctr.info.info1 = &info1;
	info_ctr.level = 1;

	rex.in.server = NULL;
	rex.in.info_ctr = &info_ctr;
	rex.in.devmode_ctr = &devmode_ctr;
	rex.in.secdesc_ctr = &secdesc_ctr;
	rex.in.userlevel_ctr = &userlevel_ctr;
	rex.out.handle = &handle;

	r.in.server = NULL;
	r.in.info_ctr = &info_ctr;
	r.in.devmode_ctr = &devmode_ctr;
	r.in.secdesc_ctr = &secdesc_ctr;
	r.out.handle = &handle;

	torture_assert_ntstatus_ok(tctx, ex ? dcerpc_spoolss_AddPrinterEx(p, tctx, &rex) :
					      dcerpc_spoolss_AddPrinter(p, tctx, &r),
		"failed to add printer");
	result = ex ? rex.out.result : r.out.result;
	torture_assert_werr_equal(tctx, result, WERR_INVALID_PRINTER_NAME,
		"unexpected result code");

	info1.name = printername;
	info1.flags = PRINTER_ATTRIBUTE_SHARED;

	torture_assert_ntstatus_ok(tctx, ex ? dcerpc_spoolss_AddPrinterEx(p, tctx, &rex) :
					      dcerpc_spoolss_AddPrinter(p, tctx, &r),
		"failed to add printer");
	result = ex ? rex.out.result : r.out.result;
	torture_assert_werr_equal(tctx, result, WERR_PRINTER_ALREADY_EXISTS,
		"unexpected result code");

	/* bizarre protocol, WERR_PRINTER_ALREADY_EXISTS means success here,
	   better do a real check to see the printer is really there */

	torture_assert(tctx, test_EnumPrinters_findname(tctx, p,
							PRINTER_ENUM_NETWORK, 1,
							printername,
							&found),
			"failed to enum printers");

	torture_assert(tctx, found, "failed to find newly added printer");

	info1.flags = 0;

	torture_assert_ntstatus_ok(tctx, ex ? dcerpc_spoolss_AddPrinterEx(p, tctx, &rex) :
					      dcerpc_spoolss_AddPrinter(p, tctx, &r),
		"failed to add printer");
	result = ex ? rex.out.result : r.out.result;
	torture_assert_werr_equal(tctx, result, WERR_PRINTER_ALREADY_EXISTS,
		"unexpected result code");

	/* bizarre protocol, WERR_PRINTER_ALREADY_EXISTS means success here,
	   better do a real check to see the printer has really been removed
	   from the well known printer list */

	found = false;

	torture_assert(tctx, test_EnumPrinters_findname(tctx, p,
							PRINTER_ENUM_NETWORK, 1,
							printername,
							&found),
			"failed to enum printers");
#if 0
	torture_assert(tctx, !found, "printer still in well known printer list");
#endif
	return true;
}

static bool test_AddPrinter_normal(struct torture_context *tctx,
				   struct dcerpc_pipe *p,
				   struct policy_handle *handle_p,
				   const char *printername,
				   const char *drivername,
				   const char *portname,
				   bool ex)
{
	WERROR result;
	struct spoolss_AddPrinter r;
	struct spoolss_AddPrinterEx rex;
	struct spoolss_SetPrinterInfoCtr info_ctr;
	struct spoolss_SetPrinterInfo2 info2;
	struct spoolss_DevmodeContainer devmode_ctr;
	struct sec_desc_buf secdesc_ctr;
	struct spoolss_UserLevelCtr userlevel_ctr;
	struct policy_handle handle;
	bool found = false;

	ZERO_STRUCT(devmode_ctr);
	ZERO_STRUCT(secdesc_ctr);
	ZERO_STRUCT(userlevel_ctr);

	torture_comment(tctx, "Testing AddPrinter%s level 2\n", ex ? "Ex":"");

	userlevel_ctr.level = 1;

	rex.in.server = NULL;
	rex.in.info_ctr = &info_ctr;
	rex.in.devmode_ctr = &devmode_ctr;
	rex.in.secdesc_ctr = &secdesc_ctr;
	rex.in.userlevel_ctr = &userlevel_ctr;
	rex.out.handle = &handle;

	r.in.server = NULL;
	r.in.info_ctr = &info_ctr;
	r.in.devmode_ctr = &devmode_ctr;
	r.in.secdesc_ctr = &secdesc_ctr;
	r.out.handle = &handle;

 again:

	/* try to add printer to printer list (level 2) */

	ZERO_STRUCT(info2);

	info_ctr.info.info2 = &info2;
	info_ctr.level = 2;

	torture_assert_ntstatus_ok(tctx, ex ? dcerpc_spoolss_AddPrinterEx(p, tctx, &rex) :
					      dcerpc_spoolss_AddPrinter(p, tctx, &r),
		"failed to add printer");
	result = ex ? rex.out.result : r.out.result;
	torture_assert_werr_equal(tctx, result, WERR_INVALID_PRINTER_NAME,
		"unexpected result code");

	info2.printername = printername;

	torture_assert_ntstatus_ok(tctx, ex ? dcerpc_spoolss_AddPrinterEx(p, tctx, &rex) :
					      dcerpc_spoolss_AddPrinter(p, tctx, &r),
		"failed to add printer");
	result = ex ? rex.out.result : r.out.result;

	if (W_ERROR_EQUAL(result, WERR_PRINTER_ALREADY_EXISTS)) {
		struct policy_handle printer_handle;

		torture_assert(tctx, call_OpenPrinterEx(tctx, p, printername, &printer_handle),
			"failed to open printer handle");

		torture_assert(tctx, test_DeletePrinter(tctx, p, &printer_handle),
			"failed to delete printer");

		torture_assert(tctx, test_ClosePrinter(tctx, p, &printer_handle),
			"failed to close server handle");

		goto again;
	}

	torture_assert_werr_equal(tctx, result, WERR_UNKNOWN_PORT,
		"unexpected result code");

	info2.portname = portname;

	torture_assert_ntstatus_ok(tctx, ex ? dcerpc_spoolss_AddPrinterEx(p, tctx, &rex) :
					      dcerpc_spoolss_AddPrinter(p, tctx, &r),
		"failed to add printer");
	result = ex ? rex.out.result : r.out.result;
	torture_assert_werr_equal(tctx, result, WERR_UNKNOWN_PRINTER_DRIVER,
		"unexpected result code");

	info2.drivername = drivername;

	torture_assert_ntstatus_ok(tctx, ex ? dcerpc_spoolss_AddPrinterEx(p, tctx, &rex) :
					      dcerpc_spoolss_AddPrinter(p, tctx, &r),
		"failed to add printer");
	result = ex ? rex.out.result : r.out.result;
	torture_assert_werr_equal(tctx, result, WERR_UNKNOWN_PRINTPROCESSOR,
		"unexpected result code");

	info2.printprocessor = "winprint";

	torture_assert_ntstatus_ok(tctx, ex ? dcerpc_spoolss_AddPrinterEx(p, tctx, &rex) :
					      dcerpc_spoolss_AddPrinter(p, tctx, &r),
		"failed to add printer");
	result = ex ? rex.out.result : r.out.result;
	torture_assert_werr_ok(tctx, result,
		"failed to add printer");

	*handle_p = handle;

	/* we are paranoid, really check if the printer is there now */

	torture_assert(tctx, test_EnumPrinters_findname(tctx, p,
							PRINTER_ENUM_LOCAL, 1,
							printername,
							&found),
			"failed to enum printers");
	torture_assert(tctx, found, "failed to find newly added printer");

	torture_assert_ntstatus_ok(tctx, ex ? dcerpc_spoolss_AddPrinterEx(p, tctx, &rex) :
					      dcerpc_spoolss_AddPrinter(p, tctx, &r),
		"failed to add printer");
	result = ex ? rex.out.result : r.out.result;
	torture_assert_werr_equal(tctx, result, WERR_PRINTER_ALREADY_EXISTS,
		"unexpected result code");

	return true;
}

static bool test_AddPrinterEx(struct torture_context *tctx,
			      struct dcerpc_pipe *p,
			      struct policy_handle *handle_p,
			      const char *printername,
			      const char *drivername,
			      const char *portname)
{
	bool ret = true;

	if (!torture_setting_bool(tctx, "samba3", false)) {
		if (!test_AddPrinter_wellknown(tctx, p, TORTURE_WELLKNOWN_PRINTER_EX, true)) {
			torture_comment(tctx, "failed to add printer to well known list\n");
			ret = false;
		}
	}

	if (!test_AddPrinter_normal(tctx, p, handle_p,
				    printername, drivername, portname,
				    true)) {
		torture_comment(tctx, "failed to add printer to printer list\n");
		ret = false;
	}

	return ret;
}

static bool test_AddPrinter(struct torture_context *tctx,
			    struct dcerpc_pipe *p,
			    struct policy_handle *handle_p,
			    const char *printername,
			    const char *drivername,
			    const char *portname)
{
	bool ret = true;

	if (!torture_setting_bool(tctx, "samba3", false)) {
		if (!test_AddPrinter_wellknown(tctx, p, TORTURE_WELLKNOWN_PRINTER, false)) {
			torture_comment(tctx, "failed to add printer to well known list\n");
			ret = false;
		}
	}

	if (!test_AddPrinter_normal(tctx, p, handle_p,
				    printername, drivername, portname,
				    false)) {
		torture_comment(tctx, "failed to add printer to printer list\n");
		ret = false;
	}

	return ret;
}

static bool test_printer_info(struct torture_context *tctx,
			      struct dcerpc_pipe *p,
			      struct policy_handle *handle)
{
	bool ret = true;

	if (!test_PrinterInfo(tctx, p, handle)) {
		ret = false;
	}

	if (!test_SetPrinter_errors(tctx, p, handle)) {
		ret = false;
	}

	return ret;
}

static bool test_EnumPrinterKey(struct torture_context *tctx,
				struct dcerpc_pipe *p,
				struct policy_handle *handle,
				const char *key_name,
				const char ***array)
{
	struct spoolss_EnumPrinterKey r;
	uint32_t needed = 0;
	union spoolss_KeyNames key_buffer;
	int32_t offered[] = { 0, 1, 2, 3, 4, 5, -1, -2, -3, -4, -5, 256, 512, 1024, 2048 };
	uint32_t _ndr_size;
	int i;

	r.in.handle = handle;
	r.in.key_name = key_name;
	r.out.key_buffer = &key_buffer;
	r.out.needed = &needed;
	r.out._ndr_size = &_ndr_size;

	for (i=0; i < ARRAY_SIZE(offered); i++) {

		if (offered[i] < 0 && needed) {
			if (needed <= 4) {
				continue;
			}
			r.in.offered = needed + offered[i];
		} else {
			r.in.offered = offered[i];
		}

		ZERO_STRUCT(key_buffer);

		torture_comment(tctx, "Testing EnumPrinterKey(%s) with %d offered\n", r.in.key_name, r.in.offered);

		torture_assert_ntstatus_ok(tctx, dcerpc_spoolss_EnumPrinterKey(p, tctx, &r),
			"failed to call EnumPrinterKey");
		if (W_ERROR_EQUAL(r.out.result, WERR_MORE_DATA)) {

			torture_assert(tctx, (_ndr_size == r.in.offered/2),
				talloc_asprintf(tctx, "EnumPrinterKey size mismatch, _ndr_size %d (expected %d)",
					_ndr_size, r.in.offered/2));

			r.in.offered = needed;
			torture_assert_ntstatus_ok(tctx, dcerpc_spoolss_EnumPrinterKey(p, tctx, &r),
				"failed to call EnumPrinterKey");
		}

		if (offered[i] > 0) {
			torture_assert_werr_ok(tctx, r.out.result,
				"failed to call EnumPrinterKey");
		}

		torture_assert(tctx, (_ndr_size == r.in.offered/2),
			talloc_asprintf(tctx, "EnumPrinterKey size mismatch, _ndr_size %d (expected %d)",
				_ndr_size, r.in.offered/2));

		torture_assert(tctx, (*r.out.needed <= r.in.offered),
			talloc_asprintf(tctx, "EnumPrinterKey size mismatch: needed %d is not <= offered %d", *r.out.needed, r.in.offered));

		torture_assert(tctx, (*r.out.needed <= _ndr_size * 2),
			talloc_asprintf(tctx, "EnumPrinterKey size mismatch: needed %d is not <= _ndr_size %d * 2", *r.out.needed, _ndr_size));

		if (key_buffer.string_array) {
			uint32_t calc_needed = 0;
			int s;
			for (s=0; key_buffer.string_array[s]; s++) {
				calc_needed += strlen_m_term(key_buffer.string_array[s])*2;
			}
			if (!key_buffer.string_array[0]) {
				calc_needed += 2;
			}
			calc_needed += 2;

			torture_assert_int_equal(tctx, *r.out.needed, calc_needed,
				"EnumPrinterKey unexpected size");
		}
	}

	if (array) {
		*array = key_buffer.string_array;
	}

	return true;
}

bool test_printer_keys(struct torture_context *tctx,
		       struct dcerpc_pipe *p,
		       struct policy_handle *handle)
{
	const char **key_array = NULL;
	int i;

	torture_assert(tctx, test_EnumPrinterKey(tctx, p, handle, "", &key_array),
		"failed to call test_EnumPrinterKey");

	for (i=0; key_array && key_array[i]; i++) {
		torture_assert(tctx, test_EnumPrinterKey(tctx, p, handle, key_array[i], NULL),
			"failed to call test_EnumPrinterKey");
	}
	for (i=0; key_array && key_array[i]; i++) {
		torture_assert(tctx, test_EnumPrinterDataEx(tctx, p, handle, key_array[i]),
			"failed to call test_EnumPrinterDataEx");
	}

	return true;
}

static bool test_printer(struct torture_context *tctx,
			 struct dcerpc_pipe *p)
{
	bool ret = true;
	struct policy_handle handle[2];
	bool found = false;
	const char *drivername = "Microsoft XPS Document Writer";
	const char *portname = "LPT1:";

	/* test printer created via AddPrinter */

	if (!test_AddPrinter(tctx, p, &handle[0], TORTURE_PRINTER, drivername, portname)) {
		return false;
	}

	if (!test_printer_info(tctx, p, &handle[0])) {
		ret = false;
	}

	if (!test_printer_keys(tctx, p, &handle[0])) {
		ret = false;
	}

	if (!test_DeletePrinter(tctx, p, &handle[0])) {
		ret = false;
	}

	if (!test_EnumPrinters_findname(tctx, p, PRINTER_ENUM_LOCAL, 1,
					TORTURE_PRINTER, &found)) {
		ret = false;
	}

	torture_assert(tctx, !found, "deleted printer still there");

	/* test printer created via AddPrinterEx */

	if (!test_AddPrinterEx(tctx, p, &handle[1], TORTURE_PRINTER_EX, drivername, portname)) {
		return false;
	}

	if (!test_printer_info(tctx, p, &handle[1])) {
		ret = false;
	}

	if (!test_printer_keys(tctx, p, &handle[1])) {
		ret = false;
	}

	if (!test_DeletePrinter(tctx, p, &handle[1])) {
		ret = false;
	}

	if (!test_EnumPrinters_findname(tctx, p, PRINTER_ENUM_LOCAL, 1,
					TORTURE_PRINTER_EX, &found)) {
		ret = false;
	}

	torture_assert(tctx, !found, "deleted printer still there");

	return ret;
}

bool torture_rpc_spoolss(struct torture_context *torture)
{
	NTSTATUS status;
	struct dcerpc_pipe *p;
	bool ret = true;
	struct test_spoolss_context *ctx;

	status = torture_rpc_connection(torture, &p, &ndr_table_spoolss);
	if (!NT_STATUS_IS_OK(status)) {
		return false;
	}

	ctx = talloc_zero(torture, struct test_spoolss_context);

	ret &= test_OpenPrinter_server(torture, p, &ctx->server_handle);
	ret &= test_GetPrinterData_list(torture, p, &ctx->server_handle);
	ret &= test_EnumForms(torture, p, &ctx->server_handle, true);
	ret &= test_AddForm(torture, p, &ctx->server_handle, true);
	ret &= test_EnumPorts(torture, p, ctx);
	ret &= test_GetPrinterDriverDirectory(torture, p, ctx);
	ret &= test_GetPrintProcessorDirectory(torture, p, ctx);
	ret &= test_EnumPrinterDrivers(torture, p, ctx, SPOOLSS_ARCHITECTURE_NT_X86);
	ret &= test_EnumPrinterDrivers(torture, p, ctx, SPOOLSS_ARCHITECTURE_ALL);
	ret &= test_EnumMonitors(torture, p, ctx);
	ret &= test_EnumPrintProcessors(torture, p, ctx);
	ret &= test_EnumPrintProcDataTypes(torture, p, ctx);
	ret &= test_EnumPrinters(torture, p, ctx);
	ret &= test_OpenPrinter_badname(torture, p, "__INVALID_PRINTER__");
	ret &= test_OpenPrinter_badname(torture, p, "\\\\__INVALID_HOST__");
	ret &= test_OpenPrinter_badname(torture, p, "");
	ret &= test_OpenPrinter_badname(torture, p, "\\\\\\");
	ret &= test_OpenPrinter_badname(torture, p, "\\\\\\__INVALID_PRINTER__");
	ret &= test_OpenPrinter_badname(torture, p, talloc_asprintf(torture, "\\\\%s\\", dcerpc_server_name(p)));
	ret &= test_OpenPrinter_badname(torture, p,
					talloc_asprintf(torture, "\\\\%s\\__INVALID_PRINTER__", dcerpc_server_name(p)));


	ret &= test_AddPort(torture, p);
	ret &= test_EnumPorts_old(torture, p);
	ret &= test_EnumPrinters_old(torture, p);
	ret &= test_EnumPrinterDrivers_old(torture, p);

	return ret;
}

struct torture_suite *torture_rpc_spoolss_printer(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "SPOOLSS-PRINTER");

	struct torture_rpc_tcase *tcase = torture_suite_add_rpc_iface_tcase(suite,
							"printer", &ndr_table_spoolss);

	torture_rpc_tcase_add_test(tcase, "printer", test_printer);

	return suite;
}
