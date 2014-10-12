/*
   Unix SMB/CIFS implementation.
   RPC pipe client

   Copyright (C) Gerald Carter                2001-2005,
   Copyright (C) Tim Potter                   2000-2002,
   Copyright (C) Andrew Tridgell              1994-2000,
   Copyright (C) Jean-Francois Micouleau      1999-2000.
   Copyright (C) Jeremy Allison                         2005.

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
#include "rpc_client/rpc_client.h"
#include "../librpc/gen_ndr/ndr_spoolss_c.h"
#include "rpc_client/cli_spoolss.h"

/**********************************************************************
 convencience wrapper around rpccli_spoolss_OpenPrinterEx
**********************************************************************/

WERROR rpccli_spoolss_openprinter_ex(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx,
				     const char *printername,
				     uint32_t access_desired,
				     struct policy_handle *handle)
{
	NTSTATUS status;
	WERROR werror;
	struct spoolss_DevmodeContainer devmode_ctr;
	union spoolss_UserLevel userlevel;
	struct spoolss_UserLevel1 level1;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	ZERO_STRUCT(devmode_ctr);

	level1.size	= 28;
	level1.client	= talloc_asprintf(mem_ctx, "\\\\%s", global_myname());
	W_ERROR_HAVE_NO_MEMORY(level1.client);
	level1.user	= cli->auth->user_name;
	level1.build	= 1381;
	level1.major	= 2;
	level1.minor	= 0;
	level1.processor = 0;

	userlevel.level1 = &level1;

	status = dcerpc_spoolss_OpenPrinterEx(b, mem_ctx,
					      printername,
					      NULL,
					      devmode_ctr,
					      access_desired,
					      1, /* level */
					      userlevel,
					      handle,
					      &werror);

	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	if (!W_ERROR_IS_OK(werror)) {
		return werror;
	}

	return WERR_OK;
}

/**********************************************************************
 convencience wrapper around rpccli_spoolss_GetPrinterDriver
**********************************************************************/

WERROR rpccli_spoolss_getprinterdriver(struct rpc_pipe_client *cli,
				       TALLOC_CTX *mem_ctx,
				       struct policy_handle *handle,
				       const char *architecture,
				       uint32_t level,
				       uint32_t offered,
				       union spoolss_DriverInfo *info)
{
	NTSTATUS status;
	WERROR werror;
	uint32_t needed;
	DATA_BLOB buffer;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (offered > 0) {
		buffer = data_blob_talloc_zero(mem_ctx, offered);
		W_ERROR_HAVE_NO_MEMORY(buffer.data);
	}

	status = dcerpc_spoolss_GetPrinterDriver(b, mem_ctx,
						 handle,
						 architecture,
						 level,
						 (offered > 0) ? &buffer : NULL,
						 offered,
						 info,
						 &needed,
						 &werror);
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}
	if (W_ERROR_EQUAL(werror, WERR_INSUFFICIENT_BUFFER)) {
		offered = needed;
		buffer = data_blob_talloc_zero(mem_ctx, needed);
		W_ERROR_HAVE_NO_MEMORY(buffer.data);

		status = dcerpc_spoolss_GetPrinterDriver(b, mem_ctx,
							 handle,
							 architecture,
							 level,
							 &buffer,
							 offered,
							 info,
							 &needed,
							 &werror);
	}
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	return werror;
}

/**********************************************************************
 convencience wrapper around rpccli_spoolss_GetPrinterDriver2
**********************************************************************/

