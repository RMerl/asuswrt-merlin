/*
   Unix SMB/CIFS implementation.
   Registry interface
   Copyright (C) 2004-2007, Jelmer Vernooij, jelmer@samba.org
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
#include "registry.h"
#include "lib/ldb/include/ldb.h"
#include "lib/ldb/include/ldb_errors.h"
#include "ldb_wrap.h"
#include "librpc/gen_ndr/winreg.h"
#include "param/param.h"

static struct hive_operations reg_backend_ldb;

struct ldb_key_data
{
	struct hive_key key;
	struct ldb_context *ldb;
	struct ldb_dn *dn;
	struct ldb_message **subkeys, **values;
	int subkey_count, value_count;
};

static void reg_ldb_unpack_value(TALLOC_CTX *mem_ctx, 
				 struct ldb_message *msg,
				 const char **name, uint32_t *type,
				 DATA_BLOB *data)
{
	const struct ldb_val *val;
	uint32_t value_type;

	if (name != NULL)
		*name = talloc_strdup(mem_ctx,
				      ldb_msg_find_attr_as_string(msg, "value",
				      NULL));

	value_type = ldb_msg_find_attr_as_uint(msg, "type", 0);
	*type = value_type; 

	val = ldb_msg_find_ldb_val(msg, "data");

	switch (value_type)
	{
	case REG_SZ:
	case REG_EXPAND_SZ:
		if (val != NULL)
			convert_string_talloc(mem_ctx, CH_UTF8, CH_UTF16,
						     val->data, val->length,
						     (void **)&data->data, &data->length, false);
		else {
			data->data = NULL;
			data->length = 0;
		}
		break;

	case REG_BINARY:
		if (val != NULL)
			*data = data_blob_talloc(mem_ctx, val->data, val->length);
		else {
			data->data = NULL;
			data->length = 0;
		}
		break;

	case REG_DWORD: {
		uint32_t tmp = strtoul((char *)val->data, NULL, 0);
		*data = data_blob_talloc(mem_ctx, &tmp, 4);
		}
		break;

	default:
		*data = data_blob_talloc(mem_ctx, val->data, val->length);
		break;
	}
}

static struct ldb_message *reg_ldb_pack_value(struct ldb_context *ctx,
					      TALLOC_CTX *mem_ctx,
					      const char *name,
					      uint32_t type, DATA_BLOB data)
{
	struct ldb_val val;
	struct ldb_message *msg = talloc_zero(mem_ctx, struct ldb_message);
	char *type_s;

	ldb_msg_add_string(msg, "value", talloc_strdup(mem_ctx, name));

	switch (type) {
	case REG_SZ:
	case REG_EXPAND_SZ:
		if (data.data[0] != '\0') {
			convert_string_talloc(mem_ctx, CH_UTF16, CH_UTF8,
						   (void *)data.data,
						   data.length,
						   (void **)&val.data, &val.length, false);
			ldb_msg_add_value(msg, "data", &val, NULL);
		} else {
			ldb_msg_add_empty(msg, "data", LDB_FLAG_MOD_DELETE, NULL);
		}
		break;

	case REG_BINARY:
		if (data.length > 0)
			ldb_msg_add_value(msg, "data", &data, NULL);
		else
			ldb_msg_add_empty(msg, "data", LDB_FLAG_MOD_DELETE, NULL);
		break;

	case REG_DWORD:
		ldb_msg_add_string(msg, "data",
				   talloc_asprintf(mem_ctx, "0x%x",
				   		   IVAL(data.data, 0)));
		break;
	default:
		ldb_msg_add_value(msg, "data", &data, NULL);
	}


	type_s = talloc_asprintf(mem_ctx, "%u", type);
	ldb_msg_add_string(msg, "type", type_s);

	return msg;
}

static char *reg_ldb_escape(TALLOC_CTX *mem_ctx, const char *value)
{
	struct ldb_val val;

	val.data = discard_const_p(uint8_t, value);
	val.length = strlen(value);

	return ldb_dn_escape_value(mem_ctx, val);
}

static int reg_close_ldb_key(struct ldb_key_data *key)
{
	if (key->subkeys != NULL) {
		talloc_free(key->subkeys);
		key->subkeys = NULL;
	}

	if (key->values != NULL) {
		talloc_free(key->values);
		key->values = NULL;
	}
	return 0;
}

static struct ldb_dn *reg_path_to_ldb(TALLOC_CTX *mem_ctx,
				      const struct hive_key *from,
				      const char *path, const char *add)
{
	TALLOC_CTX *local_ctx;
	struct ldb_dn *ret;
	char *mypath = talloc_strdup(mem_ctx, path);
	char *begin;
	struct ldb_key_data *kd = talloc_get_type(from, struct ldb_key_data);
	struct ldb_context *ldb = kd->ldb;

	local_ctx = talloc_new(mem_ctx);

	if (add) {
		ret = ldb_dn_new(mem_ctx, ldb, add);
	} else {
		ret = ldb_dn_new(mem_ctx, ldb, NULL);
	}
	if (!ldb_dn_validate(ret)) {
		talloc_free(ret);
		talloc_free(local_ctx);
		return NULL;
	}

	while (mypath) {
		char *keyname;

		begin = strrchr(mypath, '\\');

		if (begin) keyname = begin + 1;
		else keyname = mypath;

		if(strlen(keyname)) {
			if (!ldb_dn_add_base_fmt(ret, "key=%s",
						 reg_ldb_escape(local_ctx,
								keyname)))
			{
				talloc_free(local_ctx);
				return NULL;
			}
		}

		if(begin) {
			*begin = '\0';
		} else {
			break;
		}
	}

	ldb_dn_add_base(ret, kd->dn);

	talloc_free(local_ctx);

	return ret;
}

static WERROR cache_subkeys(struct ldb_key_data *kd)
{
	struct ldb_context *c = kd->ldb;
	struct ldb_result *res;
	int ret;

	ret = ldb_search(c, c, &res, kd->dn, LDB_SCOPE_ONELEVEL, NULL, "(key=*)");

	if (ret != LDB_SUCCESS) {
		DEBUG(0, ("Error getting subkeys for '%s': %s\n",
			ldb_dn_get_linearized(kd->dn), ldb_errstring(c)));
		return WERR_FOOBAR;
	}

	kd->subkey_count = res->count;
	kd->subkeys = talloc_steal(kd, res->msgs);
	talloc_free(res);

	return WERR_OK;
}

static WERROR cache_values(struct ldb_key_data *kd)
{
	struct ldb_context *c = kd->ldb;
	struct ldb_result *res;
	int ret;

	ret = ldb_search(c, c, &res, kd->dn, LDB_SCOPE_ONELEVEL,
			 NULL, "(value=*)");

	if (ret != LDB_SUCCESS) {
		DEBUG(0, ("Error getting values for '%s': %s\n",
			ldb_dn_get_linearized(kd->dn), ldb_errstring(c)));
		return WERR_FOOBAR;
	}

	kd->value_count = res->count;
	kd->values = talloc_steal(kd, res->msgs);
	talloc_free(res);

	return WERR_OK;
}


static WERROR ldb_get_subkey_by_id(TALLOC_CTX *mem_ctx,
				   const struct hive_key *k, uint32_t idx,
				   const char **name,
				   const char **classname,
				   NTTIME *last_mod_time)
{
	struct ldb_message_element *el;
	struct ldb_key_data *kd = talloc_get_type(k, struct ldb_key_data);
	
	/* Initialization */
	if (name != NULL)
		*name = NULL;
	if (classname != NULL)
		*classname = NULL; /* TODO: Store properly */
	if (last_mod_time != NULL)
		*last_mod_time = 0; /* TODO: we need to add this to the
						ldb backend properly */

	/* Do a search if necessary */
	if (kd->subkeys == NULL) {
		W_ERROR_NOT_OK_RETURN(cache_subkeys(kd));
	}

	if (idx >= kd->subkey_count)
		return WERR_NO_MORE_ITEMS;

	el = ldb_msg_find_element(kd->subkeys[idx], "key");
	SMB_ASSERT(el != NULL);
	SMB_ASSERT(el->num_values != 0);

	if (name != NULL)
		*name = talloc_strdup(mem_ctx, (char *)el->values[0].data);

	return WERR_OK;
}

