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
static WERROR smbconf_reg_open_service_key(TALLOC_CTX *mem_ctx,
					   struct smbconf_ctx *ctx,
					   const char *servicename,
					   uint32 desired_access,
					   struct registry_key **key)
{
	WERROR werr;

	if (servicename == NULL) {
		*key = rpd(ctx)->base_key;
		return WERR_OK;
	}
	werr = reg_openkey(mem_ctx, rpd(ctx)->base_key, servicename,
			   desired_access, key);

	if (W_ERROR_EQUAL(werr, WERR_BADFILE)) {
		werr = WERR_NO_SUCH_SERVICE;
	}

	return werr;
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
static WERROR smbconf_reg_create_service_key(TALLOC_CTX *mem_ctx,
					     struct smbconf_ctx *ctx,
					     const char * subkeyname,
					     struct registry_key **newkey)
{
	WERROR werr = WERR_OK;
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
		werr = WERR_FILE_EXISTS;
	}
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(5, ("Error creating key %s: %s\n",
			 subkeyname, win_errstr(werr)));
	}

	talloc_free(create_ctx);
	return werr;
}

/**
 * add a value to a key.
 */
static WERROR smbconf_reg_set_value(struct registry_key *key,
				    const char *valname,
				    const char *valstr)
{
	struct registry_value val;
	WERROR werr = WERR_OK;
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
		werr = WERR_INVALID_PARAM;
		goto done;
	}

	if (smbconf_reg_valname_forbidden(canon_valname)) {
		DEBUG(5, ("Parameter '%s' not allowed in registry.\n",
			  canon_valname));
		werr = WERR_INVALID_PARAM;
		goto done;
	}

	subkeyname = strrchr_m(key->key->name, '\\');
	if ((subkeyname == NULL) || (*(subkeyname +1) == '\0')) {
		DEBUG(5, ("Invalid registry key '%s' given as "
			  "smbconf section.\n", key->key->name));
		werr = WERR_INVALID_PARAM;
		goto done;
	}
	subkeyname++;
	if (!strequal(subkeyname, GLOBAL_NAME) &&
	    lp_parameter_is_global(valname))
	{
		DEBUG(5, ("Global parameter '%s' not allowed in "
			  "service definition ('%s').\n", canon_valname,
			  subkeyname));
		werr = WERR_INVALID_PARAM;
		goto done;
	}

	ZERO_STRUCT(val);

	val.type = REG_SZ;
	val.v.sz.str = CONST_DISCARD(char *, canon_valstr);
	val.v.sz.len = strlen(canon_valstr) + 1;

	werr = reg_setvalue(key, canon_valname, &val);
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(5, ("Error adding value '%s' to "
			  "key '%s': %s\n",
			  canon_valname, key->key->name, win_errstr(werr)));
	}

done:
	return werr;
}

static WERROR smbconf_reg_set_multi_sz_value(struct registry_key *key,
					     const char *valname,
					     const uint32_t num_strings,
					     const char **strings)
{
	WERROR werr;
	struct registry_value *value;
	uint32_t count;
	TALLOC_CTX *tmp_ctx = talloc_stackframe();

	if (strings == NULL) {
		werr = WERR_INVALID_PARAM;
		goto done;
	}

	value = TALLOC_ZERO_P(tmp_ctx, struct registry_value);

	value->type = REG_MULTI_SZ;
	value->v.multi_sz.num_strings = num_strings;
	value->v.multi_sz.strings = TALLOC_ARRAY(tmp_ctx, char *, num_strings);
	if (value->v.multi_sz.strings == NULL) {
		werr = WERR_NOMEM;
		goto done;
	}
	for (count = 0; count < num_strings; count++) {
		value->v.multi_sz.strings[count] =
			talloc_strdup(value->v.multi_sz.strings,
				      strings[count]);
		if (value->v.multi_sz.strings[count] == NULL) {
			werr = WERR_NOMEM;
			goto done;
		}
	}

	werr = reg_setvalue(key, valname, value);
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(5, ("Error adding value '%s' to key '%s': %s\n",
			  valname, key->key->name, win_errstr(werr)));
	}

