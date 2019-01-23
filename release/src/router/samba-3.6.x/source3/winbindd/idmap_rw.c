/*
 * Unix SMB/CIFS implementation.
 *
 * ID mapping: abstract r/w new-mapping mechanism
 *
 * Copyright (C) Michael Adam 2010
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

#include "includes.h"
#include "winbindd.h"
#include "idmap.h"
#include "idmap_rw.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_IDMAP

NTSTATUS idmap_rw_new_mapping(struct idmap_domain *dom,
			      struct idmap_rw_ops *ops,
			      struct id_map *map)
{
	NTSTATUS status;

	if (map == NULL) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if ((map->xid.type != ID_TYPE_UID) && (map->xid.type != ID_TYPE_GID)) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (map->sid == NULL) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	status = ops->get_new_id(dom, &map->xid);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(3, ("Could not allocate id: %s\n", nt_errstr(status)));
		return status;
	}

	DEBUG(10, ("Setting mapping: %s <-> %s %lu\n",
		   sid_string_dbg(map->sid),
		   (map->xid.type == ID_TYPE_UID) ? "UID" : "GID",
		   (unsigned long)map->xid.id));

	map->status = ID_MAPPED;
	status = ops->set_mapping(dom, map);

	if (NT_STATUS_EQUAL(status, NT_STATUS_OBJECT_NAME_COLLISION)) {
		struct id_map *ids[2];
		DEBUG(5, ("Mapping for %s exists - retrying to map sid\n",
			  sid_string_dbg(map->sid)));
		ids[0] = map;
		ids[1] = NULL;
		status = dom->methods->sids_to_unixids(dom, ids);
	}

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(3, ("Could not store the new mapping: %s\n",
			  nt_errstr(status)));
		return status;
	}

	return NT_STATUS_OK;
}
