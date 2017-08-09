/*
   Samba Unix/Linux SMB implementation
   RPC backend for the registry library
   Copyright (C) 2003-2007 Jelmer Vernooij, jelmer@samba.org
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
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include "includes.h"
#include "registry.h"
#include "librpc/gen_ndr/ndr_winreg_c.h"

#define MAX_NAMESIZE 512
#define MAX_VALSIZE 32768

struct rpc_key {
	struct registry_key key;
	struct policy_handle pol;
	struct dcerpc_binding_handle *binding_handle;
	const char* classname;	
	uint32_t num_subkeys;
	uint32_t max_subkeylen;
	uint32_t max_classlen;
	uint32_t num_values;
	uint32_t max_valnamelen;
	uint32_t max_valbufsize;
	uint32_t secdescsize;
	NTTIME last_changed_time;
};

struct rpc_registry_context {
	struct registry_context context;
	struct dcerpc_pipe *pipe;
	struct dcerpc_binding_handle *binding_handle;
};

static struct registry_operations reg_backend_rpc;

/**
 * This is the RPC backend for the registry library.
 */

#define openhive(u) static WERROR open_ ## u(struct dcerpc_binding_handle *b, TALLOC_CTX *mem_ctx, struct policy_handle *hnd) \
{ \
	struct winreg_Open ## u r; \
	NTSTATUS status; \
\
	ZERO_STRUCT(r); \
	r.in.system_name = NULL; \
	r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED; \
	r.out.handle = hnd;\
\
	status = dcerpc_winreg_Open ## u ## _r(b, mem_ctx, &r); \
\
	if (!NT_STATUS_IS_OK(status)) { \
		DEBUG(1, ("OpenHive failed - %s\n", nt_errstr(status))); \
		return ntstatus_to_werror(status); \
	} \
\
	return r.out.result;\
}

openhive(HKLM)
openhive(HKCU)
openhive(HKPD)
openhive(HKU)
openhive(HKCR)
openhive(HKDD)
openhive(HKCC)

static struct {
	uint32_t hkey;
	WERROR (*open) (struct dcerpc_binding_handle *b, TALLOC_CTX *,
			struct policy_handle *h);
} known_hives[] = {
	{ HKEY_LOCAL_MACHINE, open_HKLM },
	{ HKEY_CURRENT_USER, open_HKCU },
	{ HKEY_CLASSES_ROOT, open_HKCR },
	{ HKEY_PERFORMANCE_DATA, open_HKPD },
	{ HKEY_USERS, open_HKU },
	{ HKEY_DYN_DATA, open_HKDD },
	{ HKEY_CURRENT_CONFIG, open_HKCC },
	{ 0, NULL }
};

static WERROR rpc_query_key(TALLOC_CTX *mem_ctx, const struct registry_key *k);

static WERROR rpc_get_predefined_key(struct registry_context *ctx,
				     uint32_t hkey_type,
				     struct registry_key **k)
{
	int n;
	struct rpc_key *mykeydata;
	struct rpc_registry_context *rctx = talloc_get_type(ctx, struct rpc_registry_context);

	*k = NULL;

	for(n = 0; known_hives[n].hkey; n++) {
		if(known_hives[n].hkey == hkey_type)
			break;
	}

	if (known_hives[n].open == NULL)  {
		DEBUG(1, ("No such hive %d\n", hkey_type));
		return WERR_NO_MORE_ITEMS;
	}

	mykeydata = talloc_zero(ctx, struct rpc_key);
	W_ERROR_HAVE_NO_MEMORY(mykeydata);
	mykeydata->key.context = ctx;
	mykeydata->binding_handle = rctx->binding_handle;
	mykeydata->num_values = -1;
	mykeydata->num_subkeys = -1;
	*k = (struct registry_key *)mykeydata;
	return known_hives[n].open(mykeydata->binding_handle, mykeydata, &mykeydata->pol);
}

