/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   NBT netbios routines and daemon - version 2

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

#define WINS_LIST "wins.dat"
#define WINS_VERSION 1

extern int DEBUGLEVEL;
extern struct in_addr ipzero;


/****************************************************************************
possibly call the WINS hook external program when a WINS change is made
*****************************************************************************/
static void wins_hook(char *operation, struct name_record *namerec, int ttl)
{
	pstring command;
	char *cmd = lp_wins_hook();
	char *p;
	int i;

	if (!cmd || !*cmd) return;

	for (p=namerec->name.name; *p; p++) {
		if (!(isalnum((int)*p) || strchr("._-",*p))) {
			DEBUG(3,("not calling wins hook for invalid name %s\n", nmb_namestr(&namerec->name)));
			return;
		}
	}
	
	p = command;
	p += slprintf(p, sizeof(command), "%s %s %s %02x %d", 
		      cmd,
		      operation, 
		      namerec->name.name,
		      namerec->name.name_type,
		      ttl);

	for (i=0;i<namerec->data.num_ips;i++) {
		p += slprintf(p, sizeof(command) - (p-command), " %s", inet_ntoa(namerec->data.ip[i]));
	}

	DEBUG(3,("calling wins hook for %s\n", nmb_namestr(&namerec->name)));
	smbrun(command, NULL, False);
}


/****************************************************************************
hash our interfaces and netbios names settings
*****************************************************************************/
static unsigned wins_hash(void)
{
	int i;
	unsigned ret = iface_hash();
	extern char **my_netbios_names;

	for (i=0;my_netbios_names[i];i++)
		ret ^= str_checksum(my_netbios_names[i]);
	
	ret ^= str_checksum(lp_workgroup());

	return ret;
}
	

/****************************************************************************
Determine if this packet should be allocated to the WINS server.
*****************************************************************************/

BOOL packet_is_for_wins_server(struct packet_struct *packet)
{
  struct nmb_packet *nmb = &packet->packet.nmb;

  /* Only unicast packets go to a WINS server. */
  if((wins_server_subnet == NULL) || (nmb->header.nm_flags.bcast == True))
  {
    DEBUG(10, ("packet_is_for_wins_server: failing WINS test #1.\n"));
    return False;
  }

  /* Check for node status requests. */
  if (nmb->question.question_type != QUESTION_TYPE_NB_QUERY)
    return False;

  switch(nmb->header.opcode)
  { 
    /*
     * A WINS server issues WACKS, not receives them.
     */
    case NMB_WACK_OPCODE:
      DEBUG(10, ("packet_is_for_wins_server: failing WINS test #2 (WACK).\n"));
      return False;
    /*
     * A WINS server only processes registration and
     * release requests, not responses.
     */
    case NMB_NAME_REG_OPCODE:
    case NMB_NAME_MULTIHOMED_REG_OPCODE:
    case NMB_NAME_REFRESH_OPCODE_8: /* ambiguity in rfc1002 about which is correct. */
    case NMB_NAME_REFRESH_OPCODE_9: /* WinNT uses 8 by default. */
      if(nmb->header.response)
      {
        DEBUG(10, ("packet_is_for_wins_server: failing WINS test #3 (response = 1).\n"));
        return False;
      }
      break;

    case NMB_NAME_RELEASE_OPCODE:
      if(nmb->header.response)
      {
        DEBUG(10, ("packet_is_for_wins_server: failing WINS test #4 (response = 1).\n"));
        return False;
      }
      break;

    /*
     * Only process unicast name queries with rd = 1.
     */
    case NMB_NAME_QUERY_OPCODE:
      if(!nmb->header.response && !nmb->header.nm_flags.recursion_desired)
      {
        DEBUG(10, ("packet_is_for_wins_server: failing WINS test #5 (response = 1).\n"));
        return False;
      }
      break;
  }

  return True;
}

/****************************************************************************
Utility function to decide what ttl to give a register/refresh request.
*****************************************************************************/

static int get_ttl_from_packet(struct nmb_packet *nmb)
{
  int ttl = nmb->additional->ttl;

  if(ttl < lp_min_wins_ttl() )
    ttl = lp_min_wins_ttl();

  if(ttl > lp_max_wins_ttl() )
    ttl = lp_max_wins_ttl();

  return ttl;
}

/****************************************************************************
Load or create the WINS database.
*****************************************************************************/

