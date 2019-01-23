/*
 * Unix SMB/CIFS implementation.
 *
 * Winbind rpc backend functions
 *
 * Copyright (c) 2000-2003 Tim Potter
 * Copyright (c) 2001      Andrew Tridgell
 * Copyright (c) 2005      Volker Lendecke
 * Copyright (c) 2008      Guenther Deschner (pidl conversion)
 * Copyright (c) 2010      Andreas Schneider <asn@samba.org>
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
#include "winbindd_rpc.h"
#include "rpc_client/rpc_client.h"
#include "librpc/gen_ndr/ndr_samr_c.h"
#include "librpc/gen_ndr/ndr_lsa_c.h"
#include "rpc_client/cli_samr.h"
#include "rpc_client/cli_lsarpc.h"
#include "../libcli/security/security.h"

/* Query display info for a domain */
NTSTATUS rpc_query_user_list(TALLOC_CTX *mem_ctx,
			     struct rpc_pipe_client *samr_pipe,
			     struct policy_handle *samr_policy,
			     const struct dom_sid *domain_sid,
			     uint32_t *pnum_info,
			     struct wbint_userinfo **pinfo)
{
	struct wbint_userinfo *info = NULL;
	uint32_t num_info = 0;
	uint32_t loop_count = 0;
	uint32_t start_idx = 0;
	uint32_t i = 0;
	NTSTATUS status, result;
	struct dcerpc_binding_handle *b = samr_pipe->binding_handle;

	*pnum_info = 0;

	do {
		uint32_t j;
		uint32_t num_dom_users;
		uint32_t max_entries, max_size;
		uint32_t total_size, returned_size;
		union samr_DispInfo disp_info;

		dcerpc_get_query_dispinfo_params(loop_count,
						 &max_entries,
						 &max_size);

		status = dcerpc_samr_QueryDisplayInfo(b,
						      mem_ctx,
						      samr_policy,
						      1, /* level */
						      start_idx,
						      max_entries,
						      max_size,
						      &total_size,
						      &returned_size,
						      &disp_info,
						      &result);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
		if (!NT_STATUS_IS_OK(result)) {
			if (!NT_STATUS_EQUAL(result, STATUS_MORE_ENTRIES)) {
				return result;
			}
		}

		/* increment required start query values */
		start_idx += disp_info.info1.count;
		loop_count++;
		num_dom_users = disp_info.info1.count;

		num_info += num_dom_users;

		info = TALLOC_REALLOC_ARRAY(mem_ctx,
					    info,
					    struct wbint_userinfo,
					    num_info);
		if (info == NULL) {
			return NT_STATUS_NO_MEMORY;
		}

		for (j = 0; j < num_dom_users; i++, j++) {
			uint32_t rid = disp_info.info1.entries[j].rid;
			struct samr_DispEntryGeneral *src;
			struct wbint_userinfo *dst;

			src = &(disp_info.info1.entries[j]);
			dst = &(info[i]);

			dst->acct_name = talloc_strdup(info,
						       src->account_name.string);
			if (dst->acct_name == NULL) {
				return NT_STATUS_NO_MEMORY;
			}

			dst->full_name = talloc_strdup(info, src->full_name.string);
			if ((src->full_name.string != NULL) &&
			    (dst->full_name == NULL))
			{
				return NT_STATUS_NO_MEMORY;
			}

			dst->homedir = NULL;
			dst->shell = NULL;

			sid_compose(&dst->user_sid, domain_sid, rid);

			/* For the moment we set the primary group for
			   every user to be the Domain Users group.
			   There are serious problems with determining
			   the actual primary group for large domains.
			   This should really be made into a 'winbind
			   force group' smb.conf parameter or
			   something like that. */
			sid_compose(&dst->group_sid, domain_sid,
				    DOMAIN_RID_USERS);
		}
	} while (NT_STATUS_EQUAL(result, STATUS_MORE_ENTRIES));

	*pnum_info = num_info;
	*pinfo = info;

	return NT_STATUS_OK;
}

