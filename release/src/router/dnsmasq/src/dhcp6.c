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

#ifdef HAVE_DHCP6

struct iface_param {
  struct dhcp_context *current;
  struct in6_addr fallback;
  int ind, addr_match;
};

static int complete_context6(struct in6_addr *local,  int prefix,
			     int scope, int if_index, int dad, void *vparam);

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
  
  /* When bind-interfaces is set, there might be more than one dnmsasq
     instance binding port 547. That's OK if they serve different networks.
     Need to set REUSEADDR to make this posible, or REUSEPORT on *BSD. */
  if (option_bool(OPT_NOWILD) || option_bool(OPT_CLEVERBIND))
    {
#ifdef SO_REUSEPORT
      int rc = setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &oneopt, sizeof(oneopt));
#else
      int rc = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &oneopt, sizeof(oneopt));
#endif
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
      }

  if (!indextoname(daemon->dhcp6fd, if_index, ifr.ifr_name))
    return;
    
  for (tmp = daemon->if_except; tmp; tmp = tmp->next)
    if (tmp->name && (strcmp(tmp->name, ifr.ifr_name) == 0))
      return;

  for (tmp = daemon->dhcp_except; tmp; tmp = tmp->next)
    if (tmp->name && (strcmp(tmp->name, ifr.ifr_name) == 0))
      return;
 
  parm.current = NULL;
  parm.ind = if_index;
  parm.addr_match = 0;
  memset(&parm.fallback, 0, IN6ADDRSZ);

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
  
  if (!iface_enumerate(AF_INET6, &parm, complete_context6))
    return;
  
  if (daemon->if_names || daemon->if_addrs)
    {
      
      for (tmp = daemon->if_names; tmp; tmp = tmp->next)
	if (tmp->name && (strcmp(tmp->name, ifr.ifr_name) == 0))
	  break;

      if (!tmp && !parm.addr_match)
	return;
    }

  lease_prune(NULL, now); /* lose any expired leases */

  port = dhcp6_reply(parm.current, if_index, ifr.ifr_name, &parm.fallback, 
		     sz, IN6_IS_ADDR_MULTICAST(&from.sin6_addr), now);
  
  lease_update_file(now);
  lease_update_dns(0);
  
  /* The port in the source address of the original request should
     be correct, but at least once client sends from the server port,
     so we explicitly send to the client port to a client, and the
     server port to a relay. */
  if (port != 0)
    {
      from.sin6_port = htons(port);
      while (sendto(daemon->dhcp6fd, daemon->outpacket.iov_base, save_counter(0), 
		    0, (struct sockaddr *)&from, sizeof(from)) == -1 &&
	   retry_send());
    }
}

static int complete_context6(struct in6_addr *local,  int prefix,
			     int scope, int if_index, int dad, void *vparam)
{
  struct dhcp_context *context;
  struct iface_param *param = vparam;
  struct iname *tmp;
 
  (void)scope; /* warning */
  (void)dad;
      
  if (if_index == param->ind &&
      !IN6_IS_ADDR_LOOPBACK(local) &&
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
	  if (prefix == context->prefix &&
	      is_same_net6(local, &context->start6, prefix) &&
	      is_same_net6(local, &context->end6, prefix))
	    {
	      /* link it onto the current chain if we've not seen it before */
	      if (context->current == context)
		{
		  context->current = param->current;
		  param->current = context;
		  context->local6 = *local;
		}
	    }
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

int address6_allocate(struct dhcp_context *context,  unsigned char *clid, int clid_len, 
		      int serial, struct dhcp_netid *netids, struct in6_addr *ans)   
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
     for MAC addresses, let's see how it manages with client-ids! */
  for (j = 0, i = 0; i < clid_len; i++)
    j += clid[i] + (j << 6) + (j << 16) - j;
  
  for (pass = 0; pass <= 1; pass++)
    for (c = context; c; c = c->current)
      if (c->flags & (CONTEXT_DEPRECATE | CONTEXT_STATIC | CONTEXT_RA_STATELESS))
	continue;
      else if (!match_netid(c->filter, netids, pass))
	continue;
      else
	{ 
	  if (option_bool(OPT_CONSEC_ADDR))
	    /* seed is largest extant lease addr in this context */
	    start = lease_find_max_addr6(c) + serial;
	  else
	    start = addr6part(&c->start6) + ((j + c->addr_epoch + serial) % (1 + addr6part(&c->end6) - addr6part(&c->start6)));

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
		return 1;
	      }
	
	    addr++;
	    
	    if (addr  == addr6part(&c->end6) + 1)
	      addr = addr6part(&c->start6);
	    
	  } while (addr != start);
	}
  
  return 0;
}

