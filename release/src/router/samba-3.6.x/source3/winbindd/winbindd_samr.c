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
#include "../librpc/gen_ndr/ndr_samr_c.h"
#include "rpc_client/cli_samr.h"
#include "../librpc/gen_ndr/ndr_lsa_c.h"
#include "rpc_client/cli_lsarpc.h"
#include "rpc_server/rpc_ncacn_np.h"
#include "../libcli/security/security.h"
#include "passdb/machine_sid.h"
#include "auth.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_WINBIND

static NTSTATUS open_internal_samr_pipe(TALLOC_CTX *mem_ctx,
					struct rpc_pipe_client **samr_pipe)
{
	struct rpc_pipe_client *cli = NULL;
	struct auth_serversupplied_info *session_info = NULL;
	NTSTATUS status;

	if (session_info == NULL) {
		status = make_session_info_system(mem_ctx, &session_info);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0, ("open_samr_pipe: Could not create auth_serversupplied_info: %s\n",
				  nt_errstr(status)));
			return status;
		}
	}

	/* create a samr connection */
	status = rpc_pipe_open_interface(mem_ctx,
					&ndr_table_samr.syntax_id,
					session_info,
					NULL,
					winbind_messaging_context(),
					&cli);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("open_samr_pipe: Could not connect to samr_pipe: %s\n",
			  nt_errstr(status)));
		return status;
	}

	if (samr_pipe) {
		*samr_pipe = cli;
	}

	return NT_STATUS_OK;
}

NTSTATUS open_internal_samr_conn(TALLOC_CTX *mem_ctx,
				 struct winbindd_domain *domain,
				 struct rpc_pipe_client **samr_pipe,
				 struct policy_handle *samr_domain_hnd)
{
	NTSTATUS status, result;
	struct policy_handle samr_connect_hnd;
	struct dcerpc_binding_handle *b;

	status = open_internal_samr_pipe(mem_ctx, samr_pipe);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	b = (*samr_pipe)->binding_handle;

	status = dcerpc_samr_Connect2(b, mem_ctx,
				      (*samr_pipe)->desthost,
				      SEC_FLAG_MAXIMUM_ALLOWED,
				      &samr_connect_hnd,
				      &result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if (!NT_STATUS_IS_OK(result)) {
		return result;
	}

	status = dcerpc_samr_OpenDomain(b, mem_ctx,
					&samr_connect_hnd,
					SEC_FLAG_MAXIMUM_ALLOWED,
					&domain->sid,
					samr_domain_hnd,
					&result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	return result;
}

static NTSTATUS open_internal_lsa_pipe(TALLOC_CTX *mem_ctx,
				       struct rpc_pipe_client **lsa_pipe)
{
	struct rpc_pipe_client *cli = NULL;
	struct auth_serversupplied_info *session_info = NULL;
	NTSTATUS status;

	if (session_info == NULL) {
		status = make_session_info_system(mem_ctx, &session_info);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0, ("open_lsa_pipe: Could not create auth_serversupplied_info: %s\n",
				  nt_errstr(status)));
			return status;
		}
	}

	/* create a lsa connection */
	status = rpc_pipe_open_interface(mem_ctx,
					&ndr_table_lsarpc.syntax_id,
					session_info,
					NULL,
					winbind_messaging_context(),
					&cli);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("open_lsa_pipe: Could not connect to lsa_pipe: %s\n",
			  nt_errstr(status)));
		return status;
	}

	if (lsa_pipe) {
		*lsa_pipe = cli;
	}

	return NT_STATUS_OK;
}

static NTSTATUS open_internal_lsa_conn(TALLOC_CTX *mem_ctx,
				       struct rpc_pipe_client **lsa_pipe,
				       struct policy_handle *lsa_hnd)
{
	NTSTATUS status;

	status = open_internal_lsa_pipe(mem_ctx, lsa_pipe);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = rpccli_lsa_open_policy((*lsa_pipe),
					mem_ctx,
					true,
					SEC_FLAG_MAXIMUM_ALLOWED,
					lsa_hnd);

	return status;
}

/*********************************************************************
 SAM specific functions.
*********************************************************************/

/* List all domain groups */
static NTSTATUS sam_enum_dom_groups(struct winbindd_domain *domain,
				    TALLOC_CTX *mem_ctx,
				    uint32_t *pnum_info,
				    struct wb_acct_info **pinfo)
{
	struct rpc_pipe_client *samr_pipe;
	struct policy_handle dom_pol;
	struct wb_acct_info *info = NULL;
	uint32_t num_info = 0;
	TALLOC_CTX *tmp_ctx;
	NTSTATUS status, result;
	struct dcerpc_binding_handle *b = NULL;