static WERROR ldb_get_default_value(TALLOC_CTX *mem_ctx, struct hive_key *k,
				  const char **name, uint32_t *data_type,
				   DATA_BLOB *data)
{
	struct ldb_key_data *kd = talloc_get_type(k, struct ldb_key_data);
	struct ldb_context *c = kd->ldb;
	const char* attrs[] = { "data", "type", NULL };
	struct ldb_result *res;
	int ret;

	ret = ldb_search(c, mem_ctx, &res, kd->dn, LDB_SCOPE_BASE, attrs, "%s", "");

	if (ret != LDB_SUCCESS) {
		DEBUG(0, ("Error getting default value for '%s': %s\n",
			ldb_dn_get_linearized(kd->dn), ldb_errstring(c)));
		return WERR_FOOBAR;
	}

	if (res->count == 0 || res->msgs[0]->num_elements == 0)
		return WERR_BADFILE;

	reg_ldb_unpack_value(mem_ctx, 
		 res->msgs[0], name, data_type, data);

	talloc_free(res);

	return WERR_OK;
}

static WERROR ldb_get_value_by_id(TALLOC_CTX *mem_ctx, struct hive_key *k,
				  int idx, const char **name,
				  uint32_t *data_type, DATA_BLOB *data)
{
	struct ldb_key_data *kd = talloc_get_type(k, struct ldb_key_data);

	/* if default value exists, give it back */
	if (W_ERROR_IS_OK(ldb_get_default_value(mem_ctx, k, name, data_type,
		data))) {
		if (idx == 0)
			return WERR_OK;
		else
			--idx;
	}

	/* Do the search if necessary */
	if (kd->values == NULL) {
		W_ERROR_NOT_OK_RETURN(cache_values(kd));
	}

	if (idx >= kd->value_count)
		return WERR_NO_MORE_ITEMS;

	reg_ldb_unpack_value(mem_ctx, kd->values[idx], name, data_type, data);

	return WERR_OK;
}

