/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   name query routines
   Copyright (C) Andrew Tridgell 1994-1998
   
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

/* nmbd.c sets this to True. */
BOOL global_in_nmbd = False;

/****************************************************************************
 Interpret a node status response.
****************************************************************************/

static void _interpret_node_status(char *p, char *master,char *rname)
{
  int numnames = CVAL(p,0);
  DEBUG(1,("received %d names\n",numnames));

  if (rname) *rname = 0;
  if (master) *master = 0;

  p += 1;
  while (numnames--) {
    char qname[17];
    int type;
    fstring flags;
    int i;
    *flags = 0;
    StrnCpy(qname,p,15);
    type = CVAL(p,15);
    p += 16;

    fstrcat(flags, (p[0] & 0x80) ? "<GROUP> " : "        ");
    if ((p[0] & 0x60) == 0x00) fstrcat(flags,"B ");
    if ((p[0] & 0x60) == 0x20) fstrcat(flags,"P ");
    if ((p[0] & 0x60) == 0x40) fstrcat(flags,"M ");
    if ((p[0] & 0x60) == 0x60) fstrcat(flags,"H ");
    if (p[0] & 0x10) fstrcat(flags,"<DEREGISTERING> ");
    if (p[0] & 0x08) fstrcat(flags,"<CONFLICT> ");
    if (p[0] & 0x04) fstrcat(flags,"<ACTIVE> ");
    if (p[0] & 0x02) fstrcat(flags,"<PERMANENT> ");

    if (master && !*master && type == 0x1d) {
      StrnCpy(master,qname,15);
      trim_string(master,NULL," ");
    }

    if (rname && !*rname && type == 0x20 && !(p[0]&0x80)) {
      StrnCpy(rname,qname,15);
      trim_string(rname,NULL," ");
    }
      
    for (i = strlen( qname) ; --i >= 0 ; ) {
      if (!isprint((int)qname[i])) qname[i] = '.';
    }
    DEBUG(1,("\t%-15s <%02x> - %s\n",qname,type,flags));
    p+=2;
  }

  DEBUG(1,("num_good_sends=%d num_good_receives=%d\n",
	       IVAL(p,20),IVAL(p,24)));
}

/****************************************************************************
 Internal function handling a netbios name status query on a host.
**************************************************************************/

static BOOL internal_name_status(int fd,char *name,int name_type,BOOL recurse,
		 struct in_addr to_ip,char *master,char *rname, BOOL verbose,
         void (*fn_interpret_node_status)(char *, char *,char *),
		 void (*fn)(struct packet_struct *))
{
  BOOL found=False;
  int retries = 2;
  int retry_time = 5000;
  struct timeval tval;
  struct packet_struct p;
  struct packet_struct *p2;
  struct nmb_packet *nmb = &p.packet.nmb;
  static int name_trn_id = 0;

  memset((char *)&p,'\0',sizeof(p));

  if (!name_trn_id) name_trn_id = ((unsigned)time(NULL)%(unsigned)0x7FFF) + 
    ((unsigned)getpid()%(unsigned)100);
  name_trn_id = (name_trn_id+1) % (unsigned)0x7FFF;

  nmb->header.name_trn_id = name_trn_id;
  nmb->header.opcode = 0;
  nmb->header.response = False;
  nmb->header.nm_flags.bcast = False;
  nmb->header.nm_flags.recursion_available = False;
  nmb->header.nm_flags.recursion_desired = False;
  nmb->header.nm_flags.trunc = False;
  nmb->header.nm_flags.authoritative = False;
  nmb->header.rcode = 0;
  nmb->header.qdcount = 1;
  nmb->header.ancount = 0;
  nmb->header.nscount = 0;
  nmb->header.arcount = 0;

  make_nmb_name(&nmb->question.question_name,name,name_type);

  nmb->question.question_type = 0x21;
  nmb->question.question_class = 0x1;

  p.ip = to_ip;
  p.port = NMB_PORT;
  p.fd = fd;
  p.timestamp = time(NULL);
  p.packet_type = NMB_PACKET;

  GetTimeOfDay(&tval);

  if (!send_packet(&p)) 
    return(False);

  retries--;

  while (1) {
    struct timeval tval2;
    GetTimeOfDay(&tval2);
    if (TvalDiff(&tval,&tval2) > retry_time) {
      if (!retries)
        break;
      if (!found && !send_packet(&p))
        return False;
      GetTimeOfDay(&tval);
      retries--;
    }

    if ((p2=receive_packet(fd,NMB_PACKET,90))) {     
      struct nmb_packet *nmb2 = &p2->packet.nmb;
      debug_nmb_packet(p2);

      if (nmb->header.name_trn_id != nmb2->header.name_trn_id ||
             !nmb2->header.response) {
        /* its not for us - maybe deal with it later */
        if (fn) 
          fn(p2);
        else
          free_packet(p2);
        continue;
      }
	  
	  if (nmb2->header.opcode != 0 ||
	      nmb2->header.nm_flags.bcast ||
	      nmb2->header.rcode ||
	      !nmb2->header.ancount ||
	      nmb2->answers->rr_type != 0x21) {
	    /* XXXX what do we do with this? could be a redirect, but
	       we'll discard it for the moment */
	    free_packet(p2);
	    continue;
	  }

      if(fn_interpret_node_status)
	    (*fn_interpret_node_status)(&nmb2->answers->rdata[0],master,rname);
	  free_packet(p2);
	  return(True);
	}
  }

  if(verbose)
    DEBUG(0,("No status response (this is not unusual)\n"));

  return(False);
}

