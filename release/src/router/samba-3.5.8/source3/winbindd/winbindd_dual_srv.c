/*
   Unix SMB/CIFS implementation.

   In-Child server implementation of the routines defined in wbint.idl

   Copyright (C) Volker Lendecke 2009
   Copyright (C) Guenther Deschner 2009

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
#include "winbindd/winbindd.h"
#include "winbindd/winbindd_proto.h"
#include "librpc/gen_ndr/srv_wbint.h"
#include "../librpc/gen_ndr/cli_netlogon.h"

void _wbint_Ping(pipes_struct *p, struct wbint_Ping *r)
{
	*r->out.out_data = r->in.in_data;
}

NTSTATUS _wbint_LookupSid(pipes_struct *p, struct wbint_LookupSid *r)
{
	struct winbindd_domain *domain = wb_child_domain();
	char *dom_name;
	char *name;
	enum lsa_SidType type;
	NTSTATUS status;

	if (domain == NULL) {
		return NT_STATUS_REQUEST_NOT_ACCEPTED;
	}

	status = domain->methods->sid_to_name(domain, p->mem_ctx, r->in.sid,
					      &dom_name, &name, &type);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	*r->out.domain = dom_name;
	*r->out.name = name;
	*r->out.type = type;
	return NT_STATUS_OK;
}

NTSTATUS _wbint_LookupName(pipes_struct *p, struct wbint_LookupName *r)
{
	struct winbindd_domain *domain = wb_child_domain();

	if (domain == NULL) {
		return NT_STATUS_REQUEST_NOT_ACCEPTED;
	}

	return domain->methods->name_to_sid(
		domain, p->mem_ctx, r->in.domain, r->in.name, r->in.flags,
		r->out.sid, r->out.type);
}

NTSTATUS _wbint_Sid2Uid(pipes_struct *p, struct wbint_Sid2Uid *r)
{
	uid_t uid;
	NTSTATUS status;

	status = idmap_sid_to_uid(r->in.dom_name ? r->in.dom_name : "",
				  r->in.sid, &uid);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	*r->out.uid = uid;
	return NT_STATUS_OK;
}

NTSTATUS _wbint_Sid2Gid(pipes_struct *p, struct wbint_Sid2Gid *r)
{
	gid_t gid;
	NTSTATUS status;

	status = idmap_sid_to_gid(r->in.dom_name ? r->in.dom_name : "",
				  r->in.sid, &gid);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	*r->out.gid = gid;
	return NT_STATUS_OK;
}

NTSTATUS _wbint_Uid2Sid(pipes_struct *p, struct wbint_Uid2Sid *r)
{
	return idmap_uid_to_sid(r->in.dom_name ? r->in.dom_name : "",
				r->out.sid, r->in.uid);
}

NTSTATUS _wbint_Gid2Sid(pipes_struct *p, struct wbint_Gid2Sid *r)
{
	return idmap_gid_to_sid(r->in.dom_name ? r->in.dom_name : "",
				r->out.sid, r->in.gid);
}

NTSTATUS _wbint_AllocateUid(pipes_struct *p, struct wbint_AllocateUid *r)
{
	struct unixid xid;
	NTSTATUS status;

	status = idmap_allocate_uid(&xid);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	*r->out.uid = xid.id;
	return NT_STATUS_OK;
}

NTSTATUS _wbint_AllocateGid(pipes_struct *p, struct wbint_AllocateGid *r)
{
	struct unixid xid;
	NTSTATUS status;

	status = idmap_allocate_gid(&xid);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	*r->out.gid = xid.id;
	return NT_STATUS_OK;
}

NTSTATUS _wbint_QueryUser(pipes_struct *p, struct wbint_QueryUser *r)
{
	struct winbindd_domain *domain = wb_child_domain();

	if (domain == NULL) {
		return NT_STATUS_REQUEST_NOT_ACCEPTED;
	}

	return domain->methods->query_user(domain, p->mem_ctx, r->in.sid,
					   r->out.info);
}

NTSTATUS _wbint_LookupUserAliases(pipes_struct *p,
				  struct wbint_LookupUserAliases *r)
{
	struct winbindd_domain *domain = wb_child_domain();

	if (domain == NULL) {
		return NT_STATUS_REQUEST_NOT_ACCEPTED;
	}

	return domain->methods->lookup_useraliases(
		domain, p->mem_ctx, r->in.sids->num_sids, r->in.sids->sids,
		&r->out.rids->num_rids, &r->out.rids->rids);
}

NTSTATUS _wbint_LookupUserGroups(pipes_struct *p,
				 struct wbint_LookupUserGroups *r)
{
	struct winbindd_domain *domain = wb_child_domain();

	if (domain == NULL) {
		return NT_STATUS_REQUEST_NOT_ACCEPTED;
	}

	return domain->methods->lookup_usergroups(
		domain, p->mem_ctx, r->in.sid,
		&r->out.sids->num_sids, &r->out.sids->sids);
}

NTSTATUS _wbint_QuerySequenceNumber(pipes_struct *p,
				    struct wbint_QuerySequenceNumber *r)
{
	struct winbindd_domain *domain = wb_child_domain();

	if (domain == NULL) {
		return NT_STATUS_REQUEST_NOT_ACCEPTED;
	}

	return domain->methods->sequence_number(domain, r->out.sequence);
}

NTSTATUS _wbint_LookupGroupMembers(pipes_struct *p,
				   struct wbint_LookupGroupMembers *r)
{
	struct winbindd_domain *domain = wb_child_domain();
	uint32_t i, num_names;
	struct dom_sid *sid_mem;
	char **names;
	uint32_t *name_types;
	NTSTATUS status;

	if (domain == NULL) {
		return NT_STATUS_REQUEST_NOT_ACCEPTED;
	}

	status = domain->methods->lookup_groupmem(
		domain, p->mem_ctx, r->in.sid, r->in.type,
		&num_names, &sid_mem, &names, &name_types);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	r->out.members->num_principals = num_names;
	r->out.members->principals = talloc_array(
		r->out.members, struct wbint_Principal, num_names);
	if (r->out.members->principals == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	for (i=0; i<num_names; i++) {
		struct wbint_Principal *m = &r->out.members->principals[i];
		sid_copy(&m->sid, &sid_mem[i]);
		m->name = talloc_move(r->out.members->principals, &names[i]);
		m->type = (enum lsa_SidType)name_types[i];
	}

	return NT_STATUS_OK;
}

NTSTATUS _wbint_QueryUserList(pipes_struct *p, struct wbint_QueryUserList *r)
{
	struct winbindd_domain *domain = wb_child_domain();

	if (domain == NULL) {
		return NT_STATUS_REQUEST_NOT_ACCEPTED;
	}

	return domain->methods->query_user_list(
		domain, p->mem_ctx, &r->out.users->num_userinfos,
		&r->out.users->userinfos);
}

NTSTATUS _wbint_QueryGroupList(pipes_struct *p, struct wbint_QueryGroupList *r)
{
	struct winbindd_domain *domain = wb_child_domain();
	uint32_t i, num_groups;
	struct acct_info *groups;
	struct wbint_Principal *result;
	NTSTATUS status;

	if (domain == NULL) {
		return NT_STATUS_REQUEST_NOT_ACCEPTED;
	}

	status = domain->methods->enum_dom_groups(domain, talloc_tos(),
						  &num_groups, &groups);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	result = talloc_array(r->out.groups, struct wbint_Principal,
			      num_groups);
	if (result == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	for (i=0; i<num_groups; i++) {
		sid_compose(&result[i].sid, &domain->sid, groups[i].rid);
		result[i].type = SID_NAME_DOM_GRP;
		result[i].name = talloc_strdup(result, groups[i].acct_name);
		if (result[i].name == NULL) {
			TALLOC_FREE(result);
			TALLOC_FREE(groups);
			return NT_STATUS_NO_MEMORY;
		}
	}

	r->out.groups->num_principals = num_groups;
	r->out.groups->principals = result;
	return NT_STATUS_OK;
}

NTSTATUS _wbint_DsGetDcName(pipes_struct *p, struct wbint_DsGetDcName *r)
{
	struct winbindd_domain *domain = wb_child_domain();
	struct rpc_pipe_client *netlogon_pipe;
	struct netr_DsRGetDCNameInfo *dc_info;
	NTSTATUS status;
	WERROR werr;
	unsigned int orig_timeout;

	if (domain == NULL) {
		return dsgetdcname(p->mem_ctx, winbind_messaging_context(),
				   r->in.domain_name, r->in.domain_guid,
				   r->in.site_name ? r->in.site_name : "",
				   r->in.flags,
				   r->out.dc_info);
	}

	status = cm_connect_netlogon(domain, &netlogon_pipe);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("Can't contact the NETLOGON pipe\n"));
		return status;
	}

	/* This call can take a long time - allow the server to time out.
	   35 seconds should do it. */

	orig_timeout = rpccli_set_timeout(netlogon_pipe, 35000);

	if (domain->active_directory) {
		status = rpccli_netr_DsRGetDCName(
			netlogon_pipe, p->mem_ctx, domain->dcname,
			r->in.domain_name, NULL, r->in.domain_guid,
			r->in.flags, r->out.dc_info, &werr);
		if (NT_STATUS_IS_OK(status) && W_ERROR_IS_OK(werr)) {
			goto done;
		}
	}

	/*
	 * Fallback to less capable methods
	 */

	dc_info = talloc_zero(r->out.dc_info, struct netr_DsRGetDCNameInfo);
	if (dc_info == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}

	if (r->in.flags & DS_PDC_REQUIRED) {
		status = rpccli_netr_GetDcName(
			netlogon_pipe, p->mem_ctx, domain->dcname,
			r->in.domain_name, &dc_info->dc_unc, &werr);
	} else {
		status = rpccli_netr_GetAnyDCName(
			netlogon_pipe, p->mem_ctx, domain->dcname,
			r->in.domain_name, &dc_info->dc_unc, &werr);
	}

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("rpccli_netr_Get[Any]DCName failed: %s\n",
			   nt_errstr(status)));
		goto done;
	}
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(10, ("rpccli_netr_Get[Any]DCName failed: %s\n",
			   win_errstr(werr)));
		status = werror_to_ntstatus(werr);
		goto done;
	}

	*r->out.dc_info = dc_info;
	status = NT_STATUS_OK;

