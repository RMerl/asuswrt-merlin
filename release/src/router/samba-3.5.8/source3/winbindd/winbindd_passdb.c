/* 
   Unix SMB/CIFS implementation.

   Winbind rpc backend functions

   Copyright (C) Tim Potter 2000-2001,2003
   Copyright (C) Simo Sorce 2003
   Copyright (C) Volker Lendecke 2004
   Copyright (C) Jeremy Allison 2008

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

#include "includes.h"
#include "winbindd.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_WINBIND

static NTSTATUS enum_groups_internal(struct winbindd_domain *domain,
				TALLOC_CTX *mem_ctx,
				uint32 *num_entries, 
				struct acct_info **info,
				enum lsa_SidType sidtype)
{
	struct pdb_search *search;
	struct samr_displayentry *entries;
	int i;
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;

	if (sidtype == SID_NAME_ALIAS) {
		search = pdb_search_aliases(talloc_tos(), &domain->sid);
	} else {
		search = pdb_search_groups(talloc_tos());
	}

	if (search == NULL) goto done;

	*num_entries = pdb_search_entries(search, 0, 0xffffffff, &entries);
	if (*num_entries == 0) {
		/* Zero entries isn't an error */
		result = NT_STATUS_OK;
		goto done;
	}

	*info = TALLOC_ARRAY(mem_ctx, struct acct_info, *num_entries);
	if (*info == NULL) {
		result = NT_STATUS_NO_MEMORY;
		goto done;
	}

	for (i=0; i<*num_entries; i++) {
		fstrcpy((*info)[i].acct_name, entries[i].account_name);
		fstrcpy((*info)[i].acct_desc, entries[i].description);
		(*info)[i].rid = entries[i].rid;
	}

	result = NT_STATUS_OK;
 done:
	TALLOC_FREE(search);
	return result;
}

/* List all local groups (aliases) */
static NTSTATUS enum_local_groups(struct winbindd_domain *domain,
				TALLOC_CTX *mem_ctx,
				uint32 *num_entries, 
				struct acct_info **info)
{
	return enum_groups_internal(domain,
				mem_ctx,
				num_entries,
				info,
				SID_NAME_ALIAS);
}

/* convert a single name to a sid in a domain */
static NTSTATUS name_to_sid(struct winbindd_domain *domain,
			    TALLOC_CTX *mem_ctx,
			    const char *domain_name,
			    const char *name,
			    uint32_t flags,
			    DOM_SID *sid,
			    enum lsa_SidType *type)
{
	const char *fullname;

	flags |= LOOKUP_NAME_ALL;

	if (domain_name && domain_name[0] && strchr_m(name, '\\') == NULL) {
		fullname = talloc_asprintf(mem_ctx, "%s\\%s",
				domain_name, name);
		if (fullname == NULL) {
			return NT_STATUS_NO_MEMORY;
		}
	} else {
		fullname = name;
	}

	DEBUG(10, ("Finding fullname %s\n", fullname));

	if ( !lookup_name( mem_ctx, fullname, flags, NULL, NULL, sid, type ) ) {
		return NT_STATUS_NONE_MAPPED;
	}

	DEBUG(10, ("name_to_sid for %s returned %s (%s)\n",
		fullname,
		sid_string_dbg(sid),
		sid_type_lookup((uint32)*type)));

	return NT_STATUS_OK;
}

/*
  convert a domain SID to a user or group name
*/
static NTSTATUS sid_to_name(struct winbindd_domain *domain,
			    TALLOC_CTX *mem_ctx,
			    const DOM_SID *sid,
			    char **domain_name,
			    char **name,
			    enum lsa_SidType *type)
{
	const char *dom, *nam;

	DEBUG(10, ("Converting SID %s\n", sid_string_dbg(sid)));

