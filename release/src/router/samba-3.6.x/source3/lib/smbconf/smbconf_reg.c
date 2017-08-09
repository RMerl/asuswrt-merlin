/*
 *  Unix SMB/CIFS implementation.
 *  libsmbconf - Samba configuration library, registry backend
 *  Copyright (C) Michael Adam 2008
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
#include "lib/smbconf/smbconf_private.h"
#include "registry.h"
#include "registry/reg_api.h"
#include "registry/reg_backend_db.h"
#include "registry/reg_util_token.h"
#include "registry/reg_api_util.h"
#include "registry/reg_init_smbconf.h"
#include "lib/smbconf/smbconf_init.h"
#include "lib/smbconf/smbconf_reg.h"
#include "../libcli/registry/util_reg.h"

#define INCLUDES_VALNAME "includes"

struct reg_private_data {
	struct registry_key *base_key;
	bool open;		/* did _we_ open the registry? */
};

/**********************************************************************
 *
 * helper functions
 *
 **********************************************************************/

/**
 * a convenience helper to cast the private data structure
 */
static struct reg_private_data *rpd(struct smbconf_ctx *ctx)
{
	return (struct reg_private_data *)(ctx->data);
}

/*
 * check whether a given value name is forbidden in registry (smbconf)
 */
static bool smbconf_reg_valname_forbidden(const char *valname)
{
	/* hard code the list of forbidden names here for now */
	const char *forbidden_valnames[] = {
		"lock directory",
		"lock dir",
		"config backend",
		"include",
		"includes", /* this has a special meaning internally */
		NULL
	};
	const char **forbidden = NULL;

	for (forbidden = forbidden_valnames; *forbidden != NULL; forbidden++) {
		if (strwicmp(valname, *forbidden) == 0) {
			return true;
		}
	}
	return false;
}

static bool smbconf_reg_valname_valid(const char *valname)
{
	return (!smbconf_reg_valname_forbidden(valname) &&
		lp_parameter_is_valid(valname));
}

/**
 * Open a subkey of the base key (i.e a service)
 */
static sbcErr smbconf_reg_open_service_key(TALLOC_CTX *mem_ctx,
					   struct smbconf_ctx *ctx,
					   const char *servicename,
					   uint32 desired_access,
					   struct registry_key **key)
{
	WERROR werr;

	if (servicename == NULL) {
		*key = rpd(ctx)->base_key;
		return SBC_ERR_OK;
	}
	werr = reg_openkey(mem_ctx, rpd(ctx)->base_key, servicename,
			   desired_access, key);
	if (W_ERROR_EQUAL(werr, WERR_BADFILE)) {
		return SBC_ERR_NO_SUCH_SERVICE;
	}
	if (!W_ERROR_IS_OK(werr)) {
		return SBC_ERR_NOMEM;
	}

	return SBC_ERR_OK;
}

/**
 * check if a value exists in a given registry key
 */
static bool smbconf_value_exists(struct registry_key *key, const char *param)
{
	bool ret = false;
	WERROR werr = WERR_OK;
	TALLOC_CTX *ctx = talloc_stackframe();
	struct registry_value *value = NULL;

	werr = reg_queryvalue(ctx, key, param, &value);
	if (W_ERROR_IS_OK(werr)) {
		ret = true;
	}

	talloc_free(ctx);
	return ret;
}

/**
 * create a subkey of the base key (i.e. a service...)
 */
static sbcErr smbconf_reg_create_service_key(TALLOC_CTX *mem_ctx,
					     struct smbconf_ctx *ctx,
					     const char * subkeyname,
					     struct registry_key **newkey)
{
	WERROR werr;
	sbcErr err = SBC_ERR_OK;
	TALLOC_CTX *create_ctx;
	enum winreg_CreateAction action = REG_ACTION_NONE;

	/* create a new talloc ctx for creation. it will hold
	 * the intermediate parent key (SMBCONF) for creation
	 * and will be destroyed when leaving this function... */
	create_ctx = talloc_stackframe();

	werr = reg_createkey(mem_ctx, rpd(ctx)->base_key, subkeyname,
			     REG_KEY_WRITE, newkey, &action);
	if (W_ERROR_IS_OK(werr) && (action != REG_CREATED_NEW_KEY)) {
		DEBUG(10, ("Key '%s' already exists.\n", subkeyname));
		err = SBC_ERR_FILE_EXISTS;
	}
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(5, ("Error creating key %s: %s\n",
			 subkeyname, win_errstr(werr)));
		err = SBC_ERR_UNKNOWN_FAILURE;
	}

	talloc_free(create_ctx);
	return err;
}

