/*
   Unix SMB/CIFS implementation.
   passdb editing frontend

   Copyright (C) Simo Sorce      2000-2009
   Copyright (C) Andrew Bartlett 2001
   Copyright (C) Jelmer Vernooij 2002

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
#include "popt_common.h"
#include "../librpc/gen_ndr/samr.h"
#include "../libcli/security/security.h"
#include "passdb.h"

#define BIT_BACKEND	0x00000004
#define BIT_VERBOSE	0x00000008
#define BIT_SPSTYLE	0x00000010
#define BIT_CAN_CHANGE	0x00000020
#define BIT_MUST_CHANGE	0x00000040
#define BIT_USERSIDS	0x00000080
#define BIT_FULLNAME	0x00000100
#define BIT_HOMEDIR	0x00000200
#define BIT_HDIRDRIVE	0x00000400
#define BIT_LOGSCRIPT	0x00000800
#define BIT_PROFILE	0x00001000
#define BIT_MACHINE	0x00002000
#define BIT_USERDOMAIN	0x00004000
#define BIT_USER	0x00008000
#define BIT_LIST	0x00010000
#define BIT_MODIFY	0x00020000
#define BIT_CREATE	0x00040000
#define BIT_DELETE	0x00080000
#define BIT_ACCPOLICY	0x00100000
#define BIT_ACCPOLVAL	0x00200000
#define BIT_ACCTCTRL	0x00400000
#define BIT_RESERV_7	0x00800000
#define BIT_IMPORT	0x01000000
#define BIT_EXPORT	0x02000000
#define BIT_FIX_INIT    0x04000000
#define BIT_BADPWRESET	0x08000000
#define BIT_LOGONHOURS	0x10000000
#define BIT_KICKOFFTIME	0x20000000
#define BIT_DESCRIPTION 0x40000000

#define MASK_ALWAYS_GOOD	0x0000001F
#define MASK_USER_GOOD		0x60405FE0

static int get_sid_from_cli_string(struct dom_sid *sid, const char *str_sid)
{
	uint32_t rid;

	if (!string_to_sid(sid, str_sid)) {
		/* not a complete sid, may be a RID,
		 * try building a SID */

		if (sscanf(str_sid, "%u", &rid) != 1) {
			fprintf(stderr, "Error passed string is not "
					"a complete SID or RID!\n");
			return -1;
		}
		sid_compose(sid, get_global_sam_sid(), rid);
	}

	return 0;
}

/*********************************************************
 Add all currently available users to another db
 ********************************************************/

static int export_database (struct pdb_methods *in,
                            struct pdb_methods *out,
                            const char *username)
{
	NTSTATUS status;
	struct pdb_search *u_search;
	struct samr_displayentry userentry;

	DEBUG(3, ("export_database: username=\"%s\"\n", username ? username : "(NULL)"));

	u_search = pdb_search_init(talloc_tos(), PDB_USER_SEARCH);
	if (u_search == NULL) {
		DEBUG(0, ("pdb_search_init failed\n"));
		return 1;
	}

	if (!in->search_users(in, u_search, 0)) {
		DEBUG(0, ("Could not start searching users\n"));
		TALLOC_FREE(u_search);
		return 1;
	}

	while (u_search->next_entry(u_search, &userentry)) {
		struct samu *user;
		struct samu *account;
		struct dom_sid user_sid;

		DEBUG(4, ("Processing account %s\n", userentry.account_name));

		if ((username != NULL)
		    && (strcmp(username, userentry.account_name) != 0)) {
			/*
			 * ignore unwanted users
			 */
			continue;
		}

		user = samu_new(talloc_tos());
		if (user == NULL) {
			DEBUG(0, ("talloc failed\n"));
			break;
		}

		sid_compose(&user_sid, get_global_sam_sid(), userentry.rid);

		status = in->getsampwsid(in, user, &user_sid);

		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(2, ("getsampwsid failed: %s\n",
				  nt_errstr(status)));
			TALLOC_FREE(user);
			continue;
		}

		account = samu_new(NULL);
		if (account == NULL) {
			fprintf(stderr, "export_database: Memory allocation "
				"failure!\n");
			TALLOC_FREE( user );
			TALLOC_FREE(u_search);
			return 1;
		}

		printf("Importing account for %s...", user->username);
		status = out->getsampwnam(out, account, user->username);

		if (NT_STATUS_IS_OK(status)) {
			status = out->update_sam_account( out, user );
		} else {
			status = out->add_sam_account(out, user);
		}

		if ( NT_STATUS_IS_OK(status) ) {
			printf( "ok\n");
		} else {
			printf( "failed\n");
		}

		TALLOC_FREE( account );
		TALLOC_FREE( user );
	}

	TALLOC_FREE(u_search);

	return 0;
}

/*********************************************************
 Add all currently available group mappings to another db
 ********************************************************/

static int export_groups (struct pdb_methods *in, struct pdb_methods *out)
{
	GROUP_MAP *maps = NULL;
	size_t i, entries = 0;
	NTSTATUS status;

	status = in->enum_group_mapping(in, get_global_sam_sid(),
			SID_NAME_DOM_GRP, &maps, &entries, False);

	if ( NT_STATUS_IS_ERR(status) ) {
		fprintf(stderr, "Unable to enumerate group map entries.\n");
		return 1;
	}

	for (i=0; i<entries; i++) {
		out->add_group_mapping_entry(out, &(maps[i]));
	}

	SAFE_FREE( maps );

	return 0;
}

