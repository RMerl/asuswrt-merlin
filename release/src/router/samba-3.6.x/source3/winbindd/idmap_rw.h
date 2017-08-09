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

/*
 * This module implements the abstract logic for creating a new
 * SID<->Unix-ID mapping. It can be used by idmap backends that
 * need to create mappings for unmapped SIDs upon request.
 */

#ifndef _IDMAP_RW_H_
#define _IDMAP_RW_H_

#include "includes.h"
#include "idmap.h"

struct idmap_rw_ops {
	NTSTATUS (*get_new_id)(struct idmap_domain *dom, struct unixid *id);
	NTSTATUS (*set_mapping)(struct idmap_domain *dom,
				const struct id_map *map);
};

/**
 * This is the abstract mechanism of creating a new mapping
 * for a given SID. It is meant to be called called from an
 * allocating backend from within idmap_<backend>_sids_to_unixids().
 * It expects map->sid and map->xid.type to be set.
 * Upon success, the new mapping is stored by the backend and
 * map contains the new mapped xid.
 *
 * The caller has to take care of the necessary steps to
 * guarantee atomicity of the operation, e.g. wrapping
 * this call in a transaction if available.
 */
NTSTATUS idmap_rw_new_mapping(struct idmap_domain *dom,
			      struct idmap_rw_ops *ops,
			      struct id_map *map);

#endif /* _IDMAP_RW_H_ */
