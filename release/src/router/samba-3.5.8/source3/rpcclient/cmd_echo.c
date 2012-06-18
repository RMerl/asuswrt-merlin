/* 
   Unix SMB/CIFS implementation.
   RPC pipe client

   Copyright (C) Tim Potter 2003

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
#include "../librpc/gen_ndr/cli_echo.h"

static NTSTATUS cmd_echo_add_one(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx,
				 int argc, const char **argv)
{
	uint32 request = 1, response;
	NTSTATUS result;

	if (argc > 2) {
		printf("Usage: %s [num]\n", argv[0]);
		return NT_STATUS_OK;
	}

	if (argc == 2)
		request = atoi(argv[1]);

	result = rpccli_echo_AddOne(cli, mem_ctx, request, &response);

	if (!NT_STATUS_IS_OK(result))
		goto done;

	printf("%d + 1 = %d\n", request, response);

done:
	return result;
}

static NTSTATUS cmd_echo_data(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx,
			      int argc, const char **argv)
{
	uint32 size, i;
	NTSTATUS result;
	uint8_t *in_data = NULL, *out_data = NULL;

	if (argc != 2) {
		printf("Usage: %s num\n", argv[0]);
		return NT_STATUS_OK;
	}

	size = atoi(argv[1]);
	if ( (in_data = (uint8_t*)SMB_MALLOC(size)) == NULL ) {
		printf("Failure to allocate buff of %d bytes\n",
		       size);
		result = NT_STATUS_NO_MEMORY;
		goto done;
	}
	if ( (out_data = (uint8_t*)SMB_MALLOC(size)) == NULL ) {
		printf("Failure to allocate buff of %d bytes\n",
		       size);
		result = NT_STATUS_NO_MEMORY;
		goto done;
	}

	for (i = 0; i < size; i++)
		in_data[i] = i & 0xff;

	result = rpccli_echo_EchoData(cli, mem_ctx, size, in_data, out_data);

	if (!NT_STATUS_IS_OK(result))
		goto done;

	for (i = 0; i < size; i++) {
		if (in_data[i] != out_data[i]) {
			printf("mismatch at offset %d, %d != %d\n",
			       i, in_data[i], out_data[i]);
			result = NT_STATUS_UNSUCCESSFUL;
		}
	}

done:
	SAFE_FREE(in_data);
	SAFE_FREE(out_data);

	return result;
}

static NTSTATUS cmd_echo_source_data(struct rpc_pipe_client *cli, 
				     TALLOC_CTX *mem_ctx, int argc, 
				     const char **argv)
{
	uint32 size, i;
	NTSTATUS result;
	uint8_t *out_data = NULL;

	if (argc != 2) {
		printf("Usage: %s num\n", argv[0]);
		return NT_STATUS_OK;
	}

	size = atoi(argv[1]);
	if ( (out_data = (uint8_t*)SMB_MALLOC(size)) == NULL ) {
		printf("Failure to allocate buff of %d bytes\n",
		       size);
		result = NT_STATUS_NO_MEMORY;
		goto done;		
	}
	

	result = rpccli_echo_SourceData(cli, mem_ctx, size, out_data);

	if (!NT_STATUS_IS_OK(result))
		goto done;

	for (i = 0; i < size; i++) {
		if (out_data && out_data[i] != (i & 0xff)) {
			printf("mismatch at offset %d, %d != %d\n",
			       i, out_data[i], i & 0xff);
			result = NT_STATUS_UNSUCCESSFUL;
		}
	}

done:

	SAFE_FREE(out_data);
	return result;
}

static NTSTATUS cmd_echo_sink_data(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx,
				   int argc, const char **argv)
{
	uint32 size, i;
	NTSTATUS result;
	uint8_t *in_data = NULL;

	if (argc != 2) {
		printf("Usage: %s num\n", argv[0]);
		return NT_STATUS_OK;
	}

	size = atoi(argv[1]);
	if ( (in_data = (uint8_t*)SMB_MALLOC(size)) == NULL ) {
		printf("Failure to allocate buff of %d bytes\n",
		       size);
		result = NT_STATUS_NO_MEMORY;
		goto done;		
	}

	for (i = 0; i < size; i++)
		in_data[i] = i & 0xff;

	result = rpccli_echo_SinkData(cli, mem_ctx, size, in_data);

	if (!NT_STATUS_IS_OK(result))
		goto done;

done:
	SAFE_FREE(in_data);

	return result;
}

/* List of commands exported by this module */

struct cmd_set echo_commands[] = {

	{ "ECHO" },

	{ "echoaddone", RPC_RTYPE_NTSTATUS, cmd_echo_add_one,     NULL, &ndr_table_rpcecho.syntax_id, NULL, "Add one to a number", "" },
	{ "echodata",   RPC_RTYPE_NTSTATUS, cmd_echo_data,        NULL, &ndr_table_rpcecho.syntax_id, NULL, "Echo data",           "" },
	{ "sinkdata",   RPC_RTYPE_NTSTATUS, cmd_echo_sink_data,   NULL, &ndr_table_rpcecho.syntax_id, NULL, "Sink data",           "" },
	{ "sourcedata", RPC_RTYPE_NTSTATUS, cmd_echo_source_data, NULL, &ndr_table_rpcecho.syntax_id, NULL, "Source data",         "" },
	{ NULL }
};
