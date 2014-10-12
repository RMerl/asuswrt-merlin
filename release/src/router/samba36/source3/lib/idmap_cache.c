/*
   Unix SMB/CIFS implementation.
   ID Mapping Cache

   Copyright (C) Volker Lendecke	2008

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.*/

#include "includes.h"
#include "idmap_cache.h"
#include "../libcli/security/security.h"

/**
 * Find a sid2uid mapping
 * @param[in] sid		the sid to map
 * @param[out] puid		where to put the result
 * @param[out] expired		is the cache entry expired?
 * @retval Was anything in the cache at all?
 *
 * If *puid == -1 this was a negative mapping.
 */

bool idmap_cache_find_sid2uid(const struct dom_sid *sid, uid_t *puid,
			      bool *expired)
{
	fstring sidstr;
	char *key;
	char *value;
	char *endptr;
	time_t timeout;
	uid_t uid;
	bool ret;

	key = talloc_asprintf(talloc_tos(), "IDMAP/SID2UID/%s",
			      sid_to_fstring(sidstr, sid));
	if (key == NULL) {
		return false;
	}
	ret = gencache_get(key, &value, &timeout);
	TALLOC_FREE(key);
	if (!ret) {
		return false;
	}
	uid = strtol(value, &endptr, 10);
	ret = (*endptr == '\0');
	SAFE_FREE(value);
	if (ret) {
		*puid = uid;
		*expired = (timeout <= time(NULL));
	}
	return ret;
}

/**
 * Find a uid2sid mapping
 * @param[in] uid		the uid to map
 * @param[out] sid		where to put the result
 * @param[out] expired		is the cache entry expired?
 * @retval Was anything in the cache at all?
 *
 * If "is_null_sid(sid)", this was a negative mapping.
 */

bool idmap_cache_find_uid2sid(uid_t uid, struct dom_sid *sid, bool *expired)
{
	char *key;
	char *value;
	time_t timeout;
	bool ret = true;

	key = talloc_asprintf(talloc_tos(), "IDMAP/UID2SID/%d", (int)uid);
	if (key == NULL) {
		return false;
	}
	ret = gencache_get(key, &value, &timeout);
	TALLOC_FREE(key);
	if (!ret) {
		return false;
	}
	ZERO_STRUCTP(sid);
	if (value[0] != '-') {
		ret = string_to_sid(sid, value);
	}
	SAFE_FREE(value);
	if (ret) {
		*expired = (timeout <= time(NULL));
	}
	return ret;
}

/**
 * Store a mapping in the idmap cache
 * @param[in] sid		the sid to map
 * @param[in] uid		the uid to map
 *
 * If both parameters are valid values, then a positive mapping in both
 * directions is stored. If "is_null_sid(sid)" is true, then this will be a
 * negative mapping of uid, we want to cache that for this uid we could not
 * find anything. Likewise if "uid==-1", then we want to cache that we did not
 * find a mapping for the sid passed here.
 */

void idmap_cache_set_sid2uid(const struct dom_sid *sid, uid_t uid)
{
	time_t now = time(NULL);
	time_t timeout;
	fstring sidstr, key, value;

	if (!is_null_sid(sid)) {
		fstr_sprintf(key, "IDMAP/SID2UID/%s",
			     sid_to_fstring(sidstr, sid));
		fstr_sprintf(value, "%d", (int)uid);
		timeout = (uid == -1)
			? lp_idmap_negative_cache_time()
			: lp_idmap_cache_time();
		gencache_set(key, value, now + timeout);
	}
	if (uid != -1) {
		fstr_sprintf(key, "IDMAP/UID2SID/%d", (int)uid);
		if (is_null_sid(sid)) {
			/* negative uid mapping */
			fstrcpy(value, "-");
			timeout = lp_idmap_negative_cache_time();
		}
		else {
			sid_to_fstring(value, sid);
			timeout = lp_idmap_cache_time();
		}
		gencache_set(key, value, now + timeout);
	}
}

/**
 * Find a sid2gid mapping
 * @param[in] sid		the sid to map
 * @param[out] pgid		where to put the result
 * @param[out] expired		is the cache entry expired?
 * @retval Was anything in the cache at all?
 *
 * If *pgid == -1 this was a negative mapping.
 */

bool idmap_cache_find_sid2gid(const struct dom_sid *sid, gid_t *pgid,
			      bool *expired)
{
	fstring sidstr;
	char *key;
	char *value;
	char *endptr;
	time_t timeout;
	gid_t gid;
	bool ret;

	key = talloc_asprintf(talloc_tos(), "IDMAP/SID2GID/%s",
			      sid_to_fstring(sidstr, sid));
	if (key == NULL) {
		return false;
	}
	ret = gencache_get(key, &value, &timeout);
	TALLOC_FREE(key);
	if (!ret) {
		return false;
	}
	gid = strtol(value, &endptr, 10);
	ret = (*endptr == '\0');
	SAFE_FREE(value);
	if (ret) {
		*pgid = gid;
		*expired = (timeout <= time(NULL));
	}
	return ret;
}

/**
 * Find a gid2sid mapping
 * @param[in] gid		the gid to map
 * @param[out] sid		where to put the result
 * @param[out] expired		is the cache entry expired?
 * @retval Was anything in the cache at all?
 *
 * If "is_null_sid(sid)", this was a negative mapping.
 */

bool idmap_cache_find_gid2sid(gid_t gid, struct dom_sid *sid, bool *expired)
{
	char *key;
	char *value;
	time_t timeout;
	bool ret = true;

	key = talloc_asprintf(talloc_tos(), "IDMAP/GID2SID/%d", (int)gid);
	if (key == NULL) {
		return false;
	}
	ret = gencache_get(key, &value, &timeout);
	TALLOC_FREE(key);
	if (!ret) {
		return false;
	}
	ZERO_STRUCTP(sid);
	if (value[0] != '-') {
		ret = string_to_sid(sid, value);
	}
	SAFE_FREE(value);
	if (ret) {
		*expired = (timeout <= time(NULL));
	}
	return ret;
}

