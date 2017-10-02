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

#ifdef HAVE_DHCP

void dhcp_common_init(void)
{
  /* These each hold a DHCP option max size 255
     and get a terminating zero added */
  daemon->dhcp_buff = safe_malloc(DHCP_BUFF_SZ);
  daemon->dhcp_buff2 = safe_malloc(DHCP_BUFF_SZ); 
  daemon->dhcp_buff3 = safe_malloc(DHCP_BUFF_SZ);
  
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
  struct dhcp_opt *tmp;  

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
	for (tmp = opts; tmp; tmp = tmp->next) 
	  if (tmp->opt == opt->opt && (tmp->flags & DHOPT_TAGOK))
	    break;
	if (!tmp)
	  opt->flags |= DHOPT_TAGOK;
	else if (!tmp->netid)
	  my_syslog(MS_DHCP | LOG_WARNING, _("Ignoring duplicate dhcp-option %d"), tmp->opt); 
      }

  /* Finally, eliminate duplicate options later in the chain, and therefore earlier in the config file. */
  for (opt = opts; opt; opt = opt->next)
    if (opt->flags & DHOPT_TAGOK)
      for (tmp = opt->next; tmp; tmp = tmp->next) 
	if (tmp->opt == opt->opt)
	  tmp->flags &= ~DHOPT_TAGOK;
  
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

int config_has_mac(struct dhcp_config *config, unsigned char *hwaddr, int len, int type)
{
  struct hwaddr_config *conf_addr;
  
  for (conf_addr = config->hwaddr; conf_addr; conf_addr = conf_addr->next)
    if (conf_addr->wildcard_mask == 0 &&
	conf_addr->hwaddr_len == len &&
	(conf_addr->hwaddr_type == type || conf_addr->hwaddr_type == 0) &&
	memcmp(conf_addr->hwaddr, hwaddr, len) == 0)
      return 1;
  
  return 0;
}

static int is_config_in_context(struct dhcp_context *context, struct dhcp_config *config)
{
  if (!context) /* called via find_config() from lease_update_from_configs() */
    return 1; 

  if (!(config->flags & (CONFIG_ADDR | CONFIG_ADDR6)))
    return 1;
  
#ifdef HAVE_DHCP6
  if ((context->flags & CONTEXT_V6) && (config->flags & CONFIG_WILDCARD))
    return 1;
#endif

  for (; context; context = context->current)
#ifdef HAVE_DHCP6
    if (context->flags & CONTEXT_V6) 
      {
	if ((config->flags & CONFIG_ADDR6) && is_same_net6(&config->addr6, &context->start6, context->prefix))
	  return 1;
      }
    else 
#endif
      if ((config->flags & CONFIG_ADDR) && is_same_net(config->addr, context->start, context->netmask))
	return 1;

  return 0;
}