done:
	talloc_free(tmp_ctx);
	return werr;
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
		result = talloc_asprintf(mem_ctx, "%d", value->v.dword);
		break;
	case REG_SZ:
	case REG_EXPAND_SZ:
		result = talloc_asprintf(mem_ctx, "%s", value->v.sz.str);
		break;
	case REG_MULTI_SZ: {
		uint32 j;
		for (j = 0; j < value->v.multi_sz.num_strings; j++) {
			result = talloc_asprintf(mem_ctx, "%s\"%s\" ",
						 result ? result : "" ,
						 value->v.multi_sz.strings[j]);
			if (result == NULL) {
				break;
			}
		}
		break;
	}
	case REG_BINARY:
		result = talloc_asprintf(mem_ctx, "binary (%d bytes)",
					 (int)value->v.binary.length);
		break;
	default:
		result = talloc_asprintf(mem_ctx, "<unprintable>");
		break;
	}
	return result;
}

static WERROR smbconf_reg_get_includes_internal(TALLOC_CTX *mem_ctx,
						struct registry_key *key,
						uint32_t *num_includes,
						char ***includes)
{
	WERROR werr;
	uint32_t count;
	struct registry_value *value = NULL;
	char **tmp_includes = NULL;
	TALLOC_CTX *tmp_ctx = talloc_stackframe();

	if (!smbconf_value_exists(key, INCLUDES_VALNAME)) {
		/* no includes */
		*num_includes = 0;
		*includes = NULL;
		werr = WERR_OK;
		goto done;
	}

	werr = reg_queryvalue(tmp_ctx, key, INCLUDES_VALNAME, &value);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	if (value->type != REG_MULTI_SZ) {
		/* wrong type -- ignore */
		goto done;
	}

	for (count = 0; count < value->v.multi_sz.num_strings; count++)
	{
		werr = smbconf_add_string_to_array(tmp_ctx,
					&tmp_includes,
					count,
					value->v.multi_sz.strings[count]);
		if (!W_ERROR_IS_OK(werr)) {
			goto done;
		}
	}

	if (count > 0) {
		*includes = talloc_move(mem_ctx, &tmp_includes);
		if (*includes == NULL) {
			werr = WERR_NOMEM;
			goto done;
		}
		*num_includes = count;
	} else {
		*num_includes = 0;
		*includes = NULL;
	}

done:
	talloc_free(tmp_ctx);
	return werr;
}

/**
 * Get the values of a key as a list of value names
 * and a list of value strings (ordered)
 */
static WERROR smbconf_reg_get_values(TALLOC_CTX *mem_ctx,
				     struct registry_key *key,
				     uint32_t *num_values,
				     char ***value_names,
				     char ***value_strings)
{
	TALLOC_CTX *tmp_ctx = NULL;
	WERROR werr = WERR_OK;
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
		werr = WERR_INVALID_PARAM;
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

		werr = smbconf_add_string_to_array(tmp_ctx,
						   &tmp_valnames,
						   tmp_num_values, valname);
		if (!W_ERROR_IS_OK(werr)) {
			goto done;
		}

		valstring = smbconf_format_registry_value(tmp_ctx, valvalue);
		werr = smbconf_add_string_to_array(tmp_ctx, &tmp_valstrings,
						   tmp_num_values, valstring);
		if (!W_ERROR_IS_OK(werr)) {
			goto done;
		}
		tmp_num_values++;
	}
	if (!W_ERROR_EQUAL(WERR_NO_MORE_ITEMS, werr)) {
		goto done;
	}

	/* now add the includes at the end */
	werr = smbconf_reg_get_includes_internal(tmp_ctx, key, &num_includes,
						 &includes);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}
	for (count = 0; count < num_includes; count++) {
		werr = smbconf_add_string_to_array(tmp_ctx, &tmp_valnames,
						   tmp_num_values, "include");
		if (!W_ERROR_IS_OK(werr)) {
			goto done;
		}

		werr = smbconf_add_string_to_array(tmp_ctx, &tmp_valstrings,
						   tmp_num_values,
						   includes[count]);
		if (!W_ERROR_IS_OK(werr)) {
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
	return werr;
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
static WERROR smbconf_reg_delete_values(struct registry_key *key)
{
	WERROR werr;
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
			goto done;
		}
	}
	if (!W_ERROR_EQUAL(WERR_NO_MORE_ITEMS, werr)) {
		DEBUG(1, ("smbconf_reg_delete_values: "
			  "Error enumerating values of %s: %s\n",
			  key->key->name,
			  win_errstr(werr)));
		goto done;
	}

	werr = WERR_OK;

done:
	talloc_free(mem_ctx);
	return werr;
}

