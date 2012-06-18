/*
 *  Unix SMB/CIFS implementation.
 *  Virtual Windows Registry Layer
 *  Copyright (C) Volker Lendecke 2006
 *  Copyright (C) Michael Adam 2007-2008
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

/* Attempt to wrap the existing API in a more winreg.idl-like way */

/*
 * Here is a list of winreg.idl functions and corresponding implementations
 * provided here:
 *
 * 0x00		winreg_OpenHKCR
 * 0x01		winreg_OpenHKCU
 * 0x02		winreg_OpenHKLM
 * 0x03		winreg_OpenHKPD
 * 0x04		winreg_OpenHKU
 * 0x05		winreg_CloseKey
 * 0x06		winreg_CreateKey			reg_createkey
 * 0x07		winreg_DeleteKey			reg_deletekey
 * 0x08		winreg_DeleteValue			reg_deletevalue
 * 0x09		winreg_EnumKey				reg_enumkey
 * 0x0a		winreg_EnumValue			reg_enumvalue
 * 0x0b		winreg_FlushKey
 * 0x0c		winreg_GetKeySecurity			reg_getkeysecurity
 * 0x0d		winreg_LoadKey
 * 0x0e		winreg_NotifyChangeKeyValue
 * 0x0f		winreg_OpenKey				reg_openkey
 * 0x10		winreg_QueryInfoKey			reg_queryinfokey
 * 0x11		winreg_QueryValue			reg_queryvalue
 * 0x12		winreg_ReplaceKey
 * 0x13		winreg_RestoreKey			reg_restorekey
 * 0x14		winreg_SaveKey				reg_savekey
 * 0x15		winreg_SetKeySecurity			reg_setkeysecurity
 * 0x16		winreg_SetValue				reg_setvalue
 * 0x17		winreg_UnLoadKey
 * 0x18		winreg_InitiateSystemShutdown
 * 0x19		winreg_AbortSystemShutdown
 * 0x1a		winreg_GetVersion			reg_getversion
 * 0x1b		winreg_OpenHKCC
 * 0x1c		winreg_OpenHKDD
 * 0x1d		winreg_QueryMultipleValues
 * 0x1e		winreg_InitiateSystemShutdownEx
 * 0x1f		winreg_SaveKeyEx
 * 0x20		winreg_OpenHKPT
 * 0x21		winreg_OpenHKPN
 * 0x22		winreg_QueryMultipleValues2
 *
 */

#include "includes.h"
#include "regfio.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_REGISTRY


/**********************************************************************
 * Helper functions
 **********************************************************************/

static WERROR fill_value_cache(struct registry_key *key)
{
	if (key->values != NULL) {
		if (!reg_values_need_update(key->key, key->values)) {
			return WERR_OK;
		}
	}

	if (!(key->values = TALLOC_ZERO_P(key, struct regval_ctr))) {
		return WERR_NOMEM;
	}
	if (fetch_reg_values(key->key, key->values) == -1) {
		TALLOC_FREE(key->values);
		return WERR_BADFILE;
	}

	return WERR_OK;
}

static WERROR fill_subkey_cache(struct registry_key *key)
{
	WERROR werr;

	if (key->subkeys != NULL) {
		if (!reg_subkeys_need_update(key->key, key->subkeys)) {
			return WERR_OK;
		}
	}

	werr = regsubkey_ctr_init(key, &(key->subkeys));
	W_ERROR_NOT_OK_RETURN(werr);

	if (fetch_reg_keys(key->key, key->subkeys) == -1) {
		TALLOC_FREE(key->subkeys);
		return WERR_NO_MORE_ITEMS;
	}

	return WERR_OK;
}

static int regkey_destructor(struct registry_key_handle *key)
{
	return regdb_close();
}

static WERROR regkey_open_onelevel(TALLOC_CTX *mem_ctx, 
				   struct registry_key *parent,
				   const char *name,
				   const struct nt_user_token *token,
				   uint32 access_desired,
				   struct registry_key **pregkey)
{
	WERROR     	result = WERR_OK;
	struct registry_key *regkey;
	struct registry_key_handle *key;
	struct regsubkey_ctr	*subkeys = NULL;

	DEBUG(7,("regkey_open_onelevel: name = [%s]\n", name));

	SMB_ASSERT(strchr(name, '\\') == NULL);

	if (!(regkey = TALLOC_ZERO_P(mem_ctx, struct registry_key)) ||
	    !(regkey->token = dup_nt_token(regkey, token)) ||
	    !(regkey->key = TALLOC_ZERO_P(regkey, struct registry_key_handle)))
	{
		result = WERR_NOMEM;
		goto done;
	}

