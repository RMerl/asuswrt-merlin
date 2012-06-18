/*
   Unix SMB/CIFS implementation.
   Transparent registry backend handling
   Copyright (C) Jelmer Vernooij			2003-2007.

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
#include "../lib/util/dlinklist.h"
#include "lib/registry/registry.h"
#include "system/filesys.h"


/**
 * @file
 * @brief Main registry functions
 */

const struct reg_predefined_key reg_predefined_keys[] = {
	{HKEY_CLASSES_ROOT,"HKEY_CLASSES_ROOT" },
	{HKEY_CURRENT_USER,"HKEY_CURRENT_USER" },
	{HKEY_LOCAL_MACHINE, "HKEY_LOCAL_MACHINE" },
	{HKEY_PERFORMANCE_DATA, "HKEY_PERFORMANCE_DATA" },
	{HKEY_USERS, "HKEY_USERS" },
	{HKEY_CURRENT_CONFIG, "HKEY_CURRENT_CONFIG" },
	{HKEY_DYN_DATA, "HKEY_DYN_DATA" },
	{HKEY_PERFORMANCE_TEXT, "HKEY_PERFORMANCE_TEXT" },
	{HKEY_PERFORMANCE_NLSTEXT, "HKEY_PERFORMANCE_NLSTEXT" },
	{ 0, NULL }
};

/** Obtain name of specific hkey. */
_PUBLIC_ const char *reg_get_predef_name(uint32_t hkey)
{
	int i;
	for (i = 0; reg_predefined_keys[i].name; i++) {
		if (reg_predefined_keys[i].handle == hkey)
			return reg_predefined_keys[i].name;
	}

	return NULL;
}

/** Get predefined key by name. */
_PUBLIC_ WERROR reg_get_predefined_key_by_name(struct registry_context *ctx,
					       const char *name,
					       struct registry_key **key)
{
	int i;

	for (i = 0; reg_predefined_keys[i].name; i++) {
		if (!strcasecmp(reg_predefined_keys[i].name, name))
			return reg_get_predefined_key(ctx,
						      reg_predefined_keys[i].handle,
						      key);
	}

	DEBUG(1, ("No predefined key with name '%s'\n", name));

	return WERR_BADFILE;
}

/** Get predefined key by id. */
_PUBLIC_ WERROR reg_get_predefined_key(struct registry_context *ctx,
				       uint32_t hkey, struct registry_key **key)
{
	return ctx->ops->get_predefined_key(ctx, hkey, key);
}

/**
 * Open a key
 * First tries to use the open_key function from the backend
 * then falls back to get_subkey_by_name and later get_subkey_by_index
 */
_PUBLIC_ WERROR reg_open_key(TALLOC_CTX *mem_ctx, struct registry_key *parent,
			     const char *name, struct registry_key **result)
{
	if (parent == NULL) {
		DEBUG(0, ("Invalid parent key specified for open of '%s'\n",
			name));
		return WERR_INVALID_PARAM;
	}

	if (parent->context->ops->open_key == NULL) {
		DEBUG(0, ("Registry backend doesn't have open_key!\n"));
		return WERR_NOT_SUPPORTED;
	}

	return parent->context->ops->open_key(mem_ctx, parent, name, result);
}

/**
 * Get value by index
 */
_PUBLIC_ WERROR reg_key_get_value_by_index(TALLOC_CTX *mem_ctx,
					   const struct registry_key *key,
					   uint32_t idx, const char **name,
					   uint32_t *type, DATA_BLOB *data)
{
	if (key == NULL)
		return WERR_INVALID_PARAM;

	if (key->context->ops->enum_value == NULL)
		return WERR_NOT_SUPPORTED;

	return key->context->ops->enum_value(mem_ctx, key, idx, name,
					     type, data);
}

/**
 * Get the number of subkeys.
 */
_PUBLIC_ WERROR reg_key_get_info(TALLOC_CTX *mem_ctx,
				 const struct registry_key *key,
				 const char **classname,
				 uint32_t *num_subkeys,
				 uint32_t *num_values,
				 NTTIME *last_change_time,
				 uint32_t *max_subkeynamelen,
				 uint32_t *max_valnamelen,
				 uint32_t *max_valbufsize)
{
	if (key == NULL)
		return WERR_INVALID_PARAM;

	if (key->context->ops->get_key_info == NULL)
		return WERR_NOT_SUPPORTED;

	return key->context->ops->get_key_info(mem_ctx,
					       key, classname, num_subkeys,
					       num_values, last_change_time,
					       max_subkeynamelen,
					       max_valnamelen, max_valbufsize);
}

