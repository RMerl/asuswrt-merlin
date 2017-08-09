/*
   Unix SMB/CIFS implementation.

   Password and authentication handling by wbclient

   Copyright (C) Andrew Bartlett			2002
   Copyright (C) Jelmer Vernooij			2002
   Copyright (C) Simo Sorce				2003
   Copyright (C) Volker Lendecke			2006
   Copyright (C) Dan Sledz				2009

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/* This passdb module retrieves full passdb information for local users and
 * groups from a wbclient compatible daemon.
 *
 * The purpose of this module is to defer all SAM authorization information
 * storage and retrieval to a wbc compatible daemon.
 *
 * This passdb backend is most useful when used in conjunction with auth_wbc.
 *
 * A few current limitations of this module are:
 *   - read only interface
 *   - no privileges
 */

#include "includes.h"
#include "passdb.h"
#include "lib/winbind_util.h"

/***************************************************************************
  Default implementations of some functions.
 ****************************************************************************/
static NTSTATUS _pdb_wbc_sam_getsampw(struct pdb_methods *methods,
				       struct samu *user,
				       const struct passwd *pwd)
{
	NTSTATUS result = NT_STATUS_OK;

	if (pwd == NULL)
		return NT_STATUS_NO_SUCH_USER;

	ZERO_STRUCTP(user);

        /* Can we really get away with this little of information */
	user->methods = methods;
	result = samu_set_unix(user, pwd);

	return result;
}

static NTSTATUS pdb_wbc_sam_getsampwnam(struct pdb_methods *methods, struct samu *user, const char *sname)
{
	return _pdb_wbc_sam_getsampw(methods, user, winbind_getpwnam(sname));
}

static NTSTATUS pdb_wbc_sam_getsampwsid(struct pdb_methods *methods, struct samu *user, const struct dom_sid *sid)
{
	return _pdb_wbc_sam_getsampw(methods, user, winbind_getpwsid(sid));
}

static bool pdb_wbc_sam_uid_to_sid(struct pdb_methods *methods, uid_t uid,
				   struct dom_sid *sid)
{
	return winbind_uid_to_sid(sid, uid);
}

static bool pdb_wbc_sam_gid_to_sid(struct pdb_methods *methods, gid_t gid,
				   struct dom_sid *sid)
{
	return winbind_gid_to_sid(sid, gid);
}

static NTSTATUS pdb_wbc_sam_enum_group_members(struct pdb_methods *methods,
					       TALLOC_CTX *mem_ctx,
					       const struct dom_sid *group,
					       uint32 **pp_member_rids,
					       size_t *p_num_members)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS pdb_wbc_sam_enum_group_memberships(struct pdb_methods *methods,
						   TALLOC_CTX *mem_ctx,
						   struct samu *user,
						   struct dom_sid **pp_sids,
						   gid_t **pp_gids,
						   uint32_t *p_num_groups)
{
	size_t i;
	const char *username = pdb_get_username(user);
	uint32_t num_groups;

	if (!winbind_get_groups(mem_ctx, username, &num_groups, pp_gids)) {
		return NT_STATUS_NO_SUCH_USER;
	}
	*p_num_groups = num_groups;

	if (*p_num_groups == 0) {
		smb_panic("primary group missing");
	}

	*pp_sids = TALLOC_ARRAY(mem_ctx, struct dom_sid, *p_num_groups);

	if (*pp_sids == NULL) {
		TALLOC_FREE(*pp_gids);
		return NT_STATUS_NO_MEMORY;
	}

	for (i=0; i < *p_num_groups; i++) {
		gid_to_sid(&(*pp_sids)[i], (*pp_gids)[i]);
	}

	return NT_STATUS_OK;
}

static NTSTATUS pdb_wbc_sam_lookup_rids(struct pdb_methods *methods,
					const struct dom_sid *domain_sid,
					int num_rids,
					uint32 *rids,
					const char **names,
					enum lsa_SidType *attrs)
{
	NTSTATUS result = NT_STATUS_OK;
	char *domain = NULL;
	char **account_names = NULL;
	enum lsa_SidType *attr_list = NULL;
	int i;

	if (!winbind_lookup_rids(talloc_tos(), domain_sid, num_rids, rids,
				 (const char **)&domain,
				 (const char ***)&account_names, &attr_list))
	{
		result = NT_STATUS_NONE_MAPPED;
		goto done;
	}

	memcpy(attrs, attr_list, num_rids * sizeof(enum lsa_SidType));

	for (i=0; i<num_rids; i++) {
		if (attrs[i] == SID_NAME_UNKNOWN) {
			names[i] = NULL;
		} else {
			names[i] = talloc_strdup(names, account_names[i]);
			if (names[i] == NULL) {
				result = NT_STATUS_NO_MEMORY;
				goto done;
			}

		}
	}

done:
	TALLOC_FREE(account_names);
	TALLOC_FREE(domain);
	TALLOC_FREE(attr_list);
	return result;
}