/*********************************************************
 Reset account policies to their default values and remove marker
 ********************************************************/

static int reinit_account_policies (void)
{
	int i;

	for (i=1; decode_account_policy_name(i) != NULL; i++) {
		uint32_t policy_value;
		if (!account_policy_get_default(i, &policy_value)) {
			fprintf(stderr, "Can't get default account policy\n");
			return -1;
		}
		if (!account_policy_set(i, policy_value)) {
			fprintf(stderr, "Can't set account policy in tdb\n");
			return -1;
		}
	}

	return 0;
}


/*********************************************************
 Add all currently available account policy from tdb to one backend
 ********************************************************/

static int export_account_policies (struct pdb_methods *in, struct pdb_methods *out) 
{
	int i;

	for ( i=1; decode_account_policy_name(i) != NULL; i++ ) {
		uint32_t policy_value;
		NTSTATUS status;

		status = in->get_account_policy(in, i, &policy_value);

		if ( NT_STATUS_IS_ERR(status) ) {
			fprintf(stderr, "Unable to get account policy from %s\n", in->name);
			return -1;
		}

		status = out->set_account_policy(out, i, policy_value);

		if ( NT_STATUS_IS_ERR(status) ) {
			fprintf(stderr, "Unable to migrate account policy to %s\n", out->name);
			return -1;
		}
	}

	return 0;
}


/*********************************************************
 Print info from sam structure
**********************************************************/

static int print_sam_info (struct samu *sam_pwent, bool verbosity, bool smbpwdstyle)
{
	uid_t uid;
	time_t tmp;

	/* TODO: check if entry is a user or a workstation */
	if (!sam_pwent) return -1;

	if (verbosity) {
		char temp[44];
		const uint8_t *hours;

		printf ("Unix username:        %s\n", pdb_get_username(sam_pwent));
		printf ("NT username:          %s\n", pdb_get_nt_username(sam_pwent));
		printf ("Account Flags:        %s\n", pdb_encode_acct_ctrl(pdb_get_acct_ctrl(sam_pwent), NEW_PW_FORMAT_SPACE_PADDED_LEN));
		printf ("User SID:             %s\n",
			sid_string_tos(pdb_get_user_sid(sam_pwent)));
		printf ("Primary Group SID:    %s\n",
			sid_string_tos(pdb_get_group_sid(sam_pwent)));
		printf ("Full Name:            %s\n", pdb_get_fullname(sam_pwent));
		printf ("Home Directory:       %s\n", pdb_get_homedir(sam_pwent));
		printf ("HomeDir Drive:        %s\n", pdb_get_dir_drive(sam_pwent));
		printf ("Logon Script:         %s\n", pdb_get_logon_script(sam_pwent));
		printf ("Profile Path:         %s\n", pdb_get_profile_path(sam_pwent));
		printf ("Domain:               %s\n", pdb_get_domain(sam_pwent));
		printf ("Account desc:         %s\n", pdb_get_acct_desc(sam_pwent));
		printf ("Workstations:         %s\n", pdb_get_workstations(sam_pwent));
		printf ("Munged dial:          %s\n", pdb_get_munged_dial(sam_pwent));

		tmp = pdb_get_logon_time(sam_pwent);
		printf ("Logon time:           %s\n",
				tmp ? http_timestring(talloc_tos(), tmp) : "0");

		tmp = pdb_get_logoff_time(sam_pwent);
		printf ("Logoff time:          %s\n",
				tmp ? http_timestring(talloc_tos(), tmp) : "0");

		tmp = pdb_get_kickoff_time(sam_pwent);
		printf ("Kickoff time:         %s\n",
				tmp ? http_timestring(talloc_tos(), tmp) : "0");

		tmp = pdb_get_pass_last_set_time(sam_pwent);
		printf ("Password last set:    %s\n",
				tmp ? http_timestring(talloc_tos(), tmp) : "0");

		tmp = pdb_get_pass_can_change_time(sam_pwent);
		printf ("Password can change:  %s\n",
				tmp ? http_timestring(talloc_tos(), tmp) : "0");

		tmp = pdb_get_pass_must_change_time(sam_pwent);
		printf ("Password must change: %s\n",
				tmp ? http_timestring(talloc_tos(), tmp) : "0");

		tmp = pdb_get_bad_password_time(sam_pwent);
		printf ("Last bad password   : %s\n",
				tmp ? http_timestring(talloc_tos(), tmp) : "0");
		printf ("Bad password count  : %d\n",
			pdb_get_bad_password_count(sam_pwent));

		hours = pdb_get_hours(sam_pwent);
		pdb_sethexhours(temp, hours);
		printf ("Logon hours         : %s\n", temp);

	} else if (smbpwdstyle) {
		char lm_passwd[33];
		char nt_passwd[33];

		uid = nametouid(pdb_get_username(sam_pwent));
		pdb_sethexpwd(lm_passwd, pdb_get_lanman_passwd(sam_pwent), pdb_get_acct_ctrl(sam_pwent));
		pdb_sethexpwd(nt_passwd, pdb_get_nt_passwd(sam_pwent), pdb_get_acct_ctrl(sam_pwent));

		printf("%s:%lu:%s:%s:%s:LCT-%08X:\n",
		       pdb_get_username(sam_pwent),
		       (unsigned long)uid,
		       lm_passwd,
		       nt_passwd,
		       pdb_encode_acct_ctrl(pdb_get_acct_ctrl(sam_pwent),NEW_PW_FORMAT_SPACE_PADDED_LEN),
		       (uint32)convert_time_t_to_uint32_t(pdb_get_pass_last_set_time(sam_pwent)));
	} else {
		uid = nametouid(pdb_get_username(sam_pwent));
		printf ("%s:%lu:%s\n", pdb_get_username(sam_pwent), (unsigned long)uid,
			pdb_get_fullname(sam_pwent));
	}

	return 0;
}

