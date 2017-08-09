/*
   Unix SMB/CIFS implementation.
   test suite for eventlog rpc operations

   Copyright (C) Tim Potter 2003,2005
   Copyright (C) Jelmer Vernooij 2004
   Copyright (C) Guenther Deschner 2009

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
#include "librpc/gen_ndr/ndr_eventlog.h"
#include "librpc/gen_ndr/ndr_eventlog_c.h"
#include "torture/rpc/torture_rpc.h"
#include "param/param.h"

#define TEST_BACKUP_NAME "samrtorturetest"

static void init_lsa_String(struct lsa_String *name, const char *s)
{
	name->string = s;
	name->length = 2*strlen_m(s);
	name->size = name->length;
}

static bool get_policy_handle(struct torture_context *tctx,
			      struct dcerpc_binding_handle *b,
			      struct policy_handle *handle)
{
	struct eventlog_OpenEventLogW r;
	struct eventlog_OpenUnknown0 unknown0;
	struct lsa_String logname, servername;

	unknown0.unknown0 = 0x005c;
	unknown0.unknown1 = 0x0001;

	r.in.unknown0 = &unknown0;
	init_lsa_String(&logname, "dns server");
	init_lsa_String(&servername, NULL);
	r.in.logname = &logname;
	r.in.servername = &servername;
	r.in.major_version = 0x00000001;
	r.in.minor_version = 0x00000001;
	r.out.handle = handle;

	torture_assert_ntstatus_ok(tctx,
			dcerpc_eventlog_OpenEventLogW_r(b, tctx, &r),
			"OpenEventLog failed");

	torture_assert_ntstatus_ok(tctx, r.out.result, "OpenEventLog failed");

	return true;
}



static bool test_GetNumRecords(struct torture_context *tctx, struct dcerpc_pipe *p)
{
	struct eventlog_GetNumRecords r;
	struct eventlog_CloseEventLog cr;
	struct policy_handle handle;
	uint32_t number = 0;
	struct dcerpc_binding_handle *b = p->binding_handle;

	if (!get_policy_handle(tctx, b, &handle))
		return false;

	ZERO_STRUCT(r);
	r.in.handle = &handle;
	r.out.number = &number;

	torture_assert_ntstatus_ok(tctx,
			dcerpc_eventlog_GetNumRecords_r(b, tctx, &r),
			"GetNumRecords failed");
	torture_assert_ntstatus_ok(tctx, r.out.result,
			"GetNumRecords failed");
	torture_comment(tctx, "%d records\n", *r.out.number);

	cr.in.handle = cr.out.handle = &handle;

	torture_assert_ntstatus_ok(tctx,
			dcerpc_eventlog_CloseEventLog_r(b, tctx, &cr),
			"CloseEventLog failed");
	torture_assert_ntstatus_ok(tctx, cr.out.result,
			"CloseEventLog failed");
	return true;
}

static bool test_ReadEventLog(struct torture_context *tctx,
			      struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct eventlog_ReadEventLogW r;
	struct eventlog_CloseEventLog cr;
	struct policy_handle handle;
	struct dcerpc_binding_handle *b = p->binding_handle;

	uint32_t sent_size = 0;
	uint32_t real_size = 0;

	if (!get_policy_handle(tctx, b, &handle))
		return false;

	ZERO_STRUCT(r);
	r.in.offset = 0;
	r.in.handle = &handle;
	r.in.flags = 0;
	r.out.data = NULL;
	r.out.sent_size = &sent_size;
	r.out.real_size = &real_size;

	torture_assert_ntstatus_ok(tctx, dcerpc_eventlog_ReadEventLogW_r(b, tctx, &r),
		"ReadEventLog failed");

	torture_assert_ntstatus_equal(tctx, r.out.result, NT_STATUS_INVALID_PARAMETER,
			"ReadEventLog failed");

	while (1) {
		DATA_BLOB blob;
		struct EVENTLOGRECORD rec;
		enum ndr_err_code ndr_err;
		uint32_t size = 0;
		uint32_t pos = 0;

		/* Read first for number of bytes in record */

		r.in.number_of_bytes = 0;
		r.in.flags = EVENTLOG_BACKWARDS_READ|EVENTLOG_SEQUENTIAL_READ;
		r.out.data = NULL;
		r.out.sent_size = &sent_size;
		r.out.real_size = &real_size;

		torture_assert_ntstatus_ok(tctx, dcerpc_eventlog_ReadEventLogW_r(b, tctx, &r),
			"ReadEventLogW failed");

		if (NT_STATUS_EQUAL(r.out.result, NT_STATUS_END_OF_FILE)) {
			/* FIXME: still need to decode then */
			break;
		}

		torture_assert_ntstatus_equal(tctx, r.out.result, NT_STATUS_BUFFER_TOO_SMALL,
			"ReadEventLog failed");

		/* Now read the actual record */

		r.in.number_of_bytes = *r.out.real_size;
		r.out.data = talloc_array(tctx, uint8_t, r.in.number_of_bytes);

		torture_assert_ntstatus_ok(tctx, dcerpc_eventlog_ReadEventLogW_r(b, tctx, &r),
			"ReadEventLogW failed");

		torture_assert_ntstatus_ok(tctx, r.out.result, "ReadEventLog failed");

		/* Decode a user-marshalled record */
		size = IVAL(r.out.data, pos);

		while (size > 0) {

			blob = data_blob_const(r.out.data + pos, size);
			dump_data(0, blob.data, blob.length);

			ndr_err = ndr_pull_struct_blob_all(&blob, tctx, &rec,
				(ndr_pull_flags_fn_t)ndr_pull_EVENTLOGRECORD);
			if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
				status = ndr_map_error2ntstatus(ndr_err);
				torture_assert_ntstatus_ok(tctx, status,
					"ReadEventLog failed parsing event log record");
			}

			NDR_PRINT_DEBUG(EVENTLOGRECORD, &rec);

			pos += size;

			if (pos + 4 > *r.out.sent_size) {
				break;
			}

			size = IVAL(r.out.data, pos);
		}

		torture_assert_ntstatus_ok(tctx, r.out.result,
				"ReadEventLog failed parsing event log record");

		r.in.offset++;
	}

	cr.in.handle = cr.out.handle = &handle;

	torture_assert_ntstatus_ok(tctx,
			dcerpc_eventlog_CloseEventLog_r(b, tctx, &cr),
			"CloseEventLog failed");
	torture_assert_ntstatus_ok(tctx, cr.out.result,
			"CloseEventLog failed");

	return true;
}

