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
#include "libds/common/flag_mapping.h"

static const struct {
	const char *domain;
	const char *name;
	const char *sid;
	enum lsa_SidType rtype;
} well_known[] = {
	{
		.name = "EVERYONE",
		.sid = SID_WORLD,
		.rtype = SID_NAME_WKN_GRP,
	},
	{
		.name = "CREATOR OWNER",
		.sid = SID_CREATOR_OWNER,
		.rtype = SID_NAME_WKN_GRP,
	},
	{
		.name = "CREATOR GROUP",
		.sid = SID_CREATOR_GROUP,
		.rtype = SID_NAME_WKN_GRP,
	},
	{
		.name = "Owner Rights",
		.sid = SID_OWNER_RIGHTS,
		.rtype = SID_NAME_WKN_GRP,
	},
	{
		.domain = "NT AUTHORITY",
		.name = "Dialup",
		.sid = SID_NT_DIALUP,
		.rtype = SID_NAME_WKN_GRP,
	},
	{
		.domain = "NT AUTHORITY",
		.name = "Network",
		.sid = SID_NT_NETWORK,
		.rtype = SID_NAME_WKN_GRP,
	},
	{
		.domain = "NT AUTHORITY",
		.name = "Batch",
		.sid = SID_NT_BATCH,
		.rtype = SID_NAME_WKN_GRP,
	},
	{
		.domain = "NT AUTHORITY",
		.name = "Interactive",
		.sid = SID_NT_INTERACTIVE,
		.rtype = SID_NAME_WKN_GRP,
	},
	{
		.domain = "NT AUTHORITY",
		.name = "Service",
		.sid = SID_NT_SERVICE,
		.rtype = SID_NAME_WKN_GRP,
	},
	{
		.domain = "NT AUTHORITY",
		.name = "ANONYMOUS LOGON",
		.sid = SID_NT_ANONYMOUS,
		.rtype = SID_NAME_WKN_GRP,
	},
	{
		.domain = "NT AUTHORITY",
		.name = "Proxy",
		.sid = SID_NT_PROXY,
		.rtype = SID_NAME_WKN_GRP,
	},
	{
		.domain = "NT AUTHORITY",
		.name = "ServerLogon",
		.sid = SID_NT_ENTERPRISE_DCS,
		.rtype = SID_NAME_WKN_GRP,
	},
	{
		.domain = "NT AUTHORITY",
		.name = "Self",
		.sid = SID_NT_SELF,
		.rtype = SID_NAME_WKN_GRP,
	},
	{
		.domain = "NT AUTHORITY",
		.name = "Authenticated Users",
		.sid = SID_NT_AUTHENTICATED_USERS,
		.rtype = SID_NAME_WKN_GRP,
	},
	{
		.domain = "NT AUTHORITY",
		.name = "Restricted",
		.sid = SID_NT_RESTRICTED,
		.rtype = SID_NAME_WKN_GRP,
	},
	{
		.domain = "NT AUTHORITY",
		.name = "Terminal Server User",
		.sid = SID_NT_TERMINAL_SERVER_USERS,
		.rtype = SID_NAME_WKN_GRP,
	},
	{
		.domain = "NT AUTHORITY",
		.name = "Remote Interactive Logon",
		.sid = SID_NT_REMOTE_INTERACTIVE,
		.rtype = SID_NAME_WKN_GRP,
	},
	{
		.domain = "NT AUTHORITY",
		.name = "This Organization",
		.sid = SID_NT_THIS_ORGANISATION,
		.rtype = SID_NAME_WKN_GRP,
	},
	{
		.domain = "NT AUTHORITY",
		.name = "SYSTEM",
		.sid = SID_NT_SYSTEM,
		.rtype = SID_NAME_WKN_GRP,
	},
	{
		.domain = "NT AUTHORITY",
		.name = "Local Service",
		.sid = SID_NT_LOCAL_SERVICE,
		.rtype = SID_NAME_WKN_GRP,
	},
	{
		.domain = "NT AUTHORITY",
		.name = "Network Service",
		.sid = SID_NT_NETWORK_SERVICE,
		.rtype = SID_NAME_WKN_GRP,
	},
	{
		.domain = "NT AUTHORITY",
		.name = "Digest Authentication",
		.sid = SID_NT_DIGEST_AUTHENTICATION,
		.rtype = SID_NAME_WKN_GRP,
	},
	{
		.domain = "NT AUTHORITY",
		.name = "Enterprise Domain Controllers",
		.sid = SID_NT_ENTERPRISE_DCS,
		.rtype = SID_NAME_WKN_GRP,
	},
	{
		.domain = "NT AUTHORITY",
		.name = "NTLM Authentication",
		.sid = SID_NT_NTLM_AUTHENTICATION,
		.rtype = SID_NAME_WKN_GRP,
	},
	{
		.domain = "NT AUTHORITY",
		.name = "Other Organization",
		.sid = SID_NT_OTHER_ORGANISATION,
		.rtype = SID_NAME_WKN_GRP,
	},
	{
		.domain = "NT AUTHORITY",
		.name = "SChannel Authentication",
		.sid = SID_NT_SCHANNEL_AUTHENTICATION,
		.rtype = SID_NAME_WKN_GRP,
	},
	{
		.domain = "NT AUTHORITY",
		.name = "IUSR",
		.sid = SID_NT_IUSR,
		.rtype = SID_NAME_WKN_GRP,
	},
	{
		.sid = NULL,
	}
};