BOOL initialise_wins(void)
{
  pstring fname;
  time_t time_now = time(NULL);
  FILE *fp;
  pstring line;

  if(!lp_we_are_a_wins_server())
    return True;

  add_samba_names_to_subnet(wins_server_subnet);

  pstrcpy(fname,lp_lockdir());
  trim_string(fname,NULL,"/");
  pstrcat(fname,"/");
  pstrcat(fname,WINS_LIST);

  if((fp = sys_fopen(fname,"r")) == NULL)
  {
    DEBUG(2,("initialise_wins: Can't open wins database file %s. Error was %s\n",
           fname, strerror(errno) ));
    return True;
  }

  while (!feof(fp))
  {
    pstring name_str, ip_str, ttl_str, nb_flags_str;
    unsigned int num_ips;
    pstring name;
    struct in_addr *ip_list;
    int type = 0;
    int nb_flags;
    int ttl;
    char *ptr;
    char *p;
    BOOL got_token;
    BOOL was_ip;
    int i;
    unsigned hash;
    int version;

    /* Read a line from the wins.dat file. Strips whitespace
       from the beginning and end of the line.
     */
    if (!fgets_slash(line,sizeof(pstring),fp))
      continue;
      
    if (*line == '#')
      continue;

    if (strncmp(line,"VERSION ", 8) == 0) {
	    if (sscanf(line,"VERSION %d %u", &version, &hash) != 2 ||
		version != WINS_VERSION ||
		hash != wins_hash()) {
		    DEBUG(0,("Discarding invalid wins.dat file [%s]\n",line));
		    fclose(fp);
		    return True;
	    }
	    continue;
    }

    ptr = line;

    /* 
     * Now we handle multiple IP addresses per name we need
     * to iterate over the line twice. The first time to
     * determine how many IP addresses there are, the second
     * time to actually parse them into the ip_list array.
     */

    if (!next_token(&ptr,name_str,NULL,sizeof(name_str))) 
    {
      DEBUG(0,("initialise_wins: Failed to parse name when parsing line %s\n", line ));
      continue;
    }

    if (!next_token(&ptr,ttl_str,NULL,sizeof(ttl_str)))
    {
      DEBUG(0,("initialise_wins: Failed to parse time to live when parsing line %s\n", line ));
      continue;
    }

    /*
     * Determine the number of IP addresses per line.
     */
    num_ips = 0;
    do
    {
      got_token = next_token(&ptr,ip_str,NULL,sizeof(ip_str));
      was_ip = False;

      if(got_token && strchr(ip_str, '.'))
      {
        num_ips++;
        was_ip = True;
      }
    } while( got_token && was_ip);

    if(num_ips == 0)
    {
      DEBUG(0,("initialise_wins: Missing IP address when parsing line %s\n", line ));
      continue;
    }

    if(!got_token)
    {
      DEBUG(0,("initialise_wins: Missing nb_flags when parsing line %s\n", line ));
      continue;
    }

    /* Allocate the space for the ip_list. */
    if((ip_list = (struct in_addr *)malloc( num_ips * sizeof(struct in_addr))) == NULL)
    {
      DEBUG(0,("initialise_wins: Malloc fail !\n"));
      return False;
    }
 
    /* Reset and re-parse the line. */
    ptr = line;
    next_token(&ptr,name_str,NULL,sizeof(name_str)); 
    next_token(&ptr,ttl_str,NULL,sizeof(ttl_str));
    for(i = 0; i < num_ips; i++)
    {
      next_token(&ptr, ip_str, NULL, sizeof(ip_str));
      ip_list[i] = *interpret_addr2(ip_str);
    }
    next_token(&ptr,nb_flags_str,NULL, sizeof(nb_flags_str));

    /* 
     * Deal with SELF or REGISTER name encoding. Default is REGISTER
     * for compatibility with old nmbds.
     */

    if(nb_flags_str[strlen(nb_flags_str)-1] == 'S')
    {
      DEBUG(5,("initialise_wins: Ignoring SELF name %s\n", line));
      free((char *)ip_list);
      continue;
    }
      
    if(nb_flags_str[strlen(nb_flags_str)-1] == 'R')
      nb_flags_str[strlen(nb_flags_str)-1] = '\0';
      
    /* Netbios name. # divides the name from the type (hex): netbios#xx */
    pstrcpy(name,name_str);
      
    if((p = strchr(name,'#')) != NULL)
    {
      *p = 0;
      sscanf(p+1,"%x",&type);
    }
      
    /* Decode the netbios flags (hex) and the time-to-live (in seconds). */
    sscanf(nb_flags_str,"%x",&nb_flags);
    sscanf(ttl_str,"%d",&ttl);

    /* add all entries that have 60 seconds or more to live */
    if ((ttl - 60) > time_now || ttl == PERMANENT_TTL)
    {
      if(ttl != PERMANENT_TTL)
        ttl -= time_now;
    
      DEBUG( 4, ("initialise_wins: add name: %s#%02x ttl = %d first IP %s flags = %2x\n",
           name, type, ttl, inet_ntoa(ip_list[0]), nb_flags));

      (void)add_name_to_subnet( wins_server_subnet, name, type, nb_flags, 
                                    ttl, REGISTER_NAME, num_ips, ip_list );

    }
    else
    {
      DEBUG(4, ("initialise_wins: not adding name (ttl problem) %s#%02x ttl = %d first IP %s flags = %2x\n",
             name, type, ttl, inet_ntoa(ip_list[0]), nb_flags));
    }

    free((char *)ip_list);
  } 
    
  fclose(fp);
  return True;
}

/****************************************************************************
Send a WINS WACK (Wait ACKnowledgement) response.
**************************************************************************/

static void send_wins_wack_response(int ttl, struct packet_struct *p)
{
  struct nmb_packet *nmb = &p->packet.nmb;
  unsigned char rdata[2];

  rdata[0] = rdata[1] = 0;

  /* Taken from nmblib.c - we need to send back almost
     identical bytes from the requesting packet header. */

  rdata[0] = (nmb->header.opcode & 0xF) << 3;
  if (nmb->header.nm_flags.authoritative &&
      nmb->header.response) rdata[0] |= 0x4;
  if (nmb->header.nm_flags.trunc) rdata[0] |= 0x2;
  if (nmb->header.nm_flags.recursion_desired) rdata[0] |= 0x1;
  if (nmb->header.nm_flags.recursion_available &&
      nmb->header.response) rdata[1] |= 0x80;
  if (nmb->header.nm_flags.bcast) rdata[1] |= 0x10;

  reply_netbios_packet(p,                             /* Packet to reply to. */
                       0,                             /* Result code. */
                       NMB_WAIT_ACK,                  /* nmbd type code. */
                       NMB_WACK_OPCODE,               /* opcode. */
                       ttl,                           /* ttl. */
                       (char *)rdata,                 /* data to send. */
                       2);                            /* data length. */
}

/****************************************************************************
Send a WINS name registration response.
**************************************************************************/

static void send_wins_name_registration_response(int rcode, int ttl, struct packet_struct *p)
{
  struct nmb_packet *nmb = &p->packet.nmb;
  char rdata[6];

  memcpy(&rdata[0], &nmb->additional->rdata[0], 6);

  reply_netbios_packet(p,                             /* Packet to reply to. */
                       rcode,                         /* Result code. */
                       WINS_REG,                      /* nmbd type code. */
                       NMB_NAME_REG_OPCODE,           /* opcode. */
                       ttl,                           /* ttl. */
                       rdata,                         /* data to send. */
                       6);                            /* data length. */
}

/***********************************************************************
 Deal with a name refresh request to a WINS server.
************************************************************************/

void wins_process_name_refresh_request(struct subnet_record *subrec,
                                            struct packet_struct *p)
{
  struct nmb_packet *nmb = &p->packet.nmb;
  struct nmb_name *question = &nmb->question.question_name;
  BOOL bcast = nmb->header.nm_flags.bcast;
  uint16 nb_flags = get_nb_flags(nmb->additional->rdata);
  BOOL group = (nb_flags & NB_GROUP) ? True : False;
  struct name_record *namerec = NULL;
  int ttl = get_ttl_from_packet(nmb);
  struct in_addr from_ip;

  putip((char *)&from_ip,&nmb->additional->rdata[2]);