static NTSTATUS pdb_wbc_sam_get_account_policy(struct pdb_methods *methods, enum pdb_policy_type type, uint32_t *value)
{
	return NT_STATUS_UNSUCCESSFUL;
}

static NTSTATUS pdb_wbc_sam_set_account_policy(struct pdb_methods *methods, enum pdb_policy_type type, uint32_t value)
{
	return NT_STATUS_UNSUCCESSFUL;
}

static bool pdb_wbc_sam_search_groups(struct pdb_methods *methods,
				      struct pdb_search *search)
{
	return false;
}

static bool pdb_wbc_sam_search_aliases(struct pdb_methods *methods,
				       struct pdb_search *search,
				       const struct dom_sid *sid)
{

	return false;
}

static bool pdb_wbc_sam_get_trusteddom_pw(struct pdb_methods *methods,
					  const char *domain,
					  char **pwd,
					  struct dom_sid *sid,
					  time_t *pass_last_set_time)
{
	return false;

}

static bool pdb_wbc_sam_set_trusteddom_pw(struct pdb_methods *methods,
					  const char *domain,
					  const char *pwd,
					  const struct dom_sid *sid)
{
	return false;
}

static bool pdb_wbc_sam_del_trusteddom_pw(struct pdb_methods *methods,
					  const char *domain)
{
	return false;
}

static NTSTATUS pdb_wbc_sam_enum_trusteddoms(struct pdb_methods *methods,
					     TALLOC_CTX *mem_ctx,
					     uint32 *num_domains,
					     struct trustdom_info ***domains)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

static bool _make_group_map(struct pdb_methods *methods, const char *domain, const char *name, enum lsa_SidType name_type, gid_t gid, struct dom_sid *sid, GROUP_MAP *map)
{
	snprintf(map->nt_name, sizeof(map->nt_name), "%s%c%s",
	        domain, *lp_winbind_separator(), name);
	map->sid_name_use = name_type;
	map->sid = *sid;
	map->gid = gid;
	return true;
}

static NTSTATUS pdb_wbc_sam_getgrsid(struct pdb_methods *methods, GROUP_MAP *map,
				 struct dom_sid sid)
{
	NTSTATUS result = NT_STATUS_OK;
	char *name = NULL;
	char *domain = NULL;
	enum lsa_SidType name_type;
	gid_t gid;

	if (!winbind_lookup_sid(talloc_tos(), &sid, (const char **)&domain,
				(const char **) &name, &name_type)) {
		result = NT_STATUS_NO_SUCH_GROUP;
		goto done;
	}

	if ((name_type != SID_NAME_DOM_GRP) &&
	    (name_type != SID_NAME_DOMAIN) &&
	    (name_type != SID_NAME_ALIAS) &&
	    (name_type != SID_NAME_WKN_GRP)) {
		result = NT_STATUS_NO_SUCH_GROUP;
		goto done;
	}

	if (!winbind_sid_to_gid(&gid, &sid)) {
		result = NT_STATUS_NO_SUCH_GROUP;
		goto done;
	}

	if (!_make_group_map(methods, domain, name, name_type, gid, &sid, map)) {
		result = NT_STATUS_NO_SUCH_GROUP;
		goto done;
	}

done:
	TALLOC_FREE(name);
	TALLOC_FREE(domain);
	return result;
}

static NTSTATUS pdb_wbc_sam_getgrgid(struct pdb_methods *methods, GROUP_MAP *map,
				 gid_t gid)
{
	NTSTATUS result = NT_STATUS_OK;
	char *name = NULL;
	char *domain = NULL;
	struct dom_sid sid;
	enum lsa_SidType name_type;

	if (!winbind_gid_to_sid(&sid, gid)) {
		result = NT_STATUS_NO_SUCH_GROUP;
		goto done;
	}

	if (!winbind_lookup_sid(talloc_tos(), &sid, (const char **)&domain,
				(const char **)&name, &name_type)) {
		result = NT_STATUS_NO_SUCH_GROUP;
		goto done;
	}

	if ((name_type != SID_NAME_DOM_GRP) &&
	    (name_type != SID_NAME_DOMAIN) &&
	    (name_type != SID_NAME_ALIAS) &&
	    (name_type != SID_NAME_WKN_GRP)) {
		result = NT_STATUS_NO_SUCH_GROUP;
		goto done;
	}

	if (!_make_group_map(methods, domain, name, name_type, gid, &sid, map)) {
		result = NT_STATUS_NO_SUCH_GROUP;
		goto done;
	}

done:
	TALLOC_FREE(name);
	TALLOC_FREE(domain);

	return result;
}