	if ( !(W_ERROR_IS_OK(result = regdb_open())) ) {
		goto done;
	}

	key = regkey->key;
	talloc_set_destructor(key, regkey_destructor);
		
	/* initialization */
	
	key->type = REG_KEY_GENERIC;

	if (name[0] == '\0') {
		/*
		 * Open a copy of the parent key
		 */
		if (!parent) {
			result = WERR_BADFILE;
			goto done;
		}
		key->name = talloc_strdup(key, parent->key->name);
	}
	else {
		/*
		 * Normal subkey open
		 */
		key->name = talloc_asprintf(key, "%s%s%s",
					    parent ? parent->key->name : "",
					    parent ? "\\": "",
					    name);
	}

	if (key->name == NULL) {
		result = WERR_NOMEM;
		goto done;
	}

	/* Tag this as a Performance Counter Key */

	if( StrnCaseCmp(key->name, KEY_HKPD, strlen(KEY_HKPD)) == 0 )
		key->type = REG_KEY_HKPD;
	
	/* Look up the table of registry I/O operations */

	if ( !(key->ops = reghook_cache_find( key->name )) ) {
		DEBUG(0,("reg_open_onelevel: Failed to assign "
			 "registry_ops to [%s]\n", key->name ));
		result = WERR_BADFILE;
		goto done;
	}

	/* check if the path really exists; failed is indicated by -1 */
	/* if the subkey count failed, bail out */

	result = regsubkey_ctr_init(key, &subkeys);
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	if ( fetch_reg_keys( key, subkeys ) == -1 )  {
		result = WERR_BADFILE;
		goto done;
	}

	TALLOC_FREE( subkeys );

	if ( !regkey_access_check( key, access_desired, &key->access_granted,
				   token ) ) {
		result = WERR_ACCESS_DENIED;
		goto done;
	}

	*pregkey = regkey;
	result = WERR_OK;
	
done:
	if ( !W_ERROR_IS_OK(result) ) {
		TALLOC_FREE(regkey);
	}

	return result;
}

WERROR reg_openhive(TALLOC_CTX *mem_ctx, const char *hive,
		    uint32 desired_access,
		    const struct nt_user_token *token,
		    struct registry_key **pkey)
{
	SMB_ASSERT(hive != NULL);
	SMB_ASSERT(hive[0] != '\0');
	SMB_ASSERT(strchr(hive, '\\') == NULL);

	return regkey_open_onelevel(mem_ctx, NULL, hive, token, desired_access,
				    pkey);
}


/**********************************************************************
 * The API functions
 **********************************************************************/

WERROR reg_openkey(TALLOC_CTX *mem_ctx, struct registry_key *parent,
		   const char *name, uint32 desired_access,
		   struct registry_key **pkey)
{
	struct registry_key *direct_parent = parent;
	WERROR err;
	char *p, *path, *to_free;
	size_t len;

	if (!(path = SMB_STRDUP(name))) {
		return WERR_NOMEM;
	}
	to_free = path;

	len = strlen(path);

	if ((len > 0) && (path[len-1] == '\\')) {
		path[len-1] = '\0';
	}

	while ((p = strchr(path, '\\')) != NULL) {
		char *name_component;
		struct registry_key *tmp;

		if (!(name_component = SMB_STRNDUP(path, (p - path)))) {
			err = WERR_NOMEM;
			goto error;
		}

		err = regkey_open_onelevel(mem_ctx, direct_parent,
					   name_component, parent->token,
					   KEY_ENUMERATE_SUB_KEYS, &tmp);
		SAFE_FREE(name_component);

		if (!W_ERROR_IS_OK(err)) {
			goto error;
		}
		if (direct_parent != parent) {
			TALLOC_FREE(direct_parent);
		}

		direct_parent = tmp;
		path = p+1;
	}

	err = regkey_open_onelevel(mem_ctx, direct_parent, path, parent->token,
				   desired_access, pkey);
 error:
	if (direct_parent != parent) {
		TALLOC_FREE(direct_parent);
	}
	SAFE_FREE(to_free);
	return err;
}

