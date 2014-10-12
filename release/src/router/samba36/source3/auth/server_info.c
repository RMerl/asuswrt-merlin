/*
   Unix SMB/CIFS implementation.
   Authentication utility functions
   Copyright (C) Volker Lendecke 2010

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
#include "auth.h"
#include "../lib/crypto/arcfour.h"
#include "../librpc/gen_ndr/netlogon.h"
#include "../libcli/security/security.h"
#include "rpc_client/util_netlogon.h"
#include "nsswitch/libwbclient/wbclient.h"
#include "passdb.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_AUTH

/* FIXME: do we really still need this ? */
static int server_info_dtor(struct auth_serversupplied_info *server_info)
{
	TALLOC_FREE(server_info->info3);
	ZERO_STRUCTP(server_info);
	return 0;
}

/***************************************************************************
 Make a server_info struct. Free with TALLOC_FREE().
***************************************************************************/

struct auth_serversupplied_info *make_server_info(TALLOC_CTX *mem_ctx)
{
	struct auth_serversupplied_info *result;

	result = TALLOC_ZERO_P(mem_ctx, struct auth_serversupplied_info);
	if (result == NULL) {
		DEBUG(0, ("talloc failed\n"));
		return NULL;
	}

	talloc_set_destructor(result, server_info_dtor);

	/* Initialise the uid and gid values to something non-zero
	   which may save us from giving away root access if there
	   is a bug in allocating these fields. */

	result->utok.uid = -1;
	result->utok.gid = -1;

	return result;
}

/****************************************************************************
 inits a netr_SamInfo2 structure from an auth_serversupplied_info. sam2 must
 already be initialized and is used as the talloc parent for its members.
*****************************************************************************/

NTSTATUS serverinfo_to_SamInfo2(struct auth_serversupplied_info *server_info,
				uint8_t *pipe_session_key,
				size_t pipe_session_key_len,
				struct netr_SamInfo2 *sam2)
{
	struct netr_SamInfo3 *info3;

	info3 = copy_netr_SamInfo3(sam2, server_info->info3);
	if (!info3) {
		return NT_STATUS_NO_MEMORY;
	}

	if (server_info->user_session_key.length) {
		memcpy(info3->base.key.key,
		       server_info->user_session_key.data,
		       MIN(sizeof(info3->base.key.key),
			   server_info->user_session_key.length));
		if (pipe_session_key) {
			arcfour_crypt(info3->base.key.key,
				      pipe_session_key, 16);
		}
	}
	if (server_info->lm_session_key.length) {
		memcpy(info3->base.LMSessKey.key,
		       server_info->lm_session_key.data,
		       MIN(sizeof(info3->base.LMSessKey.key),
			   server_info->lm_session_key.length));
		if (pipe_session_key) {
			arcfour_crypt(info3->base.LMSessKey.key,
				      pipe_session_key, 8);
		}
	}

	sam2->base = info3->base;

	return NT_STATUS_OK;
}

/****************************************************************************
 inits a netr_SamInfo3 structure from an auth_serversupplied_info. sam3 must
 already be initialized and is used as the talloc parent for its members.
*****************************************************************************/

NTSTATUS serverinfo_to_SamInfo3(const struct auth_serversupplied_info *server_info,
				uint8_t *pipe_session_key,
				size_t pipe_session_key_len,
				struct netr_SamInfo3 *sam3)
{
	struct netr_SamInfo3 *info3;

	info3 = copy_netr_SamInfo3(sam3, server_info->info3);
	if (!info3) {
		return NT_STATUS_NO_MEMORY;
	}