/*********************************************************
 Get an Print User Info
**********************************************************/

static int print_user_info(const char *username,
			   bool verbosity, bool smbpwdstyle)
{
	struct samu *sam_pwent = NULL;
	bool bret;
	int ret;

	sam_pwent = samu_new(NULL);
	if (!sam_pwent) {
		return -1;
	}

	bret = pdb_getsampwnam(sam_pwent, username);
	if (!bret) {
		fprintf (stderr, "Username not found!\n");
		TALLOC_FREE(sam_pwent);
		return -1;
	}

	ret = print_sam_info(sam_pwent, verbosity, smbpwdstyle);

	TALLOC_FREE(sam_pwent);
	return ret;
}

/*********************************************************
 List Users
**********************************************************/
static int print_users_list(bool verbosity, bool smbpwdstyle)
{
	struct pdb_search *u_search;
	struct samr_displayentry userentry;
	struct samu *sam_pwent;
	TALLOC_CTX *tosctx;
	struct dom_sid user_sid;
	bool bret;
	int ret;

	tosctx = talloc_tos();
	if (!tosctx) {
		DEBUG(0, ("talloc failed\n"));
		return 1;
	}

	u_search = pdb_search_users(tosctx, 0);
	if (!u_search) {
		DEBUG(0, ("User Search failed!\n"));
		ret = 1;
		goto done;
	}

	while (u_search->next_entry(u_search, &userentry)) {

		sam_pwent = samu_new(tosctx);
		if (sam_pwent == NULL) {
			DEBUG(0, ("talloc failed\n"));
			ret = 1;
			goto done;
		}

		sid_compose(&user_sid, get_global_sam_sid(), userentry.rid);

		bret = pdb_getsampwsid(sam_pwent, &user_sid);
		if (!bret) {
			DEBUG(2, ("getsampwsid failed\n"));
			TALLOC_FREE(sam_pwent);
			continue;
		}

		if (verbosity) {
			printf ("---------------\n");
		}
		print_sam_info(sam_pwent, verbosity, smbpwdstyle);
		TALLOC_FREE(sam_pwent);
	}

	ret = 0;

done:
	TALLOC_FREE(tosctx);
	return ret;
}

/*********************************************************
 Fix a list of Users for uninitialised passwords
**********************************************************/
static int fix_users_list(void)
{
	struct pdb_search *u_search;
	struct samr_displayentry userentry;
	struct samu *sam_pwent;
	TALLOC_CTX *tosctx;
	struct dom_sid user_sid;
	NTSTATUS status;
	bool bret;
	int ret;

	tosctx = talloc_tos();
	if (!tosctx) {
		fprintf(stderr, "Out of memory!\n");
		return 1;
	}

	u_search = pdb_search_users(tosctx, 0);
	if (!u_search) {
		fprintf(stderr, "User Search failed!\n");
		ret = 1;
		goto done;
	}

	while (u_search->next_entry(u_search, &userentry)) {

		sam_pwent = samu_new(tosctx);
		if (sam_pwent == NULL) {
			fprintf(stderr, "Out of memory!\n");
			ret = 1;
			goto done;
		}

		sid_compose(&user_sid, get_global_sam_sid(), userentry.rid);

		bret = pdb_getsampwsid(sam_pwent, &user_sid);
		if (!bret) {
			DEBUG(2, ("getsampwsid failed\n"));
			TALLOC_FREE(sam_pwent);
			continue;
		}

		status = pdb_update_sam_account(sam_pwent);
		if (!NT_STATUS_IS_OK(status)) {
			printf("Update of user %s failed!\n",
			       pdb_get_username(sam_pwent));
		}
		TALLOC_FREE(sam_pwent);
	}

	ret = 0;

done:
	TALLOC_FREE(tosctx);
	return ret;
}

/*********************************************************
 Set User Info
**********************************************************/