WERROR reg_enumkey(TALLOC_CTX *mem_ctx, struct registry_key *key,
		   uint32 idx, char **name, NTTIME *last_write_time)
{
	WERROR err;

	if (!(key->key->access_granted & KEY_ENUMERATE_SUB_KEYS)) {
		return WERR_ACCESS_DENIED;
	}

	if (!W_ERROR_IS_OK(err = fill_subkey_cache(key))) {
		return err;
	}

	if (idx >= regsubkey_ctr_numkeys(key->subkeys)) {
		return WERR_NO_MORE_ITEMS;
	}

	if (!(*name = talloc_strdup(mem_ctx,
			regsubkey_ctr_specific_key(key->subkeys, idx))))
	{
		return WERR_NOMEM;
	}

	if (last_write_time) {
		*last_write_time = 0;
	}

	return WERR_OK;
}

WERROR reg_enumvalue(TALLOC_CTX *mem_ctx, struct registry_key *key,
		     uint32 idx, char **pname, struct registry_value **pval)
{
	struct registry_value *val;
	WERROR err;

	if (!(key->key->access_granted & KEY_QUERY_VALUE)) {
		return WERR_ACCESS_DENIED;
	}

	if (!(W_ERROR_IS_OK(err = fill_value_cache(key)))) {
		return err;
	}

	if (idx >= key->values->num_values) {
		return WERR_NO_MORE_ITEMS;
	}

	err = registry_pull_value(mem_ctx, &val,
				  key->values->values[idx]->type,
				  key->values->values[idx]->data_p,
				  key->values->values[idx]->size,
				  key->values->values[idx]->size);
	if (!W_ERROR_IS_OK(err)) {
		return err;
	}

	if (pname
	    && !(*pname = talloc_strdup(
			 mem_ctx, key->values->values[idx]->valuename))) {
		SAFE_FREE(val);
		return WERR_NOMEM;
	}

	*pval = val;
	return WERR_OK;
}

WERROR reg_queryvalue(TALLOC_CTX *mem_ctx, struct registry_key *key,
		      const char *name, struct registry_value **pval)
{
	WERROR err;
	uint32 i;

	if (!(key->key->access_granted & KEY_QUERY_VALUE)) {
		return WERR_ACCESS_DENIED;
	}

	if (!(W_ERROR_IS_OK(err = fill_value_cache(key)))) {
		return err;
	}

	for (i=0; i<key->values->num_values; i++) {
		if (strequal(key->values->values[i]->valuename, name)) {
			return reg_enumvalue(mem_ctx, key, i, NULL, pval);
		}
	}

	return WERR_BADFILE;
}

WERROR reg_queryinfokey(struct registry_key *key, uint32_t *num_subkeys,
			uint32_t *max_subkeylen, uint32_t *max_subkeysize, 
			uint32_t *num_values, uint32_t *max_valnamelen, 
			uint32_t *max_valbufsize, uint32_t *secdescsize,
			NTTIME *last_changed_time)
{
	uint32 i, max_size;
	size_t max_len;
	TALLOC_CTX *mem_ctx;
	WERROR err;
	struct security_descriptor *secdesc;

	if (!(key->key->access_granted & KEY_QUERY_VALUE)) {
		return WERR_ACCESS_DENIED;
	}

	if (!W_ERROR_IS_OK(fill_subkey_cache(key)) ||
	    !W_ERROR_IS_OK(fill_value_cache(key))) {
		return WERR_BADFILE;
	}

	max_len = 0;
	for (i=0; i< regsubkey_ctr_numkeys(key->subkeys); i++) {
		max_len = MAX(max_len,
			strlen(regsubkey_ctr_specific_key(key->subkeys, i)));
	}

	*num_subkeys = regsubkey_ctr_numkeys(key->subkeys);
	*max_subkeylen = max_len;
	*max_subkeysize = 0;	/* Class length? */

	max_len = 0;
	max_size = 0;
	for (i=0; i<key->values->num_values; i++) {
		max_len = MAX(max_len,
			      strlen(key->values->values[i]->valuename));
		max_size = MAX(max_size, key->values->values[i]->size);
	}

	*num_values = key->values->num_values;
	*max_valnamelen = max_len;
	*max_valbufsize = max_size;

	if (!(mem_ctx = talloc_new(key))) {
		return WERR_NOMEM;
	}

	err = regkey_get_secdesc(mem_ctx, key->key, &secdesc);
	if (!W_ERROR_IS_OK(err)) {
		TALLOC_FREE(mem_ctx);
		return err;
	}

	*secdescsize = ndr_size_security_descriptor(secdesc, NULL, 0);
	TALLOC_FREE(mem_ctx);

	*last_changed_time = 0;

	return WERR_OK;
}