	/* Paranoia check */
	if (!sid_check_is_in_builtin(sid) &&
	    !sid_check_is_in_our_domain(sid) &&
	    !sid_check_is_in_unix_users(sid) &&
	    !sid_check_is_unix_users(sid) &&
	    !sid_check_is_in_unix_groups(sid) &&
	    !sid_check_is_unix_groups(sid) &&
	    !sid_check_is_in_wellknown_domain(sid))
	{
		DEBUG(0, ("Possible deadlock: Trying to lookup SID %s with "
			  "passdb backend\n", sid_string_dbg(sid)));
		return NT_STATUS_NONE_MAPPED;
	}

	if (!lookup_sid(mem_ctx, sid, &dom, &nam, type)) {
		return NT_STATUS_NONE_MAPPED;
	}

	*domain_name = talloc_strdup(mem_ctx, dom);
	*name = talloc_strdup(mem_ctx, nam);

	return NT_STATUS_OK;
}

static NTSTATUS rids_to_names(struct winbindd_domain *domain,
			      TALLOC_CTX *mem_ctx,
			      const DOM_SID *sid,
			      uint32 *rids,
			      size_t num_rids,
			      char **domain_name,
			      char ***names,
			      enum lsa_SidType **types)
{
	size_t i;
	bool have_mapped;
	bool have_unmapped;

	*domain_name = NULL;
	*names = NULL;
	*types = NULL;

	if (!num_rids) {
		return NT_STATUS_OK;
	}

	/* Paranoia check */
	if (!sid_check_is_in_builtin(sid) &&
	    !sid_check_is_in_our_domain(sid) &&
	    !sid_check_is_in_unix_users(sid) &&
	    !sid_check_is_unix_users(sid) &&
	    !sid_check_is_in_unix_groups(sid) &&
	    !sid_check_is_unix_groups(sid) &&
	    !sid_check_is_in_wellknown_domain(sid))
	{
		DEBUG(0, ("Possible deadlock: Trying to lookup SID %s with "
			  "passdb backend\n", sid_string_dbg(sid)));
		return NT_STATUS_NONE_MAPPED;
	}

	*names = TALLOC_ARRAY(mem_ctx, char *, num_rids);
	*types = TALLOC_ARRAY(mem_ctx, enum lsa_SidType, num_rids);

	if ((*names == NULL) || (*types == NULL)) {
		return NT_STATUS_NO_MEMORY;
	}

	have_mapped = have_unmapped = false;

	for (i=0; i<num_rids; i++) {
		DOM_SID lsid;
		const char *dom = NULL, *nam = NULL;
		enum lsa_SidType type = SID_NAME_UNKNOWN;

		if (!sid_compose(&lsid, sid, rids[i])) {
			return NT_STATUS_INTERNAL_ERROR;
		}

		if (!lookup_sid(mem_ctx, &lsid, &dom, &nam, &type)) {
			have_unmapped = true;
			(*types)[i] = SID_NAME_UNKNOWN;
			(*names)[i] = talloc_strdup(mem_ctx, "");
		} else {
			have_mapped = true;
			(*types)[i] = type;
			(*names)[i] = CONST_DISCARD(char *, nam);
		}

		if (*domain_name == NULL) {
			*domain_name = CONST_DISCARD(char *, dom);
		} else {
			char *dname = CONST_DISCARD(char *, dom);
			TALLOC_FREE(dname);
		}
	}

	if (!have_mapped) {
		return NT_STATUS_NONE_MAPPED;
	}
	if (!have_unmapped) {
		return NT_STATUS_OK;
	}
	return STATUS_SOME_UNMAPPED;
}

