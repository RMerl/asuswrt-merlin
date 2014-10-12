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
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "includes.h"
#include "../lib/util/dlinklist.h"
#include "lib/registry/registry.h"
#include "system/filesys.h"

struct reg_key_path {
	uint32_t predefined_key;
	const char **elements;
};

struct registry_local {
	const struct registry_operations *ops;

	struct mountpoint {
		struct reg_key_path path;
		struct hive_key *key;
		struct mountpoint *prev, *next;
	} *mountpoints;
};

struct local_key {
	struct registry_key global;
	struct reg_key_path path;
	struct hive_key *hive_key;
};


struct registry_key *reg_import_hive_key(struct registry_context *ctx,
					 struct hive_key *hive,
					 uint32_t predefined_key,
					 const char **elements)
{
	struct local_key *local_key;
	struct reg_key_path parent_path;

	parent_path.predefined_key = predefined_key;
	parent_path.elements = elements;

	local_key = talloc(ctx, struct local_key);
	if (local_key != NULL) {
		local_key->hive_key = talloc_reference(local_key, hive);
		local_key->global.context = talloc_reference(local_key, ctx);
		local_key->path = parent_path;
	}

	return (struct registry_key *)local_key;
}


static WERROR local_open_key(TALLOC_CTX *mem_ctx,
			     struct registry_key *parent,
			     const char *path,
			     struct registry_key **result)
{
	char *orig, *curbegin, *curend;
	struct local_key *local_parent = talloc_get_type(parent,
							 struct local_key);
	struct hive_key *curkey = local_parent->hive_key;
	WERROR error;
	const char **elements = NULL;
	int el;

	if (path == NULL) {
		return WERR_INVALID_PARAM;
	}

	orig = talloc_strdup(mem_ctx, path);
	W_ERROR_HAVE_NO_MEMORY(orig);
	curbegin = orig;
	curend = strchr(orig, '\\');

	if (local_parent->path.elements != NULL) {
		elements = talloc_array(mem_ctx, const char *,
					str_list_length(local_parent->path.elements) + 1);
		W_ERROR_HAVE_NO_MEMORY(elements);
		for (el = 0; local_parent->path.elements[el] != NULL; el++) {
			elements[el] = talloc_reference(elements,
							local_parent->path.elements[el]);
		}
		elements[el] = NULL;
	} else {
		elements = NULL;
		el = 0;
	}

	while (curbegin != NULL && *curbegin) {
		if (curend != NULL)
			*curend = '\0';
		elements = talloc_realloc(mem_ctx, elements, const char *, el+2);
		W_ERROR_HAVE_NO_MEMORY(elements);
		elements[el] = talloc_strdup(elements, curbegin);
		W_ERROR_HAVE_NO_MEMORY(elements[el]);
		el++;
		elements[el] = NULL;
		error = hive_get_key_by_name(mem_ctx, curkey,
					     curbegin, &curkey);
		if (!W_ERROR_IS_OK(error)) {
			DEBUG(2, ("Opening key %s failed: %s\n", curbegin,
				win_errstr(error)));
			talloc_free(orig);
			return error;
		}
		if (curend == NULL)
			break;
		curbegin = curend + 1;
		curend = strchr(curbegin, '\\');
	}
	talloc_free(orig);

	*result = reg_import_hive_key(local_parent->global.context, curkey,
				      local_parent->path.predefined_key,
				      talloc_steal(curkey, elements));

	return WERR_OK;
}

WERROR local_get_predefined_key(struct registry_context *ctx,
				uint32_t key_id, struct registry_key **key)
{
	struct registry_local *rctx = talloc_get_type(ctx,
						      struct registry_local);
	struct mountpoint *mp;

	for (mp = rctx->mountpoints; mp != NULL; mp = mp->next) {
		if (mp->path.predefined_key == key_id &&
			mp->path.elements == NULL)
			break;
	}

	if (mp == NULL)
		return WERR_BADFILE;

	*key = reg_import_hive_key(ctx, mp->key,
				   mp->path.predefined_key,
				   mp->path.elements);

	return WERR_OK;
}

