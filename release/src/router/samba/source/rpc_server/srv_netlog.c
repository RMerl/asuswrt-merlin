
/* 
 *  Unix SMB/Netbios implementation.
 *  Version 1.9.
 *  RPC Pipe client / server routines
 *  Copyright (C) Andrew Tridgell              1992-1997,
 *  Copyright (C) Luke Kenneth Casson Leighton 1996-1997,
 *  Copyright (C) Paul Ashton                       1997.
 *  Copyright (C) Jeremy Allison                    1998.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#include "includes.h"
#include "nterr.h"

extern int DEBUGLEVEL;

extern BOOL sam_logon_in_ssb;
extern pstring samlogon_user;
extern pstring global_myname;
extern DOM_SID global_sam_sid;

/*************************************************************************
 init_net_r_req_chal:
 *************************************************************************/

static void init_net_r_req_chal(NET_R_REQ_CHAL *r_c,
                                DOM_CHAL *srv_chal, int status)
{
	DEBUG(6,("init_net_r_req_chal: %d\n", __LINE__));
	memcpy(r_c->srv_chal.data, srv_chal->data, sizeof(srv_chal->data));
	r_c->status = status;
}

/*************************************************************************
 net_reply_req_chal:
 *************************************************************************/

static BOOL net_reply_req_chal(NET_Q_REQ_CHAL *q_c, prs_struct *rdata,
					DOM_CHAL *srv_chal, uint32 srv_time)
{
	NET_R_REQ_CHAL r_c;

	DEBUG(6,("net_reply_req_chal: %d\n", __LINE__));

	/* set up the LSA REQUEST CHALLENGE response */
	init_net_r_req_chal(&r_c, srv_chal, srv_time);

	/* store the response in the SMB stream */
	if(!net_io_r_req_chal("", &r_c, rdata, 0)) {
		DEBUG(0,("net_reply_req_chal: Failed to marshall NET_R_REQ_CHAL.\n"));
		return False;
	}

	DEBUG(6,("net_reply_req_chal: %d\n", __LINE__));

	return True;
}

/*************************************************************************
 net_reply_logon_ctrl2:
 *************************************************************************/

static BOOL net_reply_logon_ctrl2(NET_Q_LOGON_CTRL2 *q_l, prs_struct *rdata,
			uint32 flags, uint32 pdc_status, uint32 logon_attempts,
			uint32 tc_status, char *trust_domain_name)
{
	NET_R_LOGON_CTRL2 r_l;

	DEBUG(6,("net_reply_logon_ctrl2: %d\n", __LINE__));

	/* set up the Logon Control2 response */
	init_r_logon_ctrl2(&r_l, q_l->query_level,
	                   flags, pdc_status, logon_attempts,
	                   tc_status, trust_domain_name);

	/* store the response in the SMB stream */
	if(!net_io_r_logon_ctrl2("", &r_l, rdata, 0)) {
		DEBUG(0,("net_reply_logon_ctrl2: Failed to marshall NET_R_LOGON_CTRL2.\n"));
		return False;
	}

	DEBUG(6,("net_reply_logon_ctrl2: %d\n", __LINE__));

	return True;
}

/*************************************************************************
 net_reply_trust_dom_list:
 *************************************************************************/

static BOOL net_reply_trust_dom_list(NET_Q_TRUST_DOM_LIST *q_t, prs_struct *rdata,
			uint32 num_trust_domains, char *trust_domain_name)
{
	NET_R_TRUST_DOM_LIST r_t;

	DEBUG(6,("net_reply_trust_dom_list: %d\n", __LINE__));

	/* set up the Trusted Domain List response */
	init_r_trust_dom(&r_t, num_trust_domains, trust_domain_name);

	/* store the response in the SMB stream */
	if(!net_io_r_trust_dom("", &r_t, rdata, 0)) {
		DEBUG(0,("net_reply_trust_dom_list: Failed to marshall NET_R_TRUST_DOM_LIST.\n"));
		return False;
	}

	DEBUG(6,("net_reply_trust_dom_listlogon_ctrl2: %d\n", __LINE__));

	return True;
}

