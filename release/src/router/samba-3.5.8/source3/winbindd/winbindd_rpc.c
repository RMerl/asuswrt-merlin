/* 
   Unix SMB/CIFS implementation.

   Winbind rpc backend functions

   Copyright (C) Tim Potter 2000-2001,2003
   Copyright (C) Andrew Tridgell 2001
   Copyright (C) Volker Lendecke 2005
   Copyright (C) Guenther Deschner 2008 (pidl conversion)

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
#include "../librpc/gen_ndr/cli_samr.h"
#include "../librpc/gen_ndr/cli_lsa.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_WINBIND


/* Query display info for a domain.  This returns enough information plus a
   bit extra to give an overview of domain users for the User Manager
   application. */
static NTSTATUS query_user_list(struct winbindd_domain *domain,
			       TALLOC_CTX *mem_ctx,
			       uint32 *num_entries, 
			       struct wbint_userinfo **info)
{
	NTSTATUS result;
	struct policy_handle dom_pol;
	unsigned int i, start_idx;
	uint32 loop_count;
	struct rpc_pipe_client *cli;

	DEBUG(3,("rpc: query_user_list\n"));

	*num_entries = 0;
	*info = NULL;

	if ( !winbindd_can_contact_domain( domain ) ) {
		DEBUG(10,("query_user_list: No incoming trust for domain %s\n",
			  domain->name));
		return NT_STATUS_OK;
	}

	result = cm_connect_sam(domain, mem_ctx, &cli, &dom_pol);
	if (!NT_STATUS_IS_OK(result))
		return result;

	i = start_idx = 0;
	loop_count = 0;

	do {
		uint32 num_dom_users, j;
		uint32 max_entries, max_size;
		uint32_t total_size, returned_size;

		union samr_DispInfo disp_info;

		/* this next bit is copied from net_user_list_internal() */

		get_query_dispinfo_params(loop_count, &max_entries,
					  &max_size);

		result = rpccli_samr_QueryDisplayInfo(cli, mem_ctx,
						      &dom_pol,
						      1,
						      start_idx,
						      max_entries,
						      max_size,
						      &total_size,
						      &returned_size,
						      &disp_info);

		if (!NT_STATUS_IS_OK(result)) {
		        if (!NT_STATUS_EQUAL(result, STATUS_MORE_ENTRIES)) {
		                return result;
		        }
		}

		num_dom_users = disp_info.info1.count;
		start_idx += disp_info.info1.count;
		loop_count++;

		*num_entries += num_dom_users;

		*info = TALLOC_REALLOC_ARRAY(mem_ctx, *info,
					     struct wbint_userinfo,
					     *num_entries);

		if (!(*info)) {
			return NT_STATUS_NO_MEMORY;
		}

		for (j = 0; j < num_dom_users; i++, j++) {

			uint32_t rid = disp_info.info1.entries[j].rid;

			(*info)[i].acct_name = talloc_strdup(mem_ctx,
				disp_info.info1.entries[j].account_name.string);
			(*info)[i].full_name = talloc_strdup(mem_ctx,
				disp_info.info1.entries[j].full_name.string);
			(*info)[i].homedir = NULL;
			(*info)[i].shell = NULL;
			sid_compose(&(*info)[i].user_sid, &domain->sid, rid);

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

	} while (NT_STATUS_EQUAL(result, STATUS_MORE_ENTRIES));

	return result;
}

/* list all domain groups */
static NTSTATUS enum_dom_groups(struct winbindd_domain *domain,
				TALLOC_CTX *mem_ctx,
				uint32 *num_entries, 
				struct acct_info **info)
{
	struct policy_handle dom_pol;
	NTSTATUS status;
	uint32 start = 0;
	struct rpc_pipe_client *cli;

	*num_entries = 0;
	*info = NULL;

	DEBUG(3,("rpc: enum_dom_groups\n"));

	if ( !winbindd_can_contact_domain( domain ) ) {
		DEBUG(10,("enum_domain_groups: No incoming trust for domain %s\n",
			  domain->name));
		return NT_STATUS_OK;
	}

	status = cm_connect_sam(domain, mem_ctx, &cli, &dom_pol);
	if (!NT_STATUS_IS_OK(status))
		return status;

	do {
		struct samr_SamArray *sam_array = NULL;
		uint32 count = 0;
		TALLOC_CTX *mem_ctx2;
		int g;

		mem_ctx2 = talloc_init("enum_dom_groups[rpc]");

		/* start is updated by this call. */
		status = rpccli_samr_EnumDomainGroups(cli, mem_ctx2,
						      &dom_pol,
						      &start,
						      &sam_array,
						      0xFFFF, /* buffer size? */
						      &count);

		if (!NT_STATUS_IS_OK(status) &&
		    !NT_STATUS_EQUAL(status, STATUS_MORE_ENTRIES)) {
			talloc_destroy(mem_ctx2);
			break;
		}

		(*info) = TALLOC_REALLOC_ARRAY(mem_ctx, *info,
					       struct acct_info,
					       (*num_entries) + count);
		if (! *info) {
			talloc_destroy(mem_ctx2);
			return NT_STATUS_NO_MEMORY;
		}

		for (g=0; g < count; g++) {

			fstrcpy((*info)[*num_entries + g].acct_name,
				sam_array->entries[g].name.string);
			(*info)[*num_entries + g].rid = sam_array->entries[g].idx;
		}

		(*num_entries) += count;
		talloc_destroy(mem_ctx2);
	} while (NT_STATUS_EQUAL(status, STATUS_MORE_ENTRIES));

