/*
 *
 * Copyright (C) 2000  Robert Olsson.
 * Swedish University of Agricultural Sciences
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
 * This work includes work with the following copywrite:
 *
 * Copyright (C) 1997, 2000 Kunihiro Ishiguro
 *
 */

/* 
 * Thanks to Jens Låås at Swedish University of Agricultural Sciences
 * for reviewing and tests.
 */


#include <zebra.h>

#ifdef HAVE_IRDP 

#include "if.h"
#include "vty.h"
#include "sockunion.h"
#include "prefix.h"
#include "command.h"
#include "memory.h"
#include "stream.h"
#include "ioctl.h"
#include "connected.h"
#include "log.h"
#include "zclient.h"
#include "thread.h"
#include "zebra/interface.h"
#include "zebra/rtadv.h"
#include "zebra/rib.h"
#include "zebra/zserv.h"
#include "zebra/redistribute.h"
#include "zebra/irdp.h"
#include <netinet/ip_icmp.h>
#include "if.h"
#include "sockunion.h"
#include "log.h"


/* Master of threads. */
extern struct zebra_t zebrad;

extern int irdp_sock;

static const char *
inet_2a(u_int32_t a, char *b)
{
  sprintf(b, "%u.%u.%u.%u",
          (a    ) & 0xFF,
          (a>> 8) & 0xFF,
          (a>>16) & 0xFF,
          (a>>24) & 0xFF);
  return  b;
}


static struct prefix *
irdp_get_prefix(struct interface *ifp)
{
  struct listnode *node;
  struct connected *ifc;
  
  if (ifp->connected)
    for (ALL_LIST_ELEMENTS_RO (ifp->connected, node, ifc))
      return ifc->address;

  return NULL;
}

/* Join to the add/leave multicast group. */
static int
if_group (struct interface *ifp, 
	  int sock, 
	  u_int32_t group, 
	  int add_leave)
{
  struct ip_mreq m;
  struct prefix *p;
  int ret;
  char b1[INET_ADDRSTRLEN];

  memset (&m, 0, sizeof (m));
  m.imr_multiaddr.s_addr = htonl (group);
  p = irdp_get_prefix(ifp);

  if(!p) {
        zlog_warn ("IRDP: can't get address for %s", ifp->name);
	return 1;
  }

  m.imr_interface = p->u.prefix4;

  ret = setsockopt (sock, IPPROTO_IP, add_leave,
		    (char *) &m, sizeof (struct ip_mreq));
  if (ret < 0)
    zlog_warn ("IRDP: %s can't setsockopt %s: %s",
	       add_leave == IP_ADD_MEMBERSHIP? "join group":"leave group", 
	       inet_2a(group, b1),
	       safe_strerror (errno));

  return ret;
}

static int
if_add_group (struct interface *ifp)
{
  struct zebra_if *zi= ifp->info;
  struct irdp_interface *irdp = &zi->irdp;
  int ret;
  char b1[INET_ADDRSTRLEN];

  ret = if_group (ifp, irdp_sock, INADDR_ALLRTRS_GROUP, IP_ADD_MEMBERSHIP);
  if (ret < 0) {
    return ret;
  }

  if(irdp->flags & IF_DEBUG_MISC )
    zlog_debug("IRDP: Adding group %s for %s", 
	       inet_2a(htonl(INADDR_ALLRTRS_GROUP), b1),
	       ifp->name);
  return 0;
}

static int
if_drop_group (struct interface *ifp)
{
  struct zebra_if *zi= ifp->info;
  struct irdp_interface *irdp = &zi->irdp;
  int ret;
  char b1[INET_ADDRSTRLEN];

  ret = if_group (ifp, irdp_sock, INADDR_ALLRTRS_GROUP, IP_DROP_MEMBERSHIP);
  if (ret < 0)
    return ret;

  if(irdp->flags & IF_DEBUG_MISC)
    zlog_debug("IRDP: Leaving group %s for %s", 
	       inet_2a(htonl(INADDR_ALLRTRS_GROUP), b1),
	       ifp->name);
  return 0;
}

static void
if_set_defaults(struct interface *ifp)
{
  struct zebra_if *zi=ifp->info;
  struct irdp_interface *irdp=&zi->irdp;

  irdp->MaxAdvertInterval = IRDP_MAXADVERTINTERVAL;
  irdp->MinAdvertInterval = IRDP_MINADVERTINTERVAL;
  irdp->Preference = IRDP_PREFERENCE;
  irdp->Lifetime = IRDP_LIFETIME;
}