static int set_user_info(const char *username, const char *fullname,
			 const char *homedir, const char *acct_desc,
			 const char *drive, const char *script,
			 const char *profile, const char *account_control,
			 const char *user_sid, const char *user_domain,
			 const bool badpw, const bool hours,
			 const char *kickoff_time)
{
	bool updated_autolock = False, updated_badpw = False;
	struct samu *sam_pwent;
	uint8_t hours_array[MAX_HOURS_LEN];
	uint32_t hours_len;
	uint32_t acb_flags;
	uint32_t not_settable;
	uint32_t new_flags;
	struct dom_sid u_sid;
	bool ret;

	sam_pwent = samu_new(NULL);
	if (!sam_pwent) {
		return 1;
	}

	ret = pdb_getsampwnam(sam_pwent, username);
	if (!ret) {
		fprintf (stderr, "Username not found!\n");
		TALLOC_FREE(sam_pwent);
		return -1;
	}

	if (hours) {
		hours_len = pdb_get_hours_len(sam_pwent);
		memset(hours_array, 0xff, hours_len);

		pdb_set_hours(sam_pwent, hours_array, hours_len, PDB_CHANGED);
	}

	if (!pdb_update_autolock_flag(sam_pwent, &updated_autolock)) {
		DEBUG(2,("pdb_update_autolock_flag failed.\n"));
	}

	if (!pdb_update_bad_password_count(sam_pwent, &updated_badpw)) {
		DEBUG(2,("pdb_update_bad_password_count failed.\n"));
	}

	if (fullname)
		pdb_set_fullname(sam_pwent, fullname, PDB_CHANGED);
	if (acct_desc)
		pdb_set_acct_desc(sam_pwent, acct_desc, PDB_CHANGED);
	if (homedir)
		pdb_set_homedir(sam_pwent, homedir, PDB_CHANGED);
	if (drive)
		pdb_set_dir_drive(sam_pwent,drive, PDB_CHANGED);
	if (script)
		pdb_set_logon_script(sam_pwent, script, PDB_CHANGED);
	if (profile)
		pdb_set_profile_path (sam_pwent, profile, PDB_CHANGED);
	if (user_domain)
		pdb_set_domain(sam_pwent, user_domain, PDB_CHANGED);

	if (account_control) {
		not_settable = ~(ACB_DISABLED | ACB_HOMDIRREQ |
				 ACB_PWNOTREQ | ACB_PWNOEXP | ACB_AUTOLOCK);

		new_flags = pdb_decode_acct_ctrl(account_control);

		if (new_flags & not_settable) {
			fprintf(stderr, "Can only set [NDHLX] flags\n");
			TALLOC_FREE(sam_pwent);
			return -1;
		}

		acb_flags = pdb_get_acct_ctrl(sam_pwent);

		pdb_set_acct_ctrl(sam_pwent,
				  (acb_flags & not_settable) | new_flags,
				  PDB_CHANGED);
	}
	if (user_sid) {
		if (get_sid_from_cli_string(&u_sid, user_sid)) {
			fprintf(stderr, "Failed to parse SID\n");
			return -1;
		}
		pdb_set_user_sid(sam_pwent, &u_sid, PDB_CHANGED);
	}

	if (badpw) {
		pdb_set_bad_password_count(sam_pwent, 0, PDB_CHANGED);
		pdb_set_bad_password_time(sam_pwent, 0, PDB_CHANGED);
	}

	if (kickoff_time) {
		char *endptr;
		time_t value = get_time_t_max();

		if (strcmp(kickoff_time, "never") != 0) {
			uint32_t num = strtoul(kickoff_time, &endptr, 10);

			if ((endptr == kickoff_time) || (endptr[0] != '\0')) {
				fprintf(stderr, "Failed to parse kickoff time\n");
				return -1;
			}

			value = convert_uint32_t_to_time_t(num);
		}

		pdb_set_kickoff_time(sam_pwent, value, PDB_CHANGED);
	}

	if (NT_STATUS_IS_OK(pdb_update_sam_account(sam_pwent))) {
		print_user_info(username, True, False);
	} else {
		fprintf (stderr, "Unable to modify entry!\n");
		TALLOC_FREE(sam_pwent);
		return -1;
	}
	TALLOC_FREE(sam_pwent);
	return 0;
}

static int set_machine_info(const char *machinename,
			    const char *account_control,
			    const char *machine_sid)
{
	struct samu *sam_pwent = NULL;
	TALLOC_CTX *tosctx;
	uint32_t acb_flags;
	uint32_t not_settable;
	uint32_t new_flags;
	struct dom_sid m_sid;
	char *name;
	int len;
	bool ret;

	len = strlen(machinename);
	if (len == 0) {
		fprintf(stderr, "No machine name given\n");
		return -1;
	}

	tosctx = talloc_tos();
	if (!tosctx) {
		fprintf(stderr, "Out of memory!\n");
		return -1;
	}

	sam_pwent = samu_new(tosctx);
	if (!sam_pwent) {
		return 1;
	}

	if (machinename[len-1] == '$') {
		name = talloc_strdup(sam_pwent, machinename);
	} else {
		name = talloc_asprintf(sam_pwent, "%s$", machinename);
	}
	if (!name) {
		fprintf(stderr, "Out of memory!\n");
		TALLOC_FREE(sam_pwent);
		return -1;
	}

	strlower_m(name);

	ret = pdb_getsampwnam(sam_pwent, name);
	if (!ret) {
		fprintf (stderr, "Username not found!\n");
		TALLOC_FREE(sam_pwent);
		return -1;
	}

	if (account_control) {
		not_settable = ~(ACB_DISABLED);

		new_flags = pdb_decode_acct_ctrl(account_control);

		if (new_flags & not_settable) {
			fprintf(stderr, "Can only set [D] flags\n");
			TALLOC_FREE(sam_pwent);
			return -1;
		}

		acb_flags = pdb_get_acct_ctrl(sam_pwent);

		pdb_set_acct_ctrl(sam_pwent,
				  (acb_flags & not_settable) | new_flags,
				  PDB_CHANGED);
	}
	if (machine_sid) {
		if (get_sid_from_cli_string(&m_sid, machine_sid)) {
			fprintf(stderr, "Failed to parse SID\n");
			return -1;
		}
		pdb_set_user_sid(sam_pwent, &m_sid, PDB_CHANGED);
	}

	if (NT_STATUS_IS_OK(pdb_update_sam_account(sam_pwent))) {
		print_user_info(name, True, False);
	} else {
		fprintf (stderr, "Unable to modify entry!\n");
		TALLOC_FREE(sam_pwent);
		return -1;
	}
	TALLOC_FREE(sam_pwent);
	return 0;
}

