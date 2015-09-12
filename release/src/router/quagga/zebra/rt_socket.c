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
#include "privs.h"

#include "zebra/debug.h"
#include "zebra/rib.h"
#include "zebra/rt.h"
#include "zebra/kernel_socket.h"

extern struct zebra_privs_t zserv_privs;

/* kernel socket export */
extern int rtm_write (int message, union sockunion *dest,
                      union sockunion *mask, union sockunion *gate,
                      unsigned int index, int zebra_flags, int metric);

/* Adjust netmask socket length. Return value is a adjusted sin_len
   value. */
static int
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
static int
kernel_rtm_ipv4 (int cmd, struct prefix *p, struct rib *rib, int family)

{
  struct sockaddr_in *mask = NULL;
  struct sockaddr_in sin_dest, sin_mask, sin_gate;
  struct nexthop *nexthop, *tnexthop;
  int recursing;
  int nexthop_num = 0;
  unsigned int ifindex = 0;
  int gate = 0;
  int error;
  char prefix_buf[INET_ADDRSTRLEN];

  if (IS_ZEBRA_DEBUG_RIB)
    inet_ntop (AF_INET, &p->u.prefix, prefix_buf, INET_ADDRSTRLEN);
  memset (&sin_dest, 0, sizeof (struct sockaddr_in));
  sin_dest.sin_family = AF_INET;
#ifdef HAVE_STRUCT_SOCKADDR_IN_SIN_LEN
  sin_dest.sin_len = sizeof (struct sockaddr_in);
#endif /* HAVE_STRUCT_SOCKADDR_IN_SIN_LEN */
  sin_dest.sin_addr = p->u.prefix4;

  memset (&sin_mask, 0, sizeof (struct sockaddr_in));

  memset (&sin_gate, 0, sizeof (struct sockaddr_in));
  sin_gate.sin_family = AF_INET;
#ifdef HAVE_STRUCT_SOCKADDR_IN_SIN_LEN
  sin_gate.sin_len = sizeof (struct sockaddr_in);
#endif /* HAVE_STRUCT_SOCKADDR_IN_SIN_LEN */

  /* Make gateway. */
  for (ALL_NEXTHOPS_RO(rib->nexthop, nexthop, tnexthop, recursing))
    {
      if (CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_RECURSIVE))
        continue;

      gate = 0;
      char gate_buf[INET_ADDRSTRLEN] = "NULL";

      /*
       * XXX We need to refrain from kernel operations in some cases,
       * but this if statement seems overly cautious - what about
       * other than ADD and DELETE?
       */
      if ((cmd == RTM_ADD
	   && CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_ACTIVE))
	  || (cmd == RTM_DELETE
	      && CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_FIB)
	      ))
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

	  if (gate && p->prefixlen == 32)
	    mask = NULL;
	  else
	    {
	      masklen2ip (p->prefixlen, &sin_mask.sin_addr);
	      sin_mask.sin_family = AF_INET;
#ifdef HAVE_STRUCT_SOCKADDR_IN_SIN_LEN
	      sin_mask.sin_len = sin_masklen (sin_mask.sin_addr);
#endif /* HAVE_STRUCT_SOCKADDR_IN_SIN_LEN */
	      mask = &sin_mask;
	    }

	  error = rtm_write (cmd,
			     (union sockunion *)&sin_dest, 
			     (union sockunion *)mask, 
			     gate ? (union sockunion *)&sin_gate : NULL,
			     ifindex,
			     rib->flags,
			     rib->metric);

           if (IS_ZEBRA_DEBUG_RIB)
           {
             if (!gate)
             {
               zlog_debug ("%s: %s/%d: attention! gate not found for rib %p",
                 __func__, prefix_buf, p->prefixlen, rib);
               rib_dump (p, rib);
             }
             else
               inet_ntop (AF_INET, &sin_gate.sin_addr, gate_buf, INET_ADDRSTRLEN);
           }
 
           switch (error)
           {
             /* We only flag nexthops as being in FIB if rtm_write() did its work. */
             case ZEBRA_ERR_NOERROR:
               nexthop_num++;
               if (IS_ZEBRA_DEBUG_RIB)
                 zlog_debug ("%s: %s/%d: successfully did NH %s",
                   __func__, prefix_buf, p->prefixlen, gate_buf);
               if (cmd == RTM_ADD)
                 SET_FLAG (nexthop->flags, NEXTHOP_FLAG_FIB);
               break;
 
             /* The only valid case for this error is kernel's failure to install
              * a multipath route, which is common for FreeBSD. This should be
              * ignored silently, but logged as an error otherwise.
              */
             case ZEBRA_ERR_RTEXIST:
               if (cmd != RTM_ADD)
                 zlog_err ("%s: rtm_write() returned %d for command %d",
                   __func__, error, cmd);
               continue;
               break;
 
             /* Given that our NEXTHOP_FLAG_FIB matches real kernel FIB, it isn't
              * normal to get any other messages in ANY case.
              */
             case ZEBRA_ERR_RTNOEXIST:
             case ZEBRA_ERR_RTUNREACH:
             default:
               /* This point is reachable regardless of debugging mode. */
               if (!IS_ZEBRA_DEBUG_RIB)
                 inet_ntop (AF_INET, &p->u.prefix, prefix_buf, INET_ADDRSTRLEN);
               zlog_err ("%s: %s/%d: rtm_write() unexpectedly returned %d for command %s",
                 __func__, prefix_buf, p->prefixlen, error, lookup (rtm_type_str, cmd));
               break;
           }
         } /* if (cmd and flags make sense) */
       else
         if (IS_ZEBRA_DEBUG_RIB)
           zlog_debug ("%s: odd command %s for flags %d",
             __func__, lookup (rtm_type_str, cmd), nexthop->flags);
     } /* for (ALL_NEXTHOPS_RO(...))*/
 
   /* If there was no useful nexthop, then complain. */
   if (nexthop_num == 0 && IS_ZEBRA_DEBUG_KERNEL)
     zlog_debug ("%s: No useful nexthops were found in RIB entry %p", __func__, rib);

  return 0; /*XXX*/
}