  if(bcast)
  {
    /*
     * We should only get unicast name refresh packets here.
     * Anyone trying to refresh broadcast should not be going to a WINS
     * server. Log an error here.
     */

    DEBUG(0,("wins_process_name_refresh_request: broadcast name refresh request \
received for name %s from IP %s on subnet %s. Error - should not be sent to WINS server\n",
          nmb_namestr(question), inet_ntoa(from_ip), subrec->subnet_name));
    return;
  }

  DEBUG(3,("wins_process_name_refresh_request: Name refresh for name %s \
IP %s\n", nmb_namestr(question), inet_ntoa(from_ip) ));

  /* 
   * See if the name already exists.
   */

  namerec = find_name_on_subnet(subrec, question, FIND_ANY_NAME);

  /*
   * If this is a refresh request and the name doesn't exist then
   * treat it like a registration request. This allows us to recover 
   * from errors (tridge)
   */

  if(namerec == NULL)
  {
    DEBUG(3,("wins_process_name_refresh_request: Name refresh for name %s and \
the name does not exist. Treating as registration.\n", nmb_namestr(question) ));
    wins_process_name_registration_request(subrec,p);
    return;
  }

  /*
   * Check that the group bits for the refreshing name and the
   * name in our database match.
   */

  if((namerec != NULL) && ((group && !NAME_GROUP(namerec)) || (!group && NAME_GROUP(namerec))) )
  {
    DEBUG(3,("wins_process_name_refresh_request: Name %s group bit = %s \
does not match group bit in WINS for this name.\n", nmb_namestr(question), group ? "True" : "False" ));
    send_wins_name_registration_response(RFS_ERR, 0, p);
    return;
  }

  /*
   * For a unique name check that the person refreshing the name is one of the registered IP
   * addresses. If not - fail the refresh. Do the same for group names with a type of 0x1c.
   * Just return success for unique 0x1d refreshes. For normal group names update the ttl
   * and return success.
   */

  if((!group || (group && (question->name_type == 0x1c))) && find_ip_in_name_record(namerec, from_ip ))
  {
    /*
     * Update the ttl.
     */
    update_name_ttl(namerec, ttl);
    send_wins_name_registration_response(0, ttl, p);
    wins_hook("refresh", namerec, ttl);
    return;
  }
  else if(group)
  {
    /* 
     * Normal groups are all registered with an IP address of 255.255.255.255 
     * so we can't search for the IP address.
     */
    update_name_ttl(namerec, ttl);
    send_wins_name_registration_response(0, ttl, p);
    return;
  }
  else if(!group && (question->name_type == 0x1d))
  {
    /*
     * Special name type - just pretend the refresh succeeded.
     */
    send_wins_name_registration_response(0, ttl, p);
    return;
  }
  else
  {
    /*
     * Fail the refresh.
     */

    DEBUG(3,("wins_process_name_refresh_request: Name refresh for name %s with IP %s and \
is IP is not known to the name.\n", nmb_namestr(question), inet_ntoa(from_ip) ));
    send_wins_name_registration_response(RFS_ERR, 0, p);
    return;
  }
}

/***********************************************************************
 Deal with a name registration request query success to a client that
 owned the name.

 We have a locked pointer to the original packet stashed away in the
 userdata pointer. The success here is actually a failure as it means
 the client we queried wants to keep the name, so we must return
 a registration failure to the original requestor.
************************************************************************/

static void wins_register_query_success(struct subnet_record *subrec,
                                             struct userdata_struct *userdata,
                                             struct nmb_name *question_name,
                                             struct in_addr ip,
                                             struct res_rec *answers)
{
  struct packet_struct *orig_reg_packet;

  memcpy((char *)&orig_reg_packet, userdata->data, sizeof(struct packet_struct *));

  DEBUG(3,("wins_register_query_success: Original client at IP %s still wants the \
name %s. Rejecting registration request.\n", inet_ntoa(ip), nmb_namestr(question_name) ));

  send_wins_name_registration_response(RFS_ERR, 0, orig_reg_packet);

  orig_reg_packet->locked = False;
  free_packet(orig_reg_packet);
}

/***********************************************************************
 Deal with a name registration request query failure to a client that
 owned the name.

 We have a locked pointer to the original packet stashed away in the
 userdata pointer. The failure here is actually a success as it means
 the client we queried didn't want to keep the name, so we can remove
 the old name record and then successfully add the new name.
************************************************************************/

static void wins_register_query_fail(struct subnet_record *subrec,
                                          struct response_record *rrec,
                                          struct nmb_name *question_name,
                                          int rcode)
{
  struct userdata_struct *userdata = rrec->userdata;
  struct packet_struct *orig_reg_packet;
  struct name_record *namerec = NULL;

  memcpy((char *)&orig_reg_packet, userdata->data, sizeof(struct packet_struct *));

  /*
   * We want to just add the name, as we now know the original owner
   * didn't want it. But we can't just do that as an arbitary
   * amount of time may have taken place between the name query
   * request and this timeout/error response. So we check that
   * the name still exists and is in the same state - if so
   * we remove it and call wins_process_name_registration_request()
   * as we know it will do the right thing now.
   */

  namerec = find_name_on_subnet(subrec, question_name, FIND_ANY_NAME);

  if( (namerec != NULL)
   && (namerec->data.source == REGISTER_NAME)
   && ip_equal(rrec->packet->ip, *namerec->data.ip) )
  {
    remove_name_from_namelist( subrec, namerec);
    namerec = NULL;
  }

  if(namerec == NULL)
    wins_process_name_registration_request(subrec, orig_reg_packet);
  else
    DEBUG(2,("wins_register_query_fail: The state of the WINS database changed between \
querying for name %s in order to replace it and this reply.\n", nmb_namestr(question_name) ));

  orig_reg_packet->locked = False;
  free_packet(orig_reg_packet);
}

/***********************************************************************
 Deal with a name registration request to a WINS server.

 Use the following pseudocode :

 registering_group
     |
     |
     +--------name exists
     |                  |
     |                  |
     |                  +--- existing name is group
     |                  |                      |
     |                  |                      |
     |                  |                      +--- add name (return).
     |                  |
     |                  |
     |                  +--- exiting name is unique
     |                                         |
     |                                         |
     |                                         +--- query existing owner (return).
     |
     |
     +--------name doesn't exist
                        |
                        |
                        +--- add name (return).

 registering_unique
     |
     |
     +--------name exists
     |                  |
     |                  |
     |                  +--- existing name is group 
     |                  |                      |
     |                  |                      |
     |                  |                      +--- fail add (return).
     |                  | 
     |                  |
     |                  +--- exiting name is unique
     |                                         |
     |                                         |
     |                                         +--- query existing owner (return).
     |
     |
     +--------name doesn't exist
                        |
                        |
                        +--- add name (return).

 As can be seen from the above, the two cases may be collapsed onto each
 other with the exception of the case where the name already exists and
 is a group name. This case we handle with an if statement.
 
************************************************************************/

