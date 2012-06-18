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
