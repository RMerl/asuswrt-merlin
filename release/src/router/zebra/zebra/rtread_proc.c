/*
 * Kernel routing readup by /proc filesystem
 * Copyright (C) 1997 Kunihiro Ishiguro
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

#include "prefix.h"
#include "log.h"
#include "if.h"
#include "rib.h"

/* Proc file system to read IPv4 routing table. */
#ifndef _PATH_PROCNET_ROUTE
#define _PATH_PROCNET_ROUTE      "/proc/net/route"
#endif /* _PATH_PROCNET_ROUTE */

/* Proc file system to read IPv6 routing table. */
#ifndef _PATH_PROCNET_ROUTE6
#define _PATH_PROCNET_ROUTE6     "/proc/net/ipv6_route"
#endif /* _PATH_PROCNET_ROUTE6 */

/* To read interface's name */
#define INTERFACE_NAMSIZ 20  

/* Reading buffer for one routing entry. */
#define RT_BUFSIZ 1024

/* Kernel routing table read up by /proc filesystem. */
int
proc_route_read ()
{
  FILE *fp;
  char buf[RT_BUFSIZ];
  char iface[INTERFACE_NAMSIZ], dest[9], gate[9], mask[9];
  int flags, refcnt, use, metric, mtu, window, rtt;

  /* Open /proc filesystem */
  fp = fopen (_PATH_PROCNET_ROUTE, "r");
  if (fp == NULL)
    {
      zlog_warn ("Can't open %s : %s\n", _PATH_PROCNET_ROUTE, strerror (errno));
      return -1;
    }
  
  /* Drop first label line. */
  fgets (buf, RT_BUFSIZ, fp);

  while (fgets (buf, RT_BUFSIZ, fp) != NULL)
    {
      int n;
      struct prefix_ipv4 p;
      struct in_addr tmpmask;
      struct in_addr gateway;
      u_char zebra_flags = 0;

      n = sscanf (buf, "%s %s %s %x %d %d %d %s %d %d %d",
		  iface, dest, gate, &flags, &refcnt, &use, &metric, 
		  mask, &mtu, &window, &rtt);
      if (n != 11)
	{	
	  zlog_warn ("can't read all of routing information\n");
	  continue;
	}
      if (! (flags & RTF_UP))
	continue;
      if (! (flags & RTF_GATEWAY))
	continue;

      if (flags & RTF_DYNAMIC)
	zebra_flags |= ZEBRA_FLAG_SELFROUTE;

      p.family = AF_INET;
      sscanf (dest, "%lX", (unsigned long *)&p.prefix);
      sscanf (mask, "%lX", (unsigned long *)&tmpmask);
      p.prefixlen = ip_masklen (tmpmask);
      sscanf (gate, "%lX", (unsigned long *)&gateway);

      rib_add_ipv4 (ZEBRA_ROUTE_KERNEL, zebra_flags, &p, &gateway, 0, 0, 0, 0);
    }

  return 0;
}

#ifdef HAVE_IPV6
int
proc_ipv6_route_read ()
{
  FILE *fp;
  char buf [RT_BUFSIZ];

  /* Open /proc filesystem */
  fp = fopen (_PATH_PROCNET_ROUTE6, "r");
  if (fp == NULL)
    {
      zlog_warn ("Can't open %s : %s", _PATH_PROCNET_ROUTE6, 
		strerror (errno));
      return -1;
    }
  
  /* There is no title line, so we don't drop first line.  */
  while (fgets (buf, RT_BUFSIZ, fp) != NULL)
    {
      int n;
      char dest[33], src[33], gate[33];
      char iface[INTERFACE_NAMSIZ];
      int dest_plen, src_plen;
      int metric, use, refcnt, flags;
      struct prefix_ipv6 p;
      struct in6_addr gateway;
      u_char zebra_flags = 0;

      /* Linux 2.1.x write this information at net/ipv6/route.c
         rt6_info_node () */
      n = sscanf (buf, "%32s %02x %32s %02x %32s %08x %08x %08x %08x %s",
		  dest, &dest_plen, src, &src_plen, gate,
		  &metric, &use, &refcnt, &flags, iface);

      if (n != 10)
	{	
	  /* zlog_warn ("can't read all of routing information %d\n%s\n", n, buf); */
	  continue;
	}

      if (! (flags & RTF_UP))
	continue;
      if (! (flags & RTF_GATEWAY))
	continue;

      if (flags & RTF_DYNAMIC)
	zebra_flags |= ZEBRA_FLAG_SELFROUTE;

      p.family = AF_INET6;
      str2in6_addr (dest, &p.prefix);
      str2in6_addr (gate, &gateway);
      p.prefixlen = dest_plen;

      rib_add_ipv6 (ZEBRA_ROUTE_KERNEL, zebra_flags, &p, &gateway, 0, 0);
    }

  return 0;
}
#endif /* HAVE_IPV6 */

void
route_read ()
{
  proc_route_read ();
#ifdef HAVE_IPV6
  proc_ipv6_route_read ();
#endif /* HAVE_IPV6 */
}
