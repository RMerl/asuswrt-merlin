/*
 * Interface looking up by ioctl ().
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
#include "sockunion.h"
#include "prefix.h"
#include "ioctl.h"
#include "connected.h"
#include "memory.h"
#include "log.h"

#include "zebra/interface.h"

/* Interface looking up using infamous SIOCGIFCONF. */
static int
interface_list_ioctl (void)
{
  int ret;
  int sock;
#define IFNUM_BASE 32
  int ifnum;
  struct ifreq *ifreq;
  struct ifconf ifconf;
  struct interface *ifp;
  int n;
  int lastlen;

  /* Normally SIOCGIFCONF works with AF_INET socket. */
  sock = socket (AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) 
    {
      zlog_warn ("Can't make AF_INET socket stream: %s", safe_strerror (errno));
      return -1;
    }

  /* Set initial ifreq count.  This will be double when SIOCGIFCONF
     fail.  Solaris has SIOCGIFNUM. */
#ifdef SIOCGIFNUM
  ret = ioctl (sock, SIOCGIFNUM, &ifnum);
  if (ret < 0)
    ifnum = IFNUM_BASE;
  else
    ifnum++;
#else
  ifnum = IFNUM_BASE;
#endif /* SIOCGIFNUM */

  ifconf.ifc_buf = NULL;

  lastlen = 0;
  /* Loop until SIOCGIFCONF success. */
  for (;;) 
    {
      ifconf.ifc_len = sizeof (struct ifreq) * ifnum;
      ifconf.ifc_buf = XREALLOC(MTYPE_TMP, ifconf.ifc_buf, ifconf.ifc_len);

      ret = ioctl(sock, SIOCGIFCONF, &ifconf);

      if (ret < 0) 
	{
	  zlog_warn ("SIOCGIFCONF: %s", safe_strerror(errno));
	  goto end;
	}
      /* Repeatedly get info til buffer fails to grow. */
      if (ifconf.ifc_len > lastlen)
	{
          lastlen = ifconf.ifc_len;
	  ifnum += 10;
	  continue;
	}
      /* Success. */
      break;
    }

  /* Allocate interface. */
  ifreq = ifconf.ifc_req;

#ifdef OPEN_BSD
  for (n = 0; n < ifconf.ifc_len; )
    {
      int size;

      ifreq = (struct ifreq *)((caddr_t) ifconf.ifc_req + n);
      ifp = if_get_by_name_len(ifreq->ifr_name,
			       strnlen(ifreq->ifr_name,
				       sizeof(ifreq->ifr_name)));
      if_add_update (ifp);
      size = ifreq->ifr_addr.sa_len;
      if (size < sizeof (ifreq->ifr_addr))
	size = sizeof (ifreq->ifr_addr);
      size += sizeof (ifreq->ifr_name);
      n += size;
    }
#else
  for (n = 0; n < ifconf.ifc_len; n += sizeof(struct ifreq))
    {
      ifp = if_get_by_name_len(ifreq->ifr_name,
			       strnlen(ifreq->ifr_name,
				       sizeof(ifreq->ifr_name)));
      if_add_update (ifp);
      ifreq++;
    }
#endif /* OPEN_BSD */

 end:
  close (sock);
  XFREE (MTYPE_TMP, ifconf.ifc_buf);

  return ret;
}

/* Get interface's index by ioctl. */
static int
if_get_index (struct interface *ifp)
{
#if defined(HAVE_IF_NAMETOINDEX)
  /* Modern systems should have if_nametoindex(3). */
  ifp->ifindex = if_nametoindex(ifp->name);
#elif defined(SIOCGIFINDEX) && !defined(HAVE_BROKEN_ALIASES)
  /* Fall-back for older linuxes. */
  int ret;
  struct ifreq ifreq;
  static int if_fake_index;

  ifreq_set_name (&ifreq, ifp);

  ret = if_ioctl (SIOCGIFINDEX, (caddr_t) &ifreq);
  if (ret < 0)
    {
      /* Linux 2.0.X does not have interface index. */
      ifp->ifindex = if_fake_index++;
      return ifp->ifindex;
    }

  /* OK we got interface index. */
#ifdef ifr_ifindex
  ifp->ifindex = ifreq.ifr_ifindex;
#else
  ifp->ifindex = ifreq.ifr_index;
#endif

#else
/* Linux 2.2.X does not provide individual interface index 
   for aliases and we know it. For others issue a warning. */
#if !defined(HAVE_BROKEN_ALIASES)
#warning "Using if_fake_index. You may want to add appropriate"
#warning "mapping from ifname to ifindex for your system..."
#endif
  /* This branch probably won't provide usable results, but anyway... */
  static int if_fake_index = 1;
  ifp->ifindex = if_fake_index++;
#endif

  return ifp->ifindex;
}

