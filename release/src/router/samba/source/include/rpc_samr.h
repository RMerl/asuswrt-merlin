/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   SMB parameters and setup
   Copyright (C) Andrew Tridgell 1992-1997
   Copyright (C) Luke Kenneth Casson Leighton 1996-1997
   Copyright (C) Paul Ashton 1997
   
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

#ifndef _RPC_SAMR_H /* _RPC_SAMR_H */
#define _RPC_SAMR_H 


#include "rpc_misc.h"


/*******************************************************************
 the following information comes from a QuickView on samsrv.dll,
 and gives an idea of exactly what is needed:
 
SamrAddMemberToAlias
SamrAddMemberToGroup
SamrAddMultipleMembersToAlias
SamrChangePasswordUser
x SamrCloseHandle
x SamrConnect
SamrCreateAliasInDomain
SamrCreateGroupInDomain
SamrCreateUserInDomain
SamrDeleteAlias
SamrDeleteGroup
SamrDeleteUser
x SamrEnumerateAliasesInDomain
SamrEnumerateDomainsInSamServer
x SamrEnumerateGroupsInDomain
x SamrEnumerateUsersInDomain
SamrGetUserDomainPasswordInformation
SamrLookupDomainInSamServer
? SamrLookupIdsInDomain
x SamrLookupNamesInDomain
x SamrOpenAlias
x SamrOpenDomain
SamrOpenGroup
x SamrOpenUser
x SamrQueryDisplayInformation
x SamrQueryInformationAlias
SamrQueryInformationDomain
? SamrQueryInformationUser
SamrQuerySecurityObject
SamrRemoveMemberFromAlias
SamrRemoveMemberFromForiegnDomain
SamrRemoveMemberFromGroup
SamrRemoveMultipleMembersFromAlias
SamrSetInformationAlias
SamrSetInformationDomain
SamrSetInformationGroup
SamrSetInformationUser
SamrSetMemberAttributesOfGroup
SamrSetSecurityObject
SamrShutdownSamServer
SamrTestPrivateFunctionsDomain
SamrTestPrivateFunctionsUser

********************************************************************/

#define SAMR_CLOSE_HND         0x01
#define SAMR_OPEN_DOMAIN       0x07
#define SAMR_QUERY_DOMAIN_INFO 0x08
#define SAMR_LOOKUP_IDS        0x10
#define SAMR_LOOKUP_NAMES      0x11
#define SAMR_UNKNOWN_3         0x03
#define SAMR_QUERY_DISPINFO    0x28
#define SAMR_OPEN_USER         0x22
#define SAMR_QUERY_USERINFO    0x24
#define SAMR_QUERY_USERGROUPS  0x27
#define SAMR_UNKNOWN_12        0x12
#define SAMR_UNKNOWN_21        0x21
#define SAMR_UNKNOWN_2C        0x2c
#define SAMR_UNKNOWN_32        0x32
#define SAMR_UNKNOWN_34        0x34
#define SAMR_CHGPASSWD_USER    0x37
#define SAMR_UNKNOWN_38        0x38
#define SAMR_CONNECT           0x39
#define SAMR_CONNECT_ANON      0x00
#define SAMR_OPEN_ALIAS        0x1b
#define SAMR_QUERY_ALIASINFO   0x1c
#define SAMR_ENUM_DOM_USERS    0x0d
#define SAMR_ENUM_DOM_ALIASES  0x0f
#define SAMR_ENUM_DOM_GROUPS   0x30


typedef struct logon_hours_info
{
	uint32 len; /* normally 21 bytes */
	uint8 hours[32];

} LOGON_HRS;

