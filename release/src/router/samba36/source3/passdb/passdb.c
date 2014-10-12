/* 
   Unix SMB/CIFS implementation.
   Password and authentication handling
   Copyright (C) Jeremy Allison 		1996-2001
   Copyright (C) Luke Kenneth Casson Leighton 	1996-1998
   Copyright (C) Gerald (Jerry) Carter		2000-2006
   Copyright (C) Andrew Bartlett		2001-2002
   Copyright (C) Simo Sorce			2003
   Copyright (C) Volker Lendecke 		2006

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
#include "system/passwd.h"
#include "../libcli/auth/libcli_auth.h"
#include "secrets.h"
#include "../libcli/security/security.h"
#include "../lib/util/util_pw.h"
#include "util_tdb.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_PASSDB

/******************************************************************
 Get the default domain/netbios name to be used when
 testing authentication.

 LEGACY: this function provides the legacy domain mapping used with
	 the lp_map_untrusted_to_domain() parameter
******************************************************************/

const char *my_sam_name(void)
{
       /* Standalone servers can only use the local netbios name */
       if ( lp_server_role() == ROLE_STANDALONE )
               return global_myname();

       /* Default to the DOMAIN name when not specified */
       return lp_workgroup();
}

/**********************************************************************
***********************************************************************/

static int samu_destroy(struct samu *user) 
{
	data_blob_clear_free( &user->lm_pw );
	data_blob_clear_free( &user->nt_pw );

	if ( user->plaintext_pw )
		memset( user->plaintext_pw, 0x0, strlen(user->plaintext_pw) );

	return 0;
}

/**********************************************************************
 generate a new struct samuser
***********************************************************************/

struct samu *samu_new( TALLOC_CTX *ctx )
{
	struct samu *user;

	if ( !(user = TALLOC_ZERO_P( ctx, struct samu )) ) {
		DEBUG(0,("samuser_new: Talloc failed!\n"));
		return NULL;
	}

	talloc_set_destructor( user, samu_destroy );

	/* no initial methods */

	user->methods = NULL;

        /* Don't change these timestamp settings without a good reason.
           They are important for NT member server compatibility. */

	user->logon_time            = (time_t)0;
	user->pass_last_set_time    = (time_t)0;
	user->pass_can_change_time  = (time_t)0;
	user->logoff_time           = get_time_t_max();
	user->kickoff_time          = get_time_t_max();
	user->pass_must_change_time = get_time_t_max();
	user->fields_present        = 0x00ffffff;
	user->logon_divs = 168; 	/* hours per week */
	user->hours_len = 21; 		/* 21 times 8 bits = 168 */
	memset(user->hours, 0xff, user->hours_len); /* available at all hours */
	user->bad_password_count = 0;
	user->logon_count = 0;
	user->unknown_6 = 0x000004ec; /* don't know */

	/* Some parts of samba strlen their pdb_get...() returns, 
	   so this keeps the interface unchanged for now. */

	user->username = "";
	user->domain = "";
	user->nt_username = "";
	user->full_name = "";
	user->home_dir = "";
	user->logon_script = "";
	user->profile_path = "";
	user->acct_desc = "";
	user->workstations = "";
	user->comment = "";
	user->munged_dial = "";

	user->plaintext_pw = NULL;

	/* Unless we know otherwise have a Account Control Bit
	   value of 'normal user'.  This helps User Manager, which
	   asks for a filtered list of users. */

	user->acct_ctrl = ACB_NORMAL;

	return user;
}

static int count_commas(const char *str)
{
	int num_commas = 0;
	const char *comma = str;

	while ((comma = strchr(comma, ',')) != NULL) {
		comma += 1;
		num_commas += 1;
	}
	return num_commas;
}

/*********************************************************************
 Initialize a struct samu from a struct passwd including the user 
 and group SIDs.  The *user structure is filled out with the Unix
 attributes and a user SID.
*********************************************************************/

static NTSTATUS samu_set_unix_internal(struct samu *user, const struct passwd *pwd, bool create)
{
	const char *guest_account = lp_guestaccount();
	const char *domain = global_myname();
	char *fullname;
	uint32_t urid;

	if ( !pwd ) {
		return NT_STATUS_NO_SUCH_USER;
	}

	/* Basic properties based upon the Unix account information */

	pdb_set_username(user, pwd->pw_name, PDB_SET);

	fullname = NULL;

	if (count_commas(pwd->pw_gecos) == 3) {
		/*
		 * Heuristic: This seems to be a gecos field that has been
		 * edited by chfn(1). Only use the part before the first
		 * comma. Fixes bug 5198.
		 */
		fullname = talloc_strndup(
			talloc_tos(), pwd->pw_gecos,
			strchr(pwd->pw_gecos, ',') - pwd->pw_gecos);
	}

	if (fullname != NULL) {
		pdb_set_fullname(user, fullname, PDB_SET);
	} else {
		pdb_set_fullname(user, pwd->pw_gecos, PDB_SET);
	}
	TALLOC_FREE(fullname);

	pdb_set_domain (user, get_global_sam_name(), PDB_DEFAULT);
#if 0
	/* This can lead to a primary group of S-1-22-2-XX which 
	   will be rejected by other parts of the Samba code. 
	   Rely on pdb_get_group_sid() to "Do The Right Thing" (TM)  
	   --jerry */

	gid_to_sid(&group_sid, pwd->pw_gid);
	pdb_set_group_sid(user, &group_sid, PDB_SET);
#endif

	/* save the password structure for later use */

	user->unix_pw = tcopy_passwd( user, pwd );

	/* Special case for the guest account which must have a RID of 501 */

	if ( strequal( pwd->pw_name, guest_account ) ) {
		if ( !pdb_set_user_sid_from_rid(user, DOMAIN_RID_GUEST, PDB_DEFAULT)) {
			return NT_STATUS_NO_SUCH_USER;
		}
		return NT_STATUS_OK;
	}

	/* Non-guest accounts...Check for a workstation or user account */

	if (pwd->pw_name[strlen(pwd->pw_name)-1] == '$') {
		/* workstation */

		if (!pdb_set_acct_ctrl(user, ACB_WSTRUST, PDB_DEFAULT)) {
			DEBUG(1, ("Failed to set 'workstation account' flags for user %s.\n", 
				pwd->pw_name));
			return NT_STATUS_INVALID_COMPUTER_NAME;
		}	
	} 
	else {
		/* user */

		if (!pdb_set_acct_ctrl(user, ACB_NORMAL, PDB_DEFAULT)) {
			DEBUG(1, ("Failed to set 'normal account' flags for user %s.\n", 
				pwd->pw_name));
			return NT_STATUS_INVALID_ACCOUNT_NAME;
		}

		/* set some basic attributes */

		pdb_set_profile_path(user, talloc_sub_specified(user, 
			lp_logon_path(), pwd->pw_name, domain, pwd->pw_uid, pwd->pw_gid), 
			PDB_DEFAULT);		
		pdb_set_homedir(user, talloc_sub_specified(user, 
			lp_logon_home(), pwd->pw_name, domain, pwd->pw_uid, pwd->pw_gid),
			PDB_DEFAULT);
		pdb_set_dir_drive(user, talloc_sub_specified(user, 
			lp_logon_drive(), pwd->pw_name, domain, pwd->pw_uid, pwd->pw_gid),
			PDB_DEFAULT);
		pdb_set_logon_script(user, talloc_sub_specified(user, 
			lp_logon_script(), pwd->pw_name, domain, pwd->pw_uid, pwd->pw_gid), 
			PDB_DEFAULT);
	}

	/* Now deal with the user SID.  If we have a backend that can generate 
	   RIDs, then do so.  But sometimes the caller just wanted a structure 
	   initialized and will fill in these fields later (such as from a 
	   netr_SamInfo3 structure) */

	if ( create && (pdb_capabilities() & PDB_CAP_STORE_RIDS)) {
		uint32_t user_rid;
		struct dom_sid user_sid;

		if ( !pdb_new_rid( &user_rid ) ) {
			DEBUG(3, ("Could not allocate a new RID\n"));
			return NT_STATUS_ACCESS_DENIED;
		}

		sid_compose(&user_sid, get_global_sam_sid(), user_rid);

		if ( !pdb_set_user_sid(user, &user_sid, PDB_SET) ) {
			DEBUG(3, ("pdb_set_user_sid failed\n"));
			return NT_STATUS_INTERNAL_ERROR;
		}

		return NT_STATUS_OK;
	}

	/* generate a SID for the user with the RID algorithm */

	urid = algorithmic_pdb_uid_to_user_rid( user->unix_pw->pw_uid );

	if ( !pdb_set_user_sid_from_rid( user, urid, PDB_SET) ) {
		return NT_STATUS_INTERNAL_ERROR;
	}

	return NT_STATUS_OK;
}

/********************************************************************
 Set the Unix user attributes
********************************************************************/

NTSTATUS samu_set_unix(struct samu *user, const struct passwd *pwd)
{
	return samu_set_unix_internal( user, pwd, False );
}

NTSTATUS samu_alloc_rid_unix(struct samu *user, const struct passwd *pwd)
{
	return samu_set_unix_internal( user, pwd, True );
}

/**********************************************************
 Encode the account control bits into a string.
 length = length of string to encode into (including terminating
 null). length *MUST BE MORE THAN 2* !
 **********************************************************/

char *pdb_encode_acct_ctrl(uint32_t acct_ctrl, size_t length)
{
	fstring acct_str;
	char *result;

	size_t i = 0;

	SMB_ASSERT(length <= sizeof(acct_str));

	acct_str[i++] = '[';

	if (acct_ctrl & ACB_PWNOTREQ ) acct_str[i++] = 'N';
	if (acct_ctrl & ACB_DISABLED ) acct_str[i++] = 'D';
	if (acct_ctrl & ACB_HOMDIRREQ) acct_str[i++] = 'H';
	if (acct_ctrl & ACB_TEMPDUP  ) acct_str[i++] = 'T'; 
	if (acct_ctrl & ACB_NORMAL   ) acct_str[i++] = 'U';
	if (acct_ctrl & ACB_MNS      ) acct_str[i++] = 'M';
	if (acct_ctrl & ACB_WSTRUST  ) acct_str[i++] = 'W';
	if (acct_ctrl & ACB_SVRTRUST ) acct_str[i++] = 'S';
	if (acct_ctrl & ACB_AUTOLOCK ) acct_str[i++] = 'L';
	if (acct_ctrl & ACB_PWNOEXP  ) acct_str[i++] = 'X';
	if (acct_ctrl & ACB_DOMTRUST ) acct_str[i++] = 'I';

	for ( ; i < length - 2 ; i++ )
		acct_str[i] = ' ';

	i = length - 2;
	acct_str[i++] = ']';
	acct_str[i++] = '\0';

	result = talloc_strdup(talloc_tos(), acct_str);
	SMB_ASSERT(result != NULL);
	return result;
}     

