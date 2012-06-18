/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   NBT netbios routines and daemon - version 2
   Copyright (C) Andrew Tridgell 1994-1998
   Copyright (C) Luke Kenneth Casson Leighton 1994-1998
   Copyright (C) Jeremy Allison 1994-1998
   
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

extern int DEBUGLEVEL;

extern fstring global_myworkgroup;

/****************************************************************************
 Deal with a response packet when registering one of our names.
****************************************************************************/

static void register_name_response(struct subnet_record *subrec,
                       struct response_record *rrec, struct packet_struct *p)
{
  /* 
   * If we are registering broadcast, then getting a response is an
   * error - we do not have the name. If we are registering unicast,
   * then we expect to get a response.
   */

  struct nmb_packet *nmb = &p->packet.nmb;
  BOOL bcast = nmb->header.nm_flags.bcast;
  BOOL success = True;
  struct nmb_name *question_name = &rrec->packet->packet.nmb.question.question_name;
  struct nmb_name *answer_name = &nmb->answers->rr_name;
  int ttl = 0;
  uint16 nb_flags = 0;
  struct in_addr registered_ip;

  /* Sanity check. Ensure that the answer name in the incoming packet is the
     same as the requested name in the outgoing packet. */

  if(!question_name || !answer_name)
  {
    DEBUG(0,("register_name_response: malformed response (%s is NULL).\n",
           question_name ? "question_name" : "answer_name" ));
    return;
  }

  if(!nmb_name_equal(question_name, answer_name))
  {
    DEBUG(0,("register_name_response: Answer name %s differs from question \
name %s.\n", nmb_namestr(answer_name), nmb_namestr(question_name)));
    return;
  }

  if(bcast)
  {
    /*
     * Special hack to cope with old Samba nmbd's.
     * Earlier versions of Samba (up to 1.9.16p11) respond 
     * to a broadcast name registration of WORKGROUP<1b> when 
     * they should not. Hence, until these versions are gone, 
     * we should treat such errors as success for this particular
     * case only. jallison@whistle.com.
     */

#if 1 /* OLD_SAMBA_SERVER_HACK */
    if((nmb->header.rcode == ACT_ERR) && strequal(global_myworkgroup, answer_name->name) &&
         (answer_name->name_type == 0x1b))
    {
      /* Pretend we did not get this. */
      rrec->num_msgs--;

      DEBUG(5,("register_name_response: Ignoring broadcast response to \
registration of name %s due to old Samba server bug.\n", nmb_namestr(answer_name)));
      return;
    }
#endif /* OLD_SAMBA_SERVER_HACK */

    /* Someone else has the name. Log the problem. */
    DEBUG(1,("register_name_response: Failed to register \
name %s on subnet %s via broadcast. Error code was %d. Reject came from IP %s\n", 
              nmb_namestr(answer_name), 
              subrec->subnet_name, nmb->header.rcode, inet_ntoa(p->ip)));
    success = False;
  }
  else
  {
    /* Unicast - check to see if the response allows us to have the name. */
    if(nmb->header.rcode != 0)
    {
      /* Error code - we didn't get the name. */
      success = False;

      DEBUG(0,("register_name_response: server at IP %s rejected our \
name registration of %s with error code %d.\n", inet_ntoa(p->ip), 
                  nmb_namestr(answer_name), nmb->header.rcode));

    }
    else if(nmb->header.opcode == NMB_WACK_OPCODE)
    {
      /* WINS server is telling us to wait. Pretend we didn't get
         the response but don't send out any more register requests. */

      DEBUG(5,("register_name_response: WACK from WINS server %s in registering \
name %s on subnet %s.\n", inet_ntoa(p->ip), nmb_namestr(answer_name), subrec->subnet_name));

      rrec->repeat_count = 0;
      /* How long we should wait for. */
      rrec->repeat_time = p->timestamp + nmb->answers->ttl;
      rrec->num_msgs--;
      return;
    }
    else
    {
      success = True;
      /* Get the data we need to pass to the success function. */
      nb_flags = get_nb_flags(nmb->answers->rdata);
      putip((char*)&registered_ip,&nmb->answers->rdata[2]);
      ttl = nmb->answers->ttl;
    }
  } 

  DEBUG(5,("register_name_response: %s in registering name %s on subnet %s.\n",
        success ? "success" : "failure", nmb_namestr(answer_name), subrec->subnet_name));