void wins_process_name_registration_request(struct subnet_record *subrec,
                                            struct packet_struct *p)
{
  struct nmb_packet *nmb = &p->packet.nmb;
  struct nmb_name *question = &nmb->question.question_name;
  BOOL bcast = nmb->header.nm_flags.bcast;
  uint16 nb_flags = get_nb_flags(nmb->additional->rdata);
  int ttl = get_ttl_from_packet(nmb);
  struct name_record *namerec = NULL;
  struct in_addr from_ip;
  BOOL registering_group_name = (nb_flags & NB_GROUP) ? True : False;

  putip((char *)&from_ip,&nmb->additional->rdata[2]);

  if(bcast)
  {
    /*
     * We should only get unicast name registration packets here.
     * Anyone trying to register broadcast should not be going to a WINS
     * server. Log an error here.
     */

    DEBUG(0,("wins_process_name_registration_request: broadcast name registration request \
received for name %s from IP %s on subnet %s. Error - should not be sent to WINS server\n",
          nmb_namestr(question), inet_ntoa(from_ip), subrec->subnet_name));
    return;
  }

  DEBUG(3,("wins_process_name_registration_request: %s name registration for name %s \
IP %s\n", registering_group_name ? "Group" : "Unique", nmb_namestr(question), inet_ntoa(from_ip) ));

  /*
   * See if the name already exists.
   */

  namerec = find_name_on_subnet(subrec, question, FIND_ANY_NAME);

  /*
   * Deal with the case where the name found was a dns entry.
   * Remove it as we now have a NetBIOS client registering the
   * name.
   */

  if( (namerec != NULL)
   && ( (namerec->data.source == DNS_NAME)
     || (namerec->data.source == DNSFAIL_NAME) ) )
  {
    DEBUG(5,("wins_process_name_registration_request: Name (%s) in WINS was \
a dns lookup - removing it.\n", nmb_namestr(question) ));
    remove_name_from_namelist( subrec, namerec );
    namerec = NULL;
  }

  /*
   * Reject if the name exists and is not a REGISTER_NAME.
   * (ie. Don't allow any static names to be overwritten.
   */

  if((namerec != NULL) && (namerec->data.source != REGISTER_NAME))
  {
    DEBUG( 3, ( "wins_process_name_registration_request: Attempt \
to register name %s. Name already exists in WINS with source type %d.\n",
                nmb_namestr(question), namerec->data.source ));
    send_wins_name_registration_response(RFS_ERR, 0, p);
    return;
  }

  /*
   * Special policy decisions based on MS documentation.
   * 1). All group names (except names ending in 0x1c) are added as 255.255.255.255.
   * 2). All unique names ending in 0x1d are ignored, although a positive response is sent.
   */

  /*
   * A group name is always added as the local broadcast address, except
   * for group names ending in 0x1c.
   * Group names with type 0x1c are registered with individual IP addresses.
   */

  if(registering_group_name && (question->name_type != 0x1c))
    from_ip = *interpret_addr2("255.255.255.255");

  /*
   * Ignore all attempts to register a unique 0x1d name, although return success.
   */

  if(!registering_group_name && (question->name_type == 0x1d))
  {
    DEBUG(3,("wins_process_name_registration_request: Ignoring request \
to register name %s from IP %s.\n", nmb_namestr(question), inet_ntoa(p->ip) ));
    send_wins_name_registration_response(0, ttl, p);
    return;
  }

  /*
   * Next two cases are the 'if statement' mentioned above.
   */

  if((namerec != NULL) && NAME_GROUP(namerec))
  {
    if(registering_group_name)
    {
      /*
       * If we are adding a group name, the name exists and is also a group entry just add this
       * IP address to it and update the ttl.
       */

      DEBUG(3,("wins_process_name_registration_request: Adding IP %s to group name %s.\n",
            inet_ntoa(from_ip), nmb_namestr(question) ));
      /* 
       * Check the ip address is not already in the group.
       */
      if(!find_ip_in_name_record(namerec, from_ip))
        add_ip_to_name_record(namerec, from_ip);
      update_name_ttl(namerec, ttl);
      send_wins_name_registration_response(0, ttl, p);
      return;
    }
    else
    {
      /*
       * If we are adding a unique name, the name exists in the WINS db 
       * and is a group name then reject the registration.
       */

      DEBUG(3,("wins_process_name_registration_request: Attempt to register name %s. Name \
already exists in WINS as a GROUP name.\n", nmb_namestr(question) ));
      send_wins_name_registration_response(RFS_ERR, 0, p);
      return;
    } 
  }

  /*
   * From here on down we know that if the name exists in the WINS db it is
   * a unique name, not a group name.
   */

  /* 
   * If the name exists and is one of our names then check the
   * registering IP address. If it's not one of ours then automatically
   * reject without doing the query - we know we will reject it.
   */

  if((namerec != NULL) && (is_myname(namerec->name.name)) )
  {
    if(!ismyip(from_ip))
    {
      DEBUG(3,("wins_process_name_registration_request: Attempt to register name %s. Name \
is one of our (WINS server) names. Denying registration.\n", nmb_namestr(question) ));
      send_wins_name_registration_response(RFS_ERR, 0, p);
      return;
    }
    else
    {
      /*
       * It's one of our names and one of our IP's - update the ttl.
       */
      update_name_ttl(namerec, ttl);
      send_wins_name_registration_response(0, ttl, p);
      wins_hook("refresh", namerec, ttl);
      return;
    }
  }

  /*
   * If the name exists and it is a unique registration and the registering IP 
   * is the same as the the (single) already registered IP then just update the ttl.
   */

  if( !registering_group_name
   && (namerec != NULL)
   && (namerec->data.num_ips == 1)
   && ip_equal( namerec->data.ip[0], from_ip ) )
  {
    update_name_ttl( namerec, ttl );
    send_wins_name_registration_response( 0, ttl, p );
    wins_hook("refresh", namerec, ttl);
    return;
  }

