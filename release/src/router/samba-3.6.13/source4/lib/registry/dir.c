/*
   Unix SMB/CIFS implementation.
   Registry interface
   Copyright (C) Jelmer Vernooij				  2004-2007.

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
#include "system/dir.h"
#include "system/filesys.h"

struct dir_key {
	struct hive_key key;
	const char *path;
};

static struct hive_operations reg_backend_dir;

static WERROR reg_dir_add_key(TALLOC_CTX *mem_ctx,
			      const struct hive_key *parent,
			      const char *name, const char *classname,
			      struct security_descriptor *desc,
			      struct hive_key **result)
{
	struct dir_key *dk = talloc_get_type(parent, struct dir_key);
	char *path;
	int ret;

	path = talloc_asprintf(mem_ctx, "%s/%s", dk->path, name);
	W_ERROR_HAVE_NO_MEMORY(path);
	ret = mkdir(path, 0700);
	if (ret == 0) {
		struct dir_key *key = talloc(mem_ctx, struct dir_key);
		W_ERROR_HAVE_NO_MEMORY(key);
		key->key.ops = &reg_backend_dir;
		key->path = talloc_steal(key, path);
		*result = (struct hive_key *)key;
		return WERR_OK;
	}

	if (errno == EEXIST)
		return WERR_ALREADY_EXISTS;
	printf("FAILED %s BECAUSE: %s\n", path, strerror(errno));
	return WERR_GENERAL_FAILURE;
}

static WERROR reg_dir_delete_recursive(const char *name)
{
	DIR *d;
	struct dirent *e;
	WERROR werr;

	d = opendir(name);
	if (d == NULL) {
		DEBUG(3,("Unable to open '%s': %s\n", name,
		      strerror(errno)));
		return WERR_BADFILE;
	}

	while((e = readdir(d))) {
		char *path;
		struct stat stbuf;

		if (ISDOT(e->d_name) || ISDOTDOT(e->d_name))
			continue;

		path = talloc_asprintf(name, "%s/%s", name, e->d_name);
		W_ERROR_HAVE_NO_MEMORY(path);

		stat(path, &stbuf);

		if (!S_ISDIR(stbuf.st_mode)) {
			if (unlink(path) < 0) {
				talloc_free(path);
				closedir(d);
				return WERR_GENERAL_FAILURE;
			}
		} else {
			werr = reg_dir_delete_recursive(path);
			if (!W_ERROR_IS_OK(werr)) {
				talloc_free(path);
				closedir(d);
				return werr;
			}
		}

		talloc_free(path);
	}
	closedir(d);

	if (rmdir(name) == 0)
		return WERR_OK;
	else if (errno == ENOENT)
		return WERR_BADFILE;
	else
		return WERR_GENERAL_FAILURE;
}

static WERROR reg_dir_del_key(TALLOC_CTX *mem_ctx, const struct hive_key *k,
			      const char *name)
{
	struct dir_key *dk = talloc_get_type(k, struct dir_key);
	char *child;
	WERROR ret;

	child = talloc_asprintf(mem_ctx, "%s/%s", dk->path, name);
	W_ERROR_HAVE_NO_MEMORY(child);

	ret = reg_dir_delete_recursive(child);

	talloc_free(child);

	return ret;
}

static WERROR reg_dir_open_key(TALLOC_CTX *mem_ctx,
			       const struct hive_key *parent,
			       const char *name, struct hive_key **subkey)
{
	DIR *d;
	char *fullpath;
	const struct dir_key *p = talloc_get_type(parent, struct dir_key);
	struct dir_key *ret;

	if (name == NULL) {
		DEBUG(0, ("NULL pointer passed as directory name!"));
		return WERR_INVALID_PARAM;
	}

	fullpath = talloc_asprintf(mem_ctx, "%s/%s", p->path, name);
	W_ERROR_HAVE_NO_MEMORY(fullpath);

	d = opendir(fullpath);
	if (d == NULL) {
		DEBUG(3,("Unable to open '%s': %s\n", fullpath,
			strerror(errno)));
		talloc_free(fullpath);
		return WERR_BADFILE;
	}
	closedir(d);
	ret = talloc(mem_ctx, struct dir_key);
	ret->key.ops = &reg_backend_dir;
	ret->path = talloc_steal(ret, fullpath);
	*subkey = (struct hive_key *)ret;
	return WERR_OK;
}

static WERROR reg_dir_key_by_index(TALLOC_CTX *mem_ctx,
				   const struct hive_key *k, uint32_t idx,
				   const char **name,
				   const char **classname,
				   NTTIME *last_mod_time)
{
	struct dirent *e;
	const struct dir_key *dk = talloc_get_type(k, struct dir_key);
	unsigned int i = 0;
	DIR *d;

	d = opendir(dk->path);

	if (d == NULL)
		return WERR_INVALID_PARAM;

	while((e = readdir(d))) {
		if(!ISDOT(e->d_name) && !ISDOTDOT(e->d_name)) {
			struct stat stbuf;
			char *thispath;

			/* Check if file is a directory */
			thispath = talloc_asprintf(mem_ctx, "%s/%s", dk->path,
						   e->d_name);
			W_ERROR_HAVE_NO_MEMORY(thispath);
			stat(thispath, &stbuf);

			if (!S_ISDIR(stbuf.st_mode)) {
				talloc_free(thispath);
				continue;
			}

			if (i == idx) {
				struct stat st;
				*name = talloc_strdup(mem_ctx, e->d_name);
				W_ERROR_HAVE_NO_MEMORY(*name);
				*classname = NULL;
				stat(thispath, &st);
				unix_to_nt_time(last_mod_time, st.st_mtime);
				talloc_free(thispath);
				closedir(d);
				return WERR_OK;
			}
			i++;

			talloc_free(thispath);
		}
	}

	closedir(d);

	return WERR_NO_MORE_ITEMS;
}

