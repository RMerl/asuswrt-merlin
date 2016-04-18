/* 
   Unix SMB/CIFS implementation.
   struct samu access routines
   Copyright (C) Jeremy Allison 		1996-2001
   Copyright (C) Luke Kenneth Casson Leighton 	1996-1998
   Copyright (C) Gerald (Jerry) Carter		2000-2006
   Copyright (C) Andrew Bartlett		2001-2002
   Copyright (C) Stefan (metze) Metzmacher	2002

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
#include "passdb.h"
#include "../libcli/auth/libcli_auth.h"
#include "../libcli/security/security.h"
#include "../lib/util/bitmap.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_PASSDB

/**
 * @todo Redefine this to NULL, but this changes the API because
 *       much of samba assumes that the pdb_get...() funtions 
 *       return strings.  (ie not null-pointers).
 *       See also pdb_fill_default_sam().
 */

#define PDB_NOT_QUITE_NULL ""

/*********************************************************************
 Test if a change time is a max value. Copes with old and new values
 of max.
 ********************************************************************/

bool pdb_is_password_change_time_max(time_t test_time)
{
	if (test_time == get_time_t_max()) {
		return true;
	}
#if (defined(SIZEOF_TIME_T) && (SIZEOF_TIME_T == 8))
	if (test_time == 0x7FFFFFFFFFFFFFFFLL) {
		return true;
	}
#endif
	if (test_time == 0x7FFFFFFF) {
		return true;
	}
	return false;
}

/*********************************************************************
 Return an unchanging version of max password change time - 0x7FFFFFFF.
 ********************************************************************/

time_t pdb_password_change_time_max(void)
{
	return 0x7FFFFFFF;
}

/*********************************************************************
 Collection of get...() functions for struct samu.
 ********************************************************************/

uint32_t pdb_get_acct_ctrl(const struct samu *sampass)
{
	return sampass->acct_ctrl;
}

time_t pdb_get_logon_time(const struct samu *sampass)
{
	return sampass->logon_time;
}

time_t pdb_get_logoff_time(const struct samu *sampass)
{
	return sampass->logoff_time;
}

time_t pdb_get_kickoff_time(const struct samu *sampass)
{
	return sampass->kickoff_time;
}

time_t pdb_get_bad_password_time(const struct samu *sampass)
{
	return sampass->bad_password_time;
}

time_t pdb_get_pass_last_set_time(const struct samu *sampass)
{
	return sampass->pass_last_set_time;
}

time_t pdb_get_pass_can_change_time(const struct samu *sampass)
{
	uint32_t allow;

	/* if the last set time is zero, it means the user cannot 
	   change their password, and this time must be zero.   jmcd 
	*/
	if (sampass->pass_last_set_time == 0)
		return (time_t) 0;

	/* if the time is max, and the field has been changed,
	   we're trying to update this real value from the sampass
	   to indicate that the user cannot change their password.  jmcd
	*/
	if (pdb_is_password_change_time_max(sampass->pass_can_change_time) &&
	    IS_SAM_CHANGED(sampass, PDB_CANCHANGETIME))
		return sampass->pass_can_change_time;

	if (!pdb_get_account_policy(PDB_POLICY_MIN_PASSWORD_AGE, &allow))
		allow = 0;

	/* in normal cases, just calculate it from policy */
	return sampass->pass_last_set_time + allow;
}

/* we need this for loading from the backend, so that we don't overwrite
   non-changed max times, otherwise the pass_can_change checking won't work */
time_t pdb_get_pass_can_change_time_noncalc(const struct samu *sampass)
{
	return sampass->pass_can_change_time;
}

time_t pdb_get_pass_must_change_time(const struct samu *sampass)
{
	uint32_t expire;

	if (sampass->pass_last_set_time == 0)
		return (time_t) 0;

	if (sampass->acct_ctrl & ACB_PWNOEXP)
		return pdb_password_change_time_max();

	if (!pdb_get_account_policy(PDB_POLICY_MAX_PASSWORD_AGE, &expire)
	    || expire == (uint32_t)-1 || expire == 0)
		return get_time_t_max();

	return sampass->pass_last_set_time + expire;
}

bool pdb_get_pass_can_change(const struct samu *sampass)
{
	if (pdb_is_password_change_time_max(sampass->pass_can_change_time))
		return False;
	return True;
}

uint16_t pdb_get_logon_divs(const struct samu *sampass)
{
	return sampass->logon_divs;
}

uint32_t pdb_get_hours_len(const struct samu *sampass)
{
	return sampass->hours_len;
}

