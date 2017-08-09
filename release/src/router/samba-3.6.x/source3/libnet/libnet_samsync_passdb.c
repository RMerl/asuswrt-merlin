/*
   Unix SMB/CIFS implementation.
   dump the remote SAM using rpc samsync operations

   Copyright (C) Andrew Tridgell 2002
   Copyright (C) Tim Potter 2001,2002
   Copyright (C) Jim McDonough <jmcd@us.ibm.com> 2005
   Modified by Volker Lendecke 2002
   Copyright (C) Jeremy Allison 2005.
   Copyright (C) Guenther Deschner 2008.

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
#include "system/passwd.h"
#include "libnet/libnet_samsync.h"
#include "../libcli/security/security.h"
#include "passdb.h"

/* Convert a struct samu_DELTA to a struct samu. */
#define STRING_CHANGED (old_string && !new_string) ||\
		    (!old_string && new_string) ||\
		(old_string && new_string && (strcmp(old_string, new_string) != 0))

#define STRING_CHANGED_NC(s1,s2) ((s1) && !(s2)) ||\
		    (!(s1) && (s2)) ||\
		((s1) && (s2) && (strcmp((s1), (s2)) != 0))

/****************************************************************
****************************************************************/

static NTSTATUS sam_account_from_delta(struct samu *account,
				       struct netr_DELTA_USER *r)
{
	const char *old_string, *new_string;
	time_t unix_time, stored_time;
	uchar zero_buf[16];

	memset(zero_buf, '\0', sizeof(zero_buf));

	/* Username, fullname, home dir, dir drive, logon script, acct
	   desc, workstations, profile. */

	if (r->account_name.string) {
		old_string = pdb_get_nt_username(account);
		new_string = r->account_name.string;

		if (STRING_CHANGED) {
			pdb_set_nt_username(account, new_string, PDB_CHANGED);
		}

		/* Unix username is the same - for sanity */
		old_string = pdb_get_username( account );
		if (STRING_CHANGED) {
			pdb_set_username(account, new_string, PDB_CHANGED);
		}
	}

	if (r->full_name.string) {
		old_string = pdb_get_fullname(account);
		new_string = r->full_name.string;

		if (STRING_CHANGED)
			pdb_set_fullname(account, new_string, PDB_CHANGED);
	}

	if (r->home_directory.string) {
		old_string = pdb_get_homedir(account);
		new_string = r->home_directory.string;

		if (STRING_CHANGED)
			pdb_set_homedir(account, new_string, PDB_CHANGED);
	}

	if (r->home_drive.string) {
		old_string = pdb_get_dir_drive(account);
		new_string = r->home_drive.string;

		if (STRING_CHANGED)
			pdb_set_dir_drive(account, new_string, PDB_CHANGED);
	}

	if (r->logon_script.string) {
		old_string = pdb_get_logon_script(account);
		new_string = r->logon_script.string;

		if (STRING_CHANGED)
			pdb_set_logon_script(account, new_string, PDB_CHANGED);
	}

	if (r->description.string) {
		old_string = pdb_get_acct_desc(account);
		new_string = r->description.string;

		if (STRING_CHANGED)
			pdb_set_acct_desc(account, new_string, PDB_CHANGED);
	}

	if (r->workstations.string) {
		old_string = pdb_get_workstations(account);
		new_string = r->workstations.string;

		if (STRING_CHANGED)
			pdb_set_workstations(account, new_string, PDB_CHANGED);
	}

	if (r->profile_path.string) {
		old_string = pdb_get_profile_path(account);
		new_string = r->profile_path.string;

		if (STRING_CHANGED)
			pdb_set_profile_path(account, new_string, PDB_CHANGED);
	}

	if (r->parameters.array) {
		DATA_BLOB mung;
		char *newstr;
		old_string = pdb_get_munged_dial(account);
		mung.length = r->parameters.length * 2;
		mung.data = (uint8_t *) r->parameters.array;
		newstr = (mung.length == 0) ? NULL :
			base64_encode_data_blob(talloc_tos(), mung);

		if (STRING_CHANGED_NC(old_string, newstr))
			pdb_set_munged_dial(account, newstr, PDB_CHANGED);
		TALLOC_FREE(newstr);
	}