/*********************************************************
 Add New User
**********************************************************/
static int new_user(const char *username, const char *fullname,
		    const char *homedir, const char *drive,
		    const char *script, const char *profile,
		    char *user_sid, bool stdin_get)
{
	char *pwd1 = NULL, *pwd2 = NULL;
	char *err = NULL, *msg = NULL;
	struct samu *sam_pwent = NULL;
	TALLOC_CTX *tosctx;
	NTSTATUS status;
	struct dom_sid u_sid;
	int flags;
	int ret;

	tosctx = talloc_tos();
	if (!tosctx) {
		fprintf(stderr, "Out of memory!\n");
		return -1;
	}

	if (user_sid) {
		if (get_sid_from_cli_string(&u_sid, user_sid)) {
			fprintf(stderr, "Failed to parse SID\n");
			return -1;
		}
	}

	pwd1 = get_pass( "new password:", stdin_get);
	pwd2 = get_pass( "retype new password:", stdin_get);
	if (!pwd1 || !pwd2) {
		fprintf(stderr, "Failed to read passwords.\n");
		return -1;
	}
	ret = strcmp(pwd1, pwd2);
	if (ret != 0) {
		fprintf (stderr, "Passwords do not match!\n");
		goto done;
	}

	flags = LOCAL_ADD_USER | LOCAL_SET_PASSWORD;

	status = local_password_change(username, flags, pwd1, &err, &msg);
	if (!NT_STATUS_IS_OK(status)) {
		if (err) fprintf(stderr, "%s", err);
		ret = -1;
		goto done;
	}

	sam_pwent = samu_new(tosctx);
	if (!sam_pwent) {
		fprintf(stderr, "Out of memory!\n");
		ret = -1;
		goto done;
	}

	if (!pdb_getsampwnam(sam_pwent, username)) {
		fprintf(stderr, "User %s not found!\n", username);
		ret = -1;
		goto done;
	}

	if (fullname)
		pdb_set_fullname(sam_pwent, fullname, PDB_CHANGED);
	if (homedir)
		pdb_set_homedir(sam_pwent, homedir, PDB_CHANGED);
	if (drive)
		pdb_set_dir_drive(sam_pwent, drive, PDB_CHANGED);
	if (script)
		pdb_set_logon_script(sam_pwent, script, PDB_CHANGED);
	if (profile)
		pdb_set_profile_path(sam_pwent, profile, PDB_CHANGED);
	if (user_sid)
		pdb_set_user_sid(sam_pwent, &u_sid, PDB_CHANGED);

	status = pdb_update_sam_account(sam_pwent);
	if (!NT_STATUS_IS_OK(status)) {
		fprintf(stderr,
			"Failed to modify entry for user %s.!\n",
			username);
		ret = -1;
		goto done;
	}

	print_user_info(username, True, False);
	ret = 0;

done:
	if (pwd1) memset(pwd1, 0, strlen(pwd1));
	if (pwd2) memset(pwd2, 0, strlen(pwd2));
	SAFE_FREE(pwd1);
	SAFE_FREE(pwd2);
	SAFE_FREE(err);
	SAFE_FREE(msg);
	TALLOC_FREE(sam_pwent);
	return ret;
}

/*********************************************************
 Add New Machine
**********************************************************/

static int new_machine(const char *machinename, char *machine_sid)
{
	char *err = NULL, *msg = NULL;
	struct samu *sam_pwent = NULL;
	TALLOC_CTX *tosctx;
	NTSTATUS status;
	struct dom_sid m_sid;
	char *compatpwd;
	char *name;
	int flags;
	int len;
	int ret;

	len = strlen(machinename);
	if (len == 0) {
		fprintf(stderr, "No machine name given\n");
		return -1;
	}

	tosctx = talloc_tos();
	if (!tosctx) {
		fprintf(stderr, "Out of memory!\n");
		return -1;
	}

	if (machine_sid) {
		if (get_sid_from_cli_string(&m_sid, machine_sid)) {
			fprintf(stderr, "Failed to parse SID\n");
			return -1;
		}
	}

	compatpwd = talloc_strdup(tosctx, machinename);
	if (!compatpwd) {
		fprintf(stderr, "Out of memory!\n");
		return -1;
	}

	if (machinename[len-1] == '$') {
		name = talloc_strdup(tosctx, machinename);
		compatpwd[len-1] = '\0';
	} else {
		name = talloc_asprintf(tosctx, "%s$", machinename);
	}
	if (!name) {
		fprintf(stderr, "Out of memory!\n");
		return -1;
	}

	strlower_m(name);

	flags = LOCAL_ADD_USER | LOCAL_TRUST_ACCOUNT | LOCAL_SET_PASSWORD;

	status = local_password_change(name, flags, compatpwd, &err, &msg);

	if (!NT_STATUS_IS_OK(status)) {
		if (err) fprintf(stderr, "%s", err);
		ret = -1;
	}

	sam_pwent = samu_new(tosctx);
	if (!sam_pwent) {
		fprintf(stderr, "Out of memory!\n");
		ret = -1;
		goto done;
	}

	if (!pdb_getsampwnam(sam_pwent, name)) {
		fprintf(stderr, "Machine %s not found!\n", name);
		ret = -1;
		goto done;
	}

	if (machine_sid)
		pdb_set_user_sid(sam_pwent, &m_sid, PDB_CHANGED);

	status = pdb_update_sam_account(sam_pwent);
	if (!NT_STATUS_IS_OK(status)) {
		fprintf(stderr,
			"Failed to modify entry for %s.!\n", name);
		ret = -1;
		goto done;
	}

	print_user_info(name, True, False);
	ret = 0;

done:
	SAFE_FREE(err);
	SAFE_FREE(msg);
	TALLOC_FREE(sam_pwent);
	return ret;
}

