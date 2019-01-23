/*
   Unix SMB/CIFS implementation.
   SAMR Pipe utility functions.

   Copyright (C) Luke Kenneth Casson Leighton 	1996-1998
   Copyright (C) Gerald (Jerry) Carter		2000-2001
   Copyright (C) Andrew Bartlett		2001-2002
   Copyright (C) Stefan (metze) Metzmacher	2002
   Copyright (C) Guenther Deschner		2008

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
#include "../librpc/gen_ndr/samr.h"
#include "rpc_server/samr/srv_samr_util.h"
#include "passdb.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_RPC_SRV

#define STRING_CHANGED (old_string && !new_string) ||\
		    (!old_string && new_string) ||\
		(old_string && new_string && (strcmp(old_string, new_string) != 0))

#define STRING_CHANGED_NC(s1,s2) ((s1) && !(s2)) ||\
		    (!(s1) && (s2)) ||\
		((s1) && (s2) && (strcmp((s1), (s2)) != 0))

/*************************************************************
 Copies a struct samr_UserInfo2 to a struct samu
**************************************************************/

void copy_id2_to_sam_passwd(struct samu *to,
			    struct samr_UserInfo2 *from)
{
	struct samr_UserInfo21 i;

	if (from == NULL || to == NULL) {
		return;
	}

	ZERO_STRUCT(i);

	i.fields_present	= SAMR_FIELD_COMMENT |
				  SAMR_FIELD_COUNTRY_CODE |
				  SAMR_FIELD_CODE_PAGE;
	i.comment		= from->comment;
	i.country_code		= from->country_code;
	i.code_page		= from->code_page;

	copy_id21_to_sam_passwd("INFO_2", to, &i);
}

/*************************************************************
 Copies a struct samr_UserInfo4 to a struct samu
**************************************************************/

void copy_id4_to_sam_passwd(struct samu *to,
			    struct samr_UserInfo4 *from)
{
	struct samr_UserInfo21 i;

	if (from == NULL || to == NULL) {
		return;
	}

	ZERO_STRUCT(i);

	i.fields_present	= SAMR_FIELD_LOGON_HOURS;
	i.logon_hours		= from->logon_hours;

	copy_id21_to_sam_passwd("INFO_4", to, &i);
}

/*************************************************************
 Copies a struct samr_UserInfo6 to a struct samu
**************************************************************/

void copy_id6_to_sam_passwd(struct samu *to,
			    struct samr_UserInfo6 *from)
{
	struct samr_UserInfo21 i;

	if (from == NULL || to == NULL) {
		return;
	}

	ZERO_STRUCT(i);

	i.fields_present	= SAMR_FIELD_ACCOUNT_NAME |
				  SAMR_FIELD_FULL_NAME;
	i.account_name		= from->account_name;
	i.full_name		= from->full_name;

	copy_id21_to_sam_passwd("INFO_6", to, &i);
}

/*************************************************************
 Copies a struct samr_UserInfo8 to a struct samu
**************************************************************/

void copy_id8_to_sam_passwd(struct samu *to,
			    struct samr_UserInfo8 *from)
{
	struct samr_UserInfo21 i;

	if (from == NULL || to == NULL) {
		return;
	}

	ZERO_STRUCT(i);

	i.fields_present	= SAMR_FIELD_FULL_NAME;
	i.full_name		= from->full_name;

	copy_id21_to_sam_passwd("INFO_8", to, &i);
}

/*************************************************************
 Copies a struct samr_UserInfo10 to a struct samu
**************************************************************/

void copy_id10_to_sam_passwd(struct samu *to,
			     struct samr_UserInfo10 *from)
{
	struct samr_UserInfo21 i;

	if (from == NULL || to == NULL) {
		return;
	}

	ZERO_STRUCT(i);

	i.fields_present	= SAMR_FIELD_HOME_DIRECTORY |
				  SAMR_FIELD_HOME_DRIVE;
	i.home_directory	= from->home_directory;
	i.home_drive		= from->home_drive;

	copy_id21_to_sam_passwd("INFO_10", to, &i);
}

/*************************************************************
 Copies a struct samr_UserInfo11 to a struct samu
**************************************************************/

void copy_id11_to_sam_passwd(struct samu *to,
			     struct samr_UserInfo11 *from)
{
	struct samr_UserInfo21 i;