int
kernel_add_ipv4 (struct prefix *p, struct rib *rib)
{
  int route;

  if (zserv_privs.change(ZPRIVS_RAISE))
    zlog (NULL, LOG_ERR, "Can't raise privileges");
  route = kernel_rtm_ipv4 (RTM_ADD, p, rib, AF_INET);
  if (zserv_privs.change(ZPRIVS_LOWER))
    zlog (NULL, LOG_ERR, "Can't lower privileges");

  return route;
}

int
kernel_delete_ipv4 (struct prefix *p, struct rib *rib)
{
  int route;

  if (zserv_privs.change(ZPRIVS_RAISE))
    zlog (NULL, LOG_ERR, "Can't raise privileges");
  route = kernel_rtm_ipv4 (RTM_DELETE, p, rib, AF_INET);
  if (zserv_privs.change(ZPRIVS_LOWER))
    zlog (NULL, LOG_ERR, "Can't lower privileges");

  return route;
}

#ifdef HAVE_IPV6

/* Calculate sin6_len value for netmask socket value. */
static int
sin6_masklen (struct in6_addr mask)
{
  struct sockaddr_in6 sin6;
  char *p, *lim;
  int len;

  if (IN6_IS_ADDR_UNSPECIFIED (&mask)) 
    return sizeof (long);

  sin6.sin6_addr = mask;
  len = sizeof (struct sockaddr_in6);

  lim = (char *) & sin6.sin6_addr;
  p = lim + sizeof (sin6.sin6_addr);

  while (*--p == 0 && p >= lim) 
    len--;

  return len;
}

/* Interface between zebra message and rtm message. */
static int
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
      sin_mask.sin6_family = AF_INET6;
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
static int
kernel_rtm_ipv6_multipath (int cmd, struct prefix *p, struct rib *rib,
			   int family)
{
  struct sockaddr_in6 *mask;
  struct sockaddr_in6 sin_dest, sin_mask, sin_gate;
  struct nexthop *nexthop, *tnexthop;
  int recursing;
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
#ifdef HAVE_STRUCT_SOCKADDR_IN_SIN_LEN
  sin_gate.sin6_len = sizeof (struct sockaddr_in6);
#endif /* HAVE_STRUCT_SOCKADDR_IN_SIN_LEN */

  /* Make gateway. */
  for (ALL_NEXTHOPS_RO(rib->nexthop, nexthop, tnexthop, recursing))
    {
      if (CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_RECURSIVE))
	continue;

      gate = 0;

      if ((cmd == RTM_ADD
	   && CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_ACTIVE))
	  || (cmd == RTM_DELETE
#if 0
	      && CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_FIB)
#endif
	      ))
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
	  sin_mask.sin6_family = AF_INET6;
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
	zlog_debug ("kernel_rtm_ipv6_multipath(): No useful nexthop.");
      return 0;
    }

  return 0; /*XXX*/
}

int
kernel_add_ipv6 (struct prefix *p, struct rib *rib)
{
  int route;

  if (zserv_privs.change(ZPRIVS_RAISE))
    zlog (NULL, LOG_ERR, "Can't raise privileges");
  route =  kernel_rtm_ipv6_multipath (RTM_ADD, p, rib, AF_INET6);
  if (zserv_privs.change(ZPRIVS_LOWER))
    zlog (NULL, LOG_ERR, "Can't lower privileges");

  return route;
}

int
kernel_delete_ipv6 (struct prefix *p, struct rib *rib)
{
  int route;

  if (zserv_privs.change(ZPRIVS_RAISE))
    zlog (NULL, LOG_ERR, "Can't raise privileges");
  route =  kernel_rtm_ipv6_multipath (RTM_DELETE, p, rib, AF_INET6);
  if (zserv_privs.change(ZPRIVS_LOWER))
    zlog (NULL, LOG_ERR, "Can't lower privileges");

  return route;
}
#endif /* HAVE_IPV6 */
