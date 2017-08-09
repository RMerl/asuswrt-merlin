/*
   Unix SMB/CIFS implementation.
   test suite for spoolss rpc operations

   Copyright (C) Tim Potter 2003
   Copyright (C) Stefan Metzmacher 2005
   Copyright (C) Jelmer Vernooij 2007
   Copyright (C) Guenther Deschner 2009-2011

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
#include "librpc/gen_ndr/ndr_misc.h"
#include "librpc/gen_ndr/ndr_spoolss.h"
#include "librpc/gen_ndr/ndr_spoolss_c.h"
#include "librpc/gen_ndr/ndr_winreg_c.h"
#include "librpc/gen_ndr/ndr_security.h"
#include "libcli/security/security.h"
#include "torture/rpc/torture_rpc.h"
#include "param/param.h"
#include "lib/registry/registry.h"
#include "libcli/libcli.h"
#include "libcli/raw/raw_proto.h"
#include "libcli/resolve/resolve.h"
#include "libcli/smb2/smb2.h"
#include "libcli/smb2/smb2_calls.h"
#include "lib/cmdline/popt_common.h"
#include "system/filesys.h"
#include "torture/ndr/ndr.h"
#include "torture/smb2/proto.h"

#define TORTURE_WELLKNOWN_PRINTER	"torture_wkn_printer"
#define TORTURE_PRINTER			"torture_printer"
#define TORTURE_WELLKNOWN_PRINTER_EX	"torture_wkn_printer_ex"
#define TORTURE_PRINTER_EX		"torture_printer_ex"
#define TORTURE_DRIVER			"torture_driver"
#define TORTURE_DRIVER_EX		"torture_driver_ex"
#define TORTURE_DRIVER_ADOBE		"torture_driver_adobe"
#define TORTURE_DRIVER_EX_ADOBE		"torture_driver_ex_adobe"
#define TORTURE_DRIVER_ADOBE_CUPSADDSMB	"torture_driver_adobe_cupsaddsmb"
#define TORTURE_DRIVER_TIMESTAMPS	"torture_driver_timestamps"
#define TORTURE_DRIVER_DELETER		"torture_driver_deleter"
#define TORTURE_DRIVER_DELETERIN	"torture_driver_deleterin"
#define TORTURE_PRINTER_STATIC1		"print1"

#define TOP_LEVEL_PRINT_KEY "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Print"
#define TOP_LEVEL_PRINT_PRINTERS_KEY TOP_LEVEL_PRINT_KEY "\\Printers"
#define TOP_LEVEL_CONTROL_KEY "SYSTEM\\CurrentControlSet\\Control\\Print"
#define TOP_LEVEL_CONTROL_FORMS_KEY TOP_LEVEL_CONTROL_KEY "\\Forms"
#define TOP_LEVEL_CONTROL_PRINTERS_KEY TOP_LEVEL_CONTROL_KEY "\\Printers"
#define TOP_LEVEL_CONTROL_ENVIRONMENTS_KEY TOP_LEVEL_CONTROL_KEY "\\Environments"

struct test_spoolss_context {
	struct dcerpc_pipe *spoolss_pipe;

	/* server environment */
	const char *environment;

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

struct torture_driver_context {
	struct {
		const char *driver_directory;
		const char *environment;
	} local;
	struct {
		const char *driver_directory;
		const char *environment;
	} remote;
	struct spoolss_AddDriverInfo8 info8;
	bool ex;
};

struct torture_printer_context {
	struct dcerpc_pipe *spoolss_pipe;
	struct spoolss_SetPrinterInfo2 info2;
	struct torture_driver_context driver;
	bool ex;
	bool wellknown;
	bool added_driver;
	bool have_driver;
	struct spoolss_DeviceMode *devmode;
	struct policy_handle handle;
};

static bool upload_printer_driver(struct torture_context *tctx,
				  const char *server_name,
				  struct torture_driver_context *d);
static bool remove_printer_driver(struct torture_context *tctx,
				  const char *server_name,
				  struct torture_driver_context *d);
static bool fillup_printserver_info(struct torture_context *tctx,
				    struct dcerpc_pipe *p,
				    struct torture_driver_context *d);
static bool test_AddPrinterDriver_args_level_3(struct torture_context *tctx,
					       struct dcerpc_binding_handle *b,
					       const char *server_name,
					       struct spoolss_AddDriverInfo8 *r,
					       uint32_t flags,
					       bool ex,
					       const char *remote_driver_dir);

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

#define CHECK_NEEDED_SIZE_ENUM_LEVEL(fn, info, level, count, needed, align) do { \
	if (torture_setting_bool(tctx, "spoolss_check_size", false)) {\
	uint32_t size = ndr_size_##fn##_info(tctx, level, count, info);\
	uint32_t round_size = DO_ROUND(size, align);\
	if (round_size != needed) {\
		torture_warning(tctx, __location__": "#fn" level %d (count: %d) got unexpected needed size: %d, we calculated: %d", level, count, needed, round_size);\
		CHECK_ALIGN(size, align);\
	}\
	}\
} while(0)

#define CHECK_NEEDED_SIZE_ENUM(fn, info, count, needed, align) do { \
	if (torture_setting_bool(tctx, "spoolss_check_size", false)) {\
	uint32_t size = ndr_size_##fn##_info(tctx, count, info);\
	uint32_t round_size = DO_ROUND(size, align);\
	if (round_size != needed) {\
		torture_warning(tctx, __location__": "#fn" (count: %d) got unexpected needed size: %d, we calculated: %d", count, needed, round_size);\
		CHECK_ALIGN(size, align);\
	}\
	}\
} while(0)

#define CHECK_NEEDED_SIZE_LEVEL(fn, info, level, needed, align) do { \
	if (torture_setting_bool(tctx, "spoolss_check_size", false)) {\
	uint32_t size = ndr_size_##fn(info, level, 0);\
	uint32_t round_size = DO_ROUND(size, align);\
	if (round_size != needed) {\
		torture_warning(tctx, __location__": "#fn" level %d got unexpected needed size: %d, we calculated: %d", level, needed, round_size);\
		CHECK_ALIGN(size, align);\
	}\
	}\
} while(0)

static bool PrinterInfo_to_SetPrinterInfo(struct torture_context *tctx,
					  const union spoolss_PrinterInfo *i,
					  uint32_t level,
					  union spoolss_SetPrinterInfo *s)
{
	switch (level) {
	case 0:
		s->info0			= talloc(tctx, struct spoolss_SetPrinterInfo0);
		break;
	case 2:
		s->info2			= talloc(tctx, struct spoolss_SetPrinterInfo2);
		s->info2->servername		= i->info2.servername;
		s->info2->printername		= i->info2.printername;
		s->info2->sharename		= i->info2.sharename;
		s->info2->portname		= i->info2.portname;
		s->info2->drivername		= i->info2.drivername;
		s->info2->comment		= i->info2.comment;
		s->info2->location		= i->info2.location;
		s->info2->devmode_ptr		= 0;
		s->info2->sepfile		= i->info2.sepfile;
		s->info2->printprocessor	= i->info2.printprocessor;
		s->info2->datatype		= i->info2.datatype;
		s->info2->parameters		= i->info2.parameters;
		s->info2->secdesc_ptr		= 0;
		s->info2->attributes		= i->info2.attributes;
		s->info2->priority		= i->info2.priority;
		s->info2->defaultpriority	= i->info2.defaultpriority;
		s->info2->starttime		= i->info2.starttime;
		s->info2->untiltime		= i->info2.untiltime;
		s->info2->status		= i->info2.status;
		s->info2->cjobs			= i->info2.cjobs;
		s->info2->averageppm		= i->info2.averageppm;
		break;
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
	case 8:
	case 9:
	default:
		return false;
	}

	return true;
}

static bool test_OpenPrinter_server(struct torture_context *tctx,
				    struct dcerpc_pipe *p,
				    struct policy_handle *server_handle)
{
	NTSTATUS status;
	struct spoolss_OpenPrinter op;
	struct dcerpc_binding_handle *b = p->binding_handle;

	op.in.printername	= talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	op.in.datatype		= NULL;
	op.in.devmode_ctr.devmode= NULL;
	op.in.access_mask	= 0;
	op.out.handle		= server_handle;

	torture_comment(tctx, "Testing OpenPrinter(%s)\n", op.in.printername);

	status = dcerpc_spoolss_OpenPrinter_r(b, tctx, &op);
	torture_assert_ntstatus_ok(tctx, status, "dcerpc_spoolss_OpenPrinter failed");
	torture_assert_werr_ok(tctx, op.out.result, "dcerpc_spoolss_OpenPrinter failed");

	return true;
}

static bool test_EnumPorts(struct torture_context *tctx,
			   void *private_data)
{
	struct test_spoolss_context *ctx =
		talloc_get_type_abort(private_data, struct test_spoolss_context);
	struct dcerpc_pipe *p = ctx->spoolss_pipe;
	struct dcerpc_binding_handle *b = p->binding_handle;
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

		status = dcerpc_spoolss_EnumPorts_r(b, ctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "dcerpc_spoolss_EnumPorts failed");
		if (W_ERROR_IS_OK(r.out.result)) {
			/* TODO: do some more checks here */
			continue;
		}
		torture_assert_werr_equal(tctx, r.out.result, WERR_INSUFFICIENT_BUFFER,
			"EnumPorts unexpected return code");

		blob = data_blob_talloc_zero(ctx, needed);
		r.in.buffer = &blob;
		r.in.offered = needed;

		status = dcerpc_spoolss_EnumPorts_r(b, ctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "dcerpc_spoolss_EnumPorts failed");

		torture_assert_werr_ok(tctx, r.out.result, "EnumPorts failed");

		torture_assert(tctx, info, "EnumPorts returned no info");

		CHECK_NEEDED_SIZE_ENUM_LEVEL(spoolss_EnumPorts, info, r.in.level, count, needed, 4);

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
					    void *private_data)
{
	struct test_spoolss_context *ctx =
		talloc_get_type_abort(private_data, struct test_spoolss_context);

	NTSTATUS status;
	struct dcerpc_pipe *p = ctx->spoolss_pipe;
	struct dcerpc_binding_handle *b = p->binding_handle;
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
			.server	= talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p))
		},{
			.level	= 1024,
			.server	= talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p))
		}
	};
	int i;
	uint32_t needed;

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		int level = levels[i].level;
		DATA_BLOB blob;

		r.in.server		= levels[i].server;
		r.in.environment	= ctx->environment;
		r.in.level		= level;
		r.in.buffer		= NULL;
		r.in.offered		= 0;
		r.out.needed		= &needed;

		torture_comment(tctx, "Testing GetPrintProcessorDirectory level %u\n", r.in.level);

		status = dcerpc_spoolss_GetPrintProcessorDirectory_r(b, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status,
			"dcerpc_spoolss_GetPrintProcessorDirectory failed");
		torture_assert_werr_equal(tctx, r.out.result, WERR_INSUFFICIENT_BUFFER,
			"GetPrintProcessorDirectory unexpected return code");

		blob = data_blob_talloc_zero(tctx, needed);
		r.in.buffer = &blob;
		r.in.offered = needed;

		status = dcerpc_spoolss_GetPrintProcessorDirectory_r(b, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "dcerpc_spoolss_GetPrintProcessorDirectory failed");

		torture_assert_werr_ok(tctx, r.out.result, "GetPrintProcessorDirectory failed");

		CHECK_NEEDED_SIZE_LEVEL(spoolss_PrintProcessorDirectoryInfo, r.out.info, r.in.level, needed, 2);
	}

	return true;
}


static bool test_GetPrinterDriverDirectory(struct torture_context *tctx,
					   void *private_data)
{
	struct test_spoolss_context *ctx =
		talloc_get_type_abort(private_data, struct test_spoolss_context);

	NTSTATUS status;
	struct dcerpc_pipe *p = ctx->spoolss_pipe;
	struct dcerpc_binding_handle *b = p->binding_handle;
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
			.server	= talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p))
		},{
			.level	= 1024,
			.server	= talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p))
		}
	};
	int i;
	uint32_t needed;

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		int level = levels[i].level;
		DATA_BLOB blob;

		r.in.server		= levels[i].server;
		r.in.environment	= ctx->environment;
		r.in.level		= level;
		r.in.buffer		= NULL;
		r.in.offered		= 0;
		r.out.needed		= &needed;

		torture_comment(tctx, "Testing GetPrinterDriverDirectory level %u\n", r.in.level);

		status = dcerpc_spoolss_GetPrinterDriverDirectory_r(b, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status,
			"dcerpc_spoolss_GetPrinterDriverDirectory failed");
		torture_assert_werr_equal(tctx, r.out.result, WERR_INSUFFICIENT_BUFFER,
			"GetPrinterDriverDirectory unexpected return code");

		blob = data_blob_talloc_zero(tctx, needed);
		r.in.buffer = &blob;
		r.in.offered = needed;

		status = dcerpc_spoolss_GetPrinterDriverDirectory_r(b, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "dcerpc_spoolss_GetPrinterDriverDirectory failed");

		torture_assert_werr_ok(tctx, r.out.result, "GetPrinterDriverDirectory failed");

		CHECK_NEEDED_SIZE_LEVEL(spoolss_DriverDirectoryInfo, r.out.info, r.in.level, needed, 2);
	}

	return true;
}

static bool test_EnumPrinterDrivers_args(struct torture_context *tctx,
					 struct dcerpc_binding_handle *b,
					 const char *server_name,
					 const char *environment,
					 uint32_t level,
					 uint32_t *count_p,
					 union spoolss_DriverInfo **info_p)
{
	struct spoolss_EnumPrinterDrivers r;
	uint32_t needed;
	uint32_t count;
	union spoolss_DriverInfo *info;

	r.in.server		= server_name;
	r.in.environment	= environment;
	r.in.level		= level;
	r.in.buffer		= NULL;
	r.in.offered		= 0;
	r.out.needed		= &needed;
	r.out.count		= &count;
	r.out.info		= &info;

	torture_comment(tctx, "Testing EnumPrinterDrivers(%s) level %u\n",
		r.in.environment, r.in.level);

	torture_assert_ntstatus_ok(tctx,
		dcerpc_spoolss_EnumPrinterDrivers_r(b, tctx, &r),
		"EnumPrinterDrivers failed");
	if (W_ERROR_EQUAL(r.out.result, WERR_INSUFFICIENT_BUFFER)) {
		DATA_BLOB blob = data_blob_talloc_zero(tctx, needed);
		r.in.buffer = &blob;
		r.in.offered = needed;

		torture_assert_ntstatus_ok(tctx,
			dcerpc_spoolss_EnumPrinterDrivers_r(b, tctx, &r),
			"EnumPrinterDrivers failed");
	}

	torture_assert_werr_ok(tctx, r.out.result,
		"EnumPrinterDrivers failed");

	if (count_p) {
		*count_p = count;
	}
	if (info_p) {
		*info_p = info;
	}

	CHECK_NEEDED_SIZE_ENUM_LEVEL(spoolss_EnumPrinterDrivers, info, r.in.level, count, needed, 4);

	return true;

}

static bool test_EnumPrinterDrivers_findone(struct torture_context *tctx,
					    struct dcerpc_binding_handle *b,
					    const char *server_name,
					    const char *environment,
					    uint32_t level,
					    const char *driver_name,
					    union spoolss_DriverInfo *info_p)
{
	uint32_t count;
	union spoolss_DriverInfo *info;
	int i;
	const char *environment_ret = NULL;

	torture_assert(tctx,
		test_EnumPrinterDrivers_args(tctx, b, server_name, environment, level, &count, &info),
		"failed to enumerate printer drivers");

	for (i=0; i < count; i++) {
		const char *driver_name_ret;
		switch (level) {
		case 1:
			driver_name_ret = info[i].info1.driver_name;
			break;
		case 2:
			driver_name_ret = info[i].info2.driver_name;
			environment_ret = info[i].info2.architecture;
			break;
		case 3:
			driver_name_ret = info[i].info3.driver_name;
			environment_ret = info[i].info3.architecture;
			break;
		case 4:
			driver_name_ret = info[i].info4.driver_name;
			environment_ret = info[i].info4.architecture;
			break;
		case 5:
			driver_name_ret = info[i].info5.driver_name;
			environment_ret = info[i].info5.architecture;
			break;
		case 6:
			driver_name_ret = info[i].info6.driver_name;
			environment_ret = info[i].info6.architecture;
			break;
		case 7:
			driver_name_ret = info[i].info7.driver_name;
			break;
		case 8:
			driver_name_ret = info[i].info8.driver_name;
			environment_ret = info[i].info8.architecture;
			break;
		default:
			break;
		}
		if (environment_ret) {
			torture_assert_str_equal(tctx, environment, environment_ret, "architecture mismatch");
		}
		if (strequal(driver_name, driver_name_ret)) {
			if (info_p) {
				*info_p = info[i];
			}
			return true;
		}
	}

	return false;
}

static bool test_EnumPrinterDrivers(struct torture_context *tctx,
				    void *private_data)
{
	struct test_spoolss_context *ctx =
		talloc_get_type_abort(private_data, struct test_spoolss_context);
	struct dcerpc_pipe *p = ctx->spoolss_pipe;
	struct dcerpc_binding_handle *b = p->binding_handle;
	uint16_t levels[] = { 1, 2, 3, 4, 5, 6, 8 };
	int i, j, a;

	/* FIXME: gd, come back and fix "" as server, and handle
	 * priority of returned error codes in torture test and samba 3
	 * server */
	const char *server_name = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	const char *environments[2];

	environments[0] = SPOOLSS_ARCHITECTURE_ALL;
	environments[1] = ctx->environment;

	for (a=0;a<ARRAY_SIZE(environments);a++) {

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		int level = levels[i];
		uint32_t count;
		union spoolss_DriverInfo *info;

		torture_assert(tctx,
			test_EnumPrinterDrivers_args(tctx, b, server_name, environments[a], level, &count, &info),
			"failed to enumerate drivers");

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
	}

	return true;
}

static bool test_EnumMonitors(struct torture_context *tctx,
			      void *private_data)
{
	struct test_spoolss_context *ctx =
		talloc_get_type_abort(private_data, struct test_spoolss_context);
	struct dcerpc_pipe *p = ctx->spoolss_pipe;
	struct dcerpc_binding_handle *b = p->binding_handle;
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

		status = dcerpc_spoolss_EnumMonitors_r(b, ctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "dcerpc_spoolss_EnumMonitors failed");
		if (W_ERROR_IS_OK(r.out.result)) {
			/* TODO: do some more checks here */
			continue;
		}
		torture_assert_werr_equal(tctx, r.out.result, WERR_INSUFFICIENT_BUFFER,
			"EnumMonitors failed");

		blob = data_blob_talloc_zero(ctx, needed);
		r.in.buffer = &blob;
		r.in.offered = needed;

		status = dcerpc_spoolss_EnumMonitors_r(b, ctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "dcerpc_spoolss_EnumMonitors failed");

		torture_assert_werr_ok(tctx, r.out.result, "EnumMonitors failed");

		CHECK_NEEDED_SIZE_ENUM_LEVEL(spoolss_EnumMonitors, info, r.in.level, count, needed, 4);

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

static bool test_EnumPrintProcessors_level(struct torture_context *tctx,
					   struct dcerpc_binding_handle *b,
					   const char *environment,
					   uint32_t level,
					   uint32_t *count_p,
					   union spoolss_PrintProcessorInfo **info_p,
					   WERROR expected_result)
{
	struct spoolss_EnumPrintProcessors r;
	DATA_BLOB blob;
	uint32_t needed;
	uint32_t count;
	union spoolss_PrintProcessorInfo *info;

	r.in.servername = "";
	r.in.environment = environment;
	r.in.level = level;
	r.in.buffer = NULL;
	r.in.offered = 0;
	r.out.needed = &needed;
	r.out.count = &count;
	r.out.info = &info;

	torture_comment(tctx, "Testing EnumPrintProcessors(%s) level %u\n",
		r.in.environment, r.in.level);

	torture_assert_ntstatus_ok(tctx,
		dcerpc_spoolss_EnumPrintProcessors_r(b, tctx, &r),
		"EnumPrintProcessors failed");
	if (W_ERROR_EQUAL(r.out.result, WERR_INSUFFICIENT_BUFFER)) {
		blob = data_blob_talloc_zero(tctx, needed);
		r.in.buffer = &blob;
		r.in.offered = needed;
		torture_assert_ntstatus_ok(tctx,
			dcerpc_spoolss_EnumPrintProcessors_r(b, tctx, &r),
			"EnumPrintProcessors failed");
	}
	torture_assert_werr_equal(tctx, r.out.result, expected_result,
		"EnumPrintProcessors failed");

	CHECK_NEEDED_SIZE_ENUM_LEVEL(spoolss_EnumPrintProcessors, info, level, count, needed, 4);

	if (count_p) {
		*count_p = count;
	}
	if (info_p) {
		*info_p = info;
	}

	return true;
}

static bool test_EnumPrintProcessors(struct torture_context *tctx,
				     void *private_data)
{
	struct test_spoolss_context *ctx =
		talloc_get_type_abort(private_data, struct test_spoolss_context);

	uint16_t levels[] = {0, 1, 2, 3, 32, 256 };
	uint16_t     ok[] = {0, 1, 0, 0, 0, 0 };
	int i;
	struct dcerpc_pipe *p = ctx->spoolss_pipe;
	struct dcerpc_binding_handle *b = p->binding_handle;

	torture_assert(tctx,
		test_EnumPrintProcessors_level(tctx, b, "phantasy", 1, NULL, NULL, WERR_INVALID_ENVIRONMENT),
		"test_EnumPrintProcessors_level failed");

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		union spoolss_PrintProcessorInfo *info;
		uint32_t count;
		WERROR expected_result = ok[i] ? WERR_OK : WERR_INVALID_LEVEL;

		torture_assert(tctx,
			test_EnumPrintProcessors_level(tctx, b, ctx->environment, levels[i], &count, &info, expected_result),
			"test_EnumPrintProcessors_level failed");
	}

	return true;
}

static bool test_EnumPrintProcDataTypes_level(struct torture_context *tctx,
					      struct dcerpc_binding_handle *b,
					      const char *print_processor_name,
					      uint32_t level,
					      uint32_t *count_p,
					      union spoolss_PrintProcDataTypesInfo **info_p,
					      WERROR expected_result)
{
	struct spoolss_EnumPrintProcDataTypes r;
	DATA_BLOB blob;
	uint32_t needed;
	uint32_t count;
	union spoolss_PrintProcDataTypesInfo *info;

	r.in.servername = "";
	r.in.print_processor_name = print_processor_name;
	r.in.level = level;
	r.in.buffer = NULL;
	r.in.offered = 0;
	r.out.needed = &needed;
	r.out.count = &count;
	r.out.info = &info;

	torture_comment(tctx, "Testing EnumPrintProcDataTypes(%s) level %u\n",
		r.in.print_processor_name, r.in.level);

	torture_assert_ntstatus_ok(tctx,
		dcerpc_spoolss_EnumPrintProcDataTypes_r(b, tctx, &r),
		"EnumPrintProcDataTypes failed");
	if (W_ERROR_EQUAL(r.out.result, WERR_INSUFFICIENT_BUFFER)) {
		blob = data_blob_talloc_zero(tctx, needed);
		r.in.buffer = &blob;
		r.in.offered = needed;
		torture_assert_ntstatus_ok(tctx,
			dcerpc_spoolss_EnumPrintProcDataTypes_r(b, tctx, &r),
			"EnumPrintProcDataTypes failed");
	}
	torture_assert_werr_equal(tctx, r.out.result, expected_result,
		"EnumPrintProcDataTypes failed");

	CHECK_NEEDED_SIZE_ENUM_LEVEL(spoolss_EnumPrintProcDataTypes, info, level, count, needed, 4);

	if (count_p) {
		*count_p = count;
	}
	if (info_p) {
		*info_p = info;
	}

	return true;
}

static bool test_EnumPrintProcDataTypes(struct torture_context *tctx,
					void *private_data)
{
	struct test_spoolss_context *ctx =
		talloc_get_type_abort(private_data, struct test_spoolss_context);

	uint16_t levels[] = {0, 1, 2, 3, 32, 256 };
	uint16_t     ok[] = {0, 1, 0, 0, 0, 0 };
	int i;
	struct dcerpc_pipe *p = ctx->spoolss_pipe;
	struct dcerpc_binding_handle *b = p->binding_handle;

	torture_assert(tctx,
		test_EnumPrintProcDataTypes_level(tctx, b, NULL, 1, NULL, NULL, WERR_UNKNOWN_PRINTPROCESSOR),
		"test_EnumPrintProcDataTypes_level failed");

	torture_assert(tctx,
		test_EnumPrintProcDataTypes_level(tctx, b, "nonexisting", 1, NULL, NULL, WERR_UNKNOWN_PRINTPROCESSOR),
		"test_EnumPrintProcDataTypes_level failed");

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		int level = levels[i];
		uint32_t count;
		union spoolss_PrintProcDataTypesInfo *info;
		WERROR expected_result = ok[i] ? WERR_OK : WERR_INVALID_LEVEL;

		torture_assert(tctx,
			test_EnumPrintProcDataTypes_level(tctx, b, "winprint", level, &count, &info, expected_result),
			"test_EnumPrintProcDataTypes_level failed");
	}

	{
		union spoolss_PrintProcessorInfo *info;
		uint32_t count;

		torture_assert(tctx,
			test_EnumPrintProcessors_level(tctx, b, ctx->environment, 1, &count, &info, WERR_OK),
			"test_EnumPrintProcessors_level failed");

		for (i=0; i < count; i++) {
			torture_assert(tctx,
				test_EnumPrintProcDataTypes_level(tctx, b, info[i].info1.print_processor_name, 1, NULL, NULL, WERR_OK),
				"test_EnumPrintProcDataTypes_level failed");
		}
	}


	return true;
}

static bool test_EnumPrinters(struct torture_context *tctx,
			      void *private_data)
{
	struct test_spoolss_context *ctx =
		talloc_get_type_abort(private_data, struct test_spoolss_context);
	struct dcerpc_pipe *p = ctx->spoolss_pipe;
	struct dcerpc_binding_handle *b = p->binding_handle;
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

		status = dcerpc_spoolss_EnumPrinters_r(b, ctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "dcerpc_spoolss_EnumPrinters failed");
		if (W_ERROR_IS_OK(r.out.result)) {
			/* TODO: do some more checks here */
			continue;
		}
		torture_assert_werr_equal(tctx, r.out.result, WERR_INSUFFICIENT_BUFFER,
			"EnumPrinters unexpected return code");

		blob = data_blob_talloc_zero(ctx, needed);
		r.in.buffer = &blob;
		r.in.offered = needed;

		status = dcerpc_spoolss_EnumPrinters_r(b, ctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "dcerpc_spoolss_EnumPrinters failed");

		torture_assert_werr_ok(tctx, r.out.result, "EnumPrinters failed");

		CHECK_NEEDED_SIZE_ENUM_LEVEL(spoolss_EnumPrinters, info, r.in.level, count, needed, 4);

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
				   struct dcerpc_binding_handle *b,
				   struct policy_handle *handle,
				   const char *driver_name,
				   const char *environment);

bool test_GetPrinter_level(struct torture_context *tctx,
			   struct dcerpc_binding_handle *b,
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

	torture_assert_ntstatus_ok(tctx, dcerpc_spoolss_GetPrinter_r(b, tctx, &r),
		"GetPrinter failed");

	if (W_ERROR_EQUAL(r.out.result, WERR_INSUFFICIENT_BUFFER)) {
		DATA_BLOB blob = data_blob_talloc_zero(tctx, needed);
		r.in.buffer = &blob;
		r.in.offered = needed;

		torture_assert_ntstatus_ok(tctx, dcerpc_spoolss_GetPrinter_r(b, tctx, &r),
			"GetPrinter failed");
	}

	torture_assert_werr_ok(tctx, r.out.result, "GetPrinter failed");

	CHECK_NEEDED_SIZE_LEVEL(spoolss_PrinterInfo, r.out.info, r.in.level, needed, 4);

	if (info && r.out.info) {
		*info = *r.out.info;
	}

	return true;
}


static bool test_GetPrinter(struct torture_context *tctx,
			    struct dcerpc_binding_handle *b,
			    struct policy_handle *handle,
			    const char *environment)
{
	uint32_t levels[] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
	int i;

	for (i=0;i<ARRAY_SIZE(levels);i++) {

		union spoolss_PrinterInfo info;

		ZERO_STRUCT(info);

		torture_assert(tctx, test_GetPrinter_level(tctx, b, handle, levels[i], &info),
			"failed to call GetPrinter");

		if ((levels[i] == 2) && info.info2.drivername && strlen(info.info2.drivername)) {
			torture_assert(tctx,
				test_GetPrinterDriver2(tctx, b, handle, info.info2.drivername, environment),
				"failed to call test_GetPrinterDriver2");
		}
	}

	return true;
}

static bool test_SetPrinter(struct torture_context *tctx,
			    struct dcerpc_binding_handle *b,
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

	torture_comment(tctx, "Testing SetPrinter level %d\n", r.in.info_ctr->level);

	torture_assert_ntstatus_ok(tctx, dcerpc_spoolss_SetPrinter_r(b, tctx, &r),
		"failed to call SetPrinter");
	torture_assert_werr_ok(tctx, r.out.result,
		"failed to call SetPrinter");

	return true;
}