/*********************************************************
 Delete user entry
**********************************************************/

static int delete_user_entry(const char *username)
{
	struct samu *samaccount;

	samaccount = samu_new(NULL);
	if (!samaccount) {
		fprintf(stderr, "Out of memory!\n");
		return -1;
	}

	if (!pdb_getsampwnam(samaccount, username)) {
		fprintf (stderr,
			 "user %s does not exist in the passdb\n", username);
		TALLOC_FREE(samaccount);
		return -1;
	}

	if (!NT_STATUS_IS_OK(pdb_delete_sam_account(samaccount))) {
		fprintf (stderr, "Unable to delete user %s\n", username);
		TALLOC_FREE(samaccount);
		return -1;
	}

	TALLOC_FREE(samaccount);
	return 0;
}

/*********************************************************
 Delete machine entry
**********************************************************/

static int delete_machine_entry(const char *machinename)
{
	struct samu *samaccount = NULL;
	const char *name;

	if (strlen(machinename) == 0) {
		fprintf(stderr, "No machine name given\n");
		return -1;
	}

	samaccount = samu_new(NULL);
	if (!samaccount) {
		fprintf(stderr, "Out of memory!\n");
		return -1;
	}

	if (machinename[strlen(machinename)-1] != '$') {
		name = talloc_asprintf(samaccount, "%s$", machinename);
	} else {
		name = machinename;
	}

	if (!pdb_getsampwnam(samaccount, name)) {
		fprintf (stderr,
			 "machine %s does not exist in the passdb\n", name);
		return -1;
		TALLOC_FREE(samaccount);
	}

	if (!NT_STATUS_IS_OK(pdb_delete_sam_account(samaccount))) {
		fprintf (stderr, "Unable to delete machine %s\n", name);
		TALLOC_FREE(samaccount);
		return -1;
	}

	TALLOC_FREE(samaccount);
	return 0;
}

/*********************************************************
 Start here.
**********************************************************/