	DEBUG(3,("sam_enum_dom_groups\n"));

	ZERO_STRUCT(dom_pol);

	if (pnum_info) {
		*pnum_info = 0;
	}

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	status = open_internal_samr_conn(tmp_ctx, domain, &samr_pipe, &dom_pol);
	if (!NT_STATUS_IS_OK(status)) {
		goto error;
	}

	b = samr_pipe->binding_handle;

	status = rpc_enum_dom_groups(tmp_ctx,
				     samr_pipe,
				     &dom_pol,
				     &num_info,
				     &info);
	if (!NT_STATUS_IS_OK(status)) {
		goto error;
	}

	if (pnum_info) {
		*pnum_info = num_info;
	}

	if (pinfo) {
		*pinfo = talloc_move(mem_ctx, &info);
	}

error:
	if (b && is_valid_policy_hnd(&dom_pol)) {
		dcerpc_samr_Close(b, mem_ctx, &dom_pol, &result);
	}
	TALLOC_FREE(tmp_ctx);
	return status;
}

/* Query display info for a domain */
static NTSTATUS sam_query_user_list(struct winbindd_domain *domain,
				    TALLOC_CTX *mem_ctx,
				    uint32_t *pnum_info,
				    struct wbint_userinfo **pinfo)
{
	struct rpc_pipe_client *samr_pipe = NULL;
	struct policy_handle dom_pol;
	struct wbint_userinfo *info = NULL;
	uint32_t num_info = 0;
	TALLOC_CTX *tmp_ctx;
	NTSTATUS status, result;
	struct dcerpc_binding_handle *b = NULL;

	DEBUG(3,("samr_query_user_list\n"));

	ZERO_STRUCT(dom_pol);

	if (pnum_info) {
		*pnum_info = 0;
	}

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	status = open_internal_samr_conn(tmp_ctx, domain, &samr_pipe, &dom_pol);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	b = samr_pipe->binding_handle;

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
	if (b && is_valid_policy_hnd(&dom_pol)) {
		dcerpc_samr_Close(b, mem_ctx, &dom_pol, &result);
	}

	TALLOC_FREE(tmp_ctx);
	return status;
}

/* Lookup user information from a rid or username. */
static NTSTATUS sam_query_user(struct winbindd_domain *domain,
			       TALLOC_CTX *mem_ctx,
			       const struct dom_sid *user_sid,
			       struct wbint_userinfo *user_info)
{
	struct rpc_pipe_client *samr_pipe;
	struct policy_handle dom_pol;
	TALLOC_CTX *tmp_ctx;
	NTSTATUS status, result;
	struct dcerpc_binding_handle *b = NULL;

	DEBUG(3,("sam_query_user\n"));

	ZERO_STRUCT(dom_pol);

	/* Paranoia check */
	if (!sid_check_is_in_our_domain(user_sid)) {
		return NT_STATUS_NO_SUCH_USER;
	}

	if (user_info) {
		user_info->homedir = NULL;
		user_info->shell = NULL;
		user_info->primary_gid = (gid_t) -1;
	}

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	status = open_internal_samr_conn(tmp_ctx, domain, &samr_pipe, &dom_pol);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	b = samr_pipe->binding_handle;

	status = rpc_query_user(tmp_ctx,
				samr_pipe,
				&dom_pol,
				&domain->sid,
				user_sid,
				user_info);

done:
	if (b && is_valid_policy_hnd(&dom_pol)) {
		dcerpc_samr_Close(b, mem_ctx, &dom_pol, &result);
	}

	TALLOC_FREE(tmp_ctx);
	return status;
}

/* get a list of trusted domains - builtin domain */
static NTSTATUS sam_trusted_domains(struct winbindd_domain *domain,
				    TALLOC_CTX *mem_ctx,
				    struct netr_DomainTrustList *ptrust_list)
{
	struct rpc_pipe_client *lsa_pipe;
	struct policy_handle lsa_policy;
	struct netr_DomainTrust *trusts = NULL;
	uint32_t num_trusts = 0;
	TALLOC_CTX *tmp_ctx;
	NTSTATUS status, result;
	struct dcerpc_binding_handle *b = NULL;

	DEBUG(3,("samr: trusted domains\n"));

