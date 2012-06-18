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

#ifndef _RPC_NETLOGON_H /* _RPC_NETLOGON_H */
#define _RPC_NETLOGON_H 


/* NETLOGON pipe */
#define NET_REQCHAL            0x04
#define NET_SRVPWSET           0x06
#define NET_SAMLOGON           0x02
#define NET_SAMLOGOFF          0x03
#define NET_AUTH2              0x0f
#define NET_LOGON_CTRL2        0x0e
#define NET_TRUST_DOM_LIST     0x13

/* Secure Channel types.  used in NetrServerAuthenticate negotiation */
#define SEC_CHAN_WKSTA   2
#define SEC_CHAN_DOMAIN  4

#if 0
/* JRATEST.... */
/* NET_USER_INFO_2 */
typedef struct net_user_info_2
{
        uint32 ptr_user_info;

        NTTIME logon_time;            /* logon time */
        NTTIME logoff_time;           /* logoff time */
        NTTIME kickoff_time;          /* kickoff time */
        NTTIME pass_last_set_time;    /* password last set time */
        NTTIME pass_can_change_time;  /* password can change time */
        NTTIME pass_must_change_time; /* password must change time */

....
	uint32 user_id;       /* User ID */
	uint32 group_id;      /* Group ID */
....
	uint32 num_groups2;        /* num groups */
	DOM_GID gids[LSA_MAX_GROUPS]; /* group info */

	UNIHDR hdr_logon_srv; /* logon server unicode string header */
	UNISTR2 uni_logon_dom; /* logon domain unicode string */
	DOM_SID2 dom_sid;
	
} NET_USER_INFO_2;
/* ! JRATEST.... */
#endif

/* NET_USER_INFO_3 */
typedef struct net_user_info_3
{
	uint32 ptr_user_info;

	NTTIME logon_time;            /* logon time */
	NTTIME logoff_time;           /* logoff time */
	NTTIME kickoff_time;          /* kickoff time */
	NTTIME pass_last_set_time;    /* password last set time */
	NTTIME pass_can_change_time;  /* password can change time */
	NTTIME pass_must_change_time; /* password must change time */

	UNIHDR hdr_user_name;    /* username unicode string header */
	UNIHDR hdr_full_name;    /* user's full name unicode string header */
	UNIHDR hdr_logon_script; /* logon script unicode string header */
	UNIHDR hdr_profile_path; /* profile path unicode string header */
	UNIHDR hdr_home_dir;     /* home directory unicode string header */
	UNIHDR hdr_dir_drive;    /* home directory drive unicode string header */

	uint16 logon_count;  /* logon count */
	uint16 bad_pw_count; /* bad password count */

	uint32 user_id;       /* User ID */
	uint32 group_id;      /* Group ID */
	uint32 num_groups;    /* num groups */
	uint32 buffer_groups; /* undocumented buffer pointer to groups. */
	uint32 user_flgs;     /* user flags */

	uint8 user_sess_key[16]; /* unused user session key */

	UNIHDR hdr_logon_srv; /* logon server unicode string header */
	UNIHDR hdr_logon_dom; /* logon domain unicode string header */

	uint32 buffer_dom_id; /* undocumented logon domain id pointer */
	uint8 padding[40];    /* unused padding bytes.  expansion room */

	uint32 num_other_sids; /* 0 - num_sids */
	uint32 buffer_other_sids; /* NULL - undocumented pointer to SIDs. */
	
	UNISTR2 uni_user_name;    /* username unicode string */
	UNISTR2 uni_full_name;    /* user's full name unicode string */
	UNISTR2 uni_logon_script; /* logon script unicode string */
	UNISTR2 uni_profile_path; /* profile path unicode string */
	UNISTR2 uni_home_dir;     /* home directory unicode string */
	UNISTR2 uni_dir_drive;    /* home directory drive unicode string */

	uint32 num_groups2;        /* num groups */
	DOM_GID gids[LSA_MAX_GROUPS]; /* group info */

	UNISTR2 uni_logon_srv; /* logon server unicode string */
	UNISTR2 uni_logon_dom; /* logon domain unicode string */

	DOM_SID2 dom_sid;           /* domain SID */
	DOM_SID2 other_sids[LSA_MAX_SIDS]; /* undocumented - domain SIDs */

} NET_USER_INFO_3;


/********************************************************
 Logon Control Query

 query_level 0x1 - pdc status
 query_level 0x3 - number of logon attempts.

 ********************************************************/