static bool test_ReportEventLog(struct torture_context *tctx,
				struct dcerpc_pipe *p)
{
	struct eventlog_ReportEventW r;
	struct eventlog_CloseEventLog cr;
	struct policy_handle handle;
	struct dcerpc_binding_handle *b = p->binding_handle;

	uint32_t record_number = 0;
	time_t time_written = 0;
	struct lsa_String servername, *strings;

	if (!get_policy_handle(tctx, b, &handle))
		return false;

	init_lsa_String(&servername, NULL);

	strings = talloc_array(tctx, struct lsa_String, 1);
	init_lsa_String(&strings[0], "Currently tortured by samba 4");

	ZERO_STRUCT(r);

	r.in.handle = &handle;
	r.in.timestamp = time(NULL);
	r.in.event_type = EVENTLOG_INFORMATION_TYPE;
	r.in.event_category = 0;
	r.in.event_id = 0;
	r.in.num_of_strings = 1;
	r.in.data_size = 0;
	r.in.servername = &servername;
	r.in.user_sid = NULL;
	r.in.strings = &strings;
	r.in.data = NULL;
	r.in.flags = 0;
	r.in.record_number = r.out.record_number = &record_number;
	r.in.time_written = r.out.time_written = &time_written;

	torture_assert_ntstatus_ok(tctx,
			dcerpc_eventlog_ReportEventW_r(b, tctx, &r),
			"ReportEventW failed");

	torture_assert_ntstatus_ok(tctx, r.out.result, "ReportEventW failed");

	cr.in.handle = cr.out.handle = &handle;

	torture_assert_ntstatus_ok(tctx,
			dcerpc_eventlog_CloseEventLog_r(b, tctx, &cr),
			"CloseEventLog failed");
	torture_assert_ntstatus_ok(tctx, cr.out.result,
			"CloseEventLog failed");

	return true;
}

static bool test_FlushEventLog(struct torture_context *tctx,
			       struct dcerpc_pipe *p)
{
	struct eventlog_FlushEventLog r;
	struct eventlog_CloseEventLog cr;
	struct policy_handle handle;
	struct dcerpc_binding_handle *b = p->binding_handle;

	if (!get_policy_handle(tctx, b, &handle))
		return false;

	r.in.handle = &handle;

