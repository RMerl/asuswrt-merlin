/*
 * Common ioctl functions.
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

#include "linklist.h"
#include "if.h"
#include "prefix.h"
#include "ioctl.h"
#include "log.h"

#include "zebra/rib.h"
#include "zebra/rt.h"

/* clear and set interface name string */
void
ifreq_set_name (struct ifreq *ifreq, struct interface *ifp)
{
  strncpy (ifreq->ifr_name, ifp->name, IFNAMSIZ);
}

/* call ioctl system call */
int
if_ioctl (u_long request, caddr_t buffer)
{
  int sock;
  int ret = 0;
  int err = 0;

  sock = socket (AF_INET, SOCK_DGRAM, 0);
  if (sock < 0)
    {
      perror ("socket");
      exit (1);
    }

  ret = ioctl (sock, request, buffer);
  if (ret < 0)
    {
      err = errno;
    }
  close (sock);
  
  if (ret < 0) 
    {
      errno = err;
      return ret;
    }
  return 0;
}

#ifdef HAVE_IPV6
int
if_ioctl_ipv6 (u_long request, caddr_t buffer)
{
  int sock;
  int ret = 0;
  int err = 0;

  sock = socket (AF_INET6, SOCK_DGRAM, 0);
  if (sock < 0)
    {
      perror ("socket");
      exit (1);
    }

  ret = ioctl (sock, request, buffer);
  if (ret < 0)
    {
      err = errno;
    }
  close (sock);
  
  if (ret < 0) 
    {
      errno = err;
      return ret;
    }
  return 0;
}
#endif /* HAVE_IPV6 */

/*
 * get interface metric
 *   -- if value is not avaliable set -1
 */
void
if_get_metric (struct interface *ifp)
{
#ifdef SIOCGIFMETRIC
  struct ifreq ifreq;

  ifreq_set_name (&ifreq, ifp);

  if (if_ioctl (SIOCGIFMETRIC, (caddr_t) &ifreq) < 0) 
    return;
  ifp->metric = ifreq.ifr_metric;
  if (ifp->metric == 0)
    ifp->metric = 1;
#else /* SIOCGIFMETRIC */
  ifp->metric = -1;
#endif /* SIOCGIFMETRIC */
}

/* get interface MTU */
void
if_get_mtu (struct interface *ifp)
{
  struct ifreq ifreq;

  ifreq_set_name (&ifreq, ifp);

#if defined(SIOCGIFMTU)
  if (if_ioctl (SIOCGIFMTU, (caddr_t) & ifreq) < 0) 
    {
      zlog_info ("Can't lookup mtu by ioctl(SIOCGIFMTU)");
      ifp->mtu = -1;
      return;
    }

#ifdef SUNOS_5
  ifp->mtu = ifreq.ifr_metric;
#else
  ifp->mtu = ifreq.ifr_mtu;
#endif /* SUNOS_5 */

#else
  zlog (NULL, LOG_INFO, "Can't lookup mtu on this system");
  ifp->mtu = -1;
#endif
}

#ifdef HAVE_NETLINK
/* Interface address setting via netlink interface. */
int
if_set_prefix (struct interface *ifp, struct connected *ifc)
{
  return kernel_address_add_ipv4 (ifp, ifc);
}

/* Interface address is removed using netlink interface. */
int
if_unset_prefix (struct interface *ifp, struct connected *ifc)
{
  return kernel_address_delete_ipv4 (ifp, ifc);
}
#else /* ! HAVE_NETLINK */
#ifdef HAVE_IFALIASREQ
/* Set up interface's IP address, netmask (and broadcas? ).  *BSD may
   has ifaliasreq structure.  */