#if 0
static WERROR rpc_key_put_rpc_data(TALLOC_CTX *mem_ctx, struct registry_key *k)
{
	struct winreg_OpenKey r;
	struct rpc_key_data *mykeydata;

	k->backend_data = mykeydata = talloc_zero(mem_ctx, struct rpc_key_data);
	mykeydata->num_values = -1;
	mykeydata->num_subkeys = -1;

	/* Then, open the handle using the hive */

	ZERO_STRUCT(r);
	r.in.handle = &(((struct rpc_key_data *)k->hive->root->backend_data)->pol);
	r.in.keyname.name = k->path;
	r.in.unknown = 0x00000000;
	r.in.access_mask = 0x02000000;
	r.out.handle = &mykeydata->pol;

	dcerpc_winreg_OpenKey((struct dcerpc_pipe *)k->hive->backend_data,
			      mem_ctx, &r);

	return r.out.result;
}
#endif

static WERROR rpc_open_key(TALLOC_CTX *mem_ctx, struct registry_key *h,
			   const char *name, struct registry_key **key)
{
	struct rpc_key *parentkeydata = talloc_get_type(h, struct rpc_key),
						    *mykeydata;
	struct winreg_OpenKey r;
	NTSTATUS status;

	mykeydata = talloc_zero(mem_ctx, struct rpc_key);
	W_ERROR_HAVE_NO_MEMORY(mykeydata);
	mykeydata->key.context = parentkeydata->key.context;
	mykeydata->binding_handle = parentkeydata->binding_handle;
	mykeydata->num_values = -1;
	mykeydata->num_subkeys = -1;
	*key = (struct registry_key *)mykeydata;

	/* Then, open the handle using the hive */
	ZERO_STRUCT(r);
	r.in.parent_handle = &parentkeydata->pol;
	r.in.keyname.name = name;
	r.in.options = 0x00000000;
	r.in.access_mask = 0x02000000;
	r.out.handle = &mykeydata->pol;

	status = dcerpc_winreg_OpenKey_r(mykeydata->binding_handle, mem_ctx, &r);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("OpenKey failed - %s\n", nt_errstr(status)));
		return ntstatus_to_werror(status);
	}

	return r.out.result;
}

static WERROR rpc_get_value_by_index(TALLOC_CTX *mem_ctx,
				     const struct registry_key *parent,
				     uint32_t n,
				     const char **value_name,
				     uint32_t *type,
				     DATA_BLOB *data)
{
	struct rpc_key *mykeydata = talloc_get_type(parent, struct rpc_key);
	struct winreg_EnumValue r;
	struct winreg_ValNameBuf name;
	uint8_t value;
	uint32_t val_size = MAX_VALSIZE;
	uint32_t zero = 0;
	WERROR error;
	NTSTATUS status;

	if (mykeydata->num_values == -1) {
		error = rpc_query_key(mem_ctx, parent);
		if(!W_ERROR_IS_OK(error)) return error;
	}

	name.name = "";
	name.size = MAX_NAMESIZE;

	ZERO_STRUCT(r);
	r.in.handle = &mykeydata->pol;
	r.in.enum_index = n;
	r.in.name = &name;
	r.in.type = (enum winreg_Type *) type;
	r.in.value = &value;
	r.in.size = &val_size;
	r.in.length = &zero;
	r.out.name = &name;
	r.out.type = (enum winreg_Type *) type;
	r.out.value = &value;
	r.out.size = &val_size;
	r.out.length = &zero;

	status = dcerpc_winreg_EnumValue_r(mykeydata->binding_handle, mem_ctx, &r);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("EnumValue failed - %s\n", nt_errstr(status)));
		return ntstatus_to_werror(status);
	}

	*value_name = talloc_steal(mem_ctx, r.out.name->name);
	*type = *(r.out.type);
	*data = data_blob_talloc(mem_ctx, r.out.value, *r.out.length);

	return r.out.result;
}

static WERROR rpc_get_value_by_name(TALLOC_CTX *mem_ctx,
				     const struct registry_key *parent,
				     const char *value_name,
				     uint32_t *type,
				     DATA_BLOB *data)
{
	struct rpc_key *mykeydata = talloc_get_type(parent, struct rpc_key);
	struct winreg_QueryValue r;
	struct winreg_String name;
	uint8_t value;
	uint32_t val_size = MAX_VALSIZE;
	uint32_t zero = 0;
	WERROR error;
	NTSTATUS status;

	if (mykeydata->num_values == -1) {
		error = rpc_query_key(mem_ctx, parent);
		if(!W_ERROR_IS_OK(error)) return error;
	}

	name.name = value_name;

	ZERO_STRUCT(r);
	r.in.handle = &mykeydata->pol;
	r.in.value_name = &name;
	r.in.type = (enum winreg_Type *) type;
	r.in.data = &value;
	r.in.data_size = &val_size;
	r.in.data_length = &zero;
	r.out.type = (enum winreg_Type *) type;
	r.out.data = &value;
	r.out.data_size = &val_size;
	r.out.data_length = &zero;

	status = dcerpc_winreg_QueryValue_r(mykeydata->binding_handle, mem_ctx, &r);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("QueryValue failed - %s\n", nt_errstr(status)));
		return ntstatus_to_werror(status);
	}

	*type = *(r.out.type);
	*data = data_blob_talloc(mem_ctx, r.out.data, *r.out.data_length);

	return r.out.result;
}