WERROR reg_createkey(TALLOC_CTX *ctx, struct registry_key *parent,
		     const char *subkeypath, uint32 desired_access,
		     struct registry_key **pkey,
		     enum winreg_CreateAction *paction)
{
	struct registry_key *key = parent;
	struct registry_key *create_parent;
	TALLOC_CTX *mem_ctx;
	char *path, *end;
	WERROR err;

	/*
	 * We must refuse to handle subkey-paths containing
	 * a '/' character because at a lower level, after
	 * normalization, '/' is treated as a key separator
	 * just like '\\'.
	 */
	if (strchr(subkeypath, '/') != NULL) {
		return WERR_INVALID_PARAM;
	}

	if (!(mem_ctx = talloc_new(ctx))) return WERR_NOMEM;

	if (!(path = talloc_strdup(mem_ctx, subkeypath))) {
		err = WERR_NOMEM;
		goto done;
	}

	while ((end = strchr(path, '\\')) != NULL) {
		struct registry_key *tmp;
		enum winreg_CreateAction action;

		*end = '\0';

		err = reg_createkey(mem_ctx, key, path,
				    KEY_ENUMERATE_SUB_KEYS, &tmp, &action);
		if (!W_ERROR_IS_OK(err)) {
			goto done;
		}

		if (key != parent) {
			TALLOC_FREE(key);
		}

		key = tmp;
		path = end+1;
	}

	/*
	 * At this point, "path" contains the one-element subkey of "key". We
	 * can try to open it.
	 */

	err = reg_openkey(ctx, key, path, desired_access, pkey);
	if (W_ERROR_IS_OK(err)) {
		if (paction != NULL) {
			*paction = REG_OPENED_EXISTING_KEY;
		}
		goto done;
	}

	if (!W_ERROR_EQUAL(err, WERR_BADFILE)) {
		/*
		 * Something but "notfound" has happened, so bail out
		 */
		goto done;
	}

	/*
	 * We have to make a copy of the current key, as we opened it only
	 * with ENUM_SUBKEY access.
	 */

	err = reg_openkey(mem_ctx, key, "", KEY_CREATE_SUB_KEY,
			  &create_parent);
	if (!W_ERROR_IS_OK(err)) {
		goto done;
	}

	/*
	 * Actually create the subkey
	 */

	err = fill_subkey_cache(create_parent);
	if (!W_ERROR_IS_OK(err)) goto done;

	err = create_reg_subkey(key->key, path);
	W_ERROR_NOT_OK_GOTO_DONE(err);

	/*
	 * Now open the newly created key
	 */

	err = reg_openkey(ctx, create_parent, path, desired_access, pkey);
	if (W_ERROR_IS_OK(err) && (paction != NULL)) {
		*paction = REG_CREATED_NEW_KEY;
	}

 done:
	TALLOC_FREE(mem_ctx);
	return err;
}

WERROR reg_deletekey(struct registry_key *parent, const char *path)
{
	WERROR err;
	char *name, *end;
	struct registry_key *tmp_key, *key;
	TALLOC_CTX *mem_ctx = talloc_stackframe();

	name = talloc_strdup(mem_ctx, path);
	if (name == NULL) {
		err = WERR_NOMEM;
		goto done;
	}

	/* check if the key has subkeys */
	err = reg_openkey(mem_ctx, parent, name, REG_KEY_READ, &key);
	W_ERROR_NOT_OK_GOTO_DONE(err);

	err = fill_subkey_cache(key);
	W_ERROR_NOT_OK_GOTO_DONE(err);

	if (regsubkey_ctr_numkeys(key->subkeys) > 0) {
		err = WERR_ACCESS_DENIED;
		goto done;
	}

	/* no subkeys - proceed with delete */
	end = strrchr(name, '\\');
	if (end != NULL) {
		*end = '\0';

		err = reg_openkey(mem_ctx, parent, name,
				  KEY_CREATE_SUB_KEY, &tmp_key);
		W_ERROR_NOT_OK_GOTO_DONE(err);

		parent = tmp_key;
		name = end+1;
	}

	if (name[0] == '\0') {
		err = WERR_INVALID_PARAM;
		goto done;
	}

	err = delete_reg_subkey(parent->key, name);

done:
	TALLOC_FREE(mem_ctx);
	return err;
}

