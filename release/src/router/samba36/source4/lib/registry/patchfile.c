/*
   Unix SMB/CIFS implementation.
   Reading registry patch files

   Copyright (C) Jelmer Vernooij 2004-2007
   Copyright (C) Wilco Baan Hofman 2006
   Copyright (C) Matthias Dieter Wallnöfer 2008-2010

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
#include "lib/registry/registry.h"
#include "system/filesys.h"


_PUBLIC_ WERROR reg_preg_diff_load(int fd,
				   const struct reg_diff_callbacks *callbacks,
				   void *callback_data);

_PUBLIC_ WERROR reg_dotreg_diff_load(int fd,
				     const struct reg_diff_callbacks *callbacks,
				     void *callback_data);

/*
 * Generate difference between two keys
 */
WERROR reg_generate_diff_key(struct registry_key *oldkey,
			     struct registry_key *newkey,
			     const char *path,
			     const struct reg_diff_callbacks *callbacks,
			     void *callback_data)
{
	unsigned int i;
	struct registry_key *t1 = NULL, *t2 = NULL;
	char *tmppath;
	const char *keyname1;
	WERROR error, error1, error2;
	TALLOC_CTX *mem_ctx = talloc_init("writediff");
	uint32_t old_num_subkeys, old_num_values,
			 new_num_subkeys, new_num_values;

	if (oldkey != NULL) {
		error = reg_key_get_info(mem_ctx, oldkey, NULL,
					 &old_num_subkeys, &old_num_values,
					 NULL, NULL, NULL, NULL);
		if (!W_ERROR_IS_OK(error)) {
			DEBUG(0, ("Error occurred while getting key info: %s\n",
				win_errstr(error)));
			talloc_free(mem_ctx);
			return error;
		}
	} else {
		old_num_subkeys = 0;
		old_num_values = 0;
	}

	/* Subkeys that were changed or deleted */
	for (i = 0; i < old_num_subkeys; i++) {
		error1 = reg_key_get_subkey_by_index(mem_ctx, oldkey, i,
						     &keyname1, NULL, NULL);
		if (!W_ERROR_IS_OK(error1)) {
			DEBUG(0, ("Error occurred while getting subkey by index: %s\n",
				win_errstr(error1)));
			continue;
		}

		if (newkey != NULL) {
			error2 = reg_open_key(mem_ctx, newkey, keyname1, &t2);
		} else {
			error2 = WERR_BADFILE;
			t2 = NULL;
		}

		if (!W_ERROR_IS_OK(error2) && !W_ERROR_EQUAL(error2, WERR_BADFILE)) {
			DEBUG(0, ("Error occurred while getting subkey by name: %s\n",
				win_errstr(error2)));
			talloc_free(mem_ctx);
			return error2;
		}

		/* if "error2" is going to be "WERR_BADFILE", then newkey */
		/* didn't have such a subkey and therefore add a del diff */
		tmppath = talloc_asprintf(mem_ctx, "%s\\%s", path, keyname1);
		if (tmppath == NULL) {
			DEBUG(0, ("Out of memory\n"));
			talloc_free(mem_ctx);
			return WERR_NOMEM;
		}
		if (!W_ERROR_IS_OK(error2))
			callbacks->del_key(callback_data, tmppath);

		/* perform here also the recursive invocation */
		error1 = reg_open_key(mem_ctx, oldkey, keyname1, &t1);
		if (!W_ERROR_IS_OK(error1)) {
			DEBUG(0, ("Error occurred while getting subkey by name: %s\n",
			win_errstr(error1)));
			talloc_free(mem_ctx);
			return error1;
		}
		reg_generate_diff_key(t1, t2, tmppath, callbacks, callback_data);

		talloc_free(tmppath);
	}

	if (newkey != NULL) {
		error = reg_key_get_info(mem_ctx, newkey, NULL,
					 &new_num_subkeys, &new_num_values,
					 NULL, NULL, NULL, NULL);
		if (!W_ERROR_IS_OK(error)) {
			DEBUG(0, ("Error occurred while getting key info: %s\n",
				win_errstr(error)));
			talloc_free(mem_ctx);
			return error;
		}
	} else {
		new_num_subkeys = 0;
		new_num_values = 0;
	}

	/* Subkeys that were added */
	for(i = 0; i < new_num_subkeys; i++) {
		error1 = reg_key_get_subkey_by_index(mem_ctx, newkey, i,
						     &keyname1, NULL, NULL);
		if (!W_ERROR_IS_OK(error1)) {
			DEBUG(0, ("Error occurred while getting subkey by index: %s\n",
				win_errstr(error1)));
			talloc_free(mem_ctx);
			return error1;
		}

		if (oldkey != NULL) {
			error2 = reg_open_key(mem_ctx, oldkey, keyname1, &t1);

			if (W_ERROR_IS_OK(error2))
				continue;
		} else {
			error2 = WERR_BADFILE;	
			t1 = NULL;
		}

		if (!W_ERROR_EQUAL(error2, WERR_BADFILE)) {
			DEBUG(0, ("Error occurred while getting subkey by name: %s\n",
				win_errstr(error2)));
			talloc_free(mem_ctx);
			return error2;
		}

		/* oldkey didn't have such a subkey, add a add diff */
		tmppath = talloc_asprintf(mem_ctx, "%s\\%s", path, keyname1);
		if (tmppath == NULL) {
			DEBUG(0, ("Out of memory\n"));
			talloc_free(mem_ctx);
			return WERR_NOMEM;
		}
		callbacks->add_key(callback_data, tmppath);

		/* perform here also the recursive invocation */
		error1 = reg_open_key(mem_ctx, newkey, keyname1, &t2);
		if (!W_ERROR_IS_OK(error1)) {
			DEBUG(0, ("Error occurred while getting subkey by name: %s\n",
			win_errstr(error1)));
			talloc_free(mem_ctx);
			return error1;
		}
		reg_generate_diff_key(t1, t2, tmppath, callbacks, callback_data);

		talloc_free(tmppath);
	}

	/* Values that were added or changed */
	for(i = 0; i < new_num_values; i++) {
		const char *name;
		uint32_t type1, type2;
		DATA_BLOB contents1 = { NULL, 0 }, contents2 = { NULL, 0 };

		error1 = reg_key_get_value_by_index(mem_ctx, newkey, i,
						    &name, &type1, &contents1);
		if (!W_ERROR_IS_OK(error1)) {
			DEBUG(0, ("Unable to get value by index: %s\n",
				win_errstr(error1)));
			talloc_free(mem_ctx);
			return error1;
		}

		if (oldkey != NULL) {
			error2 = reg_key_get_value_by_name(mem_ctx, oldkey,
							   name, &type2,
							   &contents2);
		} else
			error2 = WERR_BADFILE;

		if (!W_ERROR_IS_OK(error2)
			&& !W_ERROR_EQUAL(error2, WERR_BADFILE)) {
			DEBUG(0, ("Error occurred while getting value by name: %s\n",
				win_errstr(error2)));
			talloc_free(mem_ctx);
			return error2;
		}

		if (W_ERROR_IS_OK(error2)
		    && (data_blob_cmp(&contents1, &contents2) == 0)
		    && (type1 == type2)) {
			talloc_free(discard_const_p(char, name));
			talloc_free(contents1.data);
			talloc_free(contents2.data);
			continue;
		}

		callbacks->set_value(callback_data, path, name,
				     type1, contents1);

		talloc_free(discard_const_p(char, name));
		talloc_free(contents1.data);
		talloc_free(contents2.data);
	}

	/* Values that were deleted */
	for (i = 0; i < old_num_values; i++) {
		const char *name;
		uint32_t type;
		DATA_BLOB contents = { NULL, 0 };

		error1 = reg_key_get_value_by_index(mem_ctx, oldkey, i, &name,
						    &type, &contents);
		if (!W_ERROR_IS_OK(error1)) {
			DEBUG(0, ("Unable to get value by index: %s\n",
				win_errstr(error1)));
			talloc_free(mem_ctx);
			return error1;
		}

		if (newkey != NULL)
			error2 = reg_key_get_value_by_name(mem_ctx, newkey,
				 name, &type, &contents);
		else
			error2 = WERR_BADFILE;

		if (W_ERROR_IS_OK(error2)) {
			talloc_free(discard_const_p(char, name));
			talloc_free(contents.data);
			continue;
		}

		if (!W_ERROR_EQUAL(error2, WERR_BADFILE)) {
			DEBUG(0, ("Error occurred while getting value by name: %s\n",
				win_errstr(error2)));
			talloc_free(mem_ctx);
			return error2;
		}

		callbacks->del_value(callback_data, path, name);

		talloc_free(discard_const_p(char, name));
		talloc_free(contents.data);
	}

	talloc_free(mem_ctx);
	return WERR_OK;
}