/* Lookup groups a user is a member of.  I wish Unix had a call like this! */
static NTSTATUS lookup_usergroups(struct winbindd_domain *domain,
				  TALLOC_CTX *mem_ctx,
				  const DOM_SID *user_sid,
				  uint32 *num_groups, DOM_SID **user_gids)
{
	NTSTATUS result;
	DOM_SID *groups = NULL;
	gid_t *gids = NULL;
	size_t ngroups = 0;
	struct samu *user;

	if ( (user = samu_new(mem_ctx)) == NULL ) {
		return NT_STATUS_NO_MEMORY;
	}

	if ( !pdb_getsampwsid( user, user_sid ) ) {
		TALLOC_FREE( user );
		return NT_STATUS_NO_SUCH_USER;
	}

	result = pdb_enum_group_memberships( mem_ctx, user, &groups, &gids, &ngroups );

	TALLOC_FREE( user );

	*num_groups = (uint32)ngroups;
	*user_gids = groups;

	return result;
}

static NTSTATUS lookup_useraliases(struct winbindd_domain *domain,
				   TALLOC_CTX *mem_ctx,
				   uint32 num_sids, const DOM_SID *sids,
				   uint32 *p_num_aliases, uint32 **rids)
{
	NTSTATUS result;
	size_t num_aliases = 0;

	result = pdb_enum_alias_memberships(mem_ctx, &domain->sid,
					    sids, num_sids, rids, &num_aliases);

	*p_num_aliases = num_aliases;
	return result;
}

/* find the sequence number for a domain */
static NTSTATUS sequence_number(struct winbindd_domain *domain, uint32 *seq)
{
	bool result;
	time_t seq_num;

	result = pdb_get_seq_num(&seq_num);
	if (!result) {
		*seq = 1;
	}

	*seq = (int) seq_num;
	/* *seq = 1; */
	return NT_STATUS_OK;
}

static NTSTATUS lockout_policy(struct winbindd_domain *domain,
			       TALLOC_CTX *mem_ctx,
			       struct samr_DomInfo12 *policy)
{
	/* actually we have that */
	return NT_STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS password_policy(struct winbindd_domain *domain,
				TALLOC_CTX *mem_ctx,
				struct samr_DomInfo1 *policy)
{
	struct samr_DomInfo1 *p;
	time_t u_expire, u_min_age;
	uint32 account_policy_temp;

