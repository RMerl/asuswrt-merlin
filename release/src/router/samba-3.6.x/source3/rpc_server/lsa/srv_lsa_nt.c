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
 *  Copyright (C) Andrew Bartlett		    2010.
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
#include "ntdomain.h"
#include "../librpc/gen_ndr/srv_lsa.h"
#include "secrets.h"
#include "../librpc/gen_ndr/netlogon.h"
#include "rpc_client/init_lsa.h"
#include "../libcli/security/security.h"
#include "../libcli/security/dom_sid.h"
#include "../librpc/gen_ndr/drsblobs.h"
#include "../librpc/gen_ndr/ndr_drsblobs.h"
#include "../lib/crypto/arcfour.h"
#include "../libcli/security/dom_sid.h"
#include "../librpc/gen_ndr/ndr_security.h"
#include "passdb.h"
#include "auth.h"
#include "lib/privileges.h"
#include "rpc_server/srv_access_check.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_RPC_SRV

#define MAX_LOOKUP_SIDS 0x5000 /* 20480 */

enum lsa_handle_type {
	LSA_HANDLE_POLICY_TYPE = 1,
	LSA_HANDLE_ACCOUNT_TYPE = 2,
	LSA_HANDLE_TRUST_TYPE = 3};

struct lsa_info {
	struct dom_sid sid;
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
				    struct dom_sid *dom_sid)
{
	int num = 0;

	if (dom_name != NULL) {
		for (num = 0; num < ref->count; num++) {
			if (dom_sid_equal(dom_sid, ref->domains[num].sid)) {
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
	ref->domains[num].sid = dom_sid_dup(mem_ctx, dom_sid);
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
			     struct dom_sid *sid)
{
	init_lsa_StringLarge(&r->name, name);
	r->sid = sid;
}

/***************************************************************************
 initialize a lsa_DomainInfo structure.
 ***************************************************************************/

static void init_dom_query_5(struct lsa_DomainInfo *r,
			     const char *name,
			     struct dom_sid *sid)
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
		struct dom_sid sid;
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
		struct dom_sid sid;
		uint32 rid;
		int dom_idx;
		const char *full_name;
		const char *domain;
		enum lsa_SidType type;

		ZERO_STRUCT(sid);

		/* Split name into domain and user component */

		full_name = name[i].string;
		if (full_name == NULL) {
			return NT_STATUS_NO_MEMORY;
		}

		DEBUG(5, ("init_lsa_sids: looking up name %s\n", full_name));

		if (!lookup_name(mem_ctx, full_name, flags, &domain, NULL,
				 &sid, &type)) {
			type = SID_NAME_UNKNOWN;
		}

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
			struct dom_sid domain_sid;
			sid_copy(&domain_sid, &sid);
			sid_split_rid(&domain_sid, &rid);
			dom_idx = init_lsa_ref_domain_list(mem_ctx, ref, domain, &domain_sid);
			mapped_count++;
		}

		/* Initialize the lsa_TranslatedSid3 return. */
		trans_sids[i].sid_type = type;
		trans_sids[i].sid = dom_sid_dup(mem_ctx, &sid);
		trans_sids[i].sid_index = dom_idx;
	}

	*pmapped_count = mapped_count;
	return NT_STATUS_OK;
}

static NTSTATUS make_lsa_object_sd(TALLOC_CTX *mem_ctx, struct security_descriptor **sd, size_t *sd_size,
					const struct generic_mapping *map,
					struct dom_sid *sid, uint32_t sid_access)
{
	struct dom_sid adm_sid;
	struct security_ace ace[5];
	size_t i = 0;

	struct security_acl *psa = NULL;

	/* READ|EXECUTE access for Everyone */

	init_sec_ace(&ace[i++], &global_sid_World, SEC_ACE_TYPE_ACCESS_ALLOWED,
			map->generic_execute | map->generic_read, 0);

	/* Add Full Access 'BUILTIN\Administrators' and 'BUILTIN\Account Operators */

	init_sec_ace(&ace[i++], &global_sid_Builtin_Administrators,
			SEC_ACE_TYPE_ACCESS_ALLOWED, map->generic_all, 0);
	init_sec_ace(&ace[i++], &global_sid_Builtin_Account_Operators,
			SEC_ACE_TYPE_ACCESS_ALLOWED, map->generic_all, 0);

	/* Add Full Access for Domain Admins */
	sid_compose(&adm_sid, get_global_sam_sid(), DOMAIN_RID_ADMINS);
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
 ***************************************************************************/

static NTSTATUS create_lsa_policy_handle(TALLOC_CTX *mem_ctx,
					 struct pipes_struct *p,
					 enum lsa_handle_type type,
					 uint32_t acc_granted,
					 struct dom_sid *sid,
					 const char *name,
					 const struct security_descriptor *sd,
					 struct policy_handle *handle)
{
	struct lsa_info *info;

	ZERO_STRUCTP(handle);

	info = talloc_zero(mem_ctx, struct lsa_info);
	if (!info) {
		return NT_STATUS_NO_MEMORY;
	}

	info->type = type;
	info->access = acc_granted;

	if (sid) {
		sid_copy(&info->sid, sid);
	}

	info->name = talloc_strdup(info, name);

	if (sd) {
		info->sd = dup_sec_desc(info, sd);
		if (!info->sd) {
			talloc_free(info);
			return NT_STATUS_NO_MEMORY;
		}
	}

	if (!create_policy_hnd(p, handle, info)) {
		talloc_free(info);
		ZERO_STRUCTP(handle);
		return NT_STATUS_NO_MEMORY;
	}

	return NT_STATUS_OK;
}

/***************************************************************************
 _lsa_OpenPolicy2
 ***************************************************************************/

NTSTATUS _lsa_OpenPolicy2(struct pipes_struct *p,
			  struct lsa_OpenPolicy2 *r)
{
	struct security_descriptor *psd = NULL;
	size_t sd_size;
	uint32 des_access = r->in.access_mask;
	uint32 acc_granted;
	NTSTATUS status;

	/* Work out max allowed. */
	map_max_allowed_access(p->session_info->security_token,
			       &p->session_info->utok,
			       &des_access);

	/* map the generic bits to the lsa policy ones */
	se_map_generic(&des_access, &lsa_policy_mapping);

