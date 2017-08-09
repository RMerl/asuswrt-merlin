/*
   Unix SMB/CIFS implementation.

   endpoint server for the winreg pipe

   Copyright (C) 2004 Jelmer Vernooij, jelmer@samba.org
   Copyright (C) 2008 Matthias Dieter Wallnöfer, mwallnoefer@yahoo.de

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
#include "rpc_server/dcerpc_server.h"
#include "lib/registry/registry.h"
#include "librpc/gen_ndr/ndr_winreg.h"
#include "librpc/gen_ndr/ndr_security.h"
#include "libcli/security/session.h"

enum handle_types { HTYPE_REGVAL, HTYPE_REGKEY };

static NTSTATUS dcerpc_winreg_bind(struct dcesrv_call_state *dce_call,
				   const struct dcesrv_interface *iface)
{
	struct registry_context *ctx;
	WERROR err;

	err = reg_open_samba(dce_call->context,
			     &ctx, dce_call->event_ctx, dce_call->conn->dce_ctx->lp_ctx,
			     dce_call->conn->auth_state.session_info,
			     NULL);

	if (!W_ERROR_IS_OK(err)) {
		DEBUG(0, ("Error opening registry: %s\n", win_errstr(err)));
		return NT_STATUS_UNSUCCESSFUL;
	}

	dce_call->context->private_data = ctx;

	return NT_STATUS_OK;
}

#define DCESRV_INTERFACE_WINREG_BIND dcerpc_winreg_bind

static WERROR dcesrv_winreg_openhive(struct dcesrv_call_state *dce_call,
				     TALLOC_CTX *mem_ctx, uint32_t hkey,
				     struct policy_handle **outh)
{
	struct registry_context *ctx = dce_call->context->private_data;
	struct dcesrv_handle *h;
	WERROR result;

	h = dcesrv_handle_new(dce_call->context, HTYPE_REGKEY);
	W_ERROR_HAVE_NO_MEMORY(h);

	result = reg_get_predefined_key(ctx, hkey,
				       (struct registry_key **)&h->data);
	if (!W_ERROR_IS_OK(result)) {
		return result;
	}
	*outh = &h->wire_handle;

	return result;
}

#define func_winreg_OpenHive(k,n) static WERROR dcesrv_winreg_Open ## k (struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx, struct winreg_Open ## k *r) \
{ \
	return dcesrv_winreg_openhive (dce_call, mem_ctx, n, &r->out.handle);\
}

func_winreg_OpenHive(HKCR,HKEY_CLASSES_ROOT)
func_winreg_OpenHive(HKCU,HKEY_CURRENT_USER)
func_winreg_OpenHive(HKLM,HKEY_LOCAL_MACHINE)
func_winreg_OpenHive(HKPD,HKEY_PERFORMANCE_DATA)
func_winreg_OpenHive(HKU,HKEY_USERS)
func_winreg_OpenHive(HKCC,HKEY_CURRENT_CONFIG)
func_winreg_OpenHive(HKDD,HKEY_DYN_DATA)
func_winreg_OpenHive(HKPT,HKEY_PERFORMANCE_TEXT)
func_winreg_OpenHive(HKPN,HKEY_PERFORMANCE_NLSTEXT)

/*
  winreg_CloseKey
*/
static WERROR dcesrv_winreg_CloseKey(struct dcesrv_call_state *dce_call,
				     TALLOC_CTX *mem_ctx,
				     struct winreg_CloseKey *r)
{
	struct dcesrv_handle *h;

	DCESRV_PULL_HANDLE_FAULT(h, r->in.handle, HTYPE_REGKEY);

	talloc_unlink(dce_call->context, h);

	ZERO_STRUCTP(r->out.handle);

	return WERR_OK;
}