const uint8 *pdb_get_hours(const struct samu *sampass)
{
	return (sampass->hours);
}

const uint8 *pdb_get_nt_passwd(const struct samu *sampass)
{
	SMB_ASSERT((!sampass->nt_pw.data) 
		   || sampass->nt_pw.length == NT_HASH_LEN);
	return (uint8 *)sampass->nt_pw.data;
}

const uint8 *pdb_get_lanman_passwd(const struct samu *sampass)
{
	SMB_ASSERT((!sampass->lm_pw.data) 
		   || sampass->lm_pw.length == LM_HASH_LEN);
	return (uint8 *)sampass->lm_pw.data;
}

const uint8 *pdb_get_pw_history(const struct samu *sampass, uint32_t *current_hist_len)
{
	SMB_ASSERT((!sampass->nt_pw_his.data) 
	   || ((sampass->nt_pw_his.length % PW_HISTORY_ENTRY_LEN) == 0));
	*current_hist_len = sampass->nt_pw_his.length / PW_HISTORY_ENTRY_LEN;
	return (uint8 *)sampass->nt_pw_his.data;
}

/* Return the plaintext password if known.  Most of the time
   it isn't, so don't assume anything magic about this function.

   Used to pass the plaintext to passdb backends that might 
   want to store more than just the NTLM hashes.
*/
const char *pdb_get_plaintext_passwd(const struct samu *sampass)
{
	return sampass->plaintext_pw;
}

const struct dom_sid *pdb_get_user_sid(const struct samu *sampass)
{
	return &sampass->user_sid;
}

const struct dom_sid *pdb_get_group_sid(struct samu *sampass)
{
	NTSTATUS status;

	/* Return the cached group SID if we have that */
	if (sampass->group_sid) {
		return sampass->group_sid;
	}

	/* No algorithmic mapping, meaning that we have to figure out the
	   primary group SID according to group mapping and the user SID must
	   be a newly allocated one.  We rely on the user's Unix primary gid.
	   We have no choice but to fail if we can't find it. */
	status = get_primary_group_sid(sampass,
					pdb_get_username(sampass),
					&sampass->unix_pw,
					&sampass->group_sid);
	if (!NT_STATUS_IS_OK(status)) {
		return NULL;
	}

	return sampass->group_sid;
}

/**
 * Get flags showing what is initalised in the struct samu
 * @param sampass the struct samu in question
 * @return the flags indicating the members initialised in the struct.
 **/

enum pdb_value_state pdb_get_init_flags(const struct samu *sampass, enum pdb_elements element)
{
	enum pdb_value_state ret = PDB_DEFAULT;

        if (!sampass->change_flags || !sampass->set_flags)
        	return ret;

        if (bitmap_query(sampass->set_flags, element)) {
		DEBUG(11, ("element %d: SET\n", element)); 
        	ret = PDB_SET;
	}

        if (bitmap_query(sampass->change_flags, element)) {
		DEBUG(11, ("element %d: CHANGED\n", element)); 
        	ret = PDB_CHANGED;
	}

	if (ret == PDB_DEFAULT) {
		DEBUG(11, ("element %d: DEFAULT\n", element)); 
	}

        return ret;
}

const char *pdb_get_username(const struct samu *sampass)
{
	return sampass->username;
}

const char *pdb_get_domain(const struct samu *sampass)
{
	return sampass->domain;
}

const char *pdb_get_nt_username(const struct samu *sampass)
{
	return sampass->nt_username;
}

const char *pdb_get_fullname(const struct samu *sampass)
{
	return sampass->full_name;
}

const char *pdb_get_homedir(const struct samu *sampass)
{
	return sampass->home_dir;
}

const char *pdb_get_dir_drive(const struct samu *sampass)
{
	return sampass->dir_drive;
}

const char *pdb_get_logon_script(const struct samu *sampass)
{
	return sampass->logon_script;
}

const char *pdb_get_profile_path(const struct samu *sampass)
{
	return sampass->profile_path;
}

const char *pdb_get_acct_desc(const struct samu *sampass)
{
	return sampass->acct_desc;
}

const char *pdb_get_workstations(const struct samu *sampass)
{
	return sampass->workstations;
}

const char *pdb_get_comment(const struct samu *sampass)
{
	return sampass->comment;
}

const char *pdb_get_munged_dial(const struct samu *sampass)
{
	return sampass->munged_dial;
}

uint16_t pdb_get_bad_password_count(const struct samu *sampass)
{
	return sampass->bad_password_count;
}