/**********************************************************
 Decode the account control bits from a string.
 **********************************************************/

uint32_t pdb_decode_acct_ctrl(const char *p)
{
	uint32_t acct_ctrl = 0;
	bool finished = false;

	/*
	 * Check if the account type bits have been encoded after the
	 * NT password (in the form [NDHTUWSLXI]).
	 */

	if (*p != '[')
		return 0;

	for (p++; *p && !finished; p++) {
		switch (*p) {
			case 'N': { acct_ctrl |= ACB_PWNOTREQ ; break; /* 'N'o password. */ }
			case 'D': { acct_ctrl |= ACB_DISABLED ; break; /* 'D'isabled. */ }
			case 'H': { acct_ctrl |= ACB_HOMDIRREQ; break; /* 'H'omedir required. */ }
			case 'T': { acct_ctrl |= ACB_TEMPDUP  ; break; /* 'T'emp account. */ } 
			case 'U': { acct_ctrl |= ACB_NORMAL   ; break; /* 'U'ser account (normal). */ } 
			case 'M': { acct_ctrl |= ACB_MNS      ; break; /* 'M'NS logon user account. What is this ? */ } 
			case 'W': { acct_ctrl |= ACB_WSTRUST  ; break; /* 'W'orkstation account. */ } 
			case 'S': { acct_ctrl |= ACB_SVRTRUST ; break; /* 'S'erver account. */ } 
			case 'L': { acct_ctrl |= ACB_AUTOLOCK ; break; /* 'L'ocked account. */ } 
			case 'X': { acct_ctrl |= ACB_PWNOEXP  ; break; /* No 'X'piry on password */ } 
			case 'I': { acct_ctrl |= ACB_DOMTRUST ; break; /* 'I'nterdomain trust account. */ }
            case ' ': { break; }
			case ':':
			case '\n':
			case '\0': 
			case ']':
			default:  { finished = true; }
		}
	}

	return acct_ctrl;
}

/*************************************************************
 Routine to set 32 hex password characters from a 16 byte array.
**************************************************************/

void pdb_sethexpwd(char p[33], const unsigned char *pwd, uint32_t acct_ctrl)
{
	if (pwd != NULL) {
		int i;
		for (i = 0; i < 16; i++)
			slprintf(&p[i*2], 3, "%02X", pwd[i]);
	} else {
		if (acct_ctrl & ACB_PWNOTREQ)
			safe_strcpy(p, "NO PASSWORDXXXXXXXXXXXXXXXXXXXXX", 32);
		else
			safe_strcpy(p, "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX", 32);
	}
}

/*************************************************************
 Routine to get the 32 hex characters and turn them
 into a 16 byte array.
**************************************************************/

bool pdb_gethexpwd(const char *p, unsigned char *pwd)
{
	int i;
	unsigned char   lonybble, hinybble;
	const char      *hexchars = "0123456789ABCDEF";
	char           *p1, *p2;

	if (!p)
		return false;

	for (i = 0; i < 32; i += 2) {
		hinybble = toupper_m(p[i]);
		lonybble = toupper_m(p[i + 1]);

		p1 = strchr(hexchars, hinybble);
		p2 = strchr(hexchars, lonybble);

		if (!p1 || !p2)
			return false;

		hinybble = PTR_DIFF(p1, hexchars);
		lonybble = PTR_DIFF(p2, hexchars);

		pwd[i / 2] = (hinybble << 4) | lonybble;
	}
	return true;
}

/*************************************************************
 Routine to set 42 hex hours characters from a 21 byte array.
**************************************************************/

void pdb_sethexhours(char *p, const unsigned char *hours)
{
	if (hours != NULL) {
		int i;
		for (i = 0; i < 21; i++) {
			slprintf(&p[i*2], 3, "%02X", hours[i]);
		}
	} else {
		safe_strcpy(p, "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF", 43);
	}
}

/*************************************************************
 Routine to get the 42 hex characters and turn them
 into a 21 byte array.
**************************************************************/

bool pdb_gethexhours(const char *p, unsigned char *hours)
{
	int i;
	unsigned char   lonybble, hinybble;
	const char      *hexchars = "0123456789ABCDEF";
	char           *p1, *p2;

	if (!p) {
		return (False);
	}

	for (i = 0; i < 42; i += 2) {
		hinybble = toupper_m(p[i]);
		lonybble = toupper_m(p[i + 1]);

		p1 = strchr(hexchars, hinybble);
		p2 = strchr(hexchars, lonybble);

		if (!p1 || !p2) {
			return (False);
		}

		hinybble = PTR_DIFF(p1, hexchars);
		lonybble = PTR_DIFF(p2, hexchars);

		hours[i / 2] = (hinybble << 4) | lonybble;
	}
	return (True);
}

/********************************************************************
********************************************************************/

int algorithmic_rid_base(void)
{
	int rid_offset;

	rid_offset = lp_algorithmic_rid_base();

	if (rid_offset < BASE_RID) {  
		/* Try to prevent admin foot-shooting, we can't put algorithmic
		   rids below 1000, that's the 'well known RIDs' on NT */
		DEBUG(0, ("'algorithmic rid base' must be equal to or above %ld\n", BASE_RID));
		rid_offset = BASE_RID;
	}
	if (rid_offset & 1) {
		DEBUG(0, ("algorithmic rid base must be even\n"));
		rid_offset += 1;
	}
	return rid_offset;
}

/*******************************************************************
 Converts NT user RID to a UNIX uid.
 ********************************************************************/

uid_t algorithmic_pdb_user_rid_to_uid(uint32_t user_rid)
{
	int rid_offset = algorithmic_rid_base();
	return (uid_t)(((user_rid & (~USER_RID_TYPE)) - rid_offset)/RID_MULTIPLIER);
}

uid_t max_algorithmic_uid(void)
{
	return algorithmic_pdb_user_rid_to_uid(0xfffffffe);
}

/*******************************************************************
 converts UNIX uid to an NT User RID.
 ********************************************************************/

uint32_t algorithmic_pdb_uid_to_user_rid(uid_t uid)
{
	int rid_offset = algorithmic_rid_base();
	return (((((uint32_t)uid)*RID_MULTIPLIER) + rid_offset) | USER_RID_TYPE);
}

/*******************************************************************
 Converts NT group RID to a UNIX gid.
 ********************************************************************/

gid_t pdb_group_rid_to_gid(uint32_t group_rid)
{
	int rid_offset = algorithmic_rid_base();
	return (gid_t)(((group_rid & (~GROUP_RID_TYPE))- rid_offset)/RID_MULTIPLIER);
}

gid_t max_algorithmic_gid(void)
{
	return pdb_group_rid_to_gid(0xffffffff);
}

/*******************************************************************
 converts NT Group RID to a UNIX uid.
 
 warning: you must not call that function only
 you must do a call to the group mapping first.
 there is not anymore a direct link between the gid and the rid.
 ********************************************************************/

uint32_t algorithmic_pdb_gid_to_group_rid(gid_t gid)
{
	int rid_offset = algorithmic_rid_base();
	return (((((uint32_t)gid)*RID_MULTIPLIER) + rid_offset) | GROUP_RID_TYPE);
}

/*******************************************************************
 Decides if a RID is a well known RID.
 ********************************************************************/

static bool rid_is_well_known(uint32_t rid)
{
	/* Not using rid_offset here, because this is the actual
	   NT fixed value (1000) */

	return (rid < BASE_RID);
}

/*******************************************************************
 Decides if a RID is a user or group RID.
 ********************************************************************/

bool algorithmic_pdb_rid_is_user(uint32_t rid)
{
	if ( rid_is_well_known(rid) ) {
		/*
		 * The only well known user RIDs are DOMAIN_RID_ADMINISTRATOR
		 * and DOMAIN_RID_GUEST.
		 */
		if(rid == DOMAIN_RID_ADMINISTRATOR || rid == DOMAIN_RID_GUEST)
			return True;
	} else if((rid & RID_TYPE_MASK) == USER_RID_TYPE) {
		return True;
	}
	return False;
}

/*******************************************************************
 Convert a name into a SID. Used in the lookup name rpc.
 ********************************************************************/

bool lookup_global_sam_name(const char *name, int flags, uint32_t *rid,
			    enum lsa_SidType *type)
{
	GROUP_MAP map;
	bool ret;

	/* Windows treats "MACHINE\None" as a special name for 
	   rid 513 on non-DCs.  You cannot create a user or group
	   name "None" on Windows.  You will get an error that 
	   the group already exists. */

	if ( strequal( name, "None" ) ) {
		*rid = DOMAIN_RID_USERS;
		*type = SID_NAME_DOM_GRP;

		return True;
	}

	/* LOOKUP_NAME_GROUP is a hack to allow valid users = @foo to work
	 * correctly in the case where foo also exists as a user. If the flag
	 * is set, don't look for users at all. */

	if ((flags & LOOKUP_NAME_GROUP) == 0) {
		struct samu *sam_account = NULL;
		struct dom_sid user_sid;

		if ( !(sam_account = samu_new( NULL )) ) {
			return False;
		}

		become_root();
		ret =  pdb_getsampwnam(sam_account, name);
		unbecome_root();

		if (ret) {
			sid_copy(&user_sid, pdb_get_user_sid(sam_account));
		}

		TALLOC_FREE(sam_account);

		if (ret) {
			if (!sid_check_is_in_our_domain(&user_sid)) {
				DEBUG(0, ("User %s with invalid SID %s in passdb\n",
					  name, sid_string_dbg(&user_sid)));
				return False;
			}

			sid_peek_rid(&user_sid, rid);
			*type = SID_NAME_USER;
			return True;
		}
	}

	/*
	 * Maybe it is a group ?
	 */

	become_root();
	ret = pdb_getgrnam(&map, name);
	unbecome_root();

 	if (!ret) {
		return False;
	}

	/* BUILTIN groups are looked up elsewhere */
	if (!sid_check_is_in_our_domain(&map.sid)) {
		DEBUG(10, ("Found group %s (%s) not in our domain -- "
			   "ignoring.", name, sid_string_dbg(&map.sid)));
		return False;
	}

	/* yes it's a mapped group */
	sid_peek_rid(&map.sid, rid);
	*type = map.sid_name_use;
	return True;
}

/*************************************************************
 Change a password entry in the local passdb backend.

 Assumptions:
  - always called as root
  - ignores the account type except when adding a new account
  - will create/delete the unix account if the relative
    add/delete user script is configured

 *************************************************************/

