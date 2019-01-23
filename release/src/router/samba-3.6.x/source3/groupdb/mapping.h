/*
 *  Unix SMB/CIFS implementation.
 *  RPC Pipe client / server routines
 *  Copyright (C) Andrew Tridgell              1992-2000,
 *  Copyright (C) Jean François Micouleau      1998-2001.
 *  Copyright (C) Volker Lendecke              2006.
 *  Copyright (C) Gerald Carter                2006.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#define DATABASE_VERSION_V1 1 /* native byte format. */
#define DATABASE_VERSION_V2 2 /* le format. */

#define GROUP_PREFIX "UNIXGROUP/"
#define GROUP_PREFIX_LEN 10

/* Alias memberships are stored reverse, as memberships. The performance
 * critical operation is to determine the aliases a SID is member of, not
 * listing alias members. So we store a list of alias SIDs a SID is member of
 * hanging of the member as key.
 */
#define MEMBEROF_PREFIX "MEMBEROF/"
#define MEMBEROF_PREFIX_LEN 9

/*
  groupdb mapping backend abstraction
 */
struct mapping_backend {
	bool (*init_group_mapping)(void);
	bool (*add_mapping_entry)(GROUP_MAP *map, int flag);
	bool (*get_group_map_from_sid)(struct dom_sid sid, GROUP_MAP *map);
	bool (*get_group_map_from_gid)(gid_t gid, GROUP_MAP *map);
	bool (*get_group_map_from_ntname)(const char *name, GROUP_MAP *map);
	bool (*group_map_remove)(const struct dom_sid *sid);
	bool (*enum_group_mapping)(const struct dom_sid *domsid, enum lsa_SidType sid_name_use,
				   GROUP_MAP **pp_rmap,
				   size_t *p_num_entries, bool unix_only);
	NTSTATUS (*one_alias_membership)(const struct dom_sid *member,
					 struct dom_sid **sids, size_t *num);
	NTSTATUS (*add_aliasmem)(const struct dom_sid *alias, const struct dom_sid *member);
	NTSTATUS (*del_aliasmem)(const struct dom_sid *alias, const struct dom_sid *member);
	NTSTATUS (*enum_aliasmem)(const struct dom_sid *alias, TALLOC_CTX *mem_ctx,
				  struct dom_sid **sids, size_t *num);
};