uint16_t pdb_get_logon_count(const struct samu *sampass)
{
	return sampass->logon_count;
}

uint16_t pdb_get_country_code(const struct samu *sampass)
{
	return sampass->country_code;
}

uint16_t pdb_get_code_page(const struct samu *sampass)
{
	return sampass->code_page;
}

uint32_t pdb_get_unknown_6(const struct samu *sampass)
{
	return sampass->unknown_6;
}

void *pdb_get_backend_private_data(const struct samu *sampass, const struct pdb_methods *my_methods)
{
	if (my_methods == sampass->backend_private_methods) {
		return sampass->backend_private_data;
	} else {
		return NULL;
	}
}

/*********************************************************************
 Collection of set...() functions for struct samu.
 ********************************************************************/

bool pdb_set_acct_ctrl(struct samu *sampass, uint32_t acct_ctrl, enum pdb_value_state flag)
{
	sampass->acct_ctrl = acct_ctrl;
	return pdb_set_init_flags(sampass, PDB_ACCTCTRL, flag);
}

bool pdb_set_logon_time(struct samu *sampass, time_t mytime, enum pdb_value_state flag)
{
	sampass->logon_time = mytime;
	return pdb_set_init_flags(sampass, PDB_LOGONTIME, flag);
}

bool pdb_set_logoff_time(struct samu *sampass, time_t mytime, enum pdb_value_state flag)
{
	sampass->logoff_time = mytime;
	return pdb_set_init_flags(sampass, PDB_LOGOFFTIME, flag);
}

bool pdb_set_kickoff_time(struct samu *sampass, time_t mytime, enum pdb_value_state flag)
{
	sampass->kickoff_time = mytime;
	return pdb_set_init_flags(sampass, PDB_KICKOFFTIME, flag);
}

bool pdb_set_bad_password_time(struct samu *sampass, time_t mytime, enum pdb_value_state flag)
{
	sampass->bad_password_time = mytime;
	return pdb_set_init_flags(sampass, PDB_BAD_PASSWORD_TIME, flag);
}

bool pdb_set_pass_can_change_time(struct samu *sampass, time_t mytime, enum pdb_value_state flag)
{
	sampass->pass_can_change_time = mytime;
	return pdb_set_init_flags(sampass, PDB_CANCHANGETIME, flag);
}

bool pdb_set_pass_must_change_time(struct samu *sampass, time_t mytime, enum pdb_value_state flag)
{
	sampass->pass_must_change_time = mytime;
	return pdb_set_init_flags(sampass, PDB_MUSTCHANGETIME, flag);
}

bool pdb_set_pass_last_set_time(struct samu *sampass, time_t mytime, enum pdb_value_state flag)
{
	sampass->pass_last_set_time = mytime;
	return pdb_set_init_flags(sampass, PDB_PASSLASTSET, flag);
}

bool pdb_set_hours_len(struct samu *sampass, uint32_t len, enum pdb_value_state flag)
{
	sampass->hours_len = len;
	return pdb_set_init_flags(sampass, PDB_HOURSLEN, flag);
}

bool pdb_set_logon_divs(struct samu *sampass, uint16_t hours, enum pdb_value_state flag)
{
	sampass->logon_divs = hours;
	return pdb_set_init_flags(sampass, PDB_LOGONDIVS, flag);
}

/**
 * Set flags showing what is initalised in the struct samu
 * @param sampass the struct samu in question
 * @param flag The *new* flag to be set.  Old flags preserved
 *             this flag is only added.  
 **/

