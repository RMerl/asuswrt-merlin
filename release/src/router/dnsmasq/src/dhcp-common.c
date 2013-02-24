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

#ifdef HAVE_DHCP

void dhcp_common_init(void)
{
    /* These each hold a DHCP option max size 255
       and get a terminating zero added */
  daemon->dhcp_buff = safe_malloc(256);
  daemon->dhcp_buff2 = safe_malloc(256); 
  daemon->dhcp_buff3 = safe_malloc(256);
  
  /* dhcp_packet is used by v4 and v6, outpacket only by v6 
     sizeof(struct dhcp_packet) is as good an initial size as any,
     even for v6 */
  expand_buf(&daemon->dhcp_packet, sizeof(struct dhcp_packet));
#ifdef HAVE_DHCP6
  if (daemon->dhcp6)
    expand_buf(&daemon->outpacket, sizeof(struct dhcp_packet));
#endif
}

ssize_t recv_dhcp_packet(int fd, struct msghdr *msg)
{  
  ssize_t sz;
 
  while (1)
    {
      msg->msg_flags = 0;
      while ((sz = recvmsg(fd, msg, MSG_PEEK | MSG_TRUNC)) == -1 && errno == EINTR);
      
      if (sz == -1)
	return -1;
      
      if (!(msg->msg_flags & MSG_TRUNC))
	break;

      /* Very new Linux kernels return the actual size needed, 
	 older ones always return truncated size */
      if ((size_t)sz == msg->msg_iov->iov_len)
	{
	  if (!expand_buf(msg->msg_iov, sz + 100))
	    return -1;
	}
      else
	{
	  expand_buf(msg->msg_iov, sz);
	  break;
	}
    }
  
  while ((sz = recvmsg(fd, msg, 0)) == -1 && errno == EINTR);
  
  return (msg->msg_flags & MSG_TRUNC) ? -1 : sz;
}

struct dhcp_netid *run_tag_if(struct dhcp_netid *tags)
{
  struct tag_if *exprs;
  struct dhcp_netid_list *list;

  for (exprs = daemon->tag_if; exprs; exprs = exprs->next)
    if (match_netid(exprs->tag, tags, 1))
      for (list = exprs->set; list; list = list->next)
	{
	  list->list->next = tags;
	  tags = list->list;
	}

  return tags;
}


struct dhcp_netid *option_filter(struct dhcp_netid *tags, struct dhcp_netid *context_tags, struct dhcp_opt *opts)
{
  struct dhcp_netid *tagif = run_tag_if(tags);
  struct dhcp_opt *opt;

  /* flag options which are valid with the current tag set (sans context tags) */
  for (opt = opts; opt; opt = opt->next)
    {
      opt->flags &= ~DHOPT_TAGOK;
      if (!(opt->flags & (DHOPT_ENCAPSULATE | DHOPT_VENDOR | DHOPT_RFC3925)) &&
	  match_netid(opt->netid, tagif, 0))
	opt->flags |= DHOPT_TAGOK;
    }

  /* now flag options which are valid, including the context tags,
     otherwise valid options are inhibited if we found a higher priority one above */
  if (context_tags)
    {
      struct dhcp_netid *last_tag;

      for (last_tag = context_tags; last_tag->next; last_tag = last_tag->next);
      last_tag->next = tags;
      tagif = run_tag_if(context_tags);
      
      /* reset stuff with tag:!<tag> which now matches. */
      for (opt = opts; opt; opt = opt->next)
	if (!(opt->flags & (DHOPT_ENCAPSULATE | DHOPT_VENDOR | DHOPT_RFC3925)) &&
	    (opt->flags & DHOPT_TAGOK) &&
	    !match_netid(opt->netid, tagif, 0))
	  opt->flags &= ~DHOPT_TAGOK;

      for (opt = opts; opt; opt = opt->next)
	if (!(opt->flags & (DHOPT_ENCAPSULATE | DHOPT_VENDOR | DHOPT_RFC3925 | DHOPT_TAGOK)) &&
	    match_netid(opt->netid, tagif, 0))
	  {
	    struct dhcp_opt *tmp;  
	    for (tmp = opts; tmp; tmp = tmp->next) 
	      if (tmp->opt == opt->opt && opt->netid && (tmp->flags & DHOPT_TAGOK))
		break;
	    if (!tmp)
	      opt->flags |= DHOPT_TAGOK;
	  }      
    }
  