  /*
   * Finally if the name exists do a query to the registering machine 
   * to see if they still claim to have the name.
   */

  if( namerec != NULL )
  {
    long *ud[(sizeof(struct userdata_struct) + sizeof(struct packet_struct *))/sizeof(long *) + 1];
    struct userdata_struct *userdata = (struct userdata_struct *)ud;

    /*
     * First send a WACK to the registering machine.
     */

    send_wins_wack_response(60, p);

    /*
     * When the reply comes back we need the original packet.
     * Lock this so it won't be freed and then put it into
     * the userdata structure.
     */

    p->locked = True;

    userdata = (struct userdata_struct *)ud;

    userdata->copy_fn = NULL;
    userdata->free_fn = NULL;
    userdata->userdata_len = sizeof(struct packet_struct *);
    memcpy(userdata->data, (char *)&p, sizeof(struct packet_struct *) );

    /*
     * Use the new call to send a query directly to an IP address.
     * This sends the query directly to the IP address, and ensures
     * the recursion desired flag is not set (you were right Luke :-).
     * This function should *only* be called from the WINS server
     * code. JRA.
     */

    query_name_from_wins_server( *namerec->data.ip,
                                  question->name,
                                  question->name_type, 
                                  wins_register_query_success,
                                  wins_register_query_fail,
                                  userdata );
    return;
  }

  /*
   * Name did not exist - add it.
   */

  (void)add_name_to_subnet( subrec, question->name, question->name_type,
                            nb_flags, ttl, REGISTER_NAME, 1, &from_ip );
  if ((namerec = find_name_on_subnet(subrec, question, FIND_ANY_NAME))) {
	  wins_hook("add", namerec, ttl);
  }

  send_wins_name_registration_response(0, ttl, p);
}

/***********************************************************************
 Deal with a mutihomed name query success to the machine that
 requested the multihomed name registration.

 We have a locked pointer to the original packet stashed away in the
 userdata pointer.
************************************************************************/

static void wins_multihomed_register_query_success(struct subnet_record *subrec,
                                             struct userdata_struct *userdata,
                                             struct nmb_name *question_name,
                                             struct in_addr ip,
                                             struct res_rec *answers)
{
  struct packet_struct *orig_reg_packet;
  struct nmb_packet *nmb;
  struct name_record *namerec = NULL;
  struct in_addr from_ip;
  int ttl;

  memcpy((char *)&orig_reg_packet, userdata->data, sizeof(struct packet_struct *));

  nmb = &orig_reg_packet->packet.nmb;

  putip((char *)&from_ip,&nmb->additional->rdata[2]);
  ttl = get_ttl_from_packet(nmb);

  /*
   * We want to just add the new IP, as we now know the requesting
   * machine claims to own it. But we can't just do that as an arbitary
   * amount of time may have taken place between the name query
   * request and this response. So we check that
   * the name still exists and is in the same state - if so
   * we just add the extra IP and update the ttl.
   */

  namerec = find_name_on_subnet(subrec, question_name, FIND_ANY_NAME);

  if( (namerec == NULL) || (namerec->data.source != REGISTER_NAME) )
  {
    DEBUG(3,("wins_multihomed_register_query_success: name %s is not in the correct state to add \
a subsequent IP addess.\n", nmb_namestr(question_name) ));
    send_wins_name_registration_response(RFS_ERR, 0, orig_reg_packet);

    orig_reg_packet->locked = False;
    free_packet(orig_reg_packet);

    return;
  }

  if(!find_ip_in_name_record(namerec, from_ip))
    add_ip_to_name_record(namerec, from_ip);
  update_name_ttl(namerec, ttl);
  send_wins_name_registration_response(0, ttl, orig_reg_packet);
  wins_hook("add", namerec, ttl);

  orig_reg_packet->locked = False;
  free_packet(orig_reg_packet);
}

/***********************************************************************
 Deal with a name registration request query failure to a client that
 owned the name.

 We have a locked pointer to the original packet stashed away in the
 userdata pointer.
************************************************************************/

static void wins_multihomed_register_query_fail(struct subnet_record *subrec,
                                          struct response_record *rrec,
                                          struct nmb_name *question_name,
                                          int rcode)
{
  struct userdata_struct *userdata = rrec->userdata;
  struct packet_struct *orig_reg_packet;

  memcpy((char *)&orig_reg_packet, userdata->data, sizeof(struct packet_struct *));

  DEBUG(3,("wins_multihomed_register_query_fail: Registering machine at IP %s failed to answer \
query successfully for name %s.\n", inet_ntoa(orig_reg_packet->ip), nmb_namestr(question_name) ));
  send_wins_name_registration_response(RFS_ERR, 0, orig_reg_packet);

  orig_reg_packet->locked = False;
  free_packet(orig_reg_packet);
  return;
}

/***********************************************************************
 Deal with a multihomed name registration request to a WINS server.
 These cannot be group name registrations.
***********************************************************************/

void wins_process_multihomed_name_registration_request( struct subnet_record *subrec,
                                                        struct packet_struct *p)
{
  struct nmb_packet *nmb = &p->packet.nmb;
  struct nmb_name *question = &nmb->question.question_name;
  BOOL bcast = nmb->header.nm_flags.bcast;
  uint16 nb_flags = get_nb_flags(nmb->additional->rdata);
  int ttl = get_ttl_from_packet(nmb);
  struct name_record *namerec = NULL;
  struct in_addr from_ip;
  BOOL group = (nb_flags & NB_GROUP) ? True : False;;

  putip((char *)&from_ip,&nmb->additional->rdata[2]);

  if(bcast)
  {
    /*
     * We should only get unicast name registration packets here.
     * Anyone trying to register broadcast should not be going to a WINS
     * server. Log an error here.
     */

    DEBUG(0,("wins_process_multihomed_name_registration_request: broadcast name registration request \
received for name %s from IP %s on subnet %s. Error - should not be sent to WINS server\n",
          nmb_namestr(question), inet_ntoa(from_ip), subrec->subnet_name));
    return;
  }

  /*
   * Only unique names should be registered multihomed.
   */

  if(group)
  {
    DEBUG(0,("wins_process_multihomed_name_registration_request: group name registration request \
received for name %s from IP %s on subnet %s. Errror - group names should not be multihomed.\n",
          nmb_namestr(question), inet_ntoa(from_ip), subrec->subnet_name));
    return;
  }