	if (server_info->user_session_key.length) {
		memcpy(info3->base.key.key,
		       server_info->user_session_key.data,
		       MIN(sizeof(info3->base.key.key),
			   server_info->user_session_key.length));
		if (pipe_session_key) {
			arcfour_crypt(info3->base.key.key,
				      pipe_session_key, 16);
		}
	}
	if (server_info->lm_session_key.length) {
		memcpy(info3->base.LMSessKey.key,
		       server_info->lm_session_key.data,
		       MIN(sizeof(info3->base.LMSessKey.key),
			   server_info->lm_session_key.length));
		if (pipe_session_key) {
			arcfour_crypt(info3->base.LMSessKey.key,
				      pipe_session_key, 8);
		}
	}

	sam3->base = info3->base;

	sam3->sidcount		= 0;
	sam3->sids		= NULL;

	return NT_STATUS_OK;
}

/****************************************************************************
 inits a netr_SamInfo6 structure from an auth_serversupplied_info. sam6 must
 already be initialized and is used as the talloc parent for its members.
*****************************************************************************/

NTSTATUS serverinfo_to_SamInfo6(struct auth_serversupplied_info *server_info,
				uint8_t *pipe_session_key,
				size_t pipe_session_key_len,
				struct netr_SamInfo6 *sam6)
{
	struct pdb_domain_info *dominfo;
	struct netr_SamInfo3 *info3;

	if ((pdb_capabilities() & PDB_CAP_ADS) == 0) {
		DEBUG(10,("Not adding validation info level 6 "
			   "without ADS passdb backend\n"));
		return NT_STATUS_INVALID_INFO_CLASS;
	}