WERROR rpccli_spoolss_getprinterdriver2(struct rpc_pipe_client *cli,
					TALLOC_CTX *mem_ctx,
					struct policy_handle *handle,
					const char *architecture,
					uint32_t level,
					uint32_t offered,
					uint32_t client_major_version,
					uint32_t client_minor_version,
					union spoolss_DriverInfo *info,
					uint32_t *server_major_version,
					uint32_t *server_minor_version)
{
	NTSTATUS status;
	WERROR werror;
	uint32_t needed;
	DATA_BLOB buffer;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (offered > 0) {
		buffer = data_blob_talloc_zero(mem_ctx, offered);
		W_ERROR_HAVE_NO_MEMORY(buffer.data);
	}

	status = dcerpc_spoolss_GetPrinterDriver2(b, mem_ctx,
						  handle,
						  architecture,
						  level,
						  (offered > 0) ? &buffer : NULL,
						  offered,
						  client_major_version,
						  client_minor_version,
						  info,
						  &needed,
						  server_major_version,
						  server_minor_version,
						  &werror);
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	if (W_ERROR_EQUAL(werror, WERR_INSUFFICIENT_BUFFER)) {
		offered = needed;
		buffer = data_blob_talloc_zero(mem_ctx, needed);
		W_ERROR_HAVE_NO_MEMORY(buffer.data);

		status = dcerpc_spoolss_GetPrinterDriver2(b, mem_ctx,
							  handle,
							  architecture,
							  level,
							  &buffer,
							  offered,
							  client_major_version,
							  client_minor_version,
							  info,
							  &needed,
							  server_major_version,
							  server_minor_version,
							  &werror);
	}
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	return werror;
}

/**********************************************************************
 convencience wrapper around rpccli_spoolss_AddPrinterEx
**********************************************************************/

WERROR rpccli_spoolss_addprinterex(struct rpc_pipe_client *cli,
				   TALLOC_CTX *mem_ctx,
				   struct spoolss_SetPrinterInfoCtr *info_ctr)
{
	WERROR result;
	NTSTATUS status;
	struct spoolss_DevmodeContainer devmode_ctr;
	struct sec_desc_buf secdesc_ctr;
	struct spoolss_UserLevelCtr userlevel_ctr;
	struct spoolss_UserLevel1 level1;
	struct policy_handle handle;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	ZERO_STRUCT(devmode_ctr);
	ZERO_STRUCT(secdesc_ctr);

	level1.size		= 28;
	level1.build		= 1381;
	level1.major		= 2;
	level1.minor		= 0;
	level1.processor	= 0;
	level1.client		= talloc_asprintf(mem_ctx, "\\\\%s", global_myname());
	W_ERROR_HAVE_NO_MEMORY(level1.client);
	level1.user		= cli->auth->user_name;

	userlevel_ctr.level = 1;
	userlevel_ctr.user_info.level1 = &level1;

	status = dcerpc_spoolss_AddPrinterEx(b, mem_ctx,
					     cli->srv_name_slash,
					     info_ctr,
					     &devmode_ctr,
					     &secdesc_ctr,
					     &userlevel_ctr,
					     &handle,
					     &result);
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	return result;
}

/**********************************************************************
 convencience wrapper around rpccli_spoolss_GetPrinter
**********************************************************************/

WERROR rpccli_spoolss_getprinter(struct rpc_pipe_client *cli,
				 TALLOC_CTX *mem_ctx,
				 struct policy_handle *handle,
				 uint32_t level,
				 uint32_t offered,
				 union spoolss_PrinterInfo *info)
{
	NTSTATUS status;
	WERROR werror;
	DATA_BLOB buffer;
	uint32_t needed;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (offered > 0) {
		buffer = data_blob_talloc_zero(mem_ctx, offered);
		W_ERROR_HAVE_NO_MEMORY(buffer.data);
	}

	status = dcerpc_spoolss_GetPrinter(b, mem_ctx,
					   handle,
					   level,
					   (offered > 0) ? &buffer : NULL,
					   offered,
					   info,
					   &needed,
					   &werror);
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	if (W_ERROR_EQUAL(werror, WERR_INSUFFICIENT_BUFFER)) {

		offered = needed;
		buffer = data_blob_talloc_zero(mem_ctx, offered);
		W_ERROR_HAVE_NO_MEMORY(buffer.data);

		status = dcerpc_spoolss_GetPrinter(b, mem_ctx,
						   handle,
						   level,
						   &buffer,
						   offered,
						   info,
						   &needed,
						   &werror);
	}
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	return werror;
}

/**********************************************************************
 convencience wrapper around rpccli_spoolss_GetJob
**********************************************************************/

