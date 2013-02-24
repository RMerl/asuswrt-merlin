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


/* NB. This code may be called during a DHCPv4 or transaction which is in ping-wait
   It therefore cannot use any DHCP buffer resources except outpacket, which is
   not used by DHCPv4 code. This code may also be called when DHCP 4 or 6 isn't
   active, so we ensure that outpacket is allocated here too */

#include "dnsmasq.h"

#ifdef HAVE_DHCP6

#include <netinet/icmp6.h>

struct ra_param {
  int ind, managed, other, found_context, first;
  char *if_name;
  struct dhcp_netid *tags;
  struct in6_addr link_local;
};

struct search_param {
  time_t now; int iface;
};

static void send_ra(int iface, char *iface_name, struct in6_addr *dest);
static int add_prefixes(struct in6_addr *local,  int prefix,
			int scope, int if_index, int dad, void *vparam);
static int iface_search(struct in6_addr *local,  int prefix,
			int scope, int if_index, int dad, void *vparam);
static int add_lla(int index, unsigned int type, char *mac, size_t maclen, void *parm);

static int hop_limit;
static time_t ra_short_period_start;

void ra_init(time_t now)
{
  struct icmp6_filter filter;
  int fd;
#if defined(IPV6_TCLASS) && defined(IPTOS_CLASS_CS6)
  int class = IPTOS_CLASS_CS6;
#endif
  int val = 255; /* radvd uses this value */
  socklen_t len = sizeof(int);
  struct dhcp_context *context;
  
  /* ensure this is around even if we're not doing DHCPv6 */
  expand_buf(&daemon->outpacket, sizeof(struct dhcp_packet));
 
  /* See if we're guessing SLAAC addresses, if so we need to recieve ping replies */
  for (context = daemon->ra_contexts; context; context = context->next)
    if ((context->flags & CONTEXT_RA_NAME))
      break;
  
  ICMP6_FILTER_SETBLOCKALL(&filter);
  ICMP6_FILTER_SETPASS(ND_ROUTER_SOLICIT, &filter);
  if (context)
    ICMP6_FILTER_SETPASS(ICMP6_ECHO_REPLY, &filter);
  
  if ((fd = socket(PF_INET6, SOCK_RAW, IPPROTO_ICMPV6)) == -1 ||
      getsockopt(fd, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &hop_limit, &len) ||
#if defined(IPV6_TCLASS) && defined(IPTOS_CLASS_CS6)
      setsockopt(fd, IPPROTO_IPV6, IPV6_TCLASS, &class, sizeof(class)) == -1 ||
#endif
      !fix_fd(fd) ||
      !set_ipv6pktinfo(fd) ||
      setsockopt(fd, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &val, sizeof(val)) ||
      setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &val, sizeof(val)) ||
      setsockopt(fd, IPPROTO_ICMPV6, ICMP6_FILTER, &filter, sizeof(filter)) == -1)
    die (_("cannot create ICMPv6 socket: %s"), NULL, EC_BADNET);
  
   daemon->icmp6fd = fd;
   
   ra_start_unsolicted(now, NULL);
}

void ra_start_unsolicted(time_t now, struct dhcp_context *context)
{   
   /* init timers so that we do ra's for some/all soon. some ra_times will end up zeroed
     if it's not appropriate to advertise those contexts.
     This gets re-called on a netlink route-change to re-do the advertisement
     and pick up new interfaces */

  if (context)
     context->ra_time = now;
  else
    for (context = daemon->ra_contexts; context; context = context->next)
      context->ra_time = now + (rand16()/13000); /* range 0 - 5 */

   /* re-do frequently for a minute or so, in case the first gets lost. */
   ra_short_period_start = now;
}