	/* User and group sid */
	if (pdb_get_user_rid(account) != r->rid)
		pdb_set_user_sid_from_rid(account, r->rid, PDB_CHANGED);
	if (pdb_get_group_rid(account) != r->primary_gid)
		pdb_set_group_sid_from_rid(account, r->primary_gid, PDB_CHANGED);

	/* Logon and password information */
	if (!nt_time_is_zero(&r->last_logon)) {
		unix_time = nt_time_to_unix(r->last_logon);
		stored_time = pdb_get_logon_time(account);
		if (stored_time != unix_time)
			pdb_set_logon_time(account, unix_time, PDB_CHANGED);
	}

	if (!nt_time_is_zero(&r->last_logoff)) {
		unix_time = nt_time_to_unix(r->last_logoff);
		stored_time = pdb_get_logoff_time(account);
		if (stored_time != unix_time)
			pdb_set_logoff_time(account, unix_time,PDB_CHANGED);
	}

	/* Logon Divs */
	if (pdb_get_logon_divs(account) != r->logon_hours.units_per_week)
		pdb_set_logon_divs(account, r->logon_hours.units_per_week, PDB_CHANGED);

#if 0
	/* no idea what to do with this one - gd */
	/* Max Logon Hours */
	if (delta->unknown1 != pdb_get_unknown_6(account)) {
		pdb_set_unknown_6(account, delta->unknown1, PDB_CHANGED);
	}
#endif
	/* Logon Hours Len */
	if (r->logon_hours.units_per_week/8 != pdb_get_hours_len(account)) {
		pdb_set_hours_len(account, r->logon_hours.units_per_week/8, PDB_CHANGED);
	}

	/* Logon Hours */
	if (r->logon_hours.bits) {
		char oldstr[44], newstr[44];
		pdb_sethexhours(oldstr, pdb_get_hours(account));
		pdb_sethexhours(newstr, r->logon_hours.bits);
		if (!strequal(oldstr, newstr))
			pdb_set_hours(account, r->logon_hours.bits,
				      pdb_get_hours_len(account), PDB_CHANGED);
	}

	if (pdb_get_bad_password_count(account) != r->bad_password_count)
		pdb_set_bad_password_count(account, r->bad_password_count, PDB_CHANGED);

	if (pdb_get_logon_count(account) != r->logon_count)
		pdb_set_logon_count(account, r->logon_count, PDB_CHANGED);

	if (!nt_time_is_zero(&r->last_password_change)) {
		unix_time = nt_time_to_unix(r->last_password_change);
		stored_time = pdb_get_pass_last_set_time(account);
		if (stored_time != unix_time)
			pdb_set_pass_last_set_time(account, unix_time, PDB_CHANGED);
	} else {
		/* no last set time, make it now */
		pdb_set_pass_last_set_time(account, time(NULL), PDB_CHANGED);
	}

	if (!nt_time_is_zero(&r->acct_expiry)) {
		unix_time = nt_time_to_unix(r->acct_expiry);
		stored_time = pdb_get_kickoff_time(account);
		if (stored_time != unix_time)
			pdb_set_kickoff_time(account, unix_time, PDB_CHANGED);
	}

	/* Decode hashes from password hash
	   Note that win2000 may send us all zeros for the hashes if it doesn't
	   think this channel is secure enough - don't set the passwords at all
	   in that case
	*/
	if (memcmp(r->lmpassword.hash, zero_buf, 16) != 0) {
		pdb_set_lanman_passwd(account, r->lmpassword.hash, PDB_CHANGED);
	}

	if (memcmp(r->ntpassword.hash, zero_buf, 16) != 0) {
		pdb_set_nt_passwd(account, r->ntpassword.hash, PDB_CHANGED);
	}

	/* TODO: account expiry time */

	pdb_set_acct_ctrl(account, r->acct_flags, PDB_CHANGED);

	pdb_set_domain(account, lp_workgroup(), PDB_CHANGED);

