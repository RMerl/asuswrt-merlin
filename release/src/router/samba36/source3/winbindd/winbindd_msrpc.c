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
#include "winbindd_rpc.h"

#include "../librpc/gen_ndr/ndr_samr_c.h"
#include "rpc_client/cli_pipe.h"
#include "rpc_client/cli_samr.h"
#include "rpc_client/cli_lsarpc.h"
#include "../libcli/security/security.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_WINBIND

static NTSTATUS winbindd_lookup_names(TALLOC_CTX *mem_ctx,
				      struct winbindd_domain *domain,
				      uint32_t num_names,
				      const char **names,
				      const char ***domains,
				      struct dom_sid **sids,
				      enum lsa_SidType **types);

/* Query display info for a domain.  This returns enough information plus a
   bit extra to give an overview of domain users for the User Manager
   application. */
static NTSTATUS msrpc_query_user_list(struct winbindd_domain *domain,
				      TALLOC_CTX *mem_ctx,
				      uint32_t *pnum_info,
				      struct wbint_userinfo **pinfo)
{
	struct rpc_pipe_client *samr_pipe = NULL;
	struct policy_handle dom_pol;
	struct wbint_userinfo *info = NULL;
	uint32_t num_info = 0;
	TALLOC_CTX *tmp_ctx;
	NTSTATUS status;

	DEBUG(3, ("msrpc_query_user_list\n"));

	if (pnum_info) {
		*pnum_info = 0;
	}

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	if ( !winbindd_can_contact_domain( domain ) ) {
		DEBUG(10,("query_user_list: No incoming trust for domain %s\n",
			  domain->name));
		status = NT_STATUS_OK;
		goto done;
	}

	status = cm_connect_sam(domain, tmp_ctx, &samr_pipe, &dom_pol);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	status = rpc_query_user_list(tmp_ctx,
				     samr_pipe,
				     &dom_pol,
				     &domain->sid,
				     &num_info,
				     &info);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	if (pnum_info) {
		*pnum_info = num_info;
	}

	if (pinfo) {
		*pinfo = talloc_move(mem_ctx, &info);
	}

done:
	TALLOC_FREE(tmp_ctx);
	return status;
}

/* list all domain groups */
static NTSTATUS msrpc_enum_dom_groups(struct winbindd_domain *domain,
				      TALLOC_CTX *mem_ctx,
				      uint32_t *pnum_info,
				      struct wb_acct_info **pinfo)
{
	struct rpc_pipe_client *samr_pipe;
	struct policy_handle dom_pol;
	struct wb_acct_info *info = NULL;
	uint32_t num_info = 0;
	TALLOC_CTX *tmp_ctx;
	NTSTATUS status;

	DEBUG(3,("msrpc_enum_dom_groups\n"));

	if (pnum_info) {
		*pnum_info = 0;
	}

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	if ( !winbindd_can_contact_domain( domain ) ) {
		DEBUG(10,("enum_domain_groups: No incoming trust for domain %s\n",
			  domain->name));
		status = NT_STATUS_OK;
		goto done;
	}

	status = cm_connect_sam(domain, tmp_ctx, &samr_pipe, &dom_pol);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	status = rpc_enum_dom_groups(tmp_ctx,
				     samr_pipe,
				     &dom_pol,
				     &num_info,
				     &info);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	if (pnum_info) {
		*pnum_info = num_info;
	}

	if (pinfo) {
		*pinfo = talloc_move(mem_ctx, &info);
	}

done:
	TALLOC_FREE(tmp_ctx);
	return status;
}

/* List all domain groups */

static NTSTATUS msrpc_enum_local_groups(struct winbindd_domain *domain,
					TALLOC_CTX *mem_ctx,
					uint32_t *pnum_info,
					struct wb_acct_info **pinfo)
{
	struct rpc_pipe_client *samr_pipe;
	struct policy_handle dom_pol;
	struct wb_acct_info *info = NULL;
	uint32_t num_info = 0;
	TALLOC_CTX *tmp_ctx;
	NTSTATUS status;

	DEBUG(3,("msrpc_enum_local_groups\n"));

	if (pnum_info) {
		*pnum_info = 0;
	}

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	if ( !winbindd_can_contact_domain( domain ) ) {
		DEBUG(10,("enum_local_groups: No incoming trust for domain %s\n",
			  domain->name));
		status = NT_STATUS_OK;
		goto done;
	}

