/* 
   Samba Unix/Linux SMB client library 
   Distributed SMB/CIFS Server Management Utility 

   Copyright (C) Gerald (Jerry) Carter          2005-2006

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */
 
#include "includes.h"
#include "utils/net.h"
#include "utils/net_registry_util.h"
#include "regfio.h"
#include "reg_objects.h"
#include "../librpc/gen_ndr/cli_winreg.h"

/*******************************************************************
 connect to a registry hive root (open a registry policy)
*******************************************************************/

static NTSTATUS rpccli_winreg_Connect(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx,
				      uint32_t reg_type, uint32_t access_mask,
				      struct policy_handle *reg_hnd)
{
	ZERO_STRUCTP(reg_hnd);

	switch (reg_type)
	{
	case HKEY_CLASSES_ROOT:
		return rpccli_winreg_OpenHKCR( cli, mem_ctx, NULL,
			access_mask, reg_hnd, NULL);

	case HKEY_LOCAL_MACHINE:
		return rpccli_winreg_OpenHKLM( cli, mem_ctx, NULL,
			access_mask, reg_hnd, NULL);

	case HKEY_USERS:
		return rpccli_winreg_OpenHKU( cli, mem_ctx, NULL,
			access_mask, reg_hnd, NULL);

	case HKEY_CURRENT_USER:
		return rpccli_winreg_OpenHKCU( cli, mem_ctx, NULL,
			access_mask, reg_hnd, NULL);

	case HKEY_PERFORMANCE_DATA:
		return rpccli_winreg_OpenHKPD( cli, mem_ctx, NULL,
			access_mask, reg_hnd, NULL);

	default:
		/* fall through to end of function */
		break;
	}

	return NT_STATUS_INVALID_PARAMETER;
}

static bool reg_hive_key(TALLOC_CTX *ctx, const char *fullname,
			 uint32 *reg_type, const char **key_name)
{
	WERROR werr;
	char *hivename = NULL;
	char *tmp_keyname = NULL;
	bool ret = false;
	TALLOC_CTX *tmp_ctx = talloc_stackframe();

	werr = split_hive_key(tmp_ctx, fullname, &hivename, &tmp_keyname);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	*key_name = talloc_strdup(ctx, tmp_keyname);
	if (*key_name == NULL) {
		goto done;
	}

	if (strequal(hivename, "HKLM") ||
	    strequal(hivename, "HKEY_LOCAL_MACHINE"))
	{
		(*reg_type) = HKEY_LOCAL_MACHINE;
	} else if (strequal(hivename, "HKCR") ||
		   strequal(hivename, "HKEY_CLASSES_ROOT"))
	{
		(*reg_type) = HKEY_CLASSES_ROOT;
	} else if (strequal(hivename, "HKU") ||
		   strequal(hivename, "HKEY_USERS"))
	{
		(*reg_type) = HKEY_USERS;
	} else if (strequal(hivename, "HKCU") ||
		   strequal(hivename, "HKEY_CURRENT_USER"))
	{
		(*reg_type) = HKEY_CURRENT_USER;
	} else if (strequal(hivename, "HKPD") ||
		   strequal(hivename, "HKEY_PERFORMANCE_DATA"))
	{
		(*reg_type) = HKEY_PERFORMANCE_DATA;
	} else {
		DEBUG(10,("reg_hive_key: unrecognised hive key %s\n",
			  fullname));
		goto done;
	}

	ret = true;

done:
	TALLOC_FREE(tmp_ctx);
	return ret;
}

static NTSTATUS registry_openkey(TALLOC_CTX *mem_ctx,
				 struct rpc_pipe_client *pipe_hnd,
				 const char *name, uint32 access_mask,
				 struct policy_handle *hive_hnd,
				 struct policy_handle *key_hnd)
{
	uint32 hive;
	NTSTATUS status;
	struct winreg_String key;

	ZERO_STRUCT(key);

	if (!reg_hive_key(mem_ctx, name, &hive, &key.name)) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	status = rpccli_winreg_Connect(pipe_hnd, mem_ctx, hive, access_mask,
				       hive_hnd);
	if (!(NT_STATUS_IS_OK(status))) {
		return status;
	}

	status = rpccli_winreg_OpenKey(pipe_hnd, mem_ctx, hive_hnd, key, 0,
				       access_mask, key_hnd, NULL);
	if (!(NT_STATUS_IS_OK(status))) {
		rpccli_winreg_CloseKey(pipe_hnd, mem_ctx, hive_hnd, NULL);
		return status;
	}

	return NT_STATUS_OK;
}