void icmp6_packet(void)
{
  char interface[IF_NAMESIZE+1];
  ssize_t sz; 
  int if_index = 0;
  struct cmsghdr *cmptr;
  struct msghdr msg;
  union {
    struct cmsghdr align; /* this ensures alignment */
    char control6[CMSG_SPACE(sizeof(struct in6_pktinfo))];
  } control_u;
  struct sockaddr_in6 from;
  unsigned char *packet;
  struct iname *tmp;

  /* Note: use outpacket for input buffer */
  msg.msg_control = control_u.control6;
  msg.msg_controllen = sizeof(control_u);
  msg.msg_flags = 0;
  msg.msg_name = &from;
  msg.msg_namelen = sizeof(from);
  msg.msg_iov = &daemon->outpacket;
  msg.msg_iovlen = 1;
  
  if ((sz = recv_dhcp_packet(daemon->icmp6fd, &msg)) == -1 || sz < 8)
    return;
   
  packet = (unsigned char *)daemon->outpacket.iov_base;
  
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
  
  if (!indextoname(daemon->icmp6fd, if_index, interface))
    return;
    
  if (!iface_check(AF_LOCAL, NULL, interface))
    return;
  
  for (tmp = daemon->dhcp_except; tmp; tmp = tmp->next)
    if (tmp->name && (strcmp(tmp->name, interface) == 0))
      return;
 
  if (packet[1] != 0)
    return;
  
  if (packet[0] == ICMP6_ECHO_REPLY)
    lease_ping_reply(&from.sin6_addr, packet, interface); 
  else if (packet[0] == ND_ROUTER_SOLICIT)
    {
      char *mac = "";
      
      /* look for link-layer address option for logging */
      if (sz >= 16 && packet[8] == ICMP6_OPT_SOURCE_MAC && (packet[9] * 8) + 8 <= sz)
	{
	  print_mac(daemon->namebuff, &packet[10], (packet[9] * 8) - 2);
	  mac = daemon->namebuff;
	}
         
      my_syslog(MS_DHCP | LOG_INFO, "RTR-SOLICIT(%s) %s", interface, mac);
      /* source address may not be valid in solicit request. */
      send_ra(if_index, interface, !IN6_IS_ADDR_UNSPECIFIED(&from.sin6_addr) ? &from.sin6_addr : NULL);
    }
}

static void send_ra(int iface, char *iface_name, struct in6_addr *dest)
{
  struct ra_packet *ra;
  struct ra_param parm;
  struct ifreq ifr;
  struct sockaddr_in6 addr;
  struct dhcp_context *context;
  struct dhcp_netid iface_id;
  struct dhcp_opt *opt_cfg;
  int done_dns = 0;
 
  save_counter(0);
  ra = expand(sizeof(struct ra_packet));
  
  ra->type = ND_ROUTER_ADVERT;
  ra->code = 0;
  ra->hop_limit = hop_limit;
  ra->flags = 0x00;
  ra->lifetime = htons(1800); /* AdvDefaultLifetime*/
  ra->reachable_time = 0;
  ra->retrans_time = 0;

  parm.ind = iface;
  parm.managed = 0;
  parm.other = 0;
  parm.found_context = 0;
  parm.if_name = iface_name;
  parm.first = 1;

  /* set tag with name == interface */
  iface_id.net = iface_name;
  iface_id.next = NULL;
  parm.tags = &iface_id; 
  
  for (context = daemon->ra_contexts; context; context = context->next)
    {
      context->flags &= ~CONTEXT_RA_DONE;
      context->netid.next = &context->netid;
    }

  if (!iface_enumerate(AF_INET6, &parm, add_prefixes) ||
      !parm.found_context)
    return;

  strncpy(ifr.ifr_name, iface_name, IF_NAMESIZE);
  
  if (ioctl(daemon->icmp6fd, SIOCGIFMTU, &ifr) != -1)
    {
      put_opt6_char(ICMP6_OPT_MTU);
      put_opt6_char(1);
      put_opt6_short(0);
      put_opt6_long(ifr.ifr_mtu);
    }
     
  iface_enumerate(AF_LOCAL, &iface, add_lla);
 
  /* RDNSS, RFC 6106, use relevant DHCP6 options */
  (void)option_filter(parm.tags, NULL, daemon->dhcp_opts6);
  
  for (opt_cfg = daemon->dhcp_opts6; opt_cfg; opt_cfg = opt_cfg->next)
    {
      int i;
      
      /* netids match and not encapsulated? */
      if (!(opt_cfg->flags & DHOPT_TAGOK))
        continue;
      
      if (opt_cfg->opt == OPTION6_DNS_SERVER)
        {
	  struct in6_addr *a = (struct in6_addr *)opt_cfg->val;

	  done_dns = 1;
          if (opt_cfg->len == 0)
            continue;
	  
	  put_opt6_char(ICMP6_OPT_RDNSS);
	  put_opt6_char((opt_cfg->len/8) + 1);
	  put_opt6_short(0);
	  put_opt6_long(1800); /* lifetime - twice RA retransmit */
	  /* zero means "self" */
	  for (i = 0; i < opt_cfg->len; i += IN6ADDRSZ, a++)
	    if (IN6_IS_ADDR_UNSPECIFIED(a))
	      put_opt6(&parm.link_local, IN6ADDRSZ);
	    else
	      put_opt6(a, IN6ADDRSZ);
	}
      
      if (opt_cfg->opt == OPTION6_DOMAIN_SEARCH && opt_cfg->len != 0)
	{
	  int len = ((opt_cfg->len+7)/8);
	  
	  put_opt6_char(ICMP6_OPT_DNSSL);
	  put_opt6_char(len + 1);
	  put_opt6_short(0);
	  put_opt6_long(1800); /* lifetime - twice RA retransmit */
	  put_opt6(opt_cfg->val, opt_cfg->len);
	  
	  /* pad */
	  for (i = opt_cfg->len; i < len * 8; i++)
	    put_opt6_char(0);
	}
    }
	
  if (!done_dns)
    {
      /* default == us. */
      put_opt6_char(ICMP6_OPT_RDNSS);
      put_opt6_char(3);
      put_opt6_short(0);
      put_opt6_long(1800); /* lifetime - twice RA retransmit */
      put_opt6(&parm.link_local, IN6ADDRSZ);
    }

  /* set managed bits unless we're providing only RA on this link */
  if (parm.managed)
    ra->flags |= 0x80; /* M flag, managed, */
   if (parm.other)
    ra->flags |= 0x40; /* O flag, other */ 
			
  /* decide where we're sending */
  memset(&addr, 0, sizeof(addr));
#ifdef HAVE_SOCKADDR_SA_LEN
  addr.sin6_len = sizeof(struct sockaddr_in6);
#endif
  addr.sin6_family = AF_INET6;
  addr.sin6_port = htons(IPPROTO_ICMPV6);
  if (dest)
    {
      addr.sin6_addr = *dest;
      if (IN6_IS_ADDR_LINKLOCAL(dest) ||
	  IN6_IS_ADDR_MC_LINKLOCAL(dest))
	addr.sin6_scope_id = iface;
    }
  else
    inet_pton(AF_INET6, ALL_NODES, &addr.sin6_addr); 
  
  send_from(daemon->icmp6fd, 0, daemon->outpacket.iov_base, save_counter(0),
	    (union mysockaddr *)&addr, (struct all_addr *)&parm.link_local, iface); 
  
}