WERROR reg_setvalue(struct registry_key *key, const char *name,
		    const struct registry_value *val)
{
	WERROR err;
	DATA_BLOB value_data;
	int res;

	if (!(key->key->access_granted & KEY_SET_VALUE)) {
		return WERR_ACCESS_DENIED;
	}

	if (!W_ERROR_IS_OK(err = fill_value_cache(key))) {
		return err;
	}

	err = registry_push_value(key, val, &value_data);
	if (!W_ERROR_IS_OK(err)) {
		return err;
	}

	res = regval_ctr_addvalue(key->values, name, val->type,
				  (char *)value_data.data, value_data.length);
	TALLOC_FREE(value_data.data);

	if (res == 0) {
		TALLOC_FREE(key->values);
		return WERR_NOMEM;
	}

	if (!store_reg_values(key->key, key->values)) {
		TALLOC_FREE(key->values);
		return WERR_REG_IO_FAILURE;
	}

	return WERR_OK;
}

static WERROR reg_value_exists(struct registry_key *key, const char *name)
{
	int i;

	for (i=0; i<key->values->num_values; i++) {
		if (strequal(key->values->values[i]->valuename, name)) {
			return WERR_OK;
		}
	}

	return WERR_BADFILE;
}

WERROR reg_deletevalue(struct registry_key *key, const char *name)
{
	WERROR err;

	if (!(key->key->access_granted & KEY_SET_VALUE)) {
		return WERR_ACCESS_DENIED;
	}

	if (!W_ERROR_IS_OK(err = fill_value_cache(key))) {
		return err;
	}

	err = reg_value_exists(key, name);
	if (!W_ERROR_IS_OK(err)) {
		return err;
	}

	regval_ctr_delvalue(key->values, name);

	if (!store_reg_values(key->key, key->values)) {
		TALLOC_FREE(key->values);
		return WERR_REG_IO_FAILURE;
	}

	return WERR_OK;
}

WERROR reg_getkeysecurity(TALLOC_CTX *mem_ctx, struct registry_key *key,
			  struct security_descriptor **psecdesc)
{
	return regkey_get_secdesc(mem_ctx, key->key, psecdesc);
}

WERROR reg_setkeysecurity(struct registry_key *key,
			  struct security_descriptor *psecdesc)
{
	return regkey_set_secdesc(key->key, psecdesc);
}

WERROR reg_getversion(uint32_t *version)
{
	if (version == NULL) {
		return WERR_INVALID_PARAM;
	}

	*version = 0x00000005; /* Windows 2000 registry API version */
	return WERR_OK;
}

/*******************************************************************
 Note: topkeypat is the *full* path that this *key will be
 loaded into (including the name of the key)
 ********************************************************************/

static WERROR reg_load_tree(REGF_FILE *regfile, const char *topkeypath,
			    REGF_NK_REC *key)
{
	REGF_NK_REC *subkey;
	struct registry_key_handle registry_key;
	struct regval_ctr *values;
	struct regsubkey_ctr *subkeys;
	int i;
	char *path = NULL;
	WERROR result = WERR_OK;

	/* initialize the struct registry_key_handle structure */

	registry_key.ops = reghook_cache_find(topkeypath);
	if (!registry_key.ops) {
		DEBUG(0, ("reg_load_tree: Failed to assign registry_ops "
			  "to [%s]\n", topkeypath));
		return WERR_BADFILE;
	}

	registry_key.name = talloc_strdup(regfile->mem_ctx, topkeypath);
	if (!registry_key.name) {
		DEBUG(0, ("reg_load_tree: Talloc failed for reg_key.name!\n"));
		return WERR_NOMEM;
	}

	/* now start parsing the values and subkeys */

	result = regsubkey_ctr_init(regfile->mem_ctx, &subkeys);
	W_ERROR_NOT_OK_RETURN(result);

	values = TALLOC_ZERO_P(subkeys, struct regval_ctr);
	if (values == NULL) {
		return WERR_NOMEM;
	}

	/* copy values into the struct regval_ctr */

	for (i=0; i<key->num_values; i++) {
		regval_ctr_addvalue(values, key->values[i].valuename,
				    key->values[i].type,
				    (char*)key->values[i].data,
				    (key->values[i].data_size & ~VK_DATA_IN_OFFSET));
	}

	/* copy subkeys into the struct regsubkey_ctr */

	key->subkey_index = 0;
	while ((subkey = regfio_fetch_subkey( regfile, key ))) {
		result = regsubkey_ctr_addkey(subkeys, subkey->keyname);
		if (!W_ERROR_IS_OK(result)) {
			TALLOC_FREE(subkeys);
			return result;
		}
	}

	/* write this key and values out */

	if (!store_reg_values(&registry_key, values)
	    || !store_reg_keys(&registry_key, subkeys))
	{
		DEBUG(0,("reg_load_tree: Failed to load %s!\n", topkeypath));
		result = WERR_REG_IO_FAILURE;
	}

	TALLOC_FREE(subkeys);

	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	/* now continue to load each subkey registry tree */

	key->subkey_index = 0;
	while ((subkey = regfio_fetch_subkey(regfile, key))) {
		path = talloc_asprintf(regfile->mem_ctx,
				       "%s\\%s",
				       topkeypath,
				       subkey->keyname);
		if (path == NULL) {
			return WERR_NOMEM;
		}
		result = reg_load_tree(regfile, path, subkey);
		if (!W_ERROR_IS_OK(result)) {
			break;
		}
	}

	return result;
}