/****************************************************************************
 Do a netbios name status query on a host.
 The "master" parameter is a hack used for finding workgroups.
**************************************************************************/

BOOL name_status(int fd,char *name,int name_type,BOOL recurse,
		 struct in_addr to_ip,char *master,char *rname,
		 void (*fn)(struct packet_struct *))
{
  return internal_name_status(fd,name,name_type,recurse,
		 to_ip,master,rname,True,
         _interpret_node_status, fn);
}

/****************************************************************************
 Do a netbios name query to find someones IP.
 Returns an array of IP addresses or NULL if none.
 *count will be set to the number of addresses returned.
****************************************************************************/

struct in_addr *name_query(int fd,const char *name,int name_type, BOOL bcast,BOOL recurse,
         struct in_addr to_ip, int *count, void (*fn)(struct packet_struct *))
{
  BOOL found=False;
  int i, retries = 3;
  int retry_time = bcast?250:2000;
  struct timeval tval;
  struct packet_struct p;
  struct packet_struct *p2;
  struct nmb_packet *nmb = &p.packet.nmb;
  static int name_trn_id = 0;
  struct in_addr *ip_list = NULL;

  memset((char *)&p,'\0',sizeof(p));
  (*count) = 0;

  if (!name_trn_id) name_trn_id = ((unsigned)time(NULL)%(unsigned)0x7FFF) + 
    ((unsigned)getpid()%(unsigned)100);
  name_trn_id = (name_trn_id+1) % (unsigned)0x7FFF;

  nmb->header.name_trn_id = name_trn_id;
  nmb->header.opcode = 0;
  nmb->header.response = False;
  nmb->header.nm_flags.bcast = bcast;
  nmb->header.nm_flags.recursion_available = False;
  nmb->header.nm_flags.recursion_desired = recurse;
  nmb->header.nm_flags.trunc = False;
  nmb->header.nm_flags.authoritative = False;
  nmb->header.rcode = 0;
  nmb->header.qdcount = 1;
  nmb->header.ancount = 0;
  nmb->header.nscount = 0;
  nmb->header.arcount = 0;

  make_nmb_name(&nmb->question.question_name,name,name_type);

  nmb->question.question_type = 0x20;
  nmb->question.question_class = 0x1;

  p.ip = to_ip;
  p.port = NMB_PORT;
  p.fd = fd;
  p.timestamp = time(NULL);
  p.packet_type = NMB_PACKET;

  GetTimeOfDay(&tval);

  if (!send_packet(&p)) 
    return NULL;

  retries--;

