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

#ifdef HAVE_DHCP6

#include <netinet/icmp6.h>

struct iface_param {
  struct dhcp_context *current;
  struct dhcp_relay *relay;
  struct in6_addr fallback, relay_local, ll_addr, ula_addr;
  int ind, addr_match;
};


static int complete_context6(struct in6_addr *local,  int prefix,
			     int scope, int if_index, int flags, 
			     unsigned int preferred, unsigned int valid, void *vparam);
static int make_duid1(int index, unsigned int type, char *mac, size_t maclen, void *parm); 

void dhcp6_init(void)
{
  int fd;
  struct sockaddr_in6 saddr;
#if defined(IPV6_TCLASS) && defined(IPTOS_CLASS_CS6)
  int class = IPTOS_CLASS_CS6;
#endif
  int oneopt = 1;

  if ((fd = socket(PF_INET6, SOCK_DGRAM, IPPROTO_UDP)) == -1 ||
#if defined(IPV6_TCLASS) && defined(IPTOS_CLASS_CS6)
      setsockopt(fd, IPPROTO_IPV6, IPV6_TCLASS, &class, sizeof(class)) == -1 ||
#endif
      setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &oneopt, sizeof(oneopt)) == -1 ||
      !fix_fd(fd) ||
      !set_ipv6pktinfo(fd))
    die (_("cannot create DHCPv6 socket: %s"), NULL, EC_BADNET);
  
 /* When bind-interfaces is set, there might be more than one dnsmasq
     instance binding port 547. That's OK if they serve different networks.
     Need to set REUSEADDR|REUSEPORT to make this possible.
     Handle the case that REUSEPORT is defined, but the kernel doesn't 
     support it. This handles the introduction of REUSEPORT on Linux. */
  if (option_bool(OPT_NOWILD) || option_bool(OPT_CLEVERBIND))
    {
      int rc = 0;

#ifdef SO_REUSEPORT
      if ((rc = setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &oneopt, sizeof(oneopt))) == -1 &&
	  errno == ENOPROTOOPT)
	rc = 0;
#endif
      
      if (rc != -1)
	rc = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &oneopt, sizeof(oneopt));
      
      if (rc == -1)
	die(_("failed to set SO_REUSE{ADDR|PORT} on DHCPv6 socket: %s"), NULL, EC_BADNET);
    }
  
  memset(&saddr, 0, sizeof(saddr));
#ifdef HAVE_SOCKADDR_SA_LEN
  saddr.sin6_len = sizeof(struct sockaddr_in6);
#endif
  saddr.sin6_family = AF_INET6;
  saddr.sin6_addr = in6addr_any;
  saddr.sin6_port = htons(DHCPV6_SERVER_PORT);
  
  if (bind(fd, (struct sockaddr *)&saddr, sizeof(struct sockaddr_in6)))
    die(_("failed to bind DHCPv6 server socket: %s"), NULL, EC_BADNET);
  
  daemon->dhcp6fd = fd;
}