	if (from == NULL || to == NULL) {
		return;
	}

	ZERO_STRUCT(i);

	i.fields_present	= SAMR_FIELD_LOGON_SCRIPT;
	i.logon_script		= from->logon_script;

	copy_id21_to_sam_passwd("INFO_11", to, &i);
}

/*************************************************************
 Copies a struct samr_UserInfo12 to a struct samu
**************************************************************/

void copy_id12_to_sam_passwd(struct samu *to,
			     struct samr_UserInfo12 *from)
{
	struct samr_UserInfo21 i;

	if (from == NULL || to == NULL) {
		return;
	}

	ZERO_STRUCT(i);

	i.fields_present	= SAMR_FIELD_PROFILE_PATH;
	i.profile_path		= from->profile_path;

	copy_id21_to_sam_passwd("INFO_12", to, &i);
}

/*************************************************************
 Copies a struct samr_UserInfo13 to a struct samu
**************************************************************/

void copy_id13_to_sam_passwd(struct samu *to,
			     struct samr_UserInfo13 *from)
{
	struct samr_UserInfo21 i;

	if (from == NULL || to == NULL) {
		return;
	}

	ZERO_STRUCT(i);

	i.fields_present	= SAMR_FIELD_DESCRIPTION;
	i.description		= from->description;

	copy_id21_to_sam_passwd("INFO_13", to, &i);
}

/*************************************************************
 Copies a struct samr_UserInfo14 to a struct samu
**************************************************************/

void copy_id14_to_sam_passwd(struct samu *to,
			     struct samr_UserInfo14 *from)
{
	struct samr_UserInfo21 i;

	if (from == NULL || to == NULL) {
		return;
	}

	ZERO_STRUCT(i);

	i.fields_present	= SAMR_FIELD_WORKSTATIONS;
	i.workstations		= from->workstations;

	copy_id21_to_sam_passwd("INFO_14", to, &i);
}

/*************************************************************
 Copies a struct samr_UserInfo16 to a struct samu
**************************************************************/

void copy_id16_to_sam_passwd(struct samu *to,
			     struct samr_UserInfo16 *from)
{
	struct samr_UserInfo21 i;

	if (from == NULL || to == NULL) {
		return;
	}

	ZERO_STRUCT(i);

	i.fields_present	= SAMR_FIELD_ACCT_FLAGS;
	i.acct_flags		= from->acct_flags;

	copy_id21_to_sam_passwd("INFO_16", to, &i);
}

/*************************************************************
 Copies a struct samr_UserInfo17 to a struct samu
**************************************************************/

void copy_id17_to_sam_passwd(struct samu *to,
			     struct samr_UserInfo17 *from)
{
	struct samr_UserInfo21 i;

	if (from == NULL || to == NULL) {
		return;
	}

	ZERO_STRUCT(i);

	i.fields_present	= SAMR_FIELD_ACCT_EXPIRY;
	i.acct_expiry		= from->acct_expiry;

	copy_id21_to_sam_passwd("INFO_17", to, &i);
}

/*************************************************************
 Copies a struct samr_UserInfo18 to a struct samu
**************************************************************/

void copy_id18_to_sam_passwd(struct samu *to,
			     struct samr_UserInfo18 *from)
{
	struct samr_UserInfo21 i;

	if (from == NULL || to == NULL) {
		return;
	}

	ZERO_STRUCT(i);

	i.fields_present	= SAMR_FIELD_EXPIRED_FLAG;
	i.password_expired	= from->password_expired;

	copy_id21_to_sam_passwd("INFO_18", to, &i);
}

/*************************************************************
 Copies a struct samr_UserInfo20 to a struct samu
**************************************************************/

void copy_id20_to_sam_passwd(struct samu *to,
			     struct samr_UserInfo20 *from)
{
	const char *old_string;
	char *new_string;
	DATA_BLOB mung;

	if (from == NULL || to == NULL) {
		return;
	}