/**
 * Store a mapping in the idmap cache
 * @param[in] sid		the sid to map
 * @param[in] gid		the gid to map
 *
 * If both parameters are valid values, then a positive mapping in both
 * directions is stored. If "is_null_sid(sid)" is true, then this will be a
 * negative mapping of gid, we want to cache that for this gid we could not
 * find anything. Likewise if "gid==-1", then we want to cache that we did not
 * find a mapping for the sid passed here.
 */

void idmap_cache_set_sid2gid(const struct dom_sid *sid, gid_t gid)
{
	time_t now = time(NULL);
	time_t timeout;
	fstring sidstr, key, value;

	if (!is_null_sid(sid)) {
		fstr_sprintf(key, "IDMAP/SID2GID/%s",
			     sid_to_fstring(sidstr, sid));
		fstr_sprintf(value, "%d", (int)gid);
		timeout = (gid == -1)
			? lp_idmap_negative_cache_time()
			: lp_idmap_cache_time();
		gencache_set(key, value, now + timeout);
	}
	if (gid != -1) {
		fstr_sprintf(key, "IDMAP/GID2SID/%d", (int)gid);
		if (is_null_sid(sid)) {
			/* negative gid mapping */
			fstrcpy(value, "-");
			timeout = lp_idmap_negative_cache_time();
		}
		else {
			sid_to_fstring(value, sid);
			timeout = lp_idmap_cache_time();
		}
		gencache_set(key, value, now + timeout);
	}
}

static char* key_xid2sid_str(TALLOC_CTX* mem_ctx, char t, const char* id) {
	return talloc_asprintf(mem_ctx, "IDMAP/%cID2SID/%s", t, id);
}

static char* key_xid2sid(TALLOC_CTX* mem_ctx, char t, int id) {
	char str[32];
	snprintf(str, sizeof(str), "%d", id);
	return key_xid2sid_str(mem_ctx, t, str);
}

static char* key_sid2xid_str(TALLOC_CTX* mem_ctx, char t, const char* sid) {
	return talloc_asprintf(mem_ctx, "IDMAP/SID2%cID/%s", t, sid);
}

/* static char* key_sid2xid(TALLOC_CTX* mem_ctx, char t, const struct dom_sid* sid) */
/* { */
/* 	char* sid_str = sid_string_talloc(mem_ctx, sid); */
/* 	char* key = key_sid2xid_str(mem_ctx, t, sid_str); */
/* 	talloc_free(sid_str); */
/* 	return key; */
/* } */

static bool idmap_cache_del_xid(char t, int xid)
{
	TALLOC_CTX* mem_ctx = talloc_stackframe();
	const char* key = key_xid2sid(mem_ctx, t, xid);
	char* sid_str = NULL;
	time_t timeout;
	bool ret = true;

	if (!gencache_get(key, &sid_str, &timeout)) {
		DEBUG(3, ("no entry: %s\n", key));
		ret = false;
		goto done;
	}

	if (sid_str[0] != '-') {
		const char* sid_key = key_sid2xid_str(mem_ctx, t, sid_str);
		if (!gencache_del(sid_key)) {
			DEBUG(2, ("failed to delete: %s\n", sid_key));
			ret = false;
		} else {
			DEBUG(5, ("delete: %s\n", sid_key));
		}

	}

	if (!gencache_del(key)) {
		DEBUG(1, ("failed to delete: %s\n", key));
		ret = false;
	} else {
		DEBUG(5, ("delete: %s\n", key));
	}

done:
	talloc_free(mem_ctx);
	return ret;
}

bool idmap_cache_del_uid(uid_t uid) {
	return idmap_cache_del_xid('U', uid);
}

bool idmap_cache_del_gid(gid_t gid) {
	return idmap_cache_del_xid('G', gid);
}

static bool idmap_cache_del_sid2xid(TALLOC_CTX* mem_ctx, char t, const char* sid)
{
	const char* sid_key = key_sid2xid_str(mem_ctx, t, sid);
	char* xid_str;
	time_t timeout;
	bool ret = true;

	if (!gencache_get(sid_key, &xid_str, &timeout)) {
		ret = false;
		goto done;
	}

	if (atoi(xid_str) != -1) {
		const char* xid_key = key_xid2sid_str(mem_ctx, t, xid_str);
		if (!gencache_del(xid_key)) {
			DEBUG(2, ("failed to delete: %s\n", xid_key));
			ret = false;
		} else {
			DEBUG(5, ("delete: %s\n", xid_key));
		}
	}

	if (!gencache_del(sid_key)) {
		DEBUG(2, ("failed to delete: %s\n", sid_key));
		ret = false;
	} else {
		DEBUG(5, ("delete: %s\n", sid_key));
	}
done:
	return ret;
}

bool idmap_cache_del_sid(const struct dom_sid *sid)
{
	TALLOC_CTX* mem_ctx = talloc_stackframe();
	const char* sid_str = sid_string_talloc(mem_ctx, sid);
	bool ret = true;

	if (!idmap_cache_del_sid2xid(mem_ctx, 'U', sid_str) &&
	    !idmap_cache_del_sid2xid(mem_ctx, 'G', sid_str))
	{
		DEBUG(3, ("no entry: %s\n", key_xid2sid_str(mem_ctx, '?', sid_str)));
		ret = false;
	}

	talloc_free(mem_ctx);
	return ret;
}