int
if_set_prefix (struct interface *ifp, struct connected *ifc)
{
  int ret;
  struct ifaliasreq addreq;
  struct sockaddr_in addr;
  struct sockaddr_in mask;
  struct prefix_ipv4 *p;

  p = (struct prefix_ipv4 *) ifc->address;

  memset (&addreq, 0, sizeof addreq);
  strncpy ((char *)&addreq.ifra_name, ifp->name, sizeof addreq.ifra_name);

  memset (&addr, 0, sizeof (struct sockaddr_in));
  addr.sin_addr = p->prefix;
  addr.sin_family = p->family;
#ifdef HAVE_SIN_LEN
  addr.sin_len = sizeof (struct sockaddr_in);
#endif
  memcpy (&addreq.ifra_addr, &addr, sizeof (struct sockaddr_in));

  memset (&mask, 0, sizeof (struct sockaddr_in));
  masklen2ip (p->prefixlen, &mask.sin_addr);
  mask.sin_family = p->family;
#ifdef HAVE_SIN_LEN
  mask.sin_len = sizeof (struct sockaddr_in);
#endif
  memcpy (&addreq.ifra_mask, &mask, sizeof (struct sockaddr_in));
  
  ret = if_ioctl (SIOCAIFADDR, (caddr_t) &addreq);
  if (ret < 0)
    return ret;
  return 0;
}

/* Set up interface's IP address, netmask (and broadcas? ).  *BSD may
   has ifaliasreq structure.  */
int
if_unset_prefix (struct interface *ifp, struct connected *ifc)
{
  int ret;
  struct ifaliasreq addreq;
  struct sockaddr_in addr;
  struct sockaddr_in mask;
  struct prefix_ipv4 *p;

  p = (struct prefix_ipv4 *)ifc->address;

  memset (&addreq, 0, sizeof addreq);
  strncpy ((char *)&addreq.ifra_name, ifp->name, sizeof addreq.ifra_name);

  memset (&addr, 0, sizeof (struct sockaddr_in));
  addr.sin_addr = p->prefix;
  addr.sin_family = p->family;
#ifdef HAVE_SIN_LEN
  addr.sin_len = sizeof (struct sockaddr_in);
#endif
  memcpy (&addreq.ifra_addr, &addr, sizeof (struct sockaddr_in));

  memset (&mask, 0, sizeof (struct sockaddr_in));
  masklen2ip (p->prefixlen, &mask.sin_addr);
  mask.sin_family = p->family;
#ifdef HAVE_SIN_LEN
  mask.sin_len = sizeof (struct sockaddr_in);
#endif
  memcpy (&addreq.ifra_mask, &mask, sizeof (struct sockaddr_in));
  
  ret = if_ioctl (SIOCDIFADDR, (caddr_t) &addreq);
  if (ret < 0)
    return ret;
  return 0;
}
#else
/* Set up interface's address, netmask (and broadcas? ).  Linux or
   Solaris uses ifname:number semantics to set IP address aliases. */
int
if_set_prefix (struct interface *ifp, struct connected *ifc)
{
  int ret;
  struct ifreq ifreq;
  struct sockaddr_in addr;
  struct sockaddr_in broad;
  struct sockaddr_in mask;
  struct prefix_ipv4 ifaddr;
  struct prefix_ipv4 *p;

  p = (struct prefix_ipv4 *) ifc->address;

  ifaddr = *p;

  ifreq_set_name (&ifreq, ifp);

  addr.sin_addr = p->prefix;
  addr.sin_family = p->family;
  memcpy (&ifreq.ifr_addr, &addr, sizeof (struct sockaddr_in));
  ret = if_ioctl (SIOCSIFADDR, (caddr_t) &ifreq);
  if (ret < 0)
    return ret;
  
  /* We need mask for make broadcast addr. */
  masklen2ip (p->prefixlen, &mask.sin_addr);

  if (if_is_broadcast (ifp))
    {
      apply_mask_ipv4 (&ifaddr);
      addr.sin_addr = ifaddr.prefix;

      broad.sin_addr.s_addr = (addr.sin_addr.s_addr | ~mask.sin_addr.s_addr);
      broad.sin_family = p->family;

      memcpy (&ifreq.ifr_broadaddr, &broad, sizeof (struct sockaddr_in));
      ret = if_ioctl (SIOCSIFBRDADDR, (caddr_t) &ifreq);
      if (ret < 0)
	return ret;
    }

  mask.sin_family = p->family;
#ifdef SUNOS_5
  memcpy (&mask, &ifreq.ifr_addr, sizeof (mask));
#else
  memcpy (&ifreq.ifr_netmask, &mask, sizeof (struct sockaddr_in));
#endif /* SUNOS5 */
  ret = if_ioctl (SIOCSIFNETMASK, (caddr_t) &ifreq);
  if (ret < 0)
    return ret;

  return 0;
}