	return status;
}

/* List all domain groups */

static NTSTATUS enum_local_groups(struct winbindd_domain *domain,
				TALLOC_CTX *mem_ctx,
				uint32 *num_entries, 
				struct acct_info **info)
{
	struct policy_handle dom_pol;
	NTSTATUS result;
	struct rpc_pipe_client *cli;

	*num_entries = 0;
	*info = NULL;

	DEBUG(3,("rpc: enum_local_groups\n"));

	if ( !winbindd_can_contact_domain( domain ) ) {
		DEBUG(10,("enum_local_groups: No incoming trust for domain %s\n",
			  domain->name));
		return NT_STATUS_OK;
	}

	result = cm_connect_sam(domain, mem_ctx, &cli, &dom_pol);
	if (!NT_STATUS_IS_OK(result))
		return result;

	do {
		struct samr_SamArray *sam_array = NULL;
		uint32 count = 0, start = *num_entries;
		TALLOC_CTX *mem_ctx2;
		int g;

		mem_ctx2 = talloc_init("enum_dom_local_groups[rpc]");

		result = rpccli_samr_EnumDomainAliases(cli, mem_ctx2,
						       &dom_pol,
						       &start,
						       &sam_array,
						       0xFFFF, /* buffer size? */
						       &count);
		if (!NT_STATUS_IS_OK(result) &&
		    !NT_STATUS_EQUAL(result, STATUS_MORE_ENTRIES) )
		{
			talloc_destroy(mem_ctx2);
			return result;
		}

		(*info) = TALLOC_REALLOC_ARRAY(mem_ctx, *info,
					       struct acct_info,
					       (*num_entries) + count);
		if (! *info) {
			talloc_destroy(mem_ctx2);
			return NT_STATUS_NO_MEMORY;
		}

		for (g=0; g < count; g++) {

			fstrcpy((*info)[*num_entries + g].acct_name,
				sam_array->entries[g].name.string);
			(*info)[*num_entries + g].rid = sam_array->entries[g].idx;
		}

		(*num_entries) += count;
		talloc_destroy(mem_ctx2);

	} while (NT_STATUS_EQUAL(result, STATUS_MORE_ENTRIES));

	return result;
}

/* convert a single name to a sid in a domain */
static NTSTATUS msrpc_name_to_sid(struct winbindd_domain *domain,
				  TALLOC_CTX *mem_ctx,
				  const char *domain_name,
				  const char *name,
				  uint32_t flags,
				  DOM_SID *sid,
				  enum lsa_SidType *type)
{
	NTSTATUS result;
	DOM_SID *sids = NULL;
	enum lsa_SidType *types = NULL;
	char *full_name = NULL;
	NTSTATUS name_map_status = NT_STATUS_UNSUCCESSFUL;
	char *mapped_name = NULL;

	if (name == NULL || *name=='\0') {
		full_name = talloc_asprintf(mem_ctx, "%s", domain_name);
	} else if (domain_name == NULL || *domain_name == '\0') {
		full_name = talloc_asprintf(mem_ctx, "%s", name);
	} else {
		full_name = talloc_asprintf(mem_ctx, "%s\\%s", domain_name, name);
	}
	if (!full_name) {
		DEBUG(0, ("talloc_asprintf failed!\n"));
		return NT_STATUS_NO_MEMORY;
	}

	DEBUG(3,("rpc: name_to_sid name=%s\n", full_name));

	name_map_status = normalize_name_unmap(mem_ctx, full_name,
					       &mapped_name);

	/* Reset the full_name pointer if we mapped anytthing */

	if (NT_STATUS_IS_OK(name_map_status) ||
	    NT_STATUS_EQUAL(name_map_status, NT_STATUS_FILE_RENAMED))
	{
		full_name = mapped_name;
	}

	DEBUG(3,("name_to_sid [rpc] %s for domain %s\n",
		 full_name?full_name:"", domain_name ));

	result = winbindd_lookup_names(mem_ctx, domain, 1,
				       (const char **)&full_name, NULL,
				       &sids, &types);
	if (!NT_STATUS_IS_OK(result))
		return result;

	/* Return rid and type if lookup successful */

	sid_copy(sid, &sids[0]);
	*type = types[0];

	return NT_STATUS_OK;
}

/*
  convert a domain SID to a user or group name
*/
static NTSTATUS msrpc_sid_to_name(struct winbindd_domain *domain,
				  TALLOC_CTX *mem_ctx,
				  const DOM_SID *sid,
				  char **domain_name,
				  char **name,
				  enum lsa_SidType *type)
{
	char **domains;
	char **names;
	enum lsa_SidType *types = NULL;
	NTSTATUS result;
	NTSTATUS name_map_status = NT_STATUS_UNSUCCESSFUL;
	char *mapped_name = NULL;

	DEBUG(3,("sid_to_name [rpc] %s for domain %s\n", sid_string_dbg(sid),
		 domain->name ));