/**********************************************************************
 *
 * smbconf operations: registry implementations
 *
 **********************************************************************/

/**
 * initialize the registry smbconf backend
 */
static WERROR smbconf_reg_init(struct smbconf_ctx *ctx, const char *path)
{
	WERROR werr = WERR_OK;
	struct nt_user_token *token;

	if (path == NULL) {
		path = KEY_SMBCONF;
	}
	ctx->path = talloc_strdup(ctx, path);
	if (ctx->path == NULL) {
		werr = WERR_NOMEM;
		goto done;
	}

	ctx->data = TALLOC_ZERO_P(ctx, struct reg_private_data);

	werr = ntstatus_to_werror(registry_create_admin_token(ctx, &token));
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(1, ("Error creating admin token\n"));
		goto done;
	}
	rpd(ctx)->open = false;

	werr = registry_init_smbconf(path);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	werr = ctx->ops->open_conf(ctx);
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(1, ("Error opening the registry.\n"));
		goto done;
	}

	werr = reg_open_path(ctx, ctx->path,
			     KEY_ENUMERATE_SUB_KEYS | REG_KEY_WRITE,
			     token, &rpd(ctx)->base_key);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

done:
	return werr;
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

static WERROR smbconf_reg_open(struct smbconf_ctx *ctx)
{
	WERROR werr;

	if (rpd(ctx)->open) {
		return WERR_OK;
	}

	werr = regdb_open();
	if (W_ERROR_IS_OK(werr)) {
		rpd(ctx)->open = true;
	}
	return werr;
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

	if (!W_ERROR_IS_OK(ctx->ops->open_conf(ctx))) {
		return;
	}

	csn->csn = (uint64_t)regdb_get_seqnum();
}

/**
 * Drop the whole configuration (restarting empty) - registry version
 */
static WERROR smbconf_reg_drop(struct smbconf_ctx *ctx)
{
	char *path, *p;
	WERROR werr = WERR_OK;
	struct registry_key *parent_key = NULL;
	struct registry_key *new_key = NULL;
	TALLOC_CTX* mem_ctx = talloc_stackframe();
	enum winreg_CreateAction action;
	struct nt_user_token *token;

	werr = ntstatus_to_werror(registry_create_admin_token(ctx, &token));
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(1, ("Error creating admin token\n"));
		goto done;
	}

	path = talloc_strdup(mem_ctx, ctx->path);
	if (path == NULL) {
		werr = WERR_NOMEM;
		goto done;
	}
	p = strrchr(path, '\\');
	*p = '\0';
	werr = reg_open_path(mem_ctx, path, REG_KEY_WRITE, token,
			     &parent_key);

	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	werr = reg_deletekey_recursive(mem_ctx, parent_key, p+1);

	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	werr = reg_createkey(mem_ctx, parent_key, p+1, REG_KEY_WRITE,
			     &new_key, &action);

done:
	talloc_free(mem_ctx);
	return werr;
}

/**
 * get the list of share names defined in the configuration.
 * registry version.
 */