/*************************************************************************
 init_net_r_auth_2:
 *************************************************************************/

static void init_net_r_auth_2(NET_R_AUTH_2 *r_a,
                              DOM_CHAL *resp_cred, NEG_FLAGS *flgs, int status)
{
	memcpy(r_a->srv_chal.data, resp_cred->data, sizeof(resp_cred->data));
	memcpy(&r_a->srv_flgs, flgs, sizeof(r_a->srv_flgs));
	r_a->status = status;
}

/************************************************************************
 net_reply_auth_2:
 *************************************************************************/

static BOOL net_reply_auth_2(NET_Q_AUTH_2 *q_a, prs_struct *rdata,
				DOM_CHAL *resp_cred, int status)
{
	NET_R_AUTH_2 r_a;
	NEG_FLAGS srv_flgs;

	srv_flgs.neg_flags = 0x000001ff;

	/* set up the LSA AUTH 2 response */

	init_net_r_auth_2(&r_a, resp_cred, &srv_flgs, status);

	/* store the response in the SMB stream */
	if(!net_io_r_auth_2("", &r_a, rdata, 0)) {
		DEBUG(0,("net_reply_auth_2: Failed to marshall NET_R_AUTH_2.\n"));
		return False;
	}

	return True;
}

/***********************************************************************************
 init_net_r_srv_pwset:
 ***********************************************************************************/

static void init_net_r_srv_pwset(NET_R_SRV_PWSET *r_s,
                             DOM_CRED *srv_cred, int status)  
{
	DEBUG(5,("init_net_r_srv_pwset: %d\n", __LINE__));

	memcpy(&r_s->srv_cred, srv_cred, sizeof(r_s->srv_cred));
	r_s->status = status;

	DEBUG(5,("init_net_r_srv_pwset: %d\n", __LINE__));
}

/*************************************************************************
 net_reply_srv_pwset:
 *************************************************************************/

static BOOL net_reply_srv_pwset(NET_Q_SRV_PWSET *q_s, prs_struct *rdata,
				DOM_CRED *srv_cred, int status)
{
	NET_R_SRV_PWSET r_s;

	DEBUG(5,("net_srv_pwset: %d\n", __LINE__));

	/* set up the LSA Server Password Set response */
	init_net_r_srv_pwset(&r_s, srv_cred, status);

	/* store the response in the SMB stream */
	if(!net_io_r_srv_pwset("", &r_s, rdata, 0)) {
		DEBUG(0,("net_reply_srv_pwset: Failed to marshall NET_R_SRV_PWSET.\n"));
		return False;
	}

	DEBUG(5,("net_srv_pwset: %d\n", __LINE__));

	return True;
}

/*************************************************************************
 net_reply_sam_logon:
 *************************************************************************/

static BOOL net_reply_sam_logon(NET_Q_SAM_LOGON *q_s, prs_struct *rdata,
				DOM_CRED *srv_cred, NET_USER_INFO_3 *user_info,
				uint32 status)
{
	NET_R_SAM_LOGON r_s;

	/* XXXX maybe we want to say 'no', reject the client's credentials */
	r_s.buffer_creds = 1; /* yes, we have valid server credentials */
	memcpy(&r_s.srv_creds, srv_cred, sizeof(r_s.srv_creds));

	/* store the user information, if there is any. */
	r_s.user = user_info;
	if (status == 0x0 && user_info != NULL && user_info->ptr_user_info != 0)
		r_s.switch_value = 3; /* indicates type of validation user info */
	else
		r_s.switch_value = 0; /* indicates no info */

	r_s.status = status;
	r_s.auth_resp = 1; /* authoritative response */

	/* store the response in the SMB stream */
	if(!net_io_r_sam_logon("", &r_s, rdata, 0)) {
		DEBUG(0,("net_reply_sam_logon: Failed to marshall NET_R_SAM_LOGON.\n"));
		return False;
	}

	return True;
}


/*************************************************************************
 net_reply_sam_logoff:
 *************************************************************************/

