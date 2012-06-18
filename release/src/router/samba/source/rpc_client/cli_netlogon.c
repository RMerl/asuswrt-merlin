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


#ifdef SYSLOG
#undef SYSLOG
#endif

#include "includes.h"

extern int DEBUGLEVEL;
extern pstring global_myname;
extern fstring global_myworkgroup;

/****************************************************************************
Generate the next creds to use.
****************************************************************************/

static void gen_next_creds( struct cli_state *cli, DOM_CRED *new_clnt_cred)
{
  /*
   * Create the new client credentials.
   */

  cli->clnt_cred.timestamp.time = time(NULL);

  memcpy(new_clnt_cred, &cli->clnt_cred, sizeof(*new_clnt_cred));

  /* Calculate the new credentials. */
  cred_create(cli->sess_key, &(cli->clnt_cred.challenge),
              new_clnt_cred->timestamp, &(new_clnt_cred->challenge));

}

#if UNUSED_CODE
/****************************************************************************
do a LSA Logon Control2
****************************************************************************/
BOOL cli_net_logon_ctrl2(struct cli_state *cli, uint32 status_level)
{
  prs_struct rbuf;
  prs_struct buf; 
  NET_Q_LOGON_CTRL2 q_l;
  BOOL ok = False;

  prs_init(&buf , 1024, 4, False);
  prs_init(&rbuf, 0,    4, True );

  /* create and send a MSRPC command with api NET_LOGON_CTRL2 */

  DEBUG(4,("do_net_logon_ctrl2 from %s status level:%x\n",
           global_myname, status_level));

  /* store the parameters */
  init_q_logon_ctrl2(&q_l, cli->srv_name_slash, status_level);

  /* turn parameters into data stream */
  if(!net_io_q_logon_ctrl2("", &q_l,  &buf, 0)) {
    DEBUG(0,("cli_net_logon_ctrl2: Error : failed to marshall NET_Q_LOGON_CTRL2 struct.\n"));
    prs_mem_free(&buf);
    prs_mem_free(&rbuf);
    return False;
  }

  /* send the data on \PIPE\ */
  if (rpc_api_pipe_req(cli, NET_LOGON_CTRL2, &buf, &rbuf))
  {
    NET_R_LOGON_CTRL2 r_l;

    /*
     * Unmarshall the return buffer.
     */
    ok = net_io_r_logon_ctrl2("", &r_l, &rbuf, 0);
		
    if (ok && r_l.status != 0)
    {
      /* report error code */
      DEBUG(0,("do_net_logon_ctrl2: Error %s\n", get_nt_error_msg(r_l.status)));
      cli->nt_error = r_l.status;
      ok = False;
    }
  }

  prs_mem_free(&buf);
  prs_mem_free(&rbuf);

  return ok;
}
#endif

/****************************************************************************
LSA Authenticate 2

Send the client credential, receive back a server credential.
Ensure that the server credential returned matches the session key 
encrypt of the server challenge originally received. JRA.
****************************************************************************/