/*
  winreg_CreateKey
*/
static WERROR dcesrv_winreg_CreateKey(struct dcesrv_call_state *dce_call,
				      TALLOC_CTX *mem_ctx,
				      struct winreg_CreateKey *r)
{
	struct dcesrv_handle *h, *newh;
	struct security_descriptor sd;
	struct registry_key *key;
	WERROR result;

	DCESRV_PULL_HANDLE_FAULT(h, r->in.handle, HTYPE_REGKEY);
	key = h->data;

	newh = dcesrv_handle_new(dce_call->context, HTYPE_REGKEY);

	switch (security_session_user_level(dce_call->conn->auth_state.session_info, NULL))
	{
	case SECURITY_SYSTEM:
	case SECURITY_ADMINISTRATOR:
		/* we support only non volatile keys */
		if (r->in.options != REG_OPTION_NON_VOLATILE) {
			return WERR_NOT_SUPPORTED;
		}

		/* the security descriptor is optional */
		if (r->in.secdesc != NULL) {
			DATA_BLOB sdblob;
			enum ndr_err_code ndr_err;
			sdblob.data = r->in.secdesc->sd.data;
			sdblob.length = r->in.secdesc->sd.len;
			if (sdblob.data == NULL) {
				return WERR_INVALID_PARAM;
			}
			ndr_err = ndr_pull_struct_blob_all(&sdblob, mem_ctx, &sd,
							   (ndr_pull_flags_fn_t)ndr_pull_security_descriptor);
			if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
				return WERR_INVALID_PARAM;
			}
		}
		
		result = reg_key_add_name(newh, key, r->in.name.name, NULL,
			r->in.secdesc?&sd:NULL, (struct registry_key **)&newh->data);

		r->out.action_taken = talloc(mem_ctx, enum winreg_CreateAction);
		if (r->out.action_taken == NULL) {
			talloc_free(newh);
			return WERR_NOMEM;
		}
		*r->out.action_taken = REG_ACTION_NONE;

		if (W_ERROR_IS_OK(result)) {
			r->out.new_handle = &newh->wire_handle;
			*r->out.action_taken = REG_CREATED_NEW_KEY;
		} else {
			talloc_free(newh);
		}

		return result;
	default:
		return WERR_ACCESS_DENIED;
	}
}


/*
  winreg_DeleteKey
*/
static WERROR dcesrv_winreg_DeleteKey(struct dcesrv_call_state *dce_call,
				      TALLOC_CTX *mem_ctx,
				      struct winreg_DeleteKey *r)
{
	struct dcesrv_handle *h;
	struct registry_key *key;
	WERROR result;

	DCESRV_PULL_HANDLE_FAULT(h, r->in.handle, HTYPE_REGKEY);
	key = h->data;

	switch (security_session_user_level(dce_call->conn->auth_state.session_info, NULL))
	{
	case SECURITY_SYSTEM:
	case SECURITY_ADMINISTRATOR:
		result = reg_key_del(mem_ctx, key, r->in.key.name);
		talloc_unlink(dce_call->context, h);

		return result;
	default:
		return WERR_ACCESS_DENIED;
	}
}


/*
  winreg_DeleteValue
*/
static WERROR dcesrv_winreg_DeleteValue(struct dcesrv_call_state *dce_call,
					TALLOC_CTX *mem_ctx,
					struct winreg_DeleteValue *r)
{
	struct dcesrv_handle *h;
	struct registry_key *key;

	DCESRV_PULL_HANDLE_FAULT(h, r->in.handle, HTYPE_REGKEY);
	key = h->data;

	switch (security_session_user_level(dce_call->conn->auth_state.session_info, NULL))
	{
	case SECURITY_SYSTEM:
	case SECURITY_ADMINISTRATOR:
		return reg_del_value(mem_ctx, key, r->in.value.name);
	default:
		return WERR_ACCESS_DENIED;
	}
}