static bool test_SetPrinter_errors(struct torture_context *tctx,
				   struct dcerpc_binding_handle *b,
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

	torture_assert_ntstatus_ok(tctx, dcerpc_spoolss_SetPrinter_r(b, tctx, &r),
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

		torture_assert_ntstatus_ok(tctx, dcerpc_spoolss_SetPrinter_r(b, tctx, &r),
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
			     struct dcerpc_binding_handle *b,
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

	torture_skip(tctx, "Printer Info test is currently broken, skipping");

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
		status = dcerpc_spoolss_ ##call## _r(b, tctx, &r); \
		if (!NT_STATUS_IS_OK(status)) { \
			torture_comment(tctx, #call " level %u failed - %s (%s)\n", \
			       r.in.level, nt_errstr(status), __location__); \
			ret = false; \
			break; \
		}\
		if (W_ERROR_EQUAL(r.out.result, WERR_INSUFFICIENT_BUFFER)) {\
			DATA_BLOB blob = data_blob_talloc_zero(tctx, needed); \
			r.in.buffer = &blob; \
			r.in.offered = needed; \
		}\
		status = dcerpc_spoolss_ ##call## _r(b, tctx, &r); \
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
		status = dcerpc_spoolss_ ##call## _r(b, tctx, &r); \
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

#define SD_EQUAL(sd1, sd2, field) \
		if (!security_descriptor_equal(sd1, sd2)) { \
			torture_comment(tctx, "Failed to set %s (%s)\n", \
			       #field, __location__); \
			ret = false; \
			break; \
		}

#define TEST_PRINTERINFO_STRING_EXP_ERR(lvl1, field1, lvl2, field2, value, err) do { \
		torture_comment(tctx, "field test %d/%s vs %d/%s\n", lvl1, #field1, lvl2, #field2); \
		q.in.level = lvl1; \
		TESTGETCALL(GetPrinter, q) \
		info_ctr.level = lvl1; \
		info_ctr.info.info ## lvl1 = (struct spoolss_SetPrinterInfo ## lvl1 *)(void *)&q.out.info->info ## lvl1; \
		info_ctr.info.info ## lvl1->field1 = value;\
		TESTSETCALL_EXP(SetPrinter, s, err) \
		info_ctr.info.info ## lvl1->field1 = ""; \
		TESTGETCALL(GetPrinter, q) \
		info_ctr.info.info ## lvl1->field1 = value; \
		STRING_EQUAL(info_ctr.info.info ## lvl1->field1, value, field1); \
		q.in.level = lvl2; \
		TESTGETCALL(GetPrinter, q) \
		info_ctr.info.info ## lvl2 = (struct spoolss_SetPrinterInfo ## lvl2 *)(void *)&q.out.info->info ## lvl2; \
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
		info_ctr.info.info ## lvl1 = (struct spoolss_SetPrinterInfo ## lvl1 *)(void *)&q.out.info->info ## lvl1; \
		info_ctr.info.info ## lvl1->field1 = value; \
		TESTSETCALL(SetPrinter, s) \
		info_ctr.info.info ## lvl1->field1 = 0; \
		TESTGETCALL(GetPrinter, q) \
		info_ctr.info.info ## lvl1 = (struct spoolss_SetPrinterInfo ## lvl1 *)(void *)&q.out.info->info ## lvl1; \
		INT_EQUAL(info_ctr.info.info ## lvl1->field1, exp_value, field1); \
		q.in.level = lvl2; \
		TESTGETCALL(GetPrinter, q) \
		info_ctr.info.info ## lvl2 = (struct spoolss_SetPrinterInfo ## lvl2 *)(void *)&q.out.info->info ## lvl2; \
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

static bool test_security_descriptor_equal(struct torture_context *tctx,
					   const struct security_descriptor *sd1,
					   const struct security_descriptor *sd2)
{
	if (sd1 == sd2) {
		return true;
	}

	if (!sd1 || !sd2) {
		torture_comment(tctx, "%s\n", __location__);
		return false;
	}

	torture_assert_int_equal(tctx, sd1->revision, sd2->revision, "revision mismatch");
	torture_assert_int_equal(tctx, sd1->type, sd2->type, "type mismatch");

	torture_assert_sid_equal(tctx, sd1->owner_sid, sd2->owner_sid, "owner mismatch");
	torture_assert_sid_equal(tctx, sd1->group_sid, sd2->group_sid, "group mismatch");

	if (!security_acl_equal(sd1->sacl, sd2->sacl)) {
		torture_comment(tctx, "%s: sacl mismatch\n", __location__);
		NDR_PRINT_DEBUG(security_acl, sd1->sacl);
		NDR_PRINT_DEBUG(security_acl, sd2->sacl);
		return false;
	}
	if (!security_acl_equal(sd1->dacl, sd2->dacl)) {
		torture_comment(tctx, "%s: dacl mismatch\n", __location__);
		NDR_PRINT_DEBUG(security_acl, sd1->dacl);
		NDR_PRINT_DEBUG(security_acl, sd2->dacl);
		return false;
	}

	return true;
}

static bool test_sd_set_level(struct torture_context *tctx,
			      struct dcerpc_binding_handle *b,
			      struct policy_handle *handle,
			      uint32_t level,
			      struct security_descriptor *sd)
{
	struct spoolss_SetPrinterInfoCtr info_ctr;
	struct spoolss_DevmodeContainer devmode_ctr;
	struct sec_desc_buf secdesc_ctr;
	union spoolss_SetPrinterInfo sinfo;

	ZERO_STRUCT(devmode_ctr);
	ZERO_STRUCT(secdesc_ctr);

	switch (level) {
	case 2: {
		union spoolss_PrinterInfo info;
		torture_assert(tctx, test_GetPrinter_level(tctx, b, handle, 2, &info), "");
		torture_assert(tctx, PrinterInfo_to_SetPrinterInfo(tctx, &info, 2, &sinfo), "");

		info_ctr.level = 2;
		info_ctr.info = sinfo;

		break;
	}
	case 3: {
		struct spoolss_SetPrinterInfo3 info3;

		info3.sec_desc_ptr = 0;

		info_ctr.level = 3;
		info_ctr.info.info3 = &info3;

		break;
	}
	default:
		return false;
	}

	secdesc_ctr.sd = sd;

	torture_assert(tctx,
		test_SetPrinter(tctx, b, handle, &info_ctr, &devmode_ctr, &secdesc_ctr, 0), "");

	return true;
}

static bool test_PrinterInfo_SDs(struct torture_context *tctx,
				 struct dcerpc_binding_handle *b,
				 struct policy_handle *handle)
{
	union spoolss_PrinterInfo info;
	struct security_descriptor *sd1, *sd2;
	int i;

	/* just compare level 2 and level 3 */

	torture_assert(tctx, test_GetPrinter_level(tctx, b, handle, 2, &info), "");

	sd1 = info.info2.secdesc;

	torture_assert(tctx, test_GetPrinter_level(tctx, b, handle, 3, &info), "");

	sd2 = info.info3.secdesc;

	torture_assert(tctx, test_security_descriptor_equal(tctx, sd1, sd2),
		"SD level 2 != SD level 3");


	/* query level 2, set level 2, query level 2 */

	torture_assert(tctx, test_GetPrinter_level(tctx, b, handle, 2, &info), "");

	sd1 = info.info2.secdesc;

	torture_assert(tctx, test_sd_set_level(tctx, b, handle, 2, sd1), "");

	torture_assert(tctx, test_GetPrinter_level(tctx, b, handle, 2, &info), "");

	sd2 = info.info2.secdesc;
	if (sd1->type & SEC_DESC_DACL_DEFAULTED) {
		torture_comment(tctx, "removing SEC_DESC_DACL_DEFAULTED\n");
		sd1->type &= ~SEC_DESC_DACL_DEFAULTED;
	}

	torture_assert(tctx, test_security_descriptor_equal(tctx, sd1, sd2),
		"SD level 2 != SD level 2 after SD has been set via level 2");


	/* query level 2, set level 3, query level 2 */

	torture_assert(tctx, test_GetPrinter_level(tctx, b, handle, 2, &info), "");

	sd1 = info.info2.secdesc;

	torture_assert(tctx, test_sd_set_level(tctx, b, handle, 3, sd1), "");

	torture_assert(tctx, test_GetPrinter_level(tctx, b, handle, 2, &info), "");

	sd2 = info.info2.secdesc;

	torture_assert(tctx, test_security_descriptor_equal(tctx, sd1, sd2),
		"SD level 2 != SD level 2 after SD has been set via level 3");

	/* set modified sd level 3, query level 2 */

	for (i=0; i < 93; i++) {
		struct security_ace a;
		const char *sid_string = talloc_asprintf(tctx, "S-1-5-32-9999%i", i);
		a.type = SEC_ACE_TYPE_ACCESS_ALLOWED;
		a.flags = 0;
		a.size = 0; /* autogenerated */
		a.access_mask = 0;
		a.trustee = *dom_sid_parse_talloc(tctx, sid_string);
		torture_assert_ntstatus_ok(tctx, security_descriptor_dacl_add(sd1, &a), "");
	}

	torture_assert(tctx, test_sd_set_level(tctx, b, handle, 3, sd1), "");

	torture_assert(tctx, test_GetPrinter_level(tctx, b, handle, 2, &info), "");
	sd2 = info.info2.secdesc;

	if (sd1->type & SEC_DESC_DACL_DEFAULTED) {
		torture_comment(tctx, "removing SEC_DESC_DACL_DEFAULTED\n");
		sd1->type &= ~SEC_DESC_DACL_DEFAULTED;
	}

	torture_assert(tctx, test_security_descriptor_equal(tctx, sd1, sd2),
		"modified SD level 2 != SD level 2 after SD has been set via level 3");


	return true;
}

/*
 * wrapper call that saves original sd, runs tests, and restores sd
 */

static bool test_PrinterInfo_SD(struct torture_context *tctx,
				struct dcerpc_binding_handle *b,
				struct policy_handle *handle)
{
	union spoolss_PrinterInfo info;
	struct security_descriptor *sd;
	bool ret = true;

	torture_comment(tctx, "Testing Printer Security Descriptors\n");

	/* save original sd */

	torture_assert(tctx, test_GetPrinter_level(tctx, b, handle, 2, &info),
		"failed to get initial security descriptor");

	sd = security_descriptor_copy(tctx, info.info2.secdesc);

	/* run tests */

	ret = test_PrinterInfo_SDs(tctx, b, handle);

	/* restore original sd */

	torture_assert(tctx, test_sd_set_level(tctx, b, handle, 3, sd),
		"failed to restore initial security descriptor");

	torture_comment(tctx, "Printer Security Descriptors test %s\n\n",
		ret ? "succeeded" : "failed");


	return ret;
}

static bool test_devmode_set_level(struct torture_context *tctx,
				   struct dcerpc_binding_handle *b,
				   struct policy_handle *handle,
				   uint32_t level,
				   struct spoolss_DeviceMode *devmode)
{
	struct spoolss_SetPrinterInfoCtr info_ctr;
	struct spoolss_DevmodeContainer devmode_ctr;
	struct sec_desc_buf secdesc_ctr;
	union spoolss_SetPrinterInfo sinfo;

	ZERO_STRUCT(devmode_ctr);
	ZERO_STRUCT(secdesc_ctr);

	switch (level) {
	case 2: {
		union spoolss_PrinterInfo info;
		torture_assert(tctx, test_GetPrinter_level(tctx, b, handle, 2, &info), "");
		torture_assert(tctx, PrinterInfo_to_SetPrinterInfo(tctx, &info, 2, &sinfo), "");

		info_ctr.level = 2;
		info_ctr.info = sinfo;

		break;
	}
	case 8: {
		struct spoolss_SetPrinterInfo8 info8;

		info8.devmode_ptr = 0;

		info_ctr.level = 8;
		info_ctr.info.info8 = &info8;

		break;
	}
	default:
		return false;
	}

	devmode_ctr.devmode = devmode;

	torture_assert(tctx,
		test_SetPrinter(tctx, b, handle, &info_ctr, &devmode_ctr, &secdesc_ctr, 0), "");

	return true;
}


static bool test_devicemode_equal(struct torture_context *tctx,
				  const struct spoolss_DeviceMode *d1,
				  const struct spoolss_DeviceMode *d2)
{
	if (d1 == d2) {
		return true;
	}

	if (!d1 || !d2) {
		torture_comment(tctx, "%s\n", __location__);
		return false;
	}
	torture_assert_str_equal(tctx, d1->devicename, d2->devicename, "devicename mismatch");
	torture_assert_int_equal(tctx, d1->specversion, d2->specversion, "specversion mismatch");
	torture_assert_int_equal(tctx, d1->driverversion, d2->driverversion, "driverversion mismatch");
	torture_assert_int_equal(tctx, d1->size, d2->size, "size mismatch");
	torture_assert_int_equal(tctx, d1->__driverextra_length, d2->__driverextra_length, "__driverextra_length mismatch");
	torture_assert_int_equal(tctx, d1->fields, d2->fields, "fields mismatch");
	torture_assert_int_equal(tctx, d1->orientation, d2->orientation, "orientation mismatch");
	torture_assert_int_equal(tctx, d1->papersize, d2->papersize, "papersize mismatch");
	torture_assert_int_equal(tctx, d1->paperlength, d2->paperlength, "paperlength mismatch");
	torture_assert_int_equal(tctx, d1->paperwidth, d2->paperwidth, "paperwidth mismatch");
	torture_assert_int_equal(tctx, d1->scale, d2->scale, "scale mismatch");
	torture_assert_int_equal(tctx, d1->copies, d2->copies, "copies mismatch");
	torture_assert_int_equal(tctx, d1->defaultsource, d2->defaultsource, "defaultsource mismatch");
	torture_assert_int_equal(tctx, d1->printquality, d2->printquality, "printquality mismatch");
	torture_assert_int_equal(tctx, d1->color, d2->color, "color mismatch");
	torture_assert_int_equal(tctx, d1->duplex, d2->duplex, "duplex mismatch");
	torture_assert_int_equal(tctx, d1->yresolution, d2->yresolution, "yresolution mismatch");
	torture_assert_int_equal(tctx, d1->ttoption, d2->ttoption, "ttoption mismatch");
	torture_assert_int_equal(tctx, d1->collate, d2->collate, "collate mismatch");
	torture_assert_str_equal(tctx, d1->formname, d2->formname, "formname mismatch");
	torture_assert_int_equal(tctx, d1->logpixels, d2->logpixels, "logpixels mismatch");
	torture_assert_int_equal(tctx, d1->bitsperpel, d2->bitsperpel, "bitsperpel mismatch");
	torture_assert_int_equal(tctx, d1->pelswidth, d2->pelswidth, "pelswidth mismatch");
	torture_assert_int_equal(tctx, d1->pelsheight, d2->pelsheight, "pelsheight mismatch");
	torture_assert_int_equal(tctx, d1->displayflags, d2->displayflags, "displayflags mismatch");
	torture_assert_int_equal(tctx, d1->displayfrequency, d2->displayfrequency, "displayfrequency mismatch");
	torture_assert_int_equal(tctx, d1->icmmethod, d2->icmmethod, "icmmethod mismatch");
	torture_assert_int_equal(tctx, d1->icmintent, d2->icmintent, "icmintent mismatch");
	torture_assert_int_equal(tctx, d1->mediatype, d2->mediatype, "mediatype mismatch");
	torture_assert_int_equal(tctx, d1->dithertype, d2->dithertype, "dithertype mismatch");
	torture_assert_int_equal(tctx, d1->reserved1, d2->reserved1, "reserved1 mismatch");
	torture_assert_int_equal(tctx, d1->reserved2, d2->reserved2, "reserved2 mismatch");
	torture_assert_int_equal(tctx, d1->panningwidth, d2->panningwidth, "panningwidth mismatch");
	torture_assert_int_equal(tctx, d1->panningheight, d2->panningheight, "panningheight mismatch");
	torture_assert_data_blob_equal(tctx, d1->driverextra_data, d2->driverextra_data, "driverextra_data mismatch");

	return true;
}

static bool test_devicemode_full(struct torture_context *tctx,
				 struct dcerpc_binding_handle *b,
				 struct policy_handle *handle)
{
	struct spoolss_SetPrinter s;
	struct spoolss_GetPrinter q;
	struct spoolss_GetPrinter q0;
	struct spoolss_SetPrinterInfoCtr info_ctr;
	struct spoolss_SetPrinterInfo8 info8;
	union spoolss_PrinterInfo info;
	struct spoolss_DevmodeContainer devmode_ctr;
	struct sec_desc_buf secdesc_ctr;
	uint32_t needed;
	bool ret = true;
	NTSTATUS status;

#define TEST_DEVMODE_INT_EXP_RESULT(lvl1, field1, lvl2, field2, value, exp_value, expected_result) do { \
		torture_comment(tctx, "field test %d/%s vs %d/%s\n", lvl1, #field1, lvl2, #field2); \
		q.in.level = lvl1; \
		TESTGETCALL(GetPrinter, q) \
		info_ctr.level = lvl1; \
		if (lvl1 == 2) {\
			info_ctr.info.info ## lvl1 = (struct spoolss_SetPrinterInfo ## lvl1 *)(void *)&q.out.info->info ## lvl1; \
		} else if (lvl1 == 8) {\
			info_ctr.info.info ## lvl1 = &info8; \
		}\
		devmode_ctr.devmode = q.out.info->info ## lvl1.devmode; \
		devmode_ctr.devmode->field1 = value; \
		TESTSETCALL_EXP(SetPrinter, s, expected_result) \
		if (W_ERROR_IS_OK(expected_result)) { \
			TESTGETCALL(GetPrinter, q) \
			INT_EQUAL(q.out.info->info ## lvl1.devmode->field1, exp_value, field1); \
			q.in.level = lvl2; \
			TESTGETCALL(GetPrinter, q) \
			INT_EQUAL(q.out.info->info ## lvl2.devmode->field2, exp_value, field1); \
		}\
	} while (0)

#define TEST_DEVMODE_INT_EXP(lvl1, field1, lvl2, field2, value, expected_result) do { \
        TEST_DEVMODE_INT_EXP_RESULT(lvl1, field1, lvl2, field2, value, value, expected_result); \
        } while (0)

#define TEST_DEVMODE_INT(lvl1, field1, lvl2, field2, value) do { \
        TEST_DEVMODE_INT_EXP_RESULT(lvl1, field1, lvl2, field2, value, value, WERR_OK); \
        } while (0)

	ZERO_STRUCT(devmode_ctr);
	ZERO_STRUCT(secdesc_ctr);
	ZERO_STRUCT(info8);

	s.in.handle = handle;
	s.in.command = 0;
	s.in.info_ctr = &info_ctr;
	s.in.devmode_ctr = &devmode_ctr;
	s.in.secdesc_ctr = &secdesc_ctr;

	q.in.handle = handle;
	q.out.info = &info;
	q0 = q;

#if 0
	const char *devicename;/* [charset(UTF16)] */
	enum spoolss_DeviceModeSpecVersion specversion;
	uint16_t driverversion;
	uint16_t __driverextra_length;/* [value(r->driverextra_data.length)] */
	uint32_t fields;
#endif
	TEST_DEVMODE_INT_EXP(8, size,		8, size, __LINE__, WERR_INVALID_PARAM);
	TEST_DEVMODE_INT_EXP(8, size,		8, size, 0, WERR_INVALID_PARAM);
	TEST_DEVMODE_INT_EXP(8, size,		8, size, 0xffff, WERR_INVALID_PARAM);
	TEST_DEVMODE_INT_EXP(8, size,		8, size, ndr_size_spoolss_DeviceMode(devmode_ctr.devmode, 0), (devmode_ctr.devmode->__driverextra_length > 0 ) ? WERR_INVALID_PARAM : WERR_OK);
	TEST_DEVMODE_INT(8, size,		8, size, ndr_size_spoolss_DeviceMode(devmode_ctr.devmode, 0) - devmode_ctr.devmode->__driverextra_length);

	devmode_ctr.devmode->driverextra_data = data_blob_string_const("foobar");
	torture_assert(tctx,
		test_devmode_set_level(tctx, b, handle, 8, devmode_ctr.devmode),
		"failed to set devmode");

	TEST_DEVMODE_INT_EXP(8, size,		8, size, ndr_size_spoolss_DeviceMode(devmode_ctr.devmode, 0), (devmode_ctr.devmode->__driverextra_length > 0 ) ? WERR_INVALID_PARAM : WERR_OK);
	TEST_DEVMODE_INT(8, size,		8, size, ndr_size_spoolss_DeviceMode(devmode_ctr.devmode, 0) - devmode_ctr.devmode->__driverextra_length);

	TEST_DEVMODE_INT(8, orientation,	8, orientation, __LINE__);
	TEST_DEVMODE_INT(8, papersize,		8, papersize, __LINE__);
	TEST_DEVMODE_INT(8, paperlength,	8, paperlength, __LINE__);
	TEST_DEVMODE_INT(8, paperwidth,		8, paperwidth, __LINE__);
	TEST_DEVMODE_INT(8, scale,		8, scale, __LINE__);
	TEST_DEVMODE_INT(8, copies,		8, copies, __LINE__);
	TEST_DEVMODE_INT(8, defaultsource,	8, defaultsource, __LINE__);
	TEST_DEVMODE_INT(8, printquality,	8, printquality, __LINE__);
	TEST_DEVMODE_INT(8, color,		8, color, __LINE__);
	TEST_DEVMODE_INT(8, duplex,		8, duplex, __LINE__);
	TEST_DEVMODE_INT(8, yresolution,	8, yresolution, __LINE__);
	TEST_DEVMODE_INT(8, ttoption,		8, ttoption, __LINE__);
	TEST_DEVMODE_INT(8, collate,		8, collate, __LINE__);
#if 0
	const char *formname;/* [charset(UTF16)] */
#endif
	TEST_DEVMODE_INT(8, logpixels,		8, logpixels, __LINE__);
	TEST_DEVMODE_INT(8, bitsperpel,		8, bitsperpel, __LINE__);
	TEST_DEVMODE_INT(8, pelswidth,		8, pelswidth, __LINE__);
	TEST_DEVMODE_INT(8, pelsheight,		8, pelsheight, __LINE__);
	TEST_DEVMODE_INT(8, displayflags,	8, displayflags, __LINE__);
	TEST_DEVMODE_INT(8, displayfrequency,	8, displayfrequency, __LINE__);
	TEST_DEVMODE_INT(8, icmmethod,		8, icmmethod, __LINE__);
	TEST_DEVMODE_INT(8, icmintent,		8, icmintent, __LINE__);
	TEST_DEVMODE_INT(8, mediatype,		8, mediatype, __LINE__);
	TEST_DEVMODE_INT(8, dithertype,		8, dithertype, __LINE__);
	TEST_DEVMODE_INT(8, reserved1,		8, reserved1, __LINE__);
	TEST_DEVMODE_INT(8, reserved2,		8, reserved2, __LINE__);
	TEST_DEVMODE_INT(8, panningwidth,	8, panningwidth, __LINE__);
	TEST_DEVMODE_INT(8, panningheight,	8, panningheight, __LINE__);

	return ret;
}

static bool call_OpenPrinterEx(struct torture_context *tctx,
			       struct dcerpc_pipe *p,
			       const char *name,
			       struct spoolss_DeviceMode *devmode,
			       struct policy_handle *handle);

static bool test_PrinterInfo_DevModes(struct torture_context *tctx,
				      struct dcerpc_pipe *p,
				      struct policy_handle *handle,
				      const char *name)
{
	union spoolss_PrinterInfo info;
	struct spoolss_DeviceMode *devmode;
	struct spoolss_DeviceMode *devmode2;
	struct policy_handle handle_devmode;
	struct dcerpc_binding_handle *b = p->binding_handle;

	/* simply compare level8 and level2 devmode */

	torture_assert(tctx, test_GetPrinter_level(tctx, b, handle, 8, &info), "");

	devmode = info.info8.devmode;

	if (devmode && devmode->size == 0) {
		torture_fail(tctx,
			"devmode of zero size!");
	}

	torture_assert(tctx, test_GetPrinter_level(tctx, b, handle, 2, &info), "");

	devmode2 = info.info2.devmode;

	if (devmode2 && devmode2->size == 0) {
		torture_fail(tctx,
			"devmode of zero size!");
	}

	torture_assert(tctx, test_devicemode_equal(tctx, devmode, devmode2),
		"DM level 8 != DM level 2");


	/* set devicemode level 8 and see if it persists */

	devmode->copies = 93;
	devmode->formname = talloc_strdup(tctx, "Legal");

	torture_assert(tctx, test_devmode_set_level(tctx, b, handle, 8, devmode), "");

	torture_assert(tctx, test_GetPrinter_level(tctx, b, handle, 8, &info), "");

	devmode2 = info.info8.devmode;

	torture_assert(tctx, test_devicemode_equal(tctx, devmode, devmode2),
		"modified DM level 8 != DM level 8 after DM has been set via level 8");

	torture_assert(tctx, test_GetPrinter_level(tctx, b, handle, 2, &info), "");

	devmode2 = info.info2.devmode;

	torture_assert(tctx, test_devicemode_equal(tctx, devmode, devmode2),
		"modified DM level 8 != DM level 2");


	/* set devicemode level 2 and see if it persists */

	devmode->copies = 39;
	devmode->formname = talloc_strdup(tctx, "Executive");

	torture_assert(tctx, test_devmode_set_level(tctx, b, handle, 2, devmode), "");

	torture_assert(tctx, test_GetPrinter_level(tctx, b, handle, 8, &info), "");

	devmode2 = info.info8.devmode;

	torture_assert(tctx, test_devicemode_equal(tctx, devmode, devmode2),
		"modified DM level 8 != DM level 8 after DM has been set via level 2");

	torture_assert(tctx, test_GetPrinter_level(tctx, b, handle, 2, &info), "");

	devmode2 = info.info2.devmode;

	torture_assert(tctx, test_devicemode_equal(tctx, devmode, devmode2),
		"modified DM level 8 != DM level 2");


	/* check every single bit in public part of devicemode */

	torture_assert(tctx, test_devicemode_full(tctx, b, handle),
		"failed to set every single devicemode component");


	/* change formname upon open and see if it persists in getprinter calls */

	devmode->formname = talloc_strdup(tctx, "A4");
	devmode->copies = 42;

	torture_assert(tctx, call_OpenPrinterEx(tctx, p, name, devmode, &handle_devmode),
		"failed to open printer handle");

	torture_assert(tctx, test_GetPrinter_level(tctx, b, &handle_devmode, 8, &info), "");

	devmode2 = info.info8.devmode;

	if (strequal(devmode->devicename, devmode2->devicename)) {
		torture_warning(tctx, "devicenames are the same\n");
	} else {
		torture_comment(tctx, "devicename passed in for open: %s\n", devmode->devicename);
		torture_comment(tctx, "devicename after level 8 get: %s\n", devmode2->devicename);
	}

	if (strequal(devmode->formname, devmode2->formname)) {
		torture_warning(tctx, "formname are the same\n");
	} else {
		torture_comment(tctx, "formname passed in for open: %s\n", devmode->formname);
		torture_comment(tctx, "formname after level 8 get: %s\n", devmode2->formname);
	}

	if (devmode->copies == devmode2->copies) {
		torture_warning(tctx, "copies are the same\n");
	} else {
		torture_comment(tctx, "copies passed in for open: %d\n", devmode->copies);
		torture_comment(tctx, "copies after level 8 get: %d\n", devmode2->copies);
	}

	torture_assert(tctx, test_GetPrinter_level(tctx, b, &handle_devmode, 2, &info), "");

	devmode2 = info.info2.devmode;

	if (strequal(devmode->devicename, devmode2->devicename)) {
		torture_warning(tctx, "devicenames are the same\n");
	} else {
		torture_comment(tctx, "devicename passed in for open: %s\n", devmode->devicename);
		torture_comment(tctx, "devicename after level 2 get: %s\n", devmode2->devicename);
	}

	if (strequal(devmode->formname, devmode2->formname)) {
		torture_warning(tctx, "formname is the same\n");
	} else {
		torture_comment(tctx, "formname passed in for open: %s\n", devmode->formname);
		torture_comment(tctx, "formname after level 2 get: %s\n", devmode2->formname);
	}

	if (devmode->copies == devmode2->copies) {
		torture_warning(tctx, "copies are the same\n");
	} else {
		torture_comment(tctx, "copies passed in for open: %d\n", devmode->copies);
		torture_comment(tctx, "copies after level 2 get: %d\n", devmode2->copies);
	}

	test_ClosePrinter(tctx, b, &handle_devmode);

	return true;
}

/*
 * wrapper call that saves original devmode, runs tests, and restores devmode
 */

static bool test_PrinterInfo_DevMode(struct torture_context *tctx,
				     struct dcerpc_pipe *p,
				     struct policy_handle *handle,
				     const char *name,
				     struct spoolss_DeviceMode *addprinter_devmode)
{
	union spoolss_PrinterInfo info;
	struct spoolss_DeviceMode *devmode;
	bool ret = true;
	struct dcerpc_binding_handle *b = p->binding_handle;

	torture_comment(tctx, "Testing Printer Devicemodes\n");

	/* save original devmode */

	torture_assert(tctx, test_GetPrinter_level(tctx, b, handle, 8, &info),
		"failed to get initial global devicemode");

	devmode = info.info8.devmode;

	if (devmode && devmode->size == 0) {
		torture_fail(tctx,
			"devmode of zero size!");
	}

	if (addprinter_devmode) {
		if (!test_devicemode_equal(tctx, devmode, addprinter_devmode)) {
			torture_warning(tctx, "current global DM is != DM provided in addprinter");
		}
	}

	/* run tests */

	ret = test_PrinterInfo_DevModes(tctx, p, handle, name);

	/* restore original devmode */

	torture_assert(tctx, test_devmode_set_level(tctx, b, handle, 8, devmode),
		"failed to restore initial global device mode");

	torture_comment(tctx, "Printer Devicemodes test %s\n\n",
		ret ? "succeeded" : "failed");


	return ret;
}

bool test_ClosePrinter(struct torture_context *tctx,
		       struct dcerpc_binding_handle *b,
		       struct policy_handle *handle)
{
	NTSTATUS status;
	struct spoolss_ClosePrinter r;

	r.in.handle = handle;
	r.out.handle = handle;

	torture_comment(tctx, "Testing ClosePrinter\n");

	status = dcerpc_spoolss_ClosePrinter_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "ClosePrinter failed");
	torture_assert_werr_ok(tctx, r.out.result, "ClosePrinter failed");

	return true;
}

static bool test_GetForm_args(struct torture_context *tctx,
			      struct dcerpc_binding_handle *b,
			      struct policy_handle *handle,
			      const char *form_name,
			      uint32_t level,
			      union spoolss_FormInfo *info_p)
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

	torture_comment(tctx, "Testing GetForm(%s) level %d\n", form_name, r.in.level);

	status = dcerpc_spoolss_GetForm_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "GetForm failed");

	if (W_ERROR_EQUAL(r.out.result, WERR_INSUFFICIENT_BUFFER)) {
		DATA_BLOB blob = data_blob_talloc_zero(tctx, needed);
		r.in.buffer = &blob;
		r.in.offered = needed;
		status = dcerpc_spoolss_GetForm_r(b, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "GetForm failed");

		torture_assert_werr_ok(tctx, r.out.result, "GetForm failed");

		torture_assert(tctx, r.out.info, "No form info returned");
	}

	torture_assert_werr_ok(tctx, r.out.result, "GetForm failed");

	CHECK_NEEDED_SIZE_LEVEL(spoolss_FormInfo, r.out.info, r.in.level, needed, 4);

	if (info_p) {
		*info_p = *r.out.info;
	}

	return true;
}

static bool test_GetForm(struct torture_context *tctx,
			 struct dcerpc_binding_handle *b,
			 struct policy_handle *handle,
			 const char *form_name,
			 uint32_t level)
{
	return test_GetForm_args(tctx, b, handle, form_name, level, NULL);
}

static bool test_EnumForms(struct torture_context *tctx,
			   struct dcerpc_binding_handle *b,
			   struct policy_handle *handle,
			   bool print_server,
			   uint32_t level,
			   uint32_t *count_p,
			   union spoolss_FormInfo **info_p)
{
	struct spoolss_EnumForms r;
	uint32_t needed;
	uint32_t count;
	union spoolss_FormInfo *info;

	r.in.handle = handle;
	r.in.level = level;
	r.in.buffer = NULL;
	r.in.offered = 0;
	r.out.needed = &needed;
	r.out.count = &count;
	r.out.info = &info;

	torture_comment(tctx, "Testing EnumForms level %d\n", r.in.level);

	torture_assert_ntstatus_ok(tctx,
		dcerpc_spoolss_EnumForms_r(b, tctx, &r),
		"EnumForms failed");

	if ((r.in.level == 2) && (W_ERROR_EQUAL(r.out.result, WERR_UNKNOWN_LEVEL))) {
		torture_skip(tctx, "EnumForms level 2 not supported");
	}

	if (print_server && W_ERROR_EQUAL(r.out.result, WERR_BADFID)) {
		torture_fail(tctx, "EnumForms on the PrintServer isn't supported by test server (NT4)");
	}

	if (W_ERROR_EQUAL(r.out.result, WERR_INSUFFICIENT_BUFFER)) {
		DATA_BLOB blob = data_blob_talloc_zero(tctx, needed);
		r.in.buffer = &blob;
		r.in.offered = needed;

		torture_assert_ntstatus_ok(tctx,
			dcerpc_spoolss_EnumForms_r(b, tctx, &r),
			"EnumForms failed");

		torture_assert(tctx, info, "No forms returned");
	}

	torture_assert_werr_ok(tctx, r.out.result, "EnumForms failed");

	CHECK_NEEDED_SIZE_ENUM_LEVEL(spoolss_EnumForms, info, r.in.level, count, needed, 4);

	if (info_p) {
		*info_p = info;
	}
	if (count_p) {
		*count_p = count;
	}

	return true;
}

static bool test_EnumForms_all(struct torture_context *tctx,
			       struct dcerpc_binding_handle *b,
			       struct policy_handle *handle,
			       bool print_server)
{
	uint32_t levels[] = { 1, 2 };
	int i, j;

	for (i=0; i<ARRAY_SIZE(levels); i++) {

		uint32_t count = 0;
		union spoolss_FormInfo *info = NULL;

		torture_assert(tctx,
			test_EnumForms(tctx, b, handle, print_server, levels[i], &count, &info),
			"failed to enum forms");

		for (j = 0; j < count; j++) {
			if (!print_server) {
				torture_assert(tctx,
					test_GetForm(tctx, b, handle, info[j].info1.form_name, levels[i]),
					"failed to get form");
			}
		}
	}

	return true;
}

static bool test_EnumForms_find_one(struct torture_context *tctx,
				    struct dcerpc_binding_handle *b,
				    struct policy_handle *handle,
				    bool print_server,
				    const char *form_name)
{
	union spoolss_FormInfo *info;
	uint32_t count;
	bool found = false;
	int i;

	torture_assert(tctx,
		test_EnumForms(tctx, b, handle, print_server, 1, &count, &info),
		"failed to enumerate forms");

	for (i=0; i<count; i++) {
		if (strequal(form_name, info[i].info1.form_name)) {
			found = true;
			break;
		}
	}

	return found;
}

static bool test_DeleteForm(struct torture_context *tctx,
			    struct dcerpc_binding_handle *b,
			    struct policy_handle *handle,
			    const char *form_name,
			    WERROR expected_result)
{
	struct spoolss_DeleteForm r;

	r.in.handle = handle;
	r.in.form_name = form_name;

	torture_comment(tctx, "Testing DeleteForm(%s)\n", form_name);

	torture_assert_ntstatus_ok(tctx,
		dcerpc_spoolss_DeleteForm_r(b, tctx, &r),
		"DeleteForm failed");
	torture_assert_werr_equal(tctx, r.out.result, expected_result,
		"DeleteForm gave unexpected result");
	if (W_ERROR_IS_OK(r.out.result)) {
		torture_assert_ntstatus_ok(tctx,
			dcerpc_spoolss_DeleteForm_r(b, tctx, &r),
			"2nd DeleteForm failed");
		torture_assert_werr_equal(tctx, r.out.result, WERR_INVALID_FORM_NAME,
			"2nd DeleteForm failed");
	}

	return true;
}

static bool test_AddForm(struct torture_context *tctx,
			 struct dcerpc_binding_handle *b,
			 struct policy_handle *handle,
			 uint32_t level,
			 union spoolss_AddFormInfo *info,
			 WERROR expected_result)
{
	struct spoolss_AddForm r;

	if (level != 1) {
		torture_skip(tctx, "only level 1 supported");
	}

	r.in.handle	= handle;
	r.in.level	= level;
	r.in.info	= *info;

	torture_comment(tctx, "Testing AddForm(%s) level %d, type %d\n",
		r.in.info.info1->form_name, r.in.level,
		r.in.info.info1->flags);

	torture_assert_ntstatus_ok(tctx, dcerpc_spoolss_AddForm_r(b, tctx, &r),
		"AddForm failed");
	torture_assert_werr_equal(tctx, r.out.result, expected_result,
		"AddForm gave unexpected result");

	torture_assert_ntstatus_ok(tctx, dcerpc_spoolss_AddForm_r(b, tctx, &r),
		"2nd AddForm failed");
	if (W_ERROR_EQUAL(expected_result, WERR_INVALID_PARAM)) {
		torture_assert_werr_equal(tctx, r.out.result, WERR_INVALID_PARAM,
			"2nd AddForm gave unexpected result");
	} else {
		torture_assert_werr_equal(tctx, r.out.result, WERR_FILE_EXISTS,
			"2nd AddForm gave unexpected result");
	}

	return true;
}

static bool test_SetForm(struct torture_context *tctx,
			 struct dcerpc_binding_handle *b,
			 struct policy_handle *handle,
			 const char *form_name,
			 uint32_t level,
			 union spoolss_AddFormInfo *info)
{
	struct spoolss_SetForm r;

	r.in.handle	= handle;
	r.in.form_name	= form_name;
	r.in.level	= level;
	r.in.info	= *info;

	torture_comment(tctx, "Testing SetForm(%s) level %d\n",
		form_name, r.in.level);

	torture_assert_ntstatus_ok(tctx, dcerpc_spoolss_SetForm_r(b, tctx, &r),
		"SetForm failed");

	torture_assert_werr_ok(tctx, r.out.result,
		"SetForm failed");

	return true;
}

static bool test_GetForm_winreg(struct torture_context *tctx,
			        struct dcerpc_binding_handle *b,
				struct policy_handle *handle,
				const char *key_name,
				const char *form_name,
				enum winreg_Type *w_type,
				uint32_t *w_size,
				uint32_t *w_length,
				uint8_t **w_data);

static bool test_Forms_args(struct torture_context *tctx,
			    struct dcerpc_binding_handle *b,
			    struct policy_handle *handle,
			    bool print_server,
			    const char *printer_name,
			    struct dcerpc_binding_handle *winreg_handle,
			    struct policy_handle *hive_handle,
			    const char *form_name,
			    struct spoolss_AddFormInfo1 *info1,
			    WERROR expected_add_result,
			    WERROR expected_delete_result)
{
	union spoolss_FormInfo info;
	union spoolss_AddFormInfo add_info;

	enum winreg_Type w_type;
	uint32_t w_size;
	uint32_t w_length;
	uint8_t *w_data;

	add_info.info1 = info1;

	torture_assert(tctx,
		test_AddForm(tctx, b, handle, 1, &add_info, expected_add_result),
		"failed to add form");

	if (winreg_handle && hive_handle && W_ERROR_IS_OK(expected_add_result)) {

		struct spoolss_FormInfo1 i1;

		torture_assert(tctx,
			test_GetForm_winreg(tctx, winreg_handle, hive_handle, TOP_LEVEL_CONTROL_FORMS_KEY, form_name, &w_type, &w_size, &w_length, &w_data),
			"failed to get form via winreg");

		i1.size.width	= IVAL(w_data, 0);
		i1.size.height	= IVAL(w_data, 4);
		i1.area.left	= IVAL(w_data, 8);
		i1.area.top	= IVAL(w_data, 12);
		i1.area.right	= IVAL(w_data, 16);
		i1.area.bottom	= IVAL(w_data, 20);
		/* skip index here */
		i1.flags	= IVAL(w_data, 28);

		torture_assert_int_equal(tctx, w_type, REG_BINARY, "unexpected type");
		torture_assert_int_equal(tctx, w_size, 0x20, "unexpected size");
		torture_assert_int_equal(tctx, w_length, 0x20, "unexpected length");
		torture_assert_int_equal(tctx, i1.size.width, add_info.info1->size.width, "width mismatch");
		torture_assert_int_equal(tctx, i1.size.height, add_info.info1->size.height, "height mismatch");
		torture_assert_int_equal(tctx, i1.area.left, add_info.info1->area.left, "left mismatch");
		torture_assert_int_equal(tctx, i1.area.top, add_info.info1->area.top, "top mismatch");
		torture_assert_int_equal(tctx, i1.area.right, add_info.info1->area.right, "right mismatch");
		torture_assert_int_equal(tctx, i1.area.bottom, add_info.info1->area.bottom, "bottom mismatch");
		torture_assert_int_equal(tctx, i1.flags, add_info.info1->flags, "flags mismatch");
	}

	if (!print_server && W_ERROR_IS_OK(expected_add_result)) {
		torture_assert(tctx,
			test_GetForm_args(tctx, b, handle, form_name, 1, &info),
			"failed to get added form");

		torture_assert_int_equal(tctx, info.info1.size.width, add_info.info1->size.width, "width mismatch");
		torture_assert_int_equal(tctx, info.info1.size.height, add_info.info1->size.height, "height mismatch");
		torture_assert_int_equal(tctx, info.info1.area.left, add_info.info1->area.left, "left mismatch");
		torture_assert_int_equal(tctx, info.info1.area.top, add_info.info1->area.top, "top mismatch");
		torture_assert_int_equal(tctx, info.info1.area.right, add_info.info1->area.right, "right mismatch");
		torture_assert_int_equal(tctx, info.info1.area.bottom, add_info.info1->area.bottom, "bottom mismatch");
		torture_assert_int_equal(tctx, info.info1.flags, add_info.info1->flags, "flags mismatch");

		if (winreg_handle && hive_handle) {

			struct spoolss_FormInfo1 i1;

			i1.size.width	= IVAL(w_data, 0);
			i1.size.height	= IVAL(w_data, 4);
			i1.area.left	= IVAL(w_data, 8);
			i1.area.top	= IVAL(w_data, 12);
			i1.area.right	= IVAL(w_data, 16);
			i1.area.bottom	= IVAL(w_data, 20);
			/* skip index here */
			i1.flags	= IVAL(w_data, 28);

			torture_assert_int_equal(tctx, i1.size.width, info.info1.size.width, "width mismatch");
			torture_assert_int_equal(tctx, i1.size.height, info.info1.size.height, "height mismatch");
			torture_assert_int_equal(tctx, i1.area.left, info.info1.area.left, "left mismatch");
			torture_assert_int_equal(tctx, i1.area.top, info.info1.area.top, "top mismatch");
			torture_assert_int_equal(tctx, i1.area.right, info.info1.area.right, "right mismatch");
			torture_assert_int_equal(tctx, i1.area.bottom, info.info1.area.bottom, "bottom mismatch");
			torture_assert_int_equal(tctx, i1.flags, info.info1.flags, "flags mismatch");
		}

		add_info.info1->size.width = 1234;

		torture_assert(tctx,
			test_SetForm(tctx, b, handle, form_name, 1, &add_info),
			"failed to set form");
		torture_assert(tctx,
			test_GetForm_args(tctx, b, handle, form_name, 1, &info),
			"failed to get setted form");

		torture_assert_int_equal(tctx, info.info1.size.width, add_info.info1->size.width, "width mismatch");
	}

	if (!W_ERROR_EQUAL(expected_add_result, WERR_INVALID_PARAM)) {
		torture_assert(tctx,
			test_EnumForms_find_one(tctx, b, handle, print_server, form_name),
			"Newly added form not found in enum call");
	}

	torture_assert(tctx,
		test_DeleteForm(tctx, b, handle, form_name, expected_delete_result),
		"failed to delete form");

	return true;
}

static bool test_Forms(struct torture_context *tctx,
		       struct dcerpc_binding_handle *b,
		       struct policy_handle *handle,
		       bool print_server,
		       const char *printer_name,
		       struct dcerpc_binding_handle *winreg_handle,
		       struct policy_handle *hive_handle)
{
	const struct spoolss_FormSize size = {
		.width	= 50,
		.height	= 25
	};
	const struct spoolss_FormArea area = {
		.left	= 5,
		.top	= 10,
		.right	= 45,
		.bottom	= 15
	};
	int i;

	struct {
		struct spoolss_AddFormInfo1 info1;
		WERROR expected_add_result;
		WERROR expected_delete_result;
	} forms[] = {
		{
			.info1 = {
				.flags		= SPOOLSS_FORM_USER,
				.form_name	= "testform_user",
				.size		= size,
				.area		= area,
			},
			.expected_add_result	= WERR_OK,
			.expected_delete_result	= WERR_OK
		},
/*
		weird, we can add a builtin form but we can never remove it
		again - gd

		{
			.info1 = {
				.flags		= SPOOLSS_FORM_BUILTIN,
				.form_name	= "testform_builtin",
				.size		= size,
				.area		= area,
			},
			.expected_add_result	= WERR_OK,
			.expected_delete_result	= WERR_INVALID_PARAM,
		},
*/
		{
			.info1 = {
				.flags		= SPOOLSS_FORM_PRINTER,
				.form_name	= "testform_printer",
				.size		= size,
				.area		= area,
			},
			.expected_add_result	= WERR_OK,
			.expected_delete_result	= WERR_OK
		},
		{
			.info1 = {
				.flags		= SPOOLSS_FORM_USER,
				.form_name	= "Letter",
				.size		= size,
				.area		= area,
			},
			.expected_add_result	= WERR_FILE_EXISTS,
			.expected_delete_result	= WERR_INVALID_PARAM
		},
		{
			.info1 = {
				.flags		= SPOOLSS_FORM_BUILTIN,
				.form_name	= "Letter",
				.size		= size,
				.area		= area,
			},
			.expected_add_result	= WERR_FILE_EXISTS,
			.expected_delete_result	= WERR_INVALID_PARAM
		},
		{
			.info1 = {
				.flags		= SPOOLSS_FORM_PRINTER,
				.form_name	= "Letter",
				.size		= size,
				.area		= area,
			},
			.expected_add_result	= WERR_FILE_EXISTS,
			.expected_delete_result	= WERR_INVALID_PARAM
		},
		{
			.info1 = {
				.flags		= 12345,
				.form_name	= "invalid_flags",
				.size		= size,
				.area		= area,
			},
			.expected_add_result	= WERR_INVALID_PARAM,
			.expected_delete_result	= WERR_INVALID_FORM_NAME
		}

	};

	for (i=0; i < ARRAY_SIZE(forms); i++) {
		torture_assert(tctx,
			test_Forms_args(tctx, b, handle, print_server, printer_name,
					winreg_handle, hive_handle,
					forms[i].info1.form_name,
					&forms[i].info1,
					forms[i].expected_add_result,
					forms[i].expected_delete_result),
			talloc_asprintf(tctx, "failed to test form '%s'", forms[i].info1.form_name));
	}

	return true;
}

static bool test_EnumPorts_old(struct torture_context *tctx,
			       void *private_data)
{
	struct test_spoolss_context *ctx =
		talloc_get_type_abort(private_data, struct test_spoolss_context);

	NTSTATUS status;
	struct spoolss_EnumPorts r;
	uint32_t needed;
	uint32_t count;
	union spoolss_PortInfo *info;
	struct dcerpc_pipe *p = ctx->spoolss_pipe;
	struct dcerpc_binding_handle *b = p->binding_handle;

	r.in.servername = talloc_asprintf(tctx, "\\\\%s",
					  dcerpc_server_name(p));
	r.in.level = 2;
	r.in.buffer = NULL;
	r.in.offered = 0;
	r.out.needed = &needed;
	r.out.count = &count;
	r.out.info = &info;

	torture_comment(tctx, "Testing EnumPorts\n");

	status = dcerpc_spoolss_EnumPorts_r(b, tctx, &r);

	torture_assert_ntstatus_ok(tctx, status, "EnumPorts failed");

	if (W_ERROR_EQUAL(r.out.result, WERR_INSUFFICIENT_BUFFER)) {
		DATA_BLOB blob = data_blob_talloc_zero(tctx, needed);
		r.in.buffer = &blob;
		r.in.offered = needed;

		status = dcerpc_spoolss_EnumPorts_r(b, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "EnumPorts failed");
		torture_assert_werr_ok(tctx, r.out.result, "EnumPorts failed");

		torture_assert(tctx, info, "No ports returned");
	}

	torture_assert_werr_ok(tctx, r.out.result, "EnumPorts failed");

	CHECK_NEEDED_SIZE_ENUM_LEVEL(spoolss_EnumPorts, info, 2, count, needed, 4);

	return true;
}

static bool test_AddPort(struct torture_context *tctx,
			 void *private_data)
{
	struct test_spoolss_context *ctx =
		talloc_get_type_abort(private_data, struct test_spoolss_context);

	NTSTATUS status;
	struct spoolss_AddPort r;
	struct dcerpc_pipe *p = ctx->spoolss_pipe;
	struct dcerpc_binding_handle *b = p->binding_handle;

	r.in.server_name = talloc_asprintf(tctx, "\\\\%s",
					   dcerpc_server_name(p));
	r.in.unknown = 0;
	r.in.monitor_name = "foo";

	torture_comment(tctx, "Testing AddPort\n");

	status = dcerpc_spoolss_AddPort_r(b, tctx, &r);

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

static bool test_GetJob_args(struct torture_context *tctx,
			     struct dcerpc_binding_handle *b,
			     struct policy_handle *handle,
			     uint32_t job_id,
			     uint32_t level,
			     union spoolss_JobInfo *info_p)
{
	NTSTATUS status;
	struct spoolss_GetJob r;
	union spoolss_JobInfo info;
	uint32_t needed;

	r.in.handle = handle;
	r.in.job_id = job_id;
	r.in.level = level;
	r.in.buffer = NULL;
	r.in.offered = 0;
	r.out.needed = &needed;
	r.out.info = &info;

	torture_comment(tctx, "Testing GetJob(%d), level %d\n", job_id, r.in.level);

	status = dcerpc_spoolss_GetJob_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "GetJob failed");
	if (level == 0) {
		torture_assert_werr_equal(tctx, r.out.result, WERR_UNKNOWN_LEVEL, "Unexpected return code");
	}

	if (W_ERROR_EQUAL(r.out.result, WERR_INSUFFICIENT_BUFFER)) {
		DATA_BLOB blob = data_blob_talloc_zero(tctx, needed);
		r.in.buffer = &blob;
		r.in.offered = needed;

		status = dcerpc_spoolss_GetJob_r(b, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "GetJob failed");
	}

	torture_assert_werr_ok(tctx, r.out.result, "GetJob failed");
	torture_assert(tctx, r.out.info, "No job info returned");

	CHECK_NEEDED_SIZE_LEVEL(spoolss_JobInfo, r.out.info, r.in.level, needed, 4);

	if (info_p) {
		*info_p = *r.out.info;
	}

	return true;
}

static bool test_GetJob(struct torture_context *tctx,
			struct dcerpc_binding_handle *b,
			struct policy_handle *handle,
			uint32_t job_id)
{
	uint32_t levels[] = {0, 1, 2 /* 3, 4 */};
	uint32_t i;

	for (i=0; i < ARRAY_SIZE(levels); i++) {
		torture_assert(tctx,
			test_GetJob_args(tctx, b, handle, job_id, levels[i], NULL),
			"GetJob failed");
	}

	return true;
}

static bool test_SetJob(struct torture_context *tctx,
			struct dcerpc_binding_handle *b,
			struct policy_handle *handle,
			uint32_t job_id,
			struct spoolss_JobInfoContainer *ctr,
			enum spoolss_JobControl command)
{
	NTSTATUS status;
	struct spoolss_SetJob r;

	r.in.handle	= handle;
	r.in.job_id	= job_id;
	r.in.ctr	= ctr;
	r.in.command	= command;

	switch (command) {
	case SPOOLSS_JOB_CONTROL_PAUSE:
		torture_comment(tctx, "Testing SetJob(%d), SPOOLSS_JOB_CONTROL_PAUSE\n", job_id);
		break;
	case SPOOLSS_JOB_CONTROL_RESUME:
		torture_comment(tctx, "Testing SetJob(%d), SPOOLSS_JOB_CONTROL_RESUME\n", job_id);
		break;
	case SPOOLSS_JOB_CONTROL_CANCEL:
		torture_comment(tctx, "Testing SetJob(%d), SPOOLSS_JOB_CONTROL_CANCEL\n", job_id);
		break;
	case SPOOLSS_JOB_CONTROL_RESTART:
		torture_comment(tctx, "Testing SetJob(%d), SPOOLSS_JOB_CONTROL_RESTART\n", job_id);
		break;
	case SPOOLSS_JOB_CONTROL_DELETE:
		torture_comment(tctx, "Testing SetJob(%d), SPOOLSS_JOB_CONTROL_DELETE\n", job_id);
		break;
	case SPOOLSS_JOB_CONTROL_SEND_TO_PRINTER:
		torture_comment(tctx, "Testing SetJob(%d), SPOOLSS_JOB_CONTROL_SEND_TO_PRINTER\n", job_id);
		break;
	case SPOOLSS_JOB_CONTROL_LAST_PAGE_EJECTED:
		torture_comment(tctx, "Testing SetJob(%d), SPOOLSS_JOB_CONTROL_LAST_PAGE_EJECTED\n", job_id);
		break;
	case SPOOLSS_JOB_CONTROL_RETAIN:
		torture_comment(tctx, "Testing SetJob(%d), SPOOLSS_JOB_CONTROL_RETAIN\n", job_id);
		break;
	case SPOOLSS_JOB_CONTROL_RELEASE:
		torture_comment(tctx, "Testing SetJob(%d), SPOOLSS_JOB_CONTROL_RELEASE\n", job_id);
		break;
	default:
		torture_comment(tctx, "Testing SetJob(%d)\n", job_id);
		break;
	}

	status = dcerpc_spoolss_SetJob_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "SetJob failed");
	torture_assert_werr_ok(tctx, r.out.result, "SetJob failed");

	return true;
}

static bool test_AddJob(struct torture_context *tctx,
			struct dcerpc_binding_handle *b,
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

	status = dcerpc_spoolss_AddJob_r(b, tctx, &r);
	torture_assert_werr_equal(tctx, r.out.result, WERR_UNKNOWN_LEVEL, "AddJob failed");

	r.in.level = 1;

	status = dcerpc_spoolss_AddJob_r(b, tctx, &r);
	torture_assert_werr_equal(tctx, r.out.result, WERR_INVALID_PARAM, "AddJob failed");

	return true;
}


static bool test_EnumJobs_args(struct torture_context *tctx,
			       struct dcerpc_binding_handle *b,
			       struct policy_handle *handle,
			       uint32_t level,
			       uint32_t *count_p,
			       union spoolss_JobInfo **info_p)
{
	NTSTATUS status;
	struct spoolss_EnumJobs r;
	uint32_t needed;
	uint32_t count;
	union spoolss_JobInfo *info;

	r.in.handle = handle;
	r.in.firstjob = 0;
	r.in.numjobs = 0xffffffff;
	r.in.level = level;
	r.in.buffer = NULL;
	r.in.offered = 0;
	r.out.needed = &needed;
	r.out.count = &count;
	r.out.info = &info;

	torture_comment(tctx, "Testing EnumJobs level %d\n", level);

	status = dcerpc_spoolss_EnumJobs_r(b, tctx, &r);

	torture_assert_ntstatus_ok(tctx, status, "EnumJobs failed");

	if (W_ERROR_EQUAL(r.out.result, WERR_INSUFFICIENT_BUFFER)) {
		DATA_BLOB blob = data_blob_talloc_zero(tctx, needed);
		r.in.buffer = &blob;
		r.in.offered = needed;

		status = dcerpc_spoolss_EnumJobs_r(b, tctx, &r);

		torture_assert_ntstatus_ok(tctx, status, "EnumJobs failed");
		torture_assert_werr_ok(tctx, r.out.result, "EnumJobs failed");
		torture_assert(tctx, info, "No jobs returned");

		CHECK_NEEDED_SIZE_ENUM_LEVEL(spoolss_EnumJobs, *r.out.info, r.in.level, count, needed, 4);

	} else {
		torture_assert_werr_ok(tctx, r.out.result, "EnumJobs failed");
	}

	if (count_p) {
		*count_p = count;
	}
	if (info_p) {
		*info_p = info;
	}

	return true;
}

static bool test_DoPrintTest_add_one_job(struct torture_context *tctx,
					 struct dcerpc_binding_handle *b,
					 struct policy_handle *handle,
					 const char *document_name,
					 uint32_t *job_id)
{
	NTSTATUS status;
	struct spoolss_StartDocPrinter s;
	struct spoolss_DocumentInfo1 info1;
	struct spoolss_StartPagePrinter sp;
	struct spoolss_WritePrinter w;
	struct spoolss_EndPagePrinter ep;
	struct spoolss_EndDocPrinter e;
	int i;
	uint32_t num_written;

	torture_comment(tctx, "Testing StartDocPrinter\n");

	s.in.handle		= handle;
	s.in.level		= 1;
	s.in.info.info1		= &info1;
	s.out.job_id		= job_id;
	info1.document_name	= document_name;
	info1.output_file	= NULL;
	info1.datatype		= "RAW";

	status = dcerpc_spoolss_StartDocPrinter_r(b, tctx, &s);
	torture_assert_ntstatus_ok(tctx, status, "dcerpc_spoolss_StartDocPrinter failed");
	torture_assert_werr_ok(tctx, s.out.result, "StartDocPrinter failed");

	for (i=1; i < 4; i++) {
		torture_comment(tctx, "Testing StartPagePrinter: Page[%d], JobId[%d]\n", i, *job_id);

		sp.in.handle		= handle;

		status = dcerpc_spoolss_StartPagePrinter_r(b, tctx, &sp);
		torture_assert_ntstatus_ok(tctx, status,
					   "dcerpc_spoolss_StartPagePrinter failed");
		torture_assert_werr_ok(tctx, sp.out.result, "StartPagePrinter failed");

		torture_comment(tctx, "Testing WritePrinter: Page[%d], JobId[%d]\n", i, *job_id);

		w.in.handle		= handle;
		w.in.data		= data_blob_string_const(talloc_asprintf(tctx,"TortureTestPage: %d\nData\n",i));
		w.out.num_written	= &num_written;

		status = dcerpc_spoolss_WritePrinter_r(b, tctx, &w);
		torture_assert_ntstatus_ok(tctx, status, "dcerpc_spoolss_WritePrinter failed");
		torture_assert_werr_ok(tctx, w.out.result, "WritePrinter failed");

		torture_comment(tctx, "Testing EndPagePrinter: Page[%d], JobId[%d]\n", i, *job_id);

		ep.in.handle		= handle;

		status = dcerpc_spoolss_EndPagePrinter_r(b, tctx, &ep);
		torture_assert_ntstatus_ok(tctx, status, "dcerpc_spoolss_EndPagePrinter failed");
		torture_assert_werr_ok(tctx, ep.out.result, "EndPagePrinter failed");
	}

	torture_comment(tctx, "Testing EndDocPrinter: JobId[%d]\n", *job_id);

	e.in.handle = handle;

	status = dcerpc_spoolss_EndDocPrinter_r(b, tctx, &e);
	torture_assert_ntstatus_ok(tctx, status, "dcerpc_spoolss_EndDocPrinter failed");
	torture_assert_werr_ok(tctx, e.out.result, "EndDocPrinter failed");

	return true;
}

static bool test_DoPrintTest_check_jobs(struct torture_context *tctx,
					struct dcerpc_binding_handle *b,
					struct policy_handle *handle,
					uint32_t num_jobs,
					uint32_t *job_ids)
{
	uint32_t count;
	union spoolss_JobInfo *info = NULL;
	int i;

	torture_assert(tctx,
		test_AddJob(tctx, b, handle),
		"AddJob failed");

	torture_assert(tctx,
		test_EnumJobs_args(tctx, b, handle, 1, &count, &info),
		"EnumJobs level 1 failed");

	torture_assert_int_equal(tctx, count, num_jobs, "unexpected number of jobs in queue");

	for (i=0; i < num_jobs; i++) {
		union spoolss_JobInfo ginfo;
		const char *document_name;
		const char *new_document_name = "any_other_docname";
		struct spoolss_JobInfoContainer ctr;
		struct spoolss_SetJobInfo1 info1;

		torture_assert_int_equal(tctx, info[i].info1.job_id, job_ids[i], "job id mismatch");

		torture_assert(tctx,
			test_GetJob_args(tctx, b, handle, info[i].info1.job_id, 1, &ginfo),
			"failed to call test_GetJob");

		torture_assert_int_equal(tctx, ginfo.info1.job_id, info[i].info1.job_id, "job id mismatch");

		document_name = ginfo.info1.document_name;

		info1.job_id		= ginfo.info1.job_id;
		info1.printer_name	= ginfo.info1.printer_name;
		info1.server_name	= ginfo.info1.server_name;
		info1.user_name		= ginfo.info1.user_name;
		info1.document_name	= new_document_name;
		info1.data_type		= ginfo.info1.data_type;
		info1.text_status	= ginfo.info1.text_status;
		info1.status		= ginfo.info1.status;
		info1.priority		= ginfo.info1.priority;
		info1.position		= ginfo.info1.position;
		info1.total_pages	= ginfo.info1.total_pages;
		info1.pages_printed	= ginfo.info1.pages_printed;
		info1.submitted		= ginfo.info1.submitted;

		ctr.level = 1;
		ctr.info.info1 = &info1;

		torture_assert(tctx,
			test_SetJob(tctx, b, handle, info[i].info1.job_id, &ctr, 0),
			"failed to call test_SetJob level 1");

		torture_assert(tctx,
			test_GetJob_args(tctx, b, handle, info[i].info1.job_id, 1, &ginfo),
			"failed to call test_GetJob");

		if (strequal(ginfo.info1.document_name, document_name)) {
			torture_warning(tctx,
					"document_name did *NOT* change from '%s' to '%s'\n",
					document_name, new_document_name);
		}
	}

	for (i=0; i < num_jobs; i++) {
		if (!test_SetJob(tctx, b, handle, info[i].info1.job_id, NULL, SPOOLSS_JOB_CONTROL_PAUSE)) {
			torture_warning(tctx, "failed to pause printjob\n");
		}
		if (!test_SetJob(tctx, b, handle, info[i].info1.job_id, NULL, SPOOLSS_JOB_CONTROL_RESUME)) {
			torture_warning(tctx, "failed to resume printjob\n");
		}
	}

	return true;
}

static bool test_DoPrintTest(struct torture_context *tctx,
			     struct dcerpc_binding_handle *b,
			     struct policy_handle *handle)
{
	bool ret = true;
	uint32_t num_jobs = 8;
	uint32_t *job_ids;
	int i;

	torture_comment(tctx, "Testing real print operations\n");

	job_ids = talloc_zero_array(tctx, uint32_t, num_jobs);

	for (i=0; i < num_jobs; i++) {
		ret &= test_DoPrintTest_add_one_job(tctx, b, handle, "TorturePrintJob", &job_ids[i]);
	}

	for (i=0; i < num_jobs; i++) {
		ret &= test_SetJob(tctx, b, handle, job_ids[i], NULL, SPOOLSS_JOB_CONTROL_DELETE);
	}

	if (ret == true) {
		torture_comment(tctx, "real print operations test succeeded\n\n");
	}

	return ret;
}

static bool test_DoPrintTest_extended(struct torture_context *tctx,
				      struct dcerpc_binding_handle *b,
				      struct policy_handle *handle)
{
	bool ret = true;
	uint32_t num_jobs = 8;
	uint32_t *job_ids;
	int i;
	torture_comment(tctx, "Testing real print operations (extended)\n");

	job_ids = talloc_zero_array(tctx, uint32_t, num_jobs);

	for (i=0; i < num_jobs; i++) {
		ret &= test_DoPrintTest_add_one_job(tctx, b, handle, "TorturePrintJob", &job_ids[i]);
	}

	ret &= test_DoPrintTest_check_jobs(tctx, b, handle, num_jobs, job_ids);

	for (i=0; i < num_jobs; i++) {
		ret &= test_SetJob(tctx, b, handle, job_ids[i], NULL, SPOOLSS_JOB_CONTROL_DELETE);
	}

	if (ret == true) {
		torture_comment(tctx, "real print operations (extended) test succeeded\n\n");
	}

	return ret;
}

static bool test_PausePrinter(struct torture_context *tctx,
			      struct dcerpc_binding_handle *b,
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

	status = dcerpc_spoolss_SetPrinter_r(b, tctx, &r);

	torture_assert_ntstatus_ok(tctx, status, "SetPrinter failed");

	torture_assert_werr_ok(tctx, r.out.result, "SetPrinter failed");

	return true;
}

static bool test_ResumePrinter(struct torture_context *tctx,
			       struct dcerpc_binding_handle *b,
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

	status = dcerpc_spoolss_SetPrinter_r(b, tctx, &r);

	torture_assert_ntstatus_ok(tctx, status, "SetPrinter failed");

	torture_assert_werr_ok(tctx, r.out.result, "SetPrinter failed");

	return true;
}

static bool test_GetPrinterData_checktype(struct torture_context *tctx,
					  struct dcerpc_binding_handle *b,
					  struct policy_handle *handle,
					  const char *value_name,
					  enum winreg_Type *expected_type,
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

	status = dcerpc_spoolss_GetPrinterData_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "GetPrinterData failed");

	if (W_ERROR_EQUAL(r.out.result, WERR_MORE_DATA)) {
		if (expected_type) {
			torture_assert_int_equal(tctx, type, *expected_type, "unexpected type");
		}
		r.in.offered = needed;
		r.out.data = talloc_zero_array(tctx, uint8_t, r.in.offered);
		status = dcerpc_spoolss_GetPrinterData_r(b, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "GetPrinterData failed");
	}

	torture_assert_werr_ok(tctx, r.out.result,
		talloc_asprintf(tctx, "GetPrinterData(%s) failed", r.in.value_name));

	CHECK_NEEDED_SIZE_LEVEL(spoolss_PrinterData, &data, type, needed, 1);

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

static bool test_GetPrinterData(struct torture_context *tctx,
				struct dcerpc_binding_handle *b,
				struct policy_handle *handle,
				const char *value_name,
				enum winreg_Type *type_p,
				uint8_t **data_p,
				uint32_t *needed_p)
{
	return test_GetPrinterData_checktype(tctx, b, handle, value_name,
					     NULL, type_p, data_p, needed_p);
}

static bool test_GetPrinterDataEx_checktype(struct torture_context *tctx,
					    struct dcerpc_pipe *p,
					    struct policy_handle *handle,
					    const char *key_name,
					    const char *value_name,
					    enum winreg_Type *expected_type,
					    enum winreg_Type *type_p,
					    uint8_t **data_p,
					    uint32_t *needed_p)
{
	NTSTATUS status;
	struct spoolss_GetPrinterDataEx r;
	enum winreg_Type type;
	uint32_t needed;
	union spoolss_PrinterData data;
	struct dcerpc_binding_handle *b = p->binding_handle;

	r.in.handle = handle;
	r.in.key_name = key_name;
	r.in.value_name = value_name;
	r.in.offered = 0;
	r.out.type = &type;
	r.out.needed = &needed;
	r.out.data = talloc_zero_array(tctx, uint8_t, r.in.offered);

	torture_comment(tctx, "Testing GetPrinterDataEx(%s - %s)\n",
		r.in.key_name, r.in.value_name);

	status = dcerpc_spoolss_GetPrinterDataEx_r(b, tctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		if (NT_STATUS_EQUAL(status, NT_STATUS_RPC_PROCNUM_OUT_OF_RANGE)) {
			torture_skip(tctx, "GetPrinterDataEx not supported by server\n");
		}
		torture_assert_ntstatus_ok(tctx, status, "GetPrinterDataEx failed");
	}

	if (W_ERROR_EQUAL(r.out.result, WERR_MORE_DATA)) {
		if (expected_type) {
			torture_assert_int_equal(tctx, type, *expected_type, "unexpected type");
		}
		r.in.offered = needed;
		r.out.data = talloc_zero_array(tctx, uint8_t, r.in.offered);
		status = dcerpc_spoolss_GetPrinterDataEx_r(b, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "GetPrinterDataEx failed");
	}

	torture_assert_werr_ok(tctx, r.out.result,
		talloc_asprintf(tctx, "GetPrinterDataEx(%s - %s) failed", r.in.key_name, r.in.value_name));

	CHECK_NEEDED_SIZE_LEVEL(spoolss_PrinterData, &data, type, needed, 1);

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
	return test_GetPrinterDataEx_checktype(tctx, p, handle, key_name, value_name,
					       NULL, type_p, data_p, needed_p);
}

static bool test_get_environment(struct torture_context *tctx,
				 struct dcerpc_binding_handle *b,
				 struct policy_handle *handle,
				 const char **architecture)
{
	DATA_BLOB blob;
	enum winreg_Type type;
	uint8_t *data;
	uint32_t needed;

	torture_assert(tctx,
		test_GetPrinterData(tctx, b, handle, "Architecture", &type, &data, &needed),
		"failed to get Architecture");

	torture_assert_int_equal(tctx, type, REG_SZ, "unexpected type");

	blob = data_blob_const(data, needed);
	*architecture = reg_val_data_string(tctx, REG_SZ, blob);

	return true;
}

static bool test_GetPrinterData_list(struct torture_context *tctx,
				     void *private_data)
{
	struct test_spoolss_context *ctx =
		talloc_get_type_abort(private_data, struct test_spoolss_context);
	struct dcerpc_pipe *p = ctx->spoolss_pipe;
	struct dcerpc_binding_handle *b = p->binding_handle;
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

		torture_assert(tctx, test_GetPrinterData(tctx, b, &ctx->server_handle, list[i], &type, &data, &needed),
			talloc_asprintf(tctx, "GetPrinterData failed on %s\n", list[i]));
		torture_assert(tctx, test_GetPrinterDataEx(tctx, p, &ctx->server_handle, "random_string", list[i], &type_ex, &data_ex, &needed_ex),
			talloc_asprintf(tctx, "GetPrinterDataEx failed on %s\n", list[i]));
		torture_assert_int_equal(tctx, type, type_ex, "type mismatch");
		torture_assert_int_equal(tctx, needed, needed_ex, "needed mismatch");
		torture_assert_mem_equal(tctx, data, data_ex, needed, "data mismatch");
	}

	return true;
}

static bool test_EnumPrinterData(struct torture_context *tctx,
				 struct dcerpc_pipe *p,
				 struct policy_handle *handle,
				 uint32_t enum_index,
				 uint32_t value_offered,
				 uint32_t data_offered,
				 enum winreg_Type *type_p,
				 uint32_t *value_needed_p,
				 uint32_t *data_needed_p,
				 const char **value_name_p,
				 uint8_t **data_p,
				 WERROR *result_p)
{
	struct spoolss_EnumPrinterData r;
	uint32_t data_needed;
	uint32_t value_needed;
	enum winreg_Type type;
	struct dcerpc_binding_handle *b = p->binding_handle;

	r.in.handle = handle;
	r.in.enum_index = enum_index;
	r.in.value_offered = value_offered;
	r.in.data_offered = data_offered;
	r.out.data_needed = &data_needed;
	r.out.value_needed = &value_needed;
	r.out.type = &type;
	r.out.data = talloc_zero_array(tctx, uint8_t, r.in.data_offered);
	r.out.value_name = talloc_zero_array(tctx, const char, r.in.value_offered);

	torture_comment(tctx, "Testing EnumPrinterData(%d)\n", enum_index);

	torture_assert_ntstatus_ok(tctx,
		dcerpc_spoolss_EnumPrinterData_r(b, tctx, &r),
		"EnumPrinterData failed");

	if (type_p) {
		*type_p = type;
	}
	if (value_needed_p) {
		*value_needed_p = value_needed;
	}
	if (data_needed_p) {
		*data_needed_p = data_needed;
	}
	if (value_name_p) {
		*value_name_p = r.out.value_name;
	}
	if (data_p) {
		*data_p = r.out.data;
	}
	if (result_p) {
		*result_p = r.out.result;
	}

	return true;
}


static bool test_EnumPrinterData_all(struct torture_context *tctx,
				     struct dcerpc_pipe *p,
				     struct policy_handle *handle)
{
	uint32_t enum_index = 0;
	enum winreg_Type type;
	uint32_t value_needed;
	uint32_t data_needed;
	uint8_t *data;
	const char *value_name;
	WERROR result;

	torture_comment(tctx, "Testing EnumPrinterData\n");

	do {
		torture_assert(tctx,
			test_EnumPrinterData(tctx, p, handle, enum_index, 0, 0,
					     &type, &value_needed, &data_needed,
					     &value_name, &data, &result),
			"EnumPrinterData failed");

		if (W_ERROR_EQUAL(result, WERR_NO_MORE_ITEMS)) {
			break;
		}

		torture_assert(tctx,
			test_EnumPrinterData(tctx, p, handle, enum_index, value_needed, data_needed,
					     &type, &value_needed, &data_needed,
					     &value_name, &data, &result),
			"EnumPrinterData failed");

		if (W_ERROR_EQUAL(result, WERR_NO_MORE_ITEMS)) {
			break;
		}

		enum_index++;

	} while (W_ERROR_IS_OK(result));

	torture_comment(tctx, "EnumPrinterData test succeeded\n");

	return true;
}

static bool test_EnumPrinterDataEx(struct torture_context *tctx,
				   struct dcerpc_binding_handle *b,
				   struct policy_handle *handle,
				   const char *key_name,
				   uint32_t *count_p,
				   struct spoolss_PrinterEnumValues **info_p)
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

	torture_assert_ntstatus_ok(tctx, dcerpc_spoolss_EnumPrinterDataEx_r(b, tctx, &r),
		"EnumPrinterDataEx failed");
	if (W_ERROR_EQUAL(r.out.result, WERR_MORE_DATA)) {
		r.in.offered = needed;
		torture_assert_ntstatus_ok(tctx, dcerpc_spoolss_EnumPrinterDataEx_r(b, tctx, &r),
			"EnumPrinterDataEx failed");
	}

	torture_assert_werr_ok(tctx, r.out.result, "EnumPrinterDataEx failed");

	CHECK_NEEDED_SIZE_ENUM(spoolss_EnumPrinterDataEx, info, count, needed, 1);

	if (count_p) {
		*count_p = count;
	}
	if (info_p) {
		*info_p = info;
	}

	return true;
}

static bool test_SetPrinterData(struct torture_context *tctx,
				struct dcerpc_binding_handle *b,
				struct policy_handle *handle,
				const char *value_name,
				enum winreg_Type type,
				uint8_t *data,
				uint32_t offered);
static bool test_DeletePrinterData(struct torture_context *tctx,
				   struct dcerpc_binding_handle *b,
				   struct policy_handle *handle,
				   const char *value_name);

static bool test_EnumPrinterData_consistency(struct torture_context *tctx,
					     struct dcerpc_pipe *p,
					     struct policy_handle *handle)
{
	uint32_t count;
	struct spoolss_PrinterEnumValues *info;
	int i;
	uint32_t value_needed, data_needed;
	uint32_t value_offered, data_offered;
	WERROR result;
	struct dcerpc_binding_handle *b = p->binding_handle;

	enum winreg_Type type;
	DATA_BLOB blob;

	torture_comment(tctx, "Testing EnumPrinterData vs EnumPrinterDataEx consistency\n");

	torture_assert(tctx, push_reg_sz(tctx, &blob, "torture_data1"), "");
	type = REG_SZ;

	torture_assert(tctx,
		test_SetPrinterData(tctx, b, handle, "torture_value1", type, blob.data, blob.length),
		"SetPrinterData failed");

	blob = data_blob_string_const("torture_data2");

	torture_assert(tctx,
		test_SetPrinterData(tctx, b, handle, "torture_value2", REG_BINARY, blob.data, blob.length),
		"SetPrinterData failed");

	blob = data_blob_talloc(tctx, NULL, 4);
	SIVAL(blob.data, 0, 0x11223344);

	torture_assert(tctx,
		test_SetPrinterData(tctx, b, handle, "torture_value3", type, blob.data, blob.length),
		"SetPrinterData failed");

	torture_assert(tctx,
		test_EnumPrinterDataEx(tctx, b, handle, "PrinterDriverData", &count, &info),
		"failed to call EnumPrinterDataEx");

	/* get the max sizes for value and data */

	torture_assert(tctx,
		test_EnumPrinterData(tctx, p, handle, 0, 0, 0,
				     NULL, &value_needed, &data_needed,
				     NULL, NULL, &result),
		"EnumPrinterData failed");
	torture_assert_werr_ok(tctx, result, "unexpected result");

	/* check if the reply from the EnumPrinterData really matches max values */

	for (i=0; i < count; i++) {
		if (info[i].value_name_len > value_needed) {
			torture_fail(tctx,
				talloc_asprintf(tctx,
				"EnumPrinterDataEx gave a reply with value length %d which is larger then expected max value length %d from EnumPrinterData",
				info[i].value_name_len, value_needed));
		}
		if (info[i].data_length > data_needed) {
			torture_fail(tctx,
				talloc_asprintf(tctx,
				"EnumPrinterDataEx gave a reply with data length %d which is larger then expected max data length %d from EnumPrinterData",
				info[i].data_length, data_needed));
		}
	}

	/* assuming that both EnumPrinterData and EnumPrinterDataEx do either
	 * sort or not sort the replies by value name, we should be able to do
	 * the following entry comparison */

	data_offered = data_needed;
	value_offered = value_needed;

	for (i=0; i < count; i++) {

		const char *value_name;
		uint8_t *data;

		torture_assert(tctx,
			test_EnumPrinterData(tctx, p, handle, i, value_offered, data_offered,
					     &type, &value_needed, &data_needed,
					     &value_name, &data, &result),
			"EnumPrinterData failed");

		if (i -1 == count) {
			torture_assert_werr_equal(tctx, result, WERR_NO_MORE_ITEMS,
				"unexpected result");
			break;
		} else {
			torture_assert_werr_ok(tctx, result, "unexpected result");
		}

		torture_assert_int_equal(tctx, type, info[i].type, "type mismatch");
		torture_assert_int_equal(tctx, value_needed, info[i].value_name_len, "value name length mismatch");
		torture_assert_str_equal(tctx, value_name, info[i].value_name, "value name mismatch");
		torture_assert_int_equal(tctx, data_needed, info[i].data_length, "data length mismatch");
		torture_assert_mem_equal(tctx, data, info[i].data->data, info[i].data_length, "data mismatch");
	}

	torture_assert(tctx,
		test_DeletePrinterData(tctx, b, handle, "torture_value1"),
		"DeletePrinterData failed");
	torture_assert(tctx,
		test_DeletePrinterData(tctx, b, handle, "torture_value2"),
		"DeletePrinterData failed");
	torture_assert(tctx,
		test_DeletePrinterData(tctx, b, handle, "torture_value3"),
		"DeletePrinterData failed");

	torture_comment(tctx, "EnumPrinterData vs EnumPrinterDataEx consistency test succeeded\n\n");

	return true;
}

static bool test_DeletePrinterData(struct torture_context *tctx,
				   struct dcerpc_binding_handle *b,
				   struct policy_handle *handle,
				   const char *value_name)
{
	NTSTATUS status;
	struct spoolss_DeletePrinterData r;

	r.in.handle = handle;
	r.in.value_name = value_name;

	torture_comment(tctx, "Testing DeletePrinterData(%s)\n",
		r.in.value_name);

	status = dcerpc_spoolss_DeletePrinterData_r(b, tctx, &r);

	torture_assert_ntstatus_ok(tctx, status, "DeletePrinterData failed");
	torture_assert_werr_ok(tctx, r.out.result, "DeletePrinterData failed");

	return true;
}

static bool test_DeletePrinterDataEx(struct torture_context *tctx,
				     struct dcerpc_binding_handle *b,
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
		dcerpc_spoolss_DeletePrinterDataEx_r(b, tctx, &r),
		"DeletePrinterDataEx failed");
	torture_assert_werr_ok(tctx, r.out.result,
		"DeletePrinterDataEx failed");

	return true;
}