  while (1)
  {
    struct timeval tval2;
    GetTimeOfDay(&tval2);
    if (TvalDiff(&tval,&tval2) > retry_time) 
    {
      if (!retries)
        break;
      if (!found && !send_packet(&p))
        return NULL;
      GetTimeOfDay(&tval);
      retries--;
    }

    if ((p2=receive_packet(fd,NMB_PACKET,90)))
    {     
      struct nmb_packet *nmb2 = &p2->packet.nmb;
      debug_nmb_packet(p2);

      if (nmb->header.name_trn_id != nmb2->header.name_trn_id ||
          !nmb2->header.response)
      {
        /* 
         * Its not for us - maybe deal with it later 
         * (put it on the queue?).
         */
        if (fn) 
          fn(p2);
        else
          free_packet(p2);
        continue;
      }
	  
      if (nmb2->header.opcode != 0 ||
          nmb2->header.nm_flags.bcast ||
          nmb2->header.rcode ||
          !nmb2->header.ancount)
      {
	    /* 
         * XXXX what do we do with this? Could be a redirect, but
         * we'll discard it for the moment.
         */
        free_packet(p2);
        continue;
      }

      ip_list = (struct in_addr *)Realloc(ip_list, sizeof(ip_list[0]) * 
                                          ((*count)+nmb2->answers->rdlength/6));
      if (ip_list)
      {
        DEBUG(fn?3:2,("Got a positive name query response from %s ( ",
              inet_ntoa(p2->ip)));
        for (i=0;i<nmb2->answers->rdlength/6;i++)
        {
          putip((char *)&ip_list[(*count)],&nmb2->answers->rdata[2+i*6]);
          DEBUG(fn?3:2,("%s ",inet_ntoa(ip_list[(*count)])));
          (*count)++;
        }
        DEBUG(fn?3:2,(")\n"));
      }

      found=True;
      retries=0;
      free_packet(p2);
      if (fn)
        break;

      /*
       * If we're doing a unicast lookup we only
       * expect one reply. Don't wait the full 2
       * seconds if we got one. JRA.
       */
      if(!bcast && found)
        break;
    }
  }

  return ip_list;
}

/********************************************************
 Start parsing the lmhosts file.
*********************************************************/

FILE *startlmhosts(char *fname)
{
  FILE *fp = sys_fopen(fname,"r");
  if (!fp) {
    DEBUG(4,("startlmhosts: Can't open lmhosts file %s. Error was %s\n",
             fname, strerror(errno)));
    return NULL;
  }
  return fp;
}

/********************************************************
 Parse the next line in the lmhosts file.
*********************************************************/

BOOL getlmhostsent( FILE *fp, pstring name, int *name_type, struct in_addr *ipaddr)
{
  pstring line;

  while(!feof(fp) && !ferror(fp)) {
    pstring ip,flags,extra;
    char *ptr;
    int count = 0;

    *name_type = -1;

    if (!fgets_slash(line,sizeof(pstring),fp))
      continue;

    if (*line == '#')
      continue;

    pstrcpy(ip,"");
    pstrcpy(name,"");
    pstrcpy(flags,"");

    ptr = line;

    if (next_token(&ptr,ip   ,NULL,sizeof(ip)))
      ++count;
    if (next_token(&ptr,name ,NULL, sizeof(pstring)))
      ++count;
    if (next_token(&ptr,flags,NULL, sizeof(flags)))
      ++count;
    if (next_token(&ptr,extra,NULL, sizeof(extra)))
      ++count;

    if (count <= 0)
      continue;

    if (count > 0 && count < 2)
    {
      DEBUG(0,("getlmhostsent: Ill formed hosts line [%s]\n",line));
      continue;
    }

    if (count >= 4)
    {
      DEBUG(0,("getlmhostsent: too many columns in lmhosts file (obsolete syntax)\n"));
      continue;
    }

    DEBUG(4, ("getlmhostsent: lmhost entry: %s %s %s\n", ip, name, flags));

    if (strchr(flags,'G') || strchr(flags,'S'))
    {
      DEBUG(0,("getlmhostsent: group flag in lmhosts ignored (obsolete)\n"));
      continue;
    }

    *ipaddr = *interpret_addr2(ip);

    /* Extra feature. If the name ends in '#XX', where XX is a hex number,
       then only add that name type. */
    if((ptr = strchr(name, '#')) != NULL)
    {
      char *endptr;

      ptr++;
      *name_type = (int)strtol(ptr, &endptr, 16);

      if(!*ptr || (endptr == ptr))
      {
        DEBUG(0,("getlmhostsent: invalid name %s containing '#'.\n", name));
        continue;
      }

      *(--ptr) = '\0'; /* Truncate at the '#' */
    }

    return True;
  }

  return False;
}

/********************************************************
 Finish parsing the lmhosts file.
*********************************************************/

void endlmhosts(FILE *fp)
{
  fclose(fp);
}

/********************************************************
 Resolve via "bcast" method.
*********************************************************/

static BOOL resolve_bcast(const char *name, int name_type,
				struct in_addr **return_ip_list, int *return_count)
{
	int sock, i;
	int num_interfaces = iface_count();

