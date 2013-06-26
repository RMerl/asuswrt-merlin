/*
   Unix SMB/CIFS implementation.
   DNS utility library
   Copyright (C) Gerald (Jerry) Carter           2006.
   Copyright (C) Jeremy Allison                  2007.

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
#include "libads/sitename_cache.h"

/****************************************************************************
 Store and fetch the AD client sitename.
****************************************************************************/

#define SITENAME_KEY	"AD_SITENAME/DOMAIN/%s"

static char *sitename_key(const char *realm)
{
	char *keystr;

	if (asprintf_strupper_m(&keystr, SITENAME_KEY, realm) == -1) {
		return NULL;
	}

	return keystr;
}


/****************************************************************************
 Store the AD client sitename.
 We store indefinately as every new CLDAP query will re-write this.
****************************************************************************/

bool sitename_store(const char *realm, const char *sitename)
{
	time_t expire;
	bool ret = False;
	char *key;

	if (!realm || (strlen(realm) == 0)) {
		DEBUG(0,("sitename_store: no realm\n"));
		return False;
	}

	key = sitename_key(realm);

	if (!sitename || (sitename && !*sitename)) {
		DEBUG(5,("sitename_store: deleting empty sitename!\n"));
		ret = gencache_del(key);
		SAFE_FREE(key);
		return ret;
	}

	expire = get_time_t_max(); /* Store indefinately. */

	DEBUG(10,("sitename_store: realm = [%s], sitename = [%s], expire = [%u]\n",
		realm, sitename, (unsigned int)expire ));

	ret = gencache_set( key, sitename, expire );
	SAFE_FREE(key);
	return ret;
}

/****************************************************************************
 Fetch the AD client sitename.
 Caller must free.
****************************************************************************/

char *sitename_fetch(const char *realm)
{
	char *sitename = NULL;
	time_t timeout;
	bool ret = False;
	const char *query_realm;
	char *key;

	if (!realm || (strlen(realm) == 0)) {
		query_realm = lp_realm();
	} else {
		query_realm = realm;
	}

	key = sitename_key(query_realm);

	ret = gencache_get( key, &sitename, &timeout );
	SAFE_FREE(key);
	if ( !ret ) {
		DEBUG(5,("sitename_fetch: No stored sitename for %s\n",
			query_realm));
	} else {
		DEBUG(5,("sitename_fetch: Returning sitename for %s: \"%s\"\n",
			query_realm, sitename ));
	}
	return sitename;
}

/****************************************************************************
 Did the sitename change ?
****************************************************************************/

bool stored_sitename_changed(const char *realm, const char *sitename)
{
	bool ret = False;

	char *new_sitename;

	if (!realm || (strlen(realm) == 0)) {
		DEBUG(0,("stored_sitename_changed: no realm\n"));
		return False;
	}

	new_sitename = sitename_fetch(realm);

	if (sitename && new_sitename && !strequal(sitename, new_sitename)) {
		ret = True;
	} else if ((sitename && !new_sitename) ||
			(!sitename && new_sitename)) {
		ret = True;
	}
	SAFE_FREE(new_sitename);
	return ret;
}