/**
 * Generate diff between two registry contexts
 */
_PUBLIC_ WERROR reg_generate_diff(struct registry_context *ctx1,
				  struct registry_context *ctx2,
				  const struct reg_diff_callbacks *callbacks,
				  void *callback_data)
{
	unsigned int i;
	WERROR error;

	for (i = 0; reg_predefined_keys[i].name; i++) {
		struct registry_key *r1 = NULL, *r2 = NULL;

		error = reg_get_predefined_key(ctx1,
			reg_predefined_keys[i].handle, &r1);
		if (!W_ERROR_IS_OK(error) &&
		    !W_ERROR_EQUAL(error, WERR_BADFILE)) {
			DEBUG(0, ("Unable to open hive %s for backend 1\n",
				reg_predefined_keys[i].name));
			continue;
		}

		error = reg_get_predefined_key(ctx2,
			reg_predefined_keys[i].handle, &r2);
		if (!W_ERROR_IS_OK(error) &&
		    !W_ERROR_EQUAL(error, WERR_BADFILE)) {
			DEBUG(0, ("Unable to open hive %s for backend 2\n",
				reg_predefined_keys[i].name));
			continue;
		}

		/* if "r1" is NULL (old hive) and "r2" isn't (new hive) then
		 * the hive doesn't exist yet and we have to generate an add
		 * diff */
		if ((r1 == NULL) && (r2 != NULL)) {
			callbacks->add_key(callback_data,
					   reg_predefined_keys[i].name);
		}
		/* if "r1" isn't NULL (old hive) and "r2" is (new hive) then
		 * the hive shouldn't exist anymore and we have to generate a
		 * del diff */
		if ((r1 != NULL) && (r2 == NULL)) {
			callbacks->del_key(callback_data,
					   reg_predefined_keys[i].name);
		}

		error = reg_generate_diff_key(r1, r2,
			reg_predefined_keys[i].name, callbacks,
			callback_data);
		if (!W_ERROR_IS_OK(error)) {
			DEBUG(0, ("Unable to determine diff: %s\n",
				win_errstr(error)));
			return error;
		}
	}
	if (callbacks->done != NULL) {
		callbacks->done(callback_data);
	}
	return WERR_OK;
}

