/*
 *  Unix SMB/CIFS implementation.
 *  RPC Pipe client / server routines
 *  Copyright (C) Andrew Tridgell              1992-1997,
 *  Copyright (C) Luke Kenneth Casson Leighton 1996-1997,
 *  Copyright (C) Paul Ashton                       1997,
 *  Copyright (C) Jeremy Allison                    2001, 2006.
 *  Copyright (C) Rafal Szczesniak                  2002,
 *  Copyright (C) Jim McDonough <jmcd@us.ibm.com>   2002,
 *  Copyright (C) Simo Sorce                        2003.
 *  Copyright (C) Gerald (Jerry) Carter             2005.
 *  Copyright (C) Volker Lendecke                   2005.
 *  Copyright (C) Guenther Deschner		    2008.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/* This is the implementation of the lsa server code. */

#include "includes.h"
#include "../librpc/gen_ndr/srv_lsa.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_RPC_SRV

#define MAX_LOOKUP_SIDS 0x5000 /* 20480 */

extern PRIVS privs[];

enum lsa_handle_type { LSA_HANDLE_POLICY_TYPE = 1, LSA_HANDLE_ACCOUNT_TYPE };

struct lsa_info {
	DOM_SID sid;
	const char *name;
	uint32 access;
	enum lsa_handle_type type;
	struct security_descriptor *sd;
};

const struct generic_mapping lsa_account_mapping = {
	LSA_ACCOUNT_READ,
	LSA_ACCOUNT_WRITE,
	LSA_ACCOUNT_EXECUTE,
	LSA_ACCOUNT_ALL_ACCESS
};

const struct generic_mapping lsa_policy_mapping = {
	LSA_POLICY_READ,
	LSA_POLICY_WRITE,
	LSA_POLICY_EXECUTE,
	LSA_POLICY_ALL_ACCESS
};

const struct generic_mapping lsa_secret_mapping = {
	LSA_SECRET_READ,
	LSA_SECRET_WRITE,
	LSA_SECRET_EXECUTE,
	LSA_SECRET_ALL_ACCESS
};

const struct generic_mapping lsa_trusted_domain_mapping = {
	LSA_TRUSTED_DOMAIN_READ,
	LSA_TRUSTED_DOMAIN_WRITE,
	LSA_TRUSTED_DOMAIN_EXECUTE,
	LSA_TRUSTED_DOMAIN_ALL_ACCESS
};

/***************************************************************************
 init_lsa_ref_domain_list - adds a domain if it's not already in, returns the index.
***************************************************************************/

static int init_lsa_ref_domain_list(TALLOC_CTX *mem_ctx,
				    struct lsa_RefDomainList *ref,
				    const char *dom_name,
				    DOM_SID *dom_sid)
{
	int num = 0;

	if (dom_name != NULL) {
		for (num = 0; num < ref->count; num++) {
			if (sid_equal(dom_sid, ref->domains[num].sid)) {
				return num;
			}
		}
	} else {
		num = ref->count;
	}

	if (num >= LSA_REF_DOMAIN_LIST_MULTIPLIER) {
		/* index not found, already at maximum domain limit */
		return -1;
	}

	ref->count = num + 1;
	ref->max_size = LSA_REF_DOMAIN_LIST_MULTIPLIER;

	ref->domains = TALLOC_REALLOC_ARRAY(mem_ctx, ref->domains,
					    struct lsa_DomainInfo, ref->count);
	if (!ref->domains) {
		return -1;
	}

	ZERO_STRUCT(ref->domains[num]);

	init_lsa_StringLarge(&ref->domains[num].name, dom_name);
	ref->domains[num].sid = sid_dup_talloc(mem_ctx, dom_sid);
	if (!ref->domains[num].sid) {
		return -1;
	}

	return num;
}


/***************************************************************************
 initialize a lsa_DomainInfo structure.
 ***************************************************************************/

static void init_dom_query_3(struct lsa_DomainInfo *r,
			     const char *name,
			     DOM_SID *sid)
{
	init_lsa_StringLarge(&r->name, name);
	r->sid = sid;
}

/***************************************************************************
 initialize a lsa_DomainInfo structure.
 ***************************************************************************/

static void init_dom_query_5(struct lsa_DomainInfo *r,
			     const char *name,
			     DOM_SID *sid)
{
	init_lsa_StringLarge(&r->name, name);
	r->sid = sid;
}

/***************************************************************************
 lookup_lsa_rids. Must be called as root for lookup_name to work.
 ***************************************************************************/

static NTSTATUS lookup_lsa_rids(TALLOC_CTX *mem_ctx,
				struct lsa_RefDomainList *ref,
				struct lsa_TranslatedSid *prid,
				uint32_t num_entries,
				struct lsa_String *name,
				int flags,
				uint32_t *pmapped_count)
{
	uint32 mapped_count, i;

	SMB_ASSERT(num_entries <= MAX_LOOKUP_SIDS);

	mapped_count = 0;
	*pmapped_count = 0;

	for (i = 0; i < num_entries; i++) {
		DOM_SID sid;
		uint32 rid;
		int dom_idx;
		const char *full_name;
		const char *domain;
		enum lsa_SidType type = SID_NAME_UNKNOWN;

		/* Split name into domain and user component */

		/* follow w2k8 behavior and return the builtin domain when no
		 * input has been passed in */

		if (name[i].string) {
			full_name = name[i].string;
		} else {
			full_name = "BUILTIN";
		}

		DEBUG(5, ("lookup_lsa_rids: looking up name %s\n", full_name));

		/* We can ignore the result of lookup_name, it will not touch
		   "type" if it's not successful */

		lookup_name(mem_ctx, full_name, flags, &domain, NULL,
			    &sid, &type);

		switch (type) {
		case SID_NAME_USER:
		case SID_NAME_DOM_GRP:
		case SID_NAME_DOMAIN:
		case SID_NAME_ALIAS:
		case SID_NAME_WKN_GRP:
			DEBUG(5, ("init_lsa_rids: %s found\n", full_name));
			/* Leave these unchanged */
			break;
		default:
			/* Don't hand out anything but the list above */
			DEBUG(5, ("init_lsa_rids: %s not found\n", full_name));
			type = SID_NAME_UNKNOWN;
			break;
		}

		rid = 0;
		dom_idx = -1;

		if (type != SID_NAME_UNKNOWN) {
			if (type == SID_NAME_DOMAIN) {
				rid = (uint32_t)-1;
			} else {
				sid_split_rid(&sid, &rid);
			}
			dom_idx = init_lsa_ref_domain_list(mem_ctx, ref, domain, &sid);
			mapped_count++;
		}

		prid[i].sid_type	= type;
		prid[i].rid		= rid;
		prid[i].sid_index	= dom_idx;
	}

	*pmapped_count = mapped_count;
	return NT_STATUS_OK;
}

/***************************************************************************
 lookup_lsa_sids. Must be called as root for lookup_name to work.
 ***************************************************************************/

static NTSTATUS lookup_lsa_sids(TALLOC_CTX *mem_ctx,
				struct lsa_RefDomainList *ref,
				struct lsa_TranslatedSid3 *trans_sids,
				uint32_t num_entries,
				struct lsa_String *name,
				int flags,
				uint32 *pmapped_count)
{
	uint32 mapped_count, i;

	SMB_ASSERT(num_entries <= MAX_LOOKUP_SIDS);

	mapped_count = 0;
	*pmapped_count = 0;

	for (i = 0; i < num_entries; i++) {
		DOM_SID sid;
		uint32 rid;
		int dom_idx;
		const char *full_name;
		const char *domain;
		enum lsa_SidType type = SID_NAME_UNKNOWN;

		ZERO_STRUCT(sid);

		/* Split name into domain and user component */

		full_name = name[i].string;
		if (full_name == NULL) {
			return NT_STATUS_NO_MEMORY;
		}

		DEBUG(5, ("init_lsa_sids: looking up name %s\n", full_name));

		/* We can ignore the result of lookup_name, it will not touch
		   "type" if it's not successful */

		lookup_name(mem_ctx, full_name, flags, &domain, NULL,
			    &sid, &type);

		switch (type) {
		case SID_NAME_USER:
		case SID_NAME_DOM_GRP:
		case SID_NAME_DOMAIN:
		case SID_NAME_ALIAS:
		case SID_NAME_WKN_GRP:
			DEBUG(5, ("init_lsa_sids: %s found\n", full_name));
			/* Leave these unchanged */
			break;
		default:
			/* Don't hand out anything but the list above */
			DEBUG(5, ("init_lsa_sids: %s not found\n", full_name));
			type = SID_NAME_UNKNOWN;
			break;
		}

		rid = 0;
		dom_idx = -1;

		if (type != SID_NAME_UNKNOWN) {
			DOM_SID domain_sid;
			sid_copy(&domain_sid, &sid);
			sid_split_rid(&domain_sid, &rid);
			dom_idx = init_lsa_ref_domain_list(mem_ctx, ref, domain, &domain_sid);
			mapped_count++;
		}

		/* Initialize the lsa_TranslatedSid3 return. */
		trans_sids[i].sid_type = type;
		trans_sids[i].sid = sid_dup_talloc(mem_ctx, &sid);
		trans_sids[i].sid_index = dom_idx;
	}

	*pmapped_count = mapped_count;
	return NT_STATUS_OK;
}

static NTSTATUS make_lsa_object_sd(TALLOC_CTX *mem_ctx, SEC_DESC **sd, size_t *sd_size,
					const struct generic_mapping *map,
					DOM_SID *sid, uint32_t sid_access)
{
	DOM_SID adm_sid;
	SEC_ACE ace[5];
	size_t i = 0;

	SEC_ACL *psa = NULL;

	/* READ|EXECUTE access for Everyone */

	init_sec_ace(&ace[i++], &global_sid_World, SEC_ACE_TYPE_ACCESS_ALLOWED,
			map->generic_execute | map->generic_read, 0);

	/* Add Full Access 'BUILTIN\Administrators' and 'BUILTIN\Account Operators */

	init_sec_ace(&ace[i++], &global_sid_Builtin_Administrators,
			SEC_ACE_TYPE_ACCESS_ALLOWED, map->generic_all, 0);
	init_sec_ace(&ace[i++], &global_sid_Builtin_Account_Operators,
			SEC_ACE_TYPE_ACCESS_ALLOWED, map->generic_all, 0);

	/* Add Full Access for Domain Admins */
	sid_copy(&adm_sid, get_global_sam_sid());
	sid_append_rid(&adm_sid, DOMAIN_GROUP_RID_ADMINS);
	init_sec_ace(&ace[i++], &adm_sid, SEC_ACE_TYPE_ACCESS_ALLOWED,
			map->generic_all, 0);

	/* If we have a sid, give it some special access */

	if (sid) {
		init_sec_ace(&ace[i++], sid, SEC_ACE_TYPE_ACCESS_ALLOWED,
			sid_access, 0);
	}

	if((psa = make_sec_acl(mem_ctx, NT4_ACL_REVISION, i, ace)) == NULL)
		return NT_STATUS_NO_MEMORY;

	if((*sd = make_sec_desc(mem_ctx, SECURITY_DESCRIPTOR_REVISION_1,
				SEC_DESC_SELF_RELATIVE, &adm_sid, NULL, NULL,
				psa, sd_size)) == NULL)
		return NT_STATUS_NO_MEMORY;

	return NT_STATUS_OK;
}


