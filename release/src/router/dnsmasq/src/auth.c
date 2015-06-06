/* dnsmasq is Copyright (c) 2000-2015 Simon Kelley

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

#ifdef HAVE_AUTH

static struct addrlist *find_subnet(struct auth_zone *zone, int flag, struct all_addr *addr_u)
{
  struct addrlist *subnet;

  for (subnet = zone->subnet; subnet; subnet = subnet->next)
    {
      if (!(subnet->flags & ADDRLIST_IPV6))
	{
	  struct in_addr netmask, addr = addr_u->addr.addr4;

	  if (!(flag & F_IPV4))
	    continue;
	  
	  netmask.s_addr = htonl(~(in_addr_t)0 << (32 - subnet->prefixlen));
	  
	  if  (is_same_net(addr, subnet->addr.addr.addr4, netmask))
	    return subnet;
	}
#ifdef HAVE_IPV6
      else if (is_same_net6(&(addr_u->addr.addr6), &subnet->addr.addr.addr6, subnet->prefixlen))
	return subnet;
#endif

    }
  return NULL;
}

static int filter_zone(struct auth_zone *zone, int flag, struct all_addr *addr_u)
{
  /* No zones specified, no filter */
  if (!zone->subnet)
    return 1;
  
  return find_subnet(zone, flag, addr_u) != NULL;
}

int in_zone(struct auth_zone *zone, char *name, char **cut)
{
  size_t namelen = strlen(name);
  size_t domainlen = strlen(zone->domain);

  if (cut)
    *cut = NULL;
  
  if (namelen >= domainlen && 
      hostname_isequal(zone->domain, &name[namelen - domainlen]))
    {
      
      if (namelen == domainlen)
	return 1;
      
      if (name[namelen - domainlen - 1] == '.')
	{
	  if (cut)
	    *cut = &name[namelen - domainlen - 1]; 
	  return 1;
	}
    }

  return 0;
}