WERROR rpccli_spoolss_getjob(struct rpc_pipe_client *cli,
			     TALLOC_CTX *mem_ctx,
			     struct policy_handle *handle,
			     uint32_t job_id,
			     uint32_t level,
			     uint32_t offered,
			     union spoolss_JobInfo *info)
{
	NTSTATUS status;
	WERROR werror;
	uint32_t needed;
	DATA_BLOB buffer;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (offered > 0) {
		buffer = data_blob_talloc_zero(mem_ctx, offered);
		W_ERROR_HAVE_NO_MEMORY(buffer.data);
	}

	status = dcerpc_spoolss_GetJob(b, mem_ctx,
				       handle,
				       job_id,
				       level,
				       (offered > 0) ? &buffer : NULL,
				       offered,
				       info,
				       &needed,
				       &werror);
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	if (W_ERROR_EQUAL(werror, WERR_INSUFFICIENT_BUFFER)) {
		offered = needed;
		buffer = data_blob_talloc_zero(mem_ctx, needed);
		W_ERROR_HAVE_NO_MEMORY(buffer.data);

		status = dcerpc_spoolss_GetJob(b, mem_ctx,
					       handle,
					       job_id,
					       level,
					       &buffer,
					       offered,
					       info,
					       &needed,
					       &werror);
	}
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	return werror;
}

/**********************************************************************
 convencience wrapper around rpccli_spoolss_EnumForms
**********************************************************************/

WERROR rpccli_spoolss_enumforms(struct rpc_pipe_client *cli,
				TALLOC_CTX *mem_ctx,
				struct policy_handle *handle,
				uint32_t level,
				uint32_t offered,
				uint32_t *count,
				union spoolss_FormInfo **info)
{
	NTSTATUS status;
	WERROR werror;
	uint32_t needed;
	DATA_BLOB buffer;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (offered > 0) {
		buffer = data_blob_talloc_zero(mem_ctx, offered);
		W_ERROR_HAVE_NO_MEMORY(buffer.data);
	}

	status = dcerpc_spoolss_EnumForms(b, mem_ctx,
					  handle,
					  level,
					  (offered > 0) ? &buffer : NULL,
					  offered,
					  count,
					  info,
					  &needed,
					  &werror);
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	if (W_ERROR_EQUAL(werror, WERR_INSUFFICIENT_BUFFER)) {
		offered = needed;
		buffer = data_blob_talloc_zero(mem_ctx, needed);
		W_ERROR_HAVE_NO_MEMORY(buffer.data);

		status = dcerpc_spoolss_EnumForms(b, mem_ctx,
						  handle,
						  level,
						  (offered > 0) ? &buffer : NULL,
						  offered,
						  count,
						  info,
						  &needed,
						  &werror);
	}
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	return werror;
}

/**********************************************************************
 convencience wrapper around rpccli_spoolss_EnumPrintProcessors
**********************************************************************/

WERROR rpccli_spoolss_enumprintprocessors(struct rpc_pipe_client *cli,
					  TALLOC_CTX *mem_ctx,
					  const char *servername,
					  const char *environment,
					  uint32_t level,
					  uint32_t offered,
					  uint32_t *count,
					  union spoolss_PrintProcessorInfo **info)
{
	NTSTATUS status;
	WERROR werror;
	uint32_t needed;
	DATA_BLOB buffer;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (offered > 0) {
		buffer = data_blob_talloc_zero(mem_ctx, offered);
		W_ERROR_HAVE_NO_MEMORY(buffer.data);
	}

	status = dcerpc_spoolss_EnumPrintProcessors(b, mem_ctx,
						    servername,
						    environment,
						    level,
						    (offered > 0) ? &buffer : NULL,
						    offered,
						    count,
						    info,
						    &needed,
						    &werror);
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	if (W_ERROR_EQUAL(werror, WERR_INSUFFICIENT_BUFFER)) {
		offered = needed;
		buffer = data_blob_talloc_zero(mem_ctx, needed);
		W_ERROR_HAVE_NO_MEMORY(buffer.data);

		status = dcerpc_spoolss_EnumPrintProcessors(b, mem_ctx,
							    servername,
							    environment,
							    level,
							    (offered > 0) ? &buffer : NULL,
							    offered,
							    count,
							    info,
							    &needed,
							    &werror);
	}
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	return werror;
}

/**********************************************************************
 convencience wrapper around rpccli_spoolss_EnumPrintProcDataTypes
**********************************************************************/

