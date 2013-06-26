/* 
 *  Unix SMB/CIFS implementation.
 *  Copyright (C) Jelmer Vernooij			2009
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
#include "../lib/util/dlinklist.h"
#include "../lib/util/parmlist.h"

struct parmlist_entry *parmlist_get(struct parmlist *ctx, const char *name)
{
	struct parmlist_entry *e;
	for (e = ctx->entries; e; e = e->next) {
		if (strcasecmp(e->key, name) == 0)
			return e;
	}

	return NULL;
}

int parmlist_get_int(struct parmlist *ctx, const char *name, int default_v)
{
	struct parmlist_entry *p = parmlist_get(ctx, name);

	if (p != NULL)
		return strtol(p->value, NULL, 0); 

	return default_v;
}

bool parmlist_get_bool(struct parmlist *ctx, const char *name, bool default_v)
{
	struct parmlist_entry *p = parmlist_get(ctx, name);
	bool ret;

	if (p == NULL)
		return default_v;

	if (!set_boolean(p->value, &ret)) {
		DEBUG(0,("lp_bool(%s): value is not boolean!\n", p->value));
		return default_v;
	}

	return ret;
}

const char *parmlist_get_string(struct parmlist *ctx, const char *name, 
								const char *default_v)
{
	struct parmlist_entry *p = parmlist_get(ctx, name);

	if (p == NULL)
		return default_v;

	return p->value;
}

const char **parmlist_get_string_list(struct parmlist *ctx, const char *name, 
									  const char *separator)
{
	struct parmlist_entry *p = parmlist_get(ctx, name);

	if (p == NULL)
		return NULL;

	return (const char **)str_list_make(ctx, p->value, separator);
}

static struct parmlist_entry *parmlist_get_add(struct parmlist *ctx, const char *name)
{
	struct parmlist_entry *e = parmlist_get(ctx, name);

	if (e != NULL)
		return e;

	e = talloc(ctx, struct parmlist_entry);
	if (e == NULL)
		return NULL;
	e->key = talloc_strdup(e, name);
	DLIST_ADD(ctx->entries, e);
	return e;
}

int parmlist_set_string(struct parmlist *ctx, const char *name, 
						const char *value)
{
	struct parmlist_entry *e = parmlist_get_add(ctx, name);
	if (e == NULL)
		return -1;

	e->value = talloc_strdup(e, value);
	return 0;
}