	result = winbindd_lookup_sids(mem_ctx,
				      domain,
				      1,
				      sid,
				      &domains,
				      &names,
				      &types);
	if (!NT_STATUS_IS_OK(result)) {
		DEBUG(2,("msrpc_sid_to_name: failed to lookup sids: %s\n",
			nt_errstr(result)));
		return result;
	}


	*type = (enum lsa_SidType)types[0];
	*domain_name = domains[0];
	*name = names[0];

	DEBUG(5,("Mapped sid to [%s]\\[%s]\n", domains[0], *name));

	name_map_status = normalize_name_map(mem_ctx, domain, *name,
					     &mapped_name);
	if (NT_STATUS_IS_OK(name_map_status) ||
	    NT_STATUS_EQUAL(name_map_status, NT_STATUS_FILE_RENAMED))
	{
		*name = mapped_name;
		DEBUG(5,("returning mapped name -- %s\n", *name));
	}

	return NT_STATUS_OK;
}

static NTSTATUS msrpc_rids_to_names(struct winbindd_domain *domain,
				    TALLOC_CTX *mem_ctx,
				    const DOM_SID *sid,
				    uint32 *rids,
				    size_t num_rids,
				    char **domain_name,
				    char ***names,
				    enum lsa_SidType **types)
{
	char **domains;
	NTSTATUS result;
	DOM_SID *sids;
	size_t i;
	char **ret_names;

	DEBUG(3, ("rids_to_names [rpc] for domain %s\n", domain->name ));

	if (num_rids) {
		sids = TALLOC_ARRAY(mem_ctx, DOM_SID, num_rids);
		if (sids == NULL) {
			return NT_STATUS_NO_MEMORY;
		}
	} else {
		sids = NULL;
	}

	for (i=0; i<num_rids; i++) {
		if (!sid_compose(&sids[i], sid, rids[i])) {
			return NT_STATUS_INTERNAL_ERROR;
		}
	}

	result = winbindd_lookup_sids(mem_ctx,
				      domain,
				      num_rids,
				      sids,
				      &domains,
				      names,
				      types);

	if (!NT_STATUS_IS_OK(result) &&
	    !NT_STATUS_EQUAL(result, STATUS_SOME_UNMAPPED)) {
		return result;
	}

	ret_names = *names;
	for (i=0; i<num_rids; i++) {
		NTSTATUS name_map_status = NT_STATUS_UNSUCCESSFUL;
		char *mapped_name = NULL;

		if ((*types)[i] != SID_NAME_UNKNOWN) {
			name_map_status = normalize_name_map(mem_ctx,
							     domain,
							     ret_names[i],
							     &mapped_name);
			if (NT_STATUS_IS_OK(name_map_status) ||
			    NT_STATUS_EQUAL(name_map_status, NT_STATUS_FILE_RENAMED))
			{
				ret_names[i] = mapped_name;
			}

			*domain_name = domains[i];
		}
	}

	return result;
}

/* Lookup user information from a rid or username. */
static NTSTATUS query_user(struct winbindd_domain *domain, 
			   TALLOC_CTX *mem_ctx, 
			   const DOM_SID *user_sid, 
			   struct wbint_userinfo *user_info)
{
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;
	struct policy_handle dom_pol, user_pol;
	union samr_UserInfo *info = NULL;
	uint32 user_rid;
	struct netr_SamInfo3 *user;
	struct rpc_pipe_client *cli;

	DEBUG(3,("rpc: query_user sid=%s\n", sid_string_dbg(user_sid)));

	if (!sid_peek_check_rid(&domain->sid, user_sid, &user_rid))
		return NT_STATUS_UNSUCCESSFUL;

	user_info->homedir = NULL;
	user_info->shell = NULL;
	user_info->primary_gid = (gid_t)-1;

	/* try netsamlogon cache first */

	if ( (user = netsamlogon_cache_get( mem_ctx, user_sid )) != NULL ) 
	{

		DEBUG(5,("query_user: Cache lookup succeeded for %s\n", 
			sid_string_dbg(user_sid)));

		sid_compose(&user_info->user_sid, &domain->sid, user->base.rid);
		sid_compose(&user_info->group_sid, &domain->sid,
			    user->base.primary_gid);

		user_info->acct_name = talloc_strdup(mem_ctx,
						     user->base.account_name.string);
		user_info->full_name = talloc_strdup(mem_ctx,
						     user->base.full_name.string);

		TALLOC_FREE(user);

		return NT_STATUS_OK;
	}

	if ( !winbindd_can_contact_domain( domain ) ) {
		DEBUG(10,("query_user: No incoming trust for domain %s\n",
			  domain->name));
		return NT_STATUS_OK;
	}

	/* no cache; hit the wire */

	result = cm_connect_sam(domain, mem_ctx, &cli, &dom_pol);
	if (!NT_STATUS_IS_OK(result))
		return result;

	/* Get user handle */
	result = rpccli_samr_OpenUser(cli, mem_ctx,
				      &dom_pol,
				      SEC_FLAG_MAXIMUM_ALLOWED,
				      user_rid,
				      &user_pol);

	if (!NT_STATUS_IS_OK(result))
		return result;

