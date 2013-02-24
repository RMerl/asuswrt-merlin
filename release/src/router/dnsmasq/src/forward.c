/* dnsmasq is Copyright (c) 2000-2012 Simon Kelley

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 dated June, 1991, or
   (at your option) version 3 dated 29 June, 2007.
 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
     
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "dnsmasq.h"

static struct frec *lookup_frec(unsigned short id, unsigned int crc);
static struct frec *lookup_frec_by_sender(unsigned short id,
					  union mysockaddr *addr,
					  unsigned int crc);
static unsigned short get_id(unsigned int crc);
static void free_frec(struct frec *f);
static struct randfd *allocate_rfd(int family);

/* Send a UDP packet with its source address set as "source" 
   unless nowild is true, when we just send it with the kernel default */
int send_from(int fd, int nowild, char *packet, size_t len, 
	      union mysockaddr *to, struct all_addr *source,
	      unsigned int iface)
{
  struct msghdr msg;
  struct iovec iov[1]; 
  union {
    struct cmsghdr align; /* this ensures alignment */
#if defined(HAVE_LINUX_NETWORK)
    char control[CMSG_SPACE(sizeof(struct in_pktinfo))];
#elif defined(IP_SENDSRCADDR)
    char control[CMSG_SPACE(sizeof(struct in_addr))];
#endif
#ifdef HAVE_IPV6
    char control6[CMSG_SPACE(sizeof(struct in6_pktinfo))];
#endif
  } control_u;
  
  iov[0].iov_base = packet;
  iov[0].iov_len = len;

  msg.msg_control = NULL;
  msg.msg_controllen = 0;
  msg.msg_flags = 0;
  msg.msg_name = to;
  msg.msg_namelen = sa_len(to);
  msg.msg_iov = iov;
  msg.msg_iovlen = 1;
  
  if (!nowild)
    {
      struct cmsghdr *cmptr;
      msg.msg_control = &control_u;
      msg.msg_controllen = sizeof(control_u);
      cmptr = CMSG_FIRSTHDR(&msg);

      if (to->sa.sa_family == AF_INET)
	{
#if defined(HAVE_LINUX_NETWORK)
	  struct in_pktinfo p;
	  p.ipi_ifindex = 0;
	  p.ipi_spec_dst = source->addr.addr4;
	  memcpy(CMSG_DATA(cmptr), &p, sizeof(p));
	  msg.msg_controllen = cmptr->cmsg_len = CMSG_LEN(sizeof(struct in_pktinfo));
	  cmptr->cmsg_level = IPPROTO_IP;
	  cmptr->cmsg_type = IP_PKTINFO;
#elif defined(IP_SENDSRCADDR)
	  memcpy(CMSG_DATA(cmptr), &(source->addr.addr4), sizeof(source->addr.addr4));
	  msg.msg_controllen = cmptr->cmsg_len = CMSG_LEN(sizeof(struct in_addr));
	  cmptr->cmsg_level = IPPROTO_IP;
	  cmptr->cmsg_type = IP_SENDSRCADDR;
#endif
	}
      else
#ifdef HAVE_IPV6
	{
	  struct in6_pktinfo p;
	  p.ipi6_ifindex = iface; /* Need iface for IPv6 to handle link-local addrs */
	  p.ipi6_addr = source->addr.addr6;
	  memcpy(CMSG_DATA(cmptr), &p, sizeof(p));
	  msg.msg_controllen = cmptr->cmsg_len = CMSG_LEN(sizeof(struct in6_pktinfo));
	  cmptr->cmsg_type = daemon->v6pktinfo;
	  cmptr->cmsg_level = IPPROTO_IPV6;
	}
#else
      (void)iface; /* eliminate warning */
#endif
    }
  
  while (sendmsg(fd, &msg, 0) == -1)
    {
      if (retry_send())
	continue;
      
      /* If interface is still in DAD, EINVAL results - ignore that. */
      if (errno == EINVAL)
	break;
      
      my_syslog(LOG_ERR, _("failed to send packet: %s"), strerror(errno));
      return 0;
    }
  
  return 1;
}
          
static unsigned int search_servers(time_t now, struct all_addr **addrpp, 
				     unsigned int qtype, char *qdomain, int *type, char **domain, int *norebind)
			      