void dhcp6_packet(time_t now)
{
  struct dhcp_context *context;
  struct dhcp_relay *relay;
  struct iface_param parm;
  struct cmsghdr *cmptr;
  struct msghdr msg;
  int if_index = 0;
  union {
    struct cmsghdr align; /* this ensures alignment */
    char control6[CMSG_SPACE(sizeof(struct in6_pktinfo))];
  } control_u;
  struct sockaddr_in6 from;
  ssize_t sz; 
  struct ifreq ifr;
  struct iname *tmp;
  unsigned short port;
  struct in6_addr dst_addr;

  memset(&dst_addr, 0, sizeof(dst_addr));

  msg.msg_control = control_u.control6;
  msg.msg_controllen = sizeof(control_u);
  msg.msg_flags = 0;
  msg.msg_name = &from;
  msg.msg_namelen = sizeof(from);
  msg.msg_iov =  &daemon->dhcp_packet;
  msg.msg_iovlen = 1;
  
  if ((sz = recv_dhcp_packet(daemon->dhcp6fd, &msg)) == -1)
    return;
  
  for (cmptr = CMSG_FIRSTHDR(&msg); cmptr; cmptr = CMSG_NXTHDR(&msg, cmptr))
    if (cmptr->cmsg_level == IPPROTO_IPV6 && cmptr->cmsg_type == daemon->v6pktinfo)
      {
	union {
	  unsigned char *c;
	  struct in6_pktinfo *p;
	} p;
	p.c = CMSG_DATA(cmptr);
        
	if_index = p.p->ipi6_ifindex;
	dst_addr = p.p->ipi6_addr;
      }

  if (!indextoname(daemon->dhcp6fd, if_index, ifr.ifr_name))
    return;

  if ((port = relay_reply6(&from, sz, ifr.ifr_name)) == 0)
    {
      struct dhcp_bridge *bridge, *alias;

      for (tmp = daemon->if_except; tmp; tmp = tmp->next)
	if (tmp->name && wildcard_match(tmp->name, ifr.ifr_name))
	  return;
      
      for (tmp = daemon->dhcp_except; tmp; tmp = tmp->next)
	if (tmp->name && wildcard_match(tmp->name, ifr.ifr_name))
	  return;
      
      parm.current = NULL;
      parm.relay = NULL;
      memset(&parm.relay_local, 0, IN6ADDRSZ);
      parm.ind = if_index;
      parm.addr_match = 0;
      memset(&parm.fallback, 0, IN6ADDRSZ);
      memset(&parm.ll_addr, 0, IN6ADDRSZ);
      memset(&parm.ula_addr, 0, IN6ADDRSZ);

      /* If the interface on which the DHCPv6 request was received is
         an alias of some other interface (as specified by the
         --bridge-interface option), change parm.ind so that we look
         for DHCPv6 contexts associated with the aliased interface
         instead of with the aliasing one. */
      for (bridge = daemon->bridges; bridge; bridge = bridge->next)
	{
	  for (alias = bridge->alias; alias; alias = alias->next)
	    if (wildcard_matchn(alias->iface, ifr.ifr_name, IF_NAMESIZE))
	      {
		parm.ind = if_nametoindex(bridge->iface);
		if (!parm.ind)
		  {
		    my_syslog(MS_DHCP | LOG_WARNING,
			      _("unknown interface %s in bridge-interface"),
			      bridge->iface);
		    return;
		  }
		break;
	      }
	  if (alias)
	    break;
	}
      
      for (context = daemon->dhcp6; context; context = context->next)
	if (IN6_IS_ADDR_UNSPECIFIED(&context->start6) && context->prefix == 0)
	  {
	    /* wildcard context for DHCP-stateless only */
	    parm.current = context;
	    context->current = NULL;
	  }
	else
	  {
	    /* unlinked contexts are marked by context->current == context */
	    context->current = context;
	    memset(&context->local6, 0, IN6ADDRSZ);
	  }

      for (relay = daemon->relay6; relay; relay = relay->next)
	relay->current = relay;
      
      if (!iface_enumerate(AF_INET6, &parm, complete_context6))
	return;

      if (daemon->if_names || daemon->if_addrs)
	{
	  
	  for (tmp = daemon->if_names; tmp; tmp = tmp->next)
	    if (tmp->name && wildcard_match(tmp->name, ifr.ifr_name))
	      break;
	  
	  if (!tmp && !parm.addr_match)
	    return;
	}
      
      if (parm.relay)
	{
	  /* Ignore requests sent to the ALL_SERVERS multicast address for relay when
	     we're listening there for DHCPv6 server reasons. */
	  struct in6_addr all_servers;
	  
	  inet_pton(AF_INET6, ALL_SERVERS, &all_servers);
	  
	  if (!IN6_ARE_ADDR_EQUAL(&dst_addr, &all_servers))
	    relay_upstream6(parm.relay, sz, &from.sin6_addr, from.sin6_scope_id, now);
	  return;
	}
      
      /* May have configured relay, but not DHCP server */
      if (!daemon->doing_dhcp6)
	return;

      lease_prune(NULL, now); /* lose any expired leases */
      
      port = dhcp6_reply(parm.current, if_index, ifr.ifr_name, &parm.fallback, 
			 &parm.ll_addr, &parm.ula_addr, sz, &from.sin6_addr, now);
      
      lease_update_file(now);
      lease_update_dns(0);
    }
			  
  /* The port in the source address of the original request should
     be correct, but at least once client sends from the server port,
     so we explicitly send to the client port to a client, and the
     server port to a relay. */
  if (port != 0)
    {
      from.sin6_port = htons(port);
      while (retry_send(sendto(daemon->dhcp6fd, daemon->outpacket.iov_base, 
			       save_counter(0), 0, (struct sockaddr *)&from, 
			       sizeof(from))));
    }
}