/*******************************************************************
 ********************************************************************/

static WERROR restore_registry_key(struct registry_key_handle *krecord,
				   const char *fname)
{
	REGF_FILE *regfile;
	REGF_NK_REC *rootkey;
	WERROR result;

	/* open the registry file....fail if the file already exists */

	regfile = regfio_open(fname, (O_RDONLY), 0);
	if (regfile == NULL) {
		DEBUG(0, ("restore_registry_key: failed to open \"%s\" (%s)\n",
			  fname, strerror(errno)));
		return ntstatus_to_werror(map_nt_error_from_unix(errno));
	}

	/* get the rootkey from the regf file and then load the tree
	   via recursive calls */

	if (!(rootkey = regfio_rootkey(regfile))) {
		regfio_close(regfile);
		return WERR_REG_FILE_INVALID;
	}

	result = reg_load_tree(regfile, krecord->name, rootkey);

	/* cleanup */

	regfio_close(regfile);

	return result;
}

WERROR reg_restorekey(struct registry_key *key, const char *fname)
{
	return restore_registry_key(key->key, fname);
}

/********************************************************************
********************************************************************/

static WERROR reg_write_tree(REGF_FILE *regfile, const char *keypath,
			     REGF_NK_REC *parent)
{
	REGF_NK_REC *key;
	struct regval_ctr *values;
	struct regsubkey_ctr *subkeys;
	int i, num_subkeys;
	char *key_tmp = NULL;
	char *keyname, *parentpath;
	char *subkeypath = NULL;
	char *subkeyname;
	struct registry_key_handle registry_key;
	WERROR result = WERR_OK;
	SEC_DESC *sec_desc = NULL;

	if (!regfile) {
		return WERR_GENERAL_FAILURE;
	}

	if (!keypath) {
		return WERR_OBJECT_PATH_INVALID;
	}

	/* split up the registry key path */

	key_tmp = talloc_strdup(regfile->mem_ctx, keypath);
	if (!key_tmp) {
		return WERR_NOMEM;
	}
	if (!reg_split_key(key_tmp, &parentpath, &keyname)) {
		return WERR_OBJECT_PATH_INVALID;
	}

	if (!keyname) {
		keyname = parentpath;
	}

	/* we need a registry_key_handle object here to enumerate subkeys and values */

	ZERO_STRUCT(registry_key);

	registry_key.name = talloc_strdup(regfile->mem_ctx, keypath);
	if (registry_key.name == NULL) {
		return WERR_NOMEM;
	}

	registry_key.ops = reghook_cache_find(registry_key.name);
	if (registry_key.ops == NULL) {
		return WERR_BADFILE;
	}

	/* lookup the values and subkeys */

	result = regsubkey_ctr_init(regfile->mem_ctx, &subkeys);
	W_ERROR_NOT_OK_RETURN(result);

	values = TALLOC_ZERO_P(subkeys, struct regval_ctr);
	if (values == NULL) {
		return WERR_NOMEM;
	}

	fetch_reg_keys(&registry_key, subkeys);
	fetch_reg_values(&registry_key, values);

	result = regkey_get_secdesc(regfile->mem_ctx, &registry_key, &sec_desc);
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	/* write out this key */

	key = regfio_write_key(regfile, keyname, values, subkeys, sec_desc,
			       parent);
	if (key == NULL) {
		result = WERR_CAN_NOT_COMPLETE;
		goto done;
	}

	/* write each one of the subkeys out */

	num_subkeys = regsubkey_ctr_numkeys(subkeys);
	for (i=0; i<num_subkeys; i++) {
		subkeyname = regsubkey_ctr_specific_key(subkeys, i);
		subkeypath = talloc_asprintf(regfile->mem_ctx, "%s\\%s",
					     keypath, subkeyname);
		if (subkeypath == NULL) {
			result = WERR_NOMEM;
			goto done;
		}
		result = reg_write_tree(regfile, subkeypath, key);
		if (!W_ERROR_IS_OK(result))
			goto done;
	}

	DEBUG(6, ("reg_write_tree: wrote key [%s]\n", keypath));

done:
	TALLOC_FREE(subkeys);
	TALLOC_FREE(registry_key.name);

	return result;
}