static NTSTATUS registry_enumkeys(TALLOC_CTX *ctx,
				  struct rpc_pipe_client *pipe_hnd,
				  struct policy_handle *key_hnd,
				  uint32 *pnum_keys, char ***pnames,
				  char ***pclasses, NTTIME ***pmodtimes)
{
	TALLOC_CTX *mem_ctx;
	NTSTATUS status;
	uint32 num_subkeys, max_subkeylen, max_classlen;
	uint32 num_values, max_valnamelen, max_valbufsize;
	uint32 i;
	NTTIME last_changed_time;
	uint32 secdescsize;
	struct winreg_String classname;
	char **names, **classes;
	NTTIME **modtimes;

	if (!(mem_ctx = talloc_new(ctx))) {
		return NT_STATUS_NO_MEMORY;
	}

	ZERO_STRUCT(classname);
	status = rpccli_winreg_QueryInfoKey(
		pipe_hnd, mem_ctx, key_hnd, &classname, &num_subkeys,
		&max_subkeylen, &max_classlen, &num_values, &max_valnamelen,
		&max_valbufsize, &secdescsize, &last_changed_time, NULL );

	if (!NT_STATUS_IS_OK(status)) {
		goto error;
	}

	if (num_subkeys == 0) {
		*pnum_keys = 0;
		TALLOC_FREE(mem_ctx);
		return NT_STATUS_OK;
	}

	if ((!(names = TALLOC_ZERO_ARRAY(mem_ctx, char *, num_subkeys))) ||
	    (!(classes = TALLOC_ZERO_ARRAY(mem_ctx, char *, num_subkeys))) ||
	    (!(modtimes = TALLOC_ZERO_ARRAY(mem_ctx, NTTIME *,
					    num_subkeys)))) {
		status = NT_STATUS_NO_MEMORY;
		goto error;
	}

	for (i=0; i<num_subkeys; i++) {
		char c, n;
		struct winreg_StringBuf class_buf;
		struct winreg_StringBuf name_buf;
		NTTIME modtime;
		WERROR werr;

		c = '\0';
		class_buf.name = &c;
		class_buf.size = max_classlen+2;

		n = '\0';
		name_buf.name = &n;
		name_buf.size = max_subkeylen+2;

		ZERO_STRUCT(modtime);

		status = rpccli_winreg_EnumKey(pipe_hnd, mem_ctx, key_hnd,
					       i, &name_buf, &class_buf,
					       &modtime, &werr);

		if (W_ERROR_EQUAL(werr,
				  WERR_NO_MORE_ITEMS) ) {
			status = NT_STATUS_OK;
			break;
		}
		if (!NT_STATUS_IS_OK(status)) {
			goto error;
		}

		classes[i] = NULL;

		if (class_buf.name &&
		    (!(classes[i] = talloc_strdup(classes, class_buf.name)))) {
			status = NT_STATUS_NO_MEMORY;
			goto error;
		}

		if (!(names[i] = talloc_strdup(names, name_buf.name))) {
			status = NT_STATUS_NO_MEMORY;
			goto error;
		}

		if ((!(modtimes[i] = (NTTIME *)talloc_memdup(
			       modtimes, &modtime, sizeof(modtime))))) {
			status = NT_STATUS_NO_MEMORY;
			goto error;
		}
	}

	*pnum_keys = num_subkeys;

	if (pnames) {
		*pnames = talloc_move(ctx, &names);
	}
	if (pclasses) {
		*pclasses = talloc_move(ctx, &classes);
	}
	if (pmodtimes) {
		*pmodtimes = talloc_move(ctx, &modtimes);
	}

	status = NT_STATUS_OK;

 error:
	TALLOC_FREE(mem_ctx);
	return status;
}

static NTSTATUS registry_enumvalues(TALLOC_CTX *ctx,
				    struct rpc_pipe_client *pipe_hnd,
				    struct policy_handle *key_hnd,
				    uint32 *pnum_values, char ***pvalnames,
				    struct registry_value ***pvalues)
{
	TALLOC_CTX *mem_ctx;
	NTSTATUS status;
	uint32 num_subkeys, max_subkeylen, max_classlen;
	uint32 num_values, max_valnamelen, max_valbufsize;
	uint32 i;
	NTTIME last_changed_time;
	uint32 secdescsize;
	struct winreg_String classname;
	struct registry_value **values;
	char **names;

	if (!(mem_ctx = talloc_new(ctx))) {
		return NT_STATUS_NO_MEMORY;
	}

	ZERO_STRUCT(classname);
	status = rpccli_winreg_QueryInfoKey(
		pipe_hnd, mem_ctx, key_hnd, &classname, &num_subkeys,
		&max_subkeylen, &max_classlen, &num_values, &max_valnamelen,
		&max_valbufsize, &secdescsize, &last_changed_time, NULL );

	if (!NT_STATUS_IS_OK(status)) {
		goto error;
	}

	if (num_values == 0) {
		*pnum_values = 0;
		TALLOC_FREE(mem_ctx);
		return NT_STATUS_OK;
	}

	if ((!(names = TALLOC_ARRAY(mem_ctx, char *, num_values))) ||
	    (!(values = TALLOC_ARRAY(mem_ctx, struct registry_value *,
				     num_values)))) {
		status = NT_STATUS_NO_MEMORY;
		goto error;
	}

	for (i=0; i<num_values; i++) {
		enum winreg_Type type = REG_NONE;
		uint8 *data = NULL;
		uint32 data_size;
		uint32 value_length;

		char n;
		struct winreg_ValNameBuf name_buf;
		WERROR err;

		n = '\0';
		name_buf.name = &n;
		name_buf.size = max_valnamelen + 2;

		data_size = max_valbufsize;
		data = (uint8 *)TALLOC(mem_ctx, data_size);
		value_length = 0;

		status = rpccli_winreg_EnumValue(pipe_hnd, mem_ctx, key_hnd,
						 i, &name_buf, &type,
						 data, &data_size,
						 &value_length, &err);

		if ( W_ERROR_EQUAL(err,
				   WERR_NO_MORE_ITEMS) ) {
			status = NT_STATUS_OK;
			break;
		}

		if (!(NT_STATUS_IS_OK(status))) {
			goto error;
		}

		if (name_buf.name == NULL) {
			status = NT_STATUS_INVALID_PARAMETER;
			goto error;
		}

		if (!(names[i] = talloc_strdup(names, name_buf.name))) {
			status = NT_STATUS_NO_MEMORY;
			goto error;
		}

		err = registry_pull_value(values, &values[i], type, data,
					  data_size, value_length);
		if (!W_ERROR_IS_OK(err)) {
			status = werror_to_ntstatus(err);
			goto error;
		}
	}

