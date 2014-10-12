/*
   Unix SMB/CIFS implementation.
   RPC pipe client

   Copyright (C) Günther Deschner 2009

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
#include "rpcclient.h"
#include "../librpc/gen_ndr/ndr_eventlog.h"
#include "../librpc/gen_ndr/ndr_eventlog_c.h"
#include "rpc_client/init_lsa.h"

static NTSTATUS get_eventlog_handle(struct rpc_pipe_client *cli,
				    TALLOC_CTX *mem_ctx,
				    const char *log,
				    struct policy_handle *handle)
{
	NTSTATUS status, result;
	struct eventlog_OpenUnknown0 unknown0;
	struct lsa_String logname, servername;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	unknown0.unknown0 = 0x005c;
	unknown0.unknown1 = 0x0001;

	init_lsa_String(&logname, log);
	init_lsa_String(&servername, NULL);

	status = dcerpc_eventlog_OpenEventLogW(b, mem_ctx,
					       &unknown0,
					       &logname,
					       &servername,
					       0x00000001, /* major */
					       0x00000001, /* minor */
					       handle,
					       &result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	return result;
}

static NTSTATUS cmd_eventlog_readlog(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx,
				     int argc,
				     const char **argv)
{
	NTSTATUS status = NT_STATUS_OK;
	NTSTATUS result = NT_STATUS_OK;
	struct policy_handle handle;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	uint32_t flags = EVENTLOG_BACKWARDS_READ |
			 EVENTLOG_SEQUENTIAL_READ;
	uint32_t offset = 0;
	uint32_t number_of_bytes = 0;
	uint8_t *data = NULL;
	uint32_t sent_size = 0;
	uint32_t real_size = 0;

	if (argc < 2 || argc > 4) {
		printf("Usage: %s logname [offset] [number_of_bytes]\n", argv[0]);
		return NT_STATUS_OK;
	}

	if (argc >= 3) {
		offset = atoi(argv[2]);
	}

	if (argc >= 4) {
		number_of_bytes = atoi(argv[3]);
		data = talloc_array(mem_ctx, uint8_t, number_of_bytes);
		if (!data) {
			goto done;
		}
	}

	status = get_eventlog_handle(cli, mem_ctx, argv[1], &handle);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	do {

		enum ndr_err_code ndr_err;
		DATA_BLOB blob;
		struct EVENTLOGRECORD r;
		uint32_t size = 0;
		uint32_t pos = 0;

		status = dcerpc_eventlog_ReadEventLogW(b, mem_ctx,
						       &handle,
						       flags,
						       offset,
						       number_of_bytes,
						       data,
						       &sent_size,
						       &real_size,
						       &result);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
		if (NT_STATUS_EQUAL(result, NT_STATUS_BUFFER_TOO_SMALL) &&
		    real_size > 0 ) {
			number_of_bytes = real_size;
			data = talloc_array(mem_ctx, uint8_t, real_size);
			if (!data) {
				goto done;
			}
			status = dcerpc_eventlog_ReadEventLogW(b, mem_ctx,
							       &handle,
							       flags,
							       offset,
							       number_of_bytes,
							       data,
							       &sent_size,
							       &real_size,
							       &result);
			if (!NT_STATUS_IS_OK(status)) {
				return status;
			}
		}

		if (!NT_STATUS_EQUAL(result, NT_STATUS_END_OF_FILE) &&
		    !NT_STATUS_IS_OK(result)) {
			goto done;
		}

		number_of_bytes = 0;

		size = IVAL(data, pos);

		while (size > 0) {

			blob = data_blob_const(data + pos, size);
			/* dump_data(0, blob.data, blob.length); */
			ndr_err = ndr_pull_struct_blob_all(&blob, mem_ctx, &r,
					   (ndr_pull_flags_fn_t)ndr_pull_EVENTLOGRECORD);
			if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
				status = ndr_map_error2ntstatus(ndr_err);
				goto done;
			}

			NDR_PRINT_DEBUG(EVENTLOGRECORD, &r);

			pos += size;

			if (pos + 4 > sent_size) {
				break;
			}

			size = IVAL(data, pos);
		}

		offset++;

	} while (NT_STATUS_IS_OK(result));

 done:
	dcerpc_eventlog_CloseEventLog(b, mem_ctx, &handle, &result);

	return status;
}

static NTSTATUS cmd_eventlog_numrecords(struct rpc_pipe_client *cli,
					TALLOC_CTX *mem_ctx,
					int argc,
					const char **argv)
{
	NTSTATUS status, result;
	struct policy_handle handle;
	uint32_t number = 0;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (argc != 2) {
		printf("Usage: %s logname\n", argv[0]);
		return NT_STATUS_OK;
	}

	status = get_eventlog_handle(cli, mem_ctx, argv[1], &handle);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = dcerpc_eventlog_GetNumRecords(b, mem_ctx,
					       &handle,
					       &number,
					       &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto done;
	}

	printf("number of records: %d\n", number);

 done:
	dcerpc_eventlog_CloseEventLog(b, mem_ctx, &handle, &result);

	return status;
}