	/* Get user info */
	result = rpccli_samr_QueryUserInfo(cli, mem_ctx,
					   &user_pol,
					   0x15,
					   &info);

	rpccli_samr_Close(cli, mem_ctx, &user_pol);

	if (!NT_STATUS_IS_OK(result))
		return result;

	sid_compose(&user_info->user_sid, &domain->sid, user_rid);
	sid_compose(&user_info->group_sid, &domain->sid,
		    info->info21.primary_gid);
	user_info->acct_name = talloc_strdup(mem_ctx,
					     info->info21.account_name.string);
	user_info->full_name = talloc_strdup(mem_ctx,
					     info->info21.full_name.string);
	user_info->homedir = NULL;
	user_info->shell = NULL;
	user_info->primary_gid = (gid_t)-1;

	return NT_STATUS_OK;
}                                   

/* Lookup groups a user is a member of.  I wish Unix had a call like this! */
static NTSTATUS lookup_usergroups(struct winbindd_domain *domain,
				  TALLOC_CTX *mem_ctx,
				  const DOM_SID *user_sid,
				  uint32 *num_groups, DOM_SID **user_grpsids)
{
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;
	struct policy_handle dom_pol, user_pol;
	uint32 des_access = SEC_FLAG_MAXIMUM_ALLOWED;
	struct samr_RidWithAttributeArray *rid_array = NULL;
	unsigned int i;
	uint32 user_rid;
	struct rpc_pipe_client *cli;

	DEBUG(3,("rpc: lookup_usergroups sid=%s\n", sid_string_dbg(user_sid)));

	if (!sid_peek_check_rid(&domain->sid, user_sid, &user_rid))
		return NT_STATUS_UNSUCCESSFUL;

	*num_groups = 0;
	*user_grpsids = NULL;

	/* so lets see if we have a cached user_info_3 */
	result = lookup_usergroups_cached(domain, mem_ctx, user_sid, 
					  num_groups, user_grpsids);

	if (NT_STATUS_IS_OK(result)) {
		return NT_STATUS_OK;
	}

	if ( !winbindd_can_contact_domain( domain ) ) {
		DEBUG(10,("lookup_usergroups: No incoming trust for domain %s\n",
			  domain->name));

		/* Tell the cache manager not to remember this one */

		return NT_STATUS_SYNCHRONIZATION_REQUIRED;
	}

	/* no cache; hit the wire */

	result = cm_connect_sam(domain, mem_ctx, &cli, &dom_pol);
	if (!NT_STATUS_IS_OK(result))
		return result;

	/* Get user handle */
	result = rpccli_samr_OpenUser(cli, mem_ctx,
				      &dom_pol,
				      des_access,
				      user_rid,
				      &user_pol);

	if (!NT_STATUS_IS_OK(result))
		return result;

	/* Query user rids */
	result = rpccli_samr_GetGroupsForUser(cli, mem_ctx,
					      &user_pol,
					      &rid_array);
	*num_groups = rid_array->count;

	rpccli_samr_Close(cli, mem_ctx, &user_pol);

	if (!NT_STATUS_IS_OK(result) || (*num_groups) == 0)
		return result;

	(*user_grpsids) = TALLOC_ARRAY(mem_ctx, DOM_SID, *num_groups);
	if (!(*user_grpsids))
		return NT_STATUS_NO_MEMORY;

	for (i=0;i<(*num_groups);i++) {
		sid_copy(&((*user_grpsids)[i]), &domain->sid);
		sid_append_rid(&((*user_grpsids)[i]),
				rid_array->rids[i].rid);
	}

	return NT_STATUS_OK;
}

#define MAX_SAM_ENTRIES_W2K 0x400 /* 1024 */

static NTSTATUS msrpc_lookup_useraliases(struct winbindd_domain *domain,
					 TALLOC_CTX *mem_ctx,
					 uint32 num_sids, const DOM_SID *sids,
					 uint32 *num_aliases,
					 uint32 **alias_rids)
{
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;
	struct policy_handle dom_pol;
	uint32 num_query_sids = 0;
	int i;
	struct rpc_pipe_client *cli;
	struct samr_Ids alias_rids_query;
	int rangesize = MAX_SAM_ENTRIES_W2K;
	uint32 total_sids = 0;
	int num_queries = 1;

	*num_aliases = 0;
	*alias_rids = NULL;

	DEBUG(3,("rpc: lookup_useraliases\n"));

	if ( !winbindd_can_contact_domain( domain ) ) {
		DEBUG(10,("msrpc_lookup_useraliases: No incoming trust for domain %s\n",
			  domain->name));
		return NT_STATUS_OK;
	}

	result = cm_connect_sam(domain, mem_ctx, &cli, &dom_pol);
	if (!NT_STATUS_IS_OK(result))
		return result;

	do {
		/* prepare query */
		struct lsa_SidArray sid_array;

		ZERO_STRUCT(sid_array);

		num_query_sids = MIN(num_sids - total_sids, rangesize);

		DEBUG(10,("rpc: lookup_useraliases: entering query %d for %d sids\n", 
			num_queries, num_query_sids));	

		if (num_query_sids) {
			sid_array.sids = TALLOC_ZERO_ARRAY(mem_ctx, struct lsa_SidPtr, num_query_sids);
			if (sid_array.sids == NULL) {
				return NT_STATUS_NO_MEMORY;
			}
		} else {
			sid_array.sids = NULL;
		}

		for (i=0; i<num_query_sids; i++) {
			sid_array.sids[i].sid = sid_dup_talloc(mem_ctx, &sids[total_sids++]);
			if (!sid_array.sids[i].sid) {
				TALLOC_FREE(sid_array.sids);
				return NT_STATUS_NO_MEMORY;
			}
		}
		sid_array.num_sids = num_query_sids;

		/* do request */
		result = rpccli_samr_GetAliasMembership(cli, mem_ctx,
							&dom_pol,
							&sid_array,
							&alias_rids_query);

		if (!NT_STATUS_IS_OK(result)) {
			*num_aliases = 0;
			*alias_rids = NULL;
			TALLOC_FREE(sid_array.sids);
			goto done;
		}

		/* process output */

		for (i=0; i<alias_rids_query.count; i++) {
			size_t na = *num_aliases;
			if (!add_rid_to_array_unique(mem_ctx, alias_rids_query.ids[i],
						alias_rids, &na)) {
				return NT_STATUS_NO_MEMORY;
			}
			*num_aliases = na;
		}

		TALLOC_FREE(sid_array.sids);

		num_queries++;

	} while (total_sids < num_sids);

 done:
	DEBUG(10,("rpc: lookup_useraliases: got %d aliases in %d queries "
		"(rangesize: %d)\n", *num_aliases, num_queries, rangesize));

	return result;
}


/* Lookup group membership given a rid.   */
static NTSTATUS lookup_groupmem(struct winbindd_domain *domain,
				TALLOC_CTX *mem_ctx,
				const DOM_SID *group_sid,
				enum lsa_SidType type,
				uint32 *num_names,
				DOM_SID **sid_mem, char ***names, 
				uint32 **name_types)
{
        NTSTATUS result = NT_STATUS_UNSUCCESSFUL;
        uint32 i, total_names = 0;
        struct policy_handle dom_pol, group_pol;
        uint32 des_access = SEC_FLAG_MAXIMUM_ALLOWED;
	uint32 *rid_mem = NULL;
	uint32 group_rid;
	unsigned int j, r;
	struct rpc_pipe_client *cli;
	unsigned int orig_timeout;
	struct samr_RidTypeArray *rids = NULL;

	DEBUG(10,("rpc: lookup_groupmem %s sid=%s\n", domain->name,
		  sid_string_dbg(group_sid)));

	if ( !winbindd_can_contact_domain( domain ) ) {
		DEBUG(10,("lookup_groupmem: No incoming trust for domain %s\n",
			  domain->name));
		return NT_STATUS_OK;
	}

	if (!sid_peek_check_rid(&domain->sid, group_sid, &group_rid))
		return NT_STATUS_UNSUCCESSFUL;

	*num_names = 0;

	result = cm_connect_sam(domain, mem_ctx, &cli, &dom_pol);
	if (!NT_STATUS_IS_OK(result))
		return result;

        result = rpccli_samr_OpenGroup(cli, mem_ctx,
				       &dom_pol,
				       des_access,
				       group_rid,
				       &group_pol);

        if (!NT_STATUS_IS_OK(result))
		return result;

        /* Step #1: Get a list of user rids that are the members of the
           group. */

	/* This call can take a long time - allow the server to time out.
	   35 seconds should do it. */

	orig_timeout = rpccli_set_timeout(cli, 35000);

        result = rpccli_samr_QueryGroupMember(cli, mem_ctx,
					      &group_pol,
					      &rids);

	/* And restore our original timeout. */
	rpccli_set_timeout(cli, orig_timeout);

	rpccli_samr_Close(cli, mem_ctx, &group_pol);

        if (!NT_STATUS_IS_OK(result))
		return result;

	if (!rids || !rids->count) {
		names = NULL;
		name_types = NULL;
		sid_mem = NULL;
		return NT_STATUS_OK;
	}

	*num_names = rids->count;
	rid_mem = rids->rids;

        /* Step #2: Convert list of rids into list of usernames.  Do this
           in bunches of ~1000 to avoid crashing NT4.  It looks like there
           is a buffer overflow or something like that lurking around
           somewhere. */

#define MAX_LOOKUP_RIDS 900

        *names = TALLOC_ZERO_ARRAY(mem_ctx, char *, *num_names);
        *name_types = TALLOC_ZERO_ARRAY(mem_ctx, uint32, *num_names);
        *sid_mem = TALLOC_ZERO_ARRAY(mem_ctx, DOM_SID, *num_names);

	for (j=0;j<(*num_names);j++)
		sid_compose(&(*sid_mem)[j], &domain->sid, rid_mem[j]);

	if (*num_names>0 && (!*names || !*name_types))
		return NT_STATUS_NO_MEMORY;

	for (i = 0; i < *num_names; i += MAX_LOOKUP_RIDS) {
		int num_lookup_rids = MIN(*num_names - i, MAX_LOOKUP_RIDS);
		struct lsa_Strings tmp_names;
		struct samr_Ids tmp_types;

		/* Lookup a chunk of rids */

		result = rpccli_samr_LookupRids(cli, mem_ctx,
						&dom_pol,
						num_lookup_rids,
						&rid_mem[i],
						&tmp_names,
						&tmp_types);

		/* see if we have a real error (and yes the
		   STATUS_SOME_UNMAPPED is the one returned from 2k) */

                if (!NT_STATUS_IS_OK(result) &&
		    !NT_STATUS_EQUAL(result, STATUS_SOME_UNMAPPED))
			return result;

		/* Copy result into array.  The talloc system will take
		   care of freeing the temporary arrays later on. */

		if (tmp_names.count != tmp_types.count) {
			return NT_STATUS_UNSUCCESSFUL;
		}

		for (r=0; r<tmp_names.count; r++) {
			if (tmp_types.ids[r] == SID_NAME_UNKNOWN) {
				continue;
			}
			(*names)[total_names] = fill_domain_username_talloc(
				mem_ctx, domain->name,
				tmp_names.names[r].string, true);
			(*name_types)[total_names] = tmp_types.ids[r];
			total_names += 1;
		}
        }

        *num_names = total_names;

	return NT_STATUS_OK;
}

#ifdef HAVE_LDAP

#include <ldap.h>

static int get_ldap_seq(const char *server, int port, uint32 *seq)
{
	int ret = -1;
	struct timeval to;
	const char *attrs[] = {"highestCommittedUSN", NULL};
	LDAPMessage *res = NULL;
	char **values = NULL;
	LDAP *ldp = NULL;

	*seq = DOM_SEQUENCE_NONE;

	/*
	 * Parameterised (5) second timeout on open. This is needed as the
	 * search timeout doesn't seem to apply to doing an open as well. JRA.
	 */

	ldp = ldap_open_with_timeout(server, port, lp_ldap_timeout());
	if (ldp == NULL)
		return -1;

	/* Timeout if no response within 20 seconds. */
	to.tv_sec = 10;
	to.tv_usec = 0;

	if (ldap_search_st(ldp, "", LDAP_SCOPE_BASE, "(objectclass=*)",
			   CONST_DISCARD(char **, attrs), 0, &to, &res))
		goto done;

	if (ldap_count_entries(ldp, res) != 1)
		goto done;

	values = ldap_get_values(ldp, res, "highestCommittedUSN");
	if (!values || !values[0])
		goto done;

	*seq = atoi(values[0]);
	ret = 0;

  done:

	if (values)
		ldap_value_free(values);
	if (res)
		ldap_msgfree(res);
	if (ldp)
		ldap_unbind(ldp);
	return ret;
}

/**********************************************************************
 Get the sequence number for a Windows AD native mode domain using
 LDAP queries.
**********************************************************************/

static int get_ldap_sequence_number(struct winbindd_domain *domain, uint32 *seq)
{
	int ret = -1;
	char addr[INET6_ADDRSTRLEN];

	print_sockaddr(addr, sizeof(addr), &domain->dcaddr);
	if ((ret = get_ldap_seq(addr, LDAP_PORT, seq)) == 0) {
		DEBUG(3, ("get_ldap_sequence_number: Retrieved sequence "
			  "number for Domain (%s) from DC (%s)\n",
			domain->name, addr));
	}
	return ret;
}

#endif /* HAVE_LDAP */

/* find the sequence number for a domain */
static NTSTATUS sequence_number(struct winbindd_domain *domain, uint32 *seq)
{
	TALLOC_CTX *mem_ctx;
	union samr_DomainInfo *info = NULL;
	NTSTATUS result;
	struct policy_handle dom_pol;
	bool got_seq_num = False;
	struct rpc_pipe_client *cli;

	DEBUG(10,("rpc: fetch sequence_number for %s\n", domain->name));

	if ( !winbindd_can_contact_domain( domain ) ) {
		DEBUG(10,("sequence_number: No incoming trust for domain %s\n",
			  domain->name));
		*seq = time(NULL);
		return NT_STATUS_OK;
	}

	*seq = DOM_SEQUENCE_NONE;

	if (!(mem_ctx = talloc_init("sequence_number[rpc]")))
		return NT_STATUS_NO_MEMORY;

#ifdef HAVE_LDAP
	if ( domain->active_directory ) 
	{
		int res;

		DEBUG(8,("using get_ldap_seq() to retrieve the "
			 "sequence number\n"));

		res =  get_ldap_sequence_number( domain, seq );
		if (res == 0)
		{			
			result = NT_STATUS_OK;
			DEBUG(10,("domain_sequence_number: LDAP for "
				  "domain %s is %u\n",
				  domain->name, *seq));
			goto done;
		}

		DEBUG(10,("domain_sequence_number: failed to get LDAP "
			  "sequence number for domain %s\n",
			  domain->name ));
	}
#endif /* HAVE_LDAP */

	result = cm_connect_sam(domain, mem_ctx, &cli, &dom_pol);
	if (!NT_STATUS_IS_OK(result)) {
		goto done;
	}

	/* Query domain info */

	result = rpccli_samr_QueryDomainInfo(cli, mem_ctx,
					     &dom_pol,
					     8,
					     &info);

	if (NT_STATUS_IS_OK(result)) {
		*seq = info->info8.sequence_num;
		got_seq_num = True;
		goto seq_num;
	}

	/* retry with info-level 2 in case the dc does not support info-level 8
	 * (like all older samba2 and samba3 dc's) - Guenther */

	result = rpccli_samr_QueryDomainInfo(cli, mem_ctx,
					     &dom_pol,
					     2,
					     &info);

	if (NT_STATUS_IS_OK(result)) {
		*seq = info->general.sequence_num;
		got_seq_num = True;
	}

 seq_num:
	if (got_seq_num) {
		DEBUG(10,("domain_sequence_number: for domain %s is %u\n",
			  domain->name, (unsigned)*seq));
	} else {
		DEBUG(10,("domain_sequence_number: failed to get sequence "
			  "number (%u) for domain %s\n",
			  (unsigned)*seq, domain->name ));
	}

  done:

	talloc_destroy(mem_ctx);

	return result;
}

/* get a list of trusted domains */
static NTSTATUS trusted_domains(struct winbindd_domain *domain,
				TALLOC_CTX *mem_ctx,
				struct netr_DomainTrustList *trusts)
{
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;
	uint32 enum_ctx = 0;
	struct rpc_pipe_client *cli;
	struct policy_handle lsa_policy;