WERROR rpccli_spoolss_enumprintprocessordatatypes(struct rpc_pipe_client *cli,
						  TALLOC_CTX *mem_ctx,
						  const char *servername,
						  const char *print_processor_name,
						  uint32_t level,
						  uint32_t offered,
						  uint32_t *count,
						  union spoolss_PrintProcDataTypesInfo **info)
{
	NTSTATUS status;
	WERROR werror;
	uint32_t needed;
	DATA_BLOB buffer;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (offered > 0) {
		buffer = data_blob_talloc_zero(mem_ctx, offered);
		W_ERROR_HAVE_NO_MEMORY(buffer.data);
	}

	status = dcerpc_spoolss_EnumPrintProcDataTypes(b, mem_ctx,
						       servername,
						       print_processor_name,
						       level,
						       (offered > 0) ? &buffer : NULL,
						       offered,
						       count,
						       info,
						       &needed,
						       &werror);
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	if (W_ERROR_EQUAL(werror, WERR_INSUFFICIENT_BUFFER)) {
		offered = needed;
		buffer = data_blob_talloc_zero(mem_ctx, needed);
		W_ERROR_HAVE_NO_MEMORY(buffer.data);

		status = dcerpc_spoolss_EnumPrintProcDataTypes(b, mem_ctx,
							       servername,
							       print_processor_name,
							       level,
							       (offered > 0) ? &buffer : NULL,
							       offered,
							       count,
							       info,
							       &needed,
							       &werror);
	}
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	return werror;
}

/**********************************************************************
 convencience wrapper around rpccli_spoolss_EnumPorts
**********************************************************************/

WERROR rpccli_spoolss_enumports(struct rpc_pipe_client *cli,
				TALLOC_CTX *mem_ctx,
				const char *servername,
				uint32_t level,
				uint32_t offered,
				uint32_t *count,
				union spoolss_PortInfo **info)
{
	NTSTATUS status;
	WERROR werror;
	uint32_t needed;
	DATA_BLOB buffer;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (offered > 0) {
		buffer = data_blob_talloc_zero(mem_ctx, offered);
		W_ERROR_HAVE_NO_MEMORY(buffer.data);
	}

	status = dcerpc_spoolss_EnumPorts(b, mem_ctx,
					  servername,
					  level,
					  (offered > 0) ? &buffer : NULL,
					  offered,
					  count,
					  info,
					  &needed,
					  &werror);
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	if (W_ERROR_EQUAL(werror, WERR_INSUFFICIENT_BUFFER)) {
		offered = needed;
		buffer = data_blob_talloc_zero(mem_ctx, needed);
		W_ERROR_HAVE_NO_MEMORY(buffer.data);

		status = dcerpc_spoolss_EnumPorts(b, mem_ctx,
						  servername,
						  level,
						  (offered > 0) ? &buffer : NULL,
						  offered,
						  count,
						  info,
						  &needed,
						  &werror);
	}
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	return werror;
}

/**********************************************************************
 convencience wrapper around rpccli_spoolss_EnumMonitors
**********************************************************************/

WERROR rpccli_spoolss_enummonitors(struct rpc_pipe_client *cli,
				   TALLOC_CTX *mem_ctx,
				   const char *servername,
				   uint32_t level,
				   uint32_t offered,
				   uint32_t *count,
				   union spoolss_MonitorInfo **info)
{
	NTSTATUS status;
	WERROR werror;
	uint32_t needed;
	DATA_BLOB buffer;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (offered > 0) {
		buffer = data_blob_talloc_zero(mem_ctx, offered);
		W_ERROR_HAVE_NO_MEMORY(buffer.data);
	}

	status = dcerpc_spoolss_EnumMonitors(b, mem_ctx,
					     servername,
					     level,
					     (offered > 0) ? &buffer : NULL,
					     offered,
					     count,
					     info,
					     &needed,
					     &werror);
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	if (W_ERROR_EQUAL(werror, WERR_INSUFFICIENT_BUFFER)) {
		offered = needed;
		buffer = data_blob_talloc_zero(mem_ctx, needed);
		W_ERROR_HAVE_NO_MEMORY(buffer.data);

		status = dcerpc_spoolss_EnumMonitors(b, mem_ctx,
						     servername,
						     level,
						     (offered > 0) ? &buffer : NULL,
						     offered,
						     count,
						     info,
						     &needed,
						     &werror);
	}
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	return werror;
}

