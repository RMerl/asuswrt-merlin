/* need access mask/acl implementation */

/* 
   Unix SMB/CIFS implementation.

   endpoint server for the lsarpc pipe

   Copyright (C) Andrew Tridgell 2004
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2004-2008
   
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

#include "rpc_server/lsa/lsa.h"
#include "system/kerberos.h"
#include "auth/kerberos/kerberos.h"
#include "librpc/gen_ndr/ndr_drsblobs.h"
#include "librpc/gen_ndr/ndr_lsa.h"
#include "../lib/crypto/crypto.h"

/*
  this type allows us to distinguish handle types
*/

/*
  state associated with a lsa_OpenAccount() operation
*/
struct lsa_account_state {
	struct lsa_policy_state *policy;
	uint32_t access_mask;
	struct dom_sid *account_sid;
};


/*
  state associated with a lsa_OpenSecret() operation
*/
struct lsa_secret_state {
	struct lsa_policy_state *policy;
	uint32_t access_mask;
	struct ldb_dn *secret_dn;
	struct ldb_context *sam_ldb;
	bool global;
};

/*
  state associated with a lsa_OpenTrustedDomain() operation
*/
struct lsa_trusted_domain_state {
	struct lsa_policy_state *policy;
	uint32_t access_mask;
	struct ldb_dn *trusted_domain_dn;
	struct ldb_dn *trusted_domain_user_dn;
};

/*
  this is based on the samba3 function make_lsa_object_sd()
  It uses the same logic, but with samba4 helper functions
 */
static NTSTATUS dcesrv_build_lsa_sd(TALLOC_CTX *mem_ctx, 
				    struct security_descriptor **sd,
				    struct dom_sid *sid, 
				    uint32_t sid_access)
{
	NTSTATUS status;
	uint32_t rid;
	struct dom_sid *domain_sid, *domain_admins_sid;
	const char *domain_admins_sid_str, *sidstr;
	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);

	status = dom_sid_split_rid(tmp_ctx, sid, &domain_sid, &rid);
	NT_STATUS_NOT_OK_RETURN(status);

	domain_admins_sid = dom_sid_add_rid(tmp_ctx, domain_sid, DOMAIN_RID_ADMINS);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(domain_admins_sid, tmp_ctx);

	domain_admins_sid_str = dom_sid_string(tmp_ctx, domain_admins_sid);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(domain_admins_sid_str, tmp_ctx);
	
	sidstr = dom_sid_string(tmp_ctx, sid);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(sidstr, tmp_ctx);
						      
	*sd = security_descriptor_dacl_create(mem_ctx,
					      0, sidstr, NULL,

					      SID_WORLD,
					      SEC_ACE_TYPE_ACCESS_ALLOWED,
					      SEC_GENERIC_EXECUTE | SEC_GENERIC_READ, 0,
					      
					      SID_BUILTIN_ADMINISTRATORS,
					      SEC_ACE_TYPE_ACCESS_ALLOWED,
					      SEC_GENERIC_ALL, 0,
					      
					      SID_BUILTIN_ACCOUNT_OPERATORS,
					      SEC_ACE_TYPE_ACCESS_ALLOWED,
					      SEC_GENERIC_ALL, 0,
					      
					      domain_admins_sid_str,
					      SEC_ACE_TYPE_ACCESS_ALLOWED,
					      SEC_GENERIC_ALL, 0,

					      sidstr,
					      SEC_ACE_TYPE_ACCESS_ALLOWED,
					      sid_access, 0,

					      NULL);
	talloc_free(tmp_ctx);

	NT_STATUS_HAVE_NO_MEMORY(*sd);

	return NT_STATUS_OK;
}


static NTSTATUS dcesrv_lsa_EnumAccountRights(struct dcesrv_call_state *dce_call, 
				      TALLOC_CTX *mem_ctx,
				      struct lsa_EnumAccountRights *r);

static NTSTATUS dcesrv_lsa_AddRemoveAccountRights(struct dcesrv_call_state *dce_call, 
					   TALLOC_CTX *mem_ctx,
					   struct lsa_policy_state *state,
					   int ldb_flag,
					   struct dom_sid *sid,
					   const struct lsa_RightSet *rights);

/* 
  lsa_Close 
*/
static NTSTATUS dcesrv_lsa_Close(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
			  struct lsa_Close *r)
{
	struct dcesrv_handle *h;

	*r->out.handle = *r->in.handle;

	DCESRV_PULL_HANDLE(h, r->in.handle, DCESRV_HANDLE_ANY);

	talloc_free(h);

	ZERO_STRUCTP(r->out.handle);

	return NT_STATUS_OK;
}


/* 
  lsa_Delete 
*/
static NTSTATUS dcesrv_lsa_Delete(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
			   struct lsa_Delete *r)
{
	return NT_STATUS_NOT_SUPPORTED;
}


/* 
  lsa_DeleteObject
*/
static NTSTATUS dcesrv_lsa_DeleteObject(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct lsa_DeleteObject *r)
{
	struct dcesrv_handle *h;
	int ret;

	DCESRV_PULL_HANDLE(h, r->in.handle, DCESRV_HANDLE_ANY);

	if (h->wire_handle.handle_type == LSA_HANDLE_SECRET) {
		struct lsa_secret_state *secret_state = h->data;

		/* Ensure user is permitted to delete this... */
		switch (security_session_user_level(dce_call->conn->auth_state.session_info))
		{
		case SECURITY_SYSTEM:
		case SECURITY_ADMINISTRATOR:
			break;
		default:
			/* Users and annonymous are not allowed delete things */
			return NT_STATUS_ACCESS_DENIED;
		}

		ret = ldb_delete(secret_state->sam_ldb, 
				 secret_state->secret_dn);
		talloc_free(h);
		if (ret != 0) {
			return NT_STATUS_INVALID_HANDLE;
		}

		ZERO_STRUCTP(r->out.handle);

		return NT_STATUS_OK;
	} else if (h->wire_handle.handle_type == LSA_HANDLE_TRUSTED_DOMAIN) {
		struct lsa_trusted_domain_state *trusted_domain_state = 
			talloc_get_type(h->data, struct lsa_trusted_domain_state);
		ret = ldb_transaction_start(trusted_domain_state->policy->sam_ldb);
		if (ret != 0) {
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		ret = ldb_delete(trusted_domain_state->policy->sam_ldb, 
				 trusted_domain_state->trusted_domain_dn);
		if (ret != 0) {
			ldb_transaction_cancel(trusted_domain_state->policy->sam_ldb);
			return NT_STATUS_INVALID_HANDLE;
		}

		if (trusted_domain_state->trusted_domain_user_dn) {
			ret = ldb_delete(trusted_domain_state->policy->sam_ldb, 
					 trusted_domain_state->trusted_domain_user_dn);
			if (ret != 0) {
				ldb_transaction_cancel(trusted_domain_state->policy->sam_ldb);
				return NT_STATUS_INVALID_HANDLE;
			}
		}

		ret = ldb_transaction_commit(trusted_domain_state->policy->sam_ldb);
		if (ret != 0) {
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}
		talloc_free(h);
		ZERO_STRUCTP(r->out.handle);

		return NT_STATUS_OK;
	} else if (h->wire_handle.handle_type == LSA_HANDLE_ACCOUNT) {
		struct lsa_RightSet *rights;
		struct lsa_account_state *astate;
		struct lsa_EnumAccountRights r2;
		NTSTATUS status;

		rights = talloc(mem_ctx, struct lsa_RightSet);

		DCESRV_PULL_HANDLE(h, r->in.handle, LSA_HANDLE_ACCOUNT);
		
		astate = h->data;

		r2.in.handle = &astate->policy->handle->wire_handle;
		r2.in.sid = astate->account_sid;
		r2.out.rights = rights;

		/* dcesrv_lsa_EnumAccountRights takes a LSA_HANDLE_POLICY,
		   but we have a LSA_HANDLE_ACCOUNT here, so this call
		   will always fail */
		status = dcesrv_lsa_EnumAccountRights(dce_call, mem_ctx, &r2);
		if (NT_STATUS_EQUAL(status, NT_STATUS_OBJECT_NAME_NOT_FOUND)) {
			return NT_STATUS_OK;
		}

		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}

		status = dcesrv_lsa_AddRemoveAccountRights(dce_call, mem_ctx, astate->policy, 
						    LDB_FLAG_MOD_DELETE, astate->account_sid,
						    r2.out.rights);
		if (NT_STATUS_EQUAL(status, NT_STATUS_OBJECT_NAME_NOT_FOUND)) {
			return NT_STATUS_OK;
		}

		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}

		ZERO_STRUCTP(r->out.handle);
	} 
	
	return NT_STATUS_INVALID_HANDLE;
}


/* 
  lsa_EnumPrivs 
*/
static NTSTATUS dcesrv_lsa_EnumPrivs(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
			      struct lsa_EnumPrivs *r)
{
	struct dcesrv_handle *h;
	struct lsa_policy_state *state;
	int i;
	const char *privname;

	DCESRV_PULL_HANDLE(h, r->in.handle, LSA_HANDLE_POLICY);

	state = h->data;

	i = *r->in.resume_handle;
	if (i == 0) i = 1;

	while ((privname = sec_privilege_name(i)) &&
	       r->out.privs->count < r->in.max_count) {
		struct lsa_PrivEntry *e;

		r->out.privs->privs = talloc_realloc(r->out.privs,
						       r->out.privs->privs, 
						       struct lsa_PrivEntry, 
						       r->out.privs->count+1);
		if (r->out.privs->privs == NULL) {
			return NT_STATUS_NO_MEMORY;
		}
		e = &r->out.privs->privs[r->out.privs->count];
		e->luid.low = i;
		e->luid.high = 0;
		e->name.string = privname;
		r->out.privs->count++;
		i++;
	}

	*r->out.resume_handle = i;

	return NT_STATUS_OK;
}


/* 
  lsa_QuerySecObj 
*/
static NTSTATUS dcesrv_lsa_QuerySecurity(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
					 struct lsa_QuerySecurity *r)
{
	struct dcesrv_handle *h;
	struct security_descriptor *sd;
	NTSTATUS status;
	struct dom_sid *sid;

	DCESRV_PULL_HANDLE(h, r->in.handle, DCESRV_HANDLE_ANY);

	sid = dce_call->conn->auth_state.session_info->security_token->user_sid;

	if (h->wire_handle.handle_type == LSA_HANDLE_POLICY) {
		status = dcesrv_build_lsa_sd(mem_ctx, &sd, sid, 0);
	} else 	if (h->wire_handle.handle_type == LSA_HANDLE_ACCOUNT) {
		status = dcesrv_build_lsa_sd(mem_ctx, &sd, sid, 
					     LSA_ACCOUNT_ALL_ACCESS);
	} else {
		return NT_STATUS_INVALID_HANDLE;
	}
	NT_STATUS_NOT_OK_RETURN(status);

	(*r->out.sdbuf) = talloc(mem_ctx, struct sec_desc_buf);
	NT_STATUS_HAVE_NO_MEMORY(*r->out.sdbuf);

	(*r->out.sdbuf)->sd = sd;
	
	return NT_STATUS_OK;
}