  if(success)
  {
    /* Enter the registered name into the subnet name database before calling
       the success function. */
    standard_success_register(subrec, rrec->userdata, answer_name, nb_flags, ttl, registered_ip);
    if( rrec->success_fn)
      (*rrec->success_fn)(subrec, rrec->userdata, answer_name, nb_flags, ttl, registered_ip);
  }
  else
  {
    if( rrec->fail_fn)
      (*rrec->fail_fn)(subrec, rrec, question_name);
    /* Remove the name. */
    standard_fail_register( subrec, rrec, question_name);
  }

  /* Ensure we don't retry. */
  remove_response_record(subrec, rrec);
}

/****************************************************************************
 Deal with a timeout when registering one of our names.
****************************************************************************/

static void register_name_timeout_response(struct subnet_record *subrec,
                       struct response_record *rrec)
{
  /*
   * If we are registering unicast, then NOT getting a response is an
   * error - we do not have the name. If we are registering broadcast,
   * then we don't expect to get a response.
   */

  struct nmb_packet *sent_nmb = &rrec->packet->packet.nmb;
  BOOL bcast = sent_nmb->header.nm_flags.bcast;
  BOOL success = False;
  struct nmb_name *question_name = &sent_nmb->question.question_name;
  uint16 nb_flags = 0;
  int ttl = 0;
  struct in_addr registered_ip;

  if(bcast)
  {
    if(rrec->num_msgs == 0)
    {
      /* Not receiving a message is success for broadcast registration. */
      success = True; 

      /* Pull the success values from the original request packet. */
      nb_flags = get_nb_flags(sent_nmb->additional->rdata);
      ttl = sent_nmb->additional->ttl;
      putip(&registered_ip,&sent_nmb->additional->rdata[2]);
    }
  }
  else
  {
    /* Unicast - if no responses then it's an error. */
    if(rrec->num_msgs == 0)
    {
      DEBUG(2,("register_name_timeout_response: WINS server at address %s is not \
responding.\n", inet_ntoa(rrec->packet->ip)));

      /* Keep trying to contact the WINS server periodically. This allows
         us to work correctly if the WINS server is down temporarily when
         we come up. */

      /* Reset the number of attempts to zero and double the interval between
         retries. Max out at 5 minutes. */
      rrec->repeat_count = 3;
      rrec->repeat_interval *= 2;
      if(rrec->repeat_interval > (5 * 60))
        rrec->repeat_interval = (5 * 60);
      rrec->repeat_time = time(NULL) + rrec->repeat_interval;

      DEBUG(5,("register_name_timeout_response: increasing WINS timeout to %d seconds.\n",
              (int)rrec->repeat_interval));
      return; /* Don't remove the response record. */
    }
  }

  DEBUG(5,("register_name_timeout_response: %s in registering name %s on subnet %s.\n",
        success ? "success" : "failure", nmb_namestr(question_name), subrec->subnet_name));
  if(success)
  {
    /* Enter the registered name into the subnet name database before calling
       the success function. */
    standard_success_register(subrec, rrec->userdata, question_name, nb_flags, ttl, registered_ip);
    if( rrec->success_fn)
      (*rrec->success_fn)(subrec, rrec->userdata, question_name, nb_flags, ttl, registered_ip);
  }
  else
  {
    if( rrec->fail_fn)
      (*rrec->fail_fn)(subrec, rrec, question_name);
    /* Remove the name. */
    standard_fail_register( subrec, rrec, question_name);
  }

  /* Ensure we don't retry. */
  remove_response_record(subrec, rrec);
}

/****************************************************************************
 Try and register one of our names on the unicast subnet - multihomed.
****************************************************************************/

static BOOL multihomed_register_name( struct nmb_name *nmbname, uint16 nb_flags,
                                      register_name_success_function success_fn,
                                      register_name_fail_function fail_fn,
                                      struct userdata_struct *userdata)
{
  /*
     If we are adding a group name, we just send multiple
     register name packets to the WINS server (this is an
     internet group name.

     If we are adding a unique name, We need first to add 
     our names to the unicast subnet namelist. This is 
     because when a WINS server receives a multihomed 
     registration request, the first thing it does is to 
     send a name query to the registering machine, to see 
     if it has put the name in it's local namelist.
     We need the name there so the query response code in
     nmbd_incomingrequests.c will find it.

     We are adding this name prematurely (we don't really
     have it yet), but as this is on the unicast subnet
     only we will get away with this (only the WINS server
     will ever query names from us on this subnet).
   */