/* SAM_USER_INFO_21 */
typedef struct sam_user_info_21
{
	NTTIME logon_time;            /* logon time */
	NTTIME logoff_time;           /* logoff time */
	NTTIME kickoff_time;          /* kickoff time */
	NTTIME pass_last_set_time;    /* password last set time */
	NTTIME pass_can_change_time;  /* password can change time */
	NTTIME pass_must_change_time; /* password must change time */

	UNIHDR hdr_user_name;    /* username unicode string header */
	UNIHDR hdr_full_name;    /* user's full name unicode string header */
	UNIHDR hdr_home_dir;     /* home directory unicode string header */
	UNIHDR hdr_dir_drive;    /* home drive unicode string header */
	UNIHDR hdr_logon_script; /* logon script unicode string header */
	UNIHDR hdr_profile_path; /* profile path unicode string header */
	UNIHDR hdr_acct_desc  ;  /* user description */
	UNIHDR hdr_workstations; /* comma-separated workstations user can log in from */
	UNIHDR hdr_unknown_str ; /* don't know what this is, yet. */
	UNIHDR hdr_munged_dial ; /* munged path name and dial-back tel number */

	uint8 lm_pwd[16];    /* lm user passwords */
	uint8 nt_pwd[16];    /* nt user passwords */

	uint32 user_rid;      /* Primary User ID */
	uint32 group_rid;     /* Primary Group ID */

	uint16 acb_info; /* account info (ACB_xxxx bit-mask) */
	/* uint8 pad[2] */

	uint32 unknown_3; /* 0x00ff ffff */

	uint16 logon_divs; /* 0x0000 00a8 which is 168 which is num hrs in a week */
	/* uint8 pad[2] */
	uint32 ptr_logon_hrs; /* unknown pointer */

	uint32 unknown_5;     /* 0x0002 0000 */

	uint8 padding1[8];

	UNISTR2 uni_user_name;    /* username unicode string */
	UNISTR2 uni_full_name;    /* user's full name unicode string */
	UNISTR2 uni_home_dir;     /* home directory unicode string */
	UNISTR2 uni_dir_drive;    /* home directory drive unicode string */
	UNISTR2 uni_logon_script; /* logon script unicode string */
	UNISTR2 uni_profile_path; /* profile path unicode string */
	UNISTR2 uni_acct_desc  ;  /* user description unicode string */
	UNISTR2 uni_workstations; /* login from workstations unicode string */
	UNISTR2 uni_unknown_str ; /* don't know what this is, yet. */
	UNISTR2 uni_munged_dial ; /* munged path name and dial-back tel number */

	uint32 unknown_6; /* 0x0000 04ec */
	uint32 padding4;

	LOGON_HRS logon_hrs;

} SAM_USER_INFO_21;


/* SAM_USER_INFO_11 */
typedef struct sam_user_info_11
{
	uint8  padding_0[16];  /* 0 - padding 16 bytes */
	NTTIME expiry;         /* expiry time or something? */
	uint8  padding_1[24];  /* 0 - padding 24 bytes */

	UNIHDR hdr_mach_acct;  /* unicode header for machine account */
	uint32 padding_2;      /* 0 - padding 4 bytes */

	uint32 ptr_1;          /* pointer */
	uint8  padding_3[32];  /* 0 - padding 32 bytes */
	uint32 padding_4;      /* 0 - padding 4 bytes */

	uint32 ptr_2;          /* pointer */
	uint32 padding_5;      /* 0 - padding 4 bytes */

	uint32 ptr_3;          /* pointer */
	uint8  padding_6[32];  /* 0 - padding 32 bytes */

	uint32 rid_user;       /* user RID */
	uint32 rid_group;      /* group RID */

	uint16 acct_ctrl;      /* 0080 - ACB_XXXX */
	uint16 unknown_3;      /* 16 bit padding */

	uint16 unknown_4;      /* 0x003f      - 16 bit unknown */
	uint16 unknown_5;      /* 0x003c      - 16 bit unknown */

	uint8  padding_7[16];  /* 0 - padding 16 bytes */
	uint32 padding_8;      /* 0 - padding 4 bytes */
	
	UNISTR2 uni_mach_acct; /* unicode string for machine account */

	uint8  padding_9[48];  /* 0 - padding 48 bytes */

} SAM_USER_INFO_11;


/* SAM_USER_INFO_10 */
typedef struct sam_user_info_10
{
	uint32 acb_info;

} SAM_USER_INFO_10;



/* SAMR_Q_CLOSE_HND - probably a policy handle close */
typedef struct q_samr_close_hnd_info
{
    POLICY_HND pol;          /* policy handle */

} SAMR_Q_CLOSE_HND;


/* SAMR_R_CLOSE_HND - probably a policy handle close */
typedef struct r_samr_close_hnd_info
{
    POLICY_HND pol;       /* policy handle */
	uint32 status;         /* return status */

} SAMR_R_CLOSE_HND;


/****************************************************************************
SAMR_Q_UNKNOWN_2C - a "set user info" occurs just after this
*****************************************************************************/

/* SAMR_Q_UNKNOWN_2C */
typedef struct q_samr_unknown_2c_info
{
	POLICY_HND user_pol;          /* policy handle */

} SAMR_Q_UNKNOWN_2C;