done:
	/* And restore our original timeout. */
	rpccli_set_timeout(netlogon_pipe, orig_timeout);

	return status;
}

NTSTATUS _wbint_LookupRids(pipes_struct *p, struct wbint_LookupRids *r)
{
	struct winbindd_domain *domain = wb_child_domain();
	char *domain_name;
	char **names;
	enum lsa_SidType *types;
	struct wbint_Principal *result;
	NTSTATUS status;
	int i;

	if (domain == NULL) {
		return NT_STATUS_REQUEST_NOT_ACCEPTED;
	}

	status = domain->methods->rids_to_names(
		domain, talloc_tos(), &domain->sid, r->in.rids->rids,
		r->in.rids->num_rids, &domain_name, &names, &types);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	*r->out.domain_name = talloc_move(r->out.domain_name, &domain_name);

	result = talloc_array(p->mem_ctx, struct wbint_Principal,
			      r->in.rids->num_rids);
	if (result == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	for (i=0; i<r->in.rids->num_rids; i++) {
		sid_compose(&result[i].sid, &domain->sid, r->in.rids->rids[i]);
		result[i].type = types[i];
		result[i].name = talloc_move(result, &names[i]);
	}
	TALLOC_FREE(types);
	TALLOC_FREE(names);

	r->out.names->num_principals = r->in.rids->num_rids;
	r->out.names->principals = result;
	return NT_STATUS_OK;
}

NTSTATUS _wbint_CheckMachineAccount(pipes_struct *p,
				    struct wbint_CheckMachineAccount *r)
{
	struct winbindd_domain *domain;
	int num_retries = 0;
	NTSTATUS status;

	domain = wb_child_domain();
	if (domain == NULL) {
		return NT_STATUS_REQUEST_NOT_ACCEPTED;
	}

again:
	invalidate_cm_connection(&domain->conn);

	{
		struct rpc_pipe_client *netlogon_pipe;
		status = cm_connect_netlogon(domain, &netlogon_pipe);
	}

        /* There is a race condition between fetching the trust account
           password and the periodic machine password change.  So it's
	   possible that the trust account password has been changed on us.
	   We are returned NT_STATUS_ACCESS_DENIED if this happens. */

#define MAX_RETRIES 3

        if ((num_retries < MAX_RETRIES)
	    && NT_STATUS_EQUAL(status, NT_STATUS_ACCESS_DENIED)) {
                num_retries++;
                goto again;
        }

        if (!NT_STATUS_IS_OK(status)) {
                DEBUG(3, ("could not open handle to NETLOGON pipe\n"));
                goto done;
        }

	/* Pass back result code - zero for success, other values for
	   specific failures. */

	DEBUG(3,("domain %s secret is %s\n", domain->name,
		NT_STATUS_IS_OK(status) ? "good" : "bad"));

 done:
	DEBUG(NT_STATUS_IS_OK(status) ? 5 : 2,
	      ("Checking the trust account password for domain %s returned %s\n",
	       domain->name, nt_errstr(status)));

	return status;
}

NTSTATUS _wbint_ChangeMachineAccount(pipes_struct *p,
				     struct wbint_ChangeMachineAccount *r)
{
	struct winbindd_domain *domain;
	int num_retries = 0;
	NTSTATUS status;
	struct rpc_pipe_client *netlogon_pipe;
	TALLOC_CTX *tmp_ctx;

again:
	domain = wb_child_domain();
	if (domain == NULL) {
		return NT_STATUS_REQUEST_NOT_ACCEPTED;
	}

	invalidate_cm_connection(&domain->conn);

	{
		status = cm_connect_netlogon(domain, &netlogon_pipe);
	}

	/* There is a race condition between fetching the trust account
	   password and the periodic machine password change.  So it's
	   possible that the trust account password has been changed on us.
	   We are returned NT_STATUS_ACCESS_DENIED if this happens. */

#define MAX_RETRIES 3

	if ((num_retries < MAX_RETRIES)
	     && NT_STATUS_EQUAL(status, NT_STATUS_ACCESS_DENIED)) {
		num_retries++;
		goto again;
	}

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(3, ("could not open handle to NETLOGON pipe\n"));
		goto done;
	}

	tmp_ctx = talloc_new(p->mem_ctx);

	status = trust_pw_find_change_and_store_it(netlogon_pipe,
						   tmp_ctx,
						   domain->name);
	talloc_destroy(tmp_ctx);

	/* Pass back result code - zero for success, other values for
	   specific failures. */

	DEBUG(3,("domain %s secret %s\n", domain->name,
		NT_STATUS_IS_OK(status) ? "changed" : "unchanged"));

 done:
	DEBUG(NT_STATUS_IS_OK(status) ? 5 : 2,
	      ("Changing the trust account password for domain %s returned %s\n",
	       domain->name, nt_errstr(status)));

	return status;
}