	ZERO_STRUCT(lsa_policy);

	if (ptrust_list) {
		ZERO_STRUCTP(ptrust_list);
	}

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	status = open_internal_lsa_conn(tmp_ctx, &lsa_pipe, &lsa_policy);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	b = lsa_pipe->binding_handle;

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
	if (b && is_valid_policy_hnd(&lsa_policy)) {
		dcerpc_lsa_Close(b, mem_ctx, &lsa_policy, &result);
	}

	TALLOC_FREE(tmp_ctx);
	return status;
}

/* Lookup group membership given a rid.   */
static NTSTATUS sam_lookup_groupmem(struct winbindd_domain *domain,
				    TALLOC_CTX *mem_ctx,
				    const struct dom_sid *group_sid,
				    enum lsa_SidType type,
				    uint32_t *pnum_names,
				    struct dom_sid **psid_mem,
				    char ***pnames,
				    uint32_t **pname_types)
{
	struct rpc_pipe_client *samr_pipe;
	struct policy_handle dom_pol;

	uint32_t num_names = 0;
	struct dom_sid *sid_mem = NULL;
	char **names = NULL;
	uint32_t *name_types = NULL;

	TALLOC_CTX *tmp_ctx;
	NTSTATUS status, result;
	struct dcerpc_binding_handle *b = NULL;

	DEBUG(3,("sam_lookup_groupmem\n"));

	ZERO_STRUCT(dom_pol);

	/* Paranoia check */
	if (sid_check_is_in_builtin(group_sid) && (type != SID_NAME_ALIAS)) {
		/* There's no groups, only aliases in BUILTIN */
		return NT_STATUS_NO_SUCH_GROUP;
	}

	if (pnum_names) {
		pnum_names = 0;
	}

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	status = open_internal_samr_conn(tmp_ctx, domain, &samr_pipe, &dom_pol);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	b = samr_pipe->binding_handle;

	status = rpc_lookup_groupmem(tmp_ctx,
				     samr_pipe,
				     &dom_pol,
				     domain->name,
				     &domain->sid,
				     group_sid,
				     type,
				     &num_names,
				     &sid_mem,
				     &names,
				     &name_types);

	if (pnum_names) {
		*pnum_names = num_names;
	}

	if (pnames) {
		*pnames = talloc_move(mem_ctx, &names);
	}

	if (pname_types) {
		*pname_types = talloc_move(mem_ctx, &name_types);
	}

	if (psid_mem) {
		*psid_mem = talloc_move(mem_ctx, &sid_mem);
	}

done:
	if (b && is_valid_policy_hnd(&dom_pol)) {
		dcerpc_samr_Close(b, mem_ctx, &dom_pol, &result);
	}

	TALLOC_FREE(tmp_ctx);
	return status;
}

/*********************************************************************
 BUILTIN specific functions.
*********************************************************************/

/* List all domain groups */
static NTSTATUS builtin_enum_dom_groups(struct winbindd_domain *domain,
				TALLOC_CTX *mem_ctx,
				uint32 *num_entries,
				struct wb_acct_info **info)
{
	/* BUILTIN doesn't have domain groups */
	*num_entries = 0;
	*info = NULL;
	return NT_STATUS_OK;
}

/* Query display info for a domain */
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
				const struct dom_sid *user_sid,
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
 COMMON functions.
*********************************************************************/

/* List all local groups (aliases) */
static NTSTATUS sam_enum_local_groups(struct winbindd_domain *domain,
				      TALLOC_CTX *mem_ctx,
				      uint32_t *pnum_info,
				      struct wb_acct_info **pinfo)
{
	struct rpc_pipe_client *samr_pipe;
	struct policy_handle dom_pol;
	struct wb_acct_info *info = NULL;
	uint32_t num_info = 0;
	TALLOC_CTX *tmp_ctx;
	NTSTATUS status, result;
	struct dcerpc_binding_handle *b = NULL;

	DEBUG(3,("samr: enum local groups\n"));

	ZERO_STRUCT(dom_pol);

	if (pnum_info) {
		*pnum_info = 0;
	}

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	status = open_internal_samr_conn(tmp_ctx, domain, &samr_pipe, &dom_pol);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	b = samr_pipe->binding_handle;

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
	if (b && is_valid_policy_hnd(&dom_pol)) {
		dcerpc_samr_Close(b, mem_ctx, &dom_pol, &result);
	}

	TALLOC_FREE(tmp_ctx);
	return status;
}