static WERROR local_enum_key(TALLOC_CTX *mem_ctx,
			     const struct registry_key *key, uint32_t idx,
			     const char **name,
			     const char **keyclass,
			     NTTIME *last_changed_time)
{
	const struct local_key *local = (const struct local_key *)key;

	return hive_enum_key(mem_ctx, local->hive_key, idx, name, keyclass,
			     last_changed_time);
}

static WERROR local_create_key(TALLOC_CTX *mem_ctx,
			       struct registry_key *parent,
			       const char *path,
			       const char *key_class,
			       struct security_descriptor *security,
			       struct registry_key **result)
{
	char *orig, *curbegin, *curend;
	struct local_key *local_parent = talloc_get_type(parent,
							 struct local_key);
	struct hive_key *curkey = local_parent->hive_key;
	WERROR error;
	const char **elements = NULL;
	int el;

	if (path == NULL) {
		return WERR_INVALID_PARAM;
	}

	orig = talloc_strdup(mem_ctx, path);
	W_ERROR_HAVE_NO_MEMORY(orig);
	curbegin = orig;
	curend = strchr(orig, '\\');

	if (local_parent->path.elements != NULL) {
		elements = talloc_array(mem_ctx, const char *,
					str_list_length(local_parent->path.elements) + 1);
		W_ERROR_HAVE_NO_MEMORY(elements);
		for (el = 0; local_parent->path.elements[el] != NULL; el++) {
			elements[el] = talloc_reference(elements,
							local_parent->path.elements[el]);
		}
		elements[el] = NULL;
	} else {
		elements = NULL;
		el = 0;
	}

	while (curbegin != NULL && *curbegin) {
		if (curend != NULL)
			*curend = '\0';
		elements = talloc_realloc(mem_ctx, elements, const char *, el+2);
		W_ERROR_HAVE_NO_MEMORY(elements);
		elements[el] = talloc_strdup(elements, curbegin);
		W_ERROR_HAVE_NO_MEMORY(elements[el]);
		el++;
		elements[el] = NULL;
		error = hive_get_key_by_name(mem_ctx, curkey,
					     curbegin, &curkey);
		if (W_ERROR_EQUAL(error, WERR_BADFILE)) {
			error = hive_key_add_name(mem_ctx, curkey, curbegin,
						  key_class, security,
						  &curkey);
		}
		if (!W_ERROR_IS_OK(error)) {
			DEBUG(2, ("Open/Creation of key %s failed: %s\n",
				curbegin, win_errstr(error)));
			talloc_free(orig);
			return error;
		}
		if (curend == NULL)
			break;
		curbegin = curend + 1;
		curend = strchr(curbegin, '\\');
	}
	talloc_free(orig);

	*result = reg_import_hive_key(local_parent->global.context, curkey,
				      local_parent->path.predefined_key,
				      talloc_steal(curkey, elements));

	return WERR_OK;
}

static WERROR local_set_value(struct registry_key *key, const char *name,
			      uint32_t type, const DATA_BLOB data)
{
	struct local_key *local = (struct local_key *)key;

	if (name == NULL) {
		return WERR_INVALID_PARAM;
	}

	return hive_key_set_value(local->hive_key, name, type, data);
}

static WERROR local_get_value(TALLOC_CTX *mem_ctx,
			      const struct registry_key *key,
			      const char *name, uint32_t *type, DATA_BLOB *data)
{
	const struct local_key *local = (const struct local_key *)key;

	if (name == NULL) {
		return WERR_INVALID_PARAM;
	}

	return hive_get_value(mem_ctx, local->hive_key, name, type, data);
}

static WERROR local_enum_value(TALLOC_CTX *mem_ctx,
			       const struct registry_key *key, uint32_t idx,
			       const char **name,
			       uint32_t *type,
			       DATA_BLOB *data)
{
	const struct local_key *local = (const struct local_key *)key;

	return hive_get_value_by_index(mem_ctx, local->hive_key, idx,
				       name, type, data);
}