	DEBUG(3,("rpc: trusted_domains\n"));

	ZERO_STRUCTP(trusts);

	result = cm_connect_lsa(domain, mem_ctx, &cli, &lsa_policy);
	if (!NT_STATUS_IS_OK(result))
		return result;

	result = STATUS_MORE_ENTRIES;

	while (NT_STATUS_EQUAL(result, STATUS_MORE_ENTRIES)) {
		uint32 start_idx;
		int i;
		struct lsa_DomainList dom_list;

		result = rpccli_lsa_EnumTrustDom(cli, mem_ctx,
						 &lsa_policy,
						 &enum_ctx,
						 &dom_list,
						 (uint32_t)-1);

		if (!NT_STATUS_IS_OK(result) &&
		    !NT_STATUS_EQUAL(result, STATUS_MORE_ENTRIES))
			break;

		start_idx = trusts->count;
		trusts->count += dom_list.count;

		trusts->array = talloc_realloc(
			mem_ctx, trusts->array, struct netr_DomainTrust,
			trusts->count);
		if (trusts->array == NULL) {
			return NT_STATUS_NO_MEMORY;
		}

		for (i=0; i<dom_list.count; i++) {
			struct netr_DomainTrust *trust = &trusts->array[i];
			struct dom_sid *sid;

			ZERO_STRUCTP(trust);

			trust->netbios_name = talloc_move(
				trusts->array,
				&dom_list.domains[i].name.string);
			trust->dns_name = NULL;

			sid = talloc(trusts->array, struct dom_sid);
			if (sid == NULL) {
				return NT_STATUS_NO_MEMORY;
			}
			sid_copy(sid, dom_list.domains[i].sid);
			trust->sid = sid;
		}
	}
	return result;
}

/* find the lockout policy for a domain */
static NTSTATUS msrpc_lockout_policy(struct winbindd_domain *domain,
				     TALLOC_CTX *mem_ctx,
				     struct samr_DomInfo12 *lockout_policy)
{
	NTSTATUS result;
	struct rpc_pipe_client *cli;
	struct policy_handle dom_pol;
	union samr_DomainInfo *info = NULL;