	if (from->parameters.array) {
		old_string = pdb_get_munged_dial(to);
		mung = data_blob_const(from->parameters.array,
				       from->parameters.length);
		new_string = (mung.length == 0) ?
			NULL : base64_encode_data_blob(talloc_tos(), mung);
		DEBUG(10,("INFO_20 PARAMETERS: %s -> %s\n",
			old_string, new_string));
		if (STRING_CHANGED_NC(old_string,new_string)) {
			pdb_set_munged_dial(to, new_string, PDB_CHANGED);
		}

		TALLOC_FREE(new_string);
	}
}

/*************************************************************
 Copies a struct samr_UserInfo21 to a struct samu
**************************************************************/

void copy_id21_to_sam_passwd(const char *log_prefix,
			     struct samu *to,
			     struct samr_UserInfo21 *from)
{
	time_t unix_time, stored_time;
	const char *old_string, *new_string;
	const char *l;

	if (from == NULL || to == NULL) {
		return;
	}

	if (log_prefix) {
		l = log_prefix;
	} else {
		l = "INFO_21";
	}

	if (from->fields_present & SAMR_FIELD_LAST_LOGON) {
		unix_time = nt_time_to_unix(from->last_logon);
		stored_time = pdb_get_logon_time(to);
		DEBUG(10,("%s SAMR_FIELD_LAST_LOGON: %lu -> %lu\n", l,
			(long unsigned int)stored_time,
			(long unsigned int)unix_time));
		if (stored_time != unix_time) {
			pdb_set_logon_time(to, unix_time, PDB_CHANGED);
		}
	}

	if (from->fields_present & SAMR_FIELD_LAST_LOGOFF) {
		unix_time = nt_time_to_unix(from->last_logoff);
		stored_time = pdb_get_logoff_time(to);
		DEBUG(10,("%s SAMR_FIELD_LAST_LOGOFF: %lu -> %lu\n", l,
			(long unsigned int)stored_time,
			(long unsigned int)unix_time));
		if (stored_time != unix_time) {
			pdb_set_logoff_time(to, unix_time, PDB_CHANGED);
		}
	}

	if (from->fields_present & SAMR_FIELD_ACCT_EXPIRY) {
		unix_time = nt_time_to_unix(from->acct_expiry);
		stored_time = pdb_get_kickoff_time(to);
		DEBUG(10,("%s SAMR_FIELD_ACCT_EXPIRY: %lu -> %lu\n", l,
			(long unsigned int)stored_time,
			(long unsigned int)unix_time));
		if (stored_time != unix_time) {
			pdb_set_kickoff_time(to, unix_time , PDB_CHANGED);
		}
	}

	if (from->fields_present & SAMR_FIELD_LAST_PWD_CHANGE) {
		unix_time = nt_time_to_unix(from->last_password_change);
		stored_time = pdb_get_pass_last_set_time(to);
		DEBUG(10,("%s SAMR_FIELD_LAST_PWD_CHANGE: %lu -> %lu\n", l,
			(long unsigned int)stored_time,
			(long unsigned int)unix_time));
		if (stored_time != unix_time) {
			pdb_set_pass_last_set_time(to, unix_time, PDB_CHANGED);
		}
	}

	if ((from->fields_present & SAMR_FIELD_ACCOUNT_NAME) &&
	    (from->account_name.string)) {
		old_string = pdb_get_username(to);
		new_string = from->account_name.string;
		DEBUG(10,("%s SAMR_FIELD_ACCOUNT_NAME: %s -> %s\n", l,
			old_string, new_string));
		if (STRING_CHANGED) {
			pdb_set_username(to, new_string, PDB_CHANGED);
		}
	}

	if ((from->fields_present & SAMR_FIELD_FULL_NAME) &&
	    (from->full_name.string)) {
		old_string = pdb_get_fullname(to);
		new_string = from->full_name.string;
		DEBUG(10,("%s SAMR_FIELD_FULL_NAME: %s -> %s\n", l,
			old_string, new_string));
		if (STRING_CHANGED) {
			pdb_set_fullname(to, new_string, PDB_CHANGED);
		}
	}

	if ((from->fields_present & SAMR_FIELD_HOME_DIRECTORY) &&
	    (from->home_directory.string)) {
		old_string = pdb_get_homedir(to);
		new_string = from->home_directory.string;
		DEBUG(10,("%s SAMR_FIELD_HOME_DIRECTORY: %s -> %s\n", l,
			old_string, new_string));
		if (STRING_CHANGED) {
			pdb_set_homedir(to, new_string, PDB_CHANGED);
		}
	}

