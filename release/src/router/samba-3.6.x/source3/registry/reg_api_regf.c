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

#include "includes.h"
#include "system/filesys.h"
#include "registry.h"
#include "reg_api_regf.h"
#include "reg_cachehook.h"
#include "regfio.h"
#include "reg_util_internal.h"
#include "reg_dispatcher.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_REGISTRY

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

	result = regval_ctr_init(subkeys, &values);
	W_ERROR_NOT_OK_RETURN(result);

	/* copy values into the struct regval_ctr */

	for (i=0; i<key->num_values; i++) {
		regval_ctr_addvalue(values, key->values[i].valuename,
				    key->values[i].type,
				    key->values[i].data,
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
	struct security_descriptor *sec_desc = NULL;

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

	result = regval_ctr_init(subkeys, &values);
	W_ERROR_NOT_OK_RETURN(result);

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