	*return_ip_list = NULL;
	*return_count = 0;
	
	/*
	 * "bcast" means do a broadcast lookup on all the local interfaces.
	 */

	DEBUG(3,("resolve_bcast: Attempting broadcast lookup for name %s<0x%x>\n", name, name_type));

	sock = open_socket_in( SOCK_DGRAM, 0, 3,
			       interpret_addr(lp_socket_address()), True );

	if (sock == -1) return False;

	set_socket_options(sock,"SO_BROADCAST");
	/*
	 * Lookup the name on all the interfaces, return on
	 * the first successful match.
	 */
	for( i = num_interfaces-1; i >= 0; i--) {
		struct in_addr sendto_ip;
		/* Done this way to fix compiler error on IRIX 5.x */
		sendto_ip = *iface_bcast(*iface_n_ip(i));
		*return_ip_list = name_query(sock, name, name_type, True, 
				    True, sendto_ip, return_count, NULL);
		if(*return_ip_list != NULL) {
			close(sock);
			return True;
		}
	}

	close(sock);
	return False;
}

/********************************************************
 Resolve via "wins" method.
*********************************************************/

static BOOL resolve_wins(const char *name, int name_type,
                         struct in_addr **return_iplist, int *return_count)
{
	int sock;
	struct in_addr wins_ip;
	BOOL wins_ismyip;

	*return_iplist = NULL;
	*return_count = 0;
	
	/*
	 * "wins" means do a unicast lookup to the WINS server.
	 * Ignore if there is no WINS server specified or if the
	 * WINS server is one of our interfaces (if we're being
	 * called from within nmbd - we can't do this call as we
	 * would then block).
	 */

	DEBUG(3,("resolve_wins: Attempting wins lookup for name %s<0x%x>\n", name, name_type));

	if(!*lp_wins_server()) {
		DEBUG(3,("resolve_wins: WINS server resolution selected and no WINS server present.\n"));
		return False;
	}

	wins_ip = *interpret_addr2(lp_wins_server());
	wins_ismyip = ismyip(wins_ip);

	if((wins_ismyip && !global_in_nmbd) || !wins_ismyip) {
		sock = open_socket_in( SOCK_DGRAM, 0, 3,
							interpret_addr(lp_socket_address()), True );
	      
		if (sock != -1) {
			*return_iplist = name_query(sock, name, name_type, False, 
								True, wins_ip, return_count, NULL);
			if(*return_iplist != NULL) {
				close(sock);
				return True;
			}
			close(sock);
		}
	}

	return False;
}

/********************************************************
 Resolve via "lmhosts" method.
*********************************************************/

static BOOL resolve_lmhosts(const char *name, int name_type,
                         struct in_addr **return_iplist, int *return_count)
{
	/*
	 * "lmhosts" means parse the local lmhosts file.
	 */
	
	FILE *fp;
	pstring lmhost_name;
	int name_type2;
	struct in_addr return_ip;

	*return_iplist = NULL;
	*return_count = 0;

	DEBUG(3,("resolve_lmhosts: Attempting lmhosts lookup for name %s<0x%x>\n", name, name_type));

	fp = startlmhosts( LMHOSTSFILE );
	if(fp) {
		while (getlmhostsent(fp, lmhost_name, &name_type2, &return_ip)) {
			if (strequal(name, lmhost_name) && 
                ((name_type2 == -1) || (name_type == name_type2))
               ) {
				endlmhosts(fp);
				*return_iplist = (struct in_addr *)malloc(sizeof(struct in_addr));
				if(*return_iplist == NULL) {
					DEBUG(3,("resolve_lmhosts: malloc fail !\n"));
					return False;
				}
				**return_iplist = return_ip;
				*return_count = 1;
				return True; 
			}
		}
		endlmhosts(fp);
	}
	return False;
}


/********************************************************
 Resolve via "hosts" method.
*********************************************************/

static BOOL resolve_hosts(const char *name,
                         struct in_addr **return_iplist, int *return_count)
{
	/*
	 * "host" means do a localhost, or dns lookup.
	 */
	struct hostent *hp;

	*return_iplist = NULL;
	*return_count = 0;

	DEBUG(3,("resolve_hosts: Attempting host lookup for name %s<0x20>\n", name));
	