/**
 * add a value to a key.
 */
static sbcErr smbconf_reg_set_value(struct registry_key *key,
				    const char *valname,
				    const char *valstr)
{
	struct registry_value val;
	WERROR werr = WERR_OK;
	sbcErr err;
	char *subkeyname;
	const char *canon_valname;
	const char *canon_valstr;

	if (!lp_canonicalize_parameter_with_value(valname, valstr,
						  &canon_valname,
						  &canon_valstr))
	{
		if (canon_valname == NULL) {
			DEBUG(5, ("invalid parameter '%s' given\n",
				  valname));
		} else {
			DEBUG(5, ("invalid value '%s' given for "
				  "parameter '%s'\n", valstr, valname));
		}
		err = SBC_ERR_INVALID_PARAM;
		goto done;
	}

	if (smbconf_reg_valname_forbidden(canon_valname)) {
		DEBUG(5, ("Parameter '%s' not allowed in registry.\n",
			  canon_valname));
		err = SBC_ERR_INVALID_PARAM;
		goto done;
	}

	subkeyname = strrchr_m(key->key->name, '\\');
	if ((subkeyname == NULL) || (*(subkeyname +1) == '\0')) {
		DEBUG(5, ("Invalid registry key '%s' given as "
			  "smbconf section.\n", key->key->name));
		err = SBC_ERR_INVALID_PARAM;
		goto done;
	}
	subkeyname++;
	if (!strequal(subkeyname, GLOBAL_NAME) &&
	    lp_parameter_is_global(valname))
	{
		DEBUG(5, ("Global parameter '%s' not allowed in "
			  "service definition ('%s').\n", canon_valname,
			  subkeyname));
		err = SBC_ERR_INVALID_PARAM;
		goto done;
	}

	ZERO_STRUCT(val);

	val.type = REG_SZ;
	if (!push_reg_sz(talloc_tos(), &val.data, canon_valstr)) {
		err = SBC_ERR_NOMEM;
		goto done;
	}

	werr = reg_setvalue(key, canon_valname, &val);
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(5, ("Error adding value '%s' to "
			  "key '%s': %s\n",
			  canon_valname, key->key->name, win_errstr(werr)));
		err = SBC_ERR_NOMEM;
		goto done;
	}

	err = SBC_ERR_OK;
done:
	return err;
}

static sbcErr smbconf_reg_set_multi_sz_value(struct registry_key *key,
					     const char *valname,
					     const uint32_t num_strings,
					     const char **strings)
{
	WERROR werr;
	sbcErr err = SBC_ERR_OK;
	struct registry_value *value;
	uint32_t count;
	TALLOC_CTX *tmp_ctx = talloc_stackframe();
	const char **array;

	if (strings == NULL) {
		err = SBC_ERR_INVALID_PARAM;
		goto done;
	}

	array = talloc_zero_array(tmp_ctx, const char *, num_strings + 1);
	if (array == NULL) {
		err = SBC_ERR_NOMEM;
		goto done;
	}

	value = TALLOC_ZERO_P(tmp_ctx, struct registry_value);
	if (value == NULL) {
		err = SBC_ERR_NOMEM;
		goto done;
	}

	value->type = REG_MULTI_SZ;

	for (count = 0; count < num_strings; count++) {
		array[count] = talloc_strdup(value, strings[count]);
		if (array[count] == NULL) {
			err = SBC_ERR_NOMEM;
			goto done;
		}
	}

	if (!push_reg_multi_sz(value, &value->data, array)) {
		err = SBC_ERR_NOMEM;
		goto done;
	}

	werr = reg_setvalue(key, valname, value);
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(5, ("Error adding value '%s' to key '%s': %s\n",
			  valname, key->key->name, win_errstr(werr)));
		err = SBC_ERR_ACCESS_DENIED;
	}

done:
	talloc_free(tmp_ctx);
	return err;
}

/**
 * format a registry_value into a string.
 *
 * This is intended to be used for smbconf registry values,
 * which are ar stored as REG_SZ values, so the incomplete
 * handling should be ok.
 */
static char *smbconf_format_registry_value(TALLOC_CTX *mem_ctx,
					   struct registry_value *value)
{
	char *result = NULL;

	/* alternatively, create a new talloc context? */
	if (mem_ctx == NULL) {
		return result;
	}