/*
  winreg_EnumKey
*/
static WERROR dcesrv_winreg_EnumKey(struct dcesrv_call_state *dce_call,
				    TALLOC_CTX *mem_ctx,
				    struct winreg_EnumKey *r)
{
	struct dcesrv_handle *h;
	struct registry_key *key;
	const char *name, *classname;
	NTTIME last_mod;
	WERROR result;

	DCESRV_PULL_HANDLE_FAULT(h, r->in.handle, HTYPE_REGKEY);
	key = h->data;

	result = reg_key_get_subkey_by_index(mem_ctx, 
		key, r->in.enum_index, &name, &classname, &last_mod);

	if (2*strlen_m_term(name) > r->in.name->size) {
		return WERR_MORE_DATA;
	}

	if (name != NULL) {
		r->out.name->name = name;
		r->out.name->length = 2*strlen_m_term(name);
	} else {
		r->out.name->name = r->in.name->name;
		r->out.name->length = r->in.name->length;
	}
	r->out.name->size = r->in.name->size;

	r->out.keyclass = r->in.keyclass;
	if (classname != NULL) {
		r->out.keyclass->name = classname;
		r->out.keyclass->length = 2*strlen_m_term(classname);
	} else {
		r->out.keyclass->name = r->in.keyclass->name;
		r->out.keyclass->length = r->in.keyclass->length;
	}
	r->out.keyclass->size = r->in.keyclass->size;

	if (r->in.last_changed_time != NULL)
		r->out.last_changed_time = &last_mod;

	return result;
}


/*
  winreg_EnumValue
*/
static WERROR dcesrv_winreg_EnumValue(struct dcesrv_call_state *dce_call,
				      TALLOC_CTX *mem_ctx,
				      struct winreg_EnumValue *r)
{
	struct dcesrv_handle *h;
	struct registry_key *key;
	const char *data_name;
	uint32_t data_type;
	DATA_BLOB data;
	WERROR result;

	DCESRV_PULL_HANDLE_FAULT(h, r->in.handle, HTYPE_REGKEY);
	key = h->data;

	result = reg_key_get_value_by_index(mem_ctx, key,
		r->in.enum_index, &data_name, &data_type, &data);

	if (!W_ERROR_IS_OK(result)) {
		/* if the lookup wasn't successful, send client query back */
		data_name = r->in.name->name;
		data_type = *r->in.type;
		data.data = r->in.value;
		data.length = *r->in.length;
	}

	/* "data_name" is NULL when we query the default attribute */
	if (data_name != NULL) {
		r->out.name->name = data_name;
		r->out.name->length = 2*strlen_m_term(data_name);
	} else {
		r->out.name->name = r->in.name->name;
		r->out.name->length = r->in.name->length;
	}
	r->out.name->size = r->in.name->size;

	r->out.type = talloc(mem_ctx, enum winreg_Type);
	if (!r->out.type) {
		return WERR_NOMEM;
	}
	*r->out.type = (enum winreg_Type) data_type;

	/* check the client has enough room for the value */
	if (r->in.value != NULL &&
	    r->in.size != NULL &&
	    data.length > *r->in.size) {
		return WERR_MORE_DATA;
	}

	if (r->in.value != NULL) {
		r->out.value = data.data;
	}

	if (r->in.size != NULL) {
		r->out.size = talloc(mem_ctx, uint32_t);
		*r->out.size = data.length;
		r->out.length = r->out.size;
	}

	return result;
}


/*
  winreg_FlushKey
*/
static WERROR dcesrv_winreg_FlushKey(struct dcesrv_call_state *dce_call,
				     TALLOC_CTX *mem_ctx,
				     struct winreg_FlushKey *r)
{
	struct dcesrv_handle *h;
	struct registry_key *key;

	DCESRV_PULL_HANDLE_FAULT(h, r->in.handle, HTYPE_REGKEY);
	key = h->data;

	switch (security_session_user_level(dce_call->conn->auth_state.session_info, NULL))
	{
	case SECURITY_SYSTEM:
	case SECURITY_ADMINISTRATOR:
		return reg_key_flush(key);
	default:
		return WERR_ACCESS_DENIED;
	}
}