	*pnum_values = num_values;

	if (pvalnames) {
		*pvalnames = talloc_move(ctx, &names);
	}
	if (pvalues) {
		*pvalues = talloc_move(ctx, &values);
	}

	status = NT_STATUS_OK;

 error:
	TALLOC_FREE(mem_ctx);
	return status;
}

static NTSTATUS registry_getsd(TALLOC_CTX *mem_ctx,
			       struct rpc_pipe_client *pipe_hnd,
			       struct policy_handle *key_hnd,
			       uint32_t sec_info,
			       struct KeySecurityData *sd)
{
	return rpccli_winreg_GetKeySecurity(pipe_hnd, mem_ctx, key_hnd,
					    sec_info, sd, NULL);
}


static NTSTATUS registry_setvalue(TALLOC_CTX *mem_ctx,
				  struct rpc_pipe_client *pipe_hnd,
				  struct policy_handle *key_hnd,
				  const char *name,
				  const struct registry_value *value)
{
	struct winreg_String name_string;
	DATA_BLOB blob;
	NTSTATUS result;
	WERROR err;

	err = registry_push_value(mem_ctx, value, &blob);
	if (!W_ERROR_IS_OK(err)) {
		return werror_to_ntstatus(err);
	}

	ZERO_STRUCT(name_string);

	name_string.name = name;
	result = rpccli_winreg_SetValue(pipe_hnd, blob.data, key_hnd,
					name_string, value->type,
					blob.data, blob.length, NULL);
	TALLOC_FREE(blob.data);
	return result;
}

static NTSTATUS rpc_registry_setvalue_internal(struct net_context *c,
					       const DOM_SID *domain_sid,
					       const char *domain_name,
					       struct cli_state *cli,
					       struct rpc_pipe_client *pipe_hnd,
					       TALLOC_CTX *mem_ctx,
					       int argc,
					       const char **argv )
{
	struct policy_handle hive_hnd, key_hnd;
	NTSTATUS status;
	struct registry_value value;

	status = registry_openkey(mem_ctx, pipe_hnd, argv[0],
				  SEC_FLAG_MAXIMUM_ALLOWED,
				  &hive_hnd, &key_hnd);
	if (!NT_STATUS_IS_OK(status)) {
		d_fprintf(stderr, _("registry_openkey failed: %s\n"),
			  nt_errstr(status));
		return status;
	}

	if (!strequal(argv[2], "multi_sz") && (argc != 4)) {
		d_fprintf(stderr, _("Too many args for type %s\n"), argv[2]);
		return NT_STATUS_NOT_IMPLEMENTED;
	}

	if (strequal(argv[2], "dword")) {
		value.type = REG_DWORD;
		value.v.dword = strtoul(argv[3], NULL, 10);
	}
	else if (strequal(argv[2], "sz")) {
		value.type = REG_SZ;
		value.v.sz.len = strlen(argv[3])+1;
		value.v.sz.str = CONST_DISCARD(char *, argv[3]);
	}
	else {
		d_fprintf(stderr, _("type \"%s\" not implemented\n"), argv[2]);
		status = NT_STATUS_NOT_IMPLEMENTED;
		goto error;
	}

	status = registry_setvalue(mem_ctx, pipe_hnd, &key_hnd,
				   argv[1], &value);

	if (!NT_STATUS_IS_OK(status)) {
		d_fprintf(stderr, _("registry_setvalue failed: %s\n"),
			  nt_errstr(status));
	}

 error:
	rpccli_winreg_CloseKey(pipe_hnd, mem_ctx, &key_hnd, NULL);
	rpccli_winreg_CloseKey(pipe_hnd, mem_ctx, &hive_hnd, NULL);

	return NT_STATUS_OK;
}

static int rpc_registry_setvalue(struct net_context *c, int argc,
				 const char **argv )
{
	if (argc < 4 || c->display_usage) {
		d_fprintf(stderr, "%s\n%s",
			  _("Usage:"),
			  _("net rpc registry setvalue <key> <valuename> "
			    "<type> [<val>]+\n"));
		return -1;
	}

	return run_rpc_command(c, NULL, &ndr_table_winreg.syntax_id, 0,
		rpc_registry_setvalue_internal, argc, argv );
}

static NTSTATUS rpc_registry_deletevalue_internal(struct net_context *c,
						  const DOM_SID *domain_sid,
						  const char *domain_name,
						  struct cli_state *cli,
						  struct rpc_pipe_client *pipe_hnd,
						  TALLOC_CTX *mem_ctx,
						  int argc,
						  const char **argv )
{
	struct policy_handle hive_hnd, key_hnd;
	NTSTATUS status;
	struct winreg_String valuename;

	ZERO_STRUCT(valuename);

	status = registry_openkey(mem_ctx, pipe_hnd, argv[0],
				  SEC_FLAG_MAXIMUM_ALLOWED,
				  &hive_hnd, &key_hnd);
	if (!NT_STATUS_IS_OK(status)) {
		d_fprintf(stderr, _("registry_openkey failed: %s\n"),
			  nt_errstr(status));
		return status;
	}

	valuename.name = argv[1];

	status = rpccli_winreg_DeleteValue(pipe_hnd, mem_ctx, &key_hnd,
					   valuename, NULL);

	if (!NT_STATUS_IS_OK(status)) {
		d_fprintf(stderr, _("registry_deletevalue failed: %s\n"),
			  nt_errstr(status));
	}

	rpccli_winreg_CloseKey(pipe_hnd, mem_ctx, &key_hnd, NULL);
	rpccli_winreg_CloseKey(pipe_hnd, mem_ctx, &hive_hnd, NULL);

	return status;
}