	return NT_STATUS_OK;
}


/****************************************************************
****************************************************************/

static NTSTATUS smb_create_user(TALLOC_CTX *mem_ctx,
				uint32_t acct_flags,
				const char *account,
				struct passwd **passwd_p)
{
	struct passwd *passwd;
	char *add_script = NULL;

	passwd = Get_Pwnam_alloc(mem_ctx, account);
	if (passwd) {
		*passwd_p = passwd;
		return NT_STATUS_OK;
	}

	/* Create appropriate user */
	if (acct_flags & ACB_NORMAL) {
		add_script = talloc_strdup(mem_ctx, lp_adduser_script());
	} else if ( (acct_flags & ACB_WSTRUST) ||
		    (acct_flags & ACB_SVRTRUST) ||
		    (acct_flags & ACB_DOMTRUST) ) {
		add_script = talloc_strdup(mem_ctx, lp_addmachine_script());
	} else {
		DEBUG(1, ("Unknown user type: %s\n",
			  pdb_encode_acct_ctrl(acct_flags, NEW_PW_FORMAT_SPACE_PADDED_LEN)));
		return NT_STATUS_UNSUCCESSFUL;
	}

	if (!add_script) {
		return NT_STATUS_NO_MEMORY;
	}

	if (*add_script) {
		int add_ret;
		add_script = talloc_all_string_sub(mem_ctx, add_script,
						   "%u", account);
		if (!add_script) {
			return NT_STATUS_NO_MEMORY;
		}
		add_ret = smbrun(add_script, NULL);
		DEBUG(add_ret ? 0 : 1,("fetch_account: Running the command `%s' "
			 "gave %d\n", add_script, add_ret));
		if (add_ret == 0) {
			smb_nscd_flush_user_cache();
		}
	}

	/* try and find the possible unix account again */
	passwd = Get_Pwnam_alloc(mem_ctx, account);
	if (!passwd) {
		return NT_STATUS_NO_SUCH_USER;
	}

	*passwd_p = passwd;

	return NT_STATUS_OK;
}
/****************************************************************
****************************************************************/

static NTSTATUS fetch_account_info(TALLOC_CTX *mem_ctx,
				   uint32_t rid,
				   struct netr_DELTA_USER *r)
{

	NTSTATUS nt_ret = NT_STATUS_UNSUCCESSFUL;
	fstring account;
	struct samu *sam_account=NULL;
	GROUP_MAP map;
	struct group *grp;
	struct dom_sid user_sid;
	struct dom_sid group_sid;
	struct passwd *passwd = NULL;
	fstring sid_string;

	fstrcpy(account, r->account_name.string);
	d_printf("Creating account: %s\n", account);

	if ( !(sam_account = samu_new(mem_ctx)) ) {
		return NT_STATUS_NO_MEMORY;
	}

	nt_ret = smb_create_user(sam_account, r->acct_flags, account, &passwd);
	if (!NT_STATUS_IS_OK(nt_ret)) {
		d_fprintf(stderr, "Could not create posix account info for '%s'\n",
			account);
		goto done;
	}

	sid_compose(&user_sid, get_global_sam_sid(), r->rid);

	DEBUG(3, ("Attempting to find SID %s for user %s in the passdb\n",
		  sid_to_fstring(sid_string, &user_sid), account));
	if (!pdb_getsampwsid(sam_account, &user_sid)) {
		sam_account_from_delta(sam_account, r);
		DEBUG(3, ("Attempting to add user SID %s for user %s in the passdb\n",
			  sid_to_fstring(sid_string, &user_sid),
			  pdb_get_username(sam_account)));
		if (!NT_STATUS_IS_OK(pdb_add_sam_account(sam_account))) {
			DEBUG(1, ("SAM Account for %s failed to be added to the passdb!\n",
				  account));
			return NT_STATUS_ACCESS_DENIED;
		}
	} else {
		sam_account_from_delta(sam_account, r);
		DEBUG(3, ("Attempting to update user SID %s for user %s in the passdb\n",
			  sid_to_fstring(sid_string, &user_sid),
			  pdb_get_username(sam_account)));
		if (!NT_STATUS_IS_OK(pdb_update_sam_account(sam_account))) {
			DEBUG(1, ("SAM Account for %s failed to be updated in the passdb!\n",
				  account));
			TALLOC_FREE(sam_account);
			return NT_STATUS_ACCESS_DENIED;
		}
	}