static WERROR ldb_get_value(TALLOC_CTX *mem_ctx, struct hive_key *k,
			    const char *name, uint32_t *data_type,
			    DATA_BLOB *data)
{
	struct ldb_key_data *kd = talloc_get_type(k, struct ldb_key_data);
	struct ldb_context *c = kd->ldb;
	struct ldb_result *res;
	int ret;
	char *query;

	if (strlen(name) == 0) {
		/* default value */
		return ldb_get_default_value(mem_ctx, k, NULL, data_type, data);
	} else {
		/* normal value */
		query = talloc_asprintf(mem_ctx, "(value=%s)", name);
		ret = ldb_search(c, mem_ctx, &res, kd->dn, LDB_SCOPE_ONELEVEL, NULL, "%s", query);
		talloc_free(query);

		if (ret != LDB_SUCCESS) {
			DEBUG(0, ("Error getting values for '%s': %s\n",
				ldb_dn_get_linearized(kd->dn), ldb_errstring(c)));
			return WERR_FOOBAR;
		}

		if (res->count == 0)
			return WERR_BADFILE;

		reg_ldb_unpack_value(mem_ctx, res->msgs[0], NULL, data_type, data);

		talloc_free(res);
	}

	return WERR_OK;
}

static WERROR ldb_open_key(TALLOC_CTX *mem_ctx, const struct hive_key *h,
			   const char *name, struct hive_key **key)
{
	struct ldb_result *res;
	struct ldb_dn *ldap_path;
	int ret;
	struct ldb_key_data *newkd;
	struct ldb_key_data *kd = talloc_get_type(h, struct ldb_key_data);
	struct ldb_context *c = kd->ldb;

	ldap_path = reg_path_to_ldb(mem_ctx, h, name, NULL);

	ret = ldb_search(c, mem_ctx, &res, ldap_path, LDB_SCOPE_BASE, NULL, "(key=*)");

	if (ret != LDB_SUCCESS) {
		DEBUG(3, ("Error opening key '%s': %s\n",
			ldb_dn_get_linearized(ldap_path), ldb_errstring(c)));
		return WERR_FOOBAR;
	} else if (res->count == 0) {
		DEBUG(3, ("Key '%s' not found\n",
			ldb_dn_get_linearized(ldap_path)));
		talloc_free(res);
		return WERR_BADFILE;
	}

	newkd = talloc_zero(mem_ctx, struct ldb_key_data);
	newkd->key.ops = &reg_backend_ldb;
	newkd->ldb = talloc_reference(newkd, kd->ldb);
	newkd->dn = ldb_dn_copy(mem_ctx, res->msgs[0]->dn);

	*key = (struct hive_key *)newkd;

	return WERR_OK;
}