/**********************************************************************
 convencience wrapper around rpccli_spoolss_EnumJobs
**********************************************************************/

WERROR rpccli_spoolss_enumjobs(struct rpc_pipe_client *cli,
			       TALLOC_CTX *mem_ctx,
			       struct policy_handle *handle,
			       uint32_t firstjob,
			       uint32_t numjobs,
			       uint32_t level,
			       uint32_t offered,
			       uint32_t *count,
			       union spoolss_JobInfo **info)
{
	NTSTATUS status;
	WERROR werror;
	uint32_t needed;
	DATA_BLOB buffer;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (offered > 0) {
		buffer = data_blob_talloc_zero(mem_ctx, offered);
		W_ERROR_HAVE_NO_MEMORY(buffer.data);
	}

	status = dcerpc_spoolss_EnumJobs(b, mem_ctx,
					 handle,
					 firstjob,
					 numjobs,
					 level,
					 (offered > 0) ? &buffer : NULL,
					 offered,
					 count,
					 info,
					 &needed,
					 &werror);
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	if (W_ERROR_EQUAL(werror, WERR_INSUFFICIENT_BUFFER)) {
		offered = needed;
		buffer = data_blob_talloc_zero(mem_ctx, needed);
		W_ERROR_HAVE_NO_MEMORY(buffer.data);

		status = dcerpc_spoolss_EnumJobs(b, mem_ctx,
						 handle,
						 firstjob,
						 numjobs,
						 level,
						 (offered > 0) ? &buffer : NULL,
						 offered,
						 count,
						 info,
						 &needed,
						 &werror);
	}
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	return werror;
}

/**********************************************************************
 convencience wrapper around rpccli_spoolss_EnumPrinterDrivers
**********************************************************************/

WERROR rpccli_spoolss_enumprinterdrivers(struct rpc_pipe_client *cli,
					 TALLOC_CTX *mem_ctx,
					 const char *server,
					 const char *environment,
					 uint32_t level,
					 uint32_t offered,
					 uint32_t *count,
					 union spoolss_DriverInfo **info)
{
	NTSTATUS status;
	WERROR werror;
	uint32_t needed;
	DATA_BLOB buffer;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (offered > 0) {
		buffer = data_blob_talloc_zero(mem_ctx, offered);
		W_ERROR_HAVE_NO_MEMORY(buffer.data);
	}

	status = dcerpc_spoolss_EnumPrinterDrivers(b, mem_ctx,
						   server,
						   environment,
						   level,
						   (offered > 0) ? &buffer : NULL,
						   offered,
						   count,
						   info,
						   &needed,
						   &werror);
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	if (W_ERROR_EQUAL(werror, WERR_INSUFFICIENT_BUFFER)) {
		offered = needed;
		buffer = data_blob_talloc_zero(mem_ctx, needed);
		W_ERROR_HAVE_NO_MEMORY(buffer.data);

		status = dcerpc_spoolss_EnumPrinterDrivers(b, mem_ctx,
						   server,
						   environment,
						   level,
						   (offered > 0) ? &buffer : NULL,
						   offered,
						   count,
						   info,
						   &needed,
						   &werror);
	}
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	return werror;
}

/**********************************************************************
 convencience wrapper around rpccli_spoolss_EnumPrinters
**********************************************************************/

WERROR rpccli_spoolss_enumprinters(struct rpc_pipe_client *cli,
				   TALLOC_CTX *mem_ctx,
				   uint32_t flags,
				   const char *server,
				   uint32_t level,
				   uint32_t offered,
				   uint32_t *count,
				   union spoolss_PrinterInfo **info)
{
	NTSTATUS status;
	WERROR werror;
	uint32_t needed;
	DATA_BLOB buffer;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (offered > 0) {
		buffer = data_blob_talloc_zero(mem_ctx, offered);
		W_ERROR_HAVE_NO_MEMORY(buffer.data);
	}