{
  /* If the query ends in the domain in one of our servers, set
     domain to point to that name. We find the largest match to allow both
     domain.org and sub.domain.org to exist. */
  
  unsigned int namelen = strlen(qdomain);
  unsigned int matchlen = 0;
  struct server *serv;
  unsigned int flags = 0;
  
  for (serv = daemon->servers; serv; serv=serv->next)
    /* domain matches take priority over NODOTS matches */
    if ((serv->flags & SERV_FOR_NODOTS) && *type != SERV_HAS_DOMAIN && !strchr(qdomain, '.') && namelen != 0)
      {
	unsigned int sflag = serv->addr.sa.sa_family == AF_INET ? F_IPV4 : F_IPV6; 
	*type = SERV_FOR_NODOTS;
	if (serv->flags & SERV_NO_ADDR)
	  flags = F_NXDOMAIN;
	else if (serv->flags & SERV_LITERAL_ADDRESS) 
	  { 
	    if (sflag & qtype)
	      {
		flags = sflag;
		if (serv->addr.sa.sa_family == AF_INET) 
		  *addrpp = (struct all_addr *)&serv->addr.in.sin_addr;
#ifdef HAVE_IPV6
		else
		  *addrpp = (struct all_addr *)&serv->addr.in6.sin6_addr;
#endif 
	      }
	    else if (!flags || (flags & F_NXDOMAIN))
	      flags = F_NOERR;
	  } 
      }
    else if (serv->flags & SERV_HAS_DOMAIN)
      {
	unsigned int domainlen = strlen(serv->domain);
	char *matchstart = qdomain + namelen - domainlen;
	if (namelen >= domainlen &&
	    hostname_isequal(matchstart, serv->domain) &&
	    (domainlen == 0 || namelen == domainlen || *(matchstart-1) == '.' ))
	  {
	    if (serv->flags & SERV_NO_REBIND)	
	      *norebind = 1;
	    else
	      {
		unsigned int sflag = serv->addr.sa.sa_family == AF_INET ? F_IPV4 : F_IPV6;
		/* implement priority rules for --address and --server for same domain.
		   --address wins if the address is for the correct AF
		   --server wins otherwise. */
		if (domainlen != 0 && domainlen == matchlen)
		  {
		    if ((serv->flags & SERV_LITERAL_ADDRESS))
		      {
			if (!(sflag & qtype) && flags == 0)
			  continue;
		      }
		    else
		      {
			if (flags & (F_IPV4 | F_IPV6))
			  continue;
		      }
		  }
		
		if (domainlen >= matchlen)
		  {
		    *type = serv->flags & (SERV_HAS_DOMAIN | SERV_USE_RESOLV | SERV_NO_REBIND);
		    *domain = serv->domain;
		    matchlen = domainlen;
		    if (serv->flags & SERV_NO_ADDR)
		      flags = F_NXDOMAIN;
		    else if (serv->flags & SERV_LITERAL_ADDRESS)
		      {
			if (sflag & qtype)
			  {
			    flags = sflag;
			    if (serv->addr.sa.sa_family == AF_INET) 
			      *addrpp = (struct all_addr *)&serv->addr.in.sin_addr;
#ifdef HAVE_IPV6
			    else
			      *addrpp = (struct all_addr *)&serv->addr.in6.sin6_addr;
#endif
			  }
			else if (!flags || (flags & F_NXDOMAIN))
			  flags = F_NOERR;
		      }
		    else
		      flags = 0;
		  } 
	      }
	  }
      }
  
  if (flags == 0 && !(qtype & F_QUERY) && 
      option_bool(OPT_NODOTS_LOCAL) && !strchr(qdomain, '.') && namelen != 0)
    /* don't forward A or AAAA queries for simple names, except the empty name */
    flags = F_NOERR;
  
  if (flags == F_NXDOMAIN && check_for_local_domain(qdomain, now))
    flags = F_NOERR;

  if (flags)
    {
      int logflags = 0;
      
      if (flags == F_NXDOMAIN || flags == F_NOERR)
	logflags = F_NEG | qtype;
  
      log_query(logflags | flags | F_CONFIG | F_FORWARD, qdomain, *addrpp, NULL);
    }
  else if ((*type) & SERV_USE_RESOLV)
    {
      *type = 0; /* use normal servers for this domain */
      *domain = NULL;
    }
  return  flags;
}

static int forward_query(int udpfd, union mysockaddr *udpaddr,
			 struct all_addr *dst_addr, unsigned int dst_iface,
			 struct dns_header *header, size_t plen, time_t now, struct frec *forward)
{
  char *domain = NULL;
  int type = 0, norebind = 0;
  struct all_addr *addrp = NULL;
  unsigned int crc = questions_crc(header, plen, daemon->namebuff);
  unsigned int flags = 0;
  unsigned int gotname = extract_request(header, plen, daemon->namebuff, NULL);
  struct server *start = NULL;
  
  /* RFC 4035: sect 4.6 para 2 */
  header->hb4 &= ~HB4_AD;
  
  /* may be no servers available. */
  if (!daemon->servers)
    forward = NULL;
  else if (forward || (forward = lookup_frec_by_sender(ntohs(header->id), udpaddr, crc)))
    {
      /* retry on existing query, send to all available servers  */
      domain = forward->sentto->domain;
      forward->sentto->failed_queries++;
      if (!option_bool(OPT_ORDER))
	{
	  forward->forwardall = 1;
	  daemon->last_server = NULL;
	}
      type = forward->sentto->flags & SERV_TYPE;
      if (!(start = forward->sentto->next))
	start = daemon->servers; /* at end of list, recycle */
      header->id = htons(forward->new_id);
    }
  else 
    {
      if (gotname)
	flags = search_servers(now, &addrp, gotname, daemon->namebuff, &type, &domain, &norebind);
      
      if (!flags && !(forward = get_new_frec(now, NULL)))
	/* table full - server failure. */
	flags = F_NEG;
      
      if (forward)
	{
	  forward->source = *udpaddr;
	  forward->dest = *dst_addr;
	  forward->iface = dst_iface;
	  forward->orig_id = ntohs(header->id);
	  forward->new_id = get_id(crc);
	  forward->fd = udpfd;
	  forward->crc = crc;
	  forward->forwardall = 0;
	  if (norebind)
	    forward->flags |= FREC_NOREBIND;
	  if (header->hb4 & HB4_CD)
	    forward->flags |= FREC_CHECKING_DISABLED;

	  header->id = htons(forward->new_id);
	  
	  /* In strict_order mode, always try servers in the order 
	     specified in resolv.conf, if a domain is given 
	     always try all the available servers,
	     otherwise, use the one last known to work. */
	  
	  if (type == 0)
	    {
	      if (option_bool(OPT_ORDER))
		start = daemon->servers;
	      else if (!(start = daemon->last_server) ||
		       daemon->forwardcount++ > FORWARD_TEST ||
		       difftime(now, daemon->forwardtime) > FORWARD_TIME)
		{
		  start = daemon->servers;
		  forward->forwardall = 1;
		  daemon->forwardcount = 0;
		  daemon->forwardtime = now;
		}
	    }
	  else
	    {
	      start = daemon->servers;
	      if (!option_bool(OPT_ORDER))
		forward->forwardall = 1;
	    }
	}
    }