/* List all domain groups */
NTSTATUS rpc_enum_dom_groups(TALLOC_CTX *mem_ctx,
			     struct rpc_pipe_client *samr_pipe,
			     struct policy_handle *samr_policy,
			     uint32_t *pnum_info,
			     struct wb_acct_info **pinfo)
{
	struct wb_acct_info *info = NULL;
	uint32_t start = 0;
	uint32_t num_info = 0;
	NTSTATUS status, result;
	struct dcerpc_binding_handle *b = samr_pipe->binding_handle;

	*pnum_info = 0;

	do {
		struct samr_SamArray *sam_array = NULL;
		uint32_t count = 0;
		uint32_t g;

		/* start is updated by this call. */
		status = dcerpc_samr_EnumDomainGroups(b,
						      mem_ctx,
						      samr_policy,
						      &start,
						      &sam_array,
						      0xFFFF, /* buffer size? */
						      &count,
						      &result);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
		if (!NT_STATUS_IS_OK(result)) {
			if (!NT_STATUS_EQUAL(result, STATUS_MORE_ENTRIES)) {
				DEBUG(2,("query_user_list: failed to enum domain groups: %s\n",
					 nt_errstr(result)));
				return result;
			}
		}

		info = TALLOC_REALLOC_ARRAY(mem_ctx,
					    info,
					    struct wb_acct_info,
					    num_info + count);
		if (info == NULL) {
			return NT_STATUS_NO_MEMORY;
		}

		for (g = 0; g < count; g++) {
			fstrcpy(info[num_info + g].acct_name,
				sam_array->entries[g].name.string);

			info[num_info + g].rid = sam_array->entries[g].idx;
		}

		num_info += count;
	} while (NT_STATUS_EQUAL(result, STATUS_MORE_ENTRIES));

	*pnum_info = num_info;
	*pinfo = info;

	return NT_STATUS_OK;
}

NTSTATUS rpc_enum_local_groups(TALLOC_CTX *mem_ctx,
			       struct rpc_pipe_client *samr_pipe,
			       struct policy_handle *samr_policy,
			       uint32_t *pnum_info,
			       struct wb_acct_info **pinfo)
{
	struct wb_acct_info *info = NULL;
	uint32_t num_info = 0;
	NTSTATUS status, result;
	struct dcerpc_binding_handle *b = samr_pipe->binding_handle;

	*pnum_info = 0;

	do {
		struct samr_SamArray *sam_array = NULL;
		uint32_t count = 0;
		uint32_t start = num_info;
		uint32_t g;

		status = dcerpc_samr_EnumDomainAliases(b,
						       mem_ctx,
						       samr_policy,
						       &start,
						       &sam_array,
						       0xFFFF, /* buffer size? */
						       &count,
						       &result);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
		if (!NT_STATUS_IS_OK(result)) {
			if (!NT_STATUS_EQUAL(result, STATUS_MORE_ENTRIES)) {
				return result;
			}
		}

		info = TALLOC_REALLOC_ARRAY(mem_ctx,
					    info,
					    struct wb_acct_info,
					    num_info + count);
		if (info == NULL) {
			return  NT_STATUS_NO_MEMORY;
		}

		for (g = 0; g < count; g++) {
			fstrcpy(info[num_info + g].acct_name,
				sam_array->entries[g].name.string);
			info[num_info + g].rid = sam_array->entries[g].idx;
		}

		num_info += count;
	} while (NT_STATUS_EQUAL(result, STATUS_MORE_ENTRIES));

	*pnum_info = num_info;
	*pinfo = info;

	return NT_STATUS_OK;
}

/* convert a single name to a sid in a domain */
NTSTATUS rpc_name_to_sid(TALLOC_CTX *mem_ctx,
			 struct rpc_pipe_client *lsa_pipe,
			 struct policy_handle *lsa_policy,
			 const char *domain_name,
			 const char *name,
			 uint32_t flags,
			 struct dom_sid *sid,
			 enum lsa_SidType *type)
{
	enum lsa_SidType *types = NULL;
	struct dom_sid *sids = NULL;
	char *full_name = NULL;
	char *mapped_name = NULL;
	NTSTATUS status;

