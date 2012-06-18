/* 
   Unix SMB/CIFS implementation.
   Password and authentication handling
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2001-2004
   Copyright (C) Gerald Carter                             2003
   Copyright (C) Stefan Metzmacher                         2005
   Copyright (C) Matthias Dieter Wallnöfer                 2009
   
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
#include "system/time.h"
#include "auth/auth.h"
#include <ldb.h>
#include "../lib/util/util_ldb.h"
#include "dsdb/samdb/samdb.h"
#include "libcli/security/security.h"
#include "libcli/ldap/ldap.h"
#include "../libcli/ldap/ldap_ndr.h"
#include "librpc/gen_ndr/ndr_netlogon.h"
#include "librpc/gen_ndr/ndr_security.h"
#include "param/param.h"
#include "auth/auth_sam.h"

#define KRBTGT_ATTRS \
	/* required for the krb5 kdc */		\
	"objectClass",				\
	"sAMAccountName",			\
	"userPrincipalName",			\
	"servicePrincipalName",			\
	"msDS-KeyVersionNumber",		\
	"supplementalCredentials",		\
						\
	/* passwords */				\
	"dBCSPwd",				\
	"unicodePwd",				\
						\
	"userAccountControl",			\
	"objectSid",				\
						\
	"pwdLastSet",				\
	"accountExpires"			

const char *krbtgt_attrs[] = {
	KRBTGT_ATTRS
};

const char *server_attrs[] = {
	KRBTGT_ATTRS
};

const char *user_attrs[] = {
	KRBTGT_ATTRS,

	"logonHours",

	/* check 'allowed workstations' */
	"userWorkstations",
		       
	/* required for server_info, not access control: */
	"displayName",
	"scriptPath",
	"profilePath",
	"homeDirectory",
	"homeDrive",
	"lastLogon",
	"lastLogoff",
	"accountExpires",
	"badPwdCount",
	"logonCount",
	"primaryGroupID",
	"memberOf",
	NULL,
};

/****************************************************************************
 Check if a user is allowed to logon at this time. Note this is the
 servers local time, as logon hours are just specified as a weekly
 bitmask.
****************************************************************************/
                                                                                                              
static bool logon_hours_ok(struct ldb_message *msg, const char *name_for_logs)
{
	/* In logon hours first bit is Sunday from 12AM to 1AM */
	const struct ldb_val *hours;
	struct tm *utctime;
	time_t lasttime;
	const char *asct;
	uint8_t bitmask, bitpos;

	hours = ldb_msg_find_ldb_val(msg, "logonHours");
	if (!hours) {
		DEBUG(5,("logon_hours_ok: No hours restrictions for user %s\n", name_for_logs));
		return true;
	}

	if (hours->length != 168/8) {
		DEBUG(5,("logon_hours_ok: malformed logon hours restrictions for user %s\n", name_for_logs));
		return true;		
	}

	lasttime = time(NULL);
	utctime = gmtime(&lasttime);
	if (!utctime) {
		DEBUG(1, ("logon_hours_ok: failed to get gmtime. Failing logon for user %s\n",
			name_for_logs));
		return false;
	}

	/* find the corresponding byte and bit */
	bitpos = (utctime->tm_wday * 24 + utctime->tm_hour) % 168;
	bitmask = 1 << (bitpos % 8);

	if (! (hours->data[bitpos/8] & bitmask)) {
		struct tm *t = localtime(&lasttime);
		if (!t) {
			asct = "INVALID TIME";
		} else {
			asct = asctime(t);
			if (!asct) {
				asct = "INVALID TIME";
			}
		}
		
		DEBUG(1, ("logon_hours_ok: Account for user %s not allowed to "
			  "logon at this time (%s).\n",
			  name_for_logs, asct ));
		return false;
	}

	asct = asctime(utctime);
	DEBUG(5,("logon_hours_ok: user %s allowed to logon at this time (%s)\n",
		name_for_logs, asct ? asct : "UNKNOWN TIME" ));

	return true;
}