BOOL cli_net_auth2(struct cli_state *cli, uint16 sec_chan, 
                   uint32 neg_flags, DOM_CHAL *srv_chal)
{
  prs_struct rbuf;
  prs_struct buf; 
  NET_Q_AUTH_2 q_a;
  BOOL ok = False;

  prs_init(&buf , 1024, 4, False);
  prs_init(&rbuf, 0,    4, True );

  /* create and send a MSRPC command with api NET_AUTH2 */

  DEBUG(4,("cli_net_auth2: srv:%s acct:%s sc:%x mc: %s chal %s neg: %x\n",
         cli->srv_name_slash, cli->mach_acct, sec_chan, global_myname,
         credstr(cli->clnt_cred.challenge.data), neg_flags));

  /* store the parameters */
  init_q_auth_2(&q_a, cli->srv_name_slash, cli->mach_acct, sec_chan, global_myname,
                &cli->clnt_cred.challenge, neg_flags);

  /* turn parameters into data stream */
  if(!net_io_q_auth_2("", &q_a,  &buf, 0)) {
    DEBUG(0,("cli_net_auth2: Error : failed to marshall NET_Q_AUTH_2 struct.\n"));
    prs_mem_free(&buf);
    prs_mem_free(&rbuf);
    return False;
  }

  /* send the data on \PIPE\ */
  if (rpc_api_pipe_req(cli, NET_AUTH2, &buf, &rbuf))
  {
    NET_R_AUTH_2 r_a;

    ok = net_io_r_auth_2("", &r_a, &rbuf, 0);
		
    if (ok && r_a.status != 0)
    {
      /* report error code */
      DEBUG(0,("cli_net_auth2: Error %s\n", get_nt_error_msg(r_a.status)));
      cli->nt_error = r_a.status;
      ok = False;
    }

    if (ok)
    {
      /* 
       * Check the returned value using the initial
       * server received challenge.
       */
      UTIME zerotime;

      zerotime.time = 0;
      if(cred_assert( &r_a.srv_chal, cli->sess_key, srv_chal, zerotime) == 0) {
        /*
         * Server replied with bad credential. Fail.
         */
        DEBUG(0,("cli_net_auth2: server %s replied with bad credential (bad machine \
password ?).\n", cli->desthost ));
        ok = False;
      }
    }

#if 0
    /*
     * Try commenting this out to see if this makes the connect
     * work for a NT 3.51 PDC. JRA.
     */

    if (ok && r_a.srv_flgs.neg_flags != q_a.clnt_flgs.neg_flags)
    {
      /* report different neg_flags */
      DEBUG(0,("cli_net_auth2: error neg_flags (q,r) differ - (%x,%x)\n",
          q_a.clnt_flgs.neg_flags, r_a.srv_flgs.neg_flags));
      ok = False;
    }
#endif

  }

  prs_mem_free(&buf);
  prs_mem_free(&rbuf);

  return ok;
}

/****************************************************************************
LSA Request Challenge. Sends our challenge to server, then gets
server response. These are used to generate the credentials.
****************************************************************************/

BOOL cli_net_req_chal(struct cli_state *cli, DOM_CHAL *clnt_chal, DOM_CHAL *srv_chal)
{
  prs_struct rbuf;
  prs_struct buf; 
  NET_Q_REQ_CHAL q_c;
  BOOL valid_chal = False;

  prs_init(&buf , 1024, 4, False);
  prs_init(&rbuf, 0,    4, True );

  /* create and send a MSRPC command with api NET_REQCHAL */

  DEBUG(4,("cli_net_req_chal: LSA Request Challenge from %s to %s: %s\n",
         cli->desthost, global_myname, credstr(clnt_chal->data)));

  /* store the parameters */
  init_q_req_chal(&q_c, cli->srv_name_slash, global_myname, clnt_chal);

  /* turn parameters into data stream */
  if(!net_io_q_req_chal("", &q_c,  &buf, 0)) {
    DEBUG(0,("cli_net_req_chal: Error : failed to marshall NET_Q_REQ_CHAL struct.\n"));
    prs_mem_free(&buf);
    prs_mem_free(&rbuf);
    return False;
  }

  /* send the data on \PIPE\ */
  if (rpc_api_pipe_req(cli, NET_REQCHAL, &buf, &rbuf))
  {
    NET_R_REQ_CHAL r_c;
    BOOL ok;

    ok = net_io_r_req_chal("", &r_c, &rbuf, 0);
		
    if (ok && r_c.status != 0)
    {
      /* report error code */
      DEBUG(0,("cli_net_req_chal: Error %s\n", get_nt_error_msg(r_c.status)));
      cli->nt_error = r_c.status;
      ok = False;
    }

    if (ok)
    {
      /* ok, at last: we're happy. return the challenge */
      memcpy(srv_chal, r_c.srv_chal.data, sizeof(srv_chal->data));
      valid_chal = True;
    }
  }

  prs_mem_free(&buf);
  prs_mem_free(&rbuf);

  return valid_chal;
}