NTSTATUS local_password_change(const char *user_name,
				int local_flags,
				const char *new_passwd, 
				char **pp_err_str,
				char **pp_msg_str)
{
	TALLOC_CTX *tosctx;
	struct samu *sam_pass;
	uint32_t acb;
	uint32_t rid;
	NTSTATUS result;
	bool user_exists;
	int ret = -1;

	*pp_err_str = NULL;
	*pp_msg_str = NULL;

	tosctx = talloc_tos();

	sam_pass = samu_new(tosctx);
	if (!sam_pass) {
		result = NT_STATUS_NO_MEMORY;
		goto done;
	}

	/* Get the smb passwd entry for this user */
	user_exists = pdb_getsampwnam(sam_pass, user_name);

	/* Check delete first, we don't need to do anything else if we
	 * are going to delete the acocunt */
	if (user_exists && (local_flags & LOCAL_DELETE_USER)) {

		result = pdb_delete_user(tosctx, sam_pass);
		if (!NT_STATUS_IS_OK(result)) {
			ret = asprintf(pp_err_str,
					"Failed to delete entry for user %s.\n",
					user_name);
			if (ret < 0) {
				*pp_err_str = NULL;
			}
			result = NT_STATUS_UNSUCCESSFUL;
		} else {
			ret = asprintf(pp_msg_str,
					"Deleted user %s.\n",
					user_name);
			if (ret < 0) {
				*pp_msg_str = NULL;
			}
		}
		goto done;
	}

	if (user_exists && (local_flags & LOCAL_ADD_USER)) {
		/* the entry already existed */
		local_flags &= ~LOCAL_ADD_USER;
	}

	if (!user_exists && !(local_flags & LOCAL_ADD_USER)) {
		ret = asprintf(pp_err_str,
				"Failed to find entry for user %s.\n",
				user_name);
		if (ret < 0) {
			*pp_err_str = NULL;
		}
		result = NT_STATUS_NO_SUCH_USER;
		goto done;
	}

	/* First thing add the new user if we are required to do so */
	if (local_flags & LOCAL_ADD_USER) {

		if (local_flags & LOCAL_TRUST_ACCOUNT) {
			acb = ACB_WSTRUST;
		} else if (local_flags & LOCAL_INTERDOM_ACCOUNT) {
			acb = ACB_DOMTRUST;
		} else {
			acb = ACB_NORMAL;
		}

		result = pdb_create_user(tosctx, user_name, acb, &rid);
		if (!NT_STATUS_IS_OK(result)) {
			ret = asprintf(pp_err_str,
					"Failed to add entry for user %s.\n",
					user_name);
			if (ret < 0) {
				*pp_err_str = NULL;
			}
			result = NT_STATUS_UNSUCCESSFUL;
			goto done;
		}

		sam_pass = samu_new(tosctx);
		if (!sam_pass) {
			result = NT_STATUS_NO_MEMORY;
			goto done;
		}

		/* Now get back the smb passwd entry for this new user */
		user_exists = pdb_getsampwnam(sam_pass, user_name);
		if (!user_exists) {
			ret = asprintf(pp_err_str,
					"Failed to add entry for user %s.\n",
					user_name);
			if (ret < 0) {
				*pp_err_str = NULL;
			}
			result = NT_STATUS_UNSUCCESSFUL;
			goto done;
		}
	}

	acb = pdb_get_acct_ctrl(sam_pass);

	/*
	 * We are root - just write the new password
	 * and the valid last change time.
	 */
	if ((local_flags & LOCAL_SET_NO_PASSWORD) && !(acb & ACB_PWNOTREQ)) {
		acb |= ACB_PWNOTREQ;
		if (!pdb_set_acct_ctrl(sam_pass, acb, PDB_CHANGED)) {
			ret = asprintf(pp_err_str,
					"Failed to set 'no password required' "
					"flag for user %s.\n", user_name);
			if (ret < 0) {
				*pp_err_str = NULL;
			}
			result = NT_STATUS_UNSUCCESSFUL;
			goto done;
		}
	}

	if (local_flags & LOCAL_SET_PASSWORD) {
		/*
		 * If we're dealing with setting a completely empty user account
		 * ie. One with a password of 'XXXX', but not set disabled (like
		 * an account created from scratch) then if the old password was
		 * 'XX's then getsmbpwent will have set the ACB_DISABLED flag.
		 * We remove that as we're giving this user their first password
		 * and the decision hasn't really been made to disable them (ie.
		 * don't create them disabled). JRA.
		 */
		if ((pdb_get_lanman_passwd(sam_pass) == NULL) &&
		    (acb & ACB_DISABLED)) {
			acb &= (~ACB_DISABLED);
			if (!pdb_set_acct_ctrl(sam_pass, acb, PDB_CHANGED)) {
				ret = asprintf(pp_err_str,
						"Failed to unset 'disabled' "
						"flag for user %s.\n",
						user_name);
				if (ret < 0) {
					*pp_err_str = NULL;
				}
				result = NT_STATUS_UNSUCCESSFUL;
				goto done;
			}
		}

		acb &= (~ACB_PWNOTREQ);
		if (!pdb_set_acct_ctrl(sam_pass, acb, PDB_CHANGED)) {
			ret = asprintf(pp_err_str,
					"Failed to unset 'no password required'"
					" flag for user %s.\n", user_name);
			if (ret < 0) {
				*pp_err_str = NULL;
			}
			result = NT_STATUS_UNSUCCESSFUL;
			goto done;
		}

		if (!pdb_set_plaintext_passwd(sam_pass, new_passwd)) {
			ret = asprintf(pp_err_str,
					"Failed to set password for "
					"user %s.\n", user_name);
				if (ret < 0) {
				*pp_err_str = NULL;
			}
			result = NT_STATUS_UNSUCCESSFUL;
			goto done;
		}
	}

	if ((local_flags & LOCAL_DISABLE_USER) && !(acb & ACB_DISABLED)) {
		acb |= ACB_DISABLED;
		if (!pdb_set_acct_ctrl(sam_pass, acb, PDB_CHANGED)) {
			ret = asprintf(pp_err_str,
					"Failed to set 'disabled' flag for "
					"user %s.\n", user_name);
			if (ret < 0) {
				*pp_err_str = NULL;
			}
			result = NT_STATUS_UNSUCCESSFUL;
			goto done;
		}
	}

	if ((local_flags & LOCAL_ENABLE_USER) && (acb & ACB_DISABLED)) {
		acb &= (~ACB_DISABLED);
		if (!pdb_set_acct_ctrl(sam_pass, acb, PDB_CHANGED)) {
			ret = asprintf(pp_err_str,
					"Failed to unset 'disabled' flag for "
					"user %s.\n", user_name);
			if (ret < 0) {
				*pp_err_str = NULL;
			}
			result = NT_STATUS_UNSUCCESSFUL;
			goto done;
		}
	}

	/* now commit changes if any */
	result = pdb_update_sam_account(sam_pass);
	if (!NT_STATUS_IS_OK(result)) {
		ret = asprintf(pp_err_str,
				"Failed to modify entry for user %s.\n",
				user_name);
		if (ret < 0) {
			*pp_err_str = NULL;
		}
		goto done;
	}

	if (local_flags & LOCAL_ADD_USER) {
		ret = asprintf(pp_msg_str, "Added user %s.\n", user_name);
	} else if (local_flags & LOCAL_DISABLE_USER) {
		ret = asprintf(pp_msg_str, "Disabled user %s.\n", user_name);
	} else if (local_flags & LOCAL_ENABLE_USER) {
		ret = asprintf(pp_msg_str, "Enabled user %s.\n", user_name);
	} else if (local_flags & LOCAL_SET_NO_PASSWORD) {
		ret = asprintf(pp_msg_str,
				"User %s password set to none.\n", user_name);
	}

	if (ret < 0) {
		*pp_msg_str = NULL;
	}

	result = NT_STATUS_OK;

done:
	TALLOC_FREE(sam_pass);
	return result;
}

/**********************************************************************
 Marshall/unmarshall struct samu structs.
 *********************************************************************/

#define SAMU_BUFFER_FORMAT_V0       "ddddddBBBBBBBBBBBBddBBwdwdBwwd"
#define SAMU_BUFFER_FORMAT_V1       "dddddddBBBBBBBBBBBBddBBwdwdBwwd"
#define SAMU_BUFFER_FORMAT_V2       "dddddddBBBBBBBBBBBBddBBBwwdBwwd"
#define SAMU_BUFFER_FORMAT_V3       "dddddddBBBBBBBBBBBBddBBBdwdBwwd"
/* nothing changed between V3 and V4 */

/*********************************************************************
*********************************************************************/