  int num_ips=0;
  int i;
  struct in_addr *ip_list = NULL;
  struct subnet_record *subrec;

  for(subrec = FIRST_SUBNET; subrec; subrec = NEXT_SUBNET_EXCLUDING_UNICAST(subrec) )
    num_ips++;

  if((ip_list = (struct in_addr *)malloc(num_ips * sizeof(struct in_addr)))==NULL)
  {
    DEBUG(0,("multihomed_register_name: malloc fail !\n"));
    return True;
  }

  for( subrec = FIRST_SUBNET, i = 0; 
       subrec;
       subrec = NEXT_SUBNET_EXCLUDING_UNICAST(subrec), i++ )
    ip_list[i] = subrec->myip;

  (void)add_name_to_subnet( unicast_subnet, nmbname->name, nmbname->name_type,
                            nb_flags, lp_max_ttl(), SELF_NAME,
                            num_ips, ip_list);

  /* Now try and register the name, num_ips times. On the last time use
     the given success and fail functions. */

  for( i = 0; i < num_ips; i++)
  {
    if(queue_register_multihomed_name( unicast_subnet,
        register_name_response,
        register_name_timeout_response,
        (i == num_ips - 1) ? success_fn : NULL,
        (i == num_ips - 1) ? fail_fn : NULL,
        (i == num_ips - 1) ? userdata : NULL,
        nmbname,
        nb_flags,
        ip_list[i]) == NULL)
    {
      DEBUG(0,("multihomed_register_name: Failed to send packet trying to \
register name %s IP %s\n", nmb_namestr(nmbname), inet_ntoa(ip_list[i]) ));

      free((char *)ip_list);
      return True;
    }
  }

  free((char *)ip_list);

  return False;
}

/****************************************************************************
 Try and register one of our names.
****************************************************************************/

BOOL register_name(struct subnet_record *subrec,
                   char *name, int type, uint16 nb_flags,
                   register_name_success_function success_fn,
                   register_name_fail_function fail_fn,
                   struct userdata_struct *userdata)
{
  struct nmb_name nmbname;

  make_nmb_name(&nmbname, name, type);

  /* Always set the NB_ACTIVE flag on the name we are
     registering. Doesn't make sense without it.
   */

  nb_flags |= NB_ACTIVE;

  /* If this is the unicast subnet, and we are a multi-homed
     host, then register a multi-homed name. */

  if( (subrec == unicast_subnet) && we_are_multihomed())
    return multihomed_register_name(&nmbname, nb_flags,
                                    success_fn, fail_fn,
                                    userdata);

  if(queue_register_name( subrec,
        register_name_response,
        register_name_timeout_response,
        success_fn,
        fail_fn,
        userdata,
        &nmbname,
        nb_flags) == NULL)
  {
    DEBUG(0,("register_name: Failed to send packet trying to register name %s\n",
          nmb_namestr(&nmbname)));
    return True;
  }
  return False;
}

/****************************************************************************
 Try and refresh one of our names.
****************************************************************************/

BOOL refresh_name(struct subnet_record *subrec, struct name_record *namerec,
                  refresh_name_success_function success_fn,
                  refresh_name_fail_function fail_fn,
                  struct userdata_struct *userdata)
{
  int i;

  /* 
   * Go through and refresh the name for all known ip addresses.
   * Only call the success/fail function on the last one (it should
   * only be done once).
   */

  for( i = 0; i < namerec->data.num_ips; i++)
  {
    if(queue_refresh_name( subrec,
        register_name_response,
        register_name_timeout_response,
        (i == (namerec->data.num_ips - 1)) ? success_fn : NULL,
        (i == (namerec->data.num_ips - 1)) ? fail_fn : NULL,
        (i == (namerec->data.num_ips - 1)) ? userdata : NULL,
        namerec,
        namerec->data.ip[i]) == NULL)
    {
      DEBUG(0,("refresh_name: Failed to send packet trying to refresh name %s\n",
            nmb_namestr(&namerec->name)));
      return True;
    }
  }
  return False;
}