/***************************************************************************
LSA Server Password Set.
****************************************************************************/

BOOL cli_net_srv_pwset(struct cli_state *cli, uint8 hashed_mach_pwd[16])
{
  prs_struct rbuf;
  prs_struct buf; 
  DOM_CRED new_clnt_cred;
  NET_Q_SRV_PWSET q_s;
  BOOL ok = False;
  uint16 sec_chan_type = 2;

  gen_next_creds( cli, &new_clnt_cred);

  prs_init(&buf , 1024, 4, False);
  prs_init(&rbuf, 0,    4, True );

  /* create and send a MSRPC command with api NET_SRV_PWSET */

  DEBUG(4,("cli_net_srv_pwset: srv:%s acct:%s sc: %d mc: %s clnt %s %x\n",
           cli->srv_name_slash, cli->mach_acct, sec_chan_type, global_myname,
           credstr(new_clnt_cred.challenge.data), new_clnt_cred.timestamp.time));

  /* store the parameters */
  init_q_srv_pwset(&q_s, cli->srv_name_slash, cli->mach_acct, sec_chan_type,
                   global_myname, &new_clnt_cred, (char *)hashed_mach_pwd);

  /* turn parameters into data stream */
  if(!net_io_q_srv_pwset("", &q_s,  &buf, 0)) {
    DEBUG(0,("cli_net_srv_pwset: Error : failed to marshall NET_Q_SRV_PWSET struct.\n"));
    prs_mem_free(&buf);
    prs_mem_free(&rbuf);
    return False;
  }

  /* send the data on \PIPE\ */
  if (rpc_api_pipe_req(cli, NET_SRVPWSET, &buf, &rbuf))
  {
    NET_R_SRV_PWSET r_s;

    ok = net_io_r_srv_pwset("", &r_s, &rbuf, 0);
		
    if (ok && r_s.status != 0)
    {
      /* report error code */
      DEBUG(0,("cli_net_srv_pwset: %s\n", get_nt_error_msg(r_s.status)));
      cli->nt_error = r_s.status;
      ok = False;
    }

    /* Update the credentials. */
    if (ok && !clnt_deal_with_creds(cli->sess_key, &(cli->clnt_cred), &(r_s.srv_cred)))
    {
      /*
       * Server replied with bad credential. Fail.
       */
      DEBUG(0,("cli_net_srv_pwset: server %s replied with bad credential (bad machine \
password ?).\n", cli->desthost ));
      ok = False;
    }
  }

  prs_mem_free(&buf);
  prs_mem_free(&rbuf);

  return ok;
}

/***************************************************************************
LSA SAM Logon - interactive or network.
****************************************************************************/