static bool init_samu_from_buffer_v0(struct samu *sampass, uint8_t *buf, uint32_t buflen)
{

	/* times are stored as 32bit integer
	   take care on system with 64bit wide time_t
	   --SSS */
	uint32_t	logon_time,
		logoff_time,
		kickoff_time,
		pass_last_set_time,
		pass_can_change_time,
		pass_must_change_time;
	char *username = NULL;
	char *domain = NULL;
	char *nt_username = NULL;
	char *dir_drive = NULL;
	char *unknown_str = NULL;
	char *munged_dial = NULL;
	char *fullname = NULL;
	char *homedir = NULL;
	char *logon_script = NULL;
	char *profile_path = NULL;
	char *acct_desc = NULL;
	char *workstations = NULL;
	uint32_t	username_len, domain_len, nt_username_len,
		dir_drive_len, unknown_str_len, munged_dial_len,
		fullname_len, homedir_len, logon_script_len,
		profile_path_len, acct_desc_len, workstations_len;

	uint32_t	user_rid, group_rid, remove_me, hours_len, unknown_6;
	uint16_t	acct_ctrl, logon_divs;
	uint16_t	bad_password_count, logon_count;
	uint8_t	*hours = NULL;
	uint8_t	*lm_pw_ptr = NULL, *nt_pw_ptr = NULL;
	uint32_t		len = 0;
	uint32_t		lm_pw_len, nt_pw_len, hourslen;
	bool ret = True;

	if(sampass == NULL || buf == NULL) {
		DEBUG(0, ("init_samu_from_buffer_v0: NULL parameters found!\n"));
		return False;
	}

/* SAMU_BUFFER_FORMAT_V0       "ddddddBBBBBBBBBBBBddBBwdwdBwwd" */

	/* unpack the buffer into variables */
	len = tdb_unpack (buf, buflen, SAMU_BUFFER_FORMAT_V0,
		&logon_time,						/* d */
		&logoff_time,						/* d */
		&kickoff_time,						/* d */
		&pass_last_set_time,					/* d */
		&pass_can_change_time,					/* d */
		&pass_must_change_time,					/* d */
		&username_len, &username,				/* B */
		&domain_len, &domain,					/* B */
		&nt_username_len, &nt_username,				/* B */
		&fullname_len, &fullname,				/* B */
		&homedir_len, &homedir,					/* B */
		&dir_drive_len, &dir_drive,				/* B */
		&logon_script_len, &logon_script,			/* B */
		&profile_path_len, &profile_path,			/* B */
		&acct_desc_len, &acct_desc,				/* B */
		&workstations_len, &workstations,			/* B */
		&unknown_str_len, &unknown_str,				/* B */
		&munged_dial_len, &munged_dial,				/* B */
		&user_rid,						/* d */
		&group_rid,						/* d */
		&lm_pw_len, &lm_pw_ptr,					/* B */
		&nt_pw_len, &nt_pw_ptr,					/* B */
		&acct_ctrl,						/* w */
		&remove_me, /* remove on the next TDB_FORMAT upgarde */	/* d */
		&logon_divs,						/* w */
		&hours_len,						/* d */
		&hourslen, &hours,					/* B */
		&bad_password_count,					/* w */
		&logon_count,						/* w */
		&unknown_6);						/* d */

	if (len == (uint32_t) -1)  {
		ret = False;
		goto done;
	}

	pdb_set_logon_time(sampass, logon_time, PDB_SET);
	pdb_set_logoff_time(sampass, logoff_time, PDB_SET);
	pdb_set_kickoff_time(sampass, kickoff_time, PDB_SET);
	pdb_set_pass_can_change_time(sampass, pass_can_change_time, PDB_SET);
	pdb_set_pass_must_change_time(sampass, pass_must_change_time, PDB_SET);
	pdb_set_pass_last_set_time(sampass, pass_last_set_time, PDB_SET);

	pdb_set_username(sampass, username, PDB_SET); 
	pdb_set_domain(sampass, domain, PDB_SET);
	pdb_set_nt_username(sampass, nt_username, PDB_SET);
	pdb_set_fullname(sampass, fullname, PDB_SET);

	if (homedir) {
		pdb_set_homedir(sampass, homedir, PDB_SET);
	}
	else {
		pdb_set_homedir(sampass, 
			talloc_sub_basic(sampass, username, domain,
					 lp_logon_home()),
			PDB_DEFAULT);
	}

	if (dir_drive) 	
		pdb_set_dir_drive(sampass, dir_drive, PDB_SET);
	else {
		pdb_set_dir_drive(sampass, 
			talloc_sub_basic(sampass, username, domain,
					 lp_logon_drive()),
			PDB_DEFAULT);
	}

	if (logon_script) 
		pdb_set_logon_script(sampass, logon_script, PDB_SET);
	else {
		pdb_set_logon_script(sampass, 
			talloc_sub_basic(sampass, username, domain,
					 lp_logon_script()),
			PDB_DEFAULT);
	}

	if (profile_path) {	
		pdb_set_profile_path(sampass, profile_path, PDB_SET);
	} else {
		pdb_set_profile_path(sampass, 
			talloc_sub_basic(sampass, username, domain,
					 lp_logon_path()),
			PDB_DEFAULT);
	}

	pdb_set_acct_desc(sampass, acct_desc, PDB_SET);
	pdb_set_workstations(sampass, workstations, PDB_SET);
	pdb_set_munged_dial(sampass, munged_dial, PDB_SET);

	if (lm_pw_ptr && lm_pw_len == LM_HASH_LEN) {
		if (!pdb_set_lanman_passwd(sampass, lm_pw_ptr, PDB_SET)) {
			ret = False;
			goto done;
		}
	}

	if (nt_pw_ptr && nt_pw_len == NT_HASH_LEN) {
		if (!pdb_set_nt_passwd(sampass, nt_pw_ptr, PDB_SET)) {
			ret = False;
			goto done;
		}
	}

	pdb_set_pw_history(sampass, NULL, 0, PDB_SET);
	pdb_set_user_sid_from_rid(sampass, user_rid, PDB_SET);
	pdb_set_group_sid_from_rid(sampass, group_rid, PDB_SET);
	pdb_set_hours_len(sampass, hours_len, PDB_SET);
	pdb_set_bad_password_count(sampass, bad_password_count, PDB_SET);
	pdb_set_logon_count(sampass, logon_count, PDB_SET);
	pdb_set_unknown_6(sampass, unknown_6, PDB_SET);
	pdb_set_acct_ctrl(sampass, acct_ctrl, PDB_SET);
	pdb_set_logon_divs(sampass, logon_divs, PDB_SET);
	pdb_set_hours(sampass, hours, hours_len, PDB_SET);

done:

	SAFE_FREE(username);
	SAFE_FREE(domain);
	SAFE_FREE(nt_username);
	SAFE_FREE(fullname);
	SAFE_FREE(homedir);
	SAFE_FREE(dir_drive);
	SAFE_FREE(logon_script);
	SAFE_FREE(profile_path);
	SAFE_FREE(acct_desc);
	SAFE_FREE(workstations);
	SAFE_FREE(munged_dial);
	SAFE_FREE(unknown_str);
	SAFE_FREE(lm_pw_ptr);
	SAFE_FREE(nt_pw_ptr);
	SAFE_FREE(hours);

	return ret;
}

/*********************************************************************
*********************************************************************/

static bool init_samu_from_buffer_v1(struct samu *sampass, uint8_t *buf, uint32_t buflen)
{

	/* times are stored as 32bit integer
	   take care on system with 64bit wide time_t
	   --SSS */
	uint32_t	logon_time,
		logoff_time,
		kickoff_time,
		bad_password_time,
		pass_last_set_time,
		pass_can_change_time,
		pass_must_change_time;
	char *username = NULL;
	char *domain = NULL;
	char *nt_username = NULL;
	char *dir_drive = NULL;
	char *unknown_str = NULL;
	char *munged_dial = NULL;
	char *fullname = NULL;
	char *homedir = NULL;
	char *logon_script = NULL;
	char *profile_path = NULL;
	char *acct_desc = NULL;
	char *workstations = NULL;
	uint32_t	username_len, domain_len, nt_username_len,
		dir_drive_len, unknown_str_len, munged_dial_len,
		fullname_len, homedir_len, logon_script_len,
		profile_path_len, acct_desc_len, workstations_len;

	uint32_t	user_rid, group_rid, remove_me, hours_len, unknown_6;
	uint16_t	acct_ctrl, logon_divs;
	uint16_t	bad_password_count, logon_count;
	uint8_t	*hours = NULL;
	uint8_t	*lm_pw_ptr = NULL, *nt_pw_ptr = NULL;
	uint32_t		len = 0;
	uint32_t		lm_pw_len, nt_pw_len, hourslen;
	bool ret = True;

	if(sampass == NULL || buf == NULL) {
		DEBUG(0, ("init_samu_from_buffer_v1: NULL parameters found!\n"));
		return False;
	}

/* SAMU_BUFFER_FORMAT_V1       "dddddddBBBBBBBBBBBBddBBwdwdBwwd" */

	/* unpack the buffer into variables */
	len = tdb_unpack (buf, buflen, SAMU_BUFFER_FORMAT_V1,
		&logon_time,						/* d */
		&logoff_time,						/* d */
		&kickoff_time,						/* d */
		/* Change from V0 is addition of bad_password_time field. */
		&bad_password_time,					/* d */
		&pass_last_set_time,					/* d */
		&pass_can_change_time,					/* d */
		&pass_must_change_time,					/* d */
		&username_len, &username,				/* B */
		&domain_len, &domain,					/* B */
		&nt_username_len, &nt_username,				/* B */
		&fullname_len, &fullname,				/* B */
		&homedir_len, &homedir,					/* B */
		&dir_drive_len, &dir_drive,				/* B */
		&logon_script_len, &logon_script,			/* B */
		&profile_path_len, &profile_path,			/* B */
		&acct_desc_len, &acct_desc,				/* B */
		&workstations_len, &workstations,			/* B */
		&unknown_str_len, &unknown_str,				/* B */
		&munged_dial_len, &munged_dial,				/* B */
		&user_rid,						/* d */
		&group_rid,						/* d */
		&lm_pw_len, &lm_pw_ptr,					/* B */
		&nt_pw_len, &nt_pw_ptr,					/* B */
		&acct_ctrl,						/* w */
		&remove_me,						/* d */
		&logon_divs,						/* w */
		&hours_len,						/* d */
		&hourslen, &hours,					/* B */
		&bad_password_count,					/* w */
		&logon_count,						/* w */
		&unknown_6);						/* d */

	if (len == (uint32_t) -1)  {
		ret = False;
		goto done;
	}

	pdb_set_logon_time(sampass, logon_time, PDB_SET);
	pdb_set_logoff_time(sampass, logoff_time, PDB_SET);
	pdb_set_kickoff_time(sampass, kickoff_time, PDB_SET);

	/* Change from V0 is addition of bad_password_time field. */
	pdb_set_bad_password_time(sampass, bad_password_time, PDB_SET);
	pdb_set_pass_can_change_time(sampass, pass_can_change_time, PDB_SET);
	pdb_set_pass_must_change_time(sampass, pass_must_change_time, PDB_SET);
	pdb_set_pass_last_set_time(sampass, pass_last_set_time, PDB_SET);

	pdb_set_username(sampass, username, PDB_SET); 
	pdb_set_domain(sampass, domain, PDB_SET);
	pdb_set_nt_username(sampass, nt_username, PDB_SET);
	pdb_set_fullname(sampass, fullname, PDB_SET);

	if (homedir) {
		pdb_set_homedir(sampass, homedir, PDB_SET);
	}
	else {
		pdb_set_homedir(sampass, 
			talloc_sub_basic(sampass, username, domain,
					 lp_logon_home()),
			PDB_DEFAULT);
	}

	if (dir_drive) 	
		pdb_set_dir_drive(sampass, dir_drive, PDB_SET);
	else {
		pdb_set_dir_drive(sampass, 
			talloc_sub_basic(sampass, username, domain,
					 lp_logon_drive()),
			PDB_DEFAULT);
	}

	if (logon_script) 
		pdb_set_logon_script(sampass, logon_script, PDB_SET);
	else {
		pdb_set_logon_script(sampass, 
			talloc_sub_basic(sampass, username, domain,
					 lp_logon_script()),
			PDB_DEFAULT);
	}

	if (profile_path) {	
		pdb_set_profile_path(sampass, profile_path, PDB_SET);
	} else {
		pdb_set_profile_path(sampass, 
			talloc_sub_basic(sampass, username, domain,
					 lp_logon_path()),
			PDB_DEFAULT);
	}

	pdb_set_acct_desc(sampass, acct_desc, PDB_SET);
	pdb_set_workstations(sampass, workstations, PDB_SET);
	pdb_set_munged_dial(sampass, munged_dial, PDB_SET);

	if (lm_pw_ptr && lm_pw_len == LM_HASH_LEN) {
		if (!pdb_set_lanman_passwd(sampass, lm_pw_ptr, PDB_SET)) {
			ret = False;
			goto done;
		}
	}

	if (nt_pw_ptr && nt_pw_len == NT_HASH_LEN) {
		if (!pdb_set_nt_passwd(sampass, nt_pw_ptr, PDB_SET)) {
			ret = False;
			goto done;
		}
	}

	pdb_set_pw_history(sampass, NULL, 0, PDB_SET);

	pdb_set_user_sid_from_rid(sampass, user_rid, PDB_SET);
	pdb_set_group_sid_from_rid(sampass, group_rid, PDB_SET);
	pdb_set_hours_len(sampass, hours_len, PDB_SET);
	pdb_set_bad_password_count(sampass, bad_password_count, PDB_SET);
	pdb_set_logon_count(sampass, logon_count, PDB_SET);
	pdb_set_unknown_6(sampass, unknown_6, PDB_SET);
	pdb_set_acct_ctrl(sampass, acct_ctrl, PDB_SET);
	pdb_set_logon_divs(sampass, logon_divs, PDB_SET);
	pdb_set_hours(sampass, hours, hours_len, PDB_SET);

done:

	SAFE_FREE(username);
	SAFE_FREE(domain);
	SAFE_FREE(nt_username);
	SAFE_FREE(fullname);
	SAFE_FREE(homedir);
	SAFE_FREE(dir_drive);
	SAFE_FREE(logon_script);
	SAFE_FREE(profile_path);
	SAFE_FREE(acct_desc);
	SAFE_FREE(workstations);
	SAFE_FREE(munged_dial);
	SAFE_FREE(unknown_str);
	SAFE_FREE(lm_pw_ptr);
	SAFE_FREE(nt_pw_ptr);
	SAFE_FREE(hours);

	return ret;
}