/* 
  lsa_SetSecObj 
*/
static NTSTATUS dcesrv_lsa_SetSecObj(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
			      struct lsa_SetSecObj *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  lsa_ChangePassword 
*/
static NTSTATUS dcesrv_lsa_ChangePassword(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				   struct lsa_ChangePassword *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}

/* 
  dssetup_DsRoleGetPrimaryDomainInformation 

  This is not an LSA call, but is the only call left on the DSSETUP
  pipe (after the pipe was truncated), and needs lsa_get_policy_state
*/
static WERROR dcesrv_dssetup_DsRoleGetPrimaryDomainInformation(struct dcesrv_call_state *dce_call, 
						 TALLOC_CTX *mem_ctx,
						 struct dssetup_DsRoleGetPrimaryDomainInformation *r)
{
	union dssetup_DsRoleInfo *info;

	info = talloc(mem_ctx, union dssetup_DsRoleInfo);
	W_ERROR_HAVE_NO_MEMORY(info);

	switch (r->in.level) {
	case DS_ROLE_BASIC_INFORMATION:
	{
		enum dssetup_DsRole role = DS_ROLE_STANDALONE_SERVER;
		uint32_t flags = 0;
		const char *domain = NULL;
		const char *dns_domain = NULL;
		const char *forest = NULL;
		struct GUID domain_guid;
		struct lsa_policy_state *state;

		NTSTATUS status = dcesrv_lsa_get_policy_state(dce_call, mem_ctx, &state);
		if (!NT_STATUS_IS_OK(status)) {
			return ntstatus_to_werror(status);
		}

		ZERO_STRUCT(domain_guid);

		switch (lp_server_role(dce_call->conn->dce_ctx->lp_ctx)) {
		case ROLE_STANDALONE:
			role		= DS_ROLE_STANDALONE_SERVER;
			break;
		case ROLE_DOMAIN_MEMBER:
			role		= DS_ROLE_MEMBER_SERVER;
			break;
		case ROLE_DOMAIN_CONTROLLER:
			if (samdb_is_pdc(state->sam_ldb)) {
				role	= DS_ROLE_PRIMARY_DC;
			} else {
				role    = DS_ROLE_BACKUP_DC;
			}
			break;
		}

		switch (lp_server_role(dce_call->conn->dce_ctx->lp_ctx)) {
		case ROLE_STANDALONE:
			domain		= talloc_strdup(mem_ctx, lp_workgroup(dce_call->conn->dce_ctx->lp_ctx));
			W_ERROR_HAVE_NO_MEMORY(domain);
			break;
		case ROLE_DOMAIN_MEMBER:
			domain		= talloc_strdup(mem_ctx, lp_workgroup(dce_call->conn->dce_ctx->lp_ctx));
			W_ERROR_HAVE_NO_MEMORY(domain);
			/* TODO: what is with dns_domain and forest and guid? */
			break;
		case ROLE_DOMAIN_CONTROLLER:
			flags		= DS_ROLE_PRIMARY_DS_RUNNING;

			if (state->mixed_domain == 1) {
				flags	|= DS_ROLE_PRIMARY_DS_MIXED_MODE;
			}
			
			domain		= state->domain_name;
			dns_domain	= state->domain_dns;
			forest		= state->forest_dns;

			domain_guid	= state->domain_guid;
			flags	|= DS_ROLE_PRIMARY_DOMAIN_GUID_PRESENT;
			break;
		}

		info->basic.role        = role; 
		info->basic.flags       = flags;
		info->basic.domain      = domain;
		info->basic.dns_domain  = dns_domain;
		info->basic.forest      = forest;
		info->basic.domain_guid = domain_guid;

		r->out.info = info;
		return WERR_OK;
	}
	case DS_ROLE_UPGRADE_STATUS:
	{
		info->upgrade.upgrading     = DS_ROLE_NOT_UPGRADING;
		info->upgrade.previous_role = DS_ROLE_PREVIOUS_UNKNOWN;

		r->out.info = info;
		return WERR_OK;
	}
	case DS_ROLE_OP_STATUS:
	{
		info->opstatus.status = DS_ROLE_OP_IDLE;

		r->out.info = info;
		return WERR_OK;
	}
	default:
		return WERR_INVALID_PARAM;
	}

	return WERR_INVALID_PARAM;
}

/*
  fill in the AccountDomain info
*/
static NTSTATUS dcesrv_lsa_info_AccountDomain(struct lsa_policy_state *state, TALLOC_CTX *mem_ctx,
				       struct lsa_DomainInfo *info)
{
	info->name.string = state->domain_name;
	info->sid         = state->domain_sid;

	return NT_STATUS_OK;
}

/*
  fill in the DNS domain info
*/
static NTSTATUS dcesrv_lsa_info_DNS(struct lsa_policy_state *state, TALLOC_CTX *mem_ctx,
			     struct lsa_DnsDomainInfo *info)
{
	info->name.string = state->domain_name;
	info->sid         = state->domain_sid;
	info->dns_domain.string = state->domain_dns;
	info->dns_forest.string = state->forest_dns;
	info->domain_guid       = state->domain_guid;

	return NT_STATUS_OK;
}

/* 
  lsa_QueryInfoPolicy2
*/
static NTSTATUS dcesrv_lsa_QueryInfoPolicy2(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				     struct lsa_QueryInfoPolicy2 *r)
{
	struct lsa_policy_state *state;
	struct dcesrv_handle *h;
	union lsa_PolicyInformation *info;

	*r->out.info = NULL;

	DCESRV_PULL_HANDLE(h, r->in.handle, LSA_HANDLE_POLICY);

	state = h->data;

	info = talloc_zero(mem_ctx, union lsa_PolicyInformation);
	if (!info) {
		return NT_STATUS_NO_MEMORY;
	}
	*r->out.info = info;

	switch (r->in.level) {
	case LSA_POLICY_INFO_AUDIT_LOG:
		/* we don't need to fill in any of this */
		ZERO_STRUCT(info->audit_log);
		return NT_STATUS_OK;
	case LSA_POLICY_INFO_AUDIT_EVENTS:
		/* we don't need to fill in any of this */
		ZERO_STRUCT(info->audit_events);
		return NT_STATUS_OK;
	case LSA_POLICY_INFO_PD:
		/* we don't need to fill in any of this */
		ZERO_STRUCT(info->pd);
		return NT_STATUS_OK;

	case LSA_POLICY_INFO_DOMAIN:
		return dcesrv_lsa_info_AccountDomain(state, mem_ctx, &info->domain);
	case LSA_POLICY_INFO_ACCOUNT_DOMAIN:
		return dcesrv_lsa_info_AccountDomain(state, mem_ctx, &info->account_domain);
	case LSA_POLICY_INFO_L_ACCOUNT_DOMAIN:
		return dcesrv_lsa_info_AccountDomain(state, mem_ctx, &info->l_account_domain);


	case LSA_POLICY_INFO_ROLE:
		info->role.role = LSA_ROLE_PRIMARY;
		return NT_STATUS_OK;

	case LSA_POLICY_INFO_DNS:
	case LSA_POLICY_INFO_DNS_INT:
		return dcesrv_lsa_info_DNS(state, mem_ctx, &info->dns);

	case LSA_POLICY_INFO_REPLICA:
		ZERO_STRUCT(info->replica);
		return NT_STATUS_OK;

	case LSA_POLICY_INFO_QUOTA:
		ZERO_STRUCT(info->quota);
		return NT_STATUS_OK;

	case LSA_POLICY_INFO_MOD:
	case LSA_POLICY_INFO_AUDIT_FULL_SET:
	case LSA_POLICY_INFO_AUDIT_FULL_QUERY:
		/* windows gives INVALID_PARAMETER */
		*r->out.info = NULL;
		return NT_STATUS_INVALID_PARAMETER;
	}

	*r->out.info = NULL;
	return NT_STATUS_INVALID_INFO_CLASS;
}

/* 
  lsa_QueryInfoPolicy 
*/
static NTSTATUS dcesrv_lsa_QueryInfoPolicy(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				    struct lsa_QueryInfoPolicy *r)
{
	struct lsa_QueryInfoPolicy2 r2;
	NTSTATUS status;

	ZERO_STRUCT(r2);

	r2.in.handle = r->in.handle;
	r2.in.level = r->in.level;
	r2.out.info = r->out.info;
	
	status = dcesrv_lsa_QueryInfoPolicy2(dce_call, mem_ctx, &r2);

	return status;
}

/* 
  lsa_SetInfoPolicy 
*/
static NTSTATUS dcesrv_lsa_SetInfoPolicy(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				  struct lsa_SetInfoPolicy *r)
{
	/* need to support this */
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  lsa_ClearAuditLog 
*/
static NTSTATUS dcesrv_lsa_ClearAuditLog(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				  struct lsa_ClearAuditLog *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  lsa_CreateAccount 

  This call does not seem to have any long-term effects, hence no database operations

  we need to talk to the MS product group to find out what this account database means!

  answer is that the lsa database is totally separate from the SAM and
  ldap databases. We are going to need a separate ldb to store these
  accounts. The SIDs on this account bear no relation to the SIDs in
  AD
*/
static NTSTATUS dcesrv_lsa_CreateAccount(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				  struct lsa_CreateAccount *r)
{
	struct lsa_account_state *astate;

	struct lsa_policy_state *state;
	struct dcesrv_handle *h, *ah;

	ZERO_STRUCTP(r->out.acct_handle);

	DCESRV_PULL_HANDLE(h, r->in.handle, LSA_HANDLE_POLICY);

	state = h->data;

	astate = talloc(dce_call->conn, struct lsa_account_state);
	if (astate == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	astate->account_sid = dom_sid_dup(astate, r->in.sid);
	if (astate->account_sid == NULL) {
		talloc_free(astate);
		return NT_STATUS_NO_MEMORY;
	}
	
	astate->policy = talloc_reference(astate, state);
	astate->access_mask = r->in.access_mask;

	ah = dcesrv_handle_new(dce_call->context, LSA_HANDLE_ACCOUNT);
	if (!ah) {
		talloc_free(astate);
		return NT_STATUS_NO_MEMORY;
	}

	ah->data = talloc_steal(ah, astate);

	*r->out.acct_handle = ah->wire_handle;

	return NT_STATUS_OK;
}


/* 
  lsa_EnumAccounts 
*/
static NTSTATUS dcesrv_lsa_EnumAccounts(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				 struct lsa_EnumAccounts *r)
{
	struct dcesrv_handle *h;
	struct lsa_policy_state *state;
	int ret, i;
	struct ldb_message **res;
	const char * const attrs[] = { "objectSid", NULL};
	uint32_t count;

	DCESRV_PULL_HANDLE(h, r->in.handle, LSA_HANDLE_POLICY);

	state = h->data;

	/* NOTE: This call must only return accounts that have at least
	   one privilege set 
	*/
	ret = gendb_search(state->sam_ldb, mem_ctx, NULL, &res, attrs, 
			   "(&(objectSid=*)(privilege=*))");
	if (ret < 0) {
		return NT_STATUS_NO_SUCH_USER;
	}

	if (*r->in.resume_handle >= ret) {
		return NT_STATUS_NO_MORE_ENTRIES;
	}

	count = ret - *r->in.resume_handle;
	if (count > r->in.num_entries) {
		count = r->in.num_entries;
	}

	if (count == 0) {
		return NT_STATUS_NO_MORE_ENTRIES;
	}

	r->out.sids->sids = talloc_array(r->out.sids, struct lsa_SidPtr, count);
	if (r->out.sids->sids == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	for (i=0;i<count;i++) {
		r->out.sids->sids[i].sid = 
			samdb_result_dom_sid(r->out.sids->sids, 
					     res[i + *r->in.resume_handle],
					     "objectSid");
		NT_STATUS_HAVE_NO_MEMORY(r->out.sids->sids[i].sid);
	}

	r->out.sids->num_sids = count;
	*r->out.resume_handle = count + *r->in.resume_handle;

	return NT_STATUS_OK;
	
}


/*
  lsa_CreateTrustedDomainEx2
*/
static NTSTATUS dcesrv_lsa_CreateTrustedDomain_base(struct dcesrv_call_state *dce_call,
						    TALLOC_CTX *mem_ctx,
						    struct lsa_CreateTrustedDomainEx2 *r,
						    int op)
{
	struct dcesrv_handle *policy_handle;
	struct lsa_policy_state *policy_state;
	struct lsa_trusted_domain_state *trusted_domain_state;
	struct dcesrv_handle *handle;
	struct ldb_message **msgs, *msg, *msg_user;
	const char *attrs[] = {
		NULL
	};
	const char *netbios_name;
	const char *dns_name;
	const char *name;
	DATA_BLOB session_key = data_blob(NULL, 0);
	DATA_BLOB trustAuthIncoming, trustAuthOutgoing, auth_blob;
	struct trustDomainPasswords auth_struct;
	int ret;
	NTSTATUS nt_status;
	enum ndr_err_code ndr_err;
	
	DCESRV_PULL_HANDLE(policy_handle, r->in.policy_handle, LSA_HANDLE_POLICY);
	ZERO_STRUCTP(r->out.trustdom_handle);
	
	policy_state = policy_handle->data;

	nt_status = dcesrv_fetch_session_key(dce_call->conn, &session_key);
	if (!NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	}

	netbios_name = r->in.info->netbios_name.string;
	if (!netbios_name) {
		return NT_STATUS_INVALID_PARAMETER;
	}
	
	dns_name = r->in.info->domain_name.string;
	
	trusted_domain_state = talloc_zero(mem_ctx, struct lsa_trusted_domain_state);
	if (!trusted_domain_state) {
		return NT_STATUS_NO_MEMORY;
	}
	trusted_domain_state->policy = policy_state;

	if (strcasecmp(netbios_name, "BUILTIN") == 0
	    || (dns_name && strcasecmp(dns_name, "BUILTIN") == 0) 
	    || (dom_sid_in_domain(policy_state->builtin_sid, r->in.info->sid))) {
		return NT_STATUS_INVALID_PARAMETER;;
	}

	if (strcasecmp(netbios_name, policy_state->domain_name) == 0
	    || strcasecmp(netbios_name, policy_state->domain_dns) == 0
	    || (dns_name && strcasecmp(dns_name, policy_state->domain_dns) == 0) 
	    || (dns_name && strcasecmp(dns_name, policy_state->domain_name) == 0)
	    || (dom_sid_equal(policy_state->domain_sid, r->in.info->sid))) {
		return NT_STATUS_CURRENT_DOMAIN_NOT_ALLOWED;
	}

	/* While this is a REF pointer, some of the functions that wrap this don't provide this */
	if (op == NDR_LSA_CREATETRUSTEDDOMAIN) {
		/* No secrets are created at this time, for this function */
		auth_struct.outgoing.count = 0;
		auth_struct.incoming.count = 0;
	} else {
		auth_blob = data_blob_const(r->in.auth_info->auth_blob.data, r->in.auth_info->auth_blob.size);
		arcfour_crypt_blob(auth_blob.data, auth_blob.length, &session_key);
		ndr_err = ndr_pull_struct_blob(&auth_blob, mem_ctx, 
					       lp_iconv_convenience(dce_call->conn->dce_ctx->lp_ctx),
					       &auth_struct,
					       (ndr_pull_flags_fn_t)ndr_pull_trustDomainPasswords);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			return NT_STATUS_INVALID_PARAMETER;
		}				

		if (op == NDR_LSA_CREATETRUSTEDDOMAINEX) {
			if (auth_struct.incoming.count > 1) {
				return NT_STATUS_INVALID_PARAMETER;
			}
		}
	}

	if (auth_struct.incoming.count) {
		int i;
		struct trustAuthInOutBlob incoming;
		
		incoming.count = auth_struct.incoming.count;
		incoming.current = talloc(mem_ctx, struct AuthenticationInformationArray);
		if (!incoming.current) {
			return NT_STATUS_NO_MEMORY;
		}
		
		incoming.current->array = *auth_struct.incoming.current;
		if (!incoming.current->array) {
			return NT_STATUS_NO_MEMORY;
		}

		incoming.previous = talloc(mem_ctx, struct AuthenticationInformationArray);
		if (!incoming.previous) {
			return NT_STATUS_NO_MEMORY;
		}
		incoming.previous->array = talloc_array(mem_ctx, struct AuthenticationInformation, incoming.count);
		if (!incoming.previous->array) {
			return NT_STATUS_NO_MEMORY;
		}

		for (i = 0; i < incoming.count; i++) {
			incoming.previous->array[i].LastUpdateTime = 0;
			incoming.previous->array[i].AuthType = 0;
		}
		ndr_err = ndr_push_struct_blob(&trustAuthIncoming, mem_ctx, 
					       lp_iconv_convenience(dce_call->conn->dce_ctx->lp_ctx),
					       &incoming,
					       (ndr_push_flags_fn_t)ndr_push_trustAuthInOutBlob);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			return NT_STATUS_INVALID_PARAMETER;
		}
	} else {
		trustAuthIncoming = data_blob(NULL, 0);
	}
	
	if (auth_struct.outgoing.count) {
		int i;
		struct trustAuthInOutBlob outgoing;
		
		outgoing.count = auth_struct.outgoing.count;
		outgoing.current = talloc(mem_ctx, struct AuthenticationInformationArray);
		if (!outgoing.current) {
			return NT_STATUS_NO_MEMORY;
		}
		
		outgoing.current->array = *auth_struct.outgoing.current;
		if (!outgoing.current->array) {
			return NT_STATUS_NO_MEMORY;
		}

		outgoing.previous = talloc(mem_ctx, struct AuthenticationInformationArray);
		if (!outgoing.previous) {
			return NT_STATUS_NO_MEMORY;
		}
		outgoing.previous->array = talloc_array(mem_ctx, struct AuthenticationInformation, outgoing.count);
		if (!outgoing.previous->array) {
			return NT_STATUS_NO_MEMORY;
		}

		for (i = 0; i < outgoing.count; i++) {
			outgoing.previous->array[i].LastUpdateTime = 0;
			outgoing.previous->array[i].AuthType = 0;
		}
		ndr_err = ndr_push_struct_blob(&trustAuthOutgoing, mem_ctx, 
					       lp_iconv_convenience(dce_call->conn->dce_ctx->lp_ctx),
					       &outgoing,
					       (ndr_push_flags_fn_t)ndr_push_trustAuthInOutBlob);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			return NT_STATUS_INVALID_PARAMETER;
		}
	} else {
		trustAuthOutgoing = data_blob(NULL, 0);
	}

	ret = ldb_transaction_start(policy_state->sam_ldb);
	if (ret != LDB_SUCCESS) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	if (dns_name) {
		char *dns_encoded = ldb_binary_encode_string(mem_ctx, netbios_name);
		char *netbios_encoded = ldb_binary_encode_string(mem_ctx, netbios_name);
		/* search for the trusted_domain record */
		ret = gendb_search(policy_state->sam_ldb,
				   mem_ctx, policy_state->system_dn, &msgs, attrs,
				   "(&(|(flatname=%s)(cn=%s)(trustPartner=%s)(flatname=%s)(cn=%s)(trustPartner=%s))(objectclass=trustedDomain))", 
				   dns_encoded, dns_encoded, dns_encoded, netbios_encoded, netbios_encoded, netbios_encoded);
		if (ret > 0) {
			ldb_transaction_cancel(policy_state->sam_ldb);
			return NT_STATUS_OBJECT_NAME_COLLISION;
		}
	} else {
		char *netbios_encoded = ldb_binary_encode_string(mem_ctx, netbios_name);
		/* search for the trusted_domain record */
		ret = gendb_search(policy_state->sam_ldb,
				   mem_ctx, policy_state->system_dn, &msgs, attrs,
				   "(&(|(flatname=%s)(cn=%s)(trustPartner=%s))(objectclass=trustedDomain))", 
				   netbios_encoded, netbios_encoded, netbios_encoded);
		if (ret > 0) {
			ldb_transaction_cancel(policy_state->sam_ldb);
			return NT_STATUS_OBJECT_NAME_COLLISION;
		}
	}
	
	if (ret < 0 ) {
		ldb_transaction_cancel(policy_state->sam_ldb);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}
	
	name = dns_name ? dns_name : netbios_name;

	msg = ldb_msg_new(mem_ctx);
	if (msg == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	msg->dn = ldb_dn_copy(mem_ctx, policy_state->system_dn);
	if ( ! ldb_dn_add_child_fmt(msg->dn, "cn=%s", name)) {
			ldb_transaction_cancel(policy_state->sam_ldb);
		return NT_STATUS_NO_MEMORY;
	}
	
	samdb_msg_add_string(trusted_domain_state->policy->sam_ldb, mem_ctx, msg, "flatname", netbios_name);

	if (r->in.info->sid) {
		const char *sid_string = dom_sid_string(mem_ctx, r->in.info->sid);
		if (!sid_string) {
			ldb_transaction_cancel(policy_state->sam_ldb);
			return NT_STATUS_NO_MEMORY;
		}
			
		samdb_msg_add_string(trusted_domain_state->policy->sam_ldb, mem_ctx, msg, "securityIdentifier", sid_string);
	}

	samdb_msg_add_string(trusted_domain_state->policy->sam_ldb, mem_ctx, msg, "objectClass", "trustedDomain");

	samdb_msg_add_int(trusted_domain_state->policy->sam_ldb, mem_ctx, msg, "trustType", r->in.info->trust_type);

	samdb_msg_add_int(trusted_domain_state->policy->sam_ldb, mem_ctx, msg, "trustAttributes", r->in.info->trust_attributes);

	samdb_msg_add_int(trusted_domain_state->policy->sam_ldb, mem_ctx, msg, "trustDirection", r->in.info->trust_direction);
	
	if (dns_name) {
		samdb_msg_add_string(trusted_domain_state->policy->sam_ldb, mem_ctx, msg, "trustPartner", dns_name);
	}

	if (trustAuthIncoming.data) {
		ret = ldb_msg_add_value(msg, "trustAuthIncoming", &trustAuthIncoming, NULL);
		if (ret != LDB_SUCCESS) {
			ldb_transaction_cancel(policy_state->sam_ldb);
			return NT_STATUS_NO_MEMORY;
		}
	}
	if (trustAuthOutgoing.data) {
		ret = ldb_msg_add_value(msg, "trustAuthOutgoing", &trustAuthOutgoing, NULL);
		if (ret != LDB_SUCCESS) {
			ldb_transaction_cancel(policy_state->sam_ldb);
			return NT_STATUS_NO_MEMORY;
		}
	}

	trusted_domain_state->trusted_domain_dn = talloc_reference(trusted_domain_state, msg->dn);

	/* create the trusted_domain */
	ret = ldb_add(trusted_domain_state->policy->sam_ldb, msg);
	switch (ret) {
	case  LDB_SUCCESS:
		break;
	case  LDB_ERR_ENTRY_ALREADY_EXISTS:
		ldb_transaction_cancel(trusted_domain_state->policy->sam_ldb);
		DEBUG(0,("Failed to create trusted domain record %s: %s\n",
			 ldb_dn_get_linearized(msg->dn),
			 ldb_errstring(trusted_domain_state->policy->sam_ldb)));
		return NT_STATUS_DOMAIN_EXISTS;
	case  LDB_ERR_INSUFFICIENT_ACCESS_RIGHTS:
		ldb_transaction_cancel(trusted_domain_state->policy->sam_ldb);
		DEBUG(0,("Failed to create trusted domain record %s: %s\n",
			 ldb_dn_get_linearized(msg->dn),
			 ldb_errstring(trusted_domain_state->policy->sam_ldb)));
		return NT_STATUS_ACCESS_DENIED;
	default:
		ldb_transaction_cancel(trusted_domain_state->policy->sam_ldb);
		DEBUG(0,("Failed to create user record %s: %s\n",
			 ldb_dn_get_linearized(msg->dn),
			 ldb_errstring(trusted_domain_state->policy->sam_ldb)));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	if (r->in.info->trust_direction & LSA_TRUST_DIRECTION_INBOUND) {
		msg_user = ldb_msg_new(mem_ctx);
		if (msg_user == NULL) {
			ldb_transaction_cancel(trusted_domain_state->policy->sam_ldb);
			return NT_STATUS_NO_MEMORY;
		}

		/* Inbound trusts must also create a cn=users object to match */

		trusted_domain_state->trusted_domain_user_dn = msg_user->dn
			= ldb_dn_copy(trusted_domain_state, policy_state->domain_dn);
		if ( ! ldb_dn_add_child_fmt(msg_user->dn, "cn=users")) {
			ldb_transaction_cancel(policy_state->sam_ldb);
			return NT_STATUS_NO_MEMORY;
		}
	
		if ( ! ldb_dn_add_child_fmt(msg_user->dn, "cn=%s", netbios_name)) {
			ldb_transaction_cancel(policy_state->sam_ldb);
			return NT_STATUS_NO_MEMORY;
		}

		ldb_msg_add_string(msg_user, "objectClass", "user");

		ldb_msg_add_steal_string(msg_user, "samAccountName", 
					 talloc_asprintf(mem_ctx, "%s$", netbios_name));

		if (samdb_msg_add_uint(trusted_domain_state->policy->sam_ldb, mem_ctx, msg_user, 
				       "userAccountControl", 
				       UF_INTERDOMAIN_TRUST_ACCOUNT) != 0) { 
			ldb_transaction_cancel(policy_state->sam_ldb);
			return NT_STATUS_NO_MEMORY; 
		}

		if (auth_struct.incoming.count) {
			int i;
			for (i=0; i < auth_struct.incoming.count; i++ ) {
				if (auth_struct.incoming.current[i]->AuthType == TRUST_AUTH_TYPE_NT4OWF) {
					samdb_msg_add_hash(trusted_domain_state->policy->sam_ldb, 
							   mem_ctx, msg_user, "unicodePwd", 
							   &auth_struct.incoming.current[i]->AuthInfo.nt4owf.password);
				} else if (auth_struct.incoming.current[i]->AuthType == TRUST_AUTH_TYPE_CLEAR) {
					DATA_BLOB new_password = data_blob_const(auth_struct.incoming.current[i]->AuthInfo.clear.password,
										 auth_struct.incoming.current[i]->AuthInfo.clear.size);
					ret = ldb_msg_add_value(msg_user, "clearTextPassword", &new_password, NULL);
					if (ret != LDB_SUCCESS) {
						ldb_transaction_cancel(policy_state->sam_ldb);
						return NT_STATUS_NO_MEMORY;
					}
				} 
			}
		}

		/* create the cn=users trusted_domain account */
		ret = ldb_add(trusted_domain_state->policy->sam_ldb, msg_user);
		switch (ret) {
		case  LDB_SUCCESS:
			break;
		case  LDB_ERR_ENTRY_ALREADY_EXISTS:
			ldb_transaction_cancel(trusted_domain_state->policy->sam_ldb);
			DEBUG(0,("Failed to create trusted domain record %s: %s\n",
				 ldb_dn_get_linearized(msg_user->dn),
				 ldb_errstring(trusted_domain_state->policy->sam_ldb)));
			return NT_STATUS_DOMAIN_EXISTS;
		case  LDB_ERR_INSUFFICIENT_ACCESS_RIGHTS:
			ldb_transaction_cancel(trusted_domain_state->policy->sam_ldb);
			DEBUG(0,("Failed to create trusted domain record %s: %s\n",
				 ldb_dn_get_linearized(msg_user->dn),
				 ldb_errstring(trusted_domain_state->policy->sam_ldb)));
			return NT_STATUS_ACCESS_DENIED;
		default:
			ldb_transaction_cancel(trusted_domain_state->policy->sam_ldb);
			DEBUG(0,("Failed to create user record %s: %s\n",
				 ldb_dn_get_linearized(msg_user->dn),
				 ldb_errstring(trusted_domain_state->policy->sam_ldb)));
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}
	}

	ret = ldb_transaction_commit(policy_state->sam_ldb);
	if (ret != LDB_SUCCESS) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	handle = dcesrv_handle_new(dce_call->context, LSA_HANDLE_TRUSTED_DOMAIN);
	if (!handle) {
		return NT_STATUS_NO_MEMORY;
	}
	
	handle->data = talloc_steal(handle, trusted_domain_state);
	
	trusted_domain_state->access_mask = r->in.access_mask;
	trusted_domain_state->policy = talloc_reference(trusted_domain_state, policy_state);
	
	*r->out.trustdom_handle = handle->wire_handle;
	
	return NT_STATUS_OK;
}

/*
  lsa_CreateTrustedDomainEx2
*/
static NTSTATUS dcesrv_lsa_CreateTrustedDomainEx2(struct dcesrv_call_state *dce_call,
					   TALLOC_CTX *mem_ctx,
					   struct lsa_CreateTrustedDomainEx2 *r)
{
	return dcesrv_lsa_CreateTrustedDomain_base(dce_call, mem_ctx, r, NDR_LSA_CREATETRUSTEDDOMAINEX2);
}
/*
  lsa_CreateTrustedDomainEx
*/
static NTSTATUS dcesrv_lsa_CreateTrustedDomainEx(struct dcesrv_call_state *dce_call,
					  TALLOC_CTX *mem_ctx,
					  struct lsa_CreateTrustedDomainEx *r)
{
	struct lsa_CreateTrustedDomainEx2 r2;

	r2.in.policy_handle = r->in.policy_handle;
	r2.in.info = r->in.info;
	r2.in.auth_info = r->in.auth_info;
	r2.out.trustdom_handle = r->out.trustdom_handle;
	return dcesrv_lsa_CreateTrustedDomain_base(dce_call, mem_ctx, &r2, NDR_LSA_CREATETRUSTEDDOMAINEX);
}

/* 
  lsa_CreateTrustedDomain 
*/
static NTSTATUS dcesrv_lsa_CreateTrustedDomain(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
					struct lsa_CreateTrustedDomain *r)
{
	struct lsa_CreateTrustedDomainEx2 r2;

	r2.in.policy_handle = r->in.policy_handle;
	r2.in.info = talloc(mem_ctx, struct lsa_TrustDomainInfoInfoEx);
	if (!r2.in.info) {
		return NT_STATUS_NO_MEMORY;
	}

	r2.in.info->domain_name.string = NULL;
	r2.in.info->netbios_name = r->in.info->name;
	r2.in.info->sid = r->in.info->sid;
	r2.in.info->trust_direction = LSA_TRUST_DIRECTION_OUTBOUND;
	r2.in.info->trust_type = LSA_TRUST_TYPE_DOWNLEVEL;
	r2.in.info->trust_attributes = 0;
	
	r2.in.access_mask = r->in.access_mask;
	r2.out.trustdom_handle = r->out.trustdom_handle;

	return dcesrv_lsa_CreateTrustedDomain_base(dce_call, mem_ctx, &r2, NDR_LSA_CREATETRUSTEDDOMAIN);
			 
}

/* 
  lsa_OpenTrustedDomain
*/
static NTSTATUS dcesrv_lsa_OpenTrustedDomain(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				      struct lsa_OpenTrustedDomain *r)
{
	struct dcesrv_handle *policy_handle;
	
	struct lsa_policy_state *policy_state;
	struct lsa_trusted_domain_state *trusted_domain_state;
	struct dcesrv_handle *handle;
	struct ldb_message **msgs;
	const char *attrs[] = {
		"trustDirection",
		"flatname",
		NULL
	};

	const char *sid_string;
	int ret;

	DCESRV_PULL_HANDLE(policy_handle, r->in.handle, LSA_HANDLE_POLICY);
	ZERO_STRUCTP(r->out.trustdom_handle);
	policy_state = policy_handle->data;

	trusted_domain_state = talloc_zero(mem_ctx, struct lsa_trusted_domain_state);
	if (!trusted_domain_state) {
		return NT_STATUS_NO_MEMORY;
	}
	trusted_domain_state->policy = policy_state;

	sid_string = dom_sid_string(mem_ctx, r->in.sid);
	if (!sid_string) {
		return NT_STATUS_NO_MEMORY;
	}

	/* search for the trusted_domain record */
	ret = gendb_search(trusted_domain_state->policy->sam_ldb,
			   mem_ctx, policy_state->system_dn, &msgs, attrs,
			   "(&(securityIdentifier=%s)(objectclass=trustedDomain))", 
			   sid_string);
	if (ret == 0) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}
	
	if (ret != 1) {
		DEBUG(0,("Found %d records matching DN %s\n", ret,
			 ldb_dn_get_linearized(policy_state->system_dn)));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	trusted_domain_state->trusted_domain_dn = talloc_reference(trusted_domain_state, msgs[0]->dn);

	trusted_domain_state->trusted_domain_user_dn = NULL;

	if (ldb_msg_find_attr_as_int(msgs[0], "trustDirection", 0) & LSA_TRUST_DIRECTION_INBOUND) {
		const char *flatname = ldb_binary_encode_string(mem_ctx, ldb_msg_find_attr_as_string(msgs[0], "flatname", NULL));
		/* search for the trusted_domain record */
		ret = gendb_search(trusted_domain_state->policy->sam_ldb,
				   mem_ctx, policy_state->domain_dn, &msgs, attrs,
				   "(&(samaccountname=%s$)(objectclass=user)(userAccountControl:1.2.840.113556.1.4.803:=%d))", 
				   flatname, UF_INTERDOMAIN_TRUST_ACCOUNT);
		if (ret == 1) {
			trusted_domain_state->trusted_domain_user_dn = talloc_steal(trusted_domain_state, msgs[0]->dn);
		}
	}
	handle = dcesrv_handle_new(dce_call->context, LSA_HANDLE_TRUSTED_DOMAIN);
	if (!handle) {
		return NT_STATUS_NO_MEMORY;
	}
	
	handle->data = talloc_steal(handle, trusted_domain_state);
	
	trusted_domain_state->access_mask = r->in.access_mask;
	trusted_domain_state->policy = talloc_reference(trusted_domain_state, policy_state);
	
	*r->out.trustdom_handle = handle->wire_handle;
	
	return NT_STATUS_OK;
}


/*
  lsa_OpenTrustedDomainByName
*/
static NTSTATUS dcesrv_lsa_OpenTrustedDomainByName(struct dcesrv_call_state *dce_call,
					    TALLOC_CTX *mem_ctx,
					    struct lsa_OpenTrustedDomainByName *r)
{
	struct dcesrv_handle *policy_handle;
	
	struct lsa_policy_state *policy_state;
	struct lsa_trusted_domain_state *trusted_domain_state;
	struct dcesrv_handle *handle;
	struct ldb_message **msgs;
	const char *attrs[] = {
		NULL
	};

	int ret;

	DCESRV_PULL_HANDLE(policy_handle, r->in.handle, LSA_HANDLE_POLICY);
	ZERO_STRUCTP(r->out.trustdom_handle);
	policy_state = policy_handle->data;

	if (!r->in.name.string) {
		return NT_STATUS_INVALID_PARAMETER;
	}
	
	trusted_domain_state = talloc_zero(mem_ctx, struct lsa_trusted_domain_state);
	if (!trusted_domain_state) {
		return NT_STATUS_NO_MEMORY;
	}
	trusted_domain_state->policy = policy_state;

	/* search for the trusted_domain record */
	ret = gendb_search(trusted_domain_state->policy->sam_ldb,
			   mem_ctx, policy_state->system_dn, &msgs, attrs,
			   "(&(flatname=%s)(objectclass=trustedDomain))", 
			   ldb_binary_encode_string(mem_ctx, r->in.name.string));
	if (ret == 0) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}
	
	if (ret != 1) {
		DEBUG(0,("Found %d records matching DN %s\n", ret,
			 ldb_dn_get_linearized(policy_state->system_dn)));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	trusted_domain_state->trusted_domain_dn = talloc_reference(trusted_domain_state, msgs[0]->dn);
	
	handle = dcesrv_handle_new(dce_call->context, LSA_HANDLE_TRUSTED_DOMAIN);
	if (!handle) {
		return NT_STATUS_NO_MEMORY;
	}
	
	handle->data = talloc_steal(handle, trusted_domain_state);
	
	trusted_domain_state->access_mask = r->in.access_mask;
	trusted_domain_state->policy = talloc_reference(trusted_domain_state, policy_state);
	
	*r->out.trustdom_handle = handle->wire_handle;
	
	return NT_STATUS_OK;
}



/* 
  lsa_SetTrustedDomainInfo
*/
static NTSTATUS dcesrv_lsa_SetTrustedDomainInfo(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
					 struct lsa_SetTrustedDomainInfo *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}



/* 
  lsa_SetInfomrationTrustedDomain
*/
static NTSTATUS dcesrv_lsa_SetInformationTrustedDomain(struct dcesrv_call_state *dce_call, 
						TALLOC_CTX *mem_ctx,
						struct lsa_SetInformationTrustedDomain *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  lsa_DeleteTrustedDomain
*/
static NTSTATUS dcesrv_lsa_DeleteTrustedDomain(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				      struct lsa_DeleteTrustedDomain *r)
{
	NTSTATUS status;
	struct lsa_OpenTrustedDomain opn;
	struct lsa_DeleteObject del;
	struct dcesrv_handle *h;

	opn.in.handle = r->in.handle;
	opn.in.sid = r->in.dom_sid;
	opn.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	opn.out.trustdom_handle = talloc(mem_ctx, struct policy_handle);
	if (!opn.out.trustdom_handle) {
		return NT_STATUS_NO_MEMORY;
	}
	status = dcesrv_lsa_OpenTrustedDomain(dce_call, mem_ctx, &opn);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	DCESRV_PULL_HANDLE(h, opn.out.trustdom_handle, DCESRV_HANDLE_ANY);
	talloc_steal(mem_ctx, h);

	del.in.handle = opn.out.trustdom_handle;
	del.out.handle = opn.out.trustdom_handle;
	status = dcesrv_lsa_DeleteObject(dce_call, mem_ctx, &del);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	return NT_STATUS_OK;
}

static NTSTATUS fill_trust_domain_ex(TALLOC_CTX *mem_ctx, 
				     struct ldb_message *msg, 
				     struct lsa_TrustDomainInfoInfoEx *info_ex) 
{
	info_ex->domain_name.string
		= ldb_msg_find_attr_as_string(msg, "trustPartner", NULL);
	info_ex->netbios_name.string
		= ldb_msg_find_attr_as_string(msg, "flatname", NULL);
	info_ex->sid 
		= samdb_result_dom_sid(mem_ctx, msg, "securityIdentifier");
	info_ex->trust_direction
		= ldb_msg_find_attr_as_int(msg, "trustDirection", 0);
	info_ex->trust_type
		= ldb_msg_find_attr_as_int(msg, "trustType", 0);
	info_ex->trust_attributes
		= ldb_msg_find_attr_as_int(msg, "trustAttributes", 0);	
	return NT_STATUS_OK;
}

/* 
  lsa_QueryTrustedDomainInfo
*/
static NTSTATUS dcesrv_lsa_QueryTrustedDomainInfo(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
					   struct lsa_QueryTrustedDomainInfo *r)
{
	union lsa_TrustedDomainInfo *info = NULL;
	struct dcesrv_handle *h;
	struct lsa_trusted_domain_state *trusted_domain_state;
	struct ldb_message *msg;
	int ret;
	struct ldb_message **res;
	const char *attrs[] = {
		"flatname", 
		"trustPartner",
		"securityIdentifier",
		"trustDirection",
		"trustType",
		"trustAttributes", 
		"msDs-supportedEncryptionTypes",
		NULL
	};

	DCESRV_PULL_HANDLE(h, r->in.trustdom_handle, LSA_HANDLE_TRUSTED_DOMAIN);

	trusted_domain_state = talloc_get_type(h->data, struct lsa_trusted_domain_state);

	/* pull all the user attributes */
	ret = gendb_search_dn(trusted_domain_state->policy->sam_ldb, mem_ctx,
			      trusted_domain_state->trusted_domain_dn, &res, attrs);
	if (ret != 1) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}
	msg = res[0];
	
	info = talloc_zero(mem_ctx, union lsa_TrustedDomainInfo);
	if (!info) {
		return NT_STATUS_NO_MEMORY;
	}
	*r->out.info = info;

	switch (r->in.level) {
	case LSA_TRUSTED_DOMAIN_INFO_NAME:
		info->name.netbios_name.string
			= samdb_result_string(msg, "flatname", NULL);					   
		break;
	case LSA_TRUSTED_DOMAIN_INFO_POSIX_OFFSET:
		info->posix_offset.posix_offset
			= samdb_result_uint(msg, "posixOffset", 0);					   
		break;
#if 0  /* Win2k3 doesn't implement this */
	case LSA_TRUSTED_DOMAIN_INFO_BASIC:
		r->out.info->info_basic.netbios_name.string 
			= ldb_msg_find_attr_as_string(msg, "flatname", NULL);
		r->out.info->info_basic.sid
			= samdb_result_dom_sid(mem_ctx, msg, "securityIdentifier");
		break;
#endif
	case LSA_TRUSTED_DOMAIN_INFO_INFO_EX:
		return fill_trust_domain_ex(mem_ctx, msg, &info->info_ex);

	case LSA_TRUSTED_DOMAIN_INFO_FULL_INFO:
		ZERO_STRUCT(info->full_info);
		return fill_trust_domain_ex(mem_ctx, msg, &info->full_info.info_ex);

	case LSA_TRUSTED_DOMAIN_INFO_FULL_INFO_2_INTERNAL:
		ZERO_STRUCT(info->full_info2_internal);
		info->full_info2_internal.posix_offset.posix_offset
			= samdb_result_uint(msg, "posixOffset", 0);					   
		return fill_trust_domain_ex(mem_ctx, msg, &info->full_info2_internal.info.info_ex);
		
	case LSA_TRUSTED_DOMAIN_SUPPORTED_ENCRYPTION_TYPES:
		info->enc_types.enc_types
			= samdb_result_uint(msg, "msDs-supportedEncryptionTypes", KERB_ENCTYPE_RC4_HMAC_MD5);
		break;

	case LSA_TRUSTED_DOMAIN_INFO_CONTROLLERS:
	case LSA_TRUSTED_DOMAIN_INFO_INFO_EX2_INTERNAL:
		/* oops, we don't want to return the info after all */
		talloc_free(info);
		*r->out.info = NULL;
		return NT_STATUS_INVALID_PARAMETER;
	default:
		/* oops, we don't want to return the info after all */
		talloc_free(info);
		*r->out.info = NULL;
		return NT_STATUS_INVALID_INFO_CLASS;
	}

	return NT_STATUS_OK;
}


/* 
  lsa_QueryTrustedDomainInfoBySid
*/
static NTSTATUS dcesrv_lsa_QueryTrustedDomainInfoBySid(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
						struct lsa_QueryTrustedDomainInfoBySid *r)
{
	NTSTATUS status;
	struct lsa_OpenTrustedDomain opn;
	struct lsa_QueryTrustedDomainInfo query;
	struct dcesrv_handle *h;

	opn.in.handle = r->in.handle;
	opn.in.sid = r->in.dom_sid;
	opn.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	opn.out.trustdom_handle = talloc(mem_ctx, struct policy_handle);
	if (!opn.out.trustdom_handle) {
		return NT_STATUS_NO_MEMORY;
	}
	status = dcesrv_lsa_OpenTrustedDomain(dce_call, mem_ctx, &opn);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	/* Ensure this handle goes away at the end of this call */
	DCESRV_PULL_HANDLE(h, opn.out.trustdom_handle, DCESRV_HANDLE_ANY);
	talloc_steal(mem_ctx, h);

	query.in.trustdom_handle = opn.out.trustdom_handle;
	query.in.level = r->in.level;
	query.out.info = r->out.info;
	status = dcesrv_lsa_QueryTrustedDomainInfo(dce_call, mem_ctx, &query);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	return NT_STATUS_OK;
}

/*
  lsa_SetTrustedDomainInfoByName
*/
static NTSTATUS dcesrv_lsa_SetTrustedDomainInfoByName(struct dcesrv_call_state *dce_call,
					       TALLOC_CTX *mem_ctx,
					       struct lsa_SetTrustedDomainInfoByName *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}

/* 
   lsa_QueryTrustedDomainInfoByName
*/
static NTSTATUS dcesrv_lsa_QueryTrustedDomainInfoByName(struct dcesrv_call_state *dce_call,
						 TALLOC_CTX *mem_ctx,
						 struct lsa_QueryTrustedDomainInfoByName *r)
{
	NTSTATUS status;
	struct lsa_OpenTrustedDomainByName opn;
	struct lsa_QueryTrustedDomainInfo query;
	struct dcesrv_handle *h;

	opn.in.handle = r->in.handle;
	opn.in.name = *r->in.trusted_domain;
	opn.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	opn.out.trustdom_handle = talloc(mem_ctx, struct policy_handle);
	if (!opn.out.trustdom_handle) {
		return NT_STATUS_NO_MEMORY;
	}
	status = dcesrv_lsa_OpenTrustedDomainByName(dce_call, mem_ctx, &opn);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	
	/* Ensure this handle goes away at the end of this call */
	DCESRV_PULL_HANDLE(h, opn.out.trustdom_handle, DCESRV_HANDLE_ANY);
	talloc_steal(mem_ctx, h);

	query.in.trustdom_handle = opn.out.trustdom_handle;
	query.in.level = r->in.level;
	query.out.info = r->out.info;
	status = dcesrv_lsa_QueryTrustedDomainInfo(dce_call, mem_ctx, &query);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	
	return NT_STATUS_OK;
}

/*
  lsa_CloseTrustedDomainEx 
*/
static NTSTATUS dcesrv_lsa_CloseTrustedDomainEx(struct dcesrv_call_state *dce_call,
					 TALLOC_CTX *mem_ctx,
					 struct lsa_CloseTrustedDomainEx *r)
{
	/* The result of a bad hair day from an IDL programmer?  Not
	 * implmented in Win2k3.  You should always just lsa_Close
	 * anyway. */
	return NT_STATUS_NOT_IMPLEMENTED;
}


/*
  comparison function for sorting lsa_DomainInformation array
*/
static int compare_DomainInfo(struct lsa_DomainInfo *e1, struct lsa_DomainInfo *e2)
{
	return strcasecmp_m(e1->name.string, e2->name.string);
}

/* 
  lsa_EnumTrustDom 
*/
static NTSTATUS dcesrv_lsa_EnumTrustDom(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				 struct lsa_EnumTrustDom *r)
{
	struct dcesrv_handle *policy_handle;
	struct lsa_DomainInfo *entries;
	struct lsa_policy_state *policy_state;
	struct ldb_message **domains;
	const char *attrs[] = {
		"flatname", 
		"securityIdentifier",
		NULL
	};


	int count, i;

	*r->out.resume_handle = 0;

	r->out.domains->domains = NULL;
	r->out.domains->count = 0;

	DCESRV_PULL_HANDLE(policy_handle, r->in.handle, LSA_HANDLE_POLICY);

	policy_state = policy_handle->data;

	/* search for all users in this domain. This could possibly be cached and 
	   resumed based on resume_key */
	count = gendb_search(policy_state->sam_ldb, mem_ctx, policy_state->system_dn, &domains, attrs, 
			     "objectclass=trustedDomain");
	if (count == -1) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	/* convert to lsa_TrustInformation format */
	entries = talloc_array(mem_ctx, struct lsa_DomainInfo, count);
	if (!entries) {
		return NT_STATUS_NO_MEMORY;
	}
	for (i=0;i<count;i++) {
		entries[i].sid = samdb_result_dom_sid(mem_ctx, domains[i], "securityIdentifier");
		entries[i].name.string = samdb_result_string(domains[i], "flatname", NULL);
	}

	/* sort the results by name */
	qsort(entries, count, sizeof(*entries), 
	      (comparison_fn_t)compare_DomainInfo);

	if (*r->in.resume_handle >= count) {
		*r->out.resume_handle = -1;

		return NT_STATUS_NO_MORE_ENTRIES;
	}

	/* return the rest, limit by max_size. Note that we 
	   use the w2k3 element size value of 60 */
	r->out.domains->count = count - *r->in.resume_handle;
	r->out.domains->count = MIN(r->out.domains->count, 
				 1+(r->in.max_size/LSA_ENUM_TRUST_DOMAIN_MULTIPLIER));

	r->out.domains->domains = entries + *r->in.resume_handle;
	r->out.domains->count = r->out.domains->count;

	if (r->out.domains->count < count - *r->in.resume_handle) {
		*r->out.resume_handle = *r->in.resume_handle + r->out.domains->count;
		return STATUS_MORE_ENTRIES;
	}

	/* according to MS-LSAD 3.1.4.7.8 output resume handle MUST
	 * always be larger than the previous input resume handle, in
	 * particular when hitting the last query it is vital to set the
	 * resume handle correctly to avoid infinite client loops, as
	 * seen e.g. with Windows XP SP3 when resume handle is 0 and
	 * status is NT_STATUS_OK - gd */

	*r->out.resume_handle = (uint32_t)-1;

	return NT_STATUS_OK;
}

/*
  comparison function for sorting lsa_DomainInformation array
*/
static int compare_TrustDomainInfoInfoEx(struct lsa_TrustDomainInfoInfoEx *e1, struct lsa_TrustDomainInfoInfoEx *e2)
{
	return strcasecmp_m(e1->netbios_name.string, e2->netbios_name.string);
}

/* 
  lsa_EnumTrustedDomainsEx 
*/
static NTSTATUS dcesrv_lsa_EnumTrustedDomainsEx(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
					struct lsa_EnumTrustedDomainsEx *r)
{
	struct dcesrv_handle *policy_handle;
	struct lsa_TrustDomainInfoInfoEx *entries;
	struct lsa_policy_state *policy_state;
	struct ldb_message **domains;
	const char *attrs[] = {
		"flatname", 
		"trustPartner",
		"securityIdentifier",
		"trustDirection",
		"trustType",
		"trustAttributes", 
		NULL
	};
	NTSTATUS nt_status;

	int count, i;

	*r->out.resume_handle = 0;

	r->out.domains->domains = NULL;
	r->out.domains->count = 0;

	DCESRV_PULL_HANDLE(policy_handle, r->in.handle, LSA_HANDLE_POLICY);

	policy_state = policy_handle->data;

	/* search for all users in this domain. This could possibly be cached and 
	   resumed based on resume_key */
	count = gendb_search(policy_state->sam_ldb, mem_ctx, policy_state->system_dn, &domains, attrs, 
			     "objectclass=trustedDomain");
	if (count == -1) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	/* convert to lsa_DomainInformation format */
	entries = talloc_array(mem_ctx, struct lsa_TrustDomainInfoInfoEx, count);
	if (!entries) {
		return NT_STATUS_NO_MEMORY;
	}
	for (i=0;i<count;i++) {
		nt_status = fill_trust_domain_ex(mem_ctx, domains[i], &entries[i]);
		if (!NT_STATUS_IS_OK(nt_status)) {
			return nt_status;
		}
	}

	/* sort the results by name */
	qsort(entries, count, sizeof(*entries), 
	      (comparison_fn_t)compare_TrustDomainInfoInfoEx);

	if (*r->in.resume_handle >= count) {
		*r->out.resume_handle = -1;

		return NT_STATUS_NO_MORE_ENTRIES;
	}

	/* return the rest, limit by max_size. Note that we 
	   use the w2k3 element size value of 60 */
	r->out.domains->count = count - *r->in.resume_handle;
	r->out.domains->count = MIN(r->out.domains->count, 
				 1+(r->in.max_size/LSA_ENUM_TRUST_DOMAIN_EX_MULTIPLIER));

	r->out.domains->domains = entries + *r->in.resume_handle;
	r->out.domains->count = r->out.domains->count;

	if (r->out.domains->count < count - *r->in.resume_handle) {
		*r->out.resume_handle = *r->in.resume_handle + r->out.domains->count;
		return STATUS_MORE_ENTRIES;
	}

	return NT_STATUS_OK;
}


/* 
  lsa_OpenAccount 
*/
static NTSTATUS dcesrv_lsa_OpenAccount(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				struct lsa_OpenAccount *r)
{
	struct dcesrv_handle *h, *ah;
	struct lsa_policy_state *state;
	struct lsa_account_state *astate;

	ZERO_STRUCTP(r->out.acct_handle);

	DCESRV_PULL_HANDLE(h, r->in.handle, LSA_HANDLE_POLICY);

	state = h->data;

	astate = talloc(dce_call->conn, struct lsa_account_state);
	if (astate == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	astate->account_sid = dom_sid_dup(astate, r->in.sid);
	if (astate->account_sid == NULL) {
		talloc_free(astate);
		return NT_STATUS_NO_MEMORY;
	}
	
	astate->policy = talloc_reference(astate, state);
	astate->access_mask = r->in.access_mask;

	ah = dcesrv_handle_new(dce_call->context, LSA_HANDLE_ACCOUNT);
	if (!ah) {
		talloc_free(astate);
		return NT_STATUS_NO_MEMORY;
	}

	ah->data = talloc_steal(ah, astate);

	*r->out.acct_handle = ah->wire_handle;

	return NT_STATUS_OK;
}


/* 
  lsa_EnumPrivsAccount 
*/
static NTSTATUS dcesrv_lsa_EnumPrivsAccount(struct dcesrv_call_state *dce_call, 
				     TALLOC_CTX *mem_ctx,
				     struct lsa_EnumPrivsAccount *r)
{
	struct dcesrv_handle *h;
	struct lsa_account_state *astate;
	int ret, i;
	struct ldb_message **res;
	const char * const attrs[] = { "privilege", NULL};
	struct ldb_message_element *el;
	const char *sidstr;
	struct lsa_PrivilegeSet *privs;

	DCESRV_PULL_HANDLE(h, r->in.handle, LSA_HANDLE_ACCOUNT);

	astate = h->data;

	privs = talloc(mem_ctx, struct lsa_PrivilegeSet);
	if (privs == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	privs->count = 0;
	privs->unknown = 0;
	privs->set = NULL;

	*r->out.privs = privs;

	sidstr = ldap_encode_ndr_dom_sid(mem_ctx, astate->account_sid);
	if (sidstr == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	ret = gendb_search(astate->policy->sam_ldb, mem_ctx, NULL, &res, attrs, 
			   "objectSid=%s", sidstr);
	if (ret != 1) {
		return NT_STATUS_OK;
	}

	el = ldb_msg_find_element(res[0], "privilege");
	if (el == NULL || el->num_values == 0) {
		return NT_STATUS_OK;
	}

	privs->set = talloc_array(privs,
				  struct lsa_LUIDAttribute, el->num_values);
	if (privs->set == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	for (i=0;i<el->num_values;i++) {
		int id = sec_privilege_id((const char *)el->values[i].data);
		if (id == -1) {
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}
		privs->set[i].attribute = 0;
		privs->set[i].luid.low = id;
		privs->set[i].luid.high = 0;
	}

	privs->count = el->num_values;

	return NT_STATUS_OK;
}

/* 
  lsa_EnumAccountRights 
*/
static NTSTATUS dcesrv_lsa_EnumAccountRights(struct dcesrv_call_state *dce_call, 
				      TALLOC_CTX *mem_ctx,
				      struct lsa_EnumAccountRights *r)
{
	struct dcesrv_handle *h;
	struct lsa_policy_state *state;
	int ret, i;
	struct ldb_message **res;
	const char * const attrs[] = { "privilege", NULL};
	const char *sidstr;
	struct ldb_message_element *el;

	DCESRV_PULL_HANDLE(h, r->in.handle, LSA_HANDLE_POLICY);

	state = h->data;

	sidstr = ldap_encode_ndr_dom_sid(mem_ctx, r->in.sid);
	if (sidstr == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	ret = gendb_search(state->sam_ldb, mem_ctx, NULL, &res, attrs, 
			   "(&(objectSid=%s)(privilege=*))", sidstr);
	if (ret == 0) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}
	if (ret > 1) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}
	if (ret == -1) {
		DEBUG(3, ("searching for account rights for SID: %s failed: %s", 
			  dom_sid_string(mem_ctx, r->in.sid),
			  ldb_errstring(state->sam_ldb)));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	el = ldb_msg_find_element(res[0], "privilege");
	if (el == NULL || el->num_values == 0) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	r->out.rights->count = el->num_values;
	r->out.rights->names = talloc_array(r->out.rights, 
					    struct lsa_StringLarge, r->out.rights->count);
	if (r->out.rights->names == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	for (i=0;i<el->num_values;i++) {
		r->out.rights->names[i].string = (const char *)el->values[i].data;
	}

	return NT_STATUS_OK;
}



/* 
  helper for lsa_AddAccountRights and lsa_RemoveAccountRights
*/
static NTSTATUS dcesrv_lsa_AddRemoveAccountRights(struct dcesrv_call_state *dce_call, 
					   TALLOC_CTX *mem_ctx,
					   struct lsa_policy_state *state,
					   int ldb_flag,
					   struct dom_sid *sid,
					   const struct lsa_RightSet *rights)
{
	const char *sidstr;
	struct ldb_message *msg;
	struct ldb_message_element *el;
	int i, ret;
	struct lsa_EnumAccountRights r2;

	sidstr = ldap_encode_ndr_dom_sid(mem_ctx, sid);
	if (sidstr == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	msg = ldb_msg_new(mem_ctx);
	if (msg == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	msg->dn = samdb_search_dn(state->sam_ldb, mem_ctx, 
				  NULL, "objectSid=%s", sidstr);
	if (msg->dn == NULL) {
		NTSTATUS status;
		if (ldb_flag == LDB_FLAG_MOD_DELETE) {
			return NT_STATUS_OBJECT_NAME_NOT_FOUND;
		}
		status = samdb_create_foreign_security_principal(state->sam_ldb, mem_ctx, 
								 sid, &msg->dn);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
		return NT_STATUS_NO_SUCH_USER;
	}

	if (ldb_msg_add_empty(msg, "privilege", ldb_flag, NULL)) {
		return NT_STATUS_NO_MEMORY;
	}

	if (ldb_flag == LDB_FLAG_MOD_ADD) {
		NTSTATUS status;

		r2.in.handle = &state->handle->wire_handle;
		r2.in.sid = sid;
		r2.out.rights = talloc(mem_ctx, struct lsa_RightSet);

		status = dcesrv_lsa_EnumAccountRights(dce_call, mem_ctx, &r2);
		if (!NT_STATUS_IS_OK(status)) {
			ZERO_STRUCTP(r2.out.rights);
		}
	}

	for (i=0;i<rights->count;i++) {
		if (sec_privilege_id(rights->names[i].string) == -1) {
			return NT_STATUS_NO_SUCH_PRIVILEGE;
		}

		if (ldb_flag == LDB_FLAG_MOD_ADD) {
			int j;
			for (j=0;j<r2.out.rights->count;j++) {
				if (strcasecmp_m(r2.out.rights->names[j].string, 
					       rights->names[i].string) == 0) {
					break;
				}
			}
			if (j != r2.out.rights->count) continue;
		}

		ret = ldb_msg_add_string(msg, "privilege", rights->names[i].string);
		if (ret != LDB_SUCCESS) {
			return NT_STATUS_NO_MEMORY;
		}
	}

	el = ldb_msg_find_element(msg, "privilege");
	if (!el) {
		return NT_STATUS_OK;
	}

	ret = ldb_modify(state->sam_ldb, msg);
	if (ret != 0) {
		if (ldb_flag == LDB_FLAG_MOD_DELETE && ret == LDB_ERR_NO_SUCH_ATTRIBUTE) {
			return NT_STATUS_OBJECT_NAME_NOT_FOUND;
		}
		DEBUG(3, ("Could not %s attributes from %s: %s", 
			  ldb_flag == LDB_FLAG_MOD_DELETE ? "delete" : "add",
			  ldb_dn_get_linearized(msg->dn), ldb_errstring(state->sam_ldb)));
		return NT_STATUS_UNEXPECTED_IO_ERROR;
	}

	return NT_STATUS_OK;
}

/* 
  lsa_AddPrivilegesToAccount
*/
static NTSTATUS dcesrv_lsa_AddPrivilegesToAccount(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
					   struct lsa_AddPrivilegesToAccount *r)
{
	struct lsa_RightSet rights;
	struct dcesrv_handle *h;
	struct lsa_account_state *astate;
	int i;

	DCESRV_PULL_HANDLE(h, r->in.handle, LSA_HANDLE_ACCOUNT);

	astate = h->data;

	rights.count = r->in.privs->count;
	rights.names = talloc_array(mem_ctx, struct lsa_StringLarge, rights.count);
	if (rights.names == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	for (i=0;i<rights.count;i++) {
		int id = r->in.privs->set[i].luid.low;
		if (r->in.privs->set[i].luid.high) {
			return NT_STATUS_NO_SUCH_PRIVILEGE;
		}
		rights.names[i].string = sec_privilege_name(id);
		if (rights.names[i].string == NULL) {
			return NT_STATUS_NO_SUCH_PRIVILEGE;
		}
	}

	return dcesrv_lsa_AddRemoveAccountRights(dce_call, mem_ctx, astate->policy, 
					  LDB_FLAG_MOD_ADD, astate->account_sid,
					  &rights);
}


/* 
  lsa_RemovePrivilegesFromAccount
*/
static NTSTATUS dcesrv_lsa_RemovePrivilegesFromAccount(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
						struct lsa_RemovePrivilegesFromAccount *r)
{
	struct lsa_RightSet *rights;
	struct dcesrv_handle *h;
	struct lsa_account_state *astate;
	int i;

	DCESRV_PULL_HANDLE(h, r->in.handle, LSA_HANDLE_ACCOUNT);

	astate = h->data;

	rights = talloc(mem_ctx, struct lsa_RightSet);

	if (r->in.remove_all == 1 && 
	    r->in.privs == NULL) {
		struct lsa_EnumAccountRights r2;
		NTSTATUS status;

		r2.in.handle = &astate->policy->handle->wire_handle;
		r2.in.sid = astate->account_sid;
		r2.out.rights = rights;

		status = dcesrv_lsa_EnumAccountRights(dce_call, mem_ctx, &r2);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}

		return dcesrv_lsa_AddRemoveAccountRights(dce_call, mem_ctx, astate->policy, 
						  LDB_FLAG_MOD_DELETE, astate->account_sid,
						  r2.out.rights);
	}

	if (r->in.remove_all != 0) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	rights->count = r->in.privs->count;
	rights->names = talloc_array(mem_ctx, struct lsa_StringLarge, rights->count);
	if (rights->names == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	for (i=0;i<rights->count;i++) {
		int id = r->in.privs->set[i].luid.low;
		if (r->in.privs->set[i].luid.high) {
			return NT_STATUS_NO_SUCH_PRIVILEGE;
		}
		rights->names[i].string = sec_privilege_name(id);
		if (rights->names[i].string == NULL) {
			return NT_STATUS_NO_SUCH_PRIVILEGE;
		}
	}

	return dcesrv_lsa_AddRemoveAccountRights(dce_call, mem_ctx, astate->policy, 
					  LDB_FLAG_MOD_DELETE, astate->account_sid,
					  rights);
}


/* 
  lsa_GetQuotasForAccount
*/
static NTSTATUS dcesrv_lsa_GetQuotasForAccount(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct lsa_GetQuotasForAccount *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  lsa_SetQuotasForAccount
*/
static NTSTATUS dcesrv_lsa_SetQuotasForAccount(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct lsa_SetQuotasForAccount *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  lsa_GetSystemAccessAccount
*/
static NTSTATUS dcesrv_lsa_GetSystemAccessAccount(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct lsa_GetSystemAccessAccount *r)
{
	int i;
	NTSTATUS status;
	struct lsa_EnumPrivsAccount enumPrivs;
	struct lsa_PrivilegeSet *privs;

	privs = talloc(mem_ctx, struct lsa_PrivilegeSet);
	if (!privs) {
		return NT_STATUS_NO_MEMORY;
	}
	privs->count = 0;
	privs->unknown = 0;
	privs->set = NULL;

	enumPrivs.in.handle = r->in.handle;
	enumPrivs.out.privs = &privs;

	status = dcesrv_lsa_EnumPrivsAccount(dce_call, mem_ctx, &enumPrivs);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}	

	*(r->out.access_mask) = 0x00000000;

	for (i = 0; i < privs->count; i++) {
		int priv = privs->set[i].luid.low;

		switch (priv) {
		case SEC_PRIV_INTERACTIVE_LOGON:
			*(r->out.access_mask) |= LSA_POLICY_MODE_INTERACTIVE;
			break;
		case SEC_PRIV_NETWORK_LOGON:
			*(r->out.access_mask) |= LSA_POLICY_MODE_NETWORK;
			break;
		case SEC_PRIV_REMOTE_INTERACTIVE_LOGON:
			*(r->out.access_mask) |= LSA_POLICY_MODE_REMOTE_INTERACTIVE;
			break;
		}
	}

	return NT_STATUS_OK;
}


/* 
  lsa_SetSystemAccessAccount
*/
static NTSTATUS dcesrv_lsa_SetSystemAccessAccount(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct lsa_SetSystemAccessAccount *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  lsa_CreateSecret 
*/
static NTSTATUS dcesrv_lsa_CreateSecret(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				 struct lsa_CreateSecret *r)
{
	struct dcesrv_handle *policy_handle;
	struct lsa_policy_state *policy_state;
	struct lsa_secret_state *secret_state;
	struct dcesrv_handle *handle;
	struct ldb_message **msgs, *msg;
	const char *attrs[] = {
		NULL
	};

	const char *name;

	int ret;

	DCESRV_PULL_HANDLE(policy_handle, r->in.handle, LSA_HANDLE_POLICY);
	ZERO_STRUCTP(r->out.sec_handle);
	
	switch (security_session_user_level(dce_call->conn->auth_state.session_info))
	{
	case SECURITY_SYSTEM:
	case SECURITY_ADMINISTRATOR:
		break;
	default:
		/* Users and annonymous are not allowed create secrets */
		return NT_STATUS_ACCESS_DENIED;
	}

	policy_state = policy_handle->data;

	if (!r->in.name.string) {
		return NT_STATUS_INVALID_PARAMETER;
	}
	
	secret_state = talloc(mem_ctx, struct lsa_secret_state);
	if (!secret_state) {
		return NT_STATUS_NO_MEMORY;
	}
	secret_state->policy = policy_state;

	msg = ldb_msg_new(mem_ctx);
	if (msg == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	if (strncmp("G$", r->in.name.string, 2) == 0) {
		const char *name2;
		name = &r->in.name.string[2];
			/* We need to connect to the database as system, as this is one of the rare RPC calls that must read the secrets (and this is denied otherwise) */
		secret_state->sam_ldb = talloc_reference(secret_state, 
							 samdb_connect(mem_ctx, dce_call->event_ctx, dce_call->conn->dce_ctx->lp_ctx, system_session(secret_state, dce_call->conn->dce_ctx->lp_ctx))); 
		secret_state->global = true;

		if (strlen(name) < 1) {
			return NT_STATUS_INVALID_PARAMETER;
		}

		name2 = talloc_asprintf(mem_ctx, "%s Secret", ldb_binary_encode_string(mem_ctx, name));
		/* search for the secret record */
		ret = gendb_search(secret_state->sam_ldb,
				   mem_ctx, policy_state->system_dn, &msgs, attrs,
				   "(&(cn=%s)(objectclass=secret))", 
				   name2);
		if (ret > 0) {
			return NT_STATUS_OBJECT_NAME_COLLISION;
		}
		
		if (ret == -1) {
			DEBUG(0,("Failure searching for CN=%s: %s\n", 
				 name2, ldb_errstring(secret_state->sam_ldb)));
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		msg->dn = ldb_dn_copy(mem_ctx, policy_state->system_dn);
		if (!name2 || ! ldb_dn_add_child_fmt(msg->dn, "cn=%s", name2)) {
			return NT_STATUS_NO_MEMORY;
		}
		
		samdb_msg_add_string(secret_state->sam_ldb, mem_ctx, msg, "cn", name2);
	
	} else {
		secret_state->global = false;

		name = r->in.name.string;
		if (strlen(name) < 1) {
			return NT_STATUS_INVALID_PARAMETER;
		}

		secret_state->sam_ldb = talloc_reference(secret_state, 
							 secrets_db_connect(mem_ctx, dce_call->event_ctx, dce_call->conn->dce_ctx->lp_ctx));
		/* search for the secret record */
		ret = gendb_search(secret_state->sam_ldb, mem_ctx,
				   ldb_dn_new(mem_ctx, secret_state->sam_ldb, "cn=LSA Secrets"),
				   &msgs, attrs,
				   "(&(cn=%s)(objectclass=secret))", 
				   ldb_binary_encode_string(mem_ctx, name));
		if (ret > 0) {
			return NT_STATUS_OBJECT_NAME_COLLISION;
		}
		
		if (ret == -1) {
			DEBUG(0,("Failure searching for CN=%s: %s\n", 
				 name, ldb_errstring(secret_state->sam_ldb)));
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		msg->dn = ldb_dn_new_fmt(mem_ctx, secret_state->sam_ldb, "cn=%s,cn=LSA Secrets", name);
		samdb_msg_add_string(secret_state->sam_ldb, mem_ctx, msg, "cn", name);
	} 

	samdb_msg_add_string(secret_state->sam_ldb, mem_ctx, msg, "objectClass", "secret");
	
	secret_state->secret_dn = talloc_reference(secret_state, msg->dn);

	/* create the secret */
	ret = ldb_add(secret_state->sam_ldb, msg);
	if (ret != 0) {
		DEBUG(0,("Failed to create secret record %s: %s\n",
			 ldb_dn_get_linearized(msg->dn), 
			 ldb_errstring(secret_state->sam_ldb)));
		return NT_STATUS_ACCESS_DENIED;
	}

	handle = dcesrv_handle_new(dce_call->context, LSA_HANDLE_SECRET);
	if (!handle) {
		return NT_STATUS_NO_MEMORY;
	}
	
	handle->data = talloc_steal(handle, secret_state);
	
	secret_state->access_mask = r->in.access_mask;
	secret_state->policy = talloc_reference(secret_state, policy_state);
	
	*r->out.sec_handle = handle->wire_handle;
	
	return NT_STATUS_OK;
}


/* 
  lsa_OpenSecret 
*/
static NTSTATUS dcesrv_lsa_OpenSecret(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
			       struct lsa_OpenSecret *r)
{
	struct dcesrv_handle *policy_handle;
	
	struct lsa_policy_state *policy_state;
	struct lsa_secret_state *secret_state;
	struct dcesrv_handle *handle;
	struct ldb_message **msgs;
	const char *attrs[] = {
		NULL
	};

	const char *name;

	int ret;

	DCESRV_PULL_HANDLE(policy_handle, r->in.handle, LSA_HANDLE_POLICY);
	ZERO_STRUCTP(r->out.sec_handle);
	policy_state = policy_handle->data;

	if (!r->in.name.string) {
		return NT_STATUS_INVALID_PARAMETER;
	}
	
	switch (security_session_user_level(dce_call->conn->auth_state.session_info))
	{
	case SECURITY_SYSTEM:
	case SECURITY_ADMINISTRATOR:
		break;
	default:
		/* Users and annonymous are not allowed to access secrets */
		return NT_STATUS_ACCESS_DENIED;
	}

	secret_state = talloc(mem_ctx, struct lsa_secret_state);
	if (!secret_state) {
		return NT_STATUS_NO_MEMORY;
	}
	secret_state->policy = policy_state;

	if (strncmp("G$", r->in.name.string, 2) == 0) {
		name = &r->in.name.string[2];
		/* We need to connect to the database as system, as this is one of the rare RPC calls that must read the secrets (and this is denied otherwise) */
		secret_state->sam_ldb = talloc_reference(secret_state, 
							 samdb_connect(mem_ctx, dce_call->event_ctx, dce_call->conn->dce_ctx->lp_ctx, system_session(secret_state, dce_call->conn->dce_ctx->lp_ctx))); 
		secret_state->global = true;

		if (strlen(name) < 1) {
			return NT_STATUS_INVALID_PARAMETER;
		}

		/* search for the secret record */
		ret = gendb_search(secret_state->sam_ldb,
				   mem_ctx, policy_state->system_dn, &msgs, attrs,
				   "(&(cn=%s Secret)(objectclass=secret))", 
				   ldb_binary_encode_string(mem_ctx, name));
		if (ret == 0) {
			return NT_STATUS_OBJECT_NAME_NOT_FOUND;
		}
		
		if (ret != 1) {
			DEBUG(0,("Found %d records matching DN %s\n", ret,
				 ldb_dn_get_linearized(policy_state->system_dn)));
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}
	
	} else {
		secret_state->global = false;
		secret_state->sam_ldb = talloc_reference(secret_state, 
				 secrets_db_connect(mem_ctx, dce_call->event_ctx, dce_call->conn->dce_ctx->lp_ctx));

		name = r->in.name.string;
		if (strlen(name) < 1) {
			return NT_STATUS_INVALID_PARAMETER;
		}

		/* search for the secret record */
		ret = gendb_search(secret_state->sam_ldb, mem_ctx,
				   ldb_dn_new(mem_ctx, secret_state->sam_ldb, "cn=LSA Secrets"),
				   &msgs, attrs,
				   "(&(cn=%s)(objectclass=secret))", 
				   ldb_binary_encode_string(mem_ctx, name));
		if (ret == 0) {
			return NT_STATUS_OBJECT_NAME_NOT_FOUND;
		}
		
		if (ret != 1) {
			DEBUG(0,("Found %d records matching CN=%s\n", 
				 ret, ldb_binary_encode_string(mem_ctx, name)));
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}
	} 

	secret_state->secret_dn = talloc_reference(secret_state, msgs[0]->dn);
	
	handle = dcesrv_handle_new(dce_call->context, LSA_HANDLE_SECRET);
	if (!handle) {
		return NT_STATUS_NO_MEMORY;
	}
	
	handle->data = talloc_steal(handle, secret_state);
	
	secret_state->access_mask = r->in.access_mask;
	secret_state->policy = talloc_reference(secret_state, policy_state);
	
	*r->out.sec_handle = handle->wire_handle;
	
	return NT_STATUS_OK;
}


/* 
  lsa_SetSecret 
*/
static NTSTATUS dcesrv_lsa_SetSecret(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
			      struct lsa_SetSecret *r)
{

	struct dcesrv_handle *h;
	struct lsa_secret_state *secret_state;
	struct ldb_message *msg;
	DATA_BLOB session_key;
	DATA_BLOB crypt_secret, secret;
	struct ldb_val val;
	int ret;
	NTSTATUS status = NT_STATUS_OK;

	struct timeval now = timeval_current();
	NTTIME nt_now = timeval_to_nttime(&now);

	DCESRV_PULL_HANDLE(h, r->in.sec_handle, LSA_HANDLE_SECRET);

	secret_state = h->data;

	msg = ldb_msg_new(mem_ctx);
	if (msg == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	msg->dn = talloc_reference(mem_ctx, secret_state->secret_dn);
	if (!msg->dn) {
		return NT_STATUS_NO_MEMORY;
	}
	status = dcesrv_fetch_session_key(dce_call->conn, &session_key);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (r->in.old_val) {
		/* Decrypt */
		crypt_secret.data = r->in.old_val->data;
		crypt_secret.length = r->in.old_val->size;
		
		status = sess_decrypt_blob(mem_ctx, &crypt_secret, &session_key, &secret);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
		
		val.data = secret.data;
		val.length = secret.length;
		
		/* set value */
		if (samdb_msg_add_value(secret_state->sam_ldb, 
					mem_ctx, msg, "priorValue", &val) != 0) {
			return NT_STATUS_NO_MEMORY; 
		}
		
		/* set old value mtime */
		if (samdb_msg_add_uint64(secret_state->sam_ldb, 
					 mem_ctx, msg, "priorSetTime", nt_now) != 0) { 
			return NT_STATUS_NO_MEMORY; 
		}

	} else {
		/* If the old value is not set, then migrate the
		 * current value to the old value */
		const struct ldb_val *old_val;
		NTTIME last_set_time;
		struct ldb_message **res;
		const char *attrs[] = {
			"currentValue",
			"lastSetTime",
			NULL
		};
		
		/* search for the secret record */
		ret = gendb_search_dn(secret_state->sam_ldb,mem_ctx,
				      secret_state->secret_dn, &res, attrs);
		if (ret == 0) {
			return NT_STATUS_OBJECT_NAME_NOT_FOUND;
		}
		
		if (ret != 1) {
			DEBUG(0,("Found %d records matching dn=%s\n", ret,
				 ldb_dn_get_linearized(secret_state->secret_dn)));
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}
		
		old_val = ldb_msg_find_ldb_val(res[0], "currentValue");
		last_set_time = ldb_msg_find_attr_as_uint64(res[0], "lastSetTime", 0);
		
		if (old_val) {
			/* set old value */
			if (samdb_msg_add_value(secret_state->sam_ldb, 
						mem_ctx, msg, "priorValue", 
						old_val) != 0) {
				return NT_STATUS_NO_MEMORY; 
			}
		} else {
			if (samdb_msg_add_delete(secret_state->sam_ldb, 
						 mem_ctx, msg, "priorValue")) {
				return NT_STATUS_NO_MEMORY;
			}
			
		}
		
		/* set old value mtime */
		if (ldb_msg_find_ldb_val(res[0], "lastSetTime")) {
			if (samdb_msg_add_uint64(secret_state->sam_ldb, 
						 mem_ctx, msg, "priorSetTime", last_set_time) != 0) { 
				return NT_STATUS_NO_MEMORY; 
			}
		} else {
			if (samdb_msg_add_uint64(secret_state->sam_ldb, 
						 mem_ctx, msg, "priorSetTime", nt_now) != 0) { 
				return NT_STATUS_NO_MEMORY; 
			}
		}
	}

	if (r->in.new_val) {
		/* Decrypt */
		crypt_secret.data = r->in.new_val->data;
		crypt_secret.length = r->in.new_val->size;
		
		status = sess_decrypt_blob(mem_ctx, &crypt_secret, &session_key, &secret);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
		
		val.data = secret.data;
		val.length = secret.length;
		
		/* set value */
		if (samdb_msg_add_value(secret_state->sam_ldb, 
					mem_ctx, msg, "currentValue", &val) != 0) {
			return NT_STATUS_NO_MEMORY; 
		}
		
		/* set new value mtime */
		if (samdb_msg_add_uint64(secret_state->sam_ldb, 
					 mem_ctx, msg, "lastSetTime", nt_now) != 0) { 
			return NT_STATUS_NO_MEMORY; 
		}
		
	} else {
		/* NULL out the NEW value */
		if (samdb_msg_add_uint64(secret_state->sam_ldb, 
					 mem_ctx, msg, "lastSetTime", nt_now) != 0) { 
			return NT_STATUS_NO_MEMORY; 
		}
		if (samdb_msg_add_delete(secret_state->sam_ldb, 
					 mem_ctx, msg, "currentValue")) {
			return NT_STATUS_NO_MEMORY;
		}
	}

	/* modify the samdb record */
	ret = samdb_replace(secret_state->sam_ldb, mem_ctx, msg);
	if (ret != 0) {
		/* we really need samdb.c to return NTSTATUS */
		return NT_STATUS_UNSUCCESSFUL;
	}

	return NT_STATUS_OK;
}


/* 
  lsa_QuerySecret 
*/
static NTSTATUS dcesrv_lsa_QuerySecret(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				struct lsa_QuerySecret *r)
{
	struct dcesrv_handle *h;
	struct lsa_secret_state *secret_state;
	struct ldb_message *msg;
	DATA_BLOB session_key;
	DATA_BLOB crypt_secret, secret;
	int ret;
	struct ldb_message **res;
	const char *attrs[] = {
		"currentValue",
		"priorValue",
		"lastSetTime",
		"priorSetTime", 
		NULL
	};

	NTSTATUS nt_status;

	DCESRV_PULL_HANDLE(h, r->in.sec_handle, LSA_HANDLE_SECRET);

	/* Ensure user is permitted to read this... */
	switch (security_session_user_level(dce_call->conn->auth_state.session_info))
	{
	case SECURITY_SYSTEM:
	case SECURITY_ADMINISTRATOR:
		break;
	default:
		/* Users and annonymous are not allowed to read secrets */
		return NT_STATUS_ACCESS_DENIED;
	}

	secret_state = h->data;

	/* pull all the user attributes */
	ret = gendb_search_dn(secret_state->sam_ldb, mem_ctx,
			      secret_state->secret_dn, &res, attrs);
	if (ret != 1) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}
	msg = res[0];
	
	nt_status = dcesrv_fetch_session_key(dce_call->conn, &session_key);
	if (!NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	}
	
	if (r->in.old_val) {
		const struct ldb_val *prior_val;
		r->out.old_val = talloc_zero(mem_ctx, struct lsa_DATA_BUF_PTR);
		if (!r->out.old_val) {
			return NT_STATUS_NO_MEMORY;
		}
		prior_val = ldb_msg_find_ldb_val(res[0], "priorValue");
		
		if (prior_val && prior_val->length) {
			secret.data = prior_val->data;
			secret.length = prior_val->length;
		
			/* Encrypt */
			crypt_secret = sess_encrypt_blob(mem_ctx, &secret, &session_key);
			if (!crypt_secret.length) {
				return NT_STATUS_NO_MEMORY;
			}
			r->out.old_val->buf = talloc(mem_ctx, struct lsa_DATA_BUF);
			if (!r->out.old_val->buf) {
				return NT_STATUS_NO_MEMORY;
			}
			r->out.old_val->buf->size = crypt_secret.length;
			r->out.old_val->buf->length = crypt_secret.length;
			r->out.old_val->buf->data = crypt_secret.data;
		}
	}
	
	if (r->in.old_mtime) {
		r->out.old_mtime = talloc(mem_ctx, NTTIME);
		if (!r->out.old_mtime) {
			return NT_STATUS_NO_MEMORY;
		}
		*r->out.old_mtime = ldb_msg_find_attr_as_uint64(res[0], "priorSetTime", 0);
	}
	
	if (r->in.new_val) {
		const struct ldb_val *new_val;
		r->out.new_val = talloc_zero(mem_ctx, struct lsa_DATA_BUF_PTR);
		if (!r->out.new_val) {
			return NT_STATUS_NO_MEMORY;
		}

		new_val = ldb_msg_find_ldb_val(res[0], "currentValue");
		
		if (new_val && new_val->length) {
			secret.data = new_val->data;
			secret.length = new_val->length;
		
			/* Encrypt */
			crypt_secret = sess_encrypt_blob(mem_ctx, &secret, &session_key);
			if (!crypt_secret.length) {
				return NT_STATUS_NO_MEMORY;
			}
			r->out.new_val->buf = talloc(mem_ctx, struct lsa_DATA_BUF);
			if (!r->out.new_val->buf) {
				return NT_STATUS_NO_MEMORY;
			}
			r->out.new_val->buf->length = crypt_secret.length;
			r->out.new_val->buf->size = crypt_secret.length;
			r->out.new_val->buf->data = crypt_secret.data;
		}
	}
	
	if (r->in.new_mtime) {
		r->out.new_mtime = talloc(mem_ctx, NTTIME);
		if (!r->out.new_mtime) {
			return NT_STATUS_NO_MEMORY;
		}
		*r->out.new_mtime = ldb_msg_find_attr_as_uint64(res[0], "lastSetTime", 0);
	}
	
	return NT_STATUS_OK;
}


/* 
  lsa_LookupPrivValue
*/
static NTSTATUS dcesrv_lsa_LookupPrivValue(struct dcesrv_call_state *dce_call, 
				    TALLOC_CTX *mem_ctx,
				    struct lsa_LookupPrivValue *r)
{
	struct dcesrv_handle *h;
	struct lsa_policy_state *state;
	int id;

	DCESRV_PULL_HANDLE(h, r->in.handle, LSA_HANDLE_POLICY);

	state = h->data;

	id = sec_privilege_id(r->in.name->string);
	if (id == -1) {
		return NT_STATUS_NO_SUCH_PRIVILEGE;
	}

	r->out.luid->low = id;
	r->out.luid->high = 0;

	return NT_STATUS_OK;	
}


/* 
  lsa_LookupPrivName 
*/
static NTSTATUS dcesrv_lsa_LookupPrivName(struct dcesrv_call_state *dce_call, 
				   TALLOC_CTX *mem_ctx,
				   struct lsa_LookupPrivName *r)
{
	struct dcesrv_handle *h;
	struct lsa_policy_state *state;
	struct lsa_StringLarge *name;
	const char *privname;

	DCESRV_PULL_HANDLE(h, r->in.handle, LSA_HANDLE_POLICY);

	state = h->data;

	if (r->in.luid->high != 0) {
		return NT_STATUS_NO_SUCH_PRIVILEGE;
	}

	privname = sec_privilege_name(r->in.luid->low);
	if (privname == NULL) {
		return NT_STATUS_NO_SUCH_PRIVILEGE;
	}

	name = talloc(mem_ctx, struct lsa_StringLarge);
	if (name == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	name->string = privname;

	*r->out.name = name;

	return NT_STATUS_OK;	
}


/* 
  lsa_LookupPrivDisplayName
*/
static NTSTATUS dcesrv_lsa_LookupPrivDisplayName(struct dcesrv_call_state *dce_call, 
					  TALLOC_CTX *mem_ctx,
					  struct lsa_LookupPrivDisplayName *r)
{
	struct dcesrv_handle *h;
	struct lsa_policy_state *state;
	struct lsa_StringLarge *disp_name = NULL;
	int id;

	DCESRV_PULL_HANDLE(h, r->in.handle, LSA_HANDLE_POLICY);

	state = h->data;

	id = sec_privilege_id(r->in.name->string);
	if (id == -1) {
		return NT_STATUS_NO_SUCH_PRIVILEGE;
	}

	disp_name = talloc(mem_ctx, struct lsa_StringLarge);
	if (disp_name == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	disp_name->string = sec_privilege_display_name(id, &r->in.language_id);
	if (disp_name->string == NULL) {
		return NT_STATUS_INTERNAL_ERROR;
	}

	*r->out.disp_name = disp_name;
	*r->out.returned_language_id = 0;

	return NT_STATUS_OK;
}


/* 
  lsa_EnumAccountsWithUserRight
*/
static NTSTATUS dcesrv_lsa_EnumAccountsWithUserRight(struct dcesrv_call_state *dce_call, 
					      TALLOC_CTX *mem_ctx,
					      struct lsa_EnumAccountsWithUserRight *r)
{
	struct dcesrv_handle *h;
	struct lsa_policy_state *state;
	int ret, i;
	struct ldb_message **res;
	const char * const attrs[] = { "objectSid", NULL};
	const char *privname;

	DCESRV_PULL_HANDLE(h, r->in.handle, LSA_HANDLE_POLICY);

	state = h->data;

	if (r->in.name == NULL) {
		return NT_STATUS_NO_SUCH_PRIVILEGE;
	} 

	privname = r->in.name->string;
	if (sec_privilege_id(privname) == -1) {
		return NT_STATUS_NO_SUCH_PRIVILEGE;
	}

	ret = gendb_search(state->sam_ldb, mem_ctx, NULL, &res, attrs, 
			   "privilege=%s", privname);
	if (ret == -1) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}
	if (ret == 0) {
		return NT_STATUS_NO_MORE_ENTRIES;
	}

	r->out.sids->sids = talloc_array(r->out.sids, struct lsa_SidPtr, ret);
	if (r->out.sids->sids == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	for (i=0;i<ret;i++) {
		r->out.sids->sids[i].sid = samdb_result_dom_sid(r->out.sids->sids,
								res[i], "objectSid");
		NT_STATUS_HAVE_NO_MEMORY(r->out.sids->sids[i].sid);
	}
	r->out.sids->num_sids = ret;

	return NT_STATUS_OK;
}


/* 
  lsa_AddAccountRights
*/
static NTSTATUS dcesrv_lsa_AddAccountRights(struct dcesrv_call_state *dce_call, 
				     TALLOC_CTX *mem_ctx,
				     struct lsa_AddAccountRights *r)
{
	struct dcesrv_handle *h;
	struct lsa_policy_state *state;

	DCESRV_PULL_HANDLE(h, r->in.handle, LSA_HANDLE_POLICY);

	state = h->data;

	return dcesrv_lsa_AddRemoveAccountRights(dce_call, mem_ctx, state, 
					  LDB_FLAG_MOD_ADD,
					  r->in.sid, r->in.rights);
}


/* 
  lsa_RemoveAccountRights
*/
static NTSTATUS dcesrv_lsa_RemoveAccountRights(struct dcesrv_call_state *dce_call, 
					TALLOC_CTX *mem_ctx,
					struct lsa_RemoveAccountRights *r)
{
	struct dcesrv_handle *h;
	struct lsa_policy_state *state;

	DCESRV_PULL_HANDLE(h, r->in.handle, LSA_HANDLE_POLICY);

	state = h->data;

	return dcesrv_lsa_AddRemoveAccountRights(dce_call, mem_ctx, state, 
					  LDB_FLAG_MOD_DELETE,
					  r->in.sid, r->in.rights);
}


/* 
  lsa_StorePrivateData
*/
static NTSTATUS dcesrv_lsa_StorePrivateData(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct lsa_StorePrivateData *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  lsa_RetrievePrivateData
*/
static NTSTATUS dcesrv_lsa_RetrievePrivateData(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct lsa_RetrievePrivateData *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  lsa_GetUserName
*/
static NTSTATUS dcesrv_lsa_GetUserName(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				struct lsa_GetUserName *r)
{
	NTSTATUS status = NT_STATUS_OK;
	const char *account_name;
	const char *authority_name;
	struct lsa_String *_account_name;
	struct lsa_String *_authority_name = NULL;

	/* this is what w2k3 does */
	r->out.account_name = r->in.account_name;
	r->out.authority_name = r->in.authority_name;

	if (r->in.account_name
	    && *r->in.account_name
	    /* && *(*r->in.account_name)->string */
	    ) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (r->in.authority_name
	    && *r->in.authority_name
	    /* && *(*r->in.authority_name)->string */
	    ) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	account_name = talloc_reference(mem_ctx, dce_call->conn->auth_state.session_info->server_info->account_name);
	authority_name = talloc_reference(mem_ctx, dce_call->conn->auth_state.session_info->server_info->domain_name);

	_account_name = talloc(mem_ctx, struct lsa_String);
	NT_STATUS_HAVE_NO_MEMORY(_account_name);
	_account_name->string = account_name;

	if (r->in.authority_name) {
		_authority_name = talloc(mem_ctx, struct lsa_String);
		NT_STATUS_HAVE_NO_MEMORY(_authority_name);
		_authority_name->string = authority_name;
	}

	*r->out.account_name = _account_name;
	if (r->out.authority_name) {
		*r->out.authority_name = _authority_name;
	}

	return status;
}

/*
  lsa_SetInfoPolicy2
*/
static NTSTATUS dcesrv_lsa_SetInfoPolicy2(struct dcesrv_call_state *dce_call,
				   TALLOC_CTX *mem_ctx,
				   struct lsa_SetInfoPolicy2 *r)
{
	/* need to support these */
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}

/*
  lsa_QueryDomainInformationPolicy
*/
static NTSTATUS dcesrv_lsa_QueryDomainInformationPolicy(struct dcesrv_call_state *dce_call,
						 TALLOC_CTX *mem_ctx,
						 struct lsa_QueryDomainInformationPolicy *r)
{
	union lsa_DomainInformationPolicy *info;

	info = talloc(r->out.info, union lsa_DomainInformationPolicy);
	if (!info) {
		return NT_STATUS_NO_MEMORY;
	}

	switch (r->in.level) {
	case LSA_DOMAIN_INFO_POLICY_EFS:
		talloc_free(info);
		*r->out.info = NULL;
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	case LSA_DOMAIN_INFO_POLICY_KERBEROS:
	{
		struct lsa_DomainInfoKerberos *k = &info->kerberos_info;
		struct smb_krb5_context *smb_krb5_context;
		int ret = smb_krb5_init_context(mem_ctx, 
							dce_call->event_ctx, 
							dce_call->conn->dce_ctx->lp_ctx,
							&smb_krb5_context);
		if (ret != 0) {
			talloc_free(info);
			*r->out.info = NULL;
			return NT_STATUS_INTERNAL_ERROR;
		}
		k->enforce_restrictions = 0; /* FIXME, details missing from MS-LSAD 2.2.53 */
		k->service_tkt_lifetime = 0; /* Need to find somewhere to store this, and query in KDC too */
		k->user_tkt_lifetime = 0;    /* Need to find somewhere to store this, and query in KDC too */
		k->user_tkt_renewaltime = 0; /* Need to find somewhere to store this, and query in KDC too */
		k->clock_skew = krb5_get_max_time_skew(smb_krb5_context->krb5_context);
		talloc_free(smb_krb5_context);
		*r->out.info = info;
		return NT_STATUS_OK;
	}
	default:
		talloc_free(info);
		*r->out.info = NULL;
		return NT_STATUS_INVALID_INFO_CLASS;
	}
}

/*
  lsa_SetDomInfoPolicy
*/
static NTSTATUS dcesrv_lsa_SetDomainInformationPolicy(struct dcesrv_call_state *dce_call,
					      TALLOC_CTX *mem_ctx,
					      struct lsa_SetDomainInformationPolicy *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}

/*
  lsa_TestCall
*/
static NTSTATUS dcesrv_lsa_TestCall(struct dcesrv_call_state *dce_call,
			     TALLOC_CTX *mem_ctx,
			     struct lsa_TestCall *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}

/* 
  lsa_CREDRWRITE 
*/
static NTSTATUS dcesrv_lsa_CREDRWRITE(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct lsa_CREDRWRITE *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  lsa_CREDRREAD 
*/
static NTSTATUS dcesrv_lsa_CREDRREAD(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct lsa_CREDRREAD *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  lsa_CREDRENUMERATE 
*/
static NTSTATUS dcesrv_lsa_CREDRENUMERATE(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct lsa_CREDRENUMERATE *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  lsa_CREDRWRITEDOMAINCREDENTIALS 
*/
static NTSTATUS dcesrv_lsa_CREDRWRITEDOMAINCREDENTIALS(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct lsa_CREDRWRITEDOMAINCREDENTIALS *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  lsa_CREDRREADDOMAINCREDENTIALS 
*/
static NTSTATUS dcesrv_lsa_CREDRREADDOMAINCREDENTIALS(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct lsa_CREDRREADDOMAINCREDENTIALS *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  lsa_CREDRDELETE 
*/
static NTSTATUS dcesrv_lsa_CREDRDELETE(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct lsa_CREDRDELETE *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  lsa_CREDRGETTARGETINFO 
*/
static NTSTATUS dcesrv_lsa_CREDRGETTARGETINFO(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct lsa_CREDRGETTARGETINFO *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  lsa_CREDRPROFILELOADED 
*/
static NTSTATUS dcesrv_lsa_CREDRPROFILELOADED(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct lsa_CREDRPROFILELOADED *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  lsa_CREDRGETSESSIONTYPES 
*/
static NTSTATUS dcesrv_lsa_CREDRGETSESSIONTYPES(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct lsa_CREDRGETSESSIONTYPES *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  lsa_LSARREGISTERAUDITEVENT 
*/
static NTSTATUS dcesrv_lsa_LSARREGISTERAUDITEVENT(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct lsa_LSARREGISTERAUDITEVENT *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  lsa_LSARGENAUDITEVENT 
*/
static NTSTATUS dcesrv_lsa_LSARGENAUDITEVENT(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct lsa_LSARGENAUDITEVENT *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  lsa_LSARUNREGISTERAUDITEVENT 
*/
static NTSTATUS dcesrv_lsa_LSARUNREGISTERAUDITEVENT(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct lsa_LSARUNREGISTERAUDITEVENT *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  lsa_lsaRQueryForestTrustInformation 
*/
static NTSTATUS dcesrv_lsa_lsaRQueryForestTrustInformation(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct lsa_lsaRQueryForestTrustInformation *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  lsa_LSARSETFORESTTRUSTINFORMATION 
*/
static NTSTATUS dcesrv_lsa_LSARSETFORESTTRUSTINFORMATION(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct lsa_LSARSETFORESTTRUSTINFORMATION *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  lsa_CREDRRENAME 
*/
static NTSTATUS dcesrv_lsa_CREDRRENAME(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct lsa_CREDRRENAME *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}



/* 
  lsa_LSAROPENPOLICYSCE 
*/
static NTSTATUS dcesrv_lsa_LSAROPENPOLICYSCE(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct lsa_LSAROPENPOLICYSCE *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  lsa_LSARADTREGISTERSECURITYEVENTSOURCE 
*/
static NTSTATUS dcesrv_lsa_LSARADTREGISTERSECURITYEVENTSOURCE(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct lsa_LSARADTREGISTERSECURITYEVENTSOURCE *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  lsa_LSARADTUNREGISTERSECURITYEVENTSOURCE 
*/
static NTSTATUS dcesrv_lsa_LSARADTUNREGISTERSECURITYEVENTSOURCE(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct lsa_LSARADTUNREGISTERSECURITYEVENTSOURCE *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  lsa_LSARADTREPORTSECURITYEVENT 
*/
static NTSTATUS dcesrv_lsa_LSARADTREPORTSECURITYEVENT(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct lsa_LSARADTREPORTSECURITYEVENT *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* include the generated boilerplate */
#include "librpc/gen_ndr/ndr_lsa_s.c"



/*****************************************
NOTE! The remaining calls below were
removed in w2k3, so the DCESRV_FAULT()
replies are the correct implementation. Do
not try and fill these in with anything else
******************************************/

/* 
  dssetup_DsRoleDnsNameToFlatName 
*/
static WERROR dcesrv_dssetup_DsRoleDnsNameToFlatName(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
					struct dssetup_DsRoleDnsNameToFlatName *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  dssetup_DsRoleDcAsDc 
*/
static WERROR dcesrv_dssetup_DsRoleDcAsDc(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
			     struct dssetup_DsRoleDcAsDc *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  dssetup_DsRoleDcAsReplica 
*/
static WERROR dcesrv_dssetup_DsRoleDcAsReplica(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				  struct dssetup_DsRoleDcAsReplica *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  dssetup_DsRoleDemoteDc 
*/
static WERROR dcesrv_dssetup_DsRoleDemoteDc(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
			       struct dssetup_DsRoleDemoteDc *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  dssetup_DsRoleGetDcOperationProgress 
*/
static WERROR dcesrv_dssetup_DsRoleGetDcOperationProgress(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
					     struct dssetup_DsRoleGetDcOperationProgress *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  dssetup_DsRoleGetDcOperationResults 
*/
static WERROR dcesrv_dssetup_DsRoleGetDcOperationResults(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
					    struct dssetup_DsRoleGetDcOperationResults *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  dssetup_DsRoleCancel 
*/
static WERROR dcesrv_dssetup_DsRoleCancel(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
			     struct dssetup_DsRoleCancel *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  dssetup_DsRoleServerSaveStateForUpgrade 
*/
static WERROR dcesrv_dssetup_DsRoleServerSaveStateForUpgrade(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
						struct dssetup_DsRoleServerSaveStateForUpgrade *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  dssetup_DsRoleUpgradeDownlevelServer 
*/
static WERROR dcesrv_dssetup_DsRoleUpgradeDownlevelServer(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
					     struct dssetup_DsRoleUpgradeDownlevelServer *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  dssetup_DsRoleAbortDownlevelServerUpgrade 
*/
static WERROR dcesrv_dssetup_DsRoleAbortDownlevelServerUpgrade(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
						  struct dssetup_DsRoleAbortDownlevelServerUpgrade *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* include the generated boilerplate */
#include "librpc/gen_ndr/ndr_dssetup_s.c"

NTSTATUS dcerpc_server_lsa_init(void)
{
	NTSTATUS ret;
	
	ret = dcerpc_server_dssetup_init();
	if (!NT_STATUS_IS_OK(ret)) {
		return ret;
	}
	ret = dcerpc_server_lsarpc_init();
	if (!NT_STATUS_IS_OK(ret)) {
		return ret;
	}
	return ret;
}
