/*
   Unix SMB/CIFS implementation.
   RPC pipe client

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
#include "rpcclient.h"
#include "../librpc/gen_ndr/ndr_winreg_c.h"
#include "../librpc/gen_ndr/ndr_misc.h"

static WERROR cmd_winreg_enumkeys(struct rpc_pipe_client *cli,
				  TALLOC_CTX *mem_ctx, int argc,
				  const char **argv)
{
	NTSTATUS status;
	WERROR werr;
	struct policy_handle handle;
	uint32_t enum_index = 0;
	struct winreg_StringBuf name;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (argc < 2) {
		printf("usage: %s [name]\n", argv[0]);
		return WERR_OK;
	}

	status = dcerpc_winreg_OpenHKLM(b, mem_ctx,
					NULL,
					SEC_FLAG_MAXIMUM_ALLOWED,
					&handle,
					&werr);
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}
	if (!W_ERROR_IS_OK(werr)) {
		return werr;
	}

	ZERO_STRUCT(name);

	name.name = argv[1];
	name.length = strlen_m_term_null(name.name)*2;
	name.size = name.length;

	status = dcerpc_winreg_EnumKey(b, mem_ctx,
				       &handle,
				       enum_index,
				       &name,
				       NULL,
				       NULL,
				       &werr);
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}
	if (!W_ERROR_IS_OK(werr)) {
		return werr;
	}

	return WERR_OK;
}

/****************************************************************************
****************************************************************************/

static WERROR pull_winreg_Data(TALLOC_CTX *mem_ctx,
			       const DATA_BLOB *blob,
			       union winreg_Data *data,
			       enum winreg_Type type)
{
	enum ndr_err_code ndr_err;
	ndr_err = ndr_pull_union_blob(blob, mem_ctx, data, type,
			(ndr_pull_flags_fn_t)ndr_pull_winreg_Data);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return WERR_GENERAL_FAILURE;
	}
	return WERR_OK;
}

/****************************************************************************
****************************************************************************/

static void display_winreg_data(const char *v,
				enum winreg_Type type,
				uint8_t *data,
				uint32_t length)
{
	int i;
	union winreg_Data r;
	DATA_BLOB blob = data_blob_const(data, length);
	WERROR result;

	result = pull_winreg_Data(talloc_tos(), &blob, &r, type);
	if (!W_ERROR_IS_OK(result)) {
		return;
	}

	switch (type) {
	case REG_DWORD:
		printf("%s: REG_DWORD: 0x%08x\n", v, r.value);
		break;
	case REG_SZ:
		printf("%s: REG_SZ: %s\n", v, r.string);
		break;
	case REG_BINARY: {
		char *hex = hex_encode_talloc(NULL,
			r.binary.data, r.binary.length);
		size_t len;
		printf("%s: REG_BINARY:", v);
		len = strlen(hex);
		for (i=0; i<len; i++) {
			if (hex[i] == '\0') {
				break;
			}
			if (i%40 == 0) {
				putchar('\n');
			}
			putchar(hex[i]);
		}
		TALLOC_FREE(hex);
		putchar('\n');
		break;
	}
	case REG_MULTI_SZ:
		printf("%s: REG_MULTI_SZ: ", v);
		for (i=0; r.string_array[i] != NULL; i++) {
			printf("%s ", r.string_array[i]);
		}
		printf("\n");
		break;
	default:
		printf("%s: unknown type 0x%02x:\n", v, type);
		break;
	}
}