/* NET_Q_LOGON_CTRL2 - LSA Netr Logon Control 2*/
typedef struct net_q_logon_ctrl2_info
{
	uint32       ptr;             /* undocumented buffer pointer */
	UNISTR2      uni_server_name; /* server name, starting with two '\'s */
	
	uint32       function_code; /* 0x1 */
	uint32       query_level;   /* 0x1, 0x3 */
	uint32       switch_value;  /* 0x1 */

} NET_Q_LOGON_CTRL2;

/* NETLOGON_INFO_1 - pdc status info, i presume */
typedef struct netlogon_1_info
{
	uint32 flags;            /* 0x0 - undocumented */
	uint32 pdc_status;       /* 0x0 - undocumented */

} NETLOGON_INFO_1;

/* NETLOGON_INFO_2 - pdc status info, plus trusted domain info */
typedef struct netlogon_2_info
{
	uint32  flags;            /* 0x0 - undocumented */
	uint32  pdc_status;       /* 0x0 - undocumented */
	uint32  ptr_trusted_dc_name; /* pointer to trusted domain controller name */
	uint32  tc_status;           /* 0x051f - ERROR_NO_LOGON_SERVERS */
	UNISTR2 uni_trusted_dc_name; /* unicode string - trusted dc name */

} NETLOGON_INFO_2;

/* NETLOGON_INFO_3 - logon status info, i presume */
typedef struct netlogon_3_info
{
	uint32 flags;            /* 0x0 - undocumented */
	uint32 logon_attempts;   /* number of logon attempts */
	uint32 reserved_1;       /* 0x0 - undocumented */
	uint32 reserved_2;       /* 0x0 - undocumented */
	uint32 reserved_3;       /* 0x0 - undocumented */
	uint32 reserved_4;       /* 0x0 - undocumented */
	uint32 reserved_5;       /* 0x0 - undocumented */

} NETLOGON_INFO_3;

/*******************************************************
 Logon Control Response

 switch_value is same as query_level in request 
 *******************************************************/

/* NET_R_LOGON_CTRL2 - response to LSA Logon Control2 */
typedef struct net_r_logon_ctrl2_info
{
	uint32       switch_value;  /* 0x1, 0x3 */
	uint32       ptr;

	union
	{
		NETLOGON_INFO_1 info1;
		NETLOGON_INFO_2 info2;
		NETLOGON_INFO_3 info3;

	} logon;

	uint32 status; /* return code */

} NET_R_LOGON_CTRL2;

/* NET_Q_TRUST_DOM_LIST - LSA Query Trusted Domains */
typedef struct net_q_trust_dom_info
{
	uint32       ptr;             /* undocumented buffer pointer */
	UNISTR2      uni_server_name; /* server name, starting with two '\'s */
	
	uint32       function_code; /* 0x31 */

} NET_Q_TRUST_DOM_LIST;

#define MAX_TRUST_DOMS 1

/* NET_R_TRUST_DOM_LIST - response to LSA Trusted Domains */
typedef struct net_r_trust_dom_info
{
	UNISTR2 uni_trust_dom_name[MAX_TRUST_DOMS];

	uint32 status; /* return code */

} NET_R_TRUST_DOM_LIST;


/* NEG_FLAGS */
typedef struct neg_flags_info
{
    uint32 neg_flags; /* negotiated flags */

} NEG_FLAGS;


/* NET_Q_REQ_CHAL */
typedef struct net_q_req_chal_info
{
    uint32  undoc_buffer; /* undocumented buffer pointer */
    UNISTR2 uni_logon_srv; /* logon server unicode string */
    UNISTR2 uni_logon_clnt; /* logon client unicode string */
    DOM_CHAL clnt_chal; /* client challenge */

} NET_Q_REQ_CHAL;


/* NET_R_REQ_CHAL */
typedef struct net_r_req_chal_info
{
    DOM_CHAL srv_chal; /* server challenge */

  uint32 status; /* return code */

} NET_R_REQ_CHAL;



/* NET_Q_AUTH_2 */
typedef struct net_q_auth2_info
{
    DOM_LOG_INFO clnt_id; /* client identification info */
    DOM_CHAL clnt_chal;     /* client-calculated credentials */

    NEG_FLAGS clnt_flgs; /* usually 0x0000 01ff */

} NET_Q_AUTH_2;


/* NET_R_AUTH_2 */
typedef struct net_r_auth2_info
{
    DOM_CHAL srv_chal;     /* server-calculated credentials */
    NEG_FLAGS srv_flgs; /* usually 0x0000 01ff */

  uint32 status; /* return code */

} NET_R_AUTH_2;