/***************************************************************************
 _lsa_OpenPolicy2
 ***************************************************************************/

NTSTATUS _lsa_OpenPolicy2(pipes_struct *p,
			  struct lsa_OpenPolicy2 *r)
{
	struct lsa_info *info;
	SEC_DESC *psd = NULL;
	size_t sd_size;
	uint32 des_access = r->in.access_mask;
	uint32 acc_granted;
	NTSTATUS status;

	/* Work out max allowed. */
	map_max_allowed_access(p->server_info->ptok,
			       &p->server_info->utok,
			       &des_access);

	/* map the generic bits to the lsa policy ones */
	se_map_generic(&des_access, &lsa_policy_mapping);

	/* get the generic lsa policy SD until we store it */
	status = make_lsa_object_sd(p->mem_ctx, &psd, &sd_size, &lsa_policy_mapping,
			NULL, 0);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = access_check_object(psd, p->server_info->ptok,
				     NULL, 0, des_access,
				     &acc_granted, "_lsa_OpenPolicy2" );
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	/* associate the domain SID with the (unique) handle. */
	info = TALLOC_ZERO_P(p->mem_ctx, struct lsa_info);
	if (info == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	sid_copy(&info->sid,get_global_sam_sid());
	info->access = acc_granted;
	info->type = LSA_HANDLE_POLICY_TYPE;

	/* set up the LSA QUERY INFO response */
	if (!create_policy_hnd(p, r->out.handle, info))
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;

	return NT_STATUS_OK;
}

/***************************************************************************
 _lsa_OpenPolicy
 ***************************************************************************/

NTSTATUS _lsa_OpenPolicy(pipes_struct *p,
			 struct lsa_OpenPolicy *r)
{
	struct lsa_OpenPolicy2 o;

	o.in.system_name	= NULL; /* should be ignored */
	o.in.attr		= r->in.attr;
	o.in.access_mask	= r->in.access_mask;

	o.out.handle		= r->out.handle;

	return _lsa_OpenPolicy2(p, &o);
}

/***************************************************************************
 _lsa_EnumTrustDom - this needs fixing to do more than return NULL ! JRA.
 ufff, done :)  mimir
 ***************************************************************************/

NTSTATUS _lsa_EnumTrustDom(pipes_struct *p,
			   struct lsa_EnumTrustDom *r)
{
	struct lsa_info *info;
	uint32_t count;
	struct trustdom_info **domains;
	struct lsa_DomainInfo *entries;
	int i;
	NTSTATUS nt_status;

	if (!find_policy_by_hnd(p, r->in.handle, (void **)(void *)&info))
		return NT_STATUS_INVALID_HANDLE;

	if (info->type != LSA_HANDLE_POLICY_TYPE) {
		return NT_STATUS_INVALID_HANDLE;
	}

	/* check if the user has enough rights */
	if (!(info->access & LSA_POLICY_VIEW_LOCAL_INFORMATION))
		return NT_STATUS_ACCESS_DENIED;

	become_root();
	nt_status = pdb_enum_trusteddoms(p->mem_ctx, &count, &domains);
	unbecome_root();

	if (!NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	}

	entries = TALLOC_ZERO_ARRAY(p->mem_ctx, struct lsa_DomainInfo, count);
	if (!entries) {
		return NT_STATUS_NO_MEMORY;
	}

	for (i=0; i<count; i++) {
		init_lsa_StringLarge(&entries[i].name, domains[i]->name);
		entries[i].sid = &domains[i]->sid;
	}

	if (*r->in.resume_handle >= count) {
		*r->out.resume_handle = -1;
		TALLOC_FREE(entries);
		return NT_STATUS_NO_MORE_ENTRIES;
	}

	/* return the rest, limit by max_size. Note that we
	   use the w2k3 element size value of 60 */
	r->out.domains->count = count - *r->in.resume_handle;
	r->out.domains->count = MIN(r->out.domains->count,
				 1+(r->in.max_size/LSA_ENUM_TRUST_DOMAIN_MULTIPLIER));

	r->out.domains->domains = entries + *r->in.resume_handle;

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

#define LSA_AUDIT_NUM_CATEGORIES_NT4	7
#define LSA_AUDIT_NUM_CATEGORIES_WIN2K	9
#define LSA_AUDIT_NUM_CATEGORIES LSA_AUDIT_NUM_CATEGORIES_NT4

/***************************************************************************
 _lsa_QueryInfoPolicy
 ***************************************************************************/

NTSTATUS _lsa_QueryInfoPolicy(pipes_struct *p,
			      struct lsa_QueryInfoPolicy *r)
{
	NTSTATUS status = NT_STATUS_OK;
	struct lsa_info *handle;
	DOM_SID domain_sid;
	const char *name;
	DOM_SID *sid = NULL;
	union lsa_PolicyInformation *info = NULL;
	uint32_t acc_required = 0;

	if (!find_policy_by_hnd(p, r->in.handle, (void **)(void *)&handle))
		return NT_STATUS_INVALID_HANDLE;

	if (handle->type != LSA_HANDLE_POLICY_TYPE) {
		return NT_STATUS_INVALID_HANDLE;
	}

	switch (r->in.level) {
	case LSA_POLICY_INFO_AUDIT_LOG:
	case LSA_POLICY_INFO_AUDIT_EVENTS:
		acc_required = LSA_POLICY_VIEW_AUDIT_INFORMATION;
		break;
	case LSA_POLICY_INFO_DOMAIN:
		acc_required = LSA_POLICY_VIEW_LOCAL_INFORMATION;
		break;
	case LSA_POLICY_INFO_PD:
		acc_required = LSA_POLICY_GET_PRIVATE_INFORMATION;
		break;
	case LSA_POLICY_INFO_ACCOUNT_DOMAIN:
		acc_required = LSA_POLICY_VIEW_LOCAL_INFORMATION;
		break;
	case LSA_POLICY_INFO_ROLE:
	case LSA_POLICY_INFO_REPLICA:
		acc_required = LSA_POLICY_VIEW_LOCAL_INFORMATION;
		break;
	case LSA_POLICY_INFO_QUOTA:
		acc_required = LSA_POLICY_VIEW_LOCAL_INFORMATION;
		break;
	case LSA_POLICY_INFO_MOD:
	case LSA_POLICY_INFO_AUDIT_FULL_SET:
		/* according to MS-LSAD 3.1.4.4.3 */
		return NT_STATUS_INVALID_PARAMETER;
	case LSA_POLICY_INFO_AUDIT_FULL_QUERY:
		acc_required = LSA_POLICY_VIEW_AUDIT_INFORMATION;
		break;
	case LSA_POLICY_INFO_DNS:
	case LSA_POLICY_INFO_DNS_INT:
	case LSA_POLICY_INFO_L_ACCOUNT_DOMAIN:
		acc_required = LSA_POLICY_VIEW_LOCAL_INFORMATION;
		break;
	default:
		break;
	}

	if (!(handle->access & acc_required)) {
		/* return NT_STATUS_ACCESS_DENIED; */
	}

	info = TALLOC_ZERO_P(p->mem_ctx, union lsa_PolicyInformation);
	if (!info) {
		return NT_STATUS_NO_MEMORY;
	}

	switch (r->in.level) {
	/* according to MS-LSAD 3.1.4.4.3 */
	case LSA_POLICY_INFO_MOD:
	case LSA_POLICY_INFO_AUDIT_FULL_SET:
	case LSA_POLICY_INFO_AUDIT_FULL_QUERY:
		return NT_STATUS_INVALID_PARAMETER;
	case LSA_POLICY_INFO_AUDIT_LOG:
		info->audit_log.percent_full		= 0;
		info->audit_log.maximum_log_size	= 0;
		info->audit_log.retention_time		= 0;
		info->audit_log.shutdown_in_progress	= 0;
		info->audit_log.time_to_shutdown	= 0;
		info->audit_log.next_audit_record	= 0;
		status = NT_STATUS_OK;
		break;
	case LSA_POLICY_INFO_PD:
		info->pd.name.string			= NULL;
		status = NT_STATUS_OK;
		break;
	case LSA_POLICY_INFO_REPLICA:
		info->replica.source.string		= NULL;
		info->replica.account.string		= NULL;
		status = NT_STATUS_OK;
		break;
	case LSA_POLICY_INFO_QUOTA:
		info->quota.paged_pool			= 0;
		info->quota.non_paged_pool		= 0;
		info->quota.min_wss			= 0;
		info->quota.max_wss			= 0;
		info->quota.pagefile			= 0;
		info->quota.unknown			= 0;
		status = NT_STATUS_OK;
		break;
	case LSA_POLICY_INFO_AUDIT_EVENTS:
		{

		uint32 policy_def = LSA_AUDIT_POLICY_ALL;

		/* check if the user has enough rights */
		if (!(handle->access & LSA_POLICY_VIEW_AUDIT_INFORMATION)) {
			DEBUG(10,("_lsa_QueryInfoPolicy: insufficient access rights\n"));
			return NT_STATUS_ACCESS_DENIED;
		}

		/* fake info: We audit everything. ;) */

		info->audit_events.auditing_mode = true;
		info->audit_events.count = LSA_AUDIT_NUM_CATEGORIES;
		info->audit_events.settings = TALLOC_ZERO_ARRAY(p->mem_ctx,
								enum lsa_PolicyAuditPolicy,
								info->audit_events.count);
		if (!info->audit_events.settings) {
			return NT_STATUS_NO_MEMORY;
		}

		info->audit_events.settings[LSA_AUDIT_CATEGORY_ACCOUNT_MANAGEMENT] = policy_def;
		info->audit_events.settings[LSA_AUDIT_CATEGORY_FILE_AND_OBJECT_ACCESS] = policy_def;
		info->audit_events.settings[LSA_AUDIT_CATEGORY_LOGON] = policy_def;
		info->audit_events.settings[LSA_AUDIT_CATEGORY_PROCCESS_TRACKING] = policy_def;
		info->audit_events.settings[LSA_AUDIT_CATEGORY_SECURITY_POLICY_CHANGES] = policy_def;
		info->audit_events.settings[LSA_AUDIT_CATEGORY_SYSTEM] = policy_def;
		info->audit_events.settings[LSA_AUDIT_CATEGORY_USE_OF_USER_RIGHTS] = policy_def;

		break;
		}
	case LSA_POLICY_INFO_DOMAIN:
		/* check if the user has enough rights */
		if (!(handle->access & LSA_POLICY_VIEW_LOCAL_INFORMATION))
			return NT_STATUS_ACCESS_DENIED;

		/* Request PolicyPrimaryDomainInformation. */
		switch (lp_server_role()) {
			case ROLE_DOMAIN_PDC:
			case ROLE_DOMAIN_BDC:
				name = get_global_sam_name();
				sid = sid_dup_talloc(p->mem_ctx, get_global_sam_sid());
				if (!sid) {
					return NT_STATUS_NO_MEMORY;
				}
				break;
			case ROLE_DOMAIN_MEMBER:
				name = lp_workgroup();
				/* We need to return the Domain SID here. */
				if (secrets_fetch_domain_sid(lp_workgroup(), &domain_sid)) {
					sid = sid_dup_talloc(p->mem_ctx, &domain_sid);
					if (!sid) {
						return NT_STATUS_NO_MEMORY;
					}
				} else {
					return NT_STATUS_CANT_ACCESS_DOMAIN_INFO;
				}
				break;
			case ROLE_STANDALONE:
				name = lp_workgroup();
				sid = NULL;
				break;
			default:
				return NT_STATUS_CANT_ACCESS_DOMAIN_INFO;
		}
		init_dom_query_3(&info->domain, name, sid);
		break;
	case LSA_POLICY_INFO_ACCOUNT_DOMAIN:
		/* check if the user has enough rights */
		if (!(handle->access & LSA_POLICY_VIEW_LOCAL_INFORMATION))
			return NT_STATUS_ACCESS_DENIED;

		/* Request PolicyAccountDomainInformation. */
		name = get_global_sam_name();
		sid = get_global_sam_sid();

		init_dom_query_5(&info->account_domain, name, sid);
		break;
	case LSA_POLICY_INFO_ROLE:
		/* check if the user has enough rights */
		if (!(handle->access & LSA_POLICY_VIEW_LOCAL_INFORMATION))
			return NT_STATUS_ACCESS_DENIED;

		switch (lp_server_role()) {
			case ROLE_DOMAIN_BDC:
				/*
				 * only a BDC is a backup controller
				 * of the domain, it controls.
				 */
				info->role.role = LSA_ROLE_BACKUP;
				break;
			default:
				/*
				 * any other role is a primary
				 * of the domain, it controls.
				 */
				info->role.role = LSA_ROLE_PRIMARY;
				break;
		}
		break;
	case LSA_POLICY_INFO_DNS:
	case LSA_POLICY_INFO_DNS_INT: {
		struct pdb_domain_info *dominfo;

		if ((pdb_capabilities() & PDB_CAP_ADS) == 0) {
			DEBUG(10, ("Not replying to LSA_POLICY_INFO_DNS "
				   "without ADS passdb backend\n"));
			status = NT_STATUS_INVALID_INFO_CLASS;
			break;
		}

		dominfo = pdb_get_domain_info(info);
		if (dominfo == NULL) {
			status = NT_STATUS_NO_MEMORY;
			break;
		}

		init_lsa_StringLarge(&info->dns.name,
				     dominfo->name);
		init_lsa_StringLarge(&info->dns.dns_domain,
				     dominfo->dns_domain);
		init_lsa_StringLarge(&info->dns.dns_forest,
				     dominfo->dns_forest);
		info->dns.domain_guid = dominfo->guid;
		info->dns.sid = &dominfo->sid;
		break;
	}
	default:
		DEBUG(0,("_lsa_QueryInfoPolicy: unknown info level in Lsa Query: %d\n",
			r->in.level));
		status = NT_STATUS_INVALID_INFO_CLASS;
		break;
	}

