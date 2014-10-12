/*
 *  Unix SMB/CIFS implementation.
 *
 *  WINREG client routines
 *
 *  Copyright (c) 2011      Andreas Schneider <asn@samba.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"
#include "../librpc/gen_ndr/ndr_winreg_c.h"
#include "../librpc/gen_ndr/ndr_security.h"
#include "rpc_client/cli_winreg.h"
#include "../libcli/registry/util_reg.h"

NTSTATUS dcerpc_winreg_query_dword(TALLOC_CTX *mem_ctx,
				   struct dcerpc_binding_handle *h,
				   struct policy_handle *key_handle,
				   const char *value,
				   uint32_t *data,
				   WERROR *pwerr)
{
	struct winreg_String wvalue;
	enum winreg_Type type = REG_NONE;
	uint32_t value_len = 0;
	uint32_t data_size = 0;
	WERROR result = WERR_OK;
	NTSTATUS status;
	DATA_BLOB blob;

	wvalue.name = value;

	status = dcerpc_winreg_QueryValue(h,
					  mem_ctx,
					  key_handle,
					  &wvalue,
					  &type,
					  NULL,
					  &data_size,
					  &value_len,
					  &result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if (!W_ERROR_IS_OK(result)) {
		*pwerr = result;
		return status;
	}

	if (type != REG_DWORD) {
		*pwerr = WERR_INVALID_DATATYPE;
		return status;
	}

	if (data_size != 4) {
		*pwerr = WERR_INVALID_DATA;
		return status;
	}

	blob = data_blob_talloc_zero(mem_ctx, data_size);
	if (blob.data == NULL) {
		*pwerr = WERR_NOMEM;
		return status;
	}
	value_len = 0;

	status = dcerpc_winreg_QueryValue(h,
					  mem_ctx,
					  key_handle,
					  &wvalue,
					  &type,
					  blob.data,
					  &data_size,
					  &value_len,
					  &result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if (!W_ERROR_IS_OK(result)) {
		*pwerr = result;
		return status;
	}

	if (data) {
		*data = IVAL(blob.data, 0);
	}

	return status;
}

NTSTATUS dcerpc_winreg_query_binary(TALLOC_CTX *mem_ctx,
				    struct dcerpc_binding_handle *h,
				    struct policy_handle *key_handle,
				    const char *value,
				    DATA_BLOB *data,
				    WERROR *pwerr)
{
	struct winreg_String wvalue;
	enum winreg_Type type = REG_NONE;
	WERROR result = WERR_OK;
	uint32_t value_len = 0;
	uint32_t data_size = 0;
	NTSTATUS status;
	DATA_BLOB blob;

	ZERO_STRUCT(wvalue);
	wvalue.name = value;

	status = dcerpc_winreg_QueryValue(h,
					  mem_ctx,
					  key_handle,
					  &wvalue,
					  &type,
					  NULL,
					  &data_size,
					  &value_len,
					  &result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if (!W_ERROR_IS_OK(result)) {
		*pwerr = result;
		return status;
	}

	if (type != REG_BINARY) {
		*pwerr = WERR_INVALID_DATATYPE;
		return status;
	}

	blob = data_blob_talloc_zero(mem_ctx, data_size);
	if (blob.data == NULL) {
		*pwerr = WERR_NOMEM;
		return status;
	}
	value_len = 0;

	status = dcerpc_winreg_QueryValue(h,
					  mem_ctx,
					  key_handle,
					  &wvalue,
					  &type,
					  blob.data,
					  &data_size,
					  &value_len,
					  &result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if (!W_ERROR_IS_OK(result)) {
		*pwerr = result;
		return status;
	}

	if (data) {
		data->data = blob.data;
		data->length = blob.length;
	}

	return status;
}

NTSTATUS dcerpc_winreg_query_multi_sz(TALLOC_CTX *mem_ctx,
				      struct dcerpc_binding_handle *h,
				      struct policy_handle *key_handle,
				      const char *value,
				      const char ***data,
				      WERROR *pwerr)
{
	struct winreg_String wvalue;
	enum winreg_Type type = REG_NONE;
	WERROR result = WERR_OK;
	uint32_t value_len = 0;
	uint32_t data_size = 0;
	NTSTATUS status;
	DATA_BLOB blob;

	wvalue.name = value;

	status = dcerpc_winreg_QueryValue(h,
					  mem_ctx,
					  key_handle,
					  &wvalue,
					  &type,
					  NULL,
					  &data_size,
					  &value_len,
					  &result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if (!W_ERROR_IS_OK(result)) {
		*pwerr = result;
		return status;
	}

	if (type != REG_MULTI_SZ) {
		*pwerr = WERR_INVALID_DATATYPE;
		return status;
	}

	blob = data_blob_talloc_zero(mem_ctx, data_size);
	if (blob.data == NULL) {
		*pwerr = WERR_NOMEM;
		return status;
	}
	value_len = 0;

	status = dcerpc_winreg_QueryValue(h,
					  mem_ctx,
					  key_handle,
					  &wvalue,
					  &type,
					  blob.data,
					  &data_size,
					  &value_len,
					  &result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if (!W_ERROR_IS_OK(result)) {
		*pwerr = result;
		return status;
	}

	if (data) {
		bool ok;

		ok = pull_reg_multi_sz(mem_ctx, &blob, data);
		if (!ok) {
			*pwerr = WERR_NOMEM;
		}
	}

	return status;
}

NTSTATUS dcerpc_winreg_query_sz(TALLOC_CTX *mem_ctx,
				      struct dcerpc_binding_handle *h,
				      struct policy_handle *key_handle,
				      const char *value,
				      const char **data,
				      WERROR *pwerr)
{
	struct winreg_String wvalue;
	enum winreg_Type type = REG_NONE;
	WERROR result = WERR_OK;
	uint32_t value_len = 0;
	uint32_t data_size = 0;
	NTSTATUS status;
	DATA_BLOB blob;

	wvalue.name = value;

	status = dcerpc_winreg_QueryValue(h,
					  mem_ctx,
					  key_handle,
					  &wvalue,
					  &type,
					  NULL,
					  &data_size,
					  &value_len,
					  &result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if (!W_ERROR_IS_OK(result)) {
		*pwerr = result;
		return status;
	}

	if (type != REG_SZ) {
		*pwerr = WERR_INVALID_DATATYPE;
		return status;
	}

	blob = data_blob_talloc_zero(mem_ctx, data_size);
	if (blob.data == NULL) {
		*pwerr = WERR_NOMEM;
		return status;
	}
	value_len = 0;

	status = dcerpc_winreg_QueryValue(h,
					  mem_ctx,
					  key_handle,
					  &wvalue,
					  &type,
					  blob.data,
					  &data_size,
					  &value_len,
					  &result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if (!W_ERROR_IS_OK(result)) {
		*pwerr = result;
		return status;
	}

	if (data) {
		bool ok;

		ok = pull_reg_sz(mem_ctx, &blob, data);
		if (!ok) {
			*pwerr = WERR_NOMEM;
		}
	}

	return status;
}

NTSTATUS dcerpc_winreg_query_sd(TALLOC_CTX *mem_ctx,
				struct dcerpc_binding_handle *h,
				struct policy_handle *key_handle,
				const char *value,
				struct security_descriptor **data,
				WERROR *pwerr)
{
	WERROR result = WERR_OK;
	NTSTATUS status;
	DATA_BLOB blob;

	status = dcerpc_winreg_query_binary(mem_ctx,
					    h,
					    key_handle,
					    value,
					    &blob,
					    &result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if (!W_ERROR_IS_OK(result)) {
		*pwerr = result;
		return status;
	}

	if (data) {
		struct security_descriptor *sd;
		enum ndr_err_code ndr_err;

		sd = talloc_zero(mem_ctx, struct security_descriptor);
		if (sd == NULL) {
			*pwerr = WERR_NOMEM;
			return NT_STATUS_OK;
		}

		ndr_err = ndr_pull_struct_blob(&blob,
					       sd,
					       sd,
					       (ndr_pull_flags_fn_t) ndr_pull_security_descriptor);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			DEBUG(2, ("dcerpc_winreg_query_sd: Failed to marshall "
				  "security descriptor\n"));
			*pwerr = WERR_NOMEM;
			return NT_STATUS_OK;
		}

		*data = sd;
	}

	return status;
}

NTSTATUS dcerpc_winreg_set_dword(TALLOC_CTX *mem_ctx,
				 struct dcerpc_binding_handle *h,
				 struct policy_handle *key_handle,
				 const char *value,
				 uint32_t data,
				 WERROR *pwerr)
{
	struct winreg_String wvalue;
	DATA_BLOB blob;
	WERROR result = WERR_OK;
	NTSTATUS status;

	ZERO_STRUCT(wvalue);
	wvalue.name = value;
	blob = data_blob_talloc_zero(mem_ctx, 4);
	SIVAL(blob.data, 0, data);

	status = dcerpc_winreg_SetValue(h,
					mem_ctx,
					key_handle,
					wvalue,
					REG_DWORD,
					blob.data,
					blob.length,
					&result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if (!W_ERROR_IS_OK(result)) {
		*pwerr = result;
	}

	return status;
}

NTSTATUS dcerpc_winreg_set_sz(TALLOC_CTX *mem_ctx,
			      struct dcerpc_binding_handle *h,
			      struct policy_handle *key_handle,
			      const char *value,
			      const char *data,
			      WERROR *pwerr)
{
	struct winreg_String wvalue = { 0, };
	DATA_BLOB blob;
	WERROR result = WERR_OK;
	NTSTATUS status;

	wvalue.name = value;
	if (data == NULL) {
		blob = data_blob_string_const("");
	} else {
		if (!push_reg_sz(mem_ctx, &blob, data)) {
			DEBUG(2, ("dcerpc_winreg_set_sz: Could not marshall "
				  "string %s for %s\n",
				  data, wvalue.name));
			*pwerr = WERR_NOMEM;
			return NT_STATUS_OK;
		}
	}

	status = dcerpc_winreg_SetValue(h,
					mem_ctx,
					key_handle,
					wvalue,
					REG_SZ,
					blob.data,
					blob.length,
					&result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if (!W_ERROR_IS_OK(result)) {
		*pwerr = result;
	}

	return status;
}

NTSTATUS dcerpc_winreg_set_expand_sz(TALLOC_CTX *mem_ctx,
				     struct dcerpc_binding_handle *h,
				     struct policy_handle *key_handle,
				     const char *value,
				     const char *data,
				     WERROR *pwerr)
{
	struct winreg_String wvalue = { 0, };
	DATA_BLOB blob;
	WERROR result = WERR_OK;
	NTSTATUS status;

	wvalue.name = value;
	if (data == NULL) {
		blob = data_blob_string_const("");
	} else {
		if (!push_reg_sz(mem_ctx, &blob, data)) {
			DEBUG(2, ("dcerpc_winreg_set_expand_sz: Could not marshall "
				  "string %s for %s\n",
				  data, wvalue.name));
			*pwerr = WERR_NOMEM;
			return NT_STATUS_OK;
		}
	}

	status = dcerpc_winreg_SetValue(h,
					mem_ctx,
					key_handle,
					wvalue,
					REG_EXPAND_SZ,
					blob.data,
					blob.length,
					&result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if (!W_ERROR_IS_OK(result)) {
		*pwerr = result;
	}

	return status;
}

NTSTATUS dcerpc_winreg_set_multi_sz(TALLOC_CTX *mem_ctx,
				    struct dcerpc_binding_handle *h,
				    struct policy_handle *key_handle,
				    const char *value,
				    const char **data,
				    WERROR *pwerr)
{
	struct winreg_String wvalue = { 0, };
	DATA_BLOB blob;
	WERROR result = WERR_OK;
	NTSTATUS status;

	wvalue.name = value;
	if (!push_reg_multi_sz(mem_ctx, &blob, data)) {
		DEBUG(2, ("dcerpc_winreg_set_multi_sz: Could not marshall "
			  "string multi sz for %s\n",
			  wvalue.name));
		*pwerr = WERR_NOMEM;
		return NT_STATUS_OK;
	}

	status = dcerpc_winreg_SetValue(h,
					mem_ctx,
					key_handle,
					wvalue,
					REG_MULTI_SZ,
					blob.data,
					blob.length,
					&result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if (!W_ERROR_IS_OK(result)) {
		*pwerr = result;
	}

	return status;
}

NTSTATUS dcerpc_winreg_set_binary(TALLOC_CTX *mem_ctx,
				  struct dcerpc_binding_handle *h,
				  struct policy_handle *key_handle,
				  const char *value,
				  DATA_BLOB *data,
				  WERROR *pwerr)
{
	struct winreg_String wvalue = { 0, };
	WERROR result = WERR_OK;
	NTSTATUS status;

	wvalue.name = value;

	status = dcerpc_winreg_SetValue(h,
					mem_ctx,
					key_handle,
					wvalue,
					REG_BINARY,
					data->data,
					data->length,
					&result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if (!W_ERROR_IS_OK(result)) {
		*pwerr = result;
	}

	return status;
}

NTSTATUS dcerpc_winreg_set_sd(TALLOC_CTX *mem_ctx,
			      struct dcerpc_binding_handle *h,
			      struct policy_handle *key_handle,
			      const char *value,
			      const struct security_descriptor *data,
			      WERROR *pwerr)
{
	enum ndr_err_code ndr_err;
	DATA_BLOB blob;

	ndr_err = ndr_push_struct_blob(&blob,
				       mem_ctx,
				       data,
				       (ndr_push_flags_fn_t) ndr_push_security_descriptor);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DEBUG(2, ("dcerpc_winreg_set_sd: Failed to marshall security "
			  "descriptor\n"));
		*pwerr = WERR_NOMEM;
		return NT_STATUS_OK;
	}

	return dcerpc_winreg_set_binary(mem_ctx,
					h,
					key_handle,
					value,
					&blob,
					pwerr);
}

NTSTATUS dcerpc_winreg_add_multi_sz(TALLOC_CTX *mem_ctx,
				    struct dcerpc_binding_handle *h,
				    struct policy_handle *key_handle,
				    const char *value,
				    const char *data,
				    WERROR *pwerr)
{
	const char **a = NULL;
	const char **p;
	uint32_t i;
	WERROR result = WERR_OK;
	NTSTATUS status;

	status = dcerpc_winreg_query_multi_sz(mem_ctx,
					      h,
					      key_handle,
					      value,
					      &a,
					      &result);

	/* count the elements */
	for (p = a, i = 0; p && *p; p++, i++);

	p = TALLOC_REALLOC_ARRAY(mem_ctx, a, const char *, i + 2);
	if (p == NULL) {
		*pwerr = WERR_NOMEM;
		return NT_STATUS_OK;
	}

	p[i] = data;
	p[i + 1] = NULL;

	status = dcerpc_winreg_set_multi_sz(mem_ctx,
					    h,
					    key_handle,
					    value,
					    p,
					    pwerr);

	return status;
}