size_t answer_auth(struct dns_header *header, char *limit, size_t qlen, time_t now, union mysockaddr *peer_addr, int local_query) 
{
  char *name = daemon->namebuff;
  unsigned char *p, *ansp;
  int qtype, qclass;
  int nameoffset, axfroffset = 0;
  int q, anscount = 0, authcount = 0;
  struct crec *crecp;
  int  auth = !local_query, trunc = 0, nxdomain = 1, soa = 0, ns = 0, axfr = 0;
  struct auth_zone *zone = NULL;
  struct addrlist *subnet = NULL;
  char *cut;
  struct mx_srv_record *rec, *move, **up;
  struct txt_record *txt;
  struct interface_name *intr;
  struct naptr *na;
  struct all_addr addr;
  struct cname *a;
  
  if (ntohs(header->qdcount) == 0 || OPCODE(header) != QUERY )
    return 0;

  /* determine end of question section (we put answers there) */
  if (!(ansp = skip_questions(header, qlen)))
    return 0; /* bad packet */
  
  /* now process each question, answers go in RRs after the question */
  p = (unsigned char *)(header+1);

  for (q = ntohs(header->qdcount); q != 0; q--)
    {
      unsigned short flag = 0;
      int found = 0;
  
      /* save pointer to name for copying into answers */
      nameoffset = p - (unsigned char *)header;

      /* now extract name as .-concatenated string into name */
      if (!extract_name(header, qlen, &p, name, 1, 4))
	return 0; /* bad packet */
 
      GETSHORT(qtype, p); 
      GETSHORT(qclass, p);
      
      if (qclass != C_IN)
	{
	  auth = 0;
	  continue;
	}

      if ((qtype == T_PTR || qtype == T_SOA || qtype == T_NS) &&
	  (flag = in_arpa_name_2_addr(name, &addr)) &&
	  !local_query)
	{
	  for (zone = daemon->auth_zones; zone; zone = zone->next)
	    if ((subnet = find_subnet(zone, flag, &addr)))
	      break;
	  
	  if (!zone)
	    {
	      auth = 0;
	      continue;
	    }
	  else if (qtype == T_SOA)
	    soa = 1, found = 1;
	  else if (qtype == T_NS)
	    ns = 1, found = 1;
	}

      if (qtype == T_PTR && flag)
	{
	  intr = NULL;

	  if (flag == F_IPV4)
	    for (intr = daemon->int_names; intr; intr = intr->next)
	      {
		struct addrlist *addrlist;
		
		for (addrlist = intr->addr; addrlist; addrlist = addrlist->next)
		  if (!(addrlist->flags & ADDRLIST_IPV6) && addr.addr.addr4.s_addr == addrlist->addr.addr.addr4.s_addr)
		    break;
		
		if (addrlist)
		  break;
		else
		  while (intr->next && strcmp(intr->intr, intr->next->intr) == 0)
		    intr = intr->next;
	      }
#ifdef HAVE_IPV6
	  else if (flag == F_IPV6)
	    for (intr = daemon->int_names; intr; intr = intr->next)
	      {
		struct addrlist *addrlist;
		
		for (addrlist = intr->addr; addrlist; addrlist = addrlist->next)
		  if ((addrlist->flags & ADDRLIST_IPV6) && IN6_ARE_ADDR_EQUAL(&addr.addr.addr6, &addrlist->addr.addr.addr6))
		    break;
		
		if (addrlist)
		  break;
		else
		  while (intr->next && strcmp(intr->intr, intr->next->intr) == 0)
		    intr = intr->next;
	      }
#endif
	  
	  if (intr)
	    {
	      if (local_query || in_zone(zone, intr->name, NULL))
		{	
		  found = 1;
		  log_query(flag | F_REVERSE | F_CONFIG, intr->name, &addr, NULL);
		  if (add_resource_record(header, limit, &trunc, nameoffset, &ansp, 
					  daemon->auth_ttl, NULL,
					  T_PTR, C_IN, "d", intr->name))
		    anscount++;
		}
	    }
	  
	  if ((crecp = cache_find_by_addr(NULL, &addr, now, flag)))
	    do { 
	      strcpy(name, cache_get_name(crecp));
	      
	      if (crecp->flags & F_DHCP && !option_bool(OPT_DHCP_FQDN))
		{
		  char *p = strchr(name, '.');
		  if (p)
		    *p = 0; /* must be bare name */
		  
		  /* add  external domain */
		  if (zone)
		    {
		      strcat(name, ".");
		      strcat(name, zone->domain);
		    }
		  log_query(flag | F_DHCP | F_REVERSE, name, &addr, record_source(crecp->uid));
		  found = 1;
		  if (add_resource_record(header, limit, &trunc, nameoffset, &ansp, 
					  daemon->auth_ttl, NULL,
					  T_PTR, C_IN, "d", name))
		    anscount++;
		}
	      else if (crecp->flags & (F_DHCP | F_HOSTS) && (local_query || in_zone(zone, name, NULL)))
		{
		  log_query(crecp->flags & ~F_FORWARD, name, &addr, record_source(crecp->uid));
		  found = 1;
		  if (add_resource_record(header, limit, &trunc, nameoffset, &ansp, 
					  daemon->auth_ttl, NULL,
					  T_PTR, C_IN, "d", name))
		    anscount++;
		}
	      else
		continue;
		    
	    } while ((crecp = cache_find_by_addr(crecp, &addr, now, flag)));

	  if (found)
	    nxdomain = 0;
	  else
	    log_query(flag | F_NEG | F_NXDOMAIN | F_REVERSE | (auth ? F_AUTH : 0), NULL, &addr, NULL);

	  continue;
	}
      
    cname_restart:
      if (found)
	/* NS and SOA .arpa requests have set found above. */
	cut = NULL;
      else
	{
	  for (zone = daemon->auth_zones; zone; zone = zone->next)
	    if (in_zone(zone, name, &cut))
	      break;
	  
	  if (!zone)
	    {
	      auth = 0;
	      continue;
	    }
	}

      for (rec = daemon->mxnames; rec; rec = rec->next)
	if (!rec->issrv && hostname_isequal(name, rec->name))
	  {
	    nxdomain = 0;
	         
	    if (qtype == T_MX)
	      {
		found = 1;
		log_query(F_CONFIG | F_RRNAME, name, NULL, "<MX>"); 
		if (add_resource_record(header, limit, &trunc, nameoffset, &ansp, daemon->auth_ttl,
					NULL, T_MX, C_IN, "sd", rec->weight, rec->target))
		  anscount++;
	      }
	  }
      
      for (move = NULL, up = &daemon->mxnames, rec = daemon->mxnames; rec; rec = rec->next)
	if (rec->issrv && hostname_isequal(name, rec->name))
	  {
	    nxdomain = 0;
	    
	    if (qtype == T_SRV)
	      {
		found = 1;
		log_query(F_CONFIG | F_RRNAME, name, NULL, "<SRV>"); 
		if (add_resource_record(header, limit, &trunc, nameoffset, &ansp, daemon->auth_ttl,
					NULL, T_SRV, C_IN, "sssd", 
					rec->priority, rec->weight, rec->srvport, rec->target))

		  anscount++;
	      } 
	    
	    /* unlink first SRV record found */
	    if (!move)
	      {
		move = rec;
		*up = rec->next;
	      }
	    else
	      up = &rec->next;      
	  }
	else
	  up = &rec->next;
	  
      /* put first SRV record back at the end. */
      if (move)
	{
	  *up = move;
	  move->next = NULL;
	}

      for (txt = daemon->rr; txt; txt = txt->next)
	if (hostname_isequal(name, txt->name))
	  {
	    nxdomain = 0;
	    if (txt->class == qtype)
	      {
		found = 1;
		log_query(F_CONFIG | F_RRNAME, name, NULL, "<RR>"); 
		if (add_resource_record(header, limit, &trunc, nameoffset, &ansp, daemon->auth_ttl,
					NULL, txt->class, C_IN, "t", txt->len, txt->txt))
		  anscount++;
	      }
	  }
      
      for (txt = daemon->txt; txt; txt = txt->next)
	if (txt->class == C_IN && hostname_isequal(name, txt->name))
	  {
	    nxdomain = 0;
	    if (qtype == T_TXT)
	      {
		found = 1;
		log_query(F_CONFIG | F_RRNAME, name, NULL, "<TXT>"); 
		if (add_resource_record(header, limit, &trunc, nameoffset, &ansp, daemon->auth_ttl,
					NULL, T_TXT, C_IN, "t", txt->len, txt->txt))
		  anscount++;
	      }
	  }

       for (na = daemon->naptr; na; na = na->next)
	 if (hostname_isequal(name, na->name))
	   {
	     nxdomain = 0;
	     if (qtype == T_NAPTR)
	       {
		 found = 1;
		 log_query(F_CONFIG | F_RRNAME, name, NULL, "<NAPTR>");
		 if (add_resource_record(header, limit, &trunc, nameoffset, &ansp, daemon->auth_ttl, 
					 NULL, T_NAPTR, C_IN, "sszzzd", 
					 na->order, na->pref, na->flags, na->services, na->regexp, na->replace))
			  anscount++;
	       }
	   }
    
       if (qtype == T_A)
	 flag = F_IPV4;
       
#ifdef HAVE_IPV6
       if (qtype == T_AAAA)
	 flag = F_IPV6;
#endif
       
       for (intr = daemon->int_names; intr; intr = intr->next)
	 if (hostname_isequal(name, intr->name))
	   {
	     struct addrlist *addrlist;
	     
	     nxdomain = 0;
	     
	     if (flag)
	       for (addrlist = intr->addr; addrlist; addrlist = addrlist->next)  
		 if (((addrlist->flags & ADDRLIST_IPV6)  ? T_AAAA : T_A) == qtype &&
		     (local_query || filter_zone(zone, flag, &addrlist->addr)))
		   {
#ifdef HAVE_IPV6
		     if (addrlist->flags & ADDRLIST_REVONLY)
		       continue;
#endif
		     found = 1;
		     log_query(F_FORWARD | F_CONFIG | flag, name, &addrlist->addr, NULL);
		     if (add_resource_record(header, limit, &trunc, nameoffset, &ansp, 
					     daemon->auth_ttl, NULL, qtype, C_IN, 
					     qtype == T_A ? "4" : "6", &addrlist->addr))
		       anscount++;
		   }
	     }
       
       for (a = daemon->cnames; a; a = a->next)
	 if (hostname_isequal(name, a->alias) )
	   {
	     log_query(F_CONFIG | F_CNAME, name, NULL, NULL);
	     strcpy(name, a->target);
	     if (!strchr(name, '.'))
	       {
		 strcat(name, ".");
		 strcat(name, zone->domain);
	       }
	     found = 1;
	     if (add_resource_record(header, limit, &trunc, nameoffset, &ansp, 
				     daemon->auth_ttl, &nameoffset,
				     T_CNAME, C_IN, "d", name))
	       anscount++;
	     
	     goto cname_restart;
	   }

      if (!cut)
	{
	  nxdomain = 0;
	  
	  if (qtype == T_SOA)
	    {
	      auth = soa = 1; /* inhibits auth section */
	      found = 1;
	      log_query(F_RRNAME | F_AUTH, zone->domain, NULL, "<SOA>");
	    }
      	  else if (qtype == T_AXFR)
	    {
	      struct iname *peers;
	      
	      if (peer_addr->sa.sa_family == AF_INET)
		peer_addr->in.sin_port = 0;
#ifdef HAVE_IPV6
	      else
		{
		  peer_addr->in6.sin6_port = 0; 
		  peer_addr->in6.sin6_scope_id = 0;
		}
#endif
	      
	      for (peers = daemon->auth_peers; peers; peers = peers->next)
		if (sockaddr_isequal(peer_addr, &peers->addr))
		  break;
	      
	      /* Refuse all AXFR unless --auth-sec-servers is set */
	      if ((!peers && daemon->auth_peers) || !daemon->secondary_forward_server)
		{
		  if (peer_addr->sa.sa_family == AF_INET)
		    inet_ntop(AF_INET, &peer_addr->in.sin_addr, daemon->addrbuff, ADDRSTRLEN);
#ifdef HAVE_IPV6
		  else
		    inet_ntop(AF_INET6, &peer_addr->in6.sin6_addr, daemon->addrbuff, ADDRSTRLEN); 
#endif
		  
		  my_syslog(LOG_WARNING, _("ignoring zone transfer request from %s"), daemon->addrbuff);
		  return 0;
		}
	       	      
	      auth = 1;
	      soa = 1; /* inhibits auth section */
	      ns = 1; /* ensure we include NS records! */
	      axfr = 1;
	      found = 1;
	      axfroffset = nameoffset;
	      log_query(F_RRNAME | F_AUTH, zone->domain, NULL, "<AXFR>");
	    }
      	  else if (qtype == T_NS)
	    {
	      auth = 1;
	      ns = 1; /* inhibits auth section */
	      found = 1;
	      log_query(F_RRNAME | F_AUTH, zone->domain, NULL, "<NS>"); 
	    }
	}
      
      if (!option_bool(OPT_DHCP_FQDN) && cut)
	{	  
	  *cut = 0; /* remove domain part */
	  
	  if (!strchr(name, '.') && (crecp = cache_find_by_name(NULL, name, now, F_IPV4 | F_IPV6)))
	    {
	      if (crecp->flags & F_DHCP)
		do
		  { 
		    nxdomain = 0;
		    if ((crecp->flags & flag) && 
			(local_query || filter_zone(zone, flag, &(crecp->addr.addr))))
		      {
			*cut = '.'; /* restore domain part */
			log_query(crecp->flags, name, &crecp->addr.addr, record_source(crecp->uid));
			*cut  = 0; /* remove domain part */
			found = 1;
			if (add_resource_record(header, limit, &trunc, nameoffset, &ansp, 
						daemon->auth_ttl, NULL, qtype, C_IN, 
						qtype == T_A ? "4" : "6", &crecp->addr))
			  anscount++;
		      }
		  } while ((crecp = cache_find_by_name(crecp, name, now,  F_IPV4 | F_IPV6)));
	    }
       	  
	  *cut = '.'; /* restore domain part */	    
	}
      
      if ((crecp = cache_find_by_name(NULL, name, now, F_IPV4 | F_IPV6)))
	{
	  if ((crecp->flags & F_HOSTS) || (((crecp->flags & F_DHCP) && option_bool(OPT_DHCP_FQDN))))
	    do
	      { 
		 nxdomain = 0;
		 if ((crecp->flags & flag) && (local_query || filter_zone(zone, flag, &(crecp->addr.addr))))
		   {
		     log_query(crecp->flags, name, &crecp->addr.addr, record_source(crecp->uid));
		     found = 1;
		     if (add_resource_record(header, limit, &trunc, nameoffset, &ansp, 
					     daemon->auth_ttl, NULL, qtype, C_IN, 
					     qtype == T_A ? "4" : "6", &crecp->addr))
		       anscount++;
		   }
	      } while ((crecp = cache_find_by_name(crecp, name, now, F_IPV4 | F_IPV6)));
	}
      
      if (!found)
	log_query(flag | F_NEG | (nxdomain ? F_NXDOMAIN : 0) | F_FORWARD | F_AUTH, name, NULL, NULL);
      
    }
  
  /* Add auth section */
  if (auth && zone)
    {
      char *authname;
      int newoffset, offset = 0;

      if (!subnet)
	authname = zone->domain;
      else
	{
	  /* handle NS and SOA for PTR records */
	  
	  authname = name;

	  if (!(subnet->flags & ADDRLIST_IPV6))
	    {
	      in_addr_t a = ntohl(subnet->addr.addr.addr4.s_addr) >> 8;
	      char *p = name;
	      
	      if (subnet->prefixlen >= 24)
		p += sprintf(p, "%d.", a & 0xff);
	      a = a >> 8;
	      if (subnet->prefixlen >= 16 )
		p += sprintf(p, "%d.", a & 0xff);
	      a = a >> 8;
	      p += sprintf(p, "%d.in-addr.arpa", a & 0xff);
	      
	    }
#ifdef HAVE_IPV6
	  else
	    {
	      char *p = name;
	      int i;
	      
	      for (i = subnet->prefixlen-1; i >= 0; i -= 4)
		{ 
		  int dig = ((unsigned char *)&subnet->addr.addr.addr6)[i>>3];
		  p += sprintf(p, "%.1x.", (i>>2) & 1 ? dig & 15 : dig >> 4);
		}
	      p += sprintf(p, "ip6.arpa");
	      
	    }
#endif
	}
      
      /* handle NS and SOA in auth section or for explicit queries */
       newoffset = ansp - (unsigned char *)header;
       if (((anscount == 0 && !ns) || soa) &&
	  add_resource_record(header, limit, &trunc, 0, &ansp, 
			      daemon->auth_ttl, NULL, T_SOA, C_IN, "ddlllll",
			      authname, daemon->authserver,  daemon->hostmaster,
			      daemon->soa_sn, daemon->soa_refresh, 
			      daemon->soa_retry, daemon->soa_expiry, 
			      daemon->auth_ttl))
	{
	  offset = newoffset;
	  if (soa)
	    anscount++;
	  else
	    authcount++;
	}
      
      if (anscount != 0 || ns)
	{
	  struct name_list *secondary;
	  
	  newoffset = ansp - (unsigned char *)header;
	  if (add_resource_record(header, limit, &trunc, -offset, &ansp, 
				  daemon->auth_ttl, NULL, T_NS, C_IN, "d", offset == 0 ? authname : NULL, daemon->authserver))
	    {
	      if (offset == 0) 
		offset = newoffset;
	      if (ns) 
		anscount++;
	      else
		authcount++;
	    }

	  if (!subnet)
	    for (secondary = daemon->secondary_forward_server; secondary; secondary = secondary->next)
	      if (add_resource_record(header, limit, &trunc, offset, &ansp, 
				      daemon->auth_ttl, NULL, T_NS, C_IN, "d", secondary->name))
		{
		  if (ns) 
		    anscount++;
		  else
		    authcount++;
		}
	}
      
      if (axfr)
	{
	  for (rec = daemon->mxnames; rec; rec = rec->next)
	    if (in_zone(zone, rec->name, &cut))
	      {
		if (cut)
		   *cut = 0;

		if (rec->issrv)
		  {
		    if (add_resource_record(header, limit, &trunc, -axfroffset, &ansp, daemon->auth_ttl,
					    NULL, T_SRV, C_IN, "sssd", cut ? rec->name : NULL,
					    rec->priority, rec->weight, rec->srvport, rec->target))
		      
		      anscount++;
		  }
		else
		  {
		    if (add_resource_record(header, limit, &trunc, -axfroffset, &ansp, daemon->auth_ttl,
					    NULL, T_MX, C_IN, "sd", cut ? rec->name : NULL, rec->weight, rec->target))
		      anscount++;
		  }
		
		/* restore config data */
		if (cut)
		  *cut = '.';
	      }
	      
	  for (txt = daemon->rr; txt; txt = txt->next)
	    if (in_zone(zone, txt->name, &cut))
	      {
		if (cut)
		  *cut = 0;
		
		if (add_resource_record(header, limit, &trunc, -axfroffset, &ansp, daemon->auth_ttl,
					NULL, txt->class, C_IN, "t",  cut ? txt->name : NULL, txt->len, txt->txt))
		  anscount++;
		
		/* restore config data */
		if (cut)
		  *cut = '.';
	      }
	  
	  for (txt = daemon->txt; txt; txt = txt->next)
	    if (txt->class == C_IN && in_zone(zone, txt->name, &cut))
	      {
		if (cut)
		  *cut = 0;
		
		if (add_resource_record(header, limit, &trunc, -axfroffset, &ansp, daemon->auth_ttl,
					NULL, T_TXT, C_IN, "t", cut ? txt->name : NULL, txt->len, txt->txt))
		  anscount++;
		
		/* restore config data */
		if (cut)
		  *cut = '.';
	      }
	  
	  for (na = daemon->naptr; na; na = na->next)
	    if (in_zone(zone, na->name, &cut))
	      {
		if (cut)
		  *cut = 0;
		
		if (add_resource_record(header, limit, &trunc, -axfroffset, &ansp, daemon->auth_ttl, 
					NULL, T_NAPTR, C_IN, "sszzzd", cut ? na->name : NULL,
					na->order, na->pref, na->flags, na->services, na->regexp, na->replace))
		  anscount++;
		
		/* restore config data */
		if (cut)
		  *cut = '.'; 
	      }
	  
	  for (intr = daemon->int_names; intr; intr = intr->next)
	    if (in_zone(zone, intr->name, &cut))
	      {
		struct addrlist *addrlist;
		
		if (cut)
		  *cut = 0;
		
		for (addrlist = intr->addr; addrlist; addrlist = addrlist->next) 
		  if (!(addrlist->flags & ADDRLIST_IPV6) &&
		      (local_query || filter_zone(zone, F_IPV4, &addrlist->addr)) && 
		      add_resource_record(header, limit, &trunc, -axfroffset, &ansp, 
					  daemon->auth_ttl, NULL, T_A, C_IN, "4", cut ? intr->name : NULL, &addrlist->addr))
		    anscount++;
		
#ifdef HAVE_IPV6
		for (addrlist = intr->addr; addrlist; addrlist = addrlist->next) 
		  if ((addrlist->flags & ADDRLIST_IPV6) && 
		      (local_query || filter_zone(zone, F_IPV6, &addrlist->addr)) &&
		      add_resource_record(header, limit, &trunc, -axfroffset, &ansp, 
					  daemon->auth_ttl, NULL, T_AAAA, C_IN, "6", cut ? intr->name : NULL, &addrlist->addr))
		    anscount++;
#endif		    
		
		/* restore config data */
		if (cut)
		  *cut = '.'; 
	      }
             
	  for (a = daemon->cnames; a; a = a->next)
	    if (in_zone(zone, a->alias, &cut))
	      {
		strcpy(name, a->target);
		if (!strchr(name, '.'))
		  {
		    strcat(name, ".");
		    strcat(name, zone->domain);
		  }
		
		if (cut)
		  *cut = 0;
		
		if (add_resource_record(header, limit, &trunc, -axfroffset, &ansp, 
					daemon->auth_ttl, NULL,
					T_CNAME, C_IN, "d",  cut ? a->alias : NULL, name))
		  anscount++;
	      }
	
	  cache_enumerate(1);
	  while ((crecp = cache_enumerate(0)))
	    {
	      if ((crecp->flags & (F_IPV4 | F_IPV6)) &&
		  !(crecp->flags & (F_NEG | F_NXDOMAIN)) &&
		  (crecp->flags & F_FORWARD))
		{
		  if ((crecp->flags & F_DHCP) && !option_bool(OPT_DHCP_FQDN))
		    {
		      char *cache_name = cache_get_name(crecp);
		      if (!strchr(cache_name, '.') && 
			  (local_query || filter_zone(zone, (crecp->flags & (F_IPV6 | F_IPV4)), &(crecp->addr.addr))))
			{
			  qtype = T_A;
#ifdef HAVE_IPV6
			  if (crecp->flags & F_IPV6)
			    qtype = T_AAAA;
#endif
			  if (add_resource_record(header, limit, &trunc, -axfroffset, &ansp, 
						  daemon->auth_ttl, NULL, qtype, C_IN, 
						  (crecp->flags & F_IPV4) ? "4" : "6", cache_name, &crecp->addr))
			    anscount++;
			}
		    }
		  
		  if ((crecp->flags & F_HOSTS) || (((crecp->flags & F_DHCP) && option_bool(OPT_DHCP_FQDN))))
		    {
		      strcpy(name, cache_get_name(crecp));
		      if (in_zone(zone, name, &cut) && 
			  (local_query || filter_zone(zone, (crecp->flags & (F_IPV6 | F_IPV4)), &(crecp->addr.addr))))
			{
			  qtype = T_A;
#ifdef HAVE_IPV6
			  if (crecp->flags & F_IPV6)
			    qtype = T_AAAA;
#endif
			   if (cut)
			     *cut = 0;

			   if (add_resource_record(header, limit, &trunc, -axfroffset, &ansp, 
						   daemon->auth_ttl, NULL, qtype, C_IN, 
						   (crecp->flags & F_IPV4) ? "4" : "6", cut ? name : NULL, &crecp->addr))
			     anscount++;
			}
		    }
		}
	    }
	   
	  /* repeat SOA as last record */
	  if (add_resource_record(header, limit, &trunc, axfroffset, &ansp, 
				  daemon->auth_ttl, NULL, T_SOA, C_IN, "ddlllll",
				  daemon->authserver,  daemon->hostmaster,
				  daemon->soa_sn, daemon->soa_refresh, 
				  daemon->soa_retry, daemon->soa_expiry, 
				  daemon->auth_ttl))
	    anscount++;
	  
	}
      
    }
  
  /* done all questions, set up header and return length of result */
  /* clear authoritative and truncated flags, set QR flag */
  header->hb3 = (header->hb3 & ~(HB3_AA | HB3_TC)) | HB3_QR;

  if (local_query)
    {
      /* set RA flag */
      header->hb4 |= HB4_RA;
    }
  else
    {
      /* clear RA flag */
      header->hb4 &= ~HB4_RA;
    }

  /* authoritive */
  if (auth)
    header->hb3 |= HB3_AA;
  
  /* truncation */
  if (trunc)
    header->hb3 |= HB3_TC;
  
  if ((auth || local_query) && nxdomain)
    SET_RCODE(header, NXDOMAIN);
  else
    SET_RCODE(header, NOERROR); /* no error */
  header->ancount = htons(anscount);
  header->nscount = htons(authcount);
  header->arcount = htons(0);
  return ansp - (unsigned char *)header;
}
  
#endif  
  