  /* check for send errors here (no route to host) 
     if we fail to send to all nameservers, send back an error
     packet straight away (helps modem users when offline)  */
  
  if (!flags && forward)
    {
      struct server *firstsentto = start;
      int forwarded = 0;
      
      if (udpaddr && option_bool(OPT_ADD_MAC))
	plen = add_mac(header, plen, ((char *) header) + PACKETSZ, udpaddr);
      
      while (1)
	{ 
	  /* only send to servers dealing with our domain.
	     domain may be NULL, in which case server->domain 
	     must be NULL also. */
	  
	  if (type == (start->flags & SERV_TYPE) &&
	      (type != SERV_HAS_DOMAIN || hostname_isequal(domain, start->domain)) &&
	      !(start->flags & SERV_LITERAL_ADDRESS))
	    {
	      int fd;

	      /* find server socket to use, may need to get random one. */
	      if (start->sfd)
		fd = start->sfd->fd;
	      else 
		{
#ifdef HAVE_IPV6
		  if (start->addr.sa.sa_family == AF_INET6)
		    {
		      if (!forward->rfd6 &&
			  !(forward->rfd6 = allocate_rfd(AF_INET6)))
			break;
		      daemon->rfd_save = forward->rfd6;
		      fd = forward->rfd6->fd;
		    }
		  else
#endif
		    {
		      if (!forward->rfd4 &&
			  !(forward->rfd4 = allocate_rfd(AF_INET)))
			break;
		      daemon->rfd_save = forward->rfd4;
		      fd = forward->rfd4->fd;
		    }

#ifdef HAVE_CONNTRACK
		  /* Copy connection mark of incoming query to outgoing connection. */
		  if (option_bool(OPT_CONNTRACK))
		    {
		      unsigned int mark;
		      if (get_incoming_mark(udpaddr, dst_addr, 0, &mark))
			setsockopt(fd, SOL_SOCKET, SO_MARK, &mark, sizeof(unsigned int));
		    }
#endif
		}
	      
	      if (sendto(fd, (char *)header, plen, 0,
			 &start->addr.sa,
			 sa_len(&start->addr)) == -1)
		{
		  if (retry_send())
		    continue;
		}
	      else
		{
		  /* Keep info in case we want to re-send this packet */
		  daemon->srv_save = start;
		  daemon->packet_len = plen;
		  
		  if (!gotname)
		    strcpy(daemon->namebuff, "query");
		  if (start->addr.sa.sa_family == AF_INET)
		    log_query(F_SERVER | F_IPV4 | F_FORWARD, daemon->namebuff, 
			      (struct all_addr *)&start->addr.in.sin_addr, NULL); 
#ifdef HAVE_IPV6
		  else
		    log_query(F_SERVER | F_IPV6 | F_FORWARD, daemon->namebuff, 
			      (struct all_addr *)&start->addr.in6.sin6_addr, NULL);
#endif 
		  start->queries++;
		  forwarded = 1;
		  forward->sentto = start;
		  if (!forward->forwardall) 
		    break;
		  forward->forwardall++;
		}
	    } 
	  
	  if (!(start = start->next))
 	    start = daemon->servers;
	  
	  if (start == firstsentto)
	    break;
	}
      
      if (forwarded)
	return 1;
      
      /* could not send on, prepare to return */ 
      header->id = htons(forward->orig_id);
      free_frec(forward); /* cancel */
    }	  
  
  /* could not send on, return empty answer or address if known for whole domain */
  if (udpfd != -1)
    {
      plen = setup_reply(header, plen, addrp, flags, daemon->local_ttl);
      send_from(udpfd, option_bool(OPT_NOWILD) || option_bool(OPT_CLEVERBIND), (char *)header, plen, udpaddr, dst_addr, dst_iface);
    }

  return 0;
}

static size_t process_reply(struct dns_header *header, time_t now, 
			    struct server *server, size_t n, int check_rebind, int checking_disabled)
{
  unsigned char *pheader, *sizep;
  int munged = 0, is_sign;
  size_t plen; 

  /* If upstream is advertising a larger UDP packet size
     than we allow, trim it so that we don't get overlarge
     requests for the client. We can't do this for signed packets. */

  if ((pheader = find_pseudoheader(header, n, &plen, &sizep, &is_sign)) && !is_sign)
    {
      unsigned short udpsz;
      unsigned char *psave = sizep;
      
      GETSHORT(udpsz, sizep);
      if (udpsz > daemon->edns_pktsz)
	PUTSHORT(daemon->edns_pktsz, psave);
    }