NTSTATUS _wbint_PingDc(pipes_struct *p, struct wbint_PingDc *r)
{
	NTSTATUS status;
	struct winbindd_domain *domain;
	struct rpc_pipe_client *netlogon_pipe;
	union netr_CONTROL_QUERY_INFORMATION info;
	WERROR werr;
	fstring logon_server;

	domain = wb_child_domain();
	if (domain == NULL) {
		return NT_STATUS_REQUEST_NOT_ACCEPTED;
	}

	status = cm_connect_netlogon(domain, &netlogon_pipe);
        if (!NT_STATUS_IS_OK(status)) {
                DEBUG(3, ("could not open handle to NETLOGON pipe\n"));
		return status;
        }

	fstr_sprintf(logon_server, "\\\\%s", domain->dcname);

	/*
	 * This provokes a WERR_NOT_SUPPORTED error message. This is
	 * documented in the wspp docs. I could not get a successful
	 * call to work, but the main point here is testing that the
	 * netlogon pipe works.
	 */
	status = rpccli_netr_LogonControl(netlogon_pipe, p->mem_ctx,
					  logon_server, NETLOGON_CONTROL_QUERY,
					  2, &info, &werr);

	if (NT_STATUS_EQUAL(status, NT_STATUS_IO_TIMEOUT)) {
		DEBUG(2, ("rpccli_netr_LogonControl timed out\n"));
		invalidate_cm_connection(&domain->conn);
		return status;
	}

	if (!NT_STATUS_EQUAL(status, NT_STATUS_CTL_FILE_NOT_SUPPORTED)) {
		DEBUG(2, ("rpccli_netr_LogonControl returned %s, expected "
			  "NT_STATUS_CTL_FILE_NOT_SUPPORTED\n",
			  nt_errstr(status)));
		return status;
	}

	DEBUG(5, ("winbindd_dual_ping_dc succeeded\n"));
	return NT_STATUS_OK;
}