/* Set up interface's address, netmask (and broadcas? ).  Linux or
   Solaris uses ifname:number semantics to set IP address aliases. */
int
if_unset_prefix (struct interface *ifp, struct connected *ifc)
{
  int ret;
  struct ifreq ifreq;
  struct sockaddr_in addr;
  struct prefix_ipv4 *p;

  p = (struct prefix_ipv4 *) ifc->address;

  ifreq_set_name (&ifreq, ifp);

  memset (&addr, 0, sizeof (struct sockaddr_in));
  addr.sin_family = p->family;
  memcpy (&ifreq.ifr_addr, &addr, sizeof (struct sockaddr_in));
  ret = if_ioctl (SIOCSIFADDR, (caddr_t) &ifreq);
  if (ret < 0)
    return ret;

  return 0;
}
#endif /* HAVE_IFALIASREQ */
#endif /* HAVE_NETLINK */

/* get interface flags */
void
if_get_flags (struct interface *ifp)
{
  int ret;
  struct ifreq ifreq;

  ifreq_set_name (&ifreq, ifp);

  ret = if_ioctl (SIOCGIFFLAGS, (caddr_t) &ifreq);
  if (ret < 0) 
    {
      perror ("ioctl");
      return;
    }

  ifp->flags = ifreq.ifr_flags & 0x0000ffff;
}

/* Set interface flags */
int
if_set_flags (struct interface *ifp, unsigned long flags)
{
  int ret;
  struct ifreq ifreq;

  ifreq_set_name (&ifreq, ifp);

  ifreq.ifr_flags = ifp->flags;
  ifreq.ifr_flags |= flags;

  ret = if_ioctl (SIOCSIFFLAGS, (caddr_t) &ifreq);

  if (ret < 0)
    {
      zlog_info ("can't set interface flags");
      return ret;
    }
  return 0;
}

/* Unset interface's flag. */
int
if_unset_flags (struct interface *ifp, unsigned long flags)
{
  int ret;
  struct ifreq ifreq;

  ifreq_set_name (&ifreq, ifp);

  ifreq.ifr_flags = ifp->flags;
  ifreq.ifr_flags &= ~flags;

  ret = if_ioctl (SIOCSIFFLAGS, (caddr_t) &ifreq);

  if (ret < 0)
    {
      zlog_info ("can't unset interface flags");
      return ret;
    }
  return 0;
}

#ifdef HAVE_IPV6

#ifdef LINUX_IPV6
#ifndef _LINUX_IN6_H
/* linux/include/net/ipv6.h */
struct in6_ifreq 
{
  struct in6_addr ifr6_addr;
  u_int32_t ifr6_prefixlen;
  int ifr6_ifindex;
};
#endif /* _LINUX_IN6_H */

/* Interface's address add/delete functions. */
int
if_prefix_add_ipv6 (struct interface *ifp, struct connected *ifc)
{
  int ret;
  struct prefix_ipv6 *p;
  struct in6_ifreq ifreq;

  p = (struct prefix_ipv6 *) ifc->address;

  memset (&ifreq, 0, sizeof (struct in6_ifreq));

  memcpy (&ifreq.ifr6_addr, &p->prefix, sizeof (struct in6_addr));
  ifreq.ifr6_ifindex = ifp->ifindex;
  ifreq.ifr6_prefixlen = p->prefixlen;

  ret = if_ioctl_ipv6 (SIOCSIFADDR, (caddr_t) &ifreq);

  return ret;
}

