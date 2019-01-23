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

static struct dhcp_lease *leases = NULL, *old_leases = NULL;
static int dns_dirty, file_dirty, leases_left;

static int read_leases(time_t now, FILE *leasestream)
{
  unsigned long ei;
  struct all_addr addr;
  struct dhcp_lease *lease;
  int clid_len, hw_len, hw_type;
  int items;
  char *domain = NULL;

  *daemon->dhcp_buff3 = *daemon->dhcp_buff2 = '\0';

  /* client-id max length is 255 which is 255*2 digits + 254 colons
     borrow DNS packet buffer which is always larger than 1000 bytes

     Check various buffers are big enough for the code below */

#if (DHCP_BUFF_SZ < 255) || (MAXDNAME < 64) || (PACKETSZ+MAXDNAME+RRFIXEDSZ  < 764)
# error Buffer size breakage in leasefile parsing.
#endif

    while ((items=fscanf(leasestream, "%255s %255s", daemon->dhcp_buff3, daemon->dhcp_buff2)) == 2)
      {
	*daemon->namebuff = *daemon->dhcp_buff = *daemon->packet = '\0';
	hw_len = hw_type = clid_len = 0;
	
#ifdef HAVE_DHCP6
	if (strcmp(daemon->dhcp_buff3, "duid") == 0)
	  {
	    daemon->duid_len = parse_hex(daemon->dhcp_buff2, (unsigned char *)daemon->dhcp_buff2, 130, NULL, NULL);
	    if (daemon->duid_len < 0)
	      return 0;
	    daemon->duid = safe_malloc(daemon->duid_len);
	    memcpy(daemon->duid, daemon->dhcp_buff2, daemon->duid_len);
	    continue;
	  }
#endif
	
	if (fscanf(leasestream, " %64s %255s %764s",
		   daemon->namebuff, daemon->dhcp_buff, daemon->packet) != 3)
	  return 0;
	
	if (inet_pton(AF_INET, daemon->namebuff, &addr.addr.addr4))
	  {
	    if ((lease = lease4_allocate(addr.addr.addr4)))
	      domain = get_domain(lease->addr);
	    
	    hw_len = parse_hex(daemon->dhcp_buff2, (unsigned char *)daemon->dhcp_buff2, DHCP_CHADDR_MAX, NULL, &hw_type);
	    /* For backwards compatibility, no explicit MAC address type means ether. */
	    if (hw_type == 0 && hw_len != 0)
	      hw_type = ARPHRD_ETHER; 
	  }
#ifdef HAVE_DHCP6
	else if (inet_pton(AF_INET6, daemon->namebuff, &addr.addr.addr6))
	  {
	    char *s = daemon->dhcp_buff2;
	    int lease_type = LEASE_NA;

	    if (s[0] == 'T')
	      {
		lease_type = LEASE_TA;
		s++;
	      }
	    
	    if ((lease = lease6_allocate(&addr.addr.addr6, lease_type)))
	      {
		lease_set_iaid(lease, strtoul(s, NULL, 10));
		domain = get_domain6((struct in6_addr *)lease->hwaddr);
	      }
	  }
#endif
	else
	  return 0;

	if (!lease)
	  die (_("too many stored leases"), NULL, EC_MISC);

	if (strcmp(daemon->packet, "*") != 0)
	  clid_len = parse_hex(daemon->packet, (unsigned char *)daemon->packet, 255, NULL, NULL);
	
	lease_set_hwaddr(lease, (unsigned char *)daemon->dhcp_buff2, (unsigned char *)daemon->packet, 
			 hw_len, hw_type, clid_len, now, 0);
	
	if (strcmp(daemon->dhcp_buff, "*") !=  0)
	  lease_set_hostname(lease, daemon->dhcp_buff, 0, domain, NULL);

	ei = atol(daemon->dhcp_buff3);

#if defined(HAVE_BROKEN_RTC) || defined(HAVE_LEASEFILE_EXPIRE)
	if (ei != 0)
	  lease->expires = (time_t)ei + now;
	else
	  lease->expires = (time_t)0;
#ifdef HAVE_BROKEN_RTC
	lease->length = ei;
#endif
#else
	/* strictly time_t is opaque, but this hack should work on all sane systems,
	   even when sizeof(time_t) == 8 */
	lease->expires = (time_t)ei;
#endif
	
	/* set these correctly: the "old" events are generated later from
	   the startup synthesised SIGHUP. */
	lease->flags &= ~(LEASE_NEW | LEASE_CHANGED);
	
	*daemon->dhcp_buff3 = *daemon->dhcp_buff2 = '\0';
      }
    
    return (items == 0 || items == EOF);
}