static struct Adv *Adv_new (void)
{
  return XCALLOC (MTYPE_TMP, sizeof (struct Adv));
}

static void
Adv_free (struct Adv *adv)
{
  XFREE (MTYPE_TMP, adv);
}

static void
irdp_if_start(struct interface *ifp, int multicast, int set_defaults)
{
  struct zebra_if *zi= ifp->info;
  struct irdp_interface *irdp = &zi->irdp;
  struct listnode *node;
  struct connected *ifc;
  u_int32_t timer, seed;

  if (irdp->flags & IF_ACTIVE ) {
    zlog_warn("IRDP: Interface is already active %s", ifp->name);
    return;
  }
  if ((irdp_sock < 0) && ((irdp_sock = irdp_sock_init()) < 0)) {
    zlog_warn("IRDP: Cannot activate interface %s (cannot create "
	      "IRDP socket)", ifp->name);
    return;
  }
  irdp->flags |= IF_ACTIVE;

  if(!multicast) 
    irdp->flags |= IF_BROADCAST;

  if_add_update(ifp);

  if (! (ifp->flags & IFF_UP)) {
    zlog_warn("IRDP: Interface is down %s", ifp->name);
  }

  /* Shall we cancel if_start if if_add_group fails? */

  if( multicast) {
    if_add_group(ifp);
    
    if (! (ifp->flags & (IFF_MULTICAST|IFF_ALLMULTI))) {
      zlog_warn("IRDP: Interface not multicast enabled %s", ifp->name);
    }
  }

  if(set_defaults) 
    if_set_defaults(ifp);

  irdp->irdp_sent = 0;

  /* The spec suggests this for randomness */

  seed = 0;
  if( ifp->connected)
    for (ALL_LIST_ELEMENTS_RO (ifp->connected, node, ifc))
      {
        seed = ifc->address->u.prefix4.s_addr;
        break;
      }
  
  srandom(seed);
  timer =  (random () % IRDP_DEFAULT_INTERVAL) + 1; 

  irdp->AdvPrefList = list_new();
  irdp->AdvPrefList->del =  (void (*)(void *)) Adv_free; /* Destructor */


  /* And this for startup. Speed limit from 1991 :-). But it's OK*/

  if(irdp->irdp_sent < MAX_INITIAL_ADVERTISEMENTS &&
     timer > MAX_INITIAL_ADVERT_INTERVAL ) 
	  timer= MAX_INITIAL_ADVERT_INTERVAL;

  
  if(irdp->flags & IF_DEBUG_MISC)
    zlog_debug("IRDP: Init timer for %s set to %u", 
	       ifp->name, 
	       timer);

  irdp->t_advertise = thread_add_timer(zebrad.master, 
				       irdp_send_thread, 
				       ifp, 
				       timer);
}

static void
irdp_if_stop(struct interface *ifp)
{
  struct zebra_if *zi=ifp->info;
  struct irdp_interface *irdp=&zi->irdp;
  
  if (irdp == NULL) {
    zlog_warn ("Interface %s structure is NULL", ifp->name);
    return;
  }

  if (! (irdp->flags & IF_ACTIVE )) {
    zlog_warn("Interface is not active %s", ifp->name);
    return;
  }

  if(! (irdp->flags & IF_BROADCAST)) 
    if_drop_group(ifp);

  irdp_advert_off(ifp);

  list_delete(irdp->AdvPrefList);
  irdp->AdvPrefList=NULL;

  irdp->flags = 0;
}


static void
irdp_if_shutdown(struct interface *ifp)
{
  struct zebra_if *zi= ifp->info;
  struct irdp_interface *irdp = &zi->irdp;

  if (irdp->flags & IF_SHUTDOWN ) {
    zlog_warn("IRDP: Interface is already shutdown %s", ifp->name);
    return;
  }

  irdp->flags |= IF_SHUTDOWN;
  irdp->flags &= ~IF_ACTIVE;

  if(! (irdp->flags & IF_BROADCAST)) 
    if_drop_group(ifp);
  
  /* Tell the hosts we are out of service */
  irdp_advert_off(ifp);
}

static void
irdp_if_no_shutdown(struct interface *ifp)
{
  struct zebra_if *zi= ifp->info;
  struct irdp_interface *irdp = &zi->irdp;

  if (! (irdp->flags & IF_SHUTDOWN )) {
    zlog_warn("IRDP: Interface is not shutdown %s", ifp->name);
    return;
  }

  irdp->flags &= ~IF_SHUTDOWN;

  irdp_if_start(ifp, irdp->flags & IF_BROADCAST? FALSE : TRUE, FALSE); 

}


/* Write configuration to user */