	dominfo = pdb_get_domain_info(sam6);
	if (dominfo == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	info3 = copy_netr_SamInfo3(sam6, server_info->info3);
	if (!info3) {
		return NT_STATUS_NO_MEMORY;
	}

	if (server_info->user_session_key.length) {
		memcpy(info3->base.key.key,
		       server_info->user_session_key.data,
		       MIN(sizeof(info3->base.key.key),
			   server_info->user_session_key.length));
		if (pipe_session_key) {
			arcfour_crypt(info3->base.key.key,
				      pipe_session_key, 16);
		}
	}
	if (server_info->lm_session_key.length) {
		memcpy(info3->base.LMSessKey.key,
		       server_info->lm_session_key.data,
		       MIN(sizeof(info3->base.LMSessKey.key),
			   server_info->lm_session_key.length));
		if (pipe_session_key) {
			arcfour_crypt(info3->base.LMSessKey.key,
				      pipe_session_key, 8);
		}
	}

	sam6->base = info3->base;

	sam6->sidcount		= 0;
	sam6->sids		= NULL;

	sam6->dns_domainname.string = talloc_strdup(sam6, dominfo->dns_domain);
	if (sam6->dns_domainname.string == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	sam6->principle.string	= talloc_asprintf(sam6, "%s@%s",
						  sam6->base.account_name.string,
						  sam6->dns_domainname.string);
	if (sam6->principle.string == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	return NT_STATUS_OK;
}

static NTSTATUS append_netr_SidAttr(TALLOC_CTX *mem_ctx,
				    struct netr_SidAttr **sids,
				    uint32_t *count,
				    const struct dom_sid2 *asid,
				    uint32_t attributes)
{
	uint32_t t = *count;

	*sids = talloc_realloc(mem_ctx, *sids, struct netr_SidAttr, t + 1);
	if (*sids == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	(*sids)[t].sid = dom_sid_dup(*sids, asid);
	if ((*sids)[t].sid == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	(*sids)[t].attributes = attributes;
	*count = t + 1;

	return NT_STATUS_OK;
}

/* Fills the samr_RidWithAttributeArray with the provided sids.
 * If it happens that we have additional groups that do not belong
 * to the domain, add their sids as extra sids */
static NTSTATUS group_sids_to_info3(struct netr_SamInfo3 *info3,
				    const struct dom_sid *sids,
				    size_t num_sids)
{
	uint32_t attributes = SE_GROUP_MANDATORY |
				SE_GROUP_ENABLED_BY_DEFAULT |
				SE_GROUP_ENABLED;
	struct samr_RidWithAttributeArray *groups;
	struct dom_sid *domain_sid;
	unsigned int i;
	NTSTATUS status;
	uint32_t rid;
	bool ok;

	domain_sid = info3->base.domain_sid;
	groups = &info3->base.groups;

	groups->rids = talloc_array(info3,
				    struct samr_RidWithAttribute, num_sids);
	if (!groups->rids) {
		return NT_STATUS_NO_MEMORY;
	}

	for (i = 0; i < num_sids; i++) {
		ok = sid_peek_check_rid(domain_sid, &sids[i], &rid);
		if (ok) {
			/* store domain group rid */
			groups->rids[groups->count].rid = rid;
			groups->rids[groups->count].attributes = attributes;
			groups->count++;
			continue;
		}

		/* if this wasn't a domain sid, add it as extra sid */
		status = append_netr_SidAttr(info3, &info3->sids,
					     &info3->sidcount,
					     &sids[i], attributes);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
	}

	return NT_STATUS_OK;
}

#define RET_NOMEM(ptr) do { \
	if (!ptr) { \
		TALLOC_FREE(info3); \
		return NT_STATUS_NO_MEMORY; \
	} } while(0)

NTSTATUS samu_to_SamInfo3(TALLOC_CTX *mem_ctx,
			  struct samu *samu,
			  const char *login_server,
			  struct netr_SamInfo3 **_info3,
			  struct extra_auth_info *extra)
{
	struct netr_SamInfo3 *info3;
	const struct dom_sid *user_sid;
	const struct dom_sid *group_sid;
	struct dom_sid domain_sid;
	struct dom_sid *group_sids;
	uint32_t num_group_sids = 0;
	const char *tmp;
	gid_t *gids;
	NTSTATUS status;
	bool ok;

	user_sid = pdb_get_user_sid(samu);
	group_sid = pdb_get_group_sid(samu);

	if (!user_sid || !group_sid) {
		DEBUG(1, ("Sam account is missing sids!\n"));
		return NT_STATUS_UNSUCCESSFUL;
	}

	info3 = talloc_zero(mem_ctx, struct netr_SamInfo3);
	if (!info3) {
		return NT_STATUS_NO_MEMORY;
	}

	ZERO_STRUCT(domain_sid);

	/* check if this is a "Unix Users" domain user,
	 * we need to handle it in a special way if that's the case */
	if (sid_check_is_in_unix_users(user_sid)) {
		/* in info3 you can only set rids for the user and the
		 * primary group, and the domain sid must be that of
		 * the sam domain.
		 *
		 * Store a completely bogus value here.
		 * The real SID is stored in the extra sids.
		 * Other code will know to look there if (-1) is found
		 */
		info3->base.rid = (uint32_t)(-1);
		sid_copy(&extra->user_sid, user_sid);

		DEBUG(10, ("Unix User found in struct samu. Rid marked as "
			   "special and sid (%s) saved as extra sid\n",
			   sid_string_dbg(user_sid)));
	} else {
		sid_copy(&domain_sid, user_sid);
		sid_split_rid(&domain_sid, &info3->base.rid);
	}

	if (is_null_sid(&domain_sid)) {
		sid_copy(&domain_sid, get_global_sam_sid());
	}

	/* check if this is a "Unix Groups" domain group,
	 * if so we need special handling */
	if (sid_check_is_in_unix_groups(group_sid)) {
		/* in info3 you can only set rids for the user and the
		 * primary group, and the domain sid must be that of
		 * the sam domain.
		 *
		 * Store a completely bogus value here.
		 * The real SID is stored in the extra sids.
		 * Other code will know to look there if (-1) is found
		 */
		info3->base.primary_gid = (uint32_t)(-1);
		sid_copy(&extra->pgid_sid, group_sid);

		DEBUG(10, ("Unix Group found in struct samu. Rid marked as "
			   "special and sid (%s) saved as extra sid\n",
			   sid_string_dbg(group_sid)));

	} else {
		ok = sid_peek_check_rid(&domain_sid, group_sid,
					&info3->base.primary_gid);
		if (!ok) {
			DEBUG(1, ("The primary group domain sid(%s) does not "
				  "match the domain sid(%s) for %s(%s)\n",
				  sid_string_dbg(group_sid),
				  sid_string_dbg(&domain_sid),
				  pdb_get_username(samu),
				  sid_string_dbg(user_sid)));
			TALLOC_FREE(info3);
			return NT_STATUS_UNSUCCESSFUL;
		}
	}

	unix_to_nt_time(&info3->base.last_logon, pdb_get_logon_time(samu));
	unix_to_nt_time(&info3->base.last_logoff, get_time_t_max());
	unix_to_nt_time(&info3->base.acct_expiry, get_time_t_max());
	unix_to_nt_time(&info3->base.last_password_change,
			pdb_get_pass_last_set_time(samu));
	unix_to_nt_time(&info3->base.allow_password_change,
			pdb_get_pass_can_change_time(samu));
	unix_to_nt_time(&info3->base.force_password_change,
			pdb_get_pass_must_change_time(samu));

	tmp = pdb_get_username(samu);
	if (tmp) {
		info3->base.account_name.string	= talloc_strdup(info3, tmp);
		RET_NOMEM(info3->base.account_name.string);
	}
	tmp = pdb_get_fullname(samu);
	if (tmp) {
		info3->base.full_name.string = talloc_strdup(info3, tmp);
		RET_NOMEM(info3->base.full_name.string);
	}
	tmp = pdb_get_logon_script(samu);
	if (tmp) {
		info3->base.logon_script.string = talloc_strdup(info3, tmp);
		RET_NOMEM(info3->base.logon_script.string);
	}
	tmp = pdb_get_profile_path(samu);
	if (tmp) {
		info3->base.profile_path.string	= talloc_strdup(info3, tmp);
		RET_NOMEM(info3->base.profile_path.string);
	}
	tmp = pdb_get_homedir(samu);
	if (tmp) {
		info3->base.home_directory.string = talloc_strdup(info3, tmp);
		RET_NOMEM(info3->base.home_directory.string);
	}
	tmp = pdb_get_dir_drive(samu);
	if (tmp) {
		info3->base.home_drive.string = talloc_strdup(info3, tmp);
		RET_NOMEM(info3->base.home_drive.string);
	}

	info3->base.logon_count	= pdb_get_logon_count(samu);
	info3->base.bad_password_count = pdb_get_bad_password_count(samu);

	info3->base.domain.string = talloc_strdup(info3,
						  pdb_get_domain(samu));
	RET_NOMEM(info3->base.domain.string);

	info3->base.domain_sid = dom_sid_dup(info3, &domain_sid);
	RET_NOMEM(info3->base.domain_sid);

	status = pdb_enum_group_memberships(mem_ctx, samu,
					    &group_sids, &gids,
					    &num_group_sids);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("Failed to get groups from sam account.\n"));
		TALLOC_FREE(info3);
		return status;
	}

	if (num_group_sids) {
		status = group_sids_to_info3(info3, group_sids, num_group_sids);
		if (!NT_STATUS_IS_OK(status)) {
			TALLOC_FREE(info3);
			return status;
		}
	}

	/* We don't need sids and gids after the conversion */
	TALLOC_FREE(group_sids);
	TALLOC_FREE(gids);
	num_group_sids = 0;

	/* FIXME: should we add other flags ? */
	info3->base.user_flags = NETLOGON_EXTRA_SIDS;

	if (login_server) {
		info3->base.logon_server.string = talloc_strdup(info3, login_server);
		RET_NOMEM(info3->base.logon_server.string);
	}

	info3->base.acct_flags = pdb_get_acct_ctrl(samu);

	*_info3 = info3;
	return NT_STATUS_OK;
}

#undef RET_NOMEM

#define RET_NOMEM(ptr) do { \
	if (!ptr) { \
		TALLOC_FREE(info3); \
		return NULL; \
	} } while(0)

struct netr_SamInfo3 *copy_netr_SamInfo3(TALLOC_CTX *mem_ctx,
					 struct netr_SamInfo3 *orig)
{
	struct netr_SamInfo3 *info3;
	unsigned int i;
	NTSTATUS status;