	switch (value->type) {
	case REG_DWORD:
		if (value->data.length >= 4) {
			uint32_t v = IVAL(value->data.data, 0);
			result = talloc_asprintf(mem_ctx, "%d", v);
		}
		break;
	case REG_SZ:
	case REG_EXPAND_SZ: {
		const char *s;
		if (!pull_reg_sz(mem_ctx, &value->data, &s)) {
			break;
		}
		result = talloc_strdup(mem_ctx, s);
		break;
	}
	case REG_MULTI_SZ: {
		uint32 j;
		const char **a = NULL;
		if (!pull_reg_multi_sz(mem_ctx, &value->data, &a)) {
			break;
		}
		for (j = 0; a[j] != NULL; j++) {
			result = talloc_asprintf(mem_ctx, "%s\"%s\" ",
						 result ? result : "" ,
						 a[j]);
			if (result == NULL) {
				break;
			}
		}
		break;
	}
	case REG_BINARY:
		result = talloc_asprintf(mem_ctx, "binary (%d bytes)",
					 (int)value->data.length);
		break;
	default:
		result = talloc_asprintf(mem_ctx, "<unprintable>");
		break;
	}
	return result;
}

static sbcErr smbconf_reg_get_includes_internal(TALLOC_CTX *mem_ctx,
						struct registry_key *key,
						uint32_t *num_includes,
						char ***includes)
{
	WERROR werr;
	sbcErr err;
	uint32_t count;
	struct registry_value *value = NULL;
	char **tmp_includes = NULL;
	const char **array = NULL;
	TALLOC_CTX *tmp_ctx = talloc_stackframe();

	if (!smbconf_value_exists(key, INCLUDES_VALNAME)) {
		/* no includes */
		*num_includes = 0;
		*includes = NULL;
		err = SBC_ERR_OK;
		goto done;
	}

	werr = reg_queryvalue(tmp_ctx, key, INCLUDES_VALNAME, &value);
	if (!W_ERROR_IS_OK(werr)) {
		err = SBC_ERR_ACCESS_DENIED;
		goto done;
	}

	if (value->type != REG_MULTI_SZ) {
		/* wrong type -- ignore */
		err = SBC_ERR_OK;
		goto done;
	}

	if (!pull_reg_multi_sz(tmp_ctx, &value->data, &array)) {
		err = SBC_ERR_NOMEM;
		goto done;
	}

	for (count = 0; array[count] != NULL; count++) {
		err = smbconf_add_string_to_array(tmp_ctx,
					&tmp_includes,
					count,
					array[count]);
		if (!SBC_ERROR_IS_OK(err)) {
			goto done;
		}
	}

	if (count > 0) {
		*includes = talloc_move(mem_ctx, &tmp_includes);
		if (*includes == NULL) {
			err = SBC_ERR_NOMEM;
			goto done;
		}
		*num_includes = count;
	} else {
		*num_includes = 0;
		*includes = NULL;
	}

	err = SBC_ERR_OK;
done:
	talloc_free(tmp_ctx);
	return err;
}

/**
 * Get the values of a key as a list of value names
 * and a list of value strings (ordered)
 */
static sbcErr smbconf_reg_get_values(TALLOC_CTX *mem_ctx,
				     struct registry_key *key,
				     uint32_t *num_values,
				     char ***value_names,
				     char ***value_strings)
{
	TALLOC_CTX *tmp_ctx = NULL;
	WERROR werr = WERR_OK;
	sbcErr err;
	uint32_t count;
	struct registry_value *valvalue = NULL;
	char *valname = NULL;
	uint32_t tmp_num_values = 0;
	char **tmp_valnames = NULL;
	char **tmp_valstrings = NULL;
	uint32_t num_includes = 0;
	char **includes = NULL;

	if ((num_values == NULL) || (value_names == NULL) ||
	    (value_strings == NULL))
	{
		err = SBC_ERR_INVALID_PARAM;
		goto done;
	}

	tmp_ctx = talloc_stackframe();

	for (count = 0;
	     werr = reg_enumvalue(tmp_ctx, key, count, &valname, &valvalue),
	     W_ERROR_IS_OK(werr);
	     count++)
	{
		char *valstring;

		if (!smbconf_reg_valname_valid(valname)) {
			continue;
		}

		err = smbconf_add_string_to_array(tmp_ctx,
						  &tmp_valnames,
						  tmp_num_values, valname);
		if (!SBC_ERROR_IS_OK(err)) {
			goto done;
		}

		valstring = smbconf_format_registry_value(tmp_ctx, valvalue);
		err = smbconf_add_string_to_array(tmp_ctx, &tmp_valstrings,
						  tmp_num_values, valstring);
		if (!SBC_ERROR_IS_OK(err)) {
			goto done;
		}
		tmp_num_values++;
	}
	if (!W_ERROR_EQUAL(WERR_NO_MORE_ITEMS, werr)) {
		err = SBC_ERR_NOMEM;
		goto done;
	}

	/* now add the includes at the end */
	err = smbconf_reg_get_includes_internal(tmp_ctx, key, &num_includes,
						 &includes);
	if (!SBC_ERROR_IS_OK(err)) {
		goto done;
	}

	for (count = 0; count < num_includes; count++) {
		err = smbconf_add_string_to_array(tmp_ctx, &tmp_valnames,
						  tmp_num_values, "include");
		if (!SBC_ERROR_IS_OK(err)) {
			goto done;
		}

		err = smbconf_add_string_to_array(tmp_ctx, &tmp_valstrings,
						  tmp_num_values,
						  includes[count]);
		if (!SBC_ERROR_IS_OK(err)) {
			goto done;
		}

		tmp_num_values++;
	}

	*num_values = tmp_num_values;
	if (tmp_num_values > 0) {
		*value_names = talloc_move(mem_ctx, &tmp_valnames);
		*value_strings = talloc_move(mem_ctx, &tmp_valstrings);
	} else {
		*value_names = NULL;
		*value_strings = NULL;
	}

done:
	talloc_free(tmp_ctx);
	return err;
}