	/* Huh?  Does this RPC always return access denied? */
	torture_assert_ntstatus_ok(tctx,
			dcerpc_eventlog_FlushEventLog_r(b, tctx, &r),
			"FlushEventLog failed");

	torture_assert_ntstatus_equal(tctx,
			r.out.result,
			NT_STATUS_ACCESS_DENIED,
			"FlushEventLog failed");

	cr.in.handle = cr.out.handle = &handle;

	torture_assert_ntstatus_ok(tctx,
			dcerpc_eventlog_CloseEventLog_r(b, tctx, &cr),
			"CloseEventLog failed");
	torture_assert_ntstatus_ok(tctx, cr.out.result,
			"CloseEventLog failed");

	return true;
}

static bool test_ClearEventLog(struct torture_context *tctx,
			       struct dcerpc_pipe *p)
{
	struct eventlog_ClearEventLogW r;
	struct eventlog_CloseEventLog cr;
	struct policy_handle handle;
	struct dcerpc_binding_handle *b = p->binding_handle;

	if (!get_policy_handle(tctx, b, &handle))
		return false;

	r.in.handle = &handle;
	r.in.backupfile = NULL;

	torture_assert_ntstatus_ok(tctx,
			dcerpc_eventlog_ClearEventLogW_r(b, tctx, &r),
			"ClearEventLog failed");
	torture_assert_ntstatus_ok(tctx, r.out.result,
			"ClearEventLog failed");

	cr.in.handle = cr.out.handle = &handle;

	torture_assert_ntstatus_ok(tctx,
			dcerpc_eventlog_CloseEventLog_r(b, tctx, &cr),
			"CloseEventLog failed");
	torture_assert_ntstatus_ok(tctx, cr.out.result,
			"CloseEventLog failed");

	return true;
}

static bool test_GetLogInformation(struct torture_context *tctx,
				   struct dcerpc_pipe *p)
{
	struct eventlog_GetLogInformation r;
	struct eventlog_CloseEventLog cr;
	struct policy_handle handle;
	uint32_t bytes_needed = 0;
	struct dcerpc_binding_handle *b = p->binding_handle;

	if (!get_policy_handle(tctx, b, &handle))
		return false;

	r.in.handle = &handle;
	r.in.level = 1;
	r.in.buf_size = 0;
	r.out.buffer = NULL;
	r.out.bytes_needed = &bytes_needed;

	torture_assert_ntstatus_ok(tctx, dcerpc_eventlog_GetLogInformation_r(b, tctx, &r),
				   "GetLogInformation failed");

	torture_assert_ntstatus_equal(tctx, r.out.result, NT_STATUS_INVALID_LEVEL,
				      "GetLogInformation failed");

	r.in.level = 0;

	torture_assert_ntstatus_ok(tctx, dcerpc_eventlog_GetLogInformation_r(b, tctx, &r),
				   "GetLogInformation failed");

	torture_assert_ntstatus_equal(tctx, r.out.result, NT_STATUS_BUFFER_TOO_SMALL,
				      "GetLogInformation failed");

	r.in.buf_size = bytes_needed;
	r.out.buffer = talloc_array(tctx, uint8_t, bytes_needed);

	torture_assert_ntstatus_ok(tctx, dcerpc_eventlog_GetLogInformation_r(b, tctx, &r),
				   "GetLogInformation failed");

	torture_assert_ntstatus_ok(tctx, r.out.result, "GetLogInformation failed");

	cr.in.handle = cr.out.handle = &handle;

	torture_assert_ntstatus_ok(tctx,
			dcerpc_eventlog_CloseEventLog_r(b, tctx, &cr),
			"CloseEventLog failed");
	torture_assert_ntstatus_ok(tctx, cr.out.result,
			"CloseEventLog failed");

	return true;
}


static bool test_OpenEventLog(struct torture_context *tctx,
			      struct dcerpc_pipe *p)
{
	struct policy_handle handle;
	struct eventlog_CloseEventLog cr;
	struct dcerpc_binding_handle *b = p->binding_handle;

	if (!get_policy_handle(tctx, b, &handle))
		return false;

	cr.in.handle = cr.out.handle = &handle;

	torture_assert_ntstatus_ok(tctx,
			dcerpc_eventlog_CloseEventLog_r(b, tctx, &cr),
			"CloseEventLog failed");
	torture_assert_ntstatus_ok(tctx, cr.out.result,
			"CloseEventLog failed");

	return true;
}