void get_client_mac(struct in6_addr *client, int iface, unsigned char *mac, unsigned int *maclenp, unsigned int *mactypep, time_t now)
{
  /* Receiving a packet from a host does not populate the neighbour
     cache, so we send a neighbour discovery request if we can't 
     find the sender. Repeat a few times in case of packet loss. */
  
  struct neigh_packet neigh;
  union mysockaddr addr;
  int i, maclen;

  neigh.type = ND_NEIGHBOR_SOLICIT;
  neigh.code = 0;
  neigh.reserved = 0;
  neigh.target = *client;
  /* RFC4443 section-2.3: checksum has to be zero to be calculated */
  neigh.checksum = 0;
   
  memset(&addr, 0, sizeof(addr));
#ifdef HAVE_SOCKADDR_SA_LEN
  addr.in6.sin6_len = sizeof(struct sockaddr_in6);
#endif
  addr.in6.sin6_family = AF_INET6;
  addr.in6.sin6_port = htons(IPPROTO_ICMPV6);
  addr.in6.sin6_addr = *client;
  addr.in6.sin6_scope_id = iface;
  
  for (i = 0; i < 5; i++)
    {
      struct timespec ts;
      
      if ((maclen = find_mac(&addr, mac, 0, now)) != 0)
	break;
	  
      sendto(daemon->icmp6fd, &neigh, sizeof(neigh), 0, &addr.sa, sizeof(addr));
      
      ts.tv_sec = 0;
      ts.tv_nsec = 100000000; /* 100ms */
      nanosleep(&ts, NULL);
    }

  *maclenp = maclen;
  *mactypep = ARPHRD_ETHER;
}
    
static int complete_context6(struct in6_addr *local,  int prefix,
			     int scope, int if_index, int flags, unsigned int preferred, 
			     unsigned int valid, void *vparam)
{
  struct dhcp_context *context;
  struct dhcp_relay *relay;
  struct iface_param *param = vparam;
  struct iname *tmp;
 
  (void)scope; /* warning */
  
  if (if_index == param->ind)
    {
      if (IN6_IS_ADDR_LINKLOCAL(local))
	param->ll_addr = *local;
      else if (IN6_IS_ADDR_ULA(local))
	param->ula_addr = *local;

      if (!IN6_IS_ADDR_LOOPBACK(local) &&
	  !IN6_IS_ADDR_LINKLOCAL(local) &&
	  !IN6_IS_ADDR_MULTICAST(local))
	{
	  /* if we have --listen-address config, see if the 
	     arrival interface has a matching address. */
	  for (tmp = daemon->if_addrs; tmp; tmp = tmp->next)
	    if (tmp->addr.sa.sa_family == AF_INET6 &&
		IN6_ARE_ADDR_EQUAL(&tmp->addr.in6.sin6_addr, local))
	      param->addr_match = 1;
	  
	  /* Determine a globally address on the arrival interface, even
	     if we have no matching dhcp-context, because we're only
	     allocating on remote subnets via relays. This
	     is used as a default for the DNS server option. */
	  param->fallback = *local;
	  
	  for (context = daemon->dhcp6; context; context = context->next)
	    {
	      if ((context->flags & CONTEXT_DHCP) &&
		  !(context->flags & (CONTEXT_TEMPLATE | CONTEXT_OLD)) &&
		  prefix <= context->prefix &&
		  is_same_net6(local, &context->start6, context->prefix) &&
		  is_same_net6(local, &context->end6, context->prefix))
		{
		  
		  
		  /* link it onto the current chain if we've not seen it before */
		  if (context->current == context)
		    {
		      struct dhcp_context *tmp, **up;
		      
		      /* use interface values only for constructed contexts */
		      if (!(context->flags & CONTEXT_CONSTRUCTED))
			preferred = valid = 0xffffffff;
		      else if (flags & IFACE_DEPRECATED)
			preferred = 0;
		      
		      if (context->flags & CONTEXT_DEPRECATE)
			preferred = 0;
		      
		      /* order chain, longest preferred time first */
		      for (up = &param->current, tmp = param->current; tmp; tmp = tmp->current)
			if (tmp->preferred <= preferred)
			  break;
			else
			  up = &tmp->current;
		      
		      context->current = *up;
		      *up = context;
		      context->local6 = *local;
		      context->preferred = preferred;
		      context->valid = valid;
		    }
		}
	    }
	}

      for (relay = daemon->relay6; relay; relay = relay->next)
	if (IN6_ARE_ADDR_EQUAL(local, &relay->local.addr.addr6) && relay->current == relay &&
	    (IN6_IS_ADDR_UNSPECIFIED(&param->relay_local) || IN6_ARE_ADDR_EQUAL(local, &param->relay_local)))
	  {
	    relay->current = param->relay;
	    param->relay = relay;
	    param->relay_local = *local;
	  }
      
    }          
 
 return 1;
}