struct dhcp_config *find_config(struct dhcp_config *configs,
				struct dhcp_context *context,
				unsigned char *clid, int clid_len,
				unsigned char *hwaddr, int hw_len, 
				int hw_type, char *hostname)
{
  int count, new;
  struct dhcp_config *config, *candidate; 
  struct hwaddr_config *conf_addr;

  if (clid)
    for (config = configs; config; config = config->next)
      if (config->flags & CONFIG_CLID)
	{
	  if (config->clid_len == clid_len && 
	      memcmp(config->clid, clid, clid_len) == 0 &&
	      is_config_in_context(context, config))
	    return config;
	  
	  /* dhcpcd prefixes ASCII client IDs by zero which is wrong, but we try and
	     cope with that here. This is IPv4 only. context==NULL implies IPv4, 
	     see lease_update_from_configs() */
	  if ((!context || !(context->flags & CONTEXT_V6)) && *clid == 0 && config->clid_len == clid_len-1  &&
	      memcmp(config->clid, clid+1, clid_len-1) == 0 &&
	      is_config_in_context(context, config))
	    return config;
	}
  

  if (hwaddr)
    for (config = configs; config; config = config->next)
      if (config_has_mac(config, hwaddr, hw_len, hw_type) &&
	  is_config_in_context(context, config))
	return config;
  
  if (hostname && context)
    for (config = configs; config; config = config->next)
      if ((config->flags & CONFIG_NAME) && 
	  hostname_isequal(config->hostname, hostname) &&
	  is_config_in_context(context, config))
	return config;

  
  if (!hwaddr)
    return NULL;

  /* use match with fewest wildcard octets */
  for (candidate = NULL, count = 0, config = configs; config; config = config->next)
    if (is_config_in_context(context, config))
      for (conf_addr = config->hwaddr; conf_addr; conf_addr = conf_addr->next)
	if (conf_addr->wildcard_mask != 0 &&
	    conf_addr->hwaddr_len == hw_len &&	
	    (conf_addr->hwaddr_type == hw_type || conf_addr->hwaddr_type == 0) &&
	    (new = memcmp_masked(conf_addr->hwaddr, hwaddr, hw_len, conf_addr->wildcard_mask)) > count)
	  {
	      count = new;
	      candidate = config;
	  }
  
  return candidate;
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

#ifdef HAVE_LINUX_NETWORK 
char *whichdevice(void)
{
  /* If we are doing DHCP on exactly one interface, and running linux, do SO_BINDTODEVICE
     to that device. This is for the use case of  (eg) OpenStack, which runs a new
     dnsmasq instance for each VLAN interface it creates. Without the BINDTODEVICE, 
     individual processes don't always see the packets they should.
     SO_BINDTODEVICE is only available Linux. 

     Note that if wildcards are used in --interface, or --interface is not used at all,
     or a configured interface doesn't yet exist, then more interfaces may arrive later, 
     so we can't safely assert there is only one interface and proceed.
*/
  
  struct irec *iface, *found;
  struct iname *if_tmp;
  
  if (!daemon->if_names)
    return NULL;
  
  for (if_tmp = daemon->if_names; if_tmp; if_tmp = if_tmp->next)
    if (if_tmp->name && (!if_tmp->used || strchr(if_tmp->name, '*')))
      return NULL;

  for (found = NULL, iface = daemon->interfaces; iface; iface = iface->next)
    if (iface->dhcp_ok)
      {
	if (!found)
	  found = iface;
	else if (strcmp(found->name, iface->name) != 0) 
	  return NULL; /* more than one. */
      }

  if (found)
    return found->name;

  return NULL;
}
 
void  bindtodevice(char *device, int fd)
{
  struct ifreq ifr;
  
  strcpy(ifr.ifr_name, device);
  /* only allowed by root. */
  if (setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, (void *)&ifr, sizeof(ifr)) == -1 &&
      errno != EPERM)
    die(_("failed to set SO_BINDTODEVICE on DHCP socket: %s"), NULL, EC_BADNET);
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
  { "T1", 58, OT_TIME},
  { "T2", 59, OT_TIME},
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
  { "ntp-server", 56,  0 },
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

int lookup_dhcp_opt(int prot, char *name)
{
  const struct opttab_t *t;
  int i;

  (void)prot;

#ifdef HAVE_DHCP6
  if (prot == AF_INET6)
    t = opttab6;
  else
#endif
    t = opttab;

  for (i = 0; t[i].name; i++)
    if (strcasecmp(t[i].name, name) == 0)
      return t[i].val;
  
  return -1;
}

int lookup_dhcp_len(int prot, int val)
{
  const struct opttab_t *t;
  int i;

  (void)prot;

#ifdef HAVE_DHCP6
  if (prot == AF_INET6)
    t = opttab6;
  else
#endif
    t = opttab;

  for (i = 0; t[i].name; i++)
    if (val == t[i].val)
      return t[i].size & ~OT_DEC;

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

void log_context(int family, struct dhcp_context *context)
{
  /* Cannot use dhcp_buff* for RA contexts */

  void *start = &context->start;
  void *end = &context->end;
  char *template = "", *p = daemon->namebuff;
  
  *p = 0;
    
#ifdef HAVE_DHCP6
  if (family == AF_INET6)
    {
      struct in6_addr subnet = context->start6;
      if (!(context->flags & CONTEXT_TEMPLATE))
	setaddr6part(&subnet, 0);
      inet_ntop(AF_INET6, &subnet, daemon->addrbuff, ADDRSTRLEN); 
      start = &context->start6;
      end = &context->end6;
    }
#endif

  if (family != AF_INET && (context->flags & CONTEXT_DEPRECATE))
    strcpy(daemon->namebuff, _(", prefix deprecated"));
  else
    {
      p += sprintf(p, _(", lease time "));
      prettyprint_time(p, context->lease_time);
      p += strlen(p);
    }	

#ifdef HAVE_DHCP6
  if (context->flags & CONTEXT_CONSTRUCTED)
    {
      char ifrn_name[IFNAMSIZ];
      
      template = p;
      p += sprintf(p, ", ");
      
      if (indextoname(daemon->icmp6fd, context->if_index, ifrn_name))
	sprintf(p, "%s for %s", (context->flags & CONTEXT_OLD) ? "old prefix" : "constructed", ifrn_name);
    }
  else if (context->flags & CONTEXT_TEMPLATE && !(context->flags & CONTEXT_RA_STATELESS))
    {
      template = p;
      p += sprintf(p, ", ");
      
      sprintf(p, "template for %s", context->template_interface);  
    }
#endif
     
  if (!(context->flags & CONTEXT_OLD) &&
      ((context->flags & CONTEXT_DHCP) || family == AF_INET)) 
    {
#ifdef HAVE_DHCP6
      if (context->flags & CONTEXT_RA_STATELESS)
	{
	  if (context->flags & CONTEXT_TEMPLATE)
	    strncpy(daemon->dhcp_buff, context->template_interface, DHCP_BUFF_SZ);
	  else
	    strcpy(daemon->dhcp_buff, daemon->addrbuff);
	}
      else 
#endif
	inet_ntop(family, start, daemon->dhcp_buff, DHCP_BUFF_SZ);
      inet_ntop(family, end, daemon->dhcp_buff3, DHCP_BUFF_SZ);
      my_syslog(MS_DHCP | LOG_INFO, 
		(context->flags & CONTEXT_RA_STATELESS) ? 
		_("%s stateless on %s%.0s%.0s%s") :
		(context->flags & CONTEXT_STATIC) ? 
		_("%s, static leases only on %.0s%s%s%.0s") :
		(context->flags & CONTEXT_PROXY) ?
		_("%s, proxy on subnet %.0s%s%.0s%.0s") :
		_("%s, IP range %s -- %s%s%.0s"),
		(family != AF_INET) ? "DHCPv6" : "DHCP",
		daemon->dhcp_buff, daemon->dhcp_buff3, daemon->namebuff, template);
    }
  
#ifdef HAVE_DHCP6
  if (context->flags & CONTEXT_TEMPLATE)
    {
      strcpy(daemon->addrbuff, context->template_interface);
      template = "";
    }

  if ((context->flags & CONTEXT_RA_NAME) && !(context->flags & CONTEXT_OLD))
    my_syslog(MS_DHCP | LOG_INFO, _("DHCPv4-derived IPv6 names on %s%s"), daemon->addrbuff, template);
  
  if ((context->flags & CONTEXT_RA) || (option_bool(OPT_RA) && (context->flags & CONTEXT_DHCP) && family == AF_INET6)) 
    my_syslog(MS_DHCP | LOG_INFO, _("router advertisement on %s%s"), daemon->addrbuff, template);
#endif

}

void log_relay(int family, struct dhcp_relay *relay)
{
  inet_ntop(family, &relay->local, daemon->addrbuff, ADDRSTRLEN);
  inet_ntop(family, &relay->server, daemon->namebuff, ADDRSTRLEN); 

  if (relay->interface)
    my_syslog(MS_DHCP | LOG_INFO, _("DHCP relay from %s to %s via %s"), daemon->addrbuff, daemon->namebuff, relay->interface);
  else
    my_syslog(MS_DHCP | LOG_INFO, _("DHCP relay from %s to %s"), daemon->addrbuff, daemon->namebuff);
}
   
#endif
