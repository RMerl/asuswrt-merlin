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


/* NB. This code may be called during a DHCPv4 or transaction which is in ping-wait
   It therefore cannot use any DHCP buffer resources except outpacket, which is
   not used by DHCPv4 code. This code may also be called when DHCP 4 or 6 isn't
   active, so we ensure that outpacket is allocated here too */

#include "dnsmasq.h"

#ifdef HAVE_DHCP6

#include <netinet/icmp6.h>

struct ra_param {
  time_t now;
  int ind, managed, other, first, adv_router;
  char *if_name;
  struct dhcp_netid *tags;
  struct in6_addr link_local, link_global, ula;
  unsigned int glob_pref_time, link_pref_time, ula_pref_time, adv_interval, prio;
  struct dhcp_context *found_context;
};

struct search_param {
  time_t now; int iface;
  char name[IF_NAMESIZE+1];
};

struct alias_param {
  int iface;
  struct dhcp_bridge *bridge;
  int num_alias_ifs;
  int max_alias_ifs;
  int *alias_ifs;
};

static void send_ra(time_t now, int iface, char *iface_name, struct in6_addr *dest);
static void send_ra_alias(time_t now, int iface, char *iface_name, struct in6_addr *dest,
                    int send_iface);
static int send_ra_to_aliases(int index, unsigned int type, char *mac, size_t maclen, void *parm);
static int add_prefixes(struct in6_addr *local,  int prefix,
			int scope, int if_index, int flags, 
			unsigned int preferred, unsigned int valid, void *vparam);
static int iface_search(struct in6_addr *local,  int prefix,
			int scope, int if_index, int flags, 
			int prefered, int valid, void *vparam);
static int add_lla(int index, unsigned int type, char *mac, size_t maclen, void *parm);
static void new_timeout(struct dhcp_context *context, char *iface_name, time_t now);
static unsigned int calc_lifetime(struct ra_interface *ra);
static unsigned int calc_interval(struct ra_interface *ra);
static unsigned int calc_prio(struct ra_interface *ra);
static struct ra_interface *find_iface_param(char *iface);

static int hop_limit;

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
 
  /* See if we're guessing SLAAC addresses, if so we need to receive ping replies */
  for (context = daemon->dhcp6; context; context = context->next)
    if ((context->flags & CONTEXT_RA_NAME))
      break;
  
  /* Need ICMP6 socket for transmission for DHCPv6 even when not doing RA. */

  ICMP6_FILTER_SETBLOCKALL(&filter);
  if (daemon->doing_ra)
    {
      ICMP6_FILTER_SETPASS(ND_ROUTER_SOLICIT, &filter);
      if (context)
	ICMP6_FILTER_SETPASS(ICMP6_ECHO_REPLY, &filter);
    }
  
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
   
   if (daemon->doing_ra)
     ra_start_unsolicited(now, NULL);
}

void ra_start_unsolicited(time_t now, struct dhcp_context *context)
{   
   /* init timers so that we do ra's for some/all soon. some ra_times will end up zeroed
     if it's not appropriate to advertise those contexts.
     This gets re-called on a netlink route-change to re-do the advertisement
     and pick up new interfaces */
  
  if (context)
    context->ra_short_period_start = context->ra_time = now;
  else
    for (context = daemon->dhcp6; context; context = context->next)
      if (!(context->flags & CONTEXT_TEMPLATE))
	{
	  context->ra_time = now + (rand16()/13000); /* range 0 - 5 */
	  /* re-do frequently for a minute or so, in case the first gets lost. */
	  context->ra_short_period_start = now;
	}
}

