/*
 * Unix SMB/Netbios implementation. Version 1.9. SMB parameters and setup
 * Copyright (C) Andrew Tridgell 1992-1998 Modified by Jeremy Allison 1995.
 * Copyright (C) Benny Holmgren 1998 <bigfoot@astrakan.hgs.se> 
 * Copyright (C) Luke Kenneth Casson Leighton 1996-1998.
 * 
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 * 
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 675
 * Mass Ave, Cambridge, MA 02139, USA.
 */

#include "includes.h"

#ifdef WITH_NISPLUS

#ifdef BROKEN_NISPLUS_INCLUDE_FILES

/*
 * The following lines are needed due to buggy include files
 * in Solaris 2.6 which define GROUP in both /usr/include/sys/acl.h and
 * also in /usr/include/rpcsvc/nis.h. The definitions conflict. JRA.
 * Also GROUP_OBJ is defined as 0x4 in /usr/include/sys/acl.h and as
 * an enum in /usr/include/rpcsvc/nis.h.
 */

#if defined(GROUP)
#undef GROUP
#endif

#if defined(GROUP_OBJ)
#undef GROUP_OBJ
#endif

#endif

#include <rpcsvc/nis.h>

extern int      DEBUGLEVEL;
extern pstring samlogon_user;
extern BOOL sam_logon_in_ssb;

static VOLATILE SIG_ATOMIC_T gotalarm;

/***************************************************************

 the fields for the NIS+ table, generated from mknissmbpwtbl.sh, are:

    	name=S,nogw=r 
    	uid=S,nogw=r 
		user_rid=S,nogw=r
		smb_grpid=,nw+r
		group_rid=,nw+r
		acb=,nw+r
		          
    	lmpwd=C,nw=,g=r,o=rm 
    	ntpwd=C,nw=,g=r,o=rm 
		                     
		logon_t=,nw+r 
		logoff_t=,nw+r 
		kick_t=,nw+r 
		pwdlset_t=,nw+r 
		pwdlchg_t=,nw+r 
		pwdmchg_t=,nw+r 
		                
		full_name=,nw+r 
		home_dir=,nw+r 
		dir_drive=,nw+r 
		logon_script=,nw+r 
		profile_path=,nw+r 
		acct_desc=,nw+r 
		workstations=,nw+r 
		                   
		hours=,nw+r 

****************************************************************/

#define NPF_NAME          0
#define NPF_UID           1
#define NPF_USER_RID      2
#define NPF_SMB_GRPID     3
#define NPF_GROUP_RID     4
#define NPF_ACB           5
#define NPF_LMPWD         6
#define NPF_NTPWD         7
#define NPF_LOGON_T       8
#define NPF_LOGOFF_T      9
#define NPF_KICK_T        10
#define NPF_PWDLSET_T     11
#define NPF_PWDLCHG_T     12
#define NPF_PWDMCHG_T     13
#define NPF_FULL_NAME     14
#define NPF_HOME_DIR      15
#define NPF_DIR_DRIVE     16
#define NPF_LOGON_SCRIPT  17
#define NPF_PROFILE_PATH  18
#define NPF_ACCT_DESC     19
#define NPF_WORKSTATIONS  20
#define NPF_HOURS         21

/***************************************************************
 Signal function to tell us we timed out.
****************************************************************/
static void gotalarm_sig(void)
{
  gotalarm = 1;
}

/***************************************************************
 make_nisname_from_user_rid
 ****************************************************************/
static char *make_nisname_from_user_rid(uint32 rid, char *pfile)
{
	static pstring nisname;

	safe_strcpy(nisname, "[user_rid=", sizeof(nisname)-1);
	slprintf(nisname, sizeof(nisname)-1, "%s%d", nisname, rid);
	safe_strcat(nisname, "],", sizeof(nisname)-strlen(nisname)-1);
	safe_strcat(nisname, pfile, sizeof(nisname)-strlen(nisname)-1);

	return nisname;
}

/***************************************************************
 make_nisname_from_uid
 ****************************************************************/
static char *make_nisname_from_uid(int uid, char *pfile)
{
	static pstring nisname;

	safe_strcpy(nisname, "[uid=", sizeof(nisname)-1);
	slprintf(nisname, sizeof(nisname)-1, "%s%d", nisname, uid);
	safe_strcat(nisname, "],", sizeof(nisname)-strlen(nisname)-1);
	safe_strcat(nisname, pfile, sizeof(nisname)-strlen(nisname)-1);

	return nisname;
}

/***************************************************************
 make_nisname_from_name
 ****************************************************************/
static char *make_nisname_from_name(char *user_name, char *pfile)
{
	static pstring nisname;

	safe_strcpy(nisname, "[name=", sizeof(nisname)-1);
	safe_strcat(nisname, user_name, sizeof(nisname) - strlen(nisname) - 1);
	safe_strcat(nisname, "],", sizeof(nisname)-strlen(nisname)-1);
	safe_strcat(nisname, pfile, sizeof(nisname)-strlen(nisname)-1);

	return nisname;
}

/*************************************************************************
 gets a NIS+ attribute
 *************************************************************************/