  /* now flag untagged options which are not overridden by tagged ones */
  for (opt = opts; opt; opt = opt->next)
    if (!(opt->flags & (DHOPT_ENCAPSULATE | DHOPT_VENDOR | DHOPT_RFC3925 | DHOPT_TAGOK)) && !opt->netid)
      {
	struct dhcp_opt *tmp;  
	for (tmp = opts; tmp; tmp = tmp->next) 
	  if (tmp->opt == opt->opt && (tmp->flags & DHOPT_TAGOK))
	    break;
	if (!tmp)
	  opt->flags |= DHOPT_TAGOK;
	else if (!tmp->netid)
	  my_syslog(MS_DHCP | LOG_WARNING, _("Ignoring duplicate dhcp-option %d"), tmp->opt); 
      }

  return tagif;
}
	
/* Is every member of check matched by a member of pool? 
   If tagnotneeded, untagged is OK */
int match_netid(struct dhcp_netid *check, struct dhcp_netid *pool, int tagnotneeded)
{
  struct dhcp_netid *tmp1;
  
  if (!check && !tagnotneeded)
    return 0;

  for (; check; check = check->next)
    {
      /* '#' for not is for backwards compat. */
      if (check->net[0] != '!' && check->net[0] != '#')
	{
	  for (tmp1 = pool; tmp1; tmp1 = tmp1->next)
	    if (strcmp(check->net, tmp1->net) == 0)
	      break;
	  if (!tmp1)
	    return 0;
	}
      else
	for (tmp1 = pool; tmp1; tmp1 = tmp1->next)
	  if (strcmp((check->net)+1, tmp1->net) == 0)
	    return 0;
    }
  return 1;
}

/* return domain or NULL if none. */
char *strip_hostname(char *hostname)
{
  char *dot = strchr(hostname, '.');
 
  if (!dot)
    return NULL;
  
  *dot = 0; /* truncate */
  if (strlen(dot+1) != 0)
    return dot+1;
  
  return NULL;
}

void log_tags(struct dhcp_netid *netid, u32 xid)
{
  if (netid && option_bool(OPT_LOG_OPTS))
    {
      char *s = daemon->namebuff;
      for (*s = 0; netid; netid = netid->next)
	{
	  /* kill dupes. */
	  struct dhcp_netid *n;
	  
	  for (n = netid->next; n; n = n->next)
	    if (strcmp(netid->net, n->net) == 0)
	      break;
	  
	  if (!n)
	    {
	      strncat (s, netid->net, (MAXDNAME-1) - strlen(s));
	      if (netid->next)
		strncat (s, ", ", (MAXDNAME-1) - strlen(s));
	    }
	}
      my_syslog(MS_DHCP | LOG_INFO, _("%u tags: %s"), xid, s);
    } 
}   
  
int match_bytes(struct dhcp_opt *o, unsigned char *p, int len)
{
  int i;
  
  if (o->len > len)
    return 0;
  
  if (o->len == 0)
    return 1;
     
  if (o->flags & DHOPT_HEX)
    { 
      if (memcmp_masked(o->val, p, o->len, o->u.wildcard_mask))
	return 1;
    }
  else 
    for (i = 0; i <= (len - o->len); ) 
      {
	if (memcmp(o->val, p + i, o->len) == 0)
	  return 1;
	    
	if (o->flags & DHOPT_STRING)
	  i++;
	else
	  i += o->len;
      }
  
  return 0;
}