  /* RFC 4035 sect 4.6 para 3 */
  if (!is_sign && !option_bool(OPT_DNSSEC))
     header->hb4 &= ~HB4_AD;

  if (OPCODE(header) != QUERY || (RCODE(header) != NOERROR && RCODE(header) != NXDOMAIN))
    return n;
  
  /* Complain loudly if the upstream server is non-recursive. */
  if (!(header->hb4 & HB4_RA) && RCODE(header) == NOERROR && ntohs(header->ancount) == 0 &&
      server && !(server->flags & SERV_WARNED_RECURSIVE))
    {
      prettyprint_addr(&server->addr, daemon->namebuff);
      my_syslog(LOG_WARNING, _("nameserver %s refused to do a recursive query"), daemon->namebuff);
      if (!option_bool(OPT_LOG))
	server->flags |= SERV_WARNED_RECURSIVE;
    }  
    
  if (daemon->bogus_addr && RCODE(header) != NXDOMAIN &&
      check_for_bogus_wildcard(header, n, daemon->namebuff, daemon->bogus_addr, now))
    {
      munged = 1;
      SET_RCODE(header, NXDOMAIN);
      header->hb3 &= ~HB3_AA;
    }
  else 
    {
      if (RCODE(header) == NXDOMAIN && 
	  extract_request(header, n, daemon->namebuff, NULL) &&
	  check_for_local_domain(daemon->namebuff, now))
	{
	  /* if we forwarded a query for a locally known name (because it was for 
	     an unknown type) and the answer is NXDOMAIN, convert that to NODATA,
	     since we know that the domain exists, even if upstream doesn't */
	  munged = 1;
	  header->hb3 |= HB3_AA;
	  SET_RCODE(header, NOERROR);
	}
      
      if (extract_addresses(header, n, daemon->namebuff, now, is_sign, check_rebind, checking_disabled))
	{
	  my_syslog(LOG_WARNING, _("possible DNS-rebind attack detected: %s"), daemon->namebuff);
	  munged = 1;
	}
    }
  
  /* do this after extract_addresses. Ensure NODATA reply and remove
     nameserver info. */
  
  if (munged)
    {
      header->ancount = htons(0);
      header->nscount = htons(0);
      header->arcount = htons(0);
    }
  
  /* the bogus-nxdomain stuff, doctor and NXDOMAIN->NODATA munging can all elide
     sections of the packet. Find the new length here and put back pseudoheader
     if it was removed. */
  return resize_packet(header, n, pheader, plen);
}

/* sets new last_server */
void reply_query(int fd, int family, time_t now)
{
  /* packet from peer server, extract data for cache, and send to
     original requester */
  struct dns_header *header;
  union mysockaddr serveraddr;
  struct frec *forward;
  socklen_t addrlen = sizeof(serveraddr);
  ssize_t n = recvfrom(fd, daemon->packet, daemon->edns_pktsz, 0, &serveraddr.sa, &addrlen);
  size_t nn;
  struct server *server;
  
  /* packet buffer overwritten */
  daemon->srv_save = NULL;
  
  /* Determine the address of the server replying  so that we can mark that as good */
  serveraddr.sa.sa_family = family;
#ifdef HAVE_IPV6
  if (serveraddr.sa.sa_family == AF_INET6)
    serveraddr.in6.sin6_flowinfo = 0;
#endif
  
  /* spoof check: answer must come from known server, */
  for (server = daemon->servers; server; server = server->next)
    if (!(server->flags & (SERV_LITERAL_ADDRESS | SERV_NO_ADDR)) &&
	sockaddr_isequal(&server->addr, &serveraddr))
      break;
   
  header = (struct dns_header *)daemon->packet;
  
  if (!server ||
      n < (int)sizeof(struct dns_header) || !(header->hb3 & HB3_QR) ||
      !(forward = lookup_frec(ntohs(header->id), questions_crc(header, n, daemon->namebuff))))
    return;
   
  server = forward->sentto;
  
  if ((RCODE(header) == SERVFAIL || RCODE(header) == REFUSED) &&
      !option_bool(OPT_ORDER) &&
      forward->forwardall == 0)
    /* for broken servers, attempt to send to another one. */
    {
      unsigned char *pheader;
      size_t plen;
      int is_sign;
      
      /* recreate query from reply */
      pheader = find_pseudoheader(header, (size_t)n, &plen, NULL, &is_sign);
      if (!is_sign)
	{
	  header->ancount = htons(0);
	  header->nscount = htons(0);
	  header->arcount = htons(0);
	  if ((nn = resize_packet(header, (size_t)n, pheader, plen)))
	    {
	      header->hb3 &= ~(HB3_QR | HB3_TC);
	      forward_query(-1, NULL, NULL, 0, header, nn, now, forward);
	      return;
	    }
	}
    }   
  
  if ((forward->sentto->flags & SERV_TYPE) == 0)
    {
      if (RCODE(header) == SERVFAIL || RCODE(header) == REFUSED)
	server = NULL;
      else
	{
	  struct server *last_server;
	  
	  /* find good server by address if possible, otherwise assume the last one we sent to */ 
	  for (last_server = daemon->servers; last_server; last_server = last_server->next)
	    if (!(last_server->flags & (SERV_LITERAL_ADDRESS | SERV_HAS_DOMAIN | SERV_FOR_NODOTS | SERV_NO_ADDR)) &&
		sockaddr_isequal(&last_server->addr, &serveraddr))
	      {
		server = last_server;
		break;
	      }
	} 
      if (!option_bool(OPT_ALL_SERVERS))
	daemon->last_server = server;
    }
  
  /* If the answer is an error, keep the forward record in place in case
     we get a good reply from another server. Kill it when we've
     had replies from all to avoid filling the forwarding table when
     everything is broken */
  if (forward->forwardall == 0 || --forward->forwardall == 1 || 
      (RCODE(header) != REFUSED && RCODE(header) != SERVFAIL))
    {
      int check_rebind = !(forward->flags & FREC_NOREBIND);

      if (!option_bool(OPT_NO_REBIND))
	check_rebind = 0;
      
      if ((nn = process_reply(header, now, server, (size_t)n, check_rebind, forward->flags & FREC_CHECKING_DISABLED)))
	{
	  header->id = htons(forward->orig_id);
	  header->hb4 |= HB4_RA; /* recursion if available */
	  send_from(forward->fd, option_bool(OPT_NOWILD) || option_bool (OPT_CLEVERBIND), daemon->packet, nn, 
		    &forward->source, &forward->dest, forward->iface);
	}
      free_frec(forward); /* cancel */
    }
}