  DEBUG(3,("wins_process_multihomed_name_registration_request: name registration for name %s \
IP %s\n", nmb_namestr(question), inet_ntoa(from_ip) ));

  /*
   * Deal with policy regarding 0x1d names.
   */

  if(question->name_type == 0x1d)
  {
    DEBUG(3,("wins_process_multihomed_name_registration_request: Ignoring request \
to register name %s from IP %s.", nmb_namestr(question), inet_ntoa(p->ip) ));
    send_wins_name_registration_response(0, ttl, p);  
    return;
  }

  /*
   * See if the name already exists.
   */

  namerec = find_name_on_subnet(subrec, question, FIND_ANY_NAME);

  /*
   * Deal with the case where the name found was a dns entry.
   * Remove it as we now have a NetBIOS client registering the
   * name.
   */

  if( (namerec != NULL)
   && ( (namerec->data.source == DNS_NAME)
     || (namerec->data.source == DNSFAIL_NAME) ) )
  {
    DEBUG(5,("wins_process_multihomed_name_registration_request: Name (%s) in WINS was a dns lookup \
- removing it.\n", nmb_namestr(question) ));
    remove_name_from_namelist( subrec, namerec);
    namerec = NULL;
  }

  /*
   * Reject if the name exists and is not a REGISTER_NAME.
   * (ie. Don't allow any static names to be overwritten.
   */

  if( (namerec != NULL) && (namerec->data.source != REGISTER_NAME) )
  {
    DEBUG( 3, ( "wins_process_multihomed_name_registration_request: Attempt \
to register name %s. Name already exists in WINS with source type %d.\n",
    nmb_namestr(question), namerec->data.source ));
    send_wins_name_registration_response(RFS_ERR, 0, p);
    return;
  }

  /*
   * Reject if the name exists and is a GROUP name.
   */

  if((namerec != NULL) && NAME_GROUP(namerec))
  {
    DEBUG(3,("wins_process_multihomed_name_registration_request: Attempt to register name %s. Name \
already exists in WINS as a GROUP name.\n", nmb_namestr(question) ));
    send_wins_name_registration_response(RFS_ERR, 0, p);
    return;
  } 

  /*
   * From here on down we know that if the name exists in the WINS db it is
   * a unique name, not a group name.
   */

  /*
   * If the name exists and is one of our names then check the
   * registering IP address. If it's not one of ours then automatically
   * reject without doing the query - we know we will reject it.
   */

  if((namerec != NULL) && (is_myname(namerec->name.name)) )
  {
    if(!ismyip(from_ip))
    {
      DEBUG(3,("wins_process_multihomed_name_registration_request: Attempt to register name %s. Name \
is one of our (WINS server) names. Denying registration.\n", nmb_namestr(question) ));
      send_wins_name_registration_response(RFS_ERR, 0, p);
      return;
    }
    else
    {
      /*
       * It's one of our names and one of our IP's. Ensure the IP is in the record and
       *  update the ttl.
       */
	    if(!find_ip_in_name_record(namerec, from_ip)) {
		    add_ip_to_name_record(namerec, from_ip);
		    wins_hook("add", namerec, ttl);
	    } else {
		    wins_hook("refresh", namerec, ttl);
	    }

	    update_name_ttl(namerec, ttl);
	    send_wins_name_registration_response(0, ttl, p);
	    return;
    }
  }

  /*
   * If the name exists check if the IP address is already registered
   * to that name. If so then update the ttl and reply success.
   */

  if((namerec != NULL) && find_ip_in_name_record(namerec, from_ip))
  {
    update_name_ttl(namerec, ttl);
    send_wins_name_registration_response(0, ttl, p);
    wins_hook("refresh", namerec, ttl);
    return;
  }

  /*
   * If the name exists do a query to the owner
   * to see if they still want the name.
   */

  if(namerec != NULL)
  {
    long *ud[(sizeof(struct userdata_struct) + sizeof(struct packet_struct *))/sizeof(long *) + 1];
    struct userdata_struct *userdata = (struct userdata_struct *)ud;

    /*
     * First send a WACK to the registering machine.
     */

    send_wins_wack_response(60, p);

    /*
     * When the reply comes back we need the original packet.
     * Lock this so it won't be freed and then put it into
     * the userdata structure.
     */

    p->locked = True;

    userdata = (struct userdata_struct *)ud;

    userdata->copy_fn = NULL;
    userdata->free_fn = NULL;
    userdata->userdata_len = sizeof(struct packet_struct *);
    memcpy(userdata->data, (char *)&p, sizeof(struct packet_struct *) );

    /* 
     * Use the new call to send a query directly to an IP address.
     * This sends the query directly to the IP address, and ensures
     * the recursion desired flag is not set (you were right Luke :-).
     * This function should *only* be called from the WINS server
     * code. JRA.
     * Note that this packet is sent to the current owner of the name,
     * not the person who sent the packet.
     */

    query_name_from_wins_server( namerec->data.ip[0],
                                 question->name,
                                 question->name_type, 
                                 wins_multihomed_register_query_success,
                                 wins_multihomed_register_query_fail,
                                 userdata );

    return;
  }

  /*
   * Name did not exist - add it.
   */

  (void)add_name_to_subnet( subrec, question->name, question->name_type,
                            nb_flags, ttl, REGISTER_NAME, 1, &from_ip );

  if ((namerec = find_name_on_subnet(subrec, question, FIND_ANY_NAME))) {
	  wins_hook("add", namerec, ttl);
  }

  send_wins_name_registration_response(0, ttl, p);
}

/***********************************************************************
 Deal with the special name query for *<1b>.
***********************************************************************/
   
static void process_wins_dmb_query_request(struct subnet_record *subrec,  
                                           struct packet_struct *p)
{  
  struct name_record *namerec = NULL;
  char *prdata;
  int num_ips;

  /*
   * Go through all the names in the WINS db looking for those
   * ending in <1b>. Use this to calculate the number of IP
   * addresses we need to return.
   */

  num_ips = 0;
  for( namerec = (struct name_record *)ubi_trFirst( subrec->namelist );
       namerec;
       namerec = (struct name_record *)ubi_trNext( namerec ) )
  {
    if( namerec->name.name_type == 0x1b )
      num_ips += namerec->data.num_ips;
  }

  if(num_ips == 0)
  {
    /*
     * There are no 0x1b names registered. Return name query fail.
     */
    send_wins_name_query_response(NAM_ERR, p, NULL);
    return;
  }