void irdp_config_write (struct vty *vty, struct interface *ifp)
{
  struct zebra_if *zi=ifp->info;
  struct irdp_interface *irdp=&zi->irdp;
  struct Adv *adv;
  struct listnode *node;
  char b1[INET_ADDRSTRLEN];

  if(irdp->flags & IF_ACTIVE || irdp->flags & IF_SHUTDOWN) {

    if( irdp->flags & IF_SHUTDOWN) 
      vty_out (vty, " ip irdp shutdown %s",  VTY_NEWLINE);

    if( irdp->flags & IF_BROADCAST) 
      vty_out (vty, " ip irdp broadcast%s",  VTY_NEWLINE);
    else 
      vty_out (vty, " ip irdp multicast%s",  VTY_NEWLINE);

    vty_out (vty, " ip irdp preference %ld%s",  
	     irdp->Preference, VTY_NEWLINE);

    for (ALL_LIST_ELEMENTS_RO (irdp->AdvPrefList, node, adv))
      vty_out (vty, " ip irdp address %s preference %d%s",
                    inet_2a(adv->ip.s_addr, b1),
                    adv->pref, 
                    VTY_NEWLINE);

    vty_out (vty, " ip irdp holdtime %d%s",  
	     irdp->Lifetime, VTY_NEWLINE);

    vty_out (vty, " ip irdp minadvertinterval %ld%s",  
	     irdp->MinAdvertInterval, VTY_NEWLINE);

    vty_out (vty, " ip irdp maxadvertinterval %ld%s",  
	     irdp->MaxAdvertInterval, VTY_NEWLINE);

  }
}


DEFUN (ip_irdp_multicast,
       ip_irdp_multicast_cmd,
       "ip irdp multicast",
       IP_STR
       "ICMP Router discovery on this interface using multicast\n")
{
  struct interface *ifp;

  ifp = (struct interface *) vty->index;
  if(!ifp) {
	  return CMD_WARNING;
  }

  irdp_if_start(ifp, TRUE, TRUE);
  return CMD_SUCCESS;
}

DEFUN (ip_irdp_broadcast,
       ip_irdp_broadcast_cmd,
       "ip irdp broadcast",
       IP_STR
       "ICMP Router discovery on this interface using broadcast\n")
{
  struct interface *ifp;

  ifp = (struct interface *) vty->index;
  if(!ifp) {
	  return CMD_WARNING;
  }

  irdp_if_start(ifp, FALSE, TRUE);
  return CMD_SUCCESS;
}

DEFUN (no_ip_irdp,
       no_ip_irdp_cmd,
       "no ip irdp",
       NO_STR
       IP_STR
       "Disable ICMP Router discovery on this interface\n")
{
  struct interface *ifp;

  ifp = (struct interface *) vty->index;
  if(!ifp) {
	  return CMD_WARNING;
  }

  irdp_if_stop(ifp);
  return CMD_SUCCESS;
}

DEFUN (ip_irdp_shutdown,
       ip_irdp_shutdown_cmd,
       "ip irdp shutdown",
       IP_STR
       "ICMP Router discovery shutdown on this interface\n")
{
  struct interface *ifp;

  ifp = (struct interface *) vty->index;
  if(!ifp) {
	  return CMD_WARNING;
  }

  irdp_if_shutdown(ifp);
  return CMD_SUCCESS;
}

DEFUN (no_ip_irdp_shutdown,
       no_ip_irdp_shutdown_cmd,
       "no ip irdp shutdown",
       NO_STR
       IP_STR
       "ICMP Router discovery no shutdown on this interface\n")
{
  struct interface *ifp;

  ifp = (struct interface *) vty->index;
  if(!ifp) {
	  return CMD_WARNING;
  }

  irdp_if_no_shutdown(ifp);
  return CMD_SUCCESS;
}

DEFUN (ip_irdp_holdtime,
       ip_irdp_holdtime_cmd,
       "ip irdp holdtime <0-9000>",
       IP_STR
       "ICMP Router discovery on this interface\n"
       "Set holdtime value\n"
       "Holdtime value in seconds. Default is 1800 seconds\n")
{
  struct interface *ifp;
  struct zebra_if *zi;
  struct irdp_interface *irdp;
  ifp = (struct interface *) vty->index;
  if(!ifp) {
	  return CMD_WARNING;
  }

  zi=ifp->info;
  irdp=&zi->irdp;

  irdp->Lifetime = atoi(argv[0]);
  return CMD_SUCCESS;
}