void receive_query(struct listener *listen, time_t now)
{
  struct dns_header *header = (struct dns_header *)daemon->packet;
  union mysockaddr source_addr;
  unsigned short type;
  struct all_addr dst_addr;
  struct in_addr netmask, dst_addr_4;
  size_t m;
  ssize_t n;
  int if_index = 0;
  struct iovec iov[1];
  struct msghdr msg;
  struct cmsghdr *cmptr;
  union {
    struct cmsghdr align; /* this ensures alignment */
#ifdef HAVE_IPV6
    char control6[CMSG_SPACE(sizeof(struct in6_pktinfo))];
#endif
#if defined(HAVE_LINUX_NETWORK)
    char control[CMSG_SPACE(sizeof(struct in_pktinfo))];
#elif defined(IP_RECVDSTADDR) && defined(HAVE_SOLARIS_NETWORK)
    char control[CMSG_SPACE(sizeof(struct in_addr)) +
		 CMSG_SPACE(sizeof(unsigned int))];
#elif defined(IP_RECVDSTADDR)
    char control[CMSG_SPACE(sizeof(struct in_addr)) +
		 CMSG_SPACE(sizeof(struct sockaddr_dl))];
#endif
  } control_u;
  
  /* packet buffer overwritten */
  daemon->srv_save = NULL;
  
  if (listen->iface && listen->family == AF_INET && option_bool(OPT_NOWILD))
    {
      dst_addr_4 = listen->iface->addr.in.sin_addr;
      netmask = listen->iface->netmask;
    }
  else
    {
      dst_addr_4.s_addr = 0;
      netmask.s_addr = 0;
    }

  iov[0].iov_base = daemon->packet;
  iov[0].iov_len = daemon->edns_pktsz;
    
  msg.msg_control = control_u.control;
  msg.msg_controllen = sizeof(control_u);
  msg.msg_flags = 0;
  msg.msg_name = &source_addr;
  msg.msg_namelen = sizeof(source_addr);
  msg.msg_iov = iov;
  msg.msg_iovlen = 1;
  
  if ((n = recvmsg(listen->fd, &msg, 0)) == -1)
    return;
  
  if (n < (int)sizeof(struct dns_header) || 
      (msg.msg_flags & MSG_TRUNC) ||
      (header->hb3 & HB3_QR))
    return;
  
  source_addr.sa.sa_family = listen->family;
#ifdef HAVE_IPV6
  if (listen->family == AF_INET6)
    source_addr.in6.sin6_flowinfo = 0;
#endif

  if (!option_bool(OPT_NOWILD))
    {
      struct ifreq ifr;

      if (msg.msg_controllen < sizeof(struct cmsghdr))
	return;

#if defined(HAVE_LINUX_NETWORK)
      if (listen->family == AF_INET)
	for (cmptr = CMSG_FIRSTHDR(&msg); cmptr; cmptr = CMSG_NXTHDR(&msg, cmptr))
	  if (cmptr->cmsg_level == IPPROTO_IP && cmptr->cmsg_type == IP_PKTINFO)
	    {
	      union {
		unsigned char *c;
		struct in_pktinfo *p;
	      } p;
	      p.c = CMSG_DATA(cmptr);
	      dst_addr_4 = dst_addr.addr.addr4 = p.p->ipi_spec_dst;
	      if_index = p.p->ipi_ifindex;
	    }
#elif defined(IP_RECVDSTADDR) && defined(IP_RECVIF)
      if (listen->family == AF_INET)
	{
	  for (cmptr = CMSG_FIRSTHDR(&msg); cmptr; cmptr = CMSG_NXTHDR(&msg, cmptr))
	    {
	      union {
		unsigned char *c;
		unsigned int *i;
		struct in_addr *a;
#ifndef HAVE_SOLARIS_NETWORK
		struct sockaddr_dl *s;
#endif
	      } p;
	       p.c = CMSG_DATA(cmptr);
	       if (cmptr->cmsg_level == IPPROTO_IP && cmptr->cmsg_type == IP_RECVDSTADDR)
		 dst_addr_4 = dst_addr.addr.addr4 = *(p.a);
	       else if (cmptr->cmsg_level == IPPROTO_IP && cmptr->cmsg_type == IP_RECVIF)
#ifdef HAVE_SOLARIS_NETWORK
		 if_index = *(p.i);
#else
  	         if_index = p.s->sdl_index;
#endif
	    }
	}
#endif
      
#ifdef HAVE_IPV6
      if (listen->family == AF_INET6)
	{
	  for (cmptr = CMSG_FIRSTHDR(&msg); cmptr; cmptr = CMSG_NXTHDR(&msg, cmptr))
	    if (cmptr->cmsg_level == IPPROTO_IPV6 && cmptr->cmsg_type == daemon->v6pktinfo)
	      {
		union {
		  unsigned char *c;
		  struct in6_pktinfo *p;
		} p;
		p.c = CMSG_DATA(cmptr);
		  
		dst_addr.addr.addr6 = p.p->ipi6_addr;
		if_index = p.p->ipi6_ifindex;
	      }
	}
#endif
      
      /* enforce available interface configuration */
      
      if (!indextoname(listen->fd, if_index, ifr.ifr_name) ||
	  !iface_check(listen->family, &dst_addr, ifr.ifr_name))
	return;
      
      if (listen->family == AF_INET && option_bool(OPT_LOCALISE))
	{
	  struct irec *iface;
	  
	  /* get the netmask of the interface whch has the address we were sent to.
	     This is no neccessarily the interface we arrived on. */
	  
	  for (iface = daemon->interfaces; iface; iface = iface->next)
	    if (iface->addr.sa.sa_family == AF_INET &&
		iface->addr.in.sin_addr.s_addr == dst_addr_4.s_addr)
	      break;
	  
	  /* interface may be new */
	  if (!iface)
	    enumerate_interfaces(); 
	  
	  for (iface = daemon->interfaces; iface; iface = iface->next)
	    if (iface->addr.sa.sa_family == AF_INET &&
		iface->addr.in.sin_addr.s_addr == dst_addr_4.s_addr)
	      break;
	  
	  /* If we failed, abandon localisation */
	  if (iface)
	    netmask = iface->netmask;
	  else
	    dst_addr_4.s_addr = 0;
	}
    }
  
  if (extract_request(header, (size_t)n, daemon->namebuff, &type))
    {
      char types[20];

      querystr(types, type);

      if (listen->family == AF_INET) 
	log_query(F_QUERY | F_IPV4 | F_FORWARD, daemon->namebuff, 
		  (struct all_addr *)&source_addr.in.sin_addr, types);
#ifdef HAVE_IPV6
      else
	log_query(F_QUERY | F_IPV6 | F_FORWARD, daemon->namebuff, 
		  (struct all_addr *)&source_addr.in6.sin6_addr, types);
#endif
    }

  m = answer_request (header, ((char *) header) + PACKETSZ, (size_t)n, 
		      dst_addr_4, netmask, now);
  if (m >= 1)
    {
      send_from(listen->fd, option_bool(OPT_NOWILD) || option_bool(OPT_CLEVERBIND),
		(char *)header, m, &source_addr, &dst_addr, if_index);
      daemon->local_answer++;
    }
  else if (forward_query(listen->fd, &source_addr, &dst_addr, if_index,
			 header, (size_t)n, now, NULL))
    daemon->queries_forwarded++;
  else
    daemon->local_answer++;
}