	status = cm_connect_sam(domain, tmp_ctx, &samr_pipe, &dom_pol);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	status = rpc_enum_local_groups(mem_ctx,
				       samr_pipe,
				       &dom_pol,
				       &num_info,
				       &info);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	if (pnum_info) {
		*pnum_info = num_info;
	}

	if (pinfo) {
		*pinfo = talloc_move(mem_ctx, &info);
	}

done:
	TALLOC_FREE(tmp_ctx);
	return status;
}

/* convert a single name to a sid in a domain */
static NTSTATUS msrpc_name_to_sid(struct winbindd_domain *domain,
				  TALLOC_CTX *mem_ctx,
				  const char *domain_name,
				  const char *name,
				  uint32_t flags,
				  struct dom_sid *sid,
				  enum lsa_SidType *type)
{
	NTSTATUS result;
	struct dom_sid *sids = NULL;
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

	DEBUG(3, ("msrpc_name_to_sid: name=%s\n", full_name));

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
				  const struct dom_sid *sid,
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

	DEBUG(3, ("msrpc_sid_to_name: %s for domain %s\n", sid_string_dbg(sid),
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
				    const struct dom_sid *sid,
				    uint32 *rids,
				    size_t num_rids,
				    char **domain_name,
				    char ***names,
				    enum lsa_SidType **types)
{
	char **domains;
	NTSTATUS result;
	struct dom_sid *sids;
	size_t i;
	char **ret_names;

	DEBUG(3, ("msrpc_rids_to_names: domain %s\n", domain->name ));

	if (num_rids) {
		sids = TALLOC_ARRAY(mem_ctx, struct dom_sid, num_rids);
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
static NTSTATUS msrpc_query_user(struct winbindd_domain *domain,
			   TALLOC_CTX *mem_ctx, 
			   const struct dom_sid *user_sid,
			   struct wbint_userinfo *user_info)
{
	struct rpc_pipe_client *samr_pipe;
	struct policy_handle dom_pol;
	struct netr_SamInfo3 *user;
	TALLOC_CTX *tmp_ctx;
	NTSTATUS status;

	DEBUG(3,("msrpc_query_user sid=%s\n", sid_string_dbg(user_sid)));

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	if (user_info) {
		user_info->homedir = NULL;
		user_info->shell = NULL;
		user_info->primary_gid = (gid_t)-1;
	}

	/* try netsamlogon cache first */
	user = netsamlogon_cache_get(tmp_ctx, user_sid);
	if (user != NULL) {
		DEBUG(5,("msrpc_query_user: Cache lookup succeeded for %s\n",
			sid_string_dbg(user_sid)));

		sid_compose(&user_info->user_sid, &domain->sid, user->base.rid);
		sid_compose(&user_info->group_sid, &domain->sid,
			    user->base.primary_gid);

		user_info->acct_name = talloc_strdup(user_info,
						     user->base.account_name.string);
		user_info->full_name = talloc_strdup(user_info,
						     user->base.full_name.string);

		status = NT_STATUS_OK;
		goto done;
	}

	if ( !winbindd_can_contact_domain( domain ) ) {
		DEBUG(10,("query_user: No incoming trust for domain %s\n",
			  domain->name));
		/* Tell the cache manager not to remember this one */
		status = NT_STATUS_SYNCHRONIZATION_REQUIRED;
		goto done;
	}

	/* no cache; hit the wire */
	status = cm_connect_sam(domain, tmp_ctx, &samr_pipe, &dom_pol);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	status = rpc_query_user(tmp_ctx,
				samr_pipe,
				&dom_pol,
				&domain->sid,
				user_sid,
				user_info);

done:
	TALLOC_FREE(tmp_ctx);
	return status;
}

/* Lookup groups a user is a member of.  I wish Unix had a call like this! */
static NTSTATUS msrpc_lookup_usergroups(struct winbindd_domain *domain,
					TALLOC_CTX *mem_ctx,
					const struct dom_sid *user_sid,
					uint32_t *pnum_groups,
					struct dom_sid **puser_grpsids)
{
	struct rpc_pipe_client *samr_pipe;
	struct policy_handle dom_pol;
	struct dom_sid *user_grpsids = NULL;
	uint32_t num_groups = 0;
	TALLOC_CTX *tmp_ctx;
	NTSTATUS status;

	DEBUG(3,("msrpc_lookup_usergroups sid=%s\n", sid_string_dbg(user_sid)));

	*pnum_groups = 0;

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	/* Check if we have a cached user_info_3 */
	status = lookup_usergroups_cached(domain,
					  tmp_ctx,
					  user_sid,
					  &num_groups,
					  &user_grpsids);
	if (NT_STATUS_IS_OK(status)) {
		goto cached;
	}

	if ( !winbindd_can_contact_domain( domain ) ) {
		DEBUG(10,("lookup_usergroups: No incoming trust for domain %s\n",
			  domain->name));

		/* Tell the cache manager not to remember this one */
		status = NT_STATUS_SYNCHRONIZATION_REQUIRED;
		goto done;
	}

	/* no cache; hit the wire */
	status = cm_connect_sam(domain, tmp_ctx, &samr_pipe, &dom_pol);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	status = rpc_lookup_usergroups(tmp_ctx,
				       samr_pipe,
				       &dom_pol,
				       &domain->sid,
				       user_sid,
				       &num_groups,
				       &user_grpsids);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

cached:
	*pnum_groups = num_groups;

	if (puser_grpsids) {
		*puser_grpsids = talloc_move(mem_ctx, &user_grpsids);
	}

done:
	TALLOC_FREE(tmp_ctx);
	return status;
	return NT_STATUS_OK;
}

#define MAX_SAM_ENTRIES_W2K 0x400 /* 1024 */

static NTSTATUS msrpc_lookup_useraliases(struct winbindd_domain *domain,
					 TALLOC_CTX *mem_ctx,
					 uint32 num_sids, const struct dom_sid *sids,
					 uint32 *pnum_aliases,
					 uint32 **palias_rids)
{
	struct rpc_pipe_client *samr_pipe;
	struct policy_handle dom_pol;
	uint32_t num_aliases = 0;
	uint32_t *alias_rids = NULL;
	TALLOC_CTX *tmp_ctx;
	NTSTATUS status;

	DEBUG(3,("msrpc_lookup_useraliases\n"));

	if (pnum_aliases) {
		*pnum_aliases = 0;
	}

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	if (!winbindd_can_contact_domain(domain)) {
		DEBUG(10,("msrpc_lookup_useraliases: No incoming trust for domain %s\n",
			  domain->name));
		/* Tell the cache manager not to remember this one */
		status = NT_STATUS_SYNCHRONIZATION_REQUIRED;
		goto done;
	}

	status = cm_connect_sam(domain, tmp_ctx, &samr_pipe, &dom_pol);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	status = rpc_lookup_useraliases(tmp_ctx,
					samr_pipe,
					&dom_pol,
					num_sids,
					sids,
					&num_aliases,
					&alias_rids);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	if (pnum_aliases) {
		*pnum_aliases = num_aliases;
	}

	if (palias_rids) {
		*palias_rids = talloc_move(mem_ctx, &alias_rids);
	}

done:
	TALLOC_FREE(tmp_ctx);
	return status;
}


/* Lookup group membership given a rid.   */
static NTSTATUS msrpc_lookup_groupmem(struct winbindd_domain *domain,
				      TALLOC_CTX *mem_ctx,
				      const struct dom_sid *group_sid,
				      enum lsa_SidType type,
				      uint32_t *num_names,
				      struct dom_sid **sid_mem,
				      char ***names,
				      uint32_t **name_types)
{
	NTSTATUS status, result;
        uint32 i, total_names = 0;
        struct policy_handle dom_pol, group_pol;
        uint32 des_access = SEC_FLAG_MAXIMUM_ALLOWED;
	uint32 *rid_mem = NULL;
	uint32 group_rid;
	unsigned int j, r;
	struct rpc_pipe_client *cli;
	unsigned int orig_timeout;
	struct samr_RidAttrArray *rids = NULL;
	struct dcerpc_binding_handle *b;

	DEBUG(3,("msrpc_lookup_groupmem: %s sid=%s\n", domain->name,
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

	b = cli->binding_handle;

	status = dcerpc_samr_OpenGroup(b, mem_ctx,
				       &dom_pol,
				       des_access,
				       group_rid,
				       &group_pol,
				       &result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if (!NT_STATUS_IS_OK(result)) {
		return result;
	}

        /* Step #1: Get a list of user rids that are the members of the
           group. */

	/* This call can take a long time - allow the server to time out.
	   35 seconds should do it. */

	orig_timeout = rpccli_set_timeout(cli, 35000);

	status = dcerpc_samr_QueryGroupMember(b, mem_ctx,
					      &group_pol,
					      &rids,
					      &result);

	/* And restore our original timeout. */
	rpccli_set_timeout(cli, orig_timeout);

	{
		NTSTATUS _result;
		dcerpc_samr_Close(b, mem_ctx, &group_pol, &_result);
	}

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (!NT_STATUS_IS_OK(result)) {
		return result;
	}

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
        *sid_mem = TALLOC_ZERO_ARRAY(mem_ctx, struct dom_sid, *num_names);

	for (j=0;j<(*num_names);j++)
		sid_compose(&(*sid_mem)[j], &domain->sid, rid_mem[j]);

	if (*num_names>0 && (!*names || !*name_types))
		return NT_STATUS_NO_MEMORY;

	for (i = 0; i < *num_names; i += MAX_LOOKUP_RIDS) {
		int num_lookup_rids = MIN(*num_names - i, MAX_LOOKUP_RIDS);
		struct lsa_Strings tmp_names;
		struct samr_Ids tmp_types;

		/* Lookup a chunk of rids */

		status = dcerpc_samr_LookupRids(b, mem_ctx,
						&dom_pol,
						num_lookup_rids,
						&rid_mem[i],
						&tmp_names,
						&tmp_types,
						&result);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}

		/* see if we have a real error (and yes the
		   STATUS_SOME_UNMAPPED is the one returned from 2k) */

                if (!NT_STATUS_IS_OK(result) &&
		    !NT_STATUS_EQUAL(result, STATUS_SOME_UNMAPPED))
			return result;

		/* Copy result into array.  The talloc system will take
		   care of freeing the temporary arrays later on. */

		if (tmp_names.count != num_lookup_rids) {
			return NT_STATUS_INVALID_NETWORK_RESPONSE;
		}
		if (tmp_types.count != num_lookup_rids) {
			return NT_STATUS_INVALID_NETWORK_RESPONSE;
		}

		for (r=0; r<tmp_names.count; r++) {
			if (tmp_types.ids[r] == SID_NAME_UNKNOWN) {
				continue;
			}
			if (total_names >= *num_names) {
				break;
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
static NTSTATUS msrpc_sequence_number(struct winbindd_domain *domain,
				      uint32_t *pseq)
{
	struct rpc_pipe_client *samr_pipe;
	struct policy_handle dom_pol;
	uint32_t seq;
	TALLOC_CTX *tmp_ctx;
	NTSTATUS status;

	DEBUG(3, ("msrpc_sequence_number: fetch sequence_number for %s\n", domain->name));

	if (pseq) {
		*pseq = DOM_SEQUENCE_NONE;
	}

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	if ( !winbindd_can_contact_domain( domain ) ) {
		DEBUG(10,("sequence_number: No incoming trust for domain %s\n",
			  domain->name));
		if (pseq) {
			*pseq = time(NULL);
		}
		status = NT_STATUS_OK;
		goto done;
	}

#ifdef HAVE_LDAP
	if (domain->active_directory) {
		int rc;

		DEBUG(8,("using get_ldap_seq() to retrieve the "
			 "sequence number\n"));

		rc =  get_ldap_sequence_number(domain, &seq);
		if (rc == 0) {
			DEBUG(10,("domain_sequence_number: LDAP for "
				  "domain %s is %u\n",
				  domain->name, seq));

			if (pseq) {
				*pseq = seq;
			}

			status = NT_STATUS_OK;
			goto done;
		}

		DEBUG(10,("domain_sequence_number: failed to get LDAP "
			  "sequence number for domain %s\n",
			  domain->name ));
	}
#endif /* HAVE_LDAP */

	status = cm_connect_sam(domain, tmp_ctx, &samr_pipe, &dom_pol);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	status = rpc_sequence_number(tmp_ctx,
				     samr_pipe,
				     &dom_pol,
				     domain->name,
				     &seq);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	if (pseq) {
		*pseq = seq;
	}

done:
	TALLOC_FREE(tmp_ctx);
	return status;
}

/* get a list of trusted domains */
static NTSTATUS msrpc_trusted_domains(struct winbindd_domain *domain,
				      TALLOC_CTX *mem_ctx,
				      struct netr_DomainTrustList *ptrust_list)
{
	struct rpc_pipe_client *lsa_pipe;
	struct policy_handle lsa_policy;
	struct netr_DomainTrust *trusts = NULL;
	uint32_t num_trusts = 0;
	TALLOC_CTX *tmp_ctx;
	NTSTATUS status;

	DEBUG(3,("msrpc_trusted_domains\n"));

	if (ptrust_list) {
		ZERO_STRUCTP(ptrust_list);
	}

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	status = cm_connect_lsa(domain, tmp_ctx, &lsa_pipe, &lsa_policy);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	status = rpc_trusted_domains(tmp_ctx,
				     lsa_pipe,
				     &lsa_policy,
				     &num_trusts,
				     &trusts);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	if (ptrust_list) {
		ptrust_list->count = num_trusts;
		ptrust_list->array = talloc_move(mem_ctx, &trusts);
	}

done:
	TALLOC_FREE(tmp_ctx);
	return status;
}

/* find the lockout policy for a domain */
static NTSTATUS msrpc_lockout_policy(struct winbindd_domain *domain,
				     TALLOC_CTX *mem_ctx,
				     struct samr_DomInfo12 *lockout_policy)
{
	NTSTATUS status, result;
	struct rpc_pipe_client *cli;
	struct policy_handle dom_pol;
	union samr_DomainInfo *info = NULL;
	struct dcerpc_binding_handle *b;

	DEBUG(3, ("msrpc_lockout_policy: fetch lockout policy for %s\n", domain->name));

	if ( !winbindd_can_contact_domain( domain ) ) {
		DEBUG(10,("msrpc_lockout_policy: No incoming trust for domain %s\n",
			  domain->name));
		return NT_STATUS_NOT_SUPPORTED;
	}

	status = cm_connect_sam(domain, mem_ctx, &cli, &dom_pol);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	b = cli->binding_handle;

	status = dcerpc_samr_QueryDomainInfo(b, mem_ctx,
					     &dom_pol,
					     DomainLockoutInformation,
					     &info,
					     &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto done;
	}

	*lockout_policy = info->info12;

	DEBUG(10,("msrpc_lockout_policy: lockout_threshold %d\n",
		info->info12.lockout_threshold));

  done:

	return status;
}

/* find the password policy for a domain */
static NTSTATUS msrpc_password_policy(struct winbindd_domain *domain,
				      TALLOC_CTX *mem_ctx,
				      struct samr_DomInfo1 *password_policy)
{
	NTSTATUS status, result;
	struct rpc_pipe_client *cli;
	struct policy_handle dom_pol;
	union samr_DomainInfo *info = NULL;
	struct dcerpc_binding_handle *b;

	DEBUG(3, ("msrpc_password_policy: fetch password policy for %s\n",
		  domain->name));

	if ( !winbindd_can_contact_domain( domain ) ) {
		DEBUG(10,("msrpc_password_policy: No incoming trust for domain %s\n",
			  domain->name));
		return NT_STATUS_NOT_SUPPORTED;
	}

	status = cm_connect_sam(domain, mem_ctx, &cli, &dom_pol);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	b = cli->binding_handle;

	status = dcerpc_samr_QueryDomainInfo(b, mem_ctx,
					     &dom_pol,
					     DomainPasswordInformation,
					     &info,
					     &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		goto done;
	}

	*password_policy = info->info1;

	DEBUG(10,("msrpc_password_policy: min_length_password %d\n",
		info->info1.min_password_length));

  done:

	return status;
}

NTSTATUS winbindd_lookup_sids(TALLOC_CTX *mem_ctx,
			      struct winbindd_domain *domain,
			      uint32_t num_sids,
			      const struct dom_sid *sids,
			      char ***domains,
			      char ***names,
			      enum lsa_SidType **types)
{
	NTSTATUS status;
	NTSTATUS result;
	struct rpc_pipe_client *cli = NULL;
	struct dcerpc_binding_handle *b = NULL;
	struct policy_handle lsa_policy;
	unsigned int orig_timeout;
	bool use_lookupsids3 = false;
	bool retried = false;

 connect:
	status = cm_connect_lsat(domain, mem_ctx, &cli, &lsa_policy);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	b = cli->binding_handle;

	if (cli->transport->transport == NCACN_IP_TCP) {
		use_lookupsids3 = true;
	}

	/*
	 * This call can take a long time
	 * allow the server to time out.
	 * 35 seconds should do it.
	 */
	orig_timeout = dcerpc_binding_handle_set_timeout(b, 35000);

	status = dcerpc_lsa_lookup_sids_generic(b,
						mem_ctx,
						&lsa_policy,
						num_sids,
						sids,
						domains,
						names,
						types,
						use_lookupsids3,
						&result);

	/* And restore our original timeout. */
	dcerpc_binding_handle_set_timeout(b, orig_timeout);

	if (NT_STATUS_EQUAL(status, NT_STATUS_ACCESS_DENIED) ||
	    NT_STATUS_EQUAL(status, NT_STATUS_RPC_SEC_PKG_ERROR) ||
	    NT_STATUS_EQUAL(status, NT_STATUS_NETWORK_ACCESS_DENIED)) {
		/*
		 * This can happen if the schannel key is not
		 * valid anymore, we need to invalidate the
		 * all connections to the dc and reestablish
		 * a netlogon connection first.
		 */
		invalidate_cm_connection(&domain->conn);
		domain->can_do_ncacn_ip_tcp = domain->active_directory;
		if (!retried) {
			retried = true;
			goto connect;
		}
		status = NT_STATUS_ACCESS_DENIED;
	}

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (!NT_STATUS_IS_OK(result)) {
		return result;
	}

	return NT_STATUS_OK;
}

static NTSTATUS winbindd_lookup_names(TALLOC_CTX *mem_ctx,
				      struct winbindd_domain *domain,
				      uint32_t num_names,
				      const char **names,
				      const char ***domains,
				      struct dom_sid **sids,
				      enum lsa_SidType **types)
{
	NTSTATUS status;
	NTSTATUS result;
	struct rpc_pipe_client *cli = NULL;
	struct dcerpc_binding_handle *b = NULL;
	struct policy_handle lsa_policy;
	unsigned int orig_timeout = 0;
	bool use_lookupnames4 = false;
	bool retried = false;

 connect:
	status = cm_connect_lsat(domain, mem_ctx, &cli, &lsa_policy);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	b = cli->binding_handle;

	if (cli->transport->transport == NCACN_IP_TCP) {
		use_lookupnames4 = true;
	}

	/*
	 * This call can take a long time
	 * allow the server to time out.
	 * 35 seconds should do it.
	 */
	orig_timeout = dcerpc_binding_handle_set_timeout(b, 35000);

	status = dcerpc_lsa_lookup_names_generic(b,
						 mem_ctx,
						 &lsa_policy,
						 num_names,
						 (const char **) names,
						 domains,
						 1,
						 sids,
						 types,
						 use_lookupnames4,
						 &result);

	/* And restore our original timeout. */
	dcerpc_binding_handle_set_timeout(b, orig_timeout);

	if (NT_STATUS_EQUAL(status, NT_STATUS_ACCESS_DENIED) ||
	    NT_STATUS_EQUAL(status, NT_STATUS_RPC_SEC_PKG_ERROR) ||
	    NT_STATUS_EQUAL(status, NT_STATUS_NETWORK_ACCESS_DENIED)) {
		/*
		 * This can happen if the schannel key is not
		 * valid anymore, we need to invalidate the
		 * all connections to the dc and reestablish
		 * a netlogon connection first.
		 */
		invalidate_cm_connection(&domain->conn);
		if (!retried) {
			retried = true;
			goto connect;
		}
		status = NT_STATUS_ACCESS_DENIED;
	}

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (!NT_STATUS_IS_OK(result)) {
		return result;
	}

	return NT_STATUS_OK;
}

/* the rpc backend methods are exposed via this structure */
struct winbindd_methods msrpc_methods = {
	False,
	msrpc_query_user_list,
	msrpc_enum_dom_groups,
	msrpc_enum_local_groups,
	msrpc_name_to_sid,
	msrpc_sid_to_name,
	msrpc_rids_to_names,
	msrpc_query_user,
	msrpc_lookup_usergroups,
	msrpc_lookup_useraliases,
	msrpc_lookup_groupmem,
	msrpc_sequence_number,
	msrpc_lockout_policy,
	msrpc_password_policy,
	msrpc_trusted_domains,
};
