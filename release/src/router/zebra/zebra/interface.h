/* Interface function header.
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

/* For interface multicast configuration. */
#define IF_ZEBRA_MULTICAST_UNSPEC 0
#define IF_ZEBRA_MULTICAST_ON     1
#define IF_ZEBRA_MULTICAST_OFF    2

/* For interface shutdown configuration. */
#define IF_ZEBRA_SHUTDOWN_UNSPEC 0
#define IF_ZEBRA_SHUTDOWN_ON     1
#define IF_ZEBRA_SHUTDOWN_OFF    2

/* Router advertisement feature. */
#if (defined(LINUX_IPV6) && (defined(__GLIBC__) && __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 1)) || defined(KAME)
#define RTADV
#endif

#ifdef RTADV
/* Router advertisement parameter.  From RFC2461. */
struct rtadvconf
{
  /* A flag indicating whether or not the router sends periodic Router
     Advertisements and responds to Router Solicitations.
     Default: FALSE */
  int AdvSendAdvertisements;

  /* The maximum time allowed between sending unsolicited multicast
     Router Advertisements from the interface, in seconds.  MUST be no
     less than 4 seconds and no greater than 1800 seconds. 

     Default: 600 seconds */
  int MaxRtrAdvInterval;
#define RTADV_MAX_RTR_ADV_INTERVAL 600

  /* The minimum time allowed between sending unsolicited multicast
     Router Advertisements from the interface, in seconds.  MUST be no
     less than 3 seconds and no greater than .75 * MaxRtrAdvInterval.

     Default: 0.33 * MaxRtrAdvInterval */
  int MinRtrAdvInterval;
#define RTADV_MIN_RTR_ADV_INTERVAL (0.33 * RTADV_MAX_RTR_ADV_INTERVAL)

  /* Unsolicited Router Advertisements' interval timer. */
  int AdvIntervalTimer;

  /* The TRUE/FALSE value to be placed in the "Managed address
     configuration" flag field in the Router Advertisement.  See
     [ADDRCONF].
 
     Default: FALSE */
  int AdvManagedFlag;


  /* The TRUE/FALSE value to be placed in the "Other stateful
     configuration" flag field in the Router Advertisement.  See
     [ADDRCONF].

     Default: FALSE */
  int AdvOtherConfigFlag;

  /* The value to be placed in MTU options sent by the router.  A
     value of zero indicates that no MTU options are sent.

     Default: 0 */
  int AdvLinkMTU;


  /* The value to be placed in the Reachable Time field in the Router
     Advertisement messages sent by the router.  The value zero means
     unspecified (by this router).  MUST be no greater than 3,600,000
     milliseconds (1 hour).

     Default: 0 */
  u_int32_t AdvReachableTime;
#define RTADV_MAX_REACHABLE_TIME 3600000


  /* The value to be placed in the Retrans Timer field in the Router
     Advertisement messages sent by the router.  The value zero means
     unspecified (by this router).

     Default: 0 */
  int AdvRetransTimer;

  /* The default value to be placed in the Cur Hop Limit field in the
     Router Advertisement messages sent by the router.  The value
     should be set to that current diameter of the Internet.  The
     value zero means unspecified (by this router).

     Default: The value specified in the "Assigned Numbers" RFC
     [ASSIGNED] that was in effect at the time of implementation. */
  int AdvCurHopLimit;

  /* The value to be placed in the Router Lifetime field of Router
     Advertisements sent from the interface, in seconds.  MUST be
     either zero or between MaxRtrAdvInterval and 9000 seconds.  A
     value of zero indicates that the router is not to be used as a
     default router.

     Default: 3 * MaxRtrAdvInterval */
  int AdvDefaultLifetime;
#define RTADV_ADV_DEFAULT_LIFETIME (3 * RTADV_MAX_RTR_ADV_INTERVAL)


  /* A list of prefixes to be placed in Prefix Information options in
     Router Advertisement messages sent from the interface.

     Default: all prefixes that the router advertises via routing
     protocols as being on-link for the interface from which the
     advertisement is sent. The link-local prefix SHOULD NOT be
     included in the list of advertised prefixes. */
  list AdvPrefixList;
};

#endif /* RTADV */

/* `zebra' daemon local interface structure. */
struct zebra_if
{
  /* Shutdown configuration. */
  u_char shutdown;

  /* Multicast configuration. */
  u_char multicast;

  /* Router advertise configuration. */
  u_char rtadv_enable;

  /* Interface's address. */
  list address;

#ifdef RTADV
  struct rtadvconf rtadv;
#endif /* RTADV */

#ifdef HAVE_IRDP
#include "irdp.h"
  struct irdp_interface irdp;
#endif

};

void if_delete_update (struct interface *ifp);
void if_add_update (struct interface *ifp);
void if_up (struct interface *);
void if_down (struct interface *);
void if_refresh (struct interface *);
void zebra_interface_up_update (struct interface *ifp);
void zebra_interface_down_update (struct interface *ifp);

#ifdef HAVE_PROC_NET_DEV
int ifstat_update_proc ();
#endif /* HAVE_PROC_NET_DEV */
#ifdef HAVE_NET_RT_IFLIST
void ifstat_update_sysctl ();

#endif /* HAVE_NET_RT_IFLIST */
#ifdef HAVE_PROC_NET_DEV
int interface_list_proc ();
#endif /* HAVE_PROC_NET_DEV */
#ifdef HAVE_PROC_NET_IF_INET6
int ifaddr_proc_ipv6 ();
#endif /* HAVE_PROC_NET_IF_INET6 */

#ifdef BSDI
int if_kvm_get_mtu (struct interface *);
#endif /* BSDI */