void icmp6_packet(time_t now)
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
    
  if (!iface_check(AF_LOCAL, NULL, interface, NULL))
    return;
  
  for (tmp = daemon->dhcp_except; tmp; tmp = tmp->next)
    if (tmp->name && wildcard_match(tmp->name, interface))
      return;
 
  if (packet[1] != 0)
    return;
  
  if (packet[0] == ICMP6_ECHO_REPLY)
    lease_ping_reply(&from.sin6_addr, packet, interface); 
  else if (packet[0] == ND_ROUTER_SOLICIT)
    {
      char *mac = "";
      struct dhcp_bridge *bridge, *alias;
      
      /* look for link-layer address option for logging */
      if (sz >= 16 && packet[8] == ICMP6_OPT_SOURCE_MAC && (packet[9] * 8) + 8 <= sz)
	{
	  if ((packet[9] * 8 - 2) * 3 - 1 >= MAXDNAME) {
	    return;
	  }
	  print_mac(daemon->namebuff, &packet[10], (packet[9] * 8) - 2);
	  mac = daemon->namebuff;
	}
         
      if (!option_bool(OPT_QUIET_RA))
	my_syslog(MS_DHCP | LOG_INFO, "RTR-SOLICIT(%s) %s", interface, mac);

      /* If the incoming interface is an alias of some other one (as
         specified by the --bridge-interface option), send an RA using
         the context of the aliased interface. */
      for (bridge = daemon->bridges; bridge; bridge = bridge->next)
        {
          int bridge_index = if_nametoindex(bridge->iface);
          if (bridge_index)
	    {
	      for (alias = bridge->alias; alias; alias = alias->next)
		if (wildcard_matchn(alias->iface, interface, IF_NAMESIZE))
		  {
		    /* Send an RA on if_index with information from
		       bridge_index. */
		    send_ra_alias(now, bridge_index, bridge->iface, NULL, if_index);
		    break;
		  }
	      if (alias)
		break;
	    }
        }

      /* If the incoming interface wasn't an alias, send an RA using
	 the context of the incoming interface. */
      if (!bridge)
	/* source address may not be valid in solicit request. */
	send_ra(now, if_index, interface, !IN6_IS_ADDR_UNSPECIFIED(&from.sin6_addr) ? &from.sin6_addr : NULL);
    }
}