/* convert a single name to a sid in a domain */
static NTSTATUS sam_name_to_sid(struct winbindd_domain *domain,
				   TALLOC_CTX *mem_ctx,
				   const char *domain_name,
				   const char *name,
				   uint32_t flags,
				   struct dom_sid *psid,
				   enum lsa_SidType *ptype)
{
	struct rpc_pipe_client *lsa_pipe;
	struct policy_handle lsa_policy;
	struct dom_sid sid;
	enum lsa_SidType type;
	TALLOC_CTX *tmp_ctx;
	NTSTATUS status, result;
	struct dcerpc_binding_handle *b = NULL;

	DEBUG(3,("sam_name_to_sid\n"));

	ZERO_STRUCT(lsa_policy);

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	status = open_internal_lsa_conn(tmp_ctx, &lsa_pipe, &lsa_policy);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	b = lsa_pipe->binding_handle;

	status = rpc_name_to_sid(tmp_ctx,
				 lsa_pipe,
				 &lsa_policy,
				 domain_name,
				 name,
				 flags,
				 &sid,
				 &type);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	if (psid) {
		sid_copy(psid, &sid);
	}
	if (ptype) {
		*ptype = type;
	}

done:
	if (b && is_valid_policy_hnd(&lsa_policy)) {
		dcerpc_lsa_Close(b, mem_ctx, &lsa_policy, &result);
	}

	TALLOC_FREE(tmp_ctx);
	return status;
}

/* convert a domain SID to a user or group name */
static NTSTATUS sam_sid_to_name(struct winbindd_domain *domain,
				TALLOC_CTX *mem_ctx,
				const struct dom_sid *sid,
				char **pdomain_name,
				char **pname,
				enum lsa_SidType *ptype)
{
	struct rpc_pipe_client *lsa_pipe;
	struct policy_handle lsa_policy;
	char *domain_name = NULL;
	char *name = NULL;
	enum lsa_SidType type;
	TALLOC_CTX *tmp_ctx;
	NTSTATUS status, result;
	struct dcerpc_binding_handle *b = NULL;

	DEBUG(3,("sam_sid_to_name\n"));

	ZERO_STRUCT(lsa_policy);

	/* Paranoia check */
	if (!sid_check_is_in_builtin(sid) &&
	    !sid_check_is_in_our_domain(sid) &&
	    !sid_check_is_in_unix_users(sid) &&
	    !sid_check_is_unix_users(sid) &&
	    !sid_check_is_in_unix_groups(sid) &&
	    !sid_check_is_unix_groups(sid) &&
	    !sid_check_is_in_wellknown_domain(sid)) {
		DEBUG(0, ("sam_sid_to_name: possible deadlock - trying to "
			  "lookup SID %s\n", sid_string_dbg(sid)));
		return NT_STATUS_NONE_MAPPED;
	}

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	status = open_internal_lsa_conn(tmp_ctx, &lsa_pipe, &lsa_policy);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	b = lsa_pipe->binding_handle;

	status = rpc_sid_to_name(tmp_ctx,
				 lsa_pipe,
				 &lsa_policy,
				 domain,
				 sid,
				 &domain_name,
				 &name,
				 &type);

	if (ptype) {
		*ptype = type;
	}

	if (pname) {
		*pname = talloc_move(mem_ctx, &name);
	}

	if (pdomain_name) {
		*pdomain_name = talloc_move(mem_ctx, &domain_name);
	}

done:
	if (b && is_valid_policy_hnd(&lsa_policy)) {
		dcerpc_lsa_Close(b, mem_ctx, &lsa_policy, &result);
	}

	TALLOC_FREE(tmp_ctx);
	return status;
}

static NTSTATUS sam_rids_to_names(struct winbindd_domain *domain,
				  TALLOC_CTX *mem_ctx,
				  const struct dom_sid *domain_sid,
				  uint32 *rids,
				  size_t num_rids,
				  char **pdomain_name,
				  char ***pnames,
				  enum lsa_SidType **ptypes)
{
	struct rpc_pipe_client *lsa_pipe;
	struct policy_handle lsa_policy;
	enum lsa_SidType *types = NULL;
	char *domain_name = NULL;
	char **names = NULL;
	TALLOC_CTX *tmp_ctx;
	NTSTATUS status, result;
	struct dcerpc_binding_handle *b = NULL;

	DEBUG(3,("sam_rids_to_names for %s\n", domain->name));

	ZERO_STRUCT(lsa_policy);

