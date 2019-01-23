/*
   Unix SMB/CIFS implementation.
   Net_sam_logon info3 helpers
   Copyright (C) Alexander Bokovoy              2002.
   Copyright (C) Andrew Bartlett                2002.
   Copyright (C) Gerald Carter			2003.
   Copyright (C) Tim Potter			2003.
   Copyright (C) Guenther Deschner		2008.

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
#include "system/filesys.h"
#include "librpc/gen_ndr/ndr_krb5pac.h"
#include "../libcli/security/security.h"
#include "util_tdb.h"

#define NETSAMLOGON_TDB	"netsamlogon_cache.tdb"

static TDB_CONTEXT *netsamlogon_tdb = NULL;

/***********************************************************************
 open the tdb
 ***********************************************************************/

bool netsamlogon_cache_init(void)
{
	bool first_try = true;
	const char *path = NULL;
	int ret;
	struct tdb_context *tdb;

	if (netsamlogon_tdb) {
		return true;
	}

	path = cache_path(NETSAMLOGON_TDB);
again:
	tdb = tdb_open_log(path, 0, TDB_DEFAULT|TDB_INCOMPATIBLE_HASH,
			   O_RDWR | O_CREAT, 0600);
	if (tdb == NULL) {
		DEBUG(0,("tdb_open_log('%s') - failed\n", path));
		goto clear;
	}

	ret = tdb_check(tdb, NULL, NULL);
	if (ret != 0) {
		tdb_close(tdb);
		DEBUG(0,("tdb_check('%s') - failed\n", path));
		goto clear;
	}

	netsamlogon_tdb = tdb;
	return true;

clear:
	if (!first_try) {
		return false;
	}
	first_try = false;

	DEBUG(0,("retry after CLEAR_IF_FIRST for '%s'\n", path));
	tdb = tdb_open_log(path, 0, TDB_CLEAR_IF_FIRST|TDB_INCOMPATIBLE_HASH,
			   O_RDWR | O_CREAT, 0600);
	if (tdb) {
		tdb_close(tdb);
		goto again;
	}
	DEBUG(0,("tdb_open_log(%s) with CLEAR_IF_FIRST - failed\n", path));

	return false;
}


/***********************************************************************
 Shutdown samlogon_cache database
***********************************************************************/

bool netsamlogon_cache_shutdown(void)
{
	if (netsamlogon_tdb) {
		return (tdb_close(netsamlogon_tdb) == 0);
	}

	return true;
}

/***********************************************************************
 Clear cache getpwnam and getgroups entries from the winbindd cache
***********************************************************************/

void netsamlogon_clear_cached_user(const struct dom_sid *user_sid)
{
	fstring keystr;

	if (!netsamlogon_cache_init()) {
		DEBUG(0,("netsamlogon_clear_cached_user: cannot open "
			"%s for write!\n",
			NETSAMLOGON_TDB));
		return;
	}

	/* Prepare key as DOMAIN-SID/USER-RID string */
	sid_to_fstring(keystr, user_sid);

	DEBUG(10,("netsamlogon_clear_cached_user: SID [%s]\n", keystr));

	tdb_delete_bystring(netsamlogon_tdb, keystr);
}

/***********************************************************************
 Store a netr_SamInfo3 structure in a tdb for later user
 username should be in UTF-8 format
***********************************************************************/