static BOOL net_reply_sam_logoff(NET_Q_SAM_LOGOFF *q_s, prs_struct *rdata,
				DOM_CRED *srv_cred, 
				uint32 status)
{
	NET_R_SAM_LOGOFF r_s;

	/* XXXX maybe we want to say 'no', reject the client's credentials */
	r_s.buffer_creds = 1; /* yes, we have valid server credentials */
	memcpy(&r_s.srv_creds, srv_cred, sizeof(r_s.srv_creds));

	r_s.status = status;

	/* store the response in the SMB stream */
	if(!net_io_r_sam_logoff("", &r_s, rdata, 0)) {
		DEBUG(0,("net_reply_sam_logoff: Failed to marshall NET_R_SAM_LOGOFF.\n"));
		return False;
	}

	return True;
}

/******************************************************************
 gets a machine password entry.  checks access rights of the host.
 ******************************************************************/

static BOOL get_md4pw(char *md4pw, char *mach_name, char *mach_acct)
{
	struct smb_passwd *smb_pass;

#if 0
    /*
     * Currently this code is redundent as we already have a filter
     * by hostname list. What this code really needs to do is to 
     * get a hosts allowed/hosts denied list from the SAM database
     * on a per user basis, and make the access decision there.
     * I will leave this code here for now as a reminder to implement
     * this at a later date. JRA.
     */

	if (!allow_access(lp_domain_hostsdeny(), lp_domain_hostsallow(),
	                  client_name(Client), client_addr(Client)))
	{
		DEBUG(0,("get_md4pw: Workstation %s denied access to domain\n", mach_acct));
		return False;
	}
#endif /* 0 */

	become_root(True);
	smb_pass = getsmbpwnam(mach_acct);
	unbecome_root(True);

	if ((smb_pass) != NULL && !(smb_pass->acct_ctrl & ACB_DISABLED) &&
        (smb_pass->smb_nt_passwd != NULL))
	{
		memcpy(md4pw, smb_pass->smb_nt_passwd, 16);
		dump_data(5, md4pw, 16);

		return True;
	}
	DEBUG(0,("get_md4pw: Workstation %s: no account in domain\n", mach_acct));
	return False;
}

/*************************************************************************
 api_net_req_chal:
 *************************************************************************/

static BOOL api_net_req_chal( uint16 vuid, prs_struct *data, prs_struct *rdata)
{
	NET_Q_REQ_CHAL q_r;
	uint32 status = 0x0;

	fstring mach_acct;
	fstring mach_name;

	user_struct *vuser;

	DEBUG(5,("api_net_req_chal(%d): vuid %d\n", __LINE__, (int)vuid));

	if ((vuser = get_valid_user_struct(vuid)) == NULL)
		return False;

	/* grab the challenge... */
	if(!net_io_q_req_chal("", &q_r, data, 0)) {
		DEBUG(0,("api_net_req_chal: Failed to unmarshall NET_Q_REQ_CHAL.\n"));
		return False;
	}

	fstrcpy(mach_acct, dos_unistrn2(q_r.uni_logon_clnt.buffer,
	                            q_r.uni_logon_clnt.uni_str_len));

	fstrcpy(mach_name, mach_acct);
	strlower(mach_name);

	fstrcat(mach_acct, "$");

	if (get_md4pw((char *)vuser->dc.md4pw, mach_name, mach_acct)) {
		/* copy the client credentials */
		memcpy(vuser->dc.clnt_chal.data          , q_r.clnt_chal.data, sizeof(q_r.clnt_chal.data));
		memcpy(vuser->dc.clnt_cred.challenge.data, q_r.clnt_chal.data, sizeof(q_r.clnt_chal.data));

		/* create a server challenge for the client */
		/* Set these to random values. */
                generate_random_buffer(vuser->dc.srv_chal.data, 8, False);

		memcpy(vuser->dc.srv_cred.challenge.data, vuser->dc.srv_chal.data, 8);

		memset((char *)vuser->dc.sess_key, '\0', sizeof(vuser->dc.sess_key));

		/* from client / server challenges and md4 password, generate sess key */
		cred_session_key(&(vuser->dc.clnt_chal), &(vuser->dc.srv_chal),
				 (char *)vuser->dc.md4pw, vuser->dc.sess_key);
	} else {
		/* lkclXXXX take a guess at a good error message to return :-) */
		status = 0xC0000000 | NT_STATUS_NOLOGON_WORKSTATION_TRUST_ACCOUNT;
	}

	/* construct reply. */
	if(!net_reply_req_chal(&q_r, rdata, &vuser->dc.srv_chal, status))
		return False;

	return True;
}