static WERROR rpc_get_subkey_by_index(TALLOC_CTX *mem_ctx,
				      const struct registry_key *parent,
				      uint32_t n,
				      const char **name,
				      const char **keyclass,
				      NTTIME *last_changed_time)
{
	struct winreg_EnumKey r;
	struct rpc_key *mykeydata = talloc_get_type(parent, struct rpc_key);
	struct winreg_StringBuf namebuf, classbuf;
	NTTIME change_time = 0;
	NTSTATUS status;

	namebuf.name = "";
	namebuf.size = MAX_NAMESIZE;
	classbuf.name = NULL;
	classbuf.size = 0;

	ZERO_STRUCT(r);
	r.in.handle = &mykeydata->pol;
	r.in.enum_index = n;
	r.in.name = &namebuf;
	r.in.keyclass = &classbuf;
	r.in.last_changed_time = &change_time;
	r.out.name = &namebuf;
	r.out.keyclass = &classbuf;
	r.out.last_changed_time = &change_time;

	status = dcerpc_winreg_EnumKey_r(mykeydata->binding_handle, mem_ctx, &r);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("EnumKey failed - %s\n", nt_errstr(status)));
		return ntstatus_to_werror(status);
	}

	if (name != NULL)
		*name = talloc_steal(mem_ctx, r.out.name->name);
	if (keyclass != NULL)
		*keyclass = talloc_steal(mem_ctx, r.out.keyclass->name);
	if (last_changed_time != NULL)
		*last_changed_time = *(r.out.last_changed_time);

	return r.out.result;
}

static WERROR rpc_add_key(TALLOC_CTX *mem_ctx,
			  struct registry_key *parent, const char *path,
			  const char *key_class,
			  struct security_descriptor *sec,
			  struct registry_key **key)
{
	struct winreg_CreateKey r;
	struct rpc_key *parentkd = talloc_get_type(parent, struct rpc_key);
	struct rpc_key *rpck = talloc(mem_ctx, struct rpc_key);
	
	NTSTATUS status;

	ZERO_STRUCT(r);
	r.in.handle = &parentkd->pol;
	r.in.name.name = path;
	r.in.keyclass.name = NULL;
	r.in.options = 0;
	r.in.access_mask = 0x02000000;
	r.in.secdesc = NULL;
	r.in.action_taken = NULL;
	r.out.new_handle = &rpck->pol;
	r.out.action_taken = NULL;

	status = dcerpc_winreg_CreateKey_r(parentkd->binding_handle, mem_ctx, &r);

	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(rpck);
		DEBUG(1, ("CreateKey failed - %s\n", nt_errstr(status)));
		return ntstatus_to_werror(status);
	}

	rpck->binding_handle = parentkd->binding_handle;
	*key = (struct registry_key *)rpck;

	return r.out.result;
}

static WERROR rpc_query_key(TALLOC_CTX *mem_ctx, const struct registry_key *k)
{
	struct winreg_QueryInfoKey r;
	struct rpc_key *mykeydata = talloc_get_type(k, struct rpc_key);
	struct winreg_String classname;
	NTSTATUS status;

	classname.name = NULL;

	ZERO_STRUCT(r);
	r.in.handle = &mykeydata->pol;
	r.in.classname = &classname;
	r.out.classname = &classname;
	r.out.num_subkeys = &mykeydata->num_subkeys;
	r.out.max_subkeylen = &mykeydata->max_subkeylen;
	r.out.max_classlen = &mykeydata->max_classlen;
	r.out.num_values = &mykeydata->num_values;
	r.out.max_valnamelen = &mykeydata->max_valnamelen;
	r.out.max_valbufsize = &mykeydata->max_valbufsize;
	r.out.secdescsize = &mykeydata->secdescsize;
	r.out.last_changed_time = &mykeydata->last_changed_time;

	status = dcerpc_winreg_QueryInfoKey_r(mykeydata->binding_handle, mem_ctx, &r);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("QueryInfoKey failed - %s\n", nt_errstr(status)));
		return ntstatus_to_werror(status);
	}

	mykeydata->classname = talloc_steal(mem_ctx, r.out.classname->name);

	return r.out.result;
}