struct dhcp_config *config_find_by_address6(struct dhcp_config *configs, struct in6_addr *net, int prefix, u64 addr)
{
  struct dhcp_config *config;
  
  for (config = configs; config; config = config->next)
    if ((config->flags & CONFIG_ADDR6) &&
	is_same_net6(&config->addr6, net, prefix) &&
	(prefix == 128 || addr6part(&config->addr6) == addr))
      return config;
  
  return NULL;
}

struct dhcp_context *address6_allocate(struct dhcp_context *context,  unsigned char *clid, int clid_len, int temp_addr,
				       int iaid, int serial, struct dhcp_netid *netids, int plain_range, struct in6_addr *ans)   
{
  /* Find a free address: exclude anything in use and anything allocated to
     a particular hwaddr/clientid/hostname in our configuration.
     Try to return from contexts which match netids first. 
     
     Note that we assume the address prefix lengths are 64 or greater, so we can
     get by with 64 bit arithmetic.
*/

  u64 start, addr;
  struct dhcp_context *c, *d;
  int i, pass;
  u64 j; 

  /* hash hwaddr: use the SDBM hashing algorithm.  This works
     for MAC addresses, let's see how it manages with client-ids! 
     For temporary addresses, we generate a new random one each time. */
  if (temp_addr)
    j = rand64();
  else
    for (j = iaid, i = 0; i < clid_len; i++)
      j = clid[i] + (j << 6) + (j << 16) - j;
  
  for (pass = 0; pass <= plain_range ? 1 : 0; pass++)
    for (c = context; c; c = c->current)
      if (c->flags & (CONTEXT_DEPRECATE | CONTEXT_STATIC | CONTEXT_RA_STATELESS | CONTEXT_USED))
	continue;
      else if (!match_netid(c->filter, netids, pass))
	continue;
      else
	{ 
	  if (!temp_addr && option_bool(OPT_CONSEC_ADDR))
	    /* seed is largest extant lease addr in this context */
	    start = lease_find_max_addr6(c) + serial;
	  else
	    {
	      u64 range = 1 + addr6part(&c->end6) - addr6part(&c->start6);
	      u64 offset = j + c->addr_epoch;

	      /* don't divide by zero if range is whole 2^64 */
	      if (range != 0)
		offset = offset % range;

	      start = addr6part(&c->start6) + offset;
	    }

	  /* iterate until we find a free address. */
	  addr = start;
	  
	  do {
	    /* eliminate addresses in use by the server. */
	    for (d = context; d; d = d->current)
	      if (addr == addr6part(&d->local6))
		break;

	    if (!d &&
		!lease6_find_by_addr(&c->start6, c->prefix, addr) && 
		!config_find_by_address6(daemon->dhcp_conf, &c->start6, c->prefix, addr))
	      {
		*ans = c->start6;
		setaddr6part (ans, addr);
		return c;
	      }
	
	    addr++;
	    
	    if (addr  == addr6part(&c->end6) + 1)
	      addr = addr6part(&c->start6);
	    
	  } while (addr != start);
	}
	   
  return NULL;
}

/* can dynamically allocate addr */
struct dhcp_context *address6_available(struct dhcp_context *context, 
					struct in6_addr *taddr,
					struct dhcp_netid *netids,
					int plain_range)
{
  u64 start, end, addr = addr6part(taddr);
  struct dhcp_context *tmp;
 
  for (tmp = context; tmp; tmp = tmp->current)
    {
      start = addr6part(&tmp->start6);
      end = addr6part(&tmp->end6);

      if (!(tmp->flags & (CONTEXT_STATIC | CONTEXT_RA_STATELESS)) &&
          is_same_net6(&tmp->start6, taddr, tmp->prefix) &&
	  is_same_net6(&tmp->end6, taddr, tmp->prefix) &&
	  addr >= start &&
          addr <= end &&
          match_netid(tmp->filter, netids, plain_range))
        return tmp;
    }

  return NULL;
}