void lease_init(time_t now)
{
  FILE *leasestream;

  leases_left = daemon->dhcp_max;

  if (option_bool(OPT_LEASE_RO))
    {
      /* run "<lease_change_script> init" once to get the
	 initial state of the database. If leasefile-ro is
	 set without a script, we just do without any
	 lease database. */
#ifdef HAVE_SCRIPT
      if (daemon->lease_change_command)
	{
	  strcpy(daemon->dhcp_buff, daemon->lease_change_command);
	  strcat(daemon->dhcp_buff, " init");
	  leasestream = popen(daemon->dhcp_buff, "r");
	}
      else
#endif
	{
          file_dirty = dns_dirty = 0;
          return;
        }

    }
  else
    {
      /* NOTE: need a+ mode to create file if it doesn't exist */
      leasestream = daemon->lease_stream = fopen(daemon->lease_file, "a+");

      if (!leasestream)
	die(_("cannot open or create lease file %s: %s"), daemon->lease_file, EC_FILE);

      /* a+ mode leaves pointer at end. */
      rewind(leasestream);
    }

  if (leasestream)
    {
      if (!read_leases(now, leasestream))
	my_syslog(MS_DHCP | LOG_ERR, _("failed to parse lease database, invalid line: %s %s %s %s ..."),
		  daemon->dhcp_buff3, daemon->dhcp_buff2,
		  daemon->namebuff, daemon->dhcp_buff);

      if (ferror(leasestream))
	die(_("failed to read lease file %s: %s"), daemon->lease_file, EC_FILE);
    }
  
#ifdef HAVE_SCRIPT
  if (!daemon->lease_stream)
    {
      int rc = 0;

      /* shell returns 127 for "command not found", 126 for bad permissions. */
      if (!leasestream || (rc = pclose(leasestream)) == -1 || WEXITSTATUS(rc) == 127 || WEXITSTATUS(rc) == 126)
	{
	  if (WEXITSTATUS(rc) == 127)
	    errno = ENOENT;
	  else if (WEXITSTATUS(rc) == 126)
	    errno = EACCES;

	  die(_("cannot run lease-init script %s: %s"), daemon->lease_change_command, EC_FILE);
	}
      
      if (WEXITSTATUS(rc) != 0)
	{
	  sprintf(daemon->dhcp_buff, "%d", WEXITSTATUS(rc));
	  die(_("lease-init script returned exit code %s"), daemon->dhcp_buff, WEXITSTATUS(rc) + EC_INIT_OFFSET);
	}
    }
#endif

  /* Some leases may have expired */
  file_dirty = 0;
  lease_prune(NULL, now);
  dns_dirty = 1;
}

void lease_update_from_configs(void)
{
  /* changes to the config may change current leases. */
  
  struct dhcp_lease *lease;
  struct dhcp_config *config;
  char *name;
  
  for (lease = leases; lease; lease = lease->next)
    if (lease->flags & (LEASE_TA | LEASE_NA))
      continue;
    else if ((config = find_config(daemon->dhcp_conf, NULL, lease->clid, lease->clid_len, 
				   lease->hwaddr, lease->hwaddr_len, lease->hwaddr_type, NULL)) && 
	     (config->flags & CONFIG_NAME) &&
	     (!(config->flags & CONFIG_ADDR) || config->addr.s_addr == lease->addr.s_addr))
      lease_set_hostname(lease, config->hostname, 1, get_domain(lease->addr), NULL);
    else if ((name = host_from_dns(lease->addr)))
      lease_set_hostname(lease, name, 1, get_domain(lease->addr), NULL); /* updates auth flag only */
}

static void ourprintf(int *errp, char *format, ...)
{
  va_list ap;
  
  va_start(ap, format);
  if (!(*errp) && vfprintf(daemon->lease_stream, format, ap) < 0)
    *errp = errno;
  va_end(ap);
}

#ifdef HAVE_LEASEFILE_EXPIRE
void lease_flush_file(time_t now)
{
  static time_t flush_time = 0;

  if (difftime(flush_time, now) < 0)
    file_dirty = 1;

  lease_prune(NULL, now);
  lease_update_file(now);

  if (file_dirty == 0)
    flush_time = now;
}
#endif