static NTSTATUS cmd_eventlog_oldestrecord(struct rpc_pipe_client *cli,
					  TALLOC_CTX *mem_ctx,
					  int argc,
					  const char **argv)
{
	NTSTATUS status, result;
	struct policy_handle handle;
	uint32_t oldest_entry = 0;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (argc != 2) {
		printf("Usage: %s logname\n", argv[0]);
		return NT_STATUS_OK;
	}

	status = get_eventlog_handle(cli, mem_ctx, argv[1], &handle);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = dcerpc_eventlog_GetOldestRecord(b, mem_ctx,
						 &handle,
						 &oldest_entry,
						 &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto done;
	}

	printf("oldest entry: %d\n", oldest_entry);

 done:
	dcerpc_eventlog_CloseEventLog(b, mem_ctx, &handle, &result);

	return status;
}

static NTSTATUS cmd_eventlog_reportevent(struct rpc_pipe_client *cli,
					 TALLOC_CTX *mem_ctx,
					 int argc,
					 const char **argv)
{
	NTSTATUS status, result;
	struct policy_handle handle;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	uint16_t num_of_strings = 1;
	uint32_t data_size = 0;
	struct lsa_String servername;
	struct lsa_String *strings;
	uint8_t *data = NULL;
	uint32_t record_number = 0;
	time_t time_written = 0;

	if (argc != 2) {
		printf("Usage: %s logname\n", argv[0]);
		return NT_STATUS_OK;
	}

	status = get_eventlog_handle(cli, mem_ctx, argv[1], &handle);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	strings = talloc_array(mem_ctx, struct lsa_String, num_of_strings);
	if (!strings) {
		return NT_STATUS_NO_MEMORY;
	}

	init_lsa_String(&strings[0], "test event written by rpcclient\n");
	init_lsa_String(&servername, NULL);

	status = dcerpc_eventlog_ReportEventW(b, mem_ctx,
					      &handle,
					      time(NULL),
					      EVENTLOG_INFORMATION_TYPE,
					      0, /* event_category */
					      0, /* event_id */
					      num_of_strings,
					      data_size,
					      &servername,
					      NULL, /* user_sid */
					      &strings,
					      data,
					      0, /* flags */
					      &record_number,
					      &time_written,
					      &result);

	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto done;
	}

	printf("entry: %d written at %s\n", record_number,
		http_timestring(talloc_tos(), time_written));

 done:
	dcerpc_eventlog_CloseEventLog(b, mem_ctx, &handle, &result);

	return status;
}

static NTSTATUS cmd_eventlog_reporteventsource(struct rpc_pipe_client *cli,
					       TALLOC_CTX *mem_ctx,
					       int argc,
					       const char **argv)
{
	NTSTATUS status, result;
	struct policy_handle handle;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	uint16_t num_of_strings = 1;
	uint32_t data_size = 0;
	struct lsa_String servername, sourcename;
	struct lsa_String *strings;
	uint8_t *data = NULL;
	uint32_t record_number = 0;
	time_t time_written = 0;

	if (argc != 2) {
		printf("Usage: %s logname\n", argv[0]);
		return NT_STATUS_OK;
	}

	status = get_eventlog_handle(cli, mem_ctx, argv[1], &handle);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	strings = talloc_array(mem_ctx, struct lsa_String, num_of_strings);
	if (!strings) {
		return NT_STATUS_NO_MEMORY;
	}

	init_lsa_String(&strings[0], "test event written by rpcclient\n");
	init_lsa_String(&servername, NULL);
	init_lsa_String(&sourcename, "rpcclient");

	status = dcerpc_eventlog_ReportEventAndSourceW(b, mem_ctx,
						       &handle,
						       time(NULL),
						       EVENTLOG_INFORMATION_TYPE,
						       0, /* event_category */
						       0, /* event_id */
						       &sourcename,
						       num_of_strings,
						       data_size,
						       &servername,
						       NULL, /* user_sid */
						       &strings,
						       data,
						       0, /* flags */
						       &record_number,
						       &time_written,
						       &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto done;
	}

	printf("entry: %d written at %s\n", record_number,
		http_timestring(talloc_tos(), time_written));

 done:
	dcerpc_eventlog_CloseEventLog(b, mem_ctx, &handle, &result);

	return status;
}