static bool smbconf_reg_key_has_values(struct registry_key *key)
{
	WERROR werr;
	uint32_t num_subkeys;
	uint32_t max_subkeylen;
	uint32_t max_subkeysize;
	uint32_t num_values;
	uint32_t max_valnamelen;
	uint32_t max_valbufsize;
	uint32_t secdescsize;
	NTTIME last_changed_time;

	werr = reg_queryinfokey(key, &num_subkeys, &max_subkeylen,
				&max_subkeysize, &num_values, &max_valnamelen,
				&max_valbufsize, &secdescsize,
				&last_changed_time);
	if (!W_ERROR_IS_OK(werr)) {
		return false;
	}

	return (num_values != 0);
}

/**
 * delete all values from a key
 */
static sbcErr smbconf_reg_delete_values(struct registry_key *key)
{
	WERROR werr;
	sbcErr err;
	char *valname;
	struct registry_value *valvalue;
	uint32_t count;
	TALLOC_CTX *mem_ctx = talloc_stackframe();

	for (count = 0;
	     werr = reg_enumvalue(mem_ctx, key, count, &valname, &valvalue),
	     W_ERROR_IS_OK(werr);
	     count++)
	{
		werr = reg_deletevalue(key, valname);
		if (!W_ERROR_IS_OK(werr)) {
			err = SBC_ERR_ACCESS_DENIED;
			goto done;
		}
	}
	if (!W_ERROR_EQUAL(WERR_NO_MORE_ITEMS, werr)) {
		DEBUG(1, ("smbconf_reg_delete_values: "
			  "Error enumerating values of %s: %s\n",
			  key->key->name,
			  win_errstr(werr)));
		err = SBC_ERR_ACCESS_DENIED;
		goto done;
	}

	err = SBC_ERR_OK;

done:
	talloc_free(mem_ctx);
	return err;
}

/**********************************************************************
 *
 * smbconf operations: registry implementations
 *
 **********************************************************************/

/**
 * initialize the registry smbconf backend
 */
static sbcErr smbconf_reg_init(struct smbconf_ctx *ctx, const char *path)
{
	WERROR werr = WERR_OK;
	sbcErr err;
	struct security_token *token;

	if (path == NULL) {
		path = KEY_SMBCONF;
	}
	ctx->path = talloc_strdup(ctx, path);
	if (ctx->path == NULL) {
		err = SBC_ERR_NOMEM;
		goto done;
	}

	ctx->data = TALLOC_ZERO_P(ctx, struct reg_private_data);

	werr = ntstatus_to_werror(registry_create_admin_token(ctx, &token));
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(1, ("Error creating admin token\n"));
		err = SBC_ERR_UNKNOWN_FAILURE;
		goto done;
	}
	rpd(ctx)->open = false;

	werr = registry_init_smbconf(path);
	if (!W_ERROR_IS_OK(werr)) {
		err = SBC_ERR_BADFILE;
		goto done;
	}

	err = ctx->ops->open_conf(ctx);
	if (!SBC_ERROR_IS_OK(err)) {
		DEBUG(1, ("Error opening the registry.\n"));
		goto done;
	}

	werr = reg_open_path(ctx, ctx->path,
			     KEY_ENUMERATE_SUB_KEYS | REG_KEY_WRITE,
			     token, &rpd(ctx)->base_key);
	if (!W_ERROR_IS_OK(werr)) {
		err = SBC_ERR_UNKNOWN_FAILURE;
		goto done;
	}