/****************************************************************************
SAMR_R_UNKNOWN_2C - a "set user info" occurs just after this
*****************************************************************************/

/* SAMR_R_UNKNOWN_2C */
typedef struct r_samr_unknown_2c_info
{
	uint32 unknown_0; /* 0x0016 0000 */
	uint32 unknown_1; /* 0x0000 0000 */
	uint32 status; 

} SAMR_R_UNKNOWN_2C;


/****************************************************************************
SAMR_Q_UNKNOWN_3 - info level 4.  returns SIDs.
*****************************************************************************/

/* SAMR_Q_UNKNOWN_3 - probably get domain info... */
typedef struct q_samr_unknown_3_info
{
	POLICY_HND user_pol;          /* policy handle */
	uint16 switch_value;     /* 0x0000 0004 */
	/* uint8 pad[2] */

} SAMR_Q_UNKNOWN_3;

/* DOM_SID3 example:
   0x14 0x035b 0x0002 S-1-1
   0x18 0x07ff 0x000f S-1-5-20-DOMAIN_ALIAS_RID_ADMINS
   0x18 0x07ff 0x000f S-1-5-20-DOMAIN_ALIAS_RID_ACCOUNT_OPS
   0x24 0x0044 0x0002 S-1-5-21-nnn-nnn-nnn-0x03f1
 */

/* DOM_SID3 example:
   0x24 0x0044 0x0002 S-1-5-21-nnn-nnn-nnn-0x03ee
   0x18 0x07ff 0x000f S-1-5-20-DOMAIN_ALIAS_RID_ADMINS
   0x14 0x035b 0x0002 S-1-1
 */

/* DOM_SID3 - security id */
typedef struct sid_info_3
{
	uint16 len; /* length, bytes, including length of len :-) */
	/* uint8  pad[2]; */
	
	DOM_SID sid;

} DOM_SID3;


#define MAX_SAM_SIDS 15

/* SAM_SID_STUFF */
typedef struct sid_stuff_info
{
	uint16 unknown_2; /* 0x0001 */
	uint16 unknown_3; /* 0x8004 */

	uint8 padding1[8];

	uint32 unknown_4; /* 0x0000 0014 */
	uint32 unknown_5; /* 0x0000 0014 */

	uint16 unknown_6; /* 0x0002 */
	uint16 unknown_7; /* 0x5800 */

	uint32 num_sids;

	uint16 padding2;

	DOM_SID3 sid[MAX_SAM_SIDS];

} SAM_SID_STUFF;

/* SAMR_R_UNKNOWN_3 - probably an open */
typedef struct r_samr_unknown_3_info
{
	uint32 ptr_0;
	uint32 sid_stuff_len0;

	uint32 ptr_1;
	uint32 sid_stuff_len1;

	SAM_SID_STUFF sid_stuff;

	uint32 status;         /* return status */

} SAMR_R_UNKNOWN_3;


/****************************************************************************
SAMR_Q_QUERY_DOMAIN_INFO - probably a query on domain group info.
*****************************************************************************/

/* SAMR_Q_QUERY_DOMAIN_INFO - */
typedef struct q_samr_query_domain_info
{
	POLICY_HND domain_pol;   /* policy handle */
	uint16 switch_value;     /* 0x0002 */

} SAMR_Q_QUERY_DOMAIN_INFO;

typedef struct sam_unkown_info_2_info
{
	uint32 unknown_0; /* 0x0000 0000 */
	uint32 unknown_1; /* 0x8000 0000 */
	uint32 unknown_2; /* 0x0000 0000 */

	uint32 ptr_0;     /* pointer to unknown structure */
	UNIHDR hdr_domain; /* domain name unicode header */
	UNIHDR hdr_server; /* server name unicode header */

	/* put all the data in here, at the moment, including what the above
	   pointer is referring to
	 */

	uint32 seq_num; /* some sort of incrementing sequence number? */
	uint32 unknown_3; /* 0x0000 0000 */
	
	uint32 unknown_4; /* 0x0000 0001 */
	uint32 unknown_5; /* 0x0000 0003 */
	uint32 unknown_6; /* 0x0000 0001 */
	uint32 num_domain_usrs; /* number of users in domain */
	uint32 num_domain_grps; /* number of domain groups in domain */
	uint32 num_local_grps; /* number of local groups in domain */

	uint8 padding[12]; /* 12 bytes zeros */

	UNISTR2 uni_domain; /* domain name unicode string */
	UNISTR2 uni_server; /* server name unicode string */

} SAM_UNK_INFO_2;