NTSTATUS _wbint_SetMapping(pipes_struct *p, struct wbint_SetMapping *r)
{
	struct id_map map;

	map.sid = r->in.sid;
	map.xid.id = r->in.id;
	map.status = ID_MAPPED;

	switch (r->in.type) {
	case WBINT_ID_TYPE_UID:
		map.xid.type = ID_TYPE_UID;
		break;
	case WBINT_ID_TYPE_GID:
		map.xid.type = ID_TYPE_GID;
		break;
	default:
		return NT_STATUS_INVALID_PARAMETER;
	}

	return idmap_set_mapping(&map);
}

NTSTATUS _wbint_RemoveMapping(pipes_struct *p, struct wbint_RemoveMapping *r)
{
	struct id_map map;

	map.sid = r->in.sid;
	map.xid.id = r->in.id;
	map.status = ID_MAPPED;

	switch (r->in.type) {
	case WBINT_ID_TYPE_UID:
		map.xid.type = ID_TYPE_UID;
		break;
	case WBINT_ID_TYPE_GID:
		map.xid.type = ID_TYPE_GID;
		break;
	default:
		return NT_STATUS_INVALID_PARAMETER;
	}

	return idmap_remove_mapping(&map);
}

NTSTATUS _wbint_SetHWM(pipes_struct *p, struct wbint_SetHWM *r)
{
	struct unixid id;
	NTSTATUS status;

	id.id = r->in.id;

	switch (r->in.type) {
	case WBINT_ID_TYPE_UID:
		id.type = ID_TYPE_UID;
		status = idmap_set_uid_hwm(&id);
		break;
	case WBINT_ID_TYPE_GID:
		id.type = ID_TYPE_GID;
		status = idmap_set_gid_hwm(&id);
		break;
	default:
		status = NT_STATUS_INVALID_PARAMETER;
		break;
	}
	return status;
}