/*************************************************************************
 api_net_auth_2:
 *************************************************************************/

static BOOL api_net_auth_2( uint16 vuid, prs_struct *data, prs_struct *rdata)
{
	NET_Q_AUTH_2 q_a;
	uint32 status = 0x0;

	DOM_CHAL srv_cred;
	UTIME srv_time;

	user_struct *vuser;

	if ((vuser = get_valid_user_struct(vuid)) == NULL)
		return False;

	srv_time.time = 0;

	/* grab the challenge... */
	if(!net_io_q_auth_2("", &q_a, data, 0)) {
		DEBUG(0,("api_net_auth_2: Failed to unmarshall NET_Q_AUTH_2.\n"));
		return False;
	}

	/* check that the client credentials are valid */
	if (cred_assert(&(q_a.clnt_chal), vuser->dc.sess_key,
                    &(vuser->dc.clnt_cred.challenge), srv_time)) {

		/* create server challenge for inclusion in the reply */
		cred_create(vuser->dc.sess_key, &(vuser->dc.srv_cred.challenge), srv_time, &srv_cred);

		/* copy the received client credentials for use next time */
		memcpy(vuser->dc.clnt_cred.challenge.data, q_a.clnt_chal.data, sizeof(q_a.clnt_chal.data));
		memcpy(vuser->dc.srv_cred .challenge.data, q_a.clnt_chal.data, sizeof(q_a.clnt_chal.data));
	} else {
		status = NT_STATUS_ACCESS_DENIED | 0xC0000000;
	}

	/* construct reply. */
	if(!net_reply_auth_2(&q_a, rdata, &srv_cred, status))
		return False;

	return True;
}


/*************************************************************************
 api_net_srv_pwset:
 *************************************************************************/

static BOOL api_net_srv_pwset( uint16 vuid, prs_struct *data, prs_struct *rdata)
{
	NET_Q_SRV_PWSET q_a;
	uint32 status = NT_STATUS_WRONG_PASSWORD|0xC0000000;
	DOM_CRED srv_cred;
	pstring mach_acct;
	struct smb_passwd *smb_pass;
	BOOL ret;
	user_struct *vuser;

	if ((vuser = get_valid_user_struct(vuid)) == NULL)
		return False;

	/* grab the challenge and encrypted password ... */
	if(!net_io_q_srv_pwset("", &q_a, data, 0)) {
		DEBUG(0,("api_net_srv_pwset: Failed to unmarshall NET_Q_SRV_PWSET.\n"));
		return False;
	}

	/* checks and updates credentials.  creates reply credentials */
	if (deal_with_creds(vuser->dc.sess_key, &(vuser->dc.clnt_cred), 
	                    &(q_a.clnt_id.cred), &srv_cred))
	{
		memcpy(&(vuser->dc.srv_cred), &(vuser->dc.clnt_cred), sizeof(vuser->dc.clnt_cred));

		DEBUG(5,("api_net_srv_pwset: %d\n", __LINE__));

		pstrcpy(mach_acct, dos_unistrn2(q_a.clnt_id.login.uni_acct_name.buffer,
		                            q_a.clnt_id.login.uni_acct_name.uni_str_len));

		DEBUG(3,("Server Password Set Wksta:[%s]\n", mach_acct));

		become_root(True);
		smb_pass = getsmbpwnam(mach_acct);
		unbecome_root(True);

		if (smb_pass != NULL) {
			unsigned char pwd[16];
			int i;

			DEBUG(100,("Server password set : new given value was :\n"));
			for(i = 0; i < 16; i++)
				DEBUG(100,("%02X ", q_a.pwd[i]));
			DEBUG(100,("\n"));

			cred_hash3( pwd, q_a.pwd, vuser->dc.sess_key, 0);

			/* lies!  nt and lm passwords are _not_ the same: don't care */
			smb_pass->smb_passwd    = pwd;
			smb_pass->smb_nt_passwd = pwd;
			smb_pass->acct_ctrl     = ACB_WSTRUST;

			become_root(True);
			ret = mod_smbpwd_entry(smb_pass,False);
			unbecome_root(True);

			if (ret) {
				/* hooray! */
				status = 0x0;
			}
		}

		DEBUG(5,("api_net_srv_pwset: %d\n", __LINE__));

	} else {
		/* lkclXXXX take a guess at a sensible error code to return... */
		status = 0xC0000000 | NT_STATUS_NETWORK_CREDENTIAL_CONFLICT;
	}

	/* Construct reply. */
	if(!net_reply_srv_pwset(&q_a, rdata, &srv_cred, status))
		return False;

	return True;
}