static bool test_BackupLog(struct torture_context *tctx,
			   struct dcerpc_pipe *p)
{
	struct policy_handle handle, backup_handle;
	struct eventlog_BackupEventLogW r;
	struct eventlog_OpenBackupEventLogW br;
	struct eventlog_CloseEventLog cr;
	const char *tmp;
	struct lsa_String backup_filename;
	struct eventlog_OpenUnknown0 unknown0;
	struct dcerpc_binding_handle *b = p->binding_handle;

	if (torture_setting_bool(tctx, "samba3", false)) {
		torture_skip(tctx, "skipping BackupLog test against samba");
	}

	if (!get_policy_handle(tctx, b, &handle))
		return false;

	tmp = talloc_asprintf(tctx, "C:\\%s", TEST_BACKUP_NAME);
	init_lsa_String(&backup_filename, tmp);

	r.in.handle = &handle;
	r.in.backup_filename = &backup_filename;

	torture_assert_ntstatus_ok(tctx, dcerpc_eventlog_BackupEventLogW_r(b, tctx, &r),
		"BackupEventLogW failed");
	torture_assert_ntstatus_equal(tctx, r.out.result,
		NT_STATUS_OBJECT_PATH_SYNTAX_BAD, "BackupEventLogW failed");

	tmp = talloc_asprintf(tctx, "\\??\\C:\\%s", TEST_BACKUP_NAME);
	init_lsa_String(&backup_filename, tmp);

	r.in.handle = &handle;
	r.in.backup_filename = &backup_filename;

	torture_assert_ntstatus_ok(tctx, dcerpc_eventlog_BackupEventLogW_r(b, tctx, &r),
		"BackupEventLogW failed");
	torture_assert_ntstatus_ok(tctx, r.out.result, "BackupEventLogW failed");

	torture_assert_ntstatus_ok(tctx, dcerpc_eventlog_BackupEventLogW_r(b, tctx, &r),
		"BackupEventLogW failed");
	torture_assert_ntstatus_equal(tctx, r.out.result,
		NT_STATUS_OBJECT_NAME_COLLISION, "BackupEventLogW failed");

	cr.in.handle = cr.out.handle = &handle;

	torture_assert_ntstatus_ok(tctx,
			dcerpc_eventlog_CloseEventLog_r(b, tctx, &cr),
			"BackupLog failed");
	torture_assert_ntstatus_ok(tctx, cr.out.result,
			"BackupLog failed");

	unknown0.unknown0 = 0x005c;
	unknown0.unknown1 = 0x0001;

	br.in.unknown0 = &unknown0;
	br.in.backup_logname = &backup_filename;
	br.in.major_version = 1;
	br.in.minor_version = 1;
	br.out.handle = &backup_handle;

	torture_assert_ntstatus_ok(tctx, dcerpc_eventlog_OpenBackupEventLogW_r(b, tctx, &br),
		"OpenBackupEventLogW failed");

	torture_assert_ntstatus_ok(tctx, br.out.result, "OpenBackupEventLogW failed");

	cr.in.handle = cr.out.handle = &backup_handle;

	torture_assert_ntstatus_ok(tctx,
			dcerpc_eventlog_CloseEventLog_r(b, tctx, &cr),
			"CloseEventLog failed");
	torture_assert_ntstatus_ok(tctx, cr.out.result,
			"CloseEventLog failed");

	return true;
}

struct torture_suite *torture_rpc_eventlog(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite;
	struct torture_rpc_tcase *tcase;
	struct torture_test *test;

	suite = torture_suite_create(mem_ctx, "eventlog");
	tcase = torture_suite_add_rpc_iface_tcase(suite, "eventlog",
						  &ndr_table_eventlog);

	torture_rpc_tcase_add_test(tcase, "OpenEventLog", test_OpenEventLog);
	test = torture_rpc_tcase_add_test(tcase, "ClearEventLog",
					  test_ClearEventLog);
	test->dangerous = true;
	torture_rpc_tcase_add_test(tcase, "GetNumRecords", test_GetNumRecords);
	torture_rpc_tcase_add_test(tcase, "ReadEventLog", test_ReadEventLog);
	torture_rpc_tcase_add_test(tcase, "ReportEventLog", test_ReportEventLog);
	torture_rpc_tcase_add_test(tcase, "FlushEventLog", test_FlushEventLog);
	torture_rpc_tcase_add_test(tcase, "GetLogIntormation", test_GetLogInformation);
	torture_rpc_tcase_add_test(tcase, "BackupLog", test_BackupLog);

	return suite;
}