void lease_update_file(time_t now)
{
  struct dhcp_lease *lease;
  time_t next_event;
  int i, err = 0;

  if (file_dirty != 0 && daemon->lease_stream)
    {
      errno = 0;
      rewind(daemon->lease_stream);
      if (errno != 0 || ftruncate(fileno(daemon->lease_stream), 0) != 0)
	err = errno;
      
      for (lease = leases; lease; lease = lease->next)
	{

#ifdef HAVE_DHCP6
	  if (lease->flags & (LEASE_TA | LEASE_NA))
	    continue;
#endif

#ifdef HAVE_LEASEFILE_EXPIRE
	  ourprintf(&err, "%u ",
#ifdef HAVE_BROKEN_RTC
		    (lease->length == 0) ? 0 :
#else
		    (lease->expires == 0) ? 0 :
#endif
		    (unsigned int)difftime(lease->expires, now));
#elif defined(HAVE_BROKEN_RTC)
	  ourprintf(&err, "%u ", lease->length);
#else
	  ourprintf(&err, "%lu ", (unsigned long)lease->expires);
#endif

	  if (lease->hwaddr_type != ARPHRD_ETHER || lease->hwaddr_len == 0) 
	    ourprintf(&err, "%.2x-", lease->hwaddr_type);
	  for (i = 0; i < lease->hwaddr_len; i++)
	    {
	      ourprintf(&err, "%.2x", lease->hwaddr[i]);
	      if (i != lease->hwaddr_len - 1)
		ourprintf(&err, ":");
	    }
	  
	  inet_ntop(AF_INET, &lease->addr, daemon->addrbuff, ADDRSTRLEN); 

	  ourprintf(&err, " %s ", daemon->addrbuff);
	  ourprintf(&err, "%s ", lease->hostname ? lease->hostname : "*");
	  	  
	  if (lease->clid && lease->clid_len != 0)
	    {
	      for (i = 0; i < lease->clid_len - 1; i++)
		ourprintf(&err, "%.2x:", lease->clid[i]);
	      ourprintf(&err, "%.2x\n", lease->clid[i]);
	    }
	  else
	    ourprintf(&err, "*\n");	  
	}
      
#ifdef HAVE_DHCP6  
      if (daemon->duid)
	{
	  ourprintf(&err, "duid ");
	  for (i = 0; i < daemon->duid_len - 1; i++)
	    ourprintf(&err, "%.2x:", daemon->duid[i]);
	  ourprintf(&err, "%.2x\n", daemon->duid[i]);
	  
	  for (lease = leases; lease; lease = lease->next)
	    {
	      
	      if (!(lease->flags & (LEASE_TA | LEASE_NA)))
		continue;

#ifdef HAVE_LEASEFILE_EXPIRE
	      ourprintf(&err, "%u ",
#ifdef HAVE_BROKEN_RTC
			(lease->length == 0) ? 0 :
#else
			(lease->expires == 0) ? 0 :
#endif
			(unsigned int)difftime(lease->expires, now));
#elif defined(HAVE_BROKEN_RTC)
	      ourprintf(&err, "%u ", lease->length);
#else
	      ourprintf(&err, "%lu ", (unsigned long)lease->expires);
#endif
    
	      inet_ntop(AF_INET6, &lease->addr6, daemon->addrbuff, ADDRSTRLEN);
	 
	      ourprintf(&err, "%s%u %s ", (lease->flags & LEASE_TA) ? "T" : "",
			lease->iaid, daemon->addrbuff);
	      ourprintf(&err, "%s ", lease->hostname ? lease->hostname : "*");
	      
	      if (lease->clid && lease->clid_len != 0)
		{
		  for (i = 0; i < lease->clid_len - 1; i++)
		    ourprintf(&err, "%.2x:", lease->clid[i]);
		  ourprintf(&err, "%.2x\n", lease->clid[i]);
		}
	      else
		ourprintf(&err, "*\n");	  
	    }
	}
#endif      
	  
      if (fflush(daemon->lease_stream) != 0 ||
	  fsync(fileno(daemon->lease_stream)) < 0)
	err = errno;
      
      if (!err)
	file_dirty = 0;
    }
  
  /* Set alarm for when the first lease expires. */
  next_event = 0;

#ifdef HAVE_DHCP6
  /* do timed RAs and determine when the next is, also pings to potential SLAAC addresses */
  if (daemon->doing_ra)
    {
      time_t event;
      
      if ((event = periodic_slaac(now, leases)) != 0)
	{
	  if (next_event == 0 || difftime(next_event, event) > 0.0)
	    next_event = event;
	}
      
      if ((event = periodic_ra(now)) != 0)
	{
	  if (next_event == 0 || difftime(next_event, event) > 0.0)
	    next_event = event;
	}
    }
#endif

  for (lease = leases; lease; lease = lease->next)
    if (lease->expires != 0 &&
	(next_event == 0 || difftime(next_event, lease->expires) > 0.0))
      next_event = lease->expires;
   
  if (err)
    {
      if (next_event == 0 || difftime(next_event, LEASE_RETRY + now) > 0.0)
	next_event = LEASE_RETRY + now;
      
      my_syslog(MS_DHCP | LOG_ERR, _("failed to write %s: %s (retry in %us)"), 
		daemon->lease_file, strerror(err),
		(unsigned int)difftime(next_event, now));
    }

  send_alarm(next_event, now);
}


static int find_interface_v4(struct in_addr local, int if_index, char *label,
			     struct in_addr netmask, struct in_addr broadcast, void *vparam)
{
  struct dhcp_lease *lease;
  int prefix = netmask_length(netmask);

