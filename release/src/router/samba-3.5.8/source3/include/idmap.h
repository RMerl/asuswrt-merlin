#ifndef _IDMAP_H_
#define _IDMAP_H_
/* 
   Unix SMB/CIFS implementation.

   Idmap headers

   Copyright (C) Jim McDonough <jmcd@us.ibm.com> 2003
   Copyright (C) Simo Sorce 2003
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/* The interface version specifier. 
   Updated to 3 for enum types by JRA. */

/* Updated to 4, completely new interface, SSS */
/* Updated to 5, simplified interface by Volker */

#define SMB_IDMAP_INTERFACE_VERSION 5

struct idmap_domain {
	const char *name;
	struct idmap_methods *methods;
	void *private_data;
};

/* Filled out by IDMAP backends */
struct idmap_methods {

	/* Called when backend is first loaded */
	NTSTATUS (*init)(struct idmap_domain *dom, const char *params);

	/* Map an array of uids/gids to SIDs.  The caller specifies
	   the uid/gid and type. Gets back the SID. */
	NTSTATUS (*unixids_to_sids)(struct idmap_domain *dom, struct id_map **ids);

	/* Map an arry of SIDs to uids/gids.  The caller sets the SID
	   and type and gets back a uid or gid. */
	NTSTATUS (*sids_to_unixids)(struct idmap_domain *dom, struct id_map **ids);

	NTSTATUS (*set_mapping)(struct idmap_domain *dom, const struct id_map *map);
	NTSTATUS (*remove_mapping)(struct idmap_domain *dom, const struct id_map *map);

	/* Called to dump backends data */
	/* NOTE: caller must use talloc_free to free maps when done */
	NTSTATUS (*dump_data)(struct idmap_domain *dom, struct id_map **maps, int *num_maps);

	/* Called when backend is unloaded */
	NTSTATUS (*close_fn)(struct idmap_domain *dom);
};

struct idmap_alloc_methods {

	/* Called when backend is first loaded */
	NTSTATUS (*init)(const char *compat_params);

	NTSTATUS (*allocate_id)(struct unixid *id);
	NTSTATUS (*get_id_hwm)(struct unixid *id);
	NTSTATUS (*set_id_hwm)(struct unixid *id);

	/* Called when backend is unloaded */
	NTSTATUS (*close_fn)(void);
};

#endif /* _IDMAP_H_ */