typedef struct sam_unknown_ctr_info
{
	union
	{
		SAM_UNK_INFO_2 inf2;

	} info;

} SAM_UNK_CTR;


/* SAMR_R_QUERY_DOMAIN_INFO - */
typedef struct r_samr_query_domain_info
{
	uint32 ptr_0;
	uint16 switch_value; /* same as in query */

	SAM_UNK_CTR *ctr;

	uint32 status;         /* return status */

} SAMR_R_QUERY_DOMAIN_INFO;


/****************************************************************************
SAMR_Q_OPEN_DOMAIN - unknown_0 values seen associated with SIDs:

0x0000 03f1 and a specific   domain sid - S-1-5-21-44c01ca6-797e5c3d-33f83fd0
0x0000 0200 and a specific   domain sid - S-1-5-21-44c01ca6-797e5c3d-33f83fd0
*****************************************************************************/

/* SAMR_Q_OPEN_DOMAIN */
typedef struct q_samr_open_domain_info
{
	POLICY_HND connect_pol;   /* policy handle */
	uint32 rid;               /* 0x2000 0000; 0x0000 0211; 0x0000 0280; 0x0000 0200 - a RID? */
	DOM_SID2 dom_sid;         /* domain SID */

} SAMR_Q_OPEN_DOMAIN;


/* SAMR_R_OPEN_DOMAIN - probably an open */
typedef struct r_samr_open_domain_info
{
	POLICY_HND domain_pol; /* policy handle associated with the SID */
	uint32 status;         /* return status */

} SAMR_R_OPEN_DOMAIN;


#define MAX_SAM_ENTRIES 250

typedef struct samr_entry_info
{
	uint32 rid;
	UNIHDR hdr_name;

} SAM_ENTRY;

/* SAMR_Q_ENUM_DOM_USERS - SAM rids and names */
typedef struct q_samr_enum_dom_users_info
{
	POLICY_HND pol;          /* policy handle */

	uint16 req_num_entries;   /* number of values (0 indicates unlimited?) */
	uint16 unknown_0;         /* enumeration context? */
	uint16 acb_mask;          /* 0x0000 indicates all */
	uint16 unknown_1;         /* 0x0000 */

	uint32 max_size;              /* 0x0000 ffff */

} SAMR_Q_ENUM_DOM_USERS;


/* SAMR_R_ENUM_DOM_USERS - SAM rids and names */
typedef struct r_samr_enum_dom_users_info
{
	uint16 total_num_entries;  /* number of entries that match without the acb mask */
	uint16 unknown_0;          /* same as unknown_0 (enum context?) in request */
	uint32 ptr_entries1;       /* actual number of entries to follow, having masked some out */

	uint32 num_entries2;
	uint32 ptr_entries2;

	uint32 num_entries3;

	SAM_ENTRY sam[MAX_SAM_ENTRIES];
	UNISTR2 uni_acct_name[MAX_SAM_ENTRIES];

	uint32 num_entries4;

	uint32 status;

} SAMR_R_ENUM_DOM_USERS;


typedef struct samr_entry_info3
{
	uint32 grp_idx;

	uint32 rid_grp;
	uint32 attr;

	UNIHDR hdr_grp_name;
	UNIHDR hdr_grp_desc;

} SAM_ENTRY3;

typedef struct samr_str_entry_info3
{
	UNISTR2 uni_grp_name;
	UNISTR2 uni_grp_desc;

} SAM_STR3;

/* SAMR_Q_ENUM_DOM_GROUPS - SAM rids and names */
typedef struct q_samr_enum_dom_groups_info
{
	POLICY_HND pol;          /* policy handle */

	/* these are possibly an enumeration context handle... */
	uint16 switch_level;      /* 0x0003 */
	uint16 unknown_0;         /* 0x0000 */
	uint32 start_idx;       /* presumably the start enumeration index */
	uint32 unknown_1;       /* 0x0000 07d0 */

	uint32 max_size;        /* 0x0000 7fff */

} SAMR_Q_ENUM_DOM_GROUPS;


/* SAMR_R_ENUM_DOM_GROUPS - SAM rids and names */
typedef struct r_samr_enum_dom_groups_info
{
	uint32 unknown_0;        /* 0x0000 0492 or 0x0000 00be */
	uint32 unknown_1;        /* 0x0000 049a or 0x0000 00be */
	uint32 switch_level;     /* 0x0000 0003 */

	uint32 num_entries;
	uint32 ptr_entries;

	uint32 num_entries2;

	SAM_ENTRY3 sam[MAX_SAM_ENTRIES];
	SAM_STR3   str[MAX_SAM_ENTRIES];

	uint32 status;

} SAMR_R_ENUM_DOM_GROUPS;