done:
	return err;
}

static int smbconf_reg_shutdown(struct smbconf_ctx *ctx)
{
	return ctx->ops->close_conf(ctx);
}

static bool smbconf_reg_requires_messaging(struct smbconf_ctx *ctx)
{
#ifdef CLUSTER_SUPPORT
	if (lp_clustering() && lp_parm_bool(-1, "ctdb", "registry.tdb", true)) {
		return true;
	}
#endif
	return false;
}

static bool smbconf_reg_is_writeable(struct smbconf_ctx *ctx)
{
	/*
	 * The backend has write support.
	 *
	 *  TODO: add access checks whether the concrete
	 *  config source is really writeable by the calling user.
	 */
	return true;
}

static sbcErr smbconf_reg_open(struct smbconf_ctx *ctx)
{
	WERROR werr;

	if (rpd(ctx)->open) {
		return SBC_ERR_OK;
	}

	werr = regdb_open();
	if (!W_ERROR_IS_OK(werr)) {
		return SBC_ERR_BADFILE;
	}

	rpd(ctx)->open = true;
	return SBC_ERR_OK;
}

static int smbconf_reg_close(struct smbconf_ctx *ctx)
{
	int ret;

	if (!rpd(ctx)->open) {
		return 0;
	}

	ret = regdb_close();
	if (ret == 0) {
		rpd(ctx)->open = false;
	}
	return ret;
}

/**
 * Get the change sequence number of the given service/parameter.
 * service and parameter strings may be NULL.
 */
static void smbconf_reg_get_csn(struct smbconf_ctx *ctx,
				struct smbconf_csn *csn,
				const char *service, const char *param)
{
	if (csn == NULL) {
		return;
	}

	if (!SBC_ERROR_IS_OK(ctx->ops->open_conf(ctx))) {
		return;
	}

	csn->csn = (uint64_t)regdb_get_seqnum();
}

/**
 * Drop the whole configuration (restarting empty) - registry version
 */
static sbcErr smbconf_reg_drop(struct smbconf_ctx *ctx)
{
	char *path, *p;
	WERROR werr = WERR_OK;
	sbcErr err = SBC_ERR_OK;
	struct registry_key *parent_key = NULL;
	struct registry_key *new_key = NULL;
	TALLOC_CTX* mem_ctx = talloc_stackframe();
	enum winreg_CreateAction action;
	struct security_token *token;

	werr = ntstatus_to_werror(registry_create_admin_token(ctx, &token));
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(1, ("Error creating admin token\n"));
		err = SBC_ERR_UNKNOWN_FAILURE;
		goto done;
	}

	path = talloc_strdup(mem_ctx, ctx->path);
	if (path == NULL) {
		err = SBC_ERR_NOMEM;
		goto done;
	}
	p = strrchr(path, '\\');
	if (p == NULL) {
		err = SBC_ERR_INVALID_PARAM;
		goto done;
	}
	*p = '\0';
	werr = reg_open_path(mem_ctx, path, REG_KEY_WRITE, token,
			     &parent_key);
	if (!W_ERROR_IS_OK(werr)) {
		err = SBC_ERR_IO_FAILURE;
		goto done;
	}

	werr = reg_deletekey_recursive(parent_key, p+1);
	if (!W_ERROR_IS_OK(werr)) {
		err = SBC_ERR_IO_FAILURE;
		goto done;
	}

	werr = reg_createkey(mem_ctx, parent_key, p+1, REG_KEY_WRITE,
			     &new_key, &action);
	if (!W_ERROR_IS_OK(werr)) {
		err = SBC_ERR_IO_FAILURE;
		goto done;
	}

done:
	talloc_free(mem_ctx);
	return err;
}

/**
 * get the list of share names defined in the configuration.
 * registry version.
 */