/**
 * Load diff file
 */
_PUBLIC_ WERROR reg_diff_load(const char *filename,
			      const struct reg_diff_callbacks *callbacks,
			      void *callback_data)
{
	int fd;
	char hdr[4];

	fd = open(filename, O_RDONLY, 0);
	if (fd == -1) {
		DEBUG(0, ("Error opening registry patch file `%s'\n",
			filename));
		return WERR_GENERAL_FAILURE;
	}

	if (read(fd, &hdr, 4) != 4) {
		DEBUG(0, ("Error reading registry patch file `%s'\n",
			filename));
		close(fd);
		return WERR_GENERAL_FAILURE;
	}

	/* Reset position in file */
	lseek(fd, 0, SEEK_SET);
#if 0 /* These backends are not supported yet. */
	if (strncmp(hdr, "CREG", 4) == 0) {
		/* Must be a W9x CREG Config.pol file */
		return reg_creg_diff_load(diff, fd);
	} else if (strncmp(hdr, "regf", 4) == 0) {
		/* Must be a REGF NTConfig.pol file */
		return reg_regf_diff_load(diff, fd);
	} else
#endif
	if (strncmp(hdr, "PReg", 4) == 0) {
		/* Must be a GPO Registry.pol file */
		return reg_preg_diff_load(fd, callbacks, callback_data);
	} else {
		/* Must be a normal .REG file */
		return reg_dotreg_diff_load(fd, callbacks, callback_data);
	}
}

/**
 * The reg_diff_apply functions
 */
static WERROR reg_diff_apply_add_key(void *_ctx, const char *key_name)
{
	struct registry_context *ctx = (struct registry_context *)_ctx;
	struct registry_key *tmp;
	char *buf, *buf_ptr;
	WERROR error;

	/* Recursively create the path */
	buf = talloc_strdup(ctx, key_name);
	W_ERROR_HAVE_NO_MEMORY(buf);
	buf_ptr = buf;

	while (*buf_ptr++ != '\0' ) {
		if (*buf_ptr == '\\') {
			*buf_ptr = '\0';
			error = reg_key_add_abs(ctx, ctx, buf, 0, NULL, &tmp);

			if (!W_ERROR_EQUAL(error, WERR_ALREADY_EXISTS) &&
				    !W_ERROR_IS_OK(error)) {
				DEBUG(0, ("Error adding new key '%s': %s\n",
					key_name, win_errstr(error)));
				return error;
			}
			*buf_ptr++ = '\\';
			talloc_free(tmp);
		}
	}

	talloc_free(buf);

	/* Add the key */
	error = reg_key_add_abs(ctx, ctx, key_name, 0, NULL, &tmp);

	if (!W_ERROR_EQUAL(error, WERR_ALREADY_EXISTS) &&
		    !W_ERROR_IS_OK(error)) {
		DEBUG(0, ("Error adding new key '%s': %s\n",
			key_name, win_errstr(error)));
		return error;
	}
	talloc_free(tmp);

	return WERR_OK;
}