static void send_ra_alias(time_t now, int iface, char *iface_name, struct in6_addr *dest, int send_iface)
{
  struct ra_packet *ra;
  struct ra_param parm;
  struct sockaddr_in6 addr;
  struct dhcp_context *context, *tmp,  **up;
  struct dhcp_netid iface_id;
  struct dhcp_opt *opt_cfg;
  struct ra_interface *ra_param = find_iface_param(iface_name);
  int done_dns = 0, old_prefix = 0, mtu = 0;
  unsigned int min_pref_time;
#ifdef HAVE_LINUX_NETWORK
  FILE *f;
#endif
  
  parm.ind = iface;
  parm.managed = 0;
  parm.other = 0;
  parm.found_context = NULL;
  parm.adv_router = 0;
  parm.if_name = iface_name;
  parm.first = 1;
  parm.now = now;
  parm.glob_pref_time = parm.link_pref_time = parm.ula_pref_time = 0;
  parm.adv_interval = calc_interval(ra_param);
  parm.prio = calc_prio(ra_param);
  
  reset_counter();
  
  if (!(ra = expand(sizeof(struct ra_packet))))
    return;
  
  ra->type = ND_ROUTER_ADVERT;
  ra->code = 0;
  ra->hop_limit = hop_limit;
  ra->flags = parm.prio;
  ra->lifetime = htons(calc_lifetime(ra_param));
  ra->reachable_time = 0;
  ra->retrans_time = 0;

  /* set tag with name == interface */
  iface_id.net = iface_name;
  iface_id.next = NULL;
  parm.tags = &iface_id; 
  
  for (context = daemon->dhcp6; context; context = context->next)
    {
      context->flags &= ~CONTEXT_RA_DONE;
      context->netid.next = &context->netid;
    }

  if (!iface_enumerate(AF_INET6, &parm, add_prefixes))
    return;

  /* Find smallest preferred time within address classes,
     to use as lifetime for options. This is a rather arbitrary choice. */
  min_pref_time = 0xffffffff;
  if (parm.glob_pref_time != 0 && parm.glob_pref_time < min_pref_time)
    min_pref_time = parm.glob_pref_time;
  
  if (parm.ula_pref_time != 0 && parm.ula_pref_time < min_pref_time)
    min_pref_time = parm.ula_pref_time;

  if (parm.link_pref_time != 0 && parm.link_pref_time < min_pref_time)
    min_pref_time = parm.link_pref_time;

  /* Look for constructed contexts associated with addresses which have gone, 
     and advertise them with preferred_time == 0  RFC 6204 4.3 L-13 */
  for (up = &daemon->dhcp6, context = daemon->dhcp6; context; context = tmp)
    {
      tmp = context->next;

      if (context->if_index == iface && (context->flags & CONTEXT_OLD))
	{
	  unsigned int old = difftime(now, context->address_lost_time);
	  
	  if (old > context->saved_valid)
	    { 
	      /* We've advertised this enough, time to go */
	     
	      /* If this context held the timeout, and there's another context in use
		 transfer the timeout there. */
	      if (context->ra_time != 0 && parm.found_context && parm.found_context->ra_time == 0)
		new_timeout(parm.found_context, iface_name, now);
	      
	      *up = context->next;
	      free(context);
	    }
	  else
	    {
	      struct prefix_opt *opt;
	      struct in6_addr local = context->start6;
	      int do_slaac = 0;

	      old_prefix = 1;

	      /* zero net part of address */
	      setaddr6part(&local, addr6part(&local) & ~((context->prefix == 64) ? (u64)-1LL : (1LLU << (128 - context->prefix)) - 1LLU));
	     
	      
	      if (context->flags & CONTEXT_RA)
		{
		  do_slaac = 1;
		  if (context->flags & CONTEXT_DHCP)
		    {
		      parm.other = 1; 
		      if (!(context->flags & CONTEXT_RA_STATELESS))
			parm.managed = 1;
		    }
		}
	      else
		{
		  /* don't do RA for non-ra-only unless --enable-ra is set */
		  if (option_bool(OPT_RA))
		    {
		      parm.managed = 1;
		      parm.other = 1;
		    }
		}

	      if ((opt = expand(sizeof(struct prefix_opt))))
		{
		  opt->type = ICMP6_OPT_PREFIX;
		  opt->len = 4;
		  opt->prefix_len = context->prefix;
		  /* autonomous only if we're not doing dhcp, set
                     "on-link" unless "off-link" was specified */
		  opt->flags = (do_slaac ? 0x40 : 0) |
                    ((context->flags & CONTEXT_RA_OFF_LINK) ? 0 : 0x80);
		  opt->valid_lifetime = htonl(context->saved_valid - old);
		  opt->preferred_lifetime = htonl(0);
		  opt->reserved = 0; 
		  opt->prefix = local;
		  
		  inet_ntop(AF_INET6, &local, daemon->addrbuff, ADDRSTRLEN);
		  if (!option_bool(OPT_QUIET_RA))
		    my_syslog(MS_DHCP | LOG_INFO, "RTR-ADVERT(%s) %s old prefix", iface_name, daemon->addrbuff); 		    
		}
	   
	      up = &context->next;
	    }
	}
      else
	up = &context->next;
    }
    
  /* If we're advertising only old prefixes, set router lifetime to zero. */
  if (old_prefix && !parm.found_context)
    ra->lifetime = htons(0);

  /* No prefixes to advertise. */
  if (!old_prefix && !parm.found_context)
    return; 
  
  /* If we're sending router address instead of prefix in at least on prefix,
     include the advertisement interval option. */
  if (parm.adv_router)
    {
      put_opt6_char(ICMP6_OPT_ADV_INTERVAL);
      put_opt6_char(1);
      put_opt6_short(0);
      /* interval value is in milliseconds */
      put_opt6_long(1000 * calc_interval(find_iface_param(iface_name)));
    }

  /* Set the MTU from ra_param if any, an MTU of 0 mean automatic for linux, */
  /* an MTU of -1 prevents the option from being sent. */
  if (ra_param)
    mtu = ra_param->mtu;
#ifdef HAVE_LINUX_NETWORK
  /* Note that IPv6 MTU is not neccessarily the same as the IPv4 MTU
     available from SIOCGIFMTU */
  if (mtu == 0)
    {
      char *mtu_name = ra_param ? ra_param->mtu_name : NULL;
      sprintf(daemon->namebuff, "/proc/sys/net/ipv6/conf/%s/mtu", mtu_name ? : iface_name);
      if ((f = fopen(daemon->namebuff, "r")))
        {
          if (fgets(daemon->namebuff, MAXDNAME, f))
            mtu = atoi(daemon->namebuff);
          fclose(f);
        }
    }
#endif
  if (mtu > 0)
    {
      put_opt6_char(ICMP6_OPT_MTU);
      put_opt6_char(1);
      put_opt6_short(0);
      put_opt6_long(mtu);
    }
     
  iface_enumerate(AF_LOCAL, &send_iface, add_lla);
 
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
	  struct in6_addr *a;
	  int len;

	  done_dns = 1;

          if (opt_cfg->len == 0)
	    continue;
	  
	  /* reduce len for any addresses we can't substitute */
	  for (a = (struct in6_addr *)opt_cfg->val, len = opt_cfg->len, i = 0; 
	       i < opt_cfg->len; i += IN6ADDRSZ, a++)
	    if ((IN6_IS_ADDR_UNSPECIFIED(a) && parm.glob_pref_time == 0) ||
		(IN6_IS_ADDR_ULA_ZERO(a) && parm.ula_pref_time == 0) ||
		(IN6_IS_ADDR_LINK_LOCAL_ZERO(a) && parm.link_pref_time == 0))
	      len -= IN6ADDRSZ;

	  if (len != 0)
	    {
	      put_opt6_char(ICMP6_OPT_RDNSS);
	      put_opt6_char((len/8) + 1);
	      put_opt6_short(0);
	      put_opt6_long(min_pref_time);
	 
	      for (a = (struct in6_addr *)opt_cfg->val, i = 0; i <  opt_cfg->len; i += IN6ADDRSZ, a++)
		if (IN6_IS_ADDR_UNSPECIFIED(a))
		  {
		    if (parm.glob_pref_time != 0)
		      put_opt6(&parm.link_global, IN6ADDRSZ);
		  }
		else if (IN6_IS_ADDR_ULA_ZERO(a))
		  {
		    if (parm.ula_pref_time != 0)
		    put_opt6(&parm.ula, IN6ADDRSZ);
		  }
		else if (IN6_IS_ADDR_LINK_LOCAL_ZERO(a))
		  {
		    if (parm.link_pref_time != 0)
		      put_opt6(&parm.link_local, IN6ADDRSZ);
		  }
		else
		  put_opt6(a, IN6ADDRSZ);
	    }
	}
      
      if (opt_cfg->opt == OPTION6_DOMAIN_SEARCH && opt_cfg->len != 0)
	{
	  int len = ((opt_cfg->len+7)/8);
	  
	  put_opt6_char(ICMP6_OPT_DNSSL);
	  put_opt6_char(len + 1);
	  put_opt6_short(0);
	  put_opt6_long(min_pref_time); 
	  put_opt6(opt_cfg->val, opt_cfg->len);
	  
	  /* pad */
	  for (i = opt_cfg->len; i < len * 8; i++)
	    put_opt6_char(0);
	}
    }
	
  if (daemon->port == NAMESERVER_PORT && !done_dns && parm.link_pref_time != 0)
    {
      /* default == us, as long as we are supplying DNS service. */
      put_opt6_char(ICMP6_OPT_RDNSS);
      put_opt6_char(3);
      put_opt6_short(0);
      put_opt6_long(min_pref_time); 
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
    {
      inet_pton(AF_INET6, ALL_NODES, &addr.sin6_addr); 
      setsockopt(daemon->icmp6fd, IPPROTO_IPV6, IPV6_MULTICAST_IF, &send_iface, sizeof(send_iface));
    }
  
  while (retry_send(sendto(daemon->icmp6fd, daemon->outpacket.iov_base, 
			   save_counter(-1), 0, (struct sockaddr *)&addr, 
			   sizeof(addr))));
  
}