static WERROR rpc_del_key(TALLOC_CTX *mem_ctx, struct registry_key *parent,
			  const char *name)
{
	NTSTATUS status;
	struct rpc_key *mykeydata = talloc_get_type(parent, struct rpc_key);
	struct winreg_DeleteKey r;

	ZERO_STRUCT(r);
	r.in.handle = &mykeydata->pol;
	r.in.key.name = name;

	status = dcerpc_winreg_DeleteKey_r(mykeydata->binding_handle, mem_ctx, &r);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("DeleteKey failed - %s\n", nt_errstr(status)));
		return ntstatus_to_werror(status);
	}

	return r.out.result;
}

static WERROR rpc_get_info(TALLOC_CTX *mem_ctx, const struct registry_key *key,
						   const char **classname,
						   uint32_t *num_subkeys,
						   uint32_t *num_values,
						   NTTIME *last_changed_time,
						   uint32_t *max_subkeylen,
						   uint32_t *max_valnamelen,
						   uint32_t *max_valbufsize)
{
	struct rpc_key *mykeydata = talloc_get_type(key, struct rpc_key);
	WERROR error;

	if (mykeydata->num_values == -1) {
		error = rpc_query_key(mem_ctx, key);
		if(!W_ERROR_IS_OK(error)) return error;
	}

	if (classname != NULL)
		*classname = mykeydata->classname;

	if (num_subkeys != NULL)
		*num_subkeys = mykeydata->num_subkeys;

	if (num_values != NULL)
		*num_values = mykeydata->num_values;

	if (last_changed_time != NULL)
		*last_changed_time = mykeydata->last_changed_time;

	if (max_subkeylen != NULL)
		*max_subkeylen = mykeydata->max_subkeylen;

	if (max_valnamelen != NULL)
		*max_valnamelen = mykeydata->max_valnamelen;

	if (max_valbufsize != NULL)
		*max_valbufsize = mykeydata->max_valbufsize;

	return WERR_OK;
}

static struct registry_operations reg_backend_rpc = {
	.name = "rpc",
	.open_key = rpc_open_key,
	.get_predefined_key = rpc_get_predefined_key,
	.enum_key = rpc_get_subkey_by_index,
	.enum_value = rpc_get_value_by_index,
	.get_value = rpc_get_value_by_name,
	.create_key = rpc_add_key,
	.delete_key = rpc_del_key,
	.get_key_info = rpc_get_info,
};

_PUBLIC_ WERROR reg_open_remote(struct registry_context **ctx,
				struct auth_session_info *session_info,
				struct cli_credentials *credentials,
				struct loadparm_context *lp_ctx,
				const char *location, struct tevent_context *ev)
{
	NTSTATUS status;
	struct dcerpc_pipe *p;
	struct rpc_registry_context *rctx;

	dcerpc_init(lp_ctx);

	rctx = talloc(NULL, struct rpc_registry_context);
	W_ERROR_HAVE_NO_MEMORY(rctx);

	/* Default to local smbd if no connection is specified */
	if (!location) {
		location = talloc_strdup(rctx, "ncalrpc:");
	}

	status = dcerpc_pipe_connect(rctx /* TALLOC_CTX */,
				     &p, location,
				     &ndr_table_winreg,
				     credentials, ev, lp_ctx);
	if(NT_STATUS_IS_ERR(status)) {
		DEBUG(1, ("Unable to open '%s': %s\n", location,
			nt_errstr(status)));
		talloc_free(rctx);
		*ctx = NULL;
		return ntstatus_to_werror(status);
	}

	rctx->pipe = p;
	rctx->binding_handle = p->binding_handle;

	*ctx = (struct registry_context *)rctx;
	(*ctx)->ops = &reg_backend_rpc;

	return WERR_OK;
}