int
if_prefix_delete_ipv6 (struct interface *ifp, struct connected *ifc)
{
  int ret;
  struct prefix_ipv6 *p;
  struct in6_ifreq ifreq;

  p = (struct prefix_ipv6 *) ifc->address;

  memset (&ifreq, 0, sizeof (struct in6_ifreq));

  memcpy (&ifreq.ifr6_addr, &p->prefix, sizeof (struct in6_addr));
  ifreq.ifr6_ifindex = ifp->ifindex;
  ifreq.ifr6_prefixlen = p->prefixlen;

  ret = if_ioctl_ipv6 (SIOCDIFADDR, (caddr_t) &ifreq);

  return ret;
}
#else /* LINUX_IPV6 */
#ifdef HAVE_IN6_ALIASREQ
#ifndef ND6_INFINITE_LIFETIME
#define ND6_INFINITE_LIFETIME 0xffffffffL
#endif /* ND6_INFINITE_LIFETIME */
int
if_prefix_add_ipv6 (struct interface *ifp, struct connected *ifc)
{
  int ret;
  struct in6_aliasreq addreq;
  struct sockaddr_in6 addr;
  struct sockaddr_in6 mask;
  struct prefix_ipv6 *p;

  p = (struct prefix_ipv6 * ) ifc->address;

  memset (&addreq, 0, sizeof addreq);
  strncpy ((char *)&addreq.ifra_name, ifp->name, sizeof addreq.ifra_name);

  memset (&addr, 0, sizeof (struct sockaddr_in6));
  addr.sin6_addr = p->prefix;
  addr.sin6_family = p->family;
#ifdef HAVE_SIN_LEN
  addr.sin6_len = sizeof (struct sockaddr_in6);
#endif
  memcpy (&addreq.ifra_addr, &addr, sizeof (struct sockaddr_in6));

  memset (&mask, 0, sizeof (struct sockaddr_in6));
  masklen2ip6 (p->prefixlen, &mask.sin6_addr);
  mask.sin6_family = p->family;
#ifdef HAVE_SIN_LEN
  mask.sin6_len = sizeof (struct sockaddr_in6);
#endif
  memcpy (&addreq.ifra_prefixmask, &mask, sizeof (struct sockaddr_in6));
  
  addreq.ifra_lifetime.ia6t_pltime = ND6_INFINITE_LIFETIME; 
  addreq.ifra_lifetime.ia6t_vltime = ND6_INFINITE_LIFETIME; 

  ret = if_ioctl_ipv6 (SIOCAIFADDR_IN6, (caddr_t) &addreq);
  if (ret < 0)
    return ret;
  return 0;
}

int
if_prefix_delete_ipv6 (struct interface *ifp, struct connected *ifc)
{
  int ret;
  struct in6_aliasreq addreq;
  struct sockaddr_in6 addr;
  struct sockaddr_in6 mask;
  struct prefix_ipv6 *p;

  p = (struct prefix_ipv6 *) ifc->address;

  memset (&addreq, 0, sizeof addreq);
  strncpy ((char *)&addreq.ifra_name, ifp->name, sizeof addreq.ifra_name);

  memset (&addr, 0, sizeof (struct sockaddr_in6));
  addr.sin6_addr = p->prefix;
  addr.sin6_family = p->family;
#ifdef HAVE_SIN_LEN
  addr.sin6_len = sizeof (struct sockaddr_in6);
#endif
  memcpy (&addreq.ifra_addr, &addr, sizeof (struct sockaddr_in6));

  memset (&mask, 0, sizeof (struct sockaddr_in6));
  masklen2ip6 (p->prefixlen, &mask.sin6_addr);
  mask.sin6_family = p->family;
#ifdef HAVE_SIN_LEN
  mask.sin6_len = sizeof (struct sockaddr_in6);
#endif
  memcpy (&addreq.ifra_prefixmask, &mask, sizeof (struct sockaddr_in6));

  addreq.ifra_lifetime.ia6t_pltime = ND6_INFINITE_LIFETIME; 
  addreq.ifra_lifetime.ia6t_vltime = ND6_INFINITE_LIFETIME; 

  ret = if_ioctl_ipv6 (SIOCDIFADDR_IN6, (caddr_t) &addreq);
  if (ret < 0)
    return ret;
  return 0;
}
#else
int
if_prefix_add_ipv6 (struct interface *ifp, struct connected *ifc)
{
  return 0;
}

int
if_prefix_delete_ipv6 (struct interface *ifp, struct connected *ifc)
{
  return 0;
}
#endif /* HAVE_IN6_ALIASREQ */

#endif /* LINUX_IPV6 */

#endif /* HAVE_IPV6 */