/*************************************************************************
 api_net_sam_logoff:
 *************************************************************************/

static BOOL api_net_sam_logoff( uint16 vuid, prs_struct *data, prs_struct *rdata)
{
	NET_Q_SAM_LOGOFF q_l;
	NET_ID_INFO_CTR ctr;	

	DOM_CRED srv_cred;

	user_struct *vuser;

	if ((vuser = get_valid_user_struct(vuid)) == NULL)
		return False;

	/* the DOM_ID_INFO_1 structure is a bit big.  plus we might want to
	   dynamically allocate it inside net_io_q_sam_logon, at some point */
	q_l.sam_id.ctr = &ctr;

	/* grab the challenge... */
	if(!net_io_q_sam_logoff("", &q_l, data, 0)) {
		DEBUG(0,("api_net_sam_logoff: Failed to unmarshall NET_Q_SAM_LOGOFF.\n"));
		return False;
	}

	/* checks and updates credentials.  creates reply credentials */
	deal_with_creds(vuser->dc.sess_key, &vuser->dc.clnt_cred, 
	                &q_l.sam_id.client.cred, &srv_cred);
	memcpy(&vuser->dc.srv_cred, &vuser->dc.clnt_cred, sizeof(vuser->dc.clnt_cred));

	/* construct reply.  always indicate success */
	if(!net_reply_sam_logoff(&q_l, rdata, &srv_cred, 0x0))
		return False;

	return True;
}

/*************************************************************************
 net_login_interactive:
 *************************************************************************/

static uint32 net_login_interactive(NET_ID_INFO_1 *id1, struct smb_passwd *smb_pass,
				user_struct *vuser)
{
	uint32 status = 0x0;

	char nt_pwd[16];
	char lm_pwd[16];
	unsigned char key[16];

	memset(key, 0, 16);
	memcpy(key, vuser->dc.sess_key, 8);

	memcpy(lm_pwd, id1->lm_owf.data, 16);
	memcpy(nt_pwd, id1->nt_owf.data, 16);

#ifdef DEBUG_PASSWORD
	DEBUG(100,("key:"));
	dump_data(100, (char *)key, 16);

	DEBUG(100,("lm owf password:"));
	dump_data(100, lm_pwd, 16);

	DEBUG(100,("nt owf password:"));
	dump_data(100, nt_pwd, 16);
#endif

	SamOEMhash((uchar *)lm_pwd, key, False);
	SamOEMhash((uchar *)nt_pwd, key, False);

#ifdef DEBUG_PASSWORD
	DEBUG(100,("decrypt of lm owf password:"));
	dump_data(100, lm_pwd, 16);

	DEBUG(100,("decrypt of nt owf password:"));
	dump_data(100, nt_pwd, 16);
#endif

	if (memcmp(smb_pass->smb_passwd   , lm_pwd, 16) != 0 ||
	    memcmp(smb_pass->smb_nt_passwd, nt_pwd, 16) != 0)
	{
		status = 0xC0000000 | NT_STATUS_WRONG_PASSWORD;
	}

	return status;
}

/*************************************************************************
 net_login_network:
 *************************************************************************/