bool pdb_set_init_flags(struct samu *sampass, enum pdb_elements element, enum pdb_value_state value_flag)
{
        if (!sampass->set_flags) {
        	if ((sampass->set_flags = 
        		bitmap_talloc(sampass, 
        				PDB_COUNT))==NULL) {
        		DEBUG(0,("bitmap_talloc failed\n"));
        		return False;
        	}
        }
        if (!sampass->change_flags) {
        	if ((sampass->change_flags = 
        		bitmap_talloc(sampass, 
        				PDB_COUNT))==NULL) {
        		DEBUG(0,("bitmap_talloc failed\n"));
        		return False;
        	}
        }

        switch(value_flag) {
        	case PDB_CHANGED:
        		if (!bitmap_set(sampass->change_flags, element)) {
				DEBUG(0,("Can't set flag: %d in change_flags.\n",element));
				return False;
			}
        		if (!bitmap_set(sampass->set_flags, element)) {
				DEBUG(0,("Can't set flag: %d in set_flags.\n",element));
				return False;
			}
			DEBUG(11, ("element %d -> now CHANGED\n", element)); 
        		break;
        	case PDB_SET:
        		if (!bitmap_clear(sampass->change_flags, element)) {
				DEBUG(0,("Can't set flag: %d in change_flags.\n",element));
				return False;
			}
        		if (!bitmap_set(sampass->set_flags, element)) {
				DEBUG(0,("Can't set flag: %d in set_flags.\n",element));
				return False;
			}
			DEBUG(11, ("element %d -> now SET\n", element)); 
        		break;
        	case PDB_DEFAULT:
        	default:
        		if (!bitmap_clear(sampass->change_flags, element)) {
				DEBUG(0,("Can't set flag: %d in change_flags.\n",element));
				return False;
			}
        		if (!bitmap_clear(sampass->set_flags, element)) {
				DEBUG(0,("Can't set flag: %d in set_flags.\n",element));
				return False;
			}
			DEBUG(11, ("element %d -> now DEFAULT\n", element)); 
        		break;
	}

        return True;
}

bool pdb_set_user_sid(struct samu *sampass, const struct dom_sid *u_sid, enum pdb_value_state flag)
{
	if (!u_sid)
		return False;

	sid_copy(&sampass->user_sid, u_sid);

	DEBUG(10, ("pdb_set_user_sid: setting user sid %s\n", 
		    sid_string_dbg(&sampass->user_sid)));

	return pdb_set_init_flags(sampass, PDB_USERSID, flag);
}

bool pdb_set_user_sid_from_string(struct samu *sampass, fstring u_sid, enum pdb_value_state flag)
{
	struct dom_sid new_sid;

	if (!u_sid)
		return False;

	DEBUG(10, ("pdb_set_user_sid_from_string: setting user sid %s\n",
		   u_sid));

	if (!string_to_sid(&new_sid, u_sid)) { 
		DEBUG(1, ("pdb_set_user_sid_from_string: %s isn't a valid SID!\n", u_sid));
		return False;
	}

	if (!pdb_set_user_sid(sampass, &new_sid, flag)) {
		DEBUG(1, ("pdb_set_user_sid_from_string: could not set sid %s on struct samu!\n", u_sid));
		return False;
	}

	return True;
}

/********************************************************************
 We never fill this in from a passdb backend but rather set is 
 based on the user's primary group membership.  However, the 
 struct samu* is overloaded and reused in domain memship code 
 as well and built from the netr_SamInfo3 or PAC so we
 have to allow the explicitly setting of a group SID here.
********************************************************************/

bool pdb_set_group_sid(struct samu *sampass, const struct dom_sid *g_sid, enum pdb_value_state flag)
{
	gid_t gid;
	struct dom_sid dug_sid;

	if (!g_sid)
		return False;

	if ( !(sampass->group_sid = TALLOC_P( sampass, struct dom_sid )) ) {
		return False;
	}

	/* if we cannot resolve the SID to gid, then just ignore it and 
	   store DOMAIN_USERS as the primary groupSID */

	sid_compose(&dug_sid, get_global_sam_sid(), DOMAIN_RID_USERS);

	if (dom_sid_equal(&dug_sid, g_sid)) {
		sid_copy(sampass->group_sid, &dug_sid);
	} else if (sid_to_gid( g_sid, &gid ) ) {
		sid_copy(sampass->group_sid, g_sid);
	} else {
		sid_copy(sampass->group_sid, &dug_sid);
	}

	DEBUG(10, ("pdb_set_group_sid: setting group sid %s\n", 
		   sid_string_dbg(sampass->group_sid)));

	return pdb_set_init_flags(sampass, PDB_GROUPSID, flag);
}

/*********************************************************************
 Set the user's UNIX name.
 ********************************************************************/

bool pdb_set_username(struct samu *sampass, const char *username, enum pdb_value_state flag)
{
	if (username) { 
		DEBUG(10, ("pdb_set_username: setting username %s, was %s\n", username,
			(sampass->username)?(sampass->username):"NULL"));

		sampass->username = talloc_strdup(sampass, username);

		if (!sampass->username) {
			DEBUG(0, ("pdb_set_username: talloc_strdup() failed!\n"));
			return False;
		}
	} else {
		sampass->username = PDB_NOT_QUITE_NULL;
	}

	return pdb_set_init_flags(sampass, PDB_USERNAME, flag);
}

/*********************************************************************
 Set the domain name.
 ********************************************************************/

