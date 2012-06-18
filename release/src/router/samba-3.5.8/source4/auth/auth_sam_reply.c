/* 
   Unix SMB/CIFS implementation.

   Convert a server info struct into the form for PAC and NETLOGON replies

   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2004
   Copyright (C) Stefan Metzmacher <metze@samba.org>  2005
   
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
#include "auth/auth.h"
#include "libcli/security/security.h"
#include "librpc/gen_ndr/ndr_netlogon.h"
#include "auth/auth_sam_reply.h"

NTSTATUS auth_convert_server_info_sambaseinfo(TALLOC_CTX *mem_ctx, 
					      struct auth_serversupplied_info *server_info, 
					      struct netr_SamBaseInfo **_sam)
{
	struct netr_SamBaseInfo *sam = talloc_zero(mem_ctx, struct netr_SamBaseInfo);
	NT_STATUS_HAVE_NO_MEMORY(sam);

	sam->domain_sid = dom_sid_dup(mem_ctx, server_info->account_sid);
	NT_STATUS_HAVE_NO_MEMORY(sam->domain_sid);
	sam->domain_sid->num_auths--;

	sam->last_logon = server_info->last_logon;
	sam->last_logoff =  server_info->last_logoff;
	sam->acct_expiry = server_info->acct_expiry;
	sam->last_password_change = server_info->last_password_change;
	sam->allow_password_change = server_info->allow_password_change;
	sam->force_password_change = server_info->force_password_change;

	sam->account_name.string = server_info->account_name;
	sam->full_name.string = server_info->full_name;
	sam->logon_script.string = server_info->logon_script;
	sam->profile_path.string = server_info->profile_path;
	sam->home_directory.string = server_info->home_directory;
	sam->home_drive.string = server_info->home_drive;

	sam->logon_count = server_info->logon_count;
	sam->bad_password_count = sam->bad_password_count;
	sam->rid = server_info->account_sid->sub_auths[server_info->account_sid->num_auths-1];
	sam->primary_gid = server_info->primary_group_sid->sub_auths[server_info->primary_group_sid->num_auths-1];

	sam->groups.count = 0;
	sam->groups.rids = NULL;

	if (server_info->n_domain_groups > 0) {
		int i;
		sam->groups.rids = talloc_array(sam, struct samr_RidWithAttribute,
						server_info->n_domain_groups);

		if (sam->groups.rids == NULL)
			return NT_STATUS_NO_MEMORY;

		for (i=0; i<server_info->n_domain_groups; i++) {
			struct dom_sid *group_sid = server_info->domain_groups[i];
			if (!dom_sid_in_domain(sam->domain_sid, group_sid)) {
				/* We handle this elsewhere */
				continue;
			}
			sam->groups.rids[sam->groups.count].rid =
				group_sid->sub_auths[group_sid->num_auths-1];
			
			sam->groups.rids[sam->groups.count].attributes = 
				SE_GROUP_MANDATORY | SE_GROUP_ENABLED_BY_DEFAULT | SE_GROUP_ENABLED;
			sam->groups.count += 1;
		}
	}

	sam->user_flags = 0; /* w2k3 uses NETLOGON_EXTRA_SIDS | NETLOGON_NTLMV2_ENABLED */
	sam->acct_flags = server_info->acct_flags;
	sam->logon_server.string = server_info->logon_server;
	sam->domain.string = server_info->domain_name;

	ZERO_STRUCT(sam->unknown);

	ZERO_STRUCT(sam->key);
	if (server_info->user_session_key.length == sizeof(sam->key.key)) {
		memcpy(sam->key.key, server_info->user_session_key.data, sizeof(sam->key.key));
	}

	ZERO_STRUCT(sam->LMSessKey);
	if (server_info->lm_session_key.length == sizeof(sam->LMSessKey.key)) {
		memcpy(sam->LMSessKey.key, server_info->lm_session_key.data, 
		       sizeof(sam->LMSessKey.key));
	}
	
	*_sam = sam;

	return NT_STATUS_OK;
}	

NTSTATUS auth_convert_server_info_saminfo3(TALLOC_CTX *mem_ctx, 
					   struct auth_serversupplied_info *server_info, 
					   struct netr_SamInfo3 **_sam3)
{
	struct netr_SamBaseInfo *sam;
	struct netr_SamInfo3 *sam3 = talloc_zero(mem_ctx, struct netr_SamInfo3);
	NTSTATUS status;
	int i;
	NT_STATUS_HAVE_NO_MEMORY(sam3);

	status = auth_convert_server_info_sambaseinfo(mem_ctx, server_info, &sam);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	sam3->base = *sam;
	sam3->sidcount	= 0;
	sam3->sids	= NULL;

	
	sam3->sids = talloc_array(sam, struct netr_SidAttr,
				  server_info->n_domain_groups);
	NT_STATUS_HAVE_NO_MEMORY(sam3->sids);
	
	for (i=0; i<server_info->n_domain_groups; i++) {
		if (dom_sid_in_domain(sam->domain_sid, server_info->domain_groups[i])) {
			continue;
		}
		sam3->sids[sam3->sidcount].sid = talloc_reference(sam3->sids,server_info->domain_groups[i]);
		sam3->sids[sam3->sidcount].attributes =
			SE_GROUP_MANDATORY | SE_GROUP_ENABLED_BY_DEFAULT | SE_GROUP_ENABLED;
		sam3->sidcount += 1;
	}
	if (sam3->sidcount) {
		sam3->base.user_flags |= NETLOGON_EXTRA_SIDS;
	} else {
		sam3->sids = NULL;
	}
	*_sam3 = sam3;