static bool test_DeletePrinterKey(struct torture_context *tctx,
				  struct dcerpc_binding_handle *b,
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
		dcerpc_spoolss_DeletePrinterKey_r(b, tctx, &r),
		"DeletePrinterKey failed");
	torture_assert_werr_ok(tctx, r.out.result,
		"DeletePrinterKey failed");

	return true;
}

static bool test_winreg_OpenHKLM(struct torture_context *tctx,
				 struct dcerpc_binding_handle *b,
				 struct policy_handle *handle)
{
	struct winreg_OpenHKLM r;

	r.in.system_name = NULL;
	r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r.out.handle = handle;

	torture_comment(tctx, "Testing winreg_OpenHKLM\n");

	torture_assert_ntstatus_ok(tctx, dcerpc_winreg_OpenHKLM_r(b, tctx, &r), "OpenHKLM failed");
	torture_assert_werr_ok(tctx, r.out.result, "OpenHKLM failed");

	return true;
}

static void init_winreg_String(struct winreg_String *name, const char *s)
{
	name->name = s;
	if (s) {
		name->name_len = 2 * (strlen_m(s) + 1);
		name->name_size = name->name_len;
	} else {
		name->name_len = 0;
		name->name_size = 0;
	}
}

static bool test_winreg_OpenKey_opts(struct torture_context *tctx,
				     struct dcerpc_binding_handle *b,
				     struct policy_handle *hive_handle,
				     const char *keyname,
				     uint32_t options,
				     struct policy_handle *key_handle)
{
	struct winreg_OpenKey r;

	r.in.parent_handle = hive_handle;
	init_winreg_String(&r.in.keyname, keyname);
	r.in.options = options;
	r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r.out.handle = key_handle;

	torture_comment(tctx, "Testing winreg_OpenKey(%s)\n", keyname);

	torture_assert_ntstatus_ok(tctx, dcerpc_winreg_OpenKey_r(b, tctx, &r), "OpenKey failed");
	torture_assert_werr_ok(tctx, r.out.result, "OpenKey failed");

	return true;
}

static bool test_winreg_OpenKey(struct torture_context *tctx,
				struct dcerpc_binding_handle *b,
				struct policy_handle *hive_handle,
				const char *keyname,
				struct policy_handle *key_handle)
{
	return test_winreg_OpenKey_opts(tctx, b, hive_handle, keyname,
					REG_OPTION_NON_VOLATILE, key_handle);
}

static bool test_winreg_CloseKey(struct torture_context *tctx,
				 struct dcerpc_binding_handle *b,
				 struct policy_handle *handle)
{
	struct winreg_CloseKey r;

	r.in.handle = handle;
	r.out.handle = handle;

	torture_comment(tctx, "Testing winreg_CloseKey\n");

	torture_assert_ntstatus_ok(tctx, dcerpc_winreg_CloseKey_r(b, tctx, &r), "CloseKey failed");
	torture_assert_werr_ok(tctx, r.out.result, "CloseKey failed");

	return true;
}