BOOL cli_net_sam_logon(struct cli_state *cli, NET_ID_INFO_CTR *ctr, 
                       NET_USER_INFO_3 *user_info3)
{
  DOM_CRED new_clnt_cred;
  DOM_CRED dummy_rtn_creds;
  prs_struct rbuf;
  prs_struct buf; 
  uint16 validation_level = 3;
  NET_Q_SAM_LOGON q_s;
  BOOL ok = False;

  gen_next_creds( cli, &new_clnt_cred);

  prs_init(&buf , 1024, 4, False);
  prs_init(&rbuf, 0,    4, True );

  /* create and send a MSRPC command with api NET_SAMLOGON */

  DEBUG(4,("cli_net_sam_logon: srv:%s mc:%s clnt %s %x ll: %d\n",
             cli->srv_name_slash, global_myname, 
             credstr(new_clnt_cred.challenge.data), cli->clnt_cred.timestamp.time,
             ctr->switch_value));

  memset(&dummy_rtn_creds, '\0', sizeof(dummy_rtn_creds));
	dummy_rtn_creds.timestamp.time = time(NULL);

  /* store the parameters */
  q_s.validation_level = validation_level;
  init_sam_info(&q_s.sam_id, cli->srv_name_slash, global_myname,
         &new_clnt_cred, &dummy_rtn_creds, ctr->switch_value, ctr);

  /* turn parameters into data stream */
  if(!net_io_q_sam_logon("", &q_s,  &buf, 0)) {
    DEBUG(0,("cli_net_sam_logon: Error : failed to marshall NET_Q_SAM_LOGON struct.\n"));
    prs_mem_free(&buf);
    prs_mem_free(&rbuf);
    return False;
  }

  /* send the data on \PIPE\ */
  if (rpc_api_pipe_req(cli, NET_SAMLOGON, &buf, &rbuf))
  {
    NET_R_SAM_LOGON r_s;

    r_s.user = user_info3;

    ok = net_io_r_sam_logon("", &r_s, &rbuf, 0);
		
    if (ok && r_s.status != 0)
    {
      /* report error code */
      DEBUG(0,("cli_net_sam_logon: %s\n", get_nt_error_msg(r_s.status)));
      cli->nt_error = r_s.status;
      ok = False;
    }

    /* Update the credentials. */
    if (ok && !clnt_deal_with_creds(cli->sess_key, &(cli->clnt_cred), &(r_s.srv_creds)))
    {
      /*
       * Server replied with bad credential. Fail.
       */
      DEBUG(0,("cli_net_sam_logon: server %s replied with bad credential (bad machine \
password ?).\n", cli->desthost ));
        ok = False;
    }

    if (ok && r_s.switch_value != 3)
    {
      /* report different switch_value */
      DEBUG(0,("cli_net_sam_logon: switch_value of 3 expected %x\n",
                   r_s.switch_value));
      ok = False;
    }
  }

  prs_mem_free(&buf);
  prs_mem_free(&rbuf);

  return ok;
}

/***************************************************************************
LSA SAM Logoff.

This currently doesnt work correctly as the domain controller 
returns NT_STATUS_INVALID_INFO_CLASS - we obviously need to
send a different info level. Right now though, I'm not sure
what that needs to be (I need to see one on the wire before
I can be sure). JRA.
****************************************************************************/
BOOL cli_net_sam_logoff(struct cli_state *cli, NET_ID_INFO_CTR *ctr)
{
  DOM_CRED new_clnt_cred;
  DOM_CRED dummy_rtn_creds;
  prs_struct rbuf;
  prs_struct buf; 
  NET_Q_SAM_LOGOFF q_s;
  BOOL ok = False;

  gen_next_creds( cli, &new_clnt_cred);

  prs_init(&buf , 1024, 4, False);
  prs_init(&rbuf, 0,    4, True );

  /* create and send a MSRPC command with api NET_SAMLOGOFF */

  DEBUG(4,("cli_net_sam_logoff: srv:%s mc:%s clnt %s %x ll: %d\n",
            cli->srv_name_slash, global_myname,
            credstr(new_clnt_cred.challenge.data), new_clnt_cred.timestamp.time,
            ctr->switch_value));

  memset(&dummy_rtn_creds, '\0', sizeof(dummy_rtn_creds));

  init_sam_info(&q_s.sam_id, cli->srv_name_slash, global_myname,
                &new_clnt_cred, &dummy_rtn_creds, ctr->switch_value, ctr);

  /* turn parameters into data stream */
  if(!net_io_q_sam_logoff("", &q_s,  &buf, 0)) {
    DEBUG(0,("cli_net_sam_logoff: Error : failed to marshall NET_Q_SAM_LOGOFF struct.\n"));
    prs_mem_free(&buf);
    prs_mem_free(&rbuf);
    return False;
  }

  /* send the data on \PIPE\ */
  if (rpc_api_pipe_req(cli, NET_SAMLOGOFF, &buf, &rbuf))
  {
    NET_R_SAM_LOGOFF r_s;

    ok = net_io_r_sam_logoff("", &r_s, &rbuf, 0);
		
    if (ok && r_s.status != 0)
    {
      /* report error code */
      DEBUG(0,("cli_net_sam_logoff: %s\n", get_nt_error_msg(r_s.status)));
      cli->nt_error = r_s.status;
      ok = False;
    }

    /* Update the credentials. */
    if (ok && !clnt_deal_with_creds(cli->sess_key, &(cli->clnt_cred), &(r_s.srv_creds)))
    {
      /*
       * Server replied with bad credential. Fail.
       */
      DEBUG(0,("cli_net_sam_logoff: server %s replied with bad credential (bad machine \
password ?).\n", cli->desthost ));
      ok = False;
    }
  }

  prs_mem_free(&buf);
  prs_mem_free(&rbuf);

  return ok;
}