	if (pdb_get_group_sid(sam_account) == NULL) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	group_sid = *pdb_get_group_sid(sam_account);

	if (!pdb_getgrsid(&map, group_sid)) {
		DEBUG(0, ("Primary group of %s has no mapping!\n",
			  pdb_get_username(sam_account)));
	} else {
		if (map.gid != passwd->pw_gid) {
			if (!(grp = getgrgid(map.gid))) {
				DEBUG(0, ("Could not find unix group %lu for user %s (group SID=%s)\n",
					  (unsigned long)map.gid, pdb_get_username(sam_account), sid_string_tos(&group_sid)));
			} else {
				smb_set_primary_group(grp->gr_name, pdb_get_username(sam_account));
			}
		}
	}

	if ( !passwd ) {
		DEBUG(1, ("No unix user for this account (%s), cannot adjust mappings\n",
			pdb_get_username(sam_account)));
	}

 done:
	TALLOC_FREE(sam_account);
	return nt_ret;
}

/****************************************************************
****************************************************************/

static NTSTATUS fetch_group_info(TALLOC_CTX *mem_ctx,
				 uint32_t rid,
				 struct netr_DELTA_GROUP *r)
{
	fstring name;
	fstring comment;
	struct group *grp = NULL;
	struct dom_sid group_sid;
	fstring sid_string;
	GROUP_MAP map;
	bool insert = true;

	fstrcpy(name, r->group_name.string);
	fstrcpy(comment, r->description.string);

	/* add the group to the mapping table */
	sid_compose(&group_sid, get_global_sam_sid(), rid);
	sid_to_fstring(sid_string, &group_sid);

	if (pdb_getgrsid(&map, group_sid)) {
		if ( map.gid != -1 )
			grp = getgrgid(map.gid);
		insert = false;
	}

	if (grp == NULL) {
		gid_t gid;

		/* No group found from mapping, find it from its name. */
		if ((grp = getgrnam(name)) == NULL) {

			/* No appropriate group found, create one */

			d_printf("Creating unix group: '%s'\n", name);

			if (smb_create_group(name, &gid) != 0)
				return NT_STATUS_ACCESS_DENIED;

			if ((grp = getgrnam(name)) == NULL)
				return NT_STATUS_ACCESS_DENIED;
		}
	}

	map.gid = grp->gr_gid;
	map.sid = group_sid;
	map.sid_name_use = SID_NAME_DOM_GRP;
	fstrcpy(map.nt_name, name);
	if (r->description.string) {
		fstrcpy(map.comment, comment);
	} else {
		fstrcpy(map.comment, "");
	}

	if (insert)
		pdb_add_group_mapping_entry(&map);
	else
		pdb_update_group_mapping_entry(&map);

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static NTSTATUS fetch_group_mem_info(TALLOC_CTX *mem_ctx,
				     uint32_t rid,
				     struct netr_DELTA_GROUP_MEMBER *r)
{
	int i;
	char **nt_members = NULL;
	char **unix_members;
	struct dom_sid group_sid;
	GROUP_MAP map;
	struct group *grp;

	if (r->num_rids == 0) {
		return NT_STATUS_OK;
	}

	sid_compose(&group_sid, get_global_sam_sid(), rid);

	if (!get_domain_group_from_sid(group_sid, &map)) {
		DEBUG(0, ("Could not find global group %d\n", rid));
		return NT_STATUS_NO_SUCH_GROUP;
	}

	if (!(grp = getgrgid(map.gid))) {
		DEBUG(0, ("Could not find unix group %lu\n", (unsigned long)map.gid));
		return NT_STATUS_NO_SUCH_GROUP;
	}

	d_printf("Group members of %s: ", grp->gr_name);