	DEBUG(10,("rpc: fetch lockout policy for %s\n", domain->name));

	if ( !winbindd_can_contact_domain( domain ) ) {
		DEBUG(10,("msrpc_lockout_policy: No incoming trust for domain %s\n",
			  domain->name));
		return NT_STATUS_NOT_SUPPORTED;
	}

	result = cm_connect_sam(domain, mem_ctx, &cli, &dom_pol);
	if (!NT_STATUS_IS_OK(result)) {
		goto done;
	}

	result = rpccli_samr_QueryDomainInfo(cli, mem_ctx,
					     &dom_pol,
					     12,
					     &info);
	if (!NT_STATUS_IS_OK(result)) {
		goto done;
	}

	*lockout_policy = info->info12;

	DEBUG(10,("msrpc_lockout_policy: lockout_threshold %d\n",
		info->info12.lockout_threshold));

  done:

	return result;
}

/* find the password policy for a domain */
static NTSTATUS msrpc_password_policy(struct winbindd_domain *domain,
				      TALLOC_CTX *mem_ctx,
				      struct samr_DomInfo1 *password_policy)
{
	NTSTATUS result;
	struct rpc_pipe_client *cli;
	struct policy_handle dom_pol;
	union samr_DomainInfo *info = NULL;

	DEBUG(10,("rpc: fetch password policy for %s\n", domain->name));

