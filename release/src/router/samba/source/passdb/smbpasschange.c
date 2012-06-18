/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   change a password in a local smbpasswd file
   Copyright (C) Andrew Tridgell 1998
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "includes.h"


/*************************************************************
add a new user to the local smbpasswd file
*************************************************************/

static BOOL add_new_user(char *user_name, uid_t uid, int local_flags,
			 uchar *new_p16, uchar *new_nt_p16)
{
	struct smb_passwd new_smb_pwent;

	/* Create a new smb passwd entry and set it to the given password. */
	new_smb_pwent.smb_userid = uid;
	new_smb_pwent.smb_name = user_name; 
	new_smb_pwent.smb_passwd = NULL;
	new_smb_pwent.smb_nt_passwd = NULL;
	new_smb_pwent.pass_last_set_time = time(NULL);
	new_smb_pwent.acct_ctrl = ((local_flags & LOCAL_TRUST_ACCOUNT) ? ACB_WSTRUST : ACB_NORMAL);
	
	if(local_flags & LOCAL_DISABLE_USER) {
		new_smb_pwent.acct_ctrl |= ACB_DISABLED;
	} else if (local_flags & LOCAL_SET_NO_PASSWORD) {
		new_smb_pwent.acct_ctrl |= ACB_PWNOTREQ;
	} else {
		new_smb_pwent.smb_passwd = new_p16;
		new_smb_pwent.smb_nt_passwd = new_nt_p16;
	}

	
	return add_smbpwd_entry(&new_smb_pwent);
}


/*************************************************************
change a password entry in the local smbpasswd file
*************************************************************/

BOOL local_password_change(char *user_name, int local_flags,
			   char *new_passwd, 
			   char *err_str, size_t err_str_len,
			   char *msg_str, size_t msg_str_len)
{
	struct passwd  *pwd = NULL;
	void *vp;
	struct smb_passwd *smb_pwent;
	uchar           new_p16[16];
	uchar           new_nt_p16[16];

	*err_str = '\0';
	*msg_str = '\0';

	if (local_flags & LOCAL_ADD_USER) {
	
		/*
		 * Check for a local account - if we're adding only.
		 */
	
		if(!(pwd = sys_getpwnam(user_name))) {
			slprintf(err_str, err_str_len - 1, "User %s does not \
exist in system password file (usually /etc/passwd). Cannot add \
account without a valid local system user.\n", user_name);
			return False;
		}
	}

	/* Calculate the MD4 hash (NT compatible) of the new password. */
	nt_lm_owf_gen(new_passwd, new_nt_p16, new_p16);

	/*
	 * Open the smbpaswd file.
	 */
	vp = startsmbpwent(True);
	if (!vp && errno == ENOENT) {
		FILE *fp;
		slprintf(msg_str,msg_str_len-1,
			"smbpasswd file did not exist - attempting to create it.\n");
		fp = sys_fopen(lp_smb_passwd_file(), "w");
		if (fp) {
			fprintf(fp, "# Samba SMB password file\n");
			fclose(fp);
			vp = startsmbpwent(True);
		}
	}

	if (!vp) {
		slprintf(err_str, err_str_len-1, "Cannot open file %s. Error was %s\n",
			lp_smb_passwd_file(), strerror(errno) );
		return False;
	}
  
	/* Get the smb passwd entry for this user */
	smb_pwent = getsmbpwnam(user_name);
	if (smb_pwent == NULL) {
		if(!(local_flags & LOCAL_ADD_USER)) {
			slprintf(err_str, err_str_len-1,
				"Failed to find entry for user %s.\n", user_name);
			endsmbpwent(vp);
			return False;
		}

		if (add_new_user(user_name, pwd->pw_uid, local_flags, new_p16, new_nt_p16)) {
			slprintf(msg_str, msg_str_len-1, "Added user %s.\n", user_name);
			endsmbpwent(vp);
			return True;
		} else {
			slprintf(err_str, err_str_len-1, "Failed to add entry for user %s.\n", user_name);
			endsmbpwent(vp);
			return False;
		}
	} else {
		/* the entry already existed */
		local_flags &= ~LOCAL_ADD_USER;
	}

	/*
	 * We are root - just write the new password
	 * and the valid last change time.
	 */

	if(local_flags & LOCAL_DISABLE_USER) {
		smb_pwent->acct_ctrl |= ACB_DISABLED;
	} else if (local_flags & LOCAL_ENABLE_USER) {
		if(smb_pwent->smb_passwd == NULL) {
			smb_pwent->smb_passwd = new_p16;
			smb_pwent->smb_nt_passwd = new_nt_p16;
		}
		smb_pwent->acct_ctrl &= ~ACB_DISABLED;
	} else if (local_flags & LOCAL_SET_NO_PASSWORD) {
		smb_pwent->acct_ctrl |= ACB_PWNOTREQ;
		/* This is needed to preserve ACB_PWNOTREQ in mod_smbfilepwd_entry */
		smb_pwent->smb_passwd = NULL;
		smb_pwent->smb_nt_passwd = NULL;
	} else {
		/*
		 * If we're dealing with setting a completely empty user account
		 * ie. One with a password of 'XXXX', but not set disabled (like
		 * an account created from scratch) then if the old password was
		 * 'XX's then getsmbpwent will have set the ACB_DISABLED flag.
		 * We remove that as we're giving this user their first password
		 * and the decision hasn't really been made to disable them (ie.
		 * don't create them disabled). JRA.
		 */
		if((smb_pwent->smb_passwd == NULL) && (smb_pwent->acct_ctrl & ACB_DISABLED))
			smb_pwent->acct_ctrl &= ~ACB_DISABLED;
		smb_pwent->acct_ctrl &= ~ACB_PWNOTREQ;
		smb_pwent->smb_passwd = new_p16;
		smb_pwent->smb_nt_passwd = new_nt_p16;
	}
	
	if(local_flags & LOCAL_DELETE_USER) {
		if (del_smbpwd_entry(user_name)==False) {
			slprintf(err_str,err_str_len-1, "Failed to delete entry for user %s.\n", user_name);
			endsmbpwent(vp);
			return False;
		}
		slprintf(msg_str, msg_str_len-1, "Deleted user %s.\n", user_name);
	} else {
		if(mod_smbpwd_entry(smb_pwent,True) == False) {
			slprintf(err_str, err_str_len-1, "Failed to modify entry for user %s.\n", user_name);
			endsmbpwent(vp);
			return False;
		}
		if(local_flags & LOCAL_DISABLE_USER)
			slprintf(msg_str, msg_str_len-1, "Disabled user %s.\n", user_name);
		else if (local_flags & LOCAL_ENABLE_USER)
			slprintf(msg_str, msg_str_len-1, "Enabled user %s.\n", user_name);
		else if (local_flags & LOCAL_SET_NO_PASSWORD)
			slprintf(msg_str, msg_str_len-1, "User %s password set to none.\n", user_name);
	}

	endsmbpwent(vp);

	return True;
}