static NTSTATUS cmd_eventlog_registerevsource(struct rpc_pipe_client *cli,
					      TALLOC_CTX *mem_ctx,
					      int argc,
					      const char **argv)
{
	NTSTATUS status, result;
	struct policy_handle log_handle;
	struct lsa_String module_name, reg_module_name;
	struct eventlog_OpenUnknown0 unknown0;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	unknown0.unknown0 = 0x005c;
	unknown0.unknown1 = 0x0001;

	if (argc != 2) {
		printf("Usage: %s logname\n", argv[0]);
		return NT_STATUS_OK;
	}

	init_lsa_String(&module_name, "rpcclient");
	init_lsa_String(&reg_module_name, NULL);

	status = dcerpc_eventlog_RegisterEventSourceW(b, mem_ctx,
						      &unknown0,
						      &module_name,
						      &reg_module_name,
						      1, /* major_version */
						      1, /* minor_version */
						      &log_handle,
						      &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto done;
	}

 done:
	dcerpc_eventlog_DeregisterEventSource(b, mem_ctx, &log_handle, &result);

	return status;
}

static NTSTATUS cmd_eventlog_backuplog(struct rpc_pipe_client *cli,
				       TALLOC_CTX *mem_ctx,
				       int argc,
				       const char **argv)
{
	NTSTATUS status, result;
	struct policy_handle handle;
	struct lsa_String backup_filename;
	const char *tmp;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (argc != 3) {
		printf("Usage: %s logname backupname\n", argv[0]);
		return NT_STATUS_OK;
	}

	status = get_eventlog_handle(cli, mem_ctx, argv[1], &handle);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	tmp = talloc_asprintf(mem_ctx, "\\??\\%s", argv[2]);
	if (!tmp) {
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}

	init_lsa_String(&backup_filename, tmp);

	status = dcerpc_eventlog_BackupEventLogW(b, mem_ctx,
						 &handle,
						 &backup_filename,
						 &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto done;
	}

 done:
	dcerpc_eventlog_CloseEventLog(b, mem_ctx, &handle, &result);

	return status;
}

static NTSTATUS cmd_eventlog_loginfo(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx,
				     int argc,
				     const char **argv)
{
	NTSTATUS status, result;
	struct policy_handle handle;
	uint8_t *buffer = NULL;
	uint32_t buf_size = 0;
	uint32_t bytes_needed = 0;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (argc != 2) {
		printf("Usage: %s logname\n", argv[0]);
		return NT_STATUS_OK;
	}

	status = get_eventlog_handle(cli, mem_ctx, argv[1], &handle);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = dcerpc_eventlog_GetLogInformation(b, mem_ctx,
						   &handle,
						   0, /* level */
						   buffer,
						   buf_size,
						   &bytes_needed,
						   &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	if (!NT_STATUS_IS_OK(result) &&
	    !NT_STATUS_EQUAL(result, NT_STATUS_BUFFER_TOO_SMALL)) {
		goto done;
	}

	buf_size = bytes_needed;
	buffer = talloc_array(mem_ctx, uint8_t, bytes_needed);
	if (!buffer) {
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}

	status = dcerpc_eventlog_GetLogInformation(b, mem_ctx,
						   &handle,
						   0, /* level */
						   buffer,
						   buf_size,
						   &bytes_needed,
						   &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto done;
	}

 done:
	dcerpc_eventlog_CloseEventLog(b, mem_ctx, &handle, &result);

	return status;
}


struct cmd_set eventlog_commands[] = {
	{ "EVENTLOG" },
	{ "eventlog_readlog",		RPC_RTYPE_NTSTATUS,	cmd_eventlog_readlog,		NULL,	&ndr_table_eventlog.syntax_id,	NULL,	"Read Eventlog", "" },
	{ "eventlog_numrecord",		RPC_RTYPE_NTSTATUS,	cmd_eventlog_numrecords,	NULL,	&ndr_table_eventlog.syntax_id,	NULL,	"Get number of records", "" },
	{ "eventlog_oldestrecord",	RPC_RTYPE_NTSTATUS,	cmd_eventlog_oldestrecord,	NULL,	&ndr_table_eventlog.syntax_id,	NULL,	"Get oldest record", "" },
	{ "eventlog_reportevent",	RPC_RTYPE_NTSTATUS,	cmd_eventlog_reportevent,	NULL,	&ndr_table_eventlog.syntax_id,	NULL,	"Report event", "" },
	{ "eventlog_reporteventsource",	RPC_RTYPE_NTSTATUS,	cmd_eventlog_reporteventsource,	NULL,	&ndr_table_eventlog.syntax_id,	NULL,	"Report event and source", "" },
	{ "eventlog_registerevsource",	RPC_RTYPE_NTSTATUS,	cmd_eventlog_registerevsource,	NULL,	&ndr_table_eventlog.syntax_id,	NULL,	"Register event source", "" },
	{ "eventlog_backuplog",		RPC_RTYPE_NTSTATUS,	cmd_eventlog_backuplog,		NULL,	&ndr_table_eventlog.syntax_id,	NULL,	"Backup Eventlog File", "" },
	{ "eventlog_loginfo",		RPC_RTYPE_NTSTATUS,	cmd_eventlog_loginfo,		NULL,	&ndr_table_eventlog.syntax_id,	NULL,	"Get Eventlog Information", "" },
	{ NULL }
};