	status = dcerpc_spoolss_EnumPrinters(b, mem_ctx,
					     flags,
					     server,
					     level,
					     (offered > 0) ? &buffer : NULL,
					     offered,
					     count,
					     info,
					     &needed,
					     &werror);
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	if (W_ERROR_EQUAL(werror, WERR_INSUFFICIENT_BUFFER)) {
		offered = needed;
		buffer = data_blob_talloc_zero(mem_ctx, needed);
		W_ERROR_HAVE_NO_MEMORY(buffer.data);

		status = dcerpc_spoolss_EnumPrinters(b, mem_ctx,
						     flags,
						     server,
						     level,
						     (offered > 0) ? &buffer : NULL,
						     offered,
						     count,
						     info,
						     &needed,
						     &werror);
	}
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	return werror;
}

/**********************************************************************
 convencience wrapper around rpccli_spoolss_GetPrinterData
**********************************************************************/

WERROR rpccli_spoolss_getprinterdata(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx,
				     struct policy_handle *handle,
				     const char *value_name,
				     uint32_t offered,
				     enum winreg_Type *type,
				     uint32_t *needed_p,
				     uint8_t **data_p)
{
	NTSTATUS status;
	WERROR werror;
	uint32_t needed;
	uint8_t *data;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	data = talloc_zero_array(mem_ctx, uint8_t, offered);
	W_ERROR_HAVE_NO_MEMORY(data);

	status = dcerpc_spoolss_GetPrinterData(b, mem_ctx,
					       handle,
					       value_name,
					       type,
					       data,
					       offered,
					       &needed,
					       &werror);
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	if (W_ERROR_EQUAL(werror, WERR_MORE_DATA)) {
		offered = needed;
		data = talloc_zero_array(mem_ctx, uint8_t, offered);
		W_ERROR_HAVE_NO_MEMORY(data);

		status = dcerpc_spoolss_GetPrinterData(b, mem_ctx,
						       handle,
						       value_name,
						       type,
						       data,
						       offered,
						       &needed,
						       &werror);
	}
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	*data_p = data;
	*needed_p = needed;

	return werror;
}

/**********************************************************************
 convencience wrapper around rpccli_spoolss_EnumPrinterKey
**********************************************************************/

WERROR rpccli_spoolss_enumprinterkey(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx,
				     struct policy_handle *handle,
				     const char *key_name,
				     const char ***key_buffer,
				     uint32_t offered)
{
	NTSTATUS status;
	WERROR werror;
	uint32_t needed;
	union spoolss_KeyNames _key_buffer;
	uint32_t _ndr_size;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	status = dcerpc_spoolss_EnumPrinterKey(b, mem_ctx,
					       handle,
					       key_name,
					       &_ndr_size,
					       &_key_buffer,
					       offered,
					       &needed,
					       &werror);
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	if (W_ERROR_EQUAL(werror, WERR_MORE_DATA)) {
		offered = needed;
		status = dcerpc_spoolss_EnumPrinterKey(b, mem_ctx,
						       handle,
						       key_name,
						       &_ndr_size,
						       &_key_buffer,
						       offered,
						       &needed,
						       &werror);
	}
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	*key_buffer = _key_buffer.string_array;

	return werror;
}

/**********************************************************************
 convencience wrapper around rpccli_spoolss_EnumPrinterDataEx
**********************************************************************/

WERROR rpccli_spoolss_enumprinterdataex(struct rpc_pipe_client *cli,
					TALLOC_CTX *mem_ctx,
					struct policy_handle *handle,
					const char *key_name,
					uint32_t offered,
					uint32_t *count,
					struct spoolss_PrinterEnumValues **info)
{
	NTSTATUS status;
	WERROR werror;
	uint32_t needed;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	status = dcerpc_spoolss_EnumPrinterDataEx(b, mem_ctx,
						  handle,
						  key_name,
						  offered,
						  count,
						  info,
						  &needed,
						  &werror);
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	if (W_ERROR_EQUAL(werror, WERR_MORE_DATA)) {
		offered = needed;

		status = dcerpc_spoolss_EnumPrinterDataEx(b, mem_ctx,
							  handle,
							  key_name,
							  offered,
							  count,
							  info,
							  &needed,
							  &werror);
	}
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	return werror;
}