	*r->out.info = info;

	return status;
}

/***************************************************************************
 _lsa_QueryInfoPolicy2
 ***************************************************************************/

NTSTATUS _lsa_QueryInfoPolicy2(pipes_struct *p,
			       struct lsa_QueryInfoPolicy2 *r2)
{
	struct lsa_QueryInfoPolicy r;

	if ((pdb_capabilities() & PDB_CAP_ADS) == 0) {
		p->rng_fault_state = True;
		return NT_STATUS_NOT_IMPLEMENTED;
	}

	ZERO_STRUCT(r);
	r.in.handle = r2->in.handle;
	r.in.level = r2->in.level;
	r.out.info = r2->out.info;

	return _lsa_QueryInfoPolicy(p, &r);
}

/***************************************************************************
 _lsa_lookup_sids_internal
 ***************************************************************************/

static NTSTATUS _lsa_lookup_sids_internal(pipes_struct *p,
					  TALLOC_CTX *mem_ctx,
					  uint16_t level,			/* input */
					  int num_sids,				/* input */
					  struct lsa_SidPtr *sid,		/* input */
					  struct lsa_RefDomainList **pp_ref,	/* input/output */
					  struct lsa_TranslatedName2 **pp_names,/* input/output */
					  uint32_t *pp_mapped_count)		/* input/output */
{
	NTSTATUS status;
	int i;
	const DOM_SID **sids = NULL;
	struct lsa_RefDomainList *ref = NULL;
	uint32 mapped_count = 0;
	struct lsa_dom_info *dom_infos = NULL;
	struct lsa_name_info *name_infos = NULL;
	struct lsa_TranslatedName2 *names = NULL;

	*pp_mapped_count = 0;
	*pp_names = NULL;
	*pp_ref = NULL;

	if (num_sids == 0) {
		return NT_STATUS_OK;
	}

	sids = TALLOC_ARRAY(p->mem_ctx, const DOM_SID *, num_sids);
	ref = TALLOC_ZERO_P(p->mem_ctx, struct lsa_RefDomainList);

	if (sids == NULL || ref == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	for (i=0; i<num_sids; i++) {
		sids[i] = sid[i].sid;
	}

	status = lookup_sids(p->mem_ctx, num_sids, sids, level,
				  &dom_infos, &name_infos);

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	names = TALLOC_ARRAY(p->mem_ctx, struct lsa_TranslatedName2, num_sids);
	if (names == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	for (i=0; i<LSA_REF_DOMAIN_LIST_MULTIPLIER; i++) {

		if (!dom_infos[i].valid) {
			break;
		}

		if (init_lsa_ref_domain_list(mem_ctx, ref,
					     dom_infos[i].name,
					     &dom_infos[i].sid) != i) {
			DEBUG(0, ("Domain %s mentioned twice??\n",
				  dom_infos[i].name));
			return NT_STATUS_INTERNAL_ERROR;
		}
	}

	for (i=0; i<num_sids; i++) {
		struct lsa_name_info *name = &name_infos[i];

		if (name->type == SID_NAME_UNKNOWN) {
			fstring tmp;
			name->dom_idx = -1;
			/* Unknown sids should return the string
			 * representation of the SID. Windows 2003 behaves
			 * rather erratic here, in many cases it returns the
			 * RID as 8 bytes hex, in others it returns the full
			 * SID. We (Jerry/VL) could not figure out which the
			 * hard cases are, so leave it with the SID.  */
			name->name = talloc_asprintf(p->mem_ctx, "%s",
			                             sid_to_fstring(tmp,
								    sids[i]));
			if (name->name == NULL) {
				return NT_STATUS_NO_MEMORY;
			}
		} else {
			mapped_count += 1;
		}

		names[i].sid_type	= name->type;
		names[i].name.string	= name->name;
		names[i].sid_index	= name->dom_idx;
		names[i].unknown	= 0;
	}

	status = NT_STATUS_NONE_MAPPED;
	if (mapped_count > 0) {
		status = (mapped_count < num_sids) ?
			STATUS_SOME_UNMAPPED : NT_STATUS_OK;
	}

	DEBUG(10, ("num_sids %d, mapped_count %d, status %s\n",
		   num_sids, mapped_count, nt_errstr(status)));

	*pp_mapped_count = mapped_count;
	*pp_names = names;
	*pp_ref = ref;

	return status;
}

/***************************************************************************
 _lsa_LookupSids
 ***************************************************************************/

NTSTATUS _lsa_LookupSids(pipes_struct *p,
			 struct lsa_LookupSids *r)
{
	NTSTATUS status;
	struct lsa_info *handle;
	int num_sids = r->in.sids->num_sids;
	uint32 mapped_count = 0;
	struct lsa_RefDomainList *domains = NULL;
	struct lsa_TranslatedName *names_out = NULL;
	struct lsa_TranslatedName2 *names = NULL;
	int i;

	if ((r->in.level < 1) || (r->in.level > 6)) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (!find_policy_by_hnd(p, r->in.handle, (void **)(void *)&handle)) {
		return NT_STATUS_INVALID_HANDLE;
	}

	if (handle->type != LSA_HANDLE_POLICY_TYPE) {
		return NT_STATUS_INVALID_HANDLE;
	}

	/* check if the user has enough rights */
	if (!(handle->access & LSA_POLICY_LOOKUP_NAMES)) {
		return NT_STATUS_ACCESS_DENIED;
	}

	if (num_sids >  MAX_LOOKUP_SIDS) {
		DEBUG(5,("_lsa_LookupSids: limit of %d exceeded, requested %d\n",
			 MAX_LOOKUP_SIDS, num_sids));
		return NT_STATUS_NONE_MAPPED;
	}

	status = _lsa_lookup_sids_internal(p,
					   p->mem_ctx,
					   r->in.level,
					   num_sids,
					   r->in.sids->sids,
					   &domains,
					   &names,
					   &mapped_count);

	/* Only return here when there is a real error.
	   NT_STATUS_NONE_MAPPED is a special case as it indicates that none of
	   the requested sids could be resolved. Older versions of XP (pre SP3)
	   rely that we return with the string representations of those SIDs in
	   that case. If we don't, XP crashes - Guenther
	   */

	if (NT_STATUS_IS_ERR(status) &&
	    !NT_STATUS_EQUAL(status, NT_STATUS_NONE_MAPPED)) {
		return status;
	}

	/* Convert from lsa_TranslatedName2 to lsa_TranslatedName */
	names_out = TALLOC_ARRAY(p->mem_ctx, struct lsa_TranslatedName,
				 num_sids);
	if (!names_out) {
		return NT_STATUS_NO_MEMORY;
	}

	for (i=0; i<num_sids; i++) {
		names_out[i].sid_type = names[i].sid_type;
		names_out[i].name = names[i].name;
		names_out[i].sid_index = names[i].sid_index;
	}

	*r->out.domains = domains;
	r->out.names->count = num_sids;
	r->out.names->names = names_out;
	*r->out.count = mapped_count;

	return status;
}

/***************************************************************************
 _lsa_LookupSids2
 ***************************************************************************/

NTSTATUS _lsa_LookupSids2(pipes_struct *p,
			  struct lsa_LookupSids2 *r)
{
	NTSTATUS status;
	struct lsa_info *handle;
	int num_sids = r->in.sids->num_sids;
	uint32 mapped_count = 0;
	struct lsa_RefDomainList *domains = NULL;
	struct lsa_TranslatedName2 *names = NULL;
	bool check_policy = true;

	switch (p->hdr_req.opnum) {
		case NDR_LSA_LOOKUPSIDS3:
			check_policy = false;
			break;
		case NDR_LSA_LOOKUPSIDS2:
		default:
			check_policy = true;
	}

	if ((r->in.level < 1) || (r->in.level > 6)) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (check_policy) {
		if (!find_policy_by_hnd(p, r->in.handle, (void **)(void *)&handle)) {
			return NT_STATUS_INVALID_HANDLE;
		}

		if (handle->type != LSA_HANDLE_POLICY_TYPE) {
			return NT_STATUS_INVALID_HANDLE;
		}

		/* check if the user has enough rights */
		if (!(handle->access & LSA_POLICY_LOOKUP_NAMES)) {
			return NT_STATUS_ACCESS_DENIED;
		}
	}

	if (num_sids >  MAX_LOOKUP_SIDS) {
		DEBUG(5,("_lsa_LookupSids2: limit of %d exceeded, requested %d\n",
			 MAX_LOOKUP_SIDS, num_sids));
		return NT_STATUS_NONE_MAPPED;
	}

	status = _lsa_lookup_sids_internal(p,
					   p->mem_ctx,
					   r->in.level,
					   num_sids,
					   r->in.sids->sids,
					   &domains,
					   &names,
					   &mapped_count);

	*r->out.domains = domains;
	r->out.names->count = num_sids;
	r->out.names->names = names;
	*r->out.count = mapped_count;

	return status;
}

/***************************************************************************
 _lsa_LookupSids3
 ***************************************************************************/

NTSTATUS _lsa_LookupSids3(pipes_struct *p,
			  struct lsa_LookupSids3 *r)
{
	struct lsa_LookupSids2 q;

	/* No policy handle on this call. Restrict to crypto connections. */
	if (p->auth.auth_type != PIPE_AUTH_TYPE_SCHANNEL) {
		DEBUG(0,("_lsa_LookupSids3: client %s not using schannel for netlogon\n",
			get_remote_machine_name() ));
		return NT_STATUS_INVALID_PARAMETER;
	}

	q.in.handle		= NULL;
	q.in.sids		= r->in.sids;
	q.in.level		= r->in.level;
	q.in.lookup_options	= r->in.lookup_options;
	q.in.client_revision	= r->in.client_revision;
	q.in.names		= r->in.names;
	q.in.count		= r->in.count;

	q.out.domains		= r->out.domains;
	q.out.names		= r->out.names;
	q.out.count		= r->out.count;

	return _lsa_LookupSids2(p, &q);
}

/***************************************************************************
 ***************************************************************************/

static int lsa_lookup_level_to_flags(enum lsa_LookupNamesLevel level)
{
	int flags;

	switch (level) {
		case LSA_LOOKUP_NAMES_ALL: /* 1 */
			flags = LOOKUP_NAME_ALL;
			break;
		case LSA_LOOKUP_NAMES_DOMAINS_ONLY: /* 2 */
			flags = LOOKUP_NAME_DOMAIN|LOOKUP_NAME_REMOTE|LOOKUP_NAME_ISOLATED;
			break;
		case LSA_LOOKUP_NAMES_PRIMARY_DOMAIN_ONLY: /* 3 */
			flags = LOOKUP_NAME_DOMAIN|LOOKUP_NAME_ISOLATED;
			break;
		case LSA_LOOKUP_NAMES_UPLEVEL_TRUSTS_ONLY: /* 4 */
		case LSA_LOOKUP_NAMES_FOREST_TRUSTS_ONLY: /* 5 */
		case LSA_LOOKUP_NAMES_UPLEVEL_TRUSTS_ONLY2: /* 6 */
		case LSA_LOOKUP_NAMES_RODC_REFERRAL_TO_FULL_DC: /* 7 */
		default:
			flags = LOOKUP_NAME_NONE;
			break;
	}

	return flags;
}

/***************************************************************************
 _lsa_LookupNames
 ***************************************************************************/

NTSTATUS _lsa_LookupNames(pipes_struct *p,
			  struct lsa_LookupNames *r)
{
	NTSTATUS status = NT_STATUS_NONE_MAPPED;
	struct lsa_info *handle;
	struct lsa_String *names = r->in.names;
	uint32 num_entries = r->in.num_names;
	struct lsa_RefDomainList *domains = NULL;
	struct lsa_TranslatedSid *rids = NULL;
	uint32 mapped_count = 0;
	int flags = 0;

	if (num_entries >  MAX_LOOKUP_SIDS) {
		num_entries = MAX_LOOKUP_SIDS;
		DEBUG(5,("_lsa_LookupNames: truncating name lookup list to %d\n",
			num_entries));
	}

	flags = lsa_lookup_level_to_flags(r->in.level);

	domains = TALLOC_ZERO_P(p->mem_ctx, struct lsa_RefDomainList);
	if (!domains) {
		return NT_STATUS_NO_MEMORY;
	}

	if (num_entries) {
		rids = TALLOC_ZERO_ARRAY(p->mem_ctx, struct lsa_TranslatedSid,
					 num_entries);
		if (!rids) {
			return NT_STATUS_NO_MEMORY;
		}
	} else {
		rids = NULL;
	}

	if (!find_policy_by_hnd(p, r->in.handle, (void **)(void *)&handle)) {
		status = NT_STATUS_INVALID_HANDLE;
		goto done;
	}

	if (handle->type != LSA_HANDLE_POLICY_TYPE) {
		return NT_STATUS_INVALID_HANDLE;
	}

	/* check if the user has enough rights */
	if (!(handle->access & LSA_POLICY_LOOKUP_NAMES)) {
		status = NT_STATUS_ACCESS_DENIED;
		goto done;
	}

	/* set up the LSA Lookup RIDs response */
	become_root(); /* lookup_name can require root privs */
	status = lookup_lsa_rids(p->mem_ctx, domains, rids, num_entries,
				 names, flags, &mapped_count);
	unbecome_root();

done:

	if (NT_STATUS_IS_OK(status) && (num_entries != 0) ) {
		if (mapped_count == 0) {
			status = NT_STATUS_NONE_MAPPED;
		} else if (mapped_count != num_entries) {
			status = STATUS_SOME_UNMAPPED;
		}
	}

	*r->out.count = mapped_count;
	*r->out.domains = domains;
	r->out.sids->sids = rids;
	r->out.sids->count = num_entries;

	return status;
}

/***************************************************************************
 _lsa_LookupNames2
 ***************************************************************************/

NTSTATUS _lsa_LookupNames2(pipes_struct *p,
			   struct lsa_LookupNames2 *r)
{
	NTSTATUS status;
	struct lsa_LookupNames q;
	struct lsa_TransSidArray2 *sid_array2 = r->in.sids;
	struct lsa_TransSidArray *sid_array = NULL;
	uint32_t i;

	sid_array = TALLOC_ZERO_P(p->mem_ctx, struct lsa_TransSidArray);
	if (!sid_array) {
		return NT_STATUS_NO_MEMORY;
	}

	q.in.handle		= r->in.handle;
	q.in.num_names		= r->in.num_names;
	q.in.names		= r->in.names;
	q.in.level		= r->in.level;
	q.in.sids		= sid_array;
	q.in.count		= r->in.count;
	/* we do not know what this is for */
	/*			= r->in.unknown1; */
	/*			= r->in.unknown2; */

	q.out.domains		= r->out.domains;
	q.out.sids		= sid_array;
	q.out.count		= r->out.count;

	status = _lsa_LookupNames(p, &q);

	sid_array2->count = sid_array->count;
	sid_array2->sids = TALLOC_ARRAY(p->mem_ctx, struct lsa_TranslatedSid2, sid_array->count);
	if (!sid_array2->sids) {
		return NT_STATUS_NO_MEMORY;
	}

	for (i=0; i<sid_array->count; i++) {
		sid_array2->sids[i].sid_type  = sid_array->sids[i].sid_type;
		sid_array2->sids[i].rid       = sid_array->sids[i].rid;
		sid_array2->sids[i].sid_index = sid_array->sids[i].sid_index;
		sid_array2->sids[i].unknown   = 0;
	}

	r->out.sids = sid_array2;

	return status;
}

/***************************************************************************
 _lsa_LookupNames3
 ***************************************************************************/

NTSTATUS _lsa_LookupNames3(pipes_struct *p,
			   struct lsa_LookupNames3 *r)
{
	NTSTATUS status;
	struct lsa_info *handle;
	struct lsa_String *names = r->in.names;
	uint32 num_entries = r->in.num_names;
	struct lsa_RefDomainList *domains = NULL;
	struct lsa_TranslatedSid3 *trans_sids = NULL;
	uint32 mapped_count = 0;
	int flags = 0;
	bool check_policy = true;

	switch (p->hdr_req.opnum) {
		case NDR_LSA_LOOKUPNAMES4:
			check_policy = false;
			break;
		case NDR_LSA_LOOKUPNAMES3:
		default:
			check_policy = true;
	}

	if (num_entries >  MAX_LOOKUP_SIDS) {
		num_entries = MAX_LOOKUP_SIDS;
		DEBUG(5,("_lsa_LookupNames3: truncating name lookup list to %d\n", num_entries));
	}

	/* Probably the lookup_level is some sort of bitmask. */
	if (r->in.level == 1) {
		flags = LOOKUP_NAME_ALL;
	}

	domains = TALLOC_ZERO_P(p->mem_ctx, struct lsa_RefDomainList);
	if (!domains) {
		return NT_STATUS_NO_MEMORY;
	}

	if (num_entries) {
		trans_sids = TALLOC_ZERO_ARRAY(p->mem_ctx, struct lsa_TranslatedSid3,
					       num_entries);
		if (!trans_sids) {
			return NT_STATUS_NO_MEMORY;
		}
	} else {
		trans_sids = NULL;
	}

	if (check_policy) {

		if (!find_policy_by_hnd(p, r->in.handle, (void **)(void *)&handle)) {
			status = NT_STATUS_INVALID_HANDLE;
			goto done;
		}

		if (handle->type != LSA_HANDLE_POLICY_TYPE) {
			return NT_STATUS_INVALID_HANDLE;
		}

		/* check if the user has enough rights */
		if (!(handle->access & LSA_POLICY_LOOKUP_NAMES)) {
			status = NT_STATUS_ACCESS_DENIED;
			goto done;
		}
	}

	/* set up the LSA Lookup SIDs response */
	become_root(); /* lookup_name can require root privs */
	status = lookup_lsa_sids(p->mem_ctx, domains, trans_sids, num_entries,
				 names, flags, &mapped_count);
	unbecome_root();

done:

	if (NT_STATUS_IS_OK(status)) {
		if (mapped_count == 0) {
			status = NT_STATUS_NONE_MAPPED;
		} else if (mapped_count != num_entries) {
			status = STATUS_SOME_UNMAPPED;
		}
	}

	*r->out.count = mapped_count;
	*r->out.domains = domains;
	r->out.sids->sids = trans_sids;
	r->out.sids->count = num_entries;

	return status;
}

/***************************************************************************
 _lsa_LookupNames4
 ***************************************************************************/

NTSTATUS _lsa_LookupNames4(pipes_struct *p,
			   struct lsa_LookupNames4 *r)
{
	struct lsa_LookupNames3 q;

	/* No policy handle on this call. Restrict to crypto connections. */
	if (p->auth.auth_type != PIPE_AUTH_TYPE_SCHANNEL) {
		DEBUG(0,("_lsa_lookup_names4: client %s not using schannel for netlogon\n",
			get_remote_machine_name() ));
		return NT_STATUS_INVALID_PARAMETER;
	}

	q.in.handle		= NULL;
	q.in.num_names		= r->in.num_names;
	q.in.names		= r->in.names;
	q.in.level		= r->in.level;
	q.in.lookup_options	= r->in.lookup_options;
	q.in.client_revision	= r->in.client_revision;
	q.in.sids		= r->in.sids;
	q.in.count		= r->in.count;

	q.out.domains		= r->out.domains;
	q.out.sids		= r->out.sids;
	q.out.count		= r->out.count;

	return _lsa_LookupNames3(p, &q);
}

/***************************************************************************
 _lsa_close. Also weird - needs to check if lsa handle is correct. JRA.
 ***************************************************************************/

NTSTATUS _lsa_Close(pipes_struct *p, struct lsa_Close *r)
{
	if (!find_policy_by_hnd(p, r->in.handle, NULL)) {
		return NT_STATUS_INVALID_HANDLE;
	}

	close_policy_hnd(p, r->in.handle);
	ZERO_STRUCTP(r->out.handle);
	return NT_STATUS_OK;
}

/***************************************************************************
 ***************************************************************************/

NTSTATUS _lsa_OpenSecret(pipes_struct *p, struct lsa_OpenSecret *r)
{
	return NT_STATUS_OBJECT_NAME_NOT_FOUND;
}

/***************************************************************************
 ***************************************************************************/

NTSTATUS _lsa_OpenTrustedDomain(pipes_struct *p, struct lsa_OpenTrustedDomain *r)
{
	return NT_STATUS_OBJECT_NAME_NOT_FOUND;
}

/***************************************************************************
 ***************************************************************************/

NTSTATUS _lsa_CreateTrustedDomain(pipes_struct *p, struct lsa_CreateTrustedDomain *r)
{
	return NT_STATUS_ACCESS_DENIED;
}

/***************************************************************************
 ***************************************************************************/

NTSTATUS _lsa_CreateSecret(pipes_struct *p, struct lsa_CreateSecret *r)
{
	return NT_STATUS_ACCESS_DENIED;
}

/***************************************************************************
 ***************************************************************************/

NTSTATUS _lsa_SetSecret(pipes_struct *p, struct lsa_SetSecret *r)
{
	return NT_STATUS_ACCESS_DENIED;
}

/***************************************************************************
 _lsa_DeleteObject
 ***************************************************************************/

NTSTATUS _lsa_DeleteObject(pipes_struct *p,
			   struct lsa_DeleteObject *r)
{
	NTSTATUS status;
	struct lsa_info *info = NULL;

	if (!find_policy_by_hnd(p, r->in.handle, (void **)(void *)&info)) {
		return NT_STATUS_INVALID_HANDLE;
	}

	if (!(info->access & STD_RIGHT_DELETE_ACCESS)) {
		return NT_STATUS_ACCESS_DENIED;
	}

	switch (info->type) {
	case LSA_HANDLE_ACCOUNT_TYPE:
		status = privilege_delete_account(&info->sid);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(10,("_lsa_DeleteObject: privilege_delete_account gave: %s\n",
				nt_errstr(status)));
			return status;
		}
		break;
	default:
		return NT_STATUS_INVALID_HANDLE;
	}

	close_policy_hnd(p, r->in.handle);
	ZERO_STRUCTP(r->out.handle);

	return status;
}

/***************************************************************************
 _lsa_EnumPrivs
 ***************************************************************************/

NTSTATUS _lsa_EnumPrivs(pipes_struct *p,
			struct lsa_EnumPrivs *r)
{
	struct lsa_info *handle;
	uint32 i;
	uint32 enum_context = *r->in.resume_handle;
	int num_privs = count_all_privileges();
	struct lsa_PrivEntry *entries = NULL;
	LUID_ATTR luid;

	/* remember that the enum_context starts at 0 and not 1 */

	if ( enum_context >= num_privs )
		return NT_STATUS_NO_MORE_ENTRIES;

	DEBUG(10,("_lsa_EnumPrivs: enum_context:%d total entries:%d\n",
		enum_context, num_privs));

	if (!find_policy_by_hnd(p, r->in.handle, (void **)(void *)&handle))
		return NT_STATUS_INVALID_HANDLE;

	if (handle->type != LSA_HANDLE_POLICY_TYPE) {
		return NT_STATUS_INVALID_HANDLE;
	}

	/* check if the user has enough rights
	   I don't know if it's the right one. not documented.  */

	if (!(handle->access & LSA_POLICY_VIEW_LOCAL_INFORMATION))
		return NT_STATUS_ACCESS_DENIED;

	if (num_privs) {
		entries = TALLOC_ZERO_ARRAY(p->mem_ctx, struct lsa_PrivEntry, num_privs);
		if (!entries) {
			return NT_STATUS_NO_MEMORY;
		}
	} else {
		entries = NULL;
	}

	for (i = 0; i < num_privs; i++) {
		if( i < enum_context) {

			init_lsa_StringLarge(&entries[i].name, NULL);

			entries[i].luid.low = 0;
			entries[i].luid.high = 0;
		} else {

			init_lsa_StringLarge(&entries[i].name, privs[i].name);

			luid = get_privilege_luid( &privs[i].se_priv );

			entries[i].luid.low = luid.luid.low;
			entries[i].luid.high = luid.luid.high;
		}
	}

	enum_context = num_privs;

	*r->out.resume_handle = enum_context;
	r->out.privs->count = num_privs;
	r->out.privs->privs = entries;

	return NT_STATUS_OK;
}

/***************************************************************************
 _lsa_LookupPrivDisplayName
 ***************************************************************************/

NTSTATUS _lsa_LookupPrivDisplayName(pipes_struct *p,
				    struct lsa_LookupPrivDisplayName *r)
{
	struct lsa_info *handle;
	const char *description;
	struct lsa_StringLarge *lsa_name;

	if (!find_policy_by_hnd(p, r->in.handle, (void **)(void *)&handle))
		return NT_STATUS_INVALID_HANDLE;

	if (handle->type != LSA_HANDLE_POLICY_TYPE) {
		return NT_STATUS_INVALID_HANDLE;
	}

	/* check if the user has enough rights */

	/*
	 * I don't know if it's the right one. not documented.
	 */
	if (!(handle->access & LSA_POLICY_VIEW_LOCAL_INFORMATION))
		return NT_STATUS_ACCESS_DENIED;

	DEBUG(10,("_lsa_LookupPrivDisplayName: name = %s\n", r->in.name->string));

	description = get_privilege_dispname(r->in.name->string);
	if (!description) {
		DEBUG(10,("_lsa_LookupPrivDisplayName: doesn't exist\n"));
		return NT_STATUS_NO_SUCH_PRIVILEGE;
	}

	DEBUG(10,("_lsa_LookupPrivDisplayName: display name = %s\n", description));

	lsa_name = TALLOC_ZERO_P(p->mem_ctx, struct lsa_StringLarge);
	if (!lsa_name) {
		return NT_STATUS_NO_MEMORY;
	}

	init_lsa_StringLarge(lsa_name, description);

	*r->out.returned_language_id = r->in.language_id;
	*r->out.disp_name = lsa_name;

	return NT_STATUS_OK;
}

/***************************************************************************
 _lsa_EnumAccounts
 ***************************************************************************/

NTSTATUS _lsa_EnumAccounts(pipes_struct *p,
			   struct lsa_EnumAccounts *r)
{
	struct lsa_info *handle;
	DOM_SID *sid_list;
	int i, j, num_entries;
	NTSTATUS status;
	struct lsa_SidPtr *sids = NULL;

	if (!find_policy_by_hnd(p, r->in.handle, (void **)(void *)&handle))
		return NT_STATUS_INVALID_HANDLE;

	if (handle->type != LSA_HANDLE_POLICY_TYPE) {
		return NT_STATUS_INVALID_HANDLE;
	}

	if (!(handle->access & LSA_POLICY_VIEW_LOCAL_INFORMATION))
		return NT_STATUS_ACCESS_DENIED;

	sid_list = NULL;
	num_entries = 0;

	/* The only way we can currently find out all the SIDs that have been
	   privileged is to scan all privileges */

	status = privilege_enumerate_accounts(&sid_list, &num_entries);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (*r->in.resume_handle >= num_entries) {
		return NT_STATUS_NO_MORE_ENTRIES;
	}

	if (num_entries - *r->in.resume_handle) {
		sids = TALLOC_ZERO_ARRAY(p->mem_ctx, struct lsa_SidPtr,
					 num_entries - *r->in.resume_handle);
		if (!sids) {
			talloc_free(sid_list);
			return NT_STATUS_NO_MEMORY;
		}

		for (i = *r->in.resume_handle, j = 0; i < num_entries; i++, j++) {
			sids[j].sid = sid_dup_talloc(p->mem_ctx, &sid_list[i]);
			if (!sids[j].sid) {
				talloc_free(sid_list);
				return NT_STATUS_NO_MEMORY;
			}
		}
	}

	talloc_free(sid_list);

	*r->out.resume_handle = num_entries;
	r->out.sids->num_sids = num_entries;
	r->out.sids->sids = sids;

	return NT_STATUS_OK;
}

/***************************************************************************
 _lsa_GetUserName
 ***************************************************************************/

NTSTATUS _lsa_GetUserName(pipes_struct *p,
			  struct lsa_GetUserName *r)
{
	const char *username, *domname;
	struct lsa_String *account_name = NULL;
	struct lsa_String *authority_name = NULL;

	if (r->in.account_name &&
	   *r->in.account_name) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (r->in.authority_name &&
	   *r->in.authority_name) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (p->server_info->guest) {
		/*
		 * I'm 99% sure this is not the right place to do this,
		 * global_sid_Anonymous should probably be put into the token
		 * instead of the guest id -- vl
		 */
		if (!lookup_sid(p->mem_ctx, &global_sid_Anonymous,
				&domname, &username, NULL)) {
			return NT_STATUS_NO_MEMORY;
		}
	} else {
		username = p->server_info->sanitized_username;
		domname = pdb_get_domain(p->server_info->sam_account);
	}

	account_name = TALLOC_P(p->mem_ctx, struct lsa_String);
	if (!account_name) {
		return NT_STATUS_NO_MEMORY;
	}
	init_lsa_String(account_name, username);

	if (r->out.authority_name) {
		authority_name = TALLOC_P(p->mem_ctx, struct lsa_String);
		if (!authority_name) {
			return NT_STATUS_NO_MEMORY;
		}
		init_lsa_String(authority_name, domname);
	}

	*r->out.account_name = account_name;
	if (r->out.authority_name) {
		*r->out.authority_name = authority_name;
	}

	return NT_STATUS_OK;
}

/***************************************************************************
 _lsa_CreateAccount
 ***************************************************************************/

NTSTATUS _lsa_CreateAccount(pipes_struct *p,
			    struct lsa_CreateAccount *r)
{
	NTSTATUS status;
	struct lsa_info *handle;
	struct lsa_info *info;
	uint32_t acc_granted;
	struct security_descriptor *psd;
	size_t sd_size;

	/* find the connection policy handle. */
	if (!find_policy_by_hnd(p, r->in.handle, (void **)(void *)&handle))
		return NT_STATUS_INVALID_HANDLE;

	if (handle->type != LSA_HANDLE_POLICY_TYPE) {
		return NT_STATUS_INVALID_HANDLE;
	}

	/* check if the user has enough rights */

	if (!(handle->access & LSA_POLICY_CREATE_ACCOUNT)) {
		return NT_STATUS_ACCESS_DENIED;
	}

	/* Work out max allowed. */
	map_max_allowed_access(p->server_info->ptok,
			       &p->server_info->utok,
			       &r->in.access_mask);

	/* map the generic bits to the lsa policy ones */
	se_map_generic(&r->in.access_mask, &lsa_account_mapping);

	status = make_lsa_object_sd(p->mem_ctx, &psd, &sd_size,
				    &lsa_account_mapping,
				    r->in.sid, LSA_POLICY_ALL_ACCESS);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = access_check_object(psd, p->server_info->ptok,
				     NULL, 0, r->in.access_mask,
				     &acc_granted, "_lsa_CreateAccount");
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if ( is_privileged_sid( r->in.sid ) )
		return NT_STATUS_OBJECT_NAME_COLLISION;

	/* associate the user/group SID with the (unique) handle. */

	info = TALLOC_ZERO_P(p->mem_ctx, struct lsa_info);
	if (info == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	info->sid = *r->in.sid;
	info->access = acc_granted;
	info->type = LSA_HANDLE_ACCOUNT_TYPE;

	/* get a (unique) handle.  open a policy on it. */
	if (!create_policy_hnd(p, r->out.acct_handle, info))
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;

	return privilege_create_account( &info->sid );
}

/***************************************************************************
 _lsa_OpenAccount
 ***************************************************************************/

NTSTATUS _lsa_OpenAccount(pipes_struct *p,
			  struct lsa_OpenAccount *r)
{
	struct lsa_info *handle;
	struct lsa_info *info;
	SEC_DESC *psd = NULL;
	size_t sd_size;
	uint32_t des_access = r->in.access_mask;
	uint32_t acc_granted;
	NTSTATUS status;

	/* find the connection policy handle. */
	if (!find_policy_by_hnd(p, r->in.handle, (void **)(void *)&handle))
		return NT_STATUS_INVALID_HANDLE;

	if (handle->type != LSA_HANDLE_POLICY_TYPE) {
		return NT_STATUS_INVALID_HANDLE;
	}

	/* des_access is for the account here, not the policy
 	 * handle - so don't check against policy handle. */

	/* Work out max allowed. */
	map_max_allowed_access(p->server_info->ptok,
			       &p->server_info->utok,
			       &des_access);

	/* map the generic bits to the lsa account ones */
	se_map_generic(&des_access, &lsa_account_mapping);

	/* get the generic lsa account SD until we store it */
	status = make_lsa_object_sd(p->mem_ctx, &psd, &sd_size,
				&lsa_account_mapping,
				r->in.sid, LSA_ACCOUNT_ALL_ACCESS);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = access_check_object(psd, p->server_info->ptok,
				     NULL, 0, des_access,
				     &acc_granted, "_lsa_OpenAccount" );
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	/* TODO: Fis the parsing routine before reenabling this check! */
	#if 0
	if (!lookup_sid(&handle->sid, dom_name, name, &type))
		return NT_STATUS_ACCESS_DENIED;
	#endif
	/* associate the user/group SID with the (unique) handle. */
	info = TALLOC_ZERO_P(p->mem_ctx, struct lsa_info);
	if (info == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	info->sid = *r->in.sid;
	info->access = acc_granted;
	info->type = LSA_HANDLE_ACCOUNT_TYPE;

	/* get a (unique) handle.  open a policy on it. */
	if (!create_policy_hnd(p, r->out.acct_handle, info))
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;

	return NT_STATUS_OK;
}

/***************************************************************************
 _lsa_EnumPrivsAccount
 For a given SID, enumerate all the privilege this account has.
 ***************************************************************************/

NTSTATUS _lsa_EnumPrivsAccount(pipes_struct *p,
			       struct lsa_EnumPrivsAccount *r)
{
	NTSTATUS status = NT_STATUS_OK;
	struct lsa_info *info=NULL;
	SE_PRIV mask;
	PRIVILEGE_SET privileges;
	struct lsa_PrivilegeSet *priv_set = NULL;
	struct lsa_LUIDAttribute *luid_attrs = NULL;
	int i;

	/* find the connection policy handle. */
	if (!find_policy_by_hnd(p, r->in.handle, (void **)(void *)&info))
		return NT_STATUS_INVALID_HANDLE;

	if (info->type != LSA_HANDLE_ACCOUNT_TYPE) {
		return NT_STATUS_INVALID_HANDLE;
	}

	if (!(info->access & LSA_ACCOUNT_VIEW))
		return NT_STATUS_ACCESS_DENIED;

	get_privileges_for_sids(&mask, &info->sid, 1);

	privilege_set_init( &privileges );

	priv_set = TALLOC_ZERO_P(p->mem_ctx, struct lsa_PrivilegeSet);
	if (!priv_set) {
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}

	if ( se_priv_to_privilege_set( &privileges, &mask ) ) {

		DEBUG(10,("_lsa_EnumPrivsAccount: %s has %d privileges\n",
			  sid_string_dbg(&info->sid),
			  privileges.count));

		luid_attrs = TALLOC_ZERO_ARRAY(p->mem_ctx,
					       struct lsa_LUIDAttribute,
					       privileges.count);
		if (!luid_attrs) {
			status = NT_STATUS_NO_MEMORY;
			goto done;
		}

		for (i=0; i<privileges.count; i++) {
			luid_attrs[i].luid.low = privileges.set[i].luid.low;
			luid_attrs[i].luid.high = privileges.set[i].luid.high;
			luid_attrs[i].attribute = privileges.set[i].attr;
		}

		priv_set->count = privileges.count;
		priv_set->unknown = 0;
		priv_set->set = luid_attrs;

	} else {
		priv_set->count = 0;
		priv_set->unknown = 0;
		priv_set->set = NULL;
	}

	*r->out.privs = priv_set;

 done:
	privilege_set_free( &privileges );

	return status;
}

/***************************************************************************
 _lsa_GetSystemAccessAccount
 ***************************************************************************/

NTSTATUS _lsa_GetSystemAccessAccount(pipes_struct *p,
				     struct lsa_GetSystemAccessAccount *r)
{
	NTSTATUS status;
	struct lsa_info *info = NULL;
	struct lsa_EnumPrivsAccount e;
	struct lsa_PrivilegeSet *privset;

	/* find the connection policy handle. */

	if (!find_policy_by_hnd(p, r->in.handle, (void **)(void *)&info))
		return NT_STATUS_INVALID_HANDLE;

	if (info->type != LSA_HANDLE_ACCOUNT_TYPE) {
		return NT_STATUS_INVALID_HANDLE;
	}

	if (!(info->access & LSA_ACCOUNT_VIEW))
		return NT_STATUS_ACCESS_DENIED;

	privset = talloc_zero(p->mem_ctx, struct lsa_PrivilegeSet);
	if (!privset) {
		return NT_STATUS_NO_MEMORY;
	}

	e.in.handle = r->in.handle;
	e.out.privs = &privset;

	status = _lsa_EnumPrivsAccount(p, &e);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10,("_lsa_GetSystemAccessAccount: "
			"failed to call _lsa_EnumPrivsAccount(): %s\n",
			nt_errstr(status)));
		return status;
	}

	/* Samba4 would iterate over the privset to merge the policy mode bits,
	 * not sure samba3 can do the same here, so just return what we did in
	 * the past - gd */

	/*
	  0x01 -> Log on locally
	  0x02 -> Access this computer from network
	  0x04 -> Log on as a batch job
	  0x10 -> Log on as a service

	  they can be ORed together
	*/

	*r->out.access_mask = LSA_POLICY_MODE_INTERACTIVE |
			      LSA_POLICY_MODE_NETWORK;

	return NT_STATUS_OK;
}

/***************************************************************************
  update the systemaccount information
 ***************************************************************************/

NTSTATUS _lsa_SetSystemAccessAccount(pipes_struct *p,
				     struct lsa_SetSystemAccessAccount *r)
{
	struct lsa_info *info=NULL;
	GROUP_MAP map;

	/* find the connection policy handle. */
	if (!find_policy_by_hnd(p, r->in.handle, (void **)(void *)&info))
		return NT_STATUS_INVALID_HANDLE;

	if (info->type != LSA_HANDLE_ACCOUNT_TYPE) {
		return NT_STATUS_INVALID_HANDLE;
	}

	if (!(info->access & LSA_ACCOUNT_ADJUST_SYSTEM_ACCESS)) {
		return NT_STATUS_ACCESS_DENIED;
	}

	if (!pdb_getgrsid(&map, info->sid))
		return NT_STATUS_NO_SUCH_GROUP;

	return pdb_update_group_mapping_entry(&map);
}

/***************************************************************************
 _lsa_AddPrivilegesToAccount
 For a given SID, add some privileges.
 ***************************************************************************/

NTSTATUS _lsa_AddPrivilegesToAccount(pipes_struct *p,
				     struct lsa_AddPrivilegesToAccount *r)
{
	struct lsa_info *info = NULL;
	SE_PRIV mask;
	struct lsa_PrivilegeSet *set = NULL;

	/* find the connection policy handle. */
	if (!find_policy_by_hnd(p, r->in.handle, (void **)(void *)&info))
		return NT_STATUS_INVALID_HANDLE;

	if (info->type != LSA_HANDLE_ACCOUNT_TYPE) {
		return NT_STATUS_INVALID_HANDLE;
	}

	if (!(info->access & LSA_ACCOUNT_ADJUST_PRIVILEGES)) {
		return NT_STATUS_ACCESS_DENIED;
	}

	set = r->in.privs;
	if ( !privilege_set_to_se_priv( &mask, set ) )
		return NT_STATUS_NO_SUCH_PRIVILEGE;

	if ( !grant_privilege( &info->sid, &mask ) ) {
		DEBUG(3,("_lsa_AddPrivilegesToAccount: grant_privilege(%s) failed!\n",
			 sid_string_dbg(&info->sid) ));
		DEBUG(3,("Privilege mask:\n"));
		dump_se_priv( DBGC_ALL, 3, &mask );
		return NT_STATUS_NO_SUCH_PRIVILEGE;
	}

	return NT_STATUS_OK;
}

/***************************************************************************
 _lsa_RemovePrivilegesFromAccount
 For a given SID, remove some privileges.
 ***************************************************************************/

NTSTATUS _lsa_RemovePrivilegesFromAccount(pipes_struct *p,
					  struct lsa_RemovePrivilegesFromAccount *r)
{
	struct lsa_info *info = NULL;
	SE_PRIV mask;
	struct lsa_PrivilegeSet *set = NULL;

	/* find the connection policy handle. */
	if (!find_policy_by_hnd(p, r->in.handle, (void **)(void *)&info))
		return NT_STATUS_INVALID_HANDLE;

	if (info->type != LSA_HANDLE_ACCOUNT_TYPE) {
		return NT_STATUS_INVALID_HANDLE;
	}

	if (!(info->access & LSA_ACCOUNT_ADJUST_PRIVILEGES)) {
		return NT_STATUS_ACCESS_DENIED;
	}

	set = r->in.privs;

	if ( !privilege_set_to_se_priv( &mask, set ) )
		return NT_STATUS_NO_SUCH_PRIVILEGE;

	if ( !revoke_privilege( &info->sid, &mask ) ) {
		DEBUG(3,("_lsa_RemovePrivilegesFromAccount: revoke_privilege(%s) failed!\n",
			 sid_string_dbg(&info->sid) ));
		DEBUG(3,("Privilege mask:\n"));
		dump_se_priv( DBGC_ALL, 3, &mask );
		return NT_STATUS_NO_SUCH_PRIVILEGE;
	}

	return NT_STATUS_OK;
}

/***************************************************************************
 _lsa_LookupPrivName
 ***************************************************************************/

NTSTATUS _lsa_LookupPrivName(pipes_struct *p,
			     struct lsa_LookupPrivName *r)
{
	struct lsa_info *info = NULL;
	const char *name;
	struct lsa_StringLarge *lsa_name;

	/* find the connection policy handle. */
	if (!find_policy_by_hnd(p, r->in.handle, (void **)(void *)&info)) {
		return NT_STATUS_INVALID_HANDLE;
	}

	if (info->type != LSA_HANDLE_POLICY_TYPE) {
		return NT_STATUS_INVALID_HANDLE;
	}

	if (!(info->access & LSA_POLICY_VIEW_LOCAL_INFORMATION)) {
		return NT_STATUS_ACCESS_DENIED;
	}

	name = luid_to_privilege_name((LUID *)r->in.luid);
	if (!name) {
		return NT_STATUS_NO_SUCH_PRIVILEGE;
	}

	lsa_name = TALLOC_ZERO_P(p->mem_ctx, struct lsa_StringLarge);
	if (!lsa_name) {
		return NT_STATUS_NO_MEMORY;
	}

	lsa_name->string = talloc_strdup(lsa_name, name);
	if (!lsa_name->string) {
		TALLOC_FREE(lsa_name);
		return NT_STATUS_NO_MEMORY;
	}

	*r->out.name = lsa_name;

	return NT_STATUS_OK;
}

/***************************************************************************
 _lsa_QuerySecurity
 ***************************************************************************/

NTSTATUS _lsa_QuerySecurity(pipes_struct *p,
			    struct lsa_QuerySecurity *r)
{
	struct lsa_info *handle=NULL;
	SEC_DESC *psd = NULL;
	size_t sd_size;
	NTSTATUS status;

	/* find the connection policy handle. */
	if (!find_policy_by_hnd(p, r->in.handle, (void **)(void *)&handle))
		return NT_STATUS_INVALID_HANDLE;

	switch (handle->type) {
	case LSA_HANDLE_POLICY_TYPE:
		status = make_lsa_object_sd(p->mem_ctx, &psd, &sd_size,
				&lsa_policy_mapping, NULL, 0);
		break;
	case LSA_HANDLE_ACCOUNT_TYPE:
		status = make_lsa_object_sd(p->mem_ctx, &psd, &sd_size,
				&lsa_account_mapping,
				&handle->sid, LSA_ACCOUNT_ALL_ACCESS);
		break;
	default:
		status = NT_STATUS_INVALID_HANDLE;
		break;
	}

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	*r->out.sdbuf = make_sec_desc_buf(p->mem_ctx, sd_size, psd);
	if (!*r->out.sdbuf) {
		return NT_STATUS_NO_MEMORY;
	}

	return status;
}

/***************************************************************************
 _lsa_AddAccountRights
 ***************************************************************************/

NTSTATUS _lsa_AddAccountRights(pipes_struct *p,
			       struct lsa_AddAccountRights *r)
{
	struct lsa_info *info = NULL;
	int i = 0;
	uint32_t acc_granted = 0;
	SEC_DESC *psd = NULL;
	size_t sd_size;
	DOM_SID sid;
	NTSTATUS status;

	/* find the connection policy handle. */
	if (!find_policy_by_hnd(p, r->in.handle, (void **)(void *)&info))
		return NT_STATUS_INVALID_HANDLE;

	if (info->type != LSA_HANDLE_POLICY_TYPE) {
		return NT_STATUS_INVALID_HANDLE;
	}

        /* get the generic lsa account SD for this SID until we store it */
        status = make_lsa_object_sd(p->mem_ctx, &psd, &sd_size,
                                &lsa_account_mapping,
                                r->in.sid, LSA_ACCOUNT_ALL_ACCESS);
        if (!NT_STATUS_IS_OK(status)) {
                return status;
        }

	/*
	 * From the MS DOCs. If the sid doesn't exist, ask for LSA_POLICY_CREATE_ACCOUNT
 	 * on the policy handle. If it does, ask for
 	 * LSA_ACCOUNT_ADJUST_PRIVILEGES|LSA_ACCOUNT_ADJUST_SYSTEM_ACCESS|LSA_ACCOUNT_VIEW,
 	 * on the account sid. We don't check here so just use the latter. JRA.
 	 */

	status = access_check_object(psd, p->server_info->ptok,
				     NULL, 0,
				     LSA_ACCOUNT_ADJUST_PRIVILEGES|LSA_ACCOUNT_ADJUST_SYSTEM_ACCESS|LSA_ACCOUNT_VIEW,
				     &acc_granted, "_lsa_AddAccountRights" );
        if (!NT_STATUS_IS_OK(status)) {
                return status;
        }

	/* according to an NT4 PDC, you can add privileges to SIDs even without
	   call_lsa_create_account() first.  And you can use any arbitrary SID. */

	sid_copy( &sid, r->in.sid );

	for ( i=0; i < r->in.rights->count; i++ ) {

		const char *privname = r->in.rights->names[i].string;

		/* only try to add non-null strings */

		if ( !privname )
			continue;

		if ( !grant_privilege_by_name( &sid, privname ) ) {
			DEBUG(2,("_lsa_AddAccountRights: Failed to add privilege [%s]\n",
				privname ));
			return NT_STATUS_NO_SUCH_PRIVILEGE;
		}
	}

	return NT_STATUS_OK;
}

/***************************************************************************
 _lsa_RemoveAccountRights
 ***************************************************************************/

NTSTATUS _lsa_RemoveAccountRights(pipes_struct *p,
				  struct lsa_RemoveAccountRights *r)
{
	struct lsa_info *info = NULL;
	int i = 0;
	SEC_DESC *psd = NULL;
	size_t sd_size;
	DOM_SID sid;
	const char *privname = NULL;
	uint32_t acc_granted = 0;
	NTSTATUS status;

	/* find the connection policy handle. */
	if (!find_policy_by_hnd(p, r->in.handle, (void **)(void *)&info))
		return NT_STATUS_INVALID_HANDLE;

	if (info->type != LSA_HANDLE_POLICY_TYPE) {
		return NT_STATUS_INVALID_HANDLE;
	}

        /* get the generic lsa account SD for this SID until we store it */
        status = make_lsa_object_sd(p->mem_ctx, &psd, &sd_size,
                                &lsa_account_mapping,
                                r->in.sid, LSA_ACCOUNT_ALL_ACCESS);
        if (!NT_STATUS_IS_OK(status)) {
                return status;
        }

	/*
	 * From the MS DOCs. We need
	 * LSA_ACCOUNT_ADJUST_PRIVILEGES|LSA_ACCOUNT_ADJUST_SYSTEM_ACCESS|LSA_ACCOUNT_VIEW
	 * and DELETE on the account sid.
 	 */

	status = access_check_object(psd, p->server_info->ptok,
				     NULL, 0,
				     LSA_ACCOUNT_ADJUST_PRIVILEGES|LSA_ACCOUNT_ADJUST_SYSTEM_ACCESS|
				     LSA_ACCOUNT_VIEW|STD_RIGHT_DELETE_ACCESS,
				     &acc_granted, "_lsa_RemoveAccountRights");
        if (!NT_STATUS_IS_OK(status)) {
                return status;
        }

	sid_copy( &sid, r->in.sid );

	if ( r->in.remove_all ) {
		if ( !revoke_all_privileges( &sid ) )
			return NT_STATUS_ACCESS_DENIED;

		return NT_STATUS_OK;
	}

	for ( i=0; i < r->in.rights->count; i++ ) {

		privname = r->in.rights->names[i].string;

		/* only try to add non-null strings */

		if ( !privname )
			continue;

		if ( !revoke_privilege_by_name( &sid, privname ) ) {
			DEBUG(2,("_lsa_RemoveAccountRights: Failed to revoke privilege [%s]\n",
				privname ));
			return NT_STATUS_NO_SUCH_PRIVILEGE;
		}
	}

	return NT_STATUS_OK;
}

/*******************************************************************
********************************************************************/

static NTSTATUS init_lsa_right_set(TALLOC_CTX *mem_ctx,
				   struct lsa_RightSet *r,
				   PRIVILEGE_SET *privileges)
{
	uint32 i;
	const char *privname;
	const char **privname_array = NULL;
	int num_priv = 0;

	for (i=0; i<privileges->count; i++) {

		privname = luid_to_privilege_name(&privileges->set[i].luid);
		if (privname) {
			if (!add_string_to_array(mem_ctx, privname,
						 &privname_array, &num_priv)) {
				return NT_STATUS_NO_MEMORY;
			}
		}
	}

	if (num_priv) {

		r->names = TALLOC_ZERO_ARRAY(mem_ctx, struct lsa_StringLarge,
					     num_priv);
		if (!r->names) {
			return NT_STATUS_NO_MEMORY;
		}

		for (i=0; i<num_priv; i++) {
			init_lsa_StringLarge(&r->names[i], privname_array[i]);
		}

		r->count = num_priv;
	}

	return NT_STATUS_OK;
}

/***************************************************************************
 _lsa_EnumAccountRights
 ***************************************************************************/

NTSTATUS _lsa_EnumAccountRights(pipes_struct *p,
				struct lsa_EnumAccountRights *r)
{
	NTSTATUS status;
	struct lsa_info *info = NULL;
	DOM_SID sid;
	PRIVILEGE_SET privileges;
	SE_PRIV mask;

	/* find the connection policy handle. */

	if (!find_policy_by_hnd(p, r->in.handle, (void **)(void *)&info))
		return NT_STATUS_INVALID_HANDLE;

	if (info->type != LSA_HANDLE_POLICY_TYPE) {
		return NT_STATUS_INVALID_HANDLE;
	}

	if (!(info->access & LSA_ACCOUNT_VIEW)) {
		return NT_STATUS_ACCESS_DENIED;
	}

	/* according to an NT4 PDC, you can add privileges to SIDs even without
	   call_lsa_create_account() first.  And you can use any arbitrary SID. */

	sid_copy( &sid, r->in.sid );

	/* according to MS-LSAD 3.1.4.5.10 it is required to return
	 * NT_STATUS_OBJECT_NAME_NOT_FOUND if the account sid was not found in
	 * the lsa database */

	if (!get_privileges_for_sids(&mask, &sid, 1)) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	status = privilege_set_init(&privileges);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	se_priv_to_privilege_set(&privileges, &mask);

	DEBUG(10,("_lsa_EnumAccountRights: %s has %d privileges\n",
		  sid_string_dbg(&sid), privileges.count));

	status = init_lsa_right_set(p->mem_ctx, r->out.rights, &privileges);

	privilege_set_free( &privileges );

	return status;
}

/***************************************************************************
 _lsa_LookupPrivValue
 ***************************************************************************/

NTSTATUS _lsa_LookupPrivValue(pipes_struct *p,
			      struct lsa_LookupPrivValue *r)
{
	struct lsa_info *info = NULL;
	const char *name = NULL;
	LUID_ATTR priv_luid;
	SE_PRIV mask;

	/* find the connection policy handle. */

	if (!find_policy_by_hnd(p, r->in.handle, (void **)(void *)&info))
		return NT_STATUS_INVALID_HANDLE;

	if (info->type != LSA_HANDLE_POLICY_TYPE) {
		return NT_STATUS_INVALID_HANDLE;
	}

	if (!(info->access & LSA_POLICY_LOOKUP_NAMES))
		return NT_STATUS_ACCESS_DENIED;

	name = r->in.name->string;

	DEBUG(10,("_lsa_lookup_priv_value: name = %s\n", name));

	if ( !se_priv_from_name( name, &mask ) )
		return NT_STATUS_NO_SUCH_PRIVILEGE;

	priv_luid = get_privilege_luid( &mask );

	r->out.luid->low = priv_luid.luid.low;
	r->out.luid->high = priv_luid.luid.high;

	return NT_STATUS_OK;
}

/***************************************************************************
 _lsa_EnumAccountsWithUserRight
 ***************************************************************************/

NTSTATUS _lsa_EnumAccountsWithUserRight(pipes_struct *p,
					struct lsa_EnumAccountsWithUserRight *r)
{
	NTSTATUS status;
	struct lsa_info *info = NULL;
	struct dom_sid *sids = NULL;
	int num_sids = 0;
	uint32_t i;
	SE_PRIV mask;

	if (!find_policy_by_hnd(p, r->in.handle, (void **)(void *)&info)) {
		return NT_STATUS_INVALID_HANDLE;
	}

	if (info->type != LSA_HANDLE_POLICY_TYPE) {
		return NT_STATUS_INVALID_HANDLE;
	}

	if (!(info->access & LSA_POLICY_LOOKUP_NAMES)) {
		return NT_STATUS_ACCESS_DENIED;
	}

	if (!r->in.name || !r->in.name->string) {
		return NT_STATUS_NO_SUCH_PRIVILEGE;
	}

	if (!se_priv_from_name(r->in.name->string, &mask)) {
		return NT_STATUS_NO_SUCH_PRIVILEGE;
	}

	status = privilege_enum_sids(&mask, p->mem_ctx,
				     &sids, &num_sids);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	r->out.sids->num_sids = num_sids;
	r->out.sids->sids = talloc_array(p->mem_ctx, struct lsa_SidPtr,
					 r->out.sids->num_sids);

	for (i=0; i < r->out.sids->num_sids; i++) {
		r->out.sids->sids[i].sid = sid_dup_talloc(r->out.sids->sids,
							  &sids[i]);
		if (!r->out.sids->sids[i].sid) {
			TALLOC_FREE(r->out.sids->sids);
			r->out.sids->num_sids = 0;
			return NT_STATUS_NO_MEMORY;
		}
	}

	return NT_STATUS_OK;
}

/***************************************************************************
 _lsa_Delete
 ***************************************************************************/

NTSTATUS _lsa_Delete(pipes_struct *p,
		     struct lsa_Delete *r)
{
	return NT_STATUS_NOT_SUPPORTED;
}

/*
 * From here on the server routines are just dummy ones to make smbd link with
 * librpc/gen_ndr/srv_lsa.c. These routines are actually never called, we are
 * pulling the server stubs across one by one.
 */

NTSTATUS _lsa_SetSecObj(pipes_struct *p, struct lsa_SetSecObj *r)
{
	p->rng_fault_state = True;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_ChangePassword(pipes_struct *p, struct lsa_ChangePassword *r)
{
	p->rng_fault_state = True;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_SetInfoPolicy(pipes_struct *p, struct lsa_SetInfoPolicy *r)
{
	p->rng_fault_state = True;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_ClearAuditLog(pipes_struct *p, struct lsa_ClearAuditLog *r)
{
	p->rng_fault_state = True;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_GetQuotasForAccount(pipes_struct *p, struct lsa_GetQuotasForAccount *r)
{
	p->rng_fault_state = True;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_SetQuotasForAccount(pipes_struct *p, struct lsa_SetQuotasForAccount *r)
{
	p->rng_fault_state = True;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_QueryTrustedDomainInfo(pipes_struct *p, struct lsa_QueryTrustedDomainInfo *r)
{
	p->rng_fault_state = True;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_SetInformationTrustedDomain(pipes_struct *p, struct lsa_SetInformationTrustedDomain *r)
{
	p->rng_fault_state = True;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_QuerySecret(pipes_struct *p, struct lsa_QuerySecret *r)
{
	p->rng_fault_state = True;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_QueryTrustedDomainInfoBySid(pipes_struct *p, struct lsa_QueryTrustedDomainInfoBySid *r)
{
	p->rng_fault_state = True;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_SetTrustedDomainInfo(pipes_struct *p, struct lsa_SetTrustedDomainInfo *r)
{
	p->rng_fault_state = True;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_DeleteTrustedDomain(pipes_struct *p, struct lsa_DeleteTrustedDomain *r)
{
	p->rng_fault_state = True;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_StorePrivateData(pipes_struct *p, struct lsa_StorePrivateData *r)
{
	p->rng_fault_state = True;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_RetrievePrivateData(pipes_struct *p, struct lsa_RetrievePrivateData *r)
{
	p->rng_fault_state = True;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_SetInfoPolicy2(pipes_struct *p, struct lsa_SetInfoPolicy2 *r)
{
	p->rng_fault_state = True;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_QueryTrustedDomainInfoByName(pipes_struct *p, struct lsa_QueryTrustedDomainInfoByName *r)
{
	p->rng_fault_state = True;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_SetTrustedDomainInfoByName(pipes_struct *p, struct lsa_SetTrustedDomainInfoByName *r)
{
	p->rng_fault_state = True;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_EnumTrustedDomainsEx(pipes_struct *p, struct lsa_EnumTrustedDomainsEx *r)
{
	p->rng_fault_state = True;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_CreateTrustedDomainEx(pipes_struct *p, struct lsa_CreateTrustedDomainEx *r)
{
	p->rng_fault_state = True;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_CloseTrustedDomainEx(pipes_struct *p, struct lsa_CloseTrustedDomainEx *r)
{
	p->rng_fault_state = True;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_QueryDomainInformationPolicy(pipes_struct *p, struct lsa_QueryDomainInformationPolicy *r)
{
	p->rng_fault_state = True;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_SetDomainInformationPolicy(pipes_struct *p, struct lsa_SetDomainInformationPolicy *r)
{
	p->rng_fault_state = True;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_OpenTrustedDomainByName(pipes_struct *p, struct lsa_OpenTrustedDomainByName *r)
{
	p->rng_fault_state = True;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_TestCall(pipes_struct *p, struct lsa_TestCall *r)
{
	p->rng_fault_state = True;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_CreateTrustedDomainEx2(pipes_struct *p, struct lsa_CreateTrustedDomainEx2 *r)
{
	p->rng_fault_state = True;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_CREDRWRITE(pipes_struct *p, struct lsa_CREDRWRITE *r)
{
	p->rng_fault_state = True;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_CREDRREAD(pipes_struct *p, struct lsa_CREDRREAD *r)
{
	p->rng_fault_state = True;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_CREDRENUMERATE(pipes_struct *p, struct lsa_CREDRENUMERATE *r)
{
	p->rng_fault_state = True;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_CREDRWRITEDOMAINCREDENTIALS(pipes_struct *p, struct lsa_CREDRWRITEDOMAINCREDENTIALS *r)
{
	p->rng_fault_state = True;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_CREDRREADDOMAINCREDENTIALS(pipes_struct *p, struct lsa_CREDRREADDOMAINCREDENTIALS *r)
{
	p->rng_fault_state = True;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_CREDRDELETE(pipes_struct *p, struct lsa_CREDRDELETE *r)
{
	p->rng_fault_state = True;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_CREDRGETTARGETINFO(pipes_struct *p, struct lsa_CREDRGETTARGETINFO *r)
{
	p->rng_fault_state = True;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_CREDRPROFILELOADED(pipes_struct *p, struct lsa_CREDRPROFILELOADED *r)
{
	p->rng_fault_state = True;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_CREDRGETSESSIONTYPES(pipes_struct *p, struct lsa_CREDRGETSESSIONTYPES *r)
{
	p->rng_fault_state = True;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_LSARREGISTERAUDITEVENT(pipes_struct *p, struct lsa_LSARREGISTERAUDITEVENT *r)
{
	p->rng_fault_state = True;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_LSARGENAUDITEVENT(pipes_struct *p, struct lsa_LSARGENAUDITEVENT *r)
{
	p->rng_fault_state = True;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_LSARUNREGISTERAUDITEVENT(pipes_struct *p, struct lsa_LSARUNREGISTERAUDITEVENT *r)
{
	p->rng_fault_state = True;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_lsaRQueryForestTrustInformation(pipes_struct *p, struct lsa_lsaRQueryForestTrustInformation *r)
{
	p->rng_fault_state = True;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_LSARSETFORESTTRUSTINFORMATION(pipes_struct *p, struct lsa_LSARSETFORESTTRUSTINFORMATION *r)
{
	p->rng_fault_state = True;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_CREDRRENAME(pipes_struct *p, struct lsa_CREDRRENAME *r)
{
	p->rng_fault_state = True;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_LSAROPENPOLICYSCE(pipes_struct *p, struct lsa_LSAROPENPOLICYSCE *r)
{
	p->rng_fault_state = True;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_LSARADTREGISTERSECURITYEVENTSOURCE(pipes_struct *p, struct lsa_LSARADTREGISTERSECURITYEVENTSOURCE *r)
{
	p->rng_fault_state = True;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_LSARADTUNREGISTERSECURITYEVENTSOURCE(pipes_struct *p, struct lsa_LSARADTUNREGISTERSECURITYEVENTSOURCE *r)
{
	p->rng_fault_state = True;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_LSARADTREPORTSECURITYEVENT(pipes_struct *p, struct lsa_LSARADTREPORTSECURITYEVENT *r)
{
	p->rng_fault_state = True;
	return NT_STATUS_NOT_IMPLEMENTED;
}
