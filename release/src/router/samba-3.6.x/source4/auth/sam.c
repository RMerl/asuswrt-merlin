/* 
   Unix SMB/CIFS implementation.
   Password and authentication handling
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2001-2010
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
#include "dsdb/samdb/samdb.h"
#include "libcli/security/security.h"
#include "auth/auth_sam.h"
#include "dsdb/common/util.h"
#include "libcli/ldap/ldap_ndr.h"
#include "param/param.h"

#define KRBTGT_ATTRS \
	/* required for the krb5 kdc */		\
	"objectClass",				\
	"sAMAccountName",			\
	"userPrincipalName",			\
	"servicePrincipalName",			\
	"msDS-KeyVersionNumber",		\
	"msDS-SecondaryKrbTgtNumber",		\
	"msDS-SupportedEncryptionTypes",	\
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
	KRBTGT_ATTRS, NULL
};

const char *server_attrs[] = {
	KRBTGT_ATTRS, NULL
};

const char *user_attrs[] = {
	KRBTGT_ATTRS,

	"logonHours",

	/* check 'allowed workstations' */
	"userWorkstations",
		       
	/* required for user_info_dc, not access control: */
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
 Do a specific test for a SAM_ACCOUNT being valid for this connection
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

	workstation_list = ldb_msg_find_attr_as_string(msg, "userWorkstations", NULL);

	/* Quit if the account was disabled. */
	if (acct_flags & ACB_DISABLED) {
		DEBUG(2,("authsam_account_ok: Account for user '%s' was disabled.\n", name_for_logs));
		return NT_STATUS_ACCOUNT_DISABLED;
	}

	/* Quit if the account was locked out. */
	if (acct_flags & ACB_AUTOLOCK) {
		DEBUG(2,("authsam_account_ok: Account for user %s was locked out.\n", name_for_logs));
		return NT_STATUS_ACCOUNT_LOCKED_OUT;
	}

	/* Test account expire time */
	unix_to_nt_time(&now, time(NULL));
	if (now > acct_expiry) {
		DEBUG(2,("authsam_account_ok: Account for user '%s' has expired.\n", name_for_logs));
		DEBUG(3,("authsam_account_ok: Account expired at '%s'.\n", 
			 nt_time_string(mem_ctx, acct_expiry)));
		return NT_STATUS_ACCOUNT_EXPIRED;
	}

	/* check for immediate expiry "must change at next logon" (but not if this is a password change request) */
	if ((must_change_time == 0) && !password_change) {
		DEBUG(2,("sam_account_ok: Account for user '%s' password must change!.\n",
			 name_for_logs));
		return NT_STATUS_PASSWORD_MUST_CHANGE;
	}

	/* check for expired password (but not if this is a password change request) */
	if ((must_change_time < now) && !password_change) {
		DEBUG(2,("sam_account_ok: Account for user '%s' password expired!.\n",
			 name_for_logs));
		DEBUG(2,("sam_account_ok: Password expired at '%s' unix time.\n",
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

_PUBLIC_ NTSTATUS authsam_make_user_info_dc(TALLOC_CTX *mem_ctx,
					   struct ldb_context *sam_ctx,
					   const char *netbios_name,
					   const char *domain_name,
					   struct ldb_dn *domain_dn, 
					   struct ldb_message *msg,
					   DATA_BLOB user_sess_key,
					   DATA_BLOB lm_sess_key,
					   struct auth_user_info_dc **_user_info_dc)
{
	NTSTATUS status;
	struct auth_user_info_dc *user_info_dc;
	struct auth_user_info *info;
	const char *str, *filter;
	/* SIDs for the account and his primary group */
	struct dom_sid *account_sid;
	const char *primary_group_string;
	const char *primary_group_dn;
	DATA_BLOB primary_group_blob;
	/* SID structures for the expanded group memberships */
	struct dom_sid *sids = NULL;
	unsigned int num_sids = 0, i;
	struct dom_sid *domain_sid;
	TALLOC_CTX *tmp_ctx;
	struct ldb_message_element *el;

	user_info_dc = talloc(mem_ctx, struct auth_user_info_dc);
	NT_STATUS_HAVE_NO_MEMORY(user_info_dc);

	tmp_ctx = talloc_new(user_info_dc);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(user_info_dc, user_info_dc);

	sids = talloc_array(user_info_dc, struct dom_sid, 2);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(sids, user_info_dc);

	num_sids = 2;

	account_sid = samdb_result_dom_sid(user_info_dc, msg, "objectSid");
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(account_sid, user_info_dc);

	status = dom_sid_split_rid(tmp_ctx, account_sid, &domain_sid, NULL);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(user_info_dc);
		return status;
	}

	sids[PRIMARY_USER_SID_INDEX] = *account_sid;
	sids[PRIMARY_GROUP_SID_INDEX] = *domain_sid;
	sid_append_rid(&sids[PRIMARY_GROUP_SID_INDEX], ldb_msg_find_attr_as_uint(msg, "primaryGroupID", ~0));

	/* Filter out builtin groups from this token.  We will search
	 * for builtin groups later, and not include them in the PAC
	 * on SamLogon validation info */
	filter = talloc_asprintf(tmp_ctx, "(&(objectClass=group)(!(groupType:1.2.840.113556.1.4.803:=%u))(groupType:1.2.840.113556.1.4.803:=%u))", GROUP_TYPE_BUILTIN_LOCAL_GROUP, GROUP_TYPE_SECURITY_ENABLED);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(filter, user_info_dc);

	primary_group_string = dom_sid_string(tmp_ctx, &sids[PRIMARY_GROUP_SID_INDEX]);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(primary_group_string, user_info_dc);

	primary_group_dn = talloc_asprintf(tmp_ctx, "<SID=%s>", primary_group_string);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(primary_group_dn, user_info_dc);

	primary_group_blob = data_blob_string_const(primary_group_dn);

	/* Expands the primary group - this function takes in
	 * memberOf-like values, so we fake one up with the
	 * <SID=S-...> format of DN and then let it expand
	 * them, as long as they meet the filter - so only
	 * domain groups, not builtin groups
	 *
	 * The primary group is still treated specially, so we set the
	 * 'only childs' flag to true
	 */
	status = dsdb_expand_nested_groups(sam_ctx, &primary_group_blob, true, filter,
					   user_info_dc, &sids, &num_sids);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(user_info_dc);
		return status;
	}

	/* Expands the additional groups */
	el = ldb_msg_find_element(msg, "memberOf");
	for (i = 0; el && i < el->num_values; i++) {
		/* This function takes in memberOf values and expands
		 * them, as long as they meet the filter - so only
		 * domain groups, not builtin groups */
		status = dsdb_expand_nested_groups(sam_ctx, &el->values[i], false, filter,
						   user_info_dc, &sids, &num_sids);
		if (!NT_STATUS_IS_OK(status)) {
			talloc_free(user_info_dc);
			return status;
		}
	}

	user_info_dc->sids = sids;
	user_info_dc->num_sids = num_sids;

	user_info_dc->info = info = talloc_zero(user_info_dc, struct auth_user_info);
	NT_STATUS_HAVE_NO_MEMORY(user_info_dc->info);

	info->account_name = talloc_steal(info,
		ldb_msg_find_attr_as_string(msg, "sAMAccountName", NULL));

	info->domain_name = talloc_strdup(info, domain_name);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(info->domain_name,
		user_info_dc);

	str = ldb_msg_find_attr_as_string(msg, "displayName", "");
	info->full_name = talloc_strdup(info, str);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(info->full_name, user_info_dc);

	str = ldb_msg_find_attr_as_string(msg, "scriptPath", "");
	info->logon_script = talloc_strdup(info, str);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(info->logon_script,
		user_info_dc);

	str = ldb_msg_find_attr_as_string(msg, "profilePath", "");
	info->profile_path = talloc_strdup(info, str);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(info->profile_path,
		user_info_dc);

	str = ldb_msg_find_attr_as_string(msg, "homeDirectory", "");
	info->home_directory = talloc_strdup(info, str);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(info->home_directory,
		user_info_dc);

	str = ldb_msg_find_attr_as_string(msg, "homeDrive", "");
	info->home_drive = talloc_strdup(info, str);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(info->home_drive, user_info_dc);

	info->logon_server = talloc_strdup(info, netbios_name);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(info->logon_server,
		user_info_dc);

	info->last_logon = samdb_result_nttime(msg, "lastLogon", 0);
	info->last_logoff = samdb_result_last_logoff(msg);
	info->acct_expiry = samdb_result_account_expires(msg);
	info->last_password_change = samdb_result_nttime(msg,
		"pwdLastSet", 0);
	info->allow_password_change
		= samdb_result_allow_password_change(sam_ctx, mem_ctx, 
			domain_dn, msg, "pwdLastSet");
	info->force_password_change
		= samdb_result_force_password_change(sam_ctx, mem_ctx,
			domain_dn, msg);
	info->logon_count = ldb_msg_find_attr_as_uint(msg, "logonCount", 0);
	info->bad_password_count = ldb_msg_find_attr_as_uint(msg, "badPwdCount",
		0);

	info->acct_flags = samdb_result_acct_flags(sam_ctx, mem_ctx,
							  msg, domain_dn);

	user_info_dc->user_session_key = data_blob_talloc(user_info_dc,
							 user_sess_key.data,
							 user_sess_key.length);
	if (user_sess_key.data) {
		NT_STATUS_HAVE_NO_MEMORY_AND_FREE(user_info_dc->user_session_key.data,
						  user_info_dc);
	}
	user_info_dc->lm_session_key = data_blob_talloc(user_info_dc,
						       lm_sess_key.data,
						       lm_sess_key.length);
	if (lm_sess_key.data) {
		NT_STATUS_HAVE_NO_MEMORY_AND_FREE(user_info_dc->lm_session_key.data,
						  user_info_dc);
	}

	if (info->acct_flags & ACB_SVRTRUST) {
		/* the SID_NT_ENTERPRISE_DCS SID gets added into the
		   PAC */
		user_info_dc->sids = talloc_realloc(user_info_dc,
						   user_info_dc->sids,
						   struct dom_sid,
						   user_info_dc->num_sids+1);
		NT_STATUS_HAVE_NO_MEMORY_AND_FREE(user_info_dc->sids, user_info_dc);
		user_info_dc->sids[user_info_dc->num_sids] = global_sid_Enterprise_DCs;
		user_info_dc->num_sids++;
	}

	if ((info->acct_flags & (ACB_PARTIAL_SECRETS_ACCOUNT | ACB_WSTRUST)) ==
	    (ACB_PARTIAL_SECRETS_ACCOUNT | ACB_WSTRUST)) {
		/* the DOMAIN_RID_ENTERPRISE_READONLY_DCS PAC */
		user_info_dc->sids = talloc_realloc(user_info_dc,
						   user_info_dc->sids,
						   struct dom_sid,
						   user_info_dc->num_sids+1);
		NT_STATUS_HAVE_NO_MEMORY_AND_FREE(user_info_dc->sids, user_info_dc);
		user_info_dc->sids[user_info_dc->num_sids] = *domain_sid;
		sid_append_rid(&user_info_dc->sids[user_info_dc->num_sids],
			    DOMAIN_RID_ENTERPRISE_READONLY_DCS);
		user_info_dc->num_sids++;
	}

	info->authenticated = true;

	talloc_free(tmp_ctx);
	*_user_info_dc = user_info_dc;

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
	ret = dsdb_search_one(sam_ctx, tmp_ctx, msg, user_dn,
			      LDB_SCOPE_BASE, attrs, DSDB_SEARCH_SHOW_EXTENDED_DN, "(objectClass=*)");
	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}
	talloc_steal(mem_ctx, *msg);
	talloc_steal(mem_ctx, *domain_dn);
	talloc_free(tmp_ctx);
	
	return NT_STATUS_OK;
}

/* Used in the gensec_gssapi and gensec_krb5 server-side code, where the PAC isn't available, and for tokenGroups in the DSDB stack.

 Supply either a principal or a DN
*/
NTSTATUS authsam_get_user_info_dc_principal(TALLOC_CTX *mem_ctx,
					   struct loadparm_context *lp_ctx,
					   struct ldb_context *sam_ctx,
					   const char *principal,
					   struct ldb_dn *user_dn,
					   struct auth_user_info_dc **user_info_dc)
{
	NTSTATUS nt_status;
	DATA_BLOB user_sess_key = data_blob(NULL, 0);
	DATA_BLOB lm_sess_key = data_blob(NULL, 0);

	struct ldb_message *msg;
	struct ldb_dn *domain_dn;

	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
	if (!tmp_ctx) {
		return NT_STATUS_NO_MEMORY;
	}

	if (principal) {
		nt_status = sam_get_results_principal(sam_ctx, tmp_ctx, principal,
						      user_attrs, &domain_dn, &msg);
		if (!NT_STATUS_IS_OK(nt_status)) {
			talloc_free(tmp_ctx);
			return nt_status;
		}
	} else if (user_dn) {
		struct dom_sid *user_sid, *domain_sid;
		int ret;
		/* pull the user attributes */
		ret = dsdb_search_one(sam_ctx, tmp_ctx, &msg, user_dn,
				      LDB_SCOPE_BASE, user_attrs, DSDB_SEARCH_SHOW_EXTENDED_DN, "(objectClass=*)");
		if (ret == LDB_ERR_NO_SUCH_OBJECT) {
			talloc_free(tmp_ctx);
			return NT_STATUS_NO_SUCH_USER;
		} else if (ret != LDB_SUCCESS) {
			talloc_free(tmp_ctx);
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		user_sid = samdb_result_dom_sid(msg, msg, "objectSid");

		nt_status = dom_sid_split_rid(tmp_ctx, user_sid, &domain_sid, NULL);
		if (!NT_STATUS_IS_OK(nt_status)) {
			return nt_status;
		}

		domain_dn = samdb_search_dn(sam_ctx, mem_ctx, NULL,
					  "(&(objectSid=%s)(objectClass=domain))",
					    ldap_encode_ndr_dom_sid(tmp_ctx, domain_sid));
		if (!domain_dn) {
			DEBUG(3, ("authsam_get_user_info_dc_principal: Failed to find domain with: SID %s\n",
				  dom_sid_string(tmp_ctx, domain_sid)));
			return NT_STATUS_NO_SUCH_USER;
		}

	} else {
		return NT_STATUS_INVALID_PARAMETER;
	}

	nt_status = authsam_make_user_info_dc(tmp_ctx, sam_ctx,
					     lpcfg_netbios_name(lp_ctx),
					     lpcfg_workgroup(lp_ctx),
					     domain_dn,
					     msg,
					     user_sess_key, lm_sess_key,
					     user_info_dc);
	if (!NT_STATUS_IS_OK(nt_status)) {
		talloc_free(tmp_ctx);
		return nt_status;
	}

	talloc_steal(mem_ctx, *user_info_dc);
	talloc_free(tmp_ctx);

	return NT_STATUS_OK;
}