static void get_single_attribute(nis_object *new_obj, int col,
				char *val, int len)
{
	int entry_len;

	if (new_obj == NULL || val == NULL) return;
	
	entry_len = ENTRY_LEN(new_obj, col);
	if (len > entry_len)
	{
		DEBUG(10,("get_single_attribute: entry length truncated\n"));
		len = entry_len;
	}

	safe_strcpy(val, ENTRY_VAL(new_obj, col), len-1);
}

/************************************************************************
 makes a struct sam_passwd from a NIS+ object.
 ************************************************************************/
static BOOL make_sam_from_nisp_object(struct sam_passwd *pw_buf, nis_object *obj)
{
	int uidval;
	static pstring user_name;
	static pstring full_name;
	static pstring home_dir;
	static pstring home_drive;
	static pstring logon_script;
	static pstring profile_path;
	static pstring acct_desc;
	static pstring workstations;
	static pstring temp;
	static unsigned char smbpwd[16];
	static unsigned char smbntpwd[16];
	
	char *p;

	pdb_init_sam(pw_buf);
	pw_buf->acct_ctrl = ACB_NORMAL;

	pstrcpy(user_name, ENTRY_VAL(obj, NPF_NAME));
	pw_buf->smb_name      = user_name;

	uidval = atoi(ENTRY_VAL(obj, NPF_UID));
	pw_buf->smb_userid    = uidval;		

	/* Check the lanman password column. */
	p = (char *)ENTRY_VAL(obj, NPF_LMPWD);
	if (*p == '*' || *p == 'X') {
	  /* Password deliberately invalid - end here. */
	  DEBUG(10, ("make_sam_from_nisp_object: entry invalidated for user %s\n", user_name));
	  pw_buf->smb_nt_passwd = NULL;
	  pw_buf->smb_passwd = NULL;
	  pw_buf->acct_ctrl |= ACB_DISABLED;
	  return True;
	}
	if (!strncasecmp(p, "NO PASSWORD", 11)) {
	  pw_buf->smb_passwd = NULL;
	  pw_buf->acct_ctrl |= ACB_PWNOTREQ;
	} else {
	  if (strlen(p) != 32 || !pdb_gethexpwd(p, smbpwd))
	    {
	      DEBUG(0, ("make_sam_from_nisp_object: malformed LM pwd entry.\n"));
	      return False;
	    }
	}

	pw_buf->smb_passwd    = smbpwd;

	/* Check the NT password column. */
	p = ENTRY_VAL(obj, NPF_NTPWD);
	if (*p != '*' && *p != 'X') {
	  if (strlen(p) != 32 || !pdb_gethexpwd(p, smbntpwd))
	    {
	      DEBUG(0, ("make_smb_from_nisp_object: malformed NT pwd entry\n"));
	      return False;
	    }
	  pw_buf->smb_nt_passwd = smbntpwd;
	}

	p = (char *)ENTRY_VAL(obj, NPF_ACB);
	if (*p == '[')
	  {
	    pw_buf->acct_ctrl = pdb_decode_acct_ctrl(p);
	    
	    /* Must have some account type set. */
	    if(pw_buf->acct_ctrl == 0)
	      pw_buf->acct_ctrl = ACB_NORMAL;
	    
	    /* Now try and get the last change time. */
	    if(*p == ']')
	      p++;
	    if(*p == ':') {
	      p++;
	      if(*p && (StrnCaseCmp(p, "LCT-", 4)==0)) {
		int i;
		p += 4;
		for(i = 0; i < 8; i++) {
		  if(p[i] == '\0' || !isxdigit(p[i]))
		    break;
		}
		if(i == 8) {
		  /*
		   * p points at 8 characters of hex digits - 
		   * read into a time_t as the seconds since
		   * 1970 that the password was last changed.
		   */
		  pw_buf->pass_last_set_time = (time_t)strtol(p, NULL, 16);
		}
	      }
	    }
	} else {
	  /* 'Old' style file. Fake up based on user name. */
	  /*
	   * Currently trust accounts are kept in the same
	   * password file as 'normal accounts'. If this changes
	   * we will have to fix this code. JRA.
	   */
	  if(pw_buf->smb_name[strlen(pw_buf->smb_name) - 1] == '$') {
	    pw_buf->acct_ctrl &= ~ACB_NORMAL;
	    pw_buf->acct_ctrl |= ACB_WSTRUST;
	  }
	}

	get_single_attribute(obj, NPF_SMB_GRPID, temp, sizeof(pstring));
	pw_buf->smb_grpid = atoi(temp);

	get_single_attribute(obj, NPF_USER_RID, temp, sizeof(pstring));
	pw_buf->user_rid = (strlen(temp) > 0) ?
	  strtol(temp, NULL, 16) : pdb_uid_to_user_rid (pw_buf->smb_userid);

	if (pw_buf->smb_name[strlen(pw_buf->smb_name)-1] != '$') {
	  
	  /* XXXX hack to get standard_sub_basic() to use sam logon username */
	  /* possibly a better way would be to do a become_user() call */
	  pstrcpy(samlogon_user, pw_buf->smb_name);
	  sam_logon_in_ssb = True;
	  
	  get_single_attribute(obj, NPF_GROUP_RID, temp, sizeof(pstring));
	  pw_buf->group_rid = (strlen(temp) > 0) ?
	    strtol(temp, NULL, 16) : pdb_gid_to_group_rid (pw_buf->smb_grpid);
	  
	  get_single_attribute(obj, NPF_FULL_NAME, full_name, sizeof(pstring));
#if 1
	  /* It seems correct to use the global values - but in that case why
	   * do we want these NIS+ entries anyway ??
	   */
	  pstrcpy(logon_script , lp_logon_script       ());
	  pstrcpy(profile_path , lp_logon_path         ());
	  pstrcpy(home_drive   , lp_logon_drive        ());
	  pstrcpy(home_dir     , lp_logon_home         ());
#else
	  get_single_attribute(obj, NPF_LOGON_SCRIPT, logon_script, sizeof(pstring));
	  get_single_attribute(obj, NPF_PROFILE_PATH, profile_path, sizeof(pstring));
	  get_single_attribute(obj, NPF_DIR_DRIVE, home_drive, sizeof(pstring));
	  get_single_attribute(obj, NPF_HOME_DIR, home_dir, sizeof(pstring));
#endif
	  get_single_attribute(obj, NPF_ACCT_DESC, acct_desc, sizeof(pstring));
	  get_single_attribute(obj, NPF_WORKSTATIONS, workstations, sizeof(pstring));
	  
	  sam_logon_in_ssb = False;

	} else {
	  
	  pw_buf->group_rid = DOMAIN_GROUP_RID_USERS; /* lkclXXXX this is OBSERVED behaviour by NT PDCs, enforced here. */
	  
	  pstrcpy(full_name    , "");
	  pstrcpy(logon_script , "");
	  pstrcpy(profile_path , "");
	  pstrcpy(home_drive   , "");
	  pstrcpy(home_dir     , "");
	  pstrcpy(acct_desc    , "");
	  pstrcpy(workstations , "");
	}

	pw_buf->full_name    = full_name;
	pw_buf->home_dir     = home_dir;
	pw_buf->dir_drive    = home_drive;
	pw_buf->logon_script = logon_script;
	pw_buf->profile_path = profile_path;
	pw_buf->acct_desc    = acct_desc;
	pw_buf->workstations = workstations;

	pw_buf->unknown_str = NULL; /* don't know, yet! */
	pw_buf->munged_dial = NULL; /* "munged" dial-back telephone number */

	pw_buf->unknown_3 = 0xffffff; /* don't know */
	pw_buf->logon_divs = 168; /* hours per week */
	pw_buf->hours_len = 21; /* 21 times 8 bits = 168 */
	memset(pw_buf->hours, 0xff, pw_buf->hours_len); /* available at all hours */
	pw_buf->unknown_5 = 0x00020000; /* don't know */
	pw_buf->unknown_6 = 0x000004ec; /* don't know */

	return True;
}