/* address OK if configured */
struct dhcp_context *address6_valid(struct dhcp_context *context, 
				    struct in6_addr *taddr,
				    struct dhcp_netid *netids,
				    int plain_range)
{
  struct dhcp_context *tmp;
 
  for (tmp = context; tmp; tmp = tmp->current)
    if (is_same_net6(&tmp->start6, taddr, tmp->prefix) &&
	match_netid(tmp->filter, netids, plain_range))
      return tmp;

  return NULL;
}

int config_valid(struct dhcp_config *config, struct dhcp_context *context, struct in6_addr *addr)
{
  if (!config || !(config->flags & CONFIG_ADDR6))
    return 0;

  if ((config->flags & CONFIG_WILDCARD) && context->prefix == 64)
    {
      *addr = context->start6;
      setaddr6part(addr, addr6part(&config->addr6));
      return 1;
    }
  
  if (is_same_net6(&context->start6, &config->addr6, context->prefix))
    {
      *addr = config->addr6;
      return 1;
    }
  
  return 0;
}

void make_duid(time_t now)
{
  (void)now;

  if (daemon->duid_config)
    {
      unsigned char *p;
      
      daemon->duid = p = safe_malloc(daemon->duid_config_len + 6);
      daemon->duid_len = daemon->duid_config_len + 6;
      PUTSHORT(2, p); /* DUID_EN */
      PUTLONG(daemon->duid_enterprise, p);
      memcpy(p, daemon->duid_config, daemon->duid_config_len);
    }
  else
    {
      time_t newnow = 0;
      
      /* If we have no persistent lease database, or a non-stable RTC, use DUID_LL (newnow == 0) */
#ifndef HAVE_BROKEN_RTC
      /* rebase epoch to 1/1/2000 */
      if (!option_bool(OPT_LEASE_RO) || daemon->lease_change_command)
	newnow = now - 946684800;
#endif      
      
      iface_enumerate(AF_LOCAL, &newnow, make_duid1);
      
      if(!daemon->duid)
	die("Cannot create DHCPv6 server DUID: %s", NULL, EC_MISC);
    }
}

static int make_duid1(int index, unsigned int type, char *mac, size_t maclen, void *parm)
{
  /* create DUID as specified in RFC3315. We use the MAC of the
     first interface we find that isn't loopback or P-to-P and
     has address-type < 256. Address types above 256 are things like 
     tunnels which don't have usable MAC addresses. */
  
  unsigned char *p;
  (void)index;
  (void)parm;
  time_t newnow = *((time_t *)parm);
  
  if (type >= 256)
    return 1;

  if (newnow == 0)
    {
      daemon->duid = p = safe_malloc(maclen + 4);
      daemon->duid_len = maclen + 4;
      PUTSHORT(3, p); /* DUID_LL */
      PUTSHORT(type, p); /* address type */
    }
  else
    {
      daemon->duid = p = safe_malloc(maclen + 8);
      daemon->duid_len = maclen + 8;
      PUTSHORT(1, p); /* DUID_LLT */
      PUTSHORT(type, p); /* address type */
      PUTLONG(*((time_t *)parm), p); /* time */
    }
  
  memcpy(p, mac, maclen);

  return 0;
}

struct cparam {
  time_t now;
  int newone, newname;
};