/* SAMR_Q_ENUM_DOM_ALIASES - SAM rids and names */
typedef struct q_samr_enum_dom_aliases_info
{
	POLICY_HND pol;          /* policy handle */

	/* this is possibly an enumeration context handle... */
	uint32 unknown_0;         /* 0x0000 0000 */

	uint32 max_size;              /* 0x0000 ffff */

} SAMR_Q_ENUM_DOM_ALIASES;

/* SAMR_R_ENUM_DOM_ALIASES - SAM rids and names */
typedef struct r_samr_enum_dom_aliases_info
{
	uint32 num_entries;
	uint32 ptr_entries;

	uint32 num_entries2;
	uint32 ptr_entries2;

	uint32 num_entries3;

	SAM_ENTRY sam[MAX_SAM_ENTRIES];
	UNISTR2 uni_grp_name[MAX_SAM_ENTRIES];

	uint32 num_entries4;

	uint32 status;

} SAMR_R_ENUM_DOM_ALIASES;



/* SAMR_Q_QUERY_DISPINFO - SAM rids, names and descriptions */
typedef struct q_samr_query_disp_info
{
	POLICY_HND pol;        /* policy handle */

	uint16 switch_level;    /* 0x0001 and 0x0002 seen */
	uint16 unknown_0;       /* 0x0000 and 0x2000 seen */
	uint32 start_idx;       /* presumably the start enumeration index */
	uint32 unknown_1;       /* 0x0000 07d0, 0x0000 0400 and 0x0000 0200 seen */

	uint32 max_size;        /* 0x0000 7fff, 0x0000 7ffe and 0x0000 3fff seen*/

} SAMR_Q_QUERY_DISPINFO;

typedef struct samr_entry_info1
{
	uint32 user_idx;

	uint32 rid_user;
	uint16 acb_info;
	uint16 pad;

	UNIHDR hdr_acct_name;
	UNIHDR hdr_user_name;
	UNIHDR hdr_user_desc;

} SAM_ENTRY1;

typedef struct samr_str_entry_info1
{
	UNISTR2 uni_acct_name;
	UNISTR2 uni_full_name;
	UNISTR2 uni_acct_desc;

} SAM_STR1;

typedef struct sam_entry_info_1
{
	uint32 num_entries;
	uint32 ptr_entries;
	uint32 num_entries2;

	SAM_ENTRY1 sam[MAX_SAM_ENTRIES];
	SAM_STR1   str[MAX_SAM_ENTRIES];


} SAM_INFO_1;

typedef struct samr_entry_info2
{
	uint32 user_idx;

	uint32 rid_user;
	uint16 acb_info;
	uint16 pad;

	UNIHDR hdr_srv_name;
	UNIHDR hdr_srv_desc;

} SAM_ENTRY2;

typedef struct samr_str_entry_info2
{
	UNISTR2 uni_srv_name;
	UNISTR2 uni_srv_desc;

} SAM_STR2;

typedef struct sam_entry_info_2
{
	uint32 num_entries;
	uint32 ptr_entries;
	uint32 num_entries2;

	SAM_ENTRY2 sam[MAX_SAM_ENTRIES];
	SAM_STR2   str[MAX_SAM_ENTRIES];

} SAM_INFO_2;

typedef struct sam_info_ctr_info
{
	union
	{
		SAM_INFO_1 *info1; /* server info */
		SAM_INFO_2 *info2; /* user info */
		void       *info; /* allows assignment without typecasting, */

	} sam;

} SAM_INFO_CTR;

/* SAMR_R_QUERY_DISPINFO - SAM rids, names and descriptions */
typedef struct r_samr_query_dispinfo_info
{
	uint32 unknown_0;        /* container length? 0x0000 0492 or 0x0000 00be */
	uint32 unknown_1;        /* container length? 0x0000 049a or 0x0000 00be */
	uint16 switch_level;     /* 0x0001 or 0x0002 */
	/*uint8 pad[2] */

	SAM_INFO_CTR *ctr;

	uint32 status;

} SAMR_R_QUERY_DISPINFO;