/************************************************************************
 makes a struct sam_passwd from a NIS+ result.
 ************************************************************************/
static BOOL make_sam_from_nisresult(struct sam_passwd *pw_buf, nis_result *result)
{
	if (pw_buf == NULL || result == NULL) return False;

	if (result->status != NIS_SUCCESS && result->status != NIS_NOTFOUND)
	{
		DEBUG(0, ("make_sam_from_nisresult: NIS+ lookup failure: %s\n",
		           nis_sperrno(result->status)));
		return False;
	}

	/* User not found. */
	if (NIS_RES_NUMOBJ(result) <= 0)
	{
		DEBUG(10, ("make_sam_from_nisresult: user not found in NIS+\n"));
		return False;
	}

	if (NIS_RES_NUMOBJ(result) > 1)
	{
		DEBUG(10, ("make_sam_from_nisresult: WARNING: Multiple entries for user in NIS+ table!\n"));
	}

	/* Grab the first hit. */
	return make_sam_from_nisp_object(pw_buf, &NIS_RES_OBJECT(result)[0]);
      }

/***************************************************************
 calls nis_list, returns results.
 ****************************************************************/
static nis_result *nisp_get_nis_list(char *nis_name)
{
	nis_result *result;
	result = nis_list(nis_name, FOLLOW_PATH|EXPAND_NAME|HARD_LOOKUP,NULL,NULL);

	alarm(0);
	CatchSignal(SIGALRM, SIGNAL_CAST SIG_DFL);

	if (gotalarm)
	{
		DEBUG(0,("nisp_get_nis_list: NIS+ lookup time out\n"));
		nis_freeresult(result);
		return NULL;
	}
	return result;
}



struct nisp_enum_info
{
	nis_result *result;
	int enum_entry;
};

/***************************************************************
 Start to enumerate the nisplus passwd list. Returns a void pointer
 to ensure no modification outside this module.

 do not call this function directly.  use passdb.c instead.

 ****************************************************************/
static void *startnisppwent(BOOL update)
{
	static struct nisp_enum_info res;
	res.result = nisp_get_nis_list(lp_smb_passwd_file());
	res.enum_entry = 0;
	return res.result != NULL ? &res : NULL;
}

/***************************************************************
 End enumeration of the nisplus passwd list.
****************************************************************/
static void endnisppwent(void *vp)
{
  struct nisp_enum_info *res = (struct nisp_enum_info *)vp;
  nis_freeresult(res->result);
  DEBUG(7,("endnisppwent: freed enumeration list\n"));
}

/*************************************************************************
 Routine to return the next entry in the nisplus passwd list.

 do not call this function directly.  use passdb.c instead.

 *************************************************************************/
