/*
 * Unix SMB/CIFS implementation.
 * ID Mapping Cache
 *
 * Copyright (C) Volker Lendecke	2008
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _LIB_IDMAP_CACHE_H_
#define _LIB_IDMAP_CACHE_H_

/* The following definitions come from lib/idmap_cache.c  */

bool idmap_cache_find_sid2uid(const struct dom_sid *sid, uid_t *puid,
			      bool *expired);
bool idmap_cache_find_uid2sid(uid_t uid, struct dom_sid *sid, bool *expired);
void idmap_cache_set_sid2uid(const struct dom_sid *sid, uid_t uid);
bool idmap_cache_find_sid2gid(const struct dom_sid *sid, gid_t *pgid,
			      bool *expired);
bool idmap_cache_find_gid2sid(gid_t gid, struct dom_sid *sid, bool *expired);
void idmap_cache_set_sid2gid(const struct dom_sid *sid, gid_t gid);

bool idmap_cache_del_uid(uid_t uid);
bool idmap_cache_del_gid(gid_t gid);
bool idmap_cache_del_sid(const struct dom_sid *sid);

#endif /* _LIB_IDMAP_CACHE_H_ */
