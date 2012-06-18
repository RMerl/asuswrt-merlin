/* ICMP Router Discovery Messages
 * Copyright (C) 1997, 2000 Kunihiro Ishiguro
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

/* 
 * This file is modified and completed for the Zebra IRDP implementation
 * by Robert Olsson, Swedish University of Agricultural Sciences
 */

#ifndef _IRDP_H
#define _IRDP_H
 
#define TRUE 1
#define FALSE 0

/* ICMP Messages */
#ifndef ICMP_ROUTERADVERT
#define ICMP_ROUTERADVERT 9
#endif /* ICMP_ROUTERADVERT */

#ifndef ICMP_ROUTERSOLICIT
#define ICMP_ROUTERSOLICIT 10
#endif /* ICMP_ROUTERSOLICT */

/* Multicast groups */
#ifndef INADDR_ALLHOSTS_GROUP
#define INADDR_ALLHOSTS_GROUP 0xe0000001U    /* 224.0.0.1 */
#endif /* INADDR_ALLHOSTS_GROUP */

#ifndef INADDR_ALLRTRS_GROUP
#define INADDR_ALLRTRS_GROUP  0xe0000002U    /* 224.0.0.2 */
#endif /* INADDR_ALLRTRS_GROUP */

/* Default irdp packet interval */
#define IRDP_DEFAULT_INTERVAL 300 

/* Router constants from RFC1256 */
#define MAX_INITIAL_ADVERT_INTERVAL 16
#define MAX_INITIAL_ADVERTISEMENTS   3
#define MAX_RESPONSE_DELAY           2

#define IRDP_MAXADVERTINTERVAL 600
#define IRDP_MINADVERTINTERVAL 450 /* 0.75*600 */
#define IRDP_LIFETIME         1350 /* 3*450 */
#define IRDP_PREFERENCE 0

#define ICMP_MINLEN 8

#define IRDP_LAST_ADVERT_MESSAGES 2 /* The last adverts with Holdtime 0 */

#define IRDP_RX_BUF 1500

/* 
     Comments comes from RFC1256 ICMP Router Discovery Messages. 

     The IP destination address to be used for multicast Router
     Advertisements sent from the interface.  The only permissible
     values are the all-systems multicast address, 224.0.0.1, or the
     limited-broadcast address, 255.255.255.255.  (The all-systems
     address is preferred wherever possible, i.e., on any link where
     all listening hosts support IP multicast.)

     Default: 224.0.0.1 if the router supports IP multicast on the
     interface, else 255.255.255.255 

     The maximum time allowed between sending multicast Router
     Advertisements from the interface, in seconds.  Must be no less
     than 4 seconds and no greater than 1800 seconds.

     Default: 600 seconds 

     The minimum time allowed between sending unsolicited multicast
     Router Advertisements from the interface, in seconds.  Must be no
     less than 3 seconds and no greater than MaxAdvertisementInterval.

     Default: 0.75 * MaxAdvertisementInterval 

     The value to be placed in the Lifetime field of Router
     Advertisements sent from the interface, in seconds.  Must be no
     less than MaxAdvertisementInterval and no greater than 9000
     seconds.

     Default: 3 * MaxAdvertisementInterval 

     The preferability of the address as a default router address,
     relative to other router addresses on the same subnet.  A 32-bit,
     signed, twos-complement integer, with higher values meaning more
     preferable.  The minimum value (hex 80000000) is used to indicate
     that the address, even though it may be advertised, is not to be
     used by neighboring hosts as a default router address.

     Default: 0 
*/

struct irdp_interface 
{
  unsigned long MaxAdvertInterval;
  unsigned long MinAdvertInterval;
  unsigned long Preference;

  u_int32_t flags;

#define IF_ACTIVE               (1<<0) /* ICMP Active */
#define IF_BROADCAST            (1<<1) /* 255.255.255.255 */
#define IF_SOLICIT              (1<<2) /* Solicit active */
#define IF_DEBUG_MESSAGES       (1<<3) 
#define IF_DEBUG_PACKET         (1<<4) 
#define IF_DEBUG_MISC           (1<<5) 
#define IF_SHUTDOWN             (1<<6) 

  struct interface *ifp;
  struct thread *t_advertise;
  unsigned long irdp_sent;
  u_int16_t Lifetime;

 list AdvPrefList;

};

struct Adv 
{
  struct in_addr ip;
  int pref;
};


#endif /* _IRDP_H */