	if ( !winbindd_can_contact_domain( domain ) ) {
		DEBUG(10,("msrpc_password_policy: No incoming trust for domain %s\n",
			  domain->name));
		return NT_STATUS_NOT_SUPPORTED;
	}

	result = cm_connect_sam(domain, mem_ctx, &cli, &dom_pol);
	if (!NT_STATUS_IS_OK(result)) {
		goto done;
	}

	result = rpccli_samr_QueryDomainInfo(cli, mem_ctx,
					     &dom_pol,
					     1,
					     &info);
	if (!NT_STATUS_IS_OK(result)) {
		goto done;
	}

	*password_policy = info->info1;

	DEBUG(10,("msrpc_password_policy: min_length_password %d\n",
		info->info1.min_password_length));

  done:

	return result;
}

typedef NTSTATUS (*lookup_sids_fn_t)(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx,
				     struct policy_handle *pol,
				     int num_sids,
				     const DOM_SID *sids,
				     char ***pdomains,
				     char ***pnames,
				     enum lsa_SidType **ptypes);

NTSTATUS winbindd_lookup_sids(TALLOC_CTX *mem_ctx,
			      struct winbindd_domain *domain,
			      uint32_t num_sids,
			      const struct dom_sid *sids,
			      char ***domains,
			      char ***names,
			      enum lsa_SidType **types)
{
	NTSTATUS status;
	struct rpc_pipe_client *cli = NULL;
	struct policy_handle lsa_policy;
	unsigned int orig_timeout;
	lookup_sids_fn_t lookup_sids_fn = rpccli_lsa_lookup_sids;