static struct sam_passwd *getnisp21pwent(void *vp)
{
  struct nisp_enum_info *res = (struct nisp_enum_info *)vp;
  static struct sam_passwd pw_buf;
  int which;
  BOOL ret;
  
  if (res == NULL || (int)(res->enum_entry) < 0 ||
      (int)(res->enum_entry) > (NIS_RES_NUMOBJ(res->result) - 1)) {
    ret = False;
  } else {
    which = (int)(res->enum_entry);
    ret = make_sam_from_nisp_object(&pw_buf,
		 &NIS_RES_OBJECT(res->result)[which]);
    if (ret && which < (NIS_RES_NUMOBJ(res->result) - 1))
      (int)(res->enum_entry)++;
  }
  
  return ret ? &pw_buf : NULL;
}

/*************************************************************************
 Return the current position in the nisplus passwd list as an SMB_BIG_UINT.
 This must be treated as an opaque token.

 do not call this function directly.  use passdb.c instead.

*************************************************************************/
static SMB_BIG_UINT getnisppwpos(void *vp)
{
  struct nisp_enum_info *res = (struct nisp_enum_info *)vp;
  return (SMB_BIG_UINT)(res->enum_entry);
}

/*************************************************************************
 Set the current position in the nisplus passwd list from SMB_BIG_UINT.
 This must be treated as an opaque token.

 do not call this function directly.  use passdb.c instead.

*************************************************************************/
static BOOL setnisppwpos(void *vp, SMB_BIG_UINT tok)
{
  struct nisp_enum_info *res = (struct nisp_enum_info *)vp;
  if (tok < (NIS_RES_NUMOBJ(res->result) - 1)) {
    res->enum_entry = tok;
    return True;
  } else {
    return False;
  }
}

/*************************************************************************
 sets a NIS+ attribute
 *************************************************************************/
static void set_single_attribute(nis_object *new_obj, int col,
				char *val, int len, int flags)
{
	if (new_obj == NULL) return;

	ENTRY_VAL(new_obj, col) = val;
	ENTRY_LEN(new_obj, col) = len+1;

	if (flags != 0)
	{
		new_obj->EN_data.en_cols.en_cols_val[col].ec_flags = flags;
	}
}