  (void) label;
  (void) broadcast;
  (void) vparam;

  for (lease = leases; lease; lease = lease->next)
    if (!(lease->flags & (LEASE_TA | LEASE_NA)) &&
	is_same_net(local, lease->addr, netmask) && 
	prefix > lease->new_prefixlen) 
      {
	lease->new_interface = if_index;
        lease->new_prefixlen = prefix;
      }

  return 1;
}

#ifdef HAVE_DHCP6
static int find_interface_v6(struct in6_addr *local,  int prefix,
			     int scope, int if_index, int flags, 
			     int preferred, int valid, void *vparam)
{
  struct dhcp_lease *lease;

  (void)scope;
  (void)flags;
  (void)preferred;
  (void)valid;
  (void)vparam;

  for (lease = leases; lease; lease = lease->next)
    if ((lease->flags & (LEASE_TA | LEASE_NA)))
      if (is_same_net6(local, &lease->addr6, prefix) && prefix > lease->new_prefixlen) {
        /* save prefix length for comparison, as we might get shorter matching
         * prefix in upcoming netlink GETADDR responses
         * */
        lease->new_interface = if_index;
        lease->new_prefixlen = prefix;
      }

  return 1;
}

void lease_ping_reply(struct in6_addr *sender, unsigned char *packet, char *interface)
{
  /* We may be doing RA but not DHCPv4, in which case the lease
     database may not exist and we have nothing to do anyway */
  if (daemon->dhcp)
    slaac_ping_reply(sender, packet, interface, leases);
}

void lease_update_slaac(time_t now)
{
  /* Called when we construct a new RA-names context, to add putative
     new SLAAC addresses to existing leases. */

  struct dhcp_lease *lease;
  
  if (daemon->dhcp)
    for (lease = leases; lease; lease = lease->next)
      slaac_add_addrs(lease, now, 0);
}

#endif


/* Find interfaces associated with leases at start-up. This gets updated as
   we do DHCP transactions, but information about directly-connected subnets
   is useful from scrips and necessary for determining SLAAC addresses from
   start-time. */
void lease_find_interfaces(time_t now)
{
  struct dhcp_lease *lease;
  
  for (lease = leases; lease; lease = lease->next)
    lease->new_prefixlen = lease->new_interface = 0;

  iface_enumerate(AF_INET, &now, find_interface_v4);
#ifdef HAVE_DHCP6
  iface_enumerate(AF_INET6, &now, find_interface_v6);
#endif

  for (lease = leases; lease; lease = lease->next)
    if (lease->new_interface != 0) 
      lease_set_interface(lease, lease->new_interface, now);
}

#ifdef HAVE_DHCP6
void lease_make_duid(time_t now)
{
  /* If we're not doing DHCPv6, and there are not v6 leases, don't add the DUID to the database */
  if (!daemon->duid && daemon->doing_dhcp6)
    {
      file_dirty = 1;
      make_duid(now);
    }
}
#endif




void lease_update_dns(int force)
{
  struct dhcp_lease *lease;

  if (daemon->port != 0 && (dns_dirty || force))
    {
#ifndef HAVE_BROKEN_RTC
      /* force transfer to authoritative secondaries */
      daemon->soa_sn++;
#endif
      
      cache_unhash_dhcp();

      for (lease = leases; lease; lease = lease->next)
	{
	  int prot = AF_INET;
	  
#ifdef HAVE_DHCP6
	  if (lease->flags & (LEASE_TA | LEASE_NA))
	    prot = AF_INET6;
	  else if (lease->hostname || lease->fqdn)
	    {
	      struct slaac_address *slaac;

	      for (slaac = lease->slaac_address; slaac; slaac = slaac->next)
		if (slaac->backoff == 0)
		  {
		    if (lease->fqdn)
		      cache_add_dhcp_entry(lease->fqdn, AF_INET6, (struct all_addr *)&slaac->addr, lease->expires);
		    if (!option_bool(OPT_DHCP_FQDN) && lease->hostname)
		      cache_add_dhcp_entry(lease->hostname, AF_INET6, (struct all_addr *)&slaac->addr, lease->expires);
		  }
	    }
	  
	  if (lease->fqdn)
	    cache_add_dhcp_entry(lease->fqdn, prot, 
				 prot == AF_INET ? (struct all_addr *)&lease->addr : (struct all_addr *)&lease->addr6,
				 lease->expires);
	     
	  if (!option_bool(OPT_DHCP_FQDN) && lease->hostname)
	    cache_add_dhcp_entry(lease->hostname, prot, 
				 prot == AF_INET ? (struct all_addr *)&lease->addr : (struct all_addr *)&lease->addr6, 
				 lease->expires);
       
#else
	  if (lease->fqdn)
	    cache_add_dhcp_entry(lease->fqdn, prot, (struct all_addr *)&lease->addr, lease->expires);
	  
	  if (!option_bool(OPT_DHCP_FQDN) && lease->hostname)
	    cache_add_dhcp_entry(lease->hostname, prot, (struct all_addr *)&lease->addr, lease->expires);
#endif
	}
      
      dns_dirty = 0;
    }
}