	if ((from->fields_present & SAMR_FIELD_HOME_DRIVE) &&
	    (from->home_drive.string)) {
		old_string = pdb_get_dir_drive(to);
		new_string = from->home_drive.string;
		DEBUG(10,("%s SAMR_FIELD_HOME_DRIVE: %s -> %s\n", l,
			old_string, new_string));
		if (STRING_CHANGED) {
			pdb_set_dir_drive(to, new_string, PDB_CHANGED);
		}
	}

	if ((from->fields_present & SAMR_FIELD_LOGON_SCRIPT) &&
	    (from->logon_script.string)) {
		old_string = pdb_get_logon_script(to);
		new_string = from->logon_script.string;
		DEBUG(10,("%s SAMR_FIELD_LOGON_SCRIPT: %s -> %s\n", l,
			old_string, new_string));
		if (STRING_CHANGED) {
			pdb_set_logon_script(to  , new_string, PDB_CHANGED);
		}
	}

	if ((from->fields_present & SAMR_FIELD_PROFILE_PATH) &&
	    (from->profile_path.string)) {
		old_string = pdb_get_profile_path(to);
		new_string = from->profile_path.string;
		DEBUG(10,("%s SAMR_FIELD_PROFILE_PATH: %s -> %s\n", l,
			old_string, new_string));
		if (STRING_CHANGED) {
			pdb_set_profile_path(to  , new_string, PDB_CHANGED);
		}
	}

	if ((from->fields_present & SAMR_FIELD_DESCRIPTION) &&
	    (from->description.string)) {
		old_string = pdb_get_acct_desc(to);
		new_string = from->description.string;
		DEBUG(10,("%s SAMR_FIELD_DESCRIPTION: %s -> %s\n", l,
			old_string, new_string));
		if (STRING_CHANGED) {
			pdb_set_acct_desc(to, new_string, PDB_CHANGED);
		}
	}

	if ((from->fields_present & SAMR_FIELD_WORKSTATIONS) &&
	    (from->workstations.string)) {
		old_string = pdb_get_workstations(to);
		new_string = from->workstations.string;
		DEBUG(10,("%s SAMR_FIELD_WORKSTATIONS: %s -> %s\n", l,
			old_string, new_string));
		if (STRING_CHANGED) {
			pdb_set_workstations(to  , new_string, PDB_CHANGED);
		}
	}

	if ((from->fields_present & SAMR_FIELD_COMMENT) &&
	    (from->comment.string)) {
		old_string = pdb_get_comment(to);
		new_string = from->comment.string;
		DEBUG(10,("%s SAMR_FIELD_COMMENT: %s -> %s\n", l,
			old_string, new_string));
		if (STRING_CHANGED) {
			pdb_set_comment(to, new_string, PDB_CHANGED);
		}
	}

	if ((from->fields_present & SAMR_FIELD_PARAMETERS) &&
	    (from->parameters.array)) {
		char *newstr;
		DATA_BLOB mung;
		old_string = pdb_get_munged_dial(to);

		mung = data_blob_const(from->parameters.array,
				       from->parameters.length);
		newstr = (mung.length == 0) ?
			NULL : base64_encode_data_blob(talloc_tos(), mung);
		DEBUG(10,("%s SAMR_FIELD_PARAMETERS: %s -> %s\n", l,
			old_string, newstr));
		if (STRING_CHANGED_NC(old_string,newstr)) {
			pdb_set_munged_dial(to, newstr, PDB_CHANGED);
		}

		TALLOC_FREE(newstr);
	}

	if (from->fields_present & SAMR_FIELD_RID) {
		if (from->rid == 0) {
			DEBUG(10,("%s: Asked to set User RID to 0 !? Skipping change!\n", l));
		} else if (from->rid != pdb_get_user_rid(to)) {
			DEBUG(10,("%s SAMR_FIELD_RID: %u -> %u NOT UPDATED!\n", l,
				pdb_get_user_rid(to), from->rid));
		}
	}