static int rpc_registry_deletevalue(struct net_context *c, int argc,
				    const char **argv )
{
	if (argc != 2 || c->display_usage) {
		d_fprintf(stderr, "%s\n%s",
			  _("Usage:"),
			  _("net rpc registry deletevalue <key> <valuename>\n"));
		return -1;
	}

	return run_rpc_command(c, NULL, &ndr_table_winreg.syntax_id, 0,
		rpc_registry_deletevalue_internal, argc, argv );
}

static NTSTATUS rpc_registry_getvalue_internal(struct net_context *c,
					       const DOM_SID *domain_sid,
					       const char *domain_name,
					       struct cli_state *cli,
					       struct rpc_pipe_client *pipe_hnd,
					       TALLOC_CTX *mem_ctx,
					       bool raw,
					       int argc,
					       const char **argv)
{
	struct policy_handle hive_hnd, key_hnd;
	NTSTATUS status;
	WERROR werr;
	struct winreg_String valuename;
	struct registry_value *value = NULL;
	enum winreg_Type type = REG_NONE;
	uint8_t *data = NULL;
	uint32_t data_size = 0;
	uint32_t value_length = 0;
	TALLOC_CTX *tmp_ctx = talloc_stackframe();

	ZERO_STRUCT(valuename);

	status = registry_openkey(tmp_ctx, pipe_hnd, argv[0],
				  SEC_FLAG_MAXIMUM_ALLOWED,
				  &hive_hnd, &key_hnd);
	if (!NT_STATUS_IS_OK(status)) {
		d_fprintf(stderr, _("registry_openkey failed: %s\n"),
			  nt_errstr(status));
		return status;
	}

	valuename.name = argv[1];

	/*
	 * call QueryValue once with data == NULL to get the
	 * needed memory size to be allocated, then allocate
	 * data buffer and call again.
	 */
	status = rpccli_winreg_QueryValue(pipe_hnd, tmp_ctx, &key_hnd,
					  &valuename,
					  &type,
					  data,
					  &data_size,
					  &value_length,
					  NULL);

	if (!NT_STATUS_IS_OK(status)) {
		d_fprintf(stderr, _("registry_queryvalue failed: %s\n"),
			  nt_errstr(status));
		goto done;
	}

	data = (uint8 *)TALLOC(tmp_ctx, data_size);
	value_length = 0;

	status = rpccli_winreg_QueryValue(pipe_hnd, tmp_ctx, &key_hnd,
					  &valuename,
					  &type,
					  data,
					  &data_size,
					  &value_length,
					  NULL);

	if (!NT_STATUS_IS_OK(status)) {
		d_fprintf(stderr, _("registry_queryvalue failed: %s\n"),
			  nt_errstr(status));
		goto done;
	}

	werr = registry_pull_value(tmp_ctx, &value, type, data,
				   data_size, value_length);
	if (!W_ERROR_IS_OK(werr)) {
		status = werror_to_ntstatus(werr);
		goto done;
	}

	print_registry_value(value, raw);

done:
	rpccli_winreg_CloseKey(pipe_hnd, tmp_ctx, &key_hnd, NULL);
	rpccli_winreg_CloseKey(pipe_hnd, tmp_ctx, &hive_hnd, NULL);

	TALLOC_FREE(tmp_ctx);

	return status;
}

static NTSTATUS rpc_registry_getvalue_full(struct net_context *c,
					   const DOM_SID *domain_sid,
					   const char *domain_name,
					   struct cli_state *cli,
					   struct rpc_pipe_client *pipe_hnd,
					   TALLOC_CTX *mem_ctx,
					   int argc,
					   const char **argv)
{
	return rpc_registry_getvalue_internal(c, domain_sid, domain_name,
					      cli, pipe_hnd, mem_ctx, false,
					      argc, argv);
}

static int rpc_registry_getvalue(struct net_context *c, int argc,
				 const char **argv)
{
	if (argc != 2 || c->display_usage) {
		d_fprintf(stderr, "%s\n%s",
			  _("Usage:"),
			  _("net rpc registry getvalue <key> <valuename>\n"));
		return -1;
	}

	return run_rpc_command(c, NULL, &ndr_table_winreg.syntax_id, 0,
		rpc_registry_getvalue_full, argc, argv);
}

static NTSTATUS rpc_registry_getvalue_raw(struct net_context *c,
					  const DOM_SID *domain_sid,
					  const char *domain_name,
					  struct cli_state *cli,
					  struct rpc_pipe_client *pipe_hnd,
					  TALLOC_CTX *mem_ctx,
					  int argc,
					  const char **argv)
{
	return rpc_registry_getvalue_internal(c, domain_sid, domain_name,
					      cli, pipe_hnd, mem_ctx, true,
					      argc, argv);
}

static int rpc_registry_getvalueraw(struct net_context *c, int argc,
				    const char **argv)
{
	if (argc != 2 || c->display_usage) {
		d_fprintf(stderr, "%s\n%s",
			  _("Usage:"),
			  _("net rpc registry getvalue <key> <valuename>\n"));
		return -1;
	}

	return run_rpc_command(c, NULL, &ndr_table_winreg.syntax_id, 0,
		rpc_registry_getvalue_raw, argc, argv);
}