WERROR reg_open_directory(TALLOC_CTX *parent_ctx,
			  const char *location, struct hive_key **key)
{
	struct dir_key *dk;

	if (location == NULL)
		return WERR_INVALID_PARAM;

	dk = talloc(parent_ctx, struct dir_key);
	W_ERROR_HAVE_NO_MEMORY(dk);
	dk->key.ops = &reg_backend_dir;
	dk->path = talloc_strdup(dk, location);
	*key = (struct hive_key *)dk;
	return WERR_OK;
}

WERROR reg_create_directory(TALLOC_CTX *parent_ctx,
			    const char *location, struct hive_key **key)
{
	if (mkdir(location, 0700) != 0) {
		*key = NULL;
		return WERR_GENERAL_FAILURE;
	}

	return reg_open_directory(parent_ctx, location, key);
}

static WERROR reg_dir_get_info(TALLOC_CTX *ctx, const struct hive_key *key,
			       const char **classname,
			       uint32_t *num_subkeys,
			       uint32_t *num_values,
			       NTTIME *lastmod,
			       uint32_t *max_subkeynamelen,
			       uint32_t *max_valnamelen,
			       uint32_t *max_valbufsize)
{
	DIR *d;
	const struct dir_key *dk = talloc_get_type(key, struct dir_key);
	struct dirent *e;
	struct stat st;

	SMB_ASSERT(key != NULL);

	if (classname != NULL)
		*classname = NULL;

	d = opendir(dk->path);
	if (d == NULL)
		return WERR_INVALID_PARAM;

	if (num_subkeys != NULL)
		*num_subkeys = 0;

	if (num_values != NULL)
		*num_values = 0;

	if (max_subkeynamelen != NULL)
		*max_subkeynamelen = 0;

	if (max_valnamelen != NULL)
		*max_valnamelen = 0;

	if (max_valbufsize != NULL)
		*max_valbufsize = 0;

	while((e = readdir(d))) {
		if(!ISDOT(e->d_name) && !ISDOTDOT(e->d_name)) {
			char *path = talloc_asprintf(ctx, "%s/%s",
						     dk->path, e->d_name);
			W_ERROR_HAVE_NO_MEMORY(path);

			if (stat(path, &st) < 0) {
				DEBUG(0, ("Error statting %s: %s\n", path,
					strerror(errno)));
				talloc_free(path);
				continue;
			}

			if (S_ISDIR(st.st_mode)) {
				if (num_subkeys != NULL)
					(*num_subkeys)++;
				if (max_subkeynamelen != NULL)
					*max_subkeynamelen = MAX(*max_subkeynamelen, strlen(e->d_name));
			}

			if (!S_ISDIR(st.st_mode)) {
				if (num_values != NULL)
					(*num_values)++;
				if (max_valnamelen != NULL)
					*max_valnamelen = MAX(*max_valnamelen, strlen(e->d_name));
				if (max_valbufsize != NULL)
					*max_valbufsize = MAX(*max_valbufsize, st.st_size);
			}

			talloc_free(path);
		}
	}

	closedir(d);

	if (lastmod != NULL)
		*lastmod = 0;
	return WERR_OK;
}