/****************************************************************************
 Do a specific test for a SAM_ACCOUNT being vaild for this connection 
 (ie not disabled, expired and the like).
****************************************************************************/
_PUBLIC_ NTSTATUS authsam_account_ok(TALLOC_CTX *mem_ctx,
				     struct ldb_context *sam_ctx,
				     uint32_t logon_parameters,
				     struct ldb_dn *domain_dn,
				     struct ldb_message *msg,
				     const char *logon_workstation,
				     const char *name_for_logs,
				     bool allow_domain_trust,
				     bool password_change)
{
	uint16_t acct_flags;
	const char *workstation_list;
	NTTIME acct_expiry;
	NTTIME must_change_time;

	NTTIME now;
	DEBUG(4,("authsam_account_ok: Checking SMB password for user %s\n", name_for_logs));

	acct_flags = samdb_result_acct_flags(sam_ctx, mem_ctx, msg, domain_dn);
	
	acct_expiry = samdb_result_account_expires(msg);

	/* Check for when we must change this password, taking the
	 * userAccountControl flags into account */
	must_change_time = samdb_result_force_password_change(sam_ctx, mem_ctx, 
							      domain_dn, msg);

	workstation_list = samdb_result_string(msg, "userWorkstations", NULL);

	/* Quit if the account was disabled. */
	if (acct_flags & ACB_DISABLED) {
		DEBUG(1,("authsam_account_ok: Account for user '%s' was disabled.\n", name_for_logs));
		return NT_STATUS_ACCOUNT_DISABLED;
	}

	/* Quit if the account was locked out. */
	if (acct_flags & ACB_AUTOLOCK) {
		DEBUG(1,("authsam_account_ok: Account for user %s was locked out.\n", name_for_logs));
		return NT_STATUS_ACCOUNT_LOCKED_OUT;
	}

	/* Test account expire time */
	unix_to_nt_time(&now, time(NULL));
	if (now > acct_expiry) {
		DEBUG(1,("authsam_account_ok: Account for user '%s' has expired.\n", name_for_logs));
		DEBUG(3,("authsam_account_ok: Account expired at '%s'.\n", 
			 nt_time_string(mem_ctx, acct_expiry)));
		return NT_STATUS_ACCOUNT_EXPIRED;
	}

	/* check for immediate expiry "must change at next logon" (but not if this is a password change request) */
	if ((must_change_time == 0) && !password_change) {
		DEBUG(1,("sam_account_ok: Account for user '%s' password must change!.\n", 
			 name_for_logs));
		return NT_STATUS_PASSWORD_MUST_CHANGE;
	}

	/* check for expired password (but not if this is a password change request) */
	if ((must_change_time < now) && !password_change) {
		DEBUG(1,("sam_account_ok: Account for user '%s' password expired!.\n", 
			 name_for_logs));
		DEBUG(1,("sam_account_ok: Password expired at '%s' unix time.\n", 
			 nt_time_string(mem_ctx, must_change_time)));
		return NT_STATUS_PASSWORD_EXPIRED;
	}

	/* Test workstation. Workstation list is comma separated. */
	if (logon_workstation && workstation_list && *workstation_list) {
		bool invalid_ws = true;
		int i;
		const char **workstations = (const char **)str_list_make(mem_ctx, workstation_list, ",");
		
		for (i = 0; workstations && workstations[i]; i++) {
			DEBUG(10,("sam_account_ok: checking for workstation match '%s' and '%s'\n",
				  workstations[i], logon_workstation));

			if (strequal(workstations[i], logon_workstation)) {
				invalid_ws = false;
				break;
			}
		}

		talloc_free(workstations);

		if (invalid_ws) {
			return NT_STATUS_INVALID_WORKSTATION;
		}
	}
	
	if (!logon_hours_ok(msg, name_for_logs)) {
		return NT_STATUS_INVALID_LOGON_HOURS;
	}
	
	if (!allow_domain_trust) {
		if (acct_flags & ACB_DOMTRUST) {
			DEBUG(2,("sam_account_ok: Domain trust account %s denied by server\n", name_for_logs));
			return NT_STATUS_NOLOGON_INTERDOMAIN_TRUST_ACCOUNT;
		}
	}
	if (!(logon_parameters & MSV1_0_ALLOW_SERVER_TRUST_ACCOUNT)) {
		if (acct_flags & ACB_SVRTRUST) {
			DEBUG(2,("sam_account_ok: Server trust account %s denied by server\n", name_for_logs));
			return NT_STATUS_NOLOGON_SERVER_TRUST_ACCOUNT;
		}
	}
	if (!(logon_parameters & MSV1_0_ALLOW_WORKSTATION_TRUST_ACCOUNT)) {
		/* TODO: this fails with current solaris client. We
		   need to work with Gordon to work out why */
		if (acct_flags & ACB_WSTRUST) {
			DEBUG(4,("sam_account_ok: Wksta trust account %s denied by server\n", name_for_logs));
			return NT_STATUS_NOLOGON_WORKSTATION_TRUST_ACCOUNT;
		}
	}

	return NT_STATUS_OK;
}