static sbcErr smbconf_reg_get_share_names(struct smbconf_ctx *ctx,
					  TALLOC_CTX *mem_ctx,
					  uint32_t *num_shares,
					  char ***share_names)
{
	uint32_t count;
	uint32_t added_count = 0;
	TALLOC_CTX *tmp_ctx = NULL;
	WERROR werr;
	sbcErr err = SBC_ERR_OK;
	char *subkey_name = NULL;
	char **tmp_share_names = NULL;

	if ((num_shares == NULL) || (share_names == NULL)) {
		return SBC_ERR_INVALID_PARAM;
	}

	tmp_ctx = talloc_stackframe();

	/* if there are values in the base key, return NULL as share name */

	if (smbconf_reg_key_has_values(rpd(ctx)->base_key)) {
		err = smbconf_add_string_to_array(tmp_ctx, &tmp_share_names,
						   0, NULL);
		if (!SBC_ERROR_IS_OK(err)) {
			goto done;
		}
		added_count++;
	}

	/* make sure "global" is always listed first */
	if (smbconf_share_exists(ctx, GLOBAL_NAME)) {
		err = smbconf_add_string_to_array(tmp_ctx, &tmp_share_names,
						  added_count, GLOBAL_NAME);
		if (!SBC_ERROR_IS_OK(err)) {
			goto done;
		}
		added_count++;
	}

	for (count = 0;
	     werr = reg_enumkey(tmp_ctx, rpd(ctx)->base_key, count,
				&subkey_name, NULL),
	     W_ERROR_IS_OK(werr);
	     count++)
	{
		if (strequal(subkey_name, GLOBAL_NAME)) {
			continue;
		}

		err = smbconf_add_string_to_array(tmp_ctx,
						   &tmp_share_names,
						   added_count,
						   subkey_name);
		if (!SBC_ERROR_IS_OK(err)) {
			goto done;
		}
		added_count++;
	}
	if (!W_ERROR_EQUAL(WERR_NO_MORE_ITEMS, werr)) {
		err = SBC_ERR_NO_MORE_ITEMS;
		goto done;
	}
	err = SBC_ERR_OK;

	*num_shares = added_count;
	if (added_count > 0) {
		*share_names = talloc_move(mem_ctx, &tmp_share_names);
	} else {
		*share_names = NULL;
	}

done:
	talloc_free(tmp_ctx);
	return err;
}

/**
 * check if a share/service of a given name exists - registry version
 */
static bool smbconf_reg_share_exists(struct smbconf_ctx *ctx,
				     const char *servicename)
{
	bool ret = false;
	sbcErr err;
	TALLOC_CTX *mem_ctx = talloc_stackframe();
	struct registry_key *key = NULL;

	err = smbconf_reg_open_service_key(mem_ctx, ctx, servicename,
					   REG_KEY_READ, &key);
	if (SBC_ERROR_IS_OK(err)) {
		ret = true;
	}

	talloc_free(mem_ctx);
	return ret;
}

/**
 * Add a service if it does not already exist - registry version
 */
static sbcErr smbconf_reg_create_share(struct smbconf_ctx *ctx,
				       const char *servicename)
{
	sbcErr err;
	struct registry_key *key = NULL;

	if (servicename == NULL) {
		return SBC_ERR_OK;
	}

	err = smbconf_reg_create_service_key(talloc_tos(), ctx,
					     servicename, &key);

	talloc_free(key);
	return err;
}

/**
 * get a definition of a share (service) from configuration.
 */
static sbcErr smbconf_reg_get_share(struct smbconf_ctx *ctx,
				    TALLOC_CTX *mem_ctx,
				    const char *servicename,
				    struct smbconf_service **service)
{
	sbcErr err;
	struct registry_key *key = NULL;
	struct smbconf_service *tmp_service = NULL;
	TALLOC_CTX *tmp_ctx = talloc_stackframe();

	err = smbconf_reg_open_service_key(tmp_ctx, ctx, servicename,
					   REG_KEY_READ, &key);
	if (!SBC_ERROR_IS_OK(err)) {
		goto done;
	}

	tmp_service = TALLOC_ZERO_P(tmp_ctx, struct smbconf_service);
	if (tmp_service == NULL) {
		err = SBC_ERR_NOMEM;
		goto done;
	}

	if (servicename != NULL) {
		tmp_service->name = talloc_strdup(tmp_service, servicename);
		if (tmp_service->name == NULL) {
			err = SBC_ERR_NOMEM;
			goto done;
		}
	}

	err = smbconf_reg_get_values(tmp_service, key,
				     &(tmp_service->num_params),
				     &(tmp_service->param_names),
				     &(tmp_service->param_values));
	if (SBC_ERROR_IS_OK(err)) {
		*service = talloc_move(mem_ctx, &tmp_service);
	}

done:
	talloc_free(tmp_ctx);
	return err;
}

/**
 * delete a service from configuration
 */