static uint32 net_login_network(NET_ID_INFO_2 *id2, struct smb_passwd *smb_pass)
{
	DEBUG(5,("net_login_network: lm_len: %d nt_len: %d\n",
		id2->hdr_lm_chal_resp.str_str_len, 
		id2->hdr_nt_chal_resp.str_str_len));

	/* JRA. Check the NT password first if it exists - this is a higher quality 
           password, if it exists and it doesn't match - fail. */

	if (id2->hdr_nt_chal_resp.str_str_len == 24 && 
		smb_pass->smb_nt_passwd != NULL)
	{
		if(smb_password_check((char *)id2->nt_chal_resp.buffer,
		                   smb_pass->smb_nt_passwd,
                           id2->lm_chal)) 
			return 0x0;
		else
			return 0xC0000000 | NT_STATUS_WRONG_PASSWORD;
	}

	/* lkclXXXX this is not a good place to put disabling of LM hashes in.
	   if that is to be done, first move this entire function into a
	   library routine that calls the two smb_password_check() functions.
	   if disabling LM hashes (which nt can do for security reasons) then
	   an attempt should be made to disable them everywhere (which nt does
	   not do, for various security-hole reasons).
	 */

	if (id2->hdr_lm_chal_resp.str_str_len == 24 &&
		smb_password_check((char *)id2->lm_chal_resp.buffer,
		                   smb_pass->smb_passwd,
		                   id2->lm_chal))
	{
		return 0x0;
	}


	/* oops! neither password check succeeded */

	return 0xC0000000 | NT_STATUS_WRONG_PASSWORD;
}

/*************************************************************************
 api_net_sam_logon:
 *************************************************************************/