static void send_ra(time_t now, int iface, char *iface_name, struct in6_addr *dest)
{
  /* Send an RA on the same interface that the RA content is based
     on. */
  send_ra_alias(now, iface, iface_name, dest, iface);
}

static int add_prefixes(struct in6_addr *local,  int prefix,
			int scope, int if_index, int flags, 
			unsigned int preferred, unsigned int valid, void *vparam)
{
  struct ra_param *param = vparam;

  (void)scope; /* warning */
  
  if (if_index == param->ind)
    {
      if (IN6_IS_ADDR_LINKLOCAL(local))
	{
	  /* Can there be more than one LL address?
	     Select the one with the longest preferred time 
	     if there is. */
	  if (preferred > param->link_pref_time)
	    {
	      param->link_pref_time = preferred;
	      param->link_local = *local;
	    }
	}
      else if (!IN6_IS_ADDR_LOOPBACK(local) &&
	       !IN6_IS_ADDR_MULTICAST(local))
	{
	  int real_prefix = 0;
	  int do_slaac = 0;
	  int deprecate  = 0;
	  int constructed = 0;
	  int adv_router = 0;
	  int off_link = 0;
	  unsigned int time = 0xffffffff;
	  struct dhcp_context *context;
	  
	  for (context = daemon->dhcp6; context; context = context->next)
	    if (!(context->flags & (CONTEXT_TEMPLATE | CONTEXT_OLD)) &&
		prefix <= context->prefix &&
		is_same_net6(local, &context->start6, context->prefix) &&
		is_same_net6(local, &context->end6, context->prefix))
	      {
		context->saved_valid = valid;

		if (context->flags & CONTEXT_RA) 
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

		/* Configured to advertise router address, not prefix. See RFC 3775 7.2 
		 In this case we do all addresses associated with a context, 
		 hence the real_prefix setting here. */
		if (context->flags & CONTEXT_RA_ROUTER)
		  {
		    adv_router = 1;
		    param->adv_router = 1;
		    real_prefix = context->prefix;
		  }

		/* find floor time, don't reduce below 3 * RA interval. */
		if (time > context->lease_time)
		  {
		    time = context->lease_time;
		    if (time < ((unsigned int)(3 * param->adv_interval)))
		      time = 3 * param->adv_interval;
		  }

		if (context->flags & CONTEXT_DEPRECATE)
		  deprecate = 1;
		
		if (context->flags & CONTEXT_CONSTRUCTED)
		  constructed = 1;


		/* collect dhcp-range tags */
		if (context->netid.next == &context->netid && context->netid.net)
		  {
		    context->netid.next = param->tags;
		    param->tags = &context->netid;
		  }
		  
		/* subsequent prefixes on the same interface 
		   and subsequent instances of this prefix don't need timers.
		   Be careful not to find the same prefix twice with different
		   addresses unless we're advertising the actual addresses. */
		if (!(context->flags & CONTEXT_RA_DONE))
		  {
		    if (!param->first)
		      context->ra_time = 0;
		    context->flags |= CONTEXT_RA_DONE;
		    real_prefix = context->prefix;
                    off_link = (context->flags & CONTEXT_RA_OFF_LINK);
		  }

		param->first = 0;
		/* found_context is the _last_ one we found, so if there's 
		   more than one, it's not the first. */
		param->found_context = context;
	      }

	  /* configured time is ceiling */
	  if (!constructed || valid > time)
	    valid = time;
	  
	  if (flags & IFACE_DEPRECATED)
	    preferred = 0;
	  
	  if (deprecate)
	    time = 0;
	  
	  /* configured time is ceiling */
	  if (!constructed || preferred > time)
	    preferred = time;
	  
	  if (IN6_IS_ADDR_ULA(local))
	    {
	      if (preferred > param->ula_pref_time)
		{
		  param->ula_pref_time = preferred;
		  param->ula = *local;
		}
	    }
	  else 
	    {
	      if (preferred > param->glob_pref_time)
		{
		  param->glob_pref_time = preferred;
		  param->link_global = *local;
		}
	    }
	  
	  if (real_prefix != 0)
	    {
	      struct prefix_opt *opt;
	     	      
	      if ((opt = expand(sizeof(struct prefix_opt))))
		{
		  /* zero net part of address */
		  if (!adv_router)
		    setaddr6part(local, addr6part(local) & ~((real_prefix == 64) ? (u64)-1LL : (1LLU << (128 - real_prefix)) - 1LLU));
		  
		  opt->type = ICMP6_OPT_PREFIX;
		  opt->len = 4;
		  opt->prefix_len = real_prefix;
		  /* autonomous only if we're not doing dhcp, set
                     "on-link" unless "off-link" was specified */
		  opt->flags = (off_link ? 0 : 0x80);
		  if (do_slaac)
		    opt->flags |= 0x40;
		  if (adv_router)
		    opt->flags |= 0x20;
		  opt->valid_lifetime = htonl(valid);
		  opt->preferred_lifetime = htonl(preferred);
		  opt->reserved = 0; 
		  opt->prefix = *local;
		  
		  inet_ntop(AF_INET6, local, daemon->addrbuff, ADDRSTRLEN);
		  if (!option_bool(OPT_QUIET_RA))
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
  struct alias_param aparam;
    
  param.now = now;
  param.iface = 0;

  while (1)
    {
      /* find overdue events, and time of first future event */
      for (next_event = 0, context = daemon->dhcp6; context; context = context->next)
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
      
      if ((context->flags & CONTEXT_OLD) && 
	  context->if_index != 0 && 
	  indextoname(daemon->icmp6fd, context->if_index, param.name))
	{
	  /* A context for an old address. We'll not find the interface by 
	     looking for addresses, but we know it anyway, since the context is
	     constructed */
	  param.iface = context->if_index;
	  new_timeout(context, param.name, now);
	}
      else if (iface_enumerate(AF_INET6, &param, iface_search))
	/* There's a context overdue, but we can't find an interface
	   associated with it, because it's for a subnet we dont 
	   have an interface on. Probably we're doing DHCP on
	   a remote subnet via a relay. Zero the timer, since we won't
	   ever be able to send ra's and satisfy it. */
	context->ra_time = 0;
      
      if (param.iface != 0 &&
	  iface_check(AF_LOCAL, NULL, param.name, NULL))
	{
	  struct iname *tmp;
	  for (tmp = daemon->dhcp_except; tmp; tmp = tmp->next)
	    if (tmp->name && wildcard_match(tmp->name, param.name))
	      break;
	  if (!tmp)
            {
              send_ra(now, param.iface, param.name, NULL); 

              /* Also send on all interfaces that are aliases of this
                 one. */
              for (aparam.bridge = daemon->bridges;
                   aparam.bridge;
                   aparam.bridge = aparam.bridge->next)
                if ((int)if_nametoindex(aparam.bridge->iface) == param.iface)
                  {
                    /* Count the number of alias interfaces for this
                       'bridge', by calling iface_enumerate with
                       send_ra_to_aliases and NULL alias_ifs. */
                    aparam.iface = param.iface;
                    aparam.alias_ifs = NULL;
                    aparam.num_alias_ifs = 0;
                    iface_enumerate(AF_LOCAL, &aparam, send_ra_to_aliases);
                    my_syslog(MS_DHCP | LOG_INFO, "RTR-ADVERT(%s) %s => %d alias(es)",
                              param.name, daemon->addrbuff, aparam.num_alias_ifs);

                    /* Allocate memory to store the alias interface
                       indices. */
                    aparam.alias_ifs = (int *)whine_malloc(aparam.num_alias_ifs *
                                                           sizeof(int));
                    if (aparam.alias_ifs)
                      {
                        /* Use iface_enumerate again to get the alias
                           interface indices, then send on each of
                           those. */
                        aparam.max_alias_ifs = aparam.num_alias_ifs;
                        aparam.num_alias_ifs = 0;
                        iface_enumerate(AF_LOCAL, &aparam, send_ra_to_aliases);
                        for (; aparam.num_alias_ifs; aparam.num_alias_ifs--)
                          {
                            my_syslog(MS_DHCP | LOG_INFO, "RTR-ADVERT(%s) %s => i/f %d",
                                      param.name, daemon->addrbuff,
                                      aparam.alias_ifs[aparam.num_alias_ifs - 1]);
                            send_ra_alias(now,
                                          param.iface,
                                          param.name,
                                          NULL,
                                          aparam.alias_ifs[aparam.num_alias_ifs - 1]);
                          }
                        free(aparam.alias_ifs);
                      }

                    /* The source interface can only appear in at most
                       one --bridge-interface. */
                    break;
                  }
            }
	}
    }      
  return next_event;
}

static int send_ra_to_aliases(int index, unsigned int type, char *mac, size_t maclen, void *parm)
{
  struct alias_param *aparam = (struct alias_param *)parm;
  char ifrn_name[IFNAMSIZ];
  struct dhcp_bridge *alias;

  (void)type;
  (void)mac;
  (void)maclen;

  if (if_indextoname(index, ifrn_name))
    for (alias = aparam->bridge->alias; alias; alias = alias->next)
      if (wildcard_matchn(alias->iface, ifrn_name, IFNAMSIZ))
        {
          if (aparam->alias_ifs && (aparam->num_alias_ifs < aparam->max_alias_ifs))
            aparam->alias_ifs[aparam->num_alias_ifs] = index;
          aparam->num_alias_ifs++;
        }

  return 1;
}

static int iface_search(struct in6_addr *local,  int prefix,
			int scope, int if_index, int flags, 
			int preferred, int valid, void *vparam)
{
  struct search_param *param = vparam;
  struct dhcp_context *context;

  (void)scope;
  (void)preferred;
  (void)valid;
 
  for (context = daemon->dhcp6; context; context = context->next)
    if (!(context->flags & (CONTEXT_TEMPLATE | CONTEXT_OLD)) &&
	prefix <= context->prefix &&
	is_same_net6(local, &context->start6, context->prefix) &&
	is_same_net6(local, &context->end6, context->prefix) &&
	context->ra_time != 0 && 
	difftime(context->ra_time, param->now) <= 0.0)
      {
	/* found an interface that's overdue for RA determine new 
	   timeout value and arrange for RA to be sent unless interface is
	   still doing DAD.*/
	
	if (!(flags & IFACE_TENTATIVE))
	  param->iface = if_index;
	
	/* should never fail */
	if (!indextoname(daemon->icmp6fd, if_index, param->name))
	  {
	    param->iface = 0;
	    return 0;
	  }
	
	new_timeout(context, param->name, param->now);
	
	/* zero timers for other contexts on the same subnet, so they don't timeout 
	   independently */
	for (context = context->next; context; context = context->next)
	  if (prefix <= context->prefix &&
	      is_same_net6(local, &context->start6, context->prefix) &&
	      is_same_net6(local, &context->end6, context->prefix))
	    context->ra_time = 0;
	
	return 0; /* found, abort */
      }
  
  return 1; /* keep searching */
}
 
static void new_timeout(struct dhcp_context *context, char *iface_name, time_t now)
{
  if (difftime(now, context->ra_short_period_start) < 60.0)
    /* range 5 - 20 */
    context->ra_time = now + 5 + (rand16()/4400);
  else
    {
      /* range 3/4 - 1 times MaxRtrAdvInterval */
      unsigned int adv_interval = calc_interval(find_iface_param(iface_name));
      context->ra_time = now + (3 * adv_interval)/4 + ((adv_interval * (unsigned int)rand16()) >> 18);
    }
}

static struct ra_interface *find_iface_param(char *iface)
{
  struct ra_interface *ra;
  
  for (ra = daemon->ra_interfaces; ra; ra = ra->next)
    if (wildcard_match(ra->name, iface))
      return ra;

  return NULL;
}

static unsigned int calc_interval(struct ra_interface *ra)
{
  int interval = 600;
  
  if (ra && ra->interval != 0)
    {
      interval = ra->interval;
      if (interval > 1800)
	interval = 1800;
      else if (interval < 4)
	interval = 4;
    }
  
  return (unsigned int)interval;
}

static unsigned int calc_lifetime(struct ra_interface *ra)
{
  int lifetime, interval = (int)calc_interval(ra);
  
  if (!ra || ra->lifetime == -1) /* not specified */
    lifetime = 3 * interval;
  else
    {
      lifetime = ra->lifetime;
      if (lifetime < interval && lifetime != 0)
	lifetime = interval;
      else if (lifetime > 9000)
	lifetime = 9000;
    }
  
  return (unsigned int)lifetime;
}

static unsigned int calc_prio(struct ra_interface *ra)
{
  if (ra)
    return ra->prio;
  
  return 0;
}

#endif