	info3 = talloc_zero(mem_ctx, struct netr_SamInfo3);
	if (!info3) return NULL;

	status = copy_netr_SamBaseInfo(info3, &orig->base, &info3->base);
	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(info3);
		return NULL;
	}

	if (orig->sidcount) {
		info3->sidcount = orig->sidcount;
		info3->sids = talloc_array(info3, struct netr_SidAttr,
					   orig->sidcount);
		RET_NOMEM(info3->sids);
		for (i = 0; i < orig->sidcount; i++) {
			info3->sids[i].sid = dom_sid_dup(info3->sids,
							    orig->sids[i].sid);
			RET_NOMEM(info3->sids[i].sid);
			info3->sids[i].attributes =
				orig->sids[i].attributes;
		}
	}

	return info3;
}

static NTSTATUS wbcsids_to_samr_RidWithAttributeArray(
				TALLOC_CTX *mem_ctx,
				struct samr_RidWithAttributeArray *groups,
				const struct dom_sid *domain_sid,
				const struct wbcSidWithAttr *sids,
				size_t num_sids)
{
	unsigned int i, j = 0;
	bool ok;

	groups->rids = talloc_array(mem_ctx,
				    struct samr_RidWithAttribute, num_sids);
	if (!groups->rids) {
		return NT_STATUS_NO_MEMORY;
	}

	/* a wbcDomainSid is the same as a dom_sid */
	for (i = 0; i < num_sids; i++) {
		ok = sid_peek_check_rid(domain_sid,
					(const struct dom_sid *)&sids[i].sid,
					&groups->rids[j].rid);
		if (!ok) continue;

		groups->rids[j].attributes = SE_GROUP_MANDATORY |
					     SE_GROUP_ENABLED_BY_DEFAULT |
					     SE_GROUP_ENABLED;
		j++;
	}

	groups->count = j;
	return NT_STATUS_OK;
}