/*
  winreg_GetKeySecurity
*/
static WERROR dcesrv_winreg_GetKeySecurity(struct dcesrv_call_state *dce_call,
					   TALLOC_CTX *mem_ctx,
					   struct winreg_GetKeySecurity *r)
{
	struct dcesrv_handle *h;

	DCESRV_PULL_HANDLE_FAULT(h, r->in.handle, HTYPE_REGKEY);

	return WERR_NOT_SUPPORTED;
}


/*
  winreg_LoadKey
*/
static WERROR dcesrv_winreg_LoadKey(struct dcesrv_call_state *dce_call,
				    TALLOC_CTX *mem_ctx,
				    struct winreg_LoadKey *r)
{
	return WERR_NOT_SUPPORTED;
}


/*
  winreg_NotifyChangeKeyValue
*/
static WERROR dcesrv_winreg_NotifyChangeKeyValue(struct dcesrv_call_state *dce_call,
						 TALLOC_CTX *mem_ctx,
						 struct winreg_NotifyChangeKeyValue *r)
{
	return WERR_NOT_SUPPORTED;
}


/*
  winreg_OpenKey
*/
static WERROR dcesrv_winreg_OpenKey(struct dcesrv_call_state *dce_call,
				    TALLOC_CTX *mem_ctx,
				    struct winreg_OpenKey *r)
{
	struct dcesrv_handle *h, *newh;
	struct registry_key *key;
	WERROR result;

	DCESRV_PULL_HANDLE_FAULT(h, r->in.parent_handle, HTYPE_REGKEY);
	key = h->data;

	switch (security_session_user_level(dce_call->conn->auth_state.session_info, NULL))
	{
	case SECURITY_SYSTEM:
	case SECURITY_ADMINISTRATOR:
	case SECURITY_USER:
		if (r->in.keyname.name && strcmp(r->in.keyname.name, "") == 0) {
			newh = talloc_reference(dce_call->context, h);
			result = WERR_OK;
		} else {
			newh = dcesrv_handle_new(dce_call->context, HTYPE_REGKEY);
			result = reg_open_key(newh, key, r->in.keyname.name,
				(struct registry_key **)&newh->data);
		}
		
		if (W_ERROR_IS_OK(result)) {
			r->out.handle = &newh->wire_handle;
		} else {
			talloc_free(newh);
		}
		return result;
	default:
		return WERR_ACCESS_DENIED;
	}
}


/*
  winreg_QueryInfoKey
*/
static WERROR dcesrv_winreg_QueryInfoKey(struct dcesrv_call_state *dce_call,
					 TALLOC_CTX *mem_ctx,
					 struct winreg_QueryInfoKey *r)
{
	struct dcesrv_handle *h;
	struct registry_key *key;
	const char *classname = NULL;
	WERROR result;

	DCESRV_PULL_HANDLE_FAULT(h, r->in.handle, HTYPE_REGKEY);
	key = h->data;

	switch (security_session_user_level(dce_call->conn->auth_state.session_info, NULL))
	{
	case SECURITY_SYSTEM:
	case SECURITY_ADMINISTRATOR:
	case SECURITY_USER:
		result = reg_key_get_info(mem_ctx, key, &classname,
			r->out.num_subkeys, r->out.num_values,
			r->out.last_changed_time, r->out.max_subkeylen,
			r->out.max_valnamelen, r->out.max_valbufsize);

		if (r->out.max_subkeylen != NULL) {
			/* for UTF16 encoding */
			*r->out.max_subkeylen *= 2;
		}
		if (r->out.max_valnamelen != NULL) {
			/* for UTF16 encoding */
			*r->out.max_valnamelen *= 2;
		}

		if (classname != NULL) {
			r->out.classname->name = classname;
			r->out.classname->name_len = 2*strlen_m_term(classname);
		} else {
			r->out.classname->name = r->in.classname->name;
			r->out.classname->name_len = r->in.classname->name_len;
		}
		r->out.classname->name_size = r->in.classname->name_size;

		return result;
	default:
		return WERR_ACCESS_DENIED;
	}
}


