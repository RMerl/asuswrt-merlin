/* dnsmasq is Copyright (c) 2000-2017 Simon Kelley

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

static struct frec *lookup_frec(unsigned short id, void *hash);
static struct frec *lookup_frec_by_sender(unsigned short id,
					  union mysockaddr *addr,
					  void *hash);
static unsigned short get_id(void);
static void free_frec(struct frec *f);

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
  
  while (retry_send(sendmsg(fd, &msg, 0)));

  /* If interface is still in DAD, EINVAL results - ignore that. */
  if (errno != 0 && errno != EINVAL)
    {
      my_syslog(LOG_ERR, _("failed to send packet: %s"), strerror(errno));
      return 0;
    }
  
  return 1;
}
          
static unsigned int search_servers(time_t now, struct all_addr **addrpp, unsigned int qtype,
				   char *qdomain, int *type, char **domain, int *norebind)
			      
{
  /* If the query ends in the domain in one of our servers, set
     domain to point to that name. We find the largest match to allow both
     domain.org and sub.domain.org to exist. */
  
  unsigned int namelen = strlen(qdomain);
  unsigned int matchlen = 0;
  struct server *serv;
  unsigned int flags = 0;
  
  for (serv = daemon->servers; serv; serv=serv->next)
    if (qtype == F_DNSSECOK && !(serv->flags & SERV_DO_DNSSEC))
      continue;
    /* domain matches take priority over NODOTS matches */
    else if ((serv->flags & SERV_FOR_NODOTS) && *type != SERV_HAS_DOMAIN && !strchr(qdomain, '.') && namelen != 0)
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
	    if ((serv->flags & SERV_NO_REBIND) && norebind)	
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
		    *type = serv->flags & (SERV_HAS_DOMAIN | SERV_USE_RESOLV | SERV_NO_REBIND | SERV_DO_DNSSEC);
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
  
  if (flags == 0 && !(qtype & (F_QUERY | F_DNSSECOK)) && 
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
			 struct dns_header *header, size_t plen, time_t now, 
			 struct frec *forward, int ad_reqd, int do_bit)
{
  char *domain = NULL;
  int type = SERV_DO_DNSSEC, norebind = 0;
  struct all_addr *addrp = NULL;
  unsigned int flags = 0;
  struct server *start = NULL;
#ifdef HAVE_DNSSEC
  void *hash = hash_questions(header, plen, daemon->namebuff);
  int do_dnssec = 0;
#else
  unsigned int crc = questions_crc(header, plen, daemon->namebuff);
  void *hash = &crc;
#endif
 unsigned int gotname = extract_request(header, plen, daemon->namebuff, NULL);

 (void)do_bit;

  /* may be no servers available. */
  if (forward || (hash && (forward = lookup_frec_by_sender(ntohs(header->id), udpaddr, hash))))
    {
      /* If we didn't get an answer advertising a maximal packet in EDNS,
	 fall back to 1280, which should work everywhere on IPv6.
	 If that generates an answer, it will become the new default
	 for this server */
      forward->flags |= FREC_TEST_PKTSZ;
      
#ifdef HAVE_DNSSEC
      /* If we've already got an answer to this query, but we're awaiting keys for validation,
	 there's no point retrying the query, retry the key query instead...... */
      if (forward->blocking_query)
	{
	  int fd, is_sign;
	  unsigned char *pheader;
	  
	  forward->flags &= ~FREC_TEST_PKTSZ;
	  
	  while (forward->blocking_query)
	    forward = forward->blocking_query;
	   
	  forward->flags |= FREC_TEST_PKTSZ;
	  
	  blockdata_retrieve(forward->stash, forward->stash_len, (void *)header);
	  plen = forward->stash_len;
	  
	  if (find_pseudoheader(header, plen, NULL, &pheader, &is_sign, NULL) && !is_sign)
	    PUTSHORT(SAFE_PKTSZ, pheader);

	  if (forward->sentto->addr.sa.sa_family == AF_INET) 
	    log_query(F_NOEXTRA | F_DNSSEC | F_IPV4, "retry", (struct all_addr *)&forward->sentto->addr.in.sin_addr, "dnssec");
#ifdef HAVE_IPV6
	  else
	    log_query(F_NOEXTRA | F_DNSSEC | F_IPV6, "retry", (struct all_addr *)&forward->sentto->addr.in6.sin6_addr, "dnssec");
#endif
  
	  if (forward->sentto->sfd)
	    fd = forward->sentto->sfd->fd;
	  else
	    {
#ifdef HAVE_IPV6
	      if (forward->sentto->addr.sa.sa_family == AF_INET6)
		fd = forward->rfd6->fd;
	      else
#endif
		fd = forward->rfd4->fd;
	    }
	  
	  while (retry_send( sendto(fd, (char *)header, plen, 0,
				    &forward->sentto->addr.sa,
				    sa_len(&forward->sentto->addr))));
	  
	  return 1;
	}
#endif

      /* retry on existing query, send to all available servers  */
      domain = forward->sentto->domain;
      forward->sentto->failed_queries++;
      if (!option_bool(OPT_ORDER))
	{
	  forward->forwardall = 1;
	  daemon->last_server = NULL;
	}
      type = forward->sentto->flags & SERV_TYPE;
#ifdef HAVE_DNSSEC
      do_dnssec = forward->sentto->flags & SERV_DO_DNSSEC;
#endif

      if (!(start = forward->sentto->next))
	start = daemon->servers; /* at end of list, recycle */
      header->id = htons(forward->new_id);
    }
  else 
    {
      if (gotname)
	flags = search_servers(now, &addrp, gotname, daemon->namebuff, &type, &domain, &norebind);
      
#ifdef HAVE_DNSSEC
      do_dnssec = type & SERV_DO_DNSSEC;
#endif
      type &= ~SERV_DO_DNSSEC;      

      if (daemon->servers && !flags)
	forward = get_new_frec(now, NULL, 0);
      /* table full - flags == 0, return REFUSED */
      
      if (forward)
	{
	  forward->source = *udpaddr;
	  forward->dest = *dst_addr;
	  forward->iface = dst_iface;
	  forward->orig_id = ntohs(header->id);
	  forward->new_id = get_id();
	  forward->fd = udpfd;
	  memcpy(forward->hash, hash, HASH_SIZE);
	  forward->forwardall = 0;
	  forward->flags = 0;
	  if (norebind)
	    forward->flags |= FREC_NOREBIND;
	  if (header->hb4 & HB4_CD)
	    forward->flags |= FREC_CHECKING_DISABLED;
	  if (ad_reqd)
	    forward->flags |= FREC_AD_QUESTION;
#ifdef HAVE_DNSSEC
	  forward->work_counter = DNSSEC_WORK;
	  if (do_bit)
	    forward->flags |= FREC_DO_QUESTION;
#endif
	  
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
      int subnet, forwarded = 0;
      size_t edns0_len;

      /* If a query is retried, use the log_id for the retry when logging the answer. */
      forward->log_id = daemon->log_id;
      
      edns0_len  = add_edns0_config(header, plen, ((unsigned char *)header) + PACKETSZ, &forward->source, now, &subnet);
      
      if (edns0_len != plen)
	{
	  plen = edns0_len;
	  forward->flags |= FREC_ADDED_PHEADER;
	  
	  if (subnet)
	    forward->flags |= FREC_HAS_SUBNET;
	}
      
#ifdef HAVE_DNSSEC
      if (option_bool(OPT_DNSSEC_VALID) && do_dnssec)
	{
	  size_t new = add_do_bit(header, plen, ((unsigned char *) header) + PACKETSZ);
	 
	  if (new != plen)
	    forward->flags |= FREC_ADDED_PHEADER;

	  plen = new;
	      
	  /* For debugging, set Checking Disabled, otherwise, have the upstream check too,
	     this allows it to select auth servers when one is returning bad data. */
	  if (option_bool(OPT_DNSSEC_DEBUG))
	    header->hb4 |= HB4_CD;

	}
#endif

      /* If we're sending an EDNS0 with any options, we can't recreate the query from a reply. */
      if (find_pseudoheader(header, plen, &edns0_len, NULL, NULL, NULL) && edns0_len > 11)
	forward->flags |= FREC_HAS_EXTRADATA;
      
      while (1)
	{ 
	  /* only send to servers dealing with our domain.
	     domain may be NULL, in which case server->domain 
	     must be NULL also. */
	  
	  if (type == (start->flags & SERV_TYPE) &&
	      (type != SERV_HAS_DOMAIN || hostname_isequal(domain, start->domain)) &&
	      !(start->flags & (SERV_LITERAL_ADDRESS | SERV_LOOP)))
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
		      if (get_incoming_mark(&forward->source, &forward->dest, 0, &mark))
			setsockopt(fd, SOL_SOCKET, SO_MARK, &mark, sizeof(unsigned int));
		    }
#endif
		}
	      
#ifdef HAVE_DNSSEC
	      if (option_bool(OPT_DNSSEC_VALID) && (forward->flags & FREC_ADDED_PHEADER))
		{
		  /* Difficult one here. If our client didn't send EDNS0, we will have set the UDP
		     packet size to 512. But that won't provide space for the RRSIGS in many cases.
		     The RRSIGS will be stripped out before the answer goes back, so the packet should
		     shrink again. So, if we added a do-bit, bump the udp packet size to the value
		     known to be OK for this server. We check returned size after stripping and set
		     the truncated bit if it's still too big. */		  
		  unsigned char *pheader;
		  int is_sign;
		  if (find_pseudoheader(header, plen, NULL, &pheader, &is_sign, NULL) && !is_sign)
		    PUTSHORT(start->edns_pktsz, pheader);
		}
#endif

	      if (retry_send(sendto(fd, (char *)header, plen, 0,
				    &start->addr.sa,
				    sa_len(&start->addr))))
		continue;
	    
	      if (errno == 0)
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

static size_t process_reply(struct dns_header *header, time_t now, struct server *server, size_t n, int check_rebind, 
			    int no_cache, int cache_secure, int bogusanswer, int ad_reqd, int do_bit, int added_pheader, 
			    int check_subnet, union mysockaddr *query_source)
{
  unsigned char *pheader, *sizep;
  char **sets = 0;
  int munged = 0, is_sign;
  size_t plen; 

  (void)ad_reqd;
  (void)do_bit;
  (void)bogusanswer;

#ifdef HAVE_IPSET
  if (daemon->ipsets && extract_request(header, n, daemon->namebuff, NULL))
    {
      /* Similar algorithm to search_servers. */
      struct ipsets *ipset_pos;
      unsigned int namelen = strlen(daemon->namebuff);
      unsigned int matchlen = 0;
      for (ipset_pos = daemon->ipsets; ipset_pos; ipset_pos = ipset_pos->next) 
	{
	  unsigned int domainlen = strlen(ipset_pos->domain);
	  char *matchstart = daemon->namebuff + namelen - domainlen;
	  if (namelen >= domainlen && hostname_isequal(matchstart, ipset_pos->domain) &&
	      (domainlen == 0 || namelen == domainlen || *(matchstart - 1) == '.' ) &&
	      domainlen >= matchlen) 
	    {
	      matchlen = domainlen;
	      sets = ipset_pos->sets;
	    }
	}
    }
#endif
  
  if ((pheader = find_pseudoheader(header, n, &plen, &sizep, &is_sign, NULL)))
    {
      if (check_subnet && !check_source(header, plen, pheader, query_source))
	{
	  my_syslog(LOG_WARNING, _("discarding DNS reply: subnet option mismatch"));
	  return 0;
	}
      
      if (!is_sign)
	{
	  if (added_pheader)
	    {
	      /* client didn't send EDNS0, we added one, strip it off before returning answer. */
	      n = rrfilter(header, n, 0);
	      pheader = NULL;
	    }
	  else
	    {
	      unsigned short udpsz;

	      /* If upstream is advertising a larger UDP packet size
		 than we allow, trim it so that we don't get overlarge
		 requests for the client. We can't do this for signed packets. */
	      GETSHORT(udpsz, sizep);
	      if (udpsz > daemon->edns_pktsz)
		{
		  sizep -= 2;
		  PUTSHORT(daemon->edns_pktsz, sizep);
		}

#ifdef HAVE_DNSSEC
	      /* If the client didn't set the do bit, but we did, reset it. */
	      if (option_bool(OPT_DNSSEC_VALID) && !do_bit)
		{
		  unsigned short flags;
		  sizep += 2; /* skip RCODE */
		  GETSHORT(flags, sizep);
		  flags &= ~0x8000;
		  sizep -= 2;
		  PUTSHORT(flags, sizep);
		}
#endif
	    }
	}
    }
  
  /* RFC 4035 sect 4.6 para 3 */
  if (!is_sign && !option_bool(OPT_DNSSEC_PROXY))
     header->hb4 &= ~HB4_AD;
  
  if (OPCODE(header) != QUERY || (RCODE(header) != NOERROR && RCODE(header) != NXDOMAIN))
    return resize_packet(header, n, pheader, plen);
  
  /* Complain loudly if the upstream server is non-recursive. */
  if (!(header->hb4 & HB4_RA) && RCODE(header) == NOERROR &&
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
      cache_secure = 0;
    }
  else 
    {
      int doctored = 0;
      
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
	  cache_secure = 0;
	}
      
      if (extract_addresses(header, n, daemon->namebuff, now, sets, is_sign, check_rebind, no_cache, cache_secure, &doctored))
	{
	  my_syslog(LOG_WARNING, _("possible DNS-rebind attack detected: %s"), daemon->namebuff);
	  munged = 1;
	  cache_secure = 0;
	}

      if (doctored)
	cache_secure = 0;
    }
  
#ifdef HAVE_DNSSEC
  if (bogusanswer && !(header->hb4 & HB4_CD) && !option_bool(OPT_DNSSEC_DEBUG))
    {
      /* Bogus reply, turn into SERVFAIL */
      SET_RCODE(header, SERVFAIL);
      munged = 1;
    }

  if (option_bool(OPT_DNSSEC_VALID))
    {
      header->hb4 &= ~HB4_AD;
      
      if (!(header->hb4 & HB4_CD) && ad_reqd && cache_secure)
	header->hb4 |= HB4_AD;
      
      /* If the requestor didn't set the DO bit, don't return DNSSEC info. */
      if (!do_bit)
	n = rrfilter(header, n, 1);
    }
#endif

  /* do this after extract_addresses. Ensure NODATA reply and remove
     nameserver info. */
  
  if (munged)
    {
      header->ancount = htons(0);
      header->nscount = htons(0);
      header->arcount = htons(0);
      header->hb3 &= ~HB3_TC;
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
  ssize_t n = recvfrom(fd, daemon->packet, daemon->packet_buff_sz, 0, &serveraddr.sa, &addrlen);
  size_t nn;
  struct server *server;
  void *hash;
#ifndef HAVE_DNSSEC
  unsigned int crc;
#endif

  /* packet buffer overwritten */
  daemon->srv_save = NULL;
  
  /* Determine the address of the server replying  so that we can mark that as good */
  serveraddr.sa.sa_family = family;
#ifdef HAVE_IPV6
  if (serveraddr.sa.sa_family == AF_INET6)
    serveraddr.in6.sin6_flowinfo = 0;
#endif
  
  header = (struct dns_header *)daemon->packet;
  
  if (n < (int)sizeof(struct dns_header) || !(header->hb3 & HB3_QR))
    return;
  
  /* spoof check: answer must come from known server, */
  for (server = daemon->servers; server; server = server->next)
    if (!(server->flags & (SERV_LITERAL_ADDRESS | SERV_NO_ADDR)) &&
	sockaddr_isequal(&server->addr, &serveraddr))
      break;
  
  if (!server)
    return;
  
#ifdef HAVE_DNSSEC
  hash = hash_questions(header, n, daemon->namebuff);
#else
  hash = &crc;
  crc = questions_crc(header, n, daemon->namebuff);
#endif
  
  if (!(forward = lookup_frec(ntohs(header->id), hash)))
    return;
  
  /* log_query gets called indirectly all over the place, so 
     pass these in global variables - sorry. */
  daemon->log_display_id = forward->log_id;
  daemon->log_source_addr = &forward->source;
  
  if (daemon->ignore_addr && RCODE(header) == NOERROR &&
      check_for_ignored_address(header, n, daemon->ignore_addr))
    return;

  /* Note: if we send extra options in the EDNS0 header, we can't recreate
     the query from the reply. */
  if (RCODE(header) == REFUSED &&
      forward->forwardall == 0 &&
      !(forward->flags & FREC_HAS_EXTRADATA))
    /* for broken servers, attempt to send to another one. */
    {
      unsigned char *pheader;
      size_t plen;
      int is_sign;
      
      /* recreate query from reply */
      pheader = find_pseudoheader(header, (size_t)n, &plen, NULL, &is_sign, NULL);
      if (!is_sign)
	{
	  header->ancount = htons(0);
	  header->nscount = htons(0);
	  header->arcount = htons(0);
	  if ((nn = resize_packet(header, (size_t)n, pheader, plen)))
	    {
	      header->hb3 &= ~(HB3_QR | HB3_AA | HB3_TC);
	      header->hb4 &= ~(HB4_RA | HB4_RCODE | HB4_CD | HB4_AD);
	      if (forward->flags & FREC_CHECKING_DISABLED)
		header->hb4 |= HB4_CD;
	      if (forward->flags & FREC_AD_QUESTION)
		header->hb4 |= HB4_AD;
	      if (forward->flags & FREC_DO_QUESTION)
		add_do_bit(header, nn,  (unsigned char *)pheader + plen);
	      forward_query(-1, NULL, NULL, 0, header, nn, now, forward, forward->flags & FREC_AD_QUESTION, forward->flags & FREC_DO_QUESTION);
	      return;
	    }
	}
    }   
   
  server = forward->sentto;
  if ((forward->sentto->flags & SERV_TYPE) == 0)
    {
      if (RCODE(header) == REFUSED)
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
 
  /* We tried resending to this server with a smaller maximum size and got an answer.
     Make that permanent. To avoid reduxing the packet size for an single dropped packet,
     only do this when we get a truncated answer, or one larger than the safe size. */
  if (server && (forward->flags & FREC_TEST_PKTSZ) && 
      ((header->hb3 & HB3_TC) || n >= SAFE_PKTSZ))
    server->edns_pktsz = SAFE_PKTSZ;
  
  /* If the answer is an error, keep the forward record in place in case
     we get a good reply from another server. Kill it when we've
     had replies from all to avoid filling the forwarding table when
     everything is broken */
  if (forward->forwardall == 0 || --forward->forwardall == 1 ||
      (RCODE(header) != REFUSED && RCODE(header) != SERVFAIL))
    {
      int check_rebind = 0, no_cache_dnssec = 0, cache_secure = 0, bogusanswer = 0;

      if (option_bool(OPT_NO_REBIND))
	check_rebind = !(forward->flags & FREC_NOREBIND);
      
      /*   Don't cache replies where DNSSEC validation was turned off, either
	   the upstream server told us so, or the original query specified it.  */
      if ((header->hb4 & HB4_CD) || (forward->flags & FREC_CHECKING_DISABLED))
	no_cache_dnssec = 1;
      
#ifdef HAVE_DNSSEC
      if (server && (server->flags & SERV_DO_DNSSEC) && 
	  option_bool(OPT_DNSSEC_VALID) && !(forward->flags & FREC_CHECKING_DISABLED))
	{
	  int status = 0;

	  /* We've had a reply already, which we're validating. Ignore this duplicate */
	  if (forward->blocking_query)
	    return;
	  
	   /* Truncated answer can't be validated.
	      If this is an answer to a DNSSEC-generated query, we still
	      need to get the client to retry over TCP, so return
	      an answer with the TC bit set, even if the actual answer fits.
	   */
	  if (header->hb3 & HB3_TC)
	    status = STAT_TRUNCATED;
	  
	  while (1)
	    {
	      /* As soon as anything returns BOGUS, we stop and unwind, to do otherwise
		 would invite infinite loops, since the answers to DNSKEY and DS queries
		 will not be cached, so they'll be repeated. */
	      if (status != STAT_BOGUS && status != STAT_TRUNCATED && status != STAT_ABANDONED)
		{
		  if (forward->flags & FREC_DNSKEY_QUERY)
		    status = dnssec_validate_by_ds(now, header, n, daemon->namebuff, daemon->keyname, forward->class);
		  else if (forward->flags & FREC_DS_QUERY)
		    status = dnssec_validate_ds(now, header, n, daemon->namebuff, daemon->keyname, forward->class);
		  else
		    status = dnssec_validate_reply(now, header, n, daemon->namebuff, daemon->keyname, &forward->class, 
						   option_bool(OPT_DNSSEC_NO_SIGN) && (server->flags & SERV_DO_DNSSEC), NULL, NULL);
		}
	      
	      /* Can't validate, as we're missing key data. Put this
		 answer aside, whilst we get that. */     
	      if (status == STAT_NEED_DS || status == STAT_NEED_KEY)
		{
		  struct frec *new, *orig;
		  
		  /* Free any saved query */
		  if (forward->stash)
		    blockdata_free(forward->stash);
		  
		  /* Now save reply pending receipt of key data */
		  if (!(forward->stash = blockdata_alloc((char *)header, n)))
		    return;
		  forward->stash_len = n;
		  
		  /* Find the original query that started it all.... */
		  for (orig = forward; orig->dependent; orig = orig->dependent);
		  
		  if (--orig->work_counter == 0 || !(new = get_new_frec(now, NULL, 1)))
		    status = STAT_ABANDONED;
		  else
		    {
		      int fd, type = SERV_DO_DNSSEC;
		      struct frec *next = new->next;
		      char *domain;
		      
		      *new = *forward; /* copy everything, then overwrite */
		      new->next = next;
		      new->blocking_query = NULL;

		      /* Find server to forward to. This will normally be the 
			 same as for the original query, but may be another if
			 servers for domains are involved. */		      
		      if (search_servers(now, NULL, F_DNSSECOK, daemon->keyname, &type, &domain, NULL) == 0)
			{
			  struct server *start = server, *new_server = NULL;
			  
			  while (1)
			    {
			      if (type == (start->flags & (SERV_TYPE | SERV_DO_DNSSEC)) &&
				  (type != SERV_HAS_DOMAIN || hostname_isequal(domain, start->domain)) &&
				  !(start->flags & (SERV_LITERAL_ADDRESS | SERV_LOOP)))
				{
				  new_server = start;
				  if (server == start)
				    {
				      new_server = NULL;
				      break;
				    }
				}
			      
			      if (!(start = start->next))
				start = daemon->servers;
			      if (start == server)
				break;
			    }
			  
			  if (new_server)
			    server = new_server;
			}
		      
		      new->sentto = server;
		      new->rfd4 = NULL;
#ifdef HAVE_IPV6
		      new->rfd6 = NULL;
#endif
		      new->flags &= ~(FREC_DNSKEY_QUERY | FREC_DS_QUERY);
		      
		      new->dependent = forward; /* to find query awaiting new one. */
		      forward->blocking_query = new; /* for garbage cleaning */
		      /* validate routines leave name of required record in daemon->keyname */
		      if (status == STAT_NEED_KEY)
			{
			  new->flags |= FREC_DNSKEY_QUERY; 
			  nn = dnssec_generate_query(header, ((unsigned char *) header) + server->edns_pktsz,
						     daemon->keyname, forward->class, T_DNSKEY, &server->addr, server->edns_pktsz);
			}
		      else 
			{
			  new->flags |= FREC_DS_QUERY;
			  nn = dnssec_generate_query(header,((unsigned char *) header) + server->edns_pktsz,
						     daemon->keyname, forward->class, T_DS, &server->addr, server->edns_pktsz);
			}
		      if ((hash = hash_questions(header, nn, daemon->namebuff)))
			memcpy(new->hash, hash, HASH_SIZE);
		      new->new_id = get_id();
		      header->id = htons(new->new_id);
		      /* Save query for retransmission */
		      new->stash = blockdata_alloc((char *)header, nn);
		      new->stash_len = nn;
		      
		      /* Don't resend this. */
		      daemon->srv_save = NULL;
		      
		      if (server->sfd)
			fd = server->sfd->fd;
		      else
			{
			  fd = -1;
#ifdef HAVE_IPV6
			  if (server->addr.sa.sa_family == AF_INET6)
			    {
			      if (new->rfd6 || (new->rfd6 = allocate_rfd(AF_INET6)))
				fd = new->rfd6->fd;
			    }
			  else
#endif
			    {
			      if (new->rfd4 || (new->rfd4 = allocate_rfd(AF_INET)))
				fd = new->rfd4->fd;
			    }
			}
		      
		      if (fd != -1)
			{
#ifdef HAVE_CONNTRACK
			  /* Copy connection mark of incoming query to outgoing connection. */
			  if (option_bool(OPT_CONNTRACK))
			    {
			      unsigned int mark;
			      if (get_incoming_mark(&orig->source, &orig->dest, 0, &mark))
				setsockopt(fd, SOL_SOCKET, SO_MARK, &mark, sizeof(unsigned int));
			    }
#endif
			  while (retry_send(sendto(fd, (char *)header, nn, 0, 
						   &server->addr.sa, 
						   sa_len(&server->addr)))); 
			  server->queries++;
			}
		    }		  
		  return;
		}
	  
	      /* Validated original answer, all done. */
	      if (!forward->dependent)
		break;
	      
	      /* validated subsidiary query, (and cached result)
		 pop that and return to the previous query we were working on. */
	      struct frec *prev = forward->dependent;
	      free_frec(forward);
	      forward = prev;
	      forward->blocking_query = NULL; /* already gone */
	      blockdata_retrieve(forward->stash, forward->stash_len, (void *)header);
	      n = forward->stash_len;
	    }
	
	  
	  no_cache_dnssec = 0;
	  
	  if (status == STAT_TRUNCATED)
	    header->hb3 |= HB3_TC;
	  else
	    {
	      char *result, *domain = "result";
	      
	      if (status == STAT_ABANDONED)
		{
		  result = "ABANDONED";
		  status = STAT_BOGUS;
		}
	      else
		result = (status == STAT_SECURE ? "SECURE" : (status == STAT_INSECURE ? "INSECURE" : "BOGUS"));
	      
	      if (status == STAT_BOGUS && extract_request(header, n, daemon->namebuff, NULL))
		domain = daemon->namebuff;
	      
	      log_query(F_KEYTAG | F_SECSTAT, domain, NULL, result);
	    }
	  
	  if (status == STAT_SECURE)
	    cache_secure = 1;
	  else if (status == STAT_BOGUS)
	    {
	      no_cache_dnssec = 1;
	      bogusanswer = 1;
	    }
	}
#endif     
      
      /* restore CD bit to the value in the query */
      if (forward->flags & FREC_CHECKING_DISABLED)
	header->hb4 |= HB4_CD;
      else
	header->hb4 &= ~HB4_CD;
      
      if ((nn = process_reply(header, now, forward->sentto, (size_t)n, check_rebind, no_cache_dnssec, cache_secure, bogusanswer, 
			      forward->flags & FREC_AD_QUESTION, forward->flags & FREC_DO_QUESTION, 
			      forward->flags & FREC_ADDED_PHEADER, forward->flags & FREC_HAS_SUBNET, &forward->source)))
	{
	  header->id = htons(forward->orig_id);
	  header->hb4 |= HB4_RA; /* recursion if available */
#ifdef HAVE_DNSSEC
	  /* We added an EDNSO header for the purpose of getting DNSSEC RRs, and set the value of the UDP payload size
	     greater than the no-EDNS0-implied 512 to have if space for the RRSIGS. If, having stripped them and the EDNS0
             header, the answer is still bigger than 512, truncate it and mark it so. The client then retries with TCP. */
	  if (option_bool(OPT_DNSSEC_VALID) && (forward->flags & FREC_ADDED_PHEADER) && (nn > PACKETSZ))
	    {
	      header->ancount = htons(0);
	      header->nscount = htons(0);
	      header->arcount = htons(0);
	      header->hb3 |= HB3_TC;
	      nn = resize_packet(header, nn, NULL, 0);
	    }
#endif
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
  unsigned char *pheader;
  unsigned short type, udp_size = PACKETSZ; /* default if no EDNS0 */
  struct all_addr dst_addr;
  struct in_addr netmask, dst_addr_4;
  size_t m;
  ssize_t n;
  int if_index = 0, auth_dns = 0, do_bit = 0, have_pseudoheader = 0;
#ifdef HAVE_AUTH
  int local_auth = 0;
#endif
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
#ifdef HAVE_IPV6
   /* Can always get recvd interface for IPv6 */
  int check_dst = !option_bool(OPT_NOWILD) || listen->family == AF_INET6;
#else
  int check_dst = !option_bool(OPT_NOWILD);
#endif

  /* packet buffer overwritten */
  daemon->srv_save = NULL;
  
  dst_addr_4.s_addr = dst_addr.addr.addr4.s_addr = 0;
  netmask.s_addr = 0;
  
  if (option_bool(OPT_NOWILD) && listen->iface)
    {
      auth_dns = listen->iface->dns_auth;
     
      if (listen->family == AF_INET)
	{
	  dst_addr_4 = dst_addr.addr.addr4 = listen->iface->addr.in.sin_addr;
	  netmask = listen->iface->netmask;
	}
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

  /* Clear buffer beyond request to avoid risk of
     information disclosure. */
  memset(daemon->packet + n, 0, daemon->edns_pktsz - n);
  
  source_addr.sa.sa_family = listen->family;
  
  if (listen->family == AF_INET)
    {
       /* Source-port == 0 is an error, we can't send back to that. 
	  http://www.ietf.org/mail-archive/web/dnsop/current/msg11441.html */
      if (source_addr.in.sin_port == 0)
	return;
    }
#ifdef HAVE_IPV6
  else
    {
      /* Source-port == 0 is an error, we can't send back to that. */
      if (source_addr.in6.sin6_port == 0)
	return;
      source_addr.in6.sin6_flowinfo = 0;
    }
#endif
  
  /* We can be configured to only accept queries from at-most-one-hop-away addresses. */
  if (option_bool(OPT_LOCAL_SERVICE))
    {
      struct addrlist *addr;
#ifdef HAVE_IPV6
      if (listen->family == AF_INET6) 
	{
	  for (addr = daemon->interface_addrs; addr; addr = addr->next)
	    if ((addr->flags & ADDRLIST_IPV6) &&
		is_same_net6(&addr->addr.addr.addr6, &source_addr.in6.sin6_addr, addr->prefixlen))
	      break;
	}
      else
#endif
	{
	  struct in_addr netmask;
	  for (addr = daemon->interface_addrs; addr; addr = addr->next)
	    {
	      netmask.s_addr = htonl(~(in_addr_t)0 << (32 - addr->prefixlen));
	      if (!(addr->flags & ADDRLIST_IPV6) &&
		  is_same_net(addr->addr.addr.addr4, source_addr.in.sin_addr, netmask))
		break;
	    }
	}
      if (!addr)
	{
	  static int warned = 0;
	  if (!warned)
	    {
	      my_syslog(LOG_WARNING, _("Ignoring query from non-local network"));
	      warned = 1;
	    }
	  return;
	}
    }
		
  if (check_dst)
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
      
      if (!indextoname(listen->fd, if_index, ifr.ifr_name))
	return;
      
      if (!iface_check(listen->family, &dst_addr, ifr.ifr_name, &auth_dns))
	{
	   if (!option_bool(OPT_CLEVERBIND))
	     enumerate_interfaces(0); 
	   if (!loopback_exception(listen->fd, listen->family, &dst_addr, ifr.ifr_name) &&
	       !label_exception(if_index, listen->family, &dst_addr))
	     return;
	}

      if (listen->family == AF_INET && option_bool(OPT_LOCALISE))
	{
	  struct irec *iface;
	  
	  /* get the netmask of the interface which has the address we were sent to.
	     This is no necessarily the interface we arrived on. */
	  
	  for (iface = daemon->interfaces; iface; iface = iface->next)
	    if (iface->addr.sa.sa_family == AF_INET &&
		iface->addr.in.sin_addr.s_addr == dst_addr_4.s_addr)
	      break;
	  
	  /* interface may be new */
	  if (!iface && !option_bool(OPT_CLEVERBIND))
	    enumerate_interfaces(0); 
	  
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
   
  /* log_query gets called indirectly all over the place, so 
     pass these in global variables - sorry. */
  daemon->log_display_id = ++daemon->log_id;
  daemon->log_source_addr = &source_addr;
  
  if (extract_request(header, (size_t)n, daemon->namebuff, &type))
    {
#ifdef HAVE_AUTH
      struct auth_zone *zone;
#endif
      char *types = querystr(auth_dns ? "auth" : "query", type);
      
      if (listen->family == AF_INET) 
	log_query(F_QUERY | F_IPV4 | F_FORWARD, daemon->namebuff, 
		  (struct all_addr *)&source_addr.in.sin_addr, types);
#ifdef HAVE_IPV6
      else
	log_query(F_QUERY | F_IPV6 | F_FORWARD, daemon->namebuff, 
		  (struct all_addr *)&source_addr.in6.sin6_addr, types);
#endif

#ifdef HAVE_AUTH
      /* find queries for zones we're authoritative for, and answer them directly */
      if (!auth_dns && !option_bool(OPT_LOCALISE))
	for (zone = daemon->auth_zones; zone; zone = zone->next)
	  if (in_zone(zone, daemon->namebuff, NULL))
	    {
	      auth_dns = 1;
	      local_auth = 1;
	      break;
	    }
#endif
      
#ifdef HAVE_LOOP
      /* Check for forwarding loop */
      if (detect_loop(daemon->namebuff, type))
	return;
#endif
    }
  
  if (find_pseudoheader(header, (size_t)n, NULL, &pheader, NULL, NULL))
    { 
      unsigned short flags;
      
      have_pseudoheader = 1;
      GETSHORT(udp_size, pheader);
      pheader += 2; /* ext_rcode */
      GETSHORT(flags, pheader);
      
      if (flags & 0x8000)
	do_bit = 1;/* do bit */ 
	
      /* If the client provides an EDNS0 UDP size, use that to limit our reply.
	 (bounded by the maximum configured). If no EDNS0, then it
	 defaults to 512 */
      if (udp_size > daemon->edns_pktsz)
	udp_size = daemon->edns_pktsz;
      else if (udp_size < PACKETSZ)
	udp_size = PACKETSZ; /* Sanity check - can't reduce below default. RFC 6891 6.2.3 */
    }

#ifdef HAVE_AUTH
  if (auth_dns)
    {
      m = answer_auth(header, ((char *) header) + udp_size, (size_t)n, now, &source_addr, 
		      local_auth, do_bit, have_pseudoheader);
      if (m >= 1)
	{
	  send_from(listen->fd, option_bool(OPT_NOWILD) || option_bool(OPT_CLEVERBIND),
		    (char *)header, m, &source_addr, &dst_addr, if_index);
	  daemon->auth_answer++;
	}
    }
  else
#endif
    {
      int ad_reqd = do_bit;
       /* RFC 6840 5.7 */
      if (header->hb4 & HB4_AD)
	ad_reqd = 1;

      m = answer_request(header, ((char *) header) + udp_size, (size_t)n, 
			 dst_addr_4, netmask, now, ad_reqd, do_bit, have_pseudoheader);
      
      if (m >= 1)
	{
	  send_from(listen->fd, option_bool(OPT_NOWILD) || option_bool(OPT_CLEVERBIND),
		    (char *)header, m, &source_addr, &dst_addr, if_index);
	  daemon->local_answer++;
	}
      else if (forward_query(listen->fd, &source_addr, &dst_addr, if_index,
			     header, (size_t)n, now, NULL, ad_reqd, do_bit))
	daemon->queries_forwarded++;
      else
	daemon->local_answer++;
    }
}

#ifdef HAVE_DNSSEC
/* Recurse up the key hierarchy */
static int tcp_key_recurse(time_t now, int status, struct dns_header *header, size_t n, 
			   int class, char *name, char *keyname, struct server *server, 
			   int have_mark, unsigned int mark, int *keycount)
{
  int new_status;
  unsigned char *packet = NULL;
  unsigned char *payload = NULL;
  struct dns_header *new_header = NULL;
  u16 *length = NULL;
 
  while (1)
    {
      int type = SERV_DO_DNSSEC;
      char *domain;
      size_t m; 
      unsigned char c1, c2;
      struct server *firstsendto = NULL;
      
      /* limit the amount of work we do, to avoid cycling forever on loops in the DNS */
      if (--(*keycount) == 0)
	new_status = STAT_ABANDONED;
      else if (status == STAT_NEED_KEY)
	new_status = dnssec_validate_by_ds(now, header, n, name, keyname, class);
      else if (status == STAT_NEED_DS)
	new_status = dnssec_validate_ds(now, header, n, name, keyname, class);
      else 
	new_status = dnssec_validate_reply(now, header, n, name, keyname, &class,
					   option_bool(OPT_DNSSEC_NO_SIGN) && (server->flags & SERV_DO_DNSSEC), NULL, NULL);
      
      if (new_status != STAT_NEED_DS && new_status != STAT_NEED_KEY)
	break;

      /* Can't validate because we need a key/DS whose name now in keyname.
	 Make query for same, and recurse to validate */
      if (!packet)
	{
	  packet = whine_malloc(65536 + MAXDNAME + RRFIXEDSZ + sizeof(u16));
	  payload = &packet[2];
	  new_header = (struct dns_header *)payload;
	  length = (u16 *)packet;
	}
      
      if (!packet)
	{
	  new_status = STAT_ABANDONED;
	  break;
	}
	 
      m = dnssec_generate_query(new_header, ((unsigned char *) new_header) + 65536, keyname, class, 
				new_status == STAT_NEED_KEY ? T_DNSKEY : T_DS, &server->addr, server->edns_pktsz);
      
      *length = htons(m);

      /* Find server to forward to. This will normally be the 
	 same as for the original query, but may be another if
	 servers for domains are involved. */		      
      if (search_servers(now, NULL, F_DNSSECOK, keyname, &type, &domain, NULL) != 0)
	{
	  new_status = STAT_ABANDONED;
	  break;
	}
	
      while (1)
	{
	  if (!firstsendto)
	    firstsendto = server;
	  else
	    {
	      if (!(server = server->next))
		server = daemon->servers;
	      if (server == firstsendto)
		{
		  /* can't find server to accept our query. */
		  new_status = STAT_ABANDONED;
		  break;
		}
	    }
	  
	  if (type != (server->flags & (SERV_TYPE | SERV_DO_DNSSEC)) ||
	      (type == SERV_HAS_DOMAIN && !hostname_isequal(domain, server->domain)) ||
	      (server->flags & (SERV_LITERAL_ADDRESS | SERV_LOOP)))
	    continue;
	    
	  retry:
	    /* may need to make new connection. */
	    if (server->tcpfd == -1)
	      {
		if ((server->tcpfd = socket(server->addr.sa.sa_family, SOCK_STREAM, 0)) == -1)
		  continue; /* No good, next server */
		
#ifdef HAVE_CONNTRACK
		/* Copy connection mark of incoming query to outgoing connection. */
		if (have_mark)
		  setsockopt(server->tcpfd, SOL_SOCKET, SO_MARK, &mark, sizeof(unsigned int));
#endif	
		
		if (!local_bind(server->tcpfd,  &server->source_addr, server->interface, 1) ||
		    connect(server->tcpfd, &server->addr.sa, sa_len(&server->addr)) == -1)
		  {
		    close(server->tcpfd);
		    server->tcpfd = -1;
		    continue; /* No good, next server */
		  }
		
		server->flags &= ~SERV_GOT_TCP;
	      }
	  
	  if (!read_write(server->tcpfd, packet, m + sizeof(u16), 0) ||
	      !read_write(server->tcpfd, &c1, 1, 1) ||
	      !read_write(server->tcpfd, &c2, 1, 1) ||
	      !read_write(server->tcpfd, payload, (c1 << 8) | c2, 1))
	    {
	      close(server->tcpfd);
	      server->tcpfd = -1;
	      /* We get data then EOF, reopen connection to same server,
		 else try next. This avoids DoS from a server which accepts
		 connections and then closes them. */
	      if (server->flags & SERV_GOT_TCP)
		goto retry;
	      else
		continue;
	    }
	  
	  server->flags |= SERV_GOT_TCP;
	  
	  m = (c1 << 8) | c2;
	  new_status = tcp_key_recurse(now, new_status, new_header, m, class, name, keyname, server, have_mark, mark, keycount);
	  break;
	}
      
      if (new_status != STAT_OK)
	break;
    }
    
  if (packet)
    free(packet);
    
  return new_status;
}
#endif


/* The daemon forks before calling this: it should deal with one connection,
   blocking as necessary, and then return. Note, need to be a bit careful
   about resources for debug mode, when the fork is suppressed: that's
   done by the caller. */
unsigned char *tcp_request(int confd, time_t now,
			   union mysockaddr *local_addr, struct in_addr netmask, int auth_dns)
{
  size_t size = 0;
  int norebind = 0;
#ifdef HAVE_AUTH
  int local_auth = 0;
#endif
  int checking_disabled, do_bit, added_pheader = 0, have_pseudoheader = 0;
  int check_subnet, no_cache_dnssec = 0, cache_secure = 0, bogusanswer = 0;
  size_t m;
  unsigned short qtype;
  unsigned int gotname;
  unsigned char c1, c2;
  /* Max TCP packet + slop + size */
  unsigned char *packet = whine_malloc(65536 + MAXDNAME + RRFIXEDSZ + sizeof(u16));
  unsigned char *payload = &packet[2];
  /* largest field in header is 16-bits, so this is still sufficiently aligned */
  struct dns_header *header = (struct dns_header *)payload;
  u16 *length = (u16 *)packet;
  struct server *last_server;
  struct in_addr dst_addr_4;
  union mysockaddr peer_addr;
  socklen_t peer_len = sizeof(union mysockaddr);
  int query_count = 0;
  unsigned char *pheader;
  unsigned int mark = 0;
  int have_mark = 0;

  (void)mark;
  (void)have_mark;

  if (getpeername(confd, (struct sockaddr *)&peer_addr, &peer_len) == -1)
    return packet;

#ifdef HAVE_CONNTRACK
  /* Get connection mark of incoming query to set on outgoing connections. */
  if (option_bool(OPT_CONNTRACK))
    {
      struct all_addr local;
#ifdef HAVE_IPV6		      
      if (local_addr->sa.sa_family == AF_INET6)
	local.addr.addr6 = local_addr->in6.sin6_addr;
      else
#endif
	local.addr.addr4 = local_addr->in.sin_addr;
      
      have_mark = get_incoming_mark(&peer_addr, &local, 1, &mark);
    }
#endif	

  /* We can be configured to only accept queries from at-most-one-hop-away addresses. */
  if (option_bool(OPT_LOCAL_SERVICE))
    {
      struct addrlist *addr;
#ifdef HAVE_IPV6
      if (peer_addr.sa.sa_family == AF_INET6) 
	{
	  for (addr = daemon->interface_addrs; addr; addr = addr->next)
	    if ((addr->flags & ADDRLIST_IPV6) &&
		is_same_net6(&addr->addr.addr.addr6, &peer_addr.in6.sin6_addr, addr->prefixlen))
	      break;
	}
      else
#endif
	{
	  struct in_addr netmask;
	  for (addr = daemon->interface_addrs; addr; addr = addr->next)
	    {
	      netmask.s_addr = htonl(~(in_addr_t)0 << (32 - addr->prefixlen));
	      if (!(addr->flags & ADDRLIST_IPV6) && 
		  is_same_net(addr->addr.addr.addr4, peer_addr.in.sin_addr, netmask))
		break;
	    }
	}
      if (!addr)
	{
	  my_syslog(LOG_WARNING, _("Ignoring query from non-local network"));
	  return packet;
	}
    }

  while (1)
    {
      if (query_count == TCP_MAX_QUERIES ||
	  !packet ||
	  !read_write(confd, &c1, 1, 1) || !read_write(confd, &c2, 1, 1) ||
	  !(size = c1 << 8 | c2) ||
	  !read_write(confd, payload, size, 1))
       	return packet; 
  
      if (size < (int)sizeof(struct dns_header))
	continue;

      /* Clear buffer beyond request to avoid risk of
	 information disclosure. */
      memset(payload + size, 0, 65536 - size);
      
      query_count++;

      /* log_query gets called indirectly all over the place, so 
	 pass these in global variables - sorry. */
      daemon->log_display_id = ++daemon->log_id;
      daemon->log_source_addr = &peer_addr;
      
      /* save state of "cd" flag in query */
      if ((checking_disabled = header->hb4 & HB4_CD))
	no_cache_dnssec = 1;
       
      if ((gotname = extract_request(header, (unsigned int)size, daemon->namebuff, &qtype)))
	{
#ifdef HAVE_AUTH
	  struct auth_zone *zone;
#endif
	  char *types = querystr(auth_dns ? "auth" : "query", qtype);
	  
	  if (peer_addr.sa.sa_family == AF_INET) 
	    log_query(F_QUERY | F_IPV4 | F_FORWARD, daemon->namebuff, 
		      (struct all_addr *)&peer_addr.in.sin_addr, types);
#ifdef HAVE_IPV6
	  else
	    log_query(F_QUERY | F_IPV6 | F_FORWARD, daemon->namebuff, 
		      (struct all_addr *)&peer_addr.in6.sin6_addr, types);
#endif
	  
#ifdef HAVE_AUTH
	  /* find queries for zones we're authoritative for, and answer them directly */
	  if (!auth_dns && !option_bool(OPT_LOCALISE))
	    for (zone = daemon->auth_zones; zone; zone = zone->next)
	      if (in_zone(zone, daemon->namebuff, NULL))
		{
		  auth_dns = 1;
		  local_auth = 1;
		  break;
		}
#endif
	}
      
      if (local_addr->sa.sa_family == AF_INET)
	dst_addr_4 = local_addr->in.sin_addr;
      else
	dst_addr_4.s_addr = 0;
      
      do_bit = 0;

      if (find_pseudoheader(header, (size_t)size, NULL, &pheader, NULL, NULL))
	{ 
	  unsigned short flags;
	  
	  have_pseudoheader = 1;
	  pheader += 4; /* udp_size, ext_rcode */
	  GETSHORT(flags, pheader);
      
	  if (flags & 0x8000)
	    do_bit = 1; /* do bit */ 
	}

#ifdef HAVE_AUTH
      if (auth_dns)
	m = answer_auth(header, ((char *) header) + 65536, (size_t)size, now, &peer_addr, 
			local_auth, do_bit, have_pseudoheader);
      else
#endif
	{
	   int ad_reqd = do_bit;
	   /* RFC 6840 5.7 */
	   if (header->hb4 & HB4_AD)
	     ad_reqd = 1;
	   
	   /* m > 0 if answered from cache */
	   m = answer_request(header, ((char *) header) + 65536, (size_t)size, 
			      dst_addr_4, netmask, now, ad_reqd, do_bit, have_pseudoheader);
	  
	  /* Do this by steam now we're not in the select() loop */
	  check_log_writer(1); 
	  
	  if (m == 0)
	    {
	      unsigned int flags = 0;
	      struct all_addr *addrp = NULL;
	      int type = SERV_DO_DNSSEC;
	      char *domain = NULL;
	      size_t new_size = add_edns0_config(header, size, ((unsigned char *) header) + 65536, &peer_addr, now, &check_subnet);

	      if (size != new_size)
		{
		  added_pheader = 1;
		  size = new_size;
		}
	      
	      if (gotname)
		flags = search_servers(now, &addrp, gotname, daemon->namebuff, &type, &domain, &norebind);
	      
	      type &= ~SERV_DO_DNSSEC;
	      
	      if (type != 0  || option_bool(OPT_ORDER) || !daemon->last_server)
		last_server = daemon->servers;
	      else
		last_server = daemon->last_server;
	      
	      if (!flags && last_server)
		{
		  struct server *firstsendto = NULL;
#ifdef HAVE_DNSSEC
		  unsigned char *newhash, hash[HASH_SIZE];
		  if ((newhash = hash_questions(header, (unsigned int)size, daemon->namebuff)))
		    memcpy(hash, newhash, HASH_SIZE);
		  else
		    memset(hash, 0, HASH_SIZE);
#else
		  unsigned int crc = questions_crc(header, (unsigned int)size, daemon->namebuff);
#endif		  
		  /* Loop round available servers until we succeed in connecting to one.
		     Note that this code subtly ensures that consecutive queries on this connection
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
			  (type == SERV_HAS_DOMAIN && !hostname_isequal(domain, last_server->domain)) ||
			  (last_server->flags & (SERV_LITERAL_ADDRESS | SERV_LOOP)))
			continue;

		    retry:
		      if (last_server->tcpfd == -1)
			{
			  if ((last_server->tcpfd = socket(last_server->addr.sa.sa_family, SOCK_STREAM, 0)) == -1)
			    continue;
			  
#ifdef HAVE_CONNTRACK
			  /* Copy connection mark of incoming query to outgoing connection. */
			  if (have_mark)
			    setsockopt(last_server->tcpfd, SOL_SOCKET, SO_MARK, &mark, sizeof(unsigned int));
#endif	
		      
			  if ((!local_bind(last_server->tcpfd,  &last_server->source_addr, last_server->interface, 1) ||
			       connect(last_server->tcpfd, &last_server->addr.sa, sa_len(&last_server->addr)) == -1))
			    {
			      close(last_server->tcpfd);
			      last_server->tcpfd = -1;
			      continue;
			    }
			  
			  last_server->flags &= ~SERV_GOT_TCP;
			}
		      
#ifdef HAVE_DNSSEC
		      if (option_bool(OPT_DNSSEC_VALID) && (last_server->flags & SERV_DO_DNSSEC))
			{
			  new_size = add_do_bit(header, size, ((unsigned char *) header) + 65536);
			  
			  if (size != new_size)
			    {
			      added_pheader = 1;
			      size = new_size;
			    }
			  
			  /* For debugging, set Checking Disabled, otherwise, have the upstream check too,
			     this allows it to select auth servers when one is returning bad data. */
			  if (option_bool(OPT_DNSSEC_DEBUG))
			    header->hb4 |= HB4_CD;
			}
#endif
					      
		      *length = htons(size);

		      /* get query name again for logging - may have been overwritten */
		      if (!(gotname = extract_request(header, (unsigned int)size, daemon->namebuff, &qtype)))
			strcpy(daemon->namebuff, "query");
		      
		      if (!read_write(last_server->tcpfd, packet, size + sizeof(u16), 0) ||
			  !read_write(last_server->tcpfd, &c1, 1, 1) ||
			  !read_write(last_server->tcpfd, &c2, 1, 1) ||
			  !read_write(last_server->tcpfd, payload, (c1 << 8) | c2, 1))
			{
			  close(last_server->tcpfd);
			  last_server->tcpfd = -1;
			  /* We get data then EOF, reopen connection to same server,
			     else try next. This avoids DoS from a server which accepts
			     connections and then closes them. */
			  if (last_server->flags & SERV_GOT_TCP)
			    goto retry;
			  else
			    continue;
			}
		      
		      last_server->flags |= SERV_GOT_TCP;

		      m = (c1 << 8) | c2;
		      
		      if (last_server->addr.sa.sa_family == AF_INET)
			log_query(F_SERVER | F_IPV4 | F_FORWARD, daemon->namebuff, 
				  (struct all_addr *)&last_server->addr.in.sin_addr, NULL); 
#ifdef HAVE_IPV6
		      else
			log_query(F_SERVER | F_IPV6 | F_FORWARD, daemon->namebuff, 
				  (struct all_addr *)&last_server->addr.in6.sin6_addr, NULL);
#endif 

#ifdef HAVE_DNSSEC
		      if (option_bool(OPT_DNSSEC_VALID) && !checking_disabled && (last_server->flags & SERV_DO_DNSSEC))
			{
			  int keycount = DNSSEC_WORK; /* Limit to number of DNSSEC questions, to catch loops and avoid filling cache. */
			  int status = tcp_key_recurse(now, STAT_OK, header, m, 0, daemon->namebuff, daemon->keyname, 
						       last_server, have_mark, mark, &keycount);
			  char *result, *domain = "result";
			  
			  if (status == STAT_ABANDONED)
			    {
			      result = "ABANDONED";
			      status = STAT_BOGUS;
			    }
			  else
			    result = (status == STAT_SECURE ? "SECURE" : (status == STAT_INSECURE ? "INSECURE" : "BOGUS"));
			  
			  if (status == STAT_BOGUS && extract_request(header, m, daemon->namebuff, NULL))
			    domain = daemon->namebuff;

			  log_query(F_KEYTAG | F_SECSTAT, domain, NULL, result);
			  
			  if (status == STAT_BOGUS)
			    {
			      no_cache_dnssec = 1;
			      bogusanswer = 1;
			    }

			  if (status == STAT_SECURE)
			    cache_secure = 1;
			}
#endif

		      /* restore CD bit to the value in the query */
		      if (checking_disabled)
			header->hb4 |= HB4_CD;
		      else
			header->hb4 &= ~HB4_CD;
		      
		      /* There's no point in updating the cache, since this process will exit and
			 lose the information after a few queries. We make this call for the alias and 
			 bogus-nxdomain side-effects. */
		      /* If the crc of the question section doesn't match the crc we sent, then
			 someone might be attempting to insert bogus values into the cache by 
			 sending replies containing questions and bogus answers. */
#ifdef HAVE_DNSSEC
		      newhash = hash_questions(header, (unsigned int)m, daemon->namebuff);
		      if (!newhash || memcmp(hash, newhash, HASH_SIZE) != 0)
			{ 
			  m = 0;
			  break;
			}
#else			  
		      if (crc != questions_crc(header, (unsigned int)m, daemon->namebuff))
			{
			  m = 0;
			  break;
			}
#endif

		      m = process_reply(header, now, last_server, (unsigned int)m, 
					option_bool(OPT_NO_REBIND) && !norebind, no_cache_dnssec, cache_secure, bogusanswer,
					ad_reqd, do_bit, added_pheader, check_subnet, &peer_addr); 
		      
		      break;
		    }
		}
	
	      /* In case of local answer or no connections made. */
	      if (m == 0)
		m = setup_reply(header, (unsigned int)size, addrp, flags, daemon->local_ttl);
	    }
	}
	  
      check_log_writer(1);
      
      *length = htons(m);
           
      if (m == 0 || !read_write(confd, packet, m + sizeof(u16), 0))
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
#ifdef HAVE_DNSSEC
      f->dependent = NULL;
      f->blocking_query = NULL;
      f->stash = NULL;
#endif
      daemon->frec_list = f;
    }

  return f;
}

struct randfd *allocate_rfd(int family)
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

void free_rfd(struct randfd *rfd)
{
  if (rfd && --(rfd->refcount) == 0)
    close(rfd->fd);
}

static void free_frec(struct frec *f)
{
  free_rfd(f->rfd4);
  f->rfd4 = NULL;
  f->sentto = NULL;
  f->flags = 0;
  
#ifdef HAVE_IPV6
  free_rfd(f->rfd6);
  f->rfd6 = NULL;
#endif

#ifdef HAVE_DNSSEC
  if (f->stash)
    {
      blockdata_free(f->stash);
      f->stash = NULL;
    }

  /* Anything we're waiting on is pointless now, too */
  if (f->blocking_query)
    free_frec(f->blocking_query);
  f->blocking_query = NULL;
  f->dependent = NULL;
#endif
}



/* if wait==NULL return a free or older than TIMEOUT record.
   else return *wait zero if one available, or *wait is delay to
   when the oldest in-use record will expire. Impose an absolute
   limit of 4*TIMEOUT before we wipe things (for random sockets).
   If force is set, always return a result, even if we have
   to allocate above the limit. */
struct frec *get_new_frec(time_t now, int *wait, int force)
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
#ifdef HAVE_DNSSEC
	    /* Don't free DNSSEC sub-queries here, as we may end up with
	       dangling references to them. They'll go when their "real" query 
	       is freed. */
	    if (!f->dependent)
#endif
	      {
		if (difftime(now, f->time) >= 4*TIMEOUT)
		  {
		    free_frec(f);
		    target = f;
		  }
	     
	    
		if (!oldest || difftime(f->time, oldest->time) <= 0)
		  oldest = f;
	      }
      }

  if (target)
    {
      target->time = now;
      return target;
    }
  
  /* can't find empty one, use oldest if there is one
     and it's older than timeout */
  if (!force && oldest && ((int)difftime(now, oldest->time)) >= TIMEOUT)
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
  if (!force && count > daemon->ftabsize)
    {
      static time_t last_log = 0;
      
      if (oldest && wait)
	*wait = oldest->time + (time_t)TIMEOUT - now;
      
      if ((int)difftime(now, last_log) > 5)
	{
	  last_log = now;
	  my_syslog(LOG_WARNING, _("Maximum number of concurrent DNS queries reached (max: %d)"), daemon->ftabsize);
	}

      return NULL;
    }
  
  if (!(f = allocate_frec(now)) && wait)
    /* wait one second on malloc failure */
    *wait = 1;

  return f; /* OK if malloc fails and this is NULL */
}

/* crc is all-ones if not known. */
static struct frec *lookup_frec(unsigned short id, void *hash)
{
  struct frec *f;

  for(f = daemon->frec_list; f; f = f->next)
    if (f->sentto && f->new_id == id && 
	(!hash || memcmp(hash, f->hash, HASH_SIZE) == 0))
      return f;
      
  return NULL;
}

static struct frec *lookup_frec_by_sender(unsigned short id,
					  union mysockaddr *addr,
					  void *hash)
{
  struct frec *f;
  
  for(f = daemon->frec_list; f; f = f->next)
    if (f->sentto &&
	f->orig_id == id && 
	memcmp(hash, f->hash, HASH_SIZE) == 0 &&
	sockaddr_isequal(&f->source, addr))
      return f;
   
  return NULL;
}
 
/* Send query packet again, if we can. */
void resend_query()
{
  if (daemon->srv_save)
    {
      int fd;
      
      if (daemon->srv_save->sfd)
	fd = daemon->srv_save->sfd->fd;
      else if (daemon->rfd_save && daemon->rfd_save->refcount != 0)
	fd = daemon->rfd_save->fd;
      else
	return;
      
      while(retry_send(sendto(fd, daemon->packet, daemon->packet_len, 0,
			      &daemon->srv_save->addr.sa, 
			      sa_len(&daemon->srv_save->addr)))); 
    }
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
static unsigned short get_id(void)
{
  unsigned short ret = 0;
  
  do 
    ret = rand16();
  while (lookup_frec(ret, NULL));
  
  return ret;
}





