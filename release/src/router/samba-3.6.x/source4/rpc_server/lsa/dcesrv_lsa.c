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
#include "lib/util/tsort.h"
#include "dsdb/common/util.h"
#include "libcli/security/session.h"
#include "kdc/kdc-policy.h"

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
	NT_STATUS_NOT_OK_RETURN_AND_FREE(status, tmp_ctx);

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
		switch (security_session_user_level(dce_call->conn->auth_state.session_info, NULL))
		{
		case SECURITY_SYSTEM:
		case SECURITY_ADMINISTRATOR:
			break;
		default:
			/* Users and anonymous are not allowed to delete things */
			return NT_STATUS_ACCESS_DENIED;
		}

		ret = ldb_delete(secret_state->sam_ldb, 
				 secret_state->secret_dn);
		if (ret != LDB_SUCCESS) {
			return NT_STATUS_INVALID_HANDLE;
		}

		ZERO_STRUCTP(r->out.handle);

		return NT_STATUS_OK;

	} else if (h->wire_handle.handle_type == LSA_HANDLE_TRUSTED_DOMAIN) {
		struct lsa_trusted_domain_state *trusted_domain_state = 
			talloc_get_type(h->data, struct lsa_trusted_domain_state);
		ret = ldb_transaction_start(trusted_domain_state->policy->sam_ldb);
		if (ret != LDB_SUCCESS) {
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		ret = ldb_delete(trusted_domain_state->policy->sam_ldb, 
				 trusted_domain_state->trusted_domain_dn);
		if (ret != LDB_SUCCESS) {
			ldb_transaction_cancel(trusted_domain_state->policy->sam_ldb);
			return NT_STATUS_INVALID_HANDLE;
		}

		if (trusted_domain_state->trusted_domain_user_dn) {
			ret = ldb_delete(trusted_domain_state->policy->sam_ldb, 
					 trusted_domain_state->trusted_domain_user_dn);
			if (ret != LDB_SUCCESS) {
				ldb_transaction_cancel(trusted_domain_state->policy->sam_ldb);
				return NT_STATUS_INVALID_HANDLE;
			}
		}

		ret = ldb_transaction_commit(trusted_domain_state->policy->sam_ldb);
		if (ret != LDB_SUCCESS) {
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

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

		return NT_STATUS_OK;
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
	uint32_t i;
	enum sec_privilege priv;
	const char *privname;

	DCESRV_PULL_HANDLE(h, r->in.handle, LSA_HANDLE_POLICY);

	state = h->data;

	i = *r->in.resume_handle;

	while (((priv = sec_privilege_from_index(i)) != SEC_PRIV_INVALID) &&
	       r->out.privs->count < r->in.max_count) {
		struct lsa_PrivEntry *e;
		privname = sec_privilege_name(priv);
		r->out.privs->privs = talloc_realloc(r->out.privs,
						       r->out.privs->privs, 
						       struct lsa_PrivEntry, 
						       r->out.privs->count+1);
		if (r->out.privs->privs == NULL) {
			return NT_STATUS_NO_MEMORY;
		}
		e = &r->out.privs->privs[r->out.privs->count];
		e->luid.low = priv;
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

	sid = &dce_call->conn->auth_state.session_info->security_token->sids[PRIMARY_USER_SID_INDEX];

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

	info = talloc_zero(mem_ctx, union dssetup_DsRoleInfo);
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

		switch (lpcfg_server_role(dce_call->conn->dce_ctx->lp_ctx)) {
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

		switch (lpcfg_server_role(dce_call->conn->dce_ctx->lp_ctx)) {
		case ROLE_STANDALONE:
			domain		= talloc_strdup(mem_ctx, lpcfg_workgroup(dce_call->conn->dce_ctx->lp_ctx));
			W_ERROR_HAVE_NO_MEMORY(domain);
			break;
		case ROLE_DOMAIN_MEMBER:
			domain		= talloc_strdup(mem_ctx, lpcfg_workgroup(dce_call->conn->dce_ctx->lp_ctx));
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
	int ret;
	struct ldb_message **res;
	const char * const attrs[] = { "objectSid", NULL};
	uint32_t count, i;

	DCESRV_PULL_HANDLE(h, r->in.handle, LSA_HANDLE_POLICY);

	state = h->data;

	/* NOTE: This call must only return accounts that have at least
	   one privilege set 
	*/
	ret = gendb_search(state->pdb, mem_ctx, NULL, &res, attrs, 
			   "(&(objectSid=*)(privilege=*))");
	if (ret < 0) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
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

/* This decrypts and returns Trusted Domain Auth Information Internal data */
static NTSTATUS get_trustdom_auth_blob(struct dcesrv_call_state *dce_call,
				       TALLOC_CTX *mem_ctx, DATA_BLOB *auth_blob,
				       struct trustDomainPasswords *auth_struct)
{
	DATA_BLOB session_key = data_blob(NULL, 0);
	enum ndr_err_code ndr_err;
	NTSTATUS nt_status;

	nt_status = dcesrv_fetch_session_key(dce_call->conn, &session_key);
	if (!NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	}

	arcfour_crypt_blob(auth_blob->data, auth_blob->length, &session_key);
	ndr_err = ndr_pull_struct_blob(auth_blob, mem_ctx,
				       auth_struct,
				       (ndr_pull_flags_fn_t)ndr_pull_trustDomainPasswords);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	return NT_STATUS_OK;
}

static NTSTATUS get_trustauth_inout_blob(struct dcesrv_call_state *dce_call,
					 TALLOC_CTX *mem_ctx,
					 struct trustAuthInOutBlob *iopw,
					 DATA_BLOB *trustauth_blob)
{
	enum ndr_err_code ndr_err;

	ndr_err = ndr_push_struct_blob(trustauth_blob, mem_ctx,
				       iopw,
				       (ndr_push_flags_fn_t)ndr_push_trustAuthInOutBlob);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	return NT_STATUS_OK;
}

static NTSTATUS add_trust_user(TALLOC_CTX *mem_ctx,
			       struct ldb_context *sam_ldb,
			       struct ldb_dn *base_dn,
			       const char *netbios_name,
			       struct trustAuthInOutBlob *in,
			       struct ldb_dn **user_dn)
{
	struct ldb_message *msg;
	struct ldb_dn *dn;
	uint32_t i;
	int ret;

	dn = ldb_dn_copy(mem_ctx, base_dn);
	if (!dn) {
		return NT_STATUS_NO_MEMORY;
	}
	if (!ldb_dn_add_child_fmt(dn, "cn=%s$,cn=users", netbios_name)) {
		return NT_STATUS_NO_MEMORY;
	}

	msg = ldb_msg_new(mem_ctx);
	if (!msg) {
		return NT_STATUS_NO_MEMORY;
	}
	msg->dn = dn;

	ret = ldb_msg_add_string(msg, "objectClass", "user");
	if (ret != LDB_SUCCESS) {
		return NT_STATUS_NO_MEMORY;
	}

	ret = ldb_msg_add_fmt(msg, "samAccountName", "%s$", netbios_name);
	if (ret != LDB_SUCCESS) {
		return NT_STATUS_NO_MEMORY;
	}

	ret = samdb_msg_add_uint(sam_ldb, msg, msg, "userAccountControl",
				 UF_INTERDOMAIN_TRUST_ACCOUNT);
	if (ret != LDB_SUCCESS) {
		return NT_STATUS_NO_MEMORY;
	}

	for (i = 0; i < in->count; i++) {
		const char *attribute;
		struct ldb_val v;
		switch (in->current.array[i].AuthType) {
		case TRUST_AUTH_TYPE_NT4OWF:
			attribute = "unicodePwd";
			v.data = (uint8_t *)&in->current.array[i].AuthInfo.nt4owf.password;
			v.length = 16;
			break;
		case TRUST_AUTH_TYPE_CLEAR:
			attribute = "clearTextPassword";
			v.data = in->current.array[i].AuthInfo.clear.password;
			v.length = in->current.array[i].AuthInfo.clear.size;
			break;
		default:
			continue;
		}

		ret = ldb_msg_add_value(msg, attribute, &v, NULL);
		if (ret != LDB_SUCCESS) {
			return NT_STATUS_NO_MEMORY;
		}
	}

	/* create the trusted_domain user account */
	ret = ldb_add(sam_ldb, msg);
	if (ret != LDB_SUCCESS) {
		DEBUG(0,("Failed to create user record %s: %s\n",
			 ldb_dn_get_linearized(msg->dn),
			 ldb_errstring(sam_ldb)));

		switch (ret) {
		case LDB_ERR_ENTRY_ALREADY_EXISTS:
			return NT_STATUS_DOMAIN_EXISTS;
		case LDB_ERR_INSUFFICIENT_ACCESS_RIGHTS:
			return NT_STATUS_ACCESS_DENIED;
		default:
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}
	}

	if (user_dn) {
		*user_dn = dn;
	}
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
	struct ldb_message **msgs, *msg;
	const char *attrs[] = {
		NULL
	};
	const char *netbios_name;
	const char *dns_name;
	const char *name;
	DATA_BLOB trustAuthIncoming, trustAuthOutgoing, auth_blob;
	struct trustDomainPasswords auth_struct;
	int ret;
	NTSTATUS nt_status;
	struct ldb_context *sam_ldb;

	DCESRV_PULL_HANDLE(policy_handle, r->in.policy_handle, LSA_HANDLE_POLICY);
	ZERO_STRUCTP(r->out.trustdom_handle);

	policy_state = policy_handle->data;
	sam_ldb = policy_state->sam_ldb;

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
		return NT_STATUS_INVALID_PARAMETER;
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
		auth_blob = data_blob_const(r->in.auth_info->auth_blob.data,
					    r->in.auth_info->auth_blob.size);
		nt_status = get_trustdom_auth_blob(dce_call, mem_ctx,
						   &auth_blob, &auth_struct);
		if (!NT_STATUS_IS_OK(nt_status)) {
			return nt_status;
		}

		if (op == NDR_LSA_CREATETRUSTEDDOMAINEX) {
			if (auth_struct.incoming.count > 1) {
				return NT_STATUS_INVALID_PARAMETER;
			}
		}
	}

	if (auth_struct.incoming.count) {
		nt_status = get_trustauth_inout_blob(dce_call, mem_ctx,
						     &auth_struct.incoming,
						     &trustAuthIncoming);
		if (!NT_STATUS_IS_OK(nt_status)) {
			return nt_status;
		}
	} else {
		trustAuthIncoming = data_blob(NULL, 0);
	}

	if (auth_struct.outgoing.count) {
		nt_status = get_trustauth_inout_blob(dce_call, mem_ctx,
						     &auth_struct.outgoing,
						     &trustAuthOutgoing);
		if (!NT_STATUS_IS_OK(nt_status)) {
			return nt_status;
		}
	} else {
		trustAuthOutgoing = data_blob(NULL, 0);
	}

	ret = ldb_transaction_start(sam_ldb);
	if (ret != LDB_SUCCESS) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	if (dns_name) {
		char *dns_encoded = ldb_binary_encode_string(mem_ctx, netbios_name);
		char *netbios_encoded = ldb_binary_encode_string(mem_ctx, netbios_name);
		/* search for the trusted_domain record */
		ret = gendb_search(sam_ldb,
				   mem_ctx, policy_state->system_dn, &msgs, attrs,
				   "(&(|(flatname=%s)(cn=%s)(trustPartner=%s)(flatname=%s)(cn=%s)(trustPartner=%s))(objectclass=trustedDomain))",
				   dns_encoded, dns_encoded, dns_encoded, netbios_encoded, netbios_encoded, netbios_encoded);
		if (ret > 0) {
			ldb_transaction_cancel(sam_ldb);
			return NT_STATUS_OBJECT_NAME_COLLISION;
		}
	} else {
		char *netbios_encoded = ldb_binary_encode_string(mem_ctx, netbios_name);
		/* search for the trusted_domain record */
		ret = gendb_search(sam_ldb,
				   mem_ctx, policy_state->system_dn, &msgs, attrs,
				   "(&(|(flatname=%s)(cn=%s)(trustPartner=%s))(objectclass=trustedDomain))",
				   netbios_encoded, netbios_encoded, netbios_encoded);
		if (ret > 0) {
			ldb_transaction_cancel(sam_ldb);
			return NT_STATUS_OBJECT_NAME_COLLISION;
		}
	}

	if (ret < 0 ) {
		ldb_transaction_cancel(sam_ldb);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	name = dns_name ? dns_name : netbios_name;

	msg = ldb_msg_new(mem_ctx);
	if (msg == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	msg->dn = ldb_dn_copy(mem_ctx, policy_state->system_dn);
	if ( ! ldb_dn_add_child_fmt(msg->dn, "cn=%s", name)) {
			ldb_transaction_cancel(sam_ldb);
		return NT_STATUS_NO_MEMORY;
	}

	ldb_msg_add_string(msg, "flatname", netbios_name);

	if (r->in.info->sid) {
		ret = samdb_msg_add_dom_sid(sam_ldb, mem_ctx, msg, "securityIdentifier", r->in.info->sid);
		if (ret != LDB_SUCCESS) {
			ldb_transaction_cancel(sam_ldb);
			return NT_STATUS_INVALID_PARAMETER;
		}
	}

	ldb_msg_add_string(msg, "objectClass", "trustedDomain");

	samdb_msg_add_int(sam_ldb, mem_ctx, msg, "trustType", r->in.info->trust_type);

	samdb_msg_add_int(sam_ldb, mem_ctx, msg, "trustAttributes", r->in.info->trust_attributes);

	samdb_msg_add_int(sam_ldb, mem_ctx, msg, "trustDirection", r->in.info->trust_direction);

	if (dns_name) {
		ldb_msg_add_string(msg, "trustPartner", dns_name);
	}

	if (trustAuthIncoming.data) {
		ret = ldb_msg_add_value(msg, "trustAuthIncoming", &trustAuthIncoming, NULL);
		if (ret != LDB_SUCCESS) {
			ldb_transaction_cancel(sam_ldb);
			return NT_STATUS_NO_MEMORY;
		}
	}
	if (trustAuthOutgoing.data) {
		ret = ldb_msg_add_value(msg, "trustAuthOutgoing", &trustAuthOutgoing, NULL);
		if (ret != LDB_SUCCESS) {
			ldb_transaction_cancel(sam_ldb);
			return NT_STATUS_NO_MEMORY;
		}
	}

	trusted_domain_state->trusted_domain_dn = talloc_reference(trusted_domain_state, msg->dn);

	/* create the trusted_domain */
	ret = ldb_add(sam_ldb, msg);
	switch (ret) {
	case  LDB_SUCCESS:
		break;
	case  LDB_ERR_ENTRY_ALREADY_EXISTS:
		ldb_transaction_cancel(sam_ldb);
		DEBUG(0,("Failed to create trusted domain record %s: %s\n",
			 ldb_dn_get_linearized(msg->dn),
			 ldb_errstring(sam_ldb)));
		return NT_STATUS_DOMAIN_EXISTS;
	case  LDB_ERR_INSUFFICIENT_ACCESS_RIGHTS:
		ldb_transaction_cancel(sam_ldb);
		DEBUG(0,("Failed to create trusted domain record %s: %s\n",
			 ldb_dn_get_linearized(msg->dn),
			 ldb_errstring(sam_ldb)));
		return NT_STATUS_ACCESS_DENIED;
	default:
		ldb_transaction_cancel(sam_ldb);
		DEBUG(0,("Failed to create user record %s: %s\n",
			 ldb_dn_get_linearized(msg->dn),
			 ldb_errstring(sam_ldb)));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	if (r->in.info->trust_direction & LSA_TRUST_DIRECTION_INBOUND) {
		struct ldb_dn *user_dn;
		/* Inbound trusts must also create a cn=users object to match */
		nt_status = add_trust_user(mem_ctx, sam_ldb,
					   policy_state->domain_dn,
					   netbios_name,
					   &auth_struct.incoming,
					   &user_dn);
		if (!NT_STATUS_IS_OK(nt_status)) {
			ldb_transaction_cancel(sam_ldb);
			return nt_status;
		}

		/* save the trust user dn */
		trusted_domain_state->trusted_domain_user_dn
			= talloc_steal(trusted_domain_state, user_dn);
	}

	ret = ldb_transaction_commit(sam_ldb);
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
				   "(&(samaccountname=%s$)(objectclass=user)(userAccountControl:1.2.840.113556.1.4.803:=%u))",
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
	char *td_name;
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
	td_name = ldb_binary_encode_string(mem_ctx, r->in.name.string);
	ret = gendb_search(trusted_domain_state->policy->sam_ldb,
			   mem_ctx, policy_state->system_dn, &msgs, attrs,
			   "(&(|(flatname=%s)(cn=%s)(trustPartner=%s))"
			     "(objectclass=trustedDomain))",
			   td_name, td_name, td_name);
	if (ret == 0) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	if (ret != 1) {
		DEBUG(0,("Found %d records matching DN %s\n", ret,
			 ldb_dn_get_linearized(policy_state->system_dn)));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

        /* TODO: perform access checks */

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



/* parameters 4 to 6 are optional if the dn is a dn of a TDO object,
 * otherwise at least one must be provided */
static NTSTATUS get_tdo(struct ldb_context *sam, TALLOC_CTX *mem_ctx,
			struct ldb_dn *basedn, const char *dns_domain,
			const char *netbios, struct dom_sid2 *sid,
			struct ldb_message ***msgs)
{
	const char *attrs[] = { "flatname", "trustPartner",
				"securityIdentifier", "trustDirection",
				"trustType", "trustAttributes",
				"trustPosixOffset",
				"msDs-supportedEncryptionTypes", NULL };
	char *dns = NULL;
	char *nbn = NULL;
	char *sidstr = NULL;
	char *filter;
	int ret;


	if (dns_domain || netbios || sid) {
		filter = talloc_strdup(mem_ctx,
				   "(&(objectclass=trustedDomain)(|");
	} else {
		filter = talloc_strdup(mem_ctx,
				       "(objectclass=trustedDomain)");
	}
	if (!filter) {
		return NT_STATUS_NO_MEMORY;
	}

	if (dns_domain) {
		dns = ldb_binary_encode_string(mem_ctx, dns_domain);
		if (!dns) {
			return NT_STATUS_NO_MEMORY;
		}
		filter = talloc_asprintf_append(filter,
						"(trustPartner=%s)", dns);
		if (!filter) {
			return NT_STATUS_NO_MEMORY;
		}
	}
	if (netbios) {
		nbn = ldb_binary_encode_string(mem_ctx, netbios);
		if (!nbn) {
			return NT_STATUS_NO_MEMORY;
		}
		filter = talloc_asprintf_append(filter,
						"(flatname=%s)", nbn);
		if (!filter) {
			return NT_STATUS_NO_MEMORY;
		}
	}
	if (sid) {
		sidstr = dom_sid_string(mem_ctx, sid);
		if (!sidstr) {
			return NT_STATUS_INVALID_PARAMETER;
		}
		filter = talloc_asprintf_append(filter,
						"(securityIdentifier=%s)",
						sidstr);
		if (!filter) {
			return NT_STATUS_NO_MEMORY;
		}
	}
	if (dns_domain || netbios || sid) {
		filter = talloc_asprintf_append(filter, "))");
		if (!filter) {
			return NT_STATUS_NO_MEMORY;
		}
	}

	ret = gendb_search(sam, mem_ctx, basedn, msgs, attrs, "%s", filter);
	if (ret == 0) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	if (ret != 1) {
		return NT_STATUS_OBJECT_NAME_COLLISION;
	}

	return NT_STATUS_OK;
}

static NTSTATUS update_uint32_t_value(TALLOC_CTX *mem_ctx,
				      struct ldb_context *sam_ldb,
				      struct ldb_message *orig,
				      struct ldb_message *dest,
				      const char *attribute,
				      uint32_t value,
				      uint32_t *orig_value)
{
	const struct ldb_val *orig_val;
	uint32_t orig_uint = 0;
	int flags = 0;
	int ret;

	orig_val = ldb_msg_find_ldb_val(orig, attribute);
	if (!orig_val || !orig_val->data) {
		/* add new attribute */
		flags = LDB_FLAG_MOD_ADD;

	} else {
		errno = 0;
		orig_uint = strtoul((const char *)orig_val->data, NULL, 0);
		if (errno != 0 || orig_uint != value) {
			/* replace also if can't get value */
			flags = LDB_FLAG_MOD_REPLACE;
		}
	}

	if (flags == 0) {
		/* stored value is identical, nothing to change */
		goto done;
	}

	ret = ldb_msg_add_empty(dest, attribute, flags, NULL);
	if (ret != LDB_SUCCESS) {
		return NT_STATUS_NO_MEMORY;
	}

	ret = samdb_msg_add_uint(sam_ldb, dest, dest, attribute, value);
	if (ret != LDB_SUCCESS) {
		return NT_STATUS_NO_MEMORY;
	}

done:
	if (orig_value) {
		*orig_value = orig_uint;
	}
	return NT_STATUS_OK;
}

static NTSTATUS update_trust_user(TALLOC_CTX *mem_ctx,
				  struct ldb_context *sam_ldb,
				  struct ldb_dn *base_dn,
				  bool delete_user,
				  const char *netbios_name,
				  struct trustAuthInOutBlob *in)
{
	const char *attrs[] = { "userAccountControl", NULL };
	struct ldb_message **msgs;
	struct ldb_message *msg;
	uint32_t uac;
	uint32_t i;
	int ret;

	ret = gendb_search(sam_ldb, mem_ctx,
			   base_dn, &msgs, attrs,
			   "samAccountName=%s$", netbios_name);
	if (ret > 1) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	if (ret == 0) {
		if (delete_user) {
			return NT_STATUS_OK;
		}

		/* ok no existing user, add it from scratch */
		return add_trust_user(mem_ctx, sam_ldb, base_dn,
				      netbios_name, in, NULL);
	}

	/* check user is what we are looking for */
	uac = ldb_msg_find_attr_as_uint(msgs[0],
					"userAccountControl", 0);
	if (!(uac & UF_INTERDOMAIN_TRUST_ACCOUNT)) {
		return NT_STATUS_OBJECT_NAME_COLLISION;
	}

	if (delete_user) {
		ret = ldb_delete(sam_ldb, msgs[0]->dn);
		switch (ret) {
		case LDB_SUCCESS:
			return NT_STATUS_OK;
		case LDB_ERR_INSUFFICIENT_ACCESS_RIGHTS:
			return NT_STATUS_ACCESS_DENIED;
		default:
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}
	}

	/* entry exists, just modify secret if any */
	if (in->count == 0) {
		return NT_STATUS_OK;
	}

	msg = ldb_msg_new(mem_ctx);
	if (!msg) {
		return NT_STATUS_NO_MEMORY;
	}
	msg->dn = msgs[0]->dn;

	for (i = 0; i < in->count; i++) {
		const char *attribute;
		struct ldb_val v;
		switch (in->current.array[i].AuthType) {
		case TRUST_AUTH_TYPE_NT4OWF:
			attribute = "unicodePwd";
			v.data = (uint8_t *)&in->current.array[i].AuthInfo.nt4owf.password;
			v.length = 16;
			break;
		case TRUST_AUTH_TYPE_CLEAR:
			attribute = "clearTextPassword";
			v.data = in->current.array[i].AuthInfo.clear.password;
			v.length = in->current.array[i].AuthInfo.clear.size;
			break;
		default:
			continue;
		}

		ret = ldb_msg_add_empty(msg, attribute,
					LDB_FLAG_MOD_REPLACE, NULL);
		if (ret != LDB_SUCCESS) {
			return NT_STATUS_NO_MEMORY;
		}

		ret = ldb_msg_add_value(msg, attribute, &v, NULL);
		if (ret != LDB_SUCCESS) {
			return NT_STATUS_NO_MEMORY;
		}
	}

	/* create the trusted_domain user account */
	ret = ldb_modify(sam_ldb, msg);
	if (ret != LDB_SUCCESS) {
		DEBUG(0,("Failed to create user record %s: %s\n",
			 ldb_dn_get_linearized(msg->dn),
			 ldb_errstring(sam_ldb)));

		switch (ret) {
		case LDB_ERR_ENTRY_ALREADY_EXISTS:
			return NT_STATUS_DOMAIN_EXISTS;
		case LDB_ERR_INSUFFICIENT_ACCESS_RIGHTS:
			return NT_STATUS_ACCESS_DENIED;
		default:
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}
	}

	return NT_STATUS_OK;
}


static NTSTATUS setInfoTrustedDomain_base(struct dcesrv_call_state *dce_call,
					  struct dcesrv_handle *p_handle,
					  TALLOC_CTX *mem_ctx,
					  struct ldb_message *dom_msg,
					  enum lsa_TrustDomInfoEnum level,
					  union lsa_TrustedDomainInfo *info)
{
	struct lsa_policy_state *p_state = p_handle->data;
	uint32_t *posix_offset = NULL;
	struct lsa_TrustDomainInfoInfoEx *info_ex = NULL;
	struct lsa_TrustDomainInfoAuthInfo *auth_info = NULL;
	struct lsa_TrustDomainInfoAuthInfoInternal *auth_info_int = NULL;
	uint32_t *enc_types = NULL;
	DATA_BLOB trustAuthIncoming, trustAuthOutgoing, auth_blob;
	struct trustDomainPasswords auth_struct;
	NTSTATUS nt_status;
	struct ldb_message **msgs;
	struct ldb_message *msg;
	bool add_outgoing = false;
	bool add_incoming = false;
	bool del_outgoing = false;
	bool del_incoming = false;
	bool in_transaction = false;
	int ret;
	bool am_rodc;

	switch (level) {
	case LSA_TRUSTED_DOMAIN_INFO_POSIX_OFFSET:
		posix_offset = &info->posix_offset.posix_offset;
		break;
	case LSA_TRUSTED_DOMAIN_INFO_INFO_EX:
		info_ex = &info->info_ex;
		break;
	case LSA_TRUSTED_DOMAIN_INFO_AUTH_INFO:
		auth_info = &info->auth_info;
		break;
	case LSA_TRUSTED_DOMAIN_INFO_FULL_INFO:
		posix_offset = &info->full_info.posix_offset.posix_offset;
		info_ex = &info->full_info.info_ex;
		auth_info = &info->full_info.auth_info;
		break;
	case LSA_TRUSTED_DOMAIN_INFO_AUTH_INFO_INTERNAL:
		auth_info_int = &info->auth_info_internal;
		break;
	case LSA_TRUSTED_DOMAIN_INFO_FULL_INFO_INTERNAL:
		posix_offset = &info->full_info_internal.posix_offset.posix_offset;
		info_ex = &info->full_info_internal.info_ex;
		auth_info_int = &info->full_info_internal.auth_info;
		break;
	case LSA_TRUSTED_DOMAIN_SUPPORTED_ENCRYPTION_TYPES:
		enc_types = &info->enc_types.enc_types;
		break;
	default:
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (auth_info) {
		/* FIXME: not handled yet */
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* decode auth_info_int if set */
	if (auth_info_int) {

		/* now decrypt blob */
		auth_blob = data_blob_const(auth_info_int->auth_blob.data,
					    auth_info_int->auth_blob.size);

		nt_status = get_trustdom_auth_blob(dce_call, mem_ctx,
						   &auth_blob, &auth_struct);
		if (!NT_STATUS_IS_OK(nt_status)) {
			return nt_status;
		}
	}

	if (info_ex) {
		/* verify data matches */
		if (info_ex->trust_attributes &
		    LSA_TRUST_ATTRIBUTE_FOREST_TRANSITIVE) {
			/* TODO: check what behavior level we have */
		       if (strcasecmp_m(p_state->domain_dns,
					p_state->forest_dns) != 0) {
				return NT_STATUS_INVALID_DOMAIN_STATE;
			}
		}

		ret = samdb_rodc(p_state->sam_ldb, &am_rodc);
		if (ret == LDB_SUCCESS && am_rodc) {
			return NT_STATUS_NO_SUCH_DOMAIN;
		}

		/* verify only one object matches the dns/netbios/sid
		 * triplet and that this is the one we already have */
		nt_status = get_tdo(p_state->sam_ldb, mem_ctx,
				    p_state->system_dn,
				    info_ex->domain_name.string,
				    info_ex->netbios_name.string,
				    info_ex->sid, &msgs);
		if (!NT_STATUS_IS_OK(nt_status)) {
			return nt_status;
		}
		if (ldb_dn_compare(dom_msg->dn, msgs[0]->dn) != 0) {
			return NT_STATUS_OBJECT_NAME_COLLISION;
		}
		talloc_free(msgs);
	}

	/* TODO: should we fetch previous values from the existing entry
	 * and append them ? */
	if (auth_struct.incoming.count) {
		nt_status = get_trustauth_inout_blob(dce_call, mem_ctx,
						     &auth_struct.incoming,
						     &trustAuthIncoming);
		if (!NT_STATUS_IS_OK(nt_status)) {
			return nt_status;
		}
	} else {
		trustAuthIncoming = data_blob(NULL, 0);
	}

	if (auth_struct.outgoing.count) {
		nt_status = get_trustauth_inout_blob(dce_call, mem_ctx,
						     &auth_struct.outgoing,
						     &trustAuthOutgoing);
		if (!NT_STATUS_IS_OK(nt_status)) {
			return nt_status;
		}
	} else {
		trustAuthOutgoing = data_blob(NULL, 0);
	}

	msg = ldb_msg_new(mem_ctx);
	if (msg == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	msg->dn = dom_msg->dn;

	if (posix_offset) {
		nt_status = update_uint32_t_value(mem_ctx, p_state->sam_ldb,
						  dom_msg, msg,
						  "trustPosixOffset",
						  *posix_offset, NULL);
		if (!NT_STATUS_IS_OK(nt_status)) {
			return nt_status;
		}
	}

	if (info_ex) {
		uint32_t origattrs;
		uint32_t origdir;
		uint32_t tmp;
		int origtype;

		nt_status = update_uint32_t_value(mem_ctx, p_state->sam_ldb,
						  dom_msg, msg,
						  "trustDirection",
						  info_ex->trust_direction,
						  &origdir);
		if (!NT_STATUS_IS_OK(nt_status)) {
			return nt_status;
		}

		tmp = info_ex->trust_direction ^ origdir;
		if (tmp & LSA_TRUST_DIRECTION_INBOUND) {
			if (origdir & LSA_TRUST_DIRECTION_INBOUND) {
				del_incoming = true;
			} else {
				add_incoming = true;
			}
		}
		if (tmp & LSA_TRUST_DIRECTION_OUTBOUND) {
			if (origdir & LSA_TRUST_DIRECTION_OUTBOUND) {
				del_outgoing = true;
			} else {
				add_outgoing = true;
			}
		}

		origtype = ldb_msg_find_attr_as_int(dom_msg, "trustType", -1);
		if (origtype == -1 || origtype != info_ex->trust_type) {
			DEBUG(1, ("Attempted to change trust type! "
				  "Operation not handled\n"));
			return NT_STATUS_INVALID_PARAMETER;
		}

		nt_status = update_uint32_t_value(mem_ctx, p_state->sam_ldb,
						  dom_msg, msg,
						  "trustAttributes",
						  info_ex->trust_attributes,
						  &origattrs);
		if (!NT_STATUS_IS_OK(nt_status)) {
			return nt_status;
		}
		/* TODO: check forestFunctionality from ldb opaque */
		/* TODO: check what is set makes sense */
		/* for now refuse changes */
		if (origattrs == -1 ||
		    origattrs != info_ex->trust_attributes) {
			DEBUG(1, ("Attempted to change trust attributes! "
				  "Operation not handled\n"));
			return NT_STATUS_INVALID_PARAMETER;
		}
	}

	if (enc_types) {
		nt_status = update_uint32_t_value(mem_ctx, p_state->sam_ldb,
						  dom_msg, msg,
						  "msDS-SupportedEncryptionTypes",
						  *enc_types, NULL);
		if (!NT_STATUS_IS_OK(nt_status)) {
			return nt_status;
		}
	}

	if (add_incoming && trustAuthIncoming.data) {
		ret = ldb_msg_add_empty(msg, "trustAuthIncoming",
					LDB_FLAG_MOD_REPLACE, NULL);
		if (ret != LDB_SUCCESS) {
			return NT_STATUS_NO_MEMORY;
		}
		ret = ldb_msg_add_value(msg, "trustAuthIncoming",
					&trustAuthIncoming, NULL);
		if (ret != LDB_SUCCESS) {
			return NT_STATUS_NO_MEMORY;
		}
	}
	if (add_outgoing && trustAuthOutgoing.data) {
		ret = ldb_msg_add_empty(msg, "trustAuthIncoming",
					LDB_FLAG_MOD_REPLACE, NULL);
		if (ret != LDB_SUCCESS) {
			return NT_STATUS_NO_MEMORY;
		}
		ret = ldb_msg_add_value(msg, "trustAuthOutgoing",
					&trustAuthOutgoing, NULL);
		if (ret != LDB_SUCCESS) {
			return NT_STATUS_NO_MEMORY;
		}
	}

	/* start transaction */
	ret = ldb_transaction_start(p_state->sam_ldb);
	if (ret != LDB_SUCCESS) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}
	in_transaction = true;

	ret = ldb_modify(p_state->sam_ldb, msg);
	if (ret != LDB_SUCCESS) {
		DEBUG(1,("Failed to modify trusted domain record %s: %s\n",
			 ldb_dn_get_linearized(msg->dn),
			 ldb_errstring(p_state->sam_ldb)));
		if (ret == LDB_ERR_INSUFFICIENT_ACCESS_RIGHTS) {
			nt_status = NT_STATUS_ACCESS_DENIED;
		} else {
			nt_status = NT_STATUS_INTERNAL_DB_CORRUPTION;
		}
		goto done;
	}

	if (add_incoming || del_incoming) {
		const char *netbios_name;

		netbios_name = ldb_msg_find_attr_as_string(dom_msg,
							   "flatname", NULL);
		if (!netbios_name) {
			nt_status = NT_STATUS_INVALID_DOMAIN_STATE;
			goto done;
		}

		nt_status = update_trust_user(mem_ctx,
					      p_state->sam_ldb,
					      p_state->domain_dn,
					      del_incoming,
					      netbios_name,
					      &auth_struct.incoming);
		if (!NT_STATUS_IS_OK(nt_status)) {
			goto done;
		}
	}

	/* ok, all fine, commit transaction and return */
	ret = ldb_transaction_commit(p_state->sam_ldb);
	if (ret != LDB_SUCCESS) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}
	in_transaction = false;

	nt_status = NT_STATUS_OK;

done:
	if (in_transaction) {
		ldb_transaction_cancel(p_state->sam_ldb);
	}
	return nt_status;
}

/*
  lsa_SetInfomrationTrustedDomain
*/
static NTSTATUS dcesrv_lsa_SetInformationTrustedDomain(
				struct dcesrv_call_state *dce_call,
				TALLOC_CTX *mem_ctx,
				struct lsa_SetInformationTrustedDomain *r)
{
	struct dcesrv_handle *h;
	struct lsa_trusted_domain_state *td_state;
	struct ldb_message **msgs;
	NTSTATUS nt_status;

	DCESRV_PULL_HANDLE(h, r->in.trustdom_handle,
			   LSA_HANDLE_TRUSTED_DOMAIN);

	td_state = talloc_get_type(h->data, struct lsa_trusted_domain_state);

	/* get the trusted domain object */
	nt_status = get_tdo(td_state->policy->sam_ldb, mem_ctx,
			    td_state->trusted_domain_dn,
			    NULL, NULL, NULL, &msgs);
	if (!NT_STATUS_IS_OK(nt_status)) {
		if (NT_STATUS_EQUAL(nt_status,
				    NT_STATUS_OBJECT_NAME_NOT_FOUND)) {
			return nt_status;
		}
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	return setInfoTrustedDomain_base(dce_call, h, mem_ctx,
					 msgs[0], r->in.level, r->in.info);
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
			= ldb_msg_find_attr_as_string(msg, "flatname", NULL);
		break;
	case LSA_TRUSTED_DOMAIN_INFO_POSIX_OFFSET:
		info->posix_offset.posix_offset
			= ldb_msg_find_attr_as_uint(msg, "posixOffset", 0);
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
			= ldb_msg_find_attr_as_uint(msg, "posixOffset", 0);
		return fill_trust_domain_ex(mem_ctx, msg, &info->full_info2_internal.info.info_ex);
		
	case LSA_TRUSTED_DOMAIN_SUPPORTED_ENCRYPTION_TYPES:
		info->enc_types.enc_types
			= ldb_msg_find_attr_as_uint(msg, "msDs-supportedEncryptionTypes", KERB_ENCTYPE_RC4_HMAC_MD5);
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
	struct dcesrv_handle *policy_handle;
	struct lsa_policy_state *policy_state;
	struct ldb_message **msgs;
	NTSTATUS nt_status;

	DCESRV_PULL_HANDLE(policy_handle, r->in.handle, LSA_HANDLE_POLICY);
	policy_state = policy_handle->data;

	/* get the trusted domain object */
	nt_status = get_tdo(policy_state->sam_ldb, mem_ctx,
			    policy_state->domain_dn,
			    r->in.trusted_domain->string,
			    r->in.trusted_domain->string,
			    NULL, &msgs);
	if (!NT_STATUS_IS_OK(nt_status)) {
		if (NT_STATUS_EQUAL(nt_status,
				    NT_STATUS_OBJECT_NAME_NOT_FOUND)) {
			return nt_status;
		}
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	return setInfoTrustedDomain_base(dce_call, policy_handle, mem_ctx,
					 msgs[0], r->in.level, r->in.info);
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
	if (count < 0) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	/* convert to lsa_TrustInformation format */
	entries = talloc_array(mem_ctx, struct lsa_DomainInfo, count);
	if (!entries) {
		return NT_STATUS_NO_MEMORY;
	}
	for (i=0;i<count;i++) {
		entries[i].sid = samdb_result_dom_sid(mem_ctx, domains[i], "securityIdentifier");
		entries[i].name.string = ldb_msg_find_attr_as_string(domains[i], "flatname", NULL);
	}

	/* sort the results by name */
	TYPESAFE_QSORT(entries, count, compare_DomainInfo);

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
	if (count < 0) {
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
	TYPESAFE_QSORT(entries, count, compare_TrustDomainInfoInfoEx);

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
	int ret;
	unsigned int i, j;
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

	ret = gendb_search(astate->policy->pdb, mem_ctx, NULL, &res, attrs, 
			   "objectSid=%s", sidstr);
	if (ret < 0) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}
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

	j = 0;
	for (i=0;i<el->num_values;i++) {
		int id = sec_privilege_id((const char *)el->values[i].data);
		if (id == SEC_PRIV_INVALID) {
			/* Perhaps an account right, not a privilege */
			continue;
		}
		privs->set[j].attribute = 0;
		privs->set[j].luid.low = id;
		privs->set[j].luid.high = 0;
		j++;
	}

	privs->count = j;

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
	int ret;
	unsigned int i;
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

	ret = gendb_search(state->pdb, mem_ctx, NULL, &res, attrs, 
			   "(&(objectSid=%s)(privilege=*))", sidstr);
	if (ret == 0) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}
	if (ret != 1) {
		DEBUG(3, ("searching for account rights for SID: %s failed: %s", 
			  dom_sid_string(mem_ctx, r->in.sid),
			  ldb_errstring(state->pdb)));
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
	const char *sidstr, *sidndrstr;
	struct ldb_message *msg;
	struct ldb_message_element *el;
	int ret;
	uint32_t i;
	struct lsa_EnumAccountRights r2;
	char *dnstr;

	if (security_session_user_level(dce_call->conn->auth_state.session_info, NULL) <
	    SECURITY_ADMINISTRATOR) {
		DEBUG(0,("lsa_AddRemoveAccount refused for supplied security token\n"));
		return NT_STATUS_ACCESS_DENIED;
	}

	msg = ldb_msg_new(mem_ctx);
	if (msg == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	sidndrstr = ldap_encode_ndr_dom_sid(msg, sid);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(sidndrstr, msg);

	sidstr = dom_sid_string(msg, sid);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(sidstr, msg);

	dnstr = talloc_asprintf(msg, "sid=%s", sidstr);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(dnstr, msg);

	msg->dn = ldb_dn_new(msg, state->pdb, dnstr);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(msg->dn, msg);

	if (LDB_FLAG_MOD_TYPE(ldb_flag) == LDB_FLAG_MOD_ADD) {
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
		if (sec_privilege_id(rights->names[i].string) == SEC_PRIV_INVALID) {
			if (sec_right_bit(rights->names[i].string) == 0) {
				talloc_free(msg);
				return NT_STATUS_NO_SUCH_PRIVILEGE;
			}

			talloc_free(msg);
			return NT_STATUS_NO_SUCH_PRIVILEGE;
		}

		if (LDB_FLAG_MOD_TYPE(ldb_flag) == LDB_FLAG_MOD_ADD) {
			uint32_t j;
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
			talloc_free(msg);
			return NT_STATUS_NO_MEMORY;
		}
	}

	el = ldb_msg_find_element(msg, "privilege");
	if (!el) {
		talloc_free(msg);
		return NT_STATUS_OK;
	}

	el->flags = ldb_flag;

	ret = ldb_modify(state->pdb, msg);
	if (ret == LDB_ERR_NO_SUCH_OBJECT) {
		if (samdb_msg_add_dom_sid(state->pdb, msg, msg, "objectSid", sid) != LDB_SUCCESS) {
			talloc_free(msg);
			return NT_STATUS_NO_MEMORY;
		}
		ldb_msg_add_string(msg, "comment", "added via LSA");
		ret = ldb_add(state->pdb, msg);		
	}
	if (ret != LDB_SUCCESS) {
		if (LDB_FLAG_MOD_TYPE(ldb_flag) == LDB_FLAG_MOD_DELETE && ret == LDB_ERR_NO_SUCH_ATTRIBUTE) {
			talloc_free(msg);
			return NT_STATUS_OK;
		}
		DEBUG(3, ("Could not %s attributes from %s: %s", 
			  LDB_FLAG_MOD_TYPE(ldb_flag) == LDB_FLAG_MOD_DELETE ? "delete" : "add",
			  ldb_dn_get_linearized(msg->dn), ldb_errstring(state->pdb)));
		talloc_free(msg);
		return NT_STATUS_UNEXPECTED_IO_ERROR;
	}

	talloc_free(msg);
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
	uint32_t i;

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
	uint32_t i;

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
	struct dcesrv_handle *h;
	struct lsa_account_state *astate;
	int ret;
	unsigned int i;
	struct ldb_message **res;
	const char * const attrs[] = { "privilege", NULL};
	struct ldb_message_element *el;
	const char *sidstr;

	*(r->out.access_mask) = 0x00000000;

	DCESRV_PULL_HANDLE(h, r->in.handle, LSA_HANDLE_ACCOUNT);

	astate = h->data;

	sidstr = ldap_encode_ndr_dom_sid(mem_ctx, astate->account_sid);
	if (sidstr == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	ret = gendb_search(astate->policy->pdb, mem_ctx, NULL, &res, attrs, 
			   "objectSid=%s", sidstr);
	if (ret < 0) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}
	if (ret != 1) {
		return NT_STATUS_OK;
	}

	el = ldb_msg_find_element(res[0], "privilege");
	if (el == NULL || el->num_values == 0) {
		return NT_STATUS_OK;
	}

	for (i=0;i<el->num_values;i++) {
		uint32_t right_bit = sec_right_bit((const char *)el->values[i].data);
		if (right_bit == 0) {
			/* Perhaps an privilege, not a right */
			continue;
		}
		*(r->out.access_mask) |= right_bit;
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
	
	switch (security_session_user_level(dce_call->conn->auth_state.session_info, NULL))
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
	NT_STATUS_HAVE_NO_MEMORY(secret_state);
	secret_state->policy = policy_state;

	msg = ldb_msg_new(mem_ctx);
	if (msg == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	if (strncmp("G$", r->in.name.string, 2) == 0) {
		const char *name2;

		secret_state->global = true;

		name = &r->in.name.string[2];
		if (strlen(name) == 0) {
			return NT_STATUS_INVALID_PARAMETER;
		}

		name2 = talloc_asprintf(mem_ctx, "%s Secret",
					ldb_binary_encode_string(mem_ctx, name));
		NT_STATUS_HAVE_NO_MEMORY(name2);

		/* We need to connect to the database as system, as this is one
		 * of the rare RPC calls that must read the secrets (and this
		 * is denied otherwise) */
		secret_state->sam_ldb = talloc_reference(secret_state,
							 samdb_connect(mem_ctx, dce_call->event_ctx, dce_call->conn->dce_ctx->lp_ctx, system_session(dce_call->conn->dce_ctx->lp_ctx), 0));
		NT_STATUS_HAVE_NO_MEMORY(secret_state->sam_ldb);

		/* search for the secret record */
		ret = gendb_search(secret_state->sam_ldb,
				   mem_ctx, policy_state->system_dn, &msgs, attrs,
				   "(&(cn=%s)(objectclass=secret))", 
				   name2);
		if (ret > 0) {
			return NT_STATUS_OBJECT_NAME_COLLISION;
		}
		
		if (ret < 0) {
			DEBUG(0,("Failure searching for CN=%s: %s\n", 
				 name2, ldb_errstring(secret_state->sam_ldb)));
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		msg->dn = ldb_dn_copy(mem_ctx, policy_state->system_dn);
		NT_STATUS_HAVE_NO_MEMORY(msg->dn);
		if (!ldb_dn_add_child_fmt(msg->dn, "cn=%s", name2)) {
			return NT_STATUS_NO_MEMORY;
		}

		ret = ldb_msg_add_string(msg, "cn", name2);
		if (ret != LDB_SUCCESS) return NT_STATUS_NO_MEMORY;
	} else {
		secret_state->global = false;

		name = r->in.name.string;
		if (strlen(name) == 0) {
			return NT_STATUS_INVALID_PARAMETER;
		}

		secret_state->sam_ldb = talloc_reference(secret_state, 
							 secrets_db_connect(mem_ctx, dce_call->conn->dce_ctx->lp_ctx));
		NT_STATUS_HAVE_NO_MEMORY(secret_state->sam_ldb);

		/* search for the secret record */
		ret = gendb_search(secret_state->sam_ldb, mem_ctx,
				   ldb_dn_new(mem_ctx, secret_state->sam_ldb, "cn=LSA Secrets"),
				   &msgs, attrs,
				   "(&(cn=%s)(objectclass=secret))", 
				   ldb_binary_encode_string(mem_ctx, name));
		if (ret > 0) {
			return NT_STATUS_OBJECT_NAME_COLLISION;
		}
		
		if (ret < 0) {
			DEBUG(0,("Failure searching for CN=%s: %s\n", 
				 name, ldb_errstring(secret_state->sam_ldb)));
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		msg->dn = ldb_dn_new_fmt(mem_ctx, secret_state->sam_ldb,
					 "cn=%s,cn=LSA Secrets", name);
		NT_STATUS_HAVE_NO_MEMORY(msg->dn);
		ret = ldb_msg_add_string(msg, "cn", name);
		if (ret != LDB_SUCCESS) return NT_STATUS_NO_MEMORY;
	} 

	ret = ldb_msg_add_string(msg, "objectClass", "secret");
	if (ret != LDB_SUCCESS) return NT_STATUS_NO_MEMORY;
	
	secret_state->secret_dn = talloc_reference(secret_state, msg->dn);
	NT_STATUS_HAVE_NO_MEMORY(secret_state->secret_dn);

	/* create the secret */
	ret = ldb_add(secret_state->sam_ldb, msg);
	if (ret != LDB_SUCCESS) {
		DEBUG(0,("Failed to create secret record %s: %s\n",
			 ldb_dn_get_linearized(msg->dn), 
			 ldb_errstring(secret_state->sam_ldb)));
		return NT_STATUS_ACCESS_DENIED;
	}

	handle = dcesrv_handle_new(dce_call->context, LSA_HANDLE_SECRET);
	NT_STATUS_HAVE_NO_MEMORY(handle);

	handle->data = talloc_steal(handle, secret_state);
	
	secret_state->access_mask = r->in.access_mask;
	secret_state->policy = talloc_reference(secret_state, policy_state);
	NT_STATUS_HAVE_NO_MEMORY(secret_state->policy);
	
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
	
	switch (security_session_user_level(dce_call->conn->auth_state.session_info, NULL))
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
							 samdb_connect(mem_ctx, dce_call->event_ctx, dce_call->conn->dce_ctx->lp_ctx, system_session(dce_call->conn->dce_ctx->lp_ctx), 0));
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
							 secrets_db_connect(mem_ctx, dce_call->conn->dce_ctx->lp_ctx));

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
		if (ldb_msg_add_value(msg, "priorValue", &val, NULL) != LDB_SUCCESS) {
			return NT_STATUS_NO_MEMORY; 
		}
		
		/* set old value mtime */
		if (samdb_msg_add_uint64(secret_state->sam_ldb, 
					 mem_ctx, msg, "priorSetTime", nt_now) != LDB_SUCCESS) {
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
			if (ldb_msg_add_value(msg, "priorValue",
					      old_val, NULL) != LDB_SUCCESS) {
				return NT_STATUS_NO_MEMORY; 
			}
		} else {
			if (samdb_msg_add_delete(secret_state->sam_ldb, 
						 mem_ctx, msg, "priorValue") != LDB_SUCCESS) {
				return NT_STATUS_NO_MEMORY;
			}
			
		}
		
		/* set old value mtime */
		if (ldb_msg_find_ldb_val(res[0], "lastSetTime")) {
			if (samdb_msg_add_uint64(secret_state->sam_ldb, 
						 mem_ctx, msg, "priorSetTime", last_set_time) != LDB_SUCCESS) {
				return NT_STATUS_NO_MEMORY; 
			}
		} else {
			if (samdb_msg_add_uint64(secret_state->sam_ldb, 
						 mem_ctx, msg, "priorSetTime", nt_now) != LDB_SUCCESS) {
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
		if (ldb_msg_add_value(msg, "currentValue", &val, NULL) != LDB_SUCCESS) {
			return NT_STATUS_NO_MEMORY; 
		}
		
		/* set new value mtime */
		if (samdb_msg_add_uint64(secret_state->sam_ldb, 
					 mem_ctx, msg, "lastSetTime", nt_now) != LDB_SUCCESS) {
			return NT_STATUS_NO_MEMORY; 
		}
		
	} else {
		/* NULL out the NEW value */
		if (samdb_msg_add_uint64(secret_state->sam_ldb, 
					 mem_ctx, msg, "lastSetTime", nt_now) != LDB_SUCCESS) {
			return NT_STATUS_NO_MEMORY; 
		}
		if (samdb_msg_add_delete(secret_state->sam_ldb, 
					 mem_ctx, msg, "currentValue") != LDB_SUCCESS) {
			return NT_STATUS_NO_MEMORY;
		}
	}

	/* modify the samdb record */
	ret = dsdb_replace(secret_state->sam_ldb, msg, 0);
	if (ret != LDB_SUCCESS) {
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
	switch (security_session_user_level(dce_call->conn->auth_state.session_info, NULL))
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
	if (id == SEC_PRIV_INVALID) {
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
	enum sec_privilege id;

	DCESRV_PULL_HANDLE(h, r->in.handle, LSA_HANDLE_POLICY);

	state = h->data;

	id = sec_privilege_id(r->in.name->string);
	if (id == SEC_PRIV_INVALID) {
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
	if (sec_privilege_id(privname) == SEC_PRIV_INVALID && sec_right_bit(privname) == 0) {
		return NT_STATUS_NO_SUCH_PRIVILEGE;
	}

	ret = gendb_search(state->pdb, mem_ctx, NULL, &res, attrs, 
			   "privilege=%s", privname);
	if (ret < 0) {
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

	account_name = talloc_reference(mem_ctx, dce_call->conn->auth_state.session_info->info->account_name);
	authority_name = talloc_reference(mem_ctx, dce_call->conn->auth_state.session_info->info->domain_name);

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

	info = talloc_zero(r->out.info, union lsa_DomainInformationPolicy);
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
		kdc_get_policy(dce_call->conn->dce_ctx->lp_ctx,
			       smb_krb5_context,
			       k);
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

#define DNS_CMP_MATCH 0
#define DNS_CMP_FIRST_IS_CHILD 1
#define DNS_CMP_SECOND_IS_CHILD 2
#define DNS_CMP_NO_MATCH 3

/* this function assumes names are well formed DNS names.
 * it doesn't validate them */
static int dns_cmp(const char *s1, size_t l1,
		   const char *s2, size_t l2)
{
	const char *p1, *p2;
	size_t t1, t2;
	int cret;

	if (l1 == l2) {
		if (strcasecmp_m(s1, s2) == 0) {
			return DNS_CMP_MATCH;
		}
		return DNS_CMP_NO_MATCH;
	}

	if (l1 > l2) {
		p1 = s1;
		p2 = s2;
		t1 = l1;
		t2 = l2;
		cret = DNS_CMP_FIRST_IS_CHILD;
	} else {
		p1 = s2;
		p2 = s1;
		t1 = l2;
		t2 = l1;
		cret = DNS_CMP_SECOND_IS_CHILD;
	}

	if (p1[t1 - t2 - 1] != '.') {
		return DNS_CMP_NO_MATCH;
	}

	if (strcasecmp_m(&p1[t1 - t2], p2) == 0) {
		return cret;
	}

	return DNS_CMP_NO_MATCH;
}

/* decode all TDOs forest trust info blobs */
static NTSTATUS get_ft_info(TALLOC_CTX *mem_ctx,
			    struct ldb_message *msg,
			    struct ForestTrustInfo *info)
{
	const struct ldb_val *ft_blob;
	enum ndr_err_code ndr_err;

	ft_blob = ldb_msg_find_ldb_val(msg, "msDS-TrustForestTrustInfo");
	if (!ft_blob || !ft_blob->data) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}
	/* ldb_val is equivalent to DATA_BLOB */
	ndr_err = ndr_pull_struct_blob_all(ft_blob, mem_ctx, info,
					   (ndr_pull_flags_fn_t)ndr_pull_ForestTrustInfo);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return NT_STATUS_INVALID_DOMAIN_STATE;
	}

	return NT_STATUS_OK;
}

static NTSTATUS own_ft_info(struct lsa_policy_state *ps,
			    struct ForestTrustInfo *fti)
{
	struct ForestTrustDataDomainInfo *info;
	struct ForestTrustInfoRecord *rec;

	fti->version = 1;
	fti->count = 2;
	fti->records = talloc_array(fti,
				    struct ForestTrustInfoRecordArmor, 2);
	if (!fti->records) {
		return NT_STATUS_NO_MEMORY;
	}

        /* TLN info */
	rec = &fti->records[0].record;

	rec->flags = 0;
	rec->timestamp = 0;
	rec->type = LSA_FOREST_TRUST_TOP_LEVEL_NAME;

	rec->data.name.string = talloc_strdup(fti, ps->forest_dns);
	if (!rec->data.name.string) {
		return NT_STATUS_NO_MEMORY;
	}
	rec->data.name.size = strlen(rec->data.name.string);

        /* DOMAIN info */
	rec = &fti->records[1].record;

	rec->flags = 0;
	rec->timestamp = 0;
	rec->type = LSA_FOREST_TRUST_DOMAIN_INFO;

        info = &rec->data.info;

	info->sid = *ps->domain_sid;
	info->dns_name.string = talloc_strdup(fti, ps->domain_dns);
	if (!info->dns_name.string) {
		return NT_STATUS_NO_MEMORY;
	}
	info->dns_name.size = strlen(info->dns_name.string);
	info->netbios_name.string = talloc_strdup(fti, ps->domain_name);
	if (!info->netbios_name.string) {
		return NT_STATUS_NO_MEMORY;
	}
	info->netbios_name.size = strlen(info->netbios_name.string);

	return NT_STATUS_OK;
}

static NTSTATUS make_ft_info(TALLOC_CTX *mem_ctx,
			     struct lsa_ForestTrustInformation *lfti,
			     struct ForestTrustInfo *fti)
{
	struct lsa_ForestTrustRecord *lrec;
	struct ForestTrustInfoRecord *rec;
	struct lsa_StringLarge *tln;
	struct lsa_ForestTrustDomainInfo *info;
	uint32_t i;

	fti->version = 1;
	fti->count = lfti->count;
	fti->records = talloc_array(mem_ctx,
				    struct ForestTrustInfoRecordArmor,
				    fti->count);
	if (!fti->records) {
		return NT_STATUS_NO_MEMORY;
	}
	for (i = 0; i < fti->count; i++) {
		lrec = lfti->entries[i];
		rec = &fti->records[i].record;

		rec->flags = lrec->flags;
		rec->timestamp = lrec->time;
		rec->type = lrec->type;

		switch (lrec->type) {
		case LSA_FOREST_TRUST_TOP_LEVEL_NAME:
		case LSA_FOREST_TRUST_TOP_LEVEL_NAME_EX:
			tln = &lrec->forest_trust_data.top_level_name;
			rec->data.name.string =
				talloc_strdup(mem_ctx, tln->string);
			if (!rec->data.name.string) {
				return NT_STATUS_NO_MEMORY;
			}
			rec->data.name.size = strlen(rec->data.name.string);
			break;
		case LSA_FOREST_TRUST_DOMAIN_INFO:
			info = &lrec->forest_trust_data.domain_info;
			rec->data.info.sid = *info->domain_sid;
			rec->data.info.dns_name.string =
				talloc_strdup(mem_ctx,
					    info->dns_domain_name.string);
			if (!rec->data.info.dns_name.string) {
				return NT_STATUS_NO_MEMORY;
			}
			rec->data.info.dns_name.size =
				strlen(rec->data.info.dns_name.string);
			rec->data.info.netbios_name.string =
				talloc_strdup(mem_ctx,
					    info->netbios_domain_name.string);
			if (!rec->data.info.netbios_name.string) {
				return NT_STATUS_NO_MEMORY;
			}
			rec->data.info.netbios_name.size =
				strlen(rec->data.info.netbios_name.string);
			break;
		default:
			return NT_STATUS_INVALID_DOMAIN_STATE;
		}
	}

	return NT_STATUS_OK;
}

static NTSTATUS add_collision(struct lsa_ForestTrustCollisionInfo *c_info,
			      uint32_t idx, uint32_t collision_type,
			      uint32_t conflict_type, const char *tdo_name);

static NTSTATUS check_ft_info(TALLOC_CTX *mem_ctx,
			      const char *tdo_name,
			      struct ForestTrustInfo *tdo_fti,
			      struct ForestTrustInfo *new_fti,
			      struct lsa_ForestTrustCollisionInfo *c_info)
{
	struct ForestTrustInfoRecord *nrec;
	struct ForestTrustInfoRecord *trec;
	const char *dns_name;
	const char *nb_name;
	struct dom_sid *sid;
	const char *tname;
	size_t dns_len;
	size_t nb_len;
	size_t tlen;
	NTSTATUS nt_status;
	uint32_t new_fti_idx;
	uint32_t i;
	/* use always TDO type, until we understand when Xref can be used */
	uint32_t collision_type = LSA_FOREST_TRUST_COLLISION_TDO;
	bool tln_conflict;
	bool sid_conflict;
	bool nb_conflict;
	bool exclusion;
	bool ex_rule;
	int ret;

	for (new_fti_idx = 0; new_fti_idx < new_fti->count; new_fti_idx++) {

		nrec = &new_fti->records[new_fti_idx].record;
		dns_name = NULL;
		tln_conflict = false;
		sid_conflict = false;
		nb_conflict = false;
		exclusion = false;

		switch (nrec->type) {
		case LSA_FOREST_TRUST_TOP_LEVEL_NAME_EX:
			/* exclusions do not conflict by definition */
			break;

		case FOREST_TRUST_TOP_LEVEL_NAME:
			dns_name = nrec->data.name.string;
			dns_len = nrec->data.name.size;
			break;

		case LSA_FOREST_TRUST_DOMAIN_INFO:
			dns_name = nrec->data.info.dns_name.string;
			dns_len = nrec->data.info.dns_name.size;
			nb_name = nrec->data.info.netbios_name.string;
			nb_len = nrec->data.info.netbios_name.size;
			sid = &nrec->data.info.sid;
			break;
		}

		if (!dns_name) continue;

		/* check if this is already taken and not excluded */
		for (i = 0; i < tdo_fti->count; i++) {
			trec = &tdo_fti->records[i].record;

			switch (trec->type) {
			case FOREST_TRUST_TOP_LEVEL_NAME:
				ex_rule = false;
				tname = trec->data.name.string;
				tlen = trec->data.name.size;
				break;
			case FOREST_TRUST_TOP_LEVEL_NAME_EX:
				ex_rule = true;
				tname = trec->data.name.string;
				tlen = trec->data.name.size;
				break;
			case FOREST_TRUST_DOMAIN_INFO:
				ex_rule = false;
				tname = trec->data.info.dns_name.string;
				tlen = trec->data.info.dns_name.size;
			}
			ret = dns_cmp(dns_name, dns_len, tname, tlen);
			switch (ret) {
			case DNS_CMP_MATCH:
				/* if it matches exclusion,
				 * it doesn't conflict */
				if (ex_rule) {
					exclusion = true;
					break;
				}
				/* fall through */
			case DNS_CMP_FIRST_IS_CHILD:
			case DNS_CMP_SECOND_IS_CHILD:
				tln_conflict = true;
				/* fall through */
			default:
				break;
			}

			/* explicit exclusion, no dns name conflict here */
			if (exclusion) {
				tln_conflict = false;
			}

			if (trec->type != FOREST_TRUST_DOMAIN_INFO) {
				continue;
			}

			/* also test for domain info */
			if (!(trec->flags & LSA_SID_DISABLED_ADMIN) &&
			    dom_sid_compare(&trec->data.info.sid, sid) == 0) {
				sid_conflict = true;
			}
			if (!(trec->flags & LSA_NB_DISABLED_ADMIN) &&
			    strcasecmp_m(trec->data.info.netbios_name.string,
					 nb_name) == 0) {
				nb_conflict = true;
			}
		}

		if (tln_conflict) {
			nt_status = add_collision(c_info, new_fti_idx,
						  collision_type,
						  LSA_TLN_DISABLED_CONFLICT,
						  tdo_name);
		}
		if (sid_conflict) {
			nt_status = add_collision(c_info, new_fti_idx,
						  collision_type,
						  LSA_SID_DISABLED_CONFLICT,
						  tdo_name);
		}
		if (nb_conflict) {
			nt_status = add_collision(c_info, new_fti_idx,
						  collision_type,
						  LSA_NB_DISABLED_CONFLICT,
						  tdo_name);
		}
	}

	return NT_STATUS_OK;
}

static NTSTATUS add_collision(struct lsa_ForestTrustCollisionInfo *c_info,
			      uint32_t idx, uint32_t collision_type,
			      uint32_t conflict_type, const char *tdo_name)
{
	struct lsa_ForestTrustCollisionRecord **es;
	uint32_t i = c_info->count;

	es = talloc_realloc(c_info, c_info->entries,
			    struct lsa_ForestTrustCollisionRecord *, i + 1);
	if (!es) {
		return NT_STATUS_NO_MEMORY;
	}
	c_info->entries = es;
	c_info->count = i + 1;

	es[i] = talloc(es, struct lsa_ForestTrustCollisionRecord);
	if (!es[i]) {
		return NT_STATUS_NO_MEMORY;
	}

	es[i]->index = idx;
	es[i]->type = collision_type;
	es[i]->flags.flags = conflict_type;
	es[i]->name.string = talloc_strdup(es[i], tdo_name);
	if (!es[i]->name.string) {
		return NT_STATUS_NO_MEMORY;
	}
	es[i]->name.size = strlen(es[i]->name.string);

	return NT_STATUS_OK;
}

/*
  lsa_lsaRSetForestTrustInformation
*/
static NTSTATUS dcesrv_lsa_lsaRSetForestTrustInformation(struct dcesrv_call_state *dce_call,
							 TALLOC_CTX *mem_ctx,
							 struct lsa_lsaRSetForestTrustInformation *r)
{
	struct dcesrv_handle *h;
	struct lsa_policy_state *p_state;
	const char *trust_attrs[] = { "trustPartner", "trustAttributes",
				      "msDS-TrustForestTrustInfo", NULL };
	struct ldb_message **dom_res = NULL;
	struct ldb_dn *tdo_dn;
	struct ldb_message *msg;
	int num_res, i;
	const char *td_name;
	uint32_t trust_attributes;
	struct lsa_ForestTrustCollisionInfo *c_info;
	struct ForestTrustInfo *nfti;
	struct ForestTrustInfo *fti;
	DATA_BLOB ft_blob;
	enum ndr_err_code ndr_err;
	NTSTATUS nt_status;
	bool am_rodc;
	int ret;

	DCESRV_PULL_HANDLE(h, r->in.handle, LSA_HANDLE_POLICY);

	p_state = h->data;

	if (strcmp(p_state->domain_dns, p_state->forest_dns)) {
		return NT_STATUS_INVALID_DOMAIN_STATE;
	}

	/* abort if we are not a PDC */
	if (!samdb_is_pdc(p_state->sam_ldb)) {
		return NT_STATUS_INVALID_DOMAIN_ROLE;
	}

	ret = samdb_rodc(p_state->sam_ldb, &am_rodc);
	if (ret == LDB_SUCCESS && am_rodc) {
		return NT_STATUS_NO_SUCH_DOMAIN;
	}

	/* check caller has TRUSTED_SET_AUTH */

	/* fetch all trusted domain objects */
	num_res = gendb_search(p_state->sam_ldb, mem_ctx,
			       p_state->system_dn,
			       &dom_res, trust_attrs,
			       "(objectclass=trustedDomain)");
	if (num_res == 0) {
		return NT_STATUS_NO_SUCH_DOMAIN;
	}

	for (i = 0; i < num_res; i++) {
		td_name = ldb_msg_find_attr_as_string(dom_res[i],
						      "trustPartner", NULL);
		if (!td_name) {
			return NT_STATUS_INVALID_DOMAIN_STATE;
		}
		if (strcasecmp_m(td_name,
				 r->in.trusted_domain_name->string) == 0) {
			break;
		}
	}
	if (i >= num_res) {
		return NT_STATUS_NO_SUCH_DOMAIN;
	}

	tdo_dn = dom_res[i]->dn;

	trust_attributes = ldb_msg_find_attr_as_uint(dom_res[i],
						     "trustAttributes", 0);
	if (!(trust_attributes & NETR_TRUST_ATTRIBUTE_FOREST_TRANSITIVE)) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (r->in.highest_record_type >= LSA_FOREST_TRUST_RECORD_TYPE_LAST) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	nfti = talloc(mem_ctx, struct ForestTrustInfo);
	if (!nfti) {
		return NT_STATUS_NO_MEMORY;
	}

	nt_status = make_ft_info(nfti, r->in.forest_trust_info, nfti);
	if (!NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	}

	c_info = talloc_zero(r->out.collision_info,
			     struct lsa_ForestTrustCollisionInfo);
	if (!c_info) {
		return NT_STATUS_NO_MEMORY;
	}

        /* first check own info, then other domains */
	fti = talloc(mem_ctx, struct ForestTrustInfo);
	if (!fti) {
		return NT_STATUS_NO_MEMORY;
	}

        nt_status = own_ft_info(p_state, fti);
	if (!NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	}

	nt_status = check_ft_info(c_info, p_state->domain_dns,
                                  fti, nfti, c_info);
	if (!NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	}

	for (i = 0; i < num_res; i++) {
		fti = talloc(mem_ctx, struct ForestTrustInfo);
		if (!fti) {
			return NT_STATUS_NO_MEMORY;
		}

		nt_status = get_ft_info(mem_ctx, dom_res[i], fti);
		if (!NT_STATUS_IS_OK(nt_status)) {
			if (NT_STATUS_EQUAL(nt_status,
			    NT_STATUS_OBJECT_NAME_NOT_FOUND)) {
				continue;
			}
			return nt_status;
		}

		td_name = ldb_msg_find_attr_as_string(dom_res[i],
						      "trustPartner", NULL);
		if (!td_name) {
			return NT_STATUS_INVALID_DOMAIN_STATE;
		}

		nt_status = check_ft_info(c_info, td_name, fti, nfti, c_info);
		if (!NT_STATUS_IS_OK(nt_status)) {
			return nt_status;
		}
	}

	*r->out.collision_info = c_info;

	if (r->in.check_only != 0) {
		return NT_STATUS_OK;
	}

	/* not just a check, write info back */

	ndr_err = ndr_push_struct_blob(&ft_blob, mem_ctx, nfti,
				       (ndr_push_flags_fn_t)ndr_push_ForestTrustInfo);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	msg = ldb_msg_new(mem_ctx);
	if (msg == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	msg->dn = ldb_dn_copy(mem_ctx, tdo_dn);
	if (!msg->dn) {
		return NT_STATUS_NO_MEMORY;
	}

	ret = ldb_msg_add_empty(msg, "msDS-TrustForestTrustInfo",
				LDB_FLAG_MOD_REPLACE, NULL);
	if (ret != LDB_SUCCESS) {
		return NT_STATUS_NO_MEMORY;
	}
	ret = ldb_msg_add_value(msg, "msDS-TrustForestTrustInfo",
				&ft_blob, NULL);
	if (ret != LDB_SUCCESS) {
		return NT_STATUS_NO_MEMORY;
	}

	ret = ldb_modify(p_state->sam_ldb, msg);
	if (ret != LDB_SUCCESS) {
		DEBUG(0, ("Failed to store Forest Trust Info: %s\n",
			  ldb_errstring(p_state->sam_ldb)));

		switch (ret) {
		case LDB_ERR_INSUFFICIENT_ACCESS_RIGHTS:
			return NT_STATUS_ACCESS_DENIED;
		default:
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}
	}

	return NT_STATUS_OK;
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