/*
  winreg_QueryValue
*/
static WERROR dcesrv_winreg_QueryValue(struct dcesrv_call_state *dce_call,
				       TALLOC_CTX *mem_ctx,
				       struct winreg_QueryValue *r)
{
	struct dcesrv_handle *h;
	struct registry_key *key;
	uint32_t value_type;
	DATA_BLOB value_data;
	WERROR result;

	DCESRV_PULL_HANDLE_FAULT(h, r->in.handle, HTYPE_REGKEY);
	key = h->data;

	switch (security_session_user_level(dce_call->conn->auth_state.session_info, NULL))
	{
	case SECURITY_SYSTEM:
	case SECURITY_ADMINISTRATOR:
	case SECURITY_USER:
		if ((r->in.type == NULL) || (r->in.data_length == NULL) ||
		    (r->in.data_size == NULL)) {
			return WERR_INVALID_PARAM;
		}

		result = reg_key_get_value_by_name(mem_ctx, key, 
			 r->in.value_name->name, &value_type, &value_data);
		
		if (!W_ERROR_IS_OK(result)) {
			/* if the lookup wasn't successful, send client query back */
			value_type = *r->in.type;
			value_data.data = r->in.data;
			value_data.length = *r->in.data_length;
		} else {
			if ((r->in.data != NULL)
			    && (*r->in.data_size < value_data.length)) {
				result = WERR_MORE_DATA;
			}
		}

		r->out.type = talloc(mem_ctx, enum winreg_Type);
		if (!r->out.type) {
			return WERR_NOMEM;
		}
		*r->out.type = (enum winreg_Type) value_type;
		r->out.data_length = talloc(mem_ctx, uint32_t);
		if (!r->out.data_length) {
			return WERR_NOMEM;
		}
		*r->out.data_length = value_data.length;
		r->out.data_size = talloc(mem_ctx, uint32_t);
		if (!r->out.data_size) {
			return WERR_NOMEM;
		}
		*r->out.data_size = value_data.length;
		r->out.data = value_data.data;

		return result;
	default:
		return WERR_ACCESS_DENIED;
	}
}


/*
  winreg_ReplaceKey
*/
static WERROR dcesrv_winreg_ReplaceKey(struct dcesrv_call_state *dce_call,
				       TALLOC_CTX *mem_ctx,
				       struct winreg_ReplaceKey *r)
{
	return WERR_NOT_SUPPORTED;
}


/*
  winreg_RestoreKey
*/
static WERROR dcesrv_winreg_RestoreKey(struct dcesrv_call_state *dce_call,
				       TALLOC_CTX *mem_ctx,
				       struct winreg_RestoreKey *r)
{
	return WERR_NOT_SUPPORTED;
}


/*
  winreg_SaveKey
*/
static WERROR dcesrv_winreg_SaveKey(struct dcesrv_call_state *dce_call,
				    TALLOC_CTX *mem_ctx,
				    struct winreg_SaveKey *r)
{
	return WERR_NOT_SUPPORTED;
}


/*
  winreg_SetKeySecurity
*/
static WERROR dcesrv_winreg_SetKeySecurity(struct dcesrv_call_state *dce_call,
					   TALLOC_CTX *mem_ctx,
					   struct winreg_SetKeySecurity *r)
{
	return WERR_NOT_SUPPORTED;
}


/*
  winreg_SetValue
*/
static WERROR dcesrv_winreg_SetValue(struct dcesrv_call_state *dce_call,
				     TALLOC_CTX *mem_ctx,
				     struct winreg_SetValue *r)
{
	struct dcesrv_handle *h;
	struct registry_key *key;
	DATA_BLOB data;
	WERROR result;