static NTSTATUS lookup_well_known_names(TALLOC_CTX *mem_ctx, const char *domain,
					const char *name, const char **authority_name, 
					struct dom_sid **sid, enum lsa_SidType *rtype)
{
	unsigned int i;
	for (i=0; well_known[i].sid; i++) {
		if (domain) {
			if (strcasecmp_m(domain, well_known[i].domain) == 0
			    && strcasecmp_m(name, well_known[i].name) == 0) {
				*authority_name = well_known[i].domain;
				*sid = dom_sid_parse_talloc(mem_ctx, well_known[i].sid);
				*rtype = well_known[i].rtype;
				return NT_STATUS_OK;
			}
		} else {
			if (strcasecmp_m(name, well_known[i].name) == 0) {
				*authority_name = well_known[i].domain;
				*sid = dom_sid_parse_talloc(mem_ctx, well_known[i].sid);
				*rtype = well_known[i].rtype;
				return NT_STATUS_OK;
			}
		}
	}
	return NT_STATUS_NOT_FOUND;	
}

static NTSTATUS lookup_well_known_sids(TALLOC_CTX *mem_ctx, 
				       const char *sid_str, const char **authority_name, 
				       const char **name, enum lsa_SidType *rtype) 
{
	unsigned int i;
	for (i=0; well_known[i].sid; i++) {
		if (strcasecmp_m(sid_str, well_known[i].sid) == 0) {
			*authority_name = well_known[i].domain;
			*name = well_known[i].name;
			*rtype = well_known[i].rtype;
			return NT_STATUS_OK;
		}
	}
	return NT_STATUS_NOT_FOUND;	
}