#ifdef SIOCGIFHWADDR
static int
if_get_hwaddr (struct interface *ifp)
{
  int ret;
  struct ifreq ifreq;
  int i;

  strncpy (ifreq.ifr_name, ifp->name, IFNAMSIZ);
  ifreq.ifr_addr.sa_family = AF_INET;

  /* Fetch Hardware address if available. */
  ret = if_ioctl (SIOCGIFHWADDR, (caddr_t) &ifreq);
  if (ret < 0)
    ifp->hw_addr_len = 0;
  else
    {
      memcpy (ifp->hw_addr, ifreq.ifr_hwaddr.sa_data, 6);

      for (i = 0; i < 6; i++)
	if (ifp->hw_addr[i] != 0)
	  break;

      if (i == 6)
	ifp->hw_addr_len = 0;
      else
	ifp->hw_addr_len = 6;
    }
  return 0;
}
#endif /* SIOCGIFHWADDR */

#ifdef HAVE_GETIFADDRS
#include <ifaddrs.h>

static int
if_getaddrs (void)
{
  int ret;
  struct ifaddrs *ifap;
  struct ifaddrs *ifapfree;
  struct interface *ifp;
  int prefixlen;

  ret = getifaddrs (&ifap); 
  if (ret != 0)
    {
      zlog_err ("getifaddrs(): %s", safe_strerror (errno));
      return -1;
    }

  for (ifapfree = ifap; ifap; ifap = ifap->ifa_next)
    {
      if (ifap->ifa_addr == NULL)
        {
          zlog_err ("%s: nonsensical ifaddr with NULL ifa_addr, ifname %s",
                    __func__, (ifap->ifa_name ? ifap->ifa_name : "(null)"));
          continue;
        }
       
      ifp = if_lookup_by_name (ifap->ifa_name);
      if (ifp == NULL)
	{
	  zlog_err ("if_getaddrs(): Can't lookup interface %s\n",
		    ifap->ifa_name);
	  continue;
	}

      if (ifap->ifa_addr->sa_family == AF_INET)
	{
	  struct sockaddr_in *addr;
	  struct sockaddr_in *mask;
	  struct sockaddr_in *dest;
	  struct in_addr *dest_pnt;
	  int flags = 0;

	  addr = (struct sockaddr_in *) ifap->ifa_addr;
	  mask = (struct sockaddr_in *) ifap->ifa_netmask;
	  prefixlen = ip_masklen (mask->sin_addr);

	  dest_pnt = NULL;

	  if (ifap->ifa_dstaddr &&
	      !IPV4_ADDR_SAME(&addr->sin_addr,
			      &((struct sockaddr_in *)
			      	ifap->ifa_dstaddr)->sin_addr))
	    {
	      dest = (struct sockaddr_in *) ifap->ifa_dstaddr;
	      dest_pnt = &dest->sin_addr;
	      flags = ZEBRA_IFA_PEER;
	    }
	  else if (ifap->ifa_broadaddr &&
		   !IPV4_ADDR_SAME(&addr->sin_addr,
				   &((struct sockaddr_in *)
				     ifap->ifa_broadaddr)->sin_addr))
	    {
	      dest = (struct sockaddr_in *) ifap->ifa_broadaddr;
	      dest_pnt = &dest->sin_addr;
	    }

	  connected_add_ipv4 (ifp, flags, &addr->sin_addr,
			      prefixlen, dest_pnt, NULL);
	}
#ifdef HAVE_IPV6
      if (ifap->ifa_addr->sa_family == AF_INET6)
	{
	  struct sockaddr_in6 *addr;
	  struct sockaddr_in6 *mask;
	  struct sockaddr_in6 *dest;
	  struct in6_addr *dest_pnt;
	  int flags = 0;

	  addr = (struct sockaddr_in6 *) ifap->ifa_addr;
	  mask = (struct sockaddr_in6 *) ifap->ifa_netmask;
	  prefixlen = ip6_masklen (mask->sin6_addr);

	  dest_pnt = NULL;

	  if (ifap->ifa_dstaddr &&
	      !IPV6_ADDR_SAME(&addr->sin6_addr,
			      &((struct sockaddr_in6 *)
			      	ifap->ifa_dstaddr)->sin6_addr))
	    {
	      dest = (struct sockaddr_in6 *) ifap->ifa_dstaddr;
	      dest_pnt = &dest->sin6_addr;
	      flags = ZEBRA_IFA_PEER;
	    }
	  else if (ifap->ifa_broadaddr &&
		   !IPV6_ADDR_SAME(&addr->sin6_addr,
				   &((struct sockaddr_in6 *)
				     ifap->ifa_broadaddr)->sin6_addr))
	    {
	      dest = (struct sockaddr_in6 *) ifap->ifa_broadaddr;
	      dest_pnt = &dest->sin6_addr;
	    }

#if defined(KAME)
	  if (IN6_IS_ADDR_LINKLOCAL(&addr->sin6_addr)) 
	    {
	      addr->sin6_scope_id =
			ntohs(*(u_int16_t *)&addr->sin6_addr.s6_addr[2]);
	      addr->sin6_addr.s6_addr[2] = addr->sin6_addr.s6_addr[3] = 0;
	    }	
#endif          

	  connected_add_ipv6 (ifp, flags, &addr->sin6_addr, prefixlen, 
	                      dest_pnt, NULL);
	}
#endif /* HAVE_IPV6 */
    }

  freeifaddrs (ifapfree);

  return 0; 
}
#else /* HAVE_GETIFADDRS */
/* Interface address lookup by ioctl.  This function only looks up
   IPv4 address. */