static WERROR backup_registry_key(struct registry_key_handle *krecord,
				  const char *fname)
{
	REGF_FILE *regfile;
	WERROR result;

	/* open the registry file....fail if the file already exists */

	regfile = regfio_open(fname, (O_RDWR|O_CREAT|O_EXCL),
			      (S_IRUSR|S_IWUSR));
	if (regfile == NULL) {
		DEBUG(0,("backup_registry_key: failed to open \"%s\" (%s)\n",
			 fname, strerror(errno) ));
		return ntstatus_to_werror(map_nt_error_from_unix(errno));
	}

	/* write the registry tree to the file  */

	result = reg_write_tree(regfile, krecord->name, NULL);

	/* cleanup */

	regfio_close(regfile);

	return result;
}

WERROR reg_savekey(struct registry_key *key, const char *fname)
{
	return backup_registry_key(key->key, fname);
}

/**********************************************************************
 * Higher level utility functions
 **********************************************************************/

WERROR reg_deleteallvalues(struct registry_key *key)
{
	WERROR err;
	int i;

	if (!(key->key->access_granted & KEY_SET_VALUE)) {
		return WERR_ACCESS_DENIED;
	}

	if (!W_ERROR_IS_OK(err = fill_value_cache(key))) {
		return err;
	}

	for (i=0; i<key->values->num_values; i++) {
		regval_ctr_delvalue(key->values, key->values->values[i]->valuename);
	}

	if (!store_reg_values(key->key, key->values)) {
		TALLOC_FREE(key->values);
		return WERR_REG_IO_FAILURE;
	}

	return WERR_OK;
}

/*
 * Utility function to open a complete registry path including the hive prefix.
 */

WERROR reg_open_path(TALLOC_CTX *mem_ctx, const char *orig_path,
		     uint32 desired_access, const struct nt_user_token *token,
		     struct registry_key **pkey)
{
	struct registry_key *hive, *key;
	char *path, *p;
	WERROR err;

	if (!(path = SMB_STRDUP(orig_path))) {
		return WERR_NOMEM;
	}

	p = strchr(path, '\\');

	if ((p == NULL) || (p[1] == '\0')) {
		/*
		 * No key behind the hive, just return the hive
		 */

		err = reg_openhive(mem_ctx, path, desired_access, token,
				   &hive);
		if (!W_ERROR_IS_OK(err)) {
			SAFE_FREE(path);
			return err;
		}
		SAFE_FREE(path);
		*pkey = hive;
		return WERR_OK;
	}

	*p = '\0';

	err = reg_openhive(mem_ctx, path, KEY_ENUMERATE_SUB_KEYS, token,
			   &hive);
	if (!W_ERROR_IS_OK(err)) {
		SAFE_FREE(path);
		return err;
	}

	err = reg_openkey(mem_ctx, hive, p+1, desired_access, &key);

	TALLOC_FREE(hive);
	SAFE_FREE(path);

	if (!W_ERROR_IS_OK(err)) {
		return err;
	}

	*pkey = key;
	return WERR_OK;
}

/*
 * Utility function to delete a registry key with all its subkeys.
 * Note that reg_deletekey returns ACCESS_DENIED when called on a
 * key that has subkeys.
 */
static WERROR reg_deletekey_recursive_internal(TALLOC_CTX *ctx,
					       struct registry_key *parent,
					       const char *path,
					       bool del_key)
{
	TALLOC_CTX *mem_ctx = NULL;
	WERROR werr = WERR_OK;
	struct registry_key *key;
	char *subkey_name = NULL;
	uint32 i;

	mem_ctx = talloc_new(ctx);
	if (mem_ctx == NULL) {
		werr = WERR_NOMEM;
		goto done;
	}

	/* recurse through subkeys first */
	werr = reg_openkey(mem_ctx, parent, path, REG_KEY_ALL, &key);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	werr = fill_subkey_cache(key);
	W_ERROR_NOT_OK_GOTO_DONE(werr);

	/*
	 * loop from top to bottom for perfomance:
	 * this way, we need to rehash the regsubkey containers less
	 */
	for (i = regsubkey_ctr_numkeys(key->subkeys) ; i > 0; i--) {
		subkey_name = regsubkey_ctr_specific_key(key->subkeys, i-1);
		werr = reg_deletekey_recursive_internal(mem_ctx, key,
					subkey_name,
					true);
		W_ERROR_NOT_OK_GOTO_DONE(werr);
	}

	if (del_key) {
		/* now delete the actual key */
		werr = reg_deletekey(parent, path);
	}

done:
	TALLOC_FREE(mem_ctx);
	return werr;
}