DEFUN (ip_irdp_minadvertinterval,
       ip_irdp_minadvertinterval_cmd,
       "ip irdp minadvertinterval <3-1800>",
       IP_STR
       "ICMP Router discovery on this interface\n"
       "Set minimum time between advertisement\n"
       "Minimum advertisement interval in seconds\n")
{
  struct interface *ifp;
  struct zebra_if *zi;
  struct irdp_interface *irdp;
  ifp = (struct interface *) vty->index;
  if(!ifp) {
	  return CMD_WARNING;
  }

  zi=ifp->info;
  irdp=&zi->irdp;

  if( (unsigned) atoi(argv[0]) <= irdp->MaxAdvertInterval) {
      irdp->MinAdvertInterval = atoi(argv[0]);

      return CMD_SUCCESS;
  }

  vty_out (vty, "ICMP warning maxadvertinterval is greater or equal than minadvertinterval%s", 
	     VTY_NEWLINE);

  vty_out (vty, "Please correct!%s", 
	     VTY_NEWLINE);
  return CMD_WARNING;
}

DEFUN (ip_irdp_maxadvertinterval,
       ip_irdp_maxadvertinterval_cmd,
       "ip irdp maxadvertinterval <4-1800>",
       IP_STR
       "ICMP Router discovery on this interface\n"
       "Set maximum time between advertisement\n"
       "Maximum advertisement interval in seconds\n")
{
  struct interface *ifp;
  struct zebra_if *zi;
  struct irdp_interface *irdp;
  ifp = (struct interface *) vty->index;
  if(!ifp) {
	  return CMD_WARNING;
  }

  zi=ifp->info;
  irdp=&zi->irdp;


  if( irdp->MinAdvertInterval <= (unsigned) atoi(argv[0]) ) {
    irdp->MaxAdvertInterval = atoi(argv[0]);

      return CMD_SUCCESS;
  }

  vty_out (vty, "ICMP warning maxadvertinterval is greater or equal than minadvertinterval%s", 
	     VTY_NEWLINE);

  vty_out (vty, "Please correct!%s", 
	     VTY_NEWLINE);
  return CMD_WARNING;
}

/* DEFUN needs to be fixed for negative ranages...
 * "ip irdp preference <-2147483648-2147483647>",
 * Be positive for now. :-)
 */

DEFUN (ip_irdp_preference,
       ip_irdp_preference_cmd,
       "ip irdp preference <0-2147483647>",
       IP_STR
       "ICMP Router discovery on this interface\n"
       "Set default preference level for this interface\n"
       "Preference level\n")
{
  struct interface *ifp;
  struct zebra_if *zi;
  struct irdp_interface *irdp;
  ifp = (struct interface *) vty->index;
  if(!ifp) {
	  return CMD_WARNING;
  }

  zi=ifp->info;
  irdp=&zi->irdp;

  irdp->Preference = atoi(argv[0]);
  return CMD_SUCCESS;
}

DEFUN (ip_irdp_address_preference,
       ip_irdp_address_preference_cmd,
       "ip irdp address A.B.C.D preference <0-2147483647>",
       IP_STR
       "Alter ICMP Router discovery preference this interface\n"
       "Specify IRDP non-default preference to advertise\n"
       "Set IRDP address for advertise\n"
       "Preference level\n")
{
  struct listnode *node;
  struct in_addr ip; 
  int pref;
  int ret;
  struct interface *ifp;
  struct zebra_if *zi;
  struct irdp_interface *irdp;
  struct Adv *adv;

  ifp = (struct interface *) vty->index;
  if(!ifp) {
	  return CMD_WARNING;
  }

  zi=ifp->info;
  irdp=&zi->irdp;

  ret = inet_aton(argv[0], &ip);
  if(!ret) return CMD_WARNING;

  pref = atoi(argv[1]);

  for (ALL_LIST_ELEMENTS_RO (irdp->AdvPrefList, node, adv))
    if(adv->ip.s_addr == ip.s_addr) 
      return CMD_SUCCESS;

  adv = Adv_new();
  adv->ip = ip;
  adv->pref = pref;
  listnode_add(irdp->AdvPrefList, adv);

  return CMD_SUCCESS;

}

