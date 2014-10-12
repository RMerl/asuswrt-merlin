/* 
 *  Unix SMB/CIFS implementation.
 *  Virtual Windows Registry Layer
 *  Copyright (C) Gerald Carter                     2002.
 *  Copyright (C) Michael Adam                      2008
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

/* Implementation of registry hook cache tree */

#include "includes.h"
#include "adt_tree.h"
#include "registry.h"
#include "reg_cachehook.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_REGISTRY

static struct sorted_tree *cache_tree = NULL;
extern struct registry_ops regdb_ops;		/* these are the default */

static WERROR keyname_to_path(TALLOC_CTX *mem_ctx, const char *keyname,
			      char **path)
{
	char *tmp_path = NULL;

	if ((keyname == NULL) || (path == NULL)) {
		return WERR_INVALID_PARAM;
	}

	tmp_path = talloc_asprintf(mem_ctx, "\\%s", keyname);
	if (tmp_path == NULL) {
		DEBUG(0, ("talloc_asprintf failed!\n"));
		return WERR_NOMEM;
	}

	*path = tmp_path;

	return WERR_OK;
}

/**********************************************************************
 Initialize the cache tree if it has not been initialized yet.
 *********************************************************************/

WERROR reghook_cache_init(void)
{
	if (cache_tree != NULL) {
		return WERR_OK;
	}

	cache_tree = pathtree_init(&regdb_ops);
	if (cache_tree == NULL) {
		return WERR_NOMEM;
	}
	DEBUG(10, ("reghook_cache_init: new tree with default "
		   "ops %p for key [%s]\n", (void *)&regdb_ops,
		   KEY_TREE_ROOT));
	return WERR_OK;
}

/**********************************************************************
 Add a new registry hook to the cache.  Note that the keyname
 is not in the exact format that a struct sorted_tree expects.
 *********************************************************************/

WERROR reghook_cache_add(const char *keyname, struct registry_ops *ops)
{
	WERROR werr;
	char *key = NULL;

	if ((keyname == NULL) || (ops == NULL)) {
		return WERR_INVALID_PARAM;
	}

	werr = keyname_to_path(talloc_tos(), keyname, &key);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	DEBUG(10, ("reghook_cache_add: Adding ops %p for key [%s]\n",
		   (void *)ops, key));

	werr = pathtree_add(cache_tree, key, ops);

done:
	TALLOC_FREE(key);
	return werr;
}

/**********************************************************************
 Find a key in the cache.
 *********************************************************************/

struct registry_ops *reghook_cache_find(const char *keyname)
{
	WERROR werr;
	char *key = NULL;
	struct registry_ops *ops = NULL;

	if (keyname == NULL) {
		return NULL;
	}

	werr = keyname_to_path(talloc_tos(), keyname, &key);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	DEBUG(10,("reghook_cache_find: Searching for keyname [%s]\n", key));

	ops = (struct registry_ops *)pathtree_find(cache_tree, key);

	DEBUG(10, ("reghook_cache_find: found ops %p for key [%s]\n",
		   ops ? (void *)ops : 0, key));

done:
	TALLOC_FREE(key);

	return ops;
}

/**********************************************************************
 Print out the cache tree structure for debugging.
 *********************************************************************/

void reghook_dump_cache( int debuglevel )
{
	DEBUG(debuglevel,("reghook_dump_cache: Starting cache dump now...\n"));

	pathtree_print_keys( cache_tree, debuglevel );
}