	/* Paranoia check */
	if (!sid_check_is_builtin(domain_sid) &&
	    !sid_check_is_domain(domain_sid) &&
	    !sid_check_is_unix_users(domain_sid) &&
	    !sid_check_is_unix_groups(domain_sid) &&
	    !sid_check_is_in_wellknown_domain(domain_sid)) {
		DEBUG(0, ("sam_rids_to_names: possible deadlock - trying to "
			  "lookup SID %s\n", sid_string_dbg(domain_sid)));
		return NT_STATUS_NONE_MAPPED;
	}

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	status = open_internal_lsa_conn(tmp_ctx, &lsa_pipe, &lsa_policy);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	b = lsa_pipe->binding_handle;

	status = rpc_rids_to_names(tmp_ctx,
				   lsa_pipe,
				   &lsa_policy,
				   domain,
				   domain_sid,
				   rids,
				   num_rids,
				   &domain_name,
				   &names,
				   &types);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	if (pdomain_name) {
		*pdomain_name = talloc_move(mem_ctx, &domain_name);
	}

	if (ptypes) {
		*ptypes = talloc_move(mem_ctx, &types);
	}

	if (pnames) {
		*pnames = talloc_move(mem_ctx, &names);
	}

done:
	if (b && is_valid_policy_hnd(&lsa_policy)) {
		dcerpc_lsa_Close(b, mem_ctx, &lsa_policy, &result);
	}

	TALLOC_FREE(tmp_ctx);
	return status;
}

static NTSTATUS sam_lockout_policy(struct winbindd_domain *domain,
				   TALLOC_CTX *mem_ctx,
				   struct samr_DomInfo12 *lockout_policy)
{
	struct rpc_pipe_client *samr_pipe;
	struct policy_handle dom_pol;
	union samr_DomainInfo *info = NULL;
	TALLOC_CTX *tmp_ctx;
	NTSTATUS status, result;
	struct dcerpc_binding_handle *b = NULL;

	DEBUG(3,("sam_lockout_policy\n"));

	ZERO_STRUCT(dom_pol);

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	status = open_internal_samr_conn(tmp_ctx, domain, &samr_pipe, &dom_pol);
	if (!NT_STATUS_IS_OK(status)) {
		goto error;
	}

	b = samr_pipe->binding_handle;

	status = dcerpc_samr_QueryDomainInfo(b,
					     mem_ctx,
					     &dom_pol,
					     12,
					     &info,
					     &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto error;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto error;
	}

	*lockout_policy = info->info12;

error:
	if (b && is_valid_policy_hnd(&dom_pol)) {
		dcerpc_samr_Close(b, mem_ctx, &dom_pol, &result);
	}

	TALLOC_FREE(tmp_ctx);
	return status;
}

static NTSTATUS sam_password_policy(struct winbindd_domain *domain,
				    TALLOC_CTX *mem_ctx,
				    struct samr_DomInfo1 *passwd_policy)
{
	struct rpc_pipe_client *samr_pipe;
	struct policy_handle dom_pol;
	union samr_DomainInfo *info = NULL;
	TALLOC_CTX *tmp_ctx;
	NTSTATUS status, result;
	struct dcerpc_binding_handle *b = NULL;

	DEBUG(3,("sam_password_policy\n"));

	ZERO_STRUCT(dom_pol);

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	status = open_internal_samr_conn(tmp_ctx, domain, &samr_pipe, &dom_pol);
	if (!NT_STATUS_IS_OK(status)) {
		goto error;
	}

	b = samr_pipe->binding_handle;

	status = dcerpc_samr_QueryDomainInfo(b,
					     mem_ctx,
					     &dom_pol,
					     1,
					     &info,
					     &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto error;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto error;
	}

	*passwd_policy = info->info1;

error:
	if (b && is_valid_policy_hnd(&dom_pol)) {
		dcerpc_samr_Close(b, mem_ctx, &dom_pol, &result);
	}

	TALLOC_FREE(tmp_ctx);
	return status;
}