void dhcp_update_configs(struct dhcp_config *configs)
{
  /* Some people like to keep all static IP addresses in /etc/hosts.
     This goes through /etc/hosts and sets static addresses for any DHCP config
     records which don't have an address and whose name matches. 
     We take care to maintain the invariant that any IP address can appear
     in at most one dhcp-host. Since /etc/hosts can be re-read by SIGHUP, 
     restore the status-quo ante first. */
  
  struct dhcp_config *config, *conf_tmp;
  struct crec *crec;
  int prot = AF_INET;

  for (config = configs; config; config = config->next)
    if (config->flags & CONFIG_ADDR_HOSTS)
      config->flags &= ~(CONFIG_ADDR | CONFIG_ADDR6 | CONFIG_ADDR_HOSTS);

#ifdef HAVE_DHCP6 
 again:  
#endif

  if (daemon->port != 0)
    for (config = configs; config; config = config->next)
      {
	int conflags = CONFIG_ADDR;
	int cacheflags = F_IPV4;

#ifdef HAVE_DHCP6
	if (prot == AF_INET6)
	  {
	    conflags = CONFIG_ADDR6;
	    cacheflags = F_IPV6;
	  }
#endif
	if (!(config->flags & conflags) &&
	    (config->flags & CONFIG_NAME) && 
	    (crec = cache_find_by_name(NULL, config->hostname, 0, cacheflags)) &&
	    (crec->flags & F_HOSTS))
	  {
	    if (cache_find_by_name(crec, config->hostname, 0, cacheflags))
	      {
		/* use primary (first) address */
	      while (crec && !(crec->flags & F_REVERSE))
		crec = cache_find_by_name(crec, config->hostname, 0, cacheflags);
	      if (!crec)
		continue; /* should be never */
	      inet_ntop(prot, &crec->addr.addr, daemon->addrbuff, ADDRSTRLEN);
	      my_syslog(MS_DHCP | LOG_WARNING, _("%s has more than one address in hostsfile, using %s for DHCP"), 
			config->hostname, daemon->addrbuff);
	      }
	    
	    if (prot == AF_INET && 
		(!(conf_tmp = config_find_by_address(configs, crec->addr.addr.addr.addr4)) || conf_tmp == config))
	      {
		config->addr = crec->addr.addr.addr.addr4;
		config->flags |= CONFIG_ADDR | CONFIG_ADDR_HOSTS;
		continue;
	      }

#ifdef HAVE_DHCP6
	    if (prot == AF_INET6 && 
		(!(conf_tmp = config_find_by_address6(configs, &crec->addr.addr.addr.addr6, 128, 0)) || conf_tmp == config))
	      {
		memcpy(&config->addr6, &crec->addr.addr.addr.addr6, IN6ADDRSZ);
		config->flags |= CONFIG_ADDR6 | CONFIG_ADDR_HOSTS;
		continue;
	      }
#endif

	    inet_ntop(prot, &crec->addr.addr, daemon->addrbuff, ADDRSTRLEN);
	    my_syslog(MS_DHCP | LOG_WARNING, _("duplicate IP address %s (%s) in dhcp-config directive"), 
		      daemon->addrbuff, config->hostname);
	    
	    
	  }
      }

#ifdef HAVE_DHCP6
  if (prot == AF_INET)
    {
      prot = AF_INET6;
      goto again;
    }
#endif

}

#ifdef HAVE_DHCP6
static int join_multicast_worker(struct in6_addr *local, int prefix, 
				 int scope, int if_index, int dad, void *vparam)
{
  char ifrn_name[IFNAMSIZ];
  struct ipv6_mreq mreq;
  int fd, i, max = *((int *)vparam);
  struct iname *tmp;

  (void)prefix;
  (void)scope;
  (void)dad;
  
  /* record which interfaces we join on, so that we do it at most one per 
     interface, even when they have multiple addresses. Use outpacket
     as an array of int, since it's always allocated here and easy
     to expand for theoretical vast numbers of interfaces. */
  for (i = 0; i < max; i++)
    if (if_index == ((int *)daemon->outpacket.iov_base)[i])
      return 1;
  
  if ((fd = socket(PF_INET6, SOCK_DGRAM, 0)) == -1)
    return 0;
  
  if (!indextoname(fd, if_index, ifrn_name))
    {
      close(fd);
      return 0;
    }
  
  close(fd);

  /* Are we doing DHCP on this interface? */
  if (!iface_check(AF_INET6, (struct all_addr *)local, ifrn_name))
    return 1;
 
  for (tmp = daemon->dhcp_except; tmp; tmp = tmp->next)
    if (tmp->name && (strcmp(tmp->name, ifrn_name) == 0))
      return 1;

  mreq.ipv6mr_interface = if_index;
  
  inet_pton(AF_INET6, ALL_RELAY_AGENTS_AND_SERVERS, &mreq.ipv6mr_multiaddr);
  
  if (daemon->dhcp6 &&
      setsockopt(daemon->dhcp6fd, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mreq, sizeof(mreq)) == -1)
    return 0;

  inet_pton(AF_INET6, ALL_SERVERS, &mreq.ipv6mr_multiaddr);
  
  if (daemon->dhcp6 && 
      setsockopt(daemon->dhcp6fd, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mreq, sizeof(mreq)) == -1)
    return 0;
  
  inet_pton(AF_INET6, ALL_ROUTERS, &mreq.ipv6mr_multiaddr);
  
  if (daemon->ra_contexts &&
      setsockopt(daemon->icmp6fd, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mreq, sizeof(mreq)) == -1)
    return 0;
  
  expand_buf(&daemon->outpacket, (max+1) * sizeof(int));
  ((int *)daemon->outpacket.iov_base)[max++] = if_index;
  
  *((int *)vparam) = max;
  
  return 1;
}