	if (domain->can_do_ncacn_ip_tcp) {
		status = cm_connect_lsa_tcp(domain, mem_ctx, &cli);
		if (NT_STATUS_IS_OK(status)) {
			lookup_sids_fn = rpccli_lsa_lookup_sids3;
			goto lookup;
		}
		domain->can_do_ncacn_ip_tcp = false;
	}
	status = cm_connect_lsa(domain, mem_ctx, &cli, &lsa_policy);

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

 lookup:
	/*
	 * This call can take a long time
	 * allow the server to time out.
	 * 35 seconds should do it.
	 */
	orig_timeout = rpccli_set_timeout(cli, 35000);

	status = lookup_sids_fn(cli,
				mem_ctx,
				&lsa_policy,
				num_sids,
				sids,
				domains,
				names,
				types);

	/* And restore our original timeout. */
	rpccli_set_timeout(cli, orig_timeout);

	if (NT_STATUS_V(status) == DCERPC_FAULT_ACCESS_DENIED ||
	    NT_STATUS_V(status) == DCERPC_FAULT_SEC_PKG_ERROR) {
		/*
		 * This can happen if the schannel key is not
		 * valid anymore, we need to invalidate the
		 * all connections to the dc and reestablish
		 * a netlogon connection first.
		 */
		invalidate_cm_connection(&domain->conn);
		status = NT_STATUS_ACCESS_DENIED;
	}

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	return status;
}

typedef NTSTATUS (*lookup_names_fn_t)(struct rpc_pipe_client *cli,
				      TALLOC_CTX *mem_ctx,
				      struct policy_handle *pol,
				      int num_names,
				      const char **names,
				      const char ***dom_names,
				      int level,
				      struct dom_sid **sids,
				      enum lsa_SidType **types);

NTSTATUS winbindd_lookup_names(TALLOC_CTX *mem_ctx,
			       struct winbindd_domain *domain,
			       uint32_t num_names,
			       const char **names,
			       const char ***domains,
			       struct dom_sid **sids,
			       enum lsa_SidType **types)
{
	NTSTATUS status;
	struct rpc_pipe_client *cli = NULL;
	struct policy_handle lsa_policy;
	unsigned int orig_timeout = 0;
	lookup_names_fn_t lookup_names_fn = rpccli_lsa_lookup_names;

	if (domain->can_do_ncacn_ip_tcp) {
		status = cm_connect_lsa_tcp(domain, mem_ctx, &cli);
		if (NT_STATUS_IS_OK(status)) {
			lookup_names_fn = rpccli_lsa_lookup_names4;
			goto lookup;
		}
		domain->can_do_ncacn_ip_tcp = false;
	}
	status = cm_connect_lsa(domain, mem_ctx, &cli, &lsa_policy);

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

 lookup:

	/*
	 * This call can take a long time
	 * allow the server to time out.
	 * 35 seconds should do it.
	 */
	orig_timeout = rpccli_set_timeout(cli, 35000);

	status = lookup_names_fn(cli,
				 mem_ctx,
				 &lsa_policy,
				 num_names,
				 (const char **) names,
				 domains,
				 1,
				 sids,
				 types);

	/* And restore our original timeout. */
	rpccli_set_timeout(cli, orig_timeout);

	if (NT_STATUS_V(status) == DCERPC_FAULT_ACCESS_DENIED ||
	    NT_STATUS_V(status) == DCERPC_FAULT_SEC_PKG_ERROR) {
		/*
		 * This can happen if the schannel key is not
		 * valid anymore, we need to invalidate the
		 * all connections to the dc and reestablish
		 * a netlogon connection first.
		 */
		invalidate_cm_connection(&domain->conn);
		status = NT_STATUS_ACCESS_DENIED;
	}

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	return status;
}

/* the rpc backend methods are exposed via this structure */
struct winbindd_methods msrpc_methods = {
	False,
	query_user_list,
	enum_dom_groups,
	enum_local_groups,
	msrpc_name_to_sid,
	msrpc_sid_to_name,
	msrpc_rids_to_names,
	query_user,
	lookup_usergroups,
	msrpc_lookup_useraliases,
	lookup_groupmem,
	sequence_number,
	msrpc_lockout_policy,
	msrpc_password_policy,
	trusted_domains,
};