static bool init_samu_from_buffer_v2(struct samu *sampass, uint8_t *buf, uint32_t buflen)
{

	/* times are stored as 32bit integer
	   take care on system with 64bit wide time_t
	   --SSS */
	uint32_t	logon_time,
		logoff_time,
		kickoff_time,
		bad_password_time,
		pass_last_set_time,
		pass_can_change_time,
		pass_must_change_time;
	char *username = NULL;
	char *domain = NULL;
	char *nt_username = NULL;
	char *dir_drive = NULL;
	char *unknown_str = NULL;
	char *munged_dial = NULL;
	char *fullname = NULL;
	char *homedir = NULL;
	char *logon_script = NULL;
	char *profile_path = NULL;
	char *acct_desc = NULL;
	char *workstations = NULL;
	uint32_t	username_len, domain_len, nt_username_len,
		dir_drive_len, unknown_str_len, munged_dial_len,
		fullname_len, homedir_len, logon_script_len,
		profile_path_len, acct_desc_len, workstations_len;

	uint32_t	user_rid, group_rid, hours_len, unknown_6;
	uint16_t	acct_ctrl, logon_divs;
	uint16_t	bad_password_count, logon_count;
	uint8_t	*hours = NULL;
	uint8_t	*lm_pw_ptr = NULL, *nt_pw_ptr = NULL, *nt_pw_hist_ptr = NULL;
	uint32_t		len = 0;
	uint32_t		lm_pw_len, nt_pw_len, nt_pw_hist_len, hourslen;
	uint32_t pwHistLen = 0;
	bool ret = True;
	fstring tmp_string;
	bool expand_explicit = lp_passdb_expand_explicit();

	if(sampass == NULL || buf == NULL) {
		DEBUG(0, ("init_samu_from_buffer_v2: NULL parameters found!\n"));
		return False;
	}

/* SAMU_BUFFER_FORMAT_V2       "dddddddBBBBBBBBBBBBddBBBwwdBwwd" */

	/* unpack the buffer into variables */
	len = tdb_unpack (buf, buflen, SAMU_BUFFER_FORMAT_V2,
		&logon_time,						/* d */
		&logoff_time,						/* d */
		&kickoff_time,						/* d */
		&bad_password_time,					/* d */
		&pass_last_set_time,					/* d */
		&pass_can_change_time,					/* d */
		&pass_must_change_time,					/* d */
		&username_len, &username,				/* B */
		&domain_len, &domain,					/* B */
		&nt_username_len, &nt_username,				/* B */
		&fullname_len, &fullname,				/* B */
		&homedir_len, &homedir,					/* B */
		&dir_drive_len, &dir_drive,				/* B */
		&logon_script_len, &logon_script,			/* B */
		&profile_path_len, &profile_path,			/* B */
		&acct_desc_len, &acct_desc,				/* B */
		&workstations_len, &workstations,			/* B */
		&unknown_str_len, &unknown_str,				/* B */
		&munged_dial_len, &munged_dial,				/* B */
		&user_rid,						/* d */
		&group_rid,						/* d */
		&lm_pw_len, &lm_pw_ptr,					/* B */
		&nt_pw_len, &nt_pw_ptr,					/* B */
		/* Change from V1 is addition of password history field. */
		&nt_pw_hist_len, &nt_pw_hist_ptr,			/* B */
		&acct_ctrl,						/* w */
		/* Also "remove_me" field was removed. */
		&logon_divs,						/* w */
		&hours_len,						/* d */
		&hourslen, &hours,					/* B */
		&bad_password_count,					/* w */
		&logon_count,						/* w */
		&unknown_6);						/* d */

	if (len == (uint32_t) -1)  {
		ret = False;
		goto done;
	}

	pdb_set_logon_time(sampass, logon_time, PDB_SET);
	pdb_set_logoff_time(sampass, logoff_time, PDB_SET);
	pdb_set_kickoff_time(sampass, kickoff_time, PDB_SET);
	pdb_set_bad_password_time(sampass, bad_password_time, PDB_SET);
	pdb_set_pass_can_change_time(sampass, pass_can_change_time, PDB_SET);
	pdb_set_pass_must_change_time(sampass, pass_must_change_time, PDB_SET);
	pdb_set_pass_last_set_time(sampass, pass_last_set_time, PDB_SET);

	pdb_set_username(sampass, username, PDB_SET); 
	pdb_set_domain(sampass, domain, PDB_SET);
	pdb_set_nt_username(sampass, nt_username, PDB_SET);
	pdb_set_fullname(sampass, fullname, PDB_SET);

	if (homedir) {
		fstrcpy( tmp_string, homedir );
		if (expand_explicit) {
			standard_sub_basic( username, domain, tmp_string,
					    sizeof(tmp_string) );
		}
		pdb_set_homedir(sampass, tmp_string, PDB_SET);
	}
	else {
		pdb_set_homedir(sampass, 
			talloc_sub_basic(sampass, username, domain,
					 lp_logon_home()),
			PDB_DEFAULT);
	}

	if (dir_drive) 	
		pdb_set_dir_drive(sampass, dir_drive, PDB_SET);
	else
		pdb_set_dir_drive(sampass, lp_logon_drive(), PDB_DEFAULT );

	if (logon_script) {
		fstrcpy( tmp_string, logon_script );
		if (expand_explicit) {
			standard_sub_basic( username, domain, tmp_string,
					    sizeof(tmp_string) );
		}
		pdb_set_logon_script(sampass, tmp_string, PDB_SET);
	}
	else {
		pdb_set_logon_script(sampass, 
			talloc_sub_basic(sampass, username, domain,
					 lp_logon_script()),
			PDB_DEFAULT);
	}

	if (profile_path) {	
		fstrcpy( tmp_string, profile_path );
		if (expand_explicit) {
			standard_sub_basic( username, domain, tmp_string,
					    sizeof(tmp_string) );
		}
		pdb_set_profile_path(sampass, tmp_string, PDB_SET);
	} 
	else {
		pdb_set_profile_path(sampass, 
			talloc_sub_basic(sampass, username, domain,
					 lp_logon_path()),
			PDB_DEFAULT);
	}

	pdb_set_acct_desc(sampass, acct_desc, PDB_SET);
	pdb_set_workstations(sampass, workstations, PDB_SET);
	pdb_set_munged_dial(sampass, munged_dial, PDB_SET);

	if (lm_pw_ptr && lm_pw_len == LM_HASH_LEN) {
		if (!pdb_set_lanman_passwd(sampass, lm_pw_ptr, PDB_SET)) {
			ret = False;
			goto done;
		}
	}

	if (nt_pw_ptr && nt_pw_len == NT_HASH_LEN) {
		if (!pdb_set_nt_passwd(sampass, nt_pw_ptr, PDB_SET)) {
			ret = False;
			goto done;
		}
	}

	/* Change from V1 is addition of password history field. */
	pdb_get_account_policy(PDB_POLICY_PASSWORD_HISTORY, &pwHistLen);
	if (pwHistLen) {
		uint8_t *pw_hist = SMB_MALLOC_ARRAY(uint8_t, pwHistLen * PW_HISTORY_ENTRY_LEN);
		if (!pw_hist) {
			ret = False;
			goto done;
		}
		memset(pw_hist, '\0', pwHistLen * PW_HISTORY_ENTRY_LEN);
		if (nt_pw_hist_ptr && nt_pw_hist_len) {
			int i;
			SMB_ASSERT((nt_pw_hist_len % PW_HISTORY_ENTRY_LEN) == 0);
			nt_pw_hist_len /= PW_HISTORY_ENTRY_LEN;
			for (i = 0; (i < pwHistLen) && (i < nt_pw_hist_len); i++) {
				memcpy(&pw_hist[i*PW_HISTORY_ENTRY_LEN],
					&nt_pw_hist_ptr[i*PW_HISTORY_ENTRY_LEN],
					PW_HISTORY_ENTRY_LEN);
			}
		}
		if (!pdb_set_pw_history(sampass, pw_hist, pwHistLen, PDB_SET)) {
			SAFE_FREE(pw_hist);
			ret = False;
			goto done;
		}
		SAFE_FREE(pw_hist);
	} else {
		pdb_set_pw_history(sampass, NULL, 0, PDB_SET);
	}

	pdb_set_user_sid_from_rid(sampass, user_rid, PDB_SET);
	pdb_set_group_sid_from_rid(sampass, group_rid, PDB_SET);
	pdb_set_hours_len(sampass, hours_len, PDB_SET);
	pdb_set_bad_password_count(sampass, bad_password_count, PDB_SET);
	pdb_set_logon_count(sampass, logon_count, PDB_SET);
	pdb_set_unknown_6(sampass, unknown_6, PDB_SET);
	pdb_set_acct_ctrl(sampass, acct_ctrl, PDB_SET);
	pdb_set_logon_divs(sampass, logon_divs, PDB_SET);
	pdb_set_hours(sampass, hours, hours_len, PDB_SET);

done:

	SAFE_FREE(username);
	SAFE_FREE(domain);
	SAFE_FREE(nt_username);
	SAFE_FREE(fullname);
	SAFE_FREE(homedir);
	SAFE_FREE(dir_drive);
	SAFE_FREE(logon_script);
	SAFE_FREE(profile_path);
	SAFE_FREE(acct_desc);
	SAFE_FREE(workstations);
	SAFE_FREE(munged_dial);
	SAFE_FREE(unknown_str);
	SAFE_FREE(lm_pw_ptr);
	SAFE_FREE(nt_pw_ptr);
	SAFE_FREE(nt_pw_hist_ptr);
	SAFE_FREE(hours);

	return ret;
}