static WERROR local_delete_key(TALLOC_CTX *mem_ctx, struct registry_key *key,
			       const char *name)
{
	const struct local_key *local = (const struct local_key *)key;

	if (name == NULL) {
		return WERR_INVALID_PARAM;
	}

	return hive_key_del(mem_ctx, local->hive_key, name);
}

static WERROR local_delete_value(TALLOC_CTX *mem_ctx, struct registry_key *key,
				 const char *name)
{
	const struct local_key *local = (const struct local_key *)key;

	if (name == NULL) {
		return WERR_INVALID_PARAM;
	}

	return hive_key_del_value(mem_ctx, local->hive_key, name);
}

static WERROR local_flush_key(struct registry_key *key)
{
	const struct local_key *local = (const struct local_key *)key;

	return hive_key_flush(local->hive_key);
}

static WERROR local_get_key_info(TALLOC_CTX *mem_ctx,
				 const struct registry_key *key,
				 const char **classname,
				 uint32_t *num_subkeys,
				 uint32_t *num_values,
				 NTTIME *last_change_time,
				 uint32_t *max_subkeynamelen,
				 uint32_t *max_valnamelen,
				 uint32_t *max_valbufsize)
{
	const struct local_key *local = (const struct local_key *)key;

	return hive_key_get_info(mem_ctx, local->hive_key,
				 classname, num_subkeys, num_values,
				 last_change_time, max_subkeynamelen, 
				 max_valnamelen, max_valbufsize);
}
static WERROR local_get_sec_desc(TALLOC_CTX *mem_ctx, 
				 const struct registry_key *key, 
				 struct security_descriptor **security)
{
	const struct local_key *local = (const struct local_key *)key;

	return hive_get_sec_desc(mem_ctx, local->hive_key, security);
}
static WERROR local_set_sec_desc(struct registry_key *key, 
				 const struct security_descriptor *security)
{
	const struct local_key *local = (const struct local_key *)key;

	return hive_set_sec_desc(local->hive_key, security);
}
const static struct registry_operations local_ops = {
	.name = "local",
	.open_key = local_open_key,
	.get_predefined_key = local_get_predefined_key,
	.enum_key = local_enum_key,
	.create_key = local_create_key,
	.set_value = local_set_value,
	.get_value = local_get_value,
	.enum_value = local_enum_value,
	.delete_key = local_delete_key,
	.delete_value = local_delete_value,
	.flush_key = local_flush_key,
	.get_key_info = local_get_key_info,
	.get_sec_desc = local_get_sec_desc,
	.set_sec_desc = local_set_sec_desc,
};

WERROR reg_open_local(TALLOC_CTX *mem_ctx, struct registry_context **ctx)
{
	struct registry_local *ret = talloc_zero(mem_ctx,
						 struct registry_local);

	W_ERROR_HAVE_NO_MEMORY(ret);

	ret->ops = &local_ops;

	*ctx = (struct registry_context *)ret;

	return WERR_OK;
}

WERROR reg_mount_hive(struct registry_context *rctx,
		      struct hive_key *hive_key,
		      uint32_t key_id,
		      const char **elements)
{
	struct registry_local *reg_local = talloc_get_type(rctx,
							   struct registry_local);
	struct mountpoint *mp;
	unsigned int i = 0;

	mp = talloc(rctx, struct mountpoint);
	W_ERROR_HAVE_NO_MEMORY(mp);
	mp->path.predefined_key = key_id;
	mp->prev = mp->next = NULL;
	mp->key = hive_key;
	if (elements != NULL && str_list_length(elements) != 0) {
		mp->path.elements = talloc_array(mp, const char *,
						 str_list_length(elements));
		W_ERROR_HAVE_NO_MEMORY(mp->path.elements);
		for (i = 0; elements[i] != NULL; i++) {
			mp->path.elements[i] = talloc_reference(mp->path.elements,
								elements[i]);
		}
		mp->path.elements[i] = NULL;
	} else {
		mp->path.elements = NULL;
	}

	DLIST_ADD(reg_local->mountpoints, mp);

	return WERR_OK;
}