void join_multicast(void)
{
  int count = 0;

   if (!iface_enumerate(AF_INET6, &count, join_multicast_worker))
     die(_("failed to join DHCPv6 multicast group: %s"), NULL, EC_BADNET);
}
#endif

#ifdef HAVE_LINUX_NETWORK 
void bindtodevice(int fd)
{
  /* If we are doing DHCP on exactly one interface, and running linux, do SO_BINDTODEVICE
     to that device. This is for the use case of  (eg) OpenStack, which runs a new
     dnsmasq instance for each VLAN interface it creates. Without the BINDTODEVICE, 
     individual processes don't always see the packets they should.
     SO_BINDTODEVICE is only available Linux. */
  
  struct irec *iface, *found;
  
  for (found = NULL, iface = daemon->interfaces; iface; iface = iface->next)
    if (iface->dhcp_ok)
      {
	if (!found)
	  found = iface;
	else if (strcmp(found->name, iface->name) != 0) 
	  {
	    /* more than one. */
	    found = NULL;
	    break;
	  }
      }
  
  if (found)
    {
      struct ifreq ifr;
      strcpy(ifr.ifr_name, found->name);
      /* only allowed by root. */
      if (setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, (void *)&ifr, sizeof(ifr)) == -1 &&
	  errno != EPERM)
	die(_("failed to set SO_BINDTODEVICE on DHCP socket: %s"), NULL, EC_BADNET);
    }
}
#endif

static const struct opttab_t {
  char *name;
  u16 val, size;
} opttab[] = {
  { "netmask", 1, OT_ADDR_LIST },
  { "time-offset", 2, 4 },
  { "router", 3, OT_ADDR_LIST  },
  { "dns-server", 6, OT_ADDR_LIST },
  { "log-server", 7, OT_ADDR_LIST },
  { "lpr-server", 9, OT_ADDR_LIST },
  { "hostname", 12, OT_INTERNAL | OT_NAME },
  { "boot-file-size", 13, 2 | OT_DEC },
  { "domain-name", 15, OT_NAME },
  { "swap-server", 16, OT_ADDR_LIST },
  { "root-path", 17, OT_NAME },
  { "extension-path", 18, OT_NAME },
  { "ip-forward-enable", 19, 1 },
  { "non-local-source-routing", 20, 1 },
  { "policy-filter", 21, OT_ADDR_LIST },
  { "max-datagram-reassembly", 22, 2 | OT_DEC },
  { "default-ttl", 23, 1 | OT_DEC },
  { "mtu", 26, 2 | OT_DEC },
  { "all-subnets-local", 27, 1 },
  { "broadcast", 28, OT_INTERNAL | OT_ADDR_LIST },
  { "router-discovery", 31, 1 },
  { "router-solicitation", 32, OT_ADDR_LIST },
  { "static-route", 33, OT_ADDR_LIST },
  { "trailer-encapsulation", 34, 1 },
  { "arp-timeout", 35, 4 | OT_DEC },
  { "ethernet-encap", 36, 1 },
  { "tcp-ttl", 37, 1 },
  { "tcp-keepalive", 38, 4 | OT_DEC },
  { "nis-domain", 40, OT_NAME },
  { "nis-server", 41, OT_ADDR_LIST },
  { "ntp-server", 42, OT_ADDR_LIST },
  { "vendor-encap", 43, OT_INTERNAL },
  { "netbios-ns", 44, OT_ADDR_LIST },
  { "netbios-dd", 45, OT_ADDR_LIST },
  { "netbios-nodetype", 46, 1 },
  { "netbios-scope", 47, 0 },
  { "x-windows-fs", 48, OT_ADDR_LIST },
  { "x-windows-dm", 49, OT_ADDR_LIST },
  { "requested-address", 50, OT_INTERNAL | OT_ADDR_LIST },
  { "lease-time", 51, OT_INTERNAL | OT_TIME },
  { "option-overload", 52, OT_INTERNAL },
  { "message-type", 53, OT_INTERNAL | OT_DEC },
  { "server-identifier", 54, OT_INTERNAL | OT_ADDR_LIST },
  { "parameter-request", 55, OT_INTERNAL },
  { "message", 56, OT_INTERNAL },
  { "max-message-size", 57, OT_INTERNAL },
  { "T1", 58, OT_INTERNAL | OT_TIME},
  { "T2", 59, OT_INTERNAL | OT_TIME},
  { "vendor-class", 60, 0 },
  { "client-id", 61, OT_INTERNAL },
  { "nis+-domain", 64, OT_NAME },
  { "nis+-server", 65, OT_ADDR_LIST },
  { "tftp-server", 66, OT_NAME },
  { "bootfile-name", 67, OT_NAME },
  { "mobile-ip-home", 68, OT_ADDR_LIST }, 
  { "smtp-server", 69, OT_ADDR_LIST }, 
  { "pop3-server", 70, OT_ADDR_LIST }, 
  { "nntp-server", 71, OT_ADDR_LIST }, 
  { "irc-server", 74, OT_ADDR_LIST }, 
  { "user-class", 77, 0 },
  { "FQDN", 81, OT_INTERNAL },
  { "agent-id", 82, OT_INTERNAL },
  { "client-arch", 93, 2 | OT_DEC },
  { "client-interface-id", 94, 0 },
  { "client-machine-id", 97, 0 },
  { "subnet-select", 118, OT_INTERNAL },
  { "domain-search", 119, OT_RFC1035_NAME },
  { "sip-server", 120, 0 },
  { "classless-static-route", 121, 0 },
  { "vendor-id-encap", 125, 0 },
  { "server-ip-address", 255, OT_ADDR_LIST }, /* special, internal only, sets siaddr */
  { NULL, 0, 0 }
};