	if (r->num_rids) {
		if ((nt_members = TALLOC_ZERO_ARRAY(mem_ctx, char *, r->num_rids)) == NULL) {
			DEBUG(0, ("talloc failed\n"));
			return NT_STATUS_NO_MEMORY;
		}
	} else {
		nt_members = NULL;
	}

	for (i=0; i < r->num_rids; i++) {
		struct samu *member = NULL;
		struct dom_sid member_sid;

		if ( !(member = samu_new(mem_ctx)) ) {
			return NT_STATUS_NO_MEMORY;
		}

		sid_compose(&member_sid, get_global_sam_sid(), r->rids[i]);

		if (!pdb_getsampwsid(member, &member_sid)) {
			DEBUG(1, ("Found bogus group member: %d (member_sid=%s group=%s)\n",
				  r->rids[i], sid_string_tos(&member_sid), grp->gr_name));
			TALLOC_FREE(member);
			continue;
		}

		if (pdb_get_group_rid(member) == rid) {
			d_printf("%s(primary),", pdb_get_username(member));
			TALLOC_FREE(member);
			continue;
		}

		d_printf("%s,", pdb_get_username(member));
		nt_members[i] = talloc_strdup(mem_ctx, pdb_get_username(member));
		TALLOC_FREE(member);
	}

	d_printf("\n");

	unix_members = grp->gr_mem;

	while (*unix_members) {
		bool is_nt_member = false;
		for (i=0; i < r->num_rids; i++) {
			if (nt_members[i] == NULL) {
				/* This was a primary group */
				continue;
			}

			if (strcmp(*unix_members, nt_members[i]) == 0) {
				is_nt_member = true;
				break;
			}
		}
		if (!is_nt_member) {
			/* We look at a unix group member that is not
			   an nt group member. So, remove it. NT is
			   boss here. */
			smb_delete_user_group(grp->gr_name, *unix_members);
		}
		unix_members += 1;
	}