/* This function tests if a SID structure "sids" contains the SID "sid" */
static bool sids_contains_sid(const struct dom_sid **sids, const int num_sids,
	const struct dom_sid *sid)
{
	int i;

	for (i = 0; i < num_sids; i++) {
		if (dom_sid_equal(sids[i], sid))
			return true;
	}
	return false;
}

/*
 * This function generates the transitive closure of a given SID "sid" (it
 * basically expands nested groups of a SID).
 * If the SID isn't located in the "res_sids" structure yet and the
 * "only_childs" flag is negative, we add it to "res_sids".
 * Then we've always to consider the "memberOf" attributes. We invoke the
 * function recursively on each item of it with the "only_childs" flag set to
 * "false".
 * The "only_childs" flag is particularly useful if you have a user SID and
 * want to include all his groups (referenced with "memberOf") without his SID
 * itself.
 *
 * At the beginning "res_sids" should reference to a NULL pointer.
 */
static NTSTATUS authsam_expand_nested_groups(struct ldb_context *sam_ctx,
	const struct dom_sid *sid, const bool only_childs,
	TALLOC_CTX *res_sids_ctx, struct dom_sid ***res_sids,
	int *num_res_sids)
{
	const char * const attrs[] = { "memberOf", NULL };
	int i, ret;
	bool already_there;
	struct ldb_dn *tmp_dn;
	struct dom_sid *tmp_sid;
	TALLOC_CTX *tmp_ctx;
	struct ldb_message **res;
	NTSTATUS status;

	if (*res_sids == NULL) {
		*num_res_sids = 0;
	}

	if (sid == NULL) {
		return NT_STATUS_OK;
	}

	already_there = sids_contains_sid((const struct dom_sid**) *res_sids,
		*num_res_sids, sid);
	if (already_there) {
		return NT_STATUS_OK;
	}

	if (!only_childs) {
		tmp_sid = dom_sid_dup(res_sids_ctx, sid);
		NT_STATUS_HAVE_NO_MEMORY(tmp_sid);
		*res_sids = talloc_realloc(res_sids_ctx, *res_sids,
			struct dom_sid *, *num_res_sids + 1);
		NT_STATUS_HAVE_NO_MEMORY(*res_sids);
		(*res_sids)[*num_res_sids] = tmp_sid;
		++(*num_res_sids);
	}

	tmp_ctx = talloc_new(sam_ctx);

	ret = gendb_search(sam_ctx, tmp_ctx, NULL, &res, attrs,
		"objectSid=%s", ldap_encode_ndr_dom_sid(tmp_ctx, sid));
	if (ret != 1) {
		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	if (res[0]->num_elements == 0) {
		talloc_free(res);
		talloc_free(tmp_ctx);
		return NT_STATUS_OK;
	}

	for (i = 0; i < res[0]->elements[0].num_values; i++) {
		tmp_dn = ldb_dn_from_ldb_val(tmp_ctx, sam_ctx,
			&res[0]->elements[0].values[i]);
		tmp_sid = samdb_search_dom_sid(sam_ctx, tmp_ctx, tmp_dn,
			"objectSid", NULL);

		status = authsam_expand_nested_groups(sam_ctx, tmp_sid,
			false, res_sids_ctx, res_sids, num_res_sids);
		if (!NT_STATUS_IS_OK(status)) {
			talloc_free(res);
			talloc_free(tmp_ctx);
			return status;
		}
	}

	talloc_free(res);
	talloc_free(tmp_ctx);

	return NT_STATUS_OK;
}

_PUBLIC_ NTSTATUS authsam_make_server_info(TALLOC_CTX *mem_ctx,
					   struct ldb_context *sam_ctx,
					   const char *netbios_name,
					   const char *domain_name,
					   struct ldb_dn *domain_dn, 
					   struct ldb_message *msg,
					   DATA_BLOB user_sess_key,
					   DATA_BLOB lm_sess_key,
					   struct auth_serversupplied_info
						   **_server_info)
{
	NTSTATUS status;
	struct auth_serversupplied_info *server_info;
	const char *str;
	struct dom_sid *tmp_sid;
	/* SIDs for the account and his primary group */
	struct dom_sid *account_sid;
	struct dom_sid *primary_group_sid;
	/* SID structures for the expanded group memberships */
	struct dom_sid **groupSIDs = NULL, **groupSIDs_2 = NULL;
	int num_groupSIDs = 0, num_groupSIDs_2 = 0, i;
	uint32_t userAccountControl;

	server_info = talloc(mem_ctx, struct auth_serversupplied_info);
	NT_STATUS_HAVE_NO_MEMORY(server_info);

	account_sid = samdb_result_dom_sid(server_info, msg, "objectSid");
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(account_sid, server_info);

	primary_group_sid = dom_sid_add_rid(server_info,
		samdb_domain_sid(sam_ctx),
		samdb_result_uint(msg, "primaryGroupID", ~0));
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(primary_group_sid, server_info);

	/* Expands the primary group */
	status = authsam_expand_nested_groups(sam_ctx, primary_group_sid, false,
					      server_info, &groupSIDs, &num_groupSIDs);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(server_info);
		return status;
	}

	/* Expands the additional groups */
	status = authsam_expand_nested_groups(sam_ctx, account_sid, true,
		server_info, &groupSIDs_2, &num_groupSIDs_2);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(server_info);
		return status;
	}

	/* Merge the two expanded structures (groupSIDs, groupSIDs_2) */
	for (i = 0; i < num_groupSIDs_2; i++)
		if (!sids_contains_sid((const struct dom_sid **) groupSIDs,
				num_groupSIDs, groupSIDs_2[i])) {
			tmp_sid = dom_sid_dup(server_info, groupSIDs_2[i]);
			NT_STATUS_HAVE_NO_MEMORY_AND_FREE(tmp_sid, server_info);
			groupSIDs = talloc_realloc(server_info, groupSIDs,
				struct dom_sid *, num_groupSIDs + 1);
			NT_STATUS_HAVE_NO_MEMORY_AND_FREE(groupSIDs,
				server_info);
			groupSIDs[num_groupSIDs] = tmp_sid;
			++num_groupSIDs;
		}
	talloc_free(groupSIDs_2);

	server_info->account_sid = account_sid;
	server_info->primary_group_sid = primary_group_sid;
	
	/* DCs also get SID_NT_ENTERPRISE_DCS */
	userAccountControl = ldb_msg_find_attr_as_uint(msg, "userAccountControl", 0);
	if (userAccountControl & UF_SERVER_TRUST_ACCOUNT) {
		groupSIDs = talloc_realloc(server_info, groupSIDs, struct dom_sid *,
					   num_groupSIDs+1);
		NT_STATUS_HAVE_NO_MEMORY_AND_FREE(groupSIDs, server_info);
		groupSIDs[num_groupSIDs] = dom_sid_parse_talloc(groupSIDs, SID_NT_ENTERPRISE_DCS);
		NT_STATUS_HAVE_NO_MEMORY_AND_FREE(groupSIDs[num_groupSIDs], server_info);
		num_groupSIDs++;
	}

	server_info->domain_groups = groupSIDs;
	server_info->n_domain_groups = num_groupSIDs;

	server_info->account_name = talloc_steal(server_info,
		samdb_result_string(msg, "sAMAccountName", NULL));

	server_info->domain_name = talloc_strdup(server_info, domain_name);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(server_info->domain_name,
		server_info);

	str = samdb_result_string(msg, "displayName", "");
	server_info->full_name = talloc_strdup(server_info, str);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(server_info->full_name, server_info);

	str = samdb_result_string(msg, "scriptPath", "");
	server_info->logon_script = talloc_strdup(server_info, str);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(server_info->logon_script,
		server_info);

	str = samdb_result_string(msg, "profilePath", "");
	server_info->profile_path = talloc_strdup(server_info, str);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(server_info->profile_path,
		server_info);

	str = samdb_result_string(msg, "homeDirectory", "");
	server_info->home_directory = talloc_strdup(server_info, str);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(server_info->home_directory,
		server_info);

	str = samdb_result_string(msg, "homeDrive", "");
	server_info->home_drive = talloc_strdup(server_info, str);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(server_info->home_drive, server_info);

	server_info->logon_server = talloc_strdup(server_info, netbios_name);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(server_info->logon_server,
		server_info);

	server_info->last_logon = samdb_result_nttime(msg, "lastLogon", 0);
	server_info->last_logoff = samdb_result_last_logoff(msg);
	server_info->acct_expiry = samdb_result_account_expires(msg);
	server_info->last_password_change = samdb_result_nttime(msg,
		"pwdLastSet", 0);
	server_info->allow_password_change
		= samdb_result_allow_password_change(sam_ctx, mem_ctx, 
			domain_dn, msg, "pwdLastSet");
	server_info->force_password_change
		= samdb_result_force_password_change(sam_ctx, mem_ctx,
			domain_dn, msg);
	server_info->logon_count = samdb_result_uint(msg, "logonCount", 0);
	server_info->bad_password_count = samdb_result_uint(msg, "badPwdCount",
		0);

	server_info->acct_flags = samdb_result_acct_flags(sam_ctx, mem_ctx, 
							  msg, domain_dn);

	server_info->user_session_key = data_blob_talloc_reference(server_info,
		&user_sess_key);
	server_info->lm_session_key = data_blob_talloc_reference(server_info,
		&lm_sess_key);

	server_info->authenticated = true;

	*_server_info = server_info;

	return NT_STATUS_OK;
}

NTSTATUS sam_get_results_principal(struct ldb_context *sam_ctx,
				   TALLOC_CTX *mem_ctx, const char *principal,
				   const char **attrs,
				   struct ldb_dn **domain_dn,
				   struct ldb_message **msg)
{			   
	struct ldb_dn *user_dn;
	NTSTATUS nt_status;
	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
	int ret;

	if (!tmp_ctx) {
		return NT_STATUS_NO_MEMORY;
	}

	nt_status = crack_user_principal_name(sam_ctx, tmp_ctx, principal, 
					      &user_dn, domain_dn);
	if (!NT_STATUS_IS_OK(nt_status)) {
		talloc_free(tmp_ctx);
		return nt_status;
	}
	
	/* pull the user attributes */
	ret = gendb_search_single_extended_dn(sam_ctx, tmp_ctx, user_dn,
		LDB_SCOPE_BASE, msg, attrs, "(objectClass=*)");
	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}
	talloc_steal(mem_ctx, *msg);
	talloc_steal(mem_ctx, *domain_dn);
	talloc_free(tmp_ctx);
	
	return NT_STATUS_OK;
}