#ifdef HAVE_DHCP6
static const struct opttab_t opttab6[] = {
  { "client-id", 1, OT_INTERNAL },
  { "server-id", 2, OT_INTERNAL },
  { "ia-na", 3, OT_INTERNAL },
  { "ia-ta", 4, OT_INTERNAL },
  { "iaaddr", 5, OT_INTERNAL },
  { "oro", 6, OT_INTERNAL },
  { "preference", 7, OT_INTERNAL | OT_DEC },
  { "unicast", 12, OT_INTERNAL },
  { "status", 13, OT_INTERNAL },
  { "rapid-commit", 14, OT_INTERNAL },
  { "user-class", 15, OT_INTERNAL | OT_CSTRING },
  { "vendor-class", 16, OT_INTERNAL | OT_CSTRING },
  { "vendor-opts", 17, OT_INTERNAL },
  { "sip-server-domain", 21,  OT_RFC1035_NAME },
  { "sip-server", 22, OT_ADDR_LIST },
  { "dns-server", 23, OT_ADDR_LIST },
  { "domain-search", 24, OT_RFC1035_NAME },
  { "nis-server", 27, OT_ADDR_LIST },
  { "nis+-server", 28, OT_ADDR_LIST },
  { "nis-domain", 29,  OT_RFC1035_NAME },
  { "nis+-domain", 30, OT_RFC1035_NAME },
  { "sntp-server", 31,  OT_ADDR_LIST },
  { "information-refresh-time", 32, OT_TIME },
  { "FQDN", 39, OT_INTERNAL | OT_RFC1035_NAME },
  { "ntp-server", 56,  OT_ADDR_LIST },
  { "bootfile-url", 59, OT_NAME },
  { "bootfile-param", 60, OT_CSTRING },
  { NULL, 0, 0 }
};
#endif



void display_opts(void)
{
  int i;
  
  printf(_("Known DHCP options:\n"));
  
  for (i = 0; opttab[i].name; i++)
    if (!(opttab[i].size & OT_INTERNAL))
      printf("%3d %s\n", opttab[i].val, opttab[i].name);
}

#ifdef HAVE_DHCP6
void display_opts6(void)
{
  int i;
  printf(_("Known DHCPv6 options:\n"));
  
  for (i = 0; opttab6[i].name; i++)
    if (!(opttab6[i].size & OT_INTERNAL))
      printf("%3d %s\n", opttab6[i].val, opttab6[i].name);
}
#endif