bool test_winreg_QueryValue(struct torture_context *tctx,
			    struct dcerpc_binding_handle *b,
			    struct policy_handle *handle,
			    const char *value_name,
			    enum winreg_Type *type_p,
			    uint32_t *data_size_p,
			    uint32_t *data_length_p,
			    uint8_t **data_p)
{
	struct winreg_QueryValue r;
	enum winreg_Type type = REG_NONE;
	uint32_t data_size = 0;
	uint32_t data_length = 0;
	struct winreg_String valuename;
	uint8_t *data = NULL;

	init_winreg_String(&valuename, value_name);

	data = talloc_zero_array(tctx, uint8_t, 0);

	r.in.handle = handle;
	r.in.value_name = &valuename;
	r.in.type = &type;
	r.in.data_size = &data_size;
	r.in.data_length = &data_length;
	r.in.data = data;
	r.out.type = &type;
	r.out.data = data;
	r.out.data_size = &data_size;
	r.out.data_length = &data_length;

	torture_comment(tctx, "Testing winreg_QueryValue(%s)\n", value_name);

	torture_assert_ntstatus_ok(tctx, dcerpc_winreg_QueryValue_r(b, tctx, &r), "QueryValue failed");
	if (W_ERROR_EQUAL(r.out.result, WERR_MORE_DATA)) {
		*r.in.data_size = *r.out.data_size;
		data = talloc_zero_array(tctx, uint8_t, *r.in.data_size);
		r.in.data = data;
		r.out.data = data;
		torture_assert_ntstatus_ok(tctx, dcerpc_winreg_QueryValue_r(b, tctx, &r), "QueryValue failed");
	}
	torture_assert_werr_ok(tctx, r.out.result, "QueryValue failed");

	if (type_p) {
		*type_p = *r.out.type;
	}
	if (data_size_p) {
		*data_size_p = *r.out.data_size;
	}
	if (data_length_p) {
		*data_length_p = *r.out.data_length;
	}
	if (data_p) {
		*data_p = r.out.data;
	}

	return true;
}

static bool test_winreg_query_printerdata(struct torture_context *tctx,
					  struct dcerpc_binding_handle *b,
					  struct policy_handle *handle,
					  const char *printer_name,
					  const char *key_name,
					  const char *value_name,
					  enum winreg_Type *w_type,
					  uint32_t *w_size,
					  uint32_t *w_length,
					  uint8_t **w_data)
{
	const char *printer_key;
	struct policy_handle key_handle;

	printer_key = talloc_asprintf(tctx, "%s\\%s\\%s",
		TOP_LEVEL_PRINT_PRINTERS_KEY, printer_name, key_name);

	torture_assert(tctx,
		test_winreg_OpenKey(tctx, b, handle, printer_key, &key_handle), "");

	torture_assert(tctx,
		test_winreg_QueryValue(tctx, b, &key_handle, value_name, w_type, w_size, w_length, w_data), "");

	torture_assert(tctx,
		test_winreg_CloseKey(tctx, b, &key_handle), "");

	return true;
}

static bool test_GetForm_winreg(struct torture_context *tctx,
			        struct dcerpc_binding_handle *b,
				struct policy_handle *handle,
				const char *key_name,
				const char *form_name,
				enum winreg_Type *w_type,
				uint32_t *w_size,
				uint32_t *w_length,
				uint8_t **w_data)
{
	struct policy_handle key_handle;

	torture_assert(tctx,
		test_winreg_OpenKey(tctx, b, handle, key_name, &key_handle), "");

	torture_assert(tctx,
		test_winreg_QueryValue(tctx, b, &key_handle, form_name, w_type, w_size, w_length, w_data), "");

	torture_assert(tctx,
		test_winreg_CloseKey(tctx, b, &key_handle), "");

	return true;
}

static bool test_winreg_symbolic_link(struct torture_context *tctx,
				      struct dcerpc_binding_handle *b,
				      struct policy_handle *handle,
				      const char *symlink_keyname,
				      const char *symlink_destination)
{
	/* check if the first key is a symlink to the second key */

	enum winreg_Type w_type;
	uint32_t w_size;
	uint32_t w_length;
	uint8_t *w_data;
	struct policy_handle key_handle;
	DATA_BLOB blob;
	const char *str;

	if (torture_setting_bool(tctx, "samba3", false)) {
		torture_skip(tctx, "skip winreg symlink test against samba");
	}

	torture_assert(tctx,
		test_winreg_OpenKey_opts(tctx, b, handle, symlink_keyname, REG_OPTION_OPEN_LINK, &key_handle),
			"failed to open key link");

	torture_assert(tctx,
		test_winreg_QueryValue(tctx, b, &key_handle,
				       "SymbolicLinkValue",
				       &w_type, &w_size, &w_length, &w_data),
		"failed to query for 'SymbolicLinkValue' attribute");

	torture_assert_int_equal(tctx, w_type, REG_LINK, "unexpected type");

	blob = data_blob(w_data, w_size);
	str = reg_val_data_string(tctx, REG_SZ, blob);

	torture_assert_str_equal(tctx, str, symlink_destination, "unexpected symlink target string");

	torture_assert(tctx,
		test_winreg_CloseKey(tctx, b, &key_handle),
		"failed to close key link");

	return true;
}

static const char *strip_unc(const char *unc)
{
	char *name;

	if (!unc) {
		return NULL;
	}

	if (unc[0] == '\\' && unc[1] == '\\') {
		unc +=2;
	}

	name = strchr(unc, '\\');
	if (name) {
		return name+1;
	}

	return unc;
}

static bool test_GetPrinterInfo_winreg(struct torture_context *tctx,
				       struct dcerpc_binding_handle *b,
				       struct policy_handle *handle,
				       const char *printer_name,
				       struct dcerpc_binding_handle *winreg_handle,
				       struct policy_handle *hive_handle)
{
	union spoolss_PrinterInfo info;
	const char *keys[] = {
		TOP_LEVEL_CONTROL_PRINTERS_KEY,
		TOP_LEVEL_PRINT_PRINTERS_KEY
	};
	int i;
	const char *printername, *sharename;

	torture_comment(tctx, "Testing Printer Info and winreg consistency\n");

	torture_assert(tctx,
		test_GetPrinter_level(tctx, b, handle, 2, &info),
		"failed to get printer info level 2");