	if (((hp = Get_Hostbyname(name)) != NULL) && (hp->h_addr != NULL)) {
		struct in_addr return_ip;
		putip((char *)&return_ip,(char *)hp->h_addr);
		*return_iplist = (struct in_addr *)malloc(sizeof(struct in_addr));
		if(*return_iplist == NULL) {
			DEBUG(3,("resolve_hosts: malloc fail !\n"));
			return False;
		}
		**return_iplist = return_ip;
		*return_count = 1;
		return True;
	}
	return False;
}

/********************************************************
 Internal interface to resolve a name into an IP address.
 Use this function if the string is either an IP address, DNS
 or host name or NetBIOS name. This uses the name switch in the
 smb.conf to determine the order of name resolution.
*********************************************************/

static BOOL internal_resolve_name(const char *name, int name_type,
                         		struct in_addr **return_iplist, int *return_count)
{
  pstring name_resolve_list;
  fstring tok;
  char *ptr;
  BOOL allones = (strcmp(name,"255.255.255.255") == 0);
  BOOL allzeros = (strcmp(name,"0.0.0.0") == 0);
  BOOL is_address = is_ipaddress(name);
  *return_iplist = NULL;
  *return_count = 0;

  if (allzeros || allones || is_address) {
	*return_iplist = (struct in_addr *)malloc(sizeof(struct in_addr));
	if(*return_iplist == NULL) {
		DEBUG(3,("internal_resolve_name: malloc fail !\n"));
		return False;
	}
	if(is_address) { 
		/* if it's in the form of an IP address then get the lib to interpret it */
		(*return_iplist)->s_addr = inet_addr(name);
    } else {
		(*return_iplist)->s_addr = allones ? 0xFFFFFFFF : 0;
		*return_count = 1;
	}
    return True;
  }
  
  pstrcpy(name_resolve_list, lp_name_resolve_order());
  ptr = name_resolve_list;
  if (!ptr || !*ptr)
    ptr = "host";

  while (next_token(&ptr, tok, LIST_SEP, sizeof(tok))) {
	  if((strequal(tok, "host") || strequal(tok, "hosts"))) {
		  if (name_type == 0x20 && resolve_hosts(name, return_iplist, return_count)) {
			  return True;
		  }
	  } else if(strequal( tok, "lmhosts")) {
		  if (resolve_lmhosts(name, name_type, return_iplist, return_count)) {
			  return True;
		  }
	  } else if(strequal( tok, "wins")) {
		  /* don't resolve 1D via WINS */
		  if (name_type != 0x1D &&
		      resolve_wins(name, name_type, return_iplist, return_count)) {
			  return True;
		  }
	  } else if(strequal( tok, "bcast")) {
		  if (resolve_bcast(name, name_type, return_iplist, return_count)) {
			  return True;
		  }
	  } else {
		  DEBUG(0,("resolve_name: unknown name switch type %s\n", tok));
	  }
  }

  if((*return_iplist) != NULL) {
    free((char *)(*return_iplist));
    *return_iplist = NULL;
  }
  return False;
}

/********************************************************
 Internal interface to resolve a name into one IP address.
 Use this function if the string is either an IP address, DNS
 or host name or NetBIOS name. This uses the name switch in the
 smb.conf to determine the order of name resolution.
*********************************************************/

BOOL resolve_name(const char *name, struct in_addr *return_ip, int name_type)
{
	struct in_addr *ip_list = NULL;
	int count = 0;

	if(internal_resolve_name(name, name_type, &ip_list, &count)) {
		*return_ip = ip_list[0];
		free((char *)ip_list);
		return True;
	}
	if(ip_list != NULL)
		free((char *)ip_list);
	return False;
}

/********************************************************
 Find the IP address of the master browser or DMB for a workgroup.
*********************************************************/

BOOL find_master_ip(char *group, struct in_addr *master_ip)
{
	struct in_addr *ip_list = NULL;
	int count = 0;

	if (internal_resolve_name(group, 0x1D, &ip_list, &count)) {
		*master_ip = ip_list[0];
		free((char *)ip_list);
		return True;
	}
	if(internal_resolve_name(group, 0x1B, &ip_list, &count)) {
		*master_ip = ip_list[0];
		free((char *)ip_list);
		return True;
	}

	if(ip_list != NULL)
		free((char *)ip_list);
	return False;
}

/********************************************************
 Internal function to extract the MACHINE<0x20> name.
*********************************************************/