/* The daemon forks before calling this: it should deal with one connection,
   blocking as neccessary, and then return. Note, need to be a bit careful
   about resources for debug mode, when the fork is suppressed: that's
   done by the caller. */
unsigned char *tcp_request(int confd, time_t now,
			   union mysockaddr *local_addr, struct in_addr netmask)
{
  size_t size = 0;
  int norebind = 0;
  int checking_disabled;
  size_t m;
  unsigned short qtype;
  unsigned int gotname;
  unsigned char c1, c2;
  /* Max TCP packet + slop */
  unsigned char *packet = whine_malloc(65536 + MAXDNAME + RRFIXEDSZ);
  struct dns_header *header;
  struct server *last_server;
  struct in_addr dst_addr_4;
  union mysockaddr peer_addr;
  socklen_t peer_len = sizeof(union mysockaddr);
  
  if (getpeername(confd, (struct sockaddr *)&peer_addr, &peer_len) == -1)
    return packet;

  while (1)
    {
      if (!packet ||
	  !read_write(confd, &c1, 1, 1) || !read_write(confd, &c2, 1, 1) ||
	  !(size = c1 << 8 | c2) ||
	  !read_write(confd, packet, size, 1))
       	return packet; 
  
      if (size < (int)sizeof(struct dns_header))
	continue;
      
      header = (struct dns_header *)packet;

      /* save state of "cd" flag in query */
      checking_disabled = header->hb4 & HB4_CD;
       
      /* RFC 4035: sect 4.6 para 2 */
      header->hb4 &= ~HB4_AD;
      
      if ((gotname = extract_request(header, (unsigned int)size, daemon->namebuff, &qtype)))
	{
	  char types[20];
	  
	  querystr(types, qtype);
	  
	  if (peer_addr.sa.sa_family == AF_INET) 
	    log_query(F_QUERY | F_IPV4 | F_FORWARD, daemon->namebuff, 
		      (struct all_addr *)&peer_addr.in.sin_addr, types);
#ifdef HAVE_IPV6
	  else
	    log_query(F_QUERY | F_IPV6 | F_FORWARD, daemon->namebuff, 
		      (struct all_addr *)&peer_addr.in6.sin6_addr, types);
#endif
	}
      
      if (local_addr->sa.sa_family == AF_INET)
	dst_addr_4 = local_addr->in.sin_addr;
      else
	dst_addr_4.s_addr = 0;
      
      /* m > 0 if answered from cache */
      m = answer_request(header, ((char *) header) + 65536, (unsigned int)size, 
			 dst_addr_4, netmask, now);

      /* Do this by steam now we're not in the select() loop */
      check_log_writer(NULL); 
      
      if (m == 0)
	{
	  unsigned int flags = 0;
	  struct all_addr *addrp = NULL;
	  int type = 0;
	  char *domain = NULL;
	   
	  if (option_bool(OPT_ADD_MAC))
	    size = add_mac(header, size, ((char *) header) + 65536, &peer_addr);
	          
	  if (gotname)
	    flags = search_servers(now, &addrp, gotname, daemon->namebuff, &type, &domain, &norebind);
	  
	  if (type != 0  || option_bool(OPT_ORDER) || !daemon->last_server)
	    last_server = daemon->servers;
	  else
	    last_server = daemon->last_server;
      
	  if (!flags && last_server)
	    {
	      struct server *firstsendto = NULL;
	      unsigned int crc = questions_crc(header, (unsigned int)size, daemon->namebuff);

	      /* Loop round available servers until we succeed in connecting to one.
	         Note that this code subtley ensures that consecutive queries on this connection
	         which can go to the same server, do so. */
	      while (1) 
 		{
		  if (!firstsendto)
		    firstsendto = last_server;
		  else
		    {
		      if (!(last_server = last_server->next))
			last_server = daemon->servers;
		      
		      if (last_server == firstsendto)
			break;
		    }
	      
		  /* server for wrong domain */
		  if (type != (last_server->flags & SERV_TYPE) ||
		      (type == SERV_HAS_DOMAIN && !hostname_isequal(domain, last_server->domain)))
		    continue;

		  if (last_server->tcpfd == -1)
		    {
		      if ((last_server->tcpfd = socket(last_server->addr.sa.sa_family, SOCK_STREAM, 0)) == -1)
			continue;
		      
		      if ((!local_bind(last_server->tcpfd,  &last_server->source_addr, last_server->interface, 1) ||
			   connect(last_server->tcpfd, &last_server->addr.sa, sa_len(&last_server->addr)) == -1))
			{
			  close(last_server->tcpfd);
			  last_server->tcpfd = -1;
			  continue;
			}

#ifdef HAVE_CONNTRACK
		      /* Copy connection mark of incoming query to outgoing connection. */
		      if (option_bool(OPT_CONNTRACK))
			{
			  unsigned int mark;
			  struct all_addr local;
#ifdef HAVE_IPV6		      
			  if (local_addr->sa.sa_family == AF_INET6)
			    local.addr.addr6 = local_addr->in6.sin6_addr;
			  else
#endif
			    local.addr.addr4 = local_addr->in.sin_addr;
			  
			  if (get_incoming_mark(&peer_addr, &local, 1, &mark))
			    setsockopt(last_server->tcpfd, SOL_SOCKET, SO_MARK, &mark, sizeof(unsigned int));
			}
#endif	
		    }

		  c1 = size >> 8;
		  c2 = size;
		  
		  if (!read_write(last_server->tcpfd, &c1, 1, 0) ||
		      !read_write(last_server->tcpfd, &c2, 1, 0) ||
		      !read_write(last_server->tcpfd, packet, size, 0) ||
		      !read_write(last_server->tcpfd, &c1, 1, 1) ||
		      !read_write(last_server->tcpfd, &c2, 1, 1))
		    {
		      close(last_server->tcpfd);
		      last_server->tcpfd = -1;
		      continue;
		    } 
		  
		  m = (c1 << 8) | c2;
		  if (!read_write(last_server->tcpfd, packet, m, 1))
		    return packet;
		  
		  if (!gotname)
		    strcpy(daemon->namebuff, "query");
		  if (last_server->addr.sa.sa_family == AF_INET)
		    log_query(F_SERVER | F_IPV4 | F_FORWARD, daemon->namebuff, 
			      (struct all_addr *)&last_server->addr.in.sin_addr, NULL); 
#ifdef HAVE_IPV6
		  else
		    log_query(F_SERVER | F_IPV6 | F_FORWARD, daemon->namebuff, 
			      (struct all_addr *)&last_server->addr.in6.sin6_addr, NULL);
#endif 
		  
		  /* There's no point in updating the cache, since this process will exit and
		     lose the information after a few queries. We make this call for the alias and 
		     bogus-nxdomain side-effects. */
		  /* If the crc of the question section doesn't match the crc we sent, then
		     someone might be attempting to insert bogus values into the cache by 
		     sending replies containing questions and bogus answers. */
		  if (crc == questions_crc(header, (unsigned int)m, daemon->namebuff))
		    m = process_reply(header, now, last_server, (unsigned int)m, 
				      option_bool(OPT_NO_REBIND) && !norebind, checking_disabled);
		  
		  break;
		}
	    }
	  
	  /* In case of local answer or no connections made. */
	  if (m == 0)
	    m = setup_reply(header, (unsigned int)size, addrp, flags, daemon->local_ttl);
	}

      check_log_writer(NULL);
      
      c1 = m>>8;
      c2 = m;
      if (!read_write(confd, &c1, 1, 0) ||
	  !read_write(confd, &c2, 1, 0) || 
	  !read_write(confd, packet, m, 0))
	return packet;
    }
}