	if (name == NULL || name[0] == '\0') {
		full_name = talloc_asprintf(mem_ctx, "%s", domain_name);
	} else if (domain_name == NULL || domain_name[0] == '\0') {
		full_name = talloc_asprintf(mem_ctx, "%s", name);
	} else {
		full_name = talloc_asprintf(mem_ctx, "%s\\%s", domain_name, name);
	}

	if (full_name == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	status = normalize_name_unmap(mem_ctx, full_name, &mapped_name);
	/* Reset the full_name pointer if we mapped anything */
	if (NT_STATUS_IS_OK(status) ||
	    NT_STATUS_EQUAL(status, NT_STATUS_FILE_RENAMED)) {
		full_name = mapped_name;
	}

	DEBUG(3,("name_to_sid: %s for domain %s\n",
		 full_name ? full_name : "", domain_name ));

	/*
	 * We don't run into deadlocks here, cause winbind_off() is
	 * called in the main function.
	 */
	status = rpccli_lsa_lookup_names(lsa_pipe,
					 mem_ctx,
					 lsa_policy,
					 1, /* num_names */
					 (const char **) &full_name,
					 NULL, /* domains */
					 1, /* level */
					 &sids,
					 &types);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(2,("name_to_sid: failed to lookup name: %s\n",
			nt_errstr(status)));
		return status;
	}

	sid_copy(sid, &sids[0]);
	*type = types[0];

	return NT_STATUS_OK;
}

/* Convert a domain SID to a user or group name */
NTSTATUS rpc_sid_to_name(TALLOC_CTX *mem_ctx,
			 struct rpc_pipe_client *lsa_pipe,
			 struct policy_handle *lsa_policy,
			 struct winbindd_domain *domain,
			 const struct dom_sid *sid,
			 char **pdomain_name,
			 char **pname,
			 enum lsa_SidType *ptype)
{
	char *mapped_name = NULL;
	char **domains = NULL;
	char **names = NULL;
	enum lsa_SidType *types = NULL;
	NTSTATUS map_status;
	NTSTATUS status;