/*
  lookup a SID for 1 name
*/
static NTSTATUS dcesrv_lsa_lookup_name(struct tevent_context *ev_ctx, 
				       struct loadparm_context *lp_ctx,
				       struct lsa_policy_state *state, TALLOC_CTX *mem_ctx,
				       const char *name, const char **authority_name, 
				       struct dom_sid **sid, enum lsa_SidType *rtype,
				       uint32_t *rid)
{
	int ret, i;
	uint32_t atype;
	struct ldb_message **res;
	const char * const attrs[] = { "objectSid", "sAMAccountType", NULL};
	const char *p;
	const char *domain;
	const char *username;
	struct ldb_dn *domain_dn;
	struct dom_sid *domain_sid;
	NTSTATUS status;

	p = strchr_m(name, '\\');
	if (p != NULL) {
		domain = talloc_strndup(mem_ctx, name, p-name);
		if (!domain) {
			return NT_STATUS_NO_MEMORY;
		}
		username = p + 1;
	} else if (strchr_m(name, '@')) {
		status = crack_name_to_nt4_name(mem_ctx, ev_ctx, lp_ctx, DRSUAPI_DS_NAME_FORMAT_USER_PRINCIPAL, name, &domain, &username);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(3, ("Failed to crack name %s into an NT4 name: %s\n", name, nt_errstr(status)));
			return status;
		}
	} else {
		domain = NULL;
		username = name;
	}
	
	if (!domain) {
		/* Look up table of well known names */
		status = lookup_well_known_names(mem_ctx, NULL, username, authority_name, sid, rtype);
		if (NT_STATUS_IS_OK(status)) {
			dom_sid_split_rid(NULL, *sid, NULL, rid);
			return NT_STATUS_OK;
		}

		if (username == NULL) {
			*authority_name = NAME_BUILTIN;
			*sid = dom_sid_parse_talloc(mem_ctx, SID_BUILTIN);
			*rtype = SID_NAME_DOMAIN;
			*rid = 0xFFFFFFFF;
			return NT_STATUS_OK;
		}

		if (strcasecmp_m(username, NAME_NT_AUTHORITY) == 0) { 
			*authority_name = NAME_NT_AUTHORITY;
			*sid =  dom_sid_parse_talloc(mem_ctx, SID_NT_AUTHORITY);
			*rtype = SID_NAME_DOMAIN;
			dom_sid_split_rid(NULL, *sid, NULL, rid);
			return NT_STATUS_OK;
		}
		if (strcasecmp_m(username, NAME_BUILTIN) == 0) { 
			*authority_name = NAME_BUILTIN;
			*sid = dom_sid_parse_talloc(mem_ctx, SID_BUILTIN);
			*rtype = SID_NAME_DOMAIN;
			*rid = 0xFFFFFFFF;
			return NT_STATUS_OK;
		}
		if (strcasecmp_m(username, state->domain_dns) == 0) { 
			*authority_name = state->domain_name;
			*sid =  state->domain_sid;
			*rtype = SID_NAME_DOMAIN;
			*rid = 0xFFFFFFFF;
			return NT_STATUS_OK;
		}
		if (strcasecmp_m(username, state->domain_name) == 0) { 
			*authority_name = state->domain_name;
			*sid =  state->domain_sid;
			*rtype = SID_NAME_DOMAIN;
			*rid = 0xFFFFFFFF;
			return NT_STATUS_OK;
		}
		
		/* Perhaps this is a well known user? */
		name = talloc_asprintf(mem_ctx, "%s\\%s", NAME_NT_AUTHORITY, username);
		if (!name) {
			return NT_STATUS_NO_MEMORY;
		}
		status = dcesrv_lsa_lookup_name(ev_ctx, lp_ctx, state, mem_ctx, name, authority_name, sid, rtype, rid);
		if (NT_STATUS_IS_OK(status)) {
			return status;
		}

		/* Perhaps this is a BUILTIN user? */
		name = talloc_asprintf(mem_ctx, "%s\\%s", NAME_BUILTIN, username);
		if (!name) {
			return NT_STATUS_NO_MEMORY;
		}
		status = dcesrv_lsa_lookup_name(ev_ctx, lp_ctx, state, mem_ctx, name, authority_name, sid, rtype, rid);
		if (NT_STATUS_IS_OK(status)) {
			return status;
		}

		/* OK, I give up - perhaps we need to assume the user is in our domain? */
		name = talloc_asprintf(mem_ctx, "%s\\%s", state->domain_name, username);
		if (!name) {
			return NT_STATUS_NO_MEMORY;
		}
		status = dcesrv_lsa_lookup_name(ev_ctx, lp_ctx, state, mem_ctx, name, authority_name, sid, rtype, rid);
		if (NT_STATUS_IS_OK(status)) {
			return status;
		}

		return STATUS_SOME_UNMAPPED;
	} else if (strcasecmp_m(domain, NAME_NT_AUTHORITY) == 0) {
		if (!*username) {
			*authority_name = NAME_NT_AUTHORITY;
			*sid = dom_sid_parse_talloc(mem_ctx, SID_NT_AUTHORITY);
			*rtype = SID_NAME_DOMAIN;
			dom_sid_split_rid(NULL, *sid, NULL, rid);
			return NT_STATUS_OK;
		}

		/* Look up table of well known names */
		status = lookup_well_known_names(mem_ctx, domain, username, authority_name, 
						 sid, rtype);
		if (NT_STATUS_IS_OK(status)) {
			dom_sid_split_rid(NULL, *sid, NULL, rid);
		}
		return status;
	} else if (strcasecmp_m(domain, NAME_BUILTIN) == 0) {
		*authority_name = NAME_BUILTIN;
		domain_dn = state->builtin_dn;
	} else if (strcasecmp_m(domain, state->domain_dns) == 0) { 
		*authority_name = state->domain_name;
		domain_dn = state->domain_dn;
	} else if (strcasecmp_m(domain, state->domain_name) == 0) { 
		*authority_name = state->domain_name;
		domain_dn = state->domain_dn;
	} else {
		/* Not local, need to ask winbind in future */
		return STATUS_SOME_UNMAPPED;
	}

	ret = gendb_search_dn(state->sam_ldb, mem_ctx, domain_dn, &res, attrs);
	if (ret != 1) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}
	domain_sid = samdb_result_dom_sid(mem_ctx, res[0], "objectSid");
	if (domain_sid == NULL) {
		return NT_STATUS_INVALID_SID;
	}

	if (!*username) {
		*sid = domain_sid;
		*rtype = SID_NAME_DOMAIN;
		*rid = 0xFFFFFFFF;
		return NT_STATUS_OK;
	}
	
	ret = gendb_search(state->sam_ldb, mem_ctx, domain_dn, &res, attrs, 
			   "(&(sAMAccountName=%s)(objectSid=*))", 
			   ldb_binary_encode_string(mem_ctx, username));
	if (ret < 0) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	for (i=0; i < ret; i++) {
		*sid = samdb_result_dom_sid(mem_ctx, res[i], "objectSid");
		if (*sid == NULL) {
			return NT_STATUS_INVALID_SID;
		}

		/* Check that this is in the domain */
		if (!dom_sid_in_domain(domain_sid, *sid)) {
			continue;
		}

		atype = ldb_msg_find_attr_as_uint(res[i], "sAMAccountType", 0);
			
		*rtype = ds_atype_map(atype);
		if (*rtype == SID_NAME_UNKNOWN) {
			return STATUS_SOME_UNMAPPED;
		}

		dom_sid_split_rid(NULL, *sid, NULL, rid);
		return NT_STATUS_OK;
	}

	/* need to check for an allocated sid */

	return NT_STATUS_INVALID_SID;
}