	if (from->fields_present & SAMR_FIELD_PRIMARY_GID) {
		if (from->primary_gid == 0) {
			DEBUG(10,("%s: Asked to set Group RID to 0 !? Skipping change!\n", l));
		} else if (from->primary_gid != pdb_get_group_rid(to)) {
			DEBUG(10,("%s SAMR_FIELD_PRIMARY_GID: %u -> %u\n", l,
				pdb_get_group_rid(to), from->primary_gid));
			pdb_set_group_sid_from_rid(to,
				from->primary_gid, PDB_CHANGED);
		}
	}

	if (from->fields_present & SAMR_FIELD_ACCT_FLAGS) {
		DEBUG(10,("%s SAMR_FIELD_ACCT_FLAGS: %08X -> %08X\n", l,
			pdb_get_acct_ctrl(to), from->acct_flags));
		if (from->acct_flags != pdb_get_acct_ctrl(to)) {

			/* You cannot autolock an unlocked account via
			 * setuserinfo calls, so make sure to remove the
			 * ACB_AUTOLOCK bit here - gd */

			if ((from->acct_flags & ACB_AUTOLOCK) &&
			    !(pdb_get_acct_ctrl(to) & ACB_AUTOLOCK)) {
				from->acct_flags &= ~ACB_AUTOLOCK;
			}

			if (!(from->acct_flags & ACB_AUTOLOCK) &&
			     (pdb_get_acct_ctrl(to) & ACB_AUTOLOCK)) {
				/* We're unlocking a previously locked user. Reset bad password counts.
				   Patch from Jianliang Lu. <Jianliang.Lu@getronics.com> */
				pdb_set_bad_password_count(to, 0, PDB_CHANGED);
				pdb_set_bad_password_time(to, 0, PDB_CHANGED);
			}
			pdb_set_acct_ctrl(to, from->acct_flags, PDB_CHANGED);
		}
	}

	if (from->fields_present & SAMR_FIELD_LOGON_HOURS) {
		char oldstr[44]; /* hours strings are 42 bytes. */
		char newstr[44];
		DEBUG(15,("%s SAMR_FIELD_LOGON_HOURS (units_per_week): %08X -> %08X\n", l,
			pdb_get_logon_divs(to), from->logon_hours.units_per_week));
		if (from->logon_hours.units_per_week != pdb_get_logon_divs(to)) {
			pdb_set_logon_divs(to,
				from->logon_hours.units_per_week, PDB_CHANGED);
		}

		DEBUG(15,("%s SAMR_FIELD_LOGON_HOURS (units_per_week/8): %08X -> %08X\n", l,
			pdb_get_hours_len(to),
			from->logon_hours.units_per_week/8));
		if (from->logon_hours.units_per_week/8 != pdb_get_hours_len(to)) {
			pdb_set_hours_len(to,
				from->logon_hours.units_per_week/8, PDB_CHANGED);
		}

		DEBUG(15,("%s SAMR_FIELD_LOGON_HOURS (bits): %s -> %s\n", l,
			pdb_get_hours(to), from->logon_hours.bits));
		pdb_sethexhours(oldstr, pdb_get_hours(to));
		pdb_sethexhours(newstr, from->logon_hours.bits);
		if (!strequal(oldstr, newstr)) {
			pdb_set_hours(to, from->logon_hours.bits,
				      from->logon_hours.units_per_week/8,
				      PDB_CHANGED);
		}
	}

	if (from->fields_present & SAMR_FIELD_BAD_PWD_COUNT) {
		DEBUG(10,("%s SAMR_FIELD_BAD_PWD_COUNT: %08X -> %08X\n", l,
			pdb_get_bad_password_count(to), from->bad_password_count));
		if (from->bad_password_count != pdb_get_bad_password_count(to)) {
			pdb_set_bad_password_count(to,
				from->bad_password_count, PDB_CHANGED);
		}
	}

	if (from->fields_present & SAMR_FIELD_NUM_LOGONS) {
		DEBUG(10,("%s SAMR_FIELD_NUM_LOGONS: %08X -> %08X\n", l,
			pdb_get_logon_count(to), from->logon_count));
		if (from->logon_count != pdb_get_logon_count(to)) {
			pdb_set_logon_count(to, from->logon_count, PDB_CHANGED);
		}
	}

	/* If the must change flag is set, the last set time goes to zero.
	   the must change and can change fields also do, but they are
	   calculated from policy, not set from the wire */