  if((prdata = (char *)malloc( num_ips * 6 )) == NULL)
  {
    DEBUG(0,("process_wins_dmb_query_request: Malloc fail !.\n"));
    return;
  }

  /*
   * Go through all the names again in the WINS db looking for those
   * ending in <1b>. Add their IP addresses into the list we will
   * return.
   */ 

  num_ips = 0;
  for( namerec = (struct name_record *)ubi_trFirst( subrec->namelist );
       namerec;
       namerec = (struct name_record *)ubi_trNext( namerec ) )
  {
    if(namerec->name.name_type == 0x1b)
    {
      int i;
      for(i = 0; i < namerec->data.num_ips; i++)
      {
        set_nb_flags(&prdata[num_ips * 6],namerec->data.nb_flags);
        putip((char *)&prdata[(num_ips * 6) + 2], &namerec->data.ip[i]);
        num_ips++;
      }
    }
  }

  /*
   * Send back the reply containing the IP list.
   */

  reply_netbios_packet(p,                             /* Packet to reply to. */
                       0,                             /* Result code. */
                       WINS_QUERY,                    /* nmbd type code. */
                       NMB_NAME_QUERY_OPCODE,         /* opcode. */
                       lp_min_wins_ttl(),             /* ttl. */
                       prdata,                        /* data to send. */
                       num_ips*6);                    /* data length. */

  free(prdata);
}

/****************************************************************************
Send a WINS name query response.
**************************************************************************/

void send_wins_name_query_response(int rcode, struct packet_struct *p, 
                                          struct name_record *namerec)
{
  char rdata[6];
  char *prdata = rdata;
  int reply_data_len = 0;
  int ttl = 0;
  int i;

  memset(rdata,'\0',6);

  if(rcode == 0)
  {
    ttl = (namerec->data.death_time != PERMANENT_TTL) ?
             namerec->data.death_time - p->timestamp : lp_max_wins_ttl();

    /* Copy all known ip addresses into the return data. */
    /* Optimise for the common case of one IP address so
       we don't need a malloc. */

    if( namerec->data.num_ips == 1 )
      prdata = rdata;
    else
    {
      if((prdata = (char *)malloc( namerec->data.num_ips * 6 )) == NULL)
      {
        DEBUG(0,("send_wins_name_query_response: malloc fail !\n"));
        return;
      }
    }

    for(i = 0; i < namerec->data.num_ips; i++)
    {
      set_nb_flags(&prdata[i*6],namerec->data.nb_flags);
      putip((char *)&prdata[2+(i*6)], &namerec->data.ip[i]);
    }

    sort_query_replies(prdata, i, p->ip);

    reply_data_len = namerec->data.num_ips * 6;
  }

  reply_netbios_packet(p,                             /* Packet to reply to. */
                       rcode,                         /* Result code. */
                       WINS_QUERY,                    /* nmbd type code. */
                       NMB_NAME_QUERY_OPCODE,         /* opcode. */
                       ttl,                           /* ttl. */
                       prdata,                        /* data to send. */
                       reply_data_len);               /* data length. */

  if((prdata != rdata) && (prdata != NULL))
    free(prdata);
}

/***********************************************************************
 Deal with a name query.
***********************************************************************/

void wins_process_name_query_request(struct subnet_record *subrec, 
                                     struct packet_struct *p)
{
  struct nmb_packet *nmb = &p->packet.nmb;
  struct nmb_name *question = &nmb->question.question_name;
  struct name_record *namerec = NULL;

  DEBUG(3,("wins_process_name_query: name query for name %s from IP %s\n", 
            nmb_namestr(question), inet_ntoa(p->ip) ));

  /*
   * Special name code. If the queried name is *<1b> then search
   * the entire WINS database and return a list of all the IP addresses
   * registered to any <1b> name. This is to allow domain master browsers
   * to discover other domains that may not have a presence on their subnet.
   */

  if(strequal( question->name, "*") && (question->name_type == 0x1b))
  {
    process_wins_dmb_query_request( subrec, p);
    return;
  }

  namerec = find_name_on_subnet(subrec, question, FIND_ANY_NAME);

  if(namerec != NULL)
  {
    /* 
     * If it's a DNSFAIL_NAME then reply name not found.
     */

    if( namerec->data.source == DNSFAIL_NAME )
    {
      DEBUG(3,("wins_process_name_query: name query for name %s returning DNS fail.\n",
             nmb_namestr(question) ));
      send_wins_name_query_response(NAM_ERR, p, namerec);
      return;
    }

    /*
     * If the name has expired then reply name not found.
     */

    if( (namerec->data.death_time != PERMANENT_TTL)
     && (namerec->data.death_time < p->timestamp) )
    {
      DEBUG(3,("wins_process_name_query: name query for name %s - name expired. Returning fail.\n",
                nmb_namestr(question) ));
      send_wins_name_query_response(NAM_ERR, p, namerec);
      return;
    }

    DEBUG(3,("wins_process_name_query: name query for name %s returning first IP %s.\n",
           nmb_namestr(question), inet_ntoa(namerec->data.ip[0]) ));

    send_wins_name_query_response(0, p, namerec);
    return;
  }

  /* 
   * Name not found in WINS - try a dns query if it's a 0x20 name.
   */

  if(lp_dns_proxy() && 
     ((question->name_type == 0x20) || question->name_type == 0))
  {

    DEBUG(3,("wins_process_name_query: name query for name %s not found - doing dns lookup.\n",
              nmb_namestr(question) ));

    queue_dns_query(p, question, &namerec);
    return;
  }

  /*
   * Name not found - return error.
   */

  send_wins_name_query_response(NAM_ERR, p, NULL);
}

/****************************************************************************
Send a WINS name release response.
**************************************************************************/

static void send_wins_name_release_response(int rcode, struct packet_struct *p)
{
  struct nmb_packet *nmb = &p->packet.nmb;
  char rdata[6];

  memcpy(&rdata[0], &nmb->additional->rdata[0], 6);

  reply_netbios_packet(p,                            /* Packet to reply to. */
                       rcode,                        /* Result code. */
                       NMB_REL,                      /* nmbd type code. */
                       NMB_NAME_RELEASE_OPCODE,      /* opcode. */
                       0,                            /* ttl. */
                       rdata,                        /* data to send. */
                       6);                           /* data length. */
}

/***********************************************************************
 Deal with a name release.
***********************************************************************/