static int add_prefixes(struct in6_addr *local,  int prefix,
			int scope, int if_index, int dad, void *vparam)
{
  struct ra_param *param = vparam;

  (void)scope; /* warning */
  (void)dad;

  if (if_index == param->ind)
    {
      if (IN6_IS_ADDR_LINKLOCAL(local))
	param->link_local = *local;
      else if (!IN6_IS_ADDR_LOOPBACK(local) &&
	       !IN6_IS_ADDR_LINKLOCAL(local) &&
	       !IN6_IS_ADDR_MULTICAST(local))
	{
	  int do_prefix = 0;
	  int do_slaac = 0;
	  int deprecate  = 0;
	  unsigned int time = 0xffffffff;
	  struct dhcp_context *context;
	  
	  for (context = daemon->ra_contexts; context; context = context->next)
	    if (prefix == context->prefix &&
		is_same_net6(local, &context->start6, prefix) &&
		is_same_net6(local, &context->end6, prefix))
	      {
		if ((context->flags & 
		     (CONTEXT_RA_ONLY | CONTEXT_RA_NAME | CONTEXT_RA_STATELESS)))
		  {
		    do_slaac = 1;
		    if (context->flags & CONTEXT_DHCP)
		      {
			param->other = 1; 
			if (!(context->flags & CONTEXT_RA_STATELESS))
			  param->managed = 1;
		      }
		  }
		else
		  {
		    /* don't do RA for non-ra-only unless --enable-ra is set */
		    if (!option_bool(OPT_RA))
		      continue;
		    param->managed = 1;
		    param->other = 1;
		  }

		/* find floor time */
		if (time > context->lease_time)
		  time = context->lease_time;
		
		if (context->flags & CONTEXT_DEPRECATE)
		  deprecate = 1;

		/* collect dhcp-range tags */
		if (context->netid.next == &context->netid && context->netid.net)
		  {
		    context->netid.next = param->tags;
		    param->tags = &context->netid;
		  }
		  
		/* subsequent prefixes on the same interface 
		   and subsequent instances of this prefix don't need timers.
		   Be careful not to find the same prefix twice with different
		   addresses. */
		if (!(context->flags & CONTEXT_RA_DONE))
		  {
		    if (!param->first)
		      context->ra_time = 0;
		    context->flags |= CONTEXT_RA_DONE;
		    do_prefix = 1;
		  }

		param->first = 0;	
		param->found_context = 1;
	      }
	  
	  if (do_prefix)
	    {
	      struct prefix_opt *opt;
	     	      
	      if ((opt = expand(sizeof(struct prefix_opt))))
		{
		  /* zero net part of address */
		  setaddr6part(local, addr6part(local) & ~((prefix == 64) ? (u64)-1LL : (1LLU << (128 - prefix)) - 1LLU));
		  
		  /* lifetimes must be min 2 hrs, by RFC 2462 */
		  if (time < 7200)
		    time = 7200;
		  
		  opt->type = ICMP6_OPT_PREFIX;
		  opt->len = 4;
		  opt->prefix_len = prefix;
		  /* autonomous only if we're not doing dhcp, always set "on-link" */
		  opt->flags = do_slaac ? 0xC0 : 0x80;
		  opt->valid_lifetime = htonl(time);
		  opt->preferred_lifetime = htonl(deprecate ? 0 : time);
		  opt->reserved = 0; 
		  opt->prefix = *local;
		  
		  inet_ntop(AF_INET6, local, daemon->addrbuff, ADDRSTRLEN);
		  my_syslog(MS_DHCP | LOG_INFO, "RTR-ADVERT(%s) %s", param->if_name, daemon->addrbuff); 		    
		}
	    }
	}
    }          
  return 1;
}

