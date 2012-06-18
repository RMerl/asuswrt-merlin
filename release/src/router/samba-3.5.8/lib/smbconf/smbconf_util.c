/*
 *  Unix SMB/CIFS implementation.
 *  libsmbconf - Samba configuration library, utility functions
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
#include "smbconf_private.h"


static int smbconf_destroy_ctx(struct smbconf_ctx *ctx)
{
	return ctx->ops->shutdown(ctx);
}

/**
 * Initialize the configuration.
 *
 * This should be the first function in a sequence of calls to smbconf
 * functions:
 *
 * Upon success, this creates and returns the conf context
 * that should be passed around in subsequent calls to the other
 * smbconf functions.
 *
 * After the work with the configuration is completed, smbconf_shutdown()
 * should be called.
 */
WERROR smbconf_init_internal(TALLOC_CTX *mem_ctx, struct smbconf_ctx **conf_ctx,
			     const char *path, struct smbconf_ops *ops)
{
	WERROR werr = WERR_OK;
	struct smbconf_ctx *ctx;

	if (conf_ctx == NULL) {
		return WERR_INVALID_PARAM;
	}

	ctx = talloc_zero(mem_ctx, struct smbconf_ctx);
	if (ctx == NULL) {
		return WERR_NOMEM;
	}

	ctx->ops = ops;

	werr = ctx->ops->init(ctx, path);
	if (!W_ERROR_IS_OK(werr)) {
		goto fail;
	}

	talloc_set_destructor(ctx, smbconf_destroy_ctx);

	*conf_ctx = ctx;
	return werr;

fail:
	talloc_free(ctx);
	return werr;
}


/**
 * add a string to a talloced array of strings.
 */
WERROR smbconf_add_string_to_array(TALLOC_CTX *mem_ctx,
				   char ***array,
				   uint32_t count,
				   const char *string)
{
	char **new_array = NULL;

	if (array == NULL) {
		return WERR_INVALID_PARAM;
	}

	new_array = talloc_realloc(mem_ctx, *array, char *, count + 1);
	if (new_array == NULL) {
		return WERR_NOMEM;
	}

	if (string == NULL) {
		new_array[count] = NULL;
	} else {
		new_array[count] = talloc_strdup(new_array, string);
		if (new_array[count] == NULL) {
			talloc_free(new_array);
			return WERR_NOMEM;
		}
	}

	*array = new_array;

	return WERR_OK;
}

bool smbconf_find_in_array(const char *string, char **list,
			   uint32_t num_entries, uint32_t *entry)
{
	uint32_t i;

	if (list == NULL) {
		return false;
	}

	for (i = 0; i < num_entries; i++) {
		if (((string == NULL) && (list[i] == NULL)) ||
		    strequal(string, list[i]))
		{
			if (entry != NULL) {
				*entry = i;
			}
			return true;
		}
	}

	return false;
}

bool smbconf_reverse_find_in_array(const char *string, char **list,
				   uint32_t num_entries, uint32_t *entry)
{
	int32_t i;

	if ((string == NULL) || (list == NULL) || (num_entries == 0)) {
		return false;
	}

	for (i = num_entries - 1; i >= 0; i--) {
		if (strequal(string, list[i])) {
			if (entry != NULL) {
				*entry = i;
			}
			return true;
		}
	}

	return false;
}
