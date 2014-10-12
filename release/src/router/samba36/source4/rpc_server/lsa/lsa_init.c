/* 
   Unix SMB/CIFS implementation.

   endpoint server for the lsarpc pipe

   Copyright (C) Andrew Tridgell 2004
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2004-2007
   
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

NTSTATUS dcesrv_lsa_get_policy_state(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				     struct lsa_policy_state **_state)
{
	struct lsa_policy_state *state;
	struct ldb_result *dom_res;
	const char *dom_attrs[] = {
		"objectSid", 
		"objectGUID", 
		"nTMixedDomain",
		"fSMORoleOwner",
		NULL
	};
	char *p;
	int ret;

	state = talloc(mem_ctx, struct lsa_policy_state);
	if (!state) {
		return NT_STATUS_NO_MEMORY;
	}

	/* make sure the sam database is accessible */
	state->sam_ldb = samdb_connect(state, dce_call->event_ctx, dce_call->conn->dce_ctx->lp_ctx, dce_call->conn->auth_state.session_info, 0);
	if (state->sam_ldb == NULL) {
		return NT_STATUS_INVALID_SYSTEM_SERVICE;
	}

	/* and the privilege database */
	state->pdb = privilege_connect(state, dce_call->conn->dce_ctx->lp_ctx);
	if (state->pdb == NULL) {
		return NT_STATUS_INVALID_SYSTEM_SERVICE;
	}

	/* work out the domain_dn - useful for so many calls its worth
	   fetching here */
	state->domain_dn = ldb_get_default_basedn(state->sam_ldb);
	if (!state->domain_dn) {
		return NT_STATUS_NO_MEMORY;		
	}

	/* work out the forest root_dn - useful for so many calls its worth
	   fetching here */
	state->forest_dn = ldb_get_root_basedn(state->sam_ldb);
	if (!state->forest_dn) {
		return NT_STATUS_NO_MEMORY;		
	}

	ret = ldb_search(state->sam_ldb, mem_ctx, &dom_res,
			 state->domain_dn, LDB_SCOPE_BASE, dom_attrs, NULL);
	if (ret != LDB_SUCCESS) {
		return NT_STATUS_INVALID_SYSTEM_SERVICE;
	}
	if (dom_res->count != 1) {
		return NT_STATUS_NO_SUCH_DOMAIN;		
	}

	state->domain_sid = samdb_result_dom_sid(state, dom_res->msgs[0], "objectSid");
	if (!state->domain_sid) {
		return NT_STATUS_NO_SUCH_DOMAIN;		
	}

	state->domain_guid = samdb_result_guid(dom_res->msgs[0], "objectGUID");

	state->mixed_domain = ldb_msg_find_attr_as_uint(dom_res->msgs[0], "nTMixedDomain", 0);
	
	talloc_free(dom_res);

	state->domain_name = lpcfg_sam_name(dce_call->conn->dce_ctx->lp_ctx);

	state->domain_dns = ldb_dn_canonical_string(state, state->domain_dn);
	if (!state->domain_dns) {
		return NT_STATUS_NO_SUCH_DOMAIN;		
	}
	p = strchr(state->domain_dns, '/');
	if (p) {
		*p = '\0';
	}

	state->forest_dns = ldb_dn_canonical_string(state, state->forest_dn);
	if (!state->forest_dns) {
		return NT_STATUS_NO_SUCH_DOMAIN;		
	}
	p = strchr(state->forest_dns, '/');
	if (p) {
		*p = '\0';
	}

	/* work out the builtin_dn - useful for so many calls its worth
	   fetching here */
	state->builtin_dn = samdb_search_dn(state->sam_ldb, state, state->domain_dn, "(objectClass=builtinDomain)");
	if (!state->builtin_dn) {
		return NT_STATUS_NO_SUCH_DOMAIN;		
	}

	/* work out the system_dn - useful for so many calls its worth
	   fetching here */
	state->system_dn = samdb_search_dn(state->sam_ldb, state,
					   state->domain_dn, "(&(objectClass=container)(cn=System))");
	if (!state->system_dn) {
		return NT_STATUS_NO_SUCH_DOMAIN;		
	}

	state->builtin_sid = dom_sid_parse_talloc(state, SID_BUILTIN);
	if (!state->builtin_sid) {
		return NT_STATUS_NO_SUCH_DOMAIN;		
	}

	state->nt_authority_sid = dom_sid_parse_talloc(state, SID_NT_AUTHORITY);
	if (!state->nt_authority_sid) {
		return NT_STATUS_NO_SUCH_DOMAIN;		
	}

	state->creator_owner_domain_sid = dom_sid_parse_talloc(state, SID_CREATOR_OWNER_DOMAIN);
	if (!state->creator_owner_domain_sid) {
		return NT_STATUS_NO_SUCH_DOMAIN;		
	}

	state->world_domain_sid = dom_sid_parse_talloc(state, SID_WORLD_DOMAIN);
	if (!state->world_domain_sid) {
		return NT_STATUS_NO_SUCH_DOMAIN;		
	}

	*_state = state;

	return NT_STATUS_OK;
}

/* 
  lsa_OpenPolicy2
*/
NTSTATUS dcesrv_lsa_OpenPolicy2(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				struct lsa_OpenPolicy2 *r)
{
	NTSTATUS status;
	struct lsa_policy_state *state;
	struct dcesrv_handle *handle;

	ZERO_STRUCTP(r->out.handle);

	if (r->in.attr != NULL &&
	    r->in.attr->root_dir != NULL) {
		/* MS-LSAD 3.1.4.4.1 */
		return NT_STATUS_INVALID_PARAMETER;
	}

	status = dcesrv_lsa_get_policy_state(dce_call, mem_ctx, &state);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	handle = dcesrv_handle_new(dce_call->context, LSA_HANDLE_POLICY);
	if (!handle) {
		return NT_STATUS_NO_MEMORY;
	}

	handle->data = talloc_steal(handle, state);

	/* need to check the access mask against - need ACLs - fails
	   WSPP test */
	state->access_mask = r->in.access_mask;
	state->handle = handle;
	*r->out.handle = handle->wire_handle;

	/* note that we have completely ignored the attr element of
	   the OpenPolicy. As far as I can tell, this is what w2k3
	   does */

	return NT_STATUS_OK;
}

/* 
  lsa_OpenPolicy
  a wrapper around lsa_OpenPolicy2
*/
NTSTATUS dcesrv_lsa_OpenPolicy(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				struct lsa_OpenPolicy *r)
{
	struct lsa_OpenPolicy2 r2;

	r2.in.system_name = NULL;
	r2.in.attr = r->in.attr;
	r2.in.access_mask = r->in.access_mask;
	r2.out.handle = r->out.handle;

	return dcesrv_lsa_OpenPolicy2(dce_call, mem_ctx, &r2);
}