static WERROR cmd_winreg_querymultiplevalues_ex(struct rpc_pipe_client *cli,
						TALLOC_CTX *mem_ctx, int argc,
						const char **argv, bool multiplevalues2)
{
	NTSTATUS status;
	WERROR werr;
	struct policy_handle handle, key_handle;
	struct winreg_String key_name = { 0, };
	struct dcerpc_binding_handle *b = cli->binding_handle;

	struct QueryMultipleValue *values_in, *values_out;
	uint32_t num_values;
	uint8_t *buffer = NULL;
	int i;


	if (argc < 2) {
		printf("usage: %s [key] [value1] [value2] ...\n", argv[0]);
		return WERR_OK;
	}

	status = dcerpc_winreg_OpenHKLM(b, mem_ctx,
					NULL,
					SEC_FLAG_MAXIMUM_ALLOWED,
					&handle,
					&werr);
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}
	if (!W_ERROR_IS_OK(werr)) {
		return werr;
	}

	key_name.name = argv[1];

	status = dcerpc_winreg_OpenKey(b, mem_ctx,
				       &handle,
				       key_name,
				       0, /* options */
				       SEC_FLAG_MAXIMUM_ALLOWED,
				       &key_handle,
				       &werr);
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}
	if (!W_ERROR_IS_OK(werr)) {
		return werr;
	}

	num_values = argc-2;

	values_in = talloc_zero_array(mem_ctx, struct QueryMultipleValue, num_values);
	if (values_in == NULL) {
		return WERR_NOMEM;
	}

	values_out = talloc_zero_array(mem_ctx, struct QueryMultipleValue, num_values);
	if (values_out == NULL) {
		return WERR_NOMEM;
	}

	for (i=0; i < num_values; i++) {

		values_in[i].ve_valuename = talloc_zero(values_in, struct winreg_ValNameBuf);
		if (values_in[i].ve_valuename == NULL) {
			return WERR_NOMEM;
		}

		values_in[i].ve_valuename->name = talloc_strdup(values_in[i].ve_valuename, argv[i+2]);
		values_in[i].ve_valuename->length = strlen_m_term_null(values_in[i].ve_valuename->name)*2;
		values_in[i].ve_valuename->size = values_in[i].ve_valuename->length;
	}

	if (multiplevalues2) {

		uint32_t offered = 0, needed = 0;

		status = dcerpc_winreg_QueryMultipleValues2(b, mem_ctx,
							   &key_handle,
							   values_in,
							   values_out,
							   num_values,
							   buffer,
							   &offered,
							   &needed,
							   &werr);
		if (!NT_STATUS_IS_OK(status)) {
			return ntstatus_to_werror(status);
		}
		if (W_ERROR_EQUAL(werr, WERR_MORE_DATA)) {
			offered = needed;

			buffer = talloc_zero_array(mem_ctx, uint8_t, needed);
			if (buffer == NULL) {
				return WERR_NOMEM;
			}

			status = dcerpc_winreg_QueryMultipleValues2(b, mem_ctx,
								    &key_handle,
								    values_in,
								    values_out,
								    num_values,
								    buffer,
								    &offered,
								    &needed,
								    &werr);
			if (!NT_STATUS_IS_OK(status)) {
				return ntstatus_to_werror(status);
			}
			if (!W_ERROR_IS_OK(werr)) {
				return werr;
			}
		}

	} else {

		uint32_t buffer_size = 0xff;

		buffer = talloc_zero_array(mem_ctx, uint8_t, buffer_size);
		if (buffer == NULL) {
			return WERR_NOMEM;
		}

		status = dcerpc_winreg_QueryMultipleValues(b, mem_ctx,
							   &key_handle,
							   values_in,
							   values_out,
							   num_values,
							   buffer,
							   &buffer_size,
							   &werr);
		if (!NT_STATUS_IS_OK(status)) {
			return ntstatus_to_werror(status);
		}
		if (!W_ERROR_IS_OK(werr)) {
			return werr;
		}
	}

	for (i=0; i < num_values; i++) {
		if (buffer) {
			display_winreg_data(values_in[i].ve_valuename->name,
					    values_out[i].ve_type,
					    buffer + values_out[i].ve_valueptr,
					    values_out[i].ve_valuelen);
		}
	}

	return WERR_OK;
}

static WERROR cmd_winreg_querymultiplevalues(struct rpc_pipe_client *cli,
					     TALLOC_CTX *mem_ctx, int argc,
					     const char **argv)
{
	return cmd_winreg_querymultiplevalues_ex(cli, mem_ctx, argc, argv, false);
}

static WERROR cmd_winreg_querymultiplevalues2(struct rpc_pipe_client *cli,
					      TALLOC_CTX *mem_ctx, int argc,
					      const char **argv)
{
	return cmd_winreg_querymultiplevalues_ex(cli, mem_ctx, argc, argv, true);
}

/* List of commands exported by this module */

struct cmd_set winreg_commands[] = {

	{ "WINREG" },
	{ "winreg_enumkey", RPC_RTYPE_WERROR, NULL, cmd_winreg_enumkeys, &ndr_table_winreg.syntax_id, NULL, "Enumerate Keys", "" },
	{ "querymultiplevalues", RPC_RTYPE_WERROR, NULL, cmd_winreg_querymultiplevalues, &ndr_table_winreg.syntax_id, NULL, "Query multiple values", "" },
	{ "querymultiplevalues2", RPC_RTYPE_WERROR, NULL, cmd_winreg_querymultiplevalues2, &ndr_table_winreg.syntax_id, NULL, "Query multiple values", "" },
	{ NULL }
};
