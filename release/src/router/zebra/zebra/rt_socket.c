/*
 * Kernel routing table updates by routing socket.
 * Copyright (C) 1997, 98 Kunihiro Ishiguro
 *
 * This file is part of GNU Zebra.
 *
 * GNU Zebra is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * GNU Zebra is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Zebra; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  
 */

#include <zebra.h>

#include "if.h"
#include "prefix.h"
#include "sockunion.h"
#include "log.h"
#include "str.h"

#include "zebra/debug.h"
#include "zebra/rib.h"

int
rtm_write (int message,
	   union sockunion *dest,
	   union sockunion *mask,
	   union sockunion *gate,
	   unsigned int index,
	   int zebra_flags,
	   int metric);

/* Adjust netmask socket length. Return value is a adjusted sin_len
   value. */
int
sin_masklen (struct in_addr mask)
{
  char *p, *lim;
  int len;
  struct sockaddr_in sin;

  if (mask.s_addr == 0) 
    return sizeof (long);

  sin.sin_addr = mask;
  len = sizeof (struct sockaddr_in);

  lim = (char *) &sin.sin_addr;
  p = lim + sizeof (sin.sin_addr);

  while (*--p == 0 && p >= lim) 
    len--;
  return len;
}

/* Interface between zebra message and rtm message. */
int
kernel_rtm_ipv4 (int cmd, struct prefix *p, struct rib *rib, int family)
{
  struct sockaddr_in *mask = NULL;
  struct sockaddr_in sin_dest, sin_mask, sin_gate;
  struct nexthop *nexthop;
  int nexthop_num = 0;
  unsigned int ifindex = 0;
  int gate = 0;
  int error;

  memset (&sin_dest, 0, sizeof (struct sockaddr_in));
  sin_dest.sin_family = AF_INET;
#ifdef HAVE_SIN_LEN
  sin_dest.sin_len = sizeof (struct sockaddr_in);
#endif /* HAVE_SIN_LEN */
  sin_dest.sin_addr = p->u.prefix4;

  memset (&sin_mask, 0, sizeof (struct sockaddr_in));

  memset (&sin_gate, 0, sizeof (struct sockaddr_in));
  sin_gate.sin_family = AF_INET;
#ifdef HAVE_SIN_LEN
  sin_gate.sin_len = sizeof (struct sockaddr_in);
#endif /* HAVE_SIN_LEN */

  /* Make gateway. */
  for (nexthop = rib->nexthop; nexthop; nexthop = nexthop->next)
    {
      gate = 0;

      if ((cmd == RTM_ADD
	   && CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_ACTIVE))
	  || (cmd == RTM_DELETE
#if 0
	      && CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_FIB)
#endif
	      ))
	{
	  if (CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_RECURSIVE))
	    {
	      if (nexthop->rtype == NEXTHOP_TYPE_IPV4 ||
		  nexthop->rtype == NEXTHOP_TYPE_IPV4_IFINDEX)
		{
		  sin_gate.sin_addr = nexthop->rgate.ipv4;
		  gate = 1;
		}
	      if (nexthop->rtype == NEXTHOP_TYPE_IFINDEX
		  || nexthop->rtype == NEXTHOP_TYPE_IFNAME
		  || nexthop->rtype == NEXTHOP_TYPE_IPV4_IFINDEX)
		ifindex = nexthop->rifindex;
	    }
	  else
	    {
	      if (nexthop->type == NEXTHOP_TYPE_IPV4 ||
		  nexthop->type == NEXTHOP_TYPE_IPV4_IFINDEX)
		{
		  sin_gate.sin_addr = nexthop->gate.ipv4;
		  gate = 1;
		}
	      if (nexthop->type == NEXTHOP_TYPE_IFINDEX
		  || nexthop->type == NEXTHOP_TYPE_IFNAME
		  || nexthop->type == NEXTHOP_TYPE_IPV4_IFINDEX)
		ifindex = nexthop->ifindex;
	      if (nexthop->type == NEXTHOP_TYPE_BLACKHOLE)
		{
		  struct in_addr loopback;

		  loopback.s_addr = htonl (INADDR_LOOPBACK);
		  sin_gate.sin_addr = loopback;
		  gate = 1;
		}
	    }

	  if (cmd == RTM_ADD)
	    SET_FLAG (nexthop->flags, NEXTHOP_FLAG_FIB);

	  if (gate && p->prefixlen == 32)
	    mask = NULL;
	  else
	    {
	      masklen2ip (p->prefixlen, &sin_mask.sin_addr);
	      sin_mask.sin_family = AF_UNSPEC;
#ifdef HAVE_SIN_LEN
	      sin_mask.sin_len = sin_masklen (sin_mask.sin_addr);
#endif /* HAVE_SIN_LEN */
	      mask = &sin_mask;
	    }
	}

      error = rtm_write (cmd,
			(union sockunion *)&sin_dest, 
			(union sockunion *)mask, 
			gate ? (union sockunion *)&sin_gate : NULL,
			ifindex,
			rib->flags,
			rib->metric);