static NTSTATUS rpc_registry_createkey_internal(struct net_context *c,
						const DOM_SID *domain_sid,
						const char *domain_name,
						struct cli_state *cli,
						struct rpc_pipe_client *pipe_hnd,
						TALLOC_CTX *mem_ctx,
						int argc,
						const char **argv )
{
	uint32 hive;
	struct policy_handle hive_hnd, key_hnd;
	struct winreg_String key, keyclass;
	enum winreg_CreateAction action;
	NTSTATUS status;

	ZERO_STRUCT(key);
	ZERO_STRUCT(keyclass);

	if (!reg_hive_key(mem_ctx, argv[0], &hive, &key.name)) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	status = rpccli_winreg_Connect(pipe_hnd, mem_ctx, hive,
				       SEC_FLAG_MAXIMUM_ALLOWED,
				       &hive_hnd);
	if (!(NT_STATUS_IS_OK(status))) {
		return status;
	}

	action = REG_ACTION_NONE;
	keyclass.name = "";

	status = rpccli_winreg_CreateKey(pipe_hnd, mem_ctx, &hive_hnd, key,
					 keyclass, 0, REG_KEY_READ, NULL,
					 &key_hnd, &action, NULL);
	if (!NT_STATUS_IS_OK(status)) {
		d_fprintf(stderr, _("createkey returned %s\n"),
			  nt_errstr(status));
		rpccli_winreg_CloseKey(pipe_hnd, mem_ctx, &hive_hnd, NULL);
		return status;
	}

	switch (action) {
		case REG_ACTION_NONE:
			d_printf(_("createkey did nothing -- huh?\n"));
			break;
		case REG_CREATED_NEW_KEY:
			d_printf(_("createkey created %s\n"), argv[0]);
			break;
		case REG_OPENED_EXISTING_KEY:
			d_printf(_("createkey opened existing %s\n"), argv[0]);
			break;
	}

	rpccli_winreg_CloseKey(pipe_hnd, mem_ctx, &key_hnd, NULL);
	rpccli_winreg_CloseKey(pipe_hnd, mem_ctx, &hive_hnd, NULL);

	return status;
}

static int rpc_registry_createkey(struct net_context *c, int argc,
				  const char **argv )
{
	if (argc != 1 || c->display_usage) {
		d_fprintf(stderr, "%s\n%s",
			  _("Usage:"),
			  _("net rpc registry createkey <key>\n"));
		return -1;
	}

	return run_rpc_command(c, NULL, &ndr_table_winreg.syntax_id, 0,
		rpc_registry_createkey_internal, argc, argv );
}

static NTSTATUS rpc_registry_deletekey_internal(struct net_context *c,
						const DOM_SID *domain_sid,
						const char *domain_name,
						struct cli_state *cli,
						struct rpc_pipe_client *pipe_hnd,
						TALLOC_CTX *mem_ctx,
						int argc,
						const char **argv )
{
	uint32 hive;
	struct policy_handle hive_hnd;
	struct winreg_String key;
	NTSTATUS status;

	ZERO_STRUCT(key);

	if (!reg_hive_key(mem_ctx, argv[0], &hive, &key.name)) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	status = rpccli_winreg_Connect(pipe_hnd, mem_ctx, hive,
				       SEC_FLAG_MAXIMUM_ALLOWED,
				       &hive_hnd);
	if (!(NT_STATUS_IS_OK(status))) {
		return status;
	}

	status = rpccli_winreg_DeleteKey(pipe_hnd, mem_ctx, &hive_hnd, key, NULL);
	rpccli_winreg_CloseKey(pipe_hnd, mem_ctx, &hive_hnd, NULL);

	if (!NT_STATUS_IS_OK(status)) {
		d_fprintf(stderr, _("deletekey returned %s\n"),
			  nt_errstr(status));
	}

	return status;
}

static int rpc_registry_deletekey(struct net_context *c, int argc, const char **argv )
{
	if (argc != 1 || c->display_usage) {
		d_fprintf(stderr, "%s\n%s",
			  _("Usage:"),
			  _("net rpc registry deletekey <key>\n"));
		return -1;
	}

	return run_rpc_command(c, NULL, &ndr_table_winreg.syntax_id, 0,
		rpc_registry_deletekey_internal, argc, argv );
}

/********************************************************************
********************************************************************/