/* SAMR_Q_QUERY_ALIASINFO - SAM Alias Info */
typedef struct q_samr_enum_alias_info
{
	POLICY_HND pol;        /* policy handle */

	uint16 switch_level;    /* 0x0003 seen */

} SAMR_Q_QUERY_ALIASINFO;

typedef struct samr_alias_info3
{
	UNIHDR hdr_acct_desc;
	UNISTR2 uni_acct_desc;

} ALIAS_INFO3;

/* SAMR_R_QUERY_ALIASINFO - SAM rids, names and descriptions */
typedef struct r_samr_query_aliasinfo_info
{
	uint32 ptr;        
	uint16 switch_value;     /* 0x0003 */
	/* uint8[2] padding */

	union
 	{
		ALIAS_INFO3 info3;

	} alias;

	uint32 status;

} SAMR_R_QUERY_ALIASINFO;


/* SAMR_Q_QUERY_USERGROUPS - */
typedef struct q_samr_query_usergroup_info
{
	POLICY_HND pol;          /* policy handle associated with unknown id */

} SAMR_Q_QUERY_USERGROUPS;

/* SAMR_R_QUERY_USERGROUPS - probably a get sam info */
typedef struct r_samr_query_usergroup_info
{
	uint32 ptr_0;            /* pointer */
	uint32 num_entries;      /* number of RID groups */
	uint32 ptr_1;            /* pointer */
	uint32 num_entries2;     /* number of RID groups */

	DOM_GID *gid; /* group info */

	uint32 status;         /* return status */

} SAMR_R_QUERY_USERGROUPS;


/* SAMR_Q_QUERY_USERINFO - probably a get sam info */
typedef struct q_samr_query_user_info
{
	POLICY_HND pol;          /* policy handle associated with unknown id */
	uint16 switch_value;         /* 0x0015, 0x0011 or 0x0010 - 16 bit unknown */

} SAMR_Q_QUERY_USERINFO;

/* SAMR_R_QUERY_USERINFO - probably a get sam info */
typedef struct r_samr_query_user_info
{
	uint32 ptr;            /* pointer */
	uint16 switch_value;      /* 0x0015, 0x0011 or 0x0010 - same as in query */
	/* uint8[2] padding. */

	union
	{
		SAM_USER_INFO_10 *id10; /* auth-level 0x10 */
		SAM_USER_INFO_11 *id11; /* auth-level 0x11 */
		SAM_USER_INFO_21 *id21; /* auth-level 21 */
		void* id; /* to make typecasting easy */

	} info;

	uint32 status;         /* return status */

} SAMR_R_QUERY_USERINFO;


/****************************************************************************
SAMR_Q_LOOKUP_IDS - do a conversion from name to RID.

the policy handle allocated by an "samr open secret" call is associated
with a SID.  this policy handle is what is queried here, *not* the SID
itself.  the response to the lookup rids is relative to this SID.
*****************************************************************************/
/* SAMR_Q_LOOKUP_IDS */
typedef struct q_samr_lookup_ids_info
{
    POLICY_HND pol;       /* policy handle */

	uint32 num_sids1;      /* number of rids being looked up */
	uint32 ptr;            /* buffer pointer */
	uint32 num_sids2;      /* number of rids being looked up */

	uint32   ptr_sid[MAX_LOOKUP_SIDS]; /* pointers to sids to be looked up */
	DOM_SID2 sid    [MAX_LOOKUP_SIDS]; /* sids to be looked up. */

} SAMR_Q_LOOKUP_IDS;


/* SAMR_R_LOOKUP_IDS */
typedef struct r_samr_lookup_ids_info
{
	uint32 num_entries;
	uint32 ptr; /* undocumented buffer pointer */

	uint32 num_entries2; 
	uint32 rid[MAX_LOOKUP_SIDS]; /* domain RIDs being looked up */

	uint32 status; /* return code */

} SAMR_R_LOOKUP_IDS;

/****************************************************************************
SAMR_Q_LOOKUP_NAMES - do a conversion from Names to RIDs+types.
*****************************************************************************/
/* SAMR_Q_LOOKUP_NAMES */
typedef struct q_samr_lookup_names_info
{
	POLICY_HND pol;       /* policy handle */

	uint32 num_names1;      /* number of names being looked up */
	uint32 flags;           /* 0x0000 03e8 - unknown */
	uint32 ptr;            /* 0x0000 0000 - 32 bit unknown */
	uint32 num_names2;      /* number of names being looked up */

	UNIHDR  hdr_name[MAX_LOOKUP_SIDS]; /* unicode account name header */
	UNISTR2 uni_name[MAX_LOOKUP_SIDS]; /* unicode account name string */

} SAMR_Q_LOOKUP_NAMES;