static BOOL api_net_sam_logon( uint16 vuid, prs_struct *data, prs_struct *rdata)
{
  NET_Q_SAM_LOGON q_l;
  NET_ID_INFO_CTR ctr;	
  NET_USER_INFO_3 usr_info;
  uint32 status = 0x0;
  DOM_CRED srv_cred;
  struct smb_passwd *smb_pass = NULL;
  UNISTR2 *uni_samlogon_user = NULL;
  fstring nt_username;

  user_struct *vuser = NULL;

  if ((vuser = get_valid_user_struct(vuid)) == NULL)
    return False;

  memset(&q_l, '\0', sizeof(q_l));
  memset(&ctr, '\0', sizeof(ctr));
  memset(&usr_info, '\0', sizeof(usr_info));

  q_l.sam_id.ctr = &ctr;

  if(!net_io_q_sam_logon("", &q_l, data, 0)) {
    DEBUG(0,("api_net_sam_logon: Failed to unmarshall NET_Q_SAM_LOGON.\n"));
    return False;
  }

  /* checks and updates credentials.  creates reply credentials */
  if (!deal_with_creds(vuser->dc.sess_key, &(vuser->dc.clnt_cred), 
                       &(q_l.sam_id.client.cred), &srv_cred))
    status = 0xC0000000 | NT_STATUS_INVALID_HANDLE;
  else
    memcpy(&(vuser->dc.srv_cred), &(vuser->dc.clnt_cred), sizeof(vuser->dc.clnt_cred));

  /* find the username */

  if (status == 0) {
    switch (q_l.sam_id.logon_level) {
      case INTERACTIVE_LOGON_TYPE:
        uni_samlogon_user = &q_l.sam_id.ctr->auth.id1.uni_user_name;

        DEBUG(3,("SAM Logon (Interactive). Domain:[%s].  ", lp_workgroup()));
        break;
      case NET_LOGON_TYPE:
        uni_samlogon_user = &q_l.sam_id.ctr->auth.id2.uni_user_name;

        DEBUG(3,("SAM Logon (Network). Domain:[%s].  ", lp_workgroup()));
        break;
      default:
        DEBUG(2,("SAM Logon: unsupported switch value\n"));
        status = 0xC0000000 | NT_STATUS_INVALID_INFO_CLASS;
        break;
    } /* end switch */
  } /* end if status == 0 */

  /* check username exists */

  if (status == 0) {
    pstrcpy(nt_username, dos_unistrn2(uni_samlogon_user->buffer,
            uni_samlogon_user->uni_str_len));

    DEBUG(3,("User:[%s]\n", nt_username));

    /*
     * Convert to a UNIX username.
     */
    map_username(nt_username);

    /*
     * Do any case conversions.
     */
    (void)Get_Pwnam(nt_username, True);

    become_root(True);
    smb_pass = getsmbpwnam(nt_username);
    unbecome_root(True);

    if (smb_pass == NULL)
      status = 0xC0000000 | NT_STATUS_NO_SUCH_USER;
    else if (smb_pass->acct_ctrl & ACB_PWNOTREQ)
      status = 0;
    else if (smb_pass->acct_ctrl & ACB_DISABLED)
      status =  0xC0000000 | NT_STATUS_ACCOUNT_DISABLED;
  }

  /* Validate password - if required. */

  if ((status == 0) && !(smb_pass->acct_ctrl & ACB_PWNOTREQ)) {
    switch (q_l.sam_id.logon_level) {
      case INTERACTIVE_LOGON_TYPE:
        /* interactive login. */
        status = net_login_interactive(&q_l.sam_id.ctr->auth.id1, smb_pass, vuser);
        break;
      case NET_LOGON_TYPE:
        /* network login.  lm challenge and 24 byte responses */
        status = net_login_network(&q_l.sam_id.ctr->auth.id2, smb_pass);
        break;
    }
  }
	
  /* lkclXXXX this is the point at which, if the login was
     successful, that the SAM Local Security Authority should
     record that the user is logged in to the domain.
   */

  /* return the profile plus other bits :-) */

  if (status == 0) {
    DOM_GID *gids = NULL;
    int num_gids = 0;
    NTTIME dummy_time;
    pstring logon_script;
    pstring profile_path;
    pstring home_dir;
    pstring home_drive;
    pstring my_name;
    pstring my_workgroup;
    pstring domain_groups;
    uint32 r_uid;
    uint32 r_gid;

    /* set up pointer indicating user/password failed to be found */
    usr_info.ptr_user_info = 0;

    dummy_time.low  = 0xffffffff;
    dummy_time.high = 0x7fffffff;

    /* XXXX hack to get standard_sub_basic() to use sam logon username */
    /* possibly a better way would be to do a become_user() call */
    sam_logon_in_ssb = True;
    pstrcpy(samlogon_user, nt_username);

    pstrcpy(logon_script, lp_logon_script());
    pstrcpy(profile_path, lp_logon_path());

    pstrcpy(my_workgroup, lp_workgroup());

    pstrcpy(home_drive, lp_logon_drive());
    pstrcpy(home_dir, lp_logon_home());

    pstrcpy(my_name, global_myname);
    strupper(my_name);

    /*
     * This is the point at which we get the group
     * database - we should be getting the gid_t list
     * from /etc/group and then turning the uids into
     * rids and then into machine sids for this user.
     * JRA.
     */

    get_domain_user_groups(domain_groups, nt_username);

    /*
     * make_dom_gids allocates the gids array. JRA.
     */
    gids = NULL;
    num_gids = make_dom_gids(domain_groups, &gids);

    sam_logon_in_ssb = False;

    if (pdb_name_to_rid(nt_username, &r_uid, &r_gid))
      init_net_user_info3(&usr_info,
                          &dummy_time, /* logon_time */
                          &dummy_time, /* logoff_time */
                          &dummy_time, /* kickoff_time */
                          &dummy_time, /* pass_last_set_time */
                          &dummy_time, /* pass_can_change_time */
                          &dummy_time, /* pass_must_change_time */

                          nt_username   , /* user_name */
                          vuser->real_name, /* full_name */
                          logon_script    , /* logon_script */
                          profile_path    , /* profile_path */
                          home_dir        , /* home_dir */
                          home_drive      , /* dir_drive */

                          0, /* logon_count */
                          0, /* bad_pw_count */

                          r_uid   , /* RID user_id */
                          r_gid   , /* RID group_id */
                          num_gids,    /* uint32 num_groups */
                          gids    , /* DOM_GID *gids */
                          0x20    , /* uint32 user_flgs (?) */

                          NULL, /* char sess_key[16] */

                          my_name     , /* char *logon_srv */
                          my_workgroup, /* char *logon_dom */

                          &global_sam_sid,     /* DOM_SID *dom_sid */
                          NULL); /* char *other_sids */
    else
      status = 0xC0000000 | NT_STATUS_NO_SUCH_USER;

    /* Free any allocated groups array. */
    if(gids)
      free((char *)gids);
  }

  if(!net_reply_sam_logon(&q_l, rdata, &srv_cred, &usr_info, status))
    return False;

  return True;
}