/************************************************************************
 Routine to add an entry to the nisplus passwd file.

 do not call this function directly.  use passdb.c instead.

*************************************************************************/
static BOOL add_nisp21pwd_entry(struct sam_passwd *newpwd)
{
	char           *pfile;
	char           *nisname;
	nis_result	*nis_user;
	nis_result *result = NULL,
	*tblresult = NULL, 
	*addresult = NULL;
	nis_object new_obj, *obj;

    fstring uid;
	fstring user_rid;
	fstring smb_grpid;
	fstring group_rid;
	fstring acb;
		          
	fstring smb_passwd;
	fstring smb_nt_passwd;

	fstring logon_t;
	fstring logoff_t;
	fstring kickoff_t;
	fstring pwdlset_t;
	fstring pwdlchg_t;
	fstring pwdmchg_t;

	memset((char *)logon_t  , '\0', sizeof(logon_t  ));
	memset((char *)logoff_t , '\0', sizeof(logoff_t ));
	memset((char *)kickoff_t, '\0', sizeof(kickoff_t));
	memset((char *)pwdlset_t, '\0', sizeof(pwdlset_t));
	memset((char *)pwdlchg_t, '\0', sizeof(pwdlchg_t));
	memset((char *)pwdmchg_t, '\0', sizeof(pwdmchg_t));

	pfile = lp_smb_passwd_file();

	nisname = make_nisname_from_name(newpwd->smb_name, pfile);
	result = nisp_get_nis_list(nisname);
	if (result->status != NIS_SUCCESS && result->status != NIS_NOTFOUND)
	{
		DEBUG(3, ( "add_nis21ppwd_entry: nis_list failure: %s: %s\n",
		            nisname,  nis_sperrno(result->status)));
		nis_freeresult(result);
		return False;
	}   

	if (result->status == NIS_SUCCESS && NIS_RES_NUMOBJ(result) > 0)
	{
		DEBUG(3, ("add_nisp21pwd_entry: User already exists in NIS+ password db: %s\n",
		            pfile));
		nis_freeresult(result);
		return False;
	}

#if 0
	/* User not found. */
	if (!add_user)
	{
		DEBUG(3, ("add_nisp21pwd_entry: User not found in NIS+ password db: %s\n",
		            pfile));
		nis_freeresult(result);
		return False;
	}

#endif

	tblresult = nis_lookup(pfile, FOLLOW_PATH | EXPAND_NAME | HARD_LOOKUP );
	if (tblresult->status != NIS_SUCCESS)
	{
		nis_freeresult(result);
		nis_freeresult(tblresult);
		DEBUG(3, ( "add_nisp21pwd_entry: nis_lookup failure: %s\n",
		            nis_sperrno(tblresult->status)));
		return False;
	}

	new_obj.zo_name   = NIS_RES_OBJECT(tblresult)->zo_name;
	new_obj.zo_domain = NIS_RES_OBJECT(tblresult)->zo_domain;
	new_obj.zo_owner  = NIS_RES_OBJECT(tblresult)->zo_owner;
	new_obj.zo_group  = NIS_RES_OBJECT(tblresult)->zo_group;
	new_obj.zo_access = NIS_RES_OBJECT(tblresult)->zo_access;
	new_obj.zo_ttl    = NIS_RES_OBJECT(tblresult)->zo_ttl;

	new_obj.zo_data.zo_type = ENTRY_OBJ;

	new_obj.zo_data.objdata_u.en_data.en_type = NIS_RES_OBJECT(tblresult)->zo_data.objdata_u.ta_data.ta_type;
	new_obj.zo_data.objdata_u.en_data.en_cols.en_cols_len = NIS_RES_OBJECT(tblresult)->zo_data.objdata_u.ta_data.ta_maxcol;
	new_obj.zo_data.objdata_u.en_data.en_cols.en_cols_val = calloc(new_obj.zo_data.objdata_u.en_data.en_cols.en_cols_len, sizeof(entry_col));

	if (new_obj.zo_data.objdata_u.en_data.en_cols.en_cols_val == NULL)
	{
		DEBUG(0, ("add_nisp21pwd_entry: memory allocation failure\n"));
		nis_freeresult(result);
		nis_freeresult(tblresult);
		return False;
	}

	pdb_sethexpwd(smb_passwd   , newpwd->smb_passwd   , newpwd->acct_ctrl);
	pdb_sethexpwd(smb_nt_passwd, newpwd->smb_nt_passwd, newpwd->acct_ctrl);

	newpwd->pass_last_set_time = (time_t)time(NULL);
	newpwd->logon_time = (time_t)-1;
	newpwd->logoff_time = (time_t)-1;
	newpwd->kickoff_time = (time_t)-1;
	newpwd->pass_can_change_time = (time_t)-1;
	newpwd->pass_must_change_time = (time_t)-1;

	slprintf(logon_t,   13, "LNT-%08X", (uint32)newpwd->logon_time);
	slprintf(logoff_t,  13, "LOT-%08X", (uint32)newpwd->logoff_time);
	slprintf(kickoff_t, 13, "KOT-%08X", (uint32)newpwd->kickoff_time);
	slprintf(pwdlset_t, 13, "LCT-%08X", (uint32)newpwd->pass_last_set_time);
	slprintf(pwdlchg_t, 13, "CCT-%08X", (uint32)newpwd->pass_can_change_time);
	slprintf(pwdmchg_t, 13, "MCT-%08X", (uint32)newpwd->pass_must_change_time);

	slprintf(uid, sizeof(uid), "%u", newpwd->smb_userid);
	slprintf(user_rid, sizeof(user_rid), "0x%x", newpwd->user_rid);
	slprintf(smb_grpid, sizeof(smb_grpid), "%u", newpwd->smb_grpid);
	slprintf(group_rid, sizeof(group_rid), "0x%x", newpwd->group_rid);

	safe_strcpy(acb, pdb_encode_acct_ctrl(newpwd->acct_ctrl, NEW_PW_FORMAT_SPACE_PADDED_LEN), sizeof(acb)-1); 

	set_single_attribute(&new_obj, NPF_NAME          , newpwd->smb_name     , strlen(newpwd->smb_name)     , 0);
	set_single_attribute(&new_obj, NPF_UID           , uid                  , strlen(uid)                  , 0);
	set_single_attribute(&new_obj, NPF_USER_RID      , user_rid             , strlen(user_rid)             , 0);
	set_single_attribute(&new_obj, NPF_SMB_GRPID     , smb_grpid            , strlen(smb_grpid)            , 0);
	set_single_attribute(&new_obj, NPF_GROUP_RID     , group_rid            , strlen(group_rid)            , 0);
	set_single_attribute(&new_obj, NPF_ACB           , acb                  , strlen(acb)                  , 0);
	set_single_attribute(&new_obj, NPF_LMPWD         , smb_passwd           , strlen(smb_passwd)           , EN_CRYPT);
	set_single_attribute(&new_obj, NPF_NTPWD         , smb_nt_passwd        , strlen(smb_nt_passwd)        , EN_CRYPT);
	set_single_attribute(&new_obj, NPF_LOGON_T       , logon_t              , strlen(logon_t)              , 0);
	set_single_attribute(&new_obj, NPF_LOGOFF_T      , logoff_t             , strlen(logoff_t)             , 0);
	set_single_attribute(&new_obj, NPF_KICK_T        , kickoff_t            , strlen(kickoff_t)            , 0);
	set_single_attribute(&new_obj, NPF_PWDLSET_T     , pwdlset_t            , strlen(pwdlset_t)            , 0);
	set_single_attribute(&new_obj, NPF_PWDLCHG_T     , pwdlchg_t            , strlen(pwdlchg_t)            , 0);
	set_single_attribute(&new_obj, NPF_PWDMCHG_T     , pwdmchg_t            , strlen(pwdmchg_t)            , 0);
#if 0
	set_single_attribute(&new_obj, NPF_FULL_NAME     , newpwd->full_name    , strlen(newpwd->full_name)    , 0);
	set_single_attribute(&new_obj, NPF_HOME_DIR      , newpwd->home_dir     , strlen(newpwd->home_dir)     , 0);
	set_single_attribute(&new_obj, NPF_DIR_DRIVE     , newpwd->dir_drive    , strlen(newpwd->dir_drive)    , 0);
	set_single_attribute(&new_obj, NPF_LOGON_SCRIPT  , newpwd->logon_script , strlen(newpwd->logon_script) , 0);
	set_single_attribute(&new_obj, NPF_PROFILE_PATH  , newpwd->profile_path , strlen(newpwd->profile_path) , 0);
	set_single_attribute(&new_obj, NPF_ACCT_DESC     , newpwd->acct_desc    , strlen(newpwd->acct_desc)    , 0);
	set_single_attribute(&new_obj, NPF_WORKSTATIONS  , newpwd->workstations , strlen(newpwd->workstations) , 0);
	set_single_attribute(&new_obj, NPF_HOURS         , newpwd->hours        , newpwd->hours_len            , 0);
#endif

	obj = &new_obj;

	addresult = nis_add_entry(pfile, obj, ADD_OVERWRITE | FOLLOW_PATH | EXPAND_NAME | HARD_LOOKUP);


	if (addresult->status != NIS_SUCCESS)
	{
		DEBUG(3, ( "add_nisp21pwd_entry: NIS+ table update failed: %s\n",
		            nisname, nis_sperrno(addresult->status)));
		nis_freeresult(tblresult);
		nis_freeresult(addresult);
		nis_freeresult(result);
		return False;
	}

	nis_freeresult(tblresult);
	nis_freeresult(addresult);
	nis_freeresult(result);

	return True;
}