static sbcErr smbconf_reg_delete_share(struct smbconf_ctx *ctx,
				       const char *servicename)
{
	WERROR werr;
	sbcErr err = SBC_ERR_OK;
	TALLOC_CTX *mem_ctx = talloc_stackframe();

	if (servicename != NULL) {
		werr = reg_deletekey_recursive(rpd(ctx)->base_key, servicename);
		if (!W_ERROR_IS_OK(werr)) {
			err = SBC_ERR_ACCESS_DENIED;
		}
	} else {
		err = smbconf_reg_delete_values(rpd(ctx)->base_key);
	}

	talloc_free(mem_ctx);
	return err;
}

/**
 * set a configuration parameter to the value provided.
 */
static sbcErr smbconf_reg_set_parameter(struct smbconf_ctx *ctx,
					const char *service,
					const char *param,
					const char *valstr)
{
	sbcErr err;
	struct registry_key *key = NULL;
	TALLOC_CTX *mem_ctx = talloc_stackframe();

	err = smbconf_reg_open_service_key(mem_ctx, ctx, service,
					   REG_KEY_WRITE, &key);
	if (!SBC_ERROR_IS_OK(err)) {
		goto done;
	}

	err = smbconf_reg_set_value(key, param, valstr);

done:
	talloc_free(mem_ctx);
	return err;
}

/**
 * get the value of a configuration parameter as a string
 */
static sbcErr smbconf_reg_get_parameter(struct smbconf_ctx *ctx,
					TALLOC_CTX *mem_ctx,
					const char *service,
					const char *param,
					char **valstr)
{
	WERROR werr = WERR_OK;
	sbcErr err;
	struct registry_key *key = NULL;
	struct registry_value *value = NULL;

	err = smbconf_reg_open_service_key(mem_ctx, ctx, service,
					   REG_KEY_READ, &key);
	if (!SBC_ERROR_IS_OK(err)) {
		goto done;
	}

	if (!smbconf_reg_valname_valid(param)) {
		err = SBC_ERR_INVALID_PARAM;
		goto done;
	}

	if (!smbconf_value_exists(key, param)) {
		err = SBC_ERR_INVALID_PARAM;
		goto done;
	}

	werr = reg_queryvalue(mem_ctx, key, param, &value);
	if (!W_ERROR_IS_OK(werr)) {
		err = SBC_ERR_NOMEM;
		goto done;
	}

	*valstr = smbconf_format_registry_value(mem_ctx, value);
	if (*valstr == NULL) {
		err = SBC_ERR_NOMEM;
	}

done:
	talloc_free(key);
	talloc_free(value);
	return err;
}

/**
 * delete a parameter from configuration
 */
static sbcErr smbconf_reg_delete_parameter(struct smbconf_ctx *ctx,
					   const char *service,
					   const char *param)
{
	struct registry_key *key = NULL;
	WERROR werr;
	sbcErr err;
	TALLOC_CTX *mem_ctx = talloc_stackframe();

	err = smbconf_reg_open_service_key(mem_ctx, ctx, service,
					   REG_KEY_ALL, &key);
	if (!SBC_ERROR_IS_OK(err)) {
		goto done;
	}

	if (!smbconf_reg_valname_valid(param)) {
		err = SBC_ERR_INVALID_PARAM;
		goto done;
	}

	if (!smbconf_value_exists(key, param)) {
		err = SBC_ERR_OK;
		goto done;
	}

	werr = reg_deletevalue(key, param);
	if (!W_ERROR_IS_OK(werr)) {
		err = SBC_ERR_ACCESS_DENIED;
	}

done:
	talloc_free(mem_ctx);
	return err;
}

static sbcErr smbconf_reg_get_includes(struct smbconf_ctx *ctx,
				       TALLOC_CTX *mem_ctx,
				       const char *service,
				       uint32_t *num_includes,
				       char ***includes)
{
	sbcErr err;
	struct registry_key *key = NULL;
	TALLOC_CTX *tmp_ctx = talloc_stackframe();

	err = smbconf_reg_open_service_key(tmp_ctx, ctx, service,
					   REG_KEY_READ, &key);
	if (!SBC_ERROR_IS_OK(err)) {
		goto done;
	}

	err = smbconf_reg_get_includes_internal(mem_ctx, key, num_includes,
						 includes);
	if (!SBC_ERROR_IS_OK(err)) {
		goto done;
	}

done:
	talloc_free(tmp_ctx);
	return err;
}