WERROR reg_open_ldb_file(TALLOC_CTX *parent_ctx, const char *location,
			 struct auth_session_info *session_info,
			 struct cli_credentials *credentials,
			 struct tevent_context *ev_ctx,
			 struct loadparm_context *lp_ctx,
			 struct hive_key **k)
{
	struct ldb_key_data *kd;
	struct ldb_context *wrap;
	struct ldb_message *attrs_msg;

	if (location == NULL)
		return WERR_INVALID_PARAM;

	wrap = ldb_wrap_connect(parent_ctx, ev_ctx, lp_ctx,
				location, session_info, credentials, 0, NULL);

	if (wrap == NULL) {
		DEBUG(1, (__FILE__": unable to connect\n"));
		return WERR_FOOBAR;
	}

	attrs_msg = ldb_msg_new(wrap);
	W_ERROR_HAVE_NO_MEMORY(attrs_msg);
	attrs_msg->dn = ldb_dn_new(attrs_msg, wrap, "@ATTRIBUTES");
	W_ERROR_HAVE_NO_MEMORY(attrs_msg->dn);
	ldb_msg_add_string(attrs_msg, "key", "CASE_INSENSITIVE");
	ldb_msg_add_string(attrs_msg, "value", "CASE_INSENSITIVE");

	ldb_add(wrap, attrs_msg);

	ldb_set_debug_stderr(wrap);

	kd = talloc_zero(parent_ctx, struct ldb_key_data);
	kd->key.ops = &reg_backend_ldb;
	kd->ldb = talloc_reference(kd, wrap);
	talloc_set_destructor (kd, reg_close_ldb_key);
	kd->dn = ldb_dn_new(kd, wrap, "hive=NONE");

	*k = (struct hive_key *)kd;

	return WERR_OK;
}

static WERROR ldb_add_key(TALLOC_CTX *mem_ctx, const struct hive_key *parent,
			  const char *name, const char *classname,
			  struct security_descriptor *sd,
			  struct hive_key **newkey)
{
	struct ldb_key_data *parentkd = discard_const_p(struct ldb_key_data, parent);
	struct ldb_message *msg;
	struct ldb_key_data *newkd;
	int ret;

	msg = ldb_msg_new(mem_ctx);

	msg->dn = reg_path_to_ldb(msg, parent, name, NULL);

	ldb_msg_add_string(msg, "key", talloc_strdup(mem_ctx, name));
	if (classname != NULL)
		ldb_msg_add_string(msg, "classname",
				   talloc_strdup(mem_ctx, classname));