	printername = strip_unc(info.info2.printername);
	sharename = strip_unc(info.info2.sharename);

#define test_sz(wname, iname) \
do {\
	DATA_BLOB blob;\
	const char *str;\
	enum winreg_Type w_type;\
	uint32_t w_size;\
	uint32_t w_length;\
	uint8_t *w_data;\
	torture_assert(tctx,\
		test_winreg_QueryValue(tctx, winreg_handle, &key_handle, wname,\
				       &w_type, &w_size, &w_length, &w_data),\
		"failed to query winreg");\
	torture_assert_int_equal(tctx, w_type, REG_SZ, "unexpected type");\
	blob = data_blob(w_data, w_size);\
	str = reg_val_data_string(tctx, REG_SZ, blob);\
	if (w_size == 2 && iname == NULL) {\
		/*torture_comment(tctx, "%s: \"\", %s: (null)\n", #wname, #iname);\ */\
	} else {\
		torture_assert_str_equal(tctx, str, iname,\
			talloc_asprintf(tctx, "%s - %s mismatch", #wname, #iname));\
	}\
} while(0);

#define test_dword(wname, iname) \
do {\
	uint32_t value;\
	enum winreg_Type w_type;\
	uint32_t w_size;\
	uint32_t w_length;\
	uint8_t *w_data;\
	torture_assert(tctx,\
		test_winreg_QueryValue(tctx, winreg_handle, &key_handle, wname,\
				       &w_type, &w_size, &w_length, &w_data),\
		"failed to query winreg");\
	torture_assert_int_equal(tctx, w_type, REG_DWORD, "unexpected type");\
	torture_assert_int_equal(tctx, w_size, 4, "unexpected size");\
	torture_assert_int_equal(tctx, w_length, 4, "unexpected length");\
	value = IVAL(w_data, 0);\
	torture_assert_int_equal(tctx, value, iname,\
		talloc_asprintf(tctx, "%s - %s mismatch", #wname, #iname));\
} while(0);

#define test_binary(wname, iname) \
do {\
	enum winreg_Type w_type;\
	uint32_t w_size;\
	uint32_t w_length;\
	uint8_t *w_data;\
	torture_assert(tctx,\
		test_winreg_QueryValue(tctx, winreg_handle, &key_handle, wname,\
				       &w_type, &w_size, &w_length, &w_data),\
		"failed to query winreg");\
	torture_assert_int_equal(tctx, w_type, REG_BINARY, "unexpected type");\
	torture_assert_int_equal(tctx, w_size, iname.length, "unexpected length");\
	torture_assert_mem_equal(tctx, w_data, iname.data, w_size, \
		"binary unequal");\
} while(0);


#define test_dm(wname, iname) \
do {\
	DATA_BLOB blob;\
	struct spoolss_DeviceMode dm;\
	enum ndr_err_code ndr_err;\
	enum winreg_Type w_type;\
	uint32_t w_size;\
	uint32_t w_length;\
	uint8_t *w_data;\
	torture_assert(tctx,\
		test_winreg_QueryValue(tctx, winreg_handle, &key_handle, wname,\
				       &w_type, &w_size, &w_length, &w_data),\
		"failed to query winreg");\
	torture_assert_int_equal(tctx, w_type, REG_BINARY, "unexpected type");\
	blob = data_blob(w_data, w_size);\
	ndr_err = ndr_pull_struct_blob(&blob, tctx, &dm,\
		(ndr_pull_flags_fn_t)ndr_pull_spoolss_DeviceMode);\
	torture_assert_ndr_success(tctx, ndr_err, "failed to unmarshall dm");\
	torture_assert(tctx, test_devicemode_equal(tctx, &dm, iname),\
		"dm unequal");\
} while(0);

#define test_sd(wname, iname) \
do {\
	DATA_BLOB blob;\
	struct security_descriptor sd;\
	enum ndr_err_code ndr_err;\
	enum winreg_Type w_type;\
	uint32_t w_size;\
	uint32_t w_length;\
	uint8_t *w_data;\
	torture_assert(tctx,\
		test_winreg_QueryValue(tctx, winreg_handle, &key_handle, wname,\
				       &w_type, &w_size, &w_length, &w_data),\
		"failed to query winreg");\
	torture_assert_int_equal(tctx, w_type, REG_BINARY, "unexpected type");\
	blob = data_blob(w_data, w_size);\
	ndr_err = ndr_pull_struct_blob(&blob, tctx, &sd,\
		(ndr_pull_flags_fn_t)ndr_pull_security_descriptor);\
	torture_assert_ndr_success(tctx, ndr_err, "failed to unmarshall sd");\
	torture_assert(tctx, test_security_descriptor_equal(tctx, &sd, iname),\
		"sd unequal");\
} while(0);

#define test_multi_sz(wname, iname) \
do {\
	DATA_BLOB blob;\
	const char **array;\
	enum winreg_Type w_type;\
	uint32_t w_size;\
	uint32_t w_length;\
	uint8_t *w_data;\
	int i;\
	torture_assert(tctx,\
		test_winreg_QueryValue(tctx, winreg_handle, &key_handle, wname,\
				       &w_type, &w_size, &w_length, &w_data),\
		"failed to query winreg");\
	torture_assert_int_equal(tctx, w_type, REG_MULTI_SZ, "unexpected type");\
	blob = data_blob(w_data, w_size);\
	torture_assert(tctx, \
		pull_reg_multi_sz(tctx, &blob, &array),\
		"failed to pull multi sz");\
	for (i=0; array[i] != NULL; i++) {\
		torture_assert_str_equal(tctx, array[i], iname[i],\
			talloc_asprintf(tctx, "%s - %s mismatch", #wname, iname[i]));\
	}\
} while(0);

	if (!test_winreg_symbolic_link(tctx, winreg_handle, hive_handle,
				       TOP_LEVEL_CONTROL_PRINTERS_KEY,
				       "\\Registry\\Machine\\Software\\Microsoft\\Windows NT\\CurrentVersion\\Print\\Printers"))
	{
		torture_warning(tctx, "failed to check for winreg symlink");
	}

	for (i=0; i < ARRAY_SIZE(keys); i++) {

		const char *printer_key;
		struct policy_handle key_handle;

		printer_key = talloc_asprintf(tctx, "%s\\%s",
			keys[i], printer_name);

		torture_assert(tctx,
			test_winreg_OpenKey(tctx, winreg_handle, hive_handle, printer_key, &key_handle), "");

		test_sz("Name", printername);
		test_sz("Share Name", sharename);
		test_sz("Port", info.info2.portname);
		test_sz("Printer Driver", info.info2.drivername);
		test_sz("Description", info.info2.comment);
		test_sz("Location", info.info2.location);
		test_sz("Separator File", info.info2.sepfile);
		test_sz("Print Processor", info.info2.printprocessor);
		test_sz("Datatype", info.info2.datatype);
		test_sz("Parameters", info.info2.parameters);
		/* winreg: 0, spoolss not */
/*		test_dword("Attributes", info.info2.attributes); */
		test_dword("Priority", info.info2.priority);
		test_dword("Default Priority", info.info2.defaultpriority);
		/* winreg: 60, spoolss: 0 */
/*		test_dword("StartTime", info.info2.starttime); */
/*		test_dword("UntilTime", info.info2.untiltime); */
		/* winreg != spoolss */
/*		test_dword("Status", info.info2.status); */
		test_dm("Default DevMode", info.info2.devmode);
		test_sd("Security", info.info2.secdesc);

		torture_assert(tctx,
			test_winreg_CloseKey(tctx, winreg_handle, &key_handle), "");
	}

#undef test_dm
#undef test_sd

	torture_comment(tctx, "Printer Info and winreg consistency test succeeded\n\n");

	return true;
}

static bool test_PrintProcessors(struct torture_context *tctx,
				 struct dcerpc_binding_handle *b,
				 const char *environment,
				 struct dcerpc_binding_handle *winreg_handle,
				 struct policy_handle *hive_handle)
{
	union spoolss_PrintProcessorInfo *info;
	uint32_t count;
	int i;

	torture_comment(tctx, "Testing Print Processor Info and winreg consistency\n");

	torture_assert(tctx,
		test_EnumPrintProcessors_level(tctx, b, environment, 1, &count, &info, WERR_OK),
		"failed to enum print processors level 1");

	for (i=0; i < count; i++) {

		const char *processor_key;
		struct policy_handle key_handle;

		processor_key = talloc_asprintf(tctx, "%s\\%s\\Print Processors\\%s",
						TOP_LEVEL_CONTROL_ENVIRONMENTS_KEY,
						environment,
						info[i].info1.print_processor_name);

		torture_assert(tctx,
			test_winreg_OpenKey(tctx, winreg_handle, hive_handle, processor_key, &key_handle), "");

		/* nothing to check in there so far */

		torture_assert(tctx,
			test_winreg_CloseKey(tctx, winreg_handle, &key_handle), "");
	}

	torture_comment(tctx, "Print Processor Info and winreg consistency test succeeded\n\n");

	return true;
}

static bool test_GetPrinterDriver2_level(struct torture_context *tctx,
					 struct dcerpc_binding_handle *b,
					 struct policy_handle *handle,
					 const char *driver_name,
					 const char *architecture,
					 uint32_t level,
					 uint32_t client_major_version,
					 uint32_t client_minor_version,
					 union spoolss_DriverInfo *info_p,
					 WERROR *result);

static const char *strip_path(const char *path)
{
	char *p;

	if (path == NULL) {
		return NULL;
	}

	p = strrchr(path, '\\');
	if (p) {
		return p+1;
	}

	return path;
}

static const char **strip_paths(const char **path_array)
{
	int i;

	if (path_array == NULL) {
		return NULL;
	}

	for (i=0; path_array[i] != NULL; i++) {
		path_array[i] = strip_path(path_array[i]);
	}

	return path_array;
}

static const char *driver_winreg_date(TALLOC_CTX *mem_ctx, NTTIME nt)
{
	time_t t;
	struct tm *tm;

	if (nt == 0) {
		return talloc_strdup(mem_ctx, "01/01/1601");
	}

	t = nt_time_to_unix(nt);
	tm = localtime(&t);

	return talloc_asprintf(mem_ctx, "%02d/%02d/%04d",
		tm->tm_mon + 1, tm->tm_mday, tm->tm_year + 1900);
}

static const char *driver_winreg_version(TALLOC_CTX *mem_ctx, uint64_t v)
{
	return talloc_asprintf(mem_ctx, "%u.%u.%u.%u",
		(unsigned)((v >> 48) & 0xFFFF),
		(unsigned)((v >> 32) & 0xFFFF),
		(unsigned)((v >> 16) & 0xFFFF),
		(unsigned)(v & 0xFFFF));
}

static bool test_GetDriverInfo_winreg(struct torture_context *tctx,
				      struct dcerpc_binding_handle *b,
				      struct policy_handle *handle,
				      const char *printer_name,
				      const char *driver_name,
				      const char *environment,
				      enum spoolss_DriverOSVersion version,
				      struct dcerpc_binding_handle *winreg_handle,
				      struct policy_handle *hive_handle,
				      const char *server_name_slash)
{
	WERROR result;
	union spoolss_DriverInfo info;
	const char *driver_key;
	struct policy_handle key_handle;

	const char *driver_path;
	const char *data_file;
	const char *config_file;
	const char *help_file;
	const char **dependent_files;

	const char *driver_date;
	const char *inbox_driver_date;

	const char *driver_version;
	const char *inbox_driver_version;

	torture_comment(tctx, "Testing Driver Info and winreg consistency\n");

	driver_key = talloc_asprintf(tctx, "%s\\%s\\Drivers\\Version-%d\\%s",
				     TOP_LEVEL_CONTROL_ENVIRONMENTS_KEY,
				     environment,
				     version,
				     driver_name);

	torture_assert(tctx,
		test_winreg_OpenKey(tctx, winreg_handle, hive_handle, driver_key, &key_handle),
		"failed to open driver key");

	if (torture_setting_bool(tctx, "samba3", false) ||
	    torture_setting_bool(tctx, "w2k3", false)) {
		goto try_level6;
	}

	if (handle) {
		torture_assert(tctx,
			test_GetPrinterDriver2_level(tctx, b, handle, driver_name, environment, 8, version, 0, &info, &result),
			"failed to get driver info level 8");
	} else {
		torture_assert(tctx,
			test_EnumPrinterDrivers_findone(tctx, b, server_name_slash, environment, 8, driver_name, &info),
			"failed to get driver info level 8");
	}

	if (W_ERROR_EQUAL(result, WERR_INVALID_LEVEL)) {
		goto try_level6;
	}

	driver_path	= strip_path(info.info8.driver_path);
	data_file	= strip_path(info.info8.data_file);
	config_file	= strip_path(info.info8.config_file);
	help_file	= strip_path(info.info8.help_file);
	dependent_files = strip_paths(info.info8.dependent_files);

	driver_date		= driver_winreg_date(tctx, info.info8.driver_date);
	inbox_driver_date	= driver_winreg_date(tctx, info.info8.min_inbox_driver_ver_date);

	driver_version		= driver_winreg_version(tctx, info.info8.driver_version);
	inbox_driver_version	= driver_winreg_version(tctx, info.info8.min_inbox_driver_ver_version);

	test_sz("Configuration File",		config_file);
	test_sz("Data File",			data_file);
	test_sz("Datatype",			info.info8.default_datatype);
	test_sz("Driver",			driver_path);
	test_sz("DriverDate",			driver_date);
	test_sz("DriverVersion",		driver_version);
	test_sz("HardwareID",			info.info8.hardware_id);
	test_sz("Help File",			help_file);
	test_sz("InfPath",			info.info8.inf_path);
	test_sz("Manufacturer",			info.info8.manufacturer_name);
	test_sz("MinInboxDriverVerDate",	inbox_driver_date);
	test_sz("MinInboxDriverVerVersion",	inbox_driver_version);
	test_sz("Monitor",			info.info8.monitor_name);
	test_sz("OEM URL",			info.info8.manufacturer_url);
	test_sz("Print Processor",		info.info8.print_processor);
	test_sz("Provider",			info.info8.provider);
	test_sz("VendorSetup",			info.info8.vendor_setup);
	test_multi_sz("ColorProfiles",		info.info8.color_profiles);
	test_multi_sz("Dependent Files",	dependent_files);
	test_multi_sz("CoreDependencies",	info.info8.core_driver_dependencies);
	test_multi_sz("Previous Names",		info.info8.previous_names);
/*	test_dword("Attributes",		?); */
	test_dword("PrinterDriverAttributes",	info.info8.printer_driver_attributes);
	test_dword("Version",			info.info8.version);
/*	test_dword("TempDir",			?); */

 try_level6:

	if (handle) {
		torture_assert(tctx,
			test_GetPrinterDriver2_level(tctx, b, handle, driver_name, environment, 6, version, 0, &info, &result),
			"failed to get driver info level 6");
	} else {
		torture_assert(tctx,
			test_EnumPrinterDrivers_findone(tctx, b, server_name_slash, environment, 6, driver_name, &info),
			"failed to get driver info level 6");
	}

	driver_path	= strip_path(info.info6.driver_path);
	data_file	= strip_path(info.info6.data_file);
	config_file	= strip_path(info.info6.config_file);
	help_file	= strip_path(info.info6.help_file);
	dependent_files = strip_paths(info.info6.dependent_files);

	driver_date		= driver_winreg_date(tctx, info.info6.driver_date);

	driver_version		= driver_winreg_version(tctx, info.info6.driver_version);

	test_sz("Configuration File",		config_file);
	test_sz("Data File",			data_file);
	test_sz("Datatype",			info.info6.default_datatype);
	test_sz("Driver",			driver_path);
	if (torture_setting_bool(tctx, "w2k3", false)) {
		DATA_BLOB blob = data_blob_talloc_zero(tctx, 8);
		push_nttime(blob.data, 0, info.info6.driver_date);
		test_binary("DriverDate",	blob);
		SBVAL(blob.data, 0, info.info6.driver_version);
		test_binary("DriverVersion",	blob);
	} else {
		test_sz("DriverDate",		driver_date);
		test_sz("DriverVersion",	driver_version);
	}
	test_sz("HardwareID",			info.info6.hardware_id);
	test_sz("Help File",			help_file);
	test_sz("Manufacturer",			info.info6.manufacturer_name);
	test_sz("Monitor",			info.info6.monitor_name);
	test_sz("OEM URL",			info.info6.manufacturer_url);
	test_sz("Provider",			info.info6.provider);
	test_multi_sz("Dependent Files",	dependent_files);
	test_multi_sz("Previous Names",		info.info6.previous_names);
/*	test_dword("Attributes",		?); */
	test_dword("Version",			info.info6.version);
/*	test_dword("TempDir",			?); */

	if (handle) {
		torture_assert(tctx,
			test_GetPrinterDriver2_level(tctx, b, handle, driver_name, environment, 3, version, 0, &info, &result),
			"failed to get driver info level 3");
	} else {
		torture_assert(tctx,
			test_EnumPrinterDrivers_findone(tctx, b, server_name_slash, environment, 3, driver_name, &info),
			"failed to get driver info level 3");
	}

	driver_path	= strip_path(info.info3.driver_path);
	data_file	= strip_path(info.info3.data_file);
	config_file	= strip_path(info.info3.config_file);
	help_file	= strip_path(info.info3.help_file);
	dependent_files = strip_paths(info.info3.dependent_files);

	test_sz("Configuration File",		config_file);
	test_sz("Data File",			data_file);
	test_sz("Datatype",			info.info3.default_datatype);
	test_sz("Driver",			driver_path);
	test_sz("Help File",			help_file);
	test_sz("Monitor",			info.info3.monitor_name);
	test_multi_sz("Dependent Files",	dependent_files);
/*	test_dword("Attributes",		?); */
	test_dword("Version",			info.info3.version);
/*	test_dword("TempDir",			?); */


	torture_assert(tctx,
		test_winreg_CloseKey(tctx, winreg_handle, &key_handle), "");

	torture_comment(tctx, "Driver Info and winreg consistency test succeeded\n\n");

	return true;
}

#undef test_sz
#undef test_dword

static bool test_SetPrinterData(struct torture_context *tctx,
				struct dcerpc_binding_handle *b,
				struct policy_handle *handle,
				const char *value_name,
				enum winreg_Type type,
				uint8_t *data,
				uint32_t offered)
{
	struct spoolss_SetPrinterData r;

	r.in.handle = handle;
	r.in.value_name = value_name;
	r.in.type = type;
	r.in.data = data;
	r.in.offered = offered;

	torture_comment(tctx, "Testing SetPrinterData(%s)\n",
		r.in.value_name);

	torture_assert_ntstatus_ok(tctx,
		dcerpc_spoolss_SetPrinterData_r(b, tctx, &r),
		"SetPrinterData failed");
	torture_assert_werr_ok(tctx, r.out.result,
		"SetPrinterData failed");

	return true;
}

static bool test_SetPrinterData_matrix(struct torture_context *tctx,
				       struct dcerpc_binding_handle *b,
				       struct policy_handle *handle,
				       const char *printer_name,
				       struct dcerpc_binding_handle *winreg_handle,
				       struct policy_handle *hive_handle)
{
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

		enum winreg_Type type, expected_type = REG_SZ;
		DATA_BLOB blob;
		uint8_t *data;
		uint32_t needed;

		torture_assert(tctx, push_reg_sz(tctx, &blob, "dog"), "");
		type = REG_SZ;

		torture_assert(tctx,
			test_SetPrinterData(tctx, b, handle, values[i], REG_SZ, blob.data, blob.length),
			"SetPrinterData failed");

		torture_assert(tctx,
			test_GetPrinterData_checktype(tctx, b, handle, values[i], &expected_type, &type, &data, &needed),
			"GetPrinterData failed");

		torture_assert_int_equal(tctx, type, REG_SZ, "type mismatch");
		torture_assert_int_equal(tctx, needed, blob.length, "size mismatch");
		torture_assert_mem_equal(tctx, data, blob.data, blob.length, "buffer mismatch");

		if (winreg_handle && hive_handle) {

			enum winreg_Type w_type;
			uint32_t w_size;
			uint32_t w_length;
			uint8_t *w_data;

			torture_assert(tctx,
				test_winreg_query_printerdata(tctx, winreg_handle, hive_handle,
					printer_name, "PrinterDriverData", values[i],
					&w_type, &w_size, &w_length, &w_data), "");

			torture_assert_int_equal(tctx, w_type, REG_SZ, "winreg type mismatch");
			torture_assert_int_equal(tctx, w_size, blob.length, "winreg size mismatch");
			torture_assert_int_equal(tctx, w_length, blob.length, "winreg length mismatch");
			torture_assert_mem_equal(tctx, w_data, blob.data, blob.length, "winreg buffer mismatch");
		}

		torture_assert(tctx,
			test_DeletePrinterData(tctx, b, handle, values[i]),
			"DeletePrinterData failed");
	}

	return true;
}


static bool test_EnumPrinterKey(struct torture_context *tctx,
				struct dcerpc_binding_handle *b,
				struct policy_handle *handle,
				const char *key_name,
				const char ***array);

static bool test_SetPrinterDataEx(struct torture_context *tctx,
				  struct dcerpc_binding_handle *b,
				  struct policy_handle *handle,
				  const char *key_name,
				  const char *value_name,
				  enum winreg_Type type,
				  uint8_t *data,
				  uint32_t offered)
{
	NTSTATUS status;
	struct spoolss_SetPrinterDataEx r;

	r.in.handle = handle;
	r.in.key_name = key_name;
	r.in.value_name = value_name;
	r.in.type = type;
	r.in.data = data;
	r.in.offered = offered;

	torture_comment(tctx, "Testing SetPrinterDataEx(%s - %s) type: %s, offered: 0x%08x\n",
		r.in.key_name, r.in.value_name, str_regtype(r.in.type), r.in.offered);

	status = dcerpc_spoolss_SetPrinterDataEx_r(b, tctx, &r);

	torture_assert_ntstatus_ok(tctx, status, "SetPrinterDataEx failed");
	torture_assert_werr_ok(tctx, r.out.result, "SetPrinterDataEx failed");

	return true;
}

static bool test_SetPrinterDataEx_keys(struct torture_context *tctx,
				       struct dcerpc_pipe *p,
				       struct policy_handle *handle)
{
	struct dcerpc_binding_handle *b = p->binding_handle;
	const char *value_name = "dog";
	const char *keys[] = {
		"torturedataex",
		"torture data ex",
		"torturedataex_with_subkey\\subkey",
		"torturedataex_with_subkey\\subkey:0",
		"torturedataex_with_subkey\\subkey:1",
		"torturedataex_with_subkey\\subkey\\subsubkey",
		"torturedataex_with_subkey\\subkey\\subsubkey:0",
		"torturedataex_with_subkey\\subkey\\subsubkey:1",
		"torture,data",
		"torture,data,ex",
		"torture,data\\ex",
		"torture\\data,ex",
		"torture/data",
		"torture/data ex",
		"torture/data ex/sub",
		"torture//data",
		"torture//data ex",
		"torture//data ex/sub",
		"torture//data ex//sub",
	};
	int i;

	for (i=0; i < ARRAY_SIZE(keys); i++) {

		char *c;
		const char *key;
		enum winreg_Type type;
		DATA_BLOB blob_in, blob_out;
		const char **subkeys;
		uint32_t ecount;
		struct spoolss_PrinterEnumValues *einfo;
		uint32_t needed;

		blob_in = data_blob_talloc(tctx, NULL, 42);

		generate_random_buffer(blob_in.data, blob_in.length);

		torture_assert(tctx,
			test_SetPrinterDataEx(tctx, b, handle, keys[i], value_name, REG_BINARY, blob_in.data, blob_in.length),
			"failed to call SetPrinterDataEx");

		torture_assert(tctx,
			test_GetPrinterDataEx(tctx, p, handle, keys[i], value_name, &type, &blob_out.data, &needed),
			"failed to call GetPrinterDataEx");

		blob_out.length = needed;
		torture_assert(tctx,
			test_EnumPrinterDataEx(tctx, b, handle, keys[i], &ecount, &einfo),
			"failed to call EnumPrinterDataEx");

		torture_assert_int_equal(tctx, type, REG_BINARY, "type mismatch");
		torture_assert_int_equal(tctx, blob_out.length, blob_in.length, "size mismatch");
		torture_assert_mem_equal(tctx, blob_out.data, blob_in.data, blob_in.length, "buffer mismatch");

		torture_assert_int_equal(tctx, ecount, 1, "unexpected enum count");
		torture_assert_str_equal(tctx, einfo[0].value_name, value_name, "value_name mismatch");
		torture_assert_int_equal(tctx, einfo[0].value_name_len, strlen_m_term(value_name)*2, "unexpected value_name_len");
		torture_assert_int_equal(tctx, einfo[0].type, REG_BINARY, "type mismatch");
		torture_assert_int_equal(tctx, einfo[0].data_length, blob_in.length, "size mismatch");
		if (einfo[0].data_length > 0) {
			torture_assert_mem_equal(tctx, einfo[0].data->data, blob_in.data, blob_in.length, "buffer mismatch");
		}

		key = talloc_strdup(tctx, keys[i]);

		if (!test_DeletePrinterDataEx(tctx, b, handle, keys[i], value_name)) {
			return false;
		}

		c = strchr(key, '\\');
		if (c) {
			int k;

			/* we have subkeys */

			*c = 0;

			if (!test_EnumPrinterKey(tctx, b, handle, key, &subkeys)) {
				return false;
			}

			for (k=0; subkeys && subkeys[k]; k++) {

				const char *current_key = talloc_asprintf(tctx, "%s\\%s", key, subkeys[k]);

				if (!test_DeletePrinterKey(tctx, b, handle, current_key)) {
					return false;
				}
			}

			if (!test_DeletePrinterKey(tctx, b, handle, key)) {
				return false;
			}

		} else {
			if (!test_DeletePrinterKey(tctx, b, handle, key)) {
				return false;
			}
		}
	}

	return true;
}

static bool test_SetPrinterDataEx_values(struct torture_context *tctx,
					 struct dcerpc_pipe *p,
					 struct policy_handle *handle)
{
	struct dcerpc_binding_handle *b = p->binding_handle;
	const char *key = "torturedataex";
	const char *values[] = {
		"torture_value",
		"torture value",
		"torture,value",
		"torture/value",
		"torture\\value",
		"torture\\\\value"
	};
	int i;

	for (i=0; i < ARRAY_SIZE(values); i++) {

		enum winreg_Type type;
		DATA_BLOB blob_in, blob_out;
		uint32_t ecount;
		struct spoolss_PrinterEnumValues *einfo;
		uint32_t needed;

		if (torture_setting_bool(tctx, "samba3", false)) {
			char *q;
			q = strrchr(values[i], ',');
			if (q) {
				torture_comment(tctx, "skipping valuename '%s' including ',' character against Samba3\n",
						values[i]);
				continue;
			}
		}

		blob_in = data_blob_talloc(tctx, NULL, 42);

		generate_random_buffer(blob_in.data, blob_in.length);

		torture_assert(tctx,
			test_SetPrinterDataEx(tctx, b, handle, key, values[i], REG_BINARY, blob_in.data, blob_in.length),
			"failed to call SetPrinterDataEx");

		torture_assert(tctx,
			test_GetPrinterDataEx(tctx, p, handle, key, values[i], &type, &blob_out.data, &needed),
			"failed to call GetPrinterDataEx");

		blob_out.length = needed;
		torture_assert(tctx,
			test_EnumPrinterDataEx(tctx, b, handle, key, &ecount, &einfo),
			"failed to call EnumPrinterDataEx");

		torture_assert_int_equal(tctx, type, REG_BINARY, "type mismatch");
		torture_assert_int_equal(tctx, blob_out.length, blob_in.length, "size mismatch");
		torture_assert_mem_equal(tctx, blob_out.data, blob_in.data, blob_in.length, "buffer mismatch");

		torture_assert_int_equal(tctx, ecount, 1, "unexpected enum count");
		torture_assert_str_equal(tctx, einfo[0].value_name, values[i], "value_name mismatch");
		torture_assert_int_equal(tctx, einfo[0].value_name_len, strlen_m_term(values[i])*2, "unexpected value_name_len");
		torture_assert_int_equal(tctx, einfo[0].type, REG_BINARY, "type mismatch");
		torture_assert_int_equal(tctx, einfo[0].data_length, blob_in.length, "size mismatch");
		if (einfo[0].data_length > 0) {
			torture_assert_mem_equal(tctx, einfo[0].data->data, blob_in.data, blob_in.length, "buffer mismatch");
		}

		torture_assert(tctx,
			test_DeletePrinterDataEx(tctx, b, handle, key, values[i]),
			"failed to call DeletePrinterDataEx");
	}

	return true;
}


static bool test_SetPrinterDataEx_matrix(struct torture_context *tctx,
					 struct dcerpc_pipe *p,
					 struct policy_handle *handle,
					 const char *printername,
					 struct dcerpc_binding_handle *winreg_handle,
					 struct policy_handle *hive_handle)
{
	struct dcerpc_binding_handle *b = p->binding_handle;
	const char *value_name = "dog";
	const char *key_name = "torturedataex";
	enum winreg_Type types[] = {
		REG_SZ,
		REG_MULTI_SZ,
		REG_DWORD,
		REG_BINARY
	};
	const char *str = "abcdefghi";
	int t, s;

	for (t=0; t < ARRAY_SIZE(types); t++) {
	for (s=0; s < strlen(str); s++) {

		enum winreg_Type type;
		const char *string = talloc_strndup(tctx, str, s);
		const char *array[2];
		DATA_BLOB blob = data_blob_string_const(string);
		DATA_BLOB data;
		uint8_t *data_out;
		uint32_t needed, offered = 0;
		uint32_t ecount;
		struct spoolss_PrinterEnumValues *einfo;

		array[0] = talloc_strdup(tctx, string);
		array[1] = NULL;

		if (types[t] == REG_DWORD) {
			s = 0xffff;
		}

		switch (types[t]) {
		case REG_BINARY:
			data = blob;
			offered = blob.length;
			break;
		case REG_DWORD:
			data = data_blob_talloc(tctx, NULL, 4);
			SIVAL(data.data, 0, 0x12345678);
			offered = 4;
			break;
		case REG_SZ:
			torture_assert(tctx, push_reg_sz(tctx, &data, string), "");
			type = REG_SZ;
			offered = data.length;
			/*strlen_m_term(data.string)*2;*/
			break;
		case REG_MULTI_SZ:
			torture_assert(tctx, push_reg_multi_sz(tctx, &data, array), "");
			type = REG_MULTI_SZ;
			offered = data.length;
			break;
		default:
			torture_fail(tctx, talloc_asprintf(tctx, "type %d untested\n", types[t]));
		}

		torture_assert(tctx,
			test_SetPrinterDataEx(tctx, b, handle, key_name, value_name, types[t], data.data, offered),
			"failed to call SetPrinterDataEx");

		torture_assert(tctx,
			test_GetPrinterDataEx_checktype(tctx, p, handle, key_name, value_name, &types[t], &type, &data_out, &needed),
			"failed to call GetPrinterDataEx");

		torture_assert(tctx,
			test_EnumPrinterDataEx(tctx, b, handle, key_name, &ecount, &einfo),
			"failed to call EnumPrinterDataEx");

		torture_assert_int_equal(tctx, types[t], type, "type mismatch");
		torture_assert_int_equal(tctx, needed, offered, "size mismatch");
		torture_assert_mem_equal(tctx, data_out, data.data, offered, "buffer mismatch");

		torture_assert_int_equal(tctx, ecount, 1, "unexpected enum count");
		torture_assert_str_equal(tctx, einfo[0].value_name, value_name, "value_name mismatch");
		torture_assert_int_equal(tctx, einfo[0].value_name_len, strlen_m_term(value_name)*2, "unexpected value_name_len");
		torture_assert_int_equal(tctx, einfo[0].type, types[t], "type mismatch");
		torture_assert_int_equal(tctx, einfo[0].data_length, offered, "size mismatch");
		if (einfo[0].data_length > 0) {
			torture_assert_mem_equal(tctx, einfo[0].data->data, data.data, offered, "buffer mismatch");
		}

		if (winreg_handle && hive_handle) {
			enum winreg_Type w_type;
			uint32_t w_size;
			uint32_t w_length;
			uint8_t *w_data;

			torture_assert(tctx,
				test_winreg_query_printerdata(tctx, winreg_handle, hive_handle,
					printername, key_name, value_name,
					&w_type, &w_size, &w_length, &w_data), "");

			torture_assert_int_equal(tctx, w_type, types[t], "winreg type mismatch");
			torture_assert_int_equal(tctx, w_size, offered, "winreg size mismatch");
			torture_assert_int_equal(tctx, w_length, offered, "winreg length mismatch");
			torture_assert_mem_equal(tctx, w_data, data.data, offered, "winreg buffer mismatch");
		}

		torture_assert(tctx,
			test_DeletePrinterDataEx(tctx, b, handle, key_name, value_name),
			"failed to call DeletePrinterDataEx");
	}
	}

	return true;
}

static bool test_PrinterData_winreg(struct torture_context *tctx,
				    struct dcerpc_pipe *p,
				    struct policy_handle *handle,
				    const char *printer_name)
{
	struct dcerpc_binding_handle *b = p->binding_handle;
	struct dcerpc_pipe *p2;
	bool ret = true;
	struct policy_handle hive_handle;
	struct dcerpc_binding_handle *b2;

	torture_assert_ntstatus_ok(tctx,
		torture_rpc_connection(tctx, &p2, &ndr_table_winreg),
		"could not open winreg pipe");
	b2 = p2->binding_handle;

	torture_assert(tctx, test_winreg_OpenHKLM(tctx, b2, &hive_handle), "");

	ret &= test_SetPrinterData_matrix(tctx, b, handle, printer_name, b2, &hive_handle);
	ret &= test_SetPrinterDataEx_matrix(tctx, p, handle, printer_name, b2, &hive_handle);

	test_winreg_CloseKey(tctx, b2, &hive_handle);

	talloc_free(p2);

	return ret;
}

static bool test_Forms_winreg(struct torture_context *tctx,
			      struct dcerpc_binding_handle *b,
			      struct policy_handle *handle,
			      bool print_server,
			      const char *printer_name)
{
	struct dcerpc_pipe *p2;
	bool ret = true;
	struct policy_handle hive_handle;
	struct dcerpc_binding_handle *b2;

	torture_assert_ntstatus_ok(tctx,
		torture_rpc_connection(tctx, &p2, &ndr_table_winreg),
		"could not open winreg pipe");
	b2 = p2->binding_handle;

	torture_assert(tctx, test_winreg_OpenHKLM(tctx, b2, &hive_handle), "");

	ret = test_Forms(tctx, b, handle, print_server, printer_name, b2, &hive_handle);

	test_winreg_CloseKey(tctx, b2, &hive_handle);

	talloc_free(p2);

	return ret;
}

static bool test_PrinterInfo_winreg(struct torture_context *tctx,
				    struct dcerpc_pipe *p,
				    struct policy_handle *handle,
				    const char *printer_name)
{
	struct dcerpc_binding_handle *b = p->binding_handle;
	struct dcerpc_pipe *p2;
	bool ret = true;
	struct policy_handle hive_handle;
	struct dcerpc_binding_handle *b2;

	torture_assert_ntstatus_ok(tctx,
		torture_rpc_connection(tctx, &p2, &ndr_table_winreg),
		"could not open winreg pipe");
	b2 = p2->binding_handle;

	torture_assert(tctx, test_winreg_OpenHKLM(tctx, b2, &hive_handle), "");

	ret = test_GetPrinterInfo_winreg(tctx, b, handle, printer_name, b2, &hive_handle);

	test_winreg_CloseKey(tctx, b2, &hive_handle);

	talloc_free(p2);

	return ret;
}

static bool test_DriverInfo_winreg(struct torture_context *tctx,
				   struct dcerpc_pipe *p,
				   struct policy_handle *handle,
				   const char *printer_name,
				   const char *driver_name,
				   const char *environment,
				   enum spoolss_DriverOSVersion version)
{
	struct dcerpc_binding_handle *b = p->binding_handle;
	struct dcerpc_pipe *p2;
	bool ret = true;
	struct policy_handle hive_handle;
	struct dcerpc_binding_handle *b2;

	torture_assert_ntstatus_ok(tctx,
		torture_rpc_connection(tctx, &p2, &ndr_table_winreg),
		"could not open winreg pipe");
	b2 = p2->binding_handle;

	torture_assert(tctx, test_winreg_OpenHKLM(tctx, b2, &hive_handle), "");

	ret = test_GetDriverInfo_winreg(tctx, b, handle, printer_name, driver_name, environment, version, b2, &hive_handle, NULL);

	test_winreg_CloseKey(tctx, b2, &hive_handle);

	talloc_free(p2);

	return ret;
}

static bool test_PrintProcessors_winreg(struct torture_context *tctx,
					struct dcerpc_binding_handle *b,
					const char *environment)
{
	struct dcerpc_pipe *p2;
	bool ret = true;
	struct policy_handle hive_handle;
	struct dcerpc_binding_handle *b2;

	torture_assert_ntstatus_ok(tctx,
		torture_rpc_connection(tctx, &p2, &ndr_table_winreg),
		"could not open winreg pipe");
	b2 = p2->binding_handle;

	torture_assert(tctx, test_winreg_OpenHKLM(tctx, b2, &hive_handle), "");

	ret = test_PrintProcessors(tctx, b, environment, b2, &hive_handle);

	test_winreg_CloseKey(tctx, b2, &hive_handle);

	talloc_free(p2);

	return ret;
}

static bool test_PrinterData_DsSpooler(struct torture_context *tctx,
				       struct dcerpc_pipe *p,
				       struct policy_handle *handle,
				       const char *printer_name)
{
	struct spoolss_SetPrinterInfoCtr info_ctr;
	struct spoolss_DevmodeContainer devmode_ctr;
	struct sec_desc_buf secdesc_ctr;
	union spoolss_SetPrinterInfo sinfo;
	union spoolss_PrinterInfo info;
	struct dcerpc_binding_handle *b = p->binding_handle;
	const char *pname;

	ZERO_STRUCT(info_ctr);
	ZERO_STRUCT(devmode_ctr);
	ZERO_STRUCT(secdesc_ctr);

	torture_comment(tctx, "Testing DsSpooler <-> SetPrinter relations\n");

	torture_assert(tctx,
		test_GetPrinter_level(tctx, b, handle, 2, &info),
		"failed to query Printer level 2");

	torture_assert(tctx,
		PrinterInfo_to_SetPrinterInfo(tctx, &info, 2, &sinfo),
		"failed to convert");

	info_ctr.level = 2;
	info_ctr.info = sinfo;

#define TEST_SZ(wname, iname) \
do {\
	enum winreg_Type type;\
	uint8_t *data;\
	uint32_t needed;\
	DATA_BLOB blob;\
	const char *str;\
	torture_assert(tctx,\
		test_GetPrinterDataEx(tctx, p, handle, "DsSpooler", wname, &type, &data, &needed),\
		"failed to query");\
	torture_assert_int_equal(tctx, type, REG_SZ, "unexpected type");\
	blob = data_blob_const(data, needed);\
	torture_assert(tctx,\
		pull_reg_sz(tctx, &blob, &str),\
		"failed to pull REG_SZ");\
	torture_assert_str_equal(tctx, str, iname, "unexpected result");\
} while(0);


#define TEST_SET_SZ(wname, iname, val) \
do {\
	enum winreg_Type type;\
	uint8_t *data;\
	uint32_t needed;\
	DATA_BLOB blob;\
	const char *str;\
	sinfo.info2->iname = val;\
	torture_assert(tctx,\
		test_SetPrinter(tctx, b, handle, &info_ctr, &devmode_ctr, &secdesc_ctr, 0),\
		"failed to call SetPrinter");\
	torture_assert(tctx,\
		test_GetPrinterDataEx(tctx, p, handle, "DsSpooler", wname, &type, &data, &needed),\
		"failed to query");\
	torture_assert_int_equal(tctx, type, REG_SZ, "unexpected type");\
	blob = data_blob_const(data, needed);\
	torture_assert(tctx,\
		pull_reg_sz(tctx, &blob, &str),\
		"failed to pull REG_SZ");\
	torture_assert_str_equal(tctx, str, val, "unexpected result");\
} while(0);

#define TEST_SET_DWORD(wname, iname, val) \
do {\
	enum winreg_Type type;\
	uint8_t *data;\
	uint32_t needed;\
	uint32_t value;\
	sinfo.info2->iname = val;\
	torture_assert(tctx,\
		test_SetPrinter(tctx, b, handle, &info_ctr, &devmode_ctr, &secdesc_ctr, 0),\
		"failed to call SetPrinter");\
	torture_assert(tctx,\
		test_GetPrinterDataEx(tctx, p, handle, "DsSpooler", wname, &type, &data, &needed),\
		"failed to query");\
	torture_assert_int_equal(tctx, type, REG_DWORD, "unexpected type");\
	torture_assert_int_equal(tctx, needed, 4, "unexpected length");\
	value = IVAL(data, 0); \
	torture_assert_int_equal(tctx, value, val, "unexpected result");\
} while(0);

	TEST_SET_SZ("description", comment, "newval");
	TEST_SET_SZ("location", location, "newval");
	TEST_SET_SZ("driverName", drivername, "newval");
/*	TEST_SET_DWORD("priority", priority, 25); */

	torture_assert(tctx,
		test_GetPrinter_level(tctx, b, handle, 2, &info),
		"failed to query Printer level 2");

	TEST_SZ("description", info.info2.comment);
	TEST_SZ("driverName", info.info2.drivername);
	TEST_SZ("location", info.info2.location);

	pname = strrchr(info.info2.printername, '\\');
	if (pname == NULL) {
		pname = info.info2.printername;
	} else {
		pname++;
	}
	TEST_SZ("printerName", pname);
	/* TEST_SZ("printSeparatorFile", info.info2.sepfile); */
	/* TEST_SZ("printShareName", info.info2.sharename); */

	/* FIXME gd: complete the list */

#undef TEST_SZ
#undef TEST_SET_SZ
#undef TEST_DWORD

	torture_comment(tctx, "DsSpooler <-> SetPrinter relations test succeeded\n\n");

	return true;
}

static bool test_print_processors_winreg(struct torture_context *tctx,
					 void *private_data)
{
	struct test_spoolss_context *ctx =
		talloc_get_type_abort(private_data, struct test_spoolss_context);
	struct dcerpc_pipe *p = ctx->spoolss_pipe;
	struct dcerpc_binding_handle *b = p->binding_handle;

	return test_PrintProcessors_winreg(tctx, b, ctx->environment);
}

static bool test_GetChangeID_PrinterData(struct torture_context *tctx,
					 struct dcerpc_binding_handle *b,
					 struct policy_handle *handle,
					 uint32_t *change_id)
{
	enum winreg_Type type;
	uint8_t *data;
	uint32_t needed;

	torture_assert(tctx,
		test_GetPrinterData(tctx, b, handle, "ChangeID", &type, &data, &needed),
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
					 struct dcerpc_binding_handle *b,
					 struct policy_handle *handle,
					 uint32_t *change_id)
{
	union spoolss_PrinterInfo info;

	torture_assert(tctx, test_GetPrinter_level(tctx, b, handle, 0, &info),
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
	struct dcerpc_binding_handle *b = p->binding_handle;

	torture_comment(tctx, "Testing ChangeID: id change test #1\n");

	torture_assert(tctx, test_GetChangeID_PrinterData(tctx, b, handle, &change_id),
		"failed to query for ChangeID");
	torture_assert(tctx, test_GetChangeID_PrinterDataEx(tctx, p, handle, &change_id_ex),
		"failed to query for ChangeID");
	torture_assert(tctx, test_GetChangeID_PrinterInfo(tctx, b, handle, &change_id_info),
		"failed to query for ChangeID");

	torture_assert_int_equal(tctx, change_id, change_id_ex,
		"change_ids should all be equal");
	torture_assert_int_equal(tctx, change_id_ex, change_id_info,
		"change_ids should all be equal");


	torture_comment(tctx, "Testing ChangeID: id change test #2\n");

	torture_assert(tctx, test_GetChangeID_PrinterData(tctx, b, handle, &change_id),
		"failed to query for ChangeID");
	torture_assert(tctx, test_GetPrinter_level(tctx, b, handle, 2, &info),
		"failed to query Printer level 2");
	torture_assert(tctx, test_GetChangeID_PrinterDataEx(tctx, p, handle, &change_id_ex),
		"failed to query for ChangeID");
	torture_assert(tctx, test_GetChangeID_PrinterInfo(tctx, b, handle, &change_id_info),
		"failed to query for ChangeID");
	torture_assert_int_equal(tctx, change_id, change_id_ex,
		"change_id should not have changed");
	torture_assert_int_equal(tctx, change_id_ex, change_id_info,
		"change_id should not have changed");


	torture_comment(tctx, "Testing ChangeID: id change test #3\n");

	torture_assert(tctx, test_GetChangeID_PrinterData(tctx, b, handle, &change_id),
		"failed to query for ChangeID");
	torture_assert(tctx, test_GetChangeID_PrinterDataEx(tctx, p, handle, &change_id_ex),
		"failed to query for ChangeID");
	torture_assert(tctx, test_GetChangeID_PrinterInfo(tctx, b, handle, &change_id_info),
		"failed to query for ChangeID");
	torture_assert(tctx, test_GetPrinter_level(tctx, b, handle, 2, &info),
		"failed to query Printer level 2");
	comment = talloc_strdup(tctx, info.info2.comment);

	{
		struct spoolss_SetPrinterInfoCtr info_ctr;
		struct spoolss_DevmodeContainer devmode_ctr;
		struct sec_desc_buf secdesc_ctr;
		union spoolss_SetPrinterInfo sinfo;

		ZERO_STRUCT(info_ctr);
		ZERO_STRUCT(devmode_ctr);
		ZERO_STRUCT(secdesc_ctr);


		torture_assert(tctx, PrinterInfo_to_SetPrinterInfo(tctx, &info, 2, &sinfo), "");
		sinfo.info2->comment	= "torture_comment";

		info_ctr.level = 2;
		info_ctr.info = sinfo;

		torture_assert(tctx, test_SetPrinter(tctx, b, handle, &info_ctr, &devmode_ctr, &secdesc_ctr, 0),
			"failed to call SetPrinter");

		sinfo.info2->comment	= comment;

		torture_assert(tctx, test_SetPrinter(tctx, b, handle, &info_ctr, &devmode_ctr, &secdesc_ctr, 0),
			"failed to call SetPrinter");

	}

	torture_assert(tctx, test_GetChangeID_PrinterData(tctx, b, handle, &change_id2),
		"failed to query for ChangeID");
	torture_assert(tctx, test_GetChangeID_PrinterDataEx(tctx, p, handle, &change_id_ex2),
		"failed to query for ChangeID");
	torture_assert(tctx, test_GetChangeID_PrinterInfo(tctx, b, handle, &change_id_info2),
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

	torture_comment(tctx, "ChangeID tests succeeded\n\n");

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

	torture_comment(tctx, "Testing close on secondary pipe\n");

	status = dcerpc_parse_binding(tctx, p->conn->binding_string, &b);
	torture_assert_ntstatus_ok(tctx, status, "Failed to parse dcerpc binding");

	status = dcerpc_secondary_connection(p, &p2, b);
	torture_assert_ntstatus_ok(tctx, status, "Failed to create secondary connection");

	status = dcerpc_bind_auth_none(p2, &ndr_table_spoolss);
	torture_assert_ntstatus_ok(tctx, status, "Failed to create bind on secondary connection");

	cp.in.handle = handle;
	cp.out.handle = handle;

	status = dcerpc_spoolss_ClosePrinter_r(p2->binding_handle, tctx, &cp);
	torture_assert_ntstatus_equal(tctx, status, NT_STATUS_RPC_SS_CONTEXT_MISMATCH,
			"ERROR: Allowed close on secondary connection");

	talloc_free(p2);

	return true;
}

static bool test_OpenPrinter_badname(struct torture_context *tctx,
				     struct dcerpc_binding_handle *b, const char *name)
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

	torture_comment(tctx, "Testing OpenPrinter(%s) with bad name\n", op.in.printername);

	status = dcerpc_spoolss_OpenPrinter_r(b, tctx, &op);
	torture_assert_ntstatus_ok(tctx, status, "OpenPrinter failed");
	torture_assert_werr_equal(tctx, op.out.result, WERR_INVALID_PRINTER_NAME,
		"unexpected result");

	if (W_ERROR_IS_OK(op.out.result)) {
		ret &=test_ClosePrinter(tctx, b, &handle);
	}

	opEx.in.printername		= name;
	opEx.in.datatype		= NULL;
	opEx.in.devmode_ctr.devmode	= NULL;
	opEx.in.access_mask		= 0;
	opEx.in.level			= 1;
	opEx.in.userlevel.level1	= NULL;
	opEx.out.handle			= &handle;

	torture_comment(tctx, "Testing OpenPrinterEx(%s) with bad name\n", opEx.in.printername);

	status = dcerpc_spoolss_OpenPrinterEx_r(b, tctx, &opEx);
	torture_assert_ntstatus_ok(tctx, status, "OpenPrinterEx failed");
	torture_assert_werr_equal(tctx, opEx.out.result, WERR_INVALID_PARAM,
		"unexpected result");

	if (W_ERROR_IS_OK(opEx.out.result)) {
		ret &=test_ClosePrinter(tctx, b, &handle);
	}

	return ret;
}

static bool test_OpenPrinter_badname_list(struct torture_context *tctx,
					  void *private_data)
{
	struct test_spoolss_context *ctx =
		talloc_get_type_abort(private_data, struct test_spoolss_context);

	const char *badnames[] = {
		"__INVALID_PRINTER__",
		"\\\\__INVALID_HOST__",
		"",
		"\\\\\\",
		"\\\\\\__INVALID_PRINTER__"
	};
	const char *badname;
	struct dcerpc_pipe *p = ctx->spoolss_pipe;
	const char *server_name = dcerpc_server_name(p);
	struct dcerpc_binding_handle *b = p->binding_handle;
	int i;

	for (i=0; i < ARRAY_SIZE(badnames); i++) {
		torture_assert(tctx,
			test_OpenPrinter_badname(tctx, b, badnames[i]),
			"");
	}

	badname = talloc_asprintf(tctx, "\\\\%s\\", server_name);
	torture_assert(tctx,
		test_OpenPrinter_badname(tctx, b, badname),
		"");

	badname = talloc_asprintf(tctx, "\\\\%s\\__INVALID_PRINTER__", server_name);
	torture_assert(tctx,
		test_OpenPrinter_badname(tctx, b, badname),
		"");

	return true;
}

static bool test_OpenPrinter(struct torture_context *tctx,
			     struct dcerpc_pipe *p,
			     const char *name,
			     const char *environment,
			     bool open_only)
{
	NTSTATUS status;
	struct spoolss_OpenPrinter r;
	struct policy_handle handle;
	bool ret = true;
	struct dcerpc_binding_handle *b = p->binding_handle;

	r.in.printername	= name;
	r.in.datatype		= NULL;
	r.in.devmode_ctr.devmode= NULL;
	r.in.access_mask	= SEC_FLAG_MAXIMUM_ALLOWED;
	r.out.handle		= &handle;

	torture_comment(tctx, "Testing OpenPrinter(%s)\n", r.in.printername);

	status = dcerpc_spoolss_OpenPrinter_r(b, tctx, &r);

	torture_assert_ntstatus_ok(tctx, status, "OpenPrinter failed");

	torture_assert_werr_ok(tctx, r.out.result, "OpenPrinter failed");

	if (open_only) {
		goto close_printer;
	}

	if (!test_GetPrinter(tctx, b, &handle, environment)) {
		ret = false;
	}

	if (!torture_setting_bool(tctx, "samba3", false)) {
		if (!test_SecondaryClosePrinter(tctx, p, &handle)) {
			ret = false;
		}
	}

 close_printer:
	if (!test_ClosePrinter(tctx, b, &handle)) {
		ret = false;
	}

	return ret;
}

static bool test_OpenPrinterEx(struct torture_context *tctx,
			       struct dcerpc_binding_handle *b,
			       const char *printername,
			       const char *datatype,
			       struct spoolss_DeviceMode *devmode,
			       uint32_t access_mask,
			       uint32_t level,
			       union spoolss_UserLevel *userlevel,
			       struct policy_handle *handle,
			       WERROR expected_result)
{
	struct spoolss_OpenPrinterEx r;

	r.in.printername	= printername;
	r.in.datatype		= datatype;
	r.in.devmode_ctr.devmode= devmode;
	r.in.access_mask	= access_mask;
	r.in.level		= level;
	r.in.userlevel		= *userlevel;
	r.out.handle		= handle;

	torture_comment(tctx, "Testing OpenPrinterEx(%s)\n", r.in.printername);

	torture_assert_ntstatus_ok(tctx,
		dcerpc_spoolss_OpenPrinterEx_r(b, tctx, &r),
		"OpenPrinterEx failed");

	torture_assert_werr_equal(tctx, r.out.result, expected_result,
		"OpenPrinterEx failed");

	return true;
}

static bool call_OpenPrinterEx(struct torture_context *tctx,
			       struct dcerpc_pipe *p,
			       const char *name,
			       struct spoolss_DeviceMode *devmode,
			       struct policy_handle *handle)
{
	union spoolss_UserLevel userlevel;
	struct spoolss_UserLevel1 userlevel1;
	struct dcerpc_binding_handle *b = p->binding_handle;

	userlevel1.size = 1234;
	userlevel1.client = "hello";
	userlevel1.user = "spottyfoot!";
	userlevel1.build = 1;
	userlevel1.major = 2;
	userlevel1.minor = 3;
	userlevel1.processor = 4;

	userlevel.level1 = &userlevel1;

	return test_OpenPrinterEx(tctx, b, name, NULL, devmode,
				  SEC_FLAG_MAXIMUM_ALLOWED,
				  1,
				  &userlevel,
				  handle,
				  WERR_OK);
}

static bool test_printer_rename(struct torture_context *tctx,
				void *private_data)
{
	struct torture_printer_context *t =
		(struct torture_printer_context *)talloc_get_type_abort(private_data, struct torture_printer_context);
	struct dcerpc_pipe *p = t->spoolss_pipe;

	bool ret = true;
	union spoolss_PrinterInfo info;
	union spoolss_SetPrinterInfo sinfo;
	struct spoolss_SetPrinterInfoCtr info_ctr;
	struct spoolss_DevmodeContainer devmode_ctr;
	struct sec_desc_buf secdesc_ctr;
	const char *printer_name;
	const char *printer_name_orig;
	const char *printer_name_new = "SAMBA smbtorture Test Printer (Copy 2)";
	struct policy_handle new_handle;
	const char *q;
	struct dcerpc_binding_handle *b = p->binding_handle;

	ZERO_STRUCT(devmode_ctr);
	ZERO_STRUCT(secdesc_ctr);

	torture_comment(tctx, "Testing Printer rename operations\n");

	torture_assert(tctx,
		test_GetPrinter_level(tctx, b, &t->handle, 2, &info),
		"failed to call GetPrinter level 2");

	printer_name_orig = talloc_strdup(tctx, info.info2.printername);

	q = strrchr(info.info2.printername, '\\');
	if (q) {
		torture_warning(tctx,
			"server returns printername %s incl. servername although we did not set servername", info.info2.printername);
	}

	torture_assert(tctx,
		PrinterInfo_to_SetPrinterInfo(tctx, &info, 2, &sinfo), "");

	sinfo.info2->printername = printer_name_new;

	info_ctr.level = 2;
	info_ctr.info = sinfo;

	torture_assert(tctx,
		test_SetPrinter(tctx, b, &t->handle, &info_ctr, &devmode_ctr, &secdesc_ctr, 0),
		"failed to call SetPrinter level 2");

	torture_assert(tctx,
		test_GetPrinter_level(tctx, b, &t->handle, 2, &info),
		"failed to call GetPrinter level 2");

	printer_name = talloc_strdup(tctx, info.info2.printername);

	q = strrchr(info.info2.printername, '\\');
	if (q) {
		torture_warning(tctx,
			"server returns printername %s incl. servername although we did not set servername", info.info2.printername);
		q++;
		printer_name = q;
	}

	torture_assert_str_equal(tctx, printer_name, printer_name_new,
		"new printer name was not set");

	/* samba currently cannot fully rename printers */
	if (!torture_setting_bool(tctx, "samba3", false)) {
		torture_assert(tctx,
			test_OpenPrinter_badname(tctx, b, printer_name_orig),
			"still can open printer with oldname after rename");
	} else {
		torture_warning(tctx, "*not* checking for open with oldname after rename for samba3");
	}

	torture_assert(tctx,
		call_OpenPrinterEx(tctx, p, printer_name_new, NULL, &new_handle),
		"failed to open printer with new name");

	torture_assert(tctx,
		test_GetPrinter_level(tctx, b, &new_handle, 2, &info),
		"failed to call GetPrinter level 2");

	torture_assert_str_equal(tctx, info.info2.printername, printer_name_new,
		"new printer name was not set");

	torture_assert(tctx,
		test_ClosePrinter(tctx, b, &new_handle),
		"failed to close printer");

	torture_comment(tctx, "Printer rename operations test succeeded\n\n");

	return ret;
}

static bool test_openprinter(struct torture_context *tctx,
			     struct dcerpc_binding_handle *b,
			     const char *real_printername)
{
	union spoolss_UserLevel userlevel;
	struct policy_handle handle;
	struct spoolss_UserLevel1 userlevel1;
	const char *printername = NULL;
	int i;

	struct {
		const char *suffix;
		WERROR expected_result;
	} tests[] = {
		{
			.suffix			= "rubbish",
			.expected_result	= WERR_INVALID_PRINTER_NAME
		},{
			.suffix			= ", LocalOnl",
			.expected_result	= WERR_INVALID_PRINTER_NAME
		},{
			.suffix			= ", localOnly",
			.expected_result	= WERR_INVALID_PRINTER_NAME
		},{
			.suffix			= ", localonl",
			.expected_result	= WERR_INVALID_PRINTER_NAME
		},{
			.suffix			= ",LocalOnl",
			.expected_result	= WERR_INVALID_PRINTER_NAME
		},{
			.suffix			= ",localOnl2",
			.expected_result	= WERR_INVALID_PRINTER_NAME
		},{
			.suffix			= ", DrvConver2t",
			.expected_result	= WERR_INVALID_PRINTER_NAME
		},{
			.suffix			= ", drvconvert",
			.expected_result	= WERR_INVALID_PRINTER_NAME
		},{
			.suffix			= ",drvconvert",
			.expected_result	= WERR_INVALID_PRINTER_NAME
		},{
			.suffix			= ", DrvConvert",
			.expected_result	= WERR_OK
		},{
			.suffix			= " , DrvConvert",
			.expected_result	= WERR_INVALID_PRINTER_NAME
		},{
			.suffix			= ",DrvConvert",
			.expected_result	= WERR_OK
		},{
			.suffix			= ", DrvConvertsadfasdf",
			.expected_result	= WERR_OK
		},{
			.suffix			= ",DrvConvertasdfasd",
			.expected_result	= WERR_OK
		},{
			.suffix			= ", LocalOnly",
			.expected_result	= WERR_OK
		},{
			.suffix			= " , LocalOnly",
			.expected_result	= WERR_INVALID_PRINTER_NAME
		},{
			.suffix			= ",LocalOnly",
			.expected_result	= WERR_OK
		},{
			.suffix			= ", LocalOnlysagi4gjfkd",
			.expected_result	= WERR_OK
		},{
			.suffix			= ",LocalOnlysagi4gjfkd",
			.expected_result	= WERR_OK
		}
	};

	userlevel1.size = 1234;
	userlevel1.client = "hello";
	userlevel1.user = "spottyfoot!";
	userlevel1.build = 1;
	userlevel1.major = 2;
	userlevel1.minor = 3;
	userlevel1.processor = 4;

	userlevel.level1 = &userlevel1;

	torture_comment(tctx, "Testing openprinterex printername pattern\n");

	torture_assert(tctx,
		test_OpenPrinterEx(tctx, b, real_printername, NULL, NULL, 0, 1,
				   &userlevel, &handle,
				   WERR_OK),
		"OpenPrinterEx failed");
	test_ClosePrinter(tctx, b, &handle);

	for (i=0; i < ARRAY_SIZE(tests); i++) {

		printername = talloc_asprintf(tctx, "%s%s",
					      real_printername,
					      tests[i].suffix);

		torture_assert(tctx,
			test_OpenPrinterEx(tctx, b, printername, NULL, NULL, 0, 1,
					   &userlevel, &handle,
					   tests[i].expected_result),
			"OpenPrinterEx failed");
		if (W_ERROR_IS_OK(tests[i].expected_result)) {
			test_ClosePrinter(tctx, b, &handle);
		}
	}

	return true;
}


static bool test_existing_printer_openprinterex(struct torture_context *tctx,
						struct dcerpc_pipe *p,
						const char *name,
						const char *environment)
{
	struct policy_handle handle;
	bool ret = true;
	struct dcerpc_binding_handle *b = p->binding_handle;

	if (!test_openprinter(tctx, b, name)) {
		return false;
	}

	if (!call_OpenPrinterEx(tctx, p, name, NULL, &handle)) {
		return false;
	}

	if (!test_PrinterInfo_SD(tctx, b, &handle)) {
		ret = false;
	}

	if (!test_GetPrinter(tctx, b, &handle, environment)) {
		ret = false;
	}

	if (!test_EnumForms_all(tctx, b, &handle, false)) {
		ret = false;
	}

	if (!test_Forms(tctx, b, &handle, false, name, NULL, NULL)) {
		ret = false;
	}

	if (!test_Forms_winreg(tctx, b, &handle, false, name)) {
		ret = false;
	}

	if (!test_EnumPrinterData_all(tctx, p, &handle)) {
		ret = false;
	}

	if (!test_EnumPrinterDataEx(tctx, b, &handle, "PrinterDriverData", NULL, NULL)) {
		ret = false;
	}

	if (!test_EnumPrinterData_consistency(tctx, p, &handle)) {
		ret = false;
	}

	if (!test_printer_all_keys(tctx, b, &handle)) {
		ret = false;
	}

	if (!test_PausePrinter(tctx, b, &handle)) {
		ret = false;
	}

	if (!test_DoPrintTest(tctx, b, &handle)) {
		ret = false;
	}

	if (!test_ResumePrinter(tctx, b, &handle)) {
		ret = false;
	}

	if (!test_SetPrinterData_matrix(tctx, b, &handle, name, NULL, NULL)) {
		ret = false;
	}

	if (!test_SetPrinterDataEx_matrix(tctx, p, &handle, name, NULL, NULL)) {
		ret = false;
	}

	if (!torture_setting_bool(tctx, "samba3", false)) {
		if (!test_SecondaryClosePrinter(tctx, p, &handle)) {
			ret = false;
		}
	}

	if (!test_ClosePrinter(tctx, b, &handle)) {
		ret = false;
	}

	return ret;
}

static bool test_EnumPrinters_old(struct torture_context *tctx,
				  void *private_data)
{
	struct test_spoolss_context *ctx =
		talloc_get_type_abort(private_data, struct test_spoolss_context);
	struct spoolss_EnumPrinters r;
	NTSTATUS status;
	uint16_t levels[] = {1, 2, 4, 5};
	int i;
	bool ret = true;
	struct dcerpc_pipe *p = ctx->spoolss_pipe;
	struct dcerpc_binding_handle *b = p->binding_handle;

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

		status = dcerpc_spoolss_EnumPrinters_r(b, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "EnumPrinters failed");

		if (W_ERROR_EQUAL(r.out.result, WERR_INSUFFICIENT_BUFFER)) {
			DATA_BLOB blob = data_blob_talloc_zero(tctx, needed);
			r.in.buffer = &blob;
			r.in.offered = needed;
			status = dcerpc_spoolss_EnumPrinters_r(b, tctx, &r);
		}

		torture_assert_ntstatus_ok(tctx, status, "EnumPrinters failed");

		torture_assert_werr_ok(tctx, r.out.result, "EnumPrinters failed");

		CHECK_NEEDED_SIZE_ENUM_LEVEL(spoolss_EnumPrinters, info, r.in.level, count, needed, 4);

		if (!info) {
			torture_comment(tctx, "No printers returned\n");
			return true;
		}

		for (j=0;j<count;j++) {
			if (r.in.level == 1) {
				char *unc = talloc_strdup(tctx, info[j].info1.name);
				char *slash, *name, *full_name;
				name = unc;
				if (unc[0] == '\\' && unc[1] == '\\') {
					unc +=2;
				}
				slash = strchr(unc, '\\');
				if (slash) {
					slash++;
					name = slash;
				}
				full_name = talloc_asprintf(tctx, "\\\\%s\\%s",
							    dcerpc_server_name(p), name);
				if (!test_OpenPrinter(tctx, p, name, ctx->environment, true)) {
					ret = false;
				}
				if (!test_OpenPrinter(tctx, p, full_name, ctx->environment, true)) {
					ret = false;
				}
				if (!test_OpenPrinter(tctx, p, name, ctx->environment, false)) {
					ret = false;
				}
				if (!test_existing_printer_openprinterex(tctx, p, name, ctx->environment)) {
					ret = false;
				}
			}
		}
	}

	return ret;
}

static bool test_EnumPrinters_level(struct torture_context *tctx,
				    struct dcerpc_binding_handle *b,
				    uint32_t flags,
				    const char *servername,
				    uint32_t level,
				    uint32_t *count_p,
				    union spoolss_PrinterInfo **info_p)
{
	struct spoolss_EnumPrinters r;
	union spoolss_PrinterInfo *info;
	uint32_t needed;
	uint32_t count;

	r.in.flags	= flags;
	r.in.server	= servername;
	r.in.level	= level;
	r.in.buffer	= NULL;
	r.in.offered	= 0;
	r.out.needed	= &needed;
	r.out.count	= &count;
	r.out.info	= &info;

	torture_comment(tctx, "Testing EnumPrinters(%s) level %u\n",
		r.in.server, r.in.level);

	torture_assert_ntstatus_ok(tctx,
		dcerpc_spoolss_EnumPrinters_r(b, tctx, &r),
		"EnumPrinters failed");
	if (W_ERROR_EQUAL(r.out.result, WERR_INSUFFICIENT_BUFFER)) {
		DATA_BLOB blob = data_blob_talloc_zero(tctx, needed);
		r.in.buffer = &blob;
		r.in.offered = needed;
		torture_assert_ntstatus_ok(tctx,
			dcerpc_spoolss_EnumPrinters_r(b, tctx, &r),
			"EnumPrinters failed");
	}

	torture_assert_werr_ok(tctx, r.out.result, "EnumPrinters failed");

	CHECK_NEEDED_SIZE_ENUM_LEVEL(spoolss_EnumPrinters, info, r.in.level, count, needed, 4);

	if (count_p) {
		*count_p = count;
	}
	if (info_p) {
		*info_p = info;
	}

	return true;
}

static const char *get_short_printername(struct torture_context *tctx,
					 const char *name)
{
	const char *short_name;

	if (name[0] == '\\' && name[1] == '\\') {
		name += 2;
		short_name = strchr(name, '\\');
		if (short_name) {
			return talloc_strdup(tctx, short_name+1);
		}
	}

	return name;
}

static const char *get_full_printername(struct torture_context *tctx,
					const char *name)
{
	const char *full_name = talloc_strdup(tctx, name);
	char *p;

	if (name && name[0] == '\\' && name[1] == '\\') {
		name += 2;
		p = strchr(name, '\\');
		if (p) {
			return full_name;
		}
	}

	return NULL;
}

static bool test_OnePrinter_servername(struct torture_context *tctx,
				       struct dcerpc_pipe *p,
				       struct dcerpc_binding_handle *b,
				       const char *servername,
				       const char *printername)
{
	union spoolss_PrinterInfo info;
	const char *short_name = get_short_printername(tctx, printername);
	const char *full_name = get_full_printername(tctx, printername);

	if (short_name) {
		struct policy_handle handle;
		torture_assert(tctx,
			call_OpenPrinterEx(tctx, p, short_name, NULL, &handle),
			"failed to open printer");

		torture_assert(tctx,
			test_GetPrinter_level(tctx, b, &handle, 2, &info),
			"failed to get printer info");

		torture_assert_casestr_equal(tctx, info.info2.servername, NULL,
			"unexpected servername");
		torture_assert_casestr_equal(tctx, info.info2.printername, short_name,
			"unexpected printername");

		if (info.info2.devmode) {
			const char *expected_devicename;
			expected_devicename = talloc_strndup(tctx, short_name, MIN(strlen(short_name), 31));
			torture_assert_casestr_equal(tctx, info.info2.devmode->devicename, expected_devicename,
				"unexpected devicemode devicename");
		}

		torture_assert(tctx,
			test_ClosePrinter(tctx, b, &handle),
			"failed to close printer");
	}

	if (full_name) {
		struct policy_handle handle;

		torture_assert(tctx,
			call_OpenPrinterEx(tctx, p, full_name, NULL, &handle),
			"failed to open printer");

		torture_assert(tctx,
			test_GetPrinter_level(tctx, b, &handle, 2, &info),
			"failed to get printer info");

		torture_assert_casestr_equal(tctx, info.info2.servername, servername,
			"unexpected servername");
		torture_assert_casestr_equal(tctx, info.info2.printername, full_name,
			"unexpected printername");

		if (info.info2.devmode) {
			const char *expected_devicename;
			expected_devicename = talloc_strndup(tctx, full_name, MIN(strlen(full_name), 31));
			torture_assert_casestr_equal(tctx, info.info2.devmode->devicename, expected_devicename,
				"unexpected devicemode devicename");
		}

		torture_assert(tctx,
			test_ClosePrinter(tctx, b, &handle),
			"failed to close printer");
	}

	return true;
}

static bool test_EnumPrinters_servername(struct torture_context *tctx,
					 void *private_data)
{
	struct test_spoolss_context *ctx =
		talloc_get_type_abort(private_data, struct test_spoolss_context);
	int i;
	struct dcerpc_pipe *p = ctx->spoolss_pipe;
	struct dcerpc_binding_handle *b = p->binding_handle;
	uint32_t count;
	union spoolss_PrinterInfo *info;
	const char *servername;
	uint32_t flags = PRINTER_ENUM_NAME|PRINTER_ENUM_LOCAL;

	torture_comment(tctx, "Testing servername behaviour in EnumPrinters and GetPrinters\n");

	servername = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));

	torture_assert(tctx,
		test_EnumPrinters_level(tctx, b, flags, servername, 2, &count, &info),
		"failed to enumerate printers");

	for (i=0; i < count; i++) {

		torture_assert_casestr_equal(tctx, info[i].info2.servername, servername,
			"unexpected servername");

		torture_assert(tctx,
			test_OnePrinter_servername(tctx, p, b, servername, info[i].info2.printername),
			"failed to check printer");
	}

	servername = "";

	torture_assert(tctx,
		test_EnumPrinters_level(tctx, b, flags, servername, 2, &count, &info),
		"failed to enumerate printers");

	for (i=0; i < count; i++) {

		torture_assert_casestr_equal(tctx, info[i].info2.servername, NULL,
			"unexpected servername");

		torture_assert(tctx,
			test_OnePrinter_servername(tctx, p, b, servername, info[i].info2.printername),
			"failed to check printer");
	}


	return true;
}


static bool test_GetPrinterDriver(struct torture_context *tctx,
				  struct dcerpc_binding_handle *b,
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

	torture_assert_ntstatus_ok(tctx, dcerpc_spoolss_GetPrinterDriver_r(b, tctx, &r),
		"failed to call GetPrinterDriver");
	if (W_ERROR_EQUAL(r.out.result, WERR_INSUFFICIENT_BUFFER)) {
		DATA_BLOB blob = data_blob_talloc_zero(tctx, needed);
		r.in.buffer = &blob;
		r.in.offered = needed;
		torture_assert_ntstatus_ok(tctx, dcerpc_spoolss_GetPrinterDriver_r(b, tctx, &r),
			"failed to call GetPrinterDriver");
	}

	torture_assert_werr_ok(tctx, r.out.result,
		"failed to call GetPrinterDriver");

	CHECK_NEEDED_SIZE_LEVEL(spoolss_DriverInfo, r.out.info, r.in.level, needed, 4);

	return true;
}

static bool test_GetPrinterDriver2_level(struct torture_context *tctx,
					 struct dcerpc_binding_handle *b,
					 struct policy_handle *handle,
					 const char *driver_name,
					 const char *architecture,
					 uint32_t level,
					 uint32_t client_major_version,
					 uint32_t client_minor_version,
					 union spoolss_DriverInfo *info_p,
					 WERROR *result_p)

{
	struct spoolss_GetPrinterDriver2 r;
	uint32_t needed;
	uint32_t server_major_version;
	uint32_t server_minor_version;

	r.in.handle = handle;
	r.in.architecture = architecture;
	r.in.client_major_version = client_major_version;
	r.in.client_minor_version = client_minor_version;
	r.in.buffer = NULL;
	r.in.offered = 0;
	r.in.level = level;
	r.out.needed = &needed;
	r.out.server_major_version = &server_major_version;
	r.out.server_minor_version = &server_minor_version;

	torture_comment(tctx, "Testing GetPrinterDriver2(%s) level %d\n",
		driver_name, r.in.level);

	torture_assert_ntstatus_ok(tctx,
		dcerpc_spoolss_GetPrinterDriver2_r(b, tctx, &r),
		"failed to call GetPrinterDriver2");
	if (W_ERROR_EQUAL(r.out.result, WERR_INSUFFICIENT_BUFFER)) {
		DATA_BLOB blob = data_blob_talloc_zero(tctx, needed);
		r.in.buffer = &blob;
		r.in.offered = needed;
		torture_assert_ntstatus_ok(tctx,
			dcerpc_spoolss_GetPrinterDriver2_r(b, tctx, &r),
			"failed to call GetPrinterDriver2");
	}

	if (result_p) {
		*result_p = r.out.result;
	}

	if (W_ERROR_EQUAL(r.out.result, WERR_INVALID_LEVEL)) {
		switch (r.in.level) {
		case 101:
		case 8:
			torture_comment(tctx,
				"level %d not implemented, not considering as an error\n",
				r.in.level);
			return true;
		default:
			break;
		}
	}

	torture_assert_werr_ok(tctx, r.out.result,
		"failed to call GetPrinterDriver2");

	CHECK_NEEDED_SIZE_LEVEL(spoolss_DriverInfo, r.out.info, r.in.level, needed, 4);

	if (info_p) {
		*info_p = *r.out.info;
	}

	return true;
}

static bool test_GetPrinterDriver2(struct torture_context *tctx,
				   struct dcerpc_binding_handle *b,
				   struct policy_handle *handle,
				   const char *driver_name,
				   const char *architecture)
{
	uint16_t levels[] = {1, 2, 3, 4, 5, 6, 8, 101 };
	int i;


	for (i=0;i<ARRAY_SIZE(levels);i++) {

		torture_assert(tctx,
			test_GetPrinterDriver2_level(tctx, b, handle, driver_name, architecture, levels[i], 3, 0, NULL, NULL),
			"");
	}

	return true;
}

static bool test_EnumPrinterDrivers_old(struct torture_context *tctx,
					void *private_data)
{
	struct test_spoolss_context *ctx =
		talloc_get_type_abort(private_data, struct test_spoolss_context);
	uint16_t levels[] = {1, 2, 3, 4, 5, 6};
	int i;
	struct dcerpc_pipe *p = ctx->spoolss_pipe;
	struct dcerpc_binding_handle *b = p->binding_handle;
	const char *server_name = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));

	for (i=0;i<ARRAY_SIZE(levels);i++) {

		uint32_t count;
		union spoolss_DriverInfo *info;

		torture_assert(tctx,
			test_EnumPrinterDrivers_args(tctx, b, server_name, ctx->environment, levels[i], &count, &info),
			"failed to enumerate drivers");

		if (!info) {
			torture_comment(tctx, "No printer drivers returned\n");
			break;
		}
	}

	return true;
}

static bool test_DeletePrinter(struct torture_context *tctx,
			       struct dcerpc_binding_handle *b,
			       struct policy_handle *handle)
{
	struct spoolss_DeletePrinter r;

	torture_comment(tctx, "Testing DeletePrinter\n");

	r.in.handle = handle;

	torture_assert_ntstatus_ok(tctx, dcerpc_spoolss_DeletePrinter_r(b, tctx, &r),
		"failed to delete printer");
	torture_assert_werr_ok(tctx, r.out.result,
		"failed to delete printer");

	return true;
}

static bool test_EnumPrinters_findname(struct torture_context *tctx,
				       struct dcerpc_binding_handle *b,
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

	torture_assert_ntstatus_ok(tctx, dcerpc_spoolss_EnumPrinters_r(b, tctx, &e),
		"failed to enum printers");

	if (W_ERROR_EQUAL(e.out.result, WERR_INSUFFICIENT_BUFFER)) {
		DATA_BLOB blob = data_blob_talloc_zero(tctx, needed);
		e.in.buffer = &blob;
		e.in.offered = needed;

		torture_assert_ntstatus_ok(tctx, dcerpc_spoolss_EnumPrinters_r(b, tctx, &e),
			"failed to enum printers");
	}

	torture_assert_werr_ok(tctx, e.out.result,
		"failed to enum printers");

	for (i=0; i < count; i++) {

		const char *current = NULL;
		const char *q;

		switch (level) {
		case 1:
			current = info[i].info1.name;
			break;
		}

		if (strequal(current, name)) {
			*found = true;
			break;
		}

		q = strrchr(current, '\\');
		if (q) {
			if (!e.in.server) {
				torture_warning(tctx,
					"server returns printername %s incl. servername although we did not set servername", current);
			}
			q++;
			if (strequal(q, name)) {
				*found = true;
				break;
			}
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
	struct dcerpc_binding_handle *b = p->binding_handle;

	ZERO_STRUCT(devmode_ctr);
	ZERO_STRUCT(secdesc_ctr);
	ZERO_STRUCT(userlevel_ctr);
	ZERO_STRUCT(info1);

	torture_comment(tctx, "Testing AddPrinter%s(%s) level 1\n",
			ex ? "Ex":"", printername);

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

	torture_assert_ntstatus_ok(tctx, ex ? dcerpc_spoolss_AddPrinterEx_r(b, tctx, &rex) :
					      dcerpc_spoolss_AddPrinter_r(b, tctx, &r),
		"failed to add printer");
	result = ex ? rex.out.result : r.out.result;
	torture_assert_werr_equal(tctx, result, WERR_INVALID_PRINTER_NAME,
		"unexpected result code");

	info1.name = printername;
	info1.flags = PRINTER_ATTRIBUTE_SHARED;

	torture_assert_ntstatus_ok(tctx, ex ? dcerpc_spoolss_AddPrinterEx_r(b, tctx, &rex) :
					      dcerpc_spoolss_AddPrinter_r(b, tctx, &r),
		"failed to add printer");
	result = ex ? rex.out.result : r.out.result;
	torture_assert_werr_equal(tctx, result, WERR_PRINTER_ALREADY_EXISTS,
		"unexpected result code");

	/* bizarre protocol, WERR_PRINTER_ALREADY_EXISTS means success here,
	   better do a real check to see the printer is really there */

	torture_assert(tctx, test_EnumPrinters_findname(tctx, b,
							PRINTER_ENUM_NETWORK, 1,
							printername,
							&found),
			"failed to enum printers");

	torture_assert(tctx, found, "failed to find newly added printer");

	info1.flags = 0;

	torture_assert_ntstatus_ok(tctx, ex ? dcerpc_spoolss_AddPrinterEx_r(b, tctx, &rex) :
					      dcerpc_spoolss_AddPrinter_r(b, tctx, &r),
		"failed to add printer");
	result = ex ? rex.out.result : r.out.result;
	torture_assert_werr_equal(tctx, result, WERR_PRINTER_ALREADY_EXISTS,
		"unexpected result code");

	/* bizarre protocol, WERR_PRINTER_ALREADY_EXISTS means success here,
	   better do a real check to see the printer has really been removed
	   from the well known printer list */

	found = false;

	torture_assert(tctx, test_EnumPrinters_findname(tctx, b,
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
				   struct spoolss_DeviceMode *devmode,
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
	bool existing_printer_deleted = false;
	struct dcerpc_binding_handle *b = p->binding_handle;

	ZERO_STRUCT(devmode_ctr);
	ZERO_STRUCT(secdesc_ctr);
	ZERO_STRUCT(userlevel_ctr);

	torture_comment(tctx, "Testing AddPrinter%s(%s) level 2\n",
			ex ? "Ex":"", printername);

	devmode_ctr.devmode = devmode;

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

	torture_assert_ntstatus_ok(tctx, ex ? dcerpc_spoolss_AddPrinterEx_r(b, tctx, &rex) :
					      dcerpc_spoolss_AddPrinter_r(b, tctx, &r),
		"failed to add printer");
	result = ex ? rex.out.result : r.out.result;
	torture_assert_werr_equal(tctx, result, WERR_INVALID_PRINTER_NAME,
		"unexpected result code");

	info2.printername = printername;

	torture_assert_ntstatus_ok(tctx, ex ? dcerpc_spoolss_AddPrinterEx_r(b, tctx, &rex) :
					      dcerpc_spoolss_AddPrinter_r(b, tctx, &r),
		"failed to add printer");
	result = ex ? rex.out.result : r.out.result;

	if (W_ERROR_EQUAL(result, WERR_PRINTER_ALREADY_EXISTS)) {
		struct policy_handle printer_handle;

		if (existing_printer_deleted) {
			torture_fail(tctx, "already deleted printer still existing?");
		}

		torture_assert(tctx, call_OpenPrinterEx(tctx, p, printername, NULL, &printer_handle),
			"failed to open printer handle");

		torture_assert(tctx, test_DeletePrinter(tctx, b, &printer_handle),
			"failed to delete printer");

		torture_assert(tctx, test_ClosePrinter(tctx, b, &printer_handle),
			"failed to close server handle");

		existing_printer_deleted = true;

		goto again;
	}

	torture_assert_werr_equal(tctx, result, WERR_UNKNOWN_PORT,
		"unexpected result code");

	info2.portname = portname;

	torture_assert_ntstatus_ok(tctx, ex ? dcerpc_spoolss_AddPrinterEx_r(b, tctx, &rex) :
					      dcerpc_spoolss_AddPrinter_r(b, tctx, &r),
		"failed to add printer");
	result = ex ? rex.out.result : r.out.result;
	torture_assert_werr_equal(tctx, result, WERR_UNKNOWN_PRINTER_DRIVER,
		"unexpected result code");

	info2.drivername = drivername;

	torture_assert_ntstatus_ok(tctx, ex ? dcerpc_spoolss_AddPrinterEx_r(b, tctx, &rex) :
					      dcerpc_spoolss_AddPrinter_r(b, tctx, &r),
		"failed to add printer");
	result = ex ? rex.out.result : r.out.result;

	/* w2k8r2 allows to add printer w/o defining printprocessor */

	if (!W_ERROR_IS_OK(result)) {
		torture_assert_werr_equal(tctx, result, WERR_UNKNOWN_PRINTPROCESSOR,
			"unexpected result code");

		info2.printprocessor = "winprint";

		torture_assert_ntstatus_ok(tctx, ex ? dcerpc_spoolss_AddPrinterEx_r(b, tctx, &rex) :
						      dcerpc_spoolss_AddPrinter_r(b, tctx, &r),
			"failed to add printer");
		result = ex ? rex.out.result : r.out.result;
		torture_assert_werr_ok(tctx, result,
			"failed to add printer");
	}

	*handle_p = handle;

	/* we are paranoid, really check if the printer is there now */

	torture_assert(tctx, test_EnumPrinters_findname(tctx, b,
							PRINTER_ENUM_LOCAL, 1,
							printername,
							&found),
			"failed to enum printers");
	torture_assert(tctx, found, "failed to find newly added printer");

	torture_assert_ntstatus_ok(tctx, ex ? dcerpc_spoolss_AddPrinterEx_r(b, tctx, &rex) :
					      dcerpc_spoolss_AddPrinter_r(b, tctx, &r),
		"failed to add printer");
	result = ex ? rex.out.result : r.out.result;
	torture_assert_werr_equal(tctx, result, WERR_PRINTER_ALREADY_EXISTS,
		"unexpected result code");

	return true;
}

static bool test_printer_info(struct torture_context *tctx,
			      void *private_data)
{
	struct torture_printer_context *t =
		(struct torture_printer_context *)talloc_get_type_abort(private_data, struct torture_printer_context);
	struct dcerpc_pipe *p = t->spoolss_pipe;
	struct dcerpc_binding_handle *b = p->binding_handle;

	bool ret = true;

	if (torture_setting_bool(tctx, "samba3", false)) {
		torture_skip(tctx, "skipping printer info cross tests against samba 3");
	}

	if (!test_PrinterInfo(tctx, b, &t->handle)) {
		ret = false;
	}

	if (!test_SetPrinter_errors(tctx, b, &t->handle)) {
		ret = false;
	}

	return ret;
}

static bool test_EnumPrinterKey(struct torture_context *tctx,
				struct dcerpc_binding_handle *b,
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

		torture_assert_ntstatus_ok(tctx, dcerpc_spoolss_EnumPrinterKey_r(b, tctx, &r),
			"failed to call EnumPrinterKey");
		if (W_ERROR_EQUAL(r.out.result, WERR_MORE_DATA)) {

			torture_assert(tctx, (_ndr_size == r.in.offered/2),
				talloc_asprintf(tctx, "EnumPrinterKey size mismatch, _ndr_size %d (expected %d)",
					_ndr_size, r.in.offered/2));

			r.in.offered = needed;
			torture_assert_ntstatus_ok(tctx, dcerpc_spoolss_EnumPrinterKey_r(b, tctx, &r),
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

bool test_printer_all_keys(struct torture_context *tctx,
			   struct dcerpc_binding_handle *b,
			   struct policy_handle *handle)
{
	const char **key_array = NULL;
	int i;

	torture_comment(tctx, "Testing Printer Keys\n");

	torture_assert(tctx, test_EnumPrinterKey(tctx, b, handle, "", &key_array),
		"failed to call test_EnumPrinterKey");

	for (i=0; key_array && key_array[i]; i++) {
		torture_assert(tctx, test_EnumPrinterKey(tctx, b, handle, key_array[i], NULL),
			"failed to call test_EnumPrinterKey");
	}
	for (i=0; key_array && key_array[i]; i++) {
		torture_assert(tctx, test_EnumPrinterDataEx(tctx, b, handle, key_array[i], NULL, NULL),
			"failed to call test_EnumPrinterDataEx");
	}

	torture_comment(tctx, "Printer Keys test succeeded\n\n");

	return true;
}

static bool test_openprinter_wrap(struct torture_context *tctx,
				  void *private_data)
{
	struct torture_printer_context *t =
		(struct torture_printer_context *)talloc_get_type_abort(private_data, struct torture_printer_context);
	struct dcerpc_pipe *p = t->spoolss_pipe;
	struct dcerpc_binding_handle *b = p->binding_handle;
	const char *printername = t->info2.printername;

	return test_openprinter(tctx, b, printername);
}

static bool test_csetprinter(struct torture_context *tctx,
			     void *private_data)
{
	struct torture_printer_context *t =
		(struct torture_printer_context *)talloc_get_type_abort(private_data, struct torture_printer_context);
	struct dcerpc_pipe *p = t->spoolss_pipe;

	const char *printername = talloc_asprintf(tctx, "%s2", t->info2.printername);
	const char *drivername = t->added_driver ? t->driver.info8.driver_name : t->info2.drivername;
	const char *portname = t->info2.portname;

	union spoolss_PrinterInfo info;
	struct policy_handle new_handle, new_handle2;
	struct dcerpc_binding_handle *b = p->binding_handle;

	torture_comment(tctx, "Testing c_setprinter\n");

	torture_assert(tctx,
		test_GetPrinter_level(tctx, b, &t->handle, 0, &info),
		"failed to get level 0 printer info");
	torture_comment(tctx, "csetprinter on initial printer handle: %d\n",
		info.info0.c_setprinter);

	/* check if c_setprinter on 1st handle increases after a printer has
	 * been added */

	torture_assert(tctx,
		test_AddPrinter_normal(tctx, p, &new_handle, printername, drivername, portname, NULL, false),
		"failed to add new printer");
	torture_assert(tctx,
		test_GetPrinter_level(tctx, b, &t->handle, 0, &info),
		"failed to get level 0 printer info");
	torture_comment(tctx, "csetprinter on initial printer handle (after add): %d\n",
		info.info0.c_setprinter);

	/* check if c_setprinter on new handle increases after a printer has
	 * been added */

	torture_assert(tctx,
		test_GetPrinter_level(tctx, b, &new_handle, 0, &info),
		"failed to get level 0 printer info");
	torture_comment(tctx, "csetprinter on created handle: %d\n",
		info.info0.c_setprinter);

	/* open the new printer and check if c_setprinter increases */

	torture_assert(tctx,
		call_OpenPrinterEx(tctx, p, printername, NULL, &new_handle2),
		"failed to open created printer");
	torture_assert(tctx,
		test_GetPrinter_level(tctx, b, &new_handle2, 0, &info),
		"failed to get level 0 printer info");
	torture_comment(tctx, "csetprinter on new handle (after openprinter): %d\n",
		info.info0.c_setprinter);

	/* cleanup */

	torture_assert(tctx,
		test_ClosePrinter(tctx, b, &new_handle2),
		"failed to close printer");
	torture_assert(tctx,
		test_DeletePrinter(tctx, b, &new_handle),
		"failed to delete new printer");

	return true;
}

static bool compose_local_driver_directory(struct torture_context *tctx,
					   const char *environment,
					   const char *local_dir,
					   const char **path)
{
	char *p;

	p = strrchr(local_dir, '/');
	if (!p) {
		return NULL;
	}
	p++;

	if (strequal(environment, "Windows x64")) {
		if (!strequal(p, "x64")) {
			*path = talloc_asprintf(tctx, "%s/x64", local_dir);
		}
	} else if (strequal(environment, "Windows NT x86")) {
		if (!strequal(p, "i386")) {
			*path = talloc_asprintf(tctx, "%s/i386", local_dir);
		}
	} else {
		torture_assert(tctx, "unknown environment: '%s'\n", environment);
	}

	return true;
}

static struct spoolss_DeviceMode *torture_devicemode(TALLOC_CTX *mem_ctx,
						     const char *devicename)
{
	struct spoolss_DeviceMode *r;

	r = talloc_zero(mem_ctx, struct spoolss_DeviceMode);
	if (r == NULL) {
		return NULL;
	}

	r->devicename		= talloc_strdup(r, devicename);
	r->specversion		= DMSPEC_NT4_AND_ABOVE;
	r->driverversion	= 0x0600;
	r->size			= 0x00dc;
	r->__driverextra_length = 0;
	r->fields		= DEVMODE_FORMNAME |
				  DEVMODE_TTOPTION |
				  DEVMODE_PRINTQUALITY |
				  DEVMODE_DEFAULTSOURCE |
				  DEVMODE_COPIES |
				  DEVMODE_SCALE |
				  DEVMODE_PAPERSIZE |
				  DEVMODE_ORIENTATION;
	r->orientation		= DMORIENT_PORTRAIT;
	r->papersize		= DMPAPER_LETTER;
	r->paperlength		= 0;
	r->paperwidth		= 0;
	r->scale		= 100;
	r->copies		= 55;
	r->defaultsource	= DMBIN_FORMSOURCE;
	r->printquality		= DMRES_HIGH;
	r->color		= DMRES_MONOCHROME;
	r->duplex		= DMDUP_SIMPLEX;
	r->yresolution		= 0;
	r->ttoption		= DMTT_SUBDEV;
	r->collate		= DMCOLLATE_FALSE;
	r->formname		= talloc_strdup(r, "Letter");

	return r;
}

static bool test_architecture_buffer(struct torture_context *tctx,
				     void *private_data)
{
	struct test_spoolss_context *ctx =
		talloc_get_type_abort(private_data, struct test_spoolss_context);

	struct spoolss_OpenPrinterEx r;
	struct spoolss_UserLevel1 u1;
	struct policy_handle handle;
	uint32_t architectures[] = {
		PROCESSOR_ARCHITECTURE_INTEL,
		PROCESSOR_ARCHITECTURE_IA64,
		PROCESSOR_ARCHITECTURE_AMD64
	};
	uint32_t needed[3];
	int i;
	struct dcerpc_pipe *p = ctx->spoolss_pipe;
	struct dcerpc_binding_handle *b = p->binding_handle;

	for (i=0; i < ARRAY_SIZE(architectures); i++) {

		torture_comment(tctx, "Testing OpenPrinterEx with architecture %d\n", architectures[i]);

		u1.size = 0;
		u1.client = NULL;
		u1.user = NULL;
		u1.build = 0;
		u1.major = 3;
		u1.minor = 0;
		u1.processor = architectures[i];

		r.in.printername	= talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
		r.in.datatype		= NULL;
		r.in.devmode_ctr.devmode= NULL;
		r.in.access_mask	= SEC_FLAG_MAXIMUM_ALLOWED;
		r.in.level		 = 1;
		r.in.userlevel.level1	= &u1;
		r.out.handle		= &handle;

		torture_assert_ntstatus_ok(tctx, dcerpc_spoolss_OpenPrinterEx_r(b, tctx, &r), "");
		torture_assert_werr_ok(tctx, r.out.result, "");

		{
			struct spoolss_EnumPrinters e;
			uint32_t count;
			union spoolss_PrinterInfo *info;

			e.in.flags = PRINTER_ENUM_LOCAL;
			e.in.server = NULL;
			e.in.level = 2;
			e.in.buffer = NULL;
			e.in.offered = 0;
			e.out.count = &count;
			e.out.info = &info;
			e.out.needed = &needed[i];

			torture_assert_ntstatus_ok(tctx, dcerpc_spoolss_EnumPrinters_r(b, tctx, &e), "");
#if 0
			torture_comment(tctx, "needed was %d\n", needed[i]);
#endif
		}

		torture_assert(tctx, test_ClosePrinter(tctx, b, &handle), "");
	}

	for (i=1; i < ARRAY_SIZE(architectures); i++) {
		if (needed[i-1] != needed[i]) {
			torture_fail(tctx,
				talloc_asprintf(tctx, "needed size %d for architecture %d != needed size %d for architecture %d\n",
						needed[i-1], architectures[i-1], needed[i], architectures[i]));
		}
	}

	return true;
}

static bool test_PrintServer_Forms_Winreg(struct torture_context *tctx,
					  void *private_data)
{
	struct test_spoolss_context *ctx =
		talloc_get_type_abort(private_data, struct test_spoolss_context);
	struct dcerpc_pipe *p = ctx->spoolss_pipe;
	struct dcerpc_binding_handle *b = p->binding_handle;

	return test_Forms_winreg(tctx, b, &ctx->server_handle, true, NULL);
}

static bool test_PrintServer_Forms(struct torture_context *tctx,
				   void *private_data)
{
	struct test_spoolss_context *ctx =
		talloc_get_type_abort(private_data, struct test_spoolss_context);
	struct dcerpc_pipe *p = ctx->spoolss_pipe;
	struct dcerpc_binding_handle *b = p->binding_handle;

	return test_Forms(tctx, b, &ctx->server_handle, true, NULL, NULL, NULL);
}

static bool test_PrintServer_EnumForms(struct torture_context *tctx,
				       void *private_data)
{
	struct test_spoolss_context *ctx =
		talloc_get_type_abort(private_data, struct test_spoolss_context);
	struct dcerpc_pipe *p = ctx->spoolss_pipe;
	struct dcerpc_binding_handle *b = p->binding_handle;

	return test_EnumForms_all(tctx, b, &ctx->server_handle, true);
}

static bool torture_rpc_spoolss_setup_common(struct torture_context *tctx, struct test_spoolss_context *t)
{
	NTSTATUS status;

	status = torture_rpc_connection(tctx, &t->spoolss_pipe, &ndr_table_spoolss);

	torture_assert_ntstatus_ok(tctx, status, "Error connecting to server");

	torture_assert(tctx,
		test_OpenPrinter_server(tctx, t->spoolss_pipe, &t->server_handle),
		"failed to open printserver");
	torture_assert(tctx,
		test_get_environment(tctx, t->spoolss_pipe->binding_handle, &t->server_handle, &t->environment),
		"failed to get environment");

	return true;
}

static bool torture_rpc_spoolss_setup(struct torture_context *tctx, void **data)
{
	struct test_spoolss_context *t;

	*data = t = talloc_zero(tctx, struct test_spoolss_context);

	return torture_rpc_spoolss_setup_common(tctx, t);
}

static bool torture_rpc_spoolss_teardown_common(struct torture_context *tctx, struct test_spoolss_context *t)
{
	test_ClosePrinter(tctx, t->spoolss_pipe->binding_handle, &t->server_handle);

	return true;
}

static bool torture_rpc_spoolss_teardown(struct torture_context *tctx, void *data)
{
	struct test_spoolss_context *t = talloc_get_type(data, struct test_spoolss_context);
	bool ret;

	ret = torture_rpc_spoolss_teardown_common(tctx, t);
	talloc_free(t);

	return ret;
}

static bool torture_rpc_spoolss_printer_setup_common(struct torture_context *tctx, struct torture_printer_context *t)
{
	struct dcerpc_pipe *p;
	struct dcerpc_binding_handle *b;
	const char *server_name_slash;
	const char *driver_name;
	const char *printer_name;
	const char *port_name;

	torture_assert_ntstatus_ok(tctx,
		torture_rpc_connection(tctx, &t->spoolss_pipe, &ndr_table_spoolss),
		"Error connecting to server");

	p = t->spoolss_pipe;
	b = p->binding_handle;
	server_name_slash = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));

	t->driver.info8.version			= SPOOLSS_DRIVER_VERSION_200X;
	t->driver.info8.driver_name		= TORTURE_DRIVER;
	t->driver.info8.driver_path		= "pscript5.dll";
	t->driver.info8.data_file		= "cups6.ppd";
	t->driver.info8.config_file		= "ps5ui.dll";
	t->driver.info8.help_file		= "pscript.hlp";
	t->driver.info8.default_datatype	= "RAW";
	t->driver.info8.dependent_files		= talloc_zero(t, struct spoolss_StringArray);
	t->driver.info8.dependent_files->string = talloc_zero_array(t, const char *, 8 + 1);
	t->driver.info8.dependent_files->string[0] = "pscript5.dll";
	t->driver.info8.dependent_files->string[1] = "cups6.ppd";
	t->driver.info8.dependent_files->string[2] = "ps5ui.dll";
	t->driver.info8.dependent_files->string[3] = "pscript.hlp";
	t->driver.info8.dependent_files->string[4] = "pscript.ntf";
	t->driver.info8.dependent_files->string[5] = "cups6.ini";
	t->driver.info8.dependent_files->string[6] = "cupsps6.dll";
	t->driver.info8.dependent_files->string[7] = "cupsui6.dll";

	t->driver.local.driver_directory= "/usr/share/cups/drivers";

	t->info2.drivername		= "Microsoft XPS Document Writer";
	t->info2.portname		= "LPT1:";

	printer_name = t->info2.printername;
	port_name = t->info2.portname;

	torture_assert(tctx,
		fillup_printserver_info(tctx, p, &t->driver),
		"failed to fillup printserver info");

	t->driver.info8.architecture = talloc_strdup(t, t->driver.remote.environment);

	torture_assert(tctx,
		compose_local_driver_directory(tctx, t->driver.remote.environment,
					       t->driver.local.driver_directory,
					       &t->driver.local.driver_directory),
		"failed to compose local driver directory");

	if (test_EnumPrinterDrivers_findone(tctx, b, server_name_slash, t->driver.remote.environment, 3, t->info2.drivername, NULL)) {
		torture_comment(tctx, "driver '%s' (architecture: %s, version: 3) is present on server\n",
			t->info2.drivername, t->driver.remote.environment);
		t->have_driver = true;
		goto try_add;
	}

	torture_comment(tctx, "driver '%s' (architecture: %s, version: 3) does not exist on the server\n",
		t->info2.drivername, t->driver.remote.environment);
	torture_comment(tctx, "trying to upload own driver\n");

	if (!directory_exist(t->driver.local.driver_directory)) {
		torture_warning(tctx, "no local driver is available!");
		t->have_driver = false;
		goto try_add;
	}

	torture_assert(tctx,
		upload_printer_driver(tctx, dcerpc_server_name(p), &t->driver),
		"failed to upload printer driver");

	torture_assert(tctx,
		test_AddPrinterDriver_args_level_3(tctx, b, server_name_slash, &t->driver.info8, 0, false, NULL),
		"failed to add driver");

	t->added_driver = true;
	t->have_driver = true;

 try_add:
	driver_name = t->added_driver ? t->driver.info8.driver_name : t->info2.drivername;

	if (t->wellknown) {
		torture_assert(tctx,
			test_AddPrinter_wellknown(tctx, p, printer_name, t->ex),
			"failed to add wellknown printer");
	} else {
		torture_assert(tctx,
			test_AddPrinter_normal(tctx, p, &t->handle, printer_name, driver_name, port_name, t->devmode, t->ex),
			"failed to add printer");
	}

	return true;
}

static bool torture_rpc_spoolss_printer_setup(struct torture_context *tctx, void **data)
{
	struct torture_printer_context *t;

	*data = t = talloc_zero(tctx, struct torture_printer_context);

	t->ex			= false;
	t->wellknown		= false;
	t->info2.printername	= TORTURE_PRINTER;
	t->devmode		= NULL;

	return torture_rpc_spoolss_printer_setup_common(tctx, t);
}

static bool torture_rpc_spoolss_printerex_setup(struct torture_context *tctx, void **data)
{
	struct torture_printer_context *t;

	*data = t = talloc_zero(tctx, struct torture_printer_context);

	t->ex			= true;
	t->wellknown		= false;
	t->info2.printername	= TORTURE_PRINTER_EX;
	t->devmode		= NULL;

	return torture_rpc_spoolss_printer_setup_common(tctx, t);
}

static bool torture_rpc_spoolss_printerwkn_setup(struct torture_context *tctx, void **data)
{
	struct torture_printer_context *t;

	*data = t = talloc_zero(tctx, struct torture_printer_context);

	t->ex			= false;
	t->wellknown		= true;
	t->info2.printername	= TORTURE_WELLKNOWN_PRINTER;
	t->devmode		= NULL;

	/* FIXME */
	if (t->wellknown) {
		torture_skip(tctx, "skipping AddPrinter level 1");
	}

	return torture_rpc_spoolss_printer_setup_common(tctx, t);
}

static bool torture_rpc_spoolss_printerexwkn_setup(struct torture_context *tctx, void **data)
{
	struct torture_printer_context *t;

	*data = t = talloc_zero(tctx, struct torture_printer_context);

	t->ex			= true;
	t->wellknown		= true;
	t->info2.printername	= TORTURE_WELLKNOWN_PRINTER_EX;
	t->devmode		= NULL;

	/* FIXME */
	if (t->wellknown) {
		torture_skip(tctx, "skipping AddPrinterEx level 1");
	}

	return torture_rpc_spoolss_printer_setup_common(tctx, t);
}

static bool torture_rpc_spoolss_printerdm_setup(struct torture_context *tctx, void **data)
{
	struct torture_printer_context *t;

	*data = t = talloc_zero(tctx, struct torture_printer_context);

	t->ex			= true;
	t->wellknown		= false;
	t->info2.printername	= TORTURE_PRINTER_EX;
	t->devmode		= torture_devicemode(t, TORTURE_PRINTER_EX);

	return torture_rpc_spoolss_printer_setup_common(tctx, t);
}

static bool torture_rpc_spoolss_printer_teardown_common(struct torture_context *tctx, struct torture_printer_context *t)
{
	bool found = false;
	struct dcerpc_pipe *p = t->spoolss_pipe;
	struct dcerpc_binding_handle *b;
	const char *printer_name = t->info2.printername;

	if (t->added_driver) {
		torture_assert(tctx,
			remove_printer_driver(tctx, dcerpc_server_name(t->spoolss_pipe), &t->driver),
			"failed to remove printer driver");
	}

	if (p) {
		b = p->binding_handle;
	}

	if (!t->wellknown) {
		torture_assert(tctx,
			test_DeletePrinter(tctx, b, &t->handle),
			"failed to delete printer");

		torture_assert(tctx,
			test_EnumPrinters_findname(tctx, b, PRINTER_ENUM_LOCAL, 1,
						   printer_name, &found),
			"failed to enumerate printers");

		torture_assert(tctx, !found, "deleted printer still there");
	}

	return true;
}

static bool torture_rpc_spoolss_printer_teardown(struct torture_context *tctx, void *data)
{
	struct torture_printer_context *t = talloc_get_type(data, struct torture_printer_context);
	bool ret;

	ret = torture_rpc_spoolss_printer_teardown_common(tctx, t);
	talloc_free(t);

	return ret;
}

static bool test_print_test(struct torture_context *tctx,
			    void *private_data)
{
	struct torture_printer_context *t =
		(struct torture_printer_context *)talloc_get_type_abort(private_data, struct torture_printer_context);
	struct dcerpc_pipe *p = t->spoolss_pipe;
	struct dcerpc_binding_handle *b = p->binding_handle;

	torture_assert(tctx,
		test_PausePrinter(tctx, b, &t->handle),
		"failed to pause printer");

	torture_assert(tctx,
		test_DoPrintTest(tctx, b, &t->handle),
		"failed to do print test");

	torture_assert(tctx,
		test_ResumePrinter(tctx, b, &t->handle),
		"failed to resume printer");

	return true;
}

static bool test_print_test_extended(struct torture_context *tctx,
				     void *private_data)
{
	struct torture_printer_context *t =
		(struct torture_printer_context *)talloc_get_type_abort(private_data, struct torture_printer_context);
	struct dcerpc_pipe *p = t->spoolss_pipe;
	struct dcerpc_binding_handle *b = p->binding_handle;
	bool ret = true;

	torture_assert(tctx,
		test_PausePrinter(tctx, b, &t->handle),
		"failed to pause printer");

	ret = test_DoPrintTest_extended(tctx, b, &t->handle);
	if (ret == false) {
		torture_comment(tctx, "WARNING! failed to do extended print test\n");
		if (torture_setting_bool(tctx, "samba3", false)) {
			torture_comment(tctx, "non-critical for samba3\n");
			ret = true;
			tctx->last_result = TORTURE_SKIP;
		}
	}

	torture_assert(tctx,
		test_ResumePrinter(tctx, b, &t->handle),
		"failed to resume printer");

	return ret;
}

/* use smbd file IO to spool a print job */
static bool test_print_test_smbd(struct torture_context *tctx,
				 void *private_data)
{
	struct torture_printer_context *t =
		(struct torture_printer_context *)talloc_get_type_abort(private_data, struct torture_printer_context);
	struct dcerpc_pipe *p = t->spoolss_pipe;
	struct dcerpc_binding_handle *b = p->binding_handle;
	NTSTATUS status;
	uint32_t count;
	union spoolss_JobInfo *info = NULL;
	int i;

	struct smb2_tree *tree;
	struct smb2_handle job_h;
	struct cli_credentials *credentials = cmdline_credentials;
	struct smbcli_options options;
	TALLOC_CTX *mem_ctx = talloc_new(tctx);
	/*
	 * Do not test against the dynamically added printers, printing via
	 * smbd means that a different spoolss process may handle the
	 * OpenPrinter request to the one that handled the AddPrinter request.
	 * This currently leads to an ugly race condition where one process
	 * sees the new printer and one doesn't.
	 */
	const char *share = TORTURE_PRINTER_STATIC1;

	torture_comment(tctx, "Testing smbd job spooling\n");
	lpcfg_smbcli_options(tctx->lp_ctx, &options);

	status = smb2_connect(tctx,
			      torture_setting_string(tctx, "host", NULL),
			      lpcfg_smb_ports(tctx->lp_ctx),
			      share,
			      lpcfg_resolve_context(tctx->lp_ctx),
			      credentials,
			      &tree,
			      tctx->ev,
			      &options,
			      lpcfg_socket_options(tctx->lp_ctx),
			      lpcfg_gensec_settings(tctx, tctx->lp_ctx));
	if (!NT_STATUS_IS_OK(status)) {
		printf("Failed to connect to SMB2 printer %s - %s\n",
		       share, nt_errstr(status));
		return false;
	}

	status = torture_smb2_testfile(tree, "smbd_spooler_job", &job_h);
	torture_assert_ntstatus_ok(tctx, status, "smbd spool job create");

	status = smb2_util_write(tree, job_h, "exciting print job data", 0,
				 sizeof("exciting print job data"));
	torture_assert_ntstatus_ok(tctx, status, "smbd spool job write");

	/* check back end spoolss job was created */
	torture_assert(tctx,
		test_EnumJobs_args(tctx, b, &t->handle, 1, &count, &info),
		"EnumJobs level 1 failed");

	for (i = 0; i < count; i++) {
		if (!strcmp(info[i].info1.document_name, "smbd_spooler_job")) {
			break;
		}
	}
	torture_assert(tctx, (i != count), "smbd_spooler_job not found");

	status = smb2_util_close(tree, job_h);
	torture_assert_ntstatus_ok(tctx, status, "smbd spool job close");

	/* disconnect from printer share */
	talloc_free(mem_ctx);

	return true;
}

static bool test_printer_sd(struct torture_context *tctx,
			    void *private_data)
{
	struct torture_printer_context *t =
		(struct torture_printer_context *)talloc_get_type_abort(private_data, struct torture_printer_context);
	struct dcerpc_pipe *p = t->spoolss_pipe;
	struct dcerpc_binding_handle *b = p->binding_handle;

	torture_assert(tctx,
		test_PrinterInfo_SD(tctx, b, &t->handle),
		"failed to test security descriptors");

	return true;
}

static bool test_printer_dm(struct torture_context *tctx,
			    void *private_data)
{
	struct torture_printer_context *t =
		(struct torture_printer_context *)talloc_get_type_abort(private_data, struct torture_printer_context);
	struct dcerpc_pipe *p = t->spoolss_pipe;

	torture_assert(tctx,
		test_PrinterInfo_DevMode(tctx, p, &t->handle, t->info2.printername, t->devmode),
		"failed to test devicemodes");

	return true;
}

static bool test_printer_info_winreg(struct torture_context *tctx,
				     void *private_data)
{
	struct torture_printer_context *t =
		(struct torture_printer_context *)talloc_get_type_abort(private_data, struct torture_printer_context);
	struct dcerpc_pipe *p = t->spoolss_pipe;

	torture_assert(tctx,
		test_PrinterInfo_winreg(tctx, p, &t->handle, t->info2.printername),
		"failed to test printer info winreg");

	return true;
}

static bool test_printer_change_id(struct torture_context *tctx,
				   void *private_data)
{
	struct torture_printer_context *t =
		(struct torture_printer_context *)talloc_get_type_abort(private_data, struct torture_printer_context);
	struct dcerpc_pipe *p = t->spoolss_pipe;

	torture_assert(tctx,
		test_ChangeID(tctx, p, &t->handle),
		"failed to test change id");

	return true;
}

static bool test_printer_keys(struct torture_context *tctx,
			      void *private_data)
{
	struct torture_printer_context *t =
		(struct torture_printer_context *)talloc_get_type_abort(private_data, struct torture_printer_context);
	struct dcerpc_pipe *p = t->spoolss_pipe;
	struct dcerpc_binding_handle *b = p->binding_handle;

	torture_assert(tctx,
		test_printer_all_keys(tctx, b, &t->handle),
		"failed to test printer keys");

	return true;
}

static bool test_printer_data_consistency(struct torture_context *tctx,
					  void *private_data)
{
	struct torture_printer_context *t =
		(struct torture_printer_context *)talloc_get_type_abort(private_data, struct torture_printer_context);
	struct dcerpc_pipe *p = t->spoolss_pipe;

	torture_assert(tctx,
		test_EnumPrinterData_consistency(tctx, p, &t->handle),
		"failed to test printer data consistency");

	return true;
}

static bool test_printer_data_keys(struct torture_context *tctx,
				   void *private_data)
{
	struct torture_printer_context *t =
		(struct torture_printer_context *)talloc_get_type_abort(private_data, struct torture_printer_context);
	struct dcerpc_pipe *p = t->spoolss_pipe;

	torture_assert(tctx,
		test_SetPrinterDataEx_keys(tctx, p, &t->handle),
		"failed to test printer data keys");

	return true;
}

static bool test_printer_data_values(struct torture_context *tctx,
				     void *private_data)
{
	struct torture_printer_context *t =
		(struct torture_printer_context *)talloc_get_type_abort(private_data, struct torture_printer_context);
	struct dcerpc_pipe *p = t->spoolss_pipe;

	torture_assert(tctx,
		test_SetPrinterDataEx_values(tctx, p, &t->handle),
		"failed to test printer data values");

	return true;
}

static bool test_printer_data_set(struct torture_context *tctx,
				  void *private_data)
{
	struct torture_printer_context *t =
		(struct torture_printer_context *)talloc_get_type_abort(private_data, struct torture_printer_context);
	struct dcerpc_pipe *p = t->spoolss_pipe;

	torture_assert(tctx,
		test_SetPrinterDataEx_matrix(tctx, p, &t->handle, t->info2.printername, NULL, NULL),
		"failed to test printer data set");

	return true;
}

static bool test_printer_data_winreg(struct torture_context *tctx,
				     void *private_data)
{
	struct torture_printer_context *t =
		(struct torture_printer_context *)talloc_get_type_abort(private_data, struct torture_printer_context);
	struct dcerpc_pipe *p = t->spoolss_pipe;

	torture_assert(tctx,
		test_PrinterData_winreg(tctx, p, &t->handle, t->info2.printername),
		"failed to test printer data winreg");

	return true;
}

static bool test_printer_data_dsspooler(struct torture_context *tctx,
					void *private_data)
{
	struct torture_printer_context *t =
		(struct torture_printer_context *)talloc_get_type_abort(private_data, struct torture_printer_context);
	struct dcerpc_pipe *p = t->spoolss_pipe;

	torture_assert(tctx,
		test_PrinterData_DsSpooler(tctx, p, &t->handle, t->info2.printername),
		"failed to test printer data winreg dsspooler");

	return true;
}

static bool test_driver_info_winreg(struct torture_context *tctx,
				    void *private_data)
{
	struct torture_printer_context *t =
		(struct torture_printer_context *)talloc_get_type_abort(private_data, struct torture_printer_context);
	struct dcerpc_pipe *p = t->spoolss_pipe;
	const char *driver_name = t->added_driver ? t->driver.info8.driver_name : t->info2.drivername;

	if (!t->have_driver) {
		torture_skip(tctx, "skipping driver info winreg test as we don't have a driver");
	}

	torture_assert(tctx,
		test_DriverInfo_winreg(tctx, p, &t->handle, t->info2.printername, driver_name, t->driver.remote.environment, 3),
		"failed to test driver info winreg");

	return true;
}

void torture_tcase_printer(struct torture_tcase *tcase)
{
	torture_tcase_add_simple_test(tcase, "openprinter", test_openprinter_wrap);
	torture_tcase_add_simple_test(tcase, "csetprinter", test_csetprinter);
	torture_tcase_add_simple_test(tcase, "print_test", test_print_test);
	torture_tcase_add_simple_test(tcase, "print_test_extended", test_print_test_extended);
	torture_tcase_add_simple_test(tcase, "print_test_smbd", test_print_test_smbd);
	torture_tcase_add_simple_test(tcase, "printer_info", test_printer_info);
	torture_tcase_add_simple_test(tcase, "sd", test_printer_sd);
	torture_tcase_add_simple_test(tcase, "dm", test_printer_dm);
	torture_tcase_add_simple_test(tcase, "printer_info_winreg", test_printer_info_winreg);
	torture_tcase_add_simple_test(tcase, "change_id", test_printer_change_id);
	torture_tcase_add_simple_test(tcase, "keys", test_printer_keys);
	torture_tcase_add_simple_test(tcase, "printerdata_consistency", test_printer_data_consistency);
	torture_tcase_add_simple_test(tcase, "printerdata_keys", test_printer_data_keys);
	torture_tcase_add_simple_test(tcase, "printerdata_values", test_printer_data_values);
	torture_tcase_add_simple_test(tcase, "printerdata_set", test_printer_data_set);
	torture_tcase_add_simple_test(tcase, "printerdata_winreg", test_printer_data_winreg);
	torture_tcase_add_simple_test(tcase, "printerdata_dsspooler", test_printer_data_dsspooler);
	torture_tcase_add_simple_test(tcase, "driver_info_winreg", test_driver_info_winreg);
	torture_tcase_add_simple_test(tcase, "printer_rename", test_printer_rename);
}

struct torture_suite *torture_rpc_spoolss_printer(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "printer");
	struct torture_tcase *tcase;

	tcase = torture_suite_add_tcase(suite, "addprinter");

	torture_tcase_set_fixture(tcase,
				  torture_rpc_spoolss_printer_setup,
				  torture_rpc_spoolss_printer_teardown);

	torture_tcase_printer(tcase);

	tcase = torture_suite_add_tcase(suite, "addprinterex");

	torture_tcase_set_fixture(tcase,
				  torture_rpc_spoolss_printerex_setup,
				  torture_rpc_spoolss_printer_teardown);

	torture_tcase_printer(tcase);

	tcase = torture_suite_add_tcase(suite, "addprinterwkn");

	torture_tcase_set_fixture(tcase,
				  torture_rpc_spoolss_printerwkn_setup,
				  torture_rpc_spoolss_printer_teardown);

	tcase = torture_suite_add_tcase(suite, "addprinterexwkn");

	torture_tcase_set_fixture(tcase,
				  torture_rpc_spoolss_printerexwkn_setup,
				  torture_rpc_spoolss_printer_teardown);

#if 0
	/* test is not correct */
	tcase = torture_suite_add_tcase(suite, "addprinterdm");

	torture_tcase_set_fixture(tcase,
				  torture_rpc_spoolss_printerdm_setup,
				  torture_rpc_spoolss_printer_teardown);

	torture_tcase_printer(tcase);
#endif
	return suite;
}

struct torture_suite *torture_rpc_spoolss(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "spoolss");
	struct torture_tcase *tcase = torture_suite_add_tcase(suite, "printserver");

	torture_tcase_set_fixture(tcase,
				  torture_rpc_spoolss_setup,
				  torture_rpc_spoolss_teardown);

	torture_tcase_add_simple_test(tcase, "openprinter_badnamelist", test_OpenPrinter_badname_list);
	torture_tcase_add_simple_test(tcase, "printer_data_list", test_GetPrinterData_list);
	torture_tcase_add_simple_test(tcase, "enum_forms", test_PrintServer_EnumForms);
	torture_tcase_add_simple_test(tcase, "forms", test_PrintServer_Forms);
	torture_tcase_add_simple_test(tcase, "forms_winreg", test_PrintServer_Forms_Winreg);
	torture_tcase_add_simple_test(tcase, "enum_ports", test_EnumPorts);
	torture_tcase_add_simple_test(tcase, "add_port", test_AddPort);
	torture_tcase_add_simple_test(tcase, "get_printer_driver_directory", test_GetPrinterDriverDirectory);
	torture_tcase_add_simple_test(tcase, "get_print_processor_directory", test_GetPrintProcessorDirectory);
	torture_tcase_add_simple_test(tcase, "enum_printer_drivers", test_EnumPrinterDrivers);
	torture_tcase_add_simple_test(tcase, "enum_monitors", test_EnumMonitors);
	torture_tcase_add_simple_test(tcase, "enum_print_processors", test_EnumPrintProcessors);
	torture_tcase_add_simple_test(tcase, "print_processors_winreg", test_print_processors_winreg);
	torture_tcase_add_simple_test(tcase, "enum_printprocdata", test_EnumPrintProcDataTypes);
	torture_tcase_add_simple_test(tcase, "enum_printers", test_EnumPrinters);
	torture_tcase_add_simple_test(tcase, "enum_ports_old", test_EnumPorts_old);
	torture_tcase_add_simple_test(tcase, "enum_printers_old", test_EnumPrinters_old);
	torture_tcase_add_simple_test(tcase, "enum_printers_servername", test_EnumPrinters_servername);
	torture_tcase_add_simple_test(tcase, "enum_printer_drivers_old", test_EnumPrinterDrivers_old);
	torture_tcase_add_simple_test(tcase, "architecture_buffer", test_architecture_buffer);

	torture_suite_add_suite(suite, torture_rpc_spoolss_printer(suite));

	return suite;
}

static bool test_GetPrinterDriverDirectory_getdir(struct torture_context *tctx,
						  struct dcerpc_binding_handle *b,
						  const char *server,
						  const char *environment,
						  const char **dir_p)
{
	struct spoolss_GetPrinterDriverDirectory r;
	uint32_t needed;

	r.in.server		= server;
	r.in.environment	= environment;
	r.in.level		= 1;
	r.in.buffer		= NULL;
	r.in.offered		= 0;
	r.out.needed		= &needed;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_spoolss_GetPrinterDriverDirectory_r(b, tctx, &r),
		"failed to query driver directory");

	if (W_ERROR_EQUAL(r.out.result, WERR_INSUFFICIENT_BUFFER)) {
		DATA_BLOB blob = data_blob_talloc_zero(tctx, needed);
		r.in.buffer = &blob;
		r.in.offered = needed;

		torture_assert_ntstatus_ok(tctx,
			dcerpc_spoolss_GetPrinterDriverDirectory_r(b, tctx, &r),
			"failed to query driver directory");
	}

	torture_assert_werr_ok(tctx, r.out.result,
		"failed to query driver directory");

	if (dir_p) {
		*dir_p = r.out.info->info1.directory_name;
	}

	return true;
}

static const char *get_driver_from_info(struct spoolss_AddDriverInfoCtr *info_ctr)
{
	if (info_ctr == NULL) {
		return NULL;
	}

	switch (info_ctr->level) {
	case 1:
		return info_ctr->info.info1->driver_name;
	case 2:
		return info_ctr->info.info2->driver_name;
	case 3:
		return info_ctr->info.info3->driver_name;
	case 4:
		return info_ctr->info.info4->driver_name;
	case 6:
		return info_ctr->info.info6->driver_name;
	case 8:
		return info_ctr->info.info8->driver_name;
	default:
		return NULL;
	}
}

static const char *get_environment_from_info(struct spoolss_AddDriverInfoCtr *info_ctr)
{
	if (info_ctr == NULL) {
		return NULL;
	}

	switch (info_ctr->level) {
	case 2:
		return info_ctr->info.info2->architecture;
	case 3:
		return info_ctr->info.info3->architecture;
	case 4:
		return info_ctr->info.info4->architecture;
	case 6:
		return info_ctr->info.info6->architecture;
	case 8:
		return info_ctr->info.info8->architecture;
	default:
		return NULL;
	}
}


static bool test_AddPrinterDriver_exp(struct torture_context *tctx,
				      struct dcerpc_binding_handle *b,
				      const char *servername,
				      struct spoolss_AddDriverInfoCtr *info_ctr,
				      WERROR expected_result)
{
	struct spoolss_AddPrinterDriver r;
	const char *drivername = get_driver_from_info(info_ctr);
	const char *environment = get_environment_from_info(info_ctr);

	r.in.servername = servername;
	r.in.info_ctr = info_ctr;

	torture_comment(tctx, "Testing AddPrinterDriver(%s) level: %d, environment: '%s'\n",
		drivername, info_ctr->level, environment);

	torture_assert_ntstatus_ok(tctx,
		dcerpc_spoolss_AddPrinterDriver_r(b, tctx, &r),
		"spoolss_AddPrinterDriver failed");
	torture_assert_werr_equal(tctx, r.out.result, expected_result,
		"spoolss_AddPrinterDriver failed with unexpected result");

	return true;

}

static bool test_AddPrinterDriverEx_exp(struct torture_context *tctx,
				        struct dcerpc_binding_handle *b,
					const char *servername,
				        struct spoolss_AddDriverInfoCtr *info_ctr,
				        uint32_t flags,
					WERROR expected_result)
{
	struct spoolss_AddPrinterDriverEx r;
	const char *drivername = get_driver_from_info(info_ctr);
	const char *environment = get_environment_from_info(info_ctr);

	r.in.servername = servername;
	r.in.info_ctr = info_ctr;
	r.in.flags = flags;

	torture_comment(tctx, "Testing AddPrinterDriverEx(%s) level: %d, environment: '%s'\n",
		drivername, info_ctr->level, environment);

	torture_assert_ntstatus_ok(tctx,
		dcerpc_spoolss_AddPrinterDriverEx_r(b, tctx, &r),
		"AddPrinterDriverEx failed");
	torture_assert_werr_equal(tctx, r.out.result, expected_result,
		"AddPrinterDriverEx failed with unexpected result");

	return true;
}

#define ASSERT_DRIVER_PATH(tctx, path, driver_dir, cmt) \
	if (path && strlen(path)) {\
		torture_assert_strn_equal(tctx, path, driver_dir, strlen(driver_dir), cmt); \
	}

static bool test_AddPrinterDriver_args_level_1(struct torture_context *tctx,
					       struct dcerpc_binding_handle *b,
					       const char *server_name,
					       struct spoolss_AddDriverInfo8 *r,
					       uint32_t flags,
					       bool ex,
					       const char *remote_driver_dir)
{
	struct spoolss_AddDriverInfoCtr info_ctr;
	struct spoolss_AddDriverInfo1 info1;

	ZERO_STRUCT(info1);

	info_ctr.level = 1;
	info_ctr.info.info1 = &info1;

	if (ex) {
		torture_assert(tctx,
			test_AddPrinterDriverEx_exp(tctx, b, server_name, &info_ctr, flags, WERR_UNKNOWN_LEVEL),
			"failed to test AddPrinterDriverEx level 1");
	} else {
		torture_assert(tctx,
			test_AddPrinterDriver_exp(tctx, b, server_name, &info_ctr, WERR_UNKNOWN_LEVEL),
			"failed to test AddPrinterDriver level 1");
	}

	info1.driver_name = r->driver_name;

	if (ex) {
		torture_assert(tctx,
			test_AddPrinterDriverEx_exp(tctx, b, server_name, &info_ctr, flags, WERR_UNKNOWN_LEVEL),
			"failed to test AddPrinterDriverEx level 1");
	} else {
		torture_assert(tctx,
			test_AddPrinterDriver_exp(tctx, b, server_name, &info_ctr, WERR_UNKNOWN_LEVEL),
			"failed to test AddPrinterDriver level 1");
	}

	return true;
}

static bool test_AddPrinterDriver_args_level_2(struct torture_context *tctx,
					       struct dcerpc_binding_handle *b,
					       const char *server_name,
					       struct spoolss_AddDriverInfo8 *r,
					       uint32_t flags,
					       bool ex,
					       const char *remote_driver_dir)
{
	struct spoolss_AddDriverInfoCtr info_ctr;
	struct spoolss_AddDriverInfo2 info2;
	union spoolss_DriverInfo info;

	ZERO_STRUCT(info2);

	info_ctr.level = 2;
	info_ctr.info.info2 = &info2;

	if (ex) {
		torture_assert(tctx,
			test_AddPrinterDriverEx_exp(tctx, b, server_name, &info_ctr, flags, WERR_INVALID_PARAM),
			"failed to test AddPrinterDriverEx level 2");
	} else {
		torture_assert(tctx,
			test_AddPrinterDriver_exp(tctx, b, server_name, &info_ctr, WERR_INVALID_PARAM),
			"failed to test AddPrinterDriver level 2");
	}

	info2.driver_name = r->driver_name;

	if (ex) {
		torture_assert(tctx,
			test_AddPrinterDriverEx_exp(tctx, b, server_name, &info_ctr, flags, WERR_INVALID_PARAM),
			"failed to test AddPrinterDriverEx level 2");
	} else {
		torture_assert(tctx,
			test_AddPrinterDriver_exp(tctx, b, server_name, &info_ctr, WERR_INVALID_PARAM),
			"failed to test AddPrinterDriver level 2");
	}

	info2.version = r->version;

	if (ex) {
		torture_assert(tctx,
			test_AddPrinterDriverEx_exp(tctx, b, server_name, &info_ctr, flags, WERR_INVALID_PARAM),
			"failed to test AddPrinterDriverEx level 2");
	} else {
		torture_assert(tctx,
			test_AddPrinterDriver_exp(tctx, b, server_name, &info_ctr, WERR_INVALID_PARAM),
			"failed to test AddPrinterDriver level 2");
	}

	info2.architecture = r->architecture;

	if (ex) {
		torture_assert(tctx,
			test_AddPrinterDriverEx_exp(tctx, b, server_name, &info_ctr, flags, WERR_INVALID_PARAM),
			"failed to test AddPrinterDriverEx level 2");
	} else {
		torture_assert(tctx,
			test_AddPrinterDriver_exp(tctx, b, server_name, &info_ctr, WERR_INVALID_PARAM),
			"failed to test AddPrinterDriver level 2");
	}

	info2.driver_path = r->driver_path;

	if (ex) {
		torture_assert(tctx,
			test_AddPrinterDriverEx_exp(tctx, b, server_name, &info_ctr, flags, WERR_INVALID_PARAM),
			"failed to test AddPrinterDriverEx level 2");
	} else {
		torture_assert(tctx,
			test_AddPrinterDriver_exp(tctx, b, server_name, &info_ctr, WERR_INVALID_PARAM),
			"failed to test AddPrinterDriver level 2");
	}

	info2.data_file = r->data_file;

	if (ex) {
		torture_assert(tctx,
			test_AddPrinterDriverEx_exp(tctx, b, server_name, &info_ctr, flags, WERR_INVALID_PARAM),
			"failed to test AddPrinterDriverEx level 2");
	} else {
		torture_assert(tctx,
			test_AddPrinterDriver_exp(tctx, b, server_name, &info_ctr, WERR_INVALID_PARAM),
			"failed to test AddPrinterDriver level 2");
	}

	info2.config_file = r->config_file;

	if (ex) {
		torture_assert(tctx,
			test_AddPrinterDriverEx_exp(tctx, b, server_name, &info_ctr, 0, WERR_INVALID_PARAM),
			"failed to test AddPrinterDriverEx");
	}

	if (ex) {
		torture_assert(tctx,
			test_AddPrinterDriverEx_exp(tctx, b, server_name, &info_ctr, flags, WERR_OK),
			"failed to test AddPrinterDriverEx level 2");
	} else {
		torture_assert(tctx,
			test_AddPrinterDriver_exp(tctx, b, server_name, &info_ctr, WERR_OK),
			"failed to test AddPrinterDriver level 2");
	}

	torture_assert(tctx,
		test_EnumPrinterDrivers_findone(tctx, b, server_name, r->architecture, 2, r->driver_name, &info),
		"failed to find added printer driver");

	if (remote_driver_dir) {
		ASSERT_DRIVER_PATH(tctx, info.info2.driver_path, remote_driver_dir, "unexpected path");
		ASSERT_DRIVER_PATH(tctx, info.info2.data_file, remote_driver_dir, "unexpected path");
		ASSERT_DRIVER_PATH(tctx, info.info2.config_file, remote_driver_dir, "unexpected path");
	}

	return true;
}

static bool test_AddPrinterDriver_args_level_3(struct torture_context *tctx,
					       struct dcerpc_binding_handle *b,
					       const char *server_name,
					       struct spoolss_AddDriverInfo8 *r,
					       uint32_t flags,
					       bool ex,
					       const char *remote_driver_dir)
{
	struct spoolss_AddDriverInfoCtr info_ctr;
	struct spoolss_AddDriverInfo3 info3;
	union spoolss_DriverInfo info;

	info3.driver_name	= r->driver_name;
	info3.version		= r->version;
	info3.architecture	= r->architecture;
	info3.driver_path	= r->driver_path;
	info3.data_file		= r->data_file;
	info3.config_file	= r->config_file;
	info3.help_file		= r->help_file;
	info3.monitor_name	= r->monitor_name;
	info3.default_datatype	= r->default_datatype;
	info3._ndr_size_dependent_files = r->_ndr_size_dependent_files;
	info3.dependent_files	= r->dependent_files;

	info_ctr.level = 3;
	info_ctr.info.info3 = &info3;

	if (ex) {
		torture_assert(tctx,
			test_AddPrinterDriverEx_exp(tctx, b, server_name, &info_ctr, flags, WERR_OK),
			"failed to test AddPrinterDriverEx level 3");
	} else {
		torture_assert(tctx,
			test_AddPrinterDriver_exp(tctx, b, server_name, &info_ctr, WERR_OK),
			"failed to test AddPrinterDriver level 3");
	}

	torture_assert(tctx,
		test_EnumPrinterDrivers_findone(tctx, b, server_name, r->architecture, 3, r->driver_name, &info),
		"failed to find added printer driver");

	if (remote_driver_dir) {
		int i;
		ASSERT_DRIVER_PATH(tctx, info.info3.driver_path, remote_driver_dir, "unexpected path");
		ASSERT_DRIVER_PATH(tctx, info.info3.data_file, remote_driver_dir, "unexpected path");
		ASSERT_DRIVER_PATH(tctx, info.info3.config_file, remote_driver_dir, "unexpected path");
		ASSERT_DRIVER_PATH(tctx, info.info3.help_file, remote_driver_dir, "unexpected path");
		for (i=0; info.info3.dependent_files && info.info3.dependent_files[i] != NULL; i++) {
			ASSERT_DRIVER_PATH(tctx, info.info3.dependent_files[i], remote_driver_dir, "unexpected path");
		}
	}

	return true;
}

static bool test_AddPrinterDriver_args_level_4(struct torture_context *tctx,
					       struct dcerpc_binding_handle *b,
					       const char *server_name,
					       struct spoolss_AddDriverInfo8 *r,
					       uint32_t flags,
					       bool ex,
					       const char *remote_driver_dir)
{
	struct spoolss_AddDriverInfoCtr info_ctr;
	struct spoolss_AddDriverInfo4 info4;
	union spoolss_DriverInfo info;

	info4.version		= r->version;
	info4.driver_name	= r->driver_name;
	info4.architecture	= r->architecture;
	info4.driver_path	= r->driver_path;
	info4.data_file		= r->data_file;
	info4.config_file	= r->config_file;
	info4.help_file		= r->help_file;
	info4.monitor_name	= r->monitor_name;
	info4.default_datatype	= r->default_datatype;
	info4._ndr_size_dependent_files = r->_ndr_size_dependent_files;
	info4.dependent_files	= r->dependent_files;
	info4._ndr_size_previous_names = r->_ndr_size_previous_names;
	info4.previous_names = r->previous_names;

	info_ctr.level = 4;
	info_ctr.info.info4 = &info4;

	if (ex) {
		torture_assert(tctx,
			test_AddPrinterDriverEx_exp(tctx, b, server_name, &info_ctr, flags, WERR_OK),
			"failed to test AddPrinterDriverEx level 4");
	} else {
		torture_assert(tctx,
			test_AddPrinterDriver_exp(tctx, b, server_name, &info_ctr, WERR_OK),
			"failed to test AddPrinterDriver level 4");
	}

	torture_assert(tctx,
		test_EnumPrinterDrivers_findone(tctx, b, server_name, r->architecture, 4, r->driver_name, &info),
		"failed to find added printer driver");

	if (remote_driver_dir) {
		int i;
		ASSERT_DRIVER_PATH(tctx, info.info4.driver_path, remote_driver_dir, "unexpected path");
		ASSERT_DRIVER_PATH(tctx, info.info4.data_file, remote_driver_dir, "unexpected path");
		ASSERT_DRIVER_PATH(tctx, info.info4.config_file, remote_driver_dir, "unexpected path");
		ASSERT_DRIVER_PATH(tctx, info.info4.help_file, remote_driver_dir, "unexpected path");
		for (i=0; info.info4.dependent_files && info.info4.dependent_files[i] != NULL; i++) {
			ASSERT_DRIVER_PATH(tctx, info.info4.dependent_files[i], remote_driver_dir, "unexpected path");
		}
	}

	return true;
}

static bool test_AddPrinterDriver_args_level_6(struct torture_context *tctx,
					       struct dcerpc_binding_handle *b,
					       const char *server_name,
					       struct spoolss_AddDriverInfo8 *r,
					       uint32_t flags,
					       bool ex,
					       const char *remote_driver_dir)
{
	struct spoolss_AddDriverInfoCtr info_ctr;
	struct spoolss_AddDriverInfo6 info6;
	union spoolss_DriverInfo info;

	info6.version		= r->version;
	info6.driver_name	= r->driver_name;
	info6.architecture	= r->architecture;
	info6.driver_path	= r->driver_path;
	info6.data_file		= r->data_file;
	info6.config_file	= r->config_file;
	info6.help_file		= r->help_file;
	info6.monitor_name	= r->monitor_name;
	info6.default_datatype	= r->default_datatype;
	info6._ndr_size_dependent_files = r->_ndr_size_dependent_files;
	info6.dependent_files	= r->dependent_files;
	info6._ndr_size_previous_names = r->_ndr_size_previous_names;
	info6.previous_names	= r->previous_names;
	info6.driver_date	= r->driver_date;
	info6.driver_version	= r->driver_version;
	info6.manufacturer_name	= r->manufacturer_name;
	info6.manufacturer_url	= r->manufacturer_url;
	info6.hardware_id	= r->hardware_id;
	info6.provider		= r->provider;

	info_ctr.level = 6;
	info_ctr.info.info6 = &info6;

	if (ex) {
		torture_assert(tctx,
			test_AddPrinterDriverEx_exp(tctx, b, server_name, &info_ctr, flags, WERR_OK),
			"failed to test AddPrinterDriverEx level 6");
	} else {
		torture_assert(tctx,
			test_AddPrinterDriver_exp(tctx, b, server_name, &info_ctr, WERR_UNKNOWN_LEVEL),
			"failed to test AddPrinterDriver level 6");
	}

	/* spoolss_AddPrinterDriver does not deal with level 6 or 8 - gd */

	if (!ex) {
		return true;
	}

	torture_assert(tctx,
		test_EnumPrinterDrivers_findone(tctx, b, server_name, r->architecture, 6, r->driver_name, &info),
		"failed to find added printer driver");

	if (remote_driver_dir) {
		int i;
		ASSERT_DRIVER_PATH(tctx, info.info6.driver_path, remote_driver_dir, "unexpected path");
		ASSERT_DRIVER_PATH(tctx, info.info6.data_file, remote_driver_dir, "unexpected path");
		ASSERT_DRIVER_PATH(tctx, info.info6.config_file, remote_driver_dir, "unexpected path");
		ASSERT_DRIVER_PATH(tctx, info.info6.help_file, remote_driver_dir, "unexpected path");
		for (i=0; info.info6.dependent_files && info.info6.dependent_files[i] != NULL; i++) {
			ASSERT_DRIVER_PATH(tctx, info.info6.dependent_files[i], remote_driver_dir, "unexpected path");
		}
	}

	torture_assert_nttime_equal(tctx, info.info6.driver_date, info6.driver_date, "driverdate mismatch");
	torture_assert_u64_equal(tctx, info.info6.driver_version, info6.driver_version, "driverversion mismatch");

	return true;
}

static bool test_AddPrinterDriver_args_level_8(struct torture_context *tctx,
					       struct dcerpc_binding_handle *b,
					       const char *server_name,
					       struct spoolss_AddDriverInfo8 *r,
					       uint32_t flags,
					       bool ex,
					       const char *remote_driver_dir)
{
	struct spoolss_AddDriverInfoCtr info_ctr;
	union spoolss_DriverInfo info;

	info_ctr.level = 8;
	info_ctr.info.info8 = r;

	if (ex) {
		torture_assert(tctx,
			test_AddPrinterDriverEx_exp(tctx, b, server_name, &info_ctr, flags, WERR_OK),
			"failed to test AddPrinterDriverEx level 8");
	} else {
		torture_assert(tctx,
			test_AddPrinterDriver_exp(tctx, b, server_name, &info_ctr, WERR_UNKNOWN_LEVEL),
			"failed to test AddPrinterDriver level 8");
	}

	/* spoolss_AddPrinterDriver does not deal with level 6 or 8 - gd */

	if (!ex) {
		return true;
	}

	torture_assert(tctx,
		test_EnumPrinterDrivers_findone(tctx, b, server_name, r->architecture, 8, r->driver_name, &info),
		"failed to find added printer driver");

	if (remote_driver_dir) {
		int i;
		ASSERT_DRIVER_PATH(tctx, info.info8.driver_path, remote_driver_dir, "unexpected path");
		ASSERT_DRIVER_PATH(tctx, info.info8.data_file, remote_driver_dir, "unexpected path");
		ASSERT_DRIVER_PATH(tctx, info.info8.config_file, remote_driver_dir, "unexpected path");
		ASSERT_DRIVER_PATH(tctx, info.info8.help_file, remote_driver_dir, "unexpected path");
		for (i=0; info.info8.dependent_files && info.info8.dependent_files[i] != NULL; i++) {
			ASSERT_DRIVER_PATH(tctx, info.info8.dependent_files[i], remote_driver_dir, "unexpected path");
		}
	}

	torture_assert_nttime_equal(tctx, info.info8.driver_date, r->driver_date, "driverdate mismatch");
	torture_assert_u64_equal(tctx, info.info8.driver_version, r->driver_version, "driverversion mismatch");

	return true;
}

#undef ASSERT_DRIVER_PATH

static bool test_DeletePrinterDriver_exp(struct torture_context *tctx,
					 struct dcerpc_binding_handle *b,
					 const char *server,
					 const char *driver,
					 const char *environment,
					 WERROR expected_result)
{
	struct spoolss_DeletePrinterDriver r;

	r.in.server = server;
	r.in.architecture = environment;
	r.in.driver = driver;

	torture_comment(tctx, "Testing DeletePrinterDriver(%s)\n", driver);

	torture_assert_ntstatus_ok(tctx,
		dcerpc_spoolss_DeletePrinterDriver_r(b, tctx, &r),
		"DeletePrinterDriver failed");
	torture_assert_werr_equal(tctx, r.out.result, expected_result,
		"DeletePrinterDriver failed with unexpected result");

	return true;
}

static bool test_DeletePrinterDriverEx_exp(struct torture_context *tctx,
					   struct dcerpc_binding_handle *b,
					   const char *server,
					   const char *driver,
					   const char *environment,
					   uint32_t delete_flags,
					   uint32_t version,
					   WERROR expected_result)
{
	struct spoolss_DeletePrinterDriverEx r;

	r.in.server = server;
	r.in.architecture = environment;
	r.in.driver = driver;
	r.in.delete_flags = delete_flags;
	r.in.version = version;

	torture_comment(tctx, "Testing DeletePrinterDriverEx(%s)\n", driver);

	torture_assert_ntstatus_ok(tctx,
		dcerpc_spoolss_DeletePrinterDriverEx_r(b, tctx, &r),
		"DeletePrinterDriverEx failed");
	torture_assert_werr_equal(tctx, r.out.result, expected_result,
		"DeletePrinterDriverEx failed with unexpected result");

	return true;
}

static bool test_DeletePrinterDriver(struct torture_context *tctx,
				     struct dcerpc_binding_handle *b,
				     const char *server_name,
				     const char *driver,
				     const char *environment)
{
	torture_assert(tctx,
		test_DeletePrinterDriver_exp(tctx, b, server_name, driver, "FOOBAR", WERR_INVALID_ENVIRONMENT),
		"failed to delete driver");

	torture_assert(tctx,
		test_DeletePrinterDriver_exp(tctx, b, server_name, driver, environment, WERR_OK),
		"failed to delete driver");

	if (test_EnumPrinterDrivers_findone(tctx, b, server_name, environment, 1, driver, NULL)) {
		torture_fail(tctx, "deleted driver still enumerated");
	}

	torture_assert(tctx,
		test_DeletePrinterDriver_exp(tctx, b, server_name, driver, environment, WERR_UNKNOWN_PRINTER_DRIVER),
		"2nd delete failed");

	return true;
}

static bool test_DeletePrinterDriverEx(struct torture_context *tctx,
				       struct dcerpc_binding_handle *b,
				       const char *server_name,
				       const char *driver,
				       const char *environment,
				       uint32_t delete_flags,
				       uint32_t version)
{
	torture_assert(tctx,
		test_DeletePrinterDriverEx_exp(tctx, b, server_name, driver, "FOOBAR", delete_flags, version, WERR_INVALID_ENVIRONMENT),
		"failed to delete driver");

	torture_assert(tctx,
		test_DeletePrinterDriverEx_exp(tctx, b, server_name, driver, environment, delete_flags, version, WERR_OK),
		"failed to delete driver");

	if (test_EnumPrinterDrivers_findone(tctx, b, server_name, environment, 1, driver, NULL)) {
		torture_fail(tctx, "deleted driver still enumerated");
	}

	torture_assert(tctx,
		test_DeletePrinterDriverEx_exp(tctx, b, server_name, driver, environment, delete_flags, version, WERR_UNKNOWN_PRINTER_DRIVER),
		"2nd delete failed");

	return true;
}

static bool test_PrinterDriver_args(struct torture_context *tctx,
				    struct dcerpc_binding_handle *b,
				    const char *server_name,
				    uint32_t level,
				    struct spoolss_AddDriverInfo8 *r,
				    uint32_t add_flags,
				    uint32_t delete_flags,
				    uint32_t delete_version,
				    bool ex,
				    const char *remote_driver_dir)
{
	bool ret = true;

	switch (level) {
	case 1:
		ret = test_AddPrinterDriver_args_level_1(tctx, b, server_name, r, add_flags, ex, remote_driver_dir);
		break;
	case 2:
		ret = test_AddPrinterDriver_args_level_2(tctx, b, server_name, r, add_flags, ex, remote_driver_dir);
		break;
	case 3:
		ret = test_AddPrinterDriver_args_level_3(tctx, b, server_name, r, add_flags, ex, remote_driver_dir);
		break;
	case 4:
		ret = test_AddPrinterDriver_args_level_4(tctx, b, server_name, r, add_flags, ex, remote_driver_dir);
		break;
	case 6:
		ret = test_AddPrinterDriver_args_level_6(tctx, b, server_name, r, add_flags, ex, remote_driver_dir);
		break;
	case 8:
		ret = test_AddPrinterDriver_args_level_8(tctx, b, server_name, r, add_flags, ex, remote_driver_dir);
		break;
	default:
		return false;
	}

	if (ret == false) {
		return ret;
	}

	if (level == 1) {
		return ret;
	}

	/* spoolss_AddPrinterDriver does not deal with level 6 or 8 - gd */

	if (!ex && (level == 6 || level == 8)) {
		return ret;
	}

	{
		struct dcerpc_pipe *p2;
		struct policy_handle hive_handle;
		struct dcerpc_binding_handle *b2;

		torture_assert_ntstatus_ok(tctx,
			torture_rpc_connection(tctx, &p2, &ndr_table_winreg),
			"could not open winreg pipe");
		b2 = p2->binding_handle;

		torture_assert(tctx, test_winreg_OpenHKLM(tctx, b2, &hive_handle), "");

		ret = test_GetDriverInfo_winreg(tctx, b, NULL, NULL, r->driver_name, r->architecture, r->version, b2, &hive_handle, server_name);

		test_winreg_CloseKey(tctx, b2, &hive_handle);

		talloc_free(p2);
	}

	if (ex) {
		return test_DeletePrinterDriverEx(tctx, b, server_name, r->driver_name, r->architecture, delete_flags, r->version);
	} else {
		return test_DeletePrinterDriver(tctx, b, server_name, r->driver_name, r->architecture);
	}
}

static bool fillup_printserver_info(struct torture_context *tctx,
				    struct dcerpc_pipe *p,
				    struct torture_driver_context *d)
{
	struct policy_handle server_handle;
	struct dcerpc_binding_handle *b = p->binding_handle;
	const char *server_name_slash = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));

	torture_assert(tctx,
		test_OpenPrinter_server(tctx, p, &server_handle),
		"failed to open printserver");
	torture_assert(tctx,
		test_get_environment(tctx, b, &server_handle, &d->remote.environment),
		"failed to get environment");
	torture_assert(tctx,
		test_ClosePrinter(tctx, b, &server_handle),
		"failed to close printserver");

	torture_assert(tctx,
		test_GetPrinterDriverDirectory_getdir(tctx, b, server_name_slash,
			d->local.environment ? d->local.environment : d->remote.environment,
			&d->remote.driver_directory),
		"failed to get driver directory");

	return true;
}

static const char *driver_directory_dir(const char *driver_directory)
{
	char *p;

	p = strrchr(driver_directory, '\\');
	if (p) {
		return p+1;
	}

	return NULL;
}

static const char *driver_directory_share(struct torture_context *tctx,
					  const char *driver_directory)
{
	const char *p;
	char *tok;

	if (driver_directory[0] == '\\' && driver_directory[1] == '\\') {
		driver_directory += 2;
	}

	p = talloc_strdup(tctx, driver_directory);

	torture_assert(tctx,
		next_token_talloc(tctx, &p, &tok, "\\"),
		"cannot explode uri");
	torture_assert(tctx,
		next_token_talloc(tctx, &p, &tok, "\\"),
		"cannot explode uri");

	return tok;
}

static bool upload_printer_driver_file(struct torture_context *tctx,
				       struct smbcli_state *cli,
				       struct torture_driver_context *d,
				       const char *file_name)
{
	XFILE *f;
	int fnum;
	uint8_t *buf;
	int maxwrite = 64512;
	off_t nread = 0;
	size_t start = 0;
	const char *remote_dir = driver_directory_dir(d->remote.driver_directory);
	const char *local_name = talloc_asprintf(tctx, "%s/%s", d->local.driver_directory, file_name);
	const char *remote_name = talloc_asprintf(tctx, "%s\\%s", remote_dir, file_name);

	if (!file_name || strlen(file_name) == 0) {
		return true;
	}

	torture_comment(tctx, "Uploading %s to %s\n", local_name, remote_name);

	fnum = smbcli_open(cli->tree, remote_name, O_RDWR|O_CREAT|O_TRUNC, DENY_NONE);
	if (fnum == -1) {
		torture_fail(tctx, talloc_asprintf(tctx, "failed to open remote file: %s\n", remote_name));
	}

	f = x_fopen(local_name, O_RDONLY, 0);
	if (f == NULL) {
		torture_fail(tctx, talloc_asprintf(tctx, "failed to open local file: %s\n", local_name));
	}

	buf = talloc_array(tctx, uint8_t, maxwrite);
	if (!buf) {
		return false;
	}

	while (!x_feof(f)) {
		int n = maxwrite;
		int ret;

		if ((n = x_fread(buf, 1, n, f)) < 1) {
			if((n == 0) && x_feof(f))
				break; /* Empty local file. */

			torture_warning(tctx,
				"failed to read file: %s\n", strerror(errno));
			break;
		}

		ret = smbcli_write(cli->tree, fnum, 0, buf, nread + start, n);

		if (n != ret) {
			torture_warning(tctx,
				"failed to write file: %s\n", smbcli_errstr(cli->tree));
			break;
		}

		nread += n;
	}

	x_fclose(f);

	torture_assert_ntstatus_ok(tctx,
		smbcli_close(cli->tree, fnum),
		"failed to close file");

	return true;
}

static bool connect_printer_driver_share(struct torture_context *tctx,
					 const char *server_name,
					 const char *share_name,
					 struct smbcli_state **cli)
{
	struct smbcli_options smb_options;
	struct smbcli_session_options smb_session_options;

	torture_comment(tctx, "Connecting printer driver share '%s' on '%s'\n",
		share_name, server_name);

	lpcfg_smbcli_options(tctx->lp_ctx, &smb_options);
	lpcfg_smbcli_session_options(tctx->lp_ctx, &smb_session_options);

	torture_assert_ntstatus_ok(tctx,
		smbcli_full_connection(tctx, cli, server_name,
					lpcfg_smb_ports(tctx->lp_ctx),
					share_name, NULL,
					lpcfg_socket_options(tctx->lp_ctx),
					cmdline_credentials,
					lpcfg_resolve_context(tctx->lp_ctx),
					tctx->ev,
					&smb_options,
					&smb_session_options,
					lpcfg_gensec_settings(tctx, tctx->lp_ctx)),
		"failed to open driver share");

	return true;
}

static bool upload_printer_driver(struct torture_context *tctx,
				  const char *server_name,
				  struct torture_driver_context *d)
{
	struct smbcli_state *cli;
	const char *share_name = driver_directory_share(tctx, d->remote.driver_directory);
	int i;

	torture_assert(tctx,
		connect_printer_driver_share(tctx, server_name, share_name, &cli),
		"failed to connect to driver share");

	torture_comment(tctx, "Uploading printer driver files to \\\\%s\\%s\n",
		server_name, share_name);

	torture_assert(tctx,
		upload_printer_driver_file(tctx, cli, d, d->info8.driver_path),
		"failed to upload driver_path");
	torture_assert(tctx,
		upload_printer_driver_file(tctx, cli, d, d->info8.data_file),
		"failed to upload data_file");
	torture_assert(tctx,
		upload_printer_driver_file(tctx, cli, d, d->info8.config_file),
		"failed to upload config_file");
	torture_assert(tctx,
		upload_printer_driver_file(tctx, cli, d, d->info8.help_file),
		"failed to upload help_file");
	if (d->info8.dependent_files) {
		for (i=0; d->info8.dependent_files->string && d->info8.dependent_files->string[i] != NULL; i++) {
			torture_assert(tctx,
				upload_printer_driver_file(tctx, cli, d, d->info8.dependent_files->string[i]),
				"failed to upload dependent_files");
		}
	}

	talloc_free(cli);

	return true;
}

static bool check_printer_driver_file(struct torture_context *tctx,
				      struct smbcli_state *cli,
				      struct torture_driver_context *d,
				      const char *file_name)
{
	const char *remote_arch_dir = driver_directory_dir(d->remote.driver_directory);
	const char *remote_name = talloc_asprintf(tctx, "%s\\%d\\%s",
						  remote_arch_dir,
						  d->info8.version,
						  file_name);
	int fnum;

	torture_assert(tctx, (file_name && strlen(file_name) != 0), "invalid filename");

	torture_comment(tctx, "checking for driver file at %s\n", remote_name);

	fnum = smbcli_open(cli->tree, remote_name, O_RDONLY, DENY_NONE);
	if (fnum == -1) {
		return false;
	}

	torture_assert_ntstatus_ok(tctx,
		smbcli_close(cli->tree, fnum),
		"failed to close driver file");

	return true;
}

static bool check_printer_driver_files(struct torture_context *tctx,
				       const char *server_name,
				       struct torture_driver_context *d,
				       bool expect_exist)
{
	struct smbcli_state *cli;
	const char *share_name = driver_directory_share(tctx, d->remote.driver_directory);
	int i;

	torture_assert(tctx,
		connect_printer_driver_share(tctx, server_name, share_name, &cli),
		"failed to connect to driver share");

	torture_comment(tctx, "checking %sexistent driver files at \\\\%s\\%s\n",
			(expect_exist ? "": "non-"),
			server_name, share_name);

	if (d->info8.driver_path && d->info8.driver_path[0]) {
		torture_assert(tctx,
			check_printer_driver_file(tctx, cli, d, d->info8.driver_path) == expect_exist,
			"failed driver_path check");
	}
	if (d->info8.data_file && d->info8.data_file[0]) {
		torture_assert(tctx,
			check_printer_driver_file(tctx, cli, d, d->info8.data_file) == expect_exist,
			"failed data_file check");
	}
	if (d->info8.config_file && d->info8.config_file[0]) {
		torture_assert(tctx,
			check_printer_driver_file(tctx, cli, d, d->info8.config_file) == expect_exist,
			"failed config_file check");
	}
	if (d->info8.help_file && d->info8.help_file[0]) {
		torture_assert(tctx,
			check_printer_driver_file(tctx, cli, d, d->info8.help_file) == expect_exist,
			"failed help_file check");
	}
	if (d->info8.dependent_files) {
		for (i=0; d->info8.dependent_files->string && d->info8.dependent_files->string[i] != NULL; i++) {
			torture_assert(tctx,
				check_printer_driver_file(tctx, cli, d, d->info8.dependent_files->string[i]) == expect_exist,
				"failed dependent_files check");
		}
	}

	talloc_free(cli);

	return true;
}

static bool remove_printer_driver_file(struct torture_context *tctx,
				       struct smbcli_state *cli,
				       struct torture_driver_context *d,
				       const char *file_name)
{
	const char *remote_name;
	const char *remote_dir =  driver_directory_dir(d->remote.driver_directory);

	if (!file_name || strlen(file_name) == 0) {
		return true;
	}

	remote_name = talloc_asprintf(tctx, "%s\\%s", remote_dir, file_name);

	torture_comment(tctx, "Removing %s\n", remote_name);

	torture_assert_ntstatus_ok(tctx,
		smbcli_unlink(cli->tree, remote_name),
		"failed to unlink");

	return true;
}

static bool remove_printer_driver(struct torture_context *tctx,
				  const char *server_name,
				  struct torture_driver_context *d)
{
	struct smbcli_state *cli;
	const char *share_name = driver_directory_share(tctx, d->remote.driver_directory);
	int i;

	torture_assert(tctx,
		connect_printer_driver_share(tctx, server_name, share_name, &cli),
		"failed to connect to driver share");

	torture_comment(tctx, "Removing printer driver files from \\\\%s\\%s\n",
		server_name, share_name);

	torture_assert(tctx,
		remove_printer_driver_file(tctx, cli, d, d->info8.driver_path),
		"failed to remove driver_path");
	torture_assert(tctx,
		remove_printer_driver_file(tctx, cli, d, d->info8.data_file),
		"failed to remove data_file");
	if (!strequal(d->info8.config_file, d->info8.driver_path)) {
		torture_assert(tctx,
			remove_printer_driver_file(tctx, cli, d, d->info8.config_file),
			"failed to remove config_file");
	}
	torture_assert(tctx,
		remove_printer_driver_file(tctx, cli, d, d->info8.help_file),
		"failed to remove help_file");
	if (d->info8.dependent_files) {
		for (i=0; d->info8.dependent_files->string && d->info8.dependent_files->string[i] != NULL; i++) {
			if (strequal(d->info8.dependent_files->string[i], d->info8.driver_path) ||
			    strequal(d->info8.dependent_files->string[i], d->info8.data_file) ||
			    strequal(d->info8.dependent_files->string[i], d->info8.config_file) ||
			    strequal(d->info8.dependent_files->string[i], d->info8.help_file)) {
				continue;
			}
			torture_assert(tctx,
				remove_printer_driver_file(tctx, cli, d, d->info8.dependent_files->string[i]),
				"failed to remove dependent_files");
		}
	}

	talloc_free(cli);

	return true;

}

static bool test_add_driver_arg(struct torture_context *tctx,
				struct dcerpc_pipe *p,
				struct torture_driver_context *d)
{
	bool ret = true;
	struct dcerpc_binding_handle *b = p->binding_handle;
	const char *server_name_slash = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	uint32_t levels[] = { 1, 2, 3, 4, 6, 8 };
	int i;
	struct spoolss_AddDriverInfo8 info8;
	uint32_t add_flags = APD_COPY_NEW_FILES;
	uint32_t delete_flags = 0;

	ZERO_STRUCT(info8);

	torture_comment(tctx, "Testing PrinterDriver%s '%s' for environment '%s'\n",
		d->ex ? "Ex" : "", d->info8.driver_name, d->local.environment);

	torture_assert(tctx,
		fillup_printserver_info(tctx, p, d),
		"failed to fillup printserver info");

	if (!directory_exist(d->local.driver_directory)) {
		torture_skip(tctx, "Skipping Printer Driver test as no local driver is available");
	}

	torture_assert(tctx,
		upload_printer_driver(tctx, dcerpc_server_name(p), d),
		"failed to upload printer driver");

	info8 = d->info8;
	if (d->info8.dependent_files) {
		info8.dependent_files = talloc_zero(tctx, struct spoolss_StringArray);
		if (d->info8.dependent_files->string) {
			for (i=0; d->info8.dependent_files->string[i] != NULL; i++) {
			}
			info8.dependent_files->string = talloc_zero_array(info8.dependent_files, const char *, i+1);
			for (i=0; d->info8.dependent_files->string[i] != NULL; i++) {
				info8.dependent_files->string[i] = talloc_strdup(info8.dependent_files->string, d->info8.dependent_files->string[i]);
			}
		}
	}
	info8.architecture      = d->local.environment;

	for (i=0; i < ARRAY_SIZE(levels); i++) {

		if (torture_setting_bool(tctx, "samba3", false)) {
			switch (levels[i]) {
			case 2:
			case 4:
			case 8:
				torture_comment(tctx, "skipping level %d against samba\n", levels[i]);
				continue;
			default:
				break;
			}
		}
		if (torture_setting_bool(tctx, "w2k3", false)) {
			switch (levels[i]) {
			case 8:
				torture_comment(tctx, "skipping level %d against w2k3\n", levels[i]);
				continue;
			default:
				break;
			}
		}

		torture_comment(tctx,
			"Testing PrinterDriver%s '%s' add & delete level %d\n",
				d->ex ? "Ex" : "", info8.driver_name, levels[i]);

		ret &= test_PrinterDriver_args(tctx, b, server_name_slash, levels[i], &info8, add_flags, delete_flags, d->info8.version, d->ex, d->remote.driver_directory);
	}

	info8.driver_path	= talloc_asprintf(tctx, "%s\\%s", d->remote.driver_directory, d->info8.driver_path);
	info8.data_file		= talloc_asprintf(tctx, "%s\\%s", d->remote.driver_directory, d->info8.data_file);
	if (d->info8.config_file) {
		info8.config_file	= talloc_asprintf(tctx, "%s\\%s", d->remote.driver_directory, d->info8.config_file);
	}
	if (d->info8.help_file) {
		info8.help_file	= talloc_asprintf(tctx, "%s\\%s", d->remote.driver_directory, d->info8.help_file);
	}
	if (d->info8.dependent_files && d->info8.dependent_files->string) {
		for (i=0; d->info8.dependent_files->string[i] != NULL; i++) {
			info8.dependent_files->string[i] = talloc_asprintf(tctx, "%s\\%s", d->remote.driver_directory, d->info8.dependent_files->string[i]);
		}
	}

	for (i=0; i < ARRAY_SIZE(levels); i++) {

		if (torture_setting_bool(tctx, "samba3", false)) {
			switch (levels[i]) {
			case 2:
			case 4:
			case 8:
				torture_comment(tctx, "skipping level %d against samba\n", levels[i]);
				continue;
			default:
				break;
			}
		}
		if (torture_setting_bool(tctx, "w2k3", false)) {
			switch (levels[i]) {
			case 8:
				torture_comment(tctx, "skipping level %d against w2k3\n", levels[i]);
				continue;
			default:
				break;
			}
		}

		torture_comment(tctx,
			"Testing PrinterDriver%s '%s' add & delete level %d (full unc paths)\n",
				d->ex ? "Ex" : "", info8.driver_name, levels[i]);

		ret &= test_PrinterDriver_args(tctx, b, server_name_slash, levels[i], &info8, add_flags, delete_flags, d->info8.version, d->ex, d->remote.driver_directory);
	}

	torture_assert(tctx,
		remove_printer_driver(tctx, dcerpc_server_name(p), d),
		"failed to remove printer driver");

	torture_comment(tctx, "\n");

	return ret;
}

static bool test_add_driver_ex_64(struct torture_context *tctx,
				  struct dcerpc_pipe *p)
{
	struct torture_driver_context *d;

	d = talloc_zero(tctx, struct torture_driver_context);

	d->info8.version		= SPOOLSS_DRIVER_VERSION_200X;
	d->info8.driver_name		= TORTURE_DRIVER_EX;
	d->info8.architecture		= NULL;
	d->info8.driver_path		= talloc_strdup(d, "pscript5.dll");
	d->info8.data_file		= talloc_strdup(d, "cups6.ppd");
	d->info8.config_file		= talloc_strdup(d, "cupsui6.dll");
	d->local.environment		= talloc_strdup(d, "Windows x64");
	d->local.driver_directory	= talloc_strdup(d, "/usr/share/cups/drivers/x64");
	d->ex				= true;

	return test_add_driver_arg(tctx, p, d);
}

static bool test_add_driver_ex_32(struct torture_context *tctx,
				  struct dcerpc_pipe *p)
{
	struct torture_driver_context *d;

	d = talloc_zero(tctx, struct torture_driver_context);

	d->info8.version		= SPOOLSS_DRIVER_VERSION_200X;
	d->info8.driver_name		= TORTURE_DRIVER_EX;
	d->info8.architecture		= NULL;
	d->info8.driver_path		= talloc_strdup(d, "pscript5.dll");
	d->info8.data_file		= talloc_strdup(d, "cups6.ppd");
	d->info8.config_file		= talloc_strdup(d, "cupsui6.dll");
	d->local.environment		= talloc_strdup(d, "Windows NT x86");
	d->local.driver_directory	= talloc_strdup(d, "/usr/share/cups/drivers/i386");
	d->ex				= true;

	return test_add_driver_arg(tctx, p, d);
}

static bool test_add_driver_64(struct torture_context *tctx,
			       struct dcerpc_pipe *p)
{
	struct torture_driver_context *d;

	d = talloc_zero(tctx, struct torture_driver_context);

	d->info8.version		= SPOOLSS_DRIVER_VERSION_200X;
	d->info8.driver_name		= TORTURE_DRIVER;
	d->info8.architecture		= NULL;
	d->info8.driver_path		= talloc_strdup(d, "pscript5.dll");
	d->info8.data_file		= talloc_strdup(d, "cups6.ppd");
	d->info8.config_file		= talloc_strdup(d, "cupsui6.dll");
	d->local.environment		= talloc_strdup(d, "Windows x64");
	d->local.driver_directory	= talloc_strdup(d, "/usr/share/cups/drivers/x64");
	d->ex				= false;

	return test_add_driver_arg(tctx, p, d);
}

static bool test_add_driver_32(struct torture_context *tctx,
			       struct dcerpc_pipe *p)
{
	struct torture_driver_context *d;

	d = talloc_zero(tctx, struct torture_driver_context);

	d->info8.version		= SPOOLSS_DRIVER_VERSION_200X;
	d->info8.driver_name		= TORTURE_DRIVER;
	d->info8.architecture		= NULL;
	d->info8.driver_path		= talloc_strdup(d, "pscript5.dll");
	d->info8.data_file		= talloc_strdup(d, "cups6.ppd");
	d->info8.config_file		= talloc_strdup(d, "cupsui6.dll");
	d->local.environment		= talloc_strdup(d, "Windows NT x86");
	d->local.driver_directory	= talloc_strdup(d, "/usr/share/cups/drivers/i386");
	d->ex				= false;

	return test_add_driver_arg(tctx, p, d);
}

static bool test_add_driver_adobe(struct torture_context *tctx,
				  struct dcerpc_pipe *p)
{
	struct torture_driver_context *d;

	if (!torture_setting_bool(tctx, "samba3", false)) {
		torture_skip(tctx, "skipping adobe test which only works against samba3");
	}

	d = talloc_zero(tctx, struct torture_driver_context);

	d->info8.version		= SPOOLSS_DRIVER_VERSION_9X;
	d->info8.driver_name		= TORTURE_DRIVER_ADOBE;
	d->info8.architecture		= NULL;
	d->info8.driver_path		= talloc_strdup(d, "ADOBEPS4.DRV");
	d->info8.data_file		= talloc_strdup(d, "DEFPRTR2.PPD");
	d->info8.config_file		= talloc_strdup(d, "ADOBEPS4.DRV");
#if 0
	d->info8.help_file		= talloc_strdup(d, "ADOBEPS4.HLP");
	d->info8.monitor_name		= talloc_strdup(d, "PSMON.DLL");
#endif
	d->local.environment		= talloc_strdup(d, "Windows 4.0");
	d->local.driver_directory	= talloc_strdup(d, "/usr/share/cups/drivers/adobe/");
	d->ex				= false;

	return test_add_driver_arg(tctx, p, d);
}

static bool test_add_driver_adobe_cupsaddsmb(struct torture_context *tctx,
					     struct dcerpc_pipe *p)
{
	struct torture_driver_context *d;
	struct spoolss_StringArray *a;

	if (!torture_setting_bool(tctx, "samba3", false)) {
		torture_skip(tctx, "skipping cupsaddsmb test which only works against samba3");
	}

	d = talloc_zero(tctx, struct torture_driver_context);

	d->info8.version		= SPOOLSS_DRIVER_VERSION_9X;
	d->info8.driver_name		= TORTURE_DRIVER_ADOBE_CUPSADDSMB;
	d->info8.architecture		= NULL;
	d->info8.driver_path		= talloc_strdup(d, "ADOBEPS4.DRV");
	d->info8.data_file		= talloc_strdup(d, "DEFPRTR2.PPD");
	d->info8.config_file		= NULL;
	d->info8.help_file		= talloc_strdup(d, "ADOBEPS4.HLP");
	d->info8.monitor_name		= talloc_strdup(d, "PSMON.DLL");
	d->info8.default_datatype	= talloc_strdup(d, "RAW");

	a				= talloc_zero(d, struct spoolss_StringArray);
	a->string			= talloc_zero_array(a, const char *, 7);
	a->string[0]			= talloc_strdup(a->string, "ADOBEPS4.DRV");
	a->string[1]			= talloc_strdup(a->string, "DEFPRTR2.PPD");
	a->string[2]			= talloc_strdup(a->string, "ADOBEPS4.HLP");
	a->string[3]			= talloc_strdup(a->string, "PSMON.DLL");
	a->string[4]			= talloc_strdup(a->string, "ADFONTS.MFM");
	a->string[5]			= talloc_strdup(a->string, "ICONLIB.DLL");

	d->info8.dependent_files	= a;
	d->local.environment		= talloc_strdup(d, "Windows 4.0");
	d->local.driver_directory	= talloc_strdup(d, "/usr/share/cups/drivers/adobe/");
	d->ex				= false;

	return test_add_driver_arg(tctx, p, d);
}

static bool test_add_driver_timestamps(struct torture_context *tctx,
				       struct dcerpc_pipe *p)
{
	struct torture_driver_context *d;
	struct timeval t = timeval_current();

	d = talloc_zero(tctx, struct torture_driver_context);

	d->info8.version		= SPOOLSS_DRIVER_VERSION_200X;
	d->info8.driver_name		= TORTURE_DRIVER_TIMESTAMPS;
	d->info8.architecture		= NULL;
	d->info8.driver_path		= talloc_strdup(d, "pscript5.dll");
	d->info8.data_file		= talloc_strdup(d, "cups6.ppd");
	d->info8.config_file		= talloc_strdup(d, "cupsui6.dll");
	d->info8.driver_date		= timeval_to_nttime(&t);
	d->local.environment		= talloc_strdup(d, "Windows NT x86");
	d->local.driver_directory	= talloc_strdup(d, "/usr/share/cups/drivers/i386");
	d->ex				= true;

	torture_assert(tctx,
		test_add_driver_arg(tctx, p, d),
		"");

	unix_to_nt_time(&d->info8.driver_date, 1);

	torture_assert(tctx,
		test_add_driver_arg(tctx, p, d),
		"");

	return true;
}

static bool test_multiple_drivers(struct torture_context *tctx,
				  struct dcerpc_pipe *p)
{
	struct torture_driver_context *d;
	struct dcerpc_binding_handle *b = p->binding_handle;
	const char *server_name_slash = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	int i;
	struct spoolss_AddDriverInfo8 info8;
	uint32_t add_flags = APD_COPY_NEW_FILES;
	uint32_t delete_flags = 0;

	d = talloc_zero(tctx, struct torture_driver_context);

	d->info8.version		= SPOOLSS_DRIVER_VERSION_200X;
	d->info8.driver_path		= talloc_strdup(d, "pscript5.dll");
	d->info8.data_file		= talloc_strdup(d, "cups6.ppd");
	d->info8.config_file		= talloc_strdup(d, "cupsui6.dll");
	d->local.environment		= talloc_strdup(d, "Windows NT x86");
	d->local.driver_directory	= talloc_strdup(d, "/usr/share/cups/drivers/i386");
	d->ex				= true;

	torture_assert(tctx,
		fillup_printserver_info(tctx, p, d),
		"failed to fillup printserver info");

	if (!directory_exist(d->local.driver_directory)) {
		torture_skip(tctx, "Skipping Printer Driver test as no local driver is available");
	}

	torture_assert(tctx,
		upload_printer_driver(tctx, dcerpc_server_name(p), d),
		"failed to upload printer driver");

	info8 = d->info8;
	info8.architecture      = d->local.environment;

	for (i=0; i < 3; i++) {
		info8.driver_name		= talloc_asprintf(d, "torture_test_driver_%d", i);

		torture_assert(tctx,
			test_AddPrinterDriver_args_level_3(tctx, b, server_name_slash, &info8, add_flags, true, NULL),
			"failed to add driver");
	}

	torture_assert(tctx,
		test_DeletePrinterDriverEx(tctx, b, server_name_slash, "torture_test_driver_0", info8.architecture, delete_flags, info8.version),
		"failed to delete driver");

	torture_assert(tctx,
		test_EnumPrinterDrivers_findone(tctx, b, server_name_slash, info8.architecture, 3, "torture_test_driver_1", NULL),
		"torture_test_driver_1 no longer on the server");

	torture_assert(tctx,
		test_EnumPrinterDrivers_findone(tctx, b, server_name_slash, info8.architecture, 3, "torture_test_driver_2", NULL),
		"torture_test_driver_2 no longer on the server");

	torture_assert(tctx,
		test_DeletePrinterDriverEx(tctx, b, server_name_slash, "torture_test_driver_1", info8.architecture, delete_flags, info8.version),
		"failed to delete driver");

	torture_assert(tctx,
		test_EnumPrinterDrivers_findone(tctx, b, server_name_slash, info8.architecture, 3, "torture_test_driver_2", NULL),
		"torture_test_driver_2 no longer on the server");

	torture_assert(tctx,
		test_DeletePrinterDriverEx(tctx, b, server_name_slash, "torture_test_driver_2", info8.architecture, delete_flags, info8.version),
		"failed to delete driver");

	torture_assert(tctx,
		remove_printer_driver(tctx, dcerpc_server_name(p), d),
		"failed to remove printer driver");

	return true;
}

static bool test_del_driver_all_files(struct torture_context *tctx,
				      struct dcerpc_pipe *p)
{
	struct torture_driver_context *d;
	struct spoolss_StringArray *a;
	uint32_t add_flags = APD_COPY_NEW_FILES;
	uint32_t delete_flags = DPD_DELETE_ALL_FILES;
	struct dcerpc_binding_handle *b = p->binding_handle;
	const char *server_name_slash = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));