	if ((p = TALLOC_ZERO_P(mem_ctx, struct samr_DomInfo1)) == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	if (!pdb_get_account_policy(PDB_POLICY_MIN_PASSWORD_LEN,
				    &account_policy_temp)) {
		return NT_STATUS_ACCESS_DENIED;
	}
	p->min_password_length = account_policy_temp;

	if (!pdb_get_account_policy(PDB_POLICY_PASSWORD_HISTORY,
				    &account_policy_temp)) {
		return NT_STATUS_ACCESS_DENIED;
	}
	p->password_history_length = account_policy_temp;

	if (!pdb_get_account_policy(PDB_POLICY_USER_MUST_LOGON_TO_CHG_PASS,
				    &p->password_properties)) {
		return NT_STATUS_ACCESS_DENIED;
	}

	if (!pdb_get_account_policy(PDB_POLICY_MAX_PASSWORD_AGE, &account_policy_temp)) {
		return NT_STATUS_ACCESS_DENIED;
	}
	u_expire = account_policy_temp;

	if (!pdb_get_account_policy(PDB_POLICY_MIN_PASSWORD_AGE, &account_policy_temp)) {
		return NT_STATUS_ACCESS_DENIED;
	}
	u_min_age = account_policy_temp;

	unix_to_nt_time_abs((NTTIME *)&p->max_password_age, u_expire);
	unix_to_nt_time_abs((NTTIME *)&p->min_password_age, u_min_age);

	policy = p;

	return NT_STATUS_OK;
}

/*********************************************************************
 BUILTIN specific functions.
*********************************************************************/

/* list all domain groups */
static NTSTATUS builtin_enum_dom_groups(struct winbindd_domain *domain,
				TALLOC_CTX *mem_ctx,
				uint32 *num_entries, 
				struct acct_info **info)
{
	/* BUILTIN doesn't have domain groups */
	*num_entries = 0;
	*info = NULL;
	return NT_STATUS_OK;
}

/* Query display info for a domain.  This returns enough information plus a
   bit extra to give an overview of domain users for the User Manager
   application. */
static NTSTATUS builtin_query_user_list(struct winbindd_domain *domain,
				TALLOC_CTX *mem_ctx,
				uint32 *num_entries,
				struct wbint_userinfo **info)
{
	/* We don't have users */
	*num_entries = 0;
	*info = NULL;
	return NT_STATUS_OK;
}

/* Lookup user information from a rid or username. */
static NTSTATUS builtin_query_user(struct winbindd_domain *domain,
				TALLOC_CTX *mem_ctx,
				const DOM_SID *user_sid,
				struct wbint_userinfo *user_info)
{
	return NT_STATUS_NO_SUCH_USER;
}

/* get a list of trusted domains - builtin domain */
static NTSTATUS builtin_trusted_domains(struct winbindd_domain *domain,
					TALLOC_CTX *mem_ctx,
					struct netr_DomainTrustList *trusts)
{
	ZERO_STRUCTP(trusts);
	return NT_STATUS_OK;
}

/*********************************************************************
 SAM specific functions.
*********************************************************************/

/* list all domain groups */
static NTSTATUS sam_enum_dom_groups(struct winbindd_domain *domain,
				TALLOC_CTX *mem_ctx,
				uint32 *num_entries, 
				struct acct_info **info)
{
	return enum_groups_internal(domain,
				mem_ctx,
				num_entries,
				info,
				SID_NAME_DOM_GRP);
}

static NTSTATUS sam_query_user_list(struct winbindd_domain *domain,
				TALLOC_CTX *mem_ctx,
				uint32 *num_entries,
				struct wbint_userinfo **info)
{
	struct pdb_search *ps = pdb_search_users(talloc_tos(), ACB_NORMAL);
	struct samr_displayentry *entries = NULL;
	uint32 i;

	*num_entries = 0;
	*info = NULL;

	if (!ps) {
		return NT_STATUS_NO_MEMORY;
	}

	*num_entries = pdb_search_entries(ps, 0, 0xffffffff, &entries);

	DEBUG(10, ("sam_query_user_list: found %d users\n",
		   (int)*num_entries));

	*info = TALLOC_ZERO_ARRAY(mem_ctx, struct wbint_userinfo, *num_entries);
	if (!(*info)) {
		TALLOC_FREE(ps);
		return NT_STATUS_NO_MEMORY;
	}

	for (i = 0; i < *num_entries; i++) {
		struct samr_displayentry *e = &entries[i];

		(*info)[i].acct_name = talloc_strdup(mem_ctx, e->account_name );
		(*info)[i].full_name = talloc_strdup(mem_ctx, e->fullname );
		(*info)[i].homedir = NULL;
		(*info)[i].shell = NULL;
		sid_compose(&(*info)[i].user_sid, &domain->sid, e->rid);

		/* For the moment we set the primary group for
		   every user to be the Domain Users group.
		   There are serious problems with determining
		   the actual primary group for large domains.
		   This should really be made into a 'winbind
		   force group' smb.conf parameter or
		   something like that. */

		sid_compose(&(*info)[i].group_sid, &domain->sid,
				DOMAIN_GROUP_RID_USERS);
	}

	TALLOC_FREE(ps);
	return NT_STATUS_OK;
}

/* Lookup user information from a rid or username. */
static NTSTATUS sam_query_user(struct winbindd_domain *domain,
                           TALLOC_CTX *mem_ctx,
                           const DOM_SID *user_sid,
                           struct wbint_userinfo *user_info)
{
	struct samu *sampass = NULL;

	ZERO_STRUCTP(user_info);

	if (!sid_check_is_in_our_domain(user_sid)) {
		return NT_STATUS_NO_SUCH_USER;
	}