static struct frec *allocate_frec(time_t now)
{
  struct frec *f;
  
  if ((f = (struct frec *)whine_malloc(sizeof(struct frec))))
    {
      f->next = daemon->frec_list;
      f->time = now;
      f->sentto = NULL;
      f->rfd4 = NULL;
      f->flags = 0;
#ifdef HAVE_IPV6
      f->rfd6 = NULL;
#endif
      daemon->frec_list = f;
    }

  return f;
}

static struct randfd *allocate_rfd(int family)
{
  static int finger = 0;
  int i;

  /* limit the number of sockets we have open to avoid starvation of 
     (eg) TFTP. Once we have a reasonable number, randomness should be OK */

  for (i = 0; i < RANDOM_SOCKS; i++)
    if (daemon->randomsocks[i].refcount == 0)
      {
	if ((daemon->randomsocks[i].fd = random_sock(family)) == -1)
	  break;
      
	daemon->randomsocks[i].refcount = 1;
	daemon->randomsocks[i].family = family;
	return &daemon->randomsocks[i];
      }

  /* No free ones or cannot get new socket, grab an existing one */
  for (i = 0; i < RANDOM_SOCKS; i++)
    {
      int j = (i+finger) % RANDOM_SOCKS;
      if (daemon->randomsocks[j].refcount != 0 &&
	  daemon->randomsocks[j].family == family && 
	  daemon->randomsocks[j].refcount != 0xffff)
	{
	  finger = j;
	  daemon->randomsocks[j].refcount++;
	  return &daemon->randomsocks[j];
	}
    }