bool pdb_set_domain(struct samu *sampass, const char *domain, enum pdb_value_state flag)
{
	if (domain) { 
		DEBUG(10, ("pdb_set_domain: setting domain %s, was %s\n", domain,
			(sampass->domain)?(sampass->domain):"NULL"));

		sampass->domain = talloc_strdup(sampass, domain);

		if (!sampass->domain) {
			DEBUG(0, ("pdb_set_domain: talloc_strdup() failed!\n"));
			return False;
		}
	} else {
		sampass->domain = PDB_NOT_QUITE_NULL;
	}

	return pdb_set_init_flags(sampass, PDB_DOMAIN, flag);
}

/*********************************************************************
 Set the user's NT name.
 ********************************************************************/

bool pdb_set_nt_username(struct samu *sampass, const char *nt_username, enum pdb_value_state flag)
{
	if (nt_username) { 
		DEBUG(10, ("pdb_set_nt_username: setting nt username %s, was %s\n", nt_username,
			(sampass->nt_username)?(sampass->nt_username):"NULL"));
 
		sampass->nt_username = talloc_strdup(sampass, nt_username);

		if (!sampass->nt_username) {
			DEBUG(0, ("pdb_set_nt_username: talloc_strdup() failed!\n"));
			return False;
		}
	} else {
		sampass->nt_username = PDB_NOT_QUITE_NULL;
	}

	return pdb_set_init_flags(sampass, PDB_NTUSERNAME, flag);
}

/*********************************************************************
 Set the user's full name.
 ********************************************************************/

bool pdb_set_fullname(struct samu *sampass, const char *full_name, enum pdb_value_state flag)
{
	if (full_name) { 
		DEBUG(10, ("pdb_set_full_name: setting full name %s, was %s\n", full_name,
			(sampass->full_name)?(sampass->full_name):"NULL"));

		sampass->full_name = talloc_strdup(sampass, full_name);

		if (!sampass->full_name) {
			DEBUG(0, ("pdb_set_fullname: talloc_strdup() failed!\n"));
			return False;
		}
	} else {
		sampass->full_name = PDB_NOT_QUITE_NULL;
	}

	return pdb_set_init_flags(sampass, PDB_FULLNAME, flag);
}

/*********************************************************************
 Set the user's logon script.
 ********************************************************************/

bool pdb_set_logon_script(struct samu *sampass, const char *logon_script, enum pdb_value_state flag)
{
	if (logon_script) { 
		DEBUG(10, ("pdb_set_logon_script: setting logon script %s, was %s\n", logon_script,
			(sampass->logon_script)?(sampass->logon_script):"NULL"));

		sampass->logon_script = talloc_strdup(sampass, logon_script);

		if (!sampass->logon_script) {
			DEBUG(0, ("pdb_set_logon_script: talloc_strdup() failed!\n"));
			return False;
		}
	} else {
		sampass->logon_script = PDB_NOT_QUITE_NULL;
	}

	return pdb_set_init_flags(sampass, PDB_LOGONSCRIPT, flag);
}

/*********************************************************************
 Set the user's profile path.
 ********************************************************************/

bool pdb_set_profile_path(struct samu *sampass, const char *profile_path, enum pdb_value_state flag)
{
	if (profile_path) { 
		DEBUG(10, ("pdb_set_profile_path: setting profile path %s, was %s\n", profile_path,
			(sampass->profile_path)?(sampass->profile_path):"NULL"));

		sampass->profile_path = talloc_strdup(sampass, profile_path);

		if (!sampass->profile_path) {
			DEBUG(0, ("pdb_set_profile_path: talloc_strdup() failed!\n"));
			return False;
		}
	} else {
		sampass->profile_path = PDB_NOT_QUITE_NULL;
	}

	return pdb_set_init_flags(sampass, PDB_PROFILE, flag);
}

/*********************************************************************
 Set the user's directory drive.
 ********************************************************************/

bool pdb_set_dir_drive(struct samu *sampass, const char *dir_drive, enum pdb_value_state flag)
{
	if (dir_drive) { 
		DEBUG(10, ("pdb_set_dir_drive: setting dir drive %s, was %s\n", dir_drive,
			(sampass->dir_drive)?(sampass->dir_drive):"NULL"));

		sampass->dir_drive = talloc_strdup(sampass, dir_drive);

		if (!sampass->dir_drive) {
			DEBUG(0, ("pdb_set_dir_drive: talloc_strdup() failed!\n"));
			return False;
		}

	} else {
		sampass->dir_drive = PDB_NOT_QUITE_NULL;
	}

	return pdb_set_init_flags(sampass, PDB_DRIVE, flag);
}

