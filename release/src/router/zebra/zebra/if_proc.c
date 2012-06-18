/* Interface name and statistics get function using proc file system
 * Copyright (C) 1999 Kunihiro Ishiguro
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
#include "log.h"

#include "zebra/ioctl.h"
#include "zebra/connected.h"
#include "zebra/interface.h"

/* Proc filesystem one line buffer. */
#define PROCBUFSIZ                  1024

/* Path to device proc file system. */
#ifndef _PATH_PROC_NET_DEV
#define _PATH_PROC_NET_DEV        "/proc/net/dev"
#endif /* _PATH_PROC_NET_DEV */

/* Return statistics data pointer. */
static char *
interface_name_cut (char *buf, char **name)
{
  char *stat;

  /* Skip white space.  Line will include header spaces. */
  while (*buf == ' ')
    buf++;
  *name = buf;

  /* Cut interface name. */
  stat = strrchr (buf, ':');
  *stat++ = '\0';

  return stat;
}

/* Fetch each statistics field. */
static int
ifstat_dev_fields (int version, char *buf, struct interface *ifp)
{
  switch (version) 
    {
    case 3:
      sscanf(buf,
	     "%ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld",
	     &ifp->stats.rx_bytes,
	     &ifp->stats.rx_packets,
	     &ifp->stats.rx_errors,
	     &ifp->stats.rx_dropped,
	     &ifp->stats.rx_fifo_errors,
	     &ifp->stats.rx_frame_errors,
	     &ifp->stats.rx_compressed,
	     &ifp->stats.rx_multicast,

	     &ifp->stats.tx_bytes,
	     &ifp->stats.tx_packets,
	     &ifp->stats.tx_errors,
	     &ifp->stats.tx_dropped,
	     &ifp->stats.tx_fifo_errors,
	     &ifp->stats.collisions,
	     &ifp->stats.tx_carrier_errors,
	     &ifp->stats.tx_compressed);
      break;
    case 2:
      sscanf(buf, "%ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld",
	     &ifp->stats.rx_bytes,
	     &ifp->stats.rx_packets,
	     &ifp->stats.rx_errors,
	     &ifp->stats.rx_dropped,
	     &ifp->stats.rx_fifo_errors,
	     &ifp->stats.rx_frame_errors,

	     &ifp->stats.tx_bytes,
	     &ifp->stats.tx_packets,
	     &ifp->stats.tx_errors,
	     &ifp->stats.tx_dropped,
	     &ifp->stats.tx_fifo_errors,
	     &ifp->stats.collisions,
	     &ifp->stats.tx_carrier_errors);
      ifp->stats.rx_multicast = 0;
      break;
    case 1:
      sscanf(buf, "%ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld",
	     &ifp->stats.rx_packets,
	     &ifp->stats.rx_errors,
	     &ifp->stats.rx_dropped,
	     &ifp->stats.rx_fifo_errors,
	     &ifp->stats.rx_frame_errors,

	     &ifp->stats.tx_packets,
	     &ifp->stats.tx_errors,
	     &ifp->stats.tx_dropped,
	     &ifp->stats.tx_fifo_errors,
	     &ifp->stats.collisions,
	     &ifp->stats.tx_carrier_errors);
      ifp->stats.rx_bytes = 0;
      ifp->stats.tx_bytes = 0;
      ifp->stats.rx_multicast = 0;
      break;
    }
  return 0;
}

/* Update interface's statistics. */
int
ifstat_update_proc ()
{
  FILE *fp;
  char buf[PROCBUFSIZ];
  int version;
  struct interface *ifp;
  char *stat;
  char *name;

  /* Open /proc/net/dev. */
  fp = fopen (_PATH_PROC_NET_DEV, "r");
  if (fp == NULL)
    {
      zlog_warn ("Can't open proc file %s: %s",
		 _PATH_PROC_NET_DEV, strerror (errno));
      return -1;
    }

  /* Drop header lines. */
  fgets (buf, PROCBUFSIZ, fp);
  fgets (buf, PROCBUFSIZ, fp);

  /* To detect proc format veresion, parse second line. */
  if (strstr (buf, "compressed"))
    version = 3;
  else if (strstr (buf, "bytes"))
    version = 2;
  else
    version = 1;

  /* Update each interface's statistics. */
  while (fgets (buf, PROCBUFSIZ, fp) != NULL)
    {
      stat = interface_name_cut (buf, &name);
      ifp = if_get_by_name (name);
      ifstat_dev_fields (version, stat, ifp);
    }
  fclose(fp);
  return 0;
}

/* Interface structure allocation by proc filesystem. */
int
interface_list_proc ()
{
  FILE *fp;
  char buf[PROCBUFSIZ];
  struct interface *ifp;
  char *name;

  /* Open /proc/net/dev. */
  fp = fopen (_PATH_PROC_NET_DEV, "r");
  if (fp == NULL)
    {
      zlog_warn ("Can't open proc file %s: %s",
		 _PATH_PROC_NET_DEV, strerror (errno));
      return -1;
    }

  /* Drop header lines. */
  fgets (buf, PROCBUFSIZ, fp);
  fgets (buf, PROCBUFSIZ, fp);

  /* Only allocate interface structure.  Other jobs will be done in
     if_ioctl.c. */
  while (fgets (buf, PROCBUFSIZ, fp) != NULL)
    {
      interface_name_cut (buf, &name);
      ifp = if_get_by_name (name);
      if_add_update (ifp);
    }
  fclose(fp);
  return 0;
}

#if defined(HAVE_IPV6) && defined(HAVE_PROC_NET_IF_INET6)

#ifndef _PATH_PROC_NET_IF_INET6
#define _PATH_PROC_NET_IF_INET6          "/proc/net/if_inet6"
#endif /* _PATH_PROC_NET_IF_INET6 */

int
ifaddr_proc_ipv6 ()
{
  FILE *fp;
  char buf[PROCBUFSIZ];
  int n;
  char addr[33];
  char ifname[20];
  int ifindex, plen, scope, status;
  struct interface *ifp;
  struct prefix_ipv6 p;

  /* Open proc file system. */
  fp = fopen (_PATH_PROC_NET_IF_INET6, "r");
  if (fp == NULL)
    {
      zlog_warn ("Can't open proc file %s: %s",
		 _PATH_PROC_NET_IF_INET6, strerror (errno));
      return -1;
    }
  
  /* Get interface's IPv6 address. */
  while (fgets (buf, PROCBUFSIZ, fp) != NULL)
    {
      n = sscanf (buf, "%32s %02x %02x %02x %02x %20s", 
		  addr, &ifindex, &plen, &scope, &status, ifname);
      if (n != 6)
	continue;

      ifp = if_get_by_name (ifname);

      /* Fetch interface's IPv6 address. */
      str2in6_addr (addr, &p.prefix);
      p.prefixlen = plen;

      connected_add_ipv6 (ifp, &p.prefix, p.prefixlen, NULL);
    }
  return 0;
}
#endif /* HAVE_IPV6 && HAVE_PROC_NET_IF_INET6 */