  return NULL; /* doom */
}

static void free_frec(struct frec *f)
{
  if (f->rfd4 && --(f->rfd4->refcount) == 0)
    close(f->rfd4->fd);
    
  f->rfd4 = NULL;
  f->sentto = NULL;
  f->flags = 0;
  
#ifdef HAVE_IPV6
  if (f->rfd6 && --(f->rfd6->refcount) == 0)
    close(f->rfd6->fd);
    
  f->rfd6 = NULL;
#endif
}

/* if wait==NULL return a free or older than TIMEOUT record.
   else return *wait zero if one available, or *wait is delay to
   when the oldest in-use record will expire. Impose an absolute
   limit of 4*TIMEOUT before we wipe things (for random sockets) */
struct frec *get_new_frec(time_t now, int *wait)
{
  struct frec *f, *oldest, *target;
  int count;
  
  if (wait)
    *wait = 0;

  for (f = daemon->frec_list, oldest = NULL, target =  NULL, count = 0; f; f = f->next, count++)
    if (!f->sentto)
      target = f;
    else 
      {
	if (difftime(now, f->time) >= 4*TIMEOUT)
	  {
	    free_frec(f);
	    target = f;
	  }
	
	if (!oldest || difftime(f->time, oldest->time) <= 0)
	  oldest = f;
      }

  if (target)
    {
      target->time = now;
      return target;
    }
  
  /* can't find empty one, use oldest if there is one
     and it's older than timeout */
  if (oldest && ((int)difftime(now, oldest->time)) >= TIMEOUT)
    { 
      /* keep stuff for twice timeout if we can by allocating a new
	 record instead */
      if (difftime(now, oldest->time) < 2*TIMEOUT && 
	  count <= daemon->ftabsize &&
	  (f = allocate_frec(now)))
	return f;

      if (!wait)
	{
	  free_frec(oldest);
	  oldest->time = now;
	}
      return oldest;
    }
  
  /* none available, calculate time 'till oldest record expires */
  if (count > daemon->ftabsize)
    {
      if (oldest && wait)
	*wait = oldest->time + (time_t)TIMEOUT - now;
      return NULL;
    }
  
  if (!(f = allocate_frec(now)) && wait)
    /* wait one second on malloc failure */
    *wait = 1;

  return f; /* OK if malloc fails and this is NULL */
}
 
/* crc is all-ones if not known. */
static struct frec *lookup_frec(unsigned short id, unsigned int crc)
{
  struct frec *f;

  for(f = daemon->frec_list; f; f = f->next)
    if (f->sentto && f->new_id == id && 
	(f->crc == crc || crc == 0xffffffff))
      return f;
      
  return NULL;
}

static struct frec *lookup_frec_by_sender(unsigned short id,
					  union mysockaddr *addr,
					  unsigned int crc)
{
  struct frec *f;
  
  for(f = daemon->frec_list; f; f = f->next)
    if (f->sentto &&
	f->orig_id == id && 
	f->crc == crc &&
	sockaddr_isequal(&f->source, addr))
      return f;
   
  return NULL;
}

/* A server record is going away, remove references to it */
void server_gone(struct server *server)
{
  struct frec *f;
  
  for (f = daemon->frec_list; f; f = f->next)
    if (f->sentto && f->sentto == server)
      free_frec(f);
  
  if (daemon->last_server == server)
    daemon->last_server = NULL;

  if (daemon->srv_save == server)
    daemon->srv_save = NULL;
}

/* return unique random ids. */
static unsigned short get_id(unsigned int crc)
{
  unsigned short ret = 0;
  
  do 
    ret = rand16();
  while (lookup_frec(ret, crc));
  
  return ret;
}





