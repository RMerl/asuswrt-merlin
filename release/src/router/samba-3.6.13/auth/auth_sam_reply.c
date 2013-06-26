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
#include "librpc/gen_ndr/auth.h"
#include "libcli/security/security.h"
#include "auth/auth_sam_reply.h"

NTSTATUS auth_convert_user_info_dc_sambaseinfo(TALLOC_CTX *mem_ctx,
					      struct auth_user_info_dc *user_info_dc,
					      struct netr_SamBaseInfo **_sam)
{
	NTSTATUS status;
	struct auth_user_info *info;
	struct netr_SamBaseInfo *sam = talloc_zero(mem_ctx, struct netr_SamBaseInfo);
	NT_STATUS_HAVE_NO_MEMORY(sam);

	if (user_info_dc->num_sids > PRIMARY_USER_SID_INDEX) {
		status = dom_sid_split_rid(sam, &user_info_dc->sids[PRIMARY_USER_SID_INDEX],
					   &sam->domain_sid, &sam->rid);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
	} else {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (user_info_dc->num_sids > PRIMARY_GROUP_SID_INDEX) {
		status = dom_sid_split_rid(NULL, &user_info_dc->sids[PRIMARY_GROUP_SID_INDEX],
					   NULL, &sam->primary_gid);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
	} else {
		/* if we have to encode something like SYSTEM (with no
		 * second SID in the token) then this is the only
		 * choice */
		sam->primary_gid = sam->rid;
	}

	info = user_info_dc->info;

	sam->last_logon = info->last_logon;
	sam->last_logoff =  info->last_logoff;
	sam->acct_expiry = info->acct_expiry;
	sam->last_password_change = info->last_password_change;
	sam->allow_password_change = info->allow_password_change;
	sam->force_password_change = info->force_password_change;

	sam->account_name.string = info->account_name;
	sam->full_name.string = info->full_name;
	sam->logon_script.string = info->logon_script;
	sam->profile_path.string = info->profile_path;
	sam->home_directory.string = info->home_directory;
	sam->home_drive.string = info->home_drive;

	sam->logon_count = info->logon_count;
	sam->bad_password_count = info->bad_password_count;
	sam->groups.count = 0;
	sam->groups.rids = NULL;

	if (user_info_dc->num_sids > 2) {
		size_t i;
		sam->groups.rids = talloc_array(sam, struct samr_RidWithAttribute,
						user_info_dc->num_sids);

		if (sam->groups.rids == NULL)
			return NT_STATUS_NO_MEMORY;

		for (i=2; i<user_info_dc->num_sids; i++) {
			struct dom_sid *group_sid = &user_info_dc->sids[i];
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
	sam->acct_flags = user_info_dc->info->acct_flags;
	sam->logon_server.string = user_info_dc->info->logon_server;
	sam->domain.string = user_info_dc->info->domain_name;

	ZERO_STRUCT(sam->unknown);

	ZERO_STRUCT(sam->key);
	if (user_info_dc->user_session_key.length == sizeof(sam->key.key)) {
		memcpy(sam->key.key, user_info_dc->user_session_key.data, sizeof(sam->key.key));
	}

	ZERO_STRUCT(sam->LMSessKey);
	if (user_info_dc->lm_session_key.length == sizeof(sam->LMSessKey.key)) {
		memcpy(sam->LMSessKey.key, user_info_dc->lm_session_key.data,
		       sizeof(sam->LMSessKey.key));
	}

	*_sam = sam;

	return NT_STATUS_OK;
}

/* Note that the validity of the _sam3 structure is only as long as
 * the user_info_dc it was generated from */
NTSTATUS auth_convert_user_info_dc_saminfo3(TALLOC_CTX *mem_ctx,
					   struct auth_user_info_dc *user_info_dc,
					   struct netr_SamInfo3 **_sam3)
{
	struct netr_SamBaseInfo *sam;
	struct netr_SamInfo3 *sam3 = talloc_zero(mem_ctx, struct netr_SamInfo3);
	NTSTATUS status;
	size_t i;
	NT_STATUS_HAVE_NO_MEMORY(sam3);

	status = auth_convert_user_info_dc_sambaseinfo(sam3, user_info_dc, &sam);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(sam3);
		return status;
	}
	sam3->base = *sam;
	sam3->sidcount	= 0;
	sam3->sids	= NULL;


	sam3->sids = talloc_array(sam, struct netr_SidAttr,
				  user_info_dc->num_sids);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(sam3->sids, sam3);

	/* We don't put the user and group SIDs in there */
	for (i=2; i<user_info_dc->num_sids; i++) {
		if (dom_sid_in_domain(sam->domain_sid, &user_info_dc->sids[i])) {
			continue;
		}
		sam3->sids[sam3->sidcount].sid = dom_sid_dup(sam3->sids, &user_info_dc->sids[i]);
		NT_STATUS_HAVE_NO_MEMORY_AND_FREE(sam3->sids[sam3->sidcount].sid, sam3);
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
 * Make a user_info_dc struct from the info3 returned by a domain logon
 */
NTSTATUS make_user_info_dc_netlogon_validation(TALLOC_CTX *mem_ctx,
					      const char *account_name,
					      uint16_t validation_level,
					      union netr_Validation *validation,
					      struct auth_user_info_dc **_user_info_dc)
{
	struct auth_user_info_dc *user_info_dc;
	struct auth_user_info *info;
	struct netr_SamBaseInfo *base = NULL;
	uint32_t i;

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

	user_info_dc = talloc(mem_ctx, struct auth_user_info_dc);
	NT_STATUS_HAVE_NO_MEMORY(user_info_dc);

	/*
	   Here is where we should check the list of
	   trusted domains, and verify that the SID
	   matches.
	*/
	if (!base->domain_sid) {
		DEBUG(0, ("Cannot operate on a Netlogon Validation without a domain SID"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* The IDL layer would be a better place to check this, but to
	 * guard the integer addition below, we double-check */
	if (base->groups.count > 65535) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	user_info_dc->num_sids = 2;

	user_info_dc->sids = talloc_array(user_info_dc, struct dom_sid,  user_info_dc->num_sids + base->groups.count);
	NT_STATUS_HAVE_NO_MEMORY(user_info_dc->sids);

	user_info_dc->sids[PRIMARY_USER_SID_INDEX] = *base->domain_sid;
	if (!sid_append_rid(&user_info_dc->sids[PRIMARY_USER_SID_INDEX], base->rid)) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	user_info_dc->sids[PRIMARY_GROUP_SID_INDEX] = *base->domain_sid;
	if (!sid_append_rid(&user_info_dc->sids[PRIMARY_GROUP_SID_INDEX], base->primary_gid)) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	for (i = 0; i < base->groups.count; i++) {
		user_info_dc->sids[user_info_dc->num_sids] = *base->domain_sid;
		if (!sid_append_rid(&user_info_dc->sids[user_info_dc->num_sids], base->groups.rids[i].rid)) {
			return NT_STATUS_INVALID_PARAMETER;
		}
		user_info_dc->num_sids++;
	}

	/* Copy 'other' sids.  We need to do sid filtering here to
	   prevent possible elevation of privileges.  See:

           http://www.microsoft.com/windows2000/techinfo/administration/security/sidfilter.asp
         */

	if (validation_level == 3) {
		struct dom_sid *dgrps = user_info_dc->sids;
		size_t sidcount;

		/* The IDL layer would be a better place to check this, but to
		 * guard the integer addition below, we double-check */
		if (validation->sam3->sidcount > 65535) {
			return NT_STATUS_INVALID_PARAMETER;
		}

		sidcount = user_info_dc->num_sids + validation->sam3->sidcount;
		if (validation->sam3->sidcount > 0) {
			dgrps = talloc_realloc(user_info_dc, dgrps, struct dom_sid, sidcount);
			NT_STATUS_HAVE_NO_MEMORY(dgrps);

			for (i = 0; i < validation->sam3->sidcount; i++) {
				if (validation->sam3->sids[i].sid) {
					dgrps[user_info_dc->num_sids] = *validation->sam3->sids[i].sid;
					user_info_dc->num_sids++;
				}
			}
		}

		user_info_dc->sids = dgrps;

		/* Where are the 'global' sids?... */
	}

	user_info_dc->info = info = talloc_zero(user_info_dc, struct auth_user_info);
	NT_STATUS_HAVE_NO_MEMORY(user_info_dc->info);

	if (base->account_name.string) {
		info->account_name = talloc_reference(info, base->account_name.string);
	} else {
		info->account_name = talloc_strdup(info, account_name);
		NT_STATUS_HAVE_NO_MEMORY(info->account_name);
	}

	info->domain_name = talloc_reference(info, base->domain.string);
	info->full_name = talloc_reference(info, base->full_name.string);
	info->logon_script = talloc_reference(info, base->logon_script.string);
	info->profile_path = talloc_reference(info, base->profile_path.string);
	info->home_directory = talloc_reference(info, base->home_directory.string);
	info->home_drive = talloc_reference(info, base->home_drive.string);
	info->logon_server = talloc_reference(info, base->logon_server.string);
	info->last_logon = base->last_logon;
	info->last_logoff = base->last_logoff;
	info->acct_expiry = base->acct_expiry;
	info->last_password_change = base->last_password_change;
	info->allow_password_change = base->allow_password_change;
	info->force_password_change = base->force_password_change;
	info->logon_count = base->logon_count;
	info->bad_password_count = base->bad_password_count;
	info->acct_flags = base->acct_flags;

	info->authenticated = true;

	/* ensure we are never given NULL session keys */

	if (all_zero(base->key.key, sizeof(base->key.key))) {
		user_info_dc->user_session_key = data_blob(NULL, 0);
	} else {
		user_info_dc->user_session_key = data_blob_talloc(user_info_dc, base->key.key, sizeof(base->key.key));
		NT_STATUS_HAVE_NO_MEMORY(user_info_dc->user_session_key.data);
	}

	if (all_zero(base->LMSessKey.key, sizeof(base->LMSessKey.key))) {
		user_info_dc->lm_session_key = data_blob(NULL, 0);
	} else {
		user_info_dc->lm_session_key = data_blob_talloc(user_info_dc, base->LMSessKey.key, sizeof(base->LMSessKey.key));
		NT_STATUS_HAVE_NO_MEMORY(user_info_dc->lm_session_key.data);
	}

	*_user_info_dc = user_info_dc;
	return NT_STATUS_OK;
}

/**
 * Make a user_info_dc struct from the PAC_LOGON_INFO supplied in the krb5 logon
 */
NTSTATUS make_user_info_dc_pac(TALLOC_CTX *mem_ctx,
			      struct PAC_LOGON_INFO *pac_logon_info,
			      struct auth_user_info_dc **_user_info_dc)
{
	uint32_t i;
	NTSTATUS nt_status;
	union netr_Validation validation;
	struct auth_user_info_dc *user_info_dc;

	validation.sam3 = &pac_logon_info->info3;

	nt_status = make_user_info_dc_netlogon_validation(mem_ctx, "", 3, &validation, &user_info_dc);
	if (!NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	}

	if (pac_logon_info->res_groups.count > 0) {
		size_t sidcount;
		/* The IDL layer would be a better place to check this, but to
		 * guard the integer addition below, we double-check */
		if (pac_logon_info->res_groups.count > 65535) {
			talloc_free(user_info_dc);
			return NT_STATUS_INVALID_PARAMETER;
		}

		/*
		  Here is where we should check the list of
		  trusted domains, and verify that the SID
		  matches.
		*/
		if (!pac_logon_info->res_group_dom_sid) {
			DEBUG(0, ("Cannot operate on a PAC without a resource domain SID"));
			return NT_STATUS_INVALID_PARAMETER;
		}

		sidcount = user_info_dc->num_sids + pac_logon_info->res_groups.count;
		user_info_dc->sids
			= talloc_realloc(user_info_dc, user_info_dc->sids, struct dom_sid, sidcount);
		NT_STATUS_HAVE_NO_MEMORY_AND_FREE(user_info_dc->sids, user_info_dc);

		for (i = 0; pac_logon_info->res_group_dom_sid && i < pac_logon_info->res_groups.count; i++) {
			user_info_dc->sids[user_info_dc->num_sids] = *pac_logon_info->res_group_dom_sid;
			if (!sid_append_rid(&user_info_dc->sids[user_info_dc->num_sids],
					    pac_logon_info->res_groups.rids[i].rid)) {
				return NT_STATUS_INVALID_PARAMETER;
			}
			user_info_dc->num_sids++;
		}
	}
	*_user_info_dc = user_info_dc;
	return NT_STATUS_OK;
}
