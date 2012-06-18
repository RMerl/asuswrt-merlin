/* IP forward control by sysctl function.
 * Copyright (C) 1997, 1999 Kunihiro Ishiguro
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

#ifdef NRL
#include <netinet6/in6.h>
#endif /* NRL */

#include "log.h"

#define MIB_SIZ 4

/* IPv4 forwarding control MIB. */
int mib[MIB_SIZ] =
{
  CTL_NET,
  PF_INET,
  IPPROTO_IP,
  IPCTL_FORWARDING
};

int
ipforward ()
{
  int len;
  int ipforwarding = 0;

  len = sizeof ipforwarding;
  if (sysctl (mib, MIB_SIZ, &ipforwarding, &len, 0, 0) < 0) 
    {
      zlog_warn ("Can't get ipforwarding value");
      return -1;
    }
  return ipforwarding;
}

int
ipforward_on ()
{
  int len;
  int ipforwarding = 1;

  len = sizeof ipforwarding;
  if (sysctl (mib, MIB_SIZ, NULL, NULL, &ipforwarding, len) < 0) 
    {
      zlog_warn ("Can't set ipforwarding on");
      return -1;
    }
  return ipforwarding;
}

int
ipforward_off ()
{
  int len;
  int ipforwarding = 0;

  len = sizeof ipforwarding;
  if (sysctl (mib, MIB_SIZ, NULL, NULL, &ipforwarding, len) < 0) 
    {
      zlog_warn ("Can't set ipforwarding on");
      return -1;
    }
  return ipforwarding;
}

#ifdef HAVE_IPV6

/* IPv6 forwarding control MIB. */
int mib_ipv6[MIB_SIZ] = 
{
  CTL_NET,
  PF_INET6,
#if defined(KAME) || (defined(__bsdi__) && _BSDI_VERSION >= 199802 ) || defined(NRL)
  IPPROTO_IPV6,
  IPV6CTL_FORWARDING
#else /* NOT KAME */
  IPPROTO_IP,
  IP6CTL_FORWARDING
#endif /* KAME */
}; 

int
ipforward_ipv6 ()
{
  int len;
  int ip6forwarding = 0;

  len = sizeof ip6forwarding;
  if (sysctl (mib_ipv6, MIB_SIZ, &ip6forwarding, &len, 0, 0) < 0) 
    {
      zlog_warn ("can't get ip6forwarding value");
      return -1;
    }
  return ip6forwarding;
}

int
ipforward_ipv6_on ()
{
  int len;
  int ip6forwarding = 1;

  len = sizeof ip6forwarding;
  if (sysctl (mib_ipv6, MIB_SIZ, NULL, NULL, &ip6forwarding, len) < 0) 
    {
      zlog_warn ("can't get ip6forwarding value");
      return -1;
    }
  return ip6forwarding;
}

int
ipforward_ipv6_off ()
{
  int len;
  int ip6forwarding = 0;

  len = sizeof ip6forwarding;
  if (sysctl (mib_ipv6, MIB_SIZ, NULL, NULL, &ip6forwarding, len) < 0) 
    {
      zlog_warn ("can't get ip6forwarding value");
      return -1;
    }
  return ip6forwarding;
}
#endif /* HAVE_IPV6 */