	DEBUG(10,("sam_query_user: getting samu info for sid %s\n",
		sid_string_dbg(user_sid) ));

	if (!(sampass = samu_new(mem_ctx))) {
		return NT_STATUS_NO_MEMORY;
	}

	if (!pdb_getsampwsid(sampass, user_sid)) {
		TALLOC_FREE(sampass);
		return NT_STATUS_NO_SUCH_USER;
	}

	if (pdb_get_group_sid(sampass) == NULL) {
		TALLOC_FREE(sampass);
		return NT_STATUS_NO_SUCH_GROUP;
	}

	DEBUG(10,("sam_query_user: group sid %s\n",
		sid_string_dbg(sampass->group_sid) ));

	sid_copy(&user_info->user_sid, user_sid);
	sid_copy(&user_info->group_sid, sampass->group_sid);

	user_info->acct_name = talloc_strdup(mem_ctx, sampass->username ?
					sampass->username : "");
	user_info->full_name = talloc_strdup(mem_ctx, sampass->full_name ?
					sampass->full_name : "");
	user_info->homedir = talloc_strdup(mem_ctx, sampass->home_dir ?
					sampass->home_dir : "");
	if (sampass->unix_pw && sampass->unix_pw->pw_shell) {
		user_info->shell = talloc_strdup(mem_ctx, sampass->unix_pw->pw_shell);
	} else {
		user_info->shell = talloc_strdup(mem_ctx, "");
	}
	user_info->primary_gid = sampass->unix_pw ? sampass->unix_pw->pw_gid : (gid_t)-1;

	TALLOC_FREE(sampass);
	return NT_STATUS_OK;
}

/* Lookup group membership given a rid.   */
static NTSTATUS sam_lookup_groupmem(struct winbindd_domain *domain,
				    TALLOC_CTX *mem_ctx,
				    const DOM_SID *group_sid,
				    enum lsa_SidType type,
				    uint32 *num_names,
				    DOM_SID **sid_mem, char ***names,
				    uint32 **name_types)
{
	size_t i, num_members, num_mapped;
	NTSTATUS result;
	const DOM_SID **sids;
	struct lsa_dom_info *lsa_domains;
	struct lsa_name_info *lsa_names;
	TALLOC_CTX *tmp_ctx;

	DEBUG(10,("passdb: lookup_groupmem (sam) %s sid=%s\n", domain->name,
		  sid_string_dbg(group_sid)));

	if (sid_check_is_in_builtin(group_sid) && (type != SID_NAME_ALIAS)) {
		/* There's no groups, only aliases in BUILTIN */
		return NT_STATUS_NO_SUCH_GROUP;
	}

	if (!(tmp_ctx = talloc_init("lookup_groupmem"))) {
		return NT_STATUS_NO_MEMORY;
	}

	if (type == SID_NAME_DOM_GRP) {
		uint32 *rids;

		result = pdb_enum_group_members(tmp_ctx, group_sid, &rids,
						&num_members);
		if (!NT_STATUS_IS_OK(result)) {
			TALLOC_FREE(tmp_ctx);
			return result;
		}
		*sid_mem = talloc_array(mem_ctx, struct dom_sid, num_members);
		if (*sid_mem == NULL) {
			TALLOC_FREE(tmp_ctx);
			return NT_STATUS_NO_MEMORY;
		}
		for (i=0; i<num_members; i++) {
			sid_compose(&((*sid_mem)[i]), &domain->sid, rids[i]);
		}
		TALLOC_FREE(rids);
	} else {
		result = pdb_enum_aliasmem(group_sid, mem_ctx, sid_mem,
					   &num_members);
		if (!NT_STATUS_IS_OK(result)) {
			TALLOC_FREE(tmp_ctx);
			return result;
		}
	}

	if (num_members == 0) {
		*num_names = 0;
		*sid_mem = NULL;
		*names = NULL;
		*name_types = NULL;
		TALLOC_FREE(tmp_ctx);
		return NT_STATUS_OK;
	}

	*names = TALLOC_ARRAY(mem_ctx, char *, num_members);
	*name_types = TALLOC_ARRAY(mem_ctx, uint32, num_members);
	sids = TALLOC_ARRAY(tmp_ctx, const DOM_SID *, num_members);

	if (((*names) == NULL) || ((*name_types) == NULL) || (sids == NULL)) {
		TALLOC_FREE(tmp_ctx);
		return NT_STATUS_NO_MEMORY;
	}

	/*
	 * Prepare an array of sid pointers for the lookup_sids calling
	 * convention.
	 */

	for (i=0; i<num_members; i++) {
		sids[i] = &((*sid_mem)[i]);
	}

	result = lookup_sids(tmp_ctx, num_members, sids, 1,
			     &lsa_domains, &lsa_names);
	if (!NT_STATUS_IS_OK(result)) {
		TALLOC_FREE(tmp_ctx);
		return result;
	}

	num_mapped = 0;
	for (i=0; i<num_members; i++) {
		if (lsa_names[i].type != SID_NAME_USER) {
			DEBUG(2, ("Got %s as group member -- ignoring\n",
				  sid_type_lookup(lsa_names[i].type)));
			continue;
		}
		if (!((*names)[num_mapped] = talloc_strdup((*names),
						  lsa_names[i].name))) {
			TALLOC_FREE(tmp_ctx);
			return NT_STATUS_NO_MEMORY;
		}

		(*name_types)[num_mapped] = lsa_names[i].type;

		num_mapped += 1;
	}

	*num_names = num_mapped;

	TALLOC_FREE(tmp_ctx);
	return NT_STATUS_OK;
}

/* get a list of trusted domains */
static NTSTATUS sam_trusted_domains(struct winbindd_domain *domain,
				    TALLOC_CTX *mem_ctx,
				    struct netr_DomainTrustList *trusts)
{
	NTSTATUS nt_status;
	struct trustdom_info **domains;
	int i;

