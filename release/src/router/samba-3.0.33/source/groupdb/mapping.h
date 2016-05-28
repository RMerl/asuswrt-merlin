#define DATABASE_VERSION_V1 1 /* native byte format. */
#define DATABASE_VERSION_V2 2 /* le format. */

#define GROUP_PREFIX "UNIXGROUP/"

/* Alias memberships are stored reverse, as memberships. The performance
 * critical operation is to determine the aliases a SID is member of, not
 * listing alias members. So we store a list of alias SIDs a SID is member of
 * hanging of the member as key.
 */
#define MEMBEROF_PREFIX "MEMBEROF/"

/* internal prototypes */
BOOL enum_group_mapping(const DOM_SID *domsid, enum lsa_SidType sid_name_use, GROUP_MAP **pp_rmap,
			size_t *p_num_entries, BOOL unix_only);
BOOL group_map_remove(const DOM_SID *sid);
BOOL init_group_mapping(void);
NTSTATUS one_alias_membership(const DOM_SID *member,
			      DOM_SID **sids, size_t *num);
BOOL get_group_map_from_sid(DOM_SID sid, GROUP_MAP *map);
BOOL get_group_map_from_gid(gid_t gid, GROUP_MAP *map);
BOOL get_group_map_from_ntname(const char *name, GROUP_MAP *map);
BOOL add_mapping_entry(GROUP_MAP *map, int flag);
NTSTATUS add_aliasmem(const DOM_SID *alias, const DOM_SID *member);
NTSTATUS del_aliasmem(const DOM_SID *alias, const DOM_SID *member);
NTSTATUS enum_aliasmem(const DOM_SID *alias, DOM_SID **sids, size_t *num);