static NTSTATUS rpc_registry_enumerate_internal(struct net_context *c,
						const DOM_SID *domain_sid,
						const char *domain_name,
						struct cli_state *cli,
						struct rpc_pipe_client *pipe_hnd,
						TALLOC_CTX *mem_ctx,
						int argc,
						const char **argv )
{
	struct policy_handle pol_hive, pol_key;
	NTSTATUS status;
	uint32 num_subkeys = 0;
	uint32 num_values = 0;
	char **names = NULL, **classes = NULL;
	NTTIME **modtimes = NULL;
	uint32 i;
	struct registry_value **values = NULL;

	if (argc != 1 || c->display_usage) {
		d_printf("%s\n%s",
			 _("Usage:"),
			 _("net rpc registry enumerate <path>\n"));
		d_printf("%s  net rpc registry enumerate "
			 "'HKLM\\Software\\Samba'\n", _("Example:"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	status = registry_openkey(mem_ctx, pipe_hnd, argv[0], REG_KEY_READ,
				  &pol_hive, &pol_key);
	if (!NT_STATUS_IS_OK(status)) {
		d_fprintf(stderr, _("registry_openkey failed: %s\n"),
			  nt_errstr(status));
		return status;
	}

	status = registry_enumkeys(mem_ctx, pipe_hnd, &pol_key, &num_subkeys,
				   &names, &classes, &modtimes);
	if (!NT_STATUS_IS_OK(status)) {
		d_fprintf(stderr, _("enumerating keys failed: %s\n"),
			  nt_errstr(status));
		return status;
	}

	for (i=0; i<num_subkeys; i++) {
		print_registry_key(names[i], modtimes[i]);
	}

	status = registry_enumvalues(mem_ctx, pipe_hnd, &pol_key, &num_values,
				     &names, &values);
	if (!NT_STATUS_IS_OK(status)) {
		d_fprintf(stderr, _("enumerating values failed: %s\n"),
			  nt_errstr(status));
		return status;
	}

	for (i=0; i<num_values; i++) {
		print_registry_value_with_name(names[i], values[i]);
	}

	rpccli_winreg_CloseKey(pipe_hnd, mem_ctx, &pol_key, NULL);
	rpccli_winreg_CloseKey(pipe_hnd, mem_ctx, &pol_hive, NULL);

	return status;
}

/********************************************************************
********************************************************************/

static int rpc_registry_enumerate(struct net_context *c, int argc,
				  const char **argv )
{
	return run_rpc_command(c, NULL, &ndr_table_winreg.syntax_id, 0,
		rpc_registry_enumerate_internal, argc, argv );
}

/********************************************************************
********************************************************************/

static NTSTATUS rpc_registry_save_internal(struct net_context *c,
					const DOM_SID *domain_sid,
					const char *domain_name,
					struct cli_state *cli,
					struct rpc_pipe_client *pipe_hnd,
					TALLOC_CTX *mem_ctx,
					int argc,
					const char **argv )
{
	WERROR result = WERR_GENERAL_FAILURE;
	struct policy_handle pol_hive, pol_key;
	NTSTATUS status = NT_STATUS_UNSUCCESSFUL;
	struct winreg_String filename;

	if (argc != 2 || c->display_usage) {
		d_printf("%s\n%s",
			 _("Usage:"),
			 _("net rpc registry backup <path> <file> \n"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	status = registry_openkey(mem_ctx, pipe_hnd, argv[0], REG_KEY_ALL,
				  &pol_hive, &pol_key);
	if (!NT_STATUS_IS_OK(status)) {
		d_fprintf(stderr, _("registry_openkey failed: %s\n"),
			  nt_errstr(status));
		return status;
	}

	filename.name = argv[1];
	status = rpccli_winreg_SaveKey( pipe_hnd, mem_ctx, &pol_key, &filename, NULL, NULL);
	if ( !W_ERROR_IS_OK(result) ) {
		d_fprintf(stderr, _("Unable to save [%s] to %s:%s\n"), argv[0],
			  cli->desthost, argv[1]);
	}

	/* cleanup */

	rpccli_winreg_CloseKey(pipe_hnd, mem_ctx, &pol_key, NULL);
	rpccli_winreg_CloseKey(pipe_hnd, mem_ctx, &pol_hive, NULL);

	return status;
}

/********************************************************************
********************************************************************/

static int rpc_registry_save(struct net_context *c, int argc, const char **argv )
{
	return run_rpc_command(c, NULL, &ndr_table_winreg.syntax_id, 0,
		rpc_registry_save_internal, argc, argv );
}


/********************************************************************
********************************************************************/

static void dump_values( REGF_NK_REC *nk )
{
	int i, j;
	const char *data_str = NULL;
	uint32 data_size, data;
	DATA_BLOB blob;

	if ( !nk->values )
		return;

	for ( i=0; i<nk->num_values; i++ ) {
		d_printf( "\"%s\" = ", nk->values[i].valuename ? nk->values[i].valuename : "(default)" );
		d_printf( "(%s) ", reg_type_lookup( nk->values[i].type ) );

		data_size = nk->values[i].data_size & ~VK_DATA_IN_OFFSET;
		switch ( nk->values[i].type ) {
			case REG_SZ:
				blob = data_blob_const(nk->values[i].data, data_size);
				pull_reg_sz(talloc_tos(), &blob, &data_str);
				if (!data_str) {
					break;
				}
				d_printf( "%s", data_str );
				break;
			case REG_MULTI_SZ:
			case REG_EXPAND_SZ:
				for ( j=0; j<data_size; j++ ) {
					d_printf( "%c", nk->values[i].data[j] );
				}
				break;
			case REG_DWORD:
				data = IVAL( nk->values[i].data, 0 );
				d_printf("0x%x", data );
				break;
			case REG_BINARY:
				for ( j=0; j<data_size; j++ ) {
					d_printf( "%x", nk->values[i].data[j] );
				}
				break;
			default:
				d_printf(_("unknown"));
				break;
		}

		d_printf( "\n" );
	}

}

/********************************************************************
********************************************************************/

static bool dump_registry_tree( REGF_FILE *file, REGF_NK_REC *nk, const char *parent )
{
	REGF_NK_REC *key;

	/* depth first dump of the registry tree */

	while ( (key = regfio_fetch_subkey( file, nk )) ) {
		char *regpath;
		if (asprintf(&regpath, "%s\\%s", parent, key->keyname) < 0) {
			break;
		}
		d_printf("[%s]\n", regpath );
		dump_values( key );
		d_printf("\n");
		dump_registry_tree( file, key, regpath );
		SAFE_FREE(regpath);
	}

	return true;
}

/********************************************************************
********************************************************************/

static bool write_registry_tree( REGF_FILE *infile, REGF_NK_REC *nk,
                                 REGF_NK_REC *parent, REGF_FILE *outfile,
			         const char *parentpath )
{
	REGF_NK_REC *key, *subkey;
	struct regval_ctr *values = NULL;
	struct regsubkey_ctr *subkeys = NULL;
	int i;
	char *path = NULL;
	WERROR werr;

	werr = regsubkey_ctr_init(infile->mem_ctx, &subkeys);
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(0, ("write_registry_tree: regsubkey_ctr_init failed: "
			  "%s\n", win_errstr(werr)));
		return false;
	}

	if ( !(values = TALLOC_ZERO_P( subkeys, struct regval_ctr )) ) {
		DEBUG(0,("write_registry_tree: talloc() failed!\n"));
		TALLOC_FREE(subkeys);
		return false;
	}

	/* copy values into the struct regval_ctr */

	for ( i=0; i<nk->num_values; i++ ) {
		regval_ctr_addvalue( values, nk->values[i].valuename, nk->values[i].type,
			(const char *)nk->values[i].data, (nk->values[i].data_size & ~VK_DATA_IN_OFFSET) );
	}

	/* copy subkeys into the struct regsubkey_ctr */

	while ( (subkey = regfio_fetch_subkey( infile, nk )) ) {
		regsubkey_ctr_addkey( subkeys, subkey->keyname );
	}

	key = regfio_write_key( outfile, nk->keyname, values, subkeys, nk->sec_desc->sec_desc, parent );

	/* write each one of the subkeys out */

	path = talloc_asprintf(subkeys,
			"%s%s%s",
			parentpath,
			parent ? "\\" : "",
			nk->keyname);
	if (!path) {
		TALLOC_FREE(subkeys);
		return false;
	}

	nk->subkey_index = 0;
	while ( (subkey = regfio_fetch_subkey( infile, nk )) ) {
		write_registry_tree( infile, subkey, key, outfile, path );
	}

	d_printf("[%s]\n", path );
	TALLOC_FREE(subkeys);

	return true;
}

/********************************************************************
********************************************************************/

static int rpc_registry_dump(struct net_context *c, int argc, const char **argv)
{
	REGF_FILE   *registry;
	REGF_NK_REC *nk;

	if (argc != 1 || c->display_usage) {
		d_printf("%s\n%s",
			 _("Usage:"),
			 _("net rpc registry dump <file> \n"));
		return -1;
	}

	d_printf(_("Opening %s...."), argv[0]);
	if ( !(registry = regfio_open( argv[0], O_RDONLY, 0)) ) {
		d_fprintf(stderr, _("Failed to open %s for reading\n"),argv[0]);
		return 1;
	}
	d_printf(_("ok\n"));

	/* get the root of the registry file */

	if ((nk = regfio_rootkey( registry )) == NULL) {
		d_fprintf(stderr, _("Could not get rootkey\n"));
		regfio_close( registry );
		return 1;
	}
	d_printf("[%s]\n", nk->keyname);
	dump_values( nk );
	d_printf("\n");

	dump_registry_tree( registry, nk, nk->keyname );

#if 0
	talloc_report_full( registry->mem_ctx, stderr );
#endif
	d_printf(_("Closing registry..."));
	regfio_close( registry );
	d_printf(_("ok\n"));

	return 0;
}

/********************************************************************
********************************************************************/

static int rpc_registry_copy(struct net_context *c, int argc, const char **argv )
{
	REGF_FILE   *infile = NULL, *outfile = NULL;
	REGF_NK_REC *nk;
	int result = 1;

	if (argc != 2 || c->display_usage) {
		d_printf("%s\n%s",
			 _("Usage:"),
			 _("net rpc registry copy <srcfile> <newfile>\n"));
		return -1;
	}

	d_printf(_("Opening %s...."), argv[0]);
	if ( !(infile = regfio_open( argv[0], O_RDONLY, 0 )) ) {
		d_fprintf(stderr, _("Failed to open %s for reading\n"),argv[0]);
		return 1;
	}
	d_printf(_("ok\n"));

	d_printf(_("Opening %s...."), argv[1]);
	if ( !(outfile = regfio_open( argv[1], (O_RDWR|O_CREAT|O_TRUNC),
				      (S_IRUSR|S_IWUSR) )) ) {
		d_fprintf(stderr, _("Failed to open %s for writing\n"),argv[1]);
		goto out;
	}
	d_printf(_("ok\n"));

	/* get the root of the registry file */

	if ((nk = regfio_rootkey( infile )) == NULL) {
		d_fprintf(stderr, _("Could not get rootkey\n"));
		goto out;
	}
	d_printf(_("RootKey: [%s]\n"), nk->keyname);

	write_registry_tree( infile, nk, NULL, outfile, "" );

	result = 0;

out:

	d_printf(_("Closing %s..."), argv[1]);
	if (outfile) {
		regfio_close( outfile );
	}
	d_printf(_("ok\n"));

	d_printf(_("Closing %s..."), argv[0]);
	if (infile) {
		regfio_close( infile );
	}
	d_printf(_("ok\n"));

	return( result);
}

/********************************************************************
********************************************************************/

static NTSTATUS rpc_registry_getsd_internal(struct net_context *c,
					    const DOM_SID *domain_sid,
					    const char *domain_name,
					    struct cli_state *cli,
					    struct rpc_pipe_client *pipe_hnd,
					    TALLOC_CTX *mem_ctx,
					    int argc,
					    const char **argv)
{
	struct policy_handle pol_hive, pol_key;
	NTSTATUS status;
	enum ndr_err_code ndr_err;
	struct KeySecurityData *sd = NULL;
	uint32_t sec_info;
	DATA_BLOB blob;
	struct security_descriptor sec_desc;
	uint32_t access_mask = REG_KEY_READ |
			       SEC_FLAG_MAXIMUM_ALLOWED |
			       SEC_FLAG_SYSTEM_SECURITY;

	if (argc <1 || argc > 2 || c->display_usage) {
		d_printf("%s\n%s",
			 _("Usage:"),
			 _("net rpc registry getsd <path> <secinfo>\n"));
		d_printf("%s  net rpc registry getsd "
			   "'HKLM\\Software\\Samba'\n", _("Example:"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	status = registry_openkey(mem_ctx, pipe_hnd, argv[0],
				  access_mask,
				  &pol_hive, &pol_key);
	if (!NT_STATUS_IS_OK(status)) {
		d_fprintf(stderr, _("registry_openkey failed: %s\n"),
			  nt_errstr(status));
		return status;
	}

	sd = TALLOC_ZERO_P(mem_ctx, struct KeySecurityData);
	if (!sd) {
		status = NT_STATUS_NO_MEMORY;
		goto out;
	}

	sd->size = 0x1000;

	if (argc >= 2) {
		sscanf(argv[1], "%x", &sec_info);
	} else {
		sec_info = SECINFO_OWNER | SECINFO_GROUP | SECINFO_DACL;
	}

	status = registry_getsd(mem_ctx, pipe_hnd, &pol_key, sec_info, sd);
	if (!NT_STATUS_IS_OK(status)) {
		d_fprintf(stderr, _("getting sd failed: %s\n"),
			  nt_errstr(status));
		goto out;
	}

	blob.data = sd->data;
	blob.length = sd->size;

	ndr_err = ndr_pull_struct_blob(&blob, mem_ctx, NULL, &sec_desc,
				       (ndr_pull_flags_fn_t)ndr_pull_security_descriptor);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		status = ndr_map_error2ntstatus(ndr_err);
		goto out;
	}
	status = NT_STATUS_OK;

	display_sec_desc(&sec_desc);

 out:
	rpccli_winreg_CloseKey(pipe_hnd, mem_ctx, &pol_key, NULL);
	rpccli_winreg_CloseKey(pipe_hnd, mem_ctx, &pol_hive, NULL);

	return status;
}


static int rpc_registry_getsd(struct net_context *c, int argc, const char **argv)
{
	return run_rpc_command(c, NULL, &ndr_table_winreg.syntax_id, 0,
		rpc_registry_getsd_internal, argc, argv);
}

/********************************************************************
********************************************************************/

int net_rpc_registry(struct net_context *c, int argc, const char **argv)
{
	struct functable func[] = {
		{
			"enumerate",
			rpc_registry_enumerate,
			NET_TRANSPORT_RPC,
			N_("Enumerate registry keys and values"),
			N_("net rpc registry enumerate\n"
			   "    Enumerate registry keys and values")
		},
		{
			"createkey",
			rpc_registry_createkey,
			NET_TRANSPORT_RPC,
			N_("Create a new registry key"),
			N_("net rpc registry createkey\n"
			   "    Create a new registry key")
		},
		{
			"deletekey",
			rpc_registry_deletekey,
			NET_TRANSPORT_RPC,
			N_("Delete a registry key"),
			N_("net rpc registry deletekey\n"
			   "    Delete a registry key")
		},
		{
			"getvalue",
			rpc_registry_getvalue,
			NET_TRANSPORT_RPC,
			N_("Print a registry value"),
			N_("net rpc registry getvalue\n"
			   "    Print a registry value")
		},
		{
			"getvalueraw",
			rpc_registry_getvalueraw,
			NET_TRANSPORT_RPC,
			N_("Print a registry value"),
			N_("net rpc registry getvalueraw\n"
			   "    Print a registry value (raw version)")
		},
		{
			"setvalue",
			rpc_registry_setvalue,
			NET_TRANSPORT_RPC,
			N_("Set a new registry value"),
			N_("net rpc registry setvalue\n"
			   "    Set a new registry value")
		},
		{
			"deletevalue",
			rpc_registry_deletevalue,
			NET_TRANSPORT_RPC,
			N_("Delete a registry value"),
			N_("net rpc registry deletevalue\n"
			   "    Delete a registry value")
		},
		{
			"save",
			rpc_registry_save,
			NET_TRANSPORT_RPC,
			N_("Save a registry file"),
			N_("net rpc registry save\n"
			   "    Save a registry file")
		},
		{
			"dump",
			rpc_registry_dump,
			NET_TRANSPORT_RPC,
			N_("Dump a registry file"),
			N_("net rpc registry dump\n"
			   "    Dump a registry file")
		},
		{
			"copy",
			rpc_registry_copy,
			NET_TRANSPORT_RPC,
			N_("Copy a registry file"),
			N_("net rpc registry copy\n"
			   "    Copy a registry file")
		},
		{
			"getsd",
			rpc_registry_getsd,
			NET_TRANSPORT_RPC,
			N_("Get security descriptor"),
			N_("net rpc registry getsd\n"
			   "    Get security descriptior")
		},
		{NULL, NULL, 0, NULL, NULL}
	};

	return net_run_function(c, argc, argv, "net rpc registry", func);
}