NTSTATUS dcerpc_winreg_enum_keys(TALLOC_CTX *mem_ctx,
				 struct dcerpc_binding_handle *h,
				 struct policy_handle *key_hnd,
				 uint32_t *pnum_subkeys,
				 const char ***psubkeys,
				 WERROR *pwerr)
{
	const char **subkeys;
	uint32_t num_subkeys, max_subkeylen, max_classlen;
	uint32_t num_values, max_valnamelen, max_valbufsize;
	uint32_t i;
	NTTIME last_changed_time;
	uint32_t secdescsize;
	struct winreg_String classname;
	WERROR result = WERR_OK;
	NTSTATUS status;
	TALLOC_CTX *tmp_ctx;

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	ZERO_STRUCT(classname);

	status = dcerpc_winreg_QueryInfoKey(h,
					    tmp_ctx,
					    key_hnd,
					    &classname,
					    &num_subkeys,
					    &max_subkeylen,
					    &max_classlen,
					    &num_values,
					    &max_valnamelen,
					    &max_valbufsize,
					    &secdescsize,
					    &last_changed_time,
					    &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto error;
	}
	if (!W_ERROR_IS_OK(result)) {
		*pwerr = result;
		goto error;
	}

	subkeys = talloc_zero_array(tmp_ctx, const char *, num_subkeys + 2);
	if (subkeys == NULL) {
		*pwerr = WERR_NOMEM;
		goto error;
	}

	if (num_subkeys == 0) {
		subkeys[0] = talloc_strdup(subkeys, "");
		if (subkeys[0] == NULL) {
			*pwerr = WERR_NOMEM;
			goto error;
		}
		*pnum_subkeys = 0;
		if (psubkeys) {
			*psubkeys = talloc_move(mem_ctx, &subkeys);
		}

		TALLOC_FREE(tmp_ctx);
		return NT_STATUS_OK;
	}

	for (i = 0; i < num_subkeys; i++) {
		char c = '\0';
		char n = '\0';
		char *name = NULL;
		struct winreg_StringBuf class_buf;
		struct winreg_StringBuf name_buf;
		NTTIME modtime;

		class_buf.name = &c;
		class_buf.size = max_classlen + 2;
		class_buf.length = 0;

		name_buf.name = &n;
		name_buf.size = max_subkeylen + 2;
		name_buf.length = 0;

		ZERO_STRUCT(modtime);

		status = dcerpc_winreg_EnumKey(h,
					       tmp_ctx,
					       key_hnd,
					       i,
					       &name_buf,
					       &class_buf,
					       &modtime,
					       &result);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(5, ("dcerpc_winreg_enum_keys: Could not enumerate keys: %s\n",
				  nt_errstr(status)));
			goto error;
		}

		if (W_ERROR_EQUAL(result, WERR_NO_MORE_ITEMS) ) {
			*pwerr = WERR_OK;
			break;
		}
		if (!W_ERROR_IS_OK(result)) {
			DEBUG(5, ("dcerpc_winreg_enum_keys: Could not enumerate keys: %s\n",
				  win_errstr(result)));
			*pwerr = result;
			goto error;
		}

		if (name_buf.name == NULL) {
			*pwerr = WERR_INVALID_PARAMETER;
			goto error;
		}

		name = talloc_strdup(subkeys, name_buf.name);
		if (name == NULL) {
			*pwerr = WERR_NOMEM;
			goto error;
		}

		subkeys[i] = name;
	}

	*pnum_subkeys = num_subkeys;
	if (psubkeys) {
		*psubkeys = talloc_move(mem_ctx, &subkeys);
	}

 error:
	TALLOC_FREE(tmp_ctx);

	return status;
}

/* vim: set ts=8 sw=8 noet cindent syntax=c.doxygen: */