static NTSTATUS pdb_wbc_sam_getgrnam(struct pdb_methods *methods, GROUP_MAP *map,
				 const char *name)
{
	NTSTATUS result = NT_STATUS_OK;
	const char *domain = "";
	struct dom_sid sid;
	gid_t gid;
	enum lsa_SidType name_type;

	if (!winbind_lookup_name(domain, name, &sid, &name_type)) {
		result = NT_STATUS_NO_SUCH_GROUP;
		goto done;
	}

	if ((name_type != SID_NAME_DOM_GRP) &&
	    (name_type != SID_NAME_DOMAIN) &&
	    (name_type != SID_NAME_ALIAS) &&
	    (name_type != SID_NAME_WKN_GRP)) {
		result = NT_STATUS_NO_SUCH_GROUP;
		goto done;
	}

	if (!winbind_sid_to_gid(&gid, &sid)) {
		result = NT_STATUS_NO_SUCH_GROUP;
		goto done;
	}

	if (!_make_group_map(methods, domain, name, name_type, gid, &sid, map)) {
		result = NT_STATUS_NO_SUCH_GROUP;
		goto done;
	}

done:

	return result;
}

static NTSTATUS pdb_wbc_sam_enum_group_mapping(struct pdb_methods *methods,
					   const struct dom_sid *sid, enum lsa_SidType sid_name_use,
					   GROUP_MAP **pp_rmap, size_t *p_num_entries,
					   bool unix_only)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS pdb_wbc_sam_get_aliasinfo(struct pdb_methods *methods,
				   const struct dom_sid *sid,
				   struct acct_info *info)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS pdb_wbc_sam_enum_aliasmem(struct pdb_methods *methods,
					  const struct dom_sid *alias,
					  TALLOC_CTX *mem_ctx,
					  struct dom_sid **pp_members,
					  size_t *p_num_members)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS pdb_wbc_sam_alias_memberships(struct pdb_methods *methods,
				       TALLOC_CTX *mem_ctx,
				       const struct dom_sid *domain_sid,
				       const struct dom_sid *members,
				       size_t num_members,
				       uint32 **pp_alias_rids,
				       size_t *p_num_alias_rids)
{
	if (!winbind_get_sid_aliases(mem_ctx, domain_sid,
		    members, num_members, pp_alias_rids, p_num_alias_rids))
		return NT_STATUS_UNSUCCESSFUL;

	return NT_STATUS_OK;
}

static NTSTATUS pdb_init_wbc_sam(struct pdb_methods **pdb_method, const char *location)
{
	NTSTATUS result;

	if (!NT_STATUS_IS_OK(result = make_pdb_method( pdb_method))) {
		return result;
	}

	(*pdb_method)->name = "wbc_sam";

	(*pdb_method)->getsampwnam = pdb_wbc_sam_getsampwnam;
	(*pdb_method)->getsampwsid = pdb_wbc_sam_getsampwsid;

	(*pdb_method)->getgrsid = pdb_wbc_sam_getgrsid;
	(*pdb_method)->getgrgid = pdb_wbc_sam_getgrgid;
	(*pdb_method)->getgrnam = pdb_wbc_sam_getgrnam;
	(*pdb_method)->enum_group_mapping = pdb_wbc_sam_enum_group_mapping;
	(*pdb_method)->enum_group_members = pdb_wbc_sam_enum_group_members;
	(*pdb_method)->enum_group_memberships = pdb_wbc_sam_enum_group_memberships;
	(*pdb_method)->get_aliasinfo = pdb_wbc_sam_get_aliasinfo;
	(*pdb_method)->enum_aliasmem = pdb_wbc_sam_enum_aliasmem;
	(*pdb_method)->enum_alias_memberships = pdb_wbc_sam_alias_memberships;
	(*pdb_method)->lookup_rids = pdb_wbc_sam_lookup_rids;
	(*pdb_method)->get_account_policy = pdb_wbc_sam_get_account_policy;
	(*pdb_method)->set_account_policy = pdb_wbc_sam_set_account_policy;
	(*pdb_method)->uid_to_sid = pdb_wbc_sam_uid_to_sid;
	(*pdb_method)->gid_to_sid = pdb_wbc_sam_gid_to_sid;

	(*pdb_method)->search_groups = pdb_wbc_sam_search_groups;
	(*pdb_method)->search_aliases = pdb_wbc_sam_search_aliases;

	(*pdb_method)->get_trusteddom_pw = pdb_wbc_sam_get_trusteddom_pw;
	(*pdb_method)->set_trusteddom_pw = pdb_wbc_sam_set_trusteddom_pw;
	(*pdb_method)->del_trusteddom_pw = pdb_wbc_sam_del_trusteddom_pw;
	(*pdb_method)->enum_trusteddoms  = pdb_wbc_sam_enum_trusteddoms;

	(*pdb_method)->private_data = NULL;
	(*pdb_method)->free_private_data = NULL;

	return NT_STATUS_OK;
}

NTSTATUS pdb_wbc_sam_init(void)
{
	return smb_register_passdb(PASSDB_INTERFACE_VERSION, "wbc_sam", pdb_init_wbc_sam);
}