void lease_prune(struct dhcp_lease *target, time_t now)
{
  struct dhcp_lease *lease, *tmp, **up;

  for (lease = leases, up = &leases; lease; lease = tmp)
    {
      tmp = lease->next;
      if ((lease->expires != 0 && difftime(now, lease->expires) > 0) || lease == target)
	{
	  file_dirty = 1;
	  if (lease->hostname)
	    dns_dirty = 1;
	  
 	  *up = lease->next; /* unlink */
	  
	  /* Put on old_leases list 'till we
	     can run the script */
	  lease->next = old_leases;
	  old_leases = lease;
	  
	  leases_left++;
	}
      else
	up = &lease->next;
    }
} 
	
  
struct dhcp_lease *lease_find_by_client(unsigned char *hwaddr, int hw_len, int hw_type,
					unsigned char *clid, int clid_len)
{
  struct dhcp_lease *lease;

  if (clid)
    for (lease = leases; lease; lease = lease->next)
      {
#ifdef HAVE_DHCP6
	if (lease->flags & (LEASE_TA | LEASE_NA))
	  continue;
#endif
	if (lease->clid && clid_len == lease->clid_len &&
	    memcmp(clid, lease->clid, clid_len) == 0)
	  return lease;
      }
  
  for (lease = leases; lease; lease = lease->next)	
    {
#ifdef HAVE_DHCP6
      if (lease->flags & (LEASE_TA | LEASE_NA))
	continue;
#endif   
      if ((!lease->clid || !clid) && 
	  hw_len != 0 && 
	  lease->hwaddr_len == hw_len &&
	  lease->hwaddr_type == hw_type &&
	  memcmp(hwaddr, lease->hwaddr, hw_len) == 0)
	return lease;
    }

  return NULL;
}

struct dhcp_lease *lease_find_by_addr(struct in_addr addr)
{
  struct dhcp_lease *lease;

  for (lease = leases; lease; lease = lease->next)
    {
#ifdef HAVE_DHCP6
      if (lease->flags & (LEASE_TA | LEASE_NA))
	continue;
#endif  
      if (lease->addr.s_addr == addr.s_addr)
	return lease;
    }

  return NULL;
}

#ifdef HAVE_DHCP6
/* find address for {CLID, IAID, address} */
struct dhcp_lease *lease6_find(unsigned char *clid, int clid_len, 
			       int lease_type, int iaid, struct in6_addr *addr)
{
  struct dhcp_lease *lease;
  
  for (lease = leases; lease; lease = lease->next)
    {
      if (!(lease->flags & lease_type) || lease->iaid != iaid)
	continue;

      if (!IN6_ARE_ADDR_EQUAL(&lease->addr6, addr))
	continue;
      
      if ((clid_len != lease->clid_len ||
	   memcmp(clid, lease->clid, clid_len) != 0))
	continue;
      
      return lease;
    }
  
  return NULL;
}

/* reset "USED flags */
void lease6_reset(void)
{
  struct dhcp_lease *lease;
  
  for (lease = leases; lease; lease = lease->next)
    lease->flags &= ~LEASE_USED;
}

/* enumerate all leases belonging to {CLID, IAID} */
struct dhcp_lease *lease6_find_by_client(struct dhcp_lease *first, int lease_type, unsigned char *clid, int clid_len, int iaid)
{
  struct dhcp_lease *lease;

  if (!first)
    first = leases;
  else
    first = first->next;

  for (lease = first; lease; lease = lease->next)
    {
      if (lease->flags & LEASE_USED)
	continue;

      if (!(lease->flags & lease_type) || lease->iaid != iaid)
	continue;
 
      if ((clid_len != lease->clid_len ||
	   memcmp(clid, lease->clid, clid_len) != 0))
	continue;

      return lease;
    }
  
  return NULL;
}

struct dhcp_lease *lease6_find_by_addr(struct in6_addr *net, int prefix, u64 addr)
{
  struct dhcp_lease *lease;
    
  for (lease = leases; lease; lease = lease->next)
    {
      if (!(lease->flags & (LEASE_TA | LEASE_NA)))
	continue;
      
      if (is_same_net6(&lease->addr6, net, prefix) &&
	  (prefix == 128 || addr6part(&lease->addr6) == addr))
	return lease;
    }
  
  return NULL;
} 