/* NET_Q_SRV_PWSET */
typedef struct net_q_srv_pwset_info
{
    DOM_CLNT_INFO clnt_id; /* client identification/authentication info */
    uint8 pwd[16]; /* new password - undocumented. */

} NET_Q_SRV_PWSET;
    
/* NET_R_SRV_PWSET */
typedef struct net_r_srv_pwset_info
{
    DOM_CRED srv_cred;     /* server-calculated credentials */

  uint32 status; /* return code */

} NET_R_SRV_PWSET;

/* NET_ID_INFO_2 */
typedef struct net_network_info_2
{
	uint32            ptr_id_info2;        /* pointer to id_info_2 */
	UNIHDR            hdr_domain_name;     /* domain name unicode header */
	uint32            param_ctrl;          /* param control (0x2) */
	DOM_LOGON_ID      logon_id;            /* logon ID */
	UNIHDR            hdr_user_name;       /* user name unicode header */
	UNIHDR            hdr_wksta_name;      /* workstation name unicode header */
	uint8             lm_chal[8];          /* lan manager 8 byte challenge */
	STRHDR            hdr_nt_chal_resp;    /* nt challenge response */
	STRHDR            hdr_lm_chal_resp;    /* lm challenge response */

	UNISTR2           uni_domain_name;     /* domain name unicode string */
	UNISTR2           uni_user_name;       /* user name unicode string */
	UNISTR2           uni_wksta_name;      /* workgroup name unicode string */
	STRING2           nt_chal_resp;        /* nt challenge response */
	STRING2           lm_chal_resp;        /* lm challenge response */

} NET_ID_INFO_2;

/* NET_ID_INFO_1 */
typedef struct id_info_1
{
	uint32            ptr_id_info1;        /* pointer to id_info_1 */
	UNIHDR            hdr_domain_name;     /* domain name unicode header */
	uint32            param_ctrl;          /* param control */
	DOM_LOGON_ID      logon_id;            /* logon ID */
	UNIHDR            hdr_user_name;       /* user name unicode header */
	UNIHDR            hdr_wksta_name;      /* workstation name unicode header */
	OWF_INFO          lm_owf;              /* LM OWF Password */
	OWF_INFO          nt_owf;              /* NT OWF Password */
	UNISTR2           uni_domain_name;     /* domain name unicode string */
	UNISTR2           uni_user_name;       /* user name unicode string */
	UNISTR2           uni_wksta_name;      /* workgroup name unicode string */

} NET_ID_INFO_1;

#define INTERACTIVE_LOGON_TYPE 1
#define NET_LOGON_TYPE 2

/* NET_ID_INFO_CTR */
typedef struct net_id_info_ctr_info
{
  uint16         switch_value;
  
  union
  {
    NET_ID_INFO_1 id1; /* auth-level 1 - interactive user login */
    NET_ID_INFO_2 id2; /* auth-level 2 - workstation referred login */

  } auth;
  
} NET_ID_INFO_CTR;

/* SAM_INFO - sam logon/off id structure */
typedef struct sam_info
{
  DOM_CLNT_INFO2  client;
  uint32          ptr_rtn_cred; /* pointer to return credentials */
  DOM_CRED        rtn_cred; /* return credentials */
  uint16          logon_level;
  NET_ID_INFO_CTR *ctr;

} DOM_SAM_INFO;

/* NET_Q_SAM_LOGON */
typedef struct net_q_sam_logon_info
{
    DOM_SAM_INFO sam_id;
	uint16          validation_level;

} NET_Q_SAM_LOGON;

/* NET_R_SAM_LOGON */
typedef struct net_r_sam_logon_info
{
    uint32 buffer_creds; /* undocumented buffer pointer */
    DOM_CRED srv_creds; /* server credentials.  server time stamp appears to be ignored. */
    
	uint16 switch_value; /* 3 - indicates type of USER INFO */
    NET_USER_INFO_3 *user;

    uint32 auth_resp; /* 1 - Authoritative response; 0 - Non-Auth? */

  uint32 status; /* return code */

} NET_R_SAM_LOGON;


/* NET_Q_SAM_LOGOFF */
typedef struct net_q_sam_logoff_info
{
    DOM_SAM_INFO sam_id;

} NET_Q_SAM_LOGOFF;

/* NET_R_SAM_LOGOFF */
typedef struct net_r_sam_logoff_info
{
    uint32 buffer_creds; /* undocumented buffer pointer */
    DOM_CRED srv_creds; /* server credentials.  server time stamp appears to be ignored. */
    
  uint32 status; /* return code */

} NET_R_SAM_LOGOFF;


#endif /* _RPC_NETLOGON_H */