	for (i=0; i < r->num_rids; i++) {
		bool is_unix_member = false;

		if (nt_members[i] == NULL) {
			/* This was the primary group */
			continue;
		}

		unix_members = grp->gr_mem;

		while (*unix_members) {
			if (strcmp(*unix_members, nt_members[i]) == 0) {
				is_unix_member = true;
				break;
			}
			unix_members += 1;
		}

		if (!is_unix_member) {
			/* We look at a nt group member that is not a
                           unix group member currently. So, add the nt
                           group member. */
			smb_add_user_group(grp->gr_name, nt_members[i]);
		}
	}

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static NTSTATUS fetch_alias_info(TALLOC_CTX *mem_ctx,
				 uint32_t rid,
				 struct netr_DELTA_ALIAS *r,
				 const struct dom_sid *dom_sid)
{
	fstring name;
	fstring comment;
	struct group *grp = NULL;
	struct dom_sid alias_sid;
	fstring sid_string;
	GROUP_MAP map;
	bool insert = true;

	fstrcpy(name, r->alias_name.string);
	fstrcpy(comment, r->description.string);

	/* Find out whether the group is already mapped */
	sid_compose(&alias_sid, dom_sid, rid);
	sid_to_fstring(sid_string, &alias_sid);

	if (pdb_getgrsid(&map, alias_sid)) {
		grp = getgrgid(map.gid);
		insert = false;
	}

	if (grp == NULL) {
		gid_t gid;

		/* No group found from mapping, find it from its name. */
		if ((grp = getgrnam(name)) == NULL) {
			/* No appropriate group found, create one */
			d_printf("Creating unix group: '%s'\n", name);
			if (smb_create_group(name, &gid) != 0)
				return NT_STATUS_ACCESS_DENIED;
			if ((grp = getgrgid(gid)) == NULL)
				return NT_STATUS_ACCESS_DENIED;
		}
	}

	map.gid = grp->gr_gid;
	map.sid = alias_sid;

	if (dom_sid_equal(dom_sid, &global_sid_Builtin))
		map.sid_name_use = SID_NAME_WKN_GRP;
	else
		map.sid_name_use = SID_NAME_ALIAS;

	fstrcpy(map.nt_name, name);
	fstrcpy(map.comment, comment);

	if (insert)
		pdb_add_group_mapping_entry(&map);
	else
		pdb_update_group_mapping_entry(&map);

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static NTSTATUS fetch_alias_mem(TALLOC_CTX *mem_ctx,
				uint32_t rid,
				struct netr_DELTA_ALIAS_MEMBER *r,
				const struct dom_sid *dom_sid)
{
	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static NTSTATUS fetch_domain_info(TALLOC_CTX *mem_ctx,
				  uint32_t rid,
				  struct netr_DELTA_DOMAIN *r)
{
	time_t u_max_age, u_min_age, u_logout;
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;
	const char *domname;
	struct netr_AcctLockStr *lockstr = NULL;
	NTSTATUS status;

	status = pull_netr_AcctLockStr(mem_ctx, &r->account_lockout,
				       &lockstr);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("failed to pull account lockout string: %s\n",
			nt_errstr(status));
	}

	u_max_age = uint64s_nt_time_to_unix_abs((uint64 *)&r->max_password_age);
	u_min_age = uint64s_nt_time_to_unix_abs((uint64 *)&r->min_password_age);
	u_logout = uint64s_nt_time_to_unix_abs((uint64 *)&r->force_logoff_time);

	domname = r->domain_name.string;
	if (!domname) {
		return NT_STATUS_NO_MEMORY;
	}

	/* we don't handle BUILTIN account policies */
	if (!strequal(domname, get_global_sam_name())) {
		printf("skipping SAM_DOMAIN_INFO delta for '%s' (is not my domain)\n", domname);
		return NT_STATUS_OK;
	}


	if (!pdb_set_account_policy(PDB_POLICY_PASSWORD_HISTORY,
				    r->password_history_length))
		return nt_status;

	if (!pdb_set_account_policy(PDB_POLICY_MIN_PASSWORD_LEN,
				    r->min_password_length))
		return nt_status;

	if (!pdb_set_account_policy(PDB_POLICY_MAX_PASSWORD_AGE,
				    (uint32)u_max_age))
		return nt_status;

	if (!pdb_set_account_policy(PDB_POLICY_MIN_PASSWORD_AGE,
				    (uint32)u_min_age))
		return nt_status;

	if (!pdb_set_account_policy(PDB_POLICY_TIME_TO_LOGOUT,
				    (uint32)u_logout))
		return nt_status;

	if (lockstr) {
		time_t u_lockoutreset, u_lockouttime;

		u_lockoutreset = uint64s_nt_time_to_unix_abs(&lockstr->reset_count);
		u_lockouttime = uint64s_nt_time_to_unix_abs((uint64_t *)&lockstr->lockout_duration);

		if (!pdb_set_account_policy(PDB_POLICY_BAD_ATTEMPT_LOCKOUT,
					    lockstr->bad_attempt_lockout))
			return nt_status;

		if (!pdb_set_account_policy(PDB_POLICY_RESET_COUNT_TIME,
					    (uint32_t)u_lockoutreset/60))
			return nt_status;

		if (u_lockouttime != -1)
			u_lockouttime /= 60;

		if (!pdb_set_account_policy(PDB_POLICY_LOCK_ACCOUNT_DURATION,
					    (uint32_t)u_lockouttime))
			return nt_status;
	}

	if (!pdb_set_account_policy(PDB_POLICY_USER_MUST_LOGON_TO_CHG_PASS,
				    r->logon_to_chgpass))
		return nt_status;

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static NTSTATUS fetch_sam_entry(TALLOC_CTX *mem_ctx,
				enum netr_SamDatabaseID database_id,
				struct netr_DELTA_ENUM *r,
				struct samsync_context *ctx)
{
	NTSTATUS status = NT_STATUS_NOT_IMPLEMENTED;

	switch (r->delta_type) {
	case NETR_DELTA_USER:
		status = fetch_account_info(mem_ctx,
					    r->delta_id_union.rid,
					    r->delta_union.user);
		break;
	case NETR_DELTA_GROUP:
		status = fetch_group_info(mem_ctx,
					  r->delta_id_union.rid,
					  r->delta_union.group);
		break;
	case NETR_DELTA_GROUP_MEMBER:
		status = fetch_group_mem_info(mem_ctx,
					      r->delta_id_union.rid,
					      r->delta_union.group_member);
		break;
	case NETR_DELTA_ALIAS:
		status = fetch_alias_info(mem_ctx,
					  r->delta_id_union.rid,
					  r->delta_union.alias,
					  ctx->domain_sid);
		break;
	case NETR_DELTA_ALIAS_MEMBER:
		status = fetch_alias_mem(mem_ctx,
					 r->delta_id_union.rid,
					 r->delta_union.alias_member,
					 ctx->domain_sid);
		break;
	case NETR_DELTA_DOMAIN:
		status = fetch_domain_info(mem_ctx,
					   r->delta_id_union.rid,
					   r->delta_union.domain);
		break;
	/* The following types are recognised but not handled */
	case NETR_DELTA_RENAME_GROUP:
		d_printf("NETR_DELTA_RENAME_GROUP not handled\n");
		break;
	case NETR_DELTA_RENAME_USER:
		d_printf("NETR_DELTA_RENAME_USER not handled\n");
		break;
	case NETR_DELTA_RENAME_ALIAS:
		d_printf("NETR_DELTA_RENAME_ALIAS not handled\n");
		break;
	case NETR_DELTA_POLICY:
		d_printf("NETR_DELTA_POLICY not handled\n");
		break;
	case NETR_DELTA_TRUSTED_DOMAIN:
		d_printf("NETR_DELTA_TRUSTED_DOMAIN not handled\n");
		break;
	case NETR_DELTA_ACCOUNT:
		d_printf("NETR_DELTA_ACCOUNT not handled\n");
		break;
	case NETR_DELTA_SECRET:
		d_printf("NETR_DELTA_SECRET not handled\n");
		break;
	case NETR_DELTA_DELETE_GROUP:
		d_printf("NETR_DELTA_DELETE_GROUP not handled\n");
		break;
	case NETR_DELTA_DELETE_USER:
		d_printf("NETR_DELTA_DELETE_USER not handled\n");
		break;
	case NETR_DELTA_MODIFY_COUNT:
		d_printf("NETR_DELTA_MODIFY_COUNT not handled\n");
		break;
	case NETR_DELTA_DELETE_ALIAS:
		d_printf("NETR_DELTA_DELETE_ALIAS not handled\n");
		break;
	case NETR_DELTA_DELETE_TRUST:
		d_printf("NETR_DELTA_DELETE_TRUST not handled\n");
		break;
	case NETR_DELTA_DELETE_ACCOUNT:
		d_printf("NETR_DELTA_DELETE_ACCOUNT not handled\n");
		break;
	case NETR_DELTA_DELETE_SECRET:
		d_printf("NETR_DELTA_DELETE_SECRET not handled\n");
		break;
	case NETR_DELTA_DELETE_GROUP2:
		d_printf("NETR_DELTA_DELETE_GROUP2 not handled\n");
		break;
	case NETR_DELTA_DELETE_USER2:
		d_printf("NETR_DELTA_DELETE_USER2 not handled\n");
		break;
	default:
		d_printf("Unknown delta record type %d\n", r->delta_type);
		status = NT_STATUS_INVALID_PARAMETER;
		break;
	}

	return status;
}

/****************************************************************
****************************************************************/

static NTSTATUS fetch_sam_entries(TALLOC_CTX *mem_ctx,
				  enum netr_SamDatabaseID database_id,
				  struct netr_DELTA_ENUM_ARRAY *r,
				  uint64_t *sequence_num,
				  struct samsync_context *ctx)
{
	int i;

	for (i = 0; i < r->num_deltas; i++) {
		fetch_sam_entry(mem_ctx, database_id, &r->delta_enum[i], ctx);
	}

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

const struct samsync_ops libnet_samsync_passdb_ops = {
	.process_objects	= fetch_sam_entries,
};