/* Find largest assigned address in context */
u64 lease_find_max_addr6(struct dhcp_context *context)
{
  struct dhcp_lease *lease;
  u64 addr = addr6part(&context->start6);
  
  if (!(context->flags & (CONTEXT_STATIC | CONTEXT_PROXY)))
    for (lease = leases; lease; lease = lease->next)
      {
	if (!(lease->flags & (LEASE_TA | LEASE_NA)))
	  continue;

	if (is_same_net6(&lease->addr6, &context->start6, 64) &&
	    addr6part(&lease->addr6) > addr6part(&context->start6) &&
	    addr6part(&lease->addr6) <= addr6part(&context->end6) &&
	    addr6part(&lease->addr6) > addr)
	  addr = addr6part(&lease->addr6);
      }
  
  return addr;
}

#endif

/* Find largest assigned address in context */
struct in_addr lease_find_max_addr(struct dhcp_context *context)
{
  struct dhcp_lease *lease;
  struct in_addr addr = context->start;
  
  if (!(context->flags & (CONTEXT_STATIC | CONTEXT_PROXY)))
    for (lease = leases; lease; lease = lease->next)
      {
#ifdef HAVE_DHCP6
	if (lease->flags & (LEASE_TA | LEASE_NA))
	  continue;
#endif
	if (((unsigned)ntohl(lease->addr.s_addr)) > ((unsigned)ntohl(context->start.s_addr)) &&
	    ((unsigned)ntohl(lease->addr.s_addr)) <= ((unsigned)ntohl(context->end.s_addr)) &&
	    ((unsigned)ntohl(lease->addr.s_addr)) > ((unsigned)ntohl(addr.s_addr)))
	  addr = lease->addr;
      }
  
  return addr;
}

static struct dhcp_lease *lease_allocate(void)
{
  struct dhcp_lease *lease;
  if (!leases_left || !(lease = whine_malloc(sizeof(struct dhcp_lease))))
    return NULL;

  memset(lease, 0, sizeof(struct dhcp_lease));
  lease->flags = LEASE_NEW;
  lease->expires = 1;
#ifdef HAVE_BROKEN_RTC
  lease->length = 0xffffffff; /* illegal value */
#endif
  lease->hwaddr_len = 256; /* illegal value */
  lease->next = leases;
  leases = lease;
  
  file_dirty = 1;
  leases_left--;

  return lease;
}

struct dhcp_lease *lease4_allocate(struct in_addr addr)
{
  struct dhcp_lease *lease = lease_allocate();
  if (lease)
    lease->addr = addr;
  
  return lease;
}

#ifdef HAVE_DHCP6
struct dhcp_lease *lease6_allocate(struct in6_addr *addrp, int lease_type)
{
  struct dhcp_lease *lease = lease_allocate();

  if (lease)
    {
      lease->addr6 = *addrp;
      lease->flags |= lease_type;
      lease->iaid = 0;
    }

  return lease;
}
#endif

void lease_set_expires(struct dhcp_lease *lease, unsigned int len, time_t now)
{
  time_t exp;

  if (len == 0xffffffff)
    {
      exp = 0;
      len = 0;
    }
  else
    {
      exp = now + (time_t)len;
      /* Check for 2038 overflow. Make the lease
	 infinite in that case, as the least disruptive
	 thing we can do. */
      if (difftime(exp, now) <= 0.0)
	exp = 0;
    }

  if (exp != lease->expires)
    {
      dns_dirty = 1;
      lease->expires = exp;
#ifndef HAVE_BROKEN_RTC
      lease->flags |= LEASE_AUX_CHANGED;
      file_dirty = 1;
#endif
    }
  
#ifdef HAVE_BROKEN_RTC
  if (len != lease->length)
    {
      lease->length = len;
      lease->flags |= LEASE_AUX_CHANGED;
      file_dirty = 1; 
    }
#endif
} 

#ifdef HAVE_DHCP6
void lease_set_iaid(struct dhcp_lease *lease, int iaid)
{
  if (lease->iaid != iaid)
    {
      lease->iaid = iaid;
      lease->flags |= LEASE_CHANGED;
    }
}
#endif

