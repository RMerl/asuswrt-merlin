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

#ifdef HAVE_DHCP6

#include <netinet/icmp6.h>

static int ping_id = 0;

void slaac_add_addrs(struct dhcp_lease *lease, time_t now, int force)
{
  struct slaac_address *slaac, *old, **up;
  struct dhcp_context *context;
  int dns_dirty = 0;
  
  if (!(lease->flags & LEASE_HAVE_HWADDR) || 
      (lease->flags & (LEASE_TA | LEASE_NA)) ||
      lease->last_interface == 0 ||
      !lease->hostname)
    return ;
  
  old = lease->slaac_address;
  lease->slaac_address = NULL;

  for (context = daemon->dhcp6; context; context = context->next) 
    if ((context->flags & CONTEXT_RA_NAME) && 
	!(context->flags & CONTEXT_OLD) &&
	lease->last_interface == context->if_index)
      {
	struct in6_addr addr = context->start6;
	if (lease->hwaddr_len == 6 &&
	    (lease->hwaddr_type == ARPHRD_ETHER || lease->hwaddr_type == ARPHRD_IEEE802))
	  {
	    /* convert MAC address to EUI-64 */
	    memcpy(&addr.s6_addr[8], lease->hwaddr, 3);
	    memcpy(&addr.s6_addr[13], &lease->hwaddr[3], 3);
	    addr.s6_addr[11] = 0xff;
	    addr.s6_addr[12] = 0xfe;
	  }
#if defined(ARPHRD_EUI64)
	else if (lease->hwaddr_len == 8 &&
		 lease->hwaddr_type == ARPHRD_EUI64)
	  memcpy(&addr.s6_addr[8], lease->hwaddr, 8);
#endif
#if defined(ARPHRD_IEEE1394) && defined(ARPHRD_EUI64)
	else if (lease->clid_len == 9 && 
		 lease->clid[0] ==  ARPHRD_EUI64 &&
		 lease->hwaddr_type == ARPHRD_IEEE1394)
	  /* firewire has EUI-64 identifier as clid */
	  memcpy(&addr.s6_addr[8], &lease->clid[1], 8);
#endif
	else
	  continue;
	
	addr.s6_addr[8] ^= 0x02;
	
	/* check if we already have this one */
	for (up = &old, slaac = old; slaac; slaac = slaac->next)
	  {
	    if (IN6_ARE_ADDR_EQUAL(&addr, &slaac->addr))
	      {
		*up = slaac->next;
		/* recheck when DHCPv4 goes through init-reboot */
		if (force)
		  {
		    slaac->ping_time = now;
		    slaac->backoff = 1;
		    dns_dirty = 1;
		  }
		break;
	      }
	    up = &slaac->next;
	  }
	    
	/* No, make new one */
	if (!slaac && (slaac = whine_malloc(sizeof(struct slaac_address))))
	  {
	    slaac->ping_time = now;
	    slaac->backoff = 1;
	    slaac->addr = addr;
	    /* Do RA's to prod it */
	    ra_start_unsolicted(now, context);
	  }
	
	if (slaac)
	  {
	    slaac->next = lease->slaac_address;
	    lease->slaac_address = slaac;
	  }
      }
  
  if (old || dns_dirty)
    lease_update_dns(1);
  
  /* Free any no reused */
  for (; old; old = slaac)
    {
      slaac = old->next;
      free(old);
    }
}


time_t periodic_slaac(time_t now, struct dhcp_lease *leases)
{
  struct dhcp_context *context;
  struct dhcp_lease *lease;
  struct slaac_address *slaac;
  time_t next_event = 0;
  
  for (context = daemon->dhcp6; context; context = context->next)
    if ((context->flags & CONTEXT_RA_NAME) && !(context->flags & CONTEXT_OLD))
      break;

  /* nothing configured */
  if (!context)
    return 0;

  while (ping_id == 0)
    ping_id = rand16();

  for (lease = leases; lease; lease = lease->next)
    for (slaac = lease->slaac_address; slaac; slaac = slaac->next)
      {
	/* confirmed or given up? */
	if (slaac->backoff == 0 || slaac->ping_time == 0)
	  continue;
	
	if (difftime(slaac->ping_time, now) <= 0.0)
	  {
	    struct ping_packet *ping;
	    struct sockaddr_in6 addr;
 
	    save_counter(0);
	    ping = expand(sizeof(struct ping_packet));
	    ping->type = ICMP6_ECHO_REQUEST;
	    ping->code = 0;
	    ping->identifier = ping_id;
	    ping->sequence_no = slaac->backoff;
	    
	    memset(&addr, 0, sizeof(addr));
#ifdef HAVE_SOCKADDR_SA_LEN
	    addr.sin6_len = sizeof(struct sockaddr_in6);
#endif
	    addr.sin6_family = AF_INET6;
	    addr.sin6_port = htons(IPPROTO_ICMPV6);
	    addr.sin6_addr = slaac->addr;
	    
	    if (sendto(daemon->icmp6fd, daemon->outpacket.iov_base, save_counter(0), 0,
		       (struct sockaddr *)&addr,  sizeof(addr)) == -1 &&
		errno == EHOSTUNREACH)
	      slaac->ping_time = 0; /* Give up */ 
	    else
	      {
		slaac->ping_time += (1 << (slaac->backoff - 1)) + (rand16()/21785); /* 0 - 3 */
		if (slaac->backoff > 4)
		  slaac->ping_time += rand16()/4000; /* 0 - 15 */
		if (slaac->backoff < 12)
		  slaac->backoff++;
	      }
	  }
	
	if (slaac->ping_time != 0 &&
	    (next_event == 0 || difftime(next_event, slaac->ping_time) >= 0.0))
	  next_event = slaac->ping_time;
      }

  return next_event;
}


void slaac_ping_reply(struct in6_addr *sender, unsigned char *packet, char *interface, struct dhcp_lease *leases)
{
  struct dhcp_lease *lease;
  struct slaac_address *slaac;
  struct ping_packet *ping = (struct ping_packet *)packet;
  int gotone = 0;
  
  if (ping->identifier == ping_id)
    for (lease = leases; lease; lease = lease->next)
      for (slaac = lease->slaac_address; slaac; slaac = slaac->next)
	if (slaac->backoff != 0 && IN6_ARE_ADDR_EQUAL(sender, &slaac->addr))
	  {
	    slaac->backoff = 0;
	    gotone = 1;
	    inet_ntop(AF_INET6, sender, daemon->addrbuff, ADDRSTRLEN);
	    if (!option_bool(OPT_QUIET_DHCP6))
	      my_syslog(MS_DHCP | LOG_INFO, "SLAAC-CONFIRM(%s) %s %s", interface, daemon->addrbuff, lease->hostname); 
	  }
  
  lease_update_dns(gotone);
}
	
#endif