static void _lookup_pdc_name(char *p, char *master,char *rname)
{
  int numnames = CVAL(p,0);

  *rname = '\0';

  p += 1;
  while (numnames--) {
    int type = CVAL(p,15);
    if(type == 0x20) {
      StrnCpy(rname,p,15);
      trim_string(rname,NULL," ");
      return;
    }
    p += 18;
  }
}

/********************************************************
 Lookup a PDC name given a Domain name and IP address.
*********************************************************/

BOOL lookup_pdc_name(const char *srcname, const char *domain, struct in_addr *pdc_ip, char *ret_name)
{
#if !defined(I_HATE_WINDOWS_REPLY_CODE)

  fstring pdc_name;
  BOOL ret;

  /*
   * Due to the fact win WinNT *sucks* we must do a node status
   * query here... JRA.
   */

  int sock = open_socket_in(SOCK_DGRAM, 0, 3, interpret_addr(lp_socket_address()), True );

  if(sock == -1)
    return False;

  *pdc_name = '\0';

  ret = internal_name_status(sock,"*SMBSERVER",0x20,True,
		 *pdc_ip,NULL,pdc_name,False,
         _lookup_pdc_name,NULL);

  close(sock);

  if(ret && *pdc_name) {
    fstrcpy(ret_name, pdc_name);
    return True;
  }

  return False;

#else /* defined(I_HATE_WINDOWS_REPLY_CODE) */
  /*
   * Sigh. I *love* this code, it took me ages to get right and it's
   * completely *USELESS* because NT 4.x refuses to send the mailslot
   * reply back to the correct port (it always uses 138).
   * I hate NT when it does these things... JRA.
   */

  int retries = 3;
  int retry_time = 2000;
  struct timeval tval;
  struct packet_struct p;
  struct dgram_packet *dgram = &p.packet.dgram;
  char *ptr,*p2;
  char tmp[4];
  int len;
  struct sockaddr_in sock_name;
  int sock_len = sizeof(sock_name);
  const char *mailslot = "\\MAILSLOT\\NET\\NETLOGON";
  static int name_trn_id = 0;
  char buffer[1024];
  char *bufp;
  int sock = open_socket_in(SOCK_DGRAM, 0, 3, interpret_addr(lp_socket_address()), True );

  if(sock == -1)
    return False;

  /* Find out the transient UDP port we have been allocated. */
  if(getsockname(sock, (struct sockaddr *)&sock_name, &sock_len)<0) {
    DEBUG(0,("lookup_pdc_name: Failed to get local UDP port. Error was %s\n",
            strerror(errno)));
    close(sock);
    return False;
  }

  /*
   * Create the request data.
   */

  memset(buffer,'\0',sizeof(buffer));
  bufp = buffer;
  SSVAL(bufp,0,QUERYFORPDC);
  bufp += 2;
  fstrcpy(bufp,srcname);
  bufp += (strlen(bufp) + 1);
  fstrcpy(bufp,"\\MAILSLOT\\NET\\GETDC411");
  bufp += (strlen(bufp) + 1);
  bufp = align2(bufp, buffer);
  bufp += dos_PutUniCode(bufp, srcname, sizeof(buffer) - (bufp - buffer) - 1, True);
  SIVAL(bufp,0,1);
  SSVAL(bufp,4,0xFFFF); 
  SSVAL(bufp,6,0xFFFF); 
  bufp += 8;
  len = PTR_DIFF(bufp,buffer);

  if (!name_trn_id)
    name_trn_id = ((unsigned)time(NULL)%(unsigned)0x7FFF) + ((unsigned)getpid()%(unsigned)100);
  name_trn_id = (name_trn_id+1) % (unsigned)0x7FFF;

  memset((char *)&p,'\0',sizeof(p));

  /* DIRECT GROUP or UNIQUE datagram. */
  dgram->header.msg_type = 0x10;
  dgram->header.flags.node_type = M_NODE;
  dgram->header.flags.first = True;
  dgram->header.flags.more = False;
  dgram->header.dgm_id = name_trn_id;
  dgram->header.source_ip = sock_name.sin_addr;
  dgram->header.source_port = ntohs(sock_name.sin_port);
  dgram->header.dgm_length = 0; /* Let build_dgram() handle this. */
  dgram->header.packet_offset = 0;
 
  make_nmb_name(&dgram->source_name,srcname,0);
  make_nmb_name(&dgram->dest_name,domain,0x1B);

  ptr = &dgram->data[0];

  /* Setup the smb part. */
  ptr -= 4; /* XXX Ugliness because of handling of tcp SMB length. */
  memcpy(tmp,ptr,4);
  set_message(ptr,17,17 + len,True);
  memcpy(ptr,tmp,4);

  CVAL(ptr,smb_com) = SMBtrans;
  SSVAL(ptr,smb_vwv1,len);
  SSVAL(ptr,smb_vwv11,len);
  SSVAL(ptr,smb_vwv12,70 + strlen(mailslot));
  SSVAL(ptr,smb_vwv13,3);
  SSVAL(ptr,smb_vwv14,1);
  SSVAL(ptr,smb_vwv15,1);
  SSVAL(ptr,smb_vwv16,2);
  p2 = smb_buf(ptr);
  pstrcpy(p2,mailslot);
  p2 = skip_string(p2,1);

  memcpy(p2,buffer,len);
  p2 += len;

  dgram->datasize = PTR_DIFF(p2,ptr+4); /* +4 for tcp length. */

  p.ip = *pdc_ip;
  p.port = DGRAM_PORT;
  p.fd = sock;
  p.timestamp = time(NULL);
  p.packet_type = DGRAM_PACKET;

  GetTimeOfDay(&tval);

  if (!send_packet(&p)) {
    DEBUG(0,("lookup_pdc_name: send_packet failed.\n"));
    close(sock);
    return False;
  }

  retries--;

  while (1) {
    struct timeval tval2;
    struct packet_struct *p_ret;

    GetTimeOfDay(&tval2);
    if (TvalDiff(&tval,&tval2) > retry_time) {
      if (!retries)
        break;
      if (!send_packet(&p)) {
        DEBUG(0,("lookup_pdc_name: send_packet failed.\n"));
        close(sock);
        return False;
      }
      GetTimeOfDay(&tval);
      retries--;
    }

    if ((p_ret = receive_packet(sock,NMB_PACKET,90))) {
      struct nmb_packet *nmb2 = &p_ret->packet.nmb;
      struct dgram_packet *dgram2 = &p_ret->packet.dgram;
      char *buf;
      char *buf2;

      debug_nmb_packet(p_ret);

      if (memcmp(&p.ip, &p_ret->ip, sizeof(p.ip))) {
        /* 
         * Not for us.
         */
        DEBUG(0,("lookup_pdc_name: datagram return IP %s doesn't match\n", inet_ntoa(p_ret->ip) ));
        free_packet(p_ret);
        continue;
      }

      buf = &dgram2->data[0];
      buf -= 4;

      if (CVAL(buf,smb_com) != SMBtrans) {
        DEBUG(0,("lookup_pdc_name: datagram type %u != SMBtrans(%u)\n", (unsigned int)CVAL(buf,smb_com),
              (unsigned int)SMBtrans ));
        free_packet(p_ret);
        continue;
      }

      len = SVAL(buf,smb_vwv11);
      buf2 = smb_base(buf) + SVAL(buf,smb_vwv12);

      if (len <= 0) {
        DEBUG(0,("lookup_pdc_name: datagram len < 0 (%d)\n", len ));
        free_packet(p_ret);
        continue;
      }

      DEBUG(4,("lookup_pdc_name: datagram reply from %s to %s IP %s for %s of type %d len=%d\n",
       nmb_namestr(&dgram2->source_name),nmb_namestr(&dgram2->dest_name),
       inet_ntoa(p_ret->ip), smb_buf(buf),CVAL(buf2,0),len));

      if(SVAL(buf,0) != QUERYFORPDC_R) {
        DEBUG(0,("lookup_pdc_name: datagram type (%u) != QUERYFORPDC_R(%u)\n",
              (unsigned int)SVAL(buf,0), (unsigned int)QUERYFORPDC_R ));
        free_packet(p_ret);
        continue;
      }

      buf += 2;
      /* Note this is safe as it is a bounded strcpy. */
      fstrcpy(ret_name, buf);
      ret_name[sizeof(fstring)-1] = '\0';
      close(sock);
      free_packet(p_ret);
      return True;
    }
  }

  close(sock);
  return False;
#endif /* I_HATE_WINDOWS_REPLY_CODE */
}

/********************************************************
 Get the IP address list of the PDC/BDC's of a Domain.
*********************************************************/

BOOL get_dc_list(char *group, struct in_addr **ip_list, int *count)
{
	return internal_resolve_name(group, 0x1C, ip_list, count);
}