int main (int argc, char **argv)
{
	static int list_users = False;
	static int verbose = False;
	static int spstyle = False;
	static int machine = False;
	static int add_user = False;
	static int delete_user = False;
	static int modify_user = False;
	uint32	setparms, checkparms;
	int opt;
	static char *full_name = NULL;
	static char *acct_desc = NULL;
	static const char *user_name = NULL;
	static char *home_dir = NULL;
	static char *home_drive = NULL;
	static const char *backend = NULL;
	static char *backend_in = NULL;
	static char *backend_out = NULL;
	static int transfer_groups = False;
	static int transfer_account_policies = False;
	static int reset_account_policies = False;
	static int  force_initialised_password = False;
	static char *logon_script = NULL;
	static char *profile_path = NULL;
	static char *user_domain = NULL;
	static char *account_control = NULL;
	static char *account_policy = NULL;
	static char *user_sid = NULL;
	static char *machine_sid = NULL;
	static long int account_policy_value = 0;
	bool account_policy_value_set = False;
	static int badpw_reset = False;
	static int hours_reset = False;
	static char *pwd_time_format = NULL;
	static int pw_from_stdin = False;
	struct pdb_methods *bin, *bout;
	static char *kickoff_time = NULL;
	TALLOC_CTX *frame = talloc_stackframe();
	NTSTATUS status;
	poptContext pc;
	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{"list",	'L', POPT_ARG_NONE, &list_users, 0, "list all users", NULL},
		{"verbose",	'v', POPT_ARG_NONE, &verbose, 0, "be verbose", NULL },
		{"smbpasswd-style",	'w',POPT_ARG_NONE, &spstyle, 0, "give output in smbpasswd style", NULL},
		{"user",	'u', POPT_ARG_STRING, &user_name, 0, "use username", "USER" },
		{"account-desc",	'N', POPT_ARG_STRING, &acct_desc, 0, "set account description", NULL},
		{"fullname",	'f', POPT_ARG_STRING, &full_name, 0, "set full name", NULL},
		{"homedir",	'h', POPT_ARG_STRING, &home_dir, 0, "set home directory", NULL},
		{"drive",	'D', POPT_ARG_STRING, &home_drive, 0, "set home drive", NULL},
		{"script",	'S', POPT_ARG_STRING, &logon_script, 0, "set logon script", NULL},
		{"profile",	'p', POPT_ARG_STRING, &profile_path, 0, "set profile path", NULL},
		{"domain",	'I', POPT_ARG_STRING, &user_domain, 0, "set a users' domain", NULL},
		{"user SID",	'U', POPT_ARG_STRING, &user_sid, 0, "set user SID or RID", NULL},
		{"machine SID",	'M', POPT_ARG_STRING, &machine_sid, 0, "set machine SID or RID", NULL},
		{"create",	'a', POPT_ARG_NONE, &add_user, 0, "create user", NULL},
		{"modify",	'r', POPT_ARG_NONE, &modify_user, 0, "modify user", NULL},
		{"machine",	'm', POPT_ARG_NONE, &machine, 0, "account is a machine account", NULL},
		{"delete",	'x', POPT_ARG_NONE, &delete_user, 0, "delete user", NULL},
		{"backend",	'b', POPT_ARG_STRING, &backend, 0, "use different passdb backend as default backend", NULL},
		{"import",	'i', POPT_ARG_STRING, &backend_in, 0, "import user accounts from this backend", NULL},
		{"export",	'e', POPT_ARG_STRING, &backend_out, 0, "export user accounts to this backend", NULL},
		{"group",	'g', POPT_ARG_NONE, &transfer_groups, 0, "use -i and -e for groups", NULL},
		{"policies",	'y', POPT_ARG_NONE, &transfer_account_policies, 0, "use -i and -e to move account policies between backends", NULL},
		{"policies-reset",	0, POPT_ARG_NONE, &reset_account_policies, 0, "restore default policies", NULL},
		{"account-policy",	'P', POPT_ARG_STRING, &account_policy, 0,"value of an account policy (like maximum password age)",NULL},
		{"value",       'C', POPT_ARG_LONG, &account_policy_value, 'C',"set the account policy to this value", NULL},
		{"account-control",	'c', POPT_ARG_STRING, &account_control, 0, "Values of account control", NULL},
		{"force-initialized-passwords", 0, POPT_ARG_NONE, &force_initialised_password, 0, "Force initialization of corrupt password strings in a passdb backend", NULL},
		{"bad-password-count-reset", 'z', POPT_ARG_NONE, &badpw_reset, 0, "reset bad password count", NULL},
		{"logon-hours-reset", 'Z', POPT_ARG_NONE, &hours_reset, 0, "reset logon hours", NULL},
		{"time-format", 0, POPT_ARG_STRING, &pwd_time_format, 0, "The time format for time parameters", NULL },
		{"password-from-stdin", 't', POPT_ARG_NONE, &pw_from_stdin, 0, "get password from standard in", NULL},
		{"kickoff-time", 'K', POPT_ARG_STRING, &kickoff_time, 0, "set the kickoff time", NULL},
		POPT_COMMON_SAMBA
		POPT_TABLEEND
	};

	bin = bout = NULL;

	load_case_tables();

	setup_logging("pdbedit", DEBUG_STDOUT);

	pc = poptGetContext(NULL, argc, (const char **) argv, long_options,
			    POPT_CONTEXT_KEEP_FIRST);

	while((opt = poptGetNextOpt(pc)) != -1) {
		switch (opt) {
		case 'C':
			account_policy_value_set = True;
			break;
		}
	}

	poptGetArg(pc); /* Drop argv[0], the program name */

	if (user_name == NULL)
		user_name = poptGetArg(pc);

	if (!lp_load(get_dyn_CONFIGFILE(),True,False,False,True)) {
		fprintf(stderr, "Can't load %s - run testparm to debug it\n", get_dyn_CONFIGFILE());
		exit(1);
	}

	if (!init_names())
		exit(1);

	setparms =	(backend ? BIT_BACKEND : 0) +
			(verbose ? BIT_VERBOSE : 0) +
			(spstyle ? BIT_SPSTYLE : 0) +
			(full_name ? BIT_FULLNAME : 0) +
			(home_dir ? BIT_HOMEDIR : 0) +
			(home_drive ? BIT_HDIRDRIVE : 0) +
			(logon_script ? BIT_LOGSCRIPT : 0) +
			(profile_path ? BIT_PROFILE : 0) +
			(user_domain ? BIT_USERDOMAIN : 0) +
			(machine ? BIT_MACHINE : 0) +
			(user_name ? BIT_USER : 0) +
			(list_users ? BIT_LIST : 0) +
			(force_initialised_password ? BIT_FIX_INIT : 0) +
			(user_sid ? BIT_USERSIDS : 0) +
			(machine_sid ? BIT_USERSIDS : 0) +
			(modify_user ? BIT_MODIFY : 0) +
			(add_user ? BIT_CREATE : 0) +
			(delete_user ? BIT_DELETE : 0) +
			(account_control ? BIT_ACCTCTRL : 0) +
			(account_policy ? BIT_ACCPOLICY : 0) +
			(account_policy_value_set ? BIT_ACCPOLVAL : 0) +
			(backend_in ? BIT_IMPORT : 0) +
			(backend_out ? BIT_EXPORT : 0) +
			(badpw_reset ? BIT_BADPWRESET : 0) +
			(hours_reset ? BIT_LOGONHOURS : 0) +
			(kickoff_time ? BIT_KICKOFFTIME : 0) +
			(acct_desc ? BIT_DESCRIPTION : 0);

	if (setparms & BIT_BACKEND) {
		/* HACK: set the global passdb backend by overwriting globals.
		 * This way we can use regular pdb functions for default
		 * operations that do not involve passdb migrations */
		lp_set_passdb_backend(backend);
	} else {
		backend = lp_passdb_backend();
	}

	if (!initialize_password_db(False, NULL)) {
		fprintf(stderr, "Can't initialize passdb backend.\n");
		exit(1);
	}

	/* the lowest bit options are always accepted */
	checkparms = setparms & ~MASK_ALWAYS_GOOD;

	if (checkparms & BIT_FIX_INIT) {
		return fix_users_list();
	}

	/* account policy operations */
	if ((checkparms & BIT_ACCPOLICY) && !(checkparms & ~(BIT_ACCPOLICY + BIT_ACCPOLVAL))) {
		uint32_t value;
		enum pdb_policy_type field = account_policy_name_to_typenum(account_policy);
		if (field == 0) {
			const char **names;
			int count;
			int i;
			account_policy_names_list(&names, &count);
			fprintf(stderr, "No account policy by that name!\n");
			if (count !=0) {
				fprintf(stderr, "Account policy names are:\n");
				for (i = 0; i < count ; i++) {
                        		d_fprintf(stderr, "%s\n", names[i]);
				}
			}
			SAFE_FREE(names);
			exit(1);
		}
		if (!pdb_get_account_policy(field, &value)) {
			fprintf(stderr, "valid account policy, but unable to fetch value!\n");
			if (!account_policy_value_set)
				exit(1);
		}
		printf("account policy \"%s\" description: %s\n", account_policy, account_policy_get_desc(field));
		if (account_policy_value_set) {
			printf("account policy \"%s\" value was: %u\n", account_policy, value);
			if (!pdb_set_account_policy(field, account_policy_value)) {
				fprintf(stderr, "valid account policy, but unable to set value!\n");
				exit(1);
			}
			printf("account policy \"%s\" value is now: %lu\n", account_policy, account_policy_value);
			exit(0);
		} else {
			printf("account policy \"%s\" value is: %u\n", account_policy, value);
			exit(0);
		}
	}

	if (reset_account_policies) {
		if (!reinit_account_policies()) {
			exit(1);
		}

		exit(0);
	}

	/* import and export operations */

	if (((checkparms & BIT_IMPORT) ||
	     (checkparms & BIT_EXPORT)) &&
	    !(checkparms & ~(BIT_IMPORT +BIT_EXPORT +BIT_USER))) {

		if (backend_in) {
			status = make_pdb_method_name(&bin, backend_in);
		} else {
			status = make_pdb_method_name(&bin, backend);
		}

		if (!NT_STATUS_IS_OK(status)) {
			fprintf(stderr, "Unable to initialize %s.\n",
				backend_in ? backend_in : backend);
			return 1;
		}

		if (backend_out) {
			status = make_pdb_method_name(&bout, backend_out);
		} else {
			status = make_pdb_method_name(&bout, backend);
		}

		if (!NT_STATUS_IS_OK(status)) {
			fprintf(stderr, "Unable to initialize %s.\n",
				backend_out ? backend_out : backend);
			return 1;
		}

		if (transfer_account_policies) {

			if (!(checkparms & BIT_USER)) {
				return export_account_policies(bin, bout);
			}

		} else 	if (transfer_groups) {

			if (!(checkparms & BIT_USER)) {
				return export_groups(bin, bout);
			}

		} else {
			return export_database(bin, bout,
				(checkparms & BIT_USER) ? user_name : NULL);
		}
	}

	/* if BIT_USER is defined but nothing else then threat it as -l -u for compatibility */
	/* fake up BIT_LIST if only BIT_USER is defined */
	if ((checkparms & BIT_USER) && !(checkparms & ~BIT_USER)) {
		checkparms += BIT_LIST;
	}

	/* modify flag is optional to maintain backwards compatibility */
	/* fake up BIT_MODIFY if BIT_USER  and at least one of MASK_USER_GOOD is defined */
	if (!((checkparms & ~MASK_USER_GOOD) & ~BIT_USER) && (checkparms & MASK_USER_GOOD)) {
		checkparms += BIT_MODIFY;
	}

	/* list users operations */
	if (checkparms & BIT_LIST) {
		if (!(checkparms & ~BIT_LIST)) {
			return print_users_list(verbose, spstyle);
		}
		if (!(checkparms & ~(BIT_USER + BIT_LIST))) {
			return print_user_info(user_name, verbose, spstyle);
		}
	}

	/* mask out users options */
	checkparms &= ~MASK_USER_GOOD;

	/* if bad password count is reset, we must be modifying */
	if (checkparms & BIT_BADPWRESET) {
		checkparms |= BIT_MODIFY;
		checkparms &= ~BIT_BADPWRESET;
	}

	/* if logon hours is reset, must modify */
	if (checkparms & BIT_LOGONHOURS) {
		checkparms |= BIT_MODIFY;
		checkparms &= ~BIT_LOGONHOURS;
	}

	/* account operation */
	if ((checkparms & BIT_CREATE) || (checkparms & BIT_MODIFY) || (checkparms & BIT_DELETE)) {
		/* check use of -u option */
		if (!(checkparms & BIT_USER)) {
			fprintf (stderr, "Username not specified! (use -u option)\n");
			return -1;
		}

		/* account creation operations */
		if (!(checkparms & ~(BIT_CREATE + BIT_USER + BIT_MACHINE))) {
		       	if (checkparms & BIT_MACHINE) {
				return new_machine(user_name, machine_sid);
			} else {
				return new_user(user_name, full_name,
						home_dir, home_drive,
						logon_script, profile_path,
						user_sid, pw_from_stdin);
			}
		}

		/* account deletion operations */
		if (!(checkparms & ~(BIT_DELETE + BIT_USER + BIT_MACHINE))) {
		       	if (checkparms & BIT_MACHINE) {
				return delete_machine_entry(user_name);
			} else {
				return delete_user_entry(user_name);
			}
		}

		/* account modification operations */
		if (!(checkparms & ~(BIT_MODIFY + BIT_USER + BIT_MACHINE))) {
			if (checkparms & BIT_MACHINE) {
				return set_machine_info(user_name,
							account_control,
							machine_sid);
			} else {
				return set_user_info(user_name, full_name,
						     home_dir, acct_desc,
						     home_drive, logon_script,
						     profile_path, account_control,
						     user_sid, user_domain,
						     badpw_reset, hours_reset,
						     kickoff_time);
			}
		}
	}

	if (setparms >= 0x20) {
		fprintf (stderr, "Incompatible or insufficient options on command line!\n");
	}
	poptPrintHelp(pc, stderr, 0);

	TALLOC_FREE(frame);
	return 1;
}