/*********************************************************************
 Set the user's home directory.
 ********************************************************************/

bool pdb_set_homedir(struct samu *sampass, const char *home_dir, enum pdb_value_state flag)
{
	if (home_dir) { 
		DEBUG(10, ("pdb_set_homedir: setting home dir %s, was %s\n", home_dir,
			(sampass->home_dir)?(sampass->home_dir):"NULL"));

		sampass->home_dir = talloc_strdup(sampass, home_dir);

		if (!sampass->home_dir) {
			DEBUG(0, ("pdb_set_home_dir: talloc_strdup() failed!\n"));
			return False;
		}
	} else {
		sampass->home_dir = PDB_NOT_QUITE_NULL;
	}

	return pdb_set_init_flags(sampass, PDB_SMBHOME, flag);
}

/*********************************************************************
 Set the user's account description.
 ********************************************************************/

bool pdb_set_acct_desc(struct samu *sampass, const char *acct_desc, enum pdb_value_state flag)
{
	if (acct_desc) { 
		sampass->acct_desc = talloc_strdup(sampass, acct_desc);

		if (!sampass->acct_desc) {
			DEBUG(0, ("pdb_set_acct_desc: talloc_strdup() failed!\n"));
			return False;
		}
	} else {
		sampass->acct_desc = PDB_NOT_QUITE_NULL;
	}

	return pdb_set_init_flags(sampass, PDB_ACCTDESC, flag);
}

/*********************************************************************
 Set the user's workstation allowed list.
 ********************************************************************/

bool pdb_set_workstations(struct samu *sampass, const char *workstations, enum pdb_value_state flag)
{
	if (workstations) { 
		DEBUG(10, ("pdb_set_workstations: setting workstations %s, was %s\n", workstations,
			(sampass->workstations)?(sampass->workstations):"NULL"));

		sampass->workstations = talloc_strdup(sampass, workstations);

		if (!sampass->workstations) {
			DEBUG(0, ("pdb_set_workstations: talloc_strdup() failed!\n"));
			return False;
		}
	} else {
		sampass->workstations = PDB_NOT_QUITE_NULL;
	}

	return pdb_set_init_flags(sampass, PDB_WORKSTATIONS, flag);
}

/*********************************************************************
 ********************************************************************/

bool pdb_set_comment(struct samu *sampass, const char *comment, enum pdb_value_state flag)
{
	if (comment) { 
		sampass->comment = talloc_strdup(sampass, comment);

		if (!sampass->comment) {
			DEBUG(0, ("pdb_set_comment: talloc_strdup() failed!\n"));
			return False;
		}
	} else {
		sampass->comment = PDB_NOT_QUITE_NULL;
	}

	return pdb_set_init_flags(sampass, PDB_COMMENT, flag);
}

/*********************************************************************
 Set the user's dial string.
 ********************************************************************/

bool pdb_set_munged_dial(struct samu *sampass, const char *munged_dial, enum pdb_value_state flag)
{
	if (munged_dial) { 
		sampass->munged_dial = talloc_strdup(sampass, munged_dial);

		if (!sampass->munged_dial) {
			DEBUG(0, ("pdb_set_munged_dial: talloc_strdup() failed!\n"));
			return False;
		}
	} else {
		sampass->munged_dial = PDB_NOT_QUITE_NULL;
	}

	return pdb_set_init_flags(sampass, PDB_MUNGEDDIAL, flag);
}

/*********************************************************************
 Set the user's NT hash.
 ********************************************************************/

bool pdb_set_nt_passwd(struct samu *sampass, const uint8 pwd[NT_HASH_LEN], enum pdb_value_state flag)
{
	data_blob_clear_free(&sampass->nt_pw);

       if (pwd) {
               sampass->nt_pw =
		       data_blob_talloc(sampass, pwd, NT_HASH_LEN);
       } else {
               sampass->nt_pw = data_blob_null;
       }

	return pdb_set_init_flags(sampass, PDB_NTPASSWD, flag);
}

/*********************************************************************
 Set the user's LM hash.
 ********************************************************************/