	/* get the generic lsa policy SD until we store it */
	status = make_lsa_object_sd(p->mem_ctx, &psd, &sd_size, &lsa_policy_mapping,
			NULL, 0);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = access_check_object(psd, p->session_info->security_token,
				     SEC_PRIV_INVALID, SEC_PRIV_INVALID, 0, des_access,
				     &acc_granted, "_lsa_OpenPolicy2" );
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = create_lsa_policy_handle(p->mem_ctx, p,
					  LSA_HANDLE_POLICY_TYPE,
					  acc_granted,
					  get_global_sam_sid(),
					  NULL,
					  psd,
					  r->out.handle);
	if (!NT_STATUS_IS_OK(status)) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	return NT_STATUS_OK;
}

/***************************************************************************
 _lsa_OpenPolicy
 ***************************************************************************/

NTSTATUS _lsa_OpenPolicy(struct pipes_struct *p,
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

NTSTATUS _lsa_EnumTrustDom(struct pipes_struct *p,
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

NTSTATUS _lsa_QueryInfoPolicy(struct pipes_struct *p,
			      struct lsa_QueryInfoPolicy *r)
{
	NTSTATUS status = NT_STATUS_OK;
	struct lsa_info *handle;
	struct dom_sid domain_sid;
	const char *name;
	struct dom_sid *sid = NULL;
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
				sid = dom_sid_dup(p->mem_ctx, get_global_sam_sid());
				if (!sid) {
					return NT_STATUS_NO_MEMORY;
				}
				break;
			case ROLE_DOMAIN_MEMBER:
				name = lp_workgroup();
				/* We need to return the Domain SID here. */
				if (secrets_fetch_domain_sid(lp_workgroup(), &domain_sid)) {
					sid = dom_sid_dup(p->mem_ctx, &domain_sid);
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

NTSTATUS _lsa_QueryInfoPolicy2(struct pipes_struct *p,
			       struct lsa_QueryInfoPolicy2 *r2)
{
	struct lsa_QueryInfoPolicy r;

	if ((pdb_capabilities() & PDB_CAP_ADS) == 0) {
		p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
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

static NTSTATUS _lsa_lookup_sids_internal(struct pipes_struct *p,
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
	const struct dom_sid **sids = NULL;
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

	sids = TALLOC_ARRAY(p->mem_ctx, const struct dom_sid *, num_sids);
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
			name->dom_idx = -1;
			/* Unknown sids should return the string
			 * representation of the SID. Windows 2003 behaves
			 * rather erratic here, in many cases it returns the
			 * RID as 8 bytes hex, in others it returns the full
			 * SID. We (Jerry/VL) could not figure out which the
			 * hard cases are, so leave it with the SID.  */
			name->name = dom_sid_string(p->mem_ctx, sids[i]);
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

NTSTATUS _lsa_LookupSids(struct pipes_struct *p,
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

NTSTATUS _lsa_LookupSids2(struct pipes_struct *p,
			  struct lsa_LookupSids2 *r)
{
	NTSTATUS status;
	struct lsa_info *handle;
	int num_sids = r->in.sids->num_sids;
	uint32 mapped_count = 0;
	struct lsa_RefDomainList *domains = NULL;
	struct lsa_TranslatedName2 *names = NULL;
	bool check_policy = true;

	switch (p->opnum) {
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

NTSTATUS _lsa_LookupSids3(struct pipes_struct *p,
			  struct lsa_LookupSids3 *r)
{
	struct lsa_LookupSids2 q;

	/* No policy handle on this call. Restrict to crypto connections. */
	if (p->auth.auth_type != DCERPC_AUTH_TYPE_SCHANNEL) {
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

NTSTATUS _lsa_LookupNames(struct pipes_struct *p,
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

NTSTATUS _lsa_LookupNames2(struct pipes_struct *p,
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

NTSTATUS _lsa_LookupNames3(struct pipes_struct *p,
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

	switch (p->opnum) {
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

NTSTATUS _lsa_LookupNames4(struct pipes_struct *p,
			   struct lsa_LookupNames4 *r)
{
	struct lsa_LookupNames3 q;

	/* No policy handle on this call. Restrict to crypto connections. */
	if (p->auth.auth_type != DCERPC_AUTH_TYPE_SCHANNEL) {
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

NTSTATUS _lsa_Close(struct pipes_struct *p, struct lsa_Close *r)
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

static NTSTATUS lsa_lookup_trusted_domain_by_sid(TALLOC_CTX *mem_ctx,
						 const struct dom_sid *sid,
						 struct trustdom_info **info)
{
	NTSTATUS status;
	uint32_t num_domains = 0;
	struct trustdom_info **domains = NULL;
	int i;

	status = pdb_enum_trusteddoms(mem_ctx, &num_domains, &domains);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	for (i=0; i < num_domains; i++) {
		if (dom_sid_equal(&domains[i]->sid, sid)) {
			break;
		}
	}

	if (i == num_domains) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	*info = domains[i];

	return NT_STATUS_OK;
}

/***************************************************************************
 ***************************************************************************/

static NTSTATUS lsa_lookup_trusted_domain_by_name(TALLOC_CTX *mem_ctx,
						  const char *netbios_domain_name,
						  struct trustdom_info **info_p)
{
	NTSTATUS status;
	struct trustdom_info *info;
	struct pdb_trusted_domain *td;

	status = pdb_get_trusted_domain(mem_ctx, netbios_domain_name, &td);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	info = talloc(mem_ctx, struct trustdom_info);
	if (!info) {
		return NT_STATUS_NO_MEMORY;
	}

	info->name	= talloc_strdup(info, netbios_domain_name);
	NT_STATUS_HAVE_NO_MEMORY(info->name);

	sid_copy(&info->sid, &td->security_identifier);

	*info_p = info;

	return NT_STATUS_OK;
}

/***************************************************************************
 ***************************************************************************/

NTSTATUS _lsa_OpenSecret(struct pipes_struct *p, struct lsa_OpenSecret *r)
{
	return NT_STATUS_OBJECT_NAME_NOT_FOUND;
}

/***************************************************************************
 _lsa_OpenTrustedDomain_base
 ***************************************************************************/

static NTSTATUS _lsa_OpenTrustedDomain_base(struct pipes_struct *p,
					    uint32_t access_mask,
					    struct trustdom_info *info,
					    struct policy_handle *handle)
{
	struct security_descriptor *psd = NULL;
	size_t sd_size;
	uint32_t acc_granted;
	NTSTATUS status;

	/* des_access is for the account here, not the policy
	 * handle - so don't check against policy handle. */

	/* Work out max allowed. */
	map_max_allowed_access(p->session_info->security_token,
			       &p->session_info->utok,
			       &access_mask);

	/* map the generic bits to the lsa account ones */
	se_map_generic(&access_mask, &lsa_account_mapping);

	/* get the generic lsa account SD until we store it */
	status = make_lsa_object_sd(p->mem_ctx, &psd, &sd_size,
				    &lsa_trusted_domain_mapping,
				    NULL, 0);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = access_check_object(psd, p->session_info->security_token,
				     SEC_PRIV_INVALID, SEC_PRIV_INVALID, 0,
				     access_mask, &acc_granted,
				     "_lsa_OpenTrustedDomain");
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = create_lsa_policy_handle(p->mem_ctx, p,
					  LSA_HANDLE_TRUST_TYPE,
					  acc_granted,
					  &info->sid,
					  info->name,
					  psd,
					  handle);
	if (!NT_STATUS_IS_OK(status)) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	return NT_STATUS_OK;
}

/***************************************************************************
 _lsa_OpenTrustedDomain
 ***************************************************************************/

NTSTATUS _lsa_OpenTrustedDomain(struct pipes_struct *p,
				struct lsa_OpenTrustedDomain *r)
{
	struct lsa_info *handle = NULL;
	struct trustdom_info *info = NULL;
	NTSTATUS status;

	if (!find_policy_by_hnd(p, r->in.handle, (void **)(void *)&handle)) {
		return NT_STATUS_INVALID_HANDLE;
	}

	if (handle->type != LSA_HANDLE_POLICY_TYPE) {
		return NT_STATUS_INVALID_HANDLE;
	}

	status = lsa_lookup_trusted_domain_by_sid(p->mem_ctx,
						  r->in.sid,
						  &info);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	return _lsa_OpenTrustedDomain_base(p, r->in.access_mask, info,
					   r->out.trustdom_handle);
}

/***************************************************************************
 _lsa_OpenTrustedDomainByName
 ***************************************************************************/

NTSTATUS _lsa_OpenTrustedDomainByName(struct pipes_struct *p,
				      struct lsa_OpenTrustedDomainByName *r)
{
	struct lsa_info *handle = NULL;
	struct trustdom_info *info = NULL;
	NTSTATUS status;

	if (!find_policy_by_hnd(p, r->in.handle, (void **)(void *)&handle)) {
		return NT_STATUS_INVALID_HANDLE;
	}

	if (handle->type != LSA_HANDLE_POLICY_TYPE) {
		return NT_STATUS_INVALID_HANDLE;
	}

	status = lsa_lookup_trusted_domain_by_name(p->mem_ctx,
						   r->in.name.string,
						   &info);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	return _lsa_OpenTrustedDomain_base(p, r->in.access_mask, info,
					   r->out.trustdom_handle);
}

static NTSTATUS add_trusted_domain_user(TALLOC_CTX *mem_ctx,
					const char *netbios_name,
					const char *domain_name,
					struct trustDomainPasswords auth_struct)
{
	NTSTATUS status;
	struct samu *sam_acct;
	char *acct_name;
	uint32_t rid;
	struct dom_sid user_sid;
	int i;
	char *dummy;
	size_t dummy_size;

	sam_acct = samu_new(mem_ctx);
	if (sam_acct == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	acct_name = talloc_asprintf(mem_ctx, "%s$", netbios_name);
	if (acct_name == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	if (!pdb_set_username(sam_acct, acct_name, PDB_SET)) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	if (!pdb_set_domain(sam_acct, domain_name, PDB_SET)) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	if (!pdb_set_acct_ctrl(sam_acct, ACB_DOMTRUST, PDB_SET)) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	if (!pdb_new_rid(&rid)) {
		return NT_STATUS_DS_NO_MORE_RIDS;
	}
	sid_compose(&user_sid, get_global_sam_sid(), rid);
	if (!pdb_set_user_sid(sam_acct, &user_sid, PDB_SET)) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	for (i = 0; i < auth_struct.incoming.count; i++) {
		switch (auth_struct.incoming.current.array[i].AuthType) {
			case TRUST_AUTH_TYPE_CLEAR:
				if (!convert_string_talloc(mem_ctx,
							   CH_UTF16LE,
							   CH_UNIX,
							   auth_struct.incoming.current.array[i].AuthInfo.clear.password,
							   auth_struct.incoming.current.array[i].AuthInfo.clear.size,
							   &dummy,
							   &dummy_size,
							   false)) {
					return NT_STATUS_UNSUCCESSFUL;
				}
				if (!pdb_set_plaintext_passwd(sam_acct, dummy)) {
					return NT_STATUS_UNSUCCESSFUL;
				}
				break;
			default:
				continue;
		}
	}

	status = pdb_add_sam_account(sam_acct);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	return NT_STATUS_OK;
}

/***************************************************************************
 _lsa_CreateTrustedDomainEx2
 ***************************************************************************/

NTSTATUS _lsa_CreateTrustedDomainEx2(struct pipes_struct *p,
				     struct lsa_CreateTrustedDomainEx2 *r)
{
	struct lsa_info *policy;
	NTSTATUS status;
	uint32_t acc_granted;
	struct security_descriptor *psd;
	size_t sd_size;
	struct pdb_trusted_domain td;
	struct trustDomainPasswords auth_struct;
	enum ndr_err_code ndr_err;
	DATA_BLOB auth_blob;

	if (!IS_DC) {
		return NT_STATUS_NOT_SUPPORTED;
	}

	if (!find_policy_by_hnd(p, r->in.policy_handle, (void **)(void *)&policy)) {
		return NT_STATUS_INVALID_HANDLE;
	}

	if (!(policy->access & LSA_POLICY_TRUST_ADMIN)) {
		return NT_STATUS_ACCESS_DENIED;
	}

	if (p->session_info->utok.uid != sec_initial_uid() &&
	    !nt_token_check_domain_rid(p->session_info->security_token, DOMAIN_RID_ADMINS)) {
		return NT_STATUS_ACCESS_DENIED;
	}

	/* Work out max allowed. */
	map_max_allowed_access(p->session_info->security_token,
			       &p->session_info->utok,
			       &r->in.access_mask);

	/* map the generic bits to the lsa policy ones */
	se_map_generic(&r->in.access_mask, &lsa_account_mapping);

	status = make_lsa_object_sd(p->mem_ctx, &psd, &sd_size,
				    &lsa_trusted_domain_mapping,
				    NULL, 0);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = access_check_object(psd, p->session_info->security_token,
				     SEC_PRIV_INVALID, SEC_PRIV_INVALID, 0,
				     r->in.access_mask, &acc_granted,
				     "_lsa_CreateTrustedDomainEx2");
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	ZERO_STRUCT(td);

	td.domain_name = talloc_strdup(p->mem_ctx,
				       r->in.info->domain_name.string);
	if (td.domain_name == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	td.netbios_name = talloc_strdup(p->mem_ctx,
					r->in.info->netbios_name.string);
	if (td.netbios_name == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	sid_copy(&td.security_identifier, r->in.info->sid);
	td.trust_direction = r->in.info->trust_direction;
	td.trust_type = r->in.info->trust_type;
	td.trust_attributes = r->in.info->trust_attributes;

	if (r->in.auth_info->auth_blob.size != 0) {
		auth_blob.length = r->in.auth_info->auth_blob.size;
		auth_blob.data = r->in.auth_info->auth_blob.data;

		arcfour_crypt_blob(auth_blob.data, auth_blob.length,
				   &p->session_info->user_session_key);

		ndr_err = ndr_pull_struct_blob(&auth_blob, p->mem_ctx,
					       &auth_struct,
					       (ndr_pull_flags_fn_t) ndr_pull_trustDomainPasswords);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			return NT_STATUS_UNSUCCESSFUL;
		}

		ndr_err = ndr_push_struct_blob(&td.trust_auth_incoming, p->mem_ctx,
					       &auth_struct.incoming,
					       (ndr_push_flags_fn_t) ndr_push_trustAuthInOutBlob);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			return NT_STATUS_UNSUCCESSFUL;
		}

		ndr_err = ndr_push_struct_blob(&td.trust_auth_outgoing, p->mem_ctx,
					       &auth_struct.outgoing,
					       (ndr_push_flags_fn_t) ndr_push_trustAuthInOutBlob);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			return NT_STATUS_UNSUCCESSFUL;
		}
	} else {
		td.trust_auth_incoming.data = NULL;
		td.trust_auth_incoming.length = 0;
		td.trust_auth_outgoing.data = NULL;
		td.trust_auth_outgoing.length = 0;
	}

	status = pdb_set_trusted_domain(r->in.info->domain_name.string, &td);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (r->in.info->trust_direction & LSA_TRUST_DIRECTION_INBOUND) {
		status = add_trusted_domain_user(p->mem_ctx,
						 r->in.info->netbios_name.string,
						 r->in.info->domain_name.string,
						 auth_struct);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
	}

	status = create_lsa_policy_handle(p->mem_ctx, p,
					  LSA_HANDLE_TRUST_TYPE,
					  acc_granted,
					  r->in.info->sid,
					  r->in.info->netbios_name.string,
					  psd,
					  r->out.trustdom_handle);
	if (!NT_STATUS_IS_OK(status)) {
		pdb_del_trusteddom_pw(r->in.info->netbios_name.string);
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	return NT_STATUS_OK;
}

/***************************************************************************
 _lsa_CreateTrustedDomainEx
 ***************************************************************************/

NTSTATUS _lsa_CreateTrustedDomainEx(struct pipes_struct *p,
				    struct lsa_CreateTrustedDomainEx *r)
{
	struct lsa_CreateTrustedDomainEx2 q;

	q.in.policy_handle	= r->in.policy_handle;
	q.in.info		= r->in.info;
	q.in.auth_info		= r->in.auth_info;
	q.in.access_mask	= r->in.access_mask;
	q.out.trustdom_handle	= r->out.trustdom_handle;

	return _lsa_CreateTrustedDomainEx2(p, &q);
}

/***************************************************************************
 _lsa_CreateTrustedDomain
 ***************************************************************************/

NTSTATUS _lsa_CreateTrustedDomain(struct pipes_struct *p,
				  struct lsa_CreateTrustedDomain *r)
{
	struct lsa_CreateTrustedDomainEx2 c;
	struct lsa_TrustDomainInfoInfoEx info;
	struct lsa_TrustDomainInfoAuthInfoInternal auth_info;

	ZERO_STRUCT(auth_info);

	info.domain_name	= r->in.info->name;
	info.netbios_name	= r->in.info->name;
	info.sid		= r->in.info->sid;
	info.trust_direction	= LSA_TRUST_DIRECTION_OUTBOUND;
	info.trust_type		= LSA_TRUST_TYPE_DOWNLEVEL;
	info.trust_attributes	= 0;

	c.in.policy_handle	= r->in.policy_handle;
	c.in.info		= &info;
	c.in.auth_info		= &auth_info;
	c.in.access_mask	= r->in.access_mask;
	c.out.trustdom_handle	= r->out.trustdom_handle;

	return _lsa_CreateTrustedDomainEx2(p, &c);
}

/***************************************************************************
 _lsa_DeleteTrustedDomain
 ***************************************************************************/

NTSTATUS _lsa_DeleteTrustedDomain(struct pipes_struct *p,
				  struct lsa_DeleteTrustedDomain *r)
{
	NTSTATUS status;
	struct lsa_info *handle;
	struct pdb_trusted_domain *td;
	struct samu *sam_acct;
	char *acct_name;

	/* find the connection policy handle. */
	if (!find_policy_by_hnd(p, r->in.handle, (void **)(void *)&handle)) {
		return NT_STATUS_INVALID_HANDLE;
	}

	if (handle->type != LSA_HANDLE_POLICY_TYPE) {
		return NT_STATUS_INVALID_HANDLE;
	}

	if (!(handle->access & LSA_POLICY_TRUST_ADMIN)) {
		return NT_STATUS_ACCESS_DENIED;
	}

	status = pdb_get_trusted_domain_by_sid(p->mem_ctx, r->in.dom_sid, &td);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (td->netbios_name == NULL || *td->netbios_name == '\0') {
		DEBUG(10, ("Missing netbios name for for trusted domain %s.\n",
			   sid_string_tos(r->in.dom_sid)));
		return NT_STATUS_UNSUCCESSFUL;
	}

	if (td->trust_direction & LSA_TRUST_DIRECTION_INBOUND) {
		sam_acct = samu_new(p->mem_ctx);
		if (sam_acct == NULL) {
			return NT_STATUS_NO_MEMORY;
		}

		acct_name = talloc_asprintf(p->mem_ctx, "%s$", td->netbios_name);
		if (acct_name == NULL) {
			return NT_STATUS_NO_MEMORY;
		}
		if (!pdb_set_username(sam_acct, acct_name, PDB_SET)) {
			return NT_STATUS_UNSUCCESSFUL;
		}
		status = pdb_delete_sam_account(sam_acct);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
	}

	status = pdb_del_trusted_domain(td->netbios_name);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	return NT_STATUS_OK;
}

/***************************************************************************
 _lsa_CloseTrustedDomainEx
 ***************************************************************************/

NTSTATUS _lsa_CloseTrustedDomainEx(struct pipes_struct *p,
				   struct lsa_CloseTrustedDomainEx *r)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

/***************************************************************************
 _lsa_QueryTrustedDomainInfo
 ***************************************************************************/

NTSTATUS _lsa_QueryTrustedDomainInfo(struct pipes_struct *p,
				     struct lsa_QueryTrustedDomainInfo *r)
{
	NTSTATUS status;
	struct lsa_info *handle;
	union lsa_TrustedDomainInfo *info;
	struct pdb_trusted_domain *td;
	uint32_t acc_required;

	/* find the connection policy handle. */
	if (!find_policy_by_hnd(p, r->in.trustdom_handle, (void **)(void *)&handle)) {
		return NT_STATUS_INVALID_HANDLE;
	}

	if (handle->type != LSA_HANDLE_TRUST_TYPE) {
		return NT_STATUS_INVALID_HANDLE;
	}

	switch (r->in.level) {
	case LSA_TRUSTED_DOMAIN_INFO_NAME:
		acc_required = LSA_TRUSTED_QUERY_DOMAIN_NAME;
		break;
	case LSA_TRUSTED_DOMAIN_INFO_CONTROLLERS:
		acc_required = LSA_TRUSTED_QUERY_CONTROLLERS;
		break;
	case LSA_TRUSTED_DOMAIN_INFO_POSIX_OFFSET:
		acc_required = LSA_TRUSTED_QUERY_POSIX;
		break;
	case LSA_TRUSTED_DOMAIN_INFO_PASSWORD:
		acc_required = LSA_TRUSTED_QUERY_AUTH;
		break;
	case LSA_TRUSTED_DOMAIN_INFO_BASIC:
		acc_required = LSA_TRUSTED_QUERY_DOMAIN_NAME;
		break;
	case LSA_TRUSTED_DOMAIN_INFO_INFO_EX:
		acc_required = LSA_TRUSTED_QUERY_DOMAIN_NAME;
		break;
	case LSA_TRUSTED_DOMAIN_INFO_AUTH_INFO:
		acc_required = LSA_TRUSTED_QUERY_AUTH;
		break;
	case LSA_TRUSTED_DOMAIN_INFO_FULL_INFO:
		acc_required = LSA_TRUSTED_QUERY_DOMAIN_NAME |
			       LSA_TRUSTED_QUERY_POSIX |
			       LSA_TRUSTED_QUERY_AUTH;
		break;
	case LSA_TRUSTED_DOMAIN_INFO_AUTH_INFO_INTERNAL:
		acc_required = LSA_TRUSTED_QUERY_AUTH;
		break;
	case LSA_TRUSTED_DOMAIN_INFO_FULL_INFO_INTERNAL:
		acc_required = LSA_TRUSTED_QUERY_DOMAIN_NAME |
			       LSA_TRUSTED_QUERY_POSIX |
			       LSA_TRUSTED_QUERY_AUTH;
		break;
	case LSA_TRUSTED_DOMAIN_INFO_INFO_EX2_INTERNAL:
		acc_required = LSA_TRUSTED_QUERY_DOMAIN_NAME;
		break;
	case LSA_TRUSTED_DOMAIN_INFO_FULL_INFO_2_INTERNAL:
		acc_required = LSA_TRUSTED_QUERY_DOMAIN_NAME |
			       LSA_TRUSTED_QUERY_POSIX |
			       LSA_TRUSTED_QUERY_AUTH;
		break;
	case LSA_TRUSTED_DOMAIN_SUPPORTED_ENCRYPTION_TYPES:
		acc_required = LSA_TRUSTED_QUERY_POSIX;
		break;
	default:
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (!(handle->access & acc_required)) {
		return NT_STATUS_ACCESS_DENIED;
	}

	status = pdb_get_trusted_domain_by_sid(p->mem_ctx, &handle->sid, &td);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	info = TALLOC_ZERO_P(p->mem_ctx, union lsa_TrustedDomainInfo);
	if (!info) {
		return NT_STATUS_NO_MEMORY;
	}

	switch (r->in.level) {
	case LSA_TRUSTED_DOMAIN_INFO_NAME:
		init_lsa_StringLarge(&info->name.netbios_name, td->netbios_name);
		break;
	case LSA_TRUSTED_DOMAIN_INFO_CONTROLLERS:
		return NT_STATUS_INVALID_PARAMETER;
	case LSA_TRUSTED_DOMAIN_INFO_POSIX_OFFSET:
		break;
	case LSA_TRUSTED_DOMAIN_INFO_PASSWORD:
		return NT_STATUS_INVALID_INFO_CLASS;
	case LSA_TRUSTED_DOMAIN_INFO_BASIC:
		return NT_STATUS_INVALID_PARAMETER;
	case LSA_TRUSTED_DOMAIN_INFO_INFO_EX:
		init_lsa_StringLarge(&info->info_ex.domain_name, td->domain_name);
		init_lsa_StringLarge(&info->info_ex.netbios_name, td->netbios_name);
		info->info_ex.sid = dom_sid_dup(info, &td->security_identifier);
		if (!info->info_ex.sid) {
			return NT_STATUS_NO_MEMORY;
		}
		info->info_ex.trust_direction = td->trust_direction;
		info->info_ex.trust_type = td->trust_type;
		info->info_ex.trust_attributes = td->trust_attributes;
		break;
	case LSA_TRUSTED_DOMAIN_INFO_AUTH_INFO:
		return NT_STATUS_INVALID_INFO_CLASS;
	case LSA_TRUSTED_DOMAIN_INFO_FULL_INFO:
		break;
	case LSA_TRUSTED_DOMAIN_INFO_AUTH_INFO_INTERNAL:
		return NT_STATUS_INVALID_INFO_CLASS;
	case LSA_TRUSTED_DOMAIN_INFO_FULL_INFO_INTERNAL:
		return NT_STATUS_INVALID_INFO_CLASS;
	case LSA_TRUSTED_DOMAIN_INFO_INFO_EX2_INTERNAL:
		return NT_STATUS_INVALID_PARAMETER;
	case LSA_TRUSTED_DOMAIN_INFO_FULL_INFO_2_INTERNAL:
		break;
	case LSA_TRUSTED_DOMAIN_SUPPORTED_ENCRYPTION_TYPES:
		break;
	default:
		return NT_STATUS_INVALID_PARAMETER;
	}

	*r->out.info = info;

	return NT_STATUS_OK;
}

/***************************************************************************
 _lsa_QueryTrustedDomainInfoBySid
 ***************************************************************************/

NTSTATUS _lsa_QueryTrustedDomainInfoBySid(struct pipes_struct *p,
					  struct lsa_QueryTrustedDomainInfoBySid *r)
{
	NTSTATUS status;
	struct policy_handle trustdom_handle;
	struct lsa_OpenTrustedDomain o;
	struct lsa_QueryTrustedDomainInfo q;
	struct lsa_Close c;

	o.in.handle		= r->in.handle;
	o.in.sid		= r->in.dom_sid;
	o.in.access_mask	= SEC_FLAG_MAXIMUM_ALLOWED;
	o.out.trustdom_handle	= &trustdom_handle;

	status = _lsa_OpenTrustedDomain(p, &o);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	q.in.trustdom_handle	= &trustdom_handle;
	q.in.level		= r->in.level;
	q.out.info		= r->out.info;

	status = _lsa_QueryTrustedDomainInfo(p, &q);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	c.in.handle		= &trustdom_handle;
	c.out.handle		= &trustdom_handle;

	return _lsa_Close(p, &c);
}

/***************************************************************************
 _lsa_QueryTrustedDomainInfoByName
 ***************************************************************************/

NTSTATUS _lsa_QueryTrustedDomainInfoByName(struct pipes_struct *p,
					   struct lsa_QueryTrustedDomainInfoByName *r)
{
	NTSTATUS status;
	struct policy_handle trustdom_handle;
	struct lsa_OpenTrustedDomainByName o;
	struct lsa_QueryTrustedDomainInfo q;
	struct lsa_Close c;

	o.in.handle		= r->in.handle;
	o.in.name.string	= r->in.trusted_domain->string;
	o.in.access_mask	= SEC_FLAG_MAXIMUM_ALLOWED;
	o.out.trustdom_handle	= &trustdom_handle;

	status = _lsa_OpenTrustedDomainByName(p, &o);
	if (!NT_STATUS_IS_OK(status)) {
		if (NT_STATUS_EQUAL(status, NT_STATUS_NO_SUCH_DOMAIN)) {
			return NT_STATUS_OBJECT_NAME_NOT_FOUND;
		}
		return status;
	}

	q.in.trustdom_handle	= &trustdom_handle;
	q.in.level		= r->in.level;
	q.out.info		= r->out.info;

	status = _lsa_QueryTrustedDomainInfo(p, &q);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	c.in.handle		= &trustdom_handle;
	c.out.handle		= &trustdom_handle;

	return _lsa_Close(p, &c);
}

/***************************************************************************
 ***************************************************************************/

NTSTATUS _lsa_CreateSecret(struct pipes_struct *p, struct lsa_CreateSecret *r)
{
	return NT_STATUS_ACCESS_DENIED;
}

/***************************************************************************
 ***************************************************************************/

NTSTATUS _lsa_SetSecret(struct pipes_struct *p, struct lsa_SetSecret *r)
{
	return NT_STATUS_ACCESS_DENIED;
}

/***************************************************************************
 _lsa_DeleteObject
 ***************************************************************************/

NTSTATUS _lsa_DeleteObject(struct pipes_struct *p,
			   struct lsa_DeleteObject *r)
{
	NTSTATUS status;
	struct lsa_info *info = NULL;

	if (!find_policy_by_hnd(p, r->in.handle, (void **)(void *)&info)) {
		return NT_STATUS_INVALID_HANDLE;
	}

	if (!(info->access & SEC_STD_DELETE)) {
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

NTSTATUS _lsa_EnumPrivs(struct pipes_struct *p,
			struct lsa_EnumPrivs *r)
{
	struct lsa_info *handle;
	uint32 i;
	uint32 enum_context = *r->in.resume_handle;
	int num_privs = num_privileges_in_short_list();
	struct lsa_PrivEntry *entries = NULL;

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

			init_lsa_StringLarge(&entries[i].name, sec_privilege_name_from_index(i));

			entries[i].luid.low = sec_privilege_from_index(i);
			entries[i].luid.high = 0;
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

NTSTATUS _lsa_LookupPrivDisplayName(struct pipes_struct *p,
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

NTSTATUS _lsa_EnumAccounts(struct pipes_struct *p,
			   struct lsa_EnumAccounts *r)
{
	struct lsa_info *handle;
	struct dom_sid *sid_list;
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
			sids[j].sid = dom_sid_dup(p->mem_ctx, &sid_list[i]);
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

NTSTATUS _lsa_GetUserName(struct pipes_struct *p,
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

	if (p->session_info->guest) {
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
		username = p->session_info->sanitized_username;
		domname = p->session_info->info3->base.domain.string;
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

NTSTATUS _lsa_CreateAccount(struct pipes_struct *p,
			    struct lsa_CreateAccount *r)
{
	NTSTATUS status;
	struct lsa_info *handle;
	uint32_t acc_granted;
	struct security_descriptor *psd;
	size_t sd_size;
	uint32_t owner_access = (LSA_ACCOUNT_ALL_ACCESS &
			~(LSA_ACCOUNT_ADJUST_PRIVILEGES|
			LSA_ACCOUNT_ADJUST_SYSTEM_ACCESS|
			SEC_STD_DELETE));

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
	map_max_allowed_access(p->session_info->security_token,
			       &p->session_info->utok,
			       &r->in.access_mask);

	/* map the generic bits to the lsa policy ones */
	se_map_generic(&r->in.access_mask, &lsa_account_mapping);

	status = make_lsa_object_sd(p->mem_ctx, &psd, &sd_size,
				    &lsa_account_mapping,
				    r->in.sid, owner_access);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = access_check_object(psd, p->session_info->security_token,
				     SEC_PRIV_INVALID, SEC_PRIV_INVALID, 0, r->in.access_mask,
				     &acc_granted, "_lsa_CreateAccount");
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if ( is_privileged_sid( r->in.sid ) )
		return NT_STATUS_OBJECT_NAME_COLLISION;

	status = create_lsa_policy_handle(p->mem_ctx, p,
					  LSA_HANDLE_ACCOUNT_TYPE,
					  acc_granted,
					  r->in.sid,
					  NULL,
					  psd,
					  r->out.acct_handle);
	if (!NT_STATUS_IS_OK(status)) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	return privilege_create_account(r->in.sid);
}

/***************************************************************************
 _lsa_OpenAccount
 ***************************************************************************/

NTSTATUS _lsa_OpenAccount(struct pipes_struct *p,
			  struct lsa_OpenAccount *r)
{
	struct lsa_info *handle;
	struct security_descriptor *psd = NULL;
	size_t sd_size;
	uint32_t des_access = r->in.access_mask;
	uint32_t acc_granted;
	uint32_t owner_access = (LSA_ACCOUNT_ALL_ACCESS &
			~(LSA_ACCOUNT_ADJUST_PRIVILEGES|
			LSA_ACCOUNT_ADJUST_SYSTEM_ACCESS|
			SEC_STD_DELETE));
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
	map_max_allowed_access(p->session_info->security_token,
			       &p->session_info->utok,
			       &des_access);

	/* map the generic bits to the lsa account ones */
	se_map_generic(&des_access, &lsa_account_mapping);

	/* get the generic lsa account SD until we store it */
	status = make_lsa_object_sd(p->mem_ctx, &psd, &sd_size,
				&lsa_account_mapping,
				r->in.sid, owner_access);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = access_check_object(psd, p->session_info->security_token,
				     SEC_PRIV_INVALID, SEC_PRIV_INVALID, 0, des_access,
				     &acc_granted, "_lsa_OpenAccount" );
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	/* TODO: Fis the parsing routine before reenabling this check! */
	#if 0
	if (!lookup_sid(&handle->sid, dom_name, name, &type))
		return NT_STATUS_ACCESS_DENIED;
	#endif

	status = create_lsa_policy_handle(p->mem_ctx, p,
					  LSA_HANDLE_ACCOUNT_TYPE,
					  acc_granted,
					  r->in.sid,
					  NULL,
					  psd,
					  r->out.acct_handle);
	if (!NT_STATUS_IS_OK(status)) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	return NT_STATUS_OK;
}

/***************************************************************************
 _lsa_EnumPrivsAccount
 For a given SID, enumerate all the privilege this account has.
 ***************************************************************************/

NTSTATUS _lsa_EnumPrivsAccount(struct pipes_struct *p,
			       struct lsa_EnumPrivsAccount *r)
{
	NTSTATUS status = NT_STATUS_OK;
	struct lsa_info *info=NULL;
	PRIVILEGE_SET *privileges;
	struct lsa_PrivilegeSet *priv_set = NULL;

	/* find the connection policy handle. */
	if (!find_policy_by_hnd(p, r->in.handle, (void **)(void *)&info))
		return NT_STATUS_INVALID_HANDLE;

	if (info->type != LSA_HANDLE_ACCOUNT_TYPE) {
		return NT_STATUS_INVALID_HANDLE;
	}

	if (!(info->access & LSA_ACCOUNT_VIEW))
		return NT_STATUS_ACCESS_DENIED;

	status = get_privileges_for_sid_as_set(p->mem_ctx, &privileges, &info->sid);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	*r->out.privs = priv_set = TALLOC_ZERO_P(p->mem_ctx, struct lsa_PrivilegeSet);
	if (!priv_set) {
		return NT_STATUS_NO_MEMORY;
	}

	DEBUG(10,("_lsa_EnumPrivsAccount: %s has %d privileges\n",
		  sid_string_dbg(&info->sid),
		  privileges->count));

	priv_set->count = privileges->count;
	priv_set->unknown = 0;
	priv_set->set = talloc_move(priv_set, &privileges->set);

	return status;
}

/***************************************************************************
 _lsa_GetSystemAccessAccount
 ***************************************************************************/

NTSTATUS _lsa_GetSystemAccessAccount(struct pipes_struct *p,
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

NTSTATUS _lsa_SetSystemAccessAccount(struct pipes_struct *p,
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

NTSTATUS _lsa_AddPrivilegesToAccount(struct pipes_struct *p,
				     struct lsa_AddPrivilegesToAccount *r)
{
	struct lsa_info *info = NULL;
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

	if ( !grant_privilege_set( &info->sid, set ) ) {
		DEBUG(3,("_lsa_AddPrivilegesToAccount: grant_privilege_set(%s) failed!\n",
			 sid_string_dbg(&info->sid) ));
		return NT_STATUS_NO_SUCH_PRIVILEGE;
	}

	return NT_STATUS_OK;
}

/***************************************************************************
 _lsa_RemovePrivilegesFromAccount
 For a given SID, remove some privileges.
 ***************************************************************************/

NTSTATUS _lsa_RemovePrivilegesFromAccount(struct pipes_struct *p,
					  struct lsa_RemovePrivilegesFromAccount *r)
{
	struct lsa_info *info = NULL;
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

	if ( !revoke_privilege_set( &info->sid, set) ) {
		DEBUG(3,("_lsa_RemovePrivilegesFromAccount: revoke_privilege(%s) failed!\n",
			 sid_string_dbg(&info->sid) ));
		return NT_STATUS_NO_SUCH_PRIVILEGE;
	}

	return NT_STATUS_OK;
}

/***************************************************************************
 _lsa_LookupPrivName
 ***************************************************************************/

NTSTATUS _lsa_LookupPrivName(struct pipes_struct *p,
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

	if (r->in.luid->high != 0) {
		return NT_STATUS_NO_SUCH_PRIVILEGE;
	}

	name = sec_privilege_name(r->in.luid->low);
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

NTSTATUS _lsa_QuerySecurity(struct pipes_struct *p,
			    struct lsa_QuerySecurity *r)
{
	struct lsa_info *handle=NULL;
	struct security_descriptor *psd = NULL;
	size_t sd_size = 0;
	NTSTATUS status;

	/* find the connection policy handle. */
	if (!find_policy_by_hnd(p, r->in.handle, (void **)(void *)&handle))
		return NT_STATUS_INVALID_HANDLE;

	switch (handle->type) {
	case LSA_HANDLE_POLICY_TYPE:
	case LSA_HANDLE_ACCOUNT_TYPE:
	case LSA_HANDLE_TRUST_TYPE:
		psd = handle->sd;
		sd_size = ndr_size_security_descriptor(psd, 0);
		status = NT_STATUS_OK;
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

NTSTATUS _lsa_AddAccountRights(struct pipes_struct *p,
			       struct lsa_AddAccountRights *r)
{
	struct lsa_info *info = NULL;
	int i = 0;
	uint32_t acc_granted = 0;
	struct security_descriptor *psd = NULL;
	size_t sd_size;
	struct dom_sid sid;
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
				NULL, 0);
        if (!NT_STATUS_IS_OK(status)) {
                return status;
        }

	/*
	 * From the MS DOCs. If the sid doesn't exist, ask for LSA_POLICY_CREATE_ACCOUNT
	 * on the policy handle. If it does, ask for
	 * LSA_ACCOUNT_ADJUST_PRIVILEGES|LSA_ACCOUNT_ADJUST_SYSTEM_ACCESS|LSA_ACCOUNT_VIEW,
	 * on the account sid. We don't check here so just use the latter. JRA.
	 */

	status = access_check_object(psd, p->session_info->security_token,
				     SEC_PRIV_INVALID, SEC_PRIV_INVALID, 0,
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

NTSTATUS _lsa_RemoveAccountRights(struct pipes_struct *p,
				  struct lsa_RemoveAccountRights *r)
{
	struct lsa_info *info = NULL;
	int i = 0;
	struct security_descriptor *psd = NULL;
	size_t sd_size;
	struct dom_sid sid;
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
				NULL, 0);
        if (!NT_STATUS_IS_OK(status)) {
                return status;
        }

	/*
	 * From the MS DOCs. We need
	 * LSA_ACCOUNT_ADJUST_PRIVILEGES|LSA_ACCOUNT_ADJUST_SYSTEM_ACCESS|LSA_ACCOUNT_VIEW
	 * and DELETE on the account sid.
	 */

	status = access_check_object(psd, p->session_info->security_token,
				     SEC_PRIV_INVALID, SEC_PRIV_INVALID, 0,
				     LSA_ACCOUNT_ADJUST_PRIVILEGES|LSA_ACCOUNT_ADJUST_SYSTEM_ACCESS|
				     LSA_ACCOUNT_VIEW|SEC_STD_DELETE,
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
		if (privileges->set[i].luid.high) {
			continue;
		}
		privname = sec_privilege_name(privileges->set[i].luid.low);
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

NTSTATUS _lsa_EnumAccountRights(struct pipes_struct *p,
				struct lsa_EnumAccountRights *r)
{
	NTSTATUS status;
	struct lsa_info *info = NULL;
	PRIVILEGE_SET *privileges;

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

	/* according to MS-LSAD 3.1.4.5.10 it is required to return
	 * NT_STATUS_OBJECT_NAME_NOT_FOUND if the account sid was not found in
	 * the lsa database */

	status = get_privileges_for_sid_as_set(p->mem_ctx, &privileges, r->in.sid);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	DEBUG(10,("_lsa_EnumAccountRights: %s has %d privileges\n",
		  sid_string_dbg(r->in.sid), privileges->count));

	status = init_lsa_right_set(p->mem_ctx, r->out.rights, privileges);

	return status;
}

/***************************************************************************
 _lsa_LookupPrivValue
 ***************************************************************************/

NTSTATUS _lsa_LookupPrivValue(struct pipes_struct *p,
			      struct lsa_LookupPrivValue *r)
{
	struct lsa_info *info = NULL;
	const char *name = NULL;

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

	r->out.luid->low = sec_privilege_id(name);
	r->out.luid->high = 0;
	if (r->out.luid->low == SEC_PRIV_INVALID) {
		return NT_STATUS_NO_SUCH_PRIVILEGE;
	}
	return NT_STATUS_OK;
}

/***************************************************************************
 _lsa_EnumAccountsWithUserRight
 ***************************************************************************/

NTSTATUS _lsa_EnumAccountsWithUserRight(struct pipes_struct *p,
					struct lsa_EnumAccountsWithUserRight *r)
{
	NTSTATUS status;
	struct lsa_info *info = NULL;
	struct dom_sid *sids = NULL;
	int num_sids = 0;
	uint32_t i;
	enum sec_privilege privilege;

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

	privilege = sec_privilege_id(r->in.name->string);
	if (privilege == SEC_PRIV_INVALID) {
		return NT_STATUS_NO_SUCH_PRIVILEGE;
	}

	status = privilege_enum_sids(privilege, p->mem_ctx,
				     &sids, &num_sids);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	r->out.sids->num_sids = num_sids;
	r->out.sids->sids = talloc_array(p->mem_ctx, struct lsa_SidPtr,
					 r->out.sids->num_sids);

	for (i=0; i < r->out.sids->num_sids; i++) {
		r->out.sids->sids[i].sid = dom_sid_dup(r->out.sids->sids,
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

NTSTATUS _lsa_Delete(struct pipes_struct *p,
		     struct lsa_Delete *r)
{
	return NT_STATUS_NOT_SUPPORTED;
}

/*
 * From here on the server routines are just dummy ones to make smbd link with
 * librpc/gen_ndr/srv_lsa.c. These routines are actually never called, we are
 * pulling the server stubs across one by one.
 */

NTSTATUS _lsa_SetSecObj(struct pipes_struct *p, struct lsa_SetSecObj *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_ChangePassword(struct pipes_struct *p,
			     struct lsa_ChangePassword *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_SetInfoPolicy(struct pipes_struct *p, struct lsa_SetInfoPolicy *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_ClearAuditLog(struct pipes_struct *p, struct lsa_ClearAuditLog *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_GetQuotasForAccount(struct pipes_struct *p,
				  struct lsa_GetQuotasForAccount *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_SetQuotasForAccount(struct pipes_struct *p,
				  struct lsa_SetQuotasForAccount *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_SetInformationTrustedDomain(struct pipes_struct *p,
					  struct lsa_SetInformationTrustedDomain *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_QuerySecret(struct pipes_struct *p, struct lsa_QuerySecret *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_SetTrustedDomainInfo(struct pipes_struct *p,
				   struct lsa_SetTrustedDomainInfo *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_StorePrivateData(struct pipes_struct *p,
			       struct lsa_StorePrivateData *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_RetrievePrivateData(struct pipes_struct *p,
				  struct lsa_RetrievePrivateData *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_SetInfoPolicy2(struct pipes_struct *p,
			     struct lsa_SetInfoPolicy2 *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_SetTrustedDomainInfoByName(struct pipes_struct *p,
					 struct lsa_SetTrustedDomainInfoByName *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_EnumTrustedDomainsEx(struct pipes_struct *p,
				   struct lsa_EnumTrustedDomainsEx *r)
{
	struct lsa_info *info;
	uint32_t count;
	struct pdb_trusted_domain **domains;
	struct lsa_TrustDomainInfoInfoEx *entries;
	int i;
	NTSTATUS nt_status;

	/* bail out early if pdb backend is not capable of ex trusted domains,
	 * if we dont do that, the client might not call
	 * _lsa_EnumTrustedDomains() afterwards - gd */

	if (!(pdb_capabilities() & PDB_CAP_TRUSTED_DOMAINS_EX)) {
		p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
		return NT_STATUS_NOT_IMPLEMENTED;
	}

	if (!find_policy_by_hnd(p, r->in.handle, (void **)(void *)&info))
		return NT_STATUS_INVALID_HANDLE;

	if (info->type != LSA_HANDLE_POLICY_TYPE) {
		return NT_STATUS_INVALID_HANDLE;
	}

	/* check if the user has enough rights */
	if (!(info->access & LSA_POLICY_VIEW_LOCAL_INFORMATION))
		return NT_STATUS_ACCESS_DENIED;

	become_root();
	nt_status = pdb_enum_trusted_domains(p->mem_ctx, &count, &domains);
	unbecome_root();

	if (!NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	}

	entries = TALLOC_ZERO_ARRAY(p->mem_ctx, struct lsa_TrustDomainInfoInfoEx,
				    count);
	if (!entries) {
		return NT_STATUS_NO_MEMORY;
	}

	for (i=0; i<count; i++) {
		init_lsa_StringLarge(&entries[i].netbios_name,
				     domains[i]->netbios_name);
		entries[i].sid = &domains[i]->security_identifier;
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
				    (r->in.max_size/LSA_ENUM_TRUST_DOMAIN_EX_MULTIPLIER));

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

NTSTATUS _lsa_QueryDomainInformationPolicy(struct pipes_struct *p,
					   struct lsa_QueryDomainInformationPolicy *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_SetDomainInformationPolicy(struct pipes_struct *p,
					 struct lsa_SetDomainInformationPolicy *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_TestCall(struct pipes_struct *p, struct lsa_TestCall *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_CREDRWRITE(struct pipes_struct *p, struct lsa_CREDRWRITE *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_CREDRREAD(struct pipes_struct *p, struct lsa_CREDRREAD *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_CREDRENUMERATE(struct pipes_struct *p, struct lsa_CREDRENUMERATE *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_CREDRWRITEDOMAINCREDENTIALS(struct pipes_struct *p,
					  struct lsa_CREDRWRITEDOMAINCREDENTIALS *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_CREDRREADDOMAINCREDENTIALS(struct pipes_struct *p,
					 struct lsa_CREDRREADDOMAINCREDENTIALS *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_CREDRDELETE(struct pipes_struct *p, struct lsa_CREDRDELETE *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_CREDRGETTARGETINFO(struct pipes_struct *p,
				 struct lsa_CREDRGETTARGETINFO *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_CREDRPROFILELOADED(struct pipes_struct *p,
				 struct lsa_CREDRPROFILELOADED *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_CREDRGETSESSIONTYPES(struct pipes_struct *p,
				   struct lsa_CREDRGETSESSIONTYPES *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_LSARREGISTERAUDITEVENT(struct pipes_struct *p,
				     struct lsa_LSARREGISTERAUDITEVENT *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_LSARGENAUDITEVENT(struct pipes_struct *p,
				struct lsa_LSARGENAUDITEVENT *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_LSARUNREGISTERAUDITEVENT(struct pipes_struct *p,
				       struct lsa_LSARUNREGISTERAUDITEVENT *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_lsaRQueryForestTrustInformation(struct pipes_struct *p,
					      struct lsa_lsaRQueryForestTrustInformation *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
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
		if (StrCaseCmp(s1, s2) == 0) {
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

	if (StrCaseCmp(&p1[t1 - t2], p2) == 0) {
		return cret;
	}

	return DNS_CMP_NO_MATCH;
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
			      uint32_t index, uint32_t collision_type,
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
	const char *nb_name = NULL;
	struct dom_sid *sid = NULL;
	const char *tname = NULL;
	size_t dns_len = 0;
	size_t nb_len;
	size_t tlen = 0;
	NTSTATUS nt_status;
	uint32_t new_fti_idx;
	uint32_t i;
	/* use always TDO type, until we understand when Xref can be used */
	uint32_t collision_type = LSA_FOREST_TRUST_COLLISION_TDO;
	bool tln_conflict;
	bool sid_conflict;
	bool nb_conflict;
	bool exclusion;
	bool ex_rule = false;
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
				break;
			default:
				return NT_STATUS_INVALID_PARAMETER;
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
			    StrCaseCmp(trec->data.info.netbios_name.string,
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

static NTSTATUS get_ft_info(TALLOC_CTX *mem_ctx,
			    struct pdb_trusted_domain *td,
			    struct ForestTrustInfo *info)
{
	enum ndr_err_code ndr_err;

	if (td->trust_forest_trust_info.length == 0 ||
	    td->trust_forest_trust_info.data == NULL) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}
	ndr_err = ndr_pull_struct_blob_all(&td->trust_forest_trust_info, mem_ctx,
					   info,
					   (ndr_pull_flags_fn_t)ndr_pull_ForestTrustInfo);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return NT_STATUS_INVALID_DOMAIN_STATE;
	}

	return NT_STATUS_OK;
}

static NTSTATUS own_ft_info(struct pdb_domain_info *dom_info,
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

	rec->data.name.string = talloc_strdup(fti, dom_info->dns_forest);
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

	info->sid = dom_info->sid;
	info->dns_name.string = talloc_strdup(fti, dom_info->dns_domain);
	if (!info->dns_name.string) {
		return NT_STATUS_NO_MEMORY;
	}
	info->dns_name.size = strlen(info->dns_name.string);
	info->netbios_name.string = talloc_strdup(fti, dom_info->name);
	if (!info->netbios_name.string) {
		return NT_STATUS_NO_MEMORY;
	}
	info->netbios_name.size = strlen(info->netbios_name.string);

	return NT_STATUS_OK;
}

NTSTATUS _lsa_lsaRSetForestTrustInformation(struct pipes_struct *p,
					    struct lsa_lsaRSetForestTrustInformation *r)
{
	NTSTATUS status;
	int i;
	int j;
	struct lsa_info *handle;
	uint32_t num_domains;
	struct pdb_trusted_domain **domains;
	struct ForestTrustInfo *nfti;
	struct ForestTrustInfo *fti;
	struct lsa_ForestTrustCollisionInfo *c_info;
	struct pdb_domain_info *dom_info;
	enum ndr_err_code ndr_err;

	if (!IS_DC) {
		return NT_STATUS_NOT_SUPPORTED;
	}

	if (!find_policy_by_hnd(p, r->in.handle, (void **)(void *)&handle)) {
		return NT_STATUS_INVALID_HANDLE;
	}

	if (handle->type != LSA_HANDLE_TRUST_TYPE) {
		return NT_STATUS_INVALID_HANDLE;
	}

	if (!(handle->access & LSA_TRUSTED_SET_AUTH)) {
		return NT_STATUS_ACCESS_DENIED;
	}

	status = pdb_enum_trusted_domains(p->mem_ctx, &num_domains, &domains);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if (num_domains == 0) {
		return NT_STATUS_NO_SUCH_DOMAIN;
	}

	for (i = 0; i < num_domains; i++) {
		if (domains[i]->domain_name == NULL) {
			return NT_STATUS_INVALID_DOMAIN_STATE;
		}
		if (StrCaseCmp(domains[i]->domain_name,
			       r->in.trusted_domain_name->string) == 0) {
			break;
		}
	}
	if (i >= num_domains) {
		return NT_STATUS_NO_SUCH_DOMAIN;
	}

	if (!(domains[i]->trust_attributes &
	      LSA_TRUST_ATTRIBUTE_FOREST_TRANSITIVE)) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (r->in.highest_record_type >= LSA_FOREST_TRUST_RECORD_TYPE_LAST) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* The following section until COPY_END is a copy from
	 * source4/rpmc_server/lsa/scesrc_lsa.c */
	nfti = talloc(p->mem_ctx, struct ForestTrustInfo);
	if (!nfti) {
		return NT_STATUS_NO_MEMORY;
	}

	status = make_ft_info(nfti, r->in.forest_trust_info, nfti);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	c_info = talloc_zero(r->out.collision_info,
			     struct lsa_ForestTrustCollisionInfo);
	if (!c_info) {
		return NT_STATUS_NO_MEMORY;
	}

        /* first check own info, then other domains */
	fti = talloc(p->mem_ctx, struct ForestTrustInfo);
	if (!fti) {
		return NT_STATUS_NO_MEMORY;
	}

	dom_info = pdb_get_domain_info(p->mem_ctx);

	status = own_ft_info(dom_info, fti);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = check_ft_info(c_info, dom_info->dns_domain, fti, nfti, c_info);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	for (j = 0; j < num_domains; j++) {
		fti = talloc(p->mem_ctx, struct ForestTrustInfo);
		if (!fti) {
			return NT_STATUS_NO_MEMORY;
		}

		status = get_ft_info(p->mem_ctx, domains[j], fti);
		if (!NT_STATUS_IS_OK(status)) {
			if (NT_STATUS_EQUAL(status,
			    NT_STATUS_OBJECT_NAME_NOT_FOUND)) {
				continue;
			}
			return status;
		}

		if (domains[j]->domain_name == NULL) {
			return NT_STATUS_INVALID_DOMAIN_STATE;
		}

		status = check_ft_info(c_info, domains[j]->domain_name,
				       fti, nfti, c_info);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
	}

	*r->out.collision_info = c_info;

	if (r->in.check_only != 0) {
		return NT_STATUS_OK;
	}

	/* COPY_END */

	ndr_err = ndr_push_struct_blob(&domains[i]->trust_forest_trust_info,
				       p->mem_ctx, nfti,
				       (ndr_push_flags_fn_t)ndr_push_ForestTrustInfo);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	status = pdb_set_trusted_domain(domains[i]->domain_name, domains[i]);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	return NT_STATUS_OK;
}

NTSTATUS _lsa_CREDRRENAME(struct pipes_struct *p,
			  struct lsa_CREDRRENAME *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_LSAROPENPOLICYSCE(struct pipes_struct *p,
				struct lsa_LSAROPENPOLICYSCE *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_LSARADTREGISTERSECURITYEVENTSOURCE(struct pipes_struct *p,
						 struct lsa_LSARADTREGISTERSECURITYEVENTSOURCE *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_LSARADTUNREGISTERSECURITYEVENTSOURCE(struct pipes_struct *p,
						   struct lsa_LSARADTUNREGISTERSECURITYEVENTSOURCE *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _lsa_LSARADTREPORTSECURITYEVENT(struct pipes_struct *p,
					 struct lsa_LSARADTREPORTSECURITYEVENT *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}
