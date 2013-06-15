/* 
   Unix SMB/CIFS implementation.
   ID Mapping
   Copyright (C) Simo Sorce 2003
   Copyright (C) Jeremy Allison 2006

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.*/

#include "includes.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_IDMAP

/*****************************************************************
 Returns the SID mapped to the given UID.
 If mapping is not possible returns an error.
*****************************************************************/  

NTSTATUS idmap_uid_to_sid(DOM_SID *sid, uid_t uid)
{
	NTSTATUS ret;
	struct id_map map;
	struct id_map *maps[2];

	DEBUG(10,("uid = [%lu]\n", (unsigned long)uid));

	map.sid = sid;
	map.xid.type = ID_TYPE_UID;
	map.xid.id = uid;

	maps[0] = &map;
	maps[1] = NULL;
	
	ret = idmap_unixids_to_sids(maps);
	if ( ! NT_STATUS_IS_OK(ret)) {
		DEBUG(10, ("error mapping uid [%lu]\n", (unsigned long)uid));
		return ret;
	}

	if (map.status != ID_MAPPED) {
		DEBUG(10, ("uid [%lu] not mapped\n", (unsigned long)uid));
		return NT_STATUS_NONE_MAPPED;
	}

	return NT_STATUS_OK;
}

/*****************************************************************
 Returns SID mapped to the given GID.
 If mapping is not possible returns an error.
*****************************************************************/  

NTSTATUS idmap_gid_to_sid(DOM_SID *sid, gid_t gid)
{
	NTSTATUS ret;
	struct id_map map;
	struct id_map *maps[2];

	DEBUG(10,("gid = [%lu]\n", (unsigned long)gid));

	map.sid = sid;
	map.xid.type = ID_TYPE_GID;
	map.xid.id = gid;

	maps[0] = &map;
	maps[1] = NULL;
	
	ret = idmap_unixids_to_sids(maps);
	if ( ! NT_STATUS_IS_OK(ret)) {
		DEBUG(10, ("error mapping gid [%lu]\n", (unsigned long)gid));
		return ret;
	}

	if (map.status != ID_MAPPED) {
		DEBUG(10, ("gid [%lu] not mapped\n", (unsigned long)gid));
		return NT_STATUS_NONE_MAPPED;
	}

	return NT_STATUS_OK;
}

/*****************************************************************
 Returns the UID mapped to the given SID.
 If mapping is not possible or SID maps to a GID returns an error.
*****************************************************************/  

NTSTATUS idmap_sid_to_uid(DOM_SID *sid, uid_t *uid)
{
	NTSTATUS ret;
	struct id_map map;
	struct id_map *maps[2];

	DEBUG(10,("idmap_sid_to_uid: sid = [%s]\n", sid_string_static(sid)));

	map.sid = sid;
	map.xid.type = ID_TYPE_UID;	
	
	maps[0] = &map;
	maps[1] = NULL;
	
	ret = idmap_sids_to_unixids(maps);
	if ( ! NT_STATUS_IS_OK(ret)) {
		DEBUG(10, ("error mapping sid [%s] to uid\n", 
			   sid_string_static(sid)));
		return ret;
	}

	if ((map.status != ID_MAPPED) || (map.xid.type != ID_TYPE_UID)) {
		DEBUG(10, ("sid [%s] not mapped to an uid [%u,%u,%u]\n", 
			   sid_string_static(sid), 
			   map.status, 
			   map.xid.type, 
			   map.xid.id));
		return NT_STATUS_NONE_MAPPED;
	}

	*uid = map.xid.id;

	return NT_STATUS_OK;
}

/*****************************************************************
 Returns the GID mapped to the given SID.
 If mapping is not possible or SID maps to a UID returns an error.
*****************************************************************/  

NTSTATUS idmap_sid_to_gid(DOM_SID *sid, gid_t *gid)
{
	NTSTATUS ret;
	struct id_map map;
	struct id_map *maps[2];

	DEBUG(10,("idmap_sid_to_gid: sid = [%s]\n", sid_string_static(sid)));

	map.sid = sid;
	map.xid.type = ID_TYPE_GID;
	
	maps[0] = &map;
	maps[1] = NULL;
	
	ret = idmap_sids_to_unixids(maps);
	if ( ! NT_STATUS_IS_OK(ret)) {
		DEBUG(10, ("error mapping sid [%s] to gid\n", 
			   sid_string_static(sid)));
		return ret;
	}

	if ((map.status != ID_MAPPED) || (map.xid.type != ID_TYPE_GID)) {
		DEBUG(10, ("sid [%s] not mapped to an gid [%u,%u,%u]\n", 
			   sid_string_static(sid), 
			   map.status, 
			   map.xid.type, 
			   map.xid.id));
		return NT_STATUS_NONE_MAPPED;
	}

	*gid = map.xid.id;

	return NT_STATUS_OK;
}