/**
 * Get subkey by index.
 */
_PUBLIC_ WERROR reg_key_get_subkey_by_index(TALLOC_CTX *mem_ctx,
					    const struct registry_key *key,
					    int idx, const char **name,
					    const char **keyclass,
					    NTTIME *last_changed_time)
{
	if (key == NULL)
		return WERR_INVALID_PARAM;

	if (key->context->ops->enum_key == NULL)
		return WERR_NOT_SUPPORTED;

	return key->context->ops->enum_key(mem_ctx, key, idx, name,
					   keyclass, last_changed_time);
}

/**
 * Get value by name.
 */
_PUBLIC_ WERROR reg_key_get_value_by_name(TALLOC_CTX *mem_ctx,
					  const struct registry_key *key,
					  const char *name,
					  uint32_t *type,
					  DATA_BLOB *data)
{
	if (key == NULL)
		return WERR_INVALID_PARAM;

	if (key->context->ops->get_value == NULL)
		return WERR_NOT_SUPPORTED;

	return key->context->ops->get_value(mem_ctx, key, name, type, data);
}

/**
 * Delete a key.
 */
_PUBLIC_ WERROR reg_key_del(struct registry_key *parent, const char *name)
{
	if (parent == NULL)
		return WERR_INVALID_PARAM;

	if (parent->context->ops->delete_key == NULL)
		return WERR_NOT_SUPPORTED;

	return parent->context->ops->delete_key(parent, name);
}

/**
 * Add a key.
 */
_PUBLIC_ WERROR reg_key_add_name(TALLOC_CTX *mem_ctx,
				 struct registry_key *parent,
				 const char *name, const char *key_class,
				 struct security_descriptor *desc,
				 struct registry_key **newkey)
{
	if (parent == NULL)
		return WERR_INVALID_PARAM;

	if (parent->context->ops->create_key == NULL) {
		DEBUG(1, ("Backend '%s' doesn't support method add_key\n",
				  parent->context->ops->name));
		return WERR_NOT_SUPPORTED;
	}

	return parent->context->ops->create_key(mem_ctx, parent, name,
						key_class, desc, newkey);
}

/**
 * Set a value.
 */
_PUBLIC_ WERROR reg_val_set(struct registry_key *key, const char *value,
			    uint32_t type, const DATA_BLOB data)
{
	if (key == NULL)
		return WERR_INVALID_PARAM;

	/* A 'real' set function has preference */
	if (key->context->ops->set_value == NULL) {
		DEBUG(1, ("Backend '%s' doesn't support method set_value\n",
				  key->context->ops->name));
		return WERR_NOT_SUPPORTED;
	}

	return key->context->ops->set_value(key, value, type, data);
}

/**
 * Get the security descriptor on a key.
 */
_PUBLIC_ WERROR reg_get_sec_desc(TALLOC_CTX *ctx,
				 const struct registry_key *key,
				 struct security_descriptor **secdesc)
{
	if (key == NULL)
		return WERR_INVALID_PARAM;

	/* A 'real' set function has preference */
	if (key->context->ops->get_sec_desc == NULL)
		return WERR_NOT_SUPPORTED;

	return key->context->ops->get_sec_desc(ctx, key, secdesc);
}

/**
 * Delete a value.
 */
_PUBLIC_ WERROR reg_del_value(struct registry_key *key, const char *valname)
{
	if (key == NULL)
		return WERR_INVALID_PARAM;

	if (key->context->ops->delete_value == NULL)
		return WERR_NOT_SUPPORTED;

	return key->context->ops->delete_value(key, valname);
}

/**
 * Flush a key to disk.
 */
_PUBLIC_ WERROR reg_key_flush(struct registry_key *key)
{
	if (key == NULL)
		return WERR_INVALID_PARAM;

	if (key->context->ops->flush_key == NULL)
		return WERR_NOT_SUPPORTED;

	return key->context->ops->flush_key(key);
}

_PUBLIC_ WERROR reg_set_sec_desc(struct registry_key *key,
				 const struct security_descriptor *security)
{
	if (key == NULL)
		return WERR_INVALID_PARAM;

	if (key->context->ops->set_sec_desc == NULL)
		return WERR_NOT_SUPPORTED;

	return key->context->ops->set_sec_desc(key, security);
}
