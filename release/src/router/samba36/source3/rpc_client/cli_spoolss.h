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

#ifndef _RPC_CLIENT_CLI_SPOOLSS_H_
#define _RPC_CLIENT_CLI_SPOOLSS_H_

/* The following definitions come from rpc_client/cli_spoolss.c  */

WERROR rpccli_spoolss_openprinter_ex(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx,
				     const char *printername,
				     uint32_t access_desired,
				     struct policy_handle *handle);
WERROR rpccli_spoolss_getprinterdriver(struct rpc_pipe_client *cli,
				       TALLOC_CTX *mem_ctx,
				       struct policy_handle *handle,
				       const char *architecture,
				       uint32_t level,
				       uint32_t offered,
				       union spoolss_DriverInfo *info);
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
					uint32_t *server_minor_version);
WERROR rpccli_spoolss_addprinterex(struct rpc_pipe_client *cli,
				   TALLOC_CTX *mem_ctx,
				   struct spoolss_SetPrinterInfoCtr *info_ctr);
WERROR rpccli_spoolss_getprinter(struct rpc_pipe_client *cli,
				 TALLOC_CTX *mem_ctx,
				 struct policy_handle *handle,
				 uint32_t level,
				 uint32_t offered,
				 union spoolss_PrinterInfo *info);
WERROR rpccli_spoolss_getjob(struct rpc_pipe_client *cli,
			     TALLOC_CTX *mem_ctx,
			     struct policy_handle *handle,
			     uint32_t job_id,
			     uint32_t level,
			     uint32_t offered,
			     union spoolss_JobInfo *info);
WERROR rpccli_spoolss_enumforms(struct rpc_pipe_client *cli,
				TALLOC_CTX *mem_ctx,
				struct policy_handle *handle,
				uint32_t level,
				uint32_t offered,
				uint32_t *count,
				union spoolss_FormInfo **info);
WERROR rpccli_spoolss_enumprintprocessors(struct rpc_pipe_client *cli,
					  TALLOC_CTX *mem_ctx,
					  const char *servername,
					  const char *environment,
					  uint32_t level,
					  uint32_t offered,
					  uint32_t *count,
					  union spoolss_PrintProcessorInfo **info);
WERROR rpccli_spoolss_enumprintprocessordatatypes(struct rpc_pipe_client *cli,
						  TALLOC_CTX *mem_ctx,
						  const char *servername,
						  const char *print_processor_name,
						  uint32_t level,
						  uint32_t offered,
						  uint32_t *count,
						  union spoolss_PrintProcDataTypesInfo **info);
WERROR rpccli_spoolss_enumports(struct rpc_pipe_client *cli,
				TALLOC_CTX *mem_ctx,
				const char *servername,
				uint32_t level,
				uint32_t offered,
				uint32_t *count,
				union spoolss_PortInfo **info);
WERROR rpccli_spoolss_enummonitors(struct rpc_pipe_client *cli,
				   TALLOC_CTX *mem_ctx,
				   const char *servername,
				   uint32_t level,
				   uint32_t offered,
				   uint32_t *count,
				   union spoolss_MonitorInfo **info);
WERROR rpccli_spoolss_enumjobs(struct rpc_pipe_client *cli,
			       TALLOC_CTX *mem_ctx,
			       struct policy_handle *handle,
			       uint32_t firstjob,
			       uint32_t numjobs,
			       uint32_t level,
			       uint32_t offered,
			       uint32_t *count,
			       union spoolss_JobInfo **info);
WERROR rpccli_spoolss_enumprinterdrivers(struct rpc_pipe_client *cli,
					 TALLOC_CTX *mem_ctx,
					 const char *server,
					 const char *environment,
					 uint32_t level,
					 uint32_t offered,
					 uint32_t *count,
					 union spoolss_DriverInfo **info);
WERROR rpccli_spoolss_enumprinters(struct rpc_pipe_client *cli,
				   TALLOC_CTX *mem_ctx,
				   uint32_t flags,
				   const char *server,
				   uint32_t level,
				   uint32_t offered,
				   uint32_t *count,
				   union spoolss_PrinterInfo **info);
WERROR rpccli_spoolss_getprinterdata(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx,
				     struct policy_handle *handle,
				     const char *value_name,
				     uint32_t offered,
				     enum winreg_Type *type,
				     uint32_t *needed_p,
				     uint8_t **data_p);
WERROR rpccli_spoolss_enumprinterkey(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx,
				     struct policy_handle *handle,
				     const char *key_name,
				     const char ***key_buffer,
				     uint32_t offered);
WERROR rpccli_spoolss_enumprinterdataex(struct rpc_pipe_client *cli,
					TALLOC_CTX *mem_ctx,
					struct policy_handle *handle,
					const char *key_name,
					uint32_t offered,
					uint32_t *count,
					struct spoolss_PrinterEnumValues **info);

#endif /* _RPC_CLIENT_CLI_SPOOLSS_H_ */