u16 lookup_dhcp_opt(int prot, char *name)
{
  const struct opttab_t *t;
  int i;

#ifdef HAVE_DHCP6
  if (prot == AF_INET6)
    t = opttab6;
  else
#endif
    t = opttab;

  for (i = 0; t[i].name; i++)
    if (!(t[i].size & OT_INTERNAL) &&
	strcasecmp(t[i].name, name) == 0)
      return t[i].val;
  
  return 0;
}

u16 lookup_dhcp_len(int prot, u16 val)
{
  const struct opttab_t *t;
  int i;

#ifdef HAVE_DHCP6
  if (prot == AF_INET6)
    t = opttab6;
  else
#endif
    t = opttab;

  for (i = 0; t[i].name; i++)
    if (val == t[i].val)
      {
	if (t[i].size & OT_INTERNAL)
	  return 0;
	
	return t[i].size & ~OT_DEC;
      }
 
  return 0;
}

char *option_string(int prot, unsigned int opt, unsigned char *val, int opt_len, char *buf, int buf_len)
{
  int o, i, j, nodecode = 0;
  const struct opttab_t *ot = opttab;

#ifdef HAVE_DHCP6
  if (prot == AF_INET6)
    ot = opttab6;
#endif

  for (o = 0; ot[o].name; o++)
    if (ot[o].val == opt)
      {
	if (buf)
	  {
	    memset(buf, 0, buf_len);
	    
	    if (ot[o].size & OT_ADDR_LIST) 
	      {
		struct all_addr addr;
		int addr_len = INADDRSZ;

#ifdef HAVE_DHCP6
		if (prot == AF_INET6)
		  addr_len = IN6ADDRSZ;
#endif
		for (buf[0]= 0, i = 0; i <= opt_len - addr_len; i += addr_len) 
		  {
		    if (i != 0)
		      strncat(buf, ", ", buf_len - strlen(buf));
		    /* align */
		    memcpy(&addr, &val[i], addr_len); 
		    inet_ntop(prot, &val[i], daemon->addrbuff, ADDRSTRLEN);
		    strncat(buf, daemon->addrbuff, buf_len - strlen(buf));
		  }
	      }
	    else if (ot[o].size & OT_NAME)
		for (i = 0, j = 0; i < opt_len && j < buf_len ; i++)
		  {
		    char c = val[i];
		    if (isprint((int)c))
		      buf[j++] = c;
		  }
#ifdef HAVE_DHCP6
	    /* We don't handle compressed rfc1035 names, so no good in IPv4 land */
	    else if ((ot[o].size & OT_RFC1035_NAME) && prot == AF_INET6)
	      {
		i = 0, j = 0;
		while (i < opt_len && val[i] != 0)
		  {
		    int k, l = i + val[i] + 1;
		    for (k = i + 1; k < opt_len && k < l && j < buf_len ; k++)
		     {
		       char c = val[k];
		       if (isprint((int)c))
			 buf[j++] = c;
		     }
		    i = l;
		    if (val[i] != 0 && j < buf_len)
		      buf[j++] = '.';
		  }
	      }
	    else if ((ot[o].size & OT_CSTRING))
	      {
		int k, len;
		unsigned char *p;

		i = 0, j = 0;
		while (1)
		  {
		    p = &val[i];
		    GETSHORT(len, p);
		    for (k = 0; k < len && j < buf_len; k++)
		      {
		       char c = *p++;
		       if (isprint((int)c))
			 buf[j++] = c;
		     }
		    i += len +2;
		    if (i >= opt_len)
		      break;

		    if (j < buf_len)
		      buf[j++] = ',';
		  }
	      }	      
#endif
	    else if ((ot[o].size & (OT_DEC | OT_TIME)) && opt_len != 0)
	      {
		unsigned int dec = 0;
		
		for (i = 0; i < opt_len; i++)
		  dec = (dec << 8) | val[i]; 

		if (ot[o].size & OT_TIME)
		  prettyprint_time(buf, dec);
		else
		  sprintf(buf, "%u", dec);
	      }
	    else
	      nodecode = 1;
	  }
	break;
      }

  if (opt_len != 0 && buf && (!ot[o].name || nodecode))
    {
      int trunc  = 0;
      if (opt_len > 14)
	{
	  trunc = 1;
	  opt_len = 14;
	}
      print_mac(buf, val, opt_len);
      if (trunc)
	strncat(buf, "...", buf_len - strlen(buf));
    

    }

  return ot[o].name ? ot[o].name : "";

}

#endif