/*
  add to the lsa_RefDomainList for LookupSids and LookupNames
*/
static NTSTATUS dcesrv_lsa_authority_list(struct lsa_policy_state *state, TALLOC_CTX *mem_ctx, 
					  enum lsa_SidType rtype,
					  const char *authority_name,
					  struct dom_sid *sid, 
					  struct lsa_RefDomainList *domains,
					  uint32_t *sid_index)
{
	struct dom_sid *authority_sid;
	uint32_t i;

	if (rtype != SID_NAME_DOMAIN) {
		authority_sid = dom_sid_dup(mem_ctx, sid);
		if (authority_sid == NULL) {
			return NT_STATUS_NO_MEMORY;
		}
		authority_sid->num_auths--;
	} else {
		authority_sid = sid;
	}

	/* see if we've already done this authority name */
	for (i=0;i<domains->count;i++) {
		if (strcasecmp_m(authority_name, domains->domains[i].name.string) == 0) {
			*sid_index = i;
			return NT_STATUS_OK;
		}
	}

	domains->domains = talloc_realloc(domains, 
					  domains->domains,
					  struct lsa_DomainInfo,
					  domains->count+1);
	if (domains->domains == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	domains->domains[i].name.string = authority_name;
	domains->domains[i].sid         = authority_sid;
	domains->count++;
	domains->max_size = LSA_REF_DOMAIN_LIST_MULTIPLIER * domains->count;
	*sid_index = i;
	
	return NT_STATUS_OK;
}

/*
  lookup a name for 1 SID
*/
static NTSTATUS dcesrv_lsa_lookup_sid(struct lsa_policy_state *state, TALLOC_CTX *mem_ctx,
				      struct dom_sid *sid, const char *sid_str,
				      const char **authority_name, 
				      const char **name, enum lsa_SidType *rtype)
{
	NTSTATUS status;
	int ret;
	uint32_t atype;
	struct ldb_message **res;
	struct ldb_dn *domain_dn;
	const char * const attrs[] = { "sAMAccountName", "sAMAccountType", "cn", NULL};

	status = lookup_well_known_sids(mem_ctx, sid_str, authority_name, name, rtype);
	if (NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (dom_sid_in_domain(state->domain_sid, sid)) {
		*authority_name = state->domain_name;
		domain_dn = state->domain_dn;
	} else if (dom_sid_in_domain(state->builtin_sid, sid)) {
		*authority_name = NAME_BUILTIN;
		domain_dn = state->builtin_dn;
	} else {
		/* Not well known, our domain or built in */

		/* In future, we must look at SID histories, and at trusted domains via winbind */

		return NT_STATUS_NOT_FOUND;
	}

	/* need to re-add a check for an allocated sid */

	ret = gendb_search(state->sam_ldb, mem_ctx, domain_dn, &res, attrs, 
			   "objectSid=%s", ldap_encode_ndr_dom_sid(mem_ctx, sid));
	if ((ret < 0) || (ret > 1)) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}
	if (ret == 0) {
		return NT_STATUS_NOT_FOUND;
	}

	*name = ldb_msg_find_attr_as_string(res[0], "sAMAccountName", NULL);
	if (!*name) {
		*name = ldb_msg_find_attr_as_string(res[0], "cn", NULL);
		if (!*name) {
			*name = talloc_strdup(mem_ctx, sid_str);
			NT_STATUS_HAVE_NO_MEMORY(*name);
		}
	}

	atype = ldb_msg_find_attr_as_uint(res[0], "sAMAccountType", 0);
	*rtype = ds_atype_map(atype);

	return NT_STATUS_OK;
}


/*
  lsa_LookupSids2
*/
NTSTATUS dcesrv_lsa_LookupSids2(struct dcesrv_call_state *dce_call,
				TALLOC_CTX *mem_ctx,
				struct lsa_LookupSids2 *r)
{
	struct lsa_policy_state *state;
	struct lsa_RefDomainList *domains = NULL;
	uint32_t i;
	NTSTATUS status = NT_STATUS_OK;

	if (r->in.level < LSA_LOOKUP_NAMES_ALL ||
	    r->in.level > LSA_LOOKUP_NAMES_RODC_REFERRAL_TO_FULL_DC) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	*r->out.domains = NULL;

	/* NOTE: the WSPP test suite tries SIDs with invalid revision numbers,
	   and expects NT_STATUS_INVALID_PARAMETER back - we just treat it as 
	   an unknown SID. We could add a SID validator here. (tridge) 
	   MS-DTYP 2.4.2
	*/

	status = dcesrv_lsa_get_policy_state(dce_call, mem_ctx, &state);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	domains = talloc_zero(r->out.domains,  struct lsa_RefDomainList);
	if (domains == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	*r->out.domains = domains;

	r->out.names = talloc_zero(mem_ctx,  struct lsa_TransNameArray2);
	if (r->out.names == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	*r->out.count = 0;

	r->out.names->names = talloc_array(r->out.names, struct lsa_TranslatedName2, 
					     r->in.sids->num_sids);
	if (r->out.names->names == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	for (i=0;i<r->in.sids->num_sids;i++) {
		struct dom_sid *sid = r->in.sids->sids[i].sid;
		char *sid_str = dom_sid_string(mem_ctx, sid);
		const char *name, *authority_name;
		enum lsa_SidType rtype;
		uint32_t sid_index;
		NTSTATUS status2;

		r->out.names->count++;

		r->out.names->names[i].sid_type    = SID_NAME_UNKNOWN;
		r->out.names->names[i].name.string = sid_str;
		r->out.names->names[i].sid_index   = 0xFFFFFFFF;
		r->out.names->names[i].unknown     = 0;

		if (sid_str == NULL) {
			r->out.names->names[i].name.string = "(SIDERROR)";
			status = STATUS_SOME_UNMAPPED;
			continue;
		}

		status2 = dcesrv_lsa_lookup_sid(state, mem_ctx, sid, sid_str, 
						&authority_name, &name, &rtype);
		if (!NT_STATUS_IS_OK(status2)) {
			status = STATUS_SOME_UNMAPPED;
			continue;
		}

		/* set up the authority table */
		status2 = dcesrv_lsa_authority_list(state, mem_ctx, rtype, 
						    authority_name, sid, 
						    domains, &sid_index);
		if (!NT_STATUS_IS_OK(status2)) {
			continue;
		}

		r->out.names->names[i].sid_type    = rtype;
		r->out.names->names[i].name.string = name;
		r->out.names->names[i].sid_index   = sid_index;
		r->out.names->names[i].unknown     = 0;

		(*r->out.count)++;
	}

	if (*r->out.count == 0) {
		return NT_STATUS_NONE_MAPPED;
	}
	if (*r->out.count != r->in.sids->num_sids) {
		return STATUS_SOME_UNMAPPED;
	}

	return NT_STATUS_OK;
}


/*
  lsa_LookupSids3

  Identical to LookupSids2, but doesn't take a policy handle
  
*/
NTSTATUS dcesrv_lsa_LookupSids3(struct dcesrv_call_state *dce_call,
				TALLOC_CTX *mem_ctx,
				struct lsa_LookupSids3 *r)
{
	struct lsa_LookupSids2 r2;
	struct lsa_OpenPolicy2 pol;
	NTSTATUS status;
	struct dcesrv_handle *h;

	ZERO_STRUCT(r2);
	
	/* No policy handle on the wire, so make one up here */
	r2.in.handle = talloc(mem_ctx, struct policy_handle);
	if (!r2.in.handle) {
		return NT_STATUS_NO_MEMORY;
	}

	pol.out.handle = r2.in.handle;
	pol.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	pol.in.attr = NULL;
	pol.in.system_name = NULL;
	status = dcesrv_lsa_OpenPolicy2(dce_call, mem_ctx, &pol);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	/* ensure this handle goes away at the end of this call */
	DCESRV_PULL_HANDLE(h, r2.in.handle, LSA_HANDLE_POLICY);
	talloc_steal(mem_ctx, h);

	r2.in.sids     = r->in.sids;
	r2.in.names    = r->in.names;
	r2.in.level    = r->in.level;
	r2.in.count    = r->in.count;
	r2.in.lookup_options = r->in.lookup_options;
	r2.in.client_revision = r->in.client_revision;
	r2.out.count   = r->out.count;
	r2.out.names   = r->out.names;
	r2.out.domains = r->out.domains;

	status = dcesrv_lsa_LookupSids2(dce_call, mem_ctx, &r2);

	r->out.domains = r2.out.domains;
	r->out.names   = r2.out.names;
	r->out.count   = r2.out.count;

	return status;
}


/* 
  lsa_LookupSids 
*/
NTSTATUS dcesrv_lsa_LookupSids(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
			       struct lsa_LookupSids *r)
{
	struct lsa_LookupSids2 r2;
	NTSTATUS status;
	uint32_t i;

	ZERO_STRUCT(r2);

	r2.in.handle   = r->in.handle;
	r2.in.sids     = r->in.sids;
	r2.in.names    = NULL;
	r2.in.level    = r->in.level;
	r2.in.count    = r->in.count;
	r2.in.lookup_options = 0;
	r2.in.client_revision = 0;
	r2.out.count   = r->out.count;
	r2.out.names   = NULL;
	r2.out.domains = r->out.domains;

	status = dcesrv_lsa_LookupSids2(dce_call, mem_ctx, &r2);
	/* we deliberately don't check for error from the above,
	   as even on error we are supposed to return the names  */

	r->out.domains = r2.out.domains;
	if (!r2.out.names) {
		r->out.names = NULL;
		return status;
	}

	r->out.names = talloc(mem_ctx, struct lsa_TransNameArray);
	if (r->out.names == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	r->out.names->count = r2.out.names->count;
	r->out.names->names = talloc_array(r->out.names, struct lsa_TranslatedName, 
					     r->out.names->count);
	if (r->out.names->names == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	for (i=0;i<r->out.names->count;i++) {
		r->out.names->names[i].sid_type    = r2.out.names->names[i].sid_type;
		r->out.names->names[i].name.string = r2.out.names->names[i].name.string;
		r->out.names->names[i].sid_index   = r2.out.names->names[i].sid_index;
	}

	return status;
}


/*
  lsa_LookupNames3
*/
NTSTATUS dcesrv_lsa_LookupNames3(struct dcesrv_call_state *dce_call,
				 TALLOC_CTX *mem_ctx,
				 struct lsa_LookupNames3 *r)
{
	struct lsa_policy_state *policy_state;
	struct dcesrv_handle *policy_handle;
	uint32_t i;
	struct loadparm_context *lp_ctx = dce_call->conn->dce_ctx->lp_ctx;
	struct lsa_RefDomainList *domains;

	DCESRV_PULL_HANDLE(policy_handle, r->in.handle, LSA_HANDLE_POLICY);

	if (r->in.level < LSA_LOOKUP_NAMES_ALL ||
	    r->in.level > LSA_LOOKUP_NAMES_RODC_REFERRAL_TO_FULL_DC) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	policy_state = policy_handle->data;

	*r->out.domains = NULL;

	domains = talloc_zero(mem_ctx,  struct lsa_RefDomainList);
	if (domains == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	*r->out.domains = domains;

	r->out.sids = talloc_zero(mem_ctx,  struct lsa_TransSidArray3);
	if (r->out.sids == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	*r->out.count = 0;

	r->out.sids->sids = talloc_array(r->out.sids, struct lsa_TranslatedSid3, 
					   r->in.num_names);
	if (r->out.sids->sids == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	for (i=0;i<r->in.num_names;i++) {
		const char *name = r->in.names[i].string;
		const char *authority_name;
		struct dom_sid *sid;
		uint32_t sid_index, rid;
		enum lsa_SidType rtype;
		NTSTATUS status2;

		r->out.sids->count++;

		r->out.sids->sids[i].sid_type    = SID_NAME_UNKNOWN;
		r->out.sids->sids[i].sid         = NULL;
		r->out.sids->sids[i].sid_index   = 0xFFFFFFFF;
		r->out.sids->sids[i].flags       = 0;

		status2 = dcesrv_lsa_lookup_name(dce_call->event_ctx, lp_ctx, policy_state, mem_ctx, name,
						 &authority_name, &sid, &rtype, &rid);
		if (!NT_STATUS_IS_OK(status2) || sid->num_auths == 0) {
			continue;
		}

		status2 = dcesrv_lsa_authority_list(policy_state, mem_ctx, rtype, authority_name, 
						    sid, domains, &sid_index);
		if (!NT_STATUS_IS_OK(status2)) {
			continue;
		}

		r->out.sids->sids[i].sid_type    = rtype;
		r->out.sids->sids[i].sid         = sid;
		r->out.sids->sids[i].sid_index   = sid_index;
		r->out.sids->sids[i].flags       = 0;

		(*r->out.count)++;
	}
	
	if (*r->out.count == 0) {
		return NT_STATUS_NONE_MAPPED;
	}
	if (*r->out.count != r->in.num_names) {
		return STATUS_SOME_UNMAPPED;
	}

	return NT_STATUS_OK;
}

/* 
  lsa_LookupNames4

  Identical to LookupNames3, but doesn't take a policy handle
  
*/
NTSTATUS dcesrv_lsa_LookupNames4(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				 struct lsa_LookupNames4 *r)
{
	struct lsa_LookupNames3 r2;
	struct lsa_OpenPolicy2 pol;
	NTSTATUS status;
	struct dcesrv_handle *h;

	ZERO_STRUCT(r2);

	/* No policy handle on the wire, so make one up here */
	r2.in.handle = talloc(mem_ctx, struct policy_handle);
	if (!r2.in.handle) {
		return NT_STATUS_NO_MEMORY;
	}

	pol.out.handle = r2.in.handle;
	pol.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	pol.in.attr = NULL;
	pol.in.system_name = NULL;
	status = dcesrv_lsa_OpenPolicy2(dce_call, mem_ctx, &pol);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	/* ensure this handle goes away at the end of this call */
	DCESRV_PULL_HANDLE(h, r2.in.handle, LSA_HANDLE_POLICY);
	talloc_steal(mem_ctx, h);

	r2.in.num_names = r->in.num_names;
	r2.in.names = r->in.names;
	r2.in.level = r->in.level;
	r2.in.sids = r->in.sids;
	r2.in.count = r->in.count;
	r2.in.lookup_options = r->in.lookup_options;
	r2.in.client_revision = r->in.client_revision;
	r2.out.domains = r->out.domains;
	r2.out.sids = r->out.sids;
	r2.out.count = r->out.count;
	
	status = dcesrv_lsa_LookupNames3(dce_call, mem_ctx, &r2);
	
	r->out.domains = r2.out.domains;
	r->out.sids = r2.out.sids;
	r->out.count = r2.out.count;
	return status;
}

/*
  lsa_LookupNames2
*/
NTSTATUS dcesrv_lsa_LookupNames2(struct dcesrv_call_state *dce_call,
				 TALLOC_CTX *mem_ctx,
				 struct lsa_LookupNames2 *r)
{
	struct lsa_policy_state *state;
	struct dcesrv_handle *h;
	uint32_t i;
	struct loadparm_context *lp_ctx = dce_call->conn->dce_ctx->lp_ctx;
	struct lsa_RefDomainList *domains;

	*r->out.domains = NULL;

	DCESRV_PULL_HANDLE(h, r->in.handle, LSA_HANDLE_POLICY);

	if (r->in.level < LSA_LOOKUP_NAMES_ALL ||
	    r->in.level > LSA_LOOKUP_NAMES_RODC_REFERRAL_TO_FULL_DC) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	state = h->data;

	domains = talloc_zero(mem_ctx,  struct lsa_RefDomainList);
	if (domains == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	*r->out.domains = domains;

	r->out.sids = talloc_zero(mem_ctx,  struct lsa_TransSidArray2);
	if (r->out.sids == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	*r->out.count = 0;

	r->out.sids->sids = talloc_array(r->out.sids, struct lsa_TranslatedSid2, 
					   r->in.num_names);
	if (r->out.sids->sids == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	for (i=0;i<r->in.num_names;i++) {
		const char *name = r->in.names[i].string;
		const char *authority_name;
		struct dom_sid *sid;
		uint32_t sid_index, rid=0;
		enum lsa_SidType rtype;
		NTSTATUS status2;

		r->out.sids->count++;

		r->out.sids->sids[i].sid_type    = SID_NAME_UNKNOWN;
		/* MS-LSAT 3.1.4.7 - rid zero is considered equivalent
		   to sid NULL - so we should return 0 rid for
		   unmapped entries */
		r->out.sids->sids[i].rid         = 0;
		r->out.sids->sids[i].sid_index   = 0xFFFFFFFF;
		r->out.sids->sids[i].unknown     = 0;

		status2 = dcesrv_lsa_lookup_name(dce_call->event_ctx, lp_ctx, state, mem_ctx, name,
						 &authority_name, &sid, &rtype, &rid);
		if (!NT_STATUS_IS_OK(status2)) {
			continue;
		}

		status2 = dcesrv_lsa_authority_list(state, mem_ctx, rtype, authority_name, 
						    sid, domains, &sid_index);
		if (!NT_STATUS_IS_OK(status2)) {
			continue;
		}

		r->out.sids->sids[i].sid_type    = rtype;
		r->out.sids->sids[i].rid         = rid;
		r->out.sids->sids[i].sid_index   = sid_index;
		r->out.sids->sids[i].unknown     = 0;

		(*r->out.count)++;
	}
	
	if (*r->out.count == 0) {
		return NT_STATUS_NONE_MAPPED;
	}
	if (*r->out.count != r->in.num_names) {
		return STATUS_SOME_UNMAPPED;
	}

	return NT_STATUS_OK;
}

/* 
  lsa_LookupNames 
*/
NTSTATUS dcesrv_lsa_LookupNames(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct lsa_LookupNames *r)
{
	struct lsa_LookupNames2 r2;
	NTSTATUS status;
	uint32_t i;

	ZERO_STRUCT(r2);

	r2.in.handle    = r->in.handle;
	r2.in.num_names = r->in.num_names;
	r2.in.names     = r->in.names;
	r2.in.sids      = NULL;
	r2.in.level     = r->in.level;
	r2.in.count     = r->in.count;
	r2.in.lookup_options = 0;
	r2.in.client_revision = 0;
	r2.out.count    = r->out.count;
	r2.out.domains	= r->out.domains;

	status = dcesrv_lsa_LookupNames2(dce_call, mem_ctx, &r2);
	if (r2.out.sids == NULL) {
		return status;
	}

	r->out.sids = talloc(mem_ctx, struct lsa_TransSidArray);
	if (r->out.sids == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	r->out.sids->count = r2.out.sids->count;
	r->out.sids->sids = talloc_array(r->out.sids, struct lsa_TranslatedSid, 
					   r->out.sids->count);
	if (r->out.sids->sids == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	for (i=0;i<r->out.sids->count;i++) {
		r->out.sids->sids[i].sid_type    = r2.out.sids->sids[i].sid_type;
		r->out.sids->sids[i].rid         = r2.out.sids->sids[i].rid;
		r->out.sids->sids[i].sid_index   = r2.out.sids->sids[i].sid_index;
	}

	return status;
}