/*********************************************************
 Change the domain password on the PDC.
**********************************************************/

static BOOL modify_trust_password( char *domain, char *remote_machine, 
                          unsigned char orig_trust_passwd_hash[16],
                          unsigned char new_trust_passwd_hash[16])
{
  struct cli_state cli;

  ZERO_STRUCT(cli);
  if(cli_initialise(&cli) == False) {
    DEBUG(0,("modify_trust_password: unable to initialize client connection.\n"));
    return False;
  }

  if(!resolve_name( remote_machine, &cli.dest_ip, 0x20)) {
    DEBUG(0,("modify_trust_password: Can't resolve address for %s\n", remote_machine));
    return False;
  }

  if (ismyip(cli.dest_ip)) {
    DEBUG(0,("modify_trust_password: Machine %s is one of our addresses. Cannot add \
to ourselves.\n", remote_machine));
    return False;
  }

  if (!cli_connect(&cli, remote_machine, &cli.dest_ip)) {
    DEBUG(0,("modify_trust_password: unable to connect to SMB server on \
machine %s. Error was : %s.\n", remote_machine, cli_errstr(&cli) ));
    return False;
  }
  
  if (!attempt_netbios_session_request(&cli, global_myname, remote_machine, &cli.dest_ip)) {
    DEBUG(0,("modify_trust_password: machine %s rejected the NetBIOS \
session request. Error was %s\n", remote_machine, cli_errstr(&cli) ));
    return False;
  }

  cli.protocol = PROTOCOL_NT1;
    
  if (!cli_negprot(&cli)) {
    DEBUG(0,("modify_trust_password: machine %s rejected the negotiate protocol. \
Error was : %s.\n", remote_machine, cli_errstr(&cli) ));
    cli_shutdown(&cli);
    return False;
  }

  if (cli.protocol != PROTOCOL_NT1) {
    DEBUG(0,("modify_trust_password: machine %s didn't negotiate NT protocol.\n", 
            remote_machine));
    cli_shutdown(&cli);
    return False;
  }
    
  /*
   * Do an anonymous session setup.
   */
    
  if (!cli_session_setup(&cli, "", "", 0, "", 0, "")) {
    DEBUG(0,("modify_trust_password: machine %s rejected the session setup. \
Error was : %s.\n", remote_machine, cli_errstr(&cli) ));
    cli_shutdown(&cli);
    return False;
  }
    
  if (!(cli.sec_mode & 1)) {
    DEBUG(0,("modify_trust_password: machine %s isn't in user level security mode\n",
          remote_machine));
    cli_shutdown(&cli);
    return False;
  }
    
  if (!cli_send_tconX(&cli, "IPC$", "IPC", "", 1)) {
    DEBUG(0,("modify_trust_password: machine %s rejected the tconX on the IPC$ share. \
Error was : %s.\n", remote_machine, cli_errstr(&cli) ));
    cli_shutdown(&cli);
    return False;
  }

  /*
   * Ok - we have an anonymous connection to the IPC$ share.
   * Now start the NT Domain stuff :-).
   */
    
  if(cli_nt_session_open(&cli, PIPE_NETLOGON) == False) {
    DEBUG(0,("modify_trust_password: unable to open the domain client session to \
machine %s. Error was : %s.\n", remote_machine, cli_errstr(&cli)));
    cli_nt_session_close(&cli);
    cli_ulogoff(&cli);
    cli_shutdown(&cli);
    return False;
  } 
  
  if(cli_nt_setup_creds(&cli, orig_trust_passwd_hash) == False) {
    DEBUG(0,("modify_trust_password: unable to setup the PDC credentials to machine \
%s. Error was : %s.\n", remote_machine, cli_errstr(&cli)));
    cli_nt_session_close(&cli);
    cli_ulogoff(&cli);
    cli_shutdown(&cli);
    return False;
  } 

  if( cli_nt_srv_pwset( &cli,new_trust_passwd_hash ) == False) {
    DEBUG(0,("modify_trust_password: unable to change password for machine %s in domain \
%s to Domain controller %s. Error was %s.\n", global_myname, domain, remote_machine, 
                            cli_errstr(&cli)));
    cli_close(&cli, cli.nt_pipe_fnum);
    cli_ulogoff(&cli);
    cli_shutdown(&cli);
    return False;
  }

  cli_nt_session_close(&cli);
  cli_ulogoff(&cli);
  cli_shutdown(&cli);

  return True;
}