/*********************************************************************
*********************************************************************/

static bool init_samu_from_buffer_v3(struct samu *sampass, uint8_t *buf, uint32_t buflen)
{

	/* times are stored as 32bit integer
	   take care on system with 64bit wide time_t
	   --SSS */
	uint32_t	logon_time,
		logoff_time,
		kickoff_time,
		bad_password_time,
		pass_last_set_time,
		pass_can_change_time,
		pass_must_change_time;
	char *username = NULL;
	char *domain = NULL;
	char *nt_username = NULL;
	char *dir_drive = NULL;
	char *comment = NULL;
	char *munged_dial = NULL;
	char *fullname = NULL;
	char *homedir = NULL;
	char *logon_script = NULL;
	char *profile_path = NULL;
	char *acct_desc = NULL;
	char *workstations = NULL;
	uint32_t	username_len, domain_len, nt_username_len,
		dir_drive_len, comment_len, munged_dial_len,
		fullname_len, homedir_len, logon_script_len,
		profile_path_len, acct_desc_len, workstations_len;

	uint32_t	user_rid, group_rid, hours_len, unknown_6, acct_ctrl;
	uint16_t  logon_divs;
	uint16_t	bad_password_count, logon_count;
	uint8_t	*hours = NULL;
	uint8_t	*lm_pw_ptr = NULL, *nt_pw_ptr = NULL, *nt_pw_hist_ptr = NULL;
	uint32_t		len = 0;
	uint32_t		lm_pw_len, nt_pw_len, nt_pw_hist_len, hourslen;
	uint32_t pwHistLen = 0;
	bool ret = True;
	fstring tmp_string;
	bool expand_explicit = lp_passdb_expand_explicit();

	if(sampass == NULL || buf == NULL) {
		DEBUG(0, ("init_samu_from_buffer_v3: NULL parameters found!\n"));
		return False;
	}

/* SAMU_BUFFER_FORMAT_V3       "dddddddBBBBBBBBBBBBddBBBdwdBwwd" */

	/* unpack the buffer into variables */
	len = tdb_unpack (buf, buflen, SAMU_BUFFER_FORMAT_V3,
		&logon_time,						/* d */
		&logoff_time,						/* d */
		&kickoff_time,						/* d */
		&bad_password_time,					/* d */
		&pass_last_set_time,					/* d */
		&pass_can_change_time,					/* d */
		&pass_must_change_time,					/* d */
		&username_len, &username,				/* B */
		&domain_len, &domain,					/* B */
		&nt_username_len, &nt_username,				/* B */
		&fullname_len, &fullname,				/* B */
		&homedir_len, &homedir,					/* B */
		&dir_drive_len, &dir_drive,				/* B */
		&logon_script_len, &logon_script,			/* B */
		&profile_path_len, &profile_path,			/* B */
		&acct_desc_len, &acct_desc,				/* B */
		&workstations_len, &workstations,			/* B */
		&comment_len, &comment,					/* B */
		&munged_dial_len, &munged_dial,				/* B */
		&user_rid,						/* d */
		&group_rid,						/* d */
		&lm_pw_len, &lm_pw_ptr,					/* B */
		&nt_pw_len, &nt_pw_ptr,					/* B */
		/* Change from V1 is addition of password history field. */
		&nt_pw_hist_len, &nt_pw_hist_ptr,			/* B */
		/* Change from V2 is the uint32_t acb_mask */
		&acct_ctrl,						/* d */
		/* Also "remove_me" field was removed. */
		&logon_divs,						/* w */
		&hours_len,						/* d */
		&hourslen, &hours,					/* B */
		&bad_password_count,					/* w */
		&logon_count,						/* w */
		&unknown_6);						/* d */

	if (len == (uint32_t) -1)  {
		ret = False;
		goto done;
	}

	pdb_set_logon_time(sampass, convert_uint32_t_to_time_t(logon_time), PDB_SET);
	pdb_set_logoff_time(sampass, convert_uint32_t_to_time_t(logoff_time), PDB_SET);
	pdb_set_kickoff_time(sampass, convert_uint32_t_to_time_t(kickoff_time), PDB_SET);
	pdb_set_bad_password_time(sampass, convert_uint32_t_to_time_t(bad_password_time), PDB_SET);
	pdb_set_pass_can_change_time(sampass, convert_uint32_t_to_time_t(pass_can_change_time), PDB_SET);
	pdb_set_pass_must_change_time(sampass, convert_uint32_t_to_time_t(pass_must_change_time), PDB_SET);
	pdb_set_pass_last_set_time(sampass, convert_uint32_t_to_time_t(pass_last_set_time), PDB_SET);

	pdb_set_username(sampass, username, PDB_SET); 
	pdb_set_domain(sampass, domain, PDB_SET);
	pdb_set_nt_username(sampass, nt_username, PDB_SET);
	pdb_set_fullname(sampass, fullname, PDB_SET);

	if (homedir) {
		fstrcpy( tmp_string, homedir );
		if (expand_explicit) {
			standard_sub_basic( username, domain, tmp_string,
					    sizeof(tmp_string) );
		}
		pdb_set_homedir(sampass, tmp_string, PDB_SET);
	}
	else {
		pdb_set_homedir(sampass, 
			talloc_sub_basic(sampass, username, domain,
					 lp_logon_home()),
			PDB_DEFAULT);
	}

	if (dir_drive) 	
		pdb_set_dir_drive(sampass, dir_drive, PDB_SET);
	else
		pdb_set_dir_drive(sampass, lp_logon_drive(), PDB_DEFAULT );

	if (logon_script) {
		fstrcpy( tmp_string, logon_script );
		if (expand_explicit) {
			standard_sub_basic( username, domain, tmp_string,
					    sizeof(tmp_string) );
		}
		pdb_set_logon_script(sampass, tmp_string, PDB_SET);
	}
	else {
		pdb_set_logon_script(sampass, 
			talloc_sub_basic(sampass, username, domain,
					 lp_logon_script()),
			PDB_DEFAULT);
	}

	if (profile_path) {	
		fstrcpy( tmp_string, profile_path );
		if (expand_explicit) {
			standard_sub_basic( username, domain, tmp_string,
					    sizeof(tmp_string) );
		}
		pdb_set_profile_path(sampass, tmp_string, PDB_SET);
	} 
	else {
		pdb_set_profile_path(sampass, 
			talloc_sub_basic(sampass, username, domain, lp_logon_path()),
			PDB_DEFAULT);
	}

	pdb_set_acct_desc(sampass, acct_desc, PDB_SET);
	pdb_set_comment(sampass, comment, PDB_SET);
	pdb_set_workstations(sampass, workstations, PDB_SET);
	pdb_set_munged_dial(sampass, munged_dial, PDB_SET);

	if (lm_pw_ptr && lm_pw_len == LM_HASH_LEN) {
		if (!pdb_set_lanman_passwd(sampass, lm_pw_ptr, PDB_SET)) {
			ret = False;
			goto done;
		}
	}

	if (nt_pw_ptr && nt_pw_len == NT_HASH_LEN) {
		if (!pdb_set_nt_passwd(sampass, nt_pw_ptr, PDB_SET)) {
			ret = False;
			goto done;
		}
	}

	pdb_get_account_policy(PDB_POLICY_PASSWORD_HISTORY, &pwHistLen);
	if (pwHistLen) {
		uint8_t *pw_hist = (uint8_t *)SMB_MALLOC(pwHistLen * PW_HISTORY_ENTRY_LEN);
		if (!pw_hist) {
			ret = False;
			goto done;
		}
		memset(pw_hist, '\0', pwHistLen * PW_HISTORY_ENTRY_LEN);
		if (nt_pw_hist_ptr && nt_pw_hist_len) {
			int i;
			SMB_ASSERT((nt_pw_hist_len % PW_HISTORY_ENTRY_LEN) == 0);
			nt_pw_hist_len /= PW_HISTORY_ENTRY_LEN;
			for (i = 0; (i < pwHistLen) && (i < nt_pw_hist_len); i++) {
				memcpy(&pw_hist[i*PW_HISTORY_ENTRY_LEN],
					&nt_pw_hist_ptr[i*PW_HISTORY_ENTRY_LEN],
					PW_HISTORY_ENTRY_LEN);
			}
		}
		if (!pdb_set_pw_history(sampass, pw_hist, pwHistLen, PDB_SET)) {
			SAFE_FREE(pw_hist);
			ret = False;
			goto done;
		}
		SAFE_FREE(pw_hist);
	} else {
		pdb_set_pw_history(sampass, NULL, 0, PDB_SET);
	}

	pdb_set_user_sid_from_rid(sampass, user_rid, PDB_SET);
	pdb_set_hours_len(sampass, hours_len, PDB_SET);
	pdb_set_bad_password_count(sampass, bad_password_count, PDB_SET);
	pdb_set_logon_count(sampass, logon_count, PDB_SET);
	pdb_set_unknown_6(sampass, unknown_6, PDB_SET);
	/* Change from V2 is the uint32_t acct_ctrl */
	pdb_set_acct_ctrl(sampass, acct_ctrl, PDB_SET);
	pdb_set_logon_divs(sampass, logon_divs, PDB_SET);
	pdb_set_hours(sampass, hours, hours_len, PDB_SET);

done:

	SAFE_FREE(username);
	SAFE_FREE(domain);
	SAFE_FREE(nt_username);
	SAFE_FREE(fullname);
	SAFE_FREE(homedir);
	SAFE_FREE(dir_drive);
	SAFE_FREE(logon_script);
	SAFE_FREE(profile_path);
	SAFE_FREE(acct_desc);
	SAFE_FREE(workstations);
	SAFE_FREE(munged_dial);
	SAFE_FREE(comment);
	SAFE_FREE(lm_pw_ptr);
	SAFE_FREE(nt_pw_ptr);
	SAFE_FREE(nt_pw_hist_ptr);
	SAFE_FREE(hours);

	return ret;
}