/*************************************************************************
 api_net_trust_dom_list:
 *************************************************************************/

static BOOL api_net_trust_dom_list( uint16 vuid,
                                 prs_struct *data,
                                 prs_struct *rdata)
{
	NET_Q_TRUST_DOM_LIST q_t;

	char *trusted_domain = "test_domain";

	DEBUG(6,("api_net_trust_dom_list: %d\n", __LINE__));

	/* grab the lsa trusted domain list query... */
	if(!net_io_q_trust_dom("", &q_t, data, 0)) {
		DEBUG(0,("api_net_trust_dom_list: Failed to unmarshall NET_Q_TRUST_DOM_LIST.\n"));
		return False;
	}

	/* construct reply. */
	if(!net_reply_trust_dom_list(&q_t, rdata, 1, trusted_domain))
		return False;

	DEBUG(6,("api_net_trust_dom_list: %d\n", __LINE__));

	return True;
}


/*************************************************************************
 error messages cropping up when using nltest.exe...
 *************************************************************************/
#define ERROR_NO_SUCH_DOMAIN   0x54b
#define ERROR_NO_LOGON_SERVERS 0x51f

/*************************************************************************
 api_net_logon_ctrl2:
 *************************************************************************/

static BOOL api_net_logon_ctrl2( uint16 vuid,
                                 prs_struct *data,
                                 prs_struct *rdata)
{
	NET_Q_LOGON_CTRL2 q_l;

	/* lkclXXXX - guess what - absolutely no idea what these are! */
	uint32 flags = 0x0;
	uint32 pdc_connection_status = 0x0;
	uint32 logon_attempts = 0x0;
	uint32 tc_status = ERROR_NO_LOGON_SERVERS;
	char *trusted_domain = "test_domain";

	DEBUG(6,("api_net_logon_ctrl2: %d\n", __LINE__));

	/* grab the lsa netlogon ctrl2 query... */
	if(!net_io_q_logon_ctrl2("", &q_l, data, 0)) {
		DEBUG(0,("api_net_logon_ctrl2: Failed to unmarshall NET_Q_LOGON_CTRL2.\n"));
		return False;
	}

	/* construct reply. */
	if(!net_reply_logon_ctrl2(&q_l, rdata,
				flags, pdc_connection_status, logon_attempts,
				tc_status, trusted_domain))
		return False;

	DEBUG(6,("api_net_logon_ctrl2: %d\n", __LINE__));

	return True;
}

/*******************************************************************
 array of \PIPE\NETLOGON operations
 ********************************************************************/
static struct api_struct api_net_cmds [] =
{
	{ "NET_REQCHAL"       , NET_REQCHAL       , api_net_req_chal       }, 
	{ "NET_AUTH2"         , NET_AUTH2         , api_net_auth_2         }, 
	{ "NET_SRVPWSET"      , NET_SRVPWSET      , api_net_srv_pwset      }, 
	{ "NET_SAMLOGON"      , NET_SAMLOGON      , api_net_sam_logon      }, 
	{ "NET_SAMLOGOFF"     , NET_SAMLOGOFF     , api_net_sam_logoff     }, 
	{ "NET_LOGON_CTRL2"   , NET_LOGON_CTRL2   , api_net_logon_ctrl2    }, 
	{ "NET_TRUST_DOM_LIST", NET_TRUST_DOM_LIST, api_net_trust_dom_list },
    {  NULL               , 0                 , NULL                   }
};

/*******************************************************************
 receives a netlogon pipe and responds.
 ********************************************************************/

BOOL api_netlog_rpc(pipes_struct *p, prs_struct *data)
{
	return api_rpcTNP(p, "api_netlog_rpc", api_net_cmds, data);
}