void lease_set_hwaddr(struct dhcp_lease *lease, const unsigned char *hwaddr,
		      const unsigned char *clid, int hw_len, int hw_type,
		      int clid_len, time_t now, int force)
{
#ifdef HAVE_DHCP6
  int change = force;
  lease->flags |= LEASE_HAVE_HWADDR;
#endif

  (void)force;
  (void)now;

  if (hw_len != lease->hwaddr_len ||
      hw_type != lease->hwaddr_type || 
      (hw_len != 0 && memcmp(lease->hwaddr, hwaddr, hw_len) != 0))
    {
      if (hw_len != 0)
	memcpy(lease->hwaddr, hwaddr, hw_len);
      lease->hwaddr_len = hw_len;
      lease->hwaddr_type = hw_type;
      lease->flags |= LEASE_CHANGED;
      file_dirty = 1; /* run script on change */
    }

  /* only update clid when one is available, stops packets
     without a clid removing the record. Lease init uses
     clid_len == 0 for no clid. */
  if (clid_len != 0 && clid)
    {
      if (!lease->clid)
	lease->clid_len = 0;

      if (lease->clid_len != clid_len)
	{
	  lease->flags |= LEASE_AUX_CHANGED;
	  file_dirty = 1;
	  free(lease->clid);
	  if (!(lease->clid = whine_malloc(clid_len)))
	    return;
#ifdef HAVE_DHCP6
	  change = 1;
#endif	   
	}
      else if (memcmp(lease->clid, clid, clid_len) != 0)
	{
	  lease->flags |= LEASE_AUX_CHANGED;
	  file_dirty = 1;
#ifdef HAVE_DHCP6
	  change = 1;
#endif	
	}
      
      lease->clid_len = clid_len;
      memcpy(lease->clid, clid, clid_len);
    }
  
#ifdef HAVE_DHCP6
  if (change)
    slaac_add_addrs(lease, now, force);
#endif
}

static void kill_name(struct dhcp_lease *lease)
{
  /* run script to say we lost our old name */
  
  /* this shouldn't happen unless updates are very quick and the
     script very slow, we just avoid a memory leak if it does. */
  free(lease->old_hostname);
  
  /* If we know the fqdn, pass that. The helper will derive the
     unqualified name from it, free the unqualified name here. */

  if (lease->fqdn)
    {
      lease->old_hostname = lease->fqdn;
      free(lease->hostname);
    }
  else
    lease->old_hostname = lease->hostname;

  lease->hostname = lease->fqdn = NULL;
}

void lease_set_hostname(struct dhcp_lease *lease, const char *name, int auth, char *domain, char *config_domain)
{
  struct dhcp_lease *lease_tmp;
  char *new_name = NULL, *new_fqdn = NULL;

  if (config_domain && (!domain || !hostname_isequal(domain, config_domain)))
    my_syslog(MS_DHCP | LOG_WARNING, _("Ignoring domain %s for DHCP host name %s"), config_domain, name);
  
  if (lease->hostname && name && hostname_isequal(lease->hostname, name))
    {
      if (auth)
	lease->flags |= LEASE_AUTH_NAME;
      return;
    }
  
  if (!name && !lease->hostname)
    return;

  /* If a machine turns up on a new net without dropping the old lease,
     or two machines claim the same name, then we end up with two interfaces with
     the same name. Check for that here and remove the name from the old lease.
     Note that IPv6 leases are different. All the leases to the same DUID are 
     allowed the same name.

     Don't allow a name from the client to override a name from dnsmasq config. */
  
  if (name)
    {
      if ((new_name = whine_malloc(strlen(name) + 1)))
	{
	  strcpy(new_name, name);
	  if (domain && (new_fqdn = whine_malloc(strlen(new_name) + strlen(domain) + 2)))
	    {
	      strcpy(new_fqdn, name);
	      strcat(new_fqdn, ".");
	      strcat(new_fqdn, domain);
	    }
	}
	  
      /* Depending on mode, we check either unqualified name or FQDN. */
      for (lease_tmp = leases; lease_tmp; lease_tmp = lease_tmp->next)
	{
	  if (option_bool(OPT_DHCP_FQDN))
	    {
	      if (!new_fqdn || !lease_tmp->fqdn || !hostname_isequal(lease_tmp->fqdn, new_fqdn))
		continue;
	    }
	  else
	    {
	      if (!new_name || !lease_tmp->hostname || !hostname_isequal(lease_tmp->hostname, new_name) )
		continue; 
	    }

	  if (lease->flags & (LEASE_TA | LEASE_NA))
	    {
	      if (!(lease_tmp->flags & (LEASE_TA | LEASE_NA)))
		continue;

	      /* another lease for the same DUID is OK for IPv6 */
	      if (lease->clid_len == lease_tmp->clid_len &&
		  lease->clid && lease_tmp->clid &&
		  memcmp(lease->clid, lease_tmp->clid, lease->clid_len) == 0)
		continue;	      
	    }
	  else if (lease_tmp->flags & (LEASE_TA | LEASE_NA))
	    continue;
		   
	  if ((lease_tmp->flags & LEASE_AUTH_NAME) && !auth)
	    {
	      free(new_name);
	      free(new_fqdn);
	      return;
	    }
	
	  kill_name(lease_tmp);
	  break;
	}
    }

  if (lease->hostname)
    kill_name(lease);

  lease->hostname = new_name;
  lease->fqdn = new_fqdn;
  
  if (auth)
    lease->flags |= LEASE_AUTH_NAME;
  
  file_dirty = 1;
  dns_dirty = 1; 
  lease->flags |= LEASE_CHANGED; /* run script on change */
}