	status = rpccli_lsa_lookup_sids(lsa_pipe,
					mem_ctx,
					lsa_policy,
					1, /* num_sids */
					sid,
					&domains,
					&names,
					&types);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(2,("sid_to_name: failed to lookup sids: %s\n",
			nt_errstr(status)));
		return status;
	}

	*ptype = (enum lsa_SidType) types[0];

	map_status = normalize_name_map(mem_ctx,
					domain,
					*pname,
					&mapped_name);
	if (NT_STATUS_IS_OK(map_status) ||
	    NT_STATUS_EQUAL(map_status, NT_STATUS_FILE_RENAMED)) {
		*pname = talloc_strdup(mem_ctx, mapped_name);
		DEBUG(5,("returning mapped name -- %s\n", *pname));
	} else {
		*pname = talloc_strdup(mem_ctx, names[0]);
	}
	if (*pname == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	*pdomain_name = talloc_strdup(mem_ctx, domains[0]);
	if (*pdomain_name == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	return NT_STATUS_OK;
}

/* Convert a bunch of rids to user or group names */
NTSTATUS rpc_rids_to_names(TALLOC_CTX *mem_ctx,
			   struct rpc_pipe_client *lsa_pipe,
			   struct policy_handle *lsa_policy,
			   struct winbindd_domain *domain,
			   const struct dom_sid *sid,
			   uint32_t *rids,
			   size_t num_rids,
			   char **pdomain_name,
			   char ***pnames,
			   enum lsa_SidType **ptypes)
{
	enum lsa_SidType *types = NULL;
	char *domain_name = NULL;
	char **domains = NULL;
	char **names = NULL;
	struct dom_sid *sids;
	size_t i;
	NTSTATUS status;

	if (num_rids > 0) {
		sids = TALLOC_ARRAY(mem_ctx, struct dom_sid, num_rids);
		if (sids == NULL) {
			return NT_STATUS_NO_MEMORY;
		}
	} else {
		sids = NULL;
	}

	for (i = 0; i < num_rids; i++) {
		if (!sid_compose(&sids[i], sid, rids[i])) {
			return NT_STATUS_INTERNAL_ERROR;
		}
	}

	status = rpccli_lsa_lookup_sids(lsa_pipe,
					mem_ctx,
					lsa_policy,
					num_rids,
					sids,
					&domains,
					&names,
					&types);
	if (!NT_STATUS_IS_OK(status) &&
	    !NT_STATUS_EQUAL(status, STATUS_SOME_UNMAPPED)) {
		DEBUG(2,("rids_to_names: failed to lookup sids: %s\n",
			nt_errstr(status)));
		return status;
	}

	for (i = 0; i < num_rids; i++) {
		char *mapped_name = NULL;
		NTSTATUS map_status;

		if (types[i] != SID_NAME_UNKNOWN) {
			map_status = normalize_name_map(mem_ctx,
							domain,
							names[i],
							&mapped_name);
			if (NT_STATUS_IS_OK(map_status) ||
			    NT_STATUS_EQUAL(map_status, NT_STATUS_FILE_RENAMED)) {
				TALLOC_FREE(names[i]);
				names[i] = talloc_strdup(names, mapped_name);
				if (names[i] == NULL) {
					return NT_STATUS_NO_MEMORY;
				}
			}

			domain_name = domains[i];
		}
	}

	*pdomain_name = domain_name;
	*ptypes = types;
	*pnames = names;

	return NT_STATUS_OK;
}

/* Lookup user information from a rid or username. */
NTSTATUS rpc_query_user(TALLOC_CTX *mem_ctx,
			struct rpc_pipe_client *samr_pipe,
			struct policy_handle *samr_policy,
			const struct dom_sid *domain_sid,
			const struct dom_sid *user_sid,
			struct wbint_userinfo *user_info)
{
	struct policy_handle user_policy;
	union samr_UserInfo *info = NULL;
	uint32_t user_rid;
	NTSTATUS status, result;
	struct dcerpc_binding_handle *b = samr_pipe->binding_handle;

	if (!sid_peek_check_rid(domain_sid, user_sid, &user_rid)) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	/* Get user handle */
	status = dcerpc_samr_OpenUser(b,
				      mem_ctx,
				      samr_policy,
				      SEC_FLAG_MAXIMUM_ALLOWED,
				      user_rid,
				      &user_policy,
				      &result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if (!NT_STATUS_IS_OK(result)) {
		return result;
	}

	/* Get user info */
	status = dcerpc_samr_QueryUserInfo(b,
					   mem_ctx,
					   &user_policy,
					   0x15,
					   &info,
					   &result);
	{
		NTSTATUS _result;
		dcerpc_samr_Close(b, mem_ctx, &user_policy, &_result);
	}
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if (!NT_STATUS_IS_OK(result)) {
		return result;
	}

	sid_compose(&user_info->user_sid, domain_sid, user_rid);
	sid_compose(&user_info->group_sid, domain_sid,
		    info->info21.primary_gid);

	user_info->acct_name = talloc_strdup(user_info,
					info->info21.account_name.string);
	if (user_info->acct_name == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	user_info->full_name = talloc_strdup(user_info,
					info->info21.full_name.string);
	if ((info->info21.full_name.string != NULL) &&
	    (user_info->acct_name == NULL))
	{
		return NT_STATUS_NO_MEMORY;
	}

	user_info->homedir = NULL;
	user_info->shell = NULL;
	user_info->primary_gid = (gid_t)-1;

	return NT_STATUS_OK;
}

/* Lookup groups a user is a member of. */
NTSTATUS rpc_lookup_usergroups(TALLOC_CTX *mem_ctx,
			       struct rpc_pipe_client *samr_pipe,
			       struct policy_handle *samr_policy,
			       const struct dom_sid *domain_sid,
			       const struct dom_sid *user_sid,
			       uint32_t *pnum_groups,
			       struct dom_sid **puser_grpsids)
{
	struct policy_handle user_policy;
	struct samr_RidWithAttributeArray *rid_array = NULL;
	struct dom_sid *user_grpsids = NULL;
	uint32_t num_groups = 0, i;
	uint32_t user_rid;
	NTSTATUS status, result;
	struct dcerpc_binding_handle *b = samr_pipe->binding_handle;

	if (!sid_peek_check_rid(domain_sid, user_sid, &user_rid)) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	/* Get user handle */
	status = dcerpc_samr_OpenUser(b,
				      mem_ctx,
				      samr_policy,
				      SEC_FLAG_MAXIMUM_ALLOWED,
				      user_rid,
				      &user_policy,
				      &result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if (!NT_STATUS_IS_OK(result)) {
		return result;
	}

	/* Query user rids */
	status = dcerpc_samr_GetGroupsForUser(b,
					      mem_ctx,
					      &user_policy,
					      &rid_array,
					      &result);
	num_groups = rid_array->count;

	{
		NTSTATUS _result;
		dcerpc_samr_Close(b, mem_ctx, &user_policy, &_result);
	}

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if (!NT_STATUS_IS_OK(result) || num_groups == 0) {
		return result;
	}

	user_grpsids = TALLOC_ARRAY(mem_ctx, struct dom_sid, num_groups);
	if (user_grpsids == NULL) {
		status = NT_STATUS_NO_MEMORY;
		return status;
	}

	for (i = 0; i < num_groups; i++) {
		sid_compose(&(user_grpsids[i]), domain_sid,
			    rid_array->rids[i].rid);
	}

	*pnum_groups = num_groups;

	*puser_grpsids = user_grpsids;

	return NT_STATUS_OK;
}

NTSTATUS rpc_lookup_useraliases(TALLOC_CTX *mem_ctx,
				struct rpc_pipe_client *samr_pipe,
				struct policy_handle *samr_policy,
				uint32_t num_sids,
				const struct dom_sid *sids,
				uint32_t *pnum_aliases,
				uint32_t **palias_rids)
{
#define MAX_SAM_ENTRIES_W2K 0x400 /* 1024 */
	uint32_t num_query_sids = 0;
	uint32_t num_queries = 1;
	uint32_t num_aliases = 0;
	uint32_t total_sids = 0;
	uint32_t *alias_rids = NULL;
	uint32_t rangesize = MAX_SAM_ENTRIES_W2K;
	uint32_t i;
	struct samr_Ids alias_rids_query;
	NTSTATUS status, result;
	struct dcerpc_binding_handle *b = samr_pipe->binding_handle;

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

		for (i = 0; i < num_query_sids; i++) {
			sid_array.sids[i].sid = dom_sid_dup(mem_ctx, &sids[total_sids++]);
			if (sid_array.sids[i].sid == NULL) {
				return NT_STATUS_NO_MEMORY;
			}
		}
		sid_array.num_sids = num_query_sids;

		/* do request */
		status = dcerpc_samr_GetAliasMembership(b,
							mem_ctx,
							samr_policy,
							&sid_array,
							&alias_rids_query,
							&result);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
		if (!NT_STATUS_IS_OK(result)) {
			return result;
		}

		/* process output */
		for (i = 0; i < alias_rids_query.count; i++) {
			size_t na = num_aliases;

			if (!add_rid_to_array_unique(mem_ctx,
						     alias_rids_query.ids[i],
						     &alias_rids,
						     &na)) {
					return NT_STATUS_NO_MEMORY;
				}
				num_aliases = na;
		}

		num_queries++;

	} while (total_sids < num_sids);

	DEBUG(10,("rpc: rpc_lookup_useraliases: got %d aliases in %d queries "
		  "(rangesize: %d)\n", num_aliases, num_queries, rangesize));

	*pnum_aliases = num_aliases;
	*palias_rids = alias_rids;

	return NT_STATUS_OK;
#undef MAX_SAM_ENTRIES_W2K
}

/* Lookup group membership given a rid.   */
NTSTATUS rpc_lookup_groupmem(TALLOC_CTX *mem_ctx,
			     struct rpc_pipe_client *samr_pipe,
			     struct policy_handle *samr_policy,
			     const char *domain_name,
			     const struct dom_sid *domain_sid,
			     const struct dom_sid *group_sid,
			     enum lsa_SidType type,
			     uint32_t *pnum_names,
			     struct dom_sid **psid_mem,
			     char ***pnames,
			     uint32_t **pname_types)
{
	struct policy_handle group_policy;
	uint32_t group_rid;
	uint32_t *rid_mem = NULL;

	uint32_t num_names = 0;
	uint32_t total_names = 0;
	struct dom_sid *sid_mem = NULL;
	char **names = NULL;
	uint32_t *name_types = NULL;

	struct lsa_Strings tmp_names;
	struct samr_Ids tmp_types;

	uint32_t j, r;
	NTSTATUS status, result;
	struct dcerpc_binding_handle *b = samr_pipe->binding_handle;

	if (!sid_peek_check_rid(domain_sid, group_sid, &group_rid)) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	switch(type) {
	case SID_NAME_DOM_GRP:
	{
		struct samr_RidAttrArray *rids = NULL;

		status = dcerpc_samr_OpenGroup(b,
					       mem_ctx,
					       samr_policy,
					       SEC_FLAG_MAXIMUM_ALLOWED,
					       group_rid,
					       &group_policy,
					       &result);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
		if (!NT_STATUS_IS_OK(result)) {
			return result;
		}

		/*
		 * Step #1: Get a list of user rids that are the members of the group.
		 */
		status = dcerpc_samr_QueryGroupMember(b,
						      mem_ctx,
						      &group_policy,
						      &rids,
						      &result);
		{
			NTSTATUS _result;
			dcerpc_samr_Close(b, mem_ctx, &group_policy, &_result);
		}

		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
		if (!NT_STATUS_IS_OK(result)) {
			return result;
		}


		if (rids == NULL || rids->count == 0) {
			pnum_names = 0;
			pnames = NULL;
			pname_types = NULL;
			psid_mem = NULL;

			return NT_STATUS_OK;
		}

		num_names = rids->count;
		rid_mem = rids->rids;

		break;
	}
	case SID_NAME_WKN_GRP:
	case SID_NAME_ALIAS:
	{
		struct lsa_SidArray sid_array;
		struct lsa_SidPtr sid_ptr;
		struct samr_Ids rids_query;

		sid_ptr.sid = dom_sid_dup(mem_ctx, group_sid);
		if (sid_ptr.sid == NULL) {
			return NT_STATUS_NO_MEMORY;
		}

		sid_array.num_sids = 1;
		sid_array.sids = &sid_ptr;

		status = dcerpc_samr_GetAliasMembership(b,
							mem_ctx,
							samr_policy,
							&sid_array,
							&rids_query,
							&result);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
		if (!NT_STATUS_IS_OK(result)) {
			return result;
		}

		if (rids_query.count == 0) {
			pnum_names = 0;
			pnames = NULL;
			pname_types = NULL;
			psid_mem = NULL;

			return NT_STATUS_OK;
		}

		num_names = rids_query.count;
		rid_mem = rids_query.ids;

		break;
	}
	default:
		return NT_STATUS_UNSUCCESSFUL;
	}

	/*
	 * Step #2: Convert list of rids into list of usernames.
	 */
	if (num_names > 0) {
		names = TALLOC_ZERO_ARRAY(mem_ctx, char *, num_names);
		name_types = TALLOC_ZERO_ARRAY(mem_ctx, uint32_t, num_names);
		sid_mem = TALLOC_ZERO_ARRAY(mem_ctx, struct dom_sid, num_names);
		if (names == NULL || name_types == NULL || sid_mem == NULL) {
			return NT_STATUS_NO_MEMORY;
		}
	}

	for (j = 0; j < num_names; j++) {
		sid_compose(&sid_mem[j], domain_sid, rid_mem[j]);
	}

	status = dcerpc_samr_LookupRids(b,
					mem_ctx,
					samr_policy,
					num_names,
					rid_mem,
					&tmp_names,
					&tmp_types,
					&result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (!NT_STATUS_IS_OK(result)) {
		if (!NT_STATUS_EQUAL(result, STATUS_SOME_UNMAPPED)) {
			return result;
		}
	}

	/* Copy result into array.  The talloc system will take
	   care of freeing the temporary arrays later on. */
	if (tmp_names.count != num_names) {
		return NT_STATUS_INVALID_NETWORK_RESPONSE;
	}
	if (tmp_types.count != num_names) {
		return NT_STATUS_INVALID_NETWORK_RESPONSE;
	}

	for (r = 0; r < tmp_names.count; r++) {
		if (tmp_types.ids[r] == SID_NAME_UNKNOWN) {
			continue;
		}
		if (total_names >= num_names) {
			break;
		}
		names[total_names] = fill_domain_username_talloc(names,
								 domain_name,
								 tmp_names.names[r].string,
								 true);
		if (names[total_names] == NULL) {
			return NT_STATUS_NO_MEMORY;
		}
		name_types[total_names] = tmp_types.ids[r];
		total_names++;
	}

	*pnum_names = total_names;
	*pnames = names;
	*pname_types = name_types;
	*psid_mem = sid_mem;

	return NT_STATUS_OK;
}

/* Find the sequence number for a domain */
NTSTATUS rpc_sequence_number(TALLOC_CTX *mem_ctx,
			     struct rpc_pipe_client *samr_pipe,
			     struct policy_handle *samr_policy,
			     const char *domain_name,
			     uint32_t *pseq)
{
	union samr_DomainInfo *info = NULL;
	bool got_seq_num = false;
	NTSTATUS status, result;
	struct dcerpc_binding_handle *b = samr_pipe->binding_handle;

	/* query domain info */
	status = dcerpc_samr_QueryDomainInfo(b,
					     mem_ctx,
					     samr_policy,
					     8,
					     &info,
					     &result);
	if (NT_STATUS_IS_OK(status) && NT_STATUS_IS_OK(result)) {
		*pseq = info->info8.sequence_num;
		got_seq_num = true;
		goto seq_num;
	}

	/* retry with info-level 2 in case the dc does not support info-level 8
	 * (like all older samba2 and samba3 dc's) - Guenther */
	status = dcerpc_samr_QueryDomainInfo(b,
					     mem_ctx,
					     samr_policy,
					     2,
					     &info,
					     &result);
	if (NT_STATUS_IS_OK(status) && NT_STATUS_IS_OK(result)) {
		*pseq = info->general.sequence_num;
		got_seq_num = true;
		goto seq_num;
	}

	if (!NT_STATUS_IS_OK(status)) {
		goto seq_num;
	}

	status = result;

seq_num:
	if (got_seq_num) {
		DEBUG(10,("domain_sequence_number: for domain %s is %u\n",
			  domain_name, (unsigned) *pseq));
	} else {
		DEBUG(10,("domain_sequence_number: failed to get sequence "
			  "number (%u) for domain %s\n",
			  (unsigned) *pseq, domain_name ));
		status = NT_STATUS_OK;
	}

	return status;
}

/* Get a list of trusted domains */
NTSTATUS rpc_trusted_domains(TALLOC_CTX *mem_ctx,
			     struct rpc_pipe_client *lsa_pipe,
			     struct policy_handle *lsa_policy,
			     uint32_t *pnum_trusts,
			     struct netr_DomainTrust **ptrusts)
{
	struct netr_DomainTrust *array = NULL;
	uint32_t enum_ctx = 0;
	uint32_t count = 0;
	NTSTATUS status, result;
	struct dcerpc_binding_handle *b = lsa_pipe->binding_handle;

	do {
		struct lsa_DomainList dom_list;
		uint32_t start_idx;
		uint32_t i;

		/*
		 * We don't run into deadlocks here, cause winbind_off() is
		 * called in the main function.
		 */
		status = dcerpc_lsa_EnumTrustDom(b,
						 mem_ctx,
						 lsa_policy,
						 &enum_ctx,
						 &dom_list,
						 (uint32_t) -1,
						 &result);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
		if (!NT_STATUS_IS_OK(result)) {
			if (!NT_STATUS_EQUAL(result, STATUS_MORE_ENTRIES)) {
				return result;
			}
		}

		start_idx = count;
		count += dom_list.count;

		array = talloc_realloc(mem_ctx,
				       array,
				       struct netr_DomainTrust,
				       count);
		if (array == NULL) {
			return NT_STATUS_NO_MEMORY;
		}

		for (i = 0; i < dom_list.count; i++) {
			struct netr_DomainTrust *trust = &array[i];
			struct dom_sid *sid;

			ZERO_STRUCTP(trust);

			trust->netbios_name = talloc_move(array,
							  &dom_list.domains[i].name.string);
			trust->dns_name = NULL;

			sid = talloc(array, struct dom_sid);
			if (sid == NULL) {
				return NT_STATUS_NO_MEMORY;
			}
			sid_copy(sid, dom_list.domains[i].sid);
			trust->sid = sid;
		}
	} while (NT_STATUS_EQUAL(result, STATUS_MORE_ENTRIES));

	*pnum_trusts = count;
	*ptrusts = array;

	return NT_STATUS_OK;
}

static NTSTATUS rpc_try_lookup_sids3(TALLOC_CTX *mem_ctx,
				     struct winbindd_domain *domain,
				     struct rpc_pipe_client *cli,
				     struct lsa_SidArray *sids,
				     struct lsa_RefDomainList **pdomains,
				     struct lsa_TransNameArray **pnames)
{
	struct lsa_TransNameArray2 lsa_names2;
	struct lsa_TransNameArray *names = *pnames;
	uint32_t i, count;
	NTSTATUS status, result;

	ZERO_STRUCT(lsa_names2);
	status = dcerpc_lsa_LookupSids3(cli->binding_handle,
					mem_ctx,
					sids,
					pdomains,
					&lsa_names2,
					LSA_LOOKUP_NAMES_ALL,
					&count,
					LSA_LOOKUP_OPTION_SEARCH_ISOLATED_NAMES,
					LSA_CLIENT_REVISION_2,
					&result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if (NT_STATUS_IS_ERR(result)) {
		return result;
	}
	if (sids->num_sids != lsa_names2.count) {
		return NT_STATUS_INVALID_NETWORK_RESPONSE;
	}

	names->count = lsa_names2.count;
	names->names = talloc_array(names, struct lsa_TranslatedName,
				    names->count);
	if (names->names == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	for (i=0; i<names->count; i++) {
		names->names[i].sid_type = lsa_names2.names[i].sid_type;
		names->names[i].name.string = talloc_move(
			names->names, &lsa_names2.names[i].name.string);
		names->names[i].sid_index = lsa_names2.names[i].sid_index;

		if (names->names[i].sid_index == UINT32_MAX) {
			continue;
		}
		if ((*pdomains) == NULL) {
			return NT_STATUS_INVALID_NETWORK_RESPONSE;
		}
		if (names->names[i].sid_index >= (*pdomains)->count) {
			return NT_STATUS_INVALID_NETWORK_RESPONSE;
		}
	}
	return result;
}

NTSTATUS rpc_lookup_sids(TALLOC_CTX *mem_ctx,
			 struct winbindd_domain *domain,
			 struct lsa_SidArray *sids,
			 struct lsa_RefDomainList **pdomains,
			 struct lsa_TransNameArray **pnames)
{
	struct lsa_TransNameArray *names = *pnames;
	struct rpc_pipe_client *cli = NULL;
	struct policy_handle lsa_policy;
	uint32_t count;
	uint32_t i;
	NTSTATUS status, result;

	status = cm_connect_lsat(domain, mem_ctx, &cli, &lsa_policy);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (cli->transport->transport == NCACN_IP_TCP) {
		return rpc_try_lookup_sids3(mem_ctx, domain, cli, sids,
					    pdomains, pnames);
	}

	status = dcerpc_lsa_LookupSids(cli->binding_handle, mem_ctx,
				       &lsa_policy, sids, pdomains,
				       names, LSA_LOOKUP_NAMES_ALL,
				       &count, &result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if (NT_STATUS_IS_ERR(result)) {
		return result;
	}

	if (sids->num_sids != names->count) {
		return NT_STATUS_INVALID_NETWORK_RESPONSE;
	}

	for (i=0; i < names->count; i++) {
		if (names->names[i].sid_index == UINT32_MAX) {
			continue;
		}
		if ((*pdomains) == NULL) {
			return NT_STATUS_INVALID_NETWORK_RESPONSE;
		}
		if (names->names[i].sid_index >= (*pdomains)->count) {
			return NT_STATUS_INVALID_NETWORK_RESPONSE;
		}
	}

	return result;
}
