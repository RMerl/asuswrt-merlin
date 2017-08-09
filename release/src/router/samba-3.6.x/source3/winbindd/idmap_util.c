/* 
   Unix SMB/CIFS implementation.
   ID Mapping
   Copyright (C) Simo Sorce 2003
   Copyright (C) Jeremy Allison 2006
   Copyright (C) Michael Adam 2010

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
#include "winbindd.h"
#include "winbindd_proto.h"
#include "idmap.h"
#include "idmap_cache.h"
#include "../libcli/security/security.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_IDMAP

/*****************************************************************
 Returns the SID mapped to the given UID.
 If mapping is not possible returns an error.
*****************************************************************/  

NTSTATUS idmap_uid_to_sid(const char *domname, struct dom_sid *sid, uid_t uid)
{
	NTSTATUS ret;
	struct id_map map;
	bool expired;

	DEBUG(10,("idmap_uid_to_sid: uid = [%lu], domain = '%s'\n",
		  (unsigned long)uid, domname?domname:"NULL"));

	if (winbindd_use_idmap_cache()
	    && idmap_cache_find_uid2sid(uid, sid, &expired)) {
		DEBUG(10, ("idmap_cache_find_uid2sid found %u%s\n",
			(unsigned int)uid,
			   expired ? " (expired)": ""));
		if (expired && idmap_is_online()) {
			DEBUG(10, ("revalidating expired entry\n"));
			goto backend;
		}
		if (is_null_sid(sid)) {
			DEBUG(10, ("Returning negative cache entry\n"));
			return NT_STATUS_NONE_MAPPED;
		}
		DEBUG(10, ("Returning positive cache entry\n"));
		return NT_STATUS_OK;
	}

backend:
	map.sid = sid;
	map.xid.type = ID_TYPE_UID;
	map.xid.id = uid;

	ret = idmap_backends_unixid_to_sid(domname, &map);
	if ( ! NT_STATUS_IS_OK(ret)) {
		DEBUG(10, ("error mapping uid [%lu]\n", (unsigned long)uid));
		return ret;
	}

	if (map.status != ID_MAPPED) {
		if (winbindd_use_idmap_cache()) {
			struct dom_sid null_sid;
			ZERO_STRUCT(null_sid);
			idmap_cache_set_sid2uid(&null_sid, uid);
		}
		DEBUG(10, ("uid [%lu] not mapped\n", (unsigned long)uid));
		return NT_STATUS_NONE_MAPPED;
	}

	if (winbindd_use_idmap_cache()) {
		idmap_cache_set_sid2uid(sid, uid);
	}

	return NT_STATUS_OK;
}

/*****************************************************************
 Returns SID mapped to the given GID.
 If mapping is not possible returns an error.
*****************************************************************/  

NTSTATUS idmap_gid_to_sid(const char *domname, struct dom_sid *sid, gid_t gid)
{
	NTSTATUS ret;
	struct id_map map;
	bool expired;

	DEBUG(10,("idmap_gid_to_sid: gid = [%lu], domain = '%s'\n",
		  (unsigned long)gid, domname?domname:"NULL"));

	if (winbindd_use_idmap_cache()
	    && idmap_cache_find_gid2sid(gid, sid, &expired)) {
		DEBUG(10, ("idmap_cache_find_gid2sid found %u%s\n",
			(unsigned int)gid,
			   expired ? " (expired)": ""));
		if (expired && idmap_is_online()) {
			DEBUG(10, ("revalidating expired entry\n"));
			goto backend;
		}
		if (is_null_sid(sid)) {
			DEBUG(10, ("Returning negative cache entry\n"));
			return NT_STATUS_NONE_MAPPED;
		}
		DEBUG(10, ("Returning positive cache entry\n"));
		return NT_STATUS_OK;
	}

backend:
	map.sid = sid;
	map.xid.type = ID_TYPE_GID;
	map.xid.id = gid;

	ret = idmap_backends_unixid_to_sid(domname, &map);
	if ( ! NT_STATUS_IS_OK(ret)) {
		DEBUG(10, ("error mapping gid [%lu]\n", (unsigned long)gid));
		return ret;
	}

	if (map.status != ID_MAPPED) {
		if (winbindd_use_idmap_cache()) {
			struct dom_sid null_sid;
			ZERO_STRUCT(null_sid);
			idmap_cache_set_sid2gid(&null_sid, gid);
		}
		DEBUG(10, ("gid [%lu] not mapped\n", (unsigned long)gid));
		return NT_STATUS_NONE_MAPPED;
	}

	if (winbindd_use_idmap_cache()) {
		idmap_cache_set_sid2gid(sid, gid);
	}

	return NT_STATUS_OK;
}

/*****************************************************************
 Returns the UID mapped to the given SID.
 If mapping is not possible or SID maps to a GID returns an error.
*****************************************************************/  