int
if_get_addr (struct interface *ifp)
{
  int ret;
  struct ifreq ifreq;
  struct sockaddr_in addr;
  struct sockaddr_in mask;
  struct sockaddr_in dest;
  struct in_addr *dest_pnt;
  u_char prefixlen;
  int flags = 0;

  /* Interface's name and address family. */
  strncpy (ifreq.ifr_name, ifp->name, IFNAMSIZ);
  ifreq.ifr_addr.sa_family = AF_INET;

  /* Interface's address. */
  ret = if_ioctl (SIOCGIFADDR, (caddr_t) &ifreq);
  if (ret < 0) 
    {
      if (errno != EADDRNOTAVAIL)
	{
	  zlog_warn ("SIOCGIFADDR fail: %s", safe_strerror (errno));
	  return ret;
	}
      return 0;
    }
  memcpy (&addr, &ifreq.ifr_addr, sizeof (struct sockaddr_in));

  /* Interface's network mask. */
  ret = if_ioctl (SIOCGIFNETMASK, (caddr_t) &ifreq);
  if (ret < 0) 
    {
      if (errno != EADDRNOTAVAIL) 
	{
	  zlog_warn ("SIOCGIFNETMASK fail: %s", safe_strerror (errno));
	  return ret;
	}
      return 0;
    }
#ifdef ifr_netmask
  memcpy (&mask, &ifreq.ifr_netmask, sizeof (struct sockaddr_in));
#else
  memcpy (&mask, &ifreq.ifr_addr, sizeof (struct sockaddr_in));
#endif /* ifr_netmask */
  prefixlen = ip_masklen (mask.sin_addr);

  /* Point to point or borad cast address pointer init. */
  dest_pnt = NULL;

  ret = if_ioctl (SIOCGIFDSTADDR, (caddr_t) &ifreq);
  if (ret < 0) 
    {
      if (errno != EADDRNOTAVAIL) 
	zlog_warn ("SIOCGIFDSTADDR fail: %s", safe_strerror (errno));
    }
  else if (!IPV4_ADDR_SAME(&addr.sin_addr, &ifreq.ifr_dstaddr.sin_addr))
    {
      memcpy (&dest, &ifreq.ifr_dstaddr, sizeof (struct sockaddr_in));
      dest_pnt = &dest.sin_addr;
      flags = ZEBRA_IFA_PEER;
    }
  if (!dest_pnt)
    {
      ret = if_ioctl (SIOCGIFBRDADDR, (caddr_t) &ifreq);
      if (ret < 0) 
	{
	  if (errno != EADDRNOTAVAIL) 
	    zlog_warn ("SIOCGIFBRDADDR fail: %s", safe_strerror (errno));
	}
      else if (!IPV4_ADDR_SAME(&addr.sin_addr, &ifreq.ifr_broadaddr.sin_addr))
        {
	  memcpy (&dest, &ifreq.ifr_broadaddr, sizeof (struct sockaddr_in));
	  dest_pnt = &dest.sin_addr;
        }
    }


  /* Set address to the interface. */
  connected_add_ipv4 (ifp, flags, &addr.sin_addr, prefixlen, dest_pnt, NULL);

  return 0;
}
#endif /* HAVE_GETIFADDRS */

/* Fetch interface information via ioctl(). */
static void
interface_info_ioctl ()
{
  struct listnode *node, *nnode;
  struct interface *ifp;
  
  for (ALL_LIST_ELEMENTS (iflist, node, nnode, ifp))
    {
      if_get_index (ifp);
#ifdef SIOCGIFHWADDR
      if_get_hwaddr (ifp);
#endif /* SIOCGIFHWADDR */
      if_get_flags (ifp);
#ifndef HAVE_GETIFADDRS
      if_get_addr (ifp);
#endif /* ! HAVE_GETIFADDRS */
      if_get_mtu (ifp);
      if_get_metric (ifp);
    }
}

/* Lookup all interface information. */
void
interface_list ()
{
  /* Linux can do both proc & ioctl, ioctl is the only way to get
     interface aliases in 2.2 series kernels. */
#ifdef HAVE_PROC_NET_DEV
  interface_list_proc ();
#endif /* HAVE_PROC_NET_DEV */
  interface_list_ioctl ();

  /* After listing is done, get index, address, flags and other
     interface's information. */
  interface_info_ioctl ();

#ifdef HAVE_GETIFADDRS
  if_getaddrs ();
#endif /* HAVE_GETIFADDRS */

#if defined(HAVE_IPV6) && defined(HAVE_PROC_NET_IF_INET6)
  /* Linux provides interface's IPv6 address via
     /proc/net/if_inet6. */
  ifaddr_proc_ipv6 ();
#endif /* HAVE_IPV6 && HAVE_PROC_NET_IF_INET6 */
}