/************************************************************************
 Change the trust account password for a domain.
 The user of this function must have locked the trust password file for
 update.
************************************************************************/

BOOL change_trust_account_password( char *domain, char *remote_machine_list)
{
  fstring remote_machine;
  unsigned char old_trust_passwd_hash[16];
  unsigned char new_trust_passwd_hash[16];
  time_t lct;
  BOOL res;

  if(!get_trust_account_password( old_trust_passwd_hash, &lct)) {
    DEBUG(0,("change_trust_account_password: unable to read the machine \
account password for domain %s.\n", domain));
    return False;
  }

  /*
   * Create the new (random) password.
   */
  generate_random_buffer( new_trust_passwd_hash, 16, True);

  while(remote_machine_list && 
	next_token(&remote_machine_list, remote_machine, 
		   LIST_SEP, sizeof(remote_machine))) {
    strupper(remote_machine);
    if(strequal(remote_machine, "*")) {

      /*
       * We have been asked to dynamcially determine the IP addresses of the PDC.
       */

      struct in_addr *ip_list = NULL;
      int count = 0;
      int i;

      if(!get_dc_list(domain, &ip_list, &count))
        continue;

      /*
       * Try and connect to the PDC/BDC list in turn as an IP
       * address used as a string.
       */

      for(i = 0; i < count; i++) {
        fstring dc_name;
        if(!lookup_pdc_name(global_myname, domain, &ip_list[i], dc_name))
          continue;
        if((res = modify_trust_password( domain, dc_name,
                                         old_trust_passwd_hash, new_trust_passwd_hash)))
          break;
      }

      if(ip_list != NULL)
        free((char *)ip_list);

    } else {
      res = modify_trust_password( domain, remote_machine,
                                   old_trust_passwd_hash, new_trust_passwd_hash);
    }

    if(res) {
      DEBUG(0,("%s : change_trust_account_password: Changed password for \
domain %s.\n", timestring(False), domain));
      /*
       * Return the result of trying to write the new password
       * back into the trust account file.
       */
      res = set_trust_account_password(new_trust_passwd_hash);
      memset(new_trust_passwd_hash, 0, 16);
      memset(old_trust_passwd_hash, 0, 16);
      return res;
    }
  }

  memset(new_trust_passwd_hash, 0, 16);
  memset(old_trust_passwd_hash, 0, 16);

  DEBUG(0,("%s : change_trust_account_password: Failed to change password for \
domain %s.\n", timestring(False), domain));
  return False;
}