/* Lookup groups a user is a member of. */
static NTSTATUS sam_lookup_usergroups(struct winbindd_domain *domain,
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
	NTSTATUS status, result;
	struct dcerpc_binding_handle *b = NULL;

	DEBUG(3,("sam_lookup_usergroups\n"));

	ZERO_STRUCT(dom_pol);

	if (pnum_groups) {
		*pnum_groups = 0;
	}

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	status = open_internal_samr_conn(tmp_ctx, domain, &samr_pipe, &dom_pol);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	b = samr_pipe->binding_handle;

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

	if (pnum_groups) {
		*pnum_groups = num_groups;
	}

	if (puser_grpsids) {
		*puser_grpsids = talloc_move(mem_ctx, &user_grpsids);
	}

done:
	if (b && is_valid_policy_hnd(&dom_pol)) {
		dcerpc_samr_Close(b, mem_ctx, &dom_pol, &result);
	}

	TALLOC_FREE(tmp_ctx);
	return status;
}

static NTSTATUS sam_lookup_useraliases(struct winbindd_domain *domain,
				       TALLOC_CTX *mem_ctx,
				       uint32_t num_sids,
				       const struct dom_sid *sids,
				       uint32_t *pnum_aliases,
				       uint32_t **palias_rids)
{
	struct rpc_pipe_client *samr_pipe;
	struct policy_handle dom_pol;
	uint32_t num_aliases = 0;
	uint32_t *alias_rids = NULL;
	TALLOC_CTX *tmp_ctx;
	NTSTATUS status, result;
	struct dcerpc_binding_handle *b = NULL;

	DEBUG(3,("sam_lookup_useraliases\n"));

	ZERO_STRUCT(dom_pol);

	if (pnum_aliases) {
		*pnum_aliases = 0;
	}

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	status = open_internal_samr_conn(tmp_ctx, domain, &samr_pipe, &dom_pol);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	b = samr_pipe->binding_handle;

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
	if (b && is_valid_policy_hnd(&dom_pol)) {
		dcerpc_samr_Close(b, mem_ctx, &dom_pol, &result);
	}

	TALLOC_FREE(tmp_ctx);
	return status;
}

/* find the sequence number for a domain */
static NTSTATUS sam_sequence_number(struct winbindd_domain *domain,
				    uint32_t *pseq)
{
	struct rpc_pipe_client *samr_pipe;
	struct policy_handle dom_pol;
	uint32_t seq;
	TALLOC_CTX *tmp_ctx;
	NTSTATUS status, result;
	struct dcerpc_binding_handle *b = NULL;

	DEBUG(3,("samr: sequence number\n"));

	ZERO_STRUCT(dom_pol);

	if (pseq) {
		*pseq = DOM_SEQUENCE_NONE;
	}

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	status = open_internal_samr_conn(tmp_ctx, domain, &samr_pipe, &dom_pol);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	b = samr_pipe->binding_handle;

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
	if (b && is_valid_policy_hnd(&dom_pol)) {
		dcerpc_samr_Close(b, tmp_ctx, &dom_pol, &result);
	}

	TALLOC_FREE(tmp_ctx);
	return status;
}

/* the rpc backend methods are exposed via this structure */
struct winbindd_methods builtin_passdb_methods = {
	.consistent            = false,

	.query_user_list       = builtin_query_user_list,
	.enum_dom_groups       = builtin_enum_dom_groups,
	.enum_local_groups     = sam_enum_local_groups,
	.name_to_sid           = sam_name_to_sid,
	.sid_to_name           = sam_sid_to_name,
	.rids_to_names         = sam_rids_to_names,
	.query_user            = builtin_query_user,
	.lookup_usergroups     = sam_lookup_usergroups,
	.lookup_useraliases    = sam_lookup_useraliases,
	.lookup_groupmem       = sam_lookup_groupmem,
	.sequence_number       = sam_sequence_number,
	.lockout_policy        = sam_lockout_policy,
	.password_policy       = sam_password_policy,
	.trusted_domains       = builtin_trusted_domains
};

/* the rpc backend methods are exposed via this structure */
struct winbindd_methods sam_passdb_methods = {
	.consistent            = false,

	.query_user_list       = sam_query_user_list,
	.enum_dom_groups       = sam_enum_dom_groups,
	.enum_local_groups     = sam_enum_local_groups,
	.name_to_sid           = sam_name_to_sid,
	.sid_to_name           = sam_sid_to_name,
	.rids_to_names         = sam_rids_to_names,
	.query_user            = sam_query_user,
	.lookup_usergroups     = sam_lookup_usergroups,
	.lookup_useraliases    = sam_lookup_useraliases,
	.lookup_groupmem       = sam_lookup_groupmem,
	.sequence_number       = sam_sequence_number,
	.lockout_policy        = sam_lockout_policy,
	.password_policy       = sam_password_policy,
	.trusted_domains       = sam_trusted_domains
};