void lease_set_interface(struct dhcp_lease *lease, int interface, time_t now)
{
  (void)now;

  if (lease->last_interface == interface)
    return;

  lease->last_interface = interface;
  lease->flags |= LEASE_CHANGED; 

#ifdef HAVE_DHCP6
  slaac_add_addrs(lease, now, 0);
#endif
}

void rerun_scripts(void)
{
  struct dhcp_lease *lease;
  
  for (lease = leases; lease; lease = lease->next)
    lease->flags |= LEASE_CHANGED; 
}

/* deleted leases get transferred to the old_leases list.
   remove them here, after calling the lease change
   script. Also run the lease change script on new/modified leases.

   Return zero if nothing to do. */
int do_script_run(time_t now)
{
  struct dhcp_lease *lease;

  (void)now;

#ifdef HAVE_DBUS
  /* If we're going to be sending DBus signals, but the connection is not yet up,
     delay everything until it is. */
  if (option_bool(OPT_DBUS) && !daemon->dbus)
    return 0;
#endif

  if (old_leases)
    {
      lease = old_leases;
                  
      /* If the lease still has an old_hostname, do the "old" action on that first */
      if (lease->old_hostname)
	{
#ifdef HAVE_SCRIPT
	  queue_script(ACTION_OLD_HOSTNAME, lease, lease->old_hostname, now);
#endif
	  free(lease->old_hostname);
	  lease->old_hostname = NULL;
	  return 1;
	}
      else 
	{
#ifdef HAVE_DHCP6
	  struct slaac_address *slaac, *tmp;
	  for (slaac = lease->slaac_address; slaac; slaac = tmp)
	    {
	      tmp = slaac->next;
	      free(slaac);
	    }
#endif
	  kill_name(lease);
#ifdef HAVE_SCRIPT
	  queue_script(ACTION_DEL, lease, lease->old_hostname, now);
#endif
#ifdef HAVE_DBUS
	  emit_dbus_signal(ACTION_DEL, lease, lease->old_hostname);
#endif
	  old_leases = lease->next;
	  
	  free(lease->old_hostname); 
	  free(lease->clid);
	  free(lease->extradata);
	  free(lease);
	    
	  return 1; 
	}
    }
  
  /* make sure we announce the loss of a hostname before its new location. */
  for (lease = leases; lease; lease = lease->next)
    if (lease->old_hostname)
      {	
#ifdef HAVE_SCRIPT
	queue_script(ACTION_OLD_HOSTNAME, lease, lease->old_hostname, now);
#endif
	free(lease->old_hostname);
	lease->old_hostname = NULL;
	return 1;
      }
  
  for (lease = leases; lease; lease = lease->next)
    if ((lease->flags & (LEASE_NEW | LEASE_CHANGED)) || 
	((lease->flags & LEASE_AUX_CHANGED) && option_bool(OPT_LEASE_RO)))
      {
#ifdef HAVE_SCRIPT
	queue_script((lease->flags & LEASE_NEW) ? ACTION_ADD : ACTION_OLD, lease, 
		     lease->fqdn ? lease->fqdn : lease->hostname, now);
#endif
#ifdef HAVE_DBUS
	emit_dbus_signal((lease->flags & LEASE_NEW) ? ACTION_ADD : ACTION_OLD, lease,
			 lease->fqdn ? lease->fqdn : lease->hostname);
#endif
	lease->flags &= ~(LEASE_NEW | LEASE_CHANGED | LEASE_AUX_CHANGED);
	
	/* this is used for the "add" call, then junked, since they're not in the database */
	free(lease->extradata);
	lease->extradata = NULL;
	
	return 1;
      }

  return 0; /* nothing to do */
}

#ifdef HAVE_SCRIPT
/* delim == -1 -> delim = 0, but embedded 0s, creating extra records, are OK. */
void lease_add_extradata(struct dhcp_lease *lease, unsigned char *data, unsigned int len, int delim)
{
  unsigned int i;
  
  if (delim == -1)
    delim = 0;
  else
    /* check for embedded NULLs */
    for (i = 0; i < len; i++)
      if (data[i] == 0)
	{
	  len = i;
	  break;
	}
  
  if ((lease->extradata_size - lease->extradata_len) < (len + 1))
    {
      size_t newsz = lease->extradata_len + len + 100;
      unsigned char *new = whine_malloc(newsz);
  
      if (!new)
	return;
      
      if (lease->extradata)
	{
	  memcpy(new, lease->extradata, lease->extradata_len);
	  free(lease->extradata);
	}

      lease->extradata = new;
      lease->extradata_size = newsz;
    }

  if (len != 0)
    memcpy(lease->extradata + lease->extradata_len, data, len);
  lease->extradata[lease->extradata_len + len] = delim;
  lease->extradata_len += len + 1; 
}
#endif

#endif
	  

      