static int construct_worker(struct in6_addr *local, int prefix, 
			    int scope, int if_index, int flags, 
			    int preferred, int valid, void *vparam)
{
  char ifrn_name[IFNAMSIZ];
  struct in6_addr start6, end6;
  struct dhcp_context *template, *context;

  (void)scope;
  (void)flags;
  (void)valid;
  (void)preferred;

  struct cparam *param = vparam;

  if (IN6_IS_ADDR_LOOPBACK(local) ||
      IN6_IS_ADDR_LINKLOCAL(local) ||
      IN6_IS_ADDR_MULTICAST(local))
    return 1;

  if (!(flags & IFACE_PERMANENT))
    return 1;

  if (flags & IFACE_DEPRECATED)
    return 1;

  if (!indextoname(daemon->icmp6fd, if_index, ifrn_name))
    return 0;
  
  for (template = daemon->dhcp6; template; template = template->next)
    if (!(template->flags & CONTEXT_TEMPLATE))
      {
	/* non-template entries, just fill in interface and local addresses */
	if (prefix <= template->prefix &&
	    is_same_net6(local, &template->start6, template->prefix) &&
	    is_same_net6(local, &template->end6, template->prefix))
	  {
	    template->if_index = if_index;
	    template->local6 = *local;
	  }
	
      }
    else if (wildcard_match(template->template_interface, ifrn_name) &&
	     template->prefix >= prefix)
      {
	start6 = *local;
	setaddr6part(&start6, addr6part(&template->start6));
	end6 = *local;
	setaddr6part(&end6, addr6part(&template->end6));
	
	for (context = daemon->dhcp6; context; context = context->next)
	  if ((context->flags & CONTEXT_CONSTRUCTED) &&
	      IN6_ARE_ADDR_EQUAL(&start6, &context->start6) &&
	      IN6_ARE_ADDR_EQUAL(&end6, &context->end6))
	    {
	      int flags = context->flags;
	      context->flags &= ~(CONTEXT_GC | CONTEXT_OLD);
	      if (flags & CONTEXT_OLD)
		{
		  /* address went, now it's back */
		  log_context(AF_INET6, context); 
		  /* fast RAs for a while */
		  ra_start_unsolicited(param->now, context);
		  param->newone = 1; 
		  /* Add address to name again */
		  if (context->flags & CONTEXT_RA_NAME)
		    param->newname = 1;
		}
	      break;
	    }
	
	if (!context && (context = whine_malloc(sizeof (struct dhcp_context))))
	  {
	    *context = *template;
	    context->start6 = start6;
	    context->end6 = end6;
	    context->flags &= ~CONTEXT_TEMPLATE;
	    context->flags |= CONTEXT_CONSTRUCTED;
	    context->if_index = if_index;
	    context->local6 = *local;
	    context->saved_valid = 0;
	    
	    context->next = daemon->dhcp6;
	    daemon->dhcp6 = context;

	    ra_start_unsolicited(param->now, context);
	    /* we created a new one, need to call
	       lease_update_file to get periodic functions called */
	    param->newone = 1; 

	    /* Will need to add new putative SLAAC addresses to existing leases */
	    if (context->flags & CONTEXT_RA_NAME)
	      param->newname = 1;
	    
	    log_context(AF_INET6, context);
	  } 
      }
  
  return 1;
}

void dhcp_construct_contexts(time_t now)
{ 
  struct dhcp_context *context, *tmp, **up;
  struct cparam param;
  param.newone = 0;
  param.newname = 0;
  param.now = now;

  for (context = daemon->dhcp6; context; context = context->next)
    if (context->flags & CONTEXT_CONSTRUCTED)
      context->flags |= CONTEXT_GC;
   
  iface_enumerate(AF_INET6, &param, construct_worker);

  for (up = &daemon->dhcp6, context = daemon->dhcp6; context; context = tmp)
    {
      
      tmp = context->next; 
     
      if (context->flags & CONTEXT_GC && !(context->flags & CONTEXT_OLD))
	{
	  if ((context->flags & CONTEXT_RA) || option_bool(OPT_RA))
	    {
	      /* previously constructed context has gone. advertise it's demise */
	      context->flags |= CONTEXT_OLD;
	      context->address_lost_time = now;
	      /* Apply same ceiling of configured lease time as in radv.c */
	      if (context->saved_valid > context->lease_time)
		context->saved_valid = context->lease_time;
	      /* maximum time is 2 hours, from RFC */
	      if (context->saved_valid > 7200) /* 2 hours */
		context->saved_valid = 7200;
	      ra_start_unsolicited(now, context);
	      param.newone = 1; /* include deletion */ 
	      
	      if (context->flags & CONTEXT_RA_NAME)
		param.newname = 1; 
			      
	      log_context(AF_INET6, context);
	      
	      up = &context->next;
	    }
	  else
	    {
	      /* we were never doing RA for this, so free now */
	      *up = context->next;
	      free(context);
	    }
	}
      else
	 up = &context->next;
    }
  
  if (param.newone)
    {
      if (daemon->dhcp || daemon->doing_dhcp6)
	{
	  if (param.newname)
	    lease_update_slaac(now);
	  lease_update_file(now);
	}
      else 
	/* Not doing DHCP, so no lease system, manage alarms for ra only */
	send_alarm(periodic_ra(now), now);
    }
}

#endif