static int add_lla(int index, unsigned int type, char *mac, size_t maclen, void *parm)
{
  (void)type;

  if (index == *((int *)parm))
    {
      /* size is in units of 8 octets and includes type and length (2 bytes)
	 add 7 to round up */
      int len = (maclen + 9) >> 3;
      unsigned char *p = expand(len << 3);
      memset(p, 0, len << 3);
      *p++ = ICMP6_OPT_SOURCE_MAC;
      *p++ = len;
      memcpy(p, mac, maclen);

      return 0;
    }

  return 1;
}

time_t periodic_ra(time_t now)
{
  struct search_param param;
  struct dhcp_context *context;
  time_t next_event;
  char interface[IF_NAMESIZE+1];
  
  param.now = now;
  param.iface = 0;

  while (1)
    {
      /* find overdue events, and time of first future event */
      for (next_event = 0, context = daemon->ra_contexts; context; context = context->next)
	if (context->ra_time != 0)
	  {
	    if (difftime(context->ra_time, now) <= 0.0)
	      break; /* overdue */
	    
	    if (next_event == 0 || difftime(next_event, context->ra_time) > 0.0)
	      next_event = context->ra_time;
	  }
      
      /* none overdue */
      if (!context)
	break;
      
      /* There's a context overdue, but we can't find an interface
	 associated with it, because it's for a subnet we dont 
	 have an interface on. Probably we're doing DHCP on
	 a remote subnet via a relay. Zero the timer, since we won't
	 ever be able to send ra's and satistfy it. */
      if (iface_enumerate(AF_INET6, &param, iface_search))
	context->ra_time = 0;
      else if (param.iface != 0 &&
	       indextoname(daemon->icmp6fd, param.iface, interface) &&
	       iface_check(AF_LOCAL, NULL, interface))
	{
	  struct iname *tmp;
	  for (tmp = daemon->dhcp_except; tmp; tmp = tmp->next)
	    if (tmp->name && (strcmp(tmp->name, interface) == 0))
	      break;
	  if (!tmp)
	    send_ra(param.iface, interface, NULL); 
	}
    }      
  return next_event;
}
  
static int iface_search(struct in6_addr *local,  int prefix,
			int scope, int if_index, int dad, void *vparam)
{
  struct search_param *param = vparam;
  struct dhcp_context *context;

  (void)scope;
  (void)dad;
 
  for (context = daemon->ra_contexts; context; context = context->next)
    if (prefix == context->prefix &&
	is_same_net6(local, &context->start6, prefix) &&
	is_same_net6(local, &context->end6, prefix))
      if (context->ra_time != 0 && difftime(context->ra_time, param->now) <= 0.0)
	{
	  /* found an interface that's overdue for RA determine new 
	     timeout value and arrange for RA to be sent unless interface is
	     still doing DAD.*/

	  if (!dad)
	    param->iface = if_index;
	  
	  if (difftime(param->now, ra_short_period_start) < 60.0)
	    /* range 5 - 20 */
	    context->ra_time = param->now + 5 + (rand16()/4400);
	  else
	    /* range 450 - 600 */
	    context->ra_time = param->now + 450 + (rand16()/440);
	  
	  /* zero timers for other contexts on the same subnet, so they don't timeout 
	     independently */
	  for (context = context->next; context; context = context->next)
	    if (prefix == context->prefix &&
		is_same_net6(local, &context->start6, prefix) &&
		is_same_net6(local, &context->end6, prefix))
	      context->ra_time = 0;
	  
	  return 0; /* found, abort */
	}
  
  return 1; /* keep searching */
}


#endif