	d = talloc_zero(tctx, struct torture_driver_context);

	d->ex				= true;
	d->info8.version		= SPOOLSS_DRIVER_VERSION_200X;
	d->info8.driver_name		= TORTURE_DRIVER_DELETER;
	d->info8.architecture		= NULL;
	d->info8.driver_path		= talloc_strdup(d, "pscript5.dll");
	d->info8.data_file		= talloc_strdup(d, "cups6.ppd");
	d->info8.config_file		= talloc_strdup(d, "cupsui6.dll");
	d->info8.help_file		= talloc_strdup(d, "pscript.hlp");
	d->local.environment		= talloc_strdup(d, SPOOLSS_ARCHITECTURE_x64);
	d->local.driver_directory	= talloc_strdup(d, "/usr/share/cups/drivers/x64");

	a				= talloc_zero(d, struct spoolss_StringArray);
	a->string			= talloc_zero_array(a, const char *, 3);
	a->string[0]			= talloc_strdup(a->string, "cups6.inf");
	a->string[1]			= talloc_strdup(a->string, "cups6.ini");

	d->info8.dependent_files	= a;
	d->info8.architecture		= d->local.environment;

	torture_assert(tctx,
		fillup_printserver_info(tctx, p, d),
		"failed to fillup printserver info");