/*********************************************************************
*********************************************************************/

static uint32_t init_buffer_from_samu_v3 (uint8_t **buf, struct samu *sampass, bool size_only)
{
	size_t len, buflen;

	/* times are stored as 32bit integer
	   take care on system with 64bit wide time_t
	   --SSS */
	uint32_t	logon_time,
		logoff_time,
		kickoff_time,
		bad_password_time,
		pass_last_set_time,
		pass_can_change_time,
		pass_must_change_time;

	uint32_t  user_rid, group_rid;

	const char *username;
	const char *domain;
	const char *nt_username;
	const char *dir_drive;
	const char *comment;
	const char *munged_dial;
	const char *fullname;
	const char *homedir;
	const char *logon_script;
	const char *profile_path;
	const char *acct_desc;
	const char *workstations;
	uint32_t	username_len, domain_len, nt_username_len,
		dir_drive_len, comment_len, munged_dial_len,
		fullname_len, homedir_len, logon_script_len,
		profile_path_len, acct_desc_len, workstations_len;

	const uint8_t *lm_pw;
	const uint8_t *nt_pw;
	const uint8_t *nt_pw_hist;
	uint32_t	lm_pw_len = 16;
	uint32_t	nt_pw_len = 16;
	uint32_t  nt_pw_hist_len;
	uint32_t pwHistLen = 0;

	*buf = NULL;
	buflen = 0;

	logon_time = convert_time_t_to_uint32_t(pdb_get_logon_time(sampass));
	logoff_time = convert_time_t_to_uint32_t(pdb_get_logoff_time(sampass));
	kickoff_time = convert_time_t_to_uint32_t(pdb_get_kickoff_time(sampass));
	bad_password_time = convert_time_t_to_uint32_t(pdb_get_bad_password_time(sampass));
	pass_can_change_time = convert_time_t_to_uint32_t(pdb_get_pass_can_change_time_noncalc(sampass));
	pass_must_change_time = convert_time_t_to_uint32_t(pdb_get_pass_must_change_time(sampass));
	pass_last_set_time = convert_time_t_to_uint32_t(pdb_get_pass_last_set_time(sampass));

	user_rid = pdb_get_user_rid(sampass);
	group_rid = pdb_get_group_rid(sampass);

	username = pdb_get_username(sampass);
	if (username) {
		username_len = strlen(username) +1;
	} else {
		username_len = 0;
	}

	domain = pdb_get_domain(sampass);
	if (domain) {
		domain_len = strlen(domain) +1;
	} else {
		domain_len = 0;
	}

	nt_username = pdb_get_nt_username(sampass);
	if (nt_username) {
		nt_username_len = strlen(nt_username) +1;
	} else {
		nt_username_len = 0;
	}

	fullname = pdb_get_fullname(sampass);
	if (fullname) {
		fullname_len = strlen(fullname) +1;
	} else {
		fullname_len = 0;
	}

	/*
	 * Only updates fields which have been set (not defaults from smb.conf)
	 */

	if (!IS_SAM_DEFAULT(sampass, PDB_DRIVE)) {
		dir_drive = pdb_get_dir_drive(sampass);
	} else {
		dir_drive = NULL;
	}
	if (dir_drive) {
		dir_drive_len = strlen(dir_drive) +1;
	} else {
		dir_drive_len = 0;
	}

	if (!IS_SAM_DEFAULT(sampass, PDB_SMBHOME)) {
		homedir = pdb_get_homedir(sampass);
	} else {
		homedir = NULL;
	}
	if (homedir) {
		homedir_len = strlen(homedir) +1;
	} else {
		homedir_len = 0;
	}

	if (!IS_SAM_DEFAULT(sampass, PDB_LOGONSCRIPT)) {
		logon_script = pdb_get_logon_script(sampass);
	} else {
		logon_script = NULL;
	}
	if (logon_script) {
		logon_script_len = strlen(logon_script) +1;
	} else {
		logon_script_len = 0;
	}

	if (!IS_SAM_DEFAULT(sampass, PDB_PROFILE)) {
		profile_path = pdb_get_profile_path(sampass);
	} else {
		profile_path = NULL;
	}
	if (profile_path) {
		profile_path_len = strlen(profile_path) +1;
	} else {
		profile_path_len = 0;
	}

	lm_pw = pdb_get_lanman_passwd(sampass);
	if (!lm_pw) {
		lm_pw_len = 0;
	}

	nt_pw = pdb_get_nt_passwd(sampass);
	if (!nt_pw) {
		nt_pw_len = 0;
	}

	pdb_get_account_policy(PDB_POLICY_PASSWORD_HISTORY, &pwHistLen);
	nt_pw_hist =  pdb_get_pw_history(sampass, &nt_pw_hist_len);
	if (pwHistLen && nt_pw_hist && nt_pw_hist_len) {
		nt_pw_hist_len *= PW_HISTORY_ENTRY_LEN;
	} else {
		nt_pw_hist_len = 0;
	}

	acct_desc = pdb_get_acct_desc(sampass);
	if (acct_desc) {
		acct_desc_len = strlen(acct_desc) +1;
	} else {
		acct_desc_len = 0;
	}

	workstations = pdb_get_workstations(sampass);
	if (workstations) {
		workstations_len = strlen(workstations) +1;
	} else {
		workstations_len = 0;
	}

	comment = pdb_get_comment(sampass);
	if (comment) {
		comment_len = strlen(comment) +1;
	} else {
		comment_len = 0;
	}

	munged_dial = pdb_get_munged_dial(sampass);
	if (munged_dial) {
		munged_dial_len = strlen(munged_dial) +1;
	} else {
		munged_dial_len = 0;	
	}

/* SAMU_BUFFER_FORMAT_V3       "dddddddBBBBBBBBBBBBddBBBdwdBwwd" */

	/* one time to get the size needed */
	len = tdb_pack(NULL, 0,  SAMU_BUFFER_FORMAT_V3,
		logon_time,				/* d */
		logoff_time,				/* d */
		kickoff_time,				/* d */
		bad_password_time,			/* d */
		pass_last_set_time,			/* d */
		pass_can_change_time,			/* d */
		pass_must_change_time,			/* d */
		username_len, username,			/* B */
		domain_len, domain,			/* B */
		nt_username_len, nt_username,		/* B */
		fullname_len, fullname,			/* B */
		homedir_len, homedir,			/* B */
		dir_drive_len, dir_drive,		/* B */
		logon_script_len, logon_script,		/* B */
		profile_path_len, profile_path,		/* B */
		acct_desc_len, acct_desc,		/* B */
		workstations_len, workstations,		/* B */
		comment_len, comment,			/* B */
		munged_dial_len, munged_dial,		/* B */
		user_rid,				/* d */
		group_rid,				/* d */
		lm_pw_len, lm_pw,			/* B */
		nt_pw_len, nt_pw,			/* B */
		nt_pw_hist_len, nt_pw_hist,		/* B */
		pdb_get_acct_ctrl(sampass),		/* d */
		pdb_get_logon_divs(sampass),		/* w */
		pdb_get_hours_len(sampass),		/* d */
		MAX_HOURS_LEN, pdb_get_hours(sampass),	/* B */
		pdb_get_bad_password_count(sampass),	/* w */
		pdb_get_logon_count(sampass),		/* w */
		pdb_get_unknown_6(sampass));		/* d */

	if (size_only) {
		return buflen;
	}

	/* malloc the space needed */
	if ( (*buf=(uint8_t*)SMB_MALLOC(len)) == NULL) {
		DEBUG(0,("init_buffer_from_samu_v3: Unable to malloc() memory for buffer!\n"));
		return (-1);
	}

	/* now for the real call to tdb_pack() */
	buflen = tdb_pack(*buf, len,  SAMU_BUFFER_FORMAT_V3,
		logon_time,				/* d */
		logoff_time,				/* d */
		kickoff_time,				/* d */
		bad_password_time,			/* d */
		pass_last_set_time,			/* d */
		pass_can_change_time,			/* d */
		pass_must_change_time,			/* d */
		username_len, username,			/* B */
		domain_len, domain,			/* B */
		nt_username_len, nt_username,		/* B */
		fullname_len, fullname,			/* B */
		homedir_len, homedir,			/* B */
		dir_drive_len, dir_drive,		/* B */
		logon_script_len, logon_script,		/* B */
		profile_path_len, profile_path,		/* B */
		acct_desc_len, acct_desc,		/* B */
		workstations_len, workstations,		/* B */
		comment_len, comment,			/* B */
		munged_dial_len, munged_dial,		/* B */
		user_rid,				/* d */
		group_rid,				/* d */
		lm_pw_len, lm_pw,			/* B */
		nt_pw_len, nt_pw,			/* B */
		nt_pw_hist_len, nt_pw_hist,		/* B */
		pdb_get_acct_ctrl(sampass),		/* d */
		pdb_get_logon_divs(sampass),		/* w */
		pdb_get_hours_len(sampass),		/* d */
		MAX_HOURS_LEN, pdb_get_hours(sampass),	/* B */
		pdb_get_bad_password_count(sampass),	/* w */
		pdb_get_logon_count(sampass),		/* w */
		pdb_get_unknown_6(sampass));		/* d */

	/* check to make sure we got it correct */
	if (buflen != len) {
		DEBUG(0, ("init_buffer_from_samu_v3: somthing odd is going on here: bufflen (%lu) != len (%lu) in tdb_pack operations!\n", 
			  (unsigned long)buflen, (unsigned long)len));  
		/* error */
		SAFE_FREE (*buf);
		return (-1);
	}

	return (buflen);
}

static bool init_samu_from_buffer_v4(struct samu *sampass, uint8_t *buf, uint32_t buflen)
{
	/* nothing changed between V3 and V4 */
	return init_samu_from_buffer_v3(sampass, buf, buflen);
}

static uint32_t init_buffer_from_samu_v4(uint8_t **buf, struct samu *sampass, bool size_only)
{
	/* nothing changed between V3 and V4 */
	return init_buffer_from_samu_v3(buf, sampass, size_only);
}

/**********************************************************************
 Intialize a struct samu struct from a BYTE buffer of size len
 *********************************************************************/