bool pdb_set_lanman_passwd(struct samu *sampass, const uint8 pwd[LM_HASH_LEN], enum pdb_value_state flag)
{
	data_blob_clear_free(&sampass->lm_pw);

	/* on keep the password if we are allowing LANMAN authentication */

	if (pwd && lp_lanman_auth() ) {
		sampass->lm_pw = data_blob_talloc(sampass, pwd, LM_HASH_LEN);
	} else {
		sampass->lm_pw = data_blob_null;
	}

	return pdb_set_init_flags(sampass, PDB_LMPASSWD, flag);
}

/*********************************************************************
 Set the user's password history hash. historyLen is the number of 
 PW_HISTORY_SALT_LEN+SALTED_MD5_HASH_LEN length
 entries to store in the history - this must match the size of the uint8 array
 in pwd.
********************************************************************/

bool pdb_set_pw_history(struct samu *sampass, const uint8 *pwd, uint32_t historyLen, enum pdb_value_state flag)
{
	if (historyLen && pwd){
		data_blob_free(&(sampass->nt_pw_his));
		sampass->nt_pw_his = data_blob_talloc(sampass,
						pwd, historyLen*PW_HISTORY_ENTRY_LEN);
		if (!sampass->nt_pw_his.length) {
			DEBUG(0, ("pdb_set_pw_history: data_blob_talloc() failed!\n"));
			return False;
		}
	} else {
		sampass->nt_pw_his = data_blob_talloc(sampass, NULL, 0);
	}

	return pdb_set_init_flags(sampass, PDB_PWHISTORY, flag);
}

/*********************************************************************
 Set the user's plaintext password only (base procedure, see helper
 below)
 ********************************************************************/

bool pdb_set_plaintext_pw_only(struct samu *sampass, const char *password, enum pdb_value_state flag)
{
	if (password) { 
		if (sampass->plaintext_pw!=NULL) 
			memset(sampass->plaintext_pw,'\0',strlen(sampass->plaintext_pw)+1);

		sampass->plaintext_pw = talloc_strdup(sampass, password);

		if (!sampass->plaintext_pw) {
			DEBUG(0, ("pdb_set_unknown_str: talloc_strdup() failed!\n"));
			return False;
		}
	} else {
		sampass->plaintext_pw = NULL;
	}

	return pdb_set_init_flags(sampass, PDB_PLAINTEXT_PW, flag);
}

bool pdb_set_bad_password_count(struct samu *sampass, uint16_t bad_password_count, enum pdb_value_state flag)
{
	sampass->bad_password_count = bad_password_count;
	return pdb_set_init_flags(sampass, PDB_BAD_PASSWORD_COUNT, flag);
}

bool pdb_set_logon_count(struct samu *sampass, uint16_t logon_count, enum pdb_value_state flag)
{
	sampass->logon_count = logon_count;
	return pdb_set_init_flags(sampass, PDB_LOGON_COUNT, flag);
}

bool pdb_set_country_code(struct samu *sampass, uint16_t country_code,
			  enum pdb_value_state flag)
{
	sampass->country_code = country_code;
	return pdb_set_init_flags(sampass, PDB_COUNTRY_CODE, flag);
}

bool pdb_set_code_page(struct samu *sampass, uint16_t code_page,
		       enum pdb_value_state flag)
{
	sampass->code_page = code_page;
	return pdb_set_init_flags(sampass, PDB_CODE_PAGE, flag);
}

bool pdb_set_unknown_6(struct samu *sampass, uint32_t unkn, enum pdb_value_state flag)
{
	sampass->unknown_6 = unkn;
	return pdb_set_init_flags(sampass, PDB_UNKNOWN6, flag);
}

bool pdb_set_hours(struct samu *sampass, const uint8 *hours, int hours_len,
		   enum pdb_value_state flag)
{
	if (hours_len > sizeof(sampass->hours)) {
		return false;
	}

	if (!hours) {
		memset ((char *)sampass->hours, 0, hours_len);
	} else {
		memcpy (sampass->hours, hours, hours_len);
	}

	return pdb_set_init_flags(sampass, PDB_HOURS, flag);
}

bool pdb_set_backend_private_data(struct samu *sampass, void *private_data, 
				   void (*free_fn)(void **), 
				   const struct pdb_methods *my_methods, 
				   enum pdb_value_state flag)
{
	if (sampass->backend_private_data &&
	    sampass->backend_private_data_free_fn) {
		sampass->backend_private_data_free_fn(
			&sampass->backend_private_data);
	}

	sampass->backend_private_data = private_data;
	sampass->backend_private_data_free_fn = free_fn;
	sampass->backend_private_methods = my_methods;

	return pdb_set_init_flags(sampass, PDB_BACKEND_PRIVATE_DATA, flag);
}