static NTSTATUS wbcsids_to_netr_SidAttrArray(
				const struct dom_sid *domain_sid,
				const struct wbcSidWithAttr *sids,
				size_t num_sids,
				TALLOC_CTX *mem_ctx,
				struct netr_SidAttr **_info3_sids,
				uint32_t *info3_num_sids)
{
	unsigned int i, j = 0;
	struct netr_SidAttr *info3_sids;

	info3_sids = talloc_array(mem_ctx, struct netr_SidAttr, num_sids);
	if (info3_sids == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	/* a wbcDomainSid is the same as a dom_sid */
	for (i = 0; i < num_sids; i++) {
		const struct dom_sid *sid;

		sid = (const struct dom_sid *)&sids[i].sid;

		if (dom_sid_in_domain(domain_sid, sid)) {
			continue;
		}

		info3_sids[j].sid = dom_sid_dup(info3_sids, sid);
		if (info3_sids[j].sid == NULL) {
			talloc_free(info3_sids);
			return NT_STATUS_NO_MEMORY;
		}
		info3_sids[j].attributes = SE_GROUP_MANDATORY |
					   SE_GROUP_ENABLED_BY_DEFAULT |
					   SE_GROUP_ENABLED;
		j++;
	}

	*info3_num_sids = j;
	*_info3_sids = info3_sids;
	return NT_STATUS_OK;
}

struct netr_SamInfo3 *wbcAuthUserInfo_to_netr_SamInfo3(TALLOC_CTX *mem_ctx,
					const struct wbcAuthUserInfo *info)
{
	struct netr_SamInfo3 *info3;
	struct dom_sid user_sid;
	struct dom_sid group_sid;
	struct dom_sid domain_sid;
	NTSTATUS status;
	bool ok;

	memcpy(&user_sid, &info->sids[0].sid, sizeof(user_sid));
	memcpy(&group_sid, &info->sids[1].sid, sizeof(group_sid));

	info3 = talloc_zero(mem_ctx, struct netr_SamInfo3);
	if (!info3) return NULL;

	unix_to_nt_time(&info3->base.last_logon, info->logon_time);
	unix_to_nt_time(&info3->base.last_logoff, info->logoff_time);
	unix_to_nt_time(&info3->base.acct_expiry, info->kickoff_time);
	unix_to_nt_time(&info3->base.last_password_change, info->pass_last_set_time);
	unix_to_nt_time(&info3->base.allow_password_change,
			info->pass_can_change_time);
	unix_to_nt_time(&info3->base.force_password_change,
			info->pass_must_change_time);

	if (info->account_name) {
		info3->base.account_name.string	=
				talloc_strdup(info3, info->account_name);
		RET_NOMEM(info3->base.account_name.string);
	}
	if (info->full_name) {
		info3->base.full_name.string =
				talloc_strdup(info3, info->full_name);
		RET_NOMEM(info3->base.full_name.string);
	}
	if (info->logon_script) {
		info3->base.logon_script.string =
				talloc_strdup(info3, info->logon_script);
		RET_NOMEM(info3->base.logon_script.string);
	}
	if (info->profile_path) {
		info3->base.profile_path.string	=
				talloc_strdup(info3, info->profile_path);
		RET_NOMEM(info3->base.profile_path.string);
	}
	if (info->home_directory) {
		info3->base.home_directory.string =
				talloc_strdup(info3, info->home_directory);
		RET_NOMEM(info3->base.home_directory.string);
	}
	if (info->home_drive) {
		info3->base.home_drive.string =
				talloc_strdup(info3, info->home_drive);
		RET_NOMEM(info3->base.home_drive.string);
	}

	info3->base.logon_count	= info->logon_count;
	info3->base.bad_password_count = info->bad_password_count;

	sid_copy(&domain_sid, &user_sid);
	sid_split_rid(&domain_sid, &info3->base.rid);

	ok = sid_peek_check_rid(&domain_sid, &group_sid,
				&info3->base.primary_gid);
	if (!ok) {
		DEBUG(1, ("The primary group sid domain does not"
			  "match user sid domain for user: %s\n",
			  info->account_name));
		TALLOC_FREE(info3);
		return NULL;
	}

	status = wbcsids_to_samr_RidWithAttributeArray(info3,
						       &info3->base.groups,
						       &domain_sid,
						       &info->sids[1],
						       info->num_sids - 1);
	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(info3);
		return NULL;
	}

	status = wbcsids_to_netr_SidAttrArray(&domain_sid,
					      &info->sids[1],
					      info->num_sids - 1,
					      info3,
					      &info3->sids,
					      &info3->sidcount);
	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(info3);
		return NULL;
	}

	info3->base.user_flags = info->user_flags;
	memcpy(info3->base.key.key, info->user_session_key, 16);

	if (info->logon_server) {
		info3->base.logon_server.string =
				talloc_strdup(info3, info->logon_server);
		RET_NOMEM(info3->base.logon_server.string);
	}
	if (info->domain_name) {
		info3->base.domain.string =
				talloc_strdup(info3, info->domain_name);
		RET_NOMEM(info3->base.domain.string);
	}

	info3->base.domain_sid = dom_sid_dup(info3, &domain_sid);
	RET_NOMEM(info3->base.domain_sid);

	memcpy(info3->base.LMSessKey.key, info->lm_session_key, 8);
	info3->base.acct_flags = info->acct_flags;

	return info3;
}