/* SAMR_R_LOOKUP_NAMES */
typedef struct r_samr_lookup_names_info
{
	uint32 num_rids1;      /* number of aliases being looked up */
	uint32 ptr_rids;       /* pointer to aliases */
	uint32 num_rids2;      /* number of aliases being looked up */

	uint32 rid[MAX_LOOKUP_SIDS]; /* rids */

	uint32 num_types1;      /* number of users in aliases being looked up */
	uint32 ptr_types;       /* pointer to users in aliases */
	uint32 num_types2;      /* number of users in aliases being looked up */

	uint32 type[MAX_LOOKUP_SIDS]; /* SID_ENUM type */

	uint32 status; /* return code */

} SAMR_R_LOOKUP_NAMES;

/****************************************************************************
SAMR_Q_UNKNOWN_12 - do a conversion from RID groups to something.

called to resolve domain RID groups.
*****************************************************************************/
/* SAMR_Q_UNKNOWN_12 */
typedef struct q_samr_unknown_12_info
{
    POLICY_HND pol;       /* policy handle */

	uint32 num_gids1;      /* number of rids being looked up */
	uint32 rid;            /* 0x0000 03e8 - RID of the server doing the query? */
	uint32 ptr;            /* 0x0000 0000 - 32 bit unknown */
	uint32 num_gids2;      /* number of rids being looked up */

	uint32 gid[MAX_LOOKUP_SIDS]; /* domain RIDs being looked up */

} SAMR_Q_UNKNOWN_12;


/****************************************************************************
SAMR_R_UNKNOWN_12 - do a conversion from group RID to names

*****************************************************************************/
/* SAMR_R_UNKNOWN_12 */
typedef struct r_samr_unknown_12_info
{
    POLICY_HND pol;       /* policy handle */

	uint32 num_aliases1;      /* number of aliases being looked up */
	uint32 ptr_aliases;       /* pointer to aliases */
	uint32 num_aliases2;      /* number of aliases being looked up */

	UNIHDR  hdr_als_name[MAX_LOOKUP_SIDS]; /* unicode account name header */
	UNISTR2 uni_als_name[MAX_LOOKUP_SIDS]; /* unicode account name string */

	uint32 num_als_usrs1;      /* number of users in aliases being looked up */
	uint32 ptr_als_usrs;       /* pointer to users in aliases */
	uint32 num_als_usrs2;      /* number of users in aliases being looked up */

	uint32 num_als_usrs[MAX_LOOKUP_SIDS]; /* number of users per group */

	uint32 status;

} SAMR_R_UNKNOWN_12;


/* SAMR_Q_OPEN_USER - probably an open */
typedef struct q_samr_open_user_info
{
    POLICY_HND domain_pol;       /* policy handle */
	uint32 unknown_0;     /* 32 bit unknown - 0x02011b */
	uint32 user_rid;      /* user RID */

} SAMR_Q_OPEN_USER;


/* SAMR_R_OPEN_USER - probably an open */
typedef struct r_samr_open_user_info
{
    POLICY_HND user_pol;       /* policy handle associated with unknown id */
	uint32 status;         /* return status */

} SAMR_R_OPEN_USER;


/* SAMR_Q_UNKNOWN_13 - probably an open alias in domain */
typedef struct q_samr_unknown_13_info
{
    POLICY_HND alias_pol;        /* policy handle */

	uint16 unknown_1;            /* 16 bit unknown - 0x0200 */
	uint16 unknown_2;            /* 16 bit unknown - 0x0000 */

} SAMR_Q_UNKNOWN_13;


/* SAMR_Q_UNKNOWN_21 - probably an open group in domain */
typedef struct q_samr_unknown_21_info
{
    POLICY_HND group_pol;        /* policy handle */

	uint16 unknown_1;            /* 16 bit unknown - 0x0477 */
	uint16 unknown_2;            /* 16 bit unknown - 0x0000 */

} SAMR_Q_UNKNOWN_21;


/* SAMR_Q_UNKNOWN_32 - probably a "create SAM entry" */
typedef struct q_samr_unknown_32_info
{
    POLICY_HND pol;             /* policy handle */

	UNIHDR  hdr_mach_acct;       /* unicode machine account name header */
	UNISTR2 uni_mach_acct;       /* unicode machine account name */

	uint32 acct_ctrl;            /* 32 bit ACB_XXXX */
	uint16 unknown_1;            /* 16 bit unknown - 0x00B0 */
	uint16 unknown_2;            /* 16 bit unknown - 0xe005 */

} SAMR_Q_UNKNOWN_32;