	if (!directory_exist(d->local.driver_directory)) {
		torture_skip(tctx, "Skipping Printer Driver test as no local driver is available");
	}

	torture_assert(tctx,
		upload_printer_driver(tctx, dcerpc_server_name(p), d),
		"failed to upload printer driver");

	torture_assert(tctx,
		test_AddPrinterDriver_args_level_3(tctx, b, server_name_slash, &d->info8, add_flags, true, NULL),
		"failed to add driver");

	torture_assert(tctx,
		test_DeletePrinterDriverEx(tctx, b, server_name_slash,
					   d->info8.driver_name,
					   d->local.environment,
					   delete_flags,
					   d->info8.version),
		"failed to delete driver");

	torture_assert(tctx,
		check_printer_driver_files(tctx, dcerpc_server_name(p), d, false),
		"printer driver file check failed");

	talloc_free(d);
	return true;
}

static bool test_del_driver_unused_files(struct torture_context *tctx,
					 struct dcerpc_pipe *p)
{
	struct torture_driver_context *d1;
	struct torture_driver_context *d2;
	uint32_t add_flags = APD_COPY_NEW_FILES;
	struct dcerpc_binding_handle *b = p->binding_handle;
	const char *server_name_slash = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));

	d1 = talloc_zero(tctx, struct torture_driver_context);
	d1->ex				= true;
	d1->info8.version		= SPOOLSS_DRIVER_VERSION_200X;
	d1->info8.driver_name		= TORTURE_DRIVER_DELETER;
	d1->info8.architecture		= NULL;
	d1->info8.driver_path		= talloc_strdup(d1, "pscript5.dll");
	d1->info8.data_file		= talloc_strdup(d1, "cups6.ppd");
	d1->info8.config_file		= talloc_strdup(d1, "cupsui6.dll");
	d1->info8.help_file		= talloc_strdup(d1, "pscript.hlp");
	d1->local.environment		= talloc_strdup(d1, SPOOLSS_ARCHITECTURE_x64);
	d1->local.driver_directory	= talloc_strdup(d1, "/usr/share/cups/drivers/x64");
	d1->info8.architecture		= d1->local.environment;

	d2 = talloc_zero(tctx, struct torture_driver_context);
	d2->ex				= true;
	d2->info8.version		= SPOOLSS_DRIVER_VERSION_200X;
	d2->info8.driver_name		= TORTURE_DRIVER_DELETERIN;
	d2->info8.architecture		= NULL;
	d2->info8.driver_path		= talloc_strdup(d2, "pscript5.dll");	/* overlapping */
	d2->info8.data_file		= talloc_strdup(d2, "cupsps6.dll");
	d2->info8.config_file		= talloc_strdup(d2, "cups6.ini");
	d2->info8.help_file		= talloc_strdup(d2, "pscript.hlp");	/* overlapping */
	d2->local.environment		= talloc_strdup(d2, SPOOLSS_ARCHITECTURE_x64);
	d2->local.driver_directory	= talloc_strdup(d2, "/usr/share/cups/drivers/x64");
	d2->info8.architecture		= d2->local.environment;

	torture_assert(tctx,
		fillup_printserver_info(tctx, p, d1),
		"failed to fillup printserver info");
	torture_assert(tctx,
		fillup_printserver_info(tctx, p, d2),
		"failed to fillup printserver info");

	if (!directory_exist(d1->local.driver_directory)) {
		torture_skip(tctx, "Skipping Printer Driver test as no local driver is available");
	}

	torture_assert(tctx,
		upload_printer_driver(tctx, dcerpc_server_name(p), d1),
		"failed to upload printer driver");
	torture_assert(tctx,
		test_AddPrinterDriver_args_level_3(tctx, b, server_name_slash, &d1->info8, add_flags, true, NULL),
		"failed to add driver");

	torture_assert(tctx,
		upload_printer_driver(tctx, dcerpc_server_name(p), d2),
		"failed to upload printer driver");
	torture_assert(tctx,
		test_AddPrinterDriver_args_level_3(tctx, b, server_name_slash, &d2->info8, add_flags, true, NULL),
		"failed to add driver");

	/* some files are in use by a separate driver, should fail */
	torture_assert(tctx,
		test_DeletePrinterDriverEx_exp(tctx, b, server_name_slash,
					       d1->info8.driver_name,
					       d1->local.environment,
					       DPD_DELETE_ALL_FILES,
					       d1->info8.version,
					       WERR_PRINTER_DRIVER_IN_USE),
		"invalid delete driver response");

	/* should only delete files not in use by other driver */
	torture_assert(tctx,
		test_DeletePrinterDriverEx_exp(tctx, b, server_name_slash,
					       d1->info8.driver_name,
					       d1->local.environment,
					       DPD_DELETE_UNUSED_FILES,
					       d1->info8.version,
					       WERR_OK),
		"failed to delete driver (unused files)");

	/* check non-overlapping were deleted */
	d1->info8.driver_path = NULL;
	d1->info8.help_file = NULL;
	torture_assert(tctx,
		check_printer_driver_files(tctx, dcerpc_server_name(p), d1, false),
		"printer driver file check failed");
	/* d2 files should be uneffected */
	torture_assert(tctx,
		check_printer_driver_files(tctx, dcerpc_server_name(p), d2, true),
		"printer driver file check failed");

	torture_assert(tctx,
		test_DeletePrinterDriverEx_exp(tctx, b, server_name_slash,
					       d2->info8.driver_name,
					       d2->local.environment,
					       DPD_DELETE_ALL_FILES,
					       d2->info8.version,
					       WERR_OK),
		"failed to delete driver");

	torture_assert(tctx,
		check_printer_driver_files(tctx, dcerpc_server_name(p), d2, false),
		"printer driver file check failed");

	talloc_free(d1);
	talloc_free(d2);
	return true;
}