	ret = ldb_add(parentkd->ldb, msg);
	if (ret == LDB_ERR_ENTRY_ALREADY_EXISTS) {
		return WERR_ALREADY_EXISTS;
	}

	if (ret != LDB_SUCCESS) {
		DEBUG(1, ("ldb_add: %s\n", ldb_errstring(parentkd->ldb)));
		return WERR_FOOBAR;
	}

	DEBUG(2, ("key added: %s\n", ldb_dn_get_linearized(msg->dn)));

	newkd = talloc_zero(mem_ctx, struct ldb_key_data);
	newkd->ldb = talloc_reference(newkd, parentkd->ldb);
	newkd->key.ops = &reg_backend_ldb;
	newkd->dn = talloc_steal(newkd, msg->dn);

	*newkey = (struct hive_key *)newkd;

	/* reset cache */
	talloc_free(parentkd->subkeys);
	parentkd->subkeys = NULL;

	return WERR_OK;
}

static WERROR ldb_del_value (struct hive_key *key, const char *child)
{
	int ret;
	struct ldb_key_data *kd = talloc_get_type(key, struct ldb_key_data);
	TALLOC_CTX *mem_ctx;
	struct ldb_message *msg;
	struct ldb_dn *childdn;

	if (strlen(child) == 0) {
		/* default value */
		mem_ctx = talloc_init("ldb_del_value");

		msg = talloc_zero(mem_ctx, struct ldb_message);
		msg->dn = ldb_dn_copy(msg, kd->dn);
		ldb_msg_add_empty(msg, "data", LDB_FLAG_MOD_DELETE, NULL);
		ldb_msg_add_empty(msg, "type", LDB_FLAG_MOD_DELETE, NULL);

		ret = ldb_modify(kd->ldb, msg);
		if (ret != LDB_SUCCESS) {
			DEBUG(1, ("ldb_del_value: %s\n", ldb_errstring(kd->ldb)));
			talloc_free(mem_ctx);
			return WERR_FOOBAR;
		}

		talloc_free(mem_ctx);
	} else {
		/* normal value */
		childdn = ldb_dn_copy(kd->ldb, kd->dn);
		if (!ldb_dn_add_child_fmt(childdn, "value=%s",
				  reg_ldb_escape(childdn, child)))
		{
			talloc_free(childdn);
			return WERR_FOOBAR;
		}

		ret = ldb_delete(kd->ldb, childdn);

		talloc_free(childdn);

		if (ret == LDB_ERR_NO_SUCH_OBJECT) {
			return WERR_BADFILE;
		} else if (ret != LDB_SUCCESS) {
			DEBUG(1, ("ldb_del_value: %s\n", ldb_errstring(kd->ldb)));
			return WERR_FOOBAR;
		}
	}

	/* reset cache */
	talloc_free(kd->values);
	kd->values = NULL;

	return WERR_OK;
}

