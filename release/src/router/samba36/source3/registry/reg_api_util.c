/*
 *  Unix SMB/CIFS implementation.
 *  Virtual Windows Registry Layer
 *  Copyright (C) Volker Lendecke 2006
 *  Copyright (C) Michael Adam 2007-2010
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

/*
 * Higher level utility functions on top of reg_api.c
 */

#include "includes.h"
#include "registry.h"
#include "reg_api.h"
#include "reg_api_util.h"

/**
 * Utility function to open a complete registry path including the hive prefix.
 */
WERROR reg_open_path(TALLOC_CTX *mem_ctx, const char *orig_path,
		     uint32 desired_access, const struct security_token *token,
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

#if 0
/* these two functions are unused. */

/**
 * Utility function to create a registry key without opening the hive
 * before. Assumes the hive already exists.
 */

WERROR reg_create_path(TALLOC_CTX *mem_ctx, const char *orig_path,
		       uint32 desired_access,
		       const struct security_token *token,
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

WERROR reg_delete_path(const struct security_token *token,
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
