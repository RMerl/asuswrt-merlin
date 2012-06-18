/* 
 *  Unix SMB/CIFS implementation.
 *  Virtual Windows Registry Layer
 *  Copyright (C) Gerald Carter                     2002-2005
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

/* Implementation of registry frontend view functions. */

#include "includes.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_REGISTRY

/**
 * legacy open key function that should be replaced by uses of
 * reg_open_path
 */
WERROR regkey_open_internal(TALLOC_CTX *ctx,
			    struct registry_key_handle **regkey,
			    const char *path,
			    const struct nt_user_token *token,
			    uint32 access_desired )
{
	struct registry_key *key;
	WERROR err;

	err = reg_open_path(NULL, path, access_desired, token, &key);
	if (!W_ERROR_IS_OK(err)) {
		return err;
	}

	*regkey = talloc_move(ctx, &key->key);
	TALLOC_FREE(key);
	return WERR_OK;
}