static WERROR reg_dir_set_value(struct hive_key *key, const char *name,
				uint32_t type, const DATA_BLOB data)
{
	const struct dir_key *dk = talloc_get_type(key, struct dir_key);
	char *path;
	bool ret;

	path = talloc_asprintf(dk, "%s/%s", dk->path, name);
	W_ERROR_HAVE_NO_MEMORY(path);

	ret = file_save(path, data.data, data.length);

	talloc_free(path);

	if (!ret) {
		return WERR_GENERAL_FAILURE;
	}

	/* FIXME: Type */

	return WERR_OK;
}

static WERROR reg_dir_get_value(TALLOC_CTX *mem_ctx,
				struct hive_key *key, const char *name,
				uint32_t *type, DATA_BLOB *data)
{
	const struct dir_key *dk = talloc_get_type(key, struct dir_key);
	char *path;
	size_t size;
	char *contents;

	path = talloc_asprintf(mem_ctx, "%s/%s", dk->path, name);
	W_ERROR_HAVE_NO_MEMORY(path);

	contents = file_load(path, &size, 0, mem_ctx);

	talloc_free(path);

	if (contents == NULL)
		return WERR_BADFILE;

	if (type != NULL)
		*type = 4; /* FIXME */

	data->data = (uint8_t *)contents;
	data->length = size;

	return WERR_OK;
}

static WERROR reg_dir_enum_value(TALLOC_CTX *mem_ctx,
				 struct hive_key *key, uint32_t idx,
				 const char **name,
				 uint32_t *type, DATA_BLOB *data)
{
	const struct dir_key *dk = talloc_get_type(key, struct dir_key);
	DIR *d;
	struct dirent *e;
	unsigned int i;

	d = opendir(dk->path);
	if (d == NULL) {
		DEBUG(3,("Unable to open '%s': %s\n", dk->path,
			strerror(errno)));
		return WERR_BADFILE;
	}

	i = 0;
	while((e = readdir(d))) {
		if (ISDOT(e->d_name) || ISDOTDOT(e->d_name))
			continue;

		if (i == idx) {
			if (name != NULL) {
				*name = talloc_strdup(mem_ctx, e->d_name);
				W_ERROR_HAVE_NO_MEMORY(*name);
			}
			W_ERROR_NOT_OK_RETURN(reg_dir_get_value(mem_ctx, key,
								*name, type,
								data));
			return WERR_OK;
		}

		i++;
	}
	closedir(d);

	return WERR_NO_MORE_ITEMS;
}


static WERROR reg_dir_del_value(TALLOC_CTX *mem_ctx,
				struct hive_key *key, const char *name)
{
	const struct dir_key *dk = talloc_get_type(key, struct dir_key);
	char *path;
	int ret;

	path = talloc_asprintf(mem_ctx, "%s/%s", dk->path, name);
	W_ERROR_HAVE_NO_MEMORY(path);

	ret = unlink(path);

	talloc_free(path);

	if (ret < 0) {
		if (errno == ENOENT)
			return WERR_BADFILE;
		return WERR_GENERAL_FAILURE;
	}

	return WERR_OK;
}

static struct hive_operations reg_backend_dir = {
	.name = "dir",
	.get_key_by_name = reg_dir_open_key,
	.get_key_info = reg_dir_get_info,
	.add_key = reg_dir_add_key,
	.del_key = reg_dir_del_key,
	.enum_key = reg_dir_key_by_index,
	.set_value = reg_dir_set_value,
	.get_value_by_name = reg_dir_get_value,
	.enum_value = reg_dir_enum_value,
	.delete_value = reg_dir_del_value,
};