bool netsamlogon_cache_store(const char *username, struct netr_SamInfo3 *info3)
{
	TDB_DATA data;
	fstring keystr;
	bool result = false;
	struct dom_sid	user_sid;
	time_t t = time(NULL);
	TALLOC_CTX *mem_ctx;
	DATA_BLOB blob;
	enum ndr_err_code ndr_err;
	struct netsamlogoncache_entry r;

	if (!info3) {
		return false;
	}

	if (!netsamlogon_cache_init()) {
		DEBUG(0,("netsamlogon_cache_store: cannot open %s for write!\n",
			NETSAMLOGON_TDB));
		return false;
	}

	sid_compose(&user_sid, info3->base.domain_sid, info3->base.rid);

	/* Prepare key as DOMAIN-SID/USER-RID string */
	sid_to_fstring(keystr, &user_sid);

	DEBUG(10,("netsamlogon_cache_store: SID [%s]\n", keystr));

	/* Prepare data */

	if (!(mem_ctx = TALLOC_P( NULL, int))) {
		DEBUG(0,("netsamlogon_cache_store: talloc() failed!\n"));
		return false;
	}

	/* only Samba fills in the username, not sure why NT doesn't */
	/* so we fill it in since winbindd_getpwnam() makes use of it */

	if (!info3->base.account_name.string) {
		info3->base.account_name.string = talloc_strdup(info3, username);
	}

	r.timestamp = t;
	r.info3 = *info3;

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_DEBUG(netsamlogoncache_entry, &r);
	}

	ndr_err = ndr_push_struct_blob(&blob, mem_ctx, &r,
				       (ndr_push_flags_fn_t)ndr_push_netsamlogoncache_entry);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DEBUG(0,("netsamlogon_cache_store: failed to push entry to cache\n"));
		TALLOC_FREE(mem_ctx);
		return false;
	}

	data.dsize = blob.length;
	data.dptr = blob.data;

	if (tdb_store_bystring(netsamlogon_tdb, keystr, data, TDB_REPLACE) != -1) {
		result = true;
	}

	TALLOC_FREE(mem_ctx);

	return result;
}

/***********************************************************************
 Retrieves a netr_SamInfo3 structure from a tdb.  Caller must
 free the user_info struct (malloc()'d memory)
***********************************************************************/

struct netr_SamInfo3 *netsamlogon_cache_get(TALLOC_CTX *mem_ctx, const struct dom_sid *user_sid)
{
	struct netr_SamInfo3 *info3 = NULL;
	TDB_DATA data;
	fstring keystr, tmp;
	enum ndr_err_code ndr_err;
	DATA_BLOB blob;
	struct netsamlogoncache_entry r;

	if (!netsamlogon_cache_init()) {
		DEBUG(0,("netsamlogon_cache_get: cannot open %s for write!\n",
			NETSAMLOGON_TDB));
		return false;
	}

	/* Prepare key as DOMAIN-SID/USER-RID string */
	slprintf(keystr, sizeof(keystr), "%s", sid_to_fstring(tmp, user_sid));
	DEBUG(10,("netsamlogon_cache_get: SID [%s]\n", keystr));
	data = tdb_fetch_bystring( netsamlogon_tdb, keystr );

	if (!data.dptr) {
		return NULL;
	}

	info3 = TALLOC_ZERO_P(mem_ctx, struct netr_SamInfo3);
	if (!info3) {
		goto done;
	}

	blob = data_blob_const(data.dptr, data.dsize);

	ndr_err = ndr_pull_struct_blob(&blob, mem_ctx, &r,
				      (ndr_pull_flags_fn_t)ndr_pull_netsamlogoncache_entry);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_DEBUG(netsamlogoncache_entry, &r);
	}

	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DEBUG(0,("netsamlogon_cache_get: failed to pull entry from cache\n"));
		tdb_delete(netsamlogon_tdb, data);
		TALLOC_FREE(info3);
		goto done;
	}

	info3 = (struct netr_SamInfo3 *)talloc_memdup(mem_ctx, &r.info3,
						      sizeof(r.info3));

 done:
	SAFE_FREE(data.dptr);

	return info3;

#if 0	/* The netsamlogon cache needs to hang around.  Something about
	   this feels wrong, but it is the only way we can get all of the
	   groups.  The old universal groups cache didn't expire either.
	   --jerry */
	{
		time_t		now = time(NULL);
		uint32		time_diff;

		/* is the entry expired? */
		time_diff = now - t;

		if ( (time_diff < 0 ) || (time_diff > lp_winbind_cache_time()) ) {
			DEBUG(10,("netsamlogon_cache_get: cache entry expired \n"));
			tdb_delete( netsamlogon_tdb, key );
			TALLOC_FREE( user );
		}
	}
#endif
}

bool netsamlogon_cache_have(const struct dom_sid *user_sid)
{
	TALLOC_CTX *mem_ctx = talloc_init("netsamlogon_cache_have");
	struct netr_SamInfo3 *info3 = NULL;
	bool result;

	if (!mem_ctx)
		return False;

	info3 = netsamlogon_cache_get(mem_ctx, user_sid);

	result = (info3 != NULL);

	talloc_destroy(mem_ctx);

	return result;
}