bool init_samu_from_buffer(struct samu *sampass, uint32_t level,
			   uint8_t *buf, uint32_t buflen)
{
	switch (level) {
	case SAMU_BUFFER_V0:
		return init_samu_from_buffer_v0(sampass, buf, buflen);
	case SAMU_BUFFER_V1:
		return init_samu_from_buffer_v1(sampass, buf, buflen);
	case SAMU_BUFFER_V2:
		return init_samu_from_buffer_v2(sampass, buf, buflen);
	case SAMU_BUFFER_V3:
		return init_samu_from_buffer_v3(sampass, buf, buflen);
	case SAMU_BUFFER_V4:
		return init_samu_from_buffer_v4(sampass, buf, buflen);
	}

	return false;
}

/**********************************************************************
 Intialize a BYTE buffer from a struct samu struct
 *********************************************************************/

uint32_t init_buffer_from_samu (uint8_t **buf, struct samu *sampass, bool size_only)
{
	return init_buffer_from_samu_v4(buf, sampass, size_only);
}

/*********************************************************************
*********************************************************************/

bool pdb_copy_sam_account(struct samu *dst, struct samu *src )
{
	uint8_t *buf = NULL;
	int len;

	len = init_buffer_from_samu(&buf, src, False);
	if (len == -1 || !buf) {
		SAFE_FREE(buf);
		return False;
	}

	if (!init_samu_from_buffer( dst, SAMU_BUFFER_LATEST, buf, len )) {
		free(buf);
		return False;
	}

	dst->methods = src->methods;

	if ( src->unix_pw ) {
		dst->unix_pw = tcopy_passwd( dst, src->unix_pw );
		if (!dst->unix_pw) {
			free(buf);
			return False;
		}
	}

	if (src->group_sid) {
		pdb_set_group_sid(dst, src->group_sid, PDB_SET);
	}

	free(buf);
	return True;
}

/*********************************************************************
 Update the bad password count checking the PDB_POLICY_RESET_COUNT_TIME
*********************************************************************/

bool pdb_update_bad_password_count(struct samu *sampass, bool *updated)
{
	time_t LastBadPassword;
	uint16_t BadPasswordCount;
	uint32_t resettime;
	bool res;

	BadPasswordCount = pdb_get_bad_password_count(sampass);
	if (!BadPasswordCount) {
		DEBUG(9, ("No bad password attempts.\n"));
		return True;
	}

	become_root();
	res = pdb_get_account_policy(PDB_POLICY_RESET_COUNT_TIME, &resettime);
	unbecome_root();

	if (!res) {
		DEBUG(0, ("pdb_update_bad_password_count: pdb_get_account_policy failed.\n"));
		return False;
	}

	/* First, check if there is a reset time to compare */
	if ((resettime == (uint32_t) -1) || (resettime == 0)) {
		DEBUG(9, ("No reset time, can't reset bad pw count\n"));
		return True;
	}

	LastBadPassword = pdb_get_bad_password_time(sampass);
	DEBUG(7, ("LastBadPassword=%d, resettime=%d, current time=%d.\n", 
		   (uint32_t) LastBadPassword, resettime, (uint32_t)time(NULL)));
	if (time(NULL) > (LastBadPassword + convert_uint32_t_to_time_t(resettime)*60)){
		pdb_set_bad_password_count(sampass, 0, PDB_CHANGED);
		pdb_set_bad_password_time(sampass, 0, PDB_CHANGED);
		if (updated) {
			*updated = True;
		}
	}

	return True;
}

/*********************************************************************
 Update the ACB_AUTOLOCK flag checking the PDB_POLICY_LOCK_ACCOUNT_DURATION
*********************************************************************/

bool pdb_update_autolock_flag(struct samu *sampass, bool *updated)
{
	uint32_t duration;
	time_t LastBadPassword;
	bool res;

	if (!(pdb_get_acct_ctrl(sampass) & ACB_AUTOLOCK)) {
		DEBUG(9, ("pdb_update_autolock_flag: Account %s not autolocked, no check needed\n",
			pdb_get_username(sampass)));
		return True;
	}

	become_root();
	res = pdb_get_account_policy(PDB_POLICY_LOCK_ACCOUNT_DURATION, &duration);
	unbecome_root();

	if (!res) {
		DEBUG(0, ("pdb_update_autolock_flag: pdb_get_account_policy failed.\n"));
		return False;
	}

	/* First, check if there is a duration to compare */
	if ((duration == (uint32_t) -1)  || (duration == 0)) {
		DEBUG(9, ("pdb_update_autolock_flag: No reset duration, can't reset autolock\n"));
		return True;
	}

	LastBadPassword = pdb_get_bad_password_time(sampass);
	DEBUG(7, ("pdb_update_autolock_flag: Account %s, LastBadPassword=%d, duration=%d, current time =%d.\n",
		  pdb_get_username(sampass), (uint32_t)LastBadPassword, duration*60, (uint32_t)time(NULL)));

	if (LastBadPassword == (time_t)0) {
		DEBUG(1,("pdb_update_autolock_flag: Account %s "
			 "administratively locked out with no bad password "
			 "time. Leaving locked out.\n",
			 pdb_get_username(sampass) ));
		return True;
	}

	if ((time(NULL) > (LastBadPassword + convert_uint32_t_to_time_t(duration) * 60))) {
		pdb_set_acct_ctrl(sampass,
				  pdb_get_acct_ctrl(sampass) & ~ACB_AUTOLOCK,
				  PDB_CHANGED);
		pdb_set_bad_password_count(sampass, 0, PDB_CHANGED);
		pdb_set_bad_password_time(sampass, 0, PDB_CHANGED);
		if (updated) {
			*updated = True;
		}
	}

	return True;
}

/*********************************************************************
 Increment the bad_password_count 
*********************************************************************/

bool pdb_increment_bad_password_count(struct samu *sampass)
{
	uint32_t account_policy_lockout;
	bool autolock_updated = False, badpw_updated = False;
	bool ret;

	/* Retrieve the account lockout policy */
	become_root();
	ret = pdb_get_account_policy(PDB_POLICY_BAD_ATTEMPT_LOCKOUT, &account_policy_lockout);
	unbecome_root();
	if ( !ret ) {
		DEBUG(0, ("pdb_increment_bad_password_count: pdb_get_account_policy failed.\n"));
		return False;
	}

	/* If there is no policy, we don't need to continue checking */
	if (!account_policy_lockout) {
		DEBUG(9, ("No lockout policy, don't track bad passwords\n"));
		return True;
	}

	/* Check if the autolock needs to be cleared */
	if (!pdb_update_autolock_flag(sampass, &autolock_updated))
		return False;

	/* Check if the badpw count needs to be reset */
	if (!pdb_update_bad_password_count(sampass, &badpw_updated))
		return False;

	/*
	  Ok, now we can assume that any resetting that needs to be 
	  done has been done, and just get on with incrementing
	  and autolocking if necessary
	*/

	pdb_set_bad_password_count(sampass, 
				   pdb_get_bad_password_count(sampass)+1,
				   PDB_CHANGED);
	pdb_set_bad_password_time(sampass, time(NULL), PDB_CHANGED);


	if (pdb_get_bad_password_count(sampass) < account_policy_lockout) 
		return True;

	if (!pdb_set_acct_ctrl(sampass,
			       pdb_get_acct_ctrl(sampass) | ACB_AUTOLOCK,
			       PDB_CHANGED)) {
		DEBUG(1, ("pdb_increment_bad_password_count:failed to set 'autolock' flag. \n")); 
		return False;
	}

	return True;
}

bool is_dc_trusted_domain_situation(const char *domain_name)
{
	return IS_DC && !strequal(domain_name, lp_workgroup());
}

/*******************************************************************
 Wrapper around retrieving the clear text trust account password.
 appropriate account name is stored in account_name.
 Caller must free password, but not account_name.
*******************************************************************/

bool get_trust_pw_clear(const char *domain, char **ret_pwd,
			const char **account_name,
			enum netr_SchannelType *channel)
{
	char *pwd;
	time_t last_set_time;

	/* if we are a DC and this is not our domain, then lookup an account
	 * for the domain trust */

	if (is_dc_trusted_domain_situation(domain)) {
		if (!lp_allow_trusted_domains()) {
			return false;
		}

		if (!pdb_get_trusteddom_pw(domain, ret_pwd, NULL,
					   &last_set_time))
		{
			DEBUG(0, ("get_trust_pw: could not fetch trust "
				"account password for trusted domain %s\n",
				domain));
			return false;
		}

		if (channel != NULL) {
			*channel = SEC_CHAN_DOMAIN;
		}

		if (account_name != NULL) {
			*account_name = lp_workgroup();
		}

		return true;
	}

	/*
	 * Since we can only be member of one single domain, we are now
	 * in a member situation:
	 *
	 *  -  Either we are a DC (selfjoined) and the domain is our
	 *     own domain.
	 *  -  Or we are on a member and the domain is our own or some
	 *     other (potentially trusted) domain.
	 *
	 * In both cases, we can only get the machine account password
	 * for our own domain to connect to our own dc. (For a member,
	 * request to trusted domains are performed through our dc.)
	 *
	 * So we simply use our own domain name to retrieve the
	 * machine account passowrd and ignore the request domain here.
	 */

	pwd = secrets_fetch_machine_password(lp_workgroup(), &last_set_time, channel);

	if (pwd != NULL) {
		*ret_pwd = pwd;
		if (account_name != NULL) {
			*account_name = global_myname();
		}

		return true;
	}

	DEBUG(5, ("get_trust_pw_clear: could not fetch clear text trust "
		  "account password for domain %s\n", domain));
	return false;
}

/*******************************************************************
 Wrapper around retrieving the trust account password.
 appropriate account name is stored in account_name.
*******************************************************************/

bool get_trust_pw_hash(const char *domain, uint8_t ret_pwd[16],
		       const char **account_name,
		       enum netr_SchannelType *channel)
{
	char *pwd = NULL;
	time_t last_set_time;

	if (get_trust_pw_clear(domain, &pwd, account_name, channel)) {
		E_md4hash(pwd, ret_pwd);
		SAFE_FREE(pwd);
		return true;
	} else if (is_dc_trusted_domain_situation(domain)) {
		return false;
	}

	/* as a fallback, try to get the hashed pwd directly from the tdb... */

	if (secrets_fetch_trust_account_password_legacy(domain, ret_pwd,
							&last_set_time,
							channel))
	{
		if (account_name != NULL) {
			*account_name = global_myname();
		}

		return true;
	}

	DEBUG(5, ("get_trust_pw_hash: could not fetch trust account "
		"password for domain %s\n", domain));
	return False;
}