struct torture_suite *torture_rpc_spoolss_driver(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "spoolss.driver");

	struct torture_rpc_tcase *tcase = torture_suite_add_rpc_iface_tcase(suite,
							"driver", &ndr_table_spoolss);
	torture_rpc_tcase_add_test(tcase, "add_driver_64", test_add_driver_64);
	torture_rpc_tcase_add_test(tcase, "add_driver_ex_64", test_add_driver_ex_64);

	torture_rpc_tcase_add_test(tcase, "add_driver_32", test_add_driver_32);
	torture_rpc_tcase_add_test(tcase, "add_driver_ex_32", test_add_driver_ex_32);

	torture_rpc_tcase_add_test(tcase, "add_driver_adobe", test_add_driver_adobe);

	torture_rpc_tcase_add_test(tcase, "add_driver_adobe_cupsaddsmb", test_add_driver_adobe_cupsaddsmb);

	torture_rpc_tcase_add_test(tcase, "add_driver_timestamps", test_add_driver_timestamps);

	torture_rpc_tcase_add_test(tcase, "multiple_drivers", test_multiple_drivers);

	torture_rpc_tcase_add_test(tcase, "del_driver_all_files", test_del_driver_all_files);

	torture_rpc_tcase_add_test(tcase, "del_driver_unused_files", test_del_driver_unused_files);

	return suite;
}