	DCESRV_PULL_HANDLE_FAULT(h, r->in.handle, HTYPE_REGKEY);
	key = h->data;

	switch (security_session_user_level(dce_call->conn->auth_state.session_info, NULL))
	{
	case SECURITY_SYSTEM:
	case SECURITY_ADMINISTRATOR:
		data.data = r->in.data;
		data.length = r->in.size;
		result = reg_val_set(key, r->in.name.name, r->in.type, data);
		return result;
	default:
		return WERR_ACCESS_DENIED;
	}
}


/*
  winreg_UnLoadKey
*/
static WERROR dcesrv_winreg_UnLoadKey(struct dcesrv_call_state *dce_call,
				      TALLOC_CTX *mem_ctx,
				      struct winreg_UnLoadKey *r)
{
	return WERR_NOT_SUPPORTED;
}


/*
  winreg_InitiateSystemShutdown
*/
static WERROR dcesrv_winreg_InitiateSystemShutdown(struct dcesrv_call_state *dce_call,
						   TALLOC_CTX *mem_ctx,
						   struct winreg_InitiateSystemShutdown *r)
{
	return WERR_NOT_SUPPORTED;
}


/*
  winreg_AbortSystemShutdown
*/
static WERROR dcesrv_winreg_AbortSystemShutdown(struct dcesrv_call_state *dce_call,
						TALLOC_CTX *mem_ctx,
						struct winreg_AbortSystemShutdown *r)
{
	return WERR_NOT_SUPPORTED;
}


/*
  winreg_GetVersion
*/
static WERROR dcesrv_winreg_GetVersion(struct dcesrv_call_state *dce_call,
				       TALLOC_CTX *mem_ctx,
				       struct winreg_GetVersion *r)
{
	struct dcesrv_handle *h;

	DCESRV_PULL_HANDLE_FAULT(h, r->in.handle, HTYPE_REGKEY);

	r->out.version = talloc(mem_ctx, uint32_t);
	W_ERROR_HAVE_NO_MEMORY(r->out.version);

	*r->out.version = 5;

	return WERR_OK;
}


/*
  winreg_QueryMultipleValues
*/
static WERROR dcesrv_winreg_QueryMultipleValues(struct dcesrv_call_state *dce_call,
						TALLOC_CTX *mem_ctx,
						struct winreg_QueryMultipleValues *r)
{
	return WERR_NOT_SUPPORTED;
}


/*
  winreg_InitiateSystemShutdownEx
*/
static WERROR dcesrv_winreg_InitiateSystemShutdownEx(struct dcesrv_call_state *dce_call,
						     TALLOC_CTX *mem_ctx,
						     struct winreg_InitiateSystemShutdownEx *r)
{
	return WERR_NOT_SUPPORTED;
}


/*
  winreg_SaveKeyEx
*/
static WERROR dcesrv_winreg_SaveKeyEx(struct dcesrv_call_state *dce_call,
				      TALLOC_CTX *mem_ctx,
				      struct winreg_SaveKeyEx *r)
{
	return WERR_NOT_SUPPORTED;
}


/*
  winreg_QueryMultipleValues2
*/
static WERROR dcesrv_winreg_QueryMultipleValues2(struct dcesrv_call_state *dce_call,
						 TALLOC_CTX *mem_ctx,
						 struct winreg_QueryMultipleValues2 *r)
{
	return WERR_NOT_SUPPORTED;
}

/*
  winreg_DeleteKeyEx
*/
static WERROR dcesrv_winreg_DeleteKeyEx(struct dcesrv_call_state *dce_call,
					TALLOC_CTX *mem_ctx,
					struct winreg_DeleteKeyEx *r)
{
	return WERR_NOT_SUPPORTED;
}

/* include the generated boilerplate */
#include "librpc/gen_ndr/ndr_winreg_s.c"