/************************************************************************
 Routine to search the nisplus passwd file for an entry matching the username.
 and then modify its password entry. We can't use the startnisppwent()/
 getnisppwent()/endnisppwent() interfaces here as we depend on looking
 in the actual file to decide how much room we have to write data.
 override = False, normal
 override = True, override XXXXXXXX'd out password or NO PASS

 do not call this function directly.  use passdb.c instead.

************************************************************************/
static BOOL mod_nisp21pwd_entry(struct sam_passwd* pwd, BOOL override)
{
  char *oldnislmpwd, *oldnisntpwd, *oldnisacb, *oldnislct, *user_name;
  char lmpwd[33], ntpwd[33], lct[13];
  nis_result *result, *addresult;
  nis_object *obj;
  fstring acb;
  pstring nisname;
  BOOL got_pass_last_set_time, ret;
  int i;
  
  if (!*lp_smb_passwd_file())
    {
      DEBUG(0, ("mod_getnisp21pwd_entry: no SMB password file set\n"));
      return False;
    }
  
  DEBUG(10, ("mod_getnisp21pwd_entry: search by name: %s\n", pwd->smb_name));
  DEBUG(10, ("mod_getnisp21pwd_entry: using NIS+ table %s\n", lp_smb_passwd_file()));
  
  slprintf(nisname, sizeof(nisname)-1, "[name=%s],%s", pwd->smb_name, lp_smb_passwd_file());
  
  /* Search the table. */
  gotalarm = 0;
  CatchSignal(SIGALRM, SIGNAL_CAST gotalarm_sig);
  alarm(5);
  
  result = nis_list(nisname, FOLLOW_PATH | EXPAND_NAME | HARD_LOOKUP, NULL, NULL);
  
  alarm(0);
  CatchSignal(SIGALRM, SIGNAL_CAST SIG_DFL);
  
  if (gotalarm)
    {
      DEBUG(0,("mod_getnisp21pwd_entry: NIS+ lookup time out\n"));
      nis_freeresult(result);
      return False;
    }
  
  if(result->status != NIS_SUCCESS || NIS_RES_NUMOBJ(result) <= 0) {
    /* User not found. */
    DEBUG(0,("mod_getnisp21pwd_entry: user not found in NIS+\n"));
    nis_freeresult(result);
    return False;
  }

  DEBUG(6,("mod_getnisp21pwd_entry: entry exists\n"));
  
  obj = NIS_RES_OBJECT(result);
  
  user_name = ENTRY_VAL(obj, NPF_NAME);
  oldnislmpwd = ENTRY_VAL(obj, NPF_LMPWD);
  oldnisntpwd = ENTRY_VAL(obj, NPF_NTPWD);
  oldnisacb = ENTRY_VAL(obj, NPF_ACB);
  oldnislct = ENTRY_VAL(obj, NPF_PWDLSET_T);
  
  
  if (!override && (*oldnislmpwd == '*' || *oldnislmpwd == 'X' ||
		    *oldnisntpwd == '*' || *oldnisntpwd == 'X')) {
    /* Password deliberately invalid - end here. */
    DEBUG(10, ("mod_nisp21pwd_entry: entry invalidated for user %s\n", user_name));
    nis_freeresult(result);
    return False;
  }

  if (strlen(oldnislmpwd) != 32 || strlen(oldnisntpwd) != 32) {
    DEBUG(0, ("mod_nisp21pwd_entry: malformed password entry (incorrect length)\n"));
    nis_freeresult(result);
    return False;
  }



  /* 
   * Now check if the account info and the password last
   * change time is available.
   */

  /*
   * If both NT and lanman passwords are provided - reset password
   * not required flag.
   */

  if(pwd->smb_passwd != NULL || pwd->smb_nt_passwd != NULL) {
    /* Require password in the future (should ACB_DISABLED also be reset?) */
    pwd->acct_ctrl &= ~(ACB_PWNOTREQ);
  }

  if (*oldnisacb == '[') {
    
    i = 0;
    acb[i++] = oldnisacb[i];
    while(i < (sizeof(fstring) - 2) && (oldnisacb[i] != ']'))
      acb[i] = oldnisacb[i++];
    
    acb[i++] = ']';
    acb[i++] = '\0';
    
    if (i == NEW_PW_FORMAT_SPACE_PADDED_LEN) {
      /*
       * We are using a new format, space padded
       * acct ctrl field. Encode the given acct ctrl
       * bits into it.
       */
      fstrcpy(acb, pdb_encode_acct_ctrl(pwd->acct_ctrl, NEW_PW_FORMAT_SPACE_PADDED_LEN));
    } else {
      /*
       * If using the old format and the ACB_DISABLED or
       * ACB_PWNOTREQ are set then set the lanman and NT passwords to NULL
       * here as we have no space to encode the change.
       */
      if(pwd->acct_ctrl & (ACB_DISABLED|ACB_PWNOTREQ)) {
        pwd->smb_passwd = NULL;
        pwd->smb_nt_passwd = NULL;
      }
    }
    
    /* Now do the LCT stuff. */
    if (StrnCaseCmp(oldnislct, "LCT-", 4) == 0) {
      
      for(i = 0; i < 8; i++) {
	if(oldnislct[i+4] == '\0' || !isxdigit(oldnislct[i+4]))
	  break;
      }
      if (i == 8) {
	/*
	 * p points at 8 characters of hex digits -
	 * read into a time_t as the seconds since
	 * 1970 that the password was last changed.
	 */
	got_pass_last_set_time = True;
      } /* i == 8 */
    } /* StrnCaseCmp() */

  } /* p == '[' */

  /* Entry is correctly formed. */



  /* Create the 32 byte representation of the new p16 */
  if(pwd->smb_passwd != NULL) {
    for (i = 0; i < 16; i++) {
      slprintf(&lmpwd[i*2], 32, "%02X", (uchar) pwd->smb_passwd[i]);
    }
  } else {
    if(pwd->acct_ctrl & ACB_PWNOTREQ)
      fstrcpy(lmpwd, "NO PASSWORDXXXXXXXXXXXXXXXXXXXXX");
    else
      fstrcpy(lmpwd, "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
  }

  /* Add on the NT md4 hash */
  if (pwd->smb_nt_passwd != NULL) {
    for (i = 0; i < 16; i++) {
      slprintf(&ntpwd[i*2], 32, "%02X", (uchar) pwd->smb_nt_passwd[i]);
    }
  } else {
    if(pwd->acct_ctrl & ACB_PWNOTREQ)
      fstrcpy(ntpwd, "NO PASSWORDXXXXXXXXXXXXXXXXXXXXX");
    else
      fstrcpy(ntpwd, "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
  }
  
  pwd->pass_last_set_time = time(NULL);

  if(got_pass_last_set_time) {
    slprintf(lct, 12, "LCT-%08X", (uint32)pwd->pass_last_set_time);
  }

  set_single_attribute(obj, NPF_LMPWD,     lmpwd, strlen(lmpwd), EN_CRYPT);
  set_single_attribute(obj, NPF_NTPWD,     ntpwd, strlen(ntpwd), EN_CRYPT);
  set_single_attribute(obj, NPF_ACB,       acb,   strlen(acb),   0);
  set_single_attribute(obj, NPF_PWDLSET_T, lct,   strlen(lct),   0);

  addresult =
    nis_add_entry(lp_smb_passwd_file(), obj, 
		  ADD_OVERWRITE | FOLLOW_PATH | EXPAND_NAME | HARD_LOOKUP);
  
  if(addresult->status != NIS_SUCCESS) {
    DEBUG(0, ("mod_nisp21pwd_entry: NIS+ table update failed: %s %s\n", nisname, nis_sperrno(addresult->status)));
    nis_freeresult(addresult);
    nis_freeresult(result);
    return False;
  }
    
  DEBUG(6,("mod_nisp21pwd_entry: password changed\n"));

  nis_freeresult(addresult);
  nis_freeresult(result);
  
  return True;
}
 

/*************************************************************************
 Routine to search the nisplus passwd file for an entry matching the username
 *************************************************************************/
static struct sam_passwd *getnisp21pwnam(char *name)
{
	/* Static buffers we will return. */
	static struct sam_passwd pw_buf;
	nis_result *result;
	pstring nisname;
	BOOL ret;

	if (!*lp_smb_passwd_file())
	{
		DEBUG(0, ("No SMB password file set\n"));
		return NULL;
	}

	DEBUG(10, ("getnisp21pwnam: search by name: %s\n", name));
	DEBUG(10, ("getnisp21pwnam: using NIS+ table %s\n", lp_smb_passwd_file()));

	slprintf(nisname, sizeof(nisname)-1, "[name=%s],%s", name, lp_smb_passwd_file());

	/* Search the table. */
	gotalarm = 0;
	CatchSignal(SIGALRM, SIGNAL_CAST gotalarm_sig);
	alarm(5);

	result = nis_list(nisname, FOLLOW_PATH | EXPAND_NAME | HARD_LOOKUP, NULL, NULL);

	alarm(0);
	CatchSignal(SIGALRM, SIGNAL_CAST SIG_DFL);

	if (gotalarm)
	{
		DEBUG(0,("getnisp21pwnam: NIS+ lookup time out\n"));
		nis_freeresult(result);
		return NULL;
	}

	ret = make_sam_from_nisresult(&pw_buf, result);
	nis_freeresult(result);

	return ret ? &pw_buf : NULL;
}

/*************************************************************************
 Routine to search the nisplus passwd file for an entry matching the username
 *************************************************************************/
static struct sam_passwd *getnisp21pwrid(uint32 rid)
{
	/* Static buffers we will return. */
	static struct sam_passwd pw_buf;
	nis_result *result;
	char *nisname;
	BOOL ret;

	if (!*lp_smb_passwd_file())
	{
		DEBUG(0, ("getnisp21pwrid: no SMB password file set\n"));
		return NULL;
	}

	DEBUG(10, ("getnisp21pwrid: search by rid: %x\n", rid));
	DEBUG(10, ("getnisp21pwrid: using NIS+ table %s\n", lp_smb_passwd_file()));

	nisname = make_nisname_from_user_rid(rid, lp_smb_passwd_file());

	/* Search the table. */
	gotalarm = 0;
	CatchSignal(SIGALRM, SIGNAL_CAST gotalarm_sig);
	alarm(5);

	result = nis_list(nisname, FOLLOW_PATH | EXPAND_NAME | HARD_LOOKUP, NULL, NULL);

	alarm(0);
	CatchSignal(SIGALRM, SIGNAL_CAST SIG_DFL);

	if (gotalarm)
	{
		DEBUG(0,("getnisp21pwrid: NIS+ lookup time out\n"));
		nis_freeresult(result);
		return NULL;
	}

	ret = make_sam_from_nisresult(&pw_buf, result);
	nis_freeresult(result);

	return ret ? &pw_buf : NULL;
}

/*
 * Derived functions for NIS+.
 */

static struct smb_passwd *getnisppwent(void *vp)
{
	return pdb_sam_to_smb(getnisp21pwent(vp));
}

static BOOL add_nisppwd_entry(struct smb_passwd *newpwd)
{
 	return add_nisp21pwd_entry(pdb_smb_to_sam(newpwd));
}

static BOOL mod_nisppwd_entry(struct smb_passwd* pwd, BOOL override)
{
 	return mod_nisp21pwd_entry(pdb_smb_to_sam(pwd), override);
}

static BOOL del_nisppwd_entry(const char *name)
{
 	return False; /* Dummy. */
}

static struct smb_passwd *getnisppwnam(char *name)
{
	return pdb_sam_to_smb(getnisp21pwnam(name));
}

static struct sam_passwd *getnisp21pwuid(uid_t smb_userid)
{
	return getnisp21pwrid(pdb_uid_to_user_rid(smb_userid));
}

static struct smb_passwd *getnisppwrid(uint32 user_rid)
{
	return pdb_sam_to_smb(getnisp21pwuid(pdb_user_rid_to_uid(user_rid)));
}

static struct smb_passwd *getnisppwuid(uid_t smb_userid)
{
	return pdb_sam_to_smb(getnisp21pwuid(smb_userid));
}

static struct sam_disp_info *getnispdispnam(char *name)
{
	return pdb_sam_to_dispinfo(getnisp21pwnam(name));
}

static struct sam_disp_info *getnispdisprid(uint32 rid)
{
	return pdb_sam_to_dispinfo(getnisp21pwrid(rid));
}

static struct sam_disp_info *getnispdispent(void *vp)
{
	return pdb_sam_to_dispinfo(getnisp21pwent(vp));
}

static struct passdb_ops nispasswd_ops = {
  startnisppwent,
  endnisppwent,
  getnisppwpos,
  setnisppwpos,
  getnisppwnam,
  getnisppwuid,
  getnisppwrid,
  getnisppwent,
  add_nisppwd_entry,
  mod_nisppwd_entry,
  del_nisppwd_entry,
  getnisp21pwent,
  getnisp21pwnam,
  getnisp21pwuid,
  getnisp21pwrid, 
  add_nisp21pwd_entry,
  mod_nisp21pwd_entry,
  getnispdispnam,
  getnispdisprid,
  getnispdispent
};

struct passdb_ops *nisplus_initialize_password_db(void)
{
  return &nispasswd_ops;
}
 
#else
 void nisplus_dummy_function(void);
 void nisplus_dummy_function(void) { } /* stop some compilers complaining */
#endif /* WITH_NISPLUS */

/* useful code i can't bring myself to delete */
#if 0
static void useful_code(void) {
	/* checks user in unix password database.  don't want to do that, here. */
	nisname = make_nisname_from_name(newpwd->smb_name, "passwd.org_dir");

	nis_user = nis_list(nisname, FOLLOW_PATH | EXPAND_NAME | HARD_LOOKUP, NULL, NULL);

	if (nis_user->status != NIS_SUCCESS || NIS_RES_NUMOBJ(nis_user) <= 0)
	{
		DEBUG(3, ("useful_code: Unable to get NIS+ passwd entry for user: %s.\n",
		        nis_sperrno(nis_user->status)));
		return False;
	}

	user_obj = NIS_RES_OBJECT(nis_user);
	make_nisname_from_name(ENTRY_VAL(user_obj,0), pfile);
}
#endif
