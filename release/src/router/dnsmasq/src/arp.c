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

/* Time between forced re-loads from kernel. */
#define INTERVAL 90

#define ARP_MARK  0
#define ARP_FOUND 1  /* Confirmed */
#define ARP_NEW   2  /* Newly created */
#define ARP_EMPTY 3  /* No MAC addr */

struct arp_record {
  unsigned short hwlen, status;
  int family;
  unsigned char hwaddr[DHCP_CHADDR_MAX]; 
  struct all_addr addr;
  struct arp_record *next;
};

static struct arp_record *arps = NULL, *old = NULL, *freelist = NULL;
static time_t last = 0;

static int filter_mac(int family, char *addrp, char *mac, size_t maclen, void *parmv)
{
  struct arp_record *arp;

  (void)parmv;

  if (maclen > DHCP_CHADDR_MAX)
    return 1;

#ifndef HAVE_IPV6
  if (family != AF_INET)
    return 1;
#endif

  /* Look for existing entry */
  for (arp = arps; arp; arp = arp->next)
    {
      if (family != arp->family || arp->status == ARP_NEW)
	continue;
      
      if (family == AF_INET)
	{
	  if (arp->addr.addr.addr4.s_addr != ((struct in_addr *)addrp)->s_addr)
	    continue;
	}
#ifdef HAVE_IPV6
      else
	{
	  if (!IN6_ARE_ADDR_EQUAL(&arp->addr.addr.addr6, (struct in6_addr *)addrp))
	    continue;
	}
#endif

      if (arp->status == ARP_EMPTY)
	{
	  /* existing address, was negative. */
	  arp->status = ARP_NEW;
	  arp->hwlen = maclen;
	  memcpy(arp->hwaddr, mac, maclen);
	}
      else if (arp->hwlen == maclen && memcmp(arp->hwaddr, mac, maclen) == 0)
	/* Existing entry matches - confirm. */
	arp->status = ARP_FOUND;
      else
	continue;
      
      break;
    }

  if (!arp)
    {
      /* New entry */
      if (freelist)
	{
	  arp = freelist;
	  freelist = freelist->next;
	}
      else if (!(arp = whine_malloc(sizeof(struct arp_record))))
	return 1;
      
      arp->next = arps;
      arps = arp;
      arp->status = ARP_NEW;
      arp->hwlen = maclen;
      arp->family = family;
      memcpy(arp->hwaddr, mac, maclen);
      if (family == AF_INET)
	arp->addr.addr.addr4.s_addr = ((struct in_addr *)addrp)->s_addr;
#ifdef HAVE_IPV6
      else
	memcpy(&arp->addr.addr.addr6, addrp, IN6ADDRSZ);
#endif
    }
  
  return 1;
}

/* If in lazy mode, we cache absence of ARP entries. */
int find_mac(union mysockaddr *addr, unsigned char *mac, int lazy, time_t now)
{
  struct arp_record *arp, *tmp, **up;
  int updated = 0;

 again:
  
  /* If the database is less then INTERVAL old, look in there */
  if (difftime(now, last) < INTERVAL)
    {
      /* addr == NULL -> just make cache up-to-date */
      if (!addr)
	return 0;

      for (arp = arps; arp; arp = arp->next)
	{
	  if (addr->sa.sa_family != arp->family)
	    continue;
	    
	  if (arp->family == AF_INET &&
	      arp->addr.addr.addr4.s_addr != addr->in.sin_addr.s_addr)
	    continue;
	    
#ifdef HAVE_IPV6
	  if (arp->family == AF_INET6 && 
	      !IN6_ARE_ADDR_EQUAL(&arp->addr.addr.addr6, &addr->in6.sin6_addr))
	    continue;
#endif
	  
	  /* Only accept positive entries unless in lazy mode. */
	  if (arp->status != ARP_EMPTY || lazy || updated)
	    {
	      if (mac && arp->hwlen != 0)
		memcpy(mac, arp->hwaddr, arp->hwlen);
	      return arp->hwlen;
	    }
	}
    }

  /* Not found, try the kernel */
  if (!updated)
     {
       updated = 1;
       last = now;

       /* Mark all non-negative entries */
       for (arp = arps; arp; arp = arp->next)
	 if (arp->status != ARP_EMPTY)
	   arp->status = ARP_MARK;
       
       iface_enumerate(AF_UNSPEC, NULL, filter_mac);
       
       /* Remove all unconfirmed entries to old list. */
       for (arp = arps, up = &arps; arp; arp = tmp)
	 {
	   tmp = arp->next;
	   
	   if (arp->status == ARP_MARK)
	     {
	       *up = arp->next;
	       arp->next = old;
	       old = arp;
	     }
	   else
	     up = &arp->next;
	 }

       goto again;
     }

  /* record failure, so we don't consult the kernel each time
     we're asked for this address */
  if (freelist)
    {
      arp = freelist;
      freelist = freelist->next;
    }
  else
    arp = whine_malloc(sizeof(struct arp_record));
  
  if (arp)
    {      
      arp->next = arps;
      arps = arp;
      arp->status = ARP_EMPTY;
      arp->family = addr->sa.sa_family;
      arp->hwlen = 0;

      if (addr->sa.sa_family == AF_INET)
	arp->addr.addr.addr4.s_addr = addr->in.sin_addr.s_addr;
#ifdef HAVE_IPV6
      else
	memcpy(&arp->addr.addr.addr6, &addr->in6.sin6_addr, IN6ADDRSZ);
#endif
    }
	  
   return 0;
}

int do_arp_script_run(void)
{
  struct arp_record *arp;
  
  /* Notify any which went, then move to free list */
  if (old)
    {
#ifdef HAVE_SCRIPT
      if (option_bool(OPT_SCRIPT_ARP))
	queue_arp(ACTION_ARP_DEL, old->hwaddr, old->hwlen, old->family, &old->addr);
#endif
      arp = old;
      old = arp->next;
      arp->next = freelist;
      freelist = arp;
      return 1;
    }

  for (arp = arps; arp; arp = arp->next)
    if (arp->status == ARP_NEW)
      {
#ifdef HAVE_SCRIPT
	if (option_bool(OPT_SCRIPT_ARP))
	  queue_arp(ACTION_ARP, arp->hwaddr, arp->hwlen, arp->family, &arp->addr);
#endif
	arp->status = ARP_FOUND;
	return 1;
      }

  return 0;
}