static sbcErr smbconf_reg_set_includes(struct smbconf_ctx *ctx,
				       const char *service,
				       uint32_t num_includes,
				       const char **includes)
{
	WERROR werr = WERR_OK;
	sbcErr err;
	struct registry_key *key = NULL;
	TALLOC_CTX *tmp_ctx = talloc_stackframe();

	err = smbconf_reg_open_service_key(tmp_ctx, ctx, service,
					   REG_KEY_ALL, &key);
	if (!SBC_ERROR_IS_OK(err)) {
		goto done;
	}

	if (num_includes == 0) {
		if (!smbconf_value_exists(key, INCLUDES_VALNAME)) {
			err = SBC_ERR_OK;
			goto done;
		}
		werr = reg_deletevalue(key, INCLUDES_VALNAME);
		if (!W_ERROR_IS_OK(werr)) {
			err = SBC_ERR_ACCESS_DENIED;
			goto done;
		}
	} else {
		err = smbconf_reg_set_multi_sz_value(key, INCLUDES_VALNAME,
						      num_includes, includes);
	}

done:
	talloc_free(tmp_ctx);
	return err;
}

static sbcErr smbconf_reg_delete_includes(struct smbconf_ctx *ctx,
					  const char *service)
{
	WERROR werr;
	sbcErr err;
	struct registry_key *key = NULL;
	TALLOC_CTX *tmp_ctx = talloc_stackframe();

	err = smbconf_reg_open_service_key(tmp_ctx, ctx, service,
					   REG_KEY_ALL, &key);
	if (!SBC_ERROR_IS_OK(err)) {
		goto done;
	}

	if (!smbconf_value_exists(key, INCLUDES_VALNAME)) {
		err = SBC_ERR_OK;
		goto done;
	}

	werr = reg_deletevalue(key, INCLUDES_VALNAME);
	if (!W_ERROR_IS_OK(werr)) {
		err = SBC_ERR_ACCESS_DENIED;
		goto done;
	}

	err = SBC_ERR_OK;
done:
	talloc_free(tmp_ctx);
	return err;
}

static sbcErr smbconf_reg_transaction_start(struct smbconf_ctx *ctx)
{
	WERROR werr;

	werr = regdb_transaction_start();
	if (!W_ERROR_IS_OK(werr)) {
		return SBC_ERR_IO_FAILURE;
	}

	return SBC_ERR_OK;
}

static sbcErr smbconf_reg_transaction_commit(struct smbconf_ctx *ctx)
{
	WERROR werr;

	werr = regdb_transaction_commit();
	if (!W_ERROR_IS_OK(werr)) {
		return SBC_ERR_IO_FAILURE;
	}

	return SBC_ERR_OK;
}

static sbcErr smbconf_reg_transaction_cancel(struct smbconf_ctx *ctx)
{
	WERROR werr;

	werr = regdb_transaction_cancel();
	if (!W_ERROR_IS_OK(werr)) {
		return SBC_ERR_IO_FAILURE;
	}

	return SBC_ERR_OK;
}

struct smbconf_ops smbconf_ops_reg = {
	.init			= smbconf_reg_init,
	.shutdown		= smbconf_reg_shutdown,
	.requires_messaging	= smbconf_reg_requires_messaging,
	.is_writeable		= smbconf_reg_is_writeable,
	.open_conf		= smbconf_reg_open,
	.close_conf		= smbconf_reg_close,
	.get_csn		= smbconf_reg_get_csn,
	.drop			= smbconf_reg_drop,
	.get_share_names	= smbconf_reg_get_share_names,
	.share_exists		= smbconf_reg_share_exists,
	.create_share		= smbconf_reg_create_share,
	.get_share		= smbconf_reg_get_share,
	.delete_share		= smbconf_reg_delete_share,
	.set_parameter		= smbconf_reg_set_parameter,
	.get_parameter		= smbconf_reg_get_parameter,
	.delete_parameter	= smbconf_reg_delete_parameter,
	.get_includes		= smbconf_reg_get_includes,
	.set_includes		= smbconf_reg_set_includes,
	.delete_includes	= smbconf_reg_delete_includes,
	.transaction_start	= smbconf_reg_transaction_start,
	.transaction_commit	= smbconf_reg_transaction_commit,
	.transaction_cancel	= smbconf_reg_transaction_cancel,
};


/**
 * initialize the smbconf registry backend
 * the only function that is exported from this module
 */
sbcErr smbconf_init_reg(TALLOC_CTX *mem_ctx, struct smbconf_ctx **conf_ctx,
			const char *path)
{
	return smbconf_init_internal(mem_ctx, conf_ctx, path, &smbconf_ops_reg);
}