#if 0
      if (error)
	{
	  zlog_info ("kernel_rtm_ipv4(): nexthop %d add error=%d.",
	    nexthop_num, error);
	}
#endif

      nexthop_num++;
    }

  /* If there is no useful nexthop then return. */
  if (nexthop_num == 0)
    {
      if (IS_ZEBRA_DEBUG_KERNEL)
	zlog_info ("kernel_rtm_ipv4(): No useful nexthop.");
      return 0;
    }

  return 0; /*XXX*/
}

int
kernel_add_ipv4 (struct prefix *p, struct rib *rib)
{
  return kernel_rtm_ipv4 (RTM_ADD, p, rib, AF_INET);
}

int
kernel_delete_ipv4 (struct prefix *p, struct rib *rib)
{
  return kernel_rtm_ipv4 (RTM_DELETE, p, rib, AF_INET);
}

#ifdef HAVE_IPV6

/* Calculate sin6_len value for netmask socket value. */
int
sin6_masklen (struct in6_addr mask)
{
  struct sockaddr_in6 sin6;
  char *p, *lim;
  int len;

#if defined (INRIA)
  if (IN_ANYADDR6 (mask)) 
    return sizeof (long);
#else /* ! INRIA */
  if (IN6_IS_ADDR_UNSPECIFIED (&mask)) 
    return sizeof (long);
#endif /* ! INRIA */

  sin6.sin6_addr = mask;
  len = sizeof (struct sockaddr_in6);

  lim = (char *) & sin6.sin6_addr;
  p = lim + sizeof (sin6.sin6_addr);

  while (*--p == 0 && p >= lim) 
    len--;

  return len;
}

/* Interface between zebra message and rtm message. */
int
kernel_rtm_ipv6 (int message, struct prefix_ipv6 *dest,
		 struct in6_addr *gate, int index, int flags)
{
  struct sockaddr_in6 *mask;
  struct sockaddr_in6 sin_dest, sin_mask, sin_gate;

  memset (&sin_dest, 0, sizeof (struct sockaddr_in6));
  sin_dest.sin6_family = AF_INET6;
#ifdef SIN6_LEN
  sin_dest.sin6_len = sizeof (struct sockaddr_in6);
#endif /* SIN6_LEN */

  memset (&sin_mask, 0, sizeof (struct sockaddr_in6));

  memset (&sin_gate, 0, sizeof (struct sockaddr_in6));
  sin_gate.sin6_family = AF_INET6;
#ifdef SIN6_LEN
  sin_gate.sin6_len = sizeof (struct sockaddr_in6);
#endif /* SIN6_LEN */

  sin_dest.sin6_addr = dest->prefix;

  if (gate)
    memcpy (&sin_gate.sin6_addr, gate, sizeof (struct in6_addr));

  /* Under kame set interface index to link local address. */
#ifdef KAME

#define SET_IN6_LINKLOCAL_IFINDEX(a, i) \
  do { \
    (a).s6_addr[2] = ((i) >> 8) & 0xff; \
    (a).s6_addr[3] = (i) & 0xff; \
  } while (0)

  if (gate && IN6_IS_ADDR_LINKLOCAL(gate))
    SET_IN6_LINKLOCAL_IFINDEX (sin_gate.sin6_addr, index);
#endif /* KAME */

  if (gate && dest->prefixlen == 128)
    mask = NULL;
  else
    {
      masklen2ip6 (dest->prefixlen, &sin_mask.sin6_addr);
      sin_mask.sin6_family = AF_UNSPEC;
#ifdef SIN6_LEN
      sin_mask.sin6_len = sin6_masklen (sin_mask.sin6_addr);
#endif /* SIN6_LEN */
      mask = &sin_mask;
    }

  return rtm_write (message, 
		    (union sockunion *) &sin_dest,
		    (union sockunion *) mask,
		    gate ? (union sockunion *)&sin_gate : NULL,
		    index,
		    flags,
		    0);
}