static WERROR smbconf_reg_get_share_names(struct smbconf_ctx *ctx,
					  TALLOC_CTX *mem_ctx,
					  uint32_t *num_shares,
					  char ***share_names)
{
	uint32_t count;
	uint32_t added_count = 0;
	TALLOC_CTX *tmp_ctx = NULL;
	WERROR werr = WERR_OK;
	char *subkey_name = NULL;
	char **tmp_share_names = NULL;

	if ((num_shares == NULL) || (share_names == NULL)) {
		werr = WERR_INVALID_PARAM;
		goto done;
	}

	tmp_ctx = talloc_stackframe();

	/* if there are values in the base key, return NULL as share name */

	if (smbconf_reg_key_has_values(rpd(ctx)->base_key)) {
		werr = smbconf_add_string_to_array(tmp_ctx, &tmp_share_names,
						   0, NULL);
		if (!W_ERROR_IS_OK(werr)) {
			goto done;
		}
		added_count++;
	}

	/* make sure "global" is always listed first */
	if (smbconf_share_exists(ctx, GLOBAL_NAME)) {
		werr = smbconf_add_string_to_array(tmp_ctx, &tmp_share_names,
						   added_count, GLOBAL_NAME);
		if (!W_ERROR_IS_OK(werr)) {
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

		werr = smbconf_add_string_to_array(tmp_ctx,
						   &tmp_share_names,
						   added_count,
						   subkey_name);
		if (!W_ERROR_IS_OK(werr)) {
			goto done;
		}
		added_count++;
	}
	if (!W_ERROR_EQUAL(WERR_NO_MORE_ITEMS, werr)) {
		goto done;
	}
	werr = WERR_OK;

	*num_shares = added_count;
	if (added_count > 0) {
		*share_names = talloc_move(mem_ctx, &tmp_share_names);
	} else {
		*share_names = NULL;
	}

done:
	talloc_free(tmp_ctx);
	return werr;
}

/**
 * check if a share/service of a given name exists - registry version
 */
static bool smbconf_reg_share_exists(struct smbconf_ctx *ctx,
				     const char *servicename)
{
	bool ret = false;
	WERROR werr = WERR_OK;
	TALLOC_CTX *mem_ctx = talloc_stackframe();
	struct registry_key *key = NULL;

	werr = smbconf_reg_open_service_key(mem_ctx, ctx, servicename,
					    REG_KEY_READ, &key);
	if (W_ERROR_IS_OK(werr)) {
		ret = true;
	}

	talloc_free(mem_ctx);
	return ret;
}

/**
 * Add a service if it does not already exist - registry version
 */
static WERROR smbconf_reg_create_share(struct smbconf_ctx *ctx,
				       const char *servicename)
{
	WERROR werr;
	struct registry_key *key = NULL;

	if (servicename == NULL) {
		return WERR_OK;
	}

	werr = smbconf_reg_create_service_key(talloc_tos(), ctx,
					      servicename, &key);

	talloc_free(key);
	return werr;
}

/**
 * get a definition of a share (service) from configuration.
 */
static WERROR smbconf_reg_get_share(struct smbconf_ctx *ctx,
				    TALLOC_CTX *mem_ctx,
				    const char *servicename,
				    struct smbconf_service **service)
{
	WERROR werr = WERR_OK;
	struct registry_key *key = NULL;
	struct smbconf_service *tmp_service = NULL;
	TALLOC_CTX *tmp_ctx = talloc_stackframe();

	werr = smbconf_reg_open_service_key(tmp_ctx, ctx, servicename,
					    REG_KEY_READ, &key);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	tmp_service = TALLOC_ZERO_P(tmp_ctx, struct smbconf_service);
	if (tmp_service == NULL) {
		werr =  WERR_NOMEM;
		goto done;
	}

	if (servicename != NULL) {
		tmp_service->name = talloc_strdup(tmp_service, servicename);
		if (tmp_service->name == NULL) {
			werr = WERR_NOMEM;
			goto done;
		}
	}

	werr = smbconf_reg_get_values(tmp_service, key,
				      &(tmp_service->num_params),
				      &(tmp_service->param_names),
				      &(tmp_service->param_values));

	if (W_ERROR_IS_OK(werr)) {
		*service = talloc_move(mem_ctx, &tmp_service);
	}

done:
	talloc_free(tmp_ctx);
	return werr;
}

/**
 * delete a service from configuration
 */
static WERROR smbconf_reg_delete_share(struct smbconf_ctx *ctx,
				       const char *servicename)
{
	WERROR werr = WERR_OK;
	TALLOC_CTX *mem_ctx = talloc_stackframe();

	if (servicename != NULL) {
		werr = reg_deletekey_recursive(mem_ctx, rpd(ctx)->base_key,
					       servicename);
	} else {
		werr = smbconf_reg_delete_values(rpd(ctx)->base_key);
	}

	talloc_free(mem_ctx);
	return werr;
}

/**
 * set a configuration parameter to the value provided.
 */
static WERROR smbconf_reg_set_parameter(struct smbconf_ctx *ctx,
					const char *service,
					const char *param,
					const char *valstr)
{
	WERROR werr;
	struct registry_key *key = NULL;
	TALLOC_CTX *mem_ctx = talloc_stackframe();

	werr = smbconf_reg_open_service_key(mem_ctx, ctx, service,
					    REG_KEY_WRITE, &key);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	werr = smbconf_reg_set_value(key, param, valstr);

done:
	talloc_free(mem_ctx);
	return werr;
}

/**
 * get the value of a configuration parameter as a string
 */
static WERROR smbconf_reg_get_parameter(struct smbconf_ctx *ctx,
					TALLOC_CTX *mem_ctx,
					const char *service,
					const char *param,
					char **valstr)
{
	WERROR werr = WERR_OK;
	struct registry_key *key = NULL;
	struct registry_value *value = NULL;

	werr = smbconf_reg_open_service_key(mem_ctx, ctx, service,
					    REG_KEY_READ, &key);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	if (!smbconf_reg_valname_valid(param)) {
		werr = WERR_INVALID_PARAM;
		goto done;
	}

	if (!smbconf_value_exists(key, param)) {
		werr = WERR_INVALID_PARAM;
		goto done;
	}

	werr = reg_queryvalue(mem_ctx, key, param, &value);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	*valstr = smbconf_format_registry_value(mem_ctx, value);

	if (*valstr == NULL) {
		werr = WERR_NOMEM;
	}

done:
	talloc_free(key);
	talloc_free(value);
	return werr;
}

/**
 * delete a parameter from configuration
 */
static WERROR smbconf_reg_delete_parameter(struct smbconf_ctx *ctx,
					   const char *service,
					   const char *param)
{
	struct registry_key *key = NULL;
	WERROR werr = WERR_OK;
	TALLOC_CTX *mem_ctx = talloc_stackframe();

	werr = smbconf_reg_open_service_key(mem_ctx, ctx, service,
					    REG_KEY_ALL, &key);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	if (!smbconf_reg_valname_valid(param)) {
		werr = WERR_INVALID_PARAM;
		goto done;
	}

	if (!smbconf_value_exists(key, param)) {
		werr = WERR_INVALID_PARAM;
		goto done;
	}

	werr = reg_deletevalue(key, param);

done:
	talloc_free(mem_ctx);
	return werr;
}

static WERROR smbconf_reg_get_includes(struct smbconf_ctx *ctx,
				       TALLOC_CTX *mem_ctx,
				       const char *service,
				       uint32_t *num_includes,
				       char ***includes)
{
	WERROR werr;
	struct registry_key *key = NULL;
	TALLOC_CTX *tmp_ctx = talloc_stackframe();

	werr = smbconf_reg_open_service_key(tmp_ctx, ctx, service,
					    REG_KEY_READ, &key);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	werr = smbconf_reg_get_includes_internal(mem_ctx, key, num_includes,
						 includes);

done:
	talloc_free(tmp_ctx);
	return werr;
}

static WERROR smbconf_reg_set_includes(struct smbconf_ctx *ctx,
				       const char *service,
				       uint32_t num_includes,
				       const char **includes)
{
	WERROR werr = WERR_OK;
	struct registry_key *key = NULL;
	TALLOC_CTX *tmp_ctx = talloc_stackframe();

	werr = smbconf_reg_open_service_key(tmp_ctx, ctx, service,
					    REG_KEY_ALL, &key);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	if (num_includes == 0) {
		if (!smbconf_value_exists(key, INCLUDES_VALNAME)) {
			goto done;
		}
		werr = reg_deletevalue(key, INCLUDES_VALNAME);
	} else {
		werr = smbconf_reg_set_multi_sz_value(key, INCLUDES_VALNAME,
						      num_includes, includes);
	}

done:
	talloc_free(tmp_ctx);
	return werr;
}

static WERROR smbconf_reg_delete_includes(struct smbconf_ctx *ctx,
					  const char *service)
{
	WERROR werr = WERR_OK;
	struct registry_key *key = NULL;
	TALLOC_CTX *tmp_ctx = talloc_stackframe();

	werr = smbconf_reg_open_service_key(tmp_ctx, ctx, service,
					    REG_KEY_ALL, &key);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	if (!smbconf_value_exists(key, INCLUDES_VALNAME)) {
		goto done;
	}

	werr = reg_deletevalue(key, INCLUDES_VALNAME);


done:
	talloc_free(tmp_ctx);
	return werr;
}

static WERROR smbconf_reg_transaction_start(struct smbconf_ctx *ctx)
{
	return regdb_transaction_start();
}

static WERROR smbconf_reg_transaction_commit(struct smbconf_ctx *ctx)
{
	return regdb_transaction_commit();
}

static WERROR smbconf_reg_transaction_cancel(struct smbconf_ctx *ctx)
{
	return regdb_transaction_cancel();
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
WERROR smbconf_init_reg(TALLOC_CTX *mem_ctx, struct smbconf_ctx **conf_ctx,
			const char *path)
{
	return smbconf_init_internal(mem_ctx, conf_ctx, path, &smbconf_ops_reg);
}