static WERROR reg_diff_apply_del_key(void *_ctx, const char *key_name)
{
	struct registry_context *ctx = (struct registry_context *)_ctx;

	/* We can't proof here for success, because a common superkey could */
	/* have been deleted before the subkey's (diff order). This removed */
	/* therefore all children recursively and the "WERR_BADFILE" result is */
	/* expected. */

	reg_key_del_abs(ctx, key_name);

	return WERR_OK;
}

static WERROR reg_diff_apply_set_value(void *_ctx, const char *path,
				       const char *value_name,
				       uint32_t value_type, DATA_BLOB value)
{
	struct registry_context *ctx = (struct registry_context *)_ctx;
	struct registry_key *tmp;
	WERROR error;

	/* Open key */
	error = reg_open_key_abs(ctx, ctx, path, &tmp);

	if (W_ERROR_EQUAL(error, WERR_BADFILE)) {
		DEBUG(0, ("Error opening key '%s'\n", path));
		return error;
	}

	/* Set value */
	error = reg_val_set(tmp, value_name,
				 value_type, value);
	if (!W_ERROR_IS_OK(error)) {
		DEBUG(0, ("Error setting value '%s'\n", value_name));
		return error;
	}

	talloc_free(tmp);

	return WERR_OK;
}

static WERROR reg_diff_apply_del_value(void *_ctx, const char *key_name,
				       const char *value_name)
{
	struct registry_context *ctx = (struct registry_context *)_ctx;
	struct registry_key *tmp;
	WERROR error;

	/* Open key */
	error = reg_open_key_abs(ctx, ctx, key_name, &tmp);

	if (!W_ERROR_IS_OK(error)) {
		DEBUG(0, ("Error opening key '%s'\n", key_name));
		return error;
	}

	error = reg_del_value(ctx, tmp, value_name);
	if (!W_ERROR_IS_OK(error)) {
		DEBUG(0, ("Error deleting value '%s'\n", value_name));
		return error;
	}

	talloc_free(tmp);

	return WERR_OK;
}

static WERROR reg_diff_apply_del_all_values(void *_ctx, const char *key_name)
{
	struct registry_context *ctx = (struct registry_context *)_ctx;
	struct registry_key *key;
	WERROR error;
	const char *value_name;

	error = reg_open_key_abs(ctx, ctx, key_name, &key);

	if (!W_ERROR_IS_OK(error)) {
		DEBUG(0, ("Error opening key '%s'\n", key_name));
		return error;
	}

	W_ERROR_NOT_OK_RETURN(reg_key_get_info(ctx, key, NULL,
			       NULL, NULL, NULL, NULL, NULL, NULL));

	while (W_ERROR_IS_OK(reg_key_get_value_by_index(
			ctx, key, 0, &value_name, NULL, NULL))) {
		error = reg_del_value(ctx, key, value_name);
		if (!W_ERROR_IS_OK(error)) {
			DEBUG(0, ("Error deleting value '%s'\n", value_name));
			return error;
		}
		talloc_free(discard_const_p(char, value_name));
	}

	talloc_free(key);

	return WERR_OK;
}

/**
 * Apply diff to a registry context
 */
_PUBLIC_ WERROR reg_diff_apply(struct registry_context *ctx, 
							   const char *filename)
{
	struct reg_diff_callbacks callbacks;

	callbacks.add_key = reg_diff_apply_add_key;
	callbacks.del_key = reg_diff_apply_del_key;
	callbacks.set_value = reg_diff_apply_set_value;
	callbacks.del_value = reg_diff_apply_del_value;
	callbacks.del_all_values = reg_diff_apply_del_all_values;
	callbacks.done = NULL;

	return reg_diff_load(filename, &callbacks, ctx);
}