	return NT_STATUS_OK;
}	

/**
 * Make a server_info struct from the info3 returned by a domain logon 
 */
NTSTATUS make_server_info_netlogon_validation(TALLOC_CTX *mem_ctx,
					      const char *account_name,
					      uint16_t validation_level,
					      union netr_Validation *validation,
					      struct auth_serversupplied_info **_server_info)
{
	struct auth_serversupplied_info *server_info;
	struct netr_SamBaseInfo *base = NULL;
	int i;

	switch (validation_level) {
	case 2:
		if (!validation || !validation->sam2) {
			return NT_STATUS_INVALID_PARAMETER;
		}
		base = &validation->sam2->base;
		break;
	case 3:
		if (!validation || !validation->sam3) {
			return NT_STATUS_INVALID_PARAMETER;
		}
		base = &validation->sam3->base;
		break;
	case 6:
		if (!validation || !validation->sam6) {
			return NT_STATUS_INVALID_PARAMETER;
		}
		base = &validation->sam6->base;
		break;
	default:
		return NT_STATUS_INVALID_LEVEL;
	}

	server_info = talloc(mem_ctx, struct auth_serversupplied_info);
	NT_STATUS_HAVE_NO_MEMORY(server_info);

	/*
	   Here is where we should check the list of
	   trusted domains, and verify that the SID 
	   matches.
	*/
	server_info->account_sid = dom_sid_add_rid(server_info, base->domain_sid, base->rid);
	NT_STATUS_HAVE_NO_MEMORY(server_info->account_sid);


	server_info->primary_group_sid = dom_sid_add_rid(server_info, base->domain_sid, base->primary_gid);
	NT_STATUS_HAVE_NO_MEMORY(server_info->primary_group_sid);

	server_info->n_domain_groups = base->groups.count;
	if (base->groups.count) {
		server_info->domain_groups = talloc_array(server_info, struct dom_sid*, base->groups.count);
		NT_STATUS_HAVE_NO_MEMORY(server_info->domain_groups);
	} else {
		server_info->domain_groups = NULL;
	}

	for (i = 0; i < base->groups.count; i++) {
		server_info->domain_groups[i] = dom_sid_add_rid(server_info, base->domain_sid, base->groups.rids[i].rid);
		NT_STATUS_HAVE_NO_MEMORY(server_info->domain_groups[i]);
	}

	/* Copy 'other' sids.  We need to do sid filtering here to
 	   prevent possible elevation of privileges.  See:

           http://www.microsoft.com/windows2000/techinfo/administration/security/sidfilter.asp
         */

	if (validation_level == 3) {
		struct dom_sid **dgrps = server_info->domain_groups;
		size_t sidcount = server_info->n_domain_groups + validation->sam3->sidcount;
		size_t n_dgrps = server_info->n_domain_groups;

		if (validation->sam3->sidcount > 0) {
			dgrps = talloc_realloc(server_info, dgrps, struct dom_sid*, sidcount);
			NT_STATUS_HAVE_NO_MEMORY(dgrps);

			for (i = 0; i < validation->sam3->sidcount; i++) {
				dgrps[n_dgrps + i] = talloc_reference(dgrps, validation->sam3->sids[i].sid);
			}
		}

		server_info->n_domain_groups = sidcount;
		server_info->domain_groups = dgrps;

		/* Where are the 'global' sids?... */
	}

	if (base->account_name.string) {
		server_info->account_name = talloc_reference(server_info, base->account_name.string);
	} else {
		server_info->account_name = talloc_strdup(server_info, account_name);
		NT_STATUS_HAVE_NO_MEMORY(server_info->account_name);
	}

	server_info->domain_name = talloc_reference(server_info, base->domain.string);
	server_info->full_name = talloc_reference(server_info, base->full_name.string);
	server_info->logon_script = talloc_reference(server_info, base->logon_script.string);
	server_info->profile_path = talloc_reference(server_info, base->profile_path.string);
	server_info->home_directory = talloc_reference(server_info, base->home_directory.string);
	server_info->home_drive = talloc_reference(server_info, base->home_drive.string);
	server_info->logon_server = talloc_reference(server_info, base->logon_server.string);
	server_info->last_logon = base->last_logon;
	server_info->last_logoff = base->last_logoff;
	server_info->acct_expiry = base->acct_expiry;
	server_info->last_password_change = base->last_password_change;
	server_info->allow_password_change = base->allow_password_change;
	server_info->force_password_change = base->force_password_change;
	server_info->logon_count = base->logon_count;
	server_info->bad_password_count = base->bad_password_count;
	server_info->acct_flags = base->acct_flags;

	server_info->authenticated = true;

	/* ensure we are never given NULL session keys */

	if (all_zero(base->key.key, sizeof(base->key.key))) {
		server_info->user_session_key = data_blob(NULL, 0);
	} else {
		server_info->user_session_key = data_blob_talloc(server_info, base->key.key, sizeof(base->key.key));
		NT_STATUS_HAVE_NO_MEMORY(server_info->user_session_key.data);
	}

	if (all_zero(base->LMSessKey.key, sizeof(base->LMSessKey.key))) {
		server_info->lm_session_key = data_blob(NULL, 0);
	} else {
		server_info->lm_session_key = data_blob_talloc(server_info, base->LMSessKey.key, sizeof(base->LMSessKey.key));
		NT_STATUS_HAVE_NO_MEMORY(server_info->lm_session_key.data);
	}

	ZERO_STRUCT(server_info->pac_srv_sig);
	ZERO_STRUCT(server_info->pac_kdc_sig);

	*_server_info = server_info;
	return NT_STATUS_OK;
}

