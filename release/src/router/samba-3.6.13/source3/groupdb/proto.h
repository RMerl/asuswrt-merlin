/*
 *  Unix SMB/CIFS implementation.
 *  Group Mapping Database
 *
 *  Copyright (C) Andrew Tridgell              1992-2006
 *  Copyright (C) Jean François Micouleau      1998-2001
 *  Copyright (C) Gerald Carter                2006
 *  Copyright (C) Volker Lendecke              2006
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

#ifndef _GROUPDB_PROTO_H_
#define _GROUPDB_PROTO_H_

/* The following definitions come from groupdb/mapping.c  */

NTSTATUS add_initial_entry(gid_t gid, const char *sid, enum lsa_SidType sid_name_use, const char *nt_name, const char *comment);
bool get_domain_group_from_sid(struct dom_sid sid, GROUP_MAP *map);
int smb_create_group(const char *unix_group, gid_t *new_gid);
int smb_delete_group(const char *unix_group);
int smb_set_primary_group(const char *unix_group, const char* unix_user);
int smb_add_user_group(const char *unix_group, const char *unix_user);
int smb_delete_user_group(const char *unix_group, const char *unix_user);
NTSTATUS pdb_default_getgrsid(struct pdb_methods *methods, GROUP_MAP *map,
				 struct dom_sid sid);
NTSTATUS pdb_default_getgrgid(struct pdb_methods *methods, GROUP_MAP *map,
				 gid_t gid);
NTSTATUS pdb_default_getgrnam(struct pdb_methods *methods, GROUP_MAP *map,
				 const char *name);
NTSTATUS pdb_default_add_group_mapping_entry(struct pdb_methods *methods,
						GROUP_MAP *map);
NTSTATUS pdb_default_update_group_mapping_entry(struct pdb_methods *methods,
						   GROUP_MAP *map);
NTSTATUS pdb_default_delete_group_mapping_entry(struct pdb_methods *methods,
						   struct dom_sid sid);
NTSTATUS pdb_default_enum_group_mapping(struct pdb_methods *methods,
					   const struct dom_sid *sid, enum lsa_SidType sid_name_use,
					   GROUP_MAP **pp_rmap, size_t *p_num_entries,
					   bool unix_only);
NTSTATUS pdb_default_create_alias(struct pdb_methods *methods,
				  const char *name, uint32 *rid);
NTSTATUS pdb_default_delete_alias(struct pdb_methods *methods,
				  const struct dom_sid *sid);
struct acct_info;
NTSTATUS pdb_default_get_aliasinfo(struct pdb_methods *methods,
				   const struct dom_sid *sid,
				   struct acct_info *info);
NTSTATUS pdb_default_set_aliasinfo(struct pdb_methods *methods,
				   const struct dom_sid *sid,
				   struct acct_info *info);
NTSTATUS pdb_default_add_aliasmem(struct pdb_methods *methods,
				  const struct dom_sid *alias, const struct dom_sid *member);
NTSTATUS pdb_default_del_aliasmem(struct pdb_methods *methods,
				  const struct dom_sid *alias, const struct dom_sid *member);
NTSTATUS pdb_default_enum_aliasmem(struct pdb_methods *methods,
				   const struct dom_sid *alias, TALLOC_CTX *mem_ctx,
				   struct dom_sid **pp_members,
				   size_t *p_num_members);
NTSTATUS pdb_default_alias_memberships(struct pdb_methods *methods,
				       TALLOC_CTX *mem_ctx,
				       const struct dom_sid *domain_sid,
				       const struct dom_sid *members,
				       size_t num_members,
				       uint32 **pp_alias_rids,
				       size_t *p_num_alias_rids);
NTSTATUS pdb_nop_getgrsid(struct pdb_methods *methods, GROUP_MAP *map,
				 struct dom_sid sid);
NTSTATUS pdb_nop_getgrgid(struct pdb_methods *methods, GROUP_MAP *map,
				 gid_t gid);
NTSTATUS pdb_nop_getgrnam(struct pdb_methods *methods, GROUP_MAP *map,
				 const char *name);
NTSTATUS pdb_nop_add_group_mapping_entry(struct pdb_methods *methods,
						GROUP_MAP *map);
NTSTATUS pdb_nop_update_group_mapping_entry(struct pdb_methods *methods,
						   GROUP_MAP *map);
NTSTATUS pdb_nop_delete_group_mapping_entry(struct pdb_methods *methods,
						   struct dom_sid sid);
NTSTATUS pdb_nop_enum_group_mapping(struct pdb_methods *methods,
					   enum lsa_SidType sid_name_use,
					   GROUP_MAP **rmap, size_t *num_entries,
					   bool unix_only);
bool pdb_get_dom_grp_info(const struct dom_sid *sid, struct acct_info *info);
bool pdb_set_dom_grp_info(const struct dom_sid *sid, const struct acct_info *info);
NTSTATUS pdb_create_builtin_alias(uint32 rid);

/* The following definitions come from groupdb/mapping_tdb.c  */

const struct mapping_backend *groupdb_tdb_init(void);

#endif /* _GROUPDB_PROTO_H_ */