void wins_process_name_release_request(struct subnet_record *subrec,
                                       struct packet_struct *p)
{
  struct nmb_packet *nmb = &p->packet.nmb;
  struct nmb_name *question = &nmb->question.question_name;
  BOOL bcast = nmb->header.nm_flags.bcast;
  uint16 nb_flags = get_nb_flags(nmb->additional->rdata);
  struct name_record *namerec = NULL;
  struct in_addr from_ip;
  BOOL releasing_group_name = (nb_flags & NB_GROUP) ? True : False;;

  putip((char *)&from_ip,&nmb->additional->rdata[2]);

  if(bcast)
  {
    /*
     * We should only get unicast name registration packets here.
     * Anyone trying to register broadcast should not be going to a WINS
     * server. Log an error here.
     */

    DEBUG(0,("wins_process_name_release_request: broadcast name registration request \
received for name %s from IP %s on subnet %s. Error - should not be sent to WINS server\n",
          nmb_namestr(question), inet_ntoa(from_ip), subrec->subnet_name));
    return;
  }
  
  DEBUG(3,("wins_process_name_release_request: %s name release for name %s \
IP %s\n", releasing_group_name ? "Group" : "Unique", nmb_namestr(question), inet_ntoa(from_ip) ));
    
  /*
   * Deal with policy regarding 0x1d names.
   */

  if(!releasing_group_name && (question->name_type == 0x1d))
  {
    DEBUG(3,("wins_process_name_release_request: Ignoring request \
to release name %s from IP %s.", nmb_namestr(question), inet_ntoa(p->ip) ));
    send_wins_name_release_response(0, p);
    return;
  }

  /*
   * See if the name already exists.
   */
    
  namerec = find_name_on_subnet(subrec, question, FIND_ANY_NAME);

  if( (namerec == NULL)
   || ((namerec != NULL) && (namerec->data.source != REGISTER_NAME)) )
  {
    send_wins_name_release_response(NAM_ERR, p);
    return;
  }

  /* 
   * Check that the sending machine has permission to release this name.
   * If it's a group name not ending in 0x1c then just say yes and let
   * the group time out.
   */

  if(releasing_group_name && (question->name_type != 0x1c))
  {
    send_wins_name_release_response(0, p);
    return;
  }

  /* 
   * Check that the releasing node is on the list of IP addresses
   * for this name. Disallow the release if not.
   */

  if(!find_ip_in_name_record(namerec, from_ip))
  {
    DEBUG(3,("wins_process_name_release_request: Refusing request to \
release name %s as IP %s is not one of the known IP's for this name.\n",
           nmb_namestr(question), inet_ntoa(from_ip) ));
    send_wins_name_release_response(NAM_ERR, p);
    return;
  }    

  /* 
   * Release the name and then remove the IP from the known list.
   */

  send_wins_name_release_response(0, p);
  remove_ip_from_name_record(namerec, from_ip);

  wins_hook("delete", namerec, 0);

  /* 
   * Remove the name entirely if no IP addresses left.
   */
  if (namerec->data.num_ips == 0)
    remove_name_from_namelist(subrec, namerec);

}

/*******************************************************************
 WINS time dependent processing.
******************************************************************/

void initiate_wins_processing(time_t t)
{
  static time_t lasttime = 0;

  if (!lasttime)
    lasttime = t;
  if (t - lasttime < 20)
    return;

  lasttime = t;

  if(!lp_we_are_a_wins_server())
    return;

  expire_names_on_subnet(wins_server_subnet, t);

  if(wins_server_subnet->namelist_changed)
    wins_write_database(True);

  wins_server_subnet->namelist_changed = False;
}

/*******************************************************************
 Write out the current WINS database.
******************************************************************/
void wins_write_database(BOOL background)
{
  struct name_record *namerec;
  pstring fname, fnamenew;

  FILE *fp;
   
  if(!lp_we_are_a_wins_server())
    return;

  /* we will do the writing in a child process to ensure that the parent
     doesn't block while this is done */
  if (background) {
	  CatchChild();
	  if (fork()) {
		  return;
	  }
  }

  slprintf(fname,sizeof(fname),"%s/%s", lp_lockdir(), WINS_LIST);
  all_string_sub(fname,"//", "/", 0);
  slprintf(fnamenew,sizeof(fnamenew),"%s.%u", fname, (unsigned int)getpid());

  if((fp = sys_fopen(fnamenew,"w")) == NULL)
  {
    DEBUG(0,("wins_write_database: Can't open %s. Error was %s\n", fnamenew, strerror(errno)));
    if (background) {
	    _exit(0);
    }
    return;
  }

  DEBUG(4,("wins_write_database: Dump of WINS name list.\n"));

  fprintf(fp,"VERSION %d %u\n", WINS_VERSION, wins_hash());
 
  for( namerec 
           = (struct name_record *)ubi_trFirst( wins_server_subnet->namelist );
       namerec;
       namerec = (struct name_record *)ubi_trNext( namerec ) )
  {
    int i;
    struct tm *tm;

    DEBUGADD(4,("%-19s ", nmb_namestr(&namerec->name) ));

    if( namerec->data.death_time != PERMANENT_TTL )
    {
      char *ts, *nl;

      tm = LocalTime(&namerec->data.death_time);
      ts = asctime(tm);
      nl = strrchr( ts, '\n' );
      if( NULL != nl )
        *nl = '\0';
      DEBUGADD(4,("TTL = %s  ", ts ));
    }
    else
      DEBUGADD(4,("TTL = PERMANENT                 "));

    for (i = 0; i < namerec->data.num_ips; i++)
      DEBUGADD(4,("%15s ", inet_ntoa(namerec->data.ip[i]) ));
    DEBUGADD(4,("%2x\n", namerec->data.nb_flags ));

    if( namerec->data.source == REGISTER_NAME )
    {
      fprintf(fp, "\"%s#%02x\" %d ",
	      namerec->name.name,namerec->name.name_type, /* Ignore scope. */
	      (int)namerec->data.death_time);

      for (i = 0; i < namerec->data.num_ips; i++)
        fprintf( fp, "%s ", inet_ntoa( namerec->data.ip[i] ) );
      fprintf( fp, "%2xR\n", namerec->data.nb_flags );
    }
  }
  
  fclose(fp);
  chmod(fnamenew,0644);
  unlink(fname);
  rename(fnamenew,fname);
  if (background) {
	  _exit(0);
  }
}