DEFUN (no_ip_irdp_address_preference,
       no_ip_irdp_address_preference_cmd,
       "no ip irdp address A.B.C.D preference <0-2147483647>",
       NO_STR
       IP_STR
       "Alter ICMP Router discovery preference this interface\n"
       "Removes IRDP non-default preference\n"
       "Select IRDP address\n"
       "Old preference level\n")
{
  struct listnode *node, *nnode;
  struct in_addr ip; 
  int ret;
  struct interface *ifp;
  struct zebra_if *zi;
  struct irdp_interface *irdp;
  struct Adv *adv;

  ifp = (struct interface *) vty->index;
  if(!ifp) {
	  return CMD_WARNING;
  }

  zi=ifp->info;
  irdp=&zi->irdp;

  ret = inet_aton(argv[0], &ip);
  if (!ret) 
    return CMD_WARNING;

  for (ALL_LIST_ELEMENTS (irdp->AdvPrefList, node, nnode, adv))
    {
      if(adv->ip.s_addr == ip.s_addr )
        {
          listnode_delete(irdp->AdvPrefList, adv);
          break;
        }
    }
  
  return CMD_SUCCESS;
}

DEFUN (ip_irdp_debug_messages,
       ip_irdp_debug_messages_cmd,
       "ip irdp debug messages",
       IP_STR
       "ICMP Router discovery debug Averts. and Solicits (short)\n")
{
  struct interface *ifp;
  struct zebra_if *zi;
  struct irdp_interface *irdp;
  ifp = (struct interface *) vty->index;
  if(!ifp) {
	  return CMD_WARNING;
  }

  zi=ifp->info;
  irdp=&zi->irdp;

  irdp->flags |= IF_DEBUG_MESSAGES;

  return CMD_SUCCESS;
}

DEFUN (ip_irdp_debug_misc,
       ip_irdp_debug_misc_cmd,
       "ip irdp debug misc",
       IP_STR
       "ICMP Router discovery debug Averts. and Solicits (short)\n")
{
  struct interface *ifp;
  struct zebra_if *zi;
  struct irdp_interface *irdp;
  ifp = (struct interface *) vty->index;
  if(!ifp) {
	  return CMD_WARNING;
  }

  zi=ifp->info;
  irdp=&zi->irdp;

  irdp->flags |= IF_DEBUG_MISC;

  return CMD_SUCCESS;
}

DEFUN (ip_irdp_debug_packet,
       ip_irdp_debug_packet_cmd,
       "ip irdp debug packet",
       IP_STR
       "ICMP Router discovery debug Averts. and Solicits (short)\n")
{
  struct interface *ifp;
  struct zebra_if *zi;
  struct irdp_interface *irdp;
  ifp = (struct interface *) vty->index;
  if(!ifp) {
	  return CMD_WARNING;
  }

  zi=ifp->info;
  irdp=&zi->irdp;

  irdp->flags |= IF_DEBUG_PACKET;

  return CMD_SUCCESS;
}


DEFUN (ip_irdp_debug_disable,
       ip_irdp_debug_disable_cmd,
       "ip irdp debug disable",
       IP_STR
       "ICMP Router discovery debug Averts. and Solicits (short)\n")
{
  struct interface *ifp;
  struct zebra_if *zi;
  struct irdp_interface *irdp;
  ifp = (struct interface *) vty->index;
  if(!ifp) {
	  return CMD_WARNING;
  }

  zi=ifp->info;
  irdp=&zi->irdp;

  irdp->flags &= ~IF_DEBUG_PACKET;
  irdp->flags &= ~IF_DEBUG_MESSAGES;
  irdp->flags &= ~IF_DEBUG_MISC;

  return CMD_SUCCESS;
}

void
irdp_init ()
{
  install_element (INTERFACE_NODE, &ip_irdp_broadcast_cmd);
  install_element (INTERFACE_NODE, &ip_irdp_multicast_cmd);
  install_element (INTERFACE_NODE, &no_ip_irdp_cmd);
  install_element (INTERFACE_NODE, &ip_irdp_shutdown_cmd);
  install_element (INTERFACE_NODE, &no_ip_irdp_shutdown_cmd);
  install_element (INTERFACE_NODE, &ip_irdp_holdtime_cmd);
  install_element (INTERFACE_NODE, &ip_irdp_maxadvertinterval_cmd);
  install_element (INTERFACE_NODE, &ip_irdp_minadvertinterval_cmd);
  install_element (INTERFACE_NODE, &ip_irdp_preference_cmd);
  install_element (INTERFACE_NODE, &ip_irdp_address_preference_cmd);
  install_element (INTERFACE_NODE, &no_ip_irdp_address_preference_cmd);

  install_element (INTERFACE_NODE, &ip_irdp_debug_messages_cmd);
  install_element (INTERFACE_NODE, &ip_irdp_debug_misc_cmd);
  install_element (INTERFACE_NODE, &ip_irdp_debug_packet_cmd);
  install_element (INTERFACE_NODE, &ip_irdp_debug_disable_cmd);
}

#endif /* HAVE_IRDP */