/* SAMR_R_UNKNOWN_32 - probably a "create SAM entry" */
typedef struct r_samr_unknown_32_info
{
    POLICY_HND pol;       /* policy handle */

	/* rid4.unknown - fail: 0030 success: 0x03ff */
	DOM_RID4 rid4;         /* rid and attributes */

	uint32 status;         /* return status - fail: 0xC000 0099: user exists */

} SAMR_R_UNKNOWN_32;

/* SAMR_Q_OPEN_ALIAS - probably an open */
typedef struct q_samr_open_alias_info
{
	uint32 unknown_0;         /* 0x0000 0008 */
	uint32 rid_alias;        /* rid */

} SAMR_Q_OPEN_ALIAS;


/* SAMR_R_OPEN_ALIAS - probably an open */
typedef struct r_samr_open_alias_info
{
	POLICY_HND pol;       /* policy handle */
	uint32 status;         /* return status */

} SAMR_R_OPEN_ALIAS;


/* SAMR_Q_CONNECT_ANON - probably an open */
typedef struct q_samr_connect_anon_info
{
	uint32 ptr;                  /* ptr? */
	uint16 unknown_0;            /* 0x005c */
	uint16 unknown_1;            /* 0x0001 */
	uint32 unknown_2;            /* 0x0000 0020 */

} SAMR_Q_CONNECT_ANON;

/* SAMR_R_CONNECT_ANON - probably an open */
typedef struct r_samr_connect_anon_info
{
	POLICY_HND connect_pol;       /* policy handle */
	uint32 status;         /* return status */

} SAMR_R_CONNECT_ANON;

/* SAMR_Q_CONNECT - probably an open */
typedef struct q_samr_connect_info
{
	uint32 ptr_srv_name;         /* pointer (to server name?) */
	UNISTR2 uni_srv_name;        /* unicode server name starting with '\\' */

	uint32 unknown_0;            /* 32 bit unknown */

} SAMR_Q_CONNECT;


/* SAMR_R_CONNECT - probably an open */
typedef struct r_samr_connect_info
{
    POLICY_HND connect_pol;       /* policy handle */
	uint32 status;         /* return status */

} SAMR_R_CONNECT;

/* SAMR_Q_UNKNOWN_38 */
typedef struct q_samr_unknown_38
{
	uint32 ptr; 
	UNIHDR  hdr_srv_name;
	UNISTR2 uni_srv_name;

} SAMR_Q_UNKNOWN_38;

/* SAMR_R_UNKNOWN_38 */
typedef struct r_samr_unknown_38
{
	uint16 unk_0;
	uint16 unk_1;
	uint16 unk_2;
	uint16 unk_3;
	uint32 status;         /* return status */

} SAMR_R_UNKNOWN_38;

/* SAMR_ENC_PASSWD */
typedef struct enc_passwd_info
{
	uint32 ptr;
	uint8 pass[516];

} SAMR_ENC_PASSWD;

/* SAMR_ENC_HASH */
typedef struct enc_hash_info
{
	uint32 ptr;
	uint8 hash[16];

} SAMR_ENC_HASH;

/* SAMR_Q_CHGPASSWD_USER */
typedef struct q_samr_chgpasswd_user_info
{
	uint32 ptr_0;

	UNIHDR hdr_dest_host; /* server name unicode header */
	UNISTR2 uni_dest_host; /* server name unicode string */

	UNIHDR hdr_user_name;    /* username unicode string header */
	UNISTR2 uni_user_name;    /* username unicode string */

	SAMR_ENC_PASSWD nt_newpass;
	SAMR_ENC_HASH nt_oldhash;

	uint32 unknown; /* 0x0000 0001 */

	SAMR_ENC_PASSWD lm_newpass;
	SAMR_ENC_HASH lm_oldhash;

} SAMR_Q_CHGPASSWD_USER;

/* SAMR_R_CHGPASSWD_USER */
typedef struct r_samr_chgpasswd_user_info
{
	uint32 status; /* 0 == OK, C000006A (NT_STATUS_WRONG_PASSWORD) */

} SAMR_R_CHGPASSWD_USER;

#endif /* _RPC_SAMR_H */