NTSTATUS idmap_sid_to_uid(const char *dom_name, struct dom_sid *sid, uid_t *uid)
{
	NTSTATUS ret;
	struct id_map map;
	bool expired;

	DEBUG(10,("idmap_sid_to_uid: sid = [%s], domain = '%s'\n",
		  sid_string_dbg(sid), dom_name));

	if (winbindd_use_idmap_cache()
	    && idmap_cache_find_sid2uid(sid, uid, &expired)) {
		DEBUG(10, ("idmap_cache_find_sid2uid found %d%s\n",
			   (int)(*uid), expired ? " (expired)": ""));
		if (expired && idmap_is_online()) {
			DEBUG(10, ("revalidating expired entry\n"));
			goto backend;
		}
		if ((*uid) == -1) {
			DEBUG(10, ("Returning negative cache entry\n"));
			return NT_STATUS_NONE_MAPPED;
		}
		DEBUG(10, ("Returning positive cache entry\n"));
		return NT_STATUS_OK;
	}

backend:
	map.sid = sid;
	map.xid.type = ID_TYPE_UID;	

	ret = idmap_backends_sid_to_unixid(dom_name, &map);

	if (!NT_STATUS_IS_OK(ret)) {
		DEBUG(10, ("idmap_backends_sid_to_unixid failed: %s\n",
			   nt_errstr(ret)));
		if (winbindd_use_idmap_cache()) {
			idmap_cache_set_sid2uid(sid, -1);
		}
		return ret;
	}

	if (map.status != ID_MAPPED) {
		DEBUG(10, ("sid [%s] is not mapped\n", sid_string_dbg(sid)));
		if (winbindd_use_idmap_cache()) {
			idmap_cache_set_sid2uid(sid, -1);
		}
		return NT_STATUS_NONE_MAPPED;
	}

	if (map.xid.type != ID_TYPE_UID) {
		DEBUG(10, ("sid [%s] not mapped to a uid "
			   "[%u,%u,%u]\n",
			   sid_string_dbg(sid),
			   map.status,
			   map.xid.type,
			   map.xid.id));
		if (winbindd_use_idmap_cache()) {
			idmap_cache_set_sid2uid(sid, -1);
		}
		return NT_STATUS_NONE_MAPPED;
	}

	*uid = (uid_t)map.xid.id;
	if (winbindd_use_idmap_cache()) {
		idmap_cache_set_sid2uid(sid, *uid);
	}
	return NT_STATUS_OK;
}

/*****************************************************************
 Returns the GID mapped to the given SID.
 If mapping is not possible or SID maps to a UID returns an error.
*****************************************************************/  

NTSTATUS idmap_sid_to_gid(const char *domname, struct dom_sid *sid, gid_t *gid)
{
	NTSTATUS ret;
	struct id_map map;
	bool expired;

	DEBUG(10,("idmap_sid_to_gid: sid = [%s], domain = '%s'\n",
		  sid_string_dbg(sid), domname));

	if (winbindd_use_idmap_cache()
	    && idmap_cache_find_sid2gid(sid, gid, &expired)) {
		DEBUG(10, ("idmap_cache_find_sid2gid found %d%s\n",
			   (int)(*gid), expired ? " (expired)": ""));
		if (expired && idmap_is_online()) {
			DEBUG(10, ("revalidating expired entry\n"));
			goto backend;
		}
		if ((*gid) == -1) {
			DEBUG(10, ("Returning negative cache entry\n"));
			return NT_STATUS_NONE_MAPPED;
		}
		DEBUG(10, ("Returning positive cache entry\n"));
		return NT_STATUS_OK;
	}

backend:
	map.sid = sid;
	map.xid.type = ID_TYPE_GID;

	ret = idmap_backends_sid_to_unixid(domname, &map);

	if (!NT_STATUS_IS_OK(ret)) {
		DEBUG(10, ("idmap_backends_sid_to_unixid failed: %s\n",
			   nt_errstr(ret)));
		if (winbindd_use_idmap_cache()) {
			idmap_cache_set_sid2gid(sid, -1);
		}
		return ret;
	}

	if (map.status != ID_MAPPED) {
		DEBUG(10, ("sid [%s] is not mapped\n", sid_string_dbg(sid)));
		if (winbindd_use_idmap_cache()) {
			idmap_cache_set_sid2gid(sid, -1);
		}
		return NT_STATUS_NONE_MAPPED;
	}

	if (map.xid.type != ID_TYPE_GID) {
		DEBUG(10, ("sid [%s] not mapped to a gid "
			   "[%u,%u,%u]\n",
			   sid_string_dbg(sid),
			   map.status,
			   map.xid.type,
			   map.xid.id));
		if (winbindd_use_idmap_cache()) {
			idmap_cache_set_sid2gid(sid, -1);
		}
		return NT_STATUS_NONE_MAPPED;
	}

	*gid = map.xid.id;
	if (winbindd_use_idmap_cache()) {
		idmap_cache_set_sid2gid(sid, *gid);
	}
	return NT_STATUS_OK;
}

/**
 * check whether a given unix id is inside the filter range of an idmap domain
 */
bool idmap_unix_id_is_in_range(uint32_t id, struct idmap_domain *dom)
{
	if (id == 0) {
		/* 0 is not an allowed unix id for id mapping */
		return false;
	}

	if ((dom->low_id && (id < dom->low_id)) ||
	    (dom->high_id && (id > dom->high_id)))
	{
		return false;
	}

	return true;
}