static WERROR ldb_del_key(const struct hive_key *key, const char *name)
{
	int i, ret;
	struct ldb_key_data *parentkd = talloc_get_type(key, struct ldb_key_data);
	struct ldb_dn *ldap_path;
	TALLOC_CTX *mem_ctx = talloc_init("ldb_del_key");
	struct ldb_context *c = parentkd->ldb;
	struct ldb_result *res_keys;
	struct ldb_result *res_vals;
	WERROR werr;
	struct hive_key *hk;

	/* Verify key exists by opening it */
	werr = ldb_open_key(mem_ctx, key, name, &hk);
	if (!W_ERROR_IS_OK(werr)) {
		talloc_free(mem_ctx);
		return werr;
	}

	ldap_path = reg_path_to_ldb(mem_ctx, key, name, NULL);
	if (!ldap_path) {
		talloc_free(mem_ctx);
		return WERR_FOOBAR;
	}

	/* Search for subkeys */
	ret = ldb_search(c, mem_ctx, &res_keys, ldap_path, LDB_SCOPE_ONELEVEL,
			 NULL, "(key=*)");

	if (ret != LDB_SUCCESS) {
		DEBUG(0, ("Error getting subkeys for '%s': %s\n",
		      ldb_dn_get_linearized(ldap_path), ldb_errstring(c)));
		talloc_free(mem_ctx);
		return WERR_FOOBAR;
	}

	/* Search for values */
	ret = ldb_search(c, mem_ctx, &res_vals, ldap_path, LDB_SCOPE_ONELEVEL,
			 NULL, "(value=*)");

	if (ret != LDB_SUCCESS) {
		DEBUG(0, ("Error getting values for '%s': %s\n",
		      ldb_dn_get_linearized(ldap_path), ldb_errstring(c)));
		talloc_free(mem_ctx);
		return WERR_FOOBAR;
	}

	/* Start an explicit transaction */
	ret = ldb_transaction_start(c);

	if (ret != LDB_SUCCESS) {
		DEBUG(0, ("ldb_transaction_start: %s\n", ldb_errstring(c)));
		talloc_free(mem_ctx);
		return WERR_FOOBAR;
	}

	if (res_keys->count || res_vals->count)
	{
		/* Delete any subkeys */
		for (i = 0; i < res_keys->count; i++)
		{
			werr = ldb_del_key(hk, ldb_msg_find_attr_as_string(
							res_keys->msgs[i],
							"key", NULL));
			if (!W_ERROR_IS_OK(werr)) {
				ret = ldb_transaction_cancel(c);
				talloc_free(mem_ctx);
				return werr;
			}
		}

		/* Delete any values */
		for (i = 0; i < res_vals->count; i++)
		{
			werr = ldb_del_value(hk, ldb_msg_find_attr_as_string(
							res_vals->msgs[i],
							"value", NULL));
			if (!W_ERROR_IS_OK(werr)) {
				ret = ldb_transaction_cancel(c);
				talloc_free(mem_ctx);
				return werr;
			}
		}
	}

	/* Delete the key itself */
	ret = ldb_delete(c, ldap_path);

	if (ret != LDB_SUCCESS)
	{
		DEBUG(1, ("ldb_del_key: %s\n", ldb_errstring(c)));
		ret = ldb_transaction_cancel(c);
		talloc_free(mem_ctx);
		return WERR_FOOBAR;
	}

	/* Commit the transaction */
	ret = ldb_transaction_commit(c);

	if (ret != LDB_SUCCESS)
	{
		DEBUG(0, ("ldb_transaction_commit: %s\n", ldb_errstring(c)));
		ret = ldb_transaction_cancel(c);
		talloc_free(mem_ctx);
		return WERR_FOOBAR;
	}

	talloc_free(mem_ctx);

	/* reset cache */
	talloc_free(parentkd->subkeys);
	parentkd->subkeys = NULL;

	return WERR_OK;
}

static WERROR ldb_set_value(struct hive_key *parent,
			    const char *name, uint32_t type,
			    const DATA_BLOB data)
{
	struct ldb_message *msg;
	struct ldb_key_data *kd = talloc_get_type(parent, struct ldb_key_data);
	int ret;
	TALLOC_CTX *mem_ctx = talloc_init("ldb_set_value");

	msg = reg_ldb_pack_value(kd->ldb, mem_ctx, name, type, data);
	msg->dn = ldb_dn_copy(msg, kd->dn);
	
	if (strlen(name) > 0) {
		/* For a default value, we add/overwrite the attributes to/of the hive.
 		   For a normal value, we create new childs. */
		if (!ldb_dn_add_child_fmt(msg->dn, "value=%s",
				  reg_ldb_escape(mem_ctx, name)))
		{
			talloc_free(mem_ctx);
			return WERR_FOOBAR;
		}
	}

	ret = ldb_add(kd->ldb, msg);
	if (ret == LDB_ERR_ENTRY_ALREADY_EXISTS) {
		int i;
		for (i = 0; i < msg->num_elements; i++) {
			if (msg->elements[i].flags != LDB_FLAG_MOD_DELETE)
				msg->elements[i].flags = LDB_FLAG_MOD_REPLACE;
		}
		ret = ldb_modify(kd->ldb, msg);
	}

	if (ret != LDB_SUCCESS) {
		DEBUG(1, ("ldb_set_value: %s\n", ldb_errstring(kd->ldb)));
		talloc_free(mem_ctx);
		return WERR_FOOBAR;
	}

	/* reset cache */
	talloc_free(kd->values);
	kd->values = NULL;

	talloc_free(mem_ctx);
	return WERR_OK;
}