	if (from->fields_present & SAMR_FIELD_EXPIRED_FLAG) {
		DEBUG(10,("%s SAMR_FIELD_EXPIRED_FLAG: %02X\n", l,
			from->password_expired));
		if (from->password_expired != 0) {
			/* Only allow the set_time to zero (which means
			   "User Must Change Password on Next Login"
			   if the user object allows password change. */
			if (pdb_get_pass_can_change(to)) {
				pdb_set_pass_last_set_time(to, 0, PDB_CHANGED);
			} else {
				DEBUG(10,("%s Disallowing set of 'User Must "
					"Change Password on Next Login' as "
					"user object disallows this.\n", l));
			}
		} else {
			/* A subtlety here: some windows commands will
			   clear the expired flag even though it's not
			   set, and we don't want to reset the time
			   in these caess.  "net user /dom <user> /active:y"
			   for example, to clear an autolocked acct.
			   We must check to see if it's expired first. jmcd */

			uint32_t pwd_max_age = 0;
			time_t now = time(NULL);

			pdb_get_account_policy(PDB_POLICY_MAX_PASSWORD_AGE, &pwd_max_age);

			if (pwd_max_age == (uint32_t)-1 || pwd_max_age == 0) {
				pwd_max_age = get_time_t_max();
			}

			stored_time = pdb_get_pass_last_set_time(to);

			/* we will only *set* a pwdlastset date when
			   a) the last pwdlastset time was 0 (user was forced to
			      change password).
			   b) the users password has not expired. gd. */

			if ((stored_time == 0) ||
			    ((now - stored_time) > pwd_max_age)) {
				pdb_set_pass_last_set_time(to, now, PDB_CHANGED);
			}
		}
	}

	if (from->fields_present & SAMR_FIELD_COUNTRY_CODE) {
		DEBUG(10,("%s SAMR_FIELD_COUNTRY_CODE: %08X -> %08X\n", l,
			pdb_get_country_code(to), from->country_code));
		if (from->country_code != pdb_get_country_code(to)) {
			pdb_set_country_code(to,
				from->country_code, PDB_CHANGED);
		}
	}

	if (from->fields_present & SAMR_FIELD_CODE_PAGE) {
		DEBUG(10,("%s SAMR_FIELD_CODE_PAGE: %08X -> %08X\n", l,
			pdb_get_code_page(to), from->code_page));
		if (from->code_page != pdb_get_code_page(to)) {
			pdb_set_code_page(to,
				from->code_page, PDB_CHANGED);
		}
	}
}


/*************************************************************
 Copies a struct samr_UserInfo23 to a struct samu
**************************************************************/

void copy_id23_to_sam_passwd(struct samu *to,
			     struct samr_UserInfo23 *from)
{
	if (from == NULL || to == NULL) {
		return;
	}

	copy_id21_to_sam_passwd("INFO 23", to, &from->info);
}

/*************************************************************
 Copies a struct samr_UserInfo24 to a struct samu
**************************************************************/

void copy_id24_to_sam_passwd(struct samu *to,
			     struct samr_UserInfo24 *from)
{
	struct samr_UserInfo21 i;

	if (from == NULL || to == NULL) {
		return;
	}

	ZERO_STRUCT(i);

	i.fields_present	= SAMR_FIELD_EXPIRED_FLAG;
	i.password_expired	= from->password_expired;

	copy_id21_to_sam_passwd("INFO_24", to, &i);
}

/*************************************************************
 Copies a struct samr_UserInfo25 to a struct samu
**************************************************************/

void copy_id25_to_sam_passwd(struct samu *to,
			     struct samr_UserInfo25 *from)
{
	if (from == NULL || to == NULL) {
		return;
	}

	copy_id21_to_sam_passwd("INFO_25", to, &from->info);
}

/*************************************************************
 Copies a struct samr_UserInfo26 to a struct samu
**************************************************************/

void copy_id26_to_sam_passwd(struct samu *to,
			     struct samr_UserInfo26 *from)
{
	struct samr_UserInfo21 i;

	if (from == NULL || to == NULL) {
		return;
	}

	ZERO_STRUCT(i);

	i.fields_present	= SAMR_FIELD_EXPIRED_FLAG;
	i.password_expired	= from->password_expired;

	copy_id21_to_sam_passwd("INFO_26", to, &i);
}