static WERROR reg_deletekey_recursive_trans(TALLOC_CTX *ctx,
					    struct registry_key *parent,
					    const char *path,
					    bool del_key)
{
	WERROR werr;

	werr = regdb_transaction_start();
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(0, ("reg_deletekey_recursive_trans: "
			  "error starting transaction: %s\n",
			  win_errstr(werr)));
		return werr;
	}

	werr = reg_deletekey_recursive_internal(ctx, parent, path, del_key);

	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(1, (__location__ " failed to delete key '%s' from key "
			  "'%s': %s\n", path, parent->key->name,
			  win_errstr(werr)));
		werr = regdb_transaction_cancel();
		if (!W_ERROR_IS_OK(werr)) {
			DEBUG(0, ("reg_deletekey_recursive_trans: "
				  "error cancelling transaction: %s\n",
				  win_errstr(werr)));
		}
	} else {
		werr = regdb_transaction_commit();
		if (!W_ERROR_IS_OK(werr)) {
			DEBUG(0, ("reg_deletekey_recursive_trans: "
				  "error committing transaction: %s\n",
				  win_errstr(werr)));
		}
	}

	return werr;
}

WERROR reg_deletekey_recursive(TALLOC_CTX *ctx,
			       struct registry_key *parent,
			       const char *path)
{
	return reg_deletekey_recursive_trans(ctx, parent, path, true);
}

WERROR reg_deletesubkeys_recursive(TALLOC_CTX *ctx,
				   struct registry_key *parent,
				   const char *path)
{
	return reg_deletekey_recursive_trans(ctx, parent, path, false);
}

#if 0
/* these two functions are unused. */

/**
 * Utility function to create a registry key without opening the hive
 * before. Assumes the hive already exists.
 */

WERROR reg_create_path(TALLOC_CTX *mem_ctx, const char *orig_path,
		       uint32 desired_access,
		       const struct nt_user_token *token,
		       enum winreg_CreateAction *paction,
		       struct registry_key **pkey)
{
	struct registry_key *hive;
	char *path, *p;
	WERROR err;

	if (!(path = SMB_STRDUP(orig_path))) {
		return WERR_NOMEM;
	}

	p = strchr(path, '\\');

	if ((p == NULL) || (p[1] == '\0')) {
		/*
		 * No key behind the hive, just return the hive
		 */

		err = reg_openhive(mem_ctx, path, desired_access, token,
				   &hive);
		if (!W_ERROR_IS_OK(err)) {
			SAFE_FREE(path);
			return err;
		}
		SAFE_FREE(path);
		*pkey = hive;
		*paction = REG_OPENED_EXISTING_KEY;
		return WERR_OK;
	}

	*p = '\0';

	err = reg_openhive(mem_ctx, path,
			   (strchr(p+1, '\\') != NULL) ?
			   KEY_ENUMERATE_SUB_KEYS : KEY_CREATE_SUB_KEY,
			   token, &hive);
	if (!W_ERROR_IS_OK(err)) {
		SAFE_FREE(path);
		return err;
	}

	err = reg_createkey(mem_ctx, hive, p+1, desired_access, pkey, paction);
	SAFE_FREE(path);
	TALLOC_FREE(hive);
	return err;
}

/*
 * Utility function to create a registry key without opening the hive
 * before. Will not delete a hive.
 */

WERROR reg_delete_path(const struct nt_user_token *token,
		       const char *orig_path)
{
	struct registry_key *hive;
	char *path, *p;
	WERROR err;

	if (!(path = SMB_STRDUP(orig_path))) {
		return WERR_NOMEM;
	}

	p = strchr(path, '\\');

	if ((p == NULL) || (p[1] == '\0')) {
		SAFE_FREE(path);
		return WERR_INVALID_PARAM;
	}

	*p = '\0';

	err = reg_openhive(NULL, path,
			   (strchr(p+1, '\\') != NULL) ?
			   KEY_ENUMERATE_SUB_KEYS : KEY_CREATE_SUB_KEY,
			   token, &hive);
	if (!W_ERROR_IS_OK(err)) {
		SAFE_FREE(path);
		return err;
	}

	err = reg_deletekey(hive, p+1);
	SAFE_FREE(path);
	TALLOC_FREE(hive);
	return err;
}
#endif /* #if 0 */