/* Interface between zebra message and rtm message. */
int
kernel_rtm_ipv6_multipath (int cmd, struct prefix *p, struct rib *rib,
			   int family)
{
  struct sockaddr_in6 *mask;
  struct sockaddr_in6 sin_dest, sin_mask, sin_gate;
  struct nexthop *nexthop;
  int nexthop_num = 0;
  unsigned int ifindex = 0;
  int gate = 0;
  int error;

  memset (&sin_dest, 0, sizeof (struct sockaddr_in6));
  sin_dest.sin6_family = AF_INET6;
#ifdef SIN6_LEN
  sin_dest.sin6_len = sizeof (struct sockaddr_in6);
#endif /* SIN6_LEN */
  sin_dest.sin6_addr = p->u.prefix6;

  memset (&sin_mask, 0, sizeof (struct sockaddr_in6));

  memset (&sin_gate, 0, sizeof (struct sockaddr_in6));
  sin_gate.sin6_family = AF_INET6;
#ifdef HAVE_SIN_LEN
  sin_gate.sin6_len = sizeof (struct sockaddr_in6);
#endif /* HAVE_SIN_LEN */

  /* Make gateway. */
  for (nexthop = rib->nexthop; nexthop; nexthop = nexthop->next)
    {
      gate = 0;

      if ((cmd == RTM_ADD
	   && CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_ACTIVE))
	  || (cmd == RTM_DELETE
#if 0
	      && CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_FIB)
#endif
	      ))
	{
	  if (CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_RECURSIVE))
	    {
	      if (nexthop->rtype == NEXTHOP_TYPE_IPV6
		  || nexthop->rtype == NEXTHOP_TYPE_IPV6_IFNAME
		  || nexthop->rtype == NEXTHOP_TYPE_IPV6_IFINDEX)
		{
		  sin_gate.sin6_addr = nexthop->rgate.ipv6;
		  gate = 1;
		}
	      if (nexthop->rtype == NEXTHOP_TYPE_IFINDEX
		  || nexthop->rtype == NEXTHOP_TYPE_IFNAME
		  || nexthop->rtype == NEXTHOP_TYPE_IPV6_IFNAME
		  || nexthop->rtype == NEXTHOP_TYPE_IPV6_IFINDEX)
		ifindex = nexthop->rifindex;
	    }
	  else
	    {
	      if (nexthop->type == NEXTHOP_TYPE_IPV6
		  || nexthop->type == NEXTHOP_TYPE_IPV6_IFNAME
		  || nexthop->type == NEXTHOP_TYPE_IPV6_IFINDEX)
		{
		  sin_gate.sin6_addr = nexthop->gate.ipv6;
		  gate = 1;
		}
	      if (nexthop->type == NEXTHOP_TYPE_IFINDEX
		  || nexthop->type == NEXTHOP_TYPE_IFNAME
		  || nexthop->type == NEXTHOP_TYPE_IPV6_IFNAME
		  || nexthop->type == NEXTHOP_TYPE_IPV6_IFINDEX)
		ifindex = nexthop->ifindex;
	      if (nexthop->type == NEXTHOP_TYPE_BLACKHOLE)
		{
#ifdef HAVE_IN6ADDR_GLOBAL
		  sin_gate.sin6_addr = in6addr_loopback;
#else /*HAVE_IN6ADDR_GLOBAL*/
		  inet_pton (AF_INET6, "::1", &sin_gate.sin6_addr);
#endif /*HAVE_IN6ADDR_GLOBAL*/
		  gate = 1;
		}
	    }

	  if (cmd == RTM_ADD)
	    SET_FLAG (nexthop->flags, NEXTHOP_FLAG_FIB);
	}

      /* Under kame set interface index to link local address. */
#ifdef KAME

#define SET_IN6_LINKLOCAL_IFINDEX(a, i) \
      do { \
	(a).s6_addr[2] = ((i) >> 8) & 0xff; \
	(a).s6_addr[3] = (i) & 0xff; \
      } while (0)

      if (gate && IN6_IS_ADDR_LINKLOCAL(&sin_gate.sin6_addr))
	SET_IN6_LINKLOCAL_IFINDEX (sin_gate.sin6_addr, ifindex);
#endif /* KAME */

      if (gate && p->prefixlen == 128)
	mask = NULL;
      else
	{
	  masklen2ip6 (p->prefixlen, &sin_mask.sin6_addr);
	  sin_mask.sin6_family = AF_UNSPEC;
#ifdef SIN6_LEN
	  sin_mask.sin6_len = sin6_masklen (sin_mask.sin6_addr);
#endif /* SIN6_LEN */
	  mask = &sin_mask;
	}

      error = rtm_write (cmd,
			(union sockunion *) &sin_dest,
			(union sockunion *) mask,
			gate ? (union sockunion *)&sin_gate : NULL,
			ifindex,
			rib->flags,
			rib->metric);

#if 0
      if (error)
	{
	  zlog_info ("kernel_rtm_ipv6_multipath(): nexthop %d add error=%d.",
	    nexthop_num, error);
	}
#endif

      nexthop_num++;
    }

  /* If there is no useful nexthop then return. */
  if (nexthop_num == 0)
    {
      if (IS_ZEBRA_DEBUG_KERNEL)
	zlog_info ("kernel_rtm_ipv6_multipath(): No useful nexthop.");
      return 0;
    }

  return 0; /*XXX*/
}

int
kernel_add_ipv6 (struct prefix *p, struct rib *rib)
{
  return kernel_rtm_ipv6_multipath (RTM_ADD, p, rib, AF_INET6);
}

int
kernel_delete_ipv6 (struct prefix *p, struct rib *rib)
{
  return kernel_rtm_ipv6_multipath (RTM_DELETE, p, rib, AF_INET6);
}

/* Delete IPv6 route from the kernel. */
int
kernel_delete_ipv6_old (struct prefix_ipv6 *dest, struct in6_addr *gate,
		    int index, int flags, int table)
{
  return kernel_rtm_ipv6 (RTM_DELETE, dest, gate, index, flags);
}
#endif /* HAVE_IPV6 */