	nt_status = pdb_enum_trusteddoms(talloc_tos(), &trusts->count,
					 &domains);
	if (!NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	}

	if (trusts->count == 0) {
		trusts->array = NULL;
		return NT_STATUS_OK;
	}

	trusts->array = talloc_zero_array(
		mem_ctx, struct netr_DomainTrust, trusts->count);
	if (trusts->array == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	for (i=0; i<trusts->count; i++) {
		struct dom_sid *sid;

		trusts->array[i].netbios_name = talloc_move(
			trusts->array, &domains[i]->name);
		trusts->array[i].dns_name = NULL;

		sid = talloc(trusts->array, struct dom_sid);
		if (sid == NULL) {
			return NT_STATUS_NO_MEMORY;
		}
		sid_copy(sid, &domains[i]->sid);
		trusts->array[i].sid = sid;
	}
	return NT_STATUS_OK;
}

/* the rpc backend methods are exposed via this structure */
struct winbindd_methods builtin_passdb_methods = {
	false,
	builtin_query_user_list,
	builtin_enum_dom_groups,
	enum_local_groups,
	name_to_sid,
	sid_to_name,
	rids_to_names,
	builtin_query_user,
	lookup_usergroups,
	lookup_useraliases,
	sam_lookup_groupmem,
	sequence_number,
	lockout_policy,
	password_policy,
	builtin_trusted_domains,
};

/* the rpc backend methods are exposed via this structure */
struct winbindd_methods sam_passdb_methods = {
	false,
	sam_query_user_list,
	sam_enum_dom_groups,
	enum_local_groups,
	name_to_sid,
	sid_to_name,
	rids_to_names,
	sam_query_user,
	lookup_usergroups,
	lookup_useraliases,
	sam_lookup_groupmem,
	sequence_number,
	lockout_policy,
	password_policy,
	sam_trusted_domains,
};
