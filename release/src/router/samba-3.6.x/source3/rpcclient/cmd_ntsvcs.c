/*
   Unix SMB/CIFS implementation.
   RPC pipe client

   Copyright (C) Günther Deschner 2008

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
#include "../librpc/gen_ndr/ndr_ntsvcs_c.h"

static WERROR cmd_ntsvcs_get_version(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx,
				     int argc,
				     const char **argv)
{
	struct dcerpc_binding_handle *b = cli->binding_handle;
	NTSTATUS status;
	WERROR werr;
	uint16_t version;

	status = dcerpc_PNP_GetVersion(b, mem_ctx,
				       &version, &werr);
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	if (W_ERROR_IS_OK(werr)) {
		printf("version: %d\n", version);
	}

	return werr;
}

static WERROR cmd_ntsvcs_validate_dev_inst(struct rpc_pipe_client *cli,
					   TALLOC_CTX *mem_ctx,
					   int argc,
					   const char **argv)
{
	struct dcerpc_binding_handle *b = cli->binding_handle;
	NTSTATUS status;
	WERROR werr;
	const char *devicepath = NULL;
	uint32_t flags = 0;

	if (argc < 2 || argc > 3) {
		printf("usage: %s [devicepath] <flags>\n", argv[0]);
		return WERR_OK;
	}

	devicepath = argv[1];

	if (argc >= 3) {
		flags = atoi(argv[2]);
	}

	status = dcerpc_PNP_ValidateDeviceInstance(b, mem_ctx,
						   devicepath,
						   flags,
						   &werr);
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	return werr;
}

static WERROR cmd_ntsvcs_hw_prof_flags(struct rpc_pipe_client *cli,
				       TALLOC_CTX *mem_ctx,
				       int argc,
				       const char **argv)
{
	struct dcerpc_binding_handle *b = cli->binding_handle;
	NTSTATUS status;
	WERROR werr;
	const char *devicepath = NULL;
	uint32_t profile_flags = 0;
	uint16_t veto_type = 0;
	const char *unk5 = NULL;
	const char *unk5a = NULL;

	if (argc < 2) {
		printf("usage: %s [devicepath]\n", argv[0]);
		return WERR_OK;
	}

	devicepath = argv[1];

	status = dcerpc_PNP_HwProfFlags(b, mem_ctx,
					0,
					devicepath,
					0,
					&profile_flags,
					&veto_type,
					unk5,
					&unk5a,
					0,
					0,
					&werr);
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	return werr;
}

static WERROR cmd_ntsvcs_get_hw_prof_info(struct rpc_pipe_client *cli,
					  TALLOC_CTX *mem_ctx,
					  int argc,
					  const char **argv)
{
	struct dcerpc_binding_handle *b = cli->binding_handle;
	NTSTATUS status;
	WERROR werr;
	uint32_t idx = 0;
	struct PNP_HwProfInfo info;
	uint32_t size = 0, flags = 0;

	ZERO_STRUCT(info);

	status = dcerpc_PNP_GetHwProfInfo(b, mem_ctx,
					  idx,
					  &info,
					  size,
					  flags,
					  &werr);
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	return werr;
}

static WERROR cmd_ntsvcs_get_dev_reg_prop(struct rpc_pipe_client *cli,
					  TALLOC_CTX *mem_ctx,
					  int argc,
					  const char **argv)
{
	struct dcerpc_binding_handle *b = cli->binding_handle;
	NTSTATUS status;
	WERROR werr;
	const char *devicepath = NULL;
	uint32_t property = DEV_REGPROP_DESC;
	uint32_t reg_data_type = REG_NONE;
	uint8_t *buffer;
	uint32_t buffer_size = 0;
	uint32_t needed = 0;
	uint32_t flags = 0;

	if (argc < 2) {
		printf("usage: %s [devicepath] [buffersize]\n", argv[0]);
		return WERR_OK;
	}

	devicepath = argv[1];

	if (argc >= 3) {
		buffer_size = atoi(argv[2]);
		needed = buffer_size;
	}

	buffer = talloc_array(mem_ctx, uint8_t, buffer_size);
	W_ERROR_HAVE_NO_MEMORY(buffer);

	status = dcerpc_PNP_GetDeviceRegProp(b, mem_ctx,
					     devicepath,
					     property,
					     &reg_data_type,
					     buffer,
					     &buffer_size,
					     &needed,
					     flags,
					     &werr);
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	return werr;
}

static WERROR cmd_ntsvcs_get_dev_list_size(struct rpc_pipe_client *cli,
					   TALLOC_CTX *mem_ctx,
					   int argc,
					   const char **argv)
{
	struct dcerpc_binding_handle *b = cli->binding_handle;
	NTSTATUS status;
	WERROR werr;
	uint32_t size = 0;
	uint32_t flags = 0;
	const char *filter = NULL;

	if (argc > 3) {
		printf("usage: %s [filter] [flags]\n", argv[0]);
		return WERR_OK;
	}

	if (argc >= 2) {
		filter = argv[1];
	}

	if (argc >= 3) {
		flags = atoi(argv[2]);
	}

	status = dcerpc_PNP_GetDeviceListSize(b, mem_ctx,
					      filter,
					      &size,
					      flags,
					      &werr);
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	printf("size: %d\n", size);

	return werr;
}

static WERROR cmd_ntsvcs_get_dev_list(struct rpc_pipe_client *cli,
				      TALLOC_CTX *mem_ctx,
				      int argc,
				      const char **argv)
{
	struct dcerpc_binding_handle *b = cli->binding_handle;
	NTSTATUS status;
	WERROR werr;
	const char *filter = NULL;
	uint16_t *buffer = NULL;
	uint32_t length = 0;
	uint32_t flags = 0;

	if (argc > 4) {
		printf("usage: %s [filter] [length] [flags]\n", argv[0]);
		return WERR_OK;
	}

	if (argc >= 2) {
		filter = argv[1];
	}

	if (argc >= 3) {
		length = atoi(argv[2]);
	}

	if (argc >= 4) {
		flags = atoi(argv[3]);
	}

	buffer = talloc(mem_ctx, uint16_t);
	if (!buffer) {
		return WERR_NOMEM;
	}

	status = dcerpc_PNP_GetDeviceList(b, mem_ctx,
					  filter,
					  buffer,
					  &length,
					  flags,
					  &werr);
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	printf("devlist needs size: %d\n", length);

	return werr;
}

struct cmd_set ntsvcs_commands[] = {

	{ "NTSVCS" },
	{ "ntsvcs_getversion", RPC_RTYPE_WERROR, NULL, cmd_ntsvcs_get_version, &ndr_table_ntsvcs.syntax_id, NULL, "Query NTSVCS version", "" },
	{ "ntsvcs_validatedevinst", RPC_RTYPE_WERROR, NULL, cmd_ntsvcs_validate_dev_inst, &ndr_table_ntsvcs.syntax_id, NULL, "Query NTSVCS device instance", "" },
	{ "ntsvcs_hwprofflags", RPC_RTYPE_WERROR, NULL, cmd_ntsvcs_hw_prof_flags, &ndr_table_ntsvcs.syntax_id, NULL, "Query NTSVCS HW prof flags", "" },
	{ "ntsvcs_hwprofinfo", RPC_RTYPE_WERROR, NULL, cmd_ntsvcs_get_hw_prof_info, &ndr_table_ntsvcs.syntax_id, NULL, "Query NTSVCS HW prof info", "" },
	{ "ntsvcs_getdevregprop", RPC_RTYPE_WERROR, NULL, cmd_ntsvcs_get_dev_reg_prop, &ndr_table_ntsvcs.syntax_id, NULL, "Query NTSVCS device registry property", "" },
	{ "ntsvcs_getdevlistsize", RPC_RTYPE_WERROR, NULL, cmd_ntsvcs_get_dev_list_size, &ndr_table_ntsvcs.syntax_id, NULL, "Query NTSVCS device list size", "" },
	{ "ntsvcs_getdevlist", RPC_RTYPE_WERROR, NULL, cmd_ntsvcs_get_dev_list, &ndr_table_ntsvcs.syntax_id, NULL, "Query NTSVCS device list", "" },
	{ NULL }
};