/* Helpful interfaces to the above */

bool pdb_set_pass_can_change(struct samu *sampass, bool canchange)
{
	return pdb_set_pass_can_change_time(sampass, 
				     canchange ? 0 : pdb_password_change_time_max(),
				     PDB_CHANGED);
}


/*********************************************************************
 Set the user's PLAINTEXT password.  Used as an interface to the above.
 Also sets the last change time to NOW.
 ********************************************************************/

bool pdb_set_plaintext_passwd(struct samu *sampass, const char *plaintext)
{
	uchar new_lanman_p16[LM_HASH_LEN];
	uchar new_nt_p16[NT_HASH_LEN];
	uchar *pwhistory;
	uint32_t pwHistLen;
	uint32_t current_history_len;

	if (!plaintext)
		return False;

	/* Calculate the MD4 hash (NT compatible) of the password */
	E_md4hash(plaintext, new_nt_p16);

	if (!pdb_set_nt_passwd (sampass, new_nt_p16, PDB_CHANGED)) 
		return False;

	if (!E_deshash(plaintext, new_lanman_p16)) {
		/* E_deshash returns false for 'long' passwords (> 14
		   DOS chars).  This allows us to match Win2k, which
		   does not store a LM hash for these passwords (which
		   would reduce the effective password length to 14 */

		if (!pdb_set_lanman_passwd (sampass, NULL, PDB_CHANGED)) 
			return False;
	} else {
		if (!pdb_set_lanman_passwd (sampass, new_lanman_p16, PDB_CHANGED)) 
			return False;
	}

	if (!pdb_set_plaintext_pw_only (sampass, plaintext, PDB_CHANGED)) 
		return False;

	if (!pdb_set_pass_last_set_time (sampass, time(NULL), PDB_CHANGED))
		return False;

	if ((pdb_get_acct_ctrl(sampass) & ACB_NORMAL) == 0) {
		/*
		 * No password history for non-user accounts
		 */
		return true;
	}

	pdb_get_account_policy(PDB_POLICY_PASSWORD_HISTORY, &pwHistLen);

	if (pwHistLen == 0) {
		/* Set the history length to zero. */
		pdb_set_pw_history(sampass, NULL, 0, PDB_CHANGED);
		return true;
	}

	/*
	 * We need to make sure we don't have a race condition here -
	 * the account policy history length can change between when
	 * the pw_history was first loaded into the struct samu struct
	 * and now.... JRA.
	 */
	pwhistory = (uchar *)pdb_get_pw_history(sampass, &current_history_len);

	if ((current_history_len != 0) && (pwhistory == NULL)) {
		DEBUG(1, ("pdb_set_plaintext_passwd: pwhistory == NULL!\n"));
		return false;
	}

	if (current_history_len < pwHistLen) {
		/*
		 * Ensure we have space for the needed history. This
		 * also takes care of an account which did not have
		 * any history at all so far, i.e. pwhistory==NULL
		 */
		uchar *new_history = talloc_zero_array(
			sampass, uchar,
			pwHistLen*PW_HISTORY_ENTRY_LEN);

		if (!new_history) {
			return False;
		}

		memcpy(new_history, pwhistory,
		       current_history_len*PW_HISTORY_ENTRY_LEN);

		pwhistory = new_history;
	}

	/*
	 * Make room for the new password in the history list.
	 */
	if (pwHistLen > 1) {
		memmove(&pwhistory[PW_HISTORY_ENTRY_LEN], pwhistory,
			(pwHistLen-1)*PW_HISTORY_ENTRY_LEN );
	}

	/*
	 * Fill the salt area with 0-s: this indicates that
	 * a plain nt hash is stored in the has area.
	 * The old format was to store a 16 byte salt and
	 * then an md5hash of the nt_hash concatenated with
	 * the salt.
	 */
	memset(pwhistory, 0, PW_HISTORY_SALT_LEN);

	/*
	 * Store the plain nt hash in the second 16 bytes.
	 * The old format was to store the md5 hash of
	 * the salt+newpw.
	 */
	memcpy(&pwhistory[PW_HISTORY_SALT_LEN], new_nt_p16, SALTED_MD5_HASH_LEN);

	pdb_set_pw_history(sampass, pwhistory, pwHistLen, PDB_CHANGED);

	return True;
}

/* check for any PDB_SET/CHANGED field and fill the appropriate mask bit */
uint32_t pdb_build_fields_present(struct samu *sampass)
{
	/* value set to all for testing */
	return 0x00ffffff;
}