struct dhcp_context *address6_available(struct dhcp_context *context, 
					struct in6_addr *taddr,
					struct dhcp_netid *netids)
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
          match_netid(tmp->filter, netids, 1))
        return tmp;
    }

  return NULL;
}

struct dhcp_context *narrow_context6(struct dhcp_context *context, 
				     struct in6_addr *taddr,
				     struct dhcp_netid *netids)
{
  /* We start of with a set of possible contexts, all on the current physical interface.
     These are chained on ->current.
     Here we have an address, and return the actual context correponding to that
     address. Note that none may fit, if the address came a dhcp-host and is outside
     any dhcp-range. In that case we return a static range if possible, or failing that,
     any context on the correct subnet. (If there's more than one, this is a dodgy 
     configuration: maybe there should be a warning.) */
  
  struct dhcp_context *tmp;

  if (!(tmp = address6_available(context, taddr, netids)))
    {
      for (tmp = context; tmp; tmp = tmp->current)
        if (match_netid(tmp->filter, netids, 1) &&
            is_same_net6(taddr, &tmp->start6, tmp->prefix) && 
            (tmp->flags & CONTEXT_STATIC))
          break;
      
      if (!tmp)
        for (tmp = context; tmp; tmp = tmp->current)
          if (match_netid(tmp->filter, netids, 1) &&
              is_same_net6(taddr, &tmp->start6, tmp->prefix) &&
              !(tmp->flags & CONTEXT_PROXY))
            break;
    }
  
  /* Only one context allowed now */
  if (tmp)
    tmp->current = NULL;
  
  return tmp;
}

static int is_config_in_context6(struct dhcp_context *context, struct dhcp_config *config)
{
  if (!context) /* called via find_config() from lease_update_from_configs() */
    return 1; 
  if (!(config->flags & CONFIG_ADDR6) || is_addr_in_context6(context, &config->addr6))
    return 1;
  
  return 0;
}


int is_addr_in_context6(struct dhcp_context *context, struct in6_addr *addr)
{
  for (; context; context = context->current)
    if (is_same_net6(addr, &context->start6, context->prefix))
      return 1;
  
  return 0;
}


struct dhcp_config *find_config6(struct dhcp_config *configs,
				 struct dhcp_context *context,
				 unsigned char *duid, int duid_len,
				 char *hostname)
{
  struct dhcp_config *config; 
      
  if (duid)
    for (config = configs; config; config = config->next)
      if (config->flags & CONFIG_CLID)
	{
	  if (config->clid_len == duid_len && 
	      memcmp(config->clid, duid, duid_len) == 0 &&
	      is_config_in_context6(context, config))
	    return config;
	}
    
  if (hostname && context)
    for (config = configs; config; config = config->next)
      if ((config->flags & CONFIG_NAME) && 
          hostname_isequal(config->hostname, hostname) &&
          is_config_in_context6(context, config))
        return config;

  return NULL;
}

void make_duid(time_t now)
{
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
      /* rebase epoch to 1/1/2000 */
      time_t newnow = now - 946684800;
      
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

  if (type >= 256)
    return 1;

#ifdef HAVE_BROKEN_RTC
  daemon->duid = p = safe_malloc(maclen + 4);
  daemon->duid_len = maclen + 4;
  PUTSHORT(3, p); /* DUID_LL */
  PUTSHORT(type, p); /* address type */
#else
  daemon->duid = p = safe_malloc(maclen + 8);
  daemon->duid_len = maclen + 8;
  PUTSHORT(1, p); /* DUID_LLT */
  PUTSHORT(type, p); /* address type */
  PUTLONG(*((time_t *)parm), p); /* time */
#endif

  memcpy(p, mac, maclen);

  return 0;
}
#endif