static WERROR ldb_get_key_info(TALLOC_CTX *mem_ctx,
			       const struct hive_key *key,
			       const char **classname,
			       uint32_t *num_subkeys,
			       uint32_t *num_values,
			       NTTIME *last_change_time,
			       uint32_t *max_subkeynamelen,
			       uint32_t *max_valnamelen,
			       uint32_t *max_valbufsize)
{
	struct ldb_key_data *kd = talloc_get_type(key, struct ldb_key_data);

	/* Initialization */
	if (classname != NULL)
		*classname = NULL;
	if (num_subkeys != NULL)
		*num_subkeys = 0;
	if (num_values != NULL)
		*num_values = 0;
	if (last_change_time != NULL)
		*last_change_time = 0;
	if (max_subkeynamelen != NULL)
		*max_subkeynamelen = 0;
	if (max_valnamelen != NULL)
		*max_valnamelen = 0;
	if (max_valbufsize != NULL)
		*max_valbufsize = 0;

	if (kd->subkeys == NULL) {
		W_ERROR_NOT_OK_RETURN(cache_subkeys(kd));
	}

	if (kd->values == NULL) {
		W_ERROR_NOT_OK_RETURN(cache_values(kd));
	}

	if (num_subkeys != NULL) {
		*num_subkeys = kd->subkey_count;
	}
	if (num_values != NULL) {
		*num_values = kd->value_count;
	}


	if (max_subkeynamelen != NULL) {
		int i;
		struct ldb_message_element *el;

		*max_subkeynamelen = 0;

		for (i = 0; i < kd->subkey_count; i++) {
			el = ldb_msg_find_element(kd->subkeys[i], "key");
			*max_subkeynamelen = MAX(*max_subkeynamelen, el->values[0].length);
		}
	}

	if (max_valnamelen != NULL || max_valbufsize != NULL) {
		int i;
		struct ldb_message_element *el;
		W_ERROR_NOT_OK_RETURN(cache_values(kd));

		if (max_valbufsize != NULL)
			*max_valbufsize = 0;

		if (max_valnamelen != NULL)
			*max_valnamelen = 0;

		for (i = 0; i < kd->value_count; i++) {
			if (max_valnamelen != NULL) {
				el = ldb_msg_find_element(kd->values[i], "value");
				*max_valnamelen = MAX(*max_valnamelen, el->values[0].length);
			}

			if (max_valbufsize != NULL) {
				uint32_t data_type;
				DATA_BLOB data;
				reg_ldb_unpack_value(mem_ctx, 
						     kd->values[i], NULL, 
						     &data_type, &data);
				*max_valbufsize = MAX(*max_valbufsize, data.length);
				talloc_free(data.data);
			}
		}
	}

	return WERR_OK;
}

static struct hive_operations reg_backend_ldb = {
	.name = "ldb",
	.add_key = ldb_add_key,
	.del_key = ldb_del_key,
	.get_key_by_name = ldb_open_key,
	.enum_value = ldb_get_value_by_id,
	.enum_key = ldb_get_subkey_by_id,
	.set_value = ldb_set_value,
	.get_value_by_name = ldb_get_value,
	.delete_value = ldb_del_value,
	.get_key_info = ldb_get_key_info,
};
